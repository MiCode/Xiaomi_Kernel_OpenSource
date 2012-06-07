/* Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
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
 * OEM RAPI CLIENT Driver source file
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <mach/msm_rpcrouter.h>
#include <mach/oem_rapi_client.h>

#define OEM_RAPI_PROG  0x3000006B
#define OEM_RAPI_VERS  0x00010001

#define OEM_RAPI_NULL_PROC                        0
#define OEM_RAPI_RPC_GLUE_CODE_INFO_REMOTE_PROC   1
#define OEM_RAPI_STREAMING_FUNCTION_PROC          2

#define OEM_RAPI_CLIENT_MAX_OUT_BUFF_SIZE 128

static struct msm_rpc_client *rpc_client;
static uint32_t open_count;
static DEFINE_MUTEX(oem_rapi_client_lock);

/* TODO: check where to allocate memory for return */
static int oem_rapi_client_cb(struct msm_rpc_client *client,
			      struct rpc_request_hdr *req,
			      struct msm_rpc_xdr *xdr)
{
	uint32_t cb_id, accept_status;
	int rc;
	void *cb_func;
	uint32_t temp;

	struct oem_rapi_client_streaming_func_cb_arg arg;
	struct oem_rapi_client_streaming_func_cb_ret ret;

	arg.input = NULL;
	ret.out_len = NULL;
	ret.output = NULL;

	xdr_recv_uint32(xdr, &cb_id);                    /* cb_id */
	xdr_recv_uint32(xdr, &arg.event);                /* enum */
	xdr_recv_uint32(xdr, (uint32_t *)(&arg.handle)); /* handle */
	xdr_recv_uint32(xdr, &arg.in_len);               /* in_len */
	xdr_recv_bytes(xdr, (void **)&arg.input, &temp); /* input */
	xdr_recv_uint32(xdr, &arg.out_len_valid);        /* out_len */
	if (arg.out_len_valid) {
		ret.out_len = kmalloc(sizeof(*ret.out_len), GFP_KERNEL);
		if (!ret.out_len) {
			accept_status = RPC_ACCEPTSTAT_SYSTEM_ERR;
			goto oem_rapi_send_ack;
		}
	}

	xdr_recv_uint32(xdr, &arg.output_valid);         /* out */
	if (arg.output_valid) {
		xdr_recv_uint32(xdr, &arg.output_size);  /* ouput_size */

		ret.output = kmalloc(arg.output_size, GFP_KERNEL);
		if (!ret.output) {
			accept_status = RPC_ACCEPTSTAT_SYSTEM_ERR;
			goto oem_rapi_send_ack;
		}
	}

	cb_func = msm_rpc_get_cb_func(client, cb_id);
	if (cb_func) {
		rc = ((int (*)(struct oem_rapi_client_streaming_func_cb_arg *,
			       struct oem_rapi_client_streaming_func_cb_ret *))
		      cb_func)(&arg, &ret);
		if (rc)
			accept_status = RPC_ACCEPTSTAT_SYSTEM_ERR;
		else
			accept_status = RPC_ACCEPTSTAT_SUCCESS;
	} else
		accept_status = RPC_ACCEPTSTAT_SYSTEM_ERR;

 oem_rapi_send_ack:
	xdr_start_accepted_reply(xdr, accept_status);

	if (accept_status == RPC_ACCEPTSTAT_SUCCESS) {
		uint32_t temp = sizeof(uint32_t);
		xdr_send_pointer(xdr, (void **)&(ret.out_len), temp,
				 xdr_send_uint32);

		/* output */
		if (ret.output && ret.out_len)
			xdr_send_bytes(xdr, (const void **)&ret.output,
					     ret.out_len);
		else {
			temp = 0;
			xdr_send_uint32(xdr, &temp);
		}
	}
	rc = xdr_send_msg(xdr);
	if (rc)
		pr_err("%s: sending reply failed: %d\n", __func__, rc);

	kfree(arg.input);
	kfree(ret.out_len);
	kfree(ret.output);

	return 0;
}

static int oem_rapi_client_streaming_function_arg(struct msm_rpc_client *client,
						  struct msm_rpc_xdr *xdr,
						  void *data)
{
	int cb_id;
	struct oem_rapi_client_streaming_func_arg *arg = data;

	cb_id = msm_rpc_add_cb_func(client, (void *)arg->cb_func);
	if ((cb_id < 0) && (cb_id != MSM_RPC_CLIENT_NULL_CB_ID))
		return cb_id;

	xdr_send_uint32(xdr, &arg->event);                /* enum */
	xdr_send_uint32(xdr, &cb_id);                     /* cb_id */
	xdr_send_uint32(xdr, (uint32_t *)(&arg->handle)); /* handle */
	xdr_send_uint32(xdr, &arg->in_len);               /* in_len */
	xdr_send_bytes(xdr, (const void **)&arg->input,
			     &arg->in_len);                     /* input */
	xdr_send_uint32(xdr, &arg->out_len_valid);        /* out_len */
	xdr_send_uint32(xdr, &arg->output_valid);         /* output */

	/* output_size */
	if (arg->output_valid)
		xdr_send_uint32(xdr, &arg->output_size);

	return 0;
}

static int oem_rapi_client_streaming_function_ret(struct msm_rpc_client *client,
						  struct msm_rpc_xdr *xdr,
						  void *data)
{
	struct oem_rapi_client_streaming_func_ret *ret = data;
	uint32_t temp;

	/* out_len */
	xdr_recv_pointer(xdr, (void **)&(ret->out_len), sizeof(uint32_t),
			 xdr_recv_uint32);

	/* output */
	if (ret->out_len && *ret->out_len)
		xdr_recv_bytes(xdr, (void **)&ret->output, &temp);

	return 0;
}

int oem_rapi_client_streaming_function(
	struct msm_rpc_client *client,
	struct oem_rapi_client_streaming_func_arg *arg,
	struct oem_rapi_client_streaming_func_ret *ret)
{
	return msm_rpc_client_req2(client,
				   OEM_RAPI_STREAMING_FUNCTION_PROC,
				   oem_rapi_client_streaming_function_arg, arg,
				   oem_rapi_client_streaming_function_ret,
				   ret, -1);
}
EXPORT_SYMBOL(oem_rapi_client_streaming_function);

int oem_rapi_client_close(void)
{
	mutex_lock(&oem_rapi_client_lock);
	if (--open_count == 0) {
		msm_rpc_unregister_client(rpc_client);
		pr_info("%s: disconnected from remote oem rapi server\n",
			__func__);
	}
	mutex_unlock(&oem_rapi_client_lock);
	return 0;
}
EXPORT_SYMBOL(oem_rapi_client_close);

struct msm_rpc_client *oem_rapi_client_init(void)
{
	mutex_lock(&oem_rapi_client_lock);
	if (open_count == 0) {
		rpc_client = msm_rpc_register_client2("oemrapiclient",
						      OEM_RAPI_PROG,
						      OEM_RAPI_VERS, 0,
						      oem_rapi_client_cb);
		if (!IS_ERR(rpc_client))
			open_count++;
	}
	mutex_unlock(&oem_rapi_client_lock);
	return rpc_client;
}
EXPORT_SYMBOL(oem_rapi_client_init);

#if defined(CONFIG_DEBUG_FS)

static struct dentry *dent;
static int oem_rapi_client_test_res;

static int oem_rapi_client_null(struct msm_rpc_client *client,
				void *arg, void *ret)
{
	return msm_rpc_client_req2(client, OEM_RAPI_NULL_PROC,
				   NULL, NULL, NULL, NULL, -1);
}

static int oem_rapi_client_test_streaming_cb_func(
	struct oem_rapi_client_streaming_func_cb_arg *arg,
	struct oem_rapi_client_streaming_func_cb_ret *ret)
{
	uint32_t size;

	size = (arg->in_len < OEM_RAPI_CLIENT_MAX_OUT_BUFF_SIZE) ?
		arg->in_len : OEM_RAPI_CLIENT_MAX_OUT_BUFF_SIZE;

	if (ret->out_len != 0)
		*ret->out_len = size;

	if (ret->output != 0)
		memcpy(ret->output, arg->input, size);

	return 0;
}

static ssize_t debug_read(struct file *fp, char __user *buf,
			  size_t count, loff_t *pos)
{
	char _buf[16];

	snprintf(_buf, sizeof(_buf), "%i\n", oem_rapi_client_test_res);

	return simple_read_from_buffer(buf, count, pos, _buf, strlen(_buf));
}

static ssize_t debug_write(struct file *fp, const char __user *buf,
			   size_t count, loff_t *pos)
{
	char input[OEM_RAPI_CLIENT_MAX_OUT_BUFF_SIZE];
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;

	unsigned char cmd[64];
	int len;

	if (count < 1)
		return 0;

	len = count > 63 ? 63 : count;

	if (copy_from_user(cmd, buf, len))
		return -EFAULT;

	cmd[len] = 0;

	if (cmd[len-1] == '\n') {
		cmd[len-1] = 0;
		len--;
	}

	if (!strncmp(cmd, "null", 64)) {
		oem_rapi_client_test_res = oem_rapi_client_null(rpc_client,
								NULL, NULL);
	} else if (!strncmp(cmd, "streaming_func", 64)) {
		memset(input, 5, 16);
		arg.event = 0;
		arg.cb_func = oem_rapi_client_test_streaming_cb_func;
		arg.handle = (void *)20;
		arg.in_len = 16;
		arg.input = input;
		arg.out_len_valid = 1;
		arg.output_valid = 1;
		arg.output_size = OEM_RAPI_CLIENT_MAX_OUT_BUFF_SIZE;
		ret.out_len = NULL;
		ret.output = NULL;

		oem_rapi_client_test_res = oem_rapi_client_streaming_function(
			rpc_client, &arg, &ret);

		kfree(ret.out_len);
		kfree(ret.output);

	} else
		oem_rapi_client_test_res = -EINVAL;

	if (oem_rapi_client_test_res)
		pr_err("oem rapi client test fail %d\n",
		       oem_rapi_client_test_res);
	else
		pr_info("oem rapi client test passed\n");

	return count;
}

static int debug_release(struct inode *ip, struct file *fp)
{
	return oem_rapi_client_close();
}

static int debug_open(struct inode *ip, struct file *fp)
{
	struct msm_rpc_client *client;
	client = oem_rapi_client_init();
	if (IS_ERR(client)) {
		pr_err("%s: couldn't open oem rapi client\n", __func__);
		return PTR_ERR(client);
	} else
		pr_info("%s: connected to remote oem rapi server\n", __func__);

	return 0;
}

static const struct file_operations debug_ops = {
	.owner = THIS_MODULE,
	.open = debug_open,
	.release = debug_release,
	.read = debug_read,
	.write = debug_write,
};

static void __exit oem_rapi_client_mod_exit(void)
{
	debugfs_remove(dent);
}

static int __init oem_rapi_client_mod_init(void)
{
	dent = debugfs_create_file("oem_rapi", 0444, 0, NULL, &debug_ops);
	open_count = 0;
	oem_rapi_client_test_res = -1;
	return 0;
}

module_init(oem_rapi_client_mod_init);
module_exit(oem_rapi_client_mod_exit);

#endif

MODULE_DESCRIPTION("OEM RAPI CLIENT Driver");
MODULE_LICENSE("GPL v2");
