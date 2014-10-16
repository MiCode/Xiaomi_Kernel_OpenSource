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

#include <drm/i915_drm.h>
#include <video/intel_adf.h>

#include "intel_adf_device.h"
#include "core/common/intel_dc_regs.h"
#include "core/common/dsi/dsi_pipe.h"
#include "core/vlv/vlv_dc_regs.h"
#include "core/vlv/vlv_dc_config.h"
#include "core/intel_dc_config.h"
#include "core/vlv/vlv_sp_plane.h"
#include "core/vlv/vlv_pri_plane.h"

struct format_info {
	u32 drm_format;
	u32 hw_config;
	u8 bpp;
};

static const struct format_info format_mappings[] = {
	{
		.drm_format = DRM_FORMAT_YUYV,
		.hw_config = SP_FORMAT_YUV422 | SP_YUV_ORDER_YUYV,
		.bpp = 2,
	},

	{
		.drm_format = DRM_FORMAT_YVYU,
		.hw_config = SP_FORMAT_YUV422 | SP_YUV_ORDER_YVYU,
		.bpp = 2,
	},

	{
		.drm_format = DRM_FORMAT_UYVY,
		.hw_config = SP_FORMAT_YUV422 | SP_YUV_ORDER_UYVY,
		.bpp = 2,
	},

	{

		.drm_format = DRM_FORMAT_VYUY,
		.hw_config = SP_FORMAT_YUV422 | SP_YUV_ORDER_VYUY,
		.bpp = 2,
	},

	{
		.drm_format = DRM_FORMAT_C8,
		.hw_config = DISPPLANE_8BPP,
		.bpp = 1,
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

static void vlv_adf_flush_sp_plane(u32 pipe, u32 plane)
{
	REG_WRITE(SPSURF(pipe, plane), REG_READ(SPSURF(pipe, plane)));
}

static int context_init(struct vlv_sp_plane_context *ctx, u8 idx)
{
	switch (idx) {
	case SPRITE_A:
		ctx->plane = 0;
		ctx->pipe = 0;
		break;
	case SPRITE_B:
		ctx->plane = 1;
		ctx->pipe = 0;
		break;
	case SPRITE_C:
		ctx->plane = 0;
		ctx->pipe = 1;
		break;
	case SPRITE_D:
		ctx->plane = 1;
		ctx->pipe = 1;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static void context_destroy(struct vlv_sp_plane_context *ctx)
{
	return;
}

static int get_format_config(u32 drm_format, u32 *format, u32 *bpp)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(format_mappings); i++) {
		if (format_mappings[i].drm_format == drm_format) {
			*format = format_mappings[i].hw_config;
			*bpp = format_mappings[i].bpp;
			return 0;
		}
	}

	return -EINVAL;
}

static void vlv_sp_suspend(struct intel_dc_component *component)
{
	return;
}

static void vlv_sp_resume(struct intel_dc_component *component)
{
	return;
}

/**
 * rect_clip_scaled - perform a scaled clip operation
 * @src: source window rectangle
 * @dst: destination window rectangle
 * @clip: clip rectangle
 * @hscale: horizontal scaling factor
 * @vscale: vertical scaling factor
 *
 * Clip rectangle @dst by rectangle @clip. Clip rectangle @src by the
 * same amounts multiplied by @hscale and @vscale.
 *
 * RETURNS:
 * %true if rectangle @dst is still visible after being clipped,
 * %false otherwise
*/

static bool rect_clip_scaled(struct rectangle *src, struct rectangle *dst,
			const struct rectangle *clip)
{
	int diff;
	int width, height;

	diff = clip->x1 - dst->x1;
	if (diff > 0) {
		int64_t tmp = src->x1 + (int64_t) diff;
		src->x1 = clamp_t(int64_t, tmp, INT_MIN, INT_MAX);
	}
	diff = clip->y1 - dst->y1;
	if (diff > 0) {
		int64_t tmp = src->y1 + (int64_t) diff;
		src->y1 = clamp_t(int64_t, tmp, INT_MIN, INT_MAX);
	}
	diff = dst->x2 - clip->x2;
	if (diff > 0) {
		int64_t tmp = src->x2 - (int64_t) diff;
		src->x2 = clamp_t(int64_t, tmp, INT_MIN, INT_MAX);
	}
	diff = dst->y2 - clip->y2;
	if (diff > 0) {
		int64_t tmp = src->y2 - (int64_t) diff;
		src->y2 = clamp_t(int64_t, tmp, INT_MIN, INT_MAX);
	}

	dst->x1 = max(dst->x1, clip->x1);
	dst->y1 = max(dst->y1, clip->y1);
	dst->x2 = min(dst->x2, clip->x2);
	dst->y2 = min(dst->y2, clip->y2);

	width = dst->x2 - dst->x1;
	height = dst->y2 - dst->y1;

	return width > 0 && height > 0;
}

static bool format_is_yuv(uint32_t format)
{
	switch (format) {
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_UYVY:
	case DRM_FORMAT_VYUY:
	case DRM_FORMAT_YVYU:
		return true;
	default:
		return false;
	}
}

static int vlv_sp_calculate(struct intel_plane *planeptr,
			    struct intel_buffer *buf,
			    struct intel_plane_config *config)
{
	struct vlv_sp_plane *splane = to_vlv_sp_plane(planeptr);
	struct sp_plane_regs_value *regs = &splane->ctx.regs;
	struct intel_pipe *intel_pipe = config->pipe;
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(intel_pipe);
	struct drm_mode_modeinfo mode;
	unsigned long sprsurf_offset, linear_offset;
	int sprite_ddl, prec_multi, sp_prec_multi;
	int plane = splane->ctx.plane;
	int pipe = splane->ctx.pipe;
	int s1_zorder, s1_bottom, s2_zorder, s2_bottom;
	int order = config->zorder & 0x000F;
	u32 hw_format = 0;
	u32 bpp = 0, prev_bpp = 0;
	u32 sprctl, prev_sprctl;
	u32 mask, shift;
	u32 dst_w = (config->dst_w & VLV_SP_12BIT_MASK) - 1;
	u32 dst_h = (config->dst_h & VLV_SP_12BIT_MASK) - 1;
	u32 src_x = config->src_x & VLV_SP_12BIT_MASK;
	u32 src_y = config->src_y & VLV_SP_12BIT_MASK;
	u32 dst_x = config->dst_x & VLV_SP_12BIT_MASK;
	u32 dst_y = config->dst_y & VLV_SP_12BIT_MASK;
	u8 i = 0;

	/* Z-order */
	s1_zorder = (order >> 3) & 0x1;
	s1_bottom = (order >> 2) & 0x1;
	s2_zorder = (order >> 1) & 0x1;
	s2_bottom = (order >> 0) & 0x1;

	get_format_config(buf->format, &hw_format, &bpp);
	sprctl = REG_READ(SPCNTR(pipe, plane));
	prev_sprctl = sprctl;

	if (plane == 0) {
		if (s1_zorder)
			sprctl |= SPRITE_ZORDER_ENABLE;
		else
			sprctl &= ~SPRITE_ZORDER_ENABLE;

		if (s1_bottom)
			sprctl |= SPRITE_FORCE_BOTTOM;
		else
			sprctl &= ~SPRITE_FORCE_BOTTOM;
	} else {
		if (s2_zorder)
			sprctl |= SPRITE_ZORDER_ENABLE;
		else
			sprctl &= ~SPRITE_ZORDER_ENABLE;
		if (s2_bottom)
			sprctl |= SPRITE_FORCE_BOTTOM;
		else
			sprctl &= ~SPRITE_FORCE_BOTTOM;
	}

	/* Mask out pixel format bits in case we change it */
	sprctl &= ~SP_PIXFORMAT_MASK;
	sprctl &= ~SP_YUV_BYTE_ORDER_MASK;
	sprctl &= ~SP_TILED;

	sprctl |= hw_format;
	sprctl |= SP_GAMMA_ENABLE;
	/* Calculate the ddl if there is a change in bpp */
	for (i = 0; i < ARRAY_SIZE(format_mappings); i++) {
		if (format_mappings[i].hw_config ==
				(prev_sprctl & SP_PIXFORMAT_MASK)) {
			prev_bpp = format_mappings[i].bpp;
			break;
		}
	}
	if (plane == 0) {
		mask = DDL_SPRITEA_MASK;
		shift = DDL_SPRITEA_SHIFT;
	} else {
		mask = DDL_SPRITEB_MASK;
		shift = DDL_SPRITEB_SHIFT;
	}
	if (bpp != prev_bpp || !(REG_READ(VLV_DDL(pipe)) & mask)) {
		dsi_pipe->panel->ops->get_config_mode(&dsi_pipe->config,
				&mode);
		vlv_calculate_ddl(mode.clock, bpp, &prec_multi,
				&sprite_ddl);
		sp_prec_multi = (prec_multi ==
					DRAIN_LATENCY_PRECISION_32) ?
					DDL_PLANE_PRECISION_32 :
					DDL_PLANE_PRECISION_64;
		sprite_ddl = (sp_prec_multi | sprite_ddl) << shift;
		if (plane == 0) {
			intel_pipe->regs.sp1_ddl = sprite_ddl;
			intel_pipe->regs.sp1_ddl_mask = mask;
		} else {
			intel_pipe->regs.sp2_ddl = sprite_ddl;
			intel_pipe->regs.sp2_ddl_mask = mask;
		}
		REG_WRITE_BITS(VLV_DDL(pipe), 0x00, mask);
	}

	sprctl |= SP_ENABLE;
	regs->dspcntr = sprctl;
	/* when in maxfifo display control register cannot be modified */
	if (intel_pipe->status.maxfifo_enabled &&
					regs->dspcntr != prev_sprctl) {
		REG_WRITE(FW_BLC_SELF_VLV, ~FW_CSPWRDWNEN);
		intel_pipe->status.maxfifo_enabled = false;
		intel_pipe->status.wait_vblank = true;
		intel_pipe->status.vsync_counter =
				intel_pipe->ops->get_vsync_counter(intel_pipe,
								   0);
	}
	linear_offset = src_y * buf->stride + src_x * bpp;
	sprsurf_offset = vlv_compute_page_offset(&src_x, &src_y,
			buf->tiling_mode, bpp, buf->stride);
	linear_offset -= sprsurf_offset;

	regs->stride = buf->stride;
	regs->pos = ((dst_y << 16) | dst_x);
	regs->size = (dst_h << 16) | dst_w;
	regs->surfaddr = (buf->gtt_offset_in_pages + sprsurf_offset);

	if (buf->tiling_mode != I915_TILING_NONE) {
		regs->dspcntr |= SP_TILED;
		if (config->transform & INTEL_ADF_TRANSFORM_ROT180) {
			regs->dspcntr |= DISPPLANE_180_ROTATION_ENABLE;
			regs->tileoff = ((src_y + dst_h) << 16) |
							(src_x + dst_w);
		} else {
			regs->dspcntr &= ~DISPPLANE_180_ROTATION_ENABLE;
			regs->tileoff = (src_y << 16) | src_x;
		}
	} else {
		if (config->transform & INTEL_ADF_TRANSFORM_ROT180) {
			regs->dspcntr |= DISPPLANE_180_ROTATION_ENABLE;
			regs->linearoff = linear_offset + dst_h *
					regs->stride + (dst_w + 1) * bpp;
		} else {
			regs->dspcntr &= ~DISPPLANE_180_ROTATION_ENABLE;
			regs->linearoff = linear_offset;
		}
	}
	return 0;
}

static int vlv_sp_attach(struct intel_plane *plane, struct intel_pipe *pipe)
{
	/* attach the requested plane to pipe */
	plane->pipe = pipe;

	return 0;
}

static int vlv_sp_validate(struct intel_plane *plane, struct intel_buffer *buf,
		struct intel_plane_config *config)
{
	struct dsi_pipe *dsi_pipe;
	struct drm_mode_modeinfo *mode;
	u32 format_config, bpp;
	bool visible = false;
	struct rectangle clip;

	struct rectangle src = {
		/* sample coordinates in 16.16 fixed point */
		.x1 = config->src_x,
		.x2 = config->src_x + config->src_w,
		.y1 = config->src_y,
		.y2 = config->src_y + config->src_h,
	};

	struct rectangle dst = {
		/* integer pixels */
		.x1 = config->dst_x,
		.x2 = config->dst_x + config->dst_w,
		.y1 = config->dst_y,
		.y2 = config->dst_y + config->dst_h,
	};

	if (config->pipe->type == INTEL_PIPE_DSI) {
		dsi_pipe = to_dsi_pipe(config->pipe);
		mode = &dsi_pipe->config.perferred_mode;
	} else {
		/* handle HDMI pipe later */
		return -EINVAL;
	}

	clip.x1 = 0;
	clip.y1 = 0;
	clip.x2 = mode->hdisplay;
	clip.y2 = mode->vdisplay;

	if (get_format_config(buf->format, &format_config, &bpp)) {
		pr_err("ADF: pixel format not supported %s\n", __func__);
		return -EINVAL;
	}

	/* check buf limits */
	if (buf->w < 3 || buf->h < 3 || buf->stride > 16384) {
		pr_err("ADF: Unsutable fb for the plane %s\n", __func__);
		return -EINVAL;
	}

	/* sprite planes can be linear or x-tiled surfaces */
	if (buf->tiling_mode != I915_TILING_NONE &&
		buf->tiling_mode != I915_TILING_X) {
		pr_err("ADF: unsupported tiling mode %s\n", __func__);
		return -EINVAL;
	}

	visible = rect_clip_scaled(&src, &dst, &clip);

	config->dst_x = dst.x1;
	config->dst_y = dst.y1;
	config->dst_w = (dst.x2 - dst.x1);
	config->dst_h = (dst.y2 - dst.y1);

	if (visible) {
		/* sanity check to make sure the src viewport wasn't enlarged */
		WARN_ON(src.x1 < (int) config->src_x ||
			src.y1 < (int) config->src_y ||
			src.x2 > (int) (config->src_x + config->src_w) ||
			src.y2 > (int) (config->src_y + config->src_h));

		/*
		 * Hardware doesn't handle subpixel coordinates.
		 * Adjust to (macro)pixel boundary
		 */
		config->src_x = src.x1;
		config->src_w = (src.x2 - src.x1);
		config->src_y = src.y1;
		config->src_h = (src.y2 - src.y1);

		if (format_is_yuv(buf->format)) {
			config->src_x &= ~1;
			config->src_w &= ~1;
			config->dst_w &= ~1;
		}
	} else {
		pr_err("ADF: plane is not visible %s\n", __func__);
		return -EINVAL;
	}

	return vlv_sp_calculate(plane, buf, config);
}

static void vlv_sp_flip(struct intel_plane *planeptr, struct intel_buffer *buf,
			struct intel_plane_config *config)
{
	struct vlv_sp_plane *splane = to_vlv_sp_plane(planeptr);
	struct sp_plane_regs_value *regs = &splane->ctx.regs;
	int plane = splane->ctx.plane;
	int pipe = splane->ctx.pipe;

	REG_WRITE(SPSTRIDE(pipe, plane), regs->stride);
	REG_WRITE(SPPOS(pipe, plane), regs->pos);
	REG_WRITE(SPTILEOFF(pipe, plane), regs->tileoff);
	REG_WRITE(SPLINOFF(pipe, plane), regs->linearoff);
	REG_WRITE(SPSIZE(pipe, plane), regs->size);
	REG_WRITE(SPCNTR(pipe, plane), regs->dspcntr);
	I915_MODIFY_DISPBASE(SPSURF(pipe, plane), regs->surfaddr);
	REG_POSTING_READ(SPSURF(pipe, plane));
	vlv_update_plane_status(config->pipe,
			plane ? VLV_SPRITE2 : VLV_SPRITE1, true);

	return;
}

static int vlv_sp_enable(struct intel_plane *planeptr)
{
	u32 reg, value;
	struct vlv_sp_plane *splane = to_vlv_sp_plane(planeptr);
	int plane = splane->ctx.plane;
	int pipe = splane->ctx.pipe;

	reg = SPCNTR(pipe, plane);
	value = REG_READ(reg);
	if (value & DISPLAY_PLANE_ENABLE) {
		return 0;
		dev_dbg(planeptr->base.dev, "%splane already enabled\n",
				__func__);
	}

	REG_WRITE(reg, value | DISPLAY_PLANE_ENABLE);
	vlv_update_plane_status(planeptr->pipe,
			plane ? VLV_SPRITE2 : VLV_SPRITE1, true);
	vlv_adf_flush_sp_plane(pipe, plane);
	/*
	 * TODO:No need to wait in case of mipi.
	 * Since data will flow only when port is enabled.
	 * wait for vblank will time out for mipi
	 */
	return 0;
}

static int vlv_sp_disable(struct intel_plane *planeptr)
{
	u32 reg, value, mask;
	struct vlv_sp_plane *splane = to_vlv_sp_plane(planeptr);
	int plane = splane->ctx.plane;
	int pipe = splane->ctx.pipe;

	reg = SPCNTR(pipe, plane);
	value = REG_READ(reg);
	if ((value & DISPLAY_PLANE_ENABLE) == 0) {
		dev_dbg(planeptr->base.dev, "%splane already disabled\n",
				__func__);
		return 0;
	}

	REG_WRITE(reg, value & ~DISPLAY_PLANE_ENABLE);
	vlv_update_plane_status(planeptr->pipe,
			plane ? VLV_SPRITE2 : VLV_SPRITE1, false);
	vlv_adf_flush_sp_plane(pipe, plane);
	/* While disabling plane reset the plane DDL value */
	if (plane == 0)
		mask = DDL_SPRITEA_MASK;
	else
		mask = DDL_SPRITEB_MASK;

	REG_WRITE_BITS(VLV_DDL(pipe), 0x00, mask);

	return 0;
}

static const u32 sprite_supported_formats[] = {
	DRM_FORMAT_C8,
	DRM_FORMAT_YUYV,
	DRM_FORMAT_YVYU,
	DRM_FORMAT_UYVY,
	DRM_FORMAT_VYUY,
	DRM_FORMAT_RGB565,
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_ARGB8888,
	DRM_FORMAT_XBGR8888,
	DRM_FORMAT_ABGR8888,
	DRM_FORMAT_XBGR2101010,
	DRM_FORMAT_ABGR2101010,
};

static const u32 sprite_supported_transforms[] = {
	INTEL_ADF_TRANSFORM_ROT180,
};

static const u32 sprite_supported_blendings[] = {
	INTEL_PLANE_BLENDING_NONE,
	INTEL_PLANE_BLENDING_PREMULT,
};

static const u32 sprite_supported_tiling[] = {
	INTEL_PLANE_TILE_NONE,
	INTEL_PLANE_TILE_X,
	INTEL_PLANE_TILE_Y,
};

static const u32 sprite_supported_zorder[] = {
	INTEL_PLANE_P1S1S2C1,
	INTEL_PLANE_P1S2S1C1,
	INTEL_PLANE_S2P1S1C1,
	INTEL_PLANE_S2S1P1C1,
	INTEL_PLANE_S1P1S2C1,
	INTEL_PLANE_S1S2P1C1,
};

static const u32 sprite_supported_reservedbit[] = {
	INTEL_PLANE_RESERVED_BIT_ZERO,
	INTEL_PLANE_RESERVED_BIT_SET,
};

static const struct intel_plane_ops vlv_sp_ops = {
	.base = {
		.suspend = vlv_sp_suspend,
		.resume = vlv_sp_resume,
	},
	.adf_ops = {
		.supported_formats = sprite_supported_formats,
		.n_supported_formats = ARRAY_SIZE(sprite_supported_formats),
	},
	.attach = vlv_sp_attach,
	.validate = vlv_sp_validate,
	.flip = vlv_sp_flip,
	.enable = vlv_sp_enable,
	.disable = vlv_sp_disable,
};

static const struct intel_plane_capabilities vlv_sp_caps = {
	.supported_formats = sprite_supported_formats,
	.n_supported_formats = ARRAY_SIZE(sprite_supported_formats),
	.supported_blendings = sprite_supported_blendings,
	.n_supported_blendings = ARRAY_SIZE(sprite_supported_blendings),
	.supported_transforms = sprite_supported_transforms,
	.n_supported_transforms = ARRAY_SIZE(sprite_supported_transforms),
	.supported_scalings = NULL,
	.n_supported_scalings = 0,
	.supported_decompressions = NULL,
	.n_supported_decompressions = 0,
	.supported_tiling = sprite_supported_tiling,
	.n_supported_tiling = ARRAY_SIZE(sprite_supported_tiling),
	.supported_zorder = sprite_supported_zorder,
	.n_supported_zorder = ARRAY_SIZE(sprite_supported_zorder),
	.supported_reservedbit = sprite_supported_reservedbit,
	.n_supported_reservedbit = ARRAY_SIZE(sprite_supported_reservedbit),
};

int vlv_sp_plane_init(struct vlv_sp_plane *splane, struct device *dev, u8 idx)
{
	int err;

	if (!splane) {
		dev_err(dev, "data provided is NULL\n");
		return -EINVAL;
	}
	err = context_init(&splane->ctx, idx);
	if (err) {
		dev_err(dev, "failed to init sprite context\n");
		return err;
	}
	return intel_adf_plane_init(&splane->base, dev, idx, &vlv_sp_caps,
			&vlv_sp_ops, "sp_plane");
}

void vlv_sp_plane_destroy(struct vlv_sp_plane *splane)
{
	if (splane) {
		intel_plane_destroy(&splane->base);
		context_destroy(&splane->ctx);
	}
}
