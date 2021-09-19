/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef CPUQOS_SYS_COMMON_H
#define CPUQOS_SYS_COMMON_H
#include <linux/module.h>

extern int init_cpuqos_common_sysfs(void);
extern void cleanup_cpuqos_common_sysfs(void);

extern struct kobj_attribute show_cpuqos_status_attr;
extern struct kobj_attribute set_cache_size_attr;

#endif
