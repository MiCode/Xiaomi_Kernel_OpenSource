/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __FPSGO_USEDEXT_H__
#define __FPSGO_USEDEXT_H__

extern void (*cpufreq_notifier_fp)(int cid, unsigned long freq);
extern void (*fpsgo_notify_qudeq_fp)(int qudeq, unsigned int startend,
		int pid, unsigned long long identifier);
extern void (*fpsgo_notify_connect_fp)(int pid, int connectedAPI,
		unsigned long long identifier);
extern void (*fpsgo_notify_bqid_fp)(int pid, unsigned long long bufID,
		int queue_SF,
		unsigned long long identifier, int create);
extern void (*fpsgo_notify_vsync_fp)(void);
extern void (*fpsgo_notify_swap_buffer_fp)(int pid);

extern void (*fpsgo_get_fps_fp)(int *pid, int *fps);
extern void (*fpsgo_notify_nn_job_begin_fp)(unsigned int tid,
		unsigned long long mid);
extern void (*fpsgo_notify_nn_job_end_fp)(int pid, int tid,
		unsigned long long mid, int num_step, __s32 *boost,
		__s32 *device, __u64 *exec_time);
extern int (*fpsgo_get_nn_priority_fp)(unsigned int pid,
		unsigned long long mid);
extern void (*fpsgo_get_nn_ttime_fp)(unsigned int pid,
		unsigned long long mid, int num_step, __u64 *time);

extern void (*ged_vsync_notifier_fp)(void);

int fpsgo_is_force_enable(void);
void fpsgo_force_switch_enable(int enable);

int fpsgo_is_gpu_block_boost_enable(void);
int fpsgo_is_gpu_block_boost_perf_enable(void);
int fpsgo_is_gpu_block_boost_camera_enable(void);
void fpsgo_gpu_block_boost_enable_perf(int enable);
void fpsgo_gpu_block_boost_enable_camera(int enable);
#endif
