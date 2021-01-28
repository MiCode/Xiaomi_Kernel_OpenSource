// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2017 MediaTek Inc.
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
	[PW_WDT_DISABLE] =
		"wdt_disable",
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
	int i, buf_len;
	int max_buf_len = get_mtk_lp_kernfs_bufsz_max();

	for (i = 0; i < PW_MAX_COUNT; i++) {
		buf_len = max_buf_len > (p - buf) ? max_buf_len - (p - buf) : 0;
		p += snprintf(p, buf_len, "%s = 0x%zx\n",
				pwr_ctrl_str[i],
				SMC_CALL(GET_PWR_CTRL_ARGS,
				id, i, 0));
	}

	WARN_ON(p - buf >= get_mtk_lp_kernfs_bufsz_max());

	return p - buf;
}

static ssize_t suspend_ctrl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return show_pwr_ctrl(SPM_PWR_CTRL_SUSPEND, __spm_suspend.pwrctrl, buf);
}

static ssize_t dpidle_ctrl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return show_pwr_ctrl(SPM_PWR_CTRL_DPIDLE, __spm_dpidle.pwrctrl, buf);
}

static ssize_t sodi3_ctrl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return show_pwr_ctrl(SPM_PWR_CTRL_SODI3, __spm_sodi3.pwrctrl, buf);
}

static ssize_t sodi_ctrl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return show_pwr_ctrl(SPM_PWR_CTRL_SODI, __spm_sodi.pwrctrl, buf);
}

static ssize_t vcore_dvfs_ctrl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return 0;
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
#define SET_SPM_PWR_CASE(idx, ctrl_reg, val) \
	{/* fallthrough */\
	case idx:\
	pwrctrl->ctrl_reg = val; }
static ssize_t store_pwr_ctrl(int id,
			struct pwr_ctrl *pwrctrl,
			const char *buf, size_t count)
{
	u32 val;
	char cmd[64];
	int i;

	if (sscanf(buf, "%63s %x", cmd, &val) != 2)
		return -EPERM;

	pr_info("[SPM] pwr_ctrl: cmd = %s, val = 0x%x", cmd, val);

	if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SPM_APSRC_REQ])) {
		unsigned int req = (val == 0) ?
			SPM_RESOURCE_CONSOLE_RELEASE : SPM_RESOURCE_CONSOLE_REQ;
		if (spm_resource_req_console_by_id(id, req
			, _RES_MASK(MTK_SPM_RES_EX_DRAM_S1)) != 0) {
			pwrctrl->reg_spm_apsrc_req = val;
			SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SPM_APSRC_REQ, val);
		}
		return count;
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
		return count;
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
		return count;
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
		return count;
	} else if (!strcmp(cmd,
		pwr_ctrl_str[PW_REG_SPM_DDR_EN_REQ])) {
		unsigned int req = (val == 0) ?
			SPM_RESOURCE_CONSOLE_RELEASE : SPM_RESOURCE_CONSOLE_REQ;
		if (spm_resource_req_console_by_id(id, req
			, _RES_MASK(MTK_SPM_RES_EX_DRAM_S0)) != 0) {
			pwrctrl->reg_spm_ddr_en_req = val;
			SMC_CALL(PWR_CTRL_ARGS,
			id, PW_REG_SPM_DDR_EN_REQ, val);
		}
		return count;
	}

	for (i = 0 ; i < PW_MAX_COUNT ; i++) {
		if (!strcmp(cmd,
			pwr_ctrl_str[i])) {
			SMC_CALL(PWR_CTRL_ARGS, id, i, val);
			break;
		}
	}

	switch (i) {
		/* for SPM */
		SET_SPM_PWR_CASE(0, pcm_flags, val);
		break;
		SET_SPM_PWR_CASE(1, pcm_flags_cust, val);
		break;
		SET_SPM_PWR_CASE(2, pcm_flags_cust_set, val);
		break;
		SET_SPM_PWR_CASE(3, pcm_flags_cust_clr, val);
		break;
		SET_SPM_PWR_CASE(4, pcm_flags1, val);
		break;
		SET_SPM_PWR_CASE(5, pcm_flags1_cust, val);
		break;
		SET_SPM_PWR_CASE(6, pcm_flags1_cust_set, val);
		break;
		SET_SPM_PWR_CASE(7, pcm_flags1_cust_clr, val);
		break;
		SET_SPM_PWR_CASE(8, timer_val, val);
		break;
		SET_SPM_PWR_CASE(9, timer_val_cust, val);
		break;
		SET_SPM_PWR_CASE(10, timer_val_ramp_en, val);
		break;
		SET_SPM_PWR_CASE(11, timer_val_ramp_en_sec, val);
		break;
		SET_SPM_PWR_CASE(12, wake_src, val);
		break;
		SET_SPM_PWR_CASE(13, wake_src_cust, val);
		break;
		SET_SPM_PWR_CASE(14, wakelock_timer_val, val);
		break;
		SET_SPM_PWR_CASE(15, wdt_disable, val);
		break;
		SET_SPM_PWR_CASE(16, reg_srcclken0_ctl, val);
		break;
		SET_SPM_PWR_CASE(17, reg_srcclken1_ctl, val);
		break;
		SET_SPM_PWR_CASE(18, reg_spm_lock_infra_dcm, val);
		break;
		SET_SPM_PWR_CASE(19, reg_srcclken_mask, val);
		break;
		SET_SPM_PWR_CASE(20, reg_md1_c32rm_en, val);
		break;
		SET_SPM_PWR_CASE(21, reg_md2_c32rm_en, val);
		break;
		SET_SPM_PWR_CASE(22, reg_clksq0_sel_ctrl, val);
		break;
		SET_SPM_PWR_CASE(23, reg_clksq1_sel_ctrl, val);
		break;
		SET_SPM_PWR_CASE(24, reg_srcclken0_en, val);
		break;
		SET_SPM_PWR_CASE(25, reg_srcclken1_en, val);
		break;
		SET_SPM_PWR_CASE(26, reg_sysclk0_src_mask_b, val);
		break;
		SET_SPM_PWR_CASE(27, reg_sysclk1_src_mask_b, val);
		break;
		SET_SPM_PWR_CASE(28, reg_wfi_op, val);
		break;
		SET_SPM_PWR_CASE(29, reg_wfi_type, val);
		break;
		SET_SPM_PWR_CASE(30, reg_mp0_cputop_idle_mask, val);
		break;
		SET_SPM_PWR_CASE(31, reg_mp1_cputop_idle_mask, val);
		break;
		SET_SPM_PWR_CASE(32, reg_mcusys_idle_mask, val);
		break;
		SET_SPM_PWR_CASE(33, reg_md_apsrc_1_sel, val);
		break;
		SET_SPM_PWR_CASE(34, reg_md_apsrc_0_sel, val);
		break;
		SET_SPM_PWR_CASE(35, reg_conn_apsrc_sel, val);
		break;
		SET_SPM_PWR_CASE(36, reg_spm_apsrc_req, val);
		break;
		SET_SPM_PWR_CASE(37, reg_spm_f26m_req, val);
		break;
		SET_SPM_PWR_CASE(38, reg_spm_infra_req, val);
		break;
		SET_SPM_PWR_CASE(39, reg_spm_vrf18_req, val);
		break;
		SET_SPM_PWR_CASE(40, reg_spm_ddr_en_req, val);
		break;
		SET_SPM_PWR_CASE(41, reg_spm_ddr_en2_req, val);
		break;
		SET_SPM_PWR_CASE(42, reg_spm_dvfs_req, val);
		break;
		SET_SPM_PWR_CASE(43, reg_spm_sw_mailbox_req, val);
		break;
		SET_SPM_PWR_CASE(44, reg_spm_sspm_mailbox_req, val);
		break;
		SET_SPM_PWR_CASE(45, reg_spm_adsp_mailbox_req, val);
		break;
		SET_SPM_PWR_CASE(46, reg_spm_scp_mailbox_req, val);
		break;
		SET_SPM_PWR_CASE(47, reg_spm_mcusys_pwr_event_req, val);
		break;
		SET_SPM_PWR_CASE(48, cpu_md_dvfs_sop_force_on, val);
		break;
		SET_SPM_PWR_CASE(49, reg_md_srcclkena_0_mask_b, val);
		break;
		SET_SPM_PWR_CASE(50, reg_md_srcclkena2infra_req_0_mask_b, val);
		break;
		SET_SPM_PWR_CASE(51, reg_md_apsrc2infra_req_0_mask_b, val);
		break;
		SET_SPM_PWR_CASE(52, reg_md_apsrc_req_0_mask_b, val);
		break;
		SET_SPM_PWR_CASE(53, reg_md_vrf18_req_0_mask_b, val);
		break;
		SET_SPM_PWR_CASE(54, reg_md_ddr_en_0_mask_b, val);
		break;
		SET_SPM_PWR_CASE(55, reg_md_ddr_en2_0_mask_b, val);
		break;
		SET_SPM_PWR_CASE(56, reg_md_srcclkena_1_mask_b, val);
		break;
		SET_SPM_PWR_CASE(57, reg_md_srcclkena2infra_req_1_mask_b, val);
		break;
		SET_SPM_PWR_CASE(58, reg_md_apsrc2infra_req_1_mask_b, val);
		break;
		SET_SPM_PWR_CASE(59, reg_md_apsrc_req_1_mask_b, val);
		break;
		SET_SPM_PWR_CASE(60, reg_md_vrf18_req_1_mask_b, val);
		break;
		SET_SPM_PWR_CASE(61, reg_md_ddr_en_1_mask_b, val);
		break;
		SET_SPM_PWR_CASE(62, reg_md_ddr_en2_1_mask_b, val);
		break;
		SET_SPM_PWR_CASE(63, reg_conn_srcclkena_mask_b, val);
		break;
		SET_SPM_PWR_CASE(64, reg_conn_srcclkenb_mask_b, val);
		break;
		SET_SPM_PWR_CASE(65, reg_conn_infra_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(66, reg_conn_apsrc_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(67, reg_conn_vrf18_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(68, reg_conn_ddr_en_mask_b, val);
		break;
		SET_SPM_PWR_CASE(69, reg_conn_ddr_en2_mask_b, val);
		break;
		SET_SPM_PWR_CASE(70, reg_srcclkeni0_srcclkena_mask_b, val);
		break;
		SET_SPM_PWR_CASE(71, reg_srcclkeni0_infra_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(72, reg_srcclkeni1_srcclkena_mask_b, val);
		break;
		SET_SPM_PWR_CASE(73, reg_srcclkeni1_infra_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(74, reg_srcclkeni2_srcclkena_mask_b, val);
		break;
		SET_SPM_PWR_CASE(75, reg_srcclkeni2_infra_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(76, reg_infrasys_apsrc_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(77, reg_infrasys_ddr_en_mask_b, val);
		break;
		SET_SPM_PWR_CASE(78, reg_infrasys_ddr_en2_mask_b, val);
		break;
		SET_SPM_PWR_CASE(79, reg_md32_srcclkena_mask_b, val);
		break;
		SET_SPM_PWR_CASE(80, reg_conn_vfe28_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(81, reg_md32_infra_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(82, reg_md32_apsrc_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(83, reg_md32_vrf18_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(84, reg_md32_ddr_en_mask_b, val);
		break;
		SET_SPM_PWR_CASE(85, reg_md32_ddr_en2_mask_b, val);
		break;
		SET_SPM_PWR_CASE(86, reg_scp_srcclkena_mask_b, val);
		break;
		SET_SPM_PWR_CASE(87, reg_scp_infra_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(88, reg_scp_apsrc_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(89, reg_scp_vrf18_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(90, reg_scp_ddr_en_mask_b, val);
		break;
		SET_SPM_PWR_CASE(91, reg_scp_ddr_en2_mask_b, val);
		break;
		SET_SPM_PWR_CASE(92, reg_ufs_srcclkena_mask_b, val);
		break;
		SET_SPM_PWR_CASE(93, reg_ufs_infra_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(94, reg_ufs_apsrc_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(95, reg_ufs_vrf18_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(96, reg_ufs_ddr_en_mask_b, val);
		break;
		SET_SPM_PWR_CASE(97, reg_ufs_ddr_en2_mask_b, val);
		break;
		SET_SPM_PWR_CASE(98, reg_disp0_apsrc_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(99, reg_disp0_ddr_en_mask_b, val);
		break;
		SET_SPM_PWR_CASE(100, reg_disp0_ddr_en2_mask_b, val);
		break;
		SET_SPM_PWR_CASE(101, reg_disp1_apsrc_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(102, reg_disp1_ddr_en_mask_b, val);
		break;
		SET_SPM_PWR_CASE(103, reg_disp1_ddr_en2_mask_b, val);
		break;
		SET_SPM_PWR_CASE(104, reg_gce_infra_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(105, reg_gce_apsrc_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(106, reg_gce_vrf18_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(107, reg_gce_ddr_en_mask_b, val);
		break;
		SET_SPM_PWR_CASE(108, reg_gce_ddr_en2_mask_b, val);
		break;
		SET_SPM_PWR_CASE(109, reg_emi_ch0_ddr_en_mask_b, val);
		break;
		SET_SPM_PWR_CASE(110, reg_emi_ch1_ddr_en_mask_b, val);
		break;
		SET_SPM_PWR_CASE(111, reg_emi_ch0_ddr_en2_mask_b, val);
		break;
		SET_SPM_PWR_CASE(112, reg_emi_ch1_ddr_en2_mask_b, val);
		break;
		SET_SPM_PWR_CASE(113, reg_dvfsrc_event_trigger_mask_b, val);
		break;
		SET_SPM_PWR_CASE(114, reg_sw2spm_int0_mask_b, val);
		break;
		SET_SPM_PWR_CASE(115, reg_sw2spm_int1_mask_b, val);
		break;
		SET_SPM_PWR_CASE(116, reg_sw2spm_int2_mask_b, val);
		break;
		SET_SPM_PWR_CASE(117, reg_sw2spm_int3_mask_b, val);
		break;
		SET_SPM_PWR_CASE(118, reg_sc_adsp2spm_wakeup_mask_b, val);
		break;
		SET_SPM_PWR_CASE(119, reg_sc_sspm2spm_wakeup_mask_b, val);
		break;
		SET_SPM_PWR_CASE(120, reg_sc_scp2spm_wakeup_mask_b, val);
		break;
		SET_SPM_PWR_CASE(121, reg_csyspwrreq_mask, val);
		break;
		SET_SPM_PWR_CASE(122, reg_spm_srcclkena_reserved_mask_b, val);
		break;
		SET_SPM_PWR_CASE(123, reg_spm_infra_req_reserved_mask_b, val);
		break;
		SET_SPM_PWR_CASE(124, reg_spm_apsrc_req_reserved_mask_b, val);
		break;
		SET_SPM_PWR_CASE(125, reg_spm_vrf18_req_reserved_mask_b, val);
		break;
		SET_SPM_PWR_CASE(126, reg_spm_ddr_en_reserved_mask_b, val);
		break;
		SET_SPM_PWR_CASE(127, reg_spm_ddr_en2_reserved_mask_b, val);
		break;
		SET_SPM_PWR_CASE(128, reg_audio_dsp_srcclkena_mask_b, val);
		break;
		SET_SPM_PWR_CASE(129, reg_audio_dsp_infra_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(130, reg_audio_dsp_apsrc_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(131, reg_audio_dsp_vrf18_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(132, reg_audio_dsp_ddr_en_mask_b, val);
		break;
		SET_SPM_PWR_CASE(133, reg_audio_dsp_ddr_en2_mask_b, val);
		break;
		SET_SPM_PWR_CASE(134, reg_mcusys_pwr_event_mask_b, val);
		break;
		SET_SPM_PWR_CASE(135, reg_msdc0_srcclkena_mask_b, val);
		break;
		SET_SPM_PWR_CASE(136, reg_msdc0_infra_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(137, reg_msdc0_apsrc_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(138, reg_msdc0_vrf18_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(139, reg_msdc0_ddr_en_mask_b, val);
		break;
		SET_SPM_PWR_CASE(140, reg_msdc0_ddr_en2_mask_b, val);
		break;
		SET_SPM_PWR_CASE(141, reg_conn_srcclkenb2pwrap_mask_b, val);
		break;
		SET_SPM_PWR_CASE(142, ccif_event_mask_b, val);
		break;
		SET_SPM_PWR_CASE(143, reg_apu_core0_srcclkena_mask_b, val);
		break;
		SET_SPM_PWR_CASE(144, reg_apu_core0_infra_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(145, reg_apu_core0_apsrc_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(146, reg_apu_core0_vrf18_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(147, reg_apu_core0_ddr_en_mask_b, val);
		break;
		SET_SPM_PWR_CASE(148, reg_apu_core1_srcclkena_mask_b, val);
		break;
		SET_SPM_PWR_CASE(149, reg_apu_core1_infra_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(150, reg_apu_core1_apsrc_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(151, reg_apu_core1_vrf18_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(152, reg_apu_core1_ddr_en_mask_b, val);
		break;
		SET_SPM_PWR_CASE(153, reg_apu_core2_srcclkena_mask_b, val);
		break;
		SET_SPM_PWR_CASE(154, reg_apu_core2_infra_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(155, reg_apu_core2_apsrc_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(156, reg_apu_core2_vrf18_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(157, reg_apu_core2_ddr_en_mask_b, val);
		break;
		SET_SPM_PWR_CASE(158, reg_apu_core2_ddr_en2_mask_b, val);
		break;
		SET_SPM_PWR_CASE(159, reg_mcusys_merge_apsrc_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(160, reg_mcusys_merge_ddr_en_mask_b, val);
		break;
		SET_SPM_PWR_CASE(161, reg_mcusys_merge_ddr_en2_mask_b, val);
		break;
		SET_SPM_PWR_CASE(162, reg_apu_core0_ddr_en2_mask_b, val);
		break;
		SET_SPM_PWR_CASE(163, reg_apu_core1_ddr_en2_mask_b, val);
		break;
		SET_SPM_PWR_CASE(164, reg_cg_check_ddr_en_mask_b, val);
		break;
		SET_SPM_PWR_CASE(165, reg_cg_check_ddr_en2_mask_b, val);
		break;
		SET_SPM_PWR_CASE(166, reg_wakeup_event_mask, val);
		break;
		SET_SPM_PWR_CASE(167, reg_ext_wakeup_event_mask, val);
		break;
		SET_SPM_PWR_CASE(168, reg_msdc1_srcclkena_mask_b, val);
		break;
		SET_SPM_PWR_CASE(169, reg_msdc1_infra_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(170, reg_msdc1_apsrc_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(171, reg_msdc1_vrf18_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(172, reg_msdc1_ddr_en_mask_b, val);
		break;
		SET_SPM_PWR_CASE(173, reg_msdc1_ddr_en2_mask_b, val);
		break;
		SET_SPM_PWR_CASE(174, reg_msdc1_srcclkena_ack_mask, val);
		break;
		SET_SPM_PWR_CASE(175, reg_msdc1_infra_ack_mask, val);
		break;
		SET_SPM_PWR_CASE(176, reg_msdc1_apsrc_ack_mask, val);
		break;
		SET_SPM_PWR_CASE(177, reg_msdc1_vrf18_ack_mask, val);
		break;
		SET_SPM_PWR_CASE(178, reg_msdc1_ddr_en_ack_mask, val);
		break;
		SET_SPM_PWR_CASE(179, reg_msdc1_ddr_en2_ack_mask, val);
		break;
		SET_SPM_PWR_CASE(180, mp0_cpu0_wfi_en, val);
		break;
		SET_SPM_PWR_CASE(181, mp0_cpu1_wfi_en, val);
		break;
		SET_SPM_PWR_CASE(182, mp0_cpu2_wfi_en, val);
		break;
		SET_SPM_PWR_CASE(183, mp0_cpu3_wfi_en, val);
		break;
		SET_SPM_PWR_CASE(184, mp0_cpu4_wfi_en, val);
		break;
		SET_SPM_PWR_CASE(185, mp0_cpu5_wfi_en, val);
		break;
		SET_SPM_PWR_CASE(186, mp0_cpu6_wfi_en, val);
		break;
		SET_SPM_PWR_CASE(187, mp0_cpu7_wfi_en, val);
		break;
	/* fallthrough */
	default:
		break;
	}

	return count;
}


static ssize_t suspend_ctrl_store(struct kobject *kobj,
	struct kobj_attribute *attr,
				  const char *buf, size_t count)
{
	return store_pwr_ctrl(SPM_PWR_CTRL_SUSPEND, __spm_suspend.pwrctrl,
		buf, count);
}

static ssize_t dpidle_ctrl_store(struct kobject *kobj,
	struct kobj_attribute *attr,
				 const char *buf, size_t count)
{
	return store_pwr_ctrl(SPM_PWR_CTRL_DPIDLE, &pwrctrl_dp, buf, count);
}

static ssize_t sodi3_ctrl_store(struct kobject *kobj,
	struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	return store_pwr_ctrl(SPM_PWR_CTRL_SODI3, &pwrctrl_so3, buf, count);
}

static ssize_t sodi_ctrl_store(struct kobject *kobj,
	struct kobj_attribute *attr,
			       const char *buf, size_t count)
{
	return store_pwr_ctrl(SPM_PWR_CTRL_SODI, &pwrctrl_so, buf, count);
}

static ssize_t vcore_dvfs_ctrl_store(struct kobject *kobj,
	struct kobj_attribute *attr,
				     const char *buf, size_t count)
{
	return 0;
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
DEFINE_ATTR_RW(dpidle_ctrl);
DEFINE_ATTR_RW(sodi3_ctrl);
DEFINE_ATTR_RW(sodi_ctrl);
DEFINE_ATTR_RW(vcore_dvfs_ctrl);
DEFINE_ATTR_RW(spmfw_version);
DEFINE_ATTR_RO(fm_suspend);

static struct attribute *spm_attrs[] = {
	/* for spm_lp_scen.pwrctrl */
	__ATTR_OF(suspend_ctrl),
	__ATTR_OF(dpidle_ctrl),
	__ATTR_OF(sodi3_ctrl),
	__ATTR_OF(sodi_ctrl),
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
		pr_info("[SPM] FAILED TO CREATE /sys/power/spm (%d)\n", r);

	return r;
}

MODULE_DESCRIPTION("SPM-FS Driver v0.1");
