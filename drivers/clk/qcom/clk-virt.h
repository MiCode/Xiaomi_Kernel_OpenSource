/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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

#ifndef __QCOM_CLK_VIRT_H__
#define __QCOM_CLK_VIRT_H__

/* remote clock flag */
#define CLOCK_FLAG_NODE_TYPE_REMOTE	0xff00

/**
 * struct clk_virt - virtual clock
 * id: clock id
 * hw: hardware clock
 * flag: clock flag
 */
struct clk_virt {
	int id;
	struct clk_hw hw;
	u32 flag;
};

/**
 * struct virt_reset_map - virtual clock map
 * clk_name: clock name
 * clk_id: clock id
 */
struct virt_reset_map {
	const char *clk_name;
	int clk_id;
};

/**
 * struct clk_virt_desc - virtual clock descriptor
 * clks: clock list pointer
 * num_clks: number of clocks
 * resets: reset map
 * num_resets: number of resets
 */
struct clk_virt_desc {
	struct clk_hw **clks;
	size_t num_clks;
	struct virt_reset_map *resets;
	size_t num_resets;
};

extern const struct clk_ops clk_virt_ops;
extern const struct clk_virt_desc clk_virt_sm8150_gcc;
extern const struct clk_virt_desc clk_virt_sm8150_scc;
extern const struct clk_virt_desc clk_virt_sm6150_gcc;
extern const struct clk_virt_desc clk_virt_sm6150_scc;

#endif
