/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#define C3_ALPHA	3	/* alpha */
#define C2_R_Cr		2	/* R/Cr */
#define C1_B_Cb		1	/* B/Cb */
#define C0_G_Y		0	/* G/luma */

static struct mdss_mdp_format_params mdss_mdp_format_map[MDP_IMGTYPE_LIMIT] = {
	[MDP_RGB_565] = {
		.format = MDP_RGB_565,
		.fetch_planes = MDSS_MDP_PLANE_INTERLEAVED,
		.a_bit = 0,
		.r_bit = 1,	/* R, 5 bits */
		.b_bit = 1,	/* B, 5 bits */
		.g_bit = 2,	/* G, 6 bits */
		.alpha_enable = 0,
		.unpack_tight = 1,
		.unpack_align_msb = 0,
		.unpack_count = 2,
		.element2 = C2_R_Cr,
		.element1 = C0_G_Y,
		.element0 = C1_B_Cb,
		.bpp = 1,	/* 2 bpp */
	},
	[MDP_BGR_565] = {
		.format = MDP_BGR_565,
		.fetch_planes = MDSS_MDP_PLANE_INTERLEAVED,
		.a_bit = 0,
		.r_bit = 1,	/* R, 5 bits */
		.b_bit = 1,	/* B, 5 bits */
		.g_bit = 2,	/* G, 6 bits */
		.alpha_enable = 0,
		.unpack_tight = 1,
		.unpack_align_msb = 0,
		.unpack_count = 2,
		.element2 = C1_B_Cb,
		.element1 = C0_G_Y,
		.element0 = C2_R_Cr,
		.bpp = 1,	/* 2 bpp */
	},
	[MDP_RGB_888] = {
		.format = MDP_RGB_888,
		.fetch_planes = MDSS_MDP_PLANE_INTERLEAVED,
		.a_bit = 0,
		.r_bit = 3,	/* R, 8 bits */
		.b_bit = 3,	/* B, 8 bits */
		.g_bit = 3,	/* G, 8 bits */
		.alpha_enable = 0,
		.unpack_tight = 1,
		.unpack_align_msb = 0,
		.unpack_count = 2,
		.element2 = C1_B_Cb,
		.element1 = C0_G_Y,
		.element0 = C2_R_Cr,
		.bpp = 2,	/* 3 bpp */
	},
	[MDP_XRGB_8888] = {
		.format = MDP_XRGB_8888,
		.fetch_planes = MDSS_MDP_PLANE_INTERLEAVED,
		.a_bit = 3,	/* alpha, 4 bits */
		.r_bit = 3,	/* R, 8 bits */
		.b_bit = 3,	/* B, 8 bits */
		.g_bit = 3,	/* G, 8 bits */
		.alpha_enable = 0,
		.unpack_tight = 1,
		.unpack_align_msb = 0,
		.unpack_count = 3,
		.element3 = C1_B_Cb,
		.element2 = C0_G_Y,
		.element1 = C2_R_Cr,
		.element0 = C3_ALPHA,
		.bpp = 3,	/* 4 bpp */
	},
	[MDP_ARGB_8888] = {
		.format = MDP_ARGB_8888,
		.fetch_planes = MDSS_MDP_PLANE_INTERLEAVED,
		.a_bit = 3,	/* alpha, 4 bits */
		.r_bit = 3,	/* R, 8 bits */
		.b_bit = 3,	/* B, 8 bits */
		.g_bit = 3,	/* G, 8 bits */
		.alpha_enable = 1,
		.unpack_tight = 1,
		.unpack_align_msb = 0,
		.unpack_count = 3,
		.element3 = C1_B_Cb,
		.element2 = C0_G_Y,
		.element1 = C2_R_Cr,
		.element0 = C3_ALPHA,
		.bpp = 3,	/* 4 bpp */
	},
	[MDP_RGBA_8888] = {
		.format = MDP_RGBA_8888,
		.fetch_planes = MDSS_MDP_PLANE_INTERLEAVED,
		.a_bit = 3,	/* alpha, 4 bits */
		.r_bit = 3,	/* R, 8 bits */
		.b_bit = 3,	/* B, 8 bits */
		.g_bit = 3,	/* G, 8 bits */
		.alpha_enable = 1,
		.unpack_tight = 1,
		.unpack_align_msb = 0,
		.unpack_count = 3,
		.element3 = C3_ALPHA,
		.element2 = C1_B_Cb,
		.element1 = C0_G_Y,
		.element0 = C2_R_Cr,
		.bpp = 3,	/* 4 bpp */
	},
	[MDP_RGBX_8888] = {
		.format = MDP_RGBX_8888,
		.fetch_planes = MDSS_MDP_PLANE_INTERLEAVED,
		.a_bit = 3,
		.r_bit = 3,	/* R, 8 bits */
		.b_bit = 3,	/* B, 8 bits */
		.g_bit = 3,	/* G, 8 bits */
		.alpha_enable = 0,
		.unpack_tight = 1,
		.unpack_align_msb = 0,
		.unpack_count = 3,
		.element3 = C3_ALPHA,
		.element2 = C1_B_Cb,
		.element1 = C0_G_Y,
		.element0 = C2_R_Cr,
		.bpp = 3,	/* 4 bpp */
	},
	[MDP_BGRA_8888] = {
		.format = MDP_BGRA_8888,
		.fetch_planes = MDSS_MDP_PLANE_INTERLEAVED,
		.a_bit = 3,	/* alpha, 4 bits */
		.r_bit = 3,	/* R, 8 bits */
		.b_bit = 3,	/* B, 8 bits */
		.g_bit = 3,	/* G, 8 bits */
		.alpha_enable = 1,
		.unpack_tight = 1,
		.unpack_align_msb = 0,
		.unpack_count = 3,
		.element3 = C3_ALPHA,
		.element2 = C2_R_Cr,
		.element1 = C0_G_Y,
		.element0 = C1_B_Cb,
		.bpp = 3,	/* 4 bpp */
	},
	[MDP_YCRYCB_H2V1] = {
		.format = MDP_YCRYCB_H2V1,
		.is_yuv = 1,
		.a_bit = 0,
		.r_bit = 3,	/* R, 8 bits */
		.b_bit = 3,	/* B, 8 bits */
		.g_bit = 3,	/* G, 8 bits */
		.alpha_enable = 0,
		.unpack_tight = 1,
		.unpack_align_msb = 0,
		.unpack_count = 1,	/* 2 */
		.bpp = 1,	/* 2 bpp */
		.fetch_planes = MDSS_MDP_PLANE_INTERLEAVED,
		.chroma_sample = MDSS_MDP_CHROMA_H2V1,
		.unpack_count = 3,
		.element3 = C0_G_Y,
		.element2 = C2_R_Cr,
		.element1 = C0_G_Y,
		.element0 = C1_B_Cb,
	},
	[MDP_Y_CRCB_H2V1] = {
		.format = MDP_Y_CRCB_H2V1,
		.is_yuv = 1,
		.a_bit = 0,
		.r_bit = 3,	/* R, 8 bits */
		.b_bit = 3,	/* B, 8 bits */
		.g_bit = 3,	/* G, 8 bits */
		.alpha_enable = 0,
		.unpack_tight = 1,
		.unpack_align_msb = 0,
		.unpack_count = 1,	/* 2 */
		.bpp = 1,	/* 2 bpp */
		.fetch_planes = MDSS_MDP_PLANE_PSEUDO_PLANAR,
		.chroma_sample = MDSS_MDP_CHROMA_H2V1,
		.element1 = C1_B_Cb,
		.element0 = C2_R_Cr,
	},
	[MDP_Y_CBCR_H2V1] = {
		.format = MDP_Y_CBCR_H2V1,
		.is_yuv = 1,
		.a_bit = 0,
		.r_bit = 3,	/* R, 8 bits */
		.b_bit = 3,	/* B, 8 bits */
		.g_bit = 3,	/* G, 8 bits */
		.alpha_enable = 0,
		.unpack_tight = 1,
		.unpack_align_msb = 0,
		.unpack_count = 1,	/* 2 */
		.bpp = 1,	/* 2 bpp */
		.fetch_planes = MDSS_MDP_PLANE_PSEUDO_PLANAR,
		.chroma_sample = MDSS_MDP_CHROMA_H2V1,
		.element1 = C2_R_Cr,
		.element0 = C1_B_Cb,
	},
	[MDP_Y_CRCB_H1V2] = {
		.format = MDP_Y_CRCB_H1V2,
		.is_yuv = 1,
		.a_bit = 0,
		.r_bit = 3,	/* R, 8 bits */
		.b_bit = 3,	/* B, 8 bits */
		.g_bit = 3,	/* G, 8 bits */
		.alpha_enable = 0,
		.unpack_tight = 1,
		.unpack_align_msb = 0,
		.unpack_count = 1,	/* 2 */
		.bpp = 1,	/* 2 bpp */
		.fetch_planes = MDSS_MDP_PLANE_PSEUDO_PLANAR,
		.chroma_sample = MDSS_MDP_CHROMA_H1V2,
		.element1 = C1_B_Cb,
		.element0 = C2_R_Cr,
	},
	[MDP_Y_CBCR_H1V2] = {
		.format = MDP_Y_CBCR_H1V2,
		.is_yuv = 1,
		.a_bit = 0,
		.r_bit = 3,	/* R, 8 bits */
		.b_bit = 3,	/* B, 8 bits */
		.g_bit = 3,	/* G, 8 bits */
		.alpha_enable = 0,
		.unpack_tight = 1,
		.unpack_align_msb = 0,
		.unpack_count = 1,	/* 2 */
		.bpp = 1,	/* 2 bpp */
		.fetch_planes = MDSS_MDP_PLANE_PSEUDO_PLANAR,
		.chroma_sample = MDSS_MDP_CHROMA_H1V2,
		.element1 = C2_R_Cr,
		.element0 = C1_B_Cb,
	},
	[MDP_Y_CRCB_H2V2] = {
		.format = MDP_Y_CRCB_H2V2,
		.is_yuv = 1,
		.a_bit = 0,
		.r_bit = 3,	/* R, 8 bits */
		.b_bit = 3,	/* B, 8 bits */
		.g_bit = 3,	/* G, 8 bits */
		.alpha_enable = 0,
		.unpack_tight = 1,
		.unpack_align_msb = 0,
		.unpack_count = 1,	/* 2 */
		.bpp = 1,	/* 2 bpp */
		.fetch_planes = MDSS_MDP_PLANE_PSEUDO_PLANAR,
		.chroma_sample = MDSS_MDP_CHROMA_420,
		.element1 = C1_B_Cb,
		.element0 = C2_R_Cr,
	},
	[MDP_Y_CBCR_H2V2] = {
		.format = MDP_Y_CBCR_H2V2,
		.is_yuv = 1,
		.a_bit = 0,
		.r_bit = 3,	/* R, 8 bits */
		.b_bit = 3,	/* B, 8 bits */
		.g_bit = 3,	/* G, 8 bits */
		.alpha_enable = 0,
		.unpack_tight = 1,
		.unpack_align_msb = 0,
		.unpack_count = 1,	/* 2 */
		.bpp = 1,	/* 2 bpp */
		.fetch_planes = MDSS_MDP_PLANE_PSEUDO_PLANAR,
		.chroma_sample = MDSS_MDP_CHROMA_420,
		.element1 = C2_R_Cr,
		.element0 = C1_B_Cb,
	},
	[MDP_Y_CR_CB_H2V2] = {
		.format = MDP_Y_CR_CB_H2V2,
		.is_yuv = 1,
		.a_bit = 0,
		.r_bit = 3,	/* R, 8 bits */
		.b_bit = 3,	/* B, 8 bits */
		.g_bit = 3,	/* G, 8 bits */
		.alpha_enable = 0,
		.unpack_tight = 1,
		.unpack_align_msb = 0,
		.unpack_count = 1,	/* 2 */
		.bpp = 1,	/* 2 bpp */
		.fetch_planes = MDSS_MDP_PLANE_PLANAR,
		.chroma_sample = MDSS_MDP_CHROMA_420,
	},
	[MDP_Y_CB_CR_H2V2] = {
		.format = MDP_Y_CB_CR_H2V2,
		.is_yuv = 1,
		.a_bit = 0,
		.r_bit = 3,	/* R, 8 bits */
		.b_bit = 3,	/* B, 8 bits */
		.g_bit = 3,	/* G, 8 bits */
		.alpha_enable = 0,
		.unpack_tight = 1,
		.unpack_align_msb = 0,
		.unpack_count = 1,	/* 2 */
		.bpp = 1,	/* 2 bpp */
		.fetch_planes = MDSS_MDP_PLANE_PLANAR,
		.chroma_sample = MDSS_MDP_CHROMA_420,
	},
	[MDP_Y_CR_CB_GH2V2] = {
		.format = MDP_Y_CR_CB_GH2V2,
		.is_yuv = 1,
		.a_bit = 0,
		.r_bit = 3,	/* R, 8 bits */
		.b_bit = 3,	/* B, 8 bits */
		.g_bit = 3,	/* G, 8 bits */
		.alpha_enable = 0,
		.unpack_tight = 1,
		.unpack_align_msb = 0,
		.unpack_count = 1,	/* 2 */
		.bpp = 1,	/* 2 bpp */
		.fetch_planes = MDSS_MDP_PLANE_PLANAR,
		.chroma_sample = MDSS_MDP_CHROMA_420,
	},
};
#endif
