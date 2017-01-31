/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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

#include "sde_kms.h"
#include "sde_hwio.h"
#include "sde_hw_mdp_ctl.h"
#include "sde_mdp_formats.h"
#include "sde_hw_sspp.h"

#define DECIMATED_DIMENSION(dim, deci) (((dim) + ((1 << (deci)) - 1)) >> (deci))
#define PHASE_STEP_SHIFT	21
#define PHASE_STEP_UNIT_SCALE   ((int) (1 << PHASE_STEP_SHIFT))
#define PHASE_RESIDUAL		15

#define SDE_PLANE_FEATURE_SCALER \
	(BIT(SDE_SSPP_SCALAR_QSEED2)| \
		BIT(SDE_SSPP_SCALAR_QSEED3)| \
		BIT(SDE_SSPP_SCALAR_RGB))

#ifndef SDE_PLANE_DEBUG_START
#define SDE_PLANE_DEBUG_START()
#endif

#ifndef SDE_PLANE_DEBUG_END
#define SDE_PLANE_DEBUG_END()
#endif

struct sde_plane {
	struct drm_plane base;
	const char *name;

	int mmu_id;

	enum sde_sspp pipe;
	uint32_t features;      /* capabilities from catalog */
	uint32_t flush_mask;    /* used to commit pipe registers */
	uint32_t nformats;
	uint32_t formats[32];

	struct sde_hw_pipe *pipe_hw;
	struct sde_hw_pipe_cfg pipe_cfg;
	struct sde_hw_pixel_ext pixel_ext;
};
#define to_sde_plane(x) container_of(x, struct sde_plane, base)

static bool sde_plane_enabled(struct drm_plane_state *state)
{
	return state->fb && state->crtc;
}

static void sde_plane_set_scanout(struct drm_plane *plane,
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

static void sde_plane_scale_helper(struct drm_plane *plane,
		uint32_t src, uint32_t dst, uint32_t *phase_steps,
		enum sde_hw_filter *filter, struct sde_mdp_format_params *fmt,
		uint32_t chroma_subsampling)
{
	/* calcualte phase steps, leave init phase as zero */
	phase_steps[SDE_SSPP_COMP_LUMA] =
		mult_frac(1 << PHASE_STEP_SHIFT, src, dst);
	phase_steps[SDE_SSPP_COMP_CHROMA] =
		phase_steps[SDE_SSPP_COMP_LUMA] / chroma_subsampling;

	/* calculate scaler config, if necessary */
	if (src != dst) {
		filter[SDE_SSPP_COMP_ALPHA] = (src < dst) ?
				SDE_MDP_SCALE_FILTER_BIL :
				SDE_MDP_SCALE_FILTER_PCMN;

		if (fmt->is_yuv)
			filter[SDE_SSPP_COMP_LUMA] = SDE_MDP_SCALE_FILTER_CA;
		else
			filter[SDE_SSPP_COMP_LUMA] =
					filter[SDE_SSPP_COMP_ALPHA];
	}
}

/* CIFIX: clean up fmt/subsampling params once we're using fourcc formats */
static void _sde_plane_pixel_ext_helper(struct drm_plane *plane,
		uint32_t src, uint32_t dst, uint32_t decimated_src,
		uint32_t *phase_steps, uint32_t *out_src, int *out_edge1,
		int *out_edge2, struct sde_mdp_format_params *fmt,
		uint32_t chroma_subsampling, bool post_compare)
{
	/* CIFIX: adapted from mdss_mdp_pipe_calc_pixel_extn() */
	int64_t edge1, edge2, caf;
	uint32_t src_work;
	int i, tmp;

	if (plane && phase_steps && out_src && out_edge1 && out_edge2 && fmt) {
		/* enable CAF for YUV formats */
		if (fmt->is_yuv)
			caf = PHASE_STEP_UNIT_SCALE;
		else
			caf = 0;

		for (i = 0; i < SDE_MAX_PLANES; i++) {
			src_work = decimated_src;
			if (i == 1 || i == 2)
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
			/* CIFIX: why are we casting first to uint32_t? */
			if (edge1 >= 0) {
				tmp = (uint32_t)edge1;
				tmp >>= PHASE_STEP_SHIFT;
				*(out_edge1 + i) = -tmp;
			} else {
				tmp = (uint32_t)(-edge1);
				*(out_edge1 + i) = (tmp + PHASE_STEP_UNIT_SCALE
						- 1) >> PHASE_STEP_SHIFT;
			}
			if (edge2 >= 0) {
				tmp = (uint32_t)edge2;
				tmp >>= PHASE_STEP_SHIFT;
				*(out_edge2 + i) = -tmp;
			} else {
				tmp = (uint32_t)(-edge2);
				*(out_edge2 + i) = (tmp + PHASE_STEP_UNIT_SCALE
						- 1) >> PHASE_STEP_SHIFT;
			}
		}
	}
}

static int sde_plane_mode_set(struct drm_plane *plane,
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

	SDE_PLANE_DEBUG_START();
	nplanes = drm_format_num_planes(fb->pixel_format);

	pstate = to_sde_plane_state(plane->state);

	format = to_mdp_format(msm_framebuffer_format(fb));
	pix_format = format->base.pixel_format;

	/* src values are in Q16 fixed point, convert to integer */
	src_x = src_x >> 16;
	src_y = src_y >> 16;
	src_w = src_w >> 16;
	src_h = src_h >> 16;

	DBG("%s: FB[%u] %u,%u,%u,%u -> CRTC[%u] %d,%d,%u,%u", psde->name,
			fb->base.id, src_x, src_y, src_w, src_h,
			crtc->base.id, crtc_x, crtc_y, crtc_w, crtc_h);

	/* update format configuration */
	memset(&(psde->pipe_cfg), 0, sizeof(struct sde_hw_pipe_cfg));

	psde->pipe_cfg.src.format = sde_mdp_get_format_params(pix_format,
			0/* CIFIX: fmt_modifier */);
	psde->pipe_cfg.src.width = fb->width;
	psde->pipe_cfg.src.height = fb->height;
	psde->pipe_cfg.src.num_planes = nplanes;

	sde_plane_set_scanout(plane, &psde->pipe_cfg, fb);

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
		sde_plane_scale_helper(plane, src_w, crtc_w,
				pe->phase_step_x,
				pe->horz_filter, fmt, chroma_subsample_h);
		sde_plane_scale_helper(plane, src_h, crtc_h,
				pe->phase_step_y,
				pe->vert_filter, fmt, chroma_subsample_v);

		/* calculate left/right/top/bottom pixel extentions */
		tmp = DECIMATED_DIMENSION(src_w,
				psde->pipe_cfg.horz_decimation);
		if (fmt->is_yuv)
			tmp &= ~0x1;
		_sde_plane_pixel_ext_helper(plane, src_w, crtc_w, tmp,
				pe->phase_step_x,
				pe->roi_w,
				pe->num_ext_pxls_left,
				pe->num_ext_pxls_right, fmt,
				chroma_subsample_h, 0);

		tmp = DECIMATED_DIMENSION(src_h,
				psde->pipe_cfg.vert_decimation);
		_sde_plane_pixel_ext_helper(plane, src_h, crtc_h, tmp,
				pe->phase_step_y,
				pe->roi_h,
				pe->num_ext_pxls_top,
				pe->num_ext_pxls_btm, fmt,
				chroma_subsample_v, 1);

		/* CIFIX: port "Single pixel rgb scale adjustment"? */

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
				&psde->pipe_cfg, 0 /* CIFIX: flags */);
	if (psde->pipe_hw->ops.setup_rects)
		psde->pipe_hw->ops.setup_rects(psde->pipe_hw,
				&psde->pipe_cfg, &psde->pixel_ext);

	/* update csc */

	SDE_PLANE_DEBUG_END();
	return ret;
}

static int sde_plane_prepare_fb(struct drm_plane *plane,
		const struct drm_plane_state *new_state)
{
	struct drm_framebuffer *fb = new_state->fb;
	struct sde_plane *psde = to_sde_plane(plane);

	if (!new_state->fb)
		return 0;

	SDE_PLANE_DEBUG_START();
	SDE_PLANE_DEBUG_END();
	DBG("%s: prepare: FB[%u]", psde->name, fb->base.id);
	return msm_framebuffer_prepare(fb, psde->mmu_id);
}

static void sde_plane_cleanup_fb(struct drm_plane *plane,
		const struct drm_plane_state *old_state)
{
	struct drm_framebuffer *fb = old_state->fb;
	struct sde_plane *psde = to_sde_plane(plane);

	if (!fb)
		return;

	SDE_PLANE_DEBUG_START();
	SDE_PLANE_DEBUG_END();
	DBG("%s: cleanup: FB[%u]", psde->name, fb->base.id);
	msm_framebuffer_cleanup(fb, psde->mmu_id);
}

static int sde_plane_atomic_check(struct drm_plane *plane,
		struct drm_plane_state *state)
{
	struct sde_plane *psde = to_sde_plane(plane);
	struct drm_plane_state *old_state = plane->state;
	const struct mdp_format *format;

	SDE_PLANE_DEBUG_START();
	SDE_PLANE_DEBUG_END();
	DBG("%s: check (%d -> %d)", psde->name,
			sde_plane_enabled(old_state), sde_plane_enabled(state));

	if (sde_plane_enabled(state)) {
		/* CIFIX: don't use mdp format? */
		format = to_mdp_format(msm_framebuffer_format(state->fb));
		if (MDP_FORMAT_IS_YUV(format) &&
			(!(psde->features & SDE_PLANE_FEATURE_SCALER) ||
			 !(psde->features & BIT(SDE_SSPP_CSC)))) {
			dev_err(plane->dev->dev,
				"Pipe doesn't support YUV\n");

			return -EINVAL;
		}

		if (!(psde->features & SDE_PLANE_FEATURE_SCALER) &&
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
			DBG("%s: pixel_format change!", psde->name);
			full_modeset = true;
		}
		if (state->src_w != old_state->src_w) {
			DBG("%s: src_w change!", psde->name);
			full_modeset = true;
		}
		if (to_sde_plane_state(old_state)->pending) {
			DBG("%s: still pending!", psde->name);
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

	DBG("%s: update", sde_plane->name);

	SDE_PLANE_DEBUG_START();
	if (!sde_plane_enabled(state)) {
		to_sde_plane_state(state)->pending = true;
	} else if (to_sde_plane_state(state)->mode_changed) {
		int ret;

		to_sde_plane_state(state)->pending = true;
		ret = sde_plane_mode_set(plane,
				state->crtc, state->fb,
				state->crtc_x, state->crtc_y,
				state->crtc_w, state->crtc_h,
				state->src_x,  state->src_y,
				state->src_w, state->src_h);
		/* atomic_check should have ensured that this doesn't fail */
		WARN_ON(ret < 0);
	} else {
		sde_plane_set_scanout(plane, &sde_plane->pipe_cfg, state->fb);
	}
	SDE_PLANE_DEBUG_END();
}

/* helper to install properties which are common to planes and crtcs */
static void sde_plane_install_properties(struct drm_plane *plane,
		struct drm_mode_object *obj)
{
	struct drm_device *dev = plane->dev;
	struct msm_drm_private *dev_priv = dev->dev_private;
	struct drm_property *prop;

	SDE_PLANE_DEBUG_START();
#define INSTALL_PROPERTY(name, NAME, init_val, fnc, ...) do { \
		prop = dev_priv->plane_property[PLANE_PROP_##NAME]; \
		if (!prop) { \
			prop = drm_property_##fnc(dev, 0, #name, \
				##__VA_ARGS__); \
			if (!prop) { \
				dev_warn(dev->dev, \
					"Create property %s failed\n", \
					#name); \
				return; \
			} \
			dev_priv->plane_property[PLANE_PROP_##NAME] = prop; \
		} \
		drm_object_attach_property(&plane->base, prop, init_val); \
	} while (0)

#define INSTALL_RANGE_PROPERTY(name, NAME, min, max, init_val) \
		INSTALL_PROPERTY(name, NAME, init_val, \
				create_range, min, max)

#define INSTALL_ENUM_PROPERTY(name, NAME, init_val) \
		INSTALL_PROPERTY(name, NAME, init_val, \
				create_enum, name##_prop_enum_list, \
				ARRAY_SIZE(name##_prop_enum_list))

	INSTALL_RANGE_PROPERTY(zpos, ZPOS, 1, 255, 1);

#undef INSTALL_RANGE_PROPERTY
#undef INSTALL_ENUM_PROPERTY
#undef INSTALL_PROPERTY
	SDE_PLANE_DEBUG_END();
}

static int sde_plane_atomic_set_property(struct drm_plane *plane,
		struct drm_plane_state *state, struct drm_property *property,
		uint64_t val)
{
	struct drm_device *dev = plane->dev;
	struct sde_plane_state *pstate;
	struct msm_drm_private *dev_priv = dev->dev_private;
	int ret = 0;

	SDE_PLANE_DEBUG_START();

	pstate = to_sde_plane_state(state);

#define SET_PROPERTY(name, NAME, type) do { \
		if (dev_priv->plane_property[PLANE_PROP_##NAME] == property) { \
			pstate->name = (type)val; \
			DBG("Set property %s %d", #name, (type)val); \
			goto done; \
		} \
	} while (0)

	SET_PROPERTY(zpos, ZPOS, uint8_t);

	dev_err(dev->dev, "Invalid property\n");
	ret = -EINVAL;
done:
	SDE_PLANE_DEBUG_END();
	return ret;
#undef SET_PROPERTY
}

static int sde_plane_set_property(struct drm_plane *plane,
		struct drm_property *property, uint64_t val)
{
	int rc;

	SDE_PLANE_DEBUG_START();
	rc = sde_plane_atomic_set_property(plane, plane->state, property,
		val);
	SDE_PLANE_DEBUG_END();
	return rc;
}

static int sde_plane_atomic_get_property(struct drm_plane *plane,
		const struct drm_plane_state *state,
		struct drm_property *property, uint64_t *val)
{
	struct drm_device *dev = plane->dev;
	struct sde_plane_state *pstate;
	struct msm_drm_private *dev_priv = dev->dev_private;
	int ret = 0;

	SDE_PLANE_DEBUG_START();
	pstate = to_sde_plane_state(state);

#define GET_PROPERTY(name, NAME, type) do { \
		if (dev_priv->plane_property[PLANE_PROP_##NAME] == property) { \
			*val = pstate->name; \
			DBG("Get property %s %lld", #name, *val); \
			goto done; \
		} \
	} while (0)

	GET_PROPERTY(zpos, ZPOS, uint8_t);

	dev_err(dev->dev, "Invalid property\n");
	ret = -EINVAL;
done:
	SDE_PLANE_DEBUG_END();
	return ret;
#undef SET_PROPERTY
}

static void sde_plane_destroy(struct drm_plane *plane)
{
	struct sde_plane *psde = to_sde_plane(plane);

	SDE_PLANE_DEBUG_START();

	if (psde->pipe_hw)
		sde_hw_sspp_destroy(psde->pipe_hw);

	drm_plane_helper_disable(plane);
	drm_plane_cleanup(plane);

	kfree(psde);

	SDE_PLANE_DEBUG_END();
}

static void sde_plane_destroy_state(struct drm_plane *plane,
		struct drm_plane_state *state)
{
	SDE_PLANE_DEBUG_START();
	if (state->fb)
		drm_framebuffer_unreference(state->fb);

	kfree(to_sde_plane_state(state));
	SDE_PLANE_DEBUG_END();
}

static struct drm_plane_state *
sde_plane_duplicate_state(struct drm_plane *plane)
{
	struct sde_plane_state *pstate;

	if (WARN_ON(!plane->state))
		return NULL;

	SDE_PLANE_DEBUG_START();
	pstate = kmemdup(to_sde_plane_state(plane->state),
			sizeof(*pstate), GFP_KERNEL);

	if (pstate && pstate->base.fb)
		drm_framebuffer_reference(pstate->base.fb);

	pstate->mode_changed = false;
	pstate->pending = false;
	SDE_PLANE_DEBUG_END();

	return &pstate->base;
}

static void sde_plane_reset(struct drm_plane *plane)
{
	struct sde_plane_state *pstate;

	SDE_PLANE_DEBUG_START();
	if (plane->state && plane->state->fb)
		drm_framebuffer_unreference(plane->state->fb);

	kfree(to_sde_plane_state(plane->state));
	pstate = kzalloc(sizeof(*pstate), GFP_KERNEL);

	memset(pstate, 0, sizeof(struct sde_plane_state));

	/* assign default blend parameters */
	pstate->alpha = 255;
	pstate->premultiplied = 0;

	if (plane->type == DRM_PLANE_TYPE_PRIMARY)
		pstate->zpos = STAGE_BASE;
	else
		pstate->zpos = STAGE0 + drm_plane_index(plane);

	pstate->base.plane = plane;

	plane->state = &pstate->base;
	SDE_PLANE_DEBUG_END();
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

/* initialize plane */
struct drm_plane *sde_plane_init(struct drm_device *dev, uint32_t pipe,
		bool private_plane)
{
	static const char tmp_name[] = "---";
	struct drm_plane *plane = NULL;
	struct sde_plane *psde;
	struct sde_hw_ctl *sde_ctl;
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

	psde = kzalloc(sizeof(*psde), GFP_KERNEL);
	if (!psde) {
		ret = -ENOMEM;
		goto fail;
	}

	memset(psde, 0, sizeof(*psde));

	plane = &psde->base;

	psde->pipe = pipe;
	psde->name = tmp_name;

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
		!(psde->features & SDE_PLANE_FEATURE_SCALER));

	type = private_plane ? DRM_PLANE_TYPE_PRIMARY : DRM_PLANE_TYPE_OVERLAY;
	ret = drm_universal_plane_init(dev, plane, 0xff, &sde_plane_funcs,
				psde->formats, psde->nformats,
				type);
	if (ret)
		goto fail;

	drm_plane_helper_add(plane, &sde_plane_helper_funcs);

	sde_plane_install_properties(plane, &plane->base);

	psde->pipe_hw = sde_hw_sspp_init(pipe, kms->mmio, sde_cat);
	if (IS_ERR(psde->pipe_hw)) {
		ret = PTR_ERR(psde->pipe_hw);
		psde->pipe_hw = NULL;
		goto fail;
	}

	/* cache flush mask for later */
	sde_ctl = sde_hw_ctl_init(CTL_0, kms->mmio, sde_cat);
	if (!IS_ERR(sde_ctl)) {
		if (sde_ctl->ops.get_bitmask_sspp)
			sde_ctl->ops.get_bitmask_sspp(sde_ctl,
					&psde->flush_mask, pipe);
		sde_hw_ctl_destroy(sde_ctl);
	}

	pr_err("%s: Successfully created plane\n", __func__);
	return plane;

fail:
	pr_err("%s: Plane creation failed\n", __func__);
	if (plane)
		sde_plane_destroy(plane);
exit:
	return ERR_PTR(ret);
}
