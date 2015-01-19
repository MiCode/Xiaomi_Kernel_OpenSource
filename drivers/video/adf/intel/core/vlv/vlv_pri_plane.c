/*
 * Copyright (C) 2014, Intel Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <intel_adf_device.h>
#include <core/intel_dc_config.h>
#include <core/common/intel_dc_regs.h>
#include <core/vlv/vlv_pri_plane.h>
#include <core/vlv/vlv_dc_config.h>
#include <core/vlv/vlv_dc_regs.h>
#include <core/vlv/vlv_pm.h>
#include <drm/i915_drm.h>
#include <drm/drm_rect.h>
#include <video/intel_adf.h>

#define SEC_PLANE_OFFSET 0x1000

struct format_info {
	u32 drm_format;
	u32 hw_config;
	u8 bpp;
};

static const struct format_info format_mapping[] = {
	{
		.drm_format = DRM_FORMAT_C8,
		.hw_config = DISPPLANE_8BPP,
		.bpp = 1,
	},

	{
		.drm_format = DRM_FORMAT_XRGB1555,
		.hw_config = DISPPLANE_BGRX555,
		.bpp = 2,
	},

	{
		.drm_format = DRM_FORMAT_ARGB1555,
		.hw_config = DISPPLANE_BGRA555,
		.bpp = 2,
	},

	{
		.drm_format = DRM_FORMAT_RGB565,
		.hw_config = DISPPLANE_BGRX565,
		.bpp = 2,
	},

	{
		.drm_format = DRM_FORMAT_XRGB8888,
		.hw_config = DISPPLANE_BGRX888,
		.bpp = 4,
	},

	{
		.drm_format = DRM_FORMAT_ARGB8888,
		.hw_config = DISPPLANE_BGRA888,
		.bpp = 4,
	},

	{
		.drm_format = DRM_FORMAT_XBGR2101010,
		.hw_config = DISPPLANE_RGBX101010,
		.bpp = 4,
	},

	{
		.drm_format = DRM_FORMAT_ABGR2101010,
		.hw_config = DISPPLANE_RGBA101010,
		.bpp = 4,
	},

	{
		.drm_format = DRM_FORMAT_XRGB2101010,
		.hw_config = DISPPLANE_BGRX101010,
		.bpp = 4,
	},

	{
		.drm_format = DRM_FORMAT_ARGB2101010,
		.hw_config = DISPPLANE_BGRA101010,
		.bpp = 4,
	},

	{
		.drm_format = DRM_FORMAT_XBGR8888,
		.hw_config = DISPPLANE_RGBX888,
		.bpp = 4,
	},

	{
		.drm_format = DRM_FORMAT_ABGR8888,
		.hw_config = DISPPLANE_RGBA888,
		.bpp = 4,
	},
};

static const u32 pri_supported_formats[] = {
	DRM_FORMAT_C8,
	DRM_FORMAT_XRGB1555,
	DRM_FORMAT_ARGB1555,
	DRM_FORMAT_RGB565,
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_ARGB8888,
	DRM_FORMAT_XBGR8888,
	DRM_FORMAT_ABGR8888,
	DRM_FORMAT_XRGB2101010,
	DRM_FORMAT_ARGB2101010,
	DRM_FORMAT_XBGR2101010,
	DRM_FORMAT_ABGR2101010,
};

#if defined(CONFIG_ADF_INTEL_CHV)
static const u32 pri_supported_transforms[] = {
	INTEL_ADF_TRANSFORM_FLIPH,
	INTEL_ADF_TRANSFORM_ROT180,
};
#else
static const u32 pri_supported_transforms[] = {
	INTEL_ADF_TRANSFORM_ROT180,
};
#endif

static const u32 priB_supported_scalings[] = {
	INTEL_PLANE_SCALING_DOWNSCALING,
	INTEL_PLANE_SCALING_UPSCALING,
};

static const u32 pri_supported_blendings[] = {
	INTEL_PLANE_BLENDING_NONE,
	INTEL_PLANE_BLENDING_PREMULT,
};

static const u32 pri_supported_tiling[] = {
	INTEL_PLANE_TILE_NONE,
	INTEL_PLANE_TILE_X,
};

static int get_format_config(u32 drm_format, u32 *config, u8 *bpp)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(format_mapping); i++) {
		if (format_mapping[i].drm_format == drm_format) {
			*config = format_mapping[i].hw_config;
			*bpp = format_mapping[i].bpp;
			return 0;
		}
	}

	return -EINVAL;
}

static int init_context(struct vlv_pri_plane_context *ctx, u8 idx)
{
	switch (idx) {
	case PRIMARY_PLANE:
		ctx->plane = 0;
		break;
	case SECONDARY_PLANE:
		ctx->plane = 1;
		break;
	case TERTIARY_PLANE:
		ctx->plane = 2;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static void vlv_pri_suspend(struct intel_dc_component *comp)
{
	return;
}

static void vlv_pri_resume(struct intel_dc_component *comp)
{
	return;
}

/*
 * Computes the linear offset to the base tile and adjusts x, y.
 * bytes per pixel is assumed to be a power-of-two.
 */
unsigned long vlv_compute_page_offset(int *x, int *y,
					unsigned int tiling_mode,
					unsigned int cpp,
					unsigned int pitch)
{
	if (tiling_mode != I915_TILING_NONE) {
		unsigned int tile_rows, tiles;

		tile_rows = *y / 8;
		*y %= 8;
		tiles = *x / (512/cpp);
		*x %= 512/cpp;

		return tile_rows * pitch * 8 + tiles * 4096;
	} else {
		unsigned int offset;

		offset = *y * pitch + *x * cpp;
		*y = 0;
		*x = (offset & 4095) / cpp;
		return offset & -4096;
	}
}

static inline struct vlv_pipeline *to_vlv_pipeline_pri_plane(
	struct vlv_pri_plane *plane)
{
	return container_of(plane, struct vlv_pipeline, pplane);
}

static int chv_update_planeb_scaler(struct vlv_pri_plane *pri_plane,
		struct intel_plane_config *config, int *src_x, int *src_y,
		u32 *src_w, u32 *src_h)
{
	struct drm_mode_modeinfo mode;
	struct intel_pipe *intel_pipe = config->pipe;
	struct vlv_pipeline *disp = to_vlv_pipeline_pri_plane(pri_plane);
	struct pri_plane_regs_value *regs = &pri_plane->ctx.regs;
	bool visible;
	int ret = 0;
	int hscale, vscale;
	int max_scale, min_scale;
	int dst_x = config->dst_x;
	int dst_y = config->dst_y;
	u32 dst_w = config->dst_w;
	u32 dst_h = config->dst_h;
	struct drm_rect clip;

	/* sample coordinates in 16.16 fixed point */
	struct drm_rect src;
	struct drm_rect dst = { /* integer pixels */
		.x1 = dst_x,
		.x2 = dst_x + dst_w,
		.y1 = dst_y,
		.y2 = dst_y + dst_h,
	};

	intel_pipe->ops->get_current_mode(intel_pipe, &mode);

	clip.x1 = 0;
	clip.y1 = 0;
	clip.x2 = mode.hdisplay;
	clip.y2 = mode.vdisplay;

	*src_x = *src_x << 16;
	*src_y = *src_y << 16;
	*src_w = *src_w << 16;
	*src_h = *src_h << 16;

	src.x1 = *src_x;
	src.x2 = src.x1 + *src_w;
	src.y1 = *src_y;
	src.y2 = src.y1 + *src_h;

	/*
	 * the following code does a bunch of fuzzy adjustments to the
	 * coordinates and sizes. We probably need some way to decide whether
	 * more strict checking should be done instead.
	 */
	max_scale = pri_plane->mpo_plane->max_downscale << 16;
	min_scale = pri_plane->mpo_plane->can_scale ? 1 : (1 << 16);

	hscale = drm_rect_calc_hscale_relaxed(&src, &dst, min_scale, max_scale);
	BUG_ON(hscale < 0);

	vscale = drm_rect_calc_vscale_relaxed(&src, &dst, min_scale, max_scale);
	BUG_ON(vscale < 0);

	visible = drm_rect_clip_scaled(&src, &dst, &clip, hscale, vscale);

	dst_x = dst.x1;
	dst_y = dst.y1;
	dst_w = drm_rect_width(&dst);
	dst_h = drm_rect_height(&dst);

	if (visible) {

		/* check again in case clipping clamped the results */
		hscale = drm_rect_calc_hscale(&src, &dst, min_scale, max_scale);
		if (hscale < 0) {
			pr_err("%s:hscale factor out of limits\n", __func__);
			drm_rect_debug_print(&src, true);
			drm_rect_debug_print(&dst, false);

			return hscale;
		}

		vscale = drm_rect_calc_vscale(&src, &dst, min_scale, max_scale);
		if (vscale < 0) {
			pr_err("%s:vscale factor out of limits\n", __func__);
			drm_rect_debug_print(&src, true);
			drm_rect_debug_print(&dst, false);

			return vscale;
		}

		/*
		 * Make source viewport size an exact multiple of
		 * scaling factors.
		 */
		drm_rect_adjust_size(&src,
			drm_rect_width(&dst) * hscale - drm_rect_width(&src),
			drm_rect_height(&dst) * vscale - drm_rect_height(&src));

		/* sanity check to make sure the src viewport wasn't enlarged */
		WARN_ON(src.x1 < (int) *src_x ||
			src.y1 < (int) *src_y ||
			src.x2 > (int) (*src_x + *src_w) ||
			src.y2 > (int) (*src_y + *src_h));

		*src_x = src.x1 >> 16;
		*src_w = drm_rect_width(&src) >> 16;
		*src_y = src.y1 >> 16;
		*src_h = drm_rect_height(&src) >> 16;
		ret = chv_program_plane_scaler(disp, pri_plane->mpo_plane,
					       *src_w, *src_h, dst_w, dst_h);

		if (ret)
			return ret;

		/* Position plane output into pipe src area */
		regs->pos = (dst_y << 16) | dst_x;
		regs->size = ((dst_h - 1) << 16) | (dst_w - 1);
		pri_plane->window_updated = true;
		*src_x = src.x1 >> 16;
		*src_w = drm_rect_width(&src) >> 16;
		*src_y = src.y1 >> 16;
		*src_h = drm_rect_height(&src) >> 16;
	}
	return ret;
}

static int vlv_pri_calculate(struct intel_plane *plane,
		struct intel_buffer *buf,
		struct intel_plane_config *config)
{
	struct vlv_pri_plane *pri_plane = to_vlv_pri_plane(plane);
	struct vlv_pipeline *pipeline = to_vlv_pipeline_pri_plane(pri_plane);
	struct vlv_dc_config *vlv_config = pipeline->config;
	struct vlv_pm *pm = &pipeline->pm;
	struct pri_plane_regs_value *regs = &pri_plane->ctx.regs;
	struct intel_pipe *intel_pipe = config->pipe;
	struct drm_mode_modeinfo mode;
	unsigned long dspaddr_offset;
	int plane_ddl, prec_multi, plane_prec_multi;
	int src_x, src_y;
	u32 src_w, src_h;
	int pipe = intel_pipe->base.idx;
	u32 dspcntr;
	u32 mask;
	u32 pidx = pri_plane->ctx.plane;
	u32 format_config = 0;
	u8 alpha = 0x0;
	bool cons_alpha = 0;
	u8 bpp = 0, prev_bpp = 0;
	u8 i = 0;

	get_format_config(buf->format, &format_config, &bpp);

	if ((buf->format == DRM_FORMAT_XRGB8888) ||
				(buf->format == DRM_FORMAT_ARGB8888))
		pri_plane->ctx.pri_plane_bpp = 24;

	src_x = config->src_x;
	src_y = config->src_y;

	/* By default fb widht is the src width */
	src_w = buf->w;
	src_h = buf->h;

	regs->dspcntr = REG_READ(DSPCNTR(pidx));
	regs->dspcntr |= DISPLAY_PLANE_ENABLE;
	dspcntr = regs->dspcntr;

	regs->dspcntr &= ~DISPPLANE_PIXFORMAT_MASK;
	regs->dspcntr |= format_config;
	/* Calculate the ddl if there is a change in bpp */
	for (i = 0; i < ARRAY_SIZE(format_mapping); i++) {
		if (format_mapping[i].hw_config ==
				(dspcntr & DISPPLANE_PIXFORMAT_MASK)) {
			prev_bpp = format_mapping[i].bpp;
			break;
		}
	}
	mask = DDL_PLANEA_MASK;

	if (bpp != prev_bpp || !(REG_READ(VLV_DDL(pipe)) & mask)) {
		/*FIXME: get mode from interface itself */
		intel_pipe->ops->get_current_mode(intel_pipe, &mode);
		if (mode.clock && bpp) {
			vlv_calc_ddl(mode.clock, bpp, &prec_multi,
					&plane_ddl);
			/* FIXME : Please add DDL_PRECISION_L and H for VLV */
			plane_prec_multi = (prec_multi ==
					DDL_PRECISION_L) ?
				DDL_PLANE_PRECISION_L :
				DDL_PLANE_PRECISION_H;

			plane_ddl = plane_prec_multi | (plane_ddl);

			/* save the ddl in pm object to flush later */
			vlv_pm_save_values(pm, true, false, false, plane_ddl);
			/*
			 * FIXME: Now currently drain latency is set to zero.
			 * this is should be fixed in future.
			 */
		} else {
			pr_err("ADF: %s: Skipping DDL(clock=%u bpp=%u)\n",
					__func__, mode.clock, bpp);
		}
		REG_WRITE_BITS(VLV_DDL(pipe), 0x00, mask);
	}

	if (buf->tiling_mode != I915_TILING_NONE)
		regs->dspcntr |= DISPPLANE_TILED;
	else
		regs->dspcntr &= ~DISPPLANE_TILED;

	/* when in maxfifo display control register cannot be modified */
	if (vlv_config->status.maxfifo_enabled && regs->dspcntr != dspcntr) {
		REG_WRITE(FW_BLC_SELF_VLV, ~FW_CSPWRDWNEN);
		vlv_config->status.maxfifo_enabled = false;
		pipeline->status.wait_vblank = true;
		pipeline->status.vsync_counter =
			intel_pipe->ops->get_vsync_counter(intel_pipe, 0);
	}

	/* Setting Canvas Color */
	if ((intel_adf_get_platform_id() == gen_cherryview) &&
	    STEP_FROM(pipeline->dc_stepping, STEP_B0) && (pipe == PIPE_B) &&
	    (pri_plane->canvas_updated == true)) {
		uint64_t color = 0;

		/* BGR 8bpc ==> RGB 10bpc */
		for (i = 0; i < 3; i++)
			color |= ((((pri_plane->canvas_col >> (i*8)) &
				    0xFF) * 1023/255) << ((2-i)*10));
		regs->canvas_col = color;
	}

	/*
	 * Constant Alpha and PreMul alpha setting.
	 */
	if ((intel_adf_get_platform_id() == gen_cherryview) &&
		    STEP_FROM(pipeline->dc_stepping, STEP_B0) &&
		    (pipe == PIPE_B)) {

		alpha = (config->alpha & 0xFF);

		if (config->blending == INTEL_PLANE_BLENDING_COVERAGE)
			cons_alpha = true;
		else
			cons_alpha = false;

		if (alpha != REG_READ(CHT_PRIMB_CONSTALPHA)) {
			regs->const_alpha = (cons_alpha ? (1 << 31) : 0) |
				alpha;
			regs->blend = cons_alpha ? CHT_PIPE_B_BLEND_ANDROID :
				CHT_PIPE_B_BLEND_LEGACY;
			pri_plane->alpha_updated = true;
			pri_plane->blend_updated = true;
		}
	}

	/* Position the src/dst rectangels */
	regs->stride = buf->stride;
	regs->linearoff = src_y * regs->stride + src_x * bpp;
	dspaddr_offset = vlv_compute_page_offset(&src_x, &src_y,
				buf->tiling_mode, bpp, regs->stride);
	regs->linearoff -= dspaddr_offset;
	regs->tileoff = (src_y << 16) | src_x;

	/*
	 * H mirroring available on PIPE B Pri and sp plane only
	 * For CHV, FLIPH and 180 are mutually exclusive
	 */
	if ((intel_adf_get_platform_id() == gen_cherryview) &&
	    STEP_FROM(pipeline->dc_stepping, STEP_B0))
		regs->dspcntr &= ~(DISPPLANE_H_MIRROR_ENABLE |
				   DISPPLANE_180_ROTATION_ENABLE);
	else
		regs->dspcntr &= ~DISPPLANE_180_ROTATION_ENABLE;

	switch (config->transform) {
	case INTEL_ADF_TRANSFORM_FLIPH:
		if ((intel_adf_get_platform_id() == gen_cherryview) &&
		    STEP_FROM(pipeline->dc_stepping, STEP_B0) &&
		    (pipe == PIPE_B)) {
			regs->dspcntr |= DISPPLANE_H_MIRROR_ENABLE;
			regs->tileoff = (src_y << 16) | (src_x + src_w - 1);
			regs->linearoff += ((src_w - 1) * bpp);
		}
		break;
	case INTEL_ADF_TRANSFORM_ROT180:
		regs->dspcntr |= DISPPLANE_180_ROTATION_ENABLE;
		regs->linearoff =  regs->linearoff + (buf->h - 1) *
			regs->stride + buf->w * bpp;
		regs->tileoff = (((src_y + src_h - 1) << 16) |
				 (src_x + src_w - 1));
		break;
	}

	/*
	 * Check scaler availabity and set it.
	 * PRIVATE_4 used for MPO related
	 */
	if ((intel_adf_get_platform_id() == gen_cherryview) &&
	    STEP_FROM(pipeline->dc_stepping, STEP_B0) && (pipe == PIPE_B) &&
	    (config->flags & INTEL_ADF_PLANE_HW_PRIVATE_4)
			== INTEL_ADF_PLANE_HW_PRIVATE_4) {

		chv_chek_save_scale(pri_plane->mpo_plane, config);

		if (config->src_w && config->src_h) {

			/*
			 * Use src width and height from posted config
			 * NOTE: current HWC dont send correct val
			 */
			src_w = (config->src_w) >> 16;
			src_h = (config->src_h) >> 16;
		}

		/* program scaler */
		if (chv_update_planeb_scaler(pri_plane, config,
						     &src_x, &src_y,
						     &src_w, &src_h))
			pr_err("%s :scaler programming failed.\n", __func__);
	}

	regs->surfaddr = (buf->gtt_offset_in_pages + dspaddr_offset);
	return 0;
}

static int vlv_pri_attach(struct intel_plane *plane, struct intel_pipe *pipe)
{
	/* attach the requested plane to pipe */
	plane->pipe = pipe;

	return 0;
}

static int vlv_pri_validate(struct intel_plane *plane,
		struct intel_buffer *buf,
		struct intel_plane_config *config)
{
	return vlv_pri_calculate(plane, buf, config);
}

static void vlv_pri_flip(struct intel_plane *plane,
		struct intel_buffer *buf,
		struct intel_plane_config *config)
{
	struct vlv_pri_plane *pri_plane = to_vlv_pri_plane(plane);
	struct pri_plane_regs_value *regs = &pri_plane->ctx.regs;
	struct vlv_pipeline *pipeline = to_vlv_pipeline_pri_plane(pri_plane);
	struct vlv_dc_config *vlv_config = pipeline->config;

	REG_WRITE(pri_plane->stride_offset, regs->stride);
	REG_WRITE(pri_plane->tiled_offset, regs->tileoff);
	REG_WRITE(pri_plane->linear_offset, regs->linearoff);
	if (pri_plane->mpo_plane != NULL) {
		if (pri_plane->mpo_plane->reprogram_scaler) {
			REG_WRITE(pri_plane->mpo_plane->scaler_reg,
				  pri_plane->mpo_plane->scaler_en);
			if (pri_plane->window_updated) {
				REG_WRITE(CHT_PRIMB_POS, regs->pos);
				REG_WRITE(CHT_PRIMB_SIZE, regs->size);
				pri_plane->window_updated = false;
			}
			pri_plane->mpo_plane->reprogram_scaler = false;
		}
	}
	if (pri_plane->canvas_updated) {
		REG_WRITE(CHT_PIPE_B_CANVAS_REG, regs->canvas_col);
		pri_plane->canvas_updated = false;
	}
	if (pri_plane->alpha_updated) {
		REG_WRITE(CHT_PRIMB_CONSTALPHA, regs->const_alpha);
		pri_plane->alpha_updated = false;
	}
	if (pri_plane->blend_updated) {
		REG_WRITE(CHT_PIPEB_BLEND_CONFIG, regs->blend);
		pri_plane->blend_updated = false;
	}


	REG_WRITE(pri_plane->offset, regs->dspcntr);
	I915_MODIFY_DISPBASE(pri_plane->surf_offset, regs->surfaddr);
	REG_POSTING_READ(pri_plane->surf_offset);
	pri_plane->enabled = true;
	vlv_update_plane_status(&vlv_config->base, plane->base.idx, true);

	return;
}

static inline void vlv_adf_flush_disp_plane(struct vlv_pri_plane *plane)
{
	REG_WRITE(plane->surf_offset, REG_READ(plane->surf_offset));
}

bool vlv_pri_is_enabled(struct vlv_pri_plane *plane)
{
	return plane->enabled;
}

/*
 * called during modeset, where we just configure params without
 * enabling plane
 */
int vlv_pri_update_params(struct vlv_pri_plane *plane,
		struct vlv_plane_params *params)
{
	u32 value;

	value = REG_READ(plane->offset);

	/*
	 * FIXME: need to update rotation value based on need
	 * disable rotation for now
	 */
	value &= ~(1 << 15);

	value |= DISPPLANE_GAMMA_ENABLE;

	REG_WRITE(plane->offset, value);

	return 0;
}

static int vlv_pri_enable(struct intel_plane *intel_plane)
{
	struct vlv_pri_plane *plane = to_vlv_pri_plane(intel_plane);
	struct vlv_pipeline *pipeline = to_vlv_pipeline_pri_plane(plane);
	struct vlv_dc_config *vlv_config = pipeline->config;
	u32 value;

	value = REG_READ(plane->offset);
	if (value & DISPLAY_PLANE_ENABLE) {
		dev_dbg(plane->base.base.dev, "%splane already enabled\n",
				__func__);
		return 0;
	}

	/*
	 * FIXME: need to update rotation value based on need
	 * disable rotation for now
	 */
	value &= ~(1 << 15);

	plane->enabled = true;
	REG_WRITE(plane->offset, value | DISPLAY_PLANE_ENABLE);
	vlv_adf_flush_disp_plane(plane);
	vlv_update_plane_status(&vlv_config->base, intel_plane->base.idx, true);

	/*
	 * TODO:No need to wait in case of mipi.
	 * Since data will flow only when port is enabled.
	 * wait for vblank will time out for mipi
	 *
	 * update for HDMI later
	 */
	return 0;
}

static int vlv_pri_disable(struct intel_plane *intel_plane)
{
	struct vlv_pri_plane *plane = to_vlv_pri_plane(intel_plane);
	struct vlv_pipeline *pipeline = to_vlv_pipeline_pri_plane(plane);
	struct vlv_dc_config *vlv_config = pipeline->config;
	u32 value;
	u32 mask = DDL_PLANEA_MASK;

	value = REG_READ(plane->offset);
	if ((value & DISPLAY_PLANE_ENABLE) == 0) {
		dev_dbg(plane->base.base.dev, "%splane already disabled\n",
				__func__);
		return 0;
	}

	plane->enabled = false;
	REG_WRITE(plane->offset, value & ~DISPLAY_PLANE_ENABLE);
	vlv_adf_flush_disp_plane(plane);
	vlv_update_plane_status(&vlv_config->base,
					intel_plane->base.idx, false);
	REG_WRITE_BITS(VLV_DDL(plane->base.base.idx), 0x00, mask);
	return 0;
}



static const struct intel_plane_capabilities vlv_pri_caps = {
	.supported_formats = pri_supported_formats,
	.n_supported_formats = ARRAY_SIZE(pri_supported_formats),
	.supported_blendings = pri_supported_blendings,
	.n_supported_blendings = ARRAY_SIZE(pri_supported_blendings),
	.supported_transforms = pri_supported_transforms,
	.n_supported_transforms = ARRAY_SIZE(pri_supported_transforms),
	.supported_scalings = NULL,
	.n_supported_scalings = 0,
	.supported_decompressions = NULL,
	.n_supported_decompressions = 0,
	.supported_tiling = pri_supported_tiling,
	.n_supported_tiling = ARRAY_SIZE(pri_supported_tiling),
	.supported_zorder = NULL,
	.n_supported_zorder = 0,
	.supported_reservedbit = NULL,
	.n_supported_reservedbit = 0,
};

static const struct intel_plane_capabilities chv_pri_b_caps = {
	.supported_formats = pri_supported_formats,
	.n_supported_formats = ARRAY_SIZE(pri_supported_formats),
	.supported_blendings = pri_supported_blendings,
	.n_supported_blendings = ARRAY_SIZE(pri_supported_blendings),
	.supported_transforms = pri_supported_transforms,
	.n_supported_transforms = ARRAY_SIZE(pri_supported_transforms),
	.supported_scalings = priB_supported_scalings,
	.n_supported_scalings = ARRAY_SIZE(priB_supported_scalings),
	.supported_decompressions = NULL,
	.n_supported_decompressions = 0,
	.supported_tiling = pri_supported_tiling,
	.n_supported_tiling = ARRAY_SIZE(pri_supported_tiling),
	.supported_zorder = NULL,
	.n_supported_zorder = 0,
	.supported_reservedbit = NULL,
	.n_supported_reservedbit = 0,
};

static struct intel_plane_ops vlv_pri_ops = {
	.base = {
		.suspend = vlv_pri_suspend,
		.resume = vlv_pri_resume,
	},
	.adf_ops = {
		.base = {
			.ioctl = intel_overlay_engine_obj_ioctl,
		},
		.supported_formats = pri_supported_formats,
		.n_supported_formats = ARRAY_SIZE(pri_supported_formats),
	},
	.attach = vlv_pri_attach,
	.validate = vlv_pri_validate,
	.flip = vlv_pri_flip,
	.enable = vlv_pri_enable,
	.disable = vlv_pri_disable,
};

int vlv_pri_plane_init(struct vlv_pri_plane *pplane,
		struct intel_pipeline *pipeline, struct device *dev, u8 idx)
{
	struct vlv_pri_plane_context *ctx;
	struct vlv_pipeline *disp = NULL;
	int pipe = -EIO;
	int err;

	if (!pplane) {
		dev_err(dev, "%s: struct NULL\n", __func__);
		return -EINVAL;
	}
	ctx = &pplane->ctx;
	err = init_context(ctx, idx);
	if (err) {
		pr_err("%s: plane context initialization failed\n", __func__);
		return err;
	}

	pplane->offset = DSPCNTR(ctx->plane);
	pplane->surf_offset = DSPSURF(ctx->plane);
	pplane->stride_offset = DSPSTRIDE(ctx->plane);
	pplane->tiled_offset = DSPTILEOFF(ctx->plane);
	pplane->linear_offset = DSPLINOFF(ctx->plane);

	disp =  to_vlv_pipeline_pri_plane(pplane);
	pipe = disp->pipe.pipe_id;

	/* Allocate and initiate mpo struct */
	if ((intel_adf_get_platform_id() == gen_cherryview) &&
	    STEP_FROM(disp->dc_stepping, STEP_B0) && (pipe == PIPE_B)) {

		pplane->mpo_plane = (struct vlv_mpo_plane *)
			kzalloc(sizeof(struct vlv_mpo_plane), GFP_KERNEL);

		pplane->mpo_plane->can_scale = true;
		pplane->mpo_plane->max_downscale = CHV_MAX_DOWNSCALE;
		pplane->mpo_plane->is_primary_plane = true;

		return intel_adf_plane_init(&pplane->base, dev, idx,
					    &chv_pri_b_caps, &vlv_pri_ops,
					    "primary_plane");
	}

	return intel_adf_plane_init(&pplane->base, dev, idx, &vlv_pri_caps,
			&vlv_pri_ops, "primary_plane");
}

void vlv_pri_plane_destroy(struct vlv_pri_plane *plane)
{
	if (plane)
		intel_plane_destroy(&plane->base);
	memset(&plane->ctx, 0, sizeof(plane->ctx));
}
