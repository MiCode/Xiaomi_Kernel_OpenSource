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

#include "kree/mem.h"
#include <linux/slab.h>
#include "kree/system.h"
#include "tz_cross/trustzone.h"
#include "tz_cross/ta_system.h"
#include "linux/dma-mapping.h"
#include "m4u_sec_gz.h"
#include "m4u_priv.h"

#define mem_srv_name  "com.mediatek.geniezone.srv.mem"
#define m4u_srv_name  "com.mediatek.geniezone.srv.m4u_sec_ha"

static struct m4u_sec_ty_context m4u_ty_ctx = {
	.ctx_lock = __MUTEX_INITIALIZER(m4u_ty_ctx.ctx_lock),
};
struct m4u_gz_sec_context m4u_gz_ta_ctx;
struct gz_m4u_msg *shared_buf;
int _shm_size = PAGE_ALIGN(sizeof(struct gz_m4u_msg));
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
	shared_buf = kmalloc(_shm_size, GFP_KERNEL);
	if (!shared_buf) {
		M4ULOG_HIGH("[MTEE][%s] con_buf kmalloc Fail.\n", __func__);
		return TZ_RESULT_ERROR_OUT_OF_MEMORY;
	}

	pa = (uint64_t)virt_to_phys((void *)shared_buf);

	M4ULOG_HIGH
	    ("[MTEE][%s]: size=%d, &buf=%llx, PA=%llx, num_Pa = %d",
	     __func__, _shm_size, (uint64_t) shared_buf, pa, _num_PA);

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
		kfree(shared_buf);
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
		M4ULOG_HIGH(" %s #%d[MTEE]mem_sn create fail: %s\n",
			    __func__, __LINE__, mem_srv_name);
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
			M4ULOG_HIGH("%s #%d [MTEE]m4u_sn create fail:%s\n",
				    __func__, __LINE__, m4u_srv_name);
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
		M4ULOG_HIGH("[MTEE] err%s: before init\n", __func__);
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
		M4ULOG_HIGH("[MTEE]%s TCI/DCI error\n", __func__);
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
		M4ULOG_HIGH("[MTEE]%s TCI/DCI error\n", __func__);
		return -1;
	}
	M4ULOG_HIGH("[MTEE]%s, ", __func__);
	ret = m4u_gz_exec_session(ctx);
	if (ret < 0)
		return -1;

	return 0;
}


#ifdef M4U_GZ_SERVICE_ENABLE
static DEFINE_MUTEX(gM4u_gz_sec_init);
bool m4u_gz_en[SEC_ID_COUNT];

static int __m4u_gz_sec_init(int mtk_iommu_sec_id)
{
	int ret, i, count = 0;
	unsigned long pt_pa_nonsec = 0;
	struct m4u_gz_sec_context *ctx;
	struct m4u_domain_t *m4u_dom;

	ctx = m4u_gz_sec_ctx_get();
	if (!ctx) {
		ret = -EFAULT;
		goto err;
	}

	for (i = 0; i < SMI_LARB_NR; i++) {
		ret = larb_clock_on(i, 1);
		if (ret) {
			M4ULOG_HIGH("enable larb%d fail, ret:%d\n", i, ret);
			count = i;
			goto out;
		}
	}
	count = SMI_LARB_NR;

	//0 means multimedia
	m4u_dom = m4u_get_domain_by_port(0);
	if (m4u_dom)
		pt_pa_nonsec = m4u_dom->pgd_pa;

	ctx->gz_m4u_msg->cmd = CMD_M4UTY_INIT;
	ctx->gz_m4u_msg->iommu_sec_id = mtk_iommu_sec_id;
	ctx->gz_m4u_msg->init_param.nonsec_pt_pa = pt_pa_nonsec;
	ctx->gz_m4u_msg->init_param.l2_en = 1;
	ctx->gz_m4u_msg->init_param.sec_pt_pa = 0;

	M4ULOG_HIGH("[MTEE]%s: mtk_iommu_sec_id:%d, nonsec_pt_pa: 0x%lx\n",
				__func__, mtk_iommu_sec_id, pt_pa_nonsec);
	ret = m4u_gz_exec_cmd(ctx);
	if (ret) {
		M4ULOG_HIGH("[MTEE] err: m4u exec command fail\n");
		goto out;
	}

out:
	if (count) {
		for (i = 0; i < count; i++)
			larb_clock_off(i, 1);
	}
	m4u_gz_sec_ctx_put(ctx);
err:
	return ret;
}

int m4u_gz_sec_init(int mtk_iommu_sec_id)
{
	int ret;

	M4ULOG_LOW("[MTEE]%s: start\n", __func__);

	mutex_lock(&gM4u_gz_sec_init);

	if (m4u_gz_en[mtk_iommu_sec_id]) {
		m4u_info("wanring: already initialized[%d], init again, will clear pagetable\n",
			 m4u_gz_en[mtk_iommu_sec_id]);
		goto m4u_gz_sec_reinit;
	} else {
		m4u_gz_sec_set_context();
		ret = m4u_gz_sec_context_init();
		if (ret)
			goto err;
	}

m4u_gz_sec_reinit:
	ret = __m4u_gz_sec_init(mtk_iommu_sec_id);
	if (ret < 0) {
		m4u_gz_en[mtk_iommu_sec_id] = 0;
		m4u_gz_sec_context_deinit();
		M4ULOG_HIGH("[MTEE]%s:init fail,ret=0x%x\n", __func__, ret);
		return ret;
	} else {
		m4u_gz_en[mtk_iommu_sec_id] = 1;
	}

	/* don't deinit ta because of multiple init operation */
err:
	mutex_unlock(&gM4u_gz_sec_init);

	M4ULOG_LOW("[MTEE]%s: start\n", __func__);
	return ret;

}

int m4u_map_gz_nonsec_buf(int iommu_sec_id, int port,
			  unsigned long mva, unsigned long size)
{
	int ret;
	struct m4u_gz_sec_context *ctx;

	if ((mva > DMA_BIT_MASK(32)) ||
	    (mva + size > DMA_BIT_MASK(32))) {
		M4ULOG_HIGH("[MTEE]%s invalid mva:0x%lx, size:0x%lx\n",
			__func__, mva, size);
		return -EFAULT;
	}

	ctx = m4u_gz_sec_ctx_get();
	if (!ctx)
		return -EFAULT;

	ctx->gz_m4u_msg->cmd = CMD_M4UTY_MAP_NONSEC_BUFFER;
	ctx->gz_m4u_msg->iommu_sec_id = iommu_sec_id;
	ctx->gz_m4u_msg->buf_param.mva = mva;
	ctx->gz_m4u_msg->buf_param.size = size;
	ctx->gz_m4u_msg->buf_param.port = port;

	ret = m4u_gz_exec_cmd(ctx);
	if (ret) {
		M4ULOG_HIGH("[MTEE] err: m4u exec command fail\n");
		ret = -1;
		goto out;
	}
	ret = ctx->gz_m4u_msg->rsp;

out:
	m4u_gz_sec_ctx_put(ctx);
	return ret;
}


int m4u_unmap_gz_nonsec_buffer(int iommu_sec_id, unsigned long mva,
				unsigned long size)
{
	int ret;
	struct m4u_gz_sec_context *ctx;

	if ((mva > DMA_BIT_MASK(32)) ||
	    (mva + size > DMA_BIT_MASK(32))) {
		M4ULOG_HIGH("[MTEE]%s invalid mva:0x%lx, size:0x%lx\n",
			__func__, mva, size);
		return -EFAULT;
	}

	ctx = m4u_gz_sec_ctx_get();
	if (!ctx)
		return -EFAULT;

	ctx->gz_m4u_msg->cmd = CMD_M4UTY_UNMAP_NONSEC_BUFFER;
	ctx->gz_m4u_msg->iommu_sec_id = iommu_sec_id;
	ctx->gz_m4u_msg->buf_param.mva = mva;
	ctx->gz_m4u_msg->buf_param.size = size;

	ret = m4u_gz_exec_cmd(ctx);
	if (ret) {
		M4ULOG_HIGH("[MTEE] err: m4u exec command fail\n");
		ret = -1;
		goto out;
	}
	ret = ctx->gz_m4u_msg->rsp;

out:
	m4u_gz_sec_ctx_put(ctx);
	return ret;
}

#endif
