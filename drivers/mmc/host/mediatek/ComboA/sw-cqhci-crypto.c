// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include "mtk_sd.h"

static int msdc_complete_mqr_crypto(struct mmc_host *host)
{
	struct msdc_host *ll_host = mmc_priv(host);

	/* only for non-cqe, cqe needs nothing */
	if ((readl(ll_host->base + MSDC_AES_SWST)
			& MSDC_AES_BYPASS) == 0)
		/* disable AES path by set bypass bit */
		MSDC_SET_BIT32(ll_host->base + MSDC_AES_SWST, MSDC_AES_BYPASS);

	return 0;
}

/* non-cqe only set IV here */
static int set_crypto(struct msdc_host *host,
		u64 data_unit_num, int ddir)
{
	u32 aes_mode_current = 0;
	u32 ctr[4] = {0};
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
		msdc_dump_info(NULL, 0, NULL, host->id);
		WARN_ON(1);
		return -EINVAL;
	}

	/* 1. set IV */
	writel(ctr[0], host->base + MSDC_AES_CTR0_GP1);
	writel(ctr[1], host->base + MSDC_AES_CTR1_GP1);
	writel(ctr[2], host->base + MSDC_AES_CTR2_GP1);
	writel(ctr[3], host->base + MSDC_AES_CTR3_GP1);

	/* 2. enable AES path */
	MSDC_CLR_BIT32(host->base + MSDC_AES_SWST, MSDC_AES_BYPASS);

	/* 3. AES switch start (flush the configure) */
	if (ddir == WRITE) {
		MSDC_SET_BIT32(host->base + MSDC_AES_SWST,
			MSDC_AES_SWITCH_START_ENC);
		polling_tmo = jiffies + HZ*3;
		while (readl(host->base + MSDC_AES_SWST)
			& MSDC_AES_SWITCH_START_ENC) {
			if (time_after(jiffies, polling_tmo)) {
				pr_notice(
				"msdc error: trigger AES ENC timeout!\n");
				WARN_ON(1);
				return -EINVAL;
			}
		}
	} else {
		MSDC_SET_BIT32(host->base + MSDC_AES_SWST,
			MSDC_AES_SWITCH_START_DEC);
		polling_tmo = jiffies + HZ*3;
		while (readl(host->base + MSDC_AES_SWST)
			& MSDC_AES_SWITCH_START_DEC) {
			if (time_after(jiffies, polling_tmo)) {
				pr_notice(
				"msdc error: trigger AES DEC timeout!\n");
				WARN_ON(1);
				return -EINVAL;
			}
		}
	}
	return 0;
}

/* only non-cqe uses this to set key */
static void msdc_crypto_program_key(struct mmc_host *host,
			u32 *key, u32 *tkey, u32 config)
{
	struct msdc_host *ll_host = mmc_priv(host);
	int i;
	int iv[4] = {0};

	if (!ll_host || !ll_host->base)
		return;

	/* disable AES path firstly if need for safety */
	msdc_complete_mqr_crypto(host);

	if (unlikely(!*key && !tkey)) {
		/* disable AES path by set bypass bit */
		MSDC_SET_BIT32(ll_host->base + MSDC_AES_SWST, MSDC_AES_BYPASS);
		return;
	}

	/* switch crypto engine to MSDC */

	/* write AES config */
	MSDC_WRITE32(ll_host->base + MSDC_AES_CFG_GP1, 0);
	MSDC_SET_BIT32(ll_host->base + MSDC_AES_CFG_GP1, config);

	if (!(readl(ll_host->base + MSDC_AES_CFG_GP1)))
		pr_notice("%s write config fail %d!!\n", __func__, config);

	/* IV */
	for (i = 0; i < 4; i++)
		writel(iv[i], ll_host->base + (MSDC_AES_IV0_GP1 + i * 4));

	/* KEY */
	for (i = 0; i < 8; i++)
		writel(key[i], ll_host->base + (MSDC_AES_KEY_GP1 + i * 4));
	/* TKEY */
	for (i = 0; i < 8; i++)
		writel(tkey[i], ll_host->base + (MSDC_AES_TKEY_GP1 + i * 4));

}

/* set crypto information */
static int msdc_prepare_mqr_crypto(struct mmc_host *host,
		u64 data_unit_num, int ddir, int tag, int slot)
{
	u32 data_unit_size;
	u32 aes_config;
	struct msdc_host *ll_host = mmc_priv(host);
	u32 aes_key[8] = {0}, aes_tkey[8] = {0};

	data_unit_size = host->crypto_cfgs[slot].data_unit_size * 512;

	if (data_unit_size > 4096) {
		WARN_ON(1);
		return -EDOM;
	}
	/* There is only one cap in sw-cqhci */
	aes_config = (data_unit_size) << 16 |
		host->crypto_cap_array[0].key_size << 8 |
		host->crypto_cap_array[0].algorithm_id << 0;

	memcpy(aes_key,
	&(host->crypto_cfgs[slot].crypto_key[0]),
	MMC_CRYPTO_KEY_MAX_SIZE/2);
	memcpy(aes_tkey,
	&(host->crypto_cfgs[slot].crypto_key[MMC_CRYPTO_KEY_MAX_SIZE/2]),
	MMC_CRYPTO_KEY_MAX_SIZE/2);
	/* low layer set key: key had been set in upper layer */
	msdc_crypto_program_key(host, aes_key, aes_tkey, aes_config);

	/* switch crypto engine to MSDC */
	return set_crypto(ll_host, data_unit_num, ddir);
}

static void msdc_init_crypto(struct mmc_host *host)
{
	if (host->caps2 & (MMC_CAP2_CQE | MMC_CAP2_CQE_DCMD)) {
		WARN_ON(1);
	} else {
		host->crypto_capabilities.config_count = 32;
		/* in non-CQHCI, support only one */
		host->crypto_capabilities.num_crypto_cap = 1;
	}
}

static int msdc_get_crypto_capabilities(struct mmc_host *host)
{
	u8 cap_idx;

	/* CQE shouldn't go here */
	WARN_ON(host->caps2 & (MMC_CAP2_CQE | MMC_CAP2_CQE_DCMD));

	/*
	 * non-cqe has only one algorithm
	 * algorithm_id : AES-XTS (04)
	 * sdus_mask: support 512 & 4096 (09)
	 * key_size: 256bits (02)
	 * reserved: 0x5A (meaningless)
	 * please noted that we used 0x5A to distinguish non-cqe and cqe
	 */
	cap_idx = 0;
	host->crypto_cap_array[cap_idx].reg_val =
		((u32)0x5A020904 & 0xFFFFFFFF);

	return 0;
}

static const struct mmc_crypto_variant_ops crypto_var_ops = {
	.host_init_crypto = msdc_init_crypto,
	.get_crypto_capabilities = msdc_get_crypto_capabilities,
	.prepare_mqr_crypto = msdc_prepare_mqr_crypto,
	.host_program_key = msdc_crypto_program_key,
	.complete_mqr_crypto = msdc_complete_mqr_crypto,
};

void msdc_crypto_init_vops(struct mmc_host *host)
{
	host->crypto_vops = &crypto_var_ops;
}
