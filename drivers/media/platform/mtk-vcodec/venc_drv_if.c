// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Daniel Hsiao <daniel.hsiao@mediatek.com>
 *      Jungchang Tsao <jungchang.tsao@mediatek.com>
 *      Tiffany Lin <tiffany.lin@mediatek.com>
 */

#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include "venc_drv_base.h"
#include "venc_drv_if.h"

#include "mtk_vcodec_enc.h"
#include "mtk_vcodec_enc_pm.h"
#include "mtk_vcodec_enc_pm_plat.h"

#if IS_ENABLED(CONFIG_VIDEO_MEDIATEK_VCU)
#include "mtk_vcu.h"
const struct venc_common_if *get_enc_vcu_if(void);
#endif

#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
const struct venc_common_if *get_enc_vcp_if(void);
#endif

static const struct venc_common_if * get_data_path_ptr(void)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
	if (VCU_FPTR(vcu_get_plat_device)) {
		if (mtk_vcodec_vcp & (1 << MTK_INST_ENCODER))
			return get_enc_vcp_if();
		else
			return get_enc_vcu_if();
	} else
		return get_enc_vcp_if();
#endif
#if IS_ENABLED(CONFIG_VIDEO_MEDIATEK_VCU)
	if (VCU_FPTR(vcu_get_plat_device))
		return get_enc_vcu_if();
#endif
	return NULL;
}

int venc_if_init(struct mtk_vcodec_ctx *ctx, unsigned int fourcc)
{
	int ret = 0;

	ctx->oal_vcodec = 0;
	mtk_venc_init_ctx_pm(ctx);

	switch (fourcc) {
	case V4L2_PIX_FMT_H264:
	case V4L2_PIX_FMT_H265:
	case V4L2_PIX_FMT_HEIF:
	case V4L2_PIX_FMT_MPEG4:
	case V4L2_PIX_FMT_H263:
		ctx->enc_if = get_data_path_ptr();
		ctx->oal_vcodec = 0;
		break;
	default:
		return -EINVAL;
	}
	if (ctx->enc_if == NULL)
		return -EINVAL;

	ret = ctx->enc_if->init(ctx, (unsigned long *)&ctx->drv_handle);

	return ret;
}

int venc_if_get_param(struct mtk_vcodec_ctx *ctx, enum venc_get_param_type type,
					  void *out)
{
	struct venc_inst *inst = NULL;
	int ret = 0;
	int drv_handle_exist = 1;

	if (!ctx->drv_handle) {
		inst = kzalloc(sizeof(struct venc_inst), GFP_KERNEL);
		if (!inst)
			return -ENOMEM;
		inst->ctx = ctx;
		ctx->drv_handle = (unsigned long)(inst);
		ctx->enc_if = get_data_path_ptr();
		mtk_vcodec_add_ctx_list(ctx);
		drv_handle_exist = 0;
		mtk_v4l2_debug(0, "%s init drv_handle = 0x%lx",
			__func__, ctx->drv_handle);
	}

	ret = ctx->enc_if->get_param(ctx->drv_handle, type, out);

	if (!drv_handle_exist) {
		mtk_vcodec_del_ctx_list(ctx);
		kfree(inst);
		ctx->drv_handle = 0;
		ctx->enc_if = NULL;
	}

	return ret;
}

int venc_if_set_param(struct mtk_vcodec_ctx *ctx,
	enum venc_set_param_type type, struct venc_enc_param *in)
{
	struct venc_inst *inst = NULL;
	int ret = 0;
	int drv_handle_exist = 1;

	if (!ctx->drv_handle) {
		inst = kzalloc(sizeof(struct venc_inst), GFP_KERNEL);
		if (!inst)
			return -ENOMEM;
		inst->ctx = ctx;
		ctx->drv_handle = (unsigned long)(inst);
		ctx->enc_if = get_data_path_ptr();
		mtk_vcodec_add_ctx_list(ctx);
		drv_handle_exist = 0;
		mtk_v4l2_debug(0, "%s init drv_handle = 0x%lx",
			__func__, ctx->drv_handle);
	}

	ret = ctx->enc_if->set_param(ctx->drv_handle, type, in);

	if (!drv_handle_exist) {
		mtk_vcodec_del_ctx_list(ctx);
		kfree(inst);
		ctx->drv_handle = 0;
		ctx->enc_if = NULL;
	}

	return ret;
}

void venc_encode_prepare(void *ctx_prepare,
	unsigned int core_id, unsigned long *flags)
{
	struct mtk_vcodec_ctx *ctx = (struct mtk_vcodec_ctx *)ctx_prepare;

	if (ctx == NULL || core_id >= MTK_VENC_HW_NUM)
		return;

	mutex_lock(&ctx->hw_status);
	spin_lock_irqsave(&ctx->dev->irqlock, *flags);
	ctx->dev->curr_enc_ctx[core_id] = ctx;
	spin_unlock_irqrestore(&ctx->dev->irqlock, *flags);
	mtk_vcodec_enc_clock_on(ctx, core_id);
	if (!(mtk_vcodec_vcp & (1 << MTK_INST_ENCODER)))
		enable_irq(ctx->dev->enc_irq[core_id]);
	if (core_id == MTK_VENC_CORE_0)
		vcodec_trace_count("VENC_HW_CORE_0", 1);
	else
		vcodec_trace_count("VENC_HW_CORE_1", 1);
	mutex_unlock(&ctx->hw_status);
}

void venc_encode_unprepare(void *ctx_unprepare,
	unsigned int core_id, unsigned long *flags)
{
	struct mtk_vcodec_ctx *ctx = (struct mtk_vcodec_ctx *)ctx_unprepare;

	if (ctx == NULL || core_id >= MTK_VENC_HW_NUM)
		return;

	mutex_lock(&ctx->hw_status);
	if (ctx->dev->enc_sem[core_id].count != 0) {
		mtk_v4l2_debug(0, "HW not prepared, enc_sem[%d].count = %d",
			core_id, ctx->dev->enc_sem[core_id].count);
		mutex_unlock(&ctx->hw_status);
		return;
	}
	if (core_id == MTK_VENC_CORE_0)
		vcodec_trace_count("VENC_HW_CORE_0", 0);
	else
		vcodec_trace_count("VENC_HW_CORE_1", 0);

	if (!(mtk_vcodec_vcp & (1 << MTK_INST_ENCODER)))
		disable_irq(ctx->dev->enc_irq[core_id]);
	mtk_vcodec_enc_clock_off(ctx, core_id);
	spin_lock_irqsave(&ctx->dev->irqlock, *flags);
	ctx->dev->curr_enc_ctx[core_id] = NULL;
	spin_unlock_irqrestore(&ctx->dev->irqlock, *flags);
	mutex_unlock(&ctx->hw_status);
}

void venc_encode_pmqos_gce_begin(void *ctx_begin,
	unsigned int core_id, int job_cnt)
{
	//mtk_venc_pmqos_gce_flush(ctx_begin, core_id, job_cnt);
}

void venc_encode_pmqos_gce_end(void *ctx_end,
	unsigned int core_id, int job_cnt)
{
	//mtk_venc_pmqos_gce_done(ctx_end, core_id, job_cnt);
}

int venc_if_encode(struct mtk_vcodec_ctx *ctx,
	enum venc_start_opt opt, struct venc_frm_buf *frm_buf,
	struct mtk_vcodec_mem *bs_buf,
	struct venc_done_result *result)
{
	int ret = 0;

	if (ctx->drv_handle == 0)
		return 0;

	vcodec_trace_begin("%s", __func__);
	ret = ctx->enc_if->encode(ctx->drv_handle, opt, frm_buf,
							  bs_buf, result);
	vcodec_trace_end();

	return ret;
}

int venc_if_deinit(struct mtk_vcodec_ctx *ctx)
{
	int ret = 0;

	if (ctx->drv_handle == 0)
		return 0;

	ret = ctx->enc_if->deinit(ctx->drv_handle);

	ctx->drv_handle = 0;

	mtk_venc_deinit_ctx_pm(ctx);

	return ret;
}

void venc_check_release_lock(void *ctx_check)
{
	struct mtk_vcodec_ctx *ctx = (struct mtk_vcodec_ctx *)ctx_check;
	unsigned long flags;
	int i;

	for (i = 0; i < MTK_VENC_HW_NUM; i++) {
		if (ctx->core_locked[i] == 1) {
			venc_encode_unprepare(ctx, i, &flags);
			mtk_v4l2_err("[%d] daemon killed when holding lock %d", ctx->id, i);
		}
	}
}

int venc_lock(void *ctx_lock, int core_id, bool sec)
{
	struct mtk_vcodec_ctx *ctx = (struct mtk_vcodec_ctx *)ctx_lock;

	return mtk_venc_lock(ctx, core_id, sec);

}

void venc_unlock(void *ctx_unlock, int core_id)
{
	struct mtk_vcodec_ctx *ctx = (struct mtk_vcodec_ctx *)ctx_unlock;

	mtk_venc_unlock(ctx, core_id);
}

