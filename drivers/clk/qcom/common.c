/*
 * Copyright (c) 2013-2014, 2016-2017, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
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
#include <linux/reset-controller.h>
#include <linux/of.h>

#include "common.h"
#include "clk-rcg.h"
#include "clk-regmap.h"
#include "reset.h"
#include "gdsc.h"

struct qcom_cc {
	struct qcom_reset_controller reset;
	struct clk_onecell_data data;
	struct clk *clks[];
};

const
struct freq_tbl *qcom_find_freq(const struct freq_tbl *f, unsigned long rate)
{
	if (!f)
		return NULL;

	for (; f->freq; f++)
		if (rate <= f->freq)
			return f;

	/* Default to our fastest rate */
	return f - 1;
}
EXPORT_SYMBOL_GPL(qcom_find_freq);

int qcom_find_src_index(struct clk_hw *hw, const struct parent_map *map, u8 src)
{
	int i, num_parents = clk_hw_get_num_parents(hw);

	for (i = 0; i < num_parents; i++)
		if (src == map[i].src)
			return i;

	return -ENOENT;
}
EXPORT_SYMBOL_GPL(qcom_find_src_index);

struct regmap *
qcom_cc_map(struct platform_device *pdev, const struct qcom_cc_desc *desc)
{
	void __iomem *base;
	struct resource *res;
	struct device *dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(dev, res);
	if (IS_ERR(base))
		return ERR_CAST(base);

	return devm_regmap_init_mmio(dev, base, desc->config);
}
EXPORT_SYMBOL_GPL(qcom_cc_map);

static void qcom_cc_del_clk_provider(void *data)
{
	of_clk_del_provider(data);
}

static void qcom_cc_reset_unregister(void *data)
{
	reset_controller_unregister(data);
}

static void qcom_cc_gdsc_unregister(void *data)
{
	gdsc_unregister(data);
}

/*
 * Backwards compatibility with old DTs. Register a pass-through factor 1/1
 * clock to translate 'path' clk into 'name' clk and regsiter the 'path'
 * clk as a fixed rate clock if it isn't present.
 */
static int _qcom_cc_register_board_clk(struct device *dev, const char *path,
				       const char *name, unsigned long rate,
				       bool add_factor)
{
	struct device_node *node = NULL;
	struct device_node *clocks_node;
	struct clk_fixed_factor *factor;
	struct clk_fixed_rate *fixed;
	struct clk *clk;
	struct clk_init_data init_data = { };

	clocks_node = of_find_node_by_path("/clocks");
	if (clocks_node)
		node = of_find_node_by_name(clocks_node, path);
	of_node_put(clocks_node);

	if (!node) {
		fixed = devm_kzalloc(dev, sizeof(*fixed), GFP_KERNEL);
		if (!fixed)
			return -EINVAL;

		fixed->fixed_rate = rate;
		fixed->hw.init = &init_data;

		init_data.name = path;
		init_data.ops = &clk_fixed_rate_ops;

		clk = devm_clk_register(dev, &fixed->hw);
		if (IS_ERR(clk))
			return PTR_ERR(clk);
	}
	of_node_put(node);

	if (add_factor) {
		factor = devm_kzalloc(dev, sizeof(*factor), GFP_KERNEL);
		if (!factor)
			return -EINVAL;

		factor->mult = factor->div = 1;
		factor->hw.init = &init_data;

		init_data.name = name;
		init_data.parent_names = &path;
		init_data.num_parents = 1;
		init_data.flags = 0;
		init_data.ops = &clk_fixed_factor_ops;

		clk = devm_clk_register(dev, &factor->hw);
		if (IS_ERR(clk))
			return PTR_ERR(clk);
	}

	return 0;
}

int qcom_cc_register_board_clk(struct device *dev, const char *path,
			       const char *name, unsigned long rate)
{
	bool add_factor = true;
	struct device_node *node;

	/* The RPM clock driver will add the factor clock if present */
	if (IS_ENABLED(CONFIG_QCOM_RPMCC)) {
		node = of_find_compatible_node(NULL, NULL, "qcom,rpmcc");
		if (of_device_is_available(node))
			add_factor = false;
		of_node_put(node);
	}

	return _qcom_cc_register_board_clk(dev, path, name, rate, add_factor);
}
EXPORT_SYMBOL_GPL(qcom_cc_register_board_clk);

int qcom_cc_register_sleep_clk(struct device *dev)
{
	return _qcom_cc_register_board_clk(dev, "sleep_clk", "sleep_clk_src",
					   32768, true);
}
EXPORT_SYMBOL_GPL(qcom_cc_register_sleep_clk);

int qcom_cc_really_probe(struct platform_device *pdev,
			 const struct qcom_cc_desc *desc, struct regmap *regmap)
{
	int i = 0, ret, j = 0;
	struct device *dev = &pdev->dev;
	struct clk *clk;
	struct clk_onecell_data *data;
	struct clk **clks;
	struct qcom_reset_controller *reset;
	struct qcom_cc *cc;
	struct gdsc_desc *scd;
	size_t num_clks = desc->num_clks;
	struct clk_regmap **rclks = desc->clks;
	struct clk_hw **hw_clks = desc->hwclks;

	cc = devm_kzalloc(dev, sizeof(*cc) + sizeof(*clks) *
			(num_clks + desc->num_hwclks),
			  GFP_KERNEL);
	if (!cc)
		return -ENOMEM;

	clks = cc->clks;
	data = &cc->data;
	data->clks = clks;
	data->clk_num = num_clks + desc->num_hwclks;

	for (i = 0; i < desc->num_hwclks; i++) {
		if (!hw_clks[i]) {
			clks[i] = ERR_PTR(-ENOENT);
			continue;
		}
		clk = devm_clk_register(dev, hw_clks[i]);
		if (IS_ERR(clk))
			return PTR_ERR(clk);
		clks[i] = clk;
		pr_debug("Index for hw_clocks %d added %s\n", i,
							__clk_get_name(clk));
	}

	for (j = i; j < num_clks; j++) {
		if (!rclks[j]) {
			clks[j] = ERR_PTR(-ENOENT);
			continue;
		}
		clk = devm_clk_register_regmap(dev, rclks[j]);
		if (IS_ERR(clk))
			return PTR_ERR(clk);
		clks[j] = clk;
		pr_debug("Index for Regmap clocks %d added %s\n", j,
							__clk_get_name(clk));
	}

	ret = of_clk_add_provider(dev->of_node, of_clk_src_onecell_get, data);
	if (ret)
		return ret;

	ret = devm_add_action_or_reset(dev, qcom_cc_del_clk_provider,
				       pdev->dev.of_node);

	if (ret)
		return ret;

	reset = &cc->reset;
	reset->rcdev.of_node = dev->of_node;
	reset->rcdev.ops = &qcom_reset_ops;
	reset->rcdev.owner = dev->driver->owner;
	reset->rcdev.nr_resets = desc->num_resets;
	reset->regmap = regmap;
	reset->reset_map = desc->resets;

	ret = reset_controller_register(&reset->rcdev);
	if (ret)
		return ret;

	ret = devm_add_action_or_reset(dev, qcom_cc_reset_unregister,
				       &reset->rcdev);

	if (ret)
		return ret;

	if (desc->gdscs && desc->num_gdscs) {
		scd = devm_kzalloc(dev, sizeof(*scd), GFP_KERNEL);
		if (!scd)
			return -ENOMEM;
		scd->dev = dev;
		scd->scs = desc->gdscs;
		scd->num = desc->num_gdscs;
		ret = gdsc_register(scd, &reset->rcdev, regmap);
		if (ret)
			return ret;
		ret = devm_add_action_or_reset(dev, qcom_cc_gdsc_unregister,
					       scd);
		if (ret)
			return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(qcom_cc_really_probe);

int qcom_cc_probe(struct platform_device *pdev, const struct qcom_cc_desc *desc)
{
	struct regmap *regmap;

	regmap = qcom_cc_map(pdev, desc);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	return qcom_cc_really_probe(pdev, desc, regmap);
}
EXPORT_SYMBOL_GPL(qcom_cc_probe);

/* Debugfs Support */
static struct clk_hw *measure;

DEFINE_SPINLOCK(clk_reg_lock);

/* Sample clock for 'ticks' reference clock ticks. */
static u32 run_measurement(unsigned ticks, struct regmap *regmap,
		u32 ctl_reg, u32 status_reg)
{
	u32 regval;

	/* Stop counters and set the XO4 counter start value. */
	regmap_write(regmap, ctl_reg, ticks);

	regmap_read(regmap, status_reg, &regval);

	/* Wait for timer to become ready. */
	while ((regval & BIT(25)) != 0) {
		cpu_relax();
		regmap_read(regmap, status_reg, &regval);
	}

	/* Run measurement and wait for completion. */
	regmap_write(regmap, ctl_reg, (BIT(20)|ticks));
	regmap_read(regmap, ctl_reg, &regval);

	regmap_read(regmap, status_reg, &regval);

	while ((regval & BIT(25)) == 0) {
		cpu_relax();
		regmap_read(regmap, status_reg, &regval);
	}

	/* Return measured ticks. */
	regmap_read(regmap, status_reg, &regval);
	regval &= BM(24, 0);

	return regval;
}

/*
 * Perform a hardware rate measurement for a given clock.
 * FOR DEBUG USE ONLY: Measurements take ~15 ms!
 */
static unsigned long clk_debug_mux_measure_rate(struct clk_hw *hw)
{
	unsigned long flags, ret = 0;
	u32 gcc_xo4_reg, sample_ticks = 0x10000, multiplier = 1;
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
	raw_count_short = run_measurement(0x1000, meas->regmap[GCC],
					data->ctl_reg, data->status_reg);

	/* Run a full measurement. (~14 ms) */
	raw_count_full = run_measurement(sample_ticks, meas->regmap[GCC],
					data->ctl_reg, data->status_reg);

	gcc_xo4_reg &= ~BIT(0);
	regmap_write(meas->regmap[GCC], data->xo_div4_cbcr, gcc_xo4_reg);

	/* Return 0 if the clock is off. */
	if (raw_count_full == raw_count_short)
		ret = 0;
	else {
		/* Compute rate in Hz. */
		raw_count_full = ((raw_count_full * 10) + 15) * 4800000;
		do_div(raw_count_full, ((sample_ticks * 10) + 35));
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
			pr_debug("%s :Clock name %s index %d\n", __func__,
					hw->init->name, i);
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
		regmap_read(meas->regmap[dbg_cc], 0x0, &regval);

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
EXPORT_SYMBOL_GPL(clk_debug_mux_ops);

static int clk_debug_measure_get(void *data, u64 *val)
{
	struct clk_hw *hw = data, *par;
	int ret = 0;
	unsigned long meas_rate, sw_rate;

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

	return ret;
}

DEFINE_SIMPLE_ATTRIBUTE(clk_measure_fops, clk_debug_measure_get,
							NULL, "%lld\n");

void clk_debug_measure_add(struct clk_hw *hw, struct dentry *dentry)
{
	if (IS_ERR_OR_NULL(measure))
		return;

	if (clk_set_parent(measure->clk, hw->clk))
		return;

	debugfs_create_file("measure", S_IRUGO, dentry, hw,
					&clk_measure_fops);
}
EXPORT_SYMBOL_GPL(clk_debug_measure_add);

int clk_register_debug(struct clk_hw *hw, struct dentry *dentry)
{
	if (IS_ERR_OR_NULL(measure)) {
		if (hw->init->flags & CLK_IS_MEASURE)
			measure = hw;
		if (!IS_ERR_OR_NULL(measure))
			clk_debug_measure_add(hw, dentry);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(clk_register_debug);

MODULE_LICENSE("GPL v2");
