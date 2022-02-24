// SPDX-License-Identifier: GPL-2.0+
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
extern unsigned long dcm_emi_base;
extern unsigned long dcm_dramc_ch0_top0_base;
extern unsigned long dcm_chn0_emi_base;
extern unsigned long dcm_dramc_ch0_top5_base;
extern unsigned long dcm_dramc_ch1_top0_base;
extern unsigned long dcm_dramc_ch1_top5_base;
extern unsigned long dcm_sspm_base;
extern unsigned long dcm_audio_base;
extern unsigned long dcm_mp_cpusys_top_base;
extern unsigned long dcm_cpccfg_reg_base;

#define INFRACFG_AO_BASE (dcm_infracfg_ao_base)
#define INFRACFG_AO_MEM_BASE (dcm_infracfg_ao_mem_base)
#define INFRA_AO_BCRM_BASE (dcm_infra_ao_bcrm_base)
#define EMI_BASE (dcm_emi_base)
#define DRAMC_CH0_TOP0_BASE (dcm_dramc_ch0_top0_base)
#define CHN0_EMI_BASE (dcm_chn0_emi_base)
#define DRAMC_CH0_TOP5_BASE (dcm_dramc_ch0_top5_base)
#define DRAMC_CH1_TOP0_BASE (dcm_dramc_ch1_top0_base)
#define DRAMC_CH1_TOP5_BASE (dcm_dramc_ch1_top5_base)
#define SSPM_BASE (dcm_sspm_base)
#define AUDIO_BASE (dcm_audio_base)
#define MP_CPUSYS_TOP_BASE (dcm_mp_cpusys_top_base)
#define CPCCFG_REG_BASE (dcm_cpccfg_reg_base)

#else /* !(defined(__KERNEL__) && defined(CONFIG_OF)) */

/* Here below used in CTP and lk for references. */
#undef INFRACFG_AO_BASE
#undef INFRACFG_AO_MEM_BASE
#undef EMI_BASE
#undef DRAMC_CH0_TOP0_BASE
#undef CHN0_EMI_BASE
#undef DRAMC_CH0_TOP5_BASE
#undef DRAMC_CH1_TOP0_BASE
#undef DRAMC_CH1_TOP5_BASE
#undef SSPM_BASE
#undef AUDIO_BASE
#undef MP_CPUSYS_TOP_BASE
#undef CPCCFG_REG_BASE

/* Base */
#define INFRACFG_AO_BASE 0x10001000
#define INFRACFG_AO_MEM_BASE 0x10002000
#define INFRA_AO_BCRM_BASE 0x10022000
#define EMI_BASE 0x10219000
#define DRAMC_CH0_TOP0_BASE 0x10230000
#define CHN0_EMI_BASE 0x10235000
#define DRAMC_CH0_TOP5_BASE 0x10238000
#define DRAMC_CH1_TOP0_BASE 0x10240000
#define DRAMC_CH1_TOP5_BASE 0x10248000
#define SSPM_BASE 0x10400000
#define AUDIO_BASE 0x11210000
#define MP_CPUSYS_TOP_BASE 0xc538000
#define CPCCFG_REG_BASE 0xc53a800
#endif /* #if defined(__KERNEL__) && defined(CONFIG_OF) */

/* Register Definition */
#define CPU_PLLDIV_CFG0 (MP_CPUSYS_TOP_BASE + 0x22a0)
#define CPU_PLLDIV_CFG1 (MP_CPUSYS_TOP_BASE + 0x22a4)
#define CPU_PLLDIV_CFG2 (MP_CPUSYS_TOP_BASE + 0x22a8)
#define CPU_PLLDIV_CFG3 (MP_CPUSYS_TOP_BASE + 0x22ac)
#define CPU_PLLDIV_CFG4 (MP_CPUSYS_TOP_BASE + 0x22b0)
#define BUS_PLLDIV_CFG (MP_CPUSYS_TOP_BASE + 0x22e0)
#define MCSI_DCM0 (MP_CPUSYS_TOP_BASE + 0x2440)
#define MP_ADB_DCM_CFG0 (MP_CPUSYS_TOP_BASE + 0x2500)
#define MP_ADB_DCM_CFG4 (MP_CPUSYS_TOP_BASE + 0x2510)
#define MP_MISC_DCM_CFG0 (MP_CPUSYS_TOP_BASE + 0x2518)
#define MCUSYS_DCM_CFG0 (MP_CPUSYS_TOP_BASE + 0x25c0)
#define EMI_WFIFO (CPCCFG_REG_BASE + 0x100)
#define MP0_DCM_CFG0 (MP_CPUSYS_TOP_BASE + 0x4880)
#define MP0_DCM_CFG7 (MP_CPUSYS_TOP_BASE + 0x489c)
#define INFRA_BUS_DCM_CTRL (INFRACFG_AO_BASE + 0x70)
#define PERI_BUS_DCM_CTRL (INFRACFG_AO_BASE + 0x74)
#define P2P_RX_CLK_ON (INFRACFG_AO_BASE + 0xa0)
#define MODULE_SW_CG_2_SET (INFRACFG_AO_BASE + 0xa4)
#define MODULE_SW_CG_2_CLR (INFRACFG_AO_BASE + 0xa8)
#define INFRA_AXIMEM_IDLE_BIT_EN_0 (INFRACFG_AO_BASE + 0xa30)
#define INFRA_EMI_DCM_CFG0 (INFRACFG_AO_MEM_BASE + 0x28)
#define INFRA_EMI_DCM_CFG1 (INFRACFG_AO_MEM_BASE + 0x2c)
#define INFRA_EMI_DCM_CFG3 (INFRACFG_AO_MEM_BASE + 0x34)
#define TOP_CK_ANCHOR_CFG (INFRACFG_AO_MEM_BASE + 0x38)
#define VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0 \
	(INFRA_AO_BCRM_BASE + 0x0)
#define VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_1 \
	(INFRA_AO_BCRM_BASE + 0x4)
#define VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_2 \
	(INFRA_AO_BCRM_BASE + 0x8)
#define VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_10 \
	(INFRA_AO_BCRM_BASE + 0x28)
#define EMI_CONM (EMI_BASE + 0x60)
#define EMI_CONN (EMI_BASE + 0x68)
#define EMI_THRO_CTRL0 (EMI_BASE + 0x830)
#define DRAMC_CH0_TOP0_DDRCOMMON0 (DRAMC_CH0_TOP0_BASE + 0x0)
#define DRAMC_CH0_TOP0_TEST2_A0 (DRAMC_CH0_TOP0_BASE + 0x100)
#define DRAMC_CH0_TOP0_TEST2_A3 (DRAMC_CH0_TOP0_BASE + 0x108)
#define DRAMC_CH0_TOP0_DUMMY_RD (DRAMC_CH0_TOP0_BASE + 0x110)
#define DRAMC_CH0_TOP0_SREF_DPD_CTRL (DRAMC_CH0_TOP0_BASE + 0x168)
#define DRAMC_CH0_TOP0_ACTIMING_CTRL (DRAMC_CH0_TOP0_BASE + 0x190)
#define DRAMC_CH0_TOP0_ZQ_SET0 (DRAMC_CH0_TOP0_BASE + 0x1a0)
#define DRAMC_CH0_TOP0_TX_TRACKING_SET0 (DRAMC_CH0_TOP0_BASE + 0x1b0)
#define DRAMC_CH0_TOP0_DRAMC_PD_CTRL (DRAMC_CH0_TOP0_BASE + 0x1fc)
#define DRAMC_CH0_TOP0_DCM_CTRL0 (DRAMC_CH0_TOP0_BASE + 0x200)
#define DRAMC_CH0_TOP0_DVFS_CTRL0 (DRAMC_CH0_TOP0_BASE + 0x208)
#define DRAMC_CH0_TOP0_CMD_DEC_CTRL0 (DRAMC_CH0_TOP0_BASE + 0x21c)
#define DRAMC_CH0_TOP0_TX_CG_SET0 (DRAMC_CH0_TOP0_BASE + 0x23c)
#define DRAMC_CH0_TOP0_RX_CG_SET0 (DRAMC_CH0_TOP0_BASE + 0x244)
#define DRAMC_CH0_TOP0_MISCTL0 (DRAMC_CH0_TOP0_BASE + 0x250)
#define DRAMC_CH0_TOP0_CLKAR (DRAMC_CH0_TOP0_BASE + 0x260)
#define DRAMC_CH0_TOP0_SCSMCTRL_CG (DRAMC_CH0_TOP0_BASE + 0x288)
#define DRAMC_CH0_TOP0_SHU_APHY_TX_PICG_CTRL (DRAMC_CH0_TOP0_BASE + 0x16ac)
#define CHN0_EMI_CHN_EMI_CONB (CHN0_EMI_BASE + 0x8)
#define DRAMC_CH0_TOP5_MISC_CG_CTRL0 (DRAMC_CH0_TOP5_BASE + 0x4ec)
#define DRAMC_CH0_TOP5_MISC_CG_CTRL2 (DRAMC_CH0_TOP5_BASE + 0x4f4)
#define DRAMC_CH0_TOP5_MISC_CG_CTRL5 (DRAMC_CH0_TOP5_BASE + 0x500)
#define DRAMC_CH0_TOP5_MISC_DUTYSCAN1 (DRAMC_CH0_TOP5_BASE + 0x624)
#define DRAMC_CH0_TOP5_MISC_CTRL3 (DRAMC_CH0_TOP5_BASE + 0x648)
#define DRAMC_CH0_TOP5_MISC_CTRL4 (DRAMC_CH0_TOP5_BASE + 0x64c)
#define DRAMC_CH0_TOP5_MISC_RX_AUTOK_CFG0 (DRAMC_CH0_TOP5_BASE + 0x670)
#define DRAMC_CH0_TOP5_SHU_B0_DQ7 (DRAMC_CH0_TOP5_BASE + 0x884)
#define DRAMC_CH0_TOP5_SHU_B0_DQ8 (DRAMC_CH0_TOP5_BASE + 0x888)
#define DRAMC_CH0_TOP5_SHU_B1_DQ7 (DRAMC_CH0_TOP5_BASE + 0xa04)
#define DRAMC_CH0_TOP5_SHU_B1_DQ8 (DRAMC_CH0_TOP5_BASE + 0xa08)
#define DRAMC_CH0_TOP5_SHU_CA_CMD8 (DRAMC_CH0_TOP5_BASE + 0xb88)
#define DRAMC_CH0_TOP5_MISC_SHU_RX_CG_CTRL (DRAMC_CH0_TOP5_BASE + 0xda0)
#define DRAMC_CH0_TOP5_MISC_SHU_CG_CTRL0 (DRAMC_CH0_TOP5_BASE + 0xda4)
#define DRAMC_CH1_TOP0_DDRCOMMON0 (DRAMC_CH1_TOP0_BASE + 0x0)
#define DRAMC_CH1_TOP0_TEST2_A0 (DRAMC_CH1_TOP0_BASE + 0x100)
#define DRAMC_CH1_TOP0_TEST2_A3 (DRAMC_CH1_TOP0_BASE + 0x108)
#define DRAMC_CH1_TOP0_DUMMY_RD (DRAMC_CH1_TOP0_BASE + 0x110)
#define DRAMC_CH1_TOP0_SREF_DPD_CTRL (DRAMC_CH1_TOP0_BASE + 0x168)
#define DRAMC_CH1_TOP0_ACTIMING_CTRL (DRAMC_CH1_TOP0_BASE + 0x190)
#define DRAMC_CH1_TOP0_ZQ_SET0 (DRAMC_CH1_TOP0_BASE + 0x1a0)
#define DRAMC_CH1_TOP0_TX_TRACKING_SET0 (DRAMC_CH1_TOP0_BASE + 0x1b0)
#define DRAMC_CH1_TOP0_DRAMC_PD_CTRL (DRAMC_CH1_TOP0_BASE + 0x1fc)
#define DRAMC_CH1_TOP0_DCM_CTRL0 (DRAMC_CH1_TOP0_BASE + 0x200)
#define DRAMC_CH1_TOP0_DVFS_CTRL0 (DRAMC_CH1_TOP0_BASE + 0x208)
#define DRAMC_CH1_TOP0_CMD_DEC_CTRL0 (DRAMC_CH1_TOP0_BASE + 0x21c)
#define DRAMC_CH1_TOP0_TX_CG_SET0 (DRAMC_CH1_TOP0_BASE + 0x23c)
#define DRAMC_CH1_TOP0_RX_CG_SET0 (DRAMC_CH1_TOP0_BASE + 0x244)
#define DRAMC_CH1_TOP0_MISCTL0 (DRAMC_CH1_TOP0_BASE + 0x250)
#define DRAMC_CH1_TOP0_CLKAR (DRAMC_CH1_TOP0_BASE + 0x260)
#define DRAMC_CH1_TOP0_SCSMCTRL_CG (DRAMC_CH1_TOP0_BASE + 0x288)
#define DRAMC_CH1_TOP0_SHU_APHY_TX_PICG_CTRL (DRAMC_CH1_TOP0_BASE + 0x16ac)
#define DRAMC_CH1_TOP5_MISC_CG_CTRL0 (DRAMC_CH1_TOP5_BASE + 0x4ec)
#define DRAMC_CH1_TOP5_MISC_CG_CTRL2 (DRAMC_CH1_TOP5_BASE + 0x4f4)
#define DRAMC_CH1_TOP5_MISC_CG_CTRL5 (DRAMC_CH1_TOP5_BASE + 0x500)
#define DRAMC_CH1_TOP5_MISC_DUTYSCAN1 (DRAMC_CH1_TOP5_BASE + 0x624)
#define DRAMC_CH1_TOP5_MISC_CTRL3 (DRAMC_CH1_TOP5_BASE + 0x648)
#define DRAMC_CH1_TOP5_MISC_CTRL4 (DRAMC_CH1_TOP5_BASE + 0x64c)
#define DRAMC_CH1_TOP5_MISC_RX_AUTOK_CFG0 (DRAMC_CH1_TOP5_BASE + 0x670)
#define DRAMC_CH1_TOP5_SHU_B0_DQ7 (DRAMC_CH1_TOP5_BASE + 0x884)
#define DRAMC_CH1_TOP5_SHU_B0_DQ8 (DRAMC_CH1_TOP5_BASE + 0x888)
#define DRAMC_CH1_TOP5_SHU_B1_DQ7 (DRAMC_CH1_TOP5_BASE + 0xa04)
#define DRAMC_CH1_TOP5_SHU_B1_DQ8 (DRAMC_CH1_TOP5_BASE + 0xa08)
#define DRAMC_CH1_TOP5_SHU_CA_CMD8 (DRAMC_CH1_TOP5_BASE + 0xb88)
#define DRAMC_CH1_TOP5_MISC_SHU_RX_CG_CTRL (DRAMC_CH1_TOP5_BASE + 0xda0)
#define DRAMC_CH1_TOP5_MISC_SHU_CG_CTRL0 (DRAMC_CH1_TOP5_BASE + 0xda4)
#define SSPM_MCLK_DIV (SSPM_BASE + 0x43004)
#define SSPM_DCM_CTRL (SSPM_BASE + 0x43008)
#define AUDIO_TOP_CON0 (AUDIO_BASE + 0x0)

/* INFRACFG_AO */
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
/* INFRACFG_AO_MEM */
bool dcm_infracfg_ao_mem_dcm_emi_group_is_on(void);
void dcm_infracfg_ao_mem_dcm_emi_group(int on);
/* INFRA_AO_BCRM */
bool dcm_infra_ao_bcrm_infra_bus_dcm_is_on(void);
void dcm_infra_ao_bcrm_infra_bus_dcm(int on);
bool dcm_infra_ao_bcrm_peri_bus_dcm_is_on(void);
void dcm_infra_ao_bcrm_peri_bus_dcm(int on);
/* EMI */
bool dcm_emi_emi_dcm_is_on(void);
void dcm_emi_emi_dcm(int on);
/* DRAMC_CH0_TOP0 */
bool dcm_dramc_ch0_top0_ddrphy_is_on(void);
void dcm_dramc_ch0_top0_ddrphy(int on);
/* CHN0_EMI */
bool dcm_chn0_emi_chn_emi_dcm_is_on(void);
void dcm_chn0_emi_chn_emi_dcm(int on);
/* DRAMC_CH0_TOP5 */
bool dcm_dramc_ch0_top5_ddrphy_is_on(void);
void dcm_dramc_ch0_top5_ddrphy(int on);
/* DRAMC_CH1_TOP0 */
bool dcm_dramc_ch1_top0_ddrphy_is_on(void);
void dcm_dramc_ch1_top0_ddrphy(int on);
/* DRAMC_CH1_TOP5 */
bool dcm_dramc_ch1_top5_ddrphy_is_on(void);
void dcm_dramc_ch1_top5_ddrphy(int on);
/* SSPM */
bool dcm_sspm_sspm_dcm_is_on(void);
void dcm_sspm_sspm_dcm(int on);
/* AUDIO */
bool dcm_audio_aud_mas_ahb_ck_dcm_is_on(void);
void dcm_audio_aud_mas_ahb_ck_dcm(int on);
/* MP_CPUSYS_TOP */
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
bool dcm_mp_cpusys_top_cpu_pll_div_2_dcm_is_on(void);
void dcm_mp_cpusys_top_cpu_pll_div_2_dcm(int on);
bool dcm_mp_cpusys_top_cpu_pll_div_3_dcm_is_on(void);
void dcm_mp_cpusys_top_cpu_pll_div_3_dcm(int on);
bool dcm_mp_cpusys_top_cpu_pll_div_4_dcm_is_on(void);
void dcm_mp_cpusys_top_cpu_pll_div_4_dcm(int on);
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

