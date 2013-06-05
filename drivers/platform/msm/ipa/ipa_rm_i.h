/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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
#include <mach/ipa.h>

#define IPA_RM_RESOURCE_CONS_MAX \
	(IPA_RM_RESOURCE_MAX - IPA_RM_RESOURCE_PROD_MAX)
#define IPA_RM_RESORCE_IS_PROD(x) \
	(x >= IPA_RM_RESOURCE_PROD && x < IPA_RM_RESOURCE_PROD_MAX)
#define IPA_RM_RESORCE_IS_CONS(x) \
	(x >= IPA_RM_RESOURCE_PROD_MAX && x < IPA_RM_RESOURCE_MAX)
#define IPA_RM_INDEX_INVALID	(-1)

int ipa_rm_prod_index(enum ipa_rm_resource_name resource_name);
int ipa_rm_cons_index(enum ipa_rm_resource_name resource_name);

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

int ipa_rm_wq_send_cmd(enum ipa_rm_wq_cmd wq_cmd,
		enum ipa_rm_resource_name resource_name,
		enum ipa_rm_event event,
		bool notify_registered_only);

int ipa_rm_initialize(void);

int ipa_rm_stat(char *buf, int size);

void ipa_rm_exit(void);

#endif /* _IPA_RM_I_H_ */
