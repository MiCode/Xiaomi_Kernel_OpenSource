// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include <ufs/ufshcd.h>
#include <ufs/ufshcd-crypto.h>
#include "ufs-qcom.h"
#include <soc/qcom/ice.h>

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

static int ufshcd_qti_crypto_keyslot_program(struct blk_crypto_profile *profile,
					     const struct blk_crypto_key *key,
					     unsigned int slot)
{
	struct ufs_hba *hba =
		container_of(profile, struct ufs_hba, crypto_profile);
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	const union ufs_crypto_cap_entry *ccap_array = hba->crypto_cap_array;
	const struct ufs_crypto_alg_entry *alg =
			&ufs_crypto_algs[key->crypto_cfg.crypto_mode];
	u8 data_unit_mask = key->crypto_cfg.data_unit_size / 512;
	int i;
	int cap_idx = -1;
	union ufs_crypto_cfg_entry cfg = {};
	int err;
	u8 ice_key_size;

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

	cfg.data_unit_size = data_unit_mask;
	cfg.crypto_cap_idx = cap_idx;
	cfg.config_enable = UFS_CRYPTO_CONFIGURATION_ENABLE;

	if (key->crypto_cfg.key_type != BLK_CRYPTO_KEY_TYPE_HW_WRAPPED) {
		if (ccap_array[cap_idx].algorithm_id == UFS_CRYPTO_ALG_AES_XTS) {
			/* In XTS mode, the blk_crypto_key's size is already doubled */
			memcpy(cfg.crypto_key, key->raw, key->size / 2);
			memcpy(cfg.crypto_key + UFS_CRYPTO_KEY_MAX_SIZE / 2,
			       key->raw + key->size / 2, key->size / 2);
		} else {
			memcpy(cfg.crypto_key, key->raw, key->size);
		}
	}

	if (key->crypto_cfg.key_type == BLK_CRYPTO_KEY_TYPE_HW_WRAPPED)
		ice_key_size = QCOM_ICE_CRYPTO_KEY_SIZE_WRAPPED;
	else
		ice_key_size = QCOM_ICE_CRYPTO_KEY_SIZE_256;

	if (host->reset_in_progress) {
		pr_err("UFS host reset in progress, state = 0x%x\n",
		       hba->ufshcd_state);
		return -EINVAL;
	}

	ufshcd_hold(hba);
	err = qcom_ice_program_key_hwkm(host->ice,
					QCOM_ICE_CRYPTO_ALG_AES_XTS,
					ice_key_size, key,
					cfg.data_unit_size, slot);
	if (err)
		pr_err("%s: error programming key, err = %d, slot = %d\n",
		       __func__, err, slot);

	ufshcd_release(hba);
	memzero_explicit(&cfg, sizeof(cfg));
	return err;
}

static int ufshcd_qti_crypto_keyslot_evict(struct blk_crypto_profile *profile,
					   const struct blk_crypto_key *key,
					   unsigned int slot)
{
	struct ufs_hba *hba =
		container_of(profile, struct ufs_hba, crypto_profile);
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	int err = 0;

	if (host->reset_in_progress) {
		pr_err("UFS host reset in progress, state = 0x%x\n",
		       hba->ufshcd_state);
		return -EINVAL;
	}

	ufshcd_hold(hba);
	err = qcom_ice_evict_key(host->ice, slot);
	if (err)
		pr_err("%s: error evicting key, err = %d, slot = %d\n",
		       __func__, err, slot);

	ufshcd_release(hba);
	return err;
}

static int ufshcd_qti_crypto_derive_sw_secret(struct blk_crypto_profile *profile,
					      const u8 wkey[], size_t wkey_size,
					      u8 sw_secret[BLK_CRYPTO_SW_SECRET_SIZE])
{
	struct ufs_hba *hba =
		container_of(profile, struct ufs_hba, crypto_profile);
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	int err;

	err = qcom_ice_derive_sw_secret(host->ice, wkey, wkey_size, sw_secret);
	if (err)
		pr_err("%s: error deriving software secret, err = %d\n",
		       __func__, err);

	return err;
}

static const struct blk_crypto_ll_ops ufshcd_qti_crypto_ops = {
	.keyslot_program	= ufshcd_qti_crypto_keyslot_program,
	.keyslot_evict		= ufshcd_qti_crypto_keyslot_evict,
	.derive_sw_secret	= ufshcd_qti_crypto_derive_sw_secret,
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
	err = devm_blk_crypto_profile_init(hba->dev, &hba->crypto_profile,
					   hba->crypto_capabilities.config_count + 1);
	if (err)
		goto out;

	hba->crypto_profile.ll_ops = ufshcd_qti_crypto_ops;
	/* UFS only supports 8 bytes for any DUN */
	hba->crypto_profile.max_dun_bytes_supported = 8;
	hba->crypto_profile.key_types_supported = BLK_CRYPTO_KEY_TYPE_HW_WRAPPED;
	hba->crypto_profile.dev = hba->dev;

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
		blk_mode_num =
			ufshcd_find_blk_crypto_mode(hba->crypto_cap_array[cap_idx]);
		if (blk_mode_num != BLK_ENCRYPTION_MODE_INVALID)
			hba->crypto_profile.modes_supported[blk_mode_num] |=
				hba->crypto_cap_array[cap_idx].sdus_mask * 512;
	}

	return 0;

out:
	/* Indicate that init failed by clearing UFSHCD_CAP_CRYPTO */
	hba->caps &= ~UFSHCD_CAP_CRYPTO;
	return err;
}
EXPORT_SYMBOL_GPL(ufshcd_qti_hba_init_crypto_capabilities);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("UFS Crypto ops QTI implementation");
