/*
 * Copyright (C) 2014-2018 The Linux Foundation. All rights reserved.
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

#define SDE_QSEED3_DEFAULT_PRELOAD_H 0x4
#define SDE_QSEED3_DEFAULT_PRELOAD_V 0x3

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
 * struct sde_phy_plane - physical plane structure
 * @sde_plane: Points to virtual plane
 * @phy_plane_list: list of hw pipe(physical plane)
 * @index: index of physical plane (starts from 0, order from left to right)
 * @features: capabilities from catalog
 * @csc_cfg: Decoded user configuration for csc
 * @csc_usr_ptr: Points to csc_cfg if valid user config available
 * @csc_ptr: Points to sde_csc_cfg structure to use for current
 */
struct sde_phy_plane {
	struct sde_plane *sde_plane;
	struct list_head phy_plane_list;
	enum sde_sspp pipe;
	uint32_t index;

	uint32_t features;
	uint32_t nformats;
	uint32_t formats[64];

	struct sde_hw_pipe *pipe_hw;
	struct sde_hw_pipe_cfg pipe_cfg;
	struct sde_hw_sharp_cfg sharp_cfg;
	struct sde_hw_scaler3_cfg *scaler3_cfg;
	struct sde_hw_pipe_qos_cfg pipe_qos_cfg;
	uint32_t color_fill;
	bool is_rt_pipe;

	struct sde_hw_pixel_ext pixel_ext;
	bool pixel_ext_usr;

	struct sde_csc_cfg csc_cfg;
	struct sde_csc_cfg *csc_usr_ptr;
	struct sde_csc_cfg *csc_ptr;

	const struct sde_sspp_sub_blks *pipe_sblk;
};

/*
 * struct sde_plane - local sde plane structure
 */
struct sde_plane {
	struct drm_plane base;

	struct msm_gem_address_space *aspace;
	struct mutex lock;
	bool is_error;
	char pipe_name[SDE_NAME_SIZE];

	struct list_head phy_plane_head;
	u32 num_of_phy_planes;

	struct msm_property_info property_info;
	struct msm_property_data property_data[PLANE_PROP_COUNT];
	struct drm_property_blob *blob_info;

	/* debugfs related stuff */
	struct dentry *debugfs_root;
	struct sde_debugfs_regset32 debugfs_src;
	struct sde_debugfs_regset32 debugfs_scaler;
	struct sde_debugfs_regset32 debugfs_csc;
	bool debugfs_default_scale;
};

#define to_sde_plane(x) container_of(x, struct sde_plane, base)

static bool sde_plane_enabled(struct drm_plane_state *state)
{
	return state && state->fb && state->crtc;
}

static struct sde_kms *_sde_plane_get_kms(struct drm_plane *plane)
{
	struct msm_drm_private *priv;

	if (!plane || !plane->dev)
		return NULL;

	priv = plane->dev->dev_private;
	if (!priv)
		return NULL;

	return to_sde_kms(priv->kms);
}

/**
 * _sde_plane_calc_fill_level - calculate fill level of the given source format
 * @plane:		Pointer to drm plane
 * @fmt:		Pointer to source buffer format
 * @src_wdith:		width of source buffer
 * Return: fill level corresponding to the source buffer/format or 0 if error
 */
static inline int _sde_plane_calc_fill_level(struct sde_phy_plane *pp,
		const struct sde_format *fmt, u32 src_width)
{
	struct sde_plane *psde;
	u32 fixed_buff_size;
	u32 total_fl;

	if (!pp || !fmt) {
		SDE_ERROR("invalid arguments\n");
		return 0;
	}

	psde = pp->sde_plane;
	fixed_buff_size = pp->pipe_sblk->pixel_ram_size;

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
			psde->base.base.id, pp->pipe - SSPP_VIG0,
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
static void _sde_plane_set_qos_lut(struct sde_phy_plane *pp,
		struct drm_framebuffer *fb)
{
	struct sde_plane *psde;
	const struct sde_format *fmt = NULL;
	u32 qos_lut;
	u32 total_fl = 0;

	if (!pp || !fb) {
		SDE_ERROR("invalid arguments phy_plane %d fb %d\n",
				pp != NULL, fb != NULL);
		return;
	}

	psde = pp->sde_plane;

	if (!pp->pipe_hw || !pp->pipe_sblk) {
		SDE_ERROR("invalid arguments\n");
		return;
	} else if (!pp->pipe_hw->ops.setup_creq_lut) {
		return;
	}

	if (!pp->is_rt_pipe) {
		qos_lut = pp->pipe_sblk->creq_lut_nrt;
	} else {
		fmt = sde_get_sde_format_ext(
				fb->pixel_format,
				fb->modifier,
				drm_format_num_planes(fb->pixel_format));
		if (!fmt) {
			SDE_ERROR("%s: faile to get fmt\n", __func__);
			return;
		}

		total_fl = _sde_plane_calc_fill_level(pp, fmt,
				pp->pipe_cfg.src_rect.w);

		if (SDE_FORMAT_IS_LINEAR(fmt))
			qos_lut = _sde_plane_get_qos_lut_linear(total_fl);
		else
			qos_lut = _sde_plane_get_qos_lut_macrotile(total_fl);
	}

	pp->pipe_qos_cfg.creq_lut = qos_lut;

	trace_sde_perf_set_qos_luts(pp->pipe - SSPP_VIG0,
			(fmt) ? fmt->base.pixel_format : 0,
			pp->is_rt_pipe, total_fl, qos_lut,
			(fmt) ? SDE_FORMAT_IS_LINEAR(fmt) : 0);

	SDE_DEBUG("plane%u: pnum:%d fmt:%x rt:%d fl:%u lut:0x%x\n",
			psde->base.base.id,
			pp->pipe - SSPP_VIG0,
			(fmt) ? fmt->base.pixel_format : 0,
			pp->is_rt_pipe, total_fl, qos_lut);

	pp->pipe_hw->ops.setup_creq_lut(pp->pipe_hw, &pp->pipe_qos_cfg);
}

/**
 * _sde_plane_set_panic_lut - set danger/safe LUT of the given plane
 * @plane:		Pointer to drm plane
 * @fb:			Pointer to framebuffer associated with the given plane
 */
static void _sde_plane_set_danger_lut(struct sde_phy_plane *pp,
		struct drm_framebuffer *fb)
{
	struct sde_plane *psde;
	const struct sde_format *fmt = NULL;
	u32 danger_lut, safe_lut;

	if (!pp || !fb) {
		SDE_ERROR("invalid arguments\n");
		return;
	}

	psde = pp->sde_plane;

	if (!pp->pipe_hw || !pp->pipe_sblk) {
		SDE_ERROR("invalid arguments\n");
		return;
	} else if (!pp->pipe_hw->ops.setup_danger_safe_lut) {
		return;
	}

	if (!pp->is_rt_pipe) {
		danger_lut = pp->pipe_sblk->danger_lut_nrt;
		safe_lut = pp->pipe_sblk->safe_lut_nrt;
	} else {
		fmt = sde_get_sde_format_ext(
				fb->pixel_format,
				fb->modifier,
				drm_format_num_planes(fb->pixel_format));
		if (!fmt) {
			SDE_ERROR("%s: fail to get fmt\n", __func__);
			return;
		}

		if (SDE_FORMAT_IS_LINEAR(fmt)) {
			danger_lut = pp->pipe_sblk->danger_lut_linear;
			safe_lut = pp->pipe_sblk->safe_lut_linear;
		} else {
			danger_lut = pp->pipe_sblk->danger_lut_tile;
			safe_lut = pp->pipe_sblk->safe_lut_tile;
		}
	}

	pp->pipe_qos_cfg.danger_lut = danger_lut;
	pp->pipe_qos_cfg.safe_lut = safe_lut;

	trace_sde_perf_set_danger_luts(pp->pipe - SSPP_VIG0,
			(fmt) ? fmt->base.pixel_format : 0,
			(fmt) ? fmt->fetch_mode : 0,
			pp->pipe_qos_cfg.danger_lut,
			pp->pipe_qos_cfg.safe_lut);

	SDE_DEBUG("plane%u: pnum:%d fmt:%x mode:%d luts[0x%x, 0x%x]\n",
		psde->base.base.id,
		pp->pipe - SSPP_VIG0,
		fmt ? fmt->base.pixel_format : 0,
		fmt ? fmt->fetch_mode : -1,
		pp->pipe_qos_cfg.danger_lut,
		pp->pipe_qos_cfg.safe_lut);

	pp->pipe_hw->ops.setup_danger_safe_lut(pp->pipe_hw,
			&pp->pipe_qos_cfg);
}

/**
 * _sde_plane_set_qos_ctrl - set QoS control of the given plane
 * @plane:		Pointer to drm plane
 * @enable:		true to enable QoS control
 * @flags:		QoS control mode (enum sde_plane_qos)
 */
static void _sde_plane_set_qos_ctrl(struct sde_phy_plane *pp,
	bool enable, u32 flags)
{
	struct sde_plane *psde;

	if (!pp) {
		SDE_ERROR("invalid arguments\n");
		return;
	}

	psde = pp->sde_plane;

	if (!pp->pipe_hw || !pp->pipe_sblk) {
		SDE_ERROR("invalid arguments\n");
		return;
	} else if (!pp->pipe_hw->ops.setup_qos_ctrl) {
		return;
	}

	if (flags & SDE_PLANE_QOS_VBLANK_CTRL) {
		pp->pipe_qos_cfg.creq_vblank = pp->pipe_sblk->creq_vblank;
		pp->pipe_qos_cfg.danger_vblank =
				pp->pipe_sblk->danger_vblank;
		pp->pipe_qos_cfg.vblank_en = enable;
	}

	 if (flags & SDE_PLANE_QOS_VBLANK_AMORTIZE) {
		/* this feature overrules previous VBLANK_CTRL */
		pp->pipe_qos_cfg.vblank_en = false;
		pp->pipe_qos_cfg.creq_vblank = 0; /* clear vblank bits */
	}

	if (flags & SDE_PLANE_QOS_PANIC_CTRL)
		pp->pipe_qos_cfg.danger_safe_en = enable;

	if (!pp->is_rt_pipe) {
		pp->pipe_qos_cfg.vblank_en = false;
		pp->pipe_qos_cfg.danger_safe_en = false;
	}

	SDE_DEBUG("plane%u: pnum:%d ds:%d vb:%d pri[0x%x, 0x%x] is_rt:%d\n",
		psde->base.base.id,
		pp->pipe - SSPP_VIG0,
		pp->pipe_qos_cfg.danger_safe_en,
		pp->pipe_qos_cfg.vblank_en,
		pp->pipe_qos_cfg.creq_vblank,
		pp->pipe_qos_cfg.danger_vblank,
		pp->is_rt_pipe);

	pp->pipe_hw->ops.setup_qos_ctrl(pp->pipe_hw,
			&pp->pipe_qos_cfg);
}

static int sde_plane_danger_signal_ctrl(struct sde_phy_plane *pp, bool enable)
{
	struct sde_plane *psde;
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;

	if (!pp) {
		SDE_ERROR("invalid arguments\n");
		return -EINVAL;
	}
	psde = pp->sde_plane;

	if (!psde->base.dev) {
		SDE_ERROR("invalid arguments\n");
		return -EINVAL;
	}

	priv = psde->base.dev->dev_private;
	if (!priv || !priv->kms) {
		SDE_ERROR("invalid KMS reference\n");
		return -EINVAL;
	}

	sde_kms = to_sde_kms(priv->kms);

	if (!pp->is_rt_pipe)
		goto end;

	sde_power_resource_enable(&priv->phandle, sde_kms->core_client, true);

	_sde_plane_set_qos_ctrl(pp, enable, SDE_PLANE_QOS_PANIC_CTRL);

	sde_power_resource_enable(&priv->phandle, sde_kms->core_client, false);

end:
	return 0;
}

/**
 * _sde_plane_set_ot_limit - set OT limit for the given plane
 * @plane:		Pointer to drm plane
 * @crtc:		Pointer to drm crtc
 */
static void _sde_plane_set_ot_limit(struct sde_phy_plane *pp,
		struct drm_crtc *crtc)
{
	struct sde_plane *psde;
	struct sde_vbif_set_ot_params ot_params;
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;

	if (!pp || !crtc) {
		SDE_ERROR("invalid arguments phy_plane %d crtc %d\n",
				pp != NULL, crtc != NULL);
		return;
	}
	psde = pp->sde_plane;
	if (!psde->base.dev) {
		SDE_ERROR("invalid DRM device\n");
		return;
	}

	priv = psde->base.dev->dev_private;
	if (!priv || !priv->kms) {
		SDE_ERROR("invalid KMS reference\n");
		return;
	}

	sde_kms = to_sde_kms(priv->kms);
	if (!pp->pipe_hw) {
		SDE_ERROR("invalid pipe reference\n");
		return;
	}

	memset(&ot_params, 0, sizeof(ot_params));
	ot_params.xin_id = pp->pipe_hw->cap->xin_id;
	ot_params.num = pp->pipe_hw->idx - SSPP_NONE;
	ot_params.width = pp->pipe_cfg.src_rect.w;
	ot_params.height = pp->pipe_cfg.src_rect.h;
	ot_params.is_wfd = !pp->is_rt_pipe;
	ot_params.frame_rate = crtc->mode.vrefresh;
	ot_params.vbif_idx = VBIF_RT;
	ot_params.clk_ctrl = pp->pipe_hw->cap->clk_ctrl;
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

/**
 * _sde_plane_get_aspace: gets the address space based on the
 *            fb_translation mode property
 */
static int _sde_plane_get_aspace(
		struct sde_plane *psde,
		struct sde_plane_state *pstate,
		struct msm_gem_address_space **aspace)
{
	struct sde_kms *kms;
	int mode;

	if (!psde || !pstate || !aspace) {
		SDE_ERROR("invalid parameters\n");
		return -EINVAL;
	}

	kms = _sde_plane_get_kms(&psde->base);
	if (!kms) {
		SDE_ERROR("invalid kms\n");
		return -EINVAL;
	}

	mode = sde_plane_get_property(pstate,
			PLANE_PROP_FB_TRANSLATION_MODE);

	switch (mode) {
	case SDE_DRM_FB_NON_SEC:
		*aspace = kms->aspace[MSM_SMMU_DOMAIN_UNSECURE];
		if (!aspace)
			return -EINVAL;
		break;
	case SDE_DRM_FB_SEC:
		*aspace = kms->aspace[MSM_SMMU_DOMAIN_SECURE];
		if (!aspace)
			return -EINVAL;
		break;
	case SDE_DRM_FB_SEC_DIR_TRANS:
	case SDE_DRM_FB_NON_SEC_DIR_TRANS:
		*aspace = NULL;
		break;
	default:
		SDE_ERROR("invalid fb_translation mode:%d\n", mode);
		return -EFAULT;
	}

	return 0;
}

static inline void _sde_plane_set_scanout(struct sde_phy_plane *pp,
		struct sde_plane_state *pstate,
		struct sde_hw_pipe_cfg *pipe_cfg,
		struct drm_framebuffer *fb)
{
	struct sde_plane *psde;
	struct msm_gem_address_space *aspace = NULL;
	int ret;

	if (!pp || !pstate || !pipe_cfg || !fb) {
		SDE_ERROR(
			"invalid arg(s), phy_plane %d state %d cfg %d fb %d\n",
			pp != 0, pstate != 0, pipe_cfg != 0, fb != 0);
		return;
	}

	psde = pp->sde_plane;
	if (!pp->pipe_hw) {
		SDE_ERROR_PLANE(psde, "invalid pipe_hw\n");
		return;
	}

	ret = _sde_plane_get_aspace(psde, pstate, &aspace);
	if (ret) {
		SDE_ERROR_PLANE(psde, "Failed to get aspace %d\n", ret);
		return;
	}

	ret = sde_format_populate_layout(aspace, fb, &pipe_cfg->layout);
	if (ret == -EAGAIN)
		SDE_DEBUG_PLANE(psde, "not updating same src addrs\n");
	else if (ret)
		SDE_ERROR_PLANE(psde, "failed to get format layout, %d\n", ret);
	else if (pp->pipe_hw && pp->pipe_hw->ops.setup_sourceaddress)
		pp->pipe_hw->ops.setup_sourceaddress(pp->pipe_hw, pipe_cfg);
}

static int _sde_plane_setup_scaler3_lut(struct sde_phy_plane *pp,
		struct sde_plane_state *pstate)
{
	struct sde_plane *psde;
	struct sde_hw_scaler3_cfg *cfg;
	int ret = 0;

	if (!pp || !pp->sde_plane || !pp->scaler3_cfg) {
		SDE_ERROR("invalid args\n");
		return -EINVAL;
	} else if (!pstate) {
		/* pstate is expected to be null on forced color fill */
		SDE_DEBUG("null pstate\n");
		return -EINVAL;
	}

	psde = pp->sde_plane;
	cfg = pp->scaler3_cfg;

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

static void _sde_plane_setup_scaler3(struct sde_phy_plane *pp,
		uint32_t src_w, uint32_t src_h, uint32_t dst_w, uint32_t dst_h,
		struct sde_hw_scaler3_cfg *scale_cfg,
		const struct sde_format *fmt,
		uint32_t chroma_subsmpl_h, uint32_t chroma_subsmpl_v)
{
	uint32_t decimated, i;

	if (!pp || !scale_cfg || !fmt || !chroma_subsmpl_h ||
			!chroma_subsmpl_v) {
		SDE_ERROR("psde %pK scale_cfg %pK fmt %pK smp_h %d smp_v %d\n"
			, pp, scale_cfg, fmt, chroma_subsmpl_h,
			chroma_subsmpl_v);
		return;
	}

	memset(scale_cfg, 0, sizeof(*scale_cfg));
	memset(&pp->pixel_ext, 0, sizeof(struct sde_hw_pixel_ext));

	decimated = DECIMATED_DIMENSION(src_w,
			pp->pipe_cfg.horz_decimation);
	scale_cfg->phase_step_x[SDE_SSPP_COMP_0] =
		mult_frac((1 << PHASE_STEP_SHIFT), decimated, dst_w);
	decimated = DECIMATED_DIMENSION(src_h,
			pp->pipe_cfg.vert_decimation);
	scale_cfg->phase_step_y[SDE_SSPP_COMP_0] =
		mult_frac((1 << PHASE_STEP_SHIFT), decimated, dst_h);


	scale_cfg->phase_step_y[SDE_SSPP_COMP_1_2] =
		scale_cfg->phase_step_y[SDE_SSPP_COMP_0] / chroma_subsmpl_v;
	scale_cfg->phase_step_x[SDE_SSPP_COMP_1_2] =
		scale_cfg->phase_step_x[SDE_SSPP_COMP_0] / chroma_subsmpl_h;

	scale_cfg->phase_step_x[SDE_SSPP_COMP_2] =
		scale_cfg->phase_step_x[SDE_SSPP_COMP_1_2];
	scale_cfg->phase_step_y[SDE_SSPP_COMP_2] =
		scale_cfg->phase_step_y[SDE_SSPP_COMP_1_2];

	scale_cfg->phase_step_x[SDE_SSPP_COMP_3] =
		scale_cfg->phase_step_x[SDE_SSPP_COMP_0];
	scale_cfg->phase_step_y[SDE_SSPP_COMP_3] =
		scale_cfg->phase_step_y[SDE_SSPP_COMP_0];

	for (i = 0; i < SDE_MAX_PLANES; i++) {
		scale_cfg->src_width[i] = DECIMATED_DIMENSION(src_w,
				pp->pipe_cfg.horz_decimation);
		scale_cfg->src_height[i] = DECIMATED_DIMENSION(src_h,
				pp->pipe_cfg.vert_decimation);
		if (SDE_FORMAT_IS_YUV(fmt))
			scale_cfg->src_width[i] &= ~0x1;
		if (i == SDE_SSPP_COMP_1_2 || i == SDE_SSPP_COMP_2) {
			scale_cfg->src_width[i] /= chroma_subsmpl_h;
			scale_cfg->src_height[i] /= chroma_subsmpl_v;
		}
		scale_cfg->preload_x[i] = SDE_QSEED3_DEFAULT_PRELOAD_H;
		scale_cfg->preload_y[i] = SDE_QSEED3_DEFAULT_PRELOAD_V;
		pp->pixel_ext.num_ext_pxls_top[i] =
			scale_cfg->src_height[i];
		pp->pixel_ext.num_ext_pxls_left[i] =
			scale_cfg->src_width[i];
	}
	if (!(SDE_FORMAT_IS_YUV(fmt)) && (src_h == dst_h)
		&& (src_w == dst_w))
		return;

	scale_cfg->dst_width = dst_w;
	scale_cfg->dst_height = dst_h;
	scale_cfg->y_rgb_filter_cfg = SDE_SCALE_BIL;
	scale_cfg->uv_filter_cfg = SDE_SCALE_BIL;
	scale_cfg->alpha_filter_cfg = SDE_SCALE_ALPHA_BIL;
	scale_cfg->lut_flag = 0;
	scale_cfg->blend_cfg = 1;
	scale_cfg->enable = 1;
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

static inline void _sde_plane_setup_csc(struct sde_phy_plane *pp)
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

	struct sde_plane *psde;

	if (!pp) {
		SDE_ERROR("invalid plane\n");
		return;
	}
	psde = pp->sde_plane;

	/* revert to kernel default if override not available */
	if (pp->csc_usr_ptr)
		pp->csc_ptr = pp->csc_usr_ptr;
	else if (BIT(SDE_SSPP_CSC_10BIT) & pp->features)
		pp->csc_ptr = (struct sde_csc_cfg *)&sde_csc10_YUV2RGB_601L;
	else
		pp->csc_ptr = (struct sde_csc_cfg *)&sde_csc_YUV2RGB_601L;

	SDE_DEBUG_PLANE(psde, "using 0x%X 0x%X 0x%X...\n",
			pp->csc_ptr->csc_mv[0],
			pp->csc_ptr->csc_mv[1],
			pp->csc_ptr->csc_mv[2]);
}

static void sde_color_process_plane_setup(struct drm_plane *plane,
	struct sde_phy_plane *pp)
{
	struct sde_plane *psde;
	struct sde_plane_state *pstate;
	uint32_t hue, saturation, value, contrast;
	struct drm_msm_memcol *memcol = NULL;
	size_t memcol_sz = 0;

	psde = pp->sde_plane;
	pstate = to_sde_plane_state(plane->state);

	hue = (uint32_t) sde_plane_get_property(pstate, PLANE_PROP_HUE_ADJUST);
	if (pp->pipe_hw->ops.setup_pa_hue)
		pp->pipe_hw->ops.setup_pa_hue(pp->pipe_hw, &hue);
	saturation = (uint32_t) sde_plane_get_property(pstate,
		PLANE_PROP_SATURATION_ADJUST);
	if (pp->pipe_hw->ops.setup_pa_sat)
		pp->pipe_hw->ops.setup_pa_sat(pp->pipe_hw, &saturation);
	value = (uint32_t) sde_plane_get_property(pstate,
		PLANE_PROP_VALUE_ADJUST);
	if (pp->pipe_hw->ops.setup_pa_val)
		pp->pipe_hw->ops.setup_pa_val(pp->pipe_hw, &value);
	contrast = (uint32_t) sde_plane_get_property(pstate,
		PLANE_PROP_CONTRAST_ADJUST);
	if (pp->pipe_hw->ops.setup_pa_cont)
		pp->pipe_hw->ops.setup_pa_cont(pp->pipe_hw, &contrast);

	if (pp->pipe_hw->ops.setup_pa_memcolor) {
		/* Skin memory color setup */
		memcol = msm_property_get_blob(&psde->property_info,
					pstate->property_blobs,
					&memcol_sz,
					PLANE_PROP_SKIN_COLOR);
		pp->pipe_hw->ops.setup_pa_memcolor(pp->pipe_hw,
					MEMCOLOR_SKIN, memcol);

		/* Sky memory color setup */
		memcol = msm_property_get_blob(&psde->property_info,
					pstate->property_blobs,
					&memcol_sz,
					PLANE_PROP_SKY_COLOR);
		pp->pipe_hw->ops.setup_pa_memcolor(pp->pipe_hw,
					MEMCOLOR_SKY, memcol);

		/* Foliage memory color setup */
		memcol = msm_property_get_blob(&psde->property_info,
					pstate->property_blobs,
					&memcol_sz,
					PLANE_PROP_FOLIAGE_COLOR);
		pp->pipe_hw->ops.setup_pa_memcolor(pp->pipe_hw,
					MEMCOLOR_FOLIAGE, memcol);
	}
}

static void _sde_plane_setup_scaler(struct sde_phy_plane *pp,
		const struct sde_format *fmt,
		struct sde_plane_state *pstate)
{
	struct sde_hw_pixel_ext *pe;
	uint32_t chroma_subsmpl_h, chroma_subsmpl_v;
	struct sde_plane *psde;

	if (!pp || !fmt || !pstate || !pp->sde_plane) {
		SDE_ERROR("invalid arg(s), phy_plane %d fmt %d\n",
				pp != NULL, fmt != NULL);
		return;
	}
	psde = pp->sde_plane;

	pe = &(pp->pixel_ext);

	pp->pipe_cfg.horz_decimation =
		sde_plane_get_property(pstate, PLANE_PROP_H_DECIMATE);
	pp->pipe_cfg.vert_decimation =
		sde_plane_get_property(pstate, PLANE_PROP_V_DECIMATE);

	/* don't chroma subsample if decimating */
	chroma_subsmpl_h = pp->pipe_cfg.horz_decimation ? 1 :
		drm_format_horz_chroma_subsampling(fmt->base.pixel_format);
	chroma_subsmpl_v = pp->pipe_cfg.vert_decimation ? 1 :
		drm_format_vert_chroma_subsampling(fmt->base.pixel_format);

	/* update scaler */
	if (pp->features & BIT(SDE_SSPP_SCALER_QSEED3)) {
		int error;

		error = _sde_plane_setup_scaler3_lut(pp, pstate);
		if (error || !pp->pixel_ext_usr ||
				psde->debugfs_default_scale) {
			memset(pe, 0, sizeof(struct sde_hw_pixel_ext));
			/* calculate default config for QSEED3 */
			_sde_plane_setup_scaler3(pp,
					pp->pipe_cfg.src_rect.w,
					pp->pipe_cfg.src_rect.h,
					pp->pipe_cfg.dst_rect.w,
					pp->pipe_cfg.dst_rect.h,
					pp->scaler3_cfg, fmt,
					chroma_subsmpl_h, chroma_subsmpl_v);
		}
	} else if (!pp->pixel_ext_usr || !pstate ||
			psde->debugfs_default_scale) {
		uint32_t deci_dim, i;

		/* calculate default configuration for QSEED2 */
		memset(pe, 0, sizeof(struct sde_hw_pixel_ext));

		SDE_DEBUG_PLANE(psde, "default config\n");
		deci_dim = DECIMATED_DIMENSION(pp->pipe_cfg.src_rect.w,
				pp->pipe_cfg.horz_decimation);
		_sde_plane_setup_scaler2(psde,
				deci_dim,
				pp->pipe_cfg.dst_rect.w,
				pe->phase_step_x,
				pe->horz_filter, fmt, chroma_subsmpl_h);

		if (SDE_FORMAT_IS_YUV(fmt))
			deci_dim &= ~0x1;
		_sde_plane_setup_pixel_ext(psde, pp->pipe_cfg.src_rect.w,
				pp->pipe_cfg.dst_rect.w, deci_dim,
				pe->phase_step_x,
				pe->roi_w,
				pe->num_ext_pxls_left,
				pe->num_ext_pxls_right, pe->horz_filter, fmt,
				chroma_subsmpl_h, 0);

		deci_dim = DECIMATED_DIMENSION(pp->pipe_cfg.src_rect.h,
				pp->pipe_cfg.vert_decimation);
		_sde_plane_setup_scaler2(psde,
				deci_dim,
				pp->pipe_cfg.dst_rect.h,
				pe->phase_step_y,
				pe->vert_filter, fmt, chroma_subsmpl_v);
		_sde_plane_setup_pixel_ext(psde, pp->pipe_cfg.src_rect.h,
				pp->pipe_cfg.dst_rect.h, deci_dim,
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
static int _sde_plane_color_fill(struct sde_phy_plane *pp,
		uint32_t color, uint32_t alpha)
{
	const struct sde_format *fmt;

	if (!pp) {
		SDE_ERROR("invalid plane\n");
		return -EINVAL;
	}

	if (!pp->pipe_hw) {
		SDE_ERROR_PLANE(pp->sde_plane, "invalid plane h/w pointer\n");
		return -EINVAL;
	}

	SDE_DEBUG_PLANE(pp->sde_plane, "\n");

	/*
	 * select fill format to match user property expectation,
	 * h/w only supports RGB variants
	 */
	fmt = sde_get_sde_format(DRM_FORMAT_ABGR8888);

	/* update sspp */
	if (fmt && pp->pipe_hw->ops.setup_solidfill) {
		pp->pipe_hw->ops.setup_solidfill(pp->pipe_hw,
				(color & 0xFFFFFF) | ((alpha & 0xFF) << 24));

		/* override scaler/decimation if solid fill */
		pp->pipe_cfg.src_rect.x = 0;
		pp->pipe_cfg.src_rect.y = 0;
		pp->pipe_cfg.src_rect.w = pp->pipe_cfg.dst_rect.w;
		pp->pipe_cfg.src_rect.h = pp->pipe_cfg.dst_rect.h;

		_sde_plane_setup_scaler(pp, fmt, NULL);

		if (pp->pipe_hw->ops.setup_format)
			pp->pipe_hw->ops.setup_format(pp->pipe_hw,
					fmt, SDE_SSPP_SOLID_FILL);

		if (pp->pipe_hw->ops.setup_rects)
			pp->pipe_hw->ops.setup_rects(pp->pipe_hw,
					&pp->pipe_cfg, &pp->pixel_ext,
					pp->scaler3_cfg);
	}

	return 0;
}

static int _sde_plane_mode_set(struct drm_plane *plane,
		struct drm_plane_state *state)
{
	uint32_t nplanes, src_flags = 0x0;
	struct sde_plane *psde;
	struct sde_plane_state *pstate;
	struct sde_crtc_state *cstate;
	const struct sde_format *fmt;
	struct drm_crtc *crtc;
	struct drm_framebuffer *fb;
	struct sde_rect src, dst;
	bool q16_data = true;
	int idx;
	struct sde_phy_plane *pp;
	uint32_t num_of_phy_planes = 0;
	int mode = 0;
	uint32_t crtc_split_width;
	bool is_across_mixer_boundary  = false;

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
	crtc_split_width = get_crtc_split_width(crtc);
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

	list_for_each_entry(pp, &psde->phy_plane_head, phy_plane_list) {
		if (pstate->dirty & SDE_PLANE_DIRTY_RECTS)
			memset(&(pp->pipe_cfg), 0,
					sizeof(struct sde_hw_pipe_cfg));

		_sde_plane_set_scanout(pp, pstate, &pp->pipe_cfg, fb);

		pstate->pending = true;

		pp->is_rt_pipe = sde_crtc_is_rt(crtc);
		_sde_plane_set_qos_ctrl(pp, false, SDE_PLANE_QOS_PANIC_CTRL);
	}

	/* early out if nothing dirty */
	if (!pstate->dirty)
		return 0;

	memset(&src, 0, sizeof(struct sde_rect));

	/* update secure session flag */
	mode = sde_plane_get_property(pstate,
			PLANE_PROP_FB_TRANSLATION_MODE);
	if ((mode == SDE_DRM_FB_SEC) ||
			(mode == SDE_DRM_FB_SEC_DIR_TRANS))
		src_flags |= SDE_SSPP_SECURE_OVERLAY_SESSION;


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
				pp->pipe_cfg.layout.plane_pitch[idx] <<= 1;
			src.h /= 2;
			src.y  = DIV_ROUND_UP(src.y, 2);
			src.y &= ~0x1;
		}

		list_for_each_entry(pp, &psde->phy_plane_head, phy_plane_list)
			num_of_phy_planes++;

		/*
		 * Only need to use one physical plane if plane width
		 * is still within the limitation.
		 */
		is_across_mixer_boundary =
				(plane->state->crtc_x < crtc_split_width) &&
				(plane->state->crtc_x + plane->state->crtc_w >
					crtc_split_width);
		if (crtc_split_width >= (src.x + src.w) &&
				!is_across_mixer_boundary)
			num_of_phy_planes = 1;

		if (num_of_phy_planes > 1) {
			/* Adjust width for multi-pipe */
			src.w /= num_of_phy_planes;
			dst.w /= num_of_phy_planes;
		}

		list_for_each_entry(pp, &psde->phy_plane_head, phy_plane_list) {
			/* Adjust offset for multi-pipe */
			if (num_of_phy_planes > 1) {
				src.x += src.w * pp->index;
				dst.x += dst.w * pp->index;
			}

			/* add extra offset for shared display */
			if (crtc->state) {
				cstate = to_sde_crtc_state(crtc->state);
				if (cstate->is_shared) {
					dst.x += cstate->shared_roi.x;
					dst.y += cstate->shared_roi.y;

					if (sde_plane_get_property(pstate,
						PLANE_PROP_SRC_CONFIG) &
						BIT(SDE_DRM_LINEPADDING)) {
						src.h = cstate->shared_roi.h;
						dst.h = cstate->shared_roi.h;
					}
				}
			}

			pp->pipe_cfg.src_rect = src;
			pp->pipe_cfg.dst_rect = dst;

			/* check for color fill */
			pp->color_fill = (uint32_t)sde_plane_get_property(
					pstate, PLANE_PROP_COLOR_FILL);
			if (pp->color_fill & SDE_PLANE_COLOR_FILL_FLAG) {
				/* skip remaining processing on color fill */
				pstate->dirty = 0x0;
			} else if (pp->pipe_hw->ops.setup_rects) {
				_sde_plane_setup_scaler(pp, fmt, pstate);

				pp->pipe_hw->ops.setup_rects(pp->pipe_hw,
						&pp->pipe_cfg, &pp->pixel_ext,
						pp->scaler3_cfg);
			}
		}
	}

	list_for_each_entry(pp, &psde->phy_plane_head, phy_plane_list) {
		if (((pstate->dirty & SDE_PLANE_DIRTY_FORMAT) ||
				(src_flags &
				 SDE_SSPP_SECURE_OVERLAY_SESSION)) &&
				pp->pipe_hw->ops.setup_format) {
		SDE_DEBUG_PLANE(psde, "rotation 0x%llX\n",
			sde_plane_get_property(pstate, PLANE_PROP_ROTATION));
			if (sde_plane_get_property(pstate, PLANE_PROP_ROTATION)
				& BIT(DRM_REFLECT_X))
				src_flags |= SDE_SSPP_FLIP_LR;
			if (sde_plane_get_property(pstate,
				PLANE_PROP_ROTATION) & BIT(DRM_REFLECT_Y))
				src_flags |= SDE_SSPP_FLIP_UD;

			/* update format */
			pp->pipe_hw->ops.setup_format(pp->pipe_hw,
				fmt, src_flags);

			/* update csc */
			if (SDE_FORMAT_IS_YUV(fmt))
				_sde_plane_setup_csc(pp);
			else
				pp->csc_ptr = NULL;
		}

		sde_color_process_plane_setup(plane, pp);

		/* update sharpening */
		if ((pstate->dirty & SDE_PLANE_DIRTY_SHARPEN) &&
			pp->pipe_hw->ops.setup_sharpening) {
			pp->sharp_cfg.strength = SHARP_STRENGTH_DEFAULT;
			pp->sharp_cfg.edge_thr = SHARP_EDGE_THR_DEFAULT;
			pp->sharp_cfg.smooth_thr = SHARP_SMOOTH_THR_DEFAULT;
			pp->sharp_cfg.noise_thr = SHARP_NOISE_THR_DEFAULT;

			pp->pipe_hw->ops.setup_sharpening(pp->pipe_hw,
					&pp->sharp_cfg);
		}

		_sde_plane_set_qos_lut(pp, fb);
		_sde_plane_set_danger_lut(pp, fb);

		if (plane->type != DRM_PLANE_TYPE_CURSOR) {
			_sde_plane_set_qos_ctrl(pp, true,
				SDE_PLANE_QOS_PANIC_CTRL);
			_sde_plane_set_ot_limit(pp, crtc);
		}
	}

	/* clear dirty */
	pstate->dirty = 0x0;

	return 0;
}

static int sde_plane_prepare_fb(struct drm_plane *plane,
		const struct drm_plane_state *new_state)
{
	struct drm_framebuffer *fb;
	struct sde_plane *psde = to_sde_plane(plane);
	struct sde_plane_state *pstate;
	int rc;

	if (!psde || !new_state)
		return -EINVAL;

	if (!new_state->fb)
		return 0;

	fb = new_state->fb;
	pstate = to_sde_plane_state(new_state);
	rc = _sde_plane_get_aspace(psde, pstate, &psde->aspace);

	if (rc) {
		SDE_ERROR_PLANE(psde, "Failed to get aspace %d\n", rc);
		return rc;
	}

	SDE_DEBUG_PLANE(psde, "FB[%u]\n", fb->base.id);
	return msm_framebuffer_prepare(fb, psde->aspace);
}

static void sde_plane_cleanup_fb(struct drm_plane *plane,
		const struct drm_plane_state *old_state)
{
	struct drm_framebuffer *fb = old_state ? old_state->fb : NULL;
	struct sde_plane *psde = plane ? to_sde_plane(plane) : NULL;

	if (!fb || !psde)
		return;

	SDE_DEBUG_PLANE(psde, "FB[%u]\n", fb->base.id);
	msm_framebuffer_cleanup(fb, psde->aspace);
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
	struct sde_phy_plane *pp;
	uint32_t num_of_phy_planes = 0;

	if (!plane || !state) {
		SDE_ERROR("invalid arg(s), plane %d state %d.\n",
				plane != NULL, state != NULL);
		ret = -EINVAL;
		goto exit;
	}

	psde = to_sde_plane(plane);
	pstate = to_sde_plane_state(state);

	list_for_each_entry(pp, &psde->phy_plane_head, phy_plane_list)
		num_of_phy_planes++;

	deci_w = sde_plane_get_property(pstate, PLANE_PROP_H_DECIMATE);
	deci_h = sde_plane_get_property(pstate, PLANE_PROP_V_DECIMATE);

	/* src values are in Q16 fixed point, convert to integer */
	POPULATE_RECT(&src, state->src_x, state->src_y, state->src_w,
		state->src_h, q16_data);
	POPULATE_RECT(&dst, state->crtc_x, state->crtc_y, state->crtc_w,
		state->crtc_h, !q16_data);

	src_deci_w = DECIMATED_DIMENSION(src.w, deci_w);
	src_deci_h = DECIMATED_DIMENSION(src.h, deci_h);

	SDE_DEBUG_PLANE(psde, "check %d -> %d\n",
		sde_plane_enabled(plane->state), sde_plane_enabled(state));

	if (!sde_plane_enabled(state))
		goto modeset_update;

	fmt = to_sde_format(msm_framebuffer_format(state->fb));

	min_src_size = SDE_FORMAT_IS_YUV(fmt) ? 2 : 1;

	list_for_each_entry(pp, &psde->phy_plane_head, phy_plane_list) {
		if (!pp->pipe_sblk) {
			SDE_ERROR("invalid plane catalog\n");
			ret = -EINVAL;
			goto exit;
		}

		max_upscale = pp->pipe_sblk->maxupscale;
		max_downscale = pp->pipe_sblk->maxdwnscale;
		max_linewidth = pp->pipe_sblk->maxlinewidth;

		if (SDE_FORMAT_IS_YUV(fmt) &&
			(!(pp->features & SDE_SSPP_SCALER) ||
			 !(pp->features & (BIT(SDE_SSPP_CSC)
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
		} else if (SDE_FORMAT_IS_YUV(fmt) && ((src.x & 0x1)
					|| (src.y & 0x1) || (src.w & 0x1)
					|| (src.h & 0x1))) {
			SDE_ERROR_PLANE(psde, "invalid yuv source %u, %u,\"\
				%ux%u\n", src.x, src.y, src.w, src.h);
			ret = -EINVAL;

		/* min dst support */
		} else if (dst.w < 0x1 || dst.h < 0x1) {
			SDE_ERROR_PLANE(psde, "invalid dest rect %u, %u,\"\
				%ux%u\n", dst.x, dst.y, dst.w, dst.h);
			ret = -EINVAL;

		/* decimation validation */
		} else if (deci_w || deci_h) {
			if ((deci_w > pp->pipe_sblk->maxhdeciexp) ||
				(deci_h > pp->pipe_sblk->maxvdeciexp)) {
				SDE_ERROR_PLANE(psde,
						"too much decimation requested\n");
				ret = -EINVAL;
			} else if (fmt->fetch_mode != SDE_FETCH_LINEAR) {
				SDE_ERROR_PLANE(psde,
						"decimation requires linear fetch\n");
				ret = -EINVAL;
			}

		} else if (!(pp->features & SDE_SSPP_SCALER) &&
			((src.w != dst.w) || (src.h != dst.h))) {
			SDE_ERROR_PLANE(psde,
				"pipe doesn't support scaling %ux%u->%ux%u\n",
				src.w, src.h, dst.w, dst.h);
			ret = -EINVAL;

		/* check decimated source width */
		} else if (src_deci_w > max_linewidth * num_of_phy_planes) {
			SDE_ERROR_PLANE(psde,
				"invalid src w:%u, deci w:%u, line w:%u, num_phy_planes:%u\n",
				src.w, src_deci_w, max_linewidth,
				num_of_phy_planes);
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
	struct sde_phy_plane *pp;

	if (!plane) {
		SDE_ERROR("invalid plane\n");
		return;
	}

	psde = to_sde_plane(plane);

	/*
	 * These updates have to be done immediately before the plane flush
	 * timing, and may not be moved to the atomic_update/mode_set functions.
	 */
	list_for_each_entry(pp, &psde->phy_plane_head, phy_plane_list) {
		if (psde->is_error)
		/* force white frame with 100% alpha pipe output on error */
			_sde_plane_color_fill(pp, 0xFFFFFF, 0xFF);
		else if (pp->color_fill & SDE_PLANE_COLOR_FILL_FLAG)
			/* force 100% alpha */
			_sde_plane_color_fill(pp, pp->color_fill, 0xFF);
		else if (pp->pipe_hw && pp->csc_ptr &&
					pp->pipe_hw->ops.setup_csc)
			pp->pipe_hw->ops.setup_csc(pp->pipe_hw, pp->csc_ptr);
	}

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
	struct sde_mdss_cfg *catalog, bool plane_reserved)
{
	static const struct drm_prop_enum_list e_blend_op[] = {
		{SDE_DRM_BLEND_OP_NOT_DEFINED,    "not_defined"},
		{SDE_DRM_BLEND_OP_OPAQUE,         "opaque"},
		{SDE_DRM_BLEND_OP_PREMULTIPLIED,  "premultiplied"},
		{SDE_DRM_BLEND_OP_COVERAGE,       "coverage"}
	};
	static const struct drm_prop_enum_list e_src_config[] = {
		{SDE_DRM_DEINTERLACE, "deinterlace"},
		{SDE_DRM_LINEPADDING, "linepadding"},
	};
	static const struct drm_prop_enum_list e_fb_translation_mode[] = {
		{SDE_DRM_FB_NON_SEC, "non_sec"},
		{SDE_DRM_FB_SEC, "sec"},
		{SDE_DRM_FB_NON_SEC_DIR_TRANS, "non_sec_direct_translation"},
		{SDE_DRM_FB_SEC_DIR_TRANS, "sec_direct_translation"},
	};
	const struct sde_format_extended *format_list = NULL;
	struct sde_kms_info *info;
	struct sde_plane *psde = to_sde_plane(plane);
	int zpos_max = 255;
	int zpos_def = 0;
	char feature_name[256];
	struct sde_phy_plane *pp;
	uint32_t features = 0xFFFFFFFF, nformats = 64;
	u32 maxlinewidth = 0, maxupscale = 0, maxdwnscale = 0;
	u32 maxhdeciexp = 0, maxvdeciexp = 0;

	if (!plane || !psde) {
		SDE_ERROR("invalid plane\n");
		return;
	}
	list_for_each_entry(pp, &psde->phy_plane_head, phy_plane_list) {
		if (!pp->pipe_hw || !pp->pipe_sblk) {
			SDE_ERROR("invalid phy_plane, pipe_hw %d\"\
				pipe_sblk %d\n", pp->pipe_hw != NULL,
				pp->pipe_sblk != NULL);
			return;
		}
	}
	if (!catalog) {
		SDE_ERROR("invalid catalog\n");
		return;
	}

	list_for_each_entry(pp, &psde->phy_plane_head, phy_plane_list) {
		/* Get common features for all pipes */
		features &= pp->features;
		if (nformats > pp->nformats) {
			nformats = pp->nformats;
			format_list = pp->pipe_sblk->format_list;
		}
		if (maxlinewidth < pp->pipe_sblk->maxlinewidth)
			maxlinewidth = pp->pipe_sblk->maxlinewidth;
		if (maxupscale < pp->pipe_sblk->maxupscale)
			maxupscale = pp->pipe_sblk->maxupscale;
		if (maxdwnscale < pp->pipe_sblk->maxdwnscale)
			maxdwnscale = pp->pipe_sblk->maxdwnscale;
		if (maxhdeciexp < pp->pipe_sblk->maxhdeciexp)
			maxhdeciexp = pp->pipe_sblk->maxhdeciexp;
		if (maxvdeciexp < pp->pipe_sblk->maxvdeciexp)
			maxvdeciexp = pp->pipe_sblk->maxvdeciexp;
		break;
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

	list_for_each_entry(pp, &psde->phy_plane_head, phy_plane_list) {
		if (pp->pipe_sblk->maxhdeciexp) {
			msm_property_install_range(&psde->property_info,
				"h_decimate", 0x0, 0,
				pp->pipe_sblk->maxhdeciexp, 0,
				PLANE_PROP_H_DECIMATE);
		}

		if (pp->pipe_sblk->maxvdeciexp) {
			msm_property_install_range(&psde->property_info,
				"v_decimate", 0x0, 0,
				pp->pipe_sblk->maxvdeciexp, 0,
				PLANE_PROP_V_DECIMATE);
		}
		break;
	}

	if (features & BIT(SDE_SSPP_SCALER_QSEED3)) {
		msm_property_install_volatile_range(&psde->property_info,
			"scaler_v2", 0x0, 0, ~0, 0, PLANE_PROP_SCALER_V2);
		msm_property_install_blob(&psde->property_info, "lut_ed", 0,
			PLANE_PROP_SCALER_LUT_ED);
		msm_property_install_blob(&psde->property_info, "lut_cir", 0,
			PLANE_PROP_SCALER_LUT_CIR);
		msm_property_install_blob(&psde->property_info, "lut_sep", 0,
			PLANE_PROP_SCALER_LUT_SEP);
	} else if (features & SDE_SSPP_SCALER) {
		msm_property_install_volatile_range(&psde->property_info,
			"scaler_v1", 0x0, 0, ~0, 0, PLANE_PROP_SCALER_V1);
	}

	if (features & BIT(SDE_SSPP_CSC)) {
		msm_property_install_volatile_range(&psde->property_info,
			"csc_v1", 0x0, 0, ~0, 0, PLANE_PROP_CSC_V1);
	}

	if (features & BIT(SDE_SSPP_HSIC)) {
		snprintf(feature_name, sizeof(feature_name), "%s%d",
			"SDE_SSPP_HUE_V",
			pp->pipe_sblk->hsic_blk.version >> 16);
		msm_property_install_range(&psde->property_info,
			feature_name, 0, 0, 0xFFFFFFFF, 0,
			PLANE_PROP_HUE_ADJUST);
		snprintf(feature_name, sizeof(feature_name), "%s%d",
			"SDE_SSPP_SATURATION_V",
			pp->pipe_sblk->hsic_blk.version >> 16);
		msm_property_install_range(&psde->property_info,
			feature_name, 0, 0, 0xFFFFFFFF, 0,
			PLANE_PROP_SATURATION_ADJUST);
		snprintf(feature_name, sizeof(feature_name), "%s%d",
			"SDE_SSPP_VALUE_V",
			pp->pipe_sblk->hsic_blk.version >> 16);
		msm_property_install_range(&psde->property_info,
			feature_name, 0, 0, 0xFFFFFFFF, 0,
			PLANE_PROP_VALUE_ADJUST);
		snprintf(feature_name, sizeof(feature_name), "%s%d",
			"SDE_SSPP_CONTRAST_V",
			pp->pipe_sblk->hsic_blk.version >> 16);
		msm_property_install_range(&psde->property_info,
			feature_name, 0, 0, 0xFFFFFFFF, 0,
			PLANE_PROP_CONTRAST_ADJUST);
	}

	/* standard properties */
	msm_property_install_rotation(&psde->property_info,
		BIT(DRM_REFLECT_X) | BIT(DRM_REFLECT_Y), PLANE_PROP_ROTATION);

	msm_property_install_enum(&psde->property_info, "blend_op", 0x0, 0,
		e_blend_op, ARRAY_SIZE(e_blend_op), PLANE_PROP_BLEND_OP,
		SDE_DRM_BLEND_OP_PREMULTIPLIED);

	msm_property_install_enum(&psde->property_info, "src_config", 0x0, 1,
		e_src_config, ARRAY_SIZE(e_src_config), PLANE_PROP_SRC_CONFIG,
		0);

	list_for_each_entry(pp, &psde->phy_plane_head, phy_plane_list) {
		if (pp->pipe_hw->ops.setup_solidfill)
			msm_property_install_range(&psde->property_info,
				"color_fill", 0, 0, 0xFFFFFFFF, 0,
				PLANE_PROP_COLOR_FILL);
		break;
	}

	info = kzalloc(sizeof(struct sde_kms_info), GFP_KERNEL);
	if (!info) {
		SDE_ERROR("failed to allocate info memory\n");
		return;
	}

	msm_property_install_blob(&psde->property_info, "capabilities",
		DRM_MODE_PROP_IMMUTABLE, PLANE_PROP_INFO);
	sde_kms_info_reset(info);

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

	sde_kms_info_add_keyint(info, "max_linewidth", maxlinewidth);
	sde_kms_info_add_keyint(info, "max_upscale", maxupscale);
	sde_kms_info_add_keyint(info, "max_downscale", maxdwnscale);
	sde_kms_info_add_keyint(info, "max_horizontal_deci", maxhdeciexp);
	sde_kms_info_add_keyint(info, "max_vertical_deci", maxvdeciexp);

	/* When early RVC is enabled in bootloader and doesn't exit,
	 * user app should not touch the pipe which RVC is on.
	 * So mark the plane_unavailibility to the special pipe's property,
	 * user can parse this property of this pipe and stop this pipe's
	 * allocation after parsing.
	 * plane_reserved is 1, means the pipe is occupied in bootloader.
	 * plane_reserved is 0, means it's not used in bootloader.
	 */
	sde_kms_info_add_keyint(info, "plane_unavailability", plane_reserved);
	msm_property_set_blob(&psde->property_info, &psde->blob_info,
			info->data, info->len, PLANE_PROP_INFO);

	kfree(info);

	if (features & BIT(SDE_SSPP_MEMCOLOR)) {
		snprintf(feature_name, sizeof(feature_name), "%s%d",
			"SDE_SSPP_SKIN_COLOR_V",
			pp->pipe_sblk->memcolor_blk.version >> 16);
		msm_property_install_blob(&psde->property_info, feature_name, 0,
			PLANE_PROP_SKIN_COLOR);
		snprintf(feature_name, sizeof(feature_name), "%s%d",
			"SDE_SSPP_SKY_COLOR_V",
			pp->pipe_sblk->memcolor_blk.version >> 16);
		msm_property_install_blob(&psde->property_info, feature_name, 0,
			PLANE_PROP_SKY_COLOR);
		snprintf(feature_name, sizeof(feature_name), "%s%d",
			"SDE_SSPP_FOLIAGE_COLOR_V",
			pp->pipe_sblk->memcolor_blk.version >> 16);
		msm_property_install_blob(&psde->property_info, feature_name, 0,
			PLANE_PROP_FOLIAGE_COLOR);
	}

	msm_property_install_enum(&psde->property_info, "fb_translation_mode",
			0x0,
			0, e_fb_translation_mode,
			ARRAY_SIZE(e_fb_translation_mode),
			PLANE_PROP_FB_TRANSLATION_MODE, SDE_DRM_FB_NON_SEC);
}

static inline void _sde_plane_set_csc_v1(struct sde_phy_plane *pp,
	void *usr_ptr)
{
	struct sde_drm_csc_v1 csc_v1;
	struct sde_plane *psde;
	int i;

	if (!pp) {
		SDE_ERROR("invalid phy_plane\n");
		return;
	}
	psde = pp->sde_plane;

	pp->csc_usr_ptr = NULL;
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
		pp->csc_cfg.csc_mv[i] = csc_v1.ctm_coeff[i] >> 16;
	for (i = 0; i < SDE_CSC_BIAS_SIZE; ++i) {
		pp->csc_cfg.csc_pre_bv[i] = csc_v1.pre_bias[i];
		pp->csc_cfg.csc_post_bv[i] = csc_v1.post_bias[i];
	}
	for (i = 0; i < SDE_CSC_CLAMP_SIZE; ++i) {
		pp->csc_cfg.csc_pre_lv[i] = csc_v1.pre_clamp[i];
		pp->csc_cfg.csc_post_lv[i] = csc_v1.post_clamp[i];
	}
	pp->csc_usr_ptr = &pp->csc_cfg;
}

static inline void _sde_plane_set_scaler_v1(struct sde_phy_plane *pp,
	void *usr)
{
	struct sde_drm_scaler_v1 scale_v1;
	struct sde_hw_pixel_ext *pe;
	struct sde_plane *psde;
	int i;

	if (!pp) {
		SDE_ERROR("invalid phy_plane\n");
		return;
	}
	psde = pp->sde_plane;

	pp->pixel_ext_usr = false;
	if (!usr) {
		SDE_DEBUG_PLANE(psde, "scale data removed\n");
		return;
	}

	if (copy_from_user(&scale_v1, usr, sizeof(scale_v1))) {
		SDE_ERROR_PLANE(psde, "failed to copy scale data\n");
		return;
	}

	/* populate from user space */
	pe = &(pp->pixel_ext);
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

	pp->pixel_ext_usr = true;

	SDE_DEBUG_PLANE(psde, "user property data copied\n");
}

static inline void _sde_plane_set_scaler_v2(struct sde_phy_plane *pp,
		struct sde_plane_state *pstate, void *usr)
{
	struct sde_drm_scaler_v2 scale_v2;
	struct sde_hw_pixel_ext *pe;
	int i;
	struct sde_hw_scaler3_cfg *cfg;
	struct sde_plane *psde;

	if (!pp) {
		SDE_ERROR("invalid phy_plane\n");
		return;
	}
	psde = pp->sde_plane;

	cfg = pp->scaler3_cfg;
	pp->pixel_ext_usr = false;
	if (!usr) {
		SDE_DEBUG_PLANE(psde, "scale data removed\n");
		return;
	}

	if (copy_from_user(&scale_v2, usr, sizeof(scale_v2))) {
		SDE_ERROR_PLANE(psde, "failed to copy scale data\n");
		return;
	}

	/* detach/ignore user data if 'disabled' */
	if (!scale_v2.enable) {
		SDE_DEBUG_PLANE(psde, "scale data removed\n");
		return;
	}

	/* populate from user space */
	pe = &(pp->pixel_ext);
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
	pp->pixel_ext_usr = true;

	SDE_DEBUG_PLANE(psde, "user property data copied\n");
}

static int sde_plane_atomic_set_property(struct drm_plane *plane,
		struct drm_plane_state *state, struct drm_property *property,
		uint64_t val)
{
	struct sde_plane *psde = plane ? to_sde_plane(plane) : NULL;
	struct sde_plane_state *pstate;
	int idx, ret = -EINVAL;
	struct sde_phy_plane *pp;

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
				list_for_each_entry(pp, &psde->phy_plane_head,
					phy_plane_list) {
					_sde_plane_set_csc_v1(pp, (void *)val);
				}
				break;
			case PLANE_PROP_SCALER_V1:
				list_for_each_entry(pp, &psde->phy_plane_head,
					phy_plane_list) {
					_sde_plane_set_scaler_v1(pp,
						(void *)val);
				}
				break;
			case PLANE_PROP_SCALER_V2:
				list_for_each_entry(pp, &psde->phy_plane_head,
					phy_plane_list) {
					_sde_plane_set_scaler_v2(pp, pstate,
						(void *)val);
				}
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
	struct sde_phy_plane *pp, *n;

	SDE_DEBUG_PLANE(psde, "\n");

	if (psde) {
		list_for_each_entry(pp, &psde->phy_plane_head, phy_plane_list) {
			_sde_plane_set_qos_ctrl(pp,
				false, SDE_PLANE_QOS_PANIC_CTRL);
		}
		debugfs_remove_recursive(psde->debugfs_root);

		if (psde->blob_info)
			drm_property_unreference_blob(psde->blob_info);
		msm_property_destroy(&psde->property_info);
		mutex_destroy(&psde->lock);

		drm_plane_helper_disable(plane);

		/* this will destroy the states as well */
		drm_plane_cleanup(plane);

		list_for_each_entry_safe(pp, n,
				&psde->phy_plane_head, phy_plane_list) {
			if (pp->pipe_hw)
				sde_hw_sspp_destroy(pp->pipe_hw);
			list_del(&pp->phy_plane_list);
			kfree(pp);
		}

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

enum sde_sspp sde_plane_pipe(struct drm_plane *plane, uint32_t index)
{
	struct sde_plane *sde_plane = to_sde_plane(plane);
	struct sde_phy_plane *pp;
	int i = 0;
	enum sde_sspp default_sspp = SSPP_NONE;

	list_for_each_entry(pp, &sde_plane->phy_plane_head, phy_plane_list) {
		if (i == 0)
			default_sspp = pp->pipe;
		if (i ==  index)
			return pp->pipe;
		i++;
	}

	return default_sspp;
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
	struct sde_plane *psde;
	struct sde_phy_plane *pp;

	drm_for_each_plane(plane, kms->dev) {
		if (plane->fb && plane->state) {
			psde = to_sde_plane(plane);
			list_for_each_entry(pp, &psde->phy_plane_head,
				phy_plane_list) {
				sde_plane_danger_signal_ctrl(pp, enable);
			}
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

static void _sde_plane_init_debugfs(struct sde_plane *psde,
	struct sde_kms *kms)
{
	const struct sde_sspp_sub_blks *sblk = 0;
	const struct sde_sspp_cfg *cfg = 0;
	struct sde_phy_plane *pp;

	if (!psde || !kms) {
		SDE_ERROR("invalid arg(s), psde %d   kms %d\n",
					psde != NULL, kms != NULL);
		return;
	}

	/* create overall sub-directory for the pipe */
	psde->debugfs_root = debugfs_create_dir(psde->pipe_name,
				sde_debugfs_get_root(kms));
	if (!psde->debugfs_root)
		return;

	list_for_each_entry(pp, &psde->phy_plane_head, phy_plane_list) {
		debugfs_create_u32("pipe", S_IRUGO | S_IWUSR,
				psde->debugfs_root, &pp->pipe);

		if (!pp->pipe_hw || !pp->pipe_hw->cap ||
			!pp->pipe_hw->cap->sblk)
			continue;
		cfg = pp->pipe_hw->cap;
		sblk = cfg->sblk;

		/* don't error check these */
		debugfs_create_x32("features", S_IRUGO | S_IWUSR,
				psde->debugfs_root, &pp->features);

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
		debugfs_create_bool("default_scaling",
				0644,
				psde->debugfs_root,
				&psde->debugfs_default_scale);

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

		break;
	}
}

static int _sde_init_phy_plane(struct sde_kms *sde_kms,
	struct sde_plane *psde,	uint32_t pipe, uint32_t index,
	struct sde_phy_plane *pp)
{
	int rc = 0;

	pp->pipe_hw = sde_rm_get_hw_by_id(&sde_kms->rm,
		SDE_HW_BLK_SSPP, pipe);
	if (!pp->pipe_hw) {
		SDE_ERROR("Not found resource for id=%d\n", pipe);
		rc = -EINVAL;
		goto end;
	} else if (!pp->pipe_hw->cap || !pp->pipe_hw->cap->sblk) {
		SDE_ERROR("[%u]SSPP returned invalid cfg\n", pipe);
		rc = -EINVAL;
		goto end;
	}

	/* cache features mask for later */
	pp->features = pp->pipe_hw->cap->features;
	pp->pipe_sblk = pp->pipe_hw->cap->sblk;
	if (!pp->pipe_sblk) {
		SDE_ERROR("invalid sblk on pipe %d\n", pipe);
		rc = -EINVAL;
		goto end;
	}

	if (pp->features & BIT(SDE_SSPP_SCALER_QSEED3)) {
		pp->scaler3_cfg = kzalloc(sizeof(struct sde_hw_scaler3_cfg),
			GFP_KERNEL);
		if (!pp->scaler3_cfg) {
			SDE_ERROR("[%u]failed to allocate scale struct\n",
				pipe);
			rc = -ENOMEM;
			goto end;
		}
	}

	/* add plane to DRM framework */
	pp->nformats = sde_populate_formats(
				pp->pipe_sblk->format_list,
				pp->formats,
				NULL,
				ARRAY_SIZE(pp->formats));

	if (!pp->nformats) {
		SDE_ERROR("[%u]no valid formats for plane\n", pipe);
		if (pp->scaler3_cfg)
			kzfree(pp->scaler3_cfg);

		rc = -EINVAL;
		goto end;
	}

	pp->sde_plane = psde;
	pp->pipe = pipe;
	pp->index = index;

end:
	return rc;
}

void sde_plane_update_blob_property(struct drm_plane *plane,
				const char *key,
				int32_t value)
{
	char *kms_info_str = NULL;
	struct sde_plane *sde_plane = to_sde_plane(plane);
	size_t len;

	kms_info_str = (char *)msm_property_get_blob(&sde_plane->property_info,
				&sde_plane->blob_info, &len, 0);
	if (!kms_info_str) {
		SDE_ERROR("get plane property_info failed\n");
		return;
	}

	sde_kms_info_update_keystr(kms_info_str, key, value);
}

/* initialize plane */
struct drm_plane *sde_plane_init(struct drm_device *dev,
		uint32_t pipe, bool primary_plane,
		unsigned long possible_crtcs,
		bool vp_enabled, bool plane_reserved)
{
	struct drm_plane *plane = NULL;
	struct sde_plane *psde;
	struct sde_phy_plane *pp, *n;
	struct msm_drm_private *priv;
	struct sde_kms *kms;
	enum drm_plane_type type;
	int ret = -EINVAL;
	struct sde_vp_cfg *vp;
	struct sde_vp_sub_blks *vp_sub;
	uint32_t features = 0xFFFFFFFF, nformats = 64, formats[64];
	uint32_t index = 0;

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

	INIT_LIST_HEAD(&psde->phy_plane_head);

	/* initialize underlying h/w driver */
	if (vp_enabled) {
		vp = &(kms->catalog->vp[pipe]);
		list_for_each_entry(vp_sub, &vp->sub_blks, pipeid_list) {
			pp = kzalloc(sizeof(*pp), GFP_KERNEL);
			if (!pp) {
				SDE_ERROR("out of memory\n");
				ret = -ENOMEM;
				goto clean_plane;
			}

			ret = _sde_init_phy_plane(kms, psde, vp_sub->sspp_id,
				index, pp);
			if (ret) {
				SDE_ERROR("_sde_init_phy_plane error vp=%d\n",
					pipe);
				kfree(pp);
				ret = -EINVAL;
				goto clean_plane;
			}
			/* Get common features for all pipes */
			features &= pp->features;
			if (nformats > pp->nformats) {
				nformats = pp->nformats;
				memcpy(formats, pp->formats,
					sizeof(formats));
			}
			list_add_tail(&pp->phy_plane_list,
							&psde->phy_plane_head);
			index++;
			psde->num_of_phy_planes++;
		}
	} else {
		pp = kzalloc(sizeof(*pp), GFP_KERNEL);
		if (!pp) {
			SDE_ERROR("out of memory\n");
			ret = -ENOMEM;
			goto clean_plane;
		}

		ret = _sde_init_phy_plane(kms, psde, pipe, index, pp);
		if (ret) {
			SDE_ERROR("_sde_init_phy_plane error id=%d\n",
				pipe);
			kfree(pp);
			ret = -EINVAL;
			goto clean_plane;
		}
		features = pp->features;
		nformats = pp->nformats;
		memcpy(formats, pp->formats,
			sizeof(uint32_t) * 64);
		list_add_tail(&pp->phy_plane_list,
						&psde->phy_plane_head);
		psde->num_of_phy_planes++;
	}

	if (features & BIT(SDE_SSPP_CURSOR))
		type = DRM_PLANE_TYPE_CURSOR;
	else if (primary_plane)
		type = DRM_PLANE_TYPE_PRIMARY;
	else
		type = DRM_PLANE_TYPE_OVERLAY;
	ret = drm_universal_plane_init(dev, plane, possible_crtcs,
			&sde_plane_funcs, formats, nformats, type);
	if (ret)
		goto clean_plane;

	/* success! finalize initialization */
	drm_plane_helper_add(plane, &sde_plane_helper_funcs);

	msm_property_init(&psde->property_info, &plane->base, dev,
			priv->plane_property, psde->property_data,
			PLANE_PROP_COUNT, PLANE_PROP_BLOBCOUNT,
			sizeof(struct sde_plane_state));

	_sde_plane_install_properties(plane, kms->catalog, plane_reserved);

	/* save user friendly pipe name for later */
	snprintf(psde->pipe_name, SDE_NAME_SIZE, "plane%u", plane->base.id);

	mutex_init(&psde->lock);

	_sde_plane_init_debugfs(psde, kms);

	DRM_INFO("%s created for pipe %u\n", psde->pipe_name, pipe);
	return plane;

clean_plane:
	if (psde) {
		list_for_each_entry_safe(pp, n,
			&psde->phy_plane_head, phy_plane_list) {
			if (pp->pipe_hw)
				sde_hw_sspp_destroy(pp->pipe_hw);

			kfree(pp->scaler3_cfg);
			list_del(&pp->phy_plane_list);
			kfree(pp);
		}
		kfree(psde);
	}

exit:
	return ERR_PTR(ret);
}
