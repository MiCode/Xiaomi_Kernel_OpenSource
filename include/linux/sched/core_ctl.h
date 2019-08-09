/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016, 2019, The Linux Foundation. All rights reserved.
 */

#ifndef __CORE_CTL_H
#define __CORE_CTL_H

#define MAX_CPUS_PER_CLUSTER 6
#define MAX_CLUSTERS 3

struct core_ctl_notif_data {
	unsigned int nr_big;
	unsigned int coloc_load_pct;
	unsigned int ta_util_pct[MAX_CLUSTERS];
	unsigned int cur_cap_pct[MAX_CLUSTERS];
};

#ifdef CONFIG_SCHED_CORE_CTL
void core_ctl_check(u64 wallclock);
int core_ctl_set_boost(bool boost);
void core_ctl_notifier_register(struct notifier_block *n);
void core_ctl_notifier_unregister(struct notifier_block *n);
#else
static inline void core_ctl_check(u64 wallclock) {}
static inline int core_ctl_set_boost(bool boost)
{
	return 0;
}
static inline void core_ctl_notifier_register(struct notifier_block *n) {}
static inline void core_ctl_notifier_unregister(struct notifier_block *n) {}
#endif
#endif
