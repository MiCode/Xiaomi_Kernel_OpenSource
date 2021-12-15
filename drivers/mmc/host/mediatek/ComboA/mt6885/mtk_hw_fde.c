/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/hie.h>
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

#if defined(CONFIG_MTK_HW_FDE)
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
#else
static inline bool check_fde_mode(struct request *req,
	unsigned int *key_idx)
{
	return false;
}
#endif

static inline bool check_fbe_mode(struct request *req)
{
	if (hie_request_crypted(req))
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

static int msdc_cqhci_fbe_init(struct msdc_host *host,
	struct request *req)
{
	int err = 0;

	if (!host->is_crypto_init) {
		/* fbe init */
		mt_secure_call(MTK_SIP_KERNEL_HW_FDE_MSDC_CTL,
			(1 << 0), 4, 1, 0);
		host->is_crypto_init = true;
	}

	if (rq_data_dir(req) == WRITE)
		err = hie_encrypt(msdc_hie_get_dev(),
			req, host);
	else
		err = hie_decrypt(msdc_hie_get_dev(),
			req, host);

	if (err) {
		err = -EIO;
		ERR_MSG("%s %d: req: %p, err %d\n",
			__func__, __LINE__, req, err);
		WARN_ON(1);
	}

	return err;
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
		} else if (check_fbe_mode(req)) {
			u64 hw_hie_iv_num;

			err = msdc_cqhci_fbe_init(host, req);
			cc_idx = slot;
			c_en = true;
			hw_hie_iv_num = hie_get_iv(req);
			if (hw_hie_iv_num)
				lba = hw_hie_iv_num;
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

	hw_hie_iv_num = hie_get_iv(req);

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
	unsigned int key_idx;
	int err;
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
	else if (check_fbe_mode(req))
		is_fbe = 1;

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
		if (is_fbe) {
			if (!host->is_crypto_init) {
				/* fbe init */
				mt_secure_call(MTK_SIP_KERNEL_HW_FDE_MSDC_CTL,
					(1 << 0), 4, 1, 0);
				host->is_crypto_init = true;
			}
			if (dir == DMA_TO_DEVICE)
				err = hie_encrypt(msdc_hie_get_dev(), req,
					host);
			else
				err = hie_decrypt(msdc_hie_get_dev(), req,
					host);
			if (err) {
				err = -EIO;
				ERR_MSG(
			"%s: fail in crypto hook, req: %p, err %d\n",
					__func__, req, err);
				WARN_ON(1);
				return;
			}
			if (hie_is_nocrypt())
				return;
			if (hie_is_dummy()) {
				struct mmc_data *data = cmd->data;

				if (dir == DMA_TO_DEVICE &&
				    msdc_use_async_dma(data->host_cookie)) {
					dma_unmap_sg(mmc_dev(mmc), data->sg,
						data->sg_len, dir);
					dma_map_sg(mmc_dev(mmc), data->sg,
						data->sg_len, dir);
				}
				return;
			}
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

#ifdef CONFIG_MTK_EMMC_HW_CQ
static int _msdc_hie_cqhci_cfg(unsigned int mode, const char *key,
	int len, struct request *req, void *priv)
{
	struct msdc_host *host = (struct msdc_host *)priv;
	struct mmc_host *mmc = host->mmc;
	struct cmdq_host *cq_host = (struct cmdq_host *)mmc_cmdq_private(mmc);
	u32 addr, i, cpt_mode, cc_idx;
	union cqhci_cpt_cap cpt_cap;
	union cqhci_cap_cfg cpt_cfg;

	if (mode & BC_AES_256_XTS) {
		cpt_mode = CQHCI_CRYPTO_AES_XTS_256;
		WARN_ON(len != 64);
	} else if (mode & BC_AES_128_XTS) {
		cpt_mode = CQHCI_CRYPTO_AES_XTS_128;
		WARN_ON(len != 32);
	} else {
		ERR_MSG("%s: unknown mode 0x%x\n", __func__, mode);
		WARN_ON(1);
		return -EIO;
	}

	memset(&cpt_cfg, 0, sizeof(cpt_cfg));

	/* init key */
	if (len > 64) {
		pr_notice("Key size is over %d bits\n", len * 8);
		len = 64;
	}

	memcpy((void *)&(cpt_cfg.cfgx.key[0]), &key[0], len);

#ifdef _CQHCI_DEBUG
	for (i = 0; i < len / 4; i++) {
		pr_notice("%s: REG[%02d] = 0x%x\n", __func__, i,
			cpt_cfg.cfgx.key[i]);
	}
#endif

	/* cc idx is same as slot no. or tag no. */
	cc_idx = req->tag;

	/* enable this cfg */
	cpt_cfg.cfgx.cfg_en = 1;

	/* init capability id */
	cpt_cfg.cfgx.cap_id = (u8)cpt_mode;

	/* init data unit size: fixed as 512 * 2^0) for eMMC */
	cpt_cfg.cfgx.du_size = (1 << CQHCI_CRYPTO_DU_SIZE_512B);

	/*
	 * Get address of cfg[cfg_id], this is also
	 * address of key in cfg[cfg_id].
	 */
	cpt_cap.cap_raw = cmdq_readl(cq_host, CRCAP);
	addr = (cpt_cap.cap.cfg_ptr << 8) + (u32)(cc_idx << 7);

	/* write configuration only to register */
	for (i = 0; i < 32; i++) {
		cmdq_writel(cq_host, cpt_cfg.cfgx_raw[i],
			(addr + i * 4));
#ifdef _CQHCI_DEBUG
		pr_notice("%s: (%d)REG[0x%x]=0x%x\n",
			__func__, cc_idx,
			(addr + i * 4),
			cpt_cfg.cfgx_raw[i]);
#endif
	}

	return 0;
}
#endif
static int _msdc_hie_cfg(unsigned int mode, const char *key,
	int len, struct request *req, void *priv)
{
	struct msdc_host *host = (struct msdc_host *)priv;
	void __iomem *base = host->base;
	u32 iv[4] = {0}, aes_key[8] = {0}, aes_tkey[8] = {0};
	u32 data_unit_size, i, half_len;
	u8 key_bit, aes_mode;

	if (mode & BC_AES_256_XTS) {
		aes_mode = MSDC_CRYPTO_XTS_AES;
		key_bit = BIT_256;
		WARN_ON(len != 64);
	} else if (mode & BC_AES_128_XTS) {
		aes_mode = MSDC_CRYPTO_XTS_AES;
		key_bit = BIT_128;
		WARN_ON(len != 32);
	} else {
		ERR_MSG("%s: unknown mode 0x%x\n", __func__, mode);
		WARN_ON(1);
		return -EIO;
}

	/*
	 * limit half_len as u32 * 8
	 * prevent local buffer overflow
	 */
	half_len = min_t(u32, len / 2, sizeof(u32) * 8);

	/* Split key into key & tkey */
	memcpy(aes_key, &key[0], half_len);
	memcpy(aes_tkey, &key[half_len], half_len);

	/* eMMC block size 512bytes */
	data_unit_size = (1 << 9);

	/* AES config */
	MSDC_WRITE32(EMMC52_AES_CFG_GP1,
		(data_unit_size << 16 | key_bit << 8 | aes_mode << 0));

	/* IV */
	for (i = 0; i < 4; i++)
		MSDC_WRITE32((EMMC52_AES_IV0_GP1 + i * 4), iv[i]);

	/* KEY */
	for (i = 0; i < 8; i++)
		MSDC_WRITE32((EMMC52_AES_KEY0_GP1 + i * 4), aes_key[i]);

	/* TKEY */
	for (i = 0; i < 8; i++)
		MSDC_WRITE32((EMMC52_AES_TKEY0_GP1 + i * 4), aes_tkey[i]);

	return 0;
}

/* configure request for HIE */
static int msdc_hie_cfg_request(unsigned int mode, const char *key,
	int len, struct request *req, void *priv)
{
#ifdef CONFIG_MTK_EMMC_HW_CQ
	struct msdc_host *host = (struct msdc_host *)priv;

	if (mmc_card_cmdq(host->mmc->card))
		_msdc_hie_cqhci_cfg(mode, key, len, req, priv);
	else
#endif
		_msdc_hie_cfg(mode, key, len, req, priv);

	return 0;
}

struct hie_dev msdc_hie_dev = {
	.name = "msdc",
	.mode = (BC_AES_256_XTS | BC_AES_128_XTS),
	.encrypt = msdc_hie_cfg_request,
	.decrypt = msdc_hie_cfg_request,
	.priv = NULL,
};

struct hie_dev *msdc_hie_get_dev(void)
{
	return &msdc_hie_dev;
}

static void msdc_hie_register(struct msdc_host *host)
{
	if (host->hw->host_function == MSDC_EMMC)
		hie_register_device(&msdc_hie_dev);
}
