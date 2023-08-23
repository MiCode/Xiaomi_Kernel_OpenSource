/* Copyright (c) 2013-2017, The Linux Foundation. All rights reserved.
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

#ifndef __MSM_CLOCK_GENERIC_H
#define __MSM_CLOCK_GENERIC_H

#include <linux/clk/msm-clk-provider.h>
#include <linux/of.h>

/**
 * struct fixed_clk - fixed rate clock
 * @c: clk
 */
struct fixed_clk {
	struct clk c;
};

/* ==================== Mux clock ==================== */

struct mux_clk;

struct clk_mux_ops {
	int (*set_mux_sel)(struct mux_clk *clk, int sel);
	int (*get_mux_sel)(struct mux_clk *clk);

	/* Optional */
	bool (*is_enabled)(struct mux_clk *clk);
	int (*enable)(struct mux_clk *clk);
	void (*disable)(struct mux_clk *clk);
	void __iomem *(*list_registers)(struct mux_clk *clk, int n,
				struct clk_register_data **regs, u32 *size);
};

#define MUX_SRC_LIST(...) \
	.parents = (struct clk_src[]){__VA_ARGS__}, \
	.num_parents = ARRAY_SIZE(((struct clk_src[]){__VA_ARGS__}))

#define MUX_REC_SRC_LIST(...) \
	.rec_parents = (struct clk * []){__VA_ARGS__}, \
	.num_rec_parents = ARRAY_SIZE(((struct clk * []){__VA_ARGS__}))

struct mux_clk {
	/* Parents in decreasing order of preference for obtaining rates. */
	struct clk_src	*parents;
	int		num_parents;
	/* Recursively search for the requested parent in rec_parents. */
	struct clk	**rec_parents;
	int		num_rec_parents;
	struct clk	*safe_parent;
	int		safe_sel;
	unsigned long	safe_freq;
	/*
	 * Before attempting a clk_round_rate on available sources, attempt a
	 * clk_get_rate on all those sources. If one of them is already at the
	 * necessary rate, that source will be used.
	 */
	bool		try_get_rate;
	struct clk_mux_ops *ops;
	/*
	 * Set if you need the mux to try a new parent before falling back to
	 * the current parent. If the safe_parent field above is set, then the
	 * safe_sel intermediate source will only be used if we fall back to
	 * to the current parent during mux_set_rate.
	 */
	bool		try_new_parent;

	/* Fields not used by helper function. */
	void *const __iomem *base;
	u32		offset;
	u32		en_offset;
	u32		mask;
	u32		shift;
	u32		en_mask;
	/*
	 * Set post divider for debug mux in order to divide the clock
	 * by post_div + 1.
	 */
	u32		post_div;
	int		low_power_sel;
	void		*priv;

	struct clk	c;
};

static inline struct mux_clk *to_mux_clk(struct clk *c)
{
	return container_of(c, struct mux_clk, c);
}

extern const  struct clk_ops clk_ops_gen_mux;

/* ==================== Divider clock ==================== */

struct div_clk;

struct clk_div_ops {
	int (*set_div)(struct div_clk *clk, int div);
	int (*get_div)(struct div_clk *clk);
	bool (*is_enabled)(struct div_clk *clk);
	int (*enable)(struct div_clk *clk);
	void (*disable)(struct div_clk *clk);
	void __iomem *(*list_registers)(struct div_clk *clk, int n,
				struct clk_register_data **regs, u32 *size);
};

struct div_data {
	unsigned int div;
	unsigned int min_div;
	unsigned int max_div;
	unsigned long rate_margin;
	/*
	 * Indicate whether this divider clock supports half-integer divider.
	 * If it is, all the min_div and max_div have been doubled. It means
	 * they are 2*N.
	 */
	bool is_half_divider;
	/*
	 * Skip odd dividers since the hardware may not support them.
	 */
	bool skip_odd_div;
	bool skip_even_div;
	bool allow_div_one;
	unsigned int cached_div;
};

struct div_clk {
	struct div_data data;

	/*
	 * Some implementations may require the divider to be set to a "safe"
	 * value that allows reprogramming of upstream clocks without violating
	 * voltage constraints.
	 */
	unsigned long safe_freq;

	/* Optional */
	struct clk_div_ops *ops;

	/* Fields not used by helper function. */
	void *const __iomem *base;
	u32		offset;
	u32		mask;
	u32		shift;
	u32		en_mask;
	void		*priv;
	struct clk	c;
};

static inline struct div_clk *to_div_clk(struct clk *c)
{
	return container_of(c, struct div_clk, c);
}

extern const struct clk_ops clk_ops_div;
extern const struct clk_ops clk_ops_slave_div;

struct ext_clk {
	struct clk c;
	struct device *dev;
	char *clk_id;
};

long parent_round_rate(struct clk *c, unsigned long rate);
unsigned long parent_get_rate(struct clk *c);
int parent_set_rate(struct clk *c, unsigned long rate);

static inline struct ext_clk *to_ext_clk(struct clk *c)
{
	return container_of(c, struct ext_clk, c);
}

extern const struct clk_ops clk_ops_ext;

#define DEFINE_FIXED_DIV_CLK(clk_name, _div, _parent) \
static struct div_clk clk_name = {	\
	.data = {				\
		.max_div = _div,		\
		.min_div = _div,		\
		.div = _div,			\
	},					\
	.c = {					\
		.parent = _parent,		\
		.dbg_name = #clk_name,		\
		.ops = &clk_ops_div,		\
		CLK_INIT(clk_name.c),		\
	}					\
}

#define DEFINE_FIXED_SLAVE_DIV_CLK(clk_name, _div, _parent) \
static struct div_clk clk_name = {	\
	.data = {				\
		.max_div = _div,		\
		.min_div = _div,		\
		.div = _div,			\
	},					\
	.c = {					\
		.parent = _parent,		\
		.dbg_name = #clk_name,		\
		.ops = &clk_ops_slave_div,		\
		CLK_INIT(clk_name.c),		\
	}					\
}

#define DEFINE_EXT_CLK(clk_name, _parent) \
static struct ext_clk clk_name = {		\
	.c = {					\
		.parent = _parent,		\
		.dbg_name = #clk_name,		\
		.ops = &clk_ops_ext,		\
		CLK_INIT(clk_name.c),		\
	}					\
}

/* ==================== Mux Div clock ==================== */

struct mux_div_clk;

/*
 * struct mux_div_ops
 * the enable and disable ops are optional.
 */

struct mux_div_ops {
	int (*set_src_div)(struct mux_div_clk *, u32 src_sel, u32 div);
	void (*get_src_div)(struct mux_div_clk *, u32 *src_sel, u32 *div);
	int (*enable)(struct mux_div_clk *);
	void (*disable)(struct mux_div_clk *);
	bool (*is_enabled)(struct mux_div_clk *);
	void __iomem *(*list_registers)(struct mux_div_clk *md, int n,
				struct clk_register_data **regs, u32 *size);
};

/*
 * struct mux_div_clk - combined mux/divider clock
 * @priv
		parameters needed by ops
 * @safe_freq
		when switching rates from A to B, the mux div clock will
		instead switch from A -> safe_freq -> B. This allows the
		mux_div clock to change rates while enabled, even if this
		behavior is not supported by the parent clocks.

		If changing the rate of parent A also causes the rate of
		parent B to change, then safe_freq must be defined.

		safe_freq is expected to have a source clock which is always
		on and runs at only one rate.
 * @parents
		list of parents and mux indicies
 * @ops
		function pointers for hw specific operations
 * @src_sel
		the mux index which will be used if the clock is enabled.
 * @try_get_rate
		Set if you need the mux to directly jump to a source
		that is at the desired rate currently.
 * @force_enable_md
		Set if the mux-div needs to be force enabled/disabled during
		clk_enable/disable.
 */

struct mux_div_clk {
	/* Required parameters */
	struct mux_div_ops		*ops;
	struct div_data			data;
	struct clk_src			*parents;
	u32				num_parents;

	struct clk			c;

	/* Internal */
	u32				src_sel;

	/* Optional parameters */
	void				*priv;
	void __iomem			*base;
	u32				div_mask;
	u32				div_offset;
	u32				div_shift;
	u32				src_mask;
	u32				src_offset;
	u32				src_shift;
	u32				en_mask;
	u32				en_offset;

	u32				safe_div;
	struct clk			*safe_parent;
	unsigned long			safe_freq;
	bool				try_get_rate;
	bool				force_enable_md;
};

static inline struct mux_div_clk *to_mux_div_clk(struct clk *clk)
{
	return container_of(clk, struct mux_div_clk, c);
}

extern const struct clk_ops clk_ops_mux_div_clk;

#endif
