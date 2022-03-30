/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef _HELPER_H_
#define _HELPER_H_

#include "mkp_api.h"
#include "mkp.h"
#include "debug.h"
#include "mkp_demo.h"

int is_vmalloc_or_module_addr(/*unsigned long x */const void *x);
int mkp_set_mapping_xxx_helper(unsigned long addr, int nr_pages, uint32_t policy,
	int (*set_memory)(uint32_t policy, uint32_t handle));

#endif
