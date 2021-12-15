/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include "pseudo_m4u_gz_sec.h"
#include "pseudo_m4u_log.h"
#include "kree/mem.h"
#include <linux/slab.h>
#include "kree/system.h"
#include "tz_cross/trustzone.h"
#include "tz_cross/ta_system.h"

#define mem_srv_name  "com.mediatek.geniezone.srv.mem"
#define m4u_srv_name  "com.mediatek.geniezone.srv.m4u_sec_ha"

static struct m4u_sec_ty_context m4u_ty_ctx = {
	.ctx_lock = __MUTEX_INITIALIZER(m4u_ty_ctx.ctx_lock),
};
struct m4u_gz_sec_context m4u_gz_ta_ctx;
struct gz_m4u_msg *shared_buf;
unsigned int _shm_size = PAGE_ALIGN(sizeof(struct gz_m4u_msg));
unsigned int _shm_order;
int _num_PA;

TZ_RESULT _reg_shmem(KREE_SESSION_HANDLE mem_sn,
	       KREE_SECUREMEM_HANDLE *mem_hd, KREE_SHAREDMEM_PARAM *shm_param)
{
	int ret;

	ret = KREE_RegisterSharedmem(mem_sn, mem_hd, shm_param);
	if (ret != TZ_RESULT_SUCCESS) {
		M4ULOG_HIGH("[MTEE][%s]KREE_RegisterSharedmem fail(0x%x)\n",
					__func__, ret);
		return ret;
	}
	M4ULOG_HIGH("[MTEE][%s] Done. shmem hd=0x%x\n", __func__, *mem_hd);

	return TZ_RESULT_SUCCESS;
}

TZ_RESULT _unreg_shmem(KREE_SESSION_HANDLE mem_sn, KREE_SECUREMEM_HANDLE mem_hd)
{
	int ret;

	M4ULOG_HIGH("[MTEE]Unreg. Shmem: mem_sn=0x%x, mem_hd=0x%x\n",
				mem_sn, mem_hd);
	ret = KREE_UnregisterSharedmem(mem_sn, mem_hd);
	if (ret != TZ_RESULT_SUCCESS) {
		M4ULOG_HIGH("[MTEE]Unreg. Shmem fail: mem_hd=0x%x(ret=0x%x)\n",
					mem_hd, ret);
		return ret;
	}
	return TZ_RESULT_SUCCESS;
}

TZ_RESULT _prepare_region(KREE_SHAREDMEM_PARAM *shm_param)
{
	uint64_t pa = 0;
	int num_Pa = 0;

	num_Pa = _shm_size/PAGE_SIZE;
	if ((_shm_size % PAGE_SIZE) != 0)
		num_Pa++;

	_num_PA = num_Pa;
	_shm_order = get_order(_shm_size);
	shared_buf = (struct gz_m4u_msg *)__get_free_pages(GFP_KERNEL,
			_shm_order);
	if (!shared_buf) {
		M4ULOG_HIGH("[MTEE][%s] shared_buf alloc Fail.\n", __func__);
		return TZ_RESULT_ERROR_OUT_OF_MEMORY;
	}

	pa = (uint64_t)virt_to_phys((void *)shared_buf);

	M4ULOG_HIGH
	    ("[MTEE][%s]: size=%u, &buf=%llx, PA=%llx, num_Pa=%d, order=%u",
	     __func__, _shm_size, (uint64_t)shared_buf, pa, _num_PA,
	     _shm_order);

	shm_param->buffer = (void *)pa;
	shm_param->size = _shm_size;
	shm_param->region_id = 0;
	shm_param->mapAry = NULL;	/*continuous pages */
	M4ULOG_HIGH
	    ("[MTEE][%s]prepare buf: shm_param->buffer=%llx, size =%d",
	     __func__, shm_param->buffer, shm_param->size);
	return TZ_RESULT_SUCCESS;

}


TZ_RESULT _release_region(void)
{
	if (!shared_buf)
		free_pages((unsigned long)shared_buf, _shm_order);
	return TZ_RESULT_SUCCESS;
}

int m4u_gz_ha_init(struct m4u_gz_sec_context *ctx)
{
	TZ_RESULT ret;
	struct m4u_sec_ty_context *ty_ctx = ctx->imp;

	M4ULOG_HIGH("[MTEE]====> %s runs <====\n", __func__);

	/*prepare test shmem region */
	ret = _prepare_region(&ty_ctx->shm_param);
	if (ret != TZ_RESULT_SUCCESS)
		goto out;


	/*session: for shmem */
	ret = KREE_CreateSession(mem_srv_name, &ty_ctx->mem_sn);
	if (ret != TZ_RESULT_SUCCESS) {
		M4ULOG_HIGH("[MTEE]mem_sn create fail\n");
		goto out_free_buf;
	}

	/*reg shared mem */
	ret = _reg_shmem(ty_ctx->mem_sn, &ty_ctx->mem_hd, &ty_ctx->shm_param);
	if (ret == TZ_RESULT_SUCCESS) {
		ctx->gz_m4u_msg = (struct gz_m4u_msg *)shared_buf;
		}
	else  {
		M4ULOG_HIGH("[MTEE]reg shmem fail\n");
		goto out_create_mem_sn;
		}

	if (!m4u_gz_ta_ctx.gz_m4u_msg) {
		M4ULOG_HIGH("[MTEE]m4u msg is invalid\n");
		return -1;
		}

	if (!ty_ctx->init) {
			/*session: for shmem */
		ret = KREE_CreateSession(m4u_srv_name, &ty_ctx->m4u_sn);
		if (ret != TZ_RESULT_SUCCESS) {
			M4ULOG_HIGH("[MTEE]m4u_sn create fail\n");
			goto out_unreg_shmem;
			}
		ty_ctx->init = 1;
		}

	goto out;

out_unreg_shmem:
	/*unreg shared mem */
	ret = _unreg_shmem(ty_ctx->mem_sn, ty_ctx->mem_hd);

out_create_mem_sn:
	/*close session */
	ret = KREE_CloseSession(ty_ctx->mem_sn);
	if (ret != TZ_RESULT_SUCCESS)
		M4ULOG_HIGH("[MTEE]mem_sn close fail\n");

out_free_buf:
	/*free test shmem region */
	_release_region();

out:
	M4ULOG_HIGH("[MTEE]====> %s ends <====\n", __func__);

	return ret;

}

static int m4u_gz_ha_open(void)
{
	int ret;

	ret = m4u_gz_ha_init(&m4u_gz_ta_ctx);
	return ret;
}

int m4u_gz_sec_context_init(void)
{
	int ret;

	ret = m4u_gz_ha_open();
	if (ret)
		return ret;

	M4ULOG_HIGH("[MTEE]%s:ha open session success\n", __func__);

	return 0;
}

static int m4u_gz_ha_deinit(struct m4u_gz_sec_context *ctx)
{
	struct m4u_sec_ty_context *ty_ctx = ctx->imp;

	KREE_CloseSession(ty_ctx->m4u_sn);
	_unreg_shmem(ty_ctx->mem_sn, ty_ctx->mem_hd);
	KREE_CloseSession(ty_ctx->mem_sn);
	_release_region();
	ty_ctx->init = 0;

	return 0;
}

static int m4u_gz_ha_close(void)
{
	int ret;

	ret = m4u_gz_ha_deinit(&m4u_gz_ta_ctx);
	return ret;
}

int m4u_gz_sec_context_deinit(void)
{
	int ret;

	ret = m4u_gz_ha_close();
	return ret;
}

struct m4u_gz_sec_context *m4u_gz_sec_ctx_get(void)
{
	struct m4u_gz_sec_context *ctx = NULL;
	struct m4u_sec_ty_context *ty_ctx;

	ctx = &m4u_gz_ta_ctx;
	ty_ctx = ctx->imp;
	if (!ty_ctx->init) {
		M4U_ERR("[MTEE]%s: before init\n", __func__);
		return NULL;
	}
	mutex_lock(&ty_ctx->ctx_lock);

	return ctx;
}

void m4u_gz_sec_set_context(void)
{
	m4u_gz_ta_ctx.name = "m4u_ha";
	m4u_gz_ta_ctx.imp = &m4u_ty_ctx;
}

int m4u_gz_sec_ctx_put(struct m4u_gz_sec_context *ctx)
{
	struct m4u_sec_ty_context *ty_ctx = ctx->imp;

	mutex_unlock(&ty_ctx->ctx_lock);

	return 0;
}

static int m4u_gz_exec_session(struct m4u_gz_sec_context *ctx)
{
	int ret;
	struct m4u_sec_ty_context *ty_ctx = ctx->imp;
	union MTEEC_PARAM param[4];
	uint32_t paramTypes;

	if (!ctx->gz_m4u_msg) {
		M4U_MSG("[MTEE]%s TCI/DCI error\n", __func__);
		return -1;
	}
	param[0].value.a = ty_ctx->mem_hd;
	param[1].value.a = _num_PA;
	param[1].value.b = _shm_size;
	paramTypes = TZ_ParamTypes3(TZPT_VALUE_INPUT, TZPT_VALUE_INPUT,
				    TZPT_VALUE_OUTPUT);
	ret = KREE_TeeServiceCall(ty_ctx->m4u_sn, ctx->gz_m4u_msg->cmd,
							  paramTypes, param);
	if (ret != TZ_RESULT_SUCCESS) {
		M4ULOG_HIGH("[MTEE][%s]echo 0x%x fail(0x%x)\n", __func__,
					ctx->gz_m4u_msg->cmd, ret);
		goto exit;
	}
	M4ULOG_HIGH("[MTEE]%s, get_resp %x\n", __func__, ctx->gz_m4u_msg->cmd);
exit:
	return ret;
}

int m4u_gz_exec_cmd(struct m4u_gz_sec_context *ctx)
{
	int ret;

	if (ctx->gz_m4u_msg == NULL) {
		M4U_MSG("[MTEE]%s TCI/DCI error\n", __func__);
		return -1;
	}
	M4ULOG_HIGH("[MTEE]%s, ", __func__);
	ret = m4u_gz_exec_session(ctx);
	if (ret < 0)
		return -1;

	return 0;
}



