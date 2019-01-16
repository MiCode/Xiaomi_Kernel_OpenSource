#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/string.h>

#include "mt_spm_internal.h"
#include <mach/mt_sleep.h>

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
static ssize_t show_pcm_desc(const struct pcm_desc *pcmdesc, char *buf)
{
	char *p = buf;

	p += sprintf(p, "version = %s\n", pcmdesc->version);
	p += sprintf(p, "base = 0x%p\n" , pcmdesc->base);
	p += sprintf(p, "size = %u\n"   , pcmdesc->size);
	p += sprintf(p, "sess = %u\n"   , pcmdesc->sess);
	p += sprintf(p, "replace = %u\n", pcmdesc->replace);

	p += sprintf(p, "vec0 = 0x%x\n" , pcmdesc->vec0);
	p += sprintf(p, "vec1 = 0x%x\n" , pcmdesc->vec1);
	p += sprintf(p, "vec2 = 0x%x\n" , pcmdesc->vec2);
	p += sprintf(p, "vec3 = 0x%x\n" , pcmdesc->vec3);
	p += sprintf(p, "vec4 = 0x%x\n" , pcmdesc->vec4);
	p += sprintf(p, "vec5 = 0x%x\n" , pcmdesc->vec5);
	p += sprintf(p, "vec6 = 0x%x\n" , pcmdesc->vec6);
	p += sprintf(p, "vec7 = 0x%x\n" , pcmdesc->vec7);

	BUG_ON(p - buf >= PAGE_SIZE);
	return p - buf;
}

static ssize_t suspend_pcm_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	return show_pcm_desc(__spm_suspend.pcmdesc, buf);
}

static ssize_t dpidle_pcm_show(struct kobject *kobj, struct kobj_attribute *attr,
			       char *buf)
{
	return show_pcm_desc(__spm_dpidle.pcmdesc, buf);
}

static ssize_t sodi_pcm_show(struct kobject *kobj, struct kobj_attribute *attr,
			     char *buf)
{
	return show_pcm_desc(__spm_sodi.pcmdesc, buf);
}

static ssize_t mcdi_pcm_show(struct kobject *kobj, struct kobj_attribute *attr,
			     char *buf)
{
	return show_pcm_desc(__spm_mcdi.pcmdesc, buf);
}

static ssize_t talking_pcm_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	return show_pcm_desc(__spm_talking.pcmdesc, buf);
}

static ssize_t ddrdfs_pcm_show(struct kobject *kobj, struct kobj_attribute *attr,
			       char *buf)
{
	return show_pcm_desc(__spm_ddrdfs.pcmdesc, buf);
}


/**************************************
 * xxx_ctrl_show Function
 **************************************/
static ssize_t show_pwr_ctrl(const struct pwr_ctrl *pwrctrl, char *buf)
{
	char *p = buf;

	p += sprintf(p, "pcm_flags = 0x%x\n"      , pwrctrl->pcm_flags);
	p += sprintf(p, "pcm_flags_cust = 0x%x\n" , pwrctrl->pcm_flags_cust);
	p += sprintf(p, "pcm_reserve = 0x%x\n"    , pwrctrl->pcm_reserve);
	p += sprintf(p, "timer_val = 0x%x\n"      , pwrctrl->timer_val);
	p += sprintf(p, "timer_val_cust = 0x%x\n" , pwrctrl->timer_val_cust);
	p += sprintf(p, "wake_src = 0x%x\n"       , pwrctrl->wake_src);
	p += sprintf(p, "wake_src_cust = 0x%x\n"  , pwrctrl->wake_src_cust);
	p += sprintf(p, "wake_src_md32 = 0x%x\n"  , pwrctrl->wake_src_md32);
	p += sprintf(p, "r0_ctrl_en = %u\n"       , pwrctrl->r0_ctrl_en);
	p += sprintf(p, "r7_ctrl_en = %u\n"       , pwrctrl->r7_ctrl_en);
	p += sprintf(p, "infra_dcm_lock = %u\n"   , pwrctrl->infra_dcm_lock);
	p += sprintf(p, "pcm_apsrc_req = %u\n"    , pwrctrl->pcm_apsrc_req);

	p += sprintf(p, "mcusys_idle_mask = %u\n" , pwrctrl->mcusys_idle_mask);
	p += sprintf(p, "ca15top_idle_mask = %u\n", pwrctrl->ca15top_idle_mask);
	p += sprintf(p, "ca7top_idle_mask = %u\n" , pwrctrl->ca7top_idle_mask);
	p += sprintf(p, "wfi_op = %u\n"           , pwrctrl->wfi_op);
	p += sprintf(p, "ca15_wfi0_en = %u\n"     , pwrctrl->ca15_wfi0_en);
	p += sprintf(p, "ca15_wfi1_en = %u\n"     , pwrctrl->ca15_wfi1_en);
	p += sprintf(p, "ca15_wfi2_en = %u\n"     , pwrctrl->ca15_wfi2_en);
	p += sprintf(p, "ca15_wfi3_en = %u\n"     , pwrctrl->ca15_wfi3_en);
	p += sprintf(p, "ca7_wfi0_en = %u\n"      , pwrctrl->ca7_wfi0_en);
	p += sprintf(p, "ca7_wfi1_en = %u\n"      , pwrctrl->ca7_wfi1_en);
	p += sprintf(p, "ca7_wfi2_en = %u\n"      , pwrctrl->ca7_wfi2_en);
	p += sprintf(p, "ca7_wfi3_en = %u\n"      , pwrctrl->ca7_wfi3_en);

	p += sprintf(p, "md1_req_mask = %u\n"     , pwrctrl->md1_req_mask);
	p += sprintf(p, "md2_req_mask = %u\n"     , pwrctrl->md2_req_mask);
	p += sprintf(p, "md_apsrc_sel = %u\n"     , pwrctrl->md_apsrc_sel);
	p += sprintf(p, "md_ddr_dbc_en = %u\n"    , pwrctrl->md_ddr_dbc_en);
	p += sprintf(p, "ccif_to_ap_mask = %u\n"  , pwrctrl->ccif_to_ap_mask);
	p += sprintf(p, "ccif_to_md_mask = %u\n"  , pwrctrl->ccif_to_md_mask);

	p += sprintf(p, "disp_req_mask = %u\n"    , pwrctrl->disp_req_mask);
	p += sprintf(p, "mfg_req_mask = %u\n"     , pwrctrl->mfg_req_mask);
	p += sprintf(p, "dsi0_ddr_en_mask = %u\n" , pwrctrl->dsi0_ddr_en_mask);
	p += sprintf(p, "dsi1_ddr_en_mask = %u\n" , pwrctrl->dsi1_ddr_en_mask);
	p += sprintf(p, "dpi_ddr_en_mask = %u\n"  , pwrctrl->dpi_ddr_en_mask);
	p += sprintf(p, "isp0_ddr_en_mask = %u\n" , pwrctrl->isp0_ddr_en_mask);
	p += sprintf(p, "isp1_ddr_en_mask = %u\n" , pwrctrl->isp1_ddr_en_mask);

	p += sprintf(p, "md32_req_mask = %u\n"    , pwrctrl->md32_req_mask);
	p += sprintf(p, "syspwreq_mask = %u\n"    , pwrctrl->syspwreq_mask);
	p += sprintf(p, "srclkenai_mask = %u\n"   , pwrctrl->srclkenai_mask);

	p += sprintf(p, "param1 = 0x%x\n"         , pwrctrl->param1);
	p += sprintf(p, "param2 = 0x%x\n"         , pwrctrl->param2);
	p += sprintf(p, "param3 = 0x%x\n"         , pwrctrl->param3);

	BUG_ON(p - buf >= PAGE_SIZE);
	return p - buf;
}

static ssize_t suspend_ctrl_show(struct kobject *kobj, struct kobj_attribute *attr,
				 char *buf)
{
	return show_pwr_ctrl(__spm_suspend.pwrctrl, buf);
}

static ssize_t dpidle_ctrl_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	return show_pwr_ctrl(__spm_dpidle.pwrctrl, buf);
}

static ssize_t sodi_ctrl_show(struct kobject *kobj, struct kobj_attribute *attr,
			      char *buf)
{
	return show_pwr_ctrl(__spm_sodi.pwrctrl, buf);
}

static ssize_t mcdi_ctrl_show(struct kobject *kobj, struct kobj_attribute *attr,
			      char *buf)
{
	return show_pwr_ctrl(__spm_mcdi.pwrctrl, buf);
}

static ssize_t talking_ctrl_show(struct kobject *kobj, struct kobj_attribute *attr,
				 char *buf)
{
	return show_pwr_ctrl(__spm_talking.pwrctrl, buf);
}

static ssize_t ddrdfs_ctrl_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	return show_pwr_ctrl(__spm_ddrdfs.pwrctrl, buf);
}


/**************************************
 * xxx_ctrl_store Function
 **************************************/
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
	else if (!strcmp(cmd, "pcm_apsrc_req"))
		pwrctrl->pcm_apsrc_req = val;

	else if (!strcmp(cmd, "mcusys_idle_mask"))
		pwrctrl->mcusys_idle_mask = val;
	else if (!strcmp(cmd, "ca15top_idle_mask"))
		pwrctrl->ca15top_idle_mask = val;
	else if (!strcmp(cmd, "ca7top_idle_mask"))
		pwrctrl->ca7top_idle_mask = val;
	else if (!strcmp(cmd, "wfi_op"))
		pwrctrl->wfi_op = val;
	else if (!strcmp(cmd, "ca15_wfi0_en"))
		pwrctrl->ca15_wfi0_en = val;
	else if (!strcmp(cmd, "ca15_wfi1_en"))
		pwrctrl->ca15_wfi1_en = val;
	else if (!strcmp(cmd, "ca15_wfi2_en"))
		pwrctrl->ca15_wfi2_en = val;
	else if (!strcmp(cmd, "ca15_wfi3_en"))
		pwrctrl->ca15_wfi3_en = val;
	else if (!strcmp(cmd, "ca7_wfi0_en"))
		pwrctrl->ca7_wfi0_en = val;
	else if (!strcmp(cmd, "ca7_wfi1_en"))
		pwrctrl->ca7_wfi1_en = val;
	else if (!strcmp(cmd, "ca7_wfi2_en"))
		pwrctrl->ca7_wfi2_en = val;
	else if (!strcmp(cmd, "ca7_wfi3_en"))
		pwrctrl->ca7_wfi3_en = val;

	else if (!strcmp(cmd, "md1_req_mask"))
		pwrctrl->md1_req_mask = val;
	else if (!strcmp(cmd, "md2_req_mask"))
		pwrctrl->md2_req_mask = val;
	else if (!strcmp(cmd, "md_apsrc_sel"))
		pwrctrl->md_apsrc_sel = val;
	else if (!strcmp(cmd, "md_ddr_dbc_en"))
		pwrctrl->md_ddr_dbc_en = val;
	else if (!strcmp(cmd, "ccif_to_ap_mask"))
		pwrctrl->ccif_to_ap_mask = val;
	else if (!strcmp(cmd, "ccif_to_md_mask"))
		pwrctrl->ccif_to_md_mask = val;

	else if (!strcmp(cmd, "disp_req_mask"))
		pwrctrl->disp_req_mask = val;
	else if (!strcmp(cmd, "mfg_req_mask"))
		pwrctrl->mfg_req_mask = val;
	else if (!strcmp(cmd, "dsi0_ddr_en_mask"))
		pwrctrl->dsi0_ddr_en_mask = val;
	else if (!strcmp(cmd, "dsi1_ddr_en_mask"))
		pwrctrl->dsi1_ddr_en_mask = val;
	else if (!strcmp(cmd, "dpi_ddr_en_mask"))
		pwrctrl->dpi_ddr_en_mask = val;
	else if (!strcmp(cmd, "isp0_ddr_en_mask"))
		pwrctrl->isp0_ddr_en_mask = val;
	else if (!strcmp(cmd, "isp1_ddr_en_mask"))
		pwrctrl->isp1_ddr_en_mask = val;

	else if (!strcmp(cmd, "md32_req_mask"))
		pwrctrl->md32_req_mask = val;
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

static ssize_t sodi_ctrl_store(struct kobject *kobj, struct kobj_attribute *attr,
			       const char *buf, size_t count)
{
	return store_pwr_ctrl(__spm_sodi.pwrctrl, buf, count);
}

static ssize_t mcdi_ctrl_store(struct kobject *kobj, struct kobj_attribute *attr,
			       const char *buf, size_t count)
{
	return store_pwr_ctrl(__spm_mcdi.pwrctrl, buf, count);
}

static ssize_t talking_ctrl_store(struct kobject *kobj, struct kobj_attribute *attr,
				  const char *buf, size_t count)
{
	return store_pwr_ctrl(__spm_talking.pwrctrl, buf, count);
}

static ssize_t ddrdfs_ctrl_store(struct kobject *kobj, struct kobj_attribute *attr,
				 const char *buf, size_t count)
{
	return store_pwr_ctrl(__spm_ddrdfs.pwrctrl, buf, count);
}


/**************************************
 * ddren_debug_xxx Function
 **************************************/
static ssize_t ddren_debug_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	char *p = buf;

	p += sprintf(p, "PCM_REG13_DATA = 0x%x\n", spm_read(SPM_PCM_REG13_DATA));
	p += sprintf(p, "AP_STANBY_CON = 0x%x\n" , spm_read(SPM_AP_STANBY_CON));
	p += sprintf(p, "PCM_DEBUG_CON = 0x%x\n" , spm_read(SPM_PCM_DEBUG_CON));
	p += sprintf(p, "PCM_PASR_DPD_2 = 0x%x\n", spm_read(SPM_PCM_PASR_DPD_2));

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
		con = spm_read(SPM_AP_STANBY_CON) & ~(1U << 22);
		spm_write(SPM_AP_STANBY_CON, con | (!!val << 22));
		spin_unlock_irqrestore(&__spm_lock, flags);
	} else if (!strcmp(cmd, "md_ddr_en_out")) {
		spin_lock_irqsave(&__spm_lock, flags);
		__spm_dbgout_md_ddr_en(val);
		spin_unlock_irqrestore(&__spm_lock, flags);
	} else if (!strcmp(cmd, "mm_ddr_en_mask")) {
		spin_lock_irqsave(&__spm_lock, flags);
		spm_write(SPM_PCM_PASR_DPD_2, ~val & 0x1f);
		spin_unlock_irqrestore(&__spm_lock, flags);
	} else {
		return -EINVAL;
	}

	return count;
}


/**************************************
 * golden_dump_xxx Function
 **************************************/
static ssize_t golden_dump_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	char *p = buf;

	p += sprintf(p, "[TOPCKGEN]\n");
	p += sprintf(p, "0x10000200 = 0x%x (0x7ff)\n", spm_read(0xf0000200));
	p += sprintf(p, "0x10000204 = 0x%x (0x15)\n" , spm_read(0xf0000204));

	p += sprintf(p, "[DDRPHY]\n");
	p += sprintf(p, "0x1000f5c8 = 0x%x ([0]=1,[4]=1)\n" , spm_read(0xf000f5c8));
	p += sprintf(p, "0x100125c8 = 0x%x ([0]=1,[4]=1)\n" , spm_read(0xf00125c8));
	p += sprintf(p, "0x1000f640 = 0x%x ([2]=1,[4]=1)\n" , spm_read(0xf000f640));
	p += sprintf(p, "0x10012640 = 0x%x ([4]=1)\n"       , spm_read(0xf0012640));
	p += sprintf(p, "0x1000f5cc = 0x%x ([8]=0,[12]=1)\n", spm_read(0xf000f5cc));
	p += sprintf(p, "0x100125cc = 0x%x ([8]=0,[12]=1)\n", spm_read(0xf00125cc));
	p += sprintf(p, "0x1000f690 = 0x%x ([2]=1)\n"       , spm_read(0xf000f690));

	p += sprintf(p, "[PERICFG]\n");
	p += sprintf(p, "0x10003208 = 0x%x ([15]=1:4GB)\n", spm_read(0xf0003208));

	BUG_ON(p - buf >= PAGE_SIZE);
	return p - buf;
}

/**************************************
 * auto_suspend_resume_xxx Function
 **************************************/
static ssize_t auto_suspend_resume_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	char *p = buf;
    u8 i;

	p += sprintf(p, "auto_suspend_resume:%d times\n", 10);

    for (i=0;i<10;i++)
    {
        p += sprintf(p, "[%d]wakeup:0x%x,timer:0x%x,r13:0x%x,event=0x%x,flag=0x%x\n",
                         __spm_suspend.wakestatus[i].log_index,
                         __spm_suspend.wakestatus[i].r12,
                         __spm_suspend.wakestatus[i].timer_out,
                         __spm_suspend.wakestatus[i].r13,
                         __spm_suspend.wakestatus[i].event_reg,
                         __spm_suspend.wakestatus[i].debug_flag);
        if (0x90100000 != __spm_suspend.wakestatus[i].event_reg)
            p += sprintf(p, "SLEEP_ABORT\n");
        else
        if (0x1f != (__spm_suspend.wakestatus[i].debug_flag & 0xFF))
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

	if (sscanf(buf, "%d %d", &val, &pcm_sec) != 2)
    {
        spm_debug("auto_suspend_resume parameter fail\n");
		return -EPERM;
    }
    spm_debug("auto_suspend_resume val = %d, pcm_sec = %d\n", val, pcm_sec);
    __spm_suspend.pwrctrl->timer_val_cust = pcm_sec*32768;
    slp_create_auto_suspend_resume_thread();
    slp_start_auto_suspend_resume_timer(val);

	return count;
}

/**************************************
 * Init Function
 **************************************/
DEFINE_ATTR_RO(suspend_pcm);
DEFINE_ATTR_RO(dpidle_pcm);
DEFINE_ATTR_RO(sodi_pcm);
DEFINE_ATTR_RO(mcdi_pcm);
DEFINE_ATTR_RO(talking_pcm);
DEFINE_ATTR_RO(ddrdfs_pcm);

DEFINE_ATTR_RW(suspend_ctrl);
DEFINE_ATTR_RW(dpidle_ctrl);
DEFINE_ATTR_RW(sodi_ctrl);
DEFINE_ATTR_RW(mcdi_ctrl);
DEFINE_ATTR_RW(talking_ctrl);
DEFINE_ATTR_RW(ddrdfs_ctrl);

DEFINE_ATTR_RW(ddren_debug);
DEFINE_ATTR_RO(golden_dump);

DEFINE_ATTR_RW(auto_suspend_resume);

static struct attribute *spm_attrs[] = {
	/* for spm_lp_scen.pcmdesc */
	__ATTR_OF(suspend_pcm),
	__ATTR_OF(dpidle_pcm),
	__ATTR_OF(sodi_pcm),
	__ATTR_OF(mcdi_pcm),
	__ATTR_OF(talking_pcm),
	__ATTR_OF(ddrdfs_pcm),

	/* for spm_lp_scen.pwrctrl */
	__ATTR_OF(suspend_ctrl),
	__ATTR_OF(dpidle_ctrl),
	__ATTR_OF(sodi_ctrl),
	__ATTR_OF(mcdi_ctrl),
	__ATTR_OF(talking_ctrl),
	__ATTR_OF(ddrdfs_ctrl),

	/* other debug interface */
	__ATTR_OF(ddren_debug),
	__ATTR_OF(golden_dump),

    __ATTR_OF(auto_suspend_resume),

	/* must */
	NULL,
};

static struct attribute_group spm_attr_group = {
	.name	= "spm",
	.attrs	= spm_attrs,
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

MODULE_DESCRIPTION("SPM-FS Driver v0.6");
