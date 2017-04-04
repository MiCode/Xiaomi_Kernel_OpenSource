/*
 * Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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
 * Per-File-Key (PFK) - eCryptfs.
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
 */


/* Uncomment the line below to enable debug messages */
/* #define DEBUG 1 */
#define pr_fmt(fmt)	"pfk_ecryptfs [%s]: " fmt, __func__

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/printk.h>
#include <linux/bio.h>
#include <linux/security.h>
#include <linux/lsm_hooks.h>
#include <crypto/ice.h>

#include <linux/pfk.h>
#include <linux/ecryptfs.h>

#include "pfk_ecryptfs.h"
#include "pfk_kc.h"
#include "objsec.h"
#include "ecryptfs_kernel.h"
#include "pfk_ice.h"

static DEFINE_MUTEX(pfk_ecryptfs_lock);
static bool pfk_ecryptfs_ready;
static int g_events_handle;


/* might be replaced by a table when more than one cipher is supported */
#define PFK_SUPPORTED_CIPHER "aes_xts"
#define PFK_SUPPORTED_SALT_SIZE 32

static void *pfk_ecryptfs_get_data(const struct inode *inode);
static void pfk_ecryptfs_open_cb(struct inode *inode, void *ecryptfs_data);
static void pfk_ecryptfs_release_cb(struct inode *inode);
static bool pfk_ecryptfs_is_cipher_supported_cb(const void *ecryptfs_data);
static size_t pfk_ecryptfs_get_salt_key_size_cb(const void *ecryptfs_data);
static bool pfk_ecryptfs_is_hw_crypt_cb(void);


/**
 * pfk_is_ecryptfs_type() - return true if inode belongs to ICE ecryptfs PFE
 * @inode: inode pointer
 */
bool pfk_is_ecryptfs_type(const struct inode *inode)
{
	void *ecryptfs_data = NULL;

	/*
	 * the actual filesystem of an inode is still ext4, eCryptfs never
	 * reaches bio
	 */
	if (!pfe_is_inode_filesystem_type(inode, "ext4"))
		return false;

	ecryptfs_data = pfk_ecryptfs_get_data(inode);

	if (!ecryptfs_data)
		return false;

	return true;
}

/*
 *  pfk_ecryptfs_lsm_init() - makes sure either se-linux is
 *  registered as security module as it is required by pfk_ecryptfs.
 *
 *  This is required because ecryptfs uses a field inside security struct in
 *  inode to store its info
 */
static int __init pfk_ecryptfs_lsm_init(void)
{
	if (!selinux_is_enabled()) {
		pr_err("PFE eCryptfs requires se linux to be enabled\n");
		return -ENODEV;
	}

	return 0;
}

/*
 * pfk_ecryptfs_deinit() - Deinit function, should be invoked by upper PFK layer
 */
void pfk_ecryptfs_deinit(void)
{
	pfk_ecryptfs_ready = false;
	ecryptfs_unregister_from_events(g_events_handle);
}

/*
 * pfk_ecryptfs_init() - Init function, should be invoked by upper PFK layer
 */
int __init pfk_ecryptfs_init(void)
{
	int ret = 0;
	struct ecryptfs_events events = {0};

	events.open_cb = pfk_ecryptfs_open_cb;
	events.release_cb = pfk_ecryptfs_release_cb;
	events.is_cipher_supported_cb = pfk_ecryptfs_is_cipher_supported_cb;
	events.is_hw_crypt_cb = pfk_ecryptfs_is_hw_crypt_cb;
	events.get_salt_key_size_cb = pfk_ecryptfs_get_salt_key_size_cb;

	g_events_handle = ecryptfs_register_to_events(&events);
	if (g_events_handle == 0) {
		pr_err("could not register with eCryptfs, error %d\n", ret);
		goto fail;
	}

	ret = pfk_ecryptfs_lsm_init();
	if (ret != 0) {
		pr_debug("neither pfk nor se-linux sec modules are enabled\n");
		pr_debug("not an error, just don't enable PFK ecryptfs\n");
		ecryptfs_unregister_from_events(g_events_handle);
		return 0;
	}

	pfk_ecryptfs_ready = true;
	pr_info("PFK ecryptfs inited successfully\n");

	return 0;

fail:
	pr_err("Failed to init PFK ecryptfs\n");
	return -ENODEV;
}

/**
 * pfk_ecryptfs_is_ready() - driver is initialized and ready.
 *
 * Return: true if the driver is ready.
 */
static inline bool pfk_ecryptfs_is_ready(void)
{
	return pfk_ecryptfs_ready;
}

/**
 * pfk_ecryptfs_get_page_index() - get the inode from a bio.
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
static int pfk_ecryptfs_get_page_index(const struct bio *bio,
	pgoff_t *page_index)
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
 * pfk_ecryptfs_get_data() - retrieves ecryptfs data stored inside node
 * @inode: inode
 *
 * Return the data or NULL if there isn't any or in case of error
 * Should be invoked under lock
 */
static void *pfk_ecryptfs_get_data(const struct inode *inode)
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
 * pfk_ecryptfs_set_data() - stores ecryptfs data inside node
 * @inode: inode to update
 * @data: data to put inside the node
 *
 * Returns 0 in case of success, error otherwise
 * Should be invoked under lock
 */
static int pfk_ecryptfs_set_data(struct inode *inode, void *ecryptfs_data)
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
 * pfk_ecryptfs_parse_cipher() - parse cipher from ecryptfs to enum
 * @ecryptfs_data: ecrypfs data
 * @algo: pointer to store the output enum (can be null)
 *
 * return 0 in case of success, error otherwise (i.e not supported cipher)
 */
static int pfk_ecryptfs_parse_cipher(const void *ecryptfs_data,
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

/*
 * pfk_ecryptfs_parse_inode() - parses key and algo information from inode
 *
 * Should be invoked by upper pfk layer
 * @bio: bio
 * @inode: inode to be parsed
 * @key_info: out, key and salt information to be stored
 * @algo: out, algorithm to be stored (can be null)
 * @is_pfe: out, will be false if inode is not relevant to PFE, in such a case
 * it should be treated as non PFE by the block layer
 */
int pfk_ecryptfs_parse_inode(const struct bio *bio,
	const struct inode *inode,
	struct pfk_key_info *key_info,
	enum ice_cryto_algo_mode *algo,
	bool *is_pfe)
{
	int ret = 0;
	void *ecryptfs_data = NULL;
	pgoff_t offset;
	bool is_metadata = false;

	if (!is_pfe)
		return -EINVAL;

	/*
	 * only a few errors below can indicate that
	 * this function was not invoked within PFE context,
	 * otherwise we will consider it PFE
	 */
	*is_pfe = true;

	if (!pfk_ecryptfs_is_ready())
		return -ENODEV;

	if (!inode)
		return -EINVAL;

	if (!key_info)
		return -EINVAL;

	ecryptfs_data = pfk_ecryptfs_get_data(inode);
	if (!ecryptfs_data) {
		pr_err("internal error, no ecryptfs data\n");
		return -EINVAL;
	}

	ret = pfk_ecryptfs_get_page_index(bio, &offset);
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

	key_info->key = ecryptfs_get_key(ecryptfs_data);
	if (!key_info->key) {
		pr_err("could not parse key from ecryptfs\n");
		return -EINVAL;
	}

	key_info->key_size = ecryptfs_get_key_size(ecryptfs_data);
	if (!key_info->key_size) {
		pr_err("could not parse key size from ecryptfs\n");
		return -EINVAL;
	}

	key_info->salt = ecryptfs_get_salt(ecryptfs_data);
	if (!key_info->salt) {
		pr_err("could not parse salt from ecryptfs\n");
		return -EINVAL;
	}

	key_info->salt_size = ecryptfs_get_salt_size(ecryptfs_data);
	if (!key_info->salt_size) {
		pr_err("could not parse salt size from ecryptfs\n");
		return -EINVAL;
	}

	ret = pfk_ecryptfs_parse_cipher(ecryptfs_data, algo);
	if (ret != 0) {
		pr_err("not supported cipher\n");
		return ret;
	}

	return 0;
}

/**
 * pfk_ecryptfs_allow_merge_bio() - Check if 2 bios can be merged.
 *
 * Should be invoked by upper pfk layer
 *
 * @bio1: Pointer to first BIO structure.
 * @bio2: Pointer to second BIO structure.
 * @inode1: Pointer to inode from first bio
 * @inode2: Pointer to inode from second bio
 *
 * Prevent merging of BIOs from encrypted and non-encrypted
 * files, or files encrypted with different key.
 * Also prevent non encrypted and encrypted data from the same file
 * to be merged (ecryptfs header if stored inside file should be non
 * encrypted)
 *
 * Return: true if the BIOs allowed to be merged, false
 * otherwise.
 */
bool pfk_ecryptfs_allow_merge_bio(const struct bio *bio1,
	const struct bio *bio2, const struct inode *inode1,
	const struct inode *inode2)
{
	int ret;
	void *ecryptfs_data1 = NULL;
	void *ecryptfs_data2 = NULL;
	pgoff_t offset1, offset2;

	/* if there is no ecryptfs pfk, don't disallow merging blocks */
	if (!pfk_ecryptfs_is_ready())
		return true;

	if (!inode1 || !inode2)
		return false;

	ecryptfs_data1 = pfk_ecryptfs_get_data(inode1);
	ecryptfs_data2 = pfk_ecryptfs_get_data(inode2);

	if (!ecryptfs_data1 || !ecryptfs_data2) {
		pr_err("internal error, ecryptfs data should not be null");
		return false;
	}

	/*
	 * if we have 2 different encrypted files merge is not allowed
	 */
	if (!ecryptfs_is_data_equal(ecryptfs_data1, ecryptfs_data2))
		return false;

	/*
	 *  at this point both bio's are in the same file which is probably
	 *  encrypted, last thing to check is header vs data
	 *  We are assuming that we are not working in O_DIRECT mode,
	 *  since it is not currently supported by eCryptfs
	 */
	ret = pfk_ecryptfs_get_page_index(bio1, &offset1);
	if (ret != 0) {
		pr_err("could not get page index from bio1, probably bug %d\n",
			 ret);
		return false;
	}

	ret = pfk_ecryptfs_get_page_index(bio2, &offset2);
	if (ret != 0) {
		pr_err("could not get page index from bio2, bug %d\n", ret);
		return false;
	}

	return (ecryptfs_is_page_in_metadata(ecryptfs_data1, offset1) ==
			ecryptfs_is_page_in_metadata(ecryptfs_data2, offset2));
}

/**
 * pfk_ecryptfs_open_cb() - callback function for file open event
 * @inode: file inode
 * @data: data provided by eCryptfs
 *
 * Will be invoked from eCryptfs in case of file open event
 */
static void pfk_ecryptfs_open_cb(struct inode *inode, void *ecryptfs_data)
{
	size_t key_size;

	if (!pfk_ecryptfs_is_ready())
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

	if (pfk_ecryptfs_parse_cipher(ecryptfs_data, NULL) != 0) {
		pr_debug("open_cb: not supported cipher\n");
		return;
	}

	if (pfk_key_size_to_key_type(key_size, NULL) != 0)
		return;

	mutex_lock(&pfk_ecryptfs_lock);
	pfk_ecryptfs_set_data(inode, ecryptfs_data);
	mutex_unlock(&pfk_ecryptfs_lock);
}

/**
 * pfk_ecryptfs_release_cb() - callback function for file release event
 * @inode: file inode
 *
 * Will be invoked from eCryptfs in case of file release event
 */
static void pfk_ecryptfs_release_cb(struct inode *inode)
{
	const unsigned char *key = NULL;
	const unsigned char *salt = NULL;
	size_t key_size = 0;
	size_t salt_size = 0;
	void *data = NULL;

	if (!pfk_ecryptfs_is_ready())
		return;

	if (!inode) {
		pr_err("inode is null\n");
		return;
	}

	data = pfk_ecryptfs_get_data(inode);
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

	mutex_lock(&pfk_ecryptfs_lock);
	pfk_ecryptfs_set_data(inode, NULL);
	mutex_unlock(&pfk_ecryptfs_lock);
}

/*
 * pfk_ecryptfs_is_cipher_supported_cb() - callback function to determine
 * whether a particular cipher (stored in ecryptfs_data) is cupported by pfk
 *
 * Ecryptfs should invoke this callback whenever it needs to determine whether
 * pfk supports the particular cipher mode
 *
 * @ecryptfs_data: ecryptfs data
 */
static bool pfk_ecryptfs_is_cipher_supported_cb(const void *ecryptfs_data)
{
	if (!pfk_ecryptfs_is_ready())
		return false;

	if (!ecryptfs_data)
		return false;

	return (pfk_ecryptfs_parse_cipher(ecryptfs_data, NULL)) == 0;
}

/*
 * pfk_ecryptfs_is_hw_crypt_cb() - callback function that implements a query
 * by ecryptfs whether PFK supports HW encryption
 */
static bool pfk_ecryptfs_is_hw_crypt_cb(void)
{
	if (!pfk_ecryptfs_is_ready())
		return false;

	return true;
}

/*
 * pfk_ecryptfs_get_salt_key_size_cb() - callback function to determine
 * what is the salt size supported by PFK
 *
 * @ecryptfs_data: ecryptfs data
 */
static size_t pfk_ecryptfs_get_salt_key_size_cb(const void *ecryptfs_data)
{
	if (!pfk_ecryptfs_is_ready())
		return 0;

	if (!pfk_ecryptfs_is_cipher_supported_cb(ecryptfs_data))
		return 0;

	return PFK_SUPPORTED_SALT_SIZE;
}
