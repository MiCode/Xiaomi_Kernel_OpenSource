/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _FSCRYPT_ICE_H
#define _FSCRYPT_ICE_H

#include <linux/blkdev.h>
#include "fscrypt_private.h"

#if IS_ENABLED(CONFIG_FS_ENCRYPTION)
static inline bool fscrypt_should_be_processed_by_ice(const struct inode *inode)
{
	if (!inode->i_sb->s_cop)
		return false;
	if (!inode->i_sb->s_cop->is_encrypted((struct inode *)inode))
		return false;

	return fscrypt_using_hardware_encryption(inode);
}

static inline int fscrypt_is_ice_capable(const struct super_block *sb)
{
	return blk_queue_inlinecrypt(bdev_get_queue(sb->s_bdev));
}

int fscrypt_is_aes_xts_cipher(const struct inode *inode);

char *fscrypt_get_ice_encryption_key(const struct inode *inode);
char *fscrypt_get_ice_encryption_salt(const struct inode *inode);

bool fscrypt_is_ice_encryption_info_equal(const struct inode *inode1,
					const struct inode *inode2);

static inline size_t fscrypt_get_ice_encryption_key_size(
					const struct inode *inode)
{
	return FS_AES_256_XTS_KEY_SIZE / 2;
}

static inline size_t fscrypt_get_ice_encryption_salt_size(
					const struct inode *inode)
{
	return FS_AES_256_XTS_KEY_SIZE / 2;
}
#else
static inline bool fscrypt_should_be_processed_by_ice(const struct inode *inode)
{
	return false;
}

static inline int fscrypt_is_ice_capable(const struct super_block *sb)
{
	return false;
}

static inline char *fscrypt_get_ice_encryption_key(const struct inode *inode)
{
	return NULL;
}

static inline char *fscrypt_get_ice_encryption_salt(const struct inode *inode)
{
	return NULL;
}

static inline size_t fscrypt_get_ice_encryption_key_size(
					const struct inode *inode)
{
	return 0;
}

static inline size_t fscrypt_get_ice_encryption_salt_size(
					const struct inode *inode)
{
	return 0;
}

static inline int fscrypt_is_xts_cipher(const struct inode *inode)
{
	return 0;
}

static inline bool fscrypt_is_ice_encryption_info_equal(
					const struct inode *inode1,
					const struct inode *inode2)
{
	return false;
}

static inline int fscrypt_is_aes_xts_cipher(const struct inode *inode)
{
	return 0;
}

#endif

#endif	/* _FSCRYPT_ICE_H */
