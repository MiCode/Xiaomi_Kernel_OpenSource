// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2020 The Linux Foundation. All rights reserved. */

/* Neuron application bus type driver
 *
 * This driver creates an application bus type device and registers application
 * driver.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <linux/version.h>
#include <linux/neuron.h>

static int app_match(struct device *dev, struct device_driver *driver)
{
	if (of_driver_match_device(dev, driver))
		return 1;

	return 0;
}

static int app_probe(struct device *dev)
{
	int ret;
	struct neuron_application *app_dev = to_neuron_application(dev);
	struct neuron_app_driver *app_drv = to_neuron_app_driver(dev->driver);

	ret = 0;
	if (app_drv->probe)
		ret = app_drv->probe(app_dev);
	else if (dev->driver->probe)
		ret = dev->driver->probe(dev);

	return ret;
}

static int app_remove(struct device *dev)
{
	struct neuron_application *app_dev = to_neuron_application(dev);
	struct neuron_app_driver *app_drv = to_neuron_app_driver(dev->driver);

	if (app_drv->remove)
		app_drv->remove(app_dev);
	else if (dev->driver->remove)
		dev->driver->remove(dev);

	return 0;
}

static struct bus_type app_bus_type = {
	.name	= "neuron_application",
	.match	= app_match,
	.probe	= app_probe,
	.remove = app_remove,
};

static void app_dev_release(struct device *dev)
{
	struct neuron_application *app_dev = to_neuron_application(dev);

	put_device(&app_dev->protocol->dev);
	kfree(app_dev);
}

struct neuron_application *neuron_app_add(struct device_node *node,
					  struct device *parent)
{
	struct neuron_application *app_dev;
	int err;

	app_dev = kzalloc(sizeof(*app_dev), GFP_KERNEL);
	if (!app_dev)
		return ERR_PTR(-ENOMEM);
	device_initialize(&app_dev->dev);

	app_dev->dev.of_node = node;
	app_dev->dev.bus = &app_bus_type;
	app_dev->dev.parent = parent;
	app_dev->dev.release = app_dev_release;
	dev_set_name(&app_dev->dev, "%s:%s", dev_name(parent), node->name);

	err = device_add(&app_dev->dev);
	if (err)
		goto fail_device_add;

	return app_dev;

fail_device_add:
	put_device(&app_dev->dev);
	return ERR_PTR(err);
}
EXPORT_SYMBOL(neuron_app_add);

int neuron_register_app_driver(struct neuron_app_driver *drv)
{
	int ret;

	drv->driver.bus = &app_bus_type;
	ret = driver_register(&drv->driver);
	if (ret)
		return ret;

	return 0;
}
EXPORT_SYMBOL(neuron_register_app_driver);

void neuron_unregister_app_driver(struct neuron_app_driver *drv)
{
	driver_unregister(&drv->driver);
}
EXPORT_SYMBOL(neuron_unregister_app_driver);

static int __init app_bus_init(void)
{
	int ret;

	ret = bus_register(&app_bus_type);
	if (ret < 0) {
		pr_err("Unable to register bus\n");
		return ret;
	}

	return 0;
}

static void app_bus_exit(void)
{
	bus_unregister(&app_bus_type);
}

subsys_initcall(app_bus_init);
module_exit(app_bus_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Neuron application bus module");
