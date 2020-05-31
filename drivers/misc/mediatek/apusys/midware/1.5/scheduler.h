/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APUSYS_SCHEDULER_H__
#define __APUSYS_SCHEDULER_H__

#include <linux/types.h>
#include <linux/mutex.h>
#include "cmd_parser.h"

#ifdef CONFIG_MTK_APUSYS_RT_SUPPORT
#define preemption_support (1)
#else
#define preemption_support (0)
#endif


int apusys_sched_add_cmd(struct apusys_cmd *cmd);
int apusys_sched_wait_cmd(struct apusys_cmd *cmd);
int apusys_sched_del_cmd(struct apusys_cmd *cmd);
int apusys_sched_pause(void);
int apusys_sched_restart(void);
int apusys_sched_init(void);
int apusys_sched_destroy(void);

#endif
