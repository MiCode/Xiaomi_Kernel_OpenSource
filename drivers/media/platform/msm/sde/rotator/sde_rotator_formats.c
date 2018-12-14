/* Copyright (c) 2012, 2015-2018, The Linux Foundation. All rights reserved.
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

#include <media/msm_sde_rotator.h>

#include "sde_rotator_formats.h"
#include "sde_rotator_util.h"

#define FMT_RGB_565(fmt, desc, frame_fmt, flag_arg, e0, e1, e2, isubwc)	\
	{							\
		.format = (fmt),				\
		.description = (desc),				\
		.flag = flag_arg,				\
		.fetch_planes = SDE_MDP_PLANE_INTERLEAVED,	\
		.unpack_tight = 1,				\
		.unpack_align_msb = 0,				\
		.alpha_enable = 0,				\
		.unpack_count = 3,				\
		.bpp = 2,					\
		.frame_format = (frame_fmt),			\
		.pixel_mode = SDE_MDP_PIXEL_NORMAL,		\
		.element = { (e0), (e1), (e2) },		\
		.bits = {					\
			[C2_R_Cr] = SDE_COLOR_5BIT,		\
			[C0_G_Y] = SDE_COLOR_6BIT,		\
			[C1_B_Cb] = SDE_COLOR_5BIT,		\
		},						\
		.is_ubwc = isubwc,				\
	}

#define FMT_RGB_888(fmt, desc, frame_fmt, flag_arg, e0, e1, e2, isubwc)	\
	{							\
		.format = (fmt),				\
		.description = (desc),				\
		.flag = flag_arg,				\
		.fetch_planes = SDE_MDP_PLANE_INTERLEAVED,	\
		.unpack_tight = 1,				\
		.unpack_align_msb = 0,				\
		.alpha_enable = 0,				\
		.unpack_count = 3,				\
		.bpp = 3,					\
		.frame_format = (frame_fmt),			\
		.pixel_mode = SDE_MDP_PIXEL_NORMAL,		\
		.element = { (e0), (e1), (e2) },		\
		.bits = {					\
			[C2_R_Cr] = SDE_COLOR_8BIT,		\
			[C0_G_Y] = SDE_COLOR_8BIT,		\
			[C1_B_Cb] = SDE_COLOR_8BIT,		\
		},						\
		.is_ubwc = isubwc,				\
	}

#define FMT_RGB_8888(fmt, desc, frame_fmt, flag_arg,			\
		alpha_en, e0, e1, e2, e3, isubwc)		\
	{							\
		.format = (fmt),				\
		.description = (desc),				\
		.flag = flag_arg,				\
		.fetch_planes = SDE_MDP_PLANE_INTERLEAVED,	\
		.unpack_tight = 1,				\
		.unpack_align_msb = 0,				\
		.alpha_enable = (alpha_en),			\
		.unpack_count = 4,				\
		.bpp = 4,					\
		.frame_format = (frame_fmt),			\
		.pixel_mode = SDE_MDP_PIXEL_NORMAL,		\
		.element = { (e0), (e1), (e2), (e3) },		\
		.bits = {					\
			[C3_ALPHA] = SDE_COLOR_8BIT,		\
			[C2_R_Cr] = SDE_COLOR_8BIT,		\
			[C0_G_Y] = SDE_COLOR_8BIT,		\
			[C1_B_Cb] = SDE_COLOR_8BIT,		\
		},						\
		.is_ubwc = isubwc,				\
	}

#define FMT_YUV10_COMMON(fmt)					\
		.format = (fmt),				\
		.is_yuv = 1,					\
		.bits = {					\
			[C2_R_Cr] = SDE_COLOR_8BIT,		\
			[C0_G_Y] = SDE_COLOR_8BIT,		\
			[C1_B_Cb] = SDE_COLOR_8BIT,		\
		},						\
		.alpha_enable = 0

#define FMT_YUV_COMMON(fmt)					\
		.format = (fmt),				\
		.is_yuv = 1,					\
		.bits = {					\
			[C2_R_Cr] = SDE_COLOR_8BIT,		\
			[C0_G_Y] = SDE_COLOR_8BIT,		\
			[C1_B_Cb] = SDE_COLOR_8BIT,		\
		},						\
		.alpha_enable = 0,				\
		.unpack_tight = 1,				\
		.unpack_align_msb = 0

#define FMT_YUV_PSEUDO(fmt, desc, frame_fmt, samp, pixel_type,	\
		flag_arg, e0, e1, isubwc)			\
	{							\
		FMT_YUV_COMMON(fmt),				\
		.description = (desc),				\
		.flag = flag_arg,				\
		.fetch_planes = SDE_MDP_PLANE_PSEUDO_PLANAR,	\
		.chroma_sample = samp,				\
		.unpack_count = 2,				\
		.bpp = 2,					\
		.frame_format = (frame_fmt),			\
		.pixel_mode = (pixel_type),			\
		.element = { (e0), (e1) },			\
		.is_ubwc = isubwc,				\
	}

#define FMT_YUV_PLANR(fmt, desc, frame_fmt, samp, \
		flag_arg, e0, e1)		\
	{							\
		FMT_YUV_COMMON(fmt),				\
		.description = (desc),				\
		.flag = flag_arg,				\
		.fetch_planes = SDE_MDP_PLANE_PLANAR,		\
		.chroma_sample = samp,				\
		.bpp = 1,					\
		.unpack_count = 1,				\
		.frame_format = (frame_fmt),			\
		.pixel_mode = SDE_MDP_PIXEL_NORMAL,		\
		.element = { (e0), (e1) },			\
		.is_ubwc = SDE_MDP_COMPRESS_NONE,		\
	}

#define FMT_RGB_1555(fmt, desc, alpha_en, flag_arg, e0, e1, e2, e3)	\
	{							\
		.format = (fmt),				\
		.description = (desc),				\
		.flag = flag_arg,				\
		.fetch_planes = SDE_MDP_PLANE_INTERLEAVED,	\
		.unpack_tight = 1,				\
		.unpack_align_msb = 0,				\
		.alpha_enable = (alpha_en),			\
		.unpack_count = 4,				\
		.bpp = 2,					\
		.element = { (e0), (e1), (e2), (e3) },		\
		.frame_format = SDE_MDP_FMT_LINEAR,		\
		.pixel_mode = SDE_MDP_PIXEL_NORMAL,		\
		.bits = {					\
			[C3_ALPHA] = SDE_COLOR_ALPHA_1BIT,	\
			[C2_R_Cr] = SDE_COLOR_5BIT,		\
			[C0_G_Y] = SDE_COLOR_5BIT,		\
			[C1_B_Cb] = SDE_COLOR_5BIT,		\
		},						\
		.is_ubwc = SDE_MDP_COMPRESS_NONE,		\
	}

#define FMT_RGB_4444(fmt, desc, alpha_en, flag_arg, e0, e1, e2, e3)	\
	{							\
		.format = (fmt),				\
		.description = (desc),				\
		.flag = flag_arg,				\
		.fetch_planes = SDE_MDP_PLANE_INTERLEAVED,	\
		.unpack_tight = 1,				\
		.unpack_align_msb = 0,				\
		.alpha_enable = (alpha_en),			\
		.unpack_count = 4,				\
		.bpp = 2,					\
		.frame_format = SDE_MDP_FMT_LINEAR,		\
		.pixel_mode = SDE_MDP_PIXEL_NORMAL,		\
		.element = { (e0), (e1), (e2), (e3) },		\
		.bits = {					\
			[C3_ALPHA] = SDE_COLOR_ALPHA_4BIT,	\
			[C2_R_Cr] = SDE_COLOR_4BIT,		\
			[C0_G_Y] = SDE_COLOR_4BIT,		\
			[C1_B_Cb] = SDE_COLOR_4BIT,		\
		},						\
		.is_ubwc = SDE_MDP_COMPRESS_NONE,		\
	}

#define FMT_RGB_1010102(fmt, desc, frame_fmt, flag_arg,		\
			alpha_en, e0, e1, e2, e3, isubwc)	\
	{							\
		.format = (fmt),				\
		.description = (desc),				\
		.flag = flag_arg,				\
		.fetch_planes = SDE_MDP_PLANE_INTERLEAVED,	\
		.unpack_tight = 1,				\
		.unpack_align_msb = 0,				\
		.alpha_enable = (alpha_en),			\
		.unpack_count = 4,				\
		.bpp = 4,					\
		.frame_format = frame_fmt,			\
		.pixel_mode = SDE_MDP_PIXEL_10BIT,		\
		.element = { (e0), (e1), (e2), (e3) },		\
		.bits = {					\
			[C3_ALPHA] = SDE_COLOR_8BIT,		\
			[C2_R_Cr] = SDE_COLOR_8BIT,		\
			[C0_G_Y] = SDE_COLOR_8BIT,		\
			[C1_B_Cb] = SDE_COLOR_8BIT,		\
		},						\
		.is_ubwc = isubwc,				\
	}

/*
 * UBWC formats table:
 * This table holds the UBWC formats supported.
 * If a compression ratio needs to be used for this or any other format,
 * the data will be passed by user-space.
 */
static struct sde_mdp_format_params_ubwc sde_mdp_format_ubwc_map[] = {
	{
		.mdp_format = FMT_RGB_565(SDE_PIX_FMT_RGB_565_UBWC,
			"SDE/RGB_565_UBWC",
			SDE_MDP_FMT_TILE_A5X, 0,
			C2_R_Cr, C0_G_Y, C1_B_Cb, SDE_MDP_COMPRESS_UBWC),
		.micro = {
			.tile_height = 4,
			.tile_width = 16,
		},
	},
	{
		.mdp_format = FMT_RGB_8888(SDE_PIX_FMT_RGBA_8888_UBWC,
			"SDE/RGBA_8888_UBWC",
			SDE_MDP_FMT_TILE_A5X, 0, 1,
			C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA,
			SDE_MDP_COMPRESS_UBWC),
		.micro = {
			.tile_height = 4,
			.tile_width = 16,
		},
	},
	{
		.mdp_format = FMT_RGB_8888(SDE_PIX_FMT_RGBX_8888_UBWC,
			"SDE/RGBX_8888_UBWC",
			SDE_MDP_FMT_TILE_A5X, 0, 0,
			C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA,
			SDE_MDP_COMPRESS_UBWC),
		.micro = {
			.tile_height = 4,
			.tile_width = 16,
		},
	},
	{
		.mdp_format = FMT_YUV_PSEUDO(SDE_PIX_FMT_Y_CBCR_H2V2_UBWC,
			"SDE/Y_CBCR_H2V2_UBWC",
			SDE_MDP_FMT_TILE_A5X, SDE_MDP_CHROMA_420,
			SDE_MDP_PIXEL_NORMAL,
			0, C1_B_Cb, C2_R_Cr,
			SDE_MDP_COMPRESS_UBWC),
		.micro = {
			.tile_height = 8,
			.tile_width = 32,
		},
	},
	{
		.mdp_format = FMT_RGB_1010102(SDE_PIX_FMT_RGBA_1010102_UBWC,
			"SDE/RGBA_1010102_UBWC",
			SDE_MDP_FMT_TILE_A5X, 0, 1,
			C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA,
			SDE_MDP_COMPRESS_UBWC),
		.micro = {
			.tile_height = 4,
			.tile_width = 16,
		},
	},
	{
		.mdp_format = FMT_RGB_1010102(SDE_PIX_FMT_RGBX_1010102_UBWC,
			"SDE/RGBX_1010102_UBWC",
			SDE_MDP_FMT_TILE_A5X, 0, 0,
			C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA,
			SDE_MDP_COMPRESS_UBWC),
		.micro = {
			.tile_height = 4,
			.tile_width = 16,
		},
	},
	{
		.mdp_format = FMT_YUV_PSEUDO(SDE_PIX_FMT_Y_CBCR_H2V2_TP10_UBWC,
			"SDE/Y_CBCR_H2V2_TP10_UBWC",
			SDE_MDP_FMT_TILE_A5X, SDE_MDP_CHROMA_420,
			SDE_MDP_PIXEL_10BIT,
			0,
			C1_B_Cb, C2_R_Cr, SDE_MDP_COMPRESS_UBWC),
		.micro = {
			.tile_height = 4,
			.tile_width = 48,
		},
	},
	{
		.mdp_format = {
			FMT_YUV_COMMON(SDE_PIX_FMT_Y_CBCR_H2V2_P010_UBWC),
			.description = "SDE/Y_CBCR_H2V2_P010_UBWC",
			.flag = 0,
			.fetch_planes = SDE_MDP_PLANE_PSEUDO_PLANAR,
			.chroma_sample = SDE_MDP_CHROMA_420,
			.unpack_count = 2,
			.bpp = 2,
			.frame_format = SDE_MDP_FMT_TILE_A5X,
			.pixel_mode = SDE_MDP_PIXEL_10BIT,
			.element = { C1_B_Cb, C2_R_Cr },
			.unpack_tight = 0,
			.unpack_align_msb = 1,
			.is_ubwc = SDE_MDP_COMPRESS_UBWC
		},
		.micro = {
			.tile_height = 4,
			.tile_width = 32,
		},
	},
	{
		.mdp_format =
			FMT_RGB_1010102(SDE_PIX_FMT_RGBA_1010102_TILE,
			"SDE/RGBA_1010102_TILE",
			SDE_MDP_FMT_TILE_A5X,
			SDE_MDP_FORMAT_FLAG_PRIVATE,
			1, C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA,
			SDE_MDP_COMPRESS_NONE),
		.micro = {
			.tile_height = 4,
			.tile_width = 16,
		},
	},
	{
		.mdp_format =
			FMT_RGB_1010102(SDE_PIX_FMT_RGBX_1010102_TILE,
			"SDE/RGBX_1010102102_TILE",
			SDE_MDP_FMT_TILE_A5X,
			SDE_MDP_FORMAT_FLAG_PRIVATE,
			0, C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA,
			SDE_MDP_COMPRESS_NONE),
		.micro = {
			.tile_height = 4,
			.tile_width = 16,
		},
	},
	{
		.mdp_format =
			FMT_RGB_1010102(SDE_PIX_FMT_BGRA_1010102_TILE,
			"SDE/BGRA_1010102_TILE",
			SDE_MDP_FMT_TILE_A5X,
			SDE_MDP_FORMAT_FLAG_PRIVATE,
			1, C1_B_Cb, C0_G_Y, C2_R_Cr, C3_ALPHA,
			SDE_MDP_COMPRESS_NONE),
		.micro = {
			.tile_height = 4,
			.tile_width = 16,
		},
	},
	{
		.mdp_format =
			FMT_RGB_1010102(SDE_PIX_FMT_BGRX_1010102_TILE,
			"SDE/BGRX_1010102_TILE",
			SDE_MDP_FMT_TILE_A5X,
			SDE_MDP_FORMAT_FLAG_PRIVATE,
			0, C1_B_Cb, C0_G_Y, C2_R_Cr, C3_ALPHA,
			SDE_MDP_COMPRESS_NONE),
		.micro = {
			.tile_height = 4,
			.tile_width = 16,
		},
	},
	{
		.mdp_format =
			FMT_RGB_1010102(SDE_PIX_FMT_ARGB_2101010_TILE,
			"SDE/ARGB_2101010_TILE",
			SDE_MDP_FMT_TILE_A5X,
			SDE_MDP_FORMAT_FLAG_PRIVATE,
			1, C3_ALPHA, C2_R_Cr, C0_G_Y, C1_B_Cb,
			SDE_MDP_COMPRESS_NONE),
		.micro = {
			.tile_height = 4,
			.tile_width = 16,
		},
	},
	{
		.mdp_format =
			FMT_RGB_1010102(SDE_PIX_FMT_XRGB_2101010_TILE,
			"SDE/XRGB_2101010_TILE",
			SDE_MDP_FMT_TILE_A5X,
			SDE_MDP_FORMAT_FLAG_PRIVATE,
			0, C3_ALPHA, C2_R_Cr, C0_G_Y, C1_B_Cb,
			SDE_MDP_COMPRESS_NONE),
		.micro = {
			.tile_height = 4,
			.tile_width = 16,
		},
	},
	{
		.mdp_format =
			FMT_RGB_1010102(SDE_PIX_FMT_ABGR_2101010_TILE,
			"SDE/ABGR_2101010_TILE",
			SDE_MDP_FMT_TILE_A5X,
			SDE_MDP_FORMAT_FLAG_PRIVATE,
			1, C3_ALPHA, C1_B_Cb, C0_G_Y, C2_R_Cr,
			SDE_MDP_COMPRESS_NONE),
		.micro = {
			.tile_height = 4,
			.tile_width = 16,
		},
	},
	{
		.mdp_format =
			FMT_RGB_1010102(SDE_PIX_FMT_XBGR_2101010_TILE,
			"SDE/XBGR_2101010_TILE",
			SDE_MDP_FMT_TILE_A5X,
			SDE_MDP_FORMAT_FLAG_PRIVATE,
			0, C3_ALPHA, C1_B_Cb, C0_G_Y, C2_R_Cr,
			SDE_MDP_COMPRESS_NONE),
		.micro = {
			.tile_height = 4,
			.tile_width = 16,
		},
	},
	{
		.mdp_format =
			FMT_YUV_PSEUDO(SDE_PIX_FMT_Y_CRCB_H2V2_TILE,
			"Y_CRCB_H2V2_TILE",
			SDE_MDP_FMT_TILE_A5X,
			SDE_MDP_CHROMA_420, SDE_MDP_PIXEL_NORMAL,
			SDE_MDP_FORMAT_FLAG_PRIVATE,
			C2_R_Cr, C1_B_Cb, SDE_MDP_COMPRESS_NONE),
		.micro = {
			.tile_height = 8,
			.tile_width = 32,
		},
	},
	{
		.mdp_format =
			FMT_YUV_PSEUDO(SDE_PIX_FMT_Y_CBCR_H2V2_TILE,
			"Y_CBCR_H2V2_TILE",
			SDE_MDP_FMT_TILE_A5X,
			SDE_MDP_CHROMA_420, SDE_MDP_PIXEL_NORMAL,
			SDE_MDP_FORMAT_FLAG_PRIVATE,
			C1_B_Cb, C2_R_Cr, SDE_MDP_COMPRESS_NONE),
		.micro = {
			.tile_height = 8,
			.tile_width = 32,
		},
	},
	{
		.mdp_format =
			FMT_RGB_8888(SDE_PIX_FMT_ABGR_8888_TILE,
			"SDE/ABGR_8888_TILE",
			SDE_MDP_FMT_TILE_A5X,
			SDE_MDP_FORMAT_FLAG_PRIVATE,
			1, C3_ALPHA, C1_B_Cb, C0_G_Y, C2_R_Cr,
			SDE_MDP_COMPRESS_NONE),
		.micro = {
			.tile_height = 4,
			.tile_width = 16,
		},
	},
	{
		.mdp_format =
			FMT_RGB_8888(SDE_PIX_FMT_XRGB_8888_TILE,
			"SDE/XRGB_8888_TILE",
			SDE_MDP_FMT_TILE_A5X,
			SDE_MDP_FORMAT_FLAG_PRIVATE,
			0, C3_ALPHA, C2_R_Cr, C0_G_Y, C1_B_Cb,
			SDE_MDP_COMPRESS_NONE),
		.micro = {
			.tile_height = 4,
			.tile_width = 32,
		},
	},
	{
		.mdp_format =
			FMT_RGB_8888(SDE_PIX_FMT_ARGB_8888_TILE,
			"SDE/ARGB_8888_TILE",
			SDE_MDP_FMT_TILE_A5X,
			SDE_MDP_FORMAT_FLAG_PRIVATE,
			1, C3_ALPHA, C2_R_Cr, C0_G_Y, C1_B_Cb,
			SDE_MDP_COMPRESS_NONE),
		.micro = {
			.tile_height = 4,
			.tile_width = 16,
		},
	},
	{
		.mdp_format =
			FMT_RGB_8888(SDE_PIX_FMT_RGBA_8888_TILE,
			"SDE/RGBA_8888_TILE",
			SDE_MDP_FMT_TILE_A5X,
			SDE_MDP_FORMAT_FLAG_PRIVATE,
			1, C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA,
			SDE_MDP_COMPRESS_NONE),
		.micro = {
			.tile_height = 4,
			.tile_width = 16,
		},
	},
	{
		.mdp_format =
			FMT_RGB_8888(SDE_PIX_FMT_RGBX_8888_TILE,
			"SDE/RGBX_8888_TILE",
			SDE_MDP_FMT_TILE_A5X,
			SDE_MDP_FORMAT_FLAG_PRIVATE,
			0, C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA,
			SDE_MDP_COMPRESS_NONE),
		.micro = {
			.tile_height = 4,
			.tile_width = 16,
		},
	},
	{
		.mdp_format =
			FMT_RGB_8888(SDE_PIX_FMT_BGRA_8888_TILE,
			"SDE/BGRA_8888_TILE",
			SDE_MDP_FMT_TILE_A5X,
			SDE_MDP_FORMAT_FLAG_PRIVATE,
			1, C1_B_Cb, C0_G_Y, C2_R_Cr, C3_ALPHA,
			SDE_MDP_COMPRESS_NONE),
		.micro = {
			.tile_height = 4,
			.tile_width = 16,
		},
	},
	{
		.mdp_format =
			FMT_RGB_8888(SDE_PIX_FMT_BGRX_8888_TILE,
			"SDE/BGRX_8888_TILE",
			SDE_MDP_FMT_TILE_A5X,
			SDE_MDP_FORMAT_FLAG_PRIVATE,
			0, C1_B_Cb, C0_G_Y, C2_R_Cr, C3_ALPHA,
			SDE_MDP_COMPRESS_NONE),
		.micro = {
			.tile_height = 4,
			.tile_width = 16,
		},
	},
	{
		.mdp_format =
			FMT_RGB_8888(SDE_PIX_FMT_XBGR_8888_TILE,
			"SDE/XBGR_8888_TILE",
			SDE_MDP_FMT_TILE_A5X,
			SDE_MDP_FORMAT_FLAG_PRIVATE,
			0, C3_ALPHA, C1_B_Cb, C0_G_Y, C2_R_Cr,
			SDE_MDP_COMPRESS_NONE),
		.micro = {
			.tile_height = 4,
			.tile_width = 16,
		},
	},
	{
		.mdp_format = {
			FMT_YUV_COMMON(SDE_PIX_FMT_Y_CBCR_H2V2_P010_TILE),
			.description = "SDE/Y_CBCR_H2V2_P010_TILE",
			.flag = SDE_MDP_FORMAT_FLAG_PRIVATE,
			.fetch_planes = SDE_MDP_PLANE_PSEUDO_PLANAR,
			.chroma_sample = SDE_MDP_CHROMA_420,
			.unpack_count = 2,
			.bpp = 2,
			.frame_format = SDE_MDP_FMT_TILE_A5X,
			.pixel_mode = SDE_MDP_PIXEL_10BIT,
			.element = { C1_B_Cb, C2_R_Cr },
			.unpack_tight = 0,
			.unpack_align_msb = 1,
			.is_ubwc = SDE_MDP_COMPRESS_NONE,
		},
		.micro = {
			.tile_height = 4,
			.tile_width = 32,
		},
	},
};

static struct sde_mdp_format_params sde_mdp_format_map[] = {
	FMT_RGB_565(
		SDE_PIX_FMT_RGB_565, "RGB_565", SDE_MDP_FMT_LINEAR,
		0, C1_B_Cb, C0_G_Y, C2_R_Cr, SDE_MDP_COMPRESS_NONE),
	FMT_RGB_565(
		SDE_PIX_FMT_BGR_565, "BGR_565", SDE_MDP_FMT_LINEAR,
		0, C2_R_Cr, C0_G_Y, C1_B_Cb, SDE_MDP_COMPRESS_NONE),
	FMT_RGB_888(
		SDE_PIX_FMT_RGB_888, "RGB_888", SDE_MDP_FMT_LINEAR,
		0, C2_R_Cr, C0_G_Y, C1_B_Cb, SDE_MDP_COMPRESS_NONE),
	FMT_RGB_888(
		SDE_PIX_FMT_BGR_888, "BGR_888", SDE_MDP_FMT_LINEAR,
		0, C1_B_Cb, C0_G_Y, C2_R_Cr, SDE_MDP_COMPRESS_NONE),

	FMT_RGB_8888(
		SDE_PIX_FMT_ABGR_8888, "SDE/ABGR_8888", SDE_MDP_FMT_LINEAR,
		0, 1, C3_ALPHA, C1_B_Cb, C0_G_Y, C2_R_Cr,
		SDE_MDP_COMPRESS_NONE),

	FMT_RGB_8888(
		SDE_PIX_FMT_XRGB_8888, "SDE/XRGB_8888", SDE_MDP_FMT_LINEAR,
		0, 0, C3_ALPHA, C2_R_Cr, C0_G_Y, C1_B_Cb,
		SDE_MDP_COMPRESS_NONE),
	FMT_RGB_8888(
		SDE_PIX_FMT_ARGB_8888, "SDE/ARGB_8888", SDE_MDP_FMT_LINEAR,
		0, 1, C3_ALPHA, C2_R_Cr, C0_G_Y, C1_B_Cb,
		SDE_MDP_COMPRESS_NONE),
	FMT_RGB_8888(
		SDE_PIX_FMT_RGBA_8888, "SDE/RGBA_8888", SDE_MDP_FMT_LINEAR,
		0, 1, C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA,
		SDE_MDP_COMPRESS_NONE),
	FMT_RGB_8888(
		SDE_PIX_FMT_RGBX_8888, "SDE/RGBX_8888", SDE_MDP_FMT_LINEAR,
		0, 0, C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA,
		SDE_MDP_COMPRESS_NONE),
	FMT_RGB_8888(
		SDE_PIX_FMT_BGRA_8888, "SDE/BGRA_8888", SDE_MDP_FMT_LINEAR,
		0, 1, C1_B_Cb, C0_G_Y, C2_R_Cr, C3_ALPHA,
		SDE_MDP_COMPRESS_NONE),
	FMT_RGB_8888(
		SDE_PIX_FMT_BGRX_8888, "SDE/BGRX_8888", SDE_MDP_FMT_LINEAR,
		0, 0, C1_B_Cb, C0_G_Y, C2_R_Cr, C3_ALPHA,
		SDE_MDP_COMPRESS_NONE),
	FMT_RGB_8888(
		SDE_PIX_FMT_XBGR_8888, "SDE/XBGR_8888", SDE_MDP_FMT_LINEAR,
		0, 0, C3_ALPHA, C1_B_Cb, C0_G_Y, C2_R_Cr,
		SDE_MDP_COMPRESS_NONE),

	FMT_YUV_PSEUDO(SDE_PIX_FMT_Y_CRCB_H2V1, "Y_CRCB_H2V1",
		SDE_MDP_FMT_LINEAR,
		SDE_MDP_CHROMA_H2V1, SDE_MDP_PIXEL_NORMAL,
		0, C2_R_Cr, C1_B_Cb, SDE_MDP_COMPRESS_NONE),
	FMT_YUV_PSEUDO(SDE_PIX_FMT_Y_CBCR_H2V1, "Y_CBCR_H2V1",
		SDE_MDP_FMT_LINEAR,
		SDE_MDP_CHROMA_H2V1, SDE_MDP_PIXEL_NORMAL,
		0, C1_B_Cb, C2_R_Cr, SDE_MDP_COMPRESS_NONE),
	FMT_YUV_PSEUDO(SDE_PIX_FMT_Y_CRCB_H1V2, "Y_CRCB_H1V2",
		SDE_MDP_FMT_LINEAR,
		SDE_MDP_CHROMA_H1V2, SDE_MDP_PIXEL_NORMAL,
		0, C2_R_Cr, C1_B_Cb, SDE_MDP_COMPRESS_NONE),
	FMT_YUV_PSEUDO(SDE_PIX_FMT_Y_CBCR_H1V2, "Y_CBCR_H1V2",
		SDE_MDP_FMT_LINEAR,
		SDE_MDP_CHROMA_H1V2, SDE_MDP_PIXEL_NORMAL,
		0, C1_B_Cb, C2_R_Cr, SDE_MDP_COMPRESS_NONE),
	FMT_YUV_PSEUDO(SDE_PIX_FMT_Y_CRCB_H2V2, "Y_CRCB_H2V2",
		SDE_MDP_FMT_LINEAR,
		SDE_MDP_CHROMA_420, SDE_MDP_PIXEL_NORMAL,
		0, C2_R_Cr, C1_B_Cb, SDE_MDP_COMPRESS_NONE),
	FMT_YUV_PSEUDO(SDE_PIX_FMT_Y_CBCR_H2V2, "Y_CBCR_H2V2",
		SDE_MDP_FMT_LINEAR,
		SDE_MDP_CHROMA_420, SDE_MDP_PIXEL_NORMAL,
		0, C1_B_Cb, C2_R_Cr, SDE_MDP_COMPRESS_NONE),
	FMT_YUV_PSEUDO(SDE_PIX_FMT_Y_CBCR_H2V2_VENUS, "SDE/Y_CBCR_H2V2_VENUS",
		SDE_MDP_FMT_LINEAR,
		SDE_MDP_CHROMA_420, SDE_MDP_PIXEL_NORMAL,
		0, C1_B_Cb, C2_R_Cr, SDE_MDP_COMPRESS_NONE),
	FMT_YUV_PSEUDO(SDE_PIX_FMT_Y_CRCB_H2V2_VENUS, "SDE/Y_CRCB_H2V2_VENUS",
		SDE_MDP_FMT_LINEAR,
		SDE_MDP_CHROMA_420, SDE_MDP_PIXEL_NORMAL,
		0, C2_R_Cr, C1_B_Cb, SDE_MDP_COMPRESS_NONE),

	{
		FMT_YUV10_COMMON(SDE_PIX_FMT_Y_CBCR_H2V2_P010),
		.description = "SDE/Y_CBCR_H2V2_P010",
		.flag = 0,
		.fetch_planes = SDE_MDP_PLANE_PSEUDO_PLANAR,
		.chroma_sample = SDE_MDP_CHROMA_420,
		.unpack_count = 2,
		.bpp = 2,
		.frame_format = SDE_MDP_FMT_LINEAR,
		.pixel_mode = SDE_MDP_PIXEL_10BIT,
		.element = { C1_B_Cb, C2_R_Cr },
		.unpack_tight = 0,
		.unpack_align_msb = 1,
		.is_ubwc = SDE_MDP_COMPRESS_NONE,
	},
	{
		FMT_YUV10_COMMON(SDE_PIX_FMT_Y_CBCR_H2V2_P010_VENUS),
		.description = "SDE/Y_CBCR_H2V2_P010_VENUS",
		.flag = 0,
		.fetch_planes = SDE_MDP_PLANE_PSEUDO_PLANAR,
		.chroma_sample = SDE_MDP_CHROMA_420,
		.unpack_count = 2,
		.bpp = 2,
		.frame_format = SDE_MDP_FMT_LINEAR,
		.pixel_mode = SDE_MDP_PIXEL_10BIT,
		.element = { C1_B_Cb, C2_R_Cr },
		.unpack_tight = 0,
		.unpack_align_msb = 1,
		.is_ubwc = SDE_MDP_COMPRESS_NONE,
	},
	{
		FMT_YUV_COMMON(SDE_PIX_FMT_Y_CBCR_H2V2_TP10),
		.description = "SDE/Y_CBCR_H2V2_TP10",
		.flag = 0,
		.fetch_planes = SDE_MDP_PLANE_PSEUDO_PLANAR,
		.chroma_sample = SDE_MDP_CHROMA_420,
		.unpack_count = 2,
		.bpp = 2,
		.frame_format = SDE_MDP_FMT_TILE_A5X,
		.pixel_mode = SDE_MDP_PIXEL_10BIT,
		.element = { C1_B_Cb, C2_R_Cr },
		.unpack_tight = 1,
		.unpack_align_msb = 0,
		.is_ubwc = SDE_MDP_COMPRESS_NONE,
	},

	FMT_YUV_PLANR(SDE_PIX_FMT_Y_CB_CR_H2V2, "Y_CB_CR_H2V2",
		SDE_MDP_FMT_LINEAR,
		SDE_MDP_CHROMA_420, 0, C2_R_Cr, C1_B_Cb),
	FMT_YUV_PLANR(SDE_PIX_FMT_Y_CR_CB_H2V2, "Y_CR_CB_H2V2",
		SDE_MDP_FMT_LINEAR,
		SDE_MDP_CHROMA_420, 0, C1_B_Cb, C2_R_Cr),
	FMT_YUV_PLANR(SDE_PIX_FMT_Y_CR_CB_GH2V2, "SDE/Y_CR_CB_GH2V2",
		SDE_MDP_FMT_LINEAR,
		SDE_MDP_CHROMA_420, 0, C1_B_Cb, C2_R_Cr),

	{
		FMT_YUV_COMMON(SDE_PIX_FMT_YCBYCR_H2V1),
		.description = "YCBYCR_H2V1",
		.flag = 0,
		.fetch_planes = SDE_MDP_PLANE_INTERLEAVED,
		.chroma_sample = SDE_MDP_CHROMA_H2V1,
		.unpack_count = 4,
		.bpp = 2,
		.frame_format = SDE_MDP_FMT_LINEAR,
		.pixel_mode = SDE_MDP_PIXEL_NORMAL,
		.element = { C2_R_Cr, C0_G_Y, C1_B_Cb, C0_G_Y },
		.is_ubwc = SDE_MDP_COMPRESS_NONE,
	},
	FMT_RGB_1555(SDE_PIX_FMT_RGBA_5551, "RGBA_5551", 1, 0,
		C3_ALPHA, C1_B_Cb, C0_G_Y, C2_R_Cr),
	FMT_RGB_1555(SDE_PIX_FMT_ARGB_1555, "ARGB_1555", 1, 0,
		C1_B_Cb, C0_G_Y, C2_R_Cr, C3_ALPHA),
	FMT_RGB_1555(SDE_PIX_FMT_ABGR_1555, "ABGR_1555", 1, 0,
		C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA),
	FMT_RGB_1555(SDE_PIX_FMT_BGRA_5551, "BGRA_5551", 1, 0,
		C3_ALPHA, C2_R_Cr, C0_G_Y, C1_B_Cb),
	FMT_RGB_1555(SDE_PIX_FMT_BGRX_5551, "BGRX_5551", 0, 0,
		C3_ALPHA, C2_R_Cr, C0_G_Y, C1_B_Cb),
	FMT_RGB_1555(SDE_PIX_FMT_RGBX_5551, "RGBX_5551", 0, 0,
		C3_ALPHA, C1_B_Cb, C0_G_Y, C2_R_Cr),
	FMT_RGB_1555(SDE_PIX_FMT_XBGR_1555, "XBGR_1555", 0, 0,
		C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA),
	FMT_RGB_1555(SDE_PIX_FMT_XRGB_1555, "XRGB_1555", 0, 0,
		C1_B_Cb, C0_G_Y, C2_R_Cr, C3_ALPHA),
	FMT_RGB_4444(SDE_PIX_FMT_RGBA_4444, "RGBA_4444", 1, 0,
		C3_ALPHA, C1_B_Cb, C0_G_Y, C2_R_Cr),
	FMT_RGB_4444(SDE_PIX_FMT_ARGB_4444, "ARGB_4444", 1, 0,
		C1_B_Cb, C0_G_Y, C2_R_Cr, C3_ALPHA),
	FMT_RGB_4444(SDE_PIX_FMT_BGRA_4444, "BGRA_4444", 1, 0,
		C3_ALPHA, C2_R_Cr, C0_G_Y, C1_B_Cb),
	FMT_RGB_4444(SDE_PIX_FMT_ABGR_4444, "ABGR_4444", 1, 0,
		C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA),
	FMT_RGB_4444(SDE_PIX_FMT_RGBX_4444, "RGBX_4444", 0, 0,
		C3_ALPHA, C1_B_Cb, C0_G_Y, C2_R_Cr),
	FMT_RGB_4444(SDE_PIX_FMT_XRGB_4444, "XRGB_4444", 0, 0,
		C1_B_Cb, C0_G_Y, C2_R_Cr, C3_ALPHA),
	FMT_RGB_4444(SDE_PIX_FMT_BGRX_4444, "BGRX_4444", 0, 0,
		C3_ALPHA, C2_R_Cr, C0_G_Y, C1_B_Cb),
	FMT_RGB_4444(SDE_PIX_FMT_XBGR_4444, "XBGR_4444", 0, 0,
		C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA),
	FMT_RGB_1010102(SDE_PIX_FMT_RGBA_1010102, "SDE/RGBA_1010102",
		SDE_MDP_FMT_LINEAR,
		0, 1, C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA,
		SDE_MDP_COMPRESS_NONE),
	FMT_RGB_1010102(SDE_PIX_FMT_RGBX_1010102, "SDE/RGBX_1010102",
		SDE_MDP_FMT_LINEAR,
		0, 0, C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA,
		SDE_MDP_COMPRESS_NONE),
	FMT_RGB_1010102(SDE_PIX_FMT_BGRA_1010102, "SDE/BGRA_1010102",
		SDE_MDP_FMT_LINEAR,
		0, 1, C1_B_Cb, C0_G_Y, C2_R_Cr, C3_ALPHA,
		SDE_MDP_COMPRESS_NONE),
	FMT_RGB_1010102(SDE_PIX_FMT_BGRX_1010102, "SDE/BGRX_1010102",
		SDE_MDP_FMT_LINEAR,
		0, 0, C1_B_Cb, C0_G_Y, C2_R_Cr, C3_ALPHA,
		SDE_MDP_COMPRESS_NONE),
	FMT_RGB_1010102(SDE_PIX_FMT_ARGB_2101010, "SDE/ARGB_2101010",
		SDE_MDP_FMT_LINEAR,
		0, 1, C3_ALPHA, C2_R_Cr, C0_G_Y, C1_B_Cb,
		SDE_MDP_COMPRESS_NONE),
	FMT_RGB_1010102(SDE_PIX_FMT_XRGB_2101010, "SDE/XRGB_2101010",
		SDE_MDP_FMT_LINEAR,
		0, 0, C3_ALPHA, C2_R_Cr, C0_G_Y, C1_B_Cb,
		SDE_MDP_COMPRESS_NONE),
	FMT_RGB_1010102(SDE_PIX_FMT_ABGR_2101010, "SDE/ABGR_2101010",
		SDE_MDP_FMT_LINEAR,
		0, 1, C3_ALPHA, C1_B_Cb, C0_G_Y, C2_R_Cr,
		SDE_MDP_COMPRESS_NONE),
	FMT_RGB_1010102(SDE_PIX_FMT_XBGR_2101010, "SDE/XBGR_2101010",
		SDE_MDP_FMT_LINEAR,
		0, 0, C3_ALPHA, C1_B_Cb, C0_G_Y, C2_R_Cr,
		SDE_MDP_COMPRESS_NONE),
};

/*
 * sde_get_format_params - return format parameter of the given format
 * @format: format to lookup
 */
struct sde_mdp_format_params *sde_get_format_params(u32 format)
{
	struct sde_mdp_format_params *fmt = NULL;
	int i;
	bool fmt_found = false;

	for (i = 0; i < ARRAY_SIZE(sde_mdp_format_map); i++) {
		fmt = &sde_mdp_format_map[i];
		if (format == fmt->format) {
			fmt_found = true;
			break;
		}
	}

	if (!fmt_found) {
		for (i = 0; i < ARRAY_SIZE(sde_mdp_format_ubwc_map); i++) {
			fmt = &sde_mdp_format_ubwc_map[i].mdp_format;
			if (format == fmt->format) {
				fmt_found = true;
				break;
			}
		}
	}
	/* If format not supported than return NULL */
	if (!fmt_found)
		fmt = NULL;

	return fmt;
}

/*
 * sde_rot_get_ubwc_micro_dim - return micro dimension of the given ubwc format
 * @format: format to lookup
 * @w: Pointer to returned width dimension
 * @h: Pointer to returned height dimension
 */
int sde_rot_get_ubwc_micro_dim(u32 format, u16 *w, u16 *h)
{
	struct sde_mdp_format_params_ubwc *fmt = NULL;
	bool fmt_found = false;
	int i;

	for (i = 0; i < ARRAY_SIZE(sde_mdp_format_ubwc_map); i++) {
		fmt = &sde_mdp_format_ubwc_map[i];
		if (format == fmt->mdp_format.format) {
			fmt_found = true;
			break;
		}
	}

	if (!fmt_found)
		return -EINVAL;

	*w = fmt->micro.tile_width;
	*h = fmt->micro.tile_height;

	return 0;
}

/*
 * sde_rot_get_tilea5x_pixfmt - get base a5x tile format of given source format
 * @src_pixfmt: source pixel format to be converted
 * @dst_pixfmt: pointer to base a5x tile pixel format
 * return: 0 if success; error code otherwise
 */
int sde_rot_get_base_tilea5x_pixfmt(u32 src_pixfmt, u32 *dst_pixfmt)
{
	int rc = 0;

	if (!dst_pixfmt) {
		SDEROT_ERR("invalid parameters\n");
		return -EINVAL;
	}

	switch (src_pixfmt) {
	case SDE_PIX_FMT_Y_CBCR_H2V2:
	case SDE_PIX_FMT_Y_CBCR_H2V2_UBWC:
	case SDE_PIX_FMT_Y_CBCR_H2V2_TILE:
		*dst_pixfmt = SDE_PIX_FMT_Y_CBCR_H2V2_TILE;
		break;
	case SDE_PIX_FMT_Y_CRCB_H2V2:
	case SDE_PIX_FMT_Y_CRCB_H2V2_TILE:
		*dst_pixfmt = SDE_PIX_FMT_Y_CRCB_H2V2_TILE;
		break;
	case V4L2_PIX_FMT_RGB565:
	case SDE_PIX_FMT_RGB_565_UBWC:
	case SDE_PIX_FMT_RGB_565_TILE:
		*dst_pixfmt = SDE_PIX_FMT_RGB_565_TILE;
		break;
	case SDE_PIX_FMT_RGBA_8888:
	case SDE_PIX_FMT_RGBA_8888_UBWC:
	case SDE_PIX_FMT_RGBA_8888_TILE:
		*dst_pixfmt = SDE_PIX_FMT_RGBA_8888_TILE;
		break;
	case SDE_PIX_FMT_RGBX_8888:
	case SDE_PIX_FMT_RGBX_8888_UBWC:
	case SDE_PIX_FMT_RGBX_8888_TILE:
		*dst_pixfmt = SDE_PIX_FMT_RGBX_8888_TILE;
		break;
	case SDE_PIX_FMT_ARGB_8888:
	case SDE_PIX_FMT_ARGB_8888_TILE:
		*dst_pixfmt = SDE_PIX_FMT_ARGB_8888_TILE;
		break;
	case SDE_PIX_FMT_XRGB_8888:
	case SDE_PIX_FMT_XRGB_8888_TILE:
		*dst_pixfmt = SDE_PIX_FMT_XRGB_8888_TILE;
		break;
	case SDE_PIX_FMT_ABGR_8888:
	case SDE_PIX_FMT_ABGR_8888_TILE:
		*dst_pixfmt = SDE_PIX_FMT_ABGR_8888_TILE;
		break;
	case SDE_PIX_FMT_XBGR_8888:
	case SDE_PIX_FMT_XBGR_8888_TILE:
		*dst_pixfmt = SDE_PIX_FMT_XBGR_8888_TILE;
		break;
	case SDE_PIX_FMT_ARGB_2101010:
	case SDE_PIX_FMT_ARGB_2101010_TILE:
		*dst_pixfmt = SDE_PIX_FMT_ARGB_2101010_TILE;
		break;
	case SDE_PIX_FMT_XRGB_2101010:
	case SDE_PIX_FMT_XRGB_2101010_TILE:
		*dst_pixfmt = SDE_PIX_FMT_XRGB_2101010_TILE;
		break;
	case SDE_PIX_FMT_ABGR_2101010:
	case SDE_PIX_FMT_ABGR_2101010_TILE:
		*dst_pixfmt = SDE_PIX_FMT_ABGR_2101010_TILE;
		break;
	case SDE_PIX_FMT_XBGR_2101010:
	case SDE_PIX_FMT_XBGR_2101010_TILE:
		*dst_pixfmt = SDE_PIX_FMT_XBGR_2101010_TILE;
		break;
	case SDE_PIX_FMT_BGRA_1010102:
	case SDE_PIX_FMT_BGRA_1010102_TILE:
		*dst_pixfmt = SDE_PIX_FMT_BGRA_1010102_TILE;
		break;
	case SDE_PIX_FMT_BGRX_1010102:
	case SDE_PIX_FMT_BGRX_1010102_TILE:
		*dst_pixfmt = SDE_PIX_FMT_BGRX_1010102_TILE;
		break;
	case SDE_PIX_FMT_Y_CBCR_H2V2_P010:
	case SDE_PIX_FMT_Y_CBCR_H2V2_P010_TILE:
	case SDE_PIX_FMT_Y_CBCR_H2V2_P010_UBWC:
		*dst_pixfmt = SDE_PIX_FMT_Y_CBCR_H2V2_P010_TILE;
		break;
	case SDE_PIX_FMT_Y_CBCR_H2V2_TP10:
	case SDE_PIX_FMT_Y_CBCR_H2V2_TP10_UBWC:
		*dst_pixfmt = SDE_PIX_FMT_Y_CBCR_H2V2_TP10;
		break;
	default:
		SDEROT_ERR("invalid src pixel format %c%c%c%c\n",
				src_pixfmt >> 0, src_pixfmt >> 8,
				src_pixfmt >> 16, src_pixfmt >> 24);
		rc = -EINVAL;
		break;
	}

	return rc;
}
