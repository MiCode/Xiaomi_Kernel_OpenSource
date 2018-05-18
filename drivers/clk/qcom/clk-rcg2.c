/*
 * Copyright (c) 2013, 2016-2018, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "clk: %s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/bug.h>
#include <linux/export.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/regmap.h>
#include <linux/rational.h>
#include <linux/math64.h>
#include <linux/clk.h>

#include <asm/div64.h>

#include "clk-rcg.h"
#include "common.h"

#define CMD_REG			0x0
#define CMD_UPDATE		BIT(0)
#define CMD_ROOT_EN		BIT(1)
#define CMD_DIRTY_CFG		BIT(4)
#define CMD_DIRTY_N		BIT(5)
#define CMD_DIRTY_M		BIT(6)
#define CMD_DIRTY_D		BIT(7)
#define CMD_ROOT_OFF		BIT(31)

#define CFG_REG			0x4
#define CFG_SRC_DIV_SHIFT	0
#define CFG_SRC_SEL_SHIFT	8
#define CFG_SRC_SEL_MASK	(0x7 << CFG_SRC_SEL_SHIFT)
#define CFG_MODE_SHIFT		12
#define CFG_MODE_MASK		(0x3 << CFG_MODE_SHIFT)
#define CFG_MODE_DUAL_EDGE	(0x2 << CFG_MODE_SHIFT)
#define CFG_HW_CLK_CTRL_MASK	BIT(20)

#define M_REG			0x8
#define N_REG			0xc
#define D_REG			0x10

/* Dynamic Frequency Scaling */
#define MAX_PERF_LEVEL		16
#define SE_CMD_DFSR_OFFSET	0x14
#define SE_CMD_DFS_EN		BIT(0)
#define SE_PERF_DFSR(level)	(0x1c + 0x4 * (level))
#define SE_PERF_M_DFSR(level)	(0x5c + 0x4 * (level))
#define SE_PERF_N_DFSR(level)	(0x9c + 0x4 * (level))

static struct freq_tbl cxo_f = {
	.freq = 19200000,
	.src = 0,
	.pre_div = 1,
	.m = 0,
	.n = 0,
};

static int clk_rcg2_is_enabled(struct clk_hw *hw)
{
	struct clk_rcg2 *rcg = to_clk_rcg2(hw);
	u32 cmd;
	int ret;

	ret = regmap_read(rcg->clkr.regmap, rcg->cmd_rcgr + CMD_REG, &cmd);
	if (ret)
		return ret;

	return (cmd & CMD_ROOT_OFF) == 0;
}

static u8 clk_rcg2_get_parent(struct clk_hw *hw)
{
	struct clk_rcg2 *rcg = to_clk_rcg2(hw);
	int num_parents = clk_hw_get_num_parents(hw);
	u32 cfg;
	int i, ret;

	ret = regmap_read(rcg->clkr.regmap, rcg->cmd_rcgr + CFG_REG, &cfg);
	if (ret)
		goto err;

	cfg &= CFG_SRC_SEL_MASK;
	cfg >>= CFG_SRC_SEL_SHIFT;

	for (i = 0; i < num_parents; i++)
		if (cfg == rcg->parent_map[i].cfg)
			return i;

err:
	pr_debug("Clock %s has invalid parent, using default.\n",
		 clk_hw_get_name(hw));
	return 0;
}

static int update_config(struct clk_rcg2 *rcg)
{
	int count, ret;
	u32 cmd;
	struct clk_hw *hw = &rcg->clkr.hw;
	const char *name = clk_hw_get_name(hw);

	ret = regmap_update_bits(rcg->clkr.regmap, rcg->cmd_rcgr + CMD_REG,
				 CMD_UPDATE, CMD_UPDATE);
	if (ret)
		return ret;

	/* Wait for update to take effect */
	for (count = 500; count > 0; count--) {
		ret = regmap_read(rcg->clkr.regmap, rcg->cmd_rcgr + CMD_REG, &cmd);
		if (ret)
			return ret;
		if (!(cmd & CMD_UPDATE))
			return 0;
		udelay(1);
	}

	WARN(1, "clk: %s: rcg didn't update its configuration.", name);
	return 0;
}

static int clk_rcg2_set_parent(struct clk_hw *hw, u8 index)
{
	struct clk_rcg2 *rcg = to_clk_rcg2(hw);
	int ret;
	u32 cfg = rcg->parent_map[index].cfg << CFG_SRC_SEL_SHIFT;

	ret = regmap_update_bits(rcg->clkr.regmap, rcg->cmd_rcgr + CFG_REG,
				 CFG_SRC_SEL_MASK, cfg);
	if (ret)
		return ret;

	return update_config(rcg);
}

static int clk_rcg2_set_force_enable(struct clk_hw *hw)
{
	struct clk_rcg2 *rcg = to_clk_rcg2(hw);
	int ret = 0, count = 500;

	ret = regmap_update_bits(rcg->clkr.regmap, rcg->cmd_rcgr + CMD_REG,
					CMD_ROOT_EN, CMD_ROOT_EN);
	if (ret)
		return ret;

	for (; count > 0; count--) {
		if (clk_rcg2_is_enabled(hw))
			return ret;
		/* Delay for 1usec and retry polling the status bit */
		udelay(1);
	}

	WARN(1, "clk: %s: rcg didn't turn on.", clk_hw_get_name(hw));
	return ret;
}

static void clk_rcg2_clear_force_enable(struct clk_hw *hw)
{
	struct clk_rcg2 *rcg = to_clk_rcg2(hw);

	regmap_update_bits(rcg->clkr.regmap, rcg->cmd_rcgr + CMD_REG,
					CMD_ROOT_EN, 0);
}

static int prepare_enable_rcg_srcs(struct clk *curr, struct clk *new)
{
	int rc = 0;

	rc = clk_prepare(curr);
	if (rc)
		return rc;

	rc = clk_prepare(new);
	if (rc)
		goto err_new_src_prepare;

	rc = clk_enable(curr);
	if (rc)
		goto err_curr_src_enable;

	rc = clk_enable(new);
	if (rc)
		goto err_new_src_enable;

	return rc;

err_new_src_enable:
	clk_disable(curr);
err_curr_src_enable:
	clk_unprepare(new);
err_new_src_prepare:
	clk_unprepare(curr);

	return rc;
}

static void disable_unprepare_rcg_srcs(struct clk *curr, struct clk *new)
{
	clk_disable(new);
	clk_disable(curr);

	clk_unprepare(new);
	clk_unprepare(curr);
}

/*
 * Calculate m/n:d rate
 *
 *          parent_rate     m
 *   rate = ----------- x  ---
 *            hid_div       n
 */
unsigned long
clk_rcg2_calc_rate(unsigned long rate, u32 m, u32 n, u32 mode, u32 hid_div)
{
	if (hid_div) {
		rate *= 2;
		rate /= hid_div + 1;
	}

	if (mode) {
		u64 tmp = rate;
		tmp *= m;
		do_div(tmp, n);
		rate = tmp;
	}

	return rate;
}
EXPORT_SYMBOL(clk_rcg2_calc_rate);

static unsigned long
clk_rcg2_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	struct clk_rcg2 *rcg = to_clk_rcg2(hw);
	const struct freq_tbl *f_curr;
	u32 cfg, hid_div, m = 0, n = 0, mode = 0, mask;

	if (rcg->flags & DFS_ENABLE_RCG)
		return rcg->current_freq;

	if (rcg->enable_safe_config && (!clk_hw_is_prepared(hw)
				|| !clk_hw_is_enabled(hw))) {
		if (!rcg->current_freq)
			rcg->current_freq = cxo_f.freq;
		return rcg->current_freq;
	}

	regmap_read(rcg->clkr.regmap, rcg->cmd_rcgr + CFG_REG, &cfg);

	if (rcg->mnd_width) {
		mask = BIT(rcg->mnd_width) - 1;
		regmap_read(rcg->clkr.regmap, rcg->cmd_rcgr + M_REG, &m);
		m &= mask;
		regmap_read(rcg->clkr.regmap, rcg->cmd_rcgr + N_REG, &n);
		n =  ~n;
		n &= mask;
		n += m;
		mode = cfg & CFG_MODE_MASK;
		mode >>= CFG_MODE_SHIFT;
	}

	if (rcg->enable_safe_config) {
		f_curr = qcom_find_freq(rcg->freq_tbl, rcg->current_freq);
		if (!f_curr)
			return -EINVAL;

		hid_div = f_curr->pre_div;
	} else {
		mask = BIT(rcg->hid_width) - 1;
		hid_div = cfg >> CFG_SRC_DIV_SHIFT;
		hid_div &= mask;
	}

	return clk_rcg2_calc_rate(parent_rate, m, n, mode, hid_div);
}

static int _freq_tbl_determine_rate(struct clk_hw *hw,
		const struct freq_tbl *f, struct clk_rate_request *req)
{
	unsigned long clk_flags, rate = req->rate;
	struct clk_hw *p;
	struct clk_rcg2 *rcg = to_clk_rcg2(hw);
	int index;

	f = qcom_find_freq(f, rate);
	if (!f)
		return -EINVAL;

	index = qcom_find_src_index(hw, rcg->parent_map, f->src);
	if (index < 0)
		return index;

	clk_flags = clk_hw_get_flags(hw);
	p = clk_hw_get_parent_by_index(hw, index);
	if (clk_flags & CLK_SET_RATE_PARENT) {
		if (f->pre_div) {
			rate /= 2;
			rate *= f->pre_div + 1;
		}

		if (f->n) {
			u64 tmp = rate;
			tmp = tmp * f->n;
			do_div(tmp, f->m);
			rate = tmp;
		}
	} else {
		rate =  clk_hw_get_rate(p);
	}
	req->best_parent_hw = p;
	req->best_parent_rate = rate;
	req->rate = f->freq;

	return 0;
}

static int clk_rcg2_determine_rate(struct clk_hw *hw,
				   struct clk_rate_request *req)
{
	struct clk_rcg2 *rcg = to_clk_rcg2(hw);

	return _freq_tbl_determine_rate(hw, rcg->freq_tbl, req);
}

static int clk_rcg2_configure(struct clk_rcg2 *rcg, const struct freq_tbl *f)
{
	u32 cfg, mask;
	struct clk_hw *hw = &rcg->clkr.hw;
	int ret, index = qcom_find_src_index(hw, rcg->parent_map, f->src);

	/* Skip configuration if DFS control has been enabled for the RCG. */
	if (rcg->flags & DFS_ENABLE_RCG)
		return 0;

	if (index < 0)
		return index;

	if (rcg->mnd_width && f->n) {
		mask = BIT(rcg->mnd_width) - 1;
		ret = regmap_update_bits(rcg->clkr.regmap,
				rcg->cmd_rcgr + M_REG, mask, f->m);
		if (ret)
			return ret;

		ret = regmap_update_bits(rcg->clkr.regmap,
				rcg->cmd_rcgr + N_REG, mask, ~(f->n - f->m));
		if (ret)
			return ret;

		ret = regmap_update_bits(rcg->clkr.regmap,
				rcg->cmd_rcgr + D_REG, mask, ~f->n);
		if (ret)
			return ret;
	}

	mask = BIT(rcg->hid_width) - 1;
	mask |= CFG_SRC_SEL_MASK | CFG_MODE_MASK | CFG_HW_CLK_CTRL_MASK;
	cfg = f->pre_div << CFG_SRC_DIV_SHIFT;
	cfg |= rcg->parent_map[index].cfg << CFG_SRC_SEL_SHIFT;
	if (rcg->mnd_width && f->n && (f->m != f->n))
		cfg |= CFG_MODE_DUAL_EDGE;
	ret = regmap_update_bits(rcg->clkr.regmap,
			rcg->cmd_rcgr + CFG_REG, mask, cfg);
	if (ret)
		return ret;

	return update_config(rcg);
}

static void clk_rcg2_list_registers(struct seq_file *f, struct clk_hw *hw)
{
	struct clk_rcg2 *rcg = to_clk_rcg2(hw);
	int i = 0, size = 0, val;

	static struct clk_register_data data[] = {
		{"CMD_RCGR", 0x0},
		{"CFG_RCGR", 0x4},
	};

	static struct clk_register_data data1[] = {
		{"CMD_RCGR", 0x0},
		{"CFG_RCGR", 0x4},
		{"M_VAL", 0x8},
		{"N_VAL", 0xC},
		{"D_VAL", 0x10},
	};

	if (rcg->mnd_width) {
		size = ARRAY_SIZE(data1);
		for (i = 0; i < size; i++) {
			regmap_read(rcg->clkr.regmap, (rcg->cmd_rcgr +
					data1[i].offset), &val);
			seq_printf(f, "%20s: 0x%.8x\n",	data1[i].name, val);
		}
	} else {
		size = ARRAY_SIZE(data);
		for (i = 0; i < size; i++) {
			regmap_read(rcg->clkr.regmap, (rcg->cmd_rcgr +
				data[i].offset), &val);
			seq_printf(f, "%20s: 0x%.8x\n",	data[i].name, val);
		}
	}
}

/* Return the nth supported frequency for a given clock. */
static long clk_rcg2_list_rate(struct clk_hw *hw, unsigned int n,
		unsigned long fmax)
{
	struct clk_rcg2 *rcg = to_clk_rcg2(hw);

	if (!rcg->freq_tbl)
		return -ENXIO;

	return (rcg->freq_tbl + n)->freq;
}

static int __clk_rcg2_set_rate(struct clk_hw *hw, unsigned long rate)
{
	struct clk_rcg2 *rcg = to_clk_rcg2(hw);
	const struct freq_tbl *f, *f_curr;
	int ret, curr_src_index, new_src_index;
	struct clk_hw *curr_src = NULL, *new_src = NULL;

	f = qcom_find_freq(rcg->freq_tbl, rate);
	if (!f)
		return -EINVAL;

	/*
	 * Return if the RCG is currently disabled. This configuration update
	 * will happen as part of the RCG enable sequence.
	 */
	if (rcg->enable_safe_config && !clk_hw_is_prepared(hw)) {
		rcg->current_freq = rate;
		return 0;
	}

	if (rcg->flags & FORCE_ENABLE_RCG) {
		rcg->current_freq = clk_get_rate(hw->clk);
		if (rcg->current_freq == cxo_f.freq)
			curr_src_index = 0;
		else {
			f_curr = qcom_find_freq(rcg->freq_tbl,
							rcg->current_freq);
			if (!f_curr)
				return -EINVAL;

			curr_src_index = qcom_find_src_index(hw,
						rcg->parent_map, f_curr->src);
		}

		new_src_index = qcom_find_src_index(hw, rcg->parent_map,
							f->src);

		curr_src = clk_hw_get_parent_by_index(hw, curr_src_index);
		new_src = clk_hw_get_parent_by_index(hw, new_src_index);

		/* The RCG could currently be disabled. Enable its parents. */
		ret = prepare_enable_rcg_srcs(curr_src->clk, new_src->clk);
		clk_rcg2_set_force_enable(hw);
	}

	ret = clk_rcg2_configure(rcg, f);
	if (ret)
		return ret;

	if (rcg->flags & FORCE_ENABLE_RCG) {
		clk_rcg2_clear_force_enable(hw);
		disable_unprepare_rcg_srcs(curr_src->clk, new_src->clk);
	}

	/* Update current frequency with the requested frequency. */
	rcg->current_freq = rate;
	return ret;
}

static int clk_rcg2_set_rate(struct clk_hw *hw, unsigned long rate,
			    unsigned long parent_rate)
{
	return __clk_rcg2_set_rate(hw, rate);
}

static int clk_rcg2_set_rate_and_parent(struct clk_hw *hw,
		unsigned long rate, unsigned long parent_rate, u8 index)
{
	return __clk_rcg2_set_rate(hw, rate);
}

static int clk_rcg2_enable(struct clk_hw *hw)
{
	struct clk_rcg2 *rcg = to_clk_rcg2(hw);
	unsigned long rate;
	const struct freq_tbl *f;

	if (rcg->flags & FORCE_ENABLE_RCG) {
		clk_rcg2_set_force_enable(hw);
		return 0;
	}

	if (!rcg->enable_safe_config)
		return 0;

	/*
	 * Switch from CXO to the stashed mux selection. Force enable and
	 * disable the RCG while configuring it to safeguard against any update
	 * signal coming from the downstream clock. The current parent has
	 * already been prepared and enabled at this point, and the CXO source
	 * is always on while APPS is online. Therefore, the RCG can safely be
	 * switched.
	 */
	rate = rcg->current_freq;
	f = qcom_find_freq(rcg->freq_tbl, rate);
	if (!f)
		return -EINVAL;

	/*
	 * If CXO is not listed as a supported frequency in the frequency
	 * table, the above API would return the lowest supported frequency
	 * instead. This will lead to incorrect configuration of the RCG.
	 * Check if the RCG rate is CXO and configure it accordingly.
	 */
	if (rate == cxo_f.freq)
		f = &cxo_f;

	clk_rcg2_set_force_enable(hw);
	clk_rcg2_configure(rcg, f);
	clk_rcg2_clear_force_enable(hw);

	return 0;
}

static void clk_rcg2_disable(struct clk_hw *hw)
{
	struct clk_rcg2 *rcg = to_clk_rcg2(hw);

	if (rcg->flags & FORCE_ENABLE_RCG) {
		clk_rcg2_clear_force_enable(hw);
		return;
	}

	if (!rcg->enable_safe_config)
		return;
	/*
	 * Park the RCG at a safe configuration - sourced off the CXO. This is
	 * needed for 2 reasons: In the case of RCGs sourcing PSCBCs, due to a
	 * default HW behavior, the RCG will turn on when its corresponding
	 * GDSC is enabled. We might also have cases when the RCG might be left
	 * enabled without the overlying SW knowing about it. This results from
	 * hard to track cases of downstream clocks being left enabled. In both
	 * these cases, scaling the RCG will fail since it's enabled but with
	 * its sources cut off.
	 *
	 * Save mux select and switch to CXO. Force enable and disable the RCG
	 * while configuring it to safeguard against any update signal coming
	 * from the downstream clock. The current parent is still prepared and
	 * enabled at this point, and the CXO source is always on while APPS is
	 * online. Therefore, the RCG can safely be switched.
	 */
	clk_rcg2_set_force_enable(hw);
	clk_rcg2_configure(rcg, &cxo_f);
	clk_rcg2_clear_force_enable(hw);
}

const struct clk_ops clk_rcg2_ops = {
	.is_enabled = clk_rcg2_is_enabled,
	.enable = clk_rcg2_enable,
	.disable = clk_rcg2_disable,
	.get_parent = clk_rcg2_get_parent,
	.set_parent = clk_rcg2_set_parent,
	.recalc_rate = clk_rcg2_recalc_rate,
	.determine_rate = clk_rcg2_determine_rate,
	.set_rate = clk_rcg2_set_rate,
	.set_rate_and_parent = clk_rcg2_set_rate_and_parent,
	.list_rate = clk_rcg2_list_rate,
	.list_registers = clk_rcg2_list_registers,
};
EXPORT_SYMBOL_GPL(clk_rcg2_ops);

static int clk_rcg2_shared_force_enable(struct clk_hw *hw, unsigned long rate)
{
	struct clk_rcg2 *rcg = to_clk_rcg2(hw);
	const char *name = clk_hw_get_name(hw);
	int ret, count;

	/* force enable RCG */
	ret = regmap_update_bits(rcg->clkr.regmap, rcg->cmd_rcgr + CMD_REG,
				 CMD_ROOT_EN, CMD_ROOT_EN);
	if (ret)
		return ret;

	/* wait for RCG to turn ON */
	for (count = 500; count > 0; count--) {
		ret = clk_rcg2_is_enabled(hw);
		if (ret)
			break;
		udelay(1);
	}
	if (!count)
		pr_err("%s: RCG did not turn on\n", name);

	/* set clock rate */
	ret = __clk_rcg2_set_rate(hw, rate);
	if (ret)
		return ret;

	/* clear force enable RCG */
	return regmap_update_bits(rcg->clkr.regmap, rcg->cmd_rcgr + CMD_REG,
				 CMD_ROOT_EN, 0);
}

static int clk_rcg2_shared_set_rate(struct clk_hw *hw, unsigned long rate,
				    unsigned long parent_rate)
{
	struct clk_rcg2 *rcg = to_clk_rcg2(hw);

	/* cache the rate */
	rcg->current_freq = rate;

	if (!__clk_is_enabled(hw->clk))
		return 0;

	return clk_rcg2_shared_force_enable(hw, rcg->current_freq);
}

static unsigned long
clk_rcg2_shared_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	struct clk_rcg2 *rcg = to_clk_rcg2(hw);

	return rcg->current_freq = clk_rcg2_recalc_rate(hw, parent_rate);
}

static int clk_rcg2_shared_enable(struct clk_hw *hw)
{
	struct clk_rcg2 *rcg = to_clk_rcg2(hw);

	return clk_rcg2_shared_force_enable(hw, rcg->current_freq);
}

static void clk_rcg2_shared_disable(struct clk_hw *hw)
{
	struct clk_rcg2 *rcg = to_clk_rcg2(hw);

	/* switch to XO, which is the lowest entry in the freq table */
	clk_rcg2_shared_set_rate(hw, rcg->freq_tbl[0].freq, 0);
}

const struct clk_ops clk_rcg2_shared_ops = {
	.enable = clk_rcg2_shared_enable,
	.disable = clk_rcg2_shared_disable,
	.get_parent = clk_rcg2_get_parent,
	.recalc_rate = clk_rcg2_shared_recalc_rate,
	.determine_rate = clk_rcg2_determine_rate,
	.set_rate = clk_rcg2_shared_set_rate,
};
EXPORT_SYMBOL_GPL(clk_rcg2_shared_ops);

struct frac_entry {
	int num;
	int den;
};

static const struct frac_entry frac_table_675m[] = {	/* link rate of 270M */
	{ 52, 295 },	/* 119 M */
	{ 11, 57 },	/* 130.25 M */
	{ 63, 307 },	/* 138.50 M */
	{ 11, 50 },	/* 148.50 M */
	{ 47, 206 },	/* 154 M */
	{ 31, 100 },	/* 205.25 M */
	{ 107, 269 },	/* 268.50 M */
	{ },
};

static struct frac_entry frac_table_810m[] = { /* Link rate of 162M */
	{ 31, 211 },	/* 119 M */
	{ 32, 199 },	/* 130.25 M */
	{ 63, 307 },	/* 138.50 M */
	{ 11, 60 },	/* 148.50 M */
	{ 50, 263 },	/* 154 M */
	{ 31, 120 },	/* 205.25 M */
	{ 119, 359 },	/* 268.50 M */
	{ },
};

static int clk_edp_pixel_set_rate(struct clk_hw *hw, unsigned long rate,
			      unsigned long parent_rate)
{
	struct clk_rcg2 *rcg = to_clk_rcg2(hw);
	struct freq_tbl f = *rcg->freq_tbl;
	const struct frac_entry *frac;
	int delta = 100000;
	s64 src_rate = parent_rate;
	s64 request;
	u32 mask = BIT(rcg->hid_width) - 1;
	u32 hid_div;

	if (src_rate == 810000000)
		frac = frac_table_810m;
	else
		frac = frac_table_675m;

	for (; frac->num; frac++) {
		request = rate;
		request *= frac->den;
		request = div_s64(request, frac->num);
		if ((src_rate < (request - delta)) ||
		    (src_rate > (request + delta)))
			continue;

		regmap_read(rcg->clkr.regmap, rcg->cmd_rcgr + CFG_REG,
				&hid_div);
		f.pre_div = hid_div;
		f.pre_div >>= CFG_SRC_DIV_SHIFT;
		f.pre_div &= mask;
		f.m = frac->num;
		f.n = frac->den;

		return clk_rcg2_configure(rcg, &f);
	}

	return -EINVAL;
}

static int clk_edp_pixel_set_rate_and_parent(struct clk_hw *hw,
		unsigned long rate, unsigned long parent_rate, u8 index)
{
	/* Parent index is set statically in frequency table */
	return clk_edp_pixel_set_rate(hw, rate, parent_rate);
}

static int clk_edp_pixel_determine_rate(struct clk_hw *hw,
					struct clk_rate_request *req)
{
	struct clk_rcg2 *rcg = to_clk_rcg2(hw);
	const struct freq_tbl *f = rcg->freq_tbl;
	const struct frac_entry *frac;
	int delta = 100000;
	s64 request;
	u32 mask = BIT(rcg->hid_width) - 1;
	u32 hid_div;
	int index = qcom_find_src_index(hw, rcg->parent_map, f->src);

	/* Force the correct parent */
	req->best_parent_hw = clk_hw_get_parent_by_index(hw, index);
	req->best_parent_rate = clk_hw_get_rate(req->best_parent_hw);

	if (req->best_parent_rate == 810000000)
		frac = frac_table_810m;
	else
		frac = frac_table_675m;

	for (; frac->num; frac++) {
		request = req->rate;
		request *= frac->den;
		request = div_s64(request, frac->num);
		if ((req->best_parent_rate < (request - delta)) ||
		    (req->best_parent_rate > (request + delta)))
			continue;

		regmap_read(rcg->clkr.regmap, rcg->cmd_rcgr + CFG_REG,
				&hid_div);
		hid_div >>= CFG_SRC_DIV_SHIFT;
		hid_div &= mask;

		req->rate = clk_rcg2_calc_rate(req->best_parent_rate,
				      frac->num, frac->den,
				      !!frac->den, hid_div);
		return 0;
	}

	return -EINVAL;
}

const struct clk_ops clk_edp_pixel_ops = {
	.is_enabled = clk_rcg2_is_enabled,
	.get_parent = clk_rcg2_get_parent,
	.set_parent = clk_rcg2_set_parent,
	.recalc_rate = clk_rcg2_recalc_rate,
	.set_rate = clk_edp_pixel_set_rate,
	.set_rate_and_parent = clk_edp_pixel_set_rate_and_parent,
	.determine_rate = clk_edp_pixel_determine_rate,
	.list_registers = clk_rcg2_list_registers,
};
EXPORT_SYMBOL_GPL(clk_edp_pixel_ops);

static int clk_byte_determine_rate(struct clk_hw *hw,
				   struct clk_rate_request *req)
{
	struct clk_rcg2 *rcg = to_clk_rcg2(hw);
	const struct freq_tbl *f = rcg->freq_tbl;
	int index = qcom_find_src_index(hw, rcg->parent_map, f->src);
	unsigned long parent_rate, div;
	u32 mask = BIT(rcg->hid_width) - 1;
	struct clk_hw *p;

	if (req->rate == 0)
		return -EINVAL;

	req->best_parent_hw = p = clk_hw_get_parent_by_index(hw, index);
	req->best_parent_rate = parent_rate = clk_hw_round_rate(p, req->rate);

	div = DIV_ROUND_UP((2 * parent_rate), req->rate) - 1;
	div = min_t(u32, div, mask);

	req->rate = clk_rcg2_calc_rate(parent_rate, 0, 0, 0, div);

	return 0;
}

static int clk_byte_set_rate(struct clk_hw *hw, unsigned long rate,
			 unsigned long parent_rate)
{
	struct clk_rcg2 *rcg = to_clk_rcg2(hw);
	struct freq_tbl f = *rcg->freq_tbl;
	unsigned long div;
	u32 mask = BIT(rcg->hid_width) - 1;

	div = DIV_ROUND_UP((2 * parent_rate), rate) - 1;
	div = min_t(u32, div, mask);

	f.pre_div = div;

	return clk_rcg2_configure(rcg, &f);
}

static int clk_byte_set_rate_and_parent(struct clk_hw *hw,
		unsigned long rate, unsigned long parent_rate, u8 index)
{
	/* Parent index is set statically in frequency table */
	return clk_byte_set_rate(hw, rate, parent_rate);
}

const struct clk_ops clk_byte_ops = {
	.is_enabled = clk_rcg2_is_enabled,
	.get_parent = clk_rcg2_get_parent,
	.set_parent = clk_rcg2_set_parent,
	.recalc_rate = clk_rcg2_recalc_rate,
	.set_rate = clk_byte_set_rate,
	.set_rate_and_parent = clk_byte_set_rate_and_parent,
	.determine_rate = clk_byte_determine_rate,
	.list_registers = clk_rcg2_list_registers,
};
EXPORT_SYMBOL_GPL(clk_byte_ops);

static int clk_byte2_determine_rate(struct clk_hw *hw,
				    struct clk_rate_request *req)
{
	struct clk_rcg2 *rcg = to_clk_rcg2(hw);
	unsigned long parent_rate, div;
	u32 mask = BIT(rcg->hid_width) - 1;
	struct clk_hw *p;
	unsigned long rate = req->rate;

	if (rate == 0)
		return -EINVAL;

	p = req->best_parent_hw;
	req->best_parent_rate = parent_rate = clk_hw_round_rate(p, rate);

	div = DIV_ROUND_UP((2 * parent_rate), rate) - 1;
	div = min_t(u32, div, mask);

	req->rate = clk_rcg2_calc_rate(parent_rate, 0, 0, 0, div);

	return 0;
}

static int clk_byte2_set_rate(struct clk_hw *hw, unsigned long rate,
			 unsigned long parent_rate)
{
	struct clk_rcg2 *rcg = to_clk_rcg2(hw);
	struct freq_tbl f = { 0 };
	unsigned long div;
	int i, num_parents = clk_hw_get_num_parents(hw);
	u32 mask = BIT(rcg->hid_width) - 1;
	u32 cfg;

	div = DIV_ROUND_UP((2 * parent_rate), rate) - 1;
	div = min_t(u32, div, mask);

	f.pre_div = div;

	regmap_read(rcg->clkr.regmap, rcg->cmd_rcgr + CFG_REG, &cfg);
	cfg &= CFG_SRC_SEL_MASK;
	cfg >>= CFG_SRC_SEL_SHIFT;

	for (i = 0; i < num_parents; i++) {
		if (cfg == rcg->parent_map[i].cfg) {
			f.src = rcg->parent_map[i].src;
			return clk_rcg2_configure(rcg, &f);
		}
	}

	return -EINVAL;
}

static int clk_byte2_set_rate_and_parent(struct clk_hw *hw,
		unsigned long rate, unsigned long parent_rate, u8 index)
{
	/* Read the hardware to determine parent during set_rate */
	return clk_byte2_set_rate(hw, rate, parent_rate);
}

const struct clk_ops clk_byte2_ops = {
	.is_enabled = clk_rcg2_is_enabled,
	.get_parent = clk_rcg2_get_parent,
	.set_parent = clk_rcg2_set_parent,
	.recalc_rate = clk_rcg2_recalc_rate,
	.set_rate = clk_byte2_set_rate,
	.set_rate_and_parent = clk_byte2_set_rate_and_parent,
	.determine_rate = clk_byte2_determine_rate,
	.list_registers = clk_rcg2_list_registers,
};
EXPORT_SYMBOL_GPL(clk_byte2_ops);

static const struct frac_entry frac_table_pixel[] = {
	{ 1, 1 },
	{ 2, 3 },
	{ 4, 9 },
	{ 3, 8 },
	{ 2, 9 },
	{ }
};

static int clk_pixel_determine_rate(struct clk_hw *hw,
				    struct clk_rate_request *req)
{
	unsigned long request, src_rate;
	int delta = 100000;
	const struct frac_entry *frac = frac_table_pixel;

	for (; frac->num; frac++) {
		request = (req->rate * frac->den) / frac->num;

		src_rate = clk_hw_round_rate(req->best_parent_hw, request);
		if ((src_rate < (request - delta)) ||
			(src_rate > (request + delta)))
			continue;

		req->best_parent_rate = src_rate;
		req->rate = (src_rate * frac->num) / frac->den;
		return 0;
	}

	return -EINVAL;
}

static int clk_pixel_set_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long parent_rate)
{
	struct clk_rcg2 *rcg = to_clk_rcg2(hw);
	struct freq_tbl f = { 0 };
	const struct frac_entry *frac = frac_table_pixel;
	unsigned long request;
	int delta = 100000;
	u32 mask = BIT(rcg->hid_width) - 1;
	u32 hid_div, cfg;
	int i, num_parents = clk_hw_get_num_parents(hw);

	regmap_read(rcg->clkr.regmap, rcg->cmd_rcgr + CFG_REG, &cfg);
	cfg &= CFG_SRC_SEL_MASK;
	cfg >>= CFG_SRC_SEL_SHIFT;

	for (i = 0; i < num_parents; i++)
		if (cfg == rcg->parent_map[i].cfg) {
			f.src = rcg->parent_map[i].src;
			break;
		}

	for (; frac->num; frac++) {
		request = (rate * frac->den) / frac->num;

		if ((parent_rate < (request - delta)) ||
			(parent_rate > (request + delta)))
			continue;

		regmap_read(rcg->clkr.regmap, rcg->cmd_rcgr + CFG_REG,
				&hid_div);
		f.pre_div = hid_div;
		f.pre_div >>= CFG_SRC_DIV_SHIFT;
		f.pre_div &= mask;
		f.m = frac->num;
		f.n = frac->den;

		return clk_rcg2_configure(rcg, &f);
	}
	return -EINVAL;
}

static int clk_pixel_set_rate_and_parent(struct clk_hw *hw, unsigned long rate,
		unsigned long parent_rate, u8 index)
{
	return clk_pixel_set_rate(hw, rate, parent_rate);
}

const struct clk_ops clk_pixel_ops = {
	.is_enabled = clk_rcg2_is_enabled,
	.get_parent = clk_rcg2_get_parent,
	.set_parent = clk_rcg2_set_parent,
	.recalc_rate = clk_rcg2_recalc_rate,
	.set_rate = clk_pixel_set_rate,
	.set_rate_and_parent = clk_pixel_set_rate_and_parent,
	.determine_rate = clk_pixel_determine_rate,
	.list_registers = clk_rcg2_list_registers,
};
EXPORT_SYMBOL_GPL(clk_pixel_ops);

static int clk_dp_set_rate(struct clk_hw *hw, unsigned long rate,
			unsigned long parent_rate)
{
	struct clk_rcg2 *rcg = to_clk_rcg2(hw);
	struct clk_hw *parent = clk_hw_get_parent(hw);
	struct freq_tbl f = { 0 };
	unsigned long src_rate;
	unsigned long num, den;
	u32 mask = BIT(rcg->hid_width) - 1;
	u32 hid_div, cfg;
	int i, num_parents = clk_hw_get_num_parents(hw);

	if (!parent) {
		pr_err("RCG parent isn't initialized\n");
		return -EINVAL;
	}

	src_rate = clk_get_rate(parent->clk);
	if (src_rate <= 0) {
		pr_err("Invalid RCG parent rate\n");
		return -EINVAL;
	}

	rational_best_approximation(src_rate, rate,
			(unsigned long)(1 << 16) - 1,
			(unsigned long)(1 << 16) - 1, &den, &num);

	if (!num || !den) {
		pr_err("Invalid MN values derived for requested rate %lu\n",
							rate);
		return -EINVAL;
	}

	regmap_read(rcg->clkr.regmap, rcg->cmd_rcgr + CFG_REG, &cfg);
	hid_div = cfg;
	cfg &= CFG_SRC_SEL_MASK;
	cfg >>= CFG_SRC_SEL_SHIFT;

	for (i = 0; i < num_parents; i++)
		if (cfg == rcg->parent_map[i].cfg) {
			f.src = rcg->parent_map[i].src;
			break;
	}

	f.pre_div = hid_div;
	f.pre_div >>= CFG_SRC_DIV_SHIFT;
	f.pre_div &= mask;

	if (num == den) {
		f.m = 0;
		f.n = 0;
	} else {
		f.m = num;
		f.n = den;
	}

	return clk_rcg2_configure(rcg, &f);
}

static int clk_dp_set_rate_and_parent(struct clk_hw *hw, unsigned long rate,
		unsigned long parent_rate, u8 index)
{
	return clk_dp_set_rate(hw, rate, parent_rate);
}

static int clk_dp_determine_rate(struct clk_hw *hw,
		struct clk_rate_request *req)
{
	req->best_parent_rate = clk_hw_round_rate(req->best_parent_hw,
							req->best_parent_rate);
	return 0;
}

const struct clk_ops clk_dp_ops = {
	.is_enabled = clk_rcg2_is_enabled,
	.get_parent = clk_rcg2_get_parent,
	.set_parent = clk_rcg2_set_parent,
	.recalc_rate = clk_rcg2_recalc_rate,
	.set_rate = clk_dp_set_rate,
	.set_rate_and_parent = clk_dp_set_rate_and_parent,
	.determine_rate = clk_dp_determine_rate,
	.list_registers = clk_rcg2_list_registers,
};
EXPORT_SYMBOL_GPL(clk_dp_ops);

static int clk_gfx3d_determine_rate(struct clk_hw *hw,
				    struct clk_rate_request *req)
{
	struct clk_rate_request parent_req = { };
	struct clk_hw *p2, *p8, *p9, *xo;
	unsigned long p9_rate;
	int ret;

	xo = clk_hw_get_parent_by_index(hw, 0);
	if (req->rate == clk_hw_get_rate(xo)) {
		req->best_parent_hw = xo;
		return 0;
	}

	p9 = clk_hw_get_parent_by_index(hw, 2);
	p2 = clk_hw_get_parent_by_index(hw, 3);
	p8 = clk_hw_get_parent_by_index(hw, 4);

	/* PLL9 is a fixed rate PLL */
	p9_rate = clk_hw_get_rate(p9);

	parent_req.rate = req->rate = min(req->rate, p9_rate);
	if (req->rate == p9_rate) {
		req->rate = req->best_parent_rate = p9_rate;
		req->best_parent_hw = p9;
		return 0;
	}

	if (req->best_parent_hw == p9) {
		/* Are we going back to a previously used rate? */
		if (clk_hw_get_rate(p8) == req->rate)
			req->best_parent_hw = p8;
		else
			req->best_parent_hw = p2;
	} else if (req->best_parent_hw == p8) {
		req->best_parent_hw = p2;
	} else {
		req->best_parent_hw = p8;
	}

	ret = __clk_determine_rate(req->best_parent_hw, &parent_req);
	if (ret)
		return ret;

	req->rate = req->best_parent_rate = parent_req.rate;

	return 0;
}

static int clk_gfx3d_set_rate_and_parent(struct clk_hw *hw, unsigned long rate,
		unsigned long parent_rate, u8 index)
{
	struct clk_rcg2 *rcg = to_clk_rcg2(hw);
	u32 cfg;
	int ret;

	/* Just mux it, we don't use the division or m/n hardware */
	cfg = rcg->parent_map[index].cfg << CFG_SRC_SEL_SHIFT;
	ret = regmap_write(rcg->clkr.regmap, rcg->cmd_rcgr + CFG_REG, cfg);
	if (ret)
		return ret;

	return update_config(rcg);
}

static int clk_gfx3d_set_rate(struct clk_hw *hw, unsigned long rate,
			      unsigned long parent_rate)
{
	/*
	 * We should never get here; clk_gfx3d_determine_rate() should always
	 * make us use a different parent than what we're currently using, so
	 * clk_gfx3d_set_rate_and_parent() should always be called.
	 */
	return 0;
}

const struct clk_ops clk_gfx3d_ops = {
	.is_enabled = clk_rcg2_is_enabled,
	.get_parent = clk_rcg2_get_parent,
	.set_parent = clk_rcg2_set_parent,
	.recalc_rate = clk_rcg2_recalc_rate,
	.set_rate = clk_gfx3d_set_rate,
	.set_rate_and_parent = clk_gfx3d_set_rate_and_parent,
	.determine_rate = clk_gfx3d_determine_rate,
	.list_registers = clk_rcg2_list_registers,
};
EXPORT_SYMBOL_GPL(clk_gfx3d_ops);

static int clk_esc_determine_rate(struct clk_hw *hw,
				    struct clk_rate_request *req)
{
	struct clk_rcg2 *rcg = to_clk_rcg2(hw);
	unsigned long parent_rate, div;
	u32 mask = BIT(rcg->hid_width) - 1;
	struct clk_hw *p;
	unsigned long rate = req->rate;

	if (rate == 0)
		return -EINVAL;

	p = req->best_parent_hw;
	req->best_parent_rate = parent_rate = clk_hw_round_rate(p, rate);

	div = ((2 * parent_rate) / rate) - 1;
	div = min_t(u32, div, mask);

	req->rate = clk_rcg2_calc_rate(parent_rate, 0, 0, 0, div);

	return 0;
}

static int clk_esc_set_rate(struct clk_hw *hw, unsigned long rate,
			 unsigned long parent_rate)
{
	struct clk_rcg2 *rcg = to_clk_rcg2(hw);
	struct freq_tbl f = { 0 };
	unsigned long div;
	int i, num_parents = clk_hw_get_num_parents(hw);
	u32 mask = BIT(rcg->hid_width) - 1;
	u32 cfg;

	div = ((2 * parent_rate) / rate) - 1;
	div = min_t(u32, div, mask);

	f.pre_div = div;

	regmap_read(rcg->clkr.regmap, rcg->cmd_rcgr + CFG_REG, &cfg);
	cfg &= CFG_SRC_SEL_MASK;
	cfg >>= CFG_SRC_SEL_SHIFT;

	for (i = 0; i < num_parents; i++) {
		if (cfg == rcg->parent_map[i].cfg) {
			f.src = rcg->parent_map[i].src;
			return clk_rcg2_configure(rcg, &f);
		}
	}

	return -EINVAL;
}

static int clk_esc_set_rate_and_parent(struct clk_hw *hw,
		unsigned long rate, unsigned long parent_rate, u8 index)
{
	return clk_esc_set_rate(hw, rate, parent_rate);
}

const struct clk_ops clk_esc_ops = {
	.is_enabled = clk_rcg2_is_enabled,
	.get_parent = clk_rcg2_get_parent,
	.set_parent = clk_rcg2_set_parent,
	.recalc_rate = clk_rcg2_recalc_rate,
	.determine_rate = clk_esc_determine_rate,
	.set_rate = clk_esc_set_rate,
	.set_rate_and_parent = clk_esc_set_rate_and_parent,
	.list_registers = clk_rcg2_list_registers,
};
EXPORT_SYMBOL(clk_esc_ops);

/* Common APIs to be used for DFS based RCGR */
static u8 clk_parent_index_pre_div_and_mode(struct clk_hw *hw, u32 offset,
		u32 *mode, u32 *pre_div)
{
	struct clk_rcg2 *rcg;
	int num_parents;
	u32 cfg, mask;
	int i, ret;

	if (!hw)
		return -EINVAL;

	num_parents = clk_hw_get_num_parents(hw);

	rcg = to_clk_rcg2(hw);

	ret = regmap_read(rcg->clkr.regmap, rcg->cmd_rcgr + offset, &cfg);
	if (ret)
		goto err;

	mask = BIT(rcg->hid_width) - 1;
	*pre_div = cfg & mask ? (cfg & mask) : 1;

	*mode = cfg & CFG_MODE_MASK;
	*mode >>= CFG_MODE_SHIFT;

	cfg &= CFG_SRC_SEL_MASK;
	cfg >>= CFG_SRC_SEL_SHIFT;

	for (i = 0; i < num_parents; i++)
		if (cfg == rcg->parent_map[i].cfg)
			return i;
err:
	pr_debug("Clock %s has invalid parent, using default.\n",
		 clk_hw_get_name(hw));
	return 0;
}

static int calculate_m_and_n(struct clk_hw *hw, u32 m_offset, u32 n_offset,
		u32 mode, u32 *m, u32 *n)
{
	struct clk_rcg2 *rcg = to_clk_rcg2(hw);
	u32 val, mask;
	int ret = 0;

	if (!hw)
		return -EINVAL;

	*m = *n = 0;

	if (mode) {
		/* Calculate M & N values */
		mask = BIT(rcg->mnd_width) - 1;
		ret =  regmap_read(rcg->clkr.regmap, rcg->cmd_rcgr + m_offset,
					&val);
		if (ret) {
			pr_err("Failed to read M offset register\n");
			goto err;
		}

		val &= mask;
		*m  = val;

		ret =  regmap_read(rcg->clkr.regmap, rcg->cmd_rcgr + n_offset,
					&val);
		if (ret) {
			pr_err("Failed to read N offset register\n");
			goto err;
		}

		/* val ~(N-M) */
		val = ~val;
		val &= mask;
		val += *m;
		*n = val;
	}
err:
	return ret;
}

int clk_rcg2_get_dfs_clock_rate(struct clk_rcg2 *clk, struct device *dev,
						u8 rcg_flags)
{
	int i, j, index, ret = 0;
	unsigned long calc_freq, prate;
	u32 val, pre_div = 0, mode = 0, m = 0, n = 0;
	struct freq_tbl *dfs_freq_tbl;
	struct clk_hw *phw;

	if (!clk)
		return -EINVAL;

	/* Check for DFS_EN */
	ret = regmap_read(clk->clkr.regmap, clk->cmd_rcgr + SE_CMD_DFSR_OFFSET,
						&val);
	if (ret) {
		dev_err(dev, "Failed to read DFS enable register\n");
		return -EINVAL;
	}

	if (!(val & SE_CMD_DFS_EN))
		return ret;

	dfs_freq_tbl = devm_kzalloc(dev, MAX_PERF_LEVEL *
				sizeof(struct freq_tbl), GFP_KERNEL);
	if (!dfs_freq_tbl)
		return -ENOMEM;

	/* Populate the Perf Level */
	for (i = 0; i < MAX_PERF_LEVEL; i++) {
		/* Get parent index and mode */
		index = clk_parent_index_pre_div_and_mode(&clk->clkr.hw,
							SE_PERF_DFSR(i), &mode,
							&pre_div);
		if (index < 0) {
			pr_err("Failed to get parent index & mode %d\n", index);
			return index;
		}

		/* clock pre_div */
		dfs_freq_tbl[i].pre_div = pre_div;

		/* Fill the parent src */
		dfs_freq_tbl[i].src = clk->parent_map[index].src;

		/* Get the parent clock and parent rate */
		phw = clk_hw_get_parent_by_index(&clk->clkr.hw, index);
		prate = clk_hw_get_rate(phw);

		ret = calculate_m_and_n(&clk->clkr.hw, SE_PERF_M_DFSR(i),
					SE_PERF_N_DFSR(i), mode, &m, &n);
		if (ret)
			goto err;

		dfs_freq_tbl[i].m = m;
		dfs_freq_tbl[i].n = n;

		/* calculate the final frequency */
		calc_freq = clk_rcg2_calc_rate(prate, dfs_freq_tbl[i].m,
						dfs_freq_tbl[i].n, mode,
						dfs_freq_tbl[i].pre_div);

		/* Check for duplicate frequencies */
		for (j = 0; j  < i; j++) {
			if (dfs_freq_tbl[j].freq == calc_freq)
				goto done;
		}

		dfs_freq_tbl[i].freq = calc_freq;
	}
done:
	j = i;

	for (i = 0; i < j; i++)
		pr_debug("Index[%d]\tfreq_table.freq %ld\tfreq_table.src %d\t"
		"freq_table.pre_div %d\tfreq_table.m %d\tfreq_table.n %d\t"
		"RCG flags %x\n", i, dfs_freq_tbl[i].freq, dfs_freq_tbl[i].src,
				dfs_freq_tbl[i].pre_div, dfs_freq_tbl[i].m,
				dfs_freq_tbl[i].n, rcg_flags);
	/* Skip the safe configuration if DFS has been enabled for the RCG. */
	if (clk->enable_safe_config)
		clk->enable_safe_config = false;
	clk->flags |= rcg_flags;
	clk->freq_tbl = dfs_freq_tbl;
err:
	return ret;
}
