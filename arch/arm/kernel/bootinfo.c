/*
 * bootinfo.c
 *
 * Copyright (C) 2016 XiaoMi, Inc.
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
#include <linux/mfd/palmas.h>
#include <linux/of.h>

static const char * const powerup_reasons[PU_REASON_MAX] = {
	[PU_REASON_EVENT_KEYPAD]	= "keypad",
	[PU_REASON_EVENT_RTC]		= "rtc",
	[PU_REASON_EVENT_USB_CHG]	= "usb_chg",
	[PU_REASON_EVENT_LPK]		= "long_power_key",
	[PU_REASON_EVENT_PMUWTD]	= "pmu watchdog",
	[PU_REASON_EVENT_WDOG]		= "watchdog",
	[PU_REASON_EVENT_KPANIC]	= "kpanic",
	[PU_REASON_EVENT_NORMAL]	= "reboot",
	[PU_REASON_EVENT_OTHER]		= "other",
	[PU_REASON_EVENT_UNKNOWN1]	= "unknown_swrst",
	[PU_REASON_EVENT_UNKNOWN2]	= "unknown_pmupor",
	[PU_REASON_EVENT_UNKNOWN3]	= "unknown_aprst",
};

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
	.store	= _name##_store,		\
}

#define bootinfo_func_init(type, name, initval)   \
	    static type name = (initval);       \
	    type get_##name(void)               \
	    {                                   \
			return name;                    \
	    }                                   \
		void set_##name(type __##name)      \
	    {                                   \
			name = __##name;                \
	    }                                   \
	    EXPORT_SYMBOL(set_##name);          \
	    EXPORT_SYMBOL(get_##name);

int is_abnormal_powerup(void)
{
	u32 pu_reason = get_powerup_reason();
	return pu_reason & (PWR_ON_EVENT_KPANIC | PWR_ON_EVENT_WDOG | PWR_ON_EVENT_PMUWTD
			| PWR_ON_EVENT_OTHER | PWR_ON_EVENT_UNKNOWN1 | PWR_ON_EVENT_UNKNOWN2 | PWR_ON_EVENT_UNKNOWN3);
}

static ssize_t powerup_reason_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	char *s = buf;
	u32 pu_reason;
	int pu_reason_index = PU_REASON_MAX;

	pu_reason = get_powerup_reason();
	pu_reason_index = find_first_bit((unsigned long *)&pu_reason,
		sizeof(pu_reason)*BITS_PER_BYTE);

	if (pu_reason_index < PU_REASON_MAX && pu_reason_index >= 0) {
		s += sprintf(s, "%s", powerup_reasons[pu_reason_index]);
		printk(KERN_DEBUG "%s: pu_reason [0x%x], first non-zero bit"
			" %d\n", __func__, pu_reason, pu_reason_index);
	} else {
		s += sprintf(s, "%s", "unknown");
		printk(KERN_DEBUG "%s: pu_reason [0x%x], unknown reason",
			__func__, pu_reason);
	}

	return s - buf;
}

static ssize_t powerup_reason_details_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	u32 pu_reason;

	pu_reason = get_powerup_reason();

	return sprintf(buf, "0x%x\n", pu_reason);
}

static ssize_t powerup_reason_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t n)
{
	return n;
}

static ssize_t powerup_reason_details_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t n)
{
	return n;
}

bootinfo_attr(powerup_reason);
bootinfo_attr(powerup_reason_details);
bootinfo_func_init(u32, powerup_reason, 0);
bootinfo_func_init(u32, hw_version, 0);

static struct attribute *g[] = {
	&powerup_reason_attr.attr,
	&powerup_reason_details_attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = g,
};

static int __init setup_hwversion(char *str)
{
	hw_version = simple_strtoul(str, NULL, 0);
	return 1;
}
__setup("androidboot.hwversion=", setup_hwversion);


static int __init bootinfo_init(void)
{
	struct device_node *reset_info;
	u32 prop_val;
	int err;
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

#ifdef CONFIG_OF
	reset_info = of_find_node_by_path("/chosen/reset_info");
	if (!IS_ERR_OR_NULL(reset_info)) {
		err = of_property_read_u32(reset_info, "pu_reason", &prop_val);
		if (err)
			pr_err("failed to read /chosen/reset_info/pu_reason\n");
		else {
			set_powerup_reason(prop_val);
			pr_info("%s: powerup reason=0x%x\n", __FUNCTION__, get_powerup_reason());
		}
	} else {
		pr_info("get reset_info from dt failed\n");
	}
#endif

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

