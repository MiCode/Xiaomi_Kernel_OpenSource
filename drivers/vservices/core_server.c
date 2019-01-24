/*
 * drivers/vservices/core_server.c
 *
 * Copyright (c) 2012-2018 General Dynamics
 * Copyright (c) 2014 Open Kernel Labs, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Server side core service application driver
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/ctype.h>

#include <vservices/types.h>
#include <vservices/transport.h>
#include <vservices/session.h>
#include <vservices/buffer.h>
#include <vservices/service.h>

#include <vservices/protocol/core/types.h>
#include <vservices/protocol/core/common.h>
#include <vservices/protocol/core/server.h>

#include "transport.h"
#include "session.h"
#include "compat.h"

#define VSERVICE_CORE_SERVICE_NAME	"core"

struct core_server {
	struct vs_server_core_state	state;
	struct vs_service_device	*service;

	/*
	 * A list of messages to send, a mutex protecting it, and a
	 * work item to process the list.
	 */
	struct list_head		message_queue;
	struct mutex			message_queue_lock;
	struct work_struct		message_queue_work;

	struct mutex			alloc_lock;

	/* The following are all protected by alloc_lock. */
	unsigned long			*in_notify_map;
	int				in_notify_map_bits;

	unsigned long			*out_notify_map;
	int				out_notify_map_bits;

	unsigned			in_quota_remaining;
	unsigned			out_quota_remaining;
};

/*
 * Used for message deferral when the core service is over quota.
 */
struct pending_message {
	vservice_core_message_id_t		type;
	struct vs_service_device		*service;
	struct list_head			list;
};

#define to_core_server(x)	container_of(x, struct core_server, state)
#define dev_to_core_server(x)	to_core_server(dev_get_drvdata(x))

static struct vs_session_device *
vs_core_server_session(struct core_server *server)
{
	return vs_service_get_session(server->service);
}

static struct core_server *
vs_server_session_core_server(struct vs_session_device *session)
{
	struct vs_service_device *core_service = session->core_service;

	if (!core_service)
		return NULL;

	return dev_to_core_server(&core_service->dev);
}

static int vs_server_core_send_service_removed(struct core_server *server,
		struct vs_service_device *service)
{
	return vs_server_core_core_send_service_removed(&server->state,
			service->id, GFP_KERNEL);
}

static bool
cancel_pending_created(struct core_server *server,
		struct vs_service_device *service)
{
	struct pending_message *msg;

	list_for_each_entry(msg, &server->message_queue, list) {
		if (msg->type == VSERVICE_CORE_CORE_MSG_SERVICE_CREATED &&
				msg->service == service) {
			vs_put_service(msg->service);
			list_del(&msg->list);
			kfree(msg);

			/* there can only be one */
			return true;
		}
	}

	return false;
}

static int vs_server_core_queue_service_removed(struct core_server *server,
		struct vs_service_device *service)
{
	struct pending_message *msg;

	lockdep_assert_held(&service->ready_lock);

	mutex_lock(&server->message_queue_lock);

	/*
	 * If we haven't sent the notification that the service was created,
	 * nuke it and do nothing else.
	 *
	 * This is not just an optimisation; see below.
	 */
	if (cancel_pending_created(server, service)) {
		mutex_unlock(&server->message_queue_lock);
		return 0;
	}

	/*
	 * Do nothing if the core state is not connected. We must avoid
	 * queueing service_removed messages on a reset service.
	 *
	 * Note that we cannot take the core server state lock here, because
	 * we may (or may not) have been called from a core service message
	 * handler. Thus, we must beware of races with changes to this
	 * condition:
	 *
	 * - It becomes true when the req_connect handler sends an
	 *   ack_connect, *after* it queues service_created for each existing
	 *   service (while holding the service ready lock). The handler sends
	 *   ack_connect with the message queue lock held.
	 *
	 *   - If we see the service as connected, then the req_connect
	 *     handler has already queued and sent a service_created for this
	 *     service, so it's ok for us to send a service_removed.
	 *
	 *   - If we see it as disconnected, the req_connect handler hasn't
	 *     taken the message queue lock to send ack_connect yet, and thus
	 *     has not released the service state lock; so if it queued a
	 *     service_created we caught it in the flush above before it was
	 *     sent.
	 *
	 * - It becomes false before the reset / disconnect handlers are
	 *   called and those will both flush the message queue afterwards.
	 *
	 *   - If we see the service as connected, then the reset / disconnect
	 *     handler is going to flush the message.
	 *
	 *   - If we see it disconnected, the state change has occurred and
	 *     implicitly had the same effect as this message, so doing
	 *     nothing is correct.
	 *
	 * Note that ordering in all of the above cases is guaranteed by the
	 * message queue lock.
	 */
	if (!VSERVICE_CORE_STATE_IS_CONNECTED(server->state.state.core)) {
		mutex_unlock(&server->message_queue_lock);
		return 0;
	}

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	if (!msg) {
		mutex_unlock(&server->message_queue_lock);
		return -ENOMEM;
	}

	msg->type = VSERVICE_CORE_CORE_MSG_SERVICE_REMOVED;
	/* put by message_queue_work */
	msg->service = vs_get_service(service);

	list_add_tail(&msg->list, &server->message_queue);

	mutex_unlock(&server->message_queue_lock);
	queue_work(server->service->work_queue, &server->message_queue_work);

	return 0;
}

static int vs_server_core_send_service_created(struct core_server *server,
		struct vs_service_device *service)
{
	struct vs_session_device *session =
			vs_service_get_session(server->service);

	struct vs_mbuf *mbuf;
	struct vs_string service_name, protocol_name;
	size_t service_name_len, protocol_name_len;

	int err;

	mbuf = vs_server_core_core_alloc_service_created(&server->state,
			&service_name, &protocol_name, GFP_KERNEL);

	if (IS_ERR(mbuf))
		return PTR_ERR(mbuf);

	vs_dev_debug(VS_DEBUG_SERVER, session, &session->dev,
			"Sending service created message for %d (%s:%s)\n",
			service->id, service->name, service->protocol);

	service_name_len = strlen(service->name);
	protocol_name_len = strlen(service->protocol);

	if (service_name_len > vs_string_max_size(&service_name) ||
			protocol_name_len > vs_string_max_size(&protocol_name)) {
		dev_err(&session->dev,
				"Invalid name/protocol for service %d (%s:%s)\n",
				service->id, service->name,
				service->protocol);
		err = -EINVAL;
		goto fail;
	}

	vs_string_copyin(&service_name, service->name);
	vs_string_copyin(&protocol_name, service->protocol);

	err = vs_server_core_core_send_service_created(&server->state,
			service->id, service_name, protocol_name, mbuf);
	if (err) {
		dev_err(&session->dev,
				"Fatal error sending service creation message for %d (%s:%s): %d\n",
				service->id, service->name,
				service->protocol, err);
		goto fail;
	}

	return 0;

fail:
	vs_server_core_core_free_service_created(&server->state,
			&service_name, &protocol_name, mbuf);

	return err;
}

static int vs_server_core_queue_service_created(struct core_server *server,
		struct vs_service_device *service)
{
	struct pending_message *msg;

	lockdep_assert_held(&service->ready_lock);
	lockdep_assert_held(&server->service->state_mutex);

	mutex_lock(&server->message_queue_lock);

	/*  Do nothing if the core state is disconnected.  */
	if (!VSERVICE_CORE_STATE_IS_CONNECTED(server->state.state.core)) {
		mutex_unlock(&server->message_queue_lock);
		return 0;
	}

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	if (!msg) {
		mutex_unlock(&server->message_queue_lock);
		return -ENOMEM;
	}

	msg->type = VSERVICE_CORE_CORE_MSG_SERVICE_CREATED;
	/* put by message_queue_work */
	msg->service = vs_get_service(service);

	list_add_tail(&msg->list, &server->message_queue);

	mutex_unlock(&server->message_queue_lock);
	queue_work(server->service->work_queue, &server->message_queue_work);

	return 0;
}

static struct vs_service_device *
__vs_server_core_register_service(struct vs_session_device *session,
		vs_service_id_t service_id, struct vs_service_device *owner,
		const char *name, const char *protocol, const void *plat_data)
{
	if (!session->is_server)
		return ERR_PTR(-ENODEV);

	if (!name || strnlen(name, VSERVICE_CORE_SERVICE_NAME_SIZE + 1) >
			VSERVICE_CORE_SERVICE_NAME_SIZE || name[0] == '\n')
		return ERR_PTR(-EINVAL);

	/* The server core must only be registered as service_id zero */
	if (service_id == 0 && (owner != NULL ||
			strcmp(name, VSERVICE_CORE_SERVICE_NAME) != 0 ||
			strcmp(protocol, VSERVICE_CORE_PROTOCOL_NAME) != 0))
		return ERR_PTR(-EINVAL);

	return vs_service_register(session, owner, service_id, protocol, name,
			plat_data);
}

static struct vs_service_device *
vs_server_core_create_service(struct core_server *server,
		struct vs_session_device *session,
		struct vs_service_device *owner, vs_service_id_t service_id,
		const char *name, const char *protocol, const void *plat_data)
{
	struct vs_service_device *service;

	service = __vs_server_core_register_service(session, service_id,
			owner, name, protocol, plat_data);
	if (IS_ERR(service))
		return service;

	if (protocol) {
		vs_service_state_lock(server->service);
		vs_service_start(service);
		if (VSERVICE_CORE_STATE_IS_CONNECTED(server->state.state.core))
			vs_service_enable(service);
		vs_service_state_unlock(server->service);
	}

	return service;
}

static int
vs_server_core_send_service_reset_ready(struct core_server *server,
		vservice_core_message_id_t type,
		struct vs_service_device *service)
{
	bool is_reset = (type == VSERVICE_CORE_CORE_MSG_SERVICE_RESET);
	struct vs_session_device *session __maybe_unused =
			vs_service_get_session(server->service);
	int err;

	vs_dev_debug(VS_DEBUG_SERVER, session, &session->dev,
			"Sending %s for service %d\n",
			is_reset ? "reset" : "ready", service->id);

	if (is_reset)
		err = vs_server_core_core_send_service_reset(&server->state,
				service->id, GFP_KERNEL);
	else
		err = vs_server_core_core_send_server_ready(&server->state,
				service->id, service->recv_quota,
				service->send_quota,
				service->notify_recv_offset,
				service->notify_recv_bits,
				service->notify_send_offset,
				service->notify_send_bits,
				GFP_KERNEL);

	return err;
}

static bool
cancel_pending_ready(struct core_server *server,
		struct vs_service_device *service)
{
	struct pending_message *msg;

	list_for_each_entry(msg, &server->message_queue, list) {
		if (msg->type == VSERVICE_CORE_CORE_MSG_SERVER_READY &&
				msg->service == service) {
			vs_put_service(msg->service);
			list_del(&msg->list);
			kfree(msg);

			/* there can only be one */
			return true;
		}
	}

	return false;
}

static int
vs_server_core_queue_service_reset_ready(struct core_server *server,
		vservice_core_message_id_t type,
		struct vs_service_device *service)
{
	bool is_reset = (type == VSERVICE_CORE_CORE_MSG_SERVICE_RESET);
	struct pending_message *msg;

	mutex_lock(&server->message_queue_lock);

	/*
	 * If this is a reset, and there is an outgoing ready in the
	 * queue, we must cancel it so it can't be sent with invalid
	 * transport resources, and then return immediately so we
	 * don't send a redundant reset.
	 */
	if (is_reset && cancel_pending_ready(server, service)) {
		mutex_unlock(&server->message_queue_lock);
		return VS_SERVICE_ALREADY_RESET;
	}

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	if (!msg) {
		mutex_unlock(&server->message_queue_lock);
		return -ENOMEM;
	}

	msg->type = type;
	/* put by message_queue_work */
	msg->service = vs_get_service(service);
	list_add_tail(&msg->list, &server->message_queue);

	mutex_unlock(&server->message_queue_lock);
	queue_work(server->service->work_queue, &server->message_queue_work);

	return 0;
}

static int vs_core_server_tx_ready(struct vs_server_core_state *state)
{
	struct core_server *server = to_core_server(state);
	struct vs_session_device *session __maybe_unused =
			vs_service_get_session(server->service);

	vs_dev_debug(VS_DEBUG_SERVER, session, &session->dev, "tx_ready\n");

	queue_work(server->service->work_queue, &server->message_queue_work);

	return 0;
}

static void message_queue_work(struct work_struct *work)
{
	struct core_server *server = container_of(work, struct core_server,
			message_queue_work);
	struct pending_message *msg;
	int err;

	vs_service_state_lock(server->service);

	if (!VSERVICE_CORE_STATE_IS_CONNECTED(server->state.state.core)) {
		vs_service_state_unlock(server->service);
		return;
	}

	/*
	 * If any pending message fails we exit the loop immediately so that
	 * we preserve the message order.
	 */
	mutex_lock(&server->message_queue_lock);
	while (!list_empty(&server->message_queue)) {
		msg = list_first_entry(&server->message_queue,
				struct pending_message, list);

		switch (msg->type) {
		case VSERVICE_CORE_CORE_MSG_SERVICE_CREATED:
			err = vs_server_core_send_service_created(server,
					msg->service);
			break;

		case VSERVICE_CORE_CORE_MSG_SERVICE_REMOVED:
			err = vs_server_core_send_service_removed(server,
					msg->service);
			break;

		case VSERVICE_CORE_CORE_MSG_SERVICE_RESET:
		case VSERVICE_CORE_CORE_MSG_SERVER_READY:
			err = vs_server_core_send_service_reset_ready(
					server, msg->type, msg->service);
			break;

		default:
			dev_warn(&server->service->dev,
					"Don't know how to handle pending message type %d\n",
					msg->type);
			err = 0;
			break;
		}

		/*
		 * If we're out of quota we exit and wait for tx_ready to
		 * queue us again.
		 */
		if (err == -ENOBUFS)
			break;

		/* Any other error is fatal */
		if (err < 0) {
			dev_err(&server->service->dev,
					"Failed to send pending message type %d: %d - resetting session",
					msg->type, err);
			vs_service_reset_nosync(server->service);
			break;
		}

		/*
		 * The message sent successfully - remove it from the
		 * queue. The corresponding vs_get_service() was done
		 * when the pending message was created.
		 */
		vs_put_service(msg->service);
		list_del(&msg->list);
		kfree(msg);
	}
	mutex_unlock(&server->message_queue_lock);

	vs_service_state_unlock(server->service);

	return;
}

/*
 * Core server sysfs interface
 */
static ssize_t server_core_create_service_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct vs_service_device *service = to_vs_service_device(dev);
	struct vs_session_device *session = to_vs_session_device(dev->parent);
	struct core_server *server = dev_to_core_server(&service->dev);
	struct vs_service_device *new_service;
	char *p;
	ssize_t ret = count;

	/* FIXME - Buffer sizes are not defined in generated headers */
	/* discard leading whitespace */
	while (count && isspace(*buf)) {
		buf++;
		count--;
	}
	if (!count) {
		dev_info(dev, "empty service name");
		return -EINVAL;
	}
	/* discard trailing whitespace */
	while (count && isspace(buf[count - 1]))
		count--;

	if (count > VSERVICE_CORE_SERVICE_NAME_SIZE) {
		dev_info(dev, "service name too long (max %d)\n", VSERVICE_CORE_SERVICE_NAME_SIZE);
		return -EINVAL;
	}

	p = kstrndup(buf, count, GFP_KERNEL);

	/*
	 * Writing a service name to this file creates a new service. The
	 * service is created without a protocol. It will appear in sysfs
	 * but will not be bound to a driver until a valid protocol name
	 * has been written to the created devices protocol sysfs attribute.
	 */
	new_service = vs_server_core_create_service(server, session, service,
			VS_SERVICE_AUTO_ALLOCATE_ID, p, NULL, NULL);
	if (IS_ERR(new_service))
		ret = PTR_ERR(new_service);

	kfree(p);

	return ret;
}

static ssize_t server_core_reset_service_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct vs_service_device *core_service = to_vs_service_device(dev);
	struct vs_session_device *session =
		vs_service_get_session(core_service);
	struct vs_service_device *target;
	vs_service_id_t service_id;
	unsigned long val;
	int err;

	/*
	 * Writing a valid service_id to this file does a reset of that service
	 */
	err = kstrtoul(buf, 0, &val);
	if (err)
		return err;

	service_id = val;
	target = vs_session_get_service(session, service_id);
	if (!target)
		return -EINVAL;

	err = vs_service_reset(target, core_service);

	vs_put_service(target);
	return err < 0 ? err : count;
}

static ssize_t server_core_remove_service_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct vs_service_device *service = to_vs_service_device(dev);
	struct vs_session_device *session = vs_service_get_session(service);
	struct vs_service_device *target;
	vs_service_id_t service_id;
	unsigned long val;
	int err;

	err = kstrtoul(buf, 0, &val);
	if (err)
		return err;

	service_id = val;
	if (service_id == 0) {
		/*
		 * We don't allow removing the core service this way. The
		 * core service will be removed when the session is removed.
		 */
		return -EINVAL;
	}

	target = vs_session_get_service(session, service_id);
	if (!target)
		return -EINVAL;

	err = vs_service_delete(target, service);

	vs_put_service(target);
	return err < 0 ? err : count;
}

static DEVICE_ATTR(create_service, S_IWUSR,
		NULL, server_core_create_service_store);
static DEVICE_ATTR(reset_service, S_IWUSR,
		NULL, server_core_reset_service_store);
static DEVICE_ATTR(remove_service, S_IWUSR,
		NULL, server_core_remove_service_store);

static struct attribute *server_core_dev_attrs[] = {
	&dev_attr_create_service.attr,
	&dev_attr_reset_service.attr,
	&dev_attr_remove_service.attr,
	NULL,
};

static const struct attribute_group server_core_attr_group = {
	.attrs = server_core_dev_attrs,
};

static int init_transport_resource_allocation(struct core_server *server)
{
	struct vs_session_device *session = vs_core_server_session(server);
	struct vs_transport *transport = session->transport;
	size_t size;
	int err;

	mutex_init(&server->alloc_lock);
	mutex_lock(&server->alloc_lock);

	transport->vt->get_quota_limits(transport, &server->out_quota_remaining,
			&server->in_quota_remaining);

	transport->vt->get_notify_bits(transport, &server->out_notify_map_bits,
			&server->in_notify_map_bits);

	size = BITS_TO_LONGS(server->in_notify_map_bits) *
			sizeof(unsigned long);
	server->in_notify_map = kzalloc(size, GFP_KERNEL);
	if (server->in_notify_map_bits && !server->in_notify_map) {
		err = -ENOMEM;
		goto fail;
	}

	size = BITS_TO_LONGS(server->out_notify_map_bits) *
			sizeof(unsigned long);
	server->out_notify_map = kzalloc(size, GFP_KERNEL);
	if (server->out_notify_map_bits && !server->out_notify_map) {
		err = -ENOMEM;
		goto fail_free_in_bits;
	}

	mutex_unlock(&server->alloc_lock);

	return 0;

fail_free_in_bits:
	kfree(server->in_notify_map);
fail:
	mutex_unlock(&server->alloc_lock);
	return err;
}

static int alloc_quota(unsigned minimum, unsigned best, unsigned set,
		unsigned *remaining)
{
	unsigned quota;

	if (set) {
		quota = set;

		if (quota > *remaining)
			return -ENOSPC;
	} else if (best) {
		quota = min(best, *remaining);
	} else {
		quota = minimum;
	}

	if (quota < minimum)
		return -ENOSPC;

	*remaining -= quota;

	return min_t(unsigned, quota, INT_MAX);
}

static int alloc_notify_bits(unsigned notify_count, unsigned long *map,
		unsigned nr_bits)
{
	unsigned offset;

	if (notify_count) {
		offset = bitmap_find_next_zero_area(map, nr_bits, 0,
				notify_count, 0);

		if (offset >= nr_bits || offset > (unsigned)INT_MAX)
			return -ENOSPC;

		bitmap_set(map, offset, notify_count);
	} else {
		offset = 0;
	}

	return offset;
}

/*
 * alloc_transport_resources - Allocates the quotas and notification bits for
 * a service.
 * @server: the core service state.
 * @service: the service device to allocate resources for.
 *
 * This function allocates message quotas and notification bits. It is called
 * for the core service in alloc(), and for every other service by the server
 * bus probe() function.
 */
static int alloc_transport_resources(struct core_server *server,
		struct vs_service_device *service)
{
	struct vs_session_device *session __maybe_unused =
			vs_service_get_session(service);
	unsigned in_bit_offset, out_bit_offset;
	unsigned in_quota, out_quota;
	int ret;
	struct vs_service_driver *driver;

	if (WARN_ON(!service->dev.driver))
		return -ENODEV;

	mutex_lock(&server->alloc_lock);

	driver = to_vs_service_driver(service->dev.driver);

	/* Quota allocations */
	ret = alloc_quota(driver->in_quota_min, driver->in_quota_best,
			service->in_quota_set, &server->in_quota_remaining);
	if (ret < 0) {
		dev_err(&service->dev, "cannot allocate in quota\n");
		goto fail_in_quota;
	}
	in_quota = ret;

	ret = alloc_quota(driver->out_quota_min, driver->out_quota_best,
			service->out_quota_set, &server->out_quota_remaining);
	if (ret < 0) {
		dev_err(&service->dev, "cannot allocate out quota\n");
		goto fail_out_quota;
	}
	out_quota = ret;

	vs_dev_debug(VS_DEBUG_SERVER_CORE, session, &session->dev,
			"%d: quota in: %u out: %u; remaining in: %u out: %u\n",
			service->id, in_quota, out_quota,
			server->in_quota_remaining,
			server->out_quota_remaining);

	/* Notification bit allocations */
	ret = alloc_notify_bits(service->notify_recv_bits,
			server->in_notify_map, server->in_notify_map_bits);
	if (ret < 0) {
		dev_err(&service->dev, "cannot allocate in notify bits\n");
		goto fail_in_notify;
	}
	in_bit_offset = ret;

	ret = alloc_notify_bits(service->notify_send_bits,
			server->out_notify_map, server->out_notify_map_bits);
	if (ret < 0) {
		dev_err(&service->dev, "cannot allocate out notify bits\n");
		goto fail_out_notify;
	}
	out_bit_offset = ret;

	vs_dev_debug(VS_DEBUG_SERVER_CORE, session, &session->dev,
			"notify bits in: %u/%u out: %u/%u\n",
			in_bit_offset, service->notify_recv_bits,
			out_bit_offset, service->notify_send_bits);

	/* Fill in the device's allocations */
	service->recv_quota = in_quota;
	service->send_quota = out_quota;
	service->notify_recv_offset = in_bit_offset;
	service->notify_send_offset = out_bit_offset;

	mutex_unlock(&server->alloc_lock);

	return 0;

fail_out_notify:
	if (service->notify_recv_bits)
		bitmap_clear(server->in_notify_map,
				in_bit_offset, service->notify_recv_bits);
fail_in_notify:
	server->out_quota_remaining += out_quota;
fail_out_quota:
	server->in_quota_remaining += in_quota;
fail_in_quota:

	mutex_unlock(&server->alloc_lock);

	service->recv_quota = 0;
	service->send_quota = 0;
	service->notify_recv_bits = 0;
	service->notify_recv_offset = 0;
	service->notify_send_bits = 0;
	service->notify_send_offset = 0;

	return ret;
}

/*
 * free_transport_resources - Frees the quotas and notification bits for
 * a non-core service.
 * @server: the core service state.
 * @service: the service device to free resources for.
 *
 * This function is called by the server to free message quotas and
 * notification bits that were allocated by alloc_transport_resources. It must
 * only be called when the target service is in reset, and must be called with
 * the core service's state lock held.
 */
static int free_transport_resources(struct core_server *server,
		struct vs_service_device *service)
{
	mutex_lock(&server->alloc_lock);

	if (service->notify_recv_bits)
		bitmap_clear(server->in_notify_map,
				service->notify_recv_offset,
				service->notify_recv_bits);

	if (service->notify_send_bits)
		bitmap_clear(server->out_notify_map,
				service->notify_send_offset,
				service->notify_send_bits);

	server->in_quota_remaining += service->recv_quota;
	server->out_quota_remaining += service->send_quota;

	mutex_unlock(&server->alloc_lock);

	service->recv_quota = 0;
	service->send_quota = 0;
	service->notify_recv_bits = 0;
	service->notify_recv_offset = 0;
	service->notify_send_bits = 0;
	service->notify_send_offset = 0;

	return 0;
}

static struct vs_server_core_state *
vs_core_server_alloc(struct vs_service_device *service)
{
	struct core_server *server;
	int err;

	if (WARN_ON(service->id != 0))
		goto fail;

	server = kzalloc(sizeof(*server), GFP_KERNEL);
	if (!server)
		goto fail;

	server->service = service;
	INIT_LIST_HEAD(&server->message_queue);
	INIT_WORK(&server->message_queue_work, message_queue_work);
	mutex_init(&server->message_queue_lock);

	err = init_transport_resource_allocation(server);
	if (err)
		goto fail_init_alloc;

	err = alloc_transport_resources(server, service);
	if (err)
		goto fail_alloc_transport;

	err = sysfs_create_group(&service->dev.kobj, &server_core_attr_group);
	if (err)
		goto fail_sysfs;

	return &server->state;

fail_sysfs:
	free_transport_resources(server, service);
fail_alloc_transport:
	kfree(server->out_notify_map);
	kfree(server->in_notify_map);
fail_init_alloc:
	kfree(server);
fail:
	return NULL;
}

static void vs_core_server_release(struct vs_server_core_state *state)
{
	struct core_server *server = to_core_server(state);
	struct vs_session_device *session = vs_core_server_session(server);

	/* Delete all the other services */
	vs_session_delete_noncore(session);

	sysfs_remove_group(&server->service->dev.kobj, &server_core_attr_group);
	kfree(server->out_notify_map);
	kfree(server->in_notify_map);
	kfree(server);
}

/**
 * vs_server_create_service - create and register a new vService server
 * @session: the session to create the vService server on
 * @parent: an existing server that is managing the new server
 * @name: the name of the new service
 * @protocol: the protocol for the new service
 * @plat_data: value to be assigned to (struct device *)->platform_data
 */
struct vs_service_device *
vs_server_create_service(struct vs_session_device *session,
		struct vs_service_device *parent, const char *name,
		const char *protocol, const void *plat_data)
{
	struct vs_service_device *core_service, *new_service;
	struct core_server *server;

	if (!session->is_server || !name || !protocol)
		return NULL;

	core_service = session->core_service;
	if (!core_service)
		return NULL;

	device_lock(&core_service->dev);
	if (!core_service->dev.driver) {
		device_unlock(&core_service->dev);
		return NULL;
	}

	server = dev_to_core_server(&core_service->dev);

	if (!parent)
		parent = core_service;

	new_service = vs_server_core_create_service(server, session, parent,
			VS_SERVICE_AUTO_ALLOCATE_ID, name, protocol, plat_data);

	device_unlock(&core_service->dev);

	if (IS_ERR(new_service))
		return NULL;

	return new_service;
}
EXPORT_SYMBOL(vs_server_create_service);

/**
 * vs_server_destroy_service - destroy and unregister a vService server. This
 * function must _not_ be used from the target service's own workqueue.
 * @service: The service to destroy
 */
int vs_server_destroy_service(struct vs_service_device *service,
		struct vs_service_device *parent)
{
	struct vs_session_device *session = vs_service_get_session(service);

	if (!session->is_server || service->id == 0)
		return -EINVAL;

	if (!parent)
		parent = session->core_service;

	return vs_service_delete(service, parent);
}
EXPORT_SYMBOL(vs_server_destroy_service);

static void __queue_service_created(struct vs_service_device *service,
		void *data)
{
	struct core_server *server = (struct core_server *)data;

	vs_server_core_queue_service_created(server, service);
}

static int vs_server_core_handle_connect(struct vs_server_core_state *state)
{
	struct core_server *server = to_core_server(state);
	struct vs_session_device *session = vs_core_server_session(server);
	int err;

	/* Tell the other end that we've finished connecting. */
	err = vs_server_core_core_send_ack_connect(state, GFP_KERNEL);
	if (err)
		return err;

	/* Queue a service-created message for each existing service. */
	vs_session_for_each_service(session, __queue_service_created, server);

	/* Re-enable all the services. */
	vs_session_enable_noncore(session);

	return 0;
}

static void vs_core_server_disable_services(struct core_server *server)
{
	struct vs_session_device *session = vs_core_server_session(server);
	struct pending_message *msg;

	/* Disable all the other services */
	vs_session_disable_noncore(session);

	/* Flush all the pending service-readiness messages */
	mutex_lock(&server->message_queue_lock);
	while (!list_empty(&server->message_queue)) {
		msg = list_first_entry(&server->message_queue,
				struct pending_message, list);
		vs_put_service(msg->service);
		list_del(&msg->list);
		kfree(msg);
	}
	mutex_unlock(&server->message_queue_lock);
}

static int vs_server_core_handle_disconnect(struct vs_server_core_state *state)
{
	struct core_server *server = to_core_server(state);

	vs_core_server_disable_services(server);

	return vs_server_core_core_send_ack_disconnect(state, GFP_KERNEL);
}

static int
vs_server_core_handle_service_reset(struct vs_server_core_state *state,
		unsigned service_id)
{
	struct core_server *server = to_core_server(state);
	struct vs_session_device *session = vs_core_server_session(server);

	if (service_id == 0)
		return -EPROTO;

	return vs_service_handle_reset(session, service_id, false);
}

static void vs_core_server_start(struct vs_server_core_state *state)
{
	struct core_server *server = to_core_server(state);
	struct vs_session_device *session = vs_core_server_session(server);
	int err;

	vs_dev_debug(VS_DEBUG_SERVER_CORE, session, &server->service->dev,
			"Core server start\n");

	err = vs_server_core_core_send_startup(&server->state,
			server->service->recv_quota,
			server->service->send_quota, GFP_KERNEL);

	if (err)
		dev_err(&session->dev, "Failed to start core protocol: %d\n",
				err);
}

static void vs_core_server_reset(struct vs_server_core_state *state)
{
	struct core_server *server = to_core_server(state);
	struct vs_session_device *session = vs_core_server_session(server);

	vs_dev_debug(VS_DEBUG_SERVER_CORE, session, &server->service->dev,
			"Core server reset\n");

	vs_core_server_disable_services(server);
}

static struct vs_server_core vs_core_server_driver = {
	.alloc		= vs_core_server_alloc,
	.release	= vs_core_server_release,
	.start		= vs_core_server_start,
	.reset		= vs_core_server_reset,
	.tx_ready	= vs_core_server_tx_ready,
	.core = {
		.req_connect		= vs_server_core_handle_connect,
		.req_disconnect		= vs_server_core_handle_disconnect,
		.msg_service_reset	= vs_server_core_handle_service_reset,
	},
};

/*
 * Server bus driver
 */
static int vs_server_bus_match(struct device *dev, struct device_driver *driver)
{
	struct vs_service_device *service = to_vs_service_device(dev);
	struct vs_service_driver *vsdrv = to_vs_service_driver(driver);

	/* Don't match anything to the devio driver; it's bound manually */
	if (!vsdrv->protocol)
		return 0;

	WARN_ON_ONCE(!service->is_server || !vsdrv->is_server);

	/* Don't match anything that doesn't have a protocol set yet */
	if (!service->protocol)
		return 0;

	if (strcmp(service->protocol, vsdrv->protocol) == 0)
		return 1;

	return 0;
}

static int vs_server_bus_probe(struct device *dev)
{
	struct vs_service_device *service = to_vs_service_device(dev);
	struct vs_session_device *session = vs_service_get_session(service);
	struct core_server *server = vs_server_session_core_server(session);
	int ret;

	/*
	 * Set the notify counts for the service, unless the driver is the
	 * devio driver in which case it has already been done by the devio
	 * bind ioctl. The devio driver cannot be bound automatically.
	 */
	struct vs_service_driver *driver =
		to_vs_service_driver(service->dev.driver);
#ifdef CONFIG_VSERVICES_CHAR_DEV
	if (driver != &vs_devio_server_driver)
#endif
	{
		service->notify_recv_bits = driver->in_notify_count;
		service->notify_send_bits = driver->out_notify_count;
	}

	/*
	 * We can't allocate transport resources here for the core service
	 * because the resource pool doesn't exist yet. It's done in alloc()
	 * instead (which is called, indirectly, by vs_service_bus_probe()).
	 */
	if (service->id == 0)
		return vs_service_bus_probe(dev);

	if (!server)
		return -ENODEV;
	ret = alloc_transport_resources(server, service);
	if (ret < 0)
		goto fail;

	ret = vs_service_bus_probe(dev);
	if (ret < 0)
		goto fail_free_resources;

	return 0;

fail_free_resources:
	free_transport_resources(server, service);
fail:
	return ret;
}

static int vs_server_bus_remove(struct device *dev)
{
	struct vs_service_device *service = to_vs_service_device(dev);
	struct vs_session_device *session = vs_service_get_session(service);
	struct core_server *server = vs_server_session_core_server(session);

	vs_service_bus_remove(dev);

	/*
	 * We skip free_transport_resources for the core service because the
	 * resource pool has already been freed at this point. It's also
	 * possible that the core service has disappeared, in which case
	 * there's no work to do here.
	 */
	if (server != NULL && service->id != 0)
		free_transport_resources(server, service);

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

struct service_enable_work_struct {
	struct vs_service_device *service;
	struct work_struct work;
};

static void service_enable_work(struct work_struct *work)
{
	struct service_enable_work_struct *enable_work = container_of(work,
			struct service_enable_work_struct, work);
	struct vs_service_device *service = enable_work->service;
	struct vs_session_device *session = vs_service_get_session(service);
	struct core_server *server = vs_server_session_core_server(session);
	bool started;
	int ret;

	kfree(enable_work);

	if (!server)
		return;
	/* Start and enable the service */
	vs_service_state_lock(server->service);
	started = vs_service_start(service);
	if (!started) {
		vs_service_state_unlock(server->service);
		vs_put_service(service);
		return;
	}

	if (VSERVICE_CORE_STATE_IS_CONNECTED(server->state.state.core))
		vs_service_enable(service);
	vs_service_state_unlock(server->service);

	/* Tell the bus to search for a driver that supports the protocol */
	ret = device_attach(&service->dev);
	if (ret == 0)
		dev_warn(&service->dev, "No driver found for protocol: %s\n",
				service->protocol);
	kobject_uevent(&service->dev.kobj, KOBJ_CHANGE);

	/* The corresponding vs_get_service was done when the work was queued */
	vs_put_service(service);
}

static ssize_t dev_protocol_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct vs_service_device *service = to_vs_service_device(dev);
	struct service_enable_work_struct *enable_work;

	/* The protocol can only be set once */
	if (service->protocol)
		return -EPERM;

	/* Registering additional core servers is not allowed */
	if (strcmp(buf, VSERVICE_CORE_PROTOCOL_NAME) == 0)
		return -EINVAL;

	if (strnlen(buf, VSERVICE_CORE_PROTOCOL_NAME_SIZE) + 1 >
			VSERVICE_CORE_PROTOCOL_NAME_SIZE)
		return -E2BIG;

	enable_work = kmalloc(sizeof(*enable_work), GFP_KERNEL);
	if (!enable_work)
		return -ENOMEM;

	/* Set the protocol and tell the client about it */
	service->protocol = kstrdup(buf, GFP_KERNEL);
	if (!service->protocol) {
		kfree(enable_work);
		return -ENOMEM;
	}
	strim(service->protocol);

	/*
	 * Schedule work to enable the service. We can't do it here because
	 * we need to take the core service lock, and doing that here makes
	 * it depend circularly on this sysfs attribute, which can be deleted
	 * with that lock held.
	 *
	 * The corresponding vs_put_service is called in the enable_work
	 * function.
	 */
	INIT_WORK(&enable_work->work, service_enable_work);
	enable_work->service = vs_get_service(service);
	schedule_work(&enable_work->work);

	return count;
}

static ssize_t service_name_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct vs_service_device *service = to_vs_service_device(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n", service->name);
}

static ssize_t quota_in_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct vs_service_device *service = to_vs_service_device(dev);
	struct vs_session_device *session = vs_service_get_session(service);
	struct core_server *server = vs_server_session_core_server(session);
	int ret;
	unsigned long in_quota;

	if (!server)
		return -ENODEV;
	/*
	 * Don't allow quota to be changed for services that have a driver
	 * bound. We take the alloc lock here because the device lock is held
	 * while creating and destroying this sysfs item. This means we can
	 * race with driver binding, but that doesn't matter: we actually just
	 * want to know that alloc_transport_resources() hasn't run yet, and
	 * that takes the alloc lock.
	 */
	mutex_lock(&server->alloc_lock);
	if (service->dev.driver) {
		ret = -EPERM;
		goto out;
	}

	ret = kstrtoul(buf, 0, &in_quota);
	if (ret < 0)
		goto out;

	service->in_quota_set = in_quota;
	ret = count;

out:
	mutex_unlock(&server->alloc_lock);

	return ret;
}

static ssize_t quota_in_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct vs_service_device *service = to_vs_service_device(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", service->recv_quota);
}

static ssize_t quota_out_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct vs_service_device *service = to_vs_service_device(dev);
	struct vs_session_device *session = vs_service_get_session(service);
	struct core_server *server = vs_server_session_core_server(session);
	int ret;
	unsigned long out_quota;

	if (!server)
		return -ENODEV;
	/* See comment in quota_in_store. */
	mutex_lock(&server->alloc_lock);
	if (service->dev.driver) {
		ret = -EPERM;
		goto out;
	}

	ret = kstrtoul(buf, 0, &out_quota);
	if (ret < 0)
		goto out;

	service->out_quota_set = out_quota;
	ret = count;

out:
	mutex_unlock(&server->alloc_lock);

	return ret;
}

static ssize_t quota_out_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct vs_service_device *service = to_vs_service_device(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", service->send_quota);
}

static struct device_attribute vs_server_dev_attrs[] = {
	__ATTR_RO(id),
	__ATTR_RO(is_server),
	__ATTR(protocol, S_IRUGO | S_IWUSR,
			dev_protocol_show, dev_protocol_store),
	__ATTR_RO(service_name),
	__ATTR(quota_in, S_IRUGO | S_IWUSR,
			quota_in_show, quota_in_store),
	__ATTR(quota_out, S_IRUGO | S_IWUSR,
			quota_out_show, quota_out_store),
	__ATTR_NULL
};

static ssize_t protocol_show(struct device_driver *drv, char *buf)
{
	struct vs_service_driver *vsdrv = to_vs_service_driver(drv);

	return scnprintf(buf, PAGE_SIZE, "%s\n", vsdrv->protocol);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 0)
static struct driver_attribute vs_server_drv_attrs[] = {
	__ATTR_RO(protocol),
	__ATTR_NULL
};
#else
static DRIVER_ATTR_RO(protocol);

static struct attribute *vs_server_drv_attrs[] = {
	&driver_attr_protocol.attr,
	NULL,
};
ATTRIBUTE_GROUPS(vs_server_drv);
#endif

struct bus_type vs_server_bus_type = {
	.name		= "vservices-server",
	.dev_attrs	= vs_server_dev_attrs,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 0)
	.drv_attrs	= vs_server_drv_attrs,
#else
	.drv_groups	= vs_server_drv_groups,
#endif
	.match		= vs_server_bus_match,
	.probe		= vs_server_bus_probe,
	.remove		= vs_server_bus_remove,
	.uevent		= vs_service_bus_uevent,
};
EXPORT_SYMBOL(vs_server_bus_type);

/*
 * Server session driver
 */
static int vs_server_session_probe(struct device *dev)
{
	struct vs_session_device *session = to_vs_session_device(dev);
	struct vs_service_device *service;

	service = __vs_server_core_register_service(session, 0, NULL,
			VSERVICE_CORE_SERVICE_NAME,
			VSERVICE_CORE_PROTOCOL_NAME, NULL);
	if (IS_ERR(service))
		return PTR_ERR(service);

	return 0;
}

static int
vs_server_session_service_added(struct vs_session_device *session,
		struct vs_service_device *service)
{
	struct core_server *server = vs_server_session_core_server(session);
	int err;

	if (WARN_ON(!server || !service->id))
		return -EINVAL;

	err = vs_server_core_queue_service_created(server, service);

	if (err)
		vs_dev_debug(VS_DEBUG_SERVER_CORE, session, &session->dev,
				"failed to send service_created: %d\n", err);

	return err;
}

static int
vs_server_session_service_start(struct vs_session_device *session,
		struct vs_service_device *service)
{
	struct core_server *server = vs_server_session_core_server(session);
	int err;

	if (WARN_ON(!server || !service->id))
		return -EINVAL;

	err = vs_server_core_queue_service_reset_ready(server,
			VSERVICE_CORE_CORE_MSG_SERVER_READY, service);

	if (err)
		vs_dev_debug(VS_DEBUG_SERVER_CORE, session, &session->dev,
				"failed to send server_ready: %d\n", err);

	return err;
}

static int
vs_server_session_service_local_reset(struct vs_session_device *session,
		struct vs_service_device *service)
{
	struct core_server *server = vs_server_session_core_server(session);
	int err;

	if (WARN_ON(!server || !service->id))
		return -EINVAL;

	err = vs_server_core_queue_service_reset_ready(server,
			VSERVICE_CORE_CORE_MSG_SERVICE_RESET, service);

	if (err)
		vs_dev_debug(VS_DEBUG_SERVER_CORE, session, &session->dev,
				"failed to send service_reset: %d\n", err);

	return err;
}

static int
vs_server_session_service_removed(struct vs_session_device *session,
		struct vs_service_device *service)
{
	struct core_server *server = vs_server_session_core_server(session);
	int err;

	/*
	 * It's possible for the core server to be forcibly removed before
	 * the other services, for example when the underlying transport
	 * vanishes. If that happens, we can end up here with a NULL core
	 * server pointer.
	 */
	if (!server)
		return 0;

	if (WARN_ON(!service->id))
		return -EINVAL;

	err = vs_server_core_queue_service_removed(server, service);
	if (err)
		vs_dev_debug(VS_DEBUG_SERVER_CORE, session, &session->dev,
				"failed to send service_removed: %d\n", err);

	return err;
}

static struct vs_session_driver vs_server_session_driver = {
	.driver	= {
		.name			= "vservices-server-session",
		.owner			= THIS_MODULE,
		.bus			= &vs_session_bus_type,
		.probe			= vs_server_session_probe,
		.suppress_bind_attrs	= true,
	},
	.is_server		= true,
	.service_bus		= &vs_server_bus_type,
	.service_added		= vs_server_session_service_added,
	.service_start		= vs_server_session_service_start,
	.service_local_reset	= vs_server_session_service_local_reset,
	.service_removed	= vs_server_session_service_removed,
};

static int __init vs_core_server_init(void)
{
	int ret;

	ret = bus_register(&vs_server_bus_type);
	if (ret)
		goto fail_bus_register;

#ifdef CONFIG_VSERVICES_CHAR_DEV
	vs_devio_server_driver.driver.bus = &vs_server_bus_type;
	vs_devio_server_driver.driver.owner = THIS_MODULE;
	ret = driver_register(&vs_devio_server_driver.driver);
	if (ret)
		goto fail_devio_register;
#endif

	ret = driver_register(&vs_server_session_driver.driver);
	if (ret)
		goto fail_driver_register;

	ret = vservice_core_server_register(&vs_core_server_driver,
			"vs_core_server");
	if (ret)
		goto fail_core_register;

	vservices_server_root = kobject_create_and_add("server-sessions",
			vservices_root);
	if (!vservices_server_root) {
		ret = -ENOMEM;
		goto fail_create_root;
	}

	return 0;

fail_create_root:
	vservice_core_server_unregister(&vs_core_server_driver);
fail_core_register:
	driver_unregister(&vs_server_session_driver.driver);
fail_driver_register:
#ifdef CONFIG_VSERVICES_CHAR_DEV
	driver_unregister(&vs_devio_server_driver.driver);
	vs_devio_server_driver.driver.bus = NULL;
	vs_devio_server_driver.driver.owner = NULL;
fail_devio_register:
#endif
	bus_unregister(&vs_server_bus_type);
fail_bus_register:
	return ret;
}

static void __exit vs_core_server_exit(void)
{
	kobject_put(vservices_server_root);
	vservice_core_server_unregister(&vs_core_server_driver);
	driver_unregister(&vs_server_session_driver.driver);
#ifdef CONFIG_VSERVICES_CHAR_DEV
	driver_unregister(&vs_devio_server_driver.driver);
	vs_devio_server_driver.driver.bus = NULL;
	vs_devio_server_driver.driver.owner = NULL;
#endif
	bus_unregister(&vs_server_bus_type);
}

subsys_initcall(vs_core_server_init);
module_exit(vs_core_server_exit);

MODULE_DESCRIPTION("OKL4 Virtual Services Core Server Driver");
MODULE_AUTHOR("Open Kernel Labs, Inc");
