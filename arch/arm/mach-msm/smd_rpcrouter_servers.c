/* arch/arm/mach-msm/rpc_servers.c
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
 * Author: Iliyan Malchev <ibm@android.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h>

#include <linux/msm_rpcrouter.h>
#include <linux/uaccess.h>

#include <mach/msm_rpcrouter.h>
#include "smd_rpcrouter.h"

static struct msm_rpc_endpoint *endpoint;

#define FLAG_REGISTERED 0x0001

static LIST_HEAD(rpc_server_list);
static DEFINE_MUTEX(rpc_server_list_lock);
static int rpc_servers_active;
static struct wake_lock rpc_servers_wake_lock;
static struct msm_rpc_xdr server_xdr;
static uint32_t current_xid;

static void rpc_server_register(struct msm_rpc_server *server)
{
	int rc;
	rc = msm_rpc_register_server(endpoint, server->prog, server->vers);
	if (rc < 0)
		printk(KERN_ERR "[rpcserver] error registering %p @ %08x:%d\n",
		       server, server->prog, server->vers);
}

static struct msm_rpc_server *rpc_server_find(uint32_t prog, uint32_t vers)
{
	struct msm_rpc_server *server;

	mutex_lock(&rpc_server_list_lock);
	list_for_each_entry(server, &rpc_server_list, list) {
		if ((server->prog == prog) &&
		    msm_rpc_is_compatible_version(server->vers, vers)) {
			mutex_unlock(&rpc_server_list_lock);
			return server;
		}
	}
	mutex_unlock(&rpc_server_list_lock);
	return NULL;
}

static void rpc_server_register_all(void)
{
	struct msm_rpc_server *server;

	mutex_lock(&rpc_server_list_lock);
	list_for_each_entry(server, &rpc_server_list, list) {
		if (!(server->flags & FLAG_REGISTERED)) {
			rpc_server_register(server);
			server->flags |= FLAG_REGISTERED;
		}
	}
	mutex_unlock(&rpc_server_list_lock);
}

int msm_rpc_create_server(struct msm_rpc_server *server)
{
	void *buf;

	/* make sure we're in a sane state first */
	server->flags = 0;
	INIT_LIST_HEAD(&server->list);
	mutex_init(&server->cb_req_lock);

	server->version = 1;

	xdr_init(&server->cb_xdr);
	buf = kmalloc(MSM_RPC_MSGSIZE_MAX, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	xdr_init_output(&server->cb_xdr, buf, MSM_RPC_MSGSIZE_MAX);

	server->cb_ept = server->cb_xdr.ept = msm_rpc_open();
	if (IS_ERR(server->cb_ept)) {
		xdr_clean_output(&server->cb_xdr);
		return PTR_ERR(server->cb_ept);
	}

	server->cb_ept->flags = MSM_RPC_UNINTERRUPTIBLE;
	server->cb_ept->dst_prog = cpu_to_be32(server->prog | 0x01000000);
	server->cb_ept->dst_vers = cpu_to_be32(server->vers);

	mutex_lock(&rpc_server_list_lock);
	list_add(&server->list, &rpc_server_list);
	if (rpc_servers_active) {
		rpc_server_register(server);
		server->flags |= FLAG_REGISTERED;
	}
	mutex_unlock(&rpc_server_list_lock);

	return 0;
}
EXPORT_SYMBOL(msm_rpc_create_server);

int msm_rpc_create_server2(struct msm_rpc_server *server)
{
	int rc;

	rc = msm_rpc_create_server(server);
	server->version = 2;

	return rc;
}
EXPORT_SYMBOL(msm_rpc_create_server2);

static int rpc_send_accepted_void_reply(struct msm_rpc_endpoint *client,
					uint32_t xid, uint32_t accept_status)
{
	int rc = 0;
	uint8_t reply_buf[sizeof(struct rpc_reply_hdr)];
	struct rpc_reply_hdr *reply = (struct rpc_reply_hdr *)reply_buf;

	reply->xid = cpu_to_be32(xid);
	reply->type = cpu_to_be32(1); /* reply */
	reply->reply_stat = cpu_to_be32(RPCMSG_REPLYSTAT_ACCEPTED);

	reply->data.acc_hdr.accept_stat = cpu_to_be32(accept_status);
	reply->data.acc_hdr.verf_flavor = 0;
	reply->data.acc_hdr.verf_length = 0;

	rc = msm_rpc_write(client, reply_buf, sizeof(reply_buf));
	if (rc ==  -ENETRESET) {
		/* Modem restarted, drop reply, clear state */
		msm_rpc_clear_netreset(client);
	}
	if (rc < 0)
		printk(KERN_ERR
		       "%s: could not write response: %d\n",
		       __FUNCTION__, rc);

	return rc;
}

/*
 * Interface to be used to start accepted reply message for a
 * request.  Returns the buffer pointer to attach any payload.
 * Should call msm_rpc_server_send_accepted_reply to complete sending
 * reply.  Marshaling should be handled by user for the payload.
 *
 * server: pointer to server data structure
 *
 * xid: transaction id. Has to be same as the one in request.
 *
 * accept_status: acceptance status
 *
 * Return Value:
 *        pointer to buffer to attach the payload.
 */
void *msm_rpc_server_start_accepted_reply(struct msm_rpc_server *server,
					  uint32_t xid, uint32_t accept_status)
{
	struct rpc_reply_hdr *reply;

	mutex_lock(&server_xdr.out_lock);

	reply = (struct rpc_reply_hdr *)server_xdr.out_buf;

	reply->xid = cpu_to_be32(xid);
	reply->type = cpu_to_be32(1); /* reply */
	reply->reply_stat = cpu_to_be32(RPCMSG_REPLYSTAT_ACCEPTED);

	reply->data.acc_hdr.accept_stat = cpu_to_be32(accept_status);
	reply->data.acc_hdr.verf_flavor = 0;
	reply->data.acc_hdr.verf_length = 0;

	server_xdr.out_index += sizeof(*reply);

	return reply + 1;
}
EXPORT_SYMBOL(msm_rpc_server_start_accepted_reply);

/*
 * Interface to be used to send accepted reply for a request.
 * msm_rpc_server_start_accepted_reply should have been called before.
 * Marshaling should be handled by user for the payload.
 *
 * server: pointer to server data structure
 *
 * size: additional payload size
 *
 * Return Value:
 *        0 on success, otherwise returns an error code.
 */
int msm_rpc_server_send_accepted_reply(struct msm_rpc_server *server,
				       uint32_t size)
{
	int rc = 0;

	server_xdr.out_index += size;
	rc = msm_rpc_write(endpoint, server_xdr.out_buf,
			   server_xdr.out_index);
	if (rc > 0)
		rc = 0;

	mutex_unlock(&server_xdr.out_lock);
	return rc;
}
EXPORT_SYMBOL(msm_rpc_server_send_accepted_reply);

/*
 * Interface to be used to send a server callback request.
 * If the request takes any arguments or expects any return, the user
 * should handle it in 'arg_func' and 'ret_func' respectively.
 * Marshaling and Unmarshaling should be handled by the user in argument
 * and return functions.
 *
 * server: pointer to server data sturcture
 *
 * clnt_info: pointer to client information data structure.
 *            callback will be sent to this client.
 *
 * cb_proc: callback procedure being requested
 *
 * arg_func: argument function pointer.  'buf' is where arguments needs to
 *   be filled. 'data' is arg_data.
 *
 * ret_func: return function pointer.  'buf' is where returned data should
 *   be read from. 'data' is ret_data.
 *
 * arg_data: passed as an input parameter to argument function.
 *
 * ret_data: passed as an input parameter to return function.
 *
 * timeout: timeout for reply wait in jiffies.  If negative timeout is
 *   specified a default timeout of 10s is used.
 *
 * Return Value:
 *        0 on success, otherwise an error code is returned.
 */
int msm_rpc_server_cb_req(struct msm_rpc_server *server,
			  struct msm_rpc_client_info *clnt_info,
			  uint32_t cb_proc,
			  int (*arg_func)(struct msm_rpc_server *server,
					  void *buf, void *data),
			  void *arg_data,
			  int (*ret_func)(struct msm_rpc_server *server,
					  void *buf, void *data),
			  void *ret_data, long timeout)
{
	struct rpc_reply_hdr *rpc_rsp;
	void *buffer;
	int rc = 0;
	uint32_t req_xid;

	if (!clnt_info)
		return -EINVAL;

	mutex_lock(&server->cb_req_lock);

	msm_rpc_setup_req((struct rpc_request_hdr *)server->cb_xdr.out_buf,
			  (server->prog | 0x01000000),
			  be32_to_cpu(clnt_info->vers), cb_proc);
	server->cb_xdr.out_index = sizeof(struct rpc_request_hdr);
	req_xid = *(uint32_t *)server->cb_xdr.out_buf;

	if (arg_func) {
		rc = arg_func(server, (void *)((struct rpc_request_hdr *)
					       server->cb_xdr.out_buf + 1),
			      arg_data);
		if (rc < 0)
			goto release_locks;
		else
			server->cb_xdr.out_index += rc;
	}

	server->cb_ept->dst_pid = clnt_info->pid;
	server->cb_ept->dst_cid = clnt_info->cid;
	rc = msm_rpc_write(server->cb_ept, server->cb_xdr.out_buf,
			   server->cb_xdr.out_index);
	if (rc < 0) {
		pr_err("%s: couldn't send RPC CB request:%d\n", __func__, rc);
		goto release_locks;
	} else
		rc = 0;

	if (timeout < 0)
		timeout = msecs_to_jiffies(10000);

	do {
		buffer = NULL;
		rc = msm_rpc_read(server->cb_ept, &buffer, -1, timeout);
		xdr_init_input(&server->cb_xdr, buffer, rc);
		if ((rc < ((int)(sizeof(uint32_t) * 2))) ||
		    (be32_to_cpu(*((uint32_t *)buffer + 1)) != 1)) {
			printk(KERN_ERR "%s: Invalid reply: %d\n",
			       __func__, rc);
			goto free_and_release;
		}

		rpc_rsp = (struct rpc_reply_hdr *)server->cb_xdr.in_buf;
		if (req_xid != rpc_rsp->xid) {
			pr_info("%s: xid mismatch, req %d reply %d\n",
				__func__, be32_to_cpu(req_xid),
				be32_to_cpu(rpc_rsp->xid));
			xdr_clean_input(&server->cb_xdr);
			rc = timeout;
			/* timeout is not adjusted, but it is not critical */
		} else
			rc = 0;
	} while (rc);

	if (be32_to_cpu(rpc_rsp->reply_stat) != RPCMSG_REPLYSTAT_ACCEPTED) {
		pr_err("%s: RPC cb req was denied! %d\n", __func__,
		       be32_to_cpu(rpc_rsp->reply_stat));
		rc = -EPERM;
		goto free_and_release;
	}

	if (be32_to_cpu(rpc_rsp->data.acc_hdr.accept_stat) !=
	    RPC_ACCEPTSTAT_SUCCESS) {
		pr_err("%s: RPC cb req was not successful (%d)\n", __func__,
		       be32_to_cpu(rpc_rsp->data.acc_hdr.accept_stat));
		rc = -EINVAL;
		goto free_and_release;
	}

	if (ret_func)
		rc = ret_func(server, (void *)(rpc_rsp + 1), ret_data);

free_and_release:
	xdr_clean_input(&server->cb_xdr);
	server->cb_xdr.out_index = 0;
release_locks:
	mutex_unlock(&server->cb_req_lock);
	return rc;
}
EXPORT_SYMBOL(msm_rpc_server_cb_req);

/*
 * Interface to be used to send a server callback request.
 * If the request takes any arguments or expects any return, the user
 * should handle it in 'arg_func' and 'ret_func' respectively.
 * Marshaling and Unmarshaling should be handled by the user in argument
 * and return functions.
 *
 * server: pointer to server data sturcture
 *
 * clnt_info: pointer to client information data structure.
 *            callback will be sent to this client.
 *
 * cb_proc: callback procedure being requested
 *
 * arg_func: argument function pointer.  'xdr' is the xdr being used.
 *   'data' is arg_data.
 *
 * ret_func: return function pointer.  'xdr' is the xdr being used.
 *   'data' is ret_data.
 *
 * arg_data: passed as an input parameter to argument function.
 *
 * ret_data: passed as an input parameter to return function.
 *
 * timeout: timeout for reply wait in jiffies.  If negative timeout is
 *   specified a default timeout of 10s is used.
 *
 * Return Value:
 *        0 on success, otherwise an error code is returned.
 */
int msm_rpc_server_cb_req2(struct msm_rpc_server *server,
			   struct msm_rpc_client_info *clnt_info,
			   uint32_t cb_proc,
			   int (*arg_func)(struct msm_rpc_server *server,
					   struct msm_rpc_xdr *xdr, void *data),
			   void *arg_data,
			   int (*ret_func)(struct msm_rpc_server *server,
					   struct msm_rpc_xdr *xdr, void *data),
			   void *ret_data, long timeout)
{
	int size = 0;
	struct rpc_reply_hdr rpc_rsp;
	void *buffer;
	int rc = 0;
	uint32_t req_xid;

	if (!clnt_info)
		return -EINVAL;

	mutex_lock(&server->cb_req_lock);

	xdr_start_request(&server->cb_xdr, (server->prog | 0x01000000),
			  be32_to_cpu(clnt_info->vers), cb_proc);
	req_xid = be32_to_cpu(*(uint32_t *)server->cb_xdr.out_buf);
	if (arg_func) {
		rc = arg_func(server, &server->cb_xdr, arg_data);
		if (rc < 0)
			goto release_locks;
		else
			size += rc;
	}

	server->cb_ept->dst_pid = clnt_info->pid;
	server->cb_ept->dst_cid = clnt_info->cid;
	rc = xdr_send_msg(&server->cb_xdr);
	if (rc < 0) {
		pr_err("%s: couldn't send RPC CB request:%d\n", __func__, rc);
		goto release_locks;
	} else
		rc = 0;

	if (timeout < 0)
		timeout = msecs_to_jiffies(10000);

	do {
		buffer = NULL;
		rc = msm_rpc_read(server->cb_ept, &buffer, -1, timeout);
		if (rc < 0) {
			server->cb_xdr.out_index = 0;
			goto release_locks;
		}

		xdr_init_input(&server->cb_xdr, buffer, rc);
		rc = xdr_recv_reply(&server->cb_xdr, &rpc_rsp);
		if (rc || (rpc_rsp.type != 1)) {
			printk(KERN_ERR "%s: Invalid reply :%d\n",
			       __func__, rc);
			rc = -EINVAL;
			goto free_and_release;
		}

		if (req_xid != rpc_rsp.xid) {
			pr_info("%s: xid mismatch, req %d reply %d\n",
				__func__, req_xid, rpc_rsp.xid);
			xdr_clean_input(&server->cb_xdr);
			rc = timeout;
			/* timeout is not adjusted, but it is not critical */
		} else
			rc = 0;

	} while (rc);

	if (rpc_rsp.reply_stat != RPCMSG_REPLYSTAT_ACCEPTED) {
		pr_err("%s: RPC cb req was denied! %d\n", __func__,
		       rpc_rsp.reply_stat);
		rc = -EPERM;
		goto free_and_release;
	}

	if (rpc_rsp.data.acc_hdr.accept_stat != RPC_ACCEPTSTAT_SUCCESS) {
		pr_err("%s: RPC cb req was not successful (%d)\n", __func__,
		       rpc_rsp.data.acc_hdr.accept_stat);
		rc = -EINVAL;
		goto free_and_release;
	}

	if (ret_func)
		rc = ret_func(server, &server->cb_xdr, ret_data);

free_and_release:
	xdr_clean_input(&server->cb_xdr);
	server->cb_xdr.out_index = 0;
release_locks:
	mutex_unlock(&server->cb_req_lock);
	return rc;
}
EXPORT_SYMBOL(msm_rpc_server_cb_req2);

void msm_rpc_server_get_requesting_client(struct msm_rpc_client_info *clnt_info)
{
	if (!clnt_info)
		return;

	get_requesting_client(endpoint, current_xid, clnt_info);
}

static int rpc_servers_thread(void *data)
{
	void *buffer, *buf;
	struct rpc_request_hdr req;
	struct rpc_request_hdr *req1;
	struct msm_rpc_server *server;
	int rc;

	xdr_init(&server_xdr);
	server_xdr.ept = endpoint;

	buf = kmalloc(MSM_RPC_MSGSIZE_MAX, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	xdr_init_output(&server_xdr, buf, MSM_RPC_MSGSIZE_MAX);

	for (;;) {
		wake_unlock(&rpc_servers_wake_lock);
		rc = wait_event_interruptible(endpoint->wait_q,
					      !list_empty(&endpoint->read_q));
		wake_lock(&rpc_servers_wake_lock);

		rc = msm_rpc_read(endpoint, &buffer, -1, -1);
		if (rc < 0) {
			printk(KERN_ERR "%s: could not read: %d\n",
			       __FUNCTION__, rc);
			break;
		}

		req1 = (struct rpc_request_hdr *)buffer;
		current_xid = req1->xid;

		xdr_init_input(&server_xdr, buffer, rc);
		xdr_recv_req(&server_xdr, &req);

		server = rpc_server_find(req.prog, req.vers);

		if (req.rpc_vers != 2)
			goto free_buffer;
		if (req.type != 0)
			goto free_buffer;
		if (!server) {
			rpc_send_accepted_void_reply(
				endpoint, req.xid,
				RPC_ACCEPTSTAT_PROG_UNAVAIL);
			goto free_buffer;
		}

		if (server->version == 2)
			rc = server->rpc_call2(server, &req, &server_xdr);
		else {
			req1->type = be32_to_cpu(req1->type);
			req1->xid = be32_to_cpu(req1->xid);
			req1->rpc_vers = be32_to_cpu(req1->rpc_vers);
			req1->prog = be32_to_cpu(req1->prog);
			req1->vers = be32_to_cpu(req1->vers);
			req1->procedure = be32_to_cpu(req1->procedure);

			rc = server->rpc_call(server, req1, rc);
		}

		if (rc == 0) {
			msm_rpc_server_start_accepted_reply(
				server, req.xid,
				RPC_ACCEPTSTAT_SUCCESS);
			msm_rpc_server_send_accepted_reply(server, 0);
		} else if (rc < 0) {
			msm_rpc_server_start_accepted_reply(
				server, req.xid,
				RPC_ACCEPTSTAT_PROC_UNAVAIL);
			msm_rpc_server_send_accepted_reply(server, 0);
		}
 free_buffer:
		xdr_clean_input(&server_xdr);
		server_xdr.out_index = 0;
	}
	do_exit(0);
}

static int rpcservers_probe(struct platform_device *pdev)
{
	struct task_struct *server_thread;

	endpoint = msm_rpc_open();
	if (IS_ERR(endpoint))
		return PTR_ERR(endpoint);

	/* we're online -- register any servers installed beforehand */
	rpc_servers_active = 1;
	current_xid = 0;
	rpc_server_register_all();

	/* start the kernel thread */
	server_thread = kthread_run(rpc_servers_thread, NULL, "krpcserversd");
	if (IS_ERR(server_thread))
		return PTR_ERR(server_thread);

	return 0;
}

static struct platform_driver rpcservers_driver = {
	.probe	= rpcservers_probe,
	.driver	= {
		.name	= "oncrpc_router",
		.owner	= THIS_MODULE,
	},
};

static int __init rpc_servers_init(void)
{
	wake_lock_init(&rpc_servers_wake_lock, WAKE_LOCK_SUSPEND, "rpc_server");
	return platform_driver_register(&rpcservers_driver);
}

module_init(rpc_servers_init);

MODULE_DESCRIPTION("MSM RPC Servers");
MODULE_AUTHOR("Iliyan Malchev <ibm@android.com>");
MODULE_LICENSE("GPL");
