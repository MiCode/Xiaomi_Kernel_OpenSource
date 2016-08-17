/*
 * arch/arm/mach-tegra/include/mach/tegra_rst_reason.h
 *
 * Copyright (c) 2013-2015, NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __MACH_TEGRA_RST_REASON_H
#define __MACH_TEGRA_RST_REASON_H

#define PMC_RST_STATUS 0x1b4
#define PMC_RST_MASK   0x7

#define PMC_SCRATCH0   0x50
#define PMC_RST_FLAG   PMC_SCRATCH0

static char *pmc_rst_reason_msg[] = {
	"power on reset",
	"watchdog timeout",
	"thermal overheating",
	"software reset",
	"lp0 wakeup",
};

enum pmic_rst_reason {
	INVALID,
	NO_REASON,
	POWERON_LONG_PRESS_KEY,
	POWERDOWN,
	WATCHDOG,
	THERMAL,
	RESET_SIGNAL,
	SW_RESET,
	BATTERY_LOW,
	GPADC,
	NUM_REASONS,
	FORCE32 = 0x7FFFFFFF,
};

static char *pmic_rst_reason_msg[] = {
	"invalid reason",
	"no reason",
	"long pressing poweron key",
	"pressing powerdown key",
	"watchdog timeout",
	"pmic overheating",
	"pressing reset key",
	"software reset",
	"low voltage on power supply",
	"gpadc shutdown",
};

#endif
