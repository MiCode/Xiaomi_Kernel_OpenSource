/*
 * arch/arm/mach-tegra/wakeups-t11x.h
 *
 * Declarations of Tegra 11x LP0 wakeup sources
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __MACH_TEGRA_WAKEUPS_T11X_H
#define __MACH_TEGRA_WAKEUPS_T11X_H

#ifndef CONFIG_ARCH_TEGRA_11x_SOC
#error "Tegra 11x wakeup sources valid only for CONFIG_ARCH_TEGRA_11x_SOC"
#endif

#define TEGRA_WAKE_GPIO_PO5	0
#define TEGRA_WAKE_GPIO_PV1	1
#define TEGRA_WAKE_GPIO_PU5	6
#define TEGRA_WAKE_GPIO_PU6	7
#define TEGRA_WAKE_GPIO_PC7	8
#define TEGRA_WAKE_GPIO_PS2	9
#define TEGRA_WAKE_GPIO_PW3	11
#define TEGRA_WAKE_GPIO_PW2	12
#define TEGRA_WAKE_GPIO_PDD3	14
#define TEGRA_WAKE_GPIO_PI5	23
#define TEGRA_WAKE_GPIO_PV0	24
#define TEGRA_WAKE_GPIO_PS0	27
#define TEGRA_WAKE_GPIO_PJ0	33
#define TEGRA_WAKE_GPIO_PK2	34
#define TEGRA_WAKE_GPIO_PI6	35
#define TEGRA_WAKE_GPIO_PBB6	45
#define TEGRA_WAKE_GPIO_PT6	47
#define TEGRA_WAKE_GPIO_PR7	49
#define TEGRA_WAKE_GPIO_PR4	50
#define TEGRA_WAKE_GPIO_PQ0	51
#define TEGRA_WAKE_GPIO_PQ5	54
#define TEGRA_WAKE_GPIO_PV2	56

#endif
