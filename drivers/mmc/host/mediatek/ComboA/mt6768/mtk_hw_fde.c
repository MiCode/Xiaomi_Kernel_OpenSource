/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "mtk_secure_api.h"
#include <mmc/core/queue.h>

/* map from AES Spec */
enum {
	MSDC_CRYPTO_XTS_AES         = 4,
	MSDC_CRYPTO_AES_CBC_ESSIV   = 9,
	MSDC_CRYPTO_BITLOCKER       = 17,
	MSDC_CRYPTO_AES_ECB       = 0,
	MSDC_CRYPTO_AES_CBC       = 1,
	MSDC_CRYPTO_AES_CTR       = 2,
	MSDC_CRYPTO_AES_CBC_MAC   = 3,
} aes_mode;

enum {
	BIT_128 = 0,
	BIT_192 = 1,
	BIT_256 = 2,
	BIT_0 = 4,
};

static inline bool check_fde_mode(struct request *req,
	unsigned int *key_idx)
{
	if (req->bio && req->bio->bi_hw_fde) {
		if (key_idx)
			*key_idx = req->bio->bi_key_idx;
		return true;
	}

	return false;
}

static inline bool check_fbe_mode(struct request *req)
{
  /* TODO: Use inline-crypt method here */
	if (0)
		return true;

	return false;
}
static void msdc_enable_crypto(struct msdc_host *host)
{
	void __iomem *base = host->base;
	/* enable AES path by clr bypass bit */
	MSDC_CLR_BIT32(EMMC52_AES_SWST, EMMC52_AES_BYPASS);
}

static void msdc_disable_crypto(struct msdc_host *host)
{
	void __iomem *base = host->base;
	/* disable AES path by set bypass bit */
	MSDC_SET_BIT32(EMMC52_AES_SWST, EMMC52_AES_BYPASS);
}

#ifdef CONFIG_MTK_EMMC_HW_CQ
/* HCI crypto start */

/* crypto context fields in cmdq data command task descriptor */
#define DATA_UNIT_NUM(x)	(((u64)(x) & 0xFFFFFFFF) << 0)
#define CRYPTO_CONFIG_INDEX(x)	(((u64)(x) & 0xFF) << 32)
#define CRYPTO_ENABLE(x)	(((u64)(x) & 0x1) << 47)

union cqhci_cpt_capx {
	u32 capx_raw;
	struct {
		u8 alg_id;
		u8 du_size;
		u8 key_size;
		u8 resv;
	} capx;
};

/* multi_host/resv1/vsb/resv2 don't use,
 *just for use the same structure as UFS
 */
union cqhci_cap_cfg {
	u32 cfgx_raw[32];
	struct {
		u32 key[16];
		u8 du_size;
		u8 cap_id;
		u16 resv0:15;
		u16 cfg_en:1;
		u8 mu1ti_host;
		u8 resv1;
		u16 vsb;
		u32 resv2[14];
	} cfgx;
};

struct cqhci_crypto {
	u32 cfg_id;
	u32 cap_id;
	union cqhci_cpt_cap cap;
	union cqhci_cpt_capx capx;
	union cqhci_cap_cfg cfg;
};

/* capability id */
enum {
	CQHCI_CRYPTO_AES_XTS_128 = 0,
	CQHCI_CRYPTO_AES_XTS_256 = 1,
	CQHCI_CRYPTO_BITLOCKER_AES_CBC_128 = 2,
	CQHCI_CRYPTO_BITLOCKER_AES_CBC_256 = 3,
	CQHCI_CRYPTO_AES_ECB_128 = 4,
	CQHCI_CRYPTO_AES_ECB_192 = 5,
	CQHCI_CRYPTO_AES_ECB_256 = 6,
	CQHCI_CRYPTO_ESSIV_AES_CBC_128 = 7,
	CQHCI_CRYPTO_ESSIV_AES_CBC_192 = 8,
	CQHCI_CRYPTO_ESSIV_AES_CBC_256 = 9,
};

/* data unit size */
enum {
	CQHCI_CRYPTO_DU_SIZE_512B = 0,
};

static int msdc_cqhci_fde_init(struct msdc_host *host,
	unsigned int key_idx)
{
	if ((!host->is_crypto_init ||
		(host->key_idx != key_idx))) {
		/* cqhci fde init */
		mt_secure_call(MTK_SIP_KERNEL_HW_FDE_MSDC_CTL,
			(1 << 5), 4, 1, 0);
		host->is_crypto_init = true;
		host->key_idx = key_idx;
	}

	return 0;
}

static int msdc_cqhci_crypto_cfg(struct mmc_host *mmc,
		struct mmc_request *mrq, u32 slot, u64 *ctx)
{
	struct msdc_host *host = mmc_priv(mmc);
	sector_t lba = 0;
	unsigned int key_idx;
	int err = 0, cc_idx = slot;
	bool c_en = false;
	struct request *req;

	if (WARN_ON(!mrq))
		return -EINVAL;

	req = mrq->req;

	if (req) {
		lba = blk_rq_pos(req);

		if (check_fde_mode(req, &key_idx)) {
			err = msdc_cqhci_fde_init(host, key_idx);
			/* use slot 0 for fde mode */
			cc_idx = 0;
			c_en = true;
		}
	}

	if (ctx)
		*ctx = DATA_UNIT_NUM(lba) |
			CRYPTO_CONFIG_INDEX(cc_idx) |
			CRYPTO_ENABLE(!!c_en);

	return err;
}
#endif
static void msdc_crypto_switch_config(struct msdc_host *host,
	struct request *req,
	u32 block_address, u32 dir)
{
	void __iomem *base = host->base;
	u32 aes_mode_current = 0, aes_sw_reg = 0;
	u32 ctr[4] = {0};
	unsigned long polling_tmo = 0;
	u64 hw_hie_iv_num = 0;

	/* 1. set ctr */
	aes_sw_reg = MSDC_READ32(EMMC52_AES_EN);

	hw_hie_iv_num = 0; //hie_get_iv(req); /*need to check*/

	if (aes_sw_reg & EMMC52_AES_SWITCH_VALID0)
		MSDC_GET_FIELD(EMMC52_AES_CFG_GP0,
			EMMC52_AES_MODE_0, aes_mode_current);
	else if (aes_sw_reg & EMMC52_AES_SWITCH_VALID1)
		MSDC_GET_FIELD(EMMC52_AES_CFG_GP1,
			EMMC52_AES_MODE_1, aes_mode_current);
	else {
		pr_info("msdc: EMMC52_AES_SWITCH_VALID error\n");
		WARN_ON(1);
		return;
	}

	switch (aes_mode_current) {
	case MSDC_CRYPTO_XTS_AES:
	case MSDC_CRYPTO_AES_CBC_ESSIV:
	case MSDC_CRYPTO_BITLOCKER:
	{
		if (hw_hie_iv_num) {
			ctr[0] = hw_hie_iv_num & 0xffffffff;
#ifndef CONFIG_MTK_EMMC_HW_CQ
			ctr[1] = (hw_hie_iv_num >> 32) & 0xffffffff;
#endif
		} else {
			ctr[0] = block_address;
		}
		break;
	}
	case MSDC_CRYPTO_AES_ECB:
	case MSDC_CRYPTO_AES_CBC:
		break;
	case MSDC_CRYPTO_AES_CTR:
	{
		if (hw_hie_iv_num) {
			ctr[0] = hw_hie_iv_num & 0xffffffff;
			ctr[1] = (hw_hie_iv_num >> 32) & 0xffffffff;
		} else {
			ctr[0] = block_address;
		}
		break;
	}
	case MSDC_CRYPTO_AES_CBC_MAC:
		break;
	default:
		pr_info("msdc unknown aes mode\n");
		WARN_ON(1);
		return;
	}

	if (aes_sw_reg & EMMC52_AES_SWITCH_VALID0) {
		MSDC_WRITE32(EMMC52_AES_CTR0_GP0, ctr[0]);
		MSDC_WRITE32(EMMC52_AES_CTR1_GP0, ctr[1]);
		MSDC_WRITE32(EMMC52_AES_CTR2_GP0, ctr[2]);
		MSDC_WRITE32(EMMC52_AES_CTR3_GP0, ctr[3]);
	} else {
		MSDC_WRITE32(EMMC52_AES_CTR0_GP1, ctr[0]);
		MSDC_WRITE32(EMMC52_AES_CTR1_GP1, ctr[1]);
		MSDC_WRITE32(EMMC52_AES_CTR2_GP1, ctr[2]);
		MSDC_WRITE32(EMMC52_AES_CTR3_GP1, ctr[3]);
	}

	/* 2. enable AES path */
	msdc_enable_crypto(host);

	/* 3. AES switch start (flush the configure) */
	if (dir == DMA_TO_DEVICE) {
		MSDC_SET_BIT32(EMMC52_AES_SWST,
			EMMC52_AES_SWITCH_START_ENC);
		polling_tmo = jiffies + POLLING_BUSY;
		while (MSDC_READ32(EMMC52_AES_SWST) &
			EMMC52_AES_SWITCH_START_ENC) {
			if (time_after(jiffies, polling_tmo)) {
				pr_info("msdc%d trigger AES ENC tmo!\n",
					host->id);
				WARN_ON(1);
			}
		}
	} else {
		MSDC_SET_BIT32(EMMC52_AES_SWST,
			EMMC52_AES_SWITCH_START_DEC);
		polling_tmo = jiffies + POLLING_BUSY;
		while (MSDC_READ32(EMMC52_AES_SWST) &
			EMMC52_AES_SWITCH_START_DEC) {
			if (time_after(jiffies, polling_tmo)) {
				pr_info("msdc%d trigger AES DEC tmo!\n",
					host->id);
				WARN_ON(1);
			}
		}
	}
}

static void msdc_pre_crypto(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct mmc_command *cmd = mrq->cmd;
	u32 dir = DMA_FROM_DEVICE;
	u32 blk_addr = 0;
	u32 is_fde = 0, is_fbe = 0;
#if defined(CONFIG_MTK_HW_FDE)
	unsigned int key_idx;
#endif
	struct mmc_blk_request *brq;
	struct mmc_queue_req *mq_rq = NULL;
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	struct mmc_async_req *areq;
#endif
	struct request *req = NULL;


	if (!host->hw || !mmc->card)
		return;

	if (host->hw->host_function != MSDC_EMMC)
		return;

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	/* CMDQ Command */
	if (check_mmc_cmd4647(cmd->opcode)) {
		areq = mmc->areq_que[(cmd->arg >> 16) & 0x1f];
		mq_rq = container_of(areq, struct mmc_queue_req, areq);
		blk_addr = mq_rq->brq.que.arg;
		req = mmc_queue_req_to_req(mq_rq);
		goto check_hw_crypto;
	}
#endif

	/* Normal Read Write Command */
	if (mrq->is_mmc_req &&
		(check_mmc_cmd1718(cmd->opcode) ||
		check_mmc_cmd2425(cmd->opcode))) {
		brq = container_of(mrq, struct mmc_blk_request, mrq);
		mq_rq = container_of(brq, struct mmc_queue_req, brq);
		blk_addr = cmd->arg;
		req = mmc_queue_req_to_req(mq_rq);
		goto check_hw_crypto;
	}

	return;

check_hw_crypto:
	dir = cmd->data->flags & MMC_DATA_READ ?
		DMA_FROM_DEVICE : DMA_TO_DEVICE;

	if (check_fde_mode(req, &key_idx))
		is_fde = 1;

	if (is_fde || is_fbe) {
		if (is_fde &&
			(!host->is_crypto_init ||
			(host->key_idx != key_idx))) {
			/* fde init */
			mt_secure_call(MTK_SIP_KERNEL_HW_FDE_MSDC_CTL,
				(1 << 3), 4, 1, 0);
			host->is_crypto_init = true;
			host->key_idx = key_idx;
		}
		if (!mmc_card_blockaddr(mmc->card))
			blk_addr = blk_addr >> 9;

		/* Check data size with 16bytes */
		WARN_ON(host->dma.xfersz & 0xf);
		/* Check data addressw with 16bytes alignment */
		WARN_ON((host->dma.gpd_addr & 0xf)
			|| (host->dma.bd_addr & 0xf));
		msdc_crypto_switch_config(host, req, blk_addr, dir);
	}
}

static void msdc_post_crypto(struct msdc_host *host)
{
	msdc_disable_crypto(host);
}
