/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#ifndef __QCOM_CLK_DEBUG_H__
#define __QCOM_CLK_DEBUG_H__

#include "../clk.h"

/* Debugfs Measure Clocks */

/**
 * struct measure_clk_data - Structure of clk measure
 *
 * @cxo:		XO clock.
 * @xo_div4_cbcr:	offset of debug XO/4 div register.
 * @ctl_reg:		offset of debug control register.
 * @status_reg:		offset of debug status register.
 * @cbcr_offset:	branch register to turn on debug mux.
 */
struct measure_clk_data {
	struct clk *cxo;
	u32 ctl_reg;
	u32 status_reg;
	u32 xo_div4_cbcr;
};

/**
 * List of Debug clock controllers.
 */
enum debug_cc {
	GCC,
	CAM_CC,
	DISP_CC,
	GPU_CC,
	VIDEO_CC,
	CPU,
	MAX_NUM_CC,
};

/**
 * struct clk_src - Structure of clock source for debug mux
 *
 * @parents:		clock name to be used as parent for debug mux.
 * @prim_mux_sel:	debug mux index at global clock controller.
 * @prim_mux_div_val:	PLL post-divider setting for the primary mux.
 * @dbg_cc:		indicates the clock controller for recursive debug
 *			clock controllers.
 * @dbg_cc_mux_sel:	indicates the debug mux index at recursive debug mux.
 * @mux_sel_mask:	indicates the mask for the mux selection.
 * @mux_sel_shift:	indicates the shift required for mux selection.
 * @post_div_mask:	indicates the post div mask to be used at recursive
 *			debug mux.
 * @post_div_shift:	indicates the shift required for post divider
 *			configuration.
 * @post_div_val:	indicates the post div value to be used at recursive
 *			debug mux.
 * @mux_offset:		the debug mux offset.
 * @post_div_offset:	register with post-divider settings for the debug mux.
 * @cbcr_offset:	branch register to turn on debug mux.
 * @misc_div_val:	includes any pre-set dividers in the measurement logic.
 */
struct clk_src {
	const char *parents;
	int prim_mux_sel;
	u32 prim_mux_div_val;
	enum debug_cc dbg_cc;
	int dbg_cc_mux_sel;
	u32 mux_sel_mask;
	u32 mux_sel_shift;
	u32 post_div_mask;
	u32 post_div_shift;
	u32 post_div_val;
	u32 mux_offset;
	u32 post_div_offset;
	u32 cbcr_offset;
	u32 misc_div_val;
};

#define MUX_SRC_LIST(...) \
	.parent = (struct clk_src[]){__VA_ARGS__}, \
	.num_parents = ARRAY_SIZE(((struct clk_src[]){__VA_ARGS__}))

/**
 * struct clk_debug_mux - Structure of clock debug mux
 *
 * @parent:		structure of clk_src
 * @num_parents:	number of parents
 * @regmap:		regmaps of debug mux
 * @priv:		private measure_clk_data to be used by debug mux
 * @debug_offset:	debug mux offset.
 * @post_div_offset:	register with post-divider settings for the debug mux.
 * @cbcr_offset:	branch register to turn on debug mux.
 * @src_sel_mask:	indicates the mask to be used for src selection in
			primary mux.
 * @src_sel_shift:	indicates the shift required for source selection in
			primary mux.
 * @post_div_mask:	indicates the post div mask to be used for the primary
			mux.
 * @post_div_shift:	indicates the shift required for post divider
			selection in primary mux.
 * @hw:			handle between common and hardware-specific interfaces.
 */
struct clk_debug_mux {
	struct clk_src *parent;
	int num_parents;
	struct regmap **regmap;
	void *priv;
	u32 debug_offset;
	u32 post_div_offset;
	u32 cbcr_offset;
	u32 src_sel_mask;
	u32 src_sel_shift;
	u32 post_div_mask;
	u32 post_div_shift;
	struct clk_hw hw;
};

#define to_clk_measure(_hw) container_of((_hw), struct clk_debug_mux, hw)

extern const struct clk_ops clk_debug_mux_ops;

int clk_debug_measure_register(struct clk_hw *hw);
int clk_debug_measure_add(struct clk_hw *hw, struct dentry *dentry);

#endif
