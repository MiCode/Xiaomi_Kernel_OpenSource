/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/string.h>

#include <mtk_spm_internal.h>
#include <mtk_spm_suspend_internal.h>
#include <mtk_spm_resource_req_internal.h>
#include <mtk_idle_fs/mtk_idle_sysfs.h>
#include <mtk_spm_resource_req_console.h>

/**************************************
 * Macro and Inline
 **************************************/
#define DEFINE_ATTR_RO(_name)			\
	static struct kobj_attribute _name##_attr = {	\
		.attr	= {				\
			.name = #_name,			\
			.mode = 0444,			\
		},					\
		.show	= _name##_show,			\
	}

#define DEFINE_ATTR_RW(_name)			\
	static struct kobj_attribute _name##_attr = {	\
		.attr	= {				\
			.name = #_name,			\
			.mode = 0644,			\
		},					\
		.show	= _name##_show,			\
		.store	= _name##_store,		\
	}

#define __ATTR_OF(_name)	(&_name##_attr.attr)

static char *pwr_ctrl_str[PW_MAX_COUNT] = {
	[PW_PCM_FLAGS] =
		"pcm_flags",
	[PW_PCM_FLAGS_CUST] =
		"pcm_flags_cust",
	[PW_PCM_FLAGS_CUST_SET] =
		"pcm_flags_cust_set",
	[PW_PCM_FLAGS_CUST_CLR] =
		"pcm_flags_cust_clr",
	[PW_PCM_FLAGS1] =
		"pcm_flags1",
	[PW_PCM_FLAGS1_CUST] =
		"pcm_flags1_cust",
	[PW_PCM_FLAGS1_CUST_SET] =
		"pcm_flags1_cust_set",
	[PW_PCM_FLAGS1_CUST_CLR] =
		"pcm_flags1_cust_clr",
	[PW_TIMER_VAL] =
		"timer_val",
	[PW_TIMER_VAL_CUST] =
		"timer_val_cust",
	[PW_TIMER_VAL_RAMP_EN] =
		"timer_val_ramp_en",
	[PW_TIMER_VAL_RAMP_EN_SEC] =
		"timer_val_ramp_en_sec",
	[PW_WAKE_SRC] =
		"wake_src",
	[PW_WAKE_SRC_CUST] =
		"wake_src_cust",
	[PW_WAKELOCK_TIMER_VAL] =
		"wakelock_timer_val",
	[PW_REG_SRCCLKEN0_CTL] =
		"reg_srcclken0_ctl",
	[PW_REG_SRCCLKEN1_CTL] =
		"reg_srcclken1_ctl",
	[PW_REG_SPM_LOCK_INFRA_DCM] =
		"reg_spm_lock_infra_dcm",
	[PW_REG_SRCCLKEN_MASK] =
		"reg_srcclken_mask",
	[PW_REG_MD1_C32RM_EN] =
		"reg_md1_c32rm_en",
	[PW_REG_MD2_C32RM_EN] =
		"reg_md2_c32rm_en",
	[PW_REG_CLKSQ0_SEL_CTRL] =
		"reg_clksq0_sel_ctrl",
	[PW_REG_CLKSQ1_SEL_CTRL] =
		"reg_clksq1_sel_ctrl",
	[PW_REG_SRCCLKEN0_EN] =
		"reg_srcclken0_en",
	[PW_REG_SRCCLKEN1_EN] =
		"reg_srcclken1_en",
	[PW_REG_SYSCLK0_SRC_MASK_B] =
		"reg_sysclk0_src_mask_b",
	[PW_REG_SYSCLK1_SRC_MASK_B] =
		"reg_sysclk1_src_mask_b",
	[PW_REG_WFI_OP] =
		"reg_wfi_op",
	[PW_REG_WFI_TYPE] =
		"reg_wfi_type",
	[PW_REG_MP0_CPUTOP_IDLE_MASK] =
		"reg_mp0_cputop_idle_mask",
	[PW_REG_MP1_CPUTOP_IDLE_MASK] =
		"reg_mp1_cputop_idle_mask",
	[PW_REG_MCUSYS_IDLE_MASK] =
		"reg_mcusys_idle_mask",
	[PW_REG_MD_APSRC_1_SEL] =
		"reg_md_apsrc_1_sel",
	[PW_REG_MD_APSRC_0_SEL] =
		"reg_md_apsrc_0_sel",
	[PW_REG_CONN_APSRC_SEL] =
		"reg_conn_apsrc_sel",
	[PW_REG_SPM_APSRC_REQ] =
		"reg_spm_apsrc_req",
	[PW_REG_SPM_F26M_REQ] =
		"reg_spm_f26m_req",
	[PW_REG_SPM_INFRA_REQ] =
		"reg_spm_infra_req",
	[PW_REG_SPM_VRF18_REQ] =
		"reg_spm_vrf18_req",
	[PW_REG_SPM_DDR_EN_REQ] =
		"reg_spm_ddr_en_req",
	[PW_REG_SPM_DDR_EN2_REQ] =
		"reg_spm_ddr_en2_req",
	[PW_REG_SPM_DVFS_REQ] =
		"reg_spm_dvfs_req",
	[PW_REG_SPM_SW_MAILBOX_REQ] =
		"reg_spm_sw_mailbox_req",
	[PW_REG_SPM_SSPM_MAILBOX_REQ] =
		"reg_spm_sspm_mailbox_req",
	[PW_REG_SPM_ADSP_MAILBOX_REQ] =
		"reg_spm_adsp_mailbox_req",
	[PW_REG_SPM_SCP_MAILBOX_REQ] =
		"reg_spm_scp_mailbox_req",
	[PW_REG_SPM_MCUSYS_PWR_EVENT_REQ] =
		"reg_spm_mcusys_pwr_event_req",
	[PW_CPU_MD_DVFS_SOP_FORCE_ON] =
		"cpu_md_dvfs_sop_force_on",
	[PW_REG_MD_SRCCLKENA_0_MASK_B] =
		"reg_md_srcclkena_0_mask_b",
	[PW_REG_MD_SRCCLKENA2INFRA_REQ_0_MASK_B] =
		"reg_md_srcclkena2infra_req_0_mask_b",
	[PW_REG_MD_APSRC2INFRA_REQ_0_MASK_B] =
		"reg_md_apsrc2infra_req_0_mask_b",
	[PW_REG_MD_APSRC_REQ_0_MASK_B] =
		"reg_md_apsrc_req_0_mask_b",
	[PW_REG_MD_VRF18_REQ_0_MASK_B] =
		"reg_md_vrf18_req_0_mask_b",
	[PW_REG_MD_DDR_EN_0_MASK_B] =
		"reg_md_ddr_en_0_mask_b",
	[PW_REG_MD_DDR_EN2_0_MASK_B] =
		"reg_md_ddr_en2_0_mask_b",
	[PW_REG_MD_SRCCLKENA_1_MASK_B] =
		"reg_md_srcclkena_1_mask_b",
	[PW_REG_MD_SRCCLKENA2INFRA_REQ_1_MASK_B] =
		"reg_md_srcclkena2infra_req_1_mask_b",
	[PW_REG_MD_APSRC2INFRA_REQ_1_MASK_B] =
		"reg_md_apsrc2infra_req_1_mask_b",
	[PW_REG_MD_APSRC_REQ_1_MASK_B] =
		"reg_md_apsrc_req_1_mask_b",
	[PW_REG_MD_VRF18_REQ_1_MASK_B] =
		"reg_md_vrf18_req_1_mask_b",
	[PW_REG_MD_DDR_EN_1_MASK_B] =
		"reg_md_ddr_en_1_mask_b",
	[PW_REG_MD_DDR_EN2_1_MASK_B] =
		"reg_md_ddr_en2_1_mask_b",
	[PW_REG_CONN_SRCCLKENA_MASK_B] =
		"reg_conn_srcclkena_mask_b",
	[PW_REG_CONN_SRCCLKENB_MASK_B] =
		"reg_conn_srcclkenb_mask_b",
	[PW_REG_CONN_INFRA_REQ_MASK_B] =
		"reg_conn_infra_req_mask_b",
	[PW_REG_CONN_APSRC_REQ_MASK_B] =
		"reg_conn_apsrc_req_mask_b",
	[PW_REG_CONN_VRF18_REQ_MASK_B] =
		"reg_conn_vrf18_req_mask_b",
	[PW_REG_CONN_DDR_EN_MASK_B] =
		"reg_conn_ddr_en_mask_b",
	[PW_REG_CONN_DDR_EN2_MASK_B] =
		"reg_conn_ddr_en2_mask_b",
	[PW_REG_SRCCLKENI0_SRCCLKENA_MASK_B] =
		"reg_srcclkeni0_srcclkena_mask_b",
	[PW_REG_SRCCLKENI0_INFRA_REQ_MASK_B] =
		"reg_srcclkeni0_infra_req_mask_b",
	[PW_REG_SRCCLKENI1_SRCCLKENA_MASK_B] =
		"reg_srcclkeni1_srcclkena_mask_b",
	[PW_REG_SRCCLKENI1_INFRA_REQ_MASK_B] =
		"reg_srcclkeni1_infra_req_mask_b",
	[PW_REG_SRCCLKENI2_SRCCLKENA_MASK_B] =
		"reg_srcclkeni2_srcclkena_mask_b",
	[PW_REG_SRCCLKENI2_INFRA_REQ_MASK_B] =
		"reg_srcclkeni2_infra_req_mask_b",
	[PW_REG_INFRASYS_APSRC_REQ_MASK_B] =
		"reg_infrasys_apsrc_req_mask_b",
	[PW_REG_INFRASYS_DDR_EN_MASK_B] =
		"reg_infrasys_ddr_en_mask_b",
	[PW_REG_INFRASYS_DDR_EN2_MASK_B] =
		"reg_infrasys_ddr_en2_mask_b",
	[PW_REG_MD32_SRCCLKENA_MASK_B] =
		"reg_md32_srcclkena_mask_b",
	[PW_REG_CONN_VFE28_REQ_MASK_B] =
		"reg_conn_vfe28_req_mask_b",
	[PW_REG_MD32_INFRA_REQ_MASK_B] =
		"reg_md32_infra_req_mask_b",
	[PW_REG_MD32_APSRC_REQ_MASK_B] =
		"reg_md32_apsrc_req_mask_b",
	[PW_REG_MD32_VRF18_REQ_MASK_B] =
		"reg_md32_vrf18_req_mask_b",
	[PW_REG_MD32_DDR_EN_MASK_B] =
		"reg_md32_ddr_en_mask_b",
	[PW_REG_MD32_DDR_EN2_MASK_B] =
		"reg_md32_ddr_en2_mask_b",
	[PW_REG_SCP_SRCCLKENA_MASK_B] =
		"reg_scp_srcclkena_mask_b",
	[PW_REG_SCP_INFRA_REQ_MASK_B] =
		"reg_scp_infra_req_mask_b",
	[PW_REG_SCP_APSRC_REQ_MASK_B] =
		"reg_scp_apsrc_req_mask_b",
	[PW_REG_SCP_VRF18_REQ_MASK_B] =
		"reg_scp_vrf18_req_mask_b",
	[PW_REG_SCP_DDR_EN_MASK_B] =
		"reg_scp_ddr_en_mask_b",
	[PW_REG_SCP_DDR_EN2_MASK_B] =
		"reg_scp_ddr_en2_mask_b",
	[PW_REG_UFS_SRCCLKENA_MASK_B] =
		"reg_ufs_srcclkena_mask_b",
	[PW_REG_UFS_INFRA_REQ_MASK_B] =
		"reg_ufs_infra_req_mask_b",
	[PW_REG_UFS_APSRC_REQ_MASK_B] =
		"reg_ufs_apsrc_req_mask_b",
	[PW_REG_UFS_VRF18_REQ_MASK_B] =
		"reg_ufs_vrf18_req_mask_b",
	[PW_REG_UFS_DDR_EN_MASK_B] =
		"reg_ufs_ddr_en_mask_b",
	[PW_REG_UFS_DDR_EN2_MASK_B] =
		"reg_ufs_ddr_en2_mask_b",
	[PW_REG_DISP0_APSRC_REQ_MASK_B] =
		"reg_disp0_apsrc_req_mask_b",
	[PW_REG_DISP0_DDR_EN_MASK_B] =
		"reg_disp0_ddr_en_mask_b",
	[PW_REG_DISP0_DDR_EN2_MASK_B] =
		"reg_disp0_ddr_en2_mask_b",
	[PW_REG_DISP1_APSRC_REQ_MASK_B] =
		"reg_disp1_apsrc_req_mask_b",
	[PW_REG_DISP1_DDR_EN_MASK_B] =
		"reg_disp1_ddr_en_mask_b",
	[PW_REG_DISP1_DDR_EN2_MASK_B] =
		"reg_disp1_ddr_en2_mask_b",
	[PW_REG_GCE_INFRA_REQ_MASK_B] =
		"reg_gce_infra_req_mask_b",
	[PW_REG_GCE_APSRC_REQ_MASK_B] =
		"reg_gce_apsrc_req_mask_b",
	[PW_REG_GCE_VRF18_REQ_MASK_B] =
		"reg_gce_vrf18_req_mask_b",
	[PW_REG_GCE_DDR_EN_MASK_B] =
		"reg_gce_ddr_en_mask_b",
	[PW_REG_GCE_DDR_EN2_MASK_B] =
		"reg_gce_ddr_en2_mask_b",
	[PW_REG_EMI_CH0_DDR_EN_MASK_B] =
		"reg_emi_ch0_ddr_en_mask_b",
	[PW_REG_EMI_CH1_DDR_EN_MASK_B] =
		"reg_emi_ch1_ddr_en_mask_b",
	[PW_REG_EMI_CH0_DDR_EN2_MASK_B] =
		"reg_emi_ch0_ddr_en2_mask_b",
	[PW_REG_EMI_CH1_DDR_EN2_MASK_B] =
		"reg_emi_ch1_ddr_en2_mask_b",
	[PW_REG_DVFSRC_EVENT_TRIGGER_MASK_B] =
		"reg_dvfsrc_event_trigger_mask_b",
	[PW_REG_SW2SPM_INT0_MASK_B] =
		"reg_sw2spm_int0_mask_b",
	[PW_REG_SW2SPM_INT1_MASK_B] =
		"reg_sw2spm_int1_mask_b",
	[PW_REG_SW2SPM_INT2_MASK_B] =
		"reg_sw2spm_int2_mask_b",
	[PW_REG_SW2SPM_INT3_MASK_B] =
		"reg_sw2spm_int3_mask_b",
	[PW_REG_SC_ADSP2SPM_WAKEUP_MASK_B] =
		"reg_sc_adsp2spm_wakeup_mask_b",
	[PW_REG_SC_SSPM2SPM_WAKEUP_MASK_B] =
		"reg_sc_sspm2spm_wakeup_mask_b",
	[PW_REG_SC_SCP2SPM_WAKEUP_MASK_B] =
		"reg_sc_scp2spm_wakeup_mask_b",
	[PW_REG_CSYSPWRREQ_MASK] =
		"reg_csyspwrreq_mask",
	[PW_REG_SPM_SRCCLKENA_RESERVED_MASK_B] =
		"reg_spm_srcclkena_reserved_mask_b",
	[PW_REG_SPM_INFRA_REQ_RESERVED_MASK_B] =
		"reg_spm_infra_req_reserved_mask_b",
	[PW_REG_SPM_APSRC_REQ_RESERVED_MASK_B] =
		"reg_spm_apsrc_req_reserved_mask_b",
	[PW_REG_SPM_VRF18_REQ_RESERVED_MASK_B] =
		"reg_spm_vrf18_req_reserved_mask_b",
	[PW_REG_SPM_DDR_EN_RESERVED_MASK_B] =
		"reg_spm_ddr_en_reserved_mask_b",
	[PW_REG_SPM_DDR_EN2_RESERVED_MASK_B] =
		"reg_spm_ddr_en2_reserved_mask_b",
	[PW_REG_AUDIO_DSP_SRCCLKENA_MASK_B] =
		"reg_audio_dsp_srcclkena_mask_b",
	[PW_REG_AUDIO_DSP_INFRA_REQ_MASK_B] =
		"reg_audio_dsp_infra_req_mask_b",
	[PW_REG_AUDIO_DSP_APSRC_REQ_MASK_B] =
		"reg_audio_dsp_apsrc_req_mask_b",
	[PW_REG_AUDIO_DSP_VRF18_REQ_MASK_B] =
		"reg_audio_dsp_vrf18_req_mask_b",
	[PW_REG_AUDIO_DSP_DDR_EN_MASK_B] =
		"reg_audio_dsp_ddr_en_mask_b",
	[PW_REG_AUDIO_DSP_DDR_EN2_MASK_B] =
		"reg_audio_dsp_ddr_en2_mask_b",
	[PW_REG_MCUSYS_PWR_EVENT_MASK_B] =
		"reg_mcusys_pwr_event_mask_b",
	[PW_REG_MSDC0_SRCCLKENA_MASK_B] =
		"reg_msdc0_srcclkena_mask_b",
	[PW_REG_MSDC0_INFRA_REQ_MASK_B] =
		"reg_msdc0_infra_req_mask_b",
	[PW_REG_MSDC0_APSRC_REQ_MASK_B] =
		"reg_msdc0_apsrc_req_mask_b",
	[PW_REG_MSDC0_VRF18_REQ_MASK_B] =
		"reg_msdc0_vrf18_req_mask_b",
	[PW_REG_MSDC0_DDR_EN_MASK_B] =
		"reg_msdc0_ddr_en_mask_b",
	[PW_REG_MSDC0_DDR_EN2_MASK_B] =
		"reg_msdc0_ddr_en2_mask_b",
	[PW_REG_CONN_SRCCLKENB2PWRAP_MASK_B] =
		"reg_conn_srcclkenb2pwrap_mask_b",
	[PW_CCIF_EVENT_MASK_B] =
		"ccif_event_mask_b",
	[PW_REG_APU_CORE0_SRCCLKENA_MASK_B] =
		"reg_apu_core0_srcclkena_mask_b",
	[PW_REG_APU_CORE0_INFRA_REQ_MASK_B] =
		"reg_apu_core0_infra_req_mask_b",
	[PW_REG_APU_CORE0_APSRC_REQ_MASK_B] =
		"reg_apu_core0_apsrc_req_mask_b",
	[PW_REG_APU_CORE0_VRF18_REQ_MASK_B] =
		"reg_apu_core0_vrf18_req_mask_b",
	[PW_REG_APU_CORE0_DDR_EN_MASK_B] =
		"reg_apu_core0_ddr_en_mask_b",
	[PW_REG_APU_CORE1_SRCCLKENA_MASK_B] =
		"reg_apu_core1_srcclkena_mask_b",
	[PW_REG_APU_CORE1_INFRA_REQ_MASK_B] =
		"reg_apu_core1_infra_req_mask_b",
	[PW_REG_APU_CORE1_APSRC_REQ_MASK_B] =
		"reg_apu_core1_apsrc_req_mask_b",
	[PW_REG_APU_CORE1_VRF18_REQ_MASK_B] =
		"reg_apu_core1_vrf18_req_mask_b",
	[PW_REG_APU_CORE1_DDR_EN_MASK_B] =
		"reg_apu_core1_ddr_en_mask_b",
	[PW_REG_APU_CORE2_SRCCLKENA_MASK_B] =
		"reg_apu_core2_srcclkena_mask_b",
	[PW_REG_APU_CORE2_INFRA_REQ_MASK_B] =
		"reg_apu_core2_infra_req_mask_b",
	[PW_REG_APU_CORE2_APSRC_REQ_MASK_B] =
		"reg_apu_core2_apsrc_req_mask_b",
	[PW_REG_APU_CORE2_VRF18_REQ_MASK_B] =
		"reg_apu_core2_vrf18_req_mask_b",
	[PW_REG_APU_CORE2_DDR_EN_MASK_B] =
		"reg_apu_core2_ddr_en_mask_b",
	[PW_REG_APU_CORE2_DDR_EN2_MASK_B] =
		"reg_apu_core2_ddr_en2_mask_b",
	[PW_REG_MCUSYS_MERGE_APSRC_REQ_MASK_B] =
		"reg_mcusys_merge_apsrc_req_mask_b",
	[PW_REG_MCUSYS_MERGE_DDR_EN_MASK_B] =
		"reg_mcusys_merge_ddr_en_mask_b",
	[PW_REG_MCUSYS_MERGE_DDR_EN2_MASK_B] =
		"reg_mcusys_merge_ddr_en2_mask_b",
	[PW_REG_APU_CORE0_DDR_EN2_MASK_B] =
		"reg_apu_core0_ddr_en2_mask_b",
	[PW_REG_APU_CORE1_DDR_EN2_MASK_B] =
		"reg_apu_core1_ddr_en2_mask_b",
	[PW_REG_CG_CHECK_DDR_EN_MASK_B] =
		"reg_cg_check_ddr_en_mask_b",
	[PW_REG_CG_CHECK_DDR_EN2_MASK_B] =
		"reg_cg_check_ddr_en2_mask_b",
	[PW_REG_WAKEUP_EVENT_MASK] =
		"reg_wakeup_event_mask",
	[PW_REG_EXT_WAKEUP_EVENT_MASK] =
		"reg_ext_wakeup_event_mask",
	[PW_REG_MSDC1_SRCCLKENA_MASK_B] =
		"reg_msdc1_srcclkena_mask_b",
	[PW_REG_MSDC1_INFRA_REQ_MASK_B] =
		"reg_msdc1_infra_req_mask_b",
	[PW_REG_MSDC1_APSRC_REQ_MASK_B] =
		"reg_msdc1_apsrc_req_mask_b",
	[PW_REG_MSDC1_VRF18_REQ_MASK_B] =
		"reg_msdc1_vrf18_req_mask_b",
	[PW_REG_MSDC1_DDR_EN_MASK_B] =
		"reg_msdc1_ddr_en_mask_b",
	[PW_REG_MSDC1_DDR_EN2_MASK_B] =
		"reg_msdc1_ddr_en2_mask_b",
	[PW_REG_MSDC1_SRCCLKENA_ACK_MASK] =
		"reg_msdc1_srcclkena_ack_mask",
	[PW_REG_MSDC1_INFRA_ACK_MASK] =
		"reg_msdc1_infra_ack_mask",
	[PW_REG_MSDC1_APSRC_ACK_MASK] =
		"reg_msdc1_apsrc_ack_mask",
	[PW_REG_MSDC1_VRF18_ACK_MASK] =
		"reg_msdc1_vrf18_ack_mask",
	[PW_REG_MSDC1_DDR_EN_ACK_MASK] =
		"reg_msdc1_ddr_en_ack_mask",
	[PW_REG_MSDC1_DDR_EN2_ACK_MASK] =
		"reg_msdc1_ddr_en2_ack_mask",
	[PW_MP0_CPU0_WFI_EN] =
		"mp0_cpu0_wfi_en",
	[PW_MP0_CPU1_WFI_EN] =
		"mp0_cpu1_wfi_en",
	[PW_MP0_CPU2_WFI_EN] =
		"mp0_cpu2_wfi_en",
	[PW_MP0_CPU3_WFI_EN] =
		"mp0_cpu3_wfi_en",
	[PW_MP0_CPU4_WFI_EN] =
		"mp0_cpu4_wfi_en",
	[PW_MP0_CPU5_WFI_EN] =
		"mp0_cpu5_wfi_en",
	[PW_MP0_CPU6_WFI_EN] =
		"mp0_cpu6_wfi_en",
	[PW_MP0_CPU7_WFI_EN] =
		"mp0_cpu7_wfi_en",
};

/**************************************
 * xxx_ctrl_show Function
 **************************************/
/* code gen by spm_pwr_ctrl_atf.pl, need struct pwr_ctrl */
static ssize_t show_pwr_ctrl(int id,
			const struct pwr_ctrl *pwrctrl,
			char *buf)
{
	char *p = buf;

	p += sprintf(p, "pcm_flags = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_PCM_FLAGS, 0));
	p += sprintf(p, "pcm_flags_cust = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_PCM_FLAGS_CUST, 0));
	p += sprintf(p, "pcm_flags_cust_set = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_PCM_FLAGS_CUST_SET, 0));
	p += sprintf(p, "pcm_flags_cust_clr = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_PCM_FLAGS_CUST_CLR, 0));
	p += sprintf(p, "pcm_flags1 = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_PCM_FLAGS1, 0));
	p += sprintf(p, "pcm_flags1_cust = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_PCM_FLAGS1_CUST, 0));
	p += sprintf(p, "pcm_flags1_cust_set = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_PCM_FLAGS1_CUST_SET, 0));
	p += sprintf(p, "pcm_flags1_cust_clr = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_PCM_FLAGS1_CUST_CLR, 0));
	p += sprintf(p, "timer_val = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_TIMER_VAL, 0));
	p += sprintf(p, "timer_val_cust = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_TIMER_VAL_CUST, 0));
	p += sprintf(p, "timer_val_ramp_en = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_TIMER_VAL_RAMP_EN, 0));
	p += sprintf(p, "timer_val_ramp_en_sec = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_TIMER_VAL_RAMP_EN_SEC, 0));
	p += sprintf(p, "wake_src = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_WAKE_SRC, 0));
	p += sprintf(p, "wake_src_cust = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_WAKE_SRC_CUST, 0));
	p += sprintf(p, "wakelock_timer_val = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_WAKELOCK_TIMER_VAL, 0));
	p += sprintf(p, "reg_srcclken0_ctl = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SRCCLKEN0_CTL, 0));
	p += sprintf(p, "reg_srcclken1_ctl = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SRCCLKEN1_CTL, 0));
	p += sprintf(p, "reg_spm_lock_infra_dcm = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SPM_LOCK_INFRA_DCM, 0));
	p += sprintf(p, "reg_srcclken_mask = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SRCCLKEN_MASK, 0));
	p += sprintf(p, "reg_md1_c32rm_en = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MD1_C32RM_EN, 0));
	p += sprintf(p, "reg_md2_c32rm_en = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MD2_C32RM_EN, 0));
	p += sprintf(p, "reg_clksq0_sel_ctrl = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_CLKSQ0_SEL_CTRL, 0));
	p += sprintf(p, "reg_clksq1_sel_ctrl = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_CLKSQ1_SEL_CTRL, 0));
	p += sprintf(p, "reg_srcclken0_en = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SRCCLKEN0_EN, 0));
	p += sprintf(p, "reg_srcclken1_en = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SRCCLKEN1_EN, 0));
	p += sprintf(p, "reg_sysclk0_src_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SYSCLK0_SRC_MASK_B, 0));
	p += sprintf(p, "reg_sysclk1_src_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SYSCLK1_SRC_MASK_B, 0));
	p += sprintf(p, "reg_wfi_op = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_WFI_OP, 0));
	p += sprintf(p, "reg_wfi_type = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_WFI_TYPE, 0));
	p += sprintf(p, "reg_mp0_cputop_idle_mask = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MP0_CPUTOP_IDLE_MASK, 0));
	p += sprintf(p, "reg_mp1_cputop_idle_mask = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MP1_CPUTOP_IDLE_MASK, 0));
	p += sprintf(p, "reg_mcusys_idle_mask = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MCUSYS_IDLE_MASK, 0));
	p += sprintf(p, "reg_md_apsrc_1_sel = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MD_APSRC_1_SEL, 0));
	p += sprintf(p, "reg_md_apsrc_0_sel = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MD_APSRC_0_SEL, 0));
	p += sprintf(p, "reg_conn_apsrc_sel = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_CONN_APSRC_SEL, 0));
	p += sprintf(p, "reg_spm_apsrc_req = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SPM_APSRC_REQ, 0));
	p += sprintf(p, "reg_spm_f26m_req = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SPM_F26M_REQ, 0));
	p += sprintf(p, "reg_spm_infra_req = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SPM_INFRA_REQ, 0));
	p += sprintf(p, "reg_spm_vrf18_req = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SPM_VRF18_REQ, 0));
	p += sprintf(p, "reg_spm_ddr_en_req = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SPM_DDR_EN_REQ, 0));
	p += sprintf(p, "reg_spm_ddr_en2_req = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SPM_DDR_EN2_REQ, 0));
	p += sprintf(p, "reg_spm_dvfs_req = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SPM_DVFS_REQ, 0));
	p += sprintf(p, "reg_spm_sw_mailbox_req = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SPM_SW_MAILBOX_REQ, 0));
	p += sprintf(p, "reg_spm_sspm_mailbox_req = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SPM_SSPM_MAILBOX_REQ, 0));
	p += sprintf(p, "reg_spm_adsp_mailbox_req = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SPM_ADSP_MAILBOX_REQ, 0));
	p += sprintf(p, "reg_spm_scp_mailbox_req = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SPM_SCP_MAILBOX_REQ, 0));
	p += sprintf(p, "reg_spm_mcusys_pwr_event_req = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SPM_MCUSYS_PWR_EVENT_REQ, 0));
	p += sprintf(p, "cpu_md_dvfs_sop_force_on = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_CPU_MD_DVFS_SOP_FORCE_ON, 0));
	p += sprintf(p, "reg_md_srcclkena_0_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MD_SRCCLKENA_0_MASK_B, 0));
	p += sprintf(p, "reg_md_srcclkena2infra_req_0_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MD_SRCCLKENA2INFRA_REQ_0_MASK_B, 0));
	p += sprintf(p, "reg_md_apsrc2infra_req_0_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MD_APSRC2INFRA_REQ_0_MASK_B, 0));
	p += sprintf(p, "reg_md_apsrc_req_0_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MD_APSRC_REQ_0_MASK_B, 0));
	p += sprintf(p, "reg_md_vrf18_req_0_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MD_VRF18_REQ_0_MASK_B, 0));
	p += sprintf(p, "reg_md_ddr_en_0_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MD_DDR_EN_0_MASK_B, 0));
	p += sprintf(p, "reg_md_ddr_en2_0_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MD_DDR_EN2_0_MASK_B, 0));
	p += sprintf(p, "reg_md_srcclkena_1_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MD_SRCCLKENA_1_MASK_B, 0));
	p += sprintf(p, "reg_md_srcclkena2infra_req_1_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MD_SRCCLKENA2INFRA_REQ_1_MASK_B, 0));
	p += sprintf(p, "reg_md_apsrc2infra_req_1_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MD_APSRC2INFRA_REQ_1_MASK_B, 0));
	p += sprintf(p, "reg_md_apsrc_req_1_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MD_APSRC_REQ_1_MASK_B, 0));
	p += sprintf(p, "reg_md_vrf18_req_1_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MD_VRF18_REQ_1_MASK_B, 0));
	p += sprintf(p, "reg_md_ddr_en_1_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MD_DDR_EN_1_MASK_B, 0));
	p += sprintf(p, "reg_md_ddr_en2_1_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MD_DDR_EN2_1_MASK_B, 0));
	p += sprintf(p, "reg_conn_srcclkena_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_CONN_SRCCLKENA_MASK_B, 0));
	p += sprintf(p, "reg_conn_srcclkenb_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_CONN_SRCCLKENB_MASK_B, 0));
	p += sprintf(p, "reg_conn_infra_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_CONN_INFRA_REQ_MASK_B, 0));
	p += sprintf(p, "reg_conn_apsrc_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_CONN_APSRC_REQ_MASK_B, 0));
	p += sprintf(p, "reg_conn_vrf18_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_CONN_VRF18_REQ_MASK_B, 0));
	p += sprintf(p, "reg_conn_ddr_en_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_CONN_DDR_EN_MASK_B, 0));
	p += sprintf(p, "reg_conn_ddr_en2_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_CONN_DDR_EN2_MASK_B, 0));
	p += sprintf(p, "reg_srcclkeni0_srcclkena_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SRCCLKENI0_SRCCLKENA_MASK_B, 0));
	p += sprintf(p, "reg_srcclkeni0_infra_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SRCCLKENI0_INFRA_REQ_MASK_B, 0));
	p += sprintf(p, "reg_srcclkeni1_srcclkena_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SRCCLKENI1_SRCCLKENA_MASK_B, 0));
	p += sprintf(p, "reg_srcclkeni1_infra_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SRCCLKENI1_INFRA_REQ_MASK_B, 0));
	p += sprintf(p, "reg_srcclkeni2_srcclkena_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SRCCLKENI2_SRCCLKENA_MASK_B, 0));
	p += sprintf(p, "reg_srcclkeni2_infra_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SRCCLKENI2_INFRA_REQ_MASK_B, 0));
	p += sprintf(p, "reg_infrasys_apsrc_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_INFRASYS_APSRC_REQ_MASK_B, 0));
	p += sprintf(p, "reg_infrasys_ddr_en_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_INFRASYS_DDR_EN_MASK_B, 0));
	p += sprintf(p, "reg_infrasys_ddr_en2_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_INFRASYS_DDR_EN2_MASK_B, 0));
	p += sprintf(p, "reg_md32_srcclkena_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MD32_SRCCLKENA_MASK_B, 0));
	p += sprintf(p, "reg_conn_vfe28_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_CONN_VFE28_REQ_MASK_B, 0));
	p += sprintf(p, "reg_md32_infra_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MD32_INFRA_REQ_MASK_B, 0));
	p += sprintf(p, "reg_md32_apsrc_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MD32_APSRC_REQ_MASK_B, 0));
	p += sprintf(p, "reg_md32_vrf18_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MD32_VRF18_REQ_MASK_B, 0));
	p += sprintf(p, "reg_md32_ddr_en_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MD32_DDR_EN_MASK_B, 0));
	p += sprintf(p, "reg_md32_ddr_en2_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MD32_DDR_EN2_MASK_B, 0));
	p += sprintf(p, "reg_scp_srcclkena_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SCP_SRCCLKENA_MASK_B, 0));
	p += sprintf(p, "reg_scp_infra_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SCP_INFRA_REQ_MASK_B, 0));
	p += sprintf(p, "reg_scp_apsrc_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SCP_APSRC_REQ_MASK_B, 0));
	p += sprintf(p, "reg_scp_vrf18_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SCP_VRF18_REQ_MASK_B, 0));
	p += sprintf(p, "reg_scp_ddr_en_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SCP_DDR_EN_MASK_B, 0));
	p += sprintf(p, "reg_scp_ddr_en2_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SCP_DDR_EN2_MASK_B, 0));
	p += sprintf(p, "reg_ufs_srcclkena_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_UFS_SRCCLKENA_MASK_B, 0));
	p += sprintf(p, "reg_ufs_infra_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_UFS_INFRA_REQ_MASK_B, 0));
	p += sprintf(p, "reg_ufs_apsrc_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_UFS_APSRC_REQ_MASK_B, 0));
	p += sprintf(p, "reg_ufs_vrf18_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_UFS_VRF18_REQ_MASK_B, 0));
	p += sprintf(p, "reg_ufs_ddr_en_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_UFS_DDR_EN_MASK_B, 0));
	p += sprintf(p, "reg_ufs_ddr_en2_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_UFS_DDR_EN2_MASK_B, 0));
	p += sprintf(p, "reg_disp0_apsrc_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_DISP0_APSRC_REQ_MASK_B, 0));
	p += sprintf(p, "reg_disp0_ddr_en_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_DISP0_DDR_EN_MASK_B, 0));
	p += sprintf(p, "reg_disp0_ddr_en2_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_DISP0_DDR_EN2_MASK_B, 0));
	p += sprintf(p, "reg_disp1_apsrc_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_DISP1_APSRC_REQ_MASK_B, 0));
	p += sprintf(p, "reg_disp1_ddr_en_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_DISP1_DDR_EN_MASK_B, 0));
	p += sprintf(p, "reg_disp1_ddr_en2_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_DISP1_DDR_EN2_MASK_B, 0));
	p += sprintf(p, "reg_gce_infra_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_GCE_INFRA_REQ_MASK_B, 0));
	p += sprintf(p, "reg_gce_apsrc_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_GCE_APSRC_REQ_MASK_B, 0));
	p += sprintf(p, "reg_gce_vrf18_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_GCE_VRF18_REQ_MASK_B, 0));
	p += sprintf(p, "reg_gce_ddr_en_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_GCE_DDR_EN_MASK_B, 0));
	p += sprintf(p, "reg_gce_ddr_en2_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_GCE_DDR_EN2_MASK_B, 0));
	p += sprintf(p, "reg_emi_ch0_ddr_en_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_EMI_CH0_DDR_EN_MASK_B, 0));
	p += sprintf(p, "reg_emi_ch1_ddr_en_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_EMI_CH1_DDR_EN_MASK_B, 0));
	p += sprintf(p, "reg_emi_ch0_ddr_en2_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_EMI_CH0_DDR_EN2_MASK_B, 0));
	p += sprintf(p, "reg_emi_ch1_ddr_en2_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_EMI_CH1_DDR_EN2_MASK_B, 0));
	p += sprintf(p, "reg_dvfsrc_event_trigger_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_DVFSRC_EVENT_TRIGGER_MASK_B, 0));
	p += sprintf(p, "reg_sw2spm_int0_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SW2SPM_INT0_MASK_B, 0));
	p += sprintf(p, "reg_sw2spm_int1_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SW2SPM_INT1_MASK_B, 0));
	p += sprintf(p, "reg_sw2spm_int2_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SW2SPM_INT2_MASK_B, 0));
	p += sprintf(p, "reg_sw2spm_int3_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SW2SPM_INT3_MASK_B, 0));
	p += sprintf(p, "reg_sc_adsp2spm_wakeup_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SC_ADSP2SPM_WAKEUP_MASK_B, 0));
	p += sprintf(p, "reg_sc_sspm2spm_wakeup_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SC_SSPM2SPM_WAKEUP_MASK_B, 0));
	p += sprintf(p, "reg_sc_scp2spm_wakeup_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SC_SCP2SPM_WAKEUP_MASK_B, 0));
	p += sprintf(p, "reg_csyspwrreq_mask = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_CSYSPWRREQ_MASK, 0));
	p += sprintf(p, "reg_spm_srcclkena_reserved_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SPM_SRCCLKENA_RESERVED_MASK_B, 0));
	p += sprintf(p, "reg_spm_infra_req_reserved_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SPM_INFRA_REQ_RESERVED_MASK_B, 0));
	p += sprintf(p, "reg_spm_apsrc_req_reserved_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SPM_APSRC_REQ_RESERVED_MASK_B, 0));
	p += sprintf(p, "reg_spm_vrf18_req_reserved_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SPM_VRF18_REQ_RESERVED_MASK_B, 0));
	p += sprintf(p, "reg_spm_ddr_en_reserved_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SPM_DDR_EN_RESERVED_MASK_B, 0));
	p += sprintf(p, "reg_spm_ddr_en2_reserved_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_SPM_DDR_EN2_RESERVED_MASK_B, 0));
	p += sprintf(p, "reg_audio_dsp_srcclkena_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_AUDIO_DSP_SRCCLKENA_MASK_B, 0));
	p += sprintf(p, "reg_audio_dsp_infra_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_AUDIO_DSP_INFRA_REQ_MASK_B, 0));
	p += sprintf(p, "reg_audio_dsp_apsrc_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_AUDIO_DSP_APSRC_REQ_MASK_B, 0));
	p += sprintf(p, "reg_audio_dsp_vrf18_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_AUDIO_DSP_VRF18_REQ_MASK_B, 0));
	p += sprintf(p, "reg_audio_dsp_ddr_en_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_AUDIO_DSP_DDR_EN_MASK_B, 0));
	p += sprintf(p, "reg_audio_dsp_ddr_en2_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_AUDIO_DSP_DDR_EN2_MASK_B, 0));
	p += sprintf(p, "reg_mcusys_pwr_event_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MCUSYS_PWR_EVENT_MASK_B, 0));
	p += sprintf(p, "reg_msdc0_srcclkena_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MSDC0_SRCCLKENA_MASK_B, 0));
	p += sprintf(p, "reg_msdc0_infra_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MSDC0_INFRA_REQ_MASK_B, 0));
	p += sprintf(p, "reg_msdc0_apsrc_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MSDC0_APSRC_REQ_MASK_B, 0));
	p += sprintf(p, "reg_msdc0_vrf18_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MSDC0_VRF18_REQ_MASK_B, 0));
	p += sprintf(p, "reg_msdc0_ddr_en_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MSDC0_DDR_EN_MASK_B, 0));
	p += sprintf(p, "reg_msdc0_ddr_en2_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MSDC0_DDR_EN2_MASK_B, 0));
	p += sprintf(p, "reg_conn_srcclkenb2pwrap_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_CONN_SRCCLKENB2PWRAP_MASK_B, 0));
	p += sprintf(p, "ccif_event_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_CCIF_EVENT_MASK_B, 0));
	p += sprintf(p, "reg_apu_core0_srcclkena_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_APU_CORE0_SRCCLKENA_MASK_B, 0));
	p += sprintf(p, "reg_apu_core0_infra_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_APU_CORE0_INFRA_REQ_MASK_B, 0));
	p += sprintf(p, "reg_apu_core0_apsrc_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_APU_CORE0_APSRC_REQ_MASK_B, 0));
	p += sprintf(p, "reg_apu_core0_vrf18_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_APU_CORE0_VRF18_REQ_MASK_B, 0));
	p += sprintf(p, "reg_apu_core0_ddr_en_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_APU_CORE0_DDR_EN_MASK_B, 0));
	p += sprintf(p, "reg_apu_core1_srcclkena_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_APU_CORE1_SRCCLKENA_MASK_B, 0));
	p += sprintf(p, "reg_apu_core1_infra_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_APU_CORE1_INFRA_REQ_MASK_B, 0));
	p += sprintf(p, "reg_apu_core1_apsrc_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_APU_CORE1_APSRC_REQ_MASK_B, 0));
	p += sprintf(p, "reg_apu_core1_vrf18_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_APU_CORE1_VRF18_REQ_MASK_B, 0));
	p += sprintf(p, "reg_apu_core1_ddr_en_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_APU_CORE1_DDR_EN_MASK_B, 0));
	p += sprintf(p, "reg_apu_core2_srcclkena_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_APU_CORE2_SRCCLKENA_MASK_B, 0));
	p += sprintf(p, "reg_apu_core2_infra_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_APU_CORE2_INFRA_REQ_MASK_B, 0));
	p += sprintf(p, "reg_apu_core2_apsrc_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_APU_CORE2_APSRC_REQ_MASK_B, 0));
	p += sprintf(p, "reg_apu_core2_vrf18_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_APU_CORE2_VRF18_REQ_MASK_B, 0));
	p += sprintf(p, "reg_apu_core2_ddr_en_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_APU_CORE2_DDR_EN_MASK_B, 0));
	p += sprintf(p, "reg_apu_core2_ddr_en2_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_APU_CORE2_DDR_EN2_MASK_B, 0));
	p += sprintf(p, "reg_mcusys_merge_apsrc_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MCUSYS_MERGE_APSRC_REQ_MASK_B, 0));
	p += sprintf(p, "reg_mcusys_merge_ddr_en_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MCUSYS_MERGE_DDR_EN_MASK_B, 0));
	p += sprintf(p, "reg_mcusys_merge_ddr_en2_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MCUSYS_MERGE_DDR_EN2_MASK_B, 0));
	p += sprintf(p, "reg_apu_core0_ddr_en2_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_APU_CORE0_DDR_EN2_MASK_B, 0));
	p += sprintf(p, "reg_apu_core1_ddr_en2_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_APU_CORE1_DDR_EN2_MASK_B, 0));
	p += sprintf(p, "reg_cg_check_ddr_en_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_CG_CHECK_DDR_EN_MASK_B, 0));
	p += sprintf(p, "reg_cg_check_ddr_en2_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_CG_CHECK_DDR_EN2_MASK_B, 0));
	p += sprintf(p, "reg_wakeup_event_mask = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_WAKEUP_EVENT_MASK, 0));
	p += sprintf(p, "reg_ext_wakeup_event_mask = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_EXT_WAKEUP_EVENT_MASK, 0));
	p += sprintf(p, "reg_msdc1_srcclkena_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MSDC1_SRCCLKENA_MASK_B, 0));
	p += sprintf(p, "reg_msdc1_infra_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MSDC1_INFRA_REQ_MASK_B, 0));
	p += sprintf(p, "reg_msdc1_apsrc_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MSDC1_APSRC_REQ_MASK_B, 0));
	p += sprintf(p, "reg_msdc1_vrf18_req_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MSDC1_VRF18_REQ_MASK_B, 0));
	p += sprintf(p, "reg_msdc1_ddr_en_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MSDC1_DDR_EN_MASK_B, 0));
	p += sprintf(p, "reg_msdc1_ddr_en2_mask_b = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MSDC1_DDR_EN2_MASK_B, 0));
	p += sprintf(p, "reg_msdc1_srcclkena_ack_mask = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MSDC1_SRCCLKENA_ACK_MASK, 0));
	p += sprintf(p, "reg_msdc1_infra_ack_mask = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MSDC1_INFRA_ACK_MASK, 0));
	p += sprintf(p, "reg_msdc1_apsrc_ack_mask = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MSDC1_APSRC_ACK_MASK, 0));
	p += sprintf(p, "reg_msdc1_vrf18_ack_mask = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MSDC1_VRF18_ACK_MASK, 0));
	p += sprintf(p, "reg_msdc1_ddr_en_ack_mask = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MSDC1_DDR_EN_ACK_MASK, 0));
	p += sprintf(p, "reg_msdc1_ddr_en2_ack_mask = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_REG_MSDC1_DDR_EN2_ACK_MASK, 0));
	p += sprintf(p, "mp0_cpu0_wfi_en = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_MP0_CPU0_WFI_EN, 0));
	p += sprintf(p, "mp0_cpu1_wfi_en = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_MP0_CPU1_WFI_EN, 0));
	p += sprintf(p, "mp0_cpu2_wfi_en = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_MP0_CPU2_WFI_EN, 0));
	p += sprintf(p, "mp0_cpu3_wfi_en = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_MP0_CPU3_WFI_EN, 0));
	p += sprintf(p, "mp0_cpu4_wfi_en = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_MP0_CPU4_WFI_EN, 0));
	p += sprintf(p, "mp0_cpu5_wfi_en = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_MP0_CPU5_WFI_EN, 0));
	p += sprintf(p, "mp0_cpu6_wfi_en = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_MP0_CPU6_WFI_EN, 0));
	p += sprintf(p, "mp0_cpu7_wfi_en = 0x%zx\n",
		SMC_CALL(GET_PWR_CTRL_ARGS,
			id, PW_MP0_CPU7_WFI_EN, 0));

	WARN_ON(p - buf >= get_mtk_lp_kernfs_bufsz_max());

	return p - buf;
}

static ssize_t suspend_ctrl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return show_pwr_ctrl(SPM_PWR_CTRL_SUSPEND, &pwrctrl_suspend
		, buf);
}

static ssize_t IdleDram_ctrl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return show_pwr_ctrl(SPM_PWR_CTRL_IDLE_DRAM, &pwrctrl_dram
		, buf);
}

static ssize_t IdleSyspll_ctrl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return show_pwr_ctrl(SPM_PWR_CTRL_IDLE_SYSPLL, &pwrctrl_syspll
		, buf);
}

static ssize_t IdleBus26m_ctrl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return show_pwr_ctrl(SPM_PWR_CTRL_IDLE_BUS26M, &pwrctrl_bus26m
		, buf);
}

static ssize_t vcore_dvfs_ctrl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
#if 0 //FIXME
	return show_pwr_ctrl(SPM_PWR_CTRL_VCOREFS, __spm_vcorefs.pwrctrl, buf);
#else
	return 0;
#endif
}

static ssize_t spmfw_version_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return get_spmfw_version(buf, PAGE_SIZE, NULL);
}

/**************************************
 * xxx_ctrl_store Function
 **************************************/
/* code gen by spm_pwr_ctrl_atf.pl, need struct pwr_ctrl */
static ssize_t store_pwr_ctrl(int id, struct pwr_ctrl *pwrctrl,
	const char *buf, size_t count)
{
	u32 val;
	char cmd[64];

	if (sscanf(buf, "%63s %x", cmd, &val) != 2)
		return -EPERM;

	printk_deferred("[name:spm&] pwr_ctrl: cmd = %s, val = 0x%x\n",
		cmd, val);


	if (!strcmp(cmd,
		pwr_ctrl_str[PW_PCM_FLAGS])) {
		pwrctrl->pcm_flags = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_PCM_FLAGS, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_PCM_FLAGS_CUST])) {
		pwrctrl->pcm_flags_cust = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_PCM_FLAGS_CUST, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_PCM_FLAGS_CUST_SET])) {
		pwrctrl->pcm_flags_cust_set = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_PCM_FLAGS_CUST_SET, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_PCM_FLAGS_CUST_CLR])) {
		pwrctrl->pcm_flags_cust_clr = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_PCM_FLAGS_CUST_CLR, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_PCM_FLAGS1])) {
		pwrctrl->pcm_flags1 = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_PCM_FLAGS1, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_PCM_FLAGS1_CUST])) {
		pwrctrl->pcm_flags1_cust = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_PCM_FLAGS1_CUST, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_PCM_FLAGS1_CUST_SET])) {
		pwrctrl->pcm_flags1_cust_set = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_PCM_FLAGS1_CUST_SET, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_PCM_FLAGS1_CUST_CLR])) {
		pwrctrl->pcm_flags1_cust_clr = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_PCM_FLAGS1_CUST_CLR, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_TIMER_VAL])) {
		pwrctrl->timer_val = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_TIMER_VAL, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_TIMER_VAL_CUST])) {
		pwrctrl->timer_val_cust = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_TIMER_VAL_CUST, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_TIMER_VAL_RAMP_EN])) {
		pwrctrl->timer_val_ramp_en = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_TIMER_VAL_RAMP_EN, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_TIMER_VAL_RAMP_EN_SEC])) {
		pwrctrl->timer_val_ramp_en_sec = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_TIMER_VAL_RAMP_EN_SEC, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_WAKE_SRC])) {
		pwrctrl->wake_src = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_WAKE_SRC, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_WAKE_SRC_CUST])) {
		pwrctrl->wake_src_cust = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_WAKE_SRC_CUST, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_WAKELOCK_TIMER_VAL])) {
		pwrctrl->wakelock_timer_val = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_WAKELOCK_TIMER_VAL, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SRCCLKEN0_CTL])) {
		pwrctrl->reg_srcclken0_ctl = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SRCCLKEN0_CTL, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SRCCLKEN1_CTL])) {
		pwrctrl->reg_srcclken1_ctl = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SRCCLKEN1_CTL, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SPM_LOCK_INFRA_DCM])) {
		pwrctrl->reg_spm_lock_infra_dcm = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SPM_LOCK_INFRA_DCM, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SRCCLKEN_MASK])) {
		pwrctrl->reg_srcclken_mask = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SRCCLKEN_MASK, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MD1_C32RM_EN])) {
		pwrctrl->reg_md1_c32rm_en = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MD1_C32RM_EN, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MD2_C32RM_EN])) {
		pwrctrl->reg_md2_c32rm_en = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MD2_C32RM_EN, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_CLKSQ0_SEL_CTRL])) {
		pwrctrl->reg_clksq0_sel_ctrl = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_CLKSQ0_SEL_CTRL, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_CLKSQ1_SEL_CTRL])) {
		pwrctrl->reg_clksq1_sel_ctrl = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_CLKSQ1_SEL_CTRL, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SRCCLKEN0_EN])) {
		pwrctrl->reg_srcclken0_en = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SRCCLKEN0_EN, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SRCCLKEN1_EN])) {
		pwrctrl->reg_srcclken1_en = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SRCCLKEN1_EN, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SYSCLK0_SRC_MASK_B])) {
		pwrctrl->reg_sysclk0_src_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SYSCLK0_SRC_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SYSCLK1_SRC_MASK_B])) {
		pwrctrl->reg_sysclk1_src_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SYSCLK1_SRC_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_WFI_OP])) {
		pwrctrl->reg_wfi_op = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_WFI_OP, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_WFI_TYPE])) {
		pwrctrl->reg_wfi_type = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_WFI_TYPE, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MP0_CPUTOP_IDLE_MASK])) {
		pwrctrl->reg_mp0_cputop_idle_mask = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MP0_CPUTOP_IDLE_MASK, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MP1_CPUTOP_IDLE_MASK])) {
		pwrctrl->reg_mp1_cputop_idle_mask = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MP1_CPUTOP_IDLE_MASK, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MCUSYS_IDLE_MASK])) {
		pwrctrl->reg_mcusys_idle_mask = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MCUSYS_IDLE_MASK, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MD_APSRC_1_SEL])) {
		pwrctrl->reg_md_apsrc_1_sel = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MD_APSRC_1_SEL, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MD_APSRC_0_SEL])) {
		pwrctrl->reg_md_apsrc_0_sel = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MD_APSRC_0_SEL, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_CONN_APSRC_SEL])) {
		pwrctrl->reg_conn_apsrc_sel = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_CONN_APSRC_SEL, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SPM_APSRC_REQ])) {
		unsigned int req = (val == 0) ?
			SPM_RESOURCE_CONSOLE_RELEASE : SPM_RESOURCE_CONSOLE_REQ;
		if (spm_resource_req_console_by_id(id, req
			, _RES_MASK(MTK_SPM_RES_EX_DRAM_S0)) != 0) {
			pwrctrl->reg_spm_apsrc_req = val;
			SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SPM_APSRC_REQ, val);
		}
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SPM_F26M_REQ])) {
		unsigned int req = (val == 0) ?
			SPM_RESOURCE_CONSOLE_RELEASE : SPM_RESOURCE_CONSOLE_REQ;
		if (spm_resource_req_console_by_id(id, req
			, _RES_MASK(MTK_SPM_RES_EX_26M)) != 0) {
			pwrctrl->reg_spm_f26m_req = val;
			SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SPM_F26M_REQ, val);
		}
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SPM_INFRA_REQ])) {
		unsigned int req = (val == 0) ?
			SPM_RESOURCE_CONSOLE_RELEASE : SPM_RESOURCE_CONSOLE_REQ;
		if (spm_resource_req_console_by_id(id, req
			, _RES_MASK(MTK_SPM_RES_EX_AXI_BUS)) != 0) {
			pwrctrl->reg_spm_infra_req = val;
			SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SPM_INFRA_REQ, val);
		}
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SPM_VRF18_REQ])) {
		unsigned int req = (val == 0) ?
			SPM_RESOURCE_CONSOLE_RELEASE : SPM_RESOURCE_CONSOLE_REQ;
		if (spm_resource_req_console_by_id(id, req
			, _RES_MASK(MTK_SPM_RES_EX_MAINPLL)) != 0) {
			pwrctrl->reg_spm_vrf18_req = val;
			SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SPM_VRF18_REQ, val);
		}
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SPM_DDR_EN_REQ])) {
		unsigned int req = (val == 0) ?
			SPM_RESOURCE_CONSOLE_RELEASE : SPM_RESOURCE_CONSOLE_REQ;
		if (spm_resource_req_console_by_id(id, req
			, _RES_MASK(MTK_SPM_RES_EX_DRAM_S1)) != 0) {
			pwrctrl->reg_spm_ddr_en_req = val;
			SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SPM_DDR_EN_REQ, val);
		}
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SPM_DDR_EN2_REQ])) {
		pwrctrl->reg_spm_ddr_en2_req = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SPM_DDR_EN2_REQ, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SPM_DVFS_REQ])) {
		pwrctrl->reg_spm_dvfs_req = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SPM_DVFS_REQ, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SPM_SW_MAILBOX_REQ])) {
		pwrctrl->reg_spm_sw_mailbox_req = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SPM_SW_MAILBOX_REQ, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SPM_SSPM_MAILBOX_REQ])) {
		pwrctrl->reg_spm_sspm_mailbox_req = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SPM_SSPM_MAILBOX_REQ, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SPM_ADSP_MAILBOX_REQ])) {
		pwrctrl->reg_spm_adsp_mailbox_req = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SPM_ADSP_MAILBOX_REQ, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SPM_SCP_MAILBOX_REQ])) {
		pwrctrl->reg_spm_scp_mailbox_req = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SPM_SCP_MAILBOX_REQ, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SPM_MCUSYS_PWR_EVENT_REQ])) {
		pwrctrl->reg_spm_mcusys_pwr_event_req = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SPM_MCUSYS_PWR_EVENT_REQ, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_CPU_MD_DVFS_SOP_FORCE_ON])) {
		pwrctrl->cpu_md_dvfs_sop_force_on = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_CPU_MD_DVFS_SOP_FORCE_ON, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MD_SRCCLKENA_0_MASK_B])) {
		pwrctrl->reg_md_srcclkena_0_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MD_SRCCLKENA_0_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MD_SRCCLKENA2INFRA_REQ_0_MASK_B])) {
		pwrctrl->reg_md_srcclkena2infra_req_0_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MD_SRCCLKENA2INFRA_REQ_0_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MD_APSRC2INFRA_REQ_0_MASK_B])) {
		pwrctrl->reg_md_apsrc2infra_req_0_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MD_APSRC2INFRA_REQ_0_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MD_APSRC_REQ_0_MASK_B])) {
		pwrctrl->reg_md_apsrc_req_0_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MD_APSRC_REQ_0_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MD_VRF18_REQ_0_MASK_B])) {
		pwrctrl->reg_md_vrf18_req_0_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MD_VRF18_REQ_0_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MD_DDR_EN_0_MASK_B])) {
		pwrctrl->reg_md_ddr_en_0_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MD_DDR_EN_0_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MD_DDR_EN2_0_MASK_B])) {
		pwrctrl->reg_md_ddr_en2_0_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MD_DDR_EN2_0_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MD_SRCCLKENA_1_MASK_B])) {
		pwrctrl->reg_md_srcclkena_1_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MD_SRCCLKENA_1_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MD_SRCCLKENA2INFRA_REQ_1_MASK_B])) {
		pwrctrl->reg_md_srcclkena2infra_req_1_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MD_SRCCLKENA2INFRA_REQ_1_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MD_APSRC2INFRA_REQ_1_MASK_B])) {
		pwrctrl->reg_md_apsrc2infra_req_1_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MD_APSRC2INFRA_REQ_1_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MD_APSRC_REQ_1_MASK_B])) {
		pwrctrl->reg_md_apsrc_req_1_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MD_APSRC_REQ_1_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MD_VRF18_REQ_1_MASK_B])) {
		pwrctrl->reg_md_vrf18_req_1_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MD_VRF18_REQ_1_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MD_DDR_EN_1_MASK_B])) {
		pwrctrl->reg_md_ddr_en_1_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MD_DDR_EN_1_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MD_DDR_EN2_1_MASK_B])) {
		pwrctrl->reg_md_ddr_en2_1_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MD_DDR_EN2_1_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_CONN_SRCCLKENA_MASK_B])) {
		pwrctrl->reg_conn_srcclkena_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_CONN_SRCCLKENA_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_CONN_SRCCLKENB_MASK_B])) {
		pwrctrl->reg_conn_srcclkenb_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_CONN_SRCCLKENB_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_CONN_INFRA_REQ_MASK_B])) {
		pwrctrl->reg_conn_infra_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_CONN_INFRA_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_CONN_APSRC_REQ_MASK_B])) {
		pwrctrl->reg_conn_apsrc_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_CONN_APSRC_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_CONN_VRF18_REQ_MASK_B])) {
		pwrctrl->reg_conn_vrf18_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_CONN_VRF18_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_CONN_DDR_EN_MASK_B])) {
		pwrctrl->reg_conn_ddr_en_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_CONN_DDR_EN_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_CONN_DDR_EN2_MASK_B])) {
		pwrctrl->reg_conn_ddr_en2_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_CONN_DDR_EN2_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SRCCLKENI0_SRCCLKENA_MASK_B])) {
		pwrctrl->reg_srcclkeni0_srcclkena_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SRCCLKENI0_SRCCLKENA_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SRCCLKENI0_INFRA_REQ_MASK_B])) {
		pwrctrl->reg_srcclkeni0_infra_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SRCCLKENI0_INFRA_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SRCCLKENI1_SRCCLKENA_MASK_B])) {
		pwrctrl->reg_srcclkeni1_srcclkena_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SRCCLKENI1_SRCCLKENA_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SRCCLKENI1_INFRA_REQ_MASK_B])) {
		pwrctrl->reg_srcclkeni1_infra_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SRCCLKENI1_INFRA_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SRCCLKENI2_SRCCLKENA_MASK_B])) {
		pwrctrl->reg_srcclkeni2_srcclkena_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SRCCLKENI2_SRCCLKENA_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SRCCLKENI2_INFRA_REQ_MASK_B])) {
		pwrctrl->reg_srcclkeni2_infra_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SRCCLKENI2_INFRA_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_INFRASYS_APSRC_REQ_MASK_B])) {
		pwrctrl->reg_infrasys_apsrc_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_INFRASYS_APSRC_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_INFRASYS_DDR_EN_MASK_B])) {
		pwrctrl->reg_infrasys_ddr_en_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_INFRASYS_DDR_EN_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_INFRASYS_DDR_EN2_MASK_B])) {
		pwrctrl->reg_infrasys_ddr_en2_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_INFRASYS_DDR_EN2_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MD32_SRCCLKENA_MASK_B])) {
		pwrctrl->reg_md32_srcclkena_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MD32_SRCCLKENA_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_CONN_VFE28_REQ_MASK_B])) {
		pwrctrl->reg_conn_vfe28_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_CONN_VFE28_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MD32_INFRA_REQ_MASK_B])) {
		pwrctrl->reg_md32_infra_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MD32_INFRA_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MD32_APSRC_REQ_MASK_B])) {
		pwrctrl->reg_md32_apsrc_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MD32_APSRC_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MD32_VRF18_REQ_MASK_B])) {
		pwrctrl->reg_md32_vrf18_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MD32_VRF18_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MD32_DDR_EN_MASK_B])) {
		pwrctrl->reg_md32_ddr_en_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MD32_DDR_EN_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MD32_DDR_EN2_MASK_B])) {
		pwrctrl->reg_md32_ddr_en2_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MD32_DDR_EN2_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SCP_SRCCLKENA_MASK_B])) {
		pwrctrl->reg_scp_srcclkena_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SCP_SRCCLKENA_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SCP_INFRA_REQ_MASK_B])) {
		pwrctrl->reg_scp_infra_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SCP_INFRA_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SCP_APSRC_REQ_MASK_B])) {
		pwrctrl->reg_scp_apsrc_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SCP_APSRC_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SCP_VRF18_REQ_MASK_B])) {
		pwrctrl->reg_scp_vrf18_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SCP_VRF18_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SCP_DDR_EN_MASK_B])) {
		pwrctrl->reg_scp_ddr_en_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SCP_DDR_EN_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SCP_DDR_EN2_MASK_B])) {
		pwrctrl->reg_scp_ddr_en2_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SCP_DDR_EN2_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_UFS_SRCCLKENA_MASK_B])) {
		pwrctrl->reg_ufs_srcclkena_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_UFS_SRCCLKENA_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_UFS_INFRA_REQ_MASK_B])) {
		pwrctrl->reg_ufs_infra_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_UFS_INFRA_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_UFS_APSRC_REQ_MASK_B])) {
		pwrctrl->reg_ufs_apsrc_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_UFS_APSRC_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_UFS_VRF18_REQ_MASK_B])) {
		pwrctrl->reg_ufs_vrf18_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_UFS_VRF18_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_UFS_DDR_EN_MASK_B])) {
		pwrctrl->reg_ufs_ddr_en_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_UFS_DDR_EN_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_UFS_DDR_EN2_MASK_B])) {
		pwrctrl->reg_ufs_ddr_en2_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_UFS_DDR_EN2_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_DISP0_APSRC_REQ_MASK_B])) {
		pwrctrl->reg_disp0_apsrc_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_DISP0_APSRC_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_DISP0_DDR_EN_MASK_B])) {
		pwrctrl->reg_disp0_ddr_en_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_DISP0_DDR_EN_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_DISP0_DDR_EN2_MASK_B])) {
		pwrctrl->reg_disp0_ddr_en2_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_DISP0_DDR_EN2_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_DISP1_APSRC_REQ_MASK_B])) {
		pwrctrl->reg_disp1_apsrc_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_DISP1_APSRC_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_DISP1_DDR_EN_MASK_B])) {
		pwrctrl->reg_disp1_ddr_en_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_DISP1_DDR_EN_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_DISP1_DDR_EN2_MASK_B])) {
		pwrctrl->reg_disp1_ddr_en2_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_DISP1_DDR_EN2_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_GCE_INFRA_REQ_MASK_B])) {
		pwrctrl->reg_gce_infra_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_GCE_INFRA_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_GCE_APSRC_REQ_MASK_B])) {
		pwrctrl->reg_gce_apsrc_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_GCE_APSRC_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_GCE_VRF18_REQ_MASK_B])) {
		pwrctrl->reg_gce_vrf18_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_GCE_VRF18_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_GCE_DDR_EN_MASK_B])) {
		pwrctrl->reg_gce_ddr_en_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_GCE_DDR_EN_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_GCE_DDR_EN2_MASK_B])) {
		pwrctrl->reg_gce_ddr_en2_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_GCE_DDR_EN2_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_EMI_CH0_DDR_EN_MASK_B])) {
		pwrctrl->reg_emi_ch0_ddr_en_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_EMI_CH0_DDR_EN_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_EMI_CH1_DDR_EN_MASK_B])) {
		pwrctrl->reg_emi_ch1_ddr_en_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_EMI_CH1_DDR_EN_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_EMI_CH0_DDR_EN2_MASK_B])) {
		pwrctrl->reg_emi_ch0_ddr_en2_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_EMI_CH0_DDR_EN2_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_EMI_CH1_DDR_EN2_MASK_B])) {
		pwrctrl->reg_emi_ch1_ddr_en2_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_EMI_CH1_DDR_EN2_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_DVFSRC_EVENT_TRIGGER_MASK_B])) {
		pwrctrl->reg_dvfsrc_event_trigger_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_DVFSRC_EVENT_TRIGGER_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SW2SPM_INT0_MASK_B])) {
		pwrctrl->reg_sw2spm_int0_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SW2SPM_INT0_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SW2SPM_INT1_MASK_B])) {
		pwrctrl->reg_sw2spm_int1_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SW2SPM_INT1_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SW2SPM_INT2_MASK_B])) {
		pwrctrl->reg_sw2spm_int2_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SW2SPM_INT2_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SW2SPM_INT3_MASK_B])) {
		pwrctrl->reg_sw2spm_int3_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SW2SPM_INT3_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SC_ADSP2SPM_WAKEUP_MASK_B])) {
		pwrctrl->reg_sc_adsp2spm_wakeup_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SC_ADSP2SPM_WAKEUP_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SC_SSPM2SPM_WAKEUP_MASK_B])) {
		pwrctrl->reg_sc_sspm2spm_wakeup_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SC_SSPM2SPM_WAKEUP_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SC_SCP2SPM_WAKEUP_MASK_B])) {
		pwrctrl->reg_sc_scp2spm_wakeup_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SC_SCP2SPM_WAKEUP_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_CSYSPWRREQ_MASK])) {
		pwrctrl->reg_csyspwrreq_mask = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_CSYSPWRREQ_MASK, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SPM_SRCCLKENA_RESERVED_MASK_B])) {
		pwrctrl->reg_spm_srcclkena_reserved_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SPM_SRCCLKENA_RESERVED_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SPM_INFRA_REQ_RESERVED_MASK_B])) {
		pwrctrl->reg_spm_infra_req_reserved_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SPM_INFRA_REQ_RESERVED_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SPM_APSRC_REQ_RESERVED_MASK_B])) {
		pwrctrl->reg_spm_apsrc_req_reserved_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SPM_APSRC_REQ_RESERVED_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SPM_VRF18_REQ_RESERVED_MASK_B])) {
		pwrctrl->reg_spm_vrf18_req_reserved_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SPM_VRF18_REQ_RESERVED_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SPM_DDR_EN_RESERVED_MASK_B])) {
		pwrctrl->reg_spm_ddr_en_reserved_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SPM_DDR_EN_RESERVED_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SPM_DDR_EN2_RESERVED_MASK_B])) {
		pwrctrl->reg_spm_ddr_en2_reserved_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SPM_DDR_EN2_RESERVED_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_AUDIO_DSP_SRCCLKENA_MASK_B])) {
		pwrctrl->reg_audio_dsp_srcclkena_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_AUDIO_DSP_SRCCLKENA_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_AUDIO_DSP_INFRA_REQ_MASK_B])) {
		pwrctrl->reg_audio_dsp_infra_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_AUDIO_DSP_INFRA_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_AUDIO_DSP_APSRC_REQ_MASK_B])) {
		pwrctrl->reg_audio_dsp_apsrc_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_AUDIO_DSP_APSRC_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_AUDIO_DSP_VRF18_REQ_MASK_B])) {
		pwrctrl->reg_audio_dsp_vrf18_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_AUDIO_DSP_VRF18_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_AUDIO_DSP_DDR_EN_MASK_B])) {
		pwrctrl->reg_audio_dsp_ddr_en_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_AUDIO_DSP_DDR_EN_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_AUDIO_DSP_DDR_EN2_MASK_B])) {
		pwrctrl->reg_audio_dsp_ddr_en2_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_AUDIO_DSP_DDR_EN2_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MCUSYS_PWR_EVENT_MASK_B])) {
		pwrctrl->reg_mcusys_pwr_event_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MCUSYS_PWR_EVENT_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MSDC0_SRCCLKENA_MASK_B])) {
		pwrctrl->reg_msdc0_srcclkena_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MSDC0_SRCCLKENA_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MSDC0_INFRA_REQ_MASK_B])) {
		pwrctrl->reg_msdc0_infra_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MSDC0_INFRA_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MSDC0_APSRC_REQ_MASK_B])) {
		pwrctrl->reg_msdc0_apsrc_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MSDC0_APSRC_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MSDC0_VRF18_REQ_MASK_B])) {
		pwrctrl->reg_msdc0_vrf18_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MSDC0_VRF18_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MSDC0_DDR_EN_MASK_B])) {
		pwrctrl->reg_msdc0_ddr_en_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MSDC0_DDR_EN_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MSDC0_DDR_EN2_MASK_B])) {
		pwrctrl->reg_msdc0_ddr_en2_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MSDC0_DDR_EN2_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_CONN_SRCCLKENB2PWRAP_MASK_B])) {
		pwrctrl->reg_conn_srcclkenb2pwrap_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_CONN_SRCCLKENB2PWRAP_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_CCIF_EVENT_MASK_B])) {
		pwrctrl->ccif_event_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_CCIF_EVENT_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_APU_CORE0_SRCCLKENA_MASK_B])) {
		pwrctrl->reg_apu_core0_srcclkena_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_APU_CORE0_SRCCLKENA_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_APU_CORE0_INFRA_REQ_MASK_B])) {
		pwrctrl->reg_apu_core0_infra_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_APU_CORE0_INFRA_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_APU_CORE0_APSRC_REQ_MASK_B])) {
		pwrctrl->reg_apu_core0_apsrc_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_APU_CORE0_APSRC_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_APU_CORE0_VRF18_REQ_MASK_B])) {
		pwrctrl->reg_apu_core0_vrf18_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_APU_CORE0_VRF18_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_APU_CORE0_DDR_EN_MASK_B])) {
		pwrctrl->reg_apu_core0_ddr_en_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_APU_CORE0_DDR_EN_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_APU_CORE1_SRCCLKENA_MASK_B])) {
		pwrctrl->reg_apu_core1_srcclkena_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_APU_CORE1_SRCCLKENA_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_APU_CORE1_INFRA_REQ_MASK_B])) {
		pwrctrl->reg_apu_core1_infra_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_APU_CORE1_INFRA_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_APU_CORE1_APSRC_REQ_MASK_B])) {
		pwrctrl->reg_apu_core1_apsrc_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_APU_CORE1_APSRC_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_APU_CORE1_VRF18_REQ_MASK_B])) {
		pwrctrl->reg_apu_core1_vrf18_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_APU_CORE1_VRF18_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_APU_CORE1_DDR_EN_MASK_B])) {
		pwrctrl->reg_apu_core1_ddr_en_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_APU_CORE1_DDR_EN_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_APU_CORE2_SRCCLKENA_MASK_B])) {
		pwrctrl->reg_apu_core2_srcclkena_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_APU_CORE2_SRCCLKENA_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_APU_CORE2_INFRA_REQ_MASK_B])) {
		pwrctrl->reg_apu_core2_infra_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_APU_CORE2_INFRA_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_APU_CORE2_APSRC_REQ_MASK_B])) {
		pwrctrl->reg_apu_core2_apsrc_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_APU_CORE2_APSRC_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_APU_CORE2_VRF18_REQ_MASK_B])) {
		pwrctrl->reg_apu_core2_vrf18_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_APU_CORE2_VRF18_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_APU_CORE2_DDR_EN_MASK_B])) {
		pwrctrl->reg_apu_core2_ddr_en_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_APU_CORE2_DDR_EN_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_APU_CORE2_DDR_EN2_MASK_B])) {
		pwrctrl->reg_apu_core2_ddr_en2_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_APU_CORE2_DDR_EN2_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MCUSYS_MERGE_APSRC_REQ_MASK_B])) {
		pwrctrl->reg_mcusys_merge_apsrc_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MCUSYS_MERGE_APSRC_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MCUSYS_MERGE_DDR_EN_MASK_B])) {
		pwrctrl->reg_mcusys_merge_ddr_en_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MCUSYS_MERGE_DDR_EN_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MCUSYS_MERGE_DDR_EN2_MASK_B])) {
		pwrctrl->reg_mcusys_merge_ddr_en2_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MCUSYS_MERGE_DDR_EN2_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_APU_CORE0_DDR_EN2_MASK_B])) {
		pwrctrl->reg_apu_core0_ddr_en2_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_APU_CORE0_DDR_EN2_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_APU_CORE1_DDR_EN2_MASK_B])) {
		pwrctrl->reg_apu_core1_ddr_en2_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_APU_CORE1_DDR_EN2_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_CG_CHECK_DDR_EN_MASK_B])) {
		pwrctrl->reg_cg_check_ddr_en_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_CG_CHECK_DDR_EN_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_CG_CHECK_DDR_EN2_MASK_B])) {
		pwrctrl->reg_cg_check_ddr_en2_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_CG_CHECK_DDR_EN2_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_WAKEUP_EVENT_MASK])) {
		pwrctrl->reg_wakeup_event_mask = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_WAKEUP_EVENT_MASK, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_EXT_WAKEUP_EVENT_MASK])) {
		pwrctrl->reg_ext_wakeup_event_mask = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_EXT_WAKEUP_EVENT_MASK, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MSDC1_SRCCLKENA_MASK_B])) {
		pwrctrl->reg_msdc1_srcclkena_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MSDC1_SRCCLKENA_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MSDC1_INFRA_REQ_MASK_B])) {
		pwrctrl->reg_msdc1_infra_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MSDC1_INFRA_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MSDC1_APSRC_REQ_MASK_B])) {
		pwrctrl->reg_msdc1_apsrc_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MSDC1_APSRC_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MSDC1_VRF18_REQ_MASK_B])) {
		pwrctrl->reg_msdc1_vrf18_req_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MSDC1_VRF18_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MSDC1_DDR_EN_MASK_B])) {
		pwrctrl->reg_msdc1_ddr_en_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MSDC1_DDR_EN_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MSDC1_DDR_EN2_MASK_B])) {
		pwrctrl->reg_msdc1_ddr_en2_mask_b = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MSDC1_DDR_EN2_MASK_B, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MSDC1_SRCCLKENA_ACK_MASK])) {
		pwrctrl->reg_msdc1_srcclkena_ack_mask = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MSDC1_SRCCLKENA_ACK_MASK, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MSDC1_INFRA_ACK_MASK])) {
		pwrctrl->reg_msdc1_infra_ack_mask = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MSDC1_INFRA_ACK_MASK, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MSDC1_APSRC_ACK_MASK])) {
		pwrctrl->reg_msdc1_apsrc_ack_mask = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MSDC1_APSRC_ACK_MASK, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MSDC1_VRF18_ACK_MASK])) {
		pwrctrl->reg_msdc1_vrf18_ack_mask = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MSDC1_VRF18_ACK_MASK, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MSDC1_DDR_EN_ACK_MASK])) {
		pwrctrl->reg_msdc1_ddr_en_ack_mask = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MSDC1_DDR_EN_ACK_MASK, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_MSDC1_DDR_EN2_ACK_MASK])) {
		pwrctrl->reg_msdc1_ddr_en2_ack_mask = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_MSDC1_DDR_EN2_ACK_MASK, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_MP0_CPU0_WFI_EN])) {
		pwrctrl->mp0_cpu0_wfi_en = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_MP0_CPU0_WFI_EN, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_MP0_CPU1_WFI_EN])) {
		pwrctrl->mp0_cpu1_wfi_en = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_MP0_CPU1_WFI_EN, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_MP0_CPU2_WFI_EN])) {
		pwrctrl->mp0_cpu2_wfi_en = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_MP0_CPU2_WFI_EN, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_MP0_CPU3_WFI_EN])) {
		pwrctrl->mp0_cpu3_wfi_en = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_MP0_CPU3_WFI_EN, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_MP0_CPU4_WFI_EN])) {
		pwrctrl->mp0_cpu4_wfi_en = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_MP0_CPU4_WFI_EN, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_MP0_CPU5_WFI_EN])) {
		pwrctrl->mp0_cpu5_wfi_en = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_MP0_CPU5_WFI_EN, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_MP0_CPU6_WFI_EN])) {
		pwrctrl->mp0_cpu6_wfi_en = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_MP0_CPU6_WFI_EN, val);
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_MP0_CPU7_WFI_EN])) {
		pwrctrl->mp0_cpu7_wfi_en = val;
		SMC_CALL(PWR_CTRL_ARGS,
			id, PW_MP0_CPU7_WFI_EN, val);
	} else {
		return -EINVAL;
	}

	return count;
}


static ssize_t suspend_ctrl_store(struct kobject *kobj,
	struct kobj_attribute *attr,
				  const char *buf, size_t count)
{
	return store_pwr_ctrl(SPM_PWR_CTRL_SUSPEND
		, &pwrctrl_suspend, buf, count);
}

static ssize_t IdleDram_ctrl_store(struct kobject *kobj,
	struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	return store_pwr_ctrl(SPM_PWR_CTRL_IDLE_DRAM
		, &pwrctrl_dram, buf, count);
}

static ssize_t IdleSyspll_ctrl_store(struct kobject *kobj,
	struct kobj_attribute *attr,
			       const char *buf, size_t count)
{
	return store_pwr_ctrl(SPM_PWR_CTRL_IDLE_SYSPLL
		, &pwrctrl_syspll, buf, count);
}

static ssize_t IdleBus26m_ctrl_store(struct kobject *kobj,
	struct kobj_attribute *attr,
			       const char *buf, size_t count)
{
	return store_pwr_ctrl(SPM_PWR_CTRL_IDLE_BUS26M
		, &pwrctrl_bus26m, buf, count);
}

static ssize_t vcore_dvfs_ctrl_store(struct kobject *kobj,
	struct kobj_attribute *attr,
				     const char *buf, size_t count)
{
#if 0  //FIXME
	return store_pwr_ctrl(SPM_PWR_CTRL_VCOREFS,
		__spm_vcorefs.pwrctrl, buf, count);
#else
	return 0;
#endif
}

static ssize_t spmfw_version_store(struct kobject *kobj,
	struct kobj_attribute *attr,
				     const char *buf, size_t count)
{
	return 0;
}

/**************************************
 * fm_suspend Function
 **************************************/
static ssize_t fm_suspend_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	char *p = buf;

	WARN_ON(p - buf >= get_mtk_lp_kernfs_bufsz_max());
	return p - buf;
}

/**************************************
 * Init Function
 **************************************/
DEFINE_ATTR_RW(suspend_ctrl);
DEFINE_ATTR_RW(IdleDram_ctrl);
DEFINE_ATTR_RW(IdleSyspll_ctrl);
DEFINE_ATTR_RW(IdleBus26m_ctrl);
DEFINE_ATTR_RW(vcore_dvfs_ctrl);
DEFINE_ATTR_RW(spmfw_version);
DEFINE_ATTR_RO(fm_suspend);

static struct attribute *spm_attrs[] = {
	/* for spm_lp_scen.pwrctrl */
	__ATTR_OF(suspend_ctrl),
	__ATTR_OF(IdleDram_ctrl),
	__ATTR_OF(IdleSyspll_ctrl),
	__ATTR_OF(IdleBus26m_ctrl),
	__ATTR_OF(vcore_dvfs_ctrl),
	__ATTR_OF(spmfw_version),
	__ATTR_OF(fm_suspend),

	/* must */
	NULL,
};

static struct attribute_group spm_attr_group = {
	.name = "spm",
	.attrs = spm_attrs,
};

int spm_fs_init(void)
{
	int r;

	/* create /sys/power/spm/xxx */
	r = mtk_idle_sysfs_power_create_group(&spm_attr_group);
	if (r)
		printk_deferred("[name:spm&][SPM] FAILED TO CREATE /sys/power/spm (%d)\n",
			r);

	return r;
}

MODULE_DESCRIPTION("SPM-FS Driver v0.1");
