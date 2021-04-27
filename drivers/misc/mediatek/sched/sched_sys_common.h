/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef SCHED_SYS_COMMON_H
#define SCHED_SYS_COMMON_H
#include <linux/module.h>

extern int init_sched_common_sysfs(void);
extern void cleanup_sched_common_sysfs(void);

#endif
