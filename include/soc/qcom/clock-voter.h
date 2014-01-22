/* Copyright (c) 2010-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ARCH_ARM_MACH_MSM_CLOCK_VOTER_H
#define __ARCH_ARM_MACH_MSM_CLOCK_VOTER_H

#include <linux/clk/msm-clk-provider.h>

struct clk_ops;
extern struct clk_ops clk_ops_voter;

struct clk_voter {
	int is_branch;
	bool enabled;
	struct clk c;
};

static inline struct clk_voter *to_clk_voter(struct clk *clk)
{
	return container_of(clk, struct clk_voter, c);
}

#define __DEFINE_CLK_VOTER(clk_name, _parent, _default_rate, _is_branch) \
	struct clk_voter clk_name = { \
		.is_branch = (_is_branch), \
		.c = { \
			.parent = _parent, \
			.dbg_name = #clk_name, \
			.ops = &clk_ops_voter, \
			.rate = _default_rate, \
			CLK_INIT(clk_name.c), \
		}, \
	}

#define DEFINE_CLK_VOTER(clk_name, _parent, _default_rate) \
	 __DEFINE_CLK_VOTER(clk_name, _parent, _default_rate, 0)

#define DEFINE_CLK_BRANCH_VOTER(clk_name, _parent) \
	 __DEFINE_CLK_VOTER(clk_name, _parent, 1000, 1)

#endif
