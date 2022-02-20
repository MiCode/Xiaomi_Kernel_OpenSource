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
extern unsigned long dcm_mcusys_par_wrap_base;
extern unsigned long dcm_mp_cpusys_top_base;
extern unsigned long dcm_cpccfg_reg_base;

#if !defined(INFRACFG_AO_BASE)
#define INFRACFG_AO_BASE (dcm_infracfg_ao_base)
#endif /* !defined(INFRACFG_AO_BASE) */
#if !defined(INFRA_AO_BCRM_BASE)
#define INFRA_AO_BCRM_BASE (dcm_infra_ao_bcrm_base)
#endif /* !defined(INFRA_AO_BCRM_BASE) */
#if !defined(MCUSYS_PAR_WRAP_BASE)
#define MCUSYS_PAR_WRAP_BASE (dcm_mcusys_par_wrap_base)
#endif /* !defined(MCUSYS_PAR_WRAP_BASE) */
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
#undef MCUSYS_PAR_WRAP_BASE
#undef MP_CPUSYS_TOP_BASE
#undef CPCCFG_REG_BASE

/* Base */
#define INFRACFG_AO_BASE 0x10001000
#define INFRA_AO_BCRM_BASE 0x10022000
#define MCUSYS_PAR_WRAP_BASE 0xc530000
#define MP_CPUSYS_TOP_BASE 0xc538000
#define CPCCFG_REG_BASE 0xc53a800
#endif /* defined(CONFIG_OF)) */

/* Register Definition */
#define CPCCFG_REG_EMI_WFIFO                    (CPCCFG_REG_BASE + 0x100)
#define INFRA_AXIMEM_IDLE_BIT_EN_0              (INFRACFG_AO_BASE + 0xa30)
#define INFRA_BUS_DCM_CTRL                      (INFRACFG_AO_BASE + 0x70)
#define MCUSYS_PAR_WRAP_STALL_DCM_CONF          (MCUSYS_PAR_WRAP_BASE + 0x3230)
#define MODULE_SW_CG_2_CLR                      (INFRACFG_AO_BASE + 0xa8)
#define MODULE_SW_CG_2_SET                      (INFRACFG_AO_BASE + 0xa4)
#define MODULE_SW_CG_2_STA                      (INFRACFG_AO_BASE + 0xac)
#define MP_CPUSYS_TOP_BUS_PLLDIV_CFG            (MP_CPUSYS_TOP_BASE + 0x22e0)
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
		(INFRA_AO_BCRM_BASE + 0x30)
#define VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_1 \
		(INFRA_AO_BCRM_BASE + 0x34)
#define VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_10 \
		(INFRA_AO_BCRM_BASE + 0x58)
#define VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_2 \
		(INFRA_AO_BCRM_BASE + 0x38)
#define VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_9 \
		(INFRA_AO_BCRM_BASE + 0x54)

bool dcm_infracfg_ao_aximem_bus_dcm_is_on(void);
void dcm_infracfg_ao_aximem_bus_dcm(int on);
bool dcm_infracfg_ao_infra_bus_dcm_is_on(void);
void dcm_infracfg_ao_infra_bus_dcm(int on);
bool dcm_infracfg_ao_infra_conn_bus_dcm_is_on(void);
void dcm_infracfg_ao_infra_conn_bus_dcm(int on);
bool dcm_infracfg_ao_infra_rx_p2p_dcm_is_on(void);
void dcm_infracfg_ao_infra_rx_p2p_dcm(int on);
bool dcm_infracfg_ao_peri_bus_dcm_is_on(void);
void dcm_infracfg_ao_peri_bus_dcm(int on);
bool dcm_infracfg_ao_peri_module_dcm_is_on(void);
void dcm_infracfg_ao_peri_module_dcm(int on);
bool dcm_infra_ao_bcrm_infra_bus_dcm_is_on(void);
void dcm_infra_ao_bcrm_infra_bus_dcm(int on);
bool dcm_infra_ao_bcrm_peri_bus_dcm_is_on(void);
void dcm_infra_ao_bcrm_peri_bus_dcm(int on);
bool dcm_mcusys_par_wrap_big_dcm_is_on(void);
void dcm_mcusys_par_wrap_big_dcm(int on);
bool dcm_mp_cpusys_top_adb_dcm_is_on(void);
void dcm_mp_cpusys_top_adb_dcm(int on);
bool dcm_mp_cpusys_top_apb_dcm_is_on(void);
void dcm_mp_cpusys_top_apb_dcm(int on);
bool dcm_mp_cpusys_top_core_stall_dcm_is_on(void);
void dcm_mp_cpusys_top_core_stall_dcm(int on);
bool dcm_mp_cpusys_top_cpubiu_dcm_is_on(void);
void dcm_mp_cpusys_top_cpubiu_dcm(int on);
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
