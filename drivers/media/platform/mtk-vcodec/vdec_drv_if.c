/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: PC Chen <pc.chen@mediatek.com>
 *         Tiffany Lin <tiffany.lin@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include "vdec_drv_if.h"
#include "mtk_vcodec_dec.h"
#include "vdec_drv_base.h"
#include "mtk_vcodec_dec_pm.h"

#ifdef CONFIG_VIDEO_MEDIATEK_VCU
#include "mtk_vcu.h"
const struct vdec_common_if *get_dec_common_if(void);
#endif

#ifdef CONFIG_VIDEO_MEDIATEK_VPU
#include "mtk_vpu.h"
const struct vdec_common_if *get_h264_dec_comm_if(void);
const struct vdec_common_if *get_vp8_dec_comm_if(void);
const struct vdec_common_if *get_vp9_dec_comm_if(void);
#endif

int vdec_if_init(struct mtk_vcodec_ctx *ctx, unsigned int fourcc)
{
	int ret = 0;
	mtk_dec_init_ctx_pm(ctx);

#ifdef CONFIG_VIDEO_MEDIATEK_VCU
	switch (fourcc) {
	case V4L2_PIX_FMT_H264:
	case V4L2_PIX_FMT_H265:
	case V4L2_PIX_FMT_HEIF:
	case V4L2_PIX_FMT_MPEG1:
	case V4L2_PIX_FMT_MPEG2:
	case V4L2_PIX_FMT_MPEG4:
	case V4L2_PIX_FMT_H263:
	case V4L2_PIX_FMT_S263:
	case V4L2_PIX_FMT_XVID:
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
		ctx->dec_if = get_dec_common_if();
		break;
	default:
		return -EINVAL;
	}
#endif
#ifdef CONFIG_VIDEO_MEDIATEK_VPU
	switch (fourcc) {
	case V4L2_PIX_FMT_H264:
		ctx->dec_if = get_h264_dec_comm_if();
		break;
	case V4L2_PIX_FMT_VP8:
		ctx->dec_if = get_vp8_dec_comm_if();
		break;
	case V4L2_PIX_FMT_VP9:
		ctx->dec_if = get_vp9_dec_comm_if();
		break;
	default:
		return -EINVAL;
	}
#endif
	if (!ctx->user_lock_hw) {
		mtk_vdec_lock(ctx, MTK_VDEC_CORE);
		mtk_vcodec_dec_clock_on(&ctx->dev->pm, MTK_VDEC_CORE);
	}
	ret = ctx->dec_if->init(ctx, &ctx->drv_handle);
	if (!ctx->user_lock_hw) {
		mtk_vcodec_dec_clock_off(&ctx->dev->pm, MTK_VDEC_CORE);
		mtk_vdec_unlock(ctx, MTK_VDEC_CORE);
	}

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
	if (!ctx->user_lock_hw)
		vdec_decode_prepare(ctx, MTK_VDEC_CORE);

	ret = ctx->dec_if->decode(ctx->drv_handle, bs, fb, src_chg);

	if (!ctx->user_lock_hw)
		vdec_decode_unprepare(ctx, MTK_VDEC_CORE);

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
		ctx->drv_handle = (unsigned long)(inst);
		ctx->dec_if = get_dec_common_if();
		drv_handle_exist = 0;
	}

	ret = ctx->dec_if->get_param(ctx->drv_handle, type, out);

	if (!drv_handle_exist) {
		kfree(inst);
		ctx->drv_handle = 0;
		ctx->dec_if = NULL;
	}

	return ret;
}

int vdec_if_set_param(struct mtk_vcodec_ctx *ctx, enum vdec_set_param_type type,
					  void *in)
{
	int ret = 0;

	if (ctx->drv_handle == 0)
		return -EIO;

	ret = ctx->dec_if->set_param(ctx->drv_handle, type, in);

	return ret;
}

void vdec_if_deinit(struct mtk_vcodec_ctx *ctx)
{
	if (ctx->drv_handle == 0)
		return;
	if (!ctx->user_lock_hw)
		vdec_decode_prepare(ctx, MTK_VDEC_CORE);

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

	mtk_vdec_pmqos_prelock(ctx, hw_id);
	ret = mtk_vdec_lock(ctx, hw_id);

	mtk_vcodec_set_curr_ctx(ctx->dev, ctx, hw_id);
	mtk_vcodec_dec_clock_on(&ctx->dev->pm, hw_id);
	if (ret == 0)
		enable_irq(ctx->dev->dec_irq[hw_id]);

	mtk_vdec_pmqos_begin_frame(ctx, hw_id);
}
EXPORT_SYMBOL_GPL(vdec_decode_prepare);

void vdec_decode_unprepare(void *ctx_unprepare,
	unsigned int hw_id)
{
	struct mtk_vcodec_ctx *ctx = (struct mtk_vcodec_ctx *)ctx_unprepare;

	if (ctx == NULL || hw_id >= MTK_VDEC_HW_NUM)
		return;

	if (ctx->dev->dec_sem[hw_id].count != 0) {
		mtk_v4l2_err("HW not prepared, dec_sem[%d].count = %d",
			hw_id, ctx->dev->dec_sem[hw_id].count);
		return;
	}
	mtk_vdec_pmqos_end_frame(ctx, hw_id);

	disable_irq(ctx->dev->dec_irq[hw_id]);
	mtk_vcodec_dec_clock_off(&ctx->dev->pm, hw_id);
	mtk_vcodec_set_curr_ctx(ctx->dev, NULL, hw_id);

	mtk_vdec_unlock(ctx, hw_id);
}
EXPORT_SYMBOL_GPL(vdec_decode_unprepare);

