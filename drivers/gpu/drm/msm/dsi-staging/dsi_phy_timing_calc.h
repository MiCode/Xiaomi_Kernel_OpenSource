/*
 * Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
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

#ifndef _DSI_PHY_TIMING_CALC_H_
#define _DSI_PHY_TIMING_CALC_H_

#include <linux/math64.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/bitops.h>
#include <linux/bitmap.h>
#include <linux/errno.h>

#include "dsi_defs.h"
#include "dsi_phy_hw.h"
#include "dsi_catalog.h"

/**
 * struct timing_entry - Calculated values for each timing parameter.
 * @mipi_min:
 * @mipi_max:
 * @rec_min:
 * @rec_max:
 * @rec:
 * @reg_value:       Value to be programmed in register.
 */
struct timing_entry {
	s32 mipi_min;
	s32 mipi_max;
	s32 rec_min;
	s32 rec_max;
	s32 rec;
	u8 reg_value;
};

/**
 * struct phy_timing_desc - Timing parameters for DSI PHY.
 */
struct phy_timing_desc {
	struct timing_entry clk_prepare;
	struct timing_entry clk_zero;
	struct timing_entry clk_trail;
	struct timing_entry hs_prepare;
	struct timing_entry hs_zero;
	struct timing_entry hs_trail;
	struct timing_entry hs_rqst;
	struct timing_entry hs_rqst_clk;
	struct timing_entry hs_exit;
	struct timing_entry ta_go;
	struct timing_entry ta_sure;
	struct timing_entry ta_set;
	struct timing_entry clk_post;
	struct timing_entry clk_pre;
};

/**
 * struct phy_clk_params - Clock parameters for PHY timing calculations.
 */
struct phy_clk_params {
	u32 bitclk_mbps;
	u32 escclk_numer;
	u32 escclk_denom;
	u32 tlpx_numer_ns;
	u32 treot_ns;
	u32 clk_prep_buf;
	u32 clk_zero_buf;
	u32 clk_trail_buf;
	u32 hs_prep_buf;
	u32 hs_zero_buf;
	u32 hs_trail_buf;
	u32 hs_rqst_buf;
	u32 hs_exit_buf;
	u32 clk_pre_buf;
	u32 clk_post_buf;
};

/**
 * Various Ops needed for auto-calculation of DSI PHY timing parameters.
 */
struct phy_timing_ops {
	void (*get_default_phy_params)(struct phy_clk_params *params,
			bool is_cphy);

	int32_t (*calc_clk_zero)(s64 rec_temp1, s64 mult);

	int32_t (*calc_clk_trail_rec_min)(s64 temp_mul,
		s64 frac, s64 mult);

	int32_t (*calc_clk_trail_rec_max)(s64 temp1, s64 mult);

	int32_t (*calc_hs_zero)(s64 temp1, s64 mult);

	void (*calc_hs_trail)(struct phy_clk_params *clk_params,
			struct phy_timing_desc *desc);

	void (*update_timing_params)(struct dsi_phy_per_lane_cfgs *timing,
		struct phy_timing_desc *desc, bool is_cphy);
};

#define roundup64(x, y) \
	({ u64 _tmp = (x)+(y)-1; do_div(_tmp, y); _tmp * y; })

/* DSI PHY timing functions for 14nm */
void dsi_phy_hw_v2_0_get_default_phy_params(struct phy_clk_params *params,
		bool is_cphy);

int32_t dsi_phy_hw_v2_0_calc_clk_zero(s64 rec_temp1, s64 mult);

int32_t dsi_phy_hw_v2_0_calc_clk_trail_rec_min(s64 temp_mul,
		s64 frac, s64 mult);

int32_t dsi_phy_hw_v2_0_calc_clk_trail_rec_max(s64 temp1, s64 mult);

int32_t dsi_phy_hw_v2_0_calc_hs_zero(s64 temp1, s64 mult);

void dsi_phy_hw_v2_0_calc_hs_trail(struct phy_clk_params *clk_params,
		struct phy_timing_desc *desc);

void dsi_phy_hw_v2_0_update_timing_params(struct dsi_phy_per_lane_cfgs *timing,
		struct phy_timing_desc *desc, bool is_cphy);

/* DSI PHY timing functions for 10nm */
void dsi_phy_hw_v3_0_get_default_phy_params(struct phy_clk_params *params,
		bool is_cphy);

int32_t dsi_phy_hw_v3_0_calc_clk_zero(s64 rec_temp1, s64 mult);

int32_t dsi_phy_hw_v3_0_calc_clk_trail_rec_min(s64 temp_mul,
		s64 frac, s64 mult);

int32_t dsi_phy_hw_v3_0_calc_clk_trail_rec_max(s64 temp1, s64 mult);

int32_t dsi_phy_hw_v3_0_calc_hs_zero(s64 temp1, s64 mult);

void dsi_phy_hw_v3_0_calc_hs_trail(struct phy_clk_params *clk_params,
		struct phy_timing_desc *desc);

void dsi_phy_hw_v3_0_update_timing_params(struct dsi_phy_per_lane_cfgs *timing,
		struct phy_timing_desc *desc, bool is_cphy);

/* DSI PHY timing functions for 7nm */
void dsi_phy_hw_v4_0_get_default_phy_params(struct phy_clk_params *params,
		bool is_cphy);

int32_t dsi_phy_hw_v4_0_calc_clk_zero(s64 rec_temp1, s64 mult);

int32_t dsi_phy_hw_v4_0_calc_clk_trail_rec_min(s64 temp_mul,
		s64 frac, s64 mult);

int32_t dsi_phy_hw_v4_0_calc_clk_trail_rec_max(s64 temp1, s64 mult);

int32_t dsi_phy_hw_v4_0_calc_hs_zero(s64 temp1, s64 mult);

void dsi_phy_hw_v4_0_calc_hs_trail(struct phy_clk_params *clk_params,
		struct phy_timing_desc *desc);

void dsi_phy_hw_v4_0_update_timing_params(struct dsi_phy_per_lane_cfgs *timing,
		struct phy_timing_desc *desc, bool is_cphy);

#endif /* _DSI_PHY_TIMING_CALC_H_ */
