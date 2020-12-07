/*
 * Copyright (C) 2014-2020 The Linux Foundation. All rights reserved.
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
#include <linux/dma-buf.h>
#include <uapi/drm/sde_drm.h>
#include <uapi/drm/msm_drm_pp.h>

#include "msm_prop.h"
#include "msm_drv.h"

#include "sde_kms.h"
#include "sde_fence.h"
#include "sde_formats.h"
#include "sde_hw_sspp.h"
#include "sde_hw_catalog_format.h"
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

#define TIME_MULTIPLEX_RECT(r0, r1, buffer_lines) \
	 ((r0).y >= ((r1).y + (r1).h + buffer_lines))

/* multirect rect index */
enum {
	R0,
	R1,
	R_MAX
};

#define SDE_QSEED_DEFAULT_DYN_EXP 0x0

#define DEFAULT_REFRESH_RATE	60

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
 * @aspace: address space pointer
 * @csc_cfg: Decoded user configuration for csc
 * @csc_usr_ptr: Points to csc_cfg if valid user config available
 * @csc_ptr: Points to sde_csc_cfg structure to use for current
 * @mplane_list: List of multirect planes of the same pipe
 * @catalog: Points to sde catalog structure
 * @revalidate: force revalidation of all the plane properties
 * @xin_halt_forced_clk: whether or not clocks were forced on for xin halt
 * @blob_rot_caps: Pointer to rotator capability blob
 */
struct sde_plane {
	struct drm_plane base;

	struct mutex lock;

	enum sde_sspp pipe;
	uint32_t features;      /* capabilities from catalog */
	uint32_t perf_features; /* perf capabilities from catalog */
	uint32_t nformats;
	uint32_t formats[64];

	struct sde_hw_pipe *pipe_hw;
	struct sde_hw_pipe_cfg pipe_cfg;
	struct sde_hw_sharp_cfg sharp_cfg;
	struct sde_hw_pipe_qos_cfg pipe_qos_cfg;
	struct sde_vbif_set_qos_params cached_qos_params;
	uint32_t color_fill;
	bool is_error;
	bool is_rt_pipe;
	bool is_virtual;
	struct list_head mplane_list;
	struct sde_mdss_cfg *catalog;
	bool revalidate;
	bool xin_halt_forced_clk;

	struct sde_csc_cfg csc_cfg;
	struct sde_csc_cfg *csc_usr_ptr;
	struct sde_csc_cfg *csc_ptr;

	const struct sde_sspp_sub_blks *pipe_sblk;

	char pipe_name[SDE_NAME_SIZE];

	struct msm_property_info property_info;
	struct msm_property_data property_data[PLANE_PROP_COUNT];
	struct drm_property_blob *blob_info;
	struct drm_property_blob *blob_rot_caps;

	/* debugfs related stuff */
	struct dentry *debugfs_root;
	bool debugfs_default_scale;
};

#define to_sde_plane(x) container_of(x, struct sde_plane, base)

static int plane_prop_array[PLANE_PROP_COUNT] = {SDE_PLANE_DIRTY_ALL};

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

static struct sde_hw_ctl *_sde_plane_get_hw_ctl(const struct drm_plane *plane)
{
	struct drm_plane_state *pstate = NULL;
	struct drm_crtc *drm_crtc = NULL;
	struct sde_crtc *sde_crtc = NULL;
	struct sde_crtc_mixer *mixer = NULL;
	struct sde_hw_ctl *ctl = NULL;

	if (!plane) {
		DRM_ERROR("Invalid plane %pK\n", plane);
		return NULL;
	}

	pstate = plane->state;
	if (!pstate) {
		DRM_ERROR("Invalid plane state %pK\n", pstate);
		return NULL;
	}

	drm_crtc = pstate->crtc;
	if (!drm_crtc) {
		DRM_ERROR("Invalid drm_crtc %pK\n", drm_crtc);
		return NULL;
	}

	sde_crtc = to_sde_crtc(drm_crtc);
	if (!sde_crtc) {
		DRM_ERROR("invalid sde_crtc %pK\n", sde_crtc);
		return NULL;
	}

	/* it will always return the first mixer and single CTL */
	mixer = sde_crtc->mixers;
	if (!mixer) {
		DRM_ERROR("invalid mixer %pK\n", mixer);
		return NULL;
	}

	ctl = mixer->hw_ctl;
	if (!mixer) {
		DRM_ERROR("invalid ctl %pK\n", ctl);
		return NULL;
	}

	return ctl;
}

static bool sde_plane_enabled(const struct drm_plane_state *state)
{
	return state && state->fb && state->crtc;
}

bool sde_plane_is_sec_ui_allowed(struct drm_plane *plane)
{
	struct sde_plane *psde;

	if (!plane)
		return false;

	psde = to_sde_plane(plane);

	return !(psde->features & BIT(SDE_SSPP_BLOCK_SEC_UI));
}

void sde_plane_setup_src_split_order(struct drm_plane *plane,
		enum sde_sspp_multirect_index rect_mode, bool enable)
{
	struct sde_plane *psde;

	if (!plane)
		return;

	psde = to_sde_plane(plane);
	if (psde->pipe_hw->ops.set_src_split_order)
		psde->pipe_hw->ops.set_src_split_order(psde->pipe_hw,
					rect_mode, enable);
}

/**
 * _sde_plane_set_qos_lut - set danger, safe and creq LUT of the given plane
 * @crtc:		Pointer to drm crtc to find refresh rate on mode
 * @fb:			Pointer to framebuffer associated with the given plane
 */
static void _sde_plane_set_qos_lut(struct drm_plane *plane,
		struct drm_crtc *crtc,
		struct drm_framebuffer *fb)
{
	struct sde_plane *psde;
	const struct sde_format *fmt = NULL;
	u32 frame_rate, qos_count, fps_index = 0, lut_index, index;
	struct sde_perf_cfg *perf;
	struct sde_plane_state *pstate;

	if (!plane || !fb) {
		SDE_ERROR("invalid arguments\n");
		return;
	}

	psde = to_sde_plane(plane);
	pstate = to_sde_plane_state(plane->state);

	if (!psde->pipe_hw || !psde->pipe_sblk || !psde->catalog) {
		SDE_ERROR("invalid arguments\n");
		return;
	} else if (!psde->pipe_hw->ops.setup_qos_lut) {
		return;
	}

	frame_rate = crtc->mode.vrefresh;
	perf = &psde->catalog->perf;
	qos_count = perf->qos_refresh_count;
	while (qos_count && perf->qos_refresh_rate) {
		if (frame_rate >= perf->qos_refresh_rate[qos_count - 1]) {
			fps_index = qos_count - 1;
			break;
		}
		qos_count--;
	}

	if (!psde->is_rt_pipe) {
		lut_index = SDE_QOS_LUT_USAGE_NRT;
	} else {
		fmt = sde_get_sde_format_ext(
				fb->format->format,
				fb->modifier);

		if (fmt && SDE_FORMAT_IS_LINEAR(fmt))
			lut_index = SDE_QOS_LUT_USAGE_LINEAR;
		else if (pstate->scaler3_cfg.enable)
			lut_index = SDE_QOS_LUT_USAGE_MACROTILE_QSEED;
		else
			lut_index = SDE_QOS_LUT_USAGE_MACROTILE;
	}

	index = (fps_index * SDE_QOS_LUT_USAGE_MAX) + lut_index;
	psde->pipe_qos_cfg.danger_lut = perf->danger_lut[index];
	psde->pipe_qos_cfg.safe_lut = perf->safe_lut[index];
	psde->pipe_qos_cfg.creq_lut = perf->creq_lut[index];

	trace_sde_perf_set_qos_luts(psde->pipe - SSPP_VIG0,
			(fmt) ? fmt->base.pixel_format : 0,
			(fmt) ? fmt->fetch_mode : 0,
			psde->pipe_qos_cfg.danger_lut,
			psde->pipe_qos_cfg.safe_lut,
			psde->pipe_qos_cfg.creq_lut);

	SDE_DEBUG(
	 "plane:%u pnum:%d fmt:%4.4s fps:%d mode:%d luts[0x%x,0x%x 0x%llx]\n",
		plane->base.id,
		psde->pipe - SSPP_VIG0,
		fmt ? (char *)&fmt->base.pixel_format : NULL, frame_rate,
		fmt ? fmt->fetch_mode : -1,
		psde->pipe_qos_cfg.danger_lut,
		psde->pipe_qos_cfg.safe_lut,
		psde->pipe_qos_cfg.creq_lut);

	psde->pipe_hw->ops.setup_qos_lut(psde->pipe_hw, &psde->pipe_qos_cfg);
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

void sde_plane_set_revalidate(struct drm_plane *plane, bool enable)
{
	struct sde_plane *psde;

	if (!plane)
		return;

	psde = to_sde_plane(plane);
	psde->revalidate = enable;
}

int sde_plane_danger_signal_ctrl(struct drm_plane *plane, bool enable)
{
	struct sde_plane *psde;
	int rc;

	if (!plane) {
		SDE_ERROR("invalid arguments\n");
		return -EINVAL;
	}

	psde = to_sde_plane(plane);

	if (!psde->is_rt_pipe)
		goto end;

	rc = pm_runtime_get_sync(plane->dev->dev);
	if (rc < 0) {
		SDE_ERROR("failed to enable power resource %d\n", rc);
		SDE_EVT32(rc, SDE_EVTLOG_ERROR);
		return rc;
	}

	_sde_plane_set_qos_ctrl(plane, enable, SDE_PLANE_QOS_PANIC_CTRL);

	pm_runtime_put_sync(plane->dev->dev);

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
				!plane, !crtc);
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

/**
 * _sde_plane_set_vbif_qos - set vbif QoS for the given plane
 * @plane:		Pointer to drm plane
 * @force:		Force update of vbif QoS
 */
static void _sde_plane_set_qos_remap(struct drm_plane *plane, bool force)
{
	struct sde_plane *psde;
	struct sde_vbif_set_qos_params qos_params;
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;

	if (!plane || !plane->dev) {
		SDE_ERROR("invalid arguments\n");
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

	memset(&qos_params, 0, sizeof(qos_params));
	qos_params.vbif_idx = VBIF_RT;
	qos_params.clk_ctrl = psde->pipe_hw->cap->clk_ctrl;
	qos_params.xin_id = psde->pipe_hw->cap->xin_id;
	qos_params.num = psde->pipe_hw->idx - SSPP_VIG0;
	qos_params.client_type = psde->is_rt_pipe ?
					VBIF_RT_CLIENT : VBIF_NRT_CLIENT;

	if (!force && !memcmp(&qos_params, &psde->cached_qos_params,
			sizeof(struct sde_vbif_set_qos_params))) {
		return;
	}
	SDE_DEBUG("changes in vbif QoS parameters, remap it\n");

	memcpy(&psde->cached_qos_params, &qos_params,
			sizeof(struct sde_vbif_set_qos_params));

	SDE_DEBUG("plane%d pipe:%d vbif:%d xin:%d rt:%d, clk_ctrl:%d\n",
			plane->base.id, qos_params.num,
			qos_params.vbif_idx,
			qos_params.xin_id, qos_params.client_type,
			qos_params.clk_ctrl);

	sde_vbif_set_qos_remap(sde_kms, &qos_params);
}

/**
 * _sde_plane_set_ts_prefill - set prefill with traffic shaper
 * @plane:	Pointer to drm plane
 * @pstate:	Pointer to sde plane state
 */
static void _sde_plane_set_ts_prefill(struct drm_plane *plane,
		struct sde_plane_state *pstate)
{
	struct sde_plane *psde;
	struct sde_hw_pipe_ts_cfg cfg;
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;

	if (!plane || !plane->dev) {
		SDE_ERROR("invalid arguments");
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

	if (!psde->pipe_hw || !psde->pipe_hw->ops.setup_ts_prefill)
		return;

	_sde_plane_set_qos_ctrl(plane, false, SDE_PLANE_QOS_VBLANK_AMORTIZE);

	memset(&cfg, 0, sizeof(cfg));
	cfg.size = sde_plane_get_property(pstate,
			PLANE_PROP_PREFILL_SIZE);
	cfg.time = sde_plane_get_property(pstate,
			PLANE_PROP_PREFILL_TIME);

	SDE_DEBUG("plane%d size:%llu time:%llu\n",
			plane->base.id, cfg.size, cfg.time);
	SDE_EVT32_VERBOSE(DRMID(plane), cfg.size, cfg.time);
	psde->pipe_hw->ops.setup_ts_prefill(psde->pipe_hw, &cfg,
			pstate->multirect_index);
}

/* helper to update a state's input fence pointer from the property */
static void _sde_plane_set_input_fence(struct sde_plane *psde,
		struct sde_plane_state *pstate, uint64_t fd)
{
	if (!psde || !pstate) {
		SDE_ERROR("invalid arg(s), plane %d state %d\n",
				!psde, !pstate);
		return;
	}

	/* clear previous reference */
	if (pstate->input_fence)
		sde_sync_put(pstate->input_fence);

	/* get fence pointer for later */
	if (fd == 0)
		pstate->input_fence = NULL;
	else
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
	signed long rc;

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
			rc = sde_sync_wait(input_fence, wait_ms);

			switch (rc) {
			case 0:
				SDE_ERROR_PLANE(psde, "%ums timeout on %08X fd %d\n",
						wait_ms, prefix, sde_plane_get_property(pstate,
						PLANE_PROP_INPUT_FENCE));
				psde->is_error = true;
				sde_kms_timeline_status(plane->dev);
				ret = -ETIMEDOUT;
				break;
			case -ERESTARTSYS:
				SDE_ERROR_PLANE(psde,
					"%ums wait interrupted on %08X\n",
					wait_ms, prefix);
				psde->is_error = true;
				ret = -ERESTARTSYS;
				break;
			case -EINVAL:
				SDE_ERROR_PLANE(psde,
					"invalid fence param for %08X\n",
						prefix);
				psde->is_error = true;
				ret = -EINVAL;
				break;
			default:
				SDE_DEBUG_PLANE(psde, "signaled\n");
				ret = 0;
				break;
			}

			SDE_EVT32_VERBOSE(DRMID(plane), -ret, prefix);
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
		*aspace = NULL;
		break;
	default:
		SDE_ERROR("invalid fb_translation mode:%d\n", mode);
		return -EFAULT;
	}

	return 0;
}

static inline void _sde_plane_set_scanout(struct drm_plane *plane,
		struct sde_plane_state *pstate,
		struct sde_hw_pipe_cfg *pipe_cfg,
		struct drm_framebuffer *fb)
{
	struct sde_plane *psde;
	struct msm_gem_address_space *aspace = NULL;
	int ret, mode;
	bool secure = false;

	if (!plane || !pstate || !pipe_cfg || !fb) {
		SDE_ERROR(
			"invalid arg(s), plane %d state %d cfg %d fb %d\n",
			!plane, !pstate, !pipe_cfg, !fb);
		return;
	}

	psde = to_sde_plane(plane);
	if (!psde->pipe_hw) {
		SDE_ERROR_PLANE(psde, "invalid pipe_hw\n");
		return;
	}

	ret = _sde_plane_get_aspace(psde, pstate, &aspace);
	if (ret) {
		SDE_ERROR_PLANE(psde, "Failed to get aspace %d\n", ret);
		return;
	}

	/*
	 * framebuffer prepare is deferred for prepare_fb calls that
	 * happen during the transition from secure to non-secure.
	 * Handle the prepare at this point for such cases. This can be
	 * expected for one or two frames during the transition.
	 */
	if (aspace && pstate->defer_prepare_fb) {
		SDE_EVT32(DRMID(plane), psde->pipe, aspace->domain_attached);
		ret = msm_framebuffer_prepare(fb, pstate->aspace);
		if (ret) {
			SDE_ERROR_PLANE(psde,
				"failed to prepare framebuffer %d\n", ret);
			return;
		}
		pstate->defer_prepare_fb = false;
	}
	mode = sde_plane_get_property(pstate, PLANE_PROP_FB_TRANSLATION_MODE);
	if ((mode == SDE_DRM_FB_SEC) || (mode == SDE_DRM_FB_SEC_DIR_TRANS))
		secure = true;

	ret = sde_format_populate_layout(aspace, fb, &pipe_cfg->layout);
	if (ret == -EAGAIN)
		SDE_DEBUG_PLANE(psde, "not updating same src addrs\n");
	else if (ret) {
		SDE_ERROR_PLANE(psde, "failed to get format layout, %d\n", ret);

		/*
		 * Force solid fill color on error. This is to prevent
		 * smmu faults during secure session transition.
		 */
		psde->is_error = true;
	} else if (psde->pipe_hw->ops.setup_sourceaddress) {
		SDE_EVT32_VERBOSE(psde->pipe_hw->idx,
				pipe_cfg->layout.width,
				pipe_cfg->layout.height,
				pipe_cfg->layout.plane_addr[0],
				pipe_cfg->layout.plane_size[0],
				pipe_cfg->layout.plane_addr[1],
				pipe_cfg->layout.plane_size[1],
				pipe_cfg->layout.plane_addr[2],
				pipe_cfg->layout.plane_size[2],
				pipe_cfg->layout.plane_addr[3],
				pipe_cfg->layout.plane_size[3],
				pstate->multirect_index,
				secure);
		psde->pipe_hw->ops.setup_sourceaddress(psde->pipe_hw, pipe_cfg,
						pstate->multirect_index);
	}
}

static int _sde_plane_setup_scaler3_lut(struct sde_plane *psde,
		struct sde_plane_state *pstate)
{
	struct sde_hw_scaler3_cfg *cfg;
	int ret = 0;

	if (!psde || !pstate) {
		SDE_ERROR("invalid args\n");
		return -EINVAL;
	}

	cfg = &pstate->scaler3_cfg;

	cfg->dir_lut = msm_property_get_blob(
			&psde->property_info,
			&pstate->property_state, &cfg->dir_len,
			PLANE_PROP_SCALER_LUT_ED);
	cfg->cir_lut = msm_property_get_blob(
			&psde->property_info,
			&pstate->property_state, &cfg->cir_len,
			PLANE_PROP_SCALER_LUT_CIR);
	cfg->sep_lut = msm_property_get_blob(
			&psde->property_info,
			&pstate->property_state, &cfg->sep_len,
			PLANE_PROP_SCALER_LUT_SEP);
	if (!cfg->dir_lut || !cfg->cir_lut || !cfg->sep_lut)
		ret = -ENODATA;
	return ret;
}

static int _sde_plane_setup_scaler3lite_lut(struct sde_plane *psde,
		struct sde_plane_state *pstate)
{
	struct sde_hw_scaler3_cfg *cfg;

	cfg = &pstate->scaler3_cfg;

	cfg->sep_lut = msm_property_get_blob(
			&psde->property_info,
			&pstate->property_state, &cfg->sep_len,
			PLANE_PROP_SCALER_LUT_SEP);

	return cfg->sep_lut ? 0 : -ENODATA;
}

static void _sde_plane_setup_scaler3(struct sde_plane *psde,
		struct sde_plane_state *pstate, const struct sde_format *fmt,
		uint32_t chroma_subsmpl_h, uint32_t chroma_subsmpl_v)
{
	uint32_t decimated, i, src_w, src_h, dst_w, dst_h;
	struct sde_hw_scaler3_cfg *scale_cfg;

	if (!psde || !pstate || !fmt ||
			!chroma_subsmpl_h || !chroma_subsmpl_v) {
		SDE_ERROR("psde %d pstate %d fmt %d smp_h %d smp_v %d\n",
				!!psde, !!pstate, !!fmt, chroma_subsmpl_h,
				chroma_subsmpl_v);
		return;
	}

	scale_cfg = &pstate->scaler3_cfg;
	src_w = psde->pipe_cfg.src_rect.w;
	src_h = psde->pipe_cfg.src_rect.h;
	dst_w = psde->pipe_cfg.dst_rect.w;
	dst_h = psde->pipe_cfg.dst_rect.h;

	memset(scale_cfg, 0, sizeof(*scale_cfg));
	memset(&pstate->pixel_ext, 0, sizeof(struct sde_hw_pixel_ext));

	/*
	 * For inline rotation cases, scaler config is post-rotation,
	 * so swap the dimensions here. However, pixel extension will
	 * need pre-rotation settings, this will be corrected below
	 * when calculating pixel extension settings.
	 */
	if (pstate->rotation & DRM_MODE_ROTATE_90)
		swap(src_w, src_h);

	decimated = DECIMATED_DIMENSION(src_w,
			psde->pipe_cfg.horz_decimation);
	scale_cfg->phase_step_x[SDE_SSPP_COMP_0] =
		mult_frac((1 << PHASE_STEP_SHIFT), decimated, dst_w);
	decimated = DECIMATED_DIMENSION(src_h,
			psde->pipe_cfg.vert_decimation);
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
				psde->pipe_cfg.horz_decimation);
		scale_cfg->src_height[i] = DECIMATED_DIMENSION(src_h,
				psde->pipe_cfg.vert_decimation);
		if (i == SDE_SSPP_COMP_1_2 || i == SDE_SSPP_COMP_2) {
			scale_cfg->src_width[i] /= chroma_subsmpl_h;
			scale_cfg->src_height[i] /= chroma_subsmpl_v;
		}
		scale_cfg->preload_x[i] = psde->pipe_sblk->scaler_blk.h_preload;
		scale_cfg->preload_y[i] = psde->pipe_sblk->scaler_blk.v_preload;

		/* For pixel extension we need the pre-rotated orientation */
		if (pstate->rotation & DRM_MODE_ROTATE_90) {
			pstate->pixel_ext.num_ext_pxls_top[i] =
				scale_cfg->src_width[i];
			pstate->pixel_ext.num_ext_pxls_left[i] =
				scale_cfg->src_height[i];
		} else {
			pstate->pixel_ext.num_ext_pxls_top[i] =
				scale_cfg->src_height[i];
			pstate->pixel_ext.num_ext_pxls_left[i] =
				scale_cfg->src_width[i];
		}
	}

	if ((!(SDE_FORMAT_IS_YUV(fmt)) && (src_h == dst_h)
		&& (src_w == dst_w)) || pstate->multirect_mode)
		return;

	SDE_DEBUG_PLANE(psde,
		"setting bilinear: src:%dx%d dst:%dx%d chroma:%dx%d fmt:%x\n",
			src_w, src_h, dst_w, dst_h,
			chroma_subsmpl_v, chroma_subsmpl_h,
			fmt->base.pixel_format);

	scale_cfg->dst_width = dst_w;
	scale_cfg->dst_height = dst_h;
	scale_cfg->y_rgb_filter_cfg = SDE_SCALE_BIL;
	scale_cfg->uv_filter_cfg = SDE_SCALE_BIL;
	scale_cfg->alpha_filter_cfg = SDE_SCALE_ALPHA_BIL;
	scale_cfg->lut_flag = 0;
	scale_cfg->blend_cfg = 1;
	scale_cfg->enable = 1;
	scale_cfg->dyn_exp_disabled = SDE_QSEED_DEFAULT_DYN_EXP;
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
			!psde, !phase_steps, !filter, !fmt);
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
	struct drm_msm_3d_gamut *vig_gamut = NULL;
	struct drm_msm_igc_lut *igc = NULL;
	struct drm_msm_pgc_lut *gc = NULL;
	size_t memcol_sz = 0, size = 0;
	struct sde_hw_cp_cfg hw_cfg = {};
	struct sde_hw_ctl *ctl = _sde_plane_get_hw_ctl(plane);

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
					&pstate->property_state,
					&memcol_sz,
					PLANE_PROP_SKIN_COLOR);
		psde->pipe_hw->ops.setup_pa_memcolor(psde->pipe_hw,
					MEMCOLOR_SKIN, memcol);

		/* Sky memory color setup */
		memcol = msm_property_get_blob(&psde->property_info,
					&pstate->property_state,
					&memcol_sz,
					PLANE_PROP_SKY_COLOR);
		psde->pipe_hw->ops.setup_pa_memcolor(psde->pipe_hw,
					MEMCOLOR_SKY, memcol);

		/* Foliage memory color setup */
		memcol = msm_property_get_blob(&psde->property_info,
					&pstate->property_state,
					&memcol_sz,
					PLANE_PROP_FOLIAGE_COLOR);
		psde->pipe_hw->ops.setup_pa_memcolor(psde->pipe_hw,
					MEMCOLOR_FOLIAGE, memcol);
	}

	if (pstate->dirty & SDE_PLANE_DIRTY_VIG_GAMUT &&
			psde->pipe_hw->ops.setup_vig_gamut) {
		vig_gamut = msm_property_get_blob(&psde->property_info,
				&pstate->property_state,
				&size,
				PLANE_PROP_VIG_GAMUT);
		hw_cfg.last_feature = 0;
		hw_cfg.ctl = ctl;
		hw_cfg.len = sizeof(struct drm_msm_3d_gamut);
		hw_cfg.payload = vig_gamut;
		psde->pipe_hw->ops.setup_vig_gamut(psde->pipe_hw, &hw_cfg);
	}

	if (pstate->dirty & SDE_PLANE_DIRTY_VIG_IGC &&
			psde->pipe_hw->ops.setup_vig_igc) {
		igc = msm_property_get_blob(&psde->property_info,
				&pstate->property_state,
				&size,
				PLANE_PROP_VIG_IGC);
		hw_cfg.last_feature = 0;
		hw_cfg.ctl = ctl;
		hw_cfg.len = sizeof(struct drm_msm_igc_lut);
		hw_cfg.payload = igc;
		psde->pipe_hw->ops.setup_vig_igc(psde->pipe_hw, &hw_cfg);
	}

	if (pstate->dirty & SDE_PLANE_DIRTY_DMA_IGC &&
			psde->pipe_hw->ops.setup_dma_igc) {
		igc = msm_property_get_blob(&psde->property_info,
				&pstate->property_state,
				&size,
				PLANE_PROP_DMA_IGC);
		hw_cfg.last_feature = 0;
		hw_cfg.ctl = ctl;
		hw_cfg.len = sizeof(struct drm_msm_igc_lut);
		hw_cfg.payload = igc;
		psde->pipe_hw->ops.setup_dma_igc(psde->pipe_hw, &hw_cfg,
				pstate->multirect_index);
	}

	if (pstate->dirty & SDE_PLANE_DIRTY_DMA_GC &&
			psde->pipe_hw->ops.setup_dma_gc) {
		gc = msm_property_get_blob(&psde->property_info,
				&pstate->property_state,
				&size,
				PLANE_PROP_DMA_GC);
		hw_cfg.last_feature = 0;
		hw_cfg.ctl = ctl;
		hw_cfg.len = sizeof(struct drm_msm_pgc_lut);
		hw_cfg.payload = gc;
		psde->pipe_hw->ops.setup_dma_gc(psde->pipe_hw, &hw_cfg,
				pstate->multirect_index);
	}
}

static void _sde_plane_setup_scaler(struct sde_plane *psde,
		struct sde_plane_state *pstate,
		const struct sde_format *fmt, bool color_fill)
{
	struct sde_hw_pixel_ext *pe;
	uint32_t chroma_subsmpl_h, chroma_subsmpl_v;

	if (!psde || !fmt || !pstate) {
		SDE_ERROR("invalid arg(s), plane %d fmt %d state %d\n",
				!psde, !fmt, !pstate);
		return;
	}

	pe = &pstate->pixel_ext;

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
	if (psde->features & BIT(SDE_SSPP_SCALER_QSEED3) ||
			(psde->features & BIT(SDE_SSPP_SCALER_QSEED3LITE))) {
		int rc = -EINVAL;

		if (!color_fill && !psde->debugfs_default_scale)
			rc = is_qseed3_rev_qseed3lite(psde->pipe_hw->catalog) ?
			_sde_plane_setup_scaler3lite_lut(psde, pstate) :
				_sde_plane_setup_scaler3_lut(psde, pstate);
		if (rc || pstate->scaler_check_state !=
					SDE_PLANE_SCLCHECK_SCALER_V2) {
			SDE_EVT32_VERBOSE(DRMID(&psde->base), color_fill,
					pstate->scaler_check_state,
					psde->debugfs_default_scale, rc,
					psde->pipe_cfg.src_rect.w,
					psde->pipe_cfg.src_rect.h,
					psde->pipe_cfg.dst_rect.w,
					psde->pipe_cfg.dst_rect.h,
					pstate->multirect_mode);

			/* calculate default config for QSEED3 */
			_sde_plane_setup_scaler3(psde, pstate, fmt,
					chroma_subsmpl_h, chroma_subsmpl_v);
		}
	} else if (pstate->scaler_check_state != SDE_PLANE_SCLCHECK_SCALER_V1 ||
			color_fill || psde->debugfs_default_scale) {
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
	if (psde->pipe_hw->ops.setup_pre_downscale)
		psde->pipe_hw->ops.setup_pre_downscale(psde->pipe_hw,
				&pstate->pre_down);
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
	const struct drm_plane *plane;
	struct sde_plane_state *pstate;
	bool blend_enable = true;

	if (!psde || !psde->base.state) {
		SDE_ERROR("invalid plane\n");
		return -EINVAL;
	}

	if (!psde->pipe_hw) {
		SDE_ERROR_PLANE(psde, "invalid plane h/w pointer\n");
		return -EINVAL;
	}

	plane = &psde->base;
	pstate = to_sde_plane_state(plane->state);

	SDE_DEBUG_PLANE(psde, "\n");

	/*
	 * select fill format to match user property expectation,
	 * h/w only supports RGB variants
	 */
	fmt = sde_get_sde_format(DRM_FORMAT_ABGR8888);

	blend_enable = (SDE_DRM_BLEND_OP_OPAQUE !=
			sde_plane_get_property(pstate, PLANE_PROP_BLEND_OP));

	/* update sspp */
	if (fmt && psde->pipe_hw->ops.setup_solidfill) {
		psde->pipe_hw->ops.setup_solidfill(psde->pipe_hw,
				(color & 0xFFFFFF) | ((alpha & 0xFF) << 24),
				pstate->multirect_index);

		/* override scaler/decimation if solid fill */
		psde->pipe_cfg.src_rect.x = 0;
		psde->pipe_cfg.src_rect.y = 0;
		psde->pipe_cfg.src_rect.w = psde->pipe_cfg.dst_rect.w;
		psde->pipe_cfg.src_rect.h = psde->pipe_cfg.dst_rect.h;
		_sde_plane_setup_scaler(psde, pstate, fmt, true);

		if (psde->pipe_hw->ops.setup_format)
			psde->pipe_hw->ops.setup_format(psde->pipe_hw,
					fmt, blend_enable,
					SDE_SSPP_SOLID_FILL,
					pstate->multirect_index);

		if (psde->pipe_hw->ops.setup_rects)
			psde->pipe_hw->ops.setup_rects(psde->pipe_hw,
					&psde->pipe_cfg,
					pstate->multirect_index);

		if (psde->pipe_hw->ops.setup_pe)
			psde->pipe_hw->ops.setup_pe(psde->pipe_hw,
					&pstate->pixel_ext);
		if (psde->pipe_hw->ops.setup_scaler &&
				pstate->multirect_index != SDE_SSPP_RECT_1) {
			psde->pipe_hw->ctl = _sde_plane_get_hw_ctl(plane);
			psde->pipe_hw->ops.setup_scaler(psde->pipe_hw,
					&psde->pipe_cfg, &pstate->pixel_ext,
					&pstate->scaler3_cfg);
		}
	}

	return 0;
}

/**
* sde_plane_rot_atomic_check - verify rotator update of the given state
* @plane: Pointer to drm plane
* @state: Pointer to drm plane state to be validated
* return: 0 if success; error code otherwise
*/
static int sde_plane_rot_atomic_check(struct drm_plane *plane,
	struct drm_plane_state *state)
{
	struct sde_plane *psde;
	struct sde_plane_state *pstate, *old_pstate;
	int ret = 0;
	u32 rotation;

	if (!plane || !state) {
		SDE_ERROR("invalid plane/state\n");
		return -EINVAL;
	}

	psde = to_sde_plane(plane);
	pstate = to_sde_plane_state(state);
	old_pstate = to_sde_plane_state(plane->state);

	/* check inline rotation and simplify the transform */
	rotation = drm_rotation_simplify(
			state->rotation,
			DRM_MODE_ROTATE_0 | DRM_MODE_ROTATE_90 |
			DRM_MODE_REFLECT_X | DRM_MODE_REFLECT_Y);

	if ((rotation & DRM_MODE_ROTATE_180) ||
		(rotation & DRM_MODE_ROTATE_270)) {
		SDE_ERROR_PLANE(psde,
			"invalid rotation transform must be simplified 0x%x\n",
			rotation);
		ret = -EINVAL;
		goto exit;
	}

	if (rotation & DRM_MODE_ROTATE_90) {
		struct msm_drm_private *priv = plane->dev->dev_private;
		struct sde_kms *sde_kms;
		const struct msm_format *msm_fmt;
		const struct sde_format *fmt;
		struct sde_rect src;
		bool q16_data = true;

		POPULATE_RECT(&src, state->src_x, state->src_y,
			state->src_w, state->src_h, q16_data);
		/*
		 * DRM framework expects rotation flag in counter-clockwise
		 * direction and the HW expects in clockwise direction.
		 * Flip the flags to match with HW.
		 */
		rotation ^= (DRM_MODE_REFLECT_X | DRM_MODE_REFLECT_Y);

		if (!psde->pipe_sblk->in_rot_maxdwnscale_rt_num ||
			!psde->pipe_sblk->in_rot_maxdwnscale_rt_denom ||
			!psde->pipe_sblk->in_rot_maxdwnscale_nrt ||
			!psde->pipe_sblk->in_rot_maxheight ||
			!psde->pipe_sblk->in_rot_format_list ||
			!(psde->features & BIT(SDE_SSPP_TRUE_INLINE_ROT))) {
			SDE_ERROR_PLANE(psde,
			    "wrong config rt:%d/%d nrt:%d fmt:%d h:%d 0x%x\n",
				!psde->pipe_sblk->in_rot_maxdwnscale_rt_num,
				!psde->pipe_sblk->in_rot_maxdwnscale_rt_denom,
				!psde->pipe_sblk->in_rot_maxdwnscale_nrt,
				!psde->pipe_sblk->in_rot_format_list,
				!psde->pipe_sblk->in_rot_maxheight,
				psde->features);
			ret = -EINVAL;
			goto exit;
		}

		/* check for valid height */
		if (src.h > psde->pipe_sblk->in_rot_maxheight) {
			SDE_ERROR_PLANE(psde,
				"invalid height for inline rot:%d max:%d\n",
				src.h, psde->pipe_sblk->in_rot_maxheight);
			ret = -EINVAL;
			goto exit;
		}

		if (!sde_plane_enabled(state))
			goto exit;

		/* check for valid formats supported by inline rot */
		sde_kms = to_sde_kms(priv->kms);
		msm_fmt = msm_framebuffer_format(state->fb);
		fmt = to_sde_format(msm_fmt);
		ret = sde_format_validate_fmt(&sde_kms->base, fmt,
			psde->pipe_sblk->in_rot_format_list);
	}

exit:
	pstate->rotation = rotation;
	return ret;
}

static bool _sde_plane_halt_requests(struct drm_plane *plane,
		uint32_t xin_id, bool halt_forced_clk, bool enable)
{
	struct sde_plane *psde;
	struct msm_drm_private *priv;
	struct sde_vbif_set_xin_halt_params halt_params;

	if (!plane || !plane->dev) {
		SDE_ERROR("invalid arguments\n");
		return false;
	}

	psde = to_sde_plane(plane);
	if (!psde->pipe_hw || !psde->pipe_hw->cap) {
		SDE_ERROR("invalid pipe reference\n");
		return false;
	}

	priv = plane->dev->dev_private;
	if (!priv || !priv->kms) {
		SDE_ERROR("invalid KMS reference\n");
		return false;
	}

	memset(&halt_params, 0, sizeof(halt_params));
	halt_params.vbif_idx = VBIF_RT;
	halt_params.xin_id = xin_id;
	halt_params.clk_ctrl = psde->pipe_hw->cap->clk_ctrl;
	halt_params.forced_on = halt_forced_clk;
	halt_params.enable = enable;

	return sde_vbif_set_xin_halt(to_sde_kms(priv->kms), &halt_params);
}

void sde_plane_halt_requests(struct drm_plane *plane, bool enable)
{
	struct sde_plane *psde;

	if (!plane) {
		SDE_ERROR("invalid plane\n");
		return;
	}

	psde = to_sde_plane(plane);
	if (!psde->pipe_hw || !psde->pipe_hw->cap) {
		SDE_ERROR("invalid pipe reference\n");
		return;
	}

	SDE_EVT32(DRMID(plane), psde->xin_halt_forced_clk, enable);

	psde->xin_halt_forced_clk =
		_sde_plane_halt_requests(plane, psde->pipe_hw->cap->xin_id,
				psde->xin_halt_forced_clk, enable);
}

void sde_plane_secure_ctrl_xin_client(struct drm_plane *plane,
				struct drm_crtc *crtc)
{
	struct sde_plane *psde;

	if (!plane || !crtc) {
		SDE_ERROR("invalid plane/crtc\n");
		return;
	}
	psde = to_sde_plane(plane);

	if (psde->features & BIT(SDE_SSPP_BLOCK_SEC_UI))
		return;

	/* do all VBIF programming for the sec-ui allowed SSPP */
	_sde_plane_set_qos_remap(plane, true);
	_sde_plane_set_ot_limit(plane, crtc);
}

/**
 * sde_plane_rot_install_properties - install plane rotator properties
 * @plane: Pointer to drm plane
 * @catalog: Pointer to mdss configuration
 * return: none
 */
static void sde_plane_rot_install_properties(struct drm_plane *plane,
	struct sde_mdss_cfg *catalog)
{
	struct sde_plane *psde = to_sde_plane(plane);
	unsigned long supported_rotations = DRM_MODE_ROTATE_0 |
			DRM_MODE_REFLECT_X | DRM_MODE_REFLECT_Y;
	int ret = 0;

	if (!plane || !psde) {
		SDE_ERROR("invalid plane\n");
		return;
	} else if (!catalog) {
		SDE_ERROR("invalid catalog\n");
		return;
	}

	if (psde->features & BIT(SDE_SSPP_TRUE_INLINE_ROT))
		supported_rotations |= DRM_MODE_ROTATE_0 | DRM_MODE_ROTATE_90 |
			DRM_MODE_ROTATE_180 | DRM_MODE_ROTATE_270;

	ret = drm_plane_create_rotation_property(plane,
			DRM_MODE_ROTATE_0, supported_rotations);
	if (ret) {
		DRM_ERROR("create rotation property failed: %d\n", ret);
		return;
	}
}

void sde_plane_clear_multirect(const struct drm_plane_state *drm_state)
{
	struct sde_plane_state *pstate;

	if (!drm_state)
		return;

	pstate = to_sde_plane_state(drm_state);

	pstate->multirect_index = SDE_SSPP_RECT_SOLO;
	pstate->multirect_mode = SDE_SSPP_MULTIRECT_NONE;
}

/**
 * multi_rect validate API allows to validate only R0 and R1 RECT
 * passing for each plane. Client of this API must not pass multiple
 * plane which are not sharing same XIN client. Such calls will fail
 * even though kernel client is passing valid multirect configuration.
 */
int sde_plane_validate_multirect_v2(struct sde_multirect_plane_states *plane)
{
	struct sde_plane_state *pstate[R_MAX];
	const struct drm_plane_state *drm_state[R_MAX];
	struct sde_rect src[R_MAX], dst[R_MAX];
	struct sde_plane *sde_plane[R_MAX];
	const struct sde_format *fmt[R_MAX];
	int xin_id[R_MAX];
	bool q16_data = true;
	int i, j, buffer_lines, width_threshold[R_MAX];
	unsigned int max_tile_height = 1;
	bool parallel_fetch_qualified = true;
	enum sde_sspp_multirect_mode mode = SDE_SSPP_MULTIRECT_NONE;
	const struct msm_format *msm_fmt;
	bool const_alpha_enable = true;

	for (i = 0; i < R_MAX; i++) {
		drm_state[i] = i ? plane->r1 : plane->r0;
		if (!drm_state[i]) {
			SDE_ERROR("drm plane state is NULL\n");
			return -EINVAL;
		}

		pstate[i] = to_sde_plane_state(drm_state[i]);
		sde_plane[i] = to_sde_plane(drm_state[i]->plane);
		xin_id[i] = sde_plane[i]->pipe_hw->cap->xin_id;

		for (j = 0; j < i; j++) {
			if (xin_id[i] != xin_id[j]) {
				SDE_ERROR_PLANE(sde_plane[i],
					"invalid multirect validate call base:%d xin_id:%d curr:%d xin:%d\n",
					j, xin_id[j], i, xin_id[i]);
				return -EINVAL;
			}
		}

		msm_fmt = msm_framebuffer_format(drm_state[i]->fb);
		if (!msm_fmt) {
			SDE_ERROR_PLANE(sde_plane[i], "null fb\n");
			return -EINVAL;
		}
		fmt[i] = to_sde_format(msm_fmt);

		if (SDE_FORMAT_IS_UBWC(fmt[i]) &&
		    (fmt[i]->tile_height > max_tile_height))
			max_tile_height = fmt[i]->tile_height;

		POPULATE_RECT(&src[i], drm_state[i]->src_x, drm_state[i]->src_y,
			drm_state[i]->src_w, drm_state[i]->src_h, q16_data);
		POPULATE_RECT(&dst[i], drm_state[i]->crtc_x,
				drm_state[i]->crtc_y, drm_state[i]->crtc_w,
				drm_state[i]->crtc_h, !q16_data);

		if (src[i].w != dst[i].w || src[i].h != dst[i].h) {
			SDE_ERROR_PLANE(sde_plane[i],
				"scaling is not supported in multirect mode\n");
			return -EINVAL;
		}

		if (SDE_FORMAT_IS_YUV(fmt[i])) {
			SDE_ERROR_PLANE(sde_plane[i],
				"Unsupported format for multirect mode\n");
			return -EINVAL;
		}

		/**
		 * SSPP PD_MEM is split half - one for each RECT.
		 * Tiled formats need 5 lines of buffering while fetching
		 * whereas linear formats need only 2 lines.
		 * So we cannot support more than half of the supported SSPP
		 * width for tiled formats.
		 */
		width_threshold[i] = sde_plane[i]->pipe_sblk->maxlinewidth;
		if (SDE_FORMAT_IS_UBWC(fmt[i]))
			width_threshold[i] /= 2;

		if (parallel_fetch_qualified && src[i].w > width_threshold[i])
			parallel_fetch_qualified = false;

		if (sde_plane[i]->is_virtual)
			mode = sde_plane_get_property(pstate[i],
					PLANE_PROP_MULTIRECT_MODE);

		if (pstate[i]->const_alpha_en != const_alpha_enable)
			const_alpha_enable = false;

	}

	buffer_lines = 2 * max_tile_height;

	/**
	 * fallback to driver mode selection logic if client is using
	 * multirect plane without setting property.
	 *
	 * validate multirect mode configuration based on rectangle
	 */
	switch (mode) {
	case SDE_SSPP_MULTIRECT_NONE:
		if (parallel_fetch_qualified)
			mode = SDE_SSPP_MULTIRECT_PARALLEL;
		else if (TIME_MULTIPLEX_RECT(dst[R1], dst[R0], buffer_lines) ||
			 TIME_MULTIPLEX_RECT(dst[R0], dst[R1], buffer_lines))
			mode = SDE_SSPP_MULTIRECT_TIME_MX;
		else
			SDE_ERROR(
				"planes(%d - %d) multirect mode selection fail\n",
				drm_state[R0]->plane->base.id,
				drm_state[R1]->plane->base.id);
		break;

	case SDE_SSPP_MULTIRECT_PARALLEL:
		if (!parallel_fetch_qualified) {
			SDE_ERROR("R0 plane:%d width_threshold:%d src_w:%d\n",
				drm_state[R0]->plane->base.id,
				width_threshold[R0],  src[R0].w);
			SDE_ERROR("R1 plane:%d width_threshold:%d src_w:%d\n",
				drm_state[R1]->plane->base.id,
				width_threshold[R1],  src[R1].w);
			SDE_ERROR("parallel fetch not qualified\n");
			mode = SDE_SSPP_MULTIRECT_NONE;
		}
		break;

	case SDE_SSPP_MULTIRECT_TIME_MX:
		if (!TIME_MULTIPLEX_RECT(dst[R1], dst[R0], buffer_lines) &&
		    !TIME_MULTIPLEX_RECT(dst[R0], dst[R1], buffer_lines)) {
			SDE_ERROR(
				"buffer_lines:%d R0 plane:%d dst_y:%d dst_h:%d\n",
				buffer_lines, drm_state[R0]->plane->base.id,
				dst[R0].y, dst[R0].h);
			SDE_ERROR(
				"buffer_lines:%d R1 plane:%d dst_y:%d dst_h:%d\n",
				buffer_lines, drm_state[R1]->plane->base.id,
				dst[R1].y, dst[R1].h);
			SDE_ERROR("time multiplexed fetch not qualified\n");
			mode = SDE_SSPP_MULTIRECT_NONE;
		}
		break;

	default:
		SDE_ERROR("bad mode:%d selection\n", mode);
		mode = SDE_SSPP_MULTIRECT_NONE;
		break;
	}

	for (i = 0; i < R_MAX; i++) {
		pstate[i]->multirect_mode = mode;
		pstate[i]->const_alpha_en = const_alpha_enable;
	}

	if (mode == SDE_SSPP_MULTIRECT_NONE)
		return -EINVAL;

	if (sde_plane[R0]->is_virtual) {
		pstate[R0]->multirect_index = SDE_SSPP_RECT_1;
		pstate[R1]->multirect_index = SDE_SSPP_RECT_0;
	} else {
		pstate[R0]->multirect_index = SDE_SSPP_RECT_0;
		pstate[R1]->multirect_index = SDE_SSPP_RECT_1;
	}

	SDE_DEBUG_PLANE(sde_plane[R0], "R0: %d - %d\n",
		pstate[R0]->multirect_mode, pstate[R0]->multirect_index);
	SDE_DEBUG_PLANE(sde_plane[R1], "R1: %d - %d\n",
		pstate[R1]->multirect_mode, pstate[R1]->multirect_index);

	return 0;
}

/**
 * sde_plane_ctl_flush - set/clear control flush bitmask for the given plane
 * @plane: Pointer to drm plane structure
 * @ctl: Pointer to hardware control driver
 * @set: set if true else clear
 */
void sde_plane_ctl_flush(struct drm_plane *plane, struct sde_hw_ctl *ctl,
		bool set)
{
	if (!plane || !ctl) {
		SDE_ERROR("invalid parameters\n");
		return;
	}

	if (!ctl->ops.update_bitmask_sspp) {
		SDE_ERROR("invalid ops\n");
		return;
	}

	ctl->ops.update_bitmask_sspp(ctl, sde_plane_pipe(plane), set);
}

static int sde_plane_prepare_fb(struct drm_plane *plane,
		struct drm_plane_state *new_state)
{
	struct drm_framebuffer *fb = new_state->fb;
	struct sde_plane *psde = to_sde_plane(plane);
	struct sde_plane_state *pstate = to_sde_plane_state(new_state);
	struct sde_hw_fmt_layout layout;
	struct msm_gem_address_space *aspace;
	int ret;

	if (!fb)
		return 0;

	SDE_DEBUG_PLANE(psde, "FB[%u]\n", fb->base.id);

	ret = _sde_plane_get_aspace(psde, pstate, &aspace);
	if (ret) {
		SDE_ERROR_PLANE(psde, "Failed to get aspace\n");
		return ret;
	}

	/* cache aspace */
	pstate->aspace = aspace;

	/*
	 * when transitioning from secure to non-secure,
	 * plane->prepare_fb happens before the commit. In such case,
	 * defer the prepare_fb and handled it late, during the commit
	 * after attaching the domains as part of the transition
	 */
	pstate->defer_prepare_fb = (aspace && !aspace->domain_attached) ?
							true : false;

	if (pstate->defer_prepare_fb) {
		SDE_EVT32(DRMID(plane), psde->pipe);
		SDE_DEBUG_PLANE(psde,
		    "domain not attached, prepare_fb handled later\n");
		return 0;
	}

	if (pstate->aspace && fb) {
		ret = msm_framebuffer_prepare(fb,
				pstate->aspace);
		if (ret) {
			SDE_ERROR("failed to prepare framebuffer\n");
			return ret;
		}
	}

	/* validate framebuffer layout before commit */
	ret = sde_format_populate_layout(pstate->aspace,
			fb, &layout);
	if (ret) {
		SDE_ERROR_PLANE(psde, "failed to get format layout, %d\n", ret);
		return ret;
	}

	return 0;
}

/**
 * _sde_plane_fetch_halt - halts vbif transactions for a plane
 * @plane: Pointer to plane
 * Returns: 0 on success
 */
static int _sde_plane_fetch_halt(struct drm_plane *plane)
{
	struct sde_plane *psde;
	int xin_id;
	enum sde_clk_ctrl_type clk_ctrl;
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;

	psde = to_sde_plane(plane);
	if (!plane || !plane->dev || !psde->pipe_hw) {
		SDE_ERROR("invalid arguments\n");
		return -EINVAL;
	}

	priv = plane->dev->dev_private;
	if (!priv || !priv->kms) {
		SDE_ERROR("invalid KMS reference\n");
		return -EINVAL;
	}

	sde_kms = to_sde_kms(priv->kms);
	clk_ctrl = psde->pipe_hw->cap->clk_ctrl;
	xin_id = psde->pipe_hw->cap->xin_id;
	SDE_DEBUG_PLANE(psde, "pipe:%d xin_id:%d clk_ctrl:%d\n",
			psde->pipe - SSPP_VIG0, xin_id, clk_ctrl);
	SDE_EVT32_VERBOSE(psde, psde->pipe - SSPP_VIG0, xin_id, clk_ctrl);

	return sde_vbif_halt_plane_xin(sde_kms, xin_id, clk_ctrl);
}


static void sde_plane_cleanup_fb(struct drm_plane *plane,
		struct drm_plane_state *old_state)
{
	struct sde_plane *psde = to_sde_plane(plane);
	struct sde_plane_state *old_pstate;
	int ret;

	if (!old_state || !old_state->fb || !plane || !plane->state)
		return;

	old_pstate = to_sde_plane_state(old_state);

	SDE_DEBUG_PLANE(psde, "FB[%u]\n", old_state->fb->base.id);

	/*
	 * plane->state gets populated for next frame after swap_state. If
	 * plane->state->crtc pointer is not populated then it is not used in
	 * the next frame, hence making it an unused plane.
	 */
	if ((plane->state->crtc == NULL) && !psde->is_virtual) {
		SDE_DEBUG_PLANE(psde, "unused pipe:%u\n",
			       psde->pipe - SSPP_VIG0);

		/* halt this plane now */
		ret = pm_runtime_get_sync(plane->dev->dev);
		if (ret < 0) {
			SDE_ERROR("power resource enable failed with %d", ret);
			SDE_EVT32(ret, SDE_EVTLOG_ERROR);
			return;
		}

		ret = _sde_plane_fetch_halt(plane);
		if (ret) {
			SDE_ERROR_PLANE(psde,
				       "unused pipe %u halt failed\n",
				       psde->pipe - SSPP_VIG0);
			SDE_EVT32(DRMID(plane), psde->pipe - SSPP_VIG0,
				       ret, SDE_EVTLOG_ERROR);
		}
		pm_runtime_put_sync(plane->dev->dev);
	}

	msm_framebuffer_cleanup(old_state->fb, old_pstate->aspace);

}

static void _sde_plane_sspp_atomic_check_mode_changed(struct sde_plane *psde,
		struct drm_plane_state *state,
		struct drm_plane_state *old_state)
{
	struct sde_plane_state *pstate = to_sde_plane_state(state);
	struct sde_plane_state *old_pstate = to_sde_plane_state(old_state);
	struct drm_framebuffer *fb, *old_fb;

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
	} else if (pstate->multirect_index != old_pstate->multirect_index ||
			pstate->multirect_mode != old_pstate->multirect_mode) {
		SDE_DEBUG_PLANE(psde, "multirect config updated\n");
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
	} else if (pstate->excl_rect.w != old_pstate->excl_rect.w ||
		   pstate->excl_rect.h != old_pstate->excl_rect.h ||
		   pstate->excl_rect.x != old_pstate->excl_rect.x ||
		   pstate->excl_rect.y != old_pstate->excl_rect.y) {
		SDE_DEBUG_PLANE(psde, "excl_rect updated\n");
		pstate->dirty |= SDE_PLANE_DIRTY_RECTS;
	} else if (pstate->rotation != old_pstate->rotation) {
		SDE_DEBUG_PLANE(psde, "rotation updated 0x%x->0x%x\n",
			pstate->rotation, old_pstate->rotation);
		pstate->dirty |= SDE_PLANE_DIRTY_FORMAT;
	}

	fb = state->fb;
	old_fb = old_state->fb;

	if (!fb || !old_fb) {
		SDE_DEBUG_PLANE(psde, "can't compare fb handles\n");
	} else if ((fb->format->format != old_fb->format->format) ||
			pstate->const_alpha_en != old_pstate->const_alpha_en) {
		SDE_DEBUG_PLANE(psde, "format change\n");
		pstate->dirty |= SDE_PLANE_DIRTY_FORMAT | SDE_PLANE_DIRTY_RECTS;
	} else {
		uint64_t new_mod = fb->modifier;
		uint64_t old_mod = old_fb->modifier;
		uint32_t *new_pitches = fb->pitches;
		uint32_t *old_pitches = old_fb->pitches;
		uint32_t *new_offset = fb->offsets;
		uint32_t *old_offset = old_fb->offsets;
		int i;

		if (new_mod != old_mod) {
			SDE_DEBUG_PLANE(psde,
				"format modifiers change new_mode:%llu old_mode:%llu\n",
				new_mod, old_mod);
			pstate->dirty |= SDE_PLANE_DIRTY_FORMAT |
				SDE_PLANE_DIRTY_RECTS;
		}

		for (i = 0; i < ARRAY_SIZE(fb->pitches); i++) {
			if (new_pitches[i] != old_pitches[i]) {
				SDE_DEBUG_PLANE(psde,
					"pitches change plane:%d old_pitches:%u new_pitches:%u\n",
					i, old_pitches[i], new_pitches[i]);
				pstate->dirty |= SDE_PLANE_DIRTY_RECTS;
				break;
			}
		}
		for (i = 0; i < ARRAY_SIZE(fb->offsets); i++) {
			if (new_offset[i] != old_offset[i]) {
				SDE_DEBUG_PLANE(psde,
					"offset change plane:%d old_offset:%u new_offset:%u\n",
					i, old_offset[i], new_offset[i]);
				pstate->dirty |= SDE_PLANE_DIRTY_FORMAT |
					SDE_PLANE_DIRTY_RECTS;
				break;
			}
		}
	}
}

int sde_plane_validate_src_addr(struct drm_plane *plane,
		unsigned long base_addr, u32 size)
{
	int ret =  -EINVAL;
	u32 addr;
	struct sde_plane *psde = to_sde_plane(plane);

	if (!psde || !base_addr || !size) {
		SDE_ERROR_PLANE(psde, "invalid arguments\n");
		return ret;
	}

	if (psde->pipe_hw && psde->pipe_hw->ops.get_sourceaddress) {
		addr = psde->pipe_hw->ops.get_sourceaddress(psde->pipe_hw,
				is_sde_plane_virtual(plane));
		if ((addr >= base_addr) && (addr < (base_addr + size)))
			ret = 0;
	}

	return ret;
}

static inline bool _sde_plane_is_pre_downscale_enabled(
	struct sde_hw_inline_pre_downscale_cfg *pre_down)
{
	return pre_down->pre_downscale_x_0 || pre_down->pre_downscale_y_0;
}

static int _sde_plane_validate_scaler_v2(struct sde_plane *psde,
		struct sde_plane_state *pstate,
		const struct sde_format *fmt,
		uint32_t img_w, uint32_t img_h,
		uint32_t src_w, uint32_t src_h,
		uint32_t deci_w, uint32_t deci_h)
{
	struct sde_hw_inline_pre_downscale_cfg *pd_cfg;
	bool pre_down_en;
	int i;

	if (!psde || !pstate || !fmt) {
		SDE_ERROR_PLANE(psde, "invalid arguments\n");
		return -EINVAL;
	}

	if (psde->debugfs_default_scale ||
	   (pstate->scaler_check_state != SDE_PLANE_SCLCHECK_SCALER_V2 &&
	    pstate->scaler_check_state != SDE_PLANE_SCLCHECK_SCALER_V2_CHECK))
		return 0;

	pd_cfg = &pstate->pre_down;
	pre_down_en = _sde_plane_is_pre_downscale_enabled(pd_cfg);

	pstate->scaler_check_state = SDE_PLANE_SCLCHECK_INVALID;

	for (i = 0; i < SDE_MAX_PLANES; i++) {
		uint32_t hor_req_pixels, hor_fetch_pixels;
		uint32_t vert_req_pixels, vert_fetch_pixels;
		uint32_t src_w_tmp, src_h_tmp;
		uint32_t scaler_w, scaler_h;
		uint32_t pre_down_ratio_x = 1, pre_down_ratio_y = 1;
		bool rot;

		/* re-use color plane 1's config for plane 2 */
		if (i == 2)
			continue;

		if (pre_down_en) {
			if (i == 0 && pd_cfg->pre_downscale_x_0)
				pre_down_ratio_x = pd_cfg->pre_downscale_x_0;
			if (i == 0 && pd_cfg->pre_downscale_y_0)
				pre_down_ratio_y = pd_cfg->pre_downscale_y_0;
			if ((i == 1 || i == 2) && pd_cfg->pre_downscale_x_1)
				pre_down_ratio_x = pd_cfg->pre_downscale_x_1;
			if ((i == 1 || i == 2) && pd_cfg->pre_downscale_y_1)
				pre_down_ratio_y = pd_cfg->pre_downscale_y_1;
			SDE_DEBUG_PLANE(psde, "pre_down[%d]: x:%d, y:%d\n",
				i, pre_down_ratio_x, pre_down_ratio_y);
		}

		src_w_tmp = src_w;
		src_h_tmp = src_h;

		/*
		 * For chroma plane, width is half for the following sub sampled
		 * formats. Except in case of decimation, where hardware avoids
		 * 1 line of decimation instead of downsampling.
		 */
		if (i == 1) {
			if (!deci_w &&
					(fmt->chroma_sample == SDE_CHROMA_420 ||
					 fmt->chroma_sample == SDE_CHROMA_H2V1))
				src_w_tmp >>= 1;
			if (!deci_h &&
					(fmt->chroma_sample == SDE_CHROMA_420 ||
					 fmt->chroma_sample == SDE_CHROMA_H1V2))
				src_h_tmp >>= 1;
		}

		hor_req_pixels = pstate->pixel_ext.roi_w[i];
		vert_req_pixels = pstate->pixel_ext.roi_h[i];

		hor_fetch_pixels = DECIMATED_DIMENSION(src_w_tmp +
			(int8_t)(pstate->pixel_ext.left_ftch[i] & 0xFF) +
			(int8_t)(pstate->pixel_ext.right_ftch[i] & 0xFF),
			deci_w);
		vert_fetch_pixels = DECIMATED_DIMENSION(src_h_tmp +
			(int8_t)(pstate->pixel_ext.top_ftch[i] & 0xFF) +
			(int8_t)(pstate->pixel_ext.btm_ftch[i] & 0xFF),
			deci_h);

		if ((hor_req_pixels != hor_fetch_pixels) ||
			(hor_fetch_pixels > img_w) ||
			(vert_req_pixels != vert_fetch_pixels) ||
			(vert_fetch_pixels > img_h)) {
			SDE_ERROR_PLANE(psde,
					"req %d/%d, fetch %d/%d, src %dx%d\n",
					hor_req_pixels, vert_req_pixels,
					hor_fetch_pixels, vert_fetch_pixels,
					img_w, img_h);
			return -EINVAL;
		}

		/*
		 * swap the scaler src width & height for inline-rotation 90
		 * comparison with Pixel-Extension, as PE is based on
		 * pre-rotation and QSEED is based on post-rotation
		 */
		rot = pstate->rotation & DRM_MODE_ROTATE_90;
		scaler_w = rot ? pstate->scaler3_cfg.src_height[i]
				    : pstate->scaler3_cfg.src_width[i];
		scaler_h = rot ? pstate->scaler3_cfg.src_width[i]
				    : pstate->scaler3_cfg.src_height[i];
		/*
		 * Alpha plane can only be scaled using bilinear or pixel
		 * repeat/drop, src_width and src_height are only specified
		 * for Y and UV plane
		 */
		if (i != 3 && (hor_req_pixels / pre_down_ratio_x != scaler_w ||
					vert_req_pixels / pre_down_ratio_y
					 != scaler_h)) {
			SDE_ERROR_PLANE(psde,
			    "roi[%d] roi:%dx%d scaler:%dx%d src:%dx%d rot:%d pd:%d/%d\n",
				i, pstate->pixel_ext.roi_w[i],
				pstate->pixel_ext.roi_h[i], scaler_w,
				scaler_h, src_w, src_h, rot,
				pre_down_ratio_x, pre_down_ratio_y);
			return -EINVAL;
		}

		/*
		 * SSPP fetch , unpack output and QSEED3 input lines need
		 * to match for Y plane
		 */
		if (i == 0 &&
			(sde_plane_get_property(pstate, PLANE_PROP_SRC_CONFIG) &
			BIT(SDE_DRM_DEINTERLACE)) &&
			((pstate->scaler3_cfg.src_height[i] != (src_h/2)) ||
			(pstate->pixel_ext.roi_h[i] != (src_h/2)))) {
			SDE_ERROR_PLANE(psde,
				"de-interlace fail roi[%d] %d/%d, src %dx%d, src %dx%d\n",
				i, pstate->pixel_ext.roi_w[i],
				pstate->pixel_ext.roi_h[i],
				pstate->scaler3_cfg.src_width[i],
				pstate->scaler3_cfg.src_height[i],
				src_w, src_h);
			return -EINVAL;
		}
	}

	pstate->scaler_check_state = SDE_PLANE_SCLCHECK_SCALER_V2;
	return 0;
}

static inline bool _sde_plane_has_pre_downscale(struct sde_plane *psde)
{
	return (psde->features & BIT(SDE_SSPP_PREDOWNSCALE));
}

static int _sde_atomic_check_pre_downscale(struct sde_plane *psde,
		struct sde_plane_state *pstate, struct sde_rect *dst,
		u32 src_w, u32 src_h)
{
	int ret = 0;
	u32 min_ratio_numer, min_ratio_denom;
	struct sde_hw_inline_pre_downscale_cfg *pd_cfg = &pstate->pre_down;
	bool pd_x;
	bool pd_y;

	if (!_sde_plane_is_pre_downscale_enabled(pd_cfg))
		return ret;

	pd_x = pd_cfg->pre_downscale_x_0 > 1;
	pd_y = pd_cfg->pre_downscale_y_0 > 1;

	min_ratio_numer = psde->pipe_sblk->in_rot_maxdwnscale_rt_nopd_num;
	min_ratio_denom = psde->pipe_sblk->in_rot_maxdwnscale_rt_nopd_denom;

	if (pd_x && !(_sde_plane_has_pre_downscale(psde))) {
		SDE_ERROR_PLANE(psde,
			"hw does not support pre-downscaler X: 0x%x\n",
			psde->features);
		ret = -EINVAL;
	} else if (pd_y && !(psde->features & BIT(SDE_SSPP_PREDOWNSCALE_Y))) {
		SDE_ERROR_PLANE(psde,
			"hw does not support pre-downscale Y: 0x%x\n",
			psde->features);
		ret = -EINVAL;
	} else if (!min_ratio_numer || !min_ratio_denom) {
		SDE_ERROR_PLANE(psde,
			"min downscale ratio not set! %u / %u\n",
			min_ratio_numer, min_ratio_denom);
		ret = -EINVAL;

	/* compare pre-rotated src w/h with post-rotated dst h/w resp. */
	} else if (pd_x && (src_w < mult_frac(dst->h, min_ratio_numer,
			min_ratio_denom))) {
		SDE_ERROR_PLANE(psde,
			"failed min downscale-x check %u->%u, %u/%u\n",
			src_w, dst->h, min_ratio_numer, min_ratio_denom);
		ret = -EINVAL;
	} else if (pd_y && (src_h < mult_frac(dst->w, min_ratio_numer,
			min_ratio_denom))) {
		SDE_ERROR_PLANE(psde,
			"failed min downscale-y check %u->%u, %u/%u\n",
			src_h, dst->w, min_ratio_numer, min_ratio_denom);
		ret = -EINVAL;
	}

	return ret;

}

static void _sde_plane_get_max_downscale_limits(struct sde_plane *psde,
		struct sde_plane_state *pstate, bool rt_client,
		u32 *max_numer_w, u32 *max_denom_w,
		u32 *max_numer_h, u32 *max_denom_h)
{
	bool rotated, has_predown;
	const struct sde_sspp_sub_blks *sblk;
	struct sde_hw_inline_pre_downscale_cfg *pd;

	rotated = pstate->rotation & DRM_MODE_ROTATE_90;
	sblk = psde->pipe_sblk;
	*max_numer_w = sblk->maxdwnscale;
	*max_denom_w = 1;
	*max_numer_h = sblk->maxdwnscale;
	*max_denom_h = 1;

	has_predown = _sde_plane_has_pre_downscale(psde);
	if (has_predown)
		pd = &pstate->pre_down;

	/**
	 * Inline rotation has different max vertical downscaling limits since
	 * the source-width becomes the scaler's pre-downscaled source-height.
	 **/
	if (rotated) {
		if (rt_client && has_predown) {
			*max_numer_h = pd->pre_downscale_x_0 ?
				sblk->in_rot_maxdwnscale_rt_num :
				sblk->in_rot_maxdwnscale_rt_nopd_num;
			*max_denom_h = pd->pre_downscale_x_0 ?
				sblk->in_rot_maxdwnscale_rt_denom :
				sblk->in_rot_maxdwnscale_rt_nopd_denom;
		} else if (rt_client) {
			*max_numer_h = sblk->in_rot_maxdwnscale_rt_num;
			*max_denom_h = sblk->in_rot_maxdwnscale_rt_denom;
		} else {
			*max_numer_h = sblk->in_rot_maxdwnscale_nrt;
		}
	}
}

static int _sde_atomic_check_decimation_scaler(struct drm_plane_state *state,
	struct sde_plane *psde, const struct sde_format *fmt,
	struct sde_plane_state *pstate, struct sde_rect *src,
	struct sde_rect *dst, u32 width, u32 height)
{
	int ret = 0;
	uint32_t deci_w, deci_h, src_deci_w, src_deci_h;
	uint32_t scaler_src_w, scaler_src_h;
	uint32_t max_downscale_num_w, max_downscale_denom_w;
	uint32_t max_downscale_num_h, max_downscale_denom_h;
	uint32_t max_upscale, max_linewidth = 0;
	bool inline_rotation, rt_client;
	struct drm_crtc *crtc;
	struct drm_crtc_state *new_cstate;
	struct sde_kms *kms;
	const struct sde_sspp_sub_blks *sblk;

	if (!state || !state->state || !state->crtc) {
		SDE_ERROR_PLANE(psde, "invalid arguments\n");
		return -EINVAL;
	}

	kms = _sde_plane_get_kms(&psde->base);

	if (!kms || !kms->catalog) {
		SDE_ERROR_PLANE(psde, "invalid kms");
		return -EINVAL;
	}

	deci_w = sde_plane_get_property(pstate, PLANE_PROP_H_DECIMATE);
	deci_h = sde_plane_get_property(pstate, PLANE_PROP_V_DECIMATE);

	src_deci_w = DECIMATED_DIMENSION(src->w, deci_w);
	src_deci_h = DECIMATED_DIMENSION(src->h, deci_h);

	/* with inline rotator, the source of the scaler is post-rotated */
	inline_rotation = pstate->rotation & DRM_MODE_ROTATE_90 ? true : false;
	if (inline_rotation) {
		scaler_src_w = src_deci_h;
		scaler_src_h = src_deci_w;
	} else {
		scaler_src_w = src_deci_w;
		scaler_src_h = src_deci_h;
	}

	sblk = psde->pipe_sblk;
	max_upscale = psde->pipe_sblk->maxupscale;

	if ((scaler_src_w != state->crtc_w) || (scaler_src_h != state->crtc_h))
		max_linewidth = inline_rotation ?
				 psde->pipe_sblk->in_rot_maxheight :
				 kms->catalog->scaling_linewidth;

	if (!max_linewidth)
		max_linewidth = psde->pipe_sblk->maxlinewidth;

	crtc = state->crtc;
	new_cstate = drm_atomic_get_new_crtc_state(state->state, crtc);
	rt_client = sde_crtc_is_rt_client(crtc, new_cstate);

	_sde_plane_get_max_downscale_limits(psde, pstate, rt_client,
		&max_downscale_num_w, &max_downscale_denom_w,
		&max_downscale_num_h, &max_downscale_denom_h);

	/* decimation validation */
	if ((deci_w || deci_h)
			&& ((deci_w > sblk->maxhdeciexp)
				|| (deci_h > sblk->maxvdeciexp))) {
		SDE_ERROR_PLANE(psde, "too much decimation requested\n");
		ret = -EINVAL;

	} else if ((deci_w || deci_h)
			&& (fmt->fetch_mode != SDE_FETCH_LINEAR)) {
		SDE_ERROR_PLANE(psde, "decimation requires linear fetch\n");
		ret = -EINVAL;

	} else if (!(psde->features & SDE_SSPP_SCALER) &&
		((src->w != dst->w) || (src->h != dst->h))) {
		SDE_ERROR_PLANE(psde,
			"pipe doesn't support scaling %ux%u->%ux%u\n",
			src->w, src->h, dst->w, dst->h);
		ret = -EINVAL;

	/* check decimated source width */
	} else if (scaler_src_w > max_linewidth) {
		SDE_ERROR_PLANE(psde,
			"invalid src w:%u, deci w:%u, line w:%u, rot: %d\n",
			src->w, src_deci_w, max_linewidth, inline_rotation);
		ret = -E2BIG;

	/* check max scaler capability */
	} else if (((scaler_src_w * max_upscale) < dst->w) ||
		((scaler_src_h * max_upscale) < dst->h) ||
		(mult_frac(dst->w, max_downscale_num_w, max_downscale_denom_w)
			< scaler_src_w) ||
		(mult_frac(dst->h, max_downscale_num_h, max_downscale_denom_h)
			< scaler_src_h)) {
		SDE_ERROR_PLANE(psde,
			"too much scaling %ux%u->%ux%u rot:%d dwn:%d/%d %d/%d\n",
			scaler_src_w, scaler_src_h, dst->w, dst->h,
			inline_rotation, max_downscale_num_w,
			max_downscale_denom_w, max_downscale_num_h,
			max_downscale_denom_h);
		ret = -E2BIG;

	/* check inline pre-downscale support */
	} else if (inline_rotation && _sde_atomic_check_pre_downscale(psde,
			 pstate, dst, src_deci_w, src_deci_h)) {
		ret = -EINVAL;

	/* QSEED validation */
	} else if (_sde_plane_validate_scaler_v2(psde, pstate, fmt,
				width, height,
				src->w, src->h, deci_w, deci_h)) {
		ret = -EINVAL;
	}

	return ret;
}

static int _sde_atomic_check_excl_rect(struct sde_plane *psde,
	struct sde_plane_state *pstate, struct sde_rect *src,
	const struct sde_format *fmt, int ret)
{

	/* check excl rect configs */
	if (!ret && pstate->excl_rect.w && pstate->excl_rect.h) {
		struct sde_rect intersect;

		/*
		 * Check exclusion rect against src rect.
		 * it must intersect with source rect.
		 */
		sde_kms_rect_intersect(src, &pstate->excl_rect, &intersect);
		if (intersect.w != pstate->excl_rect.w ||
				intersect.h != pstate->excl_rect.h ||
				SDE_FORMAT_IS_YUV(fmt)) {
			SDE_ERROR_PLANE(psde,
				"invalid excl_rect:{%d,%d,%d,%d} src:{%d,%d,%d,%d}, fmt: %4.4s\n",
				pstate->excl_rect.x, pstate->excl_rect.y,
				pstate->excl_rect.w, pstate->excl_rect.h,
				src->x, src->y, src->w, src->h,
				(char *)&fmt->base.pixel_format);
			ret = -EINVAL;
		}
		SDE_DEBUG_PLANE(psde, "excl_rect: {%d,%d,%d,%d}\n",
				pstate->excl_rect.x, pstate->excl_rect.y,
				pstate->excl_rect.w, pstate->excl_rect.h);
	}

	return ret;
}


static int _sde_plane_validate_shared_crtc(struct sde_plane *psde,
				struct drm_plane_state *state)
{
	struct sde_kms *sde_kms;
	struct sde_splash_display *splash_display;
	int i, j;

	sde_kms = _sde_plane_get_kms(&psde->base);

	if (!sde_kms || !state->crtc)
		return 0;

	for (i = 0; i < MAX_DSI_DISPLAYS; i++) {
		splash_display = &sde_kms->splash_data.splash_display[i];

		if (splash_display && splash_display->cont_splash_enabled &&
			splash_display->encoder &&
			state->crtc != splash_display->encoder->crtc) {

			for (j = 0; j < MAX_DATA_PATH_PER_DSIPLAY; j++) {

				if (splash_display->pipes[j].sspp ==
						psde->pipe) {
					SDE_ERROR_PLANE(psde,
					"pipe:%d used in cont-splash on crtc:%d\n",
					psde->pipe,
					splash_display->encoder->crtc->base.id);
					return -EINVAL;
				}
			}
		}
	}

	return 0;

}

static int sde_plane_sspp_atomic_check(struct drm_plane *plane,
		struct drm_plane_state *state)
{
	int ret = 0;
	struct sde_plane *psde;
	struct sde_plane_state *pstate;
	const struct msm_format *msm_fmt;
	const struct sde_format *fmt;
	struct sde_rect src, dst;
	uint32_t min_src_size;
	bool q16_data = true;
	struct drm_framebuffer *fb;
	u32 width;
	u32 height;

	psde = to_sde_plane(plane);
	pstate = to_sde_plane_state(state);

	if (!psde->pipe_sblk) {
		SDE_ERROR_PLANE(psde, "invalid catalog\n");
		return -EINVAL;
	}

	/* src values are in Q16 fixed point, convert to integer */
	POPULATE_RECT(&src, state->src_x, state->src_y,
			state->src_w, state->src_h, q16_data);
	POPULATE_RECT(&dst, state->crtc_x, state->crtc_y, state->crtc_w,
		state->crtc_h, !q16_data);

	SDE_DEBUG_PLANE(psde, "check %d -> %d\n",
		sde_plane_enabled(plane->state), sde_plane_enabled(state));

	if (!sde_plane_enabled(state))
		goto modeset_update;

	fb = state->fb;
	width = fb ? state->fb->width : 0x0;
	height = fb ? state->fb->height : 0x0;

	SDE_DEBUG("plane%d sspp:%x/%dx%d/%4.4s/%llx\n",
			plane->base.id,
			pstate->rotation,
			width, height,
			fb ? (char *) &state->fb->format->format : 0x0,
			fb ? state->fb->modifier : 0x0);
	SDE_DEBUG("src:%dx%d %d,%d crtc:%dx%d+%d+%d\n",
			state->src_w >> 16, state->src_h >> 16,
			state->src_x >> 16, state->src_y >> 16,
			state->crtc_w, state->crtc_h,
			state->crtc_x, state->crtc_y);

	msm_fmt = msm_framebuffer_format(fb);
	fmt = to_sde_format(msm_fmt);

	min_src_size = SDE_FORMAT_IS_YUV(fmt) ? 2 : 1;

	if (SDE_FORMAT_IS_YUV(fmt) &&
		(!(psde->features & SDE_SSPP_SCALER) ||
		 !(psde->features & (BIT(SDE_SSPP_CSC)
		 | BIT(SDE_SSPP_CSC_10BIT))))) {
		SDE_ERROR_PLANE(psde,
				"plane doesn't have scaler/csc for yuv\n");
		ret = -EINVAL;

	/* check src bounds */
	} else if (width > MAX_IMG_WIDTH ||
		height > MAX_IMG_HEIGHT ||
		src.w < min_src_size || src.h < min_src_size ||
		CHECK_LAYER_BOUNDS(src.x, src.w, width) ||
		CHECK_LAYER_BOUNDS(src.y, src.h, height)) {
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
	} else if (SDE_FORMAT_IS_UBWC(fmt) &&
		!psde->catalog->ubwc_version) {
		SDE_ERROR_PLANE(psde, "ubwc not supported\n");
		ret = -EINVAL;
	}


	if (ret)
		return ret;

	ret = _sde_atomic_check_decimation_scaler(state, psde, fmt, pstate,
		&src, &dst, width, height);

	if (ret)
		return ret;

	ret = _sde_atomic_check_excl_rect(psde, pstate,
		&src, fmt, ret);

	if (ret)
		return ret;

	ret = _sde_plane_validate_shared_crtc(psde, state);

	if (ret)
		return ret;

	pstate->const_alpha_en = fmt->alpha_enable &&
		(SDE_DRM_BLEND_OP_OPAQUE !=
		 sde_plane_get_property(pstate, PLANE_PROP_BLEND_OP)) &&
		(pstate->stage != SDE_STAGE_0);

modeset_update:
	if (!ret)
		_sde_plane_sspp_atomic_check_mode_changed(psde,
				state, plane->state);
	return ret;
}

static int sde_plane_atomic_check(struct drm_plane *plane,
		struct drm_plane_state *state)
{
	int ret = 0;
	struct sde_plane *psde;
	struct sde_plane_state *pstate;

	if (!plane || !state) {
		SDE_ERROR("invalid arg(s), plane %d state %d\n",
				!plane, !state);
		ret = -EINVAL;
		goto exit;
	}

	psde = to_sde_plane(plane);
	pstate = to_sde_plane_state(state);

	SDE_DEBUG_PLANE(psde, "\n");

	ret = sde_plane_rot_atomic_check(plane, state);
	if (ret)
		goto exit;

	ret = sde_plane_sspp_atomic_check(plane, state);

exit:
	return ret;
}

void sde_plane_flush(struct drm_plane *plane)
{
	struct sde_plane *psde;
	struct sde_plane_state *pstate;

	if (!plane || !plane->state) {
		SDE_ERROR("invalid plane\n");
		return;
	}

	psde = to_sde_plane(plane);
	pstate = to_sde_plane_state(plane->state);

	/*
	 * These updates have to be done immediately before the plane flush
	 * timing, and may not be moved to the atomic_update/mode_set functions.
	 */
	if (psde->is_error)
		/* force white frame with 100% alpha pipe output on error */
		_sde_plane_color_fill(psde, 0xFFFFFF, 0xFF);
	else if (psde->color_fill & SDE_PLANE_COLOR_FILL_FLAG)
		/* force 100% alpha */
		_sde_plane_color_fill(psde, psde->color_fill, 0xFF);
	else if (psde->pipe_hw && psde->csc_ptr && psde->pipe_hw->ops.setup_csc)
		psde->pipe_hw->ops.setup_csc(psde->pipe_hw, psde->csc_ptr);

	/* flag h/w flush complete */
	if (plane->state)
		pstate->pending = false;
}

/**
 * sde_plane_set_error: enable/disable error condition
 * @plane: pointer to drm_plane structure
 */
void sde_plane_set_error(struct drm_plane *plane, bool error)
{
	struct sde_plane *psde;

	if (!plane)
		return;

	psde = to_sde_plane(plane);
	psde->is_error = error;
}

static void _sde_plane_sspp_setup_sys_cache(struct sde_plane *psde,
	struct sde_plane_state *pstate, const struct sde_format *fmt)
{
	if (!psde->pipe_hw->ops.setup_sys_cache ||
	    !(psde->perf_features & BIT(SDE_PERF_SSPP_SYS_CACHE)))
		return;

	SDE_DEBUG("features:0x%x rotation:0x%x\n",
		psde->features, pstate->rotation);

	if ((pstate->rotation & DRM_MODE_ROTATE_90) &&
			sde_format_is_tp10_ubwc(fmt)) {
		pstate->sc_cfg.rd_en = true;
		pstate->sc_cfg.rd_scid =
			psde->pipe_sblk->llcc_scid;
		pstate->sc_cfg.flags = SSPP_SYS_CACHE_EN_FLAG |
			SSPP_SYS_CACHE_SCID;
	} else {
		pstate->sc_cfg.rd_en = false;
		pstate->sc_cfg.rd_scid = 0x0;
		pstate->sc_cfg.flags = SSPP_SYS_CACHE_EN_FLAG |
			SSPP_SYS_CACHE_SCID;
	}

	psde->pipe_hw->ops.setup_sys_cache(
		psde->pipe_hw, &pstate->sc_cfg);
}

static void _sde_plane_map_prop_to_dirty_bits(void)
{
	plane_prop_array[PLANE_PROP_SCALER_V1] =
	plane_prop_array[PLANE_PROP_SCALER_V2] =
	plane_prop_array[PLANE_PROP_SCALER_LUT_ED] =
	plane_prop_array[PLANE_PROP_SCALER_LUT_CIR] =
	plane_prop_array[PLANE_PROP_SCALER_LUT_SEP] =
	plane_prop_array[PLANE_PROP_H_DECIMATE] =
	plane_prop_array[PLANE_PROP_V_DECIMATE] =
	plane_prop_array[PLANE_PROP_SRC_CONFIG] =
	plane_prop_array[PLANE_PROP_ZPOS] =
	plane_prop_array[PLANE_PROP_EXCL_RECT_V1] =
		SDE_PLANE_DIRTY_RECTS;

	plane_prop_array[PLANE_PROP_CSC_V1] =
	plane_prop_array[PLANE_PROP_CSC_DMA_V1] =
	plane_prop_array[PLANE_PROP_INVERSE_PMA] =
		SDE_PLANE_DIRTY_FORMAT;

	plane_prop_array[PLANE_PROP_MULTIRECT_MODE] =
	plane_prop_array[PLANE_PROP_COLOR_FILL] =
		SDE_PLANE_DIRTY_ALL;

	/* no special action required */
	plane_prop_array[PLANE_PROP_INFO] =
	plane_prop_array[PLANE_PROP_ALPHA] =
	plane_prop_array[PLANE_PROP_INPUT_FENCE] =
	plane_prop_array[PLANE_PROP_BLEND_OP] = 0;

	plane_prop_array[PLANE_PROP_FB_TRANSLATION_MODE] =
		SDE_PLANE_DIRTY_FB_TRANSLATION_MODE;
	plane_prop_array[PLANE_PROP_PREFILL_SIZE] =
	plane_prop_array[PLANE_PROP_PREFILL_TIME] =
		SDE_PLANE_DIRTY_PERF;

	plane_prop_array[PLANE_PROP_VIG_GAMUT] = SDE_PLANE_DIRTY_VIG_GAMUT;
	plane_prop_array[PLANE_PROP_VIG_IGC] = SDE_PLANE_DIRTY_VIG_IGC;
	plane_prop_array[PLANE_PROP_DMA_IGC] = SDE_PLANE_DIRTY_DMA_IGC;
	plane_prop_array[PLANE_PROP_DMA_GC] = SDE_PLANE_DIRTY_DMA_GC;

	plane_prop_array[PLANE_PROP_SKIN_COLOR] =
	plane_prop_array[PLANE_PROP_SKY_COLOR] =
	plane_prop_array[PLANE_PROP_FOLIAGE_COLOR] =
	plane_prop_array[PLANE_PROP_HUE_ADJUST] =
	plane_prop_array[PLANE_PROP_SATURATION_ADJUST] =
	plane_prop_array[PLANE_PROP_VALUE_ADJUST] =
	plane_prop_array[PLANE_PROP_CONTRAST_ADJUST] =
		SDE_PLANE_DIRTY_ALL;
}

static inline bool _sde_plane_allow_uidle(struct sde_plane *psde,
	struct sde_rect *src, struct sde_rect *dst)
{
	u32 max_downscale = psde->catalog->uidle_cfg.max_dwnscale;
	u32 downscale = (src->h * 1000)/dst->h;

	return (downscale > max_downscale) ? false : true;
}

static void _sde_plane_setup_uidle(struct drm_crtc *crtc,
	struct sde_plane *psde, struct sde_plane_state *pstate,
	struct sde_rect *src, struct sde_rect *dst)
{
	struct sde_hw_pipe_uidle_cfg cfg;
	struct sde_crtc *sde_crtc = to_sde_crtc(crtc);

	u32 line_time = sde_get_linetime(&crtc->mode,
			sde_crtc->comp_ratio); /* nS */
	u32 fal1_target_idle_time_ns =
		psde->catalog->uidle_cfg.fal1_target_idle_time * 1000; /* nS */
	u32 fal10_target_idle_time_ns =
		psde->catalog->uidle_cfg.fal10_target_idle_time * 1000; /* nS */
	u32 fal10_threshold =
		psde->catalog->uidle_cfg.fal10_threshold; /* uS */

	if (line_time && fal10_threshold && fal10_target_idle_time_ns &&
		fal1_target_idle_time_ns) {
		cfg.enable = _sde_plane_allow_uidle(psde, src, dst);
		cfg.fal10_threshold = fal10_threshold;
		cfg.fal10_exit_threshold = fal10_threshold + 2;
		cfg.fal1_threshold = 1 +
			(fal1_target_idle_time_ns*1000/line_time*2)/1000;
		cfg.fal_allowed_threshold = fal10_threshold +
			(fal10_target_idle_time_ns*1000/line_time*2)/1000;
	} else {
		SDE_ERROR("invalid settings, will disable UIDLE %d %d %d %d\n",
			line_time, fal10_threshold, fal10_target_idle_time_ns,
			fal1_target_idle_time_ns);
		memset(&cfg, 0, sizeof(struct sde_hw_pipe_uidle_cfg));
	}

	SDE_DEBUG_PLANE(psde,
		"tholds: fal10=%d fal10_exit=%d fal1=%d fal_allowed=%d\n",
			cfg.fal10_threshold, cfg.fal10_exit_threshold,
			cfg.fal1_threshold, cfg.fal_allowed_threshold);
	SDE_DEBUG_PLANE(psde,
		"times: line:%d fal1_idle:%d fal10_idle:%d dwnscale:%d\n",
			line_time, fal1_target_idle_time_ns,
			fal10_target_idle_time_ns,
			psde->catalog->uidle_cfg.max_dwnscale);
	SDE_EVT32_VERBOSE(cfg.enable,
		cfg.fal10_threshold, cfg.fal10_exit_threshold,
		cfg.fal1_threshold, cfg.fal_allowed_threshold,
		psde->catalog->uidle_cfg.max_dwnscale);

	psde->pipe_hw->ops.setup_uidle(
		psde->pipe_hw, &cfg,
		pstate->multirect_index);
}

static void _sde_plane_update_secure_session(struct sde_plane *psde,
	struct sde_plane_state *pstate)
{
	bool enable = false;
	int mode = sde_plane_get_property(pstate,
			PLANE_PROP_FB_TRANSLATION_MODE);

	if ((mode == SDE_DRM_FB_SEC) ||
			(mode == SDE_DRM_FB_SEC_DIR_TRANS))
		enable = true;

	/* update secure session flag */
	psde->pipe_hw->ops.setup_secure_address(psde->pipe_hw,
			pstate->multirect_index,
			enable);
}

static void _sde_plane_update_roi_config(struct drm_plane *plane,
	struct drm_crtc *crtc, struct drm_framebuffer *fb)
{
	const struct sde_format *fmt;
	const struct msm_format *msm_fmt;
	struct sde_plane *psde;
	struct drm_plane_state *state;
	struct sde_plane_state *pstate;
	struct sde_rect src, dst;
	const struct sde_rect *crtc_roi;
	bool q16_data = true;
	int idx;

	psde = to_sde_plane(plane);
	state = plane->state;

	pstate = to_sde_plane_state(state);

	msm_fmt = msm_framebuffer_format(fb);
	if (!msm_fmt) {
		SDE_ERROR("crtc%d plane%d: null format\n",
			DRMID(crtc), DRMID(plane));
		return;
	}

	fmt = to_sde_format(msm_fmt);

	POPULATE_RECT(&src, state->src_x, state->src_y,
		state->src_w, state->src_h, q16_data);
	POPULATE_RECT(&dst, state->crtc_x, state->crtc_y,
		state->crtc_w, state->crtc_h, !q16_data);

	SDE_DEBUG_PLANE(psde,
		"FB[%u] %u,%u,%ux%u->crtc%u %d,%d,%ux%u, %4.4s ubwc %d\n",
			fb->base.id, src.x, src.y, src.w, src.h,
			crtc->base.id, dst.x, dst.y, dst.w, dst.h,
			(char *)&fmt->base.pixel_format,
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

	/*
	 * adjust layer mixer position of the sspp in the presence
	 * of a partial update to the active lm origin
	 */
	sde_crtc_get_crtc_roi(crtc->state, &crtc_roi);
	dst.x -= crtc_roi->x;
	dst.y -= crtc_roi->y;

	/* check for UIDLE */
	if (psde->pipe_hw->ops.setup_uidle)
		_sde_plane_setup_uidle(crtc, psde, pstate, &src, &dst);

	psde->pipe_cfg.src_rect = src;
	psde->pipe_cfg.dst_rect = dst;

	_sde_plane_setup_scaler(psde, pstate, fmt, false);

	/* check for color fill */
	psde->color_fill = (uint32_t)sde_plane_get_property(pstate,
			PLANE_PROP_COLOR_FILL);
	if (psde->color_fill & SDE_PLANE_COLOR_FILL_FLAG) {
		/* skip remaining processing on color fill */
		pstate->dirty = 0x0;
	} else if (psde->pipe_hw->ops.setup_rects) {
		psde->pipe_hw->ops.setup_rects(psde->pipe_hw,
				&psde->pipe_cfg,
				pstate->multirect_index);
	}

	if (psde->pipe_hw->ops.setup_pe &&
			(pstate->multirect_index != SDE_SSPP_RECT_1))
		psde->pipe_hw->ops.setup_pe(psde->pipe_hw,
				&pstate->pixel_ext);

	/**
	 * when programmed in multirect mode, scalar block will be
	 * bypassed. Still we need to update alpha and bitwidth
	 * ONLY for RECT0
	 */
	if (psde->pipe_hw->ops.setup_scaler &&
			pstate->multirect_index != SDE_SSPP_RECT_1) {
		psde->pipe_hw->ctl = _sde_plane_get_hw_ctl(plane);
		psde->pipe_hw->ops.setup_scaler(psde->pipe_hw,
				&psde->pipe_cfg, &pstate->pixel_ext,
				&pstate->scaler3_cfg);
	}

	/* update excl rect */
	if (psde->pipe_hw->ops.setup_excl_rect)
		psde->pipe_hw->ops.setup_excl_rect(psde->pipe_hw,
				&pstate->excl_rect,
				pstate->multirect_index);

	if (psde->pipe_hw->ops.setup_multirect)
		psde->pipe_hw->ops.setup_multirect(
				psde->pipe_hw,
				pstate->multirect_index,
				pstate->multirect_mode);
}

static void _sde_plane_update_format_and_rects(struct sde_plane *psde,
	struct sde_plane_state *pstate, const struct sde_format *fmt)
{
	uint32_t src_flags = 0;

	SDE_DEBUG_PLANE(psde, "rotation 0x%X\n", pstate->rotation);
	if (pstate->rotation & DRM_MODE_REFLECT_X)
		src_flags |= SDE_SSPP_FLIP_LR;
	if (pstate->rotation & DRM_MODE_REFLECT_Y)
		src_flags |= SDE_SSPP_FLIP_UD;
	if (pstate->rotation & DRM_MODE_ROTATE_90)
		src_flags |= SDE_SSPP_ROT_90;

	/* update format */
	psde->pipe_hw->ops.setup_format(psde->pipe_hw, fmt,
	   pstate->const_alpha_en, src_flags,
	   pstate->multirect_index);

	if (psde->pipe_hw->ops.setup_cdp) {
		struct sde_hw_pipe_cdp_cfg *cdp_cfg = &pstate->cdp_cfg;

		memset(cdp_cfg, 0, sizeof(struct sde_hw_pipe_cdp_cfg));

		cdp_cfg->enable = psde->catalog->perf.cdp_cfg
			   [SDE_PERF_CDP_USAGE_RT].rd_enable;
		cdp_cfg->ubwc_meta_enable =
			   SDE_FORMAT_IS_UBWC(fmt);
		cdp_cfg->tile_amortize_enable =
			   SDE_FORMAT_IS_UBWC(fmt) ||
			   SDE_FORMAT_IS_TILE(fmt);
		cdp_cfg->preload_ahead = SDE_WB_CDP_PRELOAD_AHEAD_64;

		psde->pipe_hw->ops.setup_cdp(psde->pipe_hw, cdp_cfg,
			   pstate->multirect_index);
	}

	_sde_plane_sspp_setup_sys_cache(psde, pstate, fmt);

	/* update csc */
	if (SDE_FORMAT_IS_YUV(fmt))
		_sde_plane_setup_csc(psde);
	else
		psde->csc_ptr = 0;

	if (psde->pipe_hw->ops.setup_inverse_pma) {
		uint32_t pma_mode = 0;

		if (fmt->alpha_enable)
			pma_mode = (uint32_t) sde_plane_get_property(
				pstate, PLANE_PROP_INVERSE_PMA);
		psde->pipe_hw->ops.setup_inverse_pma(psde->pipe_hw,
			pstate->multirect_index, pma_mode);
	}

	if (psde->pipe_hw->ops.setup_dgm_csc)
		psde->pipe_hw->ops.setup_dgm_csc(psde->pipe_hw,
			pstate->multirect_index, psde->csc_usr_ptr);
}

static void _sde_plane_update_sharpening(struct sde_plane *psde)
{
	psde->sharp_cfg.strength = SHARP_STRENGTH_DEFAULT;
	psde->sharp_cfg.edge_thr = SHARP_EDGE_THR_DEFAULT;
	psde->sharp_cfg.smooth_thr = SHARP_SMOOTH_THR_DEFAULT;
	psde->sharp_cfg.noise_thr = SHARP_NOISE_THR_DEFAULT;

	psde->pipe_hw->ops.setup_sharpening(psde->pipe_hw,
			&psde->sharp_cfg);
}

static void _sde_plane_update_properties(struct drm_plane *plane,
	struct drm_crtc *crtc, struct drm_framebuffer *fb)
{
	uint32_t nplanes;
	const struct msm_format *msm_fmt;
	const struct sde_format *fmt;
	struct sde_plane *psde;
	struct drm_plane_state *state;
	struct sde_plane_state *pstate;

	psde = to_sde_plane(plane);
	state = plane->state;

	pstate = to_sde_plane_state(state);
	if (!pstate) {
		SDE_ERROR("invalid plane state for plane%d\n", DRMID(plane));
		return;
	}

	msm_fmt = msm_framebuffer_format(fb);
	if (!msm_fmt) {
		SDE_ERROR("crtc%d plane%d: null format\n",
			DRMID(crtc), DRMID(plane));
		return;
	}

	fmt = to_sde_format(msm_fmt);
	nplanes = fmt->num_planes;

	/* update secure session flag */
	if (pstate->dirty & SDE_PLANE_DIRTY_FB_TRANSLATION_MODE)
		_sde_plane_update_secure_session(psde, pstate);

	/* update roi config */
	if (pstate->dirty & SDE_PLANE_DIRTY_RECTS)
		_sde_plane_update_roi_config(plane, crtc, fb);

	if ((pstate->dirty & SDE_PLANE_DIRTY_FORMAT ||
			pstate->dirty & SDE_PLANE_DIRTY_RECTS) &&
			psde->pipe_hw->ops.setup_format)
		_sde_plane_update_format_and_rects(psde, pstate, fmt);

	sde_color_process_plane_setup(plane);

	/* update sharpening */
	if ((pstate->dirty & SDE_PLANE_DIRTY_SHARPEN) &&
		psde->pipe_hw->ops.setup_sharpening)
		_sde_plane_update_sharpening(psde);

	_sde_plane_set_qos_lut(plane, crtc, fb);

	if (plane->type != DRM_PLANE_TYPE_CURSOR) {
		_sde_plane_set_qos_ctrl(plane, true, SDE_PLANE_QOS_PANIC_CTRL);
		_sde_plane_set_ot_limit(plane, crtc);
		if (pstate->dirty & SDE_PLANE_DIRTY_PERF)
			_sde_plane_set_ts_prefill(plane, pstate);
	}

	if ((pstate->dirty & SDE_PLANE_DIRTY_ALL) == SDE_PLANE_DIRTY_ALL)
		_sde_plane_set_qos_remap(plane, true);
	else
		_sde_plane_set_qos_remap(plane, false);

	/* clear dirty */
	pstate->dirty = 0x0;
}

static int sde_plane_sspp_atomic_update(struct drm_plane *plane,
				struct drm_plane_state *old_state)
{
	struct sde_plane *psde;
	struct drm_plane_state *state;
	struct sde_plane_state *pstate;
	struct sde_plane_state *old_pstate;
	struct drm_crtc *crtc;
	struct drm_framebuffer *fb;
	int idx;
	int dirty_prop_flag;

	if (!plane) {
		SDE_ERROR("invalid plane\n");
		return -EINVAL;
	} else if (!plane->state) {
		SDE_ERROR("invalid plane state\n");
		return -EINVAL;
	} else if (!old_state) {
		SDE_ERROR("invalid old state\n");
		return -EINVAL;
	}

	psde = to_sde_plane(plane);
	state = plane->state;

	pstate = to_sde_plane_state(state);

	old_pstate = to_sde_plane_state(old_state);

	crtc = state->crtc;
	fb = state->fb;
	if (!crtc || !fb) {
		SDE_ERROR_PLANE(psde, "invalid crtc %d or fb %d\n",
				!crtc, !fb);
		return -EINVAL;
	}

	SDE_DEBUG(
		"plane%d sspp:%dx%d/%4.4s/%llx/%dx%d+%d+%d/%x crtc:%dx%d+%d+%d\n",
			plane->base.id,
			state->fb->width, state->fb->height,
			(char *) &state->fb->format->format,
			state->fb->modifier,
			state->src_w >> 16, state->src_h >> 16,
			state->src_x >> 16, state->src_y >> 16,
			pstate->rotation,
			state->crtc_w, state->crtc_h,
			state->crtc_x, state->crtc_y);

	/* force reprogramming of all the parameters, if the flag is set */
	if (psde->revalidate) {
		SDE_DEBUG("plane:%d - reconfigure all the parameters\n",
				plane->base.id);
		pstate->dirty = SDE_PLANE_DIRTY_ALL | SDE_PLANE_DIRTY_CP;
		psde->revalidate = false;
	}

	/* determine what needs to be refreshed */
	mutex_lock(&psde->property_info.property_lock);
	while ((idx = msm_property_pop_dirty(&psde->property_info,
				&pstate->property_state)) >= 0) {
		dirty_prop_flag = plane_prop_array[idx];
		pstate->dirty |= dirty_prop_flag;
	}
	mutex_unlock(&psde->property_info.property_lock);

	/**
	 * since plane_atomic_check is invoked before crtc_atomic_check
	 * in the commit sequence, all the parameters for updating the
	 * plane dirty flag will not be available during
	 * plane_atomic_check as some features params are updated
	 * in crtc_atomic_check (eg.:sDMA). So check for mode_change
	 * before sspp update.
	 */
	_sde_plane_sspp_atomic_check_mode_changed(psde, state,
								old_state);

	/* re-program the output rects always if partial update roi changed */
	if (sde_crtc_is_crtc_roi_dirty(crtc->state))
		pstate->dirty |= SDE_PLANE_DIRTY_RECTS;

	if (pstate->dirty & SDE_PLANE_DIRTY_RECTS)
		memset(&(psde->pipe_cfg), 0, sizeof(struct sde_hw_pipe_cfg));

	_sde_plane_set_scanout(plane, pstate, &psde->pipe_cfg, fb);

	/* early out if nothing dirty */
	if (!pstate->dirty)
		return 0;
	pstate->pending = true;

	psde->is_rt_pipe = sde_crtc_is_rt_client(crtc, crtc->state);
	_sde_plane_set_qos_ctrl(plane, false, SDE_PLANE_QOS_PANIC_CTRL);

	_sde_plane_update_properties(plane, crtc, fb);

	return 0;
}

static void _sde_plane_atomic_disable(struct drm_plane *plane,
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
	} else if (!old_state) {
		SDE_ERROR("invalid old state\n");
		return;
	}

	psde = to_sde_plane(plane);
	state = plane->state;
	pstate = to_sde_plane_state(state);

	SDE_EVT32(DRMID(plane), is_sde_plane_virtual(plane),
			pstate->multirect_mode);

	pstate->pending = true;

	if (is_sde_plane_virtual(plane) &&
			psde->pipe_hw && psde->pipe_hw->ops.setup_multirect)
		psde->pipe_hw->ops.setup_multirect(psde->pipe_hw,
				SDE_SSPP_RECT_SOLO, SDE_SSPP_MULTIRECT_NONE);
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
	state = plane->state;
	pstate = to_sde_plane_state(state);

	if (psde->is_error && !(msm_property_is_dirty(&psde->property_info,
		&pstate->property_state, PLANE_PROP_SCALER_V2)))
		pstate->scaler_check_state = SDE_PLANE_SCLCHECK_INVALID;

	psde->is_error = false;
	SDE_DEBUG_PLANE(psde, "\n");

	if (!sde_plane_enabled(state)) {
		_sde_plane_atomic_disable(plane, old_state);
	} else {
		int ret;

		ret = sde_plane_sspp_atomic_update(plane, old_state);
		/* atomic_check should have ensured that this doesn't fail */
		WARN_ON(ret < 0);
	}
}

void sde_plane_restore(struct drm_plane *plane)
{
	struct sde_plane *psde;

	if (!plane || !plane->state) {
		SDE_ERROR("invalid plane\n");
		return;
	}

	psde = to_sde_plane(plane);

	/*
	 * Revalidate is only true here if idle PC occurred and
	 * there is no plane state update in current commit cycle.
	 */
	if (!psde->revalidate)
		return;

	SDE_DEBUG_PLANE(psde, "\n");

	/* last plane state is same as current state */
	sde_plane_atomic_update(plane, plane->state);
}

bool sde_plane_is_cache_required(struct drm_plane *plane)
{
	struct sde_plane_state *pstate;

	if (!plane || !plane->state) {
		SDE_ERROR("invalid plane\n");
		return false;
	}

	pstate = to_sde_plane_state(plane->state);

	/* check if llcc is required for the plane */
	if (pstate->sc_cfg.rd_en)
		return true;
	else
		return false;
}

static void _sde_plane_install_non_master_properties(struct sde_plane *psde)
{
	char feature_name[256];

	if (psde->pipe_sblk->maxhdeciexp) {
		msm_property_install_range(&psde->property_info,
				"h_decimate", 0x0, 0,
				psde->pipe_sblk->maxhdeciexp, 0,
				PLANE_PROP_H_DECIMATE);
	}

	if (psde->pipe_sblk->maxvdeciexp) {
		msm_property_install_range(&psde->property_info,
				"v_decimate", 0x0, 0,
				psde->pipe_sblk->maxvdeciexp, 0,
				PLANE_PROP_V_DECIMATE);
	}

	if (psde->features & BIT(SDE_SSPP_SCALER_QSEED3)) {
		msm_property_install_range(
				&psde->property_info, "scaler_v2",
				0x0, 0, ~0, 0, PLANE_PROP_SCALER_V2);
		msm_property_install_blob(&psde->property_info,
				"lut_ed", 0, PLANE_PROP_SCALER_LUT_ED);
		msm_property_install_blob(&psde->property_info,
				"lut_cir", 0,
				PLANE_PROP_SCALER_LUT_CIR);
		msm_property_install_blob(&psde->property_info,
				"lut_sep", 0,
				PLANE_PROP_SCALER_LUT_SEP);
	} else if (psde->features & BIT(SDE_SSPP_SCALER_QSEED3LITE)) {
		msm_property_install_range(
				&psde->property_info, "scaler_v2",
				0x0, 0, ~0, 0, PLANE_PROP_SCALER_V2);
		msm_property_install_blob(&psde->property_info,
				"lut_sep", 0,
				PLANE_PROP_SCALER_LUT_SEP);
	} else if (psde->features & SDE_SSPP_SCALER) {
		msm_property_install_range(
				&psde->property_info, "scaler_v1", 0x0,
				0, ~0, 0, PLANE_PROP_SCALER_V1);
	}

	if (psde->features & BIT(SDE_SSPP_CSC) ||
		psde->features & BIT(SDE_SSPP_CSC_10BIT))
		msm_property_install_volatile_range(
				&psde->property_info, "csc_v1", 0x0,
				0, ~0, 0, PLANE_PROP_CSC_V1);

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

}

/* helper to install properties which are common to planes and crtcs */
static void _sde_plane_install_properties(struct drm_plane *plane,
	struct sde_mdss_cfg *catalog, u32 master_plane_id)
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
	static const struct drm_prop_enum_list e_fb_translation_mode[] = {
		{SDE_DRM_FB_NON_SEC, "non_sec"},
		{SDE_DRM_FB_SEC, "sec"},
		{SDE_DRM_FB_NON_SEC_DIR_TRANS, "non_sec_direct_translation"},
		{SDE_DRM_FB_SEC_DIR_TRANS, "sec_direct_translation"},
	};
	static const struct drm_prop_enum_list e_multirect_mode[] = {
		{SDE_SSPP_MULTIRECT_NONE, "none"},
		{SDE_SSPP_MULTIRECT_PARALLEL, "parallel"},
		{SDE_SSPP_MULTIRECT_TIME_MX,  "serial"},
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
				!psde->pipe_hw, !psde->pipe_sblk);
		return;
	} else if (!catalog) {
		SDE_ERROR("invalid catalog\n");
		return;
	}

	psde->catalog = catalog;

	if (sde_is_custom_client()) {
		if (catalog->mixer_count &&
				catalog->mixer[0].sblk->maxblendstages) {
			zpos_max = catalog->mixer[0].sblk->maxblendstages - 1;
			if (catalog->has_base_layer &&
					(zpos_max > SDE_STAGE_MAX - 1))
				zpos_max = SDE_STAGE_MAX - 1;
			else if (zpos_max > SDE_STAGE_MAX - SDE_STAGE_0 - 1)
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

	if (!master_plane_id)
		_sde_plane_install_non_master_properties(psde);

	if (psde->features & BIT(SDE_SSPP_EXCL_RECT))
		msm_property_install_volatile_range(&psde->property_info,
			"excl_rect_v1", 0x0, 0, ~0, 0, PLANE_PROP_EXCL_RECT_V1);

	sde_plane_rot_install_properties(plane, catalog);

	msm_property_install_enum(&psde->property_info, "blend_op", 0x0, 0,
		e_blend_op, ARRAY_SIZE(e_blend_op), PLANE_PROP_BLEND_OP);

	msm_property_install_enum(&psde->property_info, "src_config", 0x0, 1,
		e_src_config, ARRAY_SIZE(e_src_config), PLANE_PROP_SRC_CONFIG);

	if (psde->pipe_hw->ops.setup_solidfill)
		msm_property_install_range(&psde->property_info, "color_fill",
				0, 0, 0xFFFFFFFF, 0, PLANE_PROP_COLOR_FILL);

	msm_property_install_range(&psde->property_info,
			"prefill_size", 0x0, 0, ~0, 0,
			PLANE_PROP_PREFILL_SIZE);
	msm_property_install_range(&psde->property_info,
			"prefill_time", 0x0, 0, ~0, 0,
			PLANE_PROP_PREFILL_TIME);

	info = kzalloc(sizeof(struct sde_kms_info), GFP_KERNEL);
	if (!info) {
		SDE_ERROR("failed to allocate info memory\n");
		return;
	}

	msm_property_install_blob(&psde->property_info, "capabilities",
		DRM_MODE_PROP_IMMUTABLE, PLANE_PROP_INFO);
	sde_kms_info_reset(info);

	if (!master_plane_id) {
		format_list = psde->pipe_sblk->format_list;
	} else {
		format_list = psde->pipe_sblk->virt_format_list;
		sde_kms_info_add_keyint(info, "primary_smart_plane_id",
						master_plane_id);
		msm_property_install_enum(&psde->property_info,
			"multirect_mode", 0x0, 0, e_multirect_mode,
			ARRAY_SIZE(e_multirect_mode),
			PLANE_PROP_MULTIRECT_MODE);
	}

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

	if (psde->pipe_hw && psde->pipe_hw->ops.get_scaler_ver)
		sde_kms_info_add_keyint(info, "scaler_step_ver",
			psde->pipe_hw->ops.get_scaler_ver(psde->pipe_hw));

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
	sde_kms_info_add_keyint(info, "max_per_pipe_bw",
			psde->pipe_sblk->max_per_pipe_bw * 1000LL);
	sde_kms_info_add_keyint(info, "max_per_pipe_bw_high",
			psde->pipe_sblk->max_per_pipe_bw_high * 1000LL);

	if ((!master_plane_id &&
		(psde->features & BIT(SDE_SSPP_INVERSE_PMA))) ||
		(psde->features & BIT(SDE_SSPP_DGM_INVERSE_PMA))) {
		msm_property_install_range(&psde->property_info,
			"inverse_pma", 0x0, 0, 1, 0, PLANE_PROP_INVERSE_PMA);
		sde_kms_info_add_keyint(info, "inverse_pma", 1);
	}

	if (psde->features & BIT(SDE_SSPP_DGM_CSC)) {
		msm_property_install_volatile_range(
			&psde->property_info, "csc_dma_v1", 0x0,
			0, ~0, 0, PLANE_PROP_CSC_DMA_V1);
		sde_kms_info_add_keyint(info, "csc_dma_v1", 1);
	}

	if (psde->features & BIT(SDE_SSPP_SEC_UI_ALLOWED))
		sde_kms_info_add_keyint(info, "sec_ui_allowed", 1);
	if (psde->features & BIT(SDE_SSPP_BLOCK_SEC_UI))
		sde_kms_info_add_keyint(info, "block_sec_ui", 1);

	if (psde->features & BIT(SDE_SSPP_TRUE_INLINE_ROT)) {
		const struct sde_format_extended *inline_rot_fmt_list;

		sde_kms_info_add_keyint(info, "true_inline_rot_rev",
			 catalog->true_inline_rot_rev);
		sde_kms_info_add_keyint(info,
			"true_inline_dwnscale_rt",
			(int) (psde->pipe_sblk->in_rot_maxdwnscale_rt_num /
				psde->pipe_sblk->in_rot_maxdwnscale_rt_denom));
		sde_kms_info_add_keyint(info,
				"true_inline_dwnscale_rt_numerator",
				psde->pipe_sblk->in_rot_maxdwnscale_rt_num);
		sde_kms_info_add_keyint(info,
				"true_inline_dwnscale_rt_denominator",
				psde->pipe_sblk->in_rot_maxdwnscale_rt_denom);
		sde_kms_info_add_keyint(info, "true_inline_dwnscale_nrt",
			psde->pipe_sblk->in_rot_maxdwnscale_nrt);
		sde_kms_info_add_keyint(info, "true_inline_max_height",
			psde->pipe_sblk->in_rot_maxheight);

		inline_rot_fmt_list = psde->pipe_sblk->in_rot_format_list;

		if (inline_rot_fmt_list) {
			sde_kms_info_start(info, "inline_rot_pixel_formats");
			while (inline_rot_fmt_list->fourcc_format) {
				sde_kms_info_append_format(info,
					inline_rot_fmt_list->fourcc_format,
					inline_rot_fmt_list->modifier);
				++inline_rot_fmt_list;
			}
			sde_kms_info_stop(info);
		}

	}

	msm_property_set_blob(&psde->property_info, &psde->blob_info,
			info->data, SDE_KMS_INFO_DATALEN(info),
			PLANE_PROP_INFO);

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

	if (psde->features & BIT(SDE_SSPP_VIG_GAMUT)) {
		snprintf(feature_name, sizeof(feature_name), "%s%d",
			"SDE_VIG_3D_LUT_GAMUT_V",
			psde->pipe_sblk->gamut_blk.version >> 16);
		msm_property_install_blob(&psde->property_info, feature_name, 0,
			PLANE_PROP_VIG_GAMUT);
	}

	if (psde->features & BIT(SDE_SSPP_VIG_IGC)) {
		snprintf(feature_name, sizeof(feature_name), "%s%d",
			"SDE_VIG_1D_LUT_IGC_V",
			psde->pipe_sblk->igc_blk[0].version >> 16);
		msm_property_install_blob(&psde->property_info, feature_name, 0,
			PLANE_PROP_VIG_IGC);
	}

	if (psde->features & BIT(SDE_SSPP_DMA_IGC)) {
		snprintf(feature_name, sizeof(feature_name), "%s%d",
			"SDE_DGM_1D_LUT_IGC_V",
			psde->pipe_sblk->igc_blk[0].version >> 16);
		msm_property_install_blob(&psde->property_info, feature_name, 0,
			PLANE_PROP_DMA_IGC);
	}

	if (psde->features & BIT(SDE_SSPP_DMA_GC)) {
		snprintf(feature_name, sizeof(feature_name), "%s%d",
			"SDE_DGM_1D_LUT_GC_V",
			psde->pipe_sblk->gc_blk[0].version >> 16);
		msm_property_install_blob(&psde->property_info, feature_name, 0,
			PLANE_PROP_DMA_GC);
	}

	msm_property_install_enum(&psde->property_info, "fb_translation_mode",
			0x0,
			0, e_fb_translation_mode,
			ARRAY_SIZE(e_fb_translation_mode),
			PLANE_PROP_FB_TRANSLATION_MODE);
}

static inline void _sde_plane_set_csc_v1(struct sde_plane *psde,
		void __user *usr_ptr)
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

static inline void _sde_plane_set_scaler_v1(struct sde_plane *psde,
		struct sde_plane_state *pstate, void __user *usr)
{
	struct sde_drm_scaler_v1 scale_v1;
	struct sde_hw_pixel_ext *pe;
	int i;

	if (!psde || !pstate) {
		SDE_ERROR("invalid argument(s)\n");
		return;
	}

	pstate->scaler_check_state = SDE_PLANE_SCLCHECK_NONE;
	if (!usr) {
		SDE_DEBUG_PLANE(psde, "scale data removed\n");
		return;
	}

	if (copy_from_user(&scale_v1, usr, sizeof(scale_v1))) {
		SDE_ERROR_PLANE(psde, "failed to copy scale data\n");
		return;
	}

	/* force property to be dirty, even if the pointer didn't change */
	msm_property_set_dirty(&psde->property_info,
			&pstate->property_state, PLANE_PROP_SCALER_V1);

	/* populate from user space */
	pe = &pstate->pixel_ext;
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

	pstate->scaler_check_state = SDE_PLANE_SCLCHECK_SCALER_V1;

	SDE_EVT32_VERBOSE(DRMID(&psde->base));
	SDE_DEBUG_PLANE(psde, "user property data copied\n");
}

static void _sde_plane_clear_predownscale_settings(
			struct sde_plane_state *pstate)
{
	pstate->pre_down.pre_downscale_x_0 = 0;
	pstate->pre_down.pre_downscale_x_1 = 0;
	pstate->pre_down.pre_downscale_y_0 = 0;
	pstate->pre_down.pre_downscale_y_1 = 0;
}

static inline void _sde_plane_set_scaler_v2(struct sde_plane *psde,
		struct sde_plane_state *pstate, void __user *usr)
{
	struct sde_drm_scaler_v2 scale_v2;
	struct sde_hw_pixel_ext *pe;
	int i;
	struct sde_hw_scaler3_cfg *cfg;
	struct sde_hw_inline_pre_downscale_cfg *pd_cfg;

	if (!psde || !pstate) {
		SDE_ERROR("invalid argument(s)\n");
		return;
	}

	cfg = &pstate->scaler3_cfg;
	pd_cfg = &pstate->pre_down;
	pstate->scaler_check_state = SDE_PLANE_SCLCHECK_NONE;
	if (!usr) {
		SDE_DEBUG_PLANE(psde, "scale data removed\n");
		cfg->enable = 0;
		_sde_plane_clear_predownscale_settings(pstate);
		goto end;
	}

	if (copy_from_user(&scale_v2, usr, sizeof(scale_v2))) {
		SDE_ERROR_PLANE(psde, "failed to copy scale data\n");
		return;
	}

	/* detach/ignore user data if 'disabled' */
	if (!scale_v2.enable) {
		SDE_DEBUG_PLANE(psde, "scale data removed\n");
		cfg->enable = 0;
		_sde_plane_clear_predownscale_settings(pstate);
		goto end;
	}

	/* populate from user space */
	sde_set_scaler_v2(cfg, &scale_v2);

	if (_sde_plane_has_pre_downscale(psde)) {
		pd_cfg->pre_downscale_x_0 = scale_v2.pre_downscale_x_0;
		pd_cfg->pre_downscale_x_1 = scale_v2.pre_downscale_x_1;
		pd_cfg->pre_downscale_y_0 = scale_v2.pre_downscale_y_0;
		pd_cfg->pre_downscale_y_1 = scale_v2.pre_downscale_y_1;
	}

	pe = &pstate->pixel_ext;
	memset(pe, 0, sizeof(struct sde_hw_pixel_ext));

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
	pstate->scaler_check_state = SDE_PLANE_SCLCHECK_SCALER_V2_CHECK;

end:
	/* force property to be dirty, even if the pointer didn't change */
	msm_property_set_dirty(&psde->property_info,
			&pstate->property_state, PLANE_PROP_SCALER_V2);

	SDE_EVT32_VERBOSE(DRMID(&psde->base), cfg->enable, cfg->de.enable,
			cfg->src_width[0], cfg->src_height[0],
			cfg->dst_width, cfg->dst_height);
	SDE_DEBUG_PLANE(psde, "user property data copied\n");
}

static void _sde_plane_set_excl_rect_v1(struct sde_plane *psde,
		struct sde_plane_state *pstate, void __user *usr_ptr)
{
	struct drm_clip_rect excl_rect_v1;

	if (!psde || !pstate) {
		SDE_ERROR("invalid argument(s)\n");
		return;
	}

	if (!usr_ptr) {
		memset(&pstate->excl_rect, 0, sizeof(pstate->excl_rect));
		SDE_DEBUG_PLANE(psde, "excl_rect data cleared\n");
		return;
	}

	if (copy_from_user(&excl_rect_v1, usr_ptr, sizeof(excl_rect_v1))) {
		SDE_ERROR_PLANE(psde, "failed to copy excl_rect data\n");
		return;
	}

	/* populate from user space */
	pstate->excl_rect.x = excl_rect_v1.x1;
	pstate->excl_rect.y = excl_rect_v1.y1;
	pstate->excl_rect.w = excl_rect_v1.x2 - excl_rect_v1.x1;
	pstate->excl_rect.h = excl_rect_v1.y2 - excl_rect_v1.y1;

	SDE_DEBUG_PLANE(psde, "excl_rect: {%d,%d,%d,%d}\n",
			pstate->excl_rect.x, pstate->excl_rect.y,
			pstate->excl_rect.w, pstate->excl_rect.h);
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
				&pstate->property_state, property, val);
		if (!ret) {
			idx = msm_property_index(&psde->property_info,
					property);
			switch (idx) {
			case PLANE_PROP_INPUT_FENCE:
				_sde_plane_set_input_fence(psde, pstate, val);
				break;
			case PLANE_PROP_CSC_V1:
			case PLANE_PROP_CSC_DMA_V1:
				_sde_plane_set_csc_v1(psde, (void __user *)val);
				break;
			case PLANE_PROP_SCALER_V1:
				_sde_plane_set_scaler_v1(psde, pstate,
						(void *)(uintptr_t)val);
				break;
			case PLANE_PROP_SCALER_V2:
				_sde_plane_set_scaler_v2(psde, pstate,
						(void *)(uintptr_t)val);
				break;
			case PLANE_PROP_EXCL_RECT_V1:
				_sde_plane_set_excl_rect_v1(psde, pstate,
						(void *)(uintptr_t)val);
				break;
			default:
				/* nothing to do */
				break;
			}
		}
	}

	SDE_DEBUG_PLANE(psde, "%s[%d] <= 0x%llx ret=%d\n",
			property->name, property->base.id, val, ret);

	return ret;
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
				&pstate->property_state, property, val);
	}

	return ret;
}

int sde_plane_helper_reset_custom_properties(struct drm_plane *plane,
		struct drm_plane_state *plane_state)
{
	struct sde_plane *psde;
	struct sde_plane_state *pstate;
	struct drm_property *drm_prop;
	enum msm_mdp_plane_property prop_idx;

	if (!plane || !plane_state) {
		SDE_ERROR("invalid params\n");
		return -EINVAL;
	}

	psde = to_sde_plane(plane);
	pstate = to_sde_plane_state(plane_state);

	for (prop_idx = 0; prop_idx < PLANE_PROP_COUNT; prop_idx++) {
		uint64_t val = pstate->property_values[prop_idx].value;
		uint64_t def;
		int ret;

		drm_prop = msm_property_index_to_drm_property(
				&psde->property_info, prop_idx);
		if (!drm_prop) {
			/* not all props will be installed, based on caps */
			SDE_DEBUG_PLANE(psde, "invalid property index %d\n",
					prop_idx);
			continue;
		}

		def = msm_property_get_default(&psde->property_info, prop_idx);
		if (val == def)
			continue;

		SDE_DEBUG_PLANE(psde, "set prop %s idx %d from %llu to %llu\n",
				drm_prop->name, prop_idx, val, def);

		ret = sde_plane_atomic_set_property(plane, plane_state,
				drm_prop, def);
		if (ret) {
			SDE_ERROR_PLANE(psde,
					"set property failed, idx %d ret %d\n",
					prop_idx, ret);
			continue;
		}
	}

	return 0;
}

static void sde_plane_destroy(struct drm_plane *plane)
{
	struct sde_plane *psde = plane ? to_sde_plane(plane) : NULL;

	SDE_DEBUG_PLANE(psde, "\n");

	if (psde) {
		_sde_plane_set_qos_ctrl(plane, false, SDE_PLANE_QOS_PANIC_CTRL);

		if (psde->blob_info)
			drm_property_blob_put(psde->blob_info);
		msm_property_destroy(&psde->property_info);
		mutex_destroy(&psde->lock);

		drm_plane_helper_disable(plane, NULL);

		/* this will destroy the states as well */
		drm_plane_cleanup(plane);

		if (psde->pipe_hw)
			sde_hw_sspp_destroy(psde->pipe_hw);

		kfree(psde);
	}
}

void sde_plane_destroy_fb(struct drm_plane_state *state)
{
	struct sde_plane_state *pstate;

	if (!state) {
		SDE_ERROR("invalid arg state %d\n", !state);
		return;
	}

	pstate = to_sde_plane_state(state);

	if (sde_plane_get_property(pstate, PLANE_PROP_FB_TRANSLATION_MODE) ==
			SDE_DRM_FB_SEC) {
		/* remove ref count for frame buffers */
		if (state->fb) {
			drm_framebuffer_put(state->fb);
			state->fb = NULL;
		}
	}

}

static void sde_plane_destroy_state(struct drm_plane *plane,
		struct drm_plane_state *state)
{
	struct sde_plane *psde;
	struct sde_plane_state *pstate;

	if (!plane || !state) {
		SDE_ERROR("invalid arg(s), plane %d state %d\n",
				!plane, !state);
		return;
	}

	psde = to_sde_plane(plane);
	pstate = to_sde_plane_state(state);

	SDE_DEBUG_PLANE(psde, "\n");

	/* remove ref count for frame buffers */
	if (state->fb)
		drm_framebuffer_put(state->fb);

	/* remove ref count for fence */
	if (pstate->input_fence)
		sde_sync_put(pstate->input_fence);

	/* destroy value helper */
	msm_property_destroy_state(&psde->property_info, pstate,
			&pstate->property_state);
}

static struct drm_plane_state *
sde_plane_duplicate_state(struct drm_plane *plane)
{
	struct sde_plane *psde;
	struct sde_plane_state *pstate;
	struct sde_plane_state *old_state;
	struct drm_property *drm_prop;
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
			&pstate->property_state, pstate->property_values);

	/* clear out any input fence */
	pstate->input_fence = 0;
	input_fence_default = msm_property_get_default(
			&psde->property_info, PLANE_PROP_INPUT_FENCE);
	drm_prop = msm_property_index_to_drm_property(
				&psde->property_info, PLANE_PROP_INPUT_FENCE);
	if (msm_property_atomic_set(&psde->property_info,
				&pstate->property_state, drm_prop,
				input_fence_default))
		SDE_DEBUG_PLANE(psde,
				"error clearing duplicated input fence\n");

	pstate->dirty = 0x0;
	pstate->pending = false;

	__drm_atomic_helper_plane_duplicate_state(plane, &pstate->base);

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

	if (plane->state && !sde_crtc_is_reset_required(plane->state->crtc)) {
		SDE_DEBUG_PLANE(psde, "avoid reset for plane\n");
		return;
	}

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
			&pstate->property_state,
			pstate->property_values);

	pstate->base.plane = plane;

	plane->state = &pstate->base;
}

u32 sde_plane_get_ubwc_error(struct drm_plane *plane)
{
	u32 ubwc_error = 0;
	struct sde_plane *psde;

	if (!plane) {
		SDE_ERROR("invalid plane\n");
		return 0;
	}
	psde = to_sde_plane(plane);

	if (!psde->is_virtual && psde->pipe_hw->ops.get_ubwc_error)
		ubwc_error = psde->pipe_hw->ops.get_ubwc_error(psde->pipe_hw);

	return ubwc_error;
}

void sde_plane_clear_ubwc_error(struct drm_plane *plane)
{
	struct sde_plane *psde;

	if (!plane) {
		SDE_ERROR("invalid plane\n");
		return;
	}
	psde = to_sde_plane(plane);

	if (psde->pipe_hw->ops.clear_ubwc_error)
		psde->pipe_hw->ops.clear_ubwc_error(psde->pipe_hw);
}

#ifdef CONFIG_DEBUG_FS
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

static int _sde_plane_init_debugfs(struct drm_plane *plane)
{
	struct sde_plane *psde;
	struct sde_kms *kms;
	struct msm_drm_private *priv;
	const struct sde_sspp_sub_blks *sblk = 0;
	const struct sde_sspp_cfg *cfg = 0;

	if (!plane || !plane->dev) {
		SDE_ERROR("invalid arguments\n");
		return -EINVAL;
	}

	priv = plane->dev->dev_private;
	if (!priv || !priv->kms) {
		SDE_ERROR("invalid KMS reference\n");
		return -EINVAL;
	}

	kms = to_sde_kms(priv->kms);
	psde = to_sde_plane(plane);

	if (psde && psde->pipe_hw)
		cfg = psde->pipe_hw->cap;
	if (cfg)
		sblk = cfg->sblk;

	if (!sblk)
		return 0;

	/* create overall sub-directory for the pipe */
	psde->debugfs_root =
		debugfs_create_dir(psde->pipe_name,
				plane->dev->primary->debugfs_root);

	if (!psde->debugfs_root)
		return -ENOMEM;

	/* don't error check these */
	debugfs_create_x32("features", 0400,
		psde->debugfs_root, &psde->features);

	if (cfg->features & BIT(SDE_SSPP_SCALER_QSEED3) ||
			cfg->features & BIT(SDE_SSPP_SCALER_QSEED3LITE) ||
			cfg->features & BIT(SDE_SSPP_SCALER_QSEED2))
		debugfs_create_bool("default_scaling",
				0600,
				psde->debugfs_root,
				&psde->debugfs_default_scale);

	if (cfg->features & BIT(SDE_SSPP_TRUE_INLINE_ROT)) {
		debugfs_create_u32("in_rot_max_downscale_rt_num",
			0600,
			psde->debugfs_root,
			(u32 *) &psde->pipe_sblk->in_rot_maxdwnscale_rt_num);
		debugfs_create_u32("in_rot_max_downscale_rt_denom",
			0600,
			psde->debugfs_root,
			(u32 *) &psde->pipe_sblk->in_rot_maxdwnscale_rt_denom);
		debugfs_create_u32("in_rot_max_downscale_nrt",
			0600,
			psde->debugfs_root,
			(u32 *) &psde->pipe_sblk->in_rot_maxdwnscale_nrt);
		debugfs_create_u32("in_rot_max_height",
			0600,
			psde->debugfs_root,
			(u32 *) &psde->pipe_sblk->in_rot_maxheight);
	}

	debugfs_create_u32("xin_id",
			0400,
			psde->debugfs_root,
			(u32 *) &cfg->xin_id);
	debugfs_create_x32("creq_vblank",
			0600,
			psde->debugfs_root,
			(u32 *) &sblk->creq_vblank);
	debugfs_create_x32("danger_vblank",
			0600,
			psde->debugfs_root,
			(u32 *) &sblk->danger_vblank);

	debugfs_create_file("disable_danger",
			0600,
			psde->debugfs_root,
			kms, &sde_plane_danger_enable);

	return 0;
}

static void _sde_plane_destroy_debugfs(struct drm_plane *plane)
{
	struct sde_plane *psde;

	if (!plane)
		return;
	psde = to_sde_plane(plane);

	debugfs_remove_recursive(psde->debugfs_root);
}
#else
static int _sde_plane_init_debugfs(struct drm_plane *plane)
{
	return 0;
}
static void _sde_plane_destroy_debugfs(struct drm_plane *plane)
{
}
#endif

static int sde_plane_late_register(struct drm_plane *plane)
{
	return _sde_plane_init_debugfs(plane);
}

static void sde_plane_early_unregister(struct drm_plane *plane)
{
	_sde_plane_destroy_debugfs(plane);
}

static const struct drm_plane_funcs sde_plane_funcs = {
		.update_plane = drm_atomic_helper_update_plane,
		.disable_plane = drm_atomic_helper_disable_plane,
		.destroy = sde_plane_destroy,
		.atomic_set_property = sde_plane_atomic_set_property,
		.atomic_get_property = sde_plane_atomic_get_property,
		.reset = sde_plane_reset,
		.atomic_duplicate_state = sde_plane_duplicate_state,
		.atomic_destroy_state = sde_plane_destroy_state,
		.late_register = sde_plane_late_register,
		.early_unregister = sde_plane_early_unregister,
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

bool is_sde_plane_virtual(struct drm_plane *plane)
{
	return plane ? to_sde_plane(plane)->is_virtual : false;
}

/* initialize plane */
struct drm_plane *sde_plane_init(struct drm_device *dev,
		uint32_t pipe, bool primary_plane,
		unsigned long possible_crtcs, u32 master_plane_id)
{
	struct drm_plane *plane = NULL, *master_plane = NULL;
	const struct sde_format_extended *format_list;
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
	psde->is_virtual = (master_plane_id != 0);
	INIT_LIST_HEAD(&psde->mplane_list);
	master_plane = drm_plane_find(dev, NULL, master_plane_id);
	if (master_plane) {
		struct sde_plane *mpsde = to_sde_plane(master_plane);

		list_add_tail(&psde->mplane_list, &mpsde->mplane_list);
	}

	/* initialize underlying h/w driver */
	psde->pipe_hw = sde_hw_sspp_init(pipe, kms->mmio, kms->catalog,
							master_plane_id != 0);
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
	psde->perf_features = psde->pipe_hw->cap->perf_features;
	psde->pipe_sblk = psde->pipe_hw->cap->sblk;
	if (!psde->pipe_sblk) {
		SDE_ERROR("[%u]invalid sblk\n", pipe);
		goto clean_sspp;
	}

	if (!master_plane_id)
		format_list = psde->pipe_sblk->format_list;
	else
		format_list = psde->pipe_sblk->virt_format_list;

	psde->nformats = sde_populate_formats(format_list,
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
	ret = drm_universal_plane_init(dev, plane, 0xff, &sde_plane_funcs,
				psde->formats, psde->nformats,
				NULL, type, NULL);
	if (ret)
		goto clean_sspp;

	/* Populate static array of plane property flags */
	_sde_plane_map_prop_to_dirty_bits();

	/* success! finalize initialization */
	drm_plane_helper_add(plane, &sde_plane_helper_funcs);

	msm_property_init(&psde->property_info, &plane->base, dev,
			priv->plane_property, psde->property_data,
			PLANE_PROP_COUNT, PLANE_PROP_BLOBCOUNT,
			sizeof(struct sde_plane_state));

	_sde_plane_install_properties(plane, kms->catalog, master_plane_id);

	/* save user friendly pipe name for later */
	snprintf(psde->pipe_name, SDE_NAME_SIZE, "plane%u", plane->base.id);

	mutex_init(&psde->lock);

	SDE_DEBUG("%s created for pipe:%u id:%u virtual:%u\n", psde->pipe_name,
					pipe, plane->base.id, master_plane_id);
	return plane;

clean_sspp:
	if (psde && psde->pipe_hw)
		sde_hw_sspp_destroy(psde->pipe_hw);
clean_plane:
	kfree(psde);
exit:
	return ERR_PTR(ret);
}
