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
