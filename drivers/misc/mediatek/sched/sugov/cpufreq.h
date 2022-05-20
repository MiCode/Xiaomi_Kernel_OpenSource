/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __CPUFREQ_H__
#define __CPUFREQ_H__
#include <linux/proc_fs.h>

#define MAX_CAP_ENTRYIES 168

#define DVFS_TBL_BASE_PHYS 0x0011BC00
#define SRAM_REDZONE 0x55AA55AAAA55AA55
#define CAPACITY_TBL_OFFSET 0xFA0
#define CAPACITY_TBL_SIZE 0x100
#define CAPACITY_ENTRY_SIZE 0x2

struct mtk_em_perf_state {
	unsigned long freq;
	unsigned long capacity;
	unsigned long pwr_eff;
};

struct pd_capacity_info {
	int nr_caps;
	/* table[0].freq => the max freq.
	 * table[0].capacity => the max capacity.
	 */
	struct mtk_em_perf_state *table;
	struct cpumask cpus;

	// for util mapping in O(1)
	int nr_util_opp_map;
	int *util_opp_map;

	// for freq mapping in O(1)
	int freq_opp_shift;
	int nr_freq_opp_map;
	int *freq_opp_map;
};

struct sugov_tunables {
	struct gov_attr_set	attr_set;
	unsigned int		up_rate_limit_us;
	unsigned int		down_rate_limit_us;
};

struct sugov_policy {
	struct cpufreq_policy	*policy;

	struct sugov_tunables	*tunables;
	struct list_head	tunables_hook;

	raw_spinlock_t		update_lock;	/* For shared policies */
	u64			last_freq_update_time;
	s64			min_rate_limit_ns;
	s64			up_rate_delay_ns;
	s64			down_rate_delay_ns;
	unsigned int		next_freq;
	unsigned int		cached_raw_freq;

	/* The next fields are only needed if fast switch cannot be used: */
	struct			irq_work irq_work;
	struct			kthread_work work;
	struct			mutex work_lock;
	struct			kthread_worker worker;
	struct task_struct	*thread;
	bool			work_in_progress;

	bool			limits_changed;
	bool			need_freq_update;
};

#if IS_ENABLED(CONFIG_MTK_OPP_CAP_INFO)
int init_opp_cap_info(struct proc_dir_entry *dir);
void clear_opp_cap_info(void);
extern unsigned long pd_get_opp_capacity(int cpu, int opp);
extern unsigned long pd_get_freq_util(int cpu, unsigned long freq);
extern unsigned long pd_get_freq_opp(int cpu, unsigned long freq);
extern unsigned long pd_get_freq_pwr_eff(int cpu, unsigned long freq);
extern unsigned long pd_get_util_pwr_eff(int cpu, unsigned long util);
extern unsigned long pd_get_util_opp(int cpu, unsigned long util);
extern unsigned long pd_get_opp_pwr_eff(int cpu, int opp);
extern unsigned int pd_get_cpu_opp(int cpu);
#if IS_ENABLED(CONFIG_NONLINEAR_FREQ_CTL)
void mtk_arch_set_freq_scale(void *data, const struct cpumask *cpus,
				unsigned long freq, unsigned long max, unsigned long *scale);

extern int set_sched_capacity_margin_dvfs(unsigned int capacity_margin);
extern unsigned int get_sched_capacity_margin_dvfs(void);
#endif
#endif
extern unsigned int get_nr_gears(void);
DECLARE_PER_CPU(unsigned int, gear_id);
#endif /* __CPUFREQ_H__ */
