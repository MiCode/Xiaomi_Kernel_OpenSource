/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __EARA_THERMAL_INT_H__
#define __EARA_THERMAL_INT_H__

#include <linux/ktime.h>

extern struct ppm_cobra_data *ppm_cobra_pass_tbl(void);
extern void eara_pass_perf_first_hint(int enable);

extern struct mt_gpufreq_power_table_info *pass_gpu_table_to_eara(void);
extern unsigned int mt_gpufreq_get_dvfs_table_num(void);

extern void (*eara_thrm_gblock_bypass_fp)(int pid,
		unsigned long long bufid, int bypass);
extern void (*eara_thrm_frame_start_fp)(int pid, unsigned long long bufID,
	int cpu_time, int vpu_time, int mdla_time,
	int cpu_cap, int vpu_cap, int mdla_cap,
	int queuefps, unsigned long long q2q_time,
	int AI_cross_vpu, int AI_cross_mdla, int AI_bg_vpu,
	int AI_bg_mdla, ktime_t cur_time);
extern void (*eara_thrm_enqueue_end_fp)(int pid, unsigned long long bufID,
	int gpu_time, int gpu_freq, unsigned long long enq);

extern int get_tpcb_headroom(void);

extern void (*eara_enable_fp)(int enable);
extern void (*eara_set_tfps_diff_fp)(int max_cnt, int *pid, unsigned long long *buf_id, int *diff);
extern void (*eara_get_tfps_pair_fp)(int max_cnt, int *pid, unsigned long long *buf_id, int *tfps);
extern void (*eara_pre_active_fp)(int is_active);

/* platform */
struct thrm_pb_ratio {
	int ratio;
	int vpu_power;
	int mdla_power;
};

void eara_thrm_update_gpu_info(int *input_opp_num, int *in_max_opp_idx,
		struct mt_gpufreq_power_table_info **gpu_tbl,
		struct thrm_pb_ratio **opp_ratio);
int eara_thrm_get_vpu_core_num(void);
int eara_thrm_get_mdla_core_num(void);
int eara_thrm_vpu_opp_to_freq(int opp);
int eara_thrm_mdla_opp_to_freq(int opp);
int eara_thrm_apu_ready(void);
int eara_thrm_vpu_onoff(void);
int eara_thrm_mdla_onoff(void);
int eara_thrm_keep_little_core(void);

#endif
