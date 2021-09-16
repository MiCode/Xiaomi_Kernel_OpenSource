// SPDX-License-Identifier: GPL-2.0-only
/*
 * UFS Crypto ops QTI implementation.
 *
 * Copyright (c) 2020-2021, Linux Foundation. All rights reserved.
 */

#include <crypto/algapi.h>
#include <linux/platform_device.h>
#include <linux/crypto-qti-common.h>

#include "ufshcd-crypto-qti.h"
#include "ufs-qcom.h"

#define MINIMUM_DUN_SIZE 512
#define MAXIMUM_DUN_SIZE 65536

/* Blk-crypto modes supported by UFS crypto */
static const struct ufs_crypto_alg_entry {
	enum ufs_crypto_alg ufs_alg;
	enum ufs_crypto_key_size ufs_key_size;
} ufs_crypto_algs[BLK_ENCRYPTION_MODE_MAX] = {
	[BLK_ENCRYPTION_MODE_AES_256_XTS] = {
		.ufs_alg = UFS_CRYPTO_ALG_AES_XTS,
		.ufs_key_size = UFS_CRYPTO_KEY_SIZE_256,
	},
};

static void get_mmio_data(struct ice_mmio_data *data,
						struct ufs_qcom_host *host)
{
	data->ice_base_mmio = host->ice_mmio;
#if IS_ENABLED(CONFIG_QTI_HW_KEY_MANAGER)
	data->ice_hwkm_mmio = host->ice_hwkm_mmio;
#endif
}

static int ufshcd_crypto_qti_keyslot_program(struct blk_keyslot_manager *ksm,
					     const struct blk_crypto_key *key,
					     unsigned int slot)
{
	struct ufs_hba *hba = container_of(ksm, struct ufs_hba, ksm);
	int err = 0;
	u8 data_unit_mask = -1;
	int cap_idx = -1;
	const union ufs_crypto_cap_entry *ccap_array = hba->crypto_cap_array;
	const struct ufs_crypto_alg_entry *alg;
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	int i = 0;
	struct ice_mmio_data mmio_data;

	if (!key) {
		pr_err("Invalid/no key present\n");
		return -EINVAL;
	}

	data_unit_mask = key->crypto_cfg.data_unit_size / MINIMUM_DUN_SIZE;
	alg = &ufs_crypto_algs[key->crypto_cfg.crypto_mode];
	BUILD_BUG_ON(UFS_CRYPTO_KEY_SIZE_INVALID != 0);
	for (i = 0; i < hba->crypto_capabilities.num_crypto_cap; i++) {
		if (ccap_array[i].algorithm_id == alg->ufs_alg &&
		    ccap_array[i].key_size == alg->ufs_key_size &&
		    (ccap_array[i].sdus_mask & data_unit_mask)) {
			cap_idx = i;
			break;
		}
	}

	if (WARN_ON(cap_idx < 0))
		return -EOPNOTSUPP;

	if (host->reset_in_progress) {
		pr_err("UFS host reset in progress, state = 0x%x\n",
				hba->ufshcd_state);
		return -EINVAL;
	}

	err = ufshcd_hold(hba, false);
	if (err) {
		pr_err("%s: failed to enable clocks, err %d\n", __func__, err);
		goto out;
	}

	get_mmio_data(&mmio_data, host);
	err = crypto_qti_keyslot_program(&mmio_data, key, slot,
					data_unit_mask, cap_idx);
	if (err)
		pr_err("%s: failed with error %d\n", __func__, err);

	ufshcd_release(hba);
out:

	return err;
}

static int ufshcd_crypto_qti_keyslot_evict(struct blk_keyslot_manager *ksm,
					   const struct blk_crypto_key *key,
					   unsigned int slot)
{
	int err = 0;
	struct ufs_hba *hba = container_of(ksm, struct ufs_hba, ksm);
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	struct ice_mmio_data mmio_data;

	if (host->reset_in_progress) {
		pr_err("UFS host reset in progress, state = 0x%x\n",
				hba->ufshcd_state);
		return -EINVAL;
	}

	err = ufshcd_hold(hba, false);
	if (err) {
		pr_err("%s: failed to enable clocks, err %d\n", __func__, err);
		return err;
	}

	get_mmio_data(&mmio_data, host);
	err = crypto_qti_keyslot_evict(&mmio_data, slot);
	if (err) {
		pr_err("%s: failed with error %d\n", __func__, err);
	}

	ufshcd_release(hba);
	return err;
}

static int ufshcd_crypto_qti_derive_raw_secret(struct blk_keyslot_manager *ksm,
					       const u8 *wrapped_key,
					       unsigned int wrapped_key_size,
					       u8 *secret,
					       unsigned int secret_size)
{
	int err = 0;
	struct ufs_hba *hba = container_of(ksm, struct ufs_hba, ksm);
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);

	if (host->reset_in_progress) {
		pr_err("UFS host reset in progress, state = 0x%x\n",
				hba->ufshcd_state);
		return -EINVAL;
	}

	err = ufshcd_hold(hba, false);
	if (err) {
		pr_err("%s: failed to enable clocks, err %d\n", __func__, err);
		return err;
	}

	err =  crypto_qti_derive_raw_secret(wrapped_key, wrapped_key_size,
				secret, secret_size);
	if (err)
		pr_err("%s: failed with error %d\n", __func__, err);

	ufshcd_release(hba);
	return err;
}

static const struct blk_ksm_ll_ops ufshcd_qti_ksm_ops = {
	.keyslot_program	= ufshcd_crypto_qti_keyslot_program,
	.keyslot_evict		= ufshcd_crypto_qti_keyslot_evict,
	.derive_raw_secret	= ufshcd_crypto_qti_derive_raw_secret,
};

static enum blk_crypto_mode_num
ufshcd_find_blk_crypto_mode(union ufs_crypto_cap_entry cap)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ufs_crypto_algs); i++) {
		BUILD_BUG_ON(UFS_CRYPTO_KEY_SIZE_INVALID != 0);
		if (ufs_crypto_algs[i].ufs_alg == cap.algorithm_id &&
		    ufs_crypto_algs[i].ufs_key_size == cap.key_size) {
			return i;
		}
	}
	return BLK_ENCRYPTION_MODE_INVALID;
}

/**
 * ufshcd_hba_init_crypto_capabilities - Read crypto capabilities, init crypto
 *					 fields in hba
 * @hba: Per adapter instance
 *
 * Return: 0 if crypto was initialized or is not supported, else a -errno value.
 */
int ufshcd_qti_hba_init_crypto_capabilities(struct ufs_hba *hba)
{
	int cap_idx;
	int err = 0;
	enum blk_crypto_mode_num blk_mode_num;

	/*
	 * Don't use crypto if either the hardware doesn't advertise the
	 * standard crypto capability bit *or* if the vendor specific driver
	 * hasn't advertised that crypto is supported.
	 */

	if (!(ufshcd_readl(hba, REG_CONTROLLER_CAPABILITIES) &
	      MASK_CRYPTO_SUPPORT))
		goto out;
	if (!(hba->caps & UFSHCD_CAP_CRYPTO))
		goto out;

	hba->crypto_capabilities.reg_val =
			cpu_to_le32(ufshcd_readl(hba, REG_UFS_CCAP));
	hba->crypto_cfg_register =
		(u32)hba->crypto_capabilities.config_array_ptr * 0x100;
	hba->crypto_cap_array =
		devm_kcalloc(hba->dev, hba->crypto_capabilities.num_crypto_cap,
			     sizeof(hba->crypto_cap_array[0]), GFP_KERNEL);
	if (!hba->crypto_cap_array) {
		err = -ENOMEM;
		goto out;
	}

	/* The actual number of configurations supported is (CFGC+1) */
	err = devm_blk_ksm_init(hba->dev, &hba->ksm,
			hba->crypto_capabilities.config_count + 1);
	if (err)
		goto out;

	hba->ksm.ksm_ll_ops = ufshcd_qti_ksm_ops;
	/* UFS only supports 8 bytes for any DUN */
	hba->ksm.max_dun_bytes_supported = 8;
	hba->ksm.features = BLK_CRYPTO_FEATURE_WRAPPED_KEYS;
	hba->ksm.dev = hba->dev;

	/*
	 * Cache all the UFS crypto capabilities and advertise the supported
	 * crypto modes and data unit sizes to the block layer.
	 */
	for (cap_idx = 0; cap_idx < hba->crypto_capabilities.num_crypto_cap;
	     cap_idx++) {
		hba->crypto_cap_array[cap_idx].reg_val =
			cpu_to_le32(ufshcd_readl(hba,
						 REG_UFS_CRYPTOCAP +
						 cap_idx * sizeof(__le32)));
		blk_mode_num = ufshcd_find_blk_crypto_mode(
						hba->crypto_cap_array[cap_idx]);
		if (blk_mode_num != BLK_ENCRYPTION_MODE_INVALID)
			hba->ksm.crypto_modes_supported[blk_mode_num] |=
				hba->crypto_cap_array[cap_idx].sdus_mask * 512;
	}

	return 0;

out:
	/* Indicate that init failed by clearing UFSHCD_CAP_CRYPTO */
	hba->caps &= ~UFSHCD_CAP_CRYPTO;
	return err;
}
EXPORT_SYMBOL(ufshcd_qti_hba_init_crypto_capabilities);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("UFS Crypto ops QTI implementation");
