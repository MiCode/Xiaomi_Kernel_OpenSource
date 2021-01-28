/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
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
