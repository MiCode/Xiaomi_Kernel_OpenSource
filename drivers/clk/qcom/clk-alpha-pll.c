/*
 * Copyright (c) 2015, 2017-2018, The Linux Foundation. All rights reserved.
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
#include <linux/export.h>
#include <linux/clk-provider.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/sched/clock.h>

#include "clk-alpha-pll.h"
#include "common.h"

#define PLL_MODE		0x00
#define PLL_OUTCTRL		BIT(0)
#define PLL_BYPASSNL		BIT(1)
#define PLL_RESET_N		BIT(2)
#define PLL_OFFLINE_REQ	BIT(7)
#define PLL_LOCK_COUNT_SHIFT	8
#define PLL_LOCK_COUNT_MASK	0x3f
#define PLL_BIAS_COUNT_SHIFT	14
#define PLL_BIAS_COUNT_MASK	0x3f
#define PLL_BIAS_COUNT_VAL	0x6
#define PLL_LATCH_INTERFACE	BIT(11)
#define PLL_VOTE_FSM_ENA	BIT(20)
#define PLL_FSM_ENA		BIT(20)
#define PLL_VOTE_FSM_RESET	BIT(21)
#define PLL_UPDATE		BIT(22)
#define PLL_HW_UPDATE_LOGIC_BYPASS	BIT(23)
#define PLL_ALPHA_EN		BIT(24)
#define PLL_OFFLINE_ACK		BIT(28)
#define PLL_ACK_LATCH		BIT(29)
#define PLL_ACTIVE_FLAG		BIT(30)
#define PLL_LOCK_DET		BIT(31)

#define PLL_L_VAL		0x04
#define PLL_ALPHA_VAL		0x08
#define PLL_ALPHA_VAL_U		0x0c

#define PLL_USER_CTL		0x10
#define PLL_POST_DIV_SHIFT	8
#define PLL_POST_DIV_MASK	0xf
#define PLL_ALPHA_EN		BIT(24)
#define PLL_VCO_SHIFT		20
#define PLL_VCO_MASK		0x3

#define PLL_USER_CTL_U		0x14

#define PLL_CONFIG_CTL		0x18
#define PLL_CONFIG_CTL_U	0x20
#define PLL_TEST_CTL		0x1c
#define PLL_TEST_CTL_U		0x20
#define PLL_STATUS		0x24

/*
 * Even though 40 bits are present, use only 32 for ease of calculation.
 */
#define ALPHA_REG_BITWIDTH	40
#define ALPHA_BITWIDTH		32
#define ALPHA_16BIT_MASK	0xffff
#define TRION_PLL_BITWIDTH	16

/* TRION PLL specific settings and offsets */
#define TRION_PLL_CAL_L_VAL	0x8
#define TRION_PLL_USER_CTL	0xc
#define TRION_PLL_USER_CTL_U	0x10
#define TRION_PLL_USER_CTL_U1	0x14
#define TRION_PLL_CONFIG_CTL	0x18
#define TRION_PLL_CONFIG_CTL_U	0x1c
#define TRION_PLL_CONFIG_CTL_U1	0x20
#define TRION_PLL_TEST_CTL	0x24
#define TRION_PLL_TEST_CTL_U	0x28
#define TRION_PLL_TEST_CTL_U1	0x2c
#define TRION_PLL_OPMODE	0x38
#define TRION_PLL_ALPHA_VAL	0x40
#define TRION_PLL_STATUS	0x30

#define TRION_PLL_CAL_VAL	0x44
#define TRION_PLL_STANDBY	0x0
#define TRION_PLL_RUN		0x1
#define TRION_PLL_OUT_MASK	0x7
#define TRION_PCAL_DONE		BIT(26)
#define TRION_PLL_RATE_MARGIN	500
#define TRION_PLL_ACK_LATCH	BIT(29)
#define TRION_PLL_UPDATE	BIT(22)
#define TRION_PLL_HW_UPDATE_LOGIC_BYPASS	BIT(23)

#define XO_RATE			19200000

/* REGERA PLL specific settings and offsets */
#define REGERA_PLL_USER_CTL	0xc
#define REGERA_PLL_CONFIG_CTL	0x10
#define REGERA_PLL_CONFIG_CTL_U	0x14
#define REGERA_PLL_CONFIG_CTL_U1	0x18
#define REGERA_PLL_TEST_CTL	0x1c
#define REGERA_PLL_TEST_CTL_U	0x20
#define REGERA_PLL_TEST_CTL_U1	0x24
#define REGERA_PLL_OPMODE	0x28

#define REGERA_PLL_OFF		0x0
#define REGERA_PLL_RUN		0x1
#define REGERA_PLL_OUT_MASK	0x9

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
	u64 time;
	struct clk_hw *hw = &pll->clkr.hw;
	const char *name = clk_hw_get_name(hw);

	off = pll->offset;
	ret = regmap_read(pll->clkr.regmap, off + PLL_MODE, &val);
	if (ret)
		return ret;

	time = sched_clock();

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

	time = sched_clock() - time;

	pr_err("PLL lock bit detection total wait time: %lld ns", time);

	WARN_CLK(hw->core, name, 1, "failed to %s!\n", action);

	return -ETIMEDOUT;
}

#define wait_for_pll_enable_active(pll) \
	wait_for_pll(pll, PLL_ACTIVE_FLAG, 0, "enable")

#define wait_for_pll_enable_lock(pll) \
	wait_for_pll(pll, PLL_LOCK_DET, 0, "enable")

#define wait_for_pll_disable(pll) \
	wait_for_pll(pll, PLL_ACTIVE_FLAG, 1, "disable")

#define wait_for_pll_offline(pll) \
	wait_for_pll(pll, PLL_OFFLINE_ACK, 0, "offline")

#define wait_for_regera_pll_freq_lock(pll) \
	wait_for_pll(pll, PLL_LOCK_DET, 0, "lock after rate change")

#define wait_for_pll_latch_ack(pll) \
	wait_for_pll(pll, PLL_ACK_LATCH, 0, "latch ack")

#define wait_for_pll_update(pll) \
	wait_for_pll(pll, PLL_UPDATE, 1, "pll update")

void clk_alpha_pll_configure(struct clk_alpha_pll *pll, struct regmap *regmap,
			     const struct alpha_pll_config *config)
{
	u32 val, mask;

	regmap_write(regmap, pll->offset + PLL_L_VAL, config->l);
	regmap_write(regmap, pll->offset + PLL_ALPHA_VAL, config->alpha);
	regmap_write(regmap, pll->offset + PLL_ALPHA_VAL_U, config->alpha_u);

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

	if (config->test_ctl_mask) {
		mask = config->test_ctl_mask;
		val = config->test_ctl_val;
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
		qcom_pll_set_fsm_mode(regmap, pll->offset + PLL_MODE,
					6, 0);
}

static int clk_alpha_pll_hwfsm_enable(struct clk_hw *hw)
{
	int ret;
	u32 val, off;
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);

	off = pll->offset;
	ret = regmap_read(pll->clkr.regmap, off + PLL_MODE, &val);
	if (ret)
		return ret;

	val |= PLL_FSM_ENA;

	if (pll->flags & SUPPORTS_OFFLINE_REQ)
		val &= ~PLL_OFFLINE_REQ;

	ret = regmap_write(pll->clkr.regmap, off + PLL_MODE, val);
	if (ret)
		return ret;

	/* Make sure enable request goes through before waiting for update */
	mb();

	return wait_for_pll_enable_active(pll);
}

static void clk_alpha_pll_hwfsm_disable(struct clk_hw *hw)
{
	int ret;
	u32 val, off;
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);

	off = pll->offset;
	ret = regmap_read(pll->clkr.regmap, off + PLL_MODE, &val);
	if (ret)
		return;

	if (pll->flags & SUPPORTS_OFFLINE_REQ) {
		ret = regmap_update_bits(pll->clkr.regmap, off + PLL_MODE,
					 PLL_OFFLINE_REQ, PLL_OFFLINE_REQ);
		if (ret)
			return;

		ret = wait_for_pll_offline(pll);
		if (ret)
			return;
	}

	/* Disable hwfsm */
	ret = regmap_update_bits(pll->clkr.regmap, off + PLL_MODE,
				 PLL_FSM_ENA, 0);
	if (ret)
		return;

	wait_for_pll_disable(pll);
}

static int pll_is_enabled(struct clk_hw *hw, u32 mask)
{
	int ret;
	u32 val, off;
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);

	off = pll->offset;
	ret = regmap_read(pll->clkr.regmap, off + PLL_MODE, &val);
	if (ret)
		return ret;

	return !!(val & mask);
}

static int clk_alpha_pll_hwfsm_is_enabled(struct clk_hw *hw)
{
	return pll_is_enabled(hw, PLL_ACTIVE_FLAG);
}

static int clk_alpha_pll_is_enabled(struct clk_hw *hw)
{
	return pll_is_enabled(hw, PLL_LOCK_DET);
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
		ret = wait_for_pll_enable_active(pll);
		if (ret == 0)
			if (pll->flags & SUPPORTS_FSM_VOTE)
				*pll->soft_vote |= (pll->soft_vote_mask);
		return ret;
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

	ret = wait_for_pll_enable_lock(pll);
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

static unsigned long alpha_pll_calc_rate(const struct clk_alpha_pll *pll,
						u64 prate, u32 l, u32 a)
{
	int alpha_bw = ALPHA_BITWIDTH;

	if (pll->type == TRION_PLL || pll->type == REGERA_PLL)
		alpha_bw = TRION_PLL_BITWIDTH;

	return (prate * l) + ((prate * a) >> alpha_bw);
}

static unsigned long
alpha_pll_round_rate(const struct clk_alpha_pll *pll, unsigned long rate,
				unsigned long prate, u32 *l, u64 *a)
{
	u64 remainder;
	u64 quotient;
	int alpha_bw = ALPHA_BITWIDTH;

	/*
	 * The PLLs parent rate is zero probably since the parent hasn't
	 * registered yet. Return early with the requested rate.
	 */
	if (!prate) {
		pr_warn("PLLs parent rate hasn't been initialized.\n");
		return rate;
	}

	quotient = rate;
	remainder = do_div(quotient, prate);
	*l = quotient;

	if (!remainder) {
		*a = 0;
		return rate;
	}

	/* Trion PLLs only have 16 bits to program the fractional divider */
	if (pll->type == TRION_PLL || pll->type == REGERA_PLL)
		alpha_bw = TRION_PLL_BITWIDTH;

	/* Upper ALPHA_BITWIDTH bits of Alpha */
	quotient = remainder << alpha_bw;
	remainder = do_div(quotient, prate);

	if (remainder)
		quotient++;

	*a = quotient;
	return alpha_pll_calc_rate(pll, prate, *l, *a);
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
		if (pll->flags & SUPPORTS_16BIT_ALPHA) {
			a = low & ALPHA_16BIT_MASK;
		} else {
			regmap_read(pll->clkr.regmap, off + PLL_ALPHA_VAL_U,
				    &high);
			a = (u64)high << 32 | low;
			a >>= ALPHA_REG_BITWIDTH - ALPHA_BITWIDTH;
		}
	}

	ctl >>= PLL_POST_DIV_SHIFT;
	ctl &= PLL_POST_DIV_MASK;

	return alpha_pll_calc_rate(pll, prate, l, a) >> fls(ctl);
}

static int clk_alpha_pll_dynamic_update(struct clk_alpha_pll *pll)
{
	int ret;
	u32 off = pll->offset;

	/* Latch the input to the PLL */
	regmap_update_bits(pll->clkr.regmap, off + PLL_MODE,
				PLL_UPDATE, PLL_UPDATE);

	/* Wait for 2 reference cycle before checking ACK bit */
	udelay(1);

	ret = wait_for_pll_latch_ack(pll);
	if (ret)
		return ret;

	/* Return latch input to 0 */
	regmap_update_bits(pll->clkr.regmap, off + PLL_MODE,
				PLL_UPDATE, (u32)~PLL_UPDATE);

	ret = wait_for_pll_enable_lock(pll);
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

	rrate = alpha_pll_round_rate(pll, rate, prate, &l, &a);
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

	regmap_write(pll->clkr.regmap, off + PLL_L_VAL, l);

	if (pll->flags & SUPPORTS_16BIT_ALPHA) {
		regmap_write(pll->clkr.regmap, off + PLL_ALPHA_VAL,
			     a & ALPHA_16BIT_MASK);
	} else {
		a <<= (ALPHA_REG_BITWIDTH - ALPHA_BITWIDTH);
		regmap_write(pll->clkr.regmap, off + PLL_ALPHA_VAL_U, a >> 32);
	}

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

	rate = alpha_pll_round_rate(pll, rate, *prate, &l, &a);
	if (pll->type == ALPHA_PLL && alpha_pll_find_vco(pll, rate))
		return rate;

	min_freq = pll->vco_table[0].min_freq;
	max_freq = pll->vco_table[pll->num_vco - 1].max_freq;

	return clamp(rate, min_freq, max_freq);
}

static void print_pll_registers(struct seq_file *f, struct clk_hw *hw,
		struct clk_register_data *pll_regs, int size,
		struct clk_register_data *pll_vote_reg)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	int i, val;

	for (i = 0; i < size; i++) {
		regmap_read(pll->clkr.regmap, pll->offset + pll_regs[i].offset,
					&val);
		clock_debug_output(f, false, "%20s: 0x%.8x\n", pll_regs[i].name,
					val);
	}

	regmap_read(pll->clkr.regmap, pll->offset + PLL_MODE, &val);

	if (val & PLL_FSM_ENA) {
		regmap_read(pll->clkr.regmap, pll->clkr.enable_reg +
					pll_vote_reg->offset, &val);
		clock_debug_output(f, false, "%20s: 0x%.8x\n",
					pll_vote_reg->name, val);
	}
}

static void clk_alpha_pll_list_registers(struct seq_file *f, struct clk_hw *hw)
{
	static struct clk_register_data pll_regs[] = {
		{"PLL_MODE", 0x0},
		{"PLL_L_VAL", 0x4},
		{"PLL_ALPHA_VAL", 0x8},
		{"PLL_ALPHA_VAL_U", 0xC},
		{"PLL_USER_CTL", 0x10},
		{"PLL_USER_CTL_U", 0x14},
		{"PLL_CONFIG_CTL", 0x18},
		{"PLL_TEST_CTL", 0x1c},
		{"PLL_TEST_CTL_U", 0x20},
		{"PLL_STATUS", 0x24},
	};

	static struct clk_register_data pll_vote_reg = {
		"APSS_PLL_VOTE", 0x0
	};

	print_pll_registers(f, hw, pll_regs, ARRAY_SIZE(pll_regs),
							&pll_vote_reg);
}

static int trion_pll_is_enabled(struct clk_alpha_pll *pll,
					struct regmap *regmap)
{
	u32 mode_regval, opmode_regval;
	int ret;

	ret = regmap_read(regmap, pll->offset + PLL_MODE, &mode_regval);
	ret |= regmap_read(regmap, pll->offset + TRION_PLL_OPMODE,
					&opmode_regval);
	if (ret)
		return 0;

	return ((opmode_regval & TRION_PLL_RUN) && (mode_regval & PLL_OUTCTRL));
}

int clk_trion_pll_configure(struct clk_alpha_pll *pll, struct regmap *regmap,
				const struct alpha_pll_config *config)
{
	int ret = 0;

	if (pll->inited)
		return ret;

	if (trion_pll_is_enabled(pll, regmap)) {
		pr_warn("PLL is already enabled. Skipping configuration.\n");
		pll->inited = true;
		return ret;
	}

	if (config->l)
		regmap_write(regmap, pll->offset + PLL_L_VAL,
						config->l);

	regmap_write(regmap, pll->offset + TRION_PLL_CAL_L_VAL,
						TRION_PLL_CAL_VAL);

	if (config->alpha)
		regmap_write(regmap, pll->offset + TRION_PLL_ALPHA_VAL,
						config->alpha);
	if (config->config_ctl_val)
		regmap_write(regmap, pll->offset + TRION_PLL_CONFIG_CTL,
				config->config_ctl_val);

	if (config->config_ctl_hi_val)
		regmap_write(regmap, pll->offset + TRION_PLL_CONFIG_CTL_U,
				config->config_ctl_hi_val);

	if (config->config_ctl_hi1_val)
		regmap_write(regmap, pll->offset + TRION_PLL_CONFIG_CTL_U1,
				config->config_ctl_hi1_val);

	if (config->user_ctl_val)
		regmap_write(regmap, pll->offset + TRION_PLL_USER_CTL,
				config->user_ctl_val);

	if (config->user_ctl_hi_val)
		regmap_write(regmap, pll->offset + TRION_PLL_USER_CTL_U,
				config->user_ctl_hi_val);

	if (config->user_ctl_hi1_val)
		regmap_write(regmap, pll->offset + TRION_PLL_USER_CTL_U1,
				config->user_ctl_hi1_val);

	if (config->test_ctl_val)
		regmap_write(regmap, pll->offset + TRION_PLL_TEST_CTL,
				config->test_ctl_val);

	if (config->test_ctl_hi_val)
		regmap_write(regmap, pll->offset + TRION_PLL_TEST_CTL_U,
				config->test_ctl_hi_val);

	if (config->test_ctl_hi1_val)
		regmap_write(regmap, pll->offset + TRION_PLL_TEST_CTL_U1,
				config->test_ctl_hi1_val);

	regmap_update_bits(regmap, pll->offset + PLL_MODE,
				 TRION_PLL_HW_UPDATE_LOGIC_BYPASS,
				 TRION_PLL_HW_UPDATE_LOGIC_BYPASS);

	/* Disable PLL output */
	ret = regmap_update_bits(regmap, pll->offset + PLL_MODE,
					PLL_OUTCTRL, 0);
	if (ret)
		return ret;

	/* Set operation mode to OFF */
	regmap_write(regmap, pll->offset + TRION_PLL_OPMODE,
					TRION_PLL_STANDBY);

	/* PLL should be in OFF mode before continuing */
	wmb();

	/* Place the PLL in STANDBY mode */
	ret = regmap_update_bits(regmap, pll->offset + PLL_MODE,
				 PLL_RESET_N, PLL_RESET_N);
	if (ret)
		return ret;

	pll->inited = true;
	return 0;
}

static int clk_trion_pll_enable(struct clk_hw *hw)
{
	int ret;
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	u32 val, off = pll->offset;

	ret = regmap_read(pll->clkr.regmap, off + PLL_MODE, &val);
	if (ret)
		return ret;

	/* If in FSM mode, just vote for it */
	if (val & PLL_VOTE_FSM_ENA) {
		ret = clk_enable_regmap(hw);
		if (ret)
			return ret;
		return wait_for_pll_enable_active(pll);
	}

	if (unlikely(!pll->inited)) {
		ret = clk_trion_pll_configure(pll, pll->clkr.regmap,
						pll->config);
		if (ret) {
			pr_err("Failed to configure %s\n", clk_hw_get_name(hw));
			return ret;
		}
	}

	/* Set operation mode to RUN */
	regmap_write(pll->clkr.regmap, off + TRION_PLL_OPMODE, TRION_PLL_RUN);

	ret = wait_for_pll_enable_lock(pll);
	if (ret)
		return ret;

	/* Enable the PLL outputs */
	ret = regmap_update_bits(pll->clkr.regmap, off + TRION_PLL_USER_CTL,
				 TRION_PLL_OUT_MASK, TRION_PLL_OUT_MASK);
	if (ret)
		return ret;

	/* Enable the global PLL outputs */
	ret = regmap_update_bits(pll->clkr.regmap, off + PLL_MODE,
				 PLL_OUTCTRL, PLL_OUTCTRL);
	if (ret)
		return ret;

	/* Ensure that the write above goes through before returning. */
	mb();
	return ret;
}

static void clk_trion_pll_disable(struct clk_hw *hw)
{
	int ret;
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	u32 val, off = pll->offset;

	ret = regmap_read(pll->clkr.regmap, off + PLL_MODE, &val);
	if (ret)
		return;

	/* If in FSM mode, just unvote it */
	if (val & PLL_VOTE_FSM_ENA) {
		clk_disable_regmap(hw);
		return;
	}

	/* Disable the global PLL output */
	ret = regmap_update_bits(pll->clkr.regmap, off + PLL_MODE,
							PLL_OUTCTRL, 0);
	if (ret)
		return;

	/* Disable the PLL outputs */
	ret = regmap_update_bits(pll->clkr.regmap, off + TRION_PLL_USER_CTL,
			TRION_PLL_OUT_MASK, 0);
	if (ret)
		return;

	/* Place the PLL mode in STANDBY */
	regmap_write(pll->clkr.regmap, off + TRION_PLL_OPMODE,
			TRION_PLL_STANDBY);

	regmap_update_bits(pll->clkr.regmap, off + PLL_MODE, PLL_RESET_N,
			PLL_RESET_N);
}

/*
 * The Trion PLL requires a power-on self-calibration which happens when the
 * PLL comes out of reset. The calibration is performed at an output frequency
 * of ~1300 MHz which means that SW will have to vote on a voltage that's
 * equal to or greater than SVS_L1 on the corresponding rail. Since this is not
 * feasable to do in the atomic enable path, temporarily bring up the PLL here,
 * let it calibrate, and place it in standby before returning.
 */
static int clk_trion_pll_prepare(struct clk_hw *hw)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	u32 regval;
	int ret = 0;

	/* Return early if calibration is not needed. */
	regmap_read(pll->clkr.regmap, pll->offset + TRION_PLL_STATUS, &regval);
	if (regval & TRION_PCAL_DONE)
		return ret;

	ret = clk_vote_rate_vdd(hw->core, TRION_PLL_CAL_VAL * XO_RATE);
	if (ret)
		return ret;

	ret = clk_trion_pll_enable(hw);
	if (ret)
		goto ret_path;

	clk_trion_pll_disable(hw);
ret_path:
	clk_unvote_rate_vdd(hw->core, TRION_PLL_CAL_VAL * XO_RATE);
	return ret;
}

static unsigned long
clk_trion_pll_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	u32 l, frac;
	u64 prate = parent_rate;
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	u32 off = pll->offset;

	regmap_read(pll->clkr.regmap, off + PLL_L_VAL, &l);
	regmap_read(pll->clkr.regmap, off + TRION_PLL_ALPHA_VAL, &frac);

	return alpha_pll_calc_rate(pll, prate, l, frac);
}

static int clk_trion_pll_set_rate(struct clk_hw *hw, unsigned long rate,
				  unsigned long prate)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	unsigned long rrate;
	u32 regval, l, off = pll->offset;
	u64 a;
	int ret;

	rrate = alpha_pll_round_rate(pll, rate, prate, &l, &a);
	/*
	 * Due to a limited number of bits for fractional rate programming, the
	 * rounded up rate could be marginally higher than the requested rate.
	 */
	if (rrate > (rate + TRION_PLL_RATE_MARGIN) || rrate < rate) {
		pr_err("Call set rate on the PLL with rounded rates!\n");
		return -EINVAL;
	}

	regmap_write(pll->clkr.regmap, off + PLL_L_VAL, l);
	regmap_write(pll->clkr.regmap, off + TRION_PLL_ALPHA_VAL, a);

	/* Latch the PLL input */
	ret = regmap_update_bits(pll->clkr.regmap, off + PLL_MODE,
			   TRION_PLL_UPDATE, TRION_PLL_UPDATE);
	if (ret)
		return ret;

	/* Wait for 2 reference cycles before checking the ACK bit. */
	udelay(1);
	regmap_read(pll->clkr.regmap, off + PLL_MODE, &regval);
	if (!(regval & TRION_PLL_ACK_LATCH)) {
		WARN(1, "PLL latch failed. Output may be unstable!\n");
		return -EINVAL;
	}

	/* Return the latch input to 0 */
	ret = regmap_update_bits(pll->clkr.regmap, off + PLL_MODE,
			   TRION_PLL_UPDATE, 0);
	if (ret)
		return ret;

	if (clk_hw_is_enabled(hw)) {
		ret = wait_for_pll_enable_lock(pll);
		if (ret)
			return ret;
	}

	/* Wait for PLL output to stabilize */
	udelay(100);
	return 0;
}

static int clk_trion_pll_is_enabled(struct clk_hw *hw)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);

	return trion_pll_is_enabled(pll, pll->clkr.regmap);
}

static void clk_trion_pll_list_registers(struct seq_file *f, struct clk_hw *hw)
{
	static struct clk_register_data pll_regs[] = {
		{"PLL_MODE", 0x0},
		{"PLL_L_VAL", 0x4},
		{"PLL_CAL_L_VAL", 0x8},
		{"PLL_USER_CTL", 0xC},
		{"PLL_CONFIG_CTL", 0x18},
		{"PLL_OPMODE", 0x38},
		{"PLL_ALPHA_VAL", 0x40},
	};

	static struct clk_register_data pll_vote_reg = {
		"APSS_PLL_VOTE", 0x0
	};

	print_pll_registers(f, hw, pll_regs, ARRAY_SIZE(pll_regs),
							&pll_vote_reg);
}

int clk_regera_pll_configure(struct clk_alpha_pll *pll, struct regmap *regmap,
				const struct alpha_pll_config *config)
{
	u32 mode_regval;
	int ret = 0;

	if (pll->inited)
		return ret;

	ret = regmap_read(regmap, pll->offset + PLL_MODE, &mode_regval);
	if (ret)
		return ret;

	if (mode_regval & PLL_LOCK_DET) {
		pr_warn("PLL is already enabled. Skipping configuration.\n");
		pll->inited = true;
		return 0;
	}

	if (config->alpha)
		regmap_write(regmap, pll->offset + PLL_ALPHA_VAL,
						config->alpha);

	if (config->l)
		regmap_write(regmap, pll->offset + PLL_L_VAL, config->l);

	if (config->config_ctl_val)
		regmap_write(regmap, pll->offset + REGERA_PLL_CONFIG_CTL,
				config->config_ctl_val);

	if (config->config_ctl_hi_val)
		regmap_write(regmap, pll->offset + REGERA_PLL_CONFIG_CTL_U,
				config->config_ctl_hi_val);

	if (config->config_ctl_hi1_val)
		regmap_write(regmap, pll->offset + REGERA_PLL_CONFIG_CTL_U1,
				config->config_ctl_hi1_val);

	if (config->user_ctl_val)
		regmap_write(regmap, pll->offset + REGERA_PLL_USER_CTL,
				config->user_ctl_val);

	if (config->test_ctl_val)
		regmap_write(regmap, pll->offset + REGERA_PLL_TEST_CTL,
				config->test_ctl_val);

	if (config->test_ctl_hi_val)
		regmap_write(regmap, pll->offset + REGERA_PLL_TEST_CTL_U,
				config->test_ctl_hi_val);

	if (config->test_ctl_hi1_val)
		regmap_write(regmap, pll->offset + REGERA_PLL_TEST_CTL_U1,
				config->test_ctl_hi1_val);

	/* Set operation mode to OFF */
	regmap_write(regmap, pll->offset + REGERA_PLL_OPMODE, REGERA_PLL_OFF);

	/* PLL should be in OFF mode before continuing */
	wmb();

	pll->inited = true;
	return 0;
}

static int clk_regera_pll_enable(struct clk_hw *hw)
{
	int ret;
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	u32 val, off = pll->offset;

	ret = regmap_read(pll->clkr.regmap, off + PLL_MODE, &val);
	if (ret)
		return ret;

	/* If in FSM mode, just vote for it */
	if (val & PLL_VOTE_FSM_ENA) {
		ret = clk_enable_regmap(hw);
		if (ret)
			return ret;
		return wait_for_pll_enable_active(pll);
	}

	if (unlikely(!pll->inited)) {
		ret = clk_regera_pll_configure(pll, pll->clkr.regmap,
						pll->config);
		if (ret) {
			pr_err("Failed to configure %s\n", clk_hw_get_name(hw));
			return ret;
		}
	}

	/* Get the PLL out of bypass mode */
	ret = regmap_update_bits(pll->clkr.regmap, off + PLL_MODE,
						PLL_BYPASSNL, PLL_BYPASSNL);
	if (ret)
		return ret;

	/*
	 * H/W requires a 1us delay between disabling the bypass and
	 * de-asserting the reset.
	 */
	mb();
	udelay(1);

	ret = regmap_update_bits(pll->clkr.regmap, off + PLL_MODE,
						 PLL_RESET_N, PLL_RESET_N);
	if (ret)
		return ret;

	/* Set operation mode to RUN */
	regmap_write(pll->clkr.regmap, off + REGERA_PLL_OPMODE,
						REGERA_PLL_RUN);

	ret = wait_for_pll_enable_lock(pll);
	if (ret)
		return ret;

	/* Enable the PLL outputs */
	ret = regmap_update_bits(pll->clkr.regmap, off + REGERA_PLL_USER_CTL,
				REGERA_PLL_OUT_MASK, REGERA_PLL_OUT_MASK);
	if (ret)
		return ret;

	/* Enable the global PLL outputs */
	ret = regmap_update_bits(pll->clkr.regmap, off + PLL_MODE,
				 PLL_OUTCTRL, PLL_OUTCTRL);
	if (ret)
		return ret;

	/* Ensure that the write above goes through before returning. */
	mb();
	return ret;
}

static void clk_regera_pll_disable(struct clk_hw *hw)
{
	int ret;
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	u32 val, mask, off = pll->offset;

	ret = regmap_read(pll->clkr.regmap, off + PLL_MODE, &val);
	if (ret)
		return;

	/* If in FSM mode, just unvote it */
	if (val & PLL_VOTE_FSM_ENA) {
		clk_disable_regmap(hw);
		return;
	}

	/* Disable the global PLL output */
	ret = regmap_update_bits(pll->clkr.regmap, off + PLL_MODE,
							PLL_OUTCTRL, 0);
	if (ret)
		return;

	/* Disable the PLL outputs */
	ret = regmap_update_bits(pll->clkr.regmap, off + REGERA_PLL_USER_CTL,
					REGERA_PLL_OUT_MASK, 0);

	/* Put the PLL in bypass and reset */
	mask = PLL_RESET_N | PLL_BYPASSNL;
	ret = regmap_update_bits(pll->clkr.regmap, off + PLL_MODE, mask, 0);
	if (ret)
		return;

	/* Place the PLL mode in OFF state */
	regmap_write(pll->clkr.regmap, off + REGERA_PLL_OPMODE,
			REGERA_PLL_OFF);
}

static int clk_regera_pll_set_rate(struct clk_hw *hw, unsigned long rate,
				  unsigned long prate)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	unsigned long rrate;
	u32 l, regval, off = pll->offset;
	u64 a;
	int ret;

	rrate = alpha_pll_round_rate(pll, rate, prate, &l, &a);
	/*
	 * Due to a limited number of bits for fractional rate programming, the
	 * rounded up rate could be marginally higher than the requested rate.
	 */
	if (rrate > (rate + TRION_PLL_RATE_MARGIN) || rrate < rate) {
		pr_err("Requested rate (%lu) not matching the PLL's supported frequency (%lu)\n",
				rate, rrate);
		return -EINVAL;
	}

	regmap_write(pll->clkr.regmap, off + PLL_ALPHA_VAL, a);
	regmap_write(pll->clkr.regmap, off + PLL_L_VAL, l);

	/* Return early if the PLL is disabled */
	ret = regmap_read(pll->clkr.regmap, off + REGERA_PLL_OPMODE, &regval);
	if (ret)
		return ret;
	else if (regval == REGERA_PLL_OFF)
		return 0;

	/* Wait before polling for the frequency latch */
	udelay(5);

	ret = wait_for_regera_pll_freq_lock(pll);
	if (ret)
		return ret;

	/* Wait for PLL output to stabilize */
	udelay(100);
	return 0;
}

static unsigned long
clk_regera_pll_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	u32 l, frac;
	u64 prate = parent_rate;
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	u32 off = pll->offset;

	regmap_read(pll->clkr.regmap, off + PLL_L_VAL, &l);
	regmap_read(pll->clkr.regmap, off + PLL_ALPHA_VAL, &frac);

	return alpha_pll_calc_rate(pll, prate, l, frac);
}

static void clk_regera_pll_list_registers(struct seq_file *f, struct clk_hw *hw)
{
	static struct clk_register_data pll_regs[] = {
		{"PLL_MODE", 0x0},
		{"PLL_L_VAL", 0x4},
		{"PLL_ALPHA_VAL", 0x8},
		{"PLL_USER_CTL", 0xC},
		{"PLL_CONFIG_CTL", 0x10},
		{"PLL_OPMODE", 0x28},
	};

	static struct clk_register_data pll_vote_reg = {
		"APSS_PLL_VOTE", 0x0
	};

	print_pll_registers(f, hw, pll_regs, ARRAY_SIZE(pll_regs),
							&pll_vote_reg);
}

const struct clk_ops clk_pll_sleep_vote_ops = {
	.enable = clk_enable_regmap,
	.disable = clk_disable_regmap,
	.list_registers = clk_alpha_pll_list_registers,
};
EXPORT_SYMBOL(clk_pll_sleep_vote_ops);

const struct clk_ops clk_alpha_pll_ops = {
	.enable = clk_alpha_pll_enable,
	.disable = clk_alpha_pll_disable,
	.is_enabled = clk_alpha_pll_is_enabled,
	.recalc_rate = clk_alpha_pll_recalc_rate,
	.round_rate = clk_alpha_pll_round_rate,
	.set_rate = clk_alpha_pll_set_rate,
	.list_registers = clk_alpha_pll_list_registers,
};
EXPORT_SYMBOL_GPL(clk_alpha_pll_ops);

const struct clk_ops clk_alpha_pll_hwfsm_ops = {
	.enable = clk_alpha_pll_hwfsm_enable,
	.disable = clk_alpha_pll_hwfsm_disable,
	.is_enabled = clk_alpha_pll_hwfsm_is_enabled,
	.recalc_rate = clk_alpha_pll_recalc_rate,
	.round_rate = clk_alpha_pll_round_rate,
	.set_rate = clk_alpha_pll_set_rate,
	.list_registers = clk_alpha_pll_list_registers,
};
EXPORT_SYMBOL_GPL(clk_alpha_pll_hwfsm_ops);

const struct clk_ops clk_trion_pll_ops = {
	.prepare = clk_trion_pll_prepare,
	.enable = clk_trion_pll_enable,
	.disable = clk_trion_pll_disable,
	.is_enabled = clk_trion_pll_is_enabled,
	.recalc_rate = clk_trion_pll_recalc_rate,
	.round_rate = clk_alpha_pll_round_rate,
	.set_rate = clk_trion_pll_set_rate,
	.list_registers = clk_trion_pll_list_registers,
};
EXPORT_SYMBOL_GPL(clk_trion_pll_ops);

const struct clk_ops clk_trion_fixed_pll_ops = {
	.enable = clk_trion_pll_enable,
	.disable = clk_trion_pll_disable,
	.is_enabled = clk_trion_pll_is_enabled,
	.recalc_rate = clk_trion_pll_recalc_rate,
	.round_rate = clk_alpha_pll_round_rate,
	.list_registers = clk_trion_pll_list_registers,
};
EXPORT_SYMBOL_GPL(clk_trion_fixed_pll_ops);

const struct clk_ops clk_regera_pll_ops = {
	.enable = clk_regera_pll_enable,
	.disable = clk_regera_pll_disable,
	.is_enabled = clk_alpha_pll_is_enabled,
	.recalc_rate = clk_regera_pll_recalc_rate,
	.round_rate = clk_alpha_pll_round_rate,
	.set_rate = clk_regera_pll_set_rate,
	.list_registers = clk_regera_pll_list_registers,
};
EXPORT_SYMBOL_GPL(clk_regera_pll_ops);

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

static unsigned long clk_trion_pll_postdiv_recalc_rate(struct clk_hw *hw,
				unsigned long parent_rate)
{
	struct clk_alpha_pll_postdiv *pll = to_clk_alpha_pll_postdiv(hw);
	u32 i, div = 1, val;

	if (!pll->post_div_table) {
		pr_err("Missing the post_div_table for the PLL\n");
		return -EINVAL;
	}

	regmap_read(pll->clkr.regmap, pll->offset + TRION_PLL_USER_CTL, &val);

	val >>= pll->post_div_shift;
	val &= PLL_POST_DIV_MASK;

	for (i = 0; i < pll->num_post_div; i++) {
		if (pll->post_div_table[i].val == val) {
			div = pll->post_div_table[i].div;
			break;
		}
	}

	return (parent_rate / div);
}

static long clk_trion_pll_postdiv_round_rate(struct clk_hw *hw,
				unsigned long rate, unsigned long *prate)
{
	struct clk_alpha_pll_postdiv *pll = to_clk_alpha_pll_postdiv(hw);

	if (!pll->post_div_table)
		return -EINVAL;

	return divider_round_rate(hw, rate, prate, pll->post_div_table,
					pll->width, CLK_DIVIDER_ROUND_CLOSEST);
}

static int clk_trion_pll_postdiv_set_rate(struct clk_hw *hw,
				unsigned long rate, unsigned long parent_rate)
{
	struct clk_alpha_pll_postdiv *pll = to_clk_alpha_pll_postdiv(hw);
	int i, val = 0, div, ret;

	/*
	 * If the PLL is in FSM mode, then treat the set_rate callback
	 * as a no-operation.
	 */
	ret = regmap_read(pll->clkr.regmap, pll->offset + PLL_MODE, &val);
	if (ret)
		return ret;

	if (val & PLL_VOTE_FSM_ENA)
		return 0;

	if (!pll->post_div_table) {
		pr_err("Missing the post_div_table for the PLL\n");
		return -EINVAL;
	}

	div = DIV_ROUND_UP_ULL((u64)parent_rate, rate);
	for (i = 0; i < pll->num_post_div; i++) {
		if (pll->post_div_table[i].div == div) {
			val = pll->post_div_table[i].val;
			break;
		}
	}

	return regmap_update_bits(pll->clkr.regmap,
				pll->offset + TRION_PLL_USER_CTL,
				PLL_POST_DIV_MASK << pll->post_div_shift,
				val << pll->post_div_shift);
}

const struct clk_ops clk_trion_pll_postdiv_ops = {
	.recalc_rate = clk_trion_pll_postdiv_recalc_rate,
	.round_rate = clk_trion_pll_postdiv_round_rate,
	.set_rate = clk_trion_pll_postdiv_set_rate,
};
EXPORT_SYMBOL_GPL(clk_trion_pll_postdiv_ops);

static int clk_alpha_pll_slew_update(struct clk_alpha_pll *pll)
{
	int ret = 0;
	u32 val;

	regmap_update_bits(pll->clkr.regmap, pll->offset + PLL_MODE,
					PLL_UPDATE, PLL_UPDATE);
	regmap_read(pll->clkr.regmap, pll->offset + PLL_MODE, &val);

	ret = wait_for_pll_update(pll);
	if (ret)
		return ret;
	/*
	 * HPG mandates a wait of at least 570ns before polling the LOCK
	 * detect bit. Have a delay of 1us just to be safe.
	 */
	mb();
	udelay(1);

	ret = wait_for_pll_enable_lock(pll);

	return ret;
}

static int clk_alpha_pll_calibrate(struct clk_hw *hw);

static int clk_alpha_pll_slew_set_rate(struct clk_hw *hw, unsigned long rate,
			unsigned long parent_rate)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	unsigned long freq_hz;
	const struct pll_vco *curr_vco = NULL, *vco;
	u32 l, ctl;
	u64 a;
	int i = 0, rc;

	if (!clk_hw_is_enabled(hw)) {
		rc = clk_alpha_pll_calibrate(hw);
		if (rc)
			return rc;
	}

	freq_hz = alpha_pll_round_rate(pll, rate, parent_rate, &l, &a);
	if (freq_hz != rate) {
		pr_err("alpha_pll: Call clk_set_rate with rounded rates!\n");
		return -EINVAL;
	}

	regmap_read(pll->clkr.regmap, pll->offset + PLL_USER_CTL, &ctl);
	ctl >>= PLL_POST_DIV_SHIFT;
	ctl &= PLL_POST_DIV_MASK;

	for (i = 0; i < ARRAY_SIZE(clk_alpha_div_table); i++) {
		if (clk_alpha_div_table[i].val == ctl)
			break;
	}

	if (i < ARRAY_SIZE(clk_alpha_div_table))
		curr_vco = alpha_pll_find_vco(pll, clk_hw_get_rate(hw) *
						clk_alpha_div_table[i].div);
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
	struct clk_hw *parent;
	const struct pll_vco *vco = NULL;
	u64 a;
	u32 l, ctl;
	int rc, i = 0;

	parent = clk_hw_get_parent(hw);
	if (!parent) {
		pr_err("alpha pll: no valid parent found\n");
		return -EINVAL;
	}

	regmap_read(pll->clkr.regmap, pll->offset + PLL_USER_CTL, &ctl);
	ctl >>= PLL_POST_DIV_SHIFT;
	ctl &= PLL_POST_DIV_MASK;

	for (i = 0; i < ARRAY_SIZE(clk_alpha_div_table); i++) {
		if (clk_alpha_div_table[i].val == ctl)
			break;
	}

	if (i < ARRAY_SIZE(clk_alpha_div_table))
		vco = alpha_pll_find_vco(pll, clk_hw_get_rate(hw) *
						clk_alpha_div_table[i].div);
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

	freq_hz = alpha_pll_round_rate(pll, calibration_freq,
				clk_hw_get_rate(parent), &l, &a);
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
	freq_hz = alpha_pll_round_rate(pll, clk_hw_get_rate(hw),
				clk_hw_get_rate(parent), &l, &a);

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
	return clk_alpha_pll_enable(hw);
}

const struct clk_ops clk_alpha_pll_slew_ops = {
	.enable = clk_alpha_pll_slew_enable,
	.disable = clk_alpha_pll_disable,
	.recalc_rate = clk_alpha_pll_recalc_rate,
	.round_rate = clk_alpha_pll_round_rate,
	.set_rate = clk_alpha_pll_slew_set_rate,
	.list_registers = clk_alpha_pll_list_registers,
};
EXPORT_SYMBOL(clk_alpha_pll_slew_ops);
