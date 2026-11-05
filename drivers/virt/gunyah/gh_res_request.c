// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#include <linux/gunyah/gh_msgq.h>
#include <linux/gunyah/gh_res_request.h>

static void *msgq_desc;
static struct task_struct *gh_resource_thread;
static DEFINE_MUTEX(res_mutex);
static LIST_HEAD(gh_resource_client_list);

/**
 * gh_resource_register_req_client: Register a request client callback with
 * specific subsystem name.
 * @client: Client struct with callback function and subsystem name
 *
 * The function returns -EINVAL if the caller passes invalid arguments.
 */
int gh_resource_register_req_client(struct gh_resource_client *client)
{
	if (!client || !client->cb)
		return -EINVAL;

	mutex_lock(&res_mutex);
	list_add(&client->list, &gh_resource_client_list);
	mutex_unlock(&res_mutex);
	pr_debug("%s: Subsys:%s register req client\n", __func__,
		 client->subsys_name);

	return 0;
}
EXPORT_SYMBOL_GPL(gh_resource_register_req_client);

/**
 * gh_resource_unregister_req_client: Unregister a request client callback.
 * @client: Client struct which is already registered
 *
 * The function returns -EINVAL if the caller passes invalid arguments.
 */
int gh_resource_unregister_req_client(struct gh_resource_client *client)
{
	if (!client)
		return -EINVAL;

	mutex_lock(&res_mutex);
	list_del(&client->list);
	mutex_unlock(&res_mutex);
	pr_debug("%s: Subsys:%s unregister req client\n", __func__,
		 client->subsys_name);

	return 0;
}
EXPORT_SYMBOL_GPL(gh_resource_unregister_req_client);

/**
 * gh_resource_register_release_client: Register a release client callback with
 * specific subsystem name.
 * @client: Client struct with callback function and subsystem name
 *
 * The function returns -EINVAL if the caller passes invalid arguments.
 */
int gh_resource_register_release_client(struct gh_resource_client *client)
{
	if (!client || !client->cb)
		return -EINVAL;

	mutex_lock(&res_mutex);
	list_add(&client->list, &gh_resource_client_list);
	mutex_unlock(&res_mutex);
	pr_debug("%s: Subsys:%s register release client\n", __func__,
		 client->subsys_name);

	return 0;
}
EXPORT_SYMBOL_GPL(gh_resource_register_release_client);

/**
 * gh_resource_unregister_req_client: Unregister a release client callback.
 * @client: Client struct which is already registered
 *
 * The function returns -EINVAL if the caller passes invalid arguments.
 */
int gh_resource_unregister_release_client(struct gh_resource_client *client)
{
	if (!client)
		return -EINVAL;

	mutex_lock(&res_mutex);
	list_del(&client->list);
	mutex_unlock(&res_mutex);
	pr_debug("%s: Subsys:%s unregister release client\n", __func__,
		 client->subsys_name);

	return 0;
}
EXPORT_SYMBOL_GPL(gh_resource_unregister_release_client);

static int send_request(const char *subsys_name,
			struct gh_res_request *req_resource, uint8_t res_cnt,
			bool is_req)
{
	struct gh_resource_payload *payload;
	size_t buf_size;
	gh_vmid_t vmid;
	int ret;

	buf_size = struct_size(payload, resource, res_cnt);
	payload = kzalloc(buf_size, GFP_KERNEL);
	if (!payload)
		return -ENOMEM;
	payload->is_req = is_req;
	gh_rm_get_this_vmid(&vmid);
	payload->source_vmid = vmid;
	payload->resource_cnt = res_cnt;
	memcpy(payload->resource, req_resource,
	       sizeof(struct gh_res_request) * res_cnt);
	strscpy(payload->subsys_name, subsys_name,
		sizeof(payload->subsys_name));
	ret = gh_msgq_send(msgq_desc, payload, buf_size, 0);
	if (ret < 0)
		pr_err("%s: Send request failed\n", __func__);
	kfree(payload);

	return ret;
}

/**
 * gh_resource_request: Request resource for specific subsystem from PVM.
 * @target_vmid: Target VMID to receive the request
 * @subsys_name: Subsystem name to receive the request, only callback with same
 * subsystem name will receive
 * @req_resource: List of resources which need to request
 * @res_cnt: Count of resources which need to request
 *
 * The function returns < 0 if failed to send the request.
 */
int gh_resource_request(gh_vmid_t target_vmid, const char *subsys_name,
			struct gh_res_request *req_resource, uint8_t res_cnt)
{
	int ret;

	if (res_cnt > RESOURCE_CNT_MAX)
		return -EINVAL;

	ret = send_request(subsys_name, req_resource, res_cnt, true);

	return ret;
}
EXPORT_SYMBOL_GPL(gh_resource_request);

/**
 * gh_resource_release: Release resource for specific subsystem from PVM.
 * @target_vmid: Target VMID to release the resource
 * @subsys_name: Subsystem name to release the resource, only callback with same
 * subsystem name will receive
 * @req_resource: List of resources which need to release
 * @res_cnt: Count of resources which need to release
 *
 * The function returns < 0 if failed to send the request.
 */
int gh_resource_release(gh_vmid_t target_vmid, const char *subsys_name,
			struct gh_res_request *release_resource, uint8_t res_cnt)
{
	int ret;

	if (res_cnt > RESOURCE_CNT_MAX)
		return -EINVAL;

	ret = send_request(subsys_name, release_resource, res_cnt, false);

	return ret;
}
EXPORT_SYMBOL_GPL(gh_resource_release);

static int gh_resource_msgq_handle(void *data)
{
	struct gh_resource_payload *payload;
	void *rec_buf;
	size_t buf_size, recv_size;
	struct gh_resource_client *client;

	buf_size = struct_size(payload, resource, RESOURCE_CNT_MAX);
	rec_buf = kmalloc(buf_size, GFP_KERNEL);
	if (!rec_buf) {
		pr_err("%s: Fail to allocate msgq recv buffer\n", __func__);
		return -ENOMEM;
	}
	while (!kthread_should_stop()) {
		if (gh_msgq_recv(msgq_desc, rec_buf, buf_size, &recv_size, 0)) {
			pr_err("%s: recv msgq failed\n", __func__);
			return 0;
		}
		payload = rec_buf;

		mutex_lock(&res_mutex);
		list_for_each_entry(client, &gh_resource_client_list, list) {
			if (!strcmp(client->subsys_name,
				    payload->subsys_name)) {
				if (!client->cb) {
					pr_err("%s: NULL callback for subsys:%s\n",
					       __func__, client->subsys_name);
					continue;
				} else {
					client->cb(payload->source_vmid,
						   payload->is_req,
						   payload->resource,
						   payload->resource_cnt);
				}
			}
		}
		mutex_unlock(&res_mutex);
	}

	return 0;
}

static int __init gh_resource_init(void)
{
	msgq_desc = gh_msgq_register(GH_MSGQ_LABEL_RESOURCE_REQUEST);
	if (IS_ERR(msgq_desc)) {
		pr_err("%s: Failed to register msgq\n", __func__);
		return PTR_ERR(msgq_desc);
	}
	gh_resource_thread = kthread_run(gh_resource_msgq_handle, NULL,
					 "gh_resource_thread");
	if (IS_ERR(gh_resource_thread)) {
		pr_err("%s: Failed to start thread\n", __func__);
		return PTR_ERR(gh_resource_thread);
	}

	return 0;
}
module_init(gh_resource_init);

static void __exit gh_resource_exit(void)
{
	if (msgq_desc)
		gh_msgq_unregister(msgq_desc);
	kthread_stop(gh_resource_thread);
}
module_exit(gh_resource_exit);

MODULE_DESCRIPTION("Qualcomm Technologies, Inc. Gunyah Resource request Driver");
MODULE_LICENSE("GPL");
