/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

#ifndef __GPUFREQ_REG_MT6835_H__
#define __GPUFREQ_REG_MT6835_H__

#include <linux/io.h>
#include <linux/bits.h>

/**************************************************
 * GPUFREQ Register Definition
 **************************************************/
#define MALI_BASE                       (g_mali_base)                         /* 0x13000000 */
#define MALI_GPU_ID                     (MALI_BASE + 0x000)                   /* 0x13000000 */

#define MFG_TOP_CFG_BASE                (g_mfg_top_base)                      /* 0x13FBF000 */
#define MFG_CG_CON                      (MFG_TOP_CFG_BASE + 0x000)            /* 0x13FBF000 */
#define MFG_ASYNC_CON                   (MFG_TOP_CFG_BASE + 0x020)            /* 0x13FBF020 */
#define MFG_ASYNC_CON_1                 (MFG_TOP_CFG_BASE + 0x024)            /* 0x13FBF024 */
#define MFG_GLOBAL_CON                  (MFG_TOP_CFG_BASE + 0x0B0)            /* 0x13FBF0B0 */
#define MFG_QCHANNEL_CON                (MFG_TOP_CFG_BASE + 0x0B4)            /* 0x13FBF0B4 */
#define MFG_DEBUG_SEL                   (MFG_TOP_CFG_BASE + 0x170)            /* 0x13FBF170 */
#define MFG_DEBUG_TOP                   (MFG_TOP_CFG_BASE + 0x178)            /* 0x13FBF178 */
#define MFG_TIMESTAMP                   (MFG_TOP_CFG_BASE + 0x130)            /* 0x13FBF130 */
#define MFG_I2M_PROTECTOR_CFG_00        (MFG_TOP_CFG_BASE + 0xF60)            /* 0x13FBFF60 */
#define MFG_MERGE_R_CON_00              (MFG_TOP_CFG_BASE + 0x8A0)            /* 0x13FBF8A0 */
#define MFG_MERGE_R_CON_02              (MFG_TOP_CFG_BASE + 0x8A8)            /* 0x13FBF8A8 */
#define MFG_MERGE_R_CON_04              (MFG_TOP_CFG_BASE + 0x8C0)            /* 0x13FBF8C0 */
#define MFG_MERGE_R_CON_06              (MFG_TOP_CFG_BASE + 0x8C8)            /* 0x13FBF8C8 */
#define MFG_MERGE_W_CON_00              (MFG_TOP_CFG_BASE + 0x8B0)            /* 0x13FBF8B0 */
#define MFG_MERGE_W_CON_02              (MFG_TOP_CFG_BASE + 0x8B8)            /* 0x13FBF8B8 */
#define MFG_MERGE_W_CON_04              (MFG_TOP_CFG_BASE + 0x8D0)            /* 0x13FBF8D0 */
#define MFG_MERGE_W_CON_06              (MFG_TOP_CFG_BASE + 0x8D8)            /* 0x13FBF8D8 */

#define APMIXED_BASE                    (g_apmixed_base)                      /* 0x1000C000 */
#define MFG_PLL_CON0                    (APMIXED_BASE + 0x3C0)                /* 0x1000C3C0 */
#define MFG_PLL_CON1                    (APMIXED_BASE + 0x3C4)                /* 0x1000C3C4 */

#define SPM_BASE                        (g_sleep)                             /* 0x1C001000 */
#define MFG0_PWR_CON                    (SPM_BASE + 0xEB8)                    /* 0x1C001EB8 */
#define MFG1_PWR_CON                    (SPM_BASE + 0xEBC)                    /* 0x1C001EBC */
#define MFG2_PWR_CON                    (SPM_BASE + 0xEC0)                    /* 0x1C001EC0 */
#define MFG3_PWR_CON                    (SPM_BASE + 0xEC4)                    /* 0x1C001EC4 */
#define XPU_PWR_STATUS                  (SPM_BASE + 0xF4C)                    /* 0x1C001F4C */
#define XPU_PWR_STATUS_2ND              (SPM_BASE + 0xF50)                    /* 0x1C001F50 */

#define TOPCKGEN_BASE                   (g_topckgen_base)                     /* 0x10000000 */
#define TOPCK_CLK_CFG_13                (TOPCKGEN_BASE + 0x0E0)               /* 0x100000E0 */
#define TOPCK_CLK_CFG_20                (TOPCKGEN_BASE + 0x120)               /* 0x10000120 */

#define NTH_EMICFG_BASE                 (g_nth_emicfg_base)                   /* 0x1021C000 */
#define NTH_MFG_EMI1_GALS_SLV_DBG       (NTH_EMICFG_BASE + 0x82C)             /* 0x1021C82C */
#define NTH_MFG_EMI0_GALS_SLV_DBG       (NTH_EMICFG_BASE + 0x830)             /* 0x1021C830 */

#define STH_EMICFG_BASE                 (g_sth_emicfg_base)                   /* 0x1021E000 */
#define STH_MFG_EMI1_GALS_SLV_DBG       (STH_EMICFG_BASE + 0x82C)             /* 0x1021E82C */
#define STH_MFG_EMI0_GALS_SLV_DBG       (STH_EMICFG_BASE + 0x830)             /* 0x1021E830 */

#define NTH_EMICFG_AO_MEM_BASE          (g_nth_emicfg_ao_mem_base)            /* 0x10270000 */
#define NTH_M6M7_IDLE_BIT_EN_1          (NTH_EMICFG_AO_MEM_BASE + 0x228)      /* 0x10270228 */
#define NTH_M6M7_IDLE_BIT_EN_0          (NTH_EMICFG_AO_MEM_BASE + 0x22C)      /* 0x1027022C */

#define STH_EMICFG_AO_MEM_BASE          (g_sth_emicfg_ao_mem_base)            /* 0x1030E000 */
#define STH_M6M7_IDLE_BIT_EN_1          (STH_EMICFG_AO_MEM_BASE + 0x228)      /* 0x1030E228 */
#define STH_M6M7_IDLE_BIT_EN_0          (STH_EMICFG_AO_MEM_BASE + 0x22C)      /* 0x1030E22C */

#define IFNFRA_AO_BASE                  (g_infracfg_ao_base)                  /* 0x10001000 */
#define INFRASYS_PROTECT_EN_STA_0       (IFNFRA_AO_BASE + 0xC40)              /* 0x10001C40 */
#define INFRASYS_PROTECT_EN_SET_0       (IFNFRA_AO_BASE + 0xC44)              /* 0x10001C44 */
#define INFRASYS_PROTECT_EN_CLR_0       (IFNFRA_AO_BASE + 0xC48)              /* 0x10001C48 */
#define INFRASYS_PROTECT_RDY_STA_0      (IFNFRA_AO_BASE + 0xC4C)              /* 0x10001C4C */
#define INFRASYS_PROTECT_EN_STA_1       (IFNFRA_AO_BASE + 0xC50)              /* 0x10001C50 */
#define INFRASYS_PROTECT_EN_SET_1       (IFNFRA_AO_BASE + 0xC54)              /* 0x10001C54 */
#define INFRASYS_PROTECT_EN_CLR_1       (IFNFRA_AO_BASE + 0xC58)              /* 0x10001C58 */
#define INFRASYS_PROTECT_RDY_STA_1      (IFNFRA_AO_BASE + 0xC5C)              /* 0x10001C5C */
#define EMISYS_PROTECT_EN_STA_0         (IFNFRA_AO_BASE + 0xC60)              /* 0x10001C60 */
#define EMISYS_PROTECT_EN_SET_0         (IFNFRA_AO_BASE + 0xC64)              /* 0x10001C64 */
#define EMISYS_PROTECT_EN_CLR_0         (IFNFRA_AO_BASE + 0xC68)              /* 0x10001C68 */
#define EMISYS_PROTECT_RDY_STA_0        (IFNFRA_AO_BASE + 0xC6C)              /* 0x10001C6C */
#define MD_MFGSYS_PROTECT_EN_STA_0      (IFNFRA_AO_BASE + 0xCA0)              /* 0x10001CA0 */
#define MD_MFGSYS_PROTECT_EN_SET_0      (IFNFRA_AO_BASE + 0xCA4)              /* 0x10001CA4 */
#define MD_MFGSYS_PROTECT_EN_CLR_0      (IFNFRA_AO_BASE + 0xCA8)              /* 0x10001CA8 */
#define MD_MFGSYS_PROTECT_RDY_STA_0     (IFNFRA_AO_BASE + 0xCAC)              /* 0x10001CAC */

#define INFRA_AO_DEBUG_CTRL_BASE        (g_infra_ao_debug_ctrl)               /* 0x10023000 */
#define INFRA_AO_BUS0_U_DEBUG_CTRL0     (INFRA_AO_DEBUG_CTRL_BASE + 0x000)    /* 0x10023000 */

#define INFRA_AO1_DEBUG_CTRL_BASE       (g_infra_ao1_debug_ctrl)              /* 0x1002B000 */
#define INFRA_AO1_BUS1_U_DEBUG_CTRL0    (INFRA_AO1_DEBUG_CTRL_BASE + 0x000)   /* 0x1002B000 */

#define NTH_EMI_AO_DEBUG_CTRL_BASE      (g_nth_emi_ao_debug_ctrl)             /* 0x10042000 */
#define NTH_EMI_AO_DEBUG_CTRL0          (NTH_EMI_AO_DEBUG_CTRL_BASE + 0x000)  /* 0x10042000 */

#define STH_EMI_AO_DEBUG_CTRL_BASE      (g_sth_emi_ao_debug_ctrl)             /* 0x10028000 */
#define STH_EMI_AO_DEBUG_CTRL0          (STH_EMI_AO_DEBUG_CTRL_BASE + 0x000)  /* 0x10028000 */

#define EFUSE_BASE                      (g_efuse_base)                        /* 0x11C10000 */
#define EFUSE_PTPOD20_AVS               (EFUSE_BASE + 0x5D0)                  /* 0x11C105D0 */
#define EFUSE_PTPOD21_AVS               (EFUSE_BASE + 0x5D4)                  /* 0x11C105D4 */
#define EFUSE_PTPOD22_AVS               (EFUSE_BASE + 0x5D8)                  /* 0x11C105D8 */

/**************************************************
 * MFGSYS Register Info
 **************************************************/
enum gpufreq_reg_info_idx {
	IDX_MFG_CG_CON                   = 0,
	IDX_MFG_ASYNC_CON                = 1,
	IDX_MFG_ASYNC_CON_1              = 2,
	IDX_MFG_GLOBAL_CON               = 3,
	IDX_MFG_QCHANNEL_CON             = 4,
	IDX_MFG_I2M_PROTECTOR_CFG_00     = 5,
	IDX_MFG_PLL_CON0                 = 6,
	IDX_MFG_PLL_CON1                 = 7,
	IDX_MFG_MFG0_PWR_CON             = 8,
	IDX_MFG_MFG1_PWR_CON             = 9,
	IDX_MFG_MFG2_PWR_CON             = 10,
	IDX_MFG_MFG3_PWR_CON             = 11,
	IDX_TOPCK_CLK_CFG_13             = 12,
	IDX_TOPCK_CLK_CFG_20             = 13,
	IDX_NTH_MFG_EMI1_GALS_SLV_DBG    = 14,
	IDX_NTH_MFG_EMI0_GALS_SLV_DBG    = 15,
	IDX_STH_MFG_EMI1_GALS_SLV_DBG    = 16,
	IDX_STH_MFG_EMI0_GALS_SLV_DBG    = 17,
	IDX_NTH_M6M7_IDLE_BIT_EN_1       = 18,
	IDX_NTH_M6M7_IDLE_BIT_EN_0       = 19,
	IDX_STH_M6M7_IDLE_BIT_EN_1       = 20,
	IDX_STH_M6M7_IDLE_BIT_EN_0       = 21,
	IDX_INFRASYS_PROTECT_EN_STA_0    = 22,
	IDX_INFRASYS_PROTECT_RDY_STA_0   = 23,
	IDX_INFRASYS_PROTECT_EN_STA_1    = 24,
	IDX_INFRASYS_PROTECT_RDY_STA_1   = 25,
	IDX_EMISYS_PROTECT_EN_STA_0      = 26,
	IDX_EMISYS_PROTECT_RDY_STA_0     = 27,
	IDX_MD_MFGSYS_PROTECT_EN_STA_0   = 28,
	IDX_MD_MFGSYS_PROTECT_RDY_STA_0  = 29,
	IDX_INFRA_AO_BUS0_U_DEBUG_CTRL0  = 30,
	IDX_INFRA_AO1_BUS1_U_DEBUG_CTRL0 = 31,
	IDX_NTH_EMI_AO_DEBUG_CTRL0       = 32,
	IDX_STH_EMI_AO_DEBUG_CTRL0       = 33,
	IDX_EFUSE_PTPOD20_AVS            = 34,
	IDX_EFUSE_PTPOD21_AVS            = 35,
	IDX_EFUSE_PTPOD22_AVS            = 36,
};

#define NUM_MFGSYS_REG                  ARRAY_SIZE(g_reg_mfgsys)
static struct gpufreq_reg_info g_reg_mfgsys[] = {
	REGOP(0x13FBF000, 0), /* 0 */
	REGOP(0x13FBF020, 0), /* 1 */
	REGOP(0x13FBF024, 0), /* 2 */
	REGOP(0x13FBF0B0, 0), /* 3 */
	REGOP(0x13FBF0B4, 0), /* 4 */
	REGOP(0x13FBFF60, 0), /* 5 */
	REGOP(0x1000C3C0, 0), /* 6 */
	REGOP(0x1000C3C4, 0), /* 7 */
	REGOP(0x1C001EB8, 0), /* 8 */
	REGOP(0x1C001EBC, 0), /* 9 */
	REGOP(0x1C001EC0, 0), /* 10 */
	REGOP(0x1C001EC4, 0), /* 11 */
	REGOP(0x100000E0, 0), /* 12 */
	REGOP(0x10000120, 0), /* 13 */
	REGOP(0x1021C82C, 0), /* 14 */
	REGOP(0x1021C830, 0), /* 15 */
	REGOP(0x1021E82C, 0), /* 16 */
	REGOP(0x1021E830, 0), /* 17 */
	REGOP(0x10270228, 0), /* 18 */
	REGOP(0x1027022C, 0), /* 19 */
	REGOP(0x1030E228, 0), /* 20 */
	REGOP(0x1030E22C, 0), /* 21 */
	REGOP(0x10001C40, 0), /* 22 */
	REGOP(0x10001C4C, 0), /* 23 */
	REGOP(0x10001C50, 0), /* 24 */
	REGOP(0x10001C5C, 0), /* 25 */
	REGOP(0x10001C60, 0), /* 26 */
	REGOP(0x10001C6C, 0), /* 27 */
	REGOP(0x10001CA0, 0), /* 28 */
	REGOP(0x10001CAC, 0), /* 29 */
	REGOP(0x10023000, 0), /* 30 */
	REGOP(0x1002B000, 0), /* 31 */
	REGOP(0x10042000, 0), /* 32 */
	REGOP(0x10028000, 0), /* 33 */
	REGOP(0x11C105D0, 0), /* 34 */
	REGOP(0x11C105D4, 0), /* 35 */
	REGOP(0x11C105D8, 0), /* 36 */
};

#endif /* __GPUFREQ_REG_MT6835_H__ */
