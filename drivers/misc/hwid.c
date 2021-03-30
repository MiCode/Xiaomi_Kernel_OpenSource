// SPDX-License-Identifier: GPL-2.0
/*
 * HwId Module driver for mi dirver acquire some hwid build info,
 * which is only used for xiaomi corporation internally.
 *
 * Copyright (c) 2020 xiaomi inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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
#include <linux/hwid.h>

/*****************************************************************************
* Global variable or extern global variabls
*****************************************************************************/
static uint hwid_value;
module_param(hwid_value, uint, 0444);
MODULE_PARM_DESC(hwid_value, "xiaomi hwid value correspondingly different build");

static uint project;
module_param(project, uint, 0444);
MODULE_PARM_DESC(project, "xiaomi project serial num predefine");

static uint build_adc;
module_param(build_adc, uint, 0444);
MODULE_PARM_DESC(build_adc, "xiaomi adc value of build resistance");

static uint project_adc;
module_param(project_adc, uint, 0444);
MODULE_PARM_DESC(project_adc, "xiaomi adc value of project resistance");

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
static ssize_t hwid_project_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%x\n", project);
}

static ssize_t hwid_value_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%x\n", hwid_value);
}

static ssize_t hwid_project_adc_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", project_adc);
}

static ssize_t hwid_build_adc_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", build_adc);
}

const char *product_name_get(void)
{
	switch (project){
		case HARDWARE_PROJECT_J18: return "cetus";
		case HARDWARE_PROJECT_K1:  return "star";
		case HARDWARE_PROJECT_K2:  return "venus";
		case HARDWARE_PROJECT_K1A: return "mars";
		case HARDWARE_PROJECT_K9:  return "renoir";
		case HARDWARE_PROJECT_K11:
			if ( (uint32_t)CountryIndia == get_hw_country_version())
				return "haydnin";
			else
				return "haydn";
		default: return "unknown";
	}
}
EXPORT_SYMBOL(product_name_get);

uint32_t get_hw_project_adc(void)
{
	return project_adc;
}
EXPORT_SYMBOL(get_hw_project_adc);

uint32_t get_hw_build_adc(void)
{
	return build_adc;
}
EXPORT_SYMBOL(get_hw_build_adc);

uint32_t get_hw_version_platform(void)
{
	return project;
}
EXPORT_SYMBOL(get_hw_version_platform);


uint32_t get_hw_id_value(void)
{
	return hwid_value;
}
EXPORT_SYMBOL(get_hw_id_value);

uint32_t get_hw_country_version(void)
{
	return (hwid_value & HW_COUNTRY_VERSION_MASK) >> HW_COUNTRY_VERSION_SHIFT;
}
EXPORT_SYMBOL(get_hw_country_version);

uint32_t get_hw_version_major(void)
{
	return (hwid_value & HW_MAJOR_VERSION_MASK) >> HW_MAJOR_VERSION_SHIFT;
}
EXPORT_SYMBOL(get_hw_version_major);

uint32_t get_hw_version_minor(void)
{
	return (hwid_value & HW_MINOR_VERSION_MASK) >> HW_MINOR_VERSION_SHIFT;
}
EXPORT_SYMBOL(get_hw_version_minor);

uint32_t get_hw_version_build(void)
{
	return (hwid_value & HW_BUILD_VERSION_MASK) >> HW_BUILD_VERSION_SHIFT;
}
EXPORT_SYMBOL(get_hw_version_build);

hwid_attr(hwid_project);
hwid_attr(hwid_value);
hwid_attr(hwid_project_adc);
hwid_attr(hwid_build_adc);

static struct attribute *hwid_attrs[] = {
	&hwid_project_attr.attr,
	&hwid_value_attr.attr,
	&hwid_project_adc_attr.attr,
	&hwid_build_adc_attr.attr,
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
