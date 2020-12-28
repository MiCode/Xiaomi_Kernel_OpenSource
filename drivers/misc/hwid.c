// SPDX-License-Identifier: GPL-2.0
/*
 * Module driver for mi project type, build & project adc values read,
 * which is only used for xiaomi corporation internally.
 *
 * Copyright (c) 2020 xiaomi inc.
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
#include <soc/qcom/socinfo.h>
#include <linux/soc/qcom/smem.h>
#include <linux/hwid.h>

/*****************************************************************************
* Global variable or extern global variabls
*****************************************************************************/
static struct class *cls;
static struct device *adcdev;
static int major;
static struct project_info *pinfo;

/*****************************************************************************
* Global variable or extern global functions
*****************************************************************************/
const char *product_name_get(void)
{
	return pinfo != NULL ? pinfo->productname : "dummy";
}
EXPORT_SYMBOL(product_name_get);

uint32_t get_hw_version_platform(void)
{
	return pinfo != NULL ? pinfo->project : 0;
}
EXPORT_SYMBOL(get_hw_version_platform);

uint32_t get_hw_country_version(void)
{
	return pinfo != NULL ? (pinfo->hw_id & HW_COUNTRY_VERSION_MASK) >> HW_COUNTRY_VERSION_SHIFT : 0;
}
EXPORT_SYMBOL(get_hw_country_version);

uint32_t get_hw_version_major(void)
{
	return pinfo != NULL ? (pinfo->hw_id & HW_MAJOR_VERSION_MASK) >> HW_MAJOR_VERSION_SHIFT : 0;
}
EXPORT_SYMBOL(get_hw_version_major);

uint32_t get_hw_version_minor(void)
{
	return pinfo != NULL ? (pinfo->hw_id & HW_MINOR_VERSION_MASK) >> HW_MINOR_VERSION_SHIFT : 0;
}
EXPORT_SYMBOL(get_hw_version_minor);

uint32_t get_hw_version_build(void)
{
	return pinfo != NULL ? (pinfo->hw_id & HW_BUILD_VERSION_MASK) >> HW_BUILD_VERSION_SHIFT : 0;
}
EXPORT_SYMBOL(get_hw_version_build);

uint32_t get_hw_project_adc(void)
{
	return pinfo != NULL ? pinfo->pro_r1 : 0;
}
EXPORT_SYMBOL(get_hw_project_adc);

uint32_t get_hw_build_adc(void)
{
	return pinfo != NULL ? pinfo->hw_r1 : 0;
}
EXPORT_SYMBOL(get_hw_build_adc);

static ssize_t xiaomi_adc_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint32_t project_adc = get_hw_project_adc();
	uint32_t build_adc = get_hw_build_adc();
	if (!project_adc || !build_adc) {
		pr_err("fail to get adc pairs from socinfo\n");
		return -ENODATA;
	}

	return snprintf(buf, PAGE_SIZE, "project_adc:%u build_adc:%u\n", project_adc, build_adc);
}

static DEVICE_ATTR(adc_pairs, 0444, xiaomi_adc_show, NULL);

struct file_operations adc_ops = {
	.owner  = THIS_MODULE,
};

/*****************************************************************************
*  Name: xiaomi_adc_init
*****************************************************************************/
static int __init xiaomi_adc_init(void)
{
	int ret = 0;
	size_t size;

	pinfo = (struct project_info*) qcom_smem_get(QCOM_SMEM_HOST_ANY, SMEM_ID_VENDOR1, &size);
	if (PTR_ERR(pinfo) == -EPROBE_DEFER) {
		pinfo = NULL;
		return -EPROBE_DEFER;
		pr_err("SMEM is not initialized.\n");
	}

	if (IS_ERR(pinfo) || !size) {
		pr_err("hwid moudle install failed\n");
		ret = PTR_ERR(pinfo);
		goto hwid_unreg;
	}

	cls = class_create(THIS_MODULE, XIAOMI_ADC_CLASS);
	if (IS_ERR(cls)) {
		pr_err("Failed to create xiaomi adc class!\n");
		return PTR_ERR(cls);
	}

	major = register_chrdev(ADCDEV_MAJOR, XIAOMI_ADC_MODULE, &adc_ops);
	if (major < 0) {
		pr_err("Failed to register xiaomi adc char device!\n");
		ret = major;
		goto class_unreg;
	}

	adcdev = device_create(cls, 0, MKDEV(major,ADCDEV_MINOR), NULL, XIAOMI_ADC_DEVICE);
	if (IS_ERR(adcdev)) {
		pr_err("Failed to create xiaomi adcdev!\n");
		ret = -ENODEV;
		goto chrdev_unreg;
	}

	ret = sysfs_create_file(&(adcdev->kobj), &dev_attr_adc_pairs.attr);
	if (ret) {
		pr_err("Failed to export adc pairs to sysfs!\n");
		goto adcdev_unreg;
	}

	return ret;

adcdev_unreg:
	device_destroy(cls, MKDEV(major, ADCDEV_MINOR));
chrdev_unreg:
	unregister_chrdev(ADCDEV_MAJOR, XIAOMI_ADC_MODULE);
class_unreg:
	class_destroy(cls);
hwid_unreg:
	return ret;
}

/*****************************************************************************
*  Name: xiaomi_adc_exit
*****************************************************************************/
static void __exit xiaomi_adc_exit(void)
{
	sysfs_remove_file(&(adcdev->kobj), &dev_attr_adc_pairs.attr);
	device_destroy(cls, MKDEV(major,ADCDEV_MINOR));
	unregister_chrdev(ADCDEV_MAJOR, XIAOMI_ADC_MODULE);
	class_destroy(cls);
}

subsys_initcall(xiaomi_adc_init);
module_exit(xiaomi_adc_exit);

MODULE_AUTHOR("weixiaotian1@xiaomi.com");
MODULE_DESCRIPTION("Hwid Module Driver for Xiaomi Corporation");
MODULE_LICENSE("GPL v2");
