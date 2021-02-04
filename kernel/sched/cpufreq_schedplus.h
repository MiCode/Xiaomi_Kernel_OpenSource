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


enum mt_cpu_dvfs_id {
	MT_CPU_DVFS_LL,
	MT_CPU_DVFS_L,
	MT_CPU_DVFS_B,
	MT_CPU_DVFS_CCI,

	NR_MT_CPU_DVFS,
};

enum mt_dvfs_debug_id {
	DEBUG_FREQ_CLUSTER0,
	DEBUG_FREQ_CLUSTER1,
	DEBUG_FREQ_CLUSTER2,
	DEBUG_FREQ_ALL,
	DEBUG_FREQ_DISABLED = 100,
};

enum throttle_type {
	DVFS_THROTTLE_UP,
	DVFS_THROTTLE_DOWN,
};

/*
 * extern int mt_cpufreq_set_by_schedule_load(unsigned int cpu,
 *		enum cpu_dvfs_sched_type state, unsigned int freq);
 */
#ifdef CONFIG_MTK_CPU_FREQ
extern int  mt_cpufreq_set_by_schedule_load_cluster(int cid, unsigned int freq);
extern int  mt_cpufreq_set_by_wfi_load_cluster(int cid, unsigned int freq);
extern unsigned int mt_cpufreq_find_close_freq(
			unsigned int cluster_id, unsigned int freq);
extern unsigned int mt_cpufreq_get_freq_by_idx(enum mt_cpu_dvfs_id id, int idx);
extern int mt_cpufreq_get_sched_enable(void);
extern unsigned int mt_cpufreq_get_cur_freq(enum mt_cpu_dvfs_id id);
#else
static inline int  mt_cpufreq_set_by_schedule_load_cluster(
			int cid, unsigned int freq) { return 0; }
static inline int  mt_cpufreq_set_by_wfi_load_cluster(
			int cid, unsigned int freq) { return 0; }
static inline unsigned int mt_cpufreq_find_close_freq(
		unsigned int cluster_id, unsigned int freq) { return 0; }
static inline unsigned int mt_cpufreq_get_freq_by_idx(
			enum mt_cpu_dvfs_id id, int idx) { return 0; }
static inline int mt_cpufreq_get_sched_enable(void) { return 0; }
static inline  int mt_cpufreq_get_cur_freq(
			enum mt_cpu_dvfs_id id) { return 0; };
#endif

#ifdef CONFIG_SCHED_WALT
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
extern int cpufreq_notifier_trans_sspm_walt(
		unsigned int cpu, unsigned int new_freq);
#else
static inline int cpufreq_notifier_trans_sspm_walt(
		unsigned int cpu, unsigned int new_freq) { return 0; }
#endif
#endif

#ifdef CONFIG_CPU_FREQ_GOV_SCHEDPLUS
extern unsigned long int min_boost_freq[3];
extern unsigned long int cap_min_freq[3];
extern void update_cpu_freq_quick(int cpu, int freq);
#else
static inline void update_cpu_freq_quick(int cpu, int freq) { return; }
#endif

#ifdef CONFIG_CGROUP_SCHEDTUNE
extern int schedtune_cpu_capacity_min(int cpu);
#endif
