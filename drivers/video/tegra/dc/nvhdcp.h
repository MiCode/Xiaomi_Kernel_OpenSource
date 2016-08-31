/*
 * drivers/video/tegra/dc/nvhdcp.h
 *
 * Copyright (c) 2010-2013, NVIDIA CORPORATION, All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __DRIVERS_VIDEO_TEGRA_DC_NVHDCP_H
#define __DRIVERS_VIDEO_TEGRA_DC_NVHDCP_H
#include <video/nvhdcp.h>

struct tegra_nvhdcp;
#ifdef CONFIG_TEGRA_NVHDCP
void tegra_nvhdcp_set_plug(struct tegra_nvhdcp *nvhdcp, bool hpd);
int tegra_nvhdcp_set_policy(struct tegra_nvhdcp *nvhdcp, int pol);
void tegra_nvhdcp_suspend(struct tegra_nvhdcp *nvhdcp);
void tegra_nvhdcp_resume(struct tegra_nvhdcp *nvhdcp);
struct tegra_nvhdcp *tegra_nvhdcp_create(struct tegra_dc_hdmi_data *hdmi,
					int id, int bus);
void tegra_nvhdcp_destroy(struct tegra_nvhdcp *nvhdcp);
#else
static inline void tegra_nvhdcp_set_plug(struct tegra_nvhdcp *nvhdcp, bool hpd)
{
}
static inline int tegra_nvhdcp_set_policy(struct tegra_nvhdcp *nvhdcp, int pol)
{
	return 0;
}
static inline void tegra_nvhdcp_suspend(struct tegra_nvhdcp *nvhdcp) { }
static inline void tegra_nvhdcp_resume(struct tegra_nvhdcp *nvhdcp) { }
static inline struct tegra_nvhdcp *tegra_nvhdcp_create(
	struct tegra_dc_hdmi_data *hdmi, int id, int bus)
{
	return NULL;
}
static inline void tegra_nvhdcp_destroy(struct tegra_nvhdcp *nvhdcp) { }
#endif

#endif
