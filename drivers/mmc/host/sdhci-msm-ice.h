/*
 * Copyright (c) 2015, 2017, The Linux Foundation. All rights reserved.
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

#ifndef __SDHCI_MSM_ICE_H__
#define __SDHCI_MSM_ICE_H__

#include <linux/io.h>
#include <linux/of.h>
#include <linux/blkdev.h>
#include <crypto/ice.h>

#include "sdhci-msm.h"

#define SDHC_MSM_CRYPTO_LABEL "sdhc-msm-crypto"
/* Timeout waiting for ICE initialization, that requires TZ access */
#define SDHCI_MSM_ICE_COMPLETION_TIMEOUT_MS	500

/*
 * SDHCI host controller ICE registers. There are n [0..31]
 * of each of these registers
 */
#define NUM_SDHCI_MSM_ICE_CTRL_INFO_n_REGS	32

#define CORE_VENDOR_SPEC_ICE_CTRL		0x300
#define CORE_VENDOR_SPEC_ICE_CTRL_INFO_1_n	0x304
#define CORE_VENDOR_SPEC_ICE_CTRL_INFO_2_n	0x308
#define CORE_VENDOR_SPEC_ICE_CTRL_INFO_3_n	0x30C

/* ICE3.0 register which got added cmdq reg space */
#define ICE_CQ_CAPABILITIES	0x04
#define ICE_HCI_SUPPORT		(1 << 28)
#define ICE_CQ_CONFIG		0x08
#define CRYPTO_GENERAL_ENABLE	(1 << 1)
#define ICE_NONCQ_CRYPTO_PARAMS	0x70
#define ICE_NONCQ_CRYPTO_DUN	0x74

/* ICE3.0 register which got added hc reg space */
#define HC_VENDOR_SPECIFIC_FUNC4	0x260
#define DISABLE_CRYPTO			(1 << 15)
#define HC_VENDOR_SPECIFIC_ICE_CTRL	0x800
#define ICE_SW_RST_EN			(1 << 0)

/* SDHCI MSM ICE CTRL Info register offset */
enum {
	OFFSET_SDHCI_MSM_ICE_CTRL_INFO_BYPASS     = 0,
	OFFSET_SDHCI_MSM_ICE_CTRL_INFO_KEY_INDEX  = 1,
	OFFSET_SDHCI_MSM_ICE_CTRL_INFO_CDU        = 6,
	OFFSET_SDHCI_MSM_ICE_HCI_PARAM_CCI	  = 0,
	OFFSET_SDHCI_MSM_ICE_HCI_PARAM_CE	  = 8,
};

/* SDHCI MSM ICE CTRL Info register masks */
enum {
	MASK_SDHCI_MSM_ICE_CTRL_INFO_BYPASS     = 0x1,
	MASK_SDHCI_MSM_ICE_CTRL_INFO_KEY_INDEX  = 0x1F,
	MASK_SDHCI_MSM_ICE_CTRL_INFO_CDU        = 0x7,
	MASK_SDHCI_MSM_ICE_HCI_PARAM_CE		= 0x1,
	MASK_SDHCI_MSM_ICE_HCI_PARAM_CCI	= 0xff
};

/* SDHCI MSM ICE encryption/decryption bypass state */
enum {
	SDHCI_MSM_ICE_DISABLE_BYPASS  = 0,
	SDHCI_MSM_ICE_ENABLE_BYPASS = 1,
};

/* SDHCI MSM ICE Crypto Data Unit of target DUN of Transfer Request */
enum {
	SDHCI_MSM_ICE_TR_DATA_UNIT_512_B          = 0,
	SDHCI_MSM_ICE_TR_DATA_UNIT_1_KB           = 1,
	SDHCI_MSM_ICE_TR_DATA_UNIT_2_KB           = 2,
	SDHCI_MSM_ICE_TR_DATA_UNIT_4_KB           = 3,
	SDHCI_MSM_ICE_TR_DATA_UNIT_8_KB           = 4,
	SDHCI_MSM_ICE_TR_DATA_UNIT_16_KB          = 5,
	SDHCI_MSM_ICE_TR_DATA_UNIT_32_KB          = 6,
	SDHCI_MSM_ICE_TR_DATA_UNIT_64_KB          = 7,
};

/* SDHCI MSM ICE internal state */
enum {
	SDHCI_MSM_ICE_STATE_DISABLED   = 0,
	SDHCI_MSM_ICE_STATE_ACTIVE     = 1,
	SDHCI_MSM_ICE_STATE_SUSPENDED  = 2,
};

/* crypto context fields in cmdq data command task descriptor */
#define DATA_UNIT_NUM(x)	(((u64)(x) & 0xFFFFFFFF) << 0)
#define CRYPTO_CONFIG_INDEX(x)	(((u64)(x) & 0xFF) << 32)
#define CRYPTO_ENABLE(x)	(((u64)(x) & 0x1) << 47)

#ifdef CONFIG_MMC_SDHCI_MSM_ICE
int sdhci_msm_ice_get_dev(struct sdhci_host *host);
int sdhci_msm_ice_init(struct sdhci_host *host);
void sdhci_msm_ice_cfg_reset(struct sdhci_host *host, u32 slot);
int sdhci_msm_ice_cfg(struct sdhci_host *host, struct mmc_request *mrq,
			u32 slot);
int sdhci_msm_ice_cmdq_cfg(struct sdhci_host *host,
			struct mmc_request *mrq, u32 slot, u64 *ice_ctx);
int sdhci_msm_ice_reset(struct sdhci_host *host);
int sdhci_msm_ice_resume(struct sdhci_host *host);
int sdhci_msm_ice_suspend(struct sdhci_host *host);
int sdhci_msm_ice_get_status(struct sdhci_host *host, int *ice_status);
void sdhci_msm_ice_print_regs(struct sdhci_host *host);
#else
inline int sdhci_msm_ice_get_dev(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;

	if (msm_host) {
		msm_host->ice.pdev = NULL;
		msm_host->ice.vops = NULL;
	}
	return -ENODEV;
}
inline int sdhci_msm_ice_init(struct sdhci_host *host)
{
	return 0;
}

inline void sdhci_msm_ice_cfg_reset(struct sdhci_host *host, u32 slot)
{
}

inline int sdhci_msm_ice_cfg(struct sdhci_host *host,
		struct mmc_request *mrq, u32 slot)
{
	return 0;
}
inline int sdhci_msm_ice_cmdq_cfg(struct sdhci_host *host,
		struct mmc_request *mrq, u32 slot, u64 *ice_ctx)
{
	return 0;
}
inline int sdhci_msm_ice_reset(struct sdhci_host *host)
{
	return 0;
}
inline int sdhci_msm_ice_resume(struct sdhci_host *host)
{
	return 0;
}
inline int sdhci_msm_ice_suspend(struct sdhci_host *host)
{
	return 0;
}
inline int sdhci_msm_ice_get_status(struct sdhci_host *host,
				   int *ice_status)
{
	return 0;
}
inline void sdhci_msm_ice_print_regs(struct sdhci_host *host)
{
	return;
}
#endif /* CONFIG_MMC_SDHCI_MSM_ICE */
#endif /* __SDHCI_MSM_ICE_H__ */
