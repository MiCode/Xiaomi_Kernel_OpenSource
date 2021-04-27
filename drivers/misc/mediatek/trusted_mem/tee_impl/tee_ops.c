// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define PR_FMT_HEADER_MUST_BE_INCLUDED_BEFORE_ALL_HDRS
#include "private/tmem_pr_fmt.h" PR_FMT_HEADER_MUST_BE_INCLUDED_BEFORE_ALL_HDRS

#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/unistd.h>
#include <linux/types.h>
#include <linux/slab.h>

#include "private/mld_helper.h"
#include "private/tmem_device.h"
#include "private/tmem_error.h"
#include "private/tmem_utils.h"
#include "private/tmem_dev_desc.h"
#include "tee_impl/tee_ops.h"
#include "tee_impl/tee_gp_def.h"
#if IS_ENABLED(CONFIG_MTK_GZ_KREE)
#include "mtee_impl/mtee_invoke.h"
#endif
#include "tee_client_api.h"

/* clang-format off */
#define SECMEM_TL_GP_UUID \
	{ 0x08030000, 0x0000, 0x0000, \
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } }
/* clang-format on */

#define SECMEM_TL_GP_UUID_STRING "08030000000000000000000000000000"

struct TEE_GP_SESSION_DATA {
	struct TEEC_Context context;
	struct TEEC_Session session;
	struct TEEC_SharedMemory wsm;
	void *wsm_buffer;
};

static struct TEE_GP_SESSION_DATA *g_sess_data;
static bool is_sess_ready;
static unsigned int sess_ref_cnt;

#define TEE_SESSION_LOCK() mutex_lock(&g_sess_lock)
#define TEE_SESSION_UNLOCK() mutex_unlock(&g_sess_lock)
static DEFINE_MUTEX(g_sess_lock);

static struct TEE_GP_SESSION_DATA *tee_gp_create_session_data(void)
{
	struct TEE_GP_SESSION_DATA *data;

	data = mld_kmalloc(sizeof(struct TEE_GP_SESSION_DATA), GFP_KERNEL);
	if (INVALID(data))
		return NULL;

	memset(data, 0x0, sizeof(struct TEE_GP_SESSION_DATA));
	return data;
}

static void tee_gp_destroy_session_data(void)
{
	if (VALID(g_sess_data)) {
		mld_kfree(g_sess_data);
		g_sess_data = NULL;
	}
}

static int tee_session_open_single_session_unlocked(void)
{
	int ret = TMEM_OK;
	struct TEEC_UUID destination = SECMEM_TL_GP_UUID;

	if (is_sess_ready) {
		pr_debug("UT_SUITE:Session is already created!\n");
		return TMEM_OK;
	}

	g_sess_data = tee_gp_create_session_data();
	if (INVALID(g_sess_data)) {
		pr_err("Create session data failed: out of memory!\n");
		return TMEM_TEE_CREATE_SESSION_DATA_FAILED;
	}

	g_sess_data->wsm_buffer =
		mld_kmalloc(sizeof(struct secmem_ta_msg_t), GFP_KERNEL);
	if (INVALID(g_sess_data->wsm_buffer)) {
		pr_err("Create wsm buffer failed: out of memory!\n");
		goto err_create_wsm_buffer;
	}

	ret = TEEC_InitializeContext(SECMEM_TL_GP_UUID_STRING,
				     &g_sess_data->context);
	if (ret != TEEC_SUCCESS) {
		pr_err("TEEC_InitializeContext failed: %x\n", ret);
		goto err_initialize_context;
	}

	g_sess_data->wsm.buffer = g_sess_data->wsm_buffer;
	g_sess_data->wsm.size = sizeof(struct secmem_ta_msg_t);
	g_sess_data->wsm.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;
	memset(g_sess_data->wsm.buffer, 0, g_sess_data->wsm.size);

	ret = TEEC_RegisterSharedMemory(&g_sess_data->context,
					&g_sess_data->wsm);
	if (ret != TEEC_SUCCESS) {
		pr_err("TEEC_RegisterSharedMemory failed: %x\n", ret);
		goto err_register_shared_memory;
	}

	ret = TEEC_OpenSession(&g_sess_data->context, &g_sess_data->session,
			       &destination, TEEC_LOGIN_PUBLIC, NULL, NULL,
			       NULL);
	if (ret != TEEC_SUCCESS) {
		pr_err("TEEC_OpenSession failed: %x\n", ret);
		goto err_open_session;
	}

	is_sess_ready = true;
	return TMEM_OK;

err_open_session:
	pr_err("TEEC_ReleaseSharedMemory\n");
	TEEC_ReleaseSharedMemory(&g_sess_data->wsm);

err_register_shared_memory:
	pr_err("TEEC_FinalizeContext\n");
	TEEC_FinalizeContext(&g_sess_data->context);

err_initialize_context:
	mld_kfree(g_sess_data->wsm_buffer);

err_create_wsm_buffer:
	tee_gp_destroy_session_data();

	return TMEM_TEE_CREATE_SESSION_FAILED;
}

static int tee_session_close_single_session_unlocked(void)
{
	if (!is_sess_ready) {
		pr_debug("Session is already closed!\n");
		return TMEM_OK;
	}

	TEEC_CloseSession(&g_sess_data->session);
	TEEC_ReleaseSharedMemory(&g_sess_data->wsm);
	TEEC_FinalizeContext(&g_sess_data->context);
	mld_kfree(g_sess_data->wsm_buffer);
	tee_gp_destroy_session_data();
	is_sess_ready = false;
	return TMEM_OK;
}

int tee_session_open(void **tee_data, void *dev_desc)
{
	UNUSED(tee_data);
	UNUSED(dev_desc);

	TEE_SESSION_LOCK();

	if (tee_session_open_single_session_unlocked() == TMEM_OK)
		sess_ref_cnt++;

	TEE_SESSION_UNLOCK();
	return TMEM_OK;
}

int tee_session_close(void *tee_data, void *dev_desc)
{
	UNUSED(tee_data);
	UNUSED(dev_desc);

	TEE_SESSION_LOCK();

	if (sess_ref_cnt == 0) {
		pr_err("Session is already closed!\n");
		TEE_SESSION_UNLOCK();
		return TMEM_OK;
	}

	sess_ref_cnt--;
	if (sess_ref_cnt == 0) {
		pr_debug("Try closing session!\n");
		tee_session_close_single_session_unlocked();
	}

	TEE_SESSION_UNLOCK();
	return TMEM_OK;
}

static int secmem_execute(u32 cmd, struct secmem_param *param)
{
	int ret = TEEC_SUCCESS;
	struct TEEC_Operation op;
	struct secmem_ta_msg_t *msg;

	TEE_SESSION_LOCK();

	if (!is_sess_ready) {
		pr_err("Session is not ready!\n");
		TEE_SESSION_UNLOCK();
		return TMEM_TEE_SESSION_IS_NOT_READY;
	}

	memset(g_sess_data->wsm.buffer, 0, g_sess_data->wsm.size);
	msg = g_sess_data->wsm.buffer;
	msg->sec_handle = param->sec_handle;
	msg->alignment = param->alignment;
	msg->size = param->size;
	msg->refcount = param->refcount;

	memset(&op, 0, sizeof(struct TEEC_Operation));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INOUT, TEEC_NONE,
					 TEEC_NONE, TEEC_NONE);
	op.params[0].memref.parent = &g_sess_data->wsm;
	op.params[0].memref.offset = 0;
	op.params[0].memref.size = g_sess_data->wsm.size;

	ret = TEEC_InvokeCommand(&g_sess_data->session, cmd, &op, NULL);
	if (ret != TEEC_SUCCESS) {
		pr_err("TEEC_InvokeCommand failed! cmd:%d, ret:0x%x\n", cmd,
		       ret);
		TEE_SESSION_UNLOCK();
		return ret;
	}

	param->sec_handle = msg->sec_handle;
	param->refcount = msg->refcount;
	param->alignment = msg->alignment;
	param->size = msg->size;

	pr_debug("shndl=0x%llx refcnt=%d align=0x%llx size=0x%llx\n",
		 (u64)param->sec_handle, param->refcount, (u64)param->alignment,
		 (u64)param->size);

	TEE_SESSION_UNLOCK();
	return TMEM_OK;
}

#define SECMEM_ERROR_OUT_OF_MEMORY (0x8)
int tee_alloc(u32 alignment, u32 size, u32 *refcount, u32 *sec_handle,
	      u8 *owner, u32 id, u32 clean, void *tee_data, void *dev_desc)
{
	int ret;
	struct secmem_param param = {0};
	struct tmem_device_description *tee_dev_desc =
		(struct tmem_device_description *)dev_desc;
	struct tee_peer_ops_data *ops_data = &tee_dev_desc->u_ops_data.tee;
	u32 tee_ta_cmd = (clean ? ops_data->tee_cmds[TEE_OP_ALLOC_ZERO]
				: ops_data->tee_cmds[TEE_OP_ALLOC]);

	UNUSED(tee_data);
	UNUSED(owner);
	UNUSED(id);
	UNUSED(dev_desc);

	*refcount = 0;
	*sec_handle = 0;

	param.alignment = alignment;
	param.size = size;
	param.refcount = 0;
	param.sec_handle = 0;

	ret = secmem_execute(tee_ta_cmd, &param);
	if (ret == SECMEM_ERROR_OUT_OF_MEMORY) {
		pr_err("%s:%d out of memory!\n", __func__, __LINE__);
		return -ENOMEM;
	} else if (ret) {
		return TMEM_TEE_ALLOC_CHUNK_FAILED;
	}

	*refcount = param.refcount;
	*sec_handle = param.sec_handle;
	pr_debug("ref cnt: 0x%x, sec_handle: 0x%llx\n", param.refcount,
		 param.sec_handle);
	return TMEM_OK;
}

int tee_free(u32 sec_handle, u8 *owner, u32 id, void *tee_data, void *dev_desc)
{
	struct secmem_param param = {0};
	struct tmem_device_description *tee_dev_desc =
		(struct tmem_device_description *)dev_desc;
	struct tee_peer_ops_data *ops_data = &tee_dev_desc->u_ops_data.tee;
	u32 tee_ta_cmd = ops_data->tee_cmds[TEE_OP_FREE];

	UNUSED(tee_data);
	UNUSED(owner);
	UNUSED(id);
	UNUSED(dev_desc);

	param.sec_handle = sec_handle;

	if (secmem_execute(tee_ta_cmd, &param))
		return TMEM_TEE_FREE_CHUNK_FAILED;

	return TMEM_OK;
}

int tee_mem_reg_add(u64 pa, u32 size, void *tee_data, void *dev_desc)
{
	int ret;
	struct secmem_param param = {0};
	struct tmem_device_description *tee_dev_desc =
		(struct tmem_device_description *)dev_desc;
	struct tee_peer_ops_data *ops_data = &tee_dev_desc->u_ops_data.tee;
	u32 tee_ta_cmd = ops_data->tee_cmds[TEE_OP_REGION_ENABLE];

	UNUSED(tee_data);
	UNUSED(dev_desc);
	param.sec_handle = pa;
	param.size = size;

	if (secmem_execute(tee_ta_cmd, &param))
		return TMEM_TEE_APPEND_MEMORY_FAILED;

	if (tee_dev_desc->notify_remote && tee_dev_desc->notify_remote_fn) {
		ret = tee_dev_desc->notify_remote_fn(
			pa, size, tee_dev_desc->mtee_chunks_id);
		if (ret != 0) {
			pr_err("[%d] TEE notify reg mem add to MTEE failed:%d\n",
			       tee_dev_desc->kern_tmem_type, ret);
			return TMEM_TEE_NOTIFY_MEM_ADD_CFG_TO_MTEE_FAILED;
		}
	}

	pr_info("[%d] TEE append reg mem PASS: PA=0x%lx, size=0x%lx\n",
				tee_dev_desc->kern_tmem_type, pa, size);

	return TMEM_OK;
}

int tee_mem_reg_remove(void *tee_data, void *dev_desc)
{
	int ret;
	struct secmem_param param = {0};
	struct tmem_device_description *tee_dev_desc =
		(struct tmem_device_description *)dev_desc;
	struct tee_peer_ops_data *ops_data = &tee_dev_desc->u_ops_data.tee;
	u32 tee_ta_cmd = ops_data->tee_cmds[TEE_OP_REGION_DISABLE];

	UNUSED(tee_data);
	UNUSED(dev_desc);

	if (secmem_execute(tee_ta_cmd, &param))
		return TMEM_TEE_RELEASE_MEMORY_FAILED;

	if (tee_dev_desc->notify_remote && tee_dev_desc->notify_remote_fn) {
		ret = tee_dev_desc->notify_remote_fn(
			0x0ULL, 0x0, tee_dev_desc->mtee_chunks_id);
		if (ret != 0) {
			pr_err("[%d] TEE notify reg mem remove to MTEE failed:%d\n",
			       tee_dev_desc->mtee_chunks_id, ret);
			return TMEM_TEE_NOTIFY_MEM_REMOVE_CFG_TO_MTEE_FAILED;
		}
	}

	return TMEM_OK;
}

#define VALID_INVOKE_COMMAND(cmd)                                              \
	((cmd >= CMD_SEC_MEM_INVOKE_CMD_START)                                 \
	 && (cmd <= CMD_SEC_MEM_INVOKE_CMD_END))

static int tee_invoke_command(struct trusted_driver_cmd_params *invoke_params,
			      void *tee_data, void *dev_desc)
{
	struct secmem_param param = {0};

	UNUSED(tee_data);
	UNUSED(dev_desc);

	if (INVALID(invoke_params))
		return TMEM_PARAMETER_ERROR;

	if (!VALID_INVOKE_COMMAND(invoke_params->cmd)) {
		pr_err("%s:%d unsupported cmd:%d!\n", __func__, __LINE__,
		       invoke_params->cmd);
		return TMEM_COMMAND_NOT_SUPPORTED;
	}

	pr_debug("invoke cmd is %d (0x%llx, 0x%llx, 0x%llx, 0x%llx)\n",
		 invoke_params->cmd, invoke_params->param0,
		 invoke_params->param1, invoke_params->param2,
		 invoke_params->param3);

	param.alignment = invoke_params->param0;
	param.size = invoke_params->param1;
	/* CAUTION: USE IT CAREFULLY!
	 * For param2, secmem ta only supports 32-bit for backward compatibility
	 */
	param.refcount = (u32)invoke_params->param2;
	param.sec_handle = invoke_params->param3;

	if (secmem_execute(invoke_params->cmd, &param))
		return TMEM_TEE_INVOKE_COMMAND_FAILED;

	invoke_params->param0 = param.alignment;
	invoke_params->param1 = param.size;
	/* CAUTION: USE IT CAREFULLY!
	 * For param2, secmem ta only supports 32-bit for backward compatibility
	 */
	invoke_params->param2 = (u64)param.refcount;
	invoke_params->param3 = param.sec_handle;
	return TMEM_OK;
}

static struct trusted_driver_operations tee_gp_peer_ops = {
	.session_open = tee_session_open,
	.session_close = tee_session_close,
	.memory_alloc = tee_alloc,
	.memory_free = tee_free,
	.memory_grant = tee_mem_reg_add,
	.memory_reclaim = tee_mem_reg_remove,
	.invoke_cmd = tee_invoke_command,
};

void get_tee_peer_ops(struct trusted_driver_operations **ops)
{
	pr_info("TEE_OPS set\n");
	*ops = &tee_gp_peer_ops;
}
