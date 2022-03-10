/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __GED_KPI_H__
#define __GED_KPI_H__

#include "ged_type.h"
#include "ged_bridge_id.h"
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
unsigned int ged_kpi_get_fw_idle(void);


void ged_kpi_get_latest_perf_state(long long *t_cpu_remained,
		long long *t_gpu_remained,
		long *t_cpu_target,
		long *t_gpu_target);

GED_ERROR ged_kpi_system_init(void);
void ged_kpi_system_exit(void);
bool ged_kpi_set_cpu_remained_time(long long t_cpu_remained,
		int QedBuffer_length);
bool ged_kpi_set_gpu_dvfs_hint(int t_gpu_target, int t_gpu_cur);
unsigned int ged_kpi_enabled(void);
void ged_kpi_set_target_FPS(u64 ulID, int target_FPS);
void ged_kpi_set_target_FPS_margin(u64 ulID, int target_FPS,
		int target_FPS_margin, int eara_fps_margin, int cpu_time);
void ged_kpi_set_fw_idle(unsigned int time);

GED_ERROR ged_kpi_timer_based_pick_riskyBQ(int *pT_gpu_real, int *pT_gpu_pipe,
	int *pT_gpu_target, unsigned long long *pullWnd);

/* For Gift Usage */
GED_ERROR ged_kpi_query_dvfs_freq_pred(int *gpu_freq_cur
	, int *gpu_freq_max, int *gpu_freq_pred);
GED_ERROR ged_kpi_query_gpu_dvfs_info(struct GED_BRIDGE_OUT_QUERY_GPU_DVFS_INFO *out);
GED_ERROR ged_kpi_set_gift_status(int mode);
GED_ERROR ged_kpi_set_gift_target_pid(int pid);

extern spinlock_t gsGpuUtilLock;

// extern unsigned int g_gpufreqv2;
#endif
