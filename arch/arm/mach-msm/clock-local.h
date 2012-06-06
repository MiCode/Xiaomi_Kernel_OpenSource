/* Copyright (c) 2009-2012, The Linux Foundation. All rights reserved.
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

#ifndef __ARCH_ARM_MACH_MSM_CLOCK_LOCAL_H
#define __ARCH_ARM_MACH_MSM_CLOCK_LOCAL_H

#include <linux/spinlock.h>
#include "clock.h"

#define MN_MODE_DUAL_EDGE 0x2

/* MD Registers */
#define MD4(m_lsb, m, n_lsb, n) \
		((BVAL((m_lsb+3), m_lsb, m) | BVAL((n_lsb+3), n_lsb, ~(n))) \
		* !!(n))
#define MD8(m_lsb, m, n_lsb, n) \
		((BVAL((m_lsb+7), m_lsb, m) | BVAL((n_lsb+7), n_lsb, ~(n))) \
		* !!(n))
#define MD16(m, n) ((BVAL(31, 16, m) | BVAL(15, 0, ~(n))) * !!(n))

/* NS Registers */
#define NS(n_msb, n_lsb, n, m, mde_lsb, d_msb, d_lsb, d, s_msb, s_lsb, s) \
		(BVAL(n_msb, n_lsb, ~(n-m) * !!(n)) \
		| (BVAL((mde_lsb+1), mde_lsb, MN_MODE_DUAL_EDGE) * !!(n)) \
		| BVAL(d_msb, d_lsb, (d-1)) | BVAL(s_msb, s_lsb, s))

#define NS_MM(n_msb, n_lsb, n, m, d_msb, d_lsb, d, s_msb, s_lsb, s) \
		(BVAL(n_msb, n_lsb, ~(n-m) * !!(n))|BVAL(d_msb, d_lsb, (d-1)) \
		| BVAL(s_msb, s_lsb, s))

#define NS_DIVSRC(d_msb, d_lsb, d, s_msb, s_lsb, s) \
		(BVAL(d_msb, d_lsb, (d-1)) | BVAL(s_msb, s_lsb, s))

#define NS_DIV(d_msb, d_lsb, d) \
		BVAL(d_msb, d_lsb, (d-1))

#define NS_SRC_SEL(s_msb, s_lsb, s) \
		BVAL(s_msb, s_lsb, s)

#define NS_MND_BANKED4(n0_lsb, n1_lsb, n, m, s0_lsb, s1_lsb, s) \
		 (BVAL((n0_lsb+3), n0_lsb, ~(n-m) * !!(n)) \
		| BVAL((n1_lsb+3), n1_lsb, ~(n-m) * !!(n)) \
		| BVAL((s0_lsb+2), s0_lsb, s) \
		| BVAL((s1_lsb+2), s1_lsb, s))

#define NS_MND_BANKED8(n0_lsb, n1_lsb, n, m, s0_lsb, s1_lsb, s) \
		 (BVAL((n0_lsb+7), n0_lsb, ~(n-m) * !!(n)) \
		| BVAL((n1_lsb+7), n1_lsb, ~(n-m) * !!(n)) \
		| BVAL((s0_lsb+2), s0_lsb, s) \
		| BVAL((s1_lsb+2), s1_lsb, s))

#define NS_DIVSRC_BANKED(d0_msb, d0_lsb, d1_msb, d1_lsb, d, \
	s0_msb, s0_lsb, s1_msb, s1_lsb, s) \
		 (BVAL(d0_msb, d0_lsb, (d-1)) | BVAL(d1_msb, d1_lsb, (d-1)) \
		| BVAL(s0_msb, s0_lsb, s) \
		| BVAL(s1_msb, s1_lsb, s))

/* CC Registers */
#define CC(mde_lsb, n) (BVAL((mde_lsb+1), mde_lsb, MN_MODE_DUAL_EDGE) * !!(n))
#define CC_BANKED(mde0_lsb, mde1_lsb, n) \
		((BVAL((mde0_lsb+1), mde0_lsb, MN_MODE_DUAL_EDGE) \
		| BVAL((mde1_lsb+1), mde1_lsb, MN_MODE_DUAL_EDGE)) \
		* !!(n))

/*
 * Clock Definition Macros
 */
#define DEFINE_CLK_MEASURE(name) \
	struct clk name = { \
		.ops = &clk_ops_empty, \
		.dbg_name = #name, \
		CLK_INIT(name), \
	}; \

/*
 * Generic frequency-definition structs and macros
 */
struct clk_freq_tbl {
	const uint32_t	freq_hz;
	struct clk	*const src_clk;
	const uint32_t	md_val;
	const uint32_t	ns_val;
	const uint32_t	ctl_val;
	void		*const extra_freq_data;
};

/* Some clocks have two banks to avoid glitches when switching frequencies.
 * The unused bank is programmed while running on the other bank, and
 * switched to afterwards. The following two structs describe the banks. */
struct bank_mask_info {
	void *const md_reg;
	const uint32_t	ns_mask;
	const uint32_t	rst_mask;
	const uint32_t	mnd_en_mask;
	const uint32_t	mode_mask;
};

struct bank_masks {
	const uint32_t			bank_sel_mask;
	const struct bank_mask_info	bank0_mask;
	const struct bank_mask_info	bank1_mask;
};

#define F_RAW(f, sc, m_v, n_v, c_v, e) { \
	.freq_hz = f, \
	.src_clk = sc, \
	.md_val = m_v, \
	.ns_val = n_v, \
	.ctl_val = c_v, \
	.extra_freq_data = e, \
	}
#define FREQ_END	(UINT_MAX-1)
#define F_END { .freq_hz = FREQ_END }

/**
 * struct branch - branch on/off
 * @ctl_reg: clock control register
 * @en_mask: ORed with @ctl_reg to enable the clock
 * @hwcg_reg: hardware clock gating register
 * @hwcg_mask: ORed with @hwcg_reg to enable hardware clock gating
 * @halt_reg: halt register
 * @halt_check: type of halt check to perform
 * @halt_bit: ANDed with @halt_reg to test for clock halted
 * @reset_reg: reset register
 * @reset_mask: ORed with @reset_reg to reset the clock domain
 */
struct branch {
	void __iomem *const ctl_reg;
	const u32 en_mask;

	void __iomem *hwcg_reg;
	u32 hwcg_mask;

	void __iomem *const halt_reg;
	const u16 halt_check;
	const u16 halt_bit;

	void __iomem *const reset_reg;
	const u32 reset_mask;

	void __iomem *const retain_reg;
	const u32 retain_mask;
};

extern struct clk_ops clk_ops_branch;
extern struct clk_ops clk_ops_reset;

int branch_reset(struct branch *b, enum clk_reset_action action);
void __branch_enable_reg(const struct branch *b, const char *name);
u32 __branch_disable_reg(const struct branch *b, const char *name);
enum handoff branch_handoff(struct branch *b, struct clk *c);

/*
 * Generic clock-definition struct and macros
 */
struct rcg_clk {
	bool		enabled;
	void		*const ns_reg;
	void		*const md_reg;

	const uint32_t	root_en_mask;
	uint32_t	ns_mask;
	const uint32_t	ctl_mask;
	uint32_t	mnd_en_mask;

	void		*bank_info;
	void   (*set_rate)(struct rcg_clk *, struct clk_freq_tbl *);

	struct clk_freq_tbl *freq_tbl;
	struct clk_freq_tbl *current_freq;

	struct branch	b;
	struct clk	c;
};

static inline struct rcg_clk *to_rcg_clk(struct clk *c)
{
	return container_of(c, struct rcg_clk, c);
}

extern struct clk_ops clk_ops_rcg;

extern struct clk_freq_tbl rcg_dummy_freq;

/**
 * struct cdiv_clk - integer divider clock with external source selection
 * @ns_reg: source select and divider settings register
 * @ext_mask: bit to set to select an external source
 * @cur_div: current divider setting (or 0 for external source)
 * @max_div: maximum divider value supported (must be power of 2)
 * @div_offset: number of bits to shift divider left by in @ns_reg
 * @b: branch
 * @c: clock
 */
struct cdiv_clk {
	void __iomem *const ns_reg;
	u32 ext_mask;

	unsigned long cur_div;
	u8 div_offset;
	u32 max_div;

	struct branch b;
	struct clk c;
};

static inline struct cdiv_clk *to_cdiv_clk(struct clk *c)
{
	return container_of(c, struct cdiv_clk, c);
}

extern struct clk_ops clk_ops_cdiv;

/**
 * struct fixed_clk - fixed rate clock (used for crystal oscillators)
 * @c: clk
 */
struct fixed_clk {
	struct clk c;
};

/**
 * struct branch_clk - branch
 * @enabled: true if clock is on, false otherwise
 * @b: branch
 * @parent: clock source
 * @c: clock
 *
 * An on/off switch with a rate derived from the parent.
 */
struct branch_clk {
	bool enabled;
	struct branch b;
	struct clk *parent;
	struct clk c;
};

static inline struct branch_clk *to_branch_clk(struct clk *c)
{
	return container_of(c, struct branch_clk, c);
}

/**
 * struct measure_clk - for rate measurement debug use
 * @sample_ticks: sample period in reference clock ticks
 * @multiplier: measurement scale-up factor
 * @divider: measurement scale-down factor
 * @c: clock
*/
struct measure_clk {
	u64 sample_ticks;
	u32 multiplier;
	u32 divider;
	struct clk c;
};

extern struct clk_ops clk_ops_empty;

static inline struct measure_clk *to_measure_clk(struct clk *c)
{
	return container_of(c, struct measure_clk, c);
}

/*
 * Variables from clock-local driver
 */
extern spinlock_t		local_clock_reg_lock;
extern struct fixed_clk		gnd_clk;

/*
 * Generic set-rate implementations
 */
void set_rate_mnd(struct rcg_clk *rcg, struct clk_freq_tbl *nf);
void set_rate_nop(struct rcg_clk *rcg, struct clk_freq_tbl *nf);
void set_rate_mnd_8(struct rcg_clk *rcg, struct clk_freq_tbl *nf);
void set_rate_mnd_banked(struct rcg_clk *rcg, struct clk_freq_tbl *nf);
void set_rate_div_banked(struct rcg_clk *rcg, struct clk_freq_tbl *nf);

#endif /* __ARCH_ARM_MACH_MSM_CLOCK_LOCAL_H */

