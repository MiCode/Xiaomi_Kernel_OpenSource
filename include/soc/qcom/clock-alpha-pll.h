/*
 * Copyright (c) 2012-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ARCH_ARM_MACH_MSM_CLOCK_ALPHA_PLL_H
#define __ARCH_ARM_MACH_MSM_CLOCK_ALPHA_PLL_H

#include <linux/spinlock.h>
#include <linux/clk/msm-clk-provider.h>

struct alpha_pll_masks {
	u32 lock_mask;		/* lock_det bit */
	u32 active_mask;	/* active_flag in FSM mode */
	u32 update_mask;	/* update bit for dynamic update */
	u32 vco_mask;		/* vco_sel bits */
	u32 vco_shift;
	u32 alpha_en_mask;	/* alpha_en bit */
	u32 output_mask;	/* pllout_* bits */
	u32 post_div_mask;

	u32 test_ctl_lo_mask;
	u32 test_ctl_hi_mask;
};

struct alpha_pll_vco_tbl {
	u32 vco_val;
	unsigned long min_freq;
	unsigned long max_freq;
};

#define VCO(a, b, c) { \
	.vco_val = a,\
	.min_freq = b,\
	.max_freq = c,\
}

struct alpha_pll_clk {
	struct alpha_pll_masks *masks;
	void *const __iomem *base;
	u32 offset;

	/* if fsm_en_mask is set, config PLL to FSM mode */
	u32 fsm_reg_offset;
	u32 fsm_en_mask;

	u32 enable_config;	/* bitmask of outputs to be enabled */
	u32 post_div_config;	/* masked post divider setting */
	u32 config_ctl_val;	/* config register init value */
	u32 test_ctl_lo_val;	/* test control settings */
	u32 test_ctl_hi_val;

	struct alpha_pll_vco_tbl *vco_tbl;
	u32 num_vco;
	u32 current_vco_val;
	bool inited;
	bool slew;
	bool no_prepared_reconfig;

	/* some PLLs support dynamically updating their rate
	 * without disabling the PLL first. Set this flag
	 * to enable this support.
	 */
	bool dynamic_update;

	/*
	 * Some chipsets need the offline request bit to be
	 * cleared on a second write to the register, even though
	 * SW wants the bit to be set. Set this flag to indicate
	 * that the workaround is required.
	 */
	bool offline_bit_workaround;
	bool is_fabia;
	unsigned long min_supported_freq;
	struct clk c;
};

static inline struct alpha_pll_clk *to_alpha_pll_clk(struct clk *c)
{
	return container_of(c, struct alpha_pll_clk, c);
}


#endif
extern void __init_alpha_pll(struct clk *c);
extern struct clk_ops clk_ops_alpha_pll;
extern struct clk_ops clk_ops_alpha_pll_hwfsm;
extern struct clk_ops clk_ops_fixed_alpha_pll;
extern struct clk_ops clk_ops_dyna_alpha_pll;
extern struct clk_ops clk_ops_fixed_fabia_alpha_pll;
extern struct clk_ops clk_ops_fabia_alpha_pll;
