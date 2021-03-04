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

typedef int (*get_static_t)(cpumask_t *cpumask, int interval,
			    unsigned long voltage, u32 *power);
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

struct thermal_cooling_device *
cpufreq_power_cooling_register(struct cpufreq_policy *policy,
			       u32 capacitance, get_static_t plat_static_func);

struct thermal_cooling_device *
cpufreq_platform_cooling_register(const struct cpumask *clip_cpus,
					struct cpu_cooling_ops *ops);
/*add for thermal-k7*/
void cpu_limits_set_level(unsigned int cpu, unsigned int max_freq);

/**
 * of_cpufreq_cooling_register - create cpufreq cooling device based on DT.
 * @np: a valid struct device_node to the cooling device device tree node.
 * @policy: cpufreq policy.
 */
#ifdef CONFIG_THERMAL_OF
struct thermal_cooling_device *
of_cpufreq_cooling_register(struct device_node *np,
			    struct cpufreq_policy *policy);

struct thermal_cooling_device *
of_cpufreq_power_cooling_register(struct device_node *np,
				  struct cpufreq_policy *policy,
				  u32 capacitance,
				  get_static_t plat_static_func);
#else
static inline struct thermal_cooling_device *
of_cpufreq_cooling_register(struct device_node *np,
			    struct cpufreq_policy *policy)
{
	return ERR_PTR(-ENOSYS);
}

static inline struct thermal_cooling_device *
of_cpufreq_power_cooling_register(struct device_node *np,
				  struct cpufreq_policy *policy,
				  u32 capacitance,
				  get_static_t plat_static_func)
{
	return NULL;
}
#endif

/**
 * cpufreq_cooling_unregister - function to remove cpufreq cooling device.
 * @cdev: thermal cooling device pointer.
 */
void cpufreq_cooling_unregister(struct thermal_cooling_device *cdev);

extern void cpu_cooling_max_level_notifier_register(struct notifier_block *n);
extern void cpu_cooling_max_level_notifier_unregister(struct notifier_block *n);
extern const struct cpumask *cpu_cooling_get_max_level_cpumask(void);
#else /* !CONFIG_CPU_THERMAL */
static inline struct thermal_cooling_device *
cpufreq_cooling_register(struct cpufreq_policy *policy)
{
	return ERR_PTR(-ENOSYS);
}
static inline struct thermal_cooling_device *
cpufreq_power_cooling_register(struct cpufreq_policy *policy,
			       u32 capacitance, get_static_t plat_static_func)
{
	return NULL;
}

static inline struct thermal_cooling_device *
of_cpufreq_cooling_register(struct device_node *np,
			    struct cpufreq_policy *policy)
{
	return ERR_PTR(-ENOSYS);
}

static inline struct thermal_cooling_device *
of_cpufreq_power_cooling_register(struct device_node *np,
				  struct cpufreq_policy *policy,
				  u32 capacitance,
				  get_static_t plat_static_func)
{
	return NULL;
}

static inline struct thermal_cooling_device *
cpufreq_platform_cooling_register(const struct cpumask *clip_cpus,
					struct cpu_cooling_ops *ops)
{
	return NULL;
}

static inline
void cpufreq_cooling_unregister(struct thermal_cooling_device *cdev)
{
	return;
}

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
#endif	/* CONFIG_CPU_THERMAL */

#endif /* __CPU_COOLING_H__ */
