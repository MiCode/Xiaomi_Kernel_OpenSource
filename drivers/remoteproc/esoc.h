/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2013-2015, 2017-2018, 2020-2021, The Linux Foundation. All rights reserved.
 */
#ifndef __ESOC_H__
#define __ESOC_H__

#include <linux/cdev.h>
#include <linux/completion.h>
#include <linux/esoc_ctrl.h>
#include <linux/esoc_client.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/remoteproc.h>
#include <linux/ipc_logging.h>

#include "qcom_common.h"

#define ESOC_MDM_IPC_PAGES	10

extern void *ipc_log;

#define esoc_mdm_log(__msg, ...) ipc_log_string(ipc_log, "[%s]: "__msg, __func__, ##__VA_ARGS__)

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
	void (*handle_clink_req)(enum esoc_req req, struct esoc_eng *eng);
	void (*handle_clink_evt)(enum esoc_evt evt, struct esoc_eng *eng);
	struct esoc_clink *esoc_clink;
};

/**
 * struct esoc_clink: Representation of external esoc device
 * @name: Name of the external esoc.
 * @link_name: name of the physical link.
 * @link_info: additional info about the physical link.
 * @parent: parent device.
 * @dev: device for userspace interface.
 * @pdev: platform device to interface with SSR driver.
 * @id: id of the external device.
 * @owner: owner of the device.
 * @clink_ops: control operations for the control link
 * @req_eng: handle for request engine.
 * @cmd_eng: handle for command engine.
 * @clink_data: private data of esoc control link.
 * @compat_data: compat data of esoc driver.
 * @rproc: pointer to the esoc remoteproc struct
 * @ops: esoc driver remoteproc ops
 * @subsys_desc: descriptor for subsystem restart
 * @subsys_dev: ssr device handle.
 * @np: device tree node for esoc_clink.
 * @auto_boot: boots independently.
 * @primary: primary esoc controls(reset/poweroff) all secondary
 *	 esocs, but not	otherway around.
 * @statusline_not_a_powersource: True if status line to esoc is not a
 *				power source.
 * @userspace_handle_shutdown: True if user space handles shutdown requests.
 * @ssctl_id: SSCTL id for a subsystem.
 * @sysmon_name: Sysmon name for external soc
 * @dbg_dir: Debugfs entry associated to this clink
 */
struct esoc_clink {
	const char *name;
	const char *link_name;
	const char *link_info;
	struct device *parent;
	struct device dev;
	struct platform_device *pdev;
	unsigned int id;
	struct module *owner;
	const struct esoc_clink_ops *clink_ops;
	struct esoc_eng *req_eng;
	struct esoc_eng *cmd_eng;
	spinlock_t notify_lock;
	void *clink_data;
	void *compat_data;
	struct rproc *rproc;
	struct rproc_ops ops;
	struct qcom_sysmon *rproc_sysmon;
	struct device_node *np;
	bool auto_boot;
	bool primary;
	bool statusline_not_a_powersource;
	bool userspace_handle_shutdown;
	struct esoc_client_hook *client_hook[ESOC_MAX_HOOKS];
	int ssctl_id;
	char *sysmon_name;
	struct dentry *dbg_dir;
};

/**
 * struct esoc_clink_ops: Operations to control external soc
 * @cmd_exe: Execute control command
 * @get_status: Get current status, or response to previous command
 * @get_err_fatal: Get status of err fatal signal
 * @notify_esoc: notify external soc of events
 */
struct esoc_clink_ops {
	int (*cmd_exe)(enum esoc_cmd cmd, struct esoc_clink *dev);
	void (*get_status)(u32 *status, struct esoc_clink *dev);
	void (*get_err_fatal)(u32 *status, struct esoc_clink *dev);
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
	int (*probe)(struct esoc_clink *esoc_clink, struct esoc_drv *drv);
	int (*remove)(struct esoc_clink *esoc_clink, struct esoc_drv *drv);
};

#define to_esoc_clink(d) container_of(d, struct esoc_clink, dev)
#define to_esoc_drv(d) container_of(d, struct esoc_drv, driver)

typedef int (*esoc_func_t)(struct device *dev, void *data);

extern struct bus_type esoc_bus_type;


/* Exported apis */
void esoc_dev_exit(void);
int esoc_dev_init(void);
void mdm_drv_exit(void);
int mdm_drv_init(void);
void esoc_bus_exit(void);
int esoc_bus_init(void);
void esoc_clink_unregister(struct esoc_clink *esoc_dev);
int esoc_clink_register(struct esoc_clink *esoc_dev);
struct esoc_clink *get_esoc_clink(int id);
struct esoc_clink *get_esoc_clink_by_node(struct device_node *node);
void put_esoc_clink(struct esoc_clink *esoc_clink);
void *get_esoc_clink_data(struct esoc_clink *esoc);
void set_esoc_clink_data(struct esoc_clink *esoc, void *data);
void esoc_clink_evt_notify(enum esoc_evt, struct esoc_clink *esoc_dev);
void esoc_clink_queue_request(enum esoc_req req, struct esoc_clink *esoc_dev);
void esoc_for_each_dev(void *data, esoc_func_t fn);
int esoc_clink_register_cmd_eng(struct esoc_clink *esoc_clink, struct esoc_eng *eng);
void esoc_clink_unregister_cmd_eng(struct esoc_clink *esoc_clink, struct esoc_eng *eng);
int esoc_clink_register_req_eng(struct esoc_clink *esoc_clink, struct esoc_eng *eng);
void esoc_clink_unregister_req_eng(struct esoc_clink *esoc_clink, struct esoc_eng *eng);
int esoc_driver_register(struct esoc_drv *driver);
void esoc_driver_unregister(struct esoc_drv *driver);
void esoc_set_drv_data(struct esoc_clink *esoc_clink, void *data);
void *esoc_get_drv_data(struct esoc_clink *esoc_clink);
int esoc_clink_add_device(struct device *dev, void *dummy);
int esoc_clink_del_device(struct device *dev, void *dummy);
/* ssr operations */
int esoc_clink_register_rproc(struct esoc_clink *esoc_clink);
int esoc_clink_request_ssr(struct esoc_clink *esoc_clink);
void esoc_clink_unregister_rproc(struct esoc_clink *esoc_clink);
/* client notification */
void notify_esoc_clients(struct esoc_clink *esoc_clink, unsigned long evt);
bool esoc_req_eng_enabled(struct esoc_clink *esoc_clink);
bool esoc_cmd_eng_enabled(struct esoc_clink *esoc_clink);
#endif

/* Modem boot fail actions */
int esoc_set_boot_fail_action(struct esoc_clink *esoc_clink, u32 action);
int esoc_set_n_pon_tries(struct esoc_clink *esoc_clink, u32 n_tries);
