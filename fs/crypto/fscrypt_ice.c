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

#include "fscrypt_ice.h"

int fscrypt_using_hardware_encryption(const struct inode *inode)
{
	struct fscrypt_info *ci = inode->i_crypt_info;

	return S_ISREG(inode->i_mode) && ci &&
		ci->ci_data_mode == FS_ENCRYPTION_MODE_PRIVATE;
}
EXPORT_SYMBOL(fscrypt_using_hardware_encryption);

/*
 * Retrieves encryption key from the inode
 */
char *fscrypt_get_ice_encryption_key(const struct inode *inode)
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
char *fscrypt_get_ice_encryption_salt(const struct inode *inode)
{
	struct fscrypt_info *ci = NULL;

	if (!inode)
		return NULL;

	ci = inode->i_crypt_info;
	if (!ci)
		return NULL;

	return &(ci->ci_raw_key[fscrypt_get_ice_encryption_key_size(inode)]);
}

/*
 * returns true if the cipher mode in inode is AES XTS
 */
int fscrypt_is_aes_xts_cipher(const struct inode *inode)
{
	struct fscrypt_info *ci = inode->i_crypt_info;

	if (!ci)
		return 0;

	return (ci->ci_data_mode == FS_ENCRYPTION_MODE_PRIVATE);
}

/*
 * returns true if encryption info in both inodes is equal
 */
bool fscrypt_is_ice_encryption_info_equal(const struct inode *inode1,
					const struct inode *inode2)
{
	char *key1 = NULL;
	char *key2 = NULL;
	char *salt1 = NULL;
	char *salt2 = NULL;

	if (!inode1 || !inode2)
		return false;

	if (inode1 == inode2)
		return true;

	/* both do not belong to ice, so we don't care, they are equal
	 *for us
	 */
	if (!fscrypt_should_be_processed_by_ice(inode1) &&
			!fscrypt_should_be_processed_by_ice(inode2))
		return true;

	/* one belongs to ice, the other does not -> not equal */
	if (fscrypt_should_be_processed_by_ice(inode1) ^
			fscrypt_should_be_processed_by_ice(inode2))
		return false;

	key1 = fscrypt_get_ice_encryption_key(inode1);
	key2 = fscrypt_get_ice_encryption_key(inode2);
	salt1 = fscrypt_get_ice_encryption_salt(inode1);
	salt2 = fscrypt_get_ice_encryption_salt(inode2);

	/* key and salt should not be null by this point */
	if (!key1 || !key2 || !salt1 || !salt2 ||
		(fscrypt_get_ice_encryption_key_size(inode1) !=
		 fscrypt_get_ice_encryption_key_size(inode2)) ||
		(fscrypt_get_ice_encryption_salt_size(inode1) !=
		 fscrypt_get_ice_encryption_salt_size(inode2)))
		return false;

	if ((memcmp(key1, key2,
			fscrypt_get_ice_encryption_key_size(inode1)) == 0) &&
		(memcmp(salt1, salt2,
			fscrypt_get_ice_encryption_salt_size(inode1)) == 0))
		return true;

	return false;
}

void fscrypt_set_ice_dun(const struct inode *inode, struct bio *bio, u64 dun)
{
	if (fscrypt_should_be_processed_by_ice(inode))
		bio->bi_iter.bi_dun = dun;
}
EXPORT_SYMBOL(fscrypt_set_ice_dun);

void fscrypt_set_ice_skip(struct bio *bio, int bi_crypt_skip)
{
#ifdef CONFIG_DM_DEFAULT_KEY
	bio->bi_crypt_skip = bi_crypt_skip;
#endif
}
EXPORT_SYMBOL(fscrypt_set_ice_skip);

/*
 * This function will be used for filesystem when deciding to merge bios.
 * Basic assumption is, if inline_encryption is set, single bio has to
 * guarantee consecutive LBAs as well as ino|pg->index.
 */
bool fscrypt_mergeable_bio(struct bio *bio, u64 dun, bool bio_encrypted,
						int bi_crypt_skip)
{
	if (!bio)
		return true;

#ifdef CONFIG_DM_DEFAULT_KEY
	if (bi_crypt_skip != bio->bi_crypt_skip)
		return false;
#endif
	/* if both of them are not encrypted, no further check is needed */
	if (!bio_dun(bio) && !bio_encrypted)
		return true;

	/* ICE allows only consecutive iv_key stream. */
	return bio_end_dun(bio) == dun;
}
EXPORT_SYMBOL(fscrypt_mergeable_bio);
