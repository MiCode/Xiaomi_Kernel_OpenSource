/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _SDE_FORMATS_H
#define _SDE_FORMATS_H

#include <drm/drm_fourcc.h>
#include "msm_gem.h"
#include "sde_hw_mdss.h"

/**
 * sde_get_sde_format_ext() - Returns sde format structure pointer.
 * @format:          DRM FourCC Code
 * @modifier:        format modifier from client
 */
const struct sde_format *sde_get_sde_format_ext(
		const uint32_t format,
		const uint64_t modifier);

#define sde_get_sde_format(f) sde_get_sde_format_ext(f, 0)

/**
 * sde_get_msm_format - get an sde_format by its msm_format base
 *                     callback function registers with the msm_kms layer
 * @kms:             kms driver
 * @format:          DRM FourCC Code
 * @modifier:        data layout modifier
 */
const struct msm_format *sde_get_msm_format(
		struct msm_kms *kms,
		const uint32_t format,
		const uint64_t modifier);

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
 * sde_format_get_plane_sizes - calculate size and layout of given buffer format
 * @fmt:             pointer to sde_format
 * @w:               width of the buffer
 * @h:               height of the buffer
 * @layout:          layout of the buffer
 * @pitches:         array of size [SDE_MAX_PLANES] to populate
 *		     pitch for each plane
 *
 * Return: size of the buffer
 */
int sde_format_get_plane_sizes(
		const struct sde_format *fmt,
		const uint32_t w,
		const uint32_t h,
		struct sde_hw_fmt_layout *layout,
		const uint32_t *pitches);

/**
 * sde_format_get_block_size - get block size of given format when
 *	operating in block mode
 * @fmt:             pointer to sde_format
 * @w:               pointer to width of the block
 * @h:               pointer to height of the block
 *
 * Return: 0 if success; error oode otherwise
 */
int sde_format_get_block_size(const struct sde_format *fmt,
		uint32_t *w, uint32_t *h);

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
 * @aspace:            address space pointer
 * @fb:                framebuffer pointer
 * @fmtl:              format layout structure to populate
 *
 * Return: error code on failure, -EAGAIN if success but the addresses
 *         are the same as before or 0 if new addresses were populated
 */
int sde_format_populate_layout(
		struct msm_gem_address_space *aspace,
		struct drm_framebuffer *fb,
		struct sde_hw_fmt_layout *fmtl);

/**
 * sde_format_populate_layout_with_roi - populate the given format layout
 *                     based on mmu, fb, roi, and format found in the fb
 * @aspace:            address space pointer
 * @fb:                framebuffer pointer
 * @roi:               region of interest (optional)
 * @fmtl:              format layout structure to populate
 *
 * Return: error code on failure, 0 on success
 */
int sde_format_populate_layout_with_roi(
		struct msm_gem_address_space *aspace,
		struct drm_framebuffer *fb,
		struct sde_rect *roi,
		struct sde_hw_fmt_layout *fmtl);

/**
 * sde_format_get_framebuffer_size - get framebuffer memory size
 * @format:            DRM pixel format
 * @width:             pixel width
 * @height:            pixel height
 * @pitches:           array of size [SDE_MAX_PLANES] to populate
 *		       pitch for each plane
 * @modifier:          drm modifier
 *
 * Return: memory size required for frame buffer
 */
uint32_t sde_format_get_framebuffer_size(
		const uint32_t format,
		const uint32_t width,
		const uint32_t height,
		const uint32_t *pitches,
		const uint64_t modifier);

/**
 * sde_format_is_tp10_ubwc - check if the format is tp10 ubwc
 * @format:            DRM pixel format
 *
 * Return: returns true if the format is tp10 ubwc, otherwise false.
 */
bool sde_format_is_tp10_ubwc(const struct sde_format *fmt);

/**
 * sde_format_validate_fmt - validates if the format "sde_fmt" is within
 *	the list "fmt_list"
 * @kms: pointer to the kms object
 * @sde_fmt: pointer to the format to look within the list
 * @fmt_list: list where driver will loop to look for the 'sde_fmt' format.
 * @result: returns 0 if the format is found, otherwise will return an
 *	error code.
 */
int sde_format_validate_fmt(struct msm_kms *kms,
	const struct sde_format *sde_fmt,
	const struct sde_format_extended *fmt_list);

#endif /*_SDE_FORMATS_H */
