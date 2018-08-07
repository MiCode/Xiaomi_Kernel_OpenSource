/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __SDE_RECOVERY_MANAGER_H__
#define __SDE_RECOVERY_MANAGER_H__

#include <linux/list.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/kobject.h>
#include <drm/msm_drm.h>
#include <linux/slab.h>
#include <drm/drmP.h>



/* MSM Recovery Manager related definitions */

#define MAX_REC_NAME_LEN (16)
#define MAX_REC_UEVENT_LEN (64)
#define MAX_REC_ERR_SUPPORT (3)

/* MSM Recovery Manager Error Code */
#define SDE_SMMU_FAULT	111
#define SDE_UNDERRUN	222
#define SDE_VSYNC_MISS	333
/*
 * instance id of bridge chip is added to make error code
 * unique to individual bridge chip instance
 */
#define DBA_BRIDGE_CRITICAL_ERR	444

/**
 * struct recovery_mgr_info - Recovery manager information
 * @dev: drm device.
 * @rec_lock: mutex lock for synchronized access to recovery mgr data.
 * @event_list: list of reported events.
 * @client_list: list of registered clients.
 * @event_work: work for event handling.
 * @event_queue: Queue for scheduling the event work.
 * @num_of_clients: no. of clients registered.
 * @recovery_ongoing: status indicating execution of recovery thread.
 */
struct recovery_mgr_info {
	struct drm_device *dev;
	struct mutex rec_lock;
	struct list_head event_list;
	struct list_head client_list;
	struct work_struct event_work;
	struct workqueue_struct *event_queue;
	int num_of_clients;
	int sysfs_created;
	int recovery_ongoing;
};

/**
 * struct recovery_error_info - Error information
 * @reported_err_code: error reported for recovery.
 * @pre_err_code: list of errors to be recovered before reported_err_code.
 * @post_err_code: list of errors to be recovered after reported_err_code.
 */
struct recovery_error_info {
	int reported_err_code;
	int pre_err_code;
	int post_err_code;
};

/**
 * struct recovery_client_info - Client information
 * @name: name of the client.
 * @recovery_cb: recovery callback to recover the errors reported.
 * @err_supported: list of errors that can be detected by client.
 * @no_of_err: no. of errors supported by the client.
 * @handle: Opaque handle passed to client
 */
struct recovery_client_info {
	char name[MAX_REC_NAME_LEN];
	int (*recovery_cb)(int err_code,
		struct recovery_client_info *client_info);
	struct recovery_error_info
		err_supported[MAX_REC_ERR_SUPPORT];
	int no_of_err;
	void *pdata;
	void *handle;
};

/**
 * struct recovery_event_db - event database.
 * @err: error code that client reports.
 * @list: list pointer.
 */
struct recovery_event_db {
	int err;
	struct list_head list;
};

/**
 * struct recovery_client_db - client database.
 * @client_info: information that client registers.
 * @list: list pointer.
 */
struct recovery_client_db {
	struct recovery_client_info client_info;
	struct list_head list;
};

int sde_recovery_set_events(int err);
int sde_recovery_client_register(struct recovery_client_info *client);
int sde_recovery_client_unregister(void *handle);
int sde_init_recovery_mgr(struct drm_device *dev);


#endif /* __SDE_RECOVERY_MANAGER_H__ */
