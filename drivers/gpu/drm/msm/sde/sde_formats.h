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

/**
 * sde_get_sde_format_ext() - Returns sde format structure pointer.
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
 * sde_get_msm_format - get an sde_format by its msm_format base
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
 * sde_populate_formats - populate the given array with fourcc codes supported
 * @format_list:       pointer to list of possible formats
 * @pixel_formats:     array to populate with fourcc codes
 * @pixel_modifiers:   array to populate with drm modifiers, can be NULL
 * @pixel_formats_max: length of pixel formats array
 * Return: number of elements populated
 */
uint32_t sde_populate_formats(
		const struct sde_format_extended *format_list,
		uint32_t *pixel_formats,
		uint64_t *pixel_modifiers,
		uint32_t pixel_formats_max);

/**
 * sde_format_check_modified_format - validate format and buffers for
 *                   sde non-standard, i.e. modified format
 * @kms:             kms driver
 * @msm_fmt:         pointer to the msm_fmt base pointer of an sde_format
 * @cmd:             fb_cmd2 structure user request
 * @bos:             gem buffer object list
 *
 * Return: error code on failure, 0 on success
 */
int sde_format_check_modified_format(
		const struct msm_kms *kms,
		const struct msm_format *msm_fmt,
		const struct drm_mode_fb_cmd2 *cmd,
		struct drm_gem_object **bos);

/**
 * sde_format_populate_layout - populate the given format layout based on
 *                     mmu, fb, and format found in the fb
 * @mmu_id:            mmu id handle
 * @fb:                framebuffer pointer
 * @fmtl:              format layout structure to populate
 *
 * Return: error code on failure, -EAGAIN if success but the addresses
 *         are the same as before or 0 if new addresses were populated
 */
int sde_format_populate_layout(
		int mmu_id,
		struct drm_framebuffer *fb,
		struct sde_hw_fmt_layout *fmtl);

/**
 * sde_format_populate_layout_with_roi - populate the given format layout
 *                     based on mmu, fb, roi, and format found in the fb
 * @mmu_id:            mmu id handle
 * @fb:                framebuffer pointer
 * @roi:               region of interest (optional)
 * @fmtl:              format layout structure to populate
 *
 * Return: error code on failure, 0 on success
 */
int sde_format_populate_layout_with_roi(
		int mmu_id,
		struct drm_framebuffer *fb,
		struct sde_rect *roi,
		struct sde_hw_fmt_layout *fmtl);

#endif /*_SDE_FORMATS_H */
