/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __GPUFREQ_REG_MT6985_H__
#define __GPUFREQ_REG_MT6985_H__

#include <linux/io.h>
#include <linux/bits.h>

/**************************************************
 * GPUFREQ Register Operation
 **************************************************/
/* HW limit: MFG_TOP_CONFIG need to be continuously read twice */
#define readl_mfg readl_mfg
static inline u32 readl_mfg(const void __iomem *addr)
{
	int i = 0;
	u32 val[2] = {0, 0};

	do {
		if (likely(i < 100)) {
			val[i % 2] = readl(addr);
			val[++i % 2] = readl(addr);
		} else {
			__gpufreq_abort("read MFG_TOP_CFG (0x%08x)=(0x%08x, 0x%08x) timeout",
				addr, val[0], val[1]);
			break;
		}
	} while (val[0] != val[1]);

	return val[0];
}

/**************************************************
 * GPUFREQ Register Definition
 **************************************************/
#define MALI_BASE                       (g_mali_base)                         /* 0x13000000 */
#define MALI_GPU_ID                     (MALI_BASE + 0x000)                   /* 0x13000000 */
#define MALI_SHADER_READY_LO            (MALI_BASE + 0x140)                   /* 0x13000140 */
#define MALI_L2_PWRON_LO                (MALI_BASE + 0x1A0)                   /* 0x130001A0 */
#define MALI_L2_PWRON_HI                (MALI_BASE + 0x1A4)                   /* 0x130001A4 */
#define MALI_L2_PWROFF_LO               (MALI_BASE + 0x1E0)                   /* 0x130001E0 */
#define MALI_L2_PWROFF_HI               (MALI_BASE + 0x1E4)                   /* 0x130001E4 */
#define MALI_GPU_IRQ_CLEAR              (MALI_BASE + 0x024)                   /* 0x13000024 */
#define MALI_GPU_IRQ_MASK               (MALI_BASE + 0x028)                   /* 0x13000028 */
#define MALI_GPU_IRQ_STATUS             (MALI_BASE + 0x02C)                   /* 0x1300002C */

#define MFG_TOP_CFG_BASE                (g_mfg_top_base)                      /* 0x13FBF000 */
#define MFG_CG_CON                      (MFG_TOP_CFG_BASE + 0x000)            /* 0x13FBF000 */
#define MFG_DCM_CON_0                   (MFG_TOP_CFG_BASE + 0x010)            /* 0x13FBF010 */
#define MFG_ASYNC_CON                   (MFG_TOP_CFG_BASE + 0x020)            /* 0x13FBF020 */
#define MFG_ASYNC_CON3                  (MFG_TOP_CFG_BASE + 0x02C)            /* 0x13FBF02C */
#define MFG_GLOBAL_CON                  (MFG_TOP_CFG_BASE + 0x0B0)            /* 0x13FBF0B0 */
#define MFG_AXCOHERENCE_CON             (MFG_TOP_CFG_BASE + 0x168)            /* 0x13FBF168 */
#define MFG_1TO2AXI_CON_00              (MFG_TOP_CFG_BASE + 0x8E0)            /* 0x13FBF8E0 */
#define MFG_1TO2AXI_CON_02              (MFG_TOP_CFG_BASE + 0x8E8)            /* 0x13FBF8E8 */
#define MFG_1TO2AXI_CON_04              (MFG_TOP_CFG_BASE + 0x910)            /* 0x13FBF910 */
#define MFG_1TO2AXI_CON_06              (MFG_TOP_CFG_BASE + 0x918)            /* 0x13FBF918 */
#define MFG_OUT_1TO2AXI_CON_00          (MFG_TOP_CFG_BASE + 0x900)            /* 0x13FBF900 */
#define MFG_OUT_1TO2AXI_CON_02          (MFG_TOP_CFG_BASE + 0x908)            /* 0x13FBF908 */
#define MFG_OUT_1TO2AXI_CON_04          (MFG_TOP_CFG_BASE + 0x920)            /* 0x13FBF920 */
#define MFG_OUT_1TO2AXI_CON_06          (MFG_TOP_CFG_BASE + 0x928)            /* 0x13FBF928 */
#define MFG_ACTIVE_POWER_CON_CG         (MFG_TOP_CFG_BASE + 0x100)            /* 0x13FBF100 */
#define MFG_ACTIVE_POWER_CON_ST0        (MFG_TOP_CFG_BASE + 0x120)            /* 0x13FBF120 */
#define MFG_ACTIVE_POWER_CON_ST1        (MFG_TOP_CFG_BASE + 0x140)            /* 0x13FBF140 */
#define MFG_ACTIVE_POWER_CON_ST2        (MFG_TOP_CFG_BASE + 0x118)            /* 0x13FBF118 */
#define MFG_ACTIVE_POWER_CON_ST4        (MFG_TOP_CFG_BASE + 0x0C0)            /* 0x13FBF0C0 */
#define MFG_ACTIVE_POWER_CON_ST5        (MFG_TOP_CFG_BASE + 0x098)            /* 0x13FBF098 */
#define MFG_ACTIVE_POWER_CON_ST6        (MFG_TOP_CFG_BASE + 0x1C0)            /* 0x13FBF1C0 */
#define MFG_ACTIVE_POWER_CON_00         (MFG_TOP_CFG_BASE + 0x400)            /* 0x13FBF400 */
#define MFG_ACTIVE_POWER_CON_01         (MFG_TOP_CFG_BASE + 0x404)            /* 0x13FBF404 */
#define MFG_ACTIVE_POWER_CON_06         (MFG_TOP_CFG_BASE + 0x418)            /* 0x13FBF418 */
#define MFG_ACTIVE_POWER_CON_07         (MFG_TOP_CFG_BASE + 0x41C)            /* 0x13FBF41C */
#define MFG_ACTIVE_POWER_CON_12         (MFG_TOP_CFG_BASE + 0x430)            /* 0x13FBF430 */
#define MFG_ACTIVE_POWER_CON_13         (MFG_TOP_CFG_BASE + 0x434)            /* 0x13FBF434 */
#define MFG_ACTIVE_POWER_CON_18         (MFG_TOP_CFG_BASE + 0x448)            /* 0x13FBF448 */
#define MFG_ACTIVE_POWER_CON_19         (MFG_TOP_CFG_BASE + 0x44C)            /* 0x13FBF44C */
#define MFG_ACTIVE_POWER_CON_24         (MFG_TOP_CFG_BASE + 0x460)            /* 0x13FBF460 */
#define MFG_ACTIVE_POWER_CON_25         (MFG_TOP_CFG_BASE + 0x464)            /* 0x13FBF464 */
#define MFG_ACTIVE_POWER_CON_30         (MFG_TOP_CFG_BASE + 0x478)            /* 0x13FBF478 */
#define MFG_ACTIVE_POWER_CON_31         (MFG_TOP_CFG_BASE + 0x47C)            /* 0x13FBF47C */
#define MFG_ACTIVE_POWER_CON_36         (MFG_TOP_CFG_BASE + 0x490)            /* 0x13FBF490 */
#define MFG_ACTIVE_POWER_CON_37         (MFG_TOP_CFG_BASE + 0x494)            /* 0x13FBF494 */
#define MFG_ACTIVE_POWER_CON_42         (MFG_TOP_CFG_BASE + 0x4A8)            /* 0x13FBF4A8 */
#define MFG_ACTIVE_POWER_CON_43         (MFG_TOP_CFG_BASE + 0x4AC)            /* 0x13FBF4AC */
#define MFG_ACTIVE_POWER_CON_48         (MFG_TOP_CFG_BASE + 0x4C0)            /* 0x13FBF4C0 */
#define MFG_ACTIVE_POWER_CON_49         (MFG_TOP_CFG_BASE + 0x4C4)            /* 0x13FBF4C4 */
#define MFG_ACTIVE_POWER_CON_54         (MFG_TOP_CFG_BASE + 0x4D8)            /* 0x13FBF4D8 */
#define MFG_ACTIVE_POWER_CON_55         (MFG_TOP_CFG_BASE + 0x4DC)            /* 0x13FBF4DC */
#define MFG_ACTIVE_POWER_CON_60         (MFG_TOP_CFG_BASE + 0x4F0)            /* 0x13FBF4F0 */
#define MFG_ACTIVE_POWER_CON_61         (MFG_TOP_CFG_BASE + 0x4F4)            /* 0x13FBF4F4 */
#define MFG_SENSOR_BCLK_CG              (MFG_TOP_CFG_BASE + 0xF98)            /* 0x13FBFF98 */
#define MFG_I2M_PROTECTOR_CFG_00        (MFG_TOP_CFG_BASE + 0xF60)            /* 0x13FBFF60 */
#define MFG_I2M_PROTECTOR_CFG_01        (MFG_TOP_CFG_BASE + 0xF64)            /* 0x13FBFF64 */
#define MFG_I2M_PROTECTOR_CFG_02        (MFG_TOP_CFG_BASE + 0xF68)            /* 0x13FBFF68 */
#define MFG_I2M_PROTECTOR_CFG_03        (MFG_TOP_CFG_BASE + 0xFA8)            /* 0x13FBFFA8 */
#define MFG_DUMMY_REG                   (MFG_TOP_CFG_BASE + 0x500)            /* 0x13FBF500 */
#define MFG_SRAM_FUL_SEL_ULV            (MFG_TOP_CFG_BASE + 0x080)            /* 0x13FBF080 */
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
#define MFG_MALI_AXUSER_M0_CFG1         (MFG_TOP_CFG_BASE + 0x704)            /* 0x13FBF704 */
#define MFG_MALI_AXUSER_M0_CFG2         (MFG_TOP_CFG_BASE + 0x708)            /* 0x13FBF708 */
#define MFG_MALI_AXUSER_M0_CFG3         (MFG_TOP_CFG_BASE + 0x70C)            /* 0x13FBF70C */

#define MFG_PLL_BASE                    (g_mfg_pll_base)                      /* 0x13FA0000 */
#define MFG_PLL_CON0                    (MFG_PLL_BASE + 0x008)                /* 0x13FA0008 */
#define MFG_PLL_CON1                    (MFG_PLL_BASE + 0x00C)                /* 0x13FA000C */
#define MFG_PLL_FQMTR_CON0              (MFG_PLL_BASE + 0x040)                /* 0x13FA0040 */
#define MFG_PLL_FQMTR_CON1              (MFG_PLL_BASE + 0x044)                /* 0x13FA0044 */

#define MFGSC_PLL_BASE                  (g_mfgsc_pll_base)                    /* 0x13FA0C00 */
#define MFGSC_PLL_CON0                  (MFGSC_PLL_BASE + 0x008)              /* 0x13FA0C08 */
#define MFGSC_PLL_CON1                  (MFGSC_PLL_BASE + 0x00C)              /* 0x13FA0C0C */
#define MFGSC_PLL_FQMTR_CON0            (MFGSC_PLL_BASE + 0x040)              /* 0x13FA0C40 */
#define MFGSC_PLL_FQMTR_CON1            (MFGSC_PLL_BASE + 0x044)              /* 0x13FA0C44 */

#define MFG_RPC_BASE                    (g_mfg_rpc_base)                      /* 0x13F90000 */
#define MFG_RPC_AO_CLK_CFG              (MFG_RPC_BASE + 0x1034)               /* 0x13F91034 */
#define MFG_RPC_MFG1_PWR_CON            (MFG_RPC_BASE + 0x1070)               /* 0x13F91070 */
#define MFG_RPC_MFG2_PWR_CON            (MFG_RPC_BASE + 0x10A0)               /* 0x13F910A0 */
#define MFG_RPC_MFG3_PWR_CON            (MFG_RPC_BASE + 0x10A4)               /* 0x13F910A4 */
#define MFG_RPC_MFG4_PWR_CON            (MFG_RPC_BASE + 0x10A8)               /* 0x13F910A8 */
#define MFG_RPC_MFG5_PWR_CON            (MFG_RPC_BASE + 0x10AC)               /* 0x13F910AC */
#define MFG_RPC_MFG6_PWR_CON            (MFG_RPC_BASE + 0x10B0)               /* 0x13F910B0 */
#define MFG_RPC_MFG7_PWR_CON            (MFG_RPC_BASE + 0x10B4)               /* 0x13F910B4 */
#define MFG_RPC_MFG8_PWR_CON            (MFG_RPC_BASE + 0x10B8)               /* 0x13F910B8 */
#define MFG_RPC_MFG9_PWR_CON            (MFG_RPC_BASE + 0x10BC)               /* 0x13F910BC */
#define MFG_RPC_MFG10_PWR_CON           (MFG_RPC_BASE + 0x10C0)               /* 0x13F910C0 */
#define MFG_RPC_MFG11_PWR_CON           (MFG_RPC_BASE + 0x10C4)               /* 0x13F910C4 */
#define MFG_RPC_MFG12_PWR_CON           (MFG_RPC_BASE + 0x10C8)               /* 0x13F910C8 */
#define MFG_RPC_MFG13_PWR_CON           (MFG_RPC_BASE + 0x10CC)               /* 0x13F910CC */
#define MFG_RPC_MFG14_PWR_CON           (MFG_RPC_BASE + 0x10D0)               /* 0x13F910D0 */
#define MFG_RPC_MFG15_PWR_CON           (MFG_RPC_BASE + 0x10D4)               /* 0x13F910D4 */
#define MFG_RPC_MFG16_PWR_CON           (MFG_RPC_BASE + 0x10D8)               /* 0x13F910D8 */
#define MFG_RPC_MFG17_PWR_CON           (MFG_RPC_BASE + 0x10DC)               /* 0x13F910DC */
#define MFG_RPC_MFG18_PWR_CON           (MFG_RPC_BASE + 0x10E0)               /* 0x13F910E0 */
#define MFG_RPC_MFG19_PWR_CON           (MFG_RPC_BASE + 0x10E4)               /* 0x13F910E4 */
#define MFG_RPC_SLP_PROT_EN_SET         (MFG_RPC_BASE + 0x1040)               /* 0x13F91040 */
#define MFG_RPC_SLP_PROT_EN_CLR         (MFG_RPC_BASE + 0x1044)               /* 0x13F91044 */
#define MFG_RPC_SLP_PROT_EN_STA         (MFG_RPC_BASE + 0x1048)               /* 0x13F91048 */
#define MFG_RPC_IPS_SES_PWR_CON         (MFG_RPC_BASE + 0x10FC)               /* 0x13F910FC */

#define SPM_BASE                        (g_sleep)                             /* 0x1C001000 */
#define SPM_SPM2GPUPM_CON               (SPM_BASE + 0x410)                    /* 0x1C001410 */
#define SPM_MFG0_PWR_CON                (SPM_BASE + 0xEE8)                    /* 0x1C001EE8 */
#define SPM_XPU_PWR_STATUS              (SPM_BASE + 0xF94)                    /* 0x1C001F94 */
#define SPM_XPU_PWR_STATUS_2ND          (SPM_BASE + 0xF98)                    /* 0x1C001F98 */
#define SPM_SEMA_M3                     (SPM_BASE + 0x6A8)                    /* 0x1C0016A8 */
#define SPM_SEMA_M4                     (SPM_BASE + 0x6AC)                    /* 0x1C0016AC */
#define SPM_SOC_BUCK_ISO_CON            (SPM_BASE + 0xF78)                    /* 0x1C001F78 */
#define SPM_SOC_BUCK_ISO_CON_SET        (SPM_BASE + 0xF7C)                    /* 0x1C001F7C */
#define SPM_SOC_BUCK_ISO_CON_CLR        (SPM_BASE + 0xF80)                    /* 0x1C001F80 */
#define SPM_SRC_REQ                     (SPM_BASE + 0x818)                    /* 0x1C001818 */

#define TOPCKGEN_BASE                   (g_topckgen_base)                     /* 0x10000000 */
#define TOPCK_CLK_CFG_3                 (TOPCKGEN_BASE + 0x040)               /* 0x10000040 */
#define TOPCK_CLK_CFG_30                (TOPCKGEN_BASE + 0x1F0)               /* 0x100001F0 */

#define NTH_EMICFG_BASE                 (g_nth_emicfg_base)                   /* 0x1021C000 */
#define NTH_MFG_EMI1_GALS_SLV_DBG       (NTH_EMICFG_BASE + 0x82C)             /* 0x1021C82C */
#define NTH_MFG_EMI0_GALS_SLV_DBG       (NTH_EMICFG_BASE + 0x830)             /* 0x1021C830 */
#define NTH_APU_EMI1_GALS_SLV_DBG       (NTH_EMICFG_BASE + 0x824)             /* 0x1021C824 */
#define NTH_APU_EMI0_GALS_SLV_DBG       (NTH_EMICFG_BASE + 0x828)             /* 0x1021C828 */
#define NTH_APU_ACP_GALS_SLV_CTRL       (NTH_EMICFG_BASE + 0x600)             /* 0x1021C600 */
#define NTH_APU_EMI1_GALS_SLV_CTRL      (NTH_EMICFG_BASE + 0x624)             /* 0x1021C624 */

#define STH_EMICFG_BASE                 (g_sth_emicfg_base)                   /* 0x1021E000 */
#define STH_MFG_EMI1_GALS_SLV_DBG       (STH_EMICFG_BASE + 0x82C)             /* 0x1021E82C */
#define STH_MFG_EMI0_GALS_SLV_DBG       (STH_EMICFG_BASE + 0x830)             /* 0x1021E830 */
#define STH_APU_EMI1_GALS_SLV_DBG       (STH_EMICFG_BASE + 0x824)             /* 0x1021E824 */
#define STH_APU_EMI0_GALS_SLV_DBG       (STH_EMICFG_BASE + 0x828)             /* 0x1021E828 */

#define NTH_EMICFG_AO_MEM_BASE          (g_nth_emicfg_ao_mem_base)            /* 0x10270000 */
#define NTH_SLEEP_PROT_START            (NTH_EMICFG_AO_MEM_BASE + 0x000)      /* 0x10270000 */
#define NTH_GLITCH_PROT_RDY             (NTH_EMICFG_AO_MEM_BASE + 0x08C)      /* 0x1027008C */
#define NTH_M6M7_IDLE_BIT_EN_1          (NTH_EMICFG_AO_MEM_BASE + 0x228)      /* 0x10270228 */
#define NTH_M6M7_IDLE_BIT_EN_0          (NTH_EMICFG_AO_MEM_BASE + 0x22C)      /* 0x1027022C */

#define STH_EMICFG_AO_MEM_BASE          (g_sth_emicfg_ao_mem_base)            /* 0x1030E000 */
#define STH_SLEEP_PROT_START            (STH_EMICFG_AO_MEM_BASE + 0x000)      /* 0x1030E000 */
#define STH_GLITCH_PROT_RDY             (STH_EMICFG_AO_MEM_BASE + 0x08C)      /* 0x1030E08C */
#define STH_M6M7_IDLE_BIT_EN_1          (STH_EMICFG_AO_MEM_BASE + 0x228)      /* 0x1030E228 */
#define STH_M6M7_IDLE_BIT_EN_0          (STH_EMICFG_AO_MEM_BASE + 0x22C)      /* 0x1030E22C */

#define IFRBUS_AO_BASE                  (g_ifrbus_ao_base)                    /* 0x1002C000 */
#define IFR_MFGSYS_PROT_EN_STA_0        (IFRBUS_AO_BASE + 0x1A0)              /* 0x1002C1A0 */
#define IFR_MFGSYS_PROT_EN_W1S_0        (IFRBUS_AO_BASE + 0x1A4)              /* 0x1002C1A4 */
#define IFR_MFGSYS_PROT_EN_W1C_0        (IFRBUS_AO_BASE + 0x1A8)              /* 0x1002C1A8 */
#define IFR_MFGSYS_PROT_RDY_STA_0       (IFRBUS_AO_BASE + 0x1AC)              /* 0x1002C1AC */
#define IFR_EMISYS_PROTECT_EN_STA_0     (IFRBUS_AO_BASE + 0x100)              /* 0x1002C100 */
#define IFR_EMISYS_PROTECT_EN_W1S_0     (IFRBUS_AO_BASE + 0x104)              /* 0x1002C104 */
#define IFR_EMISYS_PROTECT_EN_W1C_0     (IFRBUS_AO_BASE + 0x108)              /* 0x1002C108 */
#define IFR_EMISYS_PROTECT_RDY_STA_0    (IFRBUS_AO_BASE + 0x10C)              /* 0x1002C10C */
#define IFR_EMISYS_PROTECT_EN_STA_1     (IFRBUS_AO_BASE + 0x120)              /* 0x1002C120 */
#define IFR_EMISYS_PROTECT_EN_W1S_1     (IFRBUS_AO_BASE + 0x124)              /* 0x1002C124 */
#define IFR_EMISYS_PROTECT_EN_W1C_1     (IFRBUS_AO_BASE + 0x128)              /* 0x1002C128 */
#define IFR_EMISYS_PROTECT_RDY_STA_1    (IFRBUS_AO_BASE + 0x12C)              /* 0x1002C12C */

#define INFRA_AO_DEBUG_CTRL_BASE        (g_infra_ao_debug_ctrl)               /* 0x10023000 */
#define INFRA_AO_BUS0_U_DEBUG_CTRL0     (INFRA_AO_DEBUG_CTRL_BASE + 0x000)    /* 0x10023000 */

#define INFRA_AO1_DEBUG_CTRL_BASE       (g_infra_ao1_debug_ctrl)              /* 0x1002B000 */
#define INFRA_AO1_BUS1_U_DEBUG_CTRL0    (INFRA_AO1_DEBUG_CTRL_BASE + 0x000)   /* 0x1002B000 */

#define NTH_EMI_AO_DEBUG_CTRL_BASE      (g_nth_emi_ao_debug_ctrl)             /* 0x10042000 */
#define NTH_EMI_AO_DEBUG_CTRL0          (NTH_EMI_AO_DEBUG_CTRL_BASE + 0x000)  /* 0x10042000 */

#define STH_EMI_AO_DEBUG_CTRL_BASE      (g_sth_emi_ao_debug_ctrl)             /* 0x10028000 */
#define STH_EMI_AO_DEBUG_CTRL0          (STH_EMI_AO_DEBUG_CTRL_BASE + 0x000)  /* 0x10028000 */

#define EFUSE_BASE                      (g_efuse_base)                        /* 0x11E80000 */
#define EFUSE_ASENSOR_RT                (EFUSE_BASE + 0x5CC)                  /* 0x11E805CC */
#define EFUSE_ASENSOR_HT                (EFUSE_BASE + 0x5D0)                  /* 0x11E805D0 */
#define EFUSE_ASENSOR_TEMPER            (EFUSE_BASE + 0x5DC)                  /* 0x11E805DC */
#define EFUSE_PTPOD21_SN                (EFUSE_BASE + 0x5D4)                  /* 0x11E805D4 */
#define EFUSE_PTPOD22_AVS               (EFUSE_BASE + 0x5D8)                  /* 0x11E805D8 */
#define EFUSE_PTPOD23_AVS               (EFUSE_BASE + 0x5DC)                  /* 0x11E805DC */
#define EFUSE_PTPOD24_AVS               (EFUSE_BASE + 0x5E0)                  /* 0x11E805E0 */
#define EFUSE_PTPOD25_AVS               (EFUSE_BASE + 0x5E4)                  /* 0x11E805E4 */
#define EFUSE_PTPOD26_AVS               (EFUSE_BASE + 0x5E8)                  /* 0x11E805E8 */
#define EFUSE_FAB_INFO5                 (EFUSE_BASE + 0x7B4)                  /* 0x11E807B4 */
#define EFUSE_FAB_INFO7                 (EFUSE_BASE + 0x7BC)                  /* 0x11E807BC */

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

#define MFG_IPS_BASE                    (g_mfg_ips_base)                      /* 0x13FE0000 */
#define MFG_IPS_01                      (MFG_IPS_BASE + 0x000)                /* 0x13FE0000 */
#define MFG_IPS_05                      (MFG_IPS_BASE + 0x010)                /* 0x13FE0010 */
#define MFG_IPS_06                      (MFG_IPS_BASE + 0x014)                /* 0x13FE0014 */
#define MFG_IPS_10                      (MFG_IPS_BASE + 0x024)                /* 0x13FE0024 */
#define MFG_IPS_12                      (MFG_IPS_BASE + 0x02C)                /* 0x13FE002C */
#define MFG_IPS_13                      (MFG_IPS_BASE + 0x030)                /* 0x13FE0030 */

#define EMI_BASE                        (g_emi_base)                          /* 0x10219000 */
#define EMI_MD_LAT_HRT_UGT_CNT          (EMI_BASE + 0x860)                    /* 0x10219860 */
#define EMI_MD_HRT_UGT_CNT              (EMI_BASE + 0x864)                    /* 0x10219864 */
#define EMI_DISP_HRT_UGT_CNT            (EMI_BASE + 0x868)                    /* 0x10219868 */
#define EMI_CAM_HRT_UGT_CNT             (EMI_BASE + 0x86C)                    /* 0x1021986C */
#define EMI_MD_WR_LAT_HRT_UGT_CNT       (EMI_BASE + 0x9A4)                    /* 0x102199A4 */
#define EMI_MDMCU_HIGH_LAT_UGT_CNT      (EMI_BASE + 0xCC4)                    /* 0x10219CC4 */
#define EMI_MDMCU_HIGH_WR_LAT_UGT_CNT   (EMI_BASE + 0xCCC)                    /* 0x10219CCC */

#define SUB_EMI_BASE                    (g_sub_emi_base)                      /* 0x1021D000 */
#define SEMI_MD_LAT_HRT_UGT_CNT         (SUB_EMI_BASE + 0x860)                /* 0x1021D860 */
#define SEMI_MD_HRT_UGT_CNT             (SUB_EMI_BASE + 0x864)                /* 0x1021D864 */
#define SEMI_DISP_HRT_UGT_CNT           (SUB_EMI_BASE + 0x868)                /* 0x1021D868 */
#define SEMI_CAM_HRT_UGT_CNT            (SUB_EMI_BASE + 0x86C)                /* 0x1021D86C */
#define SEMI_MD_WR_LAT_HRT_UGT_CNT      (SUB_EMI_BASE + 0x9A4)                /* 0x1021D9A4 */
#define SEMI_MDMCU_HIGH_LAT_UGT_CNT     (SUB_EMI_BASE + 0xCC4)                /* 0x1021DCC4 */
#define SEMI_MDMCU_HIGH_WR_LAT_UGT_CNT  (SUB_EMI_BASE + 0xCCC)                /* 0x1021DCCC */

#define NEMI_MI32_SMI_SUB_BASE          (g_nemi_mi32_smi_sub)                 /* 0x1025E000 */
#define NEMI_MI32_SMI_SUB_DEBUG_S0      (NEMI_MI32_SMI_SUB_BASE + 0x400)      /* 0x1025E400 */
#define NEMI_MI32_SMI_SUB_DEBUG_S1      (NEMI_MI32_SMI_SUB_BASE + 0x404)      /* 0x1025E404 */
#define NEMI_MI32_SMI_SUB_DEBUG_M0      (NEMI_MI32_SMI_SUB_BASE + 0x430)      /* 0x1025E430 */
#define NEMI_MI32_SMI_SUB_DEBUG_MISC    (NEMI_MI32_SMI_SUB_BASE + 0x440)      /* 0x1025E440 */

#define NEMI_MI33_SMI_SUB_BASE          (g_nemi_mi33_smi_sub)                 /* 0x1025F000 */
#define NEMI_MI33_SMI_SUB_DEBUG_S0      (NEMI_MI33_SMI_SUB_BASE + 0x400)      /* 0x1025F400 */
#define NEMI_MI33_SMI_SUB_DEBUG_S1      (NEMI_MI33_SMI_SUB_BASE + 0x404)      /* 0x1025F404 */
#define NEMI_MI33_SMI_SUB_DEBUG_M0      (NEMI_MI33_SMI_SUB_BASE + 0x430)      /* 0x1025F430 */
#define NEMI_MI33_SMI_SUB_DEBUG_MISC    (NEMI_MI33_SMI_SUB_BASE + 0x440)      /* 0x1025F440 */

#define SEMI_MI32_SMI_SUB_BASE          (g_semi_mi32_smi_sub)                 /* 0x10309000 */
#define SEMI_MI32_SMI_SUB_DEBUG_S0      (SEMI_MI32_SMI_SUB_BASE + 0x400)      /* 0x10309400 */
#define SEMI_MI32_SMI_SUB_DEBUG_S1      (SEMI_MI32_SMI_SUB_BASE + 0x404)      /* 0x10309404 */
#define SEMI_MI32_SMI_SUB_DEBUG_M0      (SEMI_MI32_SMI_SUB_BASE + 0x430)      /* 0x10309430 */
#define SEMI_MI32_SMI_SUB_DEBUG_MISC    (SEMI_MI32_SMI_SUB_BASE + 0x440)      /* 0x10309440 */

#define SEMI_MI33_SMI_SUB_BASE          (g_semi_mi33_smi_sub)                 /* 0x1030A000 */
#define SEMI_MI33_SMI_SUB_DEBUG_S0      (SEMI_MI33_SMI_SUB_BASE + 0x400)      /* 0x1030A400 */
#define SEMI_MI33_SMI_SUB_DEBUG_S1      (SEMI_MI33_SMI_SUB_BASE + 0x404)      /* 0x1030A404 */
#define SEMI_MI33_SMI_SUB_DEBUG_M0      (SEMI_MI33_SMI_SUB_BASE + 0x430)      /* 0x1030A430 */
#define SEMI_MI33_SMI_SUB_DEBUG_MISC    (SEMI_MI33_SMI_SUB_BASE + 0x440)      /* 0x1030A440 */

/**************************************************
 * MFGSYS Register Info
 **************************************************/
enum gpufreq_reg_info_idx {
	IDX_MFG_CG_CON                   = 0,
	IDX_MFG_DCM_CON_0                = 1,
	IDX_MFG_ASYNC_CON                = 2,
	IDX_MFG_ASYNC_CON3               = 3,
	IDX_MFG_GLOBAL_CON               = 4,
	IDX_MFG_AXCOHERENCE_CON          = 5,
	IDX_MFG_DUMMY_REG                = 6,
	IDX_MFG_SRAM_FUL_SEL_ULV         = 7,
	IDX_MFG_PLL_CON0                 = 8,
	IDX_MFG_PLL_CON1                 = 9,
	IDX_MFGSC_PLL_CON0               = 10,
	IDX_MFGSC_PLL_CON1               = 11,
	IDX_MFG_RPC_AO_CLK_CFG           = 12,
	IDX_MFG_RPC_MFG1_PWR_CON         = 13,
	IDX_MFG_RPC_MFG2_PWR_CON         = 14,
	IDX_MFG_RPC_MFG3_PWR_CON         = 15,
	IDX_MFG_RPC_MFG4_PWR_CON         = 16,
	IDX_MFG_RPC_MFG5_PWR_CON         = 17,
	IDX_MFG_RPC_MFG6_PWR_CON         = 18,
	IDX_MFG_RPC_MFG7_PWR_CON         = 19,
	IDX_MFG_RPC_MFG8_PWR_CON         = 20,
	IDX_MFG_RPC_MFG9_PWR_CON         = 21,
	IDX_MFG_RPC_MFG10_PWR_CON        = 22,
	IDX_MFG_RPC_MFG11_PWR_CON        = 23,
	IDX_MFG_RPC_MFG12_PWR_CON        = 24,
	IDX_MFG_RPC_MFG13_PWR_CON        = 25,
	IDX_MFG_RPC_MFG14_PWR_CON        = 26,
	IDX_MFG_RPC_MFG15_PWR_CON        = 27,
	IDX_MFG_RPC_MFG16_PWR_CON        = 28,
	IDX_MFG_RPC_MFG17_PWR_CON        = 29,
	IDX_MFG_RPC_MFG18_PWR_CON        = 30,
	IDX_MFG_RPC_MFG19_PWR_CON        = 31,
	IDX_MFG_RPC_SLP_PROT_EN_STA      = 32,
	IDX_SPM_SPM2GPUPM_CON            = 33,
	IDX_SPM_MFG0_PWR_CON             = 34,
	IDX_SPM_SOC_BUCK_ISO_CON         = 35,
	IDX_TOPCK_CLK_CFG_3              = 36,
	IDX_TOPCK_CLK_CFG_30             = 37,
	IDX_EFUSE_PTPOD21_SN             = 38,
	IDX_EFUSE_PTPOD22_AVS            = 39,
	IDX_EFUSE_PTPOD23_AVS            = 40,
	IDX_EFUSE_PTPOD24_AVS            = 41,
	IDX_EFUSE_PTPOD25_AVS            = 42,
	IDX_EFUSE_PTPOD26_AVS            = 43,
	IDX_EFUSE_FAB_INFO5              = 44,
	IDX_EFUSE_FAB_INFO7              = 45,
	IDX_NTH_MFG_EMI1_GALS_SLV_DBG    = 46,
	IDX_NTH_MFG_EMI0_GALS_SLV_DBG    = 47,
	IDX_STH_MFG_EMI1_GALS_SLV_DBG    = 48,
	IDX_STH_MFG_EMI0_GALS_SLV_DBG    = 49,
	IDX_NTH_M6M7_IDLE_BIT_EN_1       = 50,
	IDX_NTH_M6M7_IDLE_BIT_EN_0       = 51,
	IDX_STH_M6M7_IDLE_BIT_EN_1       = 52,
	IDX_STH_M6M7_IDLE_BIT_EN_0       = 53,
	IDX_IFR_MFGSYS_PROT_EN_STA_0     = 54,
	IDX_IFR_MFGSYS_PROT_RDY_STA_0    = 55,
	IDX_IFR_EMISYS_PROTECT_EN_STA_0  = 56,
	IDX_IFR_EMISYS_PROTECT_EN_STA_1  = 57,
	IDX_NTH_EMI_AO_DEBUG_CTRL0       = 58,
	IDX_STH_EMI_AO_DEBUG_CTRL0       = 59,
	IDX_INFRA_AO_BUS0_U_DEBUG_CTRL0  = 60,
	IDX_INFRA_AO1_BUS1_U_DEBUG_CTRL0 = 61,
};

#define NUM_MFGSYS_REG                  ARRAY_SIZE(g_reg_mfgsys)
static struct gpufreq_reg_info g_reg_mfgsys[] = {
	REGOP(0x13FBF000, 0), /* 0 */
	REGOP(0x13FBF010, 0), /* 1 */
	REGOP(0x13FBF020, 0), /* 2 */
	REGOP(0x13FBF02C, 0), /* 3 */
	REGOP(0x13FBF0B0, 0), /* 4 */
	REGOP(0x13FBF168, 0), /* 5 */
	REGOP(0x13FBF500, 0), /* 6 */
	REGOP(0x13FBF080, 0), /* 7 */
	REGOP(0x13FA0008, 0), /* 8 */
	REGOP(0x13FA000C, 0), /* 9 */
	REGOP(0x13FA0C08, 0), /* 10*/
	REGOP(0x13FA0C0C, 0), /* 11 */
	REGOP(0x13F91034, 0), /* 12 */
	REGOP(0x13F91070, 0), /* 13 */
	REGOP(0x13F910A0, 0), /* 14 */
	REGOP(0x13F910A4, 0), /* 15 */
	REGOP(0x13F910A8, 0), /* 16 */
	REGOP(0x13F910AC, 0), /* 17 */
	REGOP(0x13F910B0, 0), /* 18 */
	REGOP(0x13F910B4, 0), /* 19 */
	REGOP(0x13F910B8, 0), /* 20 */
	REGOP(0x13F910BC, 0), /* 21 */
	REGOP(0x13F910C0, 0), /* 22 */
	REGOP(0x13F910C4, 0), /* 23 */
	REGOP(0x13F910C8, 0), /* 24 */
	REGOP(0x13F910CC, 0), /* 25 */
	REGOP(0x13F910D0, 0), /* 26 */
	REGOP(0x13F910D4, 0), /* 27 */
	REGOP(0x13F910D8, 0), /* 28 */
	REGOP(0x13F910DC, 0), /* 29 */
	REGOP(0x13F910E0, 0), /* 30 */
	REGOP(0x13F910E4, 0), /* 31 */
	REGOP(0x13F91048, 0), /* 32 */
	REGOP(0x1C001410, 0), /* 33 */
	REGOP(0x1C001EE8, 0), /* 34 */
	REGOP(0x1C001F78, 0), /* 35 */
	REGOP(0x10000040, 0), /* 36 */
	REGOP(0x100001F0, 0), /* 37 */
	REGOP(0x11E805D4, 0), /* 38 */
	REGOP(0x11E805D8, 0), /* 39 */
	REGOP(0x11E805DC, 0), /* 40 */
	REGOP(0x11E805E0, 0), /* 41 */
	REGOP(0x11E805E4, 0), /* 42 */
	REGOP(0x11E805E8, 0), /* 43 */
	REGOP(0x11E807B4, 0), /* 44 */
	REGOP(0x11E807BC, 0), /* 45 */
	REGOP(0x1021C82C, 0), /* 46 */
	REGOP(0x1021C830, 0), /* 47 */
	REGOP(0x1021E82C, 0), /* 48 */
	REGOP(0x1021E830, 0), /* 49 */
	REGOP(0x10270228, 0), /* 50 */
	REGOP(0x1027022C, 0), /* 51 */
	REGOP(0x1030E228, 0), /* 52 */
	REGOP(0x1030E22C, 0), /* 53 */
	REGOP(0x1002C1A0, 0), /* 54 */
	REGOP(0x1002C1AC, 0), /* 55 */
	REGOP(0x1002C100, 0), /* 56 */
	REGOP(0x1002C120, 0), /* 57 */
	REGOP(0x10042000, 0), /* 58 */
	REGOP(0x10028000, 0), /* 59 */
	REGOP(0x10023000, 0), /* 60 */
	REGOP(0x1002B000, 0), /* 61 */
};

#endif /* __GPUFREQ_REG_MT6985_H__ */
