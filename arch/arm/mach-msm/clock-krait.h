/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#ifndef __ARCH_ARM_MACH_MSM_CLOCK_KRAIT_H
#define __ARCH_ARM_MACH_MSM_CLOCK_KRAIT_H

#include <mach/clk-provider.h>
#include <mach/clock-generic.h>

extern struct clk_mux_ops clk_mux_ops_kpss;
extern struct clk_div_ops clk_div_ops_kpss_div2;

#define DEFINE_KPSS_DIV2_CLK(clk_name, _parent, _offset, _lf_tree) \
static struct div_clk clk_name = {		\
	.data = {				\
		.div = 2,			\
		.min_div = 2,			\
		.max_div = 2,			\
	},					\
	.ops = &clk_div_ops_kpss_div2,		\
	.offset = _offset,			\
	.mask = 0x3,				\
	.shift = 6,				\
	.priv = (void *) _lf_tree,		\
	.c = {					\
		.parent = _parent,		\
		.dbg_name = #clk_name,		\
		.ops = &clk_ops_div,		\
		.flags = CLKFLAG_NO_RATE_CACHE,	\
		CLK_INIT(clk_name.c),		\
	}					\
}

struct hfpll_data {
	const u32 mode_offset;
	const u32 l_offset;
	const u32 m_offset;
	const u32 n_offset;
	const u32 user_offset;
	const u32 droop_offset;
	const u32 config_offset;
	const u32 status_offset;

	const u32 droop_val;
	u32 config_val;
	const u32 user_val;
	u32 user_vco_mask;
	unsigned long low_vco_max_rate;

	unsigned long min_rate;
	unsigned long max_rate;
};

struct hfpll_clk {
	void  * __iomem base;
	struct hfpll_data const *d;
	unsigned long	src_rate;
	int		init_done;

	struct clk	c;
};

static inline struct hfpll_clk *to_hfpll_clk(struct clk *c)
{
	return container_of(c, struct hfpll_clk, c);
}

extern struct clk_ops clk_ops_hfpll;

struct avs_data {
	unsigned long	*rate;
	u32		*dscr;
	int		num;
};

struct kpss_core_clk {
	int		id;
	u32		cp15_iaddr;
	u32		l2_slp_delay;
	struct avs_data	*avs_tbl;
	struct clk	c;
};

static inline struct kpss_core_clk *to_kpss_core_clk(struct clk *c)
{
	return container_of(c, struct kpss_core_clk, c);
}

extern struct clk_ops clk_ops_kpss_cpu;
extern struct clk_ops clk_ops_kpss_l2;

extern struct kpss_core_clk krait0_clk;
extern struct kpss_core_clk krait1_clk;
extern struct kpss_core_clk krait2_clk;
extern struct kpss_core_clk krait3_clk;
extern struct kpss_core_clk l2_clk;

#endif
