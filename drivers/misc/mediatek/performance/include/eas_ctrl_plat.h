/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef _EAS_CTRL_PLAT_H_
#define _EAS_CTRL_PLAT_H_

/* control migration cost */
extern unsigned int sysctl_sched_migration_cost;
extern unsigned int sysctl_sched_sync_hint_enable;

extern int schedutil_set_down_rate_limit_us(int cpu,
	unsigned int rate_limit_us);
extern int schedutil_set_up_rate_limit_us(int cpu,
	unsigned int rate_limit_us);

/* EAS */
extern int uclamp_min_pct_for_perf_idx(int group_idx, int min_value);
extern void set_sched_rotation_enable(bool enable);

#endif /* _EAS_CTRL_PLAT_H_ */
