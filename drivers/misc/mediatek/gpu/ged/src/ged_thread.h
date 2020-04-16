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

#ifndef __GED_THREAD_H__
#define __GED_THREAD_H__

#include "ged_type.h"

typedef void *GED_THREAD_HANDLE;

typedef void (*GED_THREAD_FUNC)(void *);

GED_ERROR ged_thread_create(GED_THREAD_HANDLE *phThread, const char *szThreadName, GED_THREAD_FUNC pFunc, void *pvData);

GED_ERROR ged_thread_destroy(GED_THREAD_HANDLE hThread);

#endif
