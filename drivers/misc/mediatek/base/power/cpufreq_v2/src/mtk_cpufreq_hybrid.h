/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MTK_CPUFREQ_HYBRID__
#define __MTK_CPUFREQ_HYBRID__

/* #define CPUDVFS_TIME_PROFILE 1 */

struct cpu_dvfs_log {
	unsigned int time_stamp_l_log:32;
	unsigned int time_stamp_h_log:32;

	struct {
		unsigned int turbo_bit:2;
		unsigned int cluster_en:2;
		unsigned int limit:4;
		unsigned int base:4;
		unsigned int opp_idx_log:4;
		unsigned int sche_idx_log:5;
		unsigned int wfi_idx_log:4;
		unsigned int padd:7;
	} cluster_opp_cfg[NR_MT_CPU_DVFS];
};

struct cpu_dvfs_log_box {
	unsigned long long time_stamp;
	struct {
		unsigned int freq_idx;
		unsigned int limit_idx;
		unsigned int base_idx;
	} cluster_opp_cfg[NR_MT_CPU_DVFS];
};

/* Parameter Enum */
enum cpu_dvfs_ipi_type {
	IPI_DVFS_INIT_PTBL,
	IPI_DVFS_INIT,
	IPI_SET_CLUSTER_ON_OFF,
#if 0
	IPI_SET_VOLT,
	IPI_SET_FREQ,
	IPI_GET_VOLT,
	IPI_GET_FREQ,
#endif
	IPI_TURBO_MODE,
	IPI_TIME_PROFILE,

	NR_DVFS_IPI,
};

struct cdvfs_data {
	unsigned int cmd;
	union {
		struct {
			unsigned int arg[3];
		} set_fv;
	} u;
};

int cpuhvfs_module_init(void);
int cpuhvfs_set_init_sta(void);
int cpuhvfs_set_turbo_scale(unsigned int turbo_f, unsigned int turbo_v);
int cpuhvfs_set_min_max(int cluster_id, int base, int limit);
int cpuhvfs_set_cluster_on_off(int cluster_id, int state);
int cpuhvfs_set_dvfs(int cluster_id, unsigned int freq);
int cpuhvfs_set_volt(int cluster_id, unsigned int volt);
int cpuhvfs_set_freq(int cluster_id, unsigned int freq);
int cpuhvfs_get_cur_volt(int cluster_id);
int cpuhvfs_get_volt(int buck_id);
int cpuhvfs_get_freq(int pll_id);
int cpuhvfs_set_turbo_mode(int turbo_mode, int freq_step, int volt_step);
int cpuhvfs_get_time_profile(void);
int cpuhvfs_set_dvfs_stress(unsigned int en);
int cpuhvfs_get_sched_dvfs_disable(void);
int cpuhvfs_set_sched_dvfs_disable(unsigned int disable);
int cpuhvfs_set_turbo_disable(unsigned int disable);
int cpuhvfs_get_cur_dvfs_freq_idx(int cluster_id);
#if 0
int cpuhvfs_set_cpu_load_freq(unsigned int cpu,
	enum cpu_dvfs_sched_type state, unsigned int freq);
#else
int cpuhvfs_set_cluster_load_freq(enum mt_cpu_dvfs_id id, unsigned int freq);
#endif
int cpuhvfs_set_iccs_freq(enum mt_cpu_dvfs_id id, unsigned int freq);
int cpuhvfs_update_volt(unsigned int cluster_id,
	unsigned int *volt_tbl, char nr_volt_tbl);
int dvfs_to_sspm_command(u32 cmd, struct cdvfs_data *cdvfs_d);

#endif	/* __MTK_CPUFREQ_HYBRID__ */
