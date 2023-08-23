/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
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

#ifndef __VIRTIO_CLK_COMMON__
#define __VIRTIO_CLK_COMMON__

#include <linux/types.h>

/*
 * struct clk_virtio_desc - virtio clock descriptor
 * clk_names: the pointer of clock name pointer
 * num_clks: number of clocks
 * reset_names: the pointer of reset name pointer
 * num_resets: number of resets
 */
struct clk_virtio_desc {
	const char * const *clk_names;
	size_t num_clks;
	const char * const *reset_names;
	size_t num_resets;
};

extern const struct clk_virtio_desc clk_virtio_sm8150_gcc;
extern const struct clk_virtio_desc clk_virtio_sm8150_scc;
extern const struct clk_virtio_desc clk_virtio_sm6150_gcc;
extern const struct clk_virtio_desc clk_virtio_sm6150_scc;
extern const struct clk_virtio_desc clk_virtio_sa8195p_gcc;

#endif
