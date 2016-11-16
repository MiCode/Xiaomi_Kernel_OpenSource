/* Copyright (c) 2013-2016, The Linux Foundation. All rights reserved.
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

#ifndef _IPA_RM_I_H_
#define _IPA_RM_I_H_

#include <linux/workqueue.h>
#include <linux/ipa.h>
#include "ipa_rm_resource.h"
#include "ipa_common_i.h"

#define IPA_RM_DRV_NAME "ipa_rm"

#define IPA_RM_DBG_LOW(fmt, args...) \
	do { \
		pr_debug(IPA_RM_DRV_NAME " %s:%d " fmt, __func__, __LINE__, \
			## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			IPA_RM_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)
#define IPA_RM_DBG(fmt, args...) \
	do { \
		pr_debug(IPA_RM_DRV_NAME " %s:%d " fmt, __func__, __LINE__, \
			## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
			IPA_RM_DRV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			IPA_RM_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPA_RM_ERR(fmt, args...) \
	do { \
		pr_err(IPA_RM_DRV_NAME " %s:%d " fmt, __func__, __LINE__, \
			## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
			IPA_RM_DRV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			IPA_RM_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPA_RM_RESOURCE_CONS_MAX \
	(IPA_RM_RESOURCE_MAX - IPA_RM_RESOURCE_PROD_MAX)
#define IPA_RM_RESORCE_IS_PROD(x) \
	(x >= IPA_RM_RESOURCE_PROD && x < IPA_RM_RESOURCE_PROD_MAX)
#define IPA_RM_RESORCE_IS_CONS(x) \
	(x >= IPA_RM_RESOURCE_PROD_MAX && x < IPA_RM_RESOURCE_MAX)
#define IPA_RM_INDEX_INVALID	(-1)
#define IPA_RM_RELEASE_DELAY_IN_MSEC 1000

int ipa_rm_prod_index(enum ipa_rm_resource_name resource_name);
int ipa_rm_cons_index(enum ipa_rm_resource_name resource_name);

/**
 * struct ipa_rm_delayed_release_work_type - IPA RM delayed resource release
 *				work type
 * @delayed_work: work struct
 * @ipa_rm_resource_name: name of the resource on which this work should be done
 * @needed_bw: bandwidth required for resource in Mbps
 * @dec_usage_count: decrease usage count on release ?
 */
struct ipa_rm_delayed_release_work_type {
	struct delayed_work		work;
	enum ipa_rm_resource_name	resource_name;
	u32				needed_bw;
	bool				dec_usage_count;

};

/**
 * enum ipa_rm_wq_cmd - workqueue commands
 */
enum ipa_rm_wq_cmd {
	IPA_RM_WQ_NOTIFY_PROD,
	IPA_RM_WQ_NOTIFY_CONS,
	IPA_RM_WQ_RESOURCE_CB
};

/**
 * struct ipa_rm_wq_work_type - IPA RM worqueue specific
 *				work type
 * @work: work struct
 * @wq_cmd: command that should be processed in workqueue context
 * @resource_name: name of the resource on which this work
 *			should be done
 * @dep_graph: data structure to search for resource if exists
 * @event: event to notify
 * @notify_registered_only: notify only clients registered by
 *	ipa_rm_register()
 */
struct ipa_rm_wq_work_type {
	struct work_struct		work;
	enum ipa_rm_wq_cmd		wq_cmd;
	enum ipa_rm_resource_name	resource_name;
	enum ipa_rm_event		event;
	bool				notify_registered_only;
};

/**
 * struct ipa_rm_wq_suspend_resume_work_type - IPA RM worqueue resume or
 *				suspend work type
 * @work: work struct
 * @resource_name: name of the resource on which this work
 *			should be done
 * @prev_state:
 * @needed_bw:
 */
struct ipa_rm_wq_suspend_resume_work_type {
	struct work_struct		work;
	enum ipa_rm_resource_name	resource_name;
	enum ipa_rm_resource_state	prev_state;
	u32				needed_bw;
	bool				inc_usage_count;

};

int ipa_rm_wq_send_cmd(enum ipa_rm_wq_cmd wq_cmd,
		enum ipa_rm_resource_name resource_name,
		enum ipa_rm_event event,
		bool notify_registered_only);

int ipa_rm_wq_send_resume_cmd(enum ipa_rm_resource_name resource_name,
		enum ipa_rm_resource_state prev_state,
		u32 needed_bw,
		bool inc_usage_count);

int ipa_rm_wq_send_suspend_cmd(enum ipa_rm_resource_name resource_name,
		enum ipa_rm_resource_state prev_state,
		u32 needed_bw);

int ipa_rm_initialize(void);

int ipa_rm_stat(char *buf, int size);

const char *ipa_rm_resource_str(enum ipa_rm_resource_name resource_name);

void ipa_rm_perf_profile_change(enum ipa_rm_resource_name resource_name);

int ipa_rm_request_resource_with_timer(enum ipa_rm_resource_name resource_name);

void delayed_release_work_func(struct work_struct *work);

int ipa_rm_add_dependency_from_ioctl(enum ipa_rm_resource_name resource_name,
	enum ipa_rm_resource_name depends_on_name);

int ipa_rm_delete_dependency_from_ioctl(enum ipa_rm_resource_name resource_name,
	enum ipa_rm_resource_name depends_on_name);

void ipa_rm_exit(void);

#endif /* _IPA_RM_I_H_ */
