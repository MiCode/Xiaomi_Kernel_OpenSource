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
 * SMD RPC PING MODEM Driver
 */

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/kfifo.h>
#include <mach/msm_rpcrouter.h>

#define PING_TEST_BASE 0x31

#define PTIOC_NULL_TEST _IO(PING_TEST_BASE, 1)
#define PTIOC_REG_TEST _IO(PING_TEST_BASE, 2)
#define PTIOC_DATA_REG_TEST _IO(PING_TEST_BASE, 3)
#define PTIOC_DATA_CB_REG_TEST _IO(PING_TEST_BASE, 4)

#define PING_MDM_PROG  0x30000081
#define PING_MDM_VERS  0x00010001
#define PING_MDM_CB_PROG  0x31000081
#define PING_MDM_CB_VERS  0x00010001

#define PING_MDM_NULL_PROC                        0
#define PING_MDM_RPC_GLUE_CODE_INFO_REMOTE_PROC   1
#define PING_MDM_REGISTER_PROC                    2
#define PING_MDM_UNREGISTER_PROC                  3
#define PING_MDM_REGISTER_DATA_PROC               4
#define PING_MDM_UNREGISTER_DATA_CB_PROC          5
#define PING_MDM_REGISTER_DATA_CB_PROC            6

#define PING_MDM_DATA_CB_PROC            1
#define PING_MDM_CB_PROC                 2

#define PING_MAX_RETRY			5

static struct msm_rpc_client *rpc_client;
static uint32_t open_count;
static DEFINE_MUTEX(ping_mdm_lock);

struct ping_mdm_register_cb_arg {
	uint32_t cb_id;
	int val;
};

struct ping_mdm_register_data_cb_cb_arg {
	uint32_t cb_id;
	uint32_t *data;
	uint32_t size;
	uint32_t sum;
};

struct ping_mdm_register_data_cb_cb_ret {
	uint32_t result;
};

static struct dentry *dent;
static uint32_t test_res;
static int reg_cb_num, reg_cb_num_req;
static int data_cb_num, data_cb_num_req;
static int reg_done_flag, data_cb_done_flag;
static DECLARE_WAIT_QUEUE_HEAD(reg_test_wait);
static DECLARE_WAIT_QUEUE_HEAD(data_cb_test_wait);

enum {
	PING_MODEM_NOT_IN_RESET = 0,
	PING_MODEM_IN_RESET,
	PING_LEAVING_RESET,
	PING_MODEM_REGISTER_CB
};
static int fifo_event;
static DEFINE_MUTEX(event_fifo_lock);
static DEFINE_KFIFO(event_fifo, int, sizeof(int)*16);

static int ping_mdm_register_cb(struct msm_rpc_client *client,
				struct msm_rpc_xdr *xdr)
{
	int rc;
	uint32_t accept_status;
	struct ping_mdm_register_cb_arg arg;
	void *cb_func;

	xdr_recv_uint32(xdr, &arg.cb_id);             /* cb_id */
	xdr_recv_int32(xdr, &arg.val);                /* val */

	cb_func = msm_rpc_get_cb_func(client, arg.cb_id);
	if (cb_func) {
		rc = ((int (*)(struct ping_mdm_register_cb_arg *, void *))
		      cb_func)(&arg, NULL);
		if (rc)
			accept_status = RPC_ACCEPTSTAT_SYSTEM_ERR;
		else
			accept_status = RPC_ACCEPTSTAT_SUCCESS;
	} else
		accept_status = RPC_ACCEPTSTAT_SYSTEM_ERR;

	xdr_start_accepted_reply(xdr, accept_status);
	rc = xdr_send_msg(xdr);
	if (rc)
		pr_err("%s: send accepted reply failed: %d\n", __func__, rc);

	return rc;
}

static int ping_mdm_data_cb(struct msm_rpc_client *client,
			    struct msm_rpc_xdr *xdr)
{
	int rc;
	void *cb_func;
	uint32_t size, accept_status;
	struct ping_mdm_register_data_cb_cb_arg arg;
	struct ping_mdm_register_data_cb_cb_ret ret;

	xdr_recv_uint32(xdr, &arg.cb_id);           /* cb_id */

	/* data */
	xdr_recv_array(xdr, (void **)(&(arg.data)), &size, 64,
		       sizeof(uint32_t), (void *)xdr_recv_uint32);

	xdr_recv_uint32(xdr, &arg.size);           /* size */
	xdr_recv_uint32(xdr, &arg.sum);            /* sum */

	cb_func = msm_rpc_get_cb_func(client, arg.cb_id);
	if (cb_func) {
		rc = ((int (*)
		       (struct ping_mdm_register_data_cb_cb_arg *,
			struct ping_mdm_register_data_cb_cb_ret *))
		      cb_func)(&arg, &ret);
		if (rc)
			accept_status = RPC_ACCEPTSTAT_SYSTEM_ERR;
		else
			accept_status = RPC_ACCEPTSTAT_SUCCESS;
	} else
		accept_status = RPC_ACCEPTSTAT_SYSTEM_ERR;

	xdr_start_accepted_reply(xdr, accept_status);

	if (accept_status == RPC_ACCEPTSTAT_SUCCESS)
		xdr_send_uint32(xdr, &ret.result);         /* result */

	rc = xdr_send_msg(xdr);
	if (rc)
		pr_err("%s: send accepted reply failed: %d\n", __func__, rc);

	kfree(arg.data);
	return rc;
}

static int ping_mdm_cb_func(struct msm_rpc_client *client,
			    struct rpc_request_hdr *req,
			    struct msm_rpc_xdr *xdr)
{
	int rc = 0;

	switch (req->procedure) {
	case PING_MDM_CB_PROC:
		rc = ping_mdm_register_cb(client, xdr);
		break;
	case PING_MDM_DATA_CB_PROC:
		rc = ping_mdm_data_cb(client, xdr);
		break;
	default:
		pr_err("%s: procedure not supported %d\n",
		       __func__, req->procedure);
		xdr_start_accepted_reply(xdr, RPC_ACCEPTSTAT_PROC_UNAVAIL);
		rc = xdr_send_msg(xdr);
		if (rc)
			pr_err("%s: sending reply failed: %d\n", __func__, rc);
		break;
	}
	return rc;
}

struct ping_mdm_unregister_data_cb_arg {
	int (*cb_func)(
		struct ping_mdm_register_data_cb_cb_arg *arg,
		struct ping_mdm_register_data_cb_cb_ret *ret);
};

struct ping_mdm_register_data_cb_arg {
	int (*cb_func)(
		struct ping_mdm_register_data_cb_cb_arg *arg,
		struct ping_mdm_register_data_cb_cb_ret *ret);
	uint32_t num;
	uint32_t size;
	uint32_t interval_ms;
	uint32_t num_tasks;
};

struct ping_mdm_register_data_cb_ret {
	uint32_t result;
};

struct ping_mdm_unregister_data_cb_ret {
	uint32_t result;
};

static int ping_mdm_data_cb_register_arg(struct msm_rpc_client *client,
					 struct msm_rpc_xdr *xdr, void *data)
{
	struct ping_mdm_register_data_cb_arg *arg = data;
	int cb_id;

	cb_id = msm_rpc_add_cb_func(client, (void *)arg->cb_func);
	if ((cb_id < 0) && (cb_id != MSM_RPC_CLIENT_NULL_CB_ID))
		return cb_id;

	xdr_send_uint32(xdr, &cb_id);                /* cb_id */
	xdr_send_uint32(xdr, &arg->num);             /* num */
	xdr_send_uint32(xdr, &arg->size);            /* size */
	xdr_send_uint32(xdr, &arg->interval_ms);     /* interval_ms */
	xdr_send_uint32(xdr, &arg->num_tasks);       /* num_tasks */

	return 0;
}

static int ping_mdm_data_cb_unregister_arg(struct msm_rpc_client *client,
					   struct msm_rpc_xdr *xdr, void *data)
{
	struct ping_mdm_unregister_data_cb_arg *arg = data;
	int cb_id;

	cb_id = msm_rpc_add_cb_func(client, (void *)arg->cb_func);
	if ((cb_id < 0) && (cb_id != MSM_RPC_CLIENT_NULL_CB_ID))
		return cb_id;

	xdr_send_uint32(xdr, &cb_id);                /* cb_id */

	return 0;
}

static int ping_mdm_data_cb_register_ret(struct msm_rpc_client *client,
					 struct msm_rpc_xdr *xdr, void *data)
{
	struct ping_mdm_register_data_cb_ret *ret = data;

	xdr_recv_uint32(xdr, &ret->result);      /* result */

	return 0;
}

static int ping_mdm_register_data_cb(
	struct msm_rpc_client *client,
	struct ping_mdm_register_data_cb_arg *arg,
	struct ping_mdm_register_data_cb_ret *ret)
{
	return msm_rpc_client_req2(client,
				   PING_MDM_REGISTER_DATA_CB_PROC,
				   ping_mdm_data_cb_register_arg, arg,
				   ping_mdm_data_cb_register_ret, ret, -1);
}

static int ping_mdm_unregister_data_cb(
	struct msm_rpc_client *client,
	struct ping_mdm_unregister_data_cb_arg *arg,
	struct ping_mdm_unregister_data_cb_ret *ret)
{
	return msm_rpc_client_req2(client,
				   PING_MDM_UNREGISTER_DATA_CB_PROC,
				   ping_mdm_data_cb_unregister_arg, arg,
				   ping_mdm_data_cb_register_ret, ret, -1);
}

struct ping_mdm_data_arg {
	uint32_t *data;
	uint32_t size;
};

struct ping_mdm_data_ret {
	uint32_t result;
};

static int ping_mdm_data_register_arg(struct msm_rpc_client *client,
				      struct msm_rpc_xdr *xdr, void *data)
{
	struct ping_mdm_data_arg *arg = data;

	/* data */
	xdr_send_array(xdr, (void **)&arg->data, &arg->size, 64,
	       sizeof(uint32_t), (void *)xdr_send_uint32);

	xdr_send_uint32(xdr, &arg->size);             /* size */

	return 0;
}

static int ping_mdm_data_register_ret(struct msm_rpc_client *client,
				      struct msm_rpc_xdr *xdr, void *data)
{
	struct ping_mdm_data_ret *ret = data;

	xdr_recv_uint32(xdr, &ret->result);      /* result */

	return 0;
}

static int ping_mdm_data_register(
	struct msm_rpc_client *client,
	struct ping_mdm_data_arg *arg,
	struct ping_mdm_data_ret *ret)
{
	return msm_rpc_client_req2(client,
				   PING_MDM_REGISTER_DATA_PROC,
				   ping_mdm_data_register_arg, arg,
				   ping_mdm_data_register_ret, ret, -1);
}

struct ping_mdm_register_arg {
	int (*cb_func)(struct ping_mdm_register_cb_arg *, void *);
	int num;
};

struct ping_mdm_unregister_arg {
	int (*cb_func)(struct ping_mdm_register_cb_arg *, void *);
};

struct ping_mdm_register_ret {
	uint32_t result;
};

struct ping_mdm_unregister_ret {
	uint32_t result;
};

static int ping_mdm_register_arg(struct msm_rpc_client *client,
				 struct msm_rpc_xdr *xdr, void *data)
{
	struct ping_mdm_register_arg *arg = data;
	int cb_id;

	cb_id = msm_rpc_add_cb_func(client, (void *)arg->cb_func);
	if ((cb_id < 0) && (cb_id != MSM_RPC_CLIENT_NULL_CB_ID))
		return cb_id;

	xdr_send_uint32(xdr, &cb_id);             /* cb_id */
	xdr_send_uint32(xdr, &arg->num);          /* num */

	return 0;
}

static int ping_mdm_unregister_arg(struct msm_rpc_client *client,
				   struct msm_rpc_xdr *xdr, void *data)
{
	struct ping_mdm_unregister_arg *arg = data;
	int cb_id;

	cb_id = msm_rpc_add_cb_func(client, (void *)arg->cb_func);
	if ((cb_id < 0) && (cb_id != MSM_RPC_CLIENT_NULL_CB_ID))
		return cb_id;

	xdr_send_uint32(xdr, &cb_id);             /* cb_id */

	return 0;
}

static int ping_mdm_register_ret(struct msm_rpc_client *client,
				 struct msm_rpc_xdr *xdr, void *data)
{
	struct ping_mdm_register_ret *ret = data;

	xdr_recv_uint32(xdr, &ret->result);      /* result */

	return 0;
}

static int ping_mdm_register(
	struct msm_rpc_client *client,
	struct ping_mdm_register_arg *arg,
	struct ping_mdm_register_ret *ret)
{
	return msm_rpc_client_req2(client,
				   PING_MDM_REGISTER_PROC,
				   ping_mdm_register_arg, arg,
				   ping_mdm_register_ret, ret, -1);
}

static int ping_mdm_unregister(
	struct msm_rpc_client *client,
	struct ping_mdm_unregister_arg *arg,
	struct ping_mdm_unregister_ret *ret)
{
	return msm_rpc_client_req2(client,
				   PING_MDM_UNREGISTER_PROC,
				   ping_mdm_unregister_arg, arg,
				   ping_mdm_register_ret, ret, -1);
}

static int ping_mdm_null(struct msm_rpc_client *client,
			 void *arg, void *ret)
{
	return msm_rpc_client_req2(client, PING_MDM_NULL_PROC,
				   NULL, NULL, NULL, NULL, -1);
}

static int ping_mdm_close(void)
{
	mutex_lock(&ping_mdm_lock);
	if (--open_count == 0) {
		msm_rpc_unregister_client(rpc_client);
		pr_info("%s: disconnected from remote ping server\n",
			__func__);
	}
	mutex_unlock(&ping_mdm_lock);
	return 0;
}

static void handle_restart_teardown(struct msm_rpc_client *client)
{
	int	event = PING_MODEM_IN_RESET;

	pr_info("%s: modem in reset\n", __func__);

	mutex_lock(&event_fifo_lock);
	kfifo_in(&event_fifo, &event, sizeof(event));
	fifo_event = 1;
	mutex_unlock(&event_fifo_lock);

	wake_up(&data_cb_test_wait);
}

static void handle_restart_setup(struct msm_rpc_client *client)
{
	int	event = PING_LEAVING_RESET;

	pr_info("%s: modem leaving reset\n", __func__);

	mutex_lock(&event_fifo_lock);
	kfifo_in(&event_fifo, &event, sizeof(event));
	fifo_event = 1;
	mutex_unlock(&event_fifo_lock);

	wake_up(&data_cb_test_wait);
}

static struct msm_rpc_client *ping_mdm_init(void)
{
	mutex_lock(&ping_mdm_lock);
	if (open_count == 0) {
		rpc_client = msm_rpc_register_client2("pingdef",
						      PING_MDM_PROG,
						      PING_MDM_VERS, 1,
						      ping_mdm_cb_func);
		if (!IS_ERR(rpc_client)) {
			open_count++;
			msm_rpc_register_reset_callbacks(rpc_client,
					handle_restart_teardown,
					handle_restart_setup);
		}
	}
	mutex_unlock(&ping_mdm_lock);
	return rpc_client;
}

static int ping_mdm_data_register_test(void)
{
	int i, rc = 0;
	uint32_t my_data[64];
	uint32_t my_sum = 0;
	struct ping_mdm_data_arg data_arg;
	struct ping_mdm_data_ret data_ret;

	for (i = 0; i < 64; i++) {
		my_data[i] = (42 + i);
		my_sum ^= (42 + i);
	}

	data_arg.data = my_data;
	data_arg.size = 64;

	rc = ping_mdm_data_register(rpc_client, &data_arg, &data_ret);
	if (rc)
		return rc;

	if (my_sum != data_ret.result) {
		pr_err("%s: sum mismatch %d %d\n",
		       __func__, my_sum, data_ret.result);
		rc = -1;
	}

	return rc;
}

static int ping_mdm_test_register_data_cb(
	struct ping_mdm_register_data_cb_cb_arg *arg,
	struct ping_mdm_register_data_cb_cb_ret *ret)
{
	uint32_t i, sum = 0;

	data_cb_num++;

	pr_info("%s: received cb_id %d, size = %d, sum = %u, num = %u of %u\n",
		__func__, arg->cb_id, arg->size, arg->sum, data_cb_num,
		data_cb_num_req);

	if (arg->data)
		for (i = 0; i < arg->size; i++)
			sum ^= arg->data[i];

	if (sum != arg->sum)
		pr_err("%s: sum mismatch %u %u\n", __func__, sum, arg->sum);

	if (data_cb_num == data_cb_num_req) {
		data_cb_done_flag = 1;
		wake_up(&data_cb_test_wait);
	}

	ret->result = 1;
	return 0;
}

static int ping_mdm_data_cb_register(
		struct ping_mdm_register_data_cb_ret *reg_ret)
{
	int rc;
	struct ping_mdm_register_data_cb_arg reg_arg;

	reg_arg.cb_func = ping_mdm_test_register_data_cb;
	reg_arg.num = data_cb_num_req - data_cb_num;
	reg_arg.size = 64;
	reg_arg.interval_ms = 10;
	reg_arg.num_tasks = 1;

	pr_info("%s: registering callback\n", __func__);
	rc = ping_mdm_register_data_cb(rpc_client, &reg_arg, reg_ret);
	if (rc)
		pr_err("%s: failed to register callback %d\n", __func__, rc);

	return rc;
}


static void retry_timer_cb(unsigned long data)
{
	int	event = (int)data;

	pr_info("%s: retry timer triggered\n", __func__);

	mutex_lock(&event_fifo_lock);
	kfifo_in(&event_fifo, &event, sizeof(event));
	fifo_event = 1;
	mutex_unlock(&event_fifo_lock);

	wake_up(&data_cb_test_wait);
}

static int ping_mdm_data_cb_register_test(void)
{
	int rc;
	int event;
	int retry_count = 0;
	struct ping_mdm_register_data_cb_ret reg_ret;
	struct ping_mdm_unregister_data_cb_arg unreg_arg;
	struct ping_mdm_unregister_data_cb_ret unreg_ret;
	struct timer_list retry_timer;

	mutex_init(&event_fifo_lock);
	init_timer(&retry_timer);

	data_cb_done_flag = 0;
	data_cb_num = 0;
	if (!data_cb_num_req)
		data_cb_num_req = 10;

	rc = ping_mdm_data_cb_register(&reg_ret);
	if (rc)
		return rc;

	pr_info("%s: data_cb_register result: 0x%x\n",
		__func__, reg_ret.result);

	while (!data_cb_done_flag) {
		wait_event(data_cb_test_wait, data_cb_done_flag || fifo_event);
		fifo_event = 0;

		for (;;) {
			mutex_lock(&event_fifo_lock);

			if (kfifo_is_empty(&event_fifo)) {
				mutex_unlock(&event_fifo_lock);
				break;
			}
			rc = kfifo_out(&event_fifo, &event, sizeof(event));
			mutex_unlock(&event_fifo_lock);
			BUG_ON(rc != sizeof(event));

			pr_info("%s: processing event data_cb_done_flag=%d,event=%d\n",
				__func__, data_cb_done_flag, event);

			if (event == PING_MODEM_IN_RESET) {
				pr_info("%s: modem entering reset\n", __func__);
				retry_count = 0;
			} else if (event == PING_LEAVING_RESET) {
				pr_info("%s: modem exiting reset - "
					"re-registering cb\n", __func__);

				rc = ping_mdm_data_cb_register(&reg_ret);
				if (rc) {
					retry_count++;
					if (retry_count < PING_MAX_RETRY) {
						pr_info("%s: retry %d failed\n",
							__func__, retry_count);

						retry_timer.expires = jiffies +
							msecs_to_jiffies(1000);
						retry_timer.data =
							PING_LEAVING_RESET;
						retry_timer.function =
							retry_timer_cb;
						add_timer(&retry_timer);
					} else {
						pr_err("%s: max retries exceeded, aborting\n",
								__func__);
						return -ENETRESET;
					}
				} else
					pr_info("%s: data_cb_register result: 0x%x\n",
						__func__, reg_ret.result);
			}
		}
	}

	while (del_timer(&retry_timer))
		;

	unreg_arg.cb_func = ping_mdm_test_register_data_cb;
	rc = ping_mdm_unregister_data_cb(rpc_client, &unreg_arg, &unreg_ret);
	if (rc)
		return rc;

	pr_info("%s: data_cb_unregister result: 0x%x\n",
		__func__, unreg_ret.result);

	pr_info("%s: Test completed\n", __func__);

	return 0;
}

static int ping_mdm_test_register_cb(
	struct ping_mdm_register_cb_arg *arg, void *ret)
{
	pr_info("%s: received cb_id %d, val = %d\n",
		__func__, arg->cb_id, arg->val);

	reg_cb_num++;
	if (reg_cb_num == reg_cb_num_req) {
		reg_done_flag = 1;
		wake_up(&reg_test_wait);
	}
	return 0;
}

static int ping_mdm_register_test(void)
{
	int rc = 0;
	struct ping_mdm_register_arg reg_arg;
	struct ping_mdm_unregister_arg unreg_arg;
	struct ping_mdm_register_ret reg_ret;
	struct ping_mdm_unregister_ret unreg_ret;

	reg_cb_num = 0;
	reg_cb_num_req = 10;
	reg_done_flag = 0;

	reg_arg.num = 10;
	reg_arg.cb_func = ping_mdm_test_register_cb;

	rc = ping_mdm_register(rpc_client, &reg_arg, &reg_ret);
	if (rc)
		return rc;

	pr_info("%s: register result: 0x%x\n",
		__func__, reg_ret.result);

	wait_event(reg_test_wait, reg_done_flag);

	unreg_arg.cb_func = ping_mdm_test_register_cb;
	rc = ping_mdm_unregister(rpc_client, &unreg_arg, &unreg_ret);
	if (rc)
		return rc;

	pr_info("%s: unregister result: 0x%x\n",
		__func__, unreg_ret.result);

	return 0;
}

static int ping_mdm_null_test(void)
{
	return ping_mdm_null(rpc_client, NULL, NULL);
}

static int ping_test_release(struct inode *ip, struct file *fp)
{
	return ping_mdm_close();
}

static int ping_test_open(struct inode *ip, struct file *fp)
{
	struct msm_rpc_client *client;

	client = ping_mdm_init();
	if (IS_ERR(client)) {
		pr_err("%s: couldn't open ping client\n", __func__);
		return PTR_ERR(client);
	} else
		pr_info("%s: connected to remote ping server\n",
			__func__);

	return 0;
}

static ssize_t ping_test_read(struct file *fp, char __user *buf,
			size_t count, loff_t *pos)
{
	char _buf[16];

	snprintf(_buf, sizeof(_buf), "%i\n", test_res);

	return simple_read_from_buffer(buf, count, pos, _buf, strlen(_buf));
}

static ssize_t ping_test_write(struct file *fp, const char __user *buf,
			 size_t count, loff_t *pos)
{
	unsigned char cmd[64];
	int len;

	if (count < 1)
		return 0;

	len = count > 63 ? 63 : count;

	if (copy_from_user(cmd, buf, len))
		return -EFAULT;

	cmd[len] = 0;

	/* lazy */
	if (cmd[len-1] == '\n') {
		cmd[len-1] = 0;
		len--;
	}

	if (!strncmp(cmd, "null_test", 64))
		test_res = ping_mdm_null_test();
	else if (!strncmp(cmd, "reg_test", 64))
		test_res = ping_mdm_register_test();
	else if (!strncmp(cmd, "data_reg_test", 64))
		test_res = ping_mdm_data_register_test();
	else if (!strncmp(cmd, "data_cb_reg_test", 64))
		test_res = ping_mdm_data_cb_register_test();
	else if (!strncmp(cmd, "count=", 6)) {
		long tmp;

		if (strict_strtol(cmd + 6, 0, &tmp) == 0) {
			data_cb_num_req = tmp;
			pr_info("Set repetition count to %d\n",
				data_cb_num_req);
		} else {
			data_cb_num_req = 10;
			pr_err("invalid number %s, defaulting to %d\n",
				cmd + 6, data_cb_num_req);
		}
	}
	else
		test_res = -EINVAL;

	return count;
}

static const struct file_operations debug_ops = {
	.owner = THIS_MODULE,
	.open = ping_test_open,
	.read = ping_test_read,
	.write = ping_test_write,
	.release = ping_test_release,
};

static void __exit ping_test_exit(void)
{
	debugfs_remove(dent);
}

static int __init ping_test_init(void)
{
	dent = debugfs_create_file("ping_mdm", 0444, 0, NULL, &debug_ops);
	test_res = 0;
	open_count = 0;
	return 0;
}

module_init(ping_test_init);
module_exit(ping_test_exit);

MODULE_DESCRIPTION("PING TEST Driver");
MODULE_LICENSE("GPL v2");
