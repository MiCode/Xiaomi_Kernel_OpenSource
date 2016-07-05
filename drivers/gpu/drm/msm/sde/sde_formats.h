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

#ifndef _SDE_FORMATS_H
#define _SDE_FORMATS_H

#include <drm/drm_fourcc.h>
#include "sde_hw_mdss.h"

#define SDE_FORMAT_FLAG_ROTATOR BIT(0)
#define SDE_FORMAT_FLAG_UBWC    BIT(1)

#define SDE_FORMAT_IS_YUV(X)        ((X)->is_yuv)
#define SDE_FORMAT_IS_ROTATOR(X)    ((X)->flag & SDE_FORMAT_FLAG_ROTATOR)
#define SDE_FORMAT_IS_UBWC(X)       ((X)->flag & SDE_FORMAT_FLAG_UBWC)

/**
 * SDE supported format packing, bpp, and other format
 * information.
 * SDE currently only supports interleaved RGB formats
 * UBWC support for a pixel format is indicated by the flag,
 * there is additional meta data plane for such formats
 */

#define INTERLEAVED_RGB_FMT(fmt, a, r, g, b, e0, e1, e2, e3, uc, alpha,   \
bp, flg)                                                                  \
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
	.fetch_mode = SDE_FETCH_LINEAR,                                   \
	.is_yuv = false,                                                  \
	.flag = flg                                                       \
}

#define INTERLEAVED_YUV_FMT(fmt, a, r, g, b, e0, e1, e2, e3,              \
alpha, chroma, count, bp, flg)                                            \
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
	.fetch_mode = SDE_FETCH_LINEAR,                                   \
	.is_yuv = true,                                                   \
	.flag = flg                                                       \
}

#define PSEDUO_YUV_FMT(fmt, a, r, g, b, e0, e1, chroma, flg)              \
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
	.fetch_mode = SDE_FETCH_LINEAR,                                   \
	.is_yuv = true,                                                   \
	.flag = flg                                                       \
}

#define PLANAR_YUV_FMT(fmt, a, r, g, b, e0, e1, e2, alpha, chroma, bp, flg)\
{                                                                         \
	.base.pixel_format = DRM_FORMAT_ ## fmt,                          \
	.fetch_planes = SDE_PLANE_INTERLEAVED,                            \
	.alpha_enable = alpha,                                            \
	.element = { (e0), (e1), (e2), 0 },                               \
	.bits = { g, b, r, a },                                           \
	.chroma_sample = chroma,                                          \
	.unpack_align_msb = 0,                                            \
	.unpack_tight = 1,                                                \
	.unpack_count = 0,                                                \
	.bpp = bp,                                                        \
	.fetch_mode = SDE_FETCH_LINEAR,                                   \
	.is_yuv = true,                                                   \
	.flag = flg                                                       \
}

/**
 * sde_get_sde_format_ext(): Returns sde format structure pointer.
 * @format:          DRM FourCC Code
 * @modifiers:       format modifier array from client, one per plane
 * @modifiers_len:   number of planes and array size for plane_modifiers
 */
const struct sde_format *sde_get_sde_format_ext(
		const uint32_t format,
		const uint64_t *modifiers,
		const uint32_t modifiers_len);

#define sde_get_sde_format(f) sde_get_sde_format_ext(f, NULL, 0)

/**
 * sde_get_msm_format: get an sde_format by its msm_format base
 *                     callback function registers with the msm_kms layer
 * @kms:             kms driver
 * @format:          DRM FourCC Code
 * @modifiers:       format modifier array from client, one per plane
 * @modifiers_len:   number of planes and array size for plane_modifiers
 */
const struct msm_format *sde_get_msm_format(
		struct msm_kms *kms,
		const uint32_t format,
		const uint64_t *modifiers,
		const uint32_t modifiers_len);

/**
 * sde_populate_formats: populate the given array with fourcc codes supported
 * @pixel_formats:   array to populate with fourcc codes
 * @max_formats:     length of pixel formats array
 * @rgb_only:        exclude any non-rgb formats from the list
 *
 * Return: number of elements populated
 */
uint32_t sde_populate_formats(
		uint32_t *pixel_formats,
		uint32_t max_formats,
		bool rgb_only);

#endif /*_SDE_FORMATS_H */
