// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
 */

#include "dsi_phy_timing_calc.h"

void dsi_phy_hw_v2_0_get_default_phy_params(struct phy_clk_params *params,
					    u32 phy_type)
{
	params->clk_prep_buf = 50;
	params->clk_zero_buf = 2;
	params->clk_trail_buf = 30;
	params->hs_prep_buf = 50;
	params->hs_zero_buf = 10;
	params->hs_trail_buf = 30;
	params->hs_rqst_buf = 0;
	params->hs_exit_buf = 10;
}

int32_t dsi_phy_hw_v2_0_calc_clk_zero(s64 rec_temp1, s64 mult)
{
	s64 rec_temp2, rec_temp3;

	rec_temp2 = (rec_temp1 - (11 * mult));
	rec_temp3 = roundup64(div_s64(rec_temp2, 8), mult);
	return (div_s64(rec_temp3, mult) - 3);
}

int32_t dsi_phy_hw_v2_0_calc_clk_trail_rec_min(s64 temp_mul,
		s64 frac, s64 mult)
{
	s64 rec_temp1, rec_temp2, rec_temp3;

	rec_temp1 = temp_mul + frac + (3 * mult);
	rec_temp2 = div_s64(rec_temp1, 8);
	rec_temp3 = roundup64(rec_temp2, mult);

	return div_s64(rec_temp3, mult);
}

int32_t dsi_phy_hw_v2_0_calc_clk_trail_rec_max(s64 temp1, s64 mult)
{
	s64 rec_temp2, rec_temp3;

	rec_temp2 = temp1 + (3 * mult);
	rec_temp3 = rec_temp2 / 8;
	return div_s64(rec_temp3, mult);

}

int32_t dsi_phy_hw_v2_0_calc_hs_zero(s64 temp1, s64 mult)
{
	s64 rec_temp2, rec_temp3, rec_min;

	rec_temp2 = temp1 - (11 * mult);
	rec_temp3 = roundup64((rec_temp2 / 8), mult);
	rec_min = rec_temp3 - (3 * mult);
	return div_s64(rec_min, mult);
}

void dsi_phy_hw_v2_0_calc_hs_trail(struct phy_clk_params *clk_params,
			struct phy_timing_desc *desc)
{
	s64 rec_temp1;
	struct timing_entry *t = &desc->hs_trail;

	t->rec_min = DIV_ROUND_UP(
		((t->mipi_min * clk_params->bitclk_mbps) +
		 (3 * clk_params->tlpx_numer_ns)),
		(8 * clk_params->tlpx_numer_ns));

	rec_temp1 = ((t->mipi_max * clk_params->bitclk_mbps) +
		     (3 * clk_params->tlpx_numer_ns));
	t->rec_max = DIV_ROUND_UP_ULL(rec_temp1,
				      (8 * clk_params->tlpx_numer_ns));
}

void dsi_phy_hw_v2_0_update_timing_params(
	struct dsi_phy_per_lane_cfgs *timing,
	struct phy_timing_desc *desc, u32 phy_type)
{
	int i = 0;

	for (i = DSI_LOGICAL_LANE_0; i < DSI_LANE_MAX; i++) {
		timing->lane[i][0] = desc->hs_exit.reg_value;

		if (i == DSI_LOGICAL_CLOCK_LANE)
			timing->lane[i][1] = desc->clk_zero.reg_value;
		else
			timing->lane[i][1] = desc->hs_zero.reg_value;

		if (i == DSI_LOGICAL_CLOCK_LANE)
			timing->lane[i][2] = desc->clk_prepare.reg_value;
		else
			timing->lane[i][2] = desc->hs_prepare.reg_value;

		if (i == DSI_LOGICAL_CLOCK_LANE)
			timing->lane[i][3] = desc->clk_trail.reg_value;
		else
			timing->lane[i][3] = desc->hs_trail.reg_value;

		if (i == DSI_LOGICAL_CLOCK_LANE)
			timing->lane[i][4] = desc->hs_rqst_clk.reg_value;
		else
			timing->lane[i][4] = desc->hs_rqst.reg_value;

		timing->lane[i][5] = 0x2;
		timing->lane[i][6] = 0x4;
		timing->lane[i][7] = 0xA0;
		DSI_DEBUG("[%d][%d %d %d %d %d]\n", i, timing->lane[i][0],
						    timing->lane[i][1],
						    timing->lane[i][2],
						    timing->lane[i][3],
						    timing->lane[i][4]);
	}
	timing->count_per_lane = 8;
}
