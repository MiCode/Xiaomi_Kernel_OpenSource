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
extern unsigned long dcm_vlp_ao_bcrm_base;
extern unsigned long dcm_mcusys_top_base;
extern unsigned long dcm_mcusys_cpc_base;
extern unsigned long dcm_mp_cpu4_top_base;
extern unsigned long dcm_mp_cpu5_top_base;
extern unsigned long dcm_mp_cpu6_top_base;
extern unsigned long dcm_mp_cpu7_top_base;

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
#if !defined(VLP_AO_BCRM_BASE)
#define VLP_AO_BCRM_BASE (dcm_vlp_ao_bcrm_base)
#endif /* !defined(VLP_AO_BCRM_BASE) */
#if !defined(MCUSYS_TOP_BASE)
#define MCUSYS_TOP_BASE (dcm_mcusys_top_base)
#endif /* !defined(MCUSYS_TOP_BASE) */
#if !defined(MCUSYS_CPC_BASE)
#define MCUSYS_CPC_BASE (dcm_mcusys_cpc_base)
#endif /* !defined(MCUSYS_CPC_BASE) */
#if !defined(MP_CPU4_TOP_BASE)
#define MP_CPU4_TOP_BASE (dcm_mp_cpu4_top_base)
#endif /* !defined(MP_CPU4_TOP_BASE) */
#if !defined(MP_CPU5_TOP_BASE)
#define MP_CPU5_TOP_BASE (dcm_mp_cpu5_top_base)
#endif /* !defined(MP_CPU5_TOP_BASE) */
#if !defined(MP_CPU6_TOP_BASE)
#define MP_CPU6_TOP_BASE (dcm_mp_cpu6_top_base)
#endif /* !defined(MP_CPU6_TOP_BASE) */
#if !defined(MP_CPU7_TOP_BASE)
#define MP_CPU7_TOP_BASE (dcm_mp_cpu7_top_base)
#endif /* !defined(MP_CPU7_TOP_BASE) */

#else /* !defined(CONFIG_OF)) */

/* Here below used in CTP and lk for references. */
#undef INFRACFG_AO_BASE
#undef INFRA_AO_BCRM_BASE
#undef PERI_AO_BCRM_BASE
#undef VLP_AO_BCRM_BASE
#undef MCUSYS_TOP_BASE
#undef MCUSYS_CPC_BASE
#undef MP_CPU4_TOP_BASE
#undef MP_CPU5_TOP_BASE
#undef MP_CPU6_TOP_BASE
#undef MP_CPU7_TOP_BASE

/* Base */
#define INFRACFG_AO_BASE 0x10001000
#define INFRA_AO_BCRM_BASE 0x10022000
#define PERI_AO_BCRM_BASE 0x11035000
#define VLP_AO_BCRM_BASE 0x1c017000
#define MCUSYS_TOP_BASE 0xc000000
#define MCUSYS_CPC_BASE 0xc040000
#define MP_CPU4_TOP_BASE 0xc1c0000
#define MP_CPU5_TOP_BASE 0xc1d0000
#define MP_CPU6_TOP_BASE 0xc1e0000
#define MP_CPU7_TOP_BASE 0xc1f0000
#endif /* defined(CONFIG_OF)) */

/* Register Definition */
#define INFRA_AXIMEM_IDLE_BIT_EN_0              (INFRACFG_AO_BASE + 0xa30)
#define INFRA_BUS_DCM_CTRL                      (INFRACFG_AO_BASE + 0x70)
#define MCUSYS_CPC_CPC_DCM_Enable               (MCUSYS_CPC_BASE + 0x19c)
#define MCUSYS_TOP_ADB_FIFO_DCM_EN              (MCUSYS_TOP_BASE + 0x278)
#define MCUSYS_TOP_CBIP_CABGEN_1TO2_CONFIG      (MCUSYS_TOP_BASE + 0x2ac)
#define MCUSYS_TOP_CBIP_CABGEN_2TO1_CONFIG      (MCUSYS_TOP_BASE + 0x2a4)
#define MCUSYS_TOP_CBIP_CABGEN_2TO5_CONFIG      (MCUSYS_TOP_BASE + 0x2b0)
#define MCUSYS_TOP_CBIP_CABGEN_3TO1_CONFIG      (MCUSYS_TOP_BASE + 0x2a0)
#define MCUSYS_TOP_CBIP_CABGEN_4TO2_CONFIG      (MCUSYS_TOP_BASE + 0x2a8)
#define MCUSYS_TOP_CBIP_P2P_CONFIG0             (MCUSYS_TOP_BASE + 0x2b4)
#define MCUSYS_TOP_L3GIC_ARCH_CG_CONFIG         (MCUSYS_TOP_BASE + 0x294)
#define MCUSYS_TOP_MP0_DCM_CFG0                 (MCUSYS_TOP_BASE + 0x27c)
#define MCUSYS_TOP_MP_ADB_DCM_CFG0              (MCUSYS_TOP_BASE + 0x270)
#define MCUSYS_TOP_QDCM_CONFIG0                 (MCUSYS_TOP_BASE + 0x280)
#define MCUSYS_TOP_QDCM_CONFIG1                 (MCUSYS_TOP_BASE + 0x284)
#define MCUSYS_TOP_QDCM_CONFIG2                 (MCUSYS_TOP_BASE + 0x288)
#define MCUSYS_TOP_QDCM_CONFIG3                 (MCUSYS_TOP_BASE + 0x28c)
#define MP_CPU4_TOP_PTP3_CPU_PCSM_SW_PCHANNEL   (MP_CPU4_TOP_BASE + 0x8)
#define MP_CPU4_TOP_STALL_DCM_CONF              (MP_CPU4_TOP_BASE + 0x30)
#define MP_CPU5_TOP_PTP3_CPU_PCSM_SW_PCHANNEL   (MP_CPU5_TOP_BASE + 0x8)
#define MP_CPU5_TOP_STALL_DCM_CONF              (MP_CPU5_TOP_BASE + 0x30)
#define MP_CPU6_TOP_PTP3_CPU_PCSM_SW_PCHANNEL   (MP_CPU6_TOP_BASE + 0x8)
#define MP_CPU6_TOP_STALL_DCM_CONF              (MP_CPU6_TOP_BASE + 0x30)
#define MP_CPU7_TOP_PTP3_CPU_PCSM_SW_PCHANNEL   (MP_CPU7_TOP_BASE + 0x8)
#define MP_CPU7_TOP_STALL_DCM_CONF              (MP_CPU7_TOP_BASE + 0x30)
#define P2P_RX_CLK_ON                           (INFRACFG_AO_BASE + 0xa0)
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
void dcm_infra_ao_bcrm_infra_bus_dcm(int on);
void dcm_infra_ao_bcrm_infra_bus_fmem_sub_dcm(int on);
void dcm_peri_ao_bcrm_peri_bus_dcm(int on);
void dcm_vlp_ao_bcrm_vlp_bus_dcm(int on);
void dcm_mcusys_top_mcu_acp_dcm(int on);
void dcm_mcusys_top_mcu_adb_dcm(int on);
void dcm_mcusys_top_mcu_apb_dcm(int on);
void dcm_mcusys_top_mcu_bus_qdcm(int on);
void dcm_mcusys_top_mcu_cbip_dcm(int on);
void dcm_mcusys_top_mcu_core_qdcm(int on);
void dcm_mcusys_top_mcu_io_dcm(int on);
void dcm_mcusys_top_mcu_stalldcm(int on);
void dcm_mcusys_cpc_cpc_pbi_dcm(int on);
void dcm_mcusys_cpc_cpc_turbo_dcm(int on);
void dcm_mp_cpu4_top_mcu_apb_dcm(int on);
void dcm_mp_cpu4_top_mcu_stalldcm(int on);
void dcm_mp_cpu5_top_mcu_apb_dcm(int on);
void dcm_mp_cpu5_top_mcu_stalldcm(int on);
void dcm_mp_cpu6_top_mcu_apb_dcm(int on);
void dcm_mp_cpu6_top_mcu_stalldcm(int on);
void dcm_mp_cpu7_top_mcu_apb_dcm(int on);
void dcm_mp_cpu7_top_mcu_stalldcm(int on);
#endif /* __MTK_DCM_AUTOGEN_H__ */
