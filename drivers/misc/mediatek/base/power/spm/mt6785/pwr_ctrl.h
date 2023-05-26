/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __PWR_CTRL_H__
#define __PWR_CTRL_H__

/* SPM_WAKEUP_MISC */
#define WAKE_MISC_GIC_WAKEUP              0x3FF  /* bit0 ~ bit9 */
#define WAKE_MISC_DVFSRC_IRQ	         (1U << 16)
#define WAKE_MISC_REG_CPU_WAKEUP         (1U << 17)
#define WAKE_MISC_PCM_TIMER_EVENT        (1U << 18)
#define WAKE_MISC_PMIC_OUT_B		     ((1U << 19) | (1U << 20))
#define WAKE_MISC_TWAM_IRQ_B             (1U << 21)
#define WAKE_MISC_PMSR_IRQ_B_SET0        (1U << 22)
#define WAKE_MISC_PMSR_IRQ_B_SET1        (1U << 23)
#define WAKE_MISC_PMSR_IRQ_B_SET2        (1U << 24)
#define WAKE_MISC_SPM_ACK_CHK_WAKEUP_0   (1U << 25)
#define WAKE_MISC_SPM_ACK_CHK_WAKEUP_1	 (1U << 26)
#define WAKE_MISC_SPM_ACK_CHK_WAKEUP_2	 (1U << 27)
#define WAKE_MISC_SPM_ACK_CHK_WAKEUP_3	 (1U << 28)
#define WAKE_MISC_SPM_ACK_CHK_WAKEUP_ALL (1U << 29)
#define WAKE_MISC_PMIC_IRQ_ACK			 (1U << 30)
#define WAKE_MISC_PMIC_SCP_IRQ			 (1U << 31)

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
	/* Auto-gen Start */

	/* SPM_CLK_CON */
	u8 reg_srcclken0_ctl;
	u8 reg_srcclken1_ctl;
	u8 reg_spm_lock_infra_dcm;
	u8 reg_srcclken_mask;
	u8 reg_md1_c32rm_en;
	u8 reg_md2_c32rm_en;
	u8 reg_clksq0_sel_ctrl;
	u8 reg_clksq1_sel_ctrl;
	u8 reg_srcclken0_en;
	u8 reg_srcclken1_en;
	u32 reg_sysclk0_src_mask_b;
	u32 reg_sysclk1_src_mask_b;

	/* SPM_AP_STANDBY_CON */
	u8 reg_wfi_op;
	u8 reg_wfi_type;
	u8 reg_mp0_cputop_idle_mask;
	u8 reg_mp1_cputop_idle_mask;
	u8 reg_mcusys_idle_mask;
	u8 reg_md_apsrc_1_sel;
	u8 reg_md_apsrc_0_sel;
	u8 reg_conn_apsrc_sel;

	/* SPM_SRC_REQ */
	u8 reg_spm_apsrc_req;
	u8 reg_spm_f26m_req;
	u8 reg_spm_infra_req;
	u8 reg_spm_vrf18_req;
	u8 reg_spm_ddr_en_req;
	u8 reg_spm_ddr_en2_req;
	u8 reg_spm_dvfs_req;
	u8 reg_spm_sw_mailbox_req;
	u8 reg_spm_sspm_mailbox_req;
	u8 reg_spm_adsp_mailbox_req;
	u8 reg_spm_scp_mailbox_req;
	u8 reg_spm_mcusys_pwr_event_req;
	u8 cpu_md_dvfs_sop_force_on;

	/* SPM_SRC_MASK */
	u8 reg_md_srcclkena_0_mask_b;
	u8 reg_md_srcclkena2infra_req_0_mask_b;
	u8 reg_md_apsrc2infra_req_0_mask_b;
	u8 reg_md_apsrc_req_0_mask_b;
	u8 reg_md_vrf18_req_0_mask_b;
	u8 reg_md_ddr_en_0_mask_b;
	u8 reg_md_ddr_en2_0_mask_b;
	u8 reg_md_srcclkena_1_mask_b;
	u8 reg_md_srcclkena2infra_req_1_mask_b;
	u8 reg_md_apsrc2infra_req_1_mask_b;
	u8 reg_md_apsrc_req_1_mask_b;
	u8 reg_md_vrf18_req_1_mask_b;
	u8 reg_md_ddr_en_1_mask_b;
	u8 reg_md_ddr_en2_1_mask_b;
	u8 reg_conn_srcclkena_mask_b;
	u8 reg_conn_srcclkenb_mask_b;
	u8 reg_conn_infra_req_mask_b;
	u8 reg_conn_apsrc_req_mask_b;
	u8 reg_conn_vrf18_req_mask_b;
	u8 reg_conn_ddr_en_mask_b;
	u8 reg_conn_ddr_en2_mask_b;
	u8 reg_srcclkeni0_srcclkena_mask_b;
	u8 reg_srcclkeni0_infra_req_mask_b;
	u8 reg_srcclkeni1_srcclkena_mask_b;
	u8 reg_srcclkeni1_infra_req_mask_b;
	u8 reg_srcclkeni2_srcclkena_mask_b;
	u8 reg_srcclkeni2_infra_req_mask_b;
	u8 reg_infrasys_apsrc_req_mask_b;
	u8 reg_infrasys_ddr_en_mask_b;
	u8 reg_infrasys_ddr_en2_mask_b;
	u8 reg_md32_srcclkena_mask_b;
	u8 reg_conn_vfe28_req_mask_b;

	/* SPM_SRC2_MASK */
	u8 reg_md32_infra_req_mask_b;
	u8 reg_md32_apsrc_req_mask_b;
	u8 reg_md32_vrf18_req_mask_b;
	u8 reg_md32_ddr_en_mask_b;
	u8 reg_md32_ddr_en2_mask_b;
	u8 reg_scp_srcclkena_mask_b;
	u8 reg_scp_infra_req_mask_b;
	u8 reg_scp_apsrc_req_mask_b;
	u8 reg_scp_vrf18_req_mask_b;
	u8 reg_scp_ddr_en_mask_b;
	u8 reg_scp_ddr_en2_mask_b;
	u8 reg_ufs_srcclkena_mask_b;
	u8 reg_ufs_infra_req_mask_b;
	u8 reg_ufs_apsrc_req_mask_b;
	u8 reg_ufs_vrf18_req_mask_b;
	u8 reg_ufs_ddr_en_mask_b;
	u8 reg_ufs_ddr_en2_mask_b;
	u8 reg_disp0_apsrc_req_mask_b;
	u8 reg_disp0_ddr_en_mask_b;
	u8 reg_disp0_ddr_en2_mask_b;
	u8 reg_disp1_apsrc_req_mask_b;
	u8 reg_disp1_ddr_en_mask_b;
	u8 reg_disp1_ddr_en2_mask_b;
	u8 reg_gce_infra_req_mask_b;
	u8 reg_gce_apsrc_req_mask_b;
	u8 reg_gce_vrf18_req_mask_b;
	u8 reg_gce_ddr_en_mask_b;
	u8 reg_gce_ddr_en2_mask_b;
	u8 reg_emi_ch0_ddr_en_mask_b;
	u8 reg_emi_ch1_ddr_en_mask_b;
	u8 reg_emi_ch0_ddr_en2_mask_b;
	u8 reg_emi_ch1_ddr_en2_mask_b;

	/* SPM_SRC3_MASK */
	u8 reg_dvfsrc_event_trigger_mask_b;
	u8 reg_sw2spm_int0_mask_b;
	u8 reg_sw2spm_int1_mask_b;
	u8 reg_sw2spm_int2_mask_b;
	u8 reg_sw2spm_int3_mask_b;
	u8 reg_sc_adsp2spm_wakeup_mask_b;
	u8 reg_sc_sspm2spm_wakeup_mask_b;
	u8 reg_sc_scp2spm_wakeup_mask_b;
	u8 reg_csyspwrreq_mask;
	u8 reg_spm_srcclkena_reserved_mask_b;
	u8 reg_spm_infra_req_reserved_mask_b;
	u8 reg_spm_apsrc_req_reserved_mask_b;
	u8 reg_spm_vrf18_req_reserved_mask_b;
	u8 reg_spm_ddr_en_reserved_mask_b;
	u8 reg_spm_ddr_en2_reserved_mask_b;
	u8 reg_audio_dsp_srcclkena_mask_b;
	u8 reg_audio_dsp_infra_req_mask_b;
	u8 reg_audio_dsp_apsrc_req_mask_b;
	u8 reg_audio_dsp_vrf18_req_mask_b;
	u8 reg_audio_dsp_ddr_en_mask_b;
	u8 reg_audio_dsp_ddr_en2_mask_b;
	u8 reg_mcusys_pwr_event_mask_b;
	u8 reg_msdc0_srcclkena_mask_b;
	u8 reg_msdc0_infra_req_mask_b;
	u8 reg_msdc0_apsrc_req_mask_b;
	u8 reg_msdc0_vrf18_req_mask_b;
	u8 reg_msdc0_ddr_en_mask_b;
	u8 reg_msdc0_ddr_en2_mask_b;
	u8 reg_conn_srcclkenb2pwrap_mask_b;

	/* SPM_SRC4_MASK */
	u32 ccif_event_mask_b;
	u8 reg_apu_core0_srcclkena_mask_b;
	u8 reg_apu_core0_infra_req_mask_b;
	u8 reg_apu_core0_apsrc_req_mask_b;
	u8 reg_apu_core0_vrf18_req_mask_b;
	u8 reg_apu_core0_ddr_en_mask_b;
	u8 reg_apu_core1_srcclkena_mask_b;
	u8 reg_apu_core1_infra_req_mask_b;
	u8 reg_apu_core1_apsrc_req_mask_b;
	u8 reg_apu_core1_vrf18_req_mask_b;
	u8 reg_apu_core1_ddr_en_mask_b;
	u8 reg_apu_core2_srcclkena_mask_b;
	u8 reg_apu_core2_infra_req_mask_b;
	u8 reg_apu_core2_apsrc_req_mask_b;
	u8 reg_apu_core2_vrf18_req_mask_b;
	u8 reg_apu_core2_ddr_en_mask_b;
	u8 reg_apu_core2_ddr_en2_mask_b;

	/* SPM_SRC5_MASK */
	u32 reg_mcusys_merge_apsrc_req_mask_b;
	u32 reg_mcusys_merge_ddr_en_mask_b;
	u32 reg_mcusys_merge_ddr_en2_mask_b;
	u8 reg_apu_core0_ddr_en2_mask_b;
	u8 reg_apu_core1_ddr_en2_mask_b;
	u8 reg_cg_check_ddr_en_mask_b;
	u8 reg_cg_check_ddr_en2_mask_b;

	/* SPM_WAKEUP_EVENT_MASK */
	u32 reg_wakeup_event_mask;

	/* SPM_WAKEUP_EVENT_EXT_MASK */
	u32 reg_ext_wakeup_event_mask;

	/* SPM_SRC6_MASK */
	u8 reg_msdc1_srcclkena_mask_b;
	u8 reg_msdc1_infra_req_mask_b;
	u8 reg_msdc1_apsrc_req_mask_b;
	u8 reg_msdc1_vrf18_req_mask_b;
	u8 reg_msdc1_ddr_en_mask_b;
	u8 reg_msdc1_ddr_en2_mask_b;
	u8 reg_msdc1_srcclkena_ack_mask;
	u8 reg_msdc1_infra_ack_mask;
	u8 reg_msdc1_apsrc_ack_mask;
	u8 reg_msdc1_vrf18_ack_mask;
	u8 reg_msdc1_ddr_en_ack_mask;
	u8 reg_msdc1_ddr_en2_ack_mask;

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
	PW_REG_SRCCLKEN0_CTL,
	PW_REG_SRCCLKEN1_CTL,
	PW_REG_SPM_LOCK_INFRA_DCM,
	PW_REG_SRCCLKEN_MASK,
	PW_REG_MD1_C32RM_EN,
	PW_REG_MD2_C32RM_EN,
	PW_REG_CLKSQ0_SEL_CTRL,
	PW_REG_CLKSQ1_SEL_CTRL,
	PW_REG_SRCCLKEN0_EN,
	PW_REG_SRCCLKEN1_EN,
	PW_REG_SYSCLK0_SRC_MASK_B,
	PW_REG_SYSCLK1_SRC_MASK_B,
	PW_REG_WFI_OP,
	PW_REG_WFI_TYPE,
	PW_REG_MP0_CPUTOP_IDLE_MASK,
	PW_REG_MP1_CPUTOP_IDLE_MASK,
	PW_REG_MCUSYS_IDLE_MASK,
	PW_REG_MD_APSRC_1_SEL,
	PW_REG_MD_APSRC_0_SEL,
	PW_REG_CONN_APSRC_SEL,
	PW_REG_SPM_APSRC_REQ,
	PW_REG_SPM_F26M_REQ,
	PW_REG_SPM_INFRA_REQ,
	PW_REG_SPM_VRF18_REQ,
	PW_REG_SPM_DDR_EN_REQ,
	PW_REG_SPM_DDR_EN2_REQ,
	PW_REG_SPM_DVFS_REQ,
	PW_REG_SPM_SW_MAILBOX_REQ,
	PW_REG_SPM_SSPM_MAILBOX_REQ,
	PW_REG_SPM_ADSP_MAILBOX_REQ,
	PW_REG_SPM_SCP_MAILBOX_REQ,
	PW_REG_SPM_MCUSYS_PWR_EVENT_REQ,
	PW_CPU_MD_DVFS_SOP_FORCE_ON,
	PW_REG_MD_SRCCLKENA_0_MASK_B,
	PW_REG_MD_SRCCLKENA2INFRA_REQ_0_MASK_B,
	PW_REG_MD_APSRC2INFRA_REQ_0_MASK_B,
	PW_REG_MD_APSRC_REQ_0_MASK_B,
	PW_REG_MD_VRF18_REQ_0_MASK_B,
	PW_REG_MD_DDR_EN_0_MASK_B,
	PW_REG_MD_DDR_EN2_0_MASK_B,
	PW_REG_MD_SRCCLKENA_1_MASK_B,
	PW_REG_MD_SRCCLKENA2INFRA_REQ_1_MASK_B,
	PW_REG_MD_APSRC2INFRA_REQ_1_MASK_B,
	PW_REG_MD_APSRC_REQ_1_MASK_B,
	PW_REG_MD_VRF18_REQ_1_MASK_B,
	PW_REG_MD_DDR_EN_1_MASK_B,
	PW_REG_MD_DDR_EN2_1_MASK_B,
	PW_REG_CONN_SRCCLKENA_MASK_B,
	PW_REG_CONN_SRCCLKENB_MASK_B,
	PW_REG_CONN_INFRA_REQ_MASK_B,
	PW_REG_CONN_APSRC_REQ_MASK_B,
	PW_REG_CONN_VRF18_REQ_MASK_B,
	PW_REG_CONN_DDR_EN_MASK_B,
	PW_REG_CONN_DDR_EN2_MASK_B,
	PW_REG_SRCCLKENI0_SRCCLKENA_MASK_B,
	PW_REG_SRCCLKENI0_INFRA_REQ_MASK_B,
	PW_REG_SRCCLKENI1_SRCCLKENA_MASK_B,
	PW_REG_SRCCLKENI1_INFRA_REQ_MASK_B,
	PW_REG_SRCCLKENI2_SRCCLKENA_MASK_B,
	PW_REG_SRCCLKENI2_INFRA_REQ_MASK_B,
	PW_REG_INFRASYS_APSRC_REQ_MASK_B,
	PW_REG_INFRASYS_DDR_EN_MASK_B,
	PW_REG_INFRASYS_DDR_EN2_MASK_B,
	PW_REG_MD32_SRCCLKENA_MASK_B,
	PW_REG_CONN_VFE28_REQ_MASK_B,
	PW_REG_MD32_INFRA_REQ_MASK_B,
	PW_REG_MD32_APSRC_REQ_MASK_B,
	PW_REG_MD32_VRF18_REQ_MASK_B,
	PW_REG_MD32_DDR_EN_MASK_B,
	PW_REG_MD32_DDR_EN2_MASK_B,
	PW_REG_SCP_SRCCLKENA_MASK_B,
	PW_REG_SCP_INFRA_REQ_MASK_B,
	PW_REG_SCP_APSRC_REQ_MASK_B,
	PW_REG_SCP_VRF18_REQ_MASK_B,
	PW_REG_SCP_DDR_EN_MASK_B,
	PW_REG_SCP_DDR_EN2_MASK_B,
	PW_REG_UFS_SRCCLKENA_MASK_B,
	PW_REG_UFS_INFRA_REQ_MASK_B,
	PW_REG_UFS_APSRC_REQ_MASK_B,
	PW_REG_UFS_VRF18_REQ_MASK_B,
	PW_REG_UFS_DDR_EN_MASK_B,
	PW_REG_UFS_DDR_EN2_MASK_B,
	PW_REG_DISP0_APSRC_REQ_MASK_B,
	PW_REG_DISP0_DDR_EN_MASK_B,
	PW_REG_DISP0_DDR_EN2_MASK_B,
	PW_REG_DISP1_APSRC_REQ_MASK_B,
	PW_REG_DISP1_DDR_EN_MASK_B,
	PW_REG_DISP1_DDR_EN2_MASK_B,
	PW_REG_GCE_INFRA_REQ_MASK_B,
	PW_REG_GCE_APSRC_REQ_MASK_B,
	PW_REG_GCE_VRF18_REQ_MASK_B,
	PW_REG_GCE_DDR_EN_MASK_B,
	PW_REG_GCE_DDR_EN2_MASK_B,
	PW_REG_EMI_CH0_DDR_EN_MASK_B,
	PW_REG_EMI_CH1_DDR_EN_MASK_B,
	PW_REG_EMI_CH0_DDR_EN2_MASK_B,
	PW_REG_EMI_CH1_DDR_EN2_MASK_B,
	PW_REG_DVFSRC_EVENT_TRIGGER_MASK_B,
	PW_REG_SW2SPM_INT0_MASK_B,
	PW_REG_SW2SPM_INT1_MASK_B,
	PW_REG_SW2SPM_INT2_MASK_B,
	PW_REG_SW2SPM_INT3_MASK_B,
	PW_REG_SC_ADSP2SPM_WAKEUP_MASK_B,
	PW_REG_SC_SSPM2SPM_WAKEUP_MASK_B,
	PW_REG_SC_SCP2SPM_WAKEUP_MASK_B,
	PW_REG_CSYSPWRREQ_MASK,
	PW_REG_SPM_SRCCLKENA_RESERVED_MASK_B,
	PW_REG_SPM_INFRA_REQ_RESERVED_MASK_B,
	PW_REG_SPM_APSRC_REQ_RESERVED_MASK_B,
	PW_REG_SPM_VRF18_REQ_RESERVED_MASK_B,
	PW_REG_SPM_DDR_EN_RESERVED_MASK_B,
	PW_REG_SPM_DDR_EN2_RESERVED_MASK_B,
	PW_REG_AUDIO_DSP_SRCCLKENA_MASK_B,
	PW_REG_AUDIO_DSP_INFRA_REQ_MASK_B,
	PW_REG_AUDIO_DSP_APSRC_REQ_MASK_B,
	PW_REG_AUDIO_DSP_VRF18_REQ_MASK_B,
	PW_REG_AUDIO_DSP_DDR_EN_MASK_B,
	PW_REG_AUDIO_DSP_DDR_EN2_MASK_B,
	PW_REG_MCUSYS_PWR_EVENT_MASK_B,
	PW_REG_MSDC0_SRCCLKENA_MASK_B,
	PW_REG_MSDC0_INFRA_REQ_MASK_B,
	PW_REG_MSDC0_APSRC_REQ_MASK_B,
	PW_REG_MSDC0_VRF18_REQ_MASK_B,
	PW_REG_MSDC0_DDR_EN_MASK_B,
	PW_REG_MSDC0_DDR_EN2_MASK_B,
	PW_REG_CONN_SRCCLKENB2PWRAP_MASK_B,
	PW_CCIF_EVENT_MASK_B,
	PW_REG_APU_CORE0_SRCCLKENA_MASK_B,
	PW_REG_APU_CORE0_INFRA_REQ_MASK_B,
	PW_REG_APU_CORE0_APSRC_REQ_MASK_B,
	PW_REG_APU_CORE0_VRF18_REQ_MASK_B,
	PW_REG_APU_CORE0_DDR_EN_MASK_B,
	PW_REG_APU_CORE1_SRCCLKENA_MASK_B,
	PW_REG_APU_CORE1_INFRA_REQ_MASK_B,
	PW_REG_APU_CORE1_APSRC_REQ_MASK_B,
	PW_REG_APU_CORE1_VRF18_REQ_MASK_B,
	PW_REG_APU_CORE1_DDR_EN_MASK_B,
	PW_REG_APU_CORE2_SRCCLKENA_MASK_B,
	PW_REG_APU_CORE2_INFRA_REQ_MASK_B,
	PW_REG_APU_CORE2_APSRC_REQ_MASK_B,
	PW_REG_APU_CORE2_VRF18_REQ_MASK_B,
	PW_REG_APU_CORE2_DDR_EN_MASK_B,
	PW_REG_APU_CORE2_DDR_EN2_MASK_B,
	PW_REG_MCUSYS_MERGE_APSRC_REQ_MASK_B,
	PW_REG_MCUSYS_MERGE_DDR_EN_MASK_B,
	PW_REG_MCUSYS_MERGE_DDR_EN2_MASK_B,
	PW_REG_APU_CORE0_DDR_EN2_MASK_B,
	PW_REG_APU_CORE1_DDR_EN2_MASK_B,
	PW_REG_CG_CHECK_DDR_EN_MASK_B,
	PW_REG_CG_CHECK_DDR_EN2_MASK_B,
	PW_REG_WAKEUP_EVENT_MASK,
	PW_REG_EXT_WAKEUP_EVENT_MASK,
	PW_REG_MSDC1_SRCCLKENA_MASK_B,
	PW_REG_MSDC1_INFRA_REQ_MASK_B,
	PW_REG_MSDC1_APSRC_REQ_MASK_B,
	PW_REG_MSDC1_VRF18_REQ_MASK_B,
	PW_REG_MSDC1_DDR_EN_MASK_B,
	PW_REG_MSDC1_DDR_EN2_MASK_B,
	PW_REG_MSDC1_SRCCLKENA_ACK_MASK,
	PW_REG_MSDC1_INFRA_ACK_MASK,
	PW_REG_MSDC1_APSRC_ACK_MASK,
	PW_REG_MSDC1_VRF18_ACK_MASK,
	PW_REG_MSDC1_DDR_EN_ACK_MASK,
	PW_REG_MSDC1_DDR_EN2_ACK_MASK,
	PW_MP0_CPU0_WFI_EN,
	PW_MP0_CPU1_WFI_EN,
	PW_MP0_CPU2_WFI_EN,
	PW_MP0_CPU3_WFI_EN,
	PW_MP0_CPU4_WFI_EN,
	PW_MP0_CPU5_WFI_EN,
	PW_MP0_CPU6_WFI_EN,
	PW_MP0_CPU7_WFI_EN,
	PW_MAX_COUNT,
};

#endif /* __PWR_CTRL_H__ */
