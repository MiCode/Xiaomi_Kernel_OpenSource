/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

 #ifndef __MACH_TEGRA_MCU_C2PORT_PLATFORM_DATA_H
 #define __MACH_TEGRA_MCU_C2PORT_PLATFORM_DATA_H

/* The platform data is used to tell c2port driver
 * GPIO configuratioon.
 */
struct tegra_c2port_platform_data {
	unsigned int gpio_c2ck;
	unsigned int gpio_c2d;
};

 #endif

