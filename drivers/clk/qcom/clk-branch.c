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

#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/export.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/regmap.h>

#include "clk-branch.h"
#include "clk-debug.h"
#include "clk-regmap.h"
#include "common.h"

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
	const struct clk_hw *hw = &br->clkr.hw;
	const char *name = clk_hw_get_name(hw);

	/* Skip checking halt bit if the clock is in hardware gated mode */
	if (clk_branch_in_hwcg_mode(br))
		return 0;

	/*
	 * Some of the BRANCH_VOTED clocks could be controlled by other
	 * masters via voting registers, and would require to add delay
	 * polling for the status bit to allow previous clk_disable
	 * by the GDS controller to go through.
	 */
	if (enabling && voted)
		udelay(5);

	if (br->halt_check == BRANCH_HALT_DELAY || (!enabling && voted)) {
		udelay(10);
	} else if (br->halt_check == BRANCH_HALT_ENABLE ||
		   br->halt_check == BRANCH_HALT ||
		   (enabling && voted)) {
		int count = 200;

		while (count-- > 0) {
			if (check_halt(br, enabling))
				return 0;
			udelay(1);
		}

		WARN_CLK(hw->core, name, 1, "status stuck at 'o%s'",
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

static int clk_branch_set_flags(struct clk_hw *hw, unsigned flags)
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
		clock_debug_output(f, false, "%20s: 0x%.8x\n",
							data[i].name, val);
	}

	if ((br->halt_check & BRANCH_HALT_VOTED) &&
			!(br->halt_check & BRANCH_VOTED)) {
		if (rclk->enable_reg) {
			size = ARRAY_SIZE(data1);
			for (i = 0; i < size; i++) {
				regmap_read(br->clkr.regmap, rclk->enable_reg +
						data1[i].offset, &val);
				clock_debug_output(f, false, "%20s: 0x%.8x\n",
						data1[i].name, val);
			}
		}
	}
}

static int clk_branch2_enable(struct clk_hw *hw)
{
	return clk_branch_toggle(hw, true, clk_branch2_check_halt);
}

static void clk_branch2_disable(struct clk_hw *hw)
{
	clk_branch_toggle(hw, false, clk_branch2_check_halt);
}

const struct clk_ops clk_branch2_ops = {
	.enable = clk_branch2_enable,
	.disable = clk_branch2_disable,
	.is_enabled = clk_is_enabled_regmap,
	.set_flags = clk_branch_set_flags,
	.list_registers = clk_branch2_list_registers,
	.debug_init = clk_debug_measure_add,
};
EXPORT_SYMBOL_GPL(clk_branch2_ops);

static int clk_branch2_hw_ctl_set_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long parent_rate)
{
	/*
	 * Make sure the branch clock has CLK_SET_RATE_PARENT flag,
	 * and the RCG has FORCE_ENABLE_RCGR flag set.
	 */
	if (!(hw->init->flags & CLK_SET_RATE_PARENT)) {
		pr_err("set rate would not get propagated to parent\n");
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

	clkp = __clk_get_hw(clk_get_parent(hw->clk));
	if (!clkp)
		return -EINVAL;

	req->best_parent_hw = clkp;
	req->best_parent_rate = clk_round_rate(clkp->clk, req->rate);

	return 0;
}

static int clk_branch2_hw_ctl_enable(struct clk_hw *hw)
{
	struct clk_hw *parent = __clk_get_hw(clk_get_parent(hw->clk));

	/* The parent branch clock should have been prepared prior to this. */
	if (!parent || (parent && !clk_hw_is_prepared(parent)))
		return -EINVAL;

	return clk_enable_regmap(hw);
}

static void clk_branch2_hw_ctl_disable(struct clk_hw *hw)
{
	struct clk_hw *parent = __clk_get_hw(clk_get_parent(hw->clk));

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
		clock_debug_output(f, false, "%20s: 0x%.8x\n",
						data[i].name, val);
	}
}

static int clk_gate2_set_flags(struct clk_hw *hw, unsigned flags)
{
	struct clk_gate2 *gt = to_clk_gate2(hw);

	return clk_cbcr_set_flags(gt->clkr.regmap, gt->clkr.enable_reg,
					flags);
}

const struct clk_ops clk_gate2_ops = {
	.enable = clk_gate2_enable,
	.disable = clk_gate2_disable,
	.is_enabled = clk_is_enabled_regmap,
	.list_registers = clk_gate2_list_registers,
	.set_flags = clk_gate2_set_flags,
	.debug_init = clk_debug_measure_add,
};
EXPORT_SYMBOL_GPL(clk_gate2_ops);

const struct clk_ops clk_branch_simple_ops = {
	.enable = clk_enable_regmap,
	.disable = clk_disable_regmap,
	.is_enabled = clk_is_enabled_regmap,
};
EXPORT_SYMBOL_GPL(clk_branch_simple_ops);
