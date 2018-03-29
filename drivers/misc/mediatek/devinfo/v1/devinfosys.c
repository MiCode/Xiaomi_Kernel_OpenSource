/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/cdev.h>
#include <linux/mm.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/ioctl.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include "devinfo.h"
#include "mt_devinfo.h"


/**************************************************************************
*  DEV DRIVER SYSFS
**************************************************************************/

static struct platform_driver dev_info = {
	.driver  = {
		.name = "dev_info",
		.bus = &platform_bus_type,
		.owner = THIS_MODULE,
	}
};

static ssize_t devinfo_show(struct device_driver *driver, char *buf)
{
	unsigned int i;
	unsigned int *output = (unsigned int *)buf;

	output[0] = devinfo_get_size();
	for (i = 0; i < output[0]; i++)
		output[i + 1] = get_devinfo_with_index(i);

	return (output[0] + 1) * sizeof(unsigned int);
}

DRIVER_ATTR(dev_info, 0444, devinfo_show, NULL);

static int __init devinfo_init(void)
{
	int ret = 0;

	/* register driver and create sysfs files */
	ret = driver_register(&dev_info.driver);
	if (ret) {
		pr_warn("fail to register devinfo driver\n");
		return -1;
	}

	ret = driver_create_file(&dev_info.driver, &driver_attr_dev_info);
	if (ret) {
		pr_warn("[BOOT INIT] Fail to create devinfo sysfs file\n");
		driver_unregister(&dev_info.driver);
		return -1;
	}

	return 0;
}
module_init(devinfo_init);

