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
#include <linux/workqueue.h>
#include <linux/ipa.h>
#include "ipa_i.h"
#include "ipa_rm_dependency_graph.h"
#include "ipa_rm_i.h"
#include "ipa_rm_resource.h"

static const char *resource_name_to_str[IPA_RM_RESOURCE_MAX] = {
	__stringify(IPA_RM_RESOURCE_BRIDGE_PROD),
	__stringify(IPA_RM_RESOURCE_A2_PROD),
	__stringify(IPA_RM_RESOURCE_USB_PROD),
	__stringify(IPA_RM_RESOURCE_HSIC_PROD),
	__stringify(IPA_RM_RESOURCE_STD_ECM_PROD),
	__stringify(IPA_RM_RESOURCE_RNDIS_PROD),
	__stringify(IPA_RM_RESOURCE_WWAN_0_PROD),
	__stringify(IPA_RM_RESOURCE_WWAN_1_PROD),
	__stringify(IPA_RM_RESOURCE_WWAN_2_PROD),
	__stringify(IPA_RM_RESOURCE_WWAN_3_PROD),
	__stringify(IPA_RM_RESOURCE_WWAN_4_PROD),
	__stringify(IPA_RM_RESOURCE_WWAN_5_PROD),
	__stringify(IPA_RM_RESOURCE_WWAN_6_PROD),
	__stringify(IPA_RM_RESOURCE_WWAN_7_PROD),
	__stringify(IPA_RM_RESOURCE_WLAN_PROD),
	__stringify(IPA_RM_RESOURCE_A2_CONS),
	__stringify(IPA_RM_RESOURCE_USB_CONS),
	__stringify(IPA_RM_RESOURCE_HSIC_CONS),
};

struct ipa_rm_context_type {
	struct ipa_rm_dep_graph *dep_graph;
	struct workqueue_struct *ipa_rm_wq;
	spinlock_t ipa_rm_lock;
};
static struct ipa_rm_context_type *ipa_rm_ctx;

/**
 * ipa_rm_create_resource() - create resource
 * @create_params: [in] parameters needed
 *                  for resource initialization
 *
 * Returns: 0 on success, negative on failure
 *
 * This function is called by IPA RM client to initialize client's resources.
 * This API should be called before any other IPA RM API on a given resource
 * name.
 *
 */
int ipa_rm_create_resource(struct ipa_rm_create_params *create_params)
{
	struct ipa_rm_resource *resource;
	int result;

	if (!create_params) {
		IPA_RM_ERR("invalid args\n");
		return -EINVAL;
	}
	IPA_RM_DBG("%s\n", ipa_rm_resource_str(create_params->name));

	spin_lock(&ipa_rm_ctx->ipa_rm_lock);
	if (ipa_rm_dep_graph_get_resource(ipa_rm_ctx->dep_graph,
					  create_params->name,
					  &resource) == 0) {
		IPA_RM_ERR("resource already exists\n");
		result = -EEXIST;
		goto bail;
	}
	result = ipa_rm_resource_create(create_params,
			&resource);
	if (result) {
		IPA_RM_ERR("ipa_rm_resource_create() failed\n");
		goto bail;
	}
	result = ipa_rm_dep_graph_add(ipa_rm_ctx->dep_graph, resource);
	if (result) {
		IPA_RM_ERR("ipa_rm_dep_graph_add() failed\n");
		ipa_rm_resource_delete(resource);
		goto bail;
	}
bail:
	spin_unlock(&ipa_rm_ctx->ipa_rm_lock);
	IPA_RM_DBG("EXIT with %d\n", result);

	return result;
}
EXPORT_SYMBOL(ipa_rm_create_resource);

/**
 * ipa_rm_delete_resource() - delete resource
 * @resource_name: name of resource to be deleted
 *
 * Returns: 0 on success, negative on failure
 *
 * This function is called by IPA RM client to delete client's resources.
 *
 */
int ipa_rm_delete_resource(enum ipa_rm_resource_name resource_name)
{
	struct ipa_rm_resource *resource;
	int result;

	IPA_RM_DBG("%s\n", ipa_rm_resource_str(resource_name));
	spin_lock(&ipa_rm_ctx->ipa_rm_lock);
	if (ipa_rm_dep_graph_get_resource(ipa_rm_ctx->dep_graph,
					resource_name,
						&resource) != 0) {
		IPA_RM_ERR("resource does not exist\n");
		result = -EINVAL;
		goto bail;
	}
	result = ipa_rm_resource_delete(resource);
	if (result) {
		IPA_RM_ERR("ipa_rm_resource_delete() failed\n");
		goto bail;
	}
	result = ipa_rm_dep_graph_remove(ipa_rm_ctx->dep_graph,
								resource_name);
	if (result) {
		IPA_RM_ERR("ipa_rm_dep_graph_remove() failed\n");
		goto bail;
	}
bail:
	spin_unlock(&ipa_rm_ctx->ipa_rm_lock);
	IPA_RM_DBG("EXIT with %d\n", result);

	return result;
}
EXPORT_SYMBOL(ipa_rm_delete_resource);

/**
 * ipa_rm_add_dependency() - create dependency
 *					between 2 resources
 * @resource_name: name of dependent resource
 * @depends_on_name: name of its dependency
 *
 * Returns: 0 on success, negative on failure
 *
 * Side effects: IPA_RM_RESORCE_GRANTED could be generated
 * in case client registered with IPA RM
 */
int ipa_rm_add_dependency(enum ipa_rm_resource_name resource_name,
			enum ipa_rm_resource_name depends_on_name)
{
	int result;

	IPA_RM_DBG("%s -> %s\n", ipa_rm_resource_str(resource_name),
				 ipa_rm_resource_str(depends_on_name));
	spin_lock(&ipa_rm_ctx->ipa_rm_lock);
	result = ipa_rm_dep_graph_add_dependency(
						ipa_rm_ctx->dep_graph,
						resource_name,
						depends_on_name);
	spin_unlock(&ipa_rm_ctx->ipa_rm_lock);
	IPA_RM_DBG("EXIT with %d\n", result);

	return result;
}
EXPORT_SYMBOL(ipa_rm_add_dependency);


/**
 * ipa_rm_delete_dependency() - create dependency
 *					between 2 resources
 * @resource_name: name of dependent resource
 * @depends_on_name: name of its dependency
 *
 * Returns: 0 on success, negative on failure
 *
 * Side effects: IPA_RM_RESORCE_GRANTED could be generated
 * in case client registered with IPA RM
 */
int ipa_rm_delete_dependency(enum ipa_rm_resource_name resource_name,
			enum ipa_rm_resource_name depends_on_name)
{
	int result;

	IPA_RM_DBG("%s -> %s\n", ipa_rm_resource_str(resource_name),
				 ipa_rm_resource_str(depends_on_name));
	spin_lock(&ipa_rm_ctx->ipa_rm_lock);
	result = ipa_rm_dep_graph_delete_dependency(
			  ipa_rm_ctx->dep_graph,
			  resource_name,
			  depends_on_name);
	spin_unlock(&ipa_rm_ctx->ipa_rm_lock);
	IPA_RM_DBG("EXIT with %d\n", result);

	return result;
}
EXPORT_SYMBOL(ipa_rm_delete_dependency);

/**
 * ipa_rm_request_resource() - request resource
 * @resource_name: [in] name of the requested resource
 *
 * Returns: 0 on success, negative on failure
 *
 * All registered callbacks are called with IPA_RM_RESOURCE_GRANTED
 * on successful completion of this operation.
 */
int ipa_rm_request_resource(enum ipa_rm_resource_name resource_name)
{
	struct ipa_rm_resource *resource;
	int result;

	if (!IPA_RM_RESORCE_IS_PROD(resource_name)) {
		IPA_RM_ERR("can be called on PROD only\n");
		return -EINVAL;
	}
	spin_lock(&ipa_rm_ctx->ipa_rm_lock);
	if (ipa_rm_dep_graph_get_resource(ipa_rm_ctx->dep_graph,
			resource_name,
			&resource) != 0) {
		IPA_RM_ERR("resource does not exists\n");
		result = -EPERM;
		goto bail;
	}
	result = ipa_rm_resource_producer_request(
			(struct ipa_rm_resource_prod *)resource);

bail:
	spin_unlock(&ipa_rm_ctx->ipa_rm_lock);

	return result;
}
EXPORT_SYMBOL(ipa_rm_request_resource);

/**
 * ipa_rm_release_resource() - release resource
 * @resource_name: [in] name of the requested resource
 *
 * Returns: 0 on success, negative on failure
 *
 * All registered callbacks are called with IPA_RM_RESOURCE_RELEASED
 * on successful completion of this operation.
 */
int ipa_rm_release_resource(enum ipa_rm_resource_name resource_name)
{
	struct ipa_rm_resource *resource;
	int result;

	if (!IPA_RM_RESORCE_IS_PROD(resource_name)) {
		IPA_RM_ERR("can be called on PROD only\n");
		return -EINVAL;
	}
	spin_lock(&ipa_rm_ctx->ipa_rm_lock);
	if (ipa_rm_dep_graph_get_resource(ipa_rm_ctx->dep_graph,
					  resource_name,
					  &resource) != 0) {
		IPA_RM_ERR("resource does not exists\n");
		result = -EPERM;
		goto bail;
	}
	result = ipa_rm_resource_producer_release(
		    (struct ipa_rm_resource_prod *)resource);

bail:
	spin_unlock(&ipa_rm_ctx->ipa_rm_lock);

	return result;
}
EXPORT_SYMBOL(ipa_rm_release_resource);

/**
 * ipa_rm_register() - register for event
 * @resource_name: resource name
 * @reg_params: [in] registration parameters
 *
 * Returns: 0 on success, negative on failure
 *
 * Registration parameters provided here should be the same
 * as provided later in  ipa_rm_deregister() call.
 */
int ipa_rm_register(enum ipa_rm_resource_name resource_name,
			struct ipa_rm_register_params *reg_params)
{
	int result;
	struct ipa_rm_resource *resource;

	IPA_RM_DBG("%s\n", ipa_rm_resource_str(resource_name));

	if (!IPA_RM_RESORCE_IS_PROD(resource_name)) {
		IPA_RM_ERR("can be called on PROD only\n");
		return -EINVAL;
	}
	spin_lock(&ipa_rm_ctx->ipa_rm_lock);
	if (ipa_rm_dep_graph_get_resource(ipa_rm_ctx->dep_graph,
				resource_name,
				&resource) != 0) {
		IPA_RM_ERR("resource does not exists\n");
		result = -EPERM;
		goto bail;
	}
	result = ipa_rm_resource_producer_register(
			(struct ipa_rm_resource_prod *)resource,
			reg_params,
			true);
bail:
	spin_unlock(&ipa_rm_ctx->ipa_rm_lock);
	IPA_RM_DBG("EXIT with %d\n", result);

	return result;
}
EXPORT_SYMBOL(ipa_rm_register);

/**
 * ipa_rm_deregister() - cancel the registration
 * @resource_name: resource name
 * @reg_params: [in] registration parameters
 *
 * Returns: 0 on success, negative on failure
 *
 * Registration parameters provided here should be the same
 * as provided in  ipa_rm_register() call.
 */
int ipa_rm_deregister(enum ipa_rm_resource_name resource_name,
			struct ipa_rm_register_params *reg_params)
{
	int result;
	struct ipa_rm_resource *resource;

	IPA_RM_DBG("%s\n", ipa_rm_resource_str(resource_name));

	if (!IPA_RM_RESORCE_IS_PROD(resource_name)) {
		IPA_RM_ERR("can be called on PROD only\n");
		return -EINVAL;
	}
	spin_lock(&ipa_rm_ctx->ipa_rm_lock);
	if (ipa_rm_dep_graph_get_resource(ipa_rm_ctx->dep_graph,
			resource_name,
			&resource) != 0) {
		IPA_RM_ERR("resource does not exists\n");
		result = -EPERM;
		goto bail;
	}
	result = ipa_rm_resource_producer_deregister(
			(struct ipa_rm_resource_prod *)resource,
			reg_params);
bail:
	spin_unlock(&ipa_rm_ctx->ipa_rm_lock);
	IPA_RM_DBG("EXIT with %d\n", result);

	return result;
}
EXPORT_SYMBOL(ipa_rm_deregister);

/**
 * ipa_rm_notify_completion() -
 *	consumer driver notification for
 *	request_resource / release_resource operations
 *	completion
 * @event: notified event
 * @resource_name: resource name
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_rm_notify_completion(enum ipa_rm_event event,
		enum ipa_rm_resource_name resource_name)
{
	int result;

	IPA_RM_DBG("event %d on %s\n", event,
				ipa_rm_resource_str(resource_name));
	if (!IPA_RM_RESORCE_IS_CONS(resource_name)) {
		IPA_RM_ERR("can be called on CONS only\n");
		result = -EINVAL;
		goto bail;
	}
	ipa_rm_wq_send_cmd(IPA_RM_WQ_RESOURCE_CB,
			resource_name,
			event,
			false);
	result = 0;
bail:
	IPA_RM_DBG("EXIT with %d\n", result);

	return result;
}
EXPORT_SYMBOL(ipa_rm_notify_completion);

static void ipa_rm_wq_handler(struct work_struct *work)
{
	struct ipa_rm_resource *resource;
	struct ipa_rm_wq_work_type *ipa_rm_work =
			container_of(work,
					struct ipa_rm_wq_work_type,
					work);
	IPA_RM_DBG("%s cmd=%d event=%d notify_registered_only=%d\n",
		ipa_rm_resource_str(ipa_rm_work->resource_name),
		ipa_rm_work->wq_cmd,
		ipa_rm_work->event,
		ipa_rm_work->notify_registered_only);
	switch (ipa_rm_work->wq_cmd) {
	case IPA_RM_WQ_NOTIFY_PROD:
		if (!IPA_RM_RESORCE_IS_PROD(ipa_rm_work->resource_name)) {
			IPA_RM_ERR("resource is not PROD\n");
			return;
		}
		spin_lock(&ipa_rm_ctx->ipa_rm_lock);
		if (ipa_rm_dep_graph_get_resource(ipa_rm_ctx->dep_graph,
						ipa_rm_work->resource_name,
						&resource) != 0){
			IPA_RM_ERR("resource does not exists\n");
			spin_unlock(&ipa_rm_ctx->ipa_rm_lock);
			return;
		}
		ipa_rm_resource_producer_notify_clients(
				(struct ipa_rm_resource_prod *)resource,
				ipa_rm_work->event,
				ipa_rm_work->notify_registered_only);
		spin_unlock(&ipa_rm_ctx->ipa_rm_lock);
		break;
	case IPA_RM_WQ_NOTIFY_CONS:
		break;
	case IPA_RM_WQ_RESOURCE_CB:
		spin_lock(&ipa_rm_ctx->ipa_rm_lock);
		if (ipa_rm_dep_graph_get_resource(ipa_rm_ctx->dep_graph,
						ipa_rm_work->resource_name,
						&resource) != 0){
			IPA_RM_ERR("resource does not exists\n");
			spin_unlock(&ipa_rm_ctx->ipa_rm_lock);
			return;
		}
		ipa_rm_resource_consumer_handle_cb(
				(struct ipa_rm_resource_cons *)resource,
				ipa_rm_work->event);
		spin_unlock(&ipa_rm_ctx->ipa_rm_lock);
		break;
	default:
		break;
	}

	kfree((void *) work);
}

/**
 * ipa_rm_wq_send_cmd() - send a command for deferred work
 * @wq_cmd: command that should be executed
 * @resource_name: resource on which command should be executed
 * @notify_registered_only: notify only clients registered by
 *	ipa_rm_register()
 *
 * Returns: 0 on success, negative otherwise
 */
int ipa_rm_wq_send_cmd(enum ipa_rm_wq_cmd wq_cmd,
		enum ipa_rm_resource_name resource_name,
		enum ipa_rm_event event,
		bool notify_registered_only)
{
	int result = -ENOMEM;
	struct ipa_rm_wq_work_type *work = kzalloc(sizeof(*work), GFP_KERNEL);
	if (work) {
		INIT_WORK((struct work_struct *)work, ipa_rm_wq_handler);
		work->wq_cmd = wq_cmd;
		work->resource_name = resource_name;
		work->event = event;
		work->notify_registered_only = notify_registered_only;
		result = queue_work(ipa_rm_ctx->ipa_rm_wq,
				(struct work_struct *)work);
	} else {
		IPA_RM_ERR("no mem\n");
	}

	return result;
}

/**
 * ipa_rm_initialize() - initialize IPA RM component
 *
 * Returns: 0 on success, negative otherwise
 */
int ipa_rm_initialize(void)
{
	int result;

	ipa_rm_ctx = kzalloc(sizeof(*ipa_rm_ctx), GFP_KERNEL);
	if (!ipa_rm_ctx) {
		IPA_RM_ERR("no mem\n");
		result = -ENOMEM;
		goto bail;
	}
	ipa_rm_ctx->ipa_rm_wq = create_singlethread_workqueue("ipa_rm_wq");
	if (!ipa_rm_ctx->ipa_rm_wq) {
		IPA_RM_ERR("create workqueue failed\n");
		result = -ENOMEM;
		goto create_wq_fail;
	}
	result = ipa_rm_dep_graph_create(&(ipa_rm_ctx->dep_graph));
	if (result) {
		IPA_RM_ERR("create dependency graph failed\n");
		goto graph_alloc_fail;
	}
	spin_lock_init(&ipa_rm_ctx->ipa_rm_lock);
	IPA_RM_DBG("SUCCESS\n");

	return 0;
graph_alloc_fail:
	destroy_workqueue(ipa_rm_ctx->ipa_rm_wq);
create_wq_fail:
	kfree(ipa_rm_ctx);
bail:
	return result;
}

/**
 * ipa_rm_stat() - print RM stat
 * @buf: [in] The user buff used to print
 * @size: [in] The size of buf
 * Returns: number of bytes used on success, negative on failure
 *
 * This function is called by ipa_debugfs in order to receive
 * a full picture of the current state of the RM
 */

int ipa_rm_stat(char *buf, int size)
{
	int i, cnt = 0, result = EINVAL;
	struct ipa_rm_resource *resource = NULL;

	if (!buf || size < 0)
		goto bail;

	spin_lock(&ipa_rm_ctx->ipa_rm_lock);
	for (i = 0; i < IPA_RM_RESOURCE_PROD_MAX; ++i) {
		result = ipa_rm_dep_graph_get_resource(
				ipa_rm_ctx->dep_graph,
				i,
				&resource);
		if (!result) {
			result = ipa_rm_resource_producer_print_stat(
							resource, buf + cnt,
							size-cnt);
			if (result < 0)
				goto bail;
			cnt += result;
		}
	}
	result = cnt;
bail:
	spin_unlock(&ipa_rm_ctx->ipa_rm_lock);

	return result;
}

/**
* ipa_rm_resource_str() - returns string that represent the resource
* @resource_name: [in] resource name
*/
const char *ipa_rm_resource_str(enum ipa_rm_resource_name resource_name)
{
	if (resource_name < 0 || resource_name >= IPA_RM_RESOURCE_MAX)
		return "INVALID RESOURCE";

	return resource_name_to_str[resource_name];
};

/**
 * ipa_rm_exit() - free all IPA RM resources
 */
void ipa_rm_exit(void)
{
	IPA_RM_DBG("ENTER\n");
	ipa_rm_dep_graph_delete(ipa_rm_ctx->dep_graph);
	destroy_workqueue(ipa_rm_ctx->ipa_rm_wq);
	kfree(ipa_rm_ctx);
	ipa_rm_ctx = NULL;
	IPA_RM_DBG("EXIT\n");
}
