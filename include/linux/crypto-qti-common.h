/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CRYPTO_QTI_COMMON_H
#define _CRYPTO_QTI_COMMON_H

#include <linux/bio-crypt-ctx.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/platform_device.h>

#define RAW_SECRET_SIZE 32
#define QTI_ICE_MAX_BIST_CHECK_COUNT 100
#define QTI_ICE_TYPE_NAME_LEN 8

struct crypto_vops_qti_entry {
	void __iomem *icemmio_base;
	void __iomem *hwkm_slave_mmio_base;
	uint32_t ice_hw_version;
	uint8_t ice_dev_type[QTI_ICE_TYPE_NAME_LEN];
	uint32_t flags;
};

#if IS_ENABLED(CONFIG_QTI_CRYPTO_COMMON)
int crypto_qti_init_crypto(struct device *dev, void __iomem *mmio_base,
			   void __iomem *hwkm_slave_mmio_base, void **priv_data);
int crypto_qti_enable(void *priv_data);
void crypto_qti_disable(void *priv_data);
int crypto_qti_resume(void *priv_data);
int crypto_qti_debug(void *priv_data);
int crypto_qti_keyslot_program(void *priv_data,
			       const struct blk_crypto_key *key,
			       unsigned int slot, u8 data_unit_mask,
			       int capid);
int crypto_qti_keyslot_evict(void *priv_data, unsigned int slot);
int crypto_qti_derive_raw_secret(void *priv_data,
				 const u8 *wrapped_key,
				 unsigned int wrapped_key_size, u8 *secret,
				 unsigned int secret_size);

//ICE
#if IS_ENABLED(CONFIG_QTI_CRYPTO_FDE)
/* MSM ICE Crypto Data Unit of target DUN of Transfer Request */
enum ice_crypto_data_unit {
	ICE_CRYPTO_DATA_UNIT_512_B	= 0,
	ICE_CRYPTO_DATA_UNIT_1_KB	= 1,
	ICE_CRYPTO_DATA_UNIT_2_KB	= 2,
	ICE_CRYPTO_DATA_UNIT_4_KB	= 3,
	ICE_CRYPTO_DATA_UNIT_8_KB	= 4,
	ICE_CRYPTO_DATA_UNIT_16_KB	= 5,
	ICE_CRYPTO_DATA_UNIT_32_KB	= 6,
	ICE_CRYPTO_DATA_UNIT_64_KB	= 7,
};
struct request;

enum ice_cryto_algo_mode {
	ICE_CRYPTO_ALGO_MODE_AES_ECB = 0x0,
	ICE_CRYPTO_ALGO_MODE_AES_XTS = 0x3,
};

enum ice_crpto_key_size {
	ICE_CRYPTO_KEY_SIZE_128 = 0x0,
	ICE_CRYPTO_KEY_SIZE_256 = 0x2,
};

struct ice_crypto_setting {
	enum ice_crpto_key_size		key_size;
	enum ice_cryto_algo_mode	algo_mode;
	short				key_index;
};

struct ice_data_setting {
	struct ice_crypto_setting	crypto_data;
	bool				sw_forced_context_switch;
	bool				decr_bypass;
	bool				encr_bypass;
};
typedef void (*ice_error_cb)(void *, u32 error);
int crypto_qti_ice_setup_ice_hw(const char *storage_type, int enable);
void crypto_qti_ice_set_fde_flag(int flag);
int crypto_qti_ice_config_start(struct request *req,
				struct ice_data_setting *setting);
#else //CONFIG_QTI_CRYPTO_FDE
static inline int crypto_qti_ice_setup_ice_hw(const char *storage_type, int enable)
{
	return 0;
}
static inline void crypto_qti_ice_set_fde_flag(int flag) {}
#endif //CONFIG_QTI_CRYPTO_FDE


#else
static inline int crypto_qti_init_crypto(struct device *dev,
					 void __iomem *mmio_base,
					 void __iomem *hwkm_slave_mmio_base,
					 void **priv_data)
{
	return -EOPNOTSUPP;
}
static inline int crypto_qti_enable(void *priv_data)
{
	return -EOPNOTSUPP;
}
static inline void crypto_qti_disable(void *priv_data) {}
static inline int crypto_qti_resume(void *priv_data)
{
	return -EOPNOTSUPP;
}
static inline int crypto_qti_debug(void *priv_data)
{
	return -EOPNOTSUPP;
}
static inline int crypto_qti_keyslot_program(void *priv_data,
					     const struct blk_crypto_key *key,
					     unsigned int slot,
					     u8 data_unit_mask,
					     int capid)
{
	return -EOPNOTSUPP;
}
static inline int crypto_qti_keyslot_evict(void *priv_data, unsigned int slot)
{
	return -EOPNOTSUPP;
}
static inline int crypto_qti_derive_raw_secret(void *priv_data,
					       const u8 *wrapped_key,
					       unsigned int wrapped_key_size,
					       u8 *secret,
					       unsigned int secret_size)
{
	return -EOPNOTSUPP;
}
static inline int crypto_qti_ice_setup_ice_hw(const char *storage_type, int enable)
{
	return 0;
}
static inline void crypto_qti_ice_set_fde_flag(int flag) {}

#endif /* CONFIG_QTI_CRYPTO_COMMON */

#endif /* _CRYPTO_QTI_COMMON_H */
