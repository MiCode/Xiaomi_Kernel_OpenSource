/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
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

#define pr_fmt(fmt) "AXI: %s(): " fmt, __func__

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/radix-tree.h>
#include <linux/clk.h>
#include <mach/msm_bus_board.h>
#include <mach/msm_bus.h>
#include "msm_bus_core.h"

static atomic_t num_fab = ATOMIC_INIT(0);

int msm_bus_get_num_fab(void)
{
	return atomic_read(&num_fab);
}

int msm_bus_device_match(struct device *dev, void* id)
{
	struct msm_bus_fabric_device *fabdev = to_msm_bus_fabric_device(dev);

	if (!fabdev) {
		MSM_BUS_WARN("Fabric %p returning 0\n", fabdev);
		return 0;
	}
	return (fabdev->id == (int)id);
}

struct bus_type msm_bus_type = {
	.name      = "msm-bus-type",
};
EXPORT_SYMBOL(msm_bus_type);

/**
 * msm_bus_get_fabric_device() - This function is used to search for
 * the fabric device on the bus
 * @fabid: Fabric id
 * Function returns: Pointer to the fabric device
 */
struct msm_bus_fabric_device *msm_bus_get_fabric_device(int fabid)
{
	struct device *dev;
	struct msm_bus_fabric_device *fabric;
	dev = bus_find_device(&msm_bus_type, NULL, (void *)fabid,
		msm_bus_device_match);
	fabric = to_msm_bus_fabric_device(dev);
	return fabric;
}

/**
 * msm_bus_fabric_device_register() - Registers a fabric on msm bus
 * @fabdev: Fabric device to be registered
 */
int msm_bus_fabric_device_register(struct msm_bus_fabric_device *fabdev)
{
	int ret = 0;
	fabdev->dev.bus = &msm_bus_type;
	ret = dev_set_name(&fabdev->dev, fabdev->name);
	if (ret) {
		MSM_BUS_ERR("error setting dev name\n");
		goto err;
	}
	ret = device_register(&fabdev->dev);
	if (ret < 0) {
		MSM_BUS_ERR("error registering device%d %s\n",
				ret, fabdev->name);
		goto err;
	}
	atomic_inc(&num_fab);
err:
	return ret;
}

/**
 * msm_bus_fabric_device_unregister() - Unregisters the fabric
 * devices from the msm bus
 */
void msm_bus_fabric_device_unregister(struct msm_bus_fabric_device *fabdev)
{
	device_unregister(&fabdev->dev);
	atomic_dec(&num_fab);
}

static void __exit msm_bus_exit(void)
{
	bus_unregister(&msm_bus_type);
}

static int __init msm_bus_init(void)
{
	int retval = 0;
	retval = bus_register(&msm_bus_type);
	if (retval)
		MSM_BUS_ERR("bus_register error! %d\n",
			retval);
	return retval;
}
postcore_initcall(msm_bus_init);
module_exit(msm_bus_exit);
MODULE_LICENSE("GPL v2");
MODULE_VERSION("0.2");
MODULE_ALIAS("platform:msm_bus");
