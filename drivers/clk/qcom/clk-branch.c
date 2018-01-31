/*
 * Copyright (c) 2013, 2016-2017, The Linux Foundation. All rights reserved.
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
#include <linux/delay.h>
#include <linux/export.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/regmap.h>
#include <linux/clk/qcom.h>

#include "clk-branch.h"
#include "clk-regmap.h"
#include "clk-debug.h"

static bool clk_branch_in_hwcg_mode(const struct clk_branch *br)
{
	u32 val;

	if (!br->hwcg_reg)
		return 0;

	regmap_read(br->clkr.regmap, br->hwcg_reg, &val);

	return !!(val & BIT(br->hwcg_bit));
}

static bool clk_branch_check_halt(const struct clk_branch *br, bool enabling)
{
	bool invert = (br->halt_check == BRANCH_HALT_ENABLE);
	u32 val;

	regmap_read(br->clkr.regmap, br->halt_reg, &val);

	val &= BIT(br->halt_bit);
	if (invert)
		val = !val;

	return !!val == !enabling;
}

#define BRANCH_CLK_OFF			BIT(31)
#define BRANCH_NOC_FSM_STATUS_SHIFT	28
#define BRANCH_NOC_FSM_STATUS_MASK	0x7
#define BRANCH_NOC_FSM_STATUS_ON	(0x2 << BRANCH_NOC_FSM_STATUS_SHIFT)

static bool clk_branch2_check_halt(const struct clk_branch *br, bool enabling)
{
	u32 val;
	u32 mask;

	mask = BRANCH_NOC_FSM_STATUS_MASK << BRANCH_NOC_FSM_STATUS_SHIFT;
	mask |= BRANCH_CLK_OFF;

	regmap_read(br->clkr.regmap, br->halt_reg, &val);

	if (enabling) {
		val &= mask;
		return (val & BRANCH_CLK_OFF) == 0 ||
			val == BRANCH_NOC_FSM_STATUS_ON;
	} else {
		return val & BRANCH_CLK_OFF;
	}
}

static int clk_branch_wait(const struct clk_branch *br, bool enabling,
		bool (check_halt)(const struct clk_branch *, bool))
{
	bool voted = br->halt_check & BRANCH_VOTED;
	const char *name = clk_hw_get_name(&br->clkr.hw);

	/* Skip checking halt bit if the clock is in hardware gated mode */
	if (clk_branch_in_hwcg_mode(br))
		return 0;

	if (br->halt_check == BRANCH_HALT_DELAY || (!enabling && voted)) {
		udelay(10);
	} else if (br->halt_check == BRANCH_HALT_ENABLE ||
		   br->halt_check == BRANCH_HALT ||
		   (enabling && voted)) {
		int count = 500;

		while (count-- > 0) {
			if (check_halt(br, enabling))
				return 0;
			udelay(1);
		}
		WARN(1, "clk: %s status stuck at 'o%s'", name,
				enabling ? "ff" : "n");
		return -EBUSY;
	}
	return 0;
}

static int clk_branch_toggle(struct clk_hw *hw, bool en,
		bool (check_halt)(const struct clk_branch *, bool))
{
	struct clk_branch *br = to_clk_branch(hw);
	int ret;

	if (en) {
		ret = clk_enable_regmap(hw);
		if (ret)
			return ret;
	} else {
		clk_disable_regmap(hw);
	}

	return clk_branch_wait(br, en, check_halt);
}

static int clk_branch_enable(struct clk_hw *hw)
{
	return clk_branch_toggle(hw, true, clk_branch_check_halt);
}

static int clk_cbcr_set_flags(struct regmap *regmap, unsigned int reg,
				unsigned long flags)
{
	u32 cbcr_val;

	regmap_read(regmap, reg, &cbcr_val);

	switch (flags) {
	case CLKFLAG_PERIPH_OFF_SET:
		cbcr_val |= BIT(12);
		break;
	case CLKFLAG_PERIPH_OFF_CLEAR:
		cbcr_val &= ~BIT(12);
		break;
	case CLKFLAG_RETAIN_PERIPH:
		cbcr_val |= BIT(13);
		break;
	case CLKFLAG_NORETAIN_PERIPH:
		cbcr_val &= ~BIT(13);
		break;
	case CLKFLAG_RETAIN_MEM:
		cbcr_val |= BIT(14);
		break;
	case CLKFLAG_NORETAIN_MEM:
		cbcr_val &= ~BIT(14);
		break;
	default:
		return -EINVAL;
	}

	regmap_write(regmap, reg, cbcr_val);

	/* Make sure power is enabled/disabled before returning. */
	mb();
	udelay(1);

	return 0;
}

static void clk_branch_disable(struct clk_hw *hw)
{
	clk_branch_toggle(hw, false, clk_branch_check_halt);
}

static int clk_branch_set_flags(struct clk_hw *hw, unsigned int flags)
{
	struct clk_branch *br = to_clk_branch(hw);

	return clk_cbcr_set_flags(br->clkr.regmap, br->halt_reg, flags);
}

const struct clk_ops clk_branch_ops = {
	.enable = clk_branch_enable,
	.disable = clk_branch_disable,
	.is_enabled = clk_is_enabled_regmap,
	.set_flags = clk_branch_set_flags,
};
EXPORT_SYMBOL_GPL(clk_branch_ops);

static int clk_branch2_set_rate(struct clk_hw *hw, unsigned long rate,
			    unsigned long parent_rate)
{
	struct clk_branch *branch = to_clk_branch(hw);
	struct clk_hw *parent = clk_hw_get_parent(hw);
	unsigned long curr_rate, new_rate, other_rate = 0;
	int ret = 0;

	if (!parent)
		return -EPERM;

	if (!branch->aggr_sibling_rates || !clk_hw_is_prepared(hw)) {
		branch->rate = rate;
		return 0;
	}

	other_rate = clk_aggregate_rate(hw, parent->core);
	curr_rate = max(other_rate, branch->rate);
	new_rate = max(other_rate, rate);

	if (new_rate != curr_rate) {
		ret = clk_set_rate(parent->clk, new_rate);
		if (ret)
			goto err;
	}
	branch->rate = rate;
err:
	return ret;
}

static long clk_branch2_round_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long *parent_rate)
{
	struct clk_hw *parent = clk_hw_get_parent(hw);
	unsigned long rrate = 0;

	if (!parent)
		return -EPERM;

	rrate = clk_hw_round_rate(parent, rate);
	/*
	 * If the rounded rate that's returned is valid, update the parent_rate
	 * field so that the set_rate() call can be propagated to the parent.
	 */
	if (rrate > 0)
		*parent_rate = rrate;

	return rrate;
}

static unsigned long clk_branch2_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	return to_clk_branch(hw)->rate;
}

static void clk_branch2_list_registers(struct seq_file *f, struct clk_hw *hw)
{
	struct clk_branch *br = to_clk_branch(hw);
	struct clk_regmap *rclk = to_clk_regmap(hw);
	int size, i, val;

	static struct clk_register_data data[] = {
		{"CBCR", 0x0},
	};

	static struct clk_register_data data1[] = {
		{"APSS_VOTE", 0x0},
		{"APSS_SLEEP_VOTE", 0x4},
	};

	size = ARRAY_SIZE(data);

	for (i = 0; i < size; i++) {
		regmap_read(br->clkr.regmap, br->halt_reg + data[i].offset,
					&val);
		seq_printf(f, "%20s: 0x%.8x\n", data[i].name, val);
	}

	if ((br->halt_check & BRANCH_HALT_VOTED) &&
			!(br->halt_check & BRANCH_VOTED)) {
		if (rclk->enable_reg) {
			size = ARRAY_SIZE(data1);
			for (i = 0; i < size; i++) {
				regmap_read(br->clkr.regmap, rclk->enable_reg +
						data1[i].offset, &val);
				seq_printf(f, "%20s: 0x%.8x\n",
						data1[i].name, val);
			}
		}
	}
}

static int clk_branch2_enable(struct clk_hw *hw)
{
	return clk_branch_toggle(hw, true, clk_branch2_check_halt);
}

static int clk_branch2_prepare(struct clk_hw *hw)
{
	struct clk_branch *branch;
	struct clk_hw *parent;
	unsigned long curr_rate;
	int ret = 0;

	if (!hw)
		return -EINVAL;

	branch = to_clk_branch(hw);
	parent = clk_hw_get_parent(hw);
	if (!branch)
		return -EINVAL;

	/*
	 * Do the rate aggregation and scaling of the RCG in the prepare/
	 * unprepare functions to avoid potential RPM(/h) communication due to
	 * votes on the voltage rails.
	 */
	if (branch->aggr_sibling_rates) {
		if (!parent)
			return -EINVAL;
		curr_rate = clk_aggregate_rate(hw, parent->core);
		if (branch->rate > curr_rate) {
			ret = clk_set_rate(parent->clk, branch->rate);
			if (ret)
				goto exit;
		}
	}
exit:
	return ret;
}

static void clk_branch2_disable(struct clk_hw *hw)
{
	clk_branch_toggle(hw, false, clk_branch2_check_halt);
}

static void clk_branch2_unprepare(struct clk_hw *hw)
{
	struct clk_branch *branch;
	struct clk_hw *parent;
	unsigned long curr_rate, new_rate;

	if (!hw)
		return;

	branch = to_clk_branch(hw);
	parent = clk_hw_get_parent(hw);
	if (!branch)
		return;

	if (branch->aggr_sibling_rates) {
		if (!parent)
			return;
		new_rate = clk_aggregate_rate(hw, parent->core);
		curr_rate = max(new_rate, branch->rate);
		if (new_rate < curr_rate)
			if (clk_set_rate(parent->clk, new_rate))
				pr_err("Failed to scale %s to %lu\n",
					clk_hw_get_name(parent), new_rate);
	}
}

const struct clk_ops clk_branch2_ops = {
	.prepare = clk_branch2_prepare,
	.enable = clk_branch2_enable,
	.unprepare = clk_branch2_unprepare,
	.disable = clk_branch2_disable,
	.is_enabled = clk_is_enabled_regmap,
	.set_rate = clk_branch2_set_rate,
	.round_rate = clk_branch2_round_rate,
	.recalc_rate = clk_branch2_recalc_rate,
	.set_flags = clk_branch_set_flags,
	.list_registers = clk_branch2_list_registers,
	.debug_init = clk_debug_measure_add,
};
EXPORT_SYMBOL_GPL(clk_branch2_ops);

static int clk_branch2_hw_ctl_set_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long parent_rate)
{
	if (!(hw->init->flags & CLK_SET_RATE_PARENT)) {
		pr_err("SET_RATE_PARENT flag needs to be set for %s\n",
					clk_hw_get_name(hw));
		return -EINVAL;
	}

	return 0;
}

static unsigned long clk_branch2_hw_ctl_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	return parent_rate;
}

static int clk_branch2_hw_ctl_determine_rate(struct clk_hw *hw,
		struct clk_rate_request *req)
{
	struct clk_hw *clkp;

	clkp = clk_hw_get_parent(hw);
	if (!clkp)
		return -EINVAL;

	req->best_parent_hw = clkp;
	req->best_parent_rate = clk_round_rate(clkp->clk, req->rate);

	return 0;
}

static int clk_branch2_hw_ctl_enable(struct clk_hw *hw)
{
	struct clk_hw *parent = clk_hw_get_parent(hw);

	/* The parent branch clock should have been prepared prior to this. */
	if (!parent || (parent && !clk_hw_is_prepared(parent)))
		return -EINVAL;

	return clk_enable_regmap(hw);
}

static void clk_branch2_hw_ctl_disable(struct clk_hw *hw)
{
	struct clk_hw *parent = clk_hw_get_parent(hw);

	if (!parent)
		return;

	clk_disable_regmap(hw);
}

const struct clk_ops clk_branch2_hw_ctl_ops = {
	.enable = clk_branch2_hw_ctl_enable,
	.disable = clk_branch2_hw_ctl_disable,
	.is_enabled = clk_is_enabled_regmap,
	.set_rate = clk_branch2_hw_ctl_set_rate,
	.recalc_rate = clk_branch2_hw_ctl_recalc_rate,
	.determine_rate = clk_branch2_hw_ctl_determine_rate,
	.set_flags = clk_branch_set_flags,
	.list_registers = clk_branch2_list_registers,
};
EXPORT_SYMBOL_GPL(clk_branch2_hw_ctl_ops);

static int clk_gate_toggle(struct clk_hw *hw, bool en)
{
	struct clk_gate2 *gt = to_clk_gate2(hw);
	int ret = 0;

	if (en) {
		ret = clk_enable_regmap(hw);
		if (ret)
			return ret;
	} else {
		clk_disable_regmap(hw);
	}

	if (gt->udelay)
		udelay(gt->udelay);

	return ret;
}

static int clk_gate2_enable(struct clk_hw *hw)
{
	return clk_gate_toggle(hw, true);
}

static void clk_gate2_disable(struct clk_hw *hw)
{
	clk_gate_toggle(hw, false);
}

static void clk_gate2_list_registers(struct seq_file *f, struct clk_hw *hw)
{
	struct clk_gate2 *gt = to_clk_gate2(hw);
	int size, i, val;

	static struct clk_register_data data[] = {
		{"EN_REG", 0x0},
	};

	size = ARRAY_SIZE(data);

	for (i = 0; i < size; i++) {
		regmap_read(gt->clkr.regmap, gt->clkr.enable_reg +
					data[i].offset, &val);
		seq_printf(f, "%20s: 0x%.8x\n", data[i].name, val);
	}
}

const struct clk_ops clk_gate2_ops = {
	.enable = clk_gate2_enable,
	.disable = clk_gate2_disable,
	.is_enabled = clk_is_enabled_regmap,
	.list_registers = clk_gate2_list_registers,
	.debug_init = clk_debug_measure_add,
};
EXPORT_SYMBOL_GPL(clk_gate2_ops);

const struct clk_ops clk_branch_simple_ops = {
	.enable = clk_enable_regmap,
	.disable = clk_disable_regmap,
	.is_enabled = clk_is_enabled_regmap,
};
EXPORT_SYMBOL_GPL(clk_branch_simple_ops);
