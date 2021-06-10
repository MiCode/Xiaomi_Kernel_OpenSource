/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef _MSDC_CRYPTO_H
#define _MSDC_CRYPTO_H

/* Crypto */
#define PERI_FDI_AES_SI_CTRL 0x448
#define MSDC_AES_EN       0x600
#define MSDC_AES_SWST     0x670
#define MSDC_AES_CFG_GP1  0x674
#define MSDC_AES_KEY_GP1  0x6A0
#define MSDC_AES_TKEY_GP1 0x6C0
#define MSDC_AES_IV0_GP1  0x680
#define MSDC_AES_CTR0_GP1 0x690
#define MSDC_AES_CTR1_GP1 0x694
#define MSDC_AES_CTR2_GP1 0x698
#define MSDC_AES_CTR3_GP1 0x69C
/* Crypto CQE */
#define MSDC_CRCAP        0x100
/* Crypto context fields in CQHCI data command task descriptor */
#define DATA_UNIT_NUM(x)	    (((u64)(x) & 0xFFFFFFFF) << 0)
#define CRYPTO_CONFIG_INDEX(x)	(((u64)(x) & 0xFF) << 32)
#define CRYPTO_ENABLE(x)	    (((u64)(x) & 0x1) << 47)

/* Crypto ATF */
#define MTK_SIP_KERNEL_HW_FDE_MSDC_CTL_AARCH32 (0x82000273 | 0x00000000)
#define MTK_SIP_KERNEL_HW_FDE_MSDC_CTL_AARCH64 (0xC2000273 | 0x40000000)

/* CQE */
#define CQ_TASK_DESC_TASK_PARAMS_SIZE 8
#define CQ_TASK_DESC_CE_PARAMS_SIZE 8


/*--------------------------------------------------------------------------*/
/* Register Mask                                                            */
/*--------------------------------------------------------------------------*/
/* Crypto */
#define PERI_AES_CTRL_MSDC0_EN    (4)          /* RW */
#define MSDC_AES_MODE_1           (0x1F << 0)  /* RW */
#define MSDC_AES_BYPASS           (1 << 2)     /* RW */
#define MSDC_AES_SWITCH_START_ENC (1 << 0)     /* RW */
#define MSDC_AES_SWITCH_START_DEC (1 << 1)     /* RW */
#define MSDC_AES_ON               (0x1 << 0)   /* RW */
#define MSDC_AES_SWITCH_VALID0    (0x1 << 1)   /* RW */
#define MSDC_AES_SWITCH_VALID1    (0x1 << 2)   /* RW */
#define MSDC_AES_CLK_DIV_SEL      (0x7 << 4)   /* RW */


/*--------------------------------------------------------------------------*/
/* Crypto CQE                                                    */
/*--------------------------------------------------------------------------*/
union cqhci_cpt_cap {
	u32 cap_raw;
	struct {
		u8 cap_cnt;
		u8 cfg_cnt;
		u8 resv;
		u8 cfg_ptr;
	} cap;
};

/*--------------------------------------------------------------------------*/
/* enum                                                    */
/*--------------------------------------------------------------------------*/
enum msdc_crypto_alg {
	MSDC_CRYPTO_ALG_BITLOCKER_AES_CBC	= 1,
	MSDC_CRYPTO_ALG_AES_ECB				= 2,
	MSDC_CRYPTO_ALG_ESSIV_AES_CBC		= 3,
	MSDC_CRYPTO_ALG_AES_XTS				= 4,
};

#ifdef CONFIG_MMC_CRYPTO
void msdc_crypto_init_vops(struct mmc_host *host);
#else
static inline void msdc_crypto_init_vops(struct mmc_host *host) { return ; }
#endif

#endif /* _MSDC_CRYPTO_H */
