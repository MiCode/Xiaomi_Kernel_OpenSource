/*
 * Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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
#include <linux/export.h>
#include <linux/clk-provider.h>
#include <linux/regmap.h>
#include <linux/delay.h>

#include "clk-alpha-pll.h"

#define PLL_MODE		0x00
#define PLL_OUTCTRL		BIT(0)
#define PLL_BYPASSNL		BIT(1)
#define PLL_RESET_N		BIT(2)
#define PLL_LOCK_COUNT_SHIFT	8
#define PLL_LOCK_COUNT_MASK	0x3f
#define PLL_LOCK_COUNT_VAL	0x0
#define PLL_BIAS_COUNT_SHIFT	14
#define PLL_BIAS_COUNT_MASK	0x3f
#define PLL_BIAS_COUNT_VAL	0x6
#define PLL_LATCH_INTERFACE	BIT(11)
#define PLL_VOTE_FSM_ENA	BIT(20)
#define PLL_VOTE_FSM_RESET	BIT(21)
#define PLL_UPDATE		BIT(22)
#define PLL_HW_UPDATE_LOGIC_BYPASS	BIT(23)
#define PLL_ALPHA_EN		BIT(24)
#define PLL_ACTIVE_FLAG		BIT(30)
#define PLL_LOCK_DET		BIT(31)
#define PLL_ACK_LATCH		BIT(29)

#define PLL_L_VAL		0x04
#define PLL_ALPHA_VAL		0x08
#define PLL_ALPHA_VAL_U		0x0c

#define PLL_USER_CTL		0x10
#define PLL_POST_DIV_SHIFT	8
#define PLL_POST_DIV_MASK	0xf
#define PLL_VCO_SHIFT		20
#define PLL_VCO_MASK		0x3

#define PLL_USER_CTL_U		0x14

#define PLL_CONFIG_CTL		0x18
#define PLL_TEST_CTL		0x1c
#define PLL_TEST_CTL_U		0x20
#define PLL_STATUS		0x24

/*
 * Even though 40 bits are present, use only 32 for ease of calculation.
 */
#define ALPHA_REG_BITWIDTH	40
#define ALPHA_BITWIDTH		32

#define to_clk_alpha_pll(_hw) container_of(to_clk_regmap(_hw), \
					   struct clk_alpha_pll, clkr)

#define to_clk_alpha_pll_postdiv(_hw) container_of(to_clk_regmap(_hw), \
					   struct clk_alpha_pll_postdiv, clkr)

static int wait_for_pll(struct clk_alpha_pll *pll, u32 mask, bool inverse,
			const char *action)
{
	u32 val, off;
	int count;
	int ret;
	const char *name = clk_hw_get_name(&pll->clkr.hw);

	off = pll->offset;
	ret = regmap_read(pll->clkr.regmap, off + PLL_MODE, &val);
	if (ret)
		return ret;

	for (count = 100; count > 0; count--) {
		ret = regmap_read(pll->clkr.regmap, off + PLL_MODE, &val);
		if (ret)
			return ret;
		if (inverse && !(val & mask))
			return 0;
		else if ((val & mask) == mask)
			return 0;

		udelay(1);
	}

	WARN(1, "%s failed to %s!\n", name, action);
	return -ETIMEDOUT;
}

static int wait_for_pll_enable(struct clk_alpha_pll *pll, u32 mask)
{
	return wait_for_pll(pll, mask, 0, "enable");
}

static int wait_for_pll_disable(struct clk_alpha_pll *pll, u32 mask)
{
	return wait_for_pll(pll, mask, 1, "disable");
}

static int wait_for_pll_offline(struct clk_alpha_pll *pll, u32 mask)
{
	return wait_for_pll(pll, mask, 0, "offline");
}

static int wait_for_pll_latch_ack(struct clk_alpha_pll *pll, u32 mask)
{
	return wait_for_pll(pll, mask, 0, "latch_ack");
}

static int wait_for_pll_update(struct clk_alpha_pll *pll, u32 mask)
{
	return wait_for_pll(pll, mask, 1, "update");
}

/* alpha pll with hwfsm support */

#define PLL_OFFLINE_REQ		BIT(7)
#define PLL_FSM_ENA		BIT(20)
#define PLL_OFFLINE_ACK		BIT(28)
#define PLL_ACTIVE_FLAG		BIT(30)

static void clk_alpha_set_fsm_mode(struct clk_alpha_pll *pll)
{
	u32 val;

	regmap_read(pll->clkr.regmap, pll->offset + PLL_MODE, &val);

	/* De-assert reset to FSM */
	val &= ~PLL_VOTE_FSM_RESET;

	/* Program bias count */
	val &= ~(PLL_BIAS_COUNT_MASK << PLL_BIAS_COUNT_SHIFT);
	val |= PLL_BIAS_COUNT_VAL << PLL_BIAS_COUNT_SHIFT;

	/* Program lock count */
	val &= ~(PLL_LOCK_COUNT_MASK << PLL_LOCK_COUNT_SHIFT);
	val |= PLL_LOCK_COUNT_VAL << PLL_LOCK_COUNT_SHIFT;

	/* Enable PLL FSM voting */
	val |= PLL_VOTE_FSM_ENA;

	regmap_write(pll->clkr.regmap, pll->offset + PLL_MODE, val);
}

void clk_alpha_pll_configure(struct clk_alpha_pll *pll, struct regmap *regmap,
				const struct pll_config *config)
{
	u32 val, mask;

	if (config->l)
		regmap_write(regmap, pll->offset + PLL_L_VAL,
						config->l);
	if (config->alpha)
		regmap_write(regmap, pll->offset + PLL_ALPHA_VAL,
						config->alpha);
	if (config->alpha_u)
		regmap_write(regmap, pll->offset + PLL_ALPHA_VAL_U,
						config->alpha_u);
	if (config->config_ctl_val)
		regmap_write(regmap, pll->offset + PLL_CONFIG_CTL,
				config->config_ctl_val);

	if (config->main_output_mask || config->aux_output_mask ||
		config->aux2_output_mask || config->early_output_mask ||
		config->vco_val || config->alpha_en_mask) {

		val = config->main_output_mask;
		val |= config->aux_output_mask;
		val |= config->aux2_output_mask;
		val |= config->early_output_mask;
		val |= config->vco_val;
		val |= config->alpha_en_mask;

		mask = config->main_output_mask;
		mask |= config->aux_output_mask;
		mask |= config->aux2_output_mask;
		mask |= config->early_output_mask;
		mask |= config->vco_mask;
		mask |= config->alpha_en_mask;

		regmap_update_bits(regmap, pll->offset + PLL_USER_CTL,
					mask, val);
	}

	if (config->post_div_mask) {
		mask = config->post_div_mask;
		val = config->post_div_val;
		regmap_update_bits(regmap, pll->offset + PLL_USER_CTL,
					mask, val);
	}

	/* Do not bypass the latch interface */
	if (pll->flags & SUPPORTS_SLEW)
		regmap_update_bits(regmap, pll->offset + PLL_USER_CTL_U,
		PLL_LATCH_INTERFACE, (u32)~PLL_LATCH_INTERFACE);

	if (pll->flags & SUPPORTS_DYNAMIC_UPDATE) {
		regmap_update_bits(regmap, pll->offset + PLL_MODE,
				 PLL_HW_UPDATE_LOGIC_BYPASS,
				 PLL_HW_UPDATE_LOGIC_BYPASS);
	}

	if (config->test_ctl_lo_mask) {
		mask = config->test_ctl_lo_mask;
		val = config->test_ctl_lo_val;
		regmap_update_bits(regmap, pll->offset + PLL_TEST_CTL,
					mask, val);
	}

	if (config->test_ctl_hi_mask) {
		mask = config->test_ctl_hi_mask;
		val = config->test_ctl_hi_val;
		regmap_update_bits(regmap, pll->offset + PLL_TEST_CTL_U,
					mask, val);
	}

	if (pll->flags & SUPPORTS_FSM_MODE)
		clk_alpha_set_fsm_mode(pll);
}

static int clk_alpha_pll_hwfsm_enable(struct clk_hw *hw)
{
	int ret;
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	u32 val, off;

	off = pll->offset;
	ret = regmap_read(pll->clkr.regmap, off + PLL_MODE, &val);
	if (ret)
		return ret;
	/* Enable HW FSM mode, clear OFFLINE request */
	val |= PLL_FSM_ENA;
	val &= ~PLL_OFFLINE_REQ;
	ret = regmap_write(pll->clkr.regmap, off + PLL_MODE, val);
	if (ret)
		return ret;

	/* Make sure enable request goes through before waiting for update */
	mb();

	ret = wait_for_pll_enable(pll, PLL_ACTIVE_FLAG);
	if (ret)
		return ret;

	return 0;
}

static void clk_alpha_pll_hwfsm_disable(struct clk_hw *hw)
{
	int ret;
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	u32 val, off;

	off = pll->offset;
	ret = regmap_read(pll->clkr.regmap, off + PLL_MODE, &val);
	if (ret)
		return;
	/* Request PLL_OFFLINE and wait for ack */
	ret = regmap_update_bits(pll->clkr.regmap, off + PLL_MODE,
				 PLL_OFFLINE_REQ, PLL_OFFLINE_REQ);
	if (ret)
		return;
	ret = wait_for_pll_offline(pll, PLL_OFFLINE_ACK);
	if (ret)
		return;

	/* Disable hwfsm */
	ret = regmap_update_bits(pll->clkr.regmap, off + PLL_MODE,
				 PLL_FSM_ENA, 0);
	if (ret)
		return;

	wait_for_pll_disable(pll, PLL_ACTIVE_FLAG);

}

static int clk_alpha_pll_enable(struct clk_hw *hw)
{
	int ret;
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	u32 val, mask, off;

	off = pll->offset;
	mask = PLL_OUTCTRL | PLL_RESET_N | PLL_BYPASSNL;
	ret = regmap_read(pll->clkr.regmap, off + PLL_MODE, &val);
	if (ret)
		return ret;

	/* If in FSM mode, just vote for it */
	if (val & PLL_VOTE_FSM_ENA) {
		ret = clk_enable_regmap(hw);
		if (ret)
			return ret;
		ret = wait_for_pll_enable(pll, PLL_ACTIVE_FLAG);
		if (ret == 0) {
			if (pll->flags & SUPPORTS_FSM_VOTE)
				*pll->soft_vote |= (pll->soft_vote_mask);
			return ret;
		}
	}

	/* Skip if already enabled */
	if ((val & mask) == mask)
		return 0;

	ret = regmap_update_bits(pll->clkr.regmap, off + PLL_MODE,
				 PLL_BYPASSNL, PLL_BYPASSNL);
	if (ret)
		return ret;

	/*
	 * H/W requires a 5us delay between disabling the bypass and
	 * de-asserting the reset.
	 */
	mb();
	udelay(5);

	ret = regmap_update_bits(pll->clkr.regmap, off + PLL_MODE,
				 PLL_RESET_N, PLL_RESET_N);
	if (ret)
		return ret;

	ret = wait_for_pll_enable(pll, PLL_LOCK_DET);
	if (ret)
		return ret;

	ret = regmap_update_bits(pll->clkr.regmap, off + PLL_MODE,
				 PLL_OUTCTRL, PLL_OUTCTRL);

	/* Ensure that the write above goes through before returning. */
	mb();
	return ret;
}

static void clk_alpha_pll_disable(struct clk_hw *hw)
{
	int ret;
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	u32 val, mask, off;

	off = pll->offset;

	ret = regmap_read(pll->clkr.regmap, off + PLL_MODE, &val);
	if (ret)
		return;

	/* If in FSM mode, just unvote it */
	if (val & PLL_VOTE_FSM_ENA) {
		if (pll->flags & SUPPORTS_FSM_VOTE) {
			*pll->soft_vote &= ~(pll->soft_vote_mask);
			if (!*pll->soft_vote)
				clk_disable_regmap(hw);
		} else
			clk_disable_regmap(hw);

		return;
	}

	mask = PLL_OUTCTRL;
	regmap_update_bits(pll->clkr.regmap, off + PLL_MODE, mask, 0);

	/* Delay of 2 output clock ticks required until output is disabled */
	mb();
	udelay(1);

	mask = PLL_RESET_N | PLL_BYPASSNL;
	regmap_update_bits(pll->clkr.regmap, off + PLL_MODE, mask, 0);
}

static unsigned long alpha_pll_calc_rate(u64 prate, u32 l, u32 a)
{
	return (prate * l) + ((prate * a) >> ALPHA_BITWIDTH);
}

static unsigned long
alpha_pll_round_rate(unsigned long rate, unsigned long prate, u32 *l, u64 *a)
{
	u64 remainder;
	u64 quotient;

	quotient = rate;
	remainder = do_div(quotient, prate);
	*l = quotient;

	if (!remainder) {
		*a = 0;
		return rate;
	}

	/* Upper ALPHA_BITWIDTH bits of Alpha */
	quotient = remainder << ALPHA_BITWIDTH;
	remainder = do_div(quotient, prate);

	if (remainder)
		quotient++;

	*a = quotient;
	return alpha_pll_calc_rate(prate, *l, *a);
}

static const struct pll_vco *
alpha_pll_find_vco(const struct clk_alpha_pll *pll, unsigned long rate)
{
	const struct pll_vco *v = pll->vco_table;
	const struct pll_vco *end = v + pll->num_vco;

	for (; v < end; v++)
		if (rate >= v->min_freq && rate <= v->max_freq)
			return v;

	return NULL;
}

static unsigned long
clk_alpha_pll_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	u32 l, low, high, ctl;
	u64 a = 0, prate = parent_rate;
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	u32 off = pll->offset;

	regmap_read(pll->clkr.regmap, off + PLL_L_VAL, &l);

	regmap_read(pll->clkr.regmap, off + PLL_USER_CTL, &ctl);
	if (ctl & PLL_ALPHA_EN) {
		regmap_read(pll->clkr.regmap, off + PLL_ALPHA_VAL, &low);
		regmap_read(pll->clkr.regmap, off + PLL_ALPHA_VAL_U, &high);
		a = (u64)high << 32 | low;
		a >>= ALPHA_REG_BITWIDTH - ALPHA_BITWIDTH;
	}

	ctl >>= PLL_POST_DIV_SHIFT;
	ctl &= PLL_POST_DIV_MASK;

	return alpha_pll_calc_rate(prate, l, a) >> fls(ctl);
}

static int clk_alpha_pll_dynamic_update(struct clk_alpha_pll *pll)
{
	int ret;

	/* Latch the input to the PLL */
	regmap_update_bits(pll->clkr.regmap, pll->offset + PLL_MODE,
				PLL_UPDATE, PLL_UPDATE);

	/* Wait for 2 reference cycle before checking ACK bit */
	udelay(1);

	ret = wait_for_pll_latch_ack(pll, PLL_ACK_LATCH);
	if (ret)
		return ret;

	/* Return latch input to 0 */
	regmap_update_bits(pll->clkr.regmap, pll->offset + PLL_MODE,
				PLL_UPDATE, (u32)~PLL_UPDATE);

	ret = wait_for_pll_enable(pll, PLL_LOCK_DET);
	if (ret)
		return ret;

	return 0;
}

static const struct pll_vco_data
	*find_vco_data(const struct pll_vco_data *data,
			unsigned long rate, size_t size)
{
	int i;

	if (!data)
		return NULL;

	for (i = 0; i < size; i++) {
		if (rate == data[i].freq)
			return &data[i];
	}

	return &data[i - 1];
}

static int clk_alpha_pll_set_rate(struct clk_hw *hw, unsigned long rate,
				  unsigned long prate)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	const struct pll_vco *vco;
	const struct pll_vco_data *data;
	bool is_enabled;
	u32 l, off = pll->offset;
	u64 a;
	unsigned long rrate;

	rrate = alpha_pll_round_rate(rate, prate, &l, &a);

	if (rrate != rate) {
		pr_err("alpha_pll: Call clk_set_rate with rounded rates!\n");
		return -EINVAL;
	}

	vco = alpha_pll_find_vco(pll, rrate);
	if (!vco) {
		pr_err("alpha pll not in a valid vco range\n");
		return -EINVAL;
	}

	is_enabled = clk_hw_is_enabled(hw);

	/*
	* For PLLs that do not support dynamic programming (dynamic_update
	* is not set), ensure PLL is off before changing rate. For
	* optimization reasons, assume no downstream clock is actively
	* using it.
	*/
	if (is_enabled && !(pll->flags & SUPPORTS_DYNAMIC_UPDATE))
		hw->init->ops->disable(hw);

	a <<= (ALPHA_REG_BITWIDTH - ALPHA_BITWIDTH);

	regmap_write(pll->clkr.regmap, off + PLL_L_VAL, l);
	regmap_write(pll->clkr.regmap, off + PLL_ALPHA_VAL, a);
	regmap_write(pll->clkr.regmap, off + PLL_ALPHA_VAL_U, a >> 32);

	regmap_update_bits(pll->clkr.regmap, off + PLL_USER_CTL,
			   PLL_VCO_MASK << PLL_VCO_SHIFT,
			   vco->val << PLL_VCO_SHIFT);

	data = find_vco_data(pll->vco_data, rate, pll->num_vco_data);
	if (data) {
		if (data->freq == rate)
			regmap_update_bits(pll->clkr.regmap, off + PLL_USER_CTL,
				PLL_POST_DIV_MASK << PLL_POST_DIV_SHIFT,
				data->post_div_val << PLL_POST_DIV_SHIFT);
		else
			regmap_update_bits(pll->clkr.regmap, off + PLL_USER_CTL,
					PLL_POST_DIV_MASK << PLL_POST_DIV_SHIFT,
					0x0 << PLL_VCO_SHIFT);
	}

	regmap_update_bits(pll->clkr.regmap, off + PLL_USER_CTL, PLL_ALPHA_EN,
			   PLL_ALPHA_EN);

	if (is_enabled && (pll->flags & SUPPORTS_DYNAMIC_UPDATE))
		clk_alpha_pll_dynamic_update(pll);

	if (is_enabled && !(pll->flags & SUPPORTS_DYNAMIC_UPDATE))
		hw->init->ops->enable(hw);

	return 0;
}

static long clk_alpha_pll_round_rate(struct clk_hw *hw, unsigned long rate,
				     unsigned long *prate)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	u32 l;
	u64 a;
	unsigned long min_freq, max_freq;

	if (rate < pll->min_supported_freq)
		return pll->min_supported_freq;

	rate = alpha_pll_round_rate(rate, *prate, &l, &a);
	if (alpha_pll_find_vco(pll, rate))
		return rate;

	min_freq = pll->vco_table[0].min_freq;
	max_freq = pll->vco_table[pll->num_vco - 1].max_freq;

	return clamp(rate, min_freq, max_freq);
}

static void clk_alpha_pll_list_registers(struct seq_file *f, struct clk_hw *hw)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	int size, i, val;

	static struct clk_register_data data[] = {
		{"PLL_MODE", 0x0},
		{"PLL_L_VAL", 0x4},
		{"PLL_ALPHA_VAL", 0x8},
		{"PLL_ALPHA_VAL_U", 0xC},
		{"PLL_USER_CTL", 0x10},
		{"PLL_CONFIG_CTL", 0x18},
	};

	static struct clk_register_data data1[] = {
		{"APSS_PLL_VOTE", 0x0},
	};

	size = ARRAY_SIZE(data);

	for (i = 0; i < size; i++) {
		regmap_read(pll->clkr.regmap, pll->offset + data[i].offset,
					&val);
		seq_printf(f, "%20s: 0x%.8x\n", data[i].name, val);
	}

	regmap_read(pll->clkr.regmap, pll->offset + data[0].offset, &val);

	if (val & PLL_FSM_ENA) {
		regmap_read(pll->clkr.regmap, pll->clkr.enable_reg +
					data1[0].offset, &val);
		seq_printf(f, "%20s: 0x%.8x\n", data1[0].name, val);
	}
}

const struct clk_ops clk_alpha_pll_ops = {
	.enable = clk_alpha_pll_enable,
	.disable = clk_alpha_pll_disable,
	.recalc_rate = clk_alpha_pll_recalc_rate,
	.round_rate = clk_alpha_pll_round_rate,
	.set_rate = clk_alpha_pll_set_rate,
	.list_registers = clk_alpha_pll_list_registers,
};
EXPORT_SYMBOL_GPL(clk_alpha_pll_ops);

const struct clk_ops clk_alpha_pll_hwfsm_ops = {
	.enable = clk_alpha_pll_hwfsm_enable,
	.disable = clk_alpha_pll_hwfsm_disable,
	.recalc_rate = clk_alpha_pll_recalc_rate,
	.round_rate = clk_alpha_pll_round_rate,
	.set_rate = clk_alpha_pll_set_rate,
	.list_registers = clk_alpha_pll_list_registers,
};
EXPORT_SYMBOL_GPL(clk_alpha_pll_hwfsm_ops);

static unsigned long
clk_alpha_pll_postdiv_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	struct clk_alpha_pll_postdiv *pll = to_clk_alpha_pll_postdiv(hw);
	u32 ctl;

	regmap_read(pll->clkr.regmap, pll->offset + PLL_USER_CTL, &ctl);

	ctl >>= PLL_POST_DIV_SHIFT;
	ctl &= PLL_POST_DIV_MASK;

	return parent_rate >> fls(ctl);
}

static const struct clk_div_table clk_alpha_div_table[] = {
	{ 0x0, 1 },
	{ 0x1, 2 },
	{ 0x3, 4 },
	{ 0x7, 8 },
	{ 0xf, 16 },
	{ }
};

static long
clk_alpha_pll_postdiv_round_rate(struct clk_hw *hw, unsigned long rate,
				 unsigned long *prate)
{
	struct clk_alpha_pll_postdiv *pll = to_clk_alpha_pll_postdiv(hw);

	return divider_round_rate(hw, rate, prate, clk_alpha_div_table,
				  pll->width, CLK_DIVIDER_POWER_OF_TWO);
}

static int clk_alpha_pll_postdiv_set_rate(struct clk_hw *hw, unsigned long rate,
					  unsigned long parent_rate)
{
	struct clk_alpha_pll_postdiv *pll = to_clk_alpha_pll_postdiv(hw);
	int div;

	/* 16 -> 0xf, 8 -> 0x7, 4 -> 0x3, 2 -> 0x1, 1 -> 0x0 */
	div = DIV_ROUND_UP_ULL((u64)parent_rate, rate) - 1;

	return regmap_update_bits(pll->clkr.regmap, pll->offset + PLL_USER_CTL,
				  PLL_POST_DIV_MASK << PLL_POST_DIV_SHIFT,
				  div << PLL_POST_DIV_SHIFT);
}

const struct clk_ops clk_alpha_pll_postdiv_ops = {
	.recalc_rate = clk_alpha_pll_postdiv_recalc_rate,
	.round_rate = clk_alpha_pll_postdiv_round_rate,
	.set_rate = clk_alpha_pll_postdiv_set_rate,
};
EXPORT_SYMBOL_GPL(clk_alpha_pll_postdiv_ops);

static int clk_alpha_pll_slew_update(struct clk_alpha_pll *pll)
{
	int ret = 0;
	u32 val;

	regmap_update_bits(pll->clkr.regmap, pll->offset + PLL_MODE,
					PLL_UPDATE, PLL_UPDATE);
	regmap_read(pll->clkr.regmap, pll->offset + PLL_MODE, &val);

	ret = wait_for_pll_update(pll, PLL_UPDATE);
	if (ret)
		return ret;
	/*
	 * HPG mandates a wait of at least 570ns before polling the LOCK
	 * detect bit. Have a delay of 1us just to be safe.
	 */
	mb();
	udelay(1);

	ret = wait_for_pll_enable(pll, PLL_LOCK_DET);

	return ret;
}

static int clk_alpha_pll_slew_set_rate(struct clk_hw *hw, unsigned long rate,
			unsigned long parent_rate)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	unsigned long freq_hz;
	const struct pll_vco *curr_vco, *vco;
	u32 l;
	u64 a;

	freq_hz = alpha_pll_round_rate(rate, parent_rate, &l, &a);
	if (freq_hz != rate) {
		pr_err("alpha_pll: Call clk_set_rate with rounded rates!\n");
		return -EINVAL;
	}

	curr_vco = alpha_pll_find_vco(pll, clk_hw_get_rate(hw));
	if (!curr_vco) {
		pr_err("alpha pll: not in a valid vco range\n");
		return -EINVAL;
	}

	vco = alpha_pll_find_vco(pll, freq_hz);
	if (!vco) {
		pr_err("alpha pll: not in a valid vco range\n");
		return -EINVAL;
	}

	/*
	 * Dynamic pll update will not support switching frequencies across
	 * vco ranges. In those cases fall back to normal alpha set rate.
	 */
	if (curr_vco->val != vco->val)
		return clk_alpha_pll_set_rate(hw, rate, parent_rate);

	a = a << (ALPHA_REG_BITWIDTH - ALPHA_BITWIDTH);

	regmap_write(pll->clkr.regmap, pll->offset + PLL_L_VAL, l);
	regmap_write(pll->clkr.regmap, pll->offset + PLL_ALPHA_VAL, a);
	regmap_write(pll->clkr.regmap, pll->offset + PLL_ALPHA_VAL_U, a >> 32);

	/* Ensure that the write above goes through before proceeding. */
	mb();

	if (clk_hw_is_enabled(hw))
		clk_alpha_pll_slew_update(pll);

	return 0;
}

/*
 * Slewing plls should be bought up at frequency which is in the middle of the
 * desired VCO range. So after bringing up the pll at calibration freq, set it
 * back to desired frequency(that was set by previous clk_set_rate).
 */
static int clk_alpha_pll_calibrate(struct clk_hw *hw)
{
	unsigned long calibration_freq, freq_hz;
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	const struct pll_vco *vco;
	u64 a;
	u32 l;
	int rc;

	vco = alpha_pll_find_vco(pll, clk_hw_get_rate(hw));
	if (!vco) {
		pr_err("alpha pll: not in a valid vco range\n");
		return -EINVAL;
	}

	/*
	 * As during slewing plls vco_sel won't be allowed to change, vco table
	 * should have only one entry table, i.e. index = 0, find the
	 * calibration frequency.
	 */
	calibration_freq = (pll->vco_table[0].min_freq +
					pll->vco_table[0].max_freq)/2;

	freq_hz = alpha_pll_round_rate(calibration_freq,
			clk_hw_get_rate(clk_hw_get_parent(hw)), &l, &a);
	if (freq_hz != calibration_freq) {
		pr_err("alpha_pll: call clk_set_rate with rounded rates!\n");
		return -EINVAL;
	}

	/* Setup PLL for calibration frequency */
	a <<= (ALPHA_REG_BITWIDTH - ALPHA_BITWIDTH);

	regmap_write(pll->clkr.regmap, pll->offset + PLL_L_VAL, l);
	regmap_write(pll->clkr.regmap, pll->offset + PLL_ALPHA_VAL, a);
	regmap_write(pll->clkr.regmap, pll->offset + PLL_ALPHA_VAL_U, a >> 32);

	regmap_update_bits(pll->clkr.regmap, pll->offset + PLL_USER_CTL,
				PLL_VCO_MASK << PLL_VCO_SHIFT,
				vco->val << PLL_VCO_SHIFT);

	regmap_update_bits(pll->clkr.regmap, pll->offset + PLL_USER_CTL,
				PLL_ALPHA_EN, PLL_ALPHA_EN);

	/* Bringup the pll at calibration frequency */
	rc = clk_alpha_pll_enable(hw);
	if (rc) {
		pr_err("alpha pll calibration failed\n");
		return rc;
	}

	/*
	 * PLL is already running at calibration frequency.
	 * So slew pll to the previously set frequency.
	 */
	freq_hz = alpha_pll_round_rate(clk_hw_get_rate(hw),
			clk_hw_get_rate(clk_hw_get_parent(hw)), &l, &a);

	pr_debug("pll %s: setting back to required rate %lu, freq_hz %ld\n",
				hw->init->name, clk_hw_get_rate(hw), freq_hz);

	/* Setup the PLL for the new frequency */
	a <<= (ALPHA_REG_BITWIDTH - ALPHA_BITWIDTH);

	regmap_write(pll->clkr.regmap, pll->offset + PLL_L_VAL, l);
	regmap_write(pll->clkr.regmap, pll->offset + PLL_ALPHA_VAL, a);
	regmap_write(pll->clkr.regmap, pll->offset + PLL_ALPHA_VAL_U, a >> 32);

	regmap_update_bits(pll->clkr.regmap, pll->offset + PLL_USER_CTL,
				PLL_ALPHA_EN, PLL_ALPHA_EN);

	return clk_alpha_pll_slew_update(pll);
}

static int clk_alpha_pll_slew_enable(struct clk_hw *hw)
{
	int rc;

	rc = clk_alpha_pll_calibrate(hw);
	if (rc)
		return rc;

	rc = clk_alpha_pll_enable(hw);

	return rc;
}

const struct clk_ops clk_alpha_pll_slew_ops = {
	.enable = clk_alpha_pll_slew_enable,
	.disable = clk_alpha_pll_disable,
	.recalc_rate = clk_alpha_pll_recalc_rate,
	.round_rate = clk_alpha_pll_round_rate,
	.set_rate = clk_alpha_pll_slew_set_rate,
};
EXPORT_SYMBOL_GPL(clk_alpha_pll_slew_ops);
