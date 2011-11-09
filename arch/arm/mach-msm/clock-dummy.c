/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#include "clock.h"

static int dummy_clk_reset(struct clk *clk, enum clk_reset_action action)
{
	return 0;
}

static int dummy_clk_set_rate(struct clk *clk, unsigned long rate)
{
	return 0;
}

static int dummy_clk_set_max_rate(struct clk *clk, unsigned long rate)
{
	return 0;
}

static int dummy_clk_set_flags(struct clk *clk, unsigned flags)
{
	return 0;
}

static unsigned long dummy_clk_get_rate(struct clk *clk)
{
	return 0;
}

static long dummy_clk_round_rate(struct clk *clk, unsigned long rate)
{
	return rate;
}

static bool dummy_clk_is_local(struct clk *clk)
{
	return true;
}

static struct clk_ops clk_ops_dummy = {
	.reset = dummy_clk_reset,
	.set_rate = dummy_clk_set_rate,
	.set_max_rate = dummy_clk_set_max_rate,
	.set_flags = dummy_clk_set_flags,
	.get_rate = dummy_clk_get_rate,
	.round_rate = dummy_clk_round_rate,
	.is_local = dummy_clk_is_local,
};

struct clk dummy_clk = {
	.dbg_name = "dummy_clk",
	.ops = &clk_ops_dummy,
	CLK_INIT(dummy_clk),
};
