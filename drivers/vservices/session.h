/*
 * drivers/vservices/session.h
 *
 * Copyright (c) 2012-2018 General Dynamics
 * Copyright (c) 2014 Open Kernel Labs, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Definitions related to the vservices session bus and its client and server
 * session drivers. The interfaces in this file are implementation details of
 * the vServices framework and should not be used by transport or service
 * drivers.
 */

#ifndef _VSERVICES_SESSION_PRIV_H_
#define _VSERVICES_SESSION_PRIV_H_

/* Maximum number of sessions allowed */
#define VS_MAX_SESSIONS 64

#include "debug.h"

/* For use by the core server */
#define VS_SERVICE_AUTO_ALLOCATE_ID	0xffff
#define VS_SERVICE_ALREADY_RESET	1

/*
 * The upper bits of the service id are reserved for transport driver specific
 * use. The reserve bits are always zeroed out above the transport layer.
 */
#define VS_SERVICE_ID_TRANSPORT_BITS	4
#define VS_SERVICE_ID_TRANSPORT_OFFSET	12
#define VS_SERVICE_ID_TRANSPORT_MASK ((1 << VS_SERVICE_ID_TRANSPORT_BITS) - 1)
#define VS_SERVICE_ID_MASK \
	(~(VS_SERVICE_ID_TRANSPORT_MASK << VS_SERVICE_ID_TRANSPORT_OFFSET))

/* Number of bits needed to represent the service id range as a bitmap. */
#define VS_SERVICE_ID_BITMAP_BITS \
	(1 << ((sizeof(vs_service_id_t) * 8) - VS_SERVICE_ID_TRANSPORT_BITS))

/* High service ids are reserved for use by the transport drivers */
#define VS_SERVICE_ID_RESERVED(x) \
	((1 << VS_SERVICE_ID_TRANSPORT_OFFSET) - (x))

#define VS_SERVICE_ID_RESERVED_1	VS_SERVICE_ID_RESERVED(1)

/* Name of the session device symlink in service device sysfs directory */
#define VS_SESSION_SYMLINK_NAME		"session"

/* Name of the transport device symlink in session device sysfs directory */
#define VS_TRANSPORT_SYMLINK_NAME	"transport"

static inline unsigned int
vs_get_service_id_reserved_bits(vs_service_id_t service_id)
{
	return (service_id >> VS_SERVICE_ID_TRANSPORT_OFFSET) &
			VS_SERVICE_ID_TRANSPORT_MASK;
}

static inline vs_service_id_t vs_get_real_service_id(vs_service_id_t service_id)
{
	return service_id & VS_SERVICE_ID_MASK;
}

static inline void vs_set_service_id_reserved_bits(vs_service_id_t *service_id,
		unsigned int reserved_bits)
{
	*service_id &= ~(VS_SERVICE_ID_TRANSPORT_MASK <<
			VS_SERVICE_ID_TRANSPORT_OFFSET);
	*service_id |= (reserved_bits & VS_SERVICE_ID_TRANSPORT_MASK) <<
			VS_SERVICE_ID_TRANSPORT_OFFSET;
}

extern struct bus_type vs_session_bus_type;
extern struct kobject *vservices_root;
extern struct kobject *vservices_server_root;
extern struct kobject *vservices_client_root;

/**
 * struct vs_session_driver - Session driver
 * @driver: Linux device model driver structure
 * @service_bus: Pointer to either the server or client bus type
 * @is_server: True if this driver is for a server session, false if it is for
 * a client session
 * @service_added: Called when a non-core service is added.
 * @service_start: Called when a non-core service is started.
 * @service_local_reset: Called when an active non-core service driver becomes
 * inactive.
 * @service_removed: Called when a non-core service is removed.
 */
struct vs_session_driver {
	struct device_driver driver;
	struct bus_type *service_bus;
	bool is_server;

	/* These are all called with the core service state lock held. */
	int (*service_added)(struct vs_session_device *session,
			struct vs_service_device *service);
	int (*service_start)(struct vs_session_device *session,
			struct vs_service_device *service);
	int (*service_local_reset)(struct vs_session_device *session,
			struct vs_service_device *service);
	int (*service_removed)(struct vs_session_device *session,
			struct vs_service_device *service);
};

#define to_vs_session_driver(drv) \
	container_of(drv, struct vs_session_driver, driver)

/* Service lookup */
extern struct vs_service_device * vs_session_get_service(
		struct vs_session_device *session,
		vs_service_id_t service_id);

/* Service creation & destruction */
extern struct vs_service_device *
vs_service_register(struct vs_session_device *session,
		struct vs_service_device *parent,
		vs_service_id_t service_id,
		const char *protocol,
		const char *name,
		const void *plat_data);

extern bool vs_service_start(struct vs_service_device *service);

extern int vs_service_delete(struct vs_service_device *service,
		struct vs_service_device *caller);

extern int vs_service_handle_delete(struct vs_service_device *service);

/* Service reset handling */
extern int vs_service_handle_reset(struct vs_session_device *session,
		vs_service_id_t service_id, bool disable);
extern int vs_service_enable(struct vs_service_device *service);

extern void vs_session_enable_noncore(struct vs_session_device *session);
extern void vs_session_disable_noncore(struct vs_session_device *session);
extern void vs_session_delete_noncore(struct vs_session_device *session);

/* Service bus driver management */
extern int vs_service_bus_probe(struct device *dev);
extern int vs_service_bus_remove(struct device *dev);
extern int vs_service_bus_uevent(struct device *dev,
		struct kobj_uevent_env *env);

#ifdef CONFIG_VSERVICES_CHAR_DEV

extern int vs_devio_init(void);
extern void vs_devio_exit(void);

extern struct vs_service_device *vs_service_lookup_by_devt(dev_t dev);

extern struct vs_service_driver vs_devio_server_driver;
extern struct vs_service_driver vs_devio_client_driver;

extern int vservices_cdev_major;

#else /* !CONFIG_VSERVICES_CHAR_DEV */

static inline int vs_devio_init(void)
{
	return 0;
}

static inline void vs_devio_exit(void)
{
}

#endif /* !CONFIG_VSERVICES_CHAR_DEV */

#endif /* _VSERVICES_SESSION_PRIV_H_ */
