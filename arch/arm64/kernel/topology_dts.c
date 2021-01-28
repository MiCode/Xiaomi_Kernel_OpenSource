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

 #include "../../../drivers/misc/mediatek/base/power/include/mtk_upower.h"

/* sd energy functions */
inline
const struct sched_group_energy * const cpu_cluster_energy(int cpu)
{
	struct sched_group_energy *sge = sge_array[cpu][SD_LEVEL1];
	int cluster_id = cpu_topology[cpu].cluster_id;
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

/* TODO: implement function */
int arch_get_nr_clusters(void)
{
	return 2;
}

/* TODO: implement function */
void arch_get_cluster_cpus(struct cpumask *cpus, int cluster_id)
{
}

/* TODO: implement function */
unsigned long arch_get_max_cpu_capacity(int cpu)
{
	return 0;
}

/* TODO: implement function */
unsigned long arch_get_cur_cpu_capacity(int cpu)
{
	return 0;
}

/* TODO: implement function */
int arch_get_cluster_id(unsigned int cpu)
{
	return 0;
}
