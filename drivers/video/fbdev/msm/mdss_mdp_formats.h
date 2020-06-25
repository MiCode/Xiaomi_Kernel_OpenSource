/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2012-2016, 2018, 2020, The Linux Foundation. All rights reserved. */

#ifndef MDSS_MDP_FORMATS_H
#define MDSS_MDP_FORMATS_H

#include <linux/msm_mdp.h>

#include "mdss_mdp.h"

	/*
	 * Value of enum chosen to fit the number of bits
	 * expected by the HW programming.
	 */
enum {
	COLOR_4BIT,
	COLOR_5BIT,
	COLOR_6BIT,
	COLOR_8BIT,
	COLOR_ALPHA_1BIT = 0,
	COLOR_ALPHA_4BIT = 1,
};

#define UBWC_META_MACRO_W_H 16
#define UBWC_META_BLOCK_SIZE 256

#define FMT_RGB_565(fmt, fetch_type, flag_arg, e0, e1, e2)		\
	{							\
		.format = (fmt),				\
		.flag = flag_arg,					\
		.fetch_planes = MDSS_MDP_PLANE_INTERLEAVED,	\
		.unpack_tight = 1,				\
		.unpack_align_msb = 0,				\
		.alpha_enable = 0,				\
		.unpack_count = 3,				\
		.bpp = 2,					\
		.fetch_mode = (fetch_type),			\
		.element = { (e0), (e1), (e2) },		\
		.bits = {					\
			[C2_R_Cr] = COLOR_5BIT,			\
			[C0_G_Y] = COLOR_6BIT,			\
			[C1_B_Cb] = COLOR_5BIT,			\
		},						\
	}

#define FMT_RGB_888(fmt, fetch_type, flag_arg, e0, e1, e2)		\
	{							\
		.format = (fmt),				\
		.flag = flag_arg,					\
		.fetch_planes = MDSS_MDP_PLANE_INTERLEAVED,	\
		.unpack_tight = 1,				\
		.unpack_align_msb = 0,				\
		.alpha_enable = 0,				\
		.unpack_count = 3,				\
		.bpp = 3,					\
		.fetch_mode = (fetch_type),			\
		.element = { (e0), (e1), (e2) },		\
		.bits = {					\
			[C2_R_Cr] = COLOR_8BIT,			\
			[C0_G_Y] = COLOR_8BIT,			\
			[C1_B_Cb] = COLOR_8BIT,			\
		},						\
	}

#define FMT_RGB_8888(fmt, fetch_type, flag_arg,	\
		alpha_en, e0, e1, e2, e3)	\
	{							\
		.format = (fmt),				\
		.flag = flag_arg,					\
		.fetch_planes = MDSS_MDP_PLANE_INTERLEAVED,	\
		.unpack_tight = 1,				\
		.unpack_align_msb = 0,				\
		.alpha_enable = (alpha_en),			\
		.unpack_count = 4,				\
		.bpp = 4,					\
		.fetch_mode = (fetch_type),			\
		.element = { (e0), (e1), (e2), (e3) },		\
		.bits = {					\
			[C3_ALPHA] = COLOR_8BIT,		\
			[C2_R_Cr] = COLOR_8BIT,			\
			[C0_G_Y] = COLOR_8BIT,			\
			[C1_B_Cb] = COLOR_8BIT,			\
		},						\
	}

#define FMT_YUV_COMMON(fmt)					\
		.format = (fmt),				\
		.is_yuv = 1,					\
		.bits = {					\
			[C2_R_Cr] = COLOR_8BIT,			\
			[C0_G_Y] = COLOR_8BIT,			\
			[C1_B_Cb] = COLOR_8BIT,			\
		},						\
		.alpha_enable = 0,				\
		.unpack_tight = 1,				\
		.unpack_align_msb = 0

#define FMT_YUV_PSEUDO(fmt, fetch_type, samp, \
		flag_arg, e0, e1)		\
	{							\
		FMT_YUV_COMMON(fmt),				\
		.flag = flag_arg,					\
		.fetch_planes = MDSS_MDP_PLANE_PSEUDO_PLANAR,	\
		.chroma_sample = samp,				\
		.unpack_count = 2,				\
		.bpp = 2,					\
		.fetch_mode = (fetch_type),			\
		.element = { (e0), (e1) },			\
	}

#define FMT_YUV_PLANR(fmt, fetch_type, samp, \
		flag_arg, e0, e1)		\
	{							\
		FMT_YUV_COMMON(fmt),				\
		.flag = flag_arg,					\
		.fetch_planes = MDSS_MDP_PLANE_PLANAR,		\
		.chroma_sample = samp,				\
		.bpp = 1,					\
		.unpack_count = 1,				\
		.fetch_mode = (fetch_type),			\
		.element = { (e0), (e1) }			\
	}

#define FMT_RGB_1555(fmt, alpha_en, flag_arg, e0, e1, e2, e3)		\
	{							\
		.format = (fmt),				\
		.flag = flag_arg,					\
		.fetch_planes = MDSS_MDP_PLANE_INTERLEAVED,	\
		.unpack_tight = 1,				\
		.unpack_align_msb = 0,				\
		.alpha_enable = (alpha_en),			\
		.unpack_count = 4,				\
		.bpp = 2,					\
		.element = { (e0), (e1), (e2), (e3) },		\
		.fetch_mode = MDSS_MDP_FETCH_LINEAR,		\
		.bits = {					\
			[C3_ALPHA] = COLOR_ALPHA_1BIT,		\
			[C2_R_Cr] = COLOR_5BIT,			\
			[C0_G_Y] = COLOR_5BIT,			\
			[C1_B_Cb] = COLOR_5BIT,			\
		},						\
	}

#define FMT_RGB_4444(fmt, alpha_en, flag_arg, e0, e1, e2, e3)		\
	{							\
		.format = (fmt),				\
		.flag = flag_arg,					\
		.fetch_planes = MDSS_MDP_PLANE_INTERLEAVED,	\
		.unpack_tight = 1,				\
		.unpack_align_msb = 0,				\
		.alpha_enable = (alpha_en),			\
		.unpack_count = 4,				\
		.bpp = 2,					\
		.fetch_mode = MDSS_MDP_FETCH_LINEAR,		\
		.element = { (e0), (e1), (e2), (e3) },		\
		.bits = {					\
			[C3_ALPHA] = COLOR_ALPHA_4BIT,		\
			[C2_R_Cr] = COLOR_4BIT,			\
			[C0_G_Y] = COLOR_4BIT,			\
			[C1_B_Cb] = COLOR_4BIT,			\
		},						\
	}

#define FMT_RGB_2101010(fmt, fetch_type, flag_arg,	\
		alpha_en, e0, e1, e2, e3)	\
	{							\
		.format = (fmt),				\
		.flag = flag_arg,					\
		.fetch_planes = MDSS_MDP_PLANE_INTERLEAVED,	\
		.unpack_tight = 1,				\
		.unpack_align_msb = 0,				\
		.alpha_enable = (alpha_en),			\
		.unpack_count = 4,				\
		.bpp = 4,					\
		.fetch_mode = (fetch_type),			\
		.element = { (e0), (e1), (e2), (e3) },		\
		.bits = {					\
			[C3_ALPHA] = COLOR_8BIT,		\
			[C2_R_Cr] = COLOR_8BIT,			\
			[C0_G_Y] = COLOR_8BIT,			\
			[C1_B_Cb] = COLOR_8BIT,			\
		},						\
		.unpack_dx_format = 1,	\
	}

#define FMT_YUV_PSEUDO_10(fmt, fetch_type, samp, \
		flag_arg, e0, e1, unpack_type, unpack_align)		\
	{							\
		FMT_YUV_COMMON(fmt),				\
		.flag = flag_arg,					\
		.fetch_planes = MDSS_MDP_PLANE_PSEUDO_PLANAR,	\
		.chroma_sample = samp,				\
		.unpack_count = 2,				\
		.bpp = 2,					\
		.fetch_mode = (fetch_type),			\
		.element = { (e0), (e1) },			\
		.unpack_dx_format = 1,	\
		.unpack_tight = unpack_type,	\
		.unpack_align_msb = unpack_align,	\
	}

/*
 * UBWC formats table:
 * This table holds the UBWC formats supported.
 * If a compression ratio needs to be used for this or any other format,
 * the data will be passed by user-space.
 */
static struct mdss_mdp_format_params_ubwc mdss_mdp_format_ubwc_map[] = {
	{
		.mdp_format = FMT_RGB_565(MDP_RGB_565_UBWC,
			MDSS_MDP_FETCH_UBWC,
			VALID_ROT_WB_FORMAT | VALID_MDP_WB_INTF_FORMAT,
			C2_R_Cr, C0_G_Y, C1_B_Cb),
		.micro = {
			.tile_height = 4,
			.tile_width = 16,
		},
	},
	{
		.mdp_format = FMT_RGB_8888(MDP_RGBA_8888_UBWC,
			MDSS_MDP_FETCH_UBWC,
			VALID_ROT_WB_FORMAT | VALID_MDP_WB_INTF_FORMAT, 1,
			C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA),
		.micro = {
			.tile_height = 4,
			.tile_width = 16,
		},
	},
	{
		.mdp_format = FMT_RGB_8888(MDP_RGBX_8888_UBWC,
			MDSS_MDP_FETCH_UBWC,
			VALID_ROT_WB_FORMAT | VALID_MDP_WB_INTF_FORMAT, 0,
			C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA),
		.micro = {
			.tile_height = 4,
			.tile_width = 16,
		},
	},
	{
		.mdp_format = FMT_YUV_PSEUDO(MDP_Y_CBCR_H2V2_UBWC,
			MDSS_MDP_FETCH_UBWC, MDSS_MDP_CHROMA_420,
			VALID_ROT_WB_FORMAT | VALID_MDP_WB_INTF_FORMAT,
			C1_B_Cb, C2_R_Cr),
		.micro = {
			.tile_height = 8,
			.tile_width = 32,
		},
	},
	{
		.mdp_format = FMT_RGB_2101010(MDP_RGBA_1010102_UBWC,
			MDSS_MDP_FETCH_UBWC,
			VALID_ROT_WB_FORMAT | VALID_MDP_WB_INTF_FORMAT,
			1, C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA),
		.micro = {
			.tile_height = 4,
			.tile_width = 16,
		},
	},
	{
		.mdp_format = FMT_RGB_2101010(MDP_RGBX_1010102_UBWC,
			MDSS_MDP_FETCH_UBWC,
			VALID_ROT_WB_FORMAT | VALID_MDP_WB_INTF_FORMAT,
			0, C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA),
		.micro = {
			.tile_height = 4,
			.tile_width = 16,
		},
	},
	{
		.mdp_format = FMT_YUV_PSEUDO_10(MDP_Y_CBCR_H2V2_TP10_UBWC,
			MDSS_MDP_FETCH_UBWC, MDSS_MDP_CHROMA_420,
			VALID_ROT_WB_FORMAT | VALID_MDP_WB_INTF_FORMAT,
			C1_B_Cb, C2_R_Cr, 1, 0),
		.micro = {
			.tile_height = 4,
			.tile_width = 48,
		},
	},
};

static struct mdss_mdp_format_params mdss_mdp_format_map[] = {
	FMT_RGB_565(MDP_RGB_565, MDSS_MDP_FETCH_LINEAR, VALID_ROT_WB_FORMAT |
		VALID_MDP_WB_INTF_FORMAT, C1_B_Cb, C0_G_Y, C2_R_Cr),
	FMT_RGB_565(MDP_BGR_565, MDSS_MDP_FETCH_LINEAR, VALID_ROT_WB_FORMAT |
		VALID_MDP_WB_INTF_FORMAT, C2_R_Cr, C0_G_Y, C1_B_Cb),
	FMT_RGB_565(MDP_RGB_565_TILE, MDSS_MDP_FETCH_TILE, VALID_ROT_WB_FORMAT,
		C1_B_Cb, C0_G_Y, C2_R_Cr),
	FMT_RGB_565(MDP_BGR_565_TILE, MDSS_MDP_FETCH_TILE, VALID_ROT_WB_FORMAT,
		C2_R_Cr, C0_G_Y, C1_B_Cb),
	FMT_RGB_888(MDP_RGB_888, MDSS_MDP_FETCH_LINEAR, VALID_ROT_WB_FORMAT |
		VALID_MDP_WB_INTF_FORMAT, C2_R_Cr, C0_G_Y, C1_B_Cb),
	FMT_RGB_888(MDP_BGR_888, MDSS_MDP_FETCH_LINEAR, VALID_ROT_WB_FORMAT |
		VALID_MDP_WB_INTF_FORMAT, C1_B_Cb, C0_G_Y, C2_R_Cr),

	FMT_RGB_8888(MDP_XRGB_8888, MDSS_MDP_FETCH_LINEAR, VALID_ROT_WB_FORMAT |
		VALID_MDP_WB_INTF_FORMAT | VALID_MDP_CURSOR_FORMAT, 0,
		C3_ALPHA, C2_R_Cr, C0_G_Y, C1_B_Cb),
	FMT_RGB_8888(MDP_ARGB_8888, MDSS_MDP_FETCH_LINEAR, VALID_ROT_WB_FORMAT |
		VALID_MDP_CURSOR_FORMAT | VALID_MDP_WB_INTF_FORMAT,
		1, C3_ALPHA, C2_R_Cr, C0_G_Y, C1_B_Cb),
	FMT_RGB_8888(MDP_ABGR_8888, MDSS_MDP_FETCH_LINEAR, VALID_ROT_WB_FORMAT |
		VALID_MDP_WB_INTF_FORMAT, 1,
		C3_ALPHA, C1_B_Cb, C0_G_Y, C2_R_Cr),
	FMT_RGB_8888(MDP_RGBA_8888, MDSS_MDP_FETCH_LINEAR, VALID_ROT_WB_FORMAT |
		VALID_MDP_CURSOR_FORMAT | VALID_MDP_WB_INTF_FORMAT, 1,
		C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA),
	FMT_RGB_8888(MDP_RGBX_8888, MDSS_MDP_FETCH_LINEAR, VALID_ROT_WB_FORMAT |
		VALID_MDP_WB_INTF_FORMAT, 0, C2_R_Cr, C0_G_Y, C1_B_Cb,
		C3_ALPHA),
	FMT_RGB_8888(MDP_BGRA_8888, MDSS_MDP_FETCH_LINEAR, VALID_ROT_WB_FORMAT |
		VALID_MDP_CURSOR_FORMAT | VALID_MDP_WB_INTF_FORMAT,
		1, C1_B_Cb, C0_G_Y, C2_R_Cr, C3_ALPHA),
	FMT_RGB_8888(MDP_BGRX_8888, MDSS_MDP_FETCH_LINEAR, VALID_ROT_WB_FORMAT |
		VALID_MDP_WB_INTF_FORMAT, 0, C1_B_Cb, C0_G_Y, C2_R_Cr,
		C3_ALPHA),
	FMT_RGB_8888(MDP_XBGR_8888, MDSS_MDP_FETCH_LINEAR, VALID_ROT_WB_FORMAT |
		VALID_MDP_WB_INTF_FORMAT, 0, C3_ALPHA, C1_B_Cb, C0_G_Y,
		C2_R_Cr),
	FMT_RGB_8888(MDP_RGBA_8888_TILE, MDSS_MDP_FETCH_TILE,
		VALID_ROT_WB_FORMAT, 1, C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA),
	FMT_RGB_8888(MDP_ARGB_8888_TILE, MDSS_MDP_FETCH_TILE,
		VALID_ROT_WB_FORMAT, 1, C3_ALPHA, C2_R_Cr, C0_G_Y, C1_B_Cb),
	FMT_RGB_8888(MDP_ABGR_8888_TILE, MDSS_MDP_FETCH_TILE,
		VALID_ROT_WB_FORMAT, 1, C3_ALPHA, C1_B_Cb, C0_G_Y, C2_R_Cr),
	FMT_RGB_8888(MDP_BGRA_8888_TILE, MDSS_MDP_FETCH_TILE,
		VALID_ROT_WB_FORMAT, 1, C1_B_Cb, C0_G_Y, C2_R_Cr, C3_ALPHA),
	FMT_RGB_8888(MDP_RGBX_8888_TILE, MDSS_MDP_FETCH_TILE,
		VALID_ROT_WB_FORMAT, 0, C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA),
	FMT_RGB_8888(MDP_XRGB_8888_TILE, MDSS_MDP_FETCH_TILE,
		VALID_ROT_WB_FORMAT, 0, C3_ALPHA, C2_R_Cr, C0_G_Y, C1_B_Cb),
	FMT_RGB_8888(MDP_XBGR_8888_TILE, MDSS_MDP_FETCH_TILE,
		VALID_ROT_WB_FORMAT, 0, C3_ALPHA, C1_B_Cb, C0_G_Y, C2_R_Cr),
	FMT_RGB_8888(MDP_BGRX_8888_TILE, MDSS_MDP_FETCH_TILE,
		VALID_ROT_WB_FORMAT, 0, C1_B_Cb, C0_G_Y, C2_R_Cr, C3_ALPHA),

	FMT_YUV_PSEUDO(MDP_Y_CRCB_H1V1, MDSS_MDP_FETCH_LINEAR,
		MDSS_MDP_CHROMA_RGB, 0, C2_R_Cr, C1_B_Cb),
	FMT_YUV_PSEUDO(MDP_Y_CBCR_H1V1, MDSS_MDP_FETCH_LINEAR,
		MDSS_MDP_CHROMA_RGB, 0, C1_B_Cb, C2_R_Cr),
	FMT_YUV_PSEUDO(MDP_Y_CRCB_H2V1, MDSS_MDP_FETCH_LINEAR,
		MDSS_MDP_CHROMA_H2V1, VALID_ROT_WB_FORMAT, C2_R_Cr, C1_B_Cb),
	FMT_YUV_PSEUDO(MDP_Y_CBCR_H2V1, MDSS_MDP_FETCH_LINEAR,
		MDSS_MDP_CHROMA_H2V1, VALID_ROT_WB_FORMAT |
		VALID_MDP_WB_INTF_FORMAT, C1_B_Cb, C2_R_Cr),
	FMT_YUV_PSEUDO(MDP_Y_CRCB_H1V2, MDSS_MDP_FETCH_LINEAR,
		MDSS_MDP_CHROMA_H1V2, VALID_ROT_WB_FORMAT |
		VALID_MDP_WB_INTF_FORMAT, C2_R_Cr, C1_B_Cb),
	FMT_YUV_PSEUDO(MDP_Y_CBCR_H1V2, MDSS_MDP_FETCH_LINEAR,
		MDSS_MDP_CHROMA_H1V2, VALID_ROT_WB_FORMAT |
		VALID_MDP_WB_INTF_FORMAT, C1_B_Cb, C2_R_Cr),
	FMT_YUV_PSEUDO(MDP_Y_CRCB_H2V2, MDSS_MDP_FETCH_LINEAR,
		MDSS_MDP_CHROMA_420, VALID_ROT_WB_FORMAT |
		VALID_MDP_WB_INTF_FORMAT, C2_R_Cr, C1_B_Cb),
	FMT_YUV_PSEUDO(MDP_Y_CBCR_H2V2, MDSS_MDP_FETCH_LINEAR,
		MDSS_MDP_CHROMA_420, VALID_ROT_WB_FORMAT |
		VALID_MDP_WB_INTF_FORMAT, C1_B_Cb, C2_R_Cr),
	FMT_YUV_PSEUDO(MDP_Y_CBCR_H2V2_VENUS, MDSS_MDP_FETCH_LINEAR,
		MDSS_MDP_CHROMA_420, VALID_ROT_WB_FORMAT |
		VALID_MDP_WB_INTF_FORMAT, C1_B_Cb, C2_R_Cr),
	FMT_YUV_PSEUDO(MDP_Y_CRCB_H2V2_VENUS, MDSS_MDP_FETCH_LINEAR,
		MDSS_MDP_CHROMA_420, VALID_ROT_WB_FORMAT |
		VALID_MDP_WB_INTF_FORMAT, C2_R_Cr, C1_B_Cb),

	FMT_YUV_PLANR(MDP_Y_CB_CR_H2V2, MDSS_MDP_FETCH_LINEAR,
		MDSS_MDP_CHROMA_420, VALID_ROT_WB_FORMAT |
		VALID_MDP_WB_INTF_FORMAT, C2_R_Cr, C1_B_Cb),
	FMT_YUV_PLANR(MDP_Y_CR_CB_H2V2, MDSS_MDP_FETCH_LINEAR,
		MDSS_MDP_CHROMA_420, VALID_ROT_WB_FORMAT |
		VALID_MDP_WB_INTF_FORMAT, C1_B_Cb, C2_R_Cr),
	FMT_YUV_PLANR(MDP_Y_CR_CB_GH2V2, MDSS_MDP_FETCH_LINEAR,
		MDSS_MDP_CHROMA_420, VALID_ROT_WB_FORMAT |
		VALID_MDP_WB_INTF_FORMAT, C1_B_Cb, C2_R_Cr),

	{
		FMT_YUV_COMMON(MDP_YCBCR_H1V1),
		.fetch_planes = MDSS_MDP_PLANE_INTERLEAVED,
		.chroma_sample = MDSS_MDP_CHROMA_RGB,
		.unpack_count = 3,
		.bpp = 3,
		.fetch_mode = MDSS_MDP_FETCH_LINEAR,
		.element = { C0_G_Y, C1_B_Cb, C2_R_Cr },
	},
	{
		FMT_YUV_COMMON(MDP_YCRCB_H1V1),
		.fetch_planes = MDSS_MDP_PLANE_INTERLEAVED,
		.chroma_sample = MDSS_MDP_CHROMA_RGB,
		.unpack_count = 3,
		.bpp = 3,
		.fetch_mode = MDSS_MDP_FETCH_LINEAR,
		.element = { C0_G_Y, C2_R_Cr, C1_B_Cb },
	},
	{
		FMT_YUV_COMMON(MDP_YCRYCB_H2V1),
		.fetch_planes = MDSS_MDP_PLANE_INTERLEAVED,
		.chroma_sample = MDSS_MDP_CHROMA_H2V1,
		.unpack_count = 4,
		.bpp = 2,
		.fetch_mode = MDSS_MDP_FETCH_LINEAR,
		.element = { C0_G_Y, C2_R_Cr, C0_G_Y, C1_B_Cb },
	},
	{
		FMT_YUV_COMMON(MDP_YCBYCR_H2V1),
		.flag = VALID_MDP_WB_INTF_FORMAT,
		.fetch_planes = MDSS_MDP_PLANE_INTERLEAVED,
		.chroma_sample = MDSS_MDP_CHROMA_H2V1,
		.unpack_count = 4,
		.bpp = 2,
		.fetch_mode = MDSS_MDP_FETCH_LINEAR,
		.element = { C0_G_Y, C1_B_Cb, C0_G_Y, C2_R_Cr },
	},
	{
		FMT_YUV_COMMON(MDP_CRYCBY_H2V1),
		.fetch_planes = MDSS_MDP_PLANE_INTERLEAVED,
		.chroma_sample = MDSS_MDP_CHROMA_H2V1,
		.unpack_count = 4,
		.bpp = 2,
		.fetch_mode = MDSS_MDP_FETCH_LINEAR,
		.element = { C2_R_Cr, C0_G_Y, C1_B_Cb, C0_G_Y },
	},
	{
		FMT_YUV_COMMON(MDP_CBYCRY_H2V1),
		.fetch_planes = MDSS_MDP_PLANE_INTERLEAVED,
		.chroma_sample = MDSS_MDP_CHROMA_H2V1,
		.unpack_count = 4,
		.bpp = 2,
		.fetch_mode = MDSS_MDP_FETCH_LINEAR,
		.element = { C1_B_Cb, C0_G_Y, C2_R_Cr, C0_G_Y},
	},
	FMT_RGB_1555(MDP_RGBA_5551, 1, VALID_ROT_WB_FORMAT |
		VALID_MDP_CURSOR_FORMAT | VALID_MDP_WB_INTF_FORMAT,
		C3_ALPHA, C1_B_Cb, C0_G_Y, C2_R_Cr),
	FMT_RGB_1555(MDP_ARGB_1555, 1, VALID_ROT_WB_FORMAT |
		VALID_MDP_CURSOR_FORMAT | VALID_MDP_WB_INTF_FORMAT,
		C1_B_Cb, C0_G_Y, C2_R_Cr, C3_ALPHA),
	FMT_RGB_1555(MDP_ABGR_1555, 1, VALID_ROT_WB_FORMAT |
		VALID_MDP_WB_INTF_FORMAT, C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA),
	FMT_RGB_1555(MDP_BGRA_5551, 1, VALID_ROT_WB_FORMAT |
		VALID_MDP_WB_INTF_FORMAT, C3_ALPHA, C2_R_Cr, C0_G_Y, C1_B_Cb),
	FMT_RGB_1555(MDP_BGRX_5551, 0, VALID_ROT_WB_FORMAT |
		VALID_MDP_WB_INTF_FORMAT, C3_ALPHA, C2_R_Cr, C0_G_Y, C1_B_Cb),
	FMT_RGB_1555(MDP_RGBX_5551, 0, VALID_ROT_WB_FORMAT |
		VALID_MDP_WB_INTF_FORMAT, C3_ALPHA, C1_B_Cb, C0_G_Y, C2_R_Cr),
	FMT_RGB_1555(MDP_XBGR_1555, 0, VALID_ROT_WB_FORMAT |
		VALID_MDP_WB_INTF_FORMAT, C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA),
	FMT_RGB_1555(MDP_XRGB_1555, 0, VALID_ROT_WB_FORMAT |
		VALID_MDP_WB_INTF_FORMAT, C1_B_Cb, C0_G_Y, C2_R_Cr, C3_ALPHA),
	FMT_RGB_4444(MDP_ABGR_4444, 1, VALID_ROT_WB_FORMAT |
		VALID_MDP_WB_INTF_FORMAT, C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA),
	FMT_RGB_4444(MDP_BGRA_4444, 1, VALID_ROT_WB_FORMAT |
		VALID_MDP_CURSOR_FORMAT | VALID_MDP_WB_INTF_FORMAT,
		C3_ALPHA, C2_R_Cr, C0_G_Y, C1_B_Cb),
	FMT_RGB_4444(MDP_BGRX_4444, 0, VALID_ROT_WB_FORMAT |
		VALID_MDP_WB_INTF_FORMAT, C3_ALPHA, C2_R_Cr, C0_G_Y, C1_B_Cb),
	FMT_RGB_4444(MDP_RGBX_4444, 0, VALID_ROT_WB_FORMAT |
		VALID_MDP_WB_INTF_FORMAT, C3_ALPHA, C1_B_Cb, C0_G_Y, C2_R_Cr),
	FMT_RGB_4444(MDP_XBGR_4444, 0, VALID_ROT_WB_FORMAT |
		VALID_MDP_WB_INTF_FORMAT, C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA),
	FMT_RGB_4444(MDP_XRGB_4444, 0, VALID_ROT_WB_FORMAT |
		VALID_MDP_WB_INTF_FORMAT, C1_B_Cb, C0_G_Y, C2_R_Cr, C3_ALPHA),

	FMT_RGB_4444(MDP_RGBA_4444, 1, VALID_ROT_WB_FORMAT |
		VALID_MDP_CURSOR_FORMAT | VALID_MDP_WB_INTF_FORMAT,
		C3_ALPHA, C1_B_Cb, C0_G_Y, C2_R_Cr),
	FMT_RGB_4444(MDP_ARGB_4444, 1, VALID_ROT_WB_FORMAT |
		VALID_MDP_CURSOR_FORMAT | VALID_MDP_WB_INTF_FORMAT,
		C1_B_Cb, C0_G_Y, C2_R_Cr, C3_ALPHA),

	FMT_RGB_2101010(MDP_RGBA_1010102, MDSS_MDP_FETCH_LINEAR,
		VALID_ROT_WB_FORMAT | VALID_MDP_WB_INTF_FORMAT, 1,
		C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA),
	FMT_RGB_2101010(MDP_ARGB_2101010, MDSS_MDP_FETCH_LINEAR, 0, 1,
		C3_ALPHA, C2_R_Cr, C0_G_Y, C1_B_Cb),
	FMT_RGB_2101010(MDP_RGBX_1010102, MDSS_MDP_FETCH_LINEAR,
		VALID_ROT_WB_FORMAT | VALID_MDP_WB_INTF_FORMAT, 0,
		C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA),
	FMT_RGB_2101010(MDP_XRGB_2101010, MDSS_MDP_FETCH_LINEAR, 0, 0,
		C3_ALPHA, C2_R_Cr, C0_G_Y, C1_B_Cb),
	FMT_RGB_2101010(MDP_BGRA_1010102, MDSS_MDP_FETCH_LINEAR,
		VALID_ROT_WB_FORMAT | VALID_MDP_WB_INTF_FORMAT, 1,
		C1_B_Cb, C0_G_Y, C2_R_Cr, C3_ALPHA),
	FMT_RGB_2101010(MDP_ABGR_2101010, MDSS_MDP_FETCH_LINEAR, 0, 1,
		C3_ALPHA, C1_B_Cb, C0_G_Y, C2_R_Cr),
	FMT_RGB_2101010(MDP_BGRX_1010102, MDSS_MDP_FETCH_LINEAR,
		VALID_ROT_WB_FORMAT | VALID_MDP_WB_INTF_FORMAT, 0,
		C1_B_Cb, C0_G_Y, C2_R_Cr, C3_ALPHA),
	FMT_RGB_2101010(MDP_XBGR_2101010, MDSS_MDP_FETCH_LINEAR, 0, 0,
		C3_ALPHA, C1_B_Cb, C0_G_Y, C2_R_Cr),

	FMT_YUV_PSEUDO_10(MDP_Y_CBCR_H2V2_P010, MDSS_MDP_FETCH_LINEAR,
		MDSS_MDP_CHROMA_420, VALID_ROT_WB_FORMAT,
		C1_B_Cb, C2_R_Cr, 0, 1),

};
#endif
