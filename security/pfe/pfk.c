/*
 * Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
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
 * This driver is responsible for overall management of various
 * Per File Encryption variants that work on top of or as part of different
 * file systems.
 *
 * The driver has the following purpose :
 * 1) Define priorities between PFE's if more than one is enabled
 * 2) Extract key information from inode
 * 3) Load and manage various keys in ICE HW engine
 * 4) It should be invoked from various layers in FS/BLOCK/STORAGE DRIVER
 *    that need to take decision on HW encryption management of the data
 *    Some examples:
 *	BLOCK LAYER: when it takes decision on whether 2 chunks can be united
 *	to one encryption / decryption request sent to the HW
 *
 *	UFS DRIVER: when it need to configure ICE HW with a particular key slot
 *	to be used for encryption / decryption
 *
 * PFE variants can differ on particular way of storing the cryptographic info
 * inside inode, actions to be taken upon file operations, etc., but the common
 * properties are described above
 *
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
#include <crypto/algapi.h>
#include <crypto/ice.h>

#include <linux/pfk.h>

#include "pfk_kc.h"
#include "objsec.h"
#include "pfk_ice.h"
#include "pfk_ext4.h"
#include "pfk_f2fs.h"
#include "pfk_internal.h"
//#include "ext4.h"

static bool pfk_ready;


/* might be replaced by a table when more than one cipher is supported */
#define PFK_SUPPORTED_KEY_SIZE 32
#define PFK_SUPPORTED_SALT_SIZE 32

/* Various PFE types and function tables to support each one of them */
enum pfe_type {EXT4_CRYPT_PFE, F2FS_CRYPT_PFE, INVALID_PFE};

typedef int (*pfk_parse_inode_type)(const struct bio *bio,
	const struct inode *inode,
	struct pfk_key_info *key_info,
	enum ice_cryto_algo_mode *algo,
	bool *is_pfe,
	const char *storage_type);

typedef bool (*pfk_allow_merge_bio_type)(const struct bio *bio1,
	const struct bio *bio2, const struct inode *inode1,
	const struct inode *inode2);

static const pfk_parse_inode_type pfk_parse_inode_ftable[] = {
	/* EXT4_CRYPT_PFE */ &pfk_ext4_parse_inode,
    /* F2FS_CRYPT_PFE */ &pfk_f2fs_parse_inode,
};

static const pfk_allow_merge_bio_type pfk_allow_merge_bio_ftable[] = {
	/* EXT4_CRYPT_PFE */ &pfk_ext4_allow_merge_bio,
    /* F2FS_CRYPT_PFE */ &pfk_f2fs_allow_merge_bio,
};

static void __exit pfk_exit(void)
{
	pfk_ready = false;
	pfk_ext4_deinit();
	pfk_f2fs_deinit();
	pfk_kc_deinit();
}

static int __init pfk_init(void)
{

	int ret = 0;

	ret = pfk_ext4_init();
	if (ret != 0)
		goto fail;

	ret = pfk_f2fs_init();
	if (ret != 0)
		goto fail;

	ret = pfk_kc_init();
	if (ret != 0) {
		pr_err("could init pfk key cache, error %d\n", ret);
		pfk_ext4_deinit();
		pfk_f2fs_deinit();
		goto fail;
	}

	pfk_ready = true;
	pr_info("Driver initialized successfully\n");

	return 0;

fail:
	pr_err("Failed to init driver\n");
	return -ENODEV;
}

/*
 * If more than one type is supported simultaneously, this function will also
 * set the priority between them
 */
static enum pfe_type pfk_get_pfe_type(const struct inode *inode)
{
	if (!inode)
		return INVALID_PFE;

	if (pfk_is_ext4_type(inode))
		return EXT4_CRYPT_PFE;

	if (pfk_is_f2fs_type(inode))
		return F2FS_CRYPT_PFE;

	return INVALID_PFE;
}

/**
 * inode_to_filename() - get the filename from inode pointer.
 * @inode: inode pointer
 *
 * it is used for debug prints.
 *
 * Return: filename string or "unknown".
 */
char *inode_to_filename(const struct inode *inode)
{
	struct dentry *dentry = NULL;
	char *filename = NULL;

	if (!inode)
		return "NULL";

	if (hlist_empty(&inode->i_dentry))
		return "unknown";

	dentry = hlist_entry(inode->i_dentry.first, struct dentry, d_u.d_alias);
	filename = dentry->d_iname;

	return filename;
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
		pr_debug("inode on direct-io, inode = 0x%pK.\n", inode);

		return inode;
	}

	if (!page_mapping(bio->bi_io_vec->bv_page))
		return NULL;

	return page_mapping(bio->bi_io_vec->bv_page)->host;
}

/**
 * pfk_key_size_to_key_type() - translate key size to key size enum
 * @key_size: key size in bytes
 * @key_size_type: pointer to store the output enum (can be null)
 *
 * return 0 in case of success, error otherwise (i.e not supported key size)
 */
int pfk_key_size_to_key_type(size_t key_size,
	enum ice_crpto_key_size *key_size_type)
{
	/*
	 *  currently only 32 bit key size is supported
	 *  in the future, table with supported key sizes might
	 *  be introduced
	 */

	if (key_size != PFK_SUPPORTED_KEY_SIZE) {
		pr_err("not supported key size %zu\n", key_size);
		return -EINVAL;
	}

	if (key_size_type)
		*key_size_type = ICE_CRYPTO_KEY_SIZE_256;

	return 0;
}

/*
 * Retrieves filesystem type from inode's superblock
 */
bool pfe_is_inode_filesystem_type(const struct inode *inode,
	const char *fs_type)
{
	if (!inode || !fs_type)
		return false;

	if (!inode->i_sb)
		return false;

	if (!inode->i_sb->s_type)
		return false;

	return (strcmp(inode->i_sb->s_type->name, fs_type) == 0);
}

/**
 * pfk_get_key_for_bio() - get the encryption key to be used for a bio
 *
 * @bio: pointer to the BIO
 * @key_info: pointer to the key information which will be filled in
 * @algo_mode: optional pointer to the algorithm identifier which will be set
 * @is_pfe: will be set to false if the BIO should be left unencrypted
 *
 * Return: 0 if a key is being used, otherwise a -errno value
 */
static int pfk_get_key_for_bio(const struct bio *bio,
		struct pfk_key_info *key_info,
		enum ice_cryto_algo_mode *algo_mode,
		bool *is_pfe, unsigned int *data_unit)
{
	const struct inode *inode;
	enum pfe_type which_pfe;
	char *s_type = NULL;
	const struct blk_encryption_key *key = NULL;

	inode = pfk_bio_get_inode(bio);
	which_pfe = pfk_get_pfe_type(inode);
	s_type = (char *)pfk_kc_get_storage_type();

	if (data_unit && (bio_dun(bio) ||
			!memcmp(s_type, "ufs", strlen("ufs"))))
		*data_unit = 1 << ICE_CRYPTO_DATA_UNIT_4_KB;

	if (which_pfe != INVALID_PFE) {
		/* Encrypted file; override ->bi_crypt_key */
		pr_debug("parsing inode %lu with PFE type %d\n",
			 inode->i_ino, which_pfe);
		return (*(pfk_parse_inode_ftable[which_pfe]))
				(bio, inode, key_info, algo_mode, is_pfe,
					(const char *)s_type);
	}

	/*
	 * bio is not for an encrypted file.  Use ->bi_crypt_key if it was set.
	 * Otherwise, don't encrypt/decrypt the bio.
	 */
#ifdef CONFIG_DM_DEFAULT_KEY
	key = bio->bi_crypt_key;
#endif
	if (!key) {
		*is_pfe = false;
		return -EINVAL;
	}

	/* Note: the "salt" is really just the second half of the XTS key. */
	BUILD_BUG_ON(sizeof(key->raw) !=
		     PFK_SUPPORTED_KEY_SIZE + PFK_SUPPORTED_SALT_SIZE);
	key_info->key = &key->raw[0];
	key_info->key_size = PFK_SUPPORTED_KEY_SIZE;
	key_info->salt = &key->raw[PFK_SUPPORTED_KEY_SIZE];
	key_info->salt_size = PFK_SUPPORTED_SALT_SIZE;
	if (algo_mode)
		*algo_mode = ICE_CRYPTO_ALGO_MODE_AES_XTS;
	return 0;
}


/**
 * pfk_load_key_start() - loads PFE encryption key to the ICE
 *			  Can also be invoked from non
 *			  PFE context, in this case it
 *			  is not relevant and is_pfe
 *			  flag is set to false
 *
 * @bio: Pointer to the BIO structure
 * @ice_setting: Pointer to ice setting structure that will be filled with
 * ice configuration values, including the index to which the key was loaded
 *  @is_pfe: will be false if inode is not relevant to PFE, in such a case
 * it should be treated as non PFE by the block layer
 *
 * Returns the index where the key is stored in encryption hw and additional
 * information that will be used later for configuration of the encryption hw.
 *
 * Must be followed by pfk_load_key_end when key is no longer used by ice
 *
 */
int pfk_load_key_start(const struct bio *bio,
		struct ice_crypto_setting *ice_setting, bool *is_pfe,
		bool async, int ice_rev)
{
	int ret = 0;
	struct pfk_key_info key_info = {NULL, NULL, 0, 0};
	enum ice_cryto_algo_mode algo_mode = ICE_CRYPTO_ALGO_MODE_AES_XTS;
	enum ice_crpto_key_size key_size_type = 0;
	unsigned int data_unit = 1 << ICE_CRYPTO_DATA_UNIT_512_B;
	u32 key_index = 0;

	if (!is_pfe) {
		pr_err("is_pfe is NULL\n");
		return -EINVAL;
	}

	/*
	 * only a few errors below can indicate that
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

	ret = pfk_get_key_for_bio(bio, &key_info, &algo_mode, is_pfe,
					&data_unit);

	if (ret != 0)
		return ret;

	ret = pfk_key_size_to_key_type(key_info.key_size, &key_size_type);
	if (ret != 0)
		return ret;

	ret = pfk_kc_load_key_start(key_info.key, key_info.key_size,
			key_info.salt, key_info.salt_size, &key_index, async,
			data_unit, ice_rev);
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

	pr_debug("loaded key for file %s key_index %d\n",
		inode_to_filename(pfk_bio_get_inode(bio)), key_index);

	return 0;
}

/**
 * pfk_load_key_end() - marks the PFE key as no longer used by ICE
 *			Can also be invoked from non
 *			PFE context, in this case it is not
 *			relevant and is_pfe flag is
 *			set to false
 *
 * @bio: Pointer to the BIO structure
 * @is_pfe: Pointer to is_pfe flag, which will be true if function was invoked
 *			from PFE context
 */
int pfk_load_key_end(const struct bio *bio, bool *is_pfe)
{
	int ret = 0;
	struct pfk_key_info key_info = {NULL, NULL, 0, 0};

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

	ret = pfk_get_key_for_bio(bio, &key_info, NULL, is_pfe, NULL);
	if (ret != 0)
		return ret;

	pfk_kc_load_key_end(key_info.key, key_info.key_size,
		key_info.salt, key_info.salt_size);

	pr_debug("finished using key for file %s\n",
		inode_to_filename(pfk_bio_get_inode(bio)));

	return 0;
}

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
bool pfk_allow_merge_bio(const struct bio *bio1, const struct bio *bio2)
{
	const struct blk_encryption_key *key1 = NULL;
	const struct blk_encryption_key *key2 = NULL;
	const struct inode *inode1;
	const struct inode *inode2;
	enum pfe_type which_pfe1;
	enum pfe_type which_pfe2;

#ifdef CONFIG_DM_DEFAULT_KEY
	key1 = bio1->bi_crypt_key;
	key2 = bio2->bi_crypt_key;
#endif

	if (!pfk_is_ready())
		return false;

	if (!bio1 || !bio2)
		return false;

	if (bio1 == bio2)
		return true;

	key1 = bio1->bi_crypt_key;
	key2 = bio2->bi_crypt_key;

	inode1 = pfk_bio_get_inode(bio1);
	inode2 = pfk_bio_get_inode(bio2);

	which_pfe1 = pfk_get_pfe_type(inode1);
	which_pfe2 = pfk_get_pfe_type(inode2);

	/*
	 * If one bio is for an encrypted file and the other is for a different
	 * type of encrypted file or for blocks that are not part of an
	 * encrypted file, do not merge.
	 */
	if (which_pfe1 != which_pfe2)
		return false;

	if (which_pfe1 != INVALID_PFE) {
		/* Both bios are for the same type of encrypted file. */
	return (*(pfk_allow_merge_bio_ftable[which_pfe1]))(bio1, bio2,
		inode1, inode2);
	}

	/*
	 * Neither bio is for an encrypted file.  Merge only if the default keys
	 * are the same (or both are NULL).
	 */
	return key1 == key2 ||
		(key1 && key2 &&
		 !crypto_memneq(key1->raw, key2->raw, sizeof(key1->raw)));
}

/**
 * Flush key table on storage core reset. During core reset key configuration
 * is lost in ICE. We need to flash the cache, so that the keys will be
 * reconfigured again for every subsequent transaction
 */
void pfk_clear_on_reset(void)
{
	if (!pfk_is_ready())
		return;

	pfk_kc_clear_on_reset();
}

module_init(pfk_init);
module_exit(pfk_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Per-File-Key driver");
