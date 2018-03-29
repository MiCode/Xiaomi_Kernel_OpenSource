/*
 * Copyright (c) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */


#include "mtk_mdp_core.h"
#include "mtk_mdp_type.h"
#include "uapi/drm/drm_fourcc.h"


int mtk_mdp_map_color_format(int mtk_mdp_format)
{
	switch (mtk_mdp_format) {
	case DRM_FORMAT_C8:
		return DP_COLOR_BAYER8;
	case DRM_FORMAT_RGB565:
		return DP_COLOR_RGB565;
	case DRM_FORMAT_BGR565:
		return DP_COLOR_BGR565;
	case DRM_FORMAT_RGB888:
		return DP_COLOR_RGB888;
	case DRM_FORMAT_BGR888:
		return DP_COLOR_BGR888;
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_XRGB8888:
		return DP_COLOR_RGBA8888;
	case DRM_FORMAT_ABGR8888:
		return DP_COLOR_BGRA8888;
	case DRM_FORMAT_RGBA8888:
		return DP_COLOR_ARGB8888;
	case DRM_FORMAT_BGRA8888:
		return DP_COLOR_ABGR8888;
	case DRM_FORMAT_YUYV:
		return DP_COLOR_YUYV;
	case DRM_FORMAT_YVYU:
		return DP_COLOR_YVYU;
	case DRM_FORMAT_UYVY:
		return DP_COLOR_UYVY;
	case DRM_FORMAT_VYUY:
		return DP_COLOR_VYUY;
	case DRM_FORMAT_NV12:
		return DP_COLOR_NV12;
	case DRM_FORMAT_NV21:
		return DP_COLOR_NV21;
	case DRM_FORMAT_NV16:
		return DP_COLOR_NV16;
	case DRM_FORMAT_NV61:
		return DP_COLOR_NV61;
	case DRM_FORMAT_NV24:
		return DP_COLOR_NV24;
	case DRM_FORMAT_NV42:
		return DP_COLOR_NV42;
	case DRM_FORMAT_MT21:
		return DP_COLOR_420_BLKP;
	case DRM_FORMAT_YUV420:
		return DP_COLOR_I420;
	case DRM_FORMAT_YVU420:
		return DP_COLOR_YV12;
	case DRM_FORMAT_YUV422:
		return DP_COLOR_I422;
	case DRM_FORMAT_YVU422:
		return DP_COLOR_YV16;
	case DRM_FORMAT_YUV444:
		return DP_COLOR_I444;
	case DRM_FORMAT_YVU444:
		return DP_COLOR_YV24;
	default:
		return DP_COLOR_UNKNOWN;
	}
	return DP_COLOR_UNKNOWN;
}

void mtk_mdp_hw_set_input_addr(struct mtk_mdp_ctx *ctx,
			       struct mtk_mdp_addr *addr)
{
	struct mdp_src_buffer *src_buf = &ctx->vpu.param->src_buffer;

	src_buf->addr_mva[0] = (uint32_t)addr->y;
	src_buf->addr_mva[1] = (uint32_t)addr->cb;
	src_buf->addr_mva[2] = (uint32_t)addr->cr;
}
EXPORT_SYMBOL(mtk_mdp_hw_set_input_addr);

void mtk_mdp_hw_set_output_addr(struct mtk_mdp_ctx *ctx,
				struct mtk_mdp_addr *addr)
{
	struct mdp_dst_buffer *dst_buf = &ctx->vpu.param->dst_buffer;

	dst_buf->addr_mva[0] = (uint32_t)addr->y;
	dst_buf->addr_mva[1] = (uint32_t)addr->cb;
	dst_buf->addr_mva[2] = (uint32_t)addr->cr;
}
EXPORT_SYMBOL(mtk_mdp_hw_set_output_addr);

void mtk_mdp_hw_set_in_size(struct mtk_mdp_ctx *ctx)
{
	struct mtk_mdp_frame *frame = &ctx->s_frame;
	struct mdp_src_config *config = &ctx->vpu.param->src_config;

	/* Set input pixel offset */
	config->crop_x = frame->crop.left;
	config->crop_y = frame->crop.top;

	/* Set input cropped size */
	config->crop_w = frame->crop.width;
	config->crop_h = frame->crop.height;

	/* Set input original size */
	config->x = 0;
	config->y = 0;
	config->w = frame->f_width;
	config->h = frame->f_height;
}
EXPORT_SYMBOL(mtk_mdp_hw_set_in_size);

void mtk_mdp_hw_set_in_image_format(struct mtk_mdp_ctx *ctx)
{
	unsigned int i;
	struct mtk_mdp_frame *frame = &ctx->s_frame;
	struct mdp_src_config *config = &ctx->vpu.param->src_config;
	struct mdp_src_buffer *src_buf = &ctx->vpu.param->src_buffer;

	src_buf->plane_num = frame->fmt->num_planes;
	config->format = mtk_mdp_map_color_format(frame->fmt->pixelformat);
	config->w_stride = 0; /* MDP will calculate it by color format. */
	config->h_stride = 0; /* MDP will calculate it by color format. */

	for (i = 0; i < src_buf->plane_num; i++)
		src_buf->plane_size[i] = frame->payload[i];
}
EXPORT_SYMBOL(mtk_mdp_hw_set_in_image_format);

void mtk_mdp_hw_set_out_size(struct mtk_mdp_ctx *ctx)
{
	struct mtk_mdp_frame *frame = &ctx->d_frame;
	struct mdp_dst_config *config = &ctx->vpu.param->dst_config;

	config->crop_x = frame->crop.left;
	config->crop_y = frame->crop.top;
	config->crop_w = frame->crop.width;
	config->crop_h = frame->crop.height;
	config->x = 0;
	config->y = 0;
	config->w = frame->f_width;
	config->h = frame->f_height;
}
EXPORT_SYMBOL(mtk_mdp_hw_set_out_size);

void mtk_mdp_hw_set_out_image_format(struct mtk_mdp_ctx *ctx)
{
	unsigned int i;
	struct mtk_mdp_frame *frame = &ctx->d_frame;
	struct mdp_dst_config *config = &ctx->vpu.param->dst_config;
	struct mdp_dst_buffer *dst_buf = &ctx->vpu.param->dst_buffer;

	dst_buf->plane_num = frame->fmt->num_planes;
	config->format = mtk_mdp_map_color_format(frame->fmt->pixelformat);
	config->w_stride = 0; /* MDP will calculate it by color format. */
	config->h_stride = 0; /* MDP will calculate it by color format. */
	for (i = 0; i < dst_buf->plane_num; i++)
		dst_buf->plane_size[i] = frame->payload[i];
}
EXPORT_SYMBOL(mtk_mdp_hw_set_out_image_format);

void mtk_mdp_hw_set_rotation(struct mtk_mdp_ctx *ctx)
{
	struct mdp_config_misc *misc = &ctx->vpu.param->misc;

	misc->orientation = ctx->ctrls.rotate->val;
	misc->hflip = ctx->ctrls.hflip->val;
	misc->vflip = ctx->ctrls.vflip->val;
}
EXPORT_SYMBOL(mtk_mdp_hw_set_rotation);

void mtk_mdp_hw_set_global_alpha(struct mtk_mdp_ctx *ctx)
{
	struct mdp_config_misc *misc = &ctx->vpu.param->misc;

	misc->alpha = ctx->ctrls.global_alpha->val;
}
EXPORT_SYMBOL(mtk_mdp_hw_set_global_alpha);

void mtk_mdp_hw_set_sfr_update(struct mtk_mdp_ctx *ctx)
{
	int ret;

	ret = mtk_mdp_vpu_process(&ctx->vpu);
	if (ret < 0)
		pr_err("mtk_mdp_hw_set_sfr_update fail\n");
}
EXPORT_SYMBOL(mtk_mdp_hw_set_sfr_update);

