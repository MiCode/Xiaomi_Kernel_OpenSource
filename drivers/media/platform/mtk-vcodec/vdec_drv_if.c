// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: PC Chen <pc.chen@mediatek.com>
 *         Tiffany Lin <tiffany.lin@mediatek.com>
 */

#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include "vdec_drv_if.h"
#include "mtk_vcodec_dec.h"
#include "vdec_drv_base.h"
#include "mtk_vcodec_dec_pm.h"
#include "mtk_vcodec_dec_pm_plat.h"

#if IS_ENABLED(CONFIG_VIDEO_MEDIATEK_VCU)
#include "mtk_vcu.h"
const struct vdec_common_if *get_dec_vcu_if(void);
#endif

#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
const struct vdec_common_if *get_dec_vcp_if(void);
#endif

static const struct vdec_common_if * get_data_path_ptr(void)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
	if (VCU_FPTR(vcu_get_plat_device)) {
		if (mtk_vcodec_vcp & (1 << MTK_INST_DECODER))
			return get_dec_vcp_if();
		else
			return get_dec_vcu_if();
	} else
		return get_dec_vcp_if();
#endif
#if IS_ENABLED(CONFIG_VIDEO_MEDIATEK_VCU)
	if (VCU_FPTR(vcu_get_plat_device))
		return get_dec_vcu_if();
#endif
	return NULL;
}

int vdec_if_init(struct mtk_vcodec_ctx *ctx, unsigned int fourcc)
{
	int ret = 0;
	mtk_dec_init_ctx_pm(ctx);

	switch (fourcc) {
	case V4L2_PIX_FMT_H264:
	case V4L2_PIX_FMT_H265:
	case V4L2_PIX_FMT_HEIF:
	case V4L2_PIX_FMT_MPEG1:
	case V4L2_PIX_FMT_MPEG2:
	case V4L2_PIX_FMT_MPEG4:
	case V4L2_PIX_FMT_H263:
	case V4L2_PIX_FMT_VP8:
	case V4L2_PIX_FMT_VP9:
	case V4L2_PIX_FMT_WMV1:
	case V4L2_PIX_FMT_WMV2:
	case V4L2_PIX_FMT_WMV3:
	case V4L2_PIX_FMT_WVC1:
	case V4L2_PIX_FMT_WMVA:
	case V4L2_PIX_FMT_RV30:
	case V4L2_PIX_FMT_RV40:
	case V4L2_PIX_FMT_AV1:
		ctx->dec_if = get_data_path_ptr();
		break;
	default:
		return -EINVAL;
	}

	if (ctx->dec_if == NULL)
		return -EINVAL;

	if (!ctx->user_lock_hw)
		vdec_decode_prepare(ctx, MTK_VDEC_CORE);

	ret = ctx->dec_if->init(ctx, &ctx->drv_handle);

	return ret;
}

int vdec_if_decode(struct mtk_vcodec_ctx *ctx, struct mtk_vcodec_mem *bs,
				   struct vdec_fb *fb, unsigned int *src_chg)
{
	int ret = 0;
	unsigned int i = 0;

	if (bs && !ctx->dec_params.svp_mode) {
		if ((bs->dma_addr & 63UL) != 0UL) {
			mtk_v4l2_err("bs dma_addr should 64 byte align");
			return -EINVAL;
		}
	}

	if (fb && !ctx->dec_params.svp_mode) {
		for (i = 0; i < fb->num_planes; i++) {
			if ((fb->fb_base[i].dma_addr & 511UL) != 0UL) {
				mtk_v4l2_err("fb addr should 512 byte align");
				return -EINVAL;
			}
		}
	}

	if (ctx->drv_handle == 0)
		return -EIO;

	vcodec_trace_begin("%s", __func__);
	ret = ctx->dec_if->decode(ctx->drv_handle, bs, fb, src_chg);
	vcodec_trace_end();

	return ret;
}

int vdec_if_get_param(struct mtk_vcodec_ctx *ctx, enum vdec_get_param_type type,
					  void *out)
{
	struct vdec_inst *inst = NULL;
	int ret = 0;
	int drv_handle_exist = 1;

	if (!ctx->drv_handle) {
		inst = kzalloc(sizeof(struct vdec_inst), GFP_KERNEL);
		if (inst == NULL)
			return -ENOMEM;
		inst->ctx = ctx;
		inst->vcu.ctx = ctx;
		ctx->drv_handle = (unsigned long)(inst);
		ctx->dec_if = get_data_path_ptr();
		mtk_vcodec_add_ctx_list(ctx);
		drv_handle_exist = 0;
	}

	ret = ctx->dec_if->get_param(ctx->drv_handle, type, out);

	if (!drv_handle_exist) {
		inst->vcu.abort = 1;
		mtk_vcodec_del_ctx_list(ctx);
		kfree(inst);
		ctx->drv_handle = 0;
		ctx->dec_if = NULL;
	}

	return ret;
}

int vdec_if_set_param(struct mtk_vcodec_ctx *ctx, enum vdec_set_param_type type,
					  void *in)
{
	struct vdec_inst *inst = NULL;
	int ret = 0;
	int drv_handle_exist = 1;

	if (!ctx->drv_handle) {
		inst = kzalloc(sizeof(struct vdec_inst), GFP_KERNEL);
		if (inst == NULL)
			return -ENOMEM;
		inst->ctx = ctx;
		inst->vcu.ctx = ctx;
		ctx->drv_handle = (unsigned long)(inst);
		ctx->dec_if = get_data_path_ptr();
		mtk_vcodec_add_ctx_list(ctx);
		drv_handle_exist = 0;
	}

	ret = ctx->dec_if->set_param(ctx->drv_handle, type, in);

	if (!drv_handle_exist) {
		mtk_vcodec_del_ctx_list(ctx);
		kfree(inst);
		ctx->drv_handle = 0;
		ctx->dec_if = NULL;
	}

	return ret;
}

void vdec_if_deinit(struct mtk_vcodec_ctx *ctx)
{
	if (ctx->drv_handle == 0)
		return;

	ctx->dec_if->deinit(ctx->drv_handle);

	if (!ctx->user_lock_hw)
		vdec_decode_unprepare(ctx, MTK_VDEC_CORE);

	ctx->drv_handle = 0;
}

void vdec_decode_prepare(void *ctx_prepare,
	unsigned int hw_id)
{
	struct mtk_vcodec_ctx *ctx = (struct mtk_vcodec_ctx *)ctx_prepare;
	int ret;

	if (ctx == NULL || hw_id >= MTK_VDEC_HW_NUM)
		return;

	mutex_lock(&ctx->hw_status);
	ret = mtk_vdec_lock(ctx, hw_id);
	mtk_vcodec_set_curr_ctx(ctx->dev, ctx, hw_id);
	mtk_vcodec_dec_clock_on(&ctx->dev->pm, hw_id);
	if (ret == 0 && !(mtk_vcodec_vcp & (1 << MTK_INST_DECODER)))
		enable_irq(ctx->dev->dec_irq[hw_id]);
	mtk_vdec_dvfs_begin_frame(ctx, hw_id);
	mtk_vdec_pmqos_begin_frame(ctx);
	if (hw_id == MTK_VDEC_CORE)
		vcodec_trace_count("VDEC_HW_CORE", 1);
	else
		vcodec_trace_count("VDEC_HW_LAT", 1);
	mutex_unlock(&ctx->hw_status);
}

void vdec_decode_unprepare(void *ctx_unprepare,
	unsigned int hw_id)
{
	struct mtk_vcodec_ctx *ctx = (struct mtk_vcodec_ctx *)ctx_unprepare;

	if (ctx == NULL || hw_id >= MTK_VDEC_HW_NUM)
		return;

	mutex_lock(&ctx->hw_status);
	mtk_vdec_dvfs_end_frame(ctx, hw_id);
	mtk_vdec_pmqos_end_frame(ctx);
	if (ctx->dev->dec_sem[hw_id].count != 0) {
		mtk_v4l2_debug(0, "HW not prepared, dec_sem[%d].count = %d",
			hw_id, ctx->dev->dec_sem[hw_id].count);
		mutex_unlock(&ctx->hw_status);
		return;
	}
	if (hw_id == MTK_VDEC_CORE)
		vcodec_trace_count("VDEC_HW_CORE", 0);
	else
		vcodec_trace_count("VDEC_HW_LAT", 0);

	if (!(mtk_vcodec_vcp & (1 << MTK_INST_DECODER)))
		disable_irq(ctx->dev->dec_irq[hw_id]);
	mtk_vcodec_dec_clock_off(&ctx->dev->pm, hw_id);
	mtk_vcodec_set_curr_ctx(ctx->dev, NULL, hw_id);
	mtk_vdec_unlock(ctx, hw_id);
	mutex_unlock(&ctx->hw_status);

}

void vdec_check_release_lock(void *ctx_check)
{
	struct mtk_vcodec_ctx *ctx = (struct mtk_vcodec_ctx *)ctx_check;
	int i;

	for (i = 0; i < MTK_VDEC_HW_NUM; i++) {
		if (ctx->hw_locked[i] == 1) {
			vdec_decode_unprepare(ctx, i);
			mtk_v4l2_err("[%d] daemon killed when holding lock %d", ctx->id, i);
		}
	}
}

