/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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
