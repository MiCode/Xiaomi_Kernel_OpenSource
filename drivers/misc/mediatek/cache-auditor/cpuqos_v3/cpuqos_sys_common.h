/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef CPUQOS_SYS_COMMON_H
#define CPUQOS_SYS_COMMON_H
#include <linux/module.h>

extern int init_cpuqos_common_sysfs(void);
extern void cleanup_cpuqos_common_sysfs(void);

extern struct kobj_attribute set_ct_group_ct_attr;
extern struct kobj_attribute set_ct_group_nct_attr;
extern struct kobj_attribute set_ct_task_ct_attr;
extern struct kobj_attribute set_ct_task_nct_attr;
extern struct kobj_attribute set_cpuqos_mode_attr;

extern int set_cpuqos_mode(int mode);
extern int set_ct_task(int pid, bool set);
extern int set_ct_group(int group_id, bool set);


#endif
