/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef __PWR_CTRL_H__
#define __PWR_CTRL_H__

/* SPM_WAKEUP_MISC */
#define WAKE_MISC_TWAM		(1U << 18)
#define WAKE_MISC_PCM_TIMER	(1U << 19)
#define WAKE_MISC_CPU_WAKE	(1U << 20)

struct pwr_ctrl {

	/* for SPM */
	u32 pcm_flags;
	u32 pcm_flags_cust;
	u32 pcm_flags_cust_set;
	u32 pcm_flags_cust_clr;
	u32 pcm_flags1;
	u32 pcm_flags1_cust;
	u32 pcm_flags1_cust_set;
	u32 pcm_flags1_cust_clr;
	u32 timer_val;
	u32 timer_val_cust;
	u32 timer_val_ramp_en;
	u32 timer_val_ramp_en_sec;
	u32 wake_src;
	u32 wake_src_cust;
	u32 wakelock_timer_val;
	u8 wdt_disable;
	/* Auto-gen Start */

	/* SPM_AP_STANDBY_CON */
	u8 wfi_op;
	u8 wfi_type;
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
	u8 conn_srcclkena_sel_mask;

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
	u8 mcu_apsrcreq_infra_mask_b;

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
	u8 mcu_apsrc_req_mask_b;
	u8 mcu_ddren_mask_b;

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
	u8 mcu_ddren_2_mask_b;

	/* MP0_CPU0_WFI_EN */
	u8 mp0_cpu0_wfi_en;

	/* MP0_CPU1_WFI_EN */
	u8 mp0_cpu1_wfi_en;

	/* MP0_CPU2_WFI_EN */
	u8 mp0_cpu2_wfi_en;

	/* MP0_CPU3_WFI_EN */
	u8 mp0_cpu3_wfi_en;

	/* MP0_CPU4_WFI_EN */
	u8 mp0_cpu4_wfi_en;

	/* MP0_CPU5_WFI_EN */
	u8 mp0_cpu5_wfi_en;

	/* MP0_CPU6_WFI_EN */
	u8 mp0_cpu6_wfi_en;

	/* MP0_CPU7_WFI_EN */
	u8 mp0_cpu7_wfi_en;

	/* Auto-gen End */
};

/* code gen by spm_pwr_ctrl_atf.pl, need struct pwr_ctrl */
enum pwr_ctrl_enum {
	PW_PCM_FLAGS,
	PW_PCM_FLAGS_CUST,
	PW_PCM_FLAGS_CUST_SET,
	PW_PCM_FLAGS_CUST_CLR,
	PW_PCM_FLAGS1,
	PW_PCM_FLAGS1_CUST,
	PW_PCM_FLAGS1_CUST_SET,
	PW_PCM_FLAGS1_CUST_CLR,
	PW_TIMER_VAL,
	PW_TIMER_VAL_CUST,
	PW_TIMER_VAL_RAMP_EN,
	PW_TIMER_VAL_RAMP_EN_SEC,
	PW_WAKE_SRC,
	PW_WAKE_SRC_CUST,
	PW_WAKELOCK_TIMER_VAL,
	PW_WDT_DISABLE,
	/* SPM_AP_STANDBY_CON */
	PW_WFI_OP,
	PW_WFI_TYPE,
	PW_MP0_CPUTOP_IDLE_MASK,
	PW_MP1_CPUTOP_IDLE_MASK,
	PW_MCUSYS_IDLE_MASK,
	PW_MM_MASK_B,
	PW_MD_DDR_EN_0_DBC_EN,
	PW_MD_DDR_EN_1_DBC_EN,
	PW_MD_MASK_B,
	PW_SSPM_MASK_B,
	PW_SCP_MASK_B,
	PW_SRCCLKENI_MASK_B,
	PW_MD_APSRC_1_SEL,
	PW_MD_APSRC_0_SEL,
	PW_CONN_DDR_EN_DBC_EN,
	PW_CONN_MASK_B,
	PW_CONN_APSRC_SEL,
	PW_CONN_SRCCLKENA_SEL_MASK,
	/* SPM_SRC_REQ */
	PW_SPM_APSRC_REQ,
	PW_SPM_F26M_REQ,
	PW_SPM_INFRA_REQ,
	PW_SPM_VRF18_REQ,
	PW_SPM_DDREN_REQ,
	PW_SPM_RSV_SRC_REQ,
	PW_SPM_DDREN_2_REQ,
	PW_CPU_MD_DVFS_SOP_FORCE_ON,
	/* SPM_SRC_MASK */
	PW_CSYSPWREQ_MASK,
	PW_CCIF0_MD_EVENT_MASK_B,
	PW_CCIF0_AP_EVENT_MASK_B,
	PW_CCIF1_MD_EVENT_MASK_B,
	PW_CCIF1_AP_EVENT_MASK_B,
	PW_CCIF2_MD_EVENT_MASK_B,
	PW_CCIF2_AP_EVENT_MASK_B,
	PW_CCIF3_MD_EVENT_MASK_B,
	PW_CCIF3_AP_EVENT_MASK_B,
	PW_MD_SRCCLKENA_0_INFRA_MASK_B,
	PW_MD_SRCCLKENA_1_INFRA_MASK_B,
	PW_CONN_SRCCLKENA_INFRA_MASK_B,
	PW_UFS_INFRA_REQ_MASK_B,
	PW_SRCCLKENI_INFRA_MASK_B,
	PW_MD_APSRC_REQ_0_INFRA_MASK_B,
	PW_MD_APSRC_REQ_1_INFRA_MASK_B,
	PW_CONN_APSRCREQ_INFRA_MASK_B,
	PW_UFS_SRCCLKENA_MASK_B,
	PW_MD_VRF18_REQ_0_MASK_B,
	PW_MD_VRF18_REQ_1_MASK_B,
	PW_UFS_VRF18_REQ_MASK_B,
	PW_GCE_VRF18_REQ_MASK_B,
	PW_CONN_INFRA_REQ_MASK_B,
	PW_GCE_APSRC_REQ_MASK_B,
	PW_DISP0_APSRC_REQ_MASK_B,
	PW_DISP1_APSRC_REQ_MASK_B,
	PW_MFG_REQ_MASK_B,
	PW_VDEC_REQ_MASK_B,
	PW_MCU_APSRCREQ_INFRA_MASK_B,
	/* SPM_SRC2_MASK */
	PW_MD_DDR_EN_0_MASK_B,
	PW_MD_DDR_EN_1_MASK_B,
	PW_CONN_DDR_EN_MASK_B,
	PW_DDREN_SSPM_APSRC_REQ_MASK_B,
	PW_DDREN_SCP_APSRC_REQ_MASK_B,
	PW_DISP0_DDREN_MASK_B,
	PW_DISP1_DDREN_MASK_B,
	PW_GCE_DDREN_MASK_B,
	PW_DDREN_EMI_SELF_REFRESH_CH0_MASK_B,
	PW_DDREN_EMI_SELF_REFRESH_CH1_MASK_B,
	PW_MCU_APSRC_REQ_MASK_B,
	PW_MCU_DDREN_MASK_B,
	/* SPM_WAKEUP_EVENT_MASK */
	PW_SPM_WAKEUP_EVENT_MASK,
	/* SPM_WAKEUP_EVENT_EXT_MASK */
	PW_SPM_WAKEUP_EVENT_EXT_MASK,
	/* SPM_SRC3_MASK */
	PW_MD_DDR_EN_2_0_MASK_B,
	PW_MD_DDR_EN_2_1_MASK_B,
	PW_CONN_DDR_EN_2_MASK_B,
	PW_DDREN2_SSPM_APSRC_REQ_MASK_B,
	PW_DDREN2_SCP_APSRC_REQ_MASK_B,
	PW_DISP0_DDREN2_MASK_B,
	PW_DISP1_DDREN2_MASK_B,
	PW_GCE_DDREN2_MASK_B,
	PW_DDREN2_EMI_SELF_REFRESH_CH0_MASK_B,
	PW_DDREN2_EMI_SELF_REFRESH_CH1_MASK_B,
	PW_MCU_DDREN_2_MASK_B,
	/* MP0_CPU0_WFI_EN */
	PW_MP0_CPU0_WFI_EN,
	/* MP0_CPU1_WFI_EN */
	PW_MP0_CPU1_WFI_EN,
	/* MP0_CPU2_WFI_EN */
	PW_MP0_CPU2_WFI_EN,
	/* MP0_CPU3_WFI_EN */
	PW_MP0_CPU3_WFI_EN,
	/* MP0_CPU4_WFI_EN */
	PW_MP0_CPU4_WFI_EN,
	/* MP0_CPU5_WFI_EN */
	PW_MP0_CPU5_WFI_EN,
	/* MP0_CPU6_WFI_EN */
	PW_MP0_CPU6_WFI_EN,
	/* MP0_CPU7_WFI_EN */
	PW_MP0_CPU7_WFI_EN,

	PW_MAX_COUNT,
};

#endif /* __PWR_CTRL_H__ */
