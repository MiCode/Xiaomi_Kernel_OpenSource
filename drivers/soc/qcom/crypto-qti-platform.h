/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef _CRYPTO_QTI_PLATFORM_H
#define _CRYPTO_QTI_PLATFORM_H

#include <linux/bio-crypt-ctx.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/device.h>

#if IS_ENABLED(CONFIG_QTI_CRYPTO_COMMON)
int crypto_qti_program_key(struct crypto_vops_qti_entry *ice_entry,
			   const struct blk_crypto_key *key,
			   unsigned int slot,
			   unsigned int data_unit_mask, int capid);
int crypto_qti_invalidate_key(struct crypto_vops_qti_entry *ice_entry,
			      unsigned int slot);
int crypto_qti_derive_raw_secret_platform(
				struct crypto_vops_qti_entry *ice_entry,
				const u8 *wrapped_key,
				unsigned int wrapped_key_size, u8 *secret,
				unsigned int secret_size);

#if IS_ENABLED(CONFIG_QTI_HW_KEY_MANAGER)
void crypto_qti_disable_platform(struct crypto_vops_qti_entry *ice_entry);
#else
static inline void crypto_qti_disable_platform(
				struct crypto_vops_qti_entry *ice_entry)
{}
#endif /* CONFIG_QTI_HW_KEY_MANAGER */
#else
static inline int crypto_qti_program_key(
				struct crypto_vops_qti_entry *ice_entry,
				const struct blk_crypto_key *key,
				unsigned int slot,
				unsigned int data_unit_mask, int capid)
{
	return -EOPNOTSUPP;
}
static inline int crypto_qti_invalidate_key(
		struct crypto_vops_qti_entry *ice_entry, unsigned int slot)
{
	return -EOPNOTSUPP;
}
static inline int crypto_qti_derive_raw_secret_platform(
				struct crypto_vops_qti_entry *ice_entry,
				const u8 *wrapped_key,
				unsigned int wrapped_key_size, u8 *secret,
				unsigned int secret_size)
{
	return -EOPNOTSUPP;
}

static inline void crypto_qti_disable_platform(
				struct crypto_vops_qti_entry *ice_entry)
{}
#endif /* CONFIG_QTI_CRYPTO_TZ || CONFIG_QTI_HW_KEY_MANAGER */
#endif /* _CRYPTO_QTI_PLATFORM_H */
