/*
 * HECI bus definitions
 *
 * Copyright (c) 2014-2015, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */
#ifndef _LINUX_HECI_CL_BUS_H
#define _LINUX_HECI_CL_BUS_H

#include <linux/device.h>
#include <linux/uuid.h>

/*typedef void (*heci_cl_event_cb_t)(struct heci_cl_device *device, u32 events,
	void *context);*/

struct heci_cl;
struct heci_cl_device;
struct heci_device;

#define	HECI_CL_NAME_SIZE	32

struct heci_cl_device_id {
	char name[MEI_CL_NAME_SIZE];
	kernel_ulong_t driver_info;
};

/**
 * struct heci_cl_dev_ops - HECI CL device ops
 * This structure allows ME host clients to implement technology
 * specific operations.
 *
 * @enable: Enable an HECI CL device. Some devices require specific
 *	HECI commands to initialize completely.
 * @disable: Disable an HECI CL device.
 * @send: Tx hook for the device. This allows ME host clients to trap
 *	the device driver buffers before actually physically
 *	pushing it to the ME.
 * @recv: Rx hook for the device. This allows ME host clients to trap the
 *	ME buffers before forwarding them to the device driver.
 */
struct heci_cl_dev_ops {
	int (*enable)(struct heci_cl_device *device);
	int (*disable)(struct heci_cl_device *device);
	int (*send)(struct heci_cl_device *device, u8 *buf, size_t length);
	int (*recv)(struct heci_cl_device *device, u8 *buf, size_t length);
};

struct heci_cl_device *heci_bus_add_device(struct heci_device *dev,
	uuid_le uuid, char *name, struct heci_cl_dev_ops *ops);
void heci_bus_remove_device(struct heci_cl_device *device);

/**
 * struct heci_cl_device - HECI device handle
 * An heci_cl_device pointer is returned from heci_add_device()
 * and links HECI bus clients to their actual ME host client pointer.
 * Drivers for HECI devices will get an heci_cl_device pointer
 * when being probed and shall use it for doing ME bus I/O.
 *
 * @dev: linux driver model device pointer
 * @uuid: me client uuid
 * @cl: heci client
 * @ops: ME transport ops
 * @event_cb: Drivers register this callback to get asynchronous ME
 *	events (e.g. Rx buffer pending) notifications.
 * @events: Events bitmask sent to the driver.
 * @priv_data: client private data
 */
struct heci_cl_device {
	struct device dev;
	/*struct heci_cl *cl;*/
	struct heci_device	*heci_dev;
	struct heci_me_client	*fw_client;	/* For easy reference */
	struct list_head	device_link;
	const struct heci_cl_dev_ops *ops;
	struct work_struct event_work;
	void (*event_cb)(struct heci_cl_device *device, u32 events,
		void *context);
	void *event_context;
	unsigned long events;
	void *priv_data;
};

struct heci_cl_driver {
	struct device_driver driver;
	const char *name;
	const struct heci_cl_device_id *id_table;
	int (*probe)(struct heci_cl_device *dev,
		const struct heci_cl_device_id *id);
	int (*remove)(struct heci_cl_device *dev);
};

int __heci_cl_driver_register(struct heci_cl_driver *driver,
	struct module *owner);
#define heci_cl_driver_register(driver)             \
	__heci_cl_driver_register(driver, THIS_MODULE)

void heci_cl_driver_unregister(struct heci_cl_driver *driver);
int heci_register_event_cb(struct heci_cl_device *device,
	void (*read_cb)(struct heci_cl_device *, u32, void *), void *context);

#define HECI_CL_EVENT_RX 0
#define HECI_CL_EVENT_TX 1

void *heci_cl_get_drvdata(const struct heci_cl_device *device);
void heci_cl_set_drvdata(struct heci_cl_device *device, void *data);

int heci_cl_enable_device(struct heci_cl_device *device);
int heci_cl_disable_device(struct heci_cl_device *device);

void heci_cl_bus_rx_event(struct heci_cl_device *device);
int heci_cl_bus_init(void);
void heci_cl_bus_exit(void);
int	heci_bus_new_client(struct heci_device *dev);
void	heci_remove_all_clients(struct heci_device *dev);
int	heci_cl_device_bind(struct heci_cl *cl);

#endif /* _LINUX_HECI_CL_BUS_H */
