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

#ifndef __MIDWARE_1_0_SCHED_NORMAL_H__
#define __MIDWARE_1_0_SCHED_NORMAL_H__

#include "cmd_parser.h"
#include "cmd_format.h"

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

struct normal_queue {
	struct list_head prio[APUSYS_PRIORITY_MAX];
	unsigned long node_exist[BITS_TO_LONGS(APUSYS_PRIORITY_MAX)];
};

int normal_queue_init(int type);
void normal_queue_destroy(int type);
int normal_task_empty(int type);

int normal_task_insert(struct apusys_subcmd *sc);
int normal_task_remove(struct apusys_subcmd *sc);
int normal_task_start(struct apusys_subcmd *sc);
int normal_task_end(struct apusys_subcmd *sc);
struct apusys_subcmd *normal_task_pop(int type);

#endif /* __MIDWARE_1_0_SCHED_NORMAL_H__ */

