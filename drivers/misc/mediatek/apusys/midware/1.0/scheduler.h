/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
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
void apusys_sched_set_group(void);
int apusys_sched_init(void);
int apusys_sched_destroy(void);

#endif
