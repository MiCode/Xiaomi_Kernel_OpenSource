/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Daniel Hsiao <daniel.hsiao@mediatek.com>
 *      Jungchang Tsao <jungchang.tsao@mediatek.com>
 *      Tiffany Lin <tiffany.lin@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify
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

#include "venc_drv_base.h"
#include "venc_drv_if.h"

#include "mtk_vcodec_enc.h"
#include "mtk_vcodec_enc_pm.h"

#ifdef CONFIG_VIDEO_MEDIATEK_VCU
#include "mtk_vcu.h"
const struct venc_common_if *get_enc_common_if(void);
#endif

#ifdef CONFIG_VIDEO_MEDIATEK_VPU
#include "mtk_vpu.h"
const struct venc_common_if *get_h264_enc_comm_if(void);
const struct venc_common_if *get_vp8_enc_comm_if(void);
#endif

int venc_if_init(struct mtk_vcodec_ctx *ctx, unsigned int fourcc)
{
	int ret = 0;

	ctx->oal_vcodec = 0;
	ctx->slowmotion = 0;

#ifdef CONFIG_VIDEO_MEDIATEK_VCU
	switch (fourcc) {
	case V4L2_PIX_FMT_H264:
	case V4L2_PIX_FMT_H265:
	case V4L2_PIX_FMT_HEIF:
	case V4L2_PIX_FMT_MPEG4:
	case V4L2_PIX_FMT_H263:
		ctx->enc_if = get_enc_common_if();
		ctx->oal_vcodec = 0;
		break;
	default:
		return -EINVAL;
	}
#endif
#ifdef CONFIG_VIDEO_MEDIATEK_VPU
	switch (fourcc) {
	case V4L2_PIX_FMT_VP8:
		ctx->enc_if = get_vp8_enc_comm_if();
		break;
	case V4L2_PIX_FMT_H264:
		ctx->enc_if = get_h264_enc_comm_if();
		break;
	default:
		return -EINVAL;
	}
#endif

	if (ctx->oal_vcodec == 0) {
		mtk_venc_lock(ctx);
		mtk_vcodec_enc_clock_on(&ctx->dev->pm);
	}
	ret = ctx->enc_if->init(ctx, (unsigned long *)&ctx->drv_handle);

	if (ctx->oal_vcodec == 0) {
		mtk_vcodec_enc_clock_off(&ctx->dev->pm);
		mtk_venc_unlock(ctx);
	}
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
		inst->ctx = ctx;
		ctx->drv_handle = (unsigned long)(inst);
		ctx->enc_if = get_enc_common_if();
		drv_handle_exist = 0;
	}

	if (ctx->slowmotion == 0)
		mtk_venc_lock(ctx);
	ret = ctx->enc_if->get_param(ctx->drv_handle, type, out);
	if (ctx->slowmotion == 0)
		mtk_venc_unlock(ctx);

	if (!drv_handle_exist) {
		kfree(inst);
		ctx->drv_handle = 0;
		ctx->enc_if = NULL;
	}

	return ret;
}

int venc_if_set_param(struct mtk_vcodec_ctx *ctx,
	enum venc_set_param_type type, struct venc_enc_param *in)
{
	int ret = 0;

	if (ctx->drv_handle == 0)
		return -EIO;

	if (ctx->oal_vcodec == 0) {
		mtk_venc_lock(ctx);
		mtk_vcodec_enc_clock_on(&ctx->dev->pm);
	}
	ret = ctx->enc_if->set_param(ctx->drv_handle, type, in);
	if (ctx->oal_vcodec == 0) {
		mtk_vcodec_enc_clock_off(&ctx->dev->pm);
		mtk_venc_unlock(ctx);
	}
	return ret;
}

void venc_encode_prepare(void *ctx_prepare, unsigned long *flags)
{
	struct mtk_vcodec_ctx *ctx = (struct mtk_vcodec_ctx *)ctx_prepare;

	mtk_venc_pmqos_prelock(ctx);
	mtk_venc_lock(ctx);
	spin_lock_irqsave(&ctx->dev->irqlock, *flags);
	ctx->dev->curr_ctx = ctx;
	spin_unlock_irqrestore(&ctx->dev->irqlock, *flags);
	mtk_vcodec_enc_clock_on(&ctx->dev->pm);
	enable_irq(ctx->dev->enc_irq);
	mtk_venc_pmqos_begin_frame(ctx);
}
EXPORT_SYMBOL_GPL(venc_encode_prepare);

void venc_encode_unprepare(void *ctx_unprepare, unsigned long *flags)
{
	struct mtk_vcodec_ctx *ctx = (struct mtk_vcodec_ctx *)ctx_unprepare;

	mtk_venc_pmqos_end_frame(ctx);
	disable_irq(ctx->dev->enc_irq);
	mtk_vcodec_enc_clock_off(&ctx->dev->pm);
	spin_lock_irqsave(&ctx->dev->irqlock, *flags);
	ctx->dev->curr_ctx = NULL;
	spin_unlock_irqrestore(&ctx->dev->irqlock, *flags);
	mtk_venc_unlock(ctx);
}
EXPORT_SYMBOL_GPL(venc_encode_unprepare);

int venc_if_encode(struct mtk_vcodec_ctx *ctx,
	enum venc_start_opt opt, struct venc_frm_buf *frm_buf,
	struct mtk_vcodec_mem *bs_buf,
	struct venc_done_result *result)
{
	int ret = 0;
	unsigned long flags;

	if (ctx->drv_handle == 0)
		return -EIO;

	if (ctx->oal_vcodec == 0 && ctx->slowmotion == 0)
		venc_encode_prepare(ctx, &flags);

	ret = ctx->enc_if->encode(ctx->drv_handle, opt, frm_buf,
							  bs_buf, result);

	if (ctx->oal_vcodec == 0 && ctx->slowmotion == 0)
		venc_encode_unprepare(ctx, &flags);

	return ret;
}

int venc_if_deinit(struct mtk_vcodec_ctx *ctx)
{
	int ret = 0;
	unsigned long flags;

	if (ctx->drv_handle == 0)
		return -EIO;

	if (ctx->oal_vcodec == 0 && ctx->slowmotion == 0)
		venc_encode_prepare(ctx, &flags);

	ret = ctx->enc_if->deinit(ctx->drv_handle);

	if (ctx->oal_vcodec == 0 && ctx->slowmotion == 0)
		venc_encode_unprepare(ctx, &flags);

	ctx->drv_handle = 0;

	return ret;
}
