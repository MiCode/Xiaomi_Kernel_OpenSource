/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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
#include <linux/bitops.h>
#include <linux/msm-bus.h>

#include "clk-regmap.h"
#include "clk-debug.h"

static struct clk_hw *measure;

static DEFINE_SPINLOCK(clk_reg_lock);
static DEFINE_MUTEX(clk_debug_lock);

#define TCXO_DIV_4_HZ		4800000
#define SAMPLE_TICKS_1_MS	0x1000
#define SAMPLE_TICKS_14_MS	0x10000

#define XO_DIV4_CNT_DONE	BIT(25)
#define CNT_EN			BIT(20)
#define MEASURE_CNT		GENMASK(24, 0)
#define CBCR_ENA		BIT(0)

/* Sample clock for 'ticks' reference clock ticks. */
static u32 run_measurement(unsigned int ticks, struct regmap *regmap,
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

	/* Stop the counters */
	regmap_write(regmap, ctl_reg, ticks);

	return regval;
}

/*
 * Perform a hardware rate measurement for a given clock.
 * FOR DEBUG USE ONLY: Measurements take ~15 ms!
 */
static unsigned long clk_debug_mux_measure_rate(struct clk_hw *hw)
{
	unsigned long flags, ret = 0;
	u32 gcc_xo4_reg, multiplier = 1;
	u64 raw_count_short, raw_count_full;
	struct clk_debug_mux *meas = to_clk_measure(hw);
	struct measure_clk_data *data = meas->priv;

	clk_prepare_enable(data->cxo);

	spin_lock_irqsave(&clk_reg_lock, flags);

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
	struct clk_hw *hw_clk = clk_hw_get_parent(hw);

	if (!hw_clk)
		return 0;

	for (i = 0; i < num_parents; i++) {
		if (!strcmp(meas->parent[i].parents,
					clk_hw_get_name(hw_clk))) {
			pr_debug("%s: clock parent - %s, index %d\n", __func__,
					meas->parent[i].parents, i);
			return i;
		}
	}

	return 0;
}

static int clk_debug_mux_set_parent(struct clk_hw *hw, u8 index)
{
	struct clk_debug_mux *meas = to_clk_measure(hw);
	u32 regval = 0;
	int dbg_cc = 0;

	dbg_cc = meas->parent[index].dbg_cc;

	if (dbg_cc != GCC) {
		/* Update the recursive debug mux */
		regmap_read(meas->regmap[dbg_cc],
				meas->parent[index].mux_offset, &regval);
		regval &= ~(meas->parent[index].mux_sel_mask <<
				meas->parent[index].mux_sel_shift);
		regval |= (meas->parent[index].dbg_cc_mux_sel &
				meas->parent[index].mux_sel_mask) <<
				meas->parent[index].mux_sel_shift;
		regmap_write(meas->regmap[dbg_cc],
				meas->parent[index].mux_offset, regval);

		regmap_read(meas->regmap[dbg_cc],
				meas->parent[index].post_div_offset, &regval);
		regval &= ~(meas->parent[index].post_div_mask <<
				meas->parent[index].post_div_shift);
		regval |= ((meas->parent[index].post_div_val - 1) &
				meas->parent[index].post_div_mask) <<
				meas->parent[index].post_div_shift;
		regmap_write(meas->regmap[dbg_cc],
				meas->parent[index].post_div_offset, regval);
	}

	/* Update the debug sel for GCC */
	regmap_read(meas->regmap[GCC], meas->debug_offset, &regval);
	regval &= ~(meas->src_sel_mask << meas->src_sel_shift);
	regval |= (meas->parent[index].prim_mux_sel & meas->src_sel_mask) <<
			meas->src_sel_shift;
	regmap_write(meas->regmap[GCC], meas->debug_offset, regval);

	/* Set the GCC mux's post divider bits */
	regmap_read(meas->regmap[GCC], meas->post_div_offset, &regval);
	regval &= ~(meas->post_div_mask << meas->post_div_shift);
	regval |= ((meas->parent[index].prim_mux_div_val - 1) &
			meas->post_div_mask) << meas->post_div_shift;
	regmap_write(meas->regmap[GCC], meas->post_div_offset, regval);

	return 0;
}

const struct clk_ops clk_debug_mux_ops = {
	.get_parent = clk_debug_mux_get_parent,
	.set_parent = clk_debug_mux_set_parent,
};
EXPORT_SYMBOL(clk_debug_mux_ops);

static void enable_debug_clks(struct clk_debug_mux *meas, u8 index)
{
	int dbg_cc = meas->parent[index].dbg_cc;

	meas->en_mask = meas->en_mask ? meas->en_mask : CBCR_ENA;

	if (dbg_cc != GCC) {
		/* Not all recursive muxes have a DEBUG clock. */
		if (meas->parent[index].cbcr_offset != U32_MAX)
			regmap_update_bits(meas->regmap[dbg_cc],
					meas->parent[index].cbcr_offset,
					meas->en_mask, meas->en_mask);
	}

	/* Turn on the GCC_DEBUG_CBCR */
	regmap_update_bits(meas->regmap[GCC], meas->cbcr_offset,
					meas->en_mask, meas->en_mask);

}

static void disable_debug_clks(struct clk_debug_mux *meas, u8 index)
{
	int dbg_cc = meas->parent[index].dbg_cc;

	meas->en_mask = meas->en_mask ? meas->en_mask : CBCR_ENA;

	/* Turn off the GCC_DEBUG_CBCR */
	regmap_update_bits(meas->regmap[GCC], meas->cbcr_offset,
					meas->en_mask, 0);

	if (dbg_cc != GCC) {
		if (meas->parent[index].cbcr_offset != U32_MAX)
			regmap_update_bits(meas->regmap[dbg_cc],
					meas->parent[index].cbcr_offset,
					meas->en_mask, 0);
	}
}

static int clk_debug_measure_get(void *data, u64 *val)
{
	struct clk_hw *hw = data, *par;
	struct clk_debug_mux *meas = to_clk_measure(measure);
	int index;
	int ret = 0;
	unsigned long meas_rate, sw_rate;

	mutex_lock(&clk_debug_lock);

	/*
	 * Vote for bandwidth to re-connect config ports
	 * to multimedia clock controllers.
	 */
	if (meas->bus_cl_id)
		msm_bus_scale_client_update_request(meas->bus_cl_id, 1);

	ret = clk_set_parent(measure->clk, hw->clk);
	if (!ret) {
		par = measure;
		index =  clk_debug_mux_get_parent(measure);

		enable_debug_clks(meas, index);
		while (par && par != hw) {
			if (par->init->ops->enable)
				par->init->ops->enable(par);
			par = clk_hw_get_parent(par);
		}
		*val = clk_debug_mux_measure_rate(measure);
		if (meas->parent[index].dbg_cc != GCC)
			*val *= meas->parent[index].post_div_val;
		*val *= meas->parent[index].prim_mux_div_val;

		/* Accommodate for any pre-set dividers */
		if (meas->parent[index].misc_div_val)
			*val *= meas->parent[index].misc_div_val;
	} else {
		pr_err("Failed to set the debug mux's parent.\n");
		goto exit;
	}

	meas_rate = clk_get_rate(hw->clk);
	par = clk_hw_get_parent(measure);
	if (!par) {
		ret = -EINVAL;
		goto exit1;
	}

	sw_rate = clk_get_rate(par->clk);
	if (sw_rate && meas_rate >= (sw_rate * 2))
		*val *= DIV_ROUND_CLOSEST(meas_rate, sw_rate);
exit1:
	disable_debug_clks(meas, index);
exit:
	if (meas->bus_cl_id)
		msm_bus_scale_client_update_request(meas->bus_cl_id, 0);
	mutex_unlock(&clk_debug_lock);
	return ret;
}

DEFINE_SIMPLE_ATTRIBUTE(clk_measure_fops, clk_debug_measure_get,
							NULL, "%lld\n");

static int clk_debug_read_period(void *data, u64 *val)
{
	struct clk_hw *hw = data;
	struct clk_debug_mux *meas = to_clk_measure(measure);
	int index;
	int dbg_cc;
	int ret = 0;
	u32 regval;

	mutex_lock(&clk_debug_lock);

	ret = clk_set_parent(measure->clk, hw->clk);
	if (!ret) {
		index = clk_debug_mux_get_parent(measure);
		dbg_cc = meas->parent[index].dbg_cc;

		regmap_read(meas->regmap[dbg_cc], meas->period_offset, &regval);
		if (!regval) {
			pr_err("Error reading mccc period register, ret = %d\n",
			       ret);
			mutex_unlock(&clk_debug_lock);
			return 0;
		}
		*val = 1000000000000UL;
		do_div(*val, regval);
	} else {
		pr_err("Failed to set the debug mux's parent.\n");
	}

	mutex_unlock(&clk_debug_lock);
	return ret;
}

DEFINE_SIMPLE_ATTRIBUTE(clk_read_period_fops, clk_debug_read_period,
							NULL, "%lld\n");

int clk_debug_measure_add(struct clk_hw *hw, struct dentry *dentry)
{
	int ret;
	int index;
	struct clk_debug_mux *meas;

	if (IS_ERR_OR_NULL(measure)) {
		pr_err_once("Please check if `measure` clk is registered.\n");
		return 0;
	}

	meas = to_clk_measure(measure);
	if (meas->bus_cl_id)
		msm_bus_scale_client_update_request(meas->bus_cl_id, 1);
	ret = clk_set_parent(measure->clk, hw->clk);
	if (ret) {
		pr_debug("Unable to set %s as %s's parent, ret=%d\n",
			clk_hw_get_name(hw), clk_hw_get_name(measure), ret);
		goto err;
	}

	index = clk_debug_mux_get_parent(measure);
	if (meas->parent[index].dbg_cc == MC_CC)
		debugfs_create_file("clk_measure", 0444, dentry, hw,
					&clk_read_period_fops);
	else
		debugfs_create_file("clk_measure", 0444, dentry, hw,
					&clk_measure_fops);
err:
	if (meas->bus_cl_id)
		msm_bus_scale_client_update_request(meas->bus_cl_id, 0);
	return 0;
}
EXPORT_SYMBOL(clk_debug_measure_add);

int clk_debug_measure_register(struct clk_hw *hw)
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
EXPORT_SYMBOL(clk_debug_measure_register);

void clk_debug_bus_vote(struct clk_hw *hw, bool enable)
{
	if (hw->init->bus_cl_id)
		msm_bus_scale_client_update_request(hw->init->bus_cl_id,
								enable);
}
