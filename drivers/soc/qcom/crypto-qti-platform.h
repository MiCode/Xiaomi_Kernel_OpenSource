/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CRYPTO_QTI_PLATFORM_H
#define _CRYPTO_QTI_PLATFORM_H

#include <linux/blk-crypto.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/device.h>

#if IS_ENABLED(CONFIG_QTI_CRYPTO_COMMON)
int crypto_qti_program_key(void __iomem *ice_mmio,
			   const struct blk_crypto_key *key,
			   unsigned int slot,
			   unsigned int data_unit_mask, int capid);
int crypto_qti_invalidate_key(void __iomem *ice_mmio,
			      unsigned int slot);
int crypto_qti_derive_raw_secret_platform(
				void __iomem *ice_mmio,
				const u8 *wrapped_key,
				unsigned int wrapped_key_size, u8 *secret,
				unsigned int secret_size);

#if IS_ENABLED(CONFIG_QTI_HW_KEY_MANAGER)
void crypto_qti_disable_platform(void __iomem *ice_mmio);
#else
static inline void crypto_qti_disable_platform(
				void __iomem *ice_mmio)
{}
#endif /* CONFIG_QTI_HW_KEY_MANAGER */
#else
static inline int crypto_qti_program_key(
				void __iomem *ice_mmio,
				const struct blk_crypto_key *key,
				unsigned int slot,
				unsigned int data_unit_mask, int capid)
{
	return -EOPNOTSUPP;
}
static inline int crypto_qti_invalidate_key(
		void __iomem *ice_mmio, unsigned int slot)
{
	return -EOPNOTSUPP;
}
static inline int crypto_qti_derive_raw_secret_platform(
				void __iomem *ice_mmio,
				const u8 *wrapped_key,
				unsigned int wrapped_key_size, u8 *secret,
				unsigned int secret_size)
{
	return -EOPNOTSUPP;
}

static inline void crypto_qti_disable_platform(
				void __iomem *ice_mmio)
{}
#endif /* CONFIG_QTI_CRYPTO_TZ || CONFIG_QTI_HW_KEY_MANAGER */
#endif /* _CRYPTO_QTI_PLATFORM_H */
