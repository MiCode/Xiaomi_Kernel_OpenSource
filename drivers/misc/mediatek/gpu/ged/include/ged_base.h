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

#ifndef __GED_BASE_H__
#define __GED_BASE_H__

#include <linux/compiler.h>
#include "ged_type.h"

#ifdef GED_DEBUG
#define GED_LOGI(...) pr_debug("GED:" __VA_ARGS__)
#else
#define GED_LOGI(...)
#endif
#define GED_LOGE(...) pr_debug("GED:" __VA_ARGS__)
#define GED_CONTAINER_OF(ptr, type, member) \
	((type *)(((char *)ptr) - offsetof(type, member)))

unsigned long ged_copy_to_user(void __user *pvTo,
	const void *pvFrom, unsigned long ulBytes);

unsigned long ged_copy_from_user(void *pvTo,
	const void __user *pvFrom, unsigned long ulBytes);

void *ged_alloc(int i32Size);

void *ged_alloc_atomic(int i32Size);

void ged_free(void *pvBuf, int i32Size);

long ged_get_pid(void);

unsigned long long ged_get_time(void);

struct GED_FILE_PRIVATE_BASE {
	void (*free_func)(void *f);
};

#endif
