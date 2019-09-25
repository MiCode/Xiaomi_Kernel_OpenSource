/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017, 2019, The Linux Foundation. All rights reserved.
 */

#ifndef __QCOM_CLK_VOTER_H__
#define __QCOM_CLK_VOTER_H__

#include <linux/clk-provider.h>
#include <linux/platform_device.h>

struct clk_voter {
	int is_branch;
	bool enabled;
	struct clk_hw hw;
	unsigned long rate;
};

extern const struct clk_ops clk_ops_voter;

#define to_clk_voter(_hw) container_of(_hw, struct clk_voter, hw)

#define __DEFINE_CLK_VOTER(clk_name, _parent_name, _default_rate, _is_branch) \
	struct clk_voter clk_name = {					 \
		.is_branch = (_is_branch),				  \
		.rate = _default_rate,					   \
		.hw.init = &(struct clk_init_data){			   \
			.ops = &clk_ops_voter,				   \
			.name = #clk_name,				   \
			.flags = CLK_ENABLE_HAND_OFF,			   \
			.parent_names = (const char *[]){ #_parent_name }, \
			.num_parents = 1,				   \
		},							   \
	}

#define DEFINE_CLK_VOTER(clk_name, _parent_name, _default_rate) \
	 __DEFINE_CLK_VOTER(clk_name, _parent_name, _default_rate, 0)

#define DEFINE_CLK_BRANCH_VOTER(clk_name, _parent_name) \
	 __DEFINE_CLK_VOTER(clk_name, _parent_name, 1000, 1)

int voter_clk_handoff(struct clk_hw *hw);

#endif
