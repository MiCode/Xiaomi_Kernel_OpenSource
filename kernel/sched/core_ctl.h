/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 */

#ifndef __CORE_CTL_H
#define __CORE_CTL_H

#ifdef CONFIG_SCHED_CORE_CTL
void core_ctl_check(u64 wallclock);
int core_ctl_set_boost(bool boost);
#else
static inline void core_ctl_check(u64 wallclock) {}
static inline int core_ctl_set_boost(bool boost)
{
	return 0;
}
#endif
#endif
