/*
 * Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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
#include <linux/export.h>
#include <linux/clk-provider.h>
#include <linux/regmap.h>
#include <linux/delay.h>

#include "clk-alpha-pll.h"

#define PLL_MODE		0x00
#define PLL_STANDBY		0x0
#define PLL_RUN			0x1
# define PLL_OUTCTRL		BIT(0)
# define PLL_BYPASSNL		BIT(1)
# define PLL_RESET_N		BIT(2)
# define PLL_LOCK_COUNT_SHIFT	8
# define PLL_LOCK_COUNT_MASK	0x3f
# define PLL_BIAS_COUNT_SHIFT	14
# define PLL_BIAS_COUNT_MASK	0x3f
# define PLL_VOTE_FSM_ENA	BIT(20)
# define PLL_VOTE_FSM_RESET	BIT(21)
# define PLL_ACTIVE_FLAG	BIT(30)
# define PLL_LOCK_DET		BIT(31)

#define PLL_L_VAL		0x04
#define PLL_ALPHA_VAL		0x08
#define PLL_ALPHA_VAL_U		0x0c

#define PLL_USER_CTL		0x10
# define PLL_POST_DIV_SHIFT	8
# define PLL_POST_DIV_MASK	0xf
# define PLL_ALPHA_EN		BIT(24)
# define PLL_VCO_SHIFT		20
# define PLL_VCO_MASK		0x3

#define PLL_USER_CTL_U		0x14

#define PLL_CONFIG_CTL		0x18
#define PLL_TEST_CTL		0x1c
#define PLL_TEST_CTL_U		0x20
#define PLL_STATUS		0x24
#define PLL_UPDATE		BIT(22)
#define PLL_ACK_LATCH		BIT(29)
#define PLL_CALIBRATION_MASK	(0x7<<3)
#define PLL_CALIBRATION_CONTROL	2
#define PLL_HW_UPDATE_LOGIC_BYPASS	BIT(23)
#define ALPHA_16_BIT_PLL_RATE_MARGIN	500

/*
 * Even though 40 bits are present, use only 32 for ease of calculation.
 */
#define ALPHA_REG_BITWIDTH	40
#define ALPHA_BITWIDTH		32
#define SUPPORTS_16BIT_ALPHA	16

#define FABIA_USER_CTL_LO	0xc
#define FABIA_USER_CTL_HI	0x10
#define FABIA_FRAC_VAL		0x38
#define FABIA_OPMODE		0x2c
#define FABIA_PLL_OUT_MASK	0x7
#define FABIA_PLL_ACK_LATCH	BIT(29)
#define FABIA_PLL_UPDATE	BIT(22)

#define TRION_PLL_CAL_VAL	0x44
#define TRION_PLL_CAL_L_VAL	0x8
#define TRION_PLL_USER_CTL	0xc
#define TRION_PLL_USER_CTL_U	0x10
#define TRION_PLL_USER_CTL_U1	0x14
#define TRION_PLL_CONFIG_CTL_U	0x1c
#define TRION_PLL_CONFIG_CTL_U1	0x20
#define TRION_PLL_OPMODE	0x38
#define TRION_PLL_ALPHA_VAL	0x40

#define TRION_PLL_OUT_MASK	0x7
#define TRION_PLL_ENABLE_STATE_READ	BIT(4)

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
		if (inverse && (val & mask))
			return 0;
		else if ((val & mask) == mask)
			return 0;

		udelay(1);
	}

	WARN(1, "clk: %s failed to %s!\n", name, action);
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

/* alpha pll with hwfsm support */

#define PLL_OFFLINE_REQ		BIT(7)
#define PLL_FSM_ENA		BIT(20)
#define PLL_OFFLINE_ACK		BIT(28)
#define PLL_ACTIVE_FLAG		BIT(30)

void clk_alpha_pll_configure(struct clk_alpha_pll *pll, struct regmap *regmap,
			     const struct pll_config *config)
{
	u32 val, mask;

	regmap_write(regmap, pll->offset + PLL_CONFIG_CTL,
			   config->config_ctl_val);

	val = config->main_output_mask;
	val |= config->aux_output_mask;
	val |= config->aux2_output_mask;
	val |= config->early_output_mask;
	val |= config->post_div_val;

	mask = config->main_output_mask;
	mask |= config->aux_output_mask;
	mask |= config->aux2_output_mask;
	mask |= config->early_output_mask;
	mask |= config->post_div_mask;

	regmap_update_bits(regmap, pll->offset + PLL_USER_CTL, mask, val);
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
		return wait_for_pll_enable(pll, PLL_ACTIVE_FLAG);
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

	if (pll->type == FABIA_PLL || pll->type == TRION_PLL)
		alpha_bw = SUPPORTS_16BIT_ALPHA;

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
		pr_debug("PLLs parent rate hasn't been initialized.\n");
		return rate;
	}

	quotient = rate;
	remainder = do_div(quotient, prate);
	*l = quotient;

	if (!remainder) {
		*a = 0;
		return rate;
	}

	/* Some PLLs only have 16 bits to program the fractional divider */
	if (pll->type == FABIA_PLL || pll->type == TRION_PLL)
		alpha_bw = SUPPORTS_16BIT_ALPHA;

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
		regmap_read(pll->clkr.regmap, off + PLL_ALPHA_VAL_U, &high);
		a = (u64)high << 32 | low;
		a >>= ALPHA_REG_BITWIDTH - ALPHA_BITWIDTH;
	}

	return alpha_pll_calc_rate(pll, prate, l, a);
}

static int clk_alpha_pll_set_rate(struct clk_hw *hw, unsigned long rate,
				  unsigned long prate)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	const struct pll_vco *vco;
	u32 l = 0, off = pll->offset;
	u64 a = 0;

	rate = alpha_pll_round_rate(pll, rate, prate, &l, &a);
	vco = alpha_pll_find_vco(pll, rate);
	if (!vco) {
		pr_err("alpha pll not in a valid vco range\n");
		return -EINVAL;
	}

	a <<= (ALPHA_REG_BITWIDTH - ALPHA_BITWIDTH);

	regmap_write(pll->clkr.regmap, off + PLL_L_VAL, l);
	regmap_write(pll->clkr.regmap, off + PLL_ALPHA_VAL, a);
	regmap_write(pll->clkr.regmap, off + PLL_ALPHA_VAL_U, a >> 32);

	regmap_update_bits(pll->clkr.regmap, off + PLL_USER_CTL,
			   PLL_VCO_MASK << PLL_VCO_SHIFT,
			   vco->val << PLL_VCO_SHIFT);

	regmap_update_bits(pll->clkr.regmap, off + PLL_USER_CTL, PLL_ALPHA_EN,
			   PLL_ALPHA_EN);

	return 0;
}

static long clk_alpha_pll_round_rate(struct clk_hw *hw, unsigned long rate,
				     unsigned long *prate)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	u32 l;
	u64 a;
	unsigned long min_freq, max_freq;

	rate = alpha_pll_round_rate(pll, rate, *prate, &l, &a);
	if (pll->type == FABIA_PLL || pll->type == TRION_PLL ||
		alpha_pll_find_vco(pll, rate))
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

static int clk_fabia_pll_latch_input(struct clk_alpha_pll *pll,
					struct regmap *regmap)
{
	u32 regval;
	int ret = 0;

	/* Latch the PLL input */
	ret = regmap_update_bits(regmap, pll->offset + PLL_MODE,
			   FABIA_PLL_UPDATE, FABIA_PLL_UPDATE);
	if (ret)
		return ret;

	/* Wait for 2 reference cycles before checking the ACK bit. */
	udelay(1);
	regmap_read(regmap, pll->offset + PLL_MODE, &regval);
	if (!(regval & FABIA_PLL_ACK_LATCH)) {
		WARN(1, "clk: PLL latch failed. Output may be unstable!\n");
		return -EINVAL;
	}

	/* Return the latch input to 0 */
	ret = regmap_update_bits(regmap, pll->offset + PLL_MODE,
			   FABIA_PLL_UPDATE, 0);
	if (ret)
		return ret;

	/* Wait for PLL output to stabilize */
	udelay(100);
	return ret;
}

void clk_fabia_pll_configure(struct clk_alpha_pll *pll, struct regmap *regmap,
				const struct pll_config *config)
{
	u32 val, mask;

	if (config->l)
		regmap_write(regmap, pll->offset + PLL_L_VAL,
						config->l);

	if (config->frac)
		regmap_write(regmap, pll->offset + FABIA_FRAC_VAL,
						config->frac);

	if (config->config_ctl_val)
		regmap_write(regmap, pll->offset + PLL_CONFIG_CTL,
				config->config_ctl_val);

	if (config->post_div_mask) {
		mask = config->post_div_mask;
		val = config->post_div_val;
		regmap_update_bits(regmap, pll->offset + FABIA_USER_CTL_LO,
					mask, val);
	}

	/*
	 * If the PLL has already been initialized, it would now be in a STANDBY
	 * state. Any new updates to the PLL frequency will require setting the
	 * PLL_UPDATE bit.
	 */
	if (pll->inited)
		clk_fabia_pll_latch_input(pll, regmap);

	regmap_update_bits(regmap, pll->offset + PLL_MODE,
				 PLL_HW_UPDATE_LOGIC_BYPASS,
				 PLL_HW_UPDATE_LOGIC_BYPASS);

	regmap_update_bits(regmap, pll->offset + PLL_MODE,
			   PLL_RESET_N, PLL_RESET_N);

	pll->inited = true;
}

static int clk_fabia_pll_enable(struct clk_hw *hw)
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
		return wait_for_pll_enable(pll, PLL_ACTIVE_FLAG);
	}

	if (unlikely(!pll->inited))
		clk_fabia_pll_configure(pll, pll->clkr.regmap, pll->config);

	/* Disable PLL output */
	ret = regmap_update_bits(pll->clkr.regmap, off + PLL_MODE,
					PLL_OUTCTRL, 0);
	if (ret)
		return ret;

	/* Set operation mode to STANDBY */
	regmap_write(pll->clkr.regmap, off + FABIA_OPMODE, PLL_STANDBY);

	/* PLL should be in STANDBY mode before continuing */
	mb();

	/* Bring PLL out of reset */
	ret = regmap_update_bits(pll->clkr.regmap, off + PLL_MODE,
				 PLL_RESET_N, PLL_RESET_N);
	if (ret)
		return ret;

	/* Set operation mode to RUN */
	regmap_write(pll->clkr.regmap, off + FABIA_OPMODE, PLL_RUN);

	ret = wait_for_pll_enable(pll, PLL_LOCK_DET);
	if (ret)
		return ret;

	/* Enable the main PLL output */
	ret = regmap_update_bits(pll->clkr.regmap, off + FABIA_USER_CTL_LO,
				 FABIA_PLL_OUT_MASK, FABIA_PLL_OUT_MASK);
	if (ret)
		return ret;

	/* Enable PLL outputs */
	ret = regmap_update_bits(pll->clkr.regmap, off + PLL_MODE,
				 PLL_OUTCTRL, PLL_OUTCTRL);
	if (ret)
		return ret;

	/* Ensure that the write above goes through before returning. */
	mb();
	return ret;
}

static void clk_fabia_pll_disable(struct clk_hw *hw)
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

	/* Disable PLL outputs */
	ret = regmap_update_bits(pll->clkr.regmap, off + PLL_MODE,
							PLL_OUTCTRL, 0);
	if (ret)
		return;

	/* Disable the main PLL output */
	ret = regmap_update_bits(pll->clkr.regmap, off + FABIA_USER_CTL_LO,
			FABIA_PLL_OUT_MASK, 0);
	if (ret)
		return;

	/* Place the PLL mode in STANDBY */
	regmap_write(pll->clkr.regmap, off + FABIA_OPMODE, PLL_STANDBY);
}

static unsigned long
clk_fabia_pll_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	u32 l, frac;
	u64 prate = parent_rate;
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	u32 off = pll->offset;

	regmap_read(pll->clkr.regmap, off + PLL_L_VAL, &l);
	regmap_read(pll->clkr.regmap, off + FABIA_FRAC_VAL, &frac);

	return alpha_pll_calc_rate(pll, prate, l, frac);
}

static int clk_fabia_pll_set_rate(struct clk_hw *hw, unsigned long rate,
				  unsigned long prate)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	unsigned long rrate;
	u32 regval = 0, l = 0, off = pll->offset;
	u64 a = 0;
	int ret = 0;

	ret = regmap_read(pll->clkr.regmap, off + PLL_MODE, &regval);
	if (ret)
		return ret;

	rrate = alpha_pll_round_rate(pll, rate, prate, &l, &a);
	/*
	 * Due to limited number of bits for fractional rate programming, the
	 * rounded up rate could be marginally higher than the requested rate.
	 */
	if (rrate > (rate + ALPHA_16_BIT_PLL_RATE_MARGIN) || rrate < rate) {
		pr_err("Call set rate on the PLL with rounded rates!\n");
		return -EINVAL;
	}

	regmap_write(pll->clkr.regmap, off + PLL_L_VAL, l);
	regmap_write(pll->clkr.regmap, off + FABIA_FRAC_VAL, a);

	ret = clk_fabia_pll_latch_input(pll, pll->clkr.regmap);
	return ret;
}

static void clk_fabia_pll_list_registers(struct seq_file *f, struct clk_hw *hw)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	int size, i, val;

	static struct clk_register_data data[] = {
		{"PLL_MODE", 0x0},
		{"PLL_L_VAL", 0x4},
		{"PLL_FRAC_VAL", 0x38},
		{"PLL_USER_CTL", 0xc},
		{"PLL_CONFIG_CTL", 0x14},
		{"PLL_OPMODE", 0x2c},
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

const struct clk_ops clk_fabia_pll_ops = {
	.enable = clk_fabia_pll_enable,
	.disable = clk_fabia_pll_disable,
	.recalc_rate = clk_fabia_pll_recalc_rate,
	.round_rate = clk_alpha_pll_round_rate,
	.set_rate = clk_fabia_pll_set_rate,
	.list_registers = clk_fabia_pll_list_registers,
};
EXPORT_SYMBOL_GPL(clk_fabia_pll_ops);

const struct clk_ops clk_fabia_fixed_pll_ops = {
	.enable = clk_fabia_pll_enable,
	.disable = clk_fabia_pll_disable,
	.recalc_rate = clk_fabia_pll_recalc_rate,
	.round_rate = clk_alpha_pll_round_rate,
	.list_registers = clk_fabia_pll_list_registers,
};
EXPORT_SYMBOL_GPL(clk_fabia_fixed_pll_ops);

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

static unsigned long clk_generic_pll_postdiv_recalc_rate(struct clk_hw *hw,
				unsigned long parent_rate)
{
	struct clk_alpha_pll_postdiv *pll = to_clk_alpha_pll_postdiv(hw);
	u32 i, div = 1, val;

	if (!pll->post_div_table) {
		pr_err("Missing the post_div_table for the PLL\n");
		return -EINVAL;
	}

	regmap_read(pll->clkr.regmap, pll->offset + FABIA_USER_CTL_LO, &val);

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

static long clk_generic_pll_postdiv_round_rate(struct clk_hw *hw,
				unsigned long rate, unsigned long *prate)
{
	struct clk_alpha_pll_postdiv *pll = to_clk_alpha_pll_postdiv(hw);

	if (!pll->post_div_table)
		return -EINVAL;

	return divider_round_rate(hw, rate, prate, pll->post_div_table,
					pll->width, CLK_DIVIDER_ROUND_KHZ);
}

static int clk_generic_pll_postdiv_set_rate(struct clk_hw *hw,
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
				pll->offset + FABIA_USER_CTL_LO,
				PLL_POST_DIV_MASK << pll->post_div_shift,
				val << pll->post_div_shift);
}

const struct clk_ops clk_generic_pll_postdiv_ops = {
	.recalc_rate = clk_generic_pll_postdiv_recalc_rate,
	.round_rate = clk_generic_pll_postdiv_round_rate,
	.set_rate = clk_generic_pll_postdiv_set_rate,
};
EXPORT_SYMBOL_GPL(clk_generic_pll_postdiv_ops);

static int trion_pll_is_enabled(struct clk_alpha_pll *pll,
					struct regmap *regmap)
{
	u32 mode_val, opmode_val, off = pll->offset;
	int ret;

	ret = regmap_read(regmap, off + PLL_MODE, &mode_val);
	ret |= regmap_read(regmap, off + TRION_PLL_OPMODE, &opmode_val);
	if (ret)
		return 0;

	return ((opmode_val & PLL_RUN) && (mode_val & PLL_OUTCTRL));
}

int clk_trion_pll_configure(struct clk_alpha_pll *pll, struct regmap *regmap,
				const struct pll_config *config)
{
	int ret = 0;

	if (trion_pll_is_enabled(pll, regmap)) {
		pr_debug("PLL is already enabled. Skipping configuration.\n");

		/*
		 * Set the PLL_HW_UPDATE_LOGIC_BYPASS bit to latch the input
		 * before continuing.
		 */
		regmap_update_bits(regmap, pll->offset + PLL_MODE,
				 PLL_HW_UPDATE_LOGIC_BYPASS,
				 PLL_HW_UPDATE_LOGIC_BYPASS);

		pll->inited = true;
		return ret;
	}

	/*
	 * Disable the PLL if it's already been initialized. Not doing so might
	 * lead to the PLL running with the old frequency configuration.
	 */
	if (pll->inited) {
		ret = regmap_update_bits(regmap, pll->offset + PLL_MODE,
							PLL_RESET_N, 0);
		if (ret)
			return ret;
	}

	if (config->l)
		regmap_write(regmap, pll->offset + PLL_L_VAL,
							config->l);

	regmap_write(regmap, pll->offset + TRION_PLL_CAL_L_VAL,
						TRION_PLL_CAL_VAL);

	if (config->frac)
		regmap_write(regmap, pll->offset + TRION_PLL_ALPHA_VAL,
						config->frac);

	if (config->config_ctl_val)
		regmap_write(regmap, pll->offset + PLL_CONFIG_CTL,
				config->config_ctl_val);

	if (config->config_ctl_hi_val)
		regmap_write(regmap, pll->offset + TRION_PLL_CONFIG_CTL_U,
				config->config_ctl_hi_val);

	if (config->config_ctl_hi1_val)
		regmap_write(regmap, pll->offset + TRION_PLL_CONFIG_CTL_U1,
				config->config_ctl_hi1_val);

	if (config->post_div_mask)
		regmap_update_bits(regmap, pll->offset + TRION_PLL_USER_CTL,
				config->post_div_mask, config->post_div_val);

	/* Disable state read */
	regmap_update_bits(regmap, pll->offset + TRION_PLL_USER_CTL_U,
				TRION_PLL_ENABLE_STATE_READ, 0);

	regmap_update_bits(regmap, pll->offset + PLL_MODE,
				 PLL_HW_UPDATE_LOGIC_BYPASS,
				 PLL_HW_UPDATE_LOGIC_BYPASS);

	/* Set calibration control to Automatic */
	regmap_update_bits(regmap, pll->offset + TRION_PLL_USER_CTL_U,
			PLL_CALIBRATION_MASK, PLL_CALIBRATION_CONTROL);

	/* Disable PLL output */
	ret = regmap_update_bits(regmap, pll->offset + PLL_MODE,
							PLL_OUTCTRL, 0);
	if (ret)
		return ret;

	/* Set operation mode to OFF */
	regmap_write(regmap, pll->offset + TRION_PLL_OPMODE, PLL_STANDBY);

	/* PLL should be in OFF mode before continuing */
	wmb();

	/* Place the PLL in STANDBY mode */
	ret = regmap_update_bits(regmap, pll->offset + PLL_MODE,
						PLL_RESET_N, PLL_RESET_N);
	if (ret)
		return ret;

	pll->inited = true;

	return ret;
}

static int clk_alpha_pll_latch_l_val(struct clk_alpha_pll *pll)
{
	int ret;

	/* Latch the input to the PLL */
	ret = regmap_update_bits(pll->clkr.regmap, pll->offset + PLL_MODE,
			PLL_UPDATE, PLL_UPDATE);
	if (ret)
		return ret;

	/* Wait for 2 reference cycle before checking ACK bit */
	udelay(1);

	ret = wait_for_pll_latch_ack(pll, PLL_ACK_LATCH);
	if (ret)
		return ret;

	/* Return latch input to 0 */
	ret = regmap_update_bits(pll->clkr.regmap, pll->offset + PLL_MODE,
		PLL_UPDATE, (u32)~PLL_UPDATE);
	if (ret)
		return ret;

	return 0;
}

static int clk_trion_pll_enable(struct clk_hw *hw)
{
	int ret = 0;
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
		return wait_for_pll_enable(pll, PLL_ACTIVE_FLAG);
	}

	if (unlikely(!pll->inited)) {
		ret = clk_trion_pll_configure(pll, pll->clkr.regmap,
						pll->config);
		if (ret) {
			pr_err("Failed to configure %s\n", clk_hw_get_name(hw));
			return ret;
		}
	}

	/* Skip If PLL is already running */
	if (trion_pll_is_enabled(pll, pll->clkr.regmap))
		return ret;

	/* Set operation mode to RUN */
	regmap_write(pll->clkr.regmap, off + TRION_PLL_OPMODE, PLL_RUN);

	ret = wait_for_pll_enable(pll, PLL_LOCK_DET);
	if (ret)
		return ret;

	/* Enable PLL main output */
	ret = regmap_update_bits(pll->clkr.regmap, off + TRION_PLL_USER_CTL,
				 TRION_PLL_OUT_MASK, TRION_PLL_OUT_MASK);
	if (ret)
		return ret;

	/* Enable Global PLL outputs */
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

	/* Disable Global PLL outputs */
	ret = regmap_update_bits(pll->clkr.regmap, off + PLL_MODE,
							PLL_OUTCTRL, 0);
	if (ret)
		return;

	/* Disable the main PLL output */
	ret = regmap_update_bits(pll->clkr.regmap, off + TRION_PLL_USER_CTL,
			TRION_PLL_OUT_MASK, 0);
	if (ret)
		return;

	/* Place the PLL into STANDBY mode */
	regmap_write(pll->clkr.regmap, off + TRION_PLL_OPMODE, PLL_STANDBY);

	regmap_update_bits(pll->clkr.regmap, off + PLL_MODE,
				 PLL_RESET_N, PLL_RESET_N);
}

static unsigned long
clk_trion_pll_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	u32 l, frac = 0;
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
	bool is_enabled;
	int ret;
	u32 l = 0, val = 0, off = pll->offset;
	u64 a = 0;

	rrate = alpha_pll_round_rate(pll, rate, prate, &l, &a);
	/*
	 * Due to limited number of bits for fractional rate programming, the
	 * rounded up rate could be marginally higher than the requested rate.
	 */
	if (rrate > (rate + ALPHA_16_BIT_PLL_RATE_MARGIN) || rrate < rate) {
		pr_err("Trion_pll: Call clk_set_rate with rounded rates!\n");
		return -EINVAL;
	}

	is_enabled = clk_hw_is_enabled(hw);

	if (is_enabled)
		hw->init->ops->disable(hw);

	regmap_write(pll->clkr.regmap, off + PLL_L_VAL, l);
	regmap_write(pll->clkr.regmap, off + TRION_PLL_ALPHA_VAL, a);

	ret = regmap_read(pll->clkr.regmap, off + PLL_MODE, &val);
	if (ret)
		return ret;

	/*
	 * If PLL is in Standby or RUN mode then only latch the L value
	 * Else PLL is in OFF mode and just configure L register - as per
	 * HPG no need to latch input.
	 */
	if (val & PLL_RESET_N)
		clk_alpha_pll_latch_l_val(pll);

	if (is_enabled)
		hw->init->ops->enable(hw);

	/* Wait for PLL output to stabilize */
	udelay(100);

	return ret;
}

static int clk_trion_pll_is_enabled(struct clk_hw *hw)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);

	return trion_pll_is_enabled(pll, pll->clkr.regmap);
}

static void clk_trion_pll_list_registers(struct seq_file *f, struct clk_hw *hw)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	int size, i, val;

	static struct clk_register_data data[] = {
		{"PLL_MODE", 0x0},
		{"PLL_L_VAL", 0x4},
		{"PLL_USER_CTL", 0xc},
		{"PLL_USER_CTL_U", 0x10},
		{"PLL_USER_CTL_U1", 0x14},
		{"PLL_CONFIG_CTL", 0x18},
		{"PLL_CONFIG_CTL_U", 0x1c},
		{"PLL_CONFIG_CTL_U1", 0x20},
		{"PLL_OPMODE", 0x38},
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

	if (val & PLL_VOTE_FSM_ENA) {
		regmap_read(pll->clkr.regmap, pll->clkr.enable_reg +
					data1[0].offset, &val);
		seq_printf(f, "%20s: 0x%.8x\n", data1[0].name, val);
	}
}

const struct clk_ops clk_trion_pll_ops = {
	.enable = clk_trion_pll_enable,
	.disable = clk_trion_pll_disable,
	.recalc_rate = clk_trion_pll_recalc_rate,
	.round_rate = clk_alpha_pll_round_rate,
	.set_rate = clk_trion_pll_set_rate,
	.is_enabled = clk_trion_pll_is_enabled,
	.list_registers = clk_trion_pll_list_registers,
};
EXPORT_SYMBOL(clk_trion_pll_ops);

const struct clk_ops clk_trion_fixed_pll_ops = {
	.enable = clk_trion_pll_enable,
	.disable = clk_trion_pll_disable,
	.recalc_rate = clk_trion_pll_recalc_rate,
	.round_rate = clk_alpha_pll_round_rate,
	.is_enabled = clk_trion_pll_is_enabled,
	.list_registers = clk_trion_pll_list_registers,
};
EXPORT_SYMBOL(clk_trion_fixed_pll_ops);

static unsigned long clk_trion_pll_postdiv_recalc_rate(struct clk_hw *hw,
				unsigned long parent_rate)
{
	struct clk_alpha_pll_postdiv *pll = to_clk_alpha_pll_postdiv(hw);
	u32 i, cal_div = 1, val;

	if (!pll->post_div_table) {
		pr_err("Missing the post_div_table for the PLL\n");
		return -EINVAL;
	}

	regmap_read(pll->clkr.regmap, pll->offset + TRION_PLL_USER_CTL, &val);

	val >>= pll->post_div_shift;
	val &= PLL_POST_DIV_MASK;

	for (i = 0; i < pll->num_post_div; i++) {
		if (pll->post_div_table[i].val == val) {
			cal_div = pll->post_div_table[i].div;
			break;
		}
	}

	return (parent_rate / cal_div);
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
	int i, val = 0, cal_div, ret;

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

	cal_div = DIV_ROUND_UP_ULL((u64)parent_rate, rate);
	for (i = 0; i < pll->num_post_div; i++) {
		if (pll->post_div_table[i].div == cal_div) {
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
EXPORT_SYMBOL(clk_trion_pll_postdiv_ops);
