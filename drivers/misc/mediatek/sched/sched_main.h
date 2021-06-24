/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef SCHED_MAIN_H
#define SCHED_MAIN_H

#include "sched_sys_common.h"

#define MAX_PD_COUNT 3
#define MAX_CAP_ENTRYIES 168

#define DVFS_TBL_BASE_PHYS 0x0011BC00
#define SRAM_REDZONE 0x55AA55AAAA55AA55
#define CAPACITY_TBL_OFFSET 0xFA0
#define CAPACITY_TBL_SIZE 0x100
#define CAPACITY_ENTRY_SIZE 0x2

#define MIGR_IDLE_BALANCE 1

struct pd_capacity_info {
	int nr_caps;
	unsigned long *caps;
	struct cpumask cpus;
};

extern int mtk_static_power_init(void);

#if IS_ENABLED(CONFIG_MTK_OPP_CAP_INFO)
extern int pd_freq_to_opp(int cpu, unsigned long freq);
extern unsigned long pd_get_opp_capacity(int cpu, int opp);
#endif

#if IS_ENABLED(CONFIG_MTK_EAS)
extern void mtk_find_busiest_group(void *data, struct sched_group *busiest,
		struct rq *dst_rq, int *out_balance);
extern void mtk_find_energy_efficient_cpu(void *data, struct task_struct *p,
		int prev_cpu, int sync, int *new_cpu);
#if IS_ENABLED(CONFIG_MTK_THERMAL_AWARE_SCHEDULING)
extern int sort_thermal_headroom(struct cpumask *cpus, int *cpu_order);
#endif
extern int init_sram_info(void);
extern void mtk_tick_entry(void *data, struct rq *rq);
extern void mtk_set_wake_flags(void *data, int *wake_flags, unsigned int *mode);

#if IS_ENABLED(CONFIG_MTK_NEWIDLE_BALANCE)
extern void mtk_sched_newidle_balance(void *data, struct rq *this_rq, struct rq_flags *rf, int *pulled_task, int *done);
#endif
#endif
#endif
