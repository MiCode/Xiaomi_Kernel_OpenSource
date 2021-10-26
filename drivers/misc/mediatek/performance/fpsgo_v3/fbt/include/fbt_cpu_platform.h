/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __FBT_CPU_PLATFORM_H__
#define __FBT_CPU_PLATFORM_H__

#include "fbt_cpu.h"

enum FPSGO_CPU_PREFER {
	FPSGO_PREFER_NONE = 0,
	FPSGO_PREFER_BIG = 1,
	FPSGO_PREFER_LITTLE = 2,
	FPSGO_PREFER_L_M = 3,
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
int fbt_get_L_min_ceiling(void);
void fbt_notify_CM_limit(int reach_limit);
void fbt_reg_dram_request(int reg);
void fbt_boost_dram(int boost);
int fbt_get_default_boost_ta(void);
int fbt_get_default_adj_loading(void);
int fbt_get_default_adj_count(void);
int fbt_get_default_adj_tdiff(void);
void fbt_set_cpu_prefer(int pid, unsigned int prefer_type);
void fbt_set_affinity(pid_t pid, unsigned int prefer_type);
int fbt_get_cluster_limit(int *cluster, int *freq, int *r_freq);
int fbt_get_default_uboost(void);
int fbt_get_default_qr_enable(void);
int fbt_get_default_gcc_enable(void);
int fbt_get_l_min_bhropp(void);

#endif
