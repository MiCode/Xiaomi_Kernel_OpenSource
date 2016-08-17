/*
 * arch/arm/mach-tegra/cpuidle.h
 *
 * Declarations for power state transition code
 *
 * Copyright (c) 2011-2012, NVIDIA Corporation.
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

#ifndef __MACH_TEGRA_CPUIDLE_H
#define __MACH_TEGRA_CPUIDLE_H

#include <linux/cpuidle.h>

#ifdef CONFIG_PM_SLEEP

#define TEGRA_CPUIDLE_FORCE_DO_CLKGT_VMIN 0x1
#define TEGRA_CPUIDLE_FORCE_NO_CLKGT_VMIN 0x2

extern int tegra_pg_exit_latency;
extern u32 tegra_force_clkgt_at_vmin;

struct tegra_cpuidle_ops {
	bool (*tegra_idle_pd)(struct cpuidle_device *dev,
				struct cpuidle_state *state);
	void (*cpu_idle_stats_pd_ready)(unsigned int cpu);
	void (*cpu_idle_stats_pd_time)(unsigned int cpu, s64 us);
	bool (*pd_is_allowed)(struct cpuidle_device *dev,
				struct cpuidle_state *state);
#ifdef CONFIG_DEBUG_FS
	int (*pd_debug_show)(struct seq_file *s, void *data);
#endif
};

int tegra2_cpuidle_init_soc(struct tegra_cpuidle_ops *ops);
int tegra3_cpuidle_init_soc(struct tegra_cpuidle_ops *ops);
int tegra11x_cpuidle_init_soc(struct tegra_cpuidle_ops *ops);
int tegra11_cpu_lp_idle_rate_exchange(unsigned long *rate);
int tegra11_cpu_g_idle_rate_exchange(unsigned long *rate);

static inline int tegra_cpuidle_init_soc(struct tegra_cpuidle_ops *ops)
{
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	return tegra2_cpuidle_init_soc(ops);
#endif
#ifdef CONFIG_ARCH_TEGRA_3x_SOC
	return tegra3_cpuidle_init_soc(ops);
#endif
#ifdef CONFIG_ARCH_TEGRA_11x_SOC
	return tegra11x_cpuidle_init_soc(ops);
#endif
}

static inline int tegra_cpu_g_idle_rate_exchange(unsigned long *rate)
{
#ifdef CONFIG_ARCH_TEGRA_11x_SOC
	return tegra11_cpu_g_idle_rate_exchange(rate);
#else
	return -ENOSYS;
#endif
}

static inline int tegra_cpu_lp_idle_rate_exchange(unsigned long *rate)
{
#ifdef CONFIG_ARCH_TEGRA_11x_SOC
	return tegra11_cpu_lp_idle_rate_exchange(rate);
#else
	return -ENOSYS;
#endif
}

static inline void tegra_pd_set_global_latency(struct cpuidle_state *state)
{
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	state->exit_latency = tegra_pg_exit_latency;
#endif
	/* Tegra3 does not use global exit latency */
}

void tegra_pd_update_target_residency(struct cpuidle_state *state);

#endif /* CONFIG_PM_SLEEP */

#if defined(CONFIG_CPU_IDLE) && defined(CONFIG_PM_SLEEP)
void tegra_pd_in_idle(bool enable);
#else
static inline void tegra_pd_in_idle(bool enable) {}
#endif

#endif
