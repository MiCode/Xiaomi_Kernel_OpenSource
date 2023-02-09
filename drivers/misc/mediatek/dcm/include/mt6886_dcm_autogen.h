/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
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
extern unsigned long dcm_mcusys_cpc_base;
extern unsigned long dcm_mcusys_par_wrap_complex0_base;
extern unsigned long dcm_mcusys_par_wrap_complex1_base;
extern unsigned long dcm_mcusys_par_wrap_complex2_base;

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
#if !defined(MCUSYS_CPC_BASE)
#define MCUSYS_CPC_BASE (dcm_mcusys_cpc_base)
#endif /* !defined(MCUSYS_CPC_BASE) */
#if !defined(MCUSYS_PAR_WRAP_COMPLEX0_BASE)
#define MCUSYS_PAR_WRAP_COMPLEX0_BASE (dcm_mcusys_par_wrap_complex0_base)
#endif /* !defined(MCUSYS_PAR_WRAP_COMPLEX0_BASE) */
#if !defined(MCUSYS_PAR_WRAP_COMPLEX1_BASE)
#define MCUSYS_PAR_WRAP_COMPLEX1_BASE (dcm_mcusys_par_wrap_complex1_base)
#endif /* !defined(MCUSYS_PAR_WRAP_COMPLEX1_BASE) */
#if !defined(MCUSYS_PAR_WRAP_COMPLEX2_BASE)
#define MCUSYS_PAR_WRAP_COMPLEX2_BASE (dcm_mcusys_par_wrap_complex2_base)
#endif /* !defined(MCUSYS_PAR_WRAP_COMPLEX2_BASE) */


#else /* !defined(CONFIG_OF)) */

/* Here below used in CTP and lk for references. */
#undef INFRACFG_AO_BASE
#undef INFRA_AO_BCRM_BASE
#undef PERI_AO_BCRM_BASE
#undef UFS0_AO_BCRM_BASE
#undef VLP_AO_BCRM_BASE
#undef MCUSYS_PAR_WRAP_BASE
#undef MCUSYS_CPC_BASE
#undef MCUSYS_PAR_WRAP_COMPLEX0_BASE
#undef MCUSYS_PAR_WRAP_COMPLEX1_BASE
#undef MCUSYS_PAR_WRAP_COMPLEX2_BASE

/* Base */
#define INFRACFG_AO_BASE 0x10001000
#define INFRA_AO_BCRM_BASE 0x10022000
#define PERI_AO_BCRM_BASE 0x11035000
#define UFS0_AO_BCRM_BASE 0x112ba000
#define VLP_AO_BCRM_BASE 0x1c017000
#define MCUSYS_PAR_WRAP_BASE 0xc000000
#define MCUSYS_CPC_BASE 0xc040000
#define MCUSYS_PAR_WRAP_COMPLEX0_BASE 0xc18c000
#define MCUSYS_PAR_WRAP_COMPLEX1_BASE 0xc1ac000
#define MCUSYS_PAR_WRAP_COMPLEX1_BASE 0xc1cc000
#endif /* defined(CONFIG_OF)) */

/* Register Definition */
#define INFRA_AXIMEM_IDLE_BIT_EN_0              (INFRACFG_AO_BASE + 0xa30)
#define INFRA_BUS_DCM_CTRL                      (INFRACFG_AO_BASE + 0x70)
#define MCUSYS_PAR_WRAP_ADB_FIFO_DCM_EN         (MCUSYS_PAR_WRAP_BASE + 0x278)
#define MCUSYS_PAR_WRAP_CBIP_CABGEN_1TO2_CONFIG (MCUSYS_PAR_WRAP_BASE + 0x2ac)
#define MCUSYS_PAR_WRAP_CBIP_CABGEN_2TO1_CONFIG (MCUSYS_PAR_WRAP_BASE + 0x2a4)
#define MCUSYS_PAR_WRAP_CBIP_CABGEN_2TO5_CONFIG (MCUSYS_PAR_WRAP_BASE + 0x2b0)
#define MCUSYS_PAR_WRAP_CBIP_CABGEN_3TO1_CONFIG (MCUSYS_PAR_WRAP_BASE + 0x2a0)
#define MCUSYS_PAR_WRAP_CBIP_CABGEN_4TO2_CONFIG (MCUSYS_PAR_WRAP_BASE + 0x2a8)
#define MCUSYS_PAR_WRAP_CBIP_P2P_CONFIG0        (MCUSYS_PAR_WRAP_BASE + 0x2b4)
#define MCUSYS_PAR_WRAP_CI700_DCM_CTRL          (MCUSYS_PAR_WRAP_BASE + 0x298)
#define MCUSYS_PAR_WRAP_COMPLEX0_STALL_DCM_CONF \
	(MCUSYS_PAR_WRAP_COMPLEX0_BASE + 0x210)
#define MCUSYS_PAR_WRAP_COMPLEX1_STALL_DCM_CONF \
	(MCUSYS_PAR_WRAP_COMPLEX1_BASE + 0x210)
#define MCUSYS_PAR_WRAP_COMPLEX2_STALL_DCM_CONF \
	(MCUSYS_PAR_WRAP_COMPLEX1_BASE + 0x210)
#define MCUSYS_PAR_WRAP_CPC_DCM_Enable          (MCUSYS_CPC_BASE  + 0x19c)
#define MCUSYS_PAR_WRAP_L3GIC_ARCH_CG_CONFIG    (MCUSYS_PAR_WRAP_BASE + 0x294)
#define MCUSYS_PAR_WRAP_MP0_DCM_CFG0            (MCUSYS_PAR_WRAP_BASE + 0x27c)
#define MCUSYS_PAR_WRAP_MP0_DCM_CFG1            (MCUSYS_PAR_WRAP_BASE + 0x29c)
#define MCUSYS_PAR_WRAP_MP_ADB_DCM_CFG0         (MCUSYS_PAR_WRAP_BASE + 0x270)
#define MCUSYS_PAR_WRAP_MP_CENTRAL_FABRIC_SUB_CHANNEL_CG \
	(MCUSYS_PAR_WRAP_BASE + 0x2b8)
#define MCUSYS_PAR_WRAP_QDCM_CONFIG0            (MCUSYS_PAR_WRAP_BASE + 0x280)
#define MCUSYS_PAR_WRAP_QDCM_CONFIG1            (MCUSYS_PAR_WRAP_BASE + 0x284)
#define MCUSYS_PAR_WRAP_QDCM_CONFIG2            (MCUSYS_PAR_WRAP_BASE + 0x288)
#define MCUSYS_PAR_WRAP_QDCM_CONFIG3            (MCUSYS_PAR_WRAP_BASE + 0x28c)
#define P2P_RX_CLK_ON                           (INFRACFG_AO_BASE + 0xa0)
#define VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0 \
	(INFRA_AO_BCRM_BASE + 0x28)
#define VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_1 \
	(INFRA_AO_BCRM_BASE + 0x2c)
#define VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_2 \
	(INFRA_AO_BCRM_BASE + 0x30)
#define VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_3 \
	(INFRA_AO_BCRM_BASE + 0x34)
#define VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_0 \
	(PERI_AO_BCRM_BASE + 0x18)
#define VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_1 \
	(PERI_AO_BCRM_BASE + 0x1c)
#define VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_2 \
	(PERI_AO_BCRM_BASE + 0x20)
#define VDNR_DCM_TOP_UFS_BUS_u_UFS_BUS_CTRL_0   (UFS0_AO_BCRM_BASE + 0x20)
#define VDNR_DCM_TOP_UFS_BUS_u_UFS_BUS_CTRL_1   (UFS0_AO_BCRM_BASE + 0x24)
#define VDNR_DCM_TOP_VLP_PAR_BUS_u_VLP_PAR_BUS_CTRL_3 \
	(VLP_AO_BCRM_BASE + 0xa4)

bool dcm_infracfg_ao_aximem_bus_dcm_is_on(void);
void dcm_infracfg_ao_aximem_bus_dcm(int on);
bool dcm_infracfg_ao_infra_bus_dcm_is_on(void);
void dcm_infracfg_ao_infra_bus_dcm(int on);
bool dcm_infracfg_ao_infra_rx_p2p_dcm_is_on(void);
void dcm_infracfg_ao_infra_rx_p2p_dcm(int on);
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
bool dcm_mcusys_par_wrap_mcu_acp_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_acp_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_adb_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_adb_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_apb_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_apb_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_bkr_ldcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_bkr_ldcm(int on);
bool dcm_mcusys_par_wrap_mcu_bus_qdcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_bus_qdcm(int on);
bool dcm_mcusys_par_wrap_mcu_cbip_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_cbip_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_core_qdcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_core_qdcm(int on);
bool dcm_mcusys_par_wrap_mcu_dsu_stalldcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_dsu_stalldcm(int on);
bool dcm_mcusys_par_wrap_mcu_io_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_io_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_misc_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_misc_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_stalldcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_stalldcm(int on);
#endif /* __MTK_DCM_AUTOGEN_H__ */
