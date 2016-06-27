/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/debugfs.h>
#include "sde_kms.h"
#include "sde_hwio.h"
#include "sde_hw_mdp_ctl.h"
#include "sde_mdp_formats.h"
#include "sde_hw_sspp.h"

#define DECIMATED_DIMENSION(dim, deci) (((dim) + ((1 << (deci)) - 1)) >> (deci))
#define PHASE_STEP_SHIFT	21
#define PHASE_STEP_UNIT_SCALE   ((int) (1 << PHASE_STEP_SHIFT))
#define PHASE_RESIDUAL		15

#define SHARP_STRENGTH_DEFAULT	32
#define SHARP_EDGE_THR_DEFAULT	112
#define SHARP_SMOOTH_THR_DEFAULT	8
#define SHARP_NOISE_THR_DEFAULT	2

#define SDE_PIPE_NAME_SIZE  8

struct sde_plane {
	struct drm_plane base;
	const char *name;

	int mmu_id;

	enum sde_sspp pipe;
	uint32_t features;      /* capabilities from catalog */
	uint32_t nformats;
	uint32_t formats[32];

	struct sde_hw_pipe *pipe_hw;
	struct sde_hw_pipe_cfg pipe_cfg;
	struct sde_hw_pixel_ext pixel_ext;
	struct sde_hw_sharp_cfg sharp_cfg;

	char pipe_name[SDE_PIPE_NAME_SIZE];

	/* debugfs related stuff */
	struct dentry *debugfs_root;
	struct sde_debugfs_regset32 debugfs_src;
	struct sde_debugfs_regset32 debugfs_scaler;
	struct sde_debugfs_regset32 debugfs_csc;
};
#define to_sde_plane(x) container_of(x, struct sde_plane, base)

static bool sde_plane_enabled(struct drm_plane_state *state)
{
	return state->fb && state->crtc;
}

static void _sde_plane_set_scanout(struct drm_plane *plane,
		struct sde_hw_pipe_cfg *pipe_cfg, struct drm_framebuffer *fb)
{
	struct sde_plane *psde = to_sde_plane(plane);
	int i;

	if (pipe_cfg && fb && psde->pipe_hw->ops.setup_sourceaddress) {
		/* stride */
		i = min_t(int, ARRAY_SIZE(fb->pitches), SDE_MAX_PLANES);
		while (i) {
			--i;
			pipe_cfg->src.ystride[i] = fb->pitches[i];
		}

		/* address */
		for (i = 0; i < ARRAY_SIZE(pipe_cfg->addr.plane); ++i)
			pipe_cfg->addr.plane[i] = msm_framebuffer_iova(fb,
					psde->mmu_id, i);

		/* hw driver */
		psde->pipe_hw->ops.setup_sourceaddress(psde->pipe_hw, pipe_cfg);
	}
}

static void _sde_plane_setup_scaler(struct drm_plane *plane,
		uint32_t src, uint32_t dst, uint32_t *phase_steps,
		enum sde_hw_filter *filter, struct sde_mdp_format_params *fmt,
		uint32_t chroma_subsampling)
{
	/* calcualte phase steps, leave init phase as zero */
	phase_steps[SDE_SSPP_COMP_0] =
		mult_frac(1 << PHASE_STEP_SHIFT, src, dst);
	phase_steps[SDE_SSPP_COMP_1_2] =
		phase_steps[SDE_SSPP_COMP_0] / chroma_subsampling;
	phase_steps[SDE_SSPP_COMP_2] = phase_steps[SDE_SSPP_COMP_1_2];
	phase_steps[SDE_SSPP_COMP_3] = phase_steps[SDE_SSPP_COMP_0];

	/* calculate scaler config, if necessary */
	if (fmt->is_yuv || src != dst) {
		filter[SDE_SSPP_COMP_3] =
			(src <= dst) ? SDE_MDP_SCALE_FILTER_BIL :
			SDE_MDP_SCALE_FILTER_PCMN;

		if (fmt->is_yuv) {
			filter[SDE_SSPP_COMP_0] = SDE_MDP_SCALE_FILTER_CA;
			filter[SDE_SSPP_COMP_1_2] = filter[SDE_SSPP_COMP_3];
		} else {
			filter[SDE_SSPP_COMP_0] = filter[SDE_SSPP_COMP_3];
			filter[SDE_SSPP_COMP_1_2] =
				SDE_MDP_SCALE_FILTER_NEAREST;
		}
	} else {
		/* disable scaler */
		filter[SDE_SSPP_COMP_0] = SDE_MDP_SCALE_FILTER_MAX;
		filter[SDE_SSPP_COMP_1_2] = SDE_MDP_SCALE_FILTER_MAX;
		filter[SDE_SSPP_COMP_3] = SDE_MDP_SCALE_FILTER_MAX;
	}
}

static void _sde_plane_setup_pixel_ext(struct drm_plane *plane,
		uint32_t src, uint32_t dst, uint32_t decimated_src,
		uint32_t *phase_steps, uint32_t *out_src, int *out_edge1,
		int *out_edge2, enum sde_hw_filter *filter,
		struct sde_mdp_format_params *fmt, uint32_t chroma_subsampling,
		bool post_compare)
{
	int64_t edge1, edge2, caf;
	uint32_t src_work;
	int i, tmp;

	if (plane && phase_steps && out_src && out_edge1 &&
			out_edge2 && filter && fmt) {
		/* handle CAF for YUV formats */
		if (fmt->is_yuv && SDE_MDP_SCALE_FILTER_CA == *filter)
			caf = PHASE_STEP_UNIT_SCALE;
		else
			caf = 0;

		for (i = 0; i < SDE_MAX_PLANES; i++) {
			src_work = decimated_src;
			if (i == SDE_SSPP_COMP_1_2 || i == SDE_SSPP_COMP_2)
				src_work /= chroma_subsampling;
			if (post_compare)
				src = src_work;
			if (!(fmt->is_yuv) && (src == dst)) {
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

static void _sde_plane_setup_csc(struct sde_plane *psde,
		struct sde_plane_state *pstate,
		struct sde_mdp_format_params *fmt)
{
	static const struct sde_csc_cfg sde_csc_YUV2RGB_601L = {
		{
			0x0254, 0x0000, 0x0331,
			0x0254, 0xff37, 0xfe60,
			0x0254, 0x0409, 0x0000,
		},
		{ 0xfff0, 0xff80, 0xff80,},
		{ 0x0, 0x0, 0x0,},
		{ 0x10, 0xeb, 0x10, 0xf0, 0x10, 0xf0,},
		{ 0x0, 0xff, 0x0, 0xff, 0x0, 0xff,},
	};

	static const struct sde_csc_cfg sde_csc_NOP = {
		{
			0x0200, 0x0000, 0x0000,
			0x0000, 0x0200, 0x0000,
			0x0000, 0x0000, 0x0200,
		},
		{ 0x0, 0x0, 0x0,},
		{ 0x0, 0x0, 0x0,},
		{ 0x0, 0xff, 0x0, 0xff, 0x0, 0xff,},
		{ 0x0, 0xff, 0x0, 0xff, 0x0, 0xff,},
	};

	if (!psde->pipe_hw->ops.setup_csc)
		return;

	if (fmt->is_yuv)
		psde->pipe_hw->ops.setup_csc(psde->pipe_hw,
			(struct sde_csc_cfg *)&sde_csc_YUV2RGB_601L);
	else
		psde->pipe_hw->ops.setup_csc(psde->pipe_hw,
			(struct sde_csc_cfg *)&sde_csc_NOP);
}

static int _sde_plane_mode_set(struct drm_plane *plane,
		struct drm_crtc *crtc, struct drm_framebuffer *fb,
		int crtc_x, int crtc_y,
		unsigned int crtc_w, unsigned int crtc_h,
		uint32_t src_x, uint32_t src_y,
		uint32_t src_w, uint32_t src_h)
{
	struct sde_plane *psde = to_sde_plane(plane);
	struct sde_plane_state *pstate;
	const struct mdp_format *format;
	uint32_t nplanes, pix_format, tmp;
	int i;
	struct sde_mdp_format_params *fmt;
	struct sde_hw_pixel_ext *pe;
	int ret = 0;

	DBG("");
	nplanes = drm_format_num_planes(fb->pixel_format);

	pstate = to_sde_plane_state(plane->state);

	format = to_mdp_format(msm_framebuffer_format(fb));
	pix_format = format->base.pixel_format;

	/* src values are in Q16 fixed point, convert to integer */
	src_x = src_x >> 16;
	src_y = src_y >> 16;
	src_w = src_w >> 16;
	src_h = src_h >> 16;

	DBG("%s: FB[%u] %u,%u,%u,%u -> CRTC[%u] %d,%d,%u,%u", psde->pipe_name,
			fb->base.id, src_x, src_y, src_w, src_h,
			crtc->base.id, crtc_x, crtc_y, crtc_w, crtc_h);

	/* update format configuration */
	memset(&(psde->pipe_cfg), 0, sizeof(struct sde_hw_pipe_cfg));

	psde->pipe_cfg.src.format = sde_mdp_get_format_params(pix_format,
			fb->modifier[0]);
	psde->pipe_cfg.src.width = fb->width;
	psde->pipe_cfg.src.height = fb->height;
	psde->pipe_cfg.src.num_planes = nplanes;

	_sde_plane_set_scanout(plane, &psde->pipe_cfg, fb);

	psde->pipe_cfg.src_rect.x = src_x;
	psde->pipe_cfg.src_rect.y = src_y;
	psde->pipe_cfg.src_rect.w = src_w;
	psde->pipe_cfg.src_rect.h = src_h;

	psde->pipe_cfg.dst_rect.x = crtc_x;
	psde->pipe_cfg.dst_rect.y = crtc_y;
	psde->pipe_cfg.dst_rect.w = crtc_w;
	psde->pipe_cfg.dst_rect.h = crtc_h;

	psde->pipe_cfg.horz_decimation = 0;
	psde->pipe_cfg.vert_decimation = 0;

	/* get sde pixel format definition */
	fmt = psde->pipe_cfg.src.format;

	/* update pixel extensions */
	pe = &(psde->pixel_ext);
	if (!pe->enable_pxl_ext) {
		uint32_t chroma_subsample_h, chroma_subsample_v;

		chroma_subsample_h = psde->pipe_cfg.horz_decimation ? 1 :
			drm_format_horz_chroma_subsampling(pix_format);
		chroma_subsample_v = psde->pipe_cfg.vert_decimation ? 1 :
			drm_format_vert_chroma_subsampling(pix_format);

		memset(pe, 0, sizeof(struct sde_hw_pixel_ext));

		/* calculate phase steps */
		_sde_plane_setup_scaler(plane, src_w, crtc_w,
				pe->phase_step_x,
				pe->horz_filter, fmt, chroma_subsample_h);
		_sde_plane_setup_scaler(plane, src_h, crtc_h,
				pe->phase_step_y,
				pe->vert_filter, fmt, chroma_subsample_v);

		/* calculate left/right/top/bottom pixel extentions */
		tmp = DECIMATED_DIMENSION(src_w,
				psde->pipe_cfg.horz_decimation);
		if (fmt->is_yuv)
			tmp &= ~0x1;
		_sde_plane_setup_pixel_ext(plane, src_w, crtc_w, tmp,
				pe->phase_step_x,
				pe->roi_w,
				pe->num_ext_pxls_left,
				pe->num_ext_pxls_right, pe->horz_filter, fmt,
				chroma_subsample_h, 0);

		tmp = DECIMATED_DIMENSION(src_h,
				psde->pipe_cfg.vert_decimation);
		_sde_plane_setup_pixel_ext(plane, src_h, crtc_h, tmp,
				pe->phase_step_y,
				pe->roi_h,
				pe->num_ext_pxls_top,
				pe->num_ext_pxls_btm, pe->vert_filter, fmt,
				chroma_subsample_v, 1);

		for (i = 0; i < SDE_MAX_PLANES; i++) {
			if (pe->num_ext_pxls_left[i] >= 0)
				pe->left_rpt[i] =
					pe->num_ext_pxls_left[i];
			else
				pe->left_ftch[i] =
					pe->num_ext_pxls_left[i];

			if (pe->num_ext_pxls_right[i] >= 0)
				pe->right_rpt[i] =
					pe->num_ext_pxls_right[i];
			else
				pe->right_ftch[i] =
					pe->num_ext_pxls_right[i];

			if (pe->num_ext_pxls_top[i] >= 0)
				pe->top_rpt[i] =
					pe->num_ext_pxls_top[i];
			else
				pe->top_ftch[i] =
					pe->num_ext_pxls_top[i];

			if (pe->num_ext_pxls_btm[i] >= 0)
				pe->btm_rpt[i] =
					pe->num_ext_pxls_btm[i];
			else
				pe->btm_ftch[i] =
					pe->num_ext_pxls_btm[i];
		}
	}

	if (psde->pipe_hw->ops.setup_sourceformat)
		psde->pipe_hw->ops.setup_sourceformat(psde->pipe_hw,
				&psde->pipe_cfg, 0);
	if (psde->pipe_hw->ops.setup_rects)
		psde->pipe_hw->ops.setup_rects(psde->pipe_hw,
				&psde->pipe_cfg, &psde->pixel_ext);

	/* update sharpening */
	psde->sharp_cfg.strength = SHARP_STRENGTH_DEFAULT;
	psde->sharp_cfg.edge_thr = SHARP_EDGE_THR_DEFAULT;
	psde->sharp_cfg.smooth_thr = SHARP_SMOOTH_THR_DEFAULT;
	psde->sharp_cfg.noise_thr = SHARP_NOISE_THR_DEFAULT;

	if (psde->pipe_hw->ops.setup_sharpening)
		psde->pipe_hw->ops.setup_sharpening(psde->pipe_hw,
			&psde->sharp_cfg);

	/* update csc */
	if (fmt->is_yuv)
		_sde_plane_setup_csc(psde, pstate, fmt);

	return ret;
}

static int sde_plane_prepare_fb(struct drm_plane *plane,
		const struct drm_plane_state *new_state)
{
	struct drm_framebuffer *fb = new_state->fb;
	struct sde_plane *psde = to_sde_plane(plane);

	if (!new_state->fb)
		return 0;

	DBG("%s: prepare: FB[%u]", psde->pipe_name, fb->base.id);
	return msm_framebuffer_prepare(fb, psde->mmu_id);
}

static void sde_plane_cleanup_fb(struct drm_plane *plane,
		const struct drm_plane_state *old_state)
{
	struct drm_framebuffer *fb = old_state->fb;
	struct sde_plane *psde = to_sde_plane(plane);

	if (!fb)
		return;

	DBG("%s: cleanup: FB[%u]", psde->pipe_name, fb->base.id);
	msm_framebuffer_cleanup(fb, psde->mmu_id);
}

static int sde_plane_atomic_check(struct drm_plane *plane,
		struct drm_plane_state *state)
{
	struct sde_plane *psde = to_sde_plane(plane);
	struct drm_plane_state *old_state = plane->state;
	const struct mdp_format *format;

	DBG("%s: check (%d -> %d)", psde->pipe_name,
			sde_plane_enabled(old_state), sde_plane_enabled(state));

	if (sde_plane_enabled(state)) {
		/* CIFIX: don't use mdp format? */
		format = to_mdp_format(msm_framebuffer_format(state->fb));
		if (MDP_FORMAT_IS_YUV(format) &&
			(!(psde->features & SDE_SSPP_SCALER) ||
			 !(psde->features & BIT(SDE_SSPP_CSC)))) {
			dev_err(plane->dev->dev,
				"Pipe doesn't support YUV\n");

			return -EINVAL;
		}

		if (!(psde->features & SDE_SSPP_SCALER) &&
			(((state->src_w >> 16) != state->crtc_w) ||
			((state->src_h >> 16) != state->crtc_h))) {
			dev_err(plane->dev->dev,
				"Pipe doesn't support scaling (%dx%d -> %dx%d)\n",
				state->src_w >> 16, state->src_h >> 16,
				state->crtc_w, state->crtc_h);

			return -EINVAL;
		}
	}

	if (sde_plane_enabled(state) && sde_plane_enabled(old_state)) {
		/* we cannot change SMP block configuration during scanout: */
		bool full_modeset = false;

		if (state->fb->pixel_format != old_state->fb->pixel_format) {
			DBG("%s: pixel_format change!", psde->pipe_name);
			full_modeset = true;
		}
		if (state->src_w != old_state->src_w) {
			DBG("%s: src_w change!", psde->pipe_name);
			full_modeset = true;
		}
		if (to_sde_plane_state(old_state)->pending) {
			DBG("%s: still pending!", psde->pipe_name);
			full_modeset = true;
		}
		if (full_modeset) {
			struct drm_crtc_state *crtc_state =
				drm_atomic_get_crtc_state(state->state,
					state->crtc);
			crtc_state->mode_changed = true;
			to_sde_plane_state(state)->mode_changed = true;
		}
	} else {
		to_sde_plane_state(state)->mode_changed = true;
	}

	return 0;
}

static void sde_plane_atomic_update(struct drm_plane *plane,
				struct drm_plane_state *old_state)
{
	struct sde_plane *sde_plane = to_sde_plane(plane);
	struct drm_plane_state *state = plane->state;

	DBG("%s: update", sde_plane->pipe_name);

	if (!sde_plane_enabled(state)) {
		to_sde_plane_state(state)->pending = true;
	} else if (to_sde_plane_state(state)->mode_changed) {
		int ret;

		to_sde_plane_state(state)->pending = true;
		ret = _sde_plane_mode_set(plane,
				state->crtc, state->fb,
				state->crtc_x, state->crtc_y,
				state->crtc_w, state->crtc_h,
				state->src_x,  state->src_y,
				state->src_w, state->src_h);
		/* atomic_check should have ensured that this doesn't fail */
		WARN_ON(ret < 0);
	} else {
		_sde_plane_set_scanout(plane, &sde_plane->pipe_cfg, state->fb);
	}
}

static void _sde_plane_install_range_property(struct drm_plane *plane,
		struct drm_device *dev, const char *name,
		uint64_t min, uint64_t max, uint64_t init,
		struct drm_property **prop)
{
	if (plane && dev && name && prop) {
		/* only create the property once */
		if (*prop == 0) {
			*prop = drm_property_create_range(dev,
					0 /* flags */, name, min, max);
			if (*prop == 0)
				DRM_ERROR("Create property %s failed\n", name);
		}

		/* always attach property, if created */
		if (*prop)
			drm_object_attach_property(&plane->base, *prop, init);
	}
}

static void _sde_plane_install_blob_property(struct drm_plane *plane,
		struct drm_device *dev, const char *name,
		struct drm_property **prop)
{
}

/* helper to install properties which are common to planes and crtcs */
static void _sde_plane_install_properties(struct drm_plane *plane,
		struct drm_mode_object *obj)
{
	struct drm_device *dev = plane->dev;
	struct msm_drm_private *dev_priv = dev->dev_private;

	DBG("");

	/* range/enum properties */
	_sde_plane_install_range_property(plane, dev, "zpos", 1, 255, 1,
			&(dev_priv->plane_property[PLANE_PROP_ZPOS]));

	/* blob properties */
	_sde_plane_install_blob_property(plane, dev, "pixext",
			&(dev_priv->plane_property[PLANE_PROP_PIXEXT]));
}

static int sde_plane_atomic_set_property(struct drm_plane *plane,
		struct drm_plane_state *state, struct drm_property *property,
		uint64_t val)
{
	struct drm_device *dev = plane->dev;
	struct sde_plane_state *pstate;
	struct drm_property_blob *blob, **prop_blob;
	struct msm_drm_private *dev_priv = dev->dev_private;
	int idx, ret = -EINVAL;

	DBG("");

	pstate = to_sde_plane_state(state);

	for (idx = 0; idx < PLANE_PROP_COUNT && ret; ++idx) {
		if (dev_priv->plane_property[idx] == property) {
			DBG("Set property %d <= %d", idx, (int)val);

			/* FUTURE: Add special handling here */
			if (property->flags & DRM_MODE_PROP_BLOB) {
				blob = drm_property_lookup_blob(dev,
					(uint32_t)val);
				if (!blob) {
					dev_err(dev->dev, "Blob not found\n");
					val = 0;
				} else {
					val = blob->base.id;

					/* save blobs for later */
					prop_blob =
						&pstate->property_blobs[idx -
						PLANE_PROP_FIRSTBLOB];
					/* need to clear previous reference */
					if (*prop_blob)
						drm_property_unreference_blob(
						    *prop_blob);
					*prop_blob = blob;
				}
			}
			pstate->property_values[idx] = val;
			ret = 0;
		}
	}

	if (ret == -EINVAL)
		dev_err(dev->dev, "Invalid property set\n");

	return ret;
}

static int sde_plane_set_property(struct drm_plane *plane,
		struct drm_property *property, uint64_t val)
{
	int rc;

	DBG("");
	rc = sde_plane_atomic_set_property(plane, plane->state, property,
		val);
	return rc;
}

static int sde_plane_atomic_get_property(struct drm_plane *plane,
		const struct drm_plane_state *state,
		struct drm_property *property, uint64_t *val)
{
	struct drm_device *dev = plane->dev;
	struct sde_plane_state *pstate;
	struct msm_drm_private *dev_priv = dev->dev_private;
	int idx, ret = -EINVAL;

	DBG("");

	pstate = to_sde_plane_state(state);

	for (idx = 0; idx < PLANE_PROP_COUNT; ++idx) {
		if (dev_priv->plane_property[idx] == property) {
			*val = pstate->property_values[idx];
			DBG("Get property %d %lld", idx, *val);
			ret = 0;
			break;
		}
	}

	if (ret == -EINVAL)
		dev_err(dev->dev, "Invalid property get\n");

	return ret;
}

static void sde_plane_destroy(struct drm_plane *plane)
{
	struct sde_plane *psde;

	DBG("");

	if (plane) {
		psde = to_sde_plane(plane);

		debugfs_remove_recursive(psde->debugfs_root);

		if (psde->pipe_hw)
			sde_hw_sspp_destroy(psde->pipe_hw);

		drm_plane_helper_disable(plane);

		/* this will destroy the states as well */
		drm_plane_cleanup(plane);

		kfree(psde);
	}
}

static void sde_plane_destroy_state(struct drm_plane *plane,
		struct drm_plane_state *state)
{
	struct sde_plane_state *pstate;
	int i;

	DBG("");

	/* remove ref count for frame buffers */
	if (state->fb)
		drm_framebuffer_unreference(state->fb);

	pstate = to_sde_plane_state(state);

	/* remove ref count for blobs */
	for (i = 0; i < PLANE_PROP_BLOBCOUNT; ++i)
		if (pstate->property_blobs[i])
			drm_property_unreference_blob(
					pstate->property_blobs[i]);

	kfree(pstate);
}

static struct drm_plane_state *
sde_plane_duplicate_state(struct drm_plane *plane)
{
	struct sde_plane_state *pstate;
	int i;

	if (WARN_ON(!plane->state))
		return NULL;

	DBG("");
	pstate = kmemdup(to_sde_plane_state(plane->state),
			sizeof(*pstate), GFP_KERNEL);
	if (pstate) {
		/* add ref count for frame buffer */
		if (pstate->base.fb)
			drm_framebuffer_reference(pstate->base.fb);

		/* add ref count for blobs */
		for (i = 0; i < PLANE_PROP_BLOBCOUNT; ++i)
			if (pstate->property_blobs[i])
				drm_property_reference_blob(
						pstate->property_blobs[i]);

		pstate->mode_changed = false;
		pstate->pending = false;
	}

	return pstate ? &pstate->base : NULL;
}

static void sde_plane_reset(struct drm_plane *plane)
{
	struct sde_plane_state *pstate;

	DBG("");
	if (plane->state && plane->state->fb)
		drm_framebuffer_unreference(plane->state->fb);

	kfree(to_sde_plane_state(plane->state));
	pstate = kzalloc(sizeof(*pstate), GFP_KERNEL);

	memset(pstate, 0, sizeof(struct sde_plane_state));

	/* assign default blend parameters */
	pstate->property_values[PLANE_PROP_ALPHA] = 255;
	pstate->property_values[PLANE_PROP_PREMULTIPLIED] = 0;

	if (plane->type == DRM_PLANE_TYPE_PRIMARY)
		pstate->property_values[PLANE_PROP_ZPOS] = STAGE_BASE;
	else
		pstate->property_values[PLANE_PROP_ZPOS] =
			STAGE0 + drm_plane_index(plane);

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
	struct sde_plane *sde_plane = to_sde_plane(plane);

	return sde_plane->pipe;
}

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
			debugfs_create_x32("features", 0444,
					psde->debugfs_root, &psde->features);

			/* add register dump support */
			sde_debugfs_setup_regset32(&psde->debugfs_src,
					sblk->src_blk.base + cfg->base,
					sblk->src_blk.len,
					kms->mmio);
			sde_debugfs_create_regset32("src_blk", 0444,
					psde->debugfs_root, &psde->debugfs_src);

			sde_debugfs_setup_regset32(&psde->debugfs_scaler,
					sblk->scaler_blk.base + cfg->base,
					sblk->scaler_blk.len,
					kms->mmio);
			sde_debugfs_create_regset32("scaler_blk", 0444,
					psde->debugfs_root,
					&psde->debugfs_scaler);

			sde_debugfs_setup_regset32(&psde->debugfs_csc,
					sblk->csc_blk.base + cfg->base,
					sblk->csc_blk.len,
					kms->mmio);
			sde_debugfs_create_regset32("csc_blk", 0444,
					psde->debugfs_root, &psde->debugfs_csc);
		}
	}
}

/* initialize plane */
struct drm_plane *sde_plane_init(struct drm_device *dev,
		uint32_t pipe, bool private_plane)
{
	struct drm_plane *plane = NULL;
	struct sde_plane *psde;
	struct msm_drm_private *priv;
	struct sde_kms *kms;
	struct sde_mdss_cfg *sde_cat;
	int ret;
	enum drm_plane_type type;

	priv = dev->dev_private;
	if (!priv) {
		DRM_ERROR("[%u]Private data is NULL\n", pipe);
		goto exit;
	}

	if (!priv->kms) {
		DRM_ERROR("[%u]Invalid KMS reference\n", pipe);
		goto exit;
	}
	kms = to_sde_kms(priv->kms);

	/* create and zero local structure */
	psde = kzalloc(sizeof(*psde), GFP_KERNEL);
	if (!psde) {
		ret = -ENOMEM;
		goto fail;
	}

	plane = &psde->base;

	psde->pipe = pipe;

	if (kms) {
		/* mmu id for buffer mapping */
		psde->mmu_id = kms->mmu_id;

		/* check catalog for features mask */
		sde_cat = kms->catalog;
		if (sde_cat)
			psde->features = sde_cat->sspp[pipe].features;
	}
	psde->nformats = mdp_get_formats(psde->formats,
		ARRAY_SIZE(psde->formats),
		!(psde->features & BIT(SDE_SSPP_CSC)) ||
		!(psde->features & SDE_SSPP_SCALER));

	type = private_plane ? DRM_PLANE_TYPE_PRIMARY : DRM_PLANE_TYPE_OVERLAY;
	ret = drm_universal_plane_init(dev, plane, 0xff, &sde_plane_funcs,
				psde->formats, psde->nformats,
				type);
	if (ret)
		goto fail;

	drm_plane_helper_add(plane, &sde_plane_helper_funcs);

	_sde_plane_install_properties(plane, &plane->base);

	psde->pipe_hw = sde_hw_sspp_init(pipe, kms->mmio, sde_cat);
	if (IS_ERR(psde->pipe_hw)) {
		ret = PTR_ERR(psde->pipe_hw);
		psde->pipe_hw = NULL;
		goto fail;
	}

	/* save user friendly pipe name for later */
	snprintf(psde->pipe_name, SDE_PIPE_NAME_SIZE, "pipe%u", pipe);

	_sde_plane_init_debugfs(psde, kms);

	DRM_INFO("Successfully created plane for %s\n", psde->pipe_name);
	return plane;

fail:
	pr_err("%s: Plane creation failed\n", __func__);
	if (plane)
		sde_plane_destroy(plane);
exit:
	return ERR_PTR(ret);
}
