/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __MTK_DCM_AUTOGEN_H__
#define __MTK_DCM_AUTOGEN_H__

#include <mtk_dcm.h>

#if IS_ENABLED(CONFIG_OF)
/* TODO: Fix all base addresses. */
extern unsigned long dcm_topckgen_base;
extern unsigned long dcm_ifrbus_ao_base;
extern unsigned long dcm_peri_ao_bcrm_base;
extern unsigned long dcm_ufs0_ao_bcrm_base;
extern unsigned long dcm_pcie0_ao_bcrm_base;
extern unsigned long dcm_vlp_ao_bcrm_base;
extern unsigned long dcm_mcusys_par_wrap_base;

#ifndef USE_DRAM_API_INSTEAD
extern unsigned long dcm_ddrphy1_ao_base;
#endif /* #ifndef USE_DRAM_API_INSTEAD */

#if !defined(TOPCKGEN_BASE)
#define TOPCKGEN_BASE (dcm_topckgen_base)
#endif /* !defined(TOPCKGEN_BASE) */
#if !defined(IFRBUS_AO_BASE)
#define IFRBUS_AO_BASE (dcm_ifrbus_ao_base)
#endif /* !defined(IFRBUS_AO_BASE) */
#if !defined(PERI_AO_BCRM_BASE)
#define PERI_AO_BCRM_BASE (dcm_peri_ao_bcrm_base)
#endif /* !defined(PERI_AO_BCRM_BASE) */
#if !defined(UFS0_AO_BCRM_BASE)
#define UFS0_AO_BCRM_BASE (dcm_ufs0_ao_bcrm_base)
#endif /* !defined(UFS0_AO_BCRM_BASE) */
#if !defined(PCIE0_AO_BCRM_BASE)
#define PCIE0_AO_BCRM_BASE (dcm_pcie0_ao_bcrm_base)
#endif /* !defined(PCIE0_AO_BCRM_BASE) */
#if !defined(VLP_AO_BCRM_BASE)
#define VLP_AO_BCRM_BASE (dcm_vlp_ao_bcrm_base)
#endif /* !defined(VLP_AO_BCRM_BASE) */
#if !defined(MCUSYS_PAR_WRAP_BASE)
#define MCUSYS_PAR_WRAP_BASE (dcm_mcusys_par_wrap_base)
#endif /* !defined(MCUSYS_PAR_WRAP_BASE) */

#else /* !defined(CONFIG_OF)) */

/* Here below used in CTP and lk for references. */
#undef TOPCKGEN_BASE
#undef IFRBUS_AO_BASE
#undef PERI_AO_BCRM_BASE
#undef UFS0_AO_BCRM_BASE
#undef PCIE0_AO_BCRM_BASE
#undef VLP_AO_BCRM_BASE
#undef MCUSYS_PAR_WRAP_BASE

/* Base */
#define TOPCKGEN_BASE 0x10000000
#define IFRBUS_AO_BASE 0x1002c000
#define PERI_AO_BCRM_BASE 0x11035000
#define UFS0_AO_BCRM_BASE 0x112ba000
#define PCIE0_AO_BCRM_BASE 0x112e2000
#define VLP_AO_BCRM_BASE 0x1c017000
#define MCUSYS_PAR_WRAP_BASE 0xc000000
#endif /* defined(CONFIG_OF)) */

/* Register Definition */
#define DCM_SET_RW_0                            (IFRBUS_AO_BASE + 0xb00)
#define MCUSYS_PAR_WRAP_ADB_FIFO_DCM_EN         (MCUSYS_PAR_WRAP_BASE + 0x278)
#define MCUSYS_PAR_WRAP_CBIP_CABGEN_1TO2_CONFIG (MCUSYS_PAR_WRAP_BASE + 0x2ac)
#define MCUSYS_PAR_WRAP_CBIP_CABGEN_2TO1_CONFIG (MCUSYS_PAR_WRAP_BASE + 0x2a4)
#define MCUSYS_PAR_WRAP_CBIP_CABGEN_2TO5_CONFIG (MCUSYS_PAR_WRAP_BASE + 0x2b0)
#define MCUSYS_PAR_WRAP_CBIP_CABGEN_3TO1_CONFIG (MCUSYS_PAR_WRAP_BASE + 0x2a0)
#define MCUSYS_PAR_WRAP_CBIP_CABGEN_4TO2_CONFIG (MCUSYS_PAR_WRAP_BASE + 0x2a8)
#define MCUSYS_PAR_WRAP_CBIP_P2P_CONFIG0        (MCUSYS_PAR_WRAP_BASE + 0x2b4)
#define MCUSYS_PAR_WRAP_CI700_DCM_CTRL          (MCUSYS_PAR_WRAP_BASE + 0x298)
#define MCUSYS_PAR_WRAP_COMPLEX0_STALL_DCM_CONF \
		(MCUSYS_PAR_WRAP_BASE + 0x18c210)
#define MCUSYS_PAR_WRAP_COMPLEX1_STALL_DCM_CONF \
		(MCUSYS_PAR_WRAP_BASE + 0x1ac210)
#define MCUSYS_PAR_WRAP_CPC_DCM_Enable          (MCUSYS_PAR_WRAP_BASE + 0x4019c)
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
#define TOPCKGEN_MMU_DCM_DIS                    (TOPCKGEN_BASE + 0x330050)
#define TOPCKGEN_RSI_DCM_CON                    (TOPCKGEN_BASE + 0x324004)
#define VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_0 \
		(PERI_AO_BCRM_BASE + 0x18)
#define VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_1 \
		(PERI_AO_BCRM_BASE + 0x1c)
#define VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_2 \
		(PERI_AO_BCRM_BASE + 0x20)
#define VDNR_DCM_TOP_PEXTP_BUS_u_PEXTP_BUS_CTRL_0 \
		(PCIE0_AO_BCRM_BASE + 0x34)
#define VDNR_DCM_TOP_PEXTP_BUS_u_PEXTP_BUS_CTRL_1 \
		(PCIE0_AO_BCRM_BASE + 0x38)
#define VDNR_DCM_TOP_UFS_BUS_u_UFS_BUS_CTRL_0   (UFS0_AO_BCRM_BASE + 0x20)
#define VDNR_DCM_TOP_UFS_BUS_u_UFS_BUS_CTRL_1   (UFS0_AO_BCRM_BASE + 0x24)
#define VDNR_DCM_TOP_VLP_PAR_BUS_u_VLP_PAR_BUS_CTRL_0 \
		(VLP_AO_BCRM_BASE + 0xa4)

bool dcm_topckgen_infra_iommu_dcm_is_on(void);
void dcm_topckgen_infra_iommu_dcm(int on);
bool dcm_topckgen_infra_rsi_dcm_is_on(void);
void dcm_topckgen_infra_rsi_dcm(int on);
bool dcm_ifrbus_ao_infra_bus_dcm_is_on(void);
void dcm_ifrbus_ao_infra_bus_dcm(int on);
bool dcm_peri_ao_bcrm_peri_bus_dcm_is_on(void);
void dcm_peri_ao_bcrm_peri_bus_dcm(int on);
bool dcm_ufs0_ao_bcrm_ufs_bus_dcm_is_on(void);
void dcm_ufs0_ao_bcrm_ufs_bus_dcm(int on);
bool dcm_pcie0_ao_bcrm_pextp_bus_dcm_is_on(void);
void dcm_pcie0_ao_bcrm_pextp_bus_dcm(int on);
bool dcm_vlp_ao_bcrm_vlp_bus_dcm_is_on(void);
void dcm_vlp_ao_bcrm_vlp_bus_dcm(int on);
bool dcm_mcusys_par_wrap_cpc_pbi_dcm_is_on(void);
void dcm_mcusys_par_wrap_cpc_pbi_dcm(int on);
bool dcm_mcusys_par_wrap_cpc_turbo_dcm_is_on(void);
void dcm_mcusys_par_wrap_cpc_turbo_dcm(int on);
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
bool dcm_mcusys_par_wrap_cpu0_mcu_stalldcm_is_on(void);
void dcm_mcusys_par_wrap_cpu0_mcu_stalldcm(int on);
bool dcm_mcusys_par_wrap_cpu1_mcu_stalldcm_is_on(void);
void dcm_mcusys_par_wrap_cpu1_mcu_stalldcm(int on);
bool dcm_mcusys_par_wrap_cpu2_mcu_stalldcm_is_on(void);
void dcm_mcusys_par_wrap_cpu2_mcu_stalldcm(int on);
bool dcm_mcusys_par_wrap_cpu3_mcu_stalldcm_is_on(void);
void dcm_mcusys_par_wrap_cpu3_mcu_stalldcm(int on);
bool dcm_mcusys_par_wrap_cpu4_mcu_stalldcm_is_on(void);
void dcm_mcusys_par_wrap_cpu4_mcu_stalldcm(int on);
bool dcm_mcusys_par_wrap_cpu5_mcu_stalldcm_is_on(void);
void dcm_mcusys_par_wrap_cpu5_mcu_stalldcm(int on);
bool dcm_mcusys_par_wrap_cpu6_mcu_stalldcm_is_on(void);
void dcm_mcusys_par_wrap_cpu6_mcu_stalldcm(int on);
bool dcm_mcusys_par_wrap_cpu7_mcu_stalldcm_is_on(void);
void dcm_mcusys_par_wrap_cpu7_mcu_stalldcm(int on);
#endif /* __MTK_DCM_AUTOGEN_H__ */
