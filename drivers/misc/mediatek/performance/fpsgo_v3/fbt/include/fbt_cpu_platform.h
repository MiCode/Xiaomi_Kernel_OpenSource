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
	FPSGO_PREFER_L_M = 3,
	FPSGO_PREFER_TOTAL,
};

/* DO NOT change the value */
enum FPSGO_CPU_LIMIT {
	FPSGO_LIMIT_NO_LIMIT = 0,
	FPSGO_LIMIT_FREQ = 1,
	FPSGO_LIMIT_CPU = 2,
};

extern void cm_mgr_perf_set_status(int enable);

void fbt_set_boost_value(unsigned int base_blc);
void fbt_clear_boost_value(void);
void fbt_set_per_task_cap(int pid, unsigned int min_blc, unsigned int max_blc);
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
int fbt_get_cluster_limit(int *cluster, int *freq, int *r_freq, int *cpu);
int fbt_get_default_uboost(void);
int fbt_get_default_qr_enable(void);
int fbt_get_default_gcc_enable(void);
int fbt_get_default_sbe_rescue_enable(void);
int fbt_get_l_min_bhropp(void);
int fbt_get_default_gcc_chk_avg_deq(void);
void init_fbt_platform(void);
void exit_fbt_platform(void);

#endif
