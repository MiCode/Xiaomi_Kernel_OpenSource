/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
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
 * DOG KEEPALIVE RPC CLIENT MODULE
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <mach/msm_rpcrouter.h>

#define DOG_KEEPALIVE_PROG 0x300000A2
#define DOG_KEEPALIVE_VERS 0x00010001

#define DOG_KEEPALIVE_REGISTER_PROC      2
#define DOG_KEEPALIVE_UNREGISTER_PROC    3

#define DOG_KEEPALIVE_CB_PROC            1

static int dog_keepalive_debug;
module_param_named(debug, dog_keepalive_debug,
		   int, S_IRUGO | S_IWUSR | S_IWGRP);

#if defined(DEBUG)
#define DBG(x...)  do {		                \
		if (dog_keepalive_debug)	\
			printk(KERN_INFO x);	\
	} while (0)
#else
#define DBG(x...) do { } while (0)
#endif

static struct msm_rpc_client *dog_keepalive_rpc_client;
static int32_t dog_clnt_id = -1;

struct dog_keepalive_cb_arg {
	uint32_t cb_id;
};

struct dog_keepalive_cb_ret {
	uint32_t result;
};

static int dog_keepalive_cb(struct msm_rpc_client *client,
			    struct msm_rpc_xdr *xdr)
{
	int rc;
	void *cb_func;
	uint32_t accept_status;
	struct dog_keepalive_cb_arg arg;
	struct dog_keepalive_cb_ret ret;

	xdr_recv_uint32(xdr, &arg.cb_id);           /* cb_id */

	cb_func = msm_rpc_get_cb_func(client, arg.cb_id);
	if (cb_func) {
		rc = ((int (*)
		       (struct dog_keepalive_cb_arg *,
			struct dog_keepalive_cb_ret *))
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

	return rc;
}

static int dog_keepalive_cb_func(struct msm_rpc_client *client,
				 struct rpc_request_hdr *req,
				 struct msm_rpc_xdr *xdr)
{
	int rc = 0;

	switch (req->procedure) {
	case DOG_KEEPALIVE_CB_PROC:
		rc = dog_keepalive_cb(client, xdr);
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

struct dog_keepalive_register_arg {
	int (*cb_func)(
		struct dog_keepalive_cb_arg *arg,
		struct dog_keepalive_cb_ret *ret);
	uint32_t response_msec;
	uint32_t clnt_id_valid;
};

struct dog_keepalive_register_ret {
	uint32_t *clnt_id;
	uint32_t result;
};

static int dog_keepalive_register_arg_func(struct msm_rpc_client *client,
					   struct msm_rpc_xdr *xdr, void *data)
{
	struct dog_keepalive_register_arg *arg = data;
	int cb_id;

	/* cb_func */
	cb_id = msm_rpc_add_cb_func(client, (void *)arg->cb_func);
	if ((cb_id < 0) && (cb_id != MSM_RPC_CLIENT_NULL_CB_ID))
		return cb_id;

	xdr_send_uint32(xdr, &cb_id);
	xdr_send_uint32(xdr, &arg->response_msec);    /* response_msec */
	xdr_send_uint32(xdr, &arg->clnt_id_valid);    /* clnt_id valid */
	return 0;
}

static int dog_keepalive_register_ret_func(struct msm_rpc_client *client,
					   struct msm_rpc_xdr *xdr, void *data)
{
	struct dog_keepalive_register_ret *ret = data;

	/* clnt_id */
	xdr_recv_pointer(xdr, (void **)&(ret->clnt_id), sizeof(uint32_t),
			 xdr_recv_uint32);

	/* result */
	xdr_recv_uint32(xdr, &ret->result);
	return 0;
}

static int dog_keepalive_register_func(struct msm_rpc_client *client,
				       struct dog_keepalive_register_arg *arg,
				       struct dog_keepalive_register_ret *ret)
{
	return msm_rpc_client_req2(client,
				   DOG_KEEPALIVE_REGISTER_PROC,
				   dog_keepalive_register_arg_func, arg,
				   dog_keepalive_register_ret_func, ret, -1);
}

static int dog_keepalive_cb_proc_func(struct dog_keepalive_cb_arg *arg,
				      struct dog_keepalive_cb_ret *ret)
{
	DBG("%s: received, client %d \n", __func__, dog_clnt_id);
	ret->result = 1;
	return 0;
}

static void dog_keepalive_register(void)
{
	struct dog_keepalive_register_arg arg;
	struct dog_keepalive_register_ret ret;
	int rc;

	arg.cb_func = dog_keepalive_cb_proc_func;
	arg.response_msec = 1000;
	arg.clnt_id_valid = 1;
	ret.clnt_id = NULL;
	rc = dog_keepalive_register_func(dog_keepalive_rpc_client,
					 &arg, &ret);
	if (rc)
		pr_err("%s: register request failed\n", __func__);
	else
		dog_clnt_id = *ret.clnt_id;

	kfree(ret.clnt_id);
	DBG("%s: register complete\n", __func__);
}

/* Registration with the platform driver for notification on the availability
 * of the DOG_KEEPALIVE remote server
 */
static int dog_keepalive_init_probe(struct platform_device *pdev)
{
	DBG("%s: probe called\n", __func__);
	dog_keepalive_rpc_client = msm_rpc_register_client2(
		"dog-keepalive",
		DOG_KEEPALIVE_PROG,
		DOG_KEEPALIVE_VERS,
		0, dog_keepalive_cb_func);

	if (IS_ERR(dog_keepalive_rpc_client)) {
		pr_err("%s: RPC client creation failed\n", __func__);
		return PTR_ERR(dog_keepalive_rpc_client);
	}

	/* Send RPC call to register for callbacks */
	dog_keepalive_register();

	return 0;
}

static char dog_keepalive_driver_name[] = "rs00000000";

static struct platform_driver dog_keepalive_init_driver = {
	.probe = dog_keepalive_init_probe,
	.driver = {
		.owner = THIS_MODULE,
	},
};

static int __init rpc_dog_keepalive_init(void)
{
	snprintf(dog_keepalive_driver_name, sizeof(dog_keepalive_driver_name),
		 "rs%08x", DOG_KEEPALIVE_PROG);
	dog_keepalive_init_driver.driver.name = dog_keepalive_driver_name;

	return platform_driver_register(&dog_keepalive_init_driver);
}

late_initcall(rpc_dog_keepalive_init);
MODULE_DESCRIPTION("DOG KEEPALIVE RPC CLIENT");
MODULE_LICENSE("GPL v2");
