/*
 * drivers/video/tegra/dc/lvds.h
 *
 * Copyright (c) 2012, NVIDIA Corporation.
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

#ifndef __DRIVER_VIDEO_TEGRA_DC_LVDS_H__
#define __DRIVER_VIDEO_TEGRA_DC_LVDS_H__

#include "sor.h"


struct tegra_dc_lvds_data {
	struct tegra_dc			*dc;
	struct tegra_dc_sor_data	*sor;

	bool   suspended;
};


#endif
