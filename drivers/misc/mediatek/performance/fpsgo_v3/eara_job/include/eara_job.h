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
#ifndef __EARA_JOB_H__
#define __EARA_JOB_H__

#define MAX_DEVICE 2
void fpsgo_ctrl2eara_nn_job_collect(int pid, int tid, unsigned long long mid,
	int hw_type, int num_step, __s32 *boost,
	__s32 *device, __u64 *exec_time);
int fpsgo_ctrl2eara_get_nn_priority(unsigned int pid,
	unsigned long long mid);
void fpsgo_ctrl2eara_get_nn_ttime(unsigned int pid,
	unsigned long long mid, int num_step, __u64 *time);
void fpsgo_fstb2eara_notify_fps_bound(void);
void fpsgo_fstb2eara_notify_fps_active(int active);
void fpsgo_fstb2eara_get_exec_time(int pid,
	unsigned long long mid, unsigned long long *t_v,
	unsigned long long *t_m);
void fpsgo_fstb2eara_get_boost_value(int pid, unsigned long long mid,
	int *b_v, int *b_m);
void fpsgo_fstb2eara_optimize_power(unsigned long long mid,
	int tgid, long long *t_c_time, long long t_t_t,
	long long c_time, long long v_time, long long m_time,
	unsigned int c_cap, unsigned int v_cap, unsigned int m_cap);
void fpsgo_fstb2eara_get_jobs_status(int *vpu_cross,
		int *mdla_cross, int *vpu_bg, int *mdla_bg);
#endif

