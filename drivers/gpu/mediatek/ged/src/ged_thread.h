// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __GED_THREAD_H__
#define __GED_THREAD_H__

#include "ged_type.h"

typedef void *GED_THREAD_HANDLE;

typedef void (*GED_THREAD_FUNC)(void *);

GED_ERROR ged_thread_create(GED_THREAD_HANDLE *phThread, const char *szThreadName, GED_THREAD_FUNC pFunc, void *pvData);

GED_ERROR ged_thread_destroy(GED_THREAD_HANDLE hThread);

#endif
