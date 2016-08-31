/*
 * arch/arm/mach-tegra/include/mach/tegra_usb_modem_power.c
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __MACH_TEGRA_BBC_PROXY_H
#define __MACH_TEGRA_BBC_PROXY_H

#include <linux/edp.h>

struct tegra_bbc_proxy_platform_data {
	struct edp_client *modem_boot_edp_client;
	char *edp_manager_name;
	char *ap_name;
	unsigned int i_breach_ppm;
	unsigned int i_thresh_3g_adjperiod;
	unsigned int i_thresh_lte_adjperiod;
};

int tegra_bbc_proxy_edp_register(struct device *dev, u32 num_states,
				u32 *states);
int tegra_bbc_proxy_edp_request(struct device *dev, u32 mode, u32 state,
				u32 threshold);
int tegra_bbc_proxy_bw_register(struct device *dev, u32 bw);
int tegra_bbc_proxy_bw_request(struct device *dev, u32 mode, u32 bw, u32 lt,
				u32 margin);

int tegra_bbc_proxy_restore_iso(struct device *dev);
int tegra_bbc_proxy_clear_iso(struct device *dev);
#endif /* __MAC_TEGRA_BBC_PROXY_H */
