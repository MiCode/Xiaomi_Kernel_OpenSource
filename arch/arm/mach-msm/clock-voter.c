/*
 * Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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

#include <linux/err.h>
#include <linux/spinlock.h>
#include <linux/clk.h>

#include "clock.h"
#include "clock-voter.h"

static DEFINE_SPINLOCK(voter_clk_lock);

/* Aggregate the rate of clocks that are currently on. */
static unsigned voter_clk_aggregate_rate(const struct clk *parent)
{
	struct clk *clk;
	unsigned rate = 0;

	list_for_each_entry(clk, &parent->children, siblings) {
		struct clk_voter *v = to_clk_voter(clk);
		if (v->enabled)
			rate = max(v->rate, rate);
	}
	return rate;
}

static int voter_clk_set_rate(struct clk *clk, unsigned rate)
{
	int ret = 0;
	unsigned long flags;
	struct clk *clkp;
	struct clk_voter *clkh, *v = to_clk_voter(clk);
	unsigned cur_rate, new_rate, other_rate = 0;

	spin_lock_irqsave(&voter_clk_lock, flags);

	if (v->enabled) {
		struct clk *parent = v->parent;

		/*
		 * Get the aggregate rate without this clock's vote and update
		 * if the new rate is different than the current rate
		 */
		list_for_each_entry(clkp, &parent->children, siblings) {
			clkh = to_clk_voter(clkp);
			if (clkh->enabled && clkh != v)
				other_rate = max(clkh->rate, other_rate);
		}

		cur_rate = max(other_rate, v->rate);
		new_rate = max(other_rate, rate);

		if (new_rate != cur_rate) {
			ret = clk_set_min_rate(parent, new_rate);
			if (ret)
				goto unlock;
		}
	}
	v->rate = rate;
unlock:
	spin_unlock_irqrestore(&voter_clk_lock, flags);

	return ret;
}

static int voter_clk_enable(struct clk *clk)
{
	int ret;
	unsigned long flags;
	unsigned cur_rate;
	struct clk *parent;
	struct clk_voter *v = to_clk_voter(clk);

	spin_lock_irqsave(&voter_clk_lock, flags);
	parent = v->parent;

	/*
	 * Increase the rate if this clock is voting for a higher rate
	 * than the current rate.
	 */
	cur_rate = voter_clk_aggregate_rate(parent);
	if (v->rate > cur_rate) {
		ret = clk_set_min_rate(parent, v->rate);
		if (ret)
			goto out;
	}
	v->enabled = true;
out:
	spin_unlock_irqrestore(&voter_clk_lock, flags);

	return ret;
}

static void voter_clk_disable(struct clk *clk)
{
	unsigned long flags;
	struct clk *parent;
	struct clk_voter *v = to_clk_voter(clk);
	unsigned cur_rate, new_rate;

	spin_lock_irqsave(&voter_clk_lock, flags);
	parent = v->parent;

	/*
	 * Decrease the rate if this clock was the only one voting for
	 * the highest rate.
	 */
	v->enabled = false;
	new_rate = voter_clk_aggregate_rate(parent);
	cur_rate = max(new_rate, v->rate);

	if (new_rate < cur_rate)
		clk_set_min_rate(parent, new_rate);

	spin_unlock_irqrestore(&voter_clk_lock, flags);
}

static unsigned voter_clk_get_rate(struct clk *clk)
{
	unsigned rate;
	unsigned long flags;
	struct clk_voter *v = to_clk_voter(clk);

	spin_lock_irqsave(&voter_clk_lock, flags);
	rate = v->rate;
	spin_unlock_irqrestore(&voter_clk_lock, flags);

	return rate;
}

static int voter_clk_is_enabled(struct clk *clk)
{
	struct clk_voter *v = to_clk_voter(clk);
	return v->enabled;
}

static long voter_clk_round_rate(struct clk *clk, unsigned rate)
{
	struct clk_voter *v = to_clk_voter(clk);
	return clk_round_rate(v->parent, rate);
}

static int voter_clk_set_parent(struct clk *clk, struct clk *parent)
{
	unsigned long flags;

	spin_lock_irqsave(&voter_clk_lock, flags);
	if (list_empty(&clk->siblings))
		list_add(&clk->siblings, &parent->children);
	spin_unlock_irqrestore(&voter_clk_lock, flags);

	return 0;
}

static struct clk *voter_clk_get_parent(struct clk *clk)
{
	struct clk_voter *v = to_clk_voter(clk);
	return v->parent;
}

static bool voter_clk_is_local(struct clk *clk)
{
	return true;
}

struct clk_ops clk_ops_voter = {
	.enable = voter_clk_enable,
	.disable = voter_clk_disable,
	.set_rate = voter_clk_set_rate,
	.set_min_rate = voter_clk_set_rate,
	.get_rate = voter_clk_get_rate,
	.is_enabled = voter_clk_is_enabled,
	.round_rate = voter_clk_round_rate,
	.set_parent = voter_clk_set_parent,
	.get_parent = voter_clk_get_parent,
	.is_local = voter_clk_is_local,
};
