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

#define MKP_DEMO_MODULE_CASE (1)
#define MKP_DEMO_BPF_CASE (2)

/* Available helpers */
enum helper_ops {
	HELPER_MAPPING_RO = 1,
	HELPER_MAPPING_RW = 2,
	HELPER_MAPPING_NX = 3,
	HELPER_MAPPING_X = 4,
	HELPER_CLEAR_MAPPING = 5,
};

int is_module_or_bpf_addr(/*unsigned long x */const void *x);
int mkp_set_mapping_xxx_helper(unsigned long addr, int nr_pages, uint32_t policy,
		enum helper_ops ops);

#endif
