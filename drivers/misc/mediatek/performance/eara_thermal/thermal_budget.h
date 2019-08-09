/*
 * Copyright (C) 2018 MediaTek Inc.
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

#ifndef EARA_THRM_PB_H
#define EARA_THRM_PB_H

enum THRM_MODULE {
	THRM_GPU,
	THRM_VPU,
	THRM_MDLA,
	THRM_CPU_OFFSET,
	THRM_CPU,
};

extern struct ppm_cobra_data *ppm_cobra_pass_tbl(void);
extern void eara_pass_perf_first_hint(int enable);
extern void (*eara_thrm_gblock_bypass_fp)(int pid, int bypass);
extern void (*eara_thrm_frame_start_fp)(int pid,
	int cpu_time, int vpu_time, int mdla_time,
	int cpu_cap, int vpu_cap, int mdla_cap,
	int queuefps, unsigned long long q2q_time,
	int AI_cross_vpu, int AI_cross_mdla, int AI_bg_vpu,
	int AI_bg_mdla, ktime_t cur_time);
extern void (*eara_thrm_enqueue_end_fp)(int pid, int gpu_time, int gpu_freq,
	unsigned long long enq);
extern struct mt_gpufreq_power_table_info *pass_gpu_table_to_eara(void);
extern unsigned int mt_gpufreq_get_dvfs_table_num(void);

#endif
