// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */
#include <linux/kernel.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include "mtk-mmc.h"
#include "mtk-mmc-swcqhci-crypto.h"

#define NUM_KEYSLOTS(host) \
	(((u32)(host->crypto_capabilities.config_count) & 0xFF))

/* vendor's host structure will be hook by mmc_host->private */
static inline void *get_ll_mmc_host(struct mmc_host *host)
{
	return (void *)host->private;
}

static inline bool mmc_keyslot_valid(struct swcq_host *cq_host, unsigned int slot)
{
	/*
	 * The actual number of configurations supported is (CFGC+1), so slot
	 * numbers range from 0 to config_count inclusive.
	 */
	return slot < NUM_KEYSLOTS(cq_host);
}

#ifdef DEBUG
static inline void dump_key(const char *func, const char *msg,
	const u8 *key, const u8 *tkey, size_t len)
{
	char buf[1024] = {0};
	int i = 0;
	char *tmp = buf;
	char *dump_buf = buf;

	memcpy(dump_buf, "key: ", 5);
	dump_buf += 5;
	for (i = 0; i < 8; i++) {
		sprintf(dump_buf, "%02x", key[i]);
		dump_buf += 2;
	}
	dump_buf[0] = '-';
	dump_buf++;
	for (i = len-8; i < len; i++) {
		sprintf(dump_buf, "%02x", key[i]);
		dump_buf += 2;
	}
	memcpy(dump_buf, ", tkey: ", 8);
	dump_buf += 8;
	for (i = 0; i < 8; i++) {
		sprintf(dump_buf, "%02x", tkey[i]);
		dump_buf += 2;
	}
	dump_buf[0] = '-';
	dump_buf++;
	for (i = len-8; i < len; i++) {
		sprintf(dump_buf, "%02x", tkey[i]);
		dump_buf += 2;
	}
	pr_info("mmc:[%s]: %s: %s\n", func, msg, tmp);
}
#endif

static inline bool mmc_is_crypto_supported(struct swcq_host *cq_host)
{
	return cq_host->crypto_capabilities.reg_val != 0;
}

static inline bool mmc_is_crypto_enabled(struct mmc_host *host)
{
	return host->caps2 & MMC_CAP2_CRYPTO;
}

static inline struct swcq_host *
swcq_host_from_ksm(struct blk_keyslot_manager *ksm)
{
	struct mmc_host *mmc = container_of(ksm, struct mmc_host, ksm);

	return mmc->cqe_private;
}

static bool mmc_cap_idx_valid(struct swcq_host *cq_host, u8 cap_idx)
{
	return cap_idx < cq_host->crypto_capabilities.num_crypto_cap;
}

static u8 get_data_unit_size_mask(unsigned int data_unit_size)
{
	if (data_unit_size < 512 || data_unit_size > 65536 ||
	    !is_power_of_2(data_unit_size))
		return 0;

	return data_unit_size / 512;
}

static size_t get_keysize_bytes(enum swcqhci_crypto_key_size size)
{
	switch (size) {
	case SWCQHCI_CRYPTO_KEY_SIZE_128: return 16;
	case SWCQHCI_CRYPTO_KEY_SIZE_192: return 24;
	case SWCQHCI_CRYPTO_KEY_SIZE_256: return 32;
	default: return 0;
	}
}

static u8 mmc_crypto_cap_find(void *mmc_p,
				  enum blk_crypto_mode_num crypto_mode,
				  unsigned int data_unit_size)
{
	struct swcq_host *cq_host = mmc_p;
	enum swcqhci_crypto_alg mmc_alg;
	u8 data_unit_mask, cap_idx;
	enum swcqhci_crypto_key_size mmc_key_size;
	union swcqhci_crypto_cap_entry *ccap_array = cq_host->crypto_cap_array;

	if (!mmc_is_crypto_supported(cq_host))
		return -EINVAL;

	switch (crypto_mode) {
	case BLK_ENCRYPTION_MODE_AES_256_XTS:
		/* "4" means XTS */
		mmc_alg = SWCQHCI_CRYPTO_ALG_AES_XTS;
		/* "2" means 256 bits */
		mmc_key_size = SWCQHCI_CRYPTO_KEY_SIZE_256;
		break;
	default:
		return -EINVAL;
	}

	data_unit_mask = get_data_unit_size_mask(data_unit_size);
	/* There is only one capability */
	for (cap_idx = 0; cap_idx < cq_host->crypto_capabilities.num_crypto_cap;
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
static int mmc_crypto_cfg_entry_write_key(union swcqhci_crypto_cfg_entry *cfg,
					     const u8 *key,
					     union swcqhci_crypto_cap_entry cap)
{
	size_t key_size_bytes = get_keysize_bytes(cap.key_size);

	if (key_size_bytes == 0)
		return -EINVAL;

	switch (cap.algorithm_id) {
	case SWCQHCI_CRYPTO_ALG_INVALID: /* non-cqe */
	case SWCQHCI_CRYPTO_ALG_AES_XTS:
		key_size_bytes *= 2;
		if (key_size_bytes > MMC_CRYPTO_KEY_MAX_SIZE)
			return -EINVAL;

		memcpy(cfg->crypto_key, key, key_size_bytes/2);
		memcpy(cfg->crypto_key + MMC_CRYPTO_KEY_MAX_SIZE/2,
		       key + key_size_bytes/2, key_size_bytes/2);
		return 0;
	case SWCQHCI_CRYPTO_ALG_BITLOCKER_AES_CBC:
	case SWCQHCI_CRYPTO_ALG_AES_ECB:
	case SWCQHCI_CRYPTO_ALG_ESSIV_AES_CBC:
		memcpy(cfg->crypto_key, key, key_size_bytes);
		return 0;
	}

	return -EINVAL;
}

static int mmc_crypto_keyslot_program(struct blk_keyslot_manager *ksm,
			const struct blk_crypto_key *key,
			unsigned int slot)

{
	struct swcq_host *cq_host = swcq_host_from_ksm(ksm);
	int err = 0;
	u8 data_unit_mask;
	union swcqhci_crypto_cfg_entry cfg = {};
	union swcqhci_crypto_cfg_entry *cfg_arr;
	int cap_idx = -1;

	BUILD_BUG_ON(SWCQHCI_CRYPTO_KEY_SIZE_INVALID != 4);

	if (!cq_host || !key)
		return -EINVAL;

	cfg_arr = cq_host->crypto_cfgs;

	cap_idx = mmc_crypto_cap_find(cq_host, key->crypto_cfg.crypto_mode,
					       key->crypto_cfg.data_unit_size);
	if (WARN_ON(cap_idx < 0)) {
		dev_dbg(mmc_dev(cq_host->mmc), "[%s]: cap idx[%d] is less than 0\n",
			__func__, cap_idx);
		return -EOPNOTSUPP;
	}

	if (!mmc_is_crypto_enabled(cq_host->mmc) ||
	    !mmc_keyslot_valid(cq_host, slot) ||
	    !mmc_cap_idx_valid(cq_host, cap_idx)) {
		dev_dbg(mmc_dev(cq_host->mmc), "[%s]: crypto is invalid with slot: %d, cap_idx: %d\n",
			__func__, slot, cap_idx);
		return -EINVAL;
	}

	data_unit_mask = get_data_unit_size_mask(key->crypto_cfg.data_unit_size);

	if (!(data_unit_mask & cq_host->crypto_cap_array[cap_idx].sdus_mask)) {
		dev_dbg(mmc_dev(cq_host->mmc), "[%s]: data_unit_mask[0x%x] is invalid.\n",
			__func__, data_unit_mask);
		return -EINVAL;
	}

	memset(&cfg, 0, sizeof(cfg));

	cfg.data_unit_size = data_unit_mask;
#ifdef CONFIG_MMC_CRYPTO_LEGACY
	/* used fsrypt v2 in OTA fscrypt v1 environment */
	if (key->hie_duint_size != 4096)
		cfg.data_unit_size = 1;
#endif

	cfg.crypto_cap_idx = cap_idx;
	cfg.config_enable |= MMC_CRYPTO_CONFIGURATION_ENABLE;

	err = mmc_crypto_cfg_entry_write_key(&cfg, key->raw,
				cq_host->crypto_cap_array[cap_idx]);
	if (err) {
		dev_warn(mmc_dev(cq_host->mmc), "[%s]: write key to crypto_cfg error: %d\n",
			__func__, err);
		return err;
	}

	memcpy(&cfg_arr[slot], &cfg, sizeof(cfg));
	memzero_explicit(&cfg, sizeof(cfg));

	return 0;
}

static int mmc_crypto_keyslot_evict(struct blk_keyslot_manager *ksm,
			const struct blk_crypto_key *key,
			unsigned int slot)
{
	struct swcq_host *cq_host = swcq_host_from_ksm(ksm);
	union swcqhci_crypto_cfg_entry *cfg_arr = cq_host->crypto_cfgs;

	memset(&cfg_arr[slot], 0, sizeof(cfg_arr[slot]));

	return 0;
}

static const struct blk_ksm_ll_ops swcq_ksm_ops = {
	.keyslot_program	= mmc_crypto_keyslot_program,
	.keyslot_evict		= mmc_crypto_keyslot_evict,
};

/**
 * swcq_mmc_init_crypto_spec - Read crypto capabilities, init crypto fields in host
 * @host: Per adapter instance
 *
 * Returns 0 on success. Returns -ENODEV if such capabilities don't exist, and
 * -ENOMEM upon OOM.
 */
static int swcq_mmc_init_crypto_spec(struct mmc_host *mmc,
		const struct blk_ksm_ll_ops *ksm_ops)
{
	int err;
	u32 count, num_keyslots;
	struct swcq_host *cq_host;
	unsigned int crypto_modes_supported[BLK_ENCRYPTION_MODE_MAX];
	struct device *dev = mmc_dev(mmc);

	cq_host = mmc->cqe_private;
	memset(crypto_modes_supported, 0,
		sizeof(crypto_modes_supported[BLK_ENCRYPTION_MODE_MAX]));

	if (!(mmc->caps2 & MMC_CAP2_CRYPTO)) {
		err = -ENODEV;
		goto out;
	}

	/* in sw cqhci, crypto count is fixed value 32 */
	cq_host->crypto_capabilities.config_count = 32;
	/* in swcq, support only one */
	cq_host->crypto_capabilities.num_crypto_cap = 1;

	count = ((u32)(cq_host->crypto_capabilities.num_crypto_cap) & 0xFF);
	cq_host->crypto_cap_array =
		devm_kcalloc(mmc_dev(mmc),
			     count,
			     sizeof(cq_host->crypto_cap_array[0]),
			     GFP_KERNEL);
	if (!cq_host->crypto_cap_array) {
		err = -ENOMEM;
		goto out;
	}

	/*
	 * swcq has only one algorithm
	 * algorithm_id : AES-XTS (04)
	 * sdus_mask: support 512 & 4096 (09)
	 * key_size: 256bits (02)
	 * reserved: 0x5A (meaningless)
	 */
	cq_host->crypto_cap_array[0].reg_val =
			((u32)0x5A020904 & 0xFFFFFFFF);
	dev_dbg(dev, "crypto cap array: 0x%x, id: %u, mask: %u, size: %u\n",
		cq_host->crypto_cap_array[0].reg_val,
		cq_host->crypto_cap_array[0].algorithm_id,
		cq_host->crypto_cap_array[0].sdus_mask,
		cq_host->crypto_cap_array[0].key_size);


	cq_host->crypto_cfgs =
		devm_kcalloc(mmc_dev(mmc),
			     NUM_KEYSLOTS(cq_host),
			     sizeof(cq_host->crypto_cfgs[0]),
			     GFP_KERNEL);
	if (!cq_host->crypto_cfgs) {
		err = -ENOMEM;
		goto out_free_cfg_mem;
	}

	/* swcq has only one key slot */
	num_keyslots = cq_host->crypto_capabilities.config_count;

	err = devm_blk_ksm_init(mmc_dev(mmc), &mmc->ksm, num_keyslots);
	if (err)
		goto out;

	mmc->ksm.ksm_ll_ops = swcq_ksm_ops;
	mmc->ksm.dev = mmc_dev(mmc);

	/* only supports 32 DUN bits. */
	mmc->ksm.max_dun_bytes_supported = 4;

	mmc->ksm.features = BLK_CRYPTO_FEATURE_STANDARD_KEYS;

	/* Hardcode for special swcmdq */
	mmc->ksm.crypto_modes_supported[1] = 0x1200;

	dev_dbg(dev, "mmc crypto init done 0x%p\n", &(mmc->ksm.ksm_ll_ops));

	return 0;
out:
	devm_kfree(mmc_dev(mmc), cq_host->crypto_cfgs);
out_free_cfg_mem:
	devm_kfree(mmc_dev(mmc), cq_host->crypto_cap_array);
	/* Indicate that init failed by setting crypto_capabilities to 0 */
	cq_host->crypto_capabilities.reg_val = 0;
	mmc->caps2 &= ~MMC_CAP2_CRYPTO;

	return err;
}

int swcq_mmc_init_crypto(struct mmc_host *mmc)
{
	if (mmc->caps2 & MMC_CAP2_NO_MMC)
		return 0;

	mmc->caps2 |= MMC_CAP2_CRYPTO;

	return swcq_mmc_init_crypto_spec(mmc, &swcq_ksm_ops);
}

static void msdc_complete_mqr_crypto(struct mmc_host *mmc)
{
	u32 val;
	struct msdc_host *host = mmc_priv(mmc);

	/* only for non-cqe, cqe needs nothing */
	if ((readl(host->base + MSDC_AES_SWST)
			& MSDC_AES_BYPASS) == 0) {
		/* disable AES path by set bypass bit */
		val = readl(host->base + MSDC_AES_SWST);
		val |= (u32)(MSDC_AES_BYPASS);
		writel(val, host->base + MSDC_AES_SWST);
	}
}

/* non-cqe only set IV here */
static int set_crypto(struct msdc_host *host,
		u64 data_unit_num, int ddir)
{
	u32 aes_mode_current = 0;
	u32 ctr[4] = {0};
	u32 val;
	unsigned long polling_tmo = 0;

	aes_mode_current = MSDC_AES_MODE_1 &
		readl(host->base + MSDC_AES_CFG_GP1);

	switch (aes_mode_current) {
	case MSDC_CRYPTO_ALG_AES_XTS:
	{
		ctr[0] = data_unit_num & 0xffffffff;
		if ((data_unit_num >> 32) & 0xffffffff)
			ctr[1] = (data_unit_num >> 32) & 0xffffffff;
		break;
	}
	default:
		pr_notice("msdc unknown aes mode 0x%x\n", aes_mode_current);
		WARN_ON(1);
		return -EINVAL;
	}

	/* 1. set IV */
	writel(ctr[0], host->base + MSDC_AES_CTR0_GP1);
	writel(ctr[1], host->base + MSDC_AES_CTR1_GP1);
	writel(ctr[2], host->base + MSDC_AES_CTR2_GP1);
	writel(ctr[3], host->base + MSDC_AES_CTR3_GP1);

	/* 2. enable AES path */
	val = readl(host->base + MSDC_AES_SWST);
	val &= ~((u32)(MSDC_AES_BYPASS));
	writel(val, host->base + MSDC_AES_SWST);

	/* 3. AES switch start (flush the configure) */
	if (ddir == WRITE) {
		val = readl(host->base + MSDC_AES_SWST);
		val |= (u32)(MSDC_AES_SWITCH_START_ENC);
		writel(val, host->base + MSDC_AES_SWST);
		polling_tmo = jiffies + HZ*3;
		while (readl(host->base + MSDC_AES_SWST)
			& MSDC_AES_SWITCH_START_ENC) {
			cpu_relax();
			if (time_after(jiffies, polling_tmo)) {
				pr_notice("msdc error: trigger AES ENC timeout!\n");
				WARN_ON(1);
				return -EINVAL;
			}
		}
	} else {
		val = readl(host->base + MSDC_AES_SWST);
		val |= (u32)(MSDC_AES_SWITCH_START_DEC);
		writel(val, host->base + MSDC_AES_SWST);
		polling_tmo = jiffies + HZ*3;
		while (readl(host->base + MSDC_AES_SWST)
			& MSDC_AES_SWITCH_START_DEC) {
			cpu_relax();
			if (time_after(jiffies, polling_tmo)) {
				pr_notice("msdc error: trigger AES DEC timeout!\n");
				WARN_ON(1);
				return -EINVAL;
			}
		}
	}

	return 0;
}

/* only non-cqe uses this to set key */
static void msdc_crypto_program_key(struct mmc_host *mmc,
			u32 *key, u32 *tkey, u32 config)
{
	struct msdc_host *host = mmc_priv(mmc);
	int i;
	int iv[4] = {0};
	u32 val;

	if (!host || !host->base)
		return;

	/* disable AES path firstly if need for safety */
	msdc_complete_mqr_crypto(mmc);

	if (unlikely(!*key && !tkey)) {
		/* disable AES path by set bypass bit */
		val = readl(host->base + MSDC_AES_SWST);
		val |= MSDC_AES_BYPASS;
		writel(val, host->base + MSDC_AES_SWST);
		return;
	}

	/* switch crypto engine to MSDC */

	/* write AES config */
	writel(0, host->base + MSDC_AES_CFG_GP1);
	writel(config, host->base + MSDC_AES_CFG_GP1);

	if (!(readl(host->base + MSDC_AES_CFG_GP1)))
		pr_notice("%s write config fail %d!!\n", __func__, config);

	/* IV */
	for (i = 0; i < 4; i++)
		writel(iv[i], host->base + (MSDC_AES_IV0_GP1 + i * 4));

	/* KEY */
	for (i = 0; i < 8; i++)
		writel(key[i], host->base + (MSDC_AES_KEY_GP1 + i * 4));
	/* TKEY */
	for (i = 0; i < 8; i++)
		writel(tkey[i], host->base + (MSDC_AES_TKEY_GP1 + i * 4));

}

int swcq_mmc_start_crypto(struct mmc_host *mmc,
		struct mmc_request *mrq, u32 opcode)
{
	int ddir, slot;
	u32 data_unit_size, aes_config;
	u32 aes_key[8] = {0}, aes_tkey[8] = {0};
	struct swcq_host *swcq_host = NULL;
	struct msdc_host *host = mmc_priv(mmc);

	if (!host || !mrq->crypto_ctx)
		return 0;

	slot = mrq->crypto_key_slot;
	swcq_host = host->swcq_host;

	data_unit_size = mrq->crypto_ctx->bc_key->crypto_cfg.data_unit_size;
	if (data_unit_size > 4096) {
		WARN_ON(1);
		return -EDOM;
	}

	/* There is only one cap in sw-cqhci */
	aes_config = (mrq->crypto_ctx->bc_key->crypto_cfg.data_unit_size) << 16 |
		swcq_host->crypto_cap_array[0].key_size << 8 |
		swcq_host->crypto_cap_array[0].algorithm_id << 0;

	memcpy(aes_key,	&(swcq_host->crypto_cfgs[slot].crypto_key[0]),
		MMC_CRYPTO_KEY_MAX_SIZE/2);
	memcpy(aes_tkey, &(swcq_host->crypto_cfgs[slot].crypto_key[MMC_CRYPTO_KEY_MAX_SIZE/2]),
		MMC_CRYPTO_KEY_MAX_SIZE/2);

	/* low layer set key: key had been set in upper layer */
	msdc_crypto_program_key(mmc, aes_key, aes_tkey, aes_config);

	/* set IV and trigger crypto engine */
	ddir = (opcode == MMC_EXECUTE_WRITE_TASK) ? 1 : 0;
	return set_crypto(host, mrq->crypto_ctx->bc_dun[0], ddir);
}

void swcq_mmc_complete_mqr_crypto(struct mmc_host *mmc)
{
	return msdc_complete_mqr_crypto(mmc);
}
