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
/*extern unsigned long dcm_pwrap_base;*/
extern unsigned long dcm_mcucfg_base;
extern unsigned long dcm_cpccfg_rg_base;

extern unsigned long dcm_dramc_ch0_top0_ao_base;
extern unsigned long dcm_dramc_ch1_top0_ao_base;

extern unsigned long dcm_dramc_ch0_top1_ao_base;
extern unsigned long dcm_dramc_ch1_top1_ao_base;

extern unsigned long dcm_ch0_emi_base;
extern unsigned long dcm_ch1_emi_base;
extern unsigned long dcm_emi_base;



#define INFRACFG_AO_BASE	(dcm_infracfg_ao_base)
/*#define PWRAP_BASE		(dcm_pwrap_base)*/
#define MP_CPUSYS_TOP_BASE	(dcm_mcucfg_base)
#define CPCCFG_REG_BASE		(dcm_cpccfg_rg_base)

/* dram staff */
/* dramc */
#define DRAMC_CH0_TOP0_BASE	(dcm_dramc_ch0_top0_ao_base)
#define DRAMC_CH0_TOP1_BASE	(dcm_dramc_ch0_top1_ao_base)

/* ddrphy */
#define DRAMC_CH1_TOP0_BASE	(dcm_dramc_ch1_top0_ao_base)
#define DRAMC_CH1_TOP1_BASE	(dcm_dramc_ch1_top1_ao_base)

/* emi */
#define CHN0_EMI_BASE		(dcm_ch0_emi_base)
#define CHN1_EMI_BASE		(dcm_ch1_emi_base)
#define EMI_BASE		(dcm_emi_base)


#else /* !(defined(__KERNEL__) && defined(CONFIG_OF)) */

/* Here below used in CTP and lk for references. */
#undef INFRACFG_AO_BASE
/*undef PWRAP_BASE*/
#undef EMI_BASE
#undef DRAMC_CH0_TOP0_BASE
#undef DRAMC_CH0_TOP1_BASE
#undef CHN0_EMI_BASE
#undef DRAMC_CH1_TOP0_BASE
#undef DRAMC_CH1_TOP1_BASE
#undef CHN1_EMI_BASE
#undef MP_CPUSYS_TOP_BASE
#undef CPCCFG_REG_BASE

/* Base */
#define INFRACFG_AO_BASE 0x10001000
/*#define PWRAP_BASE 0x1000d000*/
#define EMI_BASE 0x10219000
#define DRAMC_CH0_TOP0_BASE 0x10228000
#define DRAMC_CH0_TOP1_BASE 0x1022a000
#define CHN0_EMI_BASE 0x1022d000
#define DRAMC_CH1_TOP0_BASE 0x10230000
#define DRAMC_CH1_TOP1_BASE 0x10232000
#define CHN1_EMI_BASE 0x10235000
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
#define MP_ADB_DCM_CFG4 (MP_CPUSYS_TOP_BASE + 0x2510)
#define MP_MISC_DCM_CFG0 (MP_CPUSYS_TOP_BASE + 0x2518)
#define MCUSYS_DCM_CFG0 (MP_CPUSYS_TOP_BASE + 0x25c0)
#define EMI_WFIFO (CPCCFG_REG_BASE + 0x100)
#define MP0_DCM_CFG0 (MP_CPUSYS_TOP_BASE + 0x4880)
#define MP0_DCM_CFG7 (MP_CPUSYS_TOP_BASE + 0x489c)
#define INFRA_BUS_DCM_CTRL (INFRACFG_AO_BASE + 0x70)
#define PERI_BUS_DCM_CTRL (INFRACFG_AO_BASE + 0x74)
#define MEM_DCM_CTRL (INFRACFG_AO_BASE + 0x78)
#define DFS_MEM_DCM_CTRL (INFRACFG_AO_BASE + 0x7c)
/*#define PMIC_WRAP_DCM_EN (PWRAP_BASE + 0x1ec)*/
#define EMI_CONM (EMI_BASE + 0x60)
#define EMI_CONN (EMI_BASE + 0x68)
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
bool dcm_infracfg_ao_audio_bus_is_on(void);
void dcm_infracfg_ao_audio_bus(int on);
bool dcm_infracfg_ao_icusb_bus_is_on(void);
void dcm_infracfg_ao_icusb_bus(int on);
bool dcm_infracfg_ao_infra_bus_is_on(void);
void dcm_infracfg_ao_infra_bus(int on);
bool dcm_infracfg_ao_infra_dfs_mem_is_on(void);
void dcm_infracfg_ao_infra_dfs_mem(int on);
bool dcm_infracfg_ao_infra_mem_is_on(void);
void dcm_infracfg_ao_infra_mem(int on);
bool dcm_infracfg_ao_peri_bus_is_on(void);
void dcm_infracfg_ao_peri_bus(int on);
/* PWRAP */
/*bool dcm_pwrap_pmic_wrap_is_on(void);*/
/*void dcm_pwrap_pmic_wrap(int on);*/
/* EMI */
bool dcm_emi_emi_dcm_is_on(void);
void dcm_emi_emi_dcm(int on);
/* DRAMC_CH0_TOP0 */
bool dcm_dramc_ch0_top0_ddrphy_is_on(void);
void dcm_dramc_ch0_top0_ddrphy(int on);
/* DRAMC_CH0_TOP1 */
bool dcm_dramc_ch0_top1_dramc_dcm_is_on(void);
void dcm_dramc_ch0_top1_dramc_dcm(int on);
/* CHN0_EMI */
bool dcm_chn0_emi_emi_dcm_is_on(void);
void dcm_chn0_emi_emi_dcm(int on);
/* DRAMC_CH1_TOP0 */
bool dcm_dramc_ch1_top0_ddrphy_is_on(void);
void dcm_dramc_ch1_top0_ddrphy(int on);
/* DRAMC_CH1_TOP1 */
bool dcm_dramc_ch1_top1_dramc_dcm_is_on(void);
void dcm_dramc_ch1_top1_dramc_dcm(int on);
/* CHN1_EMI */
bool dcm_chn1_emi_emi_dcm_is_on(void);
void dcm_chn1_emi_emi_dcm(int on);
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

#endif /* __MTK_DCM_AUTOGEN_H__ */

