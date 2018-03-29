/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef __CMDQ_STRUCT_H__
#define __CMDQ_STRUCT_H__

#include <linux/list.h>
#include <linux/spinlock.h>

typedef struct cmdqFileNodeStruct {
	pid_t userPID;
	pid_t userTGID;
	struct list_head taskList;
	spinlock_t nodeLock;
} cmdqFileNodeStruct;

#endif				/* __CMDQ_STRUCT_H__ */
