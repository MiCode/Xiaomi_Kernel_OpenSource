/* Copyright (c) 2020, The Linux Foundation. All rights reserved.
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

#ifndef _CRYPTO_QTI_PLATFORM_H
#define _CRYPTO_QTI_PLATFORM_H

#include <linux/bio-crypt-ctx.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/device.h>

#if IS_ENABLED(CONFIG_QTI_CRYPTO_TZ)
int crypto_qti_program_key(struct crypto_vops_qti_entry *ice_entry,
			   const struct blk_crypto_key *key, unsigned int slot,
			   unsigned int data_unit_mask, int capid);
int crypto_qti_invalidate_key(struct crypto_vops_qti_entry *ice_entry,
			      unsigned int slot);
int crypto_qti_tz_raw_secret(const u8 *wrapped_key,
			     unsigned int wrapped_key_size, u8 *secret,
			     unsigned int secret_size);
#else
static inline int crypto_qti_program_key(
				struct crypto_vops_qti_entry *ice_entry,
				const struct blk_crypto_key *key,
				unsigned int slot, unsigned int data_unit_mask,
				int capid)
{
	return 0;
}
static inline int crypto_qti_invalidate_key(
		struct crypto_vops_qti_entry *ice_entry, unsigned int slot)
{
	return 0;
}
static int crypto_qti_tz_raw_secret(u8 *wrapped_key,
				    unsigned int wrapped_key_size, u8 *secret,
				    unsigned int secret_size)
{
	return 0;
}
#endif /* CONFIG_QTI_CRYPTO_TZ */

static inline void crypto_qti_disable_platform(
				struct crypto_vops_qti_entry *ice_entry)
{}

#endif /* _CRYPTO_QTI_PLATFORM_H */
