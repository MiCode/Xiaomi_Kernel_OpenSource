/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __CORE_CTL_H
#define __CORE_CTL_H

extern void core_ctl_tick(u64 wallclock);
extern int core_ctl_set_min_cpus(unsigned int cid, unsigned int min);
extern int core_ctl_set_max_cpus(unsigned int cid, unsigned int max);
extern int core_ctl_set_offline_throttle_ms(unsigned int cid,
					     unsigned int throttle_ms);
extern int core_ctl_set_limit_cpus(unsigned int cid,
				   unsigned int min,
				   unsigned int max);
extern int core_ctl_set_not_preferred(int cid, int cpu, bool enable);
extern int core_ctl_set_boost(bool boost);
extern int core_ctl_set_btask_up_thresh(int cid, unsigned int val);
extern int core_ctl_set_cpu_tj_degree(int cid, unsigned int degree);
extern int core_ctl_set_cpu_tj_btask_thresh(int cid, unsigned int val);
#endif
