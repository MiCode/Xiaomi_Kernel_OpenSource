// SPDX-License-Identifier: GPL-2.0
/*
 * HwId Module driver for mi dirver acquire some hwid build info,
 * which is only used for xiaomi corporation internally.
 *
 * Copyright (C) 2021-2022 XiaoMi, Inc.
 */

/*****************************************************************************
 * Included header files
 *****************************************************************************/
#include <linux/export.h>
#include <linux/sysfs.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/kdev_t.h>
#include <linux/printk.h>
#include <linux/errno.h>
#include <linux/err.h>
#include "hwid.h"

/*****************************************************************************
 * Global variable or extern global variabls
 *****************************************************************************/
static char *sku;
static char *country;
static char *level;
static char *version;

module_param(sku, charp, 0444);
MODULE_PARM_DESC(sku, "xiaomi sku value support");

module_param(country, charp, 0444);
MODULE_PARM_DESC(country, "xiaomi country value support");

module_param(level, charp, 0444);
MODULE_PARM_DESC(level, "xiaomi level value support");

module_param(version, charp, 0444);
MODULE_PARM_DESC(version, "xiaomi version value support");

static struct kobject *hwid_kobj;
#define hwid_attr(_name) \
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = __stringify(_name),	\
		.mode = 0444,			\
	},					\
	.show	= _name##_show,			\
	.store	= NULL,				\
}

/*****************************************************************************
 * Global variable or extern global functions
 *****************************************************************************/
static ssize_t hwid_sku_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", sku);
}

static ssize_t hwid_country_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", country);
}

static ssize_t hwid_level_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", level);
}

static ssize_t hwid_version_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", version);
}

const char *get_hw_sku(void)
{
	return sku;
}
EXPORT_SYMBOL(get_hw_sku);

const char *get_hw_country(void)
{
	return country;
}
EXPORT_SYMBOL(get_hw_country);

const char *get_hw_level(void)
{
	return level;
}
EXPORT_SYMBOL(get_hw_level);

const char *get_hw_version(void)
{
	return version;
}
EXPORT_SYMBOL(get_hw_version);

hwid_attr(hwid_sku);
hwid_attr(hwid_country);
hwid_attr(hwid_level);
hwid_attr(hwid_version);

static struct attribute *hwid_attrs[] = {
	&hwid_sku_attr.attr,
	&hwid_country_attr.attr,
	&hwid_level_attr.attr,
	&hwid_version_attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = hwid_attrs,
};

/*****************************************************************************
 *  Name: hwid_module_init
 *****************************************************************************/
static int __init hwid_module_init(void)
{
	int ret = -ENOMEM;

	hwid_kobj = kobject_create_and_add("hwid", NULL);
	if (!hwid_kobj) {
		pr_err("hwid: hwid module init failed\n");
		goto fail;
	}

	ret = sysfs_create_group(hwid_kobj, &attr_group);
	if (ret) {
		pr_err("hwid: sysfs register failed\n");
		goto sys_fail;
	}

sys_fail:
	kobject_del(hwid_kobj);
fail:
	return ret;
}

/*****************************************************************************
 *  Name: hwid_module_exit
 *****************************************************************************/
static void __exit hwid_module_exit(void)
{
	if (hwid_kobj) {
		sysfs_remove_group(hwid_kobj, &attr_group);
		kobject_del(hwid_kobj);
	}
	pr_info("hwid: hwid module exit success\n");
}

subsys_initcall(hwid_module_init);
module_exit(hwid_module_exit);

MODULE_AUTHOR("weixiaotian1@xiaomi.com");
MODULE_DESCRIPTION("Hwid Module Driver for Xiaomi Corporation");
MODULE_LICENSE("GPL v2");
