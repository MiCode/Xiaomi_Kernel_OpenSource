/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MTK_DCM_AUTOGEN_H__
#define __MTK_DCM_AUTOGEN_H__

#include <mtk_dcm.h>

#if defined(__KERNEL__) && defined(CONFIG_OF)
extern unsigned long dcm_infracfg_ao_base;
extern unsigned long dcm_topckgen_ao_base;
extern unsigned long dcm_mcucfg_base;
extern unsigned long dcm_mcucfg_phys_base;
extern unsigned long dcm_dramc_base;
extern unsigned long dcm_emi_base;
extern unsigned long dcm_chn0_emi_base;

#define INFRACFG_AO_BASE	(dcm_infracfg_ao_base)
#define TOPCKGEN_AO_BASE	(dcm_topckgen_ao_base)
#define MCUCFG_BASE		(dcm_mcucfg_base)
#define DRAMC_BASE	(dcm_dramc_base)
#define EMI_BASE		(dcm_emi_base)
#define CHN0_EMI_BASE	(dcm_chn0_emi_base)
#else /* !(defined(__KERNEL__) && defined(CONFIG_OF)) */
#undef INFRACFG_AO_BASE
#undef TOPCKGEN_AO_BASE
#undef MCUCFG_BASE
#undef DRAMC_BASE
#undef EMI_BASE
#undef CHN0_EMI_BASE

/* Base */
#define INFRACFG_AO_BASE 0x10001000
#define TOPCKGEN_AO_BASE 0x1001b000
#define DRAMC_BASE 0x1001d000
#define MCUCFG_BASE 0x10200000
#define EMI_BASE 0x10219000
#define CHN0_EMI_BASE 0x1021a000
#endif /* #if defined(__KERNEL__) && defined(CONFIG_OF) */

/* Register Definition */
#define INFRA_BUS_DCM_CTRL (INFRACFG_AO_BASE + 0x70)
#define PERI_BUS_DCM_CTRL (INFRACFG_AO_BASE + 0x74)
#define MEM_DCM_CTRL (INFRACFG_AO_BASE + 0x78)
#define DFS_MEM_DCM_CTRL (INFRACFG_AO_BASE + 0x7c)
#define P2P_RX_CLK_ON (INFRACFG_AO_BASE + 0xa0)
#define INFRA_TOPCKGEN_DCMCTL (TOPCKGEN_AO_BASE + 0x8)
#define DRAMC_DRAMC_PD_CTRL (DRAMC_BASE + 0x1dc)
#define L2C_SRAM_CTRL (MCUCFG_BASE + 0x648)
#define CCI_CLK_CTRL (MCUCFG_BASE + 0x660)
#define BUS_FABRIC_DCM_CTRL (MCUCFG_BASE + 0x668)
#define MCU_MISC_DCM_CTRL (MCUCFG_BASE + 0x66c)
#define EMI_CONM (EMI_BASE + 0x60)
#define EMI_CONN (EMI_BASE + 0x68)
#define CHN0_EMI_CHN_EMI_CONB (CHN0_EMI_BASE + 0x8)

/* INFRACFG_AO */
bool dcm_infracfg_ao_dcm_dfs_mem_ctrl_is_on(void);
void dcm_infracfg_ao_dcm_dfs_mem_ctrl(int on);
bool dcm_infracfg_ao_dcm_infra_bus_is_on(void);
void dcm_infracfg_ao_dcm_infra_bus(int on);
bool dcm_infracfg_ao_dcm_mem_ctrl_is_on(void);
void dcm_infracfg_ao_dcm_mem_ctrl(int on);
bool dcm_infracfg_ao_dcm_peri_bus_is_on(void);
void dcm_infracfg_ao_dcm_peri_bus(int on);
bool dcm_infracfg_ao_dcm_top_p2p_rx_ck_is_on(void);
void dcm_infracfg_ao_dcm_top_p2p_rx_ck(int on);
/* TOPCKGEN_AO */
bool dcm_topckgen_ao_mcu_armpll_ca7ll_is_on(void);
void dcm_topckgen_ao_mcu_armpll_ca7ll(int on);
/* DRAMC */
bool dcm_dramc_dramc_dcm_is_on(void);
void dcm_dramc_dramc_dcm(int on);
/* MCUCFG */
bool dcm_mcucfg_bus_clock_dcm_is_on(void);
void dcm_mcucfg_bus_clock_dcm(int on);
bool dcm_mcucfg_bus_fabric_dcm_is_on(void);
void dcm_mcucfg_bus_fabric_dcm(int on);
bool dcm_mcucfg_l2_shared_dcm_is_on(void);
void dcm_mcucfg_l2_shared_dcm(int on);
bool dcm_mcucfg_mcu_misc_dcm_is_on(void);
void dcm_mcucfg_mcu_misc_dcm(int on);
/* EMI */
bool dcm_emi_dcm_emi_group_is_on(void);
void dcm_emi_dcm_emi_group(int on);
/* CHN0_EMI */
bool dcm_chn0_emi_dcm_emi_group_is_on(void);
void dcm_chn0_emi_dcm_emi_group(int on);

#endif
