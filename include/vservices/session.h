/*
 * include/vservices/session.h
 *
 * Copyright (c) 2012-2018 General Dynamics
 * Copyright (c) 2014 Open Kernel Labs, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This file defines the device type for a vServices session attached to a
 * transport. This should only be used by transport drivers, the vServices
 * session code, and the inline transport-access functions defined in
 * vservices/service.h.
 *
 * Drivers for these devices are defined internally by the vServices
 * framework. Other drivers should not attach to these devices.
 */

#ifndef _VSERVICES_SESSION_H_
#define _VSERVICES_SESSION_H_

#include <linux/types.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/completion.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/idr.h>

#include <vservices/types.h>

struct vs_service_device;
struct vs_mbuf;

struct notifier_block;

/**
 * enum vs_notify_event_t - vService notifier events
 *
 * @VS_SESSION_NOTIFY_ADD: vService session added. Argument is a pointer to
 * the vs_session_device. This notification is sent after the session has been
 * added.
 *
 * @VS_SESSION_NOTIFY_REMOVE: vService session about to be removed. Argument is
 * a pointer to the vs_session_device. This notification is sent before the
 * session is removed.
 */
enum vs_notify_event_t {
	VS_SESSION_NOTIFY_ADD,
	VS_SESSION_NOTIFY_REMOVE,
};

/**
 * struct vs_session_device - Session device
 * @name: The unique human-readable name of this session.
 * @is_server: True if this session is a server, false if client
 * @transport: The transport device for this session
 * @session_num: Unique ID for this session. Used for sysfs
 * @session_lock: Mutex which protects any change to service presence or
 *     readiness
 * @core_service: The core service, if one has ever been registered. Once set,
 *     this must remain valid and unchanged until the session driver is
 *     removed. Writes are protected by the service_ids_lock.
 * @services: Dynamic array of the services on this session. Protected by
 *     service_ids_lock.
 * @alloc_service_ids: Size of the session services array
 * @service_ids_lock: Mutex protecting service array updates
 * @activation_work: work structure for handling session activation & reset
 * @activation_state: true if transport is currently active
 * @fatal_error_work: work structure for handling fatal session failures
 * @debug_mask: Debug level mask
 * @list: Entry in the global session list
 * @sysfs_entry: Kobject pointer pointing to session device in sysfs under
 *     sys/vservices
 * @dev: Device structure for the Linux device model
 */
struct vs_session_device {
	char *name;
	bool is_server;
	struct vs_transport *transport;
	int session_num;

	struct mutex session_lock;

	/*
	 * The service_idr maintains the list of currently allocated services
	 * on a session, and allows for recycling of service ids. The lock also
	 * protects core_service.
	 */
	struct idr service_idr;
	struct mutex service_idr_lock;
	struct vs_service_device *core_service;

	struct work_struct activation_work;
	atomic_t activation_state;

	struct work_struct fatal_error_work;

	unsigned long debug_mask;

	struct list_head list;
	struct kobject *sysfs_entry;

	struct device dev;
};

#define to_vs_session_device(d) \
	container_of(d, struct vs_session_device, dev)

extern struct vs_session_device *
vs_session_register(struct vs_transport *transport, struct device *parent,
		bool server, const char *transport_name);
extern void vs_session_start(struct vs_session_device *session);
extern void vs_session_unregister(struct vs_session_device *session);

extern int vs_session_handle_message(struct vs_session_device *session,
		struct vs_mbuf *mbuf, vs_service_id_t service_id);

extern void vs_session_quota_available(struct vs_session_device *session,
		vs_service_id_t service_id, unsigned count,
		bool send_tx_ready);

extern void vs_session_handle_notify(struct vs_session_device *session,
		unsigned long flags, vs_service_id_t service_id);

extern void vs_session_handle_reset(struct vs_session_device *session);
extern void vs_session_handle_activate(struct vs_session_device *session);

extern struct vs_service_device *
vs_server_create_service(struct vs_session_device *session,
		struct vs_service_device *parent, const char *name,
		const char *protocol, const void *plat_data);
extern int vs_server_destroy_service(struct vs_service_device *service,
		struct vs_service_device *parent);

extern void vs_session_register_notify(struct notifier_block *nb);
extern void vs_session_unregister_notify(struct notifier_block *nb);

extern int vs_session_unbind_driver(struct vs_service_device *service);

extern void vs_session_for_each_service(struct vs_session_device *session,
		void (*func)(struct vs_service_device *, void *), void *data);

extern struct mutex vs_session_lock;
extern int vs_session_for_each_locked(
		int (*fn)(struct vs_session_device *session, void *data),
		void *data);

static inline int vs_session_for_each(
		int (*fn)(struct vs_session_device *session, void *data),
		void *data)
{
	int r;
	mutex_lock(&vs_session_lock);
	r = vs_session_for_each_locked(fn, data);
	mutex_unlock(&vs_session_lock);
	return r;
}

#endif /* _VSERVICES_SESSION_H_ */
