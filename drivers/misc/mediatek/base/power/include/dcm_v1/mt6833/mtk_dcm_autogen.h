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
extern unsigned long dcm_infracfg_ao_mem_base;
extern unsigned long dcm_infra_ao_bcrm_base;
extern unsigned long dcm_mcusys_par_wrap_base;
extern unsigned long dcm_mp_cpusys_top_base;
extern unsigned long dcm_cpccfg_reg_base;
extern unsigned long dcm_mcusys_cfg_reg_base;

#ifndef USE_DRAM_API_INSTEAD
extern unsigned long dcm_ddrphy1_ao_base;
#endif /* #ifndef USE_DRAM_API_INSTEAD */
#define INFRACFG_AO_BASE (dcm_infracfg_ao_base)
#define INFRACFG_AO_MEM_BASE (dcm_infracfg_ao_mem_base)
#define INFRA_AO_BCRM_BASE (dcm_infra_ao_bcrm_base)
#define MCUSYS_PAR_WRAP_BASE (dcm_mcusys_par_wrap_base)
#define MP_CPUSYS_TOP_BASE (dcm_mp_cpusys_top_base)
#define CPCCFG_REG_BASE (dcm_cpccfg_reg_base)
#define MCUSYS_CFG_REG_BASE (dcm_mcusys_cfg_reg_base)

/* the DCMs that not used actually in MT6779 */


#else /* !(defined(__KERNEL__) && defined(CONFIG_OF)) */

/* Here below used in CTP and lk for references. */
#undef INFRACFG_AO_BASE
#undef INFRACFG_AO_MEM_BASE
#undef INFRA_AO_BCRM_BASE
#undef MCUSYS_PAR_WRAP_BASE
#undef MP_CPUSYS_TOP_BASE
#undef CPCCFG_REG_BASE
#undef MCUSYS_CFG_REG_BASE

/* Base */
#define INFRACFG_AO_BASE 0x10001000
#define INFRACFG_AO_MEM_BASE 0x10002000
#define INFRA_AO_BCRM_BASE 0x10022000
#define MCUSYS_PAR_WRAP_BASE 0xc530000
#define MP_CPUSYS_TOP_BASE 0xc538000
#define CPCCFG_REG_BASE 0xc53a800
#define MCUSYS_CFG_REG_BASE 0xc53c000
#endif /* #if defined(__KERNEL__) && defined(CONFIG_OF) */

/* Register Definition */
#define CPCCFG_REG_EMI_WFIFO                    (CPCCFG_REG_BASE + 0x100)
#define INFRA_AXIMEM_IDLE_BIT_EN_0              (INFRACFG_AO_BASE + 0xa30)
#define INFRA_BUS_DCM_CTRL                      (INFRACFG_AO_BASE + 0x70)
#define INFRA_EMI_DCM_CFG0                      (INFRACFG_AO_MEM_BASE + 0x28)
#define INFRA_EMI_DCM_CFG1                      (INFRACFG_AO_MEM_BASE + 0x2c)
#define INFRA_EMI_DCM_CFG3                      (INFRACFG_AO_MEM_BASE + 0x34)
#define MCUSYS_CFG_REG_MP0_DCM_CFG0             (MCUSYS_CFG_REG_BASE + 0x880)
#define MCUSYS_CFG_REG_MP0_DCM_CFG7             (MCUSYS_CFG_REG_BASE + 0x89c)
#define MCUSYS_PAR_WRAP_CPU_STALL_DCM_CTRL      (MCUSYS_PAR_WRAP_BASE + 0x230)
#define MCUSYS_PAR_WRAP_STALL_DCM_CONF          (MCUSYS_PAR_WRAP_BASE + 0x3230)
#define MODULE_SW_CG_2_CLR                      (INFRACFG_AO_BASE + 0xa8)
#define MODULE_SW_CG_2_SET                      (INFRACFG_AO_BASE + 0xa4)
#define MODULE_SW_CG_2_STA                      (INFRACFG_AO_BASE + 0xac)
#define MP_CPUSYS_TOP_BUS_PLLDIV_CFG            (MP_CPUSYS_TOP_BASE + 0x22e0)
#define MP_CPUSYS_TOP_MCSIC_DCM0                (MP_CPUSYS_TOP_BASE + 0x2440)
#define MP_CPUSYS_TOP_MCUSYS_DCM_CFG0           (MP_CPUSYS_TOP_BASE + 0x25c0)
#define MP_CPUSYS_TOP_MP_ADB_DCM_CFG0           (MP_CPUSYS_TOP_BASE + 0x2500)
#define MP_CPUSYS_TOP_MP_ADB_DCM_CFG4           (MP_CPUSYS_TOP_BASE + 0x2510)
#define MP_CPUSYS_TOP_MP_MISC_DCM_CFG0          (MP_CPUSYS_TOP_BASE + 0x2518)
#define P2P_RX_CLK_ON                           (INFRACFG_AO_BASE + 0xa0)
#define PERI_BUS_DCM_CTRL                       (INFRACFG_AO_BASE + 0x74)
#define TOP_CK_ANCHOR_CFG                       (INFRACFG_AO_MEM_BASE + 0x38)
#define VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0 \
		(INFRA_AO_BCRM_BASE + 0x34)
#define VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_1 \
		(INFRA_AO_BCRM_BASE + 0x38)
#define VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_10 \
		(INFRA_AO_BCRM_BASE + 0x5c)
#define VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_2 \
		(INFRA_AO_BCRM_BASE + 0x3c)
#define VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_9 \
		(INFRA_AO_BCRM_BASE + 0x58)

bool dcm_infracfg_ao_aximem_bus_dcm_is_on(void);
void dcm_infracfg_ao_aximem_bus_dcm(int on);
bool dcm_infracfg_ao_infra_bus_dcm_is_on(void);
void dcm_infracfg_ao_infra_bus_dcm(int on);
bool dcm_infracfg_ao_infra_conn_bus_dcm_is_on(void);
void dcm_infracfg_ao_infra_conn_bus_dcm(int on);
bool dcm_infracfg_ao_infra_rx_p2p_dcm_is_on(void);
void dcm_infracfg_ao_infra_rx_p2p_dcm(int on);
bool dcm_infracfg_ao_mts_bus_dcm_is_on(void);
void dcm_infracfg_ao_mts_bus_dcm(int on);
bool dcm_infracfg_ao_peri_bus_dcm_is_on(void);
void dcm_infracfg_ao_peri_bus_dcm(int on);
bool dcm_infracfg_ao_peri_module_dcm_is_on(void);
void dcm_infracfg_ao_peri_module_dcm(int on);
bool dcm_infracfg_ao_mem_dcm_emi_group_is_on(void);
void dcm_infracfg_ao_mem_dcm_emi_group(int on);
bool dcm_infra_ao_bcrm_infra_bus_dcm_is_on(void);
void dcm_infra_ao_bcrm_infra_bus_dcm(int on);
bool dcm_infra_ao_bcrm_peri_bus_dcm_is_on(void);
void dcm_infra_ao_bcrm_peri_bus_dcm(int on);
bool dcm_mcusys_par_wrap_big_dcm_is_on(void);
void dcm_mcusys_par_wrap_big_dcm(int on);
bool dcm_mcusys_par_wrap_little_dcm_is_on(void);
void dcm_mcusys_par_wrap_little_dcm(int on);
bool dcm_mp_cpusys_top_adb_dcm_is_on(void);
void dcm_mp_cpusys_top_adb_dcm(int on);
bool dcm_mp_cpusys_top_apb_dcm_is_on(void);
void dcm_mp_cpusys_top_apb_dcm(int on);
bool dcm_mp_cpusys_top_bus_pll_div_dcm_is_on(void);
void dcm_mp_cpusys_top_bus_pll_div_dcm(int on);
bool dcm_mp_cpusys_top_cpubiu_dcm_is_on(void);
void dcm_mp_cpusys_top_cpubiu_dcm(int on);
bool dcm_mp_cpusys_top_last_cor_idle_dcm_is_on(void);
void dcm_mp_cpusys_top_last_cor_idle_dcm(int on);
bool dcm_mp_cpusys_top_misc_dcm_is_on(void);
void dcm_mp_cpusys_top_misc_dcm(int on);
bool dcm_mp_cpusys_top_mp0_qdcm_is_on(void);
void dcm_mp_cpusys_top_mp0_qdcm(int on);
bool dcm_cpccfg_reg_emi_wfifo_is_on(void);
void dcm_cpccfg_reg_emi_wfifo(int on);
bool dcm_mcusys_cfg_reg_apb_dcm_is_on(void);
void dcm_mcusys_cfg_reg_apb_dcm(int on);
bool dcm_mcusys_cfg_reg_core_stall_dcm_is_on(void);
void dcm_mcusys_cfg_reg_core_stall_dcm(int on);
bool dcm_mcusys_cfg_reg_fcm_stall_dcm_is_on(void);
void dcm_mcusys_cfg_reg_fcm_stall_dcm(int on);
bool dcm_mcusys_cfg_reg_mp0_qdcm_is_on(void);
void dcm_mcusys_cfg_reg_mp0_qdcm(int on);
#endif /* __MTK_DCM_AUTOGEN_H__ */

