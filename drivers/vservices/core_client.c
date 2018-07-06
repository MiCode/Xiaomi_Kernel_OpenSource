/*
 * drivers/vservices/core_client.c
 *
 * Copyright (c) 2012-2018 General Dynamics
 * Copyright (c) 2014 Open Kernel Labs, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Client side core service application driver. This is responsible for:
 *
 *  - automatically connecting to the server when it becomes ready;
 *  - sending a reset command to the server if something has gone wrong; and
 *  - enumerating all the available services.
 *
 */

#include <linux/kernel.h>
#include <linux/completion.h>
#include <linux/workqueue.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/err.h>
#include <linux/module.h>

#include <vservices/types.h>
#include <vservices/transport.h>
#include <vservices/session.h>
#include <vservices/buffer.h>
#include <vservices/service.h>

#include <vservices/protocol/core/types.h>
#include <vservices/protocol/core/common.h>
#include <vservices/protocol/core/client.h>

#include "session.h"
#include "transport.h"
#include "compat.h"

struct core_client {
	struct vs_client_core_state	state;
	struct vs_service_device	*service;

	struct list_head		message_queue;
	struct mutex			message_queue_lock;
	struct work_struct		message_queue_work;
};

struct pending_reset {
	struct vs_service_device	*service;
	struct list_head		list;
};

#define to_core_client(x)	container_of(x, struct core_client, state)
#define dev_to_core_client(x)	to_core_client(dev_get_drvdata(x))

static int vs_client_core_fatal_error(struct vs_client_core_state *state)
{
	struct core_client *client = to_core_client(state);

	/* Force a transport level reset */
	dev_err(&client->service->dev," Fatal error - resetting session\n");
	return -EPROTO;
}

static struct core_client *
vs_client_session_core_client(struct vs_session_device *session)
{
	struct vs_service_device *core_service = session->core_service;

	if (!core_service)
		return NULL;

	return dev_to_core_client(&core_service->dev);
}

static ssize_t client_core_reset_service_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct vs_service_device *core_service = to_vs_service_device(dev);
	struct vs_session_device *session =
		vs_service_get_session(core_service);
	struct vs_service_device *target;
	vs_service_id_t service_id;
	unsigned long val;
	int err;

	/* Writing a valid service id to this file resets that service */
	err = kstrtoul(buf, 0, &val);
	if (err)
		return err;

	service_id = val;
	target = vs_session_get_service(session, service_id);
	if (!target)
		return -ENODEV;

	err = vs_service_reset(target, core_service);

	vs_put_service(target);
	return err < 0 ? err : count;
}

static DEVICE_ATTR(reset_service, S_IWUSR, NULL,
		client_core_reset_service_store);

static struct attribute *client_core_dev_attrs[] = {
	&dev_attr_reset_service.attr,
	NULL,
};

static const struct attribute_group client_core_attr_group = {
	.attrs = client_core_dev_attrs,
};

/*
 * Protocol callbacks
 */
static int
vs_client_core_handle_service_removed(struct vs_client_core_state *state,
		u32 service_id)
{
	struct core_client *client = to_core_client(state);
	struct vs_session_device *session =
			vs_service_get_session(client->service);
	struct vs_service_device *service;
	int ret;

	service = vs_session_get_service(session, service_id);
	if (!service)
		return -EINVAL;

	ret = vs_service_handle_delete(service);
	vs_put_service(service);
	return ret;
}

static int vs_client_core_create_service(struct core_client *client,
		struct vs_session_device *session, vs_service_id_t service_id,
		struct vs_string *protocol_name_string,
		struct vs_string *service_name_string)
{
	char *protocol_name, *service_name;
	struct vs_service_device *service;
	int ret = 0;

	protocol_name = vs_string_dup(protocol_name_string, GFP_KERNEL);
	if (!protocol_name) {
		ret = -ENOMEM;
		goto out;
	}

	service_name = vs_string_dup(service_name_string, GFP_KERNEL);
	if (!service_name) {
		ret = -ENOMEM;
		goto out_free_protocol_name;
	}

	service = vs_service_register(session, client->service, service_id,
			protocol_name, service_name, NULL);
	if (IS_ERR(service)) {
		ret = PTR_ERR(service);
		goto out_free_service_name;
	}

	vs_service_start(service);

out_free_service_name:
	kfree(service_name);
out_free_protocol_name:
	kfree(protocol_name);
out:
	return ret;
}

static int
vs_client_core_handle_service_created(struct vs_client_core_state *state,
		u32 service_id, struct vs_string service_name,
		struct vs_string protocol_name, struct vs_mbuf *mbuf)
{
	struct core_client *client = to_core_client(state);
	struct vs_session_device *session =
			vs_service_get_session(client->service);
	int err;

	vs_dev_debug(VS_DEBUG_CLIENT_CORE,
			vs_service_get_session(client->service),
			&client->service->dev, "Service info for %d received\n",
			service_id);

	err = vs_client_core_create_service(client, session, service_id,
			&protocol_name, &service_name);
	if (err)
		dev_err(&session->dev,
				"Failed to create service with id %d: %d\n",
				service_id, err);

	vs_client_core_core_free_service_created(state, &service_name,
			&protocol_name, mbuf);

	return err;
}

static int
vs_client_core_send_service_reset(struct core_client *client,
		struct vs_service_device *service)
{
	return vs_client_core_core_send_service_reset(&client->state,
			service->id, GFP_KERNEL);
}

static int
vs_client_core_queue_service_reset(struct vs_session_device *session,
		struct vs_service_device *service)
{
	struct core_client *client =
		vs_client_session_core_client(session);
	struct pending_reset *msg;

	if (!client)
		return -ENODEV;

	vs_dev_debug(VS_DEBUG_SERVER, session, &session->dev,
			"Sending reset for service %d\n", service->id);

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	mutex_lock(&client->message_queue_lock);

	/* put by message_queue_work */
	msg->service = vs_get_service(service);
	list_add_tail(&msg->list, &client->message_queue);

	mutex_unlock(&client->message_queue_lock);
	queue_work(client->service->work_queue, &client->message_queue_work);

	return 0;
}

static int vs_core_client_tx_ready(struct vs_client_core_state *state)
{
	struct core_client *client = to_core_client(state);

	queue_work(client->service->work_queue, &client->message_queue_work);

	return 0;
}

static void message_queue_work(struct work_struct *work)
{
	struct core_client *client = container_of(work, struct core_client,
			message_queue_work);
	struct vs_session_device *session =
		vs_service_get_session(client->service);
	struct pending_reset *msg;
	int err;

	vs_service_state_lock(client->service);
	if (!VSERVICE_CORE_STATE_IS_CONNECTED(client->state.state.core)) {
		vs_service_state_unlock(client->service);
		return;
	}

	vs_dev_debug(VS_DEBUG_CLIENT, session, &session->dev, "tx_ready\n");

	mutex_lock(&client->message_queue_lock);
	while (!list_empty(&client->message_queue)) {
		msg = list_first_entry(&client->message_queue,
				struct pending_reset, list);

		err = vs_client_core_send_service_reset(client, msg->service);

		/* If we're out of quota there's no point continuing */
		if (err == -ENOBUFS)
			break;

		/* Any other error is fatal */
		if (err < 0) {
			dev_err(&client->service->dev,
					"Failed to send pending reset for %d (%d) - resetting session\n",
					msg->service->id, err);
			vs_service_reset_nosync(client->service);
			break;
		}

		/*
		 * The message sent successfully - remove it from the queue.
		 * The corresponding vs_get_service() was done when the pending
		 * message was enqueued.
		 */
		vs_put_service(msg->service);
		list_del(&msg->list);
		kfree(msg);
	}
	mutex_unlock(&client->message_queue_lock);
	vs_service_state_unlock(client->service);
}

static int
vs_client_core_handle_server_ready(struct vs_client_core_state *state,
		u32 service_id, u32 in_quota, u32 out_quota, u32 in_bit_offset,
		u32 in_num_bits, u32 out_bit_offset, u32 out_num_bits)
{
	struct core_client *client = to_core_client(state);
	struct vs_session_device *session;
	struct vs_service_device *service;
	int ret;

	if (service_id == 0)
		return -EPROTO;

	if (!in_quota || !out_quota)
		return -EINVAL;

	session = vs_service_get_session(client->service);
	service = vs_session_get_service(session, service_id);
	if (!service)
		return -EINVAL;

	service->send_quota = in_quota;
	service->recv_quota = out_quota;
	service->notify_send_offset = in_bit_offset;
	service->notify_send_bits = in_num_bits;
	service->notify_recv_offset = out_bit_offset;
	service->notify_recv_bits = out_num_bits;

	ret = vs_service_enable(service);
	vs_put_service(service);
	return ret;
}

static int
vs_client_core_handle_service_reset(struct vs_client_core_state *state,
		u32 service_id)
{
	struct core_client *client = to_core_client(state);
	struct vs_session_device *session;

	if (service_id == 0)
		return -EPROTO;

	session = vs_service_get_session(client->service);

	return vs_service_handle_reset(session, service_id, true);
}

static void vs_core_client_start(struct vs_client_core_state *state)
{
	struct core_client *client = to_core_client(state);
	struct vs_session_device *session =
			vs_service_get_session(client->service);

	/* FIXME - start callback should return int */
	vs_dev_debug(VS_DEBUG_CLIENT_CORE, session, &client->service->dev,
			"Core client start\n");
}

static void vs_core_client_reset(struct vs_client_core_state *state)
{
	struct core_client *client = to_core_client(state);
	struct vs_session_device *session =
		vs_service_get_session(client->service);
	struct pending_reset *msg;

	/* Flush the pending resets - we're about to delete everything */
	while (!list_empty(&client->message_queue)) {
		msg = list_first_entry(&client->message_queue,
				struct pending_reset, list);
		vs_put_service(msg->service);
		list_del(&msg->list);
		kfree(msg);
	}

	vs_session_delete_noncore(session);

	/* Return to the initial quotas, until the next startup message */
	client->service->send_quota = 0;
	client->service->recv_quota = 1;
}

static int vs_core_client_startup(struct vs_client_core_state *state,
		u32 core_in_quota, u32 core_out_quota)
{
	struct core_client *client = to_core_client(state);
	struct vs_service_device *service = state->service;
	struct vs_session_device *session = vs_service_get_session(service);
	int ret;

	if (!core_in_quota || !core_out_quota)
		return -EINVAL;

	/*
	 * Update the service struct with our real quotas and tell the
	 * transport about the change
	 */

	service->send_quota = core_in_quota;
	service->recv_quota = core_out_quota;
	ret = session->transport->vt->service_start(session->transport, service);
	if (ret < 0)
		return ret;

	WARN_ON(!list_empty(&client->message_queue));

	return vs_client_core_core_req_connect(state, GFP_KERNEL);
}

static struct vs_client_core_state *
vs_core_client_alloc(struct vs_service_device *service)
{
	struct core_client *client;
	int err;

	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client)
		goto fail;

	client->service = service;
	INIT_LIST_HEAD(&client->message_queue);
	INIT_WORK(&client->message_queue_work, message_queue_work);
	mutex_init(&client->message_queue_lock);

	err = sysfs_create_group(&service->dev.kobj, &client_core_attr_group);
	if (err)
		goto fail_free_client;

	/*
	 * Default transport resources for the core service client. The
	 * server will inform us of the real quotas in the startup message.
	 * Note that it is important that the quotas never decrease, so these
	 * numbers are as small as possible.
	 */
	service->send_quota = 0;
	service->recv_quota = 1;
	service->notify_send_bits = 0;
	service->notify_send_offset = 0;
	service->notify_recv_bits = 0;
	service->notify_recv_offset = 0;

	return &client->state;

fail_free_client:
	kfree(client);
fail:
	return NULL;
}

static void vs_core_client_release(struct vs_client_core_state *state)
{
	struct core_client *client = to_core_client(state);

	sysfs_remove_group(&client->service->dev.kobj, &client_core_attr_group);
	kfree(client);
}

static struct vs_client_core vs_core_client_driver = {
	.alloc		= vs_core_client_alloc,
	.release	= vs_core_client_release,
	.start		= vs_core_client_start,
	.reset		= vs_core_client_reset,
	.tx_ready	= vs_core_client_tx_ready,

	.core = {
		.nack_connect		= vs_client_core_fatal_error,

		/* FIXME: Jira ticket SDK-3074 - ryanm. */
		.ack_disconnect		= vs_client_core_fatal_error,
		.nack_disconnect	= vs_client_core_fatal_error,

		.msg_service_created	= vs_client_core_handle_service_created,
		.msg_service_removed	= vs_client_core_handle_service_removed,

		.msg_startup		= vs_core_client_startup,
		/* FIXME: Jira ticket SDK-3074 - philipd. */
		.msg_shutdown		= vs_client_core_fatal_error,
		.msg_server_ready	= vs_client_core_handle_server_ready,
		.msg_service_reset	= vs_client_core_handle_service_reset,
	},
};

/*
 * Client bus driver
 */
static int vs_client_bus_match(struct device *dev, struct device_driver *driver)
{
	struct vs_service_device *service = to_vs_service_device(dev);
	struct vs_service_driver *vsdrv = to_vs_service_driver(driver);

	/* Don't match anything to the devio driver; it's bound manually */
	if (!vsdrv->protocol)
		return 0;

	WARN_ON_ONCE(service->is_server || vsdrv->is_server);

	/* Match if the protocol strings are the same */
	if (strcmp(service->protocol, vsdrv->protocol) == 0)
		return 1;

	return 0;
}

static ssize_t is_server_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct vs_service_device *service = to_vs_service_device(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", service->is_server);
}

static ssize_t id_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct vs_service_device *service = to_vs_service_device(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", service->id);
}

static ssize_t dev_protocol_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct vs_service_device *service = to_vs_service_device(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n", service->protocol ?: "");
}

static ssize_t service_name_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct vs_service_device *service = to_vs_service_device(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n", service->name);
}

static ssize_t quota_in_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct vs_service_device *service = to_vs_service_device(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", service->send_quota);
}

static ssize_t quota_out_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct vs_service_device *service = to_vs_service_device(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", service->recv_quota);
}

static struct device_attribute vs_client_dev_attrs[] = {
	__ATTR_RO(id),
	__ATTR_RO(is_server),
	__ATTR(protocol, S_IRUGO, dev_protocol_show, NULL),
	__ATTR_RO(service_name),
	__ATTR_RO(quota_in),
	__ATTR_RO(quota_out),
	__ATTR_NULL
};

static ssize_t protocol_show(struct device_driver *drv, char *buf)
{
	struct vs_service_driver *driver = to_vs_service_driver(drv);

	return scnprintf(buf, PAGE_SIZE, "%s\n", driver->protocol);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 0)
static struct driver_attribute vs_client_drv_attrs[] = {
	__ATTR_RO(protocol),
	__ATTR_NULL
};
#else
static DRIVER_ATTR_RO(protocol);

static struct attribute *vs_client_drv_attrs[] = {
	&driver_attr_protocol.attr,
	NULL,
};
ATTRIBUTE_GROUPS(vs_client_drv);
#endif

struct bus_type vs_client_bus_type = {
	.name		= "vservices-client",
	.dev_attrs	= vs_client_dev_attrs,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 0)
	.drv_attrs	= vs_client_drv_attrs,
#else
	.drv_groups	= vs_client_drv_groups,
#endif
	.match		= vs_client_bus_match,
	.probe		= vs_service_bus_probe,
	.remove		= vs_service_bus_remove,
	.uevent		= vs_service_bus_uevent,
};
EXPORT_SYMBOL(vs_client_bus_type);

/*
 * Client session driver
 */
static int vs_client_session_probe(struct device *dev)
{
	struct vs_session_device *session = to_vs_session_device(dev);
	struct vs_service_device *service;
	char *protocol, *name;
	int ret = 0;

	if (session->is_server) {
		ret = -ENODEV;
		goto fail;
	}

	/* create a service for the core protocol client */
	protocol = kstrdup(VSERVICE_CORE_PROTOCOL_NAME, GFP_KERNEL);
	if (!protocol) {
		ret = -ENOMEM;
		goto fail;
	}

	name = kstrdup("core", GFP_KERNEL);
	if (!name) {
		ret = -ENOMEM;
		goto fail_free_protocol;
	}

	service = vs_service_register(session, NULL, 0, protocol, name, NULL);
	if (IS_ERR(service)) {
		ret = PTR_ERR(service);
		goto fail_free_name;
	}

fail_free_name:
	kfree(name);
fail_free_protocol:
	kfree(protocol);
fail:
	return ret;
}

static int
vs_client_session_send_service_reset(struct vs_session_device *session,
		struct vs_service_device *service)
{
	if (WARN_ON(service->id == 0))
		return -EINVAL;

	return vs_client_core_queue_service_reset(session, service);
}

static struct vs_session_driver vs_client_session_driver = {
	.driver	= {
		.name			= "vservices-client-session",
		.owner			= THIS_MODULE,
		.bus			= &vs_session_bus_type,
		.probe			= vs_client_session_probe,
		.suppress_bind_attrs	= true,
	},
	.is_server		= false,
	.service_bus		= &vs_client_bus_type,
	.service_local_reset	= vs_client_session_send_service_reset,
};

static int __init vs_core_client_init(void)
{
	int ret;

	ret = bus_register(&vs_client_bus_type);
	if (ret)
		goto fail_bus_register;

#ifdef CONFIG_VSERVICES_CHAR_DEV
	vs_devio_client_driver.driver.bus = &vs_client_bus_type;
	vs_devio_client_driver.driver.owner = THIS_MODULE;
	ret = driver_register(&vs_devio_client_driver.driver);
	if (ret)
		goto fail_devio_register;
#endif

	ret = driver_register(&vs_client_session_driver.driver);
	if (ret)
		goto fail_driver_register;

	ret = vservice_core_client_register(&vs_core_client_driver,
			"vs_core_client");
	if (ret)
		goto fail_core_register;

	vservices_client_root = kobject_create_and_add("client-sessions",
			vservices_root);
	if (!vservices_client_root) {
		ret = -ENOMEM;
		goto fail_create_root;
	}

	return 0;

fail_create_root:
	vservice_core_client_unregister(&vs_core_client_driver);
fail_core_register:
	driver_unregister(&vs_client_session_driver.driver);
fail_driver_register:
#ifdef CONFIG_VSERVICES_CHAR_DEV
	driver_unregister(&vs_devio_client_driver.driver);
	vs_devio_client_driver.driver.bus = NULL;
	vs_devio_client_driver.driver.owner = NULL;
fail_devio_register:
#endif
	bus_unregister(&vs_client_bus_type);
fail_bus_register:
	return ret;
}

static void __exit vs_core_client_exit(void)
{
	kobject_put(vservices_client_root);
	vservice_core_client_unregister(&vs_core_client_driver);
	driver_unregister(&vs_client_session_driver.driver);
#ifdef CONFIG_VSERVICES_CHAR_DEV
	driver_unregister(&vs_devio_client_driver.driver);
	vs_devio_client_driver.driver.bus = NULL;
	vs_devio_client_driver.driver.owner = NULL;
#endif
	bus_unregister(&vs_client_bus_type);
}

subsys_initcall(vs_core_client_init);
module_exit(vs_core_client_exit);

MODULE_DESCRIPTION("OKL4 Virtual Services Core Client Driver");
MODULE_AUTHOR("Open Kernel Labs, Inc");
