/*
 * Copyright (c) 2013-2014, 2016-2017,
 *
 * The Linux Foundation. All rights reserved.
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
#include <linux/export.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/clk-provider.h>
#include <linux/of.h>

#include "clk-regmap.h"
#include "clk-debug.h"
#include "common.h"

static struct clk_hw *measure;

static DEFINE_SPINLOCK(clk_reg_lock);
static DEFINE_MUTEX(clk_debug_lock);

#define TCXO_DIV_4_HZ		4800000
#define SAMPLE_TICKS_1_MS	0x1000
#define SAMPLE_TICKS_14_MS	0x10000

#define XO_DIV4_CNT_DONE	BIT(25)
#define CNT_EN			BIT(20)
#define MEASURE_CNT		BM(24, 0)

/* Sample clock for 'ticks' reference clock ticks. */
static u32 run_measurement(unsigned ticks, struct regmap *regmap,
		u32 ctl_reg, u32 status_reg)
{
	u32 regval;

	/* Stop counters and set the XO4 counter start value. */
	regmap_write(regmap, ctl_reg, ticks);

	regmap_read(regmap, status_reg, &regval);

	/* Wait for timer to become ready. */
	while ((regval & XO_DIV4_CNT_DONE) != 0) {
		cpu_relax();
		regmap_read(regmap, status_reg, &regval);
	}

	/* Run measurement and wait for completion. */
	regmap_write(regmap, ctl_reg, (CNT_EN|ticks));

	regmap_read(regmap, status_reg, &regval);

	while ((regval & XO_DIV4_CNT_DONE) == 0) {
		cpu_relax();
		regmap_read(regmap, status_reg, &regval);
	}

	/* Return measured ticks. */
	regmap_read(regmap, status_reg, &regval);
	regval &= MEASURE_CNT;

	return regval;
}

/*
 * Perform a hardware rate measurement for a given clock.
 * FOR DEBUG USE ONLY: Measurements take ~15 ms!
 */
static unsigned long clk_debug_mux_measure_rate(struct clk_hw *hw)
{
	unsigned long flags, ret = 0;
	u32 gcc_xo4_reg, multiplier;
	u64 raw_count_short, raw_count_full;
	struct clk_debug_mux *meas = to_clk_measure(hw);
	struct measure_clk_data *data = meas->priv;

	clk_prepare_enable(data->cxo);

	spin_lock_irqsave(&clk_reg_lock, flags);

	multiplier = meas->multiplier + 1;

	/* Enable CXO/4 and RINGOSC branch. */
	regmap_read(meas->regmap[GCC], data->xo_div4_cbcr, &gcc_xo4_reg);
	gcc_xo4_reg |= BIT(0);
	regmap_write(meas->regmap[GCC], data->xo_div4_cbcr, gcc_xo4_reg);

	/*
	 * The ring oscillator counter will not reset if the measured clock
	 * is not running.  To detect this, run a short measurement before
	 * the full measurement.  If the raw results of the two are the same
	 * then the clock must be off.
	 */

	/* Run a short measurement. (~1 ms) */
	raw_count_short = run_measurement(SAMPLE_TICKS_1_MS, meas->regmap[GCC],
					data->ctl_reg, data->status_reg);

	/* Run a full measurement. (~14 ms) */
	raw_count_full = run_measurement(SAMPLE_TICKS_14_MS, meas->regmap[GCC],
					data->ctl_reg, data->status_reg);

	gcc_xo4_reg &= ~BIT(0);
	regmap_write(meas->regmap[GCC], data->xo_div4_cbcr, gcc_xo4_reg);

	/* Return 0 if the clock is off. */
	if (raw_count_full == raw_count_short)
		ret = 0;
	else {
		/* Compute rate in Hz. */
		raw_count_full = ((raw_count_full * 10) + 15) * TCXO_DIV_4_HZ;
		do_div(raw_count_full, ((SAMPLE_TICKS_14_MS * 10) + 35));
		ret = (raw_count_full * multiplier);
	}

	spin_unlock_irqrestore(&clk_reg_lock, flags);

	clk_disable_unprepare(data->cxo);

	return ret;
}

static u8 clk_debug_mux_get_parent(struct clk_hw *hw)
{
	struct clk_debug_mux *meas = to_clk_measure(hw);
	int i, num_parents = clk_hw_get_num_parents(hw);

	for (i = 0; i < num_parents; i++) {
		if (!strcmp(meas->parent[i].parents,
					hw->init->parent_names[i])) {
			pr_debug("%s: Clock name %s index %d\n", __func__,
					hw->init->name, i);
			return i;
		}
	}

	return 0;
}

static int clk_debug_mux_set_parent(struct clk_hw *hw, u8 index)
{
	struct clk_debug_mux *meas = to_clk_measure(hw);
	unsigned long lsb = 0;
	u32 regval = 0;
	int dbg_cc = 0;

	dbg_cc = meas->parent[index].dbg_cc;

	if (dbg_cc != GCC) {
		regmap_read(meas->regmap[dbg_cc], 0x0, &regval);

		/* Clear & Set post divider bits */
		if (meas->parent[index].post_div_mask) {
			regval &= ~meas->parent[index].post_div_mask;
			lsb = find_first_bit((unsigned long *)
				&meas->parent[index].post_div_mask, 32);
			regval |= (meas->parent[index].post_div_val << lsb) &
					meas->parent[index].post_div_mask;
			meas->multiplier = meas->parent[index].post_div_val;
		}

		if (meas->parent[index].mask)
			regval &= ~meas->parent[index].mask <<
					meas->parent[index].shift;
		else
			regval &= ~meas->mask;

		regval |= (meas->parent[index].next_sel & meas->mask);

		if (meas->parent[index].en_mask == 0xFF)
			/* Skip en_mask */
			regval = regval;
		else if (meas->parent[index].en_mask)
			regval |= meas->parent[index].en_mask;
		else
			regval |= meas->en_mask;

		regmap_write(meas->regmap[dbg_cc], 0x0, regval);
	}

       /* update the debug sel for GCC */
	regmap_read(meas->regmap[GCC], meas->debug_offset, &regval);

	/* clear post divider bits */
	regval &= ~BM(15, 12);
	lsb = find_first_bit((unsigned long *)
			&meas->parent[index].post_div_mask, 32);
	regval |= (meas->parent[index].post_div_val << lsb) &
			meas->parent[index].post_div_mask;
	meas->multiplier = meas->parent[index].post_div_val;
	regval &= ~meas->mask;
	regval |= (meas->parent[index].sel & meas->mask);
	regval |= meas->en_mask;

	regmap_write(meas->regmap[GCC], meas->debug_offset, regval);

	return 0;
}

const struct clk_ops clk_debug_mux_ops = {
	.get_parent = clk_debug_mux_get_parent,
	.set_parent = clk_debug_mux_set_parent,
};
EXPORT_SYMBOL(clk_debug_mux_ops);

static int clk_debug_measure_get(void *data, u64 *val)
{
	struct clk_hw *hw = data, *par;
	int ret = 0;
	unsigned long meas_rate, sw_rate;

	mutex_lock(&clk_debug_lock);

	ret = clk_set_parent(measure->clk, hw->clk);
	if (!ret) {
		par = measure;
		while (par && par != hw) {
			if (par->init->ops->enable)
				par->init->ops->enable(par);
			par = clk_hw_get_parent(par);
		}
		*val = clk_debug_mux_measure_rate(measure);
	}

	meas_rate = clk_get_rate(hw->clk);
	sw_rate = clk_get_rate(clk_hw_get_parent(measure)->clk);
	if (sw_rate && meas_rate >= (sw_rate * 2))
		*val *= DIV_ROUND_CLOSEST(meas_rate, sw_rate);

	mutex_unlock(&clk_debug_lock);

	return ret;
}

DEFINE_SIMPLE_ATTRIBUTE(clk_measure_fops, clk_debug_measure_get,
							NULL, "%lld\n");

int clk_debug_measure_add(struct clk_hw *hw, struct dentry *dentry)
{
	if (IS_ERR_OR_NULL(measure)) {
		pr_err_once("Please check if `measure` clk is registered!!!\n");
		return 0;
	}

	if (clk_set_parent(measure->clk, hw->clk))
		return 0;

	debugfs_create_file("clk_measure", S_IRUGO, dentry, hw,
					&clk_measure_fops);
	return 0;
}
EXPORT_SYMBOL(clk_debug_measure_add);

int clk_register_debug(struct clk_hw *hw)
{
	if (IS_ERR_OR_NULL(measure)) {
		if (hw->init->flags & CLK_IS_MEASURE) {
			measure = hw;
			return 0;
		}
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(clk_register_debug);

