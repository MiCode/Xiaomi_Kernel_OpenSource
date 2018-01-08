/*
 * bootinfo.c
 * Copyright (C) 2017 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <asm/setup.h>
#include <asm/bootinfo.h>
#include <linux/bitops.h>
#include <linux/qpnp/power-on.h>

static const char * const powerup_reasons[PU_REASON_MAX] = {
	[PU_REASON_EVENT_KPD]		= "keypad",
	[PU_REASON_EVENT_RTC]		= "rtc",
	[PU_REASON_EVENT_CABLE]		= "cable",
	[PU_REASON_EVENT_SMPL]		= "smpl",
	[PU_REASON_EVENT_PON1]		= "pon1",
	[PU_REASON_EVENT_USB_CHG]		= "usb_chg",
	[PU_REASON_EVENT_DC_CHG]		= "dc_chg",
	[PU_REASON_EVENT_HWRST]		= "hw_reset",
	[PU_REASON_EVENT_LPK]		= "long_power_key",
};

static const char * const reset_reasons[RS_REASON_MAX] = {
	[RS_REASON_EVENT_WDOG]		= "wdog",
	[RS_REASON_EVENT_KPANIC]		= "kpanic",
	[RS_REASON_EVENT_NORMAL]		= "reboot",
	[RS_REASON_EVENT_OTHER]		= "other",
};

#define MAX_PU_RS_REASON_HWVER_LEN		64
static struct kobject *bootinfo_kobj;
static powerup_reason_t powerup_reason;
static unsigned int hw_version;

#define bootinfo_attr(_name) \
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = __stringify(_name),	\
		.mode = 0644,			\
	},					\
	.show	= _name##_show,			\
	.store	= NULL,				\
}

#define bootinfo_func_init(type, name, initval)		\
	static type name = (initval);		\
	type get_##name(void)			\
	{					\
		return name;			\
	}					\
	void set_##name(type __##name)		\
	{					\
		name = __##name;		\
	}					\
	EXPORT_SYMBOL(set_##name);		\
	EXPORT_SYMBOL(get_##name);

int is_abnormal_powerup(void)
{
	u32 pu_reason = get_powerup_reason();
	return (pu_reason & (RESTART_EVENT_KPANIC | RESTART_EVENT_WDOG)) |
		(pu_reason & BIT(PU_REASON_EVENT_HWRST) & RESTART_EVENT_OTHER);
}

static ssize_t powerup_reason_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
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
		reset_reason_index = find_first_bit((unsigned long *)&reset_reason,
			sizeof(reset_reason)*BITS_PER_BYTE);
		if (reset_reason_index < RS_REASON_MAX && reset_reason_index >= 0) {
			s += snprintf(s, MAX_PU_RS_REASON_HWVER_LEN, "%s", reset_reasons[reset_reason_index]);
			printk(KERN_DEBUG "%s: rs_reason [0x%x], first non-zero bit"
				" %d\n", __func__, reset_reason, reset_reason_index);
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
		s += snprintf(s, MAX_PU_RS_REASON_HWVER_LEN, "%s", powerup_reasons[pu_reason_index]);
		printk(KERN_DEBUG "%s: pu_reason [0x%x] index %d\n",
			__func__, pu_reason, pu_reason_index);
		goto out;
	}
	s += snprintf(s, MAX_PU_RS_REASON_HWVER_LEN, "unknown reboot");
out:
	return s - buf;
}

static ssize_t powerup_reason_details_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	u32 pu_reason;

	pu_reason = get_powerup_reason();

	return snprintf(buf, MAX_PU_RS_REASON_HWVER_LEN, "0x%x\n", pu_reason);
}

static ssize_t hw_version_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	u32 hw_version;

	hw_version = get_hw_version();

	return snprintf(buf, MAX_PU_RS_REASON_HWVER_LEN, "0x%x\n", hw_version);
}

bootinfo_attr(powerup_reason);
bootinfo_attr(powerup_reason_details);
bootinfo_attr(hw_version);
bootinfo_func_init(u32, powerup_reason, 0);
bootinfo_func_init(u32, hw_version, 0);

unsigned int get_hw_version_devid(void)
{
	return (get_hw_version() & HW_DEVID_VERSION_MASK) >> HW_DEVID_VERSION_SHIFT;
}
EXPORT_SYMBOL(get_hw_version_devid);

unsigned int get_hw_version_major(void)
{
	return (get_hw_version() & HW_MAJOR_VERSION_MASK) >> HW_MAJOR_VERSION_SHIFT;
}
EXPORT_SYMBOL(get_hw_version_major);

unsigned int get_hw_version_minor(void)
{
	return (get_hw_version() & HW_MINOR_VERSION_MASK) >> HW_MINOR_VERSION_SHIFT;
}

static struct attribute *g[] = {
	&powerup_reason_attr.attr,
	&powerup_reason_details_attr.attr,
	&hw_version_attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = g,
};

static int __init bootinfo_init(void)
{
	int ret = -ENOMEM;

	bootinfo_kobj = kobject_create_and_add("bootinfo", NULL);
	if (bootinfo_kobj == NULL) {
		printk("bootinfo_init: subsystem_register failed\n");
		goto fail;
	}

	ret = sysfs_create_group(bootinfo_kobj, &attr_group);
	if (ret) {
		printk("bootinfo_init: subsystem_register failed\n");
		goto sys_fail;
	}

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

