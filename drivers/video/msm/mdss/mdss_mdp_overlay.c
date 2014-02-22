/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#include <linux/sw_sync.h>

#include <mach/iommu_domains.h>
#include <mach/event_timer.h>
#include <mach/msm_bus.h>
#include <mach/scm.h>
#include "mdss.h"
#include "mdss_debug.h"
#include "mdss_fb.h"
#include "mdss_mdp.h"
#include "mdss_mdp_rotator.h"

#include "splash.h"

#define VSYNC_PERIOD 16
#define BORDERFILL_NDX	0x0BF000BF
#define CHECK_BOUNDS(offset, size, max_size) \
	(((size) > (max_size)) || ((offset) > ((max_size) - (size))))

#define PP_CLK_CFG_OFF 0
#define PP_CLK_CFG_ON 1

#define MEM_PROTECT_SD_CTRL 0xF

#define INVALID_PIPE_INDEX 0xFFFF

struct sd_ctrl_req {
	unsigned int enable;
} __attribute__ ((__packed__));

static atomic_t ov_active_panels = ATOMIC_INIT(0);
static int mdss_mdp_overlay_free_fb_pipe(struct msm_fb_data_type *mfd);
static int mdss_mdp_overlay_fb_parse_dt(struct msm_fb_data_type *mfd);
static int mdss_mdp_overlay_off(struct msm_fb_data_type *mfd);
static int mdss_mdp_overlay_splash_parse_dt(struct msm_fb_data_type *mfd);

static int mdss_mdp_overlay_sd_ctrl(struct msm_fb_data_type *mfd,
					unsigned int enable)
{
	struct sd_ctrl_req request;
	unsigned int resp = -1;
	int ret = 0;
	pr_debug("sd_ctrl %u\n", enable);

	request.enable = enable;

	ret = scm_call(SCM_SVC_MP, MEM_PROTECT_SD_CTRL,
		&request, sizeof(request), &resp, sizeof(resp));
	pr_debug("scm_call MEM_PROTECT_SD_CTRL(%u): ret=%d, resp=%x",
				enable, ret, resp);
	if (ret)
		return ret;

	return resp;
}

static int mdss_mdp_overlay_get(struct msm_fb_data_type *mfd,
				struct mdp_overlay *req)
{
	struct mdss_mdp_pipe *pipe;
	struct mdss_data_type *mdata = mfd_to_mdata(mfd);

	pipe = mdss_mdp_pipe_get(mdata, req->id);
	if (IS_ERR_OR_NULL(pipe)) {
		pr_err("invalid pipe ndx=%x\n", req->id);
		return pipe ? PTR_ERR(pipe) : -ENODEV;
	}

	*req = pipe->req_data;
	mdss_mdp_pipe_unmap(pipe);

	return 0;
}

int mdss_mdp_overlay_req_check(struct msm_fb_data_type *mfd,
			       struct mdp_overlay *req,
			       struct mdss_mdp_format_params *fmt)
{
	u32 xres, yres;
	u32 min_src_size, min_dst_size;
	int content_secure;
	struct mdss_data_type *mdata = mfd_to_mdata(mfd);
	struct mdss_mdp_ctl *ctl = mfd_to_ctl(mfd);

	xres = mfd->fbi->var.xres;
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

	if (req->z_order >= MDSS_MDP_MAX_STAGE) {
		pr_err("zorder %d out of range\n", req->z_order);
		return -ERANGE;
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
		}
	}

	if (!(req->flags & MDSS_MDP_ROT_ONLY)) {
		u32 src_w, src_h, dst_w, dst_h;

		if ((CHECK_BOUNDS(req->dst_rect.x, req->dst_rect.w, xres) ||
		     CHECK_BOUNDS(req->dst_rect.y, req->dst_rect.h, yres))) {
			pr_err("invalid destination rect=%d,%d,%d,%d\n",
			       req->dst_rect.x, req->dst_rect.y,
			       req->dst_rect.w, req->dst_rect.h);
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

		if (src_w > MAX_MIXER_WIDTH) {
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

		if (req->flags & MDP_DEINTERLACE) {
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

static int __mdp_pipe_tune_perf(struct mdss_mdp_pipe *pipe)
{
	struct mdss_data_type *mdata = pipe->mixer->ctl->mdata;
	struct mdss_mdp_perf_params perf;
	int rc;

	for (;;) {
		rc = mdss_mdp_perf_calc_pipe(pipe, &perf, NULL);

		if (!rc && (perf.mdp_clk_rate <= mdata->max_mdp_clk_rate))
			break;

		/*
		 * if decimation is available try to reduce minimum clock rate
		 * requirement by applying vertical decimation and reduce
		 * mdp clock requirement
		 */
		if (mdata->has_decimation && (pipe->vert_deci < MAX_DECIMATION)
			&& !pipe->bwc_mode)
			pipe->vert_deci++;
		else
			return -EPERM;
	}

	return 0;
}

static int __mdss_mdp_overlay_setup_scaling(struct mdss_mdp_pipe *pipe)
{
	u32 src;
	int rc;

	src = pipe->src.w >> pipe->horz_deci;

	if (pipe->scale.enable_pxl_ext)
		return 0;
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
	} else if (rc) {
		pr_err("Vertical scaling calculation failed=%d! %d->%d\n",
				rc, src, pipe->dst.h);
		return rc;
	}
	pipe->scale.init_phase_x[0] = (pipe->scale.phase_step_x[0] -
					(1 << PHASE_STEP_SHIFT)) / 2;
	pipe->scale.init_phase_y[0] = (pipe->scale.phase_step_y[0] -
					(1 << PHASE_STEP_SHIFT)) / 2;
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

static int mdss_mdp_overlay_pipe_setup(struct msm_fb_data_type *mfd,
				       struct mdp_overlay *req,
				       struct mdss_mdp_pipe **ppipe)
{
	struct mdss_mdp_format_params *fmt;
	struct mdss_mdp_pipe *pipe;
	struct mdss_mdp_mixer *mixer = NULL;
	u32 pipe_type, mixer_mux, len;
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	struct mdp_histogram_start_req hist;
	int ret;
	u32 bwc_enabled;

	if (mdp5_data->ctl == NULL)
		return -ENODEV;

	if (req->flags & MDP_ROT_90) {
		pr_err("unsupported inline rotation\n");
		return -ENOTSUPP;
	}

	if ((req->dst_rect.w > MAX_DST_W) || (req->dst_rect.h > MAX_DST_H)) {
		pr_err("exceeded max mixer supported resolution %dx%d\n",
				req->dst_rect.w, req->dst_rect.h);
		return -EOVERFLOW;
	}

	if (req->flags & MDSS_MDP_RIGHT_MIXER)
		mixer_mux = MDSS_MDP_MIXER_MUX_RIGHT;
	else
		mixer_mux = MDSS_MDP_MIXER_MUX_LEFT;

	pr_debug("pipe ctl=%u req id=%x mux=%d\n", mdp5_data->ctl->num, req->id,
			mixer_mux);

	if (req->flags & (MDP_SOURCE_ROTATED_90 | MDP_BWC_EN))
		req->src.format =
			mdss_mdp_get_rotator_dst_format(req->src.format, 1);

	fmt = mdss_mdp_get_format_params(req->src.format);
	if (!fmt) {
		pr_err("invalid pipe format %d\n", req->src.format);
		return -EINVAL;
	}

	ret = mdss_mdp_overlay_req_check(mfd, req, fmt);
	if (ret)
		return ret;

	pipe = mdss_mdp_mixer_stage_pipe(mdp5_data->ctl, mixer_mux,
					req->z_order);
	if (pipe && pipe->ndx != req->id) {
		pr_debug("replacing pnum=%d at stage=%d mux=%d\n",
				pipe->num, req->z_order, mixer_mux);
		mdss_mdp_mixer_pipe_unstage(pipe);
	}

	mixer = mdss_mdp_mixer_get(mdp5_data->ctl, mixer_mux);
	if (!mixer) {
		pr_err("unable to get mixer\n");
		return -ENODEV;
	}

	if (req->id == MSMFB_NEW_REQUEST) {
		if (req->flags & MDP_OV_PIPE_FORCE_DMA)
			pipe_type = MDSS_MDP_PIPE_TYPE_DMA;
		else if (fmt->is_yuv || (req->flags & MDP_OV_PIPE_SHARE))
			pipe_type = MDSS_MDP_PIPE_TYPE_VIG;
		else
			pipe_type = MDSS_MDP_PIPE_TYPE_RGB;

		pipe = mdss_mdp_pipe_alloc(mixer, pipe_type);

		/* VIG pipes can also support RGB format */
		if (!pipe && pipe_type == MDSS_MDP_PIPE_TYPE_RGB) {
			pipe_type = MDSS_MDP_PIPE_TYPE_VIG;
			pipe = mdss_mdp_pipe_alloc(mixer, pipe_type);
		}

		if (pipe == NULL) {
			pr_err("error allocating pipe\n");
			return -ENOMEM;
		}

		ret = mdss_mdp_pipe_map(pipe);
		if (ret) {
			pr_err("unable to map pipe=%d\n", pipe->num);
			return ret;
		}

		mutex_lock(&mfd->lock);
		list_add(&pipe->used_list, &mdp5_data->pipes_used);
		mutex_unlock(&mfd->lock);
		pipe->mixer = mixer;
		pipe->mfd = mfd;
		pipe->pid = current->tgid;
		pipe->play_cnt = 0;
	} else {
		pipe = mdss_mdp_pipe_get(mdp5_data->mdata, req->id);
		if (IS_ERR_OR_NULL(pipe)) {
			pr_err("invalid pipe ndx=%x\n", req->id);
			return pipe ? PTR_ERR(pipe) : -ENODEV;
		}

		if (pipe->mixer != mixer) {
			if (!mixer->ctl || (mixer->ctl->mfd != mfd)) {
				pr_err("Can't switch mixer %d->%d pnum %d!\n",
						pipe->mixer->num, mixer->num,
						pipe->num);
				ret = -EINVAL;
				goto exit_fail;
			}
			pr_debug("switching pipe mixer %d->%d pnum %d\n",
					pipe->mixer->num, mixer->num,
					pipe->num);
			mdss_mdp_mixer_pipe_unstage(pipe);
			pipe->mixer = mixer;
		}
	}

	pipe->flags = req->flags;
	bwc_enabled = req->flags & MDP_BWC_EN;
	if (bwc_enabled  &&  !mdp5_data->mdata->has_bwc) {
		pr_err("BWC is not supported in MDP version %x\n",
			mdp5_data->mdata->mdp_rev);
		pipe->bwc_mode = 0;
	} else {
		pipe->bwc_mode = pipe->mixer->rotator_mode ?
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
				(pipe->flags & MDSS_MDP_RIGHT_MIXER))
			pipe->overfetch_disable |= OVERFETCH_DISABLE_RIGHT;
		pr_debug("overfetch flags=%x\n", pipe->overfetch_disable);
	} else {
		pipe->overfetch_disable = 0;
	}
	pipe->bg_color = req->bg_color;

	req->id = pipe->ndx;
	pipe->req_data = *req;

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

	ret = __mdp_pipe_tune_perf(pipe);
	if (ret) {
		pr_debug("unable to satisfy performance. ret=%d\n", ret);
		goto exit_fail;
	}

	ret = __mdss_mdp_overlay_setup_scaling(pipe);
	if (ret)
		goto exit_fail;

	if ((mixer->type == MDSS_MDP_MIXER_TYPE_WRITEBACK) &&
		!mdp5_data->mdata->has_wfd_blk)
		mdss_mdp_smp_release(pipe);

	ret = mdss_mdp_smp_reserve(pipe);
	if (ret) {
		pr_debug("mdss_mdp_smp_reserve failed. ret=%d\n", ret);
		goto exit_fail;
	}

	pipe->params_changed++;

	req->vert_deci = pipe->vert_deci;

	*ppipe = pipe;

	mdss_mdp_pipe_unmap(pipe);

	return ret;

exit_fail:
	mdss_mdp_pipe_unmap(pipe);

	mutex_lock(&mfd->lock);
	if (pipe->play_cnt == 0) {
		pr_debug("failed for pipe %d\n", pipe->num);
		if (!list_empty(&pipe->used_list))
			list_del_init(&pipe->used_list);
		mdss_mdp_pipe_destroy(pipe);
	}

	/* invalidate any overlays in this framebuffer after failure */
	list_for_each_entry(pipe, &mdp5_data->pipes_used, used_list) {
		pr_debug("freeing allocations for pipe %d\n", pipe->num);
		mdss_mdp_smp_unreserve(pipe);
		pipe->params_changed = 0;
	}
	mutex_unlock(&mfd->lock);
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

	if (!mfd->panel_power_on) {
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

		ret = mdss_mdp_overlay_pipe_setup(mfd, req, &pipe);

		req->z_order -= MDSS_MDP_STAGE_0;
	}

	mutex_unlock(&mdp5_data->ov_lock);

	return ret;
}

int mdss_mdp_overlay_get_buf(struct msm_fb_data_type *mfd,
					   struct mdss_mdp_data *data,
					   struct msmfb_data *planes,
					   int num_planes,
					   u32 flags)
{
	int i, rc = 0;

	if ((num_planes <= 0) || (num_planes > MAX_PLANES))
		return -EINVAL;

	mdss_bus_bandwidth_ctrl(1);
	memset(data, 0, sizeof(*data));
	for (i = 0; i < num_planes; i++) {
		data->p[i].flags = flags;
		rc = mdss_mdp_get_img(&planes[i], &data->p[i]);
		if (rc) {
			pr_err("failed to map buf p=%d flags=%x\n", i, flags);
			while (i > 0) {
				i--;
				mdss_mdp_put_img(&data->p[i]);
			}
			break;
		}
	}
	mdss_bus_bandwidth_ctrl(0);

	data->num_planes = i;

	return rc;
}

int mdss_mdp_overlay_free_buf(struct mdss_mdp_data *data)
{
	int i;

	mdss_bus_bandwidth_ctrl(1);
	for (i = 0; i < data->num_planes && data->p[i].len; i++)
		mdss_mdp_put_img(&data->p[i]);
	mdss_bus_bandwidth_ctrl(0);

	data->num_planes = 0;

	return 0;
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
		mdss_mdp_overlay_free_buf(&mdp5_data->free_list[i]);
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

static void mdss_mdp_overlay_cleanup(struct msm_fb_data_type *mfd)
{
	struct mdss_mdp_pipe *pipe, *tmp;
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	LIST_HEAD(destroy_pipes);

	mutex_lock(&mfd->lock);
	__mdss_mdp_overlay_free_list_purge(mfd);

	list_for_each_entry_safe(pipe, tmp, &mdp5_data->pipes_cleanup,
				cleanup_list) {
		list_move(&pipe->cleanup_list, &destroy_pipes);
		mdss_mdp_pipe_fetch_halt(pipe);
		mdss_mdp_overlay_free_buf(&pipe->back_buf);
		__mdss_mdp_overlay_free_list_add(mfd, &pipe->front_buf);
		pipe->mfd = NULL;
	}

	list_for_each_entry(pipe, &mdp5_data->pipes_used, used_list) {
		if (pipe->back_buf.num_planes) {
			/* make back buffer active */
			__mdss_mdp_overlay_free_list_add(mfd, &pipe->front_buf);
			swap(pipe->back_buf, pipe->front_buf);
		}
	}
	mutex_unlock(&mfd->lock);
	list_for_each_entry_safe(pipe, tmp, &destroy_pipes, cleanup_list)
		mdss_mdp_pipe_destroy(pipe);
}

static void __mdss_mdp_handoff_cleanup_pipes(struct msm_fb_data_type *mfd,
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
			list_add(&pipe->cleanup_list,
				&mdp5_data->pipes_cleanup);
			mdss_mdp_mixer_pipe_unstage(pipe);
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
static int mdss_mdp_overlay_start(struct msm_fb_data_type *mfd)
{
	int rc;
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	struct mdss_mdp_ctl *ctl = mdp5_data->ctl;

	if (ctl->power_on) {
		if (!mdp5_data->mdata->batfet)
			mdss_mdp_batfet_ctrl(mdp5_data->mdata, true);
		if (!mfd->panel_info->cont_splash_enabled)
			mdss_iommu_attach(mdp5_data->mdata);
		return 0;
	}

	pr_debug("starting fb%d overlay\n", mfd->index);

	rc = pm_runtime_get_sync(&mfd->pdev->dev);
	if (IS_ERR_VALUE(rc)) {
		pr_err("unable to resume with pm_runtime_get_sync rc=%d\n", rc);
		return rc;
	}

	/*
	 * We need to do hw init before any hw programming.
	 * Also, hw init involves programming the VBIF registers which
	 * should be done only after attaching IOMMU which in turn would call
	 * in to TZ to restore security configs on the VBIF registers.
	 * This is not needed when continuous splash screen is enabled since
	 * we would have called in to TZ to restore security configs from LK.
	 */
	if (!is_mdss_iommu_attached()) {
		if (!mfd->panel_info->cont_splash_enabled)
			mdss_iommu_attach(mdss_res);
		mdss_hw_init(mdss_res);
	}

	rc = mdss_mdp_ctl_start(ctl, false);
	if (rc == 0) {
		atomic_inc(&ov_active_panels);

		mdss_mdp_ctl_notifier_register(mdp5_data->ctl,
				&mfd->mdp_sync_pt_data.notifier);
	} else {
		pr_err("mdp ctl start failed.\n");
		goto error;
	}

	if (mfd->panel_info->cont_splash_enabled) {
		if (mdp5_data->handoff) {
			/*
			 * Set up border-fill on the handed off pipes.
			 * This is needed to ensure that there are no memory
			 * accesses prior to attaching iommu during continuous
			 * splash screen case. However, for command mode
			 * displays, this is not necessary since the panels can
			 * refresh from their internal memory if no data is sent
			 * out on the dsi lanes.
			 */
			if (ctl && ctl->is_video_mode) {
				rc = mdss_mdp_display_commit(ctl, NULL);
				if (!IS_ERR_VALUE(rc)) {
					mdss_mdp_display_wait4comp(ctl);
				} else {
					/*
					 * Since border-fill setup failed, we
					 * need to ensure that we turn off the
					 * MDP timing generator before attaching
					 * iommu
					 */
					pr_err("failed to set BF at handoff\n");
					mdp5_data->handoff = false;
					rc = 0;
				}
			}

			/* Add all the handed off pipes to the cleanup list */
			__mdss_mdp_handoff_cleanup_pipes(mfd,
				MDSS_MDP_PIPE_TYPE_RGB);
			__mdss_mdp_handoff_cleanup_pipes(mfd,
				MDSS_MDP_PIPE_TYPE_VIG);
			__mdss_mdp_handoff_cleanup_pipes(mfd,
				MDSS_MDP_PIPE_TYPE_DMA);
		}
		rc = mdss_mdp_ctl_splash_finish(ctl, mdp5_data->handoff);
		/*
		 * Remove the vote for footswitch even if above function
		 * returned error
		 */
		mdss_mdp_footswitch_ctrl_splash(0);
		if (rc)
			goto error;

		if (!is_mdss_iommu_attached())
			mdss_iommu_attach(mdss_res);
	}

error:
	if (rc) {
		mdss_mdp_ctl_destroy(ctl);
		mdp5_data->ctl = NULL;
		pm_runtime_put(&mfd->pdev->dev);
	}

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

int mdss_mdp_overlay_kickoff(struct msm_fb_data_type *mfd,
				struct mdp_display_commit *data)
{
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	struct mdss_mdp_pipe *pipe;
	struct mdss_mdp_ctl *ctl = mfd_to_ctl(mfd);
	struct mdss_mdp_ctl *tmp;
	int ret = 0;
	int sd_in_pipe = 0;

	if (ctl->shared_lock)
		mutex_lock(ctl->shared_lock);

	mutex_lock(&mdp5_data->ov_lock);
	mutex_lock(&mfd->lock);

	/*
	 * check if there is a secure display session
	 */
	list_for_each_entry(pipe, &mdp5_data->pipes_used, used_list) {
		if (pipe->flags & MDP_SECURE_DISPLAY_OVERLAY_SESSION) {
			sd_in_pipe |= 1;
			pr_debug("Secure pipe: %u : %08X\n",
					pipe->num, pipe->flags);
		}
	}
	/*
	 * If there is no secure display session and sd_enabled, disable the
	 * secure display session
	 */
	if (!sd_in_pipe && mdp5_data->sd_enabled) {
		if (0 == mdss_mdp_overlay_sd_ctrl(mfd, 0))
			mdp5_data->sd_enabled = 0;
	}

	mdss_mdp_ctl_notify(ctl, MDP_NOTIFY_FRAME_BEGIN);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);

	if (data)
		mdss_mdp_set_roi(ctl, data);

	list_for_each_entry(pipe, &mdp5_data->pipes_used, used_list) {
		struct mdss_mdp_data *buf;
		/*
		 * When secure display is enabled, if there is a non secure
		 * display pipe, skip that
		 */
		if ((mdp5_data->sd_enabled) &&
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
		    (ctl->shared_lock && !ctl->mdata->has_wfd_blk)) {
			if (ctl->mdata->mixer_switched) {
				ret = mdss_mdp_overlay_pipe_setup(mfd,
						&pipe->req_data, &pipe);
				pr_debug("reseting DMA pipe for ctl=%d",
					 ctl->num);
			}
			if (ret) {
				pr_err("can't reset DMA pipe ret=%d ctl=%d\n",
					ret, ctl->num);
				mutex_unlock(&mfd->lock);
				goto commit_fail;
			}

			tmp = mdss_mdp_ctl_mixer_switch(ctl,
					MDSS_MDP_WB_CTL_TYPE_LINE);
			if (!tmp) {
				mutex_unlock(&mfd->lock);
				ret = -EINVAL;
				goto commit_fail;
			}
			pipe->mixer = mdss_mdp_mixer_get(tmp,
					MDSS_MDP_MIXER_MUX_DEFAULT);
		}
		if (pipe->back_buf.num_planes) {
			buf = &pipe->back_buf;
		} else if (ctl->play_cnt == 0 && pipe->front_buf.num_planes) {
			pipe->params_changed++;
			buf = &pipe->front_buf;
		} else if (!pipe->params_changed) {
			continue;
		} else if (pipe->front_buf.num_planes) {
			buf = &pipe->front_buf;
		} else {
			pr_debug("no buf detected pnum=%d use solid fill\n",
					pipe->num);
			buf = NULL;
		}

		ret = mdss_mdp_pipe_queue_data(pipe, buf);
		if (IS_ERR_VALUE(ret)) {
			pr_warn("Unable to queue data for pnum=%d\n",
					pipe->num);
			mdss_mdp_mixer_pipe_unstage(pipe);
		}
	}

	if (mfd->panel.type == WRITEBACK_PANEL)
		ret = mdss_mdp_wb_kickoff(mfd);
	else
		ret = mdss_mdp_display_commit(mdp5_data->ctl, NULL);

	mutex_unlock(&mfd->lock);

	if (IS_ERR_VALUE(ret))
		goto commit_fail;

	mdss_mdp_overlay_update_pm(mdp5_data);

	ret = mdss_mdp_display_wait4comp(mdp5_data->ctl);

	if (ret == 0) {
		mutex_lock(&mfd->lock);
		if (!mdp5_data->sd_enabled && (sd_in_pipe == 1)) {
			ret = mdss_mdp_overlay_sd_ctrl(mfd, 1);
			if (ret == 0)
				mdp5_data->sd_enabled = 1;
		}
		mutex_unlock(&mfd->lock);
	}

	mdss_fb_update_notify_update(mfd);
commit_fail:
	mdss_mdp_overlay_cleanup(mfd);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
	mdss_mdp_ctl_notify(ctl, MDP_NOTIFY_FRAME_FLUSHED);

	mutex_unlock(&mdp5_data->ov_lock);
	if (ctl->shared_lock)
		mutex_unlock(ctl->shared_lock);

	return ret;
}

static int mdss_mdp_overlay_release(struct msm_fb_data_type *mfd, int ndx)
{
	struct mdss_mdp_pipe *pipe;
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	u32 pipe_ndx, unset_ndx = 0;
	int i, destroy_pipe;

	for (i = 0; unset_ndx != ndx && i < MDSS_MDP_MAX_SSPP; i++) {
		pipe_ndx = BIT(i);
		if (pipe_ndx & ndx) {
			unset_ndx |= pipe_ndx;
			pipe = mdss_mdp_pipe_get(mdp5_data->mdata, pipe_ndx);
			if (IS_ERR_OR_NULL(pipe)) {
				pr_warn("unknown pipe ndx=%x\n", pipe_ndx);
				continue;
			}

			mutex_lock(&mfd->lock);
			pipe->pid = 0;
			destroy_pipe = pipe->play_cnt == 0;

			if (!list_empty(&pipe->used_list)) {
				list_del_init(&pipe->used_list);
				if (!destroy_pipe)
					list_add(&pipe->cleanup_list,
						&mdp5_data->pipes_cleanup);
			}
			mutex_unlock(&mfd->lock);
			mdss_mdp_mixer_pipe_unstage(pipe);
			mdss_mdp_pipe_unmap(pipe);
			if (destroy_pipe)
				mdss_mdp_pipe_destroy(pipe);
		}
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

	if (!mfd->panel_power_on) {
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
	bool release_all)
{
	struct mdss_mdp_pipe *pipe;
	struct mdss_mdp_rotator_session *rot, *tmp;
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	u32 unset_ndx = 0;
	int cnt = 0;
	int pid = current->tgid;

	pr_debug("releasing all resources for fb%d pid=%d\n", mfd->index, pid);

	mutex_lock(&mdp5_data->ov_lock);
	mutex_lock(&mfd->lock);
	list_for_each_entry(pipe, &mdp5_data->pipes_used, used_list) {
		if (release_all || (pipe->pid == pid)) {
			unset_ndx |= pipe->ndx;
			cnt++;
		}
	}

	if (cnt == 0 && !list_empty(&mdp5_data->pipes_cleanup)) {
		pr_debug("overlay release on fb%d called without commit!",
			mfd->index);
		cnt++;
	}

	pr_debug("release_all=%d mfd->ref_cnt=%d unset_ndx=0x%x cnt=%d\n",
		release_all, mfd->ref_cnt, unset_ndx, cnt);

	mutex_unlock(&mfd->lock);

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
	struct mdss_data_type *mdata = mfd_to_mdata(mfd);

	pipe = mdss_mdp_pipe_get(mdata, req->id);
	if (IS_ERR_OR_NULL(pipe)) {
		pr_err("pipe ndx=%x doesn't exist\n", req->id);
		return pipe ? PTR_ERR(pipe) : -ENODEV;
	}

	pr_debug("ov queue pnum=%d\n", pipe->num);

	if (pipe->flags & MDP_SOLID_FILL)
		pr_warn("Unexpected buffer queue to a solid fill pipe\n");

	flags = (pipe->flags & MDP_SECURE_OVERLAY_SESSION);
	flags |= (pipe->flags & MDP_SECURE_DISPLAY_OVERLAY_SESSION);

	if (!mfd->panel_info->cont_splash_enabled)
		mdss_iommu_attach(mdata);

	src_data = &pipe->back_buf;
	if (src_data->num_planes) {
		pr_warn("dropped buffer pnum=%d play=%d addr=0x%x\n",
			pipe->num, pipe->play_cnt, src_data->p[0].addr);
		mdss_mdp_overlay_free_buf(src_data);
	}

	ret = mdss_mdp_overlay_get_buf(mfd, src_data, &req->data, 1, flags);
	if (IS_ERR_VALUE(ret)) {
		pr_err("src_data pmem error\n");
	}
	mdss_mdp_pipe_unmap(pipe);

	return ret;
}

static void mdss_mdp_overlay_force_cleanup(struct msm_fb_data_type *mfd)
{
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	struct mdss_mdp_ctl *ctl = mdp5_data->ctl;
	int ret;

	pr_debug("forcing cleanup to unset dma pipes on fb%d\n", mfd->index);

	/*
	 * video mode panels require the layer to be unstaged and wait for
	 * vsync to be able to release buffer.
	 */
	if (ctl && ctl->is_video_mode) {
		ret = mdss_mdp_display_commit(ctl, NULL);
		if (!IS_ERR_VALUE(ret))
			mdss_mdp_display_wait4comp(ctl);
	}

	mdss_mdp_overlay_cleanup(mfd);
}

static void mdss_mdp_overlay_force_dma_cleanup(struct mdss_data_type *mdata)
{
	struct mdss_mdp_pipe *pipe;
	int i;

	for (i = 0; i < mdata->ndma_pipes; i++) {
		pipe = mdata->dma_pipes + i;
		if (atomic_read(&pipe->ref_cnt) && pipe->mfd)
			mdss_mdp_overlay_force_cleanup(pipe->mfd);
	}
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

	if (!mfd->panel_power_on) {
		ret = -EPERM;
		goto done;
	}

	ret = mdss_mdp_overlay_start(mfd);
	if (ret) {
		pr_err("unable to start overlay %d (%d)\n", mfd->index, ret);
		goto done;
	}

	if (req->id & MDSS_MDP_ROT_SESSION_MASK) {
		mdss_mdp_overlay_force_dma_cleanup(mfd_to_mdata(mfd));

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

	pipe = mdss_mdp_mixer_stage_pipe(mdp5_data->ctl,
					MDSS_MDP_MIXER_MUX_LEFT,
					 MDSS_MDP_STAGE_BASE);
	if (pipe)
		fb_ndx |= pipe->ndx;

	pipe = mdss_mdp_mixer_stage_pipe(mdp5_data->ctl,
					MDSS_MDP_MIXER_MUX_RIGHT,
					 MDSS_MDP_STAGE_BASE);
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
					int mixer_mux,
					struct mdp_overlay *req_ov)
{
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	struct mdss_mdp_pipe *pipe;
	int ret;

	pipe = mdss_mdp_mixer_stage_pipe(mdp5_data->ctl, mixer_mux,
					 MDSS_MDP_STAGE_BASE);

	if (pipe == NULL) {
		struct mdss_mdp_mixer *mixer;

		mixer = mdss_mdp_mixer_get(mdp5_data->ctl,
					MDSS_MDP_MIXER_MUX_LEFT);
		if (!mixer) {
			pr_err("unable to retrieve mixer\n");
			return -ENODEV;
		}

		if (req_ov == NULL) {
			struct mdp_overlay req;
			struct fb_info *fbi = mfd->fbi;
			int bpp;

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

				req.flags |= MDSS_MDP_RIGHT_MIXER;
				req.src_rect.x = mixer->width;
				req.src_rect.w = fbi->var.xres - mixer->width;
			} else {
				req.src_rect.x = 0;
				req.src_rect.w = MIN(fbi->var.xres,
								mixer->width);
			}

			req.src_rect.y = 0;
			req.src_rect.h = req.src.height;
			req.dst_rect.x = 0;
			req.dst_rect.y = 0;
			req.dst_rect.w = req.src_rect.w;
			req.dst_rect.h = req.src_rect.h;
			req.z_order = MDSS_MDP_STAGE_BASE;

			pr_debug("allocating base pipe mux=%d\n", mixer_mux);

			ret = mdss_mdp_overlay_pipe_setup(mfd, &req, &pipe);
			if (ret)
				return ret;
		} else {
			if (mixer_mux == MDSS_MDP_MIXER_MUX_RIGHT) {
				req_ov->id = MSMFB_NEW_REQUEST;
				req_ov->flags |= MDSS_MDP_RIGHT_MIXER;
				req_ov->src_rect.w = MIN(mixer->width,
						req_ov->src_rect.w >> 1);
				req_ov->dst_rect.w = req_ov->src_rect.w;
				req_ov->src_rect.x = req_ov->src_rect.w;
				req_ov->dst_rect.x = 0;
			}

			ret = mdss_mdp_overlay_pipe_setup(mfd, req_ov, &pipe);
			if (ret)
				return ret;
		}
	}

	pr_debug("ctl=%d pnum=%d\n", mdp5_data->ctl->num, pipe->num);

	*ppipe = pipe;
	return 0;
}

static void mdss_mdp_overlay_pan_display(struct msm_fb_data_type *mfd,
					struct mdp_overlay *req,
					int image_size,
					int *pipe_ndx)
{
	struct mdss_mdp_data *buf;
	struct mdss_mdp_pipe *pipe;
	struct fb_info *fbi;
	struct mdss_overlay_private *mdp5_data;
	u32 offset;
	int bpp, ret;

	if (!mfd)
		return;

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

	if (!mfd->panel_power_on) {
		mutex_unlock(&mdp5_data->ov_lock);
		return;
	}

	bpp = fbi->var.bits_per_pixel / 8;
	offset = fbi->var.xoffset * bpp +
		 fbi->var.yoffset * fbi->fix.line_length;

	if (offset > fbi->fix.smem_len) {
		pr_err("invalid fb offset=%u total length=%u\n",
		       offset, fbi->fix.smem_len);
		goto pan_display_error;
	}

	ret = mdss_mdp_overlay_start(mfd);
	if (ret) {
		pr_err("unable to start overlay %d (%d)\n", mfd->index, ret);
		goto pan_display_error;
	}

	ret = mdss_mdp_overlay_get_fb_pipe(mfd, &pipe,
					MDSS_MDP_MIXER_MUX_LEFT, req);
	if (ret) {
		pr_err("unable to allocate base pipe\n");
		goto pan_display_error;
	}

	if (mdss_mdp_pipe_map(pipe)) {
		pr_err("unable to map base pipe\n");
		goto pan_display_error;
	}
	if (pipe_ndx)
		pipe_ndx[0] = pipe->ndx;

	buf = &pipe->back_buf;
	if (is_mdss_iommu_attached()) {
		if (!mfd->iova) {
			pr_err("mfd iova is zero\n");
			goto attach_err;
		}
		buf->p[0].addr = mfd->iova;
	} else {
		buf->p[0].addr = fbi->fix.smem_start;
	}

	buf->p[0].addr += offset;
	if (image_size)
		buf->p[0].len = image_size;
	else
		buf->p[0].len = fbi->fix.smem_len - offset;
	buf->num_planes = 1;
	mdss_mdp_pipe_unmap(pipe);

	if (fbi->var.xres > MAX_MIXER_WIDTH || mfd->split_display) {
		ret = mdss_mdp_overlay_get_fb_pipe(mfd, &pipe,
					   MDSS_MDP_MIXER_MUX_RIGHT, req);
		if (ret) {
			pr_err("unable to allocate right base pipe\n");
			goto attach_err;
		}
		if (mdss_mdp_pipe_map(pipe)) {
			pr_err("unable to map right base pipe\n");
			goto attach_err;
		}
		if (pipe_ndx)
			pipe_ndx[1] = pipe->ndx;

		pipe->back_buf = *buf;
		mdss_mdp_pipe_unmap(pipe);
	}
	mutex_unlock(&mdp5_data->ov_lock);

	if ((fbi->var.activate & FB_ACTIVATE_VBL) ||
	    (fbi->var.activate & FB_ACTIVATE_FORCE))
		mfd->mdp.kickoff_fnc(mfd, NULL);

	return;

attach_err:
	mutex_unlock(&mdp5_data->ov_lock);
	mdss_mdp_overlay_unset(mfd, pipe->ndx);
	if (pipe_ndx)
		pipe_ndx[0] = INVALID_PIPE_INDEX;
	return;
pan_display_error:
	mutex_unlock(&mdp5_data->ov_lock);
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
			&& !ctl->power_on) {
		pr_debug("fb%d vsync pending first update en=%d\n",
				mfd->index, en);
		return -EPERM;
	}

	pr_debug("fb%d vsync en=%d\n", mfd->index, en);

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
	if (en)
		rc = ctl->add_vsync_handler(ctl, &ctl->vsync_handler);
	else
		rc = ctl->remove_vsync_handler(ctl, &ctl->vsync_handler);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);

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

	if (!mdp5_data->ctl || !mdp5_data->ctl->power_on)
		return 0;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	if (!pdata) {
		pr_err("no panel connected for fb%d\n", mfd->index);
		return -ENODEV;
	}

	ret = snprintf(buf, PAGE_SIZE, "%d\n",
		       pdata->panel_info.mipi.frame_rate);
	pr_debug("%s: '%d'\n", __func__,
		pdata->panel_info.mipi.frame_rate);

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

	if (!mdp5_data->ctl || !mdp5_data->ctl->power_on)
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

	if (dfps < 30) {
		pr_err("Unsupported FPS. Configuring to min_fps = 30\n");
		dfps = 30;
		rc = mdss_mdp_ctl_update_fps(mdp5_data->ctl, dfps);
	} else if (dfps > 60) {
		pr_err("Unsupported FPS. Configuring to max_fps = 60\n");
		dfps = 60;
		rc = mdss_mdp_ctl_update_fps(mdp5_data->ctl, dfps);
	} else {
		rc = mdss_mdp_ctl_update_fps(mdp5_data->ctl, dfps);
	}
	if (!rc) {
		pr_info("%s: configured to '%d' FPS\n", __func__, dfps);
	} else {
		pr_err("Failed to configure '%d' FPS. rc = %d\n",
							dfps, rc);
		return rc;
	}
	pdata->panel_info.new_fps = dfps;
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
			&& !mdp5_data->ctl->power_on))
		return -EAGAIN;

	vsync_ticks = ktime_to_ns(mdp5_data->vsync_time);

	pr_debug("fb%d vsync=%llu", mfd->index, vsync_ticks);
	ret = scnprintf(buf, PAGE_SIZE, "VSYNC=%llu", vsync_ticks);

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


static DEVICE_ATTR(vsync_event, S_IRUGO, mdss_mdp_vsync_show_event, NULL);
static DEVICE_ATTR(ad, S_IRUGO | S_IWUSR | S_IWGRP, mdss_mdp_ad_show,
	mdss_mdp_ad_store);

static struct attribute *mdp_overlay_sysfs_attrs[] = {
	&dev_attr_vsync_event.attr,
	&dev_attr_ad.attr,
	NULL,
};

static struct attribute_group mdp_overlay_sysfs_group = {
	.attrs = mdp_overlay_sysfs_attrs,
};

static int mdss_mdp_hw_cursor_update(struct msm_fb_data_type *mfd,
				     struct fb_cursor *cursor)
{
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	struct mdss_mdp_mixer *mixer;
	struct fb_image *img = &cursor->image;
	u32 blendcfg;
	int off, ret = 0;

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
			return -ENOMEM;
		}
	}

	mixer = mdss_mdp_mixer_get(mdp5_data->ctl, MDSS_MDP_MIXER_MUX_DEFAULT);
	off = MDSS_MDP_REG_LM_OFFSET(mixer->num);

	if ((img->width > MDSS_MDP_CURSOR_WIDTH) ||
		(img->height > MDSS_MDP_CURSOR_HEIGHT) ||
		(img->depth != 32))
		return -EINVAL;

	pr_debug("mixer=%d enable=%x set=%x\n", mixer->num, cursor->enable,
			cursor->set);

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
	blendcfg = MDSS_MDP_REG_READ(off + MDSS_MDP_REG_LM_CURSOR_BLEND_CONFIG);

	if (cursor->set & FB_CUR_SETPOS)
		MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_LM_CURSOR_START_XY,
				   (img->dy << 16) | img->dx);

	if (cursor->set & FB_CUR_SETIMAGE) {
		int calpha_en, transp_en, alpha, size, cursor_addr;
		ret = copy_from_user(mfd->cursor_buf, img->data,
				     img->width * img->height * 4);
		if (ret)
			return ret;

		if (is_mdss_iommu_attached())
			cursor_addr = mfd->cursor_buf_iova;
		else
			cursor_addr = mfd->cursor_buf_phys;

		if (img->bg_color == 0xffffffff)
			transp_en = 0;
		else
			transp_en = 1;

		alpha = (img->fg_color & 0xff000000) >> 24;

		if (alpha)
			calpha_en = 0x0; /* xrgb */
		else
			calpha_en = 0x2; /* argb */

		size = (img->height << 16) | img->width;
		MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_LM_CURSOR_IMG_SIZE, size);
		MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_LM_CURSOR_SIZE, size);
		MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_LM_CURSOR_STRIDE,
				   img->width * 4);
		MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_LM_CURSOR_BASE_ADDR,
				   cursor_addr);

		wmb();

		blendcfg &= ~0x1;
		blendcfg |= (transp_en << 3) | (calpha_en << 1);
		MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_LM_CURSOR_BLEND_CONFIG,
				   blendcfg);
		if (calpha_en)
			MDSS_MDP_REG_WRITE(off +
					   MDSS_MDP_REG_LM_CURSOR_BLEND_PARAM,
					   alpha);

		if (transp_en) {
			MDSS_MDP_REG_WRITE(off +
				   MDSS_MDP_REG_LM_CURSOR_BLEND_TRANSP_LOW0,
				   ((img->bg_color & 0xff00) << 8) |
				   (img->bg_color & 0xff));
			MDSS_MDP_REG_WRITE(off +
				   MDSS_MDP_REG_LM_CURSOR_BLEND_TRANSP_LOW1,
				   ((img->bg_color & 0xff0000) >> 16));
			MDSS_MDP_REG_WRITE(off +
				   MDSS_MDP_REG_LM_CURSOR_BLEND_TRANSP_HIGH0,
				   ((img->bg_color & 0xff00) << 8) |
				   (img->bg_color & 0xff));
			MDSS_MDP_REG_WRITE(off +
				   MDSS_MDP_REG_LM_CURSOR_BLEND_TRANSP_HIGH1,
				   ((img->bg_color & 0xff0000) >> 16));
		}
	}

	if (!cursor->enable != !(blendcfg & 0x1)) {
		if (cursor->enable) {
			pr_debug("enable hw cursor on mixer=%d\n", mixer->num);
			blendcfg |= 0x1;
		} else {
			pr_debug("disable hw cursor on mixer=%d\n", mixer->num);
			blendcfg &= ~0x1;
		}

		MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_LM_CURSOR_BLEND_CONFIG,
				   blendcfg);

		mixer->cursor_enabled = cursor->enable;
		mixer->params_changed++;
	}

	mixer->ctl->flush_bits |= BIT(6) << mixer->num;
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);

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
	u32 copyback = 0;
	u32 copy_from_kernel = 0;

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
	u32 block;
	u32 pp_bus_handle;
	static int req = -1;

	switch (cmd) {
	case MSMFB_HISTOGRAM_START:
		if (!mfd->panel_power_on)
			return -EPERM;

		pp_bus_handle = mdss_mdp_get_mdata()->pp_bus_hdl;
		req = msm_bus_scale_client_update_request(pp_bus_handle,
				PP_CLK_CFG_ON);
		if (req)
			pr_err("Updated pp_bus_scale failed, ret = %d", req);

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

		if (!req) {
			pp_bus_handle = mdss_mdp_get_mdata()->pp_bus_hdl;
			req = msm_bus_scale_client_update_request(pp_bus_handle,
				 PP_CLK_CFG_OFF);
			if (req)
				pr_err("Updated pp_bus_scale failed, ret = %d",
					req);
		}
		break;

	case MSMFB_HISTOGRAM:
		if (!mfd->panel_power_on)
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
		if (!mfd->panel_power_on)
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

	caps->max_smp_cnt = mdss_res->smp_mb_cnt;
	caps->smp_per_pipe = mdata->smp_mb_per_pipe;

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
	case metadata_op_crc:
		if (!mfd->panel_power_on)
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

	if (mfd->split_display && pdata->next) {
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
	int rc;
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
	}

	if (!mfd->panel_info->cont_splash_enabled &&
		(mfd->panel_info->type != DTV_PANEL)) {
		rc = mdss_mdp_overlay_start(mfd);
		if (!IS_ERR_VALUE(rc) &&
			(mfd->panel_info->type != WRITEBACK_PANEL))
			rc = mdss_mdp_overlay_kickoff(mfd, NULL);
	} else {
		rc = mdss_mdp_ctl_setup(mdp5_data->ctl);
		if (rc)
			return rc;
	}

	if (IS_ERR_VALUE(rc)) {
		pr_err("Failed to turn on fb%d\n", mfd->index);
		mdss_mdp_overlay_off(mfd);
	}
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

	if (!mdp5_data->ctl->power_on)
		return 0;

	mdss_mdp_overlay_free_fb_pipe(mfd);

	mixer = mdss_mdp_mixer_get(mdp5_data->ctl, MDSS_MDP_MIXER_MUX_LEFT);
	if (mixer)
		mixer->cursor_enabled = 0;

	mixer = mdss_mdp_mixer_get(mdp5_data->ctl, MDSS_MDP_MIXER_MUX_RIGHT);
	if (mixer)
		mixer->cursor_enabled = 0;

	mutex_lock(&mfd->lock);
	need_cleanup = !list_empty(&mdp5_data->pipes_cleanup);
	mutex_unlock(&mfd->lock);

	if (need_cleanup) {
		pr_debug("cleaning up pipes on fb%d\n", mfd->index);
		mdss_mdp_overlay_kickoff(mfd, NULL);
	}

	rc = mdss_mdp_ctl_stop(mdp5_data->ctl);
	if (rc == 0) {
		__mdss_mdp_overlay_free_list_purge(mfd);
		mdss_mdp_ctl_notifier_unregister(mdp5_data->ctl,
				&mfd->mdp_sync_pt_data.notifier);

		if (!mfd->ref_cnt) {
			mdp5_data->borderfill_enable = false;
			mdss_mdp_ctl_destroy(mdp5_data->ctl);
			mdp5_data->ctl = NULL;
		}

		if (atomic_dec_return(&ov_active_panels) == 0)
			mdss_mdp_rotator_release_all();

		rc = pm_runtime_put(&mfd->pdev->dev);
		if (rc)
			pr_err("unable to suspend w/pm_runtime_put (%d)\n", rc);
	}

	return rc;
}

int mdss_panel_register_done(struct mdss_panel_data *pdata)
{
	if (pdata->panel_info.cont_splash_enabled)
		mdss_mdp_footswitch_ctrl_splash(1);

	return 0;
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
	int i, j;
	u32 reg;
	struct mdss_mdp_pipe *pipe = NULL;
	struct mdss_mdp_ctl *ctl = NULL;

	if (!mdp5_data->ctl) {
		ctl = __mdss_mdp_overlay_ctl_init(mfd);
		if (IS_ERR_OR_NULL(ctl)) {
			rc = PTR_ERR(ctl);
			goto error;
		}
		mdp5_data->ctl = ctl;
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

	for (i = 0; i < mdata->nmixers_intf; i++) {
		reg = mdss_mdp_ctl_read(ctl, MDSS_MDP_REG_CTL_LAYER(i));
		pr_debug("for lm%d reg = 0x%09x\n", i, reg);
		for (j = MDSS_MDP_SSPP_VIG0; j < MDSS_MDP_MAX_SSPP; j++) {
			u32 cfg = j * 3;
			if ((j == MDSS_MDP_SSPP_VIG3) ||
				(j == MDSS_MDP_SSPP_RGB3)) {
				/* Add 2 to account for Cursor & Border bits */
				cfg += 2;
			}
			if (reg & (0x7 << cfg)) {
				pr_debug("Pipe %d staged\n", j);
				pipe = mdss_mdp_pipe_search(mdata, BIT(j));
				if (!pipe) {
					pr_warn("Invalid pipe %d staged\n", j);
					continue;
				}

				rc = mdss_mdp_pipe_handoff(pipe);
				if (rc) {
					pr_err("Failed to handoff pipe num %d\n"
						, pipe->num);
					goto error;
				}

				rc = mdss_mdp_mixer_handoff(ctl, i, pipe);
				if (rc) {
					pr_err("failed to handoff mixer num %d\n"
						, i);
					goto error;
				}
			}
		}
	}

	rc = mdss_mdp_smp_handoff(mdata);
	if (rc)
		pr_err("Failed to handoff smps\n");

	mdp5_data->handoff = true;

error:
	if (rc && ctl) {
		__mdss_mdp_handoff_cleanup_pipes(mfd, MDSS_MDP_PIPE_TYPE_RGB);
		__mdss_mdp_handoff_cleanup_pipes(mfd, MDSS_MDP_PIPE_TYPE_VIG);
		__mdss_mdp_handoff_cleanup_pipes(mfd, MDSS_MDP_PIPE_TYPE_DMA);
		mdss_mdp_ctl_destroy(ctl);
		mdp5_data->ctl = NULL;
		mdp5_data->handoff = false;
	}

	return rc;
}

static int mdss_mdp_overlay_splash_image(struct msm_fb_data_type *mfd,
						int *pipe_ndx, int splash_event)
{
	struct mdp_overlay req;
	int rc = 0;
	struct fb_info *fbi = NULL;
	int image_len = 0;

	if (!mfd || !mfd->fbi || !mfd->fbi->screen_base || !pipe_ndx) {
		pr_err("Invalid input parameter\n");
		return -EINVAL;
	}

	fbi = mfd->fbi;
	image_len = SPLASH_IMAGE_WIDTH * SPLASH_IMAGE_HEIGHT * SPLASH_IMAGE_BPP;

	if (SPLASH_IMAGE_WIDTH > fbi->var.xres ||
			SPLASH_IMAGE_HEIGHT > fbi->var.yres ||
			SPLASH_IMAGE_BPP > fbi->var.bits_per_pixel / 8 ||
			image_len > fbi->fix.smem_len) {
		pr_err("Invalid splash parameter configuration\n");
		return -EINVAL;
	}

	if (splash_event == MDP_CREATE_SPLASH_OV) {
		pipe_ndx[0] = INVALID_PIPE_INDEX;
		pipe_ndx[1] = INVALID_PIPE_INDEX;

		memset(&req, 0, sizeof(struct mdp_overlay));
		req.src.width = req.dst_rect.w = req.src_rect.w =
				SPLASH_IMAGE_WIDTH;
		req.src.height = req.dst_rect.h = req.src_rect.h =
				SPLASH_IMAGE_HEIGHT;
		req.src.format = SPLASH_IMAGE_FORMAT;
		req.id = MSMFB_NEW_REQUEST;
		req.z_order = MDSS_MDP_STAGE_0;
		req.is_fg = 1;
		req.alpha = 0xff;
		req.transp_mask = MDP_TRANSP_NOP;
		req.dst_rect.x =
			(fbi->var.xres >> 1) - (SPLASH_IMAGE_WIDTH >> 1);
		req.dst_rect.y =
			(fbi->var.yres >> 1) - (SPLASH_IMAGE_HEIGHT >> 1);

		memcpy(fbi->screen_base, splash_bgr888_image, image_len);
		mdss_mdp_overlay_pan_display(mfd, &req, image_len, pipe_ndx);

	} else if (splash_event == MDP_REMOVE_SPLASH_OV) {
		if (pipe_ndx[0] != INVALID_PIPE_INDEX)
			mdss_mdp_overlay_unset(mfd, pipe_ndx[0]);
		if (pipe_ndx[1] != INVALID_PIPE_INDEX)
			mdss_mdp_overlay_unset(mfd, pipe_ndx[1]);
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
	struct msm_sync_pt_data *sync_pt_data;

	if (!mdp5_data->ctl || !mdp5_data->ctl->mfd)
		return;

	if (!mdp5_data->ctl->remove_vsync_handler)
		return;

	sync_pt_data = &mdp5_data->ctl->mfd->mdp_sync_pt_data;
	mutex_lock(&sync_pt_data->sync_mutex);
	if (mdp5_data->retire_cnt > 0) {
		sw_sync_timeline_inc(mdp5_data->vsync_timeline, 1);

		mdp5_data->retire_cnt--;
		if (mdp5_data->retire_cnt == 0) {
			mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
			mdp5_data->ctl->remove_vsync_handler(mdp5_data->ctl,
					&mdp5_data->vsync_retire_handler);
			mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
		}
	}
	mutex_unlock(&sync_pt_data->sync_mutex);
}

static struct sync_fence *
__vsync_retire_get_fence(struct msm_sync_pt_data *sync_pt_data)
{
	struct msm_fb_data_type *mfd;
	struct mdss_overlay_private *mdp5_data;
	struct mdss_mdp_ctl *ctl;
	int rc, value;

	mfd = container_of(sync_pt_data, typeof(*mfd), mdp_sync_pt_data);
	mdp5_data = mfd_to_mdp5_data(mfd);

	if (!mdp5_data || !mdp5_data->ctl)
		return ERR_PTR(-ENODEV);

	ctl = mdp5_data->ctl;
	if (!ctl->add_vsync_handler)
		return ERR_PTR(-EOPNOTSUPP);

	if (!ctl->power_on) {
		pr_debug("fb%d vsync pending first update\n", mfd->index);
		return ERR_PTR(-EPERM);
	}

	if (mdp5_data->retire_cnt == 0) {
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
		rc = ctl->add_vsync_handler(ctl,
				&mdp5_data->vsync_retire_handler);
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
		if (IS_ERR_VALUE(rc))
			return ERR_PTR(rc);
	}
	value = mdp5_data->vsync_timeline->value + 1 + mdp5_data->retire_cnt;
	mdp5_data->retire_cnt++;

	return mdss_fb_sync_get_fence(mdp5_data->vsync_timeline,
			"mdp-retire", value);
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

int mdss_mdp_overlay_init(struct msm_fb_data_type *mfd)
{
	struct device *dev = mfd->fbi->dev;
	struct msm_mdp_interface *mdp5_interface = &mfd->mdp;
	struct mdss_overlay_private *mdp5_data = NULL;
	int rc;

	mdp5_interface->on_fnc = mdss_mdp_overlay_on;
	mdp5_interface->off_fnc = mdss_mdp_overlay_off;
	mdp5_interface->release_fnc = __mdss_mdp_overlay_release_all;
	mdp5_interface->do_histogram = NULL;
	mdp5_interface->cursor_update = mdss_mdp_hw_cursor_update;
	mdp5_interface->dma_fnc = mdss_mdp_overlay_pan_display;
	mdp5_interface->ioctl_handler = mdss_mdp_overlay_ioctl_handler;
	mdp5_interface->panel_register_done = mdss_panel_register_done;
	mdp5_interface->kickoff_fnc = mdss_mdp_overlay_kickoff;
	mdp5_interface->get_sync_fnc = mdss_mdp_rotator_sync_pt_get;
	mdp5_interface->splash_fnc = mdss_mdp_overlay_splash_image;

	mdp5_data = kmalloc(sizeof(struct mdss_overlay_private), GFP_KERNEL);
	if (!mdp5_data) {
		pr_err("fail to allocate mdp5 private data structure");
		return -ENOMEM;
	}
	memset(mdp5_data, 0, sizeof(struct mdss_overlay_private));

	INIT_LIST_HEAD(&mdp5_data->pipes_used);
	INIT_LIST_HEAD(&mdp5_data->pipes_cleanup);
	INIT_LIST_HEAD(&mdp5_data->rot_proc_list);
	mutex_init(&mdp5_data->ov_lock);
	mdp5_data->hw_refresh = true;
	mdp5_data->overlay_play_enable = true;

	mdp5_data->mdata = dev_get_drvdata(mfd->pdev->dev.parent);
	if (!mdp5_data->mdata) {
		pr_err("unable to initialize overlay for fb%d\n", mfd->index);
		rc = -ENODEV;
		goto init_fail;
	}
	mfd->mdp.private1 = mdp5_data;

	rc = mdss_mdp_overlay_fb_parse_dt(mfd);
	if (rc)
		return rc;

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

	if (mfd->panel_info->type == MIPI_VIDEO_PANEL) {
		rc = sysfs_create_group(&dev->kobj,
			&dynamic_fps_fs_attrs_group);
		if (rc) {
			pr_err("Error dfps sysfs creation ret=%d\n", rc);
			goto init_fail;
		}
	} else if (mfd->panel_info->type == MIPI_CMD_PANEL) {
		rc = __vsync_retire_setup(mfd);
		if (IS_ERR_VALUE(rc)) {
			pr_err("unable to create vsync timeline\n");
			goto init_fail;
		}
	}
	mfd->mdp_sync_pt_data.async_wait_fences = true;
	rc = sysfs_create_link_nowarn(&dev->kobj,
			&mdp5_data->mdata->pdev->dev.kobj, "mdp");
	if (rc)
		pr_warn("problem creating link to mdp sysfs\n");

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

	return rc;
init_fail:
	kfree(mdp5_data);
	return rc;
}

static __ref int mdss_mdp_overlay_splash_parse_dt(struct msm_fb_data_type *mfd)
{
	struct platform_device *pdev = mfd->pdev;
	struct mdss_overlay_private *mdp5_mdata = mfd_to_mdp5_data(mfd);
	int len = 0, rc = 0;
	u32 offsets[2];

	of_find_property(pdev->dev.of_node, "qcom,memblock-reserve", &len);

	if (len < 1) {
		pr_debug("mem reservation for splash screen fb not present\n");
		rc = -EINVAL;
		goto error;
	}

	len = len/sizeof(u32);

	rc = of_property_read_u32_array(pdev->dev.of_node,
			"qcom,memblock-reserve", offsets, len);
	if (rc) {
		pr_debug("Error reading mem reserve settings for fb\n");
		goto error;
	}

	if (!memblock_is_reserved(offsets[0])) {
		pr_debug("failed to reserve memory for fb splash\n");
		rc = -EINVAL;
		goto error;
	}

	mdp5_mdata->splash_mem_addr = offsets[0];
	mdp5_mdata->splash_mem_size = offsets[1];
	pr_debug("memaddr=%x size=%x\n", mdp5_mdata->splash_mem_addr,
		mdp5_mdata->splash_mem_size);

error:
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

	rc = mdss_mdp_overlay_splash_parse_dt(mfd);
	if (rc && mfd->panel_info->cont_splash_enabled) {
		pr_err("No rsvd mem found in DT for splash screen\n");
	} else {
		pr_debug("Mem reservation not reqd if cont splash diasbled\n");
		rc = 0;
	}

	return rc;
}
