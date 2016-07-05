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

#include "sde_kms.h"
#include "sde_formats.h"

static struct sde_format sde_format_map[] = {
	INTERLEAVED_RGB_FMT(ARGB8888,
		COLOR_8BIT, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C1_B_Cb, C0_G_Y, C2_R_Cr, C3_ALPHA, 4,
		true, 4, SDE_FORMAT_FLAG_ROTATOR),

	INTERLEAVED_RGB_FMT(ABGR8888,
		COLOR_8BIT, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C2_R_Cr, C0_G_Y, C1_B_Cb, C3_ALPHA, 4,
		true, 4, SDE_FORMAT_FLAG_ROTATOR),

	INTERLEAVED_RGB_FMT(RGBA8888,
		COLOR_8BIT, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C3_ALPHA, C1_B_Cb, C0_G_Y, C2_R_Cr, 4,
		true, 4, SDE_FORMAT_FLAG_ROTATOR),

	INTERLEAVED_RGB_FMT(BGRA8888,
		COLOR_8BIT, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C3_ALPHA, C2_R_Cr, C0_G_Y, C1_B_Cb, 4,
		true, 4, SDE_FORMAT_FLAG_ROTATOR),

	INTERLEAVED_RGB_FMT(XRGB8888,
		COLOR_8BIT, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C1_B_Cb, C0_G_Y, C2_R_Cr, C3_ALPHA, 4,
		false, 4, SDE_FORMAT_FLAG_ROTATOR),

	INTERLEAVED_RGB_FMT(RGB888,
		0, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C1_B_Cb, C0_G_Y, C2_R_Cr, 0, 3,
		false, 3, SDE_FORMAT_FLAG_ROTATOR),

	INTERLEAVED_RGB_FMT(BGR888,
		0, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C2_R_Cr, C0_G_Y, C1_B_Cb, 0, 3,
		false, 3, SDE_FORMAT_FLAG_ROTATOR),

	INTERLEAVED_RGB_FMT(RGB565,
		0, COLOR_5BIT, COLOR_6BIT, COLOR_5BIT,
		C1_B_Cb, C0_G_Y, C2_R_Cr, 0, 3,
		false, 2, SDE_FORMAT_FLAG_ROTATOR),

	INTERLEAVED_RGB_FMT(BGR565,
		0, COLOR_5BIT, COLOR_6BIT, COLOR_5BIT,
		C2_R_Cr, C0_G_Y, C1_B_Cb, 0, 3,
		false, 2, SDE_FORMAT_FLAG_ROTATOR),

	PSEDUO_YUV_FMT(NV12,
		0, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C1_B_Cb, C2_R_Cr,
		SDE_CHROMA_420, SDE_FORMAT_FLAG_ROTATOR),

	PSEDUO_YUV_FMT(NV21,
		0, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C2_R_Cr, C1_B_Cb,
		SDE_CHROMA_420, SDE_FORMAT_FLAG_ROTATOR),

	PSEDUO_YUV_FMT(NV16,
		0, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C1_B_Cb, C2_R_Cr,
		SDE_CHROMA_H2V1, SDE_FORMAT_FLAG_ROTATOR),

	PSEDUO_YUV_FMT(NV61,
		0, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C2_R_Cr, C1_B_Cb,
		SDE_CHROMA_H2V1, SDE_FORMAT_FLAG_ROTATOR),

	INTERLEAVED_YUV_FMT(VYUY,
		0, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C2_R_Cr, C0_G_Y, C1_B_Cb, C0_G_Y,
		false, SDE_CHROMA_H2V1, 4, 2,
		0),

	INTERLEAVED_YUV_FMT(UYVY,
		0, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C1_B_Cb, C0_G_Y, C2_R_Cr, C0_G_Y,
		false, SDE_CHROMA_H2V1, 4, 2,
		0),

	INTERLEAVED_YUV_FMT(YUYV,
		0, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C0_G_Y, C1_B_Cb, C0_G_Y, C2_R_Cr,
		false, SDE_CHROMA_H2V1, 4, 2,
		0),

	INTERLEAVED_YUV_FMT(YVYU,
		0, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C0_G_Y, C2_R_Cr, C0_G_Y, C1_B_Cb,
		false, SDE_CHROMA_H2V1, 4, 2,
		0),

	PLANAR_YUV_FMT(YUV420,
		0, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C2_R_Cr, C1_B_Cb, C0_G_Y,
		false, SDE_CHROMA_420, 2,
		SDE_FORMAT_FLAG_ROTATOR),

	PLANAR_YUV_FMT(YVU420,
		0, COLOR_8BIT, COLOR_8BIT, COLOR_8BIT,
		C1_B_Cb, C2_R_Cr, C0_G_Y,
		false, SDE_CHROMA_420, 2,
		SDE_FORMAT_FLAG_ROTATOR),
};

const struct sde_format *sde_get_sde_format_ext(
		const uint32_t format,
		const uint64_t *modifiers,
		const uint32_t modifiers_len)
{
	u32 i = 0;
	uint64_t modifier = 0;
	struct sde_format *fmt = NULL;

	/*
	 * Currently only support exactly zero or one modifier, and it must be
	 * identical for all planes
	 */
	if ((modifiers_len && !modifiers) || (!modifiers_len && modifiers)) {
		DRM_ERROR("unexpected modifiers array or len\n");
		return NULL;
	} else if (modifiers && modifiers_len) {
		modifier = modifiers[0];
		for (i = 0; i < modifiers_len; i++) {
			if (modifiers[i])
				DBG("plane %d format modifier 0x%llX", i,
					modifiers[i]);

			if (modifiers[i] != modifier) {
				DRM_ERROR("bad fmt mod 0x%llX on plane %d\n",
					modifiers[i], i);
				return NULL;
			}
		}
	}

	for (i = 0; i < ARRAY_SIZE(sde_format_map); i++)
		if (format == sde_format_map[i].base.pixel_format) {
			fmt = &sde_format_map[i];
			break;
		}

	if (fmt == NULL)
		DRM_ERROR("unsupported fmt 0x%X modifier 0x%llX\n",
				format, modifier);
	else
		DBG("found fmt 0x%X modifier 0x%llX", format, modifier);

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

uint32_t sde_populate_formats(uint32_t *pixel_formats,
		uint32_t pixel_formats_max, bool rgb_only)
{
	uint32_t i;

	for (i = 0; i < ARRAY_SIZE(sde_format_map); i++) {
		const struct sde_format *fmt = &sde_format_map[i];

		if (i == pixel_formats_max)
			break;

		if (rgb_only && SDE_FORMAT_IS_YUV(fmt))
			continue;

		pixel_formats[i] = fmt->base.pixel_format;
	}

	return i;
}
