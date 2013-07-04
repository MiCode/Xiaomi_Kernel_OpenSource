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
	case IPA_RM_RESOURCE_BRIDGE_PROD:
	case IPA_RM_RESOURCE_A2_PROD:
	case IPA_RM_RESOURCE_USB_PROD:
	case IPA_RM_RESOURCE_HSIC_PROD:
	case IPA_RM_RESOURCE_STD_ECM_PROD:
	case IPA_RM_RESOURCE_WWAN_0_PROD:
	case IPA_RM_RESOURCE_WWAN_1_PROD:
	case IPA_RM_RESOURCE_WWAN_2_PROD:
	case IPA_RM_RESOURCE_WWAN_3_PROD:
	case IPA_RM_RESOURCE_WWAN_4_PROD:
	case IPA_RM_RESOURCE_WWAN_5_PROD:
	case IPA_RM_RESOURCE_WWAN_6_PROD:
	case IPA_RM_RESOURCE_WWAN_7_PROD:
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
	case IPA_RM_RESOURCE_A2_CONS:
	case IPA_RM_RESOURCE_USB_CONS:
	case IPA_RM_RESOURCE_HSIC_CONS:
		break;
	default:
		result = IPA_RM_INDEX_INVALID;
		break;
	}
	return result;
}

static int ipa_rm_resource_consumer_request(
		struct ipa_rm_resource_cons *consumer)
{
	int result = 0;
	int driver_result;
	unsigned long flags;
	IPADBG("IPA RM ::%s name %d ENTER\n",
	       __func__, consumer->resource.name);
	spin_lock_irqsave(&consumer->resource.state_lock, flags);
	switch (consumer->resource.state) {
	case IPA_RM_RELEASED:
	case IPA_RM_RELEASE_IN_PROGRESS:
	{
		enum ipa_rm_resource_state prev_state =
						consumer->resource.state;
		consumer->resource.state = IPA_RM_REQUEST_IN_PROGRESS;
		spin_unlock_irqrestore(&consumer->resource.state_lock, flags);
		driver_result = consumer->request_resource();
		spin_lock_irqsave(&consumer->resource.state_lock, flags);
		if (driver_result == 0)
			consumer->resource.state = IPA_RM_GRANTED;
		else if (driver_result != -EINPROGRESS) {
			consumer->resource.state = prev_state;
			result = driver_result;
			goto bail;
		}
		result = driver_result;
		break;
	}
	case IPA_RM_GRANTED:
		break;
	case IPA_RM_REQUEST_IN_PROGRESS:
		result = -EINPROGRESS;
		break;
	default:
		result = -EPERM;
		goto bail;
	}
	consumer->usage_count++;
bail:
	spin_unlock_irqrestore(&consumer->resource.state_lock, flags);
	IPADBG("IPA RM ::ipa_rm_resource_consumer_request EXIT [%d]\n", result);
	return result;
}

static int ipa_rm_resource_consumer_release(
		struct ipa_rm_resource_cons *consumer)
{
	int result = 0;
	int driver_result;
	unsigned long flags;
	enum ipa_rm_resource_state save_state;
	IPADBG("IPA RM ::%s name %d ENTER\n",
	       __func__, consumer->resource.name);
	spin_lock_irqsave(&consumer->resource.state_lock, flags);
	switch (consumer->resource.state) {
	case IPA_RM_RELEASED:
		break;
	case IPA_RM_GRANTED:
	case IPA_RM_REQUEST_IN_PROGRESS:
		if (consumer->usage_count > 0)
			consumer->usage_count--;
		if (consumer->usage_count == 0) {
			save_state = consumer->resource.state;
			consumer->resource.state = IPA_RM_RELEASE_IN_PROGRESS;
			spin_unlock_irqrestore(&consumer->resource.state_lock,
					flags);
			driver_result = consumer->release_resource();
			spin_lock_irqsave(&consumer->resource.state_lock,
					flags);
			if (driver_result == 0)
				consumer->resource.state = IPA_RM_RELEASED;
			else if (driver_result != -EINPROGRESS)
				consumer->resource.state = save_state;
			result = driver_result;
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
	spin_unlock_irqrestore(&consumer->resource.state_lock, flags);
	IPADBG("IPA RM ::ipa_rm_resource_consumer_release EXIT [%d]\n", result);
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
	struct ipa_rm_notification_info *reg_info, *reg_info_cloned;
	struct list_head *pos, *q;
	LIST_HEAD(cloned_list);
	read_lock(&producer->event_listeners_lock);
	list_for_each(pos, &(producer->event_listeners)) {
		reg_info = list_entry(pos,
					struct ipa_rm_notification_info,
					link);
		if (notify_registered_only && !reg_info->explicit)
			continue;
		reg_info_cloned = kzalloc(sizeof(*reg_info_cloned), GFP_ATOMIC);
		if (!reg_info_cloned)
			goto clone_list_failed;
		reg_info_cloned->reg_params.notify_cb =
				reg_info->reg_params.notify_cb;
		reg_info_cloned->reg_params.user_data =
				reg_info->reg_params.user_data;
		list_add(&reg_info_cloned->link, &cloned_list);
	}
	read_unlock(&producer->event_listeners_lock);
	list_for_each_safe(pos, q, &cloned_list) {
		reg_info = list_entry(pos,
					struct ipa_rm_notification_info,
					link);
		reg_info->reg_params.notify_cb(
				reg_info->reg_params.user_data,
				event,
				0);
		list_del(pos);
		kfree(reg_info);
	}
	return;
clone_list_failed:
	read_unlock(&producer->event_listeners_lock);
}

static int ipa_rm_resource_producer_create(struct ipa_rm_resource **resource,
		struct ipa_rm_resource_prod **producer,
		struct ipa_rm_create_params *create_params,
		int *max_peers)
{
	int result = 0;
	*producer = kzalloc(sizeof(**producer), GFP_KERNEL);
	if (*producer == NULL) {
		result = -ENOMEM;
		goto bail;
	}
	rwlock_init(&(*producer)->event_listeners_lock);
	INIT_LIST_HEAD(&((*producer)->event_listeners));
	result = ipa_rm_resource_producer_register(*producer,
			&(create_params->reg_params),
			false);
	if (result)
		goto register_fail;
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
	write_lock(&producer->event_listeners_lock);
	list_for_each_safe(pos, q, &(producer->event_listeners)) {
		reg_info = list_entry(pos,
				struct ipa_rm_notification_info,
				link);
		list_del(pos);
		kfree(reg_info);
	}
	write_unlock(&producer->event_listeners_lock);
}

static int ipa_rm_resource_consumer_create(struct ipa_rm_resource **resource,
		struct ipa_rm_resource_cons **consumer,
		struct ipa_rm_create_params *create_params,
		int *max_peers)
{
	int result = 0;
	*consumer = kzalloc(sizeof(**consumer), GFP_KERNEL);
	if (*consumer == NULL) {
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
		if (result)
			goto bail;
	} else if (IPA_RM_RESORCE_IS_CONS(create_params->name)) {
		result = ipa_rm_resource_consumer_create(resource,
				&consumer,
				create_params,
				&max_peers);
		if (result)
			goto bail;
	} else {
		result = -EPERM;
		goto bail;
	}
	result = ipa_rm_peers_list_create(max_peers,
			&((*resource)->peers_list));
	if (result)
		goto peers_alloc_fail;
	(*resource)->name = create_params->name;
	(*resource)->state = IPA_RM_RELEASED;
	spin_lock_init(&((*resource)->state_lock));
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
	struct ipa_rm_resource *consumer, *producer;
	int peers_index, result = 0, list_size;

	IPADBG("ipa_rm_resource_delete ENTER with resource %d\n",
					resource->name);
	if (!resource) {
		IPADBG("ipa_rm_resource_delete ENTER with invalid param\n");
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
	IPADBG("ipa_rm_resource_delete SUCCESS\n");
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
		result = -EPERM;
		goto bail;
	}
	IPADBG("IPA RM: %s name %d ENTER\n",
	       __func__, producer->resource.name);
	read_lock(&producer->event_listeners_lock);
	list_for_each(pos, &(producer->event_listeners)) {
		reg_info = list_entry(pos,
					struct ipa_rm_notification_info,
					link);
		if (reg_info->reg_params.notify_cb ==
						reg_params->notify_cb) {
			result = -EPERM;
			read_unlock(&producer->event_listeners_lock);
			goto bail;
		}

	}
	read_unlock(&producer->event_listeners_lock);
	reg_info = kzalloc(sizeof(*reg_info), GFP_KERNEL);
	if (reg_info == NULL) {
		result = -ENOMEM;
		goto bail;
	}
	reg_info->reg_params.user_data = reg_params->user_data;
	reg_info->reg_params.notify_cb = reg_params->notify_cb;
	reg_info->explicit = explicit;
	INIT_LIST_HEAD(&reg_info->link);
	write_lock(&producer->event_listeners_lock);
	list_add(&reg_info->link, &producer->event_listeners);
	write_unlock(&producer->event_listeners_lock);
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
	if (!producer || !reg_params)
		return -EINVAL;
	write_lock(&producer->event_listeners_lock);
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
	write_unlock(&producer->event_listeners_lock);
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
	unsigned long flags;
	int consumer_result;
	if (!resource || !depends_on)
		return -EINVAL;
	if (ipa_rm_peers_list_check_dependency(resource->peers_list,
			resource->name,
			depends_on->peers_list,
			depends_on->name))
		return -EEXIST;
	ipa_rm_peers_list_add_peer(resource->peers_list, depends_on);
	ipa_rm_peers_list_add_peer(depends_on->peers_list, resource);
	spin_lock_irqsave(&resource->state_lock, flags);
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
		spin_unlock_irqrestore(&resource->state_lock, flags);
		consumer_result = ipa_rm_resource_consumer_request(
				(struct ipa_rm_resource_cons *)depends_on);
		spin_lock_irqsave(&resource->state_lock, flags);
		if (consumer_result != -EINPROGRESS) {
			resource->state = prev_state;
			((struct ipa_rm_resource_prod *)
					resource)->pending_request--;
		}
		result = consumer_result;
		break;
	}
	default:
		result = -EPERM;
		goto bail;
	}
bail:
	spin_unlock_irqrestore(&resource->state_lock, flags);
	IPADBG("IPA RM ipa_rm_resource_add_dependency name[%d]count[%d]EXIT\n",
			resource->name, resource->peers_list->peers_count);
	IPADBG("IPA RM ipa_rm_resource_add_dependency name[%d]count[%d]EXIT\n",
			depends_on->name, depends_on->peers_list->peers_count);
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
	unsigned long flags;
	unsigned long consumer_flags;
	bool state_changed = false;
	bool release_consumer = false;
	if (!resource || !depends_on)
		return -EINVAL;
	IPADBG("IPA RM: %s from %d to %d ENTER\n",
			__func__,
	       resource->name,
	       depends_on->name);
	if (!ipa_rm_peers_list_check_dependency(resource->peers_list,
			resource->name,
			depends_on->peers_list,
			depends_on->name))
		return -EINVAL;
	spin_lock_irqsave(&resource->state_lock, flags);
	switch (resource->state) {
	case IPA_RM_RELEASED:
		break;
	case IPA_RM_GRANTED:
		release_consumer = true;
		break;
	case IPA_RM_RELEASE_IN_PROGRESS:
		if (((struct ipa_rm_resource_prod *)
			resource)->pending_release > 0)
				((struct ipa_rm_resource_prod *)
					resource)->pending_release--;
		spin_lock_irqsave(&depends_on->state_lock, consumer_flags);
		if (depends_on->state == IPA_RM_RELEASE_IN_PROGRESS &&
			((struct ipa_rm_resource_prod *)
			resource)->pending_release == 0) {
			resource->state = IPA_RM_RELEASED;
			state_changed = true;
		}
		spin_unlock_irqrestore(&depends_on->state_lock, consumer_flags);
		break;
	case IPA_RM_REQUEST_IN_PROGRESS:
		release_consumer = true;
		if (((struct ipa_rm_resource_prod *)
			resource)->pending_request > 0)
				((struct ipa_rm_resource_prod *)
					resource)->pending_request--;
		spin_lock_irqsave(&depends_on->state_lock, consumer_flags);
		if (depends_on->state == IPA_RM_REQUEST_IN_PROGRESS &&
			((struct ipa_rm_resource_prod *)
				resource)->pending_request == 0) {
			resource->state = IPA_RM_GRANTED;
			state_changed = true;
		}
		spin_unlock_irqrestore(&depends_on->state_lock, consumer_flags);
		break;
	default:
		result = -EINVAL;
		spin_unlock_irqrestore(&resource->state_lock, flags);
		goto bail;
	}
	if (state_changed &&
		ipa_rm_peers_list_has_last_peer(resource->peers_list)) {
		(void) ipa_rm_wq_send_cmd(IPA_RM_WQ_NOTIFY_PROD,
				resource->name,
				resource->state,
				false);
		result = -EINPROGRESS;
	}
	spin_unlock_irqrestore(&resource->state_lock, flags);
	ipa_rm_peers_list_remove_peer(resource->peers_list,
			depends_on->name);
	ipa_rm_peers_list_remove_peer(depends_on->peers_list,
			resource->name);
	if (release_consumer)
		(void) ipa_rm_resource_consumer_release(
				(struct ipa_rm_resource_cons *)depends_on);
	IPADBG("IPA RM: %s from %d to %d SUCCESS\n",
		__func__,
		resource->name,
		depends_on->name);
bail:
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
	unsigned long flags;
	struct ipa_rm_resource *consumer;
	int consumer_result;
	IPADBG("IPA RM ::ipa_rm_resource_producer_request [%d] ENTER\n",
			producer->resource.name);
	if (ipa_rm_peers_list_is_empty(producer->resource.peers_list)) {
		spin_lock_irqsave(&producer->resource.state_lock, flags);
		producer->resource.state = IPA_RM_GRANTED;
		(void) ipa_rm_wq_send_cmd(IPA_RM_WQ_NOTIFY_PROD,
			producer->resource.name,
			IPA_RM_RESOURCE_GRANTED,
			true);
		result = 0;
		goto unlock_and_bail;
	}
	spin_lock_irqsave(&producer->resource.state_lock, flags);
	IPADBG("IPA RM ::ipa_rm_resource_producer_request state [%d]\n",
			producer->resource.state);
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
	spin_unlock_irqrestore(&producer->resource.state_lock, flags);
	for (peers_index = 0;
		peers_index < ipa_rm_peers_list_get_size(
				producer->resource.peers_list);
		peers_index++) {
		consumer = ipa_rm_peers_list_get_resource(peers_index,
				producer->resource.peers_list);
		if (consumer) {
			spin_lock_irqsave(
				&producer->resource.state_lock, flags);
			producer->pending_request++;
			spin_unlock_irqrestore(
				&producer->resource.state_lock, flags);
			consumer_result = ipa_rm_resource_consumer_request(
				(struct ipa_rm_resource_cons *)consumer);
			if (consumer_result == -EINPROGRESS) {
				result = -EINPROGRESS;
			} else {
				spin_lock_irqsave(
					&producer->resource.state_lock, flags);
				producer->pending_request--;
				spin_unlock_irqrestore(
					&producer->resource.state_lock, flags);
				if (consumer_result != 0) {
					result = consumer_result;
					goto bail;
				}
			}
		}
	}
	spin_lock_irqsave(&producer->resource.state_lock, flags);
	if (producer->pending_request == 0) {
		producer->resource.state = IPA_RM_GRANTED;
		(void) ipa_rm_wq_send_cmd(IPA_RM_WQ_NOTIFY_PROD,
			producer->resource.name,
			IPA_RM_RESOURCE_GRANTED,
			true);
		result = 0;
	}
unlock_and_bail:
	spin_unlock_irqrestore(&producer->resource.state_lock, flags);
bail:
	IPADBG("IPA RM ::ipa_rm_resource_producer_request EXIT[%d]\n", result);
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
	unsigned long flags;
	struct ipa_rm_resource *consumer;
	int consumer_result;
	IPADBG("IPA RM: %s name %d ENTER\n",
			__func__,
			producer->resource.name);
	if (ipa_rm_peers_list_is_empty(producer->resource.peers_list)) {
		spin_lock_irqsave(&producer->resource.state_lock, flags);
		producer->resource.state = IPA_RM_RELEASED;
		(void) ipa_rm_wq_send_cmd(IPA_RM_WQ_NOTIFY_PROD,
			producer->resource.name,
			IPA_RM_RESOURCE_RELEASED,
			true);
		spin_unlock_irqrestore(&producer->resource.state_lock, flags);
		return 0;
	}
	spin_lock_irqsave(&producer->resource.state_lock, flags);
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
	spin_unlock_irqrestore(&producer->resource.state_lock, flags);
	for (peers_index = 0;
		peers_index < ipa_rm_peers_list_get_size(
				producer->resource.peers_list);
		peers_index++) {
		consumer = ipa_rm_peers_list_get_resource(peers_index,
				producer->resource.peers_list);
		if (consumer) {
			spin_lock_irqsave(
				&producer->resource.state_lock, flags);
			producer->pending_release++;
			spin_unlock_irqrestore(
				&producer->resource.state_lock, flags);
			consumer_result = ipa_rm_resource_consumer_release(
				(struct ipa_rm_resource_cons *)consumer);
			spin_lock_irqsave(
				&producer->resource.state_lock, flags);
			producer->pending_release--;
			spin_unlock_irqrestore(
				&producer->resource.state_lock, flags);
		}
	}
	spin_lock_irqsave(&producer->resource.state_lock, flags);
	if (producer->pending_release == 0) {
		producer->resource.state = IPA_RM_RELEASED;
		(void) ipa_rm_wq_send_cmd(IPA_RM_WQ_NOTIFY_PROD,
			producer->resource.name,
			IPA_RM_RESOURCE_RELEASED,
			true);
	}
bail:
	spin_unlock_irqrestore(&producer->resource.state_lock, flags);
	IPADBG("IPA RM ::ipa_rm_resource_producer_release EXIT[%d]\n", result);
	return result;
}

static void ipa_rm_resource_producer_handle_cb(
		struct ipa_rm_resource_prod *producer,
		enum ipa_rm_event event)
{
	unsigned long flags;
	spin_lock_irqsave(&producer->resource.state_lock, flags);
	switch (producer->resource.state) {
	case IPA_RM_REQUEST_IN_PROGRESS:
		if (event != IPA_RM_RESOURCE_GRANTED)
			goto unlock_and_bail;
		if (producer->pending_request > 0) {
			producer->pending_request--;
			if (producer->pending_request == 0) {
				producer->resource.state =
						IPA_RM_GRANTED;
				spin_unlock_irqrestore(
					&producer->resource.state_lock, flags);
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
				spin_unlock_irqrestore(
					&producer->resource.state_lock, flags);
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
	spin_unlock_irqrestore(&producer->resource.state_lock, flags);
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
	unsigned long flags;
	if (!consumer)
		return;
	spin_lock_irqsave(&consumer->resource.state_lock, flags);
	switch (consumer->resource.state) {
	case IPA_RM_REQUEST_IN_PROGRESS:
		if (event == IPA_RM_RESOURCE_RELEASED)
			goto bail;
		consumer->resource.state = IPA_RM_GRANTED;
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
	spin_unlock_irqrestore(&consumer->resource.state_lock, flags);
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
	spin_unlock_irqrestore(&consumer->resource.state_lock, flags);
	return;
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

	int i, nbytes, cnt = 0;
	unsigned long flags;
	struct ipa_rm_resource *consumer;

	if (!buf || size < 0)
		return -EINVAL;
	switch (resource->name) {
	case IPA_RM_RESOURCE_BRIDGE_PROD:
		nbytes = scnprintf(buf + cnt, size - cnt,
			"BRIDGE_PROD[");
		cnt += nbytes;
		break;
	case IPA_RM_RESOURCE_A2_PROD:
		nbytes = scnprintf(buf + cnt, size - cnt,
			"A2_PROD[");
		cnt += nbytes;
		break;
	case IPA_RM_RESOURCE_USB_PROD:
			nbytes = scnprintf(buf + cnt, size - cnt,
			 "USB_PROD[");
		cnt += nbytes;
		break;
	case IPA_RM_RESOURCE_HSIC_PROD:
		nbytes = scnprintf(buf + cnt, size - cnt,
			 "HSIC_PROD[");
		cnt += nbytes;
		break;
	case IPA_RM_RESOURCE_STD_ECM_PROD:
		nbytes = scnprintf(buf + cnt, size - cnt,
			 "STD_ECM_PROD[");
		cnt += nbytes;
		break;
	case IPA_RM_RESOURCE_WWAN_0_PROD:
		nbytes = scnprintf(buf + cnt, size - cnt,
			 "WWAN_0_PROD[");
		cnt += nbytes;
		break;
	case IPA_RM_RESOURCE_WWAN_1_PROD:
		nbytes = scnprintf(buf + cnt, size - cnt,
			"WWAN_1_PROD[");
		cnt += nbytes;
		break;
	case IPA_RM_RESOURCE_WWAN_2_PROD:
		nbytes = scnprintf(buf + cnt, size - cnt,
			"WWAN_2_PROD[");
		cnt += nbytes;
		break;
	case IPA_RM_RESOURCE_WWAN_3_PROD:
		nbytes = scnprintf(buf + cnt, size - cnt,
				 "WWAN_3_PROD[");
		cnt += nbytes;
		break;
	case IPA_RM_RESOURCE_WWAN_4_PROD:
		nbytes = scnprintf(buf + cnt, size - cnt,
			"WWAN_4_PROD[");
		cnt += nbytes;
		break;
	case IPA_RM_RESOURCE_WWAN_5_PROD:
		nbytes = scnprintf(buf + cnt, size - cnt,
			 "WWAN_5_PROD[");
		cnt += nbytes;
		break;
	case IPA_RM_RESOURCE_WWAN_6_PROD:
		nbytes = scnprintf(buf + cnt, size - cnt,
			"WWAN_6_PROD[");
		cnt += nbytes;
		break;
	case IPA_RM_RESOURCE_WWAN_7_PROD:
		nbytes = scnprintf(buf + cnt, size - cnt,
			 "WWAN_7_PROD[");
		cnt += nbytes;
		break;
	case IPA_RM_RESOURCE_WLAN_PROD:
		nbytes = scnprintf(buf + cnt, size - cnt,
			 "WLAN_PROD[");
		cnt += nbytes;
		break;
	default:
		return -EPERM;
	}
	spin_lock_irqsave(&resource->state_lock, flags);
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
		spin_unlock_irqrestore(
			&resource->state_lock,
			flags);
		return -EPERM;
	}
	spin_unlock_irqrestore(
			&resource->state_lock,
			flags);
	for (i = 0; i < resource->peers_list->max_peers; ++i) {
		consumer =
			ipa_rm_peers_list_get_resource(
			i,
			resource->peers_list);
		if (consumer) {
			switch (consumer->name) {
			case IPA_RM_RESOURCE_A2_CONS:
				nbytes = scnprintf(buf + cnt,
						size - cnt,
						 " A2_CONS[");
				cnt += nbytes;
				break;
			case IPA_RM_RESOURCE_USB_CONS:
				nbytes = scnprintf(buf + cnt,
						size - cnt,
						 " USB_CONS[");
				cnt += nbytes;
				break;
			case IPA_RM_RESOURCE_HSIC_CONS:
				nbytes = scnprintf(buf + cnt,
						size - cnt,
						 " HSIC_CONS[");
				cnt += nbytes;
				break;
			default:
				return -EPERM;
			}
			spin_lock_irqsave(&consumer->state_lock, flags);
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
				spin_unlock_irqrestore(
						&consumer->state_lock,
						flags);
				return -EPERM;
			}
			spin_unlock_irqrestore(
					&consumer->state_lock,
					flags);
		}
	}
	nbytes = scnprintf(buf + cnt, size - cnt,
			 "\n");
	cnt += nbytes;
	return cnt;
}
