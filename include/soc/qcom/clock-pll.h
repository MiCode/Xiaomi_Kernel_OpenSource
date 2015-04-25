/*
 * Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

#ifndef __ARCH_ARM_MACH_MSM_CLOCK_PLL_H
#define __ARCH_ARM_MACH_MSM_CLOCK_PLL_H

#include <linux/clk/msm-clk-provider.h>

/**
 * struct pll_freq_tbl - generic PLL frequency definition
 * @freq_hz: pll frequency in hz
 * @l_val: pll l value
 * @m_val: pll m value
 * @n_val: pll n value
 * @post_div_val: pll post divider value
 * @pre_div_val: pll pre-divider value
 * @vco_val: pll vco value
 */
struct pll_freq_tbl {
	const u32 freq_hz;
	const u32 l_val;
	const u32 m_val;
	const u32 n_val;
	const u32 post_div_val;
	const u32 pre_div_val;
	const u32 vco_val;
};

/**
 * struct pll_config_masks - PLL config masks struct
 * @post_div_mask: mask for post divider bits location
 * @pre_div_mask: mask for pre-divider bits location
 * @vco_mask: mask for vco bits location
 * @mn_en_mask: ORed with pll config register to enable the mn counter
 * @main_output_mask: ORed with pll config register to enable the main output
 * @apc_pdn_mask: ORed with pll config register to enable/disable APC PDN
 * @lock_mask: Mask that indicates that the PLL has locked
 */
struct pll_config_masks {
	u32 apc_pdn_mask;
	u32 post_div_mask;
	u32 pre_div_mask;
	u32 vco_mask;
	u32 mn_en_mask;
	u32 main_output_mask;
	u32 early_output_mask;
	u32 lock_mask;
};

struct pll_config_vals {
	u32 post_div_masked;
	u32 pre_div_masked;
	u32 config_ctl_val;
	u32 test_ctl_lo_val;
	u32 test_ctl_hi_val;
	u32 alpha_val;
	bool enable_mn;
};

#define PLL_FREQ_END	(UINT_MAX-1)
#define PLL_F_END { .freq_hz = PLL_FREQ_END }

/**
 * struct pll_vote_clk - phase locked loop (HW voteable)
 * @soft_vote: soft voting variable for multiple PLL software instances
 * @soft_vote_mask: soft voting mask for multiple PLL software instances
 * @en_reg: enable register
 * @en_mask: ORed with @en_reg to enable the clock
 * @status_mask: ANDed with @status_reg to determine if PLL is active.
 * @status_reg: status register
 * @c: clock
 */
struct pll_vote_clk {
	u32 *soft_vote;
	u32 soft_vote_mask;
	void __iomem *const en_reg;
	u32 en_mask;
	void __iomem *const status_reg;
	u32 status_mask;

	struct clk c;
	void *const __iomem *base;
};

extern struct clk_ops clk_ops_pll_vote;
extern struct clk_ops clk_ops_pll_acpu_vote;

/* Soft voting values */
#define PLL_SOFT_VOTE_PRIMARY   BIT(0)
#define PLL_SOFT_VOTE_ACPU      BIT(1)
#define PLL_SOFT_VOTE_AUX       BIT(2)

static inline struct pll_vote_clk *to_pll_vote_clk(struct clk *c)
{
	return container_of(c, struct pll_vote_clk, c);
}

/**
 * struct pll_clk - phase locked loop
 * @mode_reg: enable register
 * @l_reg: l value register
 * @m_reg: m value register
 * @n_reg: n value register
 * @config_reg: configuration register, contains mn divider enable, pre divider,
 *   post divider and vco configuration. register name can be configure register
 *   or user_ctl register depending on targets
 * @status_reg: status register, contains the lock detection bit
 * @init_test_ctl: initialize the test control register
 * @pgm_test_ctl_enable: program the test_ctl register in the enable sequence
 * @masks: masks used for settings in config_reg
 * @vals: configuration values to be written to PLL registers
 * @freq_tbl: pll freq table
 * @no_prepared_reconfig: Fail round_rate if pll is prepared
 * @c: clk
 * @base: pointer to base address of ioremapped registers.
 */
struct pll_clk {
	void __iomem *const mode_reg;
	void __iomem *const l_reg;
	void __iomem *const m_reg;
	void __iomem *const n_reg;
	void __iomem *const alpha_reg;
	void __iomem *const config_reg;
	void __iomem *const config_ctl_reg;
	void __iomem *const status_reg;
	void __iomem *const alt_status_reg;
	void __iomem *const test_ctl_lo_reg;
	void __iomem *const test_ctl_hi_reg;

	bool init_test_ctl;
	bool pgm_test_ctl_enable;

	struct pll_config_masks masks;
	struct pll_config_vals vals;
	struct pll_freq_tbl *freq_tbl;

	unsigned long src_rate;
	unsigned long min_rate;
	unsigned long max_rate;

	bool inited;
	bool no_prepared_reconfig;

	struct clk c;
	void *const __iomem *base;
};

extern struct clk_ops clk_ops_local_pll;
extern struct clk_ops clk_ops_sr2_pll;
extern struct clk_ops clk_ops_variable_rate_pll;

static inline struct pll_clk *to_pll_clk(struct clk *c)
{
	return container_of(c, struct pll_clk, c);
}

int sr_pll_clk_enable(struct clk *c);
int sr_hpm_lp_pll_clk_enable(struct clk *c);

struct pll_alt_config {
	u32 val;
	u32 mask;
};

struct pll_config {
	u32 l;
	u32 m;
	u32 n;
	u32 vco_val;
	u32 vco_mask;
	u32 pre_div_val;
	u32 pre_div_mask;
	u32 post_div_val;
	u32 post_div_mask;
	u32 mn_ena_val;
	u32 mn_ena_mask;
	u32 main_output_val;
	u32 main_output_mask;
	u32 aux_output_val;
	u32 aux_output_mask;
	u32 cfg_ctl_val;
	/* SR2 PLL specific fields */
	u32 add_factor_val;
	u32 add_factor_mask;
	struct pll_alt_config alt_cfg;
};

struct pll_config_regs {
	void __iomem *l_reg;
	void __iomem *m_reg;
	void __iomem *n_reg;
	void __iomem *config_reg;
	void __iomem *config_alt_reg;
	void __iomem *config_ctl_reg;
	void __iomem *mode_reg;
	void *const __iomem *base;
};

void configure_sr_pll(struct pll_config *config, struct pll_config_regs *regs,
				u32 ena_fsm_mode);
void configure_sr_hpm_lp_pll(struct pll_config *config,
				struct pll_config_regs *, u32 ena_fsm_mode);
#endif
