/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*
 * Secure-Processor-SubSystem (SPSS) utilities.
 *
 * This driver provides utilities for the Secure Processor (SP).
 *
 * The SP daemon needs to load different SPSS images based on:
 *
 * 1. Test/Production key used to sign the SPSS image (read fuse).
 * 2. SPSS HW version (selected via Device Tree).
 *
 */

#define pr_fmt(fmt)	"spss_utils [%s]: " fmt, __func__

#include <linux/kernel.h>	/* min() */
#include <linux/module.h>	/* MODULE_LICENSE */
#include <linux/device.h>	/* class_create() */
#include <linux/slab.h>	/* kzalloc() */
#include <linux/fs.h>		/* file_operations */
#include <linux/cdev.h>		/* cdev_add() */
#include <linux/errno.h>	/* EINVAL, ETIMEDOUT */
#include <linux/printk.h>	/* pr_err() */
#include <linux/bitops.h>	/* BIT(x) */
#include <linux/platform_device.h> /* platform_driver_register() */
#include <linux/of.h>		/* of_property_count_strings() */
#include <linux/io.h>		/* ioremap_nocache() */

#include <soc/qcom/subsystem_restart.h>

/* driver name */
#define DEVICE_NAME	"spss-utils"

static bool is_test_fuse_set;
static const char *test_firmware_name;
static const char *prod_firmware_name;
static const char *firmware_name;
static struct device *spss_dev;
static u32 spss_debug_reg_addr; /* SP_SCSR_MBn_SP2CL_GPm(n,m) */

/*==========================================================================*/
/*		Device Sysfs */
/*==========================================================================*/

static ssize_t firmware_name_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int ret;

	if (!dev || !attr || !buf) {
		pr_err("invalid param.\n");
		return -EINVAL;
	}

	if (firmware_name == NULL)
		ret = snprintf(buf, PAGE_SIZE, "%s\n", "unknown");
	else
		ret = snprintf(buf, PAGE_SIZE, "%s\n", firmware_name);

	return ret;
}

static ssize_t firmware_name_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t size)
{
	pr_err("set firmware name is not allowed.\n");

	return -EINVAL;
}

static DEVICE_ATTR(firmware_name, 0444,
		firmware_name_show, firmware_name_store);

static ssize_t test_fuse_state_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int ret;

	if (!dev || !attr || !buf) {
		pr_err("invalid param.\n");
		return -EINVAL;
	}

	if (is_test_fuse_set)
		ret = snprintf(buf, PAGE_SIZE, "%s", "test");
	else
		ret = snprintf(buf, PAGE_SIZE, "%s", "prod");

	return ret;
}

static ssize_t test_fuse_state_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t size)
{
	pr_err("set test fuse state is not allowed.\n");

	return -EINVAL;
}

static DEVICE_ATTR(test_fuse_state, 0444,
		test_fuse_state_show, test_fuse_state_store);

static ssize_t spss_debug_reg_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int ret;
	void __iomem *spss_debug_reg = NULL;
	int val1, val2;

	if (!dev || !attr || !buf) {
		pr_err("invalid param.\n");
		return -EINVAL;
	}

	pr_debug("spss_debug_reg_addr [0x%x].\n", spss_debug_reg_addr);

	spss_debug_reg = ioremap_nocache(spss_debug_reg_addr, 0x16);

	if (!spss_debug_reg) {
		pr_err("can't map debug reg addr.\n");
		return -EFAULT;
	}

	val1 = readl_relaxed(spss_debug_reg);
	val2 = readl_relaxed(((char *) spss_debug_reg) + 0x04);

	ret = snprintf(buf, PAGE_SIZE, "val1 [0x%x] val2 [0x%x]", val1, val2);

	iounmap(spss_debug_reg);

	return ret;
}

static ssize_t spss_debug_reg_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t size)
{
	pr_err("set debug reg is not allowed.\n");

	return -EINVAL;
}

static DEVICE_ATTR(spss_debug_reg, 0444,
		spss_debug_reg_show, spss_debug_reg_store);

static int spss_create_sysfs(struct device *dev)
{
	int ret;

	ret = device_create_file(dev, &dev_attr_firmware_name);
	if (ret < 0) {
		pr_err("failed to create sysfs file for firmware_name.\n");
		return ret;
	}

	ret = device_create_file(dev, &dev_attr_test_fuse_state);
	if (ret < 0) {
		pr_err("failed to create sysfs file for test_fuse_state.\n");
		return ret;
	}

	ret = device_create_file(dev, &dev_attr_spss_debug_reg);
	if (ret < 0) {
		pr_err("failed to create sysfs file for spss_debug_reg.\n");
		return ret;
	}

	return 0;
}

/*==========================================================================*/
/*		Device Tree */
/*==========================================================================*/

/**
 * spss_parse_dt() - Parse Device Tree info.
 */
static int spss_parse_dt(struct device_node *node)
{
	int ret;
	u32 spss_fuse_addr = 0;
	u32 spss_fuse_bit = 0;
	u32 spss_fuse_mask = 0;
	void __iomem *spss_fuse_reg = NULL;
	u32 val = 0;

	ret = of_property_read_string(node, "qcom,spss-test-firmware-name",
		&test_firmware_name);
	if (ret < 0) {
		pr_err("can't get test fw name.\n");
		return -EFAULT;
	}

	ret = of_property_read_string(node, "qcom,spss-prod-firmware-name",
		&prod_firmware_name);
	if (ret < 0) {
		pr_err("can't get prod fw name.\n");
		return -EFAULT;
	}

	ret = of_property_read_u32(node, "qcom,spss-fuse-addr",
		&spss_fuse_addr);
	if (ret < 0) {
		pr_err("can't get fuse addr.\n");
		return -EFAULT;
	}

	ret = of_property_read_u32(node, "qcom,spss-fuse-bit",
		&spss_fuse_bit);
	if (ret < 0) {
		pr_err("can't get fuse bit.\n");
		return -EFAULT;
	}

	spss_fuse_mask = BIT(spss_fuse_bit);

	pr_debug("spss_fuse_addr [0x%x] , spss_fuse_bit [%d] .\n",
		(int) spss_fuse_addr, (int) spss_fuse_bit);

	spss_fuse_reg = ioremap_nocache(spss_fuse_addr, sizeof(u32));

	if (!spss_fuse_reg) {
		pr_err("can't map fuse addr.\n");
		return -EFAULT;
	}

	val = readl_relaxed(spss_fuse_reg);

	pr_debug("spss fuse register value [0x%x].\n", (int) val);

	if (val & spss_fuse_mask)
		is_test_fuse_set = true;

	iounmap(spss_fuse_reg);

	ret = of_property_read_u32(node, "qcom,spss-debug-reg-addr",
		&spss_debug_reg_addr);
	if (ret < 0) {
		pr_err("can't get debug regs addr.\n");
		return ret;
	}

	return 0;
}

/**
 * spss_probe() - initialization sequence
 */
static int spss_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *np = NULL;
	struct device *dev = NULL;

	if (!pdev) {
		pr_err("invalid pdev.\n");
		return -ENODEV;
	}

	np = pdev->dev.of_node;
	if (!np) {
		pr_err("invalid DT node.\n");
		return -EINVAL;
	}

	dev = &pdev->dev;
	spss_dev = dev;

	if (dev == NULL) {
		pr_err("invalid dev.\n");
		return -EINVAL;
	}

	platform_set_drvdata(pdev, dev);

	ret = spss_parse_dt(np);
	if (ret < 0) {
		pr_err("fail to parse device tree.\n");
		return -EFAULT;
	}

	if (is_test_fuse_set)
		firmware_name = test_firmware_name;
	else
		firmware_name = prod_firmware_name;

	ret = subsystem_set_fwname("spss", firmware_name);
	if (ret < 0) {
		pr_err("fail to set fw name.\n");
		return -EFAULT;
	}

	spss_create_sysfs(dev);

	pr_info("Initialization completed ok, firmware_name [%s].\n",
		firmware_name);

	return 0;
}

static const struct of_device_id spss_match_table[] = {
	{ .compatible = "qcom,spss-utils", },
	{ },
};

static struct platform_driver spss_driver = {
	.probe = spss_probe,
	.driver = {
		.name = DEVICE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(spss_match_table),
	},
};

/*==========================================================================*/
/*		Driver Init/Exit					*/
/*==========================================================================*/
static int __init spss_init(void)
{
	int ret = 0;

	pr_info("spss-utils driver Ver 1.1 18-Sep-2016.\n");

	ret = platform_driver_register(&spss_driver);
	if (ret)
		pr_err("register platform driver failed, ret [%d]\n", ret);

	return 0;
}
late_initcall(spss_init); /* start after PIL driver */

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Secure Processor Utilities");
