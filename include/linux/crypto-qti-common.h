/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CRYPTO_QTI_COMMON_H
#define _CRYPTO_QTI_COMMON_H

#include <linux/blk-crypto.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/delay.h>

#define RAW_SECRET_SIZE 32
#define QTI_ICE_MAX_BIST_CHECK_COUNT 100
#define QTI_ICE_TYPE_NAME_LEN 8

#if IS_ENABLED(CONFIG_QTI_CRYPTO_COMMON)
int crypto_qti_init_crypto(void *mmio_data);
int crypto_qti_enable(void *mmio_data);
void crypto_qti_disable(void *mmio_data);
int crypto_qti_resume(void *mmio_data);
int crypto_qti_debug(void *mmio_data);
int crypto_qti_keyslot_program(void *mmio_data,
			       const struct blk_crypto_key *key,
			       unsigned int slot, u8 data_unit_mask,
			       int capid);
int crypto_qti_keyslot_evict(void *mmio_data, unsigned int slot);
int crypto_qti_derive_raw_secret(void *mmio_data,
				 const u8 *wrapped_key,
				 unsigned int wrapped_key_size, u8 *secret,
				 unsigned int secret_size);

#else
static inline int crypto_qti_init_crypto(void *mmio_data)
{
	return -EOPNOTSUPP;
}
static inline int crypto_qti_enable(void *mmio_data)
{
	return -EOPNOTSUPP;
}

static inline void crypto_qti_disable(void *mmio_data)
{
	return;
}
static inline int crypto_qti_resume(void *mmio_data)
{
	return -EOPNOTSUPP;
}
static inline int crypto_qti_debug(void *mmio_data)
{
	return -EOPNOTSUPP;
}
static inline int crypto_qti_keyslot_program(void *mmio_data,
					     const struct blk_crypto_key *key,
					     unsigned int slot,
					     u8 data_unit_mask,
					     int capid)
{
	return -EOPNOTSUPP;
}
static inline int crypto_qti_keyslot_evict(void *mmio_data, unsigned int slot)
{
	return -EOPNOTSUPP;
}
static inline int crypto_qti_derive_raw_secret(void *mmio_data,
					       const u8 *wrapped_key,
					       unsigned int wrapped_key_size,
					       u8 *secret,
					       unsigned int secret_size)
{
	return -EOPNOTSUPP;
}

#endif /* CONFIG_QTI_CRYPTO_COMMON */

#endif /* _CRYPTO_QTI_COMMON_H */
