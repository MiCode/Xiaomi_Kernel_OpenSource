/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SCHED_CPUFREQ_H
#define _LINUX_SCHED_CPUFREQ_H

#include <linux/types.h>
#include "../../../drivers/misc/mediatek/base/power/include/mtk_upower.h"
/*
 * Interface between cpufreq drivers and the scheduler:
 */

#define SCHED_CPUFREQ_IOWAIT	(1U << 0)
#define SCHED_CPUFREQ_MIGRATION	(1U << 1)

#ifdef CONFIG_CPU_FREQ
struct cpufreq_policy;

struct update_util_data {
       void (*func)(struct update_util_data *data, u64 time, unsigned int flags);
};

void cpufreq_add_update_util_hook(int cpu, struct update_util_data *data,
                       void (*func)(struct update_util_data *data, u64 time,
				    unsigned int flags));
void cpufreq_remove_update_util_hook(int cpu);
bool cpufreq_this_cpu_can_update(struct cpufreq_policy *policy);

static inline unsigned long map_util_freq(unsigned long util,
					unsigned long freq, unsigned long cap)
{
	return (freq + (freq >> 2)) * util / cap;
}

#ifdef CONFIG_NONLINEAR_FREQ_CTL
extern unsigned int capacity_margin;
extern unsigned int mt_cpufreq_get_cpu_freq(int cpu, int idx);
__attribute__((unused)) static unsigned long mtk_map_util_freq(int cpu, unsigned long util)
{
	struct upower_tbl *tbl;
	int idx, cap, target_idx = 0;

#ifdef CONFIG_MTK_SCHED_EXTENSION
	util = util * capacity_margin / SCHED_CAPACITY_SCALE;
#endif

	tbl = upower_get_core_tbl(cpu);
	for (idx = 0; idx < tbl->row_num ; idx++) {
		cap = tbl->row[idx].cap;
		if (!cap)
			break;

		target_idx = idx;

		if (cap >= util)
			break;
	}

	return mt_cpufreq_get_cpu_freq(cpu, target_idx);
}
#endif /* CONFIG_NONLINEAR_FREQ_CTL */

#endif /* CONFIG_CPU_FREQ */

#endif /* _LINUX_SCHED_CPUFREQ_H */
