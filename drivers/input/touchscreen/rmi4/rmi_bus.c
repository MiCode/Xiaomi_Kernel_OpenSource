/*
 * Copyright (c) 2011, 2012 Synaptics Incorporated
 * Copyright (c) 2011 Unixphere
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/kconfig.h>
#include <linux/list.h>
#include <linux/pm.h>
#include <linux/rmi.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/debugfs.h>
#include "rmi_driver.h"

DEFINE_MUTEX(rmi_bus_mutex);

static struct attribute *function_dev_attrs[] = {
	NULL,
};

static struct attribute_group function_dev_attr_group = {
	.attrs = function_dev_attrs,
};

static const struct attribute_group *function_dev_attr_groups[] = {
	&function_dev_attr_group,
	NULL,
};

struct device_type rmi_function_type = {
	.name = "rmi_function",
	.groups = function_dev_attr_groups,
};
EXPORT_SYMBOL_GPL(rmi_function_type);

static struct attribute *sensor_dev_attrs[] = {
	NULL,
};
static struct attribute_group sensor_dev_attr_group = {
	.attrs = sensor_dev_attrs,
};

static const struct attribute_group *sensor_dev_attr_groups[] = {
	&sensor_dev_attr_group,
	NULL,
};

struct device_type rmi_sensor_type = {
	.name = "rmi_sensor",
	.groups = sensor_dev_attr_groups,
};
EXPORT_SYMBOL_GPL(rmi_sensor_type);

static atomic_t physical_device_count = ATOMIC_INIT(0);

#ifdef CONFIG_RMI4_DEBUG
static struct dentry *rmi_debugfs_root;
#endif

#ifdef CONFIG_PM
static int rmi_bus_suspend(struct device *dev)
{
	struct device_driver *driver = dev->driver;
	const struct dev_pm_ops *pm;

	if (!driver)
		return 0;

	pm = driver->pm;
	if (pm && pm->suspend)
		return pm->suspend(dev);
	if (driver->suspend)
		return driver->suspend(dev, PMSG_SUSPEND);

	return 0;
}

static int rmi_bus_resume(struct device *dev)
{
	struct device_driver *driver = dev->driver;
	const struct dev_pm_ops *pm;

	if (!driver)
		return 0;

	pm = driver->pm;
	if (pm && pm->resume)
		return pm->resume(dev);
	if (driver->resume)
		return driver->resume(dev);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(rmi_bus_pm_ops,
			 rmi_bus_suspend, rmi_bus_resume);


static void release_rmidev_device(struct device *dev)
{
	struct rmi_device *rmi_dev = to_rmi_device(dev);
	kfree(rmi_dev);
}

/**
 * rmi_register_phys_device - register a physical device connection on the RMI
 * bus.  Physical drivers provide communication from the devices on the bus to
 * the RMI4 sensor on a bus such as SPI, I2C, and so on.
 *
 * @phys: the physical device to register
 */
int rmi_register_phys_device(struct rmi_phys_device *phys)
{
	struct rmi_device_platform_data *pdata = phys->dev->platform_data;
	struct rmi_device *rmi_dev;

	if (!pdata) {
		dev_err(phys->dev, "no platform data!\n");
		return -EINVAL;
	}

	rmi_dev = kzalloc(sizeof(struct rmi_device), GFP_KERNEL);
	if (!rmi_dev)
		return -ENOMEM;

	rmi_dev->phys = phys;
	rmi_dev->dev.bus = &rmi_bus_type;
	rmi_dev->dev.type = &rmi_sensor_type;

	rmi_dev->number = atomic_inc_return(&physical_device_count) - 1;
	rmi_dev->dev.release = release_rmidev_device;

	dev_set_name(&rmi_dev->dev, "sensor%02d", rmi_dev->number);
	dev_dbg(phys->dev, "%s: Registered %s as %s.\n", __func__,
		pdata->sensor_name, dev_name(&rmi_dev->dev));

#ifdef	CONFIG_RMI4_DEBUG
	if (rmi_debugfs_root) {
		rmi_dev->debugfs_root = debugfs_create_dir(
			dev_name(&rmi_dev->dev), rmi_debugfs_root);
		if (!rmi_dev->debugfs_root)
			dev_err(&rmi_dev->dev, "Failed to create debugfs root.\n");
	}
#endif

	phys->rmi_dev = rmi_dev;
	return device_register(&rmi_dev->dev);
}
EXPORT_SYMBOL_GPL(rmi_register_phys_device);

/**
 * rmi_unregister_phys_device - unregister a physical device connection
 * @phys: the physical driver to unregister
 *
 */
void rmi_unregister_phys_device(struct rmi_phys_device *phys)
{
	struct rmi_device *rmi_dev = phys->rmi_dev;

#ifdef	CONFIG_RMI4_DEBUG
	if (rmi_dev->debugfs_root)
		debugfs_remove(rmi_dev->debugfs_root);
#endif

	device_unregister(&rmi_dev->dev);
}
EXPORT_SYMBOL_GPL(rmi_unregister_phys_device);

static int rmi_bus_match(struct device *dev, struct device_driver *drv)
{
	struct rmi_function_driver *fn_drv;
	struct rmi_function_dev *fn;

	/*
	 * This seems a little broken to me.  It  means a system can only ever
	 * have one kind of sensor driver.  It'll work for now, but I think in
	 * the long run we need to revisit this.
	 */
	if (dev->type == &rmi_sensor_type && drv == &rmi_sensor_driver.driver)
		return 1;

	if (dev->type != &rmi_function_type)
	return 0;

	fn = to_rmi_function_dev(dev);
	fn_drv = to_rmi_function_driver(drv);

	return fn->fd.function_number == fn_drv->func;
}

static int rmi_function_probe(struct device *dev)
{
	struct rmi_function_driver *fn_drv;
	struct rmi_function_dev *fn = to_rmi_function_dev(dev);

	fn_drv = to_rmi_function_driver(dev->driver);

	if (fn_drv->probe)
		return fn_drv->probe(fn);

	return 0;
}

static int rmi_function_remove(struct device *dev)
{
	struct rmi_function_driver *fn_drv;
	struct rmi_function_dev *fn = to_rmi_function_dev(dev);

	fn_drv = to_rmi_function_driver(dev->driver);

	if (fn_drv->remove)
		return fn_drv->remove(fn);

	return 0;
}

static int rmi_sensor_remove(struct device *dev)
{
	struct rmi_driver *driver;
	struct rmi_device *rmi_dev = to_rmi_device(dev);

	driver = to_rmi_driver(dev->driver);

	if (!driver->remove)
		return driver->remove(rmi_dev);
	return 0;
}

static int rmi_bus_remove(struct device *dev)
{
	if (dev->type == &rmi_function_type)
		return rmi_function_remove(dev);
	else if (dev->type == &rmi_sensor_type)
		return rmi_sensor_remove(dev);
	return -EINVAL;
}

struct bus_type rmi_bus_type = {
	.name		= "rmi",
	.match		= rmi_bus_match,
	.remove		= rmi_bus_remove,
	.pm		= &rmi_bus_pm_ops,
};
EXPORT_SYMBOL_GPL(rmi_bus_type);

/**
 * rmi_register_function_driver - register a driver for an RMI function
 * @fn_drv: RMI driver that should be registered.
 * @module: pointer to module that implements the driver
 * @mod_name: name of the module implementing the driver
 *
 * This function performs additional setup of RMI function driver and
 * registers it with the RMI core so that it can be bound to
 * RMI function devices.
 */
int __rmi_register_function_driver(struct rmi_function_driver *fn_drv,
				     struct module *owner,
				     const char *mod_name)
{
	int error;

	fn_drv->driver.bus = &rmi_bus_type;
	fn_drv->driver.owner = owner;
	if (!fn_drv->driver.probe)
		fn_drv->driver.probe = rmi_function_probe;
	fn_drv->driver.mod_name = mod_name;

	error = driver_register(&fn_drv->driver);
	if (error) {
		pr_err("driver_register() failed for %s, error: %d\n",
			fn_drv->driver.name, error);
		return error;
		}

	return 0;
}
EXPORT_SYMBOL_GPL(__rmi_register_function_driver);

/**
 * rmi_unregister_function_driver - unregister given RMI function driver
 * @fn_drv: RMI driver that should be unregistered.
 *
 * This function unregisters given function driver from RMI core which
 * causes it to be unbound from the function devices.
 */
void rmi_unregister_function_driver(struct rmi_function_driver *fn_drv)
{
	driver_unregister(&fn_drv->driver);
}
EXPORT_SYMBOL_GPL(rmi_unregister_function_driver);

/**
 * rmi_for_each_dev - provides a way for other parts of the system to enumerate
 * the devices on the RMI bus.
 *
 * @data - will be passed into the callback function.
 * @func - will be called for each device.
 */
int rmi_for_each_dev(void *data, int (*func)(struct device *dev, void *data))
{
	int retval;
	mutex_lock(&rmi_bus_mutex);
	retval = bus_for_each_dev(&rmi_bus_type, NULL, data, func);
	mutex_unlock(&rmi_bus_mutex);
	return retval;
}
EXPORT_SYMBOL_GPL(rmi_for_each_dev);

static int __init rmi_bus_init(void)
{
	int error;

	mutex_init(&rmi_bus_mutex);

	error = bus_register(&rmi_bus_type);
	if (error) {
		pr_err("%s: error registering the RMI bus: %d\n",
			__func__, error);
		return error;
	}

#ifdef CONFIG_RMI4_DEBUG
	rmi_debugfs_root = debugfs_create_dir(rmi_bus_type.name, NULL);
	if (!rmi_debugfs_root)
		pr_err("%s: Failed to create debugfs root.\n",
			__func__);
	else if (IS_ERR(rmi_debugfs_root)) {
		pr_err("%s: Kernel may not contain debugfs support, code=%ld\n",
			__func__, PTR_ERR(rmi_debugfs_root));
		rmi_debugfs_root = NULL;
	}
#endif

	error = rmi_register_function_driver(&rmi_f01_driver);
	if (error) {
		pr_err("%s: error registering the RMI F01 driver: %d\n",
			__func__, error);
		goto err_unregister_bus;
	}

	error = rmi_register_sensor_driver();
	if (error) {
		pr_err("%s: error registering the RMI sensor driver: %d\n",
			__func__, error);
		goto err_unregister_f01;
	}

	return 0;

err_unregister_f01:
	rmi_unregister_function_driver(&rmi_f01_driver);
err_unregister_bus:
	bus_unregister(&rmi_bus_type);
	return error;
}

static void __exit rmi_bus_exit(void)
{
	/*
	 * We should only ever get here if all drivers are unloaded, so
	 * all we have to do at this point is unregister ourselves.
	 */
#ifdef CONFIG_RMI4_DEBUG
	if (rmi_debugfs_root)
		debugfs_remove(rmi_debugfs_root);
#endif
	rmi_unregister_sensor_driver();
	rmi_unregister_function_driver(&rmi_f01_driver);
	bus_unregister(&rmi_bus_type);
}

module_init(rmi_bus_init);
module_exit(rmi_bus_exit);

MODULE_AUTHOR("Christopher Heiny <cheiny@synaptics.com");
MODULE_DESCRIPTION("RMI bus");
MODULE_LICENSE("GPL");
MODULE_VERSION(RMI_DRIVER_VERSION);
