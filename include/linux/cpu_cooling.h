/*
 *  linux/include/linux/cpu_cooling.h
 *
 *  Copyright (C) 2012	Samsung Electronics Co., Ltd(http://www.samsung.com)
 *  Copyright (C) 2012  Amit Daniel <amit.kachhap@linaro.org>
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#ifndef __CPU_COOLING_H__
#define __CPU_COOLING_H__

#include <linux/of.h>
#include <linux/thermal.h>
#include <linux/cpumask.h>

struct cpufreq_policy;

typedef int (*plat_mitig_t)(int cpu, u32 clip_freq);

struct cpu_cooling_ops {
	plat_mitig_t ceil_limit, floor_limit;
};

#ifdef CONFIG_CPU_THERMAL
/**
 * cpufreq_cooling_register - function to create cpufreq cooling device.
 * @policy: cpufreq policy.
 */
struct thermal_cooling_device *
cpufreq_cooling_register(struct cpufreq_policy *policy);

/**
 * cpufreq_platform_cooling_register - create cpufreq cooling device with
 * additional platform specific mitigation function.
 *
 * @policy: cpufreq policy
 * @plat_ops: the platform mitigation functions that will be called insted of
 * cpufreq, if provided.
 */
struct thermal_cooling_device *
cpufreq_platform_cooling_register(struct cpufreq_policy *policy,
					struct cpu_cooling_ops *ops);

/**
 * cpufreq_cooling_unregister - function to remove cpufreq cooling device.
 * @cdev: thermal cooling device pointer.
 */
void cpufreq_cooling_unregister(struct thermal_cooling_device *cdev);

#else /* !CONFIG_CPU_THERMAL */
static inline struct thermal_cooling_device *
cpufreq_cooling_register(struct cpufreq_policy *policy)
{
	return ERR_PTR(-ENOSYS);
}

static inline
void cpufreq_cooling_unregister(struct thermal_cooling_device *cdev)
{
	return;
}
#endif	/* CONFIG_CPU_THERMAL */

#if defined(CONFIG_THERMAL_OF) && defined(CONFIG_CPU_THERMAL)
/**
 * of_cpufreq_cooling_register - create cpufreq cooling device based on DT.
 * @policy: cpufreq policy.
 */
struct thermal_cooling_device *
of_cpufreq_cooling_register(struct cpufreq_policy *policy);
#else
static inline struct thermal_cooling_device *
of_cpufreq_cooling_register(struct cpufreq_policy *policy)
{
	return NULL;
}

static inline struct thermal_cooling_device *
cpufreq_platform_cooling_register(struct cpufreq_policy *policy,
					struct cpu_cooling_ops *ops)
{
	return NULL;
}
#endif /* defined(CONFIG_THERMAL_OF) && defined(CONFIG_CPU_THERMAL) */

#ifdef CONFIG_QTI_CPU_ISOLATE_COOLING_DEVICE
extern void cpu_cooling_max_level_notifier_register(struct notifier_block *n);
extern void cpu_cooling_max_level_notifier_unregister(struct notifier_block *n);
extern const struct cpumask *cpu_cooling_get_max_level_cpumask(void);
#else
static inline
void cpu_cooling_max_level_notifier_register(struct notifier_block *n)
{
}

static inline
void cpu_cooling_max_level_notifier_unregister(struct notifier_block *n)
{
}

static inline const struct cpumask *cpu_cooling_get_max_level_cpumask(void)
{
	return cpu_none_mask;
}
#endif /* CONFIG_QTI_CPU_ISOLATE_COOLING_DEVICE */
#endif /* __CPU_COOLING_H__ */
