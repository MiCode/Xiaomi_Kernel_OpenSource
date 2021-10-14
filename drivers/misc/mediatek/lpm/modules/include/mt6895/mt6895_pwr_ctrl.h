/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MT6895_PWR_CTRL_H__
#define __MT6895_PWR_CTRL_H__

/* SPM_WAKEUP_MISC */
#define WAKE_MISC_GIC_WAKEUP              0x3FF  /* bit0 ~ bit9 */
#define WAKE_MISC_SRCLKEN_RC_ERR_INT	 (1U << 0)
#define WAKE_MISC_SPM_TIMEOUT_WAKEUP_0	 (1U << 1)
#define WAKE_MISC_SPM_TIMEOUT_WAKEUP_1	 (1U << 2)
#define WAKE_MISC_SPM_TIMEOUT_WAKEUP_2	 (1U << 3)
#define WAKE_MISC_DVFSRC_IRQ			 (1U << 4)
#define WAKE_MISC_TWAM_IRQ_B			 (1U << 5)
#define WAKE_MISC_SPM_ACK_CHK_WAKEUP_0   (1U << 6)
#define WAKE_MISC_SPM_ACK_CHK_WAKEUP_1	 (1U << 7)
#define WAKE_MISC_SPM_ACK_CHK_WAKEUP_2	 (1U << 8)
#define WAKE_MISC_SPM_ACK_CHK_WAKEUP_3	 (1U << 9)
#define WAKE_MISC_SPM_ACK_CHK_WAKEUP_ALL (1U << 10)
#define WAKE_MISC_VLP_BUS_TIMEOUT_IRQ	 (1U << 11)
#define WAKE_MISC_PCM_TIMER_EVENT		 (1U << 16)
#define WAKE_MISC_REG_CPU_WAKEUP		 (1U << 17)
#define WAKE_MISC_PMIC_EINT_OUT			 ((1U << 19) | (1U << 20))
#define WAKE_MISC_PMSR_IRQ_B_SET0		 (1U << 22)
#define WAKE_MISC_PMSR_IRQ_B_SET1		 (1U << 23)
#define WAKE_MISC_PMSR_IRQ_B_SET2		 (1U << 24)
#define WAKE_MISC_PMIC_IRQ_ACK			 (1U << 30)
#define WAKE_MISC_PMIC_SCP_IRQ			 (1U << 31)

struct pwr_ctrl {
	/* for SPM */
	u32 pcm_flags;
	u32 pcm_flags_cust;	/* can override pcm_flags */
	u32 pcm_flags_cust_set;	/* set bit of pcm_flags, after pcm_flags_cust */
	u32 pcm_flags_cust_clr;	/* clr bit of pcm_flags, after pcm_flags_cust */
	u32 pcm_flags1;
	u32 pcm_flags1_cust;	/* can override pcm_flags1 */
	u32 pcm_flags1_cust_set;	/* set bit of pcm_flags1, after pcm_flags1_cust */
	u32 pcm_flags1_cust_clr;	/* clr bit of pcm_flags1, after pcm_flags1_cust */
	u32 timer_val;		/* @ 1T 32K */
	u32 timer_val_cust;	/* @ 1T 32K, can override timer_val */
	u32 timer_val_ramp_en;		/* stress for dpidle */
	u32 timer_val_ramp_en_sec;	/* stress for suspend */
	u32 wake_src;
	u32 wake_src_cust;	/* can override wake_src */
	u8 wdt_disable;		/* disable wdt in suspend */

	/* Auto-gen Start */

	/* SPM_CLK_CON */
	u8 reg_spm_lock_infra_dcm_lsb;
	u8 reg_cxo32k_remove_en_lsb;
	u8 reg_spm_leave_suspend_merge_mask_lsb;
	u8 reg_sysclk0_src_mask_b_lsb;
	u8 reg_sysclk1_src_mask_b_lsb;
	u8 reg_sysclk2_src_mask_b_lsb;

	/* SPM_AP_STANDBY_CON */
	u8 reg_wfi_op;
	u8 reg_wfi_type;
	u8 reg_mp0_cputop_idle_mask;
	u8 reg_mp1_cputop_idle_mask;
	u8 reg_mcusys_idle_mask;
	u8 reg_csyspwrup_req_mask_lsb;
	u8 reg_wfi_af_sel;
	u8 reg_cpu_sleep_wfi;

	/* SPM_SRC_REQ */
	u8 reg_spm_adsp_mailbox_req;
	u8 reg_spm_apsrc_req;
	u8 reg_spm_ddren_req;
	u8 reg_spm_dvfs_req;
	u8 reg_spm_f26m_req;
	u8 reg_spm_infra_req;
	u8 reg_spm_scp_mailbox_req;
	u8 reg_spm_sspm_mailbox_req;
	u8 reg_spm_sw_mailbox_req;
	u8 reg_spm_vcore_req;
	u8 reg_spm_vrf18_req;
	u8 adsp_mailbox_state;
	u8 apsrc_state;
	u8 ddren_state;
	u8 dvfs_state;
	u8 f26m_state;
	u8 infra_state;
	u8 scp_mailbox_state;
	u8 sspm_mailbox_state;
	u8 sw_mailbox_state;
	u8 vcore_state;
	u8 vrf18_state;

	/* SPM_SRC_MASK_0 */
	u8 reg_afe_apsrc_req_mask_b;
	u8 reg_afe_ddren_req_mask_b;
	u8 reg_afe_infra_req_mask_b;
	u8 reg_afe_srcclkena_mask_b;
	u8 reg_afe_vrf18_req_mask_b;
	u8 reg_apu_apsrc_req_mask_b;
	u8 reg_apu_ddren_req_mask_b;
	u8 reg_apu_infra_req_mask_b;
	u8 reg_apu_srcclkena_mask_b;
	u8 reg_apu_vrf18_req_mask_b;
	u8 reg_audio_dsp_apsrc_req_mask_b;
	u8 reg_audio_dsp_ddren_req_mask_b;
	u8 reg_audio_dsp_infra_req_mask_b;
	u8 reg_audio_dsp_srcclkena_mask_b;
	u8 reg_audio_dsp_vrf18_req_mask_b;
	u8 reg_cam_apsrc_req_mask_b;
	u8 reg_cam_ddren_req_mask_b;
	u32 reg_ccif_apsrc_req_mask_b;

	/* SPM_SRC_MASK_1 */
	u32 reg_ccif_infra_req_mask_b;
	u32 reg_ccif_srcclkena_mask_b;
	u8 reg_cg_check_apsrc_req_mask_b;
	u8 reg_cg_check_ddren_req_mask_b;
	u8 reg_cg_check_srcclkena_mask_b;
	u8 reg_cg_check_vcore_req_mask_b;
	u8 reg_cg_check_vrf18_req_mask_b;
	u8 reg_conn_apsrc_req_mask_b;
	u8 reg_conn_ddren_req_mask_b;
	u8 reg_conn_infra_req_mask_b;

	/* SPM_SRC_MASK_2 */
	u8 reg_conn_srcclkena_mask_b;
	u8 reg_conn_srcclkenb_mask_b;
	u8 reg_conn_vcore_req_mask_b;
	u8 reg_conn_vrf18_req_mask_b;
	u8 reg_mcupm_apsrc_req_mask_b;
	u8 reg_mcupm_ddren_req_mask_b;
	u8 reg_mcupm_infra_req_mask_b;
	u8 reg_mcupm_srcclkena_mask_b;
	u8 reg_mcupm_vrf18_req_mask_b;
	u8 reg_disp0_apsrc_req_mask_b;
	u8 reg_disp0_ddren_req_mask_b;
	u8 reg_disp1_apsrc_req_mask_b;
	u8 reg_disp1_ddren_req_mask_b;
	u8 reg_dpm_apsrc_req_mask_b;
	u8 reg_dpm_infra_req_mask_b;
	u8 reg_dpm_vrf18_req_mask_b;
	u8 reg_dpmaif_apsrc_req_mask_b;
	u8 reg_dpmaif_ddren_req_mask_b;
	u8 reg_dpmaif_infra_req_mask_b;
	u8 reg_dpmaif_srcclkena_mask_b;
	u8 reg_dpmaif_vrf18_req_mask_b;
	u8 reg_dvfsrc_level_req_mask_b;
	u8 reg_gce_apsrc_req_mask_b;

	/* SPM_SRC_MASK_3 */
	u8 reg_gce_ddren_req_mask_b;
	u8 reg_gce_infra_req_mask_b;
	u8 reg_gce_vrf18_req_mask_b;
	u8 reg_gpueb_apsrc_req_mask_b;
	u8 reg_gpueb_ddren_req_mask_b;
	u8 reg_gpueb_infra_req_mask_b;
	u8 reg_gpueb_srcclkena_mask_b;
	u8 reg_gpueb_vrf18_req_mask_b;
	u8 reg_img_apsrc_req_mask_b;
	u8 reg_img_ddren_req_mask_b;
	u8 reg_infrasys_apsrc_req_mask_b;
	u8 reg_infrasys_ddren_req_mask_b;
	u8 reg_ipic_infra_req_mask_b;
	u8 reg_ipic_vrf18_req_mask_b;
	u8 reg_mcusys_apsrc_req_mask_b;
	u8 reg_mcusys_ddren_req_mask_b;
	u8 reg_md_apsrc_req_mask_b;
	u8 reg_md_ddren_req_mask_b;

	/* SPM_SRC_MASK_4 */
	u8 reg_md_infra_req_mask_b;
	u8 reg_md_srcclkena_mask_b;
	u8 reg_md_srcclkena1_mask_b;
	u8 reg_md_vcore_req_mask_b;
	u8 reg_md_vrf18_req_mask_b;
	u8 reg_mdp_apsrc_req_mask_b;
	u8 reg_mdp_ddren_req_mask_b;
	u8 reg_mm_proc_apsrc_req_mask_b;
	u8 reg_mm_proc_ddren_req_mask_b;
	u8 reg_mm_proc_infra_req_mask_b;
	u8 reg_mm_proc_srcclkena_mask_b;
	u8 reg_mm_proc_vrf18_req_mask_b;
	u8 reg_mmsys_apsrc_req_mask_b;
	u8 reg_mmsys_ddren_req_mask_b;
	u8 reg_mmsys_vrf18_req_mask_b;
	u8 reg_msdc1_apsrc_req_mask_b;
	u8 reg_msdc1_ddren_req_mask_b;
	u8 reg_msdc1_infra_req_mask_b;
	u8 reg_msdc1_srcclkena_mask_b;
	u8 reg_msdc1_vrf18_req_mask_b;
	u8 reg_msdc2_apsrc_req_mask_b;
	u8 reg_msdc2_ddren_req_mask_b;
	u8 reg_msdc2_infra_req_mask_b;
	u8 reg_msdc2_srcclkena_mask_b;
	u8 reg_msdc2_vrf18_req_mask_b;
	u8 reg_pcie0_apsrc_req_mask_b;
	u8 reg_pcie0_ddren_req_mask_b;
	u8 reg_pcie0_infra_req_mask_b;
	u8 reg_pcie0_srcclkena_mask_b;
	u8 reg_pcie0_vrf18_req_mask_b;
	u8 reg_pcie1_apsrc_req_mask_b;
	u8 reg_pcie1_ddren_req_mask_b;

	/* SPM_SRC_MASK_5 */
	u8 reg_pcie1_infra_req_mask_b;
	u8 reg_pcie1_srcclkena_mask_b;
	u8 reg_pcie1_vrf18_req_mask_b;
	u8 reg_scp_apsrc_req_mask_b;
	u8 reg_scp_ddren_req_mask_b;
	u8 reg_scp_infra_req_mask_b;
	u8 reg_scp_srcclkena_mask_b;
	u8 reg_scp_vrf18_req_mask_b;
	u8 reg_srcclkeni_infra_req_mask_b;
	u8 reg_srcclkeni_srcclkena_mask_b;
	u8 reg_sspm_apsrc_req_mask_b;
	u8 reg_sspm_ddren_req_mask_b;
	u8 reg_sspm_infra_req_mask_b;
	u8 reg_sspm_srcclkena_mask_b;
	u8 reg_sspm_vrf18_req_mask_b;
	u8 reg_ssusb0_apsrc_req_mask_b;
	u8 reg_ssusb0_ddren_req_mask_b;
	u8 reg_ssusb0_infra_req_mask_b;
	u8 reg_ssusb0_srcclkena_mask_b;
	u8 reg_ssusb0_vrf18_req_mask_b;
	u8 reg_ssusb1_apsrc_req_mask_b;
	u8 reg_ssusb1_ddren_req_mask_b;
	u8 reg_ssusb1_infra_req_mask_b;
	u8 reg_ssusb1_srcclkena_mask_b;
	u8 reg_ssusb1_vrf18_req_mask_b;
	u8 reg_ufs_apsrc_req_mask_b;
	u8 reg_ufs_ddren_req_mask_b;
	u8 reg_ufs_infra_req_mask_b;
	u8 reg_ufs_srcclkena_mask_b;
	u8 reg_ufs_vrf18_req_mask_b;

	/* SPM_SRC_MASK_6 */
	u8 reg_vdec_apsrc_req_mask_b;
	u8 reg_vdec_ddren_req_mask_b;
	u8 reg_venc_apsrc_req_mask_b;
	u8 reg_venc_ddren_req_mask_b;

	/* SPM_EVENT_CON_MISC */
	u8 reg_srcclken_fast_resp;
	u8 reg_csyspwrup_ack_mask;

	/* SPM_WAKEUP_EVENT_MASK */
	u32 reg_wakeup_event_mask;

	/* SPM_WAKEUP_EVENT_EXT_MASK */
	u32 reg_ext_wakeup_event_mask;

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
/* SPM_CLK_CON */
	PW_REG_SPM_LOCK_INFRA_DCM_LSB,
	PW_REG_MD1_C32RM_EN_LSB,
	PW_REG_SPM_LEAVE_SUSPEND_MERGE_MASK_LSB,
	PW_REG_SYSCLK0_SRC_MASK_B_LSB,
	PW_REG_SYSCLK1_SRC_MASK_B_LSB,
	PW_REG_SYSCLK2_SRC_MASK_B_LSB,

/* SPM_AP_STANDBY_CON */
	PW_REG_WFI_OP,
	PW_REG_WFI_TYPE,
	PW_REG_MP0_CPUTOP_IDLE_MASK,
	PW_REG_MP1_CPUTOP_IDLE_MASK,
	PW_REG_MCUSYS_IDLE_MASK,
	PW_REG_CSYSPWRUP_REQ_MASK,
	PW_REG_WFI_AF_SEL,
	PW_REG_CPU_SLEEP_WFI,

/* SPM_SRC_REQ */
	PW_REG_SPM_ADSP_MAILBOX_REQ,
	PW_REG_SPM_APSRC_REQ,
	PW_REG_SPM_DDREN_REQ,
	PW_REG_SPM_DVFS_REQ,
	PW_REG_SPM_F26M_REQ,
	PW_REG_SPM_INFRA_REQ,
	PW_REG_SPM_SCP_MAILBOX_REQ,
	PW_REG_SPM_SSPM_MAILBOX_REQ,
	PW_REG_SPM_SW_MAILBOX_REQ,
	PW_REG_SPM_VCORE_REQ,
	PW_REG_SPM_VRF18_REQ,
	PW_REG_ADSP_MAILBOX_STATE,
	PW_REG_APSRC_STATE,
	PW_REG_DDREN_STATE,
	PW_REG_DVFS_STATE,
	PW_REG_F26M_STATE,
	PW_REG_INFRA_STATE,
	PW_REG_SCP_MAILBOX_STATE,
	PW_REG_SSSM_MAILBOX_STATE,
	PW_REG_SW_MAILBOX_STATE,
	PW_REG_VCORE_STATE,
	PW_REG_VRF18_STATE,


/* SPM_SRC_MASK_0 */
	PW_REG_AFE_APSRC_REQ_MASK_B,
	PW_REG_AFE_DDREN_REQ_MASK_B,
	PW_REG_AFE_INFRA_REQ_MASK_B,
	PW_REG_AFE_SRCCLKENA_MASK_B,
	PW_REG_AFE_VRF18_REQ_MASK_B,
	PW_REG_APU_APSRC_REQ_MASK_B,
	PW_REG_APU_DDREN_REQ_MASK_B,
	PW_REG_APU_INFRA_REQ_MASK_B,
	PW_REG_APU_SRCCLKENA_MASK_B,
	PW_REG_APU_VRF18_REQ_MASK_B,
	PW_REG_AUDIO_DSP_APSRC_REQ_MASK_B,
	PW_REG_AUDIO_DSP_DDREN_REQ_MASK_B,
	PW_REG_AUDIO_DSP_INFRA_REQ_MASK_B,
	PW_REG_AUDIO_DSP_SRCCLKENA_MASK_B,
	PW_REG_AUDIO_DSP_VRF18_REQ_MASK_B,
	PW_REG_CAM_APSRC_REQ_MASK_B,
	PW_REG_CAM_DDREN_REQ_MASK_B,
	PW_REG_CCIF_EVENT_APSRC_REQ_MASK_B,

/* SPM_SRC_MASK_1 */
	PW_REG_CCIF_EVENT_INFRA_REQ_MASK_B,
	PW_REG_CCIF_EVENT_SRCCLKENA_MASK_B,

/* SPM_SRC_MASK_2 */
	PW_REG_CG_CHECK_APSRC_REQ_MASK_B,
	PW_REG_CG_CHECK_DDREN_REQ_MASK_B,
	PW_REG_CG_CHECK_SRCCLKENA_MASK_B,
	PW_REG_CG_CHECK_VCORE_REQ_MASK_B,
	PW_REG_CG_CHECK_VRF18_REQ_MASK_B,
	PW_REG_CONN_APSRC_REQ_MASK_B,
	PW_REG_CONN_DDREN_REQ_MASK_B,
	PW_REG_CONN_INFRA_REQ_MASK_B,
	PW_REG_CONN_SRCCLKENA_MASK_B,
	PW_REG_CONN_SRCCLKENB_MASK_B,
	PW_REG_CONN_VCORE_REQ_MASK_B,
	PW_REG_CONN_VRF18_REQ_MASK_B,
	PW_REG_MCUPM_APSRC_REQ_MASK_B,
	PW_REG_MCUPM_DDREN_REQ_MASK_B,
	PW_REG_MCUPM_INFRA_REQ_MASK_B,
	PW_REG_MCUPM_SRCCLKENA_REQ_MASK_B,
	PW_REG_MCUPM_VRF18_REQ_MASK_B,
	PW_REG_DISP0_APSRC_REQ_MASK_B,
	PW_REG_DISP0_DDREN_REQ_MASK_B,
	PW_REG_DISP1_APSRC_REQ_MASK_B,
	PW_REG_DISP1_DDREN_REQ_MASK_B,
	PW_REG_DPM_APSRC_REQ_MASK_B,
	PW_REG_DPM_INFRA_REQ_MASK_B,
	PW_REG_DPM_VRF18_REQ_MASK_B,
	PW_REG_DPMAIF_APSRC_REQ_MASK_B,
	PW_REG_DPMAIF_DDREN_REQ_MASK_B,
	PW_REG_DPMAIF_INFRA_REQ_MASK_B,
	PW_REG_DPMAIF_SRCCLKENA_MASK_B,
	PW_REG_DPMAIF_VRF18_REQ_MASK_B,
	PW_REG_DVFSRC_LEVEL_REQ_MASK_B,
	PW_REG_GCE_APSRC_REQ_MASK_B,
	PW_REG_GCE_DDREN_REQ_MASK_B,
	PW_REG_GCE_INFRA_REQ_MASK_B,
	PW_REG_GCE_VRF18_REQ_MASK_B,
	PW_REG_GPUEB_APSRC_REQ_MASK_B,

/* SPM_SRC_MASK_3 */
	PW_REG_GPUEB_DDREN_REQ_MASK_B,
	PW_REG_GPUEB_INFRA_REQ_MASK_B,
	PW_REG_GPUEB_SRCCLKENA_MASK_B,
	PW_REG_GPUEB_VRF18_REQ_MASK_B,
	PW_REG_IMG_APSRC_REQ_MASK_B,
	PW_REG_IMG_DDREN_REQ_MASK_B,
	PW_REG_INFRASYS_APSRC_REQ_MASK_B,
	PW_REG_INFRASYS_DDREN_REQ_MASK_B,
	PW_REG_IPIC_INFRA_REQ_MASK_B,
	PW_REG_IPIC_VRF18_REQ_MASK_B,

	PW_REG_MCUSYS_MERGE_APSRC_REQ_MASK_B,
	PW_REG_MCUSYS_MERGE_DDREN_REQ_MASK_B,
	PW_REG_MD_APSRC_REQ_MASK_B,

/* SPM_SRC_MASK_4 */
	PW_REG_MD_DDREN_REQ_MASK_B,
	PW_REG_MD_INFRA_REQ_MASK_B,
	PW_REG_MD_SRCCLKENA_MASK_B,
	PW_REG_MD_SRCCLKENA1_MASK_B,
	PW_REG_MD_VCORE_REQ_MASK_B,
	PW_REG_MD_VRF18_REQ_MASK_B,
	PW_REG_MDP_APSRC_REQ_MASK_B,
	PW_REG_MDP_DDREN_REQ_MASK_B,
	PW_REG_MM_PROC_APSRC_REQ_MASK_B,
	PW_REG_MM_PROC_DDREN_REQ_MASK_B,
	PW_REG_MM_PROC_INFRA_REQ_MASK_B,
	PW_REG_MM_PROC_SRCCLKENA_REQ_MASK_B,
	PW_REG_MM_PROC_VRF18_REQ_MASK_B,
	PW_REG_MMSYS_APSRC_REQ_MASK_B,
	PW_REG_MMSYS_DDREN_REQ_MASK_B,
	PW_REG_MMSYS_VRF18_REQ_MASK_B,
	PW_REG_MSDC1_APSRC_REQ_MASK_B,
	PW_REG_MSDC1_DDREN_REQ_MASK_B,
	PW_REG_MSDC1_INFRA_REQ_MASK_B,
	PW_REG_MSDC1_SRCCLKENA_MASK_B,
	PW_REG_MSDC1_VRF18_REQ_MASK_B,
	PW_REG_MSDC2_APSRC_REQ_MASK_B,
	PW_REG_MSDC2_DDREN_REQ_MASK_B,
	PW_REG_MSDC2_INFRA_REQ_MASK_B,
	PW_REG_MSDC2_SRCCLKENA_MASK_B,
	PW_REG_MSDC2_VRF18_REQ_MASK_B,
	PW_REG_PCIE0_APSRC_REQ_MASK_B,
	PW_REG_PCIE0_DDREN_REQ_MASK_B,
	PW_REG_PCIE0_INFRA_REQ_MASK_B,
	PW_REG_PCIE0_SRCCLKENA_MASK_B,
	PW_REG_PCIE_VRF18_REQ_MASK_B,
	PW_REG_PCIE1_APSRC_REQ_MASK_B,
	PW_REG_PCIE1_DDREN_REQ_MASK_B,
	PW_REG_PCIE1_INFRA_REQ_MASK_B,
	PW_REG_PCIE1_SRCCLKENA_MASK_B,
	PW_REG_PCIE1_VRF18_REQ_MASK_B,

	PW_REG_SCP_APSRC_REQ_MASK_B,
	PW_REG_SCP_DDREN_REQ_MASK_B,
	PW_REG_SCP_INFRA_REQ_MASK_B,
	PW_REG_SCP_SRCCLKENA_MASK_B,

/* SPM_SRC_MASK_5 */
	PW_REG_SCP_VRF18_REQ_MASK_B,
	PW_REG_SRCCLKENI_INFRA_REQ_MASK_B,
	PW_REG_SRCCLKENI_SRCCLKENA_MASK_B,
	PW_REG_SSPM_APSRC_REQ_MASK_B,
	PW_REG_SSPM_DDREN_REQ_MASK_B,
	PW_REG_SSPM_INFRA_REQ_MASK_B,
	PW_REG_SSPM_SRCCLKENA_MASK_B,
	PW_REG_SSPM_VRF18_REQ_MASK_B,
	PW_REG_SSUSB0_APSRC_REQ_MASK_B,
	PW_REG_SSUSB0_DDREN_REQ_MASK_B,
	PW_REG_SSUSB0_INFRA_REQ_MASK_B,
	PW_REG_SSUSB0_SRCCLKENA_MASK_B,
	PW_REG_SSUSB0_VRF18_REQ_MASK_B,
	PW_REG_SSUSB1_APSRC_REQ_MASK_B,
	PW_REG_SSUSB1_DDREN_REQ_MASK_B,
	PW_REG_SSUSB1_INFRA_REQ_MASK_B,
	PW_REG_SSUSB1_SRCCLKENA_MASK_B,
	PW_REG_SSUSB1_VRF18_REQ_MASK_B,
	PW_REG_UFS_APSRC_REQ_MASK_B,
	PW_REG_UFS_DDREN_REQ_MASK_B,
	PW_REG_UFS_INFRA_REQ_MASK_B,
	PW_REG_UFS_SRCCLKENA_MASK_B,
	PW_REG_UFS_VRF18_REQ_MASK_B,

	PW_REG_VDEC_APSRC_REQ_MASK_B,
	PW_REG_VDEC_DDREN_REQ_MASK_B,
	PW_REG_VENC_APSRC_REQ_MASK_B,
	PW_REG_VENC_DDREN_REQ_MASK_B,
/* SPM_EVENT_CON_MISC */
	PW_REG_SRCCLKEN_FAST_RESP,
	PW_REG_CSYSPWRUP_ACK_MASK,

/* SPM_WAKEUP_EVENT_MASK */
	PW_REG_WAKEUP_EVENT_MASK,
/* SPM_WAKEUP_EVENT_EXT_MASK */
	PW_REG_EXT_WAKEUP_EVENT_MASK,


	PW_MAX_COUNT,
};


#endif /* __PWR_CTRL_H__ */
