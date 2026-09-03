// SPDX-License-Identifier: GPL-2.0-only
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

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sipa_rm: " fmt

#include <linux/list.h>
#include <linux/sipa.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/wait.h>

#include "sipa_rm_res.h"
#include "sipa_rm.h"
#include "sipa_rm_peers_list.h"
#include "sipa_rm_dep_graph.h"

/**
 * sipa_rm_prod_index() - producer name to producer index mapping
 * @resource_name: [in] resource name (should be of producer)
 *
 * Returns: resource index mapping, SIPA_RM_INDEX_INVALID
 *	in case provided resource name isn't contained
 *	in enum sipa_rm_res_id or is not of producers.
 */
int sipa_rm_prod_index(enum sipa_rm_res_id resource_name)
{
	int result = resource_name;

	switch (resource_name) {
	case SIPA_RM_RES_PROD_IPA:
	case SIPA_RM_RES_PROD_PAM_U3:
	case SIPA_RM_RES_PROD_PAM_WIFI:
	case SIPA_RM_RES_PROD_AP:
	case SIPA_RM_RES_PROD_CP:
		break;
	default:
		result = SIPA_RM_INDEX_INVALID;
		break;
	}

	return result;
}

/**
 * sipa_rm_cons_index() - consumer name to consumer index mapping
 * @resource_name: [in] resource name (should be of consumer)
 *
 * Returns: resource index mapping, SIPA_RM_INDEX_INVALID
 *	in case provided resource name isn't contained
 *	in enum sipa_rm_res_id or is not of consumers.
 */
int sipa_rm_cons_index(enum sipa_rm_res_id resource_name)
{
	int result = resource_name;

	switch (resource_name) {
	case SIPA_RM_RES_CONS_WWAN_UL:
	case SIPA_RM_RES_CONS_WWAN_DL:
	case SIPA_RM_RES_CONS_WIFI_UL:
	case SIPA_RM_RES_CONS_WIFI_DL:
	case SIPA_RM_RES_CONS_USB:
		break;
	default:
		result = SIPA_RM_INDEX_INVALID;
		break;
	}

	return result;
}

static bool
consumer_check_all_pord_granted(struct sipa_rm_resource *cons)
{
	struct sipa_rm_resource *producer;
	int list_size, peers_index;

	if (!cons->peers_list)
		return true;

	list_size = sipa_rm_peers_list_get_size(cons->peers_list);
	for (peers_index = 0;
	     peers_index < list_size;
	     peers_index++) {
		producer = sipa_rm_peers_list_get_res(peers_index,
						      cons->peers_list);
		if (producer) {
			if (producer->state != SIPA_RM_GRANTED)
				return false;
		}
	}

	return true;
}

int sipa_rm_resource_producer_release_work(struct sipa_rm_res_prod *producer)
{
	int driver_result;

	pr_debug("calling driver CB\n");
	driver_result = producer->release_resource(producer->user_data);
	pr_debug("driver CB returned with %d\n", driver_result);

	if (driver_result != 0 && driver_result != -EINPROGRESS)
		pr_err("driver CB returned error %d\n", driver_result);

	/* Always set  prod in SIPA_RM_RELEASED state, just ignore
	 * driver release result.
	 */
	producer->resource.state = SIPA_RM_RELEASED;

	return driver_result;
}

int sipa_rm_resource_producer_request_work(struct sipa_rm_res_prod *prod)
{
	int driver_result;

	pr_info("calling driver CB\n");
	driver_result = prod->request_resource(prod->user_data);
	pr_info("driver CB returned with %d\n", driver_result);

	return driver_result;
}

void sipa_rm_resource_consumer_do_release(struct sipa_rm_res_cons *cons)
{
	int peers_index;
	struct sipa_rm_resource *prod;
	struct sipa_rm_res_prod *res_prod;

	for (peers_index = 0;
	     peers_index < sipa_rm_peers_list_get_size(cons->resource.peers_list);
	     peers_index++) {
		prod = sipa_rm_peers_list_get_res(peers_index,
						  cons->resource.peers_list);
		if (!prod)
			continue;

		res_prod = (struct sipa_rm_res_prod *)prod;
		sipa_rm_resource_producer_release(res_prod,
						  cons->resource.max_bw);
	}

	cons->resource.state = SIPA_RM_RELEASED;

	pr_debug("%s state: %d\n",
		 sipa_rm_res_str(cons->resource.name),
		 cons->resource.state);

	sipa_rm_wq_send_cmd(SIPA_RM_WQ_NOTIFY_CONS,
			    cons->resource.name,
			    SIPA_RM_EVT_RELEASED);
}

int sipa_rm_resource_consumer_do_request(struct sipa_rm_res_cons *cons)
{
	int result = 0;
	int prod_result;
	int peers_index;
	struct sipa_rm_resource *prod;

	cons->pending_request = 0;
	for (peers_index = 0;
	     peers_index < sipa_rm_peers_list_get_size(cons->resource.peers_list);
	     peers_index++) {
		prod = sipa_rm_peers_list_get_res(peers_index,
						  cons->resource.peers_list);
		if (prod) {
			cons->pending_request++;
			prod_result =
			sipa_rm_resource_producer_request((struct sipa_rm_res_prod *)prod,
							  cons->resource.max_bw);
			if (prod_result == -EINPROGRESS) {
				result = -EINPROGRESS;
			} else {
				cons->pending_request--;
				if (prod_result != 0)
					return prod_result;
			}
		}
	}

	return result;
}

void sipa_rm_resource_consumer_enter_granted(struct sipa_rm_res_cons *consumer)
{
	consumer->resource.state = SIPA_RM_GRANTED;

	sipa_rm_wq_send_cmd(SIPA_RM_WQ_NOTIFY_CONS,
			    consumer->resource.name,
			    SIPA_RM_EVT_GRANTED);
}

int sipa_rm_resource_producer_request(struct sipa_rm_res_prod *producer,
				      u32 cons_needed_bw)
{
	int result = 0;
	bool request_driver = false;
	enum sipa_rm_res_state prev_state;

	pr_debug("%s state: %d\n",
		 sipa_rm_res_str(producer->resource.name),
		 producer->resource.state);

	producer->resource.ref_count++;
	if (producer->resource.ref_count == 1)
		producer->resource.needed_bw += cons_needed_bw;

	prev_state = producer->resource.state;
	switch (producer->resource.state) {
	case SIPA_RM_RELEASED:
		request_driver = true;
		break;
	case SIPA_RM_RELEASE_IN_PROGRESS:
		producer->resource.state = SIPA_RM_REQUEST_IN_PROGRESS;
		result = -EINPROGRESS;
		break;
	case SIPA_RM_GRANTED:
		break;
	case SIPA_RM_REQUEST_IN_PROGRESS:
		result = -EINPROGRESS;
		break;
	default:
		result = -EPERM;
		goto bail;
	}

	if (request_driver) {
		result = sipa_rm_resource_producer_request_work(producer);

		switch (result) {
		case 0:
			producer->resource.state = SIPA_RM_GRANTED;
			break;
		case -EINPROGRESS:
			producer->resource.state = SIPA_RM_REQUEST_IN_PROGRESS;
			break;
		default:
			producer->resource.state = prev_state;
			producer->resource.needed_bw -= cons_needed_bw;
			pr_err("%s request driver fail: %dn",
			       sipa_rm_res_str(producer->resource.name),
			       result);
			break;
		}
	}

bail:
	if (prev_state != producer->resource.state)
		pr_info("%s state changed %d->%d\n",
			sipa_rm_res_str(producer->resource.name),
			prev_state,
			producer->resource.state);

	pr_info("EXIT with %d\n", result);

	return result;
}

int sipa_rm_resource_producer_release(struct sipa_rm_res_prod *prod,
				      u32 prod_needed_bw)
{
	int result = 0;
	bool release_driver = false;
	enum sipa_rm_res_state save_state;

	pr_debug("%s state: %d\n",
		 sipa_rm_res_str(prod->resource.name),
		 prod->resource.state);

	if (prod->resource.ref_count == 1)
		prod->resource.needed_bw -= prod_needed_bw;
	if (prod->resource.ref_count > 0)
		prod->resource.ref_count--;

	save_state = prod->resource.state;
	switch (prod->resource.state) {
	case SIPA_RM_RELEASED:
		goto bail;
	case SIPA_RM_GRANTED:
		if (prod->resource.ref_count == 0)
			release_driver = true;
		break;
	case SIPA_RM_REQUEST_IN_PROGRESS:
		if (prod->resource.ref_count == 0) {
			prod->resource.state = SIPA_RM_RELEASE_IN_PROGRESS;
			release_driver = true;
		}
		break;
	case SIPA_RM_RELEASE_IN_PROGRESS:
		result = -EINPROGRESS;
		goto bail;
	default:
		result = -EPERM;
		goto bail;
	}

	if (release_driver)
		sipa_rm_resource_producer_release_work(prod);

bail:
	if (save_state != prod->resource.state)
		pr_info("%s state changed %d->%d\n",
			sipa_rm_res_str(prod->resource.name),
			save_state,
			prod->resource.state);

	return result;
}

/**
 * sipa_rm_resource_consumer_notify_clients() - notify
 *	all registered clients of given producer
 * @producer: producer
 * @event: event to notify
 * @notify_registered_only: notify only clients registered by
 *	sipa_rm_register()
 */
void sipa_rm_resource_consumer_notify_clients(struct sipa_rm_res_cons *cons,
					      enum sipa_rm_event event)
{
	struct sipa_rm_notif_info *reg_info;

	pr_debug("%s event: %d\n",
		 sipa_rm_res_str(cons->resource.name),
		 event);

	list_for_each_entry(reg_info, &cons->event_listeners, link) {
		pr_debug("Notifying %s event: %d\n",
			 sipa_rm_res_str(cons->resource.name),
			 event);
		reg_info->reg_params.notify_cb(reg_info->reg_params.user_data,
					       event,
					       0);
		pr_debug("back from client CB\n");
	}
}

static int
sipa_rm_resource_producer_create(struct sipa_rm_resource **resource,
				 struct sipa_rm_res_prod **producer,
				 struct sipa_rm_create_params *create_params,
				 int *max_peers)
{
	*producer = kzalloc(sizeof(**producer), GFP_KERNEL);
	if (!*producer)
		return -ENOMEM;

	(*producer)->request_resource = create_params->request_resource;
	(*producer)->release_resource = create_params->release_resource;
	(*producer)->user_data = create_params->reg_params.user_data;
	init_completion(&(*producer)->request_prod_in_progress);

	*resource = (struct sipa_rm_resource *)*producer;
	(*resource)->type = SIPA_RM_PRODUCER;
	*max_peers = SIPA_RM_RES_CONS_MAX;

	return 0;
}

static void sipa_rm_resource_consumer_delete(struct sipa_rm_res_cons *cons)
{
	struct sipa_rm_notif_info *reg_info;
	struct list_head *pos, *q;

	sipa_rm_resource_consumer_release(cons);
	list_for_each_safe(pos, q, &cons->event_listeners) {
		reg_info = list_entry(pos,
				      struct sipa_rm_notif_info,
				      link);
		list_del(pos);
		kfree(reg_info);
	}
}

static int
sipa_rm_resource_consumer_create(struct sipa_rm_resource **resource,
				 struct sipa_rm_res_cons **consumer,
				 struct sipa_rm_create_params *create_params,
				 int *max_peers)
{
	int result;

	*consumer = kzalloc(sizeof(**consumer), GFP_KERNEL);
	if (!*consumer)
		return -ENOMEM;

	*resource = (struct sipa_rm_resource *)*consumer;
	(*resource)->type = SIPA_RM_CONSUMER;
	INIT_LIST_HEAD(&(*consumer)->event_listeners);
	(*consumer)->pending_request = 0;
	(*consumer)->pending_release = 0;
	*max_peers = SIPA_RM_RES_PROD_MAX;

	if (!create_params->reg_params.notify_cb)
		return 0;

	result = sipa_rm_resource_consumer_register(*consumer,
						    &create_params->reg_params,
						    false);
	if (result) {
		pr_err("sipa_rm_resource_consumer_register() failed\n");
		kfree(*consumer);
		return result;
	}

	return 0;
}

/**
 * sipa_rm_resource_create() - creates resource
 * @create_params: [in] parameters needed
 *			for resource initialization with SIPA RM
 * @resource: [out] created resource
 *
 * Returns: 0 on success, negative on failure
 */
int sipa_rm_resource_create(struct sipa_rm_create_params *create_params,
			    struct sipa_rm_resource **resource)
{
	struct sipa_rm_res_cons *consumer = NULL;
	struct sipa_rm_res_prod *producer;
	int max_peers;
	int result = 0;

	if (!create_params)
		return -EINVAL;

	if (SIPA_RM_RESORCE_IS_PROD(create_params->name)) {
		result = sipa_rm_resource_producer_create(resource,
							  &producer,
							  create_params,
							  &max_peers);
		if (result) {
			pr_err("sipa_rm_resource_producer_create fail\n");
			return result;
		}
	} else if (SIPA_RM_RESORCE_IS_CONS(create_params->name)) {
		result = sipa_rm_resource_consumer_create(resource,
							  &consumer,
							  create_params,
							  &max_peers);
		if (result) {
			pr_err("sipa_rm_resource_consumer_create fail\n");
			return result;
		}
	} else {
		pr_err("invalied resource\n");
		return -EPERM;
	}

	result = sipa_rm_peers_list_create(max_peers,
					   &((*resource)->peers_list));
	if (result) {
		pr_err("sipa_rm_peers_list_create failed\n");
		if ((*resource)->type == SIPA_RM_CONSUMER)
			sipa_rm_resource_consumer_delete(consumer);
		kfree(*resource);
		return result;
	}

	result = kfifo_alloc(&(*resource)->work_type_fifo,
			     sizeof(struct sipa_rm_wq_work_type) * 16,
			     GFP_KERNEL);
	if (result) {
		pr_err("sipa_rm_res %s alloc kfifo fail\n",
		       sipa_rm_res_str((*resource)->name));
		if ((*resource)->type == SIPA_RM_CONSUMER)
			sipa_rm_resource_consumer_delete(consumer);
		kfree(*resource);
		return result;
	}

	(*resource)->name = create_params->name;
	(*resource)->ref_count = 0;
	(*resource)->floor_voltage = create_params->floor_voltage;
	(*resource)->state = SIPA_RM_RELEASED;

	return result;
}

static void prod_delete_dep(struct sipa_rm_resource *resource)
{
	struct sipa_rm_resource *consumer;
	int list_size;
	bool userspace_dep;
	int peers_index;

	if (!resource->peers_list)
		return;

	list_size = sipa_rm_peers_list_get_size(resource->peers_list);

	for (peers_index = 0; peers_index < list_size; peers_index++) {
		consumer = sipa_rm_peers_list_get_res(peers_index,
						      resource->peers_list);
		if (!consumer)
			continue;

		userspace_dep =
		sipa_rm_peers_list_get_userspace_dep(peers_index,
						     resource->peers_list);
		sipa_rm_resource_delete_dependency(consumer,
						   resource,
						   userspace_dep);
	}
}

static void cons_delete_dep(struct sipa_rm_resource *resource)
{
	struct sipa_rm_resource *producer;
	int list_size;
	bool userspace_dep;
	int peers_index;

	if (!resource->peers_list)
		return;

	list_size = sipa_rm_peers_list_get_size(resource->peers_list);

	for (peers_index = 0; peers_index < list_size; peers_index++) {
		producer = sipa_rm_peers_list_get_res(peers_index,
						      resource->peers_list);
		if (!producer)
			continue;

		userspace_dep = sipa_rm_peers_list_get_userspace_dep(peers_index,
								     resource->peers_list);
		sipa_rm_resource_delete_dependency(resource,
						   producer,
						   userspace_dep);
	}
}

/**
 * sipa_rm_resource_delete() - deletes resource
 * @resource: [in] resource
 *			for resource initialization with SIPA RM
 *
 * Returns: 0 on success, negative on failure
 */
int sipa_rm_resource_delete(struct sipa_rm_resource *resource)
{
	int result = 0;
	struct sipa_rm_res_cons *cons;

	if (!resource) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	if (resource->type == SIPA_RM_PRODUCER) {
		prod_delete_dep(resource);
	} else if (resource->type == SIPA_RM_CONSUMER) {
		cons = (struct sipa_rm_res_cons *)resource;
		cons_delete_dep(resource);
		sipa_rm_resource_consumer_delete(cons);
	}
	sipa_rm_peers_list_delete(resource->peers_list);
	kfree(resource);
	return result;
}

/**
 * sipa_rm_resource_register() - register resource
 * @resource: [in] resource
 * @reg_params: [in] registration parameters
 * @explicit: [in] registered explicitly by sipa_rm_register()
 *
 * Returns: 0 on success, negative on failure
 *
 * Producer resource is expected for this call.
 */
int sipa_rm_resource_consumer_register(struct sipa_rm_res_cons *cons,
				       struct sipa_rm_register_params *reg_para,
				       bool explicit)
{
	struct sipa_rm_notif_info *reg_info;
	struct list_head *pos;

	if (!cons || !reg_para) {
		pr_err("invalid params\n");
		return -EPERM;
	}

	list_for_each(pos, &cons->event_listeners) {
		reg_info = list_entry(pos,
				      struct sipa_rm_notif_info,
				      link);
		if (reg_info->reg_params.notify_cb ==
		    reg_para->notify_cb &&
		    reg_info->reg_params.user_data ==
		    reg_para->user_data) {
			pr_err("already registered\n");
			return -EPERM;
		}
	}

	reg_info = kzalloc(sizeof(*reg_info), GFP_ATOMIC);
	if (!reg_info)
		return -ENOMEM;

	reg_info->reg_params.user_data = reg_para->user_data;
	reg_info->reg_params.notify_cb = reg_para->notify_cb;
	reg_info->explicit = explicit;
	INIT_LIST_HEAD(&reg_info->link);
	list_add(&reg_info->link, &cons->event_listeners);

	return 0;
}

/**
 * sipa_rm_resource_consumer_deregister() - register resource
 * @resource: [in] resource
 * @reg_params: [in] registration parameters
 *
 * Returns: 0 on success, negative on failure
 *
 * Producer resource is expected for this call.
 * This function deleted only single instance of
 * registration info.
 */
int
sipa_rm_resource_consumer_deregister(struct sipa_rm_res_cons *consumer,
				     struct sipa_rm_register_params *reg_params)
{
	struct sipa_rm_notif_info *reg_info;
	struct list_head *pos, *q;

	if (!consumer || !reg_params) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	list_for_each_safe(pos, q, &consumer->event_listeners) {
		reg_info = list_entry(pos,
				      struct sipa_rm_notif_info,
				      link);
		if (reg_info->reg_params.notify_cb ==
		    reg_params->notify_cb &&
		    reg_info->reg_params.user_data ==
		    reg_params->user_data) {
			list_del(pos);
			kfree(reg_info);
			return 0;
		}
	}

	return 0;
}

/**
 * sipa_rm_resource_add_dependency() - add dependency between two
 *				given resources
 * @cons: [in] cons resource
 * @prod: [in] depends_on resource
 *
 * Returns: 0 on success, negative on failure
 */
int sipa_rm_resource_add_dependency(struct sipa_rm_resource *cons,
				    struct sipa_rm_resource *prod,
				    bool userspace_dep)
{
	int result = 0;
	int prod_result;
	bool add_dep_by_userspace;

	if (!cons || !prod) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	if (sipa_rm_peers_list_check_dependency(cons->peers_list,
						cons->name,
						prod->peers_list,
						prod->name,
						&add_dep_by_userspace)) {
		pr_err("dependency already exists, added by %s\n",
		       add_dep_by_userspace ? "userspace" : "kernel");
		return -EEXIST;
	}

	sipa_rm_peers_list_add_peer(cons->peers_list, prod,
				    userspace_dep);
	sipa_rm_peers_list_add_peer(prod->peers_list, cons,
				    userspace_dep);
	pr_info("%s state: %d\n", sipa_rm_res_str(cons->name),
		cons->state);

	prod->needed_bw += cons->max_bw;

	switch (cons->state) {
	case SIPA_RM_RELEASED:
	case SIPA_RM_RELEASE_IN_PROGRESS:
		break;
	case SIPA_RM_GRANTED:
	case SIPA_RM_REQUEST_IN_PROGRESS: {
		enum sipa_rm_res_state prev_state = cons->state;

		cons->state = SIPA_RM_REQUEST_IN_PROGRESS;
		((struct sipa_rm_res_cons *)
		 cons)->pending_request++;
		prod_result = sipa_rm_resource_producer_request((struct sipa_rm_res_prod *)prod,
								cons->max_bw);
		if (prod_result != -EINPROGRESS) {
			cons->state = prev_state;
			((struct sipa_rm_res_cons *)
			 cons)->pending_request--;
		}
		result = prod_result;
		break;
	}
	default:
		pr_err("invalid state\n");
		result = -EPERM;
		break;
	}

	pr_debug("%s new state: %d\n", sipa_rm_res_str(cons->name),
		 cons->state);

	return result;
}

/**
 * sipa_rm_resource_delete_dependency() - add dependency between two
 *				given resources
 * @consumer: [in] consumer resource
 * @producer: [in] depends_on resource
 *
 * Returns: 0 on success, negative on failure
 * In case the resource state was changed, a notification
 * will be sent to the RM client
 */
int sipa_rm_resource_delete_dependency(struct sipa_rm_resource *consumer,
				       struct sipa_rm_resource *producer,
				       bool userspace_dep)
{
	bool check_cons_state = false;
	bool release_producer = false;
	bool add_dep_by_userspace;
	enum sipa_rm_res_state prev_state;
	struct sipa_rm_res_cons *cons;

	if (!consumer || !producer) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	if (!sipa_rm_peers_list_check_dependency(consumer->peers_list,
						 consumer->name,
						 producer->peers_list,
						 producer->name,
						 &add_dep_by_userspace)) {
		pr_err("dependency does not exist\n");
		return -EINVAL;
	}

	/* to avoid race conditions between kernel and userspace
	 * need to check that the dependency was added by same entity
	 */
	if (add_dep_by_userspace != userspace_dep) {
		pr_info("dependency was added by %s\n",
			add_dep_by_userspace ? "userspace" : "kernel");
		pr_info("ignore request to delete dependency by %s\n",
			userspace_dep ? "userspace" : "kernel");
		return 0;
	}

	pr_info("%s state: %d\n", sipa_rm_res_str(consumer->name),
		consumer->state);

	producer->needed_bw -= consumer->max_bw;
	switch (consumer->state) {
	case SIPA_RM_RELEASED:
		break;
	case SIPA_RM_GRANTED:
		release_producer = true;
		break;
	case SIPA_RM_REQUEST_IN_PROGRESS:
	case SIPA_RM_RELEASE_IN_PROGRESS:
		release_producer = true;
		check_cons_state = true;
		break;
	default:
		return -EINVAL;
	}
	sipa_rm_peers_list_remove_peer(consumer->peers_list,
				       producer->name);
	sipa_rm_peers_list_remove_peer(producer->peers_list,
				       consumer->name);

	if (check_cons_state) {
		prev_state = consumer->state;
		cons = (struct sipa_rm_res_cons *)consumer;

		if (consumer_check_all_pord_granted((struct sipa_rm_resource *)consumer)) {
			if (prev_state == SIPA_RM_RELEASE_IN_PROGRESS)
				sipa_rm_resource_consumer_do_release(cons);
			else if (prev_state == SIPA_RM_REQUEST_IN_PROGRESS)
				sipa_rm_resource_consumer_enter_granted(cons);

			if (prev_state != consumer->state)
				pr_info("%s state changed %d->%d\n",
					sipa_rm_res_str(consumer->name),
					prev_state,
					consumer->state);
		}
	}

	if (release_producer) {
		sipa_rm_resource_producer_release((struct sipa_rm_res_prod *)producer,
						  consumer->max_bw);
	}

	return 0;
}

/**
 * sipa_rm_resource_consumer_request() - consumer resource request
 * @consumer: [in] consumer
 *
 * Returns: 0 on success, negative on failure
 */
int sipa_rm_resource_consumer_request(struct sipa_rm_res_cons *cons)
{
	int result = 0;
	bool request_producer = false;
	enum sipa_rm_res_state state;

	state = cons->resource.state;

	cons->resource.ref_count++;
	switch (cons->resource.state) {
	case SIPA_RM_RELEASED:
		request_producer = true;
		break;
	case SIPA_RM_RELEASE_IN_PROGRESS:
		cons->resource.state = SIPA_RM_REQUEST_IN_PROGRESS;
		result = -EINPROGRESS;
		break;
	case SIPA_RM_GRANTED:
		goto bail;
	case SIPA_RM_REQUEST_IN_PROGRESS:
		result = -EINPROGRESS;
		goto bail;
	default:
		result = -EINVAL;
		goto bail;
	}

	if (request_producer) {
		result = sipa_rm_resource_consumer_do_request(cons);

		if (result == 0)
			sipa_rm_resource_consumer_enter_granted(cons);
		else
			cons->resource.state = SIPA_RM_REQUEST_IN_PROGRESS;
	}
bail:
	if (state != cons->resource.state)
		pr_info("%s state changed %d->%d\n",
			sipa_rm_res_str(cons->resource.name),
			state,
			cons->resource.state);
	return result;
}

/**
 * sipa_rm_resource_consumer_release() - consumer resource release
 * consumer: [in] consumer resource
 *
 * Returns: 0 on success, negative on failure
 */
int sipa_rm_resource_consumer_release(struct sipa_rm_res_cons *cons)
{
	int result = 0;
	bool release_producer = false;
	enum sipa_rm_res_state state;

	state = cons->resource.state;
	if (cons->resource.ref_count > 0)
		cons->resource.ref_count--;
	switch (cons->resource.state) {
	case SIPA_RM_RELEASED:
		goto bail;
	case SIPA_RM_GRANTED:
		if (cons->resource.ref_count == 0)
			release_producer = true;
		break;
	case SIPA_RM_REQUEST_IN_PROGRESS:
		if (!cons->resource.ref_count) {
			cons->resource.state = SIPA_RM_RELEASE_IN_PROGRESS;
			release_producer = true;
		}
		break;
	case SIPA_RM_RELEASE_IN_PROGRESS:
		result = -EINPROGRESS;
		goto bail;
	default:
		result = -EPERM;
		goto bail;
	}

	if (release_producer)
		sipa_rm_resource_consumer_do_release(cons);

bail:
	if (state != cons->resource.state)
		pr_info("%s state changed %d->%d\n",
			sipa_rm_res_str(cons->resource.name),
			state,
			cons->resource.state);

	return result;
}

static void sipa_rm_resource_consumer_handle_cb(struct sipa_rm_res_cons *cons,
						enum sipa_rm_event event)
{
	bool check_cons_state = false;
	enum sipa_rm_res_state prev_state;

	pr_debug("%s state: %d event: %d\n",
		 sipa_rm_res_str(cons->resource.name),
		 cons->resource.state,
		 event);

	/* all released events are ignored */
	if (event == SIPA_RM_EVT_RELEASED) {
		return;
	} else if (event == SIPA_RM_EVT_FAIL) {
		sipa_rm_resource_consumer_do_release(cons);
		return;
	}

	prev_state = cons->resource.state;
	switch (cons->resource.state) {
	case SIPA_RM_REQUEST_IN_PROGRESS:
		check_cons_state = true;
		break;
	case SIPA_RM_RELEASE_IN_PROGRESS:
		check_cons_state = true;
		break;
	case SIPA_RM_GRANTED:
	case SIPA_RM_RELEASED:
	default:
		return;
	}

	if (check_cons_state &&
	    consumer_check_all_pord_granted(&cons->resource)) {
		if (prev_state == SIPA_RM_RELEASE_IN_PROGRESS)
			sipa_rm_resource_consumer_do_release(cons);
		else if (prev_state == SIPA_RM_REQUEST_IN_PROGRESS)
			sipa_rm_resource_consumer_enter_granted(cons);
	}

	if (prev_state != cons->resource.state)
		pr_info("%s state changed %d->%d\n",
			sipa_rm_res_str(cons->resource.name),
			prev_state,
			cons->resource.state);
}

/**
 * sipa_rm_resource_producer_handle_cb() - propagates resource
 *	notification to all dependent producers
 * @consumer: [in] notifying resource
 */
void sipa_rm_resource_producer_handle_cb(struct sipa_rm_res_prod *prod,
					 enum sipa_rm_event event)
{
	int peers_index;
	struct sipa_rm_resource *cons;
	enum sipa_rm_res_state save_state;

	if (!prod) {
		pr_err("invalid params\n");
		return;
	}
	pr_debug("%s state: %d event: %d\n",
		 sipa_rm_res_str(prod->resource.name),
		 prod->resource.state,
		 event);

	save_state = prod->resource.state;
	/* all released events are ignored */
	if (event == SIPA_RM_EVT_RELEASED) {
		return;
	} else if (event == SIPA_RM_EVT_FAIL) {
		prod->resource.state = SIPA_RM_RELEASED;
		complete_all(&prod->request_prod_in_progress);
		goto cons;
	}

	switch (prod->resource.state) {
	case SIPA_RM_REQUEST_IN_PROGRESS:
		prod->resource.state = SIPA_RM_GRANTED;
		complete_all(&prod->request_prod_in_progress);
		break;
	case SIPA_RM_RELEASE_IN_PROGRESS:
		sipa_rm_resource_producer_release_work(prod);
		prod->resource.state = SIPA_RM_RELEASED;
		complete_all(&prod->request_prod_in_progress);
		goto bail;
	case SIPA_RM_GRANTED:
		goto bail;
	case SIPA_RM_RELEASED:
		if (prod->resource.ref_count > 0)
			prod->resource.state = SIPA_RM_GRANTED;
		else
			goto bail;
		break;
	default:
		goto bail;
	}

cons:
	for (peers_index = 0;
	     peers_index < sipa_rm_peers_list_get_size(prod->resource.peers_list);
	     peers_index++) {
		cons = sipa_rm_peers_list_get_res(peers_index,
						  prod->resource.peers_list);
		if (cons)
			sipa_rm_resource_consumer_handle_cb((struct sipa_rm_res_cons *)cons,
							    event);
	}

	return;
bail:
	if (save_state != prod->resource.state)
		pr_info("%s state changed %d->%d\n",
			sipa_rm_res_str(prod->resource.name),
			save_state,
			prod->resource.state);
}

/* sipa_rm_resource_consumer_print_stat() - print the
 * resource status and all his dependencies
 *
 * @resource: [in] Resource resource
 * @buff: [in] The buf used to print
 * @size: [in] Buf size
 *
 * Returns: number of bytes used on success, negative on failure
 */
int sipa_rm_resource_consumer_print_stat(struct sipa_rm_resource *resource,
					 char *buf,
					 int size)
{
	int i;
	int nbytes;
	int cnt = 0;
	struct sipa_rm_resource *producer;

	if (!buf || size < 0)
		return -EINVAL;

	nbytes = scnprintf(buf + cnt, size - cnt,
			   sipa_rm_res_str(resource->name));
	cnt += nbytes;
	nbytes = scnprintf(buf + cnt, size - cnt, "[%d, ", resource->max_bw);
	cnt += nbytes;

	switch (resource->state) {
	case SIPA_RM_RELEASED:
		nbytes = scnprintf(buf + cnt, size - cnt,
				   "Released] -> ");
		cnt += nbytes;
		break;
	case SIPA_RM_REQUEST_IN_PROGRESS:
		nbytes = scnprintf(buf + cnt, size - cnt,
				   "Request In Progress] -> ");
		cnt += nbytes;
		break;
	case SIPA_RM_GRANTED:
		nbytes = scnprintf(buf + cnt, size - cnt,
				   "Granted] -> ");
		cnt += nbytes;
		break;
	case SIPA_RM_RELEASE_IN_PROGRESS:
		nbytes = scnprintf(buf + cnt, size - cnt,
				   "Release In Progress] -> ");
		cnt += nbytes;
		break;
	default:
		return -EPERM;
	}

	for (i = 0; i < resource->peers_list->max_peers; ++i) {
		producer = sipa_rm_peers_list_get_res(i, resource->peers_list);
		if (!producer)
			continue;

		nbytes = scnprintf(buf + cnt, size - cnt,
				   sipa_rm_res_str(producer->name));
		cnt += nbytes;
		nbytes = scnprintf(buf + cnt, size - cnt, "[%d, ",
				   producer->max_bw);
		cnt += nbytes;

		switch (producer->state) {
		case SIPA_RM_RELEASED:
			nbytes = scnprintf(buf + cnt, size - cnt,
					   "Released], ");
			cnt += nbytes;
			break;
		case SIPA_RM_REQUEST_IN_PROGRESS:
			nbytes = scnprintf(buf + cnt, size - cnt,
					   "Request In Progress], ");
			cnt += nbytes;
			break;
		case SIPA_RM_GRANTED:
			nbytes = scnprintf(buf + cnt, size - cnt,
					   "Granted], ");
			cnt += nbytes;
			break;
		case SIPA_RM_RELEASE_IN_PROGRESS:
			nbytes = scnprintf(buf + cnt, size - cnt,
					   "Release In Progress], ");
			cnt += nbytes;
			break;
		default:
			return -EPERM;
		}
	}
	nbytes = scnprintf(buf + cnt, size - cnt, "\n");
	cnt += nbytes;

	return cnt;
}
