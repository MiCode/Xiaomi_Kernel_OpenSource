// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <crypto/algapi.h>
#include <linux/mmc/host.h>
#include "mmc-crypto.h"
#include "queue.h"

static bool mmc_cap_idx_valid(struct mmc_host *host, u8 cap_idx)
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

static size_t get_keysize_bytes(enum mmc_crypto_key_size size)
{
	switch (size) {
	case MMC_CRYPTO_KEY_SIZE_128: return 16;
	case MMC_CRYPTO_KEY_SIZE_192: return 24;
	case MMC_CRYPTO_KEY_SIZE_256: return 32;
	default: return 0;
	}
}

static u8 mmc_crypto_cap_find(void *mmc_p,
				  enum blk_crypto_mode_num crypto_mode,
				  unsigned int data_unit_size)
{
	struct mmc_host *host = mmc_p;
	enum mmc_crypto_alg mmc_alg;
	u8 data_unit_mask, cap_idx;
	enum mmc_crypto_key_size mmc_key_size;
	union mmc_crypto_cap_entry *ccap_array = host->crypto_cap_array;

	if (!mmc_is_crypto_supported(host))
		return -EINVAL;

	switch (crypto_mode) {
	case BLK_ENCRYPTION_MODE_AES_256_XTS:
		if (host->caps2 & (MMC_CAP2_CQE | MMC_CAP2_CQE_DCMD)) {
			mmc_alg = MMC_CRYPTO_ALG_AES_XTS;
			mmc_key_size = MMC_CRYPTO_KEY_SIZE_256;
		} else {
			/* "4" means XTS */
			mmc_alg = MMC_CRYPTO_ALG_INVALID;
			/* "2" means 256 bits */
			mmc_key_size = MMC_CRYPTO_KEY_SIZE_192;
		}
		break;
	default:
		return -EINVAL;
	}

	data_unit_mask = get_data_unit_size_mask(data_unit_size);

	for (cap_idx = 0; cap_idx < host->crypto_capabilities.num_crypto_cap;
	     cap_idx++) {
		if (ccap_array[cap_idx].algorithm_id == mmc_alg &&
		    (ccap_array[cap_idx].sdus_mask & data_unit_mask) &&
		    ccap_array[cap_idx].key_size == mmc_key_size)
			return cap_idx;
	}
	return -EINVAL;
}

/**
 * mmc_crypto_cfg_entry_write_key - Write a key into a crypto_cfg_entry
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
static int mmc_crypto_cfg_entry_write_key(union mmc_crypto_cfg_entry *cfg,
					     const u8 *key,
					     union mmc_crypto_cap_entry cap)
{
	size_t key_size_bytes = get_keysize_bytes(cap.key_size);

	/* non-cqe, only support 256 bits key size */
	if (cap.reserved == 0x5A)
		key_size_bytes = 32;

	if (key_size_bytes == 0)
		return -EINVAL;

	switch (cap.algorithm_id) {
	case MMC_CRYPTO_ALG_INVALID: /* non-cqe */
	case MMC_CRYPTO_ALG_AES_XTS:
		key_size_bytes *= 2;
		if (key_size_bytes > MMC_CRYPTO_KEY_MAX_SIZE)
			return -EINVAL;

		memcpy(cfg->crypto_key, key, key_size_bytes/2);
		memcpy(cfg->crypto_key + MMC_CRYPTO_KEY_MAX_SIZE/2,
		       key + key_size_bytes/2, key_size_bytes/2);
		return 0;
	case MMC_CRYPTO_ALG_BITLOCKER_AES_CBC: // fallthrough
	case MMC_CRYPTO_ALG_AES_ECB: // fallthrough
	case MMC_CRYPTO_ALG_ESSIV_AES_CBC:
		memcpy(cfg->crypto_key, key, key_size_bytes);
		return 0;
	}

	return -EINVAL;
}

static void program_key(struct mmc_host *host,
			const union mmc_crypto_cfg_entry *cfg,
			int slot)
{
	u32 aes_key[8] = {0}, aes_tkey[8] = {0};
	size_t key_size_bytes;
	enum mmc_crypto_key_size size;

	/* if cqe enabled, skip program key here,
	 * we will do it in low level driver
	 */
	if ((host->caps2 & (MMC_CAP2_CQE | MMC_CAP2_CQE_DCMD)))
		return;

	/* limit half_len as sizeof(u32)*8, avoid local buffer overflow */
	size = host->crypto_cap_array[slot].key_size;
	key_size_bytes = get_keysize_bytes(size);

	/* split key into key & tkey */
	memcpy(aes_key, &cfg->crypto_key[0], key_size_bytes/2);
	memcpy(aes_tkey,
		&cfg->crypto_key[MMC_CRYPTO_KEY_MAX_SIZE/2], key_size_bytes/2);
}

static int mmc_crypto_keyslot_program(struct keyslot_manager *ksm,
			const struct blk_crypto_key *key,
			unsigned int slot)

{
	struct mmc_host *host = keyslot_manager_private(ksm);
	int err = 0;
	u8 data_unit_mask;
	union mmc_crypto_cfg_entry cfg;
	union mmc_crypto_cfg_entry *cfg_arr;
	u8 cap_idx;

	if (!host || !key)
		return -EINVAL;

	cfg_arr = host->crypto_cfgs;

	cap_idx = mmc_crypto_cap_find(host, key->crypto_mode,
					       key->data_unit_size);

	if (!mmc_is_crypto_enabled(host) ||
	    !mmc_keyslot_valid(host, slot) ||
	    !mmc_cap_idx_valid(host, cap_idx))
		return -EINVAL;

	data_unit_mask = get_data_unit_size_mask(key->data_unit_size);

	if (!(data_unit_mask & host->crypto_cap_array[cap_idx].sdus_mask))
		return -EINVAL;

	memset(&cfg, 0, sizeof(cfg));
	cfg.data_unit_size = data_unit_mask;
	cfg.crypto_cap_idx = cap_idx;
	cfg.config_enable |= MMC_CRYPTO_CONFIGURATION_ENABLE;

	err = mmc_crypto_cfg_entry_write_key(&cfg, key->raw,
				host->crypto_cap_array[cap_idx]);
	if (err)
		return err;

	/* if cqe enabled, skip program key here,
	 * we will do it in low level driver
	 */
	if (!(host->caps2 & (MMC_CAP2_CQE | MMC_CAP2_CQE_DCMD)))
		program_key(host, &cfg, slot);

	memcpy(&cfg_arr[slot], &cfg, sizeof(cfg));
	memzero_explicit(&cfg, sizeof(cfg));

	return 0;
}

static int mmc_crypto_keyslot_evict(struct keyslot_manager *ksm,
			const struct blk_crypto_key *key,
			unsigned int slot)
{
	struct mmc_host *host = keyslot_manager_private(ksm);
	union mmc_crypto_cfg_entry *cfg_arr = host->crypto_cfgs;

	if (!mmc_is_crypto_enabled(host) ||
	    !mmc_keyslot_valid(host, slot))
		return -EINVAL;

	memset(&cfg_arr[slot], 0, sizeof(cfg_arr[slot]));

	/*
	 * Clear the crypto cfg on the device. Clearing CFGE
	 * might not be sufficient, so just clear the entire cfg.
	 */

	if (host->crypto_vops->msdc_crypto_keyslot_evict)
		host->crypto_vops->msdc_crypto_keyslot_evict(host);

	return 0;
}

struct mmc_crypto_variant_ops crypto_var_ops;
void mmc_crypto_enable_spec(struct mmc_host *host)
{
	union mmc_crypto_cfg_entry *cfg_arr = host->crypto_cfgs;
	int slot;

	if (!mmc_is_crypto_supported(host))
		return;

	/*
	 * Reset might clear all keys, so reprogram all the keys.
	 * Also serves to clear keys on driver init.
	 */
	for (slot = 0; slot < NUM_KEYSLOTS(host); slot++)
		program_key(host, &cfg_arr[slot], slot);
}
EXPORT_SYMBOL(mmc_crypto_enable_spec);

void mmc_crypto_disable_spec(struct mmc_host *host)
{
	host->caps2 &= ~MMC_CAP2_CRYPTO;
}
EXPORT_SYMBOL(mmc_crypto_disable_spec);

static const struct keyslot_mgmt_ll_ops mmc_ksm_ops = {
	.keyslot_program	= mmc_crypto_keyslot_program,
	.keyslot_evict		= mmc_crypto_keyslot_evict,
};

/**
 * mmc_init_crypto_spec - Read crypto capabilities, init crypto fields in host
 * @host: Per adapter instance
 *
 * Returns 0 on success. Returns -ENODEV if such capabilities don't exist, and
 * -ENOMEM upon OOM.
 */
int mmc_init_crypto_spec(struct mmc_host *host,
				const struct keyslot_mgmt_ll_ops *ksm_ops)
{
	int err;
	u32 count;
	unsigned int crypto_modes_supported[BLK_ENCRYPTION_MODE_MAX];

	if (!(host->caps2 & MMC_CAP2_CRYPTO)) {
		err = -ENODEV;
		goto out;
	}

	/*
	 * Crypto Capabilities should never be 0, because the
	 * config_array_ptr != 0. So we use a 0 value to indicate that
	 * crypto init failed, and can't be enabled.
	 */
	if (host->crypto_vops->host_init_crypto)
		host->crypto_vops->host_init_crypto(host);

	count = ((u32)(host->crypto_capabilities.num_crypto_cap) & 0xFF);
	host->crypto_cap_array =
		devm_kcalloc(&host->class_dev,
			     count,
			     sizeof(host->crypto_cap_array[0]),
			     GFP_KERNEL);
	if (!host->crypto_cap_array) {
		err = -ENOMEM;
		goto out;
	}

	/*
	 * Store all the capabilities now so that we don't need to repeatedly
	 * access the device each time we want to know its capabilities
	 */
	if (host->crypto_vops->get_crypto_capabilities)
		err = host->crypto_vops->get_crypto_capabilities(host);
	if (err)
		goto out_free_cfg_mem;

	host->crypto_cfgs =
		devm_kcalloc(&host->class_dev,
			     NUM_KEYSLOTS(host),
			     sizeof(host->crypto_cfgs[0]),
			     GFP_KERNEL);
	if (!host->crypto_cfgs) {
		err = -ENOMEM;
		goto out_free_cfg_mem;
	}
	/* Peng: temp */
	crypto_modes_supported[1] = 4096;

	host->ksm = keyslot_manager_create(NUM_KEYSLOTS(host), ksm_ops,
				crypto_modes_supported, host);

	if (!host->ksm) {
		err = -ENOMEM;
		goto out_free_crypto_cfgs;
	}

	return 0;
out_free_crypto_cfgs:
	devm_kfree(&host->class_dev, host->crypto_cfgs);
out_free_cfg_mem:
	devm_kfree(&host->class_dev, host->crypto_cap_array);
out:
	// TODO: print error?
	/* Indicate that init failed by setting crypto_capabilities to 0 */
	host->crypto_capabilities.reg_val = 0;

	return err;
}
EXPORT_SYMBOL(mmc_init_crypto_spec);

void mmc_crypto_setup_rq_keyslot_manager_spec(struct mmc_host *host,
						 struct request_queue *q)
{
	if (!mmc_is_crypto_supported(host) || !q)
		return;

	q->ksm = host->ksm;
}
EXPORT_SYMBOL(mmc_crypto_setup_rq_keyslot_manager_spec);

void mmc_crypto_destroy_rq_keyslot_manager_spec(struct mmc_host *host,
						   struct request_queue *q)
{
	keyslot_manager_destroy(host->ksm);
}
EXPORT_SYMBOL(mmc_crypto_destroy_rq_keyslot_manager_spec);

int mmc_prepare_mqr_crypto_spec(struct mmc_host *host,
					struct mmc_queue_req *mqr)
{
	struct bio_crypt_ctx *bc;
	struct request *request = mmc_queue_req_to_req(mqr);

	if (!request) {
		mqr->brq.mrq.crypto_enable = false;
		return 0;
	}

	if (!request->bio ||
	    !bio_crypt_should_process(request)) {
		mqr->brq.mrq.crypto_enable = false;
		return 0;
	}

	if (WARN_ON(!mmc_is_crypto_enabled(host))) {
		/*
		 * Upper layer asked us to do inline encryption
		 * but that isn't enabled, so we fail this request.
		 */
		return -EINVAL;
	}

	bc = request->bio->bi_crypt_context;
	if (!mmc_keyslot_valid(host, bc->bc_keyslot))
		return -EINVAL;

	mqr->brq.mrq.crypto_enable = true;
	mqr->brq.mrq.crypto_key_slot = bc->bc_keyslot;
	mqr->brq.mrq.data_unit_num = bc->bc_dun[0];
	return 0;
}
EXPORT_SYMBOL(mmc_prepare_mqr_crypto_spec);


/* Crypto Variant Ops Support */

void mmc_crypto_enable(struct mmc_host *host)
{
	if (host->crypto_vops && host->crypto_vops->enable)
		return host->crypto_vops->enable(host);

	return mmc_crypto_enable_spec(host);
}

void mmc_crypto_disable(struct mmc_host *host)
{
	if (host->crypto_vops && host->crypto_vops->disable)
		return host->crypto_vops->disable(host);

	return mmc_crypto_disable_spec(host);
}

int mmc_init_crypto(struct mmc_host *host)
{
	if (!(host->caps2 & MMC_CAP2_NO_SD))
		return 0;

	host->caps2 |= MMC_CAP2_CRYPTO;

	if (host->crypto_vops && host->crypto_vops->init_crypto)
		return host->crypto_vops->init_crypto(host,
							 &mmc_ksm_ops);

	return mmc_init_crypto_spec(host, &mmc_ksm_ops);
}

void mmc_crypto_setup_rq_keyslot_manager(struct mmc_host *host,
					    struct request_queue *q)
{
	if (host->crypto_vops && host->crypto_vops->setup_rq_keyslot_manager)
		return host->crypto_vops->setup_rq_keyslot_manager(host, q);

	return mmc_crypto_setup_rq_keyslot_manager_spec(host, q);
}

void mmc_crypto_destroy_rq_keyslot_manager(struct mmc_host *host,
					      struct request_queue *q)
{
	if (host->crypto_vops && host->crypto_vops->destroy_rq_keyslot_manager)
		return host->crypto_vops->destroy_rq_keyslot_manager(host, q);

	return mmc_crypto_destroy_rq_keyslot_manager_spec(host, q);
}

int mmc_prepare_mqr_crypto(struct mmc_host *host,
					struct mmc_queue_req *mqr)
{
	int ret, ddir, slot, tag;
	struct request *req = mmc_queue_req_to_req(mqr);

	ret = mmc_prepare_mqr_crypto_spec(host, mqr);
	if (ret || !mqr->brq.mrq.crypto_enable)
		return ret;
	///TODO: fix no need parameters
	/* non-CQE */
	if (!(host->caps2 & (MMC_CAP2_CQE | MMC_CAP2_CQE_DCMD))) {
		ddir = rq_data_dir(req);
		slot = req->bio->bi_crypt_context->bc_keyslot;
		tag = mqr->brq.mrq.tag;
		if (host->crypto_vops && host->crypto_vops->prepare_mqr_crypto)
			return host->crypto_vops->prepare_mqr_crypto(host,
			mqr->brq.mrq.data_unit_num, ddir, tag, slot);
	}

	return 0;
}

int mmc_swcq_prepare_mqr_crypto(struct mmc_host *host,
					struct mmc_request *mrq)
{
	int ret, ddir, slot, tag;
	struct request *req;
	struct mmc_queue_req *mqr;

	req = mrq->req;
	mqr = req_to_mmc_queue_req(req);

	ret = mmc_prepare_mqr_crypto_spec(host, mqr);
	if (ret || !mqr->brq.mrq.crypto_enable)
		return ret;

	/* non-CQE */
	if (!(host->caps2 & (MMC_CAP2_CQE | MMC_CAP2_CQE_DCMD))) {
		ddir = rq_data_dir(req);
		slot = req->bio->bi_crypt_context->bc_keyslot;
		tag = mqr->brq.mrq.tag;
		if (host->crypto_vops && host->crypto_vops->prepare_mqr_crypto)
			return host->crypto_vops->prepare_mqr_crypto(host,
			mqr->brq.mrq.data_unit_num, ddir, tag, slot);
	}

	return 0;
}


int mmc_complete_mqr_crypto(struct mmc_host *host)
{
	if (host->crypto_vops && host->crypto_vops->complete_mqr_crypto)
		return host->crypto_vops->complete_mqr_crypto(host);

	return 0;
}

void mmc_crypto_debug(struct mmc_host *host)
{
	if (host->crypto_vops && host->crypto_vops->debug)
		host->crypto_vops->debug(host);
}

int mmc_crypto_suspend(struct mmc_host *host)
{
	if (host->crypto_vops && host->crypto_vops->suspend)
		return host->crypto_vops->suspend(host);

	return 0;
}

int mmc_crypto_resume(struct mmc_host *host)
{
	if (host->crypto_vops && host->crypto_vops->resume)
		return host->crypto_vops->resume(host);

	return 0;
}

void mmc_crypto_set_vops(struct mmc_host *host,
			    struct mmc_crypto_variant_ops *crypto_vops)
{
	host->crypto_vops = crypto_vops;
}

