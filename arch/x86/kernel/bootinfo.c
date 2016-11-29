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

static char *bootinfo;
static struct kobject *bootinfo_kobj;

#define bootinfo_attr(_name) \
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = __stringify(_name),	\
		.mode = 0644,			\
	},					\
	.show	= _name##_show,			\
	.store	= NULL,				\
}

ssize_t powerup_reason_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", bootinfo);
}

bootinfo_attr(powerup_reason);

static struct attribute *g[] = {
	&powerup_reason_attr.attr,
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

static int __init bootinfo_setup(char *str)
{
	bootinfo = str;
	return 1;
}

__setup("androidboot.bootreason=", bootinfo_setup);

module_init(bootinfo_init);
