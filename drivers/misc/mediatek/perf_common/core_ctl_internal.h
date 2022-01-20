/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

enum mt_cpu_dvfs_id {
	MT_CPU_DVFS_LL,
	MT_CPU_DVFS_L,
	MT_CPU_DVFS_B,
	MT_CPU_DVFS_CCI,

	NR_MT_CPU_DVFS,
};

extern void set_overutil_threshold(int index, int val);
extern int get_immediate_tslvts1_1_wrap(void); /* CPU7 TS */
extern int sched_get_nr_overutil_avg(int cluster_id,
				     int *l_avg,
				     int *h_avg,
				     int *sum_nr_overutil_l,
				     int *sum_nr_overutil_h,
				     int *max_nr);
extern int update_userlimit_cpu_freq(int kicker,
				     int num_cluster,
				     struct ppm_limit_data *freq_limit);
extern int sched_isolate_cpu(int cpu);
extern int sched_unisolate_cpu(int cpu);
extern int sched_unisolate_cpu_unlocked(int cpu);

#ifdef CONFIG_MTK_CPU_FREQ
extern unsigned int mt_cpufreq_get_freq_by_idx(enum mt_cpu_dvfs_id id, int idx);
extern unsigned int mt_cpufreq_get_cur_freq(enum mt_cpu_dvfs_id id);
extern unsigned int mt_cpufreq_find_close_freq(unsigned int cluster_id, unsigned int freq);
extern int mt_cpufreq_set_by_schedule_load_cluster(unsigned int cluster_id, unsigned int freq);
#else
static unsigned int mt_cpufreq_get_freq_by_idx(enum mt_cpu_dvfs_id id, int idx) {return 0; }
static unsigned int mt_cpufreq_get_cur_freq(enum mt_cpu_dvfs_id id) {return 0; }
static unsigned int mt_cpufreq_find_close_freq(unsigned int cluster_id, unsigned int freq)
{
	return 0;
}
static int mt_cpufreq_set_by_schedule_load_cluster(unsigned int cluster_id, unsigned int freq)
{
	return 0;
}
#endif
