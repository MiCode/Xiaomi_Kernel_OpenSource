/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#include "ext4_ice.h"
#include "fscrypt_private.h"

/*
 * Retrieves encryption key from the inode
 */
char *ext4_get_ice_encryption_key(const struct inode *inode)
{
	struct fscrypt_info *ci = NULL;

	if (!inode)
		return NULL;

	ci = inode->i_crypt_info;
	if (!ci)
		return NULL;

	return &(ci->ci_raw_key[0]);
}

/*
 * Retrieves encryption salt from the inode
 */
char *ext4_get_ice_encryption_salt(const struct inode *inode)
{
	struct fscrypt_info *ci = NULL;

	if (!inode)
		return NULL;

	ci = inode->i_crypt_info;
	if (!ci)
		return NULL;

	return &(ci->ci_raw_key[ext4_get_ice_encryption_key_size(inode)]);
}

/*
 * returns true if the cipher mode in inode is AES XTS
 */
int ext4_is_aes_xts_cipher(const struct inode *inode)
{
	struct fscrypt_info *ci = NULL;

	ci = inode->i_crypt_info;
	if (!ci)
		return 0;

	return (ci->ci_data_mode == FS_ENCRYPTION_MODE_PRIVATE);
}

/*
 * returns true if encryption info in both inodes is equal
 */
int ext4_is_ice_encryption_info_equal(const struct inode *inode1,
	const struct inode *inode2)
{
	char *key1 = NULL;
	char *key2 = NULL;
	char *salt1 = NULL;
	char *salt2 = NULL;

	if (!inode1 || !inode2)
		return 0;

	if (inode1 == inode2)
		return 1;

	/* both do not belong to ice, so we don't care, they are equal for us */
	if (!ext4_should_be_processed_by_ice(inode1) &&
		!ext4_should_be_processed_by_ice(inode2))
		return 1;

	/* one belongs to ice, the other does not -> not equal */
	if (ext4_should_be_processed_by_ice(inode1) ^
		ext4_should_be_processed_by_ice(inode2))
		return 0;

	key1 = ext4_get_ice_encryption_key(inode1);
	key2 = ext4_get_ice_encryption_key(inode2);
	salt1 = ext4_get_ice_encryption_salt(inode1);
	salt2 = ext4_get_ice_encryption_salt(inode2);

	/* key and salt should not be null by this point */
	if (!key1 || !key2 || !salt1 || !salt2 ||
		(ext4_get_ice_encryption_key_size(inode1) !=
		 ext4_get_ice_encryption_key_size(inode2)) ||
		(ext4_get_ice_encryption_salt_size(inode1) !=
		 ext4_get_ice_encryption_salt_size(inode2)))
		return 0;

	return ((memcmp(key1, key2,
			ext4_get_ice_encryption_key_size(inode1)) == 0) &&
		(memcmp(salt1, salt2,
			ext4_get_ice_encryption_salt_size(inode1)) == 0));
}
