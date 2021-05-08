/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */


#ifndef __GED_KPI_H__
#define __GED_KPI_H__

#include "ged_type.h"
/* To-Do: EAS*/
/*#include "eas_ctrl.h"*/
#include <linux/sched.h>
#include <linux/cpufreq.h>
#include <linux/topology.h>

GED_ERROR ged_kpi_dequeue_buffer_ts(int pid,
		u64 ullWdnd,
		int i32FrameID,
		int fence_fd,
		int isSF);
GED_ERROR ged_kpi_queue_buffer_ts(int pid,
		u64 ullWdnd,
		int i32FrameID,
		int fence,
		int QedBuffer_length);
GED_ERROR ged_kpi_acquire_buffer_ts(int pid, u64 ullWdnd, int i32FrameID);
GED_ERROR ged_kpi_sw_vsync(void);
GED_ERROR ged_kpi_hw_vsync(void);
int ged_kpi_get_uncompleted_count(void);

unsigned int ged_kpi_get_cur_fps(void);
unsigned int ged_kpi_get_cur_avg_cpu_time(void);
unsigned int ged_kpi_get_cur_avg_gpu_time(void);
unsigned int ged_kpi_get_cur_avg_response_time(void);
unsigned int ged_kpi_get_cur_avg_gpu_remained_time(void);
unsigned int ged_kpi_get_cur_avg_cpu_remained_time(void);
unsigned int ged_kpi_get_cur_avg_gpu_freq(void);
void ged_kpi_get_latest_perf_state(long long *t_cpu_remained,
		long long *t_gpu_remained,
		long *t_cpu_target,
		long *t_gpu_target);

GED_ERROR ged_kpi_system_init(void);
void ged_kpi_system_exit(void);
bool ged_kpi_set_cpu_remained_time(long long t_cpu_remained,
		int QedBuffer_length);
bool ged_kpi_set_gpu_dvfs_hint(int t_gpu_target, int t_gpu_cur);
void ged_kpi_set_game_hint(int mode);
unsigned int ged_kpi_enabled(void);
void ged_kpi_set_target_FPS(u64 ulID, int target_FPS);
void ged_kpi_set_target_FPS_margin(u64 ulID, int target_FPS,
	int target_FPS_margin, int cpu_time);
#ifdef GED_ENABLE_TIMER_BASED_DVFS_MARGIN
GED_ERROR ged_kpi_timer_based_pick_riskyBQ(int *pT_gpu_real, int *pT_gpu_pipe,
	int *pT_gpu_target, unsigned long long *pullWnd);
#endif
GED_ERROR ged_kpi_query_dvfs_freq_pred(int *gpu_freq_cur
	, int *gpu_freq_max, int *gpu_freq_pred);
GED_ERROR ged_kpi_set_gift_status(int mode);

extern int linear_real_boost(int linear_boost);
#ifdef GED_KPI_CPU_INFO
extern unsigned int sched_get_cpu_load(int cpu);
extern unsigned long cpufreq_scale_freq_capacity(struct sched_domain *sd
					, int cpu);
extern unsigned long arch_scale_get_max_freq(int cpu);
#endif
#ifdef GED_ENABLE_FB_DVFS
extern spinlock_t gsGpuUtilLock;
#endif
#endif
