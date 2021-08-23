// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020 - 2021, Linux Foundation. All rights reserved.
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
#include "sdhci.h"
#include "sdhci-pltfm.h"
#include "cqhci-crypto-qti.h"
#include <linux/crypto-qti-common.h>

#define RAW_SECRET_SIZE 32
#define MINIMUM_DUN_SIZE 512
#define MAXIMUM_DUN_SIZE 65536

static struct cqhci_host_crypto_variant_ops __maybe_unused cqhci_crypto_qti_variant_ops = {
	.host_init_crypto = cqhci_crypto_qti_init_crypto,
	.enable = cqhci_crypto_qti_enable,
	.disable = cqhci_crypto_qti_disable,
	.resume = cqhci_crypto_qti_resume,
	.debug = cqhci_crypto_qti_debug,
	.recovery_finish = cqhci_crypto_qti_recovery_finish,
};

static bool ice_cap_idx_valid(struct cqhci_host *host,
					unsigned int cap_idx)
{
	return cap_idx < host->crypto_capabilities.num_crypto_cap;
}

static uint8_t get_data_unit_size_mask(unsigned int data_unit_size)
{
	if (data_unit_size < MINIMUM_DUN_SIZE ||
		data_unit_size > MAXIMUM_DUN_SIZE ||
	    !is_power_of_2(data_unit_size))
		return 0;

	return data_unit_size / MINIMUM_DUN_SIZE;
}


void cqhci_crypto_qti_enable(struct cqhci_host *host)
{
	int err = 0;

	if (!cqhci_host_is_crypto_supported(host))
		return;

	host->caps |= CQHCI_CAP_CRYPTO_SUPPORT;

	err = crypto_qti_enable(host->crypto_vops->priv);
	if (err) {
		pr_err("%s: Error enabling crypto, err %d\n",
				__func__, err);
		cqhci_crypto_qti_disable(host);
	}
}

void cqhci_crypto_qti_disable(struct cqhci_host *host)
{
	cqhci_crypto_disable_spec(host);
	crypto_qti_disable(host->crypto_vops->priv);
}

static int cqhci_crypto_qti_keyslot_program(struct keyslot_manager *ksm,
					    const struct blk_crypto_key *key,
					    unsigned int slot)
{
	struct cqhci_host *host = keyslot_manager_private(ksm);
	int err = 0;
	u8 data_unit_mask;
	int crypto_alg_id;

	crypto_alg_id = cqhci_crypto_cap_find(host, key->crypto_mode,
					       key->data_unit_size);

	if (!cqhci_is_crypto_enabled(host) ||
	    !cqhci_keyslot_valid(host, slot) ||
	    !ice_cap_idx_valid(host, crypto_alg_id)) {
		return -EINVAL;
	}

	data_unit_mask = get_data_unit_size_mask(key->data_unit_size);

	if (!(data_unit_mask &
	      host->crypto_cap_array[crypto_alg_id].sdus_mask)) {
		return -EINVAL;
	}

	err = crypto_qti_keyslot_program(host->crypto_vops->priv, key,
					 slot, data_unit_mask, crypto_alg_id);
	if (err)
		pr_err("%s: failed with error %d\n", __func__, err);

	return err;
}

static int cqhci_crypto_qti_keyslot_evict(struct keyslot_manager *ksm,
					  const struct blk_crypto_key *key,
					  unsigned int slot)
{
	int err = 0;
	struct cqhci_host *host = keyslot_manager_private(ksm);

	if (!cqhci_is_crypto_enabled(host) ||
	    !cqhci_keyslot_valid(host, slot))
		return -EINVAL;

	err = crypto_qti_keyslot_evict(host->crypto_vops->priv, slot);
	if (err)
		pr_err("%s: failed with error %d\n", __func__, err);

	return err;
}

static int cqhci_crypto_qti_derive_raw_secret(struct keyslot_manager *ksm,
		const u8 *wrapped_key, unsigned int wrapped_key_size,
		u8 *secret, unsigned int secret_size)
{
	int err = 0;
	struct cqhci_host *host = keyslot_manager_private(ksm);

	err = crypto_qti_derive_raw_secret(host->crypto_vops->priv, wrapped_key, wrapped_key_size,
					  secret, secret_size);
	return err;
}

static const struct keyslot_mgmt_ll_ops cqhci_crypto_qti_ksm_ops = {
	.keyslot_program	= cqhci_crypto_qti_keyslot_program,
	.keyslot_evict		= cqhci_crypto_qti_keyslot_evict,
	.derive_raw_secret	= cqhci_crypto_qti_derive_raw_secret
};

enum blk_crypto_mode_num cqhci_blk_crypto_qti_mode_num_for_alg_dusize(
	enum cqhci_crypto_alg cqhci_crypto_alg,
	enum cqhci_crypto_key_size key_size)
{
	/*
	 * Currently the only mode that eMMC and blk-crypto both support.
	 */
	if (cqhci_crypto_alg == CQHCI_CRYPTO_ALG_AES_XTS &&
		key_size == CQHCI_CRYPTO_KEY_SIZE_256)
		return BLK_ENCRYPTION_MODE_AES_256_XTS;

	return BLK_ENCRYPTION_MODE_INVALID;
}

int cqhci_host_init_crypto_qti_spec(struct cqhci_host *host,
				    const struct keyslot_mgmt_ll_ops *ksm_ops)
{
	int cap_idx = 0;
	int err = 0;
	unsigned int crypto_modes_supported[BLK_ENCRYPTION_MODE_MAX];
	enum blk_crypto_mode_num blk_mode_num;

	/* Default to disabling crypto */
	host->caps &= ~CQHCI_CAP_CRYPTO_SUPPORT;

	if (!(cqhci_readl(host, CQHCI_CAP) & CQHCI_CAP_CS)) {
		pr_debug("%s no crypto capability\n", __func__);
		err = -ENODEV;
		goto out;
	}

	/*
	 * Crypto Capabilities should never be 0, because the
	 * config_array_ptr > 04h. So we use a 0 value to indicate that
	 * crypto init failed, and can't be enabled.
	 */
	host->crypto_capabilities.reg_val = cqhci_readl(host, CQHCI_CCAP);
	host->crypto_cfg_register =
		(u32)host->crypto_capabilities.config_array_ptr * 0x100;
	host->crypto_cap_array =
		devm_kcalloc(mmc_dev(host->mmc),
				host->crypto_capabilities.num_crypto_cap,
				sizeof(host->crypto_cap_array[0]), GFP_KERNEL);
	if (!host->crypto_cap_array) {
		err = -ENOMEM;
		pr_err("%s failed to allocate memory\n", __func__);
		goto out;
	}

	memset(crypto_modes_supported, 0, sizeof(crypto_modes_supported));

	/*
	 * Store all the capabilities now so that we don't need to repeatedly
	 * access the device each time we want to know its capabilities
	 */
	for (cap_idx = 0; cap_idx < host->crypto_capabilities.num_crypto_cap;
	     cap_idx++) {
		host->crypto_cap_array[cap_idx].reg_val =
			cpu_to_le32(cqhci_readl(host,
						 CQHCI_CRYPTOCAP +
						 cap_idx * sizeof(__le32)));
		blk_mode_num = cqhci_blk_crypto_qti_mode_num_for_alg_dusize(
				host->crypto_cap_array[cap_idx].algorithm_id,
				host->crypto_cap_array[cap_idx].key_size);
		if (blk_mode_num == BLK_ENCRYPTION_MODE_INVALID)
			continue;
		crypto_modes_supported[blk_mode_num] |=
				host->crypto_cap_array[cap_idx].sdus_mask * 512;
	}

	host->mmc->ksm = keyslot_manager_create(host->mmc->parent,
				       cqhci_num_keyslots(host), ksm_ops,
				       BLK_CRYPTO_FEATURE_WRAPPED_KEYS,
				       crypto_modes_supported,
				       host);

	if (!host->mmc->ksm) {
		err = -ENOMEM;
		goto out;
	}

	host->mmc->caps2 |= MMC_CAP2_CRYPTO;
	keyslot_manager_set_max_dun_bytes(host->mmc->ksm, sizeof(u32));

	/*
	 * In case host controller supports cryptographic operations
	 * then, it uses 128bit task descriptor. Upper 64 bits of task
	 * descriptor would be used to pass crypto specific informaton.
	 */
	host->caps |= CQHCI_TASK_DESC_SZ_128;

	return 0;

out:
	/* Indicate that init failed by setting crypto_capabilities to 0 */
	host->crypto_capabilities.reg_val = 0;
	return err;
}

int cqhci_crypto_qti_init_crypto(struct cqhci_host *host,
				const struct keyslot_mgmt_ll_ops *ksm_ops)
{
	int err = 0;
	struct resource *cqhci_ice_memres = NULL;
	struct resource *hwkm_ice_memres = NULL;
	void __iomem *hwkm_ice_mmio = NULL;

	cqhci_ice_memres = platform_get_resource_byname(host->pdev,
							IORESOURCE_MEM,
							"cqhci_ice");
	if (!cqhci_ice_memres) {
		pr_debug("%s ICE not supported\n", __func__);
		host->icemmio = NULL;
		host->caps &= ~CQHCI_CAP_CRYPTO_SUPPORT;
		return err;
	}

	host->icemmio = devm_ioremap(&host->pdev->dev,
				     cqhci_ice_memres->start,
				     resource_size(cqhci_ice_memres));
	if (!host->icemmio) {
		pr_err("%s failed to remap ice regs\n", __func__);
		return PTR_ERR(host->icemmio);
	}

	hwkm_ice_memres = platform_get_resource_byname(host->pdev,
						       IORESOURCE_MEM,
						       "cqhci_ice_hwkm");

	if (!hwkm_ice_memres) {
		pr_err("%s: Either no entry in dtsi or no memory available for IORESOURCE\n",
		       __func__);
	} else {
		hwkm_ice_mmio = devm_ioremap_resource(&host->pdev->dev,
						      hwkm_ice_memres);
		if (IS_ERR(hwkm_ice_mmio)) {
			err = PTR_ERR(hwkm_ice_mmio);
			pr_err("%s: Error = %d mapping HWKM memory\n",
				__func__, err);
			return err;
		}
	}

	err = cqhci_host_init_crypto_qti_spec(host, &cqhci_crypto_qti_ksm_ops);
	if (err) {
		pr_err("%s: Error initiating crypto capabilities, err %d\n",
					__func__, err);
		return err;
	}

	err = crypto_qti_init_crypto(&host->pdev->dev, host->icemmio,
				     hwkm_ice_mmio,
				     (void **)&host->crypto_vops->priv);
	if (err) {
		pr_err("%s: Error initiating crypto, err %d\n",
					__func__, err);
	}
	return err;
}

int cqhci_crypto_qti_debug(struct cqhci_host *host)
{
	return crypto_qti_debug(host->crypto_vops->priv);
}

void cqhci_crypto_qti_set_vops(struct cqhci_host *host)
{
	return cqhci_crypto_set_vops(host, &cqhci_crypto_qti_variant_ops);
}
EXPORT_SYMBOL(cqhci_crypto_qti_set_vops);

int cqhci_crypto_qti_resume(struct cqhci_host *host)
{
	return crypto_qti_resume(host->crypto_vops->priv);
}

int cqhci_crypto_qti_recovery_finish(struct cqhci_host *host)
{
	keyslot_manager_reprogram_all_keys(host->mmc->ksm);
	return 0;
}

MODULE_DESCRIPTION("Vendor specific CQHCI Crypto Engine Support");
MODULE_LICENSE("GPL v2");
