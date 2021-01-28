/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MT6761_DCM_AUTOGEN_H__
#define __MT6761_DCM_AUTOGEN_H__

#include <mtk_dcm.h>

#if defined(__KERNEL__) && defined(CONFIG_OF)
extern unsigned long dcm_infracfg_ao_base;
extern unsigned long dcm_mcucfg_base;
extern unsigned long dcm_mcucfg_phys_base;
extern unsigned long dcm_dramc0_ao_base;
extern unsigned long dcm_dramc1_ao_base;
extern unsigned long dcm_ddrphy0_ao_base;
extern unsigned long dcm_ddrphy1_ao_base;
extern unsigned long dcm_chn0_emi_base;
extern unsigned long dcm_chn1_emi_base;
/* extern unsigned long dcm_emi_base; */

#define INFRACFG_AO_BASE	(dcm_infracfg_ao_base)
#define MCUCFG_BASE		(dcm_mcucfg_base)
#define MP0_CPUCFG_BASE		(MCUCFG_BASE)
#define MCU_MISCCFG_BASE	(MCUCFG_BASE + 0x400)
#define DRAMC_CH0_TOP0_BASE	(dcm_ddrphy0_ao_base)
#define DRAMC_CH0_TOP1_BASE	(dcm_dramc0_ao_base)
#define DRAMC_CH1_TOP0_BASE	(dcm_ddrphy1_ao_base)
#define DRAMC_CH1_TOP1_BASE	(dcm_dramc1_ao_base)
#define CHN0_EMI_BASE		(dcm_chn0_emi_base)
#define CHN1_EMI_BASE		(dcm_chn1_emi_base)
/* #define EMI_BASE		(dcm_emi_base) */
#else /* !(defined(__KERNEL__) && defined(CONFIG_OF)) */
#undef INFRACFG_AO_BASE
#undef MCUCFG_BASE
#undef MP0_CPUCFG_BASE
#undef MCU_MISCCFG_BASE
#undef DRAMC_CH0_TOP0_BASE
#undef DRAMC_CH0_TOP1_BASE
#undef CHN0_EMI_BASE
#undef DRAMC_CH1_TOP0_BASE
#undef DRAMC_CH1_TOP1_BASE
#undef CHN1_EMI_BASE

/* Base */
#define INFRACFG_AO_BASE 0x10001000
#define MCUCFG_BASE 0x10200000
#define MP0_CPUCFG_BASE 0x10200000
#define MCU_MISCCFG_BASE 0x10200400
#define DRAMC_CH0_TOP0_BASE 0x10228000
#define DRAMC_CH0_TOP1_BASE 0x1022a000
#define CHN0_EMI_BASE 0x1022d000
#define DRAMC_CH1_TOP0_BASE 0x10230000
#define DRAMC_CH1_TOP1_BASE 0x10232000
#define CHN1_EMI_BASE 0x10235000
#endif /* #if defined(__KERNEL__) && defined(CONFIG_OF) */

/* Register Definition */
#define INFRA_BUS_DCM_CTRL (INFRACFG_AO_BASE + 0x70)
#define PERI_BUS_DCM_CTRL (INFRACFG_AO_BASE + 0x74)
#define MEM_DCM_CTRL (INFRACFG_AO_BASE + 0x78)
#define DFS_MEM_DCM_CTRL (INFRACFG_AO_BASE + 0x7c)
#define P2P_RX_CLK_ON (INFRACFG_AO_BASE + 0xa0)
#define MP0_CPUCFG_MP0_RGU_DCM_CONFIG (MP0_CPUCFG_BASE + 0x88)
#define L2C_SRAM_CTRL (MCU_MISCCFG_BASE + 0x248)
#define CCI_CLK_CTRL (MCU_MISCCFG_BASE + 0x260)
#define BUS_FABRIC_DCM_CTRL (MCU_MISCCFG_BASE + 0x268)
#define MCU_MISC_DCM_CTRL (MCU_MISCCFG_BASE + 0x26c)
#define CCI_ADB400_DCM_CONFIG (MCU_MISCCFG_BASE + 0x340)
#define SYNC_DCM_CONFIG (MCU_MISCCFG_BASE + 0x344)
#define MP_GIC_RGU_SYNC_DCM (MCU_MISCCFG_BASE + 0x358)
#define BUS_PLL_DIVIDER_CFG (MCU_MISCCFG_BASE + 0x3c0)
#define DRAMC_CH0_TOP0_MISC_CG_CTRL0 (DRAMC_CH0_TOP0_BASE + 0x284)
#define DRAMC_CH0_TOP0_MISC_CG_CTRL2 (DRAMC_CH0_TOP0_BASE + 0x28c)
#define DRAMC_CH0_TOP0_MISC_CTRL3 (DRAMC_CH0_TOP0_BASE + 0x2a8)
#define DRAMC_CH0_TOP1_DRAMC_PD_CTRL (DRAMC_CH0_TOP1_BASE + 0x38)
#define DRAMC_CH0_TOP1_CLKAR (DRAMC_CH0_TOP1_BASE + 0x3c)
#define CHN0_EMI_CHN_EMI_CONB (CHN0_EMI_BASE + 0x8)
#define DRAMC_CH1_TOP0_MISC_CG_CTRL0 (DRAMC_CH1_TOP0_BASE + 0x284)
#define DRAMC_CH1_TOP0_MISC_CG_CTRL2 (DRAMC_CH1_TOP0_BASE + 0x28c)
#define DRAMC_CH1_TOP0_MISC_CTRL3 (DRAMC_CH1_TOP0_BASE + 0x2a8)
#define DRAMC_CH1_TOP1_DRAMC_PD_CTRL (DRAMC_CH1_TOP1_BASE + 0x38)
#define DRAMC_CH1_TOP1_CLKAR (DRAMC_CH1_TOP1_BASE + 0x3c)
#define CHN1_EMI_CHN_EMI_CONB (CHN1_EMI_BASE + 0x8)

/* INFRACFG_AO */
bool dcm_infracfg_ao_dcm_infrabus_group_is_on(void);
void dcm_infracfg_ao_dcm_infrabus_group(int on);
bool dcm_infracfg_ao_dcm_mem_group_is_on(void);
void dcm_infracfg_ao_dcm_mem_group(int on);
bool dcm_infracfg_ao_dcm_peribus_group_is_on(void);
void dcm_infracfg_ao_dcm_peribus_group(int on);
bool dcm_infracfg_ao_dcm_ssusb_group_is_on(void);
void dcm_infracfg_ao_dcm_ssusb_group(int on);
/* MP0_CPUCFG */
bool dcm_mp0_cpucfg_mp0_rgu_dcm_is_on(void);
void dcm_mp0_cpucfg_mp0_rgu_dcm(int on);
/* MCU_MISCCFG */
bool dcm_mcu_misccfg_adb400_dcm_is_on(void);
void dcm_mcu_misccfg_adb400_dcm(int on);
bool dcm_mcu_misccfg_bus_arm_pll_divider_dcm_is_on(void);
void dcm_mcu_misccfg_bus_arm_pll_divider_dcm(int on);
bool dcm_mcu_misccfg_bus_clock_dcm_is_on(void);
void dcm_mcu_misccfg_bus_clock_dcm(int on);
bool dcm_mcu_misccfg_bus_fabric_dcm_is_on(void);
void dcm_mcu_misccfg_bus_fabric_dcm(int on);
bool dcm_mcu_misccfg_gic_sync_dcm_is_on(void);
void dcm_mcu_misccfg_gic_sync_dcm(int on);
bool dcm_mcu_misccfg_l2_shared_dcm_is_on(void);
void dcm_mcu_misccfg_l2_shared_dcm(int on);
bool dcm_mcu_misccfg_mcu_misc_dcm_is_on(void);
void dcm_mcu_misccfg_mcu_misc_dcm(int on);
/* DRAMC_CH0_TOP0 */
bool dcm_dramc_ch0_top0_ddrphy_is_on(void);
void dcm_dramc_ch0_top0_ddrphy(int on);
/* DRAMC_CH0_TOP1 */
bool dcm_dramc_ch0_top1_dcm_dramc_group_is_on(void);
void dcm_dramc_ch0_top1_dcm_dramc_group(int on);
/* CHN0_EMI */
bool dcm_chn0_emi_dcm_emi_group_is_on(void);
void dcm_chn0_emi_dcm_emi_group(int on);
/* DRAMC_CH1_TOP0 */
bool dcm_dramc_ch1_top0_ddrphy_is_on(void);
void dcm_dramc_ch1_top0_ddrphy(int on);
/* DRAMC_CH1_TOP1 */
bool dcm_dramc_ch1_top1_dcm_dramc_group_is_on(void);
void dcm_dramc_ch1_top1_dcm_dramc_group(int on);
/* CHN1_EMI */
bool dcm_chn1_emi_dcm_emi_group_is_on(void);
void dcm_chn1_emi_dcm_emi_group(int on);


#endif /* __MT6761_DCM_AUTOGEN_H__ */

