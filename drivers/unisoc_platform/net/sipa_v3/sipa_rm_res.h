/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _SIPA_RM_RES_H_
#define _SIPA_RM_RES_H_

#include <linux/list.h>
#include <linux/sipa.h>
#include <linux/kfifo.h>
#include "sipa_rm.h"
#include "sipa_rm_peers_list.h"

/**
 * enum sipa_rm_res_state - resource state
 */
enum sipa_rm_res_state {
	SIPA_RM_RELEASED,
	SIPA_RM_REQUEST_IN_PROGRESS,
	SIPA_RM_GRANTED,
	SIPA_RM_RELEASE_IN_PROGRESS
};

/**
 * enum sipa_rm_res_state - resource state
 */
enum sipa_rm_res_pending_operation {
	SIPA_RM_PENDING_NONE,
	SIPA_RM_PENDING_REQUEST,
	SIPA_RM_PENDING_RELEASE,
};

/**
 * enum sipa_rm_res_type - SIPA resource manager resource type
 */
enum sipa_rm_res_type {
	SIPA_RM_PRODUCER,
	SIPA_RM_CONSUMER
};

/**
 * struct sipa_rm_notif_info - notification information
 *				of SIPA RM client
 * @reg_params: registration parameters
 * @explicit: registered explicitly by sipa_rm_register()
 * @link: link to the list of all registered clients information
 */
struct sipa_rm_notif_info {
	struct sipa_rm_register_params	reg_params;
	bool				explicit;
	struct list_head		link;
};

/**
 * struct sipa_rm_resource - SIPA RM resource
 * @name: name identifying resource
 * @type: type of resource (PRODUCER or CONSUMER)
 * @floor_voltage: minimum voltage level for operation
 * @max_bw: maximum bandwidth required for resource in Mbps
 * @state: state of the resource
 * @peers_list: list of the peers of the resource
 */
struct sipa_rm_resource {
	enum sipa_rm_res_id	name;
	enum sipa_rm_res_type	type;
	int ref_count;
	struct work_struct	work;
	enum sipa_voltage_level		floor_voltage;
	u32				max_bw;
	u32				needed_bw;
	enum sipa_rm_res_state	state;
	struct sipa_rm_peers_list	*peers_list;
	struct kfifo work_type_fifo;
};

struct sipa_rm_res_prod {
	struct sipa_rm_resource resource;
	struct completion request_prod_in_progress;
	void *user_data;
	int (*request_resource)(void *name);
	int (*release_resource)(void *name);
};

struct sipa_rm_res_cons {
	struct sipa_rm_resource	resource;
	struct list_head	event_listeners;
	int	pending_request;
	int	pending_release;
};

int sipa_rm_resource_create(struct sipa_rm_create_params *create_params,
			    struct sipa_rm_resource **resource);

int sipa_rm_resource_delete(struct sipa_rm_resource *resource);

int sipa_rm_resource_consumer_register(struct sipa_rm_res_cons *cons,
				       struct sipa_rm_register_params *reg_para,
				       bool explicit);

int sipa_rm_resource_consumer_deregister(struct sipa_rm_res_cons *cons,
					 struct sipa_rm_register_params *reg_para);

int sipa_rm_resource_add_dependency(struct sipa_rm_resource *resource,
				    struct sipa_rm_resource *depends_on,
				    bool userspace_dep);

int sipa_rm_resource_delete_dependency(struct sipa_rm_resource *res,
				       struct sipa_rm_resource *depends_on,
				       bool userspace_dep);

int sipa_rm_resource_producer_request(struct sipa_rm_res_prod *prod,
				      u32 needed_bw);

int sipa_rm_resource_producer_release(struct sipa_rm_res_prod *prod,
				      u32 needed_bw);

int sipa_rm_resource_consumer_request(struct sipa_rm_res_cons *cons);

int sipa_rm_resource_consumer_release(struct sipa_rm_res_cons *cons);

void sipa_rm_resource_producer_handle_cb(struct sipa_rm_res_prod *prod,
					 enum sipa_rm_event event);

void sipa_rm_resource_consumer_notify_clients(struct sipa_rm_res_cons *cons,
					      enum sipa_rm_event event);

int sipa_rm_resource_consumer_print_stat(struct sipa_rm_resource *resource,
					 char *buf,
					 int size);

int sipa_rm_resource_producer_request_work(struct sipa_rm_res_prod *prod);

int sipa_rm_resource_consumer_release_work(struct sipa_rm_res_prod *cons,
					   enum sipa_rm_res_state prev_state,
					   bool notify_completion);

const char *sipa_rm_res_str(enum sipa_rm_res_id resource_name);

#endif /* _SIPA_RM_RES_H_ */
