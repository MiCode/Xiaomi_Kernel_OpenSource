// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2020 The Linux Foundation. All rights reserved. */

/* Neuron channel bus type driver
 *
 * This driver creates a channel bus type device and registers a channel driver.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <linux/version.h>
#include <linux/neuron.h>

static int channel_match(struct device *dev, struct device_driver *driver)
{
	struct neuron_channel *channel = to_neuron_channel(dev);
	struct neuron_channel_driver *channel_drv =
					to_neuron_channel_driver(driver);

	/* The channel's type must match the driver's. */
	if (channel->type != channel_drv->type)
		return 0;

	/* The driver must be able to drive the channel in the required
	 * direction(s).
	 */
	if ((channel->direction & channel_drv->direction) !=
			channel->direction)
		return 0;

	if (of_driver_match_device(dev, driver))
		return 1;

	return 0;
}

static int channel_probe(struct device *dev)
{
	struct neuron_channel *channel_dev = to_neuron_channel(dev);
	struct neuron_channel_driver *channel_drv =
					to_neuron_channel_driver(dev->driver);

	if (channel_drv->probe)
		return channel_drv->probe(channel_dev);
	else if (dev->driver->probe)
		return dev->driver->probe(dev);

	return 0;
}

static int channel_remove(struct device *dev)
{
	struct neuron_channel *channel_dev = to_neuron_channel(dev);
	struct neuron_channel_driver *channel_drv =
					to_neuron_channel_driver(dev->driver);

	if (channel_drv->remove)
		channel_drv->remove(channel_dev);
	else if (dev->driver->remove)
		dev->driver->remove(dev);

	return 0;
}

static struct bus_type channel_bus_type = {
	.name	= "neuron_channel",
	.match	= channel_match,
	.probe	= channel_probe,
	.remove = channel_remove,
};

static void channel_dev_release(struct device *dev)
{
	struct neuron_channel *channel_dev = to_neuron_channel(dev);

	put_device(&channel_dev->protocol->dev);
	kfree(channel_dev);
}

struct neuron_channel *neuron_channel_add(struct device_node *node,
					  struct device *parent)
{
	struct neuron_channel *channel_dev;
	const char *str;
	int reg = 0;
	int err;

	channel_dev = kzalloc(sizeof(*channel_dev), GFP_KERNEL);
	if (!channel_dev)
		return ERR_PTR(-ENOMEM);
	device_initialize(&channel_dev->dev);

	channel_dev->dev.of_node = node;
	channel_dev->dev.bus = &channel_bus_type;
	channel_dev->dev.parent = parent;
	channel_dev->dev.release = channel_dev_release;

	err = of_property_read_u32(node, "reg", &reg);
	if (err < 0) {
		dev_err(parent, "channel %s has no reg property\n",
			node->full_name);
		goto fail_properties;
	}
	channel_dev->id = reg;

	err = of_property_read_string(node, "direction", &str);
	if (err < 0) {
		dev_err(parent, "channel %d: channel direction is undefined\n",
			reg);
		goto fail_properties;
	}

	err = -EINVAL;
	if (!strcmp(str, "send")) {
		channel_dev->direction = NEURON_CHANNEL_SEND;
	} else if (!strcmp(str, "receive")) {
		channel_dev->direction = NEURON_CHANNEL_RECEIVE;
	} else if (!strcmp(str, "both")) {
		channel_dev->direction = NEURON_CHANNEL_BIDIRECTIONAL;
	} else {
		dev_err(parent, "channel %d: bad channel direction \"%s\"\n",
			reg, str);
		goto fail_properties;
	}

	err = of_property_read_string(node, "class", &str);
	if (err < 0) {
		dev_err(parent, "channel %d: channel type is undefined\n",
			reg);
		goto fail_properties;
	}

	err = -EINVAL;
	if (!strcmp(str, "message-queue")) {
		u64 max_size = 0;
		u32 queue_length = 0;

		channel_dev->type = NEURON_CHANNEL_MESSAGE_QUEUE;

		/* If these reads fail, we let the driver decide */
		of_property_read_u64(node, "max-size", &max_size);
		channel_dev->max_size = (size_t)max_size;
		of_property_read_u32(node, "queue-length", &queue_length);
		channel_dev->queue_length = (unsigned int)queue_length;
	} else if (!strcmp(str, "notification")) {
		channel_dev->type = NEURON_CHANNEL_NOTIFICATION;
	} else if (!strcmp(str, "shared-memory")) {
		channel_dev->type = NEURON_CHANNEL_SHARED_MEMORY;
	} else {
		dev_err(parent, "channel %d: unknown channel type \"%s\"\n",
			reg, str);
		goto fail_properties;
	}

	dev_set_name(&channel_dev->dev, "%s:%s%d", dev_name(parent),
		     node->name, reg);

	err =  device_add(&channel_dev->dev);
	if (err)
		goto fail_device_add;

	return channel_dev;

fail_device_add:
fail_properties:
	put_device(&channel_dev->dev);
	kfree(channel_dev);
	return ERR_PTR(err);
}
EXPORT_SYMBOL(neuron_channel_add);

int neuron_register_channel_driver(struct neuron_channel_driver *drv)
{
	int ret;

	drv->driver.bus = &channel_bus_type;
	ret = driver_register(&drv->driver);
	if (ret)
		return ret;

	return 0;
}
EXPORT_SYMBOL(neuron_register_channel_driver);

void neuron_unregister_channel_driver(struct neuron_channel_driver *drv)
{
	driver_unregister(&drv->driver);
}
EXPORT_SYMBOL(neuron_unregister_channel_driver);

static int __init channel_bus_init(void)
{
	int ret;

	ret = bus_register(&channel_bus_type);
	if (ret < 0) {
		pr_err("Unable to register bus\n");
		return ret;
	}

	return 0;
}

static void channel_bus_exit(void)
{
	bus_unregister(&channel_bus_type);
}

subsys_initcall(channel_bus_init);
module_exit(channel_bus_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Neuron channel bus module");
