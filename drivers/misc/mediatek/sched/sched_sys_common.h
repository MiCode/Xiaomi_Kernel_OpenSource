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
extern struct kobj_attribute set_sched_pause_cpu_attr;
extern struct kobj_attribute set_sched_resume_cpu_attr;
#endif

#endif
