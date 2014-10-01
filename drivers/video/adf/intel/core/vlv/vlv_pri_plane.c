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
#include <drm/i915_drm.h>

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

static const u32 pri_supported_transforms[] = {
	INTEL_PLANE_TRANSFORM_ROT180,
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

static int vlv_pri_validate(struct intel_plane *plane,
		struct intel_buffer *buf,
		struct intel_plane_config *config)
{
	u32 format_config;
	u8 bpp;
	return get_format_config(buf->format, &format_config, &bpp);
}

/* Computes the linear offset to the base tile and adjusts x, y. bytes per pixel
* is assumed to be a power-of-two. */
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

static void vlv_pri_flip(struct intel_plane *plane,
		struct intel_buffer *buf,
		struct intel_plane_config *config)
{
	struct vlv_pri_plane *pri_plane = to_vlv_pri_plane(plane);
	struct pri_plane_regs_value *regs = &pri_plane->ctx.regs;
	int src_x, src_y;
	u32 format_config = 0;
	u8 bpp = 0;
	u32 pidx = pri_plane->ctx.plane;
	unsigned long dspaddr_offset;

	get_format_config(buf->format, &format_config, &bpp);

	src_x = config->src_x;
	src_y = config->src_y;

	regs->dspcntr = REG_READ(DSPCNTR(pidx));
	regs->dspcntr |= DISPLAY_PLANE_ENABLE;
	regs->dspcntr &= ~DISPPLANE_PIXFORMAT_MASK;
	regs->dspcntr |= format_config;

	if (buf->tiling_mode != I915_TILING_NONE)
		regs->dspcntr |= DISPPLANE_TILED;
	else
		regs->dspcntr &= ~DISPPLANE_TILED;

	regs->stride = buf->stride;

	regs->linearoff = src_y * regs->stride + src_x * bpp;

	dspaddr_offset = vlv_compute_page_offset(&src_x, &src_y,
				buf->tiling_mode, bpp, regs->stride);

	regs->linearoff -= dspaddr_offset;

	regs->tileoff = (src_y << 16) | src_x;

	regs->surfaddr = (buf->gtt_offset_in_pages + dspaddr_offset);

	REG_WRITE(DSPCNTR(pidx), regs->dspcntr);
	REG_WRITE(DSPSTRIDE(pidx), regs->stride);
	REG_WRITE(DSPTILEOFF(pidx), regs->tileoff);
	REG_WRITE(DSPLINOFF(pidx), regs->linearoff);
	I915_MODIFY_DISPBASE(DSPSURF(pidx), regs->surfaddr);
	REG_POSTING_READ(DSPSURF(pidx));

	return;
}

static inline void vlv_adf_flush_disp_plane(u8 plane)
{
	REG_WRITE(DSPSURF(plane), REG_READ(DSPSURF(plane)));
}

static int vlv_pri_enable(struct intel_plane *plane)
{
	u32 reg, value;

	reg = DSPCNTR(plane->base.idx);
	value = REG_READ(reg);
	if (value & DISPLAY_PLANE_ENABLE) {
		dev_dbg(plane->base.dev, "%splane already enabled\n",
				__func__);
		return 0;
	}

	REG_WRITE(reg, value | DISPLAY_PLANE_ENABLE);
	vlv_adf_flush_disp_plane(plane->base.idx);

	/*
	 * TODO:No need to wait in case of mipi.
	 * Since data will flow only when port is enabled.
	 * wait for vblank will time out for mipi
	 *
	 * update for HDMI later
	 */
	return 0;
}

static int vlv_pri_disable(struct intel_plane *plane)
{
	u32 reg, value;

	reg = DSPCNTR(plane->base.idx);
	value = REG_READ(reg);
	if ((value & DISPLAY_PLANE_ENABLE) == 0) {
		dev_dbg(plane->base.dev, "%splane already disabled\n",
				__func__);
		return 0;
	}

	REG_WRITE(reg, value & ~DISPLAY_PLANE_ENABLE);
	vlv_adf_flush_disp_plane(plane->base.idx);
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

static const struct intel_plane_ops vlv_pri_ops = {
	.base = {
		.suspend = vlv_pri_suspend,
		.resume = vlv_pri_resume,
	},
	.adf_ops = {
		.supported_formats = pri_supported_formats,
		.n_supported_formats = ARRAY_SIZE(pri_supported_formats),
	},
	.validate = vlv_pri_validate,
	.flip = vlv_pri_flip,
	.enable = vlv_pri_enable,
	.disable = vlv_pri_disable,
};

int vlv_pri_plane_init(struct vlv_pri_plane *pplane, struct device *dev, u8 idx)
{
	int err;

	if (!pplane) {
		dev_err(dev, "%s: struct NULL\n", __func__);
		return -EINVAL;
	}
	err = init_context(&pplane->ctx, idx);
	if (err) {
		pr_err("%s: plane context initialization failed\n", __func__);
		return err;
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
