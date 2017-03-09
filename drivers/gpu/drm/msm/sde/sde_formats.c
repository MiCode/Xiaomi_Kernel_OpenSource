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

#include <uapi/drm/drm_fourcc.h>

#include "sde_kms.h"
#include "sde_formats.h"

#define SDE_UBWC_META_MACRO_W_H		16
#define SDE_UBWC_META_BLOCK_SIZE	256
#define SDE_MAX_IMG_WIDTH		0x3FFF
#define SDE_MAX_IMG_HEIGHT		0x3FFF

/**
 * SDE supported format packing, bpp, and other format
 * information.
 * SDE currently only supports interleaved RGB formats
 * UBWC support for a pixel format is indicated by the flag,
 * there is additional meta data plane for such formats
 */

#define INTERLEAVED_RGB_FMT(fmt, a, r, g, b, e0, e1, e2, e3, uc, alpha,   \
bp, flg, fm, np)                                                          \
{                                                                         \
	.base.pixel_format = DRM_FORMAT_ ## fmt,                          \
	.fetch_planes = SDE_PLANE_INTERLEAVED,                            \
	.alpha_enable = alpha,                                            \
	.element = { (e0), (e1), (e2), (e3) },                            \
	.bits = { g, b, r, a },                                           \
	.chroma_sample = SDE_CHROMA_RGB,                                  \
	.unpack_align_msb = 0,                                            \
	.unpack_tight = 1,                                                \
	.unpack_count = uc,                                               \
	.bpp = bp,                                                        \
	.fetch_mode = fm,                                                 \
	.flag = flg,                                                      \
	.num_planes = np                                                  \
}

#define INTERLEAVED_YUV_FMT(fmt, a, r, g, b, e0, e1, e2, e3,              \
alpha, chroma, count, bp, flg, fm, np)                                    \
{                                                                         \
	.base.pixel_format = DRM_FORMAT_ ## fmt,                          \
	.fetch_planes = SDE_PLANE_INTERLEAVED,                            \
	.alpha_enable = alpha,                                            \
	.element = { (e0), (e1), (e2), (e3)},                             \
	.bits = { g, b, r, a },                                           \
	.chroma_sample = chroma,                                          \
	.unpack_align_msb = 0,                                            \
	.unpack_tight = 1,                                                \
	.unpack_count = count,                                            \
	.bpp = bp,                                                        \
	.fetch_mode = fm,                                                 \
	.flag = flg,                                                      \
	.num_planes = np                                                  \
}

#define PSEUDO_YUV_FMT(fmt, a, r, g, b, e0, e1, chroma, flg, fm, np)      \
{                                                                         \
	.base.pixel_format = DRM_FORMAT_ ## fmt,                          \
	.fetch_planes = SDE_PLANE_PSEUDO_PLANAR,                          \
	.alpha_enable = false,                                            \
	.element = { (e0), (e1), 0, 0 },                                  \
	.bits = { g, b, r, a },                                           \
	.chroma_sample = chroma,                                          \
	.unpack_align_msb = 0,                                            \
	.unpack_tight = 1,                                                \
	.unpack_count = 2,                                                \
	.bpp = 2,                                                         \
	.fetch_mode = fm,                                                 \
	.flag = flg,                                                      \
	.num_planes = np                                                  \
}

#define PLANAR_YUV_FMT(fmt, a, r, g, b, e0, e1, e2, alpha, chroma, bp,    \
flg, fm, np)                                                      \
{                                                                         \
	.base.pixel_format = DRM_FORMAT_ ## fmt,                          \
	.fetch_planes = SDE_PLANE_PLANAR,                                 \
	.alpha_enable = alpha,                                            \
	.element = { (e0), (e1), (e2), 0 },                               \
	.bits = { g, b, r, a },                                           \
	.chroma_sample = chroma,                                          \
	.unpack_align_msb = 0,                                            \
	.unpack_tight = 1,                                                \
	.unpack_count = 1,                                                \
	.bpp = bp,                                                        \
	.fetch_mode = fm,                                                 \
	.flag = flg,                                                      \
	.num_planes = np                                                  \
}

static const struct sde_format sde_format_map[] = {
	INTERLEAVED_RGB_FMT(ARGB8888,
		COLOR_8BIT, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C1_B_Cb, C0_G_Y, C2_R_Cr, C3_ALPHA, 4,
		true, 4, 0,
		SDE_FETCH_LINEAR, 1),

	INTERLEAVED_RGB_FMT(ABGR8888,
		COLOR_8BIT, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA, 4,
		true, 4, 0,
		SDE_FETCH_LINEAR, 1),

	INTERLEAVED_RGB_FMT(XBGR8888,
		COLOR_8BIT, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA, 4,
		true, 4, 0,
		SDE_FETCH_LINEAR, 1),

	INTERLEAVED_RGB_FMT(RGBA8888,
		COLOR_8BIT, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C3_ALPHA, C1_B_Cb, C0_G_Y, C2_R_Cr, 4,
		true, 4, 0,
		SDE_FETCH_LINEAR, 1),

	INTERLEAVED_RGB_FMT(BGRA8888,
		COLOR_8BIT, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C3_ALPHA, C2_R_Cr, C0_G_Y, C1_B_Cb, 4,
		true, 4, 0,
		SDE_FETCH_LINEAR, 1),

	INTERLEAVED_RGB_FMT(BGRX8888,
		COLOR_8BIT, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C3_ALPHA, C2_R_Cr, C0_G_Y, C1_B_Cb, 4,
		false, 4, 0,
		SDE_FETCH_LINEAR, 1),

	INTERLEAVED_RGB_FMT(XRGB8888,
		COLOR_8BIT, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C1_B_Cb, C0_G_Y, C2_R_Cr, C3_ALPHA, 4,
		false, 4, 0,
		SDE_FETCH_LINEAR, 1),

	INTERLEAVED_RGB_FMT(RGBX8888,
		COLOR_8BIT, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C3_ALPHA, C1_B_Cb, C0_G_Y, C2_R_Cr, 4,
		false, 4, 0,
		SDE_FETCH_LINEAR, 1),

	INTERLEAVED_RGB_FMT(RGB888,
		0, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C1_B_Cb, C0_G_Y, C2_R_Cr, 0, 3,
		false, 3, 0,
		SDE_FETCH_LINEAR, 1),

	INTERLEAVED_RGB_FMT(BGR888,
		0, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C2_R_Cr, C0_G_Y, C1_B_Cb, 0, 3,
		false, 3, 0,
		SDE_FETCH_LINEAR, 1),

	INTERLEAVED_RGB_FMT(RGB565,
		0, COLOR_5BIT, COLOR_6BIT, COLOR_5BIT,
		C1_B_Cb, C0_G_Y, C2_R_Cr, 0, 3,
		false, 2, 0,
		SDE_FETCH_LINEAR, 1),

	INTERLEAVED_RGB_FMT(BGR565,
		0, COLOR_5BIT, COLOR_6BIT, COLOR_5BIT,
		C2_R_Cr, C0_G_Y, C1_B_Cb, 0, 3,
		false, 2, 0,
		SDE_FETCH_LINEAR, 1),

	INTERLEAVED_RGB_FMT(ARGB1555,
		COLOR_ALPHA_1BIT, COLOR_5BIT, COLOR_5BIT, COLOR_5BIT,
		C1_B_Cb, C0_G_Y, C2_R_Cr, C3_ALPHA, 4,
		true, 2, 0,
		SDE_FETCH_LINEAR, 1),

	INTERLEAVED_RGB_FMT(ABGR1555,
		COLOR_ALPHA_1BIT, COLOR_5BIT, COLOR_5BIT, COLOR_5BIT,
		C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA, 4,
		true, 2, 0,
		SDE_FETCH_LINEAR, 1),

	INTERLEAVED_RGB_FMT(RGBA5551,
		COLOR_ALPHA_1BIT, COLOR_5BIT, COLOR_5BIT, COLOR_5BIT,
		C3_ALPHA, C1_B_Cb, C0_G_Y, C2_R_Cr, 4,
		true, 2, 0,
		SDE_FETCH_LINEAR, 1),

	INTERLEAVED_RGB_FMT(BGRA5551,
		COLOR_ALPHA_1BIT, COLOR_5BIT, COLOR_5BIT, COLOR_5BIT,
		C3_ALPHA, C2_R_Cr, C0_G_Y, C1_B_Cb, 4,
		true, 2, 0,
		SDE_FETCH_LINEAR, 1),

	INTERLEAVED_RGB_FMT(XRGB1555,
		COLOR_ALPHA_1BIT, COLOR_5BIT, COLOR_5BIT, COLOR_5BIT,
		C1_B_Cb, C0_G_Y, C2_R_Cr, C3_ALPHA, 4,
		false, 2, 0,
		SDE_FETCH_LINEAR, 1),

	INTERLEAVED_RGB_FMT(XBGR1555,
		COLOR_ALPHA_1BIT, COLOR_5BIT, COLOR_5BIT, COLOR_5BIT,
		C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA, 4,
		false, 2, 0,
		SDE_FETCH_LINEAR, 1),

	INTERLEAVED_RGB_FMT(RGBX5551,
		COLOR_ALPHA_1BIT, COLOR_5BIT, COLOR_5BIT, COLOR_5BIT,
		C3_ALPHA, C1_B_Cb, C0_G_Y, C2_R_Cr, 4,
		false, 2, 0,
		SDE_FETCH_LINEAR, 1),

	INTERLEAVED_RGB_FMT(BGRX5551,
		COLOR_ALPHA_1BIT, COLOR_5BIT, COLOR_5BIT, COLOR_5BIT,
		C3_ALPHA, C2_R_Cr, C0_G_Y, C1_B_Cb, 4,
		false, 2, 0,
		SDE_FETCH_LINEAR, 1),

	INTERLEAVED_RGB_FMT(ARGB4444,
		COLOR_ALPHA_4BIT, COLOR_4BIT, COLOR_4BIT, COLOR_4BIT,
		C1_B_Cb, C0_G_Y, C2_R_Cr, C3_ALPHA, 4,
		true, 2, 0,
		SDE_FETCH_LINEAR, 1),

	INTERLEAVED_RGB_FMT(ABGR4444,
		COLOR_ALPHA_4BIT, COLOR_4BIT, COLOR_4BIT, COLOR_4BIT,
		C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA, 4,
		true, 2, 0,
		SDE_FETCH_LINEAR, 1),

	INTERLEAVED_RGB_FMT(RGBA4444,
		COLOR_ALPHA_4BIT, COLOR_4BIT, COLOR_4BIT, COLOR_4BIT,
		C3_ALPHA, C1_B_Cb, C0_G_Y, C2_R_Cr, 4,
		true, 2, 0,
		SDE_FETCH_LINEAR, 1),

	INTERLEAVED_RGB_FMT(BGRA4444,
		COLOR_ALPHA_4BIT, COLOR_4BIT, COLOR_4BIT, COLOR_4BIT,
		C3_ALPHA, C2_R_Cr, C0_G_Y, C1_B_Cb, 4,
		true, 2, 0,
		SDE_FETCH_LINEAR, 1),

	INTERLEAVED_RGB_FMT(XRGB4444,
		COLOR_ALPHA_4BIT, COLOR_4BIT, COLOR_4BIT, COLOR_4BIT,
		C1_B_Cb, C0_G_Y, C2_R_Cr, C3_ALPHA, 4,
		false, 2, 0,
		SDE_FETCH_LINEAR, 1),

	INTERLEAVED_RGB_FMT(XBGR4444,
		COLOR_ALPHA_4BIT, COLOR_4BIT, COLOR_4BIT, COLOR_4BIT,
		C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA, 4,
		false, 2, 0,
		SDE_FETCH_LINEAR, 1),

	INTERLEAVED_RGB_FMT(RGBX4444,
		COLOR_ALPHA_4BIT, COLOR_4BIT, COLOR_4BIT, COLOR_4BIT,
		C3_ALPHA, C1_B_Cb, C0_G_Y, C2_R_Cr, 4,
		false, 2, 0,
		SDE_FETCH_LINEAR, 1),

	INTERLEAVED_RGB_FMT(BGRX4444,
		COLOR_ALPHA_4BIT, COLOR_4BIT, COLOR_4BIT, COLOR_4BIT,
		C3_ALPHA, C2_R_Cr, C0_G_Y, C1_B_Cb, 4,
		false, 2, 0,
		SDE_FETCH_LINEAR, 1),

	INTERLEAVED_RGB_FMT(BGRA1010102,
		COLOR_8BIT, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C1_B_Cb, C0_G_Y, C2_R_Cr, C3_ALPHA, 4,
		true, 4, SDE_FORMAT_FLAG_DX,
		SDE_FETCH_LINEAR, 1),

	INTERLEAVED_RGB_FMT(RGBA1010102,
		COLOR_8BIT, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA, 4,
		true, 4, SDE_FORMAT_FLAG_DX,
		SDE_FETCH_LINEAR, 1),

	INTERLEAVED_RGB_FMT(ABGR2101010,
		COLOR_8BIT, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C3_ALPHA, C1_B_Cb, C0_G_Y, C2_R_Cr, 4,
		true, 4, SDE_FORMAT_FLAG_DX,
		SDE_FETCH_LINEAR, 1),

	INTERLEAVED_RGB_FMT(ARGB2101010,
		COLOR_8BIT, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C3_ALPHA, C2_R_Cr, C0_G_Y, C1_B_Cb, 4,
		true, 4, SDE_FORMAT_FLAG_DX,
		SDE_FETCH_LINEAR, 1),

	INTERLEAVED_RGB_FMT(XRGB2101010,
		COLOR_8BIT, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C3_ALPHA, C2_R_Cr, C0_G_Y, C1_B_Cb, 4,
		false, 4, SDE_FORMAT_FLAG_DX,
		SDE_FETCH_LINEAR, 1),

	INTERLEAVED_RGB_FMT(BGRX1010102,
		COLOR_8BIT, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C1_B_Cb, C0_G_Y, C2_R_Cr, C3_ALPHA, 4,
		false, 4, SDE_FORMAT_FLAG_DX,
		SDE_FETCH_LINEAR, 1),

	INTERLEAVED_RGB_FMT(XBGR2101010,
		COLOR_8BIT, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C3_ALPHA, C1_B_Cb, C0_G_Y, C2_R_Cr, 4,
		false, 4, SDE_FORMAT_FLAG_DX,
		SDE_FETCH_LINEAR, 1),

	INTERLEAVED_RGB_FMT(RGBX1010102,
		COLOR_8BIT, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA, 4,
		false, 4, SDE_FORMAT_FLAG_DX,
		SDE_FETCH_LINEAR, 1),

	PSEUDO_YUV_FMT(NV12,
		0, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C1_B_Cb, C2_R_Cr,
		SDE_CHROMA_420, SDE_FORMAT_FLAG_YUV,
		SDE_FETCH_LINEAR, 2),

	PSEUDO_YUV_FMT(NV21,
		0, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C2_R_Cr, C1_B_Cb,
		SDE_CHROMA_420, SDE_FORMAT_FLAG_YUV,
		SDE_FETCH_LINEAR, 2),

	PSEUDO_YUV_FMT(NV16,
		0, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C1_B_Cb, C2_R_Cr,
		SDE_CHROMA_H2V1, SDE_FORMAT_FLAG_YUV,
		SDE_FETCH_LINEAR, 2),

	PSEUDO_YUV_FMT(NV61,
		0, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C2_R_Cr, C1_B_Cb,
		SDE_CHROMA_H2V1, SDE_FORMAT_FLAG_YUV,
		SDE_FETCH_LINEAR, 2),

	INTERLEAVED_YUV_FMT(VYUY,
		0, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C2_R_Cr, C0_G_Y, C1_B_Cb, C0_G_Y,
		false, SDE_CHROMA_H2V1, 4, 2, SDE_FORMAT_FLAG_YUV,
		SDE_FETCH_LINEAR, 2),

	INTERLEAVED_YUV_FMT(UYVY,
		0, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C1_B_Cb, C0_G_Y, C2_R_Cr, C0_G_Y,
		false, SDE_CHROMA_H2V1, 4, 2, SDE_FORMAT_FLAG_YUV,
		SDE_FETCH_LINEAR, 2),

	INTERLEAVED_YUV_FMT(YUYV,
		0, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C0_G_Y, C1_B_Cb, C0_G_Y, C2_R_Cr,
		false, SDE_CHROMA_H2V1, 4, 2, SDE_FORMAT_FLAG_YUV,
		SDE_FETCH_LINEAR, 2),

	INTERLEAVED_YUV_FMT(YVYU,
		0, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C0_G_Y, C2_R_Cr, C0_G_Y, C1_B_Cb,
		false, SDE_CHROMA_H2V1, 4, 2, SDE_FORMAT_FLAG_YUV,
		SDE_FETCH_LINEAR, 2),

	PLANAR_YUV_FMT(YUV420,
		0, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C2_R_Cr, C1_B_Cb, C0_G_Y,
		false, SDE_CHROMA_420, 1, SDE_FORMAT_FLAG_YUV,
		SDE_FETCH_LINEAR, 3),

	PLANAR_YUV_FMT(YVU420,
		0, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C1_B_Cb, C2_R_Cr, C0_G_Y,
		false, SDE_CHROMA_420, 1, SDE_FORMAT_FLAG_YUV,
		SDE_FETCH_LINEAR, 3),
};

/*
 * UBWC formats table:
 * This table holds the UBWC formats supported.
 * If a compression ratio needs to be used for this or any other format,
 * the data will be passed by user-space.
 */
static const struct sde_format sde_format_map_ubwc[] = {
	INTERLEAVED_RGB_FMT(RGB565,
		0, COLOR_5BIT, COLOR_6BIT, COLOR_5BIT,
		C2_R_Cr, C0_G_Y, C1_B_Cb, 0, 3,
		false, 2, 0,
		SDE_FETCH_UBWC, 2),

	INTERLEAVED_RGB_FMT(RGBA8888,
		COLOR_8BIT, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA, 4,
		true, 4, 0,
		SDE_FETCH_UBWC, 2),

	INTERLEAVED_RGB_FMT(RGBX8888,
		COLOR_8BIT, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA, 4,
		false, 4, 0,
		SDE_FETCH_UBWC, 2),

	INTERLEAVED_RGB_FMT(RGBA1010102,
		COLOR_8BIT, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA, 4,
		true, 4, SDE_FORMAT_FLAG_DX,
		SDE_FETCH_UBWC, 2),

	INTERLEAVED_RGB_FMT(RGBX1010102,
		COLOR_8BIT, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA, 4,
		true, 4, SDE_FORMAT_FLAG_DX,
		SDE_FETCH_UBWC, 2),

	PSEUDO_YUV_FMT(NV12,
		0, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C1_B_Cb, C2_R_Cr,
		SDE_CHROMA_420, SDE_FORMAT_FLAG_YUV,
		SDE_FETCH_UBWC, 4),
};

/* _sde_get_v_h_subsample_rate - Get subsample rates for all formats we support
 *   Note: Not using the drm_format_*_subsampling since we have formats
 */
static void _sde_get_v_h_subsample_rate(
	enum sde_chroma_samp_type chroma_sample,
	uint32_t *v_sample,
	uint32_t *h_sample)
{
	if (!v_sample || !h_sample)
		return;

	switch (chroma_sample) {
	case SDE_CHROMA_H2V1:
		*v_sample = 1;
		*h_sample = 2;
		break;
	case SDE_CHROMA_H1V2:
		*v_sample = 2;
		*h_sample = 1;
		break;
	case SDE_CHROMA_420:
		*v_sample = 2;
		*h_sample = 2;
		break;
	default:
		*v_sample = 1;
		*h_sample = 1;
		break;
	}
}

static int _sde_format_get_plane_sizes_ubwc(
		const struct sde_format *fmt,
		const uint32_t width,
		const uint32_t height,
		struct sde_hw_fmt_layout *layout)
{
	int i;

	memset(layout, 0, sizeof(struct sde_hw_fmt_layout));
	layout->format = fmt;
	layout->width = width;
	layout->height = height;
	layout->num_planes = fmt->num_planes;

	if (fmt->base.pixel_format == DRM_FORMAT_NV12) {
		uint32_t y_stride_alignment, uv_stride_alignment;
		uint32_t y_height_alignment, uv_height_alignment;
		uint32_t y_tile_width = 32;
		uint32_t y_tile_height = 8;
		uint32_t uv_tile_width = y_tile_width / 2;
		uint32_t uv_tile_height = y_tile_height;
		uint32_t y_bpp_numer = 1, y_bpp_denom = 1;
		uint32_t uv_bpp_numer = 1, uv_bpp_denom = 1;

		y_stride_alignment = 128;
		uv_stride_alignment = 64;
		y_height_alignment = 32;
		uv_height_alignment = 32;
		y_bpp_numer = 1;
		uv_bpp_numer = 2;
		y_bpp_denom = 1;
		uv_bpp_denom = 1;

		layout->num_planes = 4;
		/* Y bitstream stride and plane size */
		layout->plane_pitch[0] = ALIGN(width, y_stride_alignment);
		layout->plane_pitch[0] = (layout->plane_pitch[0] * y_bpp_numer)
				/ y_bpp_denom;
		layout->plane_size[0] = ALIGN(layout->plane_pitch[0] *
				ALIGN(height, y_height_alignment), 4096);

		/* CbCr bitstream stride and plane size */
		layout->plane_pitch[1] = ALIGN(width / 2, uv_stride_alignment);
		layout->plane_pitch[1] = (layout->plane_pitch[1] * uv_bpp_numer)
				/ uv_bpp_denom;
		layout->plane_size[1] = ALIGN(layout->plane_pitch[1] *
			ALIGN(height / 2, uv_height_alignment), 4096);

		/* Y meta data stride and plane size */
		layout->plane_pitch[2] = ALIGN(
				DIV_ROUND_UP(width, y_tile_width), 64);
		layout->plane_size[2] = ALIGN(layout->plane_pitch[2] *
			ALIGN(DIV_ROUND_UP(height, y_tile_height), 16), 4096);

		/* CbCr meta data stride and plane size */
		layout->plane_pitch[3] = ALIGN(
				DIV_ROUND_UP(width / 2, uv_tile_width), 64);
		layout->plane_size[3] = ALIGN(layout->plane_pitch[3] *
			ALIGN(DIV_ROUND_UP(height / 2, uv_tile_height), 16),
			4096);

	} else if (fmt->base.pixel_format == DRM_FORMAT_ABGR8888 ||
		fmt->base.pixel_format == DRM_FORMAT_XBGR8888    ||
		fmt->base.pixel_format == DRM_FORMAT_BGRA1010102 ||
		fmt->base.pixel_format == DRM_FORMAT_BGRX1010102 ||
		fmt->base.pixel_format == DRM_FORMAT_BGR565) {

		uint32_t stride_alignment, aligned_bitstream_width;

		if (fmt->base.pixel_format == DRM_FORMAT_BGR565)
			stride_alignment = 128;
		else
			stride_alignment = 64;
		layout->num_planes = 3;

		/* Nothing in plane[1] */

		/* RGB bitstream stride and plane size */
		aligned_bitstream_width = ALIGN(width, stride_alignment);
		layout->plane_pitch[0] = aligned_bitstream_width * fmt->bpp;
		layout->plane_size[0] = ALIGN(fmt->bpp * aligned_bitstream_width
				* ALIGN(height, 16), 4096);

		/* RGB meta data stride and plane size */
		layout->plane_pitch[2] = ALIGN(DIV_ROUND_UP(
				aligned_bitstream_width, 16), 64);
		layout->plane_size[2] = ALIGN(layout->plane_pitch[2] *
			ALIGN(DIV_ROUND_UP(height, 4), 16), 4096);
	} else {
		DRM_ERROR("UBWC format not supported for fmt:0x%X\n",
			fmt->base.pixel_format);
		return -EINVAL;
	}

	for (i = 0; i < SDE_MAX_PLANES; i++)
		layout->total_size += layout->plane_size[i];

	return 0;
}

static int _sde_format_get_plane_sizes_linear(
		const struct sde_format *fmt,
		const uint32_t width,
		const uint32_t height,
		struct sde_hw_fmt_layout *layout)
{
	int i;

	memset(layout, 0, sizeof(struct sde_hw_fmt_layout));
	layout->format = fmt;
	layout->width = width;
	layout->height = height;
	layout->num_planes = fmt->num_planes;

	/* Due to memset above, only need to set planes of interest */
	if (fmt->fetch_planes == SDE_PLANE_INTERLEAVED) {
		layout->num_planes = 1;
		layout->plane_size[0] = width * height * layout->format->bpp;
		layout->plane_pitch[0] = width * layout->format->bpp;
	} else {
		uint32_t v_subsample, h_subsample;
		uint32_t chroma_samp;

		chroma_samp = fmt->chroma_sample;
		_sde_get_v_h_subsample_rate(chroma_samp, &v_subsample,
				&h_subsample);

		if (width % h_subsample || height % v_subsample) {
			DRM_ERROR("mismatch in subsample vs dimensions\n");
			return -EINVAL;
		}

		layout->plane_pitch[0] = width;
		layout->plane_pitch[1] = width / h_subsample;
		layout->plane_size[0] = layout->plane_pitch[0] * height;
		layout->plane_size[1] = layout->plane_pitch[1] *
				(height / v_subsample);

		if (fmt->fetch_planes == SDE_PLANE_PSEUDO_PLANAR) {
			layout->num_planes = 2;
			layout->plane_size[1] *= 2;
			layout->plane_pitch[1] *= 2;
		} else {
			/* planar */
			layout->num_planes = 3;
			layout->plane_size[2] = layout->plane_size[1];
			layout->plane_pitch[2] = layout->plane_pitch[1];
		}
	}

	for (i = 0; i < SDE_MAX_PLANES; i++)
		layout->total_size += layout->plane_size[i];

	return 0;
}

static int _sde_format_get_plane_sizes(
		const struct sde_format *fmt,
		const uint32_t w,
		const uint32_t h,
		struct sde_hw_fmt_layout *layout)
{
	if (!layout || !fmt) {
		DRM_ERROR("invalid pointer\n");
		return -EINVAL;
	}

	if ((w > SDE_MAX_IMG_WIDTH) || (h > SDE_MAX_IMG_HEIGHT)) {
		DRM_ERROR("image dimensions outside max range\n");
		return -ERANGE;
	}

	if (SDE_FORMAT_IS_UBWC(fmt))
		return _sde_format_get_plane_sizes_ubwc(fmt, w, h, layout);

	return _sde_format_get_plane_sizes_linear(fmt, w, h, layout);
}

static int _sde_format_populate_addrs_ubwc(
		struct msm_gem_address_space *aspace,
		struct drm_framebuffer *fb,
		struct sde_hw_fmt_layout *layout)
{
	uint32_t base_addr;

	if (!fb || !layout) {
		DRM_ERROR("invalid pointers\n");
		return -EINVAL;
	}

	base_addr = msm_framebuffer_iova(fb, aspace, 0);
	if (!base_addr) {
		DRM_ERROR("failed to retrieve base addr\n");
		return -EFAULT;
	}

	/* Per-format logic for verifying active planes */
	if (SDE_FORMAT_IS_YUV(layout->format)) {
		/************************************************/
		/*      UBWC            **                      */
		/*      buffer          **      SDE PLANE       */
		/*      format          **                      */
		/************************************************/
		/* -------------------  ** -------------------- */
		/* |      Y meta     |  ** |    Y bitstream   | */
		/* |       data      |  ** |       plane      | */
		/* -------------------  ** -------------------- */
		/* |    Y bitstream  |  ** |  CbCr bitstream  | */
		/* |       data      |  ** |       plane      | */
		/* -------------------  ** -------------------- */
		/* |   Cbcr metadata |  ** |       Y meta     | */
		/* |       data      |  ** |       plane      | */
		/* -------------------  ** -------------------- */
		/* |  CbCr bitstream |  ** |     CbCr meta    | */
		/* |       data      |  ** |       plane      | */
		/* -------------------  ** -------------------- */
		/************************************************/

		/* configure Y bitstream plane */
		layout->plane_addr[0] = base_addr + layout->plane_size[2];

		/* configure CbCr bitstream plane */
		layout->plane_addr[1] = base_addr + layout->plane_size[0]
			+ layout->plane_size[2] + layout->plane_size[3];

		/* configure Y metadata plane */
		layout->plane_addr[2] = base_addr;

		/* configure CbCr metadata plane */
		layout->plane_addr[3] = base_addr + layout->plane_size[0]
			+ layout->plane_size[2];

	} else {
		/************************************************/
		/*      UBWC            **                      */
		/*      buffer          **      SDE PLANE       */
		/*      format          **                      */
		/************************************************/
		/* -------------------  ** -------------------- */
		/* |      RGB meta   |  ** |   RGB bitstream  | */
		/* |       data      |  ** |       plane      | */
		/* -------------------  ** -------------------- */
		/* |  RGB bitstream  |  ** |       NONE       | */
		/* |       data      |  ** |                  | */
		/* -------------------  ** -------------------- */
		/*                      ** |     RGB meta     | */
		/*                      ** |       plane      | */
		/*                      ** -------------------- */
		/************************************************/

		layout->plane_addr[0] = base_addr + layout->plane_size[2];
		layout->plane_addr[1] = 0;
		layout->plane_addr[2] = base_addr;
		layout->plane_addr[3] = 0;
	}

	return 0;
}

static int _sde_format_populate_addrs_linear(
		struct msm_gem_address_space *aspace,
		struct drm_framebuffer *fb,
		struct sde_hw_fmt_layout *layout)
{
	unsigned int i;

	/* Can now check the pitches given vs pitches expected */
	for (i = 0; i < layout->num_planes; ++i) {
		if (layout->plane_pitch[i] != fb->pitches[i]) {
			DRM_ERROR("plane %u expected pitch %u, fb %u\n",
				i, layout->plane_pitch[i], fb->pitches[i]);
			return -EINVAL;
		}
	}

	/* Populate addresses for simple formats here */
	for (i = 0; i < layout->num_planes; ++i) {
		layout->plane_addr[i] = msm_framebuffer_iova(fb, aspace, i);
		if (!layout->plane_addr[i]) {
			DRM_ERROR("failed to retrieve base addr\n");
			return -EFAULT;
		}
	}

	return 0;
}

int sde_format_populate_layout(
		struct msm_gem_address_space *aspace,
		struct drm_framebuffer *fb,
		struct sde_hw_fmt_layout *layout)
{
	uint32_t plane_addr[SDE_MAX_PLANES];
	int i, ret;

	if (!fb || !layout) {
		DRM_ERROR("invalid arguments\n");
		return -EINVAL;
	}

	if ((fb->width > SDE_MAX_IMG_WIDTH) ||
			(fb->height > SDE_MAX_IMG_HEIGHT)) {
		DRM_ERROR("image dimensions outside max range\n");
		return -ERANGE;
	}

	layout->format = to_sde_format(msm_framebuffer_format(fb));

	/* Populate the plane sizes etc via get_format */
	ret = _sde_format_get_plane_sizes(layout->format, fb->width, fb->height,
			layout);
	if (ret)
		return ret;

	for (i = 0; i < SDE_MAX_PLANES; ++i)
		plane_addr[i] = layout->plane_addr[i];

	/* Populate the addresses given the fb */
	if (SDE_FORMAT_IS_UBWC(layout->format))
		ret = _sde_format_populate_addrs_ubwc(aspace, fb, layout);
	else
		ret = _sde_format_populate_addrs_linear(aspace, fb, layout);

	/* check if anything changed */
	if (!ret && !memcmp(plane_addr, layout->plane_addr, sizeof(plane_addr)))
		ret = -EAGAIN;

	return ret;
}

static void _sde_format_calc_offset_linear(struct sde_hw_fmt_layout *source,
		u32 x, u32 y)
{
	if ((x == 0) && (y == 0))
		return;

	source->plane_addr[0] += y * source->plane_pitch[0];

	if (source->num_planes == 1) {
		source->plane_addr[0] += x * source->format->bpp;
	} else {
		uint32_t xoff, yoff;
		uint32_t v_subsample = 1;
		uint32_t h_subsample = 1;

		_sde_get_v_h_subsample_rate(source->format->chroma_sample,
				&v_subsample, &h_subsample);

		xoff = x / h_subsample;
		yoff = y / v_subsample;

		source->plane_addr[0] += x;
		source->plane_addr[1] += xoff +
				(yoff * source->plane_pitch[1]);
		if (source->num_planes == 2) /* pseudo planar */
			source->plane_addr[1] += xoff;
		else /* planar */
			source->plane_addr[2] += xoff +
				(yoff * source->plane_pitch[2]);
	}
}

int sde_format_populate_layout_with_roi(
		struct msm_gem_address_space *aspace,
		struct drm_framebuffer *fb,
		struct sde_rect *roi,
		struct sde_hw_fmt_layout *layout)
{
	int ret;

	ret = sde_format_populate_layout(aspace, fb, layout);
	if (ret || !roi)
		return ret;

	if (!roi->w || !roi->h || (roi->x + roi->w > fb->width) ||
			(roi->y + roi->h > fb->height)) {
		DRM_ERROR("invalid roi=[%d,%d,%d,%d], fb=[%u,%u]\n",
				roi->x, roi->y, roi->w, roi->h,
				fb->width, fb->height);
		ret = -EINVAL;
	} else if (SDE_FORMAT_IS_LINEAR(layout->format)) {
		_sde_format_calc_offset_linear(layout, roi->x, roi->y);
		layout->width = roi->w;
		layout->height = roi->h;
	} else if (roi->x || roi->y || (roi->w != fb->width) ||
			(roi->h != fb->height)) {
		DRM_ERROR("non-linear layout with roi not supported\n");
		ret = -EINVAL;
	}

	return ret;
}

int sde_format_check_modified_format(
		const struct msm_kms *kms,
		const struct msm_format *msm_fmt,
		const struct drm_mode_fb_cmd2 *cmd,
		struct drm_gem_object **bos)
{
	int ret, i, num_base_fmt_planes;
	const struct sde_format *fmt;
	struct sde_hw_fmt_layout layout;
	uint32_t bos_total_size = 0;

	if (!msm_fmt || !cmd || !bos) {
		DRM_ERROR("invalid arguments\n");
		return -EINVAL;
	}

	fmt = to_sde_format(msm_fmt);
	num_base_fmt_planes = drm_format_num_planes(fmt->base.pixel_format);

	ret = _sde_format_get_plane_sizes(fmt, cmd->width, cmd->height,
			&layout);
	if (ret)
		return ret;

	for (i = 0; i < num_base_fmt_planes; i++) {
		if (!bos[i]) {
			DRM_ERROR("invalid handle for plane %d\n", i);
			return -EINVAL;
		}
		bos_total_size += bos[i]->size;
	}

	if (bos_total_size < layout.total_size) {
		DRM_ERROR("buffers total size too small %u expected %u\n",
				bos_total_size, layout.total_size);
		return -EINVAL;
	}

	return 0;
}

const struct sde_format *sde_get_sde_format_ext(
		const uint32_t format,
		const uint64_t *modifiers,
		const uint32_t modifiers_len)
{
	uint32_t i = 0;
	uint64_t mod0 = 0;
	const struct sde_format *fmt = NULL;
	const struct sde_format *map = NULL;
	ssize_t map_size = 0;

	/*
	 * Currently only support exactly zero or one modifier.
	 * All planes used must specify the same modifier.
	 */
	if (modifiers_len && !modifiers) {
		DRM_ERROR("invalid modifiers array\n");
		return NULL;
	} else if (modifiers && modifiers_len && modifiers[0]) {
		mod0 = modifiers[0];
		DBG("plane format modifier 0x%llX", mod0);
		for (i = 1; i < modifiers_len; i++) {
			if (modifiers[i] != mod0) {
				DRM_ERROR("bad fmt mod 0x%llX on plane %d\n",
					modifiers[i], i);
				return NULL;
			}
		}
	}

	switch (mod0) {
	case 0:
		map = sde_format_map;
		map_size = ARRAY_SIZE(sde_format_map);
		break;
	case DRM_FORMAT_MOD_QCOM_COMPRESSED:
		map = sde_format_map_ubwc;
		map_size = ARRAY_SIZE(sde_format_map_ubwc);
		DBG("found fmt 0x%X DRM_FORMAT_MOD_QCOM_COMPRESSED", format);
		break;
	default:
		DRM_ERROR("unsupported format modifier %llX\n", mod0);
		return NULL;
	}

	for (i = 0; i < map_size; i++) {
		if (format == map[i].base.pixel_format) {
			fmt = &map[i];
			break;
		}
	}

	if (fmt == NULL)
		DRM_ERROR("unsupported fmt 0x%X modifier 0x%llX\n",
				format, mod0);
	else
		DBG("fmt %s mod 0x%llX ubwc %d yuv %d",
				drm_get_format_name(format), mod0,
				SDE_FORMAT_IS_UBWC(fmt),
				SDE_FORMAT_IS_YUV(fmt));

	return fmt;
}

const struct msm_format *sde_get_msm_format(
		struct msm_kms *kms,
		const uint32_t format,
		const uint64_t *modifiers,
		const uint32_t modifiers_len)
{
	const struct sde_format *fmt = sde_get_sde_format_ext(format,
			modifiers, modifiers_len);
	if (fmt)
		return &fmt->base;
	return NULL;
}

uint32_t sde_populate_formats(
		const struct sde_format_extended *format_list,
		uint32_t *pixel_formats,
		uint64_t *pixel_modifiers,
		uint32_t pixel_formats_max)
{
	uint32_t i, fourcc_format;

	if (!format_list || !pixel_formats)
		return 0;

	for (i = 0, fourcc_format = 0;
			format_list->fourcc_format && i < pixel_formats_max;
			++format_list) {
		/* verify if listed format is in sde_format_map? */

		/* optionally return modified formats */
		if (pixel_modifiers) {
			/* assume same modifier for all fb planes */
			pixel_formats[i] = format_list->fourcc_format;
			pixel_modifiers[i++] = format_list->modifier;
		} else {
			/* assume base formats grouped together */
			if (fourcc_format != format_list->fourcc_format) {
				fourcc_format = format_list->fourcc_format;
				pixel_formats[i++] = fourcc_format;
			}
		}
	}

	return i;
}
