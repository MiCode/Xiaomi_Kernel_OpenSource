/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
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

#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/msm_mdp.h>
#include <linux/memblock.h>
#include <linux/sort.h>
#include <linux/sw_sync.h>

#include <linux/msm_iommu_domains.h>
#include <soc/qcom/event_timer.h>
#include <linux/msm-bus.h>
#include "mdss.h"
#include "mdss_debug.h"
#include "mdss_fb.h"
#include "mdss_mdp.h"
#include "mdss_mdp_rotator.h"

#define VSYNC_PERIOD 16
#define BORDERFILL_NDX	0x0BF000BF
#define CHECK_BOUNDS(offset, size, max_size) \
	(((size) > (max_size)) || ((offset) > ((max_size) - (size))))

#define IS_RIGHT_MIXER_OV(flags, dst_x, left_lm_w)	\
	((flags & MDSS_MDP_RIGHT_MIXER) || (dst_x >= left_lm_w))

static int mdss_mdp_overlay_free_fb_pipe(struct msm_fb_data_type *mfd);
static int mdss_mdp_overlay_fb_parse_dt(struct msm_fb_data_type *mfd);
static int mdss_mdp_overlay_off(struct msm_fb_data_type *mfd);
static void __overlay_kickoff_requeue(struct msm_fb_data_type *mfd);
static void __vsync_retire_signal(struct msm_fb_data_type *mfd, int val);
static int __vsync_set_vsync_handler(struct msm_fb_data_type *mfd);

static inline bool is_ov_right_blend(struct mdp_rect *left_blend,
	struct mdp_rect *right_blend, u32 left_lm_w)
{
	return (((left_blend->x + left_blend->w) == right_blend->x)	&&
		((left_blend->x + left_blend->w) != left_lm_w)		&&
		(left_blend->x != right_blend->x)			&&
		(left_blend->y == right_blend->y)			&&
		(left_blend->h == right_blend->h));
}

/**
 * __is_more_decimation_doable() -
 * @pipe: pointer to pipe data structure
 *
 * if per pipe BW exceeds the limit and user
 * has not requested decimation then return
 * -E2BIG error back to user else try more
 * decimation based on following table config.
 *
 * ----------------------------------------------------------
 * error | split mode | src_split | v_deci |     action     |
 * ------|------------|-----------|--------|----------------|
 *       |            |           |   00   | return error   |
 *       |            |  enabled  |--------|----------------|
 *       |            |           |   >1   | more decmation |
 *       |     yes    |-----------|--------|----------------|
 *       |            |           |   00   | return error   |
 *       |            | disabled  |--------|----------------|
 *       |            |           |   >1   | return error   |
 * E2BIG |------------|-----------|--------|----------------|
 *       |            |           |   00   | return error   |
 *       |            |  enabled  |--------|----------------|
 *       |            |           |   >1   | more decmation |
 *       |     no     |-----------|--------|----------------|
 *       |            |           |   00   | return error   |
 *       |            | disabled  |--------|----------------|
 *       |            |           |   >1   | more decmation |
 * ----------------------------------------------------------
 */
static inline bool __is_more_decimation_doable(struct mdss_mdp_pipe *pipe)
{
	struct mdss_data_type *mdata = pipe->mixer_left->ctl->mdata;
	struct msm_fb_data_type *mfd = pipe->mixer_left->ctl->mfd;

	if (!mfd->split_mode && !pipe->vert_deci)
		return false;
	else if (mfd->split_mode && (!mdata->has_src_split ||
	   (mdata->has_src_split && !pipe->vert_deci)))
		return false;
	else
		return true;
}

static struct mdss_mdp_pipe *__overlay_find_pipe(
		struct msm_fb_data_type *mfd, u32 ndx)
{
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	struct mdss_mdp_pipe *tmp, *pipe = NULL;

	mutex_lock(&mdp5_data->list_lock);
	list_for_each_entry(tmp, &mdp5_data->pipes_used, list) {
		if (tmp->ndx == ndx) {
			pipe = tmp;
			break;
		}
	}
	mutex_unlock(&mdp5_data->list_lock);

	return pipe;
}

static int mdss_mdp_overlay_get(struct msm_fb_data_type *mfd,
				struct mdp_overlay *req)
{
	struct mdss_mdp_pipe *pipe;

	pipe = __overlay_find_pipe(mfd, req->id);
	if (!pipe) {
		pr_err("invalid pipe ndx=%x\n", req->id);
		return pipe ? PTR_ERR(pipe) : -ENODEV;
	}

	*req = pipe->req_data;

	return 0;
}

static int mdss_mdp_ov_xres_check(struct msm_fb_data_type *mfd,
	struct mdp_overlay *req)
{
	u32 xres = 0;
	u32 left_lm_w = left_lm_w_from_mfd(mfd);
	struct mdss_data_type *mdata = mfd_to_mdata(mfd);
	struct mdss_mdp_ctl *ctl = mfd_to_ctl(mfd);

	if (IS_RIGHT_MIXER_OV(req->flags, req->dst_rect.x, left_lm_w)) {
		if (mdata->has_src_split) {
			xres = left_lm_w;

			if (req->flags & MDSS_MDP_RIGHT_MIXER) {
				pr_warn("invalid use of RIGHT_MIXER flag.\n");
				/*
				 * if chip-set is capable of source split then
				 * all layers which are only on right LM should
				 * have their x offset relative to left LM's
				 * left-top or in other words relative to
				 * panel width.
				 * By modifying dst_x below, we are assuming
				 * that client is running in legacy mode
				 * chipset capable of source split.
				 */
				if (req->dst_rect.x < left_lm_w)
					req->dst_rect.x += left_lm_w;

				req->flags &= ~MDSS_MDP_RIGHT_MIXER;
			}
		} else if (req->dst_rect.x >= left_lm_w) {
			/*
			 * this is a step towards removing a reliance on
			 * MDSS_MDP_RIGHT_MIXER flags. With the new src split
			 * code, some clients of non-src-split chipsets have
			 * stopped sending MDSS_MDP_RIGHT_MIXER flag and
			 * modified their xres relative to full panel
			 * dimensions. In such cases, we need to deduct left
			 * layer mixer width before we programm this HW.
			 */
			req->dst_rect.x -= left_lm_w;
			req->flags |= MDSS_MDP_RIGHT_MIXER;
		}

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

	if (CHECK_BOUNDS(req->dst_rect.x, req->dst_rect.w, xres)) {
		pr_err("dst_xres is invalid. dst_x:%d, dst_w:%d, xres:%d\n",
			req->dst_rect.x, req->dst_rect.w, xres);
		return -EOVERFLOW;
	}

	return 0;
}

int mdss_mdp_overlay_req_check(struct msm_fb_data_type *mfd,
			       struct mdp_overlay *req,
			       struct mdss_mdp_format_params *fmt)
{
	u32 yres;
	u32 min_src_size, min_dst_size;
	int content_secure;
	struct mdss_data_type *mdata = mfd_to_mdata(mfd);
	struct mdss_mdp_ctl *ctl = mfd_to_ctl(mfd);

	yres = mfd->fbi->var.yres;

	content_secure = (req->flags & MDP_SECURE_OVERLAY_SESSION);
	if (!ctl->is_secure && content_secure &&
				 (mfd->panel.type == WRITEBACK_PANEL)) {
		pr_debug("return due to security concerns\n");
		return -EPERM;
	}
	if (mdata->mdp_rev >= MDSS_MDP_HW_REV_102) {
		min_src_size = fmt->is_yuv ? 2 : 1;
		min_dst_size = 1;
	} else {
		min_src_size = fmt->is_yuv ? 10 : 5;
		min_dst_size = 2;
	}

	if (req->z_order >= mdata->max_target_zorder) {
		pr_err("zorder %d out of range\n", req->z_order);
		return -ERANGE;
	}

	/*
	 * Cursor overlays are only supported for targets
	 * with dedicated cursors within VP
	 */
	if ((req->pipe_type == MDSS_MDP_PIPE_TYPE_CURSOR) &&
		((req->z_order != mdata->max_target_zorder - 1) ||
		 !mdata->ncursor_pipes ||
		 (req->src_rect.w > mdata->max_cursor_size))) {
		pr_err("Inccorect cursor overlay cursor_pipes=%d zorder=%d\n",
			mdata->ncursor_pipes, req->z_order);
		return -EINVAL;
	}

	if (req->src.width > MAX_IMG_WIDTH ||
	    req->src.height > MAX_IMG_HEIGHT ||
	    req->src_rect.w < min_src_size || req->src_rect.h < min_src_size ||
	    CHECK_BOUNDS(req->src_rect.x, req->src_rect.w, req->src.width) ||
	    CHECK_BOUNDS(req->src_rect.y, req->src_rect.h, req->src.height)) {
		pr_err("invalid source image img wh=%dx%d rect=%d,%d,%d,%d\n",
		       req->src.width, req->src.height,
		       req->src_rect.x, req->src_rect.y,
		       req->src_rect.w, req->src_rect.h);
		return -EOVERFLOW;
	}

	if (req->dst_rect.w < min_dst_size || req->dst_rect.h < min_dst_size) {
		pr_err("invalid destination resolution (%dx%d)",
		       req->dst_rect.w, req->dst_rect.h);
		return -EOVERFLOW;
	}

	if (req->horz_deci || req->vert_deci) {
		if (!mdata->has_decimation) {
			pr_err("No Decimation in MDP V=%x\n", mdata->mdp_rev);
			return -EINVAL;
		} else if ((req->horz_deci > MAX_DECIMATION) ||
				(req->vert_deci > MAX_DECIMATION))  {
			pr_err("Invalid decimation factors horz=%d vert=%d\n",
					req->horz_deci, req->vert_deci);
			return -EINVAL;
		} else if (req->flags & MDP_BWC_EN) {
			pr_err("Decimation can't be enabled with BWC\n");
			return -EINVAL;
		} else if (fmt->tile) {
			pr_err("Decimation can't be enabled with MacroTile format\n");
			return -EINVAL;
		}
	}

	if (!(req->flags & MDSS_MDP_ROT_ONLY)) {
		u32 src_w, src_h, dst_w, dst_h;

		if (CHECK_BOUNDS(req->dst_rect.y, req->dst_rect.h, yres)) {
			pr_err("invalid vertical destination: y=%d, h=%d\n",
				req->dst_rect.y, req->dst_rect.h);
			return -EOVERFLOW;
		}

		if (req->flags & MDP_ROT_90) {
			dst_h = req->dst_rect.w;
			dst_w = req->dst_rect.h;
		} else {
			dst_w = req->dst_rect.w;
			dst_h = req->dst_rect.h;
		}

		src_w = req->src_rect.w >> req->horz_deci;
		src_h = req->src_rect.h >> req->vert_deci;

		if (src_w > mdata->max_mixer_width) {
			pr_err("invalid source width=%d HDec=%d\n",
					req->src_rect.w, req->horz_deci);
			return -EINVAL;
		}

		if ((src_w * MAX_UPSCALE_RATIO) < dst_w) {
			pr_err("too much upscaling Width %d->%d\n",
			       req->src_rect.w, req->dst_rect.w);
			return -EINVAL;
		}

		if ((src_h * MAX_UPSCALE_RATIO) < dst_h) {
			pr_err("too much upscaling. Height %d->%d\n",
			       req->src_rect.h, req->dst_rect.h);
			return -EINVAL;
		}

		if (src_w > (dst_w * MAX_DOWNSCALE_RATIO)) {
			pr_err("too much downscaling. Width %d->%d H Dec=%d\n",
			       src_w, req->dst_rect.w, req->horz_deci);
			return -EINVAL;
		}

		if (src_h > (dst_h * MAX_DOWNSCALE_RATIO)) {
			pr_err("too much downscaling. Height %d->%d V Dec=%d\n",
			       src_h, req->dst_rect.h, req->vert_deci);
			return -EINVAL;
		}

		if (req->flags & MDP_BWC_EN) {
			if ((req->src.width != req->src_rect.w) ||
			    (req->src.height != req->src_rect.h)) {
				pr_err("BWC: unequal src img and rect w,h\n");
				return -EINVAL;
			}

			if (req->flags & MDP_DECIMATION_EN) {
				pr_err("Can't enable BWC decode && decimate\n");
				return -EINVAL;
			}
		}

		if ((req->flags & MDP_DEINTERLACE) &&
					!req->scale.enable_pxl_ext) {
			if (req->flags & MDP_SOURCE_ROTATED_90) {
				if ((req->src_rect.w % 4) != 0) {
					pr_err("interlaced rect not h/4\n");
					return -EINVAL;
				}
			} else if ((req->src_rect.h % 4) != 0) {
				pr_err("interlaced rect not h/4\n");
				return -EINVAL;
			}
		}
	} else {
		if (req->flags & MDP_DEINTERLACE) {
			if ((req->src_rect.h % 4) != 0) {
				pr_err("interlaced rect h not multiple of 4\n");
				return -EINVAL;
			}
		}
	}

	if (fmt->is_yuv) {
		if ((req->src_rect.x & 0x1) || (req->src_rect.y & 0x1) ||
		    (req->src_rect.w & 0x1) || (req->src_rect.h & 0x1)) {
			pr_err("invalid odd src resolution or coordinates\n");
			return -EINVAL;
		}
	}

	return 0;
}

static int __mdp_pipe_tune_perf(struct mdss_mdp_pipe *pipe,
	u32 flags)
{
	struct mdss_data_type *mdata = pipe->mixer_left->ctl->mdata;
	struct mdss_mdp_perf_params perf;
	int rc;

	flags |= PERF_CALC_PIPE_APPLY_CLK_FUDGE |
		PERF_CALC_PIPE_CALC_SMP_SIZE;

	for (;;) {
		rc = mdss_mdp_perf_calc_pipe(pipe, &perf, NULL,
			flags);

		if (!rc && (perf.mdp_clk_rate <= mdata->max_mdp_clk_rate)) {
			rc = mdss_mdp_perf_bw_check_pipe(&perf, pipe);
			if (!rc) {
				break;
			} else if (rc == -E2BIG &&
				   !__is_more_decimation_doable(pipe)) {
				pr_debug("pipe%d exceeded per pipe BW\n",
					pipe->num);
				return rc;
			}
		}

		/*
		 * if decimation is available try to reduce minimum clock rate
		 * requirement by applying vertical decimation and reduce
		 * mdp clock requirement
		 */
		if (mdata->has_decimation && (pipe->vert_deci < MAX_DECIMATION)
			&& !pipe->bwc_mode && !pipe->src_fmt->tile &&
			!pipe->scale.enable_pxl_ext)
			pipe->vert_deci++;
		else
			return -E2BIG;
	}

	return 0;
}

static int __mdss_mdp_validate_pxl_extn(struct mdss_mdp_pipe *pipe)
{
	int plane;

	for (plane = 0; plane < MAX_PLANES; plane++) {
		u32 hor_req_pixels, hor_fetch_pixels;
		u32 hor_ov_fetch, vert_ov_fetch;
		u32 vert_req_pixels, vert_fetch_pixels;
		u32 src_w = pipe->src.w >> pipe->horz_deci;
		u32 src_h = pipe->src.h >> pipe->vert_deci;

		/*
		 * plane 1 and 2 are for chroma and are same. While configuring
		 * HW, programming only one of the chroma components is
		 * sufficient.
		 */
		if (plane == 2)
			continue;

		/*
		 * For chroma plane, width is half for the following sub sampled
		 * formats. Except in case of decimation, where hardware avoids
		 * 1 line of decimation instead of downsampling.
		 */
		if (plane == 1 && !pipe->horz_deci &&
		    ((pipe->src_fmt->chroma_sample == MDSS_MDP_CHROMA_420) ||
		     (pipe->src_fmt->chroma_sample == MDSS_MDP_CHROMA_H2V1))) {
			src_w >>= 1;
		}

		if (plane == 1 && !pipe->vert_deci &&
		    ((pipe->src_fmt->chroma_sample == MDSS_MDP_CHROMA_420) ||
		     (pipe->src_fmt->chroma_sample == MDSS_MDP_CHROMA_H1V2)))
			src_h >>= 1;

		hor_req_pixels = pipe->scale.roi_w[plane] +
			pipe->scale.num_ext_pxls_left[plane] +
			pipe->scale.num_ext_pxls_right[plane];

		hor_fetch_pixels = src_w +
			pipe->scale.left_ftch[plane] +
			pipe->scale.left_rpt[plane] +
			pipe->scale.right_ftch[plane] +
			pipe->scale.right_rpt[plane];

		hor_ov_fetch = src_w + pipe->scale.left_ftch[plane] +
			pipe->scale.right_ftch[plane];

		vert_req_pixels = pipe->scale.num_ext_pxls_top[plane] +
			pipe->scale.num_ext_pxls_btm[plane];

		vert_fetch_pixels = pipe->scale.top_ftch[plane] +
			pipe->scale.top_rpt[plane] +
			pipe->scale.btm_ftch[plane] +
			pipe->scale.btm_rpt[plane];

		vert_ov_fetch = src_h + pipe->scale.top_ftch[plane] +
			pipe->scale.btm_ftch[plane];

		if ((hor_req_pixels != hor_fetch_pixels) ||
			(hor_ov_fetch > pipe->img_width) ||
			(vert_req_pixels != vert_fetch_pixels) ||
			(vert_ov_fetch > pipe->img_height)) {
			pr_err("err: plane=%d h_req:%d h_fetch:%d v_req:%d v_fetch:%d src_img:[%d,%d]\n",
					plane,
					hor_req_pixels, hor_fetch_pixels,
					vert_req_pixels, vert_fetch_pixels,
					pipe->img_width, pipe->img_height);
			pipe->scale.enable_pxl_ext = 0;
			return -EINVAL;
		}
	}

	return 0;
}

static int __mdss_mdp_overlay_setup_scaling(struct mdss_mdp_pipe *pipe)
{
	u32 src;
	int rc;

	src = pipe->src.w >> pipe->horz_deci;

	if (pipe->scale.enable_pxl_ext) {
		rc = __mdss_mdp_validate_pxl_extn(pipe);
		return rc;
	}

	memset(&pipe->scale, 0, sizeof(struct mdp_scale_data));
	rc = mdss_mdp_calc_phase_step(src, pipe->dst.w,
			&pipe->scale.phase_step_x[0]);
	if (rc == -EOVERFLOW) {
		/* overflow on horizontal direction is acceptable */
		rc = 0;
	} else if (rc) {
		pr_err("Horizontal scaling calculation failed=%d! %d->%d\n",
				rc, src, pipe->dst.w);
		return rc;
	}

	src = pipe->src.h >> pipe->vert_deci;
	rc = mdss_mdp_calc_phase_step(src, pipe->dst.h,
			&pipe->scale.phase_step_y[0]);

	if ((rc == -EOVERFLOW) && (pipe->type == MDSS_MDP_PIPE_TYPE_VIG)) {
		/* overflow on Qseed2 scaler is acceptable */
		rc = 0;
	} else if (rc == -EOVERFLOW) {
		/* overflow expected and should fallback to GPU */
		rc = -ECANCELED;
	} else if (rc) {
		pr_err("Vertical scaling calculation failed=%d! %d->%d\n",
				rc, src, pipe->dst.h);
	}
	return rc;
}

static inline void __mdss_mdp_overlay_set_chroma_sample(
	struct mdss_mdp_pipe *pipe)
{
	pipe->chroma_sample_v = pipe->chroma_sample_h = 0;

	switch (pipe->src_fmt->chroma_sample) {
	case MDSS_MDP_CHROMA_H1V2:
		pipe->chroma_sample_v = 1;
		break;
	case MDSS_MDP_CHROMA_H2V1:
		pipe->chroma_sample_h = 1;
		break;
	case MDSS_MDP_CHROMA_420:
		pipe->chroma_sample_v = 1;
		pipe->chroma_sample_h = 1;
		break;
	}
	if (pipe->horz_deci)
		pipe->chroma_sample_h = 0;
	if (pipe->vert_deci)
		pipe->chroma_sample_v = 0;
}

int mdss_mdp_overlay_pipe_setup(struct msm_fb_data_type *mfd,
	struct mdp_overlay *req, struct mdss_mdp_pipe **ppipe,
	struct mdss_mdp_pipe *left_blend_pipe, bool is_single_layer)
{
	struct mdss_mdp_format_params *fmt;
	struct mdss_mdp_pipe *pipe;
	struct mdss_mdp_mixer *mixer = NULL;
	u32 pipe_type, mixer_mux, len;
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	struct mdss_data_type *mdata = mfd_to_mdata(mfd);
	struct mdp_histogram_start_req hist;
	int ret;
	u32 bwc_enabled;
	u32 rot90;
	bool is_vig_needed = false;
	u32 left_lm_w = left_lm_w_from_mfd(mfd);
	u32 flags = 0;

	if (mdp5_data->ctl == NULL)
		return -ENODEV;

	if (req->flags & MDP_ROT_90) {
		pr_err("unsupported inline rotation\n");
		return -EOPNOTSUPP;
	}

	if ((req->dst_rect.w > mdata->max_mixer_width) ||
		(req->dst_rect.h > MAX_DST_H)) {
		pr_err("exceeded max mixer supported resolution %dx%d\n",
				req->dst_rect.w, req->dst_rect.h);
		return -EOVERFLOW;
	}

	if (IS_RIGHT_MIXER_OV(req->flags, req->dst_rect.x, left_lm_w))
		mixer_mux = MDSS_MDP_MIXER_MUX_RIGHT;
	else
		mixer_mux = MDSS_MDP_MIXER_MUX_LEFT;

	pr_debug("ctl=%u req id=%x mux=%d z_order=%d flags=0x%x dst_x:%d\n",
		mdp5_data->ctl->num, req->id, mixer_mux, req->z_order,
		req->flags, req->dst_rect.x);

	fmt = mdss_mdp_get_format_params(req->src.format);
	if (!fmt) {
		pr_err("invalid pipe format %d\n", req->src.format);
		return -EINVAL;
	}

	bwc_enabled = req->flags & MDP_BWC_EN;
	rot90 = req->flags & MDP_SOURCE_ROTATED_90;

	/*
	 * Always set yuv rotator output to pseudo planar.
	 */
	if (bwc_enabled || rot90) {
		req->src.format =
			mdss_mdp_get_rotator_dst_format(req->src.format, rot90,
				bwc_enabled);
		fmt = mdss_mdp_get_format_params(req->src.format);
		if (!fmt) {
			pr_err("invalid pipe format %d\n", req->src.format);
			return -EINVAL;
		}
	}

	ret = mdss_mdp_ov_xres_check(mfd, req);
	if (ret)
		return ret;

	ret = mdss_mdp_overlay_req_check(mfd, req, fmt);
	if (ret)
		return ret;

	mixer = mdss_mdp_mixer_get(mdp5_data->ctl, mixer_mux);
	if (!mixer) {
		pr_err("unable to get mixer\n");
		return -ENODEV;
	}

	if ((mdata->has_non_scalar_rgb) &&
		((req->src_rect.w != req->dst_rect.w) ||
			(req->src_rect.h != req->dst_rect.h)))
		is_vig_needed = true;

	if (req->id == MSMFB_NEW_REQUEST) {
		switch (req->pipe_type) {
		case PIPE_TYPE_VIG:
			pipe_type = MDSS_MDP_PIPE_TYPE_VIG;
			break;
		case PIPE_TYPE_RGB:
			pipe_type = MDSS_MDP_PIPE_TYPE_RGB;
			break;
		case PIPE_TYPE_DMA:
			pipe_type = MDSS_MDP_PIPE_TYPE_DMA;
			break;
		case PIPE_TYPE_CURSOR:
			pipe_type = MDSS_MDP_PIPE_TYPE_CURSOR;
			break;
		case PIPE_TYPE_AUTO:
		default:
			if (req->flags & MDP_OV_PIPE_FORCE_DMA)
				pipe_type = MDSS_MDP_PIPE_TYPE_DMA;
			else if (fmt->is_yuv ||
				(req->flags & MDP_OV_PIPE_SHARE) ||
				is_vig_needed)
				pipe_type = MDSS_MDP_PIPE_TYPE_VIG;
			else
				pipe_type = MDSS_MDP_PIPE_TYPE_RGB;
			break;
		}

		pipe = mdss_mdp_pipe_alloc(mixer, pipe_type, left_blend_pipe);

		/* RGB pipes can be used instead of DMA */
		if ((req->pipe_type == PIPE_TYPE_AUTO) && !pipe &&
		    (pipe_type == MDSS_MDP_PIPE_TYPE_DMA)) {
			pr_debug("giving RGB pipe for fb%d. flags:0x%x\n",
				mfd->index, req->flags);
			pipe_type = MDSS_MDP_PIPE_TYPE_RGB;
			pipe = mdss_mdp_pipe_alloc(mixer, pipe_type,
				left_blend_pipe);
		}

		/* VIG pipes can also support RGB format */
		if ((req->pipe_type == PIPE_TYPE_AUTO) && !pipe &&
		    (pipe_type == MDSS_MDP_PIPE_TYPE_RGB)) {
			pr_debug("giving ViG pipe for fb%d. flags:0x%x\n",
				mfd->index, req->flags);
			pipe_type = MDSS_MDP_PIPE_TYPE_VIG;
			pipe = mdss_mdp_pipe_alloc(mixer, pipe_type,
				left_blend_pipe);
		}

		if (pipe == NULL) {
			pr_err("error allocating pipe. flags=0x%x\n",
				req->flags);
			return -ENODEV;
		}

		ret = mdss_mdp_pipe_map(pipe);
		if (ret) {
			pr_err("unable to map pipe=%d\n", pipe->num);
			return ret;
		}

		mutex_lock(&mdp5_data->list_lock);
		list_add(&pipe->list, &mdp5_data->pipes_used);
		mutex_unlock(&mdp5_data->list_lock);
		pipe->mixer_left = mixer;
		pipe->mfd = mfd;
		pipe->pid = current->tgid;
		pipe->play_cnt = 0;
	} else {
		pipe = __overlay_find_pipe(mfd, req->id);
		if (!pipe) {
			pr_err("invalid pipe ndx=%x\n", req->id);
			return -ENODEV;
		}

		ret = mdss_mdp_pipe_map(pipe);
		if (IS_ERR_VALUE(ret)) {
			pr_err("Unable to map used pipe%d ndx=%x\n",
					pipe->num, pipe->ndx);
			return ret;
		}

		if (is_vig_needed && (pipe->type != MDSS_MDP_PIPE_TYPE_VIG)) {
			pr_err("pipe is non-scalar ndx=%x\n", req->id);
			ret = -EINVAL;
			goto exit_fail;
		}

		if (pipe->mixer_left != mixer) {
			if (!mixer->ctl || (mixer->ctl->mfd != mfd)) {
				pr_err("Can't switch mixer %d->%d pnum %d!\n",
					pipe->mixer_left->num, mixer->num,
						pipe->num);
				ret = -EINVAL;
				goto exit_fail;
			}
			pr_debug("switching pipe%d mixer %d->%d stage%d\n",
				pipe->num,
				pipe->mixer_left ? pipe->mixer_left->num : -1,
				mixer->num, req->z_order);
			mdss_mdp_mixer_pipe_unstage(pipe, pipe->mixer_left);
			pipe->mixer_left = mixer;
		}
	}

	if (left_blend_pipe) {
		if (pipe->priority <= left_blend_pipe->priority) {
			pr_debug("priority limitation. left:%d right%d\n",
				left_blend_pipe->priority, pipe->priority);
			ret = -EPERM;
			goto exit_fail;
		} else {
			pr_debug("pipe%d is a right_pipe\n", pipe->num);
			pipe->is_right_blend = true;
		}
	} else if (pipe->is_right_blend) {
		/*
		 * pipe used to be right blend need to update mixer
		 * configuration to remove it as a right blend
		 */
		mdss_mdp_mixer_pipe_unstage(pipe, pipe->mixer_left);
		mdss_mdp_mixer_pipe_unstage(pipe, pipe->mixer_right);
		pipe->is_right_blend = false;
	}

	if (mfd->panel_orientation)
		req->flags ^= mfd->panel_orientation;

	req->priority = pipe->priority;
	if (!pipe->dirty && !memcmp(req, &pipe->req_data, sizeof(*req))) {
		pr_debug("skipping pipe_reconfiguration\n");
		goto skip_reconfigure;
	}

	pipe->flags = req->flags;
	if (bwc_enabled  &&  !mdp5_data->mdata->has_bwc) {
		pr_err("BWC is not supported in MDP version %x\n",
			mdp5_data->mdata->mdp_rev);
		pipe->bwc_mode = 0;
	} else {
		pipe->bwc_mode = pipe->mixer_left->rotator_mode ?
			0 : (bwc_enabled ? 1 : 0) ;
	}
	pipe->img_width = req->src.width & 0x3fff;
	pipe->img_height = req->src.height & 0x3fff;
	pipe->src.x = req->src_rect.x;
	pipe->src.y = req->src_rect.y;
	pipe->src.w = req->src_rect.w;
	pipe->src.h = req->src_rect.h;
	pipe->dst.x = req->dst_rect.x;
	pipe->dst.y = req->dst_rect.y;
	pipe->dst.w = req->dst_rect.w;
	pipe->dst.h = req->dst_rect.h;

	pipe->horz_deci = req->horz_deci;
	pipe->vert_deci = req->vert_deci;

	/*
	 * check if overlay span across two mixers and if source split is
	 * available. If yes, enable src_split_req flag so that during mixer
	 * staging, same pipe will be stagged on both layer mixers.
	 */
	if (mdata->has_src_split) {
		if ((mixer_mux == MDSS_MDP_MIXER_MUX_LEFT) &&
		    ((req->dst_rect.x + req->dst_rect.w) > mixer->width)) {
			if (req->dst_rect.x >= mixer->width) {
				pr_err("%pS: err dst_x can't lie in right half",
					__builtin_return_address(0));
				pr_cont(" flags:0x%x dst x:%d w:%d lm_w:%d\n",
					req->flags, req->dst_rect.x,
					req->dst_rect.w, mixer->width);
				ret = -EINVAL;
				goto exit_fail;
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

	memcpy(&pipe->scale, &req->scale, sizeof(struct mdp_scale_data));
	pipe->src_fmt = fmt;
	__mdss_mdp_overlay_set_chroma_sample(pipe);

	pipe->mixer_stage = req->z_order;
	pipe->is_fg = req->is_fg;
	pipe->alpha = req->alpha;
	pipe->transp = req->transp_mask;
	pipe->blend_op = req->blend_op;
	if (pipe->blend_op == BLEND_OP_NOT_DEFINED)
		pipe->blend_op = fmt->alpha_enable ?
					BLEND_OP_PREMULTIPLIED :
					BLEND_OP_OPAQUE;

	if (!fmt->alpha_enable && (pipe->blend_op != BLEND_OP_OPAQUE))
		pr_debug("Unintended blend_op %d on layer with no alpha plane\n",
			pipe->blend_op);

	if (fmt->is_yuv && !(pipe->flags & MDP_SOURCE_ROTATED_90) &&
			!pipe->scale.enable_pxl_ext) {
		pipe->overfetch_disable = OVERFETCH_DISABLE_BOTTOM;

		if (!(pipe->flags & MDSS_MDP_DUAL_PIPE) ||
		    IS_RIGHT_MIXER_OV(pipe->flags, pipe->dst.x, left_lm_w))
			pipe->overfetch_disable |= OVERFETCH_DISABLE_RIGHT;
		pr_debug("overfetch flags=%x\n", pipe->overfetch_disable);
	} else {
		pipe->overfetch_disable = 0;
	}
	pipe->bg_color = req->bg_color;

	if (pipe->type == MDSS_MDP_PIPE_TYPE_CURSOR)
		goto cursor_done;

	if (pipe->flags & MDP_OVERLAY_PP_CFG_EN) {
		memcpy(&pipe->pp_cfg, &req->overlay_pp_cfg,
					sizeof(struct mdp_overlay_pp_params));
		len = pipe->pp_cfg.igc_cfg.len;
		if ((pipe->pp_cfg.config_ops & MDP_OVERLAY_PP_IGC_CFG) &&
						(len == IGC_LUT_ENTRIES)) {
			ret = copy_from_user(pipe->pp_res.igc_c0_c1,
					pipe->pp_cfg.igc_cfg.c0_c1_data,
					sizeof(uint32_t) * len);
			if (ret) {
				ret = -ENOMEM;
				goto exit_fail;
			}
			ret = copy_from_user(pipe->pp_res.igc_c2,
					pipe->pp_cfg.igc_cfg.c2_data,
					sizeof(uint32_t) * len);
			if (ret) {
				ret = -ENOMEM;
				goto exit_fail;
			}
			pipe->pp_cfg.igc_cfg.c0_c1_data =
							pipe->pp_res.igc_c0_c1;
			pipe->pp_cfg.igc_cfg.c2_data = pipe->pp_res.igc_c2;
		}
		if (pipe->pp_cfg.config_ops & MDP_OVERLAY_PP_HIST_CFG) {
			if (pipe->pp_cfg.hist_cfg.ops & MDP_PP_OPS_ENABLE) {
				hist.block = pipe->pp_cfg.hist_cfg.block;
				hist.frame_cnt =
					pipe->pp_cfg.hist_cfg.frame_cnt;
				hist.bit_mask = pipe->pp_cfg.hist_cfg.bit_mask;
				hist.num_bins = pipe->pp_cfg.hist_cfg.num_bins;
				mdss_mdp_hist_start(&hist);
			} else if (pipe->pp_cfg.hist_cfg.ops &
							MDP_PP_OPS_DISABLE) {
				mdss_mdp_hist_stop(pipe->pp_cfg.hist_cfg.block);
			}
		}
		len = pipe->pp_cfg.hist_lut_cfg.len;
		if ((pipe->pp_cfg.config_ops & MDP_OVERLAY_PP_HIST_LUT_CFG) &&
						(len == ENHIST_LUT_ENTRIES)) {
			ret = copy_from_user(pipe->pp_res.hist_lut,
					pipe->pp_cfg.hist_lut_cfg.data,
					sizeof(uint32_t) * len);
			if (ret) {
				ret = -ENOMEM;
				goto exit_fail;
			}
			pipe->pp_cfg.hist_lut_cfg.data = pipe->pp_res.hist_lut;
		}
	}

	/*
	 * Populate Color Space.
	*/
	if (pipe->src_fmt->is_yuv && (pipe->type == MDSS_MDP_PIPE_TYPE_VIG))
		pipe->csc_coeff_set = req->color_space;
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

	if (is_single_layer)
		flags |= PERF_CALC_PIPE_SINGLE_LAYER;

	ret = __mdp_pipe_tune_perf(pipe, flags);
	if (ret) {
		pr_debug("unable to satisfy performance. ret=%d\n", ret);
		goto exit_fail;
	}

	ret = __mdss_mdp_overlay_setup_scaling(pipe);
	if (ret)
		goto exit_fail;

	if ((mixer->type == MDSS_MDP_MIXER_TYPE_WRITEBACK) &&
		(mdp5_data->mdata->wfd_mode == MDSS_MDP_WFD_SHARED))
		mdss_mdp_smp_release(pipe);

	ret = mdss_mdp_smp_reserve(pipe);
	if (ret) {
		pr_debug("mdss_mdp_smp_reserve failed. pnum:%d ret=%d\n",
			pipe->num, ret);
		goto exit_fail;
	}


	req->id = pipe->ndx;

cursor_done:
	req->vert_deci = pipe->vert_deci;

	pipe->req_data = *req;
	pipe->dirty = false;

	pipe->params_changed++;
skip_reconfigure:
	*ppipe = pipe;

	mdss_mdp_pipe_unmap(pipe);

	return ret;
exit_fail:
	mdss_mdp_pipe_unmap(pipe);

	mutex_lock(&mdp5_data->list_lock);
	if (pipe->play_cnt == 0) {
		pr_debug("failed for pipe %d\n", pipe->num);
		if (!list_empty(&pipe->list))
			list_del_init(&pipe->list);
		mdss_mdp_pipe_destroy(pipe);
	}

	/* invalidate any overlays in this framebuffer after failure */
	list_for_each_entry(pipe, &mdp5_data->pipes_used, list) {
		pr_debug("freeing allocations for pipe %d\n", pipe->num);
		mdss_mdp_smp_unreserve(pipe);
		pipe->params_changed = 0;
		pipe->dirty = true;
	}
	mutex_unlock(&mdp5_data->list_lock);
	return ret;
}

static int mdss_mdp_overlay_set(struct msm_fb_data_type *mfd,
				struct mdp_overlay *req)
{
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	int ret;

	ret = mutex_lock_interruptible(&mdp5_data->ov_lock);
	if (ret)
		return ret;

	if (mdss_fb_is_power_off(mfd)) {
		mutex_unlock(&mdp5_data->ov_lock);
		return -EPERM;
	}

	if (req->flags & MDSS_MDP_ROT_ONLY) {
		ret = mdss_mdp_rotator_setup(mfd, req);
	} else if (req->src.format == MDP_RGB_BORDERFILL) {
		req->id = BORDERFILL_NDX;
	} else {
		struct mdss_mdp_pipe *pipe;

		/* userspace zorder start with stage 0 */
		req->z_order += MDSS_MDP_STAGE_0;

		ret = mdss_mdp_overlay_pipe_setup(mfd, req, &pipe, NULL, false);

		req->z_order -= MDSS_MDP_STAGE_0;
	}

	mutex_unlock(&mdp5_data->ov_lock);

	return ret;
}

/**
 * __mdss_mdp_overlay_free_list_purge() - clear free list of buffers
 * @mfd:	Msm frame buffer data structure for the associated fb
 *
 * Frees memory and clears current list of buffers which are pending free
 */
static void __mdss_mdp_overlay_free_list_purge(struct msm_fb_data_type *mfd)
{
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	int i;

	pr_debug("purging fb%d free list\n", mfd->index);
	for (i = 0; i < mdp5_data->free_list_size; i++)
		mdss_mdp_data_free(&mdp5_data->free_list[i]);
	mdp5_data->free_list_size = 0;
}

/**
 * __mdss_mdp_overlay_free_list_add() - add a buffer to free list
 * @mfd:	Msm frame buffer data structure for the associated fb
 */
static void __mdss_mdp_overlay_free_list_add(struct msm_fb_data_type *mfd,
		struct mdss_mdp_data *buf)
{
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	int i;

	/* if holding too many buffers free current list */
	if (mdp5_data->free_list_size >= MAX_FREE_LIST_SIZE) {
		pr_warn("max free list size for fb%d, purging\n", mfd->index);
		__mdss_mdp_overlay_free_list_purge(mfd);
	}

	BUG_ON(mdp5_data->free_list_size >= MAX_FREE_LIST_SIZE);
	i = mdp5_data->free_list_size++;
	mdp5_data->free_list[i] = *buf;
	memset(buf, 0, sizeof(*buf));
}

/**
 * mdss_mdp_overlay_cleanup() - handles cleanup after frame commit
 * @mfd:           Msm frame buffer data structure for the associated fb
 * @destroy_pipes: list of pipes that should be destroyed as part of cleanup
 *
 * Goes through destroy_pipes list and ensures they are ready to be destroyed
 * and cleaned up. Also cleanup of any pipe buffers after flip.
 */
static void mdss_mdp_overlay_cleanup(struct msm_fb_data_type *mfd,
		struct list_head *destroy_pipes)
{
	struct mdss_mdp_pipe *pipe, *tmp;
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	struct mdss_mdp_ctl *ctl = mfd_to_ctl(mfd);
	bool recovery_mode = false;

	mutex_lock(&mdp5_data->list_lock);
	list_for_each_entry(pipe, destroy_pipes, list) {
		/* make sure pipe fetch has been halted before freeing buffer */
		if (mdss_mdp_pipe_fetch_halt(pipe)) {
			/*
			 * if pipe is not able to halt. Enter recovery mode,
			 * by un-staging any pipes that are attached to mixer
			 * so that any freed pipes that are not able to halt
			 * can be staged in solid fill mode and be reset
			 * with next vsync
			 */
			if (!recovery_mode) {
				recovery_mode = true;
				mdss_mdp_mixer_unstage_all(ctl->mixer_left);
				mdss_mdp_mixer_unstage_all(ctl->mixer_right);
			}
			pipe->params_changed++;
			mdss_mdp_pipe_queue_data(pipe, NULL);
		}
	}

	if (recovery_mode) {
		pr_warn("performing recovery sequence for fb%d\n", mfd->index);
		__overlay_kickoff_requeue(mfd);
	}

	__mdss_mdp_overlay_free_list_purge(mfd);

	list_for_each_entry(pipe, &mdp5_data->pipes_used, list) {
		if (pipe->back_buf.num_planes) {
			/* make back buffer active */
			__mdss_mdp_overlay_free_list_add(mfd, &pipe->front_buf);
			swap(pipe->back_buf, pipe->front_buf);
		}
	}

	list_for_each_entry_safe(pipe, tmp, destroy_pipes, list) {
		/*
		 * in case of secure UI, the buffer needs to be released as
		 * soon as session is closed.
		 */
		if (pipe->flags & MDP_SECURE_DISPLAY_OVERLAY_SESSION)
			mdss_mdp_data_free(&pipe->front_buf);
		else
			__mdss_mdp_overlay_free_list_add(mfd, &pipe->front_buf);
		mdss_mdp_data_free(&pipe->back_buf);
		list_del_init(&pipe->list);
		if (recovery_mode) {
			mdss_mdp_mixer_pipe_unstage(pipe, pipe->mixer_left);
			mdss_mdp_mixer_pipe_unstage(pipe, pipe->mixer_right);
		}
		mdss_mdp_pipe_destroy(pipe);
	}
	mutex_unlock(&mdp5_data->list_lock);
}

void mdss_mdp_handoff_cleanup_pipes(struct msm_fb_data_type *mfd,
	u32 type)
{
	u32 i, npipes;
	struct mdss_mdp_pipe *pipes;
	struct mdss_mdp_pipe *pipe;
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	struct mdss_data_type *mdata = mfd_to_mdata(mfd);

	switch (type) {
	case MDSS_MDP_PIPE_TYPE_VIG:
		pipes = mdata->vig_pipes;
		npipes = mdata->nvig_pipes;
		break;
	case MDSS_MDP_PIPE_TYPE_RGB:
		pipes = mdata->rgb_pipes;
		npipes = mdata->nrgb_pipes;
		break;
	case MDSS_MDP_PIPE_TYPE_DMA:
		pipes = mdata->dma_pipes;
		npipes = mdata->ndma_pipes;
		break;
	default:
		return;
	}

	for (i = 0; i < npipes; i++) {
		pipe = &pipes[i];
		if (pipe->is_handed_off) {
			pr_debug("Unmapping handed off pipe %d\n", pipe->num);
			list_add(&pipe->list, &mdp5_data->pipes_cleanup);
			mdss_mdp_mixer_pipe_unstage(pipe, pipe->mixer_left);
			pipe->is_handed_off = false;
		}
	}
}

/**
 * mdss_mdp_overlay_start() - Programs the MDP control data path to hardware
 * @mfd: Msm frame buffer structure associated with fb device.
 *
 * Program the MDP hardware with the control settings for the framebuffer
 * device. In addition to this, this function also handles the transition
 * from the the splash screen to the android boot animation when the
 * continuous splash screen feature is enabled.
 */
int mdss_mdp_overlay_start(struct msm_fb_data_type *mfd)
{
	int rc;
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_mdp_ctl *ctl = mdp5_data->ctl;

	pr_debug("starting fb%d overlay called from %pS\n", mfd->index,
		__builtin_return_address(0));

	if (mdss_mdp_ctl_is_power_on(ctl)) {
		if (!mdp5_data->mdata->batfet)
			mdss_mdp_batfet_ctrl(mdp5_data->mdata, true);
		return 0;
	} else if (mfd->panel_info->cont_splash_enabled) {
		mutex_lock(&mdp5_data->list_lock);
		rc = list_empty(&mdp5_data->pipes_used);
		mutex_unlock(&mdp5_data->list_lock);
		if (rc) {
			pr_debug("empty kickoff on fb%d during cont splash\n",
					mfd->index);
			return 0;
		}
	}

	pr_debug("starting fb%d overlay\n", mfd->index);

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);

	/*
	 * If idle pc feature is not enabled, then get a reference to the
	 * runtime device which will be released when overlay is turned off
	 */
	if (!mdp5_data->mdata->idle_pc_enabled ||
		(mfd->panel_info->type != MIPI_CMD_PANEL)) {
		rc = pm_runtime_get_sync(&mfd->pdev->dev);
		if (IS_ERR_VALUE(rc)) {
			pr_err("unable to resume with pm_runtime_get_sync rc=%d\n",
				rc);
			goto end;
		}
	}

	/*
	 * We need to do hw init before any hw programming.
	 * Also, hw init involves programming the VBIF registers which
	 * should be done only after attaching IOMMU which in turn would call
	 * in to TZ to restore security configs on the VBIF registers.
	 * This is not needed when continuous splash screen is enabled since
	 * we would have called in to TZ to restore security configs from LK.
	 */
	if (!mdata->mdss_util->iommu_attached()) {
		if (!mfd->panel_info->cont_splash_enabled) {
			rc = mdss_iommu_ctrl(1);
			if (IS_ERR_VALUE(rc)) {
				pr_err("iommu attach failed rc=%d\n", rc);
				goto end;
			}
			mdss_hw_init(mdss_res);
			mdss_iommu_ctrl(0);
		}
	}

	/*
	 * Increment the overlay active count prior to calling ctl_start.
	 * This is needed to ensure that if idle power collapse kicks in
	 * right away, it would be handled correctly.
	 */
	atomic_inc(&mdp5_data->mdata->active_intf_cnt);
	rc = mdss_mdp_ctl_start(ctl, false);
	if (rc == 0) {
		mdss_mdp_ctl_notifier_register(mdp5_data->ctl,
				&mfd->mdp_sync_pt_data.notifier);
	} else {
		pr_err("mdp ctl start failed.\n");
		goto ctl_error;
	}

	rc = mdss_mdp_splash_cleanup(mfd, true);
	if (!rc)
		goto end;

ctl_error:
	mdss_mdp_ctl_destroy(ctl);
	atomic_dec(&mdp5_data->mdata->active_intf_cnt);
	mdp5_data->ctl = NULL;
end:
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
	return rc;
}

static void mdss_mdp_overlay_update_pm(struct mdss_overlay_private *mdp5_data)
{
	ktime_t wakeup_time;

	if (!mdp5_data->cpu_pm_hdl)
		return;

	if (mdss_mdp_display_wakeup_time(mdp5_data->ctl, &wakeup_time))
		return;

	activate_event_timer(mdp5_data->cpu_pm_hdl, wakeup_time);
}

static int __overlay_queue_pipes(struct msm_fb_data_type *mfd)
{
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	struct mdss_mdp_pipe *pipe;
	struct mdss_mdp_ctl *ctl = mfd_to_ctl(mfd);
	struct mdss_mdp_ctl *tmp;
	int ret = 0;

	list_for_each_entry(pipe, &mdp5_data->pipes_used, list) {
		struct mdss_mdp_data *buf;
		/*
		 * When secure display is enabled, if there is a non secure
		 * display pipe, skip that
		 */
		if (mdss_get_sd_client_cnt() &&
			!(pipe->flags & MDP_SECURE_DISPLAY_OVERLAY_SESSION)) {
			pr_warn("Non secure pipe during secure display: %u: %08X, skip\n",
					pipe->num, pipe->flags);
			continue;
		}
		/*
		 * When external is connected and no dedicated wfd is present,
		 * reprogram DMA pipe before kickoff to clear out any previous
		 * block mode configuration.
		 */
		if ((pipe->type == MDSS_MDP_PIPE_TYPE_DMA) &&
		    (ctl->shared_lock &&
		    (ctl->mdata->wfd_mode == MDSS_MDP_WFD_SHARED))) {
			if (ctl->mdata->mixer_switched) {
				ret = mdss_mdp_overlay_pipe_setup(mfd,
					&pipe->req_data, &pipe, NULL, false);
				pr_debug("reseting DMA pipe for ctl=%d",
					 ctl->num);
			}
			if (ret) {
				pr_err("can't reset DMA pipe ret=%d ctl=%d\n",
					ret, ctl->num);
				return ret;
			}

			tmp = mdss_mdp_ctl_mixer_switch(ctl,
					MDSS_MDP_WB_CTL_TYPE_LINE);
			if (!tmp)
				return -EINVAL;
			pipe->mixer_left = mdss_mdp_mixer_get(tmp,
					MDSS_MDP_MIXER_MUX_DEFAULT);
		}

		/* ensure pipes are always reconfigured after power off/on */
		if (ctl->play_cnt == 0)
			pipe->params_changed++;

		if (pipe->back_buf.num_planes) {
			buf = &pipe->back_buf;

			ret = mdss_mdp_data_map(buf);
		} else if (!pipe->params_changed) {
			continue;
		} else if (pipe->front_buf.num_planes) {
			buf = &pipe->front_buf;
		} else {
			pr_debug("no buf detected pnum=%d use solid fill\n",
					pipe->num);
			if ((pipe->flags & MDP_SOLID_FILL) == 0) {
				pr_warn("commit without buffer on pipe %d\n",
					pipe->num);
				ret = -EINVAL;
			}
			buf = NULL;
		}

		if (!IS_ERR_VALUE(ret))
			ret = mdss_mdp_pipe_queue_data(pipe, buf);

		if (IS_ERR_VALUE(ret)) {
			pr_warn("Unable to queue data for pnum=%d\n",
					pipe->num);
			mdss_mdp_mixer_pipe_unstage(pipe, pipe->mixer_left);
			mdss_mdp_mixer_pipe_unstage(pipe, pipe->mixer_right);
		}
	}

	return 0;
}

static void __overlay_kickoff_requeue(struct msm_fb_data_type *mfd)
{
	struct mdss_mdp_ctl *ctl = mfd_to_ctl(mfd);

	mdss_mdp_display_commit(ctl, NULL, NULL);
	mdss_mdp_display_wait4comp(ctl);

	ATRACE_BEGIN("sspp_programming");
	__overlay_queue_pipes(mfd);
	ATRACE_END("sspp_programming");

	mdss_mdp_display_commit(ctl, NULL,  NULL);
	mdss_mdp_display_wait4comp(ctl);
}

static int mdss_mdp_commit_cb(enum mdp_commit_stage_type commit_stage,
	void *data)
{
	int ret = 0;
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)data;
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	struct mdss_mdp_ctl *ctl;

	switch (commit_stage) {
	case MDP_COMMIT_STAGE_SETUP_DONE:
		ctl = mfd_to_ctl(mfd);
		mdss_mdp_ctl_notify(ctl, MDP_NOTIFY_FRAME_CTX_DONE);
		mdp5_data->kickoff_released = true;
		mutex_unlock(&mdp5_data->ov_lock);
		break;
	case MDP_COMMIT_STAGE_READY_FOR_KICKOFF:
		mutex_lock(&mdp5_data->ov_lock);
		break;
	default:
		pr_err("Invalid commit stage %x", commit_stage);
		break;
	}

	return ret;
}

int mdss_mdp_overlay_kickoff(struct msm_fb_data_type *mfd,
				struct mdp_display_commit *data)
{
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	struct mdss_mdp_pipe *pipe, *tmp;
	struct mdss_mdp_ctl *ctl = mfd_to_ctl(mfd);
	struct mdp_display_commit temp_data;
	int ret = 0;
	int sd_in_pipe = 0;
	bool need_cleanup = false;
	struct mdss_mdp_commit_cb commit_cb;
	LIST_HEAD(destroy_pipes);

	ATRACE_BEGIN(__func__);
	if (ctl->shared_lock) {
		mdss_mdp_ctl_notify(ctl, MDP_NOTIFY_FRAME_BEGIN);
		mutex_lock(ctl->shared_lock);
	}

	mutex_lock(&mdp5_data->ov_lock);
	ret = mdss_mdp_overlay_start(mfd);
	if (ret) {
		pr_err("unable to start overlay %d (%d)\n", mfd->index, ret);
		goto unlock_exit;
	}

	if (!mdss_mdp_ctl_is_power_on(ctl)) {
		pr_debug("ctl is not powerd on. skip kickoff\n");
		goto unlock_exit;
	}

	ret = mdss_iommu_ctrl(1);
	if (IS_ERR_VALUE(ret)) {
		pr_err("iommu attach failed rc=%d\n", ret);
		goto unlock_exit;
	}
	mutex_lock(&mdp5_data->list_lock);

	/*
	 * check if there is a secure display session
	 */
	list_for_each_entry(pipe, &mdp5_data->pipes_used, list) {
		if (pipe->flags & MDP_SECURE_DISPLAY_OVERLAY_SESSION) {
			sd_in_pipe = 1;
			pr_debug("Secure pipe: %u : %08X\n",
					pipe->num, pipe->flags);
		}
	}
	/*
	 * If there is no secure display session and sd_enabled, disable the
	 * secure display session
	 */
	if (!sd_in_pipe && mdp5_data->sd_enabled) {
		/* disable the secure display on last client */
		if (mdss_get_sd_client_cnt() == 1)
			ret = mdss_mdp_secure_display_ctrl(0);
		if (!ret) {
			mdss_update_sd_client(mdp5_data->mdata, false);
			mdp5_data->sd_enabled = 0;
		}
	}

	if (!ctl->shared_lock)
		mdss_mdp_ctl_notify(ctl, MDP_NOTIFY_FRAME_BEGIN);

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);

	__vsync_set_vsync_handler(mfd);

	if (data) {
		mdss_mdp_set_roi(ctl, data);
	} else {
		temp_data.l_roi = (struct mdp_rect){0, 0,
			ctl->mixer_left->width, ctl->mixer_left->height};
		if (ctl->mixer_right) {
			temp_data.r_roi = (struct mdp_rect) {0, 0,
			ctl->mixer_right->width, ctl->mixer_right->height};
		}
		mdss_mdp_set_roi(ctl, &temp_data);
	}


	/*
	 * Setup pipe in solid fill before unstaging,
	 * to ensure no fetches are happening after dettach or reattach.
	 */
	list_for_each_entry_safe(pipe, tmp, &mdp5_data->pipes_cleanup, list) {
		mdss_mdp_pipe_queue_data(pipe, NULL);
		mdss_mdp_mixer_pipe_unstage(pipe, pipe->mixer_left);
		mdss_mdp_mixer_pipe_unstage(pipe, pipe->mixer_right);
		list_move(&pipe->list, &destroy_pipes);
		need_cleanup = true;
	}

	ATRACE_BEGIN("sspp_programming");
	ret = __overlay_queue_pipes(mfd);
	ATRACE_END("sspp_programming");
	mutex_unlock(&mdp5_data->list_lock);

	mdp5_data->kickoff_released = false;

	if (mfd->panel.type == WRITEBACK_PANEL) {
		ATRACE_BEGIN("wb_kickoff");
		if (!need_cleanup) {
			commit_cb.commit_cb_fnc = mdss_mdp_commit_cb;
			commit_cb.data = mfd;
			ret = mdss_mdp_wb_kickoff(mfd, &commit_cb);
		} else {
			mdss_mdp_wb_kickoff(mfd, NULL);
		}
		ATRACE_END("wb_kickoff");
	} else {
		ATRACE_BEGIN("display_commit");
		if (!need_cleanup) {
			commit_cb.commit_cb_fnc = mdss_mdp_commit_cb;
			commit_cb.data = mfd;
			ret = mdss_mdp_display_commit(mdp5_data->ctl, NULL,
				&commit_cb);
		} else  {
			ret = mdss_mdp_display_commit(mdp5_data->ctl, NULL,
				NULL);
		}
		ATRACE_END("display_commit");
	}

	if ((!need_cleanup) && (!mdp5_data->kickoff_released))
		mdss_mdp_ctl_notify(ctl, MDP_NOTIFY_FRAME_CTX_DONE);

	if (IS_ERR_VALUE(ret))
		goto commit_fail;

	mutex_unlock(&mdp5_data->ov_lock);
	mdss_mdp_overlay_update_pm(mdp5_data);

	ATRACE_BEGIN("display_wait4comp");
	ret = mdss_mdp_display_wait4comp(mdp5_data->ctl);
	ATRACE_END("display_wait4comp");
	mutex_lock(&mdp5_data->ov_lock);

	if (ret == 0) {
		if (!mdp5_data->sd_enabled && sd_in_pipe) {
			if (!mdss_get_sd_client_cnt())
				ret = mdss_mdp_secure_display_ctrl(1);
			if (!ret) {
				mdp5_data->sd_enabled = 1;
				mdss_update_sd_client(mdp5_data->mdata, true);
			}
		}
	}

	mdss_fb_update_notify_update(mfd);
commit_fail:
	ATRACE_BEGIN("overlay_cleanup");
	mdss_mdp_overlay_cleanup(mfd, &destroy_pipes);
	ATRACE_END("overlay_cleanup");
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
	mdss_mdp_ctl_notify(ctl, MDP_NOTIFY_FRAME_FLUSHED);
	if (!mdp5_data->kickoff_released)
		mdss_mdp_ctl_notify(ctl, MDP_NOTIFY_FRAME_CTX_DONE);

	mdss_iommu_ctrl(0);
unlock_exit:
	mutex_unlock(&mdp5_data->ov_lock);
	if (ctl->shared_lock)
		mutex_unlock(ctl->shared_lock);
	ATRACE_END(__func__);

	return ret;
}

int mdss_mdp_overlay_release(struct msm_fb_data_type *mfd, int ndx)
{
	struct mdss_mdp_pipe *pipe, *tmp;
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	u32 unset_ndx = 0;
	int destroy_pipe;

	mutex_lock(&mdp5_data->list_lock);
	list_for_each_entry_safe(pipe, tmp, &mdp5_data->pipes_used, list) {
		if (pipe->ndx & ndx) {
			if (mdss_mdp_pipe_map(pipe)) {
				pr_err("Unable to map used pipe%d ndx=%x\n",
						pipe->num, pipe->ndx);
				continue;
			}

			unset_ndx |= pipe->ndx;

			pipe->pid = 0;
			destroy_pipe = pipe->play_cnt == 0;

			if (destroy_pipe)
				list_del_init(&pipe->list);
			else
				list_move(&pipe->list,
						&mdp5_data->pipes_cleanup);

			mdss_mdp_pipe_unmap(pipe);
			if (destroy_pipe)
				mdss_mdp_pipe_destroy(pipe);

			if (unset_ndx == ndx)
				break;
		}
	}
	mutex_unlock(&mdp5_data->list_lock);

	if (unset_ndx != ndx) {
		pr_warn("Unable to unset pipe(s) ndx=0x%x unset=0x%x\n",
				ndx, unset_ndx);
		return -ENOENT;
	}

	return 0;
}

static int mdss_mdp_overlay_unset(struct msm_fb_data_type *mfd, int ndx)
{
	int ret = 0;
	struct mdss_overlay_private *mdp5_data;

	if (!mfd)
		return -ENODEV;

	mdp5_data = mfd_to_mdp5_data(mfd);

	if (!mdp5_data || !mdp5_data->ctl)
		return -ENODEV;

	ret = mutex_lock_interruptible(&mdp5_data->ov_lock);
	if (ret)
		return ret;

	if (ndx == BORDERFILL_NDX) {
		pr_debug("borderfill disable\n");
		mdp5_data->borderfill_enable = false;
		ret = 0;
		goto done;
	}

	if (mdss_fb_is_power_off(mfd)) {
		ret = -EPERM;
		goto done;
	}

	pr_debug("unset ndx=%x\n", ndx);

	if (ndx & MDSS_MDP_ROT_SESSION_MASK) {
		ret = mdss_mdp_rotator_unset(ndx);
	} else {
		ret = mdss_mdp_overlay_release(mfd, ndx);
	}

done:
	mutex_unlock(&mdp5_data->ov_lock);

	return ret;
}

/**
 * mdss_mdp_overlay_release_all() - release any overlays associated with fb dev
 * @mfd:	Msm frame buffer structure associated with fb device
 * @release_all: ignore pid and release all the pipes
 *
 * Release any resources allocated by calling process, this can be called
 * on fb_release to release any overlays/rotator sessions left open.
 */
static int __mdss_mdp_overlay_release_all(struct msm_fb_data_type *mfd,
	bool release_all, uint32_t pid)
{
	struct mdss_mdp_pipe *pipe;
	struct mdss_mdp_rotator_session *rot, *tmp;
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	u32 unset_ndx = 0;
	int cnt = 0;

	pr_debug("releasing all resources for fb%d pid=%d\n", mfd->index, pid);

	mutex_lock(&mdp5_data->ov_lock);
	mutex_lock(&mdp5_data->list_lock);
	list_for_each_entry(pipe, &mdp5_data->pipes_used, list) {
		if (release_all || (pipe->pid == pid)) {
			unset_ndx |= pipe->ndx;
			cnt++;
		}
	}

	if (!mfd->ref_cnt && !list_empty(&mdp5_data->pipes_cleanup)) {
		pr_debug("fb%d:: free pipes present in cleanup list",
			mfd->index);
		cnt++;
	}

	pr_debug("release_all=%d mfd->ref_cnt=%d unset_ndx=0x%x cnt=%d\n",
		release_all, mfd->ref_cnt, unset_ndx, cnt);

	mutex_unlock(&mdp5_data->list_lock);

	if (unset_ndx) {
		pr_debug("%d pipes need cleanup (%x)\n", cnt, unset_ndx);
		mdss_mdp_overlay_release(mfd, unset_ndx);
	}
	mutex_unlock(&mdp5_data->ov_lock);

	if (cnt)
		mfd->mdp.kickoff_fnc(mfd, NULL);

	list_for_each_entry_safe(rot, tmp, &mdp5_data->rot_proc_list, list) {
		if (rot->pid == pid) {
			if (!list_empty(&rot->list))
				list_del_init(&rot->list);
			mdss_mdp_rotator_release(rot);
		}
	}

	return 0;
}

static int mdss_mdp_overlay_play_wait(struct msm_fb_data_type *mfd,
				      struct msmfb_overlay_data *req)
{
	int ret = 0;

	if (!mfd)
		return -ENODEV;

	ret = mfd->mdp.kickoff_fnc(mfd, NULL);
	if (!ret)
		pr_err("error displaying\n");

	return ret;
}

static int mdss_mdp_overlay_queue(struct msm_fb_data_type *mfd,
				  struct msmfb_overlay_data *req)
{
	struct mdss_mdp_pipe *pipe;
	struct mdss_mdp_data *src_data;
	int ret;
	u32 flags;

	pipe = __overlay_find_pipe(mfd, req->id);
	if (!pipe) {
		pr_err("pipe ndx=%x doesn't exist\n", req->id);
		return -ENODEV;
	}

	ret = mdss_mdp_pipe_map(pipe);
	if (IS_ERR_VALUE(ret)) {
		pr_err("Unable to map used pipe%d ndx=%x\n",
				pipe->num, pipe->ndx);
		return ret;
	}

	pr_debug("ov queue pnum=%d\n", pipe->num);

	if (pipe->flags & MDP_SOLID_FILL)
		pr_warn("Unexpected buffer queue to a solid fill pipe\n");

	flags = (pipe->flags & MDP_SECURE_OVERLAY_SESSION);

	src_data = &pipe->back_buf;
	if (src_data->num_planes) {
		pr_warn("dropped buffer pnum=%d play=%d addr=0x%pa\n",
			pipe->num, pipe->play_cnt, &src_data->p[0].addr);
		mdss_mdp_data_free(src_data);
	}

	ret = mdss_mdp_data_get(src_data, &req->data, 1, flags);
	if (IS_ERR_VALUE(ret))
		pr_err("src_data pmem error\n");

	mdss_mdp_pipe_unmap(pipe);

	return ret;
}

static int mdss_mdp_overlay_play(struct msm_fb_data_type *mfd,
				 struct msmfb_overlay_data *req)
{
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	int ret = 0;

	pr_debug("play req id=%x\n", req->id);

	ret = mutex_lock_interruptible(&mdp5_data->ov_lock);
	if (ret)
		return ret;

	if (mdss_fb_is_power_off(mfd)) {
		ret = -EPERM;
		goto done;
	}

	mdss_mdp_release_splash_pipe(mfd);

	if (req->id & MDSS_MDP_ROT_SESSION_MASK) {
		ret = mdss_mdp_rotator_play(mfd, req);
	} else if (req->id == BORDERFILL_NDX) {
		pr_debug("borderfill enable\n");
		mdp5_data->borderfill_enable = true;
		ret = mdss_mdp_overlay_free_fb_pipe(mfd);
	} else {
		ret = mdss_mdp_overlay_queue(mfd, req);
	}

done:
	mutex_unlock(&mdp5_data->ov_lock);

	return ret;
}

static int mdss_mdp_overlay_free_fb_pipe(struct msm_fb_data_type *mfd)
{
	struct mdss_mdp_pipe *pipe;
	u32 fb_ndx = 0;
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);

	pipe = mdss_mdp_get_staged_pipe(mdp5_data->ctl,
		MDSS_MDP_MIXER_MUX_LEFT, MDSS_MDP_STAGE_BASE, false);
	if (pipe)
		fb_ndx |= pipe->ndx;

	pipe = mdss_mdp_get_staged_pipe(mdp5_data->ctl,
		MDSS_MDP_MIXER_MUX_RIGHT, MDSS_MDP_STAGE_BASE, false);
	if (pipe)
		fb_ndx |= pipe->ndx;

	if (fb_ndx) {
		pr_debug("unstaging framebuffer pipes %x\n", fb_ndx);
		mdss_mdp_overlay_release(mfd, fb_ndx);
	}
	return 0;
}

static int mdss_mdp_overlay_get_fb_pipe(struct msm_fb_data_type *mfd,
					struct mdss_mdp_pipe **ppipe,
					int mixer_mux)
{
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	struct mdss_mdp_pipe *pipe;
	int ret;

	pipe = mdss_mdp_get_staged_pipe(mdp5_data->ctl, mixer_mux,
		MDSS_MDP_STAGE_BASE, false);

	if (pipe == NULL) {
		struct mdp_overlay req;
		struct fb_info *fbi = mfd->fbi;
		struct mdss_mdp_mixer *mixer;
		int bpp;

		mixer = mdss_mdp_mixer_get(mdp5_data->ctl,
					MDSS_MDP_MIXER_MUX_LEFT);
		if (!mixer) {
			pr_err("unable to retrieve mixer\n");
			return -ENODEV;
		}

		memset(&req, 0, sizeof(req));

		bpp = fbi->var.bits_per_pixel / 8;
		req.id = MSMFB_NEW_REQUEST;
		req.src.format = mfd->fb_imgType;
		req.src.height = fbi->var.yres;
		req.src.width = fbi->fix.line_length / bpp;
		if (mixer_mux == MDSS_MDP_MIXER_MUX_RIGHT) {
			if (req.src.width <= mixer->width) {
				pr_warn("right fb pipe not needed\n");
				return -EINVAL;
			}

			req.src_rect.x = mixer->width;
			req.src_rect.w = fbi->var.xres - mixer->width;
		} else {
			req.src_rect.x = 0;
			req.src_rect.w = MIN(fbi->var.xres,
							mixer->width);
		}

		req.src_rect.y = 0;
		req.src_rect.h = req.src.height;
		req.dst_rect = req.src_rect;
		req.z_order = MDSS_MDP_STAGE_BASE;

		pr_debug("allocating base pipe mux=%d\n", mixer_mux);

		ret = mdss_mdp_overlay_pipe_setup(mfd, &req, &pipe, NULL,
			false);
		if (ret)
			return ret;
	}
	pr_debug("ctl=%d pnum=%d\n", mdp5_data->ctl->num, pipe->num);

	*ppipe = pipe;
	return 0;
}

static void mdss_mdp_overlay_pan_display(struct msm_fb_data_type *mfd)
{
	struct mdss_mdp_data *buf;
	struct mdss_mdp_pipe *pipe;
	struct fb_info *fbi;
	struct mdss_overlay_private *mdp5_data;
	struct mdss_data_type *mdata;
	u32 offset;
	int bpp, ret;

	if (!mfd || !mfd->mdp.private1)
		return;

	mdata = mfd_to_mdata(mfd);
	fbi = mfd->fbi;
	mdp5_data = mfd_to_mdp5_data(mfd);

	if (!mdp5_data || !mdp5_data->ctl)
		return;

	if (!fbi->fix.smem_start || fbi->fix.smem_len == 0 ||
			mdp5_data->borderfill_enable) {
		mfd->mdp.kickoff_fnc(mfd, NULL);
		return;
	}

	if (mutex_lock_interruptible(&mdp5_data->ov_lock))
		return;

	if ((mdss_fb_is_power_off(mfd)) &&
		!((mfd->dcm_state == DCM_ENTER) &&
		(mfd->panel.type == MIPI_CMD_PANEL))) {
		mutex_unlock(&mdp5_data->ov_lock);
		return;
	}

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);


	bpp = fbi->var.bits_per_pixel / 8;
	offset = fbi->var.xoffset * bpp +
		 fbi->var.yoffset * fbi->fix.line_length;

	if (offset > fbi->fix.smem_len) {
		pr_err("invalid fb offset=%u total length=%u\n",
		       offset, fbi->fix.smem_len);
		goto pan_display_error;
	}

	ret = mdss_mdp_overlay_get_fb_pipe(mfd, &pipe,
					MDSS_MDP_MIXER_MUX_LEFT);
	if (ret) {
		pr_err("unable to allocate base pipe\n");
		goto pan_display_error;
	}

	if (mdss_mdp_pipe_map(pipe)) {
		pr_err("unable to map base pipe\n");
		goto pan_display_error;
	}

	ret = mdss_mdp_overlay_start(mfd);
	if (ret) {
		pr_err("unable to start overlay %d (%d)\n", mfd->index, ret);
		goto pan_display_error;
	}

	ret = mdss_iommu_ctrl(1);
	if (IS_ERR_VALUE(ret)) {
		pr_err("IOMMU attach failed\n");
		goto pan_display_error;
	}

	buf = &pipe->back_buf;
	if (mdata->mdss_util->iommu_attached()) {
		if (!mfd->iova) {
			pr_err("mfd iova is zero\n");
			mdss_mdp_pipe_unmap(pipe);
			goto pan_display_error;
		}
		buf->p[0].addr = mfd->iova;
	} else {
		buf->p[0].addr = fbi->fix.smem_start;
	}

	buf->p[0].addr += offset;
	buf->p[0].len = fbi->fix.smem_len - offset;
	buf->num_planes = 1;
	mdss_mdp_pipe_unmap(pipe);

	if (fbi->var.xres > mdata->max_mixer_width || is_split_lm(mfd)) {
		ret = mdss_mdp_overlay_get_fb_pipe(mfd, &pipe,
					   MDSS_MDP_MIXER_MUX_RIGHT);
		if (ret) {
			pr_err("unable to allocate right base pipe\n");
			goto pan_display_error;
		}
		if (mdss_mdp_pipe_map(pipe)) {
			pr_err("unable to map right base pipe\n");
			goto pan_display_error;
		}

		pipe->back_buf = *buf;
		mdss_mdp_pipe_unmap(pipe);
	}
	mutex_unlock(&mdp5_data->ov_lock);

	if ((fbi->var.activate & FB_ACTIVATE_VBL) ||
	    (fbi->var.activate & FB_ACTIVATE_FORCE))
		mfd->mdp.kickoff_fnc(mfd, NULL);

	mdss_iommu_ctrl(0);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
	return;

pan_display_error:
	mdss_iommu_ctrl(0);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
	mutex_unlock(&mdp5_data->ov_lock);
}

static void remove_underrun_vsync_handler(struct work_struct *work)
{
	int rc;
	struct mdss_mdp_ctl *ctl =
		container_of(work, typeof(*ctl), remove_underrun_handler);

	if (!ctl || !ctl->remove_vsync_handler) {
		pr_err("ctl or vsync handler is NULL\n");
		return;
	}

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);
	rc = ctl->remove_vsync_handler(ctl,
			&ctl->recover_underrun_handler);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
}

static void mdss_mdp_recover_underrun_handler(struct mdss_mdp_ctl *ctl,
						ktime_t t)
{
	if (!ctl) {
		pr_err("ctl is NULL\n");
		return;
	}

	mdss_mdp_ctl_reset(ctl);
	schedule_work(&ctl->remove_underrun_handler);
}

/* function is called in irq context should have minimum processing */
static void mdss_mdp_overlay_handle_vsync(struct mdss_mdp_ctl *ctl,
						ktime_t t)
{
	struct msm_fb_data_type *mfd = NULL;
	struct mdss_overlay_private *mdp5_data = NULL;

	if (!ctl) {
		pr_err("ctl is NULL\n");
		return;
	}

	mfd = ctl->mfd;
	if (!mfd || !mfd->mdp.private1) {
		pr_warn("Invalid handle for vsync\n");
		return;
	}

	mdp5_data = mfd_to_mdp5_data(mfd);
	if (!mdp5_data) {
		pr_err("mdp5_data is NULL\n");
		return;
	}

	pr_debug("vsync on fb%d play_cnt=%d\n", mfd->index, ctl->play_cnt);

	mdp5_data->vsync_time = t;
	sysfs_notify_dirent(mdp5_data->vsync_event_sd);
}

int mdss_mdp_overlay_vsync_ctrl(struct msm_fb_data_type *mfd, int en)
{
	struct mdss_mdp_ctl *ctl = mfd_to_ctl(mfd);
	int rc;

	if (!ctl)
		return -ENODEV;
	if (!ctl->add_vsync_handler || !ctl->remove_vsync_handler)
		return -EOPNOTSUPP;
	if (!ctl->panel_data->panel_info.cont_splash_enabled
			&& !mdss_mdp_ctl_is_power_on(ctl)) {
		pr_debug("fb%d vsync pending first update en=%d\n",
				mfd->index, en);
		return -EPERM;
	}

	pr_debug("fb%d vsync en=%d\n", mfd->index, en);

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);
	if (en)
		rc = ctl->add_vsync_handler(ctl, &ctl->vsync_handler);
	else
		rc = ctl->remove_vsync_handler(ctl, &ctl->vsync_handler);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);

	return rc;
}

static ssize_t dynamic_fps_sysfs_rda_dfps(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	struct mdss_panel_data *pdata;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);

	if (!mdp5_data->ctl || !mdss_mdp_ctl_is_power_on(mdp5_data->ctl))
		return 0;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	if (!pdata) {
		pr_err("no panel connected for fb%d\n", mfd->index);
		return -ENODEV;
	}

	mutex_lock(&mdp5_data->dfps_lock);
	ret = snprintf(buf, PAGE_SIZE, "%d\n",
		       pdata->panel_info.mipi.frame_rate);
	pr_debug("%s: '%d'\n", __func__,
		pdata->panel_info.mipi.frame_rate);
	mutex_unlock(&mdp5_data->dfps_lock);

	return ret;
} /* dynamic_fps_sysfs_rda_dfps */

static ssize_t dynamic_fps_sysfs_wta_dfps(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int dfps, rc = 0;
	struct mdss_panel_data *pdata;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);

	rc = kstrtoint(buf, 10, &dfps);
	if (rc) {
		pr_err("%s: kstrtoint failed. rc=%d\n", __func__, rc);
		return rc;
	}

	if (!mdp5_data->ctl || !mdss_mdp_ctl_is_power_on(mdp5_data->ctl))
		return 0;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	if (!pdata) {
		pr_err("no panel connected for fb%d\n", mfd->index);
		return -ENODEV;
	}

	if (dfps == pdata->panel_info.mipi.frame_rate) {
		pr_debug("%s: FPS is already %d\n",
			__func__, dfps);
		return count;
	}

	mutex_lock(&mdp5_data->dfps_lock);
	if (dfps < pdata->panel_info.min_fps) {
		pr_err("Unsupported FPS. min_fps = %d\n",
				pdata->panel_info.min_fps);
		mutex_unlock(&mdp5_data->dfps_lock);
		return -EINVAL;
	} else if (dfps > pdata->panel_info.max_fps) {
		pr_warn("Unsupported FPS. Configuring to max_fps = %d\n",
				pdata->panel_info.max_fps);
		dfps = pdata->panel_info.max_fps;
		rc = mdss_mdp_ctl_update_fps(mdp5_data->ctl, dfps);
	} else {
		rc = mdss_mdp_ctl_update_fps(mdp5_data->ctl, dfps);
	}
	if (!rc) {
		pr_debug("%s: configured to '%d' FPS\n", __func__, dfps);
	} else {
		pr_err("Failed to configure '%d' FPS. rc = %d\n",
							dfps, rc);
		mutex_unlock(&mdp5_data->dfps_lock);
		return rc;
	}
	pdata->panel_info.new_fps = dfps;
	mutex_unlock(&mdp5_data->dfps_lock);
	return count;
} /* dynamic_fps_sysfs_wta_dfps */


static DEVICE_ATTR(dynamic_fps, S_IRUGO | S_IWUSR, dynamic_fps_sysfs_rda_dfps,
	dynamic_fps_sysfs_wta_dfps);

static struct attribute *dynamic_fps_fs_attrs[] = {
	&dev_attr_dynamic_fps.attr,
	NULL,
};
static struct attribute_group dynamic_fps_fs_attrs_group = {
	.attrs = dynamic_fps_fs_attrs,
};

static ssize_t mdss_mdp_vsync_show_event(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	u64 vsync_ticks;
	int ret;

	if (!mdp5_data->ctl ||
		(!mdp5_data->ctl->panel_data->panel_info.cont_splash_enabled
			&& !mdss_mdp_ctl_is_power_on(mdp5_data->ctl)))
		return -EAGAIN;

	vsync_ticks = ktime_to_ns(mdp5_data->vsync_time);

	pr_debug("fb%d vsync=%llu\n", mfd->index, vsync_ticks);
	ret = scnprintf(buf, PAGE_SIZE, "VSYNC=%llu\n", vsync_ticks);

	return ret;
}

static inline int mdss_mdp_ad_is_supported(struct msm_fb_data_type *mfd)
{
	struct mdss_mdp_ctl *ctl = mfd_to_ctl(mfd);
	struct mdss_mdp_mixer *mixer;

	if (!ctl) {
		pr_debug("there is no ctl attached to fb\n");
		return 0;
	}

	mixer = mdss_mdp_mixer_get(ctl, MDSS_MDP_MIXER_MUX_LEFT);
	if (mixer && (mixer->num > ctl->mdata->nad_cfgs)) {
		if (!mixer)
			pr_warn("there is no mixer attached to fb\n");
		else
			pr_debug("mixer attached (%d) doesnt support ad\n",
				 mixer->num);
		return 0;
	}

	mixer = mdss_mdp_mixer_get(ctl, MDSS_MDP_MIXER_MUX_RIGHT);
	if (mixer && (mixer->num > ctl->mdata->nad_cfgs))
		return 0;

	return 1;
}

static ssize_t mdss_mdp_ad_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = fbi->par;
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	int ret, state;

	state = mdss_mdp_ad_is_supported(mfd) ? mdp5_data->ad_state : -1;

	ret = scnprintf(buf, PAGE_SIZE, "%d", state);

	return ret;
}

static ssize_t mdss_mdp_ad_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = fbi->par;
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	int ret, ad;

	ret = kstrtoint(buf, 10, &ad);
	if (ret) {
		pr_err("Invalid input for ad\n");
		return -EINVAL;
	}

	mdp5_data->ad_state = ad;
	sysfs_notify(&dev->kobj, NULL, "ad");

	return count;
}

static ssize_t mdss_mdp_dyn_pu_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = fbi->par;
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	int ret, state;

	state = (mdp5_data->dyn_pu_state >= 0) ? mdp5_data->dyn_pu_state : -1;

	ret = scnprintf(buf, PAGE_SIZE, "%d", state);

	return ret;
}

static ssize_t mdss_mdp_dyn_pu_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = fbi->par;
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	int ret, dyn_pu;

	ret = kstrtoint(buf, 10, &dyn_pu);
	if (ret) {
		pr_err("Invalid input for partial udpate: ret = %d\n", ret);
		return ret;
	}

	mdp5_data->dyn_pu_state = dyn_pu;
	sysfs_notify(&dev->kobj, NULL, "dyn_pu");

	return count;
}

static DEVICE_ATTR(vsync_event, S_IRUGO, mdss_mdp_vsync_show_event, NULL);
static DEVICE_ATTR(ad, S_IRUGO | S_IWUSR | S_IWGRP, mdss_mdp_ad_show,
	mdss_mdp_ad_store);
static DEVICE_ATTR(dyn_pu, S_IRUGO | S_IWUSR | S_IWGRP, mdss_mdp_dyn_pu_show,
	mdss_mdp_dyn_pu_store);

static struct attribute *mdp_overlay_sysfs_attrs[] = {
	&dev_attr_vsync_event.attr,
	&dev_attr_ad.attr,
	&dev_attr_dyn_pu.attr,
	NULL,
};

static struct attribute_group mdp_overlay_sysfs_group = {
	.attrs = mdp_overlay_sysfs_attrs,
};

static void mdss_mdp_hw_cursor_setpos(struct mdss_mdp_mixer *mixer,
		struct mdss_rect *roi, u32 start_x, u32 start_y)
{
	int roi_xy = (roi->y << 16) | roi->x;
	int start_xy = (start_y << 16) | start_x;
	int roi_size = (roi->h << 16) | roi->w;

	if (!mixer) {
		pr_err("mixer not available\n");
		return;
	}
	mdp_mixer_write(mixer, MDSS_MDP_REG_LM_CURSOR_XY, roi_xy);
	mdp_mixer_write(mixer, MDSS_MDP_REG_LM_CURSOR_START_XY, start_xy);
	mdp_mixer_write(mixer, MDSS_MDP_REG_LM_CURSOR_SIZE, roi_size);
}

static void mdss_mdp_hw_cursor_setimage(struct mdss_mdp_mixer *mixer,
	struct fb_cursor *cursor, u32 cursor_addr, struct mdss_rect *roi)
{
	int calpha_en, transp_en, alpha, size;
	struct fb_image *img = &cursor->image;
	u32 blendcfg;
	int roi_size = 0;

	if (!mixer) {
		pr_err("mixer not available\n");
		return;
	}

	if (img->bg_color == 0xffffffff)
		transp_en = 0;
	else
		transp_en = 1;

	alpha = (img->fg_color & 0xff000000) >> 24;

	if (alpha)
		calpha_en = 0x0; /* xrgb */
	else
		calpha_en = 0x2; /* argb */

	roi_size = (roi->h << 16) | roi->w;
	size = (img->height << 16) | img->width;
	mdp_mixer_write(mixer, MDSS_MDP_REG_LM_CURSOR_IMG_SIZE, size);
	mdp_mixer_write(mixer, MDSS_MDP_REG_LM_CURSOR_SIZE, roi_size);
	mdp_mixer_write(mixer, MDSS_MDP_REG_LM_CURSOR_STRIDE,
				img->width * 4);
	mdp_mixer_write(mixer, MDSS_MDP_REG_LM_CURSOR_BASE_ADDR,
				cursor_addr);
	blendcfg = mdp_mixer_read(mixer, MDSS_MDP_REG_LM_CURSOR_BLEND_CONFIG);
	blendcfg &= ~0x1;
	blendcfg |= (transp_en << 3) | (calpha_en << 1);
	mdp_mixer_write(mixer, MDSS_MDP_REG_LM_CURSOR_BLEND_CONFIG,
				blendcfg);
	if (calpha_en)
		mdp_mixer_write(mixer, MDSS_MDP_REG_LM_CURSOR_BLEND_PARAM,
				alpha);

	if (transp_en) {
		mdp_mixer_write(mixer,
				MDSS_MDP_REG_LM_CURSOR_BLEND_TRANSP_LOW0,
				((img->bg_color & 0xff00) << 8) |
				(img->bg_color & 0xff));
		mdp_mixer_write(mixer,
				MDSS_MDP_REG_LM_CURSOR_BLEND_TRANSP_LOW1,
				((img->bg_color & 0xff0000) >> 16));
		mdp_mixer_write(mixer,
				MDSS_MDP_REG_LM_CURSOR_BLEND_TRANSP_HIGH0,
				((img->bg_color & 0xff00) << 8) |
				(img->bg_color & 0xff));
		mdp_mixer_write(mixer,
				MDSS_MDP_REG_LM_CURSOR_BLEND_TRANSP_HIGH1,
				((img->bg_color & 0xff0000) >> 16));
	}
}

static void mdss_mdp_hw_cursor_blend_config(struct mdss_mdp_mixer *mixer,
		struct fb_cursor *cursor)
{
	u32 blendcfg;
	if (!mixer) {
		pr_err("mixer not availbale\n");
		return;
	}

	blendcfg = mdp_mixer_read(mixer, MDSS_MDP_REG_LM_CURSOR_BLEND_CONFIG);
	if (!cursor->enable != !(blendcfg & 0x1)) {
		if (cursor->enable) {
			pr_debug("enable hw cursor on mixer=%d\n", mixer->num);
			blendcfg |= 0x1;
		} else {
			pr_debug("disable hw cursor on mixer=%d\n", mixer->num);
			blendcfg &= ~0x1;
		}

		mdp_mixer_write(mixer, MDSS_MDP_REG_LM_CURSOR_BLEND_CONFIG,
				   blendcfg);
		mixer->cursor_enabled = cursor->enable;
		mixer->params_changed++;
	}

}

void mdss_mdp_set_rect(struct mdp_rect *rect, u16 x, u16 y, u16 w, u16 h)
{
	rect->x = x;
	rect->y = y;
	rect->w = w;
	rect->h = h;
}

void mdss_mdp_curor_pipe_cleanup(struct msm_fb_data_type *mfd, int cursor_pipe)
{
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);

	if (mdp5_data->cursor_ndx[cursor_pipe] != MSMFB_NEW_REQUEST) {
		mdss_mdp_overlay_release(mfd,
				mdp5_data->cursor_ndx[cursor_pipe]);
		mdp5_data->cursor_ndx[cursor_pipe] = MSMFB_NEW_REQUEST;
	}
}

int mdss_mdp_cursor_pipe_setup(struct msm_fb_data_type *mfd,
		struct mdp_overlay *req, int cursor_pipe) {
	struct mdss_mdp_pipe *pipe;
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	int ret = 0;

	req->id = mdp5_data->cursor_ndx[cursor_pipe];
	ret = mdss_mdp_overlay_pipe_setup(mfd, req, &pipe, NULL, false);
	if (ret) {
		pr_err("cursor pipe setup failed, cursor_pipe:%d, ret:%d",
				cursor_pipe, ret);
		mdp5_data->cursor_ndx[cursor_pipe] = MSMFB_NEW_REQUEST;
		return ret;
	}
	mdp5_data->cursor_ndx[cursor_pipe] = req->id;
	return ret;
}

static int mdss_mdp_hw_cursor_pipe_update(struct msm_fb_data_type *mfd,
				     struct fb_cursor *cursor)
{
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	struct mdss_mdp_mixer *mixer;
	struct fb_image *img = &cursor->image;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdp_overlay req;
	struct mdss_rect roi;
	int ret = 0;
	u32 xres = mfd->fbi->var.xres;
	u32 yres = mfd->fbi->var.yres;
	u32 start_x = img->dx;
	u32 start_y = img->dy;
	u32 left_lm_w = left_lm_w_from_mfd(mfd);

	ret = mutex_lock_interruptible(&mdp5_data->ov_lock);
	if (ret)
		return ret;

	if (mdss_fb_is_power_off(mfd)) {
		ret = -EPERM;
		goto done;
	}

	if (!cursor->enable) {
		mdss_mdp_curor_pipe_cleanup(mfd, CURSOR_PIPE_LEFT);
		mdss_mdp_curor_pipe_cleanup(mfd, CURSOR_PIPE_RIGHT);
		goto done;
	}

	mixer = mdss_mdp_mixer_get(mdp5_data->ctl, MDSS_MDP_MIXER_MUX_DEFAULT);
	if (!mixer) {
		ret = -ENODEV;
		goto done;
	}

	if (!mfd->cursor_buf && (cursor->set & FB_CUR_SETIMAGE)) {
		mfd->cursor_buf = dma_alloc_coherent(&mfd->pdev->dev,
			MDSS_MDP_CURSOR_SIZE, (dma_addr_t *)
			&mfd->cursor_buf_phys, GFP_KERNEL);
		if (!mfd->cursor_buf) {
			pr_err("can't allocate cursor buffer\n");
			ret = -ENOMEM;
			goto done;
		}

		ret = msm_iommu_map_contig_buffer(mfd->cursor_buf_phys,
			mdss_get_iommu_domain(MDSS_IOMMU_DOMAIN_UNSECURE),
			0, MDSS_MDP_CURSOR_SIZE, SZ_4K, 0,
			&(mfd->cursor_buf_iova));
		if (IS_ERR_VALUE(ret)) {
			dma_free_coherent(&mfd->pdev->dev, MDSS_MDP_CURSOR_SIZE,
					  mfd->cursor_buf,
					  (dma_addr_t) mfd->cursor_buf_phys);
			pr_err("unable to map cursor buffer to iommu(%d)\n",
			       ret);
			goto done;
		}

		mixer->cursor_hotx = 0;
		mixer->cursor_hoty = 0;
	}

	if ((img->width > MDSS_MDP_CURSOR_WIDTH) ||
		(img->height > MDSS_MDP_CURSOR_HEIGHT) ||
		(img->depth != 32) || (start_x >= xres) || (start_y >= yres)) {
		ret = -EINVAL;
		goto done;
	}

	pr_debug("mixer=%d enable=%x set=%x\n", mixer->num, cursor->enable,
			cursor->set);

	if (cursor->set & FB_CUR_SETHOT) {
		if ((cursor->hot.x < img->width) &&
			(cursor->hot.y < img->height)) {
			mixer->cursor_hotx = cursor->hot.x;
			mixer->cursor_hoty = cursor->hot.y;
			 /* Update cursor position */
			cursor->set |= FB_CUR_SETPOS;
		} else {
			pr_err("Invalid cursor hotspot coordinates\n");
			ret = -EINVAL;
			goto done;
		}
	}

	memset(&roi, 0, sizeof(struct mdss_rect));
	if (start_x > mixer->cursor_hotx) {
		start_x -= mixer->cursor_hotx;
	} else {
		roi.x = mixer->cursor_hotx - start_x;
		start_x = 0;
	}
	if (start_y > mixer->cursor_hoty) {
		start_y -= mixer->cursor_hoty;
	} else {
		roi.y = mixer->cursor_hoty - start_y;
		start_y = 0;
	}

	roi.w = min(xres - start_x, img->width - roi.x);
	roi.h = min(yres - start_y, img->height - roi.y);

	memset(&req, 0, sizeof(struct mdp_overlay));
	req.pipe_type = PIPE_TYPE_CURSOR;
	req.z_order = MDSS_MDP_STAGE_6;

	req.src.width = img->width;
	req.src.height = img->height;
	req.src.format = MDP_ARGB_8888;

	mdss_mdp_set_rect(&req.src_rect, roi.x, roi.y, img->width, img->height);
	mdss_mdp_set_rect(&req.dst_rect, start_x, start_y, roi.w, roi.h);

	req.bg_color = img->bg_color;
	req.alpha = (img->fg_color & 0xff000000) >> 24;
	if (req.alpha == 0xff)
		req.blend_op = BLEND_OP_OPAQUE;
	else
		req.blend_op = BLEND_OP_COVERAGE;
	req.transp_mask = (img->bg_color & 0xffffff);

	if (mfd->cursor_buf && (cursor->set & FB_CUR_SETIMAGE)) {
		u32 cursor_addr;
		ret = copy_from_user(mfd->cursor_buf, img->data,
				     img->width * img->height * 4);
		if (ret) {
			pr_err("copy_from_user error. rc=%d\n", ret);
			goto done;
		}

		if (mdata->mdss_util->iommu_attached()) {
			cursor_addr = mfd->cursor_buf_iova;
		} else {
			if (MDSS_LPAE_CHECK(mfd->cursor_buf_phys)) {
				pr_err("can't access phy mem >4GB w/o iommu\n");
				ret = -ERANGE;
				goto done;
			}
			cursor_addr = mfd->cursor_buf_phys;
		}

		mixer->cursor_hotx = 0;
		mixer->cursor_hoty = 0;
		mdp5_data->cursor_ndx[CURSOR_PIPE_LEFT] = MSMFB_NEW_REQUEST;
		mdp5_data->cursor_ndx[CURSOR_PIPE_RIGHT] = MSMFB_NEW_REQUEST;
	}

	if (start_x + roi.w <= left_lm_w) {
		ret = mdss_mdp_cursor_pipe_setup(mfd, &req, CURSOR_PIPE_LEFT);
		mdss_mdp_curor_pipe_cleanup(mfd, CURSOR_PIPE_RIGHT);
	} else if (start_x >= left_lm_w) {
		ret = mdss_mdp_cursor_pipe_setup(mfd, &req, CURSOR_PIPE_RIGHT);
		mdss_mdp_curor_pipe_cleanup(mfd, CURSOR_PIPE_LEFT);
	} else {
		mdss_mdp_set_rect(&req.dst_rect, start_x, start_y,
				(left_lm_w - start_x), roi.h);
		mdss_mdp_set_rect(&req.src_rect, 0, 0, (left_lm_w -
				start_x), img->height);
		ret = mdss_mdp_cursor_pipe_setup(mfd, &req, CURSOR_PIPE_LEFT);
		if (ret)
			goto done;

		mdss_mdp_set_rect(&req.dst_rect, left_lm_w, start_y, ((start_x +
				roi.w) - left_lm_w), roi.h);
		mdss_mdp_set_rect(&req.src_rect, (left_lm_w - start_x), 0,
				(img->width - (left_lm_w - start_x)),
				img->height);
		ret = mdss_mdp_cursor_pipe_setup(mfd, &req, CURSOR_PIPE_RIGHT);
		if (ret)
			goto done;
	}

done:
	mutex_unlock(&mdp5_data->ov_lock);
	return ret;
}

static int mdss_mdp_hw_cursor_update(struct msm_fb_data_type *mfd,
				     struct fb_cursor *cursor)
{
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	struct mdss_mdp_mixer *mixer_left = NULL;
	struct mdss_mdp_mixer *mixer_right = NULL;
	struct fb_image *img = &cursor->image;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct fbcurpos cursor_hot;
	struct mdss_rect roi;
	int ret = 0;
	u32 xres = mfd->fbi->var.xres;
	u32 yres = mfd->fbi->var.yres;
	u32 start_x = img->dx;
	u32 start_y = img->dy;
	u32 left_lm_w = left_lm_w_from_mfd(mfd);

	mixer_left = mdss_mdp_mixer_get(mdp5_data->ctl,
			MDSS_MDP_MIXER_MUX_DEFAULT);
	if (!mixer_left)
		return -ENODEV;
	if (is_split_lm(mfd)) {
		mixer_right = mdss_mdp_mixer_get(mdp5_data->ctl,
				MDSS_MDP_MIXER_MUX_RIGHT);
		if (!mixer_right)
			return -ENODEV;
	}

	if (!mfd->cursor_buf && (cursor->set & FB_CUR_SETIMAGE)) {
		mfd->cursor_buf = dma_alloc_coherent(NULL, MDSS_MDP_CURSOR_SIZE,
					(dma_addr_t *) &mfd->cursor_buf_phys,
					GFP_KERNEL);
		if (!mfd->cursor_buf) {
			pr_err("can't allocate cursor buffer\n");
			return -ENOMEM;
		}

		ret = msm_iommu_map_contig_buffer(mfd->cursor_buf_phys,
			mdss_get_iommu_domain(MDSS_IOMMU_DOMAIN_UNSECURE),
			0, MDSS_MDP_CURSOR_SIZE, SZ_4K, 0,
			&(mfd->cursor_buf_iova));
		if (IS_ERR_VALUE(ret)) {
			dma_free_coherent(NULL, MDSS_MDP_CURSOR_SIZE,
					  mfd->cursor_buf,
					  (dma_addr_t) mfd->cursor_buf_phys);
			pr_err("unable to map cursor buffer to iommu(%d)\n",
			       ret);
			return ret;
		}
	}

	if ((img->width > MDSS_MDP_CURSOR_WIDTH) ||
		(img->height > MDSS_MDP_CURSOR_HEIGHT) ||
		(img->depth != 32) || (start_x >= xres) || (start_y >= yres))
		return -EINVAL;

	pr_debug("enable=%x set=%x\n", cursor->enable, cursor->set);

	memset(&cursor_hot, 0, sizeof(struct fbcurpos));
	memset(&roi, 0, sizeof(struct mdss_rect));
	if (cursor->set & FB_CUR_SETHOT) {
		if ((cursor->hot.x < img->width) &&
			(cursor->hot.y < img->height)) {
			cursor_hot.x = cursor->hot.x;
			cursor_hot.y = cursor->hot.y;
			 /* Update cursor position */
			cursor->set |= FB_CUR_SETPOS;
		} else {
			pr_err("Invalid cursor hotspot coordinates\n");
			return -EINVAL;
		}
	}

	if (start_x > cursor_hot.x) {
		start_x -= cursor_hot.x;
	} else {
		roi.x = cursor_hot.x - start_x;
		start_x = 0;
	}
	if (start_y > cursor_hot.y) {
		start_y -= cursor_hot.y;
	} else {
		roi.y = cursor_hot.y - start_y;
		start_y = 0;
	}

	roi.w = min(xres - start_x, img->width - roi.x);
	roi.h = min(yres - start_y, img->height - roi.y);

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);

	if (cursor->set & FB_CUR_SETIMAGE) {
		u32 cursor_addr;
		ret = copy_from_user(mfd->cursor_buf, img->data,
				     img->width * img->height * 4);
		if (ret) {
			pr_err("copy_from_user error. rc=%d\n", ret);
			mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
			return ret;
		}

		if (mdata->mdss_util->iommu_attached()) {
			cursor_addr = mfd->cursor_buf_iova;
		} else {
			if (MDSS_LPAE_CHECK(mfd->cursor_buf_phys)) {
				pr_err("can't access phy mem >4GB w/o iommu\n");
				mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
				return -ERANGE;
			}
			cursor_addr = mfd->cursor_buf_phys;
		}
		mdss_mdp_hw_cursor_setimage(mixer_left, cursor, cursor_addr,
				&roi);
		if (is_split_lm(mfd))
			mdss_mdp_hw_cursor_setimage(mixer_right, cursor,
					cursor_addr, &roi);
	}

	if ((start_x + roi.w) <= left_lm_w) {
		if (cursor->set & FB_CUR_SETPOS)
			mdss_mdp_hw_cursor_setpos(mixer_left, &roi, start_x,
					start_y);
		mdss_mdp_hw_cursor_blend_config(mixer_left, cursor);
		cursor->enable = false;
		mdss_mdp_hw_cursor_blend_config(mixer_right, cursor);
	} else if (start_x >= left_lm_w) {
		start_x -= left_lm_w;
		if (cursor->set & FB_CUR_SETPOS)
			mdss_mdp_hw_cursor_setpos(mixer_right, &roi, start_x,
					start_y);
		mdss_mdp_hw_cursor_blend_config(mixer_right, cursor);
		cursor->enable = false;
		mdss_mdp_hw_cursor_blend_config(mixer_left, cursor);
	} else {
		struct mdss_rect roi_right = roi;
		roi.w = left_lm_w - start_x;
		if (cursor->set & FB_CUR_SETPOS)
			mdss_mdp_hw_cursor_setpos(mixer_left, &roi, start_x,
					start_y);
		mdss_mdp_hw_cursor_blend_config(mixer_left, cursor);

		roi_right.x = 0;
		roi_right.w = (start_x + roi_right.w) - left_lm_w;
		start_x = 0;
		if (cursor->set & FB_CUR_SETPOS)
			mdss_mdp_hw_cursor_setpos(mixer_right, &roi_right,
					start_x, start_y);
		mdss_mdp_hw_cursor_blend_config(mixer_right, cursor);
	}

	mixer_left->ctl->flush_bits |= BIT(6) << mixer_left->num;
	if (is_split_lm(mfd))
		mixer_right->ctl->flush_bits |= BIT(6) << mixer_right->num;
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
	return 0;
}

static int mdss_bl_scale_config(struct msm_fb_data_type *mfd,
						struct mdp_bl_scale_data *data)
{
	int ret = 0;
	int curr_bl;
	mutex_lock(&mfd->bl_lock);
	curr_bl = mfd->bl_level;
	mfd->bl_scale = data->scale;
	mfd->bl_min_lvl = data->min_lvl;
	pr_debug("update scale = %d, min_lvl = %d\n", mfd->bl_scale,
							mfd->bl_min_lvl);

	/* update current backlight to use new scaling*/
	mdss_fb_set_backlight(mfd, curr_bl);
	mutex_unlock(&mfd->bl_lock);
	return ret;
}

static int mdss_mdp_pp_ioctl(struct msm_fb_data_type *mfd,
				void __user *argp)
{
	int ret;
	struct msmfb_mdp_pp mdp_pp;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	u32 copyback = 0;
	u32 copy_from_kernel = 0;

	if (!mdata)
		return -EPERM;

	ret = copy_from_user(&mdp_pp, argp, sizeof(mdp_pp));
	if (ret)
		return ret;

	/* Supprt only MDP register read/write and
	exit_dcm in DCM state*/
	if (mfd->dcm_state == DCM_ENTER &&
			(mdp_pp.op != mdp_op_calib_buffer &&
			mdp_pp.op != mdp_op_calib_dcm_state))
		return -EPERM;

	switch (mdp_pp.op) {
	case mdp_op_pa_cfg:
		ret = mdss_mdp_pa_config(&mdp_pp.data.pa_cfg_data,
					&copyback);
		break;

	case mdp_op_pa_v2_cfg:
		ret = mdss_mdp_pa_v2_config(&mdp_pp.data.pa_v2_cfg_data,
					&copyback);
		break;

	case mdp_op_pcc_cfg:
		ret = mdss_mdp_pcc_config(&mdp_pp.data.pcc_cfg_data,
					&copyback);
		break;

	case mdp_op_lut_cfg:
		switch (mdp_pp.data.lut_cfg_data.lut_type) {
		case mdp_lut_igc:
			ret = mdss_mdp_igc_lut_config(
					(struct mdp_igc_lut_data *)
					&mdp_pp.data.lut_cfg_data.data,
					&copyback, copy_from_kernel);
			break;

		case mdp_lut_pgc:
			ret = mdss_mdp_argc_config(
				&mdp_pp.data.lut_cfg_data.data.pgc_lut_data,
				&copyback);
			break;

		case mdp_lut_hist:
			ret = mdss_mdp_hist_lut_config(
				(struct mdp_hist_lut_data *)
				&mdp_pp.data.lut_cfg_data.data, &copyback);
			break;

		default:
			ret = -ENOTSUPP;
			break;
		}
		break;
	case mdp_op_dither_cfg:
		ret = mdss_mdp_dither_config(
				&mdp_pp.data.dither_cfg_data,
				&copyback);
		break;
	case mdp_op_gamut_cfg:
		ret = mdss_mdp_gamut_config(
				&mdp_pp.data.gamut_cfg_data,
				&copyback);
		break;
	case mdp_bl_scale_cfg:
		ret = mdss_bl_scale_config(mfd, (struct mdp_bl_scale_data *)
						&mdp_pp.data.bl_scale_data);
		break;
	case mdp_op_ad_cfg:
		ret = mdss_mdp_ad_config(mfd, &mdp_pp.data.ad_init_cfg);
		break;
	case mdp_op_ad_input:
		ret = mdss_mdp_ad_input(mfd, &mdp_pp.data.ad_input, 1);
		if (ret > 0) {
			ret = 0;
			copyback = 1;
		}
		break;
	case mdp_op_calib_cfg:
		ret = mdss_mdp_calib_config((struct mdp_calib_config_data *)
					 &mdp_pp.data.calib_cfg, &copyback);
		break;
	case mdp_op_calib_mode:
		ret = mdss_mdp_calib_mode(mfd, &mdp_pp.data.mdss_calib_cfg);
		break;
	case mdp_op_calib_buffer:
		ret = mdss_mdp_calib_config_buffer(
				(struct mdp_calib_config_buffer *)
				 &mdp_pp.data.calib_buffer, &copyback);
		break;
	case mdp_op_calib_dcm_state:
		ret = mdss_fb_dcm(mfd, mdp_pp.data.calib_dcm.dcm_state);
		break;
	case mdp_op_pp_init_cfg:
		ret = mdss_mdp_pp_override_pu(
				mdp_pp.data.init_data.init_request);
		break;
	default:
		pr_err("Unsupported request to MDP_PP IOCTL. %d = op\n",
								mdp_pp.op);
		ret = -EINVAL;
		break;
	}
	if ((ret == 0) && copyback)
		ret = copy_to_user(argp, &mdp_pp, sizeof(struct msmfb_mdp_pp));
	return ret;
}

static int mdss_mdp_histo_ioctl(struct msm_fb_data_type *mfd, u32 cmd,
				void __user *argp)
{
	int ret = -ENOSYS;
	struct mdp_histogram_data hist;
	struct mdp_histogram_start_req hist_req;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	u32 block;
	static int req = -1;

	if (!mdata)
		return -EPERM;

	switch (cmd) {
	case MSMFB_HISTOGRAM_START:
		if (mdss_fb_is_power_off(mfd))
			return -EPERM;

		if (mdata->needs_hist_vote && (mdata->reg_bus_hdl)) {
			req = msm_bus_scale_client_update_request(
					mdata->reg_bus_hdl,
					REG_CLK_CFG_LOW);
			if (req)
				pr_err("Updated pp_bus_scale failed, ret = %d",
						req);
		}

		ret = copy_from_user(&hist_req, argp, sizeof(hist_req));
		if (ret)
			return ret;

		ret = mdss_mdp_hist_start(&hist_req);
		break;

	case MSMFB_HISTOGRAM_STOP:
		ret = copy_from_user(&block, argp, sizeof(int));
		if (ret)
			return ret;

		ret = mdss_mdp_hist_stop(block);
		if (ret)
			return ret;

		if (mdata->needs_hist_vote && (mdata->reg_bus_hdl && !req)) {
			req = msm_bus_scale_client_update_request(
				mdata->reg_bus_hdl,
				REG_CLK_CFG_OFF);
			if (req)
				pr_err("Updated pp_bus_scale failed, ret = %d",
					req);
		}
		break;

	case MSMFB_HISTOGRAM:
		if (mdss_fb_is_power_off(mfd))
			return -EPERM;

		ret = copy_from_user(&hist, argp, sizeof(hist));
		if (ret)
			return ret;

		ret = mdss_mdp_hist_collect(&hist);
		if (!ret)
			ret = copy_to_user(argp, &hist, sizeof(hist));
		break;
	default:
		break;
	}
	return ret;
}

static int mdss_fb_set_metadata(struct msm_fb_data_type *mfd,
				struct msmfb_metadata *metadata)
{
	struct mdss_data_type *mdata = mfd_to_mdata(mfd);
	struct mdss_mdp_ctl *ctl = mfd_to_ctl(mfd);
	int ret = 0;
	if (!ctl)
		return  -EPERM;
	switch (metadata->op) {
	case metadata_op_vic:
		if (mfd->panel_info)
			mfd->panel_info->vic =
				metadata->data.video_info_code;
		else
			ret = -EINVAL;
		break;
	case metadata_op_crc:
		if (mdss_fb_is_power_off(mfd))
			return -EPERM;
		ret = mdss_misr_set(mdata, &metadata->data.misr_request, ctl);
		break;
	case metadata_op_wb_format:
		ret = mdss_mdp_wb_set_format(mfd,
				metadata->data.mixer_cfg.writeback_format);
		break;
	case metadata_op_wb_secure:
		ret = mdss_mdp_wb_set_secure(mfd, metadata->data.secure_en);
		break;
	default:
		pr_warn("unsupported request to MDP META IOCTL\n");
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int mdss_fb_get_hw_caps(struct msm_fb_data_type *mfd,
		struct mdss_hw_caps *caps)
{
	struct mdss_data_type *mdata = mfd_to_mdata(mfd);
	caps->mdp_rev = mdata->mdp_rev;
	caps->vig_pipes = mdata->nvig_pipes;
	caps->rgb_pipes = mdata->nrgb_pipes;
	caps->dma_pipes = mdata->ndma_pipes;
	if (mdata->has_bwc)
		caps->features |= MDP_BWC_EN;
	if (mdata->has_decimation)
		caps->features |= MDP_DECIMATION_EN;

	if (mdata->smp_mb_cnt) {
		caps->max_smp_cnt = mdata->smp_mb_cnt;
		caps->smp_per_pipe = mdata->smp_mb_per_pipe;
	}

	return 0;
}

static int mdss_fb_get_metadata(struct msm_fb_data_type *mfd,
				struct msmfb_metadata *metadata)
{
	struct mdss_data_type *mdata = mfd_to_mdata(mfd);
	struct mdss_mdp_ctl *ctl = mfd_to_ctl(mfd);
	int ret = 0;
	if (!ctl)
		return -EPERM;
	switch (metadata->op) {
	case metadata_op_frame_rate:
		metadata->data.panel_frame_rate =
			mdss_panel_get_framerate(mfd->panel_info);
		break;
	case metadata_op_get_caps:
		ret = mdss_fb_get_hw_caps(mfd, &metadata->data.caps);
		break;
	case metadata_op_get_ion_fd:
		if (mfd->fb_ion_handle) {
			metadata->data.fbmem_ionfd =
				dma_buf_fd(mfd->fbmem_buf, 0);
			if (metadata->data.fbmem_ionfd < 0)
				pr_err("fd allocation failed. fd = %d\n",
						metadata->data.fbmem_ionfd);
		}
		break;
	case metadata_op_crc:
		if (mdss_fb_is_power_off(mfd))
			return -EPERM;
		ret = mdss_misr_get(mdata, &metadata->data.misr_request, ctl);
		break;
	case metadata_op_wb_format:
		ret = mdss_mdp_wb_get_format(mfd, &metadata->data.mixer_cfg);
		break;
	case metadata_op_wb_secure:
		ret = mdss_mdp_wb_get_secure(mfd, &metadata->data.secure_en);
		break;
	default:
		pr_warn("Unsupported request to MDP META IOCTL.\n");
		ret = -EINVAL;
		break;
	}
	return ret;
}

/*
 * This routine serves two purposes.
 * 1. Propagate overlay_id returned from sorted list to original list
 *    to user-space.
 * 2. In case of error processing sorted list, map the error overlay's
 *    index to original list because user-space is not aware of the sorted list.
 */
static int __mdss_overlay_map(struct mdp_overlay *ovs,
	struct mdp_overlay *op_ovs, int num_ovs, int num_ovs_processed)
{
	int i = num_ovs_processed, j, k;

	for (j = 0; j < num_ovs; j++) {
		for (k = 0; k < num_ovs; k++) {
			if ((ovs[j].dst_rect.x == op_ovs[k].dst_rect.x) &&
			    (ovs[j].z_order == op_ovs[k].z_order)) {
				op_ovs[k].id = ovs[j].id;
				op_ovs[k].priority = ovs[j].priority;
				break;
			}
		}
		if ((i != num_ovs) && (i != j) &&
		    (ovs[j].dst_rect.x == op_ovs[k].dst_rect.x) &&
		    (ovs[i].z_order == op_ovs[k].z_order)) {
			pr_debug("mapped %d->%d\n", i, j);
			i = j;
		}
	}

	return i;
}

static inline void __overlay_swap_func(void *a, void *b, int size)
{
	swap(*(struct mdp_overlay *)a, *(struct mdp_overlay *)b);
}

static inline int __zorder_dstx_cmp_func(const void *a, const void *b)
{
	int rc = 0;
	const struct mdp_overlay *ov1 = a;
	const struct mdp_overlay *ov2 = b;

	if (ov1->z_order < ov2->z_order)
		rc = -1;
	else if ((ov1->z_order == ov2->z_order) &&
		 (ov1->dst_rect.x < ov2->dst_rect.x))
		rc = -1;

	return rc;
}

/*
 * first sort list of overlays based on z_order and then within
 * same z_order sort them on dst_x.
 */
static int __mdss_overlay_src_split_sort(struct msm_fb_data_type *mfd,
	struct mdp_overlay *ovs, int num_ovs)
{
	int i;
	int left_lm_zo_cnt[MDSS_MDP_MAX_STAGE] = {0};
	int right_lm_zo_cnt[MDSS_MDP_MAX_STAGE] = {0};
	u32 left_lm_w = left_lm_w_from_mfd(mfd);

	sort(ovs, num_ovs, sizeof(struct mdp_overlay), __zorder_dstx_cmp_func,
		__overlay_swap_func);

	for (i = 0; i < num_ovs; i++) {
		if (ovs[i].dst_rect.x < left_lm_w) {
			if (left_lm_zo_cnt[ovs[i].z_order] == 2) {
				pr_err("more than 2 ov @ stage%d on left lm\n",
					ovs[i].z_order);
				return -EINVAL;
			}
			left_lm_zo_cnt[ovs[i].z_order]++;
		} else {
			if (right_lm_zo_cnt[ovs[i].z_order] == 2) {
				pr_err("more than 2 ov @ stage%d on right lm\n",
					ovs[i].z_order);
				return -EINVAL;
			}
			right_lm_zo_cnt[ovs[i].z_order]++;
		}
	}

	return 0;
}

static int __handle_overlay_prepare(struct msm_fb_data_type *mfd,
	struct mdp_overlay_list *ovlist, struct mdp_overlay *ip_ovs)
{
	int ret, i;
	int new_reqs = 0, left_cnt = 0, right_cnt = 0;
	int num_ovs = ovlist->num_overlays;
	u32 left_lm_w = left_lm_w_from_mfd(mfd);
	u32 left_lm_ovs = 0, right_lm_ovs = 0;
	bool is_single_layer = false;

	struct mdss_data_type *mdata = mfd_to_mdata(mfd);
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);

	struct mdp_overlay *sorted_ovs = NULL;
	struct mdp_overlay *req, *prev_req;

	struct mdss_mdp_pipe *pipe, *left_blend_pipe;
	struct mdss_mdp_pipe *right_plist[MAX_PIPES_PER_LM] = { 0 };
	struct mdss_mdp_pipe *left_plist[MAX_PIPES_PER_LM] = { 0 };

	bool sort_needed = mdata->has_src_split && (num_ovs > 1);

	ret = mutex_lock_interruptible(&mdp5_data->ov_lock);
	if (ret)
		return ret;

	if (mdss_fb_is_power_off(mfd)) {
		mutex_unlock(&mdp5_data->ov_lock);
		return -EPERM;
	}

	if (sort_needed) {
		sorted_ovs = kzalloc(num_ovs * sizeof(*ip_ovs), GFP_KERNEL);
		if (!sorted_ovs) {
			pr_err("error allocating ovlist mem\n");
			return -ENOMEM;
		}
		memcpy(sorted_ovs, ip_ovs, num_ovs * sizeof(*ip_ovs));
		ret = __mdss_overlay_src_split_sort(mfd, sorted_ovs, num_ovs);
		if (ret) {
			pr_err("src_split_sort failed. ret=%d\n", ret);
			kfree(sorted_ovs);
			return ret;
		}
	}

	pr_debug("prepare fb%d num_ovs=%d\n", mfd->index, num_ovs);

	for (i = 0; i < num_ovs; i++) {
		if (IS_RIGHT_MIXER_OV(ip_ovs[i].flags, ip_ovs[i].dst_rect.x,
			left_lm_w))
			right_lm_ovs++;
		else
			left_lm_ovs++;

		if ((left_lm_ovs > 1) && (right_lm_ovs > 1))
			break;
	}

	for (i = 0; i < num_ovs; i++) {
		left_blend_pipe = NULL;

		if (sort_needed) {
			req = &sorted_ovs[i];
			prev_req = (i > 0) ? &sorted_ovs[i - 1] : NULL;

			/*
			 * check if current overlay is at same z_order as
			 * previous one and qualifies as a right blend. If yes,
			 * pass a pointer to the pipe representing previous
			 * overlay or in other terms left blend overlay.
			 */
			if (prev_req && (prev_req->z_order == req->z_order) &&
			    is_ov_right_blend(&prev_req->dst_rect,
				    &req->dst_rect, left_lm_w)) {
				left_blend_pipe = pipe;
			}
		} else {
			req = &ip_ovs[i];
		}

		if (IS_RIGHT_MIXER_OV(ip_ovs[i].flags, ip_ovs[i].dst_rect.x,
			left_lm_w))
			is_single_layer = (right_lm_ovs == 1);
		else
			is_single_layer = (left_lm_ovs == 1);

		req->z_order += MDSS_MDP_STAGE_0;
		ret = mdss_mdp_overlay_pipe_setup(mfd, req, &pipe,
			left_blend_pipe, is_single_layer);
		req->z_order -= MDSS_MDP_STAGE_0;

		if (IS_ERR_VALUE(ret))
			goto validate_exit;

		pr_debug("pnum:%d id:0x%x flags:0x%x dst_x:%d l_blend_pnum%d\n",
			pipe->num, req->id, req->flags, req->dst_rect.x,
			left_blend_pipe ? left_blend_pipe->num : -1);

		/* keep track of the new overlays to unset in case of errors */
		if (pipe->play_cnt == 0)
			new_reqs |= pipe->ndx;

		if (IS_RIGHT_MIXER_OV(pipe->flags, pipe->dst.x, left_lm_w)) {
			if (right_cnt >= MAX_PIPES_PER_LM) {
				pr_err("too many pipes on right mixer\n");
				ret = -EINVAL;
				goto validate_exit;
			}
			right_plist[right_cnt] = pipe;
			right_cnt++;
		} else {
			if (left_cnt >= MAX_PIPES_PER_LM) {
				pr_err("too many pipes on left mixer\n");
				ret = -EINVAL;
				goto validate_exit;
			}
			left_plist[left_cnt] = pipe;
			left_cnt++;
		}
	}

	ret = mdss_mdp_perf_bw_check(mdp5_data->ctl, left_plist, left_cnt,
			right_plist, right_cnt);

validate_exit:
	if (sort_needed)
		ovlist->processed_overlays =
			__mdss_overlay_map(sorted_ovs, ip_ovs, num_ovs, i);
	else
		ovlist->processed_overlays = i;

	if (IS_ERR_VALUE(ret)) {
		pr_debug("err=%d total_ovs:%d processed:%d left:%d right:%d\n",
			ret, num_ovs, ovlist->processed_overlays, left_lm_ovs,
			right_lm_ovs);
		mdss_mdp_overlay_release(mfd, new_reqs);
	}
	mutex_unlock(&mdp5_data->ov_lock);

	kfree(sorted_ovs);

	return ret;
}

static int __handle_ioctl_overlay_prepare(struct msm_fb_data_type *mfd,
		void __user *argp)
{
	struct mdp_overlay_list ovlist;
	struct mdp_overlay *req_list[OVERLAY_MAX];
	struct mdp_overlay *overlays;
	int i, ret;

	if (copy_from_user(&ovlist, argp, sizeof(ovlist)))
		return -EFAULT;

	if (ovlist.num_overlays >= OVERLAY_MAX) {
		pr_err("Number of overlays exceeds max\n");
		return -EINVAL;
	}

	overlays = kmalloc(ovlist.num_overlays * sizeof(*overlays), GFP_KERNEL);
	if (!overlays) {
		pr_err("Unable to allocate memory for overlays\n");
		return -ENOMEM;
	}

	if (copy_from_user(req_list, ovlist.overlay_list,
				sizeof(struct mdp_overlay *) *
				ovlist.num_overlays)) {
		ret = -EFAULT;
		goto validate_exit;
	}

	for (i = 0; i < ovlist.num_overlays; i++) {
		if (copy_from_user(overlays + i, req_list[i],
				sizeof(struct mdp_overlay))) {
			ret = -EFAULT;
			goto validate_exit;
		}
	}

	ret = __handle_overlay_prepare(mfd, &ovlist, overlays);
	if (!IS_ERR_VALUE(ret)) {
		for (i = 0; i < ovlist.num_overlays; i++) {
			if (copy_to_user(req_list[i], overlays + i,
					sizeof(struct mdp_overlay))) {
				ret = -EFAULT;
				goto validate_exit;
			}
		}
	}

	if (copy_to_user(argp, &ovlist, sizeof(ovlist)))
		ret = -EFAULT;

validate_exit:
	kfree(overlays);

	return ret;
}

static int mdss_mdp_overlay_ioctl_handler(struct msm_fb_data_type *mfd,
					  u32 cmd, void __user *argp)
{
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	struct mdp_overlay *req = NULL;
	int val, ret = -ENOSYS;
	struct msmfb_metadata metadata;

	switch (cmd) {
	case MSMFB_MDP_PP:
		ret = mdss_mdp_pp_ioctl(mfd, argp);
		break;

	case MSMFB_HISTOGRAM_START:
	case MSMFB_HISTOGRAM_STOP:
	case MSMFB_HISTOGRAM:
		ret = mdss_mdp_histo_ioctl(mfd, cmd, argp);
		break;

	case MSMFB_OVERLAY_GET:
		req = kmalloc(sizeof(struct mdp_overlay), GFP_KERNEL);
		if (!req)
			return -ENOMEM;
		ret = copy_from_user(req, argp, sizeof(*req));
		if (!ret) {
			ret = mdss_mdp_overlay_get(mfd, req);

			if (!IS_ERR_VALUE(ret))
				ret = copy_to_user(argp, req, sizeof(*req));
		}

		if (ret)
			pr_debug("OVERLAY_GET failed (%d)\n", ret);
		break;

	case MSMFB_OVERLAY_SET:
		req = kmalloc(sizeof(struct mdp_overlay), GFP_KERNEL);
		if (!req)
			return -ENOMEM;
		ret = copy_from_user(req, argp, sizeof(*req));
		if (!ret) {
			ret = mdss_mdp_overlay_set(mfd, req);

			if (!IS_ERR_VALUE(ret))
				ret = copy_to_user(argp, req, sizeof(*req));
		}
		if (ret)
			pr_debug("OVERLAY_SET failed (%d)\n", ret);
		break;


	case MSMFB_OVERLAY_UNSET:
		if (!IS_ERR_VALUE(copy_from_user(&val, argp, sizeof(val))))
			ret = mdss_mdp_overlay_unset(mfd, val);
		break;

	case MSMFB_OVERLAY_PLAY_ENABLE:
		if (!copy_from_user(&val, argp, sizeof(val))) {
			mdp5_data->overlay_play_enable = val;
			ret = 0;
		} else {
			pr_err("OVERLAY_PLAY_ENABLE failed (%d)\n", ret);
			ret = -EFAULT;
		}
		break;

	case MSMFB_OVERLAY_PLAY:
		if (mdp5_data->overlay_play_enable) {
			struct msmfb_overlay_data data;

			ret = copy_from_user(&data, argp, sizeof(data));
			if (!ret)
				ret = mdss_mdp_overlay_play(mfd, &data);

			if (ret)
				pr_debug("OVERLAY_PLAY failed (%d)\n", ret);
		} else {
			ret = 0;
		}
		break;

	case MSMFB_OVERLAY_PLAY_WAIT:
		if (mdp5_data->overlay_play_enable) {
			struct msmfb_overlay_data data;

			ret = copy_from_user(&data, argp, sizeof(data));
			if (!ret)
				ret = mdss_mdp_overlay_play_wait(mfd, &data);

			if (ret)
				pr_err("OVERLAY_PLAY_WAIT failed (%d)\n", ret);
		} else {
			ret = 0;
		}
		break;

	case MSMFB_VSYNC_CTRL:
	case MSMFB_OVERLAY_VSYNC_CTRL:
		if (!copy_from_user(&val, argp, sizeof(val))) {
			ret = mdss_mdp_overlay_vsync_ctrl(mfd, val);
		} else {
			pr_err("MSMFB_OVERLAY_VSYNC_CTRL failed (%d)\n", ret);
			ret = -EFAULT;
		}
		break;
	case MSMFB_OVERLAY_COMMIT:
		mdss_fb_wait_for_fence(&(mfd->mdp_sync_pt_data));
		ret = mfd->mdp.kickoff_fnc(mfd, NULL);
		break;
	case MSMFB_METADATA_SET:
		ret = copy_from_user(&metadata, argp, sizeof(metadata));
		if (ret)
			return ret;
		ret = mdss_fb_set_metadata(mfd, &metadata);
		break;
	case MSMFB_METADATA_GET:
		ret = copy_from_user(&metadata, argp, sizeof(metadata));
		if (ret)
			return ret;
		ret = mdss_fb_get_metadata(mfd, &metadata);
		if (!ret)
			ret = copy_to_user(argp, &metadata, sizeof(metadata));
		break;
	case MSMFB_OVERLAY_PREPARE:
		ret = __handle_ioctl_overlay_prepare(mfd, argp);
		break;
	default:
		if (mfd->panel.type == WRITEBACK_PANEL)
			ret = mdss_mdp_wb_ioctl_handler(mfd, cmd, argp);
		break;
	}

	kfree(req);
	return ret;
}

/**
 * __mdss_mdp_overlay_ctl_init - Helper function to intialize control structure
 * @mfd: msm frame buffer data structure associated with the fb device.
 *
 * Helper function that allocates and initializes the mdp control structure
 * for a frame buffer device. Whenver applicable, this function will also setup
 * the control for the split display path as well.
 *
 * Return: pointer to the newly allocated control structure.
 */
static struct mdss_mdp_ctl *__mdss_mdp_overlay_ctl_init(
	struct msm_fb_data_type *mfd)
{
	int rc = 0;
	struct mdss_mdp_ctl *ctl;
	struct mdss_panel_data *pdata;

	if (!mfd)
		return ERR_PTR(-EINVAL);

	pdata = dev_get_platdata(&mfd->pdev->dev);
	if (!pdata) {
		pr_err("no panel connected for fb%d\n", mfd->index);
		rc = -ENODEV;
		goto error;
	}

	ctl = mdss_mdp_ctl_init(pdata, mfd);
	if (IS_ERR_OR_NULL(ctl)) {
		pr_err("Unable to initialize ctl for fb%d\n",
			mfd->index);
		rc = PTR_ERR(ctl);
		goto error;
	}
	ctl->vsync_handler.vsync_handler =
					mdss_mdp_overlay_handle_vsync;
	ctl->vsync_handler.cmd_post_flush = false;

	ctl->recover_underrun_handler.vsync_handler =
			mdss_mdp_recover_underrun_handler;
	ctl->recover_underrun_handler.cmd_post_flush = false;

	INIT_WORK(&ctl->remove_underrun_handler,
				remove_underrun_vsync_handler);

	if (is_split_lm(mfd)) {
		/* enable split display */
		rc = mdss_mdp_ctl_split_display_setup(ctl, pdata->next);
		if (rc) {
			mdss_mdp_ctl_destroy(ctl);
			goto error;
		}
	}

error:
	if (rc)
		return ERR_PTR(rc);
	else
		return ctl;
}

static int mdss_mdp_overlay_on(struct msm_fb_data_type *mfd)
{
	int rc, ad_ret;
	struct mdss_overlay_private *mdp5_data;
	struct mdss_mdp_ctl *ctl = NULL;

	if (!mfd)
		return -ENODEV;

	if (mfd->key != MFD_KEY)
		return -EINVAL;

	mdp5_data = mfd_to_mdp5_data(mfd);
	if (!mdp5_data)
		return -EINVAL;

	if (!mdp5_data->ctl) {
		ctl = __mdss_mdp_overlay_ctl_init(mfd);
		if (IS_ERR_OR_NULL(ctl))
			return PTR_ERR(ctl);
		mdp5_data->ctl = ctl;
	} else {
		ctl = mdp5_data->ctl;
	}

	if (mdss_fb_is_power_on(mfd)) {
		pr_debug("panel was never turned off\n");
		rc = mdss_mdp_ctl_start(mdp5_data->ctl, false);
		goto panel_on;
	}

	if (!mfd->panel_info->cont_splash_enabled &&
		(mfd->panel_info->type != DTV_PANEL)) {
		rc = mdss_mdp_overlay_start(mfd);
		if (rc)
			goto end;
		if (mfd->panel_info->type != WRITEBACK_PANEL) {
			atomic_inc(&mfd->mdp_sync_pt_data.commit_cnt);
			rc = mdss_mdp_overlay_kickoff(mfd, NULL);
		}
	} else {
		rc = mdss_mdp_ctl_setup(mdp5_data->ctl);
		if (rc)
			goto end;
	}

panel_on:
	if (IS_ERR_VALUE(rc)) {
		pr_err("Failed to turn on fb%d\n", mfd->index);
		mdss_mdp_overlay_off(mfd);
		goto end;
	}

	if (mfd->mdp.ad_work_setup) {
		ad_ret = mfd->mdp.ad_work_setup(mfd);
		if (ad_ret)
			pr_err("AD work queue setup failed! ret =%d\n", ad_ret);
	}

end:
	return rc;
}

static int mdss_mdp_overlay_off(struct msm_fb_data_type *mfd)
{
	int rc;
	struct mdss_overlay_private *mdp5_data;
	struct mdss_mdp_mixer *mixer;
	int need_cleanup;

	if (!mfd)
		return -ENODEV;

	if (mfd->key != MFD_KEY)
		return -EINVAL;

	mdp5_data = mfd_to_mdp5_data(mfd);

	if (!mdp5_data || !mdp5_data->ctl) {
		pr_err("ctl not initialized\n");
		return -ENODEV;
	}

	if (!mdss_mdp_ctl_is_power_on(mdp5_data->ctl)) {
		if (mfd->panel_reconfig) {
			mdp5_data->borderfill_enable = false;
			mdss_mdp_ctl_destroy(mdp5_data->ctl);
			mdp5_data->ctl = NULL;
		}
		return 0;
	}

	/*
	 * Keep a reference to the runtime pm until the overlay is turned
	 * off, and then release this last reference at the end. This will
	 * help in distinguishing between idle power collapse versus suspend
	 * power collapse
	 */
	pm_runtime_get_sync(&mfd->pdev->dev);

	if (mdss_fb_is_power_on_lp(mfd)) {
		pr_debug("panel not turned off. keeping overlay on\n");
		goto ctl_stop;
	}

	mutex_lock(&mdp5_data->ov_lock);

	mdss_mdp_overlay_free_fb_pipe(mfd);

	mixer = mdss_mdp_mixer_get(mdp5_data->ctl, MDSS_MDP_MIXER_MUX_LEFT);
	if (mixer)
		mixer->cursor_enabled = 0;

	mixer = mdss_mdp_mixer_get(mdp5_data->ctl, MDSS_MDP_MIXER_MUX_RIGHT);
	if (mixer)
		mixer->cursor_enabled = 0;

	mutex_lock(&mdp5_data->list_lock);
	need_cleanup = !list_empty(&mdp5_data->pipes_cleanup);
	mutex_unlock(&mdp5_data->list_lock);
	mutex_unlock(&mdp5_data->ov_lock);

	if (need_cleanup) {
		pr_debug("cleaning up pipes on fb%d\n", mfd->index);
		mdss_mdp_overlay_kickoff(mfd, NULL);
	}

	/*
	 * If retire fences are still active wait for a vsync time
	 * for retire fence to be updated.
	 * As a last resort signal the timeline if vsync doesn't arrive.
	 */
	if (mdp5_data->retire_cnt) {
		u32 fps = mdss_panel_get_framerate(mfd->panel_info);
		u32 vsync_time = 1000 / (fps ? : DEFAULT_FRAME_RATE);

		msleep(vsync_time);

		__vsync_retire_signal(mfd, mdp5_data->retire_cnt);
	}

ctl_stop:
	mutex_lock(&mdp5_data->ov_lock);
	rc = mdss_mdp_ctl_stop(mdp5_data->ctl, mfd->panel_power_state);
	if (rc == 0) {
		if (mdss_fb_is_power_off(mfd)) {
			mutex_lock(&mdp5_data->list_lock);
			__mdss_mdp_overlay_free_list_purge(mfd);
			mutex_unlock(&mdp5_data->list_lock);
			mdss_mdp_ctl_notifier_unregister(mdp5_data->ctl,
					&mfd->mdp_sync_pt_data.notifier);

			if (!mfd->ref_cnt || mfd->panel_reconfig) {
				mdp5_data->borderfill_enable = false;
				mdss_mdp_ctl_destroy(mdp5_data->ctl);
				mdp5_data->ctl = NULL;
			}

			if (atomic_dec_return(
				&mdp5_data->mdata->active_intf_cnt) == 0)
				mdss_mdp_rotator_release_all();

			if (!mdp5_data->mdata->idle_pc_enabled ||
				(mfd->panel_info->type != MIPI_CMD_PANEL)) {
				rc = pm_runtime_put(&mfd->pdev->dev);
				if (rc)
					pr_err("unable to suspend w/pm_runtime_put (%d)\n",
						rc);
			}
		}
	}
	mutex_unlock(&mdp5_data->ov_lock);

	/* Release the last reference to the runtime device */
	rc = pm_runtime_put(&mfd->pdev->dev);
	if (rc)
		pr_err("unable to suspend w/pm_runtime_put (%d)\n", rc);

	return rc;
}

int mdss_panel_register_done(struct mdss_panel_data *pdata)
{
	if (pdata->panel_info.cont_splash_enabled)
		mdss_mdp_footswitch_ctrl_splash(1);

	return 0;
}

static int __mdss_mdp_ctl_handoff(struct mdss_mdp_ctl *ctl,
	struct mdss_data_type *mdata)
{
	int rc = 0;
	int i, j;
	u32 mixercfg;
	struct mdss_mdp_pipe *pipe = NULL;

	if (!ctl || !mdata)
		return -EINVAL;

	for (i = 0; i < mdata->nmixers_intf; i++) {
		mixercfg = mdss_mdp_ctl_read(ctl, MDSS_MDP_REG_CTL_LAYER(i));
		pr_debug("for lm%d mixercfg = 0x%09x\n", i, mixercfg);

		j = MDSS_MDP_SSPP_VIG0;
		for (; j < MDSS_MDP_SSPP_CURSOR0 && mixercfg; j++) {
			u32 cfg = j * 3;
			if ((j == MDSS_MDP_SSPP_VIG3) ||
			    (j == MDSS_MDP_SSPP_RGB3)) {
				/* Add 2 to account for Cursor & Border bits */
				cfg += 2;
			}
			if (mixercfg & (0x7 << cfg)) {
				pr_debug("Pipe %d staged\n", j);
				pipe = mdss_mdp_pipe_search(mdata, BIT(j));
				if (!pipe) {
					pr_warn("Invalid pipe %d staged\n", j);
					continue;
				}

				rc = mdss_mdp_pipe_handoff(pipe);
				if (rc) {
					pr_err("Failed to handoff pipe%d\n",
						pipe->num);
					goto exit;
				}

				rc = mdss_mdp_mixer_handoff(ctl, i, pipe);
				if (rc) {
					pr_err("failed to handoff mix%d\n", i);
					goto exit;
				}
			}
		}
	}
exit:
	return rc;
}

/**
 * mdss_mdp_overlay_handoff() - Read MDP registers to handoff an active ctl path
 * @mfd: Msm frame buffer structure associated with the fb device.
 *
 * This function populates the MDP software structures with the current state of
 * the MDP hardware to handoff any active control path for the framebuffer
 * device. This is needed to identify any ctl, mixers and pipes being set up by
 * the bootloader to display the splash screen when the continuous splash screen
 * feature is enabled in kernel.
 */
static int mdss_mdp_overlay_handoff(struct msm_fb_data_type *mfd)
{
	int rc = 0;
	struct mdss_data_type *mdata = mfd_to_mdata(mfd);
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	struct mdss_mdp_ctl *ctl = NULL;
	struct mdss_mdp_ctl *sctl = NULL;

	if (!mdp5_data->ctl) {
		ctl = __mdss_mdp_overlay_ctl_init(mfd);
		if (IS_ERR_OR_NULL(ctl)) {
			rc = PTR_ERR(ctl);
			goto error;
		}
		mdp5_data->ctl = ctl;
	} else {
		ctl = mdp5_data->ctl;
	}

	/*
	 * vsync interrupt needs on during continuous splash, this is
	 * to initialize necessary ctl members here.
	 */
	rc = mdss_mdp_ctl_start(ctl, true);
	if (rc) {
		pr_err("Failed to initialize ctl\n");
		goto error;
	}

	ctl->clk_rate = mdss_mdp_get_clk_rate(MDSS_CLK_MDP_SRC);
	pr_debug("Set the ctl clock rate to %d Hz\n", ctl->clk_rate);

	rc = __mdss_mdp_ctl_handoff(ctl, mdata);
	if (rc) {
		pr_err("primary ctl handoff failed. rc=%d\n", rc);
		goto error;
	}

	if (is_split_lm(mfd)) {
		sctl = mdss_mdp_get_split_ctl(ctl);
		if (!sctl) {
			pr_err("cannot get secondary ctl. fail the handoff\n");
			rc = -EPERM;
			goto error;
		}
		rc = __mdss_mdp_ctl_handoff(sctl, mdata);
		if (rc) {
			pr_err("secondary ctl handoff failed. rc=%d\n", rc);
			goto error;
		}
	}

	rc = mdss_mdp_smp_handoff(mdata);
	if (rc)
		pr_err("Failed to handoff smps\n");

	mdp5_data->handoff = true;

error:
	if (rc && ctl) {
		mdss_mdp_handoff_cleanup_pipes(mfd, MDSS_MDP_PIPE_TYPE_RGB);
		mdss_mdp_handoff_cleanup_pipes(mfd, MDSS_MDP_PIPE_TYPE_VIG);
		mdss_mdp_handoff_cleanup_pipes(mfd, MDSS_MDP_PIPE_TYPE_DMA);
		mdss_mdp_ctl_destroy(ctl);
		mdp5_data->ctl = NULL;
		mdp5_data->handoff = false;
	}

	return rc;
}

static void __vsync_retire_handle_vsync(struct mdss_mdp_ctl *ctl, ktime_t t)
{
	struct msm_fb_data_type *mfd = ctl->mfd;
	struct mdss_overlay_private *mdp5_data;

	if (!mfd || !mfd->mdp.private1) {
		pr_warn("Invalid handle for vsync\n");
		return;
	}

	mdp5_data = mfd_to_mdp5_data(mfd);
	schedule_work(&mdp5_data->retire_work);
}

static void __vsync_retire_work_handler(struct work_struct *work)
{
	struct mdss_overlay_private *mdp5_data =
		container_of(work, typeof(*mdp5_data), retire_work);

	if (!mdp5_data->ctl || !mdp5_data->ctl->mfd)
		return;

	if (!mdp5_data->ctl->remove_vsync_handler)
		return;

	__vsync_retire_signal(mdp5_data->ctl->mfd, 1);
}

static void __vsync_retire_signal(struct msm_fb_data_type *mfd, int val)
{
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);

	mutex_lock(&mfd->mdp_sync_pt_data.sync_mutex);
	if (mdp5_data->retire_cnt > 0) {
		sw_sync_timeline_inc(mdp5_data->vsync_timeline, val);

		mdp5_data->retire_cnt -= min(val, mdp5_data->retire_cnt);
		if (mdp5_data->retire_cnt == 0) {
			mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);
			mdp5_data->ctl->remove_vsync_handler(mdp5_data->ctl,
					&mdp5_data->vsync_retire_handler);
			mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
		}
	}
	mutex_unlock(&mfd->mdp_sync_pt_data.sync_mutex);
}

static struct sync_fence *
__vsync_retire_get_fence(struct msm_sync_pt_data *sync_pt_data)
{
	struct msm_fb_data_type *mfd;
	struct mdss_overlay_private *mdp5_data;
	struct mdss_mdp_ctl *ctl;
	int value;

	mfd = container_of(sync_pt_data, typeof(*mfd), mdp_sync_pt_data);
	mdp5_data = mfd_to_mdp5_data(mfd);

	if (!mdp5_data || !mdp5_data->ctl)
		return ERR_PTR(-ENODEV);

	ctl = mdp5_data->ctl;
	if (!ctl->add_vsync_handler)
		return ERR_PTR(-EOPNOTSUPP);

	if (!mdss_mdp_ctl_is_power_on(ctl)) {
		pr_debug("fb%d vsync pending first update\n", mfd->index);
		return ERR_PTR(-EPERM);
	}

	value = mdp5_data->vsync_timeline->value + 1 + mdp5_data->retire_cnt;
	mdp5_data->retire_cnt++;

	return mdss_fb_sync_get_fence(mdp5_data->vsync_timeline,
			"mdp-retire", value);
}

static int __vsync_set_vsync_handler(struct msm_fb_data_type *mfd)
{
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	struct mdss_mdp_ctl *ctl;
	int rc;

	ctl = mdp5_data->ctl;
	if (!mdp5_data->retire_cnt ||
		mdp5_data->vsync_retire_handler.enabled)
		return 0;

	if (!ctl->add_vsync_handler)
		return -EOPNOTSUPP;

	if (!mdss_mdp_ctl_is_power_on(ctl)) {
		pr_debug("fb%d vsync pending first update\n", mfd->index);
		return -EPERM;
	}

	rc = ctl->add_vsync_handler(ctl,
			&mdp5_data->vsync_retire_handler);
	return rc;
}

static int __vsync_retire_setup(struct msm_fb_data_type *mfd)
{
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	char name[24];

	snprintf(name, sizeof(name), "mdss_fb%d_retire", mfd->index);
	mdp5_data->vsync_timeline = sw_sync_timeline_create(name);
	if (mdp5_data->vsync_timeline == NULL) {
		pr_err("cannot vsync create time line");
		return -ENOMEM;
	}
	mfd->mdp_sync_pt_data.get_retire_fence = __vsync_retire_get_fence;

	mdp5_data->vsync_retire_handler.vsync_handler =
		__vsync_retire_handle_vsync;
	mdp5_data->vsync_retire_handler.cmd_post_flush = false;
	INIT_WORK(&mdp5_data->retire_work, __vsync_retire_work_handler);

	return 0;
}

static int mdss_mdp_update_panel_info(struct msm_fb_data_type *mfd, int mode)
{
	int ret = 0;
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	struct mdss_mdp_ctl *ctl = mdp5_data->ctl;

	ret = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_DSI_DYNAMIC_SWITCH,
						(void *)(unsigned long)mode);
	if (ret)
		pr_err("Dynamic switch to %s mode failed!\n",
					mode ? "command" : "video");
	/*
	 * Destroy current ctrl sturcture as this is
	 * going to be re-initialized with the requested mode.
	 */
	mdss_mdp_ctl_destroy(mdp5_data->ctl);
	mdp5_data->ctl = NULL;

	return 0;
}

int mdss_mdp_overlay_init(struct msm_fb_data_type *mfd)
{
	struct device *dev = mfd->fbi->dev;
	struct msm_mdp_interface *mdp5_interface = &mfd->mdp;
	struct mdss_overlay_private *mdp5_data = NULL;
	int rc;

	mdp5_data = kzalloc(sizeof(struct mdss_overlay_private), GFP_KERNEL);
	if (!mdp5_data) {
		pr_err("fail to allocate mdp5 private data structure");
		return -ENOMEM;
	}

	mdp5_data->mdata = dev_get_drvdata(mfd->pdev->dev.parent);
	if (!mdp5_data->mdata) {
		pr_err("unable to initialize overlay for fb%d\n", mfd->index);
		rc = -ENODEV;
		goto init_fail;
	}

	mdp5_interface->on_fnc = mdss_mdp_overlay_on;
	mdp5_interface->off_fnc = mdss_mdp_overlay_off;
	mdp5_interface->release_fnc = __mdss_mdp_overlay_release_all;
	mdp5_interface->do_histogram = NULL;
	if (mdp5_data->mdata->ncursor_pipes)
		mdp5_interface->cursor_update = mdss_mdp_hw_cursor_pipe_update;
	else
		mdp5_interface->cursor_update = mdss_mdp_hw_cursor_update;
	mdp5_interface->dma_fnc = mdss_mdp_overlay_pan_display;
	mdp5_interface->ioctl_handler = mdss_mdp_overlay_ioctl_handler;
	mdp5_interface->panel_register_done = mdss_panel_register_done;
	mdp5_interface->kickoff_fnc = mdss_mdp_overlay_kickoff;
	mdp5_interface->get_sync_fnc = mdss_mdp_rotator_sync_pt_get;
	mdp5_interface->splash_init_fnc = mdss_mdp_splash_init;
	mdp5_interface->configure_panel = mdss_mdp_update_panel_info;

	INIT_LIST_HEAD(&mdp5_data->pipes_used);
	INIT_LIST_HEAD(&mdp5_data->pipes_cleanup);
	INIT_LIST_HEAD(&mdp5_data->rot_proc_list);
	mutex_init(&mdp5_data->list_lock);
	mutex_init(&mdp5_data->ov_lock);
	mutex_init(&mdp5_data->dfps_lock);
	mdp5_data->hw_refresh = true;
	mdp5_data->overlay_play_enable = true;
	mdp5_data->cursor_ndx[CURSOR_PIPE_LEFT] = MSMFB_NEW_REQUEST;
	mdp5_data->cursor_ndx[CURSOR_PIPE_RIGHT] = MSMFB_NEW_REQUEST;

	mfd->mdp.private1 = mdp5_data;
	mfd->wait_for_kickoff = true;
	if (is_panel_split(mfd) && mdp5_data->mdata->has_dst_split)
		mfd->split_mode = MDP_SPLIT_MODE_DST;

	if (mfd->panel_info->partial_update_enabled && is_split_lm(mfd))
		mdp5_data->mdata->has_src_split = false;

	if (mfd->panel_info->partial_update_enabled)
		mdp5_data->mdata->pp_enable = MDP_PP_DISABLE;
	else
		mdp5_data->mdata->pp_enable = MDP_PP_ENABLE;

	rc = mdss_mdp_overlay_fb_parse_dt(mfd);
	if (rc)
		return rc;

	mfd->panel_orientation = mfd->panel_info->panel_orientation;

	if ((mfd->panel_info->panel_orientation & MDP_FLIP_LR) &&
		(is_split_lm(mfd)))
		mdp5_data->mixer_swap = true;

	rc = sysfs_create_group(&dev->kobj, &mdp_overlay_sysfs_group);
	if (rc) {
		pr_err("vsync sysfs group creation failed, ret=%d\n", rc);
		goto init_fail;
	}

	mdp5_data->vsync_event_sd = sysfs_get_dirent(dev->kobj.sd, NULL,
						     "vsync_event");
	if (!mdp5_data->vsync_event_sd) {
		pr_err("vsync_event sysfs lookup failed\n");
		rc = -ENODEV;
		goto init_fail;
	}

	rc = sysfs_create_link_nowarn(&dev->kobj,
			&mdp5_data->mdata->pdev->dev.kobj, "mdp");
	if (rc)
		pr_warn("problem creating link to mdp sysfs\n");

	rc = sysfs_create_link_nowarn(&dev->kobj,
			&mfd->pdev->dev.kobj, "mdss_fb");
	if (rc)
		pr_warn("problem creating link to mdss_fb sysfs\n");

	if (mfd->panel_info->type == MIPI_VIDEO_PANEL) {
		rc = sysfs_create_group(&dev->kobj,
			&dynamic_fps_fs_attrs_group);
		if (rc) {
			pr_err("Error dfps sysfs creation ret=%d\n", rc);
			goto init_fail;
		}
	}

	if (mfd->panel_info->mipi.dynamic_switch_enabled ||
			mfd->panel_info->type == MIPI_CMD_PANEL) {
		rc = __vsync_retire_setup(mfd);
		if (IS_ERR_VALUE(rc)) {
			pr_err("unable to create vsync timeline\n");
			goto init_fail;
		}
	}
	mfd->mdp_sync_pt_data.async_wait_fences = true;

	pm_runtime_set_suspended(&mfd->pdev->dev);
	pm_runtime_enable(&mfd->pdev->dev);

	kobject_uevent(&dev->kobj, KOBJ_ADD);
	pr_debug("vsync kobject_uevent(KOBJ_ADD)\n");

	mdp5_data->cpu_pm_hdl = add_event_timer(NULL, (void *)mdp5_data);
	if (!mdp5_data->cpu_pm_hdl)
		pr_warn("%s: unable to add event timer\n", __func__);

	if (mfd->panel_info->cont_splash_enabled) {
		rc = mdss_mdp_overlay_handoff(mfd);
		if (rc) {
			/*
			 * Even though handoff failed, it is not fatal.
			 * MDP can continue, just that we would have a longer
			 * delay in transitioning from splash screen to boot
			 * animation
			 */
			pr_warn("Overlay handoff failed for fb%d. rc=%d\n",
				mfd->index, rc);
			rc = 0;
		}
	}
	mdp5_data->dyn_pu_state = mfd->panel_info->partial_update_enabled;

	if (mdss_mdp_pp_overlay_init(mfd))
		pr_warn("Failed to initialize pp overlay data.\n");
	return rc;
init_fail:
	kfree(mdp5_data);
	return rc;
}

static int mdss_mdp_overlay_fb_parse_dt(struct msm_fb_data_type *mfd)
{
	int rc = 0;
	struct platform_device *pdev = mfd->pdev;
	struct mdss_overlay_private *mdp5_mdata = mfd_to_mdp5_data(mfd);

	mdp5_mdata->mixer_swap = of_property_read_bool(pdev->dev.of_node,
					   "qcom,mdss-mixer-swap");
	if (mdp5_mdata->mixer_swap) {
		pr_info("mixer swap is enabled for fb device=%s\n",
			pdev->name);
	}

	return rc;
}
