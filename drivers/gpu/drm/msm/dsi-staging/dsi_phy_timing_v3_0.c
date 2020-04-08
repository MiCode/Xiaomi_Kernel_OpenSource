/*
 * Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "dsi-phy-timing:" fmt
#include "dsi_phy_timing_calc.h"

void dsi_phy_hw_v3_0_get_default_phy_params(
		struct phy_clk_params *params, bool is_cphy)
{
	if (is_cphy) {
		params->clk_prep_buf = 50;
		params->clk_pre_buf = 10;
		params->clk_post_buf = 10;
		params->hs_rqst_buf = 0;
		params->hs_exit_buf = 10;
	} else {
		params->clk_prep_buf = 0;
		params->clk_zero_buf = 0;
		params->clk_trail_buf = 0;
		params->hs_prep_buf = 0;
		params->hs_zero_buf = 0;
		params->hs_trail_buf = 0;
		params->hs_rqst_buf = 0;
		params->hs_exit_buf = 0;
	}
}

int32_t dsi_phy_hw_v3_0_calc_clk_zero(s64 rec_temp1, s64 mult)
{
	s64 rec_temp2, rec_temp3;

	rec_temp2 = (rec_temp1 - mult);
	rec_temp3 = roundup64(div_s64(rec_temp2, 8), mult);
	return (div_s64(rec_temp3, mult) - 1);
}

int32_t dsi_phy_hw_v3_0_calc_clk_trail_rec_min(s64 temp_mul,
		s64 frac, s64 mult)
{
	s64 rec_temp1, rec_temp2, rec_temp3;

	rec_temp1 = temp_mul + frac;
	rec_temp2 = div_s64(rec_temp1, 8);
	rec_temp3 = roundup64(rec_temp2, mult);
	return (div_s64(rec_temp3, mult) - 1);
}

int32_t dsi_phy_hw_v3_0_calc_clk_trail_rec_max(s64 temp1, s64 mult)
{
	s64 rec_temp2;

	rec_temp2 = temp1 / 8;
	return (div_s64(rec_temp2, mult) - 1);
}

int32_t dsi_phy_hw_v3_0_calc_hs_zero(s64 temp1, s64 mult)
{
	s64 rec_temp2, rec_min;

	rec_temp2 = roundup64((temp1 / 8), mult);
	rec_min = rec_temp2 - (1 * mult);
	return div_s64(rec_min, mult);
}

void dsi_phy_hw_v3_0_calc_hs_trail(struct phy_clk_params *clk_params,
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

void dsi_phy_hw_v3_0_update_timing_params(
	struct dsi_phy_per_lane_cfgs *timing,
	struct phy_timing_desc *desc, bool is_cphy)
{
	if (is_cphy) {
		timing->lane_v3[0] = 0x00;
		timing->lane_v3[1] = 0x00;
		timing->lane_v3[2] = 0x00;
		timing->lane_v3[3] = 0x00;
		timing->lane_v3[4] = desc->hs_exit.reg_value;
		timing->lane_v3[5] = desc->clk_pre.reg_value;
		timing->lane_v3[6] = desc->clk_prepare.reg_value;
		timing->lane_v3[7] = desc->clk_post.reg_value;
		timing->lane_v3[8] = desc->hs_rqst.reg_value;
		timing->lane_v3[9] = 0x02;
		timing->lane_v3[10] = 0x04;
		timing->lane_v3[11] = 0x00;
	} else {
		timing->lane_v3[0] = 0x00;
		timing->lane_v3[1] = desc->clk_zero.reg_value;
		timing->lane_v3[2] = desc->clk_prepare.reg_value;
		timing->lane_v3[3] = desc->clk_trail.reg_value;
		timing->lane_v3[4] = desc->hs_exit.reg_value;
		timing->lane_v3[5] = desc->hs_zero.reg_value;
		timing->lane_v3[6] = desc->hs_prepare.reg_value;
		timing->lane_v3[7] = desc->hs_trail.reg_value;
		timing->lane_v3[8] = desc->hs_rqst.reg_value;
		timing->lane_v3[9] = 0x02;
		timing->lane_v3[10] = 0x04;
		timing->lane_v3[11] = 0x00;
	}

	pr_debug("[%d %d %d %d]\n", timing->lane_v3[0],
		timing->lane_v3[1], timing->lane_v3[2], timing->lane_v3[3]);
	pr_debug("[%d %d %d %d]\n", timing->lane_v3[4],
		timing->lane_v3[5], timing->lane_v3[6], timing->lane_v3[7]);
	pr_debug("[%d %d %d %d]\n", timing->lane_v3[8],
		timing->lane_v3[9], timing->lane_v3[10], timing->lane_v3[11]);
	timing->count_per_lane = 12;
}

