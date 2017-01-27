/*
 * Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*
 * Per-File-Key (PFK).
 *
 * This driver is used for storing eCryptfs information (mainly file
 * encryption key) in file node as part of eCryptfs hardware enhanced solution
 * provided by Qualcomm Technologies, Inc.
 *
 * The information is stored in node when file is first opened (eCryptfs
 * will fire a callback notifying PFK about this event) and will be later
 * accessed by Block Device Driver to actually load the key to encryption hw.
 *
 * PFK exposes API's for loading and removing keys from encryption hw
 * and also API to determine whether 2 adjacent blocks can be agregated by
 * Block Layer in one request to encryption hw.
 * PFK is only supposed to be used by eCryptfs, except the below.
 *
 * Please note, the only API that uses EXPORT_SYMBOL() is pfk_remove_key,
 * this is intentionally, as it is the only API that is intended to be used
 * by any kernel module, including dynamically loaded ones. All other API's,
 * as mentioned above are only supposed to be used by eCryptfs which is
 * a static module.
 */


/* Uncomment the line below to enable debug messages */
/* #define DEBUG 1 */
#define pr_fmt(fmt)	"pfk [%s]: " fmt, __func__

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/printk.h>
#include <linux/bio.h>
#include <linux/security.h>
#include <crypto/ice.h>

#include <linux/pfk.h>
#include <linux/ecryptfs.h>

#include "pfk_kc.h"
#include "objsec.h"
#include "ecryptfs_kernel.h"
#include "pfk_ice.h"

static DEFINE_MUTEX(pfk_lock);
static bool pfk_ready;
static int g_events_handle;


/* might be replaced by a table when more than one cipher is supported */
#define PFK_SUPPORTED_CIPHER "aes_xts"
#define PFK_SUPPORTED_KEY_SIZE 32
#define PFK_SUPPORTED_SALT_SIZE 32


/**
 * inode_to_filename() - get the filename from inode pointer.
 * @inode: inode pointer
 *
 * it is used for debug prints.
 *
 * Return: filename string or "unknown".
 */
static char *inode_to_filename(struct inode *inode)
{
	struct dentry *dentry = NULL;
	char *filename = NULL;

	if (hlist_empty(&inode->i_dentry))
		return "unknown";

	dentry = hlist_entry(inode->i_dentry.first, struct dentry, d_u.d_alias);
	filename = dentry->d_iname;

	return filename;
}

static int __init pfk_lsm_init(void)
{
	if (!selinux_is_enabled()) {
		pr_err("se linux is not enabled.\n");
		return -ENODEV;
	}

	return 0;
}

/**
 * pfk_is_ready() - driver is initialized and ready.
 *
 * Return: true if the driver is ready.
 */
static inline bool pfk_is_ready(void)
{
	return pfk_ready;
}

/**
 * pfk_get_page_index() - get the inode from a bio.
 * @bio: Pointer to BIO structure.
 *
 * Walk the bio struct links to get the inode.
 * Please note, that in general bio may consist of several pages from
 * several files, but in our case we always assume that all pages come
 * from the same file, since our logic ensures it. That is why we only
 * walk through the first page to look for inode.
 *
 * Return: pointer to the inode struct if successful, or NULL otherwise.
 *
 */
static int pfk_get_page_index(const struct bio *bio, pgoff_t *page_index)
{
	if (!bio || !page_index)
		return -EINVAL;

	if (!bio_has_data((struct bio *)bio))
		return -EINVAL;

	if (!bio->bi_io_vec)
		return -EINVAL;
	if (!bio->bi_io_vec->bv_page)
		return -EINVAL;

	*page_index = bio->bi_io_vec->bv_page->index;

	return 0;
}

/**
 * pfk_bio_get_inode() - get the inode from a bio.
 * @bio: Pointer to BIO structure.
 *
 * Walk the bio struct links to get the inode.
 * Please note, that in general bio may consist of several pages from
 * several files, but in our case we always assume that all pages come
 * from the same file, since our logic ensures it. That is why we only
 * walk through the first page to look for inode.
 *
 * Return: pointer to the inode struct if successful, or NULL otherwise.
 *
 */
static struct inode *pfk_bio_get_inode(const struct bio *bio)
{
	if (!bio)
		return NULL;

	if (!bio_has_data((struct bio *)bio))
		return NULL;

	if (!bio->bi_io_vec)
		return NULL;
	if (!bio->bi_io_vec->bv_page)
		return NULL;

	if (PageAnon(bio->bi_io_vec->bv_page)) {
		struct inode *inode;

		/* Using direct-io (O_DIRECT) without page cache */
		inode = dio_bio_get_inode((struct bio *)bio);
		pr_debug("inode on direct-io, inode = 0x%p.\n", inode);

		return inode;
	}

	if (!bio->bi_io_vec->bv_page->mapping)
		return NULL;

	if (!bio->bi_io_vec->bv_page->mapping->host)
		return NULL;

	return bio->bi_io_vec->bv_page->mapping->host;
}

/**
 * pfk_get_ecryptfs_data() - retrieves ecryptfs data stored inside node
 * @inode: inode
 *
 * Return the data or NULL if there isn't any or in case of error
 * Should be invoked under lock
 */
static void *pfk_get_ecryptfs_data(const struct inode *inode)
{
	struct inode_security_struct *isec = NULL;

	if (!inode)
		return NULL;

	isec = inode->i_security;

	if (!isec) {
		pr_debug("i_security is NULL, could be irrelevant file\n");
		return NULL;
	}

	return isec->pfk_data;
}

/**
 * pfk_set_ecryptfs_data() - stores ecryptfs data inside node
 * @inode: inode to update
 * @data: data to put inside the node
 *
 * Returns 0 in case of success, error otherwise
 * Should be invoked under lock
 */
static int pfk_set_ecryptfs_data(struct inode *inode, void *ecryptfs_data)
{
	struct inode_security_struct *isec = NULL;

	if (!inode)
		return -EINVAL;

	isec = inode->i_security;

	if (!isec) {
		pr_err("i_security is NULL, not ready yet\n");
		return -EINVAL;
	}

	isec->pfk_data = ecryptfs_data;

	return 0;
}


/**
 * pfk_parse_cipher() - parse cipher from ecryptfs to enum
 * @ecryptfs_data: ecrypfs data
 * @algo: pointer to store the output enum (can be null)
 *
 * return 0 in case of success, error otherwise (i.e not supported cipher)
 */
static int pfk_parse_cipher(const void *ecryptfs_data,
	enum ice_cryto_algo_mode *algo)
{
	/*
	 * currently only AES XTS algo is supported
	 * in the future, table with supported ciphers might
	 * be introduced
	 */

	if (!ecryptfs_data)
		return -EINVAL;

	if (!ecryptfs_cipher_match(ecryptfs_data,
			PFK_SUPPORTED_CIPHER, sizeof(PFK_SUPPORTED_CIPHER))) {
		pr_debug("ecryptfs alghoritm is not supported by pfk\n");
		return -EINVAL;
	}

	if (algo)
		*algo = ICE_CRYPTO_ALGO_MODE_AES_XTS;

	return 0;
}

/**
 * pfk_key_size_to_key_type() - translate key size to key size enum
 * @key_size: key size in bytes
 * @key_size_type: pointer to store the output enum (can be null)
 *
 * return 0 in case of success, error otherwise (i.e not supported key size)
 */
static int pfk_key_size_to_key_type(size_t key_size,
	enum ice_crpto_key_size *key_size_type)
{
	/*
	 *  currently only 32 bit key size is supported
	 *  in the future, table with supported key sizes might
	 *  be introduced
	 */

	if (key_size != PFK_SUPPORTED_KEY_SIZE) {
		pr_err("not supported key size %lu\n", key_size);
		return -EINVAL;
	}

	if (key_size_type)
		*key_size_type = ICE_CRYPTO_KEY_SIZE_256;

	return 0;
}

static int pfk_bio_to_key(const struct bio *bio, unsigned char const **key,
		size_t *key_size, unsigned char const **salt, size_t *salt_size,
		bool *is_pfe)
{
	struct inode *inode = NULL;
	int ret = 0;
	void *ecryptfs_data = NULL;
	pgoff_t offset;
	bool is_metadata = false;

	/* only a few errors below can indicate that
	 * this function was not invoked within PFE context,
	 * otherwise we will consider it PFE
	 */
	*is_pfe = true;


	if (!bio)
		return -EINVAL;

	if (!key || !salt || !key_size || !salt_size)
		return -EINVAL;

	inode = pfk_bio_get_inode(bio);
	if (!inode) {
		*is_pfe = false;
		return -EINVAL;
	}

	ecryptfs_data = pfk_get_ecryptfs_data(inode);
	if (!ecryptfs_data) {
		*is_pfe = false;
		return -EPERM;
	}

	pr_debug("loading key for file %s\n", inode_to_filename(inode));

	ret = pfk_get_page_index(bio, &offset);
	if (ret != 0) {
		pr_err("could not get page index from bio, probably bug %d\n",
				ret);
		return -EINVAL;
	}

	is_metadata = ecryptfs_is_page_in_metadata(ecryptfs_data, offset);
	if (is_metadata == true) {
		pr_debug("ecryptfs metadata, bypassing ICE\n");
		*is_pfe = false;
		return -EPERM;
	}

	*key = ecryptfs_get_key(ecryptfs_data);
	if (!key) {
		pr_err("could not parse key from ecryptfs\n");
		return -EINVAL;
	}

	*key_size = ecryptfs_get_key_size(ecryptfs_data);
	if (!(*key_size)) {
		pr_err("could not parse key size from ecryptfs\n");
		return -EINVAL;
	}

	*salt = ecryptfs_get_salt(ecryptfs_data);
	if (!salt) {
		pr_err("could not parse salt from ecryptfs\n");
		return -EINVAL;
	}

	*salt_size = ecryptfs_get_salt_size(ecryptfs_data);
	if (!(*salt_size)) {
		pr_err("could not parse salt size from ecryptfs\n");
		return -EINVAL;
	}

	return 0;
}

/**
 * pfk_load_key_start() - loads PFE encryption key to the ICE
 *						  Can also be invoked from non
 *						  PFE context, than it is not
 *						  relevant and is_pfe flag is
 *						  set to true
 * @bio: Pointer to the BIO structure
 * @ice_setting: Pointer to ice setting structure that will be filled with
 * ice configuration values, including the index to which the key was loaded
 * @is_pfe: Pointer to is_pfe flag, which will be true if function was invoked
 *			from PFE context
 *
 * Via bio gets access to ecryptfs key stored in auxiliary structure inside
 * inode and loads it to encryption hw.
 * Returns the index where the key is stored in encryption hw and additional
 * information that will be used later for configuration of the encryption hw.
 *
 * Must be followed by pfk_load_key_end when key is no longer used by ice
 *
 */
int pfk_load_key_start(const struct bio *bio,
		struct ice_crypto_setting *ice_setting, bool *is_pfe,
		bool async)
{
	int ret = 0;
	const unsigned char *key = NULL;
	const unsigned char *salt = NULL;
	size_t key_size = 0;
	size_t salt_size = 0;
	enum ice_cryto_algo_mode algo_mode = 0;
	enum ice_crpto_key_size key_size_type = 0;
	void *ecryptfs_data = NULL;
	u32 key_index = 0;
	struct inode *inode = NULL;

	if (!is_pfe) {
		pr_err("is_pfe is NULL\n");
		return -EINVAL;
	}

	/* only a few errors below can indicate that
	 * this function was not invoked within PFE context,
	 * otherwise we will consider it PFE
	 */
	*is_pfe = true;

	if (!pfk_is_ready())
		return -ENODEV;

	if (!ice_setting) {
		pr_err("ice setting is NULL\n");
		return -EINVAL;
	}

	ret = pfk_bio_to_key(bio, &key, &key_size, &salt, &salt_size, is_pfe);
	if (ret != 0)
		return ret;

	inode = pfk_bio_get_inode(bio);
	if (!inode) {
		*is_pfe = false;
		return -EINVAL;
	}

	ecryptfs_data = pfk_get_ecryptfs_data(inode);
	if (!ecryptfs_data) {
		*is_pfe = false;
		return -EPERM;
	}

	ret = pfk_parse_cipher(ecryptfs_data, &algo_mode);
	if (ret != 0) {
		pr_err("not supported cipher\n");
		return ret;
	}

	ret = pfk_key_size_to_key_type(key_size, &key_size_type);
	if (ret != 0)
		return ret;

	ret = pfk_kc_load_key_start(key, key_size, salt, salt_size, &key_index,
			async);
	if (ret) {
		if (ret != -EBUSY && ret != -EAGAIN)
			pr_err("start: could not load key into pfk key cache, error %d\n",
					ret);

		return ret;
	}

	ice_setting->key_size = key_size_type;
	ice_setting->algo_mode = algo_mode;
	/* hardcoded for now */
	ice_setting->key_mode = ICE_CRYPTO_USE_LUT_SW_KEY;
	ice_setting->key_index = key_index;

	return 0;
}

/**
 * pfk_load_key_end() - marks the PFE key as no longer used by ICE
 *						Can also be invoked from non
 *						PFE context, than it is not
 *						relevant and is_pfe flag is
 *						set to true
 * @bio: Pointer to the BIO structure
 * @is_pfe: Pointer to is_pfe flag, which will be true if function was invoked
 *			from PFE context
 *
 * Via bio gets access to ecryptfs key stored in auxiliary structure inside
 * inode and loads it to encryption hw.
 *
 */
int pfk_load_key_end(const struct bio *bio, bool *is_pfe)
{
	int ret = 0;
	const unsigned char *key = NULL;
	const unsigned char *salt = NULL;
	size_t key_size = 0;
	size_t salt_size = 0;

	if (!is_pfe) {
		pr_err("is_pfe is NULL\n");
		return -EINVAL;
	}

	/* only a few errors below can indicate that
	 * this function was not invoked within PFE context,
	 * otherwise we will consider it PFE
	 */
	*is_pfe = true;

	if (!pfk_is_ready())
		return -ENODEV;

	ret = pfk_bio_to_key(bio, &key, &key_size, &salt, &salt_size, is_pfe);
	if (ret != 0)
		return ret;

	pfk_kc_load_key_end(key, key_size, salt, salt_size);

	return 0;
}

/**
 * pfk_remove_key() - removes key from hw
 * @key: pointer to the key
 * @key_size: key size
 *
 * Will be used by external clients to remove a particular key for security
 * reasons.
 * The only API that can be used by dynamically loaded modules,
 * see explanations above at the beginning of this file.
 * The key is removed securely (by memsetting the previous value)
 */
int pfk_remove_key(const unsigned char *key, size_t key_size)
{
	int ret = 0;

	if (!pfk_is_ready())
		return -ENODEV;

	if (!key)
		return -EINVAL;

	ret = pfk_kc_remove_key(key, key_size);

	return ret;
}
EXPORT_SYMBOL(pfk_remove_key);

/**
 * pfk_allow_merge_bio() - Check if 2 BIOs can be merged.
 * @bio1:	Pointer to first BIO structure.
 * @bio2:	Pointer to second BIO structure.
 *
 * Prevent merging of BIOs from encrypted and non-encrypted
 * files, or files encrypted with different key.
 * Also prevent non encrypted and encrypted data from the same file
 * to be merged (ecryptfs header if stored inside file should be non
 * encrypted)
 * This API is called by the file system block layer.
 *
 * Return: true if the BIOs allowed to be merged, false
 * otherwise.
 */
bool pfk_allow_merge_bio(struct bio *bio1, struct bio *bio2)
{
	int ret;
	void *ecryptfs_data1 = NULL;
	void *ecryptfs_data2 = NULL;
	pgoff_t offset1, offset2;
	bool res = false;

	/* if there is no pfk, don't disallow merging blocks */
	if (!pfk_is_ready())
		return true;

	if (!bio1 || !bio2)
		return false;

	ecryptfs_data1 = pfk_get_ecryptfs_data(pfk_bio_get_inode(bio1));
	ecryptfs_data2 = pfk_get_ecryptfs_data(pfk_bio_get_inode(bio2));

	/*
	 * if we have 2 different encrypted files or 1 encrypted and 1 regular,
	 * merge is forbidden
	 */
	if (!ecryptfs_is_data_equal(ecryptfs_data1, ecryptfs_data2)) {
		res = false;
		goto end;
	}

	/*
	 * if both are equall in their NULLINNESS, we have 2 unencrypted files,
	 * allow merge
	 */
	if (!ecryptfs_data1) {
		res = true;
		goto end;
	}

	/*
	 *  at this point both bio's are in the same file which is probably
	 *  encrypted, last thing to check is header vs data
	 *  We are assuming that we are not working in O_DIRECT mode,
	 *  since it is not currently supported by eCryptfs
	 */
	ret = pfk_get_page_index(bio1, &offset1);
	if (ret != 0) {
		pr_err("could not get page index from bio1, probably bug %d\n",
			 ret);
		res = false;
		goto end;
	}

	ret = pfk_get_page_index(bio2, &offset2);
	if (ret != 0) {
		pr_err("could not get page index from bio2, bug %d\n", ret);
		res = false;
		goto end;
	}

	res =  (ecryptfs_is_page_in_metadata(ecryptfs_data1, offset1) ==
			ecryptfs_is_page_in_metadata(ecryptfs_data2, offset2));

	/* fall through */

end:

	return res;
}

/**
 * pfk_open_cb() - callback function for file open event
 * @inode: file inode
 * @data: data provided by eCryptfs
 *
 * Will be invoked from eCryptfs in case of file open event
 */
static void pfk_open_cb(struct inode *inode, void *ecryptfs_data)
{
	size_t key_size;

	if (!pfk_is_ready())
		return;

	if (!inode) {
		pr_err("inode is null\n");
		return;
	}

	key_size = ecryptfs_get_key_size(ecryptfs_data);
	if (!(key_size)) {
		pr_err("could not parse key size from ecryptfs\n");
		return;
	}

	if (0 != pfk_parse_cipher(ecryptfs_data, NULL)) {
		pr_debug("open_cb: not supported cipher\n");
		return;
	}


	if (0 != pfk_key_size_to_key_type(key_size, NULL))
		return;

	mutex_lock(&pfk_lock);
	pfk_set_ecryptfs_data(inode, ecryptfs_data);
	mutex_unlock(&pfk_lock);
}

/**
 * pfk_release_cb() - callback function for file release event
 * @inode: file inode
 *
 * Will be invoked from eCryptfs in case of file release event
 */
static void pfk_release_cb(struct inode *inode)
{
	const unsigned char *key = NULL;
	const unsigned char *salt = NULL;
	size_t key_size = 0;
	size_t salt_size = 0;
	void *data = NULL;

	if (!pfk_is_ready())
		return;

	if (!inode) {
		pr_err("inode is null\n");
		return;
	}

	data = pfk_get_ecryptfs_data(inode);
	if (!data) {
		pr_debug("could not get ecryptfs data from inode\n");
		return;
	}

	key = ecryptfs_get_key(data);
	if (!key) {
		pr_err("could not parse key from ecryptfs\n");
		return;
	}

	key_size = ecryptfs_get_key_size(data);
	if (!(key_size)) {
		pr_err("could not parse key size from ecryptfs\n");
		return;
	}

	salt = ecryptfs_get_salt(data);
	if (!salt) {
		pr_err("could not parse salt from ecryptfs\n");
		return;
	}

	salt_size = ecryptfs_get_salt_size(data);
	if (!salt_size) {
		pr_err("could not parse salt size from ecryptfs\n");
		return;
	}

	pfk_kc_remove_key_with_salt(key, key_size, salt, salt_size);


	mutex_lock(&pfk_lock);
	pfk_set_ecryptfs_data(inode, NULL);
	mutex_unlock(&pfk_lock);
}

static bool pfk_is_cipher_supported_cb(const void *ecryptfs_data)
{
	if (!pfk_is_ready())
		return false;

	if (!ecryptfs_data)
		return false;

	return (pfk_parse_cipher(ecryptfs_data, NULL)) == 0;
}

static bool pfk_is_hw_crypt_cb(void)
{
	if (!pfk_is_ready())
		return false;

	return true;
}

static size_t pfk_get_salt_key_size_cb(const void *ecryptfs_data)
{
	if (!pfk_is_ready())
		return 0;

	if (!pfk_is_cipher_supported_cb(ecryptfs_data))
		return 0;

	return PFK_SUPPORTED_SALT_SIZE;
}


static void __exit pfk_exit(void)
{
	pfk_ready = false;
	ecryptfs_unregister_from_events(g_events_handle);
	pfk_kc_deinit();
}

static int __init pfk_init(void)
{

	int ret = 0;
	struct ecryptfs_events events = {0};

	events.open_cb = pfk_open_cb;
	events.release_cb = pfk_release_cb;
	events.is_cipher_supported_cb = pfk_is_cipher_supported_cb;
	events.is_hw_crypt_cb = pfk_is_hw_crypt_cb;
	events.get_salt_key_size_cb = pfk_get_salt_key_size_cb;

	g_events_handle = ecryptfs_register_to_events(&events);
	if (0 == g_events_handle) {
		pr_err("could not register with eCryptfs, error %d\n", ret);
		goto fail;
	}

	ret = pfk_kc_init();
	if (ret != 0) {
		pr_err("could init pfk key cache, error %d\n", ret);
		ecryptfs_unregister_from_events(g_events_handle);
		goto fail;
	}

	ret = pfk_lsm_init();
	if (ret != 0) {
		pr_debug("neither pfk nor se-linux sec modules are enabled\n");
		pr_debug("not an error, just don't enable pfk\n");
		pfk_kc_deinit();
		ecryptfs_unregister_from_events(g_events_handle);
		return 0;
	}

	pfk_ready = true;
	pr_info("Driver initialized successfully\n");

	return 0;

fail:
	pr_err("Failed to init driver\n");
	return -ENODEV;
}

module_init(pfk_init);
module_exit(pfk_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Per-File-Key driver");
