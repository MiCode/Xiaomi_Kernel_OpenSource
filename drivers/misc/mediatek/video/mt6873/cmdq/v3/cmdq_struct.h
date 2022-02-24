// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __CMDQ_STRUCT_H__
#define __CMDQ_STRUCT_H__

#include <linux/list.h>
#include <linux/spinlock.h>

struct cmdqFileNodeStruct {
	pid_t userPID;
	pid_t userTGID;
	struct list_head taskList;
	spinlock_t nodeLock;
};

#endif				/* __CMDQ_STRUCT_H__ */
