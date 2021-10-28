/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_DCM_AUTOGEN_H__
#define __MTK_DCM_AUTOGEN_H__

#include <mtk_dcm.h>

#if IS_ENABLED(CONFIG_OF)
/* TODO: Fix all base addresses. */
extern unsigned long dcm_infracfg_ao_base;
extern unsigned long dcm_infra_ao_bcrm_base;
extern unsigned long dcm_infracfg_ao_mem_base;
extern unsigned long dcm_peri_ao_bcrm_base;
extern unsigned long dcm_vlp_ao_bcrm_base;
extern unsigned long dcm_mp_cpusys_top_base;
extern unsigned long dcm_cpccfg_reg_base;

#ifndef USE_DRAM_API_INSTEAD
extern unsigned long dcm_ddrphy1_ao_base;
#endif /* #ifndef (USE_DRAM_API_INSTEAD) */

#if !defined(INFRACFG_AO_BASE)
#define INFRACFG_AO_BASE (dcm_infracfg_ao_base)
#endif /* !defined(INFRACFG_AO_BASE) */
#if !defined(INFRA_AO_BCRM_BASE)
#define INFRA_AO_BCRM_BASE (dcm_infra_ao_bcrm_base)
#endif /* !defined(INFRA_AO_BCRM_BASE) */
#if !defined(INFRACFG_AO_MEM_BASE)
#define INFRACFG_AO_MEM_BASE (dcm_infracfg_ao_mem_base)
#endif /* !defined(INFRACFG_AO_MEM_BASE) */
#if !defined(PERI_AO_BCRM_BASE)
#define PERI_AO_BCRM_BASE (dcm_peri_ao_bcrm_base)
#endif /* !defined(PERI_AO_BCRM_BASE) */
#if !defined(VLP_AO_BCRM_BASE)
#define VLP_AO_BCRM_BASE (dcm_vlp_ao_bcrm_base)
#endif /* !defined(VLP_AO_BCRM_BASE) */
#if !defined(MP_CPUSYS_TOP_BASE)
#define MP_CPUSYS_TOP_BASE (dcm_mp_cpusys_top_base)
#endif /* !defined(MP_CPUSYS_TOP_BASE) */
#if !defined(CPCCFG_REG_BASE)
#define CPCCFG_REG_BASE (dcm_cpccfg_reg_base)
#endif /* !defined(CPCCFG_REG_BASE) */

#else /* !defined(CONFIG_OF)) */

/* Here below used in CTP and lk for references. */
#undef INFRACFG_AO_BASE
#undef INFRA_AO_BCRM_BASE
#undef INFRACFG_AO_MEM_BASE
#undef PERI_AO_BCRM_BASE
#undef VLP_AO_BCRM_BASE
#undef MP_CPUSYS_TOP_BASE
#undef CPCCFG_REG_BASE

/* Base */
#define INFRACFG_AO_BASE 0x10001000
#define INFRA_AO_BCRM_BASE 0x10022000
#define INFRACFG_AO_MEM_BASE 0x10270000
#define PERI_AO_BCRM_BASE 0x11035000
#define VLP_AO_BCRM_BASE 0x1c017000
#define MP_CPUSYS_TOP_BASE 0xc538000
#define CPCCFG_REG_BASE 0xc53a800
#endif /* defined(CONFIG_OF)) */

/* Register Definition */
#define CPCCFG_REG_EMI_WFIFO            (CPCCFG_REG_BASE + 0x100)
#define INFRA_AXIMEM_IDLE_BIT_EN_0      (INFRACFG_AO_BASE + 0xa30)
#define INFRA_BUS_DCM_CTRL              (INFRACFG_AO_BASE + 0x70)
#define MP_CPUSYS_TOP_BUS_PLLDIV_CFG    (MP_CPUSYS_TOP_BASE + 0x22e0)
#define MP_CPUSYS_TOP_CPU_PLLDIV_CFG0   (MP_CPUSYS_TOP_BASE + 0x22a0)
#define MP_CPUSYS_TOP_CPU_PLLDIV_CFG1   (MP_CPUSYS_TOP_BASE + 0x22a4)
#define MP_CPUSYS_TOP_CPU_PLLDIV_CFG2   (MP_CPUSYS_TOP_BASE + 0x22a8)
#define MP_CPUSYS_TOP_MCSIC_DCM0        (MP_CPUSYS_TOP_BASE + 0x2440)
#define MP_CPUSYS_TOP_MCUSYS_DCM_CFG0   (MP_CPUSYS_TOP_BASE + 0x25c0)
#define MP_CPUSYS_TOP_MP0_DCM_CFG0      (MP_CPUSYS_TOP_BASE + 0x4880)
#define MP_CPUSYS_TOP_MP0_DCM_CFG7      (MP_CPUSYS_TOP_BASE + 0x489c)
#define MP_CPUSYS_TOP_MP_ADB_DCM_CFG0   (MP_CPUSYS_TOP_BASE + 0x2500)
#define MP_CPUSYS_TOP_MP_ADB_DCM_CFG4   (MP_CPUSYS_TOP_BASE + 0x2510)
#define MP_CPUSYS_TOP_MP_MISC_DCM_CFG0  (MP_CPUSYS_TOP_BASE + 0x2518)
#define P2P_RX_CLK_ON                   (INFRACFG_AO_BASE + 0xa0)
#define VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0 \
		(INFRA_AO_BCRM_BASE + 0x10)
#define VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_1 \
		(INFRA_AO_BCRM_BASE + 0x14)
#define VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_2 \
		(INFRA_AO_BCRM_BASE + 0x18)
#define VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_0 \
		(PERI_AO_BCRM_BASE + 0x18)
#define VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_1 \
		(PERI_AO_BCRM_BASE + 0x1c)
#define VDNR_DCM_TOP_VLP_PAR_BUS_u_VLP_PAR_BUS_CTRL_0 \
		(VLP_AO_BCRM_BASE + 0x98)

void dcm_infracfg_ao_aximem_bus_dcm(int on);
void dcm_infracfg_ao_infra_bus_dcm(int on);
void dcm_infracfg_ao_infra_rx_p2p_dcm(int on);
void dcm_infracfg_ao_mts_bus_dcm(int on);
void dcm_infra_ao_bcrm_infra_bus_dcm(int on);
void dcm_infra_ao_bcrm_infra_bus_fmem_sub_dcm(int on);
void dcm_peri_ao_bcrm_peri_bus_dcm(int on);
void dcm_vlp_ao_bcrm_vlp_bus_dcm(int on);
void dcm_mp_cpusys_top_adb_dcm(int on);
void dcm_mp_cpusys_top_apb_dcm(int on);
void dcm_mp_cpusys_top_bus_pll_div_dcm(int on);
void dcm_mp_cpusys_top_core_stall_dcm(int on);
void dcm_mp_cpusys_top_cpubiu_dcm(int on);
void dcm_mp_cpusys_top_cpu_pll_div_0_dcm(int on);
void dcm_mp_cpusys_top_cpu_pll_div_1_dcm(int on);
void dcm_mp_cpusys_top_cpu_pll_div_2_dcm(int on);
void dcm_mp_cpusys_top_fcm_stall_dcm(int on);
void dcm_mp_cpusys_top_last_cor_idle_dcm(int on);
void dcm_mp_cpusys_top_misc_dcm(int on);
void dcm_mp_cpusys_top_mp0_qdcm(int on);
void dcm_cpccfg_reg_emi_wfifo(int on);
#endif /* __MTK_DCM_AUTOGEN_H__ */
