// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2016, 2019, The Linux Foundation. All rights reserved. */

#include <linux/clk.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/bitops.h>
#include <linux/mfd/syscon.h>
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
	regmap_read(meas->regmap, data->xo_div4_cbcr, &gcc_xo4_reg);
	gcc_xo4_reg |= BIT(0);
	regmap_write(meas->regmap, data->xo_div4_cbcr, gcc_xo4_reg);

	/*
	 * The ring oscillator counter will not reset if the measured clock
	 * is not running.  To detect this, run a short measurement before
	 * the full measurement.  If the raw results of the two are the same
	 * then the clock must be off.
	 */

	/* Run a short measurement. (~1 ms) */
	raw_count_short = run_measurement(SAMPLE_TICKS_1_MS, meas->regmap,
				data->ctl_reg, data->status_reg);

	/* Run a full measurement. (~14 ms) */
	raw_count_full = run_measurement(SAMPLE_TICKS_14_MS, meas->regmap,
				data->ctl_reg, data->status_reg);

	gcc_xo4_reg &= ~BIT(0);
	regmap_write(meas->regmap, data->xo_div4_cbcr, gcc_xo4_reg);

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

static int clk_find_and_set_parent(struct clk_hw *mux, struct clk_hw *clk)
{
	int i;

	if (!clk || !mux || !(mux->init->flags & CLK_IS_MEASURE))
		return -EINVAL;

	if (!clk_set_parent(mux->clk, clk->clk))
		return 0;

	for (i = 0; i < clk_hw_get_num_parents(mux); i++) {
		struct clk_hw *parent = clk_hw_get_parent_by_index(mux, i);

		if (!clk_find_and_set_parent(parent, clk))
			return clk_set_parent(mux->clk, parent->clk);
	}

	return -EINVAL;
}

static u8 clk_debug_mux_get_parent(struct clk_hw *hw)
{
	int i, num_parents = clk_hw_get_num_parents(hw);
	struct clk_hw *hw_clk = clk_hw_get_parent(hw);

	if (!hw_clk)
		return 0;
	for (i = 0; i < num_parents; i++) {
		if (!strcmp(hw->init->parent_names[i],
					clk_hw_get_name(hw_clk))) {
			pr_debug("%s: clock parent - %s, index %d\n", __func__,
					hw->init->parent_names[i], i);
			return i;
		}
	}
	return 0;
}

static int clk_debug_mux_set_parent(struct clk_hw *hw, u8 index)
{
	struct clk_debug_mux *mux = to_clk_measure(hw);
	int ret;

	if (!mux->mux_sels)
		return 0;

	/* Update the debug sel for mux */
	ret = regmap_update_bits(mux->regmap, mux->debug_offset,
		mux->src_sel_mask,
		mux->mux_sels[index] << mux->src_sel_shift);
	if (ret)
		return ret;

	/* Set the mux's post divider bits */
	return regmap_update_bits(mux->regmap, mux->post_div_offset,
		mux->post_div_mask,
		(mux->post_div_val - 1) << mux->post_div_shift);
}

const struct clk_ops clk_debug_mux_ops = {
	.get_parent = clk_debug_mux_get_parent,
	.set_parent = clk_debug_mux_set_parent,
};
EXPORT_SYMBOL(clk_debug_mux_ops);

static void enable_debug_clks(struct clk_hw *mux)
{
	struct clk_debug_mux *meas = to_clk_measure(mux);
	struct clk_hw *parent;

	if (!mux || !(mux->init->flags & CLK_IS_MEASURE))
		return;

	parent = clk_hw_get_parent(mux);
	enable_debug_clks(parent);

	meas->en_mask = meas->en_mask ? meas->en_mask : CBCR_ENA;

	/* Not all muxes have a DEBUG clock. */
	if (meas->cbcr_offset != U32_MAX)
		regmap_update_bits(meas->regmap, meas->cbcr_offset,
				   meas->en_mask, meas->en_mask);
}

static void disable_debug_clks(struct clk_hw *mux)
{
	struct clk_debug_mux *meas = to_clk_measure(mux);
	struct clk_hw *parent;

	if (!mux || !(mux->init->flags & CLK_IS_MEASURE))
		return;

	meas->en_mask = meas->en_mask ? meas->en_mask : CBCR_ENA;

	if (meas->cbcr_offset != U32_MAX)
		regmap_update_bits(meas->regmap, meas->cbcr_offset,
					meas->en_mask, 0);

	parent = clk_hw_get_parent(mux);
	disable_debug_clks(parent);
}

static u32 get_mux_divs(struct clk_hw *mux)
{
	struct clk_debug_mux *meas = to_clk_measure(mux);
	struct clk_hw *parent;
	u32 div_val;

	if (!mux || !(mux->init->flags & CLK_IS_MEASURE))
		return 1;

	WARN_ON(!meas->post_div_val);
	div_val = meas->post_div_val;

	if (meas->pre_div_vals) {
		int i = clk_debug_mux_get_parent(mux);

		div_val *= meas->pre_div_vals[i];
	}
	parent = clk_hw_get_parent(mux);
	return div_val * get_mux_divs(parent);
}

static int clk_debug_measure_get(void *data, u64 *val)
{
	struct clk_hw *hw = data;
	struct clk_debug_mux *meas = to_clk_measure(measure);
	int ret = 0;

	mutex_lock(&clk_debug_lock);

	/*
	 * Vote for bandwidth to re-connect config ports
	 * to multimedia clock controllers.
	 */
	if (meas->bus_cl_id)
		msm_bus_scale_client_update_request(meas->bus_cl_id, 1);

	ret = clk_find_and_set_parent(measure, hw);
	if (ret) {
		pr_err("Failed to set the debug mux's parent.\n");
		goto exit;
	}

	enable_debug_clks(measure);
	*val = clk_debug_mux_measure_rate(measure);

	/* recursively calculate actual freq */
	*val *= get_mux_divs(measure);
	disable_debug_clks(measure);
exit:
	if (meas->bus_cl_id)
		msm_bus_scale_client_update_request(meas->bus_cl_id, 0);
	mutex_unlock(&clk_debug_lock);
	return ret;
}

DEFINE_DEBUGFS_ATTRIBUTE(clk_measure_fops, clk_debug_measure_get,
							NULL, "%lld\n");

static int clk_debug_read_period(void *data, u64 *val)
{
	struct clk_hw *hw = data;
	struct clk_hw *parent;
	struct clk_debug_mux *mux;
	int ret = 0;
	u32 regval;

	mutex_lock(&clk_debug_lock);

	ret = clk_find_and_set_parent(measure, hw);
	if (!ret) {
		parent = clk_hw_get_parent(measure);
		if (!parent) {
			mutex_unlock(&clk_debug_lock);
			return -EINVAL;
		}
		mux = to_clk_measure(parent);
		regmap_read(mux->regmap, mux->period_offset, &regval);
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

void clk_debug_measure_add(struct clk_hw *hw, struct dentry *dentry)
{
	int ret;
	struct clk_hw *parent;
	struct clk_debug_mux *meas;
	struct clk_debug_mux *meas_parent;

	if (IS_ERR_OR_NULL(measure)) {
		pr_err_once("Please check if `measure` clk is registered.\n");
		return;
	}

	meas = to_clk_measure(measure);
	if (meas->bus_cl_id)
		msm_bus_scale_client_update_request(meas->bus_cl_id, 1);
	ret = clk_find_and_set_parent(measure, hw);
	if (ret) {
		pr_debug("Unable to set %s as %s's parent, ret=%d\n",
			clk_hw_get_name(hw), clk_hw_get_name(measure), ret);
		goto err;
	}

	parent = clk_hw_get_parent(measure);
	if (!parent)
		return;
	meas_parent = to_clk_measure(parent);

	if (parent->init->flags & CLK_IS_MEASURE && !meas_parent->mux_sels) {
		debugfs_create_file("clk_measure", 0444, dentry, hw,
				&clk_read_period_fops);
	}
	else
		debugfs_create_file("clk_measure", 0444, dentry, hw,
				&clk_measure_fops);
err:
	if (meas->bus_cl_id)
		msm_bus_scale_client_update_request(meas->bus_cl_id, 0);
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

/**
 * map_debug_bases - maps each debug mux based on phandle
 * @pdev: the platform device used to find phandles
 * @base: regmap base name used to look up phandle
 * @mux: debug mux that requires a regmap
 *
 * This function attempts to look up and map a regmap for a debug mux
 * using syscon_regmap_lookup_by_phandle if the base name property exists
 * and assigns an appropriate regmap.
 *
 * Returns 0 on success, -EBADR when it can't find base name, -EERROR otherwise.
 */
int map_debug_bases(struct platform_device *pdev, const char *base,
		    struct clk_debug_mux *mux)
{
	if (!of_get_property(pdev->dev.of_node, base, NULL))
		return -EBADR;

	mux->regmap = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
						     base);
	if (IS_ERR(mux->regmap)) {
		pr_err("Failed to map %s (ret=%ld)\n", base,
				PTR_ERR(mux->regmap));
		return PTR_ERR(mux->regmap);
	}
	return 0;
}
EXPORT_SYMBOL(map_debug_bases);
