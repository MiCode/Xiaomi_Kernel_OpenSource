/* Copyright (c) 2015-2016, 2018, The Linux Foundation. All rights reserved.
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

#include "mdss_dsi_phy.h"

#define ESC_CLK_MHZ 192
#define ESCCLK_MMSS_CC_PREDIV 10

#define TLPX_NUMER 1000
#define TR_EOT 20
#define TA_GO  3
#define TA_SURE 0
#define TA_GET 4

#define CLK_PREPARE_SPEC_MIN	38
#define CLK_PREPARE_SPEC_MAX	95
#define CLK_TRAIL_SPEC_MIN	60
#define HS_EXIT_SPEC_MIN	100
#define HS_EXIT_RECO_MAX	255
#define HS_RQST_SPEC_MIN	50
#define CLK_ZERO_RECO_MAX1	511
#define CLK_ZERO_RECO_MAX2	255

/* No. of timing params for phy rev 2.0 */
#define TIMING_PARAM_DLANE_COUNT	32
#define TIMING_PARAM_CLK_COUNT	8

struct timing_entry {
	int32_t mipi_min;
	int32_t mipi_max;
	int32_t rec_min;
	int32_t rec_max;
	int32_t rec;
	char program_value;
};

struct dsi_phy_timing {
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
	struct timing_entry ta_get;
	struct timing_entry clk_post;
	struct timing_entry clk_pre;
};

struct dsi_phy_t_clk_param {
	uint32_t bitclk_mbps;
	uint32_t escclk_numer;
	uint32_t escclk_denom;
	uint32_t tlpx_numer_ns;
	uint32_t treot_ns;
};

static int  mdss_dsi_phy_common_validate_and_set(struct timing_entry *te,
		char const *te_name)
{
	if (te->rec & 0xffffff00) {
		/* Output value can only be 8 bits */
		pr_err("Incorrect %s calculations - %d\n", te_name, te->rec);
		return -EINVAL;
	}
	pr_debug("%s program value=%d\n", te_name, te->rec);
	te->program_value = te->rec;
	return 0;
}

static int mdss_dsi_phy_validate_and_set(struct timing_entry *te,
		char const *te_name)
{
	if (te->rec < 0)
		te->program_value = 0;
	else
		return mdss_dsi_phy_common_validate_and_set(te, te_name);

	return 0;
}

static int mdss_dsi_phy_initialize_defaults(struct dsi_phy_t_clk_param *t_clk,
		struct dsi_phy_timing *t_param, u32 phy_rev)
{

	if (phy_rev <= DSI_PHY_REV_UNKNOWN || phy_rev >= DSI_PHY_REV_MAX) {
		pr_err("Invalid PHY %d revision\n", phy_rev);
		return -EINVAL;
	}

	t_param->clk_prepare.mipi_min = CLK_PREPARE_SPEC_MIN;
	t_param->clk_prepare.mipi_max = CLK_PREPARE_SPEC_MAX;
	t_param->clk_trail.mipi_min = CLK_TRAIL_SPEC_MIN;
	t_param->hs_exit.mipi_min = HS_EXIT_SPEC_MIN;
	t_param->hs_exit.rec_max = HS_EXIT_RECO_MAX;

	if (phy_rev == DSI_PHY_REV_20) {
		t_param->clk_prepare.rec_min =
			DIV_ROUND_UP((t_param->clk_prepare.mipi_min
						* t_clk->bitclk_mbps),
					(8 * t_clk->tlpx_numer_ns));
		t_param->clk_prepare.rec_max =
			rounddown(mult_frac(t_param->clk_prepare.mipi_max
						* t_clk->bitclk_mbps, 1,
						(8 * t_clk->tlpx_numer_ns)), 1);
		t_param->hs_rqst.mipi_min = HS_RQST_SPEC_MIN;
		t_param->hs_rqst_clk.mipi_min = HS_RQST_SPEC_MIN;
	} else if (phy_rev == DSI_PHY_REV_10) {
		t_param->clk_prepare.rec_min =
			(DIV_ROUND_UP(t_param->clk_prepare.mipi_min *
				      t_clk->bitclk_mbps,
				      t_clk->tlpx_numer_ns)) - 2;
		t_param->clk_prepare.rec_max =
			(DIV_ROUND_UP(t_param->clk_prepare.mipi_max *
				      t_clk->bitclk_mbps,
				      t_clk->tlpx_numer_ns)) - 2;
	}

	pr_debug("clk_prepare: min=%d, max=%d\n", t_param->clk_prepare.rec_min,
			t_param->clk_prepare.rec_max);

	return 0;
}

static int mdss_dsi_phy_calc_param_phy_rev_2(struct dsi_phy_t_clk_param *t_clk,
		struct dsi_phy_timing *t_param)
{
	/* recommended fraction for PHY REV 2.0 */
	u32 const min_prepare_frac = 50;
	u32 const hs_exit_min_frac = 10;
	u32 const phy_timing_frac = 30;
	u32 const hs_zero_min_frac = 10;
	u32 const clk_zero_min_frac = 2;
	int tmp;
	int t_hs_prep_actual;
	int teot_clk_lane, teot_data_lane;
	u64 dividend;
	u64 temp, rc = 0;
	u64 multiplier = BIT(20);
	u64 temp_multiple;
	s64 mipi_min, mipi_max, mipi_max_tr, rec_min, rec_prog;
	s64 clk_prep_actual;
	s64 actual_intermediate;
	s32 actual_frac;
	s64 rec_temp1, rec_temp2, rec_temp3;
	int tclk_prepare_program, dsiphy_halfbyteclk_en, tclk_zero_program;
	int ths_request_clk_prepare, hstx_prepare_delay, temp_rec_min;
	s64 tclk_prepare_theoretical, tclk_zero_theoretical;
	s64 ths_request_theoretical;

	/* clk_prepare calculations */
	dividend = ((t_param->clk_prepare.rec_max
			- t_param->clk_prepare.rec_min)
			* min_prepare_frac * multiplier);
	temp = roundup(div_s64(dividend, 100), multiplier);
	temp += (t_param->clk_prepare.rec_min * multiplier);
	t_param->clk_prepare.rec = div_s64(temp, multiplier);

	rc = mdss_dsi_phy_common_validate_and_set(&t_param->clk_prepare,
			"clk prepare");
	if (rc)
		goto error;

	/* clk_ prepare theoretical value*/
	temp_multiple = (8 * t_param->clk_prepare.program_value
			* t_clk->tlpx_numer_ns * multiplier);
	actual_intermediate = div_s64(temp_multiple, t_clk->bitclk_mbps);
	div_s64_rem(temp_multiple, t_clk->bitclk_mbps, &actual_frac);
	clk_prep_actual =
		div_s64((actual_intermediate + actual_frac), multiplier);

	pr_debug("CLK PREPARE: mipi_min=%d, max=%d, rec_min=%d, rec_max=%d",
			t_param->clk_prepare.mipi_min,
			t_param->clk_prepare.mipi_max,
			t_param->clk_prepare.rec_min,
			t_param->clk_prepare.rec_max);
	pr_debug("prog value = %d, actual=%lld\n",
			t_param->clk_prepare.rec, clk_prep_actual);

	/* clk zero calculations */
	/* Mipi spec min*/
	mipi_min = (300 * multiplier) - (actual_intermediate + actual_frac);
	t_param->clk_zero.mipi_min = div_s64(mipi_min, multiplier);

	/* recommended min */
	rec_temp1 = div_s64(mipi_min * t_clk->bitclk_mbps,
			t_clk->tlpx_numer_ns);
	rec_temp2 = rec_temp1 - (11 * multiplier);
	rec_temp3 = roundup(div_s64(rec_temp2, 8),  multiplier);
	rec_min = div_s64(rec_temp3, multiplier) - 3;
	t_param->clk_zero.rec_min = rec_min;

	/* recommended max */
	t_param->clk_zero.rec_max =
			((t_param->clk_zero.rec_min > 255) ? 511 : 255);

	/* Programmed value */
	t_param->clk_zero.rec = DIV_ROUND_UP(
			(t_param->clk_zero.rec_max - t_param->clk_zero.rec_min)
				* clk_zero_min_frac
				+ (t_param->clk_zero.rec_min * 100), 100);

	rc = mdss_dsi_phy_common_validate_and_set(&t_param->clk_zero,
			"clk zero");
	if (rc)
		goto error;

	pr_debug("CLK ZERO: mipi_min=%d, max=%d, rec_min=%d, rec_max=%d, prog value = %d\n",
			t_param->clk_zero.mipi_min, t_param->clk_zero.mipi_max,
			t_param->clk_zero.rec_min, t_param->clk_zero.rec_max,
			t_param->clk_zero.rec);

	/* clk trail calculations */
	temp_multiple = div_s64(12 * multiplier * t_clk->tlpx_numer_ns,
						t_clk->bitclk_mbps);
	div_s64_rem(temp_multiple, multiplier, &actual_frac);

	mipi_max_tr = 105 * multiplier + (temp_multiple + actual_frac);
	teot_clk_lane = div_s64(mipi_max_tr, multiplier);

	mipi_max = mipi_max_tr - (t_clk->treot_ns * multiplier);

	t_param->clk_trail.mipi_max =  div_s64(mipi_max, multiplier);

	/* recommended min*/
	temp_multiple = div_s64(t_param->clk_trail.mipi_min * multiplier *
			t_clk->bitclk_mbps, t_clk->tlpx_numer_ns);
	div_s64_rem(temp_multiple, multiplier, &actual_frac);
	rec_temp1 = temp_multiple + actual_frac + 3 * multiplier;
	rec_temp2 = div_s64(rec_temp1, 8);
	rec_temp3 = roundup(rec_temp2, multiplier);

	t_param->clk_trail.rec_min = div_s64(rec_temp3, multiplier);

	/* recommended max */
	rec_temp1 = div_s64(mipi_max * t_clk->bitclk_mbps,
						t_clk->tlpx_numer_ns);
	rec_temp2 = rec_temp1 + 3 * multiplier;
	rec_temp3 = rec_temp2 / 8;
	t_param->clk_trail.rec_max = div_s64(rec_temp3, multiplier);

	/* Programmed value */
	t_param->clk_trail.rec = DIV_ROUND_UP(
		(t_param->clk_trail.rec_max - t_param->clk_trail.rec_min)
			* phy_timing_frac
			+ (t_param->clk_trail.rec_min * 100), 100);

	rc = mdss_dsi_phy_common_validate_and_set(&t_param->clk_trail,
			"clk trail");
	if (rc)
		goto error;

	pr_debug("CLK TRAIL: mipi_min=%d, max=%d, rec_min=%d, rec_max=%d, prog value = %d\n",
			t_param->clk_trail.mipi_min,
			t_param->clk_trail.mipi_max,
			t_param->clk_trail.rec_min,
			t_param->clk_trail.rec_max,
			t_param->clk_trail.rec);

	/* hs prepare calculations */
	/* mipi min */
	temp_multiple = div_s64(4 * t_clk->tlpx_numer_ns * multiplier,
			t_clk->bitclk_mbps);
	div_s64_rem(temp_multiple, multiplier, &actual_frac);
	mipi_min = 40 * multiplier + (temp_multiple + actual_frac);
	t_param->hs_prepare.mipi_min = div_s64(mipi_min, multiplier);

	/* mipi max */
	temp_multiple = div_s64(6 * t_clk->tlpx_numer_ns * multiplier,
			t_clk->bitclk_mbps);
	div_s64_rem(temp_multiple, multiplier, &actual_frac);
	mipi_max = 85 * multiplier + temp_multiple;
	t_param->hs_prepare.mipi_max = div_s64(mipi_max, multiplier);

	/* recommended min */
	temp_multiple = div_s64(mipi_min * t_clk->bitclk_mbps,
			t_clk->tlpx_numer_ns);
	div_s64_rem(temp_multiple, multiplier, &actual_frac);
	rec_temp1 = roundup((temp_multiple + actual_frac)/8, multiplier);
	t_param->hs_prepare.rec_min = div_s64(rec_temp1, multiplier);

	/* recommended max*/
	temp_multiple = div_s64(mipi_max * t_clk->bitclk_mbps,
			t_clk->tlpx_numer_ns);
	div_s64_rem(temp_multiple, multiplier, &actual_frac);
	rec_temp2 = rounddown((temp_multiple + actual_frac)/8, multiplier);
	t_param->hs_prepare.rec_max = div_s64(rec_temp2, multiplier);

	/* prog value*/
	dividend = (rec_temp2 - rec_temp1) * min_prepare_frac;
	temp = roundup(div_u64(dividend, 100), multiplier);
	rec_prog = temp + rec_temp1;
	t_param->hs_prepare.rec = div_s64(rec_prog, multiplier);

	rc = mdss_dsi_phy_common_validate_and_set(&t_param->hs_prepare,
			"HS prepare");
	if (rc)
		goto error;

	/* theoretical Value */
	temp_multiple = div_s64(8 * rec_prog * t_clk->tlpx_numer_ns,
			t_clk->bitclk_mbps);
	div_s64_rem(temp_multiple, multiplier, &actual_frac);
	t_hs_prep_actual = div_s64(temp_multiple, multiplier);
	pr_debug("HS PREPARE: mipi_min=%d, max=%d, rec_min=%d, rec_max=%d, prog value = %d, actual=%d\n",
			t_param->hs_prepare.mipi_min,
			t_param->hs_prepare.mipi_max,
			t_param->hs_prepare.rec_min,
			t_param->hs_prepare.rec_max,
			t_param->hs_prepare.rec, t_hs_prep_actual);

	/* hs zero calculations */
	/* mipi min*/
	mipi_min = div_s64(10 * t_clk->tlpx_numer_ns * multiplier,
			t_clk->bitclk_mbps);
	rec_temp1 = (145 * multiplier) + mipi_min - temp_multiple;
	t_param->hs_zero.mipi_min = div_s64(rec_temp1, multiplier);

	/* recommended min */
	rec_temp1 = div_s64(rec_temp1 * t_clk->bitclk_mbps,
			t_clk->tlpx_numer_ns);
	rec_temp2 = rec_temp1 - (11 * multiplier);
	rec_temp3 = roundup((rec_temp2/8), multiplier);
	rec_min = rec_temp3 - (3 * multiplier);
	t_param->hs_zero.rec_min = div_s64(rec_min, multiplier);

	t_param->hs_zero.rec_max =
			((t_param->hs_zero.rec_min > 255) ? 511 : 255);

	/* prog value */
	t_param->hs_zero.rec = DIV_ROUND_UP(
		(t_param->hs_zero.rec_max - t_param->hs_zero.rec_min)
		* hs_zero_min_frac + (t_param->hs_zero.rec_min * 100),
		100);

	rc = mdss_dsi_phy_common_validate_and_set(&t_param->hs_zero, "HS zero");
	if (rc)
		goto error;

	pr_debug("HS ZERO: mipi_min=%d, max=%d, rec_min=%d, rec_max=%d, prog value = %d\n",
			t_param->hs_zero.mipi_min, t_param->hs_zero.mipi_max,
			t_param->hs_zero.rec_min, t_param->hs_zero.rec_max,
			t_param->hs_zero.rec);

	/* hs_trail calculations */
	teot_data_lane  = teot_clk_lane;
	t_param->hs_trail.mipi_min =  60 +
		mult_frac(t_clk->tlpx_numer_ns, 4, t_clk->bitclk_mbps);
	t_param->hs_trail.mipi_max =  teot_clk_lane - t_clk->treot_ns;
	t_param->hs_trail.rec_min = DIV_ROUND_UP(
		((t_param->hs_trail.mipi_min * t_clk->bitclk_mbps)
		 + 3 * t_clk->tlpx_numer_ns), (8 * t_clk->tlpx_numer_ns));
	tmp = ((t_param->hs_trail.mipi_max * t_clk->bitclk_mbps)
		 + (3 * t_clk->tlpx_numer_ns));
	t_param->hs_trail.rec_max = tmp/(8 * t_clk->tlpx_numer_ns);
	tmp = DIV_ROUND_UP((t_param->hs_trail.rec_max
			- t_param->hs_trail.rec_min) * phy_timing_frac,
			100);
	t_param->hs_trail.rec = tmp + t_param->hs_trail.rec_min;


	rc = mdss_dsi_phy_common_validate_and_set(&t_param->hs_trail,
			"HS trail");
	if (rc)
		goto error;

	pr_debug("HS TRAIL: mipi_min=%d, max=%d, rec_min=%d, rec_max=%d, prog value = %d\n",
			t_param->hs_trail.mipi_min, t_param->hs_trail.mipi_max,
			t_param->hs_trail.rec_min, t_param->hs_trail.rec_max,
			t_param->hs_trail.rec);

	/* hs rqst calculations for Data lane */
	t_param->hs_rqst.rec = DIV_ROUND_UP(
		(t_param->hs_rqst.mipi_min * t_clk->bitclk_mbps)
		- (8 * t_clk->tlpx_numer_ns), (8 * t_clk->tlpx_numer_ns));

	rc = mdss_dsi_phy_common_validate_and_set(&t_param->hs_rqst, "HS rqst");
	if (rc)
		goto error;

	pr_debug("HS RQST-DATA: mipi_min=%d, max=%d, rec_min=%d, rec_max=%d, prog value = %d\n",
			t_param->hs_rqst.mipi_min, t_param->hs_rqst.mipi_max,
			t_param->hs_rqst.rec_min, t_param->hs_rqst.rec_max,
			t_param->hs_rqst.rec);

	/* hs exit calculations */
	t_param->hs_exit.rec_min = DIV_ROUND_UP(
		(t_param->hs_exit.mipi_min * t_clk->bitclk_mbps),
		(8 * t_clk->tlpx_numer_ns)) - 1;
	t_param->hs_exit.rec = DIV_ROUND_UP(
		(t_param->hs_exit.rec_max - t_param->hs_exit.rec_min)
			* hs_exit_min_frac
			+ (t_param->hs_exit.rec_min * 100), 100);

	rc = mdss_dsi_phy_common_validate_and_set(&t_param->hs_exit, "HS exit");
	if (rc)
		goto error;

	pr_debug("HS EXIT: mipi_min=%d, max=%d, rec_min=%d, rec_max=%d, prog value = %d\n",
			t_param->hs_exit.mipi_min, t_param->hs_exit.mipi_max,
			t_param->hs_exit.rec_min, t_param->hs_exit.rec_max,
			t_param->hs_exit.rec);

	/* hs rqst calculations for Clock lane */
	t_param->hs_rqst_clk.rec = DIV_ROUND_UP(
		(t_param->hs_rqst_clk.mipi_min * t_clk->bitclk_mbps)
		- (8 * t_clk->tlpx_numer_ns), (8 * t_clk->tlpx_numer_ns));

	rc = mdss_dsi_phy_common_validate_and_set(&t_param->hs_rqst_clk,
			"HS rqst clk");
	if (rc)
		goto error;

	pr_debug("HS RQST-CLK: mipi_min=%d, max=%d, rec_min=%d, rec_max=%d, prog value = %d\n",
			t_param->hs_rqst_clk.mipi_min,
			t_param->hs_rqst_clk.mipi_max,
			t_param->hs_rqst_clk.rec_min,
			t_param->hs_rqst_clk.rec_max,
			t_param->hs_rqst_clk.rec);

	/* clk post and pre value calculation */
	tmp = ((60 * (int)t_clk->bitclk_mbps) + (52 * 1000) - (43 * 1000));

	/* clk_post minimum value can be a negetive number */
	if (tmp % (8 * 1000) != 0) {
		if (tmp < 0)
			tmp = (tmp / (8 * 1000))  - 1;
		else
			tmp = (tmp / (8 * 1000)) + 1;
	} else {
		tmp = tmp / (8 * 1000);
	}
	tmp = tmp - 1;

	t_param->clk_post.program_value =
		DIV_ROUND_UP((63 - tmp) * hs_exit_min_frac, 100);
	t_param->clk_post.program_value += tmp;

	if (t_param->clk_post.program_value & 0xffffff00) {
		pr_err("Invalid clk post calculations - %d\n",
			   t_param->clk_post.program_value);
		goto error;
	}

	t_param->clk_post.rec_min = tmp;

	if (t_param->hs_rqst_clk.rec < 0)
		ths_request_clk_prepare = 0;
	else
		ths_request_clk_prepare = t_param->hs_rqst_clk.program_value;

	ths_request_theoretical = (ths_request_clk_prepare + 1);

	tclk_prepare_program = t_param->clk_prepare.program_value;

	dsiphy_halfbyteclk_en = 0;

	if (t_clk->bitclk_mbps > 100)
		hstx_prepare_delay = 0;
	else
		hstx_prepare_delay = 3;

	tclk_prepare_theoretical = ((tclk_prepare_program * 8)
					+ (dsiphy_halfbyteclk_en * 4)
					+ (hstx_prepare_delay * 2));

	tclk_zero_program = t_param->clk_zero.program_value;

	tclk_zero_theoretical = ((tclk_zero_program + 3) * 8) + 11
						- (hstx_prepare_delay * 2);

	temp_rec_min = (8 * 1000) + (tclk_prepare_theoretical * 1000)
			+ (tclk_zero_theoretical * 1000)
			+ (ths_request_theoretical * 8 * 1000);

	t_param->clk_pre.rec_min = DIV_ROUND_UP(temp_rec_min, 8 * 1000) - 1;

	if (t_param->clk_pre.rec_min > 63) {
		t_param->clk_pre.program_value =
			DIV_ROUND_UP((2 * 63 - t_param->clk_pre.rec_min)
						* hs_exit_min_frac, 100);
		t_param->clk_pre.program_value += t_param->clk_pre.rec_min;
	} else {
		t_param->clk_pre.program_value =
			DIV_ROUND_UP((63 - t_param->clk_pre.rec_min)
						* hs_exit_min_frac, 100);
		t_param->clk_pre.program_value += t_param->clk_pre.rec_min;
	}

	if (t_param->clk_pre.program_value & 0xffffff00) {
		pr_err("Invalid clk pre calculations - %d\n",
				t_param->clk_pre.program_value);
		goto error;
	}
	pr_debug("t_clk_post: %d t_clk_pre: %d\n",
			t_param->clk_post.program_value,
			t_param->clk_pre.program_value);

	pr_debug("teot_clk=%d, data=%d\n", teot_clk_lane, teot_data_lane);
	return 0;

error:
	return -EINVAL;
}

static int mdss_dsi_phy_calc_hs_param_phy_rev_1(
		struct dsi_phy_t_clk_param *t_clk,
		struct dsi_phy_timing *t_param)
{
	int percent_min = 10;
	int percent_allowable_phy = 0;
	int percent_min_ths;
	int tmp, rc = 0;
	int tclk_prepare_theoretical, tclk_zero_theoretical;
	int tlpx, ths_exit_theoretical;

	if (t_clk->bitclk_mbps > 1200)
		percent_min_ths = 15;
	else
		percent_min_ths = 10;

	if (t_clk->bitclk_mbps > 180)
		percent_allowable_phy = 10;
	else
		percent_allowable_phy = 40;

	t_param->hs_prepare.rec_min =
		DIV_ROUND_UP((40 * t_clk->bitclk_mbps)
		+ (4 * t_clk->tlpx_numer_ns), t_clk->tlpx_numer_ns) - 2;
	t_param->hs_prepare.rec_max =
		DIV_ROUND_UP((85 * t_clk->bitclk_mbps)
		+ (6 * t_clk->tlpx_numer_ns), t_clk->tlpx_numer_ns) - 2;
	tmp = DIV_ROUND_UP((t_param->hs_prepare.rec_max
		- t_param->hs_prepare.rec_min) * percent_min_ths, 100);
	tmp += t_param->hs_prepare.rec_min;
	t_param->hs_prepare.rec = (tmp & ~0x1);

	rc = mdss_dsi_phy_validate_and_set(&t_param->hs_prepare, "HS prepare");
	if (rc)
		goto error;

	tmp = (t_param->hs_prepare.program_value / 2) + 1;
	t_param->hs_zero.rec_min = DIV_ROUND_UP((145 * t_clk->bitclk_mbps)
		+ ((10 - (2 * (tmp + 1))) * 1000), 1000) - 2;
	t_param->hs_zero.rec_max = 255;
	tmp = DIV_ROUND_UP((t_param->hs_zero.rec_max
		- t_param->hs_zero.rec_min) * percent_min, 100);
	tmp += t_param->hs_zero.rec_min;
	t_param->hs_zero.rec = (tmp & ~0x1);

	rc = mdss_dsi_phy_validate_and_set(&t_param->hs_zero, "HS zero");
	if (rc)
		goto error;

	t_param->hs_trail.rec_min = DIV_ROUND_UP((60 * t_clk->bitclk_mbps)
		+ 4000, 1000) - 2;
	t_param->hs_trail.rec_max = DIV_ROUND_UP((105 - t_clk->treot_ns)
		* t_clk->bitclk_mbps + 12000, 1000) - 2;
	tmp = DIV_ROUND_UP((t_param->hs_trail.rec_max
		- t_param->hs_trail.rec_min) * percent_allowable_phy, 100);
	tmp += t_param->hs_trail.rec_min;
	t_param->hs_trail.rec = tmp & ~0x1;

	rc = mdss_dsi_phy_validate_and_set(&t_param->hs_trail, "HS trail");
	if (rc)
		goto error;

	t_param->hs_exit.rec_min = DIV_ROUND_UP(100 * t_clk->bitclk_mbps,
		t_clk->tlpx_numer_ns) - 2;
	t_param->hs_exit.rec_max = 255;
	tmp = DIV_ROUND_UP((t_param->hs_exit.rec_max
		- t_param->hs_exit.rec_min) * percent_min, 100);
	tmp += t_param->hs_exit.rec_min;
	t_param->hs_exit.rec = (tmp & ~0x1);

	rc = mdss_dsi_phy_validate_and_set(&t_param->hs_exit, "HS exit");
	if (rc)
		goto error;

	/* clk post and pre value calculation */
	ths_exit_theoretical = (t_param->hs_exit.program_value / 2) + 1;
	tmp = ((60 * (int)t_clk->bitclk_mbps) + (52 * 1000)
			- (24 * 1000) - (ths_exit_theoretical * 2 * 1000));
	/* clk_post minimum value can be a negetive number */
	if (tmp % (8 * 1000) != 0) {
		if (tmp < 0)
			tmp = (tmp / (8 * 1000))  - 1;
		else
			tmp = (tmp / (8 * 1000)) + 1;
	} else {
		tmp = tmp / (8 * 1000);
	}
	tmp = tmp - 1;

	t_param->clk_post.program_value =
		DIV_ROUND_UP((63 - tmp) * percent_min, 100);
	t_param->clk_post.program_value += tmp;

	if (t_param->clk_post.program_value & 0xffffff00) {
		pr_err("Invalid clk post calculations - %d\n",
				t_param->clk_post.program_value);
		goto error;
	}

	t_param->clk_post.rec_min = tmp;

	tclk_prepare_theoretical = (t_param->clk_prepare.program_value / 2) + 1;
	tclk_zero_theoretical = (t_param->clk_zero.program_value / 2) + 1;
	tlpx = 10000/t_clk->escclk_numer;

	t_param->clk_pre.rec_min =
		DIV_ROUND_UP((tlpx * t_clk->bitclk_mbps) + (8 * 1000)
			+ (tclk_prepare_theoretical * 2 * 1000)
			+ (tclk_zero_theoretical * 2 * 1000), 8 * 1000) - 1;
	if (t_param->clk_pre.rec_min > 63) {
		t_param->clk_pre.program_value =
			DIV_ROUND_UP((2 * 63 - t_param->clk_pre.rec_min)
						* percent_min, 100);
		t_param->clk_pre.program_value += t_param->clk_pre.rec_min;
	} else {
		t_param->clk_pre.program_value =
			DIV_ROUND_UP((63 - t_param->clk_pre.rec_min)
							* percent_min, 100);
		t_param->clk_pre.program_value += t_param->clk_pre.rec_min;
	}

	if (t_param->clk_pre.program_value & 0xffffff00) {
		pr_err("Invalid clk pre calculations - %d\n",
				t_param->clk_pre.program_value);
		goto error;
	}
	pr_debug("t_clk_post: %d t_clk_pre: %d\n",
			t_param->clk_post.program_value,
			t_param->clk_pre.program_value);

	return 0;

error:
	return -EINVAL;

}

static int mdss_dsi_phy_calc_param_phy_rev_1(struct dsi_phy_t_clk_param *t_clk,
		struct dsi_phy_timing *t_param)
{
	int percent_allowable_phy = 0;
	int percent_min_t_clk = 10;
	int tmp, rc = 0;
	int clk_prep_actual;
	int teot_clk_lane;
	u32 temp = 0;

	if (t_clk->bitclk_mbps > 180)
		percent_allowable_phy = 10;
	else
		percent_allowable_phy = 40;

	tmp = DIV_ROUND_UP((t_param->clk_prepare.rec_max -
		t_param->clk_prepare.rec_min) * percent_min_t_clk, 100);
	tmp += t_param->clk_prepare.rec_min;

	t_param->clk_prepare.rec = (tmp & ~0x1);

	rc = mdss_dsi_phy_common_validate_and_set(&t_param->clk_prepare,
			"clk prepare");
	if (rc)
		goto error;

	clk_prep_actual = 2 * ((t_param->clk_prepare.program_value
				/ 2) + 1) * t_clk->tlpx_numer_ns;
	clk_prep_actual /= t_clk->bitclk_mbps;

	tmp = t_clk->bitclk_mbps * t_clk->escclk_denom
		/ t_clk->escclk_numer;
	t_param->hs_rqst.rec = tmp;
	if (!(tmp & 0x1))
		t_param->hs_rqst.rec -= 2;

	rc = mdss_dsi_phy_common_validate_and_set(&t_param->hs_rqst, "HS rqst");
	if (rc)
		goto error;

	if (t_param->hs_rqst.program_value < 0)
		t_param->hs_rqst.program_value = 0;

	/* t_clk_zero calculation */
	t_param->clk_zero.mipi_min = (300 - clk_prep_actual);
	t_param->clk_zero.rec_min = (DIV_ROUND_UP(t_param->clk_zero.mipi_min
			* t_clk->bitclk_mbps, t_clk->tlpx_numer_ns)) - 2;

	if (t_param->clk_zero.rec_min > 255) {
		t_param->clk_zero.rec_max = CLK_ZERO_RECO_MAX1;
		t_param->clk_zero.rec =
			DIV_ROUND_UP(t_param->clk_zero.rec_min * 10
				+ (t_param->clk_zero.rec_min * 100), 100);
	} else {
		t_param->clk_zero.rec_max = CLK_ZERO_RECO_MAX2;
		temp = t_param->clk_zero.rec_max - t_param->clk_zero.rec_min;
		t_param->clk_zero.rec = DIV_ROUND_UP(temp * 10
				+ (t_param->clk_zero.rec_min * 100), 100);
	}

	t_param->clk_zero.rec &= ~0x1;

	if (((t_param->hs_rqst.rec + t_param->clk_zero.rec +
					t_param->clk_prepare.rec) % 8) != 0)
		t_param->clk_zero.rec +=
			(8 - ((t_param->hs_rqst.rec + t_param->clk_zero.rec +
			       t_param->clk_prepare.rec) % 8));

	rc = mdss_dsi_phy_common_validate_and_set(&t_param->clk_zero,
			"clk zero");
	if (rc)
		goto error;

	pr_debug("hs_rqst.rec: %d clk_zero.rec: %d clk_prepare.rec: %d\n",
				t_param->hs_rqst.rec, t_param->clk_zero.rec,
				t_param->clk_prepare.rec);
	teot_clk_lane  = 105 + (12 * t_clk->tlpx_numer_ns
		/ t_clk->bitclk_mbps);
	t_param->clk_trail.mipi_max = teot_clk_lane - t_clk->treot_ns;
	t_param->clk_trail.rec_min = DIV_ROUND_UP(t_param->clk_trail.mipi_min *
		t_clk->bitclk_mbps, t_clk->tlpx_numer_ns) - 2;
	t_param->clk_trail.rec_max = DIV_ROUND_UP(t_param->clk_trail.mipi_max *
		t_clk->bitclk_mbps, t_clk->tlpx_numer_ns) - 2;

	tmp = DIV_ROUND_UP((t_param->clk_trail.rec_max -
		t_param->clk_trail.rec_min) * percent_allowable_phy, 100);
	tmp += t_param->clk_trail.rec_min;
	t_param->clk_trail.rec = (tmp & ~0x1);

	rc = mdss_dsi_phy_validate_and_set(&t_param->clk_trail, "clk trail");
	if (rc)
		goto error;

	rc = mdss_dsi_phy_calc_hs_param_phy_rev_1(t_clk, t_param);
	if (rc)
		pr_err("Invalid HS param calculations\n");

error:
	return rc;
}

static void mdss_dsi_phy_update_timing_param(struct mdss_panel_info *pinfo,
		struct dsi_phy_timing *t_param)
{
	struct mdss_dsi_phy_ctrl *reg;

	reg = &(pinfo->mipi.dsi_phy_db);

	pinfo->mipi.t_clk_post = t_param->clk_post.program_value;
	pinfo->mipi.t_clk_pre = t_param->clk_pre.program_value;

	if (t_param->clk_zero.rec > 255) {
		reg->timing[0] = t_param->clk_zero.program_value - 255;
		reg->timing[3] = 1;
	} else {
		reg->timing[0] = t_param->clk_zero.program_value;
		reg->timing[3] = 0;
	}
	reg->timing[1] = t_param->clk_trail.program_value;
	reg->timing[2] = t_param->clk_prepare.program_value;
	reg->timing[4] = t_param->hs_exit.program_value;
	reg->timing[5] = t_param->hs_zero.program_value;
	reg->timing[6] = t_param->hs_prepare.program_value;
	reg->timing[7] = t_param->hs_trail.program_value;
	reg->timing[8] = t_param->hs_rqst.program_value;
	reg->timing[9] = (TA_SURE << 16) + TA_GO;
	reg->timing[10] = TA_GET;
	reg->timing[11] = 0;

	pr_debug("[%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x]\n",
		reg->timing[0], reg->timing[1], reg->timing[2], reg->timing[3],
		reg->timing[4], reg->timing[5], reg->timing[6], reg->timing[7],
		reg->timing[8], reg->timing[9], reg->timing[10],
		reg->timing[11]);
}

static void mdss_dsi_phy_update_timing_param_rev_2(
		struct mdss_panel_info *pinfo,
		struct dsi_phy_timing *t_param)
{
	struct mdss_dsi_phy_ctrl *reg;
	int i = 0;

	reg = &(pinfo->mipi.dsi_phy_db);

	pinfo->mipi.t_clk_post = t_param->clk_post.program_value;
	pinfo->mipi.t_clk_pre = t_param->clk_pre.program_value;

	for (i = 0; i < TIMING_PARAM_DLANE_COUNT; i += 8) {
		reg->timing_8996[i] = t_param->hs_exit.program_value;
		reg->timing_8996[i + 1] = t_param->hs_zero.program_value;
		reg->timing_8996[i + 2] = t_param->hs_prepare.program_value;
		reg->timing_8996[i + 3] = t_param->hs_trail.program_value;
		reg->timing_8996[i + 4] = t_param->hs_rqst.program_value;
		reg->timing_8996[i + 5] = 0x3;
		reg->timing_8996[i + 6] = 0x4;
		reg->timing_8996[i + 7] = 0xA0;
	}

	for (i = TIMING_PARAM_DLANE_COUNT;
			i < TIMING_PARAM_DLANE_COUNT + TIMING_PARAM_CLK_COUNT;
			i += 8) {
		reg->timing_8996[i] = t_param->hs_exit.program_value;
		reg->timing_8996[i + 1] = t_param->clk_zero.program_value;
		reg->timing_8996[i + 2] = t_param->clk_prepare.program_value;
		reg->timing_8996[i + 3] = t_param->clk_trail.program_value;
		reg->timing_8996[i + 4] = t_param->hs_rqst_clk.program_value;
		reg->timing_8996[i + 5] = 0x3;
		reg->timing_8996[i + 6] = 0x4;
		reg->timing_8996[i + 7] = 0xA0;
	}
}

int mdss_dsi_phy_calc_timing_param(struct mdss_panel_info *pinfo, u32 phy_rev,
		u32 frate_hz)
{
	struct dsi_phy_t_clk_param t_clk;
	struct dsi_phy_timing t_param;
	int hsync_period;
	int vsync_period;
	unsigned long inter_num;
	uint32_t lane_config = 0;
	unsigned long x, y;
	int rc = 0;

	if (!pinfo) {
		pr_err("invalid panel info\n");
		return -EINVAL;
	}

	hsync_period = mdss_panel_get_htotal(pinfo, true);
	vsync_period = mdss_panel_get_vtotal(pinfo);

	inter_num = pinfo->bpp * frate_hz;

	if (pinfo->mipi.data_lane0)
		lane_config++;
	if (pinfo->mipi.data_lane1)
		lane_config++;
	if (pinfo->mipi.data_lane2)
		lane_config++;
	if (pinfo->mipi.data_lane3)
		lane_config++;

	x = mult_frac(vsync_period * hsync_period, inter_num, lane_config);
	y = rounddown(x, 1);
	t_clk.bitclk_mbps = rounddown(mult_frac(y, 1, 1000000), 1);
	t_clk.escclk_numer = ESC_CLK_MHZ;
	t_clk.escclk_denom = ESCCLK_MMSS_CC_PREDIV;
	t_clk.tlpx_numer_ns = TLPX_NUMER;
	t_clk.treot_ns = TR_EOT;
	pr_debug("hperiod=%d, vperiod=%d, inter_num=%lu, lane_cfg=%d\n",
			hsync_period, vsync_period, inter_num, lane_config);
	pr_debug("x=%lu, y=%lu, bitrate=%d\n", x, y, t_clk.bitclk_mbps);

	switch (phy_rev) {
	case DSI_PHY_REV_10:
		rc = mdss_dsi_phy_initialize_defaults(&t_clk, &t_param,
				phy_rev);
		if (rc) {
			pr_err("phy%d initialization failed\n", phy_rev);
			goto timing_calc_end;
		}
			mdss_dsi_phy_calc_param_phy_rev_1(&t_clk, &t_param);
		mdss_dsi_phy_update_timing_param(pinfo, &t_param);
		break;
	case DSI_PHY_REV_20:
		rc = mdss_dsi_phy_initialize_defaults(&t_clk, &t_param,
				phy_rev);
		if (rc) {
			pr_err("phy%d initialization failed\n", phy_rev);
			goto timing_calc_end;
		}

		rc = mdss_dsi_phy_calc_param_phy_rev_2(&t_clk, &t_param);
		if (rc) {
			pr_err("Phy timing calculations failed\n");
			goto timing_calc_end;
		}
		mdss_dsi_phy_update_timing_param_rev_2(pinfo, &t_param);
		break;
	default:
		pr_err("phy rev %d not supported\n", phy_rev);
		return -EINVAL;
	}

timing_calc_end:
	return rc;
}
