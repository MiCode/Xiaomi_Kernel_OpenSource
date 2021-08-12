// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __FPSGO_THERMAL_AWARE_H__
#define __FPSGO_THERMAL_AWARE_H__

void __init thrm_aware_init(struct kobject *dir_kobj, int cpu);
void __exit thrm_aware_exit(void);
void thrm_aware_frame_start(int pid, int perf_hint);
void thrm_aware_switch(void);
void thrm_aware_stop(void);

#if IS_ENABLED(CONFIG_MTK_CORE_CTL)
extern int core_ctl_set_limit_cpus(unsigned int cid, unsigned int min,
	unsigned int max);
#endif

#if IS_ENABLED(CONFIG_MTK_CORE_PAUSE)
extern int sched_pause_cpu(int val);
extern int sched_resume_cpu(int val);
#endif

#endif
