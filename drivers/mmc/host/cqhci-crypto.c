// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2020 Google LLC
 */

#include <linux/blk-crypto.h>
#include <linux/keyslot-manager.h>
#include <linux/mmc/host.h>

#include "cqhci-crypto.h"

#define CQHCI_CRYPTO_CONFIG_INDEX(x)	(((u64)(x) & 0xFF) << 32)
#define CQHCI_CRYPTO_ENABLE_BIT		(((u64)1) << 47)

/* Blk-crypto modes supported by CQHCI MMC crypto */
static const struct cqhci_crypto_alg_entry {
	enum cqhci_crypto_alg cqhci_alg;
	enum cqhci_crypto_key_size cqhci_key_size;
} cqhci_crypto_algs[BLK_ENCRYPTION_MODE_MAX] = {
	[BLK_ENCRYPTION_MODE_AES_256_XTS] = {
		.cqhci_alg = CQHCI_CRYPTO_ALG_AES_XTS,
		.cqhci_key_size = CQHCI_CRYPTO_KEY_SIZE_256,
	},
};

static void cqhci_crypto_program_key(struct cqhci_host *host,
				     const union cqhci_crypto_cfg_entry *cfg,
				     int slot)
{
	u32 slot_offset = host->crypto_cfg_register + slot * sizeof(*cfg);
	int i;

	/* Ensure that CFGE is cleared before programming the key */
	cqhci_writel(host, 0, slot_offset + 16 * sizeof(cfg->reg_val[0]));
	for (i = 0; i < 16; i++) {
		cqhci_writel(host, le32_to_cpu(cfg->reg_val[i]),
			     slot_offset + i * sizeof(cfg->reg_val[0]));
	}
	/* Write dword 17 */
	cqhci_writel(host, le32_to_cpu(cfg->reg_val[17]),
		     slot_offset + 17 * sizeof(cfg->reg_val[0]));
	/* Write dword 16 */
	cqhci_writel(host, le32_to_cpu(cfg->reg_val[16]),
		     slot_offset + 16 * sizeof(cfg->reg_val[0]));
}

static int cqhci_crypto_keyslot_program(struct keyslot_manager *ksm,
					const struct blk_crypto_key *key,
					unsigned int slot)

{
	struct cqhci_host *host = keyslot_manager_private(ksm);
	const union cqhci_crypto_cap_entry *ccap_array = host->crypto_cap_array;
	const struct cqhci_crypto_alg_entry *alg =
				&cqhci_crypto_algs[key->crypto_mode];
	u8 data_unit_mask = key->data_unit_size / 512;
	int i;
	int cap_idx = -1;
	union cqhci_crypto_cfg_entry cfg = { { 0 } };

	BUILD_BUG_ON(CQHCI_CRYPTO_KEY_SIZE_INVALID != 0);
	for (i = 0; i < host->crypto_capabilities.num_crypto_cap; i++) {
		if (ccap_array[i].algorithm_id == alg->cqhci_alg &&
		    ccap_array[i].key_size == alg->cqhci_key_size &&
		    (ccap_array[i].sdus_mask & data_unit_mask)) {
			cap_idx = i;
			break;
		}
	}

	if (WARN_ON(cap_idx < 0))
		return -EOPNOTSUPP;

	cfg.data_unit_size = data_unit_mask;
	cfg.crypto_cap_idx = cap_idx;
	cfg.config_enable = CQHCI_CRYPTO_CONFIGURATION_ENABLE;

	if (ccap_array[cap_idx].algorithm_id == CQHCI_CRYPTO_ALG_AES_XTS) {
		/* In XTS mode, the blk_crypto_key's size is already doubled */
		memcpy(cfg.crypto_key, key->raw, key->size/2);
		memcpy(cfg.crypto_key + CQHCI_CRYPTO_KEY_MAX_SIZE/2,
		       key->raw + key->size/2, key->size/2);
	} else {
		memcpy(cfg.crypto_key, key->raw, key->size);
	}

	cqhci_crypto_program_key(host, &cfg, slot);

	memzero_explicit(&cfg, sizeof(cfg));
	return 0;
}

static void cqhci_crypto_clear_keyslot(struct cqhci_host *host, int slot)
{
	/*
	 * Clear the crypto cfg on the device. Clearing CFGE
	 * might not be sufficient, so just clear the entire cfg.
	 */
	union cqhci_crypto_cfg_entry cfg = { { 0 } };

	cqhci_crypto_program_key(host, &cfg, slot);
}

static int cqhci_crypto_keyslot_evict(struct keyslot_manager *ksm,
				      const struct blk_crypto_key *key,
				      unsigned int slot)
{
	cqhci_crypto_clear_keyslot(keyslot_manager_private(ksm), slot);
	return 0;
}

static const struct keyslot_mgmt_ll_ops cqhci_ksm_ops = {
	.keyslot_program	= cqhci_crypto_keyslot_program,
	.keyslot_evict		= cqhci_crypto_keyslot_evict,
};

bool cqhci_crypto_enable(struct cqhci_host *host)
{
	if (!(host->mmc->caps2 & MMC_CAP2_CRYPTO))
		return false;

	/* Reset might clear all keys, so reprogram all the keys. */
	keyslot_manager_reprogram_all_keys(host->mmc->ksm);
	return true;
}

static enum blk_crypto_mode_num
cqhci_find_blk_crypto_mode(union cqhci_crypto_cap_entry cap)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(cqhci_crypto_algs); i++) {
		BUILD_BUG_ON(CQHCI_CRYPTO_KEY_SIZE_INVALID != 0);
		if (cqhci_crypto_algs[i].cqhci_alg == cap.algorithm_id &&
		    cqhci_crypto_algs[i].cqhci_key_size == cap.key_size) {
			return i;
		}
	}
	return BLK_ENCRYPTION_MODE_INVALID;
}

/**
 * cqhci_host_init_crypto - Read crypto capabilities, init crypto fields in host
 * @host: Per adapter instance
 *
 * Return: 0 if crypto was initialized, or is not supported, else a -errno value
 */
int cqhci_host_init_crypto(struct cqhci_host *host)
{
	int cap_idx = 0;
	int err = 0;
	enum blk_crypto_mode_num blk_mode_num;
	int slot = 0;
	struct device *dev = &host->mmc->class_dev;
	unsigned int crypto_modes_supported[BLK_ENCRYPTION_MODE_MAX] = {0};
	int num_keyslots;
	struct keyslot_manager *ksm;

	/*
	 * Don't use crypto if the vendor specific driver doesn't set the
	 * standard crypto capability bit *or* the hardware doesn't advertise
	 * that crypto is supported.
	 */
	if (!(host->mmc->caps2 & MMC_CAP2_CRYPTO) ||
	    !(cqhci_readl(host, CQHCI_CAP) & CQHCI_CAP_CS))
		return 0;

	host->crypto_capabilities.reg_val =
			cpu_to_le32(cqhci_readl(host, CQHCI_CCAP));

	host->crypto_cfg_register =
		(u32)host->crypto_capabilities.config_array_ptr * 0x100;

	host->crypto_cap_array =
		devm_kcalloc(dev, host->crypto_capabilities.num_crypto_cap,
			     sizeof(host->crypto_cap_array[0]), GFP_KERNEL);
	if (!host->crypto_cap_array) {
		err = -ENOMEM;
		goto out;
	}

	for (cap_idx = 0; cap_idx < host->crypto_capabilities.num_crypto_cap;
	     cap_idx++) {
		host->crypto_cap_array[cap_idx].reg_val =
			cpu_to_le32(cqhci_readl(host, CQHCI_CRYPTOCAP +
						cap_idx * sizeof(__le32)));
		blk_mode_num = cqhci_find_blk_crypto_mode(
					host->crypto_cap_array[cap_idx]);
		if (blk_mode_num == BLK_ENCRYPTION_MODE_INVALID)
			continue;
		crypto_modes_supported[blk_mode_num] |=
				host->crypto_cap_array[cap_idx].sdus_mask * 512;
	}

	/* The actual number of configurations supported is (CFGC+1) */
	num_keyslots = host->crypto_capabilities.config_count + 1;
	ksm = keyslot_manager_create(num_keyslots,
			&cqhci_ksm_ops, crypto_modes_supported, host);

	if (!ksm) {
		err = -ENOMEM;
		goto out_free_caps;
	}
	host->mmc->ksm = ksm;

	for (slot = 0; slot < num_keyslots; slot++)
		cqhci_crypto_clear_keyslot(host, slot);

	/* CQHCI crypto uses 128-bit task descriptor */
	host->caps |= CQHCI_TASK_DESC_SZ_128;

	return 0;
out_free_caps:
	devm_kfree(dev, host->crypto_cap_array);
out:
	/* Indicate that init failed by clearing MMC_CAP2_CRYPTO */
	host->mmc->caps2 &= ~MMC_CAP2_CRYPTO;
	return err;
}

int cqhci_prep_crypto_desc(struct mmc_request *mrq, __le64 *task_desc)
{
	u64 crypto_desc = 0;

	if (mmc_request_crypto_enabled(mrq)) {
		/* eMMC v5.2 only supports 32 bits for DUN */
		if (WARN_ON_ONCE(upper_32_bits(mrq->data_unit_num) != 0))
			return -EINVAL;

		crypto_desc = lower_32_bits(mrq->data_unit_num) |
			      CQHCI_CRYPTO_CONFIG_INDEX(mrq->crypto_key_slot) |
			      CQHCI_CRYPTO_ENABLE_BIT;
		task_desc[1] = cpu_to_le64(crypto_desc);
	} else
		task_desc[1] = 0;

	/*
	 *  Assign upper 64bits data of 128 bits task descriptor
	 *  with the crypto context
	 */
	//task_desc[1] = cpu_to_le64(crypto_desc);
	return 0;
}

void cqhci_crypto_recovery_finish(struct cqhci_host *host)
{
	/* Reset/Recovery might clear all keys, so reprogram all the keys. */
	keyslot_manager_reprogram_all_keys(host->mmc->ksm);
}
