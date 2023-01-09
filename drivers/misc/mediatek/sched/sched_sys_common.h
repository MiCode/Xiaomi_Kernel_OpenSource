/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef SCHED_SYS_COMMON_H
#define SCHED_SYS_COMMON_H
#include <linux/module.h>

extern int init_sched_common_sysfs(void);
extern void cleanup_sched_common_sysfs(void);

#if IS_ENABLED(CONFIG_MTK_CORE_PAUSE)
extern struct kobj_attribute sched_core_pause_info_attr;
extern int sched_pause_cpu(int cpu);
extern int sched_resume_cpu(int cpu);
extern int resume_cpus(struct cpumask *cpus);
#if IS_ENABLED(CONFIG_MTK_THERMAL_INTERFACE)
extern int set_cpu_active_bitmask(int mask);
#endif
#endif

#if IS_ENABLED(CONFIG_MTK_SCHED_BIG_TASK_ROTATE)
extern void task_rotate_init(void);
extern void check_for_migration(struct task_struct *p);
#endif
extern unsigned long get_turn_point_freq(int gearid);
extern int set_turn_point_freq(int gearid, unsigned long freq);
extern int set_target_margin(int gearid, int margin);
extern unsigned int get_target_margin(int gearid);
extern struct kobj_attribute sched_turn_point_freq_attr;
extern struct kobj_attribute sched_target_margin_attr;
extern struct kobj_attribute sched_util_est_ctrl;
extern int set_util_est_ctrl(bool enable);

#endif
