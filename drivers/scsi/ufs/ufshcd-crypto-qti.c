// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <crypto/algapi.h>
#include <linux/platform_device.h>
#include <linux/crypto-qti-common.h>
#if IS_ENABLED(CONFIG_CRYPTO_DEV_QCOM_ICE)
#include <crypto/ice.h>
#include <linux/blkdev.h>
#endif
#include "ufshcd-crypto-qti.h"

#define MINIMUM_DUN_SIZE 512
#define MAXIMUM_DUN_SIZE 65536

#define NUM_KEYSLOTS(hba) (hba->crypto_capabilities.config_count + 1)

static struct ufs_hba_crypto_variant_ops ufshcd_crypto_qti_variant_ops = {
	.hba_init_crypto = ufshcd_crypto_qti_init_crypto,
	.enable = ufshcd_crypto_qti_enable,
	.disable = ufshcd_crypto_qti_disable,
	.resume = ufshcd_crypto_qti_resume,
	.debug = ufshcd_crypto_qti_debug,
	.prepare_lrbp_crypto = ufshcd_crypto_qti_prep_lrbp_crypto,
};

static uint8_t get_data_unit_size_mask(unsigned int data_unit_size)
{
	if (data_unit_size < MINIMUM_DUN_SIZE ||
		data_unit_size > MAXIMUM_DUN_SIZE ||
	    !is_power_of_2(data_unit_size))
		return 0;

	return data_unit_size / MINIMUM_DUN_SIZE;
}

static bool ice_cap_idx_valid(struct ufs_hba *hba,
			      unsigned int cap_idx)
{
	return cap_idx < hba->crypto_capabilities.num_crypto_cap;
}

void ufshcd_crypto_qti_enable(struct ufs_hba *hba)
{
	int err = 0;

	if (!ufshcd_hba_is_crypto_supported(hba))
		return;

	err = crypto_qti_enable(hba->crypto_vops->priv);
	if (err) {
		pr_err("%s: Error enabling crypto, err %d\n",
				__func__, err);
		ufshcd_crypto_qti_disable(hba);
	}

	ufshcd_crypto_enable_spec(hba);
}

void ufshcd_crypto_qti_disable(struct ufs_hba *hba)
{
	ufshcd_crypto_disable_spec(hba);
	crypto_qti_disable(hba->crypto_vops->priv);
}


static int ufshcd_crypto_qti_keyslot_program(struct keyslot_manager *ksm,
					     const struct blk_crypto_key *key,
					     unsigned int slot)
{
	struct ufs_hba *hba = keyslot_manager_private(ksm);
	int err = 0;
	u8 data_unit_mask;
	int crypto_alg_id;

	crypto_alg_id = ufshcd_crypto_cap_find(hba, key->crypto_mode,
					       key->data_unit_size);

	if (!ufshcd_is_crypto_enabled(hba) ||
	    !ufshcd_keyslot_valid(hba, slot) ||
	    !ice_cap_idx_valid(hba, crypto_alg_id))
		return -EINVAL;

	data_unit_mask = get_data_unit_size_mask(key->data_unit_size);

	if (!(data_unit_mask &
	      hba->crypto_cap_array[crypto_alg_id].sdus_mask))
		return -EINVAL;

	if (!hba->pm_op_in_progress)
		pm_runtime_get_sync(hba->dev);
	err = ufshcd_hold(hba, false);
	if (err) {
		pr_err("%s: failed to enable clocks, err %d\n", __func__, err);
		goto out;
	}

	err = crypto_qti_keyslot_program(hba->crypto_vops->priv, key, slot,
					data_unit_mask, crypto_alg_id);
	if (err)
		pr_err("%s: failed with error %d\n", __func__, err);

	ufshcd_release(hba, false);

out:
	if (!hba->pm_op_in_progress)
		pm_runtime_put_sync(hba->dev);
	return err;
}

static int ufshcd_crypto_qti_keyslot_evict(struct keyslot_manager *ksm,
					   const struct blk_crypto_key *key,
					   unsigned int slot)
{
	int err = 0;
	struct ufs_hba *hba = keyslot_manager_private(ksm);

	if (!ufshcd_is_crypto_enabled(hba) ||
	    !ufshcd_keyslot_valid(hba, slot))
		return -EINVAL;

	pm_runtime_get_sync(hba->dev);
	err = ufshcd_hold(hba, false);
	if (err) {
		pr_err("%s: failed to enable clocks, err %d\n", __func__, err);
		return err;
	}

	err = crypto_qti_keyslot_evict(hba->crypto_vops->priv, slot);
	if (err) {
		pr_err("%s: failed with error %d\n",
			__func__, err);
		ufshcd_release(hba, false);
		pm_runtime_put_sync(hba->dev);
		return err;
	}

	ufshcd_release(hba, false);
	pm_runtime_put_sync(hba->dev);

	return err;
}

static int ufshcd_crypto_qti_derive_raw_secret(struct keyslot_manager *ksm,
					       const u8 *wrapped_key,
					       unsigned int wrapped_key_size,
					       u8 *secret,
					       unsigned int secret_size)
{
	return crypto_qti_derive_raw_secret(wrapped_key, wrapped_key_size,
			secret, secret_size);
}

static const struct keyslot_mgmt_ll_ops ufshcd_crypto_qti_ksm_ops = {
	.keyslot_program	= ufshcd_crypto_qti_keyslot_program,
	.keyslot_evict		= ufshcd_crypto_qti_keyslot_evict,
	.derive_raw_secret	= ufshcd_crypto_qti_derive_raw_secret,
};

static enum blk_crypto_mode_num ufshcd_blk_crypto_qti_mode_num_for_alg_dusize(
					enum ufs_crypto_alg ufs_crypto_alg,
					enum ufs_crypto_key_size key_size)
{
	/*
	 * This is currently the only mode that UFS and blk-crypto both support.
	 */
	if (ufs_crypto_alg == UFS_CRYPTO_ALG_AES_XTS &&
		key_size == UFS_CRYPTO_KEY_SIZE_256)
		return BLK_ENCRYPTION_MODE_AES_256_XTS;

	return BLK_ENCRYPTION_MODE_INVALID;
}

static int ufshcd_hba_init_crypto_qti_spec(struct ufs_hba *hba,
				    const struct keyslot_mgmt_ll_ops *ksm_ops)
{
	int cap_idx = 0;
	int err = 0;
	unsigned int crypto_modes_supported[BLK_ENCRYPTION_MODE_MAX];
	enum blk_crypto_mode_num blk_mode_num;

	/* Default to disabling crypto */
	hba->caps &= ~UFSHCD_CAP_CRYPTO;

	if (!(hba->capabilities & MASK_CRYPTO_SUPPORT)) {
		err = -ENODEV;
		goto out;
	}

	/*
	 * Crypto Capabilities should never be 0, because the
	 * config_array_ptr > 04h. So we use a 0 value to indicate that
	 * crypto init failed, and can't be enabled.
	 */
	hba->crypto_capabilities.reg_val =
			  cpu_to_le32(ufshcd_readl(hba, REG_UFS_CCAP));
	hba->crypto_cfg_register =
		 (u32)hba->crypto_capabilities.config_array_ptr * 0x100;
	hba->crypto_cap_array =
		 devm_kcalloc(hba->dev,
				hba->crypto_capabilities.num_crypto_cap,
				sizeof(hba->crypto_cap_array[0]),
				GFP_KERNEL);
	if (!hba->crypto_cap_array) {
		err = -ENOMEM;
		goto out;
	}

	memset(crypto_modes_supported, 0, sizeof(crypto_modes_supported));
	/*
	 * Store all the capabilities now so that we don't need to repeatedly
	 * access the device each time we want to know its capabilities
	 */
	for (cap_idx = 0; cap_idx < hba->crypto_capabilities.num_crypto_cap;
	     cap_idx++) {
		hba->crypto_cap_array[cap_idx].reg_val =
				cpu_to_le32(ufshcd_readl(hba,
						REG_UFS_CRYPTOCAP +
						cap_idx * sizeof(__le32)));
		blk_mode_num = ufshcd_blk_crypto_qti_mode_num_for_alg_dusize(
				hba->crypto_cap_array[cap_idx].algorithm_id,
				hba->crypto_cap_array[cap_idx].key_size);
		if (blk_mode_num == BLK_ENCRYPTION_MODE_INVALID)
			continue;
		crypto_modes_supported[blk_mode_num] |=
			hba->crypto_cap_array[cap_idx].sdus_mask * 512;
	}

	hba->ksm = keyslot_manager_create(hba->dev, ufshcd_num_keyslots(hba),
					  ksm_ops,
					  BLK_CRYPTO_FEATURE_STANDARD_KEYS |
					  BLK_CRYPTO_FEATURE_WRAPPED_KEYS,
					  crypto_modes_supported, hba);

	if (!hba->ksm) {
		err = -ENOMEM;
		goto out;
	}
	pr_debug("%s: keyslot manager created\n", __func__);

	return 0;

out:
	/* Indicate that init failed by setting crypto_capabilities to 0 */
	hba->crypto_capabilities.reg_val = 0;
	return err;
}

int ufshcd_crypto_qti_init_crypto(struct ufs_hba *hba,
				  const struct keyslot_mgmt_ll_ops *ksm_ops)
{
	int err = 0;
	struct platform_device *pdev = to_platform_device(hba->dev);
	void __iomem *mmio_base;
	struct resource *mem_res;

	mem_res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
								"ufs_ice");
	mmio_base = devm_ioremap_resource(hba->dev, mem_res);
	if (IS_ERR(mmio_base)) {
		pr_err("%s: Unable to get ufs_crypto mmio base\n", __func__);
		hba->caps &= ~UFSHCD_CAP_CRYPTO;
		hba->quirks |= UFSHCD_QUIRK_BROKEN_CRYPTO;
		return err;
	}

	err = ufshcd_hba_init_crypto_qti_spec(hba, &ufshcd_crypto_qti_ksm_ops);
	if (err) {
		pr_err("%s: Error initiating crypto capabilities, err %d\n",
					__func__, err);
		return err;
	}

	err = crypto_qti_init_crypto(hba->dev,
			mmio_base, (void **)&hba->crypto_vops->priv);
	if (err) {
		pr_err("%s: Error initiating crypto, err %d\n",
					__func__, err);
	}
	return err;
}

int ufshcd_crypto_qti_prep_lrbp_crypto(struct ufs_hba *hba,
				       struct scsi_cmnd *cmd,
				       struct ufshcd_lrb *lrbp)
{
	struct bio_crypt_ctx *bc;
	int ret = 0;
#if IS_ENABLED(CONFIG_CRYPTO_DEV_QCOM_ICE)
	struct ice_data_setting setting;
	bool bypass = true;
	short key_index = 0;
#endif
	struct request *req;

	lrbp->crypto_enable = false;
	req = cmd->request;
	if (!req || !req->bio)
		return ret;

	if (!bio_crypt_should_process(req)) {
#if IS_ENABLED(CONFIG_CRYPTO_DEV_QCOM_ICE)
		ret = qcom_ice_config_start(req, &setting);
		if (!ret) {
			key_index = setting.crypto_data.key_index;
			bypass = (rq_data_dir(req) == WRITE) ?
				 setting.encr_bypass : setting.decr_bypass;
			lrbp->crypto_enable = !bypass;
			lrbp->crypto_key_slot = key_index;
			lrbp->data_unit_num = req->bio->bi_iter.bi_sector >>
					      ICE_CRYPTO_DATA_UNIT_4_KB;
		} else {
			pr_err("%s crypto config failed err = %d\n", __func__,
			       ret);
		}
#endif
		return ret;
	}
	bc = req->bio->bi_crypt_context;

	if (WARN_ON(!ufshcd_is_crypto_enabled(hba))) {
		/*
		 * Upper layer asked us to do inline encryption
		 * but that isn't enabled, so we fail this request.
		 */
		return -EINVAL;
	}
	if (!ufshcd_keyslot_valid(hba, bc->bc_keyslot))
		return -EINVAL;

	lrbp->crypto_enable = true;
	lrbp->crypto_key_slot = bc->bc_keyslot;
	if (bc->is_ext4) {
		lrbp->data_unit_num = (u64)cmd->request->bio->bi_iter.bi_sector;
		lrbp->data_unit_num >>= 3;
	} else {
		lrbp->data_unit_num = bc->bc_dun[0];
	}
	return 0;
}

int ufshcd_crypto_qti_debug(struct ufs_hba *hba)
{
	return crypto_qti_debug(hba->crypto_vops->priv);
}

void ufshcd_crypto_qti_set_vops(struct ufs_hba *hba)
{
	return ufshcd_crypto_set_vops(hba, &ufshcd_crypto_qti_variant_ops);
}

int ufshcd_crypto_qti_resume(struct ufs_hba *hba,
			     enum ufs_pm_op pm_op)
{
	return crypto_qti_resume(hba->crypto_vops->priv);
}
