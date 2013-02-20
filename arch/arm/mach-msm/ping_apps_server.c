/* Copyright (c) 2010, The Linux Foundation. All rights reserved.
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

/*
 * PING APPS SERVER Driver
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/kthread.h>
#include <linux/workqueue.h>
#include <mach/msm_rpcrouter.h>

/* ping server definitions */

#define PING_APPS_PROG 0x30000082
#define PING_APPS_VERS 0x00010001

#define PING_APPS_NULL  0
#define PING_APPS_DATA  4
#define PING_APPS_REG   2
#define PING_APPS_UNREG 3
#define PING_APPS_DATA_CB_REG    6
#define PING_APPS_DATA_CB_UNREG  5

#define PING_APPS_REG_CB  2
#define PING_APPS_DATA_CB 1

static LIST_HEAD(cb_entry_list);
static DEFINE_MUTEX(cb_entry_list_lock);

static struct task_struct *server_thread;

struct ping_apps_register_arg {
	uint32_t cb_id;
	int32_t num;
};

struct ping_apps_unregister_arg {
	uint32_t cb_id;
};

struct ping_apps_register_cb_arg {
	uint32_t cb_id;
	int32_t num;
};

struct ping_apps_register_ret {
	uint32_t result;
};

struct ping_apps_unregister_ret {
	uint32_t result;
};

struct ping_apps_data_cb_reg_arg {
	uint32_t cb_id;
	uint32_t num;
	uint32_t size;
	uint32_t interval_ms;
	uint32_t num_tasks;
};

struct ping_apps_data_cb_unreg_arg {
	uint32_t cb_id;
};

struct ping_apps_data_cb_arg {
	uint32_t cb_id;
	uint32_t *data;
	uint32_t size;
	uint32_t sum;
};

struct ping_apps_data_cb_reg_ret {
	uint32_t result;
};

struct ping_apps_data_cb_unreg_ret {
	uint32_t result;
};

struct ping_apps_data_cb_ret {
	uint32_t result;
};

struct ping_apps_data_arg {
	uint32_t *data;
	uint32_t size;
};

struct ping_apps_data_ret {
	uint32_t result;
};

struct ping_apps_data_cb_info {
	void *cb_func;
	uint32_t size;
	uint32_t num_tasks;
};

struct ping_apps_cb_entry {
	struct list_head list;

	struct msm_rpc_client_info clnt_info;
	void *cb_info;
	uint32_t cb_id;
	int32_t num;
	uint32_t interval_ms;
	uint32_t time_to_next_cb;
	void (*cb_func)(struct ping_apps_cb_entry *);
};

static int handle_rpc_call(struct msm_rpc_server *server,
			   struct rpc_request_hdr *req,
			   struct msm_rpc_xdr *xdr);

static int ping_apps_data_cb(struct msm_rpc_server *server,
			     struct msm_rpc_client_info *clnt_info,
			     struct ping_apps_data_cb_arg *arg,
			     struct ping_apps_data_cb_ret *ret);

static int ping_apps_register_cb(struct msm_rpc_server *server,
				 struct msm_rpc_client_info *clnt_info,
				 struct ping_apps_register_cb_arg *arg,
				 void *ret);

static struct msm_rpc_server rpc_server = {
	.prog = PING_APPS_PROG,
	.vers = PING_APPS_VERS,
	.rpc_call2 = handle_rpc_call,
};

static void handle_ping_apps_data_cb(struct ping_apps_cb_entry *cb_entry)
{
	struct ping_apps_data_cb_arg arg;
	struct ping_apps_data_cb_ret ret;
	uint32_t my_sum = 0;
	uint32_t *my_data;
	int i;

	if (cb_entry->num > 0) {
		cb_entry->num--;
		arg.cb_id = cb_entry->cb_id;
		arg.size = ((struct ping_apps_data_cb_info *)
			    (cb_entry->cb_info))->size;

		my_data = kmalloc((arg.size * sizeof(uint32_t)), GFP_KERNEL);
		if (!my_data)
			return;

		for (i = 0; i < arg.size; i++) {
			my_data[i] = (42 + i);
			my_sum ^= (42 + i);
		}
		arg.data = my_data;
		arg.sum = my_sum;

		((int (*)(struct msm_rpc_server *,
			  struct msm_rpc_client_info *,
			  struct ping_apps_data_cb_arg *,
			  struct ping_apps_data_cb_ret *))
		 ((struct ping_apps_data_cb_info *)
		  (cb_entry->cb_info))->cb_func)(&rpc_server,
					     &cb_entry->clnt_info,
					     &arg, &ret);
		pr_info("%s: cb_id = %d, ret = %d\n",
			__func__, arg.cb_id, ret.result);
		kfree(my_data);
	}
}

static void handle_ping_apps_register_cb(struct ping_apps_cb_entry *cb_entry)
{
	struct ping_apps_register_cb_arg arg;

	if (cb_entry->num > 0) {
		cb_entry->num--;
		arg.cb_id = cb_entry->cb_id;
		arg.num = cb_entry->num;

		pr_info("%s: cb_id = %d, num = %d\n",
			__func__, arg.cb_id, arg.num);
		((int (*)(struct msm_rpc_server *,
			  struct msm_rpc_client_info *,
			  struct ping_apps_register_cb_arg *,
			  void *))cb_entry->cb_info)(&rpc_server,
						     &cb_entry->clnt_info,
						     &arg, NULL);
	}

}

static int ping_apps_cb_process_thread(void *data)
{
	struct ping_apps_cb_entry *cb_entry;
	uint32_t sleep_time;
	uint32_t time_slept = 0;

	pr_info("%s: thread started\n", __func__);
	for (;;) {
		sleep_time = 1000;
		mutex_lock(&cb_entry_list_lock);
		list_for_each_entry(cb_entry, &cb_entry_list, list) {
			if (cb_entry->time_to_next_cb <= time_slept) {
				cb_entry->cb_func(cb_entry);
				cb_entry->time_to_next_cb =
					cb_entry->interval_ms;
			} else
				cb_entry->time_to_next_cb -= time_slept;

			if (cb_entry->time_to_next_cb < sleep_time)
				sleep_time = cb_entry->time_to_next_cb;
		}
		mutex_unlock(&cb_entry_list_lock);

		msleep(sleep_time);
		time_slept = sleep_time;
	}

	do_exit(0);
}

static int ping_apps_data_register(struct ping_apps_data_arg *arg,
				   struct ping_apps_data_ret *ret)
{
	int i;

	ret->result = 0;
	for (i = 0; i < arg->size; i++)
		ret->result ^= arg->data[i];

	return 0;
}

static int ping_apps_data_cb_reg(struct ping_apps_data_cb_reg_arg *arg,
				 struct ping_apps_data_cb_reg_ret *ret)
{
	struct ping_apps_cb_entry *cb_entry;
	struct ping_apps_data_cb_info *cb_info;

	cb_entry = kmalloc(sizeof(*cb_entry), GFP_KERNEL);
	if (!cb_entry)
		return -ENOMEM;

	cb_entry->cb_info = kmalloc(sizeof(struct ping_apps_data_cb_info),
				    GFP_KERNEL);
	if (!cb_entry->cb_info) {
		kfree(cb_entry);
		return -ENOMEM;
	}
	cb_info = (struct ping_apps_data_cb_info *)cb_entry->cb_info;

	INIT_LIST_HEAD(&cb_entry->list);
	cb_entry->cb_func = handle_ping_apps_data_cb;
	cb_entry->cb_id = arg->cb_id;
	cb_entry->num = arg->num;
	cb_entry->interval_ms = arg->interval_ms;
	cb_entry->time_to_next_cb = arg->interval_ms;
	cb_info->cb_func = ping_apps_data_cb;
	cb_info->size = arg->size;
	cb_info->num_tasks = arg->num_tasks;

	mutex_lock(&cb_entry_list_lock);
	list_add_tail(&cb_entry->list, &cb_entry_list);
	mutex_unlock(&cb_entry_list_lock);

	msm_rpc_server_get_requesting_client(&cb_entry->clnt_info);

	if (IS_ERR(server_thread))
		server_thread = kthread_run(ping_apps_cb_process_thread,
					    NULL, "kpingrpccbprocessd");
	if (IS_ERR(server_thread)) {
		kfree(cb_entry);
		return PTR_ERR(server_thread);
	}

	ret->result = 1;
	return 0;
}

static int ping_apps_data_cb_unreg(struct ping_apps_data_cb_unreg_arg *arg,
				   struct ping_apps_data_cb_unreg_ret *ret)
{
	struct ping_apps_cb_entry *cb_entry, *tmp_cb_entry;

	mutex_lock(&cb_entry_list_lock);
	list_for_each_entry_safe(cb_entry, tmp_cb_entry,
				 &cb_entry_list, list) {
		if (cb_entry->cb_id == arg->cb_id) {
			list_del(&cb_entry->list);
			kfree(cb_entry->cb_info);
			kfree(cb_entry);
			break;
		}
	}
	mutex_unlock(&cb_entry_list_lock);

	ret->result = 1;
	return 0;
}

static int ping_apps_register(struct ping_apps_register_arg *arg,
			      struct ping_apps_register_ret *ret)
{
	struct ping_apps_cb_entry *cb_entry;

	cb_entry = kmalloc(sizeof(*cb_entry), GFP_KERNEL);
	if (!cb_entry)
		return -ENOMEM;

	INIT_LIST_HEAD(&cb_entry->list);
	cb_entry->cb_func = handle_ping_apps_register_cb;
	cb_entry->cb_info = ping_apps_register_cb;
	cb_entry->cb_id = arg->cb_id;
	cb_entry->num = arg->num;
	cb_entry->interval_ms = 100;
	cb_entry->time_to_next_cb = 100;

	mutex_lock(&cb_entry_list_lock);
	list_add_tail(&cb_entry->list, &cb_entry_list);
	mutex_unlock(&cb_entry_list_lock);

	msm_rpc_server_get_requesting_client(&cb_entry->clnt_info);

	if (IS_ERR(server_thread))
		server_thread = kthread_run(ping_apps_cb_process_thread,
					    NULL, "kpingrpccbprocessd");
	if (IS_ERR(server_thread)) {
		kfree(cb_entry);
		return PTR_ERR(server_thread);
	}

	ret->result = 1;
	return 0;
}

static int ping_apps_unregister(struct ping_apps_unregister_arg *arg,
				struct ping_apps_unregister_ret *ret)
{
	struct ping_apps_cb_entry *cb_entry, *tmp_cb_entry;

	mutex_lock(&cb_entry_list_lock);
	list_for_each_entry_safe(cb_entry, tmp_cb_entry,
				 &cb_entry_list, list) {
		if (cb_entry->cb_id == arg->cb_id) {
			list_del(&cb_entry->list);
			kfree(cb_entry);
			break;
		}
	}
	mutex_unlock(&cb_entry_list_lock);

	ret->result = 1;
	return 0;
}

static int ping_apps_data_cb_arg_func(struct msm_rpc_server *server,
				      struct msm_rpc_xdr *xdr, void *data)
{
	struct ping_apps_data_cb_arg *arg = data;

	xdr_send_uint32(xdr, &arg->cb_id);
	xdr_send_array(xdr, (void **)&arg->data, &arg->size, 64,
		       sizeof(uint32_t), (void *)xdr_send_uint32);
	xdr_send_uint32(xdr, &arg->size);
	xdr_send_uint32(xdr, &arg->sum);

	return 0;
}

static int ping_apps_data_cb_ret_func(struct msm_rpc_server *server,
				      struct msm_rpc_xdr *xdr, void *data)
{
	struct ping_apps_data_cb_ret *ret = data;

	xdr_recv_uint32(xdr, &ret->result);

	return 0;
}

static int ping_apps_data_cb(struct msm_rpc_server *server,
			     struct msm_rpc_client_info *clnt_info,
			     struct ping_apps_data_cb_arg *arg,
			     struct ping_apps_data_cb_ret *ret)
{
	return msm_rpc_server_cb_req2(server, clnt_info,
				      PING_APPS_DATA_CB,
				      ping_apps_data_cb_arg_func, arg,
				      ping_apps_data_cb_ret_func, ret, -1);
}

static int ping_apps_register_cb_arg(struct msm_rpc_server *server,
				     struct msm_rpc_xdr *xdr, void *data)
{
	struct ping_apps_register_cb_arg *arg = data;

	xdr_send_uint32(xdr, &arg->cb_id);
	xdr_send_int32(xdr, &arg->num);

	return 0;
}

static int ping_apps_register_cb(struct msm_rpc_server *server,
				struct msm_rpc_client_info *clnt_info,
				struct ping_apps_register_cb_arg *arg,
				void *ret)
{
	return msm_rpc_server_cb_req2(server, clnt_info,
				      PING_APPS_REG_CB,
				      ping_apps_register_cb_arg,
				      arg, NULL, NULL, -1);
}

static int handle_ping_apps_data_register(struct msm_rpc_server *server,
					  struct rpc_request_hdr *req,
					  struct msm_rpc_xdr *xdr)
{
	uint32_t rc;
	struct ping_apps_data_arg arg;
	struct ping_apps_data_ret ret;

	pr_info("%s: request received\n", __func__);

	xdr_recv_array(xdr, (void **)&arg.data, &arg.size, 64,
		       sizeof(uint32_t), (void *)xdr_recv_uint32);
	xdr_recv_uint32(xdr, &arg.size);

	rc = ping_apps_data_register(&arg, &ret);
	if (rc < 0)
		goto free_and_return;

	xdr_start_accepted_reply(xdr, RPC_ACCEPTSTAT_SUCCESS);
	xdr_send_uint32(xdr, &ret.result);
	rc = xdr_send_msg(xdr);
	if (rc < 0)
		pr_info("%s: sending reply failed\n", __func__);
	else
		rc = 1;

 free_and_return:
	kfree(arg.data);
	return rc;
}

static int handle_ping_apps_data_cb_reg(struct msm_rpc_server *server,
					struct rpc_request_hdr *req,
					struct msm_rpc_xdr *xdr)
{
	uint32_t rc;
	struct ping_apps_data_cb_reg_arg arg;
	struct ping_apps_data_cb_reg_ret ret;

	pr_info("%s: request received\n", __func__);

	xdr_recv_uint32(xdr, &arg.cb_id);
	xdr_recv_uint32(xdr, &arg.num);
	xdr_recv_uint32(xdr, &arg.size);
	xdr_recv_uint32(xdr, &arg.interval_ms);
	xdr_recv_uint32(xdr, &arg.num_tasks);

	rc = ping_apps_data_cb_reg(&arg, &ret);
	if (rc < 0)
		return rc;

	xdr_start_accepted_reply(xdr, RPC_ACCEPTSTAT_SUCCESS);
	xdr_send_uint32(xdr, &ret.result);
	rc = xdr_send_msg(xdr);
	if (rc < 0)
		pr_info("%s: sending reply failed\n", __func__);
	else
		rc = 1;

	return rc;
}

static int handle_ping_apps_data_cb_unreg(struct msm_rpc_server *server,
					  struct rpc_request_hdr *req,
					  struct msm_rpc_xdr *xdr)
{
	uint32_t rc;
	struct ping_apps_data_cb_unreg_arg arg;
	struct ping_apps_data_cb_unreg_ret ret;

	pr_info("%s: request received\n", __func__);

	xdr_recv_uint32(xdr, &arg.cb_id);

	rc = ping_apps_data_cb_unreg(&arg, &ret);
	if (rc < 0)
		return rc;

	xdr_start_accepted_reply(xdr, RPC_ACCEPTSTAT_SUCCESS);
	xdr_send_uint32(xdr, &ret.result);
	rc = xdr_send_msg(xdr);
	if (rc < 0)
		pr_info("%s: sending reply failed\n", __func__);
	else
		rc = 1;

	return rc;
}

static int handle_ping_apps_register(struct msm_rpc_server *server,
				     struct rpc_request_hdr *req,
				     struct msm_rpc_xdr *xdr)
{
	uint32_t rc;
	struct ping_apps_register_arg arg;
	struct ping_apps_register_ret ret;

	pr_info("%s: request received\n", __func__);

	xdr_recv_uint32(xdr, &arg.cb_id);
	xdr_recv_int32(xdr, &arg.num);

	rc = ping_apps_register(&arg, &ret);
	if (rc < 0)
		return rc;

	xdr_start_accepted_reply(xdr, RPC_ACCEPTSTAT_SUCCESS);
	xdr_send_uint32(xdr, &ret.result);
	rc = xdr_send_msg(xdr);
	if (rc < 0)
		pr_info("%s: sending reply failed\n", __func__);
	else
		rc = 1;

	return rc;
}

static int handle_ping_apps_unregister(struct msm_rpc_server *server,
				       struct rpc_request_hdr *req,
				       struct msm_rpc_xdr *xdr)
{
	uint32_t rc;
	struct ping_apps_unregister_arg arg;
	struct ping_apps_unregister_ret ret;

	pr_info("%s: request received\n", __func__);

	xdr_recv_uint32(xdr, &arg.cb_id);

	rc = ping_apps_unregister(&arg, &ret);
	if (rc < 0)
		return rc;

	xdr_start_accepted_reply(xdr, RPC_ACCEPTSTAT_SUCCESS);
	xdr_send_uint32(xdr, &ret.result);
	rc = xdr_send_msg(xdr);
	if (rc < 0)
		pr_info("%s: sending reply failed\n", __func__);
	else
		rc = 1;

	return rc;
}

static int handle_rpc_call(struct msm_rpc_server *server,
			   struct rpc_request_hdr *req,
			   struct msm_rpc_xdr *xdr)
{
	switch (req->procedure) {
	case PING_APPS_NULL:
		pr_info("%s: null procedure request received\n", __func__);
		return 0;
	case PING_APPS_DATA:
		return handle_ping_apps_data_register(server, req, xdr);
	case PING_APPS_REG:
		return handle_ping_apps_register(server, req, xdr);
	case PING_APPS_UNREG:
		return handle_ping_apps_unregister(server, req, xdr);
	case PING_APPS_DATA_CB_REG:
		return handle_ping_apps_data_cb_reg(server, req, xdr);
	case PING_APPS_DATA_CB_UNREG:
		return handle_ping_apps_data_cb_unreg(server, req, xdr);
	default:
		return -ENODEV;
	}
}

static int __init ping_apps_server_init(void)
{
	INIT_LIST_HEAD(&cb_entry_list);
	server_thread = ERR_PTR(-1);
	return msm_rpc_create_server2(&rpc_server);
}

module_init(ping_apps_server_init);

MODULE_DESCRIPTION("PING APPS SERVER Driver");
MODULE_LICENSE("GPL v2");
