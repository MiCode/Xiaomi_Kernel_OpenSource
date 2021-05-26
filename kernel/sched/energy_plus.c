/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifdef CONFIG_MTK_UNIFY_POWER
#include "../../drivers/misc/mediatek/base/power/include/mtk_upower.h"
/*
 * MTK static specific energy cost model data. There are no unit requirements
 * for the data. Data can be normalized to any reference point, but the
 * normalization must be consistent. That is, one bogo-joule/watt must be the
 * same quantity for all data, but we don't care what it is.
 */
static struct idle_state default_idle_states[] = {
	{ .power = 0 }, /* 0: active idle = WFI, [P8].leak */
	{ .power = 0 }, /* 1: disabled */
	{ .power = 0 }, /* 2: disabled */
	{ .power = 0 }, /* 3: disabled */
	{ .power = 0 }, /* 4: MCDI */
	{ .power = 0 }, /* 5: disabled */
	{ .power = 0 }, /* 6: WFI/SPARK */
};

static void update_cpu_capacity(unsigned int cpu)
{
	unsigned long capacity = SCHED_CAPACITY_SCALE;

	if (cpu_core_energy(cpu)) {
		int max_cap_idx = cpu_core_energy(cpu)->nr_cap_states - 1;

		capacity = cpu_core_energy(cpu)->cap_states[max_cap_idx].cap;
	}

	topology_set_cpu_scale(cpu, capacity);

	pr_info("CPU%d: update cpu_capacity %lu\n",
		cpu, arch_scale_cpu_capacity(NULL, cpu));
}

void init_sched_energy_costs(void)
{
	struct sched_group_energy *sge;
	int sd_level, cpu;

	for_each_possible_cpu(cpu) {

		for_each_possible_sd_level(sd_level) {
			sge = kcalloc(1, sizeof(struct sched_group_energy),
				GFP_NOWAIT);

			sge->nr_idle_states = ARRAY_SIZE(default_idle_states);
#ifdef CONFIG_MTK_SCHED_EAS_POWER_SUPPORT
			sge->idle_power = mtk_idle_power;
			sge->busy_power = mtk_busy_power;
#endif
			sge->idle_states = default_idle_states;
			sge_array[cpu][sd_level] = sge;
		}
		update_cpu_capacity(cpu);
	}


	return;
}

#endif

