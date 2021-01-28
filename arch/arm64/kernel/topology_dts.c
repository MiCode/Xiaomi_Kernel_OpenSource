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
