/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk.h>

#include "clk-voter.h"

static int voter_clk_set_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long parent_rate)
{
	int ret = 0;
	struct clk_voter *v = to_clk_voter(hw);
	unsigned long cur_rate, new_rate, other_rate = 0;

	if (v->is_branch)
		return ret;

	if (v->enabled) {
		struct clk_hw *parent = clk_hw_get_parent(hw);

		/*
		 * Get the aggregate rate without this clock's vote and update
		 * if the new rate is different than the current rate.
		 */
		other_rate = clk_aggregate_rate(hw, parent->core);

		cur_rate = max(other_rate, clk_get_rate(hw->clk));
		new_rate = max(other_rate, rate);

		if (new_rate != cur_rate) {
			ret = clk_set_rate(parent->clk, new_rate);
			if (ret)
				return ret;
		}
	}
	v->rate =  rate;

	return ret;
}

static int voter_clk_prepare(struct clk_hw *hw)
{
	int ret = 0;
	unsigned long cur_rate;
	struct clk_hw *parent;
	struct clk_voter *v = to_clk_voter(hw);

	parent = clk_hw_get_parent(hw);

	if (v->is_branch) {
		v->enabled = true;
		return ret;
	}

	/*
	 * Increase the rate if this clock is voting for a higher rate
	 * than the current rate.
	 */
	cur_rate = clk_aggregate_rate(hw, parent->core);

	if (v->rate > cur_rate) {
		ret = clk_set_rate(parent->clk, v->rate);
		if (ret)
			return ret;
	}
	v->enabled = true;

	return ret;
}

static void voter_clk_unprepare(struct clk_hw *hw)
{
	unsigned long cur_rate, new_rate;
	struct clk_hw *parent;
	struct clk_voter *v = to_clk_voter(hw);


	parent = clk_hw_get_parent(hw);

	/*
	 * Decrease the rate if this clock was the only one voting for
	 * the highest rate.
	 */
	v->enabled = false;
	if (v->is_branch)
		return;

	new_rate = clk_aggregate_rate(hw, parent->core);
	cur_rate = max(new_rate, v->rate);

	if (new_rate < cur_rate)
		clk_set_rate(parent->clk, new_rate);
}

static int voter_clk_is_enabled(struct clk_hw *hw)
{
	struct clk_voter *v = to_clk_voter(hw);

	return v->enabled;
}

static long voter_clk_round_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long *parent_rate)
{
	return clk_hw_round_rate(clk_hw_get_parent(hw), rate);
}

static unsigned long voter_clk_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct clk_voter *v = to_clk_voter(hw);

	return v->rate;
}

int voter_clk_handoff(struct clk_hw *hw)
{
	struct clk_voter *v = to_clk_voter(hw);

	v->enabled = true;

	return 0;
}
EXPORT_SYMBOL(voter_clk_handoff);

struct clk_ops clk_ops_voter = {
	.prepare = voter_clk_prepare,
	.unprepare = voter_clk_unprepare,
	.set_rate = voter_clk_set_rate,
	.is_enabled = voter_clk_is_enabled,
	.round_rate = voter_clk_round_rate,
	.recalc_rate = voter_clk_recalc_rate,
};
