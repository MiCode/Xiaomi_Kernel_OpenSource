/*
 * tegra_alt_asoc_utils.h - Definitions for MCLK and DAP Utility driver
 *
 * Author: Stephen Warren <swarren@nvidia.com>
 * Copyright (c) 2011-2013 NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef __TEGRA_ASOC_UTILS_ALT_H__
#define __TEGRA_ASOC_UTILS_ALT_H_

struct clk;
struct device;

enum tegra_asoc_utils_soc {
	TEGRA_ASOC_UTILS_SOC_TEGRA20,
	TEGRA_ASOC_UTILS_SOC_TEGRA30,
	TEGRA_ASOC_UTILS_SOC_TEGRA114,
	TEGRA_ASOC_UTILS_SOC_TEGRA148,
	TEGRA_ASOC_UTILS_SOC_TEGRA124,
};

struct tegra_asoc_audio_clock_info {
	struct device *dev;
	struct snd_soc_card *card;
	enum tegra_asoc_utils_soc soc;
	struct clk *clk_pll_a;
	int clk_pll_a_state;
	struct clk *clk_pll_a_out0;
	int clk_pll_a_out0_state;
	struct clk *clk_cdev1;
	int clk_cdev1_state;
	struct clk *clk_out1;
	struct clk *clk_m;
	int clk_m_state;
	struct clk *clk_pll_p_out1;
	int set_mclk;
	int lock_count;
	int set_baseclock;
};

int tegra_alt_asoc_utils_set_rate(struct tegra_asoc_audio_clock_info *data,
				int srate,
				int mclk,
				int clk_out_rate);
void tegra_alt_asoc_utils_lock_clk_rate(
				struct tegra_asoc_audio_clock_info *data,
				int lock);
int tegra_alt_asoc_utils_init(struct tegra_asoc_audio_clock_info *data,
				struct device *dev, struct snd_soc_card *card);
void tegra_alt_asoc_utils_fini(struct tegra_asoc_audio_clock_info *data);

int tegra_alt_asoc_utils_set_parent(struct tegra_asoc_audio_clock_info *data,
				int is_i2s_master);
int tegra_alt_asoc_utils_clk_enable(struct tegra_asoc_audio_clock_info *data);
int tegra_alt_asoc_utils_clk_disable(struct tegra_asoc_audio_clock_info *data);
int tegra_alt_asoc_utils_register_ctls(struct tegra_asoc_audio_clock_info *data);

int tegra_alt_asoc_utils_tristate_dap(int id, bool tristate);

#endif
