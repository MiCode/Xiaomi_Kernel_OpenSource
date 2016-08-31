/*
 * Copyright (C) 2013, NVIDIA Corporation. All rights reserved.
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
 * include/linux/platform_data/tegra_ahci.h
 *
 *
 */

#ifndef __MACH_TEGRA_AHCI_PDATA_H
#define __MACH_TEGRA_AHCI_PDATA_H

struct tegra_ahci_platform_data {
	s16 gen2_rx_eq;
	int pexp_gpio;
};

#endif
