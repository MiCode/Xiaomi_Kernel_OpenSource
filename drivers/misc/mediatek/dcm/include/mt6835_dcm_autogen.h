/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_DCM_AUTOGEN_H__
#define __MTK_DCM_AUTOGEN_H__

#include <mtk_dcm.h>

#if IS_ENABLED(CONFIG_OF)
/* TODO: Fix all base addresses. */
extern unsigned long dcm_infracfg_ao_base;
extern unsigned long dcm_infra_ao_bcrm_base;
extern unsigned long dcm_peri_ao_bcrm_base;
extern unsigned long dcm_ufs0_ao_bcrm_base;
extern unsigned long dcm_vlp_ao_bcrm_base;
extern unsigned long dcm_mcusys_par_wrap_base;
extern unsigned long dcm_mp_cpusys_top_base;

#ifndef USE_DRAM_API_INSTEAD
extern unsigned long dcm_ddrphy1_ao_base;
#endif /* #ifndef USE_DRAM_API_INSTEAD */

#if !defined(INFRACFG_AO_BASE)
#define INFRACFG_AO_BASE (dcm_infracfg_ao_base)
#endif /* !defined(INFRACFG_AO_BASE) */
#if !defined(INFRA_AO_BCRM_BASE)
#define INFRA_AO_BCRM_BASE (dcm_infra_ao_bcrm_base)
#endif /* !defined(INFRA_AO_BCRM_BASE) */
#if !defined(PERI_AO_BCRM_BASE)
#define PERI_AO_BCRM_BASE (dcm_peri_ao_bcrm_base)
#endif /* !defined(PERI_AO_BCRM_BASE) */
#if !defined(UFS0_AO_BCRM_BASE)
#define UFS0_AO_BCRM_BASE (dcm_ufs0_ao_bcrm_base)
#endif /* !defined(UFS0_AO_BCRM_BASE) */
#if !defined(VLP_AO_BCRM_BASE)
#define VLP_AO_BCRM_BASE (dcm_vlp_ao_bcrm_base)
#endif /* !defined(VLP_AO_BCRM_BASE) */
#if !defined(MCUSYS_PAR_WRAP_BASE)
#define MCUSYS_PAR_WRAP_BASE (dcm_mcusys_par_wrap_base)
#endif /* !defined(MCUSYS_PAR_WRAP_BASE) */
#if !defined(MP_CPUSYS_TOP_BASE)
#define MP_CPUSYS_TOP_BASE (dcm_mp_cpusys_top_base)
#endif /* !defined(MP_CPUSYS_TOP_BASE) */

#else /* !IS_ENABLED(CONFIG_OF)) */

/* Here below used in CTP and lk for references. */
#undef INFRACFG_AO_BASE
#undef INFRA_AO_BCRM_BASE
#undef PERI_AO_BCRM_BASE
#undef UFS0_AO_BCRM_BASE
#undef VLP_AO_BCRM_BASE
#undef MCUSYS_PAR_WRAP_BASE
#undef MP_CPUSYS_TOP_BASE

/* Base */
#define INFRACFG_AO_BASE 0x10001000
#define INFRA_AO_BCRM_BASE 0x10022000
#define PERI_AO_BCRM_BASE 0x11035000
#define UFS0_AO_BCRM_BASE 0x112ba000
#define VLP_AO_BCRM_BASE 0x1c017000
#define MCUSYS_PAR_WRAP_BASE 0xc530000
#define MP_CPUSYS_TOP_BASE 0xc538000
#endif /* if IS_ENABLED(CONFIG_OF)) */

/* Register Definition */
#define CPCCFG_REG_EMI_WFIFO                    (MP_CPUSYS_TOP_BASE + 0x2900)
#define INFRA_AXIMEM_IDLE_BIT_EN_0              (INFRACFG_AO_BASE + 0xa30)
#define INFRA_BUS_DCM_CTRL                      (INFRACFG_AO_BASE + 0x70)
#define MCUSYS_PAR_WRAP_STALL_DCM_CONF          (MCUSYS_PAR_WRAP_BASE + 0x3230)
#define MP_CPUSYS_TOP_BUS_PLLDIV_CFG            (MP_CPUSYS_TOP_BASE + 0x22e0)
#define MP_CPUSYS_TOP_CPU_PLLDIV_CFG0           (MP_CPUSYS_TOP_BASE + 0x22a0)
#define MP_CPUSYS_TOP_CPU_PLLDIV_CFG1           (MP_CPUSYS_TOP_BASE + 0x22a4)
#define MP_CPUSYS_TOP_MCSIC_DCM0                (MP_CPUSYS_TOP_BASE + 0x2440)
#define MP_CPUSYS_TOP_MCUSYS_DCM_CFG0           (MP_CPUSYS_TOP_BASE + 0x25c0)
#define MP_CPUSYS_TOP_MP0_DCM_CFG0              (MP_CPUSYS_TOP_BASE + 0x4880)
#define MP_CPUSYS_TOP_MP0_DCM_CFG7              (MP_CPUSYS_TOP_BASE + 0x489c)
#define MP_CPUSYS_TOP_MP_ADB_DCM_CFG0           (MP_CPUSYS_TOP_BASE + 0x2500)
#define MP_CPUSYS_TOP_MP_ADB_DCM_CFG4           (MP_CPUSYS_TOP_BASE + 0x2510)
#define MP_CPUSYS_TOP_MP_MISC_DCM_CFG0          (MP_CPUSYS_TOP_BASE + 0x2518)
#define P2P_RX_CLK_ON                           (INFRACFG_AO_BASE + 0xa0)
#define PERI_BUS_DCM_CTRL                       (INFRACFG_AO_BASE + 0x74)
#define VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0 \
		(INFRA_AO_BCRM_BASE + 0x14)
#define VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_1 \
		(INFRA_AO_BCRM_BASE + 0x18)
#define VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_2 \
		(INFRA_AO_BCRM_BASE + 0x1c)
#define VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_0 \
		(PERI_AO_BCRM_BASE + 0x14)
#define VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_1 \
		(PERI_AO_BCRM_BASE + 0x18)
#define VDNR_DCM_TOP_UFS_BUS_u_UFS_BUS_CTRL_0   (UFS0_AO_BCRM_BASE + 0x1c)
#define VLP_AO_BCRM_VDNR_DCM_TOP_VLP_PAR_BUS_u_VLP_PAR_BUS_CTRL_0 \
		(VLP_AO_BCRM_BASE + 0xa4)

bool dcm_infracfg_ao_aximem_bus_dcm_is_on(void);
void dcm_infracfg_ao_aximem_bus_dcm(int on);
bool dcm_infracfg_ao_infra_bus_dcm_is_on(void);
void dcm_infracfg_ao_infra_bus_dcm(int on);
bool dcm_infracfg_ao_infra_rx_p2p_dcm_is_on(void);
void dcm_infracfg_ao_infra_rx_p2p_dcm(int on);
bool dcm_infracfg_ao_peri_bus_dcm_is_on(void);
void dcm_infracfg_ao_peri_bus_dcm(int on);
bool dcm_infracfg_ao_peri_module_dcm_is_on(void);
void dcm_infracfg_ao_peri_module_dcm(int on);
bool dcm_infra_ao_bcrm_infra_bus_dcm_is_on(void);
void dcm_infra_ao_bcrm_infra_bus_dcm(int on);
bool dcm_infra_ao_bcrm_infra_bus_fmem_sub_dcm_is_on(void);
void dcm_infra_ao_bcrm_infra_bus_fmem_sub_dcm(int on);
bool dcm_peri_ao_bcrm_peri_bus_dcm_is_on(void);
void dcm_peri_ao_bcrm_peri_bus_dcm(int on);
bool dcm_ufs0_ao_bcrm_ufs_bus_dcm_is_on(void);
void dcm_ufs0_ao_bcrm_ufs_bus_dcm(int on);
bool dcm_vlp_ao_bcrm_vlp_bus_dcm_is_on(void);
void dcm_vlp_ao_bcrm_vlp_bus_dcm(int on);
bool dcm_mcusys_par_wrap_big_dcm_is_on(void);
void dcm_mcusys_par_wrap_big_dcm(int on);
bool dcm_mp_cpusys_top_adb_dcm_is_on(void);
void dcm_mp_cpusys_top_adb_dcm(int on);
bool dcm_mp_cpusys_top_apb_dcm_is_on(void);
void dcm_mp_cpusys_top_apb_dcm(int on);
bool dcm_mp_cpusys_top_bus_pll_div_dcm_is_on(void);
void dcm_mp_cpusys_top_bus_pll_div_dcm(int on);
bool dcm_mp_cpusys_top_core_stall_dcm_is_on(void);
void dcm_mp_cpusys_top_core_stall_dcm(int on);
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
