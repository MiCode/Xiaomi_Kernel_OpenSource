/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef CPUQOS_V3_H
#define CPUQOS_V3_H
#include <linux/ioctl.h>

#if IS_ENABLED(CONFIG_MTK_SLBC)
extern u32 slbc_sram_read(u32 offset);
extern void slbc_sram_write(u32 offset, u32 val);
#endif

struct _CPUQOS_V3_PACKAGE {
	__u32 mode;
	__u32 pid;
	__u32 set_task;
	__u32 group_id;
	__u32 set_group;
};

#define CPUQOS_V3_SET_CPUQOS_MODE		_IOW('g', 14, struct _CPUQOS_V3_PACKAGE)
#define CPUQOS_V3_SET_CT_TASK			_IOW('g', 15, struct _CPUQOS_V3_PACKAGE)
#define CPUQOS_V3_SET_CT_GROUP			_IOW('g', 16, struct _CPUQOS_V3_PACKAGE)

#endif

