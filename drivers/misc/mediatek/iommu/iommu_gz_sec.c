// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

#define pr_fmt(fmt)    "mtk_iommu: gz " fmt

#include "iommu_gz_sec.h"
#include "tz_cross/trustzone.h"
#include "tz_cross/ta_system.h"

#define mem_srv_name	"com.mediatek.geniezone.srv.mem"
#define m4u_srv_name	"com.mediatek.geniezone.srv.m4u_sec_ha"

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
		pr_err("[MTEE]%s, KREE_RegisterSharedmem fail(ret=0x%x)\n",
		       __func__, ret);
		return ret;
	}
	pr_info("[MTEE]%s done. shmem hd=0x%x\n", __func__, *mem_hd);

	return TZ_RESULT_SUCCESS;
}

TZ_RESULT _unreg_shmem(KREE_SESSION_HANDLE mem_sn, KREE_SECUREMEM_HANDLE mem_hd)
{
	int ret;

	pr_info("[MTEE]Unreg. Shmem: mem_sn=0x%x, mem_hd=0x%x\n",
		mem_sn, mem_hd);
	ret = KREE_UnregisterSharedmem(mem_sn, mem_hd);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_err("[MTEE]Unreg. Shmem fail: mem_hd=0x%x(ret=0x%x)\n",
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
		pr_err("[MTEE]%s, shared_buf alloc fail\n", __func__);
		return TZ_RESULT_ERROR_OUT_OF_MEMORY;
	}

	pa = (uint64_t)virt_to_phys((void *)shared_buf);
	pr_info("[MTEE]%s: size=%u, num_Pa=%d, order=%u",
		__func__, _shm_size, _num_PA, _shm_order);

	shm_param->buffer = (void *)pa;
	shm_param->size = _shm_size;
	shm_param->region_id = 0;
	shm_param->mapAry = NULL;

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

	pr_info("[MTEE]====> %s runs <====\n", __func__);

	/*prepare test shmem region */
	ret = _prepare_region(&ty_ctx->shm_param);
	if (ret != TZ_RESULT_SUCCESS)
		goto out;


	/*session: for shmem */
	ret = KREE_CreateSession(mem_srv_name, &ty_ctx->mem_sn);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_err("[MTEE]mem_sn create fail\n");
		goto out_free_buf;
	}

	/*reg shared mem */
	ret = _reg_shmem(ty_ctx->mem_sn, &ty_ctx->mem_hd, &ty_ctx->shm_param);
	if (ret == TZ_RESULT_SUCCESS) {
		ctx->gz_m4u_msg = (struct gz_m4u_msg *)shared_buf;
	} else {
		pr_err("[MTEE]reg shmem fail\n");
		goto out_create_mem_sn;
	}

	if (!m4u_gz_ta_ctx.gz_m4u_msg) {
		pr_err("[MTEE]m4u msg is invalid\n");
		return -1;
	}

	if (!ty_ctx->init) {
		/*session: for shmem */
		ret = KREE_CreateSession(m4u_srv_name, &ty_ctx->m4u_sn);
		if (ret != TZ_RESULT_SUCCESS) {
			pr_err("[MTEE]m4u_sn create fail\n");
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
		pr_err("[MTEE]mem_sn close fail\n");

out_free_buf:
	/*free test shmem region */
	_release_region();

out:
	pr_info("[MTEE]====> %s ends <====\n", __func__);

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

	pr_info("[MTEE]%s:ha open session success\n", __func__);

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
		pr_err("[MTEE]%s: before init\n", __func__);
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
	int ret = 0;
	struct m4u_sec_ty_context *ty_ctx = ctx->imp;
	union MTEEC_PARAM param[4];
	uint32_t paramTypes;

	if (!ctx->gz_m4u_msg) {
		pr_err("[MTEE]%s error, gz_m4u_msg is NULL\n", __func__);
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
		pr_err("[MTEE]%s, cmd:0x%x fail(ret=0x%x)\n", __func__,
		       ctx->gz_m4u_msg->cmd, ret);
		goto exit;
	}
	pr_info("[MTEE]%s, cmd:0x%x get_resp\n", __func__,
		ctx->gz_m4u_msg->cmd);

exit:
	return ret;
}

int m4u_gz_exec_cmd(struct m4u_gz_sec_context *ctx)
{
	int ret = 0;

	if (ctx->gz_m4u_msg == NULL) {
		pr_err("[MTEE]%s error, gz_m4u_msg is NULL\n", __func__);
		return -1;
	}

	pr_info("[MTEE]%s, cmd:0x%x\n", __func__, ctx->gz_m4u_msg->cmd);
	ret = m4u_gz_exec_session(ctx);
	if (ret < 0)
		return -1;

	return 0;
}

MODULE_LICENSE("GPL v2");
