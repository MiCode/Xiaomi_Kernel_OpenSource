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

#include <linux/slab.h>
#include "ipa_i.h"
#include "ipa_rm_resource.h"
#include "ipa_rm_i.h"

/**
 * ipa_rm_dep_prod_index() - producer name to producer index mapping
 * @resource_name: [in] resource name (should be of producer)
 *
 * Returns: resource index mapping, IPA_RM_INDEX_INVALID
 *	in case provided resource name isn't contained
 *	in enum ipa_rm_resource_name or is not of producers.
 *
 */
int ipa_rm_prod_index(enum ipa_rm_resource_name resource_name)
{
	int result = resource_name;

	switch (resource_name) {
	case IPA_RM_RESOURCE_Q6_PROD:
	case IPA_RM_RESOURCE_USB_PROD:
	case IPA_RM_RESOURCE_HSIC_PROD:
	case IPA_RM_RESOURCE_STD_ECM_PROD:
	case IPA_RM_RESOURCE_RNDIS_PROD:
	case IPA_RM_RESOURCE_WWAN_0_PROD:
	case IPA_RM_RESOURCE_ODU_PROD:
	case IPA_RM_RESOURCE_ODU_BRIDGE_PROD:
	case IPA_RM_RESOURCE_WLAN_PROD:
		break;
	default:
		result = IPA_RM_INDEX_INVALID;
		break;
	}

	return result;
}

/**
 * ipa_rm_cons_index() - consumer name to consumer index mapping
 * @resource_name: [in] resource name (should be of consumer)
 *
 * Returns: resource index mapping, IPA_RM_INDEX_INVALID
 *	in case provided resource name isn't contained
 *	in enum ipa_rm_resource_name or is not of consumers.
 *
 */
int ipa_rm_cons_index(enum ipa_rm_resource_name resource_name)
{
	int result = resource_name;

	switch (resource_name) {
	case IPA_RM_RESOURCE_Q6_CONS:
	case IPA_RM_RESOURCE_USB_CONS:
	case IPA_RM_RESOURCE_HSIC_CONS:
	case IPA_RM_RESOURCE_WLAN_CONS:
	case IPA_RM_RESOURCE_APPS_CONS:
		break;
	default:
		result = IPA_RM_INDEX_INVALID;
		break;
	}

	return result;
}

int ipa_rm_resource_consumer_release_work(
		struct ipa_rm_resource_cons *consumer,
		enum ipa_rm_resource_state prev_state,
		bool notify_completion)
{
	int driver_result;

	IPA_RM_DBG("calling driver CB\n");
	driver_result = consumer->release_resource();
	IPA_RM_DBG("driver CB returned with %d\n", driver_result);
	/*
	 * Treat IPA_RM_RELEASE_IN_PROGRESS as IPA_RM_RELEASED
	 * for CONS which remains in RELEASE_IN_PROGRESS.
	 */
	if (driver_result == -EINPROGRESS)
		driver_result = 0;
	if (driver_result != 0 && driver_result != -EINPROGRESS) {
		IPA_RM_ERR("driver CB returned error %d\n", driver_result);
		consumer->resource.state = prev_state;
		goto bail;
	}
	if (driver_result == 0) {
		if (notify_completion)
			ipa_rm_resource_consumer_handle_cb(consumer,
					IPA_RM_RESOURCE_RELEASED);
		else
			consumer->resource.state = IPA_RM_RELEASED;
	}

	ipa_rm_perf_profile_change(consumer->resource.name);
bail:
	return driver_result;
}

int ipa_rm_resource_consumer_request_work(struct ipa_rm_resource_cons *consumer,
		enum ipa_rm_resource_state prev_state,
		u32 prod_needed_bw,
		bool notify_completion)
{
	int driver_result;

	IPA_RM_DBG("calling driver CB\n");
	driver_result = consumer->request_resource();
	IPA_RM_DBG("driver CB returned with %d\n", driver_result);
	if (driver_result == 0) {
		if (notify_completion) {
			ipa_rm_resource_consumer_handle_cb(consumer,
					IPA_RM_RESOURCE_GRANTED);
		} else {
			consumer->resource.state = IPA_RM_GRANTED;
			ipa_rm_perf_profile_change(consumer->resource.name);
			ipa_resume_resource(consumer->resource.name);
		}
	} else if (driver_result != -EINPROGRESS) {
		consumer->resource.state = prev_state;
		consumer->resource.needed_bw -= prod_needed_bw;
		consumer->usage_count--;
	}

	return driver_result;
}

int ipa_rm_resource_consumer_request(
		struct ipa_rm_resource_cons *consumer,
		u32 prod_needed_bw)
{
	int result = 0;
	enum ipa_rm_resource_state prev_state;

	IPA_RM_DBG("%s state: %d\n",
			ipa_rm_resource_str(consumer->resource.name),
			consumer->resource.state);

	prev_state = consumer->resource.state;
	consumer->resource.needed_bw += prod_needed_bw;
	switch (consumer->resource.state) {
	case IPA_RM_RELEASED:
	case IPA_RM_RELEASE_IN_PROGRESS:
		consumer->resource.state = IPA_RM_REQUEST_IN_PROGRESS;
		if (prev_state == IPA_RM_RELEASE_IN_PROGRESS ||
				ipa_inc_client_enable_clks_no_block() != 0) {
			IPA_RM_DBG("async resume work for %s\n",
				ipa_rm_resource_str(consumer->resource.name));
			ipa_rm_wq_send_resume_cmd(consumer->resource.name,
						prev_state,
						prod_needed_bw);
			result = -EINPROGRESS;
			break;
		}
		result = ipa_rm_resource_consumer_request_work(consumer,
						prev_state,
						prod_needed_bw,
						false);
		break;
	case IPA_RM_GRANTED:
		ipa_rm_perf_profile_change(consumer->resource.name);
		break;
	case IPA_RM_REQUEST_IN_PROGRESS:
		result = -EINPROGRESS;
		break;
	default:
		consumer->resource.needed_bw -= prod_needed_bw;
		result = -EPERM;
		goto bail;
	}
	consumer->usage_count++;
bail:
	IPA_RM_DBG("%s new state: %d\n",
		ipa_rm_resource_str(consumer->resource.name),
		consumer->resource.state);
	IPA_RM_DBG("EXIT with %d\n", result);

	return result;
}

int ipa_rm_resource_consumer_release(
		struct ipa_rm_resource_cons *consumer,
		u32 prod_needed_bw)
{
	int result = 0;
	enum ipa_rm_resource_state save_state;

	IPA_RM_DBG("%s state: %d\n",
		ipa_rm_resource_str(consumer->resource.name),
		consumer->resource.state);
	save_state = consumer->resource.state;
	consumer->resource.needed_bw -= prod_needed_bw;
	switch (consumer->resource.state) {
	case IPA_RM_RELEASED:
		break;
	case IPA_RM_GRANTED:
	case IPA_RM_REQUEST_IN_PROGRESS:
		if (consumer->usage_count == 0) {
			IPA_RM_ERR("consumer not used\n");
			result = -EPERM;
			break;
		}
		consumer->usage_count--;
		if (consumer->usage_count == 0) {
			consumer->resource.state = IPA_RM_RELEASE_IN_PROGRESS;
			if (save_state == IPA_RM_REQUEST_IN_PROGRESS ||
			    ipa_suspend_resource_no_block(
						consumer->resource.name) != 0) {
				ipa_rm_wq_send_suspend_cmd(
						consumer->resource.name,
						save_state,
						prod_needed_bw);
				result = -EINPROGRESS;
				goto bail;
			}
			result = ipa_rm_resource_consumer_release_work(consumer,
					save_state, false);
			goto bail;
		} else if (consumer->resource.state == IPA_RM_GRANTED) {
			ipa_rm_perf_profile_change(consumer->resource.name);
		}
		break;
	case IPA_RM_RELEASE_IN_PROGRESS:
		if (consumer->usage_count > 0)
			consumer->usage_count--;
		result = -EINPROGRESS;
		break;
	default:
		result = -EPERM;
		goto bail;
	}
bail:
	IPA_RM_DBG("%s new state: %d\n",
		ipa_rm_resource_str(consumer->resource.name),
		consumer->resource.state);
	IPA_RM_DBG("EXIT with %d\n", result);

	return result;
}

/**
 * ipa_rm_resource_producer_notify_clients() - notify
 *	all registered clients of given producer
 * @producer: producer
 * @event: event to notify
 * @notify_registered_only: notify only clients registered by
 *	ipa_rm_register()
 */
void ipa_rm_resource_producer_notify_clients(
				struct ipa_rm_resource_prod *producer,
				enum ipa_rm_event event,
				bool notify_registered_only)
{
	struct ipa_rm_notification_info *reg_info;

	IPA_RM_DBG("%s event: %d notify_registered_only: %d\n",
		ipa_rm_resource_str(producer->resource.name),
		event,
		notify_registered_only);

	list_for_each_entry(reg_info, &(producer->event_listeners), link) {
		if (notify_registered_only && !reg_info->explicit)
			continue;

		IPA_RM_DBG("Notifying %s event: %d\n",
			   ipa_rm_resource_str(producer->resource.name), event);
		reg_info->reg_params.notify_cb(reg_info->reg_params.user_data,
					       event,
					       0);
		IPA_RM_DBG("back from client CB\n");
	}

	return;
}

static int ipa_rm_resource_producer_create(struct ipa_rm_resource **resource,
		struct ipa_rm_resource_prod **producer,
		struct ipa_rm_create_params *create_params,
		int *max_peers)
{
	int result = 0;

	*producer = kzalloc(sizeof(**producer), GFP_ATOMIC);
	if (*producer == NULL) {
		IPA_RM_ERR("no mem\n");
		result = -ENOMEM;
		goto bail;
	}

	INIT_LIST_HEAD(&((*producer)->event_listeners));
	result = ipa_rm_resource_producer_register(*producer,
			&(create_params->reg_params),
			false);
	if (result) {
		IPA_RM_ERR("ipa_rm_resource_producer_register() failed\n");
		goto register_fail;
	}

	(*resource) = (struct ipa_rm_resource *) (*producer);
	(*resource)->type = IPA_RM_PRODUCER;
	*max_peers = IPA_RM_RESOURCE_CONS_MAX;
	goto bail;
register_fail:
	kfree(*producer);
bail:
	return result;
}

static void ipa_rm_resource_producer_delete(
				struct ipa_rm_resource_prod *producer)
{
	struct ipa_rm_notification_info *reg_info;
	struct list_head *pos, *q;

	ipa_rm_resource_producer_release(producer);
	list_for_each_safe(pos, q, &(producer->event_listeners)) {
		reg_info = list_entry(pos,
				struct ipa_rm_notification_info,
				link);
		list_del(pos);
		kfree(reg_info);
	}
}

static int ipa_rm_resource_consumer_create(struct ipa_rm_resource **resource,
		struct ipa_rm_resource_cons **consumer,
		struct ipa_rm_create_params *create_params,
		int *max_peers)
{
	int result = 0;

	*consumer = kzalloc(sizeof(**consumer), GFP_ATOMIC);
	if (*consumer == NULL) {
		IPA_RM_ERR("no mem\n");
		result = -ENOMEM;
		goto bail;
	}

	(*consumer)->request_resource = create_params->request_resource;
	(*consumer)->release_resource = create_params->release_resource;
	(*resource) = (struct ipa_rm_resource *) (*consumer);
	(*resource)->type = IPA_RM_CONSUMER;
	*max_peers = IPA_RM_RESOURCE_PROD_MAX;
bail:
	return result;
}

/**
 * ipa_rm_resource_create() - creates resource
 * @create_params: [in] parameters needed
 *			for resource initialization with IPA RM
 * @resource: [out] created resource
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_rm_resource_create(
		struct ipa_rm_create_params *create_params,
		struct ipa_rm_resource **resource)
{
	struct ipa_rm_resource_cons *consumer;
	struct ipa_rm_resource_prod *producer;
	int max_peers;
	int result = 0;

	if (!create_params) {
		result = -EINVAL;
		goto bail;
	}

	if (IPA_RM_RESORCE_IS_PROD(create_params->name)) {
		result = ipa_rm_resource_producer_create(resource,
				&producer,
				create_params,
				&max_peers);
		if (result) {
			IPA_RM_ERR("ipa_rm_resource_producer_create failed\n");
			goto bail;
		}
	} else if (IPA_RM_RESORCE_IS_CONS(create_params->name)) {
		result = ipa_rm_resource_consumer_create(resource,
				&consumer,
				create_params,
				&max_peers);
		if (result) {
			IPA_RM_ERR("ipa_rm_resource_producer_create failed\n");
			goto bail;
		}
	} else {
		IPA_RM_ERR("invalied resource\n");
		result = -EPERM;
		goto bail;
	}

	result = ipa_rm_peers_list_create(max_peers,
			&((*resource)->peers_list));
	if (result) {
		IPA_RM_ERR("ipa_rm_peers_list_create failed\n");
		goto peers_alloc_fail;
	}
	(*resource)->name = create_params->name;
	(*resource)->floor_voltage = create_params->floor_voltage;
	(*resource)->state = IPA_RM_RELEASED;
	goto bail;

peers_alloc_fail:
	ipa_rm_resource_delete(*resource);
bail:
	return result;
}

/**
 * ipa_rm_resource_delete() - deletes resource
 * @resource: [in] resource
 *			for resource initialization with IPA RM
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_rm_resource_delete(struct ipa_rm_resource *resource)
{
	struct ipa_rm_resource *consumer;
	struct ipa_rm_resource *producer;
	int peers_index;
	int result = 0;
	int list_size;

	IPA_RM_DBG("ipa_rm_resource_delete ENTER with resource %d\n",
					resource->name);
	if (!resource) {
		IPA_RM_ERR("invalid params\n");
		return -EINVAL;
	}

	if (resource->type == IPA_RM_PRODUCER) {
		if (resource->peers_list) {
			list_size = ipa_rm_peers_list_get_size(
				resource->peers_list);
			for (peers_index = 0;
				peers_index < list_size;
				peers_index++) {
				consumer = ipa_rm_peers_list_get_resource(
						peers_index,
						resource->peers_list);
				if (consumer)
					ipa_rm_resource_delete_dependency(
						resource,
						consumer);
			}
			ipa_rm_peers_list_delete(resource->peers_list);
		}

		ipa_rm_resource_producer_delete(
				(struct ipa_rm_resource_prod *) resource);
		kfree((struct ipa_rm_resource_prod *) resource);
	} else if (resource->type == IPA_RM_CONSUMER) {
		if (resource->peers_list) {
			list_size = ipa_rm_peers_list_get_size(
				resource->peers_list);
			for (peers_index = 0;
					peers_index < list_size;
					peers_index++){
				producer = ipa_rm_peers_list_get_resource(
							peers_index,
							resource->peers_list);
				if (producer)
					ipa_rm_resource_delete_dependency(
							producer,
							resource);
			}
			ipa_rm_peers_list_delete(resource->peers_list);
		}
		kfree((struct ipa_rm_resource_cons *) resource);
	}
	return result;
}

/**
 * ipa_rm_resource_register() - register resource
 * @resource: [in] resource
 * @reg_params: [in] registration parameters
 * @explicit: [in] registered explicitly by ipa_rm_register()
 *
 * Returns: 0 on success, negative on failure
 *
 * Producer resource is expected for this call.
 *
 */
int ipa_rm_resource_producer_register(struct ipa_rm_resource_prod *producer,
		struct ipa_rm_register_params *reg_params,
		bool explicit)
{
	int result = 0;
	struct ipa_rm_notification_info *reg_info;
	struct list_head *pos;

	if (!producer || !reg_params) {
		IPA_RM_ERR("invalid params\n");
		result = -EPERM;
		goto bail;
	}

	list_for_each(pos, &(producer->event_listeners)) {
		reg_info = list_entry(pos,
					struct ipa_rm_notification_info,
					link);
		if (reg_info->reg_params.notify_cb ==
						reg_params->notify_cb) {
			IPA_RM_ERR("already registered\n");
			result = -EPERM;
			goto bail;
		}

	}

	reg_info = kzalloc(sizeof(*reg_info), GFP_ATOMIC);
	if (reg_info == NULL) {
		IPA_RM_ERR("no mem\n");
		result = -ENOMEM;
		goto bail;
	}

	reg_info->reg_params.user_data = reg_params->user_data;
	reg_info->reg_params.notify_cb = reg_params->notify_cb;
	reg_info->explicit = explicit;
	INIT_LIST_HEAD(&reg_info->link);
	list_add(&reg_info->link, &producer->event_listeners);
bail:
	return result;
}

/**
 * ipa_rm_resource_deregister() - register resource
 * @resource: [in] resource
 * @reg_params: [in] registration parameters
 *
 * Returns: 0 on success, negative on failure
 *
 * Producer resource is expected for this call.
 * This function deleted only single instance of
 * registration info.
 *
 */
int ipa_rm_resource_producer_deregister(struct ipa_rm_resource_prod *producer,
		struct ipa_rm_register_params *reg_params)
{
	int result = -EINVAL;
	struct ipa_rm_notification_info *reg_info;
	struct list_head *pos, *q;

	if (!producer || !reg_params) {
		IPA_RM_ERR("invalid params\n");
		return -EINVAL;
	}

	list_for_each_safe(pos, q, &(producer->event_listeners)) {
		reg_info = list_entry(pos,
				struct ipa_rm_notification_info,
				link);
		if (reg_info->reg_params.notify_cb ==
						reg_params->notify_cb) {
			list_del(pos);
			kfree(reg_info);
			result = 0;
			goto bail;
		}
	}
bail:
	return result;
}

/**
 * ipa_rm_resource_add_dependency() - add dependency between two
 *				given resources
 * @resource: [in] resource resource
 * @depends_on: [in] depends_on resource
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_rm_resource_add_dependency(struct ipa_rm_resource *resource,
				   struct ipa_rm_resource *depends_on)
{
	int result = 0;
	int consumer_result;

	if (!resource || !depends_on) {
		IPA_RM_ERR("invalid params\n");
		return -EINVAL;
	}

	if (ipa_rm_peers_list_check_dependency(resource->peers_list,
			resource->name,
			depends_on->peers_list,
			depends_on->name)) {
		IPA_RM_ERR("dependency already exists\n");
		return -EEXIST;
	}

	ipa_rm_peers_list_add_peer(resource->peers_list, depends_on);
	ipa_rm_peers_list_add_peer(depends_on->peers_list, resource);
	IPA_RM_DBG("%s state: %d\n", ipa_rm_resource_str(resource->name),
				resource->state);

	resource->needed_bw += depends_on->max_bw;
	switch (resource->state) {
	case IPA_RM_RELEASED:
	case IPA_RM_RELEASE_IN_PROGRESS:
		break;
	case IPA_RM_GRANTED:
	case IPA_RM_REQUEST_IN_PROGRESS:
	{
		enum ipa_rm_resource_state prev_state = resource->state;
		resource->state = IPA_RM_REQUEST_IN_PROGRESS;
		((struct ipa_rm_resource_prod *)
					resource)->pending_request++;
		consumer_result = ipa_rm_resource_consumer_request(
				(struct ipa_rm_resource_cons *)depends_on,
				resource->max_bw);
		if (consumer_result != -EINPROGRESS) {
			resource->state = prev_state;
			((struct ipa_rm_resource_prod *)
					resource)->pending_request--;
			ipa_rm_perf_profile_change(resource->name);
		}
		result = consumer_result;
		break;
	}
	default:
		IPA_RM_ERR("invalid state\n");
		result = -EPERM;
		goto bail;
	}
bail:
	IPA_RM_DBG("%s new state: %d\n", ipa_rm_resource_str(resource->name),
					resource->state);
	IPA_RM_DBG("EXIT with %d\n", result);

	return result;
}

/**
 * ipa_rm_resource_delete_dependency() - add dependency between two
 *				given resources
 * @resource: [in] resource resource
 * @depends_on: [in] depends_on resource
 *
 * Returns: 0 on success, negative on failure
 * EINPROGRESS is returned in case this is the last dependency
 * of given resource and IPA RM client should receive the RELEASED cb
 */
int ipa_rm_resource_delete_dependency(struct ipa_rm_resource *resource,
				   struct ipa_rm_resource *depends_on)
{
	int result = 0;
	bool state_changed = false;
	bool release_consumer = false;
	enum ipa_rm_event evt;

	if (!resource || !depends_on) {
		IPA_RM_ERR("invalid params\n");
		return -EINVAL;
	}

	if (!ipa_rm_peers_list_check_dependency(resource->peers_list,
			resource->name,
			depends_on->peers_list,
			depends_on->name)) {
		IPA_RM_ERR("dependency does not exist\n");
		return -EINVAL;
	}
	IPA_RM_DBG("%s state: %d\n", ipa_rm_resource_str(resource->name),
				resource->state);

	resource->needed_bw -= depends_on->max_bw;
	switch (resource->state) {
	case IPA_RM_RELEASED:
		break;
	case IPA_RM_GRANTED:
		ipa_rm_perf_profile_change(resource->name);
		release_consumer = true;
		break;
	case IPA_RM_RELEASE_IN_PROGRESS:
		if (((struct ipa_rm_resource_prod *)
			resource)->pending_release > 0)
				((struct ipa_rm_resource_prod *)
					resource)->pending_release--;
		if (depends_on->state == IPA_RM_RELEASE_IN_PROGRESS &&
			((struct ipa_rm_resource_prod *)
			resource)->pending_release == 0) {
			resource->state = IPA_RM_RELEASED;
			state_changed = true;
			evt = IPA_RM_RESOURCE_RELEASED;
			ipa_rm_perf_profile_change(resource->name);
		}
		break;
	case IPA_RM_REQUEST_IN_PROGRESS:
		release_consumer = true;
		if (((struct ipa_rm_resource_prod *)
			resource)->pending_request > 0)
				((struct ipa_rm_resource_prod *)
					resource)->pending_request--;
		if (depends_on->state == IPA_RM_REQUEST_IN_PROGRESS &&
			((struct ipa_rm_resource_prod *)
				resource)->pending_request == 0) {
			resource->state = IPA_RM_GRANTED;
			state_changed = true;
			evt = IPA_RM_RESOURCE_GRANTED;
			ipa_rm_perf_profile_change(resource->name);
		}
		break;
	default:
		result = -EINVAL;
		goto bail;
	}
	if (state_changed &&
		ipa_rm_peers_list_has_last_peer(resource->peers_list)) {
		(void) ipa_rm_wq_send_cmd(IPA_RM_WQ_NOTIFY_PROD,
				resource->name,
				evt,
				false);
		result = -EINPROGRESS;
	}
	IPA_RM_DBG("%s new state: %d\n", ipa_rm_resource_str(resource->name),
					resource->state);
	ipa_rm_peers_list_remove_peer(resource->peers_list,
			depends_on->name);
	ipa_rm_peers_list_remove_peer(depends_on->peers_list,
			resource->name);
	if (release_consumer)
		(void) ipa_rm_resource_consumer_release(
				(struct ipa_rm_resource_cons *)depends_on,
				resource->max_bw);
bail:
	IPA_RM_DBG("EXIT with %d\n", result);

	return result;
}

/**
 * ipa_rm_resource_producer_request() - producer resource request
 * @producer: [in] producer
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_rm_resource_producer_request(struct ipa_rm_resource_prod *producer)
{
	int peers_index;
	int result = 0;
	struct ipa_rm_resource *consumer;
	int consumer_result;
	enum ipa_rm_resource_state state;

	state = producer->resource.state;
	switch (producer->resource.state) {
	case IPA_RM_RELEASED:
	case IPA_RM_RELEASE_IN_PROGRESS:
		producer->resource.state = IPA_RM_REQUEST_IN_PROGRESS;
		break;
	case IPA_RM_GRANTED:
		goto unlock_and_bail;
	case IPA_RM_REQUEST_IN_PROGRESS:
		result = -EINPROGRESS;
		goto unlock_and_bail;
	default:
		result = -EINVAL;
		goto unlock_and_bail;
	}

	producer->pending_request = 0;
	for (peers_index = 0;
		peers_index < ipa_rm_peers_list_get_size(
				producer->resource.peers_list);
		peers_index++) {
		consumer = ipa_rm_peers_list_get_resource(peers_index,
				producer->resource.peers_list);
		if (consumer) {
			producer->pending_request++;
			consumer_result = ipa_rm_resource_consumer_request(
				(struct ipa_rm_resource_cons *)consumer,
				producer->resource.max_bw);
			if (consumer_result == -EINPROGRESS) {
				result = -EINPROGRESS;
			} else {
				producer->pending_request--;
				if (consumer_result != 0) {
					result = consumer_result;
					goto bail;
				}
			}
		}
	}

	if (producer->pending_request == 0) {
		producer->resource.state = IPA_RM_GRANTED;
		ipa_rm_perf_profile_change(producer->resource.name);
		(void) ipa_rm_wq_send_cmd(IPA_RM_WQ_NOTIFY_PROD,
			producer->resource.name,
			IPA_RM_RESOURCE_GRANTED,
			true);
		result = 0;
	}
unlock_and_bail:
	if (state != producer->resource.state)
		IPA_RM_DBG("%s state changed %d->%d\n",
			ipa_rm_resource_str(producer->resource.name),
			state,
			producer->resource.state);
bail:
	return result;
}

/**
 * ipa_rm_resource_producer_release() - producer resource release
 * producer: [in] producer resource
 *
 * Returns: 0 on success, negative on failure
 *
 */
int ipa_rm_resource_producer_release(struct ipa_rm_resource_prod *producer)
{
	int peers_index;
	int result = 0;
	struct ipa_rm_resource *consumer;
	int consumer_result;
	enum ipa_rm_resource_state state;

	state = producer->resource.state;
	switch (producer->resource.state) {
	case IPA_RM_RELEASED:
		goto bail;
	case IPA_RM_GRANTED:
	case IPA_RM_REQUEST_IN_PROGRESS:
		producer->resource.state = IPA_RM_RELEASE_IN_PROGRESS;
		break;
	case IPA_RM_RELEASE_IN_PROGRESS:
		result = -EINPROGRESS;
		goto bail;
	default:
		result = -EPERM;
		goto bail;
	}

	producer->pending_release = 0;
	for (peers_index = 0;
		peers_index < ipa_rm_peers_list_get_size(
				producer->resource.peers_list);
		peers_index++) {
		consumer = ipa_rm_peers_list_get_resource(peers_index,
				producer->resource.peers_list);
		if (consumer) {
			producer->pending_release++;
			consumer_result = ipa_rm_resource_consumer_release(
				(struct ipa_rm_resource_cons *)consumer,
				producer->resource.max_bw);
			producer->pending_release--;
		}
	}

	if (producer->pending_release == 0) {
		producer->resource.state = IPA_RM_RELEASED;
		ipa_rm_perf_profile_change(producer->resource.name);
		(void) ipa_rm_wq_send_cmd(IPA_RM_WQ_NOTIFY_PROD,
			producer->resource.name,
			IPA_RM_RESOURCE_RELEASED,
			true);
	}
bail:
	if (state != producer->resource.state)
		IPA_RM_DBG("%s state changed %d->%d\n",
		ipa_rm_resource_str(producer->resource.name),
		state,
		producer->resource.state);

	return result;
}

static void ipa_rm_resource_producer_handle_cb(
		struct ipa_rm_resource_prod *producer,
		enum ipa_rm_event event)
{
	IPA_RM_DBG("%s state: %d event: %d pending_request: %d\n",
		ipa_rm_resource_str(producer->resource.name),
		producer->resource.state,
		event,
		producer->pending_request);

	switch (producer->resource.state) {
	case IPA_RM_REQUEST_IN_PROGRESS:
		if (event != IPA_RM_RESOURCE_GRANTED)
			goto unlock_and_bail;
		if (producer->pending_request > 0) {
			producer->pending_request--;
			if (producer->pending_request == 0) {
				producer->resource.state =
						IPA_RM_GRANTED;
				ipa_rm_perf_profile_change(
					producer->resource.name);
				ipa_rm_resource_producer_notify_clients(
						producer,
						IPA_RM_RESOURCE_GRANTED,
						false);
				goto bail;
			}
		}
		break;
	case IPA_RM_RELEASE_IN_PROGRESS:
		if (event != IPA_RM_RESOURCE_RELEASED)
			goto unlock_and_bail;
		if (producer->pending_release > 0) {
			producer->pending_release--;
			if (producer->pending_release == 0) {
				producer->resource.state =
						IPA_RM_RELEASED;
				ipa_rm_perf_profile_change(
					producer->resource.name);
				ipa_rm_resource_producer_notify_clients(
						producer,
						IPA_RM_RESOURCE_RELEASED,
						false);
				goto bail;
			}
		}
		break;
	case IPA_RM_GRANTED:
	case IPA_RM_RELEASED:
	default:
		goto unlock_and_bail;
	}
unlock_and_bail:
	IPA_RM_DBG("%s new state: %d\n",
		ipa_rm_resource_str(producer->resource.name),
		producer->resource.state);
bail:
	return;
}

/**
 * ipa_rm_resource_consumer_handle_cb() - propagates resource
 *	notification to all dependent producers
 * @consumer: [in] notifying resource
 *
 */
void ipa_rm_resource_consumer_handle_cb(struct ipa_rm_resource_cons *consumer,
				enum ipa_rm_event event)
{
	int peers_index;
	struct ipa_rm_resource *producer;

	if (!consumer) {
		IPA_RM_ERR("invalid params\n");
		return;
	}
	IPA_RM_DBG("%s state: %d event: %d\n",
		ipa_rm_resource_str(consumer->resource.name),
		consumer->resource.state,
		event);

	switch (consumer->resource.state) {
	case IPA_RM_REQUEST_IN_PROGRESS:
		if (event == IPA_RM_RESOURCE_RELEASED)
			goto bail;
		consumer->resource.state = IPA_RM_GRANTED;
		ipa_rm_perf_profile_change(consumer->resource.name);
		ipa_resume_resource(consumer->resource.name);
		break;
	case IPA_RM_RELEASE_IN_PROGRESS:
		if (event == IPA_RM_RESOURCE_GRANTED)
			goto bail;
		consumer->resource.state = IPA_RM_RELEASED;
		break;
	case IPA_RM_GRANTED:
	case IPA_RM_RELEASED:
	default:
		goto bail;
	}

	for (peers_index = 0;
		peers_index < ipa_rm_peers_list_get_size(
				consumer->resource.peers_list);
		peers_index++) {
		producer = ipa_rm_peers_list_get_resource(peers_index,
				consumer->resource.peers_list);
		if (producer)
			ipa_rm_resource_producer_handle_cb(
					(struct ipa_rm_resource_prod *)
						producer,
						event);
	}

	return;
bail:
	IPA_RM_DBG("%s new state: %d\n",
		ipa_rm_resource_str(consumer->resource.name),
		consumer->resource.state);

	return;
}

/*
 * ipa_rm_resource_set_perf_profile() - sets the performance profile to
 *					resource.
 *
 * @resource: [in] resource
 * @profile: [in] profile to be set
 *
 * sets the profile to the given resource, In case the resource is
 * granted, update bandwidth vote of the resource
 */
int ipa_rm_resource_set_perf_profile(struct ipa_rm_resource *resource,
				     struct ipa_rm_perf_profile *profile)
{
	int peers_index;
	struct ipa_rm_resource *peer;

	if (!resource || !profile) {
		IPA_RM_ERR("invalid params\n");
		return -EINVAL;
	}

	if (profile->max_supported_bandwidth_mbps == resource->max_bw) {
		IPA_RM_DBG("same profile\n");
		return 0;
	}

	if ((resource->type == IPA_RM_PRODUCER &&
	    (resource->state == IPA_RM_GRANTED ||
	    resource->state == IPA_RM_REQUEST_IN_PROGRESS)) ||
	    resource->type == IPA_RM_CONSUMER) {
		for (peers_index = 0;
		     peers_index < ipa_rm_peers_list_get_size(
		     resource->peers_list);
		     peers_index++) {
			peer = ipa_rm_peers_list_get_resource(peers_index,
				resource->peers_list);
			if (!peer)
				continue;
			peer->needed_bw -= resource->max_bw;
			peer->needed_bw +=
				profile->max_supported_bandwidth_mbps;
			if (peer->state == IPA_RM_GRANTED)
				ipa_rm_perf_profile_change(peer->name);
		}
	}

	resource->max_bw = profile->max_supported_bandwidth_mbps;
	if (resource->state == IPA_RM_GRANTED)
		ipa_rm_perf_profile_change(resource->name);

	return 0;
}


/*
 * ipa_rm_resource_producer_print_stat() - print the
 * resource status and all his dependencies
 *
 * @resource: [in] Resource resource
 * @buff: [in] The buf used to print
 * @size: [in] Buf size
 *
 * Returns: number of bytes used on success, negative on failure
 */
int ipa_rm_resource_producer_print_stat(
				struct ipa_rm_resource *resource,
				char *buf,
				int size){

	int i;
	int nbytes;
	int cnt = 0;
	struct ipa_rm_resource *consumer;

	if (!buf || size < 0)
		return -EINVAL;

	nbytes = scnprintf(buf + cnt, size - cnt,
		ipa_rm_resource_str(resource->name));
	cnt += nbytes;
	nbytes = scnprintf(buf + cnt, size - cnt, "[");
	cnt += nbytes;

	switch (resource->state) {
	case IPA_RM_RELEASED:
		nbytes = scnprintf(buf + cnt, size - cnt,
			"Released] -> ");
		cnt += nbytes;
		break;
	case IPA_RM_REQUEST_IN_PROGRESS:
		nbytes = scnprintf(buf + cnt, size - cnt,
			"Request In Progress] -> ");
		cnt += nbytes;
		break;
	case IPA_RM_GRANTED:
		nbytes = scnprintf(buf + cnt, size - cnt,
			"Granted] -> ");
		cnt += nbytes;
		break;
	case IPA_RM_RELEASE_IN_PROGRESS:
		nbytes = scnprintf(buf + cnt, size - cnt,
			"Release In Progress] -> ");
		cnt += nbytes;
		break;
	default:
		return -EPERM;
	}

	for (i = 0; i < resource->peers_list->max_peers; ++i) {
		consumer =
			ipa_rm_peers_list_get_resource(
			i,
			resource->peers_list);
		if (consumer) {
			nbytes = scnprintf(buf + cnt, size - cnt,
				ipa_rm_resource_str(consumer->name));
			cnt += nbytes;
			nbytes = scnprintf(buf + cnt, size - cnt, "[");
			cnt += nbytes;

			switch (consumer->state) {
			case IPA_RM_RELEASED:
				nbytes = scnprintf(buf + cnt, size - cnt,
					"Released], ");
				cnt += nbytes;
				break;
			case IPA_RM_REQUEST_IN_PROGRESS:
				nbytes = scnprintf(buf + cnt, size - cnt,
						"Request In Progress], ");
				cnt += nbytes;
					break;
			case IPA_RM_GRANTED:
				nbytes = scnprintf(buf + cnt, size - cnt,
						"Granted], ");
				cnt += nbytes;
				break;
			case IPA_RM_RELEASE_IN_PROGRESS:
				nbytes = scnprintf(buf + cnt, size - cnt,
						"Release In Progress], ");
				cnt += nbytes;
				break;
			default:
				return -EPERM;
			}
		}
	}
	nbytes = scnprintf(buf + cnt, size - cnt, "\n");
	cnt += nbytes;

	return cnt;
}
