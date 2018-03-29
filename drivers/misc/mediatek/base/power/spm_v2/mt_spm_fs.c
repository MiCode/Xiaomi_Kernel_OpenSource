/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
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


/**************************************
 * xxx_pcm_show Function
 **************************************/
#if 0				/* FIXME */
static ssize_t show_pcm_desc(const struct pcm_desc *pcmdesc, char *buf)
{
	char *p = buf;

	p += sprintf(p, "version = %s\n", pcmdesc->version);
	p += sprintf(p, "base = 0x%p\n", pcmdesc->base);
	p += sprintf(p, "size = %u\n", pcmdesc->size);
	p += sprintf(p, "sess = %u\n", pcmdesc->sess);
	p += sprintf(p, "replace = %u\n", pcmdesc->replace);

	p += sprintf(p, "vec0 = 0x%x\n", pcmdesc->vec0);
	p += sprintf(p, "vec1 = 0x%x\n", pcmdesc->vec1);
	p += sprintf(p, "vec2 = 0x%x\n", pcmdesc->vec2);
	p += sprintf(p, "vec3 = 0x%x\n", pcmdesc->vec3);
	p += sprintf(p, "vec4 = 0x%x\n", pcmdesc->vec4);
	p += sprintf(p, "vec5 = 0x%x\n", pcmdesc->vec5);
	p += sprintf(p, "vec6 = 0x%x\n", pcmdesc->vec6);
	p += sprintf(p, "vec7 = 0x%x\n", pcmdesc->vec7);

	BUG_ON(p - buf >= PAGE_SIZE);
	return p - buf;
}

static ssize_t suspend_pcm_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return show_pcm_desc(__spm_suspend.pcmdesc, buf);
}

static ssize_t dpidle_pcm_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return show_pcm_desc(__spm_dpidle.pcmdesc, buf);
}

static ssize_t sodi3_pcm_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return show_pcm_desc(__spm_sodi3.pcmdesc, buf);
}

static ssize_t sodi_pcm_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return show_pcm_desc(__spm_sodi.pcmdesc, buf);
}

static ssize_t mcdi_pcm_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return show_pcm_desc(__spm_mcdi.pcmdesc, buf);
}

static ssize_t talking_pcm_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
#if 0
	return show_pcm_desc(__spm_talking.pcmdesc, buf);
#else
	return 0;
#endif
}

static ssize_t ddrdfs_pcm_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
#if 0
	return show_pcm_desc(__spm_ddrdfs.pcmdesc, buf);
#else
	return 0;
#endif
}
#endif


/**************************************
 * xxx_ctrl_show Function
 **************************************/
#if defined(CONFIG_ARCH_MT6757)
static ssize_t show_pwr_ctrl(const struct pwr_ctrl *pwrctrl, char *buf)
{
	char *p = buf;

	p += sprintf(p, "pcm_flags = 0x%x\n", pwrctrl->pcm_flags);
	p += sprintf(p, "pcm_flags_cust = 0x%x\n", pwrctrl->pcm_flags_cust);
	p += sprintf(p, "pcm_reserve = 0x%x\n", pwrctrl->pcm_reserve);
	p += sprintf(p, "timer_val = 0x%x\n", pwrctrl->timer_val);
	p += sprintf(p, "timer_val_cust = 0x%x\n", pwrctrl->timer_val_cust);
	p += sprintf(p, "timer_val_ramp_en = %d\n", pwrctrl->timer_val_ramp_en);
	p += sprintf(p, "timer_val_ramp_en_sec = %d\n", pwrctrl->timer_val_ramp_en_sec);
	p += sprintf(p, "wake_src = 0x%x\n", pwrctrl->wake_src);
	p += sprintf(p, "wake_src_cust = 0x%x\n", pwrctrl->wake_src_cust);
	p += sprintf(p, "wake_src_md32 = 0x%x\n", pwrctrl->wake_src_md32);
	p += sprintf(p, "r0_ctrl_en = %u\n", pwrctrl->r0_ctrl_en);
	p += sprintf(p, "r7_ctrl_en = %u\n", pwrctrl->r7_ctrl_en);
	p += sprintf(p, "infra_dcm_lock = %u\n", pwrctrl->infra_dcm_lock);
	p += sprintf(p, "wdt_disable = %u\n", pwrctrl->wdt_disable);

	/* SPM_AP_STANDBY_CON */
	p += sprintf(p, "wfi_op = %u\n", pwrctrl->wfi_op);
	p += sprintf(p, "mp0_cputop_idle_mask = %u\n", pwrctrl->mp0_cputop_idle_mask);
	p += sprintf(p, "mp1_cputop_idle_mask = %u\n", pwrctrl->mp1_cputop_idle_mask);
	p += sprintf(p, "mcusys_idle_mask = %u\n", pwrctrl->mcusys_idle_mask);
	p += sprintf(p, "mm_mask_b = %u\n", pwrctrl->mm_mask_b);
	p += sprintf(p, "md_ddr_en_dbc_en = %u\n", pwrctrl->md_ddr_en_dbc_en);
	p += sprintf(p, "md_mask_b = %u\n", pwrctrl->md_mask_b);
	p += sprintf(p, "scp_mask_b = %u\n", pwrctrl->scp_mask_b);
	p += sprintf(p, "lte_mask_b = %u\n", pwrctrl->lte_mask_b);
	p += sprintf(p, "srcclkeni_mask_b = %u\n", pwrctrl->srcclkeni_mask_b);
	p += sprintf(p, "md_apsrc_1_sel = %u\n", pwrctrl->md_apsrc_1_sel);
	p += sprintf(p, "md_apsrc_0_sel = %u\n", pwrctrl->md_apsrc_0_sel);
	p += sprintf(p, "conn_mask_b = %u\n", pwrctrl->conn_mask_b);
	p += sprintf(p, "conn_apsrc_sel = %u\n", pwrctrl->conn_apsrc_sel);

	/* SPM_SRC_REQ */
	p += sprintf(p, "spm_apsrc_req = %u\n", pwrctrl->spm_apsrc_req);
	p += sprintf(p, "spm_f26m_req = %u\n", pwrctrl->spm_f26m_req);
	p += sprintf(p, "spm_lte_req = %u\n", pwrctrl->spm_lte_req);
	p += sprintf(p, "spm_infra_req = %u\n", pwrctrl->spm_infra_req);
	p += sprintf(p, "spm_vrf18_req = %u\n", pwrctrl->spm_vrf18_req);
	p += sprintf(p, "spm_dvfs_req = %u\n", pwrctrl->spm_dvfs_req);
	p += sprintf(p, "spm_dvfs_force_down = %u\n", pwrctrl->spm_dvfs_force_down);
	p += sprintf(p, "spm_ddren_req = %u\n", pwrctrl->spm_ddren_req);
	p += sprintf(p, "spm_rsv_src_req = %u\n", pwrctrl->spm_rsv_src_req);
	p += sprintf(p, "cpu_md_dvfs_sop_force_on = %u\n",
		pwrctrl->cpu_md_dvfs_sop_force_on);

	/* SPM_SRC_MASK */
	p += sprintf(p, "csyspwreq_mask = %u\n", pwrctrl->csyspwreq_mask);
	p += sprintf(p, "ccif0_md_event_mask_b = %u\n",
		pwrctrl->ccif0_md_event_mask_b);
	p += sprintf(p, "ccif0_ap_event_mask_b = %u\n",
		pwrctrl->ccif0_ap_event_mask_b);
	p += sprintf(p, "ccif1_md_event_mask_b = %u\n",
		pwrctrl->ccif1_md_event_mask_b);
	p += sprintf(p, "ccif1_ap_event_mask_b = %u\n",
		pwrctrl->ccif1_ap_event_mask_b);
	p += sprintf(p, "ccifmd_md1_event_mask_b = %u\n",
		pwrctrl->ccifmd_md1_event_mask_b);
	p += sprintf(p, "ccifmd_md2_event_mask_b = %u\n",
		pwrctrl->ccifmd_md2_event_mask_b);
	p += sprintf(p, "dsi0_vsync_mask_b = %u\n", pwrctrl->dsi0_vsync_mask_b);
	p += sprintf(p, "dsi1_vsync_mask_b = %u\n", pwrctrl->dsi1_vsync_mask_b);
	p += sprintf(p, "dpi_vsync_mask_b = %u\n", pwrctrl->dpi_vsync_mask_b);
	p += sprintf(p, "isp0_vsync_mask_b = %u\n", pwrctrl->isp0_vsync_mask_b);
	p += sprintf(p, "isp1_vsync_mask_b = %u\n", pwrctrl->isp1_vsync_mask_b);
	p += sprintf(p, "md_srcclkena_0_infra_mask_b = %u\n",
		pwrctrl->md_srcclkena_0_infra_mask_b);
	p += sprintf(p, "md_srcclkena_1_infra_mask_b = %u\n",
		pwrctrl->md_srcclkena_1_infra_mask_b);
	p += sprintf(p, "conn_srcclkena_infra_mask_b = %u\n",
		pwrctrl->conn_srcclkena_infra_mask_b);
	p += sprintf(p, "md32_srcclkena_infra_mask_b = %u\n",
		pwrctrl->md32_srcclkena_infra_mask_b);
	p += sprintf(p, "srcclkeni_infra_mask_b = %u\n",
		pwrctrl->srcclkeni_infra_mask_b);
	p += sprintf(p, "md_apsrc_req_0_infra_mask_b = %u\n",
		pwrctrl->md_apsrc_req_0_infra_mask_b);
	p += sprintf(p, "md_apsrc_req_1_infra_mask_b = %u\n",
		pwrctrl->md_apsrc_req_1_infra_mask_b);
	p += sprintf(p, "conn_apsrcreq_infra_mask_b = %u\n",
		pwrctrl->conn_apsrcreq_infra_mask_b);
	p += sprintf(p, "md32_apsrcreq_infra_mask_b = %u\n",
		pwrctrl->md32_apsrcreq_infra_mask_b);
	p += sprintf(p, "md_ddr_en_0_mask_b = %u\n", pwrctrl->md_ddr_en_0_mask_b);
	p += sprintf(p, "md_ddr_en_1_mask_b = %u\n", pwrctrl->md_ddr_en_1_mask_b);
	p += sprintf(p, "md_vrf18_req_0_mask_b = %u\n",
		pwrctrl->md_vrf18_req_0_mask_b);
	p += sprintf(p, "md_vrf18_req_1_mask_b = %u\n",
		pwrctrl->md_vrf18_req_1_mask_b);
	p += sprintf(p, "md1_dvfs_req_mask = %u\n", pwrctrl->md1_dvfs_req_mask);
	p += sprintf(p, "cpu_dvfs_req_mask = %u\n", pwrctrl->cpu_dvfs_req_mask);
	p += sprintf(p, "emi_bw_dvfs_req_mask = %u\n", pwrctrl->emi_bw_dvfs_req_mask);
	p += sprintf(p, "md_srcclkena_0_dvfs_req_mask_b = %u\n",
		pwrctrl->md_srcclkena_0_dvfs_req_mask_b);
	p += sprintf(p, "md_srcclkena_1_dvfs_req_mask_b = %u\n",
		pwrctrl->md_srcclkena_1_dvfs_req_mask_b);
	p += sprintf(p, "conn_srcclkena_dvfs_req_mask_b = %u\n",
		pwrctrl->conn_srcclkena_dvfs_req_mask_b);

	/* SPM_SRC2_MASK */
	p += sprintf(p, "dvfs_halt_mask_b = %u\n", pwrctrl->dvfs_halt_mask_b);
	p += sprintf(p, "vdec_req_mask_b = %u\n", pwrctrl->vdec_req_mask_b);
	p += sprintf(p, "gce_req_mask_b = %u\n", pwrctrl->gce_req_mask_b);
	p += sprintf(p, "cpu_md_dvfs_req_merge_mask_b = %u\n",
		pwrctrl->cpu_md_dvfs_req_merge_mask_b);
	p += sprintf(p, "md_ddr_en_dvfs_halt_mask_b = %u\n",
		pwrctrl->md_ddr_en_dvfs_halt_mask_b);
	p += sprintf(p, "dsi0_vsync_dvfs_halt_mask_b = %u\n",
		pwrctrl->dsi0_vsync_dvfs_halt_mask_b);
	p += sprintf(p, "dsi1_vsync_dvfs_halt_mask_b = %u\n",
		pwrctrl->dsi1_vsync_dvfs_halt_mask_b);
	p += sprintf(p, "dpi_vsync_dvfs_halt_mask_b = %u\n",
		pwrctrl->dpi_vsync_dvfs_halt_mask_b);
	p += sprintf(p, "isp0_vsync_dvfs_halt_mask_b = %u\n",
		pwrctrl->isp0_vsync_dvfs_halt_mask_b);
	p += sprintf(p, "isp1_vsync_dvfs_halt_mask_b = %u\n",
		pwrctrl->isp1_vsync_dvfs_halt_mask_b);
	p += sprintf(p, "conn_ddr_en_mask_b = %u\n", pwrctrl->conn_ddr_en_mask_b);
	p += sprintf(p, "disp_req_mask_b = %u\n", pwrctrl->disp_req_mask_b);
	p += sprintf(p, "disp1_req_mask_b = %u\n", pwrctrl->disp1_req_mask_b);
	p += sprintf(p, "mfg_req_mask_b = %u\n", pwrctrl->mfg_req_mask_b);
	p += sprintf(p, "c2k_ps_rccif_wake_mask_b = %u\n",
		pwrctrl->c2k_ps_rccif_wake_mask_b);
	p += sprintf(p, "c2k_l1_rccif_wake_mask_b = %u\n",
		pwrctrl->c2k_l1_rccif_wake_mask_b);
	p += sprintf(p, "ps_c2k_rccif_wake_mask_b = %u\n",
		pwrctrl->ps_c2k_rccif_wake_mask_b);
	p += sprintf(p, "l1_c2k_rccif_wake_mask_b = %u\n",
		pwrctrl->l1_c2k_rccif_wake_mask_b);
	p += sprintf(p, "sdio_on_dvfs_req_mask_b = %u\n",
		pwrctrl->sdio_on_dvfs_req_mask_b);
	p += sprintf(p, "emi_boost_dvfs_req_mask_b = %u\n",
		pwrctrl->emi_boost_dvfs_req_mask_b);
	p += sprintf(p, "cpu_md_emi_dvfs_req_prot_dis = %u\n",
		pwrctrl->cpu_md_emi_dvfs_req_prot_dis);
	p += sprintf(p, "dramc_spcmd_apsrc_req_mask_b = %u\n",
		pwrctrl->dramc_spcmd_apsrc_req_mask_b);

#if 0
	/* SPM_WAKEUP_EVENT_MASK */
	p += sprintf(p, "spm_wakeup_event_mask = %u\n",
		pwrctrl->spm_wakeup_event_mask);

	/* SPM_WAKEUP_EVENT_EXT_MASK */
	p += sprintf(p, "spm_wakeup_event_ext_mask = %u\n",
		pwrctrl->spm_wakeup_event_ext_mask);
#endif

	BUG_ON(p - buf >= PAGE_SIZE);
	return p - buf;
}
#else
static ssize_t show_pwr_ctrl(const struct pwr_ctrl *pwrctrl, char *buf)
{
	char *p = buf;

	p += sprintf(p, "pcm_flags = 0x%x\n", pwrctrl->pcm_flags);
	p += sprintf(p, "pcm_flags_cust = 0x%x\n", pwrctrl->pcm_flags_cust);
	p += sprintf(p, "pcm_reserve = 0x%x\n", pwrctrl->pcm_reserve);
	p += sprintf(p, "timer_val = 0x%x\n", pwrctrl->timer_val);
	p += sprintf(p, "timer_val_cust = 0x%x\n", pwrctrl->timer_val_cust);
	p += sprintf(p, "timer_val_ramp_en = %d\n", pwrctrl->timer_val_ramp_en);
	p += sprintf(p, "timer_val_ramp_en_sec = %d\n", pwrctrl->timer_val_ramp_en_sec);
	p += sprintf(p, "wake_src = 0x%x\n", pwrctrl->wake_src);
	p += sprintf(p, "wake_src_cust = 0x%x\n", pwrctrl->wake_src_cust);
	p += sprintf(p, "wake_src_md32 = 0x%x\n", pwrctrl->wake_src_md32);
	p += sprintf(p, "r0_ctrl_en = %u\n", pwrctrl->r0_ctrl_en);
	p += sprintf(p, "r7_ctrl_en = %u\n", pwrctrl->r7_ctrl_en);
	p += sprintf(p, "infra_dcm_lock = %u\n", pwrctrl->infra_dcm_lock);
	p += sprintf(p, "wdt_disable = %u\n", pwrctrl->wdt_disable);
	p += sprintf(p, "spm_apsrc_req = %u\n", pwrctrl->spm_apsrc_req);
	p += sprintf(p, "spm_f26m_req = %u\n", pwrctrl->spm_f26m_req);

	p += sprintf(p, "mcusys_idle_mask = %u\n", pwrctrl->mcusys_idle_mask);
	p += sprintf(p, "mp1top_idle_mask = %u\n", pwrctrl->mp1top_idle_mask);
	p += sprintf(p, "mp0top_idle_mask = %u\n", pwrctrl->mp0top_idle_mask);
	p += sprintf(p, "wfi_op = %u\n", pwrctrl->wfi_op);
	p += sprintf(p, "mp1_cpu0_wfi_en = %u\n", pwrctrl->mp1_cpu0_wfi_en);
	p += sprintf(p, "mp1_cpu1_wfi_en = %u\n", pwrctrl->mp1_cpu1_wfi_en);
	p += sprintf(p, "mp1_cpu2_wfi_en = %u\n", pwrctrl->mp1_cpu2_wfi_en);
	p += sprintf(p, "mp1_cpu3_wfi_en = %u\n", pwrctrl->mp1_cpu3_wfi_en);
	p += sprintf(p, "mp0_cpu0_wfi_en = %u\n", pwrctrl->mp0_cpu0_wfi_en);
	p += sprintf(p, "mp0_cpu1_wfi_en = %u\n", pwrctrl->mp0_cpu1_wfi_en);
	p += sprintf(p, "mp0_cpu2_wfi_en = %u\n", pwrctrl->mp0_cpu2_wfi_en);
	p += sprintf(p, "mp0_cpu3_wfi_en = %u\n", pwrctrl->mp0_cpu3_wfi_en);

	p += sprintf(p, "md1_req_mask_b = %u\n", pwrctrl->md1_req_mask_b);
	p += sprintf(p, "md2_req_mask_b = %u\n", pwrctrl->md2_req_mask_b);
	p += sprintf(p, "md_apsrc0_sel = %u\n", pwrctrl->md_apsrc0_sel);
	p += sprintf(p, "md_apsrc1_sel = %u\n", pwrctrl->md_apsrc1_sel);
	p += sprintf(p, "conn_apsrc_sel = %u\n", pwrctrl->conn_apsrc_sel);
	p += sprintf(p, "md_ddr_dbc_en = %u\n", pwrctrl->md_ddr_dbc_en);
	p += sprintf(p, "ccif0_to_ap_mask_b = %u\n", pwrctrl->ccif0_to_ap_mask_b);
	p += sprintf(p, "ccif0_to_md_mask_b = %u\n", pwrctrl->ccif0_to_md_mask_b);
	p += sprintf(p, "ccif1_to_ap_mask_b = %u\n", pwrctrl->ccif1_to_ap_mask_b);
	p += sprintf(p, "ccif1_to_md_mask_b = %u\n", pwrctrl->ccif1_to_md_mask_b);
	p += sprintf(p, "lte_mask_b = %u\n", pwrctrl->lte_mask_b);
	p += sprintf(p, "ccifmd_md1_event_mask_b = %u\n", pwrctrl->ccifmd_md1_event_mask_b);
	p += sprintf(p, "ccifmd_md2_event_mask_b = %u\n", pwrctrl->ccifmd_md2_event_mask_b);

	p += sprintf(p, "conn_mask_b = %u\n", pwrctrl->conn_mask_b);

	p += sprintf(p, "dsi0_ddr_en_mask_b = %u\n", pwrctrl->dsi0_ddr_en_mask_b);
	p += sprintf(p, "dsi1_ddr_en_mask_b = %u\n", pwrctrl->dsi1_ddr_en_mask_b);
	p += sprintf(p, "dpi_ddr_en_mask_b = %u\n", pwrctrl->dpi_ddr_en_mask_b);
	p += sprintf(p, "isp0_ddr_en_mask_b = %u\n", pwrctrl->isp0_ddr_en_mask_b);
	p += sprintf(p, "isp1_ddr_en_mask_b = %u\n", pwrctrl->isp1_ddr_en_mask_b);

	p += sprintf(p, "scp_req_mask_b = %u\n", pwrctrl->scp_req_mask_b);
	p += sprintf(p, "syspwreq_mask = %u\n", pwrctrl->syspwreq_mask);
	p += sprintf(p, "srclkenai_mask = %u\n", pwrctrl->srclkenai_mask);

	p += sprintf(p, "gce_req_mask_b = %u\n", pwrctrl->gce_req_mask_b);
	p += sprintf(p, "disp_req_mask_b = %u\n", pwrctrl->disp_req_mask_b);
	p += sprintf(p, "disp1_req_mask_b = %u\n", pwrctrl->disp1_req_mask_b);
	p += sprintf(p, "mfg_req_mask_b = %u\n", pwrctrl->mfg_req_mask_b);
#if defined(CONFIG_ARCH_MT6797)
	p += sprintf(p, "disp_od_req_mask_b = %u\n", pwrctrl->disp_od_req_mask_b);
#endif

	p += sprintf(p, "param1 = 0x%x\n", pwrctrl->param1);
	p += sprintf(p, "param2 = 0x%x\n", pwrctrl->param2);
	p += sprintf(p, "param3 = 0x%x\n", pwrctrl->param3);

	p += sprintf(p, "dvfs_halt_mask_b = 0x%x\n", pwrctrl->dvfs_halt_mask_b);
	p += sprintf(p, "sdio_on_dvfs_req_mask_b = %u\n", pwrctrl->sdio_on_dvfs_req_mask_b);

	p += sprintf(p, "cpu_md_dvfs_erq_merge_mask_b = %u\n",
		     pwrctrl->cpu_md_dvfs_erq_merge_mask_b);
	p += sprintf(p, "md1_ddr_en_dvfs_halt_mask_b = %u\n", pwrctrl->md1_ddr_en_dvfs_halt_mask_b);
	p += sprintf(p, "md2_ddr_en_dvfs_halt_mask_b = %u\n", pwrctrl->md2_ddr_en_dvfs_halt_mask_b);


	p += sprintf(p, "md_srcclkena_0_dvfs_req_mask_b = %u\n",
		     pwrctrl->md_srcclkena_0_dvfs_req_mask_b);
	p += sprintf(p, "md_srcclkena_1_dvfs_req_mask_b = %u\n",
		     pwrctrl->md_srcclkena_1_dvfs_req_mask_b);
	p += sprintf(p, "conn_srcclkena_dvfs_req_mask_b = %u\n",
		     pwrctrl->conn_srcclkena_dvfs_req_mask_b);

	p += sprintf(p, "vsync_dvfs_halt_mask_b = 0x%x\n", pwrctrl->vsync_dvfs_halt_mask_b);
	p += sprintf(p, "emi_boost_dvfs_req_mask_b = %u\n", pwrctrl->emi_boost_dvfs_req_mask_b);
	p += sprintf(p, "cpu_md_emi_dvfs_req_prot_dis = %u\n",
		     pwrctrl->cpu_md_emi_dvfs_req_prot_dis);
	p += sprintf(p, "emi_bw_dvfs_req_mask = %u\n", pwrctrl->emi_bw_dvfs_req_mask);

	p += sprintf(p, "spm_dvfs_req = %u\n", pwrctrl->spm_dvfs_req);
	p += sprintf(p, "spm_dvfs_force_down = %u\n", pwrctrl->spm_dvfs_force_down);
	p += sprintf(p, "cpu_md_dvfs_sop_force_on = %u\n", pwrctrl->cpu_md_dvfs_sop_force_on);

	BUG_ON(p - buf >= PAGE_SIZE);
	return p - buf;
}
#endif

static ssize_t suspend_ctrl_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return show_pwr_ctrl(__spm_suspend.pwrctrl, buf);
}

static ssize_t dpidle_ctrl_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return show_pwr_ctrl(__spm_dpidle.pwrctrl, buf);
}

#if !defined(CONFIG_FPGA_EARLY_PORTING)
static ssize_t sodi3_ctrl_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return show_pwr_ctrl(__spm_sodi3.pwrctrl, buf);
}

static ssize_t sodi_ctrl_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return show_pwr_ctrl(__spm_sodi.pwrctrl, buf);
}

static ssize_t mcdi_ctrl_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return 0;
}
#endif
static ssize_t talking_ctrl_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
#if 0
	return show_pwr_ctrl(__spm_talking.pwrctrl, buf);
#else
	return 0;
#endif
}

static ssize_t vcore_dvfs_ctrl_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	return show_pwr_ctrl(__spm_vcore_dvfs.pwrctrl, buf);
#else
	return 0;
#endif
}


/**************************************
 * xxx_ctrl_store Function
 **************************************/
#if defined(CONFIG_ARCH_MT6757)
static ssize_t store_pwr_ctrl(struct pwr_ctrl *pwrctrl, const char *buf, size_t count)
{
	u32 val;
	char cmd[32];

	if (sscanf(buf, "%31s %x", cmd, &val) != 2)
		return -EPERM;

	spm_debug("pwr_ctrl: cmd = %s, val = 0x%x\n", cmd, val);

	if (!strcmp(cmd, "pcm_flags"))
		pwrctrl->pcm_flags = val;
	else if (!strcmp(cmd, "pcm_flags_cust"))
		pwrctrl->pcm_flags_cust = val;
	else if (!strcmp(cmd, "pcm_reserve"))
		pwrctrl->pcm_reserve = val;
	else if (!strcmp(cmd, "timer_val"))
		pwrctrl->timer_val = val;
	else if (!strcmp(cmd, "timer_val_cust"))
		pwrctrl->timer_val_cust = val;
	else if (!strcmp(cmd, "timer_val_ramp_en"))
		pwrctrl->timer_val_ramp_en = val;
	else if (!strcmp(cmd, "timer_val_ramp_en_sec"))
		pwrctrl->timer_val_ramp_en_sec = val;
	else if (!strcmp(cmd, "wake_src"))
		pwrctrl->wake_src = val;
	else if (!strcmp(cmd, "wake_src_cust"))
		pwrctrl->wake_src_cust = val;
	else if (!strcmp(cmd, "wake_src_md32"))
		pwrctrl->wake_src_md32 = val;
	else if (!strcmp(cmd, "r0_ctrl_en"))
		pwrctrl->r0_ctrl_en = val;
	else if (!strcmp(cmd, "r7_ctrl_en"))
		pwrctrl->r7_ctrl_en = val;
	else if (!strcmp(cmd, "infra_dcm_lock"))
		pwrctrl->infra_dcm_lock = val;
	else if (!strcmp(cmd, "wdt_disable"))
		pwrctrl->wdt_disable = val;

	/* SPM_AP_STANDBY_CON */
	else if (!strcmp(cmd, "wfi_op"))
		pwrctrl->wfi_op = val;
	else if (!strcmp(cmd, "mp0_cputop_idle_mask"))
		pwrctrl->mp0_cputop_idle_mask = val;
	else if (!strcmp(cmd, "mp1_cputop_idle_mask"))
		pwrctrl->mp1_cputop_idle_mask = val;
	else if (!strcmp(cmd, "mcusys_idle_mask"))
		pwrctrl->mcusys_idle_mask = val;
	else if (!strcmp(cmd, "mm_mask_b"))
		pwrctrl->mm_mask_b = val;
	else if (!strcmp(cmd, "md_ddr_en_dbc_en"))
		pwrctrl->md_ddr_en_dbc_en = val;
	else if (!strcmp(cmd, "md_mask_b"))
		pwrctrl->md_mask_b = val;
	else if (!strcmp(cmd, "scp_mask_b"))
		pwrctrl->scp_mask_b = val;
	else if (!strcmp(cmd, "lte_mask_b"))
		pwrctrl->lte_mask_b = val;
	else if (!strcmp(cmd, "srcclkeni_mask_b"))
		pwrctrl->srcclkeni_mask_b = val;
	else if (!strcmp(cmd, "md_apsrc_1_sel"))
		pwrctrl->md_apsrc_1_sel = val;
	else if (!strcmp(cmd, "md_apsrc_0_sel"))
		pwrctrl->md_apsrc_0_sel = val;
	else if (!strcmp(cmd, "conn_mask_b"))
		pwrctrl->conn_mask_b = val;
	else if (!strcmp(cmd, "conn_apsrc_sel"))
		pwrctrl->conn_apsrc_sel = val;

	/* SPM_SRC_REQ */
	else if (!strcmp(cmd, "spm_apsrc_req"))
		pwrctrl->spm_apsrc_req = val;
	else if (!strcmp(cmd, "spm_f26m_req"))
		pwrctrl->spm_f26m_req = val;
	else if (!strcmp(cmd, "spm_lte_req"))
		pwrctrl->spm_lte_req = val;
	else if (!strcmp(cmd, "spm_infra_req"))
		pwrctrl->spm_infra_req = val;
	else if (!strcmp(cmd, "spm_vrf18_req"))
		pwrctrl->spm_vrf18_req = val;
	else if (!strcmp(cmd, "spm_dvfs_req"))
		pwrctrl->spm_dvfs_req = val;
	else if (!strcmp(cmd, "spm_dvfs_force_down"))
		pwrctrl->spm_dvfs_force_down = val;
	else if (!strcmp(cmd, "spm_ddren_req"))
		pwrctrl->spm_ddren_req = val;
	else if (!strcmp(cmd, "spm_rsv_src_req"))
		pwrctrl->spm_rsv_src_req = val;
	else if (!strcmp(cmd, "cpu_md_dvfs_sop_force_on"))
		pwrctrl->cpu_md_dvfs_sop_force_on = val;

	/* SPM_SRC_MASK */
	else if (!strcmp(cmd, "csyspwreq_mask"))
		pwrctrl->csyspwreq_mask = val;
	else if (!strcmp(cmd, "ccif0_md_event_mask_b"))
		pwrctrl->ccif0_md_event_mask_b = val;
	else if (!strcmp(cmd, "ccif0_ap_event_mask_b"))
		pwrctrl->ccif0_ap_event_mask_b = val;
	else if (!strcmp(cmd, "ccif1_md_event_mask_b"))
		pwrctrl->ccif1_md_event_mask_b = val;
	else if (!strcmp(cmd, "ccif1_ap_event_mask_b"))
		pwrctrl->ccif1_ap_event_mask_b = val;
	else if (!strcmp(cmd, "ccifmd_md1_event_mask_b"))
		pwrctrl->ccifmd_md1_event_mask_b = val;
	else if (!strcmp(cmd, "ccifmd_md2_event_mask_b"))
		pwrctrl->ccifmd_md2_event_mask_b = val;
	else if (!strcmp(cmd, "dsi0_vsync_mask_b"))
		pwrctrl->dsi0_vsync_mask_b = val;
	else if (!strcmp(cmd, "dsi1_vsync_mask_b"))
		pwrctrl->dsi1_vsync_mask_b = val;
	else if (!strcmp(cmd, "dpi_vsync_mask_b"))
		pwrctrl->dpi_vsync_mask_b = val;
	else if (!strcmp(cmd, "isp0_vsync_mask_b"))
		pwrctrl->isp0_vsync_mask_b = val;
	else if (!strcmp(cmd, "isp1_vsync_mask_b"))
		pwrctrl->isp1_vsync_mask_b = val;
	else if (!strcmp(cmd, "md_srcclkena_0_infra_mask_b"))
		pwrctrl->md_srcclkena_0_infra_mask_b = val;
	else if (!strcmp(cmd, "md_srcclkena_1_infra_mask_b"))
		pwrctrl->md_srcclkena_1_infra_mask_b = val;
	else if (!strcmp(cmd, "conn_srcclkena_infra_mask_b"))
		pwrctrl->conn_srcclkena_infra_mask_b = val;
	else if (!strcmp(cmd, "md32_srcclkena_infra_mask_b"))
		pwrctrl->md32_srcclkena_infra_mask_b = val;
	else if (!strcmp(cmd, "srcclkeni_infra_mask_b"))
		pwrctrl->srcclkeni_infra_mask_b = val;
	else if (!strcmp(cmd, "md_apsrc_req_0_infra_mask_b"))
		pwrctrl->md_apsrc_req_0_infra_mask_b = val;
	else if (!strcmp(cmd, "md_apsrc_req_1_infra_mask_b"))
		pwrctrl->md_apsrc_req_1_infra_mask_b = val;
	else if (!strcmp(cmd, "conn_apsrcreq_infra_mask_b"))
		pwrctrl->conn_apsrcreq_infra_mask_b = val;
	else if (!strcmp(cmd, "md32_apsrcreq_infra_mask_b"))
		pwrctrl->md32_apsrcreq_infra_mask_b = val;
	else if (!strcmp(cmd, "md_ddr_en_0_mask_b"))
		pwrctrl->md_ddr_en_0_mask_b = val;
	else if (!strcmp(cmd, "md_ddr_en_1_mask_b"))
		pwrctrl->md_ddr_en_1_mask_b = val;
	else if (!strcmp(cmd, "md_vrf18_req_0_mask_b"))
		pwrctrl->md_vrf18_req_0_mask_b = val;
	else if (!strcmp(cmd, "md_vrf18_req_1_mask_b"))
		pwrctrl->md_vrf18_req_1_mask_b = val;
	else if (!strcmp(cmd, "md1_dvfs_req_mask"))
		pwrctrl->md1_dvfs_req_mask = val;
	else if (!strcmp(cmd, "cpu_dvfs_req_mask"))
		pwrctrl->cpu_dvfs_req_mask = val;
	else if (!strcmp(cmd, "emi_bw_dvfs_req_mask"))
		pwrctrl->emi_bw_dvfs_req_mask = val;
	else if (!strcmp(cmd, "md_srcclkena_0_dvfs_req_mask_b"))
		pwrctrl->md_srcclkena_0_dvfs_req_mask_b = val;
	else if (!strcmp(cmd, "md_srcclkena_1_dvfs_req_mask_b"))
		pwrctrl->md_srcclkena_1_dvfs_req_mask_b = val;
	else if (!strcmp(cmd, "conn_srcclkena_dvfs_req_mask_b"))
		pwrctrl->conn_srcclkena_dvfs_req_mask_b = val;

	/* SPM_SRC2_MASK */
	else if (!strcmp(cmd, "dvfs_halt_mask_b"))
		pwrctrl->dvfs_halt_mask_b = val;
	else if (!strcmp(cmd, "vdec_req_mask_b"))
		pwrctrl->vdec_req_mask_b = val;
	else if (!strcmp(cmd, "gce_req_mask_b"))
		pwrctrl->gce_req_mask_b = val;
	else if (!strcmp(cmd, "cpu_md_dvfs_req_merge_mask_b"))
		pwrctrl->cpu_md_dvfs_req_merge_mask_b = val;
	else if (!strcmp(cmd, "md_ddr_en_dvfs_halt_mask_b"))
		pwrctrl->md_ddr_en_dvfs_halt_mask_b = val;
	else if (!strcmp(cmd, "dsi0_vsync_dvfs_halt_mask_b"))
		pwrctrl->dsi0_vsync_dvfs_halt_mask_b = val;
	else if (!strcmp(cmd, "dsi1_vsync_dvfs_halt_mask_b"))
		pwrctrl->dsi1_vsync_dvfs_halt_mask_b = val;
	else if (!strcmp(cmd, "dpi_vsync_dvfs_halt_mask_b"))
		pwrctrl->dpi_vsync_dvfs_halt_mask_b = val;
	else if (!strcmp(cmd, "isp0_vsync_dvfs_halt_mask_b"))
		pwrctrl->isp0_vsync_dvfs_halt_mask_b = val;
	else if (!strcmp(cmd, "isp1_vsync_dvfs_halt_mask_b"))
		pwrctrl->isp1_vsync_dvfs_halt_mask_b = val;
	else if (!strcmp(cmd, "conn_ddr_en_mask_b"))
		pwrctrl->conn_ddr_en_mask_b = val;
	else if (!strcmp(cmd, "disp_req_mask_b"))
		pwrctrl->disp_req_mask_b = val;
	else if (!strcmp(cmd, "disp1_req_mask_b"))
		pwrctrl->disp1_req_mask_b = val;
	else if (!strcmp(cmd, "mfg_req_mask_b"))
		pwrctrl->mfg_req_mask_b = val;
	else if (!strcmp(cmd, "c2k_ps_rccif_wake_mask_b"))
		pwrctrl->c2k_ps_rccif_wake_mask_b = val;
	else if (!strcmp(cmd, "c2k_l1_rccif_wake_mask_b"))
		pwrctrl->c2k_l1_rccif_wake_mask_b = val;
	else if (!strcmp(cmd, "ps_c2k_rccif_wake_mask_b"))
		pwrctrl->ps_c2k_rccif_wake_mask_b = val;
	else if (!strcmp(cmd, "l1_c2k_rccif_wake_mask_b"))
		pwrctrl->l1_c2k_rccif_wake_mask_b = val;
	else if (!strcmp(cmd, "sdio_on_dvfs_req_mask_b"))
		pwrctrl->sdio_on_dvfs_req_mask_b = val;
	else if (!strcmp(cmd, "emi_boost_dvfs_req_mask_b"))
		pwrctrl->emi_boost_dvfs_req_mask_b = val;
	else if (!strcmp(cmd, "cpu_md_emi_dvfs_req_prot_dis"))
		pwrctrl->cpu_md_emi_dvfs_req_prot_dis = val;
	else if (!strcmp(cmd, "dramc_spcmd_apsrc_req_mask_b"))
		pwrctrl->dramc_spcmd_apsrc_req_mask_b = val;

#if 0
	/* SPM_WAKEUP_EVENT_MASK */
	else if (!strcmp(cmd, "spm_wakeup_event_mask"))
		pwrctrl->spm_wakeup_event_mask = val;

	/* SPM_WAKEUP_EVENT_EXT_MASK */
	else if (!strcmp(cmd, "spm_wakeup_event_ext_mask"))
		pwrctrl->spm_wakeup_event_ext_mask = val;
#endif

	else
		return -EINVAL;

	return count;
}
#else
static ssize_t store_pwr_ctrl(struct pwr_ctrl *pwrctrl, const char *buf, size_t count)
{
	u32 val;
	char cmd[32];

	if (sscanf(buf, "%31s %x", cmd, &val) != 2)
		return -EPERM;

	spm_debug("pwr_ctrl: cmd = %s, val = 0x%x\n", cmd, val);

	if (!strcmp(cmd, "pcm_flags"))
		pwrctrl->pcm_flags = val;
	else if (!strcmp(cmd, "pcm_flags_cust"))
		pwrctrl->pcm_flags_cust = val;
	else if (!strcmp(cmd, "pcm_reserve"))
		pwrctrl->pcm_reserve = val;
	else if (!strcmp(cmd, "timer_val"))
		pwrctrl->timer_val = val;
	else if (!strcmp(cmd, "timer_val_cust"))
		pwrctrl->timer_val_cust = val;
	else if (!strcmp(cmd, "timer_val_ramp_en"))
		pwrctrl->timer_val_ramp_en = val;
	else if (!strcmp(cmd, "timer_val_ramp_en_sec"))
		pwrctrl->timer_val_ramp_en_sec = val;
	else if (!strcmp(cmd, "wake_src"))
		pwrctrl->wake_src = val;
	else if (!strcmp(cmd, "wake_src_cust"))
		pwrctrl->wake_src_cust = val;
	else if (!strcmp(cmd, "wake_src_md32"))
		pwrctrl->wake_src_md32 = val;
	else if (!strcmp(cmd, "r0_ctrl_en"))
		pwrctrl->r0_ctrl_en = val;
	else if (!strcmp(cmd, "r7_ctrl_en"))
		pwrctrl->r7_ctrl_en = val;
	else if (!strcmp(cmd, "infra_dcm_lock"))
		pwrctrl->infra_dcm_lock = val;
	else if (!strcmp(cmd, "wdt_disable"))
		pwrctrl->wdt_disable = val;

	else if (!strcmp(cmd, "spm_apsrc_req"))
		pwrctrl->spm_apsrc_req = val;
	else if (!strcmp(cmd, "spm_f26m_req"))
		pwrctrl->spm_f26m_req = val;
	else if (!strcmp(cmd, "spm_lte_req"))
		pwrctrl->spm_lte_req = val;
	else if (!strcmp(cmd, "spm_infra_req"))
		pwrctrl->spm_infra_req = val;
	else if (!strcmp(cmd, "spm_vrf18_req"))
		pwrctrl->spm_vrf18_req = val;
	else if (!strcmp(cmd, "spm_dvfs_req"))
		pwrctrl->spm_dvfs_req = val;
	else if (!strcmp(cmd, "spm_dvfs_force_down"))
		pwrctrl->spm_dvfs_force_down = val;
	else if (!strcmp(cmd, "spm_ddren_req"))
		pwrctrl->spm_ddren_req = val;
	else if (!strcmp(cmd, "spm_flag_keep_csyspwrupack_high"))
		pwrctrl->spm_flag_keep_csyspwrupack_high = val;
	else if (!strcmp(cmd, "spm_flag_dis_vproc_vsram_dvs"))
		pwrctrl->spm_flag_dis_vproc_vsram_dvs = val;
	else if (!strcmp(cmd, "spm_flag_run_common_scenario"))
		pwrctrl->spm_flag_run_common_scenario = val;
	else if (!strcmp(cmd, "cpu_md_dvfs_sop_force_on"))
		pwrctrl->cpu_md_dvfs_sop_force_on = val;

	else if (!strcmp(cmd, "mcusys_idle_mask"))
		pwrctrl->mcusys_idle_mask = val;
	else if (!strcmp(cmd, "mp1top_idle_mask"))
		pwrctrl->mp1top_idle_mask = val;
	else if (!strcmp(cmd, "mp0top_idle_mask"))
		pwrctrl->mp0top_idle_mask = val;
	else if (!strcmp(cmd, "wfi_op"))
		pwrctrl->wfi_op = val;
	else if (!strcmp(cmd, "mp1_cpu0_wfi_en"))
		pwrctrl->mp1_cpu0_wfi_en = val;
	else if (!strcmp(cmd, "mp1_cpu1_wfi_en"))
		pwrctrl->mp1_cpu1_wfi_en = val;
	else if (!strcmp(cmd, "mp1_cpu2_wfi_en"))
		pwrctrl->mp1_cpu2_wfi_en = val;
	else if (!strcmp(cmd, "mp1_cpu3_wfi_en"))
		pwrctrl->mp1_cpu3_wfi_en = val;
	else if (!strcmp(cmd, "mp0_cpu0_wfi_en"))
		pwrctrl->mp0_cpu0_wfi_en = val;
	else if (!strcmp(cmd, "mp0_cpu1_wfi_en"))
		pwrctrl->mp0_cpu1_wfi_en = val;
	else if (!strcmp(cmd, "mp0_cpu2_wfi_en"))
		pwrctrl->mp0_cpu2_wfi_en = val;
	else if (!strcmp(cmd, "mp0_cpu3_wfi_en"))
		pwrctrl->mp0_cpu3_wfi_en = val;

	else if (!strcmp(cmd, "md1_req_mask_b"))
		pwrctrl->md1_req_mask_b = val;
	else if (!strcmp(cmd, "md2_req_mask_b"))
		pwrctrl->md2_req_mask_b = val;
	else if (!strcmp(cmd, "md_apsrc0_sel"))
		pwrctrl->md_apsrc0_sel = val;
	else if (!strcmp(cmd, "md_apsrc1_sel"))
		pwrctrl->md_apsrc1_sel = val;
	else if (!strcmp(cmd, "conn_apsrc_sel"))
		pwrctrl->conn_apsrc_sel = val;
	else if (!strcmp(cmd, "md_ddr_dbc_en"))
		pwrctrl->md_ddr_dbc_en = val;
	else if (!strcmp(cmd, "ccif0_to_ap_mask_b"))
		pwrctrl->ccif0_to_ap_mask_b = val;
	else if (!strcmp(cmd, "ccif0_to_md_mask_b"))
		pwrctrl->ccif0_to_md_mask_b = val;
	else if (!strcmp(cmd, "ccif1_to_ap_mask_b"))
		pwrctrl->ccif1_to_ap_mask_b = val;
	else if (!strcmp(cmd, "ccif1_to_md_mask_b"))
		pwrctrl->ccif1_to_md_mask_b = val;
	else if (!strcmp(cmd, "lte_mask_b"))
		pwrctrl->lte_mask_b = val;
	else if (!strcmp(cmd, "ccifmd_md1_event_mask_b"))
		pwrctrl->ccifmd_md1_event_mask_b = val;
	else if (!strcmp(cmd, "ccifmd_md2_event_mask_b"))
		pwrctrl->ccifmd_md2_event_mask_b = val;
	else if (!strcmp(cmd, "vsync_mask_b"))
		pwrctrl->vsync_mask_b = val;
	else if (!strcmp(cmd, "md_srcclkena_0_infra_mask_b"))
		pwrctrl->md_srcclkena_0_infra_mask_b = val;
	else if (!strcmp(cmd, "md_srcclkena_1_infra_mask_b"))
		pwrctrl->md_srcclkena_1_infra_mask_b = val;
	else if (!strcmp(cmd, "conn_srcclkena_infra_mask_b"))
		pwrctrl->conn_srcclkena_infra_mask_b = val;
	else if (!strcmp(cmd, "md32_srcclkena_infra_mask_b"))
		pwrctrl->md32_srcclkena_infra_mask_b = val;
	else if (!strcmp(cmd, "srcclkeni_infra_mask_b"))
		pwrctrl->srcclkeni_infra_mask_b = val;
	else if (!strcmp(cmd, "md_apsrcreq_0_infra_mask_b"))
		pwrctrl->md_apsrcreq_0_infra_mask_b = val;
	else if (!strcmp(cmd, "md_apsrcreq_1_infra_mask_b"))
		pwrctrl->md_apsrcreq_1_infra_mask_b = val;
	else if (!strcmp(cmd, "conn_apsrcreq_infra_mask_b"))
		pwrctrl->conn_apsrcreq_infra_mask_b = val;
	else if (!strcmp(cmd, "md32_apsrcreq_infra_mask_b"))
		pwrctrl->md32_apsrcreq_infra_mask_b = val;
	else if (!strcmp(cmd, "md_ddr_en_0_mask_b"))
		pwrctrl->md_ddr_en_0_mask_b = val;
	else if (!strcmp(cmd, "md_ddr_en_1_mask_b"))
		pwrctrl->md_ddr_en_1_mask_b = val;
	else if (!strcmp(cmd, "md_vrf18_req_0_mask_b"))
		pwrctrl->md_vrf18_req_0_mask_b = val;
	else if (!strcmp(cmd, "md_vrf18_req_1_mask_b"))
		pwrctrl->md_vrf18_req_1_mask_b = val;
	else if (!strcmp(cmd, "emi_bw_dvfs_req_mask"))
		pwrctrl->emi_bw_dvfs_req_mask = val;
	else if (!strcmp(cmd, "md_srcclkena_0_dvfs_req_mask_b"))
		pwrctrl->md_srcclkena_0_dvfs_req_mask_b = val;
	else if (!strcmp(cmd, "md_srcclkena_1_dvfs_req_mask_b"))
		pwrctrl->md_srcclkena_1_dvfs_req_mask_b = val;
	else if (!strcmp(cmd, "conn_srcclkena_dvfs_req_mask_b"))
		pwrctrl->conn_srcclkena_dvfs_req_mask_b = val;

	else if (!strcmp(cmd, "dvfs_halt_mask_b"))
		pwrctrl->dvfs_halt_mask_b = val;
	else if (!strcmp(cmd, "vdec_req_mask_b"))
		pwrctrl->vdec_req_mask_b = val;
	else if (!strcmp(cmd, "gce_req_mask_b"))
		pwrctrl->gce_req_mask_b = val;
	else if (!strcmp(cmd, "cpu_md_dvfs_erq_merge_mask_b"))
		pwrctrl->cpu_md_dvfs_erq_merge_mask_b = val;
	else if (!strcmp(cmd, "md1_ddr_en_dvfs_halt_mask_b"))
		pwrctrl->md1_ddr_en_dvfs_halt_mask_b = val;
	else if (!strcmp(cmd, "md2_ddr_en_dvfs_halt_mask_b"))
		pwrctrl->md2_ddr_en_dvfs_halt_mask_b = val;
	else if (!strcmp(cmd, "vsync_dvfs_halt_mask_b"))
		pwrctrl->vsync_dvfs_halt_mask_b = val;
	else if (!strcmp(cmd, "conn_ddr_en_mask_b"))
		pwrctrl->conn_ddr_en_mask_b = val;
	else if (!strcmp(cmd, "disp_req_mask_b"))
		pwrctrl->disp_req_mask_b = val;
	else if (!strcmp(cmd, "disp1_req_mask_b"))
		pwrctrl->disp1_req_mask_b = val;
	else if (!strcmp(cmd, "mfg_req_mask_b"))
		pwrctrl->mfg_req_mask_b = val;
	else if (!strcmp(cmd, "c2k_ps_rccif_wake_mask_b"))
		pwrctrl->c2k_ps_rccif_wake_mask_b = val;
	else if (!strcmp(cmd, "c2k_l1_rccif_wake_mask_b"))
		pwrctrl->c2k_l1_rccif_wake_mask_b = val;
	else if (!strcmp(cmd, "ps_c2k_rccif_wake_mask_b"))
		pwrctrl->ps_c2k_rccif_wake_mask_b = val;
	else if (!strcmp(cmd, "l1_c2k_rccif_wake_mask_b"))
		pwrctrl->l1_c2k_rccif_wake_mask_b = val;
	else if (!strcmp(cmd, "sdio_on_dvfs_req_mask_b"))
		pwrctrl->sdio_on_dvfs_req_mask_b = val;
	else if (!strcmp(cmd, "emi_boost_dvfs_req_mask_b"))
		pwrctrl->emi_boost_dvfs_req_mask_b = val;
	else if (!strcmp(cmd, "cpu_md_emi_dvfs_req_prot_dis"))
		pwrctrl->cpu_md_emi_dvfs_req_prot_dis = val;
#if defined(CONFIG_ARCH_MT6797)
	else if (!strcmp(cmd, "disp_od_req_mask_b"))
		pwrctrl->disp_od_req_mask_b = val;
#endif

	else if (!strcmp(cmd, "conn_mask_b"))
		pwrctrl->conn_mask_b = val;

	else if (!strcmp(cmd, "dsi0_ddr_en_mask_b"))
		pwrctrl->dsi0_ddr_en_mask_b = val;
	else if (!strcmp(cmd, "dsi1_ddr_en_mask_b"))
		pwrctrl->dsi1_ddr_en_mask_b = val;
	else if (!strcmp(cmd, "dpi_ddr_en_mask_b"))
		pwrctrl->dpi_ddr_en_mask_b = val;
	else if (!strcmp(cmd, "isp0_ddr_en_mask_b"))
		pwrctrl->isp0_ddr_en_mask_b = val;
	else if (!strcmp(cmd, "isp1_ddr_en_mask_b"))
		pwrctrl->isp1_ddr_en_mask_b = val;

	else if (!strcmp(cmd, "scp_req_mask_b"))
		pwrctrl->scp_req_mask_b = val;
	else if (!strcmp(cmd, "syspwreq_mask"))
		pwrctrl->syspwreq_mask = val;
	else if (!strcmp(cmd, "srclkenai_mask"))
		pwrctrl->srclkenai_mask = val;

	else if (!strcmp(cmd, "param1"))
		pwrctrl->param1 = val;
	else if (!strcmp(cmd, "param2"))
		pwrctrl->param2 = val;
	else if (!strcmp(cmd, "param3"))
		pwrctrl->param3 = val;
	else
		return -EINVAL;

	return count;
}
#endif

static ssize_t suspend_ctrl_store(struct kobject *kobj, struct kobj_attribute *attr,
				  const char *buf, size_t count)
{
	return store_pwr_ctrl(__spm_suspend.pwrctrl, buf, count);
}

static ssize_t dpidle_ctrl_store(struct kobject *kobj, struct kobj_attribute *attr,
				 const char *buf, size_t count)
{
	return store_pwr_ctrl(__spm_dpidle.pwrctrl, buf, count);
}

#if !defined(CONFIG_FPGA_EARLY_PORTING)
static ssize_t sodi3_ctrl_store(struct kobject *kobj, struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	return store_pwr_ctrl(__spm_sodi3.pwrctrl, buf, count);
}

static ssize_t sodi_ctrl_store(struct kobject *kobj, struct kobj_attribute *attr,
			       const char *buf, size_t count)
{
	return store_pwr_ctrl(__spm_sodi.pwrctrl, buf, count);
}

static ssize_t mcdi_ctrl_store(struct kobject *kobj, struct kobj_attribute *attr,
			       const char *buf, size_t count)
{
	return 0;
}
#endif

static ssize_t talking_ctrl_store(struct kobject *kobj, struct kobj_attribute *attr,
				  const char *buf, size_t count)
{
#if 0
	return store_pwr_ctrl(__spm_talking.pwrctrl, buf, count);
#else
	return 0;
#endif
}

static ssize_t vcore_dvfs_ctrl_store(struct kobject *kobj, struct kobj_attribute *attr,
				     const char *buf, size_t count)
{
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	return store_pwr_ctrl(__spm_vcore_dvfs.pwrctrl, buf, count);
#else
	return 0;
#endif
}


/**************************************
 * ddren_debug_xxx Function
 **************************************/
static ssize_t ddren_debug_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	char *p = buf;

	p += sprintf(p, "PCM_REG13_DATA = 0x%x\n", spm_read(PCM_REG13_DATA));
	p += sprintf(p, "AP_STANBY_CON = 0x%x\n", spm_read(SPM_AP_STANDBY_CON));
	p += sprintf(p, "PCM_DEBUG_CON = 0x%x\n", spm_read(PCM_DEBUG_CON));
	p += sprintf(p, "PCM_PASR_DPD_2 = 0x%x\n", spm_read(SPM_PASR_DPD_2));

	BUG_ON(p - buf >= PAGE_SIZE);
	return p - buf;
}

static ssize_t ddren_debug_store(struct kobject *kobj, struct kobj_attribute *attr,
				 const char *buf, size_t count)
{
	u32 val, con;
	char cmd[32];
	unsigned long flags;

	if (sscanf(buf, "%31s %x", cmd, &val) != 2)
		return -EPERM;

	spm_debug("ddren_debug: cmd = %s, val = 0x%x\n", cmd, val);

	if (!strcmp(cmd, "ddr_en_sel")) {
		spin_lock_irqsave(&__spm_lock, flags);
		con = spm_read(SPM_AP_STANDBY_CON) & ~MD_DDR_EN_DBC_EN_LSB;
		spm_write(SPM_AP_STANDBY_CON, con | (!!val << 18));
		spin_unlock_irqrestore(&__spm_lock, flags);
	} else if (!strcmp(cmd, "md_ddr_en_out")) {
		spin_lock_irqsave(&__spm_lock, flags);
		__spm_dbgout_md_ddr_en(val);
		spin_unlock_irqrestore(&__spm_lock, flags);
	} else if (!strcmp(cmd, "mm_ddr_en_mask")) {
		spin_lock_irqsave(&__spm_lock, flags);
		spm_write(SPM_PASR_DPD_2, ~val & 0x1f);
		spin_unlock_irqrestore(&__spm_lock, flags);
	} else {
		return -EINVAL;
	}

	return count;
}

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
 * auto_suspend_resume_xxx Function
 **************************************/
#if 0				/* FIXME */
static ssize_t auto_suspend_resume_show(struct kobject *kobj, struct kobj_attribute *attr,
					char *buf)
{
	char *p = buf;
	u8 i;

	p += sprintf(p, "auto_suspend_resume:%d times\n", 10);

	for (i = 0; i < 10; i++) {
		p += sprintf(p, "[%d]wakeup:0x%x,timer:0x%x,r13:0x%x,event=0x%x,flag=0x%x\n",
			     __spm_suspend.wakestatus[i].log_index,
			     __spm_suspend.wakestatus[i].r12,
			     __spm_suspend.wakestatus[i].r12_ext,
			     __spm_suspend.wakestatus[i].timer_out,
			     __spm_suspend.wakestatus[i].r13,
			     __spm_suspend.wakestatus[i].event_reg,
			     __spm_suspend.wakestatus[i].debug_flag);
		if (0x90100000 != __spm_suspend.wakestatus[i].event_reg)
			p += sprintf(p, "SLEEP_ABORT\n");
		else if (0xf != (__spm_suspend.wakestatus[i].debug_flag & 0xF))
			p += sprintf(p, "NOT_DEEP_SLEEP\n");
		else
			p += sprintf(p, "SLEEP_PASS\n");
	}

	slp_set_auto_suspend_wakelock(0);

	BUG_ON(p - buf >= PAGE_SIZE);
	return p - buf;
}

static ssize_t auto_suspend_resume_store(struct kobject *kobj, struct kobj_attribute *attr,
					 const char *buf, size_t count)
{
	u32 val, pcm_sec;

	if (sscanf(buf, "%d %d", &val, &pcm_sec) != 2) {
		spm_debug("auto_suspend_resume parameter fail\n");
		return -EPERM;
	}
	spm_debug("auto_suspend_resume val = %d, pcm_sec = %d\n", val, pcm_sec);
	__spm_suspend.pwrctrl->timer_val_cust = pcm_sec * 32768;
	slp_create_auto_suspend_resume_thread();
	slp_start_auto_suspend_resume_timer(val);

	return count;
}
#endif				/* 0 */

/**************************************
 * Init Function
 **************************************/
/* DEFINE_ATTR_RO(suspend_pcm); */
/* DEFINE_ATTR_RO(dpidle_pcm); */
/* DEFINE_ATTR_RO(sodi3_pcm); */
/* DEFINE_ATTR_RO(sodi_pcm); */
/* DEFINE_ATTR_RO(mcdi_pcm); */
/* DEFINE_ATTR_RO(talking_pcm); */
/* DEFINE_ATTR_RO(ddrdfs_pcm); */

DEFINE_ATTR_RW(suspend_ctrl);
DEFINE_ATTR_RW(dpidle_ctrl);
#if !defined(CONFIG_FPGA_EARLY_PORTING)
DEFINE_ATTR_RW(sodi3_ctrl);
DEFINE_ATTR_RW(sodi_ctrl);
DEFINE_ATTR_RW(mcdi_ctrl);
#endif
DEFINE_ATTR_RW(talking_ctrl);
DEFINE_ATTR_RW(vcore_dvfs_ctrl);

DEFINE_ATTR_RW(ddren_debug);
DEFINE_ATTR_RO(fm_suspend);

/* DEFINE_ATTR_RW(auto_suspend_resume); */

static struct attribute *spm_attrs[] = {
	/* for spm_lp_scen.pcmdesc */
	/* __ATTR_OF(suspend_pcm), */
	/* __ATTR_OF(dpidle_pcm), */
	/* __ATTR_OF(sodi3_pcm), */
	/* __ATTR_OF(sodi_pcm), */
	/* __ATTR_OF(mcdi_pcm), */
	/* __ATTR_OF(talking_pcm), */
	/* __ATTR_OF(vcore_dvfs_pcm), */

	/* for spm_lp_scen.pwrctrl */
	__ATTR_OF(suspend_ctrl),
	__ATTR_OF(dpidle_ctrl),
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	__ATTR_OF(sodi3_ctrl),
	__ATTR_OF(sodi_ctrl),
	__ATTR_OF(mcdi_ctrl),
#endif
	__ATTR_OF(talking_ctrl),
	__ATTR_OF(vcore_dvfs_ctrl),

	/* other debug interface */
	__ATTR_OF(ddren_debug),
	__ATTR_OF(fm_suspend),

	/* __ATTR_OF(auto_suspend_resume), */

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
