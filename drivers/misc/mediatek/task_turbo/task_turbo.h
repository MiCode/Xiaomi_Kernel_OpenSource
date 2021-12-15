/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef _TASK_TURBO_H_
#define _TASK_TURBO_H_

enum {
	START_INHERIT   = -1,
	RWSEM_INHERIT   = 0,
	BINDER_INHERIT,
	END_INHERIT,
};

extern int sched_set_cpuprefer(pid_t pid, unsigned int prefer_type);
#endif
