/*
 * drivers/video/tegra/dc/dc_config.c
 *
 * Copyright (c) 2012, NVIDIA CORPORATION, All rights reserved.
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

#include "dc_config.h"

static struct tegra_dc_feature_entry t20_feature_entries_a[] = {
	{ 0, TEGRA_DC_FEATURE_FORMATS, {TEGRA_WIN_FMT_WIN_A,} },
	{ 0, TEGRA_DC_FEATURE_BLEND_TYPE, {1,} },
	{ 0, TEGRA_DC_FEATURE_MAXIMUM_SIZE, {4095, 1, 4095, 1,} },
	{ 0, TEGRA_DC_FEATURE_MAXIMUM_SCALE, {2, 2, 2, 2,} },
	{ 0, TEGRA_DC_FEATURE_FILTER_TYPE, {0, 0,} },
	{ 0, TEGRA_DC_FEATURE_LAYOUT_TYPE, {1, 1,} },
	{ 0, TEGRA_DC_FEATURE_INVERT_TYPE, {1, 1, 0,} },

	{ 1, TEGRA_DC_FEATURE_FORMATS, {TEGRA_WIN_FMT_WIN_B,} },
	{ 1, TEGRA_DC_FEATURE_PREFERRED_FORMATS, {TEGRA_WIN_PREF_FMT_WIN_B,} },
	{ 1, TEGRA_DC_FEATURE_BLEND_TYPE, {1,} },
	{ 1, TEGRA_DC_FEATURE_MAXIMUM_SIZE, {4095, 1, 4095, 1,} },
	{ 1, TEGRA_DC_FEATURE_MAXIMUM_SCALE, {2, 2, 2, 2,} },
	{ 1, TEGRA_DC_FEATURE_FILTER_TYPE, {1, 1,} },
	{ 1, TEGRA_DC_FEATURE_LAYOUT_TYPE, {1, 1,} },
	{ 1, TEGRA_DC_FEATURE_INVERT_TYPE, {1, 1, 0,} },

	{ 2, TEGRA_DC_FEATURE_FORMATS, {TEGRA_WIN_FMT_WIN_C,} },
	{ 2, TEGRA_DC_FEATURE_BLEND_TYPE, {1,} },
	{ 2, TEGRA_DC_FEATURE_MAXIMUM_SIZE, {4095, 1, 4095, 1,} },
	{ 2, TEGRA_DC_FEATURE_MAXIMUM_SCALE, {2, 2, 2, 2,} },
	{ 2, TEGRA_DC_FEATURE_FILTER_TYPE, {0, 1,} },
	{ 2, TEGRA_DC_FEATURE_LAYOUT_TYPE, {1, 1,} },
	{ 2, TEGRA_DC_FEATURE_INVERT_TYPE, {1, 1, 0,} },
};

static struct tegra_dc_feature_entry t20_feature_entries_b[] = {
	{ 0, TEGRA_DC_FEATURE_FORMATS, {TEGRA_WIN_FMT_WIN_A,} },
	{ 0, TEGRA_DC_FEATURE_BLEND_TYPE, {1,} },
	{ 0, TEGRA_DC_FEATURE_MAXIMUM_SIZE, {4095, 1, 4095, 1,} },
	{ 0, TEGRA_DC_FEATURE_MAXIMUM_SCALE, {2, 2, 2, 2,} },
	{ 0, TEGRA_DC_FEATURE_FILTER_TYPE, {0, 0,} },
	{ 0, TEGRA_DC_FEATURE_LAYOUT_TYPE, {1, 1,} },
	{ 0, TEGRA_DC_FEATURE_INVERT_TYPE, {1, 1, 0,} },

	{ 1, TEGRA_DC_FEATURE_FORMATS, {TEGRA_WIN_FMT_WIN_B,} },
	{ 1, TEGRA_DC_FEATURE_PREFERRED_FORMATS, {TEGRA_WIN_PREF_FMT_WIN_B,} },
	{ 1, TEGRA_DC_FEATURE_BLEND_TYPE, {1,} },
	{ 1, TEGRA_DC_FEATURE_MAXIMUM_SIZE, {4095, 1, 4095, 1,} },
	{ 1, TEGRA_DC_FEATURE_MAXIMUM_SCALE, {2, 2, 2, 2,} },
	{ 1, TEGRA_DC_FEATURE_FILTER_TYPE, {1, 1,} },
	{ 1, TEGRA_DC_FEATURE_LAYOUT_TYPE, {1, 1,} },
	{ 1, TEGRA_DC_FEATURE_INVERT_TYPE, {1, 1, 0,} },

	{ 2, TEGRA_DC_FEATURE_FORMATS, {TEGRA_WIN_FMT_WIN_C,} },
	{ 2, TEGRA_DC_FEATURE_BLEND_TYPE, {1,} },
	{ 2, TEGRA_DC_FEATURE_MAXIMUM_SIZE, {4095, 1, 4095, 1,} },
	{ 2, TEGRA_DC_FEATURE_MAXIMUM_SCALE, {2, 2, 2, 2,} },
	{ 2, TEGRA_DC_FEATURE_FILTER_TYPE, {0, 1,} },
	{ 2, TEGRA_DC_FEATURE_LAYOUT_TYPE, {1, 1,} },
	{ 2, TEGRA_DC_FEATURE_INVERT_TYPE, {1, 1, 0,} },
};

struct tegra_dc_feature t20_feature_table_a = {
	ARRAY_SIZE(t20_feature_entries_a), t20_feature_entries_a,
};

struct tegra_dc_feature t20_feature_table_b = {
	ARRAY_SIZE(t20_feature_entries_b), t20_feature_entries_b,
};

static struct tegra_dc_feature_entry t30_feature_entries_a[] = {
	{ 0, TEGRA_DC_FEATURE_FORMATS, {TEGRA_WIN_FMT_WIN_A,} },
	{ 0, TEGRA_DC_FEATURE_BLEND_TYPE, {1,} },
	{ 0, TEGRA_DC_FEATURE_MAXIMUM_SIZE, {4095, 1, 4095, 1,} },
	{ 0, TEGRA_DC_FEATURE_MAXIMUM_SCALE, {2, 2, 2, 2,} },
	{ 0, TEGRA_DC_FEATURE_FILTER_TYPE, {0, 0,} },
	{ 0, TEGRA_DC_FEATURE_LAYOUT_TYPE, {1, 1} },
	{ 0, TEGRA_DC_FEATURE_INVERT_TYPE, {1, 1, 0,} },

	{ 1, TEGRA_DC_FEATURE_FORMATS, {TEGRA_WIN_FMT_WIN_B,} },
	{ 1, TEGRA_DC_FEATURE_PREFERRED_FORMATS, {TEGRA_WIN_PREF_FMT_WIN_B,} },
	{ 1, TEGRA_DC_FEATURE_BLEND_TYPE, {1,} },
	{ 1, TEGRA_DC_FEATURE_MAXIMUM_SIZE, {4095, 1, 4095, 1,} },
	{ 1, TEGRA_DC_FEATURE_MAXIMUM_SCALE, {2, 2, 2, 2,} },
	{ 1, TEGRA_DC_FEATURE_FILTER_TYPE, {1, 1,} },
	{ 1, TEGRA_DC_FEATURE_LAYOUT_TYPE, {1, 1} },
	{ 1, TEGRA_DC_FEATURE_INVERT_TYPE, {1, 1, 0,} },

	{ 2, TEGRA_DC_FEATURE_FORMATS, {TEGRA_WIN_FMT_WIN_C,} },
	{ 2, TEGRA_DC_FEATURE_BLEND_TYPE, {1,} },
	{ 2, TEGRA_DC_FEATURE_MAXIMUM_SIZE, {4095, 1, 4095, 1,} },
	{ 2, TEGRA_DC_FEATURE_MAXIMUM_SCALE, {2, 2, 2, 2,} },
	{ 2, TEGRA_DC_FEATURE_FILTER_TYPE, {0, 1,} },
	{ 2, TEGRA_DC_FEATURE_LAYOUT_TYPE, {1, 1} },
	{ 2, TEGRA_DC_FEATURE_INVERT_TYPE, {1, 1, 0,} },
};

static struct tegra_dc_feature_entry t30_feature_entries_b[] = {
	{ 0, TEGRA_DC_FEATURE_FORMATS, {TEGRA_WIN_FMT_WIN_A,} },
	{ 0, TEGRA_DC_FEATURE_BLEND_TYPE, {1,} },
	{ 0, TEGRA_DC_FEATURE_MAXIMUM_SIZE, {4095, 1, 4095, 1,} },
	{ 0, TEGRA_DC_FEATURE_MAXIMUM_SCALE, {2, 2, 2, 2,} },
	{ 0, TEGRA_DC_FEATURE_FILTER_TYPE, {0, 0,} },
	{ 0, TEGRA_DC_FEATURE_LAYOUT_TYPE, {1, 1,} },
	{ 0, TEGRA_DC_FEATURE_INVERT_TYPE, {1, 1, 0,} },

	{ 1, TEGRA_DC_FEATURE_FORMATS, {TEGRA_WIN_FMT_WIN_B,} },
	{ 1, TEGRA_DC_FEATURE_PREFERRED_FORMATS, {TEGRA_WIN_PREF_FMT_WIN_B,} },
	{ 1, TEGRA_DC_FEATURE_BLEND_TYPE, {1,} },
	{ 1, TEGRA_DC_FEATURE_MAXIMUM_SIZE, {4095, 1, 4095, 1,} },
	{ 1, TEGRA_DC_FEATURE_MAXIMUM_SCALE, {2, 2, 2, 2,} },
	{ 1, TEGRA_DC_FEATURE_FILTER_TYPE, {1, 1,} },
	{ 1, TEGRA_DC_FEATURE_LAYOUT_TYPE, {1, 1,} },
	{ 1, TEGRA_DC_FEATURE_INVERT_TYPE, {1, 1, 0,} },

	{ 2, TEGRA_DC_FEATURE_FORMATS, {TEGRA_WIN_FMT_WIN_C,} },
	{ 2, TEGRA_DC_FEATURE_BLEND_TYPE, {1,} },
	{ 2, TEGRA_DC_FEATURE_MAXIMUM_SIZE, {4095, 1, 4095, 1,} },
	{ 2, TEGRA_DC_FEATURE_MAXIMUM_SCALE, {2, 2, 2, 2,} },
	{ 2, TEGRA_DC_FEATURE_FILTER_TYPE, {0, 1,} },
	{ 2, TEGRA_DC_FEATURE_LAYOUT_TYPE, {1, 1,} },
	{ 2, TEGRA_DC_FEATURE_INVERT_TYPE, {1, 1, 0,} },
};

struct tegra_dc_feature t30_feature_table_a = {
	ARRAY_SIZE(t30_feature_entries_a), t30_feature_entries_a,
};

struct tegra_dc_feature t30_feature_table_b = {
	ARRAY_SIZE(t30_feature_entries_b), t30_feature_entries_b,
};

static struct tegra_dc_feature_entry t114_feature_entries_a[] = {
	{ 0, TEGRA_DC_FEATURE_FORMATS, {TEGRA_WIN_FMT_BASE,} },
	{ 0, TEGRA_DC_FEATURE_BLEND_TYPE, {1,} },
	{ 0, TEGRA_DC_FEATURE_MAXIMUM_SIZE, {4095, 1, 4095, 1,} },
	{ 0, TEGRA_DC_FEATURE_MAXIMUM_SCALE, {2, 2, 2, 2,} },
	{ 0, TEGRA_DC_FEATURE_FILTER_TYPE, {1, 1,} },
	{ 0, TEGRA_DC_FEATURE_LAYOUT_TYPE, {1, 1,} },
	{ 0, TEGRA_DC_FEATURE_INVERT_TYPE, {1, 1, 1,} },

	{ 1, TEGRA_DC_FEATURE_FORMATS, {TEGRA_WIN_FMT_BASE,} },
	{ 1, TEGRA_DC_FEATURE_BLEND_TYPE, {1,} },
	{ 1, TEGRA_DC_FEATURE_MAXIMUM_SIZE, {4095, 1, 4095, 1,} },
	{ 1, TEGRA_DC_FEATURE_MAXIMUM_SCALE, {2, 2, 2, 2,} },
	{ 1, TEGRA_DC_FEATURE_FILTER_TYPE, {1, 1,} },
	{ 1, TEGRA_DC_FEATURE_LAYOUT_TYPE, {1, 1,} },
	{ 1, TEGRA_DC_FEATURE_INVERT_TYPE, {1, 1, 1,} },

	{ 2, TEGRA_DC_FEATURE_FORMATS, {TEGRA_WIN_FMT_BASE,} },
	{ 2, TEGRA_DC_FEATURE_BLEND_TYPE, {1,} },
	{ 2, TEGRA_DC_FEATURE_MAXIMUM_SIZE, {4095, 1, 4095, 1,} },
	{ 2, TEGRA_DC_FEATURE_MAXIMUM_SCALE, {2, 2, 2, 2,} },
	{ 2, TEGRA_DC_FEATURE_FILTER_TYPE, {1, 1,} },
	{ 2, TEGRA_DC_FEATURE_LAYOUT_TYPE, {1, 1,} },
	{ 2, TEGRA_DC_FEATURE_INVERT_TYPE, {1, 1, 1,} },
};

static struct tegra_dc_feature_entry t114_feature_entries_b[] = {
	{ 0, TEGRA_DC_FEATURE_FORMATS, {TEGRA_WIN_FMT_BASE,} },
	{ 0, TEGRA_DC_FEATURE_BLEND_TYPE, {1,} },
	{ 0, TEGRA_DC_FEATURE_MAXIMUM_SIZE, {4095, 1, 4095, 1,} },
	{ 0, TEGRA_DC_FEATURE_MAXIMUM_SCALE, {2, 2, 2, 2,} },
	{ 0, TEGRA_DC_FEATURE_FILTER_TYPE, {1, 1,} },
	{ 0, TEGRA_DC_FEATURE_LAYOUT_TYPE, {1, 1,} },
	{ 0, TEGRA_DC_FEATURE_INVERT_TYPE, {1, 1, 1,} },

	{ 1, TEGRA_DC_FEATURE_FORMATS, {TEGRA_WIN_FMT_BASE,} },
	{ 1, TEGRA_DC_FEATURE_BLEND_TYPE, {1,} },
	{ 1, TEGRA_DC_FEATURE_MAXIMUM_SIZE, {4095, 1, 4095, 1,} },
	{ 1, TEGRA_DC_FEATURE_MAXIMUM_SCALE, {2, 2, 2, 2,} },
	{ 1, TEGRA_DC_FEATURE_FILTER_TYPE, {1, 1,} },
	{ 1, TEGRA_DC_FEATURE_LAYOUT_TYPE, {1, 1,} },
	{ 1, TEGRA_DC_FEATURE_INVERT_TYPE, {1, 1, 1,} },

	{ 2, TEGRA_DC_FEATURE_FORMATS, {TEGRA_WIN_FMT_BASE,} },
	{ 2, TEGRA_DC_FEATURE_BLEND_TYPE, {1,} },
	{ 2, TEGRA_DC_FEATURE_MAXIMUM_SIZE, {4095, 1, 4095, 1,} },
	{ 2, TEGRA_DC_FEATURE_MAXIMUM_SCALE, {2, 2, 2, 2,} },
	{ 2, TEGRA_DC_FEATURE_FILTER_TYPE, {1, 1,} },
	{ 2, TEGRA_DC_FEATURE_LAYOUT_TYPE, {1, 1,} },
	{ 2, TEGRA_DC_FEATURE_INVERT_TYPE, {1, 1, 1,} },
};

struct tegra_dc_feature t114_feature_table_a = {
	ARRAY_SIZE(t114_feature_entries_a), t114_feature_entries_a,
};

struct tegra_dc_feature t114_feature_table_b = {
	ARRAY_SIZE(t114_feature_entries_b), t114_feature_entries_b,
};

int tegra_dc_get_feature(struct tegra_dc_feature *feature, int win_idx,
					enum tegra_dc_feature_option option)
{
	int i;
	struct tegra_dc_feature_entry *entry;

	if (!feature)
		return -EINVAL;

	for (i = 0; i < feature->num_entries; i++) {
		entry = &feature->entries[i];
		if (entry->window_index == win_idx && entry->option == option)
			return i;
	}

	return -EINVAL;
}

long *tegra_dc_parse_feature(struct tegra_dc *dc, int win_idx, int operation)
{
	int idx;
	struct tegra_dc_feature_entry *entry;
	enum tegra_dc_feature_option option;
	struct tegra_dc_feature *feature = dc->feature;

	switch (operation) {
	case GET_WIN_FORMATS:
		option = TEGRA_DC_FEATURE_FORMATS;
		break;
	case GET_WIN_SIZE:
		option = TEGRA_DC_FEATURE_MAXIMUM_SIZE;
		break;
	case HAS_SCALE:
		option = TEGRA_DC_FEATURE_MAXIMUM_SCALE;
		break;
	case HAS_TILED:
		option = TEGRA_DC_FEATURE_LAYOUT_TYPE;
		break;
	case HAS_V_FILTER:
		option = TEGRA_DC_FEATURE_FILTER_TYPE;
		break;
	case HAS_H_FILTER:
		option = TEGRA_DC_FEATURE_FILTER_TYPE;
		break;
	case HAS_GEN2_BLEND:
		option = TEGRA_DC_FEATURE_BLEND_TYPE;
		break;
	default:
		return NULL;
	}

	idx = tegra_dc_get_feature(feature, win_idx, option);
	if (IS_ERR_VALUE(idx))
		return NULL;
	entry = &feature->entries[idx];

	return entry->arg;
}

int tegra_dc_feature_has_scaling(struct tegra_dc *dc, int win_idx)
{
	int i;
	long *addr = tegra_dc_parse_feature(dc, win_idx, HAS_SCALE);

	for (i = 0; i < ENTRY_SIZE; i++)
		if (addr[i] != 1)
			return 1;
	return 0;
}

int tegra_dc_feature_has_tiling(struct tegra_dc *dc, int win_idx)
{
	long *addr = tegra_dc_parse_feature(dc, win_idx, HAS_TILED);

	return addr[TILED_LAYOUT];
}

int tegra_dc_feature_has_filter(struct tegra_dc *dc, int win_idx, int operation)
{
	long *addr = tegra_dc_parse_feature(dc, win_idx, operation);

	if (operation == HAS_V_FILTER)
		return addr[V_FILTER];
	else
		return addr[H_FILTER];
}

int tegra_dc_feature_is_gen2_blender(struct tegra_dc *dc, int win_idx)
{
	long *addr = tegra_dc_parse_feature(dc, win_idx, HAS_GEN2_BLEND);

	if (addr[BLEND_GENERATION] == 2)
		return 1;

	return 0;
}

void tegra_dc_feature_register(struct tegra_dc *dc)
{
	int i;
	struct tegra_dc_feature_entry *entry;
#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
	if (!dc->ndev->id)
		dc->feature = &t20_feature_table_a;
	else
		dc->feature = &t20_feature_table_b;
#elif defined(CONFIG_ARCH_TEGRA_3x_SOC)
	if (!dc->ndev->id)
		dc->feature = &t30_feature_table_a;
	else
		dc->feature = &t30_feature_table_b;
#elif defined(CONFIG_ARCH_TEGRA_11x_SOC)
	if (!dc->ndev->id)
		dc->feature = &t114_feature_table_a;
	else
		dc->feature = &t114_feature_table_b;
#endif
	/* Count the number of windows using gen1 blender. */
	dc->gen1_blend_num = 0;
	for (i = 0; i < dc->feature->num_entries; i++) {
		entry = &dc->feature->entries[i];
		if (entry->option == TEGRA_DC_FEATURE_BLEND_TYPE &&
					entry->arg[BLEND_GENERATION] == 1)
			dc->gen1_blend_num++;
	}
}
