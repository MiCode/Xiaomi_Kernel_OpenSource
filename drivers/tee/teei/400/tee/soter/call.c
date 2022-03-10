// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <tee_drv.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/delay.h>

#define IMSG_TAG "[tz_driver]"
#include <imsg_log.h>

#include "soter_private.h"
#include "soter_smc.h"
#include <teei_cancel_cmd.h>

#ifdef CONFIG_MICROTRUST_TZDRIVER_DYNAMICAL_DEBUG
unsigned long ree_dynamical_debug;
struct timeval start_val;
struct timeval end_val;
#endif

static struct tee_shm *get_msg_arg(struct tee_context *ctx, size_t num_params,
				   struct optee_msg_arg **msg_arg,
				   phys_addr_t *msg_parg)
{
	int rc;
	struct tee_shm *shm;
	struct optee_msg_arg *ma;

	shm = isee_shm_alloc_noid(ctx, OPTEE_MSG_GET_ARG_SIZE(num_params),
			    TEE_SHM_MAPPED);
	if (IS_ERR(shm))
		return shm;

	ma = isee_shm_get_va(shm, 0);
	if (IS_ERR(ma)) {
		rc = PTR_ERR(ma);
		goto out;
	}

	rc = isee_shm_get_pa(shm, 0, msg_parg);
	if (rc)
		goto out;

	memset(ma, 0, OPTEE_MSG_GET_ARG_SIZE(num_params));
	ma->num_params = num_params;
	*msg_arg = ma;
out:
	if (rc) {
		isee_shm_free(shm);
		return ERR_PTR(rc);
	}

	return shm;
}

/* Requires the filpstate mutex to be held */
static struct soter_session *find_session(struct soter_context_data *ctxdata,
					  u32 session_id)
{
	struct soter_session *sess;

	list_for_each_entry(sess, &ctxdata->sess_list, list_node)
		if (sess->session_id == session_id)
			return sess;

	return NULL;
}

/**
 * isee_from_msg_param() - convert from OPTEE_MSG parameters to
 *			    struct tee_param
 * @params:	subsystem internal parameter representation
 * @num_params:	number of elements in the parameter arrays
 * @msg_params:	OPTEE_MSG parameters
 * Returns 0 on success or <0 on failure
 */
int isee_from_msg_param(struct tee_param *params, size_t num_params,
			 const struct optee_msg_param *msg_params)
{
	int rc;
	size_t n;
	struct tee_shm *shm;
	phys_addr_t pa;

	for (n = 0; n < num_params; n++) {
		struct tee_param *p = params + n;
		const struct optee_msg_param *mp = msg_params + n;
		u32 attr = mp->attr & OPTEE_MSG_ATTR_TYPE_MASK;

		switch (attr) {
		case OPTEE_MSG_ATTR_TYPE_NONE:
			p->attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
			memset(&p->u, 0, sizeof(p->u));
			break;
		case OPTEE_MSG_ATTR_TYPE_VALUE_INPUT:
		case OPTEE_MSG_ATTR_TYPE_VALUE_OUTPUT:
		case OPTEE_MSG_ATTR_TYPE_VALUE_INOUT:
			p->attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT +
				  attr - OPTEE_MSG_ATTR_TYPE_VALUE_INPUT;
			p->u.value.a = mp->u.value.a;
			p->u.value.b = mp->u.value.b;
			p->u.value.c = mp->u.value.c;
			break;
		case OPTEE_MSG_ATTR_TYPE_TMEM_INPUT:
		case OPTEE_MSG_ATTR_TYPE_TMEM_OUTPUT:
		case OPTEE_MSG_ATTR_TYPE_TMEM_INOUT:
			p->attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT +
				  attr - OPTEE_MSG_ATTR_TYPE_TMEM_INPUT;
			p->u.memref.size = mp->u.tmem.size;
			shm = (struct tee_shm *)(unsigned long)
				mp->u.tmem.shm_ref;
			if (!shm) {
				p->u.memref.shm_offs = 0;
				p->u.memref.shm = NULL;
				break;
			}
			rc = isee_shm_get_pa(shm, 0, &pa);
			if (rc)
				return rc;
			p->u.memref.shm_offs = mp->u.tmem.buf_ptr - pa;
			p->u.memref.shm = shm;

			/* Check that the memref is covered by the shm object */
			if (p->u.memref.size) {
				size_t o = p->u.memref.shm_offs +
					   p->u.memref.size - 1;

				rc = isee_shm_get_pa(shm, o, NULL);
				if (rc)
					return rc;
			}
			break;
		default:
			return -EINVAL;
		}
	}
	return 0;
}

/**
 * isee_to_msg_param() - convert from struct tee_params to OPTEE_MSG parameters
 * @msg_params:	OPTEE_MSG parameters
 * @num_params:	number of elements in the parameter arrays
 * @params:	subsystem itnernal parameter representation
 * Returns 0 on success or <0 on failure
 */
int isee_to_msg_param(struct optee_msg_param *msg_params, size_t num_params,
		       const struct tee_param *params)
{
	int rc;
	size_t n;
	phys_addr_t pa;

	for (n = 0; n < num_params; n++) {
		const struct tee_param *p = params + n;
		struct optee_msg_param *mp = msg_params + n;

		switch (p->attr) {
		case TEE_IOCTL_PARAM_ATTR_TYPE_NONE:
			mp->attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
			memset(&mp->u, 0, sizeof(mp->u));
			break;
		case TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT:
		case TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_OUTPUT:
		case TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INOUT:
			mp->attr = OPTEE_MSG_ATTR_TYPE_VALUE_INPUT + p->attr -
				   TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
			mp->u.value.a = p->u.value.a;
			mp->u.value.b = p->u.value.b;
			mp->u.value.c = p->u.value.c;
			break;
		case TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT:
		case TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_OUTPUT:
		case TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INOUT:
			mp->attr = OPTEE_MSG_ATTR_TYPE_TMEM_INPUT +
				   p->attr -
				   TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT;
			mp->u.tmem.shm_ref = (unsigned long)p->u.memref.shm;
			mp->u.tmem.size = p->u.memref.size;
			if (!p->u.memref.shm) {
				mp->u.tmem.buf_ptr = 0;
				break;
			}
			rc = isee_shm_get_pa(p->u.memref.shm,
					    p->u.memref.shm_offs, &pa);
			if (rc)
				return rc;
			mp->u.tmem.buf_ptr = pa;
			mp->attr |= OPTEE_MSG_ATTR_CACHE_PREDEFINED <<
					OPTEE_MSG_ATTR_CACHE_SHIFT;
			break;
		default:
			return -EINVAL;
		}
	}
	return 0;
}

#ifdef CONFIG_MICROTRUST_TZDRIVER_DYNAMICAL_DEBUG
static int unhexify(char *obuf, __u8 *ibuf, int len)
{
	char *p = NULL;
	int i = 0;

	p = obuf;

	for (i = 0; i < len; i++)
		p += sprintf(p, "%02x", ibuf[i]);

	return 0;
}
#endif

int soter_open_session(struct tee_context *ctx,
		       struct tee_ioctl_open_session_arg *arg,
		       struct tee_param *param)
{
	struct soter_context_data *ctxdata = ctx->data;
	int rc;
	struct tee_shm *shm;
	struct optee_msg_arg *msg_arg;
	phys_addr_t msg_parg;
	struct soter_session *sess = NULL;

	while (teei_capi_ready != 1)
		msleep(50);

	/* +2 for the meta parameters added below */
	shm = get_msg_arg(ctx, arg->num_params + 2, &msg_arg, &msg_parg);
	if (IS_ERR(shm))
		return PTR_ERR(shm);

	msg_arg->cmd = OPTEE_MSG_CMD_OPEN_SESSION;
	msg_arg->cancel_id = arg->cancel_id;

	/*
	 * Initialize and add the meta parameters needed when opening a
	 * session.
	 */
	msg_arg->params[0].attr = OPTEE_MSG_ATTR_TYPE_VALUE_INPUT |
				  OPTEE_MSG_ATTR_META;
	msg_arg->params[1].attr = OPTEE_MSG_ATTR_TYPE_VALUE_INPUT |
				  OPTEE_MSG_ATTR_META;
	memcpy(&msg_arg->params[0].u.value, arg->uuid, sizeof(arg->uuid));
	memcpy(&msg_arg->params[1].u.value, arg->uuid, sizeof(arg->clnt_uuid));
	msg_arg->params[1].u.value.c = arg->clnt_login;

	rc = isee_to_msg_param(msg_arg->params + 2, arg->num_params, param);
	if (rc)
		goto out;

	sess = kzalloc(sizeof(*sess), GFP_KERNEL);
	if (!sess) {
		rc = -ENOMEM;
		goto out;
	}

	if (soter_do_call_with_arg(ctx, msg_parg)) {
		msg_arg->ret = TEEC_ERROR_COMMUNICATION;
		msg_arg->ret_origin = TEEC_ORIGIN_COMMS;
	}

	if (msg_arg->ret == TEEC_SUCCESS) {
		/* A new session has been created, add it to the list. */
		sess->session_id = msg_arg->session;
#ifdef CONFIG_MICROTRUST_TZDRIVER_DYNAMICAL_DEBUG
		memset(sess->uuid, 0, TEEI_UUID_MAX_LEN);
		unhexify(sess->uuid, arg->uuid, sizeof(arg->uuid));
#endif
		mutex_lock(&ctxdata->mutex);
		list_add(&sess->list_node, &ctxdata->sess_list);
		mutex_unlock(&ctxdata->mutex);
	} else {
		kfree(sess);
	}

	if (isee_from_msg_param(param, arg->num_params, msg_arg->params + 2)) {
		arg->ret = TEEC_ERROR_COMMUNICATION;
		arg->ret_origin = TEEC_ORIGIN_COMMS;
		/* Close session again to avoid leakage */
		soter_close_session(ctx, msg_arg->session);
	} else {
		arg->session = msg_arg->session;
		arg->ret = msg_arg->ret;
		arg->ret_origin = msg_arg->ret_origin;
	}
out:
	isee_shm_free(shm);

	return rc;
}

int soter_close_session(struct tee_context *ctx, u32 session)
{
	struct soter_context_data *ctxdata = ctx->data;
	struct tee_shm *shm;
	struct optee_msg_arg *msg_arg;
	phys_addr_t msg_parg;
	struct soter_session *sess;

	/* Check that the session is valid and remove it from the list */
	mutex_lock(&ctxdata->mutex);
	sess = find_session(ctxdata, session);
	if (sess)
		list_del(&sess->list_node);
	mutex_unlock(&ctxdata->mutex);
	if (!sess)
		return -EINVAL;

	kfree(sess);

	shm = get_msg_arg(ctx, 0, &msg_arg, &msg_parg);
	if (IS_ERR(shm))
		return PTR_ERR(shm);

	msg_arg->cmd = OPTEE_MSG_CMD_CLOSE_SESSION;
	msg_arg->session = session;
	soter_do_call_with_arg(ctx, msg_parg);

	isee_shm_free(shm);

	return 0;
}

int soter_invoke_func(struct tee_context *ctx, struct tee_ioctl_invoke_arg *arg,
		      struct tee_param *param)
{
	struct soter_context_data *ctxdata = ctx->data;
	struct tee_shm *shm;
	struct optee_msg_arg *msg_arg;
	phys_addr_t msg_parg;
	struct soter_session *sess;
	int rc;
#ifdef CONFIG_MICROTRUST_TZDRIVER_DYNAMICAL_DEBUG
	long time_used;
#endif

	/* Check that the session is valid */
	mutex_lock(&ctxdata->mutex);
	sess = find_session(ctxdata, arg->session);
	mutex_unlock(&ctxdata->mutex);
	if (!sess)
		return -EINVAL;

	shm = get_msg_arg(ctx, arg->num_params, &msg_arg, &msg_parg);
	if (IS_ERR(shm))
		return PTR_ERR(shm);
	msg_arg->cmd = OPTEE_MSG_CMD_INVOKE_COMMAND;
	msg_arg->func = arg->func;
	msg_arg->session = arg->session;
	msg_arg->cancel_id = arg->cancel_id;

	rc = isee_to_msg_param(msg_arg->params, arg->num_params, param);
	if (rc)
		goto out;

#ifdef CONFIG_MICROTRUST_TZDRIVER_DYNAMICAL_DEBUG
	if (tzdriver_dynamical_debug_flag == 1) {
		IMSG_PRINTK("TEEI: %s START (TA: %s).\n",
							__func__, sess->uuid);
		do_gettimeofday(&start_val);
		if (likely(ree_dynamical_debug == REE_DYNAMICAL_START)) {
			time_used = (start_val.tv_sec - end_val.tv_sec)
				* 1000000 +
				(start_val.tv_usec - end_val.tv_usec);
			IMSG_PRINTK(
				"TEEI: TA(%s) CMD_ID: 0x%x REE USED %ld us.\n",
				sess->uuid, arg->func, time_used);
		}
	}
#endif

	if (soter_do_call_with_arg(ctx, msg_parg)) {
		msg_arg->ret = TEEC_ERROR_COMMUNICATION;
		msg_arg->ret_origin = TEEC_ORIGIN_COMMS;
	}

#ifdef CONFIG_MICROTRUST_TZDRIVER_DYNAMICAL_DEBUG
	if (tzdriver_dynamical_debug_flag == 1) {
		do_gettimeofday(&end_val);
		time_used = (end_val.tv_sec - start_val.tv_sec) * 1000000
				+ (end_val.tv_usec - start_val.tv_usec);

		IMSG_PRINTK("TEEI: TA(%s) CMD_ID: 0x%x TEE USED %ld us.\n",
				sess->uuid, arg->func, time_used);
		if (unlikely(ree_dynamical_debug != REE_DYNAMICAL_START))
			ree_dynamical_debug = REE_DYNAMICAL_START;
	}
#endif

	if (isee_from_msg_param(param, arg->num_params, msg_arg->params)) {
		msg_arg->ret = TEEC_ERROR_COMMUNICATION;
		msg_arg->ret_origin = TEEC_ORIGIN_COMMS;
	}

	arg->ret = msg_arg->ret;
	arg->ret_origin = msg_arg->ret_origin;
out:
	isee_shm_free(shm);
	return rc;
}

struct soter_cancel_struct {
	u32 session_id;
	u32 cancel_id;
};

int soter_cancel_func(struct tee_context *ctx, u32 cancel_id, u32 session)
{
	struct soter_context_data *ctxdata = ctx->data;
	struct soter_cancel_struct cancel_param;
	struct soter_session *sess = NULL;

	if ((void *)cancel_message_buff == NULL) {
		IMSG_ERROR("[%s][%d]cancel_message_buff is NULL!\n",
				__func__, __LINE__);
		return -EINVAL;
	}

	mutex_lock(&ctxdata->mutex);
	sess = find_session(ctxdata, session);
	mutex_unlock(&ctxdata->mutex);
	if (sess == NULL) {
		IMSG_ERROR("[%s][%d]session is NULL!\n",
				__func__, __LINE__);
		return -EINVAL;
	}

	cancel_param.session_id = session;
	cancel_param.cancel_id = cancel_id;
	memcpy((void *)cancel_message_buff,
			   (void *)&cancel_param,
			   sizeof(struct soter_cancel_struct));

	return send_cancel_command(0);
}
