// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2017 MediaTek Inc.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/string.h>
#include <linux/kernfs.h>

#include <mtk_idle_fs/mtk_idle_sysfs.h>
#include <mtk_spm_internal.h>
#include <mtk_spm_suspend_internal.h>
#include <mtk_spm_resource_req_internal.h>

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
	/* SPM_AP_STANDBY_CON */
	[PW_WFI_OP] = "wfi_op",
	[PW_MP0_CPUTOP_IDLE_MASK] = "mp0_cputop_idle_mask",
	[PW_MP1_CPUTOP_IDLE_MASK] = "mp1_cputop_idle_mask",
	[PW_MCUSYS_IDLE_MASK] = "mcusys_idle_mask",
	[PW_MM_MASK_B] = "mm_mask_b",
	[PW_MD_DDR_EN_0_DBC_EN] = "md_ddr_en_0_dbc_en",
	[PW_MD_DDR_EN_1_DBC_EN] = "md_ddr_en_1_dbc_en",
	[PW_MD_MASK_B] = "md_mask_b",
	[PW_SSPM_MASK_B] = "sspm_mask_b",
	[PW_SCP_MASK_B] = "scp_mask_b",
	[PW_SRCCLKENI_MASK_B] = "srcclkeni_mask_b",
	[PW_MD_APSRC_1_SEL] = "md_apsrc_1_sel",
	[PW_MD_APSRC_0_SEL] = "md_apsrc_0_sel",
	[PW_CONN_DDR_EN_DBC_EN] = "conn_ddr_en_dbc_en",
	[PW_CONN_MASK_B] = "conn_mask_b",
	[PW_CONN_APSRC_SEL] = "conn_apsrc_sel",
	[PW_CONN_SRCCLKENA_SEL_MASK] = "conn_srcclkena_sel_mask",
	/* SPM_SRC_REQ */
	[PW_SPM_APSRC_REQ] = "spm_apsrc_req",
	[PW_SPM_F26M_REQ] = "spm_f26m_req",
	[PW_SPM_INFRA_REQ] = "spm_infra_req",
	[PW_SPM_VRF18_REQ] = "spm_vrf18_req",
	[PW_SPM_DDREN_REQ] = "spm_ddren_req",
	[PW_SPM_RSV_SRC_REQ] = "spm_rsv_src_req",
	[PW_SPM_DDREN_2_REQ] = "spm_ddren_2_req",
	[PW_CPU_MD_DVFS_SOP_FORCE_ON] = "cpu_md_dvfs_sop_force_on",
	/* SPM_SRC_MASK */
	[PW_CSYSPWREQ_MASK] = "csyspwreq_mask",
	[PW_CCIF0_MD_EVENT_MASK_B] = "ccif0_md_event_mask_b",
	[PW_CCIF0_AP_EVENT_MASK_B] = "ccif0_ap_event_mask_b",
	[PW_CCIF1_MD_EVENT_MASK_B] = "ccif1_md_event_mask_b",
	[PW_CCIF1_AP_EVENT_MASK_B] = "ccif1_ap_event_mask_b",
	[PW_CCIF2_MD_EVENT_MASK_B] = "ccif2_md_event_mask_b",
	[PW_CCIF2_AP_EVENT_MASK_B] = "ccif2_ap_event_mask_b",
	[PW_CCIF3_MD_EVENT_MASK_B] = "ccif3_md_event_mask_b",
	[PW_CCIF3_AP_EVENT_MASK_B] = "ccif3_ap_event_mask_b",
	[PW_MD_SRCCLKENA_0_INFRA_MASK_B] = "md_srcclkena_0_infra_mask_b",
	[PW_MD_SRCCLKENA_1_INFRA_MASK_B] = "md_srcclkena_1_infra_mask_b",
	[PW_CONN_SRCCLKENA_INFRA_MASK_B] = "conn_srcclkena_infra_mask_b",
	[PW_UFS_INFRA_REQ_MASK_B] = "ufs_infra_req_mask_b",
	[PW_SRCCLKENI_INFRA_MASK_B] = "srcclkeni_infra_mask_b",
	[PW_MD_APSRC_REQ_0_INFRA_MASK_B] = "md_apsrc_req_0_infra_mask_b",
	[PW_MD_APSRC_REQ_1_INFRA_MASK_B] = "md_apsrc_req_1_infra_mask_b",
	[PW_CONN_APSRCREQ_INFRA_MASK_B] = "conn_apsrcreq_infra_mask_b",
	[PW_UFS_SRCCLKENA_MASK_B] = "ufs_srcclkena_mask_b",
	[PW_MD_VRF18_REQ_0_MASK_B] = "md_vrf18_req_0_mask_b",
	[PW_MD_VRF18_REQ_1_MASK_B] = "md_vrf18_req_1_mask_b",
	[PW_UFS_VRF18_REQ_MASK_B] = "ufs_vrf18_req_mask_b",
	[PW_GCE_VRF18_REQ_MASK_B] = "gce_vrf18_req_mask_b",
	[PW_CONN_INFRA_REQ_MASK_B] = "conn_infra_req_mask_b",
	[PW_GCE_APSRC_REQ_MASK_B] = "gce_apsrc_req_mask_b",
	[PW_DISP0_APSRC_REQ_MASK_B] = "disp0_apsrc_req_mask_b",
	[PW_DISP1_APSRC_REQ_MASK_B] = "disp1_apsrc_req_mask_b",
	[PW_MFG_REQ_MASK_B] = "mfg_req_mask_b",
	[PW_VDEC_REQ_MASK_B] = "vdec_req_mask_b",
	[PW_MCU_APSRCREQ_INFRA_MASK_B] = "mcu_apsrcreq_infra_mask_b",
	/* SPM_SRC2_MASK */
	[PW_MD_DDR_EN_0_MASK_B] = "md_ddr_en_0_mask_b",
	[PW_MD_DDR_EN_1_MASK_B] = "md_ddr_en_1_mask_b",
	[PW_CONN_DDR_EN_MASK_B] = "conn_ddr_en_mask_b",
	[PW_DDREN_SSPM_APSRC_REQ_MASK_B] = "ddren_sspm_apsrc_req_mask_b",
	[PW_DDREN_SCP_APSRC_REQ_MASK_B] = "ddren_scp_apsrc_req_mask_b",
	[PW_DISP0_DDREN_MASK_B] = "disp0_ddren_mask_b",
	[PW_DISP1_DDREN_MASK_B] = "disp1_ddren_mask_b",
	[PW_GCE_DDREN_MASK_B] = "gce_ddren_mask_b",
	[PW_DDREN_EMI_SELF_REFRESH_CH0_MASK_B] =
		"ddren_emi_self_refresh_ch0_mask_b",
	[PW_DDREN_EMI_SELF_REFRESH_CH1_MASK_B] =
		"ddren_emi_self_refresh_ch1_mask_b",
	[PW_MCU_APSRC_REQ_MASK_B] = "mcu_apsrc_req_mask_b",
	[PW_MCU_DDREN_MASK_B] = "mcu_ddren_mask_b",
	/* SPM_WAKEUP_EVENT_MASK */
	[PW_SPM_WAKEUP_EVENT_MASK] = "spm_wakeup_event_mask",
	/* SPM_WAKEUP_EVENT_EXT_MASK */
	[PW_SPM_WAKEUP_EVENT_EXT_MASK] = "spm_wakeup_event_ext_mask",
	/* SPM_SRC3_MASK */
	[PW_MD_DDR_EN_2_0_MASK_B] = "md_ddr_en_2_0_mask_b",
	[PW_MD_DDR_EN_2_1_MASK_B] = "md_ddr_en_2_1_mask_b",
	[PW_CONN_DDR_EN_2_MASK_B] = "conn_ddr_en_2_mask_b",
	[PW_DDREN2_SSPM_APSRC_REQ_MASK_B] = "ddren2_sspm_apsrc_req_mask_b",
	[PW_DDREN2_SCP_APSRC_REQ_MASK_B] = "ddren2_scp_apsrc_req_mask_b",
	[PW_DISP0_DDREN2_MASK_B] = "disp0_ddren2_mask_b",
	[PW_DISP1_DDREN2_MASK_B] = "disp1_ddren2_mask_b",
	[PW_GCE_DDREN2_MASK_B] = "gce_ddren2_mask_b",
	[PW_DDREN2_EMI_SELF_REFRESH_CH0_MASK_B] =
		"ddren2_emi_self_refresh_ch0_mask_b",
	[PW_DDREN2_EMI_SELF_REFRESH_CH1_MASK_B] =
		"ddren2_emi_self_refresh_ch1_mask_b",
	[PW_MCU_DDREN_2_MASK_B] = "mcu_ddren_2_mask_b",
	/* MP0_CPU0_WFI_EN */
	[PW_MP0_CPU0_WFI_EN] = "mp0_cpu0_wfi_en",
	/* MP0_CPU1_WFI_EN */
	[PW_MP0_CPU1_WFI_EN] = "mp0_cpu1_wfi_en",
	/* MP0_CPU2_WFI_EN */
	[PW_MP0_CPU2_WFI_EN] = "mp0_cpu2_wfi_en",
	/* MP0_CPU3_WFI_EN */
	[PW_MP0_CPU3_WFI_EN] = "mp0_cpu3_wfi_en",
	/* MP1_CPU0_WFI_EN */
	[PW_MP1_CPU0_WFI_EN] = "mp1_cpu0_wfi_en",
	/* MP1_CPU1_WFI_EN */
	[PW_MP1_CPU1_WFI_EN] = "mp1_cpu1_wfi_en",
	/* MP1_CPU2_WFI_EN */
	[PW_MP1_CPU2_WFI_EN] = "mp1_cpu2_wfi_en",
	/* MP1_CPU3_WFI_EN */
	[PW_MP1_CPU3_WFI_EN] = "mp1_cpu3_wfi_en",
};

/**************************************
 * xxx_ctrl_show Function
 **************************************/
/* code gen by spm_pwr_ctrl_atf.pl, need struct pwr_ctrl */
static ssize_t show_pwr_ctrl(int id, const struct pwr_ctrl *pwrctrl, char *buf)
{
	char *p = buf;
	int i;

	for (i = 0; i < PW_MAX_COUNT; i++) {
		p += sprintf(p, "%s = 0x%zx\n",
				pwr_ctrl_str[i],
				SMC_CALL(GET_PWR_CTRL_ARGS,
				id, i, 0));
	}
	WARN_ON(p - buf >= PAGE_SIZE);

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
	return show_pwr_ctrl(SPM_PWR_CTRL_DPIDLE, &pwrctrl_dp, buf);
}

static ssize_t sodi3_ctrl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return show_pwr_ctrl(SPM_PWR_CTRL_SODI3, &pwrctrl_so3, buf);
}

static ssize_t sodi_ctrl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return show_pwr_ctrl(SPM_PWR_CTRL_SODI, &pwrctrl_so, buf);
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
static ssize_t store_pwr_ctrl(int id, struct pwr_ctrl *pwrctrl,
	const char *buf, size_t count)
{
	u32 val;
	char cmd[64];
	int i;

	if (sscanf(buf, "%63s %x", cmd, &val) != 2)
		return -EPERM;

	pr_info("[SPM] pwr_ctrl: cmd = %s, val = 0x%x\n", cmd, val);


	if (!strcmp(cmd,
		pwr_ctrl_str[PW_SPM_APSRC_REQ])) {
		unsigned int req = (val == 0) ?
			SPM_RESOURCE_CONSOLE_RELEASE : SPM_RESOURCE_CONSOLE_REQ;
		if (spm_resource_req_console_by_id(id, req
			, _RES_MASK(MTK_SPM_RES_EX_DRAM_S1)) != 0) {
			pwrctrl->spm_apsrc_req = val;
			SMC_CALL(PWR_CTRL_ARGS,
					id, PW_SPM_APSRC_REQ, val);
		}
		return count;
	} else if (!strcmp(cmd,
			pwr_ctrl_str[PW_SPM_F26M_REQ])) {
		unsigned int req = (val == 0) ?
			SPM_RESOURCE_CONSOLE_RELEASE : SPM_RESOURCE_CONSOLE_REQ;
		if (spm_resource_req_console_by_id(id, req
			, _RES_MASK(MTK_SPM_RES_EX_26M)) != 0) {
			pwrctrl->spm_f26m_req = val;
			SMC_CALL(PWR_CTRL_ARGS,
					id, PW_SPM_F26M_REQ, val);
		}
		return count;
	} else if (!strcmp(cmd,
			pwr_ctrl_str[PW_SPM_INFRA_REQ])) {
		unsigned int req = (val == 0) ?
			SPM_RESOURCE_CONSOLE_RELEASE : SPM_RESOURCE_CONSOLE_REQ;
		if (spm_resource_req_console_by_id(id, req
			, _RES_MASK(MTK_SPM_RES_EX_AXI_BUS)) != 0) {
			pwrctrl->spm_infra_req = val;
			SMC_CALL(PWR_CTRL_ARGS,
					id, PW_SPM_INFRA_REQ, val);
		}
		return count;
	} else if (!strcmp(cmd,
			pwr_ctrl_str[PW_SPM_VRF18_REQ])) {
		unsigned int req = (val == 0) ?
			SPM_RESOURCE_CONSOLE_RELEASE : SPM_RESOURCE_CONSOLE_REQ;
		if (spm_resource_req_console_by_id(id, req
			, _RES_MASK(MTK_SPM_RES_EX_MAINPLL)) != 0) {
			pwrctrl->spm_vrf18_req = val;
			SMC_CALL(PWR_CTRL_ARGS,
					id, PW_SPM_VRF18_REQ, val);
		}
		return count;
	} else if (!strcmp(cmd,
			pwr_ctrl_str[PW_SPM_DDREN_REQ])) {
		unsigned int req = (val == 0) ?
			SPM_RESOURCE_CONSOLE_RELEASE : SPM_RESOURCE_CONSOLE_REQ;
		if (spm_resource_req_console_by_id(id, req
			, _RES_MASK(MTK_SPM_RES_EX_DRAM_S0)) != 0) {
			pwrctrl->spm_ddren_req = val;
			SMC_CALL(PWR_CTRL_ARGS,
					id, PW_SPM_DDREN_REQ, val);
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
		/* SPM_AP_STANDBY_CON */
		SET_SPM_PWR_CASE(16, wfi_op, val);
		break;
		SET_SPM_PWR_CASE(17, mp0_cputop_idle_mask, val);
		break;
		SET_SPM_PWR_CASE(18, mp1_cputop_idle_mask, val);
		break;
		SET_SPM_PWR_CASE(19, mcusys_idle_mask, val);
		break;
		SET_SPM_PWR_CASE(20, mm_mask_b, val);
		break;
		SET_SPM_PWR_CASE(21, md_ddr_en_0_dbc_en, val);
		break;
		SET_SPM_PWR_CASE(22, md_ddr_en_1_dbc_en, val);
		break;
		SET_SPM_PWR_CASE(23, md_mask_b, val);
		break;
		SET_SPM_PWR_CASE(24, sspm_mask_b, val);
		break;
		SET_SPM_PWR_CASE(25, scp_mask_b, val);
		break;
		SET_SPM_PWR_CASE(26, srcclkeni_mask_b, val);
		break;
		SET_SPM_PWR_CASE(27, md_apsrc_1_sel, val);
		break;
		SET_SPM_PWR_CASE(28, md_apsrc_0_sel, val);
		break;
		SET_SPM_PWR_CASE(29, conn_ddr_en_dbc_en, val);
		break;
		SET_SPM_PWR_CASE(30, conn_mask_b, val);
		break;
		SET_SPM_PWR_CASE(31, conn_apsrc_sel, val);
		break;
		SET_SPM_PWR_CASE(32, conn_srcclkena_sel_mask, val);
		break;

		/* SPM_SRC_REQ */
		SET_SPM_PWR_CASE(33, spm_apsrc_req, val);
		break;
		SET_SPM_PWR_CASE(34, spm_f26m_req, val);
		break;
		SET_SPM_PWR_CASE(35, spm_infra_req, val);
		break;
		SET_SPM_PWR_CASE(36, spm_vrf18_req, val);
		break;
		SET_SPM_PWR_CASE(37, spm_ddren_req, val);
		break;
		SET_SPM_PWR_CASE(38, spm_rsv_src_req, val);
		break;
		SET_SPM_PWR_CASE(39, spm_ddren_2_req, val);
		break;
		SET_SPM_PWR_CASE(40, cpu_md_dvfs_sop_force_on, val);
		break;

		/* SPM_SRC_MASK */
		SET_SPM_PWR_CASE(41, csyspwreq_mask, val);
		break;
		SET_SPM_PWR_CASE(42, ccif0_md_event_mask_b, val);
		break;
		SET_SPM_PWR_CASE(43, ccif0_ap_event_mask_b, val);
		break;
		SET_SPM_PWR_CASE(44, ccif1_md_event_mask_b, val);
		break;
		SET_SPM_PWR_CASE(45, ccif1_ap_event_mask_b, val);
		break;
		SET_SPM_PWR_CASE(46, ccif2_md_event_mask_b, val);
		break;
		SET_SPM_PWR_CASE(47, ccif2_ap_event_mask_b, val);
		break;
		SET_SPM_PWR_CASE(48, ccif3_md_event_mask_b, val);
		break;
		SET_SPM_PWR_CASE(49, ccif3_ap_event_mask_b, val);
		break;
		SET_SPM_PWR_CASE(50, md_srcclkena_0_infra_mask_b, val);
		break;
		SET_SPM_PWR_CASE(51, md_srcclkena_1_infra_mask_b, val);
		break;
		SET_SPM_PWR_CASE(52, conn_srcclkena_infra_mask_b, val);
		break;
		SET_SPM_PWR_CASE(53, ufs_infra_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(54, srcclkeni_infra_mask_b, val);
		break;
		SET_SPM_PWR_CASE(55, md_apsrc_req_0_infra_mask_b, val);
		break;
		SET_SPM_PWR_CASE(56, md_apsrc_req_1_infra_mask_b, val);
		break;
		SET_SPM_PWR_CASE(57, conn_apsrcreq_infra_mask_b, val);
		break;
		SET_SPM_PWR_CASE(58, ufs_srcclkena_mask_b, val);
		break;
		SET_SPM_PWR_CASE(59, md_vrf18_req_0_mask_b, val);
		break;
		SET_SPM_PWR_CASE(60, md_vrf18_req_1_mask_b, val);
		break;
		SET_SPM_PWR_CASE(61, ufs_vrf18_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(62, gce_vrf18_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(63, conn_infra_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(64, gce_apsrc_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(65, disp0_apsrc_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(66, disp1_apsrc_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(67, mfg_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(68, vdec_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(69, mcu_apsrcreq_infra_mask_b, val);
		break;

		/* SPM_SRC2_MASK */
		SET_SPM_PWR_CASE(70, md_ddr_en_0_mask_b, val);
		break;
		SET_SPM_PWR_CASE(71, md_ddr_en_1_mask_b, val);
		break;
		SET_SPM_PWR_CASE(72, conn_ddr_en_mask_b, val);
		break;
		SET_SPM_PWR_CASE(73, ddren_sspm_apsrc_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(74, ddren_scp_apsrc_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(75, disp0_ddren_mask_b, val);
		break;
		SET_SPM_PWR_CASE(76, disp1_ddren_mask_b, val);
		break;
		SET_SPM_PWR_CASE(77, gce_ddren_mask_b, val);
		break;
		SET_SPM_PWR_CASE(78, ddren_emi_self_refresh_ch0_mask_b, val);
		break;
		SET_SPM_PWR_CASE(79, ddren_emi_self_refresh_ch1_mask_b, val);
		break;
		SET_SPM_PWR_CASE(80, mcu_apsrc_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(81, mcu_ddren_mask_b, val);
		break;

		/* SPM_WAKEUP_EVENT_MASK */
		SET_SPM_PWR_CASE(82, spm_wakeup_event_mask, val);
		break;

		/* SPM_WAKEUP_EVENT_EXT_MASK */
		SET_SPM_PWR_CASE(83, spm_wakeup_event_ext_mask, val);
		break;

		/* SPM_SRC3_MASK */
		SET_SPM_PWR_CASE(84, md_ddr_en_2_0_mask_b, val);
		break;
		SET_SPM_PWR_CASE(85, md_ddr_en_2_1_mask_b, val);
		break;
		SET_SPM_PWR_CASE(86, conn_ddr_en_2_mask_b, val);
		break;
		SET_SPM_PWR_CASE(87, ddren2_sspm_apsrc_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(88, ddren2_scp_apsrc_req_mask_b, val);
		break;
		SET_SPM_PWR_CASE(89, disp0_ddren2_mask_b, val);
		break;
		SET_SPM_PWR_CASE(90, disp1_ddren2_mask_b, val);
		break;
		SET_SPM_PWR_CASE(91, gce_ddren2_mask_b, val);
		break;
		SET_SPM_PWR_CASE(92, ddren2_emi_self_refresh_ch0_mask_b, val);
		break;
		SET_SPM_PWR_CASE(93, ddren2_emi_self_refresh_ch1_mask_b, val);
		break;
		SET_SPM_PWR_CASE(94, mcu_ddren_2_mask_b, val);
		break;

		/* MP0_CPU0_WFI_EN */
		SET_SPM_PWR_CASE(95, mp0_cpu0_wfi_en, val);
		break;

		/* MP0_CPU1_WFI_EN */
		SET_SPM_PWR_CASE(96, mp0_cpu1_wfi_en, val);
		break;

		/* MP0_CPU2_WFI_EN */
		SET_SPM_PWR_CASE(97, mp0_cpu2_wfi_en, val);
		break;

		/* MP0_CPU3_WFI_EN */
		SET_SPM_PWR_CASE(98, mp0_cpu3_wfi_en, val);
		break;

		/* MP1_CPU0_WFI_EN */
		SET_SPM_PWR_CASE(99, mp1_cpu0_wfi_en, val);
		break;

		/* MP1_CPU1_WFI_EN */
		SET_SPM_PWR_CASE(100, mp1_cpu1_wfi_en, val);
		break;

		/* MP1_CPU2_WFI_EN */
		SET_SPM_PWR_CASE(101, mp1_cpu2_wfi_en, val);
		break;

		/* MP1_CPU3_WFI_EN */
		SET_SPM_PWR_CASE(102, mp1_cpu3_wfi_en, val);
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

	WARN_ON(p - buf >= PAGE_SIZE);
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
