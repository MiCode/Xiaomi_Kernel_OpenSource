/*
 * arch/arm/mach-tegra/include/mach/tegra_wakeup_monitor.h
 *
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __MACH_TEGRA_WAKEUP_MONITOR_H
#define __MACH_TEGRA_WAKEUP_MONITOR_H

/* Wakeup source */
#define TEGRA_WAKEUP_SOURCE_OTHERS	0
#define TEGRA_WAKEUP_SOURCE_WIFI	1

/* Wow wakeup event*/
#define TEGRA_WOW_WAKEUP_ENABLE	"TEGRA_WOW_WAKEUP_ENABLE=1"
#define TEGRA_WOW_WAKEUP_DISABLE	"TEGRA_WOW_WAKEUP_ENABLE=0"

/* Suspend prepare uevent string */
#define TEGRA_SUSPEND_PREPARE_UEVENT_OTHERS	\
					"PM_SUSPEND_PREPARE_WAKEUP_SOURCE=0"
#define TEGRA_SUSPEND_PREPARE_UEVENT_WIFI	\
					"PM_SUSPEND_PREPARE_WAKEUP_SOURCE=1"
/* post suspend uevent string */
#define TEGRA_POST_SUSPEND_UEVENT	"PM_POST_SUSPEND"

/* Timeout to get cmd from up-lever */
#define TEGRA_WAKEUP_MONITOR_CMD_TIMEOUT_MS	100

/* tegra wakeup monitor platform data */
struct tegra_wakeup_monitor_platform_data {
	int wifi_wakeup_source;
};

#endif /* __MACH_TEGRA_WAKEUP_MONITOR_H */
