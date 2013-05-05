/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/errno.h>

#include <linux/clk.h>
#include <mach/clk-provider.h>
#include <mach/clock-generic.h>

/* ==================== Mux clock ==================== */

static int parent_to_src_sel(struct mux_clk *mux, struct clk *p)
{
	int i;

	for (i = 0; i < mux->num_parents; i++) {
		if (mux->parents[i].src == p)
			return mux->parents[i].sel;
	}

	return -EINVAL;
}

static int mux_set_parent(struct clk *c, struct clk *p)
{
	struct mux_clk *mux = to_mux_clk(c);
	int sel = parent_to_src_sel(mux, p);
	struct clk *old_parent;
	int rc = 0;
	unsigned long flags;

	if (sel < 0)
		return sel;

	rc = __clk_pre_reparent(c, p, &flags);
	if (rc)
		goto out;

	rc = mux->ops->set_mux_sel(mux, sel);
	if (rc)
		goto set_fail;

	old_parent = c->parent;
	c->parent = p;
	__clk_post_reparent(c, old_parent, &flags);

	return 0;

set_fail:
	__clk_post_reparent(c, p, &flags);
out:
	return rc;
}

static long mux_round_rate(struct clk *c, unsigned long rate)
{
	struct mux_clk *mux = to_mux_clk(c);
	int i;
	long prate, max_prate = 0, rrate = LONG_MAX;

	for (i = 0; i < mux->num_parents; i++) {
		prate = clk_round_rate(mux->parents[i].src, rate);
		if (prate < rate) {
			max_prate = max(prate, max_prate);
			continue;
		}

		rrate = min(rrate, prate);
	}
	if (rrate == LONG_MAX)
		rrate = max_prate;

	return rrate ? rrate : -EINVAL;
}

static int mux_set_rate(struct clk *c, unsigned long rate)
{
	struct mux_clk *mux = to_mux_clk(c);
	struct clk *new_parent = NULL;
	int rc = 0, i;
	unsigned long new_par_curr_rate;

	for (i = 0; i < mux->num_parents; i++) {
		if (clk_round_rate(mux->parents[i].src, rate) == rate) {
			new_parent = mux->parents[i].src;
			break;
		}
	}
	if (new_parent == NULL)
		return -EINVAL;

	/*
	 * Switch to safe parent since the old and new parent might be the
	 * same and the parent might temporarily turn off while switching
	 * rates.
	 */
	if (mux->safe_sel >= 0)
		rc = mux->ops->set_mux_sel(mux, mux->safe_sel);
	if (rc)
		return rc;

	new_par_curr_rate = clk_get_rate(new_parent);
	rc = clk_set_rate(new_parent, rate);
	if (rc)
		goto set_rate_fail;

	rc = mux_set_parent(c, new_parent);
	if (rc)
		goto set_par_fail;

	return 0;

set_par_fail:
	clk_set_rate(new_parent, new_par_curr_rate);
set_rate_fail:
	WARN(mux->ops->set_mux_sel(mux, parent_to_src_sel(mux, c->parent)),
		"Set rate failed for %s. Also in bad state!\n", c->dbg_name);
	return rc;
}

static int mux_enable(struct clk *c)
{
	struct mux_clk *mux = to_mux_clk(c);
	if (mux->ops->enable)
		return mux->ops->enable(mux);
	return 0;
}

static void mux_disable(struct clk *c)
{
	struct mux_clk *mux = to_mux_clk(c);
	if (mux->ops->disable)
		return mux->ops->disable(mux);
}

static struct clk *mux_get_parent(struct clk *c)
{
	struct mux_clk *mux = to_mux_clk(c);
	int sel = mux->ops->get_mux_sel(mux);
	int i;

	for (i = 0; i < mux->num_parents; i++) {
		if (mux->parents[i].sel == sel)
			return mux->parents[i].src;
	}

	/* Unfamiliar parent. */
	return NULL;
}

static enum handoff mux_handoff(struct clk *c)
{
	struct mux_clk *mux = to_mux_clk(c);

	c->rate = clk_get_rate(c->parent);
	mux->safe_sel = parent_to_src_sel(mux, mux->safe_parent);

	if (mux->en_mask && mux->ops && mux->ops->is_enabled)
		return mux->ops->is_enabled(mux)
			? HANDOFF_ENABLED_CLK
			: HANDOFF_DISABLED_CLK;

	/*
	 * If this function returns 'enabled' even when the clock downstream
	 * of this clock is disabled, then handoff code will unnecessarily
	 * enable the current parent of this clock. If this function always
	 * returns 'disabled' and a clock downstream is on, the clock handoff
	 * code will bump up the ref count for this clock and its current
	 * parent as necessary. So, clocks without an actual HW gate can
	 * always return disabled.
	 */
	return HANDOFF_DISABLED_CLK;
}

struct clk_ops clk_ops_gen_mux = {
	.enable = mux_enable,
	.disable = mux_disable,
	.set_parent = mux_set_parent,
	.round_rate = mux_round_rate,
	.set_rate = mux_set_rate,
	.handoff = mux_handoff,
	.get_parent = mux_get_parent,
};


/* ==================== Divider clock ==================== */

static long __div_round_rate(struct clk *c, unsigned long rate, int *best_div)
{
	struct div_clk *d = to_div_clk(c);
	unsigned int div, min_div, max_div;
	long p_rrate, rrate = LONG_MAX;

	rate = max(rate, 1UL);

	if (!d->ops || !d->ops->set_div)
		min_div = max_div = d->div;
	else {
		min_div = max(d->min_div, 1U);
		max_div = min(d->max_div, (unsigned int) (LONG_MAX / rate));
	}

	for (div = min_div; div <= max_div; div++) {
		p_rrate = clk_round_rate(c->parent, rate * div);
		if (p_rrate < 0)
			break;

		p_rrate /= div;
		/*
		 * Trying higher dividers is only going to ask the parent for
		 * a higher rate. If it can't even output a rate higher than
		 * the one we request for this divider, the parent is not
		 * going to be able to output an even higher rate required
		 * for a higher divider. So, stop trying higher dividers.
		 */
		if (p_rrate < rate) {
			if (rrate == LONG_MAX) {
				rrate = p_rrate;
				if (best_div)
					*best_div = div;
			}
			break;
		}
		if (p_rrate < rrate) {
			rrate = p_rrate;
			if (best_div)
				*best_div = div;
		}

		if (rrate <= rate + d->rate_margin)
			break;
	}

	if (rrate == LONG_MAX)
		return -EINVAL;

	return rrate;
}

static long div_round_rate(struct clk *c, unsigned long rate)
{
	return __div_round_rate(c, rate, NULL);
}

static int div_set_rate(struct clk *c, unsigned long rate)
{
	struct div_clk *d = to_div_clk(c);
	int div, rc = 0;
	long rrate, old_prate;

	rrate = __div_round_rate(c, rate, &div);
	if (rrate != rate)
		return -EINVAL;

	if (div > d->div)
		rc = d->ops->set_div(d, div);
	if (rc)
		return rc;

	old_prate = clk_get_rate(c->parent);
	rc = clk_set_rate(c->parent, rate * div);
	if (rc)
		goto set_rate_fail;

	if (div < d->div)
		rc = d->ops->set_div(d, div);
	if (rc)
		goto div_dec_fail;

	d->div = div;

	return 0;

div_dec_fail:
	WARN(clk_set_rate(c->parent, old_prate),
		"Set rate failed for %s. Also in bad state!\n", c->dbg_name);
set_rate_fail:
	if (div > d->div)
		WARN(d->ops->set_div(d, d->div),
			"Set rate failed for %s. Also in bad state!\n",
			c->dbg_name);
	return rc;
}

static int div_enable(struct clk *c)
{
	struct div_clk *d = to_div_clk(c);
	if (d->ops->enable)
		return d->ops->enable(d);
	return 0;
}

static void div_disable(struct clk *c)
{
	struct div_clk *d = to_div_clk(c);
	if (d->ops->disable)
		return d->ops->disable(d);
}

static enum handoff div_handoff(struct clk *c)
{
	struct div_clk *d = to_div_clk(c);

	if (d->ops->get_div)
		d->div = max(d->ops->get_div(d), 1);
	d->div = max(d->div, 1U);
	c->rate = clk_get_rate(c->parent) / d->div;

	if (d->en_mask && d->ops && d->ops->is_enabled)
		return d->ops->is_enabled(d)
			? HANDOFF_ENABLED_CLK
			: HANDOFF_DISABLED_CLK;

	/*
	 * If this function returns 'enabled' even when the clock downstream
	 * of this clock is disabled, then handoff code will unnecessarily
	 * enable the current parent of this clock. If this function always
	 * returns 'disabled' and a clock downstream is on, the clock handoff
	 * code will bump up the ref count for this clock and its current
	 * parent as necessary. So, clocks without an actual HW gate can
	 * always return disabled.
	 */
	return HANDOFF_DISABLED_CLK;
}

struct clk_ops clk_ops_div = {
	.enable = div_enable,
	.disable = div_disable,
	.round_rate = div_round_rate,
	.set_rate = div_set_rate,
	.handoff = div_handoff,
};

static long __slave_div_round_rate(struct clk *c, unsigned long rate,
					int *best_div)
{
	struct div_clk *d = to_div_clk(c);
	unsigned int div, min_div, max_div;
	long p_rate;

	rate = max(rate, 1UL);

	if (!d->ops || !d->ops->set_div)
		min_div = max_div = d->div;
	else {
		min_div = d->min_div;
		max_div = d->max_div;
	}

	p_rate = clk_get_rate(c->parent);
	div = p_rate / rate;
	div = max(div, min_div);
	div = min(div, max_div);
	if (best_div)
		*best_div = div;

	return p_rate / div;
}

static long slave_div_round_rate(struct clk *c, unsigned long rate)
{
	return __slave_div_round_rate(c, rate, NULL);
}

static int slave_div_set_rate(struct clk *c, unsigned long rate)
{
	struct div_clk *d = to_div_clk(c);
	int div, rc = 0;
	long rrate;

	rrate = __slave_div_round_rate(c, rate, &div);
	if (rrate != rate)
		return -EINVAL;

	if (div == d->div)
		return 0;

	if (d->ops->set_div)
		rc = d->ops->set_div(d, div);
	if (rc)
		return rc;

	d->div = div;

	return 0;
}

struct clk_ops clk_ops_slave_div = {
	.enable = div_enable,
	.disable = div_disable,
	.round_rate = slave_div_round_rate,
	.set_rate = slave_div_set_rate,
	.handoff = div_handoff,
};
