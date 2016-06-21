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

#ifndef __QCOM_CLK_KRAIT_H
#define __QCOM_CLK_KRAIT_H

#include <linux/clk-provider.h>

struct krait_mux_clk {
	unsigned int	*parent_map;
	bool		has_safe_parent;
	u8		safe_sel;
	u32		offset;
	u32		mask;
	u32		shift;
	u32		en_mask;
	bool		lpl;

	struct clk_hw	hw;
};

#define to_krait_mux_clk(_hw) container_of(_hw, struct krait_mux_clk, hw)

extern const struct clk_ops krait_mux_clk_ops;

struct krait_div2_clk {
	u32		offset;
	u8		width;
	u32		shift;
	bool		lpl;

	struct clk_hw	hw;
};

#define to_krait_div2_clk(_hw) container_of(_hw, struct krait_div2_clk, hw)

extern const struct clk_ops krait_div2_clk_ops;

#endif
