/*
 * Copyright (C) 2012 NVIDIA CORPORATION. All rights reserved.
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
 *
 */

#ifndef __MACH_TEGRA_PMC_H
#define __MACH_TEGRA_PMC_H

enum tegra_suspend_mode {
	TEGRA_SUSPEND_NONE = 0,
	TEGRA_SUSPEND_LP2,	/* CPU voltage off */
	TEGRA_SUSPEND_LP1,	/* CPU voltage off, DRAM self-refresh */
	TEGRA_SUSPEND_LP0,      /* CPU + core voltage off, DRAM self-refresh */
	TEGRA_MAX_SUSPEND_MODE,
};

#if defined(CONFIG_OF) && defined(CONFIG_COMMON_CLK) && defined(CONFIG_PM_SLEEP)
void set_power_timers(unsigned long us_on, unsigned long us_off);
#endif

bool tegra_pmc_cpu_is_powered(int cpuid);
int tegra_pmc_cpu_power_on(int cpuid);
int tegra_pmc_cpu_remove_clamping(int cpuid);

void tegra_pmc_init(void);

struct pmc_pm_data {
	u32 cpu_good_time;	/* CPU power good time in uS */
	u32 cpu_off_time;	/* CPU power off time in uS */
	u32 core_osc_time;	/* Core power good osc time in uS */
	u32 core_pmu_time;	/* Core power good pmu time in uS */
	u32 core_off_time;	/* Core power off time in uS */
	bool corereq_high;	/* Core power request active-high */
	bool sysclkreq_high;	/* System clock request active-high */
	bool combined_req;	/* Combined pwr req for CPU & Core */
	bool cpu_pwr_good_en;	/* CPU power good signal is enabled */
	u32 lp0_vec_phy_addr;	/* The phy addr of LP0 warm boot code */
	u32 lp0_vec_size;	/* The size of LP0 warm boot code */
	enum tegra_suspend_mode suspend_mode;
};
struct pmc_pm_data *tegra_get_pm_data(void);

#endif
