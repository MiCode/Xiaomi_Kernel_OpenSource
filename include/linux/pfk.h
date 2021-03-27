/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2021, The Linux Foundation. All rights reserved.
 */

#ifndef PFK_H_
#define PFK_H_

#include <linux/bio.h>


#ifdef CONFIG_PFK

/*
 * Default key for inline encryption.
 *
 * For now only AES-256-XTS is supported, so this is a fixed length.  But if
 * ever needed, this should be made variable-length with a 'mode' and 'size'.
 * (Remember to update pfk_allow_merge_bio() when doing so!)
 */
#define BLK_ENCRYPTION_KEY_SIZE_AES_256_XTS 64

struct blk_encryption_key {
	u8 raw[BLK_ENCRYPTION_KEY_SIZE_AES_256_XTS];
};

int pfk_fbe_clear_key(const unsigned char *key, size_t key_size,
		const unsigned char *salt, size_t salt_size);

#else
static inline int pfk_fbe_clear_key(const unsigned char *key, size_t key_size,
			const unsigned char *salt, size_t salt_size)
{
	return -ENODEV;
}

#endif /* CONFIG_PFK */

#endif /* PFK_H */
