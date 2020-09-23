/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef _MTK_PIDMAP_H
#define _MTK_PIDMAP_H

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

#if IS_ENABLED(CONFIG_MTK_PID_MAP)
extern void get_pidmap_aee_buffer(unsigned long *vaddr, unsigned long *size);
#else
static inline void get_pidmap_aee_buffer(unsigned long *vaddr,
					 unsigned long *size)
{
	/* return valid buffer size */
	if (size)
		*size = 0;
}
#endif

#endif /* _MTK_PIDMAP_H */

