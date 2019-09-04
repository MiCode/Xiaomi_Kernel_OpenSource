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

/*
 * generic entry point for cpu mask construction, dedicated for
 * mediatek scheduler.
 */
void __init arch_build_cpu_topology_domain(void) {}

#ifdef CONFIG_MTK_UNIFY_POWER

#include "../../../drivers/misc/mediatek/base/power/include/mtk_upower.h"

/* sd energy functions */
inline
const struct sched_group_energy * const cpu_cluster_energy(int cpu)
{
	struct sched_group_energy *sge = sge_array[cpu][SD_LEVEL1];
	int cluster_id = cpu_topology[cpu].socket_id;
	struct upower_tbl_info **addr_ptr_tbl_info;
	struct upower_tbl_info *ptr_tbl_info;
	struct upower_tbl *ptr_tbl;

	if (!sge) {
		pr_warn("Invalid sched_group_energy for Cluster%d\n", cpu);
		return NULL;
	}

	addr_ptr_tbl_info = upower_get_tbl();
	ptr_tbl_info = *addr_ptr_tbl_info;

	ptr_tbl = ptr_tbl_info[UPOWER_BANK_CLS_BASE+cluster_id].p_upower_tbl;

	sge->nr_cap_states = ptr_tbl->row_num;
	sge->cap_states = ptr_tbl->row;
	sge->lkg_idx = ptr_tbl->lkg_idx;

	return sge;
}

inline
const struct sched_group_energy * const cpu_core_energy(int cpu)
{
	struct sched_group_energy *sge = sge_array[cpu][SD_LEVEL0];
	struct upower_tbl *ptr_tbl;

	if (!sge) {
		pr_warn("Invalid sched_group_energy for CPU%d\n", cpu);
		return NULL;
	}

	ptr_tbl = upower_get_core_tbl(cpu);

	sge->nr_cap_states = ptr_tbl->row_num;
	sge->cap_states = ptr_tbl->row;
	sge->lkg_idx = ptr_tbl->lkg_idx;

	return sge;
}
#endif

int arch_get_nr_clusters(void)
{
	int __arch_nr_clusters = -1;
	int max_id = 0;
	unsigned int cpu;

	/* assume socket id is monotonic increasing without gap. */
	for_each_possible_cpu(cpu) {
		struct cputopo_arm *cpu_topo = &cpu_topology[cpu];

		if (cpu_topo->socket_id > max_id)
			max_id = cpu_topo->socket_id;
	}
	__arch_nr_clusters = max_id + 1;
	return __arch_nr_clusters;
}

int arch_is_multi_cluster(void)
{
	return (arch_get_nr_clusters() > 1);
}

void arch_get_cluster_cpus(struct cpumask *cpus, int socket_id)
{
	unsigned int cpu;

	cpumask_clear(cpus);
	for_each_possible_cpu(cpu) {
		struct cputopo_arm *cpu_topo = &cpu_topology[cpu];

		if (cpu_topo->socket_id == socket_id)
			cpumask_set_cpu(cpu, cpus);
	}
}

unsigned long arch_get_max_cpu_capacity(int cpu)
{
	return per_cpu(cpu_scale, cpu);
}


unsigned long arch_get_cur_cpu_capacity(int cpu)
{
	unsigned long scale_freq;

	scale_freq  = arch_scale_freq_capacity(NULL, cpu);

	if (!scale_freq)
		scale_freq = SCHED_CAPACITY_SCALE;

	return (per_cpu(cpu_scale, cpu) * scale_freq / SCHED_CAPACITY_SCALE);
}

int arch_get_cluster_id(unsigned int cpu)
{
	struct cputopo_arm *cpu_topo = &cpu_topology[cpu];

	return cpu_topo->socket_id < 0 ? 0 : cpu_topo->socket_id;
}

int arch_is_smp(void)
{
	int cap = 0, max_cap = 0;
	int cpu;

	for_each_possible_cpu(cpu) {
		cap = arch_get_max_cpu_capacity(cpu);
		if (max_cap == 0)
			max_cap = cap;

		if (max_cap != cap)
			return 0;
	}
	return 1;
}
