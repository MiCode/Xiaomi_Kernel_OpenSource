/* Copyright (c) 2012-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/pm_runtime.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/msm_mdp.h>
#include <linux/memblock.h>
#include <linux/sync.h>
#include <linux/sw_sync.h>
#include <linux/file.h>

#include <soc/qcom/event_timer.h>
#include "mdss.h"
#include "mdss_debug.h"
#include "mdss_fb.h"
#include "mdss_mdp.h"
#include "mdss_mdp_rotator.h"
#include "mdss_mdp_wfd.h"

#define CHECK_LAYER_BOUNDS(offset, size, max_size) \
	(((size) > (max_size)) || ((offset) > ((max_size) - (size))))

#define IS_PIPE_TYPE_CURSOR(pipe_ndx) \
	((pipe_ndx >= (1 << MDSS_MDP_SSPP_CURSOR0)) &&\
	(pipe_ndx <= (1 << MDSS_MDP_SSPP_CURSOR1)))

#define IS_PIPE_TYPE_DMA(pipe_ndx) \
	((pipe_ndx >= (1 << MDSS_MDP_SSPP_DMA0)) &&\
	(pipe_ndx <= (1 << MDSS_MDP_SSPP_DMA1)))

enum {
	MDSS_MDP_RELEASE_FENCE = 0,
	MDSS_MDP_RETIRE_FENCE,
};

enum layer_pipe_q {
	LAYER_USES_NEW_PIPE_Q = 0,
	LAYER_USES_USED_PIPE_Q,
	LAYER_USES_DESTROY_PIPE_Q,
};

static inline bool is_layer_right_blend(struct mdp_rect *left_blend,
	struct mdp_rect *right_blend, u32 left_lm_w)
{
	return ((left_blend->x + left_blend->w) == right_blend->x)	&&
	       ((left_blend->x + left_blend->w) != left_lm_w)		&&
	       (left_blend->y == right_blend->y)			&&
	       (left_blend->h == right_blend->h);
}

static bool is_pipe_type_vig(struct mdss_data_type *mdata, u32 ndx)
{
	u32 i;

	for (i = 0; i < mdata->nvig_pipes; i++) {
		if (mdata->vig_pipes[i].ndx == ndx)
			break;
	}

	return i < mdata->nvig_pipes;
}

static int __async_update_position_check(struct msm_fb_data_type *mfd,
		struct mdss_mdp_pipe *pipe, struct mdp_point *src,
		struct mdp_point *dst)
{
	struct fb_var_screeninfo *var = &mfd->fbi->var;
	u32 xres = var->xres;
	u32 yres = var->yres;

	if (!pipe->async_update
		|| CHECK_LAYER_BOUNDS(src->x, pipe->src.w, pipe->img_width)
		|| CHECK_LAYER_BOUNDS(src->y, pipe->src.h, pipe->img_height)
		|| CHECK_LAYER_BOUNDS(dst->x, pipe->dst.w, xres)
		|| CHECK_LAYER_BOUNDS(dst->y, pipe->dst.h, yres)) {
		pr_err("invalid configs: async_update=%d, src:{%d,%d}, dst:{%d,%d}\n",
			pipe->async_update, src->x, src->y, dst->x, dst->y);
		pr_err("pipe:- src:{%d,%d,%d,%d}, dst:{%d,%d,%d,%d}\n",
			pipe->src.x, pipe->src.y, pipe->src.w, pipe->src.h,
			pipe->dst.x, pipe->dst.y, pipe->dst.w, pipe->dst.h);
		return -EINVAL;
	}
	return 0;
}

static int __cursor_layer_check(struct msm_fb_data_type *mfd,
		struct mdp_input_layer *layer)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	if ((layer->z_order != HW_CURSOR_STAGE(mdata))
			|| layer->src_rect.w > mdata->max_cursor_size
			|| layer->src_rect.h > mdata->max_cursor_size
			|| layer->src_rect.w != layer->dst_rect.w
			|| layer->src_rect.h != layer->dst_rect.h
			|| !mdata->ncursor_pipes) {
		pr_err("Incorrect cursor configs for pipe:%d, cursor_pipes:%d, z_order:%d\n",
				layer->pipe_ndx, mdata->ncursor_pipes,
				layer->z_order);
		pr_err("src:{%d,%d,%d,%d}, dst:{%d,%d,%d,%d}\n",
				layer->src_rect.x, layer->src_rect.y,
				layer->src_rect.w, layer->src_rect.h,
				layer->dst_rect.x, layer->dst_rect.y,
				layer->dst_rect.w, layer->dst_rect.h);
		return -EINVAL;
	}

	return 0;
}

static int __layer_xres_check(struct msm_fb_data_type *mfd,
	struct mdp_input_layer *layer)
{
	u32 xres = 0;
	u32 left_lm_w = left_lm_w_from_mfd(mfd);
	struct mdss_mdp_ctl *ctl = mfd_to_ctl(mfd);
	struct mdss_data_type *mdata = mfd_to_mdata(mfd);

	if (layer->dst_rect.x >= left_lm_w) {
		if (mdata->has_src_split)
			xres = left_lm_w;
		else
			layer->dst_rect.x -= left_lm_w;

		if (ctl->mixer_right) {
			xres += ctl->mixer_right->width;
		} else {
			pr_err("ov cannot be placed on right mixer\n");
			return -EPERM;
		}
	} else {
		if (ctl->mixer_left) {
			xres = ctl->mixer_left->width;
		} else {
			pr_err("ov cannot be placed on left mixer\n");
			return -EPERM;
		}

		if (mdata->has_src_split && ctl->mixer_right)
			xres += ctl->mixer_right->width;
	}

	if (CHECK_LAYER_BOUNDS(layer->dst_rect.x, layer->dst_rect.w, xres)) {
		pr_err("dst_xres is invalid. dst_x:%d, dst_w:%d, xres:%d\n",
			layer->dst_rect.x, layer->dst_rect.w, xres);
		return -EINVAL;
	}

	return 0;
}

static int __layer_param_check(struct msm_fb_data_type *mfd,
	struct mdp_input_layer *layer, struct mdss_mdp_format_params *fmt)
{
	u32 yres;
	u32 min_src_size, min_dst_size = 1;
	int content_secure;
	struct mdss_data_type *mdata = mfd_to_mdata(mfd);
	struct mdss_mdp_ctl *ctl = mfd_to_ctl(mfd);
	u32 src_w, src_h, dst_w, dst_h, width, height;

	if (!ctl) {
		pr_err("ctl is null\n");
		return -EINVAL;
	}

	if (ctl->mixer_left) {
		yres = ctl->mixer_left->height;
	} else {
		pr_debug("Using fb var screen infor for height\n");
		yres = mfd->fbi->var.yres;
	}

	content_secure = (layer->flags & MDP_LAYER_SECURE_SESSION);
	if (!ctl->is_secure && content_secure &&
				 (mfd->panel.type == WRITEBACK_PANEL)) {
		pr_debug("return due to security concerns\n");
		return -EPERM;
	}
	min_src_size = fmt->is_yuv ? 2 : 1;

	if (layer->z_order >= (mdata->max_target_zorder + MDSS_MDP_STAGE_0)) {
		pr_err("zorder %d out of range\n", layer->z_order);
		return -EINVAL;
	}

	if (!mdss_mdp_pipe_search(mdata, layer->pipe_ndx)) {
		pr_err("layer pipe is invalid :%d\n", layer->pipe_ndx);
		return -EINVAL;
	}

	width = layer->buffer.width;
	height = layer->buffer.height;
	if (layer->flags & MDP_LAYER_DEINTERLACE) {
		width *= 2;
		height /= 2;
	}

	if (layer->buffer.width > MAX_IMG_WIDTH ||
	    layer->buffer.height > MAX_IMG_HEIGHT ||
	    layer->src_rect.w < min_src_size ||
	    layer->src_rect.h < min_src_size ||
	    CHECK_LAYER_BOUNDS(layer->src_rect.x, layer->src_rect.w, width) ||
	    CHECK_LAYER_BOUNDS(layer->src_rect.y, layer->src_rect.h, height)) {
		pr_err("invalid source image img flag=%d wh=%dx%d rect=%d,%d,%d,%d\n",
		       layer->flags, width, height,
		       layer->src_rect.x, layer->src_rect.y,
		       layer->src_rect.w, layer->src_rect.h);
		return -EINVAL;
	}

	if (layer->dst_rect.w < min_dst_size ||
		layer->dst_rect.h < min_dst_size) {
		pr_err("invalid destination resolution (%dx%d)",
		       layer->dst_rect.w, layer->dst_rect.h);
		return -EINVAL;
	}

	if (layer->horz_deci || layer->vert_deci) {
		if (!mdata->has_decimation) {
			pr_err("No Decimation in MDP V=%x\n", mdata->mdp_rev);
			return -EINVAL;
		} else if ((layer->horz_deci > MAX_DECIMATION) ||
				(layer->vert_deci > MAX_DECIMATION))  {
			pr_err("Invalid decimation factors horz=%d vert=%d\n",
					layer->horz_deci, layer->vert_deci);
			return -EINVAL;
		} else if (layer->flags & MDP_LAYER_BWC) {
			pr_err("Decimation can't be enabled with BWC\n");
			return -EINVAL;
		} else if (fmt->fetch_mode != MDSS_MDP_FETCH_LINEAR) {
			pr_err("Decimation can't be enabled with MacroTile format\n");
			return -EINVAL;
		}
	}

	if (CHECK_LAYER_BOUNDS(layer->dst_rect.y, layer->dst_rect.h, yres)) {
		pr_err("invalid vertical destination: y=%d, h=%d, yres=%d\n",
			layer->dst_rect.y, layer->dst_rect.h, yres);
		return -EOVERFLOW;
	}

	dst_w = layer->dst_rect.w;
	dst_h = layer->dst_rect.h;

	src_w = layer->src_rect.w >> layer->horz_deci;
	src_h = layer->src_rect.h >> layer->vert_deci;

	if (src_w > mdata->max_mixer_width) {
		pr_err("invalid source width=%d HDec=%d\n",
			layer->src_rect.w, layer->horz_deci);
		return -EINVAL;
	}

	if ((src_w * MAX_UPSCALE_RATIO) < dst_w) {
		pr_err("too much upscaling Width %d->%d\n",
		       layer->src_rect.w, layer->dst_rect.w);
		return -E2BIG;
	}

	if ((src_h * MAX_UPSCALE_RATIO) < dst_h) {
		pr_err("too much upscaling. Height %d->%d\n",
		       layer->src_rect.h, layer->dst_rect.h);
		return -E2BIG;
	}

	if (src_w > (dst_w * MAX_DOWNSCALE_RATIO)) {
		pr_err("too much downscaling. Width %d->%d H Dec=%d\n",
		       src_w, layer->dst_rect.w, layer->horz_deci);
		return -E2BIG;
	}

	if (src_h > (dst_h * MAX_DOWNSCALE_RATIO)) {
		pr_err("too much downscaling. Height %d->%d V Dec=%d\n",
		       src_h, layer->dst_rect.h, layer->vert_deci);
		return -E2BIG;
	}

	if (layer->flags & MDP_LAYER_BWC) {
		if ((layer->buffer.width != layer->src_rect.w) ||
		    (layer->buffer.height != layer->src_rect.h)) {
			pr_err("BWC: mismatch of src img=%dx%d rect=%dx%d\n",
				layer->buffer.width, layer->buffer.height,
				layer->src_rect.w, layer->src_rect.h);
			return -EINVAL;
		}

		if (layer->horz_deci || layer->vert_deci) {
			pr_err("Can't enable BWC decode && decimate\n");
			return -EINVAL;
		}
	}

	if ((layer->flags & MDP_LAYER_DEINTERLACE) &&
		!(layer->flags & MDP_LAYER_ENABLE_PIXEL_EXT)) {
		if (layer->flags & MDP_SOURCE_ROTATED_90) {
			if ((layer->src_rect.w % 4) != 0) {
				pr_err("interlaced rect not h/4\n");
				return -EINVAL;
			}
		} else if ((layer->src_rect.h % 4) != 0) {
			pr_err("interlaced rect not h/4\n");
			return -EINVAL;
		}
	}

	if (fmt->is_yuv) {
		if ((layer->src_rect.x & 0x1) || (layer->src_rect.y & 0x1) ||
		    (layer->src_rect.w & 0x1) || (layer->src_rect.h & 0x1)) {
			pr_err("invalid odd src resolution or coordinates\n");
			return -EINVAL;
		}
	}

	return 0;
}

/* compare all reconfiguration parameter validation in this API */
static int __validate_layer_reconfig(struct mdp_input_layer *layer,
	struct mdss_mdp_pipe *pipe)
{
	int status = 0;
	struct mdss_mdp_format_params *src_fmt;

	/*
	 * csc registers are not double buffered. It is not permitted
	 * to change them on staged pipe with YUV layer.
	 */
	if (pipe->csc_coeff_set != layer->color_space) {
		src_fmt = mdss_mdp_get_format_params(layer->buffer.format);
		if (pipe->src_fmt->is_yuv && src_fmt && src_fmt->is_yuv) {
			status = -EPERM;
			pr_err("csc change is not permitted on used pipe\n");
		}
	}

	return status;
}

static int __validate_single_layer(struct msm_fb_data_type *mfd,
	struct mdp_input_layer *layer, u32 mixer_mux)
{
	u32 bwc_enabled;
	int ret;
	bool is_vig_needed = false;

	struct mdss_mdp_format_params *fmt;
	struct mdss_mdp_mixer *mixer = NULL;
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	struct mdss_data_type *mdata = mfd_to_mdata(mfd);

	if ((layer->dst_rect.w > mdata->max_mixer_width) ||
		(layer->dst_rect.h > MAX_DST_H)) {
		pr_err("exceeded max mixer supported resolution %dx%d\n",
				layer->dst_rect.w, layer->dst_rect.h);
		ret = -EINVAL;
		goto exit_fail;
	}

	pr_debug("ctl=%u mux=%d z_order=%d flags=0x%x dst_x:%d\n",
		mdp5_data->ctl->num, mixer_mux, layer->z_order,
		layer->flags, layer->dst_rect.x);

	fmt = mdss_mdp_get_format_params(layer->buffer.format);
	if (!fmt) {
		pr_err("invalid layer format %d\n", layer->buffer.format);
		ret = -EINVAL;
		goto exit_fail;
	}

	bwc_enabled = layer->flags & MDP_LAYER_BWC;

	if (bwc_enabled) {
		if (!mdp5_data->mdata->has_bwc) {
			pr_err("layer uses bwc format but MDP does not support it\n");
			ret = -EINVAL;
			goto exit_fail;
		}

		layer->buffer.format =
			mdss_mdp_get_rotator_dst_format(
				layer->buffer.format, false, bwc_enabled);
		fmt = mdss_mdp_get_format_params(layer->buffer.format);
		if (!fmt) {
			pr_err("invalid layer format %d\n",
				layer->buffer.format);
			ret = -EINVAL;
			goto exit_fail;
		}
	}

	if (IS_PIPE_TYPE_CURSOR(layer->pipe_ndx)) {
		ret = __cursor_layer_check(mfd, layer);
		if (ret)
			goto exit_fail;
	}

	ret = __layer_xres_check(mfd, layer);
	if (ret)
		goto exit_fail;

	ret = __layer_param_check(mfd, layer, fmt);
	if (ret)
		goto exit_fail;

	mixer = mdss_mdp_mixer_get(mdp5_data->ctl, mixer_mux);
	if (!mixer) {
		pr_err("unable to get %s mixer\n",
			(mixer_mux == MDSS_MDP_MIXER_MUX_RIGHT) ?
			"right" : "left");
		ret = -EPERM;
		goto exit_fail;
	}

	if (fmt->is_yuv || (mdata->has_non_scalar_rgb &&
		((layer->src_rect.w != layer->dst_rect.w) ||
			(layer->src_rect.h != layer->dst_rect.h))))
		is_vig_needed = true;

	if (is_vig_needed && !is_pipe_type_vig(mdata, layer->pipe_ndx)) {
		pr_err("pipe is non-scalar ndx=%x\n", layer->pipe_ndx);
		ret = -EINVAL;
		goto exit_fail;
	}

	if ((IS_PIPE_TYPE_DMA(layer->pipe_ndx) ||
		IS_PIPE_TYPE_CURSOR(layer->pipe_ndx)) &&
		(layer->dst_rect.h != layer->src_rect.h ||
		 layer->dst_rect.w != layer->src_rect.w)) {
		pr_err("no scaling supported on dma/cursor pipe, pipe num:%d\n",
				layer->pipe_ndx);
		return -EINVAL;
	}

exit_fail:
	return ret;
}

static int __configure_pipe_params(struct msm_fb_data_type *mfd,
	struct mdp_input_layer *layer, struct mdss_mdp_pipe *pipe,
	struct mdss_mdp_pipe *left_blend_pipe, bool is_single_layer,
	u32 mixer_mux)
{
	int ret = 0;
	u32 left_lm_w = left_lm_w_from_mfd(mfd);
	u32 flags;

	struct mdss_mdp_mixer *mixer = NULL;
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	struct mdss_data_type *mdata = mfd_to_mdata(mfd);

	mixer = mdss_mdp_mixer_get(mdp5_data->ctl, mixer_mux);
	pipe->src_fmt = mdss_mdp_get_format_params(layer->buffer.format);
	if (!pipe->src_fmt || !mixer) {
		pr_err("invalid layer format:%d or mixer:%p\n",
				layer->buffer.format, pipe->mixer_left);
		ret = -EINVAL;
		goto end;
	}

	pipe->comp_ratio = layer->buffer.comp_ratio;

	if (mfd->panel_orientation)
		layer->flags ^= mfd->panel_orientation;

	pipe->mixer_left = mixer;
	pipe->mfd = mfd;
	pipe->play_cnt = 0;
	pipe->flags = 0;

	if (layer->flags & MDP_LAYER_FLIP_LR)
		pipe->flags = MDP_FLIP_LR;
	if (layer->flags & MDP_LAYER_FLIP_UD)
		pipe->flags |= MDP_FLIP_UD;
	if (layer->flags & MDP_LAYER_SECURE_SESSION)
		pipe->flags |= MDP_SECURE_OVERLAY_SESSION;
	if (layer->flags & MDP_LAYER_SECURE_DISPLAY_SESSION)
		pipe->flags |= MDP_SECURE_DISPLAY_OVERLAY_SESSION;
	if (layer->flags & MDP_LAYER_SOLID_FILL)
		pipe->flags |= MDP_SOLID_FILL;
	if (layer->flags & MDP_LAYER_DEINTERLACE)
		pipe->flags |= MDP_DEINTERLACE;
	if (layer->flags & MDP_LAYER_BWC)
		pipe->flags |= MDP_BWC_EN;
	if (layer->flags & MDP_LAYER_PP)
		pipe->flags |= MDP_OVERLAY_PP_CFG_EN;

	pipe->scale.enable_pxl_ext = layer->flags & MDP_LAYER_ENABLE_PIXEL_EXT;
	pipe->is_fg = layer->flags & MDP_LAYER_FORGROUND;
	pipe->img_width = layer->buffer.width & 0x3fff;
	pipe->img_height = layer->buffer.height & 0x3fff;
	pipe->src.x = layer->src_rect.x;
	pipe->src.y = layer->src_rect.y;
	pipe->src.w = layer->src_rect.w;
	pipe->src.h = layer->src_rect.h;
	pipe->dst.x = layer->dst_rect.x;
	pipe->dst.y = layer->dst_rect.y;
	pipe->dst.w = layer->dst_rect.w;
	pipe->dst.h = layer->dst_rect.h;
	pipe->horz_deci = layer->horz_deci;
	pipe->vert_deci = layer->vert_deci;
	pipe->bg_color = layer->bg_color;
	pipe->alpha = layer->alpha;
	pipe->transp = layer->transp_mask;
	pipe->blend_op = layer->blend_op;
	pipe->is_handed_off = false;
	pipe->async_update = (layer->flags & MDP_LAYER_ASYNC) ? true : false;
	pipe->csc_coeff_set = layer->color_space;

	if (mixer->ctl) {
		pipe->dst.x += mixer->ctl->border_x_off;
		pipe->dst.y += mixer->ctl->border_y_off;
		pr_debug("border{%d,%d}\n", mixer->ctl->border_x_off,
				mixer->ctl->border_y_off);
	}
	pr_debug("src{%d,%d,%d,%d}, dst{%d,%d,%d,%d}\n",
		pipe->src.x, pipe->src.y, pipe->src.w, pipe->src.h,
		pipe->dst.x, pipe->dst.y, pipe->dst.w, pipe->dst.h);

	flags = pipe->flags;
	if (is_single_layer)
		flags |= PERF_CALC_PIPE_SINGLE_LAYER;

	/*
	 * async update is allowed only in video mode panels with single LM
	 * or dual LM with src_split enabled.
	 */
	if (pipe->async_update && ((is_split_lm(mfd) && !mdata->has_src_split)
			|| (!mdp5_data->ctl->is_video_mode))) {
		pr_err("async update allowed only in video mode panel with src_split\n");
		ret = -EINVAL;
		goto end;
	}

	/*
	 * unstage the pipe if it's current z_order does not match with new
	 * z_order because client may only call the validate.
	 */
	if (pipe->mixer_stage != layer->z_order)
		mdss_mdp_mixer_pipe_unstage(pipe, pipe->mixer_left);

	/*
	 * check if overlay span across two mixers and if source split is
	 * available. If yes, enable src_split_req flag so that during mixer
	 * staging, same pipe will be stagged on both layer mixers.
	 */
	if (mdata->has_src_split) {
		if (left_blend_pipe) {
			if (pipe->priority <= left_blend_pipe->priority) {
				pr_err("priority limitation. left:%d right%d\n",
					left_blend_pipe->priority,
					pipe->priority);
				ret = -EPERM;
				goto end;
			} else {
				pr_debug("pipe%d is a right_pipe\n", pipe->num);
				pipe->is_right_blend = true;
			}
		} else if (pipe->is_right_blend) {
			/*
			 * pipe used to be right blend. So need to update mixer
			 * configuration to remove it as a right blend.
			 */
			mdss_mdp_mixer_pipe_unstage(pipe, pipe->mixer_left);
			mdss_mdp_mixer_pipe_unstage(pipe, pipe->mixer_right);
			pipe->is_right_blend = false;
		}

		if (pipe->async_update && is_split_lm(mfd)) {
			pipe->src_split_req = true;
		} else if ((mixer_mux == MDSS_MDP_MIXER_MUX_LEFT) &&
		    ((layer->dst_rect.x + layer->dst_rect.w) > mixer->width)) {
			if (layer->dst_rect.x >= mixer->width) {
				pr_err("%pS: err dst_x can't lie in right half",
					__builtin_return_address(0));
				pr_cont(" flags:0x%x dst x:%d w:%d lm_w:%d\n",
					layer->flags, layer->dst_rect.x,
					layer->dst_rect.w, mixer->width);
				ret = -EINVAL;
				goto end;
			} else {
				pipe->src_split_req = true;
			}
		} else {
			if (pipe->src_split_req) {
				mdss_mdp_mixer_pipe_unstage(pipe,
					pipe->mixer_right);
				pipe->mixer_right = NULL;
			}
			pipe->src_split_req = false;
		}
	}

	pipe->mixer_stage = layer->z_order;

	if (mfd->panel_orientation & MDP_FLIP_LR)
		pipe->dst.x = pipe->mixer_left->width - pipe->dst.x -
			pipe->dst.w;
	if (mfd->panel_orientation & MDP_FLIP_UD)
		pipe->dst.y = pipe->mixer_left->height - pipe->dst.y -
			pipe->dst.h;

	memcpy(&pipe->layer, layer, sizeof(struct mdp_input_layer));

	mdss_mdp_overlay_set_chroma_sample(pipe);

	if (pipe->blend_op == BLEND_OP_NOT_DEFINED)
		pipe->blend_op = pipe->src_fmt->alpha_enable ?
			BLEND_OP_PREMULTIPLIED : BLEND_OP_OPAQUE;

	if (pipe->src_fmt->is_yuv && !(pipe->flags & MDP_SOURCE_ROTATED_90) &&
			!pipe->scale.enable_pxl_ext) {
		pipe->overfetch_disable = OVERFETCH_DISABLE_BOTTOM;

	if (pipe->dst.x >= left_lm_w)
		pipe->overfetch_disable |= OVERFETCH_DISABLE_RIGHT;
		pr_debug("overfetch flags=%x\n", pipe->overfetch_disable);
	} else {
		pipe->overfetch_disable = 0;
	}

	/*
	 * When scaling is enabled src crop and image
	 * width and height is modified by user
	 */
	if ((pipe->flags & MDP_DEINTERLACE) && !pipe->scale.enable_pxl_ext) {
		if (pipe->flags & MDP_SOURCE_ROTATED_90) {
			pipe->src.x = DIV_ROUND_UP(pipe->src.x, 2);
			pipe->src.x &= ~1;
			pipe->src.w /= 2;
			pipe->img_width /= 2;
		} else {
			pipe->src.h /= 2;
			pipe->src.y = DIV_ROUND_UP(pipe->src.y, 2);
			pipe->src.y &= ~1;
		}
	}

	if (layer->flags & MDP_LAYER_ENABLE_PIXEL_EXT)
		memcpy(&pipe->scale, layer->scale,
			sizeof(struct mdp_scale_data));
	ret = mdss_mdp_overlay_setup_scaling(pipe);
	if (ret) {
		pr_err("scaling setup failed %d\n", ret);
		goto end;
	}

	if (layer->flags & MDP_LAYER_PP) {
		memcpy(&pipe->pp_cfg, layer->pp_info,
				sizeof(struct mdp_overlay_pp_params));
		ret = mdss_mdp_pp_sspp_config(pipe);
		if (ret) {
			pr_err("pp setup failed %d\n", ret);
			goto end;
		}
	}

	if (pipe->type == MDSS_MDP_PIPE_TYPE_CURSOR)
		goto end;

	ret = mdp_pipe_tune_perf(pipe, flags);
	if (ret) {
		pr_err("unable to satisfy performance. ret=%d\n", ret);
		goto end;
	}

	ret = mdss_mdp_smp_reserve(pipe);
	if (ret) {
		pr_err("mdss_mdp_smp_reserve failed. pnum:%d ret=%d\n",
			pipe->num, ret);
		goto end;
	}
end:
	return ret;
}

static struct sync_fence *__create_fence(struct msm_fb_data_type *mfd,
	struct msm_sync_pt_data *sync_pt_data, u32 fence_type,
	int *fence_fd, int value)
{
	struct mdss_overlay_private *mdp5_data;
	struct mdss_mdp_ctl *ctl;
	struct sync_fence *sync_fence = NULL;
	char fence_name[32];

	mdp5_data = mfd_to_mdp5_data(mfd);

	ctl = mdp5_data->ctl;
	if (!ctl->ops.add_vsync_handler) {
		pr_err("fb%d vsync pending first update\n", mfd->index);
		return ERR_PTR(-EOPNOTSUPP);
	}

	if (!mdss_mdp_ctl_is_power_on(ctl)) {
		pr_err("fb%d ctl power on failed\n", mfd->index);
		return ERR_PTR(-EPERM);
	}

	if (fence_type == MDSS_MDP_RETIRE_FENCE)
		snprintf(fence_name, sizeof(fence_name), "fb%d_retire",
			mfd->index);
	else
		snprintf(fence_name, sizeof(fence_name), "fb%d_release",
			mfd->index);

	if ((fence_type == MDSS_MDP_RETIRE_FENCE) &&
		(mfd->panel.type == MIPI_CMD_PANEL)) {
		if (mdp5_data->vsync_timeline) {
			value = mdp5_data->vsync_timeline->value + 1 +
				mdp5_data->retire_cnt++;
			sync_fence = mdss_fb_sync_get_fence(
				mdp5_data->vsync_timeline, fence_name, value);
		} else {
			return ERR_PTR(-EPERM);
		}
	} else {
		sync_fence = mdss_fb_sync_get_fence(sync_pt_data->timeline,
			fence_name, value);
	}

	if (IS_ERR_OR_NULL(sync_fence)) {
		pr_err("%s: unable to retrieve release fence\n", fence_name);
		goto end;
	}

	/* get fence fd */
	*fence_fd = get_unused_fd_flags(0);
	if (*fence_fd < 0) {
		pr_err("%s: get_unused_fd_flags failed error:0x%x\n",
			fence_name, *fence_fd);
		sync_fence_put(sync_fence);
		sync_fence = NULL;
		goto end;
	}

	sync_fence_install(sync_fence, *fence_fd);
end:
	return sync_fence;
}

/*
 * __handle_buffer_fences() - copy sync fences and return release/retire
 * fence to caller.
 *
 * This function copies all input sync fences to acquire fence array and
 * returns release/retire fences to caller. It acts like buff_sync ioctl.
 */
static int __handle_buffer_fences(struct msm_fb_data_type *mfd,
	struct mdp_layer_commit_v1 *commit, struct mdp_input_layer *layer_list)
{
	struct sync_fence *fence, *release_fence, *retire_fence;
	struct msm_sync_pt_data *sync_pt_data = NULL;
	struct mdp_input_layer *layer;
	int value;

	u32 acq_fen_count, i, ret = 0;
	u32 layer_count = commit->input_layer_cnt;

	sync_pt_data = &mfd->mdp_sync_pt_data;
	if (!sync_pt_data) {
		pr_err("sync point data are NULL\n");
		return -EINVAL;
	}

	i = mdss_fb_wait_for_fence(sync_pt_data);
	if (i > 0)
		pr_warn("%s: waited on %d active fences\n",
			sync_pt_data->fence_name, i);

	mutex_lock(&sync_pt_data->sync_mutex);
	for (i = 0, acq_fen_count = 0; i < layer_count; i++) {
		layer = &layer_list[i];

		if (layer->buffer.fence < 0)
			continue;

		fence = sync_fence_fdget(layer->buffer.fence);
		if (!fence) {
			pr_err("%s: sync fence get failed! fd=%d\n",
				sync_pt_data->fence_name, layer->buffer.fence);
			ret = -EINVAL;
			break;
		} else {
			sync_pt_data->acq_fen[acq_fen_count++] = fence;
		}
	}
	sync_pt_data->acq_fen_cnt = acq_fen_count;
	if (ret)
		goto sync_fence_err;

	value = sync_pt_data->timeline_value + sync_pt_data->threshold +
			atomic_read(&sync_pt_data->commit_cnt);

	release_fence = __create_fence(mfd, sync_pt_data,
		MDSS_MDP_RELEASE_FENCE, &commit->release_fence, value);
	if (IS_ERR_OR_NULL(release_fence)) {
		pr_err("unable to retrieve release fence\n");
		ret = PTR_ERR(release_fence);
		goto release_fence_err;
	}

	retire_fence = __create_fence(mfd, sync_pt_data,
		MDSS_MDP_RETIRE_FENCE, &commit->retire_fence, value);
	if (IS_ERR_OR_NULL(retire_fence)) {
		pr_err("unable to retrieve retire fence\n");
		ret = PTR_ERR(retire_fence);
		goto retire_fence_err;
	}

	mutex_unlock(&sync_pt_data->sync_mutex);
	return ret;

retire_fence_err:
	put_unused_fd(commit->release_fence);
	sync_fence_put(release_fence);
release_fence_err:
	commit->retire_fence = -1;
	commit->release_fence = -1;
sync_fence_err:
	for (i = 0; i < sync_pt_data->acq_fen_cnt; i++)
		sync_fence_put(sync_pt_data->acq_fen[i]);
	sync_pt_data->acq_fen_cnt = 0;

	mutex_unlock(&sync_pt_data->sync_mutex);

	return ret;
}

/*
 * __map_layer_buffer() - map input layer buffer
 *
 * This function maps input layer buffer. It supports only single layer
 * buffer mapping right now. This is case for all formats including UBWC.
 */
static struct mdss_mdp_data *__map_layer_buffer(struct msm_fb_data_type *mfd,
	struct mdss_mdp_pipe *pipe, struct mdp_input_layer *layer_list,
	u32 layer_count)
{
	struct mdss_mdp_data *src_data;
	struct mdp_input_layer *layer = NULL;
	struct mdp_layer_buffer *buffer;
	struct msmfb_data image;
	int i, ret;
	u32 flags;

	for (i = 0; i < layer_count; i++) {
		layer = &layer_list[i];
		if (layer->pipe_ndx == pipe->ndx)
			break;
	}

	if (i == layer_count) {
		pr_err("layer count index is out of bound\n");
		src_data = ERR_PTR(-EINVAL);
		goto end;
	}

	buffer = &layer->buffer;

	if (pipe->flags & MDP_SOLID_FILL) {
		pr_err("Unexpected buffer queue to a solid fill pipe\n");
		src_data = ERR_PTR(-EINVAL);
		goto end;
	}

	flags = (pipe->flags & (MDP_SECURE_OVERLAY_SESSION |
				MDP_SECURE_DISPLAY_OVERLAY_SESSION));

	/* current implementation only supports one plane mapping */
	if (buffer->planes[0].fd < 0) {
		pr_err("invalid file descriptor for layer buffer\n");
		src_data = ERR_PTR(-EINVAL);
		goto end;
	}

	src_data = mdss_mdp_overlay_buf_alloc(mfd, pipe);
	if (!src_data) {
		pr_err("unable to allocate source buffer\n");
		src_data = ERR_PTR(-ENOMEM);
		goto end;
	}
	memset(&image, 0, sizeof(image));

	image.memory_id = buffer->planes[0].fd;
	image.offset = buffer->planes[0].offset;
	ret = mdss_mdp_data_get_and_validate_size(src_data, &image, 1,
			flags, &mfd->pdev->dev, false, DMA_TO_DEVICE,
			buffer);
	if (ret)
		goto end_buf_free;

	src_data->num_planes = 1;
	return src_data;

end_buf_free:
	mdss_mdp_overlay_buf_free(mfd, src_data);
	src_data = ERR_PTR(ret);
end:
	return src_data;
}

static inline bool __compare_layer_config(struct mdp_input_layer *validate,
	struct mdss_mdp_pipe *pipe)
{
	struct mdp_input_layer *layer = &pipe->layer;
	bool status = true;

	status = !memcmp(&validate->src_rect, &layer->src_rect,
			sizeof(validate->src_rect)) &&
		!memcmp(&validate->dst_rect, &layer->dst_rect,
			sizeof(validate->dst_rect)) &&
		validate->flags == layer->flags &&
		validate->horz_deci == layer->horz_deci &&
		validate->vert_deci == layer->vert_deci &&
		validate->alpha == layer->alpha &&
		validate->color_space == layer->color_space &&
		validate->z_order == (layer->z_order - MDSS_MDP_STAGE_0) &&
		validate->transp_mask == layer->transp_mask &&
		validate->bg_color == layer->bg_color &&
		validate->blend_op == layer->blend_op &&
		validate->buffer.width == layer->buffer.width &&
		validate->buffer.height == layer->buffer.height &&
		validate->buffer.format == layer->buffer.format;

	if (status && (validate->flags & MDP_LAYER_ENABLE_PIXEL_EXT))
		status = !memcmp(validate->scale, &pipe->scale,
			sizeof(pipe->scale));

	return status;
}

/*
 * __find_layer_in_validate_q() - Search layer in validation queue
 *
 * This functions helps to skip validation for layers where only buffer is
 * changing. For ex: video playback case. In order to skip validation, it
 * compares all input layer params except buffer handle, offset, fences.
 */
static struct mdss_mdp_pipe *__find_layer_in_validate_q(
	struct mdp_input_layer *layer,
	struct mdss_overlay_private *mdp5_data)
{
	bool found = false;
	struct mdss_mdp_pipe *pipe, *tmp;

	mutex_lock(&mdp5_data->list_lock);
	list_for_each_entry_safe(pipe, tmp, &mdp5_data->pipes_used, list) {
		if (pipe->ndx == layer->pipe_ndx) {
			if (__compare_layer_config(layer, pipe))
				found = true;
			break;
		}
	}
	mutex_unlock(&mdp5_data->list_lock);

	return found ? pipe : NULL;
}

static bool __find_pipe_in_list(struct list_head *head,
			int pipe_ndx, struct mdss_mdp_pipe **out_pipe)
{
	struct mdss_mdp_pipe *pipe;

	list_for_each_entry(pipe, head, list) {
		if (pipe_ndx == pipe->ndx) {
			*out_pipe = pipe;
			return true;
		}
	}

	return false;
}

/*
 * Search pipe from destroy and cleanup list to avoid validation failure.
 * It is caller responsibility to hold the list lock before calling this API.
 */
static struct mdss_mdp_pipe *__find_and_move_cleanup_pipe(
	struct mdss_overlay_private *mdp5_data, u32 pipe_ndx)
{
	struct mdss_mdp_pipe *pipe = NULL;

	if (__find_pipe_in_list(&mdp5_data->pipes_destroy, pipe_ndx, &pipe)) {
		pr_debug("reuse destroy pipe id:%d ndx:%d\n", pipe->num,
			pipe_ndx);
		list_move(&pipe->list, &mdp5_data->pipes_used);
	} else if (__find_pipe_in_list(&mdp5_data->pipes_cleanup, pipe_ndx,
			&pipe)) {
		pr_debug("reuse cleanup pipe id:%d ndx:%d\n", pipe->num,
			pipe_ndx);
		mdss_mdp_mixer_pipe_unstage(pipe, pipe->mixer_left);
		mdss_mdp_mixer_pipe_unstage(pipe, pipe->mixer_right);
		pipe->mixer_stage = MDSS_MDP_STAGE_UNUSED;
		list_move(&pipe->list, &mdp5_data->pipes_used);
	}

	return pipe;
}

/*
 * __assign_pipe_for_layer() - get a pipe for layer
 *
 * This function first searches the pipe from used list, cleanup list and
 * destroy list. On successful search, it returns the same pipe for current
 * layer. It also un-stage the pipe from current mixer for used, cleanup,
 * destroy pipes if they switches the mixer. On failure search, it returns
 * the null pipe.
 */
static struct mdss_mdp_pipe *__assign_pipe_for_layer(
	struct msm_fb_data_type *mfd,
	struct mdss_mdp_mixer *mixer, u32 pipe_ndx,
	enum layer_pipe_q *pipe_q_type)
{
	struct mdss_mdp_pipe *pipe = NULL;
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	struct mdss_data_type *mdata = mfd_to_mdata(mfd);

	mutex_lock(&mdp5_data->list_lock);
	__find_pipe_in_list(&mdp5_data->pipes_used, pipe_ndx, &pipe);
	if (IS_ERR_OR_NULL(pipe)) {
		pipe = __find_and_move_cleanup_pipe(mdp5_data, pipe_ndx);
		if (IS_ERR_OR_NULL(pipe))
			*pipe_q_type = LAYER_USES_NEW_PIPE_Q;
		else
			*pipe_q_type = LAYER_USES_DESTROY_PIPE_Q;
	} else {
		*pipe_q_type = LAYER_USES_USED_PIPE_Q;
	}
	mutex_unlock(&mdp5_data->list_lock);

	/* found the pipe from used, destroy or cleanup list */
	if (!IS_ERR_OR_NULL(pipe)) {
		if (pipe->mixer_left != mixer) {
			if (!mixer->ctl || (mixer->ctl->mfd != mfd)) {
				pr_err("Can't switch mixer %d->%d pnum %d!\n",
					pipe->mixer_left->num, mixer->num,
						pipe->num);
				pipe = ERR_PTR(-EINVAL);
				goto end;
			}
			pr_debug("switching pipe%d mixer %d->%d\n",
				pipe->num,
				pipe->mixer_left ? pipe->mixer_left->num : -1,
				mixer->num);
			mdss_mdp_mixer_pipe_unstage(pipe, pipe->mixer_left);
			pipe->mixer_left = mixer;
		}
		goto end;
	}

	pipe = mdss_mdp_pipe_assign(mdata, mixer, pipe_ndx);
	if (IS_ERR_OR_NULL(pipe)) {
		pr_err("error reserving pipe. pipe_ndx=0x%x mfd ndx=%d\n",
			pipe_ndx, mfd->index);
		goto end;
	}

	mutex_lock(&mdp5_data->list_lock);
	list_add(&pipe->list, &mdp5_data->pipes_used);
	mutex_unlock(&mdp5_data->list_lock);

end:
	if (!IS_ERR_OR_NULL(pipe)) {
		pipe->dirty = false;
		pipe->params_changed++;
	}
	return pipe;
}

/*
 * __validate_secure_display() - validate secure display
 *
 * This function travers through used pipe list and checks if any pipe
 * is with secure display enabled flag. It fails if client tries to stage
 * unsecure content with secure display session.
 *
 */
static int __validate_secure_display(struct mdss_overlay_private *mdp5_data)
{
	struct mdss_mdp_pipe *pipe, *tmp;
	uint32_t sd_pipes = 0, nonsd_pipes = 0;

	mutex_lock(&mdp5_data->list_lock);
	list_for_each_entry_safe(pipe, tmp, &mdp5_data->pipes_used, list) {
		if (pipe->flags & MDP_SECURE_DISPLAY_OVERLAY_SESSION)
			sd_pipes++;
		else
			nonsd_pipes++;
	}
	mutex_unlock(&mdp5_data->list_lock);

	pr_debug("pipe count:: secure display:%d non-secure:%d\n",
		sd_pipes, nonsd_pipes);

	if ((sd_pipes || mdss_get_sd_client_cnt()) && nonsd_pipes) {
		pr_err("non-secure layer validation request during secure display session\n");
		pr_err(" secure client cnt:%d secure pipe cnt:%d non-secure pipe cnt:%d\n",
			mdss_get_sd_client_cnt(), sd_pipes, nonsd_pipes);
		return -EINVAL;
	} else {
		return 0;
	}
}

/*
 * __handle_free_list() - updates free pipe list
 *
 * This function travers through used pipe list and checks if any pipe
 * is not staged in current validation cycle. It moves the pipe to cleanup
 * list if no layer is attached for that pipe.
 *
 * This should be called after validation is successful for current cycle.
 * Moving pipes before can affects staged pipe for previous cycle.
 */
static void __handle_free_list(struct mdss_overlay_private *mdp5_data,
	struct mdp_input_layer *layer_list, u32 layer_count)
{
	int i;

	struct mdp_input_layer *layer;
	struct mdss_mdp_pipe *pipe, *tmp;

	mutex_lock(&mdp5_data->list_lock);
	list_for_each_entry_safe(pipe, tmp, &mdp5_data->pipes_used, list) {
		for (i = 0; i < layer_count; i++) {
			layer = &layer_list[i];
			if (pipe->ndx == layer->pipe_ndx)
				break;
		}

		/*
		 * if validate cycle is not attaching any layer for this
		 * pipe then move it to cleanup list. It does overlay_unset
		 * task.
		 */
		if (i == layer_count)
			list_move(&pipe->list, &mdp5_data->pipes_cleanup);
	}
	mutex_unlock(&mdp5_data->list_lock);
}

/*
 * __validate_layers() - validate input layers
 * @mfd:	Framebuffer data structure for display
 * @commit:	Commit version-1 structure for display
 *
 * This function validates all input layers present in layer_list. In case
 * of failure, it updates the "error_code" for failed layer. It is possible
 * to find failed layer from layer_list based on "error_code".
 */
static int __validate_layers(struct msm_fb_data_type *mfd,
	struct file *file, struct mdp_layer_commit_v1 *commit)
{
	int ret, i, release_ndx = 0, inputndx = 0, destroy_ndx = 0;
	u32 left_lm_layers = 0, right_lm_layers = 0;
	u32 left_cnt = 0, right_cnt = 0;
	u32 left_lm_w = left_lm_w_from_mfd(mfd);
	u32 mixer_mux, dst_x;
	int layer_count = commit->input_layer_cnt;

	struct mdss_mdp_pipe *pipe, *tmp, *left_blend_pipe;
	struct mdss_mdp_pipe *right_plist[MAX_PIPES_PER_LM] = {0};
	struct mdss_mdp_pipe *left_plist[MAX_PIPES_PER_LM] = {0};
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);

	struct mdss_mdp_mixer *mixer = NULL;
	struct mdp_input_layer *layer, *prev_layer, *layer_list;
	bool is_single_layer = false;
	enum layer_pipe_q pipe_q_type;

	ret = mutex_lock_interruptible(&mdp5_data->ov_lock);
	if (ret)
		return ret;

	layer_list = commit->input_layers;

	for (i = 0; i < layer_count; i++) {
		if (layer_list[i].dst_rect.x >= left_lm_w)
			right_lm_layers++;
		else
			left_lm_layers++;

		if (right_lm_layers >= MAX_PIPES_PER_LM ||
			left_lm_layers >= MAX_PIPES_PER_LM) {
			pr_err("too many pipes stagged mixer left: %d mixer right:%d\n",
				left_lm_layers, right_lm_layers);
			ret = -EINVAL;
			goto end;
		}
		inputndx |= layer_list[i].pipe_ndx;
	}

	for (i = 0; i < layer_count; i++) {
		layer = &layer_list[i];
		dst_x = layer->dst_rect.x;
		left_blend_pipe = NULL;

		prev_layer = (i > 0) ? &layer_list[i - 1] : NULL;
		/*
		 * check if current layer is at same z_order as
		 * previous one, and fail if any or both are async layers,
		 * as async layers should have unique z_order.
		 *
		 * If it has same z_order and qualifies as a right blend,
		 * pass a pointer to the pipe representing previous overlay or
		 * in other terms left blend layer.
		 *
		 * Following logic of selecting left_blend has an inherent
		 * assumption that layer list is sorted on dst_x within a
		 * same z_order.
		 */
		if (prev_layer && (prev_layer->z_order == layer->z_order)) {
			if ((layer->flags & MDP_LAYER_ASYNC)
				|| (prev_layer->flags & MDP_LAYER_ASYNC)) {
				ret = -EINVAL;
				layer->error_code = ret;
				pr_err("async layer should have unique z_order\n");
				goto validate_exit;
			}

			if (is_layer_right_blend(&prev_layer->dst_rect,
				 &layer->dst_rect, left_lm_w))
				left_blend_pipe = pipe;
		}

		if ((layer->dst_rect.x < left_lm_w) ||
				(layer->flags & MDP_LAYER_ASYNC)) {
			is_single_layer = (left_lm_layers == 1);
			mixer_mux = MDSS_MDP_MIXER_MUX_LEFT;
		} else {
			is_single_layer = (right_lm_layers == 1);
			mixer_mux = MDSS_MDP_MIXER_MUX_RIGHT;
		}

		/**
		 * search pipe in current used list to find if parameters
		 * are same. validation can be skipped if only buffer handle
		 * is changed.
		 */
		pipe = __find_layer_in_validate_q(layer, mdp5_data);
		if (pipe) {
			if (mixer_mux == MDSS_MDP_MIXER_MUX_RIGHT)
				right_plist[right_cnt++] = pipe;
			else
				left_plist[left_cnt++] = pipe;

			if (layer->flags & MDP_LAYER_PP) {
				memcpy(&pipe->pp_cfg, layer->pp_info,
					sizeof(struct mdp_overlay_pp_params));
				ret = mdss_mdp_pp_sspp_config(pipe);
				if (ret)
					pr_err("pp setup failed %d\n", ret);
				else
					pipe->params_changed++;
			}
			pipe->dirty = false;
			continue;
		}

		mixer = mdss_mdp_mixer_get(mdp5_data->ctl, mixer_mux);
		if (!mixer) {
			pr_err("unable to get %s mixer\n",
				(mixer_mux == MDSS_MDP_MIXER_MUX_RIGHT) ?
				"right" : "left");
			ret = -EINVAL;
			layer->error_code = ret;
			goto validate_exit;
		}

		layer->z_order += MDSS_MDP_STAGE_0;
		ret = __validate_single_layer(mfd, layer, mixer_mux);
		if (ret) {
			pr_err("layer:%d validation failed ret=%d\n", i, ret);
			layer->error_code = ret;
			goto validate_exit;
		}

		pipe = __assign_pipe_for_layer(mfd, mixer, layer->pipe_ndx,
			&pipe_q_type);
		if (IS_ERR_OR_NULL(pipe)) {
			pr_err("error assigning pipe id=0x%x rc:%ld\n",
				layer->pipe_ndx, PTR_ERR(pipe));
			ret = PTR_ERR(pipe);
			layer->error_code = ret;
			goto validate_exit;
		}

		if (pipe_q_type == LAYER_USES_NEW_PIPE_Q)
			release_ndx |= pipe->ndx;
		if (pipe_q_type == LAYER_USES_DESTROY_PIPE_Q)
			destroy_ndx |= pipe->ndx;

		ret = mdss_mdp_pipe_map(pipe);
		if (IS_ERR_VALUE(ret)) {
			pr_err("Unable to map used pipe%d ndx=%x\n",
				pipe->num, pipe->ndx);
			layer->error_code = ret;
			goto validate_exit;
		}

		if (pipe_q_type == LAYER_USES_USED_PIPE_Q) {
			/*
			 * reconfig is allowed on new/destroy pipes. Only used
			 * pipe needs this extra validation.
			 */
			ret = __validate_layer_reconfig(layer, pipe);
			if (ret) {
				pr_err("layer reconfig validation failed=%d\n",
					ret);
				mdss_mdp_pipe_unmap(pipe);
				layer->error_code = ret;
				goto validate_exit;
			}
		}

		ret = __configure_pipe_params(mfd, layer, pipe,
			left_blend_pipe, is_single_layer, mixer_mux);
		if (ret) {
			pr_err("configure pipe param failed: pipe index= %d\n",
				pipe->ndx);
			mdss_mdp_pipe_unmap(pipe);
			layer->error_code = ret;
			goto validate_exit;
		}

		mdss_mdp_pipe_unmap(pipe);

		/* keep the original copy of dst_x */
		pipe->layer.dst_rect.x = layer->dst_rect.x = dst_x;

		if (mixer_mux == MDSS_MDP_MIXER_MUX_RIGHT)
			right_plist[right_cnt++] = pipe;
		else
			left_plist[left_cnt++] = pipe;

		pr_debug("id:0x%x flags:0x%x dst_x:%d\n",
			layer->pipe_ndx, layer->flags, layer->dst_rect.x);
		layer->z_order -= MDSS_MDP_STAGE_0;
	}

	ret = mdss_mdp_perf_bw_check(mdp5_data->ctl, left_plist, left_cnt,
		right_plist, right_cnt);
	if (ret) {
		pr_err("bw validation check failed: %d\n", ret);
		goto validate_exit;
	}

	__handle_free_list(mdp5_data, layer_list, layer_count);

	ret = __validate_secure_display(mdp5_data);

validate_exit:
	pr_debug("err=%d total_layer:%d left:%d right:%d release_ndx=0x%x destroy_ndx=0x%x processed=%d\n",
		ret, layer_count, left_lm_layers, right_lm_layers,
		release_ndx, destroy_ndx, i);
	MDSS_XLOG(inputndx, layer_count, left_lm_layers, right_lm_layers,
		release_ndx, destroy_ndx, ret);
	mutex_lock(&mdp5_data->list_lock);
	list_for_each_entry_safe(pipe, tmp, &mdp5_data->pipes_used, list) {
		if (IS_ERR_VALUE(ret)) {
			if (pipe->ndx & release_ndx) {
				mdss_mdp_smp_unreserve(pipe);
				pipe->params_changed = 0;
				pipe->dirty = true;
				if (!list_empty(&pipe->list))
					list_del_init(&pipe->list);
				mdss_mdp_pipe_destroy(pipe);
			} else if (pipe->ndx & destroy_ndx) {
				/*
				 * cleanup/destroy list pipes should move back
				 * to destroy list. Next/current kickoff cycle
				 * will release the pipe because validate also
				 * acquires ov_lock.
				 */
				list_move(&pipe->list,
					&mdp5_data->pipes_destroy);
			}
		} else {
			pipe->file = file;
			pr_debug("file pointer attached with pipe is %p\n",
				file);
		}
	}
	mutex_unlock(&mdp5_data->list_lock);
end:
	mutex_unlock(&mdp5_data->ov_lock);

	pr_debug("fb%d validated layers =%d\n", mfd->index, i);

	return ret;
}

/*
 * mdss_mdp_layer_pre_commit() - pre commit validation for input layers
 * @mfd:	Framebuffer data structure for display
 * @commit:	Commit version-1 structure for display
 *
 * This function checks if layers present in commit request are already
 * validated or not. If there is mismatch in validate and commit layers
 * then it validate all input layers again. On successful validation, it
 * maps the input layer buffer and creates release/retire fences.
 *
 * This function is called from client context and can return the error.
 */
int mdss_mdp_layer_pre_commit(struct msm_fb_data_type *mfd,
	struct file *file, struct mdp_layer_commit_v1 *commit)
{
	int ret, i;
	int layer_count = commit->input_layer_cnt;
	bool validate_failed = false;

	struct mdss_mdp_pipe *pipe, *tmp;
	struct mdp_input_layer *layer_list, *layer;
	struct mdss_overlay_private *mdp5_data;
	struct mdss_mdp_data *src_data[MDSS_MDP_MAX_SSPP];

	mdp5_data = mfd_to_mdp5_data(mfd);

	if (!mdp5_data || !mdp5_data->ctl)
		return -EINVAL;

	layer_list = commit->input_layers;

	/* handle null commit */
	if (!layer_count) {
		__handle_free_list(mdp5_data, NULL, layer_count);
		return 0;
	}

	for (i = 0; i < layer_count; i++) {
		layer = &layer_list[i];
		pipe =  __find_layer_in_validate_q(layer, mdp5_data);
		if (!pipe) {
			validate_failed = true;
			break;
		}
	}

	if (validate_failed) {
		ret = __validate_layers(mfd, file, commit);
		if (ret)
			goto end;
	} else {
		/*
		 * move unassigned pipes to cleanup list since commit
		 * supports validate+commit operation.
		 */
		__handle_free_list(mdp5_data, layer_list, layer_count);
	}

	i = 0;

	mutex_lock(&mdp5_data->list_lock);
	list_for_each_entry_safe(pipe, tmp, &mdp5_data->pipes_used, list) {
		if (pipe->flags & MDP_SOLID_FILL) {
			src_data[i] = NULL;
			continue;
		}
		src_data[i] = __map_layer_buffer(mfd, pipe, layer_list,
			layer_count);
		if (IS_ERR_OR_NULL(src_data[i++])) {
			i--;
			mutex_unlock(&mdp5_data->list_lock);
			ret =  PTR_ERR(src_data[i]);
			goto map_err;
		}
	}
	mutex_unlock(&mdp5_data->list_lock);

	ret = mdss_mdp_overlay_start(mfd);
	if (ret) {
		pr_err("unable to start overlay %d (%d)\n", mfd->index, ret);
		goto map_err;
	}

	ret = __handle_buffer_fences(mfd, commit, layer_list);

map_err:
	if (ret) {
		mutex_lock(&mdp5_data->list_lock);
		for (i--; i >= 0; i--)
			if (src_data[i])
				mdss_mdp_overlay_buf_free(mfd, src_data[i]);
		mutex_unlock(&mdp5_data->list_lock);
	}
end:

	return ret;
}

/*
 * mdss_mdp_layer_atomic_validate() - validate input layers
 * @mfd:	Framebuffer data structure for display
 * @commit:	Commit version-1 structure for display
 *
 * This function validates only input layers received from client. It
 * does perform any validation for mdp_output_layer defined for writeback
 * display.
 */
int mdss_mdp_layer_atomic_validate(struct msm_fb_data_type *mfd,
	struct file *file, struct mdp_layer_commit_v1 *commit)
{
	struct mdss_overlay_private *mdp5_data;

	if (!mfd || !commit) {
		pr_err("invalid input params\n");
		return -EINVAL;
	}

	mdp5_data = mfd_to_mdp5_data(mfd);

	if (!mdp5_data || !mdp5_data->ctl) {
		pr_err("invalid input params\n");
		return -ENODEV;
	}

	if (mdss_fb_is_power_off(mfd)) {
		pr_err("display interface is in off state fb:%d\n",
			mfd->index);
		return -EPERM;
	}

	return __validate_layers(mfd, file, commit);
}

int mdss_mdp_layer_pre_commit_wfd(struct msm_fb_data_type *mfd,
	struct file *file, struct mdp_layer_commit_v1 *commit)
{
	int rc, count;
	struct mdss_overlay_private *mdp5_data;
	struct mdss_mdp_wfd *wfd = NULL;
	struct mdp_output_layer *output_layer = NULL;
	struct mdss_mdp_wfd_data *data = NULL;
	struct sync_fence *fence = NULL;
	struct msm_sync_pt_data *sync_pt_data = NULL;

	if (!mfd || !commit)
		return -EINVAL;

	mdp5_data = mfd_to_mdp5_data(mfd);

	if (!mdp5_data || !mdp5_data->ctl || !mdp5_data->wfd) {
		pr_err("invalid wfd state\n");
		return -ENODEV;
	}

	if (commit->output_layer) {
		wfd = mdp5_data->wfd;
		output_layer = commit->output_layer;

		data = mdss_mdp_wfd_add_data(wfd, output_layer);
		if (IS_ERR_OR_NULL(data))
			return PTR_ERR(data);

		if (output_layer->buffer.fence >= 0) {
			fence = sync_fence_fdget(output_layer->buffer.fence);
			if (!fence) {
				pr_err("fail to get output buffer fence\n");
				rc = -EINVAL;
				goto fence_get_err;
			}
		}
	} else {
		wfd = mdp5_data->wfd;
		if (!wfd->ctl || !wfd->ctl->wb) {
			pr_err("wfd commit with null out layer and no validate\n");
			return -EINVAL;
		}
	}

	rc = mdss_mdp_layer_pre_commit(mfd, file, commit);
	if (rc) {
		pr_err("fail to import input layer buffers\n");
		goto input_layer_err;
	}

	if (fence) {
		sync_pt_data = &mfd->mdp_sync_pt_data;
		mutex_lock(&sync_pt_data->sync_mutex);
		count = sync_pt_data->acq_fen_cnt;
		sync_pt_data->acq_fen[count] = fence;
		sync_pt_data->acq_fen_cnt++;
		mutex_unlock(&sync_pt_data->sync_mutex);
	}
	return rc;

input_layer_err:
	if (fence)
		sync_fence_put(fence);
fence_get_err:
	if (data)
		mdss_mdp_wfd_remove_data(wfd, data);
	return rc;
}

int mdss_mdp_layer_atomic_validate_wfd(struct msm_fb_data_type *mfd,
	struct file *file, struct mdp_layer_commit_v1 *commit)
{
	int rc = 0;
	struct mdss_overlay_private *mdp5_data;
	struct mdss_mdp_wfd *wfd;
	struct mdp_output_layer *output_layer;

	if (!mfd || !commit)
		return -EINVAL;

	mdp5_data = mfd_to_mdp5_data(mfd);

	if (!mdp5_data || !mdp5_data->ctl || !mdp5_data->wfd) {
		pr_err("invalid wfd state\n");
		return -ENODEV;
	}

	if (!commit->output_layer) {
		pr_err("no output layer defined\n");
		return -EINVAL;
	}

	wfd = mdp5_data->wfd;
	output_layer = commit->output_layer;

	rc = mdss_mdp_wfd_validate(wfd, output_layer);
	if (rc) {
		pr_err("fail to validate the output layer = %d\n", rc);
		goto validate_failed;
	}

	rc = mdss_mdp_wfd_setup(wfd, output_layer);
	if (rc) {
		pr_err("fail to prepare wfd = %d\n", rc);
		goto validate_failed;
	}

	rc = mdss_mdp_layer_atomic_validate(mfd, file, commit);
	if (rc) {
		pr_err("fail to validate the input layers = %d\n", rc);
		goto validate_failed;
	}

validate_failed:
	return rc;
}

int mdss_mdp_async_position_update(struct msm_fb_data_type *mfd,
		struct mdp_position_update *update_pos)
{
	int i, rc = 0;
	struct mdss_mdp_pipe *pipe = NULL;
	struct mdp_async_layer *layer;
	struct mdss_rect dst, src;
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	u32 flush_bits = 0, inputndx = 0;

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);

	for (i = 0; i < update_pos->input_layer_cnt; i++) {
		layer = &update_pos->input_layers[i];
		mutex_lock(&mdp5_data->list_lock);
		__find_pipe_in_list(&mdp5_data->pipes_used, layer->pipe_ndx,
			&pipe);
		mutex_unlock(&mdp5_data->list_lock);
		if (!pipe) {
			pr_err("invalid pipe ndx=0x%x for async update\n",
					layer->pipe_ndx);
			rc = -ENODEV;
			layer->error_code = rc;
			goto done;
		}

		rc =  __async_update_position_check(mfd, pipe, &layer->src,
				&layer->dst);
		if (rc) {
			layer->error_code = rc;
			goto done;
		}

		src = (struct mdss_rect) {layer->src.x, layer->src.y,
				pipe->src.w, pipe->src.h};
		dst = (struct mdss_rect) {layer->dst.x, layer->dst.y,
				pipe->src.w, pipe->src.h};

		pr_debug("src:{%d,%d,%d,%d}, dst:{%d,%d,%d,%d}\n",
				src.x, src.y, src.w, src.h,
				dst.x, dst.y, dst.w, dst.h);

		mdss_mdp_pipe_position_update(pipe, &src, &dst);

		flush_bits |= mdss_mdp_get_pipe_flush_bits(pipe);
		inputndx |= layer->pipe_ndx;
	}
	mdss_mdp_async_ctl_flush(mfd, flush_bits);

done:
	MDSS_XLOG(inputndx, update_pos->input_layer_cnt, flush_bits, rc);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
	return rc;
}

