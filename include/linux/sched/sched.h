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
#ifndef _LINUX_SCHED_SCHED_H
#define _LINUX_SCHED_SCHED_H

#ifdef CONFIG_MTK_SCHED_TRACE
#ifdef CONFIG_MTK_SCHED_DEBUG
#define mt_sched_printf(event, x...) \
do { \
	char strings[128] = "";  \
	snprintf(strings, sizeof(strings)-1, x); \
	pr_debug(x); \
	trace_##event(strings); \
} while (0)
#else
#define mt_sched_printf(event, x...) \
do { \
	char strings[128] = "";  \
	snprintf(strings, sizeof(strings)-1, x); \
	trace_##event(strings); \
} while (0)

#endif
#else
#define mt_sched_printf(event, x...) do {} while (0)
#endif


/* cpu_core_energy & cpu_cluster_energy both implmented in topology.c */
extern
const struct sched_group_energy * const cpu_core_energy(int cpu);

extern
const struct sched_group_energy * const cpu_cluster_energy(int cpu);

#ifdef CONFIG_MTK_SCHED_EAS_POWER_SUPPORT
typedef int (*idle_power_func)(int, int, int, void *, int);
typedef int (*busy_power_func)(int, int, void*, int);
#endif

#endif
