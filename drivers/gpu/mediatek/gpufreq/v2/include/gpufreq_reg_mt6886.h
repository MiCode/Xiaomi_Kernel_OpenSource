/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __GPUFREQ_REG_MT6886_H__
#define __GPUFREQ_REG_MT6886_H__
/**************************************************
 * GPUFREQ Register Definition
 **************************************************/
#define MALI_BASE                       (g_mali_base)                         /* 0x13000000 */
#define MALI_GPU_ID                     (MALI_BASE + 0x000)                   /* 0x13000000 */

#define MFG_TOP_CFG_BASE                (g_mfg_top_base)                      /* 0x13FBF000 */
#define MFG_CG_CON                      (MFG_TOP_CFG_BASE + 0x000)            /* 0x13FBF000 */
#define MFG_DCM_CON_0                   (MFG_TOP_CFG_BASE + 0x010)            /* 0x13FBF010 */
#define MFG_ASYNC_CON                   (MFG_TOP_CFG_BASE + 0x020)            /* 0x13FBF020 */
#define MFG_GLOBAL_CON                  (MFG_TOP_CFG_BASE + 0x0B0)            /* 0x13FBF0B0 */
#define MFG_AXCOHERENCE_CON             (MFG_TOP_CFG_BASE + 0x168)            /* 0x13FBF168 */
#define MFG_1TO2AXI_CON_00              (MFG_TOP_CFG_BASE + 0x8E0)            /* 0x13FBF8E0 */
#define MFG_1TO2AXI_CON_02              (MFG_TOP_CFG_BASE + 0x8E8)            /* 0x13FBF8E8 */
#define MFG_1TO2AXI_CON_04              (MFG_TOP_CFG_BASE + 0x910)            /* 0x13FBF910 */
#define MFG_1TO2AXI_CON_06              (MFG_TOP_CFG_BASE + 0x918)            /* 0x13FBF918 */
#define MFG_ACTIVE_POWER_CON_CG_06      (MFG_TOP_CFG_BASE + 0x100)            /* 0x13FBF100 */
#define MFG_ACTIVE_POWER_CON_00         (MFG_TOP_CFG_BASE + 0x400)            /* 0x13FBF400 */
#define MFG_ACTIVE_POWER_CON_01         (MFG_TOP_CFG_BASE + 0x404)            /* 0x13FBF404 */
#define MFG_ACTIVE_POWER_CON_06         (MFG_TOP_CFG_BASE + 0x418)            /* 0x13FBF418 */
#define MFG_ACTIVE_POWER_CON_07         (MFG_TOP_CFG_BASE + 0x41C)            /* 0x13FBF41C */
#define MFG_ACTIVE_POWER_CON_12         (MFG_TOP_CFG_BASE + 0x430)            /* 0x13FBF430 */
#define MFG_ACTIVE_POWER_CON_13         (MFG_TOP_CFG_BASE + 0x434)            /* 0x13FBF434 */
#define MFG_ACTIVE_POWER_CON_18         (MFG_TOP_CFG_BASE + 0x448)            /* 0x13FBF448 */
#define MFG_ACTIVE_POWER_CON_19         (MFG_TOP_CFG_BASE + 0x44C)            /* 0x13FBF44C */
#define MFG_SENSOR_BCLK_CG              (MFG_TOP_CFG_BASE + 0xF98)            /* 0x13FBFF98 */
#define MFG_I2M_PROTECTOR_CFG_00        (MFG_TOP_CFG_BASE + 0xF60)            /* 0x13FBFF60 */
#define MFG_I2M_PROTECTOR_CFG_01        (MFG_TOP_CFG_BASE + 0xF64)            /* 0x13FBFF64 */
#define MFG_I2M_PROTECTOR_CFG_02        (MFG_TOP_CFG_BASE + 0xF68)            /* 0x13FBFF68 */
#define MFG_I2M_PROTECTOR_CFG_03        (MFG_TOP_CFG_BASE + 0xFA8)            /* 0x13FBFFA8 */
#define MFG_QCHANNEL_CON                (MFG_TOP_CFG_BASE + 0x0B4)            /* 0x13FBF0B4 */
#define MFG_DEBUG_SEL                   (MFG_TOP_CFG_BASE + 0x170)            /* 0x13FBF170 */
#define MFG_DEBUG_TOP                   (MFG_TOP_CFG_BASE + 0x178)            /* 0x13FBF178 */
#define MFG_TIMESTAMP                   (MFG_TOP_CFG_BASE + 0x130)            /* 0x13FBF130 */
#define MFG_DEBUGMON_CON_00             (MFG_TOP_CFG_BASE + 0x8F8)            /* 0x13FBF8F8 */
#define MFG_DFD_CON_0                   (MFG_TOP_CFG_BASE + 0xA00)            /* 0x13FBFA00 */
#define MFG_DFD_CON_1                   (MFG_TOP_CFG_BASE + 0xA04)            /* 0x13FBFA04 */
#define MFG_DFD_CON_2                   (MFG_TOP_CFG_BASE + 0xA08)            /* 0x13FBFA08 */
#define MFG_DFD_CON_3                   (MFG_TOP_CFG_BASE + 0xA0C)            /* 0x13FBFA0C */
#define MFG_DFD_CON_4                   (MFG_TOP_CFG_BASE + 0xA10)            /* 0x13FBFA10 */
#define MFG_DFD_CON_5                   (MFG_TOP_CFG_BASE + 0xA14)            /* 0x13FBFA14 */
#define MFG_DFD_CON_6                   (MFG_TOP_CFG_BASE + 0xA18)            /* 0x13FBFA18 */
#define MFG_DFD_CON_7                   (MFG_TOP_CFG_BASE + 0xA1C)            /* 0x13FBFA1C */
#define MFG_DFD_CON_8                   (MFG_TOP_CFG_BASE + 0xA20)            /* 0x13FBFA20 */
#define MFG_DFD_CON_9                   (MFG_TOP_CFG_BASE + 0xA24)            /* 0x13FBFA24 */
#define MFG_DFD_CON_10                  (MFG_TOP_CFG_BASE + 0xA28)            /* 0x13FBFA28 */
#define MFG_DFD_CON_11                  (MFG_TOP_CFG_BASE + 0xA2C)            /* 0x13FBFA2C */
#define MFG_MERGE_R_CON_00              (MFG_TOP_CFG_BASE + 0x8A0)            /* 0x13FBF8A0 */
#define MFG_MERGE_R_CON_02              (MFG_TOP_CFG_BASE + 0x8A8)            /* 0x13FBF8A8 */
#define MFG_MERGE_R_CON_04              (MFG_TOP_CFG_BASE + 0x8C0)            /* 0x13FBF8C0 */
#define MFG_MERGE_R_CON_06              (MFG_TOP_CFG_BASE + 0x8C8)            /* 0x13FBF8C8 */
#define MFG_MERGE_W_CON_00              (MFG_TOP_CFG_BASE + 0x8B0)            /* 0x13FBF8B0 */
#define MFG_MERGE_W_CON_02              (MFG_TOP_CFG_BASE + 0x8B8)            /* 0x13FBF8B8 */
#define MFG_MERGE_W_CON_04              (MFG_TOP_CFG_BASE + 0x8D0)            /* 0x13FBF8D0 */
#define MFG_MERGE_W_CON_06              (MFG_TOP_CFG_BASE + 0x8D8)            /* 0x13FBF8D8 */

#define MFG_PLL_BASE                    (g_mfg_pll_base)                      /* 0x13FA0000 */
#define MFG_PLL_CON0                    (MFG_PLL_BASE + 0x008)                /* 0x13FA0008 */
#define MFG_PLL_CON1                    (MFG_PLL_BASE + 0x00C)                /* 0x13FA000C */
#define MFG_PLL_FQMTR_CON0              (MFG_PLL_BASE + 0x040)                /* 0x13FA0040 */
#define MFG_PLL_FQMTR_CON1              (MFG_PLL_BASE + 0x044)                /* 0x13FA0044 */

#define SENSOR_PLL_BASE                 (g_sensor_pll_base)                   /* 0x13FA0400 */
#define SENSOR_PLL_CON0                 (SENSOR_PLL_BASE + 0x008)             /* 0x13FA0408 */
#define SENSOR_PLL_CON1                 (SENSOR_PLL_BASE + 0x00C)             /* 0x13FA040C */
#define SENSOR_PLL_FQMTR_CON0           (SENSOR_PLL_BASE + 0x040)             /* 0x13FA0440 */
#define SENSOR_PLL_FQMTR_CON1           (SENSOR_PLL_BASE + 0x044)             /* 0x13FA0444 */

#define MFGSC_PLL_BASE                  (g_mfgsc_pll_base)                    /* 0x13FA0C00 */
#define MFGSC_PLL_CON0                  (MFGSC_PLL_BASE + 0x008)              /* 0x13FA0C08 */
#define MFGSC_PLL_CON1                  (MFGSC_PLL_BASE + 0x00C)              /* 0x13FA0C0C */
#define MFGSC_PLL_FQMTR_CON0            (MFGSC_PLL_BASE + 0x040)              /* 0x13FA0C40 */
#define MFGSC_PLL_FQMTR_CON1            (MFGSC_PLL_BASE + 0x044)              /* 0x13FA0C44 */

#define MFG_RPC_BASE                    (g_mfg_rpc_base)                      /* 0x13F90000 */
#define MFG_RPC_AO_CLK_CFG              (MFG_RPC_BASE + 0x1034)               /* 0x13F91034 */
#define MFG_RPC_MFG1_PWR_CON            (MFG_RPC_BASE + 0x1070)               /* 0x13F91070 */
#define MFG_RPC_MFG2_PWR_CON            (MFG_RPC_BASE + 0x10A0)               /* 0x13F910A0 */
#define MFG_RPC_MFG9_PWR_CON            (MFG_RPC_BASE + 0x10BC)               /* 0x13F910BC */
#define MFG_RPC_MFG10_PWR_CON           (MFG_RPC_BASE + 0x10C0)               /* 0x13F910C0 */
#define MFG_RPC_MFG11_PWR_CON           (MFG_RPC_BASE + 0x10C4)               /* 0x13F910C4 */
#define MFG_RPC_MFG12_PWR_CON           (MFG_RPC_BASE + 0x10C8)               /* 0x13F910C8 */
#define MFG_RPC_SLP_PROT_EN_SET         (MFG_RPC_BASE + 0x1040)               /* 0x13F91040 */
#define MFG_RPC_SLP_PROT_EN_CLR         (MFG_RPC_BASE + 0x1044)               /* 0x13F91044 */
#define MFG_RPC_SLP_PROT_EN_STA         (MFG_RPC_BASE + 0x1048)               /* 0x13F91048 */

#define SPM_BASE                        (g_sleep)                             /* 0x1C001000 */
#define SPM_SPM2GPUPM_CON               (SPM_BASE + 0x410)                    /* 0x1C001410 */
#define SPM_MFG0_PWR_CON                (SPM_BASE + 0xEE8)                    /* 0x1C001EE8 */
#define SPM_SEMA_M4                     (SPM_BASE + 0x6AC)                    /* 0x1C0016AC */
#define SPM_SOC_BUCK_ISO_CON            (SPM_BASE + 0xF74)                    /* 0x1C001F74 */

#define NTH_EMICFG_BASE                 (g_nth_emicfg_base)                   /* 0x1021C000 */
#define NTH_MFG_EMI1_GALS_SLV_DBG       (NTH_EMICFG_BASE + 0x82C)             /* 0x1021C82C */
#define NTH_MFG_EMI0_GALS_SLV_DBG       (NTH_EMICFG_BASE + 0x830)             /* 0x1021C830 */
#define NTH_APU_ACP_GALS_SLV_CTRL       (NTH_EMICFG_BASE + 0x600)             /* 0x1021C600 */
#define NTH_APU_EMI1_GALS_SLV_CTRL      (NTH_EMICFG_BASE + 0x624)             /* 0x1021C624 */

#define NTH_EMICFG_AO_MEM_BASE          (g_nth_emicfg_ao_mem_base)            /* 0x10270000 */
#define NTH_M6M7_IDLE_BIT_EN_1          (NTH_EMICFG_AO_MEM_BASE + 0x228)      /* 0x10270228 */
#define NTH_M6M7_IDLE_BIT_EN_0          (NTH_EMICFG_AO_MEM_BASE + 0x22C)      /* 0x1027022C */

#define IFRCFG_AO_BASE                  (g_infracfg_ao_base)                  /* 0x10001000 */
#define EMISYS_PROTECT_EN_SET_0         (IFRCFG_AO_BASE + 0xC64)              /* 0x10001C64 */
#define EMISYS_PROTECT_EN_CLR_0         (IFRCFG_AO_BASE + 0xC68)              /* 0x10001C68 */
#define EMISYS_PROTECT_RDY_STA_0        (IFRCFG_AO_BASE + 0xC6C)              /* 0x10001C6C */
#define MD_MFGSYS_PROTECT_EN_STA_0      (IFRCFG_AO_BASE + 0xCA0)              /* 0x10001CA0 */
#define MD_MFGSYS_PROTECT_EN_SET_0      (IFRCFG_AO_BASE + 0xCA4)              /* 0x10001CA4 */
#define MD_MFGSYS_PROTECT_EN_CLR_0      (IFRCFG_AO_BASE + 0xCA8)              /* 0x10001CA8 */
#define MD_MFGSYS_PROTECT_RDY_STA_0     (IFRCFG_AO_BASE + 0xCAC)              /* 0x10001CAC */

#define INFRA_AO_DEBUG_CTRL_BASE        (g_infra_ao_debug_ctrl)               /* 0x10023000 */
#define INFRA_AO_BUS0_U_DEBUG_CTRL0     (INFRA_AO_DEBUG_CTRL_BASE + 0x000)    /* 0x10023000 */

#define INFRA_AO1_DEBUG_CTRL_BASE       (g_infra_ao1_debug_ctrl)              /* 0x1002B000 */
#define INFRA_AO1_BUS1_U_DEBUG_CTRL0    (INFRA_AO1_DEBUG_CTRL_BASE + 0x000)   /* 0x1002B000 */

#define NTH_EMI_AO_DEBUG_CTRL_BASE      (g_nth_emi_ao_debug_ctrl)             /* 0x10042000 */
#define NTH_EMI_AO_DEBUG_CTRL0          (NTH_EMI_AO_DEBUG_CTRL_BASE + 0x000)  /* 0x10042000 */

#define EFUSE_BASE                      (g_efuse_base)                        /* 0x11E30000 */
#define EFUSE_PTPOD21                   (EFUSE_BASE + 0x5C0)                  /* 0x11E305C0 */
#define EFUSE_PTPOD22                   (EFUSE_BASE + 0x5C4)                  /* 0x11E305C4 */
#define EFUSE_PTPOD23                   (EFUSE_BASE + 0x5C8)                  /* 0x11E305C8 */
#define EFUSE_ASENSOR_RT                (EFUSE_BASE + 0xA6C)                  /* 0x11E30A6C */
#define EFUSE_ASENSOR_HT                (EFUSE_BASE + 0xA70)                  /* 0x11E30A70 */
#define EFUSE_ASENSOR_TEMPER            (EFUSE_BASE + 0x5C4)                  /* 0x11E305C4 */

#define MFG_CPE_CTRL_MCU_BASE           (g_mfg_cpe_ctrl_mcu_base)             /* 0x13FB9C00 */
#define MFG_CPE_CTRL_MCU_REG_CPEMONCTL  (MFG_CPE_CTRL_MCU_BASE + 0x000)       /* 0x13FB9C00 */
#define MFG_CPE_CTRL_MCU_REG_CEPEN      (MFG_CPE_CTRL_MCU_BASE + 0x004)       /* 0x13FB9C04 */
#define MFG_CPE_CTRL_MCU_REG_CPEIRQSTS  (MFG_CPE_CTRL_MCU_BASE + 0x010)       /* 0x13FB9C10 */
#define MFG_CPE_CTRL_MCU_REG_CPEINTSTS  (MFG_CPE_CTRL_MCU_BASE + 0x028)       /* 0x13FB9C28 */

#define MFG_CPE_SENSOR_BASE             (g_mfg_cpe_sensor_base)               /* 0x13FB6000 */
#define MFG_CPE_SENSOR_C0ASENSORDATA2   (MFG_CPE_SENSOR_BASE + 0x008)         /* 0x13FB6008 */
#define MFG_CPE_SENSOR_C0ASENSORDATA3   (MFG_CPE_SENSOR_BASE + 0x00C)         /* 0x13FB600C */

#define MFG_SECURE_BASE                 (g_mfg_secure_base)                   /* 0x13FBC000 */
#define MFG_SECURE_REG                  (MFG_SECURE_BASE + 0xFE0)             /* 0x13FBCFE0 */

#define DRM_DEBUG_BASE                  (g_drm_debug_base)                    /* 0x1000D000 */
#define DRM_DEBUG_MFG_REG               (DRM_DEBUG_BASE + 0x060)              /* 0x1000D060 */

/**************************************************
 * MFGSYS Register Info
 **************************************************/
enum gpufreq_reg_info_idx {
	IDX_MFG_CG_CON                   = 0,
	IDX_MFG_DCM_CON_0                = 1,
	IDX_MFG_ASYNC_CON                = 2,
	IDX_MFG_GLOBAL_CON               = 3,
	IDX_MFG_AXCOHERENCE_CON          = 4,
	IDX_MFG_PLL_CON0                 = 5,
	IDX_MFG_PLL_CON1                 = 6,
	IDX_MFGSC_PLL_CON0               = 7,
	IDX_MFGSC_PLL_CON1               = 8,
	IDX_MFG_RPC_AO_CLK_CFG           = 9,
	IDX_MFG_RPC_MFG1_PWR_CON         = 10,
	IDX_MFG_RPC_MFG2_PWR_CON         = 11,
	IDX_MFG_RPC_MFG9_PWR_CON         = 12,
	IDX_MFG_RPC_MFG10_PWR_CON        = 13,
	IDX_MFG_RPC_MFG11_PWR_CON        = 14,
	IDX_MFG_RPC_MFG12_PWR_CON        = 15,
	IDX_MFG_RPC_SLP_PROT_EN_SET      = 16,
	IDX_MFG_RPC_SLP_PROT_EN_CLR      = 17,
	IDX_MFG_RPC_SLP_PROT_EN_STA      = 18,
	IDX_SPM_SPM2GPUPM_CON            = 19,
	IDX_SPM_MFG0_PWR_CON             = 20,
	IDX_SPM_SOC_BUCK_ISO_CON         = 21,
	IDX_EFUSE_PTPOD21                = 22,
	IDX_EFUSE_PTPOD22                = 23,
	IDX_EFUSE_PTPOD23                = 24,
	IDX_NTH_MFG_EMI1_GALS_SLV_DBG    = 25,
	IDX_NTH_MFG_EMI0_GALS_SLV_DBG    = 26,
	IDX_NTH_M6M7_IDLE_BIT_EN_1       = 27,
	IDX_NTH_M6M7_IDLE_BIT_EN_0       = 28,
	IDX_EMISYS_PROTECT_EN_SET_0      = 29,
	IDX_EMISYS_PROTECT_EN_CLR_0      = 30,
	IDX_EMISYS_PROTECT_RDY_STA_0     = 31,
	IDX_MD_MFGSYS_PROTECT_EN_STA_0   = 32,
	IDX_MD_MFGSYS_PROTECT_EN_SET_0   = 33,
	IDX_MD_MFGSYS_PROTECT_EN_CLR_0   = 34,
	IDX_MD_MFGSYS_PROTECT_RDY_STA_0  = 35,
	IDX_NTH_EMI_AO_DEBUG_CTRL0       = 36,
	IDX_INFRA_AO_BUS0_U_DEBUG_CTRL0  = 37,
	IDX_INFRA_AO1_BUS1_U_DEBUG_CTRL0 = 38,
};

#define NUM_MFGSYS_REG                  ARRAY_SIZE(g_reg_mfgsys)
static struct gpufreq_reg_info g_reg_mfgsys[] = {
	REGOP(0x13FBF000, 0), /* 0 */
	REGOP(0x13FBF010, 0), /* 1 */
	REGOP(0x13FBF020, 0), /* 2 */
	REGOP(0x13FBF0B0, 0), /* 3 */
	REGOP(0x13FBF168, 0), /* 4 */
	REGOP(0x13FA0008, 0), /* 5 */
	REGOP(0x13FA000C, 0), /* 6 */
	REGOP(0x13FA0C08, 0), /* 7 */
	REGOP(0x13FA0C0C, 0), /* 8 */
	REGOP(0x13F91034, 0), /* 9 */
	REGOP(0x13F91070, 0), /* 10 */
	REGOP(0x13F910A0, 0), /* 11 */
	REGOP(0x13F910BC, 0), /* 12 */
	REGOP(0x13F910C0, 0), /* 13 */
	REGOP(0x13F910C4, 0), /* 15 */
	REGOP(0x13F910C8, 0), /* 15 */
	REGOP(0x13F91040, 0), /* 16 */
	REGOP(0x13F91044, 0), /* 17 */
	REGOP(0x13F91048, 0), /* 18 */
	REGOP(0x1C001410, 0), /* 19 */
	REGOP(0x1C001EE8, 0), /* 20 */
	REGOP(0x1C001F78, 0), /* 21 */
	REGOP(0x11E805D4, 0), /* 22 */
	REGOP(0x11E805D8, 0), /* 23 */
	REGOP(0x11E805DC, 0), /* 24 */
	REGOP(0x1021C82C, 0), /* 25 */
	REGOP(0x1021C830, 0), /* 26 */
	REGOP(0x10270228, 0), /* 27 */
	REGOP(0x1027022C, 0), /* 28 */
	REGOP(0x10001C64, 0), /* 29 */
	REGOP(0x10001C68, 0), /* 30 */
	REGOP(0x10001C6C, 0), /* 31 */
	REGOP(0x10001CA0, 0), /* 32 */
	REGOP(0x10001CA4, 0), /* 33 */
	REGOP(0x10001CA8, 0), /* 34 */
	REGOP(0x10001CAC, 0), /* 35 */
	REGOP(0x10042000, 0), /* 36 */
	REGOP(0x10023000, 0), /* 37 */
	REGOP(0x1002B000, 0), /* 38 */
};

#endif /* __GPUFREQ_REG_MT6886_H__ */
