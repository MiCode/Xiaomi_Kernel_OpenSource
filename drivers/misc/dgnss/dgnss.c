/*
 * dgnss.c: Intel interface for gps devices
 *
 * (C) Copyright 2013 Intel Corporation
 * Author: Venkat Raghavulu
 *		 Dinesh Sharma
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/acpi.h>
#include <linux/pm.h>

#define DRIVER_NAME "dgnss"

#ifdef CONFIG_ACPI
#define ACPI_DEVICE_ID_BCM4752 "BCM4752"
#endif

static int dgnss_enable_status;

/*********************************************************************
 *		Driver sysfs attribute functions
 *********************************************************************/

static ssize_t dgnss_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	long int enable;
	int ret = 0;

	ret = kstrtol(buf, 10, &enable);
	if (ret < 0) {
		dev_err(dev, "Failed to convert buffer to a valid value\n");
		return ret;
	}

	/* This driver uses _PSx routines to control power because the GPIO
		PIN and its state may vary by platform and/or board.*/
#ifdef CONFIG_ACPI
	if (enable)
		ret = acpi_evaluate_object(ACPI_HANDLE(dev),
			"_PS0", NULL, NULL);
	else
		ret = acpi_evaluate_object(ACPI_HANDLE(dev),
			"_PS3", NULL, NULL);

	if (ACPI_FAILURE(ret))
		dev_err(dev, "Failed to %s the dGNSS device\n",
				(enable ? "Enable":"Disable"));
	else
		dgnss_enable_status = enable;
#endif

	return size;
}


static DEVICE_ATTR(enable, S_IRUGO|S_IWUSR, NULL, dgnss_enable_store);

static struct attribute *dgnss_attrs[] = {
	&dev_attr_enable.attr,
	NULL,
};

static struct attribute_group dgnss_attr_group = {
	.name = DRIVER_NAME,
	.attrs = dgnss_attrs,
};

/*********************************************************************
 *		Driver GPIO probe/remove functions
 *********************************************************************/

static int dgnss_runtime_suspend(struct device *dev)
{
	int ret = 0;

#ifdef CONFIG_ACPI
	if (dgnss_enable_status) {
		ret = acpi_evaluate_object(ACPI_HANDLE(dev),
				"_PS3", NULL, NULL);

		if (ACPI_FAILURE(ret))
			dev_err(dev, "Failed to suspend dgnss device\n");
		else
			dgnss_enable_status = !dgnss_enable_status;
	}
#endif

	return ret;
}

static int dgnss_runtime_resume(struct device *dev)
{
	int ret = 0;

#ifdef CONFIG_ACPI
	if (!dgnss_enable_status) {
		ret = acpi_evaluate_object(ACPI_HANDLE(dev),
				"_PS0", NULL, NULL);

		if (ACPI_FAILURE(ret))
			dev_err(dev, "Failed to resume dgnss device\n");
		else
			dgnss_enable_status = !dgnss_enable_status;
	}
#endif

	return ret;
}

static int dgnss_runtime_idle(struct device *dev)
{
	return 0;
}

static int dgnss_probe(struct platform_device *pdev)
{
	int ret = sysfs_create_group(&pdev->dev.kobj, &dgnss_attr_group);

	if (ret)
		dev_err(&pdev->dev, "Failed to create dgnss sysfs interface\n");

	return ret;
}

static int dgnss_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &dgnss_attr_group);

	return 0;
}

static void dgnss_shutdown(struct platform_device *pdev)
{
}

/*********************************************************************
 *		Driver initialisation and finalization
 *********************************************************************/

#ifdef CONFIG_ACPI
static const struct acpi_device_id acpi_gps_id_table[] = {
	/* ACPI IDs here */
	{ACPI_DEVICE_ID_BCM4752},
	{ }
};
MODULE_DEVICE_TABLE(acpi, acpi_gps_id_table);
#endif

static const struct dev_pm_ops dgnss_pm_ops = {
	.runtime_suspend = dgnss_runtime_suspend,
	.runtime_resume = &dgnss_runtime_resume,
	.runtime_idle = &dgnss_runtime_idle,

};


static struct platform_driver dgnss_driver = {
	.probe		= dgnss_probe,
	.remove		= dgnss_remove,
	.driver		= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_ACPI
		.acpi_match_table = ACPI_PTR(acpi_gps_id_table),
#endif
				.pm = &dgnss_pm_ops,
	},
	.shutdown	= dgnss_shutdown,
};

static int __init dgnss_driver_init(void)
{
	return platform_driver_register(&dgnss_driver);
};


static void __exit dgnss_driver_exit(void)
{
	platform_driver_unregister(&dgnss_driver);
}

module_init(dgnss_driver_init);
module_exit(dgnss_driver_exit);

MODULE_AUTHOR("Dinesh Sharma");
MODULE_DESCRIPTION("DGNSS driver");
MODULE_LICENSE("GPL");
