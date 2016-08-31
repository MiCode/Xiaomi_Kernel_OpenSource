/*
 * drivers/video/tegra/dc/dc_config.c
 *
 * Copyright (c) 2010-2013, NVIDIA CORPORATION, All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __DRIVERS_VIDEO_TEGRA_DC_DC_CONFIG_H
#define __DRIVERS_VIDEO_TEGRA_DC_DC_CONFIG_H

#include <linux/errno.h>
#include <mach/dc.h>

#include "dc_priv.h"

#define ENTRY_SIZE	4	/* Size of feature entry args */

/* adjust large bit shift for an individual 32-bit word */
#define BIT_FOR_WORD(word, x) ( \
		(x) >= (word) * 32 && \
		(x) < 32 + (word) * 32 \
		? BIT((x) - 32) : 0)
#define HIGHBIT(x) BIT_FOR_WORD(1, x)
#define LOWBIT(x) BIT_FOR_WORD(0, x)

/* Define the supported formats. TEGRA_WIN_FMT_WIN_x macros are defined
 * based on T20/T30 formats. */
#define TEGRA_WIN_FMT_BASE	(BIT(TEGRA_WIN_FMT_P8) | \
				BIT(TEGRA_WIN_FMT_B4G4R4A4) | \
				BIT(TEGRA_WIN_FMT_B5G5R5A) | \
				BIT(TEGRA_WIN_FMT_B5G6R5) | \
				BIT(TEGRA_WIN_FMT_AB5G5R5) | \
				BIT(TEGRA_WIN_FMT_B8G8R8A8) | \
				BIT(TEGRA_WIN_FMT_R8G8B8A8) | \
				BIT(TEGRA_WIN_FMT_YCbCr422) | \
				BIT(TEGRA_WIN_FMT_YUV422) | \
				BIT(TEGRA_WIN_FMT_YCbCr420P) | \
				BIT(TEGRA_WIN_FMT_YUV420P) | \
				BIT(TEGRA_WIN_FMT_YCbCr422P) | \
				BIT(TEGRA_WIN_FMT_YUV422P) | \
				BIT(TEGRA_WIN_FMT_YCbCr422R) | \
				BIT(TEGRA_WIN_FMT_YUV422R))

#define TEGRA_WIN_FMT_T124_LOW TEGRA_WIN_FMT_BASE
#define TEGRA_WIN_FMT_T124_HIGH  (HIGHBIT(TEGRA_DC_EXT_FMT_YCbCr444P) | \
				HIGHBIT(TEGRA_DC_EXT_FMT_YUV444P) | \
				HIGHBIT(TEGRA_DC_EXT_FMT_YCrCb420SP) | \
				HIGHBIT(TEGRA_DC_EXT_FMT_YCbCr420SP) | \
				HIGHBIT(TEGRA_DC_EXT_FMT_YCrCb422SP) | \
				HIGHBIT(TEGRA_DC_EXT_FMT_YCbCr422SP) | \
				HIGHBIT(TEGRA_DC_EXT_FMT_YVU420SP) | \
				HIGHBIT(TEGRA_DC_EXT_FMT_YUV420SP) | \
				HIGHBIT(TEGRA_DC_EXT_FMT_YVU422SP) | \
				HIGHBIT(TEGRA_DC_EXT_FMT_YUV422SP) | \
				HIGHBIT(TEGRA_DC_EXT_FMT_YVU444SP) | \
				HIGHBIT(TEGRA_DC_EXT_FMT_YUV444SP))

#define TEGRA_WIN_FMT_WIN_A	(BIT(TEGRA_WIN_FMT_P1) | \
				BIT(TEGRA_WIN_FMT_P2) | \
				BIT(TEGRA_WIN_FMT_P4) | \
				BIT(TEGRA_WIN_FMT_P8) | \
				BIT(TEGRA_WIN_FMT_B4G4R4A4) | \
				BIT(TEGRA_WIN_FMT_B5G5R5A) | \
				BIT(TEGRA_WIN_FMT_B5G6R5) | \
				BIT(TEGRA_WIN_FMT_AB5G5R5) | \
				BIT(TEGRA_WIN_FMT_B8G8R8A8) | \
				BIT(TEGRA_WIN_FMT_R8G8B8A8) | \
				BIT(TEGRA_WIN_FMT_B6x2G6x2R6x2A8) | \
				BIT(TEGRA_WIN_FMT_R6x2G6x2B6x2A8))

#define TEGRA_WIN_FMT_WIN_B	(TEGRA_WIN_FMT_BASE | \
				BIT(TEGRA_WIN_FMT_B6x2G6x2R6x2A8) | \
				BIT(TEGRA_WIN_FMT_R6x2G6x2B6x2A8) | \
				BIT(TEGRA_WIN_FMT_YCbCr422RA) | \
				BIT(TEGRA_WIN_FMT_YUV422RA))

#define TEGRA_WIN_FMT_WIN_C	(TEGRA_WIN_FMT_BASE | \
				BIT(TEGRA_WIN_FMT_B6x2G6x2R6x2A8) | \
				BIT(TEGRA_WIN_FMT_R6x2G6x2B6x2A8) | \
				BIT(TEGRA_WIN_FMT_YCbCr422RA) | \
				BIT(TEGRA_WIN_FMT_YUV422RA))

/* preferred formats do not include 32-bpp formats */
#define TEGRA_WIN_PREF_FMT_WIN_B	(TEGRA_WIN_FMT_WIN_B & \
				~BIT(TEGRA_WIN_FMT_B8G8R8A8) & \
				~BIT(TEGRA_WIN_FMT_R8G8B8A8))

#define TEGRA_WIN_FMT_SIMPLE	(BIT(TEGRA_WIN_FMT_B4G4R4A4) | \
				BIT(TEGRA_WIN_FMT_B5G5R5A) | \
				BIT(TEGRA_WIN_FMT_B5G6R5) | \
				BIT(TEGRA_WIN_FMT_B8G8R8A8) | \
				BIT(TEGRA_WIN_FMT_R8G8B8A8))


/* For each entry, we define the offset to read specific feature. Define the
 * offset for TEGRA_DC_FEATURE_MAXIMUM_SCALE */
#define H_SCALE_UP	0
#define V_SCALE_UP	1
#define H_FILTER_DOWN	2
#define V_FILTER_DOWN	3

/* Define the offset for TEGRA_DC_FEATURE_MAXIMUM_SIZE */
#define MAX_WIDTH	0
#define MIN_WIDTH	1
#define MAX_HEIGHT	2
#define MIN_HEIGHT	3
#define CHECK_SIZE(val, min, max)	( \
		((val) < (min) || (val) > (max)) ? -EINVAL : 0)

/* Define the offset for TEGRA_DC_FEATURE_FILTER_TYPE */
#define V_FILTER	0
#define H_FILTER	1

/* Define the offset for TEGRA_DC_FEATURE_INVERT_TYPE */
#define H_INVERT	0
#define V_INVERT	1
#define SCAN_COLUMN	2

/* Define the offset for TEGRA_DC_FEATURE_LAYOUT_TYPE. */
#define PITCHED_LAYOUT	0
#define TILED_LAYOUT	1
#define BLOCK_LINEAR	2

/* Define the offset for TEGRA_DC_FEATURE_BLEND_TYPE. */
#define BLEND_GENERATION	0

#define INTERLACE		0

/* Available operations on feature table. */
enum {
	HAS_SCALE,
	HAS_TILED,
	HAS_V_FILTER,
	HAS_H_FILTER,
	HAS_GEN2_BLEND,
	GET_WIN_FORMATS,
	GET_WIN_SIZE,
	HAS_BLOCKLINEAR,
	HAS_INTERLACE,
};

enum tegra_dc_feature_option {
	TEGRA_DC_FEATURE_FORMATS,
	TEGRA_DC_FEATURE_BLEND_TYPE,
	TEGRA_DC_FEATURE_MAXIMUM_SIZE,
	TEGRA_DC_FEATURE_MAXIMUM_SCALE,
	TEGRA_DC_FEATURE_FILTER_TYPE,
	TEGRA_DC_FEATURE_LAYOUT_TYPE,
	TEGRA_DC_FEATURE_INVERT_TYPE,
	TEGRA_DC_FEATURE_PREFERRED_FORMATS,
	TEGRA_DC_FEATURE_FIELD_TYPE,
};

struct tegra_dc_feature_entry {
	u32 window_index;
	u32 option;
	long arg[ENTRY_SIZE];
};

struct tegra_dc_feature {
	u32 num_entries;
	struct tegra_dc_feature_entry *entries;
};

int tegra_dc_feature_has_scaling(struct tegra_dc *dc, int win_idx);
int tegra_dc_feature_has_tiling(struct tegra_dc *dc, int win_idx);
int tegra_dc_feature_has_blocklinear(struct tegra_dc *dc, int win_idx);
int tegra_dc_feature_has_interlace(struct tegra_dc *dc, int win_idx);
int tegra_dc_feature_has_filter(struct tegra_dc *dc, int win_idx, int operation);
int tegra_dc_feature_is_gen2_blender(struct tegra_dc *dc, int win_idx);

long *tegra_dc_parse_feature(struct tegra_dc *dc, int win_idx, int operation);
void tegra_dc_feature_register(struct tegra_dc *dc);

static inline bool win_use_v_filter(struct tegra_dc *dc,
	const struct tegra_dc_win *win)
{
	return tegra_dc_feature_has_filter(dc, win->idx, HAS_V_FILTER) &&
		(win->flags & TEGRA_WIN_FLAG_SCAN_COLUMN ?
			win->w.full != dfixed_const(win->out_h)
			: win->h.full != dfixed_const(win->out_h));
}

static inline bool win_use_h_filter(struct tegra_dc *dc,
	const struct tegra_dc_win *win)
{
	return tegra_dc_feature_has_filter(dc, win->idx, HAS_H_FILTER) &&
		(win->flags & TEGRA_WIN_FLAG_SCAN_COLUMN ?
			win->h.full != dfixed_const(win->out_w)
			: win->w.full != dfixed_const(win->out_w));
}
#endif
