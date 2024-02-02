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

#ifndef _CRYPTO_QTI_COMMON_H
#define _CRYPTO_QTI_COMMON_H

#include <linux/bio-crypt-ctx.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/delay.h>

#define RAW_SECRET_SIZE 32
#define QTI_ICE_MAX_BIST_CHECK_COUNT 100
#define QTI_ICE_TYPE_NAME_LEN 8

struct crypto_vops_qti_entry {
	void __iomem *icemmio_base;
	uint32_t ice_hw_version;
	uint8_t ice_dev_type[QTI_ICE_TYPE_NAME_LEN];
	uint32_t flags;
};

#if IS_ENABLED(CONFIG_QTI_CRYPTO_COMMON)
// crypto-qti-common.c
int crypto_qti_init_crypto(struct device *dev, void __iomem *mmio_base,
			   void **priv_data);
int crypto_qti_enable(void *priv_data);
void crypto_qti_disable(void *priv_data);
int crypto_qti_resume(void *priv_data);
int crypto_qti_debug(void *priv_data);
int crypto_qti_keyslot_program(void *priv_data,
			       const struct blk_crypto_key *key,
			       unsigned int slot, u8 data_unit_mask,
			       int capid);
int crypto_qti_keyslot_evict(void *priv_data, unsigned int slot);
int crypto_qti_derive_raw_secret(const u8 *wrapped_key,
				 unsigned int wrapped_key_size, u8 *secret,
				 unsigned int secret_size);

#else
static inline int crypto_qti_init_crypto(struct device *dev,
					 void __iomem *mmio_base,
					 void **priv_data)
{
	return 0;
}
static inline int crypto_qti_enable(void *priv_data)
{
	return 0;
}
static inline void crypto_qti_disable(void *priv_data)
{
	return 0;
}
static inline int crypto_qti_resume(void *priv_data)
{
	return 0;
}
static inline int crypto_qti_debug(void *priv_data)
{
	return 0;
}
static inline int crypto_qti_keyslot_program(void *priv_data,
					     const struct blk_crypto_key *key,
					     unsigned int slot,
					     u8 data_unit_mask,
					     int capid)
{
	return 0;
}
static inline int crypto_qti_keyslot_evict(void *priv_data, unsigned int slot)
{
	return 0;
}
static inline int crypto_qti_derive_raw_secret(const u8 *wrapped_key,
					       unsigned int wrapped_key_size,
					       u8 *secret,
					       unsigned int secret_size)
{
	return 0;
}

#endif /* CONFIG_QTI_CRYPTO_COMMON */

#endif /* _CRYPTO_QTI_COMMON_H */
