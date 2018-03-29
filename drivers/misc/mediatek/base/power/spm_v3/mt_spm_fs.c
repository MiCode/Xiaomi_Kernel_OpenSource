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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/string.h>

#include "mt_spm_internal.h"
#include "mt_sleep.h"

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

/* code gen by spm_pwr_ctrl.pl, need struct pwr_ctrl */
static char *pwr_ctrl_str[PWR_MAX_COUNT] = {
	[PWR_PCM_FLAGS] = "pcm_flags",
	[PWR_PCM_FLAGS_CUST] = "pcm_flags_cust",
	[PWR_PCM_FLAGS_CUST_SET] = "pcm_flags_cust_set",
	[PWR_PCM_FLAGS_CUST_CLR] = "pcm_flags_cust_clr",
	[PWR_PCM_RESERVE] = "pcm_reserve",
	[PWR_TIMER_VAL] = "timer_val",
	[PWR_TIMER_VAL_CUST] = "timer_val_cust",
	[PWR_TIMER_VAL_RAMP_EN] = "timer_val_ramp_en",
	[PWR_TIMER_VAL_RAMP_EN_SEC] = "timer_val_ramp_en_sec",
	[PWR_WAKE_SRC] = "wake_src",
	[PWR_WAKE_SRC_CUST] = "wake_src_cust",
	[PWR_WAKE_SRC_MD32] = "wake_src_md32",
	[PWR_WDT_DISABLE] = "wdt_disable",
	[PWR_DVFS_HALT_SRC_CHK] = "dvfs_halt_src_chk",
	[PWR_SYSPWREQ_MASK] = "syspwreq_mask",
	[PWR_REG_SRCCLKEN0_CTL] = "reg_srcclken0_ctl",
	[PWR_REG_SRCCLKEN1_CTL] = "reg_srcclken1_ctl",
	[PWR_REG_SPM_LOCK_INFRA_DCM] = "reg_spm_lock_infra_dcm",
	[PWR_REG_SRCCLKEN_MASK] = "reg_srcclken_mask",
	[PWR_REG_MD1_C32RM_EN] = "reg_md1_c32rm_en",
	[PWR_REG_MD2_C32RM_EN] = "reg_md2_c32rm_en",
	[PWR_REG_CLKSQ0_SEL_CTRL] = "reg_clksq0_sel_ctrl",
	[PWR_REG_CLKSQ1_SEL_CTRL] = "reg_clksq1_sel_ctrl",
	[PWR_REG_SRCCLKEN0_EN] = "reg_srcclken0_en",
	[PWR_REG_SRCCLKEN1_EN] = "reg_srcclken1_en",
	[PWR_REG_SYSCLK0_SRC_MASK_B] = "reg_sysclk0_src_mask_b",
	[PWR_REG_SYSCLK1_SRC_MASK_B] = "reg_sysclk1_src_mask_b",
	[PWR_REG_MPWFI_OP] = "reg_mpwfi_op",
	[PWR_REG_MP0_CPUTOP_IDLE_MASK] = "reg_mp0_cputop_idle_mask",
	[PWR_REG_MP1_CPUTOP_IDLE_MASK] = "reg_mp1_cputop_idle_mask",
	[PWR_REG_DEBUGTOP_IDLE_MASK] = "reg_debugtop_idle_mask",
	[PWR_REG_MP_TOP_IDLE_MASK] = "reg_mp_top_idle_mask",
	[PWR_REG_MCUSYS_IDLE_MASK] = "reg_mcusys_idle_mask",
	[PWR_REG_MD_DDR_EN_0_DBC_EN] = "reg_md_ddr_en_0_dbc_en",
	[PWR_REG_MD_DDR_EN_1_DBC_EN] = "reg_md_ddr_en_1_dbc_en",
	[PWR_REG_CONN_DDR_EN_DBC_EN] = "reg_conn_ddr_en_dbc_en",
	[PWR_REG_MD32_MASK_B] = "reg_md32_mask_b",
	[PWR_REG_MD_0_MASK_B] = "reg_md_0_mask_b",
	[PWR_REG_MD_1_MASK_B] = "reg_md_1_mask_b",
	[PWR_REG_SCP_MASK_B] = "reg_scp_mask_b",
	[PWR_REG_SRCCLKENI0_MASK_B] = "reg_srcclkeni0_mask_b",
	[PWR_REG_SRCCLKENI1_MASK_B] = "reg_srcclkeni1_mask_b",
	[PWR_REG_MD_APSRC_1_SEL] = "reg_md_apsrc_1_sel",
	[PWR_REG_MD_APSRC_0_SEL] = "reg_md_apsrc_0_sel",
	[PWR_REG_CONN_MASK_B] = "reg_conn_mask_b",
	[PWR_REG_CONN_APSRC_SEL] = "reg_conn_apsrc_sel",
	[PWR_REG_SPM_APSRC_REQ] = "reg_spm_apsrc_req",
	[PWR_REG_SPM_F26M_REQ] = "reg_spm_f26m_req",
	[PWR_REG_SPM_INFRA_REQ] = "reg_spm_infra_req",
	[PWR_REG_SPM_DDREN_REQ] = "reg_spm_ddren_req",
	[PWR_REG_SPM_VRF18_REQ] = "reg_spm_vrf18_req",
	[PWR_REG_SPM_DVFS_LEVEL0_REQ] = "reg_spm_dvfs_level0_req",
	[PWR_REG_SPM_DVFS_LEVEL1_REQ] = "reg_spm_dvfs_level1_req",
	[PWR_REG_SPM_DVFS_LEVEL2_REQ] = "reg_spm_dvfs_level2_req",
	[PWR_REG_SPM_DVFS_LEVEL3_REQ] = "reg_spm_dvfs_level3_req",
	[PWR_REG_SPM_DVFS_LEVEL4_REQ] = "reg_spm_dvfs_level4_req",
	[PWR_REG_SPM_PMCU_MAILBOX_REQ] = "reg_spm_pmcu_mailbox_req",
	[PWR_REG_SPM_SW_MAILBOX_REQ] = "reg_spm_sw_mailbox_req",
	[PWR_REG_SPM_CKSEL2_REQ] = "reg_spm_cksel2_req",
	[PWR_REG_SPM_CKSEL3_REQ] = "reg_spm_cksel3_req",
	[PWR_REG_CSYSPWREQ_MASK] = "reg_csyspwreq_mask",
	[PWR_REG_MD_SRCCLKENA_0_INFRA_MASK_B] = "reg_md_srcclkena_0_infra_mask_b",
	[PWR_REG_MD_SRCCLKENA_1_INFRA_MASK_B] = "reg_md_srcclkena_1_infra_mask_b",
	[PWR_REG_MD_APSRC_REQ_0_INFRA_MASK_B] = "reg_md_apsrc_req_0_infra_mask_b",
	[PWR_REG_MD_APSRC_REQ_1_INFRA_MASK_B] = "reg_md_apsrc_req_1_infra_mask_b",
	[PWR_REG_CONN_SRCCLKENA_INFRA_MASK_B] = "reg_conn_srcclkena_infra_mask_b",
	[PWR_REG_CONN_INFRA_REQ_MASK_B] = "reg_conn_infra_req_mask_b",
	[PWR_REG_MD32_SRCCLKENA_INFRA_MASK_B] = "reg_md32_srcclkena_infra_mask_b",
	[PWR_REG_MD32_INFRA_REQ_MASK_B] = "reg_md32_infra_req_mask_b",
	[PWR_REG_SCP_SRCCLKENA_INFRA_MASK_B] = "reg_scp_srcclkena_infra_mask_b",
	[PWR_REG_SCP_INFRA_REQ_MASK_B] = "reg_scp_infra_req_mask_b",
	[PWR_REG_SRCCLKENI0_INFRA_MASK_B] = "reg_srcclkeni0_infra_mask_b",
	[PWR_REG_SRCCLKENI1_INFRA_MASK_B] = "reg_srcclkeni1_infra_mask_b",
	[PWR_REG_CCIF0_MD_EVENT_MASK_B] = "reg_ccif0_md_event_mask_b",
	[PWR_REG_CCIF0_AP_EVENT_MASK_B] = "reg_ccif0_ap_event_mask_b",
	[PWR_REG_CCIF1_MD_EVENT_MASK_B] = "reg_ccif1_md_event_mask_b",
	[PWR_REG_CCIF1_AP_EVENT_MASK_B] = "reg_ccif1_ap_event_mask_b",
	[PWR_REG_CCIF2_MD_EVENT_MASK_B] = "reg_ccif2_md_event_mask_b",
	[PWR_REG_CCIF2_AP_EVENT_MASK_B] = "reg_ccif2_ap_event_mask_b",
	[PWR_REG_CCIF3_MD_EVENT_MASK_B] = "reg_ccif3_md_event_mask_b",
	[PWR_REG_CCIF3_AP_EVENT_MASK_B] = "reg_ccif3_ap_event_mask_b",
	[PWR_REG_CCIFMD_MD1_EVENT_MASK_B] = "reg_ccifmd_md1_event_mask_b",
	[PWR_REG_CCIFMD_MD2_EVENT_MASK_B] = "reg_ccifmd_md2_event_mask_b",
	[PWR_REG_C2K_PS_RCCIF_WAKE_MASK_B] = "reg_c2k_ps_rccif_wake_mask_b",
	[PWR_REG_C2K_L1_RCCIF_WAKE_MASK_B] = "reg_c2k_l1_rccif_wake_mask_b",
	[PWR_REG_PS_C2K_RCCIF_WAKE_MASK_B] = "reg_ps_c2k_rccif_wake_mask_b",
	[PWR_REG_L1_C2K_RCCIF_WAKE_MASK_B] = "reg_l1_c2k_rccif_wake_mask_b",
	[PWR_REG_DQSSOC_REQ_MASK_B] = "reg_dqssoc_req_mask_b",
	[PWR_REG_DISP2_REQ_MASK_B] = "reg_disp2_req_mask_b",
	[PWR_REG_MD_DDR_EN_0_MASK_B] = "reg_md_ddr_en_0_mask_b",
	[PWR_REG_MD_DDR_EN_1_MASK_B] = "reg_md_ddr_en_1_mask_b",
	[PWR_REG_CONN_DDR_EN_MASK_B] = "reg_conn_ddr_en_mask_b",
	[PWR_REG_DISP0_REQ_MASK_B] = "reg_disp0_req_mask_b",
	[PWR_REG_DISP1_REQ_MASK_B] = "reg_disp1_req_mask_b",
	[PWR_REG_DISP_OD_REQ_MASK_B] = "reg_disp_od_req_mask_b",
	[PWR_REG_MFG_REQ_MASK_B] = "reg_mfg_req_mask_b",
	[PWR_REG_VDEC0_REQ_MASK_B] = "reg_vdec0_req_mask_b",
	[PWR_REG_GCE_VRF18_REQ_MASK_B] = "reg_gce_vrf18_req_mask_b",
	[PWR_REG_GCE_REQ_MASK_B] = "reg_gce_req_mask_b",
	[PWR_REG_LPDMA_REQ_MASK_B] = "reg_lpdma_req_mask_b",
	[PWR_REG_SRCCLKENI1_CKSEL2_MASK_B] = "reg_srcclkeni1_cksel2_mask_b",
	[PWR_REG_CONN_SRCCLKENA_CKSEL2_MASK_B] = "reg_conn_srcclkena_cksel2_mask_b",
	[PWR_REG_SRCCLKENI0_CKSEL3_MASK_B] = "reg_srcclkeni0_cksel3_mask_b",
	[PWR_REG_MD32_APSRC_REQ_DDREN_MASK_B] = "reg_md32_apsrc_req_ddren_mask_b",
	[PWR_REG_SCP_APSRC_REQ_DDREN_MASK_B] = "reg_scp_apsrc_req_ddren_mask_b",
	[PWR_REG_MD_VRF18_REQ_0_MASK_B] = "reg_md_vrf18_req_0_mask_b",
	[PWR_REG_MD_VRF18_REQ_1_MASK_B] = "reg_md_vrf18_req_1_mask_b",
	[PWR_REG_NEXT_DVFS_LEVEL0_MASK_B] = "reg_next_dvfs_level0_mask_b",
	[PWR_REG_NEXT_DVFS_LEVEL1_MASK_B] = "reg_next_dvfs_level1_mask_b",
	[PWR_REG_NEXT_DVFS_LEVEL2_MASK_B] = "reg_next_dvfs_level2_mask_b",
	[PWR_REG_NEXT_DVFS_LEVEL3_MASK_B] = "reg_next_dvfs_level3_mask_b",
	[PWR_REG_NEXT_DVFS_LEVEL4_MASK_B] = "reg_next_dvfs_level4_mask_b",
	[PWR_REG_MSDC1_DVFS_HALT_MASK] = "reg_msdc1_dvfs_halt_mask",
	[PWR_REG_MSDC2_DVFS_HALT_MASK] = "reg_msdc2_dvfs_halt_mask",
	[PWR_REG_MSDC3_DVFS_HALT_MASK] = "reg_msdc3_dvfs_halt_mask",
	[PWR_REG_SW2SPM_INT0_MASK_B] = "reg_sw2spm_int0_mask_b",
	[PWR_REG_SW2SPM_INT1_MASK_B] = "reg_sw2spm_int1_mask_b",
	[PWR_REG_SW2SPM_INT2_MASK_B] = "reg_sw2spm_int2_mask_b",
	[PWR_REG_SW2SPM_INT3_MASK_B] = "reg_sw2spm_int3_mask_b",
	[PWR_REG_PMCU2SPM_INT0_MASK_B] = "reg_pmcu2spm_int0_mask_b",
	[PWR_REG_PMCU2SPM_INT1_MASK_B] = "reg_pmcu2spm_int1_mask_b",
	[PWR_REG_PMCU2SPM_INT2_MASK_B] = "reg_pmcu2spm_int2_mask_b",
	[PWR_REG_PMCU2SPM_INT3_MASK_B] = "reg_pmcu2spm_int3_mask_b",
	[PWR_REG_WAKEUP_EVENT_MASK] = "reg_wakeup_event_mask",
	[PWR_REG_EXT_WAKEUP_EVENT_MASK] = "reg_ext_wakeup_event_mask",
	[PWR_MP0_CPU0_WFI_EN] = "mp0_cpu0_wfi_en",
	[PWR_MP0_CPU1_WFI_EN] = "mp0_cpu1_wfi_en",
	[PWR_MP0_CPU2_WFI_EN] = "mp0_cpu2_wfi_en",
	[PWR_MP0_CPU3_WFI_EN] = "mp0_cpu3_wfi_en",
	[PWR_MP1_CPU0_WFI_EN] = "mp1_cpu0_wfi_en",
	[PWR_MP1_CPU1_WFI_EN] = "mp1_cpu1_wfi_en",
	[PWR_MP1_CPU2_WFI_EN] = "mp1_cpu2_wfi_en",
	[PWR_MP1_CPU3_WFI_EN] = "mp1_cpu3_wfi_en",
	[PWR_DEBUG0_WFI_EN] = "debug0_wfi_en",
	[PWR_DEBUG1_WFI_EN] = "debug1_wfi_en",
	[PWR_DEBUG2_WFI_EN] = "debug2_wfi_en",
	[PWR_DEBUG3_WFI_EN] = "debug3_wfi_en",
};

/**************************************
 * xxx_ctrl_show Function
 **************************************/
/* code gen by spm_pwr_ctrl.pl, need struct pwr_ctrl */
static ssize_t show_pwr_ctrl(const struct pwr_ctrl *pwrctrl, char *buf)
{
	char *p = buf;

	p += sprintf(p, "pcm_flags = 0x%x\n", pwrctrl->pcm_flags);
	p += sprintf(p, "pcm_flags_cust = 0x%x\n", pwrctrl->pcm_flags_cust);
	p += sprintf(p, "pcm_flags_cust_set = 0x%x\n", pwrctrl->pcm_flags_cust_set);
	p += sprintf(p, "pcm_flags_cust_clr = 0x%x\n", pwrctrl->pcm_flags_cust_clr);
	p += sprintf(p, "pcm_reserve = 0x%x\n", pwrctrl->pcm_reserve);
	p += sprintf(p, "timer_val = 0x%x\n", pwrctrl->timer_val);
	p += sprintf(p, "timer_val_cust = 0x%x\n", pwrctrl->timer_val_cust);
	p += sprintf(p, "timer_val_ramp_en = 0x%x\n", pwrctrl->timer_val_ramp_en);
	p += sprintf(p, "timer_val_ramp_en_sec = 0x%x\n", pwrctrl->timer_val_ramp_en_sec);
	p += sprintf(p, "wake_src = 0x%x\n", pwrctrl->wake_src);
	p += sprintf(p, "wake_src_cust = 0x%x\n", pwrctrl->wake_src_cust);
	p += sprintf(p, "wake_src_md32 = 0x%x\n", pwrctrl->wake_src_md32);
	p += sprintf(p, "wdt_disable = 0x%x\n", pwrctrl->wdt_disable);
	p += sprintf(p, "dvfs_halt_src_chk = 0x%x\n", pwrctrl->dvfs_halt_src_chk);
	p += sprintf(p, "syspwreq_mask = 0x%x\n", pwrctrl->syspwreq_mask);
	p += sprintf(p, "reg_srcclken0_ctl = 0x%x\n", pwrctrl->reg_srcclken0_ctl);
	p += sprintf(p, "reg_srcclken1_ctl = 0x%x\n", pwrctrl->reg_srcclken1_ctl);
	p += sprintf(p, "reg_spm_lock_infra_dcm = 0x%x\n", pwrctrl->reg_spm_lock_infra_dcm);
	p += sprintf(p, "reg_srcclken_mask = 0x%x\n", pwrctrl->reg_srcclken_mask);
	p += sprintf(p, "reg_md1_c32rm_en = 0x%x\n", pwrctrl->reg_md1_c32rm_en);
	p += sprintf(p, "reg_md2_c32rm_en = 0x%x\n", pwrctrl->reg_md2_c32rm_en);
	p += sprintf(p, "reg_clksq0_sel_ctrl = 0x%x\n", pwrctrl->reg_clksq0_sel_ctrl);
	p += sprintf(p, "reg_clksq1_sel_ctrl = 0x%x\n", pwrctrl->reg_clksq1_sel_ctrl);
	p += sprintf(p, "reg_srcclken0_en = 0x%x\n", pwrctrl->reg_srcclken0_en);
	p += sprintf(p, "reg_srcclken1_en = 0x%x\n", pwrctrl->reg_srcclken1_en);
	p += sprintf(p, "reg_sysclk0_src_mask_b = 0x%x\n", pwrctrl->reg_sysclk0_src_mask_b);
	p += sprintf(p, "reg_sysclk1_src_mask_b = 0x%x\n", pwrctrl->reg_sysclk1_src_mask_b);
	p += sprintf(p, "reg_mpwfi_op = 0x%x\n", pwrctrl->reg_mpwfi_op);
	p += sprintf(p, "reg_mp0_cputop_idle_mask = 0x%x\n", pwrctrl->reg_mp0_cputop_idle_mask);
	p += sprintf(p, "reg_mp1_cputop_idle_mask = 0x%x\n", pwrctrl->reg_mp1_cputop_idle_mask);
	p += sprintf(p, "reg_debugtop_idle_mask = 0x%x\n", pwrctrl->reg_debugtop_idle_mask);
	p += sprintf(p, "reg_mp_top_idle_mask = 0x%x\n", pwrctrl->reg_mp_top_idle_mask);
	p += sprintf(p, "reg_mcusys_idle_mask = 0x%x\n", pwrctrl->reg_mcusys_idle_mask);
	p += sprintf(p, "reg_md_ddr_en_0_dbc_en = 0x%x\n", pwrctrl->reg_md_ddr_en_0_dbc_en);
	p += sprintf(p, "reg_md_ddr_en_1_dbc_en = 0x%x\n", pwrctrl->reg_md_ddr_en_1_dbc_en);
	p += sprintf(p, "reg_conn_ddr_en_dbc_en = 0x%x\n", pwrctrl->reg_conn_ddr_en_dbc_en);
	p += sprintf(p, "reg_md32_mask_b = 0x%x\n", pwrctrl->reg_md32_mask_b);
	p += sprintf(p, "reg_md_0_mask_b = 0x%x\n", pwrctrl->reg_md_0_mask_b);
	p += sprintf(p, "reg_md_1_mask_b = 0x%x\n", pwrctrl->reg_md_1_mask_b);
	p += sprintf(p, "reg_scp_mask_b = 0x%x\n", pwrctrl->reg_scp_mask_b);
	p += sprintf(p, "reg_srcclkeni0_mask_b = 0x%x\n", pwrctrl->reg_srcclkeni0_mask_b);
	p += sprintf(p, "reg_srcclkeni1_mask_b = 0x%x\n", pwrctrl->reg_srcclkeni1_mask_b);
	p += sprintf(p, "reg_md_apsrc_1_sel = 0x%x\n", pwrctrl->reg_md_apsrc_1_sel);
	p += sprintf(p, "reg_md_apsrc_0_sel = 0x%x\n", pwrctrl->reg_md_apsrc_0_sel);
	p += sprintf(p, "reg_conn_mask_b = 0x%x\n", pwrctrl->reg_conn_mask_b);
	p += sprintf(p, "reg_conn_apsrc_sel = 0x%x\n", pwrctrl->reg_conn_apsrc_sel);
	p += sprintf(p, "reg_spm_apsrc_req = 0x%x\n", pwrctrl->reg_spm_apsrc_req);
	p += sprintf(p, "reg_spm_f26m_req = 0x%x\n", pwrctrl->reg_spm_f26m_req);
	p += sprintf(p, "reg_spm_infra_req = 0x%x\n", pwrctrl->reg_spm_infra_req);
	p += sprintf(p, "reg_spm_ddren_req = 0x%x\n", pwrctrl->reg_spm_ddren_req);
	p += sprintf(p, "reg_spm_vrf18_req = 0x%x\n", pwrctrl->reg_spm_vrf18_req);
	p += sprintf(p, "reg_spm_dvfs_level0_req = 0x%x\n", pwrctrl->reg_spm_dvfs_level0_req);
	p += sprintf(p, "reg_spm_dvfs_level1_req = 0x%x\n", pwrctrl->reg_spm_dvfs_level1_req);
	p += sprintf(p, "reg_spm_dvfs_level2_req = 0x%x\n", pwrctrl->reg_spm_dvfs_level2_req);
	p += sprintf(p, "reg_spm_dvfs_level3_req = 0x%x\n", pwrctrl->reg_spm_dvfs_level3_req);
	p += sprintf(p, "reg_spm_dvfs_level4_req = 0x%x\n", pwrctrl->reg_spm_dvfs_level4_req);
	p += sprintf(p, "reg_spm_pmcu_mailbox_req = 0x%x\n", pwrctrl->reg_spm_pmcu_mailbox_req);
	p += sprintf(p, "reg_spm_sw_mailbox_req = 0x%x\n", pwrctrl->reg_spm_sw_mailbox_req);
	p += sprintf(p, "reg_spm_cksel2_req = 0x%x\n", pwrctrl->reg_spm_cksel2_req);
	p += sprintf(p, "reg_spm_cksel3_req = 0x%x\n", pwrctrl->reg_spm_cksel3_req);
	p += sprintf(p, "reg_csyspwreq_mask = 0x%x\n", pwrctrl->reg_csyspwreq_mask);
	p += sprintf(p, "reg_md_srcclkena_0_infra_mask_b = 0x%x\n", pwrctrl->reg_md_srcclkena_0_infra_mask_b);
	p += sprintf(p, "reg_md_srcclkena_1_infra_mask_b = 0x%x\n", pwrctrl->reg_md_srcclkena_1_infra_mask_b);
	p += sprintf(p, "reg_md_apsrc_req_0_infra_mask_b = 0x%x\n", pwrctrl->reg_md_apsrc_req_0_infra_mask_b);
	p += sprintf(p, "reg_md_apsrc_req_1_infra_mask_b = 0x%x\n", pwrctrl->reg_md_apsrc_req_1_infra_mask_b);
	p += sprintf(p, "reg_conn_srcclkena_infra_mask_b = 0x%x\n", pwrctrl->reg_conn_srcclkena_infra_mask_b);
	p += sprintf(p, "reg_conn_infra_req_mask_b = 0x%x\n", pwrctrl->reg_conn_infra_req_mask_b);
	p += sprintf(p, "reg_md32_srcclkena_infra_mask_b = 0x%x\n", pwrctrl->reg_md32_srcclkena_infra_mask_b);
	p += sprintf(p, "reg_md32_infra_req_mask_b = 0x%x\n", pwrctrl->reg_md32_infra_req_mask_b);
	p += sprintf(p, "reg_scp_srcclkena_infra_mask_b = 0x%x\n", pwrctrl->reg_scp_srcclkena_infra_mask_b);
	p += sprintf(p, "reg_scp_infra_req_mask_b = 0x%x\n", pwrctrl->reg_scp_infra_req_mask_b);
	p += sprintf(p, "reg_srcclkeni0_infra_mask_b = 0x%x\n", pwrctrl->reg_srcclkeni0_infra_mask_b);
	p += sprintf(p, "reg_srcclkeni1_infra_mask_b = 0x%x\n", pwrctrl->reg_srcclkeni1_infra_mask_b);
	p += sprintf(p, "reg_ccif0_md_event_mask_b = 0x%x\n", pwrctrl->reg_ccif0_md_event_mask_b);
	p += sprintf(p, "reg_ccif0_ap_event_mask_b = 0x%x\n", pwrctrl->reg_ccif0_ap_event_mask_b);
	p += sprintf(p, "reg_ccif1_md_event_mask_b = 0x%x\n", pwrctrl->reg_ccif1_md_event_mask_b);
	p += sprintf(p, "reg_ccif1_ap_event_mask_b = 0x%x\n", pwrctrl->reg_ccif1_ap_event_mask_b);
	p += sprintf(p, "reg_ccif2_md_event_mask_b = 0x%x\n", pwrctrl->reg_ccif2_md_event_mask_b);
	p += sprintf(p, "reg_ccif2_ap_event_mask_b = 0x%x\n", pwrctrl->reg_ccif2_ap_event_mask_b);
	p += sprintf(p, "reg_ccif3_md_event_mask_b = 0x%x\n", pwrctrl->reg_ccif3_md_event_mask_b);
	p += sprintf(p, "reg_ccif3_ap_event_mask_b = 0x%x\n", pwrctrl->reg_ccif3_ap_event_mask_b);
	p += sprintf(p, "reg_ccifmd_md1_event_mask_b = 0x%x\n", pwrctrl->reg_ccifmd_md1_event_mask_b);
	p += sprintf(p, "reg_ccifmd_md2_event_mask_b = 0x%x\n", pwrctrl->reg_ccifmd_md2_event_mask_b);
	p += sprintf(p, "reg_c2k_ps_rccif_wake_mask_b = 0x%x\n", pwrctrl->reg_c2k_ps_rccif_wake_mask_b);
	p += sprintf(p, "reg_c2k_l1_rccif_wake_mask_b = 0x%x\n", pwrctrl->reg_c2k_l1_rccif_wake_mask_b);
	p += sprintf(p, "reg_ps_c2k_rccif_wake_mask_b = 0x%x\n", pwrctrl->reg_ps_c2k_rccif_wake_mask_b);
	p += sprintf(p, "reg_l1_c2k_rccif_wake_mask_b = 0x%x\n", pwrctrl->reg_l1_c2k_rccif_wake_mask_b);
	p += sprintf(p, "reg_dqssoc_req_mask_b = 0x%x\n", pwrctrl->reg_dqssoc_req_mask_b);
	p += sprintf(p, "reg_disp2_req_mask_b = 0x%x\n", pwrctrl->reg_disp2_req_mask_b);
	p += sprintf(p, "reg_md_ddr_en_0_mask_b = 0x%x\n", pwrctrl->reg_md_ddr_en_0_mask_b);
	p += sprintf(p, "reg_md_ddr_en_1_mask_b = 0x%x\n", pwrctrl->reg_md_ddr_en_1_mask_b);
	p += sprintf(p, "reg_conn_ddr_en_mask_b = 0x%x\n", pwrctrl->reg_conn_ddr_en_mask_b);
	p += sprintf(p, "reg_disp0_req_mask_b = 0x%x\n", pwrctrl->reg_disp0_req_mask_b);
	p += sprintf(p, "reg_disp1_req_mask_b = 0x%x\n", pwrctrl->reg_disp1_req_mask_b);
	p += sprintf(p, "reg_disp_od_req_mask_b = 0x%x\n", pwrctrl->reg_disp_od_req_mask_b);
	p += sprintf(p, "reg_mfg_req_mask_b = 0x%x\n", pwrctrl->reg_mfg_req_mask_b);
	p += sprintf(p, "reg_vdec0_req_mask_b = 0x%x\n", pwrctrl->reg_vdec0_req_mask_b);
	p += sprintf(p, "reg_gce_vrf18_req_mask_b = 0x%x\n", pwrctrl->reg_gce_vrf18_req_mask_b);
	p += sprintf(p, "reg_gce_req_mask_b = 0x%x\n", pwrctrl->reg_gce_req_mask_b);
	p += sprintf(p, "reg_lpdma_req_mask_b = 0x%x\n", pwrctrl->reg_lpdma_req_mask_b);
	p += sprintf(p, "reg_srcclkeni1_cksel2_mask_b = 0x%x\n", pwrctrl->reg_srcclkeni1_cksel2_mask_b);
	p += sprintf(p, "reg_conn_srcclkena_cksel2_mask_b = 0x%x\n", pwrctrl->reg_conn_srcclkena_cksel2_mask_b);
	p += sprintf(p, "reg_srcclkeni0_cksel3_mask_b = 0x%x\n", pwrctrl->reg_srcclkeni0_cksel3_mask_b);
	p += sprintf(p, "reg_md32_apsrc_req_ddren_mask_b = 0x%x\n", pwrctrl->reg_md32_apsrc_req_ddren_mask_b);
	p += sprintf(p, "reg_scp_apsrc_req_ddren_mask_b = 0x%x\n", pwrctrl->reg_scp_apsrc_req_ddren_mask_b);
	p += sprintf(p, "reg_md_vrf18_req_0_mask_b = 0x%x\n", pwrctrl->reg_md_vrf18_req_0_mask_b);
	p += sprintf(p, "reg_md_vrf18_req_1_mask_b = 0x%x\n", pwrctrl->reg_md_vrf18_req_1_mask_b);
	p += sprintf(p, "reg_next_dvfs_level0_mask_b = 0x%x\n", pwrctrl->reg_next_dvfs_level0_mask_b);
	p += sprintf(p, "reg_next_dvfs_level1_mask_b = 0x%x\n", pwrctrl->reg_next_dvfs_level1_mask_b);
	p += sprintf(p, "reg_next_dvfs_level2_mask_b = 0x%x\n", pwrctrl->reg_next_dvfs_level2_mask_b);
	p += sprintf(p, "reg_next_dvfs_level3_mask_b = 0x%x\n", pwrctrl->reg_next_dvfs_level3_mask_b);
	p += sprintf(p, "reg_next_dvfs_level4_mask_b = 0x%x\n", pwrctrl->reg_next_dvfs_level4_mask_b);
	p += sprintf(p, "reg_msdc1_dvfs_halt_mask = 0x%x\n", pwrctrl->reg_msdc1_dvfs_halt_mask);
	p += sprintf(p, "reg_msdc2_dvfs_halt_mask = 0x%x\n", pwrctrl->reg_msdc2_dvfs_halt_mask);
	p += sprintf(p, "reg_msdc3_dvfs_halt_mask = 0x%x\n", pwrctrl->reg_msdc3_dvfs_halt_mask);
	p += sprintf(p, "reg_sw2spm_int0_mask_b = 0x%x\n", pwrctrl->reg_sw2spm_int0_mask_b);
	p += sprintf(p, "reg_sw2spm_int1_mask_b = 0x%x\n", pwrctrl->reg_sw2spm_int1_mask_b);
	p += sprintf(p, "reg_sw2spm_int2_mask_b = 0x%x\n", pwrctrl->reg_sw2spm_int2_mask_b);
	p += sprintf(p, "reg_sw2spm_int3_mask_b = 0x%x\n", pwrctrl->reg_sw2spm_int3_mask_b);
	p += sprintf(p, "reg_pmcu2spm_int0_mask_b = 0x%x\n", pwrctrl->reg_pmcu2spm_int0_mask_b);
	p += sprintf(p, "reg_pmcu2spm_int1_mask_b = 0x%x\n", pwrctrl->reg_pmcu2spm_int1_mask_b);
	p += sprintf(p, "reg_pmcu2spm_int2_mask_b = 0x%x\n", pwrctrl->reg_pmcu2spm_int2_mask_b);
	p += sprintf(p, "reg_pmcu2spm_int3_mask_b = 0x%x\n", pwrctrl->reg_pmcu2spm_int3_mask_b);
	p += sprintf(p, "reg_wakeup_event_mask = 0x%x\n", pwrctrl->reg_wakeup_event_mask);
	p += sprintf(p, "reg_ext_wakeup_event_mask = 0x%x\n", pwrctrl->reg_ext_wakeup_event_mask);
	p += sprintf(p, "mp0_cpu0_wfi_en = 0x%x\n", pwrctrl->mp0_cpu0_wfi_en);
	p += sprintf(p, "mp0_cpu1_wfi_en = 0x%x\n", pwrctrl->mp0_cpu1_wfi_en);
	p += sprintf(p, "mp0_cpu2_wfi_en = 0x%x\n", pwrctrl->mp0_cpu2_wfi_en);
	p += sprintf(p, "mp0_cpu3_wfi_en = 0x%x\n", pwrctrl->mp0_cpu3_wfi_en);
	p += sprintf(p, "mp1_cpu0_wfi_en = 0x%x\n", pwrctrl->mp1_cpu0_wfi_en);
	p += sprintf(p, "mp1_cpu1_wfi_en = 0x%x\n", pwrctrl->mp1_cpu1_wfi_en);
	p += sprintf(p, "mp1_cpu2_wfi_en = 0x%x\n", pwrctrl->mp1_cpu2_wfi_en);
	p += sprintf(p, "mp1_cpu3_wfi_en = 0x%x\n", pwrctrl->mp1_cpu3_wfi_en);
	p += sprintf(p, "debug0_wfi_en = 0x%x\n", pwrctrl->debug0_wfi_en);
	p += sprintf(p, "debug1_wfi_en = 0x%x\n", pwrctrl->debug1_wfi_en);
	p += sprintf(p, "debug2_wfi_en = 0x%x\n", pwrctrl->debug2_wfi_en);
	p += sprintf(p, "debug3_wfi_en = 0x%x\n", pwrctrl->debug3_wfi_en);

	BUG_ON(p - buf >= PAGE_SIZE);

	return p - buf;
}

static ssize_t suspend_ctrl_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return show_pwr_ctrl(__spm_suspend.pwrctrl, buf);
}

static ssize_t dpidle_ctrl_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return show_pwr_ctrl(__spm_dpidle.pwrctrl, buf);
}

static ssize_t sodi3_ctrl_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return show_pwr_ctrl(__spm_sodi3.pwrctrl, buf);
}

static ssize_t sodi_ctrl_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return show_pwr_ctrl(__spm_sodi.pwrctrl, buf);
}

#ifndef CONFIG_MTK_FPGA
static ssize_t mcdi_ctrl_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t vcore_dvfs_ctrl_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return show_pwr_ctrl(__spm_vcore_dvfs.pwrctrl, buf);
}
#endif


/**************************************
 * xxx_ctrl_store Function
 **************************************/
/* code gen by spm_pwr_ctrl.pl, need struct pwr_ctrl */
static ssize_t store_pwr_ctrl(int id, struct pwr_ctrl *pwrctrl, const char *buf, size_t count)
{
	u32 val;
	char cmd[32];
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	struct spm_data spm_d;
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */

	if (sscanf(buf, "%31s %x", cmd, &val) != 2)
		return -EPERM;

	spm_debug("pwr_ctrl: cmd = %s, val = 0x%x\n", cmd, val);

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	spm_d.u.pwr_ctrl.val = val;
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */

	if (!strcmp(cmd, pwr_ctrl_str[PWR_PCM_FLAGS])) {
		pwrctrl->pcm_flags = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_PCM_FLAGS;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_PCM_FLAGS_CUST])) {
		pwrctrl->pcm_flags_cust = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_PCM_FLAGS_CUST;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_PCM_FLAGS_CUST_SET])) {
		pwrctrl->pcm_flags_cust_set = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_PCM_FLAGS_CUST_SET;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_PCM_FLAGS_CUST_CLR])) {
		pwrctrl->pcm_flags_cust_clr = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_PCM_FLAGS_CUST_CLR;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_PCM_RESERVE])) {
		pwrctrl->pcm_reserve = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_PCM_RESERVE;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_TIMER_VAL])) {
		pwrctrl->timer_val = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_TIMER_VAL;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_TIMER_VAL_CUST])) {
		pwrctrl->timer_val_cust = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_TIMER_VAL_CUST;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_TIMER_VAL_RAMP_EN])) {
		pwrctrl->timer_val_ramp_en = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_TIMER_VAL_RAMP_EN;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_TIMER_VAL_RAMP_EN_SEC])) {
		pwrctrl->timer_val_ramp_en_sec = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_TIMER_VAL_RAMP_EN_SEC;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_WAKE_SRC])) {
		pwrctrl->wake_src = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_WAKE_SRC;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_WAKE_SRC_CUST])) {
		pwrctrl->wake_src_cust = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_WAKE_SRC_CUST;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_WAKE_SRC_MD32])) {
		pwrctrl->wake_src_md32 = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_WAKE_SRC_MD32;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_WDT_DISABLE])) {
		pwrctrl->wdt_disable = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_WDT_DISABLE;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_DVFS_HALT_SRC_CHK])) {
		pwrctrl->dvfs_halt_src_chk = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_DVFS_HALT_SRC_CHK;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_SYSPWREQ_MASK])) {
		pwrctrl->syspwreq_mask = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_SYSPWREQ_MASK;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_SRCCLKEN0_CTL])) {
		pwrctrl->reg_srcclken0_ctl = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_SRCCLKEN0_CTL;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_SRCCLKEN1_CTL])) {
		pwrctrl->reg_srcclken1_ctl = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_SRCCLKEN1_CTL;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_SPM_LOCK_INFRA_DCM])) {
		pwrctrl->reg_spm_lock_infra_dcm = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_SPM_LOCK_INFRA_DCM;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_SRCCLKEN_MASK])) {
		pwrctrl->reg_srcclken_mask = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_SRCCLKEN_MASK;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_MD1_C32RM_EN])) {
		pwrctrl->reg_md1_c32rm_en = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_MD1_C32RM_EN;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_MD2_C32RM_EN])) {
		pwrctrl->reg_md2_c32rm_en = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_MD2_C32RM_EN;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_CLKSQ0_SEL_CTRL])) {
		pwrctrl->reg_clksq0_sel_ctrl = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_CLKSQ0_SEL_CTRL;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_CLKSQ1_SEL_CTRL])) {
		pwrctrl->reg_clksq1_sel_ctrl = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_CLKSQ1_SEL_CTRL;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_SRCCLKEN0_EN])) {
		pwrctrl->reg_srcclken0_en = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_SRCCLKEN0_EN;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_SRCCLKEN1_EN])) {
		pwrctrl->reg_srcclken1_en = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_SRCCLKEN1_EN;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_SYSCLK0_SRC_MASK_B])) {
		pwrctrl->reg_sysclk0_src_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_SYSCLK0_SRC_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_SYSCLK1_SRC_MASK_B])) {
		pwrctrl->reg_sysclk1_src_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_SYSCLK1_SRC_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_MPWFI_OP])) {
		pwrctrl->reg_mpwfi_op = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_MPWFI_OP;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_MP0_CPUTOP_IDLE_MASK])) {
		pwrctrl->reg_mp0_cputop_idle_mask = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_MP0_CPUTOP_IDLE_MASK;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_MP1_CPUTOP_IDLE_MASK])) {
		pwrctrl->reg_mp1_cputop_idle_mask = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_MP1_CPUTOP_IDLE_MASK;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_DEBUGTOP_IDLE_MASK])) {
		pwrctrl->reg_debugtop_idle_mask = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_DEBUGTOP_IDLE_MASK;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_MP_TOP_IDLE_MASK])) {
		pwrctrl->reg_mp_top_idle_mask = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_MP_TOP_IDLE_MASK;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_MCUSYS_IDLE_MASK])) {
		pwrctrl->reg_mcusys_idle_mask = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_MCUSYS_IDLE_MASK;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_MD_DDR_EN_0_DBC_EN])) {
		pwrctrl->reg_md_ddr_en_0_dbc_en = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_MD_DDR_EN_0_DBC_EN;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_MD_DDR_EN_1_DBC_EN])) {
		pwrctrl->reg_md_ddr_en_1_dbc_en = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_MD_DDR_EN_1_DBC_EN;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_CONN_DDR_EN_DBC_EN])) {
		pwrctrl->reg_conn_ddr_en_dbc_en = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_CONN_DDR_EN_DBC_EN;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_MD32_MASK_B])) {
		pwrctrl->reg_md32_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_MD32_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_MD_0_MASK_B])) {
		pwrctrl->reg_md_0_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_MD_0_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_MD_1_MASK_B])) {
		pwrctrl->reg_md_1_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_MD_1_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_SCP_MASK_B])) {
		pwrctrl->reg_scp_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_SCP_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_SRCCLKENI0_MASK_B])) {
		pwrctrl->reg_srcclkeni0_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_SRCCLKENI0_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_SRCCLKENI1_MASK_B])) {
		pwrctrl->reg_srcclkeni1_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_SRCCLKENI1_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_MD_APSRC_1_SEL])) {
		pwrctrl->reg_md_apsrc_1_sel = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_MD_APSRC_1_SEL;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_MD_APSRC_0_SEL])) {
		pwrctrl->reg_md_apsrc_0_sel = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_MD_APSRC_0_SEL;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_CONN_MASK_B])) {
		pwrctrl->reg_conn_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_CONN_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_CONN_APSRC_SEL])) {
		pwrctrl->reg_conn_apsrc_sel = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_CONN_APSRC_SEL;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_SPM_APSRC_REQ])) {
		pwrctrl->reg_spm_apsrc_req = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_SPM_APSRC_REQ;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_SPM_F26M_REQ])) {
		pwrctrl->reg_spm_f26m_req = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_SPM_F26M_REQ;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_SPM_INFRA_REQ])) {
		pwrctrl->reg_spm_infra_req = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_SPM_INFRA_REQ;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_SPM_DDREN_REQ])) {
		pwrctrl->reg_spm_ddren_req = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_SPM_DDREN_REQ;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_SPM_VRF18_REQ])) {
		pwrctrl->reg_spm_vrf18_req = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_SPM_VRF18_REQ;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_SPM_DVFS_LEVEL0_REQ])) {
		pwrctrl->reg_spm_dvfs_level0_req = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_SPM_DVFS_LEVEL0_REQ;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_SPM_DVFS_LEVEL1_REQ])) {
		pwrctrl->reg_spm_dvfs_level1_req = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_SPM_DVFS_LEVEL1_REQ;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_SPM_DVFS_LEVEL2_REQ])) {
		pwrctrl->reg_spm_dvfs_level2_req = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_SPM_DVFS_LEVEL2_REQ;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_SPM_DVFS_LEVEL3_REQ])) {
		pwrctrl->reg_spm_dvfs_level3_req = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_SPM_DVFS_LEVEL3_REQ;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_SPM_DVFS_LEVEL4_REQ])) {
		pwrctrl->reg_spm_dvfs_level4_req = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_SPM_DVFS_LEVEL4_REQ;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_SPM_PMCU_MAILBOX_REQ])) {
		pwrctrl->reg_spm_pmcu_mailbox_req = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_SPM_PMCU_MAILBOX_REQ;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_SPM_SW_MAILBOX_REQ])) {
		pwrctrl->reg_spm_sw_mailbox_req = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_SPM_SW_MAILBOX_REQ;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_SPM_CKSEL2_REQ])) {
		pwrctrl->reg_spm_cksel2_req = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_SPM_CKSEL2_REQ;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_SPM_CKSEL3_REQ])) {
		pwrctrl->reg_spm_cksel3_req = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_SPM_CKSEL3_REQ;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_CSYSPWREQ_MASK])) {
		pwrctrl->reg_csyspwreq_mask = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_CSYSPWREQ_MASK;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_MD_SRCCLKENA_0_INFRA_MASK_B])) {
		pwrctrl->reg_md_srcclkena_0_infra_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_MD_SRCCLKENA_0_INFRA_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_MD_SRCCLKENA_1_INFRA_MASK_B])) {
		pwrctrl->reg_md_srcclkena_1_infra_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_MD_SRCCLKENA_1_INFRA_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_MD_APSRC_REQ_0_INFRA_MASK_B])) {
		pwrctrl->reg_md_apsrc_req_0_infra_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_MD_APSRC_REQ_0_INFRA_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_MD_APSRC_REQ_1_INFRA_MASK_B])) {
		pwrctrl->reg_md_apsrc_req_1_infra_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_MD_APSRC_REQ_1_INFRA_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_CONN_SRCCLKENA_INFRA_MASK_B])) {
		pwrctrl->reg_conn_srcclkena_infra_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_CONN_SRCCLKENA_INFRA_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_CONN_INFRA_REQ_MASK_B])) {
		pwrctrl->reg_conn_infra_req_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_CONN_INFRA_REQ_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_MD32_SRCCLKENA_INFRA_MASK_B])) {
		pwrctrl->reg_md32_srcclkena_infra_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_MD32_SRCCLKENA_INFRA_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_MD32_INFRA_REQ_MASK_B])) {
		pwrctrl->reg_md32_infra_req_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_MD32_INFRA_REQ_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_SCP_SRCCLKENA_INFRA_MASK_B])) {
		pwrctrl->reg_scp_srcclkena_infra_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_SCP_SRCCLKENA_INFRA_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_SCP_INFRA_REQ_MASK_B])) {
		pwrctrl->reg_scp_infra_req_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_SCP_INFRA_REQ_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_SRCCLKENI0_INFRA_MASK_B])) {
		pwrctrl->reg_srcclkeni0_infra_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_SRCCLKENI0_INFRA_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_SRCCLKENI1_INFRA_MASK_B])) {
		pwrctrl->reg_srcclkeni1_infra_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_SRCCLKENI1_INFRA_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_CCIF0_MD_EVENT_MASK_B])) {
		pwrctrl->reg_ccif0_md_event_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_CCIF0_MD_EVENT_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_CCIF0_AP_EVENT_MASK_B])) {
		pwrctrl->reg_ccif0_ap_event_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_CCIF0_AP_EVENT_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_CCIF1_MD_EVENT_MASK_B])) {
		pwrctrl->reg_ccif1_md_event_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_CCIF1_MD_EVENT_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_CCIF1_AP_EVENT_MASK_B])) {
		pwrctrl->reg_ccif1_ap_event_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_CCIF1_AP_EVENT_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_CCIF2_MD_EVENT_MASK_B])) {
		pwrctrl->reg_ccif2_md_event_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_CCIF2_MD_EVENT_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_CCIF2_AP_EVENT_MASK_B])) {
		pwrctrl->reg_ccif2_ap_event_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_CCIF2_AP_EVENT_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_CCIF3_MD_EVENT_MASK_B])) {
		pwrctrl->reg_ccif3_md_event_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_CCIF3_MD_EVENT_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_CCIF3_AP_EVENT_MASK_B])) {
		pwrctrl->reg_ccif3_ap_event_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_CCIF3_AP_EVENT_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_CCIFMD_MD1_EVENT_MASK_B])) {
		pwrctrl->reg_ccifmd_md1_event_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_CCIFMD_MD1_EVENT_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_CCIFMD_MD2_EVENT_MASK_B])) {
		pwrctrl->reg_ccifmd_md2_event_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_CCIFMD_MD2_EVENT_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_C2K_PS_RCCIF_WAKE_MASK_B])) {
		pwrctrl->reg_c2k_ps_rccif_wake_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_C2K_PS_RCCIF_WAKE_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_C2K_L1_RCCIF_WAKE_MASK_B])) {
		pwrctrl->reg_c2k_l1_rccif_wake_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_C2K_L1_RCCIF_WAKE_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_PS_C2K_RCCIF_WAKE_MASK_B])) {
		pwrctrl->reg_ps_c2k_rccif_wake_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_PS_C2K_RCCIF_WAKE_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_L1_C2K_RCCIF_WAKE_MASK_B])) {
		pwrctrl->reg_l1_c2k_rccif_wake_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_L1_C2K_RCCIF_WAKE_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_DQSSOC_REQ_MASK_B])) {
		pwrctrl->reg_dqssoc_req_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_DQSSOC_REQ_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_DISP2_REQ_MASK_B])) {
		pwrctrl->reg_disp2_req_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_DISP2_REQ_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_MD_DDR_EN_0_MASK_B])) {
		pwrctrl->reg_md_ddr_en_0_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_MD_DDR_EN_0_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_MD_DDR_EN_1_MASK_B])) {
		pwrctrl->reg_md_ddr_en_1_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_MD_DDR_EN_1_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_CONN_DDR_EN_MASK_B])) {
		pwrctrl->reg_conn_ddr_en_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_CONN_DDR_EN_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_DISP0_REQ_MASK_B])) {
		pwrctrl->reg_disp0_req_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_DISP0_REQ_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_DISP1_REQ_MASK_B])) {
		pwrctrl->reg_disp1_req_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_DISP1_REQ_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_DISP_OD_REQ_MASK_B])) {
		pwrctrl->reg_disp_od_req_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_DISP_OD_REQ_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_MFG_REQ_MASK_B])) {
		pwrctrl->reg_mfg_req_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_MFG_REQ_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_VDEC0_REQ_MASK_B])) {
		pwrctrl->reg_vdec0_req_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_VDEC0_REQ_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_GCE_VRF18_REQ_MASK_B])) {
		pwrctrl->reg_gce_vrf18_req_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_GCE_VRF18_REQ_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_GCE_REQ_MASK_B])) {
		pwrctrl->reg_gce_req_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_GCE_REQ_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_LPDMA_REQ_MASK_B])) {
		pwrctrl->reg_lpdma_req_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_LPDMA_REQ_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_SRCCLKENI1_CKSEL2_MASK_B])) {
		pwrctrl->reg_srcclkeni1_cksel2_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_SRCCLKENI1_CKSEL2_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_CONN_SRCCLKENA_CKSEL2_MASK_B])) {
		pwrctrl->reg_conn_srcclkena_cksel2_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_CONN_SRCCLKENA_CKSEL2_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_SRCCLKENI0_CKSEL3_MASK_B])) {
		pwrctrl->reg_srcclkeni0_cksel3_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_SRCCLKENI0_CKSEL3_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_MD32_APSRC_REQ_DDREN_MASK_B])) {
		pwrctrl->reg_md32_apsrc_req_ddren_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_MD32_APSRC_REQ_DDREN_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_SCP_APSRC_REQ_DDREN_MASK_B])) {
		pwrctrl->reg_scp_apsrc_req_ddren_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_SCP_APSRC_REQ_DDREN_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_MD_VRF18_REQ_0_MASK_B])) {
		pwrctrl->reg_md_vrf18_req_0_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_MD_VRF18_REQ_0_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_MD_VRF18_REQ_1_MASK_B])) {
		pwrctrl->reg_md_vrf18_req_1_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_MD_VRF18_REQ_1_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_NEXT_DVFS_LEVEL0_MASK_B])) {
		pwrctrl->reg_next_dvfs_level0_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_NEXT_DVFS_LEVEL0_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_NEXT_DVFS_LEVEL1_MASK_B])) {
		pwrctrl->reg_next_dvfs_level1_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_NEXT_DVFS_LEVEL1_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_NEXT_DVFS_LEVEL2_MASK_B])) {
		pwrctrl->reg_next_dvfs_level2_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_NEXT_DVFS_LEVEL2_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_NEXT_DVFS_LEVEL3_MASK_B])) {
		pwrctrl->reg_next_dvfs_level3_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_NEXT_DVFS_LEVEL3_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_NEXT_DVFS_LEVEL4_MASK_B])) {
		pwrctrl->reg_next_dvfs_level4_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_NEXT_DVFS_LEVEL4_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_MSDC1_DVFS_HALT_MASK])) {
		pwrctrl->reg_msdc1_dvfs_halt_mask = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_MSDC1_DVFS_HALT_MASK;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_MSDC2_DVFS_HALT_MASK])) {
		pwrctrl->reg_msdc2_dvfs_halt_mask = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_MSDC2_DVFS_HALT_MASK;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_MSDC3_DVFS_HALT_MASK])) {
		pwrctrl->reg_msdc3_dvfs_halt_mask = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_MSDC3_DVFS_HALT_MASK;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_SW2SPM_INT0_MASK_B])) {
		pwrctrl->reg_sw2spm_int0_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_SW2SPM_INT0_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_SW2SPM_INT1_MASK_B])) {
		pwrctrl->reg_sw2spm_int1_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_SW2SPM_INT1_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_SW2SPM_INT2_MASK_B])) {
		pwrctrl->reg_sw2spm_int2_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_SW2SPM_INT2_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_SW2SPM_INT3_MASK_B])) {
		pwrctrl->reg_sw2spm_int3_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_SW2SPM_INT3_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_PMCU2SPM_INT0_MASK_B])) {
		pwrctrl->reg_pmcu2spm_int0_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_PMCU2SPM_INT0_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_PMCU2SPM_INT1_MASK_B])) {
		pwrctrl->reg_pmcu2spm_int1_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_PMCU2SPM_INT1_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_PMCU2SPM_INT2_MASK_B])) {
		pwrctrl->reg_pmcu2spm_int2_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_PMCU2SPM_INT2_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_PMCU2SPM_INT3_MASK_B])) {
		pwrctrl->reg_pmcu2spm_int3_mask_b = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_PMCU2SPM_INT3_MASK_B;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_WAKEUP_EVENT_MASK])) {
		pwrctrl->reg_wakeup_event_mask = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_WAKEUP_EVENT_MASK;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_REG_EXT_WAKEUP_EVENT_MASK])) {
		pwrctrl->reg_ext_wakeup_event_mask = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_REG_EXT_WAKEUP_EVENT_MASK;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_MP0_CPU0_WFI_EN])) {
		pwrctrl->mp0_cpu0_wfi_en = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_MP0_CPU0_WFI_EN;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_MP0_CPU1_WFI_EN])) {
		pwrctrl->mp0_cpu1_wfi_en = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_MP0_CPU1_WFI_EN;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_MP0_CPU2_WFI_EN])) {
		pwrctrl->mp0_cpu2_wfi_en = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_MP0_CPU2_WFI_EN;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_MP0_CPU3_WFI_EN])) {
		pwrctrl->mp0_cpu3_wfi_en = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_MP0_CPU3_WFI_EN;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_MP1_CPU0_WFI_EN])) {
		pwrctrl->mp1_cpu0_wfi_en = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_MP1_CPU0_WFI_EN;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_MP1_CPU1_WFI_EN])) {
		pwrctrl->mp1_cpu1_wfi_en = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_MP1_CPU1_WFI_EN;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_MP1_CPU2_WFI_EN])) {
		pwrctrl->mp1_cpu2_wfi_en = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_MP1_CPU2_WFI_EN;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_MP1_CPU3_WFI_EN])) {
		pwrctrl->mp1_cpu3_wfi_en = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_MP1_CPU3_WFI_EN;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_DEBUG0_WFI_EN])) {
		pwrctrl->debug0_wfi_en = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_DEBUG0_WFI_EN;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_DEBUG1_WFI_EN])) {
		pwrctrl->debug1_wfi_en = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_DEBUG1_WFI_EN;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_DEBUG2_WFI_EN])) {
		pwrctrl->debug2_wfi_en = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_DEBUG2_WFI_EN;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else if (!strcmp(cmd, pwr_ctrl_str[PWR_DEBUG3_WFI_EN])) {
		pwrctrl->debug3_wfi_en = val;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_d.u.pwr_ctrl.idx = PWR_DEBUG3_WFI_EN;
		spm_to_sspm_command(id, &spm_d);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else {
		return -EINVAL;
	}

	return count;
}

static ssize_t suspend_ctrl_store(struct kobject *kobj, struct kobj_attribute *attr,
				  const char *buf, size_t count)
{
	return store_pwr_ctrl(SPM_PWR_CTRL_SUSPEND, __spm_suspend.pwrctrl, buf, count);
}

static ssize_t dpidle_ctrl_store(struct kobject *kobj, struct kobj_attribute *attr,
				 const char *buf, size_t count)
{
	return store_pwr_ctrl(SPM_PWR_CTRL_DPIDLE, __spm_dpidle.pwrctrl, buf, count);
}

static ssize_t sodi3_ctrl_store(struct kobject *kobj, struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	return store_pwr_ctrl(SPM_PWR_CTRL_SODI3, __spm_sodi3.pwrctrl, buf, count);
}

static ssize_t sodi_ctrl_store(struct kobject *kobj, struct kobj_attribute *attr,
			       const char *buf, size_t count)
{
	return store_pwr_ctrl(SPM_PWR_CTRL_SODI, __spm_sodi.pwrctrl, buf, count);
}

#ifndef CONFIG_MTK_FPGA
static ssize_t mcdi_ctrl_store(struct kobject *kobj, struct kobj_attribute *attr,
			       const char *buf, size_t count)
{
	return 0;
}

static ssize_t vcore_dvfs_ctrl_store(struct kobject *kobj, struct kobj_attribute *attr,
				     const char *buf, size_t count)
{
	return store_pwr_ctrl(__spm_vcore_dvfs.pwrctrl, buf, count);
}
#endif

/**************************************
 * fm_suspend Function
 **************************************/
static ssize_t fm_suspend_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	char *p = buf;

	BUG_ON(p - buf >= PAGE_SIZE);
	return p - buf;
}

/**************************************
 * Init Function
 **************************************/
/* DEFINE_ATTR_RO(suspend_pcm); */
/* DEFINE_ATTR_RO(dpidle_pcm); */
/* DEFINE_ATTR_RO(sodi3_pcm); */
/* DEFINE_ATTR_RO(sodi_pcm); */
/* DEFINE_ATTR_RO(mcdi_pcm); */
/* DEFINE_ATTR_RO(ddrdfs_pcm); */

DEFINE_ATTR_RW(suspend_ctrl);
DEFINE_ATTR_RW(dpidle_ctrl);
DEFINE_ATTR_RW(sodi3_ctrl);
DEFINE_ATTR_RW(sodi_ctrl);
#ifndef CONFIG_MTK_FPGA
DEFINE_ATTR_RW(mcdi_ctrl);
DEFINE_ATTR_RW(vcore_dvfs_ctrl);
#endif
DEFINE_ATTR_RO(fm_suspend);

static struct attribute *spm_attrs[] = {
	/* for spm_lp_scen.pcmdesc */
	/* __ATTR_OF(suspend_pcm), */
	/* __ATTR_OF(dpidle_pcm), */
	/* __ATTR_OF(sodi3_pcm), */
	/* __ATTR_OF(sodi_pcm), */
	/* __ATTR_OF(mcdi_pcm), */
	/* __ATTR_OF(vcore_dvfs_pcm), */

	/* for spm_lp_scen.pwrctrl */
	__ATTR_OF(suspend_ctrl),
	__ATTR_OF(dpidle_ctrl),
	__ATTR_OF(sodi3_ctrl),
	__ATTR_OF(sodi_ctrl),
#ifndef CONFIG_MTK_FPGA
	__ATTR_OF(mcdi_ctrl),
	__ATTR_OF(vcore_dvfs_ctrl),
#endif
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
	r = sysfs_create_group(power_kobj, &spm_attr_group);
	if (r)
		spm_err("FAILED TO CREATE /sys/power/spm (%d)\n", r);

	return r;
}

MODULE_DESCRIPTION("SPM-FS Driver v0.1");
