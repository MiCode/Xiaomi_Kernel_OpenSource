// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2020 The Linux Foundation. All rights reserved. */

/* Neuron protocol bus type driver
 *
 * This driver creates a protocol bus type device and registers a protocol
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

static int protocol_match(struct device *dev, struct device_driver *driver)
{
	struct neuron_protocol *protocol_dev = to_neuron_protocol(dev);
	struct neuron_protocol_driver *protocol_drv =
					to_neuron_protocol_driver(driver);
	int i;

	/* The list of channel types and directions must exactly match those
	 * expected by the driver.
	 */
	if (protocol_dev->channel_count != protocol_drv->channel_count)
		return 0;

	for (i = 0; i < protocol_drv->channel_count; i++) {
		if (protocol_dev->channels[i]->type !=
				protocol_drv->channels[i].type)
			return 0;

		if (protocol_dev->channels[i]->direction !=
				protocol_drv->channels[i].direction)
			return 0;
	}

	/* The list of processes must exactly match those implemented by the
	 * driver. Typically this will just be "client" or "server", but some
	 * protocols might have multiple processes.
	 */
	if (protocol_dev->process_count != protocol_drv->process_count)
		return 0;
	for (i = 0; i < protocol_dev->process_count; i++)
		if (strcmp(protocol_dev->processes[i],
			   protocol_drv->processes[i]))
			return 0;

	/* The protocol driver requires the application to be bound by a
	 * specific application driver, but that may not have happened yet so
	 * we can't check it here. It's checked in probe instead.
	 *
	 * It should not be possible to bind the wrong app driver anyway,
	 * unless the device tree has incorrect compatible strings. That is
	 * unlikely enough that we can live with the added overhead of not
	 * failing until probe.
	 */

	if (of_driver_match_device(dev, driver))
		return 1;

	return 0;
}

static int protocol_probe(struct device *dev)
{
	int i, ret;
	struct neuron_protocol *protocol_dev = to_neuron_protocol(dev);
	struct neuron_protocol_driver *protocol_drv =
		to_neuron_protocol_driver(dev->driver);
	struct neuron_app_driver *app_drv =
		to_neuron_app_driver(protocol_dev->application->dev.driver);

	/* Fail if the application driver is not the expected one. */
	if (protocol_drv != app_drv->protocol_driver)
		return -EINVAL;

	ret = 0;
	if (protocol_drv->probe)
		ret = protocol_drv->probe(protocol_dev);
	else if (dev->driver->probe)
		ret = dev->driver->probe(dev);

	if (ret < 0)
		return ret;

	for (i = 0; i < protocol_dev->channel_count; i++)
		rcu_assign_pointer(protocol_dev->channels[i]->protocol_drv,
				   protocol_drv);

	if (ret >= 0)
		rcu_assign_pointer(protocol_dev->application->protocol_drv,
				   protocol_drv);

	if (app_drv->start)
		app_drv->start(protocol_dev->application);

	return ret;
}

static int protocol_remove(struct device *dev)
{
	int i;
	struct neuron_protocol *protocol_dev = to_neuron_protocol(dev);
	struct neuron_protocol_driver *protocol_drv =
					to_neuron_protocol_driver(dev->driver);

	/* Disconnect the channels and application, and wait for them to
	 * notice
	 */
	for (i = 0; i < protocol_dev->channel_count; i++)
		rcu_assign_pointer(protocol_dev->channels[i]->protocol_drv,
				   NULL);
	rcu_assign_pointer(protocol_dev->application->protocol_drv, NULL);

	synchronize_rcu();

	if (protocol_drv->remove)
		protocol_drv->remove(protocol_dev);
	else if (dev->driver->remove)
		dev->driver->remove(dev);

	return 0;
}

static struct bus_type protocol_bus_type = {
	.name	= "neuron_protocol",
	.match	= protocol_match,
	.probe	= protocol_probe,
	.remove = protocol_remove,
};

static void protocol_dev_release(struct device *dev)
{
	int i;
	struct neuron_protocol *protocol_dev = to_neuron_protocol(dev);

	for (i = 0; i < protocol_dev->channel_count; i++)
		put_device(&protocol_dev->channels[i]->dev);

	kfree(protocol_dev->processes);
	kfree(protocol_dev);
}

struct neuron_protocol *neuron_protocol_add(struct device_node *node,
					    unsigned int channel_count,
					    struct neuron_channel **channels,
					    struct device *parent,
					    struct neuron_application *app_dev)
{
	struct neuron_protocol *protocol_dev;
	const char *process;
	struct property *prop;
	int err;
	int i;

	protocol_dev = kzalloc(sizeof(*protocol_dev) +
			(sizeof(struct neuron_channel *) * channel_count),
			GFP_KERNEL);
	if (!protocol_dev)
		return ERR_PTR(-ENOMEM);
	device_initialize(&protocol_dev->dev);

	protocol_dev->dev.of_node = node;
	protocol_dev->dev.bus = &protocol_bus_type;
	protocol_dev->dev.parent = parent;
	protocol_dev->dev.release = protocol_dev_release;
	dev_set_name(&protocol_dev->dev, "%s:%s", dev_name(parent),
		     node->name);

	/* Save process names */
	err = of_property_count_strings(node, "processes");
	if (err < 0)
		goto fail_properties;
	protocol_dev->process_count = err;
	protocol_dev->processes = kcalloc(protocol_dev->process_count,
					  sizeof(const char *),
					  GFP_KERNEL);
	if (!protocol_dev->processes)
		goto fail_properties;
	i = 0;
	of_property_for_each_string(node, "processes", prop, process) {
		protocol_dev->processes[i] = process;
		i++;
	}

	/* Save channel pointers */
	protocol_dev->channel_count = 0;
	for (i = 0; i < channel_count; i++) {
		protocol_dev->channels[i] = &(*channels[i]);
		get_device(&(*channels[i]).dev);
		protocol_dev->channel_count++;
	}

	/* Save application pointer in protocol */
	protocol_dev->application = app_dev;
	get_device(&app_dev->dev);

	if (!device_link_add(&protocol_dev->dev, &app_dev->dev, 0)) {
		err = -ENODEV;
		goto fail_link;
	}

	err = device_add(&protocol_dev->dev);
	if (err)
		goto fail_device_add;

	return protocol_dev;

fail_device_add:
fail_link:
fail_properties:
	put_device(&protocol_dev->dev);
	return ERR_PTR(err);
}
EXPORT_SYMBOL(neuron_protocol_add);

int neuron_register_protocol_driver(struct neuron_protocol_driver *drv)
{
	int ret;

	drv->driver.bus = &protocol_bus_type;
	ret = driver_register(&drv->driver);
	if (ret)
		return ret;

	return 0;
}
EXPORT_SYMBOL(neuron_register_protocol_driver);

void neuron_unregister_protocol_driver(struct neuron_protocol_driver *drv)
{
	driver_unregister(&drv->driver);
}
EXPORT_SYMBOL(neuron_unregister_protocol_driver);

static int __init protocol_bus_init(void)
{
	int ret;

	ret = bus_register(&protocol_bus_type);
	if (ret < 0) {
		pr_err("Unable to register bus\n");
		return ret;
	}

	return 0;
}

static void protocol_bus_exit(void)
{
	bus_unregister(&protocol_bus_type);
}

subsys_initcall(protocol_bus_init);
module_exit(protocol_bus_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Neuron protocol bus module");
