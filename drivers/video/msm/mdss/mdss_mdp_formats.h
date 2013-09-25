/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#ifndef MDSS_MDP_FORMATS_H
#define MDSS_MDP_FORMATS_H

#include <linux/msm_mdp.h>

#include "mdss_mdp.h"

	/*
	 * number of bits for source component,
	 * 0 = 1 bit, 1 = 2 bits, 2 = 6 bits, 3 = 8 bits
	 */
enum {
	COLOR_4BIT,
	COLOR_5BIT,
	COLOR_6BIT,
	COLOR_8BIT,
};

#define FMT_RGB_565(fmt, e0, e1, e2)				\
	{							\
		.format = (fmt),				\
		.fetch_planes = MDSS_MDP_PLANE_INTERLEAVED,	\
		.unpack_tight = 1,				\
		.unpack_align_msb = 0,				\
		.alpha_enable = 0,				\
		.unpack_count = 3,				\
		.bpp = 2,					\
		.element = { (e0), (e1), (e2) },		\
		.bits = {					\
			[C2_R_Cr] = COLOR_5BIT,			\
			[C0_G_Y] = COLOR_6BIT,			\
			[C1_B_Cb] = COLOR_5BIT,			\
		},						\
	}

#define FMT_RGB_888(fmt, e0, e1, e2)				\
	{							\
		.format = (fmt),				\
		.fetch_planes = MDSS_MDP_PLANE_INTERLEAVED,	\
		.unpack_tight = 1,				\
		.unpack_align_msb = 0,				\
		.alpha_enable = 0,				\
		.unpack_count = 3,				\
		.bpp = 3,					\
		.element = { (e0), (e1), (e2) },		\
		.bits = {					\
			[C2_R_Cr] = COLOR_8BIT,			\
			[C0_G_Y] = COLOR_8BIT,			\
			[C1_B_Cb] = COLOR_8BIT,			\
		},						\
	}

#define FMT_RGB_8888(fmt, alpha_en, e0, e1, e2, e3)		\
	{							\
		.format = (fmt),				\
		.fetch_planes = MDSS_MDP_PLANE_INTERLEAVED,	\
		.unpack_tight = 1,				\
		.unpack_align_msb = 0,				\
		.alpha_enable = (alpha_en),			\
		.unpack_count = 4,				\
		.bpp = 4,					\
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

#define FMT_YUV_PSEUDO(fmt, samp, e0, e1)			\
	{							\
		FMT_YUV_COMMON(fmt),				\
		.fetch_planes = MDSS_MDP_PLANE_PSEUDO_PLANAR,	\
		.chroma_sample = samp,				\
		.unpack_count = 2,				\
		.bpp = 2,					\
		.element = { (e0), (e1) },			\
	}

#define FMT_YUV_PLANR(fmt, samp, e0, e1)			\
	{							\
		FMT_YUV_COMMON(fmt),				\
		.fetch_planes = MDSS_MDP_PLANE_PLANAR,		\
		.chroma_sample = samp,				\
		.bpp = 1,					\
		.unpack_count = 1,				\
		.element = { (e0), (e1) }			\
	}

static struct mdss_mdp_format_params mdss_mdp_format_map[] = {
	FMT_RGB_565(MDP_RGB_565, C1_B_Cb, C0_G_Y, C2_R_Cr),
	FMT_RGB_565(MDP_BGR_565, C2_R_Cr, C0_G_Y, C1_B_Cb),
	FMT_RGB_888(MDP_RGB_888, C2_R_Cr, C0_G_Y, C1_B_Cb),
	FMT_RGB_888(MDP_BGR_888, C1_B_Cb, C0_G_Y, C2_R_Cr),

	FMT_RGB_8888(MDP_XRGB_8888, 0, C3_ALPHA, C2_R_Cr, C0_G_Y, C1_B_Cb),
	FMT_RGB_8888(MDP_ARGB_8888, 1, C3_ALPHA, C2_R_Cr, C0_G_Y, C1_B_Cb),
	FMT_RGB_8888(MDP_RGBA_8888, 1, C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA),
	FMT_RGB_8888(MDP_RGBX_8888, 0, C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA),
	FMT_RGB_8888(MDP_BGRA_8888, 1, C1_B_Cb, C0_G_Y, C2_R_Cr, C3_ALPHA),
	FMT_RGB_8888(MDP_BGRX_8888, 0, C1_B_Cb, C0_G_Y, C2_R_Cr, C3_ALPHA),

	FMT_YUV_PSEUDO(MDP_Y_CRCB_H1V1, MDSS_MDP_CHROMA_RGB, C2_R_Cr, C1_B_Cb),
	FMT_YUV_PSEUDO(MDP_Y_CBCR_H1V1, MDSS_MDP_CHROMA_RGB, C1_B_Cb, C2_R_Cr),
	FMT_YUV_PSEUDO(MDP_Y_CRCB_H2V1, MDSS_MDP_CHROMA_H2V1, C2_R_Cr, C1_B_Cb),
	FMT_YUV_PSEUDO(MDP_Y_CBCR_H2V1, MDSS_MDP_CHROMA_H2V1, C1_B_Cb, C2_R_Cr),
	FMT_YUV_PSEUDO(MDP_Y_CRCB_H1V2, MDSS_MDP_CHROMA_H1V2, C2_R_Cr, C1_B_Cb),
	FMT_YUV_PSEUDO(MDP_Y_CBCR_H1V2, MDSS_MDP_CHROMA_H1V2, C1_B_Cb, C2_R_Cr),
	FMT_YUV_PSEUDO(MDP_Y_CRCB_H2V2, MDSS_MDP_CHROMA_420, C2_R_Cr, C1_B_Cb),
	FMT_YUV_PSEUDO(MDP_Y_CBCR_H2V2, MDSS_MDP_CHROMA_420, C1_B_Cb, C2_R_Cr),
	FMT_YUV_PSEUDO(MDP_Y_CBCR_H2V2_VENUS, MDSS_MDP_CHROMA_420,
		       C1_B_Cb, C2_R_Cr),

	FMT_YUV_PLANR(MDP_Y_CB_CR_H2V2, MDSS_MDP_CHROMA_420, C2_R_Cr, C1_B_Cb),
	FMT_YUV_PLANR(MDP_Y_CR_CB_H2V2, MDSS_MDP_CHROMA_420, C1_B_Cb, C2_R_Cr),
	FMT_YUV_PLANR(MDP_Y_CR_CB_GH2V2, MDSS_MDP_CHROMA_420, C1_B_Cb, C2_R_Cr),

	{
		FMT_YUV_COMMON(MDP_YCBCR_H1V1),
		.fetch_planes = MDSS_MDP_PLANE_INTERLEAVED,
		.chroma_sample = MDSS_MDP_CHROMA_RGB,
		.unpack_count = 3,
		.bpp = 3,
		.element = { C2_R_Cr, C1_B_Cb, C0_G_Y },
	},
	{
		FMT_YUV_COMMON(MDP_YCRCB_H1V1),
		.fetch_planes = MDSS_MDP_PLANE_INTERLEAVED,
		.chroma_sample = MDSS_MDP_CHROMA_RGB,
		.unpack_count = 3,
		.bpp = 3,
		.element = { C1_B_Cb, C2_R_Cr, C0_G_Y },
	},
	{
		FMT_YUV_COMMON(MDP_YCRYCB_H2V1),
		.fetch_planes = MDSS_MDP_PLANE_INTERLEAVED,
		.chroma_sample = MDSS_MDP_CHROMA_H2V1,
		.unpack_count = 4,
		.bpp = 2,
		.element = { C1_B_Cb, C0_G_Y, C2_R_Cr, C0_G_Y },
	},
	{
		FMT_YUV_COMMON(MDP_YCBYCR_H2V1),
		.fetch_planes = MDSS_MDP_PLANE_INTERLEAVED,
		.chroma_sample = MDSS_MDP_CHROMA_H2V1,
		.unpack_count = 4,
		.bpp = 2,
		.element = { C2_R_Cr, C0_G_Y, C1_B_Cb, C0_G_Y },
	},

};
#endif
