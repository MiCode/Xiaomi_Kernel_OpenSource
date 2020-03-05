/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
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
#if defined(CONFIG_MTK_GZ_KREE)
#include <kree/system.h>
#include <kree/mem.h>
#include <kree/tz_mod.h>
#include <tz_cross/ta_mem.h>
#endif

#include "private/mld_helper.h"
#include "private/tmem_device.h"
#include "private/tmem_error.h"
#include "private/tmem_utils.h"
#include "private/tmem_dev_desc.h"
#include "public/mtee_regions.h"
/* clang-format off */
#include "mtee_impl/mtee_ops.h"
/* clang-format on */
#include "tee_impl/tee_invoke.h"

static const char mem_srv_name[] = "com.mediatek.geniezone.srv.mem";

#define LOCK_BY_CALLEE (0)
#if LOCK_BY_CALLEE
#define MTEE_SESSION_LOCK_INIT() mutex_init(&sess_data->lock)
#define MTEE_SESSION_LOCK() mutex_lock(&sess_data->lock)
#define MTEE_SESSION_UNLOCK() mutex_unlock(&sess_data->lock)
#else
#define MTEE_SESSION_LOCK_INIT()
#define MTEE_SESSION_LOCK()
#define MTEE_SESSION_UNLOCK()
#endif

struct MTEE_SESSION_DATA {
	KREE_SESSION_HANDLE session_handle;
	KREE_SECUREMEM_HANDLE append_mem_handle;
#if LOCK_BY_CALLEE
	struct mutex lock;
#endif
};

static struct MTEE_SESSION_DATA *
mtee_create_session_data(enum TRUSTED_MEM_TYPE mem_type)
{
	struct MTEE_SESSION_DATA *sess_data;

	sess_data = mld_kmalloc(sizeof(struct MTEE_SESSION_DATA), GFP_KERNEL);
	if (INVALID(sess_data)) {
		pr_err("%s:%d %d:out of memory!\n", __func__, __LINE__,
		       mem_type);
		return NULL;
	}

	memset(sess_data, 0x0, sizeof(struct MTEE_SESSION_DATA));

	MTEE_SESSION_LOCK_INIT();
	return sess_data;
}

static void mtee_destroy_session_data(struct MTEE_SESSION_DATA *sess_data)
{
	if (VALID(sess_data))
		mld_kfree(sess_data);
}

static int mtee_session_open(void **peer_data, void *dev_desc)
{
	int ret = 0;
	struct MTEE_SESSION_DATA *sess_data;
	struct tmem_device_description *mtee_dev_desc =
		(struct tmem_device_description *)dev_desc;
	struct mtee_peer_ops_data *ops_data = &mtee_dev_desc->u_ops_data.mtee;

	UNUSED(dev_desc);

	sess_data = mtee_create_session_data(mtee_dev_desc->kern_tmem_type);
	if (INVALID(sess_data)) {
		pr_err("[%d] Create session data failed: out of memory!\n",
		       mtee_dev_desc->kern_tmem_type);
		return TMEM_MTEE_CREATE_SESSION_FAILED;
	}

	MTEE_SESSION_LOCK();

	if (unlikely(ops_data->service_name))
		ret = KREE_CreateSession(ops_data->service_name,
					 &sess_data->session_handle);
	else
		ret = KREE_CreateSession(mem_srv_name,
					 &sess_data->session_handle);
	if (ret != 0) {
		pr_err("[%d] MTEE open session failed:%d (srv=%s)\n",
		       mtee_dev_desc->kern_tmem_type, ret,
		       (ops_data->service_name ? ops_data->service_name
					       : mem_srv_name));
		MTEE_SESSION_UNLOCK();
		return TMEM_MTEE_CREATE_SESSION_FAILED;
	}

	*peer_data = (void *)sess_data;
	MTEE_SESSION_UNLOCK();
	return TMEM_OK;
}

static int mtee_session_close(void *peer_data, void *dev_desc)
{
	int ret;
	struct MTEE_SESSION_DATA *sess_data =
		(struct MTEE_SESSION_DATA *)peer_data;
	struct tmem_device_description *mtee_dev_desc =
		(struct tmem_device_description *)dev_desc;
	struct mtee_peer_ops_data *ops_data = &mtee_dev_desc->u_ops_data.mtee;

	UNUSED(ops_data);
	MTEE_SESSION_LOCK();

	ret = KREE_CloseSession(sess_data->session_handle);
	if (ret != 0) {
		pr_err("[%d] MTEE close session failed:%d\n",
		       mtee_dev_desc->kern_tmem_type, ret);
		MTEE_SESSION_UNLOCK();
		return TMEM_MTEE_CLOSE_SESSION_FAILED;
	}

	MTEE_SESSION_UNLOCK();
	mtee_destroy_session_data(sess_data);
	return TMEM_OK;
}

static int mtee_alloc(u32 alignment, u32 size, u32 *refcount, u32 *sec_handle,
		      u8 *owner, u32 id, u32 clean, void *peer_data,
		      void *dev_desc)
{
	int ret;
	struct MTEE_SESSION_DATA *sess_data =
		(struct MTEE_SESSION_DATA *)peer_data;
	struct tmem_device_description *mtee_dev_desc =
		(struct tmem_device_description *)dev_desc;
	struct mtee_peer_ops_data *ops_data = &mtee_dev_desc->u_ops_data.mtee;

	UNUSED(ops_data);
	MTEE_SESSION_LOCK();

	if (clean) {
		ret = KREE_ION_ZallocChunkmem(sess_data->session_handle,
					      sess_data->append_mem_handle,
					      sec_handle, alignment, size);
	} else {
		ret = KREE_ION_AllocChunkmem(sess_data->session_handle,
					     sess_data->append_mem_handle,
					     sec_handle, alignment, size);
	}

	if (*sec_handle == 0) {
		pr_err("%s:%d out of memory, ret=%d!\n", __func__, __LINE__,
		       ret);
		MTEE_SESSION_UNLOCK();
		return -ENOMEM;
	} else if (ret != 0) {
		pr_err("[%d] MTEE alloc chunk memory failed:%d\n",
		       mtee_dev_desc->kern_tmem_type, ret);
		MTEE_SESSION_UNLOCK();
		return TMEM_MTEE_ALLOC_CHUNK_FAILED;
	}

	*refcount = 1;
	MTEE_SESSION_UNLOCK();
	return TMEM_OK;
}

static int mtee_free(u32 sec_handle, u8 *owner, u32 id, void *peer_data,
		     void *dev_desc)
{
	int ret;
	struct MTEE_SESSION_DATA *sess_data =
		(struct MTEE_SESSION_DATA *)peer_data;
	struct tmem_device_description *mtee_dev_desc =
		(struct tmem_device_description *)dev_desc;
	struct mtee_peer_ops_data *ops_data = &mtee_dev_desc->u_ops_data.mtee;

	UNUSED(ops_data);
	MTEE_SESSION_LOCK();

	ret = KREE_ION_UnreferenceChunkmem(sess_data->session_handle,
					   sec_handle);
	if (ret != 0) {
		pr_err("[%d] MTEE free chunk memory failed:%d\n",
		       mtee_dev_desc->kern_tmem_type, ret);
		MTEE_SESSION_UNLOCK();
		return TMEM_MTEE_FREE_CHUNK_FAILED;
	}

	MTEE_SESSION_UNLOCK();
	return TMEM_OK;
}

static int mtee_mem_reg_add(u64 pa, u32 size, void *peer_data, void *dev_desc)
{
	int ret;
	struct MTEE_SESSION_DATA *sess_data =
		(struct MTEE_SESSION_DATA *)peer_data;
	struct tmem_device_description *mtee_dev_desc =
		(struct tmem_device_description *)dev_desc;
	struct mtee_peer_ops_data *ops_data = &mtee_dev_desc->u_ops_data.mtee;
	KREE_SHAREDMEM_PARAM mem_param;

	mem_param.buffer = (void *)pa;
	mem_param.size = size;
	mem_param.mapAry = NULL;
	mem_param.region_id = mtee_dev_desc->mtee_chunks_id;

	UNUSED(ops_data);
	MTEE_SESSION_LOCK();

	ret = KREE_AppendSecureMultichunkmem(sess_data->session_handle,
					     &sess_data->append_mem_handle,
					     &mem_param);
	if (ret != 0) {
		pr_err("[%d] MTEE append reg mem failed:%d\n",
		       mtee_dev_desc->kern_tmem_type, ret);
		MTEE_SESSION_UNLOCK();
		return TMEM_MTEE_APPEND_MEMORY_FAILED;
	}

	if (mtee_dev_desc->notify_remote && mtee_dev_desc->notify_remote_fn) {
		ret = mtee_dev_desc->notify_remote_fn(
			pa, size, mtee_dev_desc->tee_smem_type);
		if (ret != 0) {
			pr_err("[%d] MTEE notify reg mem add to TEE failed:%d\n",
			       mtee_dev_desc->tee_smem_type, ret);
			MTEE_SESSION_UNLOCK();
			return TMEM_MTEE_NOTIFY_MEM_ADD_CFG_TO_TEE_FAILED;
		}
	}

	MTEE_SESSION_UNLOCK();
	return TMEM_OK;
}

static int mtee_mem_reg_remove(void *peer_data, void *dev_desc)
{
	int ret;
	struct MTEE_SESSION_DATA *sess_data =
		(struct MTEE_SESSION_DATA *)peer_data;
	struct tmem_device_description *mtee_dev_desc =
		(struct tmem_device_description *)dev_desc;
	struct mtee_peer_ops_data *ops_data = &mtee_dev_desc->u_ops_data.mtee;

	UNUSED(ops_data);
	MTEE_SESSION_LOCK();

	ret = KREE_ReleaseSecureMultichunkmem(sess_data->session_handle,
					      sess_data->append_mem_handle);
	if (ret != 0) {
		pr_err("[%d] MTEE release reg mem failed:%d\n",
		       mtee_dev_desc->kern_tmem_type, ret);
		MTEE_SESSION_UNLOCK();
		return TMEM_MTEE_RELEASE_MEMORY_FAILED;
	}

	if (mtee_dev_desc->notify_remote && mtee_dev_desc->notify_remote_fn) {
		ret = mtee_dev_desc->notify_remote_fn(
			0x0ULL, 0x0, mtee_dev_desc->tee_smem_type);
		if (ret != 0) {
			pr_err("[%d] MTEE notify reg mem remove to TEE failed:%d\n",
			       mtee_dev_desc->tee_smem_type, ret);
			MTEE_SESSION_UNLOCK();
			return TMEM_MTEE_NOTIFY_MEM_REMOVE_CFG_TO_TEE_FAILED;
		}
	}

	MTEE_SESSION_UNLOCK();
	return TMEM_OK;
}

static int mtee_drv_execute(KREE_SESSION_HANDLE session_handle, u32 cmd,
			    struct mtee_driver_params *drv_params)
{
	union MTEEC_PARAM svc_call_param[4];

	svc_call_param[0].mem.buffer = drv_params;
	svc_call_param[0].mem.size = sizeof(struct mtee_driver_params);

	return KREE_TeeServiceCall(session_handle, cmd,
				   TZ_ParamTypes1(TZPT_MEM_INOUT),
				   svc_call_param);
}

static int mtee_mem_srv_execute(KREE_SESSION_HANDLE session_handle, u32 cmd,
				struct mtee_driver_params *drv_params)
{
	int ret = TMEM_OK;

	switch (cmd) {
	case TZCMD_MEM_CONFIG_CHUNKMEM_INFO_ION:
		ret = KREE_ConfigSecureMultiChunkMemInfo(
			session_handle, drv_params->param0, drv_params->param1,
			drv_params->param2);
		break;
	default:
		pr_err("%s:%d operation is not implemented yet!\n", __func__,
		       __LINE__);
		ret = TMEM_OPERATION_NOT_IMPLEMENTED;
		break;
	}

	return ret;
}

static int mtee_invoke_command(struct trusted_driver_cmd_params *invoke_params,
			       void *peer_data, void *dev_desc)
{
	int ret = TMEM_OK;
	struct MTEE_SESSION_DATA *sess_data =
		(struct MTEE_SESSION_DATA *)peer_data;
	struct tmem_device_description *mtee_dev_desc =
		(struct tmem_device_description *)dev_desc;
	struct mtee_peer_ops_data *ops_data = &mtee_dev_desc->u_ops_data.mtee;
	struct mtee_driver_params drv_params = {0};

	if (INVALID(invoke_params))
		return TMEM_PARAMETER_ERROR;

	drv_params.param0 = invoke_params->param0;
	drv_params.param1 = invoke_params->param1;
	drv_params.param2 = invoke_params->param2;
	drv_params.param3 = invoke_params->param3;

	if (unlikely(ops_data->service_name)) {
		ret = mtee_drv_execute(sess_data->session_handle,
				       invoke_params->cmd, &drv_params);
		if (ret) {
			pr_err("%s:%d invoke failed! cmd:%d, ret:0x%x\n",
			       __func__, __LINE__, invoke_params->cmd, ret);
			return TMEM_MTEE_INVOKE_COMMAND_FAILED;
		}
	} else {
		ret = mtee_mem_srv_execute(sess_data->session_handle,
					   invoke_params->cmd, &drv_params);
		if (ret) {
			pr_err("%s:%d invoke failed! cmd:%d, ret:0x%x\n",
			       __func__, __LINE__, invoke_params->cmd, ret);
			return TMEM_MTEE_INVOKE_COMMAND_FAILED;
		}
	}

	invoke_params->param0 = drv_params.param0;
	invoke_params->param1 = drv_params.param1;
	invoke_params->param2 = drv_params.param2;
	invoke_params->param3 = drv_params.param3;

	return TMEM_OK;
}

static struct trusted_driver_operations mtee_peer_ops = {
	.session_open = mtee_session_open,
	.session_close = mtee_session_close,
	.memory_alloc = mtee_alloc,
	.memory_free = mtee_free,
	.memory_grant = mtee_mem_reg_add,
	.memory_reclaim = mtee_mem_reg_remove,
	.invoke_cmd = mtee_invoke_command,
};

void get_mtee_peer_ops(struct trusted_driver_operations **ops)
{
	pr_info("MTEE_PEER_OPS\n");
	*ops = &mtee_peer_ops;
}
