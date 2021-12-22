/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef GBE_USEDEXT_H
#define GBE_USEDEXT_H
#include "fpsgo_base.h"

extern int gbe2xgf_get_dep_list_num(int pid, unsigned long long bufID);
extern int gbe2xgf_get_dep_list(int pid, int count,
	struct gbe_runtime *arr, unsigned long long bufID);

#endif

