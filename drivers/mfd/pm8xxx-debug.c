/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/mfd/pm8xxx/core.h>
#include <linux/debugfs.h>

#define PM8XXX_DEBUG_DEV_NAME "pm8xxx-debug"

struct pm8xxx_debug_device {
	struct mutex		debug_mutex;
	struct device		*parent;
	struct dentry		*dir;
	int			addr;
};

static bool pm8xxx_debug_addr_is_valid(int addr)
{
	if (addr < 0 || addr > 0x3FF) {
		pr_err("PMIC register address is invalid: %d\n", addr);
		return false;
	}
	return true;
}

static int pm8xxx_debug_data_set(void *data, u64 val)
{
	struct pm8xxx_debug_device *debugdev = data;
	u8 reg = val;
	int rc;

	mutex_lock(&debugdev->debug_mutex);

	if (pm8xxx_debug_addr_is_valid(debugdev->addr)) {
		rc = pm8xxx_writeb(debugdev->parent, debugdev->addr, reg);

		if (rc)
			pr_err("pm8xxx_writeb(0x%03X)=0x%02X failed: rc=%d\n",
				debugdev->addr, reg, rc);
	}

	mutex_unlock(&debugdev->debug_mutex);
	return 0;
}

static int pm8xxx_debug_data_get(void *data, u64 *val)
{
	struct pm8xxx_debug_device *debugdev = data;
	int rc;
	u8 reg;

	mutex_lock(&debugdev->debug_mutex);

	if (pm8xxx_debug_addr_is_valid(debugdev->addr)) {
		rc = pm8xxx_readb(debugdev->parent, debugdev->addr, &reg);

		if (rc)
			pr_err("pm8xxx_readb(0x%03X) failed: rc=%d\n",
				debugdev->addr, rc);
		else
			*val = reg;
	}

	mutex_unlock(&debugdev->debug_mutex);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_data_fops, pm8xxx_debug_data_get,
			pm8xxx_debug_data_set, "0x%02llX\n");

static int pm8xxx_debug_addr_set(void *data, u64 val)
{
	struct pm8xxx_debug_device *debugdev = data;

	if (pm8xxx_debug_addr_is_valid(val)) {
		mutex_lock(&debugdev->debug_mutex);
		debugdev->addr = val;
		mutex_unlock(&debugdev->debug_mutex);
	}

	return 0;
}

static int pm8xxx_debug_addr_get(void *data, u64 *val)
{
	struct pm8xxx_debug_device *debugdev = data;

	mutex_lock(&debugdev->debug_mutex);

	if (pm8xxx_debug_addr_is_valid(debugdev->addr))
		*val = debugdev->addr;

	mutex_unlock(&debugdev->debug_mutex);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_addr_fops, pm8xxx_debug_addr_get,
			pm8xxx_debug_addr_set, "0x%03llX\n");

static int __devinit pm8xxx_debug_probe(struct platform_device *pdev)
{
	char *name = pdev->dev.platform_data;
	struct pm8xxx_debug_device *debugdev;
	struct dentry *dir;
	struct dentry *temp;
	int rc;

	if (name == NULL) {
		pr_err("debugfs directory name must be specified in "
			"platform_data pointer\n");
		return -EINVAL;
	}

	debugdev = kzalloc(sizeof(struct pm8xxx_debug_device), GFP_KERNEL);
	if (debugdev == NULL) {
		pr_err("kzalloc failed\n");
		return -ENOMEM;
	}

	debugdev->parent = pdev->dev.parent;
	debugdev->addr = -1;

	dir = debugfs_create_dir(name, NULL);
	if (dir == NULL || IS_ERR(dir)) {
		pr_err("debugfs_create_dir failed: rc=%ld\n", PTR_ERR(dir));
		rc = PTR_ERR(dir);
		goto dir_error;
	}

	temp = debugfs_create_file("addr", S_IRUSR | S_IWUSR, dir, debugdev,
				   &debug_addr_fops);
	if (temp == NULL || IS_ERR(temp)) {
		pr_err("debugfs_create_file failed: rc=%ld\n", PTR_ERR(temp));
		rc = PTR_ERR(temp);
		goto file_error;
	}

	temp = debugfs_create_file("data", S_IRUSR | S_IWUSR, dir, debugdev,
				   &debug_data_fops);
	if (temp == NULL || IS_ERR(temp)) {
		pr_err("debugfs_create_file failed: rc=%ld\n", PTR_ERR(temp));
		rc = PTR_ERR(temp);
		goto file_error;
	}

	mutex_init(&debugdev->debug_mutex);

	debugdev->dir = dir;
	platform_set_drvdata(pdev, debugdev);

	return 0;

file_error:
	debugfs_remove_recursive(dir);
dir_error:
	kfree(debugdev);

	return rc;
}

static int __devexit pm8xxx_debug_remove(struct platform_device *pdev)
{
	struct pm8xxx_debug_device *debugdev = platform_get_drvdata(pdev);

	if (debugdev) {
		debugfs_remove_recursive(debugdev->dir);
		mutex_destroy(&debugdev->debug_mutex);
		kfree(debugdev);
	}

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver pm8xxx_debug_driver = {
	.probe		= pm8xxx_debug_probe,
	.remove		= __devexit_p(pm8xxx_debug_remove),
	.driver		= {
		.name	= PM8XXX_DEBUG_DEV_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init pm8xxx_debug_init(void)
{
	return platform_driver_register(&pm8xxx_debug_driver);
}
subsys_initcall(pm8xxx_debug_init);

static void __exit pm8xxx_debug_exit(void)
{
	platform_driver_unregister(&pm8xxx_debug_driver);
}
module_exit(pm8xxx_debug_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PM8XXX Debug driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:" PM8XXX_DEBUG_DEV_NAME);
