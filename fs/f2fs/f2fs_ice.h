/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#ifndef _F2FS_ICE_H
#define _F2FS_ICE_H

#include "f2fs.h"
#include "f2fs_crypto.h"

#ifdef CONFIG_F2FS_FS_ICE_ENCRYPTION
static inline int f2fs_should_be_processed_by_ice(const struct inode *inode)
{
	if (!f2fs_encrypted_inode((struct inode *)inode))
		return 0;

	return f2fs_using_hardware_encryption((struct inode *)inode);
}

static inline int f2fs_is_ice_enabled(void)
{
	return 1;
}

int f2fs_is_aes_xts_cipher(const struct inode *inode);

char *f2fs_get_ice_encryption_key(const struct inode *inode);
char *f2fs_get_ice_encryption_salt(const struct inode *inode);

int f2fs_is_ice_encryption_info_equal(const struct inode *inode1,
	const struct inode *inode2);

static inline size_t f2fs_get_ice_encryption_key_size(
	const struct inode *inode)
{
	return F2FS_AES_256_XTS_KEY_SIZE / 2;
}

static inline size_t f2fs_get_ice_encryption_salt_size(
	const struct inode *inode)
{
	return F2FS_AES_256_XTS_KEY_SIZE / 2;
}

#else
static inline int f2fs_should_be_processed_by_ice(const struct inode *inode)
{
	return 0;
}
static inline int f2fs_is_ice_enabled(void)
{
	return 0;
}

static inline char *f2fs_get_ice_encryption_key(const struct inode *inode)
{
	return NULL;
}

static inline char *f2fs_get_ice_encryption_salt(const struct inode *inode)
{
	return NULL;
}

static inline size_t f2fs_get_ice_encryption_key_size(
	const struct inode *inode)
{
	return 0;
}

static inline size_t f2fs_get_ice_encryption_salt_size(
	const struct inode *inode)
{
	return 0;
}

static inline int f2fs_is_xts_cipher(const struct inode *inode)
{
	return 0;
}

static inline int f2fs_is_ice_encryption_info_equal(
	const struct inode *inode1,
	const struct inode *inode2)
{
	return 0;
}

static inline int f2fs_is_aes_xts_cipher(const struct inode *inode)
{
	return 0;
}

#endif

#endif	/* _F2FS_ICE_H */
