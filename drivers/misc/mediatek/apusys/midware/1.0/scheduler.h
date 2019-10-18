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

/* total 32 priority, 16 for user, 16 reserved for system */
enum {
	/* user used */
	APUSYS_PRIORITY_NORMAL = 0x1,
	APUSYS_PRIORITY_HIGH = 0x2,
	APUSYS_PRIORITY_ULTRA = 0x0,

	/* system used */
	APUSYS_PRIORITY_SYSTEM_0 = 0x10,
	APUSYS_PRIORITY_SYSTEM_1 = 0x11,
	APUSYS_PRIORITY_SYSTEM_2 = 0x12,
	APUSYS_PRIORITY_SYSTEM_3 = 0x13,
	APUSYS_PRIORITY_SYSTEM_4 = 0x14,

	APUSYS_PRIORITY_MAX = 0x20,
};
int apusys_sched_cmd_abort(struct apusys_cmd *cmd);
int apusys_sched_wait_cmd(struct apusys_cmd *cmd);
int apusys_sched_add_list(struct apusys_cmd *cmd);
int apusys_sched_init(void);
int apusys_sched_destroy(void);

#endif
