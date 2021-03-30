// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
 */

#include "dsi_phy_timing_calc.h"

void dsi_phy_hw_v4_0_get_default_phy_params(
		struct phy_clk_params *params, u32 phy_type)
{
	if (phy_type == DSI_PHY_TYPE_CPHY) {
		params->clk_prep_buf = 50;
		params->clk_pre_buf = 20;
		params->clk_post_buf = 80;
		params->hs_rqst_buf = 1;
		params->hs_exit_buf = 10;
	} else {
		params->clk_prep_buf = 50;
		params->clk_zero_buf = 2;
		params->clk_trail_buf = 30;
		params->hs_prep_buf = 50;
		params->hs_zero_buf = 10;
		params->hs_trail_buf = 30;
		params->hs_rqst_buf = 0;
		params->hs_exit_buf = 10;
		/* 1.25 is used in code for precision */
		params->clk_pre_buf = 1;
		params->clk_post_buf = 5;
	}
}

int32_t dsi_phy_hw_v4_0_calc_clk_zero(s64 rec_temp1, s64 mult)
{
	s64 rec_temp2, rec_temp3;

	rec_temp2 = rec_temp1;
	rec_temp3 = roundup64(div_s64(rec_temp2, 8), mult);
	return (div_s64(rec_temp3, mult) - 1);
}

int32_t dsi_phy_hw_v4_0_calc_clk_trail_rec_min(s64 temp_mul,
		s64 frac, s64 mult)
{
	s64 rec_temp1, rec_temp2, rec_temp3;

	rec_temp1 = temp_mul;
	rec_temp2 = div_s64(rec_temp1, 8);
	rec_temp3 = roundup64(rec_temp2, mult);
	return (div_s64(rec_temp3, mult) - 1);
}

int32_t dsi_phy_hw_v4_0_calc_clk_trail_rec_max(s64 temp1, s64 mult)
{
	s64 rec_temp2;

	rec_temp2 = temp1 / 8;
	return (div_s64(rec_temp2, mult) - 1);
}

int32_t dsi_phy_hw_v4_0_calc_hs_zero(s64 temp1, s64 mult)
{
	s64 rec_temp2, rec_min;

	rec_temp2 = roundup64((temp1 / 8), mult);
	rec_min = rec_temp2 - (1 * mult);
	return div_s64(rec_min, mult);
}

void dsi_phy_hw_v4_0_calc_hs_trail(struct phy_clk_params *clk_params,
			struct phy_timing_desc *desc)
{
	s64 rec_temp1;
	struct timing_entry *t = &desc->hs_trail;

	t->rec_min = DIV_ROUND_UP(
			(t->mipi_min * clk_params->bitclk_mbps),
			(8 * clk_params->tlpx_numer_ns)) - 1;

	rec_temp1 = (t->mipi_max * clk_params->bitclk_mbps);
	t->rec_max =
		 (div_s64(rec_temp1, (8 * clk_params->tlpx_numer_ns))) - 1;
}

void dsi_phy_hw_v4_0_update_timing_params(
	struct dsi_phy_per_lane_cfgs *timing,
	struct phy_timing_desc *desc, u32 phy_type)
{
	if (phy_type == DSI_PHY_TYPE_CPHY) {
		timing->lane_v4[0] = 0x00;
		timing->lane_v4[1] = 0x00;
		timing->lane_v4[2] = 0x00;
		timing->lane_v4[3] = 0x00;
		timing->lane_v4[4] = desc->hs_exit.reg_value;
		timing->lane_v4[5] = desc->clk_pre.reg_value;
		timing->lane_v4[6] = desc->clk_prepare.reg_value;
		timing->lane_v4[7] = desc->clk_post.reg_value;
		timing->lane_v4[8] = desc->hs_rqst.reg_value;
		timing->lane_v4[9] = 0x02;
		timing->lane_v4[10] = 0x04;
		timing->lane_v4[11] = 0x00;
	} else {
		timing->lane_v4[0] = 0x00;
		timing->lane_v4[1] = desc->clk_zero.reg_value;
		timing->lane_v4[2] = desc->clk_prepare.reg_value;
		timing->lane_v4[3] = desc->clk_trail.reg_value;
		timing->lane_v4[4] = desc->hs_exit.reg_value;
		timing->lane_v4[5] = desc->hs_zero.reg_value;
		timing->lane_v4[6] = desc->hs_prepare.reg_value;
		timing->lane_v4[7] = desc->hs_trail.reg_value;
		timing->lane_v4[8] = desc->hs_rqst.reg_value;
		timing->lane_v4[9] = 0x02;
		timing->lane_v4[10] = 0x04;
		timing->lane_v4[11] = 0x00;
		timing->lane_v4[12] = desc->clk_pre.reg_value;
		timing->lane_v4[13] = desc->clk_post.reg_value;
	}

	DSI_DEBUG("[%d %d %d %d]\n", timing->lane_v4[0],
		timing->lane_v4[1], timing->lane_v4[2], timing->lane_v4[3]);
	DSI_DEBUG("[%d %d %d %d]\n", timing->lane_v4[4],
		timing->lane_v4[5], timing->lane_v4[6], timing->lane_v4[7]);
	DSI_DEBUG("[%d %d %d %d]\n", timing->lane_v4[8],
		timing->lane_v4[9], timing->lane_v4[10], timing->lane_v4[11]);
	DSI_DEBUG("[%d %d]\n", timing->lane_v4[12], timing->lane_v4[13]);
	timing->count_per_lane = 14;
}
