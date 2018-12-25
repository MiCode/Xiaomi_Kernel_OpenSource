/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MTK_CPUFREQ_API_H__
#define __MTK_CPUFREQ_API_H__

#define VOLT_PRECHANGE 0
#define VOLT_POSTCHANGE 1
#define VOLT_UP 0
#define VOLT_DOWN 1

#include \
"../../../../../base/power/cpufreq_v1/src/mach/mt6765/mtk_cpufreq_config.h"

enum cpu_dvfs_sched_type {
	SCHE_INVALID,
	SCHE_VALID,
	SCHE_ONESHOT,

	NUM_SCHE_TYPE
};

/* Schedule Assist Input */
extern int mt_cpufreq_set_by_wfi_load_cluster(unsigned int cluster_id,
	unsigned int freq);
extern int mt_cpufreq_set_by_schedule_load_cluster(unsigned int cluster_id,
	unsigned int freq);
extern unsigned int mt_cpufreq_find_close_freq(unsigned int cluster_id,
	unsigned int freq);
extern int mt_cpufreq_get_sched_enable(void);

/* ICCS */
extern int mt_cpufreq_set_iccs_frequency_by_cluster(int en,
	unsigned int cluster_id, unsigned int freq);

/* PTP-OD */
extern unsigned int mt_cpufreq_get_freq_by_idx(enum mt_cpu_dvfs_id id, int idx);
extern unsigned int mt_cpufreq_get_volt_by_idx(enum mt_cpu_dvfs_id id, int idx);
extern unsigned int mt_cpufreq_get_cur_volt(enum mt_cpu_dvfs_id id);

typedef void (*mt_cpufreq_set_ptbl_funcPTP)(enum mt_cpu_dvfs_id id,
	int restore);
extern void mt_cpufreq_set_ptbl_registerCB(mt_cpufreq_set_ptbl_funcPTP pCB);

extern int mt_cpufreq_update_volt(enum mt_cpu_dvfs_id id,
	unsigned int *volt_tbl, int nr_volt_tbl);
extern unsigned int mt_cpufreq_get_cur_volt(enum mt_cpu_dvfs_id id);
extern unsigned int mt_cpufreq_get_cur_freq(enum mt_cpu_dvfs_id id);

extern void notify_cpu_volt_sampler(enum mt_cpu_dvfs_id id,
	unsigned int volt, int up, int event);
typedef void (*cpuVoltsampler_func) (enum mt_cpu_dvfs_id,
	unsigned int mv, int up, int event);
extern void mt_cpufreq_setvolt_registerCB(cpuVoltsampler_func pCB);

/* PPB */
extern int mt_cpufreq_get_ppb_state(void);

/* PPM */
extern unsigned int mt_cpufreq_get_cur_phy_freq(enum mt_cpu_dvfs_id id);
extern unsigned int mt_cpufreq_get_cur_phy_freq_no_lock(enum mt_cpu_dvfs_id id);
extern void mt_cpufreq_setvolt_ocp_registerCB(cpuVoltsampler_func pCB);

/* Upower */
extern unsigned int mt_cpufreq_get_cpu_level(void);

/* CPI */
extern unsigned int mt_cpufreq_get_cur_freq_idx(unsigned int cluster_id);
extern unsigned int
mt_cpufreq_get_cur_phy_freq_idx_no_lock(unsigned int cluster_id);

typedef void (*cpuFreqsampler_func)(unsigned int cluster_id, unsigned int freq);
extern void mt_cpufreq_set_governor_freq_registerCB(cpuFreqsampler_func pCB);

/* CPUFREQ */
extern void aee_record_cpufreq_cb(unsigned int step);

#endif	/* __MTK_CPUFREQ_API_H__ */
