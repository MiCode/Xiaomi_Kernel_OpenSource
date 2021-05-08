/*
 * Copyright (C) 2018 MediaTek Inc.
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

#ifndef __MTK_DCM_AUTOGEN_H__
#define __MTK_DCM_AUTOGEN_H__

#include <mtk_dcm.h>

#if defined(__KERNEL__) && defined(CONFIG_OF)
/* TODO: Fix all base addresses. */
extern unsigned long dcm_infracfg_ao_base;
extern unsigned long dcm_mcucfg_base;
extern unsigned long dcm_mcucfg_phys_base;
#ifndef USE_DRAM_API_INSTEAD
extern unsigned long dcm_dramc0_ao_base;
extern unsigned long dcm_dramc1_ao_base;
extern unsigned long dcm_ddrphy0_ao_base;
extern unsigned long dcm_ddrphy1_ao_base;
#endif /* #ifndef USE_DRAM_API_INSTEAD */
extern unsigned long dcm_chn0_emi_base;
extern unsigned long dcm_chn1_emi_base;
extern unsigned long dcm_emi_base;

#define INFRACFG_AO_BASE	(dcm_infracfg_ao_base)
#define MCUCFG_BASE		(dcm_mcucfg_base)
#define MP0_CPUCFG_BASE		(MCUCFG_BASE)
#define MP1_CPUCFG_BASE		(MCUCFG_BASE + 0x200)
#define MCU_MISCCFG_BASE	(MCUCFG_BASE + 0x400)
#define MCU_MISC1CFG_BASE	(MCUCFG_BASE + 0x800)
#define DDRPHY0_AO_BASE	(dcm_ddrphy0_ao_base)
#define DRAMC0_AO_BASE	(dcm_dramc0_ao_base)
#define DDRPHY1_AO_BASE	(dcm_ddrphy1_ao_base)
#define DRAMC1_AO_BASE	(dcm_dramc1_ao_base)
#define CHN0_EMI_BASE	(dcm_chn0_emi_base)
#define CHN1_EMI_BASE	(dcm_chn1_emi_base)
#define EMI_BASE		(dcm_emi_base)
#else /* !(defined(__KERNEL__) && defined(CONFIG_OF)) */
#undef INFRACFG_AO_BASE
#undef EMI_BASE
#undef DRAMC_CH0_TOP0_BASE
#undef DRAMC_CH0_TOP1_BASE
#undef DRAMC_CH1_TOP0_BASE
#undef DRAMC_CH1_TOP1_BASE
#undef CHN0_EMI_BASE
#undef CHN1_EMI_BASE
#undef MP0_CPUCFG_BASE
#undef MP1_CPUCFG_BASE
#undef MCU_MISCCFG_BASE
#undef MCU_MISC1CFG_BASE

/* Base */
#define INFRACFG_AO_BASE 0x10001000
#define SECURITY_AO_BASE 0x1001a000
#define MP0_CPUCFG_BASE 0x10200000
#define MCUCFG_BASE 0x10200000
#define MP1_CPUCFG_BASE 0x10200200
#define MCU_MISCCFG_BASE 0x10200400
#define CHN0_EMI_BASE 0x1022d000
#define CHN1_EMI_BASE 0x10235000
#define GCE_BASE 0x10238000
#define AUDIO_BASE 0x11220000
#define EFUSEC_BASE 0x11c50000
#define MFGCFG_BASE 0x13000000
#define MMSYS_CONFIG_BASE 0x14000000
#define SMI_COMMON_BASE 0x14002000
#define SMI_LARB0_BASE 0x14003000
#define SMI_LARB2_BASE 0x15021000
#define SMI_LARB1_BASE 0x17010000
#define VENC_BASE 0x17020000
#define JPGENC_BASE 0x17030000
#define SMI_LARB3_BASE 0x1a002000
#define SECURITY_AO_BASE 0x1001a000
#define MP0_CPUCFG_BASE 0x10200000
#define MP1_CPUCFG_BASE 0x10200200
#define MCU_MISCCFG_BASE 0x10200400
#define CHN0_EMI_BASE 0x1022d000
#define CHN1_EMI_BASE 0x10235000
#define MFGCFG_BASE 0x13000000
#endif /* #if defined(__KERNEL__) && defined(CONFIG_OF) */

/* Register Definition */
#define INFRA_BUS_DCM_CTRL (INFRACFG_AO_BASE + 0x70)
#define PERI_BUS_DCM_CTRL (INFRACFG_AO_BASE + 0x74)
#define MEM_DCM_CTRL (INFRACFG_AO_BASE + 0x78)
#define DFS_MEM_DCM_CTRL (INFRACFG_AO_BASE + 0x7c)
#define P2P_RX_CLK_ON (INFRACFG_AO_BASE + 0xa0)
#define INFRA_MISC (INFRACFG_AO_BASE + 0xf00)
#define DXCC_NEW_HWDCM_CFG (SECURITY_AO_BASE + 0x208)
#define MP0_CPUCFG_MP0_RGU_DCM_CONFIG (MP0_CPUCFG_BASE + 0x88)
#define MP1_CPUCFG_MP1_RGU_DCM_CONFIG (MP1_CPUCFG_BASE + 0x88)
#define L2C_SRAM_CTRL (MCU_MISCCFG_BASE + 0x248)
#define CCI_CLK_CTRL (MCU_MISCCFG_BASE + 0x260)
#define BUS_FABRIC_DCM_CTRL (MCU_MISCCFG_BASE + 0x268)
#define MCU_MISC_DCM_CTRL (MCU_MISCCFG_BASE + 0x26c)
#define CCI_ADB400_DCM_CONFIG (MCU_MISCCFG_BASE + 0x340)
#define SYNC_DCM_CONFIG (MCU_MISCCFG_BASE + 0x344)
#define SYNC_DCM_CLUSTER_CONFIG (MCU_MISCCFG_BASE + 0x34c)
#define MP_GIC_RGU_SYNC_DCM (MCU_MISCCFG_BASE + 0x358)
#define MP0_PLL_DIVIDER_CFG (MCU_MISCCFG_BASE + 0x3a0)
#define MP1_PLL_DIVIDER_CFG (MCU_MISCCFG_BASE + 0x3a4)
#define BUS_PLL_DIVIDER_CFG (MCU_MISCCFG_BASE + 0x3c0)
#define MCSIA_DCM_EN (MCUCFG_BASE + 0xb60)
#define CHN0_EMI_CHN_EMI_CONB (CHN0_EMI_BASE + 0x8)
#define CHN1_EMI_CHN_EMI_CONB (CHN1_EMI_BASE + 0x8)
#define GCE_CTL_INT0 (GCE_BASE + 0xf0)
#define AUDIO_TOP_CON0 (AUDIO_BASE + 0x0)
#define EFUSEC_DCM_ON (EFUSEC_BASE + 0x480)
#define MFGCFG_MFG_DCM_CON_0 (MFGCFG_BASE + 0xffe010)
#define MMSYS_HW_DCM_1ST_DIS0 (MMSYS_CONFIG_BASE + 0x120)
#define MMSYS_HW_DCM_2ND_DIS0 (MMSYS_CONFIG_BASE + 0x130)
#define SMI_COMMON_SMI_DCM (SMI_COMMON_BASE + 0x300)
#define SMI_LARB0_CON_SET (SMI_LARB0_BASE + 0x14)
#define SMI_LARB0_CON_CLR (SMI_LARB0_BASE + 0x18)
#define SMI_LARB2_CON_SET (SMI_LARB2_BASE + 0x14)
#define SMI_LARB2_CON_CLR (SMI_LARB2_BASE + 0x18)
#define SMI_LARB1_CON_SET (SMI_LARB1_BASE + 0x14)
#define SMI_LARB1_CON_CLR (SMI_LARB1_BASE + 0x18)
#define VENC_CLK_DCM_CTRL (VENC_BASE + 0xf4)
#define VENC_CLK_CG_CTRL (VENC_BASE + 0x130)
#define JPGENC_DCM_CTRL (JPGENC_BASE + 0x300)
#define SMI_LARB3_CON_SET (SMI_LARB3_BASE + 0x14)
#define SMI_LARB3_CON_CLR (SMI_LARB3_BASE + 0x18)

/* INFRACFG_AO */
bool dcm_infracfg_ao_audio_is_on(void);
void dcm_infracfg_ao_audio(int on);
bool dcm_infracfg_ao_dfs_mem_is_on(void);
void dcm_infracfg_ao_dfs_mem(int on);
bool dcm_infracfg_ao_icusb_is_on(void);
void dcm_infracfg_ao_icusb(int on);
bool dcm_infracfg_ao_infra_md_is_on(void);
void dcm_infracfg_ao_infra_md(int on);
bool dcm_infracfg_ao_infra_mem_is_on(void);
void dcm_infracfg_ao_infra_mem(int on);
bool dcm_infracfg_ao_infra_peri_is_on(void);
void dcm_infracfg_ao_infra_peri(int on);
bool dcm_infracfg_ao_p2p_dsi_csi_is_on(void);
void dcm_infracfg_ao_p2p_dsi_csi(int on);
bool dcm_infracfg_ao_pmic_is_on(void);
void dcm_infracfg_ao_pmic(int on);
bool dcm_infracfg_ao_ssusb_is_on(void);
void dcm_infracfg_ao_ssusb(int on);
bool dcm_infracfg_ao_usb_is_on(void);
void dcm_infracfg_ao_usb(int on);
#if 0
/* SECURITY_AO */
bool dcm_security_ao_infra_dxcc_is_on(void);
void dcm_security_ao_infra_dxcc(int on);
#endif
/* MCUCFG */
bool dcm_mcucfg_mcsi_dcm_is_on(void);
void dcm_mcucfg_mcsi_dcm(int on);
/* MP0_CPUCFG */
bool dcm_mp0_cpucfg_mp0_rgu_dcm_is_on(void);
void dcm_mp0_cpucfg_mp0_rgu_dcm(int on);
/* MP1_CPUCFG */
bool dcm_mp1_cpucfg_mp1_rgu_dcm_is_on(void);
void dcm_mp1_cpucfg_mp1_rgu_dcm(int on);
/* MCU_MISCCFG */
bool dcm_mcu_misccfg_adb400_dcm_is_on(void);
void dcm_mcu_misccfg_adb400_dcm(int on);
bool dcm_mcu_misccfg_bus_arm_pll_divider_dcm_is_on(void);
void dcm_mcu_misccfg_bus_arm_pll_divider_dcm(int on);
bool dcm_mcu_misccfg_bus_sync_dcm_is_on(void);
void dcm_mcu_misccfg_bus_sync_dcm(int on);
bool dcm_mcu_misccfg_bus_clock_dcm_is_on(void);
void dcm_mcu_misccfg_bus_clock_dcm(int on);
bool dcm_mcu_misccfg_bus_fabric_dcm_is_on(void);
void dcm_mcu_misccfg_bus_fabric_dcm(int on);
bool dcm_mcu_misccfg_gic_sync_dcm_is_on(void);
void dcm_mcu_misccfg_gic_sync_dcm(int on);
bool dcm_mcu_misccfg_l2_shared_dcm_is_on(void);
void dcm_mcu_misccfg_l2_shared_dcm(int on);
bool dcm_mcu_misccfg_mp0_arm_pll_divider_dcm_is_on(void);
void dcm_mcu_misccfg_mp0_arm_pll_divider_dcm(int on);
bool dcm_mcu_misccfg_mp0_stall_dcm_is_on(void);
void dcm_mcu_misccfg_mp0_stall_dcm(int on);
bool dcm_mcu_misccfg_mp0_sync_dcm_enable_is_on(void);
void dcm_mcu_misccfg_mp0_sync_dcm_enable(int on);
bool dcm_mcu_misccfg_mp1_arm_pll_divider_dcm_is_on(void);
void dcm_mcu_misccfg_mp1_arm_pll_divider_dcm(int on);
bool dcm_mcu_misccfg_mp1_stall_dcm_is_on(void);
void dcm_mcu_misccfg_mp1_stall_dcm(int on);
bool dcm_mcu_misccfg_mp1_sync_dcm_enable_is_on(void);
void dcm_mcu_misccfg_mp1_sync_dcm_enable(int on);
bool dcm_mcu_misccfg_mcu_misc_dcm_is_on(void);
void dcm_mcu_misccfg_mcu_misc_dcm(int on);
/* CHN0_EMI */
bool dcm_chn0_emi_dcm_emi_group_is_on(void);
void dcm_chn0_emi_dcm_emi_group(int on);
/* CHN1_EMI */
bool dcm_chn1_emi_dcm_emi_group_is_on(void);
void dcm_chn1_emi_dcm_emi_group(int on);
#if 0
/* GCE */
bool dcm_gce_gce_dcm_is_on(void);
void dcm_gce_gce_dcm(int on);
/* AUDIO */
bool dcm_audio_audio_bus_is_on(void);
void dcm_audio_audio_bus(int on);
/* EFUSEC */
bool dcm_efusec_efuse_dcm_is_on(void);
void dcm_efusec_efuse_dcm(int on);
/* MFGCFG */
bool dcm_mfgcfg_meg_is_on(void);
void dcm_mfgcfg_meg(int on);
bool dcm_mfgcfg_mfg_is_on(void);
void dcm_mfgcfg_mfg(int on);
/* MMSYS_CONFIG */
bool dcm_mmsys_config_mmsys_config_is_on(void);
void dcm_mmsys_config_mmsys_config(int on);
/* SMI_COMMON */
bool dcm_smi_common_smi_comm_is_on(void);
void dcm_smi_common_smi_comm(int on);
/* SMI_LARB0 */
bool dcm_smi_larb0_smi_larb0_is_on(void);
void dcm_smi_larb0_smi_larb0(int on);
/* SMI_LARB2 */
bool dcm_smi_larb2_smi_larb2_is_on(void);
void dcm_smi_larb2_smi_larb2(int on);
/* SMI_LARB1 */
bool dcm_smi_larb1_smi_larb1_is_on(void);
void dcm_smi_larb1_smi_larb1(int on);
/* VENC */
bool dcm_venc_venc_cg_ctrl_is_on(void);
void dcm_venc_venc_cg_ctrl(int on);
bool dcm_venc_venc_dcm_ctrl_is_on(void);
void dcm_venc_venc_dcm_ctrl(int on);
/* JPGENC */
bool dcm_jpgenc_jpgenc_is_on(void);
void dcm_jpgenc_jpgenc(int on);
/* SMI_LARB3 */
bool dcm_smi_larb3_smi_larb3_is_on(void);
void dcm_smi_larb3_smi_larb3(int on);
#endif

#endif /* __MTK_DCM_AUTOGEN_H__ */

