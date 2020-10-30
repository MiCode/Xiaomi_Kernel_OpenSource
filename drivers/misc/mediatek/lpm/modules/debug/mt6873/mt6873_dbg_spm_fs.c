// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/spinlock.h>

#include <lpm_dbg_common_v1.h>
#include <lpm_module.h>
#include <mtk_idle_sysfs.h>
#include <mtk_suspend_sysfs.h>
#include <mtk_spm_sysfs.h>

#include <mt6873_pwr_ctrl.h>
#include <lpm_dbg_fs_common.h>
#include <lpm_spm_comm.h>

/* Determine for node route */
#define MT_LP_RQ_NODE	"/proc/mtk_lpm/spm/spm_resource_req"

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

#undef mtk_dbg_spm_log
#define mtk_dbg_spm_log(fmt, args...) \
	do { \
		int l = scnprintf(p, sz, fmt, ##args); \
		p += l; \
		sz -= l; \
	} while (0)


static char *mt6873_pwr_ctrl_str[PW_MAX_COUNT] = {
	[PW_PCM_FLAGS] = "pcm_flags",
	[PW_PCM_FLAGS_CUST] = "pcm_flags_cust",
	[PW_PCM_FLAGS_CUST_SET] = "pcm_flags_cust_set",
	[PW_PCM_FLAGS_CUST_CLR] = "pcm_flags_cust_clr",
	[PW_PCM_FLAGS1] = "pcm_flags1",
	[PW_PCM_FLAGS1_CUST] = "pcm_flags1_cust",
	[PW_PCM_FLAGS1_CUST_SET] = "pcm_flags1_cust_set",
	[PW_PCM_FLAGS1_CUST_CLR] = "pcm_flags1_cust_clr",
	[PW_TIMER_VAL] = "timer_val",
	[PW_TIMER_VAL_CUST] = "timer_val_cust",
	[PW_TIMER_VAL_RAMP_EN] = "timer_val_ramp_en",
	[PW_TIMER_VAL_RAMP_EN_SEC] = "timer_val_ramp_en_sec",
	[PW_WAKE_SRC] = "wake_src",
	[PW_WAKE_SRC_CUST] = "wake_src_cust",
	[PW_WAKELOCK_TIMER_VAL] = "wakelock_timer_val",
	[PW_WDT_DISABLE] = "wdt_disable",
	/* SPM_CLK_CON */
	[PW_REG_SRCCLKEN0_CTL] = "reg_srcclken0_ctl",
	[PW_REG_SRCCLKEN1_CTL] = "reg_srcclken1_ctl",
	[PW_REG_SPM_LOCK_INFRA_DCM] = "reg_spm_lock_infra_dcm",
	[PW_REG_SRCCLKEN_MASK] = "reg_srcclken_mask",
	[PW_REG_MD1_C32RM_EN] = "reg_md1_c32rm_en",
	[PW_REG_MD2_C32RM_EN] = "reg_md2_c32rm_en",
	[PW_REG_CLKSQ0_SEL_CTRL] = "reg_clksq0_sel_ctrl",
	[PW_REG_CLKSQ1_SEL_CTRL] = "reg_clksq1_sel_ctrl",
	[PW_REG_SRCCLKEN0_EN] = "reg_srcclken0_en",
	[PW_REG_SRCCLKEN1_EN] = "reg_srcclken1_en",
	[PW_REG_SYSCLK0_SRC_MASK_B] = "reg_sysclk0_src_mask_b",
	[PW_REG_SYSCLK1_SRC_MASK_B] = "reg_sysclk1_src_mask_b",
	/* SPM_AP_STANDBY_CON */
	[PW_REG_WFI_OP] = "reg_wfi_op",
	[PW_REG_WFI_TYPE] = "reg_wfi_type",
	[PW_REG_MP0_CPUTOP_IDLE_MASK] = "reg_mp0_cputop_idle_mask",
	[PW_REG_MP1_CPUTOP_IDLE_MASK] = "reg_mp1_cputop_idle_mask",
	[PW_REG_MCUSYS_IDLE_MASK] = "reg_mcusys_idle_mask",
	[PW_REG_MD_APSRC_1_SEL] = "reg_md_apsrc_1_sel",
	[PW_REG_MD_APSRC_0_SEL] = "reg_md_apsrc_0_sel",
	[PW_REG_CONN_APSRC_SEL] = "reg_conn_apsrc_sel",
	/* SPM_SRC_REQ */
	[PW_REG_SPM_APSRC_REQ] = "reg_spm_apsrc_req",
	[PW_REG_SPM_F26M_REQ] = "reg_spm_f26m_req",
	[PW_REG_SPM_INFRA_REQ] = "reg_spm_infra_req",
	[PW_REG_SPM_VRF18_REQ] = "reg_spm_vrf18_req",
	[PW_REG_SPM_DDR_EN_REQ] = "reg_spm_ddr_en_req",
	[PW_REG_SPM_DVFS_REQ] = "reg_spm_dvfs_req",
	[PW_REG_SPM_SW_MAILBOX_REQ] = "reg_spm_sw_mailbox_req",
	[PW_REG_SPM_SSPM_MAILBOX_REQ] = "reg_spm_sspm_mailbox_req",
	[PW_REG_SPM_ADSP_MAILBOX_REQ] = "reg_spm_adsp_mailbox_req",
	[PW_REG_SPM_SCP_MAILBOX_REQ] = "reg_spm_scp_mailbox_req",
	/* SPM_SRC_MASK */
	[PW_REG_MD_SRCCLKENA_0_MASK_B] = "reg_md_srcclkena_0_mask_b",
	[PW_REG_MD_SRCCLKENA2INFRA_REQ_0_MASK_B] =
	"reg_md_srcclkena2infra_req_0_mask_b",
	[PW_REG_MD_APSRC2INFRA_REQ_0_MASK_B] =
	"reg_md_apsrc2infra_req_0_mask_b",
	[PW_REG_MD_APSRC_REQ_0_MASK_B] = "reg_md_apsrc_req_0_mask_b",
	[PW_REG_MD_VRF18_REQ_0_MASK_B] = "reg_md_vrf18_req_0_mask_b",
	[PW_REG_MD_DDR_EN_0_MASK_B] = "reg_md_ddr_en_0_mask_b",
	[PW_REG_MD_SRCCLKENA_1_MASK_B] = "reg_md_srcclkena_1_mask_b",
	[PW_REG_MD_SRCCLKENA2INFRA_REQ_1_MASK_B] =
	"reg_md_srcclkena2infra_req_1_mask_b",
	[PW_REG_MD_APSRC2INFRA_REQ_1_MASK_B] =
	"reg_md_apsrc2infra_req_1_mask_b",
	[PW_REG_MD_APSRC_REQ_1_MASK_B] = "reg_md_apsrc_req_1_mask_b",
	[PW_REG_MD_VRF18_REQ_1_MASK_B] = "reg_md_vrf18_req_1_mask_b",
	[PW_REG_MD_DDR_EN_1_MASK_B] = "reg_md_ddr_en_1_mask_b",
	[PW_REG_CONN_SRCCLKENA_MASK_B] = "reg_conn_srcclkena_mask_b",
	[PW_REG_CONN_SRCCLKENB_MASK_B] = "reg_conn_srcclkenb_mask_b",
	[PW_REG_CONN_INFRA_REQ_MASK_B] = "reg_conn_infra_req_mask_b",
	[PW_REG_CONN_APSRC_REQ_MASK_B] = "reg_conn_apsrc_req_mask_b",
	[PW_REG_CONN_VRF18_REQ_MASK_B] = "reg_conn_vrf18_req_mask_b",
	[PW_REG_CONN_DDR_EN_MASK_B] = "reg_conn_ddr_en_mask_b",
	[PW_REG_CONN_VFE28_MASK_B] = "reg_conn_vfe28_mask_b",
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
	[PW_REG_INFRASYS_APSRC_REQ_MASK_B] = "reg_infrasys_apsrc_req_mask_b",
	[PW_REG_INFRASYS_DDR_EN_MASK_B] = "reg_infrasys_ddr_en_mask_b",
	[PW_REG_MD32_SRCCLKENA_MASK_B] = "reg_md32_srcclkena_mask_b",
	[PW_REG_MD32_INFRA_REQ_MASK_B] = "reg_md32_infra_req_mask_b",
	[PW_REG_MD32_APSRC_REQ_MASK_B] = "reg_md32_apsrc_req_mask_b",
	[PW_REG_MD32_VRF18_REQ_MASK_B] = "reg_md32_vrf18_req_mask_b",
	[PW_REG_MD32_DDR_EN_MASK_B] = "reg_md32_ddr_en_mask_b",
	/* SPM_SRC2_MASK */
	[PW_REG_SCP_SRCCLKENA_MASK_B] = "reg_scp_srcclkena_mask_b",
	[PW_REG_SCP_INFRA_REQ_MASK_B] = "reg_scp_infra_req_mask_b",
	[PW_REG_SCP_APSRC_REQ_MASK_B] = "reg_scp_apsrc_req_mask_b",
	[PW_REG_SCP_VRF18_REQ_MASK_B] = "reg_scp_vrf18_req_mask_b",
	[PW_REG_SCP_DDR_EN_MASK_B] = "reg_scp_ddr_en_mask_b",
	[PW_REG_AUDIO_DSP_SRCCLKENA_MASK_B] = "reg_audio_dsp_srcclkena_mask_b",
	[PW_REG_AUDIO_DSP_INFRA_REQ_MASK_B] = "reg_audio_dsp_infra_req_mask_b",
	[PW_REG_AUDIO_DSP_APSRC_REQ_MASK_B] = "reg_audio_dsp_apsrc_req_mask_b",
	[PW_REG_AUDIO_DSP_VRF18_REQ_MASK_B] = "reg_audio_dsp_vrf18_req_mask_b",
	[PW_REG_AUDIO_DSP_DDR_EN_MASK_B] = "reg_audio_dsp_ddr_en_mask_b",
	[PW_REG_UFS_SRCCLKENA_MASK_B] = "reg_ufs_srcclkena_mask_b",
	[PW_REG_UFS_INFRA_REQ_MASK_B] = "reg_ufs_infra_req_mask_b",
	[PW_REG_UFS_APSRC_REQ_MASK_B] = "reg_ufs_apsrc_req_mask_b",
	[PW_REG_UFS_VRF18_REQ_MASK_B] = "reg_ufs_vrf18_req_mask_b",
	[PW_REG_UFS_DDR_EN_MASK_B] = "reg_ufs_ddr_en_mask_b",
	[PW_REG_DISP0_APSRC_REQ_MASK_B] = "reg_disp0_apsrc_req_mask_b",
	[PW_REG_DISP0_DDR_EN_MASK_B] = "reg_disp0_ddr_en_mask_b",
	[PW_REG_DISP1_APSRC_REQ_MASK_B] = "reg_disp1_apsrc_req_mask_b",
	[PW_REG_DISP1_DDR_EN_MASK_B] = "reg_disp1_ddr_en_mask_b",
	[PW_REG_GCE_INFRA_REQ_MASK_B] = "reg_gce_infra_req_mask_b",
	[PW_REG_GCE_APSRC_REQ_MASK_B] = "reg_gce_apsrc_req_mask_b",
	[PW_REG_GCE_VRF18_REQ_MASK_B] = "reg_gce_vrf18_req_mask_b",
	[PW_REG_GCE_DDR_EN_MASK_B] = "reg_gce_ddr_en_mask_b",
	[PW_REG_APU_SRCCLKENA_MASK_B] = "reg_apu_srcclkena_mask_b",
	[PW_REG_APU_INFRA_REQ_MASK_B] = "reg_apu_infra_req_mask_b",
	[PW_REG_APU_APSRC_REQ_MASK_B] = "reg_apu_apsrc_req_mask_b",
	[PW_REG_APU_VRF18_REQ_MASK_B] = "reg_apu_vrf18_req_mask_b",
	[PW_REG_APU_DDR_EN_MASK_B] = "reg_apu_ddr_en_mask_b",
	[PW_REG_CG_CHECK_SRCCLKENA_MASK_B] = "reg_cg_check_srcclkena_mask_b",
	[PW_REG_CG_CHECK_APSRC_REQ_MASK_B] = "reg_cg_check_apsrc_req_mask_b",
	[PW_REG_CG_CHECK_VRF18_REQ_MASK_B] = "reg_cg_check_vrf18_req_mask_b",
	[PW_REG_CG_CHECK_DDR_EN_MASK_B] = "reg_cg_check_ddr_en_mask_b",
	/* SPM_SRC3_MASK */
	[PW_REG_DVFSRC_EVENT_TRIGGER_MASK_B] =
	"reg_dvfsrc_event_trigger_mask_b",
	[PW_REG_SW2SPM_INT0_MASK_B] = "reg_sw2spm_int0_mask_b",
	[PW_REG_SW2SPM_INT1_MASK_B] = "reg_sw2spm_int1_mask_b",
	[PW_REG_SW2SPM_INT2_MASK_B] = "reg_sw2spm_int2_mask_b",
	[PW_REG_SW2SPM_INT3_MASK_B] = "reg_sw2spm_int3_mask_b",
	[PW_REG_SC_ADSP2SPM_WAKEUP_MASK_B] = "reg_sc_adsp2spm_wakeup_mask_b",
	[PW_REG_SC_SSPM2SPM_WAKEUP_MASK_B] = "reg_sc_sspm2spm_wakeup_mask_b",
	[PW_REG_SC_SCP2SPM_WAKEUP_MASK_B] = "reg_sc_scp2spm_wakeup_mask_b",
	[PW_REG_CSYSPWRREQ_MASK] = "reg_csyspwrreq_mask",
	[PW_REG_SPM_SRCCLKENA_RESERVED_MASK_B] =
	"reg_spm_srcclkena_reserved_mask_b",
	[PW_REG_SPM_INFRA_REQ_RESERVED_MASK_B] =
	"reg_spm_infra_req_reserved_mask_b",
	[PW_REG_SPM_APSRC_REQ_RESERVED_MASK_B] =
	"reg_spm_apsrc_req_reserved_mask_b",
	[PW_REG_SPM_VRF18_REQ_RESERVED_MASK_B] =
	"reg_spm_vrf18_req_reserved_mask_b",
	[PW_REG_SPM_DDR_EN_RESERVED_MASK_B] = "reg_spm_ddr_en_reserved_mask_b",
	[PW_REG_MCUPM_SRCCLKENA_MASK_B] = "reg_mcupm_srcclkena_mask_b",
	[PW_REG_MCUPM_INFRA_REQ_MASK_B] = "reg_mcupm_infra_req_mask_b",
	[PW_REG_MCUPM_APSRC_REQ_MASK_B] = "reg_mcupm_apsrc_req_mask_b",
	[PW_REG_MCUPM_VRF18_REQ_MASK_B] = "reg_mcupm_vrf18_req_mask_b",
	[PW_REG_MCUPM_DDR_EN_MASK_B] = "reg_mcupm_ddr_en_mask_b",
	[PW_REG_MSDC0_SRCCLKENA_MASK_B] = "reg_msdc0_srcclkena_mask_b",
	[PW_REG_MSDC0_INFRA_REQ_MASK_B] = "reg_msdc0_infra_req_mask_b",
	[PW_REG_MSDC0_APSRC_REQ_MASK_B] = "reg_msdc0_apsrc_req_mask_b",
	[PW_REG_MSDC0_VRF18_REQ_MASK_B] = "reg_msdc0_vrf18_req_mask_b",
	[PW_REG_MSDC0_DDR_EN_MASK_B] = "reg_msdc0_ddr_en_mask_b",
	[PW_REG_MSDC1_SRCCLKENA_MASK_B] = "reg_msdc1_srcclkena_mask_b",
	[PW_REG_MSDC1_INFRA_REQ_MASK_B] = "reg_msdc1_infra_req_mask_b",
	[PW_REG_MSDC1_APSRC_REQ_MASK_B] = "reg_msdc1_apsrc_req_mask_b",
	[PW_REG_MSDC1_VRF18_REQ_MASK_B] = "reg_msdc1_vrf18_req_mask_b",
	[PW_REG_MSDC1_DDR_EN_MASK_B] = "reg_msdc1_ddr_en_mask_b",
	/* SPM_SRC4_MASK */
	[PW_CCIF_EVENT_MASK_B] = "ccif_event_mask_b",
	[PW_REG_BAK_PSRI_SRCCLKENA_MASK_B] = "reg_bak_psri_srcclkena_mask_b",
	[PW_REG_BAK_PSRI_INFRA_REQ_MASK_B] = "reg_bak_psri_infra_req_mask_b",
	[PW_REG_BAK_PSRI_APSRC_REQ_MASK_B] = "reg_bak_psri_apsrc_req_mask_b",
	[PW_REG_BAK_PSRI_VRF18_REQ_MASK_B] = "reg_bak_psri_vrf18_req_mask_b",
	[PW_REG_BAK_PSRI_DDR_EN_MASK_B] = "reg_bak_psri_ddr_en_mask_b",
	[PW_REG_DRAMC0_MD32_INFRA_REQ_MASK_B] =
	"reg_dramc0_md32_infra_req_mask_b",
	[PW_REG_DRAMC0_MD32_VRF18_REQ_MASK_B] =
	"reg_dramc0_md32_vrf18_req_mask_b",
	[PW_REG_DRAMC1_MD32_INFRA_REQ_MASK_B] =
	"reg_dramc1_md32_infra_req_mask_b",
	[PW_REG_DRAMC1_MD32_VRF18_REQ_MASK_B] =
	"reg_dramc1_md32_vrf18_req_mask_b",
	[PW_REG_CONN_SRCCLKENB2PWRAP_MASK_B] =
	"reg_conn_srcclkenb2pwrap_mask_b",
	[PW_REG_DRAMC0_MD32_WAKEUP_MASK] = "reg_dramc0_md32_wakeup_mask",
	[PW_REG_DRAMC1_MD32_WAKEUP_MASK] = "reg_dramc1_md32_wakeup_mask",
	/* SPM_SRC5_MASK */
	[PW_REG_MCUSYS_MERGE_APSRC_REQ_MASK_B] =
	"reg_mcusys_merge_apsrc_req_mask_b",
	[PW_REG_MCUSYS_MERGE_DDR_EN_MASK_B] = "reg_mcusys_merge_ddr_en_mask_b",
	[PW_REG_MSDC2_SRCCLKENA_MASK_B] = "reg_msdc2_srcclkena_mask_b",
	[PW_REG_MSDC2_INFRA_REQ_MASK_B] = "reg_msdc2_infra_req_mask_b",
	[PW_REG_MSDC2_APSRC_REQ_MASK_B] = "reg_msdc2_apsrc_req_mask_b",
	[PW_REG_MSDC2_VRF18_REQ_MASK_B] = "reg_msdc2_vrf18_req_mask_b",
	[PW_REG_MSDC2_DDR_EN_MASK_B] = "reg_msdc2_ddr_en_mask_b",
	[PW_REG_PCIE_SRCCLKENA_MASK_B] = "reg_pcie_srcclkena_mask_b",
	[PW_REG_PCIE_INFRA_REQ_MASK_B] = "reg_pcie_infra_req_mask_b",
	[PW_REG_PCIE_APSRC_REQ_MASK_B] = "reg_pcie_apsrc_req_mask_b",
	[PW_REG_PCIE_VRF18_REQ_MASK_B] = "reg_pcie_vrf18_req_mask_b",
	[PW_REG_PCIE_DDR_EN_MASK_B] = "reg_pcie_ddr_en_mask_b",
	/* SPM_SRC6_MASK */
	[PW_REG_DPMAIF_SRCCLKENA_MASK_B] = "reg_dpmaif_srcclkena_mask_b",
	[PW_REG_DPMAIF_INFRA_REQ_MASK_B] = "reg_dpmaif_infra_req_mask_b",
	[PW_REG_DPMAIF_APSRC_REQ_MASK_B] = "reg_dpmaif_apsrc_req_mask_b",
	[PW_REG_DPMAIF_VRF18_REQ_MASK_B] = "reg_dpmaif_vrf18_req_mask_b",
	[PW_REG_DPMAIF_DDR_EN_MASK_B] = "reg_dpmaif_ddr_en_mask_b",
	/* SPM_WAKEUP_EVENT_MASK */
	[PW_REG_WAKEUP_EVENT_MASK] = "reg_wakeup_event_mask",
	/* SPM_WAKEUP_EVENT_EXT_MASK */
	[PW_REG_EXT_WAKEUP_EVENT_MASK] = "reg_ext_wakeup_event_mask",
};

/**************************************
 * xxx_ctrl_show Function
 **************************************/
/* code gen by spm_pwr_ctrl_atf.pl, need struct pwr_ctrl */
static ssize_t mt6873_show_pwr_ctrl(int id, char *buf, size_t buf_sz)
{
	char *p = buf;
	size_t mSize = 0;

	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"pcm_flags = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_PCM_FLAGS, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"pcm_flags_cust = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_PCM_FLAGS_CUST, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"pcm_flags_cust_set = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_PCM_FLAGS_CUST_SET, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"pcm_flags_cust_clr = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_PCM_FLAGS_CUST_CLR, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"pcm_flags1 = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_PCM_FLAGS1, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"pcm_flags1_cust = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_PCM_FLAGS1_CUST, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"pcm_flags1_cust_set = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_PCM_FLAGS1_CUST_SET, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"pcm_flags1_cust_clr = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_PCM_FLAGS1_CUST_CLR, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"timer_val = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_TIMER_VAL, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"timer_val_cust = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_TIMER_VAL_CUST, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"timer_val_ramp_en = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_TIMER_VAL_RAMP_EN, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"timer_val_ramp_en_sec = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_TIMER_VAL_RAMP_EN_SEC, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"wake_src = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_WAKE_SRC, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"wake_src_cust = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_WAKE_SRC_CUST, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"wakelock_timer_val = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_WAKELOCK_TIMER_VAL, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"wdt_disable = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_WDT_DISABLE, 0));
	/* SPM_CLK_CON */
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_srcclken0_ctl = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SRCCLKEN0_CTL, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_srcclken1_ctl = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SRCCLKEN1_CTL, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_spm_lock_infra_dcm = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SPM_LOCK_INFRA_DCM, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_srcclken_mask = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SRCCLKEN_MASK, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_md1_c32rm_en = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MD1_C32RM_EN, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_md2_c32rm_en = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MD2_C32RM_EN, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_clksq0_sel_ctrl = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_CLKSQ0_SEL_CTRL, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_clksq1_sel_ctrl = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_CLKSQ1_SEL_CTRL, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_srcclken0_en = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SRCCLKEN0_EN, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_srcclken1_en = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SRCCLKEN1_EN, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_sysclk0_src_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SYSCLK0_SRC_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_sysclk1_src_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SYSCLK1_SRC_MASK_B, 0));
	/* SPM_AP_STANDBY_CON */
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_wfi_op = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_WFI_OP, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_wfi_type = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_WFI_TYPE, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_mp0_cputop_idle_mask = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MP0_CPUTOP_IDLE_MASK, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_mp1_cputop_idle_mask = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MP1_CPUTOP_IDLE_MASK, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_mcusys_idle_mask = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MCUSYS_IDLE_MASK, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_md_apsrc_1_sel = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MD_APSRC_1_SEL, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_md_apsrc_0_sel = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MD_APSRC_0_SEL, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_conn_apsrc_sel = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_CONN_APSRC_SEL, 0));
	/* SPM_SRC_REQ */
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_spm_apsrc_req = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SPM_APSRC_REQ, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_spm_f26m_req = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SPM_F26M_REQ, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_spm_infra_req = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SPM_INFRA_REQ, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_spm_vrf18_req = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SPM_VRF18_REQ, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_spm_ddr_en_req = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SPM_DDR_EN_REQ, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_spm_dvfs_req = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SPM_DVFS_REQ, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_spm_sw_mailbox_req = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SPM_SW_MAILBOX_REQ, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_spm_sspm_mailbox_req = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SPM_SSPM_MAILBOX_REQ, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_spm_adsp_mailbox_req = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SPM_ADSP_MAILBOX_REQ, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_spm_scp_mailbox_req = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SPM_SCP_MAILBOX_REQ, 0));
	/* SPM_SRC_MASK */
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_md_srcclkena_0_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MD_SRCCLKENA_0_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_md_srcclkena2infra_req_0_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MD_SRCCLKENA2INFRA_REQ_0_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_md_apsrc2infra_req_0_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MD_APSRC2INFRA_REQ_0_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_md_apsrc_req_0_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MD_APSRC_REQ_0_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_md_vrf18_req_0_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MD_VRF18_REQ_0_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_md_ddr_en_0_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MD_DDR_EN_0_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_md_srcclkena_1_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MD_SRCCLKENA_1_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_md_srcclkena2infra_req_1_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MD_SRCCLKENA2INFRA_REQ_1_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_md_apsrc2infra_req_1_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MD_APSRC2INFRA_REQ_1_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_md_apsrc_req_1_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MD_APSRC_REQ_1_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_md_vrf18_req_1_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MD_VRF18_REQ_1_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_md_ddr_en_1_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MD_DDR_EN_1_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_conn_srcclkena_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_CONN_SRCCLKENA_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_conn_srcclkenb_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_CONN_SRCCLKENB_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_conn_infra_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_CONN_INFRA_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_conn_apsrc_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_CONN_APSRC_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_conn_vrf18_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_CONN_VRF18_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_conn_ddr_en_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_CONN_DDR_EN_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_conn_vfe28_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_CONN_VFE28_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_srcclkeni0_srcclkena_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SRCCLKENI0_SRCCLKENA_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_srcclkeni0_infra_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SRCCLKENI0_INFRA_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_srcclkeni1_srcclkena_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SRCCLKENI1_SRCCLKENA_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_srcclkeni1_infra_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SRCCLKENI1_INFRA_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_srcclkeni2_srcclkena_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SRCCLKENI2_SRCCLKENA_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_srcclkeni2_infra_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SRCCLKENI2_INFRA_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_infrasys_apsrc_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_INFRASYS_APSRC_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_infrasys_ddr_en_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_INFRASYS_DDR_EN_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_md32_srcclkena_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MD32_SRCCLKENA_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_md32_infra_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MD32_INFRA_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_md32_apsrc_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MD32_APSRC_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_md32_vrf18_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MD32_VRF18_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_md32_ddr_en_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MD32_DDR_EN_MASK_B, 0));
	/* SPM_SRC2_MASK */
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_scp_srcclkena_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SCP_SRCCLKENA_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_scp_infra_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SCP_INFRA_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_scp_apsrc_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SCP_APSRC_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_scp_vrf18_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SCP_VRF18_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_scp_ddr_en_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SCP_DDR_EN_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_audio_dsp_srcclkena_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_AUDIO_DSP_SRCCLKENA_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_audio_dsp_infra_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_AUDIO_DSP_INFRA_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_audio_dsp_apsrc_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_AUDIO_DSP_APSRC_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_audio_dsp_vrf18_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_AUDIO_DSP_VRF18_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_audio_dsp_ddr_en_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_AUDIO_DSP_DDR_EN_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_ufs_srcclkena_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_UFS_SRCCLKENA_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_ufs_infra_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_UFS_INFRA_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_ufs_apsrc_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_UFS_APSRC_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_ufs_vrf18_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_UFS_VRF18_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_ufs_ddr_en_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_UFS_DDR_EN_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_disp0_apsrc_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_DISP0_APSRC_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_disp0_ddr_en_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_DISP0_DDR_EN_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_disp1_apsrc_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_DISP1_APSRC_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_disp1_ddr_en_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_DISP1_DDR_EN_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_gce_infra_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_GCE_INFRA_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_gce_apsrc_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_GCE_APSRC_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_gce_vrf18_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_GCE_VRF18_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_gce_ddr_en_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_GCE_DDR_EN_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_apu_srcclkena_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_APU_SRCCLKENA_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_apu_infra_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_APU_INFRA_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_apu_apsrc_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_APU_APSRC_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_apu_vrf18_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_APU_VRF18_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_apu_ddr_en_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_APU_DDR_EN_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_cg_check_srcclkena_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_CG_CHECK_SRCCLKENA_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_cg_check_apsrc_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_CG_CHECK_APSRC_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_cg_check_vrf18_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_CG_CHECK_VRF18_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_cg_check_ddr_en_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_CG_CHECK_DDR_EN_MASK_B, 0));
	/* SPM_SRC3_MASK */
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_dvfsrc_event_trigger_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_DVFSRC_EVENT_TRIGGER_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_sw2spm_int0_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SW2SPM_INT0_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_sw2spm_int1_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SW2SPM_INT1_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_sw2spm_int2_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SW2SPM_INT2_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_sw2spm_int3_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SW2SPM_INT3_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_sc_adsp2spm_wakeup_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SC_ADSP2SPM_WAKEUP_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_sc_sspm2spm_wakeup_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SC_SSPM2SPM_WAKEUP_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_sc_scp2spm_wakeup_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SC_SCP2SPM_WAKEUP_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_csyspwrreq_mask = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_CSYSPWRREQ_MASK, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_spm_srcclkena_reserved_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SPM_SRCCLKENA_RESERVED_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_spm_infra_req_reserved_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SPM_INFRA_REQ_RESERVED_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_spm_apsrc_req_reserved_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SPM_APSRC_REQ_RESERVED_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_spm_vrf18_req_reserved_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SPM_VRF18_REQ_RESERVED_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_spm_ddr_en_reserved_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_SPM_DDR_EN_RESERVED_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_mcupm_srcclkena_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MCUPM_SRCCLKENA_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_mcupm_infra_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MCUPM_INFRA_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_mcupm_apsrc_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MCUPM_APSRC_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_mcupm_vrf18_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MCUPM_VRF18_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_mcupm_ddr_en_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MCUPM_DDR_EN_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_msdc0_srcclkena_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MSDC0_SRCCLKENA_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_msdc0_infra_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MSDC0_INFRA_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_msdc0_apsrc_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MSDC0_APSRC_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_msdc0_vrf18_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MSDC0_VRF18_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_msdc0_ddr_en_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MSDC0_DDR_EN_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_msdc1_srcclkena_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MSDC1_SRCCLKENA_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_msdc1_infra_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MSDC1_INFRA_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_msdc1_apsrc_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MSDC1_APSRC_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_msdc1_vrf18_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MSDC1_VRF18_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_msdc1_ddr_en_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MSDC1_DDR_EN_MASK_B, 0));
	/* SPM_SRC4_MASK */
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"ccif_event_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_CCIF_EVENT_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_bak_psri_srcclkena_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_BAK_PSRI_SRCCLKENA_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_bak_psri_infra_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_BAK_PSRI_INFRA_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_bak_psri_apsrc_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_BAK_PSRI_APSRC_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_bak_psri_vrf18_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_BAK_PSRI_VRF18_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_bak_psri_ddr_en_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_BAK_PSRI_DDR_EN_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_dramc0_md32_infra_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_DRAMC0_MD32_INFRA_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_dramc0_md32_vrf18_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_DRAMC0_MD32_VRF18_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_dramc1_md32_infra_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_DRAMC1_MD32_INFRA_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_dramc1_md32_vrf18_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_DRAMC1_MD32_VRF18_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_conn_srcclkenb2pwrap_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_CONN_SRCCLKENB2PWRAP_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_dramc0_md32_wakeup_mask = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_DRAMC0_MD32_WAKEUP_MASK, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_dramc1_md32_wakeup_mask = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_DRAMC1_MD32_WAKEUP_MASK, 0));
	/* SPM_SRC5_MASK */
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_mcusys_merge_apsrc_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MCUSYS_MERGE_APSRC_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_mcusys_merge_ddr_en_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MCUSYS_MERGE_DDR_EN_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_msdc2_srcclkena_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MSDC2_SRCCLKENA_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_msdc2_infra_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MSDC2_INFRA_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_msdc2_apsrc_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MSDC2_APSRC_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_msdc2_vrf18_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MSDC2_VRF18_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_msdc2_ddr_en_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_MSDC2_DDR_EN_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_pcie_srcclkena_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_PCIE_SRCCLKENA_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_pcie_infra_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_PCIE_INFRA_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_pcie_apsrc_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_PCIE_APSRC_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_pcie_vrf18_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_PCIE_VRF18_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_pcie_ddr_en_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_PCIE_DDR_EN_MASK_B, 0));
	/* SPM_SRC6_MASK */
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_dpmaif_srcclkena_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_DPMAIF_SRCCLKENA_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_dpmaif_infra_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_DPMAIF_INFRA_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_dpmaif_apsrc_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_DPMAIF_APSRC_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_dpmaif_vrf18_req_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_DPMAIF_VRF18_REQ_MASK_B, 0));
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_dpmaif_ddr_en_mask_b = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_DPMAIF_DDR_EN_MASK_B, 0));

	/* SPM_WAKEUP_EVENT_MASK */
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_wakeup_event_mask = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_WAKEUP_EVENT_MASK, 0));
	/* SPM_WAKEUP_EVENT_EXT_MASK */
	mSize += scnprintf(p + mSize, buf_sz - mSize,
			"reg_ext_wakeup_event_mask = 0x%zx\n",
			lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_GET,
				PW_REG_EXT_WAKEUP_EVENT_MASK, 0));

	WARN_ON(buf_sz - mSize <= 0);

	return mSize;
}

/**************************************
 * xxx_ctrl_store Function
 **************************************/
/* code gen by spm_pwr_ctrl_atf.pl, need struct pwr_ctrl */
static ssize_t mt6873_store_pwr_ctrl(int id,	const char *buf, size_t count)
{
	u32 val;
	char cmd[64];

	if (sscanf(buf, "%63s %x", cmd, &val) != 2)
		return -EINVAL;
	pr_info("[SPM] pwr_ctrl: cmd = %s, val = 0x%x\n", cmd, val);
	if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_PCM_FLAGS])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_PCM_FLAGS, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_PCM_FLAGS_CUST])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_PCM_FLAGS_CUST, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_PCM_FLAGS_CUST_SET])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_PCM_FLAGS_CUST_SET, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_PCM_FLAGS_CUST_CLR])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_PCM_FLAGS_CUST_CLR, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_PCM_FLAGS1])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_PCM_FLAGS1, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_PCM_FLAGS1_CUST])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_PCM_FLAGS1_CUST, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_PCM_FLAGS1_CUST_SET])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_PCM_FLAGS1_CUST_SET, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_PCM_FLAGS1_CUST_CLR])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_PCM_FLAGS1_CUST_CLR, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_TIMER_VAL])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_TIMER_VAL, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_TIMER_VAL_CUST])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_TIMER_VAL_CUST, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_TIMER_VAL_RAMP_EN])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_TIMER_VAL_RAMP_EN, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_TIMER_VAL_RAMP_EN_SEC])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_TIMER_VAL_RAMP_EN_SEC, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_WAKE_SRC])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_WAKE_SRC, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_WAKE_SRC_CUST])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_WAKE_SRC_CUST, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_WAKELOCK_TIMER_VAL])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_WAKELOCK_TIMER_VAL, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_WDT_DISABLE])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_WDT_DISABLE, val);
	/* SPM_CLK_CON */
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SRCCLKEN0_CTL])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SRCCLKEN0_CTL, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SRCCLKEN1_CTL])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SRCCLKEN1_CTL, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SPM_LOCK_INFRA_DCM])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SPM_LOCK_INFRA_DCM, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SRCCLKEN_MASK])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SRCCLKEN_MASK, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MD1_C32RM_EN])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MD1_C32RM_EN, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MD2_C32RM_EN])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MD2_C32RM_EN, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_CLKSQ0_SEL_CTRL])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_CLKSQ0_SEL_CTRL, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_CLKSQ1_SEL_CTRL])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_CLKSQ1_SEL_CTRL, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SRCCLKEN0_EN])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SRCCLKEN0_EN, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SRCCLKEN1_EN])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SRCCLKEN1_EN, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SYSCLK0_SRC_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SYSCLK0_SRC_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SYSCLK1_SRC_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SYSCLK1_SRC_MASK_B, val);
	/* SPM_AP_STANDBY_CON */
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_WFI_OP])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_WFI_OP, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_WFI_TYPE])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_WFI_TYPE, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MP0_CPUTOP_IDLE_MASK])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MP0_CPUTOP_IDLE_MASK, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MP1_CPUTOP_IDLE_MASK])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MP1_CPUTOP_IDLE_MASK, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MCUSYS_IDLE_MASK])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MCUSYS_IDLE_MASK, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MD_APSRC_1_SEL])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MD_APSRC_1_SEL, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MD_APSRC_0_SEL])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MD_APSRC_0_SEL, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_CONN_APSRC_SEL])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_CONN_APSRC_SEL, val);
	/* SPM_SRC_REQ */
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SPM_APSRC_REQ])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SPM_APSRC_REQ, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SPM_F26M_REQ])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SPM_F26M_REQ, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SPM_INFRA_REQ])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SPM_INFRA_REQ, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SPM_VRF18_REQ])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SPM_VRF18_REQ, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SPM_DDR_EN_REQ])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SPM_DDR_EN_REQ, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SPM_DVFS_REQ])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SPM_DVFS_REQ, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SPM_SW_MAILBOX_REQ])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SPM_SW_MAILBOX_REQ, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SPM_SSPM_MAILBOX_REQ])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SPM_SSPM_MAILBOX_REQ, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SPM_ADSP_MAILBOX_REQ])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SPM_ADSP_MAILBOX_REQ, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SPM_SCP_MAILBOX_REQ])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SPM_SCP_MAILBOX_REQ, val);
	/* SPM_SRC_MASK */
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MD_SRCCLKENA_0_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MD_SRCCLKENA_0_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MD_SRCCLKENA2INFRA_REQ_0_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MD_SRCCLKENA2INFRA_REQ_0_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MD_APSRC2INFRA_REQ_0_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MD_APSRC2INFRA_REQ_0_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MD_APSRC_REQ_0_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MD_APSRC_REQ_0_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MD_VRF18_REQ_0_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MD_VRF18_REQ_0_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MD_DDR_EN_0_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MD_DDR_EN_0_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MD_SRCCLKENA_1_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MD_SRCCLKENA_1_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MD_SRCCLKENA2INFRA_REQ_1_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MD_SRCCLKENA2INFRA_REQ_1_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MD_APSRC2INFRA_REQ_1_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MD_APSRC2INFRA_REQ_1_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MD_APSRC_REQ_1_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MD_APSRC_REQ_1_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MD_VRF18_REQ_1_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MD_VRF18_REQ_1_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MD_DDR_EN_1_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MD_DDR_EN_1_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_CONN_SRCCLKENA_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_CONN_SRCCLKENA_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_CONN_SRCCLKENB_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_CONN_SRCCLKENB_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_CONN_INFRA_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_CONN_INFRA_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_CONN_APSRC_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_CONN_APSRC_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_CONN_VRF18_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_CONN_VRF18_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_CONN_DDR_EN_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_CONN_DDR_EN_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_CONN_VFE28_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_CONN_VFE28_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SRCCLKENI0_SRCCLKENA_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SRCCLKENI0_SRCCLKENA_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SRCCLKENI0_INFRA_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SRCCLKENI0_INFRA_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SRCCLKENI1_SRCCLKENA_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SRCCLKENI1_SRCCLKENA_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SRCCLKENI1_INFRA_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SRCCLKENI1_INFRA_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SRCCLKENI2_SRCCLKENA_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SRCCLKENI2_SRCCLKENA_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SRCCLKENI2_INFRA_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SRCCLKENI2_INFRA_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_INFRASYS_APSRC_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_INFRASYS_APSRC_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_INFRASYS_DDR_EN_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_INFRASYS_DDR_EN_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MD32_SRCCLKENA_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MD32_SRCCLKENA_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MD32_INFRA_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MD32_INFRA_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MD32_APSRC_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MD32_APSRC_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MD32_VRF18_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MD32_VRF18_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MD32_DDR_EN_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MD32_DDR_EN_MASK_B, val);
	/* SPM_SRC2_MASK */
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SCP_SRCCLKENA_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SCP_SRCCLKENA_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SCP_INFRA_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SCP_INFRA_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SCP_APSRC_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SCP_APSRC_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SCP_VRF18_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SCP_VRF18_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SCP_DDR_EN_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SCP_DDR_EN_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_AUDIO_DSP_SRCCLKENA_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_AUDIO_DSP_SRCCLKENA_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_AUDIO_DSP_INFRA_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_AUDIO_DSP_INFRA_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_AUDIO_DSP_APSRC_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_AUDIO_DSP_APSRC_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_AUDIO_DSP_VRF18_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_AUDIO_DSP_VRF18_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_AUDIO_DSP_DDR_EN_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_AUDIO_DSP_DDR_EN_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_UFS_SRCCLKENA_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_UFS_SRCCLKENA_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_UFS_INFRA_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_UFS_INFRA_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_UFS_APSRC_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_UFS_APSRC_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_UFS_VRF18_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_UFS_VRF18_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_UFS_DDR_EN_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_UFS_DDR_EN_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_DISP0_APSRC_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_DISP0_APSRC_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_DISP0_DDR_EN_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_DISP0_DDR_EN_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_DISP1_APSRC_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_DISP1_APSRC_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_DISP1_DDR_EN_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_DISP1_DDR_EN_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_GCE_INFRA_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_GCE_INFRA_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_GCE_APSRC_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_GCE_APSRC_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_GCE_VRF18_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_GCE_VRF18_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_GCE_DDR_EN_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_GCE_DDR_EN_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_APU_SRCCLKENA_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_APU_SRCCLKENA_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_APU_INFRA_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_APU_INFRA_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_APU_APSRC_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_APU_APSRC_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_APU_VRF18_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_APU_VRF18_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_APU_DDR_EN_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_APU_DDR_EN_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_CG_CHECK_SRCCLKENA_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_CG_CHECK_SRCCLKENA_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_CG_CHECK_APSRC_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_CG_CHECK_APSRC_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_CG_CHECK_VRF18_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_CG_CHECK_VRF18_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_CG_CHECK_DDR_EN_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_CG_CHECK_DDR_EN_MASK_B, val);
	/* SPM_SRC3_MASK */
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_DVFSRC_EVENT_TRIGGER_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_DVFSRC_EVENT_TRIGGER_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SW2SPM_INT0_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SW2SPM_INT0_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SW2SPM_INT1_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SW2SPM_INT1_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SW2SPM_INT2_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SW2SPM_INT2_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SW2SPM_INT3_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SW2SPM_INT3_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SC_ADSP2SPM_WAKEUP_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SC_ADSP2SPM_WAKEUP_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SC_SSPM2SPM_WAKEUP_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SC_SSPM2SPM_WAKEUP_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SC_SCP2SPM_WAKEUP_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SC_SCP2SPM_WAKEUP_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_CSYSPWRREQ_MASK])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_CSYSPWRREQ_MASK, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SPM_SRCCLKENA_RESERVED_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SPM_SRCCLKENA_RESERVED_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SPM_INFRA_REQ_RESERVED_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SPM_INFRA_REQ_RESERVED_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SPM_APSRC_REQ_RESERVED_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SPM_APSRC_REQ_RESERVED_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SPM_VRF18_REQ_RESERVED_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SPM_VRF18_REQ_RESERVED_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_SPM_DDR_EN_RESERVED_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_SPM_DDR_EN_RESERVED_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MCUPM_SRCCLKENA_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MCUPM_SRCCLKENA_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MCUPM_INFRA_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MCUPM_INFRA_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MCUPM_APSRC_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MCUPM_APSRC_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MCUPM_VRF18_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MCUPM_VRF18_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MCUPM_DDR_EN_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MCUPM_DDR_EN_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MSDC0_SRCCLKENA_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MSDC0_SRCCLKENA_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MSDC0_INFRA_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MSDC0_INFRA_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MSDC0_APSRC_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MSDC0_APSRC_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MSDC0_VRF18_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MSDC0_VRF18_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MSDC0_DDR_EN_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MSDC0_DDR_EN_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MSDC1_SRCCLKENA_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MSDC1_SRCCLKENA_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MSDC1_INFRA_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MSDC1_INFRA_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MSDC1_APSRC_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MSDC1_APSRC_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MSDC1_VRF18_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MSDC1_VRF18_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MSDC1_DDR_EN_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MSDC1_DDR_EN_MASK_B, val);
	/* SPM_SRC4_MASK */
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_CCIF_EVENT_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
					PW_CCIF_EVENT_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_BAK_PSRI_SRCCLKENA_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_BAK_PSRI_SRCCLKENA_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_BAK_PSRI_INFRA_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_BAK_PSRI_INFRA_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_BAK_PSRI_APSRC_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_BAK_PSRI_APSRC_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_BAK_PSRI_VRF18_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_BAK_PSRI_VRF18_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_BAK_PSRI_DDR_EN_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_BAK_PSRI_DDR_EN_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_DRAMC0_MD32_INFRA_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_DRAMC0_MD32_INFRA_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_DRAMC0_MD32_VRF18_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_DRAMC0_MD32_VRF18_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_DRAMC1_MD32_INFRA_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_DRAMC1_MD32_INFRA_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_DRAMC1_MD32_VRF18_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_DRAMC1_MD32_VRF18_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_CONN_SRCCLKENB2PWRAP_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_CONN_SRCCLKENB2PWRAP_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_DRAMC0_MD32_WAKEUP_MASK])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_DRAMC0_MD32_WAKEUP_MASK, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_DRAMC1_MD32_WAKEUP_MASK])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_DRAMC1_MD32_WAKEUP_MASK, val);
	/* SPM_SRC5_MASK */
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MCUSYS_MERGE_APSRC_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MCUSYS_MERGE_APSRC_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MCUSYS_MERGE_DDR_EN_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MCUSYS_MERGE_DDR_EN_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MSDC2_SRCCLKENA_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MSDC2_SRCCLKENA_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MSDC2_INFRA_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MSDC2_INFRA_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MSDC2_APSRC_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MSDC2_APSRC_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MSDC2_VRF18_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MSDC2_VRF18_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_MSDC2_DDR_EN_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_MSDC2_DDR_EN_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_PCIE_SRCCLKENA_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_PCIE_SRCCLKENA_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_PCIE_INFRA_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_PCIE_INFRA_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_PCIE_APSRC_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_PCIE_APSRC_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_PCIE_VRF18_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_PCIE_VRF18_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_PCIE_DDR_EN_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_PCIE_DDR_EN_MASK_B, val);
	/* SPM_SRC6_MASK */
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_DPMAIF_SRCCLKENA_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_DPMAIF_SRCCLKENA_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_DPMAIF_INFRA_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_DPMAIF_INFRA_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_DPMAIF_APSRC_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_DPMAIF_APSRC_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_DPMAIF_VRF18_REQ_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_DPMAIF_VRF18_REQ_MASK_B, val);
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_DPMAIF_DDR_EN_MASK_B])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_DPMAIF_DDR_EN_MASK_B, val);
	/* SPM_WAKEUP_EVENT_MASK */
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_WAKEUP_EVENT_MASK])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_WAKEUP_EVENT_MASK, val);
	/* SPM_WAKEUP_EVENT_EXT_MASK */
	} else if (!strcmp(cmd,
		mt6873_pwr_ctrl_str[PW_REG_EXT_WAKEUP_EVENT_MASK])) {
		lpm_smc_spm_dbg(id, MT_LPM_SMC_ACT_SET,
				PW_REG_EXT_WAKEUP_EVENT_MASK, val);
	}

	return count;
}

static ssize_t
mt6873_generic_spm_read(char *ToUserBuf, size_t sz, void *priv);

static ssize_t
mt6873_generic_spm_write(char *FromUserBuf, size_t sz, void *priv);

struct mt6873_SPM_ENTERY {
	const char *name;
	int mode;
	struct mtk_lp_sysfs_handle handle;
};

struct mt6873_SPM_NODE {
	const char *name;
	int mode;
	struct mtk_lp_sysfs_handle handle;
	struct mtk_lp_sysfs_op op;
};


struct mt6873_SPM_ENTERY mt6873_spm_root = {
	.name = "power",
	.mode = 0644,
};

struct mt6873_SPM_NODE mt6873_spm_idle = {
	.name = "idle_ctrl",
	.mode = 0644,
	.op = {
		.fs_read = mt6873_generic_spm_read,
		.fs_write = mt6873_generic_spm_write,
		.priv = (void *)&mt6873_spm_idle,
	},
};

struct mt6873_SPM_NODE mt6873_spm_suspend = {
	.name = "suspend_ctrl",
	.mode = 0644,
	.op = {
		.fs_read = mt6873_generic_spm_read,
		.fs_write = mt6873_generic_spm_write,
		.priv = (void *)&mt6873_spm_suspend,
	},
};

static ssize_t
mt6873_generic_spm_read(char *ToUserBuf, size_t sz, void *priv)
{
	int id = MT_SPM_DBG_SMC_UID_SUSPEND_PWR_CTRL;

	if (priv == &mt6873_spm_idle)
		id = MT_SPM_DBG_SMC_UID_IDLE_PWR_CTRL;
	return mt6873_show_pwr_ctrl(id, ToUserBuf, sz);
}

#include <mtk_lpm_sysfs.h>

static ssize_t
mt6873_generic_spm_write(char *FromUserBuf, size_t sz, void *priv)
{
	int id = MT_SPM_DBG_SMC_UID_SUSPEND_PWR_CTRL;

	if (priv == &mt6873_spm_idle)
		id = MT_SPM_DBG_SMC_UID_IDLE_PWR_CTRL;

	return mt6873_store_pwr_ctrl(id, FromUserBuf, sz);
}

static char *mt6873_spm_resource_str[MT_SPM_RES_MAX] = {
	[MT_SPM_RES_XO_FPM] = "XO_FPM",
	[MT_SPM_RES_CK_26M] = "CK_26M",
	[MT_SPM_RES_INFRA] = "INFRA",
	[MT_SPM_RES_SYSPLL] = "SYSPLL",
	[MT_SPM_RES_DRAM_S0] = "DRAM_S0",
	[MT_SPM_RES_DRAM_S1] = "DRAM_S1",
};

static ssize_t mt6873_spm_res_rq_read(char *ToUserBuf, size_t sz, void *priv)
{
	char *p = ToUserBuf;
	int i, s, u;
	unsigned int unum, uvalid, uname_i, uname_t;
	unsigned int rnum, rusage, per_usage;
	char uname[MT_LP_RQ_USER_NAME_LEN+1];

	mtk_dbg_spm_log("resource_num=%d, user_num=%d, user_valid=0x%x\n",
	    rnum = lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RES_NUM,
				       MT_LPM_SMC_ACT_GET, 0, 0),
	    unum = lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RES_USER_NUM,
				       MT_LPM_SMC_ACT_GET, 0, 0),
	    uvalid = lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RES_USER_VALID,
					 MT_LPM_SMC_ACT_GET, 0, 0));
	rusage = lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RES_USAGE,
				     MT_LPM_SMC_ACT_GET,
				     MT_LP_RQ_ID_ALL_USAGE, 0);
	mtk_dbg_spm_log("\n");
	mtk_dbg_spm_log("user [bit][valid]:\n");
	for (i = 0; i < unum; i++) {
		uname_i = lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RES_USER_NAME,
					    MT_LPM_SMC_ACT_GET, i, 0);
		for (s = 0, u = 0; s < MT_LP_RQ_USER_NAME_LEN;
		     s++, u += MT_LP_RQ_USER_CHAR_U) {
			uname_t = ((uname_i >> u) & MT_LP_RQ_USER_CHAR_MASK);
			uname[s] = (uname_t) ? (char)uname_t : ' ';
		}
		uname[s] = '\0';
		mtk_dbg_spm_log("%4s [%3d][%3s]\n", uname, i,
		    ((1<<i) & uvalid) ? "yes" : "no");
	}
	mtk_dbg_spm_log("\n");

	if (rnum != MT_SPM_RES_MAX) {
		mtk_dbg_spm_log("Platform resource amount mismatch\n");
		rnum = (rnum > MT_SPM_RES_MAX) ? MT_SPM_RES_MAX : rnum;
	}

	mtk_dbg_spm_log("resource [bit][user_usage][blocking]:\n");
	for (i = 0; i < rnum; i++) {
		mtk_dbg_spm_log("%8s [%3d][0x%08x][%3s]\n",
			mt6873_spm_resource_str[i], i,
			(per_usage =
			lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RES_USAGE,
					    MT_LPM_SMC_ACT_GET, i, 0)),
			((1<<i) & rusage) ? "yes" : "no"
		   );
	}
	mtk_dbg_spm_log("\n");
	mtk_dbg_spm_log("resource request command help:\n");
	mtk_dbg_spm_log("echo enable ${user_bit} > %s\n", MT_LP_RQ_NODE);
	mtk_dbg_spm_log("echo bypass ${user_bit} > %s\n", MT_LP_RQ_NODE);
	mtk_dbg_spm_log("echo request ${resource_bit} > %s\n", MT_LP_RQ_NODE);
	mtk_dbg_spm_log("echo release > %s\n", MT_LP_RQ_NODE);

	return p - ToUserBuf;
}

static ssize_t mt6873_spm_res_rq_write(char *FromUserBuf, size_t sz, void *priv)
{
	char cmd[128];
	int parm;

	if (sscanf(FromUserBuf, "%127s %x", cmd, &parm) == 2) {
		if (!strcmp(cmd, "bypass"))
			lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RES_USER_VALID,
					    MT_LPM_SMC_ACT_SET,
					    parm, 0);
		else if (!strcmp(cmd, "enable"))
			lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RES_USER_VALID,
					    MT_LPM_SMC_ACT_SET,
					    parm, 1);
		else if (!strcmp(cmd, "request"))
			lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RES_REQ,
					    MT_LPM_SMC_ACT_SET,
					    0, parm);
		return sz;
	} else if (sscanf(FromUserBuf, "%127s", cmd) == 1) {
		if (!strcmp(cmd, "release"))
			lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RES_REQ,
					    MT_LPM_SMC_ACT_CLR,
					    0, 0);
		return sz;
	} else if ((!kstrtoint(FromUserBuf, 10, &parm)) == 1) {
		return sz;
	}

	return -EINVAL;
}

static const struct mtk_lp_sysfs_op mt6873_spm_res_rq_fops = {
	.fs_read = mt6873_spm_res_rq_read,
	.fs_write = mt6873_spm_res_rq_write,
};

int lpm_spm_fs_init(void)
{
	int r;

	mtk_spm_sysfs_root_entry_create();
	mtk_spm_sysfs_entry_node_add("spm_resource_req", 0444
			, &mt6873_spm_res_rq_fops, NULL);

	r = mtk_lp_sysfs_entry_func_create(mt6873_spm_root.name,
					   mt6873_spm_root.mode, NULL,
					   &mt6873_spm_root.handle);
	if (!r) {
		mtk_lp_sysfs_entry_func_node_add(mt6873_spm_suspend.name,
						mt6873_spm_suspend.mode,
						&mt6873_spm_suspend.op,
						&mt6873_spm_root.handle,
						&mt6873_spm_suspend.handle);

		mtk_lp_sysfs_entry_func_node_add(mt6873_spm_idle.name,
						 mt6873_spm_idle.mode,
						 &mt6873_spm_idle.op,
						 &mt6873_spm_root.handle,
						 &mt6873_spm_idle.handle);
	}

	return r;
}

int lpm_spm_fs_deinit(void)
{
	return 0;
}

