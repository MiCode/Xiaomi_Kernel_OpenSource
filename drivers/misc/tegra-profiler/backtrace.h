/*
 * drivers/misc/tegra-profiler/backtrace.h
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/types.h>

#define QUADD_MAX_STACK_DEPTH		64

struct quadd_callchain {
	int nr;
	u32 callchain[QUADD_MAX_STACK_DEPTH];
};

unsigned int
quadd_get_user_callchain(struct pt_regs *regs,
			 struct quadd_callchain *callchain_data);


#endif  /* __QUADD_BACKTRACE_H */
