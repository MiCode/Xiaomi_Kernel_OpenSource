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

#include <linux/kernel.h>
#include "sde_mdp_formats.h"

static struct sde_mdp_format_params sde_mdp_format_map[] = {
	INTERLEAVED_RGB_FMT(ARGB8888,
		COLOR_8BIT, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C1_B_Cb, C0_G_Y, C2_R_Cr, C3_ALPHA,
		true, 4, 0),

	INTERLEAVED_RGB_FMT(ABGR8888,
		COLOR_8BIT, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA,
		true, 4, 0),

	INTERLEAVED_RGB_FMT(RGBA8888,
		COLOR_8BIT, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C3_ALPHA, C1_B_Cb, C0_G_Y, C2_R_Cr,
		true, 4, 0),

	INTERLEAVED_RGB_FMT(BGRA8888,
		COLOR_8BIT, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C3_ALPHA, C2_R_Cr, C0_G_Y, C1_B_Cb,
		true, 4, 0),

	INTERLEAVED_RGB_FMT(XRGB8888,
		COLOR_8BIT, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C1_B_Cb, C0_G_Y, C2_R_Cr, C3_ALPHA,
		true, 4, 0),

	INTERLEAVED_RGB_FMT(RGB888,
		0, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C1_B_Cb, C0_G_Y, C2_R_Cr, 0,
		false, 3, 0),

	INTERLEAVED_RGB_FMT(BGR888,
		0, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C2_R_Cr, C0_G_Y, C1_B_Cb, 0,
		false, 3, 0),

	INTERLEAVED_RGB_FMT(RGB565,
		0, COLOR_5BIT, COLOR_6BIT, COLOR_5BIT,
		C1_B_Cb, C0_G_Y, C2_R_Cr, 0,
		false, 2, 0),

	INTERLEAVED_RGB_FMT(BGR565,
		0, 5, 6, 5,
		C2_R_Cr, C0_G_Y, C1_B_Cb, 0,
		false, 2, 0),

	PSEDUO_YUV_FMT(NV12,
		0, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C1_B_Cb, C2_R_Cr,
		SDE_MDP_CHROMA_420, 0),

	PSEDUO_YUV_FMT(NV21,
		0, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C2_R_Cr, C1_B_Cb,
		SDE_MDP_CHROMA_420, 0),

	PSEDUO_YUV_FMT(NV16,
		0, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C1_B_Cb, C2_R_Cr,
		SDE_MDP_CHROMA_H2V1, 0),

	PSEDUO_YUV_FMT(NV61,
		0, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C2_R_Cr, C1_B_Cb,
		SDE_MDP_CHROMA_H2V1, 0),

	INTERLEAVED_YUV_FMT(VYUY,
		0, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C2_R_Cr, C0_G_Y, C1_B_Cb, C0_G_Y,
		false, SDE_MDP_CHROMA_H2V1, 4, 2,
		0),

	INTERLEAVED_YUV_FMT(UYVY,
		0, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C1_B_Cb, C0_G_Y, C2_R_Cr, C0_G_Y,
		false, SDE_MDP_CHROMA_H2V1, 4, 2,
		0),

	INTERLEAVED_YUV_FMT(YUYV,
		0, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C0_G_Y, C1_B_Cb, C0_G_Y, C2_R_Cr,
		false, SDE_MDP_CHROMA_H2V1, 4, 2,
		0),

	INTERLEAVED_YUV_FMT(YVYU,
		0, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C0_G_Y, C2_R_Cr, C0_G_Y, C1_B_Cb,
		false, SDE_MDP_CHROMA_H2V1, 4, 2,
		0),

	PLANAR_YUV_FMT(YUV420,
		0, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C2_R_Cr, C1_B_Cb, C0_G_Y,
		false, SDE_MDP_CHROMA_420, 2,
		0),

	PLANAR_YUV_FMT(YVU420,
		0, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C1_B_Cb, C2_R_Cr, C0_G_Y,
		false, SDE_MDP_CHROMA_420, 2,
		0),
};

struct sde_mdp_format_params *sde_mdp_get_format_params(u32 format,
		u32 fmt_modifier)
{
	u32 i = 0;
	struct sde_mdp_format_params *fmt = NULL;

	for (i = 0; i < sizeof(sde_mdp_format_map)/sizeof(*sde_mdp_format_map);
			i++)
		if (format == sde_mdp_format_map[i].format) {
			fmt = &sde_mdp_format_map[i];
			break;
		}

	return fmt;
}

