/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ARCH_ARM_MACH_MSM_LPM_STATS_H
#define __ARCH_ARM_MACH_MSM_LPM_STATS_H

struct lpm_stats;

#ifdef CONFIG_MSM_IDLE_STATS
struct lpm_stats *lpm_stats_config_level(const char *name,
	const char **levels, int num_levels, struct lpm_stats *parent,
	struct cpumask *mask);
void lpm_stats_cluster_enter(struct lpm_stats *stats, uint32_t index);
void lpm_stats_cluster_exit(struct lpm_stats *stats, uint32_t index,
				bool success);
void lpm_stats_cpu_enter(uint32_t index);
void lpm_stats_cpu_exit(uint32_t index, bool success);
void lpm_stats_suspend_enter(void);
void lpm_stats_suspend_exit(void);
#else
static inline struct lpm_stats *lpm_stats_config_level(const char *name,
	const char **levels, int num_levels, struct lpm_stats *parent,
	struct cpumask *mask)
{
	return ERR_PTR(-ENODEV);
}

static inline void lpm_stats_cluster_enter(struct lpm_stats *stats,
						uint32_t index)
{
	return;
}

static inline void lpm_stats_cluster_exit(struct lpm_stats *stats,
					uint32_t index, bool success)
{
	return;
}

static inline void lpm_stats_cpu_enter(uint32_t index)
{
	return;
}

static inline void lpm_stats_cpu_exit(uint32_t index, bool success)
{
	return;
}

static inline void lpm_stats_suspend_enter(void)
{
	return;
}

static inline void lpm_stats_suspend_exit(void)
{
	return;
}
#endif
#endif  /* __ARCH_ARM_MACH_MSM_LPM_STATS_H */
