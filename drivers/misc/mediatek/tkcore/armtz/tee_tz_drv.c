/*
 * Copyright (c) 2015-2018 TrustKernel Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/sched.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/init.h>

#include <linux/tee_core.h>
#include <linux/tee_ioc.h>

#include <tee_shm.h>
#include <tee_supp_com.h>
#include <tee_wait_queue.h>

#include <arm_common/teesmc.h>
#include <arm_common/teesmc_st.h>

#include "asm/io.h"
#include "tee_mem.h"
#include "tee_tz_op.h"
#include "tee_tz_priv.h"
#include "handle.h"
#include "tee_smc_xfer.h"

#include "tee_procfs.h"
#include "pm.h"

#define _TEE_TZ_NAME "tkcoredrv"
#define DEV (ptee->tee->dev)

/* #define TEE_STRESS_OUTERCACHE_FLUSH */

/* magic config: bit 1 is set, Secure TEE shall handler NSec IRQs */
#define SEC_ROM_NO_FLAG_MASK	0x0000
#define SEC_ROM_IRQ_ENABLE_MASK	0x0001
#define SEC_ROM_DEFAULT		SEC_ROM_IRQ_ENABLE_MASK
#define TEE_RETURN_BUSY		0x3
#define ALLOC_ALIGN		SZ_4K

#define CAPABLE(tee) !(tee->conf & TEE_CONF_FW_NOT_CAPABLE)

static struct tee_tz *tee_tz;

static struct handle_db shm_handle_db = HANDLE_DB_INITIALIZER;

/*******************************************************************
 * Calling TEE
 *******************************************************************/

static void handle_rpc_func_cmd_wait_queue(struct tee_tz *ptee,
		struct teesmc32_arg *arg32)
{
	struct teesmc32_param *params;

	if (arg32->num_params != 2)
		goto bad;

	params = TEESMC32_GET_PARAMS(arg32);

	if ((params[0].attr & TEESMC_ATTR_TYPE_MASK) !=
		TEESMC_ATTR_TYPE_VALUE_INPUT)
		goto bad;
	if ((params[1].attr & TEESMC_ATTR_TYPE_MASK) !=
		TEESMC_ATTR_TYPE_NONE)
		goto bad;

	switch (arg32->cmd) {
	case TEE_RPC_WAIT_QUEUE_SLEEP:
		tee_wait_queue_sleep(DEV, &ptee->wait_queue,
					params[0].u.value.a);
		break;
	case TEE_RPC_WAIT_QUEUE_WAKEUP:
		tee_wait_queue_wakeup(DEV, &ptee->wait_queue,
					params[0].u.value.a);
		break;
	default:
		goto bad;
	}

	arg32->ret = TEEC_SUCCESS;
	return;
bad:
	arg32->ret = TEEC_ERROR_BAD_PARAMETERS;
}



static void handle_rpc_func_cmd_wait(struct teesmc32_arg *arg32)
{
	struct teesmc32_param *params;
	u32 msec_to_wait;

	if (arg32->num_params != 1)
		goto bad;

	params = TEESMC32_GET_PARAMS(arg32);
	msec_to_wait = params[0].u.value.a;

	/* set task's state to interruptible sleep */
	set_current_state(TASK_INTERRUPTIBLE);

	/* take a nap */
	schedule_timeout(msecs_to_jiffies(msec_to_wait));

	arg32->ret = TEEC_SUCCESS;
	return;
bad:
	arg32->ret = TEEC_ERROR_BAD_PARAMETERS;
}

static void handle_rpc_func_cmd_to_supplicant(struct tee_tz *ptee,
		struct teesmc32_arg *arg32)
{
	struct teesmc32_param *params;
	struct tee_rpc_invoke inv;
	size_t n;
	uint32_t ret;

	if (arg32->num_params > TEE_RPC_BUFFER_NUMBER) {
		arg32->ret = TEEC_ERROR_GENERIC;
		return;
	}

	pr_debug("arg32=%p cmd=0x%x num_param=0x%x\n",
		arg32, arg32->cmd, arg32->num_params);

	params = TEESMC32_GET_PARAMS(arg32);

	memset(&inv, 0, sizeof(inv));
	inv.cmd = arg32->cmd;
	/*
	 * Set a suitable error code in case teed
	 * ignores the request.
	 */
	inv.res = TEEC_ERROR_NOT_IMPLEMENTED;
	inv.nbr_bf = arg32->num_params;
	for (n = 0; n < arg32->num_params; n++) {
		switch (params[n].attr & TEESMC_ATTR_TYPE_MASK) {
		case TEESMC_ATTR_TYPE_VALUE_INPUT:
		case TEESMC_ATTR_TYPE_VALUE_INOUT:
			inv.cmds[n].fd = (int)params[n].u.value.a;
		/* Fall through */
		case TEESMC_ATTR_TYPE_VALUE_OUTPUT:
			inv.cmds[n].type = TEE_RPC_VALUE;
			pr_debug("param of type value\n");
			break;
		case TEESMC_ATTR_TYPE_MEMREF_INPUT:
		case TEESMC_ATTR_TYPE_MEMREF_OUTPUT:
		case TEESMC_ATTR_TYPE_MEMREF_INOUT:
			inv.cmds[n].buffer =
				(void *)(uintptr_t)params[n].u.memref.buf_ptr;
			inv.cmds[n].size = params[n].u.memref.size;
			inv.cmds[n].type = TEE_RPC_BUFFER;
			pr_debug("param memref buffer %p size 0x%x\n",
				inv.cmds[n].buffer, inv.cmds[n].size);
			break;
		default:
			arg32->ret = TEEC_ERROR_GENERIC;
			return;
		}
	}

	ret = tee_supp_cmd(ptee->tee, TEE_RPC_ICMD_INVOKE,
			   &inv, sizeof(inv));
	if (ret == TEEC_RPC_OK)
		arg32->ret = inv.res;

	for (n = 0; n < arg32->num_params; n++) {
		switch (params[n].attr & TEESMC_ATTR_TYPE_MASK) {
		case TEESMC_ATTR_TYPE_MEMREF_OUTPUT:
		case TEESMC_ATTR_TYPE_MEMREF_INOUT:
			/*
			 * Allow teed to assign a new pointer
			 * to an out-buffer. Needed when the
			 * teed allocates a new buffer, for
			 * instance when loading a TA.
			 */
			params[n].u.memref.buf_ptr =
				(uint32_t)(uintptr_t)inv.cmds[n].buffer;
			params[n].u.memref.size = inv.cmds[n].size;
			break;
		case TEESMC_ATTR_TYPE_VALUE_OUTPUT:
		case TEESMC_ATTR_TYPE_VALUE_INOUT:
			params[n].u.value.a = inv.cmds[n].fd;
			break;
		default:
			break;
		}
	}
}

#ifdef RPMB_SUPPORT

#include "linux/tee_rpmb.h"

/*
 * Need to be in consistency with
 * struct rpmb_req {...} defined in
 * tee/core/arch/arm/tee/tee_rpmb.c
 */
struct tee_rpmb_cmd {
	uint16_t cmd;
	uint16_t dev_id;
	uint32_t req_nr;
	uint32_t resp_nr;
};

int rpmb_exec(void *req)
{
	return tkcore_emmc_rpmb_execute((struct tkcore_rpmb_request *) req);
}

#endif

static void handle_rpmb_cmd(struct tee_tz *ptee,
				struct teesmc32_arg *arg32)
{
#ifdef RPMB_SUPPORT
	uint32_t req_size, resp_size;
	struct teesmc32_param *params;

	uint8_t *data_frame;
	struct tee_rpmb_cmd *rpmb_req;
	struct tkcore_rpmb_request teec_rpmb_req;
	void *resp;

	if (arg32->num_params != TEE_RPMB_BUFFER_NUMBER) {
		arg32->ret = TEEC_ERROR_BAD_PARAMETERS;
		return;
	}

	params = TEESMC32_GET_PARAMS(arg32);

	if (((params[0].attr & TEESMC_ATTR_TYPE_MASK)
			!= TEESMC_ATTR_TYPE_MEMREF_INPUT) ||
		((params[1].attr & TEESMC_ATTR_TYPE_MASK)
			!= TEESMC_ATTR_TYPE_MEMREF_OUTPUT)) {
		arg32->ret = TEEC_ERROR_GENERIC;
		return;
	}


	rpmb_req = (struct tee_rpmb_cmd *) tee_shm_pool_p2v(
			   ptee->tee->dev,
			   ptee->shm_pool,
			   params[0].u.memref.buf_ptr);

	if (rpmb_req == NULL)  {
		pr_err("Bad RPC request buffer 0x%x.\n",
			params[0].u.memref.buf_ptr);
		arg32->ret = TEEC_ERROR_GENERIC;
		return;
	}

	resp = tee_shm_pool_p2v(
			ptee->tee->dev,
			ptee->shm_pool,
			params[1].u.memref.buf_ptr);

	if (resp == NULL) {
		pr_err("Bad RPC response buffer 0x%x.\n",
			params[1].u.memref.buf_ptr);
		arg32->ret = TEEC_ERROR_GENERIC;
		return;
	}

	if (rpmb_req->cmd != TEE_RPMB_GET_DEV_INFO) {
		int r;
		uint32_t frm_size;

		req_size = rpmb_req->req_nr * 512;
		resp_size = rpmb_req->resp_nr * 512;
		frm_size = req_size > resp_size ? req_size : resp_size;

		if (frm_size & (511)) {
			pr_err("bad RPMB frame size 0x%x\n", frm_size);
			arg32->ret = TEEC_ERROR_BAD_PARAMETERS;
			return;
		}

		teec_rpmb_req.type = rpmb_req->cmd;
		teec_rpmb_req.blk_cnt = frm_size / 512;
		/* not used by emmc_rpmb driver */
		teec_rpmb_req.addr = (uint16_t) 0;

		teec_rpmb_req.data_frame = data_frame = kmalloc(
				frm_size,
				GFP_KERNEL);

		if (teec_rpmb_req.data_frame == NULL) {
			arg32->ret = TEEC_ERROR_OUT_OF_MEMORY;
			return;
		}

		memcpy(data_frame,
			((uint8_t *) rpmb_req) + sizeof(struct tee_rpmb_cmd),
			req_size);
		r = tkcore_stay_awake(rpmb_exec, (void *) &teec_rpmb_req);

		if (r != 0) {
			pr_err("Bad rpmb_exec: %d\n", r);
			arg32->ret = (uint32_t) r;
		} else {
			arg32->ret = TEEC_SUCCESS;
			memcpy(resp, data_frame, resp_size);
		}

		kfree(data_frame);
	} else {
		struct tee_rpmb_dev_info *dev_info;

		teec_rpmb_req.type = rpmb_req->cmd;
		teec_rpmb_req.blk_cnt = 1;
		/* not used by emmc_rpmb driver */
		teec_rpmb_req.addr = (uint16_t) 0;
		teec_rpmb_req.data_frame = (uint8_t *) resp;

		dev_info = (struct tee_rpmb_dev_info *) resp;

		dev_info->ret_code =
			(uint8_t) tkcore_emmc_rpmb_execute(&teec_rpmb_req);

		arg32->ret = TEEC_SUCCESS;
	}

#else
	arg32->ret = TEEC_ERROR_NOT_IMPLEMENTED;
#endif
}


static void handle_rpc_func_cmd(struct tee_tz *ptee, u32 parg32)
{
	struct teesmc32_arg *arg32;

	arg32 = tee_shm_pool_p2v(DEV, ptee->shm_pool, parg32);
	pr_debug("rpc_func parg32=0x%x\n", parg32);
	if (!arg32)
		return;

	switch (arg32->cmd) {
	case TEE_RPC_WAIT_QUEUE_SLEEP:
	case TEE_RPC_WAIT_QUEUE_WAKEUP:
		handle_rpc_func_cmd_wait_queue(ptee, arg32);
		break;
	case TEE_RPC_WAIT:
		handle_rpc_func_cmd_wait(arg32);
		break;
	case TEE_RPC_RPMB_CMD:
		handle_rpmb_cmd(ptee, arg32);
		break;
	default:
		handle_rpc_func_cmd_to_supplicant(ptee, arg32);
	}
}

static struct tee_shm *handle_rpc_alloc(struct tee_tz *ptee, size_t size)
{
	struct tee_rpc_alloc rpc_alloc;

	memset((void *) &rpc_alloc, 0, sizeof(struct tee_rpc_alloc));

	rpc_alloc.size = size;
	tee_supp_cmd(ptee->tee, TEE_RPC_ICMD_ALLOCATE,
		&rpc_alloc, sizeof(rpc_alloc));
	return rpc_alloc.shm;
}

static void handle_rpc_free(struct tee_tz *ptee, struct tee_shm *shm)
{
	struct tee_rpc_free rpc_free;

	if (!shm)
		return;
	rpc_free.shm = shm;
	tee_supp_cmd(ptee->tee, TEE_RPC_ICMD_FREE, &rpc_free, sizeof(rpc_free));
}

static u32 handle_rpc(struct tee_tz *ptee, struct smc_param *param)
{
	struct tee_shm *shm;
	int cookie;

	switch (TEESMC_RETURN_GET_RPC_FUNC(param->a0)) {
	case TEESMC_RPC_FUNC_ALLOC_ARG:
		param->a1 =
			tee_shm_pool_alloc(DEV, ptee->shm_pool, param->a1, 4);

		break;
	case TEESMC_RPC_FUNC_ALLOC_PAYLOAD:
		/* Can't support payload shared memory with this interface */
		param->a2 = 0;
		break;
	case TEESMC_RPC_FUNC_FREE_ARG:
		tee_shm_pool_free(DEV, ptee->shm_pool, param->a1, 0);
		break;
	case TEESMC_RPC_FUNC_FREE_PAYLOAD:
		/* Can't support payload shared memory with this interface */
		break;
	case TEESMC_ST_RPC_FUNC_ALLOC_PAYLOAD:
		shm = handle_rpc_alloc(ptee, param->a1);
		if (IS_ERR_OR_NULL(shm)) {
			param->a1 = 0;
			break;
		}
		cookie = handle_get(&shm_handle_db, shm);
		if (cookie < 0) {
			handle_rpc_free(ptee, shm);
			param->a1 = 0;
			break;
		}
		param->a1 = shm->resv.paddr;
		param->a2 = cookie;
		break;
	case TEESMC_ST_RPC_FUNC_FREE_PAYLOAD:
		shm = handle_put(&shm_handle_db, param->a1);
		handle_rpc_free(ptee, shm);
		break;
	case TEESMC_RPC_FUNC_CMD:
		handle_rpc_func_cmd(ptee, param->a1);
		break;
	default:
		pr_warn("Unknown RPC func 0x%x\n",
			 (u32) TEESMC_RETURN_GET_RPC_FUNC(param->a0));
		break;
	}

	/* TODO refine this piece of logic. the irq status
	 * can no longer determine whether it's a fastcall
	 * or not
	 */
	if (irqs_disabled())
		return TEESMC32_FASTCALL_RETURN_FROM_RPC;
	else
		return TEESMC32_CALL_RETURN_FROM_RPC;
}

static void call_tee(struct tee_tz *ptee, uintptr_t parg32,
			struct teesmc32_arg *arg32, u32 funcid)
{
	u32 ret;

	struct smc_param param = {
		.a1 = parg32,
	};

	for (;;) {
		param.a0 = funcid;
		__call_tee(&param);

		if (!TEESMC_RETURN_IS_RPC(param.a0))
			break;

		funcid = handle_rpc(ptee, &param);
	}

	ret = param.a0;

	if (unlikely(ret != TEESMC_RETURN_OK &&
			ret != TEESMC_RETURN_UNKNOWN_FUNCTION)) {
		arg32->ret = TEEC_ERROR_COMMUNICATION;
		arg32->ret_origin = TEEC_ORIGIN_COMMS;
	}
}

static inline void stdcall_tee(struct tee_tz *ptee, uintptr_t parg32,
				struct teesmc32_arg *arg32)
{
	call_tee(ptee, parg32, arg32, TEESMC32_CALL_WITH_ARG);
}

static inline void fastcall_tee(struct tee_tz *ptee, uintptr_t parg32,
				struct teesmc32_arg *arg32)
{
	call_tee(ptee, parg32, arg32, TEESMC32_FASTCALL_WITH_ARG);
}

/*******************************************************************
 * TEE service invoke formating
 *******************************************************************/

/* allocate tee service argument buffer and return virtual address */
static void *alloc_tee_arg(struct tee_tz *ptee, unsigned long *p, size_t l)
{
	void *vaddr;

	WARN_ON(!CAPABLE(ptee->tee));

	if ((p == NULL) || (l == 0))
		return NULL;

	/* assume a 4 bytes aligned is sufficient */
	*p = tee_shm_pool_alloc(DEV, ptee->shm_pool, l, ALLOC_ALIGN);
	if (*p == 0)
		return NULL;

	vaddr = tee_shm_pool_p2v(DEV, ptee->shm_pool, *p);


	return vaddr;
}

/* free tee service argument buffer (from its physical address) */
static void free_tee_arg(struct tee_tz *ptee, unsigned long p)
{
	WARN_ON(!CAPABLE(ptee->tee));

	if (p)
		tee_shm_pool_free(DEV, ptee->shm_pool, p, 0);

}

static uint32_t get_cache_attrs(struct tee_tz *ptee)
{
	if (tee_shm_pool_is_cached(ptee->shm_pool))
		return TEESMC_ATTR_CACHE_DEFAULT << TEESMC_ATTR_CACHE_SHIFT;
	else
		return TEESMC_ATTR_CACHE_NONCACHE << TEESMC_ATTR_CACHE_SHIFT;
}

static uint32_t param_type_teec2teesmc(uint8_t type)
{
	switch (type) {
	case TEEC_NONE:
		return TEESMC_ATTR_TYPE_NONE;
	case TEEC_VALUE_INPUT:
		return TEESMC_ATTR_TYPE_VALUE_INPUT;
	case TEEC_VALUE_OUTPUT:
		return TEESMC_ATTR_TYPE_VALUE_OUTPUT;
	case TEEC_VALUE_INOUT:
		return TEESMC_ATTR_TYPE_VALUE_INOUT;
	case TEEC_MEMREF_TEMP_INPUT:
	case TEEC_MEMREF_PARTIAL_INPUT:
		return TEESMC_ATTR_TYPE_MEMREF_INPUT;
	case TEEC_MEMREF_TEMP_OUTPUT:
	case TEEC_MEMREF_PARTIAL_OUTPUT:
		return TEESMC_ATTR_TYPE_MEMREF_OUTPUT;
	case TEEC_MEMREF_WHOLE:
	case TEEC_MEMREF_TEMP_INOUT:
	case TEEC_MEMREF_PARTIAL_INOUT:
		return TEESMC_ATTR_TYPE_MEMREF_INOUT;
	default:
		WARN_ON(true);
		return 0;
	}
}

static void set_params(struct tee_tz *ptee,
	struct teesmc32_param params32[TEEC_CONFIG_PAYLOAD_REF_COUNT],
	uint32_t param_types,
	struct tee_data *data)
{
	size_t n;
	struct tee_shm *shm;
	struct TEEC_Value *value;

	for (n = 0; n < TEEC_CONFIG_PAYLOAD_REF_COUNT; n++) {
		uint32_t type = TEEC_PARAM_TYPE_GET(param_types, n);

		params32[n].attr = param_type_teec2teesmc(type);
		if (params32[n].attr == TEESMC_ATTR_TYPE_NONE)
			continue;
		if (params32[n].attr < TEESMC_ATTR_TYPE_MEMREF_INPUT) {
			value = (struct TEEC_Value *)&data->params[n];
			params32[n].u.value.a = value->a;
			params32[n].u.value.b = value->b;
			continue;
		}
		shm = data->params[n].shm;
		params32[n].attr |= get_cache_attrs(ptee);
		params32[n].u.memref.buf_ptr = shm->resv.paddr;
		params32[n].u.memref.size = shm->size_req;
	}
}

static void get_params(struct tee_data *data,
	struct teesmc32_param params32[TEEC_CONFIG_PAYLOAD_REF_COUNT])
{
	size_t n;
	struct tee_shm *shm;
	struct TEEC_Value *value;

	for (n = 0; n < TEEC_CONFIG_PAYLOAD_REF_COUNT; n++) {
		if (params32[n].attr == TEESMC_ATTR_TYPE_NONE)
			continue;
		if (params32[n].attr < TEESMC_ATTR_TYPE_MEMREF_INPUT) {
			value = &data->params[n].value;
			value->a = params32[n].u.value.a;
			value->b = params32[n].u.value.b;
			continue;
		}
		shm = data->params[n].shm;
		shm->size_req = params32[n].u.memref.size;
	}
}

/*
 * tee_open_session - invoke TEE to open a GP TEE session
 */
static int tz_open(struct tee_session *sess, struct tee_cmd *cmd)
{
	struct tee *tee;
	struct tee_tz *ptee;
	int ret = 0;

	struct teesmc32_arg *arg32;
	struct teesmc32_param *params32;
	struct teesmc_meta_open_session *meta;
	uintptr_t parg32;
	uintptr_t pmeta;
	size_t num_meta = 1;
	uint8_t *ta;
	struct TEEC_UUID *uuid;

	WARN_ON(!sess->ctx->tee);
	WARN_ON(!sess->ctx->tee->priv);
	tee = sess->ctx->tee;
	ptee = tee->priv;

	if (cmd->uuid)
		uuid = cmd->uuid->resv.kaddr;
	else
		uuid = NULL;

	pr_debug("> ta kaddr %p, uuid=%08x-%04x-%04x\n",
		(cmd->ta) ? cmd->ta->resv.kaddr : NULL,
		((uuid) ? uuid->timeLow : 0xDEAD),
		((uuid) ? uuid->timeMid : 0xDEAD),
		((uuid) ? uuid->timeHiAndVersion : 0xDEAD));

	if (!CAPABLE(ptee->tee)) {
		pr_err("tz_open: not capable\n");
		return -EBUSY;
	}

	/* case ta binary is inside the open request */
	ta = NULL;
	if (cmd->ta)
		ta = cmd->ta->resv.kaddr;
	if (ta)
		num_meta++;

	arg32 = alloc_tee_arg(ptee, &parg32,
		TEESMC32_GET_ARG_SIZE(
		TEEC_CONFIG_PAYLOAD_REF_COUNT + num_meta));
	meta = alloc_tee_arg(ptee, &pmeta, sizeof(*meta));

	if ((arg32 == NULL) || (meta == NULL)) {
		free_tee_arg(ptee, parg32);
		free_tee_arg(ptee, pmeta);
		return -ENOMEM;
	}

	memset(arg32, 0, sizeof(*arg32));
	memset(meta, 0, sizeof(*meta));
	arg32->num_params = TEEC_CONFIG_PAYLOAD_REF_COUNT + num_meta;
	params32 = TEESMC32_GET_PARAMS(arg32);

	arg32->cmd = TEESMC_CMD_OPEN_SESSION;

	params32[0].u.memref.buf_ptr = pmeta;
	params32[0].u.memref.size = sizeof(*meta);
	params32[0].attr = TEESMC_ATTR_TYPE_MEMREF_INPUT |
			   TEESMC_ATTR_META | get_cache_attrs(ptee);

	if (ta) {
		params32[1].u.memref.buf_ptr =
			tee_shm_pool_v2p(DEV, ptee->shm_pool,
				cmd->ta->resv.kaddr);
		params32[1].u.memref.size = cmd->ta->size_req;
		params32[1].attr = TEESMC_ATTR_TYPE_MEMREF_INPUT |
				   TEESMC_ATTR_META | get_cache_attrs(ptee);
	}

	if (uuid != NULL)
		memcpy(meta->uuid, uuid, TEESMC_UUID_LEN);
	meta->clnt_login = 0; /* FIXME: is this reliable ? used ? */

	params32 += num_meta;
	set_params(ptee, params32, cmd->param.type, &cmd->param);

	stdcall_tee(ptee, parg32, arg32);

	get_params(&cmd->param, params32);

	if (arg32->ret != TEEC_ERROR_COMMUNICATION) {
		sess->sessid = arg32->session;
		cmd->err = arg32->ret;
		cmd->origin = arg32->ret_origin;
	} else
		ret = -EBUSY;

	free_tee_arg(ptee, parg32);
	free_tee_arg(ptee, pmeta);

	return ret;
}

/*
 * tee_invoke_command - invoke TEE to invoke a GP TEE command
 */
static int tz_invoke(struct tee_session *sess, struct tee_cmd *cmd)
{
	struct tee *tee;
	struct tee_tz *ptee;
	int ret = 0;

	struct teesmc32_arg *arg32;
	uintptr_t parg32;
	struct teesmc32_param *params32;

	WARN_ON(!sess->ctx->tee);
	WARN_ON(!sess->ctx->tee->priv);
	tee = sess->ctx->tee;
	ptee = tee->priv;

	pr_debug("> sessid %x cmd %x type %x\n",
		sess->sessid, cmd->cmd, cmd->param.type);

	if (!CAPABLE(tee)) {
		pr_err("tz_invoke: not capable\n");
		return -EBUSY;
	}

	arg32 = (typeof(arg32))alloc_tee_arg(ptee, &parg32,
		TEESMC32_GET_ARG_SIZE(TEEC_CONFIG_PAYLOAD_REF_COUNT));
	if (!arg32) {
		free_tee_arg(ptee, parg32);
		return TEEC_ERROR_OUT_OF_MEMORY;
	}

	memset(arg32, 0, sizeof(*arg32));
	arg32->num_params = TEEC_CONFIG_PAYLOAD_REF_COUNT;
	params32 = TEESMC32_GET_PARAMS(arg32);

	arg32->cmd = TEESMC_CMD_INVOKE_COMMAND;
	arg32->session = sess->sessid;
	arg32->ta_func = cmd->cmd;

	set_params(ptee, params32, cmd->param.type, &cmd->param);

	stdcall_tee(ptee, parg32, arg32);

	get_params(&cmd->param, params32);

	if (arg32->ret != TEEC_ERROR_COMMUNICATION) {
		cmd->err = arg32->ret;
		cmd->origin = arg32->ret_origin;
	} else
		ret = -EBUSY;

	free_tee_arg(ptee, parg32);

	return ret;
}

/*
 * tee_cancel_command - invoke TEE to cancel a GP TEE command
 */
static int tz_cancel(struct tee_session *sess, struct tee_cmd *cmd)
{
	struct tee *tee;
	struct tee_tz *ptee;
	int ret = 0;

	struct teesmc32_arg *arg32;
	uintptr_t parg32;

	WARN_ON(!sess->ctx->tee);
	WARN_ON(!sess->ctx->tee->priv);
	tee = sess->ctx->tee;
	ptee = tee->priv;


	arg32 = alloc_tee_arg(ptee, &parg32, TEESMC32_GET_ARG_SIZE(0));
	if (arg32 == NULL) {
		free_tee_arg(ptee, parg32);
		return TEEC_ERROR_OUT_OF_MEMORY;
	}

	memset(arg32, 0, sizeof(*arg32));
	arg32->cmd = TEESMC_CMD_CANCEL;
	arg32->session = sess->sessid;

	fastcall_tee(ptee, parg32, arg32);

	if (arg32->ret == TEEC_ERROR_COMMUNICATION)
		ret = -EBUSY;

	free_tee_arg(ptee, parg32);

	return ret;
}

/*
 * tee_close_session - invoke TEE to close a GP TEE session
 */
static int tz_close(struct tee_session *sess)
{
	struct tee *tee;
	struct tee_tz *ptee;
	int ret = 0;

	struct teesmc32_arg *arg32;
	uintptr_t parg32;

	WARN_ON(!sess->ctx->tee);
	WARN_ON(!sess->ctx->tee->priv);
	tee = sess->ctx->tee;
	ptee = tee->priv;


	if (!CAPABLE(tee)) {
		pr_err("tz_close: not capable\n");
		return -EBUSY;
	}

	arg32 = alloc_tee_arg(ptee, &parg32, TEESMC32_GET_ARG_SIZE(0));
	if (arg32 == NULL) {
		pr_err("Failed to allocate tee arg\n");
		free_tee_arg(ptee, parg32);
		return TEEC_ERROR_OUT_OF_MEMORY;
	}

	memset(arg32, 0, sizeof(*arg32));
	arg32->cmd = TEESMC_CMD_CLOSE_SESSION;
	arg32->session = sess->sessid;

	stdcall_tee(ptee, parg32, arg32);

	if (arg32->ret == TEEC_ERROR_COMMUNICATION)
		ret = -EBUSY;

	free_tee_arg(ptee, parg32);

	return ret;
}

static struct tee_shm *tz_alloc(struct tee *tee, size_t size, uint32_t flags)
{
	struct tee_shm *shm = NULL;
	struct tee_tz *ptee;
	size_t size_aligned;

	WARN_ON(!tee->priv);
	ptee = tee->priv;


	size_aligned = ((size / SZ_4K) + 1) * SZ_4K;
	if (unlikely(size_aligned == 0)) {
		pr_err("requested size too big\n");
		return NULL;
	}

	shm = devm_kzalloc(tee->dev, sizeof(struct tee_shm), GFP_KERNEL);
	if (!shm)
		return ERR_PTR(-ENOMEM);

	shm->size_alloc = ((size / SZ_4K) + 1) * SZ_4K;
	shm->size_req = size;

	shm->resv.paddr = tee_shm_pool_alloc(tee->dev, ptee->shm_pool,
						shm->size_alloc, ALLOC_ALIGN);
	if (!shm->resv.paddr) {
		pr_err("tz_alloc: cannot alloc memory, size 0x%lx\n",
			(unsigned long)shm->size_alloc);
		devm_kfree(tee->dev, shm);
		return ERR_PTR(-ENOMEM);
	}
	shm->resv.kaddr =
		tee_shm_pool_p2v(tee->dev, ptee->shm_pool, shm->resv.paddr);
	if (!shm->resv.kaddr) {
		pr_err("tz_alloc: p2v(%p)=0\n",
			(void *) (unsigned long) shm->resv.paddr);
		tee_shm_pool_free(tee->dev,
			ptee->shm_pool, shm->resv.paddr, NULL);
		devm_kfree(tee->dev, shm);
		return ERR_PTR(-EFAULT);
	}
	shm->flags = flags;
	if (ptee->shm_cached)
		shm->flags |= TEE_SHM_CACHED;

	pr_debug(
		"kaddr=%p, paddr=%p, shm=%p, size %x:%x\n",
		shm->resv.kaddr,
		(void *)(unsigned long) shm->resv.paddr, shm,
		(unsigned int) shm->size_req,
		(unsigned int) shm->size_alloc);

	return shm;
}

static void tz_free(struct tee_shm *shm)
{
	size_t size;
	int ret;
	struct tee *tee;
	struct tee_tz *ptee;

	WARN_ON(!shm->tee);
	WARN_ON(!shm->tee->priv);
	tee = shm->tee;
	ptee = tee->priv;


	ret = tee_shm_pool_free(tee->dev, ptee->shm_pool,
		shm->resv.paddr, &size);
	if (!ret) {
		pr_debug("tee_shm_pool_free ret: 0x%x\n",
			ret);
		devm_kfree(tee->dev, shm);
		shm = NULL;
	}
}

static int tz_shm_inc_ref(struct tee_shm *shm)
{
	struct tee *tee;
	struct tee_tz *ptee;

	WARN_ON(!shm->tee);
	WARN_ON(!shm->tee->priv);
	tee = shm->tee;
	ptee = tee->priv;

	return tee_shm_pool_incref(tee->dev, ptee->shm_pool, shm->resv.paddr);
}

#ifdef CONFIG_OUTER_CACHE
/*
 * Synchronised outer cache maintenance support
 */
#ifndef CONFIG_ARM_TZ_SUPPORT
/* weak outer_tz_mutex in case not supported by kernel */
bool __weak outer_tz_mutex(unsigned long *p)
{
	if (p != NULL)
		return false;
	return true;
}
#endif

/* register_outercache_mutex - Negotiate/Disable outer cache shared mutex */
static int register_outercache_mutex(struct tee_tz *ptee, bool reg)
{
	unsigned long *vaddr = NULL;
	int ret = 0;
	struct smc_param param;
	uintptr_t paddr = 0;

	WARN_ON(!CAPABLE(ptee->tee));

	if ((reg == true) && (ptee->tz_outer_cache_mutex != NULL)) {
		pr_err("outer cache shared mutex already registered\n");
		return -EINVAL;
	}
	if ((reg == false) && (ptee->tz_outer_cache_mutex == NULL))
		return 0;

	if (reg == false) {
		vaddr = ptee->tz_outer_cache_mutex;
		ptee->tz_outer_cache_mutex = NULL;
		goto out;
	}

	memset(&param, 0, sizeof(param));
	param.a0 = TEESMC32_ST_FASTCALL_L2CC_MUTEX;
	param.a1 = TEESMC_ST_L2CC_MUTEX_GET_ADDR;
	smc_xfer(&param);

	if (param.a0 != TEESMC_RETURN_OK) {
		pr_err("no TZ l2cc mutex service supported\n");
		goto out;
	}
	paddr = param.a2;
	pr_debug("outer cache shared mutex paddr=0x%lx\n", paddr);

	vaddr = tee_map_cached_shm(paddr, sizeof(u32));
	if (vaddr == NULL) {
		pr_err("TZ l2cc mutex disabled: ioremap failed\n");
		ret = -ENOMEM;
		goto out;
	}

	pr_debug("outer cache shared mutex vaddr=%p\n", vaddr);
	if (outer_tz_mutex(vaddr) == false) {
		pr_err("TZ l2cc mutex disabled: outer cache refused\n");
		goto out;
	}

	memset(&param, 0, sizeof(param));
	param.a0 = TEESMC32_ST_FASTCALL_L2CC_MUTEX;
	param.a1 = TEESMC_ST_L2CC_MUTEX_ENABLE;
	smc_xfer(&param);

	if (param.a0 != TEESMC_RETURN_OK) {
		pr_warn("TZ l2cc mutex disabled: TZ enable failed\n");
		goto out;
	}
	ptee->tz_outer_cache_mutex = vaddr;

out:
	if (ptee->tz_outer_cache_mutex == NULL) {
		memset(&param, 0, sizeof(param));
		param.a0 = TEESMC32_ST_FASTCALL_L2CC_MUTEX;
		param.a1 = TEESMC_ST_L2CC_MUTEX_DISABLE;
		smc_xfer(&param);
		outer_tz_mutex(NULL);
		if (vaddr)
			iounmap(vaddr);

		pr_debug("outer cache shared mutex disabled\n");
	}

	pr_debug("< teetz outer mutex: ret=%d pa=0x%lX va=0x%p %sabled\n",
		ret, paddr, vaddr, ptee->tz_outer_cache_mutex ? "en" : "dis");
	return ret;
}
#endif

/* configure_shm - Negotiate Shared Memory configuration with teetz. */
static int configure_shm(struct tee_tz *ptee)
{
	struct smc_param param = { 0 };
	size_t shm_size = -1;
	int ret = 0;

	WARN_ON(!CAPABLE(ptee->tee));

	param.a0 = TEESMC32_ST_FASTCALL_GET_SHM_CONFIG;
	smc_xfer(&param);

	if (param.a0 != TEESMC_RETURN_OK) {
		pr_err("shm service not available: 0x%x",
			(uint) param.a0);
		ret = -EINVAL;
		goto out;
	}

	ptee->shm_paddr = param.a1;
	shm_size = param.a2;
	ptee->shm_cached = (bool)param.a3;

	if (ptee->shm_cached)
		ptee->shm_vaddr = tee_map_cached_shm(ptee->shm_paddr, shm_size);
	else
		ptee->shm_vaddr = ioremap_nocache(ptee->shm_paddr, shm_size);

	if (ptee->shm_vaddr == NULL) {
		pr_err("shm ioremap failed\n");
		ret = -ENOMEM;
		goto out;
	}

	ptee->shm_pool = tee_shm_pool_create(DEV, shm_size,
		ptee->shm_vaddr, ptee->shm_paddr);

	if (!ptee->shm_pool) {
		pr_err("shm pool creation failed (%zu)", shm_size);
		ret = -EINVAL;
		goto out;
	}

	if (ptee->shm_cached)
		tee_shm_pool_set_cached(ptee->shm_pool);
out:
	pr_debug("< ret=%d pa=0x%lX va=0x%p size=%zu, %scached",
		ret, ptee->shm_paddr, ptee->shm_vaddr, shm_size,
		(ptee->shm_cached == 1) ? "" : "un");
	return ret;
}

static int tz_start(struct tee *tee)
{
	struct tee_tz *ptee;
	int ret;

	WARN_ON(!tee || !tee->priv);

	if (!CAPABLE(tee)) {
		pr_err("not capable\n");
		return -EBUSY;
	}

	ptee = tee->priv;
	WARN_ON(ptee->started);
	ptee->started = true;

	ret = configure_shm(ptee);
	if (ret)
		goto exit;


#ifdef CONFIG_OUTER_CACHE
	ret = register_outercache_mutex(ptee, true);
	if (ret)
		goto exit;
#endif

exit:
	if (ret)
		ptee->started = false;

	pr_debug("< ret=%d dev=%s\n", ret, tee->name);
	return ret;
}

static int tz_stop(struct tee *tee)
{
	struct tee_tz *ptee;

	WARN_ON(!tee || !tee->priv);

	ptee = tee->priv;

	pr_debug("> dev=%s\n", tee->name);

	if (!CAPABLE(tee)) {
		pr_err("tee: bad state\n");
		return -EBUSY;
	}

#ifdef CONFIG_OUTER_CACHE
	register_outercache_mutex(ptee, false);
#endif
	tee_shm_pool_destroy(tee->dev, ptee->shm_pool);
	iounmap(ptee->shm_vaddr);
	ptee->started = false;

	pr_debug("< ret=0 dev=%s\n", tee->name);
	return 0;
}

/******************************************************************************/

const struct tee_ops tee_tz_fops = {
	.type = "tz",
	.owner = THIS_MODULE,
	.start = tz_start,
	.stop = tz_stop,
	.invoke = tz_invoke,
	.cancel = tz_cancel,
	.open = tz_open,
	.close = tz_close,
	.alloc = tz_alloc,
	.free = tz_free,
	.shm_inc_ref = tz_shm_inc_ref,

	.call_tee = smc_xfer,
	.raw_call_tee = tee_smc_call,
};

static int tz_tee_init(struct platform_device *pdev)
{
	int ret = 0;

	struct tee *tee = platform_get_drvdata(pdev);
	struct tee_tz *ptee = tee->priv;

	tee_tz = ptee;

	ptee->started = false;

	tee_wait_queue_init(&ptee->wait_queue);

	if (ret) {
		pr_err("dev=%s, Secure failed (%d)\n",
			tee->name, ret);
	}

	return ret;
}

static void tz_tee_deinit(struct platform_device *pdev)
{
	struct tee *tee = platform_get_drvdata(pdev);
	struct tee_tz *ptee = tee->priv;

	if (!CAPABLE(tee))
		return;

	tee_wait_queue_exit(&ptee->wait_queue);

	pr_debug("dev=%s started=%d\n",
		tee->name, ptee->started);
}

static int tz_tee_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct tee *tee;
	struct tee_tz *ptee;

	tee = tee_core_alloc(dev, _TEE_TZ_NAME, pdev->id, &tee_tz_fops,
				sizeof(struct tee_tz));
	if (!tee)
		return -ENOMEM;

	ptee = tee->priv;
	ptee->tee = tee;

	platform_set_drvdata(pdev, tee);

	ret = tz_tee_init(pdev);
	if (ret)
		goto bail0;

	ret = tee_core_add(tee);
	if (ret)
		goto bail1;

	pr_debug("tee=%p, id=%d, iminor=%d\n", tee, tee->id,
		 tee->miscdev.minor);

	ret = __tee_get(tee);
	if (ret) {
		tz_tee_deinit(pdev);
		tee_core_del(tee);

		return ret;
	}

	return 0;

bail1:
	tz_tee_deinit(pdev);
bail0:
	tee_core_free(tee);
	return ret;
}

static int tz_tee_remove(struct platform_device *pdev)
{
	struct tee *tee = platform_get_drvdata(pdev);

	tz_tee_deinit(pdev);
	tee_core_del(tee);
	return 0;
}

static const struct of_device_id tz_tee_match[] = {
	{
		.compatible = "trustkernel,tzdrv",
	},
	{},
};

static struct platform_driver tz_tee_driver = {
	.probe = tz_tee_probe,
	.remove = tz_tee_remove,

	.driver = {
		.name = "tzdrv",
		.owner = THIS_MODULE,
		.of_match_table = tz_tee_match,
	},
};

static struct platform_device tz_0_plt_device = {
	.name = "tzdrv",
	.id = 0,
	.dev = {
		/*	.platform_data = tz_0_tee_data,*/
	},
};

static int __init tee_tz_init(void)
{
	int rc;

	pr_info("TrustKernel TEE Driver initialization\n");

	rc = tee_init_smc_xfer();
	if (rc < 0)
		return rc;

	rc = platform_device_register(&tz_0_plt_device);
	if (rc < 0) {
		pr_err(
			"failed to register the platform devices 0 (rc=%d)\n",
			rc);
		goto err0;
	}

	rc = platform_driver_register(&tz_tee_driver);
	if (rc < 0) {
		pr_err("failed to probe the platform driver (rc=%d)\n",
			rc);
		goto err1;
	}

	return 0;

err1:
	platform_device_unregister(&tz_0_plt_device);
err0:
	tee_exit_smc_xfer();

	return rc;
}

static void __exit tee_tz_exit(void)
{
	pr_info("TrustKernel TEE Driver Release\n");

	platform_device_unregister(&tz_0_plt_device);
	platform_driver_unregister(&tz_tee_driver);

	tee_exit_smc_xfer();
}

#ifndef MODULE
rootfs_initcall(tee_tz_init);
#else
module_init(tee_tz_init);
#endif
module_exit(tee_tz_exit);

MODULE_AUTHOR("TrustKernel");
MODULE_DESCRIPTION("TrustKernel TKCore TZ driver");
MODULE_SUPPORTED_DEVICE("");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
