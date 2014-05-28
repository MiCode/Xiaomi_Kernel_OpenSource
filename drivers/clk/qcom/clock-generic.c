/*
 * Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/clk/msm-clk-provider.h>
#include <linux/clk/msm-clock-generic.h>

/* ==================== Mux clock ==================== */

static int mux_parent_to_src_sel(struct mux_clk *mux, struct clk *p)
{
	return parent_to_src_sel(mux->parents, mux->num_parents, p);
}

static int mux_set_parent(struct clk *c, struct clk *p)
{
	struct mux_clk *mux = to_mux_clk(c);
	int sel = mux_parent_to_src_sel(mux, p);
	struct clk *old_parent;
	int rc = 0, i;
	unsigned long flags;

	if (sel < 0 && mux->rec_parents) {
		for (i = 0; i < mux->num_rec_parents; i++) {
			rc = clk_set_parent(mux->rec_parents[i], p);
			if (!rc) {
				/*
				 * This is necessary to ensure prepare/enable
				 * counts get propagated correctly.
				 */
				p = mux->rec_parents[i];
				sel = mux_parent_to_src_sel(mux, p);
				break;
			}
		}
	}

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
	c->rate = clk_get_rate(p);
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
	unsigned long prate, rrate = 0;

	for (i = 0; i < mux->num_parents; i++) {
		prate = clk_round_rate(mux->parents[i].src, rate);
		if (is_better_rate(rate, rrate, prate))
			rrate = prate;
	}
	if (!rrate)
		return -EINVAL;

	return rrate;
}

static int mux_set_rate(struct clk *c, unsigned long rate)
{
	struct mux_clk *mux = to_mux_clk(c);
	struct clk *new_parent = NULL;
	int rc = 0, i;
	unsigned long new_par_curr_rate;
	unsigned long flags;

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
	if (mux->safe_sel >= 0) {
		/*
		 * The safe parent might be a clock with multiple sources;
		 * to select the "safe" source, set a safe frequency.
		 */
		if (mux->safe_freq) {
			rc = clk_set_rate(mux->safe_parent, mux->safe_freq);
			if (rc) {
				pr_err("Failed to set safe rate on %s\n",
					clk_name(mux->safe_parent));
				return rc;
			}
		}

		/*
		 * Some mux implementations might switch to/from a low power
		 * parent as part of their disable/enable ops. Grab the
		 * enable lock to avoid racing with these implementations.
		 */
		spin_lock_irqsave(&c->lock, flags);
		rc = mux->ops->set_mux_sel(mux, mux->safe_sel);
		spin_unlock_irqrestore(&c->lock, flags);
	}
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
	WARN(mux->ops->set_mux_sel(mux,
		mux_parent_to_src_sel(mux, c->parent)),
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
	mux->safe_sel = mux_parent_to_src_sel(mux, mux->safe_parent);

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

static void __iomem *mux_clk_list_registers(struct clk *c, int n,
			struct clk_register_data **regs, u32 *size)
{
	struct mux_clk *mux = to_mux_clk(c);

	if (mux->ops && mux->ops->list_registers)
		return mux->ops->list_registers(mux, n, regs, size);

	return ERR_PTR(-EINVAL);
}

struct clk_ops clk_ops_gen_mux = {
	.enable = mux_enable,
	.disable = mux_disable,
	.set_parent = mux_set_parent,
	.round_rate = mux_round_rate,
	.set_rate = mux_set_rate,
	.handoff = mux_handoff,
	.get_parent = mux_get_parent,
	.list_registers = mux_clk_list_registers,
};

/* ==================== Divider clock ==================== */

static long __div_round_rate(struct div_data *data, unsigned long rate,
	struct clk *parent, unsigned int *best_div, unsigned long *best_prate)
{
	unsigned int div, min_div, max_div, _best_div = 1;
	unsigned long prate, _best_prate = 0, rrate = 0, req_prate, actual_rate;
	unsigned int numer;

	rate = max(rate, 1UL);

	min_div = max(data->min_div, 1U);
	max_div = min(data->max_div, (unsigned int) (ULONG_MAX / rate));

	/*
	 * div values are doubled for half dividers.
	 * Adjust for that by picking a numer of 2.
	 */
	numer = data->is_half_divider ? 2 : 1;

	for (div = min_div; div <= max_div; div++) {
		req_prate = mult_frac(rate, div, numer);
		prate = clk_round_rate(parent, req_prate);
		if (IS_ERR_VALUE(prate))
			break;

		actual_rate = mult_frac(prate, numer, div);
		if (is_better_rate(rate, rrate, actual_rate)) {
			rrate = actual_rate;
			_best_div = div;
			_best_prate = prate;
		}

		/*
		 * Trying higher dividers is only going to ask the parent for
		 * a higher rate. If it can't even output a rate higher than
		 * the one we request for this divider, the parent is not
		 * going to be able to output an even higher rate required
		 * for a higher divider. So, stop trying higher dividers.
		 */
		if (actual_rate < rate)
			break;

		if (rrate <= rate + data->rate_margin)
			break;
	}

	if (!rrate)
		return -EINVAL;
	if (best_div)
		*best_div = _best_div;
	if (best_prate)
		*best_prate = _best_prate;

	return rrate;
}

static long div_round_rate(struct clk *c, unsigned long rate)
{
	struct div_clk *d = to_div_clk(c);

	return __div_round_rate(&d->data, rate, c->parent, NULL, NULL);
}

static int _find_safe_div(struct clk *c, unsigned long rate)
{
	struct div_clk *d = to_div_clk(c);
	struct div_data *data = &d->data;
	unsigned long fast = max(rate, c->rate);
	unsigned int numer = data->is_half_divider ? 2 : 1;
	int i, safe_div = 0;

	if (!d->safe_freq)
		return 0;

	/* Find the max safe freq that is lesser than fast */
	for (i = data->max_div; i >= data->min_div; i--)
		if (mult_frac(d->safe_freq, numer, i) <= fast)
			safe_div = i;

	return safe_div ?: -EINVAL;
}

static int div_set_rate(struct clk *c, unsigned long rate)
{
	struct div_clk *d = to_div_clk(c);
	int safe_div, div, rc = 0;
	long rrate, old_prate, new_prate;
	struct div_data *data = &d->data;

	rrate = __div_round_rate(data, rate, c->parent, &div, &new_prate);
	if (rrate != rate)
		return -EINVAL;

	/*
	 * For fixed divider clock we don't want to return an error if the
	 * requested rate matches the achievable rate. So, don't check for
	 * !d->ops and return an error. __div_round_rate() ensures div ==
	 * d->div if !d->ops.
	 */

	safe_div = _find_safe_div(c, rate);
	if (d->safe_freq && safe_div < 0) {
		pr_err("No safe div on %s for transitioning from %lu to %lu\n",
			c->dbg_name, c->rate, rate);
		return -EINVAL;
	}

	safe_div = max(safe_div, div);

	if (safe_div > data->div) {
		rc = d->ops->set_div(d, safe_div);
		if (rc) {
			pr_err("Failed to set div %d on %s\n", safe_div,
				c->dbg_name);
			return rc;
		}
	}

	old_prate = clk_get_rate(c->parent);
	rc = clk_set_rate(c->parent, new_prate);
	if (rc)
		goto set_rate_fail;

	if (div < data->div)
		rc = d->ops->set_div(d, div);
	else if (div < safe_div)
		rc = d->ops->set_div(d, div);
	if (rc)
		goto div_dec_fail;

	data->div = div;

	return 0;

div_dec_fail:
	WARN(clk_set_rate(c->parent, old_prate),
		"Set rate failed for %s. Also in bad state!\n", c->dbg_name);
set_rate_fail:
	if (safe_div > data->div)
		WARN(d->ops->set_div(d, data->div),
			"Set rate failed for %s. Also in bad state!\n",
			c->dbg_name);
	return rc;
}

static int div_enable(struct clk *c)
{
	struct div_clk *d = to_div_clk(c);
	if (d->ops && d->ops->enable)
		return d->ops->enable(d);
	return 0;
}

static void div_disable(struct clk *c)
{
	struct div_clk *d = to_div_clk(c);
	if (d->ops && d->ops->disable)
		return d->ops->disable(d);
}

static enum handoff div_handoff(struct clk *c)
{
	struct div_clk *d = to_div_clk(c);
	unsigned int div = d->data.div;

	if (d->ops && d->ops->get_div)
		div = max(d->ops->get_div(d), 1);
	div = max(div, 1U);
	c->rate = clk_get_rate(c->parent) / div;

	if (!d->ops || !d->ops->set_div)
		d->data.min_div = d->data.max_div = div;
	d->data.div = div;

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

static void __iomem *div_clk_list_registers(struct clk *c, int n,
			struct clk_register_data **regs, u32 *size)
{
	struct div_clk *d = to_div_clk(c);

	if (d->ops && d->ops->list_registers)
		return d->ops->list_registers(d, n, regs, size);

	return ERR_PTR(-EINVAL);
}

struct clk_ops clk_ops_div = {
	.enable = div_enable,
	.disable = div_disable,
	.round_rate = div_round_rate,
	.set_rate = div_set_rate,
	.handoff = div_handoff,
	.list_registers = div_clk_list_registers,
};

static long __slave_div_round_rate(struct clk *c, unsigned long rate,
					int *best_div)
{
	struct div_clk *d = to_div_clk(c);
	unsigned int div, min_div, max_div;
	long p_rate;

	rate = max(rate, 1UL);

	min_div = d->data.min_div;
	max_div = d->data.max_div;

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

	if (div == d->data.div)
		return 0;

	/*
	 * For fixed divider clock we don't want to return an error if the
	 * requested rate matches the achievable rate. So, don't check for
	 * !d->ops and return an error. __slave_div_round_rate() ensures
	 * div == d->data.div if !d->ops.
	 */
	rc = d->ops->set_div(d, div);
	if (rc)
		return rc;

	d->data.div = div;

	return 0;
}

static unsigned long slave_div_get_rate(struct clk *c)
{
	struct div_clk *d = to_div_clk(c);
	if (!d->data.div)
		return 0;
	return clk_get_rate(c->parent) / d->data.div;
}

struct clk_ops clk_ops_slave_div = {
	.enable = div_enable,
	.disable = div_disable,
	.round_rate = slave_div_round_rate,
	.set_rate = slave_div_set_rate,
	.get_rate = slave_div_get_rate,
	.handoff = div_handoff,
	.list_registers = div_clk_list_registers,
};


/**
 * External clock
 * Some clock controllers have input clock signal that come from outside the
 * clock controller. That input clock signal might then be used as a source for
 * several clocks inside the clock controller. This external clock
 * implementation models this input clock signal by just passing on the requests
 * to the clock's parent, the original external clock source. The driver for the
 * clock controller should clk_get() the original external clock in the probe
 * function and set is as a parent to this external clock..
 */

long parent_round_rate(struct clk *c, unsigned long rate)
{
	return clk_round_rate(c->parent, rate);
}

static int ext_set_rate(struct clk *c, unsigned long rate)
{
	return clk_set_rate(c->parent, rate);
}

unsigned long parent_get_rate(struct clk *c)
{
	return clk_get_rate(c->parent);
}

static int ext_set_parent(struct clk *c, struct clk *p)
{
	return clk_set_parent(c->parent, p);
}

static struct clk *ext_get_parent(struct clk *c)
{
	struct ext_clk *ext = to_ext_clk(c);

	if (!IS_ERR_OR_NULL(c->parent))
		return c->parent;
	return clk_get(ext->dev, ext->clk_id);
}

static enum handoff ext_handoff(struct clk *c)
{
	c->rate = clk_get_rate(c->parent);
	/* Similar reasoning applied in div_handoff, see comment there. */
	return HANDOFF_DISABLED_CLK;
}

struct clk_ops clk_ops_ext = {
	.handoff = ext_handoff,
	.round_rate = parent_round_rate,
	.set_rate = ext_set_rate,
	.get_rate = parent_get_rate,
	.set_parent = ext_set_parent,
	.get_parent = ext_get_parent,
};


/* ==================== Mux_div clock ==================== */

static int mux_div_clk_enable(struct clk *c)
{
	struct mux_div_clk *md = to_mux_div_clk(c);

	if (md->ops->enable)
		return md->ops->enable(md);
	return 0;
}

static void mux_div_clk_disable(struct clk *c)
{
	struct mux_div_clk *md = to_mux_div_clk(c);

	if (md->ops->disable)
		return md->ops->disable(md);
}

static long __mux_div_round_rate(struct clk *c, unsigned long rate,
	struct clk **best_parent, int *best_div, unsigned long *best_prate)
{
	struct mux_div_clk *md = to_mux_div_clk(c);
	unsigned int i;
	unsigned long rrate, best = 0, _best_div = 0, _best_prate = 0;
	struct clk *_best_parent = 0;

	for (i = 0; i < md->num_parents; i++) {
		int div;
		unsigned long prate;

		rrate = __div_round_rate(&md->data, rate, md->parents[i].src,
				&div, &prate);

		if (is_better_rate(rate, best, rrate)) {
			best = rrate;
			_best_div = div;
			_best_prate = prate;
			_best_parent = md->parents[i].src;
		}

		if (rate <= rrate && rrate <= rate + md->data.rate_margin)
			break;
	}

	if (best_div)
		*best_div = _best_div;
	if (best_prate)
		*best_prate = _best_prate;
	if (best_parent)
		*best_parent = _best_parent;

	if (best)
		return best;
	return -EINVAL;
}

static long mux_div_clk_round_rate(struct clk *c, unsigned long rate)
{
	return __mux_div_round_rate(c, rate, NULL, NULL, NULL);
}

/* requires enable lock to be held */
static int __set_src_div(struct mux_div_clk *md, struct clk *parent, u32 div)
{
	u32 rc = 0, src_sel;

	src_sel = parent_to_src_sel(md->parents, md->num_parents, parent);
	/*
	 * If the clock is disabled, don't change to the new settings until
	 * the clock is reenabled
	 */
	if (md->c.count)
		rc = md->ops->set_src_div(md, src_sel, div);
	if (!rc) {
		md->data.div = div;
		md->src_sel = src_sel;
	}

	return rc;
}

static int set_src_div(struct mux_div_clk *md, struct clk *parent, u32 div)
{
	unsigned long flags;
	u32 rc;

	spin_lock_irqsave(&md->c.lock, flags);
	rc = __set_src_div(md, parent, div);
	spin_unlock_irqrestore(&md->c.lock, flags);

	return rc;
}

/* Must be called after handoff to ensure parent clock rates are initialized */
static int safe_parent_init_once(struct clk *c)
{
	unsigned long rrate;
	u32 best_div;
	struct clk *best_parent;
	struct mux_div_clk *md = to_mux_div_clk(c);

	if (IS_ERR(md->safe_parent))
		return -EINVAL;
	if (!md->safe_freq || md->safe_parent)
		return 0;

	rrate = __mux_div_round_rate(c, md->safe_freq, &best_parent,
			&best_div, NULL);

	if (rrate == md->safe_freq) {
		md->safe_div = best_div;
		md->safe_parent = best_parent;
	} else {
		md->safe_parent = ERR_PTR(-EINVAL);
		return -EINVAL;
	}
	return 0;
}

static int mux_div_clk_set_rate(struct clk *c, unsigned long rate)
{
	struct mux_div_clk *md = to_mux_div_clk(c);
	unsigned long flags, rrate;
	unsigned long new_prate, old_prate;
	struct clk *old_parent, *new_parent;
	u32 new_div, old_div;
	int rc;

	rc = safe_parent_init_once(c);
	if (rc)
		return rc;

	rrate = __mux_div_round_rate(c, rate, &new_parent, &new_div,
							&new_prate);
	if (rrate != rate)
		return -EINVAL;

	old_parent = c->parent;
	old_div = md->data.div;
	old_prate = clk_get_rate(c->parent);

	/* Refer to the description of safe_freq in clock-generic.h */
	if (md->safe_freq)
		rc = set_src_div(md, md->safe_parent, md->safe_div);

	else if (new_parent == old_parent && new_div >= old_div) {
		/*
		 * If both the parent_rate and divider changes, there may be an
		 * intermediate frequency generated. Ensure this intermediate
		 * frequency is less than both the new rate and previous rate.
		 */
		rc = set_src_div(md, old_parent, new_div);
	}
	if (rc)
		return rc;

	rc = clk_set_rate(new_parent, new_prate);
	if (rc) {
		pr_err("failed to set %s to %ld\n",
			clk_name(new_parent), new_prate);
		goto err_set_rate;
	}

	rc = __clk_pre_reparent(c, new_parent, &flags);
	if (rc)
		goto err_pre_reparent;

	/* Set divider and mux src atomically */
	rc = __set_src_div(md, new_parent, new_div);
	if (rc)
		goto err_set_src_div;

	c->parent = new_parent;

	__clk_post_reparent(c, old_parent, &flags);
	return 0;

err_set_src_div:
	/* Not switching to new_parent, so disable it */
	__clk_post_reparent(c, new_parent, &flags);
err_pre_reparent:
	rc = clk_set_rate(old_parent, old_prate);
	WARN(rc, "%s: error changing parent (%s) rate to %ld\n",
		clk_name(c), clk_name(old_parent), old_prate);
err_set_rate:
	rc = set_src_div(md, old_parent, old_div);
	WARN(rc, "%s: error changing back to original div (%d) and parent (%s)\n",
		clk_name(c), old_div, clk_name(old_parent));

	return rc;
}

static struct clk *mux_div_clk_get_parent(struct clk *c)
{
	struct mux_div_clk *md = to_mux_div_clk(c);
	u32 i, div, src_sel;

	md->ops->get_src_div(md, &src_sel, &div);

	md->data.div = div;
	md->src_sel = src_sel;

	for (i = 0; i < md->num_parents; i++) {
		if (md->parents[i].sel == src_sel)
			return md->parents[i].src;
	}

	return NULL;
}

static enum handoff mux_div_clk_handoff(struct clk *c)
{
	struct mux_div_clk *md = to_mux_div_clk(c);
	unsigned long parent_rate;
	unsigned int numer;

	parent_rate = clk_get_rate(c->parent);
	if (!parent_rate)
		return HANDOFF_DISABLED_CLK;
	/*
	 * div values are doubled for half dividers.
	 * Adjust for that by picking a numer of 2.
	 */
	numer = md->data.is_half_divider ? 2 : 1;

	if (md->data.div) {
		c->rate = mult_frac(parent_rate, numer, md->data.div);
	} else {
		c->rate = 0;
		return HANDOFF_DISABLED_CLK;
	}

	if (!md->ops->is_enabled)
		return HANDOFF_DISABLED_CLK;
	if (md->ops->is_enabled(md))
		return HANDOFF_ENABLED_CLK;
	return HANDOFF_DISABLED_CLK;
}

static void __iomem *mux_div_clk_list_registers(struct clk *c, int n,
				struct clk_register_data **regs, u32 *size)
{
	struct mux_div_clk *md = to_mux_div_clk(c);

	if (md->ops && md->ops->list_registers)
		return md->ops->list_registers(md, n , regs, size);

	return ERR_PTR(-EINVAL);
}

struct clk_ops clk_ops_mux_div_clk = {
	.enable = mux_div_clk_enable,
	.disable = mux_div_clk_disable,
	.set_rate = mux_div_clk_set_rate,
	.round_rate = mux_div_clk_round_rate,
	.get_parent = mux_div_clk_get_parent,
	.handoff = mux_div_clk_handoff,
	.list_registers = mux_div_clk_list_registers,
};
