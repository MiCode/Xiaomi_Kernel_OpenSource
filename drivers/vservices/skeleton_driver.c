/*
 * drivers/vservices/skeleton_driver.c
 *
 * Copyright (c) 2012-2018 General Dynamics
 * Copyright (c) 2014 Open Kernel Labs, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Skeleton testing driver for templating vService client/server drivers
 */

#include <linux/device.h>
#include <linux/slab.h>
#include <linux/module.h>

#include <vservices/session.h>
#include <vservices/buffer.h>
#include <vservices/service.h>

struct skeleton_info {
	unsigned dummy;
};

static void vs_skeleton_handle_start(struct vs_service_device *service)
{
	/* NOTE: Do not change this message - is it used for system testing */
	dev_info(&service->dev, "skeleton handle_start\n");
}

static int vs_skeleton_handle_message(struct vs_service_device *service,
					  struct vs_mbuf *mbuf)
{
	dev_info(&service->dev, "skeleton handle_messasge\n");
	return -EBADMSG;
}

static void vs_skeleton_handle_notify(struct vs_service_device *service,
					  u32 flags)
{
	dev_info(&service->dev, "skeleton handle_notify\n");
}

static void vs_skeleton_handle_reset(struct vs_service_device *service)
{
	dev_info(&service->dev, "skeleton handle_reset %s service %d\n",
			service->is_server ? "server" : "client", service->id);
}

static int vs_skeleton_probe(struct vs_service_device *service)
{
	struct skeleton_info *info;
	int err = -ENOMEM;

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		goto fail;

	dev_set_drvdata(&service->dev, info);
	return 0;

fail:
	return err;
}

static int vs_skeleton_remove(struct vs_service_device *service)
{
	struct skeleton_info *info = dev_get_drvdata(&service->dev);

	dev_info(&service->dev, "skeleton remove\n");
	kfree(info);
	return 0;
}

static struct vs_service_driver server_skeleton_driver = {
	.protocol	= "com.ok-labs.skeleton",
	.is_server	= true,
	.probe		= vs_skeleton_probe,
	.remove		= vs_skeleton_remove,
	.start		= vs_skeleton_handle_start,
	.receive	= vs_skeleton_handle_message,
	.notify		= vs_skeleton_handle_notify,
	.reset		= vs_skeleton_handle_reset,
	.driver		= {
		.name		= "vs-server-skeleton",
		.owner		= THIS_MODULE,
		.bus		= &vs_server_bus_type,
	},
};

static struct vs_service_driver client_skeleton_driver = {
	.protocol	= "com.ok-labs.skeleton",
	.is_server	= false,
	.probe		= vs_skeleton_probe,
	.remove		= vs_skeleton_remove,
	.start		= vs_skeleton_handle_start,
	.receive	= vs_skeleton_handle_message,
	.notify		= vs_skeleton_handle_notify,
	.reset		= vs_skeleton_handle_reset,
	.driver		= {
		.name		= "vs-client-skeleton",
		.owner		= THIS_MODULE,
		.bus		= &vs_client_bus_type,
	},
};

static int __init vs_skeleton_init(void)
{
	int ret;

	ret = driver_register(&server_skeleton_driver.driver);
	if (ret)
		return ret;

	ret = driver_register(&client_skeleton_driver.driver);
	if (ret)
		driver_unregister(&server_skeleton_driver.driver);

	return ret;
}

static void __exit vs_skeleton_exit(void)
{
	driver_unregister(&server_skeleton_driver.driver);
	driver_unregister(&client_skeleton_driver.driver);
}

module_init(vs_skeleton_init);
module_exit(vs_skeleton_exit);

MODULE_DESCRIPTION("OKL4 Virtual Services Skeleton Client/Server Driver");
MODULE_AUTHOR("Open Kernel Labs, Inc");
