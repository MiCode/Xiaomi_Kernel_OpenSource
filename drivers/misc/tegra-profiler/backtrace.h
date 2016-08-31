/*
 * drivers/misc/tegra-profiler/backtrace.h
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef __QUADD_BACKTRACE_H
#define __QUADD_BACKTRACE_H

#include <linux/mm.h>

#define QUADD_MAX_STACK_DEPTH		64

struct quadd_callchain {
	int nr;

	union {
		u32 ip_32[QUADD_MAX_STACK_DEPTH];
		u64 ip_64[QUADD_MAX_STACK_DEPTH];
	};

	int cs_64;

	unsigned int unw_method;
	unsigned int unw_rc;

	unsigned long curr_sp;
	unsigned long curr_fp;
};

struct quadd_ctx;
struct pt_regs;

unsigned int
quadd_get_user_callchain(struct pt_regs *regs,
			 struct quadd_callchain *cc_data,
			 struct quadd_ctx *ctx,
			 struct task_struct *task);

int
quadd_callchain_store(struct quadd_callchain *cc,
		      unsigned long ip);

unsigned long
quadd_user_stack_pointer(struct pt_regs *regs);

unsigned long
quadd_user_link_register(struct pt_regs *regs);

static inline int
is_vma_addr(unsigned long addr, struct vm_area_struct *vma,
	    unsigned long nbytes)
{
	return	vma &&
		addr >= vma->vm_start &&
		addr < vma->vm_end - nbytes;
}


#endif  /* __QUADD_BACKTRACE_H */
