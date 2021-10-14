/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MT6893_PWR_CTRL_H__
#define __MT6893_PWR_CTRL_H__

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
	uint32_t pcm_flags;
	uint32_t pcm_flags_cust;
	uint32_t pcm_flags_cust_set;
	uint32_t pcm_flags_cust_clr;
	uint32_t pcm_flags1;
	uint32_t pcm_flags1_cust;
	uint32_t pcm_flags1_cust_set;
	uint32_t pcm_flags1_cust_clr;
	uint32_t timer_val;
	uint32_t timer_val_cust;
	uint32_t timer_val_ramp_en;
	uint32_t timer_val_ramp_en_sec;
	uint32_t wake_src;
	uint32_t wake_src_cust;
	uint32_t wakelock_timer_val;
	uint8_t wdt_disable;
	/* Auto-gen Start */

	/* SPM_CLK_CON */
	uint8_t reg_srcclken0_ctl;
	uint8_t reg_srcclken1_ctl;
	uint8_t reg_spm_lock_infra_dcm;
	uint8_t reg_srcclken_mask;
	uint8_t reg_md1_c32rm_en;
	uint8_t reg_md2_c32rm_en;
	uint8_t reg_clksq0_sel_ctrl;
	uint8_t reg_clksq1_sel_ctrl;
	uint8_t reg_srcclken0_en;
	uint8_t reg_srcclken1_en;
	uint32_t reg_sysclk0_src_mask_b;
	uint32_t reg_sysclk1_src_mask_b;

	/* SPM_AP_STANDBY_CON */
	uint8_t reg_wfi_op;
	uint8_t reg_wfi_type;
	uint8_t reg_mp0_cputop_idle_mask;
	uint8_t reg_mp1_cputop_idle_mask;
	uint8_t reg_mcusys_idle_mask;
	uint8_t reg_md_apsrc_1_sel;
	uint8_t reg_md_apsrc_0_sel;
	uint8_t reg_conn_apsrc_sel;

	/* SPM_SRC_REQ */
	uint8_t reg_spm_apsrc_req;
	uint8_t reg_spm_f26m_req;
	uint8_t reg_spm_infra_req;
	uint8_t reg_spm_vrf18_req;
	uint8_t reg_spm_ddr_en_req;
	uint8_t reg_spm_dvfs_req;
	uint8_t reg_spm_sw_mailbox_req;
	uint8_t reg_spm_sspm_mailbox_req;
	uint8_t reg_spm_adsp_mailbox_req;
	uint8_t reg_spm_scp_mailbox_req;

	/* SPM_SRC_MASK */
	uint8_t reg_md_srcclkena_0_mask_b;
	uint8_t reg_md_srcclkena2infra_req_0_mask_b;
	uint8_t reg_md_apsrc2infra_req_0_mask_b;
	uint8_t reg_md_apsrc_req_0_mask_b;
	uint8_t reg_md_vrf18_req_0_mask_b;
	uint8_t reg_md_ddr_en_0_mask_b;
	uint8_t reg_md_srcclkena_1_mask_b;
	uint8_t reg_md_srcclkena2infra_req_1_mask_b;
	uint8_t reg_md_apsrc2infra_req_1_mask_b;
	uint8_t reg_md_apsrc_req_1_mask_b;
	uint8_t reg_md_vrf18_req_1_mask_b;
	uint8_t reg_md_ddr_en_1_mask_b;
	uint8_t reg_conn_srcclkena_mask_b;
	uint8_t reg_conn_srcclkenb_mask_b;
	uint8_t reg_conn_infra_req_mask_b;
	uint8_t reg_conn_apsrc_req_mask_b;
	uint8_t reg_conn_vrf18_req_mask_b;
	uint8_t reg_conn_ddr_en_mask_b;
	uint8_t reg_conn_vfe28_mask_b;
	uint8_t reg_srcclkeni0_srcclkena_mask_b;
	uint8_t reg_srcclkeni0_infra_req_mask_b;
	uint8_t reg_srcclkeni1_srcclkena_mask_b;
	uint8_t reg_srcclkeni1_infra_req_mask_b;
	uint8_t reg_srcclkeni2_srcclkena_mask_b;
	uint8_t reg_srcclkeni2_infra_req_mask_b;
	uint8_t reg_infrasys_apsrc_req_mask_b;
	uint8_t reg_infrasys_ddr_en_mask_b;
	uint8_t reg_md32_srcclkena_mask_b;
	uint8_t reg_md32_infra_req_mask_b;
	uint8_t reg_md32_apsrc_req_mask_b;
	uint8_t reg_md32_vrf18_req_mask_b;
	uint8_t reg_md32_ddr_en_mask_b;

	/* SPM_SRC2_MASK */
	uint8_t reg_scp_srcclkena_mask_b;
	uint8_t reg_scp_infra_req_mask_b;
	uint8_t reg_scp_apsrc_req_mask_b;
	uint8_t reg_scp_vrf18_req_mask_b;
	uint8_t reg_scp_ddr_en_mask_b;
	uint8_t reg_audio_dsp_srcclkena_mask_b;
	uint8_t reg_audio_dsp_infra_req_mask_b;
	uint8_t reg_audio_dsp_apsrc_req_mask_b;
	uint8_t reg_audio_dsp_vrf18_req_mask_b;
	uint8_t reg_audio_dsp_ddr_en_mask_b;
	uint8_t reg_ufs_srcclkena_mask_b;
	uint8_t reg_ufs_infra_req_mask_b;
	uint8_t reg_ufs_apsrc_req_mask_b;
	uint8_t reg_ufs_vrf18_req_mask_b;
	uint8_t reg_ufs_ddr_en_mask_b;
	uint8_t reg_disp0_apsrc_req_mask_b;
	uint8_t reg_disp0_ddr_en_mask_b;
	uint8_t reg_disp1_apsrc_req_mask_b;
	uint8_t reg_disp1_ddr_en_mask_b;
	uint8_t reg_gce_infra_req_mask_b;
	uint8_t reg_gce_apsrc_req_mask_b;
	uint8_t reg_gce_vrf18_req_mask_b;
	uint8_t reg_gce_ddr_en_mask_b;
	uint8_t reg_apu_srcclkena_mask_b;
	uint8_t reg_apu_infra_req_mask_b;
	uint8_t reg_apu_apsrc_req_mask_b;
	uint8_t reg_apu_vrf18_req_mask_b;
	uint8_t reg_apu_ddr_en_mask_b;
	uint8_t reg_cg_check_srcclkena_mask_b;
	uint8_t reg_cg_check_apsrc_req_mask_b;
	uint8_t reg_cg_check_vrf18_req_mask_b;
	uint8_t reg_cg_check_ddr_en_mask_b;

	/* SPM_SRC3_MASK */
	uint8_t reg_dvfsrc_event_trigger_mask_b;
	uint8_t reg_sw2spm_int0_mask_b;
	uint8_t reg_sw2spm_int1_mask_b;
	uint8_t reg_sw2spm_int2_mask_b;
	uint8_t reg_sw2spm_int3_mask_b;
	uint8_t reg_sc_adsp2spm_wakeup_mask_b;
	uint8_t reg_sc_sspm2spm_wakeup_mask_b;
	uint8_t reg_sc_scp2spm_wakeup_mask_b;
	uint8_t reg_csyspwrreq_mask;
	uint8_t reg_spm_srcclkena_reserved_mask_b;
	uint8_t reg_spm_infra_req_reserved_mask_b;
	uint8_t reg_spm_apsrc_req_reserved_mask_b;
	uint8_t reg_spm_vrf18_req_reserved_mask_b;
	uint8_t reg_spm_ddr_en_reserved_mask_b;
	uint8_t reg_mcupm_srcclkena_mask_b;
	uint8_t reg_mcupm_infra_req_mask_b;
	uint8_t reg_mcupm_apsrc_req_mask_b;
	uint8_t reg_mcupm_vrf18_req_mask_b;
	uint8_t reg_mcupm_ddr_en_mask_b;
	uint8_t reg_msdc0_srcclkena_mask_b;
	uint8_t reg_msdc0_infra_req_mask_b;
	uint8_t reg_msdc0_apsrc_req_mask_b;
	uint8_t reg_msdc0_vrf18_req_mask_b;
	uint8_t reg_msdc0_ddr_en_mask_b;
	uint8_t reg_msdc1_srcclkena_mask_b;
	uint8_t reg_msdc1_infra_req_mask_b;
	uint8_t reg_msdc1_apsrc_req_mask_b;
	uint8_t reg_msdc1_vrf18_req_mask_b;
	uint8_t reg_msdc1_ddr_en_mask_b;

	/* SPM_SRC4_MASK */
	uint32_t ccif_event_mask_b;
	uint8_t reg_bak_psri_srcclkena_mask_b;
	uint8_t reg_bak_psri_infra_req_mask_b;
	uint8_t reg_bak_psri_apsrc_req_mask_b;
	uint8_t reg_bak_psri_vrf18_req_mask_b;
	uint8_t reg_bak_psri_ddr_en_mask_b;
	uint8_t reg_dramc0_md32_infra_req_mask_b;
	uint8_t reg_dramc0_md32_vrf18_req_mask_b;
	uint8_t reg_dramc1_md32_infra_req_mask_b;
	uint8_t reg_dramc1_md32_vrf18_req_mask_b;
	uint8_t reg_conn_srcclkenb2pwrap_mask_b;
	uint8_t reg_dramc0_md32_wakeup_mask;
	uint8_t reg_dramc1_md32_wakeup_mask;

	/* SPM_SRC5_MASK */
	uint32_t reg_mcusys_merge_apsrc_req_mask_b;
	uint32_t reg_mcusys_merge_ddr_en_mask_b;

	/* SPM_WAKEUP_EVENT_MASK */
	uint32_t reg_wakeup_event_mask;

	/* SPM_WAKEUP_EVENT_EXT_MASK */
	uint32_t reg_ext_wakeup_event_mask;

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

	/* SPM_AP_STANDBY_CON */
	PW_REG_WFI_OP,
	PW_REG_WFI_TYPE,
	PW_REG_MP0_CPUTOP_IDLE_MASK,
	PW_REG_MP1_CPUTOP_IDLE_MASK,
	PW_REG_MCUSYS_IDLE_MASK,
	PW_REG_MD_APSRC_1_SEL,
	PW_REG_MD_APSRC_0_SEL,
	PW_REG_CONN_APSRC_SEL,

	/* SPM_SRC_REQ */
	PW_REG_SPM_APSRC_REQ,
	PW_REG_SPM_F26M_REQ,
	PW_REG_SPM_INFRA_REQ,
	PW_REG_SPM_VRF18_REQ,
	PW_REG_SPM_DDR_EN_REQ,
	PW_REG_SPM_DVFS_REQ,
	PW_REG_SPM_SW_MAILBOX_REQ,
	PW_REG_SPM_SSPM_MAILBOX_REQ,
	PW_REG_SPM_ADSP_MAILBOX_REQ,
	PW_REG_SPM_SCP_MAILBOX_REQ,

	/* SPM_SRC_MASK */
	PW_REG_MD_SRCCLKENA_0_MASK_B,
	PW_REG_MD_SRCCLKENA2INFRA_REQ_0_MASK_B,
	PW_REG_MD_APSRC2INFRA_REQ_0_MASK_B,
	PW_REG_MD_APSRC_REQ_0_MASK_B,
	PW_REG_MD_VRF18_REQ_0_MASK_B,
	PW_REG_MD_DDR_EN_0_MASK_B,
	PW_REG_MD_SRCCLKENA_1_MASK_B,
	PW_REG_MD_SRCCLKENA2INFRA_REQ_1_MASK_B,
	PW_REG_MD_APSRC2INFRA_REQ_1_MASK_B,
	PW_REG_MD_APSRC_REQ_1_MASK_B,
	PW_REG_MD_VRF18_REQ_1_MASK_B,
	PW_REG_MD_DDR_EN_1_MASK_B,
	PW_REG_CONN_SRCCLKENA_MASK_B,
	PW_REG_CONN_SRCCLKENB_MASK_B,
	PW_REG_CONN_INFRA_REQ_MASK_B,
	PW_REG_CONN_APSRC_REQ_MASK_B,
	PW_REG_CONN_VRF18_REQ_MASK_B,
	PW_REG_CONN_DDR_EN_MASK_B,
	PW_REG_CONN_VFE28_MASK_B,
	PW_REG_SRCCLKENI0_SRCCLKENA_MASK_B,
	PW_REG_SRCCLKENI0_INFRA_REQ_MASK_B,
	PW_REG_SRCCLKENI1_SRCCLKENA_MASK_B,
	PW_REG_SRCCLKENI1_INFRA_REQ_MASK_B,
	PW_REG_SRCCLKENI2_SRCCLKENA_MASK_B,
	PW_REG_SRCCLKENI2_INFRA_REQ_MASK_B,
	PW_REG_INFRASYS_APSRC_REQ_MASK_B,
	PW_REG_INFRASYS_DDR_EN_MASK_B,
	PW_REG_MD32_SRCCLKENA_MASK_B,
	PW_REG_MD32_INFRA_REQ_MASK_B,
	PW_REG_MD32_APSRC_REQ_MASK_B,
	PW_REG_MD32_VRF18_REQ_MASK_B,
	PW_REG_MD32_DDR_EN_MASK_B,

	/* SPM_SRC2_MASK */
	PW_REG_SCP_SRCCLKENA_MASK_B,
	PW_REG_SCP_INFRA_REQ_MASK_B,
	PW_REG_SCP_APSRC_REQ_MASK_B,
	PW_REG_SCP_VRF18_REQ_MASK_B,
	PW_REG_SCP_DDR_EN_MASK_B,
	PW_REG_AUDIO_DSP_SRCCLKENA_MASK_B,
	PW_REG_AUDIO_DSP_INFRA_REQ_MASK_B,
	PW_REG_AUDIO_DSP_APSRC_REQ_MASK_B,
	PW_REG_AUDIO_DSP_VRF18_REQ_MASK_B,
	PW_REG_AUDIO_DSP_DDR_EN_MASK_B,
	PW_REG_UFS_SRCCLKENA_MASK_B,
	PW_REG_UFS_INFRA_REQ_MASK_B,
	PW_REG_UFS_APSRC_REQ_MASK_B,
	PW_REG_UFS_VRF18_REQ_MASK_B,
	PW_REG_UFS_DDR_EN_MASK_B,
	PW_REG_DISP0_APSRC_REQ_MASK_B,
	PW_REG_DISP0_DDR_EN_MASK_B,
	PW_REG_DISP1_APSRC_REQ_MASK_B,
	PW_REG_DISP1_DDR_EN_MASK_B,
	PW_REG_GCE_INFRA_REQ_MASK_B,
	PW_REG_GCE_APSRC_REQ_MASK_B,
	PW_REG_GCE_VRF18_REQ_MASK_B,
	PW_REG_GCE_DDR_EN_MASK_B,
	PW_REG_APU_SRCCLKENA_MASK_B,
	PW_REG_APU_INFRA_REQ_MASK_B,
	PW_REG_APU_APSRC_REQ_MASK_B,
	PW_REG_APU_VRF18_REQ_MASK_B,
	PW_REG_APU_DDR_EN_MASK_B,
	PW_REG_CG_CHECK_SRCCLKENA_MASK_B,
	PW_REG_CG_CHECK_APSRC_REQ_MASK_B,
	PW_REG_CG_CHECK_VRF18_REQ_MASK_B,
	PW_REG_CG_CHECK_DDR_EN_MASK_B,

	/* SPM_SRC3_MASK */
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
	PW_REG_MCUPM_SRCCLKENA_MASK_B,
	PW_REG_MCUPM_INFRA_REQ_MASK_B,
	PW_REG_MCUPM_APSRC_REQ_MASK_B,
	PW_REG_MCUPM_VRF18_REQ_MASK_B,
	PW_REG_MCUPM_DDR_EN_MASK_B,
	PW_REG_MSDC0_SRCCLKENA_MASK_B,
	PW_REG_MSDC0_INFRA_REQ_MASK_B,
	PW_REG_MSDC0_APSRC_REQ_MASK_B,
	PW_REG_MSDC0_VRF18_REQ_MASK_B,
	PW_REG_MSDC0_DDR_EN_MASK_B,
	PW_REG_MSDC1_SRCCLKENA_MASK_B,
	PW_REG_MSDC1_INFRA_REQ_MASK_B,
	PW_REG_MSDC1_APSRC_REQ_MASK_B,
	PW_REG_MSDC1_VRF18_REQ_MASK_B,
	PW_REG_MSDC1_DDR_EN_MASK_B,

	/* SPM_SRC4_MASK */
	PW_CCIF_EVENT_MASK_B,
	PW_REG_BAK_PSRI_SRCCLKENA_MASK_B,
	PW_REG_BAK_PSRI_INFRA_REQ_MASK_B,
	PW_REG_BAK_PSRI_APSRC_REQ_MASK_B,
	PW_REG_BAK_PSRI_VRF18_REQ_MASK_B,
	PW_REG_BAK_PSRI_DDR_EN_MASK_B,
	PW_REG_DRAMC0_MD32_INFRA_REQ_MASK_B,
	PW_REG_DRAMC0_MD32_VRF18_REQ_MASK_B,
	PW_REG_DRAMC1_MD32_INFRA_REQ_MASK_B,
	PW_REG_DRAMC1_MD32_VRF18_REQ_MASK_B,
	PW_REG_CONN_SRCCLKENB2PWRAP_MASK_B,
	PW_REG_DRAMC0_MD32_WAKEUP_MASK,
	PW_REG_DRAMC1_MD32_WAKEUP_MASK,

	/* SPM_SRC5_MASK */
	PW_REG_MCUSYS_MERGE_APSRC_REQ_MASK_B,
	PW_REG_MCUSYS_MERGE_DDR_EN_MASK_B,

	/* SPM_WAKEUP_EVENT_MASK */
	PW_REG_WAKEUP_EVENT_MASK,

	/* SPM_WAKEUP_EVENT_EXT_MASK */
	PW_REG_EXT_WAKEUP_EVENT_MASK,

	PW_MAX_COUNT,
};


#endif /* __PWR_CTRL_H__ */
