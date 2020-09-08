// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015, 2018-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/clk-provider.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/sched/clock.h>

#include "clk-alpha-pll.h"
#include "common.h"
#include "clk-debug.h"

#define PLL_MODE(p)		((p)->offset + 0x0)
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
#define PLL_UPDATE_BYPASS	BIT(23)
#define PLL_ALPHA_EN		BIT(24)
#define PLL_OFFLINE_ACK		BIT(28)
#define ALPHA_PLL_ACK_LATCH	BIT(29)
#define PLL_ACTIVE_FLAG		BIT(30)
#define PLL_LOCK_DET		BIT(31)

#define PLL_L_VAL(p)		((p)->offset + (p)->regs[PLL_OFF_L_VAL])
#define PLL_CAL_L_VAL(p)	((p)->offset + (p)->regs[PLL_OFF_CAL_L_VAL])
#define PLL_ALPHA_VAL(p)	((p)->offset + (p)->regs[PLL_OFF_ALPHA_VAL])
#define PLL_ALPHA_VAL_U(p)	((p)->offset + (p)->regs[PLL_OFF_ALPHA_VAL_U])

#define PLL_USER_CTL(p)		((p)->offset + (p)->regs[PLL_OFF_USER_CTL])
#define PLL_POST_DIV_SHIFT	8
#define PLL_POST_DIV_MASK(p)	GENMASK((p)->width, 0)
#define PLL_ALPHA_EN		BIT(24)
#define PLL_ALPHA_MODE		BIT(25)
#define PLL_VCO_SHIFT		20
#define PLL_VCO_MASK		0x3

#define PLL_USER_CTL_U(p)	((p)->offset + (p)->regs[PLL_OFF_USER_CTL_U])
#define PLL_USER_CTL_U1(p)	((p)->offset + (p)->regs[PLL_OFF_USER_CTL_U1])

#define PLL_CONFIG_CTL(p)	((p)->offset + (p)->regs[PLL_OFF_CONFIG_CTL])
#define PLL_CONFIG_CTL_U(p)	((p)->offset + (p)->regs[PLL_OFF_CONFIG_CTL_U])
#define PLL_CONFIG_CTL_U1(p)	((p)->offset + (p)->regs[PLL_OFF_CONFIG_CTL_U1])
#define PLL_TEST_CTL(p)		((p)->offset + (p)->regs[PLL_OFF_TEST_CTL])
#define PLL_TEST_CTL_U(p)	((p)->offset + (p)->regs[PLL_OFF_TEST_CTL_U])
#define PLL_TEST_CTL_U1(p)	((p)->offset + (p)->regs[PLL_OFF_TEST_CTL_U1])
#define PLL_STATUS(p)		((p)->offset + (p)->regs[PLL_OFF_STATUS])
#define PLL_OPMODE(p)		((p)->offset + (p)->regs[PLL_OFF_OPMODE])
#define PLL_FRAC(p)		((p)->offset + (p)->regs[PLL_OFF_FRAC])

const u8 clk_alpha_pll_regs[][PLL_OFF_MAX_REGS] = {
	[CLK_ALPHA_PLL_TYPE_DEFAULT] =  {
		[PLL_OFF_L_VAL] = 0x04,
		[PLL_OFF_ALPHA_VAL] = 0x08,
		[PLL_OFF_ALPHA_VAL_U] = 0x0c,
		[PLL_OFF_USER_CTL] = 0x10,
		[PLL_OFF_USER_CTL_U] = 0x14,
		[PLL_OFF_CONFIG_CTL] = 0x18,
		[PLL_OFF_TEST_CTL] = 0x1c,
		[PLL_OFF_TEST_CTL_U] = 0x20,
		[PLL_OFF_STATUS] = 0x24,
	},
	[CLK_ALPHA_PLL_TYPE_HUAYRA] =  {
		[PLL_OFF_L_VAL] = 0x04,
		[PLL_OFF_ALPHA_VAL] = 0x08,
		[PLL_OFF_USER_CTL] = 0x10,
		[PLL_OFF_CONFIG_CTL] = 0x14,
		[PLL_OFF_CONFIG_CTL_U] = 0x18,
		[PLL_OFF_TEST_CTL] = 0x1c,
		[PLL_OFF_TEST_CTL_U] = 0x20,
		[PLL_OFF_STATUS] = 0x24,
	},
	[CLK_ALPHA_PLL_TYPE_BRAMMO] =  {
		[PLL_OFF_L_VAL] = 0x04,
		[PLL_OFF_ALPHA_VAL] = 0x08,
		[PLL_OFF_ALPHA_VAL_U] = 0x0c,
		[PLL_OFF_USER_CTL] = 0x10,
		[PLL_OFF_USER_CTL_U] = 0x14,
		[PLL_OFF_CONFIG_CTL] = 0x18,
		[PLL_OFF_TEST_CTL] = 0x1c,
		[PLL_OFF_TEST_CTL_U] = 0x20,
		[PLL_OFF_STATUS] = 0x24,
	},
	[CLK_ALPHA_PLL_TYPE_FABIA] =  {
		[PLL_OFF_L_VAL] = 0x04,
		[PLL_OFF_CAL_L_VAL] = 0x08,
		[PLL_OFF_USER_CTL] = 0x0c,
		[PLL_OFF_USER_CTL_U] = 0x10,
		[PLL_OFF_CONFIG_CTL] = 0x14,
		[PLL_OFF_CONFIG_CTL_U] = 0x18,
		[PLL_OFF_TEST_CTL] = 0x1c,
		[PLL_OFF_TEST_CTL_U] = 0x20,
		[PLL_OFF_STATUS] = 0x24,
		[PLL_OFF_OPMODE] = 0x2c,
		[PLL_OFF_FRAC] = 0x38,
	},
	[CLK_ALPHA_PLL_TYPE_LUCID] =  {
		[PLL_OFF_L_VAL] = 0x04,
		[PLL_OFF_CAL_L_VAL] = 0x08,
		[PLL_OFF_USER_CTL] = 0x0c,
		[PLL_OFF_USER_CTL_U] = 0x10,
		[PLL_OFF_USER_CTL_U1] = 0x14,
		[PLL_OFF_CONFIG_CTL] = 0x18,
		[PLL_OFF_CONFIG_CTL_U] = 0x1c,
		[PLL_OFF_CONFIG_CTL_U1] = 0x20,
		[PLL_OFF_TEST_CTL] = 0x24,
		[PLL_OFF_TEST_CTL_U] = 0x28,
		[PLL_OFF_TEST_CTL_U1] = 0x2c,
		[PLL_OFF_STATUS] = 0x30,
		[PLL_OFF_OPMODE] = 0x38,
		[PLL_OFF_ALPHA_VAL] = 0x40,
	},
	[CLK_ALPHA_PLL_TYPE_ZONDA] =  {
		[PLL_OFF_L_VAL] = 0x04,
		[PLL_OFF_ALPHA_VAL] = 0x08,
		[PLL_OFF_USER_CTL] = 0x0c,
		[PLL_OFF_CONFIG_CTL] = 0x10,
		[PLL_OFF_CONFIG_CTL_U] = 0x14,
		[PLL_OFF_CONFIG_CTL_U1] = 0x18,
		[PLL_OFF_TEST_CTL] = 0x1c,
		[PLL_OFF_TEST_CTL_U] = 0x20,
		[PLL_OFF_TEST_CTL_U1] = 0x24,
		[PLL_OFF_OPMODE] = 0x28,
		[PLL_OFF_STATUS] = 0x38,
	},
	[CLK_ALPHA_PLL_TYPE_AGERA] =  {
		[PLL_OFF_L_VAL] = 0x04,
		[PLL_OFF_ALPHA_VAL] = 0x08,
		[PLL_OFF_USER_CTL] = 0x0c,
		[PLL_OFF_CONFIG_CTL] = 0x10,
		[PLL_OFF_CONFIG_CTL_U] = 0x14,
		[PLL_OFF_TEST_CTL] = 0x18,
		[PLL_OFF_TEST_CTL_U] = 0x1c,
		[PLL_OFF_STATUS] = 0x2c,
	},
};
EXPORT_SYMBOL_GPL(clk_alpha_pll_regs);

/*
 * Even though 40 bits are present, use only 32 for ease of calculation.
 */
#define ALPHA_REG_BITWIDTH	40
#define ALPHA_REG_16BIT_WIDTH	16
#define ALPHA_BITWIDTH		32U
#define ALPHA_SHIFT(w)		min(w, ALPHA_BITWIDTH)

#define PLL_HUAYRA_M_WIDTH		8
#define PLL_HUAYRA_M_SHIFT		8
#define PLL_HUAYRA_M_MASK		0xff
#define PLL_HUAYRA_N_SHIFT		0
#define PLL_HUAYRA_N_MASK		0xff
#define PLL_HUAYRA_ALPHA_WIDTH		16

#define PLL_OPMODE_STANDBY	0x0
#define PLL_OPMODE_RUN		0x1

#define PLL_OUT_MASK	0x7
#define PLL_OUT_RATE_MARGIN	500

/* LUCID PLL specific settings and offsets */
#define LUCID_PLL_CAL_VAL	0x44
#define LUCID_PCAL_DONE		BIT(27)

/* ZONDA PLL specific offsets */
#define ZONDA_PLL_OUT_MASK	0xF
#define ZONDA_STAY_IN_CFA	BIT(16)
#define ZONDA_PLL_FREQ_LOCK_DET	BIT(29)

/* FABIA PLL specific settings */
#define FABIA_PLL_CAL_VAL	0x3F

#define pll_alpha_width(p)					\
		((PLL_ALPHA_VAL_U(p) - PLL_ALPHA_VAL(p) == 4) ?	\
				 ALPHA_REG_BITWIDTH : ALPHA_REG_16BIT_WIDTH)

#define pll_has_64bit_config(p)	((PLL_CONFIG_CTL_U(p) - PLL_CONFIG_CTL(p)) == 4)

#define to_clk_alpha_pll(_hw) container_of(to_clk_regmap(_hw), \
					   struct clk_alpha_pll, clkr)

#define to_clk_alpha_pll_postdiv(_hw) container_of(to_clk_regmap(_hw), \
					   struct clk_alpha_pll_postdiv, clkr)

static int wait_for_pll(struct clk_alpha_pll *pll, u32 mask, bool inverse,
			const char *action)
{
	u32 val;
	int count;
	int ret;
	u64 time;
	struct clk_hw *hw = &pll->clkr.hw;
	const char *name = clk_hw_get_name(hw);

	ret = regmap_read(pll->clkr.regmap, PLL_MODE(pll), &val);
	if (ret)
		return ret;

	time = sched_clock();

	for (count = 100; count > 0; count--) {
		ret = regmap_read(pll->clkr.regmap, PLL_MODE(pll), &val);
		if (ret)
			return ret;
		if (inverse && !(val & mask))
			return 0;
		else if ((val & mask) == mask)
			return 0;

		udelay(1);
	}
	time = sched_clock() - time;

	pr_err("PLL lock bit detection total wait time: %lld ns\n", time);

	WARN_CLK(hw->core, name, 1, "failed to %s!\n", action);
	return -ETIMEDOUT;
}

#define wait_for_pll_enable_active(pll) \
	wait_for_pll(pll, PLL_ACTIVE_FLAG, 0, "enable")

#define wait_for_pll_enable_lock(pll) \
	wait_for_pll(pll, PLL_LOCK_DET, 0, "enable")

#define wait_for_zonda_pll_freq_lock(pll) \
	wait_for_pll(pll, ZONDA_PLL_FREQ_LOCK_DET, 0, "freq enable")

#define wait_for_pll_disable(pll) \
	wait_for_pll(pll, PLL_ACTIVE_FLAG, 1, "disable")

#define wait_for_pll_offline(pll) \
	wait_for_pll(pll, PLL_OFFLINE_ACK, 0, "offline")

#define wait_for_pll_update(pll) \
	wait_for_pll(pll, PLL_UPDATE, 1, "update")

#define wait_for_pll_update_ack_set(pll) \
	wait_for_pll(pll, ALPHA_PLL_ACK_LATCH, 0, "update_ack_set")

#define wait_for_pll_update_ack_clear(pll) \
	wait_for_pll(pll, ALPHA_PLL_ACK_LATCH, 1, "update_ack_clear")

#define wait_for_pll_latch_ack(pll) \
	wait_for_pll(pll, ALPHA_PLL_ACK_LATCH, 0, "latch ack")

void clk_alpha_pll_configure(struct clk_alpha_pll *pll, struct regmap *regmap,
			     const struct alpha_pll_config *config)
{
	u32 val, mask;

	if (config->l)
		regmap_write(regmap, PLL_L_VAL(pll), config->l);
	if (config->alpha)
		regmap_write(regmap, PLL_ALPHA_VAL(pll), config->alpha);
	if (config->config_ctl_val)
		regmap_write(regmap, PLL_CONFIG_CTL(pll),
				config->config_ctl_val);

	if (pll_has_64bit_config(pll))
		regmap_write(regmap, PLL_CONFIG_CTL_U(pll),
			     config->config_ctl_hi_val);

	if (pll_alpha_width(pll) > 32)
		regmap_write(regmap, PLL_ALPHA_VAL_U(pll), config->alpha_hi);

	if (config->main_output_mask || config->aux_output_mask ||
		config->aux2_output_mask || config->early_output_mask ||
		config->pre_div_val || config->vco_val ||
		config->alpha_en_mask) {
		val = config->main_output_mask;
		val |= config->aux_output_mask;
		val |= config->aux2_output_mask;
		val |= config->early_output_mask;
		val |= config->pre_div_val;
		val |= config->vco_val;
		val |= config->alpha_en_mask;

		mask = config->main_output_mask;
		mask |= config->aux_output_mask;
		mask |= config->aux2_output_mask;
		mask |= config->early_output_mask;
		mask |= config->pre_div_mask;
		mask |= config->vco_mask;
		mask |= config->alpha_en_mask;

		regmap_update_bits(regmap, PLL_USER_CTL(pll), mask, val);
	}

	if (config->post_div_mask) {
		mask = config->post_div_mask;
		val = config->post_div_val;
		regmap_update_bits(regmap, PLL_USER_CTL(pll), mask, val);
	}

	/* Do not bypass the latch interface */
	if (pll->flags & SUPPORTS_SLEW)
		regmap_update_bits(regmap, PLL_USER_CTL_U(pll),
		PLL_LATCH_INTERFACE, (u32)~PLL_LATCH_INTERFACE);

	if (pll->flags & SUPPORTS_DYNAMIC_UPDATE) {
		regmap_update_bits(regmap, PLL_MODE(pll),
				 PLL_UPDATE_BYPASS,
				 PLL_UPDATE_BYPASS);
	}

	if (config->test_ctl_mask) {
		mask = config->test_ctl_mask;
		val = config->test_ctl_val;
		regmap_update_bits(regmap, PLL_TEST_CTL(pll), mask, val);
	}

	if (config->test_ctl_hi_mask) {
		mask = config->test_ctl_hi_mask;
		val = config->test_ctl_hi_val;
		regmap_update_bits(regmap, PLL_TEST_CTL_U(pll), mask, val);
	}

	if (pll->flags & SUPPORTS_DYNAMIC_UPDATE)
		regmap_update_bits(regmap, PLL_MODE(pll), PLL_UPDATE_BYPASS,
					PLL_UPDATE_BYPASS);

	if (pll->flags & SUPPORTS_FSM_MODE)
		qcom_pll_set_fsm_mode(regmap, PLL_MODE(pll), 6, 0);
}

static int clk_alpha_pll_hwfsm_enable(struct clk_hw *hw)
{
	int ret;
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	u32 val;

	ret = regmap_read(pll->clkr.regmap, PLL_MODE(pll), &val);
	if (ret)
		return ret;

	val |= PLL_FSM_ENA;

	if (pll->flags & SUPPORTS_OFFLINE_REQ)
		val &= ~PLL_OFFLINE_REQ;

	ret = regmap_write(pll->clkr.regmap, PLL_MODE(pll), val);
	if (ret)
		return ret;

	/* Make sure enable request goes through before waiting for update */
	mb();

	return wait_for_pll_enable_active(pll);
}

static void clk_alpha_pll_hwfsm_disable(struct clk_hw *hw)
{
	int ret;
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	u32 val;

	ret = regmap_read(pll->clkr.regmap, PLL_MODE(pll), &val);
	if (ret)
		return;

	if (pll->flags & SUPPORTS_OFFLINE_REQ) {
		ret = regmap_update_bits(pll->clkr.regmap, PLL_MODE(pll),
					 PLL_OFFLINE_REQ, PLL_OFFLINE_REQ);
		if (ret)
			return;

		ret = wait_for_pll_offline(pll);
		if (ret)
			return;
	}

	/* Disable hwfsm */
	ret = regmap_update_bits(pll->clkr.regmap, PLL_MODE(pll),
				 PLL_FSM_ENA, 0);
	if (ret)
		return;

	wait_for_pll_disable(pll);
}

static int pll_is_enabled(struct clk_hw *hw, u32 mask)
{
	int ret;
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	u32 val;

	ret = regmap_read(pll->clkr.regmap, PLL_MODE(pll), &val);
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
	u32 val, mask;

	mask = PLL_OUTCTRL | PLL_RESET_N | PLL_BYPASSNL;
	ret = regmap_read(pll->clkr.regmap, PLL_MODE(pll), &val);
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

	ret = regmap_update_bits(pll->clkr.regmap, PLL_MODE(pll),
				 PLL_BYPASSNL, PLL_BYPASSNL);
	if (ret)
		return ret;

	/*
	 * H/W requires a 5us delay between disabling the bypass and
	 * de-asserting the reset.
	 */
	mb();
	udelay(5);

	ret = regmap_update_bits(pll->clkr.regmap, PLL_MODE(pll),
				 PLL_RESET_N, PLL_RESET_N);
	if (ret)
		return ret;

	ret = wait_for_pll_enable_lock(pll);
	if (ret)
		return ret;

	ret = regmap_update_bits(pll->clkr.regmap, PLL_MODE(pll),
				 PLL_OUTCTRL, PLL_OUTCTRL);

	/* Ensure that the write above goes through before returning. */
	mb();
	return ret;
}

static void clk_alpha_pll_disable(struct clk_hw *hw)
{
	int ret;
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	u32 val, mask;

	ret = regmap_read(pll->clkr.regmap, PLL_MODE(pll), &val);
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
	regmap_update_bits(pll->clkr.regmap, PLL_MODE(pll), mask, 0);

	/* Delay of 2 output clock ticks required until output is disabled */
	mb();
	udelay(1);

	mask = PLL_RESET_N | PLL_BYPASSNL;
	regmap_update_bits(pll->clkr.regmap, PLL_MODE(pll), mask, 0);
}

static unsigned long
alpha_pll_calc_rate(u64 prate, u32 l, u32 a, u32 alpha_width)
{
	return (prate * l) + ((prate * a) >> ALPHA_SHIFT(alpha_width));
}

static unsigned long
alpha_pll_round_rate(unsigned long rate, unsigned long prate, u32 *l, u64 *a,
		     u32 alpha_width)
{
	u64 remainder;
	u64 quotient;

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

	/* Upper ALPHA_BITWIDTH bits of Alpha */
	quotient = remainder << ALPHA_SHIFT(alpha_width);

	remainder = do_div(quotient, prate);

	if (remainder)
		quotient++;

	*a = quotient;
	return alpha_pll_calc_rate(prate, *l, *a, alpha_width);
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
	u32 alpha_width = pll_alpha_width(pll);

	regmap_read(pll->clkr.regmap, PLL_L_VAL(pll), &l);

	regmap_read(pll->clkr.regmap, PLL_USER_CTL(pll), &ctl);
	if (ctl & PLL_ALPHA_EN) {
		regmap_read(pll->clkr.regmap, PLL_ALPHA_VAL(pll), &low);
		if (alpha_width > 32) {
			regmap_read(pll->clkr.regmap, PLL_ALPHA_VAL_U(pll),
				    &high);
			a = (u64)high << 32 | low;
		} else {
			a = low & GENMASK(alpha_width - 1, 0);
		}

		if (alpha_width > ALPHA_BITWIDTH)
			a >>= alpha_width - ALPHA_BITWIDTH;
	}

	return alpha_pll_calc_rate(prate, l, a, alpha_width);
}

static int clk_alpha_pll_dynamic_update(struct clk_alpha_pll *pll)
{
	int ret;

	/* Latch the input to the PLL */
	regmap_update_bits(pll->clkr.regmap, PLL_MODE(pll),
				PLL_UPDATE, PLL_UPDATE);

	/* Wait for 2 reference cycle before checking ACK bit */
	udelay(1);

	ret = wait_for_pll_latch_ack(pll);
	if (ret)
		return ret;

	/* Return latch input to 0 */
	regmap_update_bits(pll->clkr.regmap, PLL_MODE(pll),
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

static int __clk_alpha_pll_update_latch(struct clk_alpha_pll *pll)
{
	int ret;
	u32 mode;

	regmap_read(pll->clkr.regmap, PLL_MODE(pll), &mode);

	/* Latch the input to the PLL */
	regmap_update_bits(pll->clkr.regmap, PLL_MODE(pll), PLL_UPDATE,
			   PLL_UPDATE);

	/* Wait for 2 reference cycle before checking ACK bit */
	udelay(1);

	/*
	 * PLL will latch the new L, Alpha and freq control word.
	 * PLL will respond by raising PLL_ACK_LATCH output when new programming
	 * has been latched in and PLL is being updated. When
	 * UPDATE_LOGIC_BYPASS bit is not set, PLL_UPDATE will be cleared
	 * automatically by hardware when PLL_ACK_LATCH is asserted by PLL.
	 */
	if (mode & PLL_UPDATE_BYPASS) {
		ret = wait_for_pll_update_ack_set(pll);
		if (ret)
			return ret;

		regmap_update_bits(pll->clkr.regmap, PLL_MODE(pll), PLL_UPDATE, 0);
	} else {
		ret = wait_for_pll_update(pll);
		if (ret)
			return ret;
	}

	if (pll->flags & SUPPORTS_DYNAMIC_UPDATE)
		ret = wait_for_pll_enable_lock(pll);
	else
		ret = wait_for_pll_update_ack_clear(pll);
	if (ret)
		return ret;

	/* Wait for PLL output to stabilize */
	udelay(10);

	return 0;
}

static int clk_alpha_pll_update_latch(struct clk_alpha_pll *pll,
				      int (*is_enabled)(struct clk_hw *))
{
	if (!is_enabled(&pll->clkr.hw) ||
	    !(pll->flags & SUPPORTS_DYNAMIC_UPDATE))
		return 0;

	return __clk_alpha_pll_update_latch(pll);
}

static int __clk_alpha_pll_set_rate(struct clk_hw *hw, unsigned long rate,
				    unsigned long prate,
				    int (*is_enabled)(struct clk_hw *))
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	struct clk_alpha_pll_postdiv *pll_postdiv =
		to_clk_alpha_pll_postdiv(hw);
	const struct pll_vco *vco;
	const struct pll_vco_data *data;
	u32 l, alpha_width = pll_alpha_width(pll);
	u64 a;
	unsigned long rrate;

	rrate = alpha_pll_round_rate(rate, prate, &l, &a, alpha_width);
	if (rrate != rate) {
		pr_err("alpha_pll: Call clk_set_rate with rounded rates!\n");
		return -EINVAL;
	}

	vco = alpha_pll_find_vco(pll, rrate);
	if (pll->vco_table && !vco) {
		pr_err("alpha pll not in a valid vco range\n");
		return -EINVAL;
	}

	/*
	 * For PLLs that do not support dynamic programming (dynamic_update
	 * is not set), ensure PLL is off before changing rate. For
	 * optimization reasons, assume no downstream clock is actively
	 * using it.
	 */
	if (is_enabled(&pll->clkr.hw) &&
	    !(pll->flags & SUPPORTS_DYNAMIC_UPDATE))
		hw->init->ops->disable(hw);

	regmap_write(pll->clkr.regmap, PLL_L_VAL(pll), l);

	if (alpha_width > ALPHA_BITWIDTH)
		a <<= alpha_width - ALPHA_BITWIDTH;

	if (alpha_width > 32)
		regmap_write(pll->clkr.regmap, PLL_ALPHA_VAL_U(pll), a >> 32);

	regmap_write(pll->clkr.regmap, PLL_ALPHA_VAL(pll), a);

	if (vco) {
		regmap_update_bits(pll->clkr.regmap, PLL_USER_CTL(pll),
				   PLL_VCO_MASK << PLL_VCO_SHIFT,
				   vco->val << PLL_VCO_SHIFT);
	}

	data = find_vco_data(pll->vco_data, rate, pll->num_vco_data);
	if (data) {
		if (data->freq == rate)
			regmap_update_bits(pll->clkr.regmap, PLL_USER_CTL(pll),
				PLL_POST_DIV_MASK(pll_postdiv)
				<< PLL_POST_DIV_SHIFT,
				data->post_div_val << PLL_POST_DIV_SHIFT);
		else
			regmap_update_bits(pll->clkr.regmap, PLL_USER_CTL(pll),
					PLL_POST_DIV_MASK(pll_postdiv)
					<< PLL_POST_DIV_SHIFT,
					0x0 << PLL_VCO_SHIFT);
	}

	regmap_update_bits(pll->clkr.regmap, PLL_USER_CTL(pll),
			   PLL_ALPHA_EN, PLL_ALPHA_EN);

	if (is_enabled(&pll->clkr.hw) &&
		(pll->flags & SUPPORTS_DYNAMIC_UPDATE))
		clk_alpha_pll_dynamic_update(pll);

	if (is_enabled(&pll->clkr.hw) &&
	    !(pll->flags & SUPPORTS_DYNAMIC_UPDATE))
		hw->init->ops->enable(hw);

	return clk_alpha_pll_update_latch(pll, is_enabled);
}

static int clk_alpha_pll_set_rate(struct clk_hw *hw, unsigned long rate,
				  unsigned long prate)
{
	return __clk_alpha_pll_set_rate(hw, rate, prate,
					clk_alpha_pll_is_enabled);
}

static int clk_alpha_pll_hwfsm_set_rate(struct clk_hw *hw, unsigned long rate,
					unsigned long prate)
{
	return __clk_alpha_pll_set_rate(hw, rate, prate,
					clk_alpha_pll_hwfsm_is_enabled);
}

static long clk_alpha_pll_round_rate(struct clk_hw *hw, unsigned long rate,
				     unsigned long *prate)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	u32 l, alpha_width = pll_alpha_width(pll);
	u64 a;
	unsigned long min_freq, max_freq;

	if (rate < pll->min_supported_freq)
		return pll->min_supported_freq;

	rate = alpha_pll_round_rate(rate, *prate, &l, &a, alpha_width);
	if (!pll->vco_table || alpha_pll_find_vco(pll, rate))
		return rate;

	min_freq = pll->vco_table[0].min_freq;
	max_freq = pll->vco_table[pll->num_vco - 1].max_freq;

	return clamp(rate, min_freq, max_freq);
}

void clk_huayra_pll_configure(struct clk_alpha_pll *pll, struct regmap *regmap,
				const struct alpha_pll_config *config)
{
	if (config->config_ctl_val)
		regmap_write(regmap, PLL_CONFIG_CTL(pll),
				config->config_ctl_val);

	if (config->config_ctl_hi_val)
		regmap_write(regmap, PLL_CONFIG_CTL_U(pll),
				config->config_ctl_hi_val);

	if (config->config_ctl_hi1_val)
		regmap_write(regmap, PLL_CONFIG_CTL_U1(pll),
				config->config_ctl_hi1_val);

	if (config->test_ctl_val)
		regmap_write(regmap, PLL_TEST_CTL(pll),
				config->test_ctl_val);

	if (config->test_ctl_hi_val)
		regmap_write(regmap, PLL_TEST_CTL_U(pll),
				config->test_ctl_hi_val);

	if (config->test_ctl_hi1_val)
		regmap_write(regmap, PLL_TEST_CTL_U1(pll),
				config->test_ctl_hi1_val);

	if (config->l)
		regmap_write(regmap, PLL_L_VAL(pll), config->l);

	if (config->alpha)
		regmap_write(regmap, PLL_ALPHA_VAL(pll), config->alpha);

	if (config->user_ctl_val)
		regmap_write(regmap, PLL_USER_CTL(pll),
				config->user_ctl_val);

	/* Set PLL_BYPASSNL */
	regmap_update_bits(regmap, PLL_MODE(pll),
			 PLL_BYPASSNL, PLL_BYPASSNL);

	/*
	 * H/W requires a 1us delay between disabling the bypass and
	 * de-asserting the reset.
	 */
	mb();
	udelay(5);

	/* Take PLL out from reset state */
	regmap_update_bits(regmap, PLL_MODE(pll),
				 PLL_RESET_N, PLL_RESET_N);

	/* Wait 50us for pll_lock_det bit to go high */
	mb();
	udelay(50);

	/* Enables PLL output */
	regmap_update_bits(regmap, PLL_MODE(pll),
			 PLL_OUTCTRL, PLL_OUTCTRL);
}

static unsigned long
alpha_huayra_pll_calc_rate(u64 prate, u32 l, u32 a)
{
	/*
	 * a contains 16 bit alpha_val in two’s compliment number in the range
	 * of [-0.5, 0.5).
	 */
	if (a >= BIT(PLL_HUAYRA_ALPHA_WIDTH - 1))
		l -= 1;

	return (prate * l) + (prate * a >> PLL_HUAYRA_ALPHA_WIDTH);
}

static unsigned long
alpha_huayra_pll_round_rate(unsigned long rate, unsigned long prate,
			    u32 *l, u32 *a)
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

	quotient = remainder << PLL_HUAYRA_ALPHA_WIDTH;
	remainder = do_div(quotient, prate);

	if (remainder)
		quotient++;

	/*
	 * alpha_val should be in two’s compliment number in the range
	 * of [-0.5, 0.5) so if quotient >= 0.5 then increment the l value
	 * since alpha value will be subtracted in this case.
	 */
	if (quotient >= BIT(PLL_HUAYRA_ALPHA_WIDTH - 1))
		*l += 1;

	*a = quotient;
	return alpha_huayra_pll_calc_rate(prate, *l, *a);
}

static unsigned long
alpha_pll_huayra_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	u64 rate = parent_rate, tmp;
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	u32 l, alpha = 0, ctl, alpha_m, alpha_n;

	regmap_read(pll->clkr.regmap, PLL_L_VAL(pll), &l);
	regmap_read(pll->clkr.regmap, PLL_USER_CTL(pll), &ctl);

	if (ctl & PLL_ALPHA_EN) {
		regmap_read(pll->clkr.regmap, PLL_ALPHA_VAL(pll), &alpha);
		/*
		 * Depending upon alpha_mode, it can be treated as M/N value or
		 * as a two’s compliment number. When alpha_mode=1,
		 * pll_alpha_val<15:8>=M and pll_apla_val<7:0>=N
		 *
		 *		Fout=FIN*(L+(M/N))
		 *
		 * M is a signed number (-128 to 127) and N is unsigned
		 * (0 to 255). M/N has to be within +/-0.5.
		 *
		 * When alpha_mode=0, it is a two’s compliment number in the
		 * range [-0.5, 0.5).
		 *
		 *		Fout=FIN*(L+(alpha_val)/2^16)
		 *
		 * where alpha_val is two’s compliment number.
		 */
		if (!(ctl & PLL_ALPHA_MODE))
			return alpha_huayra_pll_calc_rate(rate, l, alpha);

		alpha_m = alpha >> PLL_HUAYRA_M_SHIFT & PLL_HUAYRA_M_MASK;
		alpha_n = alpha >> PLL_HUAYRA_N_SHIFT & PLL_HUAYRA_N_MASK;

		rate *= l;
		tmp = parent_rate;
		if (alpha_m >= BIT(PLL_HUAYRA_M_WIDTH - 1)) {
			alpha_m = BIT(PLL_HUAYRA_M_WIDTH) - alpha_m;
			tmp *= alpha_m;
			do_div(tmp, alpha_n);
			rate -= tmp;
		} else {
			tmp *= alpha_m;
			do_div(tmp, alpha_n);
			rate += tmp;
		}

		return rate;
	}

	return alpha_huayra_pll_calc_rate(rate, l, alpha);
}

static int alpha_pll_huayra_set_rate(struct clk_hw *hw, unsigned long rate,
				     unsigned long prate)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	u32 l, a, ctl, cur_alpha = 0;

	rate = alpha_huayra_pll_round_rate(rate, prate, &l, &a);

	regmap_read(pll->clkr.regmap, PLL_USER_CTL(pll), &ctl);

	if (ctl & PLL_ALPHA_EN)
		regmap_read(pll->clkr.regmap, PLL_ALPHA_VAL(pll), &cur_alpha);

	/*
	 * Huayra PLL supports PLL dynamic programming. User can change L_VAL,
	 * without having to go through the power on sequence.
	 */
	if (clk_alpha_pll_is_enabled(hw)) {
		if (cur_alpha != a) {
			pr_err("clock needs to be gated %s\n",
			       clk_hw_get_name(hw));
			return -EBUSY;
		}

		regmap_write(pll->clkr.regmap, PLL_L_VAL(pll), l);
		/* Ensure that the write above goes to detect L val change. */
		mb();
		return wait_for_pll_enable_lock(pll);
	}

	regmap_write(pll->clkr.regmap, PLL_L_VAL(pll), l);
	regmap_write(pll->clkr.regmap, PLL_ALPHA_VAL(pll), a);

	if (a == 0)
		regmap_update_bits(pll->clkr.regmap, PLL_USER_CTL(pll),
				   PLL_ALPHA_EN, 0x0);
	else
		regmap_update_bits(pll->clkr.regmap, PLL_USER_CTL(pll),
				   PLL_ALPHA_EN | PLL_ALPHA_MODE, PLL_ALPHA_EN);

	return 0;
}

static int alpha_pll_huayra_determine_rate(struct clk_hw *hw,
				struct clk_rate_request *req)
{
	unsigned long rrate, prate;
	u32 l, a;
	struct clk_hw *parent_hw;

	parent_hw = clk_hw_get_parent(hw);
	if (!parent_hw)
		return -EINVAL;

	prate = clk_hw_get_rate(parent_hw);
	rrate = alpha_huayra_pll_round_rate(req->rate, prate, &l, &a);

	req->best_parent_hw = clk_hw_get_parent(hw);
	req->best_parent_rate = prate;
	req->rate = rrate;

	return 0;
}

static void clk_huayra_pll_list_registers(struct seq_file *f, struct clk_hw *hw)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	int size, i, val;

	static struct clk_register_data data[] = {
		{"PLL_MODE", PLL_OFF_MODE},
		{"PLL_L_VAL", PLL_OFF_L_VAL},
		{"PLL_ALPHA_VAL", PLL_OFF_ALPHA_VAL},
		{"PLL_USER_CTL", PLL_OFF_USER_CTL},
		{"PLL_CONFIG_CTL", PLL_OFF_CONFIG_CTL},
		{"PLL_CONFIG_CTL_U", PLL_OFF_CONFIG_CTL_U},
		{"PLL_CONFIG_CTL_U1", PLL_OFF_CONFIG_CTL_U1},
		{"PLL_TEST_CTL", PLL_OFF_TEST_CTL},
		{"PLL_TEST_CTL_U", PLL_OFF_TEST_CTL_U},
		{"PLL_TEST_CTL_U1", PLL_OFF_TEST_CTL_U1},
		{"PLL_OPMODE", PLL_OFF_OPMODE},
		{"PLL_STATUS", PLL_OFF_STATUS},
	};

	static struct clk_register_data data1[] = {
		{"APSS_PLL_VOTE", 0x0},
	};

	size = ARRAY_SIZE(data);

	for (i = 0; i < size; i++) {
		regmap_read(pll->clkr.regmap,
				pll->offset + pll->regs[data[i].offset], &val);
		clock_debug_output(f, false,
				"%20s: 0x%.8x\n", data[i].name, val);
	}

	regmap_read(pll->clkr.regmap, pll->offset +
					pll->regs[data[0].offset], &val);

	if (val & PLL_FSM_ENA) {
		regmap_read(pll->clkr.regmap, pll->clkr.enable_reg +
					data1[0].offset, &val);
		clock_debug_output(f, false,
				"%20s: 0x%.8x\n", data1[0].name, val);
	}
}

static void clk_alpha_pll_list_registers(struct seq_file *f, struct clk_hw *hw)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	int size, i, val;

	static struct clk_register_data data[] = {
		{"PLL_MODE", PLL_OFF_MODE},
		{"PLL_L_VAL", PLL_OFF_L_VAL},
		{"PLL_ALPHA_VAL", PLL_OFF_ALPHA_VAL},
		{"PLL_ALPHA_VAL_U", PLL_OFF_ALPHA_VAL_U},
		{"PLL_TEST_CTL", PLL_OFF_TEST_CTL},
		{"PLL_TEST_CTL_U", PLL_OFF_TEST_CTL_U},
		{"PLL_USER_CTL", PLL_OFF_USER_CTL},
		{"PLL_USER_CTL_U", PLL_OFF_USER_CTL_U},
		{"PLL_CONFIG_CTL", PLL_OFF_CONFIG_CTL},
		{"PLL_STATUS", PLL_OFF_STATUS},
	};

	static struct clk_register_data data1[] = {
		{"APSS_PLL_VOTE", 0x0},
	};

	size = ARRAY_SIZE(data);

	for (i = 0; i < size; i++) {
		regmap_read(pll->clkr.regmap,
				pll->offset + pll->regs[data[i].offset], &val);
		clock_debug_output(f, false,
				"%20s: 0x%.8x\n", data[i].name, val);
	}

	regmap_read(pll->clkr.regmap, pll->offset +
					pll->regs[data[0].offset], &val);

	if (val & PLL_FSM_ENA) {
		regmap_read(pll->clkr.regmap, pll->clkr.enable_reg +
					data1[0].offset, &val);
		clock_debug_output(f, false,
				"%20s: 0x%.8x\n", data1[0].name, val);
	}
}

static int clk_zonda_pll_is_enabled(struct clk_hw *hw)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	u32 mode_regval, opmode_regval;
	int ret;

	ret = regmap_read(pll->clkr.regmap, PLL_MODE(pll), &mode_regval);
	if (ret)
		return ret;
	ret = regmap_read(pll->clkr.regmap, PLL_OPMODE(pll), &opmode_regval);
	if (ret)
		return ret;

	return (opmode_regval & PLL_OPMODE_RUN) &&
		(mode_regval & PLL_OUTCTRL);
}

void clk_zonda_pll_configure(struct clk_alpha_pll *pll, struct regmap *regmap,
				const struct alpha_pll_config *config)
{
	if (config->l)
		regmap_write(regmap, PLL_L_VAL(pll), config->l);

	if (config->alpha)
		regmap_write(regmap, PLL_ALPHA_VAL(pll), config->alpha);

	if (config->config_ctl_val)
		regmap_write(regmap, PLL_CONFIG_CTL(pll),
				config->config_ctl_val);

	if (config->config_ctl_hi_val)
		regmap_write(regmap, PLL_CONFIG_CTL_U(pll),
				config->config_ctl_hi_val);

	if (config->config_ctl_hi1_val)
		regmap_write(regmap, PLL_CONFIG_CTL_U1(pll),
				config->config_ctl_hi1_val);

	if (config->user_ctl_val)
		regmap_write(regmap, PLL_USER_CTL(pll),
				config->user_ctl_val);

	if (config->user_ctl_hi_val)
		regmap_write(regmap, PLL_USER_CTL_U(pll),
				config->user_ctl_hi_val);

	if (config->user_ctl_hi1_val)
		regmap_write(regmap, PLL_USER_CTL_U1(pll),
				config->user_ctl_hi1_val);

	if (config->test_ctl_val)
		regmap_write(regmap, PLL_TEST_CTL(pll),
				config->test_ctl_val);

	if (config->test_ctl_hi_val)
		regmap_write(regmap, PLL_TEST_CTL_U(pll),
				config->test_ctl_hi_val);

	if (config->test_ctl_hi1_val)
		regmap_write(regmap, PLL_TEST_CTL_U1(pll),
				config->test_ctl_hi1_val);

	regmap_update_bits(regmap, PLL_MODE(pll),
			 PLL_BYPASSNL, 0);

	/* Disable PLL output */
	regmap_update_bits(regmap, PLL_MODE(pll),
			 PLL_OUTCTRL, 0);

	/* Set operation mode to OFF */
	regmap_write(regmap, PLL_OPMODE(pll), PLL_OPMODE_STANDBY);

	/* PLL should be in OFF mode before continuing */
	wmb();

	/* Place the PLL in STANDBY mode */
	regmap_update_bits(regmap, PLL_MODE(pll),
				 PLL_RESET_N, PLL_RESET_N);
}

static int clk_zonda_pll_enable(struct clk_hw *hw)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	u32 val, test_ctl_val;
	int ret;

	ret = regmap_read(pll->clkr.regmap, PLL_MODE(pll), &val);
	if (ret)
		return ret;

	/* If in FSM mode, just vote for it */
	if (val & PLL_VOTE_FSM_ENA) {
		ret = clk_enable_regmap(hw);
		if (ret)
			return ret;
		return wait_for_pll_enable_active(pll);
	}

	/* Get the PLL out of bypass mode */
	ret = regmap_update_bits(pll->clkr.regmap, PLL_MODE(pll),
						PLL_BYPASSNL, PLL_BYPASSNL);
	if (ret)
		return ret;

	/*
	 * H/W requires a 1us delay between disabling the bypass and
	 * de-asserting the reset.
	 */
	mb();
	udelay(1);

	ret = regmap_update_bits(pll->clkr.regmap, PLL_MODE(pll),
						 PLL_RESET_N, PLL_RESET_N);
	if (ret)
		return ret;

	/* Set operation mode to RUN */
	regmap_write(pll->clkr.regmap, PLL_OPMODE(pll),
						PLL_OPMODE_RUN);

	ret = regmap_read(pll->clkr.regmap, PLL_TEST_CTL(pll), &test_ctl_val);
	if (ret)
		return ret;

	/* If cfa mode then poll for freq lock */
	if (test_ctl_val & ZONDA_STAY_IN_CFA)
		ret = wait_for_zonda_pll_freq_lock(pll);
	else
		ret = wait_for_pll_enable_lock(pll);
	if (ret)
		return ret;

	/* Enable the PLL outputs */
	ret = regmap_update_bits(pll->clkr.regmap, PLL_USER_CTL(pll),
				ZONDA_PLL_OUT_MASK, ZONDA_PLL_OUT_MASK);
	if (ret)
		return ret;

	/* Enable the global PLL outputs */
	ret = regmap_update_bits(pll->clkr.regmap, PLL_MODE(pll),
				 PLL_OUTCTRL, PLL_OUTCTRL);
	if (ret)
		return ret;

	/* Ensure that the write above goes through before returning. */
	mb();

	return 0;
}

static void clk_zonda_pll_disable(struct clk_hw *hw)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	u32 val, mask, off = pll->offset;
	int ret;

	ret = regmap_read(pll->clkr.regmap, off + PLL_MODE(pll), &val);
	if (ret)
		return;

	/* If in FSM mode, just unvote it */
	if (val & PLL_VOTE_FSM_ENA) {
		clk_disable_regmap(hw);
		return;
	}

	/* Disable the global PLL output */
	ret = regmap_update_bits(pll->clkr.regmap, PLL_MODE(pll),
							PLL_OUTCTRL, 0);
	if (ret)
		return;

	/* Disable the PLL outputs */
	ret = regmap_update_bits(pll->clkr.regmap, PLL_USER_CTL(pll),
					ZONDA_PLL_OUT_MASK, 0);

	/* Put the PLL in bypass and reset */
	mask = PLL_RESET_N | PLL_BYPASSNL;
	ret = regmap_update_bits(pll->clkr.regmap, PLL_MODE(pll), mask, 0);
	if (ret)
		return;

	/* Place the PLL mode in OFF state */
	regmap_write(pll->clkr.regmap, PLL_OPMODE(pll),
			0x0);
}

static int clk_zonda_pll_set_rate(struct clk_hw *hw, unsigned long rate,
				  unsigned long prate)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	unsigned long rrate;
	u32 test_ctl_val;
	u32 l;
	u64 a;
	int ret;

	rrate = alpha_pll_round_rate(rate, prate, &l, &a, ALPHA_BITWIDTH);
	/*
	 * Due to a limited number of bits for fractional rate programming, the
	 * rounded up rate could be marginally higher than the requested rate.
	 */
	if (rrate > (rate + PLL_OUT_RATE_MARGIN) || rrate < rate) {
		pr_err("Requested rate (%lu) not matching the PLL's supported frequency (%lu)\n",
				rate, rrate);
		return -EINVAL;
	}

	regmap_write(pll->clkr.regmap, PLL_ALPHA_VAL(pll), a);
	regmap_write(pll->clkr.regmap, PLL_L_VAL(pll), l);

	/* Wait before polling for the frequency latch */
	udelay(5);

	/* Read stay in cfa mode */
	ret = regmap_read(pll->clkr.regmap, PLL_TEST_CTL(pll), &test_ctl_val);
	if (ret)
		return ret;

	/* If cfa mode then poll for freq lock */
	if (test_ctl_val & ZONDA_STAY_IN_CFA)
		ret = wait_for_zonda_pll_freq_lock(pll);
	else
		ret = wait_for_pll_enable_lock(pll);
	if (ret)
		return ret;

	/* Wait for PLL output to stabilize */
	udelay(100);
	return 0;
}

static unsigned long
clk_zonda_pll_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	u32 l, frac;

	regmap_read(pll->clkr.regmap, PLL_L_VAL(pll), &l);
	regmap_read(pll->clkr.regmap, PLL_ALPHA_VAL(pll), &frac);

	return alpha_pll_calc_rate(parent_rate, l, frac, ALPHA_BITWIDTH);
}

static void clk_zonda_pll_list_registers(struct seq_file *f, struct clk_hw *hw)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	int size, i, val;

	static struct clk_register_data pll_regs[] = {
		{"PLL_MODE", PLL_OFF_MODE},
		{"PLL_L_VAL", PLL_OFF_L_VAL},
		{"PLL_ALPHA_VAL", PLL_OFF_ALPHA_VAL},
		{"PLL_USER_CTL", PLL_OFF_USER_CTL},
		{"PLL_CONFIG_CTL", PLL_OFF_CONFIG_CTL},
		{"PLL_CONFIG_CTL_U", PLL_OFF_CONFIG_CTL_U},
		{"PLL_CONFIG_CTL_U1", PLL_OFF_CONFIG_CTL_U1},
		{"PLL_TEST_CTL", PLL_OFF_TEST_CTL},
		{"PLL_TEST_CTL_U", PLL_OFF_TEST_CTL_U},
		{"PLL_TEST_CTL_U1", PLL_OFF_TEST_CTL_U1},
		{"PLL_OPMODE", PLL_OFF_OPMODE},
		{"PLL_STATUS", PLL_OFF_STATUS},
	};

	static struct clk_register_data pll_vote_reg = {
		"APSS_PLL_VOTE", 0x0
	};

	size = ARRAY_SIZE(pll_regs);

	for (i = 0; i < size; i++) {
		regmap_read(pll->clkr.regmap, pll->offset +
					pll->regs[pll_regs[i].offset], &val);
		clock_debug_output(f, false,
				"%20s: 0x%.8x\n", pll_regs[i].name, val);
	}

	regmap_read(pll->clkr.regmap, pll->offset +
					pll->regs[pll_regs[0].offset], &val);

	if (val & PLL_FSM_ENA) {
		regmap_read(pll->clkr.regmap, pll->clkr.enable_reg +
					pll_vote_reg.offset, &val);
		clock_debug_output(f, false,
				"%20s: 0x%.8x\n", pll_vote_reg.name, val);
	}
}

const struct clk_ops clk_alpha_pll_ops = {
	.enable = clk_alpha_pll_enable,
	.disable = clk_alpha_pll_disable,
	.is_enabled = clk_alpha_pll_is_enabled,
	.recalc_rate = clk_alpha_pll_recalc_rate,
	.round_rate = clk_alpha_pll_round_rate,
	.set_rate = clk_alpha_pll_set_rate,
	.list_registers = clk_alpha_pll_list_registers,
	.bus_vote = clk_debug_bus_vote,
};
EXPORT_SYMBOL_GPL(clk_alpha_pll_ops);

const struct clk_ops clk_alpha_pll_huayra_ops = {
	.enable = clk_alpha_pll_enable,
	.disable = clk_alpha_pll_disable,
	.is_enabled = clk_alpha_pll_is_enabled,
	.recalc_rate = alpha_pll_huayra_recalc_rate,
	.determine_rate = alpha_pll_huayra_determine_rate,
	.set_rate = alpha_pll_huayra_set_rate,
	.list_registers = clk_huayra_pll_list_registers,
	.bus_vote = clk_debug_bus_vote,
};
EXPORT_SYMBOL_GPL(clk_alpha_pll_huayra_ops);

const struct clk_ops clk_alpha_pll_hwfsm_ops = {
	.enable = clk_alpha_pll_hwfsm_enable,
	.disable = clk_alpha_pll_hwfsm_disable,
	.is_enabled = clk_alpha_pll_hwfsm_is_enabled,
	.recalc_rate = clk_alpha_pll_recalc_rate,
	.round_rate = clk_alpha_pll_round_rate,
	.set_rate = clk_alpha_pll_hwfsm_set_rate,
	.list_registers = clk_alpha_pll_list_registers,
	.bus_vote = clk_debug_bus_vote,
};
EXPORT_SYMBOL_GPL(clk_alpha_pll_hwfsm_ops);

const struct clk_ops clk_alpha_pll_zonda_ops = {
	.enable = clk_zonda_pll_enable,
	.disable = clk_zonda_pll_disable,
	.is_enabled = clk_zonda_pll_is_enabled,
	.recalc_rate = clk_zonda_pll_recalc_rate,
	.round_rate = clk_alpha_pll_round_rate,
	.set_rate = clk_zonda_pll_set_rate,
	.list_registers = clk_zonda_pll_list_registers,
	.bus_vote = clk_debug_bus_vote,
};
EXPORT_SYMBOL(clk_alpha_pll_zonda_ops);

static unsigned long
clk_alpha_pll_postdiv_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	struct clk_alpha_pll_postdiv *pll = to_clk_alpha_pll_postdiv(hw);
	u32 ctl;

	regmap_read(pll->clkr.regmap, PLL_USER_CTL(pll), &ctl);

	ctl >>= PLL_POST_DIV_SHIFT;
	ctl &= PLL_POST_DIV_MASK(pll);

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

static const struct clk_div_table clk_alpha_2bit_div_table[] = {
	{ 0x0, 1 },
	{ 0x1, 2 },
	{ 0x3, 4 },
	{ }
};

static long
clk_alpha_pll_postdiv_round_rate(struct clk_hw *hw, unsigned long rate,
				 unsigned long *prate)
{
	struct clk_alpha_pll_postdiv *pll = to_clk_alpha_pll_postdiv(hw);
	const struct clk_div_table *table;

	if (pll->width == 2)
		table = clk_alpha_2bit_div_table;
	else
		table = clk_alpha_div_table;

	return divider_round_rate(hw, rate, prate, table,
				  pll->width, CLK_DIVIDER_POWER_OF_TWO);
}

static long
clk_alpha_pll_postdiv_round_ro_rate(struct clk_hw *hw, unsigned long rate,
				    unsigned long *prate)
{
	struct clk_alpha_pll_postdiv *pll = to_clk_alpha_pll_postdiv(hw);
	struct clk_hw *parent_hw;
	u32 ctl, div;

	regmap_read(pll->clkr.regmap, PLL_USER_CTL(pll), &ctl);

	ctl >>= PLL_POST_DIV_SHIFT;
	ctl &= BIT(pll->width) - 1;
	div = 1 << fls(ctl);

	if (clk_hw_get_flags(hw) & CLK_SET_RATE_PARENT) {
		parent_hw = clk_hw_get_parent(hw);
		if (!parent_hw)
			return -EINVAL;

		*prate = clk_hw_round_rate(parent_hw, div * rate);
	}

	return DIV_ROUND_UP_ULL((u64)*prate, div);
}

static int clk_alpha_pll_postdiv_set_rate(struct clk_hw *hw, unsigned long rate,
					  unsigned long parent_rate)
{
	struct clk_alpha_pll_postdiv *pll = to_clk_alpha_pll_postdiv(hw);
	int div;

	/* 16 -> 0xf, 8 -> 0x7, 4 -> 0x3, 2 -> 0x1, 1 -> 0x0 */
	div = DIV_ROUND_UP_ULL((u64)parent_rate, rate) - 1;

	return regmap_update_bits(pll->clkr.regmap, PLL_USER_CTL(pll),
				  PLL_POST_DIV_MASK(pll) << PLL_POST_DIV_SHIFT,
				  div << PLL_POST_DIV_SHIFT);
}

const struct clk_ops clk_alpha_pll_postdiv_ops = {
	.recalc_rate = clk_alpha_pll_postdiv_recalc_rate,
	.round_rate = clk_alpha_pll_postdiv_round_rate,
	.set_rate = clk_alpha_pll_postdiv_set_rate,
	.bus_vote = clk_debug_bus_vote,
};
EXPORT_SYMBOL_GPL(clk_alpha_pll_postdiv_ops);

const struct clk_ops clk_alpha_pll_postdiv_ro_ops = {
	.round_rate = clk_alpha_pll_postdiv_round_ro_rate,
	.recalc_rate = clk_alpha_pll_postdiv_recalc_rate,
	.bus_vote = clk_debug_bus_vote,
};
EXPORT_SYMBOL_GPL(clk_alpha_pll_postdiv_ro_ops);

static void clk_alpha_pll_custom_configure(struct clk_alpha_pll *pll,
		struct regmap *regmap, const struct alpha_pll_config *config)
{
	int i;

	for (i = 0; i < config->num_custom_reg; i++)
		regmap_write(regmap, pll->offset + config->custom_reg_offset[i],
				config->custom_reg_val[i]);
}

int clk_fabia_pll_configure(struct clk_alpha_pll *pll, struct regmap *regmap,
			     const struct alpha_pll_config *config)
{
	u32 val, mask;

	if (!config) {
		pr_err("PLL configuration missing.\n");
		return -EINVAL;
	}

	if (config->l)
		regmap_write(regmap, PLL_L_VAL(pll), config->l);

	if (config->cal_l)
		regmap_write(regmap, PLL_CAL_L_VAL(pll), config->cal_l);
	else
		regmap_write(regmap, PLL_CAL_L_VAL(pll), FABIA_PLL_CAL_VAL);

	if (config->alpha)
		regmap_write(regmap, PLL_FRAC(pll), config->alpha);

	if (config->config_ctl_val)
		regmap_write(regmap, PLL_CONFIG_CTL(pll),
						config->config_ctl_val);

	if (config->config_ctl_hi_val)
		regmap_write(regmap, PLL_CONFIG_CTL_U(pll),
					config->config_ctl_hi_val);

	if (config->user_ctl_val)
		regmap_write(regmap, PLL_USER_CTL(pll),
					config->user_ctl_val);

	if (config->user_ctl_hi_val)
		regmap_write(regmap, PLL_USER_CTL_U(pll),
					config->user_ctl_hi_val);

	if (config->test_ctl_val)
		regmap_write(regmap, PLL_TEST_CTL(pll),
					config->test_ctl_val);

	if (config->test_ctl_hi_val)
		regmap_write(regmap, PLL_TEST_CTL_U(pll),
					config->test_ctl_hi_val);

	if (config->post_div_mask) {
		mask = config->post_div_mask;
		val = config->post_div_val;
		regmap_update_bits(regmap, PLL_USER_CTL(pll), mask, val);
	}

	clk_alpha_pll_custom_configure(pll, regmap, config);

	regmap_update_bits(regmap, PLL_MODE(pll), PLL_UPDATE_BYPASS,
							PLL_UPDATE_BYPASS);

	regmap_update_bits(regmap, PLL_MODE(pll), PLL_RESET_N, PLL_RESET_N);
	return 0;
}
EXPORT_SYMBOL_GPL(clk_fabia_pll_configure);

static int alpha_pll_fabia_enable(struct clk_hw *hw)
{
	int ret;
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	u32 val, opmode_val, l_val, cal_val;
	struct regmap *regmap = pll->clkr.regmap;

	ret = regmap_read(regmap, PLL_MODE(pll), &val);
	if (ret)
		return ret;

	/* If in FSM mode, just vote for it */
	if (val & PLL_VOTE_FSM_ENA) {
		ret = clk_enable_regmap(hw);
		if (ret)
			return ret;
		return wait_for_pll_enable_active(pll);
	}

	ret = regmap_read(regmap, PLL_OPMODE(pll), &opmode_val);
	if (ret)
		return ret;

	/* Skip If PLL is already running */
	if ((opmode_val & PLL_OPMODE_RUN) && (val & PLL_OUTCTRL))
		return 0;

	ret = regmap_read(regmap, PLL_L_VAL(pll), &l_val);
	if (ret)
		return ret;

	ret = regmap_read(regmap, PLL_CAL_L_VAL(pll), &cal_val);
	if (ret)
		return ret;

	/* PLL has lost it's L or CAL value, needs reconfiguration */
	if (!l_val || !cal_val) {
		ret = clk_fabia_pll_configure(pll, regmap, pll->config);
		if (ret) {
			pr_err("Failed to configure %s\n", clk_hw_get_name(hw));
			return ret;
		}
		pr_warn("PLL configuration lost, reconfiguration of PLL done.\n");
	}

	ret = regmap_update_bits(regmap, PLL_MODE(pll), PLL_OUTCTRL, 0);
	if (ret)
		return ret;

	ret = regmap_write(regmap, PLL_OPMODE(pll), PLL_OPMODE_STANDBY);
	if (ret)
		return ret;

	ret = regmap_update_bits(regmap, PLL_MODE(pll), PLL_RESET_N,
				 PLL_RESET_N);
	if (ret)
		return ret;

	ret = regmap_write(regmap, PLL_OPMODE(pll), PLL_OPMODE_RUN);
	if (ret)
		return ret;

	ret = wait_for_pll_enable_lock(pll);
	if (ret)
		return ret;

	ret = regmap_update_bits(regmap, PLL_USER_CTL(pll),
				 PLL_OUT_MASK, PLL_OUT_MASK);
	if (ret)
		return ret;

	return regmap_update_bits(regmap, PLL_MODE(pll), PLL_OUTCTRL,
				 PLL_OUTCTRL);
}

static void alpha_pll_fabia_disable(struct clk_hw *hw)
{
	int ret;
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	u32 val;
	struct regmap *regmap = pll->clkr.regmap;

	ret = regmap_read(regmap, PLL_MODE(pll), &val);
	if (ret)
		return;

	/* If in FSM mode, just unvote it */
	if (val & PLL_FSM_ENA) {
		clk_disable_regmap(hw);
		return;
	}

	ret = regmap_update_bits(regmap, PLL_MODE(pll), PLL_OUTCTRL, 0);
	if (ret)
		return;

	/* Disable main outputs */
	ret = regmap_update_bits(regmap, PLL_USER_CTL(pll), PLL_OUT_MASK,
				 0);
	if (ret)
		return;

	/* Place the PLL in STANDBY */
	regmap_write(regmap, PLL_OPMODE(pll), PLL_OPMODE_STANDBY);
}

static unsigned long alpha_pll_fabia_recalc_rate(struct clk_hw *hw,
						unsigned long parent_rate)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	u32 l, frac, alpha_width = pll_alpha_width(pll);

	regmap_read(pll->clkr.regmap, PLL_L_VAL(pll), &l);
	regmap_read(pll->clkr.regmap, PLL_FRAC(pll), &frac);

	return alpha_pll_calc_rate(parent_rate, l, frac, alpha_width);
}

static int alpha_pll_fabia_set_rate(struct clk_hw *hw, unsigned long rate,
						unsigned long prate)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	u32 l, cal_val, alpha_width = pll_alpha_width(pll);
	u64 a;
	unsigned long rrate;
	int ret = 0;

	ret = regmap_read(pll->clkr.regmap, PLL_CAL_L_VAL(pll), &cal_val);
	if (ret)
		return ret;

	/* PLL has lost it's CAL value, needs reconfiguration */
	if (!cal_val) {
		ret = clk_fabia_pll_configure(pll, pll->clkr.regmap,
				pll->config);
		if (ret) {
			pr_err("Failed to configure %s\n", clk_hw_get_name(hw));
			return ret;
		}
		pr_warn("%s: PLL configuration lost, reconfiguration of PLL done.\n",
				clk_hw_get_name(hw));
	}

	rrate = alpha_pll_round_rate(rate, prate, &l, &a, alpha_width);

	/*
	 * Due to limited number of bits for fractional rate programming, the
	 * rounded up rate could be marginally higher than the requested rate.
	 */
	if (rrate > (rate + PLL_OUT_RATE_MARGIN) || rrate < rate) {
		pr_err("Call set rate on the PLL with rounded rates!\n");
		return -EINVAL;
	}

	regmap_write(pll->clkr.regmap, PLL_L_VAL(pll), l);
	regmap_write(pll->clkr.regmap, PLL_FRAC(pll), a);

	return __clk_alpha_pll_update_latch(pll);
}

/*
 * Fabia PLL requires power-on self calibration which happen when the PLL comes
 * out of reset. Calibration frequency is calculated by below relation:
 *
 * calibration freq = ((pll_l_valmax + pll_l_valmin) * 0.54)
 */
static int alpha_pll_fabia_prepare(struct clk_hw *hw)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	const struct pll_vco *vco;
	struct clk_hw *parent;
	unsigned long calibration_freq, freq_hz;
	u32 l, alpha_width = pll_alpha_width(pll), regval;
	u64 a;
	int ret;

	/* Check if calibration needs to be done i.e. PLL is in reset */
	ret = regmap_read(pll->clkr.regmap, PLL_MODE(pll), &regval);
	if (ret)
		return ret;

	/* Return early if calibration is not needed. */
	if (regval & PLL_RESET_N)
		return 0;

	vco = alpha_pll_find_vco(pll, clk_hw_get_rate(hw));
	if (!vco) {
		pr_err("alpha pll: not in a valid vco range\n");
		return -EINVAL;
	}

	calibration_freq = ((pll->vco_table[0].min_freq +
				pll->vco_table[0].max_freq) * 54)/100;

	parent = clk_hw_get_parent(hw);
	if (!parent)
		return -EINVAL;

	freq_hz = alpha_pll_round_rate(calibration_freq,
			clk_hw_get_rate(parent), &l, &a, alpha_width);
	/*
	 * Due to a limited number of bits for fractional rate programming, the
	 * rounded up rate could be marginally higher than the requested rate.
	 */
	if (freq_hz > (calibration_freq + PLL_OUT_RATE_MARGIN) ||
						freq_hz < calibration_freq)
		return -EINVAL;

	/* Setup PLL for calibration frequency */
	regmap_write(pll->clkr.regmap, PLL_CAL_L_VAL(pll), FABIA_PLL_CAL_VAL);

	/* Bringup the pll at calibration frequency */
	ret = alpha_pll_fabia_enable(hw);
	if (ret) {
		pr_err("alpha pll calibration failed\n");
		return ret;
	}

	alpha_pll_fabia_disable(hw);
	return 0;
}

static void clk_fabia_pll_list_registers(struct seq_file *f, struct clk_hw *hw)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	int size, i, val;

	static struct clk_register_data data[] = {
		{"PLL_MODE", PLL_OFF_MODE},
		{"PLL_L_VAL", PLL_OFF_L_VAL},
		{"PLL_CAL_L_VAL", PLL_OFF_CAL_L_VAL},
		{"PLL_USER_CTL", PLL_OFF_USER_CTL},
		{"PLL_USER_CTL_U", PLL_OFF_USER_CTL_U},
		{"PLL_CONFIG_CTL", PLL_OFF_CONFIG_CTL},
		{"PLL_CONFIG_CTL_U", PLL_OFF_CONFIG_CTL_U},
		{"PLL_TEST_CTL", PLL_OFF_TEST_CTL},
		{"PLL_TEST_CTL_U", PLL_OFF_TEST_CTL_U},
		{"PLL_STATUS", PLL_OFF_STATUS},
		{"PLL_OPMODE", PLL_OFF_MODE},
		{"PLL_FRAC", PLL_OFF_FRAC},
	};

	static struct clk_register_data data1[] = {
		{"APSS_PLL_VOTE", 0x0},
	};

	size = ARRAY_SIZE(data);

	for (i = 0; i < size; i++) {
		regmap_read(pll->clkr.regmap, pll->offset +
					pll->regs[data[i].offset], &val);
		clock_debug_output(f, false,
				"%20s: 0x%.8x\n", data[i].name, val);
	}

	regmap_read(pll->clkr.regmap, pll->offset + pll->regs[data[0].offset],
								&val);

	if (val & PLL_FSM_ENA) {
		regmap_read(pll->clkr.regmap, pll->clkr.enable_reg +
				data1[0].offset, &val);
		clock_debug_output(f, false,
				"%20s: 0x%.8x\n", data1[0].name, val);
	}
}

const struct clk_ops clk_alpha_pll_fabia_ops = {
	.prepare = alpha_pll_fabia_prepare,
	.enable = alpha_pll_fabia_enable,
	.disable = alpha_pll_fabia_disable,
	.is_enabled = clk_alpha_pll_is_enabled,
	.set_rate = alpha_pll_fabia_set_rate,
	.recalc_rate = alpha_pll_fabia_recalc_rate,
	.round_rate = clk_alpha_pll_round_rate,
	.list_registers = clk_fabia_pll_list_registers,
	.bus_vote = clk_debug_bus_vote,
};
EXPORT_SYMBOL_GPL(clk_alpha_pll_fabia_ops);

const struct clk_ops clk_alpha_pll_fixed_fabia_ops = {
	.enable = alpha_pll_fabia_enable,
	.disable = alpha_pll_fabia_disable,
	.is_enabled = clk_alpha_pll_is_enabled,
	.recalc_rate = alpha_pll_fabia_recalc_rate,
	.round_rate = clk_alpha_pll_round_rate,
	.list_registers = clk_fabia_pll_list_registers,
	.bus_vote = clk_debug_bus_vote,
};
EXPORT_SYMBOL_GPL(clk_alpha_pll_fixed_fabia_ops);

static unsigned long clk_alpha_pll_postdiv_fabia_recalc_rate(struct clk_hw *hw,
					unsigned long parent_rate)
{
	struct clk_alpha_pll_postdiv *pll = to_clk_alpha_pll_postdiv(hw);
	u32 i, div = 1, val;
	int ret;

	if (!pll->post_div_table) {
		pr_err("Missing the post_div_table for the PLL\n");
		return -EINVAL;
	}

	ret = regmap_read(pll->clkr.regmap, PLL_USER_CTL(pll), &val);
	if (ret)
		return ret;

	val >>= pll->post_div_shift;
	val &= BIT(pll->width) - 1;

	for (i = 0; i < pll->num_post_div; i++) {
		if (pll->post_div_table[i].val == val) {
			div = pll->post_div_table[i].div;
			break;
		}
	}

	return (parent_rate / div);
}

static long clk_alpha_pll_postdiv_fabia_round_rate(struct clk_hw *hw,
				unsigned long rate, unsigned long *prate)
{
	struct clk_alpha_pll_postdiv *pll = to_clk_alpha_pll_postdiv(hw);

	if (!pll->post_div_table) {
		pr_err("Missing the post_div_table for the PLL\n");
		return -EINVAL;
	}

	return divider_round_rate(hw, rate, prate, pll->post_div_table,
				pll->width, CLK_DIVIDER_ROUND_CLOSEST);
}

static int clk_alpha_pll_postdiv_fabia_set_rate(struct clk_hw *hw,
				unsigned long rate, unsigned long parent_rate)
{
	struct clk_alpha_pll_postdiv *pll = to_clk_alpha_pll_postdiv(hw);
	int i, val = 0, div, ret;

	/*
	 * If the PLL is in FSM mode, then treat set_rate callback as a
	 * no-operation.
	 */
	ret = regmap_read(pll->clkr.regmap, PLL_MODE(pll), &val);
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

	return regmap_update_bits(pll->clkr.regmap, PLL_USER_CTL(pll),
				(BIT(pll->width) - 1) << pll->post_div_shift,
				val << pll->post_div_shift);
}

const struct clk_ops clk_alpha_pll_postdiv_fabia_ops = {
	.recalc_rate = clk_alpha_pll_postdiv_fabia_recalc_rate,
	.round_rate = clk_alpha_pll_postdiv_fabia_round_rate,
	.set_rate = clk_alpha_pll_postdiv_fabia_set_rate,
	.bus_vote = clk_debug_bus_vote,
};
EXPORT_SYMBOL_GPL(clk_alpha_pll_postdiv_fabia_ops);

const struct clk_ops clk_alpha_pll_postdiv_zonda_ops = {
	.recalc_rate = clk_alpha_pll_postdiv_fabia_recalc_rate,
	.round_rate = clk_alpha_pll_postdiv_fabia_round_rate,
	.set_rate = clk_alpha_pll_postdiv_fabia_set_rate,
	.bus_vote = clk_debug_bus_vote,
};
EXPORT_SYMBOL(clk_alpha_pll_postdiv_zonda_ops);

static int lucid_pll_is_enabled(struct clk_alpha_pll *pll,
					struct regmap *regmap)
{
	u32 mode_regval, opmode_regval;
	int ret;

	ret = regmap_read(regmap, PLL_MODE(pll), &mode_regval);
	ret |= regmap_read(regmap, PLL_OPMODE(pll), &opmode_regval);
	if (ret)
		return 0;

	return ((opmode_regval & PLL_OPMODE_RUN) &&
		(mode_regval & PLL_OUTCTRL));
}

void clk_lucid_pll_configure(struct clk_alpha_pll *pll, struct regmap *regmap,
				const struct alpha_pll_config *config)
{
	if (lucid_pll_is_enabled(pll, regmap))
		return;

	if (config->l)
		regmap_write(regmap, PLL_L_VAL(pll), config->l);

	if (config->cal_l)
		regmap_write(regmap, PLL_CAL_L_VAL(pll), config->cal_l);
	else
		regmap_write(regmap, PLL_CAL_L_VAL(pll), LUCID_PLL_CAL_VAL);

	if (config->alpha)
		regmap_write(regmap, PLL_ALPHA_VAL(pll), config->alpha);
	if (config->config_ctl_val)
		regmap_write(regmap, PLL_CONFIG_CTL(pll),
				config->config_ctl_val);

	if (config->config_ctl_hi_val)
		regmap_write(regmap, PLL_CONFIG_CTL_U(pll),
				config->config_ctl_hi_val);

	if (config->config_ctl_hi1_val)
		regmap_write(regmap, PLL_CONFIG_CTL_U1(pll),
				config->config_ctl_hi1_val);

	if (config->user_ctl_val)
		regmap_write(regmap, PLL_USER_CTL(pll),
				config->user_ctl_val);

	if (config->user_ctl_hi_val)
		regmap_write(regmap, PLL_USER_CTL_U(pll),
				config->user_ctl_hi_val);

	if (config->user_ctl_hi1_val)
		regmap_write(regmap, PLL_USER_CTL_U1(pll),
				config->user_ctl_hi1_val);

	if (config->test_ctl_val)
		regmap_write(regmap, PLL_TEST_CTL(pll),
				config->test_ctl_val);

	if (config->test_ctl_hi_val)
		regmap_write(regmap, PLL_TEST_CTL_U(pll),
				config->test_ctl_hi_val);

	if (config->test_ctl_hi1_val)
		regmap_write(regmap, PLL_TEST_CTL_U1(pll),
				config->test_ctl_hi1_val);

	clk_alpha_pll_custom_configure(pll, regmap, config);

	regmap_update_bits(regmap, PLL_MODE(pll),
				 PLL_UPDATE_BYPASS,
				 PLL_UPDATE_BYPASS);

	/* Disable PLL output */
	regmap_update_bits(regmap, PLL_MODE(pll),
					PLL_OUTCTRL, 0);
	/* Set operation mode to OFF */
	regmap_write(regmap, PLL_OPMODE(pll), PLL_OPMODE_STANDBY);

	/* PLL should be in OFF mode before continuing */
	wmb();

	/* Place the PLL in STANDBY mode */
	regmap_update_bits(regmap, PLL_MODE(pll),
				 PLL_RESET_N, PLL_RESET_N);
}

static int alpha_pll_lucid_enable(struct clk_hw *hw)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	u32 val;
	int ret;

	ret = regmap_read(pll->clkr.regmap, PLL_MODE(pll), &val);
	if (ret)
		return ret;

	/* If in FSM mode, just vote for it */
	if (val & PLL_VOTE_FSM_ENA) {
		ret = clk_enable_regmap(hw);
		if (ret)
			return ret;
		return wait_for_pll_enable_active(pll);
	}

	/* Set operation mode to RUN */
	regmap_write(pll->clkr.regmap, PLL_OPMODE(pll), PLL_OPMODE_RUN);

	ret = wait_for_pll_enable_lock(pll);
	if (ret)
		return ret;

	/* Enable the PLL outputs */
	ret = regmap_update_bits(pll->clkr.regmap, PLL_USER_CTL(pll),
				 PLL_OUT_MASK, PLL_OUT_MASK);
	if (ret)
		return ret;

	/* Enable the global PLL outputs */
	ret = regmap_update_bits(pll->clkr.regmap, PLL_MODE(pll),
				 PLL_OUTCTRL, PLL_OUTCTRL);
	if (ret)
		return ret;

	/* Ensure that the write above goes through before returning. */
	mb();
	return ret;
}

static void alpha_pll_lucid_disable(struct clk_hw *hw)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	u32 val;
	int ret;

	ret = regmap_read(pll->clkr.regmap, PLL_MODE(pll), &val);
	if (ret)
		return;

	/* If in FSM mode, just unvote it */
	if (val & PLL_VOTE_FSM_ENA) {
		clk_disable_regmap(hw);
		return;
	}

	/* Disable the global PLL output */
	ret = regmap_update_bits(pll->clkr.regmap, PLL_MODE(pll),
							PLL_OUTCTRL, 0);
	if (ret)
		return;

	/* Disable the PLL outputs */
	ret = regmap_update_bits(pll->clkr.regmap, PLL_USER_CTL(pll),
			PLL_OUT_MASK, 0);
	if (ret)
		return;

	/* Place the PLL mode in STANDBY */
	regmap_write(pll->clkr.regmap, PLL_OPMODE(pll),
			PLL_OPMODE_STANDBY);

	regmap_update_bits(pll->clkr.regmap, PLL_MODE(pll), PLL_RESET_N,
			PLL_RESET_N);
}

/*
 * The Lucid PLL requires a power-on self-calibration which happens when the
 * PLL comes out of reset. The calibration is performed at an output frequency
 * of ~1300 MHz which means that SW will have to vote on a voltage that's
 * equal to or greater than SVS_L1 on the corresponding rail. Since this is not
 * feasable to do in the atomic enable path, temporarily bring up the PLL here,
 * let it calibrate, and place it in standby before returning.
 */
static int alpha_pll_lucid_prepare(struct clk_hw *hw)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	struct clk_hw *p;
	u32 regval;
	unsigned long prate;
	int ret;

	/* Return early if calibration is not needed. */
	regmap_read(pll->clkr.regmap, PLL_MODE(pll), &regval);
	if (regval & LUCID_PCAL_DONE)
		return 0;

	if (pll->config) {
		/*
		 * Reconfigure the PLL if CAL_L_VAL is 0 (which implies that all
		 * clock controller registers have been reset).
		 */
		regmap_read(pll->clkr.regmap, PLL_CAL_L_VAL(pll), &regval);
		if (!regval) {
			pr_debug("reconfiguring %s after it was reset\n",
				clk_hw_get_name(hw));
			clk_lucid_pll_configure(pll, pll->clkr.regmap,
						pll->config);
		}
	}

	p = clk_hw_get_parent(hw);
	if (!p)
		return -EINVAL;

	prate = clk_hw_get_rate(p);
	if (!prate)
		return -EINVAL;

	ret = clk_vote_rate_vdd(hw->core, LUCID_PLL_CAL_VAL * prate);
	if (ret)
		return ret;

	ret = alpha_pll_lucid_enable(hw);
	if (ret)
		goto ret_path;

	alpha_pll_lucid_disable(hw);
ret_path:
	clk_unvote_rate_vdd(hw->core, LUCID_PLL_CAL_VAL * prate);
	return 0;
}

static unsigned long
alpha_pll_lucid_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	u32 l, frac;

	regmap_read(pll->clkr.regmap, PLL_L_VAL(pll), &l);
	regmap_read(pll->clkr.regmap, PLL_ALPHA_VAL(pll), &frac);

	return alpha_pll_calc_rate(parent_rate, l, frac, ALPHA_REG_16BIT_WIDTH);
}

static int alpha_pll_lucid_set_rate(struct clk_hw *hw, unsigned long rate,
				  unsigned long prate)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	unsigned long rrate;
	u32 regval, l;
	u64 a;
	int ret;

	rrate = alpha_pll_round_rate(rate, prate, &l, &a,
					ALPHA_REG_16BIT_WIDTH);
	/*
	 * Due to a limited number of bits for fractional rate programming, the
	 * rounded up rate could be marginally higher than the requested rate.
	 */
	if (rrate > (rate + PLL_OUT_RATE_MARGIN) || rrate < rate) {
		pr_err("Call set rate on the PLL with rounded rates!\n");
		return -EINVAL;
	}

	regmap_write(pll->clkr.regmap, PLL_L_VAL(pll), l);
	regmap_write(pll->clkr.regmap, PLL_ALPHA_VAL(pll), a);

	/* Latch the PLL input */
	ret = regmap_update_bits(pll->clkr.regmap, PLL_MODE(pll),
			   PLL_UPDATE, PLL_UPDATE);
	if (ret)
		return ret;

	/* Wait for 2 reference cycles before checking the ACK bit. */
	udelay(1);
	regmap_read(pll->clkr.regmap, PLL_MODE(pll), &regval);
	if (!(regval & ALPHA_PLL_ACK_LATCH)) {
		WARN_CLK(hw->core, clk_hw_get_name(hw), 1,
				"PLL latch failed. Output may be unstable!\n");
		return -EINVAL;
	}

	/* Return the latch input to 0 */
	ret = regmap_update_bits(pll->clkr.regmap, PLL_MODE(pll),
			   PLL_UPDATE, 0);
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

static int alpha_pll_lucid_is_enabled(struct clk_hw *hw)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);

	return lucid_pll_is_enabled(pll, pll->clkr.regmap);
}

static void clk_alpha_pll_lucid_list_registers(struct seq_file *f,
		struct clk_hw *hw)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	int size, i, val;

	static struct clk_register_data data[] = {
		{"PLL_MODE", PLL_OFF_MODE},
		{"PLL_L_VAL", PLL_OFF_L_VAL},
		{"PLL_CAL_L_VAL", PLL_OFF_CAL_L_VAL},
		{"PLL_USER_CTL", PLL_OFF_USER_CTL},
		{"PLL_USER_CTL_U", PLL_OFF_USER_CTL_U},
		{"PLL_USER_CTL_U1", PLL_OFF_USER_CTL_U1},
		{"PLL_CONFIG_CTL", PLL_OFF_CONFIG_CTL},
		{"PLL_CONFIG_CTL_U", PLL_OFF_CONFIG_CTL_U},
		{"PLL_CONFIG_CTL_U1", PLL_OFF_CONFIG_CTL_U1},
		{"PLL_TEST_CTL", PLL_OFF_TEST_CTL},
		{"PLL_TEST_CTL_U", PLL_OFF_TEST_CTL_U},
		{"PLL_TEST_CTL_U1", PLL_OFF_TEST_CTL_U1},
		{"PLL_STATUS", PLL_OFF_STATUS},
		{"PLL_ALPHA_VAL", PLL_OFF_ALPHA_VAL},
	};

	static struct clk_register_data data1[] = {
		{"APSS_PLL_VOTE", 0x0},
	};

	size = ARRAY_SIZE(data);

	for (i = 0; i < size; i++) {
		regmap_read(pll->clkr.regmap, pll->offset +
					pll->regs[data[i].offset], &val);
		clock_debug_output(f, false,
				"%20s: 0x%.8x\n", data[i].name, val);
	}

	regmap_read(pll->clkr.regmap, pll->offset +
					pll->regs[data[0].offset], &val);

	if (val & PLL_FSM_ENA) {
		regmap_read(pll->clkr.regmap, pll->clkr.enable_reg +
					data1[0].offset, &val);
		clock_debug_output(f, false,
				"%20s: 0x%.8x\n", data1[0].name, val);
	}
}
const struct clk_ops clk_alpha_pll_lucid_ops = {
	.prepare = alpha_pll_lucid_prepare,
	.enable = alpha_pll_lucid_enable,
	.disable = alpha_pll_lucid_disable,
	.is_enabled = alpha_pll_lucid_is_enabled,
	.recalc_rate = alpha_pll_lucid_recalc_rate,
	.round_rate = clk_alpha_pll_round_rate,
	.set_rate = alpha_pll_lucid_set_rate,
	.list_registers = clk_alpha_pll_lucid_list_registers,
	.bus_vote = clk_debug_bus_vote,
};
EXPORT_SYMBOL_GPL(clk_alpha_pll_lucid_ops);

const struct clk_ops clk_alpha_pll_fixed_lucid_ops = {
	.enable = alpha_pll_lucid_enable,
	.disable = alpha_pll_lucid_disable,
	.is_enabled = alpha_pll_lucid_is_enabled,
	.recalc_rate = alpha_pll_lucid_recalc_rate,
	.round_rate = clk_alpha_pll_round_rate,
	.list_registers = clk_alpha_pll_lucid_list_registers,
	.bus_vote = clk_debug_bus_vote,
};
EXPORT_SYMBOL_GPL(clk_alpha_pll_fixed_lucid_ops);

const struct clk_ops clk_alpha_pll_postdiv_lucid_ops = {
	.recalc_rate = clk_alpha_pll_postdiv_fabia_recalc_rate,
	.round_rate = clk_alpha_pll_postdiv_fabia_round_rate,
	.set_rate = clk_alpha_pll_postdiv_fabia_set_rate,
	.bus_vote = clk_debug_bus_vote,
};
EXPORT_SYMBOL_GPL(clk_alpha_pll_postdiv_lucid_ops);

int clk_agera_pll_configure(struct clk_alpha_pll *pll, struct regmap *regmap,
				const struct alpha_pll_config *config)
{
	u32 val, mask;

	if (!config) {
		pr_err("PLL configuration missing.\n");
		return -EINVAL;
	}

	if (config->l)
		regmap_write(regmap, PLL_L_VAL(pll), config->l);

	if (config->alpha)
		regmap_write(regmap, PLL_ALPHA_VAL(pll), config->alpha);

	if (config->post_div_mask) {
		mask = config->post_div_mask;
		val = config->post_div_val;
		regmap_update_bits(regmap, PLL_USER_CTL(pll), mask, val);
	}

	if (config->main_output_mask || config->aux_output_mask ||
		config->aux2_output_mask || config->early_output_mask) {

		val = config->main_output_mask;
		val |= config->aux_output_mask;
		val |= config->aux2_output_mask;
		val |= config->early_output_mask;

		mask = config->main_output_mask;
		mask |= config->aux_output_mask;
		mask |= config->aux2_output_mask;
		mask |= config->early_output_mask;

		regmap_update_bits(regmap, PLL_USER_CTL(pll), mask, val);
	}

	if (config->config_ctl_val)
		regmap_write(regmap, PLL_CONFIG_CTL(pll),
					config->config_ctl_val);

	if (config->config_ctl_hi_val)
		regmap_write(regmap, PLL_CONFIG_CTL_U(pll),
					config->config_ctl_hi_val);

	if (config->test_ctl_val)
		regmap_write(regmap, PLL_TEST_CTL(pll), config->test_ctl_val);

	if (config->test_ctl_hi_val)
		regmap_write(regmap, PLL_TEST_CTL_U(pll),
					config->test_ctl_hi_val);

	return 0;
}

static unsigned long
clk_agera_pll_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	u32 l, a, alpha_width = pll_alpha_width(pll);
	u64 prate = parent_rate;

	regmap_read(pll->clkr.regmap, PLL_L_VAL(pll), &l);
	regmap_read(pll->clkr.regmap, PLL_ALPHA_VAL(pll), &a);

	return alpha_pll_calc_rate(prate, l, a, alpha_width);
}

static int clk_agera_pll_set_rate(struct clk_hw *hw, unsigned long rate,
				  unsigned long prate)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	unsigned long rrate;
	int ret;
	u32 l, alpha_width = pll_alpha_width(pll);
	u64 a;

	rrate = alpha_pll_round_rate(rate, prate, &l, &a, alpha_width);
	/*
	 * Due to limited number of bits for fractional rate programming, the
	 * rounded up rate could be marginally higher than the requested rate.
	 */
	if (rrate > (rate + PLL_OUT_RATE_MARGIN) || rrate < rate) {
		pr_err("Call set rate on the PLL with rounded rates!\n");
		return -EINVAL;
	}

	/* change L_VAL without having to go through the power on sequence */
	regmap_write(pll->clkr.regmap, PLL_L_VAL(pll), l);
	regmap_write(pll->clkr.regmap, PLL_ALPHA_VAL(pll), a);

	/* Ensure that the write above goes through before proceeding. */
	mb();

	if (clk_hw_is_enabled(hw)) {
		ret = wait_for_pll_enable_lock(pll);
		if (ret) {
			pr_err("Failed to lock after L_VAL update\n");
			return ret;
		}
	}

	return 0;
}

static void clk_agera_pll_list_registers(struct seq_file *f, struct clk_hw *hw)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	int size, i, val;

	static struct clk_register_data data[] = {
		{"PLL_MODE", PLL_OFF_MODE},
		{"PLL_L_VAL", PLL_OFF_L_VAL},
		{"PLL_ALPHA_VAL", PLL_OFF_ALPHA_VAL},
		{"PLL_USER_CTL", PLL_OFF_USER_CTL},
		{"PLL_CONFIG_CTL", PLL_OFF_CONFIG_CTL},
		{"PLL_CONFIG_CTL_U", PLL_OFF_CONFIG_CTL_U},
		{"PLL_TEST_CTL", PLL_OFF_TEST_CTL},
		{"PLL_TEST_CTL_U", PLL_OFF_TEST_CTL_U},
		{"PLL_STATUS", PLL_OFF_STATUS},
	};

	static struct clk_register_data data1 = {
		"APSS_PLL_VOTE", 0x0
	};

	size = ARRAY_SIZE(data);

	for (i = 0; i < size; i++) {
		regmap_read(pll->clkr.regmap, pll->offset +
					pll->regs[data[i].offset], &val);
		clock_debug_output(f, false,
				"%20s: 0x%.8x\n", data[i].name, val);
	}

	regmap_read(pll->clkr.regmap, pll->offset +
					pll->regs[data[0].offset], &val);

	if (val & PLL_FSM_ENA) {
		regmap_read(pll->clkr.regmap, pll->clkr.enable_reg +
					data1.offset, &val);
		clock_debug_output(f, false,
				"%20s: 0x%.8x\n", data1.name, val);
	}
}

const struct clk_ops clk_alpha_pll_agera_ops = {
	.enable = clk_alpha_pll_enable,
	.disable = clk_alpha_pll_disable,
	.is_enabled = clk_alpha_pll_is_enabled,
	.recalc_rate = clk_agera_pll_recalc_rate,
	.round_rate = clk_alpha_pll_round_rate,
	.set_rate = clk_agera_pll_set_rate,
	.list_registers = clk_agera_pll_list_registers,
	.bus_vote = clk_debug_bus_vote,
};
EXPORT_SYMBOL(clk_alpha_pll_agera_ops);

static int clk_alpha_pll_slew_update(struct clk_alpha_pll *pll)
{
	int ret = 0;
	u32 val;

	regmap_update_bits(pll->clkr.regmap, PLL_MODE(pll),
					PLL_UPDATE, PLL_UPDATE);
	regmap_read(pll->clkr.regmap, PLL_MODE(pll), &val);

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

static int clk_alpha_pll_slew_set_rate(struct clk_hw *hw, unsigned long rate,
			unsigned long parent_rate)
{
	struct clk_alpha_pll *pll = to_clk_alpha_pll(hw);
	unsigned long freq_hz;
	const struct pll_vco *curr_vco, *vco;
	u32 l, alpha_width = pll_alpha_width(pll);
	u64 a;

	freq_hz =  alpha_pll_round_rate(rate, parent_rate, &l, &a, alpha_width);
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

	regmap_write(pll->clkr.regmap, PLL_L_VAL(pll), l);
	regmap_write(pll->clkr.regmap, PLL_ALPHA_VAL(pll), a);
	regmap_write(pll->clkr.regmap, PLL_ALPHA_VAL_U(pll), a >> 32);

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
	const struct pll_vco *vco;
	u64 a;
	u32 l, alpha_width = pll_alpha_width(pll);
	int rc;

	parent = clk_hw_get_parent(hw);
	if (!parent) {
		pr_err("alpha pll: no valid parent found\n");
		return -EINVAL;
	}

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
			clk_hw_get_rate(parent), &l, &a, alpha_width);
	if (freq_hz != calibration_freq) {
		pr_err("alpha_pll: call clk_set_rate with rounded rates!\n");
		return -EINVAL;
	}

	/* Setup PLL for calibration frequency */
	a <<= (ALPHA_REG_BITWIDTH - ALPHA_BITWIDTH);

	regmap_write(pll->clkr.regmap, PLL_L_VAL(pll), l);
	regmap_write(pll->clkr.regmap, PLL_ALPHA_VAL(pll), a);
	regmap_write(pll->clkr.regmap, PLL_ALPHA_VAL_U(pll), a >> 32);

	regmap_update_bits(pll->clkr.regmap, PLL_USER_CTL(pll),
				PLL_VCO_MASK << PLL_VCO_SHIFT,
				vco->val << PLL_VCO_SHIFT);

	regmap_update_bits(pll->clkr.regmap, PLL_USER_CTL(pll),
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
			clk_hw_get_rate(parent), &l, &a, alpha_width);


	pr_debug("pll %s: setting back to required rate %lu, freq_hz %ld\n",
				hw->init->name, clk_hw_get_rate(hw), freq_hz);

	/* Setup the PLL for the new frequency */
	a <<= (ALPHA_REG_BITWIDTH - ALPHA_BITWIDTH);

	regmap_write(pll->clkr.regmap, PLL_L_VAL(pll), l);
	regmap_write(pll->clkr.regmap, PLL_ALPHA_VAL(pll), a);
	regmap_write(pll->clkr.regmap, PLL_ALPHA_VAL_U(pll), a >> 32);

	regmap_update_bits(pll->clkr.regmap, PLL_USER_CTL(pll),
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
	.list_registers = clk_alpha_pll_list_registers,
};
