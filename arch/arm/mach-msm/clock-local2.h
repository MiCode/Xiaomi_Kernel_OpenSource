/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#ifndef __ARCH_ARM_MACH_MSM_CLOCK_LOCAL_2_H
#define __ARCH_ARM_MACH_MSM_CLOCK_LOCAL_2_H

#include <linux/spinlock.h>
#include "clock.h"

/*
 * Generic frequency-definition structs and macros
 */

/**
 * @freq_hz: output rate
 * @src_clk: source clock for freq_hz
 * @m_val: M value corresponding to freq_hz
 * @n_val: N value corresponding to freq_hz
 * @d_val: D value corresponding to freq_hz
 * @div_src_val: Pre divider value and source selection mux index for freq_hz
 * @sys_vdd: Voltage level required for freq_hz
 */
struct clk_freq_tbl {
	unsigned long	freq_hz;
	struct clk	*src_clk;
	const u32	m_val;
	const u32	n_val;
	const u32	d_val;
	const u32	div_src_val;
	const unsigned	sys_vdd;
};

#define FREQ_END	(UINT_MAX-1)
#define F_END { .freq_hz = FREQ_END }

/*
 * Generic clock-definition struct and macros
 */
/**
 * struct rcg_clk - root clock generator
 * @cmd_rcgr_reg: command register
 * @set_rate: function to set frequency
 * @freq_tbl: frequency table for this RCG
 * @current_freq: current RCG frequency
 * @c: generic clock data
 * @base: pointer to base address of ioremapped registers.
 */
struct rcg_clk {
	const u32 cmd_rcgr_reg;

	void   (*set_rate)(struct rcg_clk *, struct clk_freq_tbl *);

	struct clk_freq_tbl *freq_tbl;
	struct clk_freq_tbl *current_freq;
	struct clk	c;

	void *const __iomem *base;
};

static inline struct rcg_clk *to_rcg_clk(struct clk *clk)
{
	return container_of(clk, struct rcg_clk, c);
}

extern struct clk_freq_tbl rcg_dummy_freq;

/**
 * struct fixed_clk - fixed rate clock (used for crystal oscillators)
 * @rate: output rate
 * @c: clk
 */
struct fixed_clk {
	struct clk c;
};

/**
 * struct branch_clk - branch clock
 * @set_rate: Set the frequency of this branch clock.
 * @parent: clock source
 * @c: clk
 * @cbcr_reg: branch control register
 * @bcr_reg: block reset register
 * @has_sibling: true if other branches are derived from this branch's source
 * @cur_div: current branch divider value
 * @max_div: maximum branch divider value (if zero, no divider exists)
 * @halt_check: halt checking type
 * @base: pointer to base address of ioremapped registers.
 */
struct branch_clk {
	void   (*set_rate)(struct branch_clk *, struct clk_freq_tbl *);
	struct clk *parent;
	struct clk c;
	const u32 cbcr_reg;
	const u32 bcr_reg;
	int has_sibling;
	u32 cur_div;
	const u32 max_div;
	const u32 halt_check;
	void *const __iomem *base;
};

static inline struct branch_clk *to_branch_clk(struct clk *clk)
{
	return container_of(clk, struct branch_clk, c);
}

/**
 * struct local_vote_clk - Voteable branch clock
 * @c: clk
 * @cbcr_reg: branch control register
 * @vote_reg: voting register
 * @en_mask: enable mask
 * @halt_check: halt checking type
 * @base: pointer to base address of ioremapped registers.
 * An on/off switch with a rate derived from the parent.
 */
struct local_vote_clk {
	struct clk c;
	const u32 cbcr_reg;
	const u32 vote_reg;
	const u32 bcr_reg;
	const u32 en_mask;
	const u32 halt_check;
	void *const __iomem *base;
};

static inline struct local_vote_clk *to_local_vote_clk(struct clk *clk)
{
	return container_of(clk, struct local_vote_clk, c);
}

/**
 * struct measure_clk - for rate measurement debug use
 * @sample_ticks: sample period in reference clock ticks
 * @multiplier: measurement scale-up factor
 * @divider: measurement scale-down factor
 * @c: clk
*/
struct measure_clk {
	u64 sample_ticks;
	u32 multiplier;
	u32 divider;
	struct clk c;
};

static inline struct measure_clk *to_measure_clk(struct clk *clk)
{
	return container_of(clk, struct measure_clk, c);
}

/*
 * Generic set-rate implementations
 */
void set_rate_mnd(struct rcg_clk *clk, struct clk_freq_tbl *nf);
void set_rate_hid(struct rcg_clk *clk, struct clk_freq_tbl *nf);

/*
 * Variables from the clock-local driver
 */
extern spinlock_t local_clock_reg_lock;

extern struct clk_ops clk_ops_rcg;
extern struct clk_ops clk_ops_rcg_mnd;
extern struct clk_ops clk_ops_branch;
extern struct clk_ops clk_ops_vote;

#endif /* __ARCH_ARM_MACH_MSM_CLOCK_LOCAL_2_H */

