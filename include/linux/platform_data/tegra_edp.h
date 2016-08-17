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

#ifndef _LINUX_TEGRA_EDP_H
#define _LINUX_TEGRA_EDP_H

#include <linux/kernel.h>
#include <linux/errno.h>

struct tegra_system_edp_entry {
	char speedo_id;
	char power_limit_100mW;
	unsigned int freq_limits[NR_CPUS];
};

struct tegra_sysedp_devcap {
	unsigned int cpu_power;
	unsigned int gpufreq;
	unsigned int emcfreq;
};

struct tegra_sysedp_corecap {
	unsigned int power;
	struct tegra_sysedp_devcap cpupri;
	struct tegra_sysedp_devcap gpupri;
};

struct tegra_sysedp_platform_data {
	struct tegra_system_edp_entry *cpufreq_lim;
	unsigned int cpufreq_lim_size;
	struct tegra_sysedp_corecap *corecap;
	struct tegra_sysedp_corecap *high_corecap;
	unsigned int corecap_size;
	unsigned int init_req_watts;
};

enum tegra_sysedp_profile {
	TEGRA_SYSEDP_PROFILE_NORMAL,
	TEGRA_SYSEDP_PROFILE_HIGHCORE,
	TEGRA_SYSEDP_PROFILE_NUM
};

#ifdef CONFIG_EDP_FRAMEWORK
void tegra_edp_notify_gpu_load(unsigned int load);
#else
static inline void tegra_edp_notify_gpu_load(unsigned int load) {}
#endif

#endif
