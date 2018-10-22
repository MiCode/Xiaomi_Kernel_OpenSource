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
 * Per-File-Key (PFK) - EXT4
 *
 * This driver is used for working with EXT4 crypt extension
 *
 * The key information  is stored in node by EXT4 when file is first opened
 * and will be later accessed by Block Device Driver to actually load the key
 * to encryption hw.
 *
 * PFK exposes API's for loading and removing keys from encryption hw
 * and also API to determine whether 2 adjacent blocks can be agregated by
 * Block Layer in one request to encryption hw.
 *
 */


/* Uncomment the line below to enable debug messages */
/* #define DEBUG 1 */
#define pr_fmt(fmt)	"pfk_ext4 [%s]: " fmt, __func__

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/printk.h>

#include "fscrypt_ice.h"
#include "pfk_ext4.h"
//#include "ext4_ice.h"

static bool pfk_ext4_ready;

/*
 * pfk_ext4_deinit() - Deinit function, should be invoked by upper PFK layer
 */
void pfk_ext4_deinit(void)
{
	pfk_ext4_ready = false;
}

/*
 * pfk_ecryptfs_init() - Init function, should be invoked by upper PFK layer
 */
int __init pfk_ext4_init(void)
{
	pfk_ext4_ready = true;
	pr_info("PFK EXT4 inited successfully\n");

	return 0;
}

/**
 * pfk_ecryptfs_is_ready() - driver is initialized and ready.
 *
 * Return: true if the driver is ready.
 */
static inline bool pfk_ext4_is_ready(void)
{
	return pfk_ext4_ready;
}

/**
 * pfk_ext4_dump_inode() - dumps all interesting info about inode to the screen
 *
 *
 */
/*
 * static void pfk_ext4_dump_inode(const struct inode* inode)
 * {
 *	struct ext4_crypt_info *ci = ext4_encryption_info((struct inode*)inode);
 *
 *	pr_debug("dumping inode with address 0x%p\n", inode);
 *	pr_debug("S_ISREG is %d\n", S_ISREG(inode->i_mode));
 *	pr_debug("EXT4_INODE_ENCRYPT flag is %d\n",
 *		ext4_test_inode_flag((struct inode*)inode, EXT4_INODE_ENCRYPT));
 *	if (ci) {
 *		pr_debug("crypt_info address 0x%p\n", ci);
 *		pr_debug("ci->ci_data_mode %d\n", ci->ci_data_mode);
 *	} else {
 *		pr_debug("crypt_info is NULL\n");
 *	}
 * }
 */

/**
 * pfk_is_ext4_type() - return true if inode belongs to ICE EXT4 PFE
 * @inode: inode pointer
 */
bool pfk_is_ext4_type(const struct inode *inode)
{
	if (!pfe_is_inode_filesystem_type(inode, "ext4"))
		return false;

	return fscrypt_should_be_processed_by_ice(inode);
}

/**
 * pfk_ext4_parse_cipher() - parse cipher from inode to enum
 * @inode: inode
 * @algo: pointer to store the output enum (can be null)
 *
 * return 0 in case of success, error otherwise (i.e not supported cipher)
 */
static int pfk_ext4_parse_cipher(const struct inode *inode,
	enum ice_cryto_algo_mode *algo)
{
	/*
	 * currently only AES XTS algo is supported
	 * in the future, table with supported ciphers might
	 * be introduced
	 */

	if (!inode)
		return -EINVAL;

	if (!fscrypt_is_aes_xts_cipher(inode)) {
		pr_err("ext4 alghoritm is not supported by pfk\n");
		return -EINVAL;
	}

	if (algo)
		*algo = ICE_CRYPTO_ALGO_MODE_AES_XTS;

	return 0;
}


int pfk_ext4_parse_inode(const struct bio *bio,
	const struct inode *inode,
	struct pfk_key_info *key_info,
	enum ice_cryto_algo_mode *algo,
	bool *is_pfe,
	const char *storage_type)
{
	int ret = 0;

	if (!is_pfe)
		return -EINVAL;

	/*
	 * only a few errors below can indicate that
	 * this function was not invoked within PFE context,
	 * otherwise we will consider it PFE
	 */
	*is_pfe = true;

	if (!pfk_ext4_is_ready())
		return -ENODEV;

	if (!inode)
		return -EINVAL;

	if (!key_info)
		return -EINVAL;

	key_info->key = fscrypt_get_ice_encryption_key(inode);
	if (!key_info->key) {
		pr_err("could not parse key from ext4\n");
		return -EINVAL;
	}

	key_info->key_size = fscrypt_get_ice_encryption_key_size(inode);
	if (!key_info->key_size) {
		pr_err("could not parse key size from ext4\n");
		return -EINVAL;
	}

	key_info->salt = fscrypt_get_ice_encryption_salt(inode);
	if (!key_info->salt) {
		pr_err("could not parse salt from ext4\n");
		return -EINVAL;
	}

	key_info->salt_size = fscrypt_get_ice_encryption_salt_size(inode);
	if (!key_info->salt_size) {
		pr_err("could not parse salt size from ext4\n");
		return -EINVAL;
	}

	ret = pfk_ext4_parse_cipher(inode, algo);
	if (ret != 0) {
		pr_err("not supported cipher\n");
		return ret;
	}

	return 0;
}

bool pfk_ext4_allow_merge_bio(const struct bio *bio1,
	const struct bio *bio2, const struct inode *inode1,
	const struct inode *inode2)
{
	/* if there is no ext4 pfk, don't disallow merging blocks */
	if (!pfk_ext4_is_ready())
		return true;

	if (!inode1 || !inode2)
		return false;

	return fscrypt_is_ice_encryption_info_equal(inode1, inode2);
}
