/*
 * Copyright (C) 2014-2017 The Linux Foundation. All rights reserved.
 * Copyright (C) 2013 Red Hat
 * Author: Rob Clark <robdclark@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define pr_fmt(fmt)	"[drm:%s:%d] " fmt, __func__, __LINE__

#include <linux/debugfs.h>
#include <uapi/drm/sde_drm.h>
#include <uapi/drm/msm_drm_pp.h>

#include "msm_prop.h"

#include "sde_kms.h"
#include "sde_fence.h"
#include "sde_formats.h"
#include "sde_hw_sspp.h"
#include "sde_trace.h"
#include "sde_crtc.h"
#include "sde_vbif.h"
#include "sde_plane.h"
#include "sde_color_processing.h"

#define SDE_DEBUG_PLANE(pl, fmt, ...) SDE_DEBUG("plane%d " fmt,\
		(pl) ? (pl)->base.base.id : -1, ##__VA_ARGS__)

#define SDE_ERROR_PLANE(pl, fmt, ...) SDE_ERROR("plane%d " fmt,\
		(pl) ? (pl)->base.base.id : -1, ##__VA_ARGS__)

#define DECIMATED_DIMENSION(dim, deci) (((dim) + ((1 << (deci)) - 1)) >> (deci))
#define PHASE_STEP_SHIFT	21
#define PHASE_STEP_UNIT_SCALE   ((int) (1 << PHASE_STEP_SHIFT))
#define PHASE_RESIDUAL		15

#define SHARP_STRENGTH_DEFAULT	32
#define SHARP_EDGE_THR_DEFAULT	112
#define SHARP_SMOOTH_THR_DEFAULT	8
#define SHARP_NOISE_THR_DEFAULT	2

#define SDE_NAME_SIZE  12

#define SDE_PLANE_COLOR_FILL_FLAG	BIT(31)

/* dirty bits for update function */
#define SDE_PLANE_DIRTY_RECTS	0x1
#define SDE_PLANE_DIRTY_FORMAT	0x2
#define SDE_PLANE_DIRTY_SHARPEN	0x4
#define SDE_PLANE_DIRTY_ALL	0xFFFFFFFF

/**
 * enum sde_plane_qos - Different qos configurations for each pipe
 *
 * @SDE_PLANE_QOS_VBLANK_CTRL: Setup VBLANK qos for the pipe.
 * @SDE_PLANE_QOS_VBLANK_AMORTIZE: Enables Amortization within pipe.
 *	this configuration is mutually exclusive from VBLANK_CTRL.
 * @SDE_PLANE_QOS_PANIC_CTRL: Setup panic for the pipe.
 */
enum sde_plane_qos {
	SDE_PLANE_QOS_VBLANK_CTRL = BIT(0),
	SDE_PLANE_QOS_VBLANK_AMORTIZE = BIT(1),
	SDE_PLANE_QOS_PANIC_CTRL = BIT(2),
};

/*
 * struct sde_plane - local sde plane structure
 * @csc_cfg: Decoded user configuration for csc
 * @csc_usr_ptr: Points to csc_cfg if valid user config available
 * @csc_ptr: Points to sde_csc_cfg structure to use for current
 */
struct sde_plane {
	struct drm_plane base;

	int mmu_id;

	struct mutex lock;

	enum sde_sspp pipe;
	uint32_t features;      /* capabilities from catalog */
	uint32_t nformats;
	uint32_t formats[64];

	struct sde_hw_pipe *pipe_hw;
	struct sde_hw_pipe_cfg pipe_cfg;
	struct sde_hw_sharp_cfg sharp_cfg;
	struct sde_hw_scaler3_cfg *scaler3_cfg;
	struct sde_hw_pipe_qos_cfg pipe_qos_cfg;
	uint32_t color_fill;
	bool is_error;
	bool is_rt_pipe;

	struct sde_hw_pixel_ext pixel_ext;
	bool pixel_ext_usr;

	struct sde_csc_cfg csc_cfg;
	struct sde_csc_cfg *csc_usr_ptr;
	struct sde_csc_cfg *csc_ptr;

	const struct sde_sspp_sub_blks *pipe_sblk;

	char pipe_name[SDE_NAME_SIZE];

	struct msm_property_info property_info;
	struct msm_property_data property_data[PLANE_PROP_COUNT];
	struct drm_property_blob *blob_info;

	/* debugfs related stuff */
	struct dentry *debugfs_root;
	struct sde_debugfs_regset32 debugfs_src;
	struct sde_debugfs_regset32 debugfs_scaler;
	struct sde_debugfs_regset32 debugfs_csc;
};

#define to_sde_plane(x) container_of(x, struct sde_plane, base)

static bool sde_plane_enabled(struct drm_plane_state *state)
{
	return state && state->fb && state->crtc;
}

/**
 * _sde_plane_calc_fill_level - calculate fill level of the given source format
 * @plane:		Pointer to drm plane
 * @fmt:		Pointer to source buffer format
 * @src_wdith:		width of source buffer
 * Return: fill level corresponding to the source buffer/format or 0 if error
 */
static inline int _sde_plane_calc_fill_level(struct drm_plane *plane,
		const struct sde_format *fmt, u32 src_width)
{
	struct sde_plane *psde;
	u32 fixed_buff_size;
	u32 total_fl;

	if (!plane || !fmt) {
		SDE_ERROR("invalid arguments\n");
		return 0;
	}

	psde = to_sde_plane(plane);
	fixed_buff_size = psde->pipe_sblk->pixel_ram_size;

	if (fmt->fetch_planes == SDE_PLANE_PSEUDO_PLANAR) {
		if (fmt->chroma_sample == SDE_CHROMA_420) {
			/* NV12 */
			total_fl = (fixed_buff_size / 2) /
				((src_width + 32) * fmt->bpp);
		} else {
			/* non NV12 */
			total_fl = (fixed_buff_size) /
				((src_width + 32) * fmt->bpp);
		}
	} else {
		total_fl = (fixed_buff_size * 2) /
			((src_width + 32) * fmt->bpp);
	}

	SDE_DEBUG("plane%u: pnum:%d fmt:%x w:%u fl:%u\n",
			plane->base.id, psde->pipe - SSPP_VIG0,
			fmt->base.pixel_format, src_width, total_fl);

	return total_fl;
}

/**
 * _sde_plane_get_qos_lut_linear - get linear LUT mapping
 * @total_fl:		fill level
 * Return: LUT setting corresponding to the fill level
 */
static inline u32 _sde_plane_get_qos_lut_linear(u32 total_fl)
{
	u32 qos_lut;

	if (total_fl <= 4)
		qos_lut = 0x1B;
	else if (total_fl <= 5)
		qos_lut = 0x5B;
	else if (total_fl <= 6)
		qos_lut = 0x15B;
	else if (total_fl <= 7)
		qos_lut = 0x55B;
	else if (total_fl <= 8)
		qos_lut = 0x155B;
	else if (total_fl <= 9)
		qos_lut = 0x555B;
	else if (total_fl <= 10)
		qos_lut = 0x1555B;
	else if (total_fl <= 11)
		qos_lut = 0x5555B;
	else if (total_fl <= 12)
		qos_lut = 0x15555B;
	else
		qos_lut = 0x55555B;

	return qos_lut;
}

/**
 * _sde_plane_get_qos_lut_macrotile - get macrotile LUT mapping
 * @total_fl:		fill level
 * Return: LUT setting corresponding to the fill level
 */
static inline u32 _sde_plane_get_qos_lut_macrotile(u32 total_fl)
{
	u32 qos_lut;

	if (total_fl <= 10)
		qos_lut = 0x1AAff;
	else if (total_fl <= 11)
		qos_lut = 0x5AAFF;
	else if (total_fl <= 12)
		qos_lut = 0x15AAFF;
	else
		qos_lut = 0x55AAFF;

	return qos_lut;
}

/**
 * _sde_plane_set_qos_lut - set QoS LUT of the given plane
 * @plane:		Pointer to drm plane
 * @fb:			Pointer to framebuffer associated with the given plane
 */
static void _sde_plane_set_qos_lut(struct drm_plane *plane,
		struct drm_framebuffer *fb)
{
	struct sde_plane *psde;
	const struct sde_format *fmt = NULL;
	u32 qos_lut;
	u32 total_fl = 0;

	if (!plane || !fb) {
		SDE_ERROR("invalid arguments plane %d fb %d\n",
				plane != 0, fb != 0);
		return;
	}

	psde = to_sde_plane(plane);

	if (!psde->pipe_hw || !psde->pipe_sblk) {
		SDE_ERROR("invalid arguments\n");
		return;
	} else if (!psde->pipe_hw->ops.setup_creq_lut) {
		return;
	}

	if (!psde->is_rt_pipe) {
		qos_lut = psde->pipe_sblk->creq_lut_nrt;
	} else {
		fmt = sde_get_sde_format_ext(
				fb->pixel_format,
				fb->modifier,
				drm_format_num_planes(fb->pixel_format));
		total_fl = _sde_plane_calc_fill_level(plane, fmt,
				psde->pipe_cfg.src_rect.w);

		if (SDE_FORMAT_IS_LINEAR(fmt))
			qos_lut = _sde_plane_get_qos_lut_linear(total_fl);
		else
			qos_lut = _sde_plane_get_qos_lut_macrotile(total_fl);
	}

	psde->pipe_qos_cfg.creq_lut = qos_lut;

	trace_sde_perf_set_qos_luts(psde->pipe - SSPP_VIG0,
			(fmt) ? fmt->base.pixel_format : 0,
			psde->is_rt_pipe, total_fl, qos_lut,
			(fmt) ? SDE_FORMAT_IS_LINEAR(fmt) : 0);

	SDE_DEBUG("plane%u: pnum:%d fmt:%x rt:%d fl:%u lut:0x%x\n",
			plane->base.id,
			psde->pipe - SSPP_VIG0,
			(fmt) ? fmt->base.pixel_format : 0,
			psde->is_rt_pipe, total_fl, qos_lut);

	psde->pipe_hw->ops.setup_creq_lut(psde->pipe_hw, &psde->pipe_qos_cfg);
}

/**
 * _sde_plane_set_panic_lut - set danger/safe LUT of the given plane
 * @plane:		Pointer to drm plane
 * @fb:			Pointer to framebuffer associated with the given plane
 */
static void _sde_plane_set_danger_lut(struct drm_plane *plane,
		struct drm_framebuffer *fb)
{
	struct sde_plane *psde;
	const struct sde_format *fmt = NULL;
	u32 danger_lut, safe_lut;

	if (!plane || !fb) {
		SDE_ERROR("invalid arguments\n");
		return;
	}

	psde = to_sde_plane(plane);

	if (!psde->pipe_hw || !psde->pipe_sblk) {
		SDE_ERROR("invalid arguments\n");
		return;
	} else if (!psde->pipe_hw->ops.setup_danger_safe_lut) {
		return;
	}

	if (!psde->is_rt_pipe) {
		danger_lut = psde->pipe_sblk->danger_lut_nrt;
		safe_lut = psde->pipe_sblk->safe_lut_nrt;
	} else {
		fmt = sde_get_sde_format_ext(
				fb->pixel_format,
				fb->modifier,
				drm_format_num_planes(fb->pixel_format));

		if (SDE_FORMAT_IS_LINEAR(fmt)) {
			danger_lut = psde->pipe_sblk->danger_lut_linear;
			safe_lut = psde->pipe_sblk->safe_lut_linear;
		} else {
			danger_lut = psde->pipe_sblk->danger_lut_tile;
			safe_lut = psde->pipe_sblk->safe_lut_tile;
		}
	}

	psde->pipe_qos_cfg.danger_lut = danger_lut;
	psde->pipe_qos_cfg.safe_lut = safe_lut;

	trace_sde_perf_set_danger_luts(psde->pipe - SSPP_VIG0,
			(fmt) ? fmt->base.pixel_format : 0,
			(fmt) ? fmt->fetch_mode : 0,
			psde->pipe_qos_cfg.danger_lut,
			psde->pipe_qos_cfg.safe_lut);

	SDE_DEBUG("plane%u: pnum:%d fmt:%x mode:%d luts[0x%x, 0x%x]\n",
		plane->base.id,
		psde->pipe - SSPP_VIG0,
		fmt ? fmt->base.pixel_format : 0,
		fmt ? fmt->fetch_mode : -1,
		psde->pipe_qos_cfg.danger_lut,
		psde->pipe_qos_cfg.safe_lut);

	psde->pipe_hw->ops.setup_danger_safe_lut(psde->pipe_hw,
			&psde->pipe_qos_cfg);
}

/**
 * _sde_plane_set_qos_ctrl - set QoS control of the given plane
 * @plane:		Pointer to drm plane
 * @enable:		true to enable QoS control
 * @flags:		QoS control mode (enum sde_plane_qos)
 */
static void _sde_plane_set_qos_ctrl(struct drm_plane *plane,
	bool enable, u32 flags)
{
	struct sde_plane *psde;

	if (!plane) {
		SDE_ERROR("invalid arguments\n");
		return;
	}

	psde = to_sde_plane(plane);

	if (!psde->pipe_hw || !psde->pipe_sblk) {
		SDE_ERROR("invalid arguments\n");
		return;
	} else if (!psde->pipe_hw->ops.setup_qos_ctrl) {
		return;
	}

	if (flags & SDE_PLANE_QOS_VBLANK_CTRL) {
		psde->pipe_qos_cfg.creq_vblank = psde->pipe_sblk->creq_vblank;
		psde->pipe_qos_cfg.danger_vblank =
				psde->pipe_sblk->danger_vblank;
		psde->pipe_qos_cfg.vblank_en = enable;
	}

	 if (flags & SDE_PLANE_QOS_VBLANK_AMORTIZE) {
		/* this feature overrules previous VBLANK_CTRL */
		psde->pipe_qos_cfg.vblank_en = false;
		psde->pipe_qos_cfg.creq_vblank = 0; /* clear vblank bits */
	}

	if (flags & SDE_PLANE_QOS_PANIC_CTRL)
		psde->pipe_qos_cfg.danger_safe_en = enable;

	if (!psde->is_rt_pipe) {
		psde->pipe_qos_cfg.vblank_en = false;
		psde->pipe_qos_cfg.danger_safe_en = false;
	}

	SDE_DEBUG("plane%u: pnum:%d ds:%d vb:%d pri[0x%x, 0x%x] is_rt:%d\n",
		plane->base.id,
		psde->pipe - SSPP_VIG0,
		psde->pipe_qos_cfg.danger_safe_en,
		psde->pipe_qos_cfg.vblank_en,
		psde->pipe_qos_cfg.creq_vblank,
		psde->pipe_qos_cfg.danger_vblank,
		psde->is_rt_pipe);

	psde->pipe_hw->ops.setup_qos_ctrl(psde->pipe_hw,
			&psde->pipe_qos_cfg);
}

int sde_plane_danger_signal_ctrl(struct drm_plane *plane, bool enable)
{
	struct sde_plane *psde;
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;

	if (!plane || !plane->dev) {
		SDE_ERROR("invalid arguments\n");
		return -EINVAL;
	}

	priv = plane->dev->dev_private;
	if (!priv || !priv->kms) {
		SDE_ERROR("invalid KMS reference\n");
		return -EINVAL;
	}

	sde_kms = to_sde_kms(priv->kms);
	psde = to_sde_plane(plane);

	if (!psde->is_rt_pipe)
		goto end;

	sde_power_resource_enable(&priv->phandle, sde_kms->core_client, true);

	_sde_plane_set_qos_ctrl(plane, enable, SDE_PLANE_QOS_PANIC_CTRL);

	sde_power_resource_enable(&priv->phandle, sde_kms->core_client, false);

end:
	return 0;
}

/**
 * _sde_plane_set_ot_limit - set OT limit for the given plane
 * @plane:		Pointer to drm plane
 * @crtc:		Pointer to drm crtc
 */
static void _sde_plane_set_ot_limit(struct drm_plane *plane,
		struct drm_crtc *crtc)
{
	struct sde_plane *psde;
	struct sde_vbif_set_ot_params ot_params;
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;

	if (!plane || !plane->dev || !crtc) {
		SDE_ERROR("invalid arguments plane %d crtc %d\n",
				plane != 0, crtc != 0);
		return;
	}

	priv = plane->dev->dev_private;
	if (!priv || !priv->kms) {
		SDE_ERROR("invalid KMS reference\n");
		return;
	}

	sde_kms = to_sde_kms(priv->kms);
	psde = to_sde_plane(plane);
	if (!psde->pipe_hw) {
		SDE_ERROR("invalid pipe reference\n");
		return;
	}

	memset(&ot_params, 0, sizeof(ot_params));
	ot_params.xin_id = psde->pipe_hw->cap->xin_id;
	ot_params.num = psde->pipe_hw->idx - SSPP_NONE;
	ot_params.width = psde->pipe_cfg.src_rect.w;
	ot_params.height = psde->pipe_cfg.src_rect.h;
	ot_params.is_wfd = !psde->is_rt_pipe;
	ot_params.frame_rate = crtc->mode.vrefresh;
	ot_params.vbif_idx = VBIF_RT;
	ot_params.clk_ctrl = psde->pipe_hw->cap->clk_ctrl;
	ot_params.rd = true;

	sde_vbif_set_ot_limit(sde_kms, &ot_params);
}

/* helper to update a state's input fence pointer from the property */
static void _sde_plane_set_input_fence(struct sde_plane *psde,
		struct sde_plane_state *pstate, uint64_t fd)
{
	if (!psde || !pstate) {
		SDE_ERROR("invalid arg(s), plane %d state %d\n",
				psde != 0, pstate != 0);
		return;
	}

	/* clear previous reference */
	if (pstate->input_fence)
		sde_sync_put(pstate->input_fence);

	/* get fence pointer for later */
	pstate->input_fence = sde_sync_get(fd);

	SDE_DEBUG_PLANE(psde, "0x%llX\n", fd);
}

int sde_plane_wait_input_fence(struct drm_plane *plane, uint32_t wait_ms)
{
	struct sde_plane *psde;
	struct sde_plane_state *pstate;
	uint32_t prefix;
	void *input_fence;
	int ret = -EINVAL;

	if (!plane) {
		SDE_ERROR("invalid plane\n");
	} else if (!plane->state) {
		SDE_ERROR_PLANE(to_sde_plane(plane), "invalid state\n");
	} else {
		psde = to_sde_plane(plane);
		pstate = to_sde_plane_state(plane->state);
		input_fence = pstate->input_fence;

		if (input_fence) {
			prefix = sde_sync_get_name_prefix(input_fence);
			ret = sde_sync_wait(input_fence, wait_ms);

			SDE_EVT32(DRMID(plane), -ret, prefix);

			switch (ret) {
			case 0:
				SDE_DEBUG_PLANE(psde, "signaled\n");
				break;
			case -ETIME:
				SDE_ERROR_PLANE(psde, "%ums timeout on %08X\n",
						wait_ms, prefix);
				psde->is_error = true;
				break;
			default:
				SDE_ERROR_PLANE(psde, "error %d on %08X\n",
						ret, prefix);
				psde->is_error = true;
				break;
			}
		} else {
			ret = 0;
		}
	}
	return ret;
}

static inline void _sde_plane_set_scanout(struct drm_plane *plane,
		struct sde_plane_state *pstate,
		struct sde_hw_pipe_cfg *pipe_cfg,
		struct drm_framebuffer *fb)
{
	struct sde_plane *psde;
	int ret;

	if (!plane || !pstate || !pipe_cfg || !fb) {
		SDE_ERROR(
			"invalid arg(s), plane %d state %d cfg %d fb %d\n",
			plane != 0, pstate != 0, pipe_cfg != 0, fb != 0);
		return;
	}

	psde = to_sde_plane(plane);
	if (!psde->pipe_hw) {
		SDE_ERROR_PLANE(psde, "invalid pipe_hw\n");
		return;
	}

	ret = sde_format_populate_layout(psde->mmu_id, fb, &pipe_cfg->layout);
	if (ret == -EAGAIN)
		SDE_DEBUG_PLANE(psde, "not updating same src addrs\n");
	else if (ret)
		SDE_ERROR_PLANE(psde, "failed to get format layout, %d\n", ret);
	else if (psde->pipe_hw->ops.setup_sourceaddress)
		psde->pipe_hw->ops.setup_sourceaddress(psde->pipe_hw, pipe_cfg);
}

static int _sde_plane_setup_scaler3_lut(struct sde_plane *psde,
		struct sde_plane_state *pstate)
{
	struct sde_hw_scaler3_cfg *cfg = psde->scaler3_cfg;
	int ret = 0;

	cfg->dir_lut = msm_property_get_blob(
			&psde->property_info,
			pstate->property_blobs, &cfg->dir_len,
			PLANE_PROP_SCALER_LUT_ED);
	cfg->cir_lut = msm_property_get_blob(
			&psde->property_info,
			pstate->property_blobs, &cfg->cir_len,
			PLANE_PROP_SCALER_LUT_CIR);
	cfg->sep_lut = msm_property_get_blob(
			&psde->property_info,
			pstate->property_blobs, &cfg->sep_len,
			PLANE_PROP_SCALER_LUT_SEP);
	if (!cfg->dir_lut || !cfg->cir_lut || !cfg->sep_lut)
		ret = -ENODATA;
	return ret;
}

static void _sde_plane_setup_scaler3(struct sde_plane *psde,
		uint32_t src_w, uint32_t src_h, uint32_t dst_w, uint32_t dst_h,
		struct sde_hw_scaler3_cfg *scale_cfg,
		const struct sde_format *fmt,
		uint32_t chroma_subsmpl_h, uint32_t chroma_subsmpl_v)
{
}

/**
 * _sde_plane_setup_scaler2 - determine default scaler phase steps/filter type
 * @psde: Pointer to SDE plane object
 * @src: Source size
 * @dst: Destination size
 * @phase_steps: Pointer to output array for phase steps
 * @filter: Pointer to output array for filter type
 * @fmt: Pointer to format definition
 * @chroma_subsampling: Subsampling amount for chroma channel
 *
 * Returns: 0 on success
 */
static int _sde_plane_setup_scaler2(struct sde_plane *psde,
		uint32_t src, uint32_t dst, uint32_t *phase_steps,
		enum sde_hw_filter *filter, const struct sde_format *fmt,
		uint32_t chroma_subsampling)
{
	if (!psde || !phase_steps || !filter || !fmt) {
		SDE_ERROR(
			"invalid arg(s), plane %d phase %d filter %d fmt %d\n",
			psde != 0, phase_steps != 0, filter != 0, fmt != 0);
		return -EINVAL;
	}

	/* calculate phase steps, leave init phase as zero */
	phase_steps[SDE_SSPP_COMP_0] =
		mult_frac(1 << PHASE_STEP_SHIFT, src, dst);
	phase_steps[SDE_SSPP_COMP_1_2] =
		phase_steps[SDE_SSPP_COMP_0] / chroma_subsampling;
	phase_steps[SDE_SSPP_COMP_2] = phase_steps[SDE_SSPP_COMP_1_2];
	phase_steps[SDE_SSPP_COMP_3] = phase_steps[SDE_SSPP_COMP_0];

	/* calculate scaler config, if necessary */
	if (SDE_FORMAT_IS_YUV(fmt) || src != dst) {
		filter[SDE_SSPP_COMP_3] =
			(src <= dst) ? SDE_SCALE_FILTER_BIL :
			SDE_SCALE_FILTER_PCMN;

		if (SDE_FORMAT_IS_YUV(fmt)) {
			filter[SDE_SSPP_COMP_0] = SDE_SCALE_FILTER_CA;
			filter[SDE_SSPP_COMP_1_2] = filter[SDE_SSPP_COMP_3];
		} else {
			filter[SDE_SSPP_COMP_0] = filter[SDE_SSPP_COMP_3];
			filter[SDE_SSPP_COMP_1_2] =
				SDE_SCALE_FILTER_NEAREST;
		}
	} else {
		/* disable scaler */
		filter[SDE_SSPP_COMP_0] = SDE_SCALE_FILTER_MAX;
		filter[SDE_SSPP_COMP_1_2] = SDE_SCALE_FILTER_MAX;
		filter[SDE_SSPP_COMP_3] = SDE_SCALE_FILTER_MAX;
	}
	return 0;
}

/**
 * _sde_plane_setup_pixel_ext - determine default pixel extension values
 * @psde: Pointer to SDE plane object
 * @src: Source size
 * @dst: Destination size
 * @decimated_src: Source size after decimation, if any
 * @phase_steps: Pointer to output array for phase steps
 * @out_src: Output array for pixel extension values
 * @out_edge1: Output array for pixel extension first edge
 * @out_edge2: Output array for pixel extension second edge
 * @filter: Pointer to array for filter type
 * @fmt: Pointer to format definition
 * @chroma_subsampling: Subsampling amount for chroma channel
 * @post_compare: Whether to chroma subsampled source size for comparisions
 */
static void _sde_plane_setup_pixel_ext(struct sde_plane *psde,
		uint32_t src, uint32_t dst, uint32_t decimated_src,
		uint32_t *phase_steps, uint32_t *out_src, int *out_edge1,
		int *out_edge2, enum sde_hw_filter *filter,
		const struct sde_format *fmt, uint32_t chroma_subsampling,
		bool post_compare)
{
	int64_t edge1, edge2, caf;
	uint32_t src_work;
	int i, tmp;

	if (psde && phase_steps && out_src && out_edge1 &&
			out_edge2 && filter && fmt) {
		/* handle CAF for YUV formats */
		if (SDE_FORMAT_IS_YUV(fmt) && *filter == SDE_SCALE_FILTER_CA)
			caf = PHASE_STEP_UNIT_SCALE;
		else
			caf = 0;

		for (i = 0; i < SDE_MAX_PLANES; i++) {
			src_work = decimated_src;
			if (i == SDE_SSPP_COMP_1_2 || i == SDE_SSPP_COMP_2)
				src_work /= chroma_subsampling;
			if (post_compare)
				src = src_work;
			if (!SDE_FORMAT_IS_YUV(fmt) && (src == dst)) {
				/* unity */
				edge1 = 0;
				edge2 = 0;
			} else if (dst >= src) {
				/* upscale */
				edge1 = (1 << PHASE_RESIDUAL);
				edge1 -= caf;
				edge2 = (1 << PHASE_RESIDUAL);
				edge2 += (dst - 1) * *(phase_steps + i);
				edge2 -= (src_work - 1) * PHASE_STEP_UNIT_SCALE;
				edge2 += caf;
				edge2 = -(edge2);
			} else {
				/* downscale */
				edge1 = 0;
				edge2 = (dst - 1) * *(phase_steps + i);
				edge2 -= (src_work - 1) * PHASE_STEP_UNIT_SCALE;
				edge2 += *(phase_steps + i);
				edge2 = -(edge2);
			}

			/* only enable CAF for luma plane */
			caf = 0;

			/* populate output arrays */
			*(out_src + i) = src_work;

			/* edge updates taken from __pxl_extn_helper */
			if (edge1 >= 0) {
				tmp = (uint32_t)edge1;
				tmp >>= PHASE_STEP_SHIFT;
				*(out_edge1 + i) = -tmp;
			} else {
				tmp = (uint32_t)(-edge1);
				*(out_edge1 + i) =
					(tmp + PHASE_STEP_UNIT_SCALE - 1) >>
					PHASE_STEP_SHIFT;
			}
			if (edge2 >= 0) {
				tmp = (uint32_t)edge2;
				tmp >>= PHASE_STEP_SHIFT;
				*(out_edge2 + i) = -tmp;
			} else {
				tmp = (uint32_t)(-edge2);
				*(out_edge2 + i) =
					(tmp + PHASE_STEP_UNIT_SCALE - 1) >>
					PHASE_STEP_SHIFT;
			}
		}
	}
}

static inline void _sde_plane_setup_csc(struct sde_plane *psde)
{
	static const struct sde_csc_cfg sde_csc_YUV2RGB_601L = {
		{
			/* S15.16 format */
			0x00012A00, 0x00000000, 0x00019880,
			0x00012A00, 0xFFFF9B80, 0xFFFF3000,
			0x00012A00, 0x00020480, 0x00000000,
		},
		/* signed bias */
		{ 0xfff0, 0xff80, 0xff80,},
		{ 0x0, 0x0, 0x0,},
		/* unsigned clamp */
		{ 0x10, 0xeb, 0x10, 0xf0, 0x10, 0xf0,},
		{ 0x00, 0xff, 0x00, 0xff, 0x00, 0xff,},
	};
	static const struct sde_csc_cfg sde_csc10_YUV2RGB_601L = {
		{
			/* S15.16 format */
			0x00012A00, 0x00000000, 0x00019880,
			0x00012A00, 0xFFFF9B80, 0xFFFF3000,
			0x00012A00, 0x00020480, 0x00000000,
			},
		/* signed bias */
		{ 0xffc0, 0xfe00, 0xfe00,},
		{ 0x0, 0x0, 0x0,},
		/* unsigned clamp */
		{ 0x40, 0x3ac, 0x40, 0x3c0, 0x40, 0x3c0,},
		{ 0x00, 0x3ff, 0x00, 0x3ff, 0x00, 0x3ff,},
	};

	if (!psde) {
		SDE_ERROR("invalid plane\n");
		return;
	}

	/* revert to kernel default if override not available */
	if (psde->csc_usr_ptr)
		psde->csc_ptr = psde->csc_usr_ptr;
	else if (BIT(SDE_SSPP_CSC_10BIT) & psde->features)
		psde->csc_ptr = (struct sde_csc_cfg *)&sde_csc10_YUV2RGB_601L;
	else
		psde->csc_ptr = (struct sde_csc_cfg *)&sde_csc_YUV2RGB_601L;

	SDE_DEBUG_PLANE(psde, "using 0x%X 0x%X 0x%X...\n",
			psde->csc_ptr->csc_mv[0],
			psde->csc_ptr->csc_mv[1],
			psde->csc_ptr->csc_mv[2]);
}

static void sde_color_process_plane_setup(struct drm_plane *plane)
{
	struct sde_plane *psde;
	struct sde_plane_state *pstate;
	uint32_t hue, saturation, value, contrast;
	struct drm_msm_memcol *memcol = NULL;
	size_t memcol_sz = 0;

	psde = to_sde_plane(plane);
	pstate = to_sde_plane_state(plane->state);

	hue = (uint32_t) sde_plane_get_property(pstate, PLANE_PROP_HUE_ADJUST);
	if (psde->pipe_hw->ops.setup_pa_hue)
		psde->pipe_hw->ops.setup_pa_hue(psde->pipe_hw, &hue);
	saturation = (uint32_t) sde_plane_get_property(pstate,
		PLANE_PROP_SATURATION_ADJUST);
	if (psde->pipe_hw->ops.setup_pa_sat)
		psde->pipe_hw->ops.setup_pa_sat(psde->pipe_hw, &saturation);
	value = (uint32_t) sde_plane_get_property(pstate,
		PLANE_PROP_VALUE_ADJUST);
	if (psde->pipe_hw->ops.setup_pa_val)
		psde->pipe_hw->ops.setup_pa_val(psde->pipe_hw, &value);
	contrast = (uint32_t) sde_plane_get_property(pstate,
		PLANE_PROP_CONTRAST_ADJUST);
	if (psde->pipe_hw->ops.setup_pa_cont)
		psde->pipe_hw->ops.setup_pa_cont(psde->pipe_hw, &contrast);

	if (psde->pipe_hw->ops.setup_pa_memcolor) {
		/* Skin memory color setup */
		memcol = msm_property_get_blob(&psde->property_info,
					pstate->property_blobs,
					&memcol_sz,
					PLANE_PROP_SKIN_COLOR);
		psde->pipe_hw->ops.setup_pa_memcolor(psde->pipe_hw,
					MEMCOLOR_SKIN, memcol);

		/* Sky memory color setup */
		memcol = msm_property_get_blob(&psde->property_info,
					pstate->property_blobs,
					&memcol_sz,
					PLANE_PROP_SKY_COLOR);
		psde->pipe_hw->ops.setup_pa_memcolor(psde->pipe_hw,
					MEMCOLOR_SKY, memcol);

		/* Foliage memory color setup */
		memcol = msm_property_get_blob(&psde->property_info,
					pstate->property_blobs,
					&memcol_sz,
					PLANE_PROP_FOLIAGE_COLOR);
		psde->pipe_hw->ops.setup_pa_memcolor(psde->pipe_hw,
					MEMCOLOR_FOLIAGE, memcol);
	}
}

static void _sde_plane_setup_scaler(struct sde_plane *psde,
		const struct sde_format *fmt,
		struct sde_plane_state *pstate)
{
	struct sde_hw_pixel_ext *pe;
	uint32_t chroma_subsmpl_h, chroma_subsmpl_v;

	if (!psde || !fmt) {
		SDE_ERROR("invalid arg(s), plane %d fmt %d state %d\n",
				psde != 0, fmt != 0, pstate != 0);
		return;
	}

	pe = &(psde->pixel_ext);

	psde->pipe_cfg.horz_decimation =
		sde_plane_get_property(pstate, PLANE_PROP_H_DECIMATE);
	psde->pipe_cfg.vert_decimation =
		sde_plane_get_property(pstate, PLANE_PROP_V_DECIMATE);

	/* don't chroma subsample if decimating */
	chroma_subsmpl_h = psde->pipe_cfg.horz_decimation ? 1 :
		drm_format_horz_chroma_subsampling(fmt->base.pixel_format);
	chroma_subsmpl_v = psde->pipe_cfg.vert_decimation ? 1 :
		drm_format_vert_chroma_subsampling(fmt->base.pixel_format);

	/* update scaler */
	if (psde->features & BIT(SDE_SSPP_SCALER_QSEED3)) {
		int error;

		error = _sde_plane_setup_scaler3_lut(psde, pstate);
		if (error || !psde->pixel_ext_usr) {
			/* calculate default config for QSEED3 */
			_sde_plane_setup_scaler3(psde,
					psde->pipe_cfg.src_rect.w,
					psde->pipe_cfg.src_rect.h,
					psde->pipe_cfg.dst_rect.w,
					psde->pipe_cfg.dst_rect.h,
					psde->scaler3_cfg, fmt,
					chroma_subsmpl_h, chroma_subsmpl_v);
		}
	} else if (!psde->pixel_ext_usr) {
		uint32_t deci_dim, i;

		/* calculate default configuration for QSEED2 */
		memset(pe, 0, sizeof(struct sde_hw_pixel_ext));

		SDE_DEBUG_PLANE(psde, "default config\n");
		deci_dim = DECIMATED_DIMENSION(psde->pipe_cfg.src_rect.w,
				psde->pipe_cfg.horz_decimation);
		_sde_plane_setup_scaler2(psde,
				deci_dim,
				psde->pipe_cfg.dst_rect.w,
				pe->phase_step_x,
				pe->horz_filter, fmt, chroma_subsmpl_h);

		if (SDE_FORMAT_IS_YUV(fmt))
			deci_dim &= ~0x1;
		_sde_plane_setup_pixel_ext(psde, psde->pipe_cfg.src_rect.w,
				psde->pipe_cfg.dst_rect.w, deci_dim,
				pe->phase_step_x,
				pe->roi_w,
				pe->num_ext_pxls_left,
				pe->num_ext_pxls_right, pe->horz_filter, fmt,
				chroma_subsmpl_h, 0);

		deci_dim = DECIMATED_DIMENSION(psde->pipe_cfg.src_rect.h,
				psde->pipe_cfg.vert_decimation);
		_sde_plane_setup_scaler2(psde,
				deci_dim,
				psde->pipe_cfg.dst_rect.h,
				pe->phase_step_y,
				pe->vert_filter, fmt, chroma_subsmpl_v);
		_sde_plane_setup_pixel_ext(psde, psde->pipe_cfg.src_rect.h,
				psde->pipe_cfg.dst_rect.h, deci_dim,
				pe->phase_step_y,
				pe->roi_h,
				pe->num_ext_pxls_top,
				pe->num_ext_pxls_btm, pe->vert_filter, fmt,
				chroma_subsmpl_v, 1);

		for (i = 0; i < SDE_MAX_PLANES; i++) {
			if (pe->num_ext_pxls_left[i] >= 0)
				pe->left_rpt[i] = pe->num_ext_pxls_left[i];
			else
				pe->left_ftch[i] = pe->num_ext_pxls_left[i];

			if (pe->num_ext_pxls_right[i] >= 0)
				pe->right_rpt[i] = pe->num_ext_pxls_right[i];
			else
				pe->right_ftch[i] = pe->num_ext_pxls_right[i];

			if (pe->num_ext_pxls_top[i] >= 0)
				pe->top_rpt[i] = pe->num_ext_pxls_top[i];
			else
				pe->top_ftch[i] = pe->num_ext_pxls_top[i];

			if (pe->num_ext_pxls_btm[i] >= 0)
				pe->btm_rpt[i] = pe->num_ext_pxls_btm[i];
			else
				pe->btm_ftch[i] = pe->num_ext_pxls_btm[i];
		}
	}
}

/**
 * _sde_plane_color_fill - enables color fill on plane
 * @psde:   Pointer to SDE plane object
 * @color:  RGB fill color value, [23..16] Blue, [15..8] Green, [7..0] Red
 * @alpha:  8-bit fill alpha value, 255 selects 100% alpha
 * Returns: 0 on success
 */
static int _sde_plane_color_fill(struct sde_plane *psde,
		uint32_t color, uint32_t alpha)
{
	const struct sde_format *fmt;

	if (!psde) {
		SDE_ERROR("invalid plane\n");
		return -EINVAL;
	}

	if (!psde->pipe_hw) {
		SDE_ERROR_PLANE(psde, "invalid plane h/w pointer\n");
		return -EINVAL;
	}

	SDE_DEBUG_PLANE(psde, "\n");

	/*
	 * select fill format to match user property expectation,
	 * h/w only supports RGB variants
	 */
	fmt = sde_get_sde_format(DRM_FORMAT_ABGR8888);

	/* update sspp */
	if (fmt && psde->pipe_hw->ops.setup_solidfill) {
		psde->pipe_hw->ops.setup_solidfill(psde->pipe_hw,
				(color & 0xFFFFFF) | ((alpha & 0xFF) << 24));

		/* override scaler/decimation if solid fill */
		psde->pipe_cfg.src_rect.x = 0;
		psde->pipe_cfg.src_rect.y = 0;
		psde->pipe_cfg.src_rect.w = psde->pipe_cfg.dst_rect.w;
		psde->pipe_cfg.src_rect.h = psde->pipe_cfg.dst_rect.h;

		_sde_plane_setup_scaler(psde, fmt, 0);

		if (psde->pipe_hw->ops.setup_format)
			psde->pipe_hw->ops.setup_format(psde->pipe_hw,
					fmt, SDE_SSPP_SOLID_FILL);

		if (psde->pipe_hw->ops.setup_rects)
			psde->pipe_hw->ops.setup_rects(psde->pipe_hw,
					&psde->pipe_cfg, &psde->pixel_ext,
					psde->scaler3_cfg);
	}

	return 0;
}

static int _sde_plane_mode_set(struct drm_plane *plane,
				struct drm_plane_state *state)
{
	uint32_t nplanes, src_flags;
	struct sde_plane *psde;
	struct sde_plane_state *pstate;
	const struct sde_format *fmt;
	struct drm_crtc *crtc;
	struct drm_framebuffer *fb;
	struct sde_rect src, dst;
	bool q16_data = true;
	int idx;

	if (!plane) {
		SDE_ERROR("invalid plane\n");
		return -EINVAL;
	} else if (!plane->state) {
		SDE_ERROR("invalid plane state\n");
		return -EINVAL;
	}

	psde = to_sde_plane(plane);
	pstate = to_sde_plane_state(plane->state);

	crtc = state->crtc;
	fb = state->fb;
	if (!crtc || !fb) {
		SDE_ERROR_PLANE(psde, "invalid crtc %d or fb %d\n",
				crtc != 0, fb != 0);
		return -EINVAL;
	}
	fmt = to_sde_format(msm_framebuffer_format(fb));
	nplanes = fmt->num_planes;

	/* determine what needs to be refreshed */
	while ((idx = msm_property_pop_dirty(&psde->property_info)) >= 0) {
		switch (idx) {
		case PLANE_PROP_SCALER_V1:
		case PLANE_PROP_SCALER_V2:
		case PLANE_PROP_H_DECIMATE:
		case PLANE_PROP_V_DECIMATE:
		case PLANE_PROP_SRC_CONFIG:
		case PLANE_PROP_ZPOS:
			pstate->dirty |= SDE_PLANE_DIRTY_RECTS;
			break;
		case PLANE_PROP_CSC_V1:
			pstate->dirty |= SDE_PLANE_DIRTY_FORMAT;
			break;
		case PLANE_PROP_COLOR_FILL:
			/* potentially need to refresh everything */
			pstate->dirty = SDE_PLANE_DIRTY_ALL;
			break;
		case PLANE_PROP_ROTATION:
			pstate->dirty |= SDE_PLANE_DIRTY_FORMAT;
			break;
		case PLANE_PROP_INFO:
		case PLANE_PROP_ALPHA:
		case PLANE_PROP_INPUT_FENCE:
		case PLANE_PROP_BLEND_OP:
			/* no special action required */
			break;
		default:
			/* unknown property, refresh everything */
			pstate->dirty |= SDE_PLANE_DIRTY_ALL;
			SDE_ERROR("executing full mode set, prp_idx %d\n", idx);
			break;
		}
	}

	if (pstate->dirty & SDE_PLANE_DIRTY_RECTS)
		memset(&(psde->pipe_cfg), 0, sizeof(struct sde_hw_pipe_cfg));

	_sde_plane_set_scanout(plane, pstate, &psde->pipe_cfg, fb);

	/* early out if nothing dirty */
	if (!pstate->dirty)
		return 0;
	pstate->pending = true;

	psde->is_rt_pipe = sde_crtc_is_rt(crtc);
	_sde_plane_set_qos_ctrl(plane, false, SDE_PLANE_QOS_PANIC_CTRL);

	/* update roi config */
	if (pstate->dirty & SDE_PLANE_DIRTY_RECTS) {
		POPULATE_RECT(&src, state->src_x, state->src_y,
			state->src_w, state->src_h, q16_data);
		POPULATE_RECT(&dst, state->crtc_x, state->crtc_y,
			state->crtc_w, state->crtc_h, !q16_data);

		SDE_DEBUG_PLANE(psde,
			"FB[%u] %u,%u,%ux%u->crtc%u %d,%d,%ux%u, %s ubwc %d\n",
				fb->base.id, src.x, src.y, src.w, src.h,
				crtc->base.id, dst.x, dst.y, dst.w, dst.h,
				drm_get_format_name(fmt->base.pixel_format),
				SDE_FORMAT_IS_UBWC(fmt));

		if (sde_plane_get_property(pstate, PLANE_PROP_SRC_CONFIG) &
			BIT(SDE_DRM_DEINTERLACE)) {
			SDE_DEBUG_PLANE(psde, "deinterlace\n");
			for (idx = 0; idx < SDE_MAX_PLANES; ++idx)
				psde->pipe_cfg.layout.plane_pitch[idx] <<= 1;
			src.h /= 2;
			src.y  = DIV_ROUND_UP(src.y, 2);
			src.y &= ~0x1;
		}

		psde->pipe_cfg.src_rect = src;
		psde->pipe_cfg.dst_rect = dst;

		/* check for color fill */
		psde->color_fill = (uint32_t)sde_plane_get_property(pstate,
				PLANE_PROP_COLOR_FILL);
		if (psde->color_fill & SDE_PLANE_COLOR_FILL_FLAG) {
			/* skip remaining processing on color fill */
			pstate->dirty = 0x0;
		} else if (psde->pipe_hw->ops.setup_rects) {
			_sde_plane_setup_scaler(psde, fmt, pstate);

			psde->pipe_hw->ops.setup_rects(psde->pipe_hw,
					&psde->pipe_cfg, &psde->pixel_ext,
					psde->scaler3_cfg);
		}
	}

	if ((pstate->dirty & SDE_PLANE_DIRTY_FORMAT) &&
			psde->pipe_hw->ops.setup_format) {
		src_flags = 0x0;
		SDE_DEBUG_PLANE(psde, "rotation 0x%llX\n",
			sde_plane_get_property(pstate, PLANE_PROP_ROTATION));
		if (sde_plane_get_property(pstate, PLANE_PROP_ROTATION) &
			BIT(DRM_REFLECT_X))
			src_flags |= SDE_SSPP_FLIP_LR;
		if (sde_plane_get_property(pstate, PLANE_PROP_ROTATION) &
			BIT(DRM_REFLECT_Y))
			src_flags |= SDE_SSPP_FLIP_UD;

		/* update format */
		psde->pipe_hw->ops.setup_format(psde->pipe_hw, fmt, src_flags);

		/* update csc */
		if (SDE_FORMAT_IS_YUV(fmt))
			_sde_plane_setup_csc(psde);
		else
			psde->csc_ptr = 0;
	}

	sde_color_process_plane_setup(plane);

	/* update sharpening */
	if ((pstate->dirty & SDE_PLANE_DIRTY_SHARPEN) &&
		psde->pipe_hw->ops.setup_sharpening) {
		psde->sharp_cfg.strength = SHARP_STRENGTH_DEFAULT;
		psde->sharp_cfg.edge_thr = SHARP_EDGE_THR_DEFAULT;
		psde->sharp_cfg.smooth_thr = SHARP_SMOOTH_THR_DEFAULT;
		psde->sharp_cfg.noise_thr = SHARP_NOISE_THR_DEFAULT;

		psde->pipe_hw->ops.setup_sharpening(psde->pipe_hw,
				&psde->sharp_cfg);
	}

	_sde_plane_set_qos_lut(plane, fb);
	_sde_plane_set_danger_lut(plane, fb);

	if (plane->type != DRM_PLANE_TYPE_CURSOR) {
		_sde_plane_set_qos_ctrl(plane, true, SDE_PLANE_QOS_PANIC_CTRL);
		_sde_plane_set_ot_limit(plane, crtc);
	}

	/* clear dirty */
	pstate->dirty = 0x0;

	return 0;
}

static int sde_plane_prepare_fb(struct drm_plane *plane,
		const struct drm_plane_state *new_state)
{
	struct drm_framebuffer *fb = new_state->fb;
	struct sde_plane *psde = to_sde_plane(plane);

	if (!new_state->fb)
		return 0;

	SDE_DEBUG_PLANE(psde, "FB[%u]\n", fb->base.id);
	return msm_framebuffer_prepare(fb, psde->mmu_id);
}

static void sde_plane_cleanup_fb(struct drm_plane *plane,
		const struct drm_plane_state *old_state)
{
	struct drm_framebuffer *fb = old_state ? old_state->fb : NULL;
	struct sde_plane *psde = plane ? to_sde_plane(plane) : NULL;

	if (!fb)
		return;

	SDE_DEBUG_PLANE(psde, "FB[%u]\n", fb->base.id);
	msm_framebuffer_cleanup(fb, psde->mmu_id);
}

static void _sde_plane_atomic_check_mode_changed(struct sde_plane *psde,
		struct drm_plane_state *state,
		struct drm_plane_state *old_state)
{
	struct sde_plane_state *pstate = to_sde_plane_state(state);

	/* no need to check it again */
	if (pstate->dirty == SDE_PLANE_DIRTY_ALL)
		return;

	if (!sde_plane_enabled(state) || !sde_plane_enabled(old_state)
			|| psde->is_error) {
		SDE_DEBUG_PLANE(psde,
			"enabling/disabling full modeset required\n");
		pstate->dirty |= SDE_PLANE_DIRTY_ALL;
	} else if (to_sde_plane_state(old_state)->pending) {
		SDE_DEBUG_PLANE(psde, "still pending\n");
		pstate->dirty |= SDE_PLANE_DIRTY_ALL;
	} else if (state->src_w != old_state->src_w ||
		   state->src_h != old_state->src_h ||
		   state->src_x != old_state->src_x ||
		   state->src_y != old_state->src_y) {
		SDE_DEBUG_PLANE(psde, "src rect updated\n");
		pstate->dirty |= SDE_PLANE_DIRTY_RECTS;
	} else if (state->crtc_w != old_state->crtc_w ||
		   state->crtc_h != old_state->crtc_h ||
		   state->crtc_x != old_state->crtc_x ||
		   state->crtc_y != old_state->crtc_y) {
		SDE_DEBUG_PLANE(psde, "crtc rect updated\n");
		pstate->dirty |= SDE_PLANE_DIRTY_RECTS;
	}

	if (!state->fb || !old_state->fb) {
		SDE_DEBUG_PLANE(psde, "can't compare fb handles\n");
	} else if (state->fb->pixel_format != old_state->fb->pixel_format) {
		SDE_DEBUG_PLANE(psde, "format change\n");
		pstate->dirty |= SDE_PLANE_DIRTY_FORMAT | SDE_PLANE_DIRTY_RECTS;
	} else {
		uint64_t *new_mods = state->fb->modifier;
		uint64_t *old_mods = old_state->fb->modifier;
		uint32_t *new_pitches = state->fb->pitches;
		uint32_t *old_pitches = old_state->fb->pitches;
		uint32_t *new_offset = state->fb->offsets;
		uint32_t *old_offset = old_state->fb->offsets;
		int i;

		for (i = 0; i < ARRAY_SIZE(state->fb->modifier); i++) {
			if (new_mods[i] != old_mods[i]) {
				SDE_DEBUG_PLANE(psde,
					"format modifiers change\"\
					plane:%d new_mode:%llu old_mode:%llu\n",
					i, new_mods[i], old_mods[i]);
				pstate->dirty |= SDE_PLANE_DIRTY_FORMAT |
					SDE_PLANE_DIRTY_RECTS;
				break;
			}
		}
		for (i = 0; i < ARRAY_SIZE(state->fb->pitches); i++) {
			if (new_pitches[i] != old_pitches[i]) {
				SDE_DEBUG_PLANE(psde,
					"pitches change plane:%d\"\
					old_pitches:%u new_pitches:%u\n",
					i, old_pitches[i], new_pitches[i]);
				pstate->dirty |= SDE_PLANE_DIRTY_RECTS;
				break;
			}
		}
		for (i = 0; i < ARRAY_SIZE(state->fb->offsets); i++) {
			if (new_offset[i] != old_offset[i]) {
				SDE_DEBUG_PLANE(psde,
					"offset change plane:%d\"\
					old_offset:%u new_offset:%u\n",
					i, old_offset[i], new_offset[i]);
				pstate->dirty |= SDE_PLANE_DIRTY_FORMAT |
					SDE_PLANE_DIRTY_RECTS;
				break;
			}
		}
	}
}

static int sde_plane_atomic_check(struct drm_plane *plane,
		struct drm_plane_state *state)
{
	int ret = 0;
	struct sde_plane *psde;
	struct sde_plane_state *pstate;
	const struct sde_format *fmt;
	struct sde_rect src, dst;
	uint32_t deci_w, deci_h, src_deci_w, src_deci_h;
	uint32_t max_upscale, max_downscale, min_src_size, max_linewidth;
	bool q16_data = true;

	if (!plane || !state) {
		SDE_ERROR("invalid arg(s), plane %d state %d\n",
				plane != 0, state != 0);
		ret = -EINVAL;
		goto exit;
	}

	psde = to_sde_plane(plane);
	pstate = to_sde_plane_state(state);

	if (!psde->pipe_sblk) {
		SDE_ERROR_PLANE(psde, "invalid catalog\n");
		ret = -EINVAL;
		goto exit;
	}

	deci_w = sde_plane_get_property(pstate, PLANE_PROP_H_DECIMATE);
	deci_h = sde_plane_get_property(pstate, PLANE_PROP_V_DECIMATE);

	/* src values are in Q16 fixed point, convert to integer */
	POPULATE_RECT(&src, state->src_x, state->src_y, state->src_w,
		state->src_h, q16_data);
	POPULATE_RECT(&dst, state->crtc_x, state->crtc_y, state->crtc_w,
		state->crtc_h, !q16_data);

	src_deci_w = DECIMATED_DIMENSION(src.w, deci_w);
	src_deci_h = DECIMATED_DIMENSION(src.h, deci_h);

	max_upscale = psde->pipe_sblk->maxupscale;
	max_downscale = psde->pipe_sblk->maxdwnscale;
	max_linewidth = psde->pipe_sblk->maxlinewidth;

	SDE_DEBUG_PLANE(psde, "check %d -> %d\n",
		sde_plane_enabled(plane->state), sde_plane_enabled(state));

	if (!sde_plane_enabled(state))
		goto modeset_update;

	fmt = to_sde_format(msm_framebuffer_format(state->fb));

	min_src_size = SDE_FORMAT_IS_YUV(fmt) ? 2 : 1;

	if (SDE_FORMAT_IS_YUV(fmt) &&
		(!(psde->features & SDE_SSPP_SCALER) ||
		 !(psde->features & (BIT(SDE_SSPP_CSC)
		 | BIT(SDE_SSPP_CSC_10BIT))))) {
		SDE_ERROR_PLANE(psde,
				"plane doesn't have scaler/csc for yuv\n");
		ret = -EINVAL;

	/* check src bounds */
	} else if (state->fb->width > MAX_IMG_WIDTH ||
		state->fb->height > MAX_IMG_HEIGHT ||
		src.w < min_src_size || src.h < min_src_size ||
		CHECK_LAYER_BOUNDS(src.x, src.w, state->fb->width) ||
		CHECK_LAYER_BOUNDS(src.y, src.h, state->fb->height)) {
		SDE_ERROR_PLANE(psde, "invalid source %u, %u, %ux%u\n",
			src.x, src.y, src.w, src.h);
		ret = -E2BIG;

	/* valid yuv image */
	} else if (SDE_FORMAT_IS_YUV(fmt) && ((src.x & 0x1) || (src.y & 0x1) ||
			 (src.w & 0x1) || (src.h & 0x1))) {
		SDE_ERROR_PLANE(psde, "invalid yuv source %u, %u, %ux%u\n",
				src.x, src.y, src.w, src.h);
		ret = -EINVAL;

	/* min dst support */
	} else if (dst.w < 0x1 || dst.h < 0x1) {
		SDE_ERROR_PLANE(psde, "invalid dest rect %u, %u, %ux%u\n",
				dst.x, dst.y, dst.w, dst.h);
		ret = -EINVAL;

	/* decimation validation */
	} else if (deci_w || deci_h) {
		if ((deci_w > psde->pipe_sblk->maxhdeciexp) ||
			(deci_h > psde->pipe_sblk->maxvdeciexp)) {
			SDE_ERROR_PLANE(psde,
					"too much decimation requested\n");
			ret = -EINVAL;
		} else if (fmt->fetch_mode != SDE_FETCH_LINEAR) {
			SDE_ERROR_PLANE(psde,
					"decimation requires linear fetch\n");
			ret = -EINVAL;
		}

	} else if (!(psde->features & SDE_SSPP_SCALER) &&
		((src.w != dst.w) || (src.h != dst.h))) {
		SDE_ERROR_PLANE(psde,
			"pipe doesn't support scaling %ux%u->%ux%u\n",
			src.w, src.h, dst.w, dst.h);
		ret = -EINVAL;

	/* check decimated source width */
	} else if (src_deci_w > max_linewidth) {
		SDE_ERROR_PLANE(psde,
				"invalid src w:%u, deci w:%u, line w:%u\n",
				src.w, src_deci_w, max_linewidth);
		ret = -E2BIG;

	/* check max scaler capability */
	} else if (((src_deci_w * max_upscale) < dst.w) ||
		((src_deci_h * max_upscale) < dst.h) ||
		((dst.w * max_downscale) < src_deci_w) ||
		((dst.h * max_downscale) < src_deci_h)) {
		SDE_ERROR_PLANE(psde,
			"too much scaling requested %ux%u->%ux%u\n",
			src_deci_w, src_deci_h, dst.w, dst.h);
		ret = -E2BIG;
	}

modeset_update:
	if (!ret)
		_sde_plane_atomic_check_mode_changed(psde, state, plane->state);
exit:
	return ret;
}

/**
 * sde_plane_flush - final plane operations before commit flush
 * @plane: Pointer to drm plane structure
 */
void sde_plane_flush(struct drm_plane *plane)
{
	struct sde_plane *psde;

	if (!plane) {
		SDE_ERROR("invalid plane\n");
		return;
	}

	psde = to_sde_plane(plane);

	/*
	 * These updates have to be done immediately before the plane flush
	 * timing, and may not be moved to the atomic_update/mode_set functions.
	 */
	if (psde->is_error)
		/* force white frame with 0% alpha pipe output on error */
		_sde_plane_color_fill(psde, 0xFFFFFF, 0x0);
	else if (psde->color_fill & SDE_PLANE_COLOR_FILL_FLAG)
		/* force 100% alpha */
		_sde_plane_color_fill(psde, psde->color_fill, 0xFF);
	else if (psde->pipe_hw && psde->csc_ptr && psde->pipe_hw->ops.setup_csc)
		psde->pipe_hw->ops.setup_csc(psde->pipe_hw, psde->csc_ptr);

	/* flag h/w flush complete */
	if (plane->state)
		to_sde_plane_state(plane->state)->pending = false;
}

static void sde_plane_atomic_update(struct drm_plane *plane,
				struct drm_plane_state *old_state)
{
	struct sde_plane *psde;
	struct drm_plane_state *state;
	struct sde_plane_state *pstate;

	if (!plane) {
		SDE_ERROR("invalid plane\n");
		return;
	} else if (!plane->state) {
		SDE_ERROR("invalid plane state\n");
		return;
	}

	psde = to_sde_plane(plane);
	psde->is_error = false;
	state = plane->state;
	pstate = to_sde_plane_state(state);

	SDE_DEBUG_PLANE(psde, "\n");

	if (!sde_plane_enabled(state)) {
		pstate->pending = true;
	} else {
		int ret;

		ret = _sde_plane_mode_set(plane, state);
		/* atomic_check should have ensured that this doesn't fail */
		WARN_ON(ret < 0);
	}
}


/* helper to install properties which are common to planes and crtcs */
static void _sde_plane_install_properties(struct drm_plane *plane,
	struct sde_mdss_cfg *catalog)
{
	static const struct drm_prop_enum_list e_blend_op[] = {
		{SDE_DRM_BLEND_OP_NOT_DEFINED,    "not_defined"},
		{SDE_DRM_BLEND_OP_OPAQUE,         "opaque"},
		{SDE_DRM_BLEND_OP_PREMULTIPLIED,  "premultiplied"},
		{SDE_DRM_BLEND_OP_COVERAGE,       "coverage"}
	};
	static const struct drm_prop_enum_list e_src_config[] = {
		{SDE_DRM_DEINTERLACE, "deinterlace"}
	};
	const struct sde_format_extended *format_list;
	struct sde_kms_info *info;
	struct sde_plane *psde = to_sde_plane(plane);
	int zpos_max = 255;
	int zpos_def = 0;
	char feature_name[256];

	if (!plane || !psde) {
		SDE_ERROR("invalid plane\n");
		return;
	} else if (!psde->pipe_hw || !psde->pipe_sblk) {
		SDE_ERROR("invalid plane, pipe_hw %d pipe_sblk %d\n",
				psde->pipe_hw != 0, psde->pipe_sblk != 0);
		return;
	} else if (!catalog) {
		SDE_ERROR("invalid catalog\n");
		return;
	}

	if (sde_is_custom_client()) {
		if (catalog->mixer_count && catalog->mixer &&
				catalog->mixer[0].sblk->maxblendstages) {
			zpos_max = catalog->mixer[0].sblk->maxblendstages - 1;
			if (zpos_max > SDE_STAGE_MAX - SDE_STAGE_0 - 1)
				zpos_max = SDE_STAGE_MAX - SDE_STAGE_0 - 1;
		}
	} else if (plane->type != DRM_PLANE_TYPE_PRIMARY) {
		/* reserve zpos == 0 for primary planes */
		zpos_def = drm_plane_index(plane) + 1;
	}

	msm_property_install_range(&psde->property_info, "zpos",
		0x0, 0, zpos_max, zpos_def, PLANE_PROP_ZPOS);

	msm_property_install_range(&psde->property_info, "alpha",
		0x0, 0, 255, 255, PLANE_PROP_ALPHA);

	/* linux default file descriptor range on each process */
	msm_property_install_range(&psde->property_info, "input_fence",
		0x0, 0, INR_OPEN_MAX, 0, PLANE_PROP_INPUT_FENCE);

	if (psde->pipe_sblk->maxhdeciexp) {
		msm_property_install_range(&psde->property_info, "h_decimate",
			0x0, 0, psde->pipe_sblk->maxhdeciexp, 0,
			PLANE_PROP_H_DECIMATE);
	}

	if (psde->pipe_sblk->maxvdeciexp) {
		msm_property_install_range(&psde->property_info, "v_decimate",
				0x0, 0, psde->pipe_sblk->maxvdeciexp, 0,
				PLANE_PROP_V_DECIMATE);
	}

	if (psde->features & BIT(SDE_SSPP_SCALER_QSEED3)) {
		msm_property_install_volatile_range(&psde->property_info,
			"scaler_v2", 0x0, 0, ~0, 0, PLANE_PROP_SCALER_V2);
		msm_property_install_blob(&psde->property_info, "lut_ed", 0,
			PLANE_PROP_SCALER_LUT_ED);
		msm_property_install_blob(&psde->property_info, "lut_cir", 0,
			PLANE_PROP_SCALER_LUT_CIR);
		msm_property_install_blob(&psde->property_info, "lut_sep", 0,
			PLANE_PROP_SCALER_LUT_SEP);
	} else if (psde->features & SDE_SSPP_SCALER) {
		msm_property_install_volatile_range(&psde->property_info,
			"scaler_v1", 0x0, 0, ~0, 0, PLANE_PROP_SCALER_V1);
	}

	if (psde->features & BIT(SDE_SSPP_CSC)) {
		msm_property_install_volatile_range(&psde->property_info,
			"csc_v1", 0x0, 0, ~0, 0, PLANE_PROP_CSC_V1);
	}

	if (psde->features & BIT(SDE_SSPP_HSIC)) {
		snprintf(feature_name, sizeof(feature_name), "%s%d",
			"SDE_SSPP_HUE_V",
			psde->pipe_sblk->hsic_blk.version >> 16);
		msm_property_install_range(&psde->property_info,
			feature_name, 0, 0, 0xFFFFFFFF, 0,
			PLANE_PROP_HUE_ADJUST);
		snprintf(feature_name, sizeof(feature_name), "%s%d",
			"SDE_SSPP_SATURATION_V",
			psde->pipe_sblk->hsic_blk.version >> 16);
		msm_property_install_range(&psde->property_info,
			feature_name, 0, 0, 0xFFFFFFFF, 0,
			PLANE_PROP_SATURATION_ADJUST);
		snprintf(feature_name, sizeof(feature_name), "%s%d",
			"SDE_SSPP_VALUE_V",
			psde->pipe_sblk->hsic_blk.version >> 16);
		msm_property_install_range(&psde->property_info,
			feature_name, 0, 0, 0xFFFFFFFF, 0,
			PLANE_PROP_VALUE_ADJUST);
		snprintf(feature_name, sizeof(feature_name), "%s%d",
			"SDE_SSPP_CONTRAST_V",
			psde->pipe_sblk->hsic_blk.version >> 16);
		msm_property_install_range(&psde->property_info,
			feature_name, 0, 0, 0xFFFFFFFF, 0,
			PLANE_PROP_CONTRAST_ADJUST);
	}

	/* standard properties */
	msm_property_install_rotation(&psde->property_info,
		BIT(DRM_REFLECT_X) | BIT(DRM_REFLECT_Y), PLANE_PROP_ROTATION);

	msm_property_install_enum(&psde->property_info, "blend_op", 0x0, 0,
		e_blend_op, ARRAY_SIZE(e_blend_op), PLANE_PROP_BLEND_OP);

	msm_property_install_enum(&psde->property_info, "src_config", 0x0, 1,
		e_src_config, ARRAY_SIZE(e_src_config), PLANE_PROP_SRC_CONFIG);

	if (psde->pipe_hw->ops.setup_solidfill)
		msm_property_install_range(&psde->property_info, "color_fill",
				0, 0, 0xFFFFFFFF, 0, PLANE_PROP_COLOR_FILL);

	info = kzalloc(sizeof(struct sde_kms_info), GFP_KERNEL);
	if (!info) {
		SDE_ERROR("failed to allocate info memory\n");
		return;
	}

	msm_property_install_blob(&psde->property_info, "capabilities",
		DRM_MODE_PROP_IMMUTABLE, PLANE_PROP_INFO);
	sde_kms_info_reset(info);

	format_list = psde->pipe_sblk->format_list;
	if (format_list) {
		sde_kms_info_start(info, "pixel_formats");
		while (format_list->fourcc_format) {
			sde_kms_info_append_format(info,
					format_list->fourcc_format,
					format_list->modifier);
			++format_list;
		}
		sde_kms_info_stop(info);
	}

	sde_kms_info_add_keyint(info, "max_linewidth",
			psde->pipe_sblk->maxlinewidth);
	sde_kms_info_add_keyint(info, "max_upscale",
			psde->pipe_sblk->maxupscale);
	sde_kms_info_add_keyint(info, "max_downscale",
			psde->pipe_sblk->maxdwnscale);
	sde_kms_info_add_keyint(info, "max_horizontal_deci",
			psde->pipe_sblk->maxhdeciexp);
	sde_kms_info_add_keyint(info, "max_vertical_deci",
			psde->pipe_sblk->maxvdeciexp);
	msm_property_set_blob(&psde->property_info, &psde->blob_info,
			info->data, info->len, PLANE_PROP_INFO);

	kfree(info);

	if (psde->features & BIT(SDE_SSPP_MEMCOLOR)) {
		snprintf(feature_name, sizeof(feature_name), "%s%d",
			"SDE_SSPP_SKIN_COLOR_V",
			psde->pipe_sblk->memcolor_blk.version >> 16);
		msm_property_install_blob(&psde->property_info, feature_name, 0,
			PLANE_PROP_SKIN_COLOR);
		snprintf(feature_name, sizeof(feature_name), "%s%d",
			"SDE_SSPP_SKY_COLOR_V",
			psde->pipe_sblk->memcolor_blk.version >> 16);
		msm_property_install_blob(&psde->property_info, feature_name, 0,
			PLANE_PROP_SKY_COLOR);
		snprintf(feature_name, sizeof(feature_name), "%s%d",
			"SDE_SSPP_FOLIAGE_COLOR_V",
			psde->pipe_sblk->memcolor_blk.version >> 16);
		msm_property_install_blob(&psde->property_info, feature_name, 0,
			PLANE_PROP_FOLIAGE_COLOR);
	}
}

static inline void _sde_plane_set_csc_v1(struct sde_plane *psde, void *usr_ptr)
{
	struct sde_drm_csc_v1 csc_v1;
	int i;

	if (!psde) {
		SDE_ERROR("invalid plane\n");
		return;
	}

	psde->csc_usr_ptr = NULL;
	if (!usr_ptr) {
		SDE_DEBUG_PLANE(psde, "csc data removed\n");
		return;
	}

	if (copy_from_user(&csc_v1, usr_ptr, sizeof(csc_v1))) {
		SDE_ERROR_PLANE(psde, "failed to copy csc data\n");
		return;
	}

	/* populate from user space */
	for (i = 0; i < SDE_CSC_MATRIX_COEFF_SIZE; ++i)
		psde->csc_cfg.csc_mv[i] = csc_v1.ctm_coeff[i] >> 16;
	for (i = 0; i < SDE_CSC_BIAS_SIZE; ++i) {
		psde->csc_cfg.csc_pre_bv[i] = csc_v1.pre_bias[i];
		psde->csc_cfg.csc_post_bv[i] = csc_v1.post_bias[i];
	}
	for (i = 0; i < SDE_CSC_CLAMP_SIZE; ++i) {
		psde->csc_cfg.csc_pre_lv[i] = csc_v1.pre_clamp[i];
		psde->csc_cfg.csc_post_lv[i] = csc_v1.post_clamp[i];
	}
	psde->csc_usr_ptr = &psde->csc_cfg;
}

static inline void _sde_plane_set_scaler_v1(struct sde_plane *psde, void *usr)
{
	struct sde_drm_scaler_v1 scale_v1;
	struct sde_hw_pixel_ext *pe;
	int i;

	if (!psde) {
		SDE_ERROR("invalid plane\n");
		return;
	}

	psde->pixel_ext_usr = false;
	if (!usr) {
		SDE_DEBUG_PLANE(psde, "scale data removed\n");
		return;
	}

	if (copy_from_user(&scale_v1, usr, sizeof(scale_v1))) {
		SDE_ERROR_PLANE(psde, "failed to copy scale data\n");
		return;
	}

	/* populate from user space */
	pe = &(psde->pixel_ext);
	memset(pe, 0, sizeof(struct sde_hw_pixel_ext));
	for (i = 0; i < SDE_MAX_PLANES; i++) {
		pe->init_phase_x[i] = scale_v1.init_phase_x[i];
		pe->phase_step_x[i] = scale_v1.phase_step_x[i];
		pe->init_phase_y[i] = scale_v1.init_phase_y[i];
		pe->phase_step_y[i] = scale_v1.phase_step_y[i];

		pe->horz_filter[i] = scale_v1.horz_filter[i];
		pe->vert_filter[i] = scale_v1.vert_filter[i];
	}
	for (i = 0; i < SDE_MAX_PLANES; i++) {
		pe->left_ftch[i] = scale_v1.pe.left_ftch[i];
		pe->right_ftch[i] = scale_v1.pe.right_ftch[i];
		pe->left_rpt[i] = scale_v1.pe.left_rpt[i];
		pe->right_rpt[i] = scale_v1.pe.right_rpt[i];
		pe->roi_w[i] = scale_v1.pe.num_ext_pxls_lr[i];

		pe->top_ftch[i] = scale_v1.pe.top_ftch[i];
		pe->btm_ftch[i] = scale_v1.pe.btm_ftch[i];
		pe->top_rpt[i] = scale_v1.pe.top_rpt[i];
		pe->btm_rpt[i] = scale_v1.pe.btm_rpt[i];
		pe->roi_h[i] = scale_v1.pe.num_ext_pxls_tb[i];
	}

	psde->pixel_ext_usr = true;

	SDE_DEBUG_PLANE(psde, "user property data copied\n");
}

static inline void _sde_plane_set_scaler_v2(struct sde_plane *psde,
		struct sde_plane_state *pstate, void *usr)
{
	struct sde_drm_scaler_v2 scale_v2;
	struct sde_hw_pixel_ext *pe;
	int i;
	struct sde_hw_scaler3_cfg *cfg;

	if (!psde) {
		SDE_ERROR("invalid plane\n");
		return;
	}

	cfg = psde->scaler3_cfg;
	psde->pixel_ext_usr = false;
	if (!usr) {
		SDE_DEBUG_PLANE(psde, "scale data removed\n");
		return;
	}

	if (copy_from_user(&scale_v2, usr, sizeof(scale_v2))) {
		SDE_ERROR_PLANE(psde, "failed to copy scale data\n");
		return;
	}

	/* populate from user space */
	pe = &(psde->pixel_ext);
	memset(pe, 0, sizeof(struct sde_hw_pixel_ext));
	cfg->enable = scale_v2.enable;
	cfg->dir_en = scale_v2.dir_en;
	for (i = 0; i < SDE_MAX_PLANES; i++) {
		cfg->init_phase_x[i] = scale_v2.init_phase_x[i];
		cfg->phase_step_x[i] = scale_v2.phase_step_x[i];
		cfg->init_phase_y[i] = scale_v2.init_phase_y[i];
		cfg->phase_step_y[i] = scale_v2.phase_step_y[i];

		cfg->preload_x[i] = scale_v2.preload_x[i];
		cfg->preload_y[i] = scale_v2.preload_y[i];
		cfg->src_width[i] = scale_v2.src_width[i];
		cfg->src_height[i] = scale_v2.src_height[i];
	}
	cfg->dst_width = scale_v2.dst_width;
	cfg->dst_height = scale_v2.dst_height;

	cfg->y_rgb_filter_cfg = scale_v2.y_rgb_filter_cfg;
	cfg->uv_filter_cfg = scale_v2.uv_filter_cfg;
	cfg->alpha_filter_cfg = scale_v2.alpha_filter_cfg;
	cfg->blend_cfg = scale_v2.blend_cfg;

	cfg->lut_flag = scale_v2.lut_flag;
	cfg->dir_lut_idx = scale_v2.dir_lut_idx;
	cfg->y_rgb_cir_lut_idx = scale_v2.y_rgb_cir_lut_idx;
	cfg->uv_cir_lut_idx = scale_v2.uv_cir_lut_idx;
	cfg->y_rgb_sep_lut_idx = scale_v2.y_rgb_sep_lut_idx;
	cfg->uv_sep_lut_idx = scale_v2.uv_sep_lut_idx;

	cfg->de.enable = scale_v2.de.enable;
	cfg->de.sharpen_level1 = scale_v2.de.sharpen_level1;
	cfg->de.sharpen_level2 = scale_v2.de.sharpen_level2;
	cfg->de.clip = scale_v2.de.clip;
	cfg->de.limit = scale_v2.de.limit;
	cfg->de.thr_quiet = scale_v2.de.thr_quiet;
	cfg->de.thr_dieout = scale_v2.de.thr_dieout;
	cfg->de.thr_low = scale_v2.de.thr_low;
	cfg->de.thr_high = scale_v2.de.thr_high;
	cfg->de.prec_shift = scale_v2.de.prec_shift;
	for (i = 0; i < SDE_MAX_DE_CURVES; i++) {
		cfg->de.adjust_a[i] = scale_v2.de.adjust_a[i];
		cfg->de.adjust_b[i] = scale_v2.de.adjust_b[i];
		cfg->de.adjust_c[i] = scale_v2.de.adjust_c[i];
	}
	for (i = 0; i < SDE_MAX_PLANES; i++) {
		pe->left_ftch[i] = scale_v2.pe.left_ftch[i];
		pe->right_ftch[i] = scale_v2.pe.right_ftch[i];
		pe->left_rpt[i] = scale_v2.pe.left_rpt[i];
		pe->right_rpt[i] = scale_v2.pe.right_rpt[i];
		pe->roi_w[i] = scale_v2.pe.num_ext_pxls_lr[i];

		pe->top_ftch[i] = scale_v2.pe.top_ftch[i];
		pe->btm_ftch[i] = scale_v2.pe.btm_ftch[i];
		pe->top_rpt[i] = scale_v2.pe.top_rpt[i];
		pe->btm_rpt[i] = scale_v2.pe.btm_rpt[i];
		pe->roi_h[i] = scale_v2.pe.num_ext_pxls_tb[i];
	}
	psde->pixel_ext_usr = true;

	SDE_DEBUG_PLANE(psde, "user property data copied\n");
}

static int sde_plane_atomic_set_property(struct drm_plane *plane,
		struct drm_plane_state *state, struct drm_property *property,
		uint64_t val)
{
	struct sde_plane *psde = plane ? to_sde_plane(plane) : NULL;
	struct sde_plane_state *pstate;
	int idx, ret = -EINVAL;

	SDE_DEBUG_PLANE(psde, "\n");

	if (!plane) {
		SDE_ERROR("invalid plane\n");
	} else if (!state) {
		SDE_ERROR_PLANE(psde, "invalid state\n");
	} else {
		pstate = to_sde_plane_state(state);
		ret = msm_property_atomic_set(&psde->property_info,
				pstate->property_values, pstate->property_blobs,
				property, val);
		if (!ret) {
			idx = msm_property_index(&psde->property_info,
					property);
			switch (idx) {
			case PLANE_PROP_INPUT_FENCE:
				_sde_plane_set_input_fence(psde, pstate, val);
				break;
			case PLANE_PROP_CSC_V1:
				_sde_plane_set_csc_v1(psde, (void *)val);
				break;
			case PLANE_PROP_SCALER_V1:
				_sde_plane_set_scaler_v1(psde, (void *)val);
				break;
			case PLANE_PROP_SCALER_V2:
				_sde_plane_set_scaler_v2(psde, pstate,
					(void *)val);
				break;
			default:
				/* nothing to do */
				break;
			}
		}
	}

	return ret;
}

static int sde_plane_set_property(struct drm_plane *plane,
		struct drm_property *property, uint64_t val)
{
	SDE_DEBUG("\n");

	return sde_plane_atomic_set_property(plane,
			plane->state, property, val);
}

static int sde_plane_atomic_get_property(struct drm_plane *plane,
		const struct drm_plane_state *state,
		struct drm_property *property, uint64_t *val)
{
	struct sde_plane *psde = plane ? to_sde_plane(plane) : NULL;
	struct sde_plane_state *pstate;
	int ret = -EINVAL;

	if (!plane) {
		SDE_ERROR("invalid plane\n");
	} else if (!state) {
		SDE_ERROR("invalid state\n");
	} else {
		SDE_DEBUG_PLANE(psde, "\n");
		pstate = to_sde_plane_state(state);
		ret = msm_property_atomic_get(&psde->property_info,
				pstate->property_values, pstate->property_blobs,
				property, val);
	}

	return ret;
}

static void sde_plane_destroy(struct drm_plane *plane)
{
	struct sde_plane *psde = plane ? to_sde_plane(plane) : NULL;

	SDE_DEBUG_PLANE(psde, "\n");

	if (psde) {
		_sde_plane_set_qos_ctrl(plane, false, SDE_PLANE_QOS_PANIC_CTRL);

		debugfs_remove_recursive(psde->debugfs_root);

		if (psde->blob_info)
			drm_property_unreference_blob(psde->blob_info);
		msm_property_destroy(&psde->property_info);
		mutex_destroy(&psde->lock);

		drm_plane_helper_disable(plane);

		/* this will destroy the states as well */
		drm_plane_cleanup(plane);

		if (psde->pipe_hw)
			sde_hw_sspp_destroy(psde->pipe_hw);

		kfree(psde);
	}
}

static void sde_plane_destroy_state(struct drm_plane *plane,
		struct drm_plane_state *state)
{
	struct sde_plane *psde;
	struct sde_plane_state *pstate;

	if (!plane || !state) {
		SDE_ERROR("invalid arg(s), plane %d state %d\n",
				plane != 0, state != 0);
		return;
	}

	psde = to_sde_plane(plane);
	pstate = to_sde_plane_state(state);

	SDE_DEBUG_PLANE(psde, "\n");

	/* remove ref count for frame buffers */
	if (state->fb)
		drm_framebuffer_unreference(state->fb);

	/* remove ref count for fence */
	if (pstate->input_fence)
		sde_sync_put(pstate->input_fence);

	/* destroy value helper */
	msm_property_destroy_state(&psde->property_info, pstate,
			pstate->property_values, pstate->property_blobs);
}

static struct drm_plane_state *
sde_plane_duplicate_state(struct drm_plane *plane)
{
	struct sde_plane *psde;
	struct sde_plane_state *pstate;
	struct sde_plane_state *old_state;
	uint64_t input_fence_default;

	if (!plane) {
		SDE_ERROR("invalid plane\n");
		return NULL;
	} else if (!plane->state) {
		SDE_ERROR("invalid plane state\n");
		return NULL;
	}

	old_state = to_sde_plane_state(plane->state);
	psde = to_sde_plane(plane);
	pstate = msm_property_alloc_state(&psde->property_info);
	if (!pstate) {
		SDE_ERROR_PLANE(psde, "failed to allocate state\n");
		return NULL;
	}

	SDE_DEBUG_PLANE(psde, "\n");

	/* duplicate value helper */
	msm_property_duplicate_state(&psde->property_info, old_state, pstate,
			pstate->property_values, pstate->property_blobs);

	/* add ref count for frame buffer */
	if (pstate->base.fb)
		drm_framebuffer_reference(pstate->base.fb);

	/* clear out any input fence */
	pstate->input_fence = 0;
	input_fence_default = msm_property_get_default(
			&psde->property_info, PLANE_PROP_INPUT_FENCE);
	msm_property_set_property(&psde->property_info, pstate->property_values,
			PLANE_PROP_INPUT_FENCE, input_fence_default);

	pstate->dirty = 0x0;
	pstate->pending = false;

	return &pstate->base;
}

static void sde_plane_reset(struct drm_plane *plane)
{
	struct sde_plane *psde;
	struct sde_plane_state *pstate;

	if (!plane) {
		SDE_ERROR("invalid plane\n");
		return;
	}

	psde = to_sde_plane(plane);
	SDE_DEBUG_PLANE(psde, "\n");

	/* remove previous state, if present */
	if (plane->state) {
		sde_plane_destroy_state(plane, plane->state);
		plane->state = 0;
	}

	pstate = msm_property_alloc_state(&psde->property_info);
	if (!pstate) {
		SDE_ERROR_PLANE(psde, "failed to allocate state\n");
		return;
	}

	/* reset value helper */
	msm_property_reset_state(&psde->property_info, pstate,
			pstate->property_values, pstate->property_blobs);

	pstate->base.plane = plane;

	plane->state = &pstate->base;
}

static const struct drm_plane_funcs sde_plane_funcs = {
		.update_plane = drm_atomic_helper_update_plane,
		.disable_plane = drm_atomic_helper_disable_plane,
		.destroy = sde_plane_destroy,
		.set_property = sde_plane_set_property,
		.atomic_set_property = sde_plane_atomic_set_property,
		.atomic_get_property = sde_plane_atomic_get_property,
		.reset = sde_plane_reset,
		.atomic_duplicate_state = sde_plane_duplicate_state,
		.atomic_destroy_state = sde_plane_destroy_state,
};

static const struct drm_plane_helper_funcs sde_plane_helper_funcs = {
		.prepare_fb = sde_plane_prepare_fb,
		.cleanup_fb = sde_plane_cleanup_fb,
		.atomic_check = sde_plane_atomic_check,
		.atomic_update = sde_plane_atomic_update,
};

enum sde_sspp sde_plane_pipe(struct drm_plane *plane)
{
	return plane ? to_sde_plane(plane)->pipe : SSPP_NONE;
}

static ssize_t _sde_plane_danger_read(struct file *file,
			char __user *buff, size_t count, loff_t *ppos)
{
	struct sde_kms *kms = file->private_data;
	struct sde_mdss_cfg *cfg = kms->catalog;
	int len = 0;
	char buf[40] = {'\0'};

	if (!cfg)
		return -ENODEV;

	if (*ppos)
		return 0; /* the end */

	len = snprintf(buf, sizeof(buf), "%d\n", !kms->has_danger_ctrl);
	if (len < 0 || len >= sizeof(buf))
		return 0;

	if ((count < sizeof(buf)) || copy_to_user(buff, buf, len))
		return -EFAULT;

	*ppos += len;   /* increase offset */

	return len;
}

static void _sde_plane_set_danger_state(struct sde_kms *kms, bool enable)
{
	struct drm_plane *plane;

	drm_for_each_plane(plane, kms->dev) {
		if (plane->fb && plane->state) {
			sde_plane_danger_signal_ctrl(plane, enable);
			SDE_DEBUG("plane:%d img:%dx%d ",
				plane->base.id, plane->fb->width,
				plane->fb->height);
			SDE_DEBUG("src[%d,%d,%d,%d] dst[%d,%d,%d,%d]\n",
				plane->state->src_x >> 16,
				plane->state->src_y >> 16,
				plane->state->src_w >> 16,
				plane->state->src_h >> 16,
				plane->state->crtc_x, plane->state->crtc_y,
				plane->state->crtc_w, plane->state->crtc_h);
		} else {
			SDE_DEBUG("Inactive plane:%d\n", plane->base.id);
		}
	}
}

static ssize_t _sde_plane_danger_write(struct file *file,
		    const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct sde_kms *kms = file->private_data;
	struct sde_mdss_cfg *cfg = kms->catalog;
	int disable_panic;
	char buf[10];

	if (!cfg)
		return -EFAULT;

	if (count >= sizeof(buf))
		return -EFAULT;

	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;

	buf[count] = 0;	/* end of string */

	if (kstrtoint(buf, 0, &disable_panic))
		return -EFAULT;

	if (disable_panic) {
		/* Disable panic signal for all active pipes */
		SDE_DEBUG("Disabling danger:\n");
		_sde_plane_set_danger_state(kms, false);
		kms->has_danger_ctrl = false;
	} else {
		/* Enable panic signal for all active pipes */
		SDE_DEBUG("Enabling danger:\n");
		kms->has_danger_ctrl = true;
		_sde_plane_set_danger_state(kms, true);
	}

	return count;
}

static const struct file_operations sde_plane_danger_enable = {
	.open = simple_open,
	.read = _sde_plane_danger_read,
	.write = _sde_plane_danger_write,
};

static void _sde_plane_init_debugfs(struct sde_plane *psde, struct sde_kms *kms)
{
	const struct sde_sspp_sub_blks *sblk = 0;
	const struct sde_sspp_cfg *cfg = 0;

	if (psde && psde->pipe_hw)
		cfg = psde->pipe_hw->cap;
	if (cfg)
		sblk = cfg->sblk;

	if (kms && sblk) {
		/* create overall sub-directory for the pipe */
		psde->debugfs_root =
			debugfs_create_dir(psde->pipe_name,
					sde_debugfs_get_root(kms));
		if (psde->debugfs_root) {
			/* don't error check these */
			debugfs_create_x32("features", S_IRUGO | S_IWUSR,
					psde->debugfs_root, &psde->features);

			/* add register dump support */
			sde_debugfs_setup_regset32(&psde->debugfs_src,
					sblk->src_blk.base + cfg->base,
					sblk->src_blk.len,
					kms);
			sde_debugfs_create_regset32("src_blk", S_IRUGO,
					psde->debugfs_root, &psde->debugfs_src);

			sde_debugfs_setup_regset32(&psde->debugfs_scaler,
					sblk->scaler_blk.base + cfg->base,
					sblk->scaler_blk.len,
					kms);
			sde_debugfs_create_regset32("scaler_blk", S_IRUGO,
					psde->debugfs_root,
					&psde->debugfs_scaler);

			sde_debugfs_setup_regset32(&psde->debugfs_csc,
					sblk->csc_blk.base + cfg->base,
					sblk->csc_blk.len,
					kms);
			sde_debugfs_create_regset32("csc_blk", S_IRUGO,
					psde->debugfs_root, &psde->debugfs_csc);

			debugfs_create_u32("xin_id",
					S_IRUGO,
					psde->debugfs_root,
					(u32 *) &cfg->xin_id);
			debugfs_create_u32("clk_ctrl",
					S_IRUGO,
					psde->debugfs_root,
					(u32 *) &cfg->clk_ctrl);
			debugfs_create_x32("creq_vblank",
					S_IRUGO | S_IWUSR,
					psde->debugfs_root,
					(u32 *) &sblk->creq_vblank);
			debugfs_create_x32("danger_vblank",
					S_IRUGO | S_IWUSR,
					psde->debugfs_root,
					(u32 *) &sblk->danger_vblank);

			debugfs_create_file("disable_danger",
					S_IRUGO | S_IWUSR,
					psde->debugfs_root,
					kms, &sde_plane_danger_enable);
		}
	}
}

/* initialize plane */
struct drm_plane *sde_plane_init(struct drm_device *dev,
		uint32_t pipe, bool primary_plane,
		unsigned long possible_crtcs)
{
	struct drm_plane *plane = NULL;
	struct sde_plane *psde;
	struct msm_drm_private *priv;
	struct sde_kms *kms;
	enum drm_plane_type type;
	int ret = -EINVAL;

	if (!dev) {
		SDE_ERROR("[%u]device is NULL\n", pipe);
		goto exit;
	}

	priv = dev->dev_private;
	if (!priv) {
		SDE_ERROR("[%u]private data is NULL\n", pipe);
		goto exit;
	}

	if (!priv->kms) {
		SDE_ERROR("[%u]invalid KMS reference\n", pipe);
		goto exit;
	}
	kms = to_sde_kms(priv->kms);

	if (!kms->catalog) {
		SDE_ERROR("[%u]invalid catalog reference\n", pipe);
		goto exit;
	}

	/* create and zero local structure */
	psde = kzalloc(sizeof(*psde), GFP_KERNEL);
	if (!psde) {
		SDE_ERROR("[%u]failed to allocate local plane struct\n", pipe);
		ret = -ENOMEM;
		goto exit;
	}

	/* cache local stuff for later */
	plane = &psde->base;
	psde->pipe = pipe;
	psde->mmu_id = kms->mmu_id[MSM_SMMU_DOMAIN_UNSECURE];

	/* initialize underlying h/w driver */
	psde->pipe_hw = sde_hw_sspp_init(pipe, kms->mmio, kms->catalog);
	if (IS_ERR(psde->pipe_hw)) {
		SDE_ERROR("[%u]SSPP init failed\n", pipe);
		ret = PTR_ERR(psde->pipe_hw);
		goto clean_plane;
	} else if (!psde->pipe_hw->cap || !psde->pipe_hw->cap->sblk) {
		SDE_ERROR("[%u]SSPP init returned invalid cfg\n", pipe);
		goto clean_sspp;
	}

	/* cache features mask for later */
	psde->features = psde->pipe_hw->cap->features;
	psde->pipe_sblk = psde->pipe_hw->cap->sblk;
	if (!psde->pipe_sblk) {
		SDE_ERROR("[%u]invalid sblk\n", pipe);
		goto clean_sspp;
	}

	if (psde->features & BIT(SDE_SSPP_SCALER_QSEED3)) {
		psde->scaler3_cfg = kzalloc(sizeof(struct sde_hw_scaler3_cfg),
			GFP_KERNEL);
		if (!psde->scaler3_cfg) {
			SDE_ERROR("[%u]failed to allocate scale struct\n",
				pipe);
			ret = -ENOMEM;
			goto clean_sspp;
		}
	}

	/* add plane to DRM framework */
	psde->nformats = sde_populate_formats(psde->pipe_sblk->format_list,
			psde->formats,
			0,
			ARRAY_SIZE(psde->formats));

	if (!psde->nformats) {
		SDE_ERROR("[%u]no valid formats for plane\n", pipe);
		goto clean_sspp;
	}

	if (psde->features & BIT(SDE_SSPP_CURSOR))
		type = DRM_PLANE_TYPE_CURSOR;
	else if (primary_plane)
		type = DRM_PLANE_TYPE_PRIMARY;
	else
		type = DRM_PLANE_TYPE_OVERLAY;
	ret = drm_universal_plane_init(dev, plane, possible_crtcs,
			&sde_plane_funcs, psde->formats, psde->nformats, type);
	if (ret)
		goto clean_sspp;

	/* success! finalize initialization */
	drm_plane_helper_add(plane, &sde_plane_helper_funcs);

	msm_property_init(&psde->property_info, &plane->base, dev,
			priv->plane_property, psde->property_data,
			PLANE_PROP_COUNT, PLANE_PROP_BLOBCOUNT,
			sizeof(struct sde_plane_state));

	_sde_plane_install_properties(plane, kms->catalog);

	/* save user friendly pipe name for later */
	snprintf(psde->pipe_name, SDE_NAME_SIZE, "plane%u", plane->base.id);

	mutex_init(&psde->lock);

	_sde_plane_init_debugfs(psde, kms);

	DRM_INFO("%s created for pipe %u\n", psde->pipe_name, pipe);
	return plane;

clean_sspp:
	if (psde && psde->pipe_hw)
		sde_hw_sspp_destroy(psde->pipe_hw);

	if (psde && psde->scaler3_cfg)
		kfree(psde->scaler3_cfg);
clean_plane:
	kfree(psde);
exit:
	return ERR_PTR(ret);
}
