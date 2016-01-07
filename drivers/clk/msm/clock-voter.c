/* Copyright (c) 2010-2015, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/err.h>
#include <linux/rtmutex.h>
#include <linux/clk.h>
#include <linux/clk/msm-clk-provider.h>
#include <soc/qcom/clock-voter.h>
#include <soc/qcom/msm-clock-controller.h>

static DEFINE_RT_MUTEX(voter_clk_lock);

/* Aggregate the rate of clocks that are currently on. */
static unsigned long voter_clk_aggregate_rate(const struct clk *parent)
{
	struct clk *clk;
	unsigned long rate = 0;

	list_for_each_entry(clk, &parent->children, siblings) {
		struct clk_voter *v = to_clk_voter(clk);
		if (v->enabled)
			rate = max(clk->rate, rate);
	}
	return rate;
}

static int voter_clk_set_rate(struct clk *clk, unsigned long rate)
{
	int ret = 0;
	struct clk *clkp;
	struct clk_voter *clkh, *v = to_clk_voter(clk);
	unsigned long cur_rate, new_rate, other_rate = 0;

	if (v->is_branch)
		return 0;

	rt_mutex_lock(&voter_clk_lock);

	if (v->enabled) {
		struct clk *parent = clk->parent;

		/*
		 * Get the aggregate rate without this clock's vote and update
		 * if the new rate is different than the current rate
		 */
		list_for_each_entry(clkp, &parent->children, siblings) {
			clkh = to_clk_voter(clkp);
			if (clkh->enabled && clkh != v)
				other_rate = max(clkp->rate, other_rate);
		}

		cur_rate = max(other_rate, clk->rate);
		new_rate = max(other_rate, rate);

		if (new_rate != cur_rate) {
			ret = clk_set_rate(parent, new_rate);
			if (ret)
				goto unlock;
		}
	}
	clk->rate = rate;
unlock:
	rt_mutex_unlock(&voter_clk_lock);

	return ret;
}

static int voter_clk_prepare(struct clk *clk)
{
	int ret = 0;
	unsigned long cur_rate;
	struct clk *parent;
	struct clk_voter *v = to_clk_voter(clk);

	rt_mutex_lock(&voter_clk_lock);
	parent = clk->parent;

	if (v->is_branch) {
		v->enabled = true;
		goto out;
	}

	/*
	 * Increase the rate if this clock is voting for a higher rate
	 * than the current rate.
	 */
	cur_rate = voter_clk_aggregate_rate(parent);
	if (clk->rate > cur_rate) {
		ret = clk_set_rate(parent, clk->rate);
		if (ret)
			goto out;
	}
	v->enabled = true;
out:
	rt_mutex_unlock(&voter_clk_lock);

	return ret;
}

static void voter_clk_unprepare(struct clk *clk)
{
	unsigned long cur_rate, new_rate;
	struct clk *parent;
	struct clk_voter *v = to_clk_voter(clk);


	rt_mutex_lock(&voter_clk_lock);
	parent = clk->parent;

	/*
	 * Decrease the rate if this clock was the only one voting for
	 * the highest rate.
	 */
	v->enabled = false;
	if (v->is_branch)
		goto out;

	new_rate = voter_clk_aggregate_rate(parent);
	cur_rate = max(new_rate, clk->rate);

	if (new_rate < cur_rate)
		clk_set_rate(parent, new_rate);

out:
	rt_mutex_unlock(&voter_clk_lock);
}

static int voter_clk_is_enabled(struct clk *clk)
{
	struct clk_voter *v = to_clk_voter(clk);
	return v->enabled;
}

static long voter_clk_round_rate(struct clk *clk, unsigned long rate)
{
	return clk_round_rate(clk->parent, rate);
}

static bool voter_clk_is_local(struct clk *clk)
{
	return true;
}

static enum handoff voter_clk_handoff(struct clk *clk)
{
	if (!clk->rate)
		return HANDOFF_DISABLED_CLK;

	/*
	 * Send the default rate to the parent if necessary and update the
	 * software state of the voter clock.
	 */
	if (voter_clk_prepare(clk) < 0)
		return HANDOFF_DISABLED_CLK;

	return HANDOFF_ENABLED_CLK;
}

struct clk_ops clk_ops_voter = {
	.prepare = voter_clk_prepare,
	.unprepare = voter_clk_unprepare,
	.set_rate = voter_clk_set_rate,
	.is_enabled = voter_clk_is_enabled,
	.round_rate = voter_clk_round_rate,
	.is_local = voter_clk_is_local,
	.handoff = voter_clk_handoff,
};

static void *sw_vote_clk_dt_parser(struct device *dev,
					struct device_node *np)
{
	struct clk_voter *v;
	int rc;
	u32 temp;

	v = devm_kzalloc(dev, sizeof(*v), GFP_KERNEL);
	if (!v) {
		dt_err(np, "failed to alloc memory\n");
		return ERR_PTR(-ENOMEM);
	}

	rc = of_property_read_u32(np, "qcom,config-rate", &temp);
	if (rc) {
		dt_prop_err(np, "qcom,config-rate", "is missing");
		return ERR_PTR(rc);
	}

	v->c.ops = &clk_ops_voter;
	return msmclk_generic_clk_init(dev, np, &v->c);
}
MSMCLK_PARSER(sw_vote_clk_dt_parser, "qcom,sw-vote-clk", 0);
