/*
 * bootinfo.c
 *
 * Copyright (C) 2011 Xiaomi Ltd.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <asm/setup.h>
#include <asm/bootinfo.h>
#include <linux/bitops.h>
#include <linux/input/qpnp-power-on.h>

static const char * const powerup_reasons[PU_REASON_MAX] = {
	[PU_REASON_EVENT_KPD]		= "keypad",
	[PU_REASON_EVENT_RTC]		= "rtc",
	[PU_REASON_EVENT_CABLE]		= "cable",
	[PU_REASON_EVENT_SMPL]		= "smpl",
	[PU_REASON_EVENT_PON1]		= "pon1",
	[PU_REASON_EVENT_USB_CHG]	= "usb_chg",
	[PU_REASON_EVENT_DC_CHG]	= "dc_chg",
	[PU_REASON_EVENT_HWRST]		= "hw_reset",
	[PU_REASON_EVENT_LPK]		= "long_power_key",
};

static const char * const reset_reasons[RS_REASON_MAX] = {
	[RS_REASON_EVENT_WDOG]		= "wdog",
	[RS_REASON_EVENT_KPANIC]	= "kpanic",
	[RS_REASON_EVENT_NORMAL]	= "reboot",
	[RS_REASON_EVENT_OTHER]		= "other",
	[RS_REASON_EVENT_DVE]		= "dm_verity_enforcing",
	[RS_REASON_EVENT_DVL]		= "dm_verity_logging",
	[RS_REASON_EVENT_DVK]		= "dm_verity_keysclear",
	[RS_REASON_EVENT_FASTBOOT]	= "fastboot_reboot",
};

static struct kobject *bootinfo_kobj;
static powerup_reason_t powerup_reason;

#define bootinfo_attr(_name) \
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = __stringify(_name),	\
		.mode = 0644,			\
	},					\
	.show	= _name##_show,			\
	.store	= NULL,				\
}

#define bootinfo_func_init(type, name, initval)	\
static type name = (initval);			\
type get_##name(void)				\
{						\
	return name;				\
}						\
void set_##name(type __##name)			\
{						\
	name = __##name;			\
}

int is_abnormal_powerup(void)
{
	u32 pu_reason = get_powerup_reason();

	return (pu_reason & (RESTART_EVENT_KPANIC | RESTART_EVENT_WDOG)) |
		(pu_reason & BIT(PU_REASON_EVENT_HWRST) & RESTART_EVENT_OTHER);
}

static ssize_t powerup_reason_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	char *s = buf;
	u32 pu_reason;
	int pu_reason_index = PU_REASON_MAX;
	u32 reset_reason;
	int reset_reason_index = RS_REASON_MAX;

	pu_reason = get_powerup_reason();
	if (((pu_reason & BIT(PU_REASON_EVENT_HWRST))
		&& qpnp_pon_is_ps_hold_reset()) ||
		(pu_reason & BIT(PU_REASON_EVENT_WARMRST))) {
		reset_reason = pu_reason >> 16;
		reset_reason_index =
		find_first_bit((unsigned long *)&reset_reason,
				sizeof(reset_reason)*BITS_PER_BYTE);
		if (reset_reason_index < RS_REASON_MAX
			&& reset_reason_index >= 0) {
			if (reset_reason_index == RS_REASON_EVENT_FASTBOOT)
				reset_reason_index = RS_REASON_EVENT_NORMAL;
			s += snprintf(s,
				strlen(reset_reasons[reset_reason_index]) + 2,
				"%s\n", reset_reasons[reset_reason_index]);
			pr_debug("%s: rs_reason [0x%x], first non-zero bit %d\n",
				__func__, reset_reason, reset_reason_index);
			goto out;
		};
	}
	if (qpnp_pon_is_lpk() &&
		(pu_reason & BIT(PU_REASON_EVENT_HWRST)))
		pu_reason_index = PU_REASON_EVENT_LPK;
	else if (pu_reason & BIT(PU_REASON_EVENT_HWRST))
		pu_reason_index = PU_REASON_EVENT_HWRST;
	else if (pu_reason & BIT(PU_REASON_EVENT_SMPL))
		pu_reason_index = PU_REASON_EVENT_SMPL;
	else if (pu_reason & BIT(PU_REASON_EVENT_RTC))
		pu_reason_index = PU_REASON_EVENT_RTC;
	else if (pu_reason & BIT(PU_REASON_EVENT_USB_CHG))
		pu_reason_index = PU_REASON_EVENT_USB_CHG;
	else if (pu_reason & BIT(PU_REASON_EVENT_DC_CHG))
		pu_reason_index = PU_REASON_EVENT_DC_CHG;
	else if (pu_reason & BIT(PU_REASON_EVENT_KPD))
		pu_reason_index = PU_REASON_EVENT_KPD;
	else if (pu_reason & BIT(PU_REASON_EVENT_PON1))
		pu_reason_index = PU_REASON_EVENT_PON1;
	if (pu_reason_index < PU_REASON_MAX && pu_reason_index >= 0) {
		s += snprintf(s, strlen(powerup_reasons[pu_reason_index]) + 2,
				"%s\n", powerup_reasons[pu_reason_index]);
		pr_debug("%s: pu_reason [0x%x] index %d\n",
			__func__, pu_reason, pu_reason_index);
		goto out;
	}
	s += snprintf(s, 15, "unknown reboot\n");
out:
	return (s - buf);
}

static ssize_t powerup_reason_details_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	u32 pu_reason;

	pu_reason = get_powerup_reason();

	return snprintf(buf, 11, "0x%x\n", pu_reason);
}

bootinfo_attr(powerup_reason);
bootinfo_attr(powerup_reason_details);
bootinfo_func_init(u32, powerup_reason, 0);

static struct attribute *g[] = {
	&powerup_reason_attr.attr,
	&powerup_reason_details_attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = g,
};

static int cpumaxfreq_show(struct seq_file *m, void *v)
{
	/* value is used for setting cpumaxfreq */
	seq_printf(m, "2.2\n");

	return 0;
}

static int cpumaxfreq_open(struct inode *inode, struct file *file)
{
	return single_open(file, &cpumaxfreq_show, NULL);
};

static const struct file_operations proc_cpumaxfreq_operations = {
	.open       = cpumaxfreq_open,
	.read       = seq_read,
	.llseek     = seq_lseek,
	.release    = seq_release,
};

static int __init bootinfo_init(void)
{
	int ret = -ENOMEM;

	bootinfo_kobj = kobject_create_and_add("bootinfo", NULL);
	if (bootinfo_kobj == NULL) {
		pr_err("bootinfo_init: subsystem_register failed\n");
		goto fail;
	}

	ret = sysfs_create_group(bootinfo_kobj, &attr_group);
	if (ret) {
		pr_err("bootinfo_init: subsystem_register failed\n");
		goto sys_fail;
	}
	proc_create("cpumaxfreq", 0, NULL, &proc_cpumaxfreq_operations);

	return ret;

sys_fail:
	kobject_del(bootinfo_kobj);
fail:
	return ret;

}

static void __exit bootinfo_exit(void)
{
	if (bootinfo_kobj) {
		sysfs_remove_group(bootinfo_kobj, &attr_group);
		kobject_del(bootinfo_kobj);
	}
}

core_initcall(bootinfo_init);
module_exit(bootinfo_exit);
