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
	unsigned long *caps;
	struct cpumask cpus;
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

extern int pd_freq_to_opp(int cpu, unsigned long freq);
extern unsigned long pd_get_opp_capacity(int cpu, int opp);
#endif

#endif /* __CPUFREQ_H__ */
