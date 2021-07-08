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
#if IS_ENABLED(CONFIG_MTK_THERMAL_INTERFACE)
extern int set_cpu_active_bitmask(int mask);
#endif
#endif

#if IS_ENABLED(CONFIG_MTK_SCHED_BIG_TASK_ROTATE)
extern void task_rotate_init(void);
extern void check_for_migration(struct task_struct *p);
#endif

#endif
