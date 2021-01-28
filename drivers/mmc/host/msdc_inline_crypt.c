// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/blkdev.h>
#include "mtk_sd.h"
#include "cmdq_hci.h"
#include "msdc_inline_crypt.h"

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
		/* Now, mediatek host had a
		 * fixed the crypto data unit (512bytes).
		 * That means that we cannot support the
		 * non-512bytes sector size.
		 */
		ctr[0] = ctr[0] * 8;
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

static inline
int cqhci_set_crypto_desc(u64 task_desc, u64 ctx)
{
	u64 *cqhci_crypto_desc = NULL;

	/*
	 * Get the address of crypto context for the given task descriptor.
	 * ice context is present in the upper 64bits of task descriptor
	 */
	cqhci_crypto_desc = (__le64 __force *)(task_desc +
						CQ_TASK_DESC_TASK_PARAMS_SIZE);
	if (IS_ERR_OR_NULL(cqhci_crypto_desc))
		return -EFAULT;

	memset(cqhci_crypto_desc, 0, CQ_TASK_DESC_CE_PARAMS_SIZE);

	/*
	 *  Assign upper 64bits data of task descritor with crypto context
	 */
	if (cqhci_crypto_desc)
		*cqhci_crypto_desc = cpu_to_le64(ctx);

	return 0;
}

/* cqe set key here */
static void msdc_cqhci_crypto_cfg(struct mmc_host *host,
		int tag, int slot)
{
	struct msdc_host *ll_host = mmc_priv(host);
	u32 addr, i, cc_idx;
	union cqhci_cpt_cap cpt_cap;
	union mmc_crypto_cfg_entry cfg;

	cc_idx = tag;
	cpt_cap.cap_raw = readl(ll_host->cq_host->mmio + MSDC_CRCAP);
	addr = (cpt_cap.cap.cfg_ptr << 8) + (u32)(cc_idx << 7);

	memset(&cfg, 0, sizeof(cfg));
	memcpy(&cfg, &host->crypto_cfgs[slot], sizeof(cfg));

	/* fixed 512 bytes */
	cfg.data_unit_size = 1;

	/* write configuration only to register */
	for (i = 0; i < 32; i++) {
		writel(cfg.reg_val[i],
			ll_host->cq_host->mmio + (addr + i * 4));
#ifdef _CQHCI_DEBUG
		pr_notice("%s: (%d)REG[0x%x]=0x%x\n",
			__func__, cc_idx,
			(addr + i * 4),
			cfg.reg_val[i]);
#endif
	}
}

int cqhci_crypto_start(struct mmc_host *host, struct mmc_request *mrq)
{
	struct msdc_host *ll_host = mmc_priv(host);
	u64 ctx, task_desc, dun;
	int slot, err, cc_idx;
	u8 tag;
	bool c_en = false;
	u8 *start;

	if (!mrq->crypto_enable)
		goto skip_crypto;

	slot = mrq->crypto_key_slot;
	cc_idx = mrq->cmdq_req->tag;

	/* Now, mediatek host had a fixed the crypto data unit (512bytes)
	 * That means that we cannot support the non-512bytes sector size.
	 * Specially, don't care overflow, because mmc host only support 32
	 * bits dun.
	 */
	dun = mrq->data_unit_num * 8;

	/* KEY */
	msdc_cqhci_crypto_cfg(host, mrq->cmdq_req->tag, slot);
	c_en = true;

	/* IV */
	ctx = DATA_UNIT_NUM(dun) |
		CRYPTO_CONFIG_INDEX(cc_idx) |
		CRYPTO_ENABLE(!!c_en);

	tag = mrq->cmdq_req->tag;
	start = get_desc(ll_host->cq_host, tag);
	if (IS_ERR_OR_NULL(start))
		return -EFAULT;

	task_desc = (u64)start;

	err = cqhci_set_crypto_desc((u64)task_desc, ctx);
	if (err)
		return err;

skip_crypto:
	return 0;
}


/* only non-cqe uses this to set key */
static void msdc_crypto_program_key(struct mmc_host *host,
			u32 *key, u32 *tkey, u32 config)
{
	struct msdc_host *ll_host = mmc_priv(host);
	int i;
	int iv[4] = {0};

	/* CQE shouldn't go here */
	WARN_ON(host->caps2 & (MMC_CAP2_CQE | MMC_CAP2_CQE_DCMD));

	if (!ll_host || !ll_host->base)
		return;

	if (unlikely(!*key && !tkey)) {
		/* disable AES path by set bypass bit */
		MSDC_SET_BIT32(ll_host->base + MSDC_AES_SWST, MSDC_AES_BYPASS);
		return;
	}

	/* switch crypto engine to MSDC */

	/* write AES config */
	MSDC_SET_BIT32(ll_host->base + MSDC_AES_CFG_GP1, config);

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
	struct msdc_host *ll_host = mmc_priv(host);
	u32 aes_key[8] = {0}, aes_tkey[8] = {0};
	u32 aes_config = (512) << 16 |
		host->crypto_cap_array[slot].key_size << 8 |
		host->crypto_cap_array[slot].algorithm_id << 0;

	/* CQE shouldn't go here */
	WARN_ON(host->caps2 & (MMC_CAP2_CQE | MMC_CAP2_CQE_DCMD));
	/*
	 *if (unlikely(!data_unit_num))
	 *return -EINVAL;
	 */

	memcpy(aes_key, &(host->crypto_cfgs[slot].crypto_key[0]), 32/2);
	memcpy(aes_tkey,
	&(host->crypto_cfgs[slot].crypto_key[MMC_CRYPTO_KEY_MAX_SIZE/2]),
	32/2);
	/* low layer set key: key had been set in upper layer */
	msdc_crypto_program_key(host, aes_key, aes_tkey, aes_config);

	/* switch crypto engine to MSDC */
	return set_crypto(ll_host, data_unit_num, ddir);
}

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

static void msdc_init_crypto(struct mmc_host *host)
{
	struct msdc_host *ll_host = mmc_priv(host);
	void __iomem *reg;

	if (host->caps2 & (MMC_CAP2_CQE | MMC_CAP2_CQE_DCMD)) {
	#ifdef CONFIG_MTK_EMMC_HW_CQ
		reg = ll_host->cq_host->mmio;
		host->crypto_capabilities.reg_val =
				cpu_to_le32(readl(reg + MSDC_CRCAP));
		host->crypto_cfg_register =
				host->crypto_capabilities.config_array_ptr;
		/* DCOMD will use a fixed tag of CQE,
		 * so total count should be -1
		 */
		host->crypto_capabilities.config_count--;
	#endif
	} else {
		host->crypto_capabilities.config_count = 0;
		/* in non-CQHCI, support only one */
		host->crypto_capabilities.num_crypto_cap = 1;
	}
}

static int msdc_get_crypto_capabilities(struct mmc_host *host)
{
	struct msdc_host *ll_host = mmc_priv(host);
	u8 cap_idx;

	if (!(host->caps2 & (MMC_CAP2_CQE | MMC_CAP2_CQE_DCMD)))
		goto non_cqe;

	if (unlikely(host->crypto_capabilities.num_crypto_cap != 0xA))
		return -EINVAL;

	for (cap_idx = 0; cap_idx < host->crypto_capabilities.num_crypto_cap;
	     cap_idx++) {
	#ifdef CONFIG_MTK_EMMC_HW_CQ
		host->crypto_cap_array[cap_idx].reg_val =
			cpu_to_le32(readl(ll_host->cq_host->mmio +
				MSDC_CRCAP +
				(cap_idx + 1) * sizeof(__le32)));
	#endif
	}

	return 0;
non_cqe:
	/*
	 * non-cqe has only one algorithm
	 * algorithm_id : AES-XTS (04)
	 * sdus_mask: meaningless (09)
	 * key_size: 256bits (02)
	 * reserved: 0x5A
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
