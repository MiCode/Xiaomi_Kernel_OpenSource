/* Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
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

#ifndef _IPA_RM_RESOURCE_H_
#define _IPA_RM_RESOURCE_H_

#include <linux/list.h>
#include <linux/ipa.h>
#include "ipa_rm_peers_list.h"

/**
 * enum ipa_rm_resource_state - resource state
 */
enum ipa_rm_resource_state {
	IPA_RM_RELEASED,
	IPA_RM_REQUEST_IN_PROGRESS,
	IPA_RM_GRANTED,
	IPA_RM_RELEASE_IN_PROGRESS
};

/**
 * enum ipa_rm_resource_type - IPA resource manager resource type
 */
enum ipa_rm_resource_type {
	IPA_RM_PRODUCER,
	IPA_RM_CONSUMER
};

/**
 * struct ipa_rm_notification_info - notification information
 *				of IPA RM client
 * @reg_params: registration parameters
 * @explicit: registered explicitly by ipa_rm_register()
 * @link: link to the list of all registered clients information
 */
struct ipa_rm_notification_info {
	struct ipa_rm_register_params	reg_params;
	bool				explicit;
	struct list_head		link;
};

/**
 * struct ipa_rm_resource - IPA RM resource
 * @name: name identifying resource
 * @type: type of resource (PRODUCER or CONSUMER)
 * @floor_voltage: minimum voltage level for operation
 * @max_bw: maximum bandwidth required for resource in Mbps
 * @state: state of the resource
 * @peers_list: list of the peers of the resource
 */
struct ipa_rm_resource {
	enum ipa_rm_resource_name	name;
	enum ipa_rm_resource_type	type;
	enum ipa_voltage_level		floor_voltage;
	u32				max_bw;
	u32				needed_bw;
	enum ipa_rm_resource_state	state;
	struct ipa_rm_peers_list	*peers_list;
};

/**
 * struct ipa_rm_resource_cons - IPA RM consumer
 * @resource: resource
 * @usage_count: number of producers in GRANTED / REQUESTED state
 *		using this consumer
 * @request_consumer_in_progress: when set, the consumer is during its request
 *		phase
 * @request_resource: function which should be called to request resource
 *			from resource manager
 * @release_resource: function which should be called to release resource
 *			from resource manager
 * Add new fields after @resource only.
 */
struct ipa_rm_resource_cons {
	struct ipa_rm_resource resource;
	int usage_count;
	struct completion request_consumer_in_progress;
	int (*request_resource)(void);
	int (*release_resource)(void);
};

/**
 * struct ipa_rm_resource_prod - IPA RM producer
 * @resource: resource
 * @event_listeners: clients registered with this producer
 *		for notifications in resource state
 * list Add new fields after @resource only.
 */
struct ipa_rm_resource_prod {
	struct ipa_rm_resource	resource;
	struct list_head	event_listeners;
	int			pending_request;
	int			pending_release;
};

int ipa_rm_resource_create(
		struct ipa_rm_create_params *create_params,
		struct ipa_rm_resource **resource);

int ipa_rm_resource_delete(struct ipa_rm_resource *resource);

int ipa_rm_resource_producer_register(struct ipa_rm_resource_prod *producer,
				struct ipa_rm_register_params *reg_params,
				bool explicit);

int ipa_rm_resource_producer_deregister(struct ipa_rm_resource_prod *producer,
				struct ipa_rm_register_params *reg_params);

int ipa_rm_resource_add_dependency(struct ipa_rm_resource *resource,
				   struct ipa_rm_resource *depends_on);

int ipa_rm_resource_delete_dependency(struct ipa_rm_resource *resource,
				      struct ipa_rm_resource *depends_on);

int ipa_rm_resource_producer_request(struct ipa_rm_resource_prod *producer);

int ipa_rm_resource_producer_release(struct ipa_rm_resource_prod *producer);

int ipa_rm_resource_consumer_request(struct ipa_rm_resource_cons *consumer,
				u32 needed_bw,
				bool inc_usage_count);

int ipa_rm_resource_consumer_release(struct ipa_rm_resource_cons *consumer,
				u32 needed_bw,
				bool dec_usage_count);

int ipa_rm_resource_set_perf_profile(struct ipa_rm_resource *resource,
				     struct ipa_rm_perf_profile *profile);

void ipa_rm_resource_consumer_handle_cb(struct ipa_rm_resource_cons *consumer,
				enum ipa_rm_event event);

void ipa_rm_resource_producer_notify_clients(
				struct ipa_rm_resource_prod *producer,
				enum ipa_rm_event event,
				bool notify_registered_only);

int ipa_rm_resource_producer_print_stat(
		struct ipa_rm_resource *resource,
		char *buf,
		int size);

int ipa_rm_resource_consumer_request_work(struct ipa_rm_resource_cons *consumer,
		enum ipa_rm_resource_state prev_state,
		u32 needed_bw,
		bool notify_completion);

int ipa_rm_resource_consumer_release_work(
		struct ipa_rm_resource_cons *consumer,
		enum ipa_rm_resource_state prev_state,
		bool notify_completion);

#endif /* _IPA_RM_RESOURCE_H_ */
