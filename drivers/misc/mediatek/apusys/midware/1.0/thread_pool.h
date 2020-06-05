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

#ifndef __APUSYS_THREAD_POOL_H__
#define __APUSYS_THREAD_POOL_H__

enum {
	APUSYS_THREAD_STATUS_IDLE,
	APUSYS_THREAD_STATUS_BUSY,

	APUSYS_THREAD_STATUS_MAX,
};

#define APUSYS_THD_TASK_FILE_PATH "/dev/stune/low_latency/tasks"

typedef int (*routine_func)(void *, void *);

/* for dump thread status */
void thread_pool_dump(void);

int thread_pool_trigger(void *cmd, void *dev_info);
void thread_pool_set_group(void);

int thread_pool_add_once(void);
int thread_pool_delete(int num);
int thread_pool_init(routine_func func_ptr);
int thread_pool_destroy(void);

#endif
