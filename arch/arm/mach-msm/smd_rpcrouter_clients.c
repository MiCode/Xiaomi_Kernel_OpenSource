/* Copyright (c) 2009-2011, Code Aurora Forum. All rights reserved.
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
 * SMD RPCROUTER CLIENTS module.
 */

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>

#include <mach/msm_rpcrouter.h>
#include "smd_rpcrouter.h"

struct msm_rpc_client_cb_item {
	struct list_head list;

	void *buf;
	int size;
};

struct msm_rpc_cb_table_item {
	struct list_head list;

	uint32_t cb_id;
	void *cb_func;
};

static int rpc_clients_cb_thread(void *data)
{
	struct msm_rpc_client_cb_item *cb_item;
	struct msm_rpc_client *client;
	struct rpc_request_hdr req;
	int ret;

	client = data;
	for (;;) {
		wait_event(client->cb_wait, client->cb_avail);
		if (client->exit_flag)
			break;

		client->cb_avail = 0;
		mutex_lock(&client->cb_item_list_lock);
		while (!list_empty(&client->cb_item_list)) {
			cb_item = list_first_entry(
				&client->cb_item_list,
				struct msm_rpc_client_cb_item,
				list);
			list_del(&cb_item->list);
			mutex_unlock(&client->cb_item_list_lock);
			xdr_init_input(&client->cb_xdr, cb_item->buf,
				       cb_item->size);
			ret = xdr_recv_req(&client->cb_xdr, &req);
			if (ret)
				goto bad_rpc;

			if (req.type != 0)
				goto bad_rpc;
			if (req.rpc_vers != 2)
				goto bad_rpc;
			if (req.prog !=
			    (client->prog | 0x01000000))
				goto bad_rpc;

			if (client->version == 2)
				client->cb_func2(client, &req, &client->cb_xdr);
			else
				client->cb_func(client, client->cb_xdr.in_buf,
						client->cb_xdr.in_size);
 bad_rpc:
			xdr_clean_input(&client->cb_xdr);
			kfree(cb_item);
			mutex_lock(&client->cb_item_list_lock);
		}
		mutex_unlock(&client->cb_item_list_lock);
	}
	complete_and_exit(&client->cb_complete, 0);
}

static int rpc_clients_thread(void *data)
{
	void *buffer;
	uint32_t type;
	struct msm_rpc_client *client;
	int rc = 0;
	struct msm_rpc_client_cb_item *cb_item;
	struct rpc_request_hdr req;

	client = data;
	for (;;) {
		buffer = NULL;
		rc = msm_rpc_read(client->ept, &buffer, -1, -1);

		if (client->exit_flag) {
			kfree(buffer);
			break;
		}

		if (rc < 0) {
			/* wakeup any pending requests */
			wake_up(&client->reply_wait);
			kfree(buffer);
			continue;
		}

		if (rc < ((int)(sizeof(uint32_t) * 2))) {
			kfree(buffer);
			continue;
		}

		type = be32_to_cpu(*((uint32_t *)buffer + 1));
		if (type == 1) {
			xdr_init_input(&client->xdr, buffer, rc);
			wake_up(&client->reply_wait);
		} else if (type == 0) {
			if (client->cb_thread == NULL) {
				xdr_init_input(&client->cb_xdr, buffer, rc);
				xdr_recv_req(&client->cb_xdr, &req);

				if ((req.rpc_vers == 2) &&
				    (req.prog == (client->prog | 0x01000000))) {
					if (client->version == 2)
						client->cb_func2(client, &req,
							 &client->cb_xdr);
					else
						client->cb_func(client,
						client->cb_xdr.in_buf, rc);
				}
				xdr_clean_input(&client->cb_xdr);
			} else {
				cb_item = kmalloc(sizeof(*cb_item), GFP_KERNEL);
				if (!cb_item) {
					pr_err("%s: no memory for cb item\n",
					       __func__);
					continue;
				}

				INIT_LIST_HEAD(&cb_item->list);
				cb_item->buf = buffer;
				cb_item->size = rc;
				mutex_lock(&client->cb_item_list_lock);
				list_add_tail(&cb_item->list,
					      &client->cb_item_list);
				mutex_unlock(&client->cb_item_list_lock);
				client->cb_avail = 1;
				wake_up(&client->cb_wait);
			}
		}
	}
	complete_and_exit(&client->complete, 0);
}

static struct msm_rpc_client *msm_rpc_create_client(void)
{
	struct msm_rpc_client *client;
	void *buf;

	client = kmalloc(sizeof(struct msm_rpc_client), GFP_KERNEL);
	if (!client)
		return ERR_PTR(-ENOMEM);

	xdr_init(&client->xdr);
	xdr_init(&client->cb_xdr);

	buf = kmalloc(MSM_RPC_MSGSIZE_MAX, GFP_KERNEL);
	if (!buf) {
		kfree(client);
		return ERR_PTR(-ENOMEM);
	}
	xdr_init_output(&client->xdr, buf, MSM_RPC_MSGSIZE_MAX);

	buf = kmalloc(MSM_RPC_MSGSIZE_MAX, GFP_KERNEL);
	if (!buf) {
		xdr_clean_output(&client->xdr);
		kfree(client);
		return ERR_PTR(-ENOMEM);
	}
	xdr_init_output(&client->cb_xdr, buf, MSM_RPC_MSGSIZE_MAX);

	init_waitqueue_head(&client->reply_wait);
	mutex_init(&client->req_lock);
	client->buf = NULL;
	client->cb_buf = NULL;
	client->cb_size = 0;
	client->exit_flag = 0;
	client->cb_restart_teardown = NULL;
	client->cb_restart_setup = NULL;
	client->in_reset = 0;

	init_completion(&client->complete);
	init_completion(&client->cb_complete);
	INIT_LIST_HEAD(&client->cb_item_list);
	mutex_init(&client->cb_item_list_lock);
	client->cb_avail = 0;
	init_waitqueue_head(&client->cb_wait);
	INIT_LIST_HEAD(&client->cb_list);
	mutex_init(&client->cb_list_lock);
	atomic_set(&client->next_cb_id, 1);

	return client;
}

static void msm_rpc_destroy_client(struct msm_rpc_client *client)
{
	xdr_clean_output(&client->xdr);
	xdr_clean_output(&client->cb_xdr);

	kfree(client);
}

void msm_rpc_remove_all_cb_func(struct msm_rpc_client *client)
{
	struct msm_rpc_cb_table_item *cb_item, *tmp_cb_item;

	mutex_lock(&client->cb_list_lock);
	list_for_each_entry_safe(cb_item, tmp_cb_item,
				 &client->cb_list, list) {
		list_del(&cb_item->list);
		kfree(cb_item);
	}
	mutex_unlock(&client->cb_list_lock);
}

static void cb_restart_teardown(void *client_data)
{
	struct msm_rpc_client *client;

	client = (struct msm_rpc_client *)client_data;
	if (client) {
		client->in_reset = 1;
		msm_rpc_remove_all_cb_func(client);
		client->xdr.out_index = 0;

		if (client->cb_restart_teardown)
			client->cb_restart_teardown(client);
	}
}

static void cb_restart_setup(void *client_data)
{
	struct msm_rpc_client *client;

	client = (struct msm_rpc_client *)client_data;

	if (client) {
		client->in_reset = 0;
		if (client->cb_restart_setup)
			client->cb_restart_setup(client);
	}
}

/* Returns the reset state of the client.
 *
 * Return Value:
 *	0 if client isn't in reset, >0 otherwise.
 */
int msm_rpc_client_in_reset(struct msm_rpc_client *client)
{
	int ret = 1;

	if (client)
		ret = client->in_reset;

	return ret;
}
EXPORT_SYMBOL(msm_rpc_client_in_reset);

/*
 * Interface to be used to register the client.
 *
 * name: string representing the client
 *
 * prog: program number of the client
 *
 * ver: version number of the client
 *
 * create_cb_thread: if set calls the callback function from a seprate thread
 *                   which helps the client requests to be processed without
 *                   getting loaded by callback handling.
 *
 * cb_func: function to be called if callback request is received.
 *          unmarshaling should be handled by the user in callback function
 *
 * Return Value:
 *        Pointer to initialized client data sturcture
 *        Or, the error code if registration fails.
 *
 */
struct msm_rpc_client *msm_rpc_register_client(
	const char *name,
	uint32_t prog, uint32_t ver,
	uint32_t create_cb_thread,
	int (*cb_func)(struct msm_rpc_client *, void *, int))
{
	struct msm_rpc_client *client;
	struct msm_rpc_endpoint *ept;
	int rc;

	client = msm_rpc_create_client();
	if (IS_ERR(client))
		return client;

	ept = msm_rpc_connect_compatible(prog, ver, MSM_RPC_UNINTERRUPTIBLE);
	if (IS_ERR(ept)) {
		msm_rpc_destroy_client(client);
		return (struct msm_rpc_client *)ept;
	}

	ept->client_data = client;
	ept->cb_restart_teardown = cb_restart_teardown;
	ept->cb_restart_setup = cb_restart_setup;

	client->prog = prog;
	client->ver = ver;
	client->ept = client->xdr.ept = client->cb_xdr.ept = ept;
	client->cb_func = cb_func;
	client->version = 1;

	/* start the read thread */
	client->read_thread = kthread_run(rpc_clients_thread, client,
					  "k%sclntd", name);
	if (IS_ERR(client->read_thread)) {
		rc = PTR_ERR(client->read_thread);
		msm_rpc_close(client->ept);
		msm_rpc_destroy_client(client);
		return ERR_PTR(rc);
	}

	if (!create_cb_thread || (cb_func == NULL)) {
		client->cb_thread = NULL;
		return client;
	}

	/* start the callback thread */
	client->cb_thread = kthread_run(rpc_clients_cb_thread, client,
					"k%sclntcbd", name);
	if (IS_ERR(client->cb_thread)) {
		rc = PTR_ERR(client->cb_thread);
		client->exit_flag = 1;
		msm_rpc_read_wakeup(client->ept);
		wait_for_completion(&client->complete);
		msm_rpc_close(client->ept);
		msm_rpc_destroy_client(client);
		return ERR_PTR(rc);
	}

	return client;
}
EXPORT_SYMBOL(msm_rpc_register_client);

/*
 * Interface to be used to register the client.
 *
 * name: string representing the client
 *
 * prog: program number of the client
 *
 * ver: version number of the client
 *
 * create_cb_thread: if set calls the callback function from a seprate thread
 *                   which helps the client requests to be processed without
 *                   getting loaded by callback handling.
 *
 * cb_func: function to be called if callback request is received.
 *          unmarshaling should be handled by the user in callback function
 *
 * Return Value:
 *        Pointer to initialized client data sturcture
 *        Or, the error code if registration fails.
 *
 */
struct msm_rpc_client *msm_rpc_register_client2(
	const char *name,
	uint32_t prog, uint32_t ver,
	uint32_t create_cb_thread,
	int (*cb_func)(struct msm_rpc_client *,
		       struct rpc_request_hdr *req, struct msm_rpc_xdr *))
{
	struct msm_rpc_client *client;
	struct msm_rpc_endpoint *ept;
	int rc;

	client = msm_rpc_create_client();
	if (IS_ERR(client))
		return client;

	ept = msm_rpc_connect_compatible(prog, ver, MSM_RPC_UNINTERRUPTIBLE);
	if (IS_ERR(ept)) {
		msm_rpc_destroy_client(client);
		return (struct msm_rpc_client *)ept;
	}

	client->prog = prog;
	client->ver = ver;
	client->ept = client->xdr.ept = client->cb_xdr.ept = ept;
	client->cb_func2 = cb_func;
	client->version = 2;

	ept->client_data = client;
	ept->cb_restart_teardown = cb_restart_teardown;
	ept->cb_restart_setup = cb_restart_setup;

	/* start the read thread */
	client->read_thread = kthread_run(rpc_clients_thread, client,
					  "k%sclntd", name);
	if (IS_ERR(client->read_thread)) {
		rc = PTR_ERR(client->read_thread);
		msm_rpc_close(client->ept);
		msm_rpc_destroy_client(client);
		return ERR_PTR(rc);
	}

	if (!create_cb_thread || (cb_func == NULL)) {
		client->cb_thread = NULL;
		return client;
	}

	/* start the callback thread */
	client->cb_thread = kthread_run(rpc_clients_cb_thread, client,
					"k%sclntcbd", name);
	if (IS_ERR(client->cb_thread)) {
		rc = PTR_ERR(client->cb_thread);
		client->exit_flag = 1;
		msm_rpc_read_wakeup(client->ept);
		wait_for_completion(&client->complete);
		msm_rpc_close(client->ept);
		msm_rpc_destroy_client(client);
		return ERR_PTR(rc);
	}

	return client;
}
EXPORT_SYMBOL(msm_rpc_register_client2);

/*
 * Register callbacks for modem state changes.
 *
 * Teardown is called when the modem is going into reset.
 * Setup is called after the modem has come out of reset (but may not
 * be available, yet).
 *
 * client: pointer to client data structure.
 *
 * Return Value:
 *        0 (success)
 *        1 (client pointer invalid)
 */
int msm_rpc_register_reset_callbacks(
	struct msm_rpc_client *client,
	void (*teardown)(struct msm_rpc_client *client),
	void (*setup)(struct msm_rpc_client *client)
	)
{
	int rc = 1;

	if (client) {
		client->cb_restart_teardown = teardown;
		client->cb_restart_setup = setup;
		rc = 0;
	}

	return rc;
}
EXPORT_SYMBOL(msm_rpc_register_reset_callbacks);

/*
 * Interface to be used to unregister the client
 * No client operations should be done once the unregister function
 * is called.
 *
 * client: pointer to client data structure.
 *
 * Return Value:
 *        Always returns 0 (success).
 */
int msm_rpc_unregister_client(struct msm_rpc_client *client)
{
	pr_info("%s: stopping client...\n", __func__);
	client->exit_flag = 1;
	if (client->cb_thread) {
		client->cb_avail = 1;
		wake_up(&client->cb_wait);
		wait_for_completion(&client->cb_complete);
	}

	msm_rpc_read_wakeup(client->ept);
	wait_for_completion(&client->complete);

	msm_rpc_close(client->ept);
	msm_rpc_remove_all_cb_func(client);
	xdr_clean_output(&client->xdr);
	xdr_clean_output(&client->cb_xdr);
	kfree(client);
	return 0;
}
EXPORT_SYMBOL(msm_rpc_unregister_client);

/*
 * Interface to be used to send a client request.
 * If the request takes any arguments or expects any return, the user
 * should handle it in 'arg_func' and 'ret_func' respectively.
 * Marshaling and Unmarshaling should be handled by the user in argument
 * and return functions.
 *
 * client: pointer to client data sturcture
 *
 * proc: procedure being requested
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
int msm_rpc_client_req(struct msm_rpc_client *client, uint32_t proc,
		       int (*arg_func)(struct msm_rpc_client *client,
				       void *buf, void *data),
		       void *arg_data,
		       int (*ret_func)(struct msm_rpc_client *client,
				       void *buf, void *data),
		       void *ret_data, long timeout)
{
	struct rpc_reply_hdr *rpc_rsp;
	int rc = 0;
	uint32_t req_xid;

	mutex_lock(&client->req_lock);

	msm_rpc_setup_req((struct rpc_request_hdr *)client->xdr.out_buf,
			  client->prog, client->ver, proc);
	client->xdr.out_index = sizeof(struct rpc_request_hdr);
	req_xid = *(uint32_t *)client->xdr.out_buf;
	if (arg_func) {
		rc = arg_func(client,
			      (void *)((struct rpc_request_hdr *)
				       client->xdr.out_buf + 1),
			      arg_data);
		if (rc < 0)
			goto release_locks;
		else
			client->xdr.out_index += rc;
	}

	rc = msm_rpc_write(client->ept, client->xdr.out_buf,
			   client->xdr.out_index);
	if (rc < 0) {
		pr_err("%s: couldn't send RPC request:%d\n", __func__, rc);
		goto release_locks;
	} else
		rc = 0;

	if (timeout < 0)
		timeout = msecs_to_jiffies(10000);

	do {
		rc = wait_event_timeout(client->reply_wait,
			xdr_read_avail(&client->xdr) || client->in_reset,
			timeout);

		if (client->in_reset) {
			rc = -ENETRESET;
			goto release_locks;
		}

		if (rc == 0) {
			pr_err("%s: request timeout\n", __func__);
			rc = -ETIMEDOUT;
			goto release_locks;
		}

		rpc_rsp = (struct rpc_reply_hdr *)client->xdr.in_buf;
		if (req_xid != rpc_rsp->xid) {
			pr_info("%s: xid mismatch, req %d reply %d\n",
			       __func__, be32_to_cpu(req_xid),
			       be32_to_cpu(rpc_rsp->xid));
			timeout = rc;
			xdr_clean_input(&client->xdr);
		} else
			rc = 0;
	} while (rc);

	if (be32_to_cpu(rpc_rsp->reply_stat) != RPCMSG_REPLYSTAT_ACCEPTED) {
		pr_err("%s: RPC call was denied! %d\n", __func__,
		       be32_to_cpu(rpc_rsp->reply_stat));
		rc = -EPERM;
		goto free_and_release;
	}

	if (be32_to_cpu(rpc_rsp->data.acc_hdr.accept_stat) !=
	    RPC_ACCEPTSTAT_SUCCESS) {
		pr_err("%s: RPC call was not successful (%d)\n", __func__,
		       be32_to_cpu(rpc_rsp->data.acc_hdr.accept_stat));
		rc = -EINVAL;
		goto free_and_release;
	}

	if (ret_func)
		rc = ret_func(client, (void *)(rpc_rsp + 1), ret_data);

 free_and_release:
	xdr_clean_input(&client->xdr);
	client->xdr.out_index = 0;
 release_locks:
	mutex_unlock(&client->req_lock);
	return rc;
}
EXPORT_SYMBOL(msm_rpc_client_req);

/*
 * Interface to be used to send a client request.
 * If the request takes any arguments or expects any return, the user
 * should handle it in 'arg_func' and 'ret_func' respectively.
 * Marshaling and Unmarshaling should be handled by the user in argument
 * and return functions.
 *
 * client: pointer to client data sturcture
 *
 * proc: procedure being requested
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
int msm_rpc_client_req2(struct msm_rpc_client *client, uint32_t proc,
			int (*arg_func)(struct msm_rpc_client *client,
					struct msm_rpc_xdr *xdr, void *data),
			void *arg_data,
			int (*ret_func)(struct msm_rpc_client *client,
					struct msm_rpc_xdr *xdr, void *data),
			void *ret_data, long timeout)
{
	struct rpc_reply_hdr rpc_rsp;
	int rc = 0;
	uint32_t req_xid;

	mutex_lock(&client->req_lock);

	if (client->in_reset) {
		rc = -ENETRESET;
		goto release_locks;
	}

	xdr_start_request(&client->xdr, client->prog, client->ver, proc);
	req_xid = be32_to_cpu(*(uint32_t *)client->xdr.out_buf);
	if (arg_func) {
		rc = arg_func(client, &client->xdr, arg_data);
		if (rc < 0) {
			mutex_unlock(&client->xdr.out_lock);
			goto release_locks;
		}
	}

	rc = xdr_send_msg(&client->xdr);
	if (rc < 0) {
		pr_err("%s: couldn't send RPC request:%d\n", __func__, rc);
		goto release_locks;
	} else
		rc = 0;

	if (timeout < 0)
		timeout = msecs_to_jiffies(10000);

	do {
		rc = wait_event_timeout(client->reply_wait,
			xdr_read_avail(&client->xdr) || client->in_reset,
			timeout);

		if (client->in_reset) {
			rc = -ENETRESET;
			goto release_locks;
		}

		if (rc == 0) {
			pr_err("%s: request timeout\n", __func__);
			rc = -ETIMEDOUT;
			goto release_locks;
		}

		xdr_recv_reply(&client->xdr, &rpc_rsp);
		/* TODO: may be this check should be a xdr function */
		if (req_xid != rpc_rsp.xid) {
			pr_info("%s: xid mismatch, req %d reply %d\n",
				__func__, req_xid, rpc_rsp.xid);
			timeout = rc;
			xdr_clean_input(&client->xdr);
		} else
			rc = 0;
	} while (rc);

	if (rpc_rsp.reply_stat != RPCMSG_REPLYSTAT_ACCEPTED) {
		pr_err("%s: RPC call was denied! %d\n",
		       __func__, rpc_rsp.reply_stat);
		rc = -EPERM;
		goto free_and_release;
	}

	if (rpc_rsp.data.acc_hdr.accept_stat != RPC_ACCEPTSTAT_SUCCESS) {
		pr_err("%s: RPC call was not successful (%d)\n", __func__,
		       rpc_rsp.data.acc_hdr.accept_stat);
		rc = -EINVAL;
		goto free_and_release;
	}

	if (ret_func)
		rc = ret_func(client, &client->xdr, ret_data);

 free_and_release:
	xdr_clean_input(&client->xdr);
	/* TODO: put it in xdr_reset_output */
	client->xdr.out_index = 0;
 release_locks:
	mutex_unlock(&client->req_lock);
	return rc;
}
EXPORT_SYMBOL(msm_rpc_client_req2);

/*
 * Interface to be used to start accepted reply message required in
 * callback handling. Returns the buffer pointer to attach any
 * payload.  Should call msm_rpc_send_accepted_reply to complete
 * sending reply.  Marshaling should be handled by user for the payload.
 *
 * client: pointer to client data structure
 *
 * xid: transaction id. Has to be same as the one in callback request.
 *
 * accept_status: acceptance status
 *
 * Return Value:
 *        pointer to buffer to attach the payload.
 */
void *msm_rpc_start_accepted_reply(struct msm_rpc_client *client,
				   uint32_t xid, uint32_t accept_status)
{
	struct rpc_reply_hdr *reply;

	mutex_lock(&client->cb_xdr.out_lock);

	reply = (struct rpc_reply_hdr *)client->cb_xdr.out_buf;

	reply->xid = cpu_to_be32(xid);
	reply->type = cpu_to_be32(1); /* reply */
	reply->reply_stat = cpu_to_be32(RPCMSG_REPLYSTAT_ACCEPTED);

	reply->data.acc_hdr.accept_stat = cpu_to_be32(accept_status);
	reply->data.acc_hdr.verf_flavor = 0;
	reply->data.acc_hdr.verf_length = 0;

	client->cb_xdr.out_index = sizeof(*reply);
	return reply + 1;
}
EXPORT_SYMBOL(msm_rpc_start_accepted_reply);

/*
 * Interface to be used to send accepted reply required in callback handling.
 * msm_rpc_start_accepted_reply should have been called before.
 * Marshaling should be handled by user for the payload.
 *
 * client: pointer to client data structure
 *
 * size: additional payload size
 *
 * Return Value:
 *        0 on success, otherwise returns an error code.
 */
int msm_rpc_send_accepted_reply(struct msm_rpc_client *client, uint32_t size)
{
	int rc = 0;

	client->cb_xdr.out_index += size;
	rc = msm_rpc_write(client->ept, client->cb_xdr.out_buf,
			   client->cb_xdr.out_index);
	if (rc > 0)
		rc = 0;

	mutex_unlock(&client->cb_xdr.out_lock);
	return rc;
}
EXPORT_SYMBOL(msm_rpc_send_accepted_reply);

/*
 * Interface to be used to add a callback function.
 * If the call back function is already in client's 'cb_id - cb_func'
 * table, then that cb_id is returned.  otherwise, new entry
 * is added to the above table and corresponding cb_id is returned.
 *
 * client: pointer to client data structure
 *
 * cb_func: callback function
 *
 * Return Value:
 *         callback ID on success, otherwise returns an error code.
 *         If cb_func is NULL, the callback Id returned is 0xffffffff.
 *         This tells the other processor that no callback is reqested.
 */
int msm_rpc_add_cb_func(struct msm_rpc_client *client, void *cb_func)
{
	struct msm_rpc_cb_table_item *cb_item;

	if (cb_func == NULL)
		return MSM_RPC_CLIENT_NULL_CB_ID;

	mutex_lock(&client->cb_list_lock);
	list_for_each_entry(cb_item, &client->cb_list, list) {
		if (cb_item->cb_func == cb_func) {
			mutex_unlock(&client->cb_list_lock);
			return cb_item->cb_id;
		}
	}
	mutex_unlock(&client->cb_list_lock);

	cb_item = kmalloc(sizeof(struct msm_rpc_cb_table_item), GFP_KERNEL);
	if (!cb_item)
		return -ENOMEM;

	INIT_LIST_HEAD(&cb_item->list);
	cb_item->cb_id = atomic_add_return(1, &client->next_cb_id);
	cb_item->cb_func = cb_func;

	mutex_lock(&client->cb_list_lock);
	list_add_tail(&cb_item->list, &client->cb_list);
	mutex_unlock(&client->cb_list_lock);

	return cb_item->cb_id;
}
EXPORT_SYMBOL(msm_rpc_add_cb_func);

/*
 * Interface to be used to get a callback function from a callback ID.
 * If no entry is found, NULL is returned.
 *
 * client: pointer to client data structure
 *
 * cb_id: callback ID
 *
 * Return Value:
 *         callback function pointer if entry with given cb_id is found,
 *         otherwise returns NULL.
 */
void *msm_rpc_get_cb_func(struct msm_rpc_client *client, uint32_t cb_id)
{
	struct msm_rpc_cb_table_item *cb_item;

	mutex_lock(&client->cb_list_lock);
	list_for_each_entry(cb_item, &client->cb_list, list) {
		if (cb_item->cb_id == cb_id) {
			mutex_unlock(&client->cb_list_lock);
			return cb_item->cb_func;
		}
	}
	mutex_unlock(&client->cb_list_lock);
	return NULL;
}
EXPORT_SYMBOL(msm_rpc_get_cb_func);

/*
 * Interface to be used to remove a callback function.
 *
 * client: pointer to client data structure
 *
 * cb_func: callback function
 *
 */
void msm_rpc_remove_cb_func(struct msm_rpc_client *client, void *cb_func)
{
	struct msm_rpc_cb_table_item *cb_item, *tmp_cb_item;

	if (cb_func == NULL)
		return;

	mutex_lock(&client->cb_list_lock);
	list_for_each_entry_safe(cb_item, tmp_cb_item,
				 &client->cb_list, list) {
		if (cb_item->cb_func == cb_func) {
			list_del(&cb_item->list);
			kfree(cb_item);
			mutex_unlock(&client->cb_list_lock);
			return;
		}
	}
	mutex_unlock(&client->cb_list_lock);
}
EXPORT_SYMBOL(msm_rpc_remove_cb_func);
