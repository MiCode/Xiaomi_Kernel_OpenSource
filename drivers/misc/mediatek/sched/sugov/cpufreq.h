/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __CPUFREQ_H__
#define __CPUFREQ_H__
 #include <linux/proc_fs.h>

#define MAX_PD_COUNT 3
#define MAX_CAP_ENTRYIES 168

#define DVFS_TBL_BASE_PHYS 0x0011BC00
#define SRAM_REDZONE 0x55AA55AAAA55AA55
#define CAPACITY_TBL_OFFSET 0xFA0
#define CAPACITY_TBL_SIZE 0x100
#define CAPACITY_ENTRY_SIZE 0x2

struct pd_capacity_info {
	int nr_caps;
	unsigned int *util_opp;
	unsigned int *util_freq;
	unsigned long *caps;
	struct cpumask cpus;
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
#if IS_ENABLED(CONFIG_NONLINEAR_FREQ_CTL)
void mtk_arch_set_freq_scale(void *data, const struct cpumask *cpus,
				unsigned long freq, unsigned long max, unsigned long *scale);

extern int set_sched_capacity_margin_dvfs(unsigned int capacity_margin);
extern unsigned int get_sched_capacity_margin_dvfs(void);
#endif
#endif

extern unsigned long pd_get_opp_capacity(int cpu, int opp);
#endif /* __CPUFREQ_H__ */
