/*
 * arch/arm/mach-tegra/wakeups-t3.h
 *
 * Declarations of Tegra 3 LP0 wakeup sources
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __MACH_TEGRA_WAKEUPS_T3_H
#define __MACH_TEGRA_WAKEUPS_T3_H

#ifndef CONFIG_ARCH_TEGRA_3x_SOC
#error "Tegra 3 wakeup sources valid only for CONFIG_ARCH_TEGRA_3x_SOC"
#endif

#define TEGRA_WAKE_GPIO_PO5	0
#define TEGRA_WAKE_GPIO_PV1	1
#define TEGRA_WAKE_GPIO_PL1	2
#define TEGRA_WAKE_GPIO_PB6	3
#define TEGRA_WAKE_GPIO_PN7	4
#define TEGRA_WAKE_GPIO_PBB6	5
#define TEGRA_WAKE_GPIO_PU5	6
#define TEGRA_WAKE_GPIO_PU6	7
#define TEGRA_WAKE_GPIO_PC7	8
#define TEGRA_WAKE_GPIO_PS2	9
#define TEGRA_WAKE_GPIO_PAA1	10
#define TEGRA_WAKE_GPIO_PW3	11
#define TEGRA_WAKE_GPIO_PW2	12
#define TEGRA_WAKE_GPIO_PY6	13
#define TEGRA_WAKE_GPIO_PDD3	14
#define TEGRA_WAKE_GPIO_PJ2	15
#define TEGRA_WAKE_RTC_ALARM	16
#define TEGRA_WAKE_KBC_EVENT	17
#define TEGRA_WAKE_PWR_INT	18
#define TEGRA_WAKE_USB1_VBUS	19
#define TEGRA_WAKE_USB2_VBUS	20
#define TEGRA_WAKE_USB1_ID	21
#define TEGRA_WAKE_USB2_ID	22
#define TEGRA_WAKE_GPIO_PI5	23
#define TEGRA_WAKE_GPIO_PV0	24
#define TEGRA_WAKE_GPIO_PS4	25
#define TEGRA_WAKE_GPIO_PS5	26
#define TEGRA_WAKE_GPIO_PS0	27
#define TEGRA_WAKE_GPIO_PS6	28
#define TEGRA_WAKE_GPIO_PS7	29
#define TEGRA_WAKE_GPIO_PN2	30
/* bit 31 is unused */

#define TEGRA_WAKE_GPIO_PO4	32
#define TEGRA_WAKE_GPIO_PJ0	33
#define TEGRA_WAKE_GPIO_PK2	34
#define TEGRA_WAKE_GPIO_PI6	35
#define TEGRA_WAKE_GPIO_PBB1	36
#define TEGRA_WAKE_USB3_ID	37
#define TEGRA_WAKE_USB3_VBUS	38

#endif
