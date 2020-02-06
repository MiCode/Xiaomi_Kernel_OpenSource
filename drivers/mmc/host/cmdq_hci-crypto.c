/*
 * Copyright 2020 Google LLC
 *
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
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
 * drivers/mmc/host/cmdq-crypto.c - Qualcomm Technologies, Inc.
 *
 * Original source is taken from:
 * https://android.googlesource.com/kernel/common/+/4bac1109a10c55d49c0aa4f7ebdc4bc53cc368e8
 * The driver caters to crypto engine support for UFS controllers.
 * The crypto engine programming sequence, HW functionality and register
 * offset is almost same in UFS and eMMC controllers.
 */

#include <crypto/algapi.h>
#include "cmdq_hci-crypto.h"
#include "../core/queue.h"

static bool cmdq_cap_idx_valid(struct cmdq_host *host, unsigned int cap_idx)
{
	return cap_idx < host->crypto_capabilities.num_crypto_cap;
}

static u8 get_data_unit_size_mask(unsigned int data_unit_size)
{
	if (data_unit_size < 512 || data_unit_size > 65536 ||
	    !is_power_of_2(data_unit_size))
		return 0;

	return data_unit_size / 512;
}

static size_t get_keysize_bytes(enum cmdq_crypto_key_size size)
{
	switch (size) {
	case CMDQ_CRYPTO_KEY_SIZE_128:
		return 16;
	case CMDQ_CRYPTO_KEY_SIZE_192:
		return 24;
	case CMDQ_CRYPTO_KEY_SIZE_256:
		return 32;
	case CMDQ_CRYPTO_KEY_SIZE_512:
		return 64;
	default:
		return 0;
	}
}

int cmdq_crypto_cap_find(void *host_p, enum blk_crypto_mode_num crypto_mode,
			  unsigned int data_unit_size)
{
	struct cmdq_host *host = host_p;
	enum cmdq_crypto_alg cmdq_alg;
	u8 data_unit_mask;
	int cap_idx;
	enum cmdq_crypto_key_size cmdq_key_size;
	union cmdq_crypto_cap_entry *ccap_array = host->crypto_cap_array;

	if (!cmdq_host_is_crypto_supported(host))
		return -EINVAL;

	switch (crypto_mode) {
	case BLK_ENCRYPTION_MODE_AES_256_XTS:
		cmdq_alg = CMDQ_CRYPTO_ALG_AES_XTS;
		cmdq_key_size = CMDQ_CRYPTO_KEY_SIZE_256;
		break;
	default:
		return -EINVAL;
	}

	data_unit_mask = get_data_unit_size_mask(data_unit_size);

	for (cap_idx = 0; cap_idx < host->crypto_capabilities.num_crypto_cap;
	     cap_idx++) {
		if (ccap_array[cap_idx].algorithm_id == cmdq_alg &&
		    (ccap_array[cap_idx].sdus_mask & data_unit_mask) &&
		    ccap_array[cap_idx].key_size == cmdq_key_size)
			return cap_idx;
	}

	return -EINVAL;
}
EXPORT_SYMBOL(cmdq_crypto_cap_find);

/**
 * cmdq_crypto_cfg_entry_write_key - Write a key into a crypto_cfg_entry
 *
 *	Writes the key with the appropriate format - for AES_XTS,
 *	the first half of the key is copied as is, the second half is
 *	copied with an offset halfway into the cfg->crypto_key array.
 *	For the other supported crypto algs, the key is just copied.
 *
 * @cfg: The crypto config to write to
 * @key: The key to write
 * @cap: The crypto capability (which specifies the crypto alg and key size)
 *
 * Returns 0 on success, or -EINVAL
 */
static int cmdq_crypto_cfg_entry_write_key(union cmdq_crypto_cfg_entry *cfg,
					     const u8 *key,
					     union cmdq_crypto_cap_entry cap)
{
	size_t key_size_bytes = get_keysize_bytes(cap.key_size);

	if (key_size_bytes == 0)
		return -EINVAL;

	switch (cap.algorithm_id) {
	case CMDQ_CRYPTO_ALG_AES_XTS:
		key_size_bytes *= 2;
		if (key_size_bytes > CMDQ_CRYPTO_KEY_MAX_SIZE)
			return -EINVAL;

		memcpy(cfg->crypto_key, key, key_size_bytes/2);
		memcpy(cfg->crypto_key + CMDQ_CRYPTO_KEY_MAX_SIZE/2,
		       key + key_size_bytes/2, key_size_bytes/2);
		return 0;
	case CMDQ_CRYPTO_ALG_BITLOCKER_AES_CBC:
		/* fall through */
	case CMDQ_CRYPTO_ALG_AES_ECB:
		/* fall through */
	case CMDQ_CRYPTO_ALG_ESSIV_AES_CBC:
		memcpy(cfg->crypto_key, key, key_size_bytes);
		return 0;
	}

	return -EINVAL;
}

static void cmdq_program_key(struct cmdq_host *host,
			const union cmdq_crypto_cfg_entry *cfg,
			int slot)
{
	int i;
	u32 slot_offset = host->crypto_cfg_register + slot * sizeof(*cfg);

	if (host->crypto_vops && host->crypto_vops->program_key)
		host->crypto_vops->program_key(host, cfg, slot);

	/* Clear the dword 16 */
	cmdq_writel(host, 0, slot_offset + 16 * sizeof(cfg->reg_val[0]));
	/* Ensure that CFGE is cleared before programming the key */
	wmb();
	for (i = 0; i < 16; i++) {
		cmdq_writel(host, le32_to_cpu(cfg->reg_val[i]),
			      slot_offset + i * sizeof(cfg->reg_val[0]));
		/* Spec says each dword in key must be written sequentially */
		wmb();
	}
	/* Write dword 17 */
	cmdq_writel(host, le32_to_cpu(cfg->reg_val[17]),
		      slot_offset + 17 * sizeof(cfg->reg_val[0]));
	/* Dword 16 must be written last */
	wmb();
	/* Write dword 16 */
	cmdq_writel(host, le32_to_cpu(cfg->reg_val[16]),
		      slot_offset + 16 * sizeof(cfg->reg_val[0]));
	/*Ensure that dword 16 is written */
	wmb();
}

static void cmdq_crypto_clear_keyslot(struct cmdq_host *host, int slot)
{
	union cmdq_crypto_cfg_entry cfg = { {0} };

	cmdq_program_key(host, &cfg, slot);
}

static void cmdq_crypto_clear_all_keyslots(struct cmdq_host *host)
{
	int slot;

	for (slot = 0; slot < cmdq_num_keyslots(host); slot++)
		cmdq_crypto_clear_keyslot(host, slot);
}

static int cmdq_crypto_keyslot_program(struct keyslot_manager *ksm,
					const struct blk_crypto_key *key,
					unsigned int slot)
{
	struct cmdq_host *host = keyslot_manager_private(ksm);
	int err = 0;
	u8 data_unit_mask;
	union cmdq_crypto_cfg_entry cfg;
	int cap_idx;

	cap_idx = cmdq_crypto_cap_find(host, key->crypto_mode,
					key->data_unit_size);

	if (!cmdq_is_crypto_enabled(host) ||
	    !cmdq_keyslot_valid(host, slot) ||
	    !cmdq_cap_idx_valid(host, cap_idx))
		return -EINVAL;

	data_unit_mask = get_data_unit_size_mask(key->data_unit_size);

	if (!(data_unit_mask & host->crypto_cap_array[cap_idx].sdus_mask))
		return -EINVAL;

	memset(&cfg, 0, sizeof(cfg));
	cfg.data_unit_size = data_unit_mask;
	cfg.crypto_cap_idx = cap_idx;
	cfg.config_enable |= CMDQ_CRYPTO_CONFIGURATION_ENABLE;

	err = cmdq_crypto_cfg_entry_write_key(&cfg, key->raw,
					host->crypto_cap_array[cap_idx]);
	if (err)
		return err;

	cmdq_program_key(host, &cfg, slot);

	memzero_explicit(&cfg, sizeof(cfg));

	return 0;
}

static int cmdq_crypto_keyslot_evict(struct keyslot_manager *ksm,
				      const struct blk_crypto_key *key,
				      unsigned int slot)
{
	struct cmdq_host *host = keyslot_manager_private(ksm);

	if (!cmdq_is_crypto_enabled(host) ||
	    !cmdq_keyslot_valid(host, slot))
		return -EINVAL;

	/*
	 * Clear the crypto cfg on the device. Clearing CFGE
	 * might not be sufficient, so just clear the entire cfg.
	 */
	cmdq_crypto_clear_keyslot(host, slot);

	return 0;
}

/* Functions implementing eMMC v5.2 specification behaviour */
void cmdq_crypto_enable_spec(struct cmdq_host *host)
{
	if (!cmdq_host_is_crypto_supported(host))
		return;

	host->caps |= CMDQ_CAP_CRYPTO_SUPPORT;
}
EXPORT_SYMBOL(cmdq_crypto_enable_spec);

void cmdq_crypto_disable_spec(struct cmdq_host *host)
{
	host->caps &= ~CMDQ_CAP_CRYPTO_SUPPORT;
}
EXPORT_SYMBOL(cmdq_crypto_disable_spec);

static const struct keyslot_mgmt_ll_ops cmdq_ksm_ops = {
	.keyslot_program	= cmdq_crypto_keyslot_program,
	.keyslot_evict		= cmdq_crypto_keyslot_evict,
};

enum blk_crypto_mode_num cmdq_crypto_blk_crypto_mode_num_for_alg_dusize(
	enum cmdq_crypto_alg cmdq_crypto_alg,
	enum cmdq_crypto_key_size key_size)
{
	/*
	 * Currently the only mode that eMMC and blk-crypto both support.
	 */
	if (cmdq_crypto_alg == CMDQ_CRYPTO_ALG_AES_XTS &&
		key_size == CMDQ_CRYPTO_KEY_SIZE_256)
		return BLK_ENCRYPTION_MODE_AES_256_XTS;

	return BLK_ENCRYPTION_MODE_INVALID;
}

/**
 * cmdq_host_init_crypto - Read crypto capabilities, init crypto fields in host
 * @host: Per adapter instance
 *
 * Returns 0 on success. Returns -ENODEV if such capabilities don't exist, and
 * -ENOMEM upon OOM.
 */
int cmdq_host_init_crypto_spec(struct cmdq_host *host,
				const struct keyslot_mgmt_ll_ops *ksm_ops)
{
	int cap_idx = 0;
	int err = 0;
	unsigned int crypto_modes_supported[BLK_ENCRYPTION_MODE_MAX];
	enum blk_crypto_mode_num blk_mode_num;

	/* Default to disabling crypto */
	host->caps &= ~CMDQ_CAP_CRYPTO_SUPPORT;

	if (!(cmdq_readl(host, CQCAP) & CQ_CAP_CS)) {
		pr_err("%s no crypto capability\n", __func__);
		err = -ENODEV;
		goto out;
	}

	/*
	 * Crypto Capabilities should never be 0, because the
	 * config_array_ptr > 04h. So we use a 0 value to indicate that
	 * crypto init failed, and can't be enabled.
	 */
	host->crypto_capabilities.reg_val = cmdq_readl(host, CQ_CCAP);
	host->crypto_cfg_register =
		(u32)host->crypto_capabilities.config_array_ptr * 0x100;
	host->crypto_cap_array =
		devm_kcalloc(mmc_dev(host->mmc),
				host->crypto_capabilities.num_crypto_cap,
				sizeof(host->crypto_cap_array[0]), GFP_KERNEL);
	if (!host->crypto_cap_array) {
		err = -ENOMEM;
		pr_err("%s no memory cap\n", __func__);
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
			cpu_to_le32(cmdq_readl(host,
						 CQ_CRYPTOCAP +
						 cap_idx * sizeof(__le32)));
		blk_mode_num = cmdq_crypto_blk_crypto_mode_num_for_alg_dusize(
				host->crypto_cap_array[cap_idx].algorithm_id,
				host->crypto_cap_array[cap_idx].key_size);
		if (blk_mode_num == BLK_ENCRYPTION_MODE_INVALID)
			continue;
		crypto_modes_supported[blk_mode_num] |=
				host->crypto_cap_array[cap_idx].sdus_mask * 512;
	}

	cmdq_crypto_clear_all_keyslots(host);

	host->ksm = keyslot_manager_create(cmdq_num_keyslots(host), ksm_ops,
					   BLK_CRYPTO_FEATURE_STANDARD_KEYS,
					   crypto_modes_supported, host);

	if (!host->ksm) {
		err = -ENOMEM;
		goto out_free_caps;
	}
	/*
	 * In case host controller supports cryptographic operations
	 * then, it uses 128bit task descriptor. Upper 64 bits of task
	 * descriptor would be used to pass crypto specific informaton.
	 */
	host->caps |= CMDQ_TASK_DESC_SZ_128;

	return 0;
out_free_caps:
	devm_kfree(mmc_dev(host->mmc), host->crypto_cap_array);
out:
	// TODO: print error?
	/* Indicate that init failed by setting crypto_capabilities to 0 */
	host->crypto_capabilities.reg_val = 0;
	return err;
}
EXPORT_SYMBOL(cmdq_host_init_crypto_spec);

void cmdq_crypto_setup_rq_keyslot_manager_spec(struct cmdq_host *host,
				struct request_queue *q)
{
	if (!cmdq_host_is_crypto_supported(host) || !q)
		return;

	q->ksm = host->ksm;
}
EXPORT_SYMBOL(cmdq_crypto_setup_rq_keyslot_manager_spec);

void cmdq_crypto_destroy_rq_keyslot_manager_spec(struct cmdq_host *host,
					      struct request_queue *q)
{
	keyslot_manager_destroy(host->ksm);
}
EXPORT_SYMBOL(cmdq_crypto_destroy_rq_keyslot_manager_spec);

int cmdq_prepare_crypto_desc_spec(struct cmdq_host *host,
				struct mmc_request *mrq,
				u64 *ice_ctx)
{
	struct bio_crypt_ctx *bc;
	struct request *req = mrq->req;

	if (!req->bio ||
	    !bio_crypt_should_process(req)) {
		*ice_ctx = 0;
		return 0;
	}
	if (WARN_ON(!cmdq_is_crypto_enabled(host))) {
		/*
		 * Upper layer asked us to do inline encryption
		 * but that isn't enabled, so we fail this request.
		 */
		return -EINVAL;
	}

	bc = req->bio->bi_crypt_context;

	if (!cmdq_keyslot_valid(host, bc->bc_keyslot))
		return -EINVAL;

	if (ice_ctx) {
		*ice_ctx = DATA_UNIT_NUM(bc->bc_dun[0]) |
			   CRYPTO_CONFIG_INDEX(bc->bc_keyslot) |
			   CRYPTO_ENABLE(true);
	}

	return 0;
}
EXPORT_SYMBOL(cmdq_prepare_crypto_desc_spec);

/* Crypto Variant Ops Support */

void cmdq_crypto_enable(struct cmdq_host *host)
{
	if (host->crypto_vops && host->crypto_vops->enable)
		return host->crypto_vops->enable(host);

	return cmdq_crypto_enable_spec(host);
}

void cmdq_crypto_disable(struct cmdq_host *host)
{
	if (host->crypto_vops && host->crypto_vops->disable)
		return host->crypto_vops->disable(host);

	return cmdq_crypto_disable_spec(host);
}

int cmdq_host_init_crypto(struct cmdq_host *host)
{
	if (host->crypto_vops && host->crypto_vops->host_init_crypto)
		return host->crypto_vops->host_init_crypto(host,
							   &cmdq_ksm_ops);

	return cmdq_host_init_crypto_spec(host, &cmdq_ksm_ops);
}

void cmdq_crypto_setup_rq_keyslot_manager(struct cmdq_host *host,
					    struct request_queue *q)
{
	if (host->crypto_vops && host->crypto_vops->setup_rq_keyslot_manager)
		return host->crypto_vops->setup_rq_keyslot_manager(host, q);

	return cmdq_crypto_setup_rq_keyslot_manager_spec(host, q);
}

void cmdq_crypto_destroy_rq_keyslot_manager(struct cmdq_host *host,
					      struct request_queue *q)
{
	if (host->crypto_vops && host->crypto_vops->destroy_rq_keyslot_manager)
		return host->crypto_vops->destroy_rq_keyslot_manager(host, q);

	return cmdq_crypto_destroy_rq_keyslot_manager_spec(host, q);
}

int cmdq_crypto_get_ctx(struct cmdq_host *host,
			       struct mmc_request *mrq,
			       u64 *ice_ctx)
{
	if (host->crypto_vops && host->crypto_vops->prepare_crypto_desc)
		return host->crypto_vops->prepare_crypto_desc(host, mrq,
								ice_ctx);

	return cmdq_prepare_crypto_desc_spec(host, mrq, ice_ctx);
}

int cmdq_complete_crypto_desc(struct cmdq_host *host,
				struct mmc_request *mrq,
				u64 *ice_ctx)
{
	if (host->crypto_vops && host->crypto_vops->complete_crypto_desc)
		return host->crypto_vops->complete_crypto_desc(host, mrq,
								ice_ctx);

	return 0;
}

void cmdq_crypto_debug(struct cmdq_host *host)
{
	if (host->crypto_vops && host->crypto_vops->debug)
		host->crypto_vops->debug(host);
}

void cmdq_crypto_set_vops(struct cmdq_host *host,
			struct cmdq_host_crypto_variant_ops *crypto_vops)
{
	if (host)
		host->crypto_vops = crypto_vops;
}

int cmdq_crypto_suspend(struct cmdq_host *host)
{
	if (host->crypto_vops && host->crypto_vops->suspend)
		return host->crypto_vops->suspend(host);

	return 0;
}

int cmdq_crypto_resume(struct cmdq_host *host)
{
	if (host->crypto_vops && host->crypto_vops->resume)
		return host->crypto_vops->resume(host);

	return 0;
}

int cmdq_crypto_reset(struct cmdq_host *host)
{
	if (host->crypto_vops && host->crypto_vops->reset)
		return host->crypto_vops->reset(host);

	return 0;
}

int cmdq_crypto_recovery_finish(struct cmdq_host *host)
{
	if (host->crypto_vops && host->crypto_vops->recovery_finish)
		return host->crypto_vops->recovery_finish(host);

	/* Reset/Recovery might clear all keys, so reprogram all the keys. */
	keyslot_manager_reprogram_all_keys(host->ksm);

	return 0;
}
