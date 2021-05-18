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
extern unsigned long dcm_mp_cpusys_top_base;
extern unsigned long dcm_cpccfg_reg_base;

#ifndef USE_DRAM_API_INSTEAD
extern unsigned long dcm_ddrphy1_ao_base;
#endif /* #ifndef USE_DRAM_API_INSTEAD */
#ifndef INFRACFG_AO_BASE
#define INFRACFG_AO_BASE (dcm_infracfg_ao_base)
#endif /* avoid to build warning */
#ifndef MP_CPUSYS_TOP_BASE
#define MP_CPUSYS_TOP_BASE (dcm_mp_cpusys_top_base)
#endif /* avoid to build warning */
#ifndef CPCCFG_REG_BASE
#define CPCCFG_REG_BASE (dcm_cpccfg_reg_base)
#endif /* avoid to build warning */

/* the DCMs that not used actually in MT6779 */


#else /* !(defined(__KERNEL__) && defined(CONFIG_OF)) */

/* Here below used in CTP and lk for references. */
#ifdef INFRACFG_AO_BASE
#undef INFRACFG_AO_BASE
#endif /* avoid to build warning */
#ifdef MP_CPUSYS_TOP_BASE
#undef MP_CPUSYS_TOP_BASE
#endif /* avoid to build warning */
#ifdef CPCCFG_REG_BASE
#undef CPCCFG_REG_BASE
#endif /* avoid to build warning */

/* Base */
#ifndef INFRACFG_AO_BASE
#define INFRACFG_AO_BASE 0x10001000
#endif /* avoid to build warning */
#ifndef MP_CPUSYS_TOP_BASE
#define MP_CPUSYS_TOP_BASE 0xc538000
#endif /* avoid to build warning */
#ifndef CPCCFG_REG_BASE
#define CPCCFG_REG_BASE 0xc53a800
#endif /* avoid to build warning */
#endif /* #if defined(__KERNEL__) && defined(CONFIG_OF) */

/* Register Definition */
#define CPCCFG_REG_EMI_WFIFO            (CPCCFG_REG_BASE + 0x100)
#define INFRA_BUS_DCM_CTRL              (INFRACFG_AO_BASE + 0x70)
#define MP_CPUSYS_TOP_BUS_PLLDIV_CFG    (MP_CPUSYS_TOP_BASE + 0x22e0)
#define MP_CPUSYS_TOP_CPU_PLLDIV_CFG0   (MP_CPUSYS_TOP_BASE + 0x22a0)
#define MP_CPUSYS_TOP_CPU_PLLDIV_CFG1   (MP_CPUSYS_TOP_BASE + 0x22a4)
#define MP_CPUSYS_TOP_MCSIC_DCM0        (MP_CPUSYS_TOP_BASE + 0x2440)
#define MP_CPUSYS_TOP_MCSI_CFG2         (MP_CPUSYS_TOP_BASE + 0x2418)
#define MP_CPUSYS_TOP_MCUSYS_DCM_CFG0   (MP_CPUSYS_TOP_BASE + 0x25c0)
#define MP_CPUSYS_TOP_MP0_DCM_CFG0      (MP_CPUSYS_TOP_BASE + 0x4880)
#define MP_CPUSYS_TOP_MP0_DCM_CFG7      (MP_CPUSYS_TOP_BASE + 0x489c)
#define MP_CPUSYS_TOP_MP_ADB_DCM_CFG4   (MP_CPUSYS_TOP_BASE + 0x2510)
#define MP_CPUSYS_TOP_MP_MISC_DCM_CFG0  (MP_CPUSYS_TOP_BASE + 0x2518)
#define P2P_RX_CLK_ON                   (INFRACFG_AO_BASE + 0xa0)
#define PERI_BUS_DCM_CTRL               (INFRACFG_AO_BASE + 0x74)

bool dcm_infracfg_ao_audio_bus_is_on(void);
void dcm_infracfg_ao_audio_bus(int on);
bool dcm_infracfg_ao_icusb_bus_is_on(void);
void dcm_infracfg_ao_icusb_bus(int on);
bool dcm_infracfg_ao_infra_bus_is_on(void);
void dcm_infracfg_ao_infra_bus(int on);
bool dcm_infracfg_ao_p2p_rx_clk_is_on(void);
void dcm_infracfg_ao_p2p_rx_clk(int on);
bool dcm_infracfg_ao_peri_bus_is_on(void);
void dcm_infracfg_ao_peri_bus(int on);
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
bool dcm_mp_cpusys_top_fcm_stall_dcm_is_on(void);
void dcm_mp_cpusys_top_fcm_stall_dcm(int on);
bool dcm_mp_cpusys_top_last_cor_idle_dcm_is_on(void);
void dcm_mp_cpusys_top_last_cor_idle_dcm(int on);
bool dcm_mp_cpusys_top_misc_dcm_is_on(void);
void dcm_mp_cpusys_top_misc_dcm(int on);
bool dcm_mp_cpusys_top_mp0_qdcm_is_on(void);
void dcm_mp_cpusys_top_mp0_qdcm(int on);
bool dcm_cpccfg_reg_emi_wfifo_is_on(void);
void dcm_cpccfg_reg_emi_wfifo(int on);
#endif /* __MTK_DCM_AUTOGEN_H__ */

