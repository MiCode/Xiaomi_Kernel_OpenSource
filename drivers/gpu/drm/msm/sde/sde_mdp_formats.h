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

#ifndef _SDE_MDP_FORMATS_H
#define _SDE_MDP_FORMATS_H

#include <drm/drm_fourcc.h>
#include "sde_hw_mdss.h"

/**
 * MDP supported format packing, bpp, and other format
 * information.
 * MDP currently only supports interleaved RGB formats
 * UBWC support for a pixel format is indicated by the flag,
 * there is additional meta data plane for such formats
 */

#define INTERLEAVED_RGB_FMT(fmt, a, r, g, b, e0, e1, e2, e3, alpha, bp, flg) \
{                                                                         \
	.format = DRM_FORMAT_ ## fmt,                                     \
	.fetch_planes = SDE_MDP_PLANE_INTERLEAVED,                        \
	.alpha_enable = alpha,                                            \
	.element = { (e0), (e1), (e2), (e3) },                            \
	.bits = { a, r, g, b},                                            \
	.chroma_sample = SDE_MDP_CHROMA_RGB,                              \
	.unpack_align_msb = 0,                                            \
	.unpack_tight = 1,                                                \
	.unpack_count = (alpha == true) ? 4:3,                            \
	.bpp = bp,                                                        \
	.fetch_mode = SDE_MDP_FETCH_LINEAR,                               \
	.is_yuv = false,                                                  \
	.flag = flg                                                      \
}

#define INTERLEAVED_YUV_FMT(fmt, a, r, g, b, e0, e1, e2, e3,               \
alpha, chroma, count, bp, flg)                                          \
{                                                                         \
	.format = DRM_FORMAT_ ## fmt,                                     \
	.fetch_planes = SDE_MDP_PLANE_INTERLEAVED,                        \
	.alpha_enable = alpha,                                            \
	.element = { (e0), (e1), (e2), (e3)},                             \
	.bits = { a, r, g, b},                                            \
	.chroma_sample = chroma,                                          \
	.unpack_align_msb = 0,                                            \
	.unpack_tight = 1,                                                \
	.unpack_count = count,                                            \
	.bpp = bp,                                                       \
	.fetch_mode = SDE_MDP_FETCH_LINEAR,                               \
	.is_yuv = true,                                                   \
	.flag = flg                                                      \
}

#define PSEDUO_YUV_FMT(fmt, a, r, g, b, e0, e1, chroma, flg)             \
{                                                                         \
	.format = DRM_FORMAT_ ## fmt,                                     \
	.fetch_planes = SDE_MDP_PLANE_PSEUDO_PLANAR,                      \
	.alpha_enable = false,                                            \
	.element = { (e0), (e1), 0, 0 },                                  \
	.bits = { a, r, g, b},                                            \
	.chroma_sample = chroma,                                          \
	.unpack_align_msb = 0,                                            \
	.unpack_tight = 1,                                                \
	.unpack_count = 2,                                                \
	.bpp = 2,                                                         \
	.fetch_mode = SDE_MDP_FETCH_LINEAR,                               \
	.is_yuv = true,                                                   \
	.flag = flg                                                      \
}

#define PLANAR_YUV_FMT(fmt, a, r, g, b, e0, e1, e2, alpha, chroma, bp, flg)\
{                                                                            \
	.format = DRM_FORMAT_ ## fmt,                                        \
	.fetch_planes = SDE_MDP_PLANE_INTERLEAVED,                           \
	.alpha_enable = alpha,                                               \
	.element = { (e0), (e1), (e2), 0 },                               \
	.bits = { a, r, g, b},                                               \
	.chroma_sample = chroma,                                             \
	.unpack_align_msb = 0,                                               \
	.unpack_tight = 1,                                                   \
	.unpack_count = 0,                                                   \
	.bpp = bp,                                                          \
	.fetch_mode = SDE_MDP_FETCH_LINEAR,                                  \
	.is_yuv = true,                                                      \
	.flag = flg                                                         \
}

/**
 * sde_mdp_get_format_params(): Returns sde format structure pointer.
 * @format:  DRM format
 * @fmt_modifier: DRM format modifier
 */
struct sde_mdp_format_params *sde_mdp_get_format_params(u32 format,
		u32 fmt_modifier);

#endif /*_SDE_MDP_FORMATS_H */
