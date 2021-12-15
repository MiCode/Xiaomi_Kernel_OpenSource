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
	uint8_t reg_spm_lock_infra_dcm_lsb;
	uint8_t reg_md1_c32rm_en_lsb;
	uint8_t reg_spm_leave_suspend_merge_mask_lsb;
	uint8_t reg_sysclk0_src_mask_b_lsb;
	uint8_t reg_sysclk1_src_mask_b_lsb;

	/* SPM_AP_STANDBY_CON */
	uint8_t reg_wfi_op;
	uint8_t reg_wfi_type;
	uint8_t reg_mp0_cputop_idle_mask;
	uint8_t reg_mp1_cputop_idle_mask;
	uint8_t reg_mcusys_idle_mask;
	uint8_t reg_wfi_af_sel;
	uint8_t reg_cpu_sleep_wfi;

	/* SPM_SRC_REQ */
	uint8_t reg_spm_adsp_mailbox_req;
	uint8_t reg_spm_apsrc_req;
	uint8_t reg_spm_ddren_req;
	uint8_t reg_spm_dvfs_req;
	uint8_t reg_spm_f26m_req;
	uint8_t reg_spm_infra_req;
	uint8_t reg_spm_scp_mailbox_req;
	uint8_t reg_spm_sspm_mailbox_req;
	uint8_t reg_spm_sw_mailbox_req;
	uint8_t reg_spm_vcore_req;
	uint8_t reg_spm_vrf18_req;

	/* SPM_SRC_MASK_0 */
	uint8_t reg_afe_apsrc_req_mask_b;
	uint8_t reg_afe_ddren_req_mask_b;
	uint8_t reg_afe_infra_req_mask_b;
	uint8_t reg_afe_srcclkena_mask_b;
	uint8_t reg_afe_vrf18_req_mask_b;
	uint8_t reg_apu_apsrc_req_mask_b;
	uint8_t reg_apu_ddren_req_mask_b;
	uint8_t reg_apu_infra_req_mask_b;
	uint8_t reg_apu_srcclkena_mask_b;
	uint8_t reg_apu_vrf18_req_mask_b;
	uint8_t reg_audio_dsp_apsrc_req_mask_b;
	uint8_t reg_audio_dsp_ddren_req_mask_b;
	uint8_t reg_audio_dsp_infra_req_mask_b;
	uint8_t reg_audio_dsp_srcclkena_mask_b;
	uint8_t reg_audio_dsp_vrf18_req_mask_b;
	uint32_t reg_ccif_event_apsrc_req_mask_b;

	/* SPM_SRC_MASK_1 */
	uint32_t reg_ccif_event_infra_req_mask_b;
	uint32_t reg_ccif_event_srcclkena_mask_b;

	/* SPM_SRC_MASK_2 */
	uint8_t reg_cg_check_apsrc_req_mask_b;
	uint8_t reg_cg_check_ddren_req_mask_b;
	uint8_t reg_cg_check_srcclkena_mask_b;
	uint8_t reg_cg_check_vcore_req_mask_b;
	uint8_t reg_cg_check_vrf18_req_mask_b;
	uint8_t reg_conn_apsrc_req_mask_b;
	uint8_t reg_conn_ddren_req_mask_b;
	uint8_t reg_conn_infra_req_mask_b;
	uint8_t reg_conn_srcclkena_mask_b;
	uint8_t reg_conn_srcclkenb_mask_b;
	uint8_t reg_conn_vrf18_req_mask_b;
	uint8_t reg_disp0_apsrc_req_mask_b;
	uint8_t reg_disp0_ddren_req_mask_b;
	uint8_t reg_disp1_apsrc_req_mask_b;
	uint8_t reg_disp1_ddren_req_mask_b;
	uint8_t reg_dpmaif_apsrc_req_mask_b;
	uint8_t reg_dpmaif_ddren_req_mask_b;
	uint8_t reg_dpmaif_infra_req_mask_b;
	uint8_t reg_dpmaif_srcclkena_mask_b;
	uint8_t reg_dpmaif_vrf18_req_mask_b;
	uint8_t reg_dramc_md32_apsrc_req_mask_b;
	uint8_t reg_dramc_md32_infra_req_mask_b;
	uint8_t reg_dramc_md32_vrf18_req_mask_b;
	uint8_t reg_dvfsrc_event_trigger_mask_b;
	uint8_t reg_gce_apsrc_req_mask_b;
	uint8_t reg_gce_ddren_req_mask_b;
	uint8_t reg_gce_infra_req_mask_b;
	uint8_t reg_gce_vrf18_req_mask_b;
	uint8_t reg_gpueb_apsrc_req_mask_b;

	/* SPM_SRC_MASK_3 */
	uint8_t reg_gpueb_ddren_req_mask_b;
	uint8_t reg_gpueb_idle_mask_b;
	uint8_t reg_gpueb_infra_req_mask_b;
	uint8_t reg_gpueb_srcclkena_mask_b;
	uint8_t reg_gpueb_vrf18_req_mask_b;
	uint8_t reg_infrasys_apsrc_req_mask_b;
	uint8_t reg_infrasys_ddren_req_mask_b;
	uint8_t reg_mcupm_apsrc_req_mask_b;
	uint8_t reg_mcupm_ddren_req_mask_b;
	uint8_t reg_mcupm_idle_mask_b;
	uint8_t reg_mcupm_infra_req_mask_b;
	uint8_t reg_mcupm_srcclkena_mask_b;
	uint8_t reg_mcupm_vrf18_req_mask_b;
	uint32_t reg_mcusys_merge_apsrc_req_mask_b;
	uint32_t reg_mcusys_merge_ddren_req_mask_b;
	uint8_t reg_md_apsrc_req_mask_b;

	/* SPM_SRC_MASK_4 */
	uint8_t reg_md_ddren_req_mask_b;
	uint8_t reg_md_infra_req_mask_b;
	uint8_t reg_md_srcclkena_mask_b;
	uint8_t reg_md_srcclkena1_mask_b;
	uint8_t reg_md_vrf18_req_mask_b;
	uint8_t reg_mmsys_apsrc_req_mask_b;
	uint8_t reg_mmsys_ddren_req_mask_b;
	uint8_t reg_mmsys_vrf18_req_mask_b;
	uint8_t reg_msdc0_apsrc_req_mask_b;
	uint8_t reg_msdc0_ddren_req_mask_b;
	uint8_t reg_msdc0_infra_req_mask_b;
	uint8_t reg_msdc0_srcclkena_mask_b;
	uint8_t reg_msdc0_vrf18_req_mask_b;
	uint8_t reg_msdc1_apsrc_req_mask_b;
	uint8_t reg_msdc1_ddren_req_mask_b;
	uint8_t reg_msdc1_infra_req_mask_b;
	uint8_t reg_msdc1_srcclkena_mask_b;
	uint8_t reg_msdc1_vrf18_req_mask_b;
	uint8_t reg_msdc2_apsrc_req_mask_b;
	uint8_t reg_msdc2_ddren_req_mask_b;
	uint8_t reg_msdc2_infra_req_mask_b;
	uint8_t reg_msdc2_srcclkena_mask_b;
	uint8_t reg_msdc2_vrf18_req_mask_b;
	uint8_t reg_pcie_apsrc_req_mask_b;
	uint8_t reg_pcie_ddren_req_mask_b;
	uint8_t reg_pcie_infra_req_mask_b;
	uint8_t reg_pcie_srcclkena_mask_b;
	uint8_t reg_pcie_vrf18_req_mask_b;
	uint8_t reg_scp_apsrc_req_mask_b;
	uint8_t reg_scp_ddren_req_mask_b;
	uint8_t reg_scp_infra_req_mask_b;
	uint8_t reg_scp_srcclkena_mask_b;

	/* SPM_SRC_MASK_5 */
	uint8_t reg_scp_vrf18_req_mask_b;
	uint8_t reg_spm_reserved_apsrc_req_mask_b;
	uint8_t reg_spm_reserved_ddren_req_mask_b;
	uint8_t reg_spm_reserved_infra_req_mask_b;
	uint8_t reg_spm_reserved_srcclkena_mask_b;
	uint8_t reg_spm_reserved_vrf18_req_mask_b;
	uint8_t reg_srcclkeni_infra_req_mask_b;
	uint8_t reg_srcclkeni_srcclkena_mask_b;
	uint8_t reg_sspm_apsrc_req_mask_b;
	uint8_t reg_sspm_ddren_req_mask_b;
	uint8_t reg_sspm_idle_mask_b;
	uint8_t reg_sspm_infra_req_mask_b;
	uint8_t reg_sspm_srcclkena_mask_b;
	uint8_t reg_sspm_vrf18_req_mask_b;
	uint8_t reg_ufs_apsrc_req_mask_b;
	uint8_t reg_ufs_ddren_req_mask_b;
	uint8_t reg_ufs_infra_req_mask_b;
	uint8_t reg_ufs_srcclkena_mask_b;
	uint8_t reg_ufs_vrf18_req_mask_b;

	/* SPM_EVENT_CON_MISC */
	uint8_t reg_srcclken_fast_resp;
	uint8_t reg_csyspwrup_ack_mask;

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
	PW_REG_SPM_LOCK_INFRA_DCM_LSB,
	PW_REG_MD1_C32RM_EN_LSB,
	PW_REG_SPM_LEAVE_SUSPEND_MERGE_MASK_LSB,
	PW_REG_SYSCLK0_SRC_MASK_B_LSB,
	PW_REG_SYSCLK1_SRC_MASK_B_LSB,

	/* SPM_AP_STANDBY_CON */
	PW_REG_WFI_OP,
	PW_REG_WFI_TYPE,
	PW_REG_MP0_CPUTOP_IDLE_MASK,
	PW_REG_MP1_CPUTOP_IDLE_MASK,
	PW_REG_MCUSYS_IDLE_MASK,
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
	PW_REG_CONN_VRF18_REQ_MASK_B,
	PW_REG_DISP0_APSRC_REQ_MASK_B,
	PW_REG_DISP0_DDREN_REQ_MASK_B,
	PW_REG_DISP1_APSRC_REQ_MASK_B,
	PW_REG_DISP1_DDREN_REQ_MASK_B,
	PW_REG_DPMAIF_APSRC_REQ_MASK_B,
	PW_REG_DPMAIF_DDREN_REQ_MASK_B,
	PW_REG_DPMAIF_INFRA_REQ_MASK_B,
	PW_REG_DPMAIF_SRCCLKENA_MASK_B,
	PW_REG_DPMAIF_VRF18_REQ_MASK_B,
	PW_REG_DRAMC_MD32_APSRC_REQ_MASK_B,
	PW_REG_DRAMC_MD32_INFRA_REQ_MASK_B,
	PW_REG_DRAMC_MD32_VRF18_REQ_MASK_B,
	PW_REG_DVFSRC_EVENT_TRIGGER_MASK_B,
	PW_REG_GCE_APSRC_REQ_MASK_B,
	PW_REG_GCE_DDREN_REQ_MASK_B,
	PW_REG_GCE_INFRA_REQ_MASK_B,
	PW_REG_GCE_VRF18_REQ_MASK_B,
	PW_REG_GPUEB_APSRC_REQ_MASK_B,

	/* SPM_SRC_MASK_3 */
	PW_REG_GPUEB_DDREN_REQ_MASK_B,
	PW_REG_GPUEB_IDLE_MASK_B,
	PW_REG_GPUEB_INFRA_REQ_MASK_B,
	PW_REG_GPUEB_SRCCLKENA_MASK_B,
	PW_REG_GPUEB_VRF18_REQ_MASK_B,
	PW_REG_INFRASYS_APSRC_REQ_MASK_B,
	PW_REG_INFRASYS_DDREN_REQ_MASK_B,
	PW_REG_MCUPM_APSRC_REQ_MASK_B,
	PW_REG_MCUPM_DDREN_REQ_MASK_B,
	PW_REG_MCUPM_IDLE_MASK_B,
	PW_REG_MCUPM_INFRA_REQ_MASK_B,
	PW_REG_MCUPM_SRCCLKENA_MASK_B,
	PW_REG_MCUPM_VRF18_REQ_MASK_B,
	PW_REG_MCUSYS_MERGE_APSRC_REQ_MASK_B,
	PW_REG_MCUSYS_MERGE_DDREN_REQ_MASK_B,
	PW_REG_MD_APSRC_REQ_MASK_B,

	/* SPM_SRC_MASK_4 */
	PW_REG_MD_DDREN_REQ_MASK_B,
	PW_REG_MD_INFRA_REQ_MASK_B,
	PW_REG_MD_SRCCLKENA_MASK_B,
	PW_REG_MD_SRCCLKENA1_MASK_B,
	PW_REG_MD_VRF18_REQ_MASK_B,
	PW_REG_MMSYS_APSRC_REQ_MASK_B,
	PW_REG_MMSYS_DDREN_REQ_MASK_B,
	PW_REG_MMSYS_VRF18_REQ_MASK_B,
	PW_REG_MSDC0_APSRC_REQ_MASK_B,
	PW_REG_MSDC0_DDREN_REQ_MASK_B,
	PW_REG_MSDC0_INFRA_REQ_MASK_B,
	PW_REG_MSDC0_SRCCLKENA_MASK_B,
	PW_REG_MSDC0_VRF18_REQ_MASK_B,
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
	PW_REG_PCIE_APSRC_REQ_MASK_B,
	PW_REG_PCIE_DDREN_REQ_MASK_B,
	PW_REG_PCIE_INFRA_REQ_MASK_B,
	PW_REG_PCIE_SRCCLKENA_MASK_B,
	PW_REG_PCIE_VRF18_REQ_MASK_B,
	PW_REG_SCP_APSRC_REQ_MASK_B,
	PW_REG_SCP_DDREN_REQ_MASK_B,
	PW_REG_SCP_INFRA_REQ_MASK_B,
	PW_REG_SCP_SRCCLKENA_MASK_B,

	/* SPM_SRC_MASK_5 */
	PW_REG_SCP_VRF18_REQ_MASK_B,
	PW_REG_SPM_RESERVED_APSRC_REQ_MASK_B,
	PW_REG_SPM_RESERVED_DDREN_REQ_MASK_B,
	PW_REG_SPM_RESERVED_INFRA_REQ_MASK_B,
	PW_REG_SPM_RESERVED_SRCCLKENA_MASK_B,
	PW_REG_SPM_RESERVED_VRF18_REQ_MASK_B,
	PW_REG_SRCCLKENI_INFRA_REQ_MASK_B,
	PW_REG_SRCCLKENI_SRCCLKENA_MASK_B,
	PW_REG_SSPM_APSRC_REQ_MASK_B,
	PW_REG_SSPM_DDREN_REQ_MASK_B,
	PW_REG_SSPM_IDLE_MASK_B,
	PW_REG_SSPM_INFRA_REQ_MASK_B,
	PW_REG_SSPM_SRCCLKENA_MASK_B,
	PW_REG_SSPM_VRF18_REQ_MASK_B,
	PW_REG_UFS_APSRC_REQ_MASK_B,
	PW_REG_UFS_DDREN_REQ_MASK_B,
	PW_REG_UFS_INFRA_REQ_MASK_B,
	PW_REG_UFS_SRCCLKENA_MASK_B,
	PW_REG_UFS_VRF18_REQ_MASK_B,

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
