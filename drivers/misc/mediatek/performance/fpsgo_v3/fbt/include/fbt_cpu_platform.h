/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __FBT_CPU_PLATFORM_H__
#define __FBT_CPU_PLATFORM_H__

#include "fbt_cpu.h"

enum FPSGO_CPU_PREFER {
	FPSGO_PREFER_NONE = 0,
	FPSGO_PREFER_BIG = 1,
	FPSGO_PREFER_LITTLE = 2,
	FPSGO_PREFER_TOTAL,
};

extern long sched_setaffinity(pid_t pid, const struct cpumask *in_mask);
extern int sched_set_cpuprefer(pid_t pid, unsigned int prefer_type);
extern int capacity_min_write_for_perf_idx(int idx, int capacity_min);
extern void cm_mgr_perf_set_status(int enable);
extern int set_task_util_min_pct(pid_t pid, unsigned int min);

void fbt_set_boost_value(unsigned int base_blc);
void fbt_clear_boost_value(void);
void fbt_set_per_task_min_cap(int pid, unsigned int base_blc);
int fbt_get_L_cluster_num(void);
int fbt_get_L_min_ceiling(void);
void fbt_notify_CM_limit(int reach_limit);
void fbt_reg_dram_request(int reg);
void fbt_boost_dram(int boost);
int fbt_get_default_boost_ta(void);
int fbt_get_default_adj_loading(void);
void fbt_set_cpu_prefer(int pid, unsigned int prefer_type);
void fbt_set_affinity(pid_t pid, unsigned int prefer_type);

#endif
