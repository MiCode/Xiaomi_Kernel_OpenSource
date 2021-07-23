/*
 * Copyright (C) 2018 MediaTek Inc.
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

#ifndef _MTK_PIDMAP_H
#define _MTK_PIDMAP_H

#if defined(CONFIG_MTK_PID_MAP)

#include <linux/types.h>
#include <linux/sched.h>

/* buffer size */
#define PIDMAP_AEE_BUF_SIZE       (512 * 1024)
#define PIDMAP_PROC_CMD_BUF_SIZE  (1)

/* raw file format, unit: bytes */
#define PIDMAP_TGID_SIZE          (sizeof(uint16_t))

/* mtk_pidmap_struct */
#define PIDMAP_ENTRY_SIZE         (16)
#define PIDMAP_ENTRY_CNT	\
	(PIDMAP_AEE_BUF_SIZE / PIDMAP_ENTRY_SIZE)
#define PIDMAP_TASKNAME_SIZE \
	(PIDMAP_ENTRY_SIZE - PIDMAP_TGID_SIZE)

enum PIDMAP_PROC_MODE {
	PIDMAP_PROC_DUMP_RAW      = 0,
	PIDMAP_PROC_DUMP_READABLE = 1,
};

#ifndef USER_BUILD_KERNEL
#define PIDMAP_PROC_PERM          (0660)
#else
#define PIDMAP_PROC_PERM          (0440)
#endif

void mtk_pidmap_update(struct task_struct *task);

#else /* !CONFIG_MTK_PID_MAP */

#define mtk_pidmap_update(task)

#endif /* CONFIG_MTK_PID_MAP */

#endif /* _MTK_PIDMAP_H */

