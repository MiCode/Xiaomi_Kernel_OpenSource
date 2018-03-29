/*
 * Copyright (c) 2015 MediaTek Inc.
 * Author: PC Chen <pc.chen@mediatek.com>
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

#include "vdec_drv_base.h"
#include "vdec_drv_if.h"

#include "mtk_vcodec_pm.h"
#include "mtk_vcodec_util.h"
#include "mtk_vpu.h"

#include "h264_dec/vdec_h264_if.h"
#include "vp8_dec/vdec_vp8_if.h"
#include "vp9_dec/vdec_vp9_if.h"
#include "mpeg4_dec/vdec_mpeg4_if.h"


int vdec_if_create(struct mtk_vcodec_ctx *ctx, unsigned int fourcc)
{

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
	case V4L2_PIX_FMT_MPEG4:
		ctx->dec_if = get_mpeg4_dec_comm_if();
		break;
	default:
		goto err_out;
	}

	return 0;

err_out:
	return -EINVAL;
}

int vdec_if_init(struct mtk_vcodec_ctx *ctx, struct mtk_vcodec_mem *bs,
		 struct vdec_pic_info *pic)
{
	int ret = 0;

	if (ctx->dec_if == NULL)
		return -EINVAL;

	mtk_vdec_lock(ctx);
	mtk_vcodec_dec_clock_on(&ctx->dev->pm);
	ret = ctx->dec_if->init(ctx, bs, &ctx->drv_handle, pic);
	mtk_vcodec_dec_clock_off(&ctx->dev->pm);
	mtk_vdec_unlock(ctx);

	return ret;
}

int vdec_if_decode(struct mtk_vcodec_ctx *ctx, struct mtk_vcodec_mem *bs,
		   struct vdec_fb *fb, bool *res_chg)
{
	int ret = 0;

	if (bs) {
		if ((bs->dma_addr & 63) != 0)
			return -EINVAL;

		if (((fb->base_y.dma_addr & 511) != 0) ||
		    ((fb->base_c.dma_addr & 511) != 0))
			return -EINVAL;
		}

	mtk_vdec_lock(ctx);
	mtk_vcodec_dec_clock_on(&ctx->dev->pm);
	enable_irq(ctx->dev->dec_irq);
	ret = ctx->dec_if->decode(ctx->drv_handle, bs, fb, res_chg);
	disable_irq(ctx->dev->dec_irq);
	mtk_vcodec_dec_clock_off(&ctx->dev->pm);
	mtk_vdec_unlock(ctx);

	return ret;
}

int vdec_if_get_param(struct mtk_vcodec_ctx *ctx, enum vdec_get_param_type type,
		      void *out)
{
	int ret = 0;

	mtk_vdec_lock(ctx);
	ret = ctx->dec_if->get_param(ctx->drv_handle, type, out);
	mtk_vdec_unlock(ctx);

	return ret;
}

int vdec_if_release(struct mtk_vcodec_ctx *ctx)
{
	int ret = 0;

	if (ctx->drv_handle == 0)
		return 0;

	mtk_vdec_lock(ctx);
	mtk_vcodec_dec_clock_on(&ctx->dev->pm);
	ret = ctx->dec_if->deinit(ctx->drv_handle);
	mtk_vcodec_dec_clock_off(&ctx->dev->pm);
	mtk_vdec_unlock(ctx);

	ctx->drv_handle = 0;
	return ret;
}

