/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_DCM_AUTOGEN_H__
#define __MTK_DCM_AUTOGEN_H__

#include <mtk_dcm.h>

#if defined(__KERNEL__) && defined(CONFIG_OF)
/* TODO: Fix all base addresses. */
extern unsigned long dcm_infracfg_ao_base;
extern unsigned long dcm_pwrap_base;
extern unsigned long dcm_mcucfg_base;
extern unsigned long dcm_cpccfg_rg_base;

extern unsigned long dcm_infracfg_ao_mem_base;

extern unsigned long dcm_dramc_ch0_top0_ao_base;
extern unsigned long dcm_dramc_ch1_top0_ao_base;

extern unsigned long dcm_dramc_ch0_top5_ao_base;
extern unsigned long dcm_dramc_ch1_top5_ao_base;

extern unsigned long dcm_ch0_emi_base;
extern unsigned long dcm_emi_base;

/* not used actually in MT6779 */
extern unsigned long dcm_mm_iommu_base;
extern unsigned long dcm_vpu_iommu_base;
extern unsigned long dcm_sspm_base;
extern unsigned long dcm_audio_base;
extern unsigned long dcm_msdc1_base;


#ifndef USE_DRAM_API_INSTEAD
extern unsigned long dcm_ddrphy1_ao_base;
#endif /* #ifndef USE_DRAM_API_INSTEAD */
#define INFRACFG_AO_BASE	(dcm_infracfg_ao_base)
#define PWRAP_BASE		(dcm_pwrap_base)
#define MP_CPUSYS_TOP_BASE	(dcm_mcucfg_base)
#define CPCCFG_REG_BASE		(dcm_cpccfg_rg_base)

#define INFRACFG_AO_MEM_BASE	(dcm_infracfg_ao_mem_base)

/* dram staff */
/* dramc */
#define DRAMC_CH0_TOP0_BASE	(dcm_dramc_ch0_top0_ao_base)
#define DRAMC_CH0_TOP5_BASE	(dcm_dramc_ch0_top5_ao_base)

/* ddrphy */
#define DRAMC_CH1_TOP0_BASE	(dcm_dramc_ch1_top0_ao_base)
#define DRAMC_CH1_TOP5_BASE	(dcm_dramc_ch1_top5_ao_base)

/* emi */
#define CHN0_EMI_BASE		(dcm_ch0_emi_base)
#define EMI_BASE		(dcm_emi_base)

/* not used actually in MT6779 */
#define MM_IOMMU_BASE		(dcm_mm_iommu_base)
#define VPU_IOMMU_BASE		(dcm_vpu_iommu_base)
#define SSPM_BASE		(dcm_sspm_base)
#define AUDIO_BASE		(dcm_audio_base)
#define MSDC1_BASE		(dcm_msdc1_base)


#else /* !(defined(__KERNEL__) && defined(CONFIG_OF)) */

/* Here below used in CTP and lk for references. */
#undef INFRACFG_AO_BASE
#undef INFRACFG_AO_MEM_BASE
#undef PWRAP_BASE
#undef EMI_BASE
#undef MM_IOMMU_BASE		/* not used */
#undef DRAMC_CH0_TOP0_BASE
#undef CHN0_EMI_BASE
#undef DRAMC_CH0_TOP5_BASE
#undef DRAMC_CH1_TOP0_BASE
#undef DRAMC_CH1_TOP5_BASE
#undef VPU_IOMMU_BASE		/* not used */
#undef SSPM_BASE		/* not used */
#undef AUDIO_BASE		/* not used */
#undef MSDC1_BASE		/* not used */
#undef MP_CPUSYS_TOP_BASE
#undef CPCCFG_REG_BASE

/* Base */
#define INFRACFG_AO_BASE 0x10001000
#define INFRACFG_AO_MEM_BASE 0x10002000
#define PWRAP_BASE 0x1000d000
#define EMI_BASE 0x10219000
#define MM_IOMMU_BASE 0x10220000
#define DRAMC_CH0_TOP0_BASE 0x10230000
#define CHN0_EMI_BASE 0x10235000
#define DRAMC_CH0_TOP5_BASE 0x10238000
#define DRAMC_CH1_TOP0_BASE 0x10240000
#define DRAMC_CH1_TOP5_BASE 0x10248000
#define VPU_IOMMU_BASE 0x1024f000
#define SSPM_BASE 0x10400000
#define AUDIO_BASE 0x11210000
#define MSDC1_BASE 0x11240000
#define MP_CPUSYS_TOP_BASE 0xc538000
#define CPCCFG_REG_BASE 0xc53a800
#endif /* #if defined(__KERNEL__) && defined(CONFIG_OF) */

/* Register Definition */
#define CPU_PLLDIV_CFG0 (MP_CPUSYS_TOP_BASE + 0x22a0)
#define CPU_PLLDIV_CFG1 (MP_CPUSYS_TOP_BASE + 0x22a4)
#define CPU_PLLDIV_CFG2 (MP_CPUSYS_TOP_BASE + 0x22a8)
#define BUS_PLLDIV_CFG (MP_CPUSYS_TOP_BASE + 0x22e0)
#define MCSI_CFG2 (MP_CPUSYS_TOP_BASE + 0x2418)
#define MCSI_DCM0 (MP_CPUSYS_TOP_BASE + 0x2440)
#define MP_ADB_DCM_CFG0 (MP_CPUSYS_TOP_BASE + 0x2500)
#define MP_ADB_DCM_CFG2 (MP_CPUSYS_TOP_BASE + 0x2508)
#define MP_ADB_DCM_CFG4 (MP_CPUSYS_TOP_BASE + 0x2510)
#define MP_MISC_DCM_CFG0 (MP_CPUSYS_TOP_BASE + 0x2518)
#define MCUSYS_DCM_CFG0 (MP_CPUSYS_TOP_BASE + 0x25c0)
#define EMI_WFIFO (CPCCFG_REG_BASE + 0x100)
#define SLOW_CK_CFG (CPCCFG_REG_BASE + 0x120)
#define MP0_DCM_CFG0 (MP_CPUSYS_TOP_BASE + 0x4880)
#define MP0_DCM_CFG7 (MP_CPUSYS_TOP_BASE + 0x489c)
#define INFRA_BUS_DCM_CTRL (INFRACFG_AO_BASE + 0x70)
#define PERI_BUS_DCM_CTRL (INFRACFG_AO_BASE + 0x74)
#define MEM_DCM_CTRL (INFRACFG_AO_BASE + 0x78)
#define P2P_RX_CLK_ON (INFRACFG_AO_BASE + 0xa0)
#define INFRA_AXIMEM_IDLE_BIT_EN_0 (INFRACFG_AO_BASE + 0xa30)
#define INFRA_EMI_DCM_CFG0 (INFRACFG_AO_MEM_BASE + 0x28)
#define INFRA_EMI_DCM_CFG1 (INFRACFG_AO_MEM_BASE + 0x2c)
#define INFRA_EMI_DCM_CFG3 (INFRACFG_AO_MEM_BASE + 0x34)
#define TOP_CK_ANCHOR_CFG (INFRACFG_AO_MEM_BASE + 0x38)
#define PMIC_WRAP_DCM_EN (PWRAP_BASE + 0x1ec)
#define EMI_CONM (EMI_BASE + 0x60)
#define EMI_CONN (EMI_BASE + 0x68)
#define EMI_THRO_CTRL0 (EMI_BASE + 0x830)
#define MM_MMU_DCM_DIS (MM_IOMMU_BASE + 0x50)
#define DRAMC_CH0_TOP0_DRAMC_PD_CTRL (DRAMC_CH0_TOP0_BASE + 0x38)
#define DRAMC_CH0_TOP0_CLKAR (DRAMC_CH0_TOP0_BASE + 0x3c)
#define CHN0_EMI_CHN_EMI_CONB (CHN0_EMI_BASE + 0x8)
#define DRAMC_CH0_TOP5_MISC_CG_CTRL0 (DRAMC_CH0_TOP5_BASE + 0x284)
#define DRAMC_CH0_TOP5_MISC_CG_CTRL2 (DRAMC_CH0_TOP5_BASE + 0x28c)
#define DRAMC_CH0_TOP5_MISC_CTRL2 (DRAMC_CH0_TOP5_BASE + 0x2a4)
#define DRAMC_CH1_TOP0_DRAMC_PD_CTRL (DRAMC_CH1_TOP0_BASE + 0x38)
#define DRAMC_CH1_TOP0_CLKAR (DRAMC_CH1_TOP0_BASE + 0x3c)
#define DRAMC_CH1_TOP5_MISC_CG_CTRL0 (DRAMC_CH1_TOP5_BASE + 0x284)
#define DRAMC_CH1_TOP5_MISC_CG_CTRL2 (DRAMC_CH1_TOP5_BASE + 0x28c)
#define DRAMC_CH1_TOP5_MISC_CTRL2 (DRAMC_CH1_TOP5_BASE + 0x2a4)
#define VPU_MMU_DCM_DIS (VPU_IOMMU_BASE + 0x50)
#define SSPM_MCLK_DIV (SSPM_BASE + 0x43004)
#define SSPM_DCM_CTRL (SSPM_BASE + 0x43008)
#define AUDIO_TOP_CON0 (AUDIO_BASE + 0x0)
#define MSDC1_PATCH_BIT1 (MSDC1_BASE + 0xb4)

/* INFRACFG_AO */
bool dcm_infracfg_ao_infra_bus_dcm_is_on(void);
void dcm_infracfg_ao_infra_bus_dcm(int on);
bool dcm_infracfg_ao_infra_rx_p2p_dcm_is_on(void);
void dcm_infracfg_ao_infra_rx_p2p_dcm(int on);
bool dcm_infracfg_ao_peri_bus_dcm_is_on(void);
void dcm_infracfg_ao_peri_bus_dcm(int on);
bool dcm_infracfg_ao_peri_module_dcm_is_on(void);
void dcm_infracfg_ao_peri_module_dcm(int on);
/* INFRACFG_AO_MEM */
bool dcm_infracfg_ao_mem_dcm_emi_group_is_on(void);
void dcm_infracfg_ao_mem_dcm_emi_group(int on);
/* PWRAP */
bool dcm_pwrap_pmic_wrap_is_on(void);
void dcm_pwrap_pmic_wrap(int on);
/* EMI */
bool dcm_emi_emi_dcm_is_on(void);
void dcm_emi_emi_dcm(int on);
/* MM_IOMMU */
bool dcm_mm_iommu_mm_mmu_dcm_cfg_is_on(void);
void dcm_mm_iommu_mm_mmu_dcm_cfg(int on);
/* DRAMC_CH0_TOP0 */
bool dcm_dramc_ch0_top0_ddrphy_is_on(void);
void dcm_dramc_ch0_top0_ddrphy(int on);
/* CHN0_EMI */
bool dcm_chn0_emi_chn_emi_dcm_is_on(void);
void dcm_chn0_emi_chn_emi_dcm(int on);
/* DRAMC_CH0_TOP5 */
bool dcm_dramc_ch0_top5_ddrphy_is_on(void);
void dcm_dramc_ch0_top5_ddrphy(int on);
/* DRAMC_CH1_TOP0 */
bool dcm_dramc_ch1_top0_ddrphy_is_on(void);
void dcm_dramc_ch1_top0_ddrphy(int on);
/* DRAMC_CH1_TOP5 */
bool dcm_dramc_ch1_top5_ddrphy_is_on(void);
void dcm_dramc_ch1_top5_ddrphy(int on);
/* VPU_IOMMU */
bool dcm_vpu_iommu_vpu_mmu_dcm_cfg_is_on(void);
void dcm_vpu_iommu_vpu_mmu_dcm_cfg(int on);
/* SSPM */
bool dcm_sspm_sspm_dcm_is_on(void);
void dcm_sspm_sspm_dcm(int on);
/* AUDIO */
bool dcm_audio_aud_mas_ahb_ck_dcm_is_on(void);
void dcm_audio_aud_mas_ahb_ck_dcm(int on);
/* MSDC1 */
bool dcm_msdc1_dcmen_dcm_is_on(void);
void dcm_msdc1_dcmen_dcm(int on);
bool dcm_msdc1_hgdmacken_dcm_is_on(void);
void dcm_msdc1_hgdmacken_dcm(int on);
bool dcm_msdc1_macmdcken_dcm_is_on(void);
void dcm_msdc1_macmdcken_dcm(int on);
bool dcm_msdc1_mpsccken_dcm_is_on(void);
void dcm_msdc1_mpsccken_dcm(int on);
bool dcm_msdc1_mrctlcken_dcm_is_on(void);
void dcm_msdc1_mrctlcken_dcm(int on);
bool dcm_msdc1_msdcken_dcm_is_on(void);
void dcm_msdc1_msdcken_dcm(int on);
bool dcm_msdc1_mshbfcken_dcm_is_on(void);
void dcm_msdc1_mshbfcken_dcm(int on);
bool dcm_msdc1_mspccken_dcm_is_on(void);
void dcm_msdc1_mspccken_dcm(int on);
bool dcm_msdc1_mvoldtcken_dcm_is_on(void);
void dcm_msdc1_mvoldtcken_dcm(int on);
bool dcm_msdc1_mwctlcken_dcm_is_on(void);
void dcm_msdc1_mwctlcken_dcm(int on);
/* MP_CPUSYS_TOP */
bool dcm_mp_cpusys_top_adb_dcm_is_on(void);
void dcm_mp_cpusys_top_adb_dcm(int on);
bool dcm_mp_cpusys_top_apb_dcm_is_on(void);
void dcm_mp_cpusys_top_apb_dcm(int on);
bool dcm_mp_cpusys_top_bus_pll_div_dcm_is_on(void);
void dcm_mp_cpusys_top_bus_pll_div_dcm(int on);
bool dcm_mp_cpusys_top_core_stall_dcm_is_on(void);
void dcm_mp_cpusys_top_core_stall_dcm(int on);
bool dcm_mp_cpusys_top_cpubiu_dbg_cg_is_on(void);
void dcm_mp_cpusys_top_cpubiu_dbg_cg(int on);
bool dcm_mp_cpusys_top_cpubiu_dcm_is_on(void);
void dcm_mp_cpusys_top_cpubiu_dcm(int on);
bool dcm_mp_cpusys_top_cpu_pll_div_0_dcm_is_on(void);
void dcm_mp_cpusys_top_cpu_pll_div_0_dcm(int on);
bool dcm_mp_cpusys_top_cpu_pll_div_1_dcm_is_on(void);
void dcm_mp_cpusys_top_cpu_pll_div_1_dcm(int on);
bool dcm_mp_cpusys_top_cpu_pll_div_2_dcm_is_on(void);
void dcm_mp_cpusys_top_cpu_pll_div_2_dcm(int on);
bool dcm_mp_cpusys_top_fcm_stall_dcm_is_on(void);
void dcm_mp_cpusys_top_fcm_stall_dcm(int on);
bool dcm_mp_cpusys_top_last_cor_idle_dcm_is_on(void);
void dcm_mp_cpusys_top_last_cor_idle_dcm(int on);
bool dcm_mp_cpusys_top_misc_dcm_is_on(void);
void dcm_mp_cpusys_top_misc_dcm(int on);
bool dcm_mp_cpusys_top_mp0_qdcm_is_on(void);
void dcm_mp_cpusys_top_mp0_qdcm(int on);
/* CPCCFG_REG */
bool dcm_cpccfg_reg_emi_wfifo_is_on(void);
void dcm_cpccfg_reg_emi_wfifo(int on);
bool dcm_cpccfg_reg_mp_stall_dcm_is_on(void);
void dcm_cpccfg_reg_mp_stall_dcm(int on);

#endif /* __MTK_DCM_AUTOGEN_H__ */

