/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MTK_SPM_INTERNAL_H__
#define __MTK_SPM_INTERNAL_H__

#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>
#include <linux/io.h>
#include <mt-plat/aee.h>

#include "mtk_spm.h"
#include <mtk_lpae.h>

/**************************************
 * Config and Parameter
 **************************************/
/* SPM_POWER_ON_VAL1 */
#define POWER_ON_VAL1_DEF	0x00015820
/* PCM_FSM_STA */
#define PCM_FSM_STA_DEF		0x00048490
/* SPM_WAKEUP_EVENT_MASK */
#define SPM_WAKEUP_EVENT_MASK_DEF	0xF0F92218

/* PCM_WDT_VAL */
#define PCM_WDT_TIMEOUT		(30 * 32768)	/* 30s */
/* PCM_TIMER_VAL */
#define PCM_TIMER_MAX		(0xffffffff - PCM_WDT_TIMEOUT)

/**************************************
 * Define and Declare
 **************************************/
/* PCM_PWR_IO_EN */
#define PCM_PWRIO_EN_R0		(1U << 0)
#define PCM_PWRIO_EN_R7		(1U << 7)
#define PCM_RF_SYNC_R0		(1U << 16)
#define PCM_RF_SYNC_R6		(1U << 22)
#define PCM_RF_SYNC_R7		(1U << 23)

/* SPM_SWINT */
#define PCM_SW_INT0		(1U << 0)
#define PCM_SW_INT1		(1U << 1)
#define PCM_SW_INT2		(1U << 2)
#define PCM_SW_INT3		(1U << 3)
#define PCM_SW_INT4		(1U << 4)
#define PCM_SW_INT5		(1U << 5)
#define PCM_SW_INT6		(1U << 6)
#define PCM_SW_INT7		(1U << 7)
#define PCM_SW_INT8		(1U << 8)
#define PCM_SW_INT9		(1U << 9)
#define PCM_SW_INT_ALL		(PCM_SW_INT9 | PCM_SW_INT8 | PCM_SW_INT7 | \
				 PCM_SW_INT6 | PCM_SW_INT5 | PCM_SW_INT4 | \
				 PCM_SW_INT3 | PCM_SW_INT2 | PCM_SW_INT1 | \
				 PCM_SW_INT0)

/* SPM_AP_STANDBY_CON */
#define WFI_OP_AND		1
#define WFI_OP_OR		0

/* SPM_IRQ_MASK */
#define ISRM_TWAM		(1U << 2)
#define ISRM_PCM_RETURN		(1U << 3)
#define ISRM_RET_IRQ0		(1U << 8)
#define ISRM_RET_IRQ1		(1U << 9)
#define ISRM_RET_IRQ2		(1U << 10)
#define ISRM_RET_IRQ3		(1U << 11)
#define ISRM_RET_IRQ4		(1U << 12)
#define ISRM_RET_IRQ5		(1U << 13)
#define ISRM_RET_IRQ6		(1U << 14)
#define ISRM_RET_IRQ7		(1U << 15)
#define ISRM_RET_IRQ8		(1U << 16)
#define ISRM_RET_IRQ9		(1U << 17)
#define ISRM_RET_IRQ_AUX	(ISRM_RET_IRQ9 | ISRM_RET_IRQ8 | \
				 ISRM_RET_IRQ7 | ISRM_RET_IRQ6 | \
				 ISRM_RET_IRQ5 | ISRM_RET_IRQ4 | \
				 ISRM_RET_IRQ3 | ISRM_RET_IRQ2 | \
				 ISRM_RET_IRQ1)
#define ISRM_ALL_EXC_TWAM \
	(ISRM_RET_IRQ_AUX /*| ISRM_RET_IRQ0 | ISRM_PCM_RETURN*/)
#define ISRM_ALL		(ISRM_ALL_EXC_TWAM | ISRM_TWAM)

/* SPM_IRQ_STA */
#define ISRS_TWAM		(1U << 2)
#define ISRS_PCM_RETURN		(1U << 3)
#define ISRS_SW_INT0		(1U << 4)
#define ISRS_SW_INT1		(1U << 5)
#define ISRC_TWAM		ISRS_TWAM
#define ISRC_ALL_EXC_TWAM	ISRS_PCM_RETURN
#define ISRC_ALL		(ISRC_ALL_EXC_TWAM | ISRC_TWAM)

/* SPM_WAKEUP_MISC */
#define WAKE_MISC_TWAM		(1U << 18)
#define WAKE_MISC_PCM_TIMER	(1U << 19)
#define WAKE_MISC_CPU_WAKE	(1U << 20)

struct pcm_desc {
	const char *version;	/* PCM code version */
	const u32 *base;	/* binary array base */
	dma_addr_t base_dma;	/* dma addr of base */
	const u16 size;		/* binary array size */
	const u8 sess;		/* session number */
	const u8 replace;	/* replace mode */
	const u16 addr_2nd;	/* 2nd binary array size */
	const u16 reserved;	/* for 32bit alignment */

	u32 vec0;		/* event vector 0 config */
	u32 vec1;		/* event vector 1 config */
	u32 vec2;		/* event vector 2 config */
	u32 vec3;		/* event vector 3 config */
	u32 vec4;		/* event vector 4 config */
	u32 vec5;		/* event vector 5 config */
	u32 vec6;		/* event vector 6 config */
	u32 vec7;		/* event vector 7 config */
	u32 vec8;		/* event vector 8 config */
	u32 vec9;		/* event vector 9 config */
	u32 vec10;		/* event vector 10 config */
	u32 vec11;		/* event vector 11 config */
	u32 vec12;		/* event vector 12 config */
	u32 vec13;		/* event vector 13 config */
	u32 vec14;		/* event vector 14 config */
	u32 vec15;		/* event vector 15 config */
};

#if defined(CONFIG_MACH_MT6763)
struct pwr_ctrl {
	/* for SPM */
	u32 pcm_flags;
	u32 pcm_flags_cust;	/* can override pcm_flags */
	u32 pcm_flags_cust_set;	/* set bit of pcm_flags, after pcm_flags_cust */
	u32 pcm_flags_cust_clr;	/* clr bit of pcm_flags, after pcm_flags_cust */
	u32 pcm_flags1;
	u32 pcm_flags1_cust;	/* can override pcm_flags1 */
    /* set bit of pcm_flags1, after pcm_flags1_cust */
	u32 pcm_flags1_cust_set;
	/* clr bit of pcm_flags1, after pcm_flags1_cust */
	u32 pcm_flags1_cust_clr;
	u32 timer_val;		/* @ 1T 32K */
	u32 timer_val_cust;	/* @ 1T 32K, can override timer_val */
	u32 timer_val_ramp_en;		/* stress for dpidle */
	u32 timer_val_ramp_en_sec;	/* stress for suspend */
	u32 wake_src;
	u32 wake_src_cust;	/* can override wake_src */
	u32 wakelock_timer_val;
	u8 wdt_disable;		/* disable wdt in suspend */

	/* Auto-gen Start */

	/* SPM_AP_STANDBY_CON */
	u8 wfi_op;
	u8 mp0_cputop_idle_mask;
	u8 mp1_cputop_idle_mask;
	u8 mcusys_idle_mask;
	u8 mm_mask_b;
	u8 md_ddr_en_0_dbc_en;
	u8 md_ddr_en_1_dbc_en;
	u8 md_mask_b;
	u8 sspm_mask_b;
	u8 lte_mask_b;
	u8 srcclkeni_mask_b;
	u8 md_apsrc_1_sel;
	u8 md_apsrc_0_sel;
	u8 conn_ddr_en_dbc_en;
	u8 conn_mask_b;
	u8 conn_apsrc_sel;

	/* SPM_SRC_REQ */
	u8 spm_apsrc_req;
	u8 spm_f26m_req;
	u8 spm_lte_req;
	u8 spm_infra_req;
	u8 spm_vrf18_req;
	u8 spm_dvfs_req;
	u8 spm_dvfs_force_down;
	u8 spm_ddren_req;
	u8 spm_rsv_src_req;
	u8 spm_ddren_2_req;
	u8 cpu_md_dvfs_sop_force_on;

	/* SPM_SRC_MASK */
	u8 csyspwreq_mask;
	u8 ccif0_md_event_mask_b;
	u8 ccif0_ap_event_mask_b;
	u8 ccif1_md_event_mask_b;
	u8 ccif1_ap_event_mask_b;
	u8 ccifmd_md1_event_mask_b;
	u8 ccifmd_md2_event_mask_b;
	u8 dsi0_vsync_mask_b;
	u8 dsi1_vsync_mask_b;
	u8 dpi_vsync_mask_b;
	u8 isp0_vsync_mask_b;
	u8 isp1_vsync_mask_b;
	u8 md_srcclkena_0_infra_mask_b;
	u8 md_srcclkena_1_infra_mask_b;
	u8 conn_srcclkena_infra_mask_b;
	u8 sspm_srcclkena_infra_mask_b;
	u8 srcclkeni_infra_mask_b;
	u8 md_apsrc_req_0_infra_mask_b;
	u8 md_apsrc_req_1_infra_mask_b;
	u8 conn_apsrcreq_infra_mask_b;
	u8 sspm_apsrcreq_infra_mask_b;
	u8 md_ddr_en_0_mask_b;
	u8 md_ddr_en_1_mask_b;
	u8 md_vrf18_req_0_mask_b;
	u8 md_vrf18_req_1_mask_b;
	u8 md1_dvfs_req_mask;
	u8 cpu_dvfs_req_mask;
	u8 emi_bw_dvfs_req_mask;
	u8 md_srcclkena_0_dvfs_req_mask_b;
	u8 md_srcclkena_1_dvfs_req_mask_b;
	u8 conn_srcclkena_dvfs_req_mask_b;

	/* SPM_SRC2_MASK */
	u8 dvfs_halt_mask_b;
	u8 vdec_req_mask_b;
	u8 gce_req_mask_b;
	u8 cpu_md_dvfs_req_merge_mask_b;
	u8 md_ddr_en_dvfs_halt_mask_b;
	u8 dsi0_vsync_dvfs_halt_mask_b;
	u8 dsi1_vsync_dvfs_halt_mask_b;
	u8 dpi_vsync_dvfs_halt_mask_b;
	u8 isp0_vsync_dvfs_halt_mask_b;
	u8 isp1_vsync_dvfs_halt_mask_b;
	u8 conn_ddr_en_mask_b;
	u8 disp_req_mask_b;
	u8 disp1_req_mask_b;
	u8 mfg_req_mask_b;
	u8 ufs_srcclkena_mask_b;
	u8 ufs_vrf18_req_mask_b;
	u8 ps_c2k_rccif_wake_mask_b;
	u8 l1_c2k_rccif_wake_mask_b;
	u8 sdio_on_dvfs_req_mask_b;
	u8 emi_boost_dvfs_req_mask_b;
	u8 cpu_md_emi_dvfs_req_prot_dis;
	u8 dramc_spcmd_apsrc_req_mask_b;
	u8 emi_boost_dvfs_req_2_mask_b;
	u8 emi_bw_dvfs_req_2_mask;
	u8 gce_vrf18_req_mask_b;

	/* SPM_WAKEUP_EVENT_MASK */
	u32 spm_wakeup_event_mask;

	/* SPM_WAKEUP_EVENT_EXT_MASK */
	u32 spm_wakeup_event_ext_mask;

	/* SPM_SRC3_MASK */
	u8 md_ddr_en_2_0_mask_b;
	u8 md_ddr_en_2_1_mask_b;
	u8 conn_ddr_en_2_mask_b;
	u8 dramc_spcmd_apsrc_req_2_mask_b;
	u8 spare1_ddren_2_mask_b;
	u8 spare2_ddren_2_mask_b;
	u8 ddren_emi_self_refresh_ch0_mask_b;
	u8 ddren_emi_self_refresh_ch1_mask_b;
	u8 ddren_mm_state_mask_b;
	u8 ddren_sspm_apsrc_req_mask_b;
	u8 ddren_dqssoc_req_mask_b;
	u8 ddren2_emi_self_refresh_ch0_mask_b;
	u8 ddren2_emi_self_refresh_ch1_mask_b;
	u8 ddren2_mm_state_mask_b;
	u8 ddren2_sspm_apsrc_req_mask_b;
	u8 ddren2_dqssoc_req_mask_b;

	/* MP0_CPU0_WFI_EN */
	u8 mp0_cpu0_wfi_en;

	/* MP0_CPU1_WFI_EN */
	u8 mp0_cpu1_wfi_en;

	/* MP0_CPU2_WFI_EN */
	u8 mp0_cpu2_wfi_en;

	/* MP0_CPU3_WFI_EN */
	u8 mp0_cpu3_wfi_en;

	/* MP1_CPU0_WFI_EN */
	u8 mp1_cpu0_wfi_en;

	/* MP1_CPU1_WFI_EN */
	u8 mp1_cpu1_wfi_en;

	/* MP1_CPU2_WFI_EN */
	u8 mp1_cpu2_wfi_en;

	/* MP1_CPU3_WFI_EN */
	u8 mp1_cpu3_wfi_en;

	/* Auto-gen End */
};

/* code gen by spm_pwr_ctrl_atf.pl, need struct pwr_ctrl */
enum pwr_ctrl_enum {
	PWR_PCM_FLAGS,
	PWR_PCM_FLAGS_CUST,
	PWR_PCM_FLAGS_CUST_SET,
	PWR_PCM_FLAGS_CUST_CLR,
	PWR_PCM_FLAGS1,
	PWR_PCM_FLAGS1_CUST,
	PWR_PCM_FLAGS1_CUST_SET,
	PWR_PCM_FLAGS1_CUST_CLR,
	PWR_TIMER_VAL,
	PWR_TIMER_VAL_CUST,
	PWR_TIMER_VAL_RAMP_EN,
	PWR_TIMER_VAL_RAMP_EN_SEC,
	PWR_WAKE_SRC,
	PWR_WAKE_SRC_CUST,
	PWR_WAKELOCK_TIMER_VAL,
	PWR_WDT_DISABLE,
	PWR_WFI_OP,
	PWR_MP0_CPUTOP_IDLE_MASK,
	PWR_MP1_CPUTOP_IDLE_MASK,
	PWR_MCUSYS_IDLE_MASK,
	PWR_MM_MASK_B,
	PWR_MD_DDR_EN_0_DBC_EN,
	PWR_MD_DDR_EN_1_DBC_EN,
	PWR_MD_MASK_B,
	PWR_SSPM_MASK_B,
	PWR_LTE_MASK_B,
	PWR_SRCCLKENI_MASK_B,
	PWR_MD_APSRC_1_SEL,
	PWR_MD_APSRC_0_SEL,
	PWR_CONN_DDR_EN_DBC_EN,
	PWR_CONN_MASK_B,
	PWR_CONN_APSRC_SEL,
	PWR_SPM_APSRC_REQ,
	PWR_SPM_F26M_REQ,
	PWR_SPM_LTE_REQ,
	PWR_SPM_INFRA_REQ,
	PWR_SPM_VRF18_REQ,
	PWR_SPM_DVFS_REQ,
	PWR_SPM_DVFS_FORCE_DOWN,
	PWR_SPM_DDREN_REQ,
	PWR_SPM_RSV_SRC_REQ,
	PWR_SPM_DDREN_2_REQ,
	PWR_CPU_MD_DVFS_SOP_FORCE_ON,
	PWR_CSYSPWREQ_MASK,
	PWR_CCIF0_MD_EVENT_MASK_B,
	PWR_CCIF0_AP_EVENT_MASK_B,
	PWR_CCIF1_MD_EVENT_MASK_B,
	PWR_CCIF1_AP_EVENT_MASK_B,
	PWR_CCIFMD_MD1_EVENT_MASK_B,
	PWR_CCIFMD_MD2_EVENT_MASK_B,
	PWR_DSI0_VSYNC_MASK_B,
	PWR_DSI1_VSYNC_MASK_B,
	PWR_DPI_VSYNC_MASK_B,
	PWR_ISP0_VSYNC_MASK_B,
	PWR_ISP1_VSYNC_MASK_B,
	PWR_MD_SRCCLKENA_0_INFRA_MASK_B,
	PWR_MD_SRCCLKENA_1_INFRA_MASK_B,
	PWR_CONN_SRCCLKENA_INFRA_MASK_B,
	PWR_SSPM_SRCCLKENA_INFRA_MASK_B,
	PWR_SRCCLKENI_INFRA_MASK_B,
	PWR_MD_APSRC_REQ_0_INFRA_MASK_B,
	PWR_MD_APSRC_REQ_1_INFRA_MASK_B,
	PWR_CONN_APSRCREQ_INFRA_MASK_B,
	PWR_SSPM_APSRCREQ_INFRA_MASK_B,
	PWR_MD_DDR_EN_0_MASK_B,
	PWR_MD_DDR_EN_1_MASK_B,
	PWR_MD_VRF18_REQ_0_MASK_B,
	PWR_MD_VRF18_REQ_1_MASK_B,
	PWR_MD1_DVFS_REQ_MASK,
	PWR_CPU_DVFS_REQ_MASK,
	PWR_EMI_BW_DVFS_REQ_MASK,
	PWR_MD_SRCCLKENA_0_DVFS_REQ_MASK_B,
	PWR_MD_SRCCLKENA_1_DVFS_REQ_MASK_B,
	PWR_CONN_SRCCLKENA_DVFS_REQ_MASK_B,
	PWR_DVFS_HALT_MASK_B,
	PWR_VDEC_REQ_MASK_B,
	PWR_GCE_REQ_MASK_B,
	PWR_CPU_MD_DVFS_REQ_MERGE_MASK_B,
	PWR_MD_DDR_EN_DVFS_HALT_MASK_B,
	PWR_DSI0_VSYNC_DVFS_HALT_MASK_B,
	PWR_DSI1_VSYNC_DVFS_HALT_MASK_B,
	PWR_DPI_VSYNC_DVFS_HALT_MASK_B,
	PWR_ISP0_VSYNC_DVFS_HALT_MASK_B,
	PWR_ISP1_VSYNC_DVFS_HALT_MASK_B,
	PWR_CONN_DDR_EN_MASK_B,
	PWR_DISP_REQ_MASK_B,
	PWR_DISP1_REQ_MASK_B,
	PWR_MFG_REQ_MASK_B,
	PWR_UFS_SRCCLKENA_MASK_B,
	PWR_UFS_VRF18_REQ_MASK_B,
	PWR_PS_C2K_RCCIF_WAKE_MASK_B,
	PWR_L1_C2K_RCCIF_WAKE_MASK_B,
	PWR_SDIO_ON_DVFS_REQ_MASK_B,
	PWR_EMI_BOOST_DVFS_REQ_MASK_B,
	PWR_CPU_MD_EMI_DVFS_REQ_PROT_DIS,
	PWR_DRAMC_SPCMD_APSRC_REQ_MASK_B,
	PWR_EMI_BOOST_DVFS_REQ_2_MASK_B,
	PWR_EMI_BW_DVFS_REQ_2_MASK,
	PWR_GCE_VRF18_REQ_MASK_B,
	PWR_SPM_WAKEUP_EVENT_MASK,
	PWR_SPM_WAKEUP_EVENT_EXT_MASK,
	PWR_MD_DDR_EN_2_0_MASK_B,
	PWR_MD_DDR_EN_2_1_MASK_B,
	PWR_CONN_DDR_EN_2_MASK_B,
	PWR_DRAMC_SPCMD_APSRC_REQ_2_MASK_B,
	PWR_SPARE1_DDREN_2_MASK_B,
	PWR_SPARE2_DDREN_2_MASK_B,
	PWR_DDREN_EMI_SELF_REFRESH_CH0_MASK_B,
	PWR_DDREN_EMI_SELF_REFRESH_CH1_MASK_B,
	PWR_DDREN_MM_STATE_MASK_B,
	PWR_DDREN_SSPM_APSRC_REQ_MASK_B,
	PWR_DDREN_DQSSOC_REQ_MASK_B,
	PWR_DDREN2_EMI_SELF_REFRESH_CH0_MASK_B,
	PWR_DDREN2_EMI_SELF_REFRESH_CH1_MASK_B,
	PWR_DDREN2_MM_STATE_MASK_B,
	PWR_DDREN2_SSPM_APSRC_REQ_MASK_B,
	PWR_DDREN2_DQSSOC_REQ_MASK_B,
	PWR_MP0_CPU0_WFI_EN,
	PWR_MP0_CPU1_WFI_EN,
	PWR_MP0_CPU2_WFI_EN,
	PWR_MP0_CPU3_WFI_EN,
	PWR_MP1_CPU0_WFI_EN,
	PWR_MP1_CPU1_WFI_EN,
	PWR_MP1_CPU2_WFI_EN,
	PWR_MP1_CPU3_WFI_EN,
	PWR_MAX_COUNT,
};

#elif defined(CONFIG_MACH_MT6739)

struct pwr_ctrl {
	/* for SPM */
	u32 pcm_flags;
	u32 pcm_flags_cust;	/* can override pcm_flags */
	u32 pcm_flags_cust_set;	/* set bit of pcm_flags, after pcm_flags_cust */
	u32 pcm_flags_cust_clr;	/* clr bit of pcm_flags, after pcm_flags_cust */
	u32 pcm_flags1;
	u32 pcm_flags1_cust;	/* can override pcm_flags1 */
	/* set bit of pcm_flags1, after pcm_flags1_cust */
	u32 pcm_flags1_cust_set;
	/* clr bit of pcm_flags1, after pcm_flags1_cust */
	u32 pcm_flags1_cust_clr;
	u32 timer_val;		/* @ 1T 32K */
	u32 timer_val_cust;	/* @ 1T 32K, can override timer_val */
	u32 timer_val_ramp_en;		/* stress for dpidle */
	u32 timer_val_ramp_en_sec;	/* stress for suspend */
	u32 wake_src;
	u32 wake_src_cust;	/* can override wake_src */
	u32 wakelock_timer_val;
	u8 wdt_disable;		/* disable wdt in suspend */

	/* Auto-gen Start */

	/* SPM_AP_STANDBY_CON */
	u8 wfi_op;
	u8 mp0_cputop_idle_mask;
	u8 mcusys_idle_mask;
	u8 mcu_ddren_req_dbc_en;
	u8 mcu_apsrc_sel;
	u8 mm_mask_b;
	u8 md_ddr_en_0_dbc_en;
	u8 md_ddr_en_1_dbc_en;
	u8 md_mask_b;
	u8 lte_mask_b;
	u8 srcclkeni_mask_b;
	u8 md_apsrc_1_sel;
	u8 md_apsrc_0_sel;
	u8 conn_ddr_en_dbc_en;
	u8 conn_mask_b;
	u8 conn_apsrc_sel;
	u8 conn_srcclkena_sel_mask;

	/* SPM_SRC_REQ */
	u8 spm_apsrc_req;
	u8 spm_f26m_req;
	u8 spm_lte_req;
	u8 spm_infra_req;
	u8 spm_vrf18_req;
	u8 spm_dvfs_req;
	u8 spm_dvfs_force_down;
	u8 spm_ddren_req;
	u8 spm_rsv_src_req;
	u8 spm_ddren_2_req;
	u8 cpu_md_dvfs_sop_force_on;

	/* SPM_SRC_MASK */
	u8 csyspwreq_mask;
	u8 ccif0_md_event_mask_b;
	u8 ccif0_ap_event_mask_b;
	u8 ccif1_md_event_mask_b;
	u8 ccif1_ap_event_mask_b;
	u8 ccifmd_md1_event_mask_b;
	u8 ccifmd_md2_event_mask_b;
	u8 dsi0_vsync_mask_b;
	u8 dsi1_vsync_mask_b;
	u8 dpi_vsync_mask_b;
	u8 isp0_vsync_mask_b;
	u8 isp1_vsync_mask_b;
	u8 md_srcclkena_0_infra_mask_b;
	u8 md_srcclkena_1_infra_mask_b;
	u8 conn_srcclkena_infra_mask_b;
	u8 sspm_srcclkena_infra_mask_b;
	u8 srcclkeni_infra_mask_b;
	u8 md_apsrc_req_0_infra_mask_b;
	u8 md_apsrc_req_1_infra_mask_b;
	u8 conn_apsrcreq_infra_mask_b;
	u8 mcu_apsrcreq_infra_mask_b;
	u8 md_ddr_en_0_mask_b;
	u8 md_ddr_en_1_mask_b;
	u8 md_vrf18_req_0_mask_b;
	u8 md_vrf18_req_1_mask_b;
	u8 md1_dvfs_req_mask;
	u8 cpu_dvfs_req_mask;
	u8 emi_bw_dvfs_req_mask;
	u8 md_srcclkena_0_dvfs_req_mask_b;
	u8 md_srcclkena_1_dvfs_req_mask_b;
	u8 conn_srcclkena_dvfs_req_mask_b;

	/* SPM_SRC2_MASK */
	u8 dvfs_halt_mask_b;
	u8 vdec_req_mask_b;
	u8 gce_req_mask_b;
	u8 cpu_md_dvfs_req_merge_mask_b;
	u8 md_ddr_en_dvfs_halt_mask_b;
	u8 dsi0_vsync_dvfs_halt_mask_b;
	u8 dsi1_vsync_dvfs_halt_mask_b;
	u8 dpi_vsync_dvfs_halt_mask_b;
	u8 isp0_vsync_dvfs_halt_mask_b;
	u8 isp1_vsync_dvfs_halt_mask_b;
	u8 conn_ddr_en_mask_b;
	u8 disp_req_mask_b;
	u8 disp1_req_mask_b;
	u8 mfg_req_mask_b;
	u8 mcu_ddren_req_mask_b;
	u8 mcu_apsrc_req_mask_b;
	u8 ps_c2k_rccif_wake_mask_b;
	u8 l1_c2k_rccif_wake_mask_b;
	u8 sdio_on_dvfs_req_mask_b;
	u8 emi_boost_dvfs_req_mask_b;
	u8 cpu_md_emi_dvfs_req_prot_dis;
	u8 dramc_spcmd_apsrc_req_mask_b;
	u8 emi_boost_dvfs_req_2_mask_b;
	u8 emi_bw_dvfs_req_2_mask;
	u8 gce_vrf18_req_mask_b;

	/* SPM_WAKEUP_EVENT_MASK */
	u32 spm_wakeup_event_mask;

	/* SPM_WAKEUP_EVENT_EXT_MASK */
	u32 spm_wakeup_event_ext_mask;

	/* SPM_SRC3_MASK */
	u8 md_ddr_en_2_0_mask_b;
	u8 md_ddr_en_2_1_mask_b;
	u8 conn_ddr_en_2_mask_b;
	u8 dramc_spcmd_apsrc_req_2_mask_b;
	u8 spare1_ddren_2_mask_b;
	u8 spare2_ddren_2_mask_b;
	u8 ddren_emi_self_refresh_ch0_mask_b;
	u8 ddren_emi_self_refresh_ch1_mask_b;
	u8 ddren_mm_state_mask_b;
	u8 ddren_sspm_apsrc_req_mask_b;
	u8 ddren_dqssoc_req_mask_b;
	u8 ddren2_emi_self_refresh_ch0_mask_b;
	u8 ddren2_emi_self_refresh_ch1_mask_b;
	u8 ddren2_mm_state_mask_b;
	u8 ddren2_sspm_apsrc_req_mask_b;
	u8 ddren2_dqssoc_req_mask_b;

	/* MP0_CPU0_WFI_EN */
	u8 mp0_cpu0_wfi_en;

	/* MP0_CPU1_WFI_EN */
	u8 mp0_cpu1_wfi_en;

	/* MP0_CPU2_WFI_EN */
	u8 mp0_cpu2_wfi_en;

	/* MP0_CPU3_WFI_EN */
	u8 mp0_cpu3_wfi_en;

	/* Auto-gen End */
};

/* code gen by spm_pwr_ctrl_atf.pl, need struct pwr_ctrl */
enum pwr_ctrl_enum {
	PWR_PCM_FLAGS,
	PWR_PCM_FLAGS_CUST,
	PWR_PCM_FLAGS_CUST_SET,
	PWR_PCM_FLAGS_CUST_CLR,
	PWR_PCM_FLAGS1,
	PWR_PCM_FLAGS1_CUST,
	PWR_PCM_FLAGS1_CUST_SET,
	PWR_PCM_FLAGS1_CUST_CLR,
	PWR_TIMER_VAL,
	PWR_TIMER_VAL_CUST,
	PWR_TIMER_VAL_RAMP_EN,
	PWR_TIMER_VAL_RAMP_EN_SEC,
	PWR_WAKE_SRC,
	PWR_WAKE_SRC_CUST,
	PWR_WAKELOCK_TIMER_VAL,
	PWR_WDT_DISABLE,
	PWR_WFI_OP,
	PWR_MP0_CPUTOP_IDLE_MASK,
	PWR_MCUSYS_IDLE_MASK,
	PWR_MCU_DDREN_REQ_DBC_EN,
	PWR_MCU_APSRC_SEL,
	PWR_MM_MASK_B,
	PWR_MD_DDR_EN_0_DBC_EN,
	PWR_MD_DDR_EN_1_DBC_EN,
	PWR_MD_MASK_B,
	PWR_LTE_MASK_B,
	PWR_SRCCLKENI_MASK_B,
	PWR_MD_APSRC_1_SEL,
	PWR_MD_APSRC_0_SEL,
	PWR_CONN_DDR_EN_DBC_EN,
	PWR_CONN_MASK_B,
	PWR_CONN_APSRC_SEL,
	PWR_CONN_SRCCLKENA_SEL_MASK,
	PWR_SPM_APSRC_REQ,
	PWR_SPM_F26M_REQ,
	PWR_SPM_LTE_REQ,
	PWR_SPM_INFRA_REQ,
	PWR_SPM_VRF18_REQ,
	PWR_SPM_DVFS_REQ,
	PWR_SPM_DVFS_FORCE_DOWN,
	PWR_SPM_DDREN_REQ,
	PWR_SPM_RSV_SRC_REQ,
	PWR_SPM_DDREN_2_REQ,
	PWR_CPU_MD_DVFS_SOP_FORCE_ON,
	PWR_CSYSPWREQ_MASK,
	PWR_CCIF0_MD_EVENT_MASK_B,
	PWR_CCIF0_AP_EVENT_MASK_B,
	PWR_CCIF1_MD_EVENT_MASK_B,
	PWR_CCIF1_AP_EVENT_MASK_B,
	PWR_CCIFMD_MD1_EVENT_MASK_B,
	PWR_CCIFMD_MD2_EVENT_MASK_B,
	PWR_DSI0_VSYNC_MASK_B,
	PWR_DSI1_VSYNC_MASK_B,
	PWR_DPI_VSYNC_MASK_B,
	PWR_ISP0_VSYNC_MASK_B,
	PWR_ISP1_VSYNC_MASK_B,
	PWR_MD_SRCCLKENA_0_INFRA_MASK_B,
	PWR_MD_SRCCLKENA_1_INFRA_MASK_B,
	PWR_CONN_SRCCLKENA_INFRA_MASK_B,
	PWR_SSPM_SRCCLKENA_INFRA_MASK_B,
	PWR_SRCCLKENI_INFRA_MASK_B,
	PWR_MD_APSRC_REQ_0_INFRA_MASK_B,
	PWR_MD_APSRC_REQ_1_INFRA_MASK_B,
	PWR_CONN_APSRCREQ_INFRA_MASK_B,
	PWR_MCU_APSRCREQ_INFRA_MASK_B,
	PWR_MD_DDR_EN_0_MASK_B,
	PWR_MD_DDR_EN_1_MASK_B,
	PWR_MD_VRF18_REQ_0_MASK_B,
	PWR_MD_VRF18_REQ_1_MASK_B,
	PWR_MD1_DVFS_REQ_MASK,
	PWR_CPU_DVFS_REQ_MASK,
	PWR_EMI_BW_DVFS_REQ_MASK,
	PWR_MD_SRCCLKENA_0_DVFS_REQ_MASK_B,
	PWR_MD_SRCCLKENA_1_DVFS_REQ_MASK_B,
	PWR_CONN_SRCCLKENA_DVFS_REQ_MASK_B,
	PWR_DVFS_HALT_MASK_B,
	PWR_VDEC_REQ_MASK_B,
	PWR_GCE_REQ_MASK_B,
	PWR_CPU_MD_DVFS_REQ_MERGE_MASK_B,
	PWR_MD_DDR_EN_DVFS_HALT_MASK_B,
	PWR_DSI0_VSYNC_DVFS_HALT_MASK_B,
	PWR_DSI1_VSYNC_DVFS_HALT_MASK_B,
	PWR_DPI_VSYNC_DVFS_HALT_MASK_B,
	PWR_ISP0_VSYNC_DVFS_HALT_MASK_B,
	PWR_ISP1_VSYNC_DVFS_HALT_MASK_B,
	PWR_CONN_DDR_EN_MASK_B,
	PWR_DISP_REQ_MASK_B,
	PWR_DISP1_REQ_MASK_B,
	PWR_MFG_REQ_MASK_B,
	PWR_MCU_DDREN_REQ_MASK_B,
	PWR_MCU_APSRC_REQ_MASK_B,
	PWR_PS_C2K_RCCIF_WAKE_MASK_B,
	PWR_L1_C2K_RCCIF_WAKE_MASK_B,
	PWR_SDIO_ON_DVFS_REQ_MASK_B,
	PWR_EMI_BOOST_DVFS_REQ_MASK_B,
	PWR_CPU_MD_EMI_DVFS_REQ_PROT_DIS,
	PWR_DRAMC_SPCMD_APSRC_REQ_MASK_B,
	PWR_EMI_BOOST_DVFS_REQ_2_MASK_B,
	PWR_EMI_BW_DVFS_REQ_2_MASK,
	PWR_GCE_VRF18_REQ_MASK_B,
	PWR_SPM_WAKEUP_EVENT_MASK,
	PWR_SPM_WAKEUP_EVENT_EXT_MASK,
	PWR_MD_DDR_EN_2_0_MASK_B,
	PWR_MD_DDR_EN_2_1_MASK_B,
	PWR_CONN_DDR_EN_2_MASK_B,
	PWR_DRAMC_SPCMD_APSRC_REQ_2_MASK_B,
	PWR_SPARE1_DDREN_2_MASK_B,
	PWR_SPARE2_DDREN_2_MASK_B,
	PWR_DDREN_EMI_SELF_REFRESH_CH0_MASK_B,
	PWR_DDREN_EMI_SELF_REFRESH_CH1_MASK_B,
	PWR_DDREN_MM_STATE_MASK_B,
	PWR_DDREN_SSPM_APSRC_REQ_MASK_B,
	PWR_DDREN_DQSSOC_REQ_MASK_B,
	PWR_DDREN2_EMI_SELF_REFRESH_CH0_MASK_B,
	PWR_DDREN2_EMI_SELF_REFRESH_CH1_MASK_B,
	PWR_DDREN2_MM_STATE_MASK_B,
	PWR_DDREN2_SSPM_APSRC_REQ_MASK_B,
	PWR_DDREN2_DQSSOC_REQ_MASK_B,
	PWR_MP0_CPU0_WFI_EN,
	PWR_MP0_CPU1_WFI_EN,
	PWR_MP0_CPU2_WFI_EN,
	PWR_MP0_CPU3_WFI_EN,
	PWR_MAX_COUNT,
};
#elif defined(CONFIG_MACH_MT6771)
struct pwr_ctrl {

	/* for SPM */
	u32 pcm_flags;
	u32 pcm_flags_cust;	/* can override pcm_flags */
	u32 pcm_flags_cust_set;	/* set bit of pcm_flags, after pcm_flags_cust */
	u32 pcm_flags_cust_clr;	/* clr bit of pcm_flags, after pcm_flags_cust */
	u32 pcm_flags1;
	u32 pcm_flags1_cust;	/* can override pcm_flags1 */
	/* set bit of pcm_flags1, after pcm_flags1_cust */
	u32 pcm_flags1_cust_set;
	/* clr bit of pcm_flags1, after pcm_flags1_cust */
	u32 pcm_flags1_cust_clr;
	u32 timer_val;		/* @ 1T 32K */
	u32 timer_val_cust;	/* @ 1T 32K, can override timer_val */
	u32 timer_val_ramp_en;		/* stress for dpidle */
	u32 timer_val_ramp_en_sec;	/* stress for suspend */
	u32 wake_src;
	u32 wake_src_cust;	/* can override wake_src */
	u32 wakelock_timer_val;
	u8 wdt_disable;		/* disable wdt in suspend */
	/* Auto-gen Start */

	/* SPM_AP_STANDBY_CON */
	u8 wfi_op;
	u8 mp0_cputop_idle_mask;
	u8 mp1_cputop_idle_mask;
	u8 mcusys_idle_mask;
	u8 mm_mask_b;
	u8 md_ddr_en_0_dbc_en;
	u8 md_ddr_en_1_dbc_en;
	u8 md_mask_b;
	u8 sspm_mask_b;
	u8 scp_mask_b;
	u8 srcclkeni_mask_b;
	u8 md_apsrc_1_sel;
	u8 md_apsrc_0_sel;
	u8 conn_ddr_en_dbc_en;
	u8 conn_mask_b;
	u8 conn_apsrc_sel;

	/* SPM_SRC_REQ */
	u8 spm_apsrc_req;
	u8 spm_f26m_req;
	u8 spm_infra_req;
	u8 spm_vrf18_req;
	u8 spm_ddren_req;
	u8 spm_rsv_src_req;
	u8 spm_ddren_2_req;
	u8 cpu_md_dvfs_sop_force_on;

	/* SPM_SRC_MASK */
	u8 csyspwreq_mask;
	u8 ccif0_md_event_mask_b;
	u8 ccif0_ap_event_mask_b;
	u8 ccif1_md_event_mask_b;
	u8 ccif1_ap_event_mask_b;
	u8 ccif2_md_event_mask_b;
	u8 ccif2_ap_event_mask_b;
	u8 ccif3_md_event_mask_b;
	u8 ccif3_ap_event_mask_b;
	u8 md_srcclkena_0_infra_mask_b;
	u8 md_srcclkena_1_infra_mask_b;
	u8 conn_srcclkena_infra_mask_b;
	u8 ufs_infra_req_mask_b;
	u8 srcclkeni_infra_mask_b;
	u8 md_apsrc_req_0_infra_mask_b;
	u8 md_apsrc_req_1_infra_mask_b;
	u8 conn_apsrcreq_infra_mask_b;
	u8 ufs_srcclkena_mask_b;
	u8 md_vrf18_req_0_mask_b;
	u8 md_vrf18_req_1_mask_b;
	u8 ufs_vrf18_req_mask_b;
	u8 gce_vrf18_req_mask_b;
	u8 conn_infra_req_mask_b;
	u8 gce_apsrc_req_mask_b;
	u8 disp0_apsrc_req_mask_b;
	u8 disp1_apsrc_req_mask_b;
	u8 mfg_req_mask_b;
	u8 vdec_req_mask_b;

	/* SPM_SRC2_MASK */
	u8 md_ddr_en_0_mask_b;
	u8 md_ddr_en_1_mask_b;
	u8 conn_ddr_en_mask_b;
	u8 ddren_sspm_apsrc_req_mask_b;
	u8 ddren_scp_apsrc_req_mask_b;
	u8 disp0_ddren_mask_b;
	u8 disp1_ddren_mask_b;
	u8 gce_ddren_mask_b;
	u8 ddren_emi_self_refresh_ch0_mask_b;
	u8 ddren_emi_self_refresh_ch1_mask_b;

	/* SPM_WAKEUP_EVENT_MASK */
	u32 spm_wakeup_event_mask;

	/* SPM_WAKEUP_EVENT_EXT_MASK */
	u32 spm_wakeup_event_ext_mask;

	/* SPM_SRC3_MASK */
	u8 md_ddr_en_2_0_mask_b;
	u8 md_ddr_en_2_1_mask_b;
	u8 conn_ddr_en_2_mask_b;
	u8 ddren2_sspm_apsrc_req_mask_b;
	u8 ddren2_scp_apsrc_req_mask_b;
	u8 disp0_ddren2_mask_b;
	u8 disp1_ddren2_mask_b;
	u8 gce_ddren2_mask_b;
	u8 ddren2_emi_self_refresh_ch0_mask_b;
	u8 ddren2_emi_self_refresh_ch1_mask_b;

	/* MP0_CPU0_WFI_EN */
	u8 mp0_cpu0_wfi_en;

	/* MP0_CPU1_WFI_EN */
	u8 mp0_cpu1_wfi_en;

	/* MP0_CPU2_WFI_EN */
	u8 mp0_cpu2_wfi_en;

	/* MP0_CPU3_WFI_EN */
	u8 mp0_cpu3_wfi_en;

	/* MP1_CPU0_WFI_EN */
	u8 mp1_cpu0_wfi_en;

	/* MP1_CPU1_WFI_EN */
	u8 mp1_cpu1_wfi_en;

	/* MP1_CPU2_WFI_EN */
	u8 mp1_cpu2_wfi_en;

	/* MP1_CPU3_WFI_EN */
	u8 mp1_cpu3_wfi_en;

	/* Auto-gen End */
};
/* code gen by spm_pwr_ctrl_atf.pl, need struct pwr_ctrl */
enum pwr_ctrl_enum {
	PWR_PCM_FLAGS,
	PWR_PCM_FLAGS_CUST,
	PWR_PCM_FLAGS_CUST_SET,
	PWR_PCM_FLAGS_CUST_CLR,
	PWR_PCM_FLAGS1,
	PWR_PCM_FLAGS1_CUST,
	PWR_PCM_FLAGS1_CUST_SET,
	PWR_PCM_FLAGS1_CUST_CLR,
	PWR_TIMER_VAL,
	PWR_TIMER_VAL_CUST,
	PWR_TIMER_VAL_RAMP_EN,
	PWR_TIMER_VAL_RAMP_EN_SEC,
	PWR_WAKE_SRC,
	PWR_WAKE_SRC_CUST,
	PWR_WAKELOCK_TIMER_VAL,
	PWR_WDT_DISABLE,
	/* SPM_AP_STANDBY_CON */
	PWR_WFI_OP,
	PWR_MP0_CPUTOP_IDLE_MASK,
	PWR_MP1_CPUTOP_IDLE_MASK,
	PWR_MCUSYS_IDLE_MASK,
	PWR_MM_MASK_B,
	PWR_MD_DDR_EN_0_DBC_EN,
	PWR_MD_DDR_EN_1_DBC_EN,
	PWR_MD_MASK_B,
	PWR_SSPM_MASK_B,
	PWR_SCP_MASK_B,
	PWR_SRCCLKENI_MASK_B,
	PWR_MD_APSRC_1_SEL,
	PWR_MD_APSRC_0_SEL,
	PWR_CONN_DDR_EN_DBC_EN,
	PWR_CONN_MASK_B,
	PWR_CONN_APSRC_SEL,
	/* SPM_SRC_REQ */
	PWR_SPM_APSRC_REQ,
	PWR_SPM_F26M_REQ,
	PWR_SPM_INFRA_REQ,
	PWR_SPM_VRF18_REQ,
	PWR_SPM_DDREN_REQ,
	PWR_SPM_RSV_SRC_REQ,
	PWR_SPM_DDREN_2_REQ,
	PWR_CPU_MD_DVFS_SOP_FORCE_ON,
	/* SPM_SRC_MASK */
	PWR_CSYSPWREQ_MASK,
	PWR_CCIF0_MD_EVENT_MASK_B,
	PWR_CCIF0_AP_EVENT_MASK_B,
	PWR_CCIF1_MD_EVENT_MASK_B,
	PWR_CCIF1_AP_EVENT_MASK_B,
	PWR_CCIF2_MD_EVENT_MASK_B,
	PWR_CCIF2_AP_EVENT_MASK_B,
	PWR_CCIF3_MD_EVENT_MASK_B,
	PWR_CCIF3_AP_EVENT_MASK_B,
	PWR_MD_SRCCLKENA_0_INFRA_MASK_B,
	PWR_MD_SRCCLKENA_1_INFRA_MASK_B,
	PWR_CONN_SRCCLKENA_INFRA_MASK_B,
	PWR_UFS_INFRA_REQ_MASK_B,
	PWR_SRCCLKENI_INFRA_MASK_B,
	PWR_MD_APSRC_REQ_0_INFRA_MASK_B,
	PWR_MD_APSRC_REQ_1_INFRA_MASK_B,
	PWR_CONN_APSRCREQ_INFRA_MASK_B,
	PWR_UFS_SRCCLKENA_MASK_B,
	PWR_MD_VRF18_REQ_0_MASK_B,
	PWR_MD_VRF18_REQ_1_MASK_B,
	PWR_UFS_VRF18_REQ_MASK_B,
	PWR_GCE_VRF18_REQ_MASK_B,
	PWR_CONN_INFRA_REQ_MASK_B,
	PWR_GCE_APSRC_REQ_MASK_B,
	PWR_DISP0_APSRC_REQ_MASK_B,
	PWR_DISP1_APSRC_REQ_MASK_B,
	PWR_MFG_REQ_MASK_B,
	PWR_VDEC_REQ_MASK_B,
	/* SPM_SRC2_MASK */
	PWR_MD_DDR_EN_0_MASK_B,
	PWR_MD_DDR_EN_1_MASK_B,
	PWR_CONN_DDR_EN_MASK_B,
	PWR_DDREN_SSPM_APSRC_REQ_MASK_B,
	PWR_DDREN_SCP_APSRC_REQ_MASK_B,
	PWR_DISP0_DDREN_MASK_B,
	PWR_DISP1_DDREN_MASK_B,
	PWR_GCE_DDREN_MASK_B,
	PWR_DDREN_EMI_SELF_REFRESH_CH0_MASK_B,
	PWR_DDREN_EMI_SELF_REFRESH_CH1_MASK_B,
	/* SPM_WAKEUP_EVENT_MASK */
	PWR_SPM_WAKEUP_EVENT_MASK,
	/* SPM_WAKEUP_EVENT_EXT_MASK */
	PWR_SPM_WAKEUP_EVENT_EXT_MASK,
	/* SPM_SRC3_MASK */
	PWR_MD_DDR_EN_2_0_MASK_B,
	PWR_MD_DDR_EN_2_1_MASK_B,
	PWR_CONN_DDR_EN_2_MASK_B,
	PWR_DDREN2_SSPM_APSRC_REQ_MASK_B,
	PWR_DDREN2_SCP_APSRC_REQ_MASK_B,
	PWR_DISP0_DDREN2_MASK_B,
	PWR_DISP1_DDREN2_MASK_B,
	PWR_GCE_DDREN2_MASK_B,
	PWR_DDREN2_EMI_SELF_REFRESH_CH0_MASK_B,
	PWR_DDREN2_EMI_SELF_REFRESH_CH1_MASK_B,
	/* MP0_CPU0_WFI_EN */
	PWR_MP0_CPU0_WFI_EN,
	/* MP0_CPU1_WFI_EN */
	PWR_MP0_CPU1_WFI_EN,
	/* MP0_CPU2_WFI_EN */
	PWR_MP0_CPU2_WFI_EN,
	/* MP0_CPU3_WFI_EN */
	PWR_MP0_CPU3_WFI_EN,
	/* MP1_CPU0_WFI_EN */
	PWR_MP1_CPU0_WFI_EN,
	/* MP1_CPU1_WFI_EN */
	PWR_MP1_CPU1_WFI_EN,
	/* MP1_CPU2_WFI_EN */
	PWR_MP1_CPU2_WFI_EN,
	/* MP1_CPU3_WFI_EN */
	PWR_MP1_CPU3_WFI_EN,

	PWR_MAX_COUNT,
};
#endif

enum {
	SPM_SUSPEND,
	SPM_RESUME,
	SPM_DPIDLE_ENTER,
	SPM_DPIDLE_LEAVE,
	SPM_ENTER_SODI,
	SPM_LEAVE_SODI,
	SPM_ENTER_SODI3,
	SPM_LEAVE_SODI3,
	SPM_SUSPEND_PREPARE,
	SPM_POST_SUSPEND,
	SPM_DPIDLE_PREPARE,
	SPM_POST_DPIDLE,
	SPM_SODI_PREPARE,
	SPM_POST_SODI,
	SPM_SODI3_PREPARE,
	SPM_POST_SODI3,
	SPM_VCORE_PWARP_CMD,
	SPM_PWR_CTRL_SUSPEND,
	SPM_PWR_CTRL_DPIDLE,
	SPM_PWR_CTRL_SODI,
	SPM_PWR_CTRL_SODI3,
	SPM_PWR_CTRL_VCOREFS,
};

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT

enum {
	SPM_OPT_SLEEP_DPIDLE  = (1 << 0),
	SPM_OPT_UNIVPLL_STAT  = (1 << 1),
	SPM_OPT_GPS_STAT      = (1 << 2),
	SPM_OPT_VCORE_LP_MODE = (1 << 3),
	SPM_OPT_XO_UFS_OFF    = (1 << 4),
	SPM_OPT_CLKBUF_ENTER_BBLPM = (1 << 5),
	NF_SPM_OPT
};

struct spm_data {
	unsigned int cmd;
	union {
		struct {
			unsigned int sys_timestamp_l;
			unsigned int sys_timestamp_h;
			unsigned int sys_src_clk_l;
			unsigned int sys_src_clk_h;
			unsigned int spm_opt;
		} suspend;
		struct {
			unsigned int vcore_level0;
			unsigned int vcore_level1;
			unsigned int vcore_level2;
			unsigned int vcore_level3;
		} vcorefs;
		struct {
			unsigned int args1;
			unsigned int args2;
			unsigned int args3;
			unsigned int args4;
			unsigned int args5;
			unsigned int args6;
			unsigned int args7;
		} args;
	} u;
};

#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */

struct wake_status {
	u32 assert_pc;		/* PCM_REG_DATA_INI */
	u32 r12;		/* PCM_REG12_DATA */
	u32 r12_ext;		/* PCM_REG12_EXT_DATA */
	u32 raw_sta;		/* SPM_WAKEUP_STA */
	u32 raw_ext_sta;	/* SPM_WAKEUP_EXT_STA */
	u32 wake_misc;		/* SPM_WAKEUP_MISC */
	u32 timer_out;		/* PCM_TIMER_OUT */
	u32 r13;		/* PCM_REG13_DATA */
	u32 idle_sta;		/* SUBSYS_IDLE_STA */
	u32 req_sta;		/* SRC_REQ_STA */
	u32 debug_flag;		/* SPM_SW_DEBUG */
	u32 debug_flag1;	/* WDT_LATCH_SPARE0_FIX */
	u32 event_reg;		/* PCM_EVENT_REG_STA */
	u32 isr;		/* SPM_IRQ_STA */
	u32 log_index;
};

struct spm_lp_scen {
	struct pcm_desc *pcmdesc;
	struct pwr_ctrl *pwrctrl;
	struct wake_status *wakestatus;
};

extern spinlock_t __spm_lock;

extern struct spm_lp_scen __spm_suspend;
extern struct spm_lp_scen __spm_dpidle;
extern struct spm_lp_scen __spm_sodi3;
extern struct spm_lp_scen __spm_sodi;
extern struct spm_lp_scen __spm_vcorefs;

extern int __spm_get_md_srcclkena_setting(void);
extern int __spm_get_pcm_timer_val(const struct pwr_ctrl *pwrctrl);
extern void __spm_sync_pcm_flags(struct pwr_ctrl *pwrctrl);

extern void __spm_get_wakeup_status(struct wake_status *wakesta);
extern unsigned int __spm_output_wake_reason(
		const struct wake_status *wakesta,
		const struct pcm_desc *pcmdesc,
		bool suspend, const char *scenario);

extern void rekick_vcorefs_scenario(void);

/* set dram dummy read address */
void spm_set_dummy_read_addr(int debug);

extern int spm_fs_init(void);

extern int spm_golden_setting_cmp(bool en);
extern void spm_phypll_mode_check(void);
extern u32 _spm_get_wake_period(int pwake_time, unsigned int last_wr);
extern void __sync_big_buck_ctrl_pcm_flag(u32 *flag);
extern void __sync_vcore_ctrl_pcm_flag(u32 oper_cond, u32 *flag);

/**************************************
 * Macro and Inline
 **************************************/
#define EVENT_VEC(event, resume, imme, pc)	\
	(((pc) << 16) |				\
	 (!!(imme) << 7) |			\
	 (!!(resume) << 6) |			\
	 ((event) & 0x3f))

#define spm_emerg(fmt, args...)		pr_info("[SPM] " fmt, ##args)
#define spm_alert(fmt, args...)		pr_info("[SPM] " fmt, ##args)
#define spm_crit(fmt, args...)		pr_info("[SPM] " fmt, ##args)
#define spm_err(fmt, args...)		pr_info("[SPM] " fmt, ##args)
#define spm_warn(fmt, args...)		pr_info("[SPM] " fmt, ##args)
#define spm_notice(fmt, args...)	pr_notice("[SPM] " fmt, ##args)
#define spm_info(fmt, args...)		pr_info("[SPM] " fmt, ##args)
/* pr_debug show nothing */
#define spm_debug(fmt, args...)		pr_info("[SPM] " fmt, ##args)

/* just use in suspend flow for important log due to console suspend */
#define spm_crit2(fmt, args...)		\
do {					\
	aee_sram_printk(fmt, ##args);	\
	spm_crit(fmt, ##args);		\
} while (0)

#define wfi_with_sync()					\
do {							\
	isb();						\
	/* add mb() before wfi */			\
	mb();						\
	__asm__ __volatile__("wfi" : : : "memory");	\
} while (0)

static inline void set_pwrctrl_pcm_flags(struct pwr_ctrl *pwrctrl, u32 flags)
{
	if (pwrctrl->pcm_flags_cust == 0)
		pwrctrl->pcm_flags = flags;
	else
		pwrctrl->pcm_flags = pwrctrl->pcm_flags_cust;
}

static inline void set_pwrctrl_pcm_flags1(struct pwr_ctrl *pwrctrl, u32 flags)
{
	if (pwrctrl->pcm_flags1_cust == 0)
		pwrctrl->pcm_flags1 = flags;
	else
		pwrctrl->pcm_flags1 = pwrctrl->pcm_flags1_cust;
}

#endif
