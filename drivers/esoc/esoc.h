/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __ESOC_H__
#define __ESOC_H__

#include <linux/cdev.h>
#include <linux/completion.h>
#include <linux/esoc_ctrl.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/subsystem_notif.h>

#define ESOC_DEV_MAX		4
#define ESOC_NAME_LEN		20
#define ESOC_LINK_LEN		20

struct esoc_clink;
/**
 * struct esoc_eng: Engine of the esoc control link
 * @handle_clink_req: handle incoming esoc requests.
 * @handle_clink_evt: handle for esoc events.
 * @esoc_clink: pointer to esoc control link.
 */
struct esoc_eng {
	void (*handle_clink_req)(enum esoc_req req,
						struct esoc_eng *eng);
	void (*handle_clink_evt)(enum esoc_evt evt,
						struct esoc_eng *eng);
	struct esoc_clink *esoc_clink;
};

/**
 * struct esoc_clink: Representation of external esoc device
 * @name: Name of the external esoc.
 * @link_name: name of the physical link.
 * @parent: parent device.
 * @dev: device for userspace interface.
 * @id: id of the external device.
 * @owner: owner of the device.
 * @clink_ops: control operations for the control link
 * @req_eng: handle for request engine.
 * @cmd_eng: handle for command engine.
 * @clink_data: private data of esoc control link.
 * @compat_data: compat data of esoc driver.
 * @subsys_desc: descriptor for subsystem restart
 * @subsys_dev: ssr device handle.
 * @np: device tree node for esoc_clink.
 */
struct esoc_clink {
	const char *name;
	const char *link_name;
	struct device *parent;
	struct device dev;
	unsigned int id;
	struct module *owner;
	const struct esoc_clink_ops const *clink_ops;
	struct esoc_eng *req_eng;
	struct esoc_eng *cmd_eng;
	spinlock_t notify_lock;
	void *clink_data;
	void *compat_data;
	struct subsys_desc subsys;
	struct subsys_device *subsys_dev;
	struct device_node *np;
};

/**
 * struct esoc_clink_ops: Operations to control external soc
 * @cmd_exe: Execute control command
 * @get_status: Get current status, or response to previous command
 * @notify_esoc: notify external soc of events
 */
struct esoc_clink_ops {
	int (*cmd_exe)(enum esoc_cmd cmd, struct esoc_clink *dev);
	int (*get_status)(u32 *status, struct esoc_clink *dev);
	void (*notify)(enum esoc_notify notify, struct esoc_clink *dev);
};

/**
 * struct esoc_compat: Compatibility of esoc drivers.
 * @name: esoc link that driver is compatible with.
 * @data: driver data associated with esoc clink.
 */
struct esoc_compat {
	const char *name;
	void *data;
};

/**
 * struct esoc_drv: Driver for an esoc clink
 * @driver: drivers for esoc.
 * @owner: module owner of esoc driver.
 * @compat_table: compatible table for driver.
 * @compat_entries
 * @probe: probe function for esoc driver.
 */
struct esoc_drv {
	struct device_driver driver;
	struct module *owner;
	struct esoc_compat *compat_table;
	unsigned int compat_entries;
	int (*probe)(struct esoc_clink *esoc_clink);
};

#define to_esoc_clink(d) container_of(d, struct esoc_clink, dev)
#define to_esoc_drv(d) container_of(d, struct esoc_drv, driver)

extern struct bus_type esoc_bus_type;


/* Exported apis */
void esoc_dev_exit(void);
int esoc_dev_init(void);
void esoc_clink_unregister(struct esoc_clink *esoc_dev);
int esoc_clink_register(struct esoc_clink *esoc_dev);
struct esoc_clink *get_esoc_clink(int id);
struct esoc_clink *get_esoc_clink_by_node(struct device_node *node);
void put_esoc_clink(struct esoc_clink *esoc_clink);
void *get_esoc_clink_data(struct esoc_clink *esoc);
void set_esoc_clink_data(struct esoc_clink *esoc, void *data);
void esoc_clink_evt_notify(enum esoc_evt, struct esoc_clink *esoc_dev);
void esoc_clink_queue_request(enum esoc_req req, struct esoc_clink *esoc_dev);
void esoc_for_each_dev(void *data, int (*fn)(struct device *dev,
								void *data));
int esoc_clink_register_cmd_eng(struct esoc_clink *esoc_clink,
						struct esoc_eng *eng);
void esoc_clink_unregister_cmd_eng(struct esoc_clink *esoc_clink,
						struct esoc_eng *eng);
int esoc_clink_register_req_eng(struct esoc_clink *esoc_clink,
						struct esoc_eng *eng);
void esoc_clink_unregister_req_eng(struct esoc_clink *esoc_clink,
						struct esoc_eng *eng);
int esoc_drv_register(struct esoc_drv *driver);
void esoc_set_drv_data(struct esoc_clink *esoc_clink, void *data);
void *esoc_get_drv_data(struct esoc_clink *esoc_clink);
/* ssr operations */
int esoc_clink_register_ssr(struct esoc_clink *esoc_clink);
int esoc_clink_request_ssr(struct esoc_clink *esoc_clink);
void esoc_clink_unregister_ssr(struct esoc_clink *esoc_clink);
/* client notification */
#ifdef CONFIG_ESOC_CLIENT
void notify_esoc_clients(struct esoc_clink *esoc_clink, unsigned long evt);
#else
static inline void notify_esoc_clients(struct esoc_clink *esoc_clink,
							unsigned long evt)
{
	return;
}
#endif
bool esoc_req_eng_enabled(struct esoc_clink *esoc_clink);
bool esoc_cmd_eng_enabled(struct esoc_clink *esoc_clink);
#endif
