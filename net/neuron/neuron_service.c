// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2020 The Linux Foundation. All rights reserved. */

/* Neuron service driver
 *
 * This driver parses DT description of a Neuron service and creates protocol,
 * application, and channel devices based on the DT nodes.
 */
#include <linux/init.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/of.h>
#include <linux/slab.h>

#include "neuron_service.h"

#define DRIVER_NAME "neuron-service"
#define DEVICE_NAME "neuron-service"

#ifndef CONFIG_OF
#error "neuron service driver only supported on device tree kernels"
#endif

static const struct of_device_id neuron_service_match[] = {
	{
		.compatible = "qcom,neuron-service",
	},
	{},
};
MODULE_DEVICE_TABLE(of, neuron_service_match);

static int neuron_service_probe(struct platform_device *pdev)
{
	struct neuron_service *neuron_serv;
	static struct device_node *node;
	static struct device_node *pnode;
	int count = 0;
	int err;
	int i = 0;

	node = NULL;
	pnode = pdev->dev.of_node;

	for_each_child_of_node(pnode, node)
		if (node->name && (of_node_cmp(node->name, "channel") == 0))
			count++;

	if (!count) {
		err = -ENODEV;
		goto fail_find_channels;
	}

	neuron_serv = kzalloc(sizeof(*neuron_serv) +
			sizeof(struct neuron_channel *) * count,
			GFP_KERNEL);
	if (!neuron_serv) {
		err = -ENOMEM;
		goto fail_alloc_neuron_serv;
	}

	neuron_serv->channel_count = count;
	node = NULL;

	for_each_child_of_node(pnode, node) {
		if (node->name && (of_node_cmp(node->name, "channel") == 0)) {
			neuron_serv->channels[i] =
				neuron_channel_add(node, &pdev->dev);
			if (IS_ERR(neuron_serv->channels[i])) {
				err = PTR_ERR(neuron_serv->channels[i]);
				goto fail_add_channel;
			}
			neuron_serv->channels[i]->id = i;
			i++;
		}
	}

	node = of_get_child_by_name(pnode, "application");
	if (!node) {
		err = -ENODEV;
		goto fail_find_application;
	}

	neuron_serv->application =
		neuron_app_add(node, &pdev->dev);

	if (IS_ERR(neuron_serv->application)) {
		err = PTR_ERR(neuron_serv->application);
		goto fail_add_application;
	}

	node = of_get_child_by_name(pnode, "protocol");
	if (!node) {
		err = -ENODEV;
		goto fail_find_protocol;
	}

	neuron_serv->protocol =
		neuron_protocol_add(node, count, neuron_serv->channels,
				    &pdev->dev, neuron_serv->application);
	if (IS_ERR(neuron_serv->protocol)) {
		err = PTR_ERR(neuron_serv->protocol);
		goto fail_add_protocol;
	}

	for (i = 0; i < neuron_serv->channel_count; i++) {
		neuron_serv->channels[i]->protocol = neuron_serv->protocol;
		get_device(&neuron_serv->protocol->dev);
	}

	/* Save protocol pointer */
	neuron_serv->application->protocol = neuron_serv->protocol;
	get_device(&neuron_serv->protocol->dev);

	dev_set_drvdata(&pdev->dev, neuron_serv);

	return 0;

fail_add_application:
fail_find_application:
	device_unregister(&neuron_serv->protocol->dev);
	put_device(&neuron_serv->protocol->dev);
fail_add_protocol:
fail_find_protocol:
	for (i = 0; i < (neuron_serv->channel_count); i++) {
		device_unregister(&neuron_serv->channels[i]->dev);
		put_device(&neuron_serv->channels[i]->dev);
	}
fail_add_channel:
	dev_set_drvdata(&pdev->dev, NULL);
	kfree(neuron_serv);
fail_alloc_neuron_serv:
fail_find_channels:
	return err;
}

static int neuron_service_remove(struct platform_device *pdev)
{
	struct neuron_service *neuron_serv;
	int i;

	neuron_serv = dev_get_drvdata(&pdev->dev);

	/* Clearing all pointers to devices */
	neuron_serv->application->protocol = NULL;
	neuron_serv->protocol->application = NULL;
	for (i = 0; i < (neuron_serv->channel_count); i++) {
		neuron_serv->protocol->channels[i] = NULL;
		neuron_serv->channels[i]->protocol = NULL;
	}

	device_unregister(&neuron_serv->application->dev);
	put_device(&neuron_serv->application->dev);

	device_unregister(&neuron_serv->protocol->dev);
	put_device(&neuron_serv->protocol->dev);

	for (i = 0; i < (neuron_serv->channel_count); i++) {
		device_unregister(&neuron_serv->channels[i]->dev);
		put_device(&neuron_serv->channels[i]->dev);
	}

	dev_set_drvdata(&pdev->dev, NULL);
	kfree(neuron_serv);

	return 0;
}

static struct platform_driver neuron_service_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = neuron_service_match,
	},
	.probe	= neuron_service_probe,
	.remove = neuron_service_remove,
};

static int __init neuron_service_init(void)
{
	int ret;

	ret = platform_driver_register(&neuron_service_driver);
	if (ret < 0) {
		pr_err("Failed to register driver\n");
		return ret;
	}

	return 0;
}

static void __exit neuron_service_exit(void)
{
	platform_driver_unregister(&neuron_service_driver);
}

module_init(neuron_service_init);
module_exit(neuron_service_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Neuron service - configuration layer");
