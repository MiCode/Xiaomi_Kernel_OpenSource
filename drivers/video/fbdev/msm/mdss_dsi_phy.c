/* Copyright (c) 2015-2016, 2018 The Linux Foundation. All rights reserved.
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
	u32 phy_rev;
	uint32_t bitclk_mbps;
	uint32_t escclk_numer;
	uint32_t escclk_denom;
	uint32_t tlpx_numer_ns;
	uint32_t treot_ns;
	u32 clk_prep_buf;
	u32 clk_zero_buf;
	u32 clk_trail_buf;
	u32 hs_prep_buf;
	u32 hs_zero_buf;
	u32 hs_trail_buf;
	u32 hs_rqst_buf;
	u32 hs_exit_buf;
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
	if (!t_clk || !t_param)
		return -EINVAL;

	if (phy_rev <= DSI_PHY_REV_UNKNOWN || phy_rev >= DSI_PHY_REV_MAX) {
		pr_err("Invalid PHY %d revision\n", phy_rev);
		return -EINVAL;
	}

	t_param->clk_prepare.mipi_min = CLK_PREPARE_SPEC_MIN;
	t_param->clk_prepare.mipi_max = CLK_PREPARE_SPEC_MAX;
	t_param->clk_trail.mipi_min = CLK_TRAIL_SPEC_MIN;
	t_param->hs_exit.mipi_min = HS_EXIT_SPEC_MIN;
	t_param->hs_exit.rec_max = HS_EXIT_RECO_MAX;

	t_clk->phy_rev = phy_rev;
	if (phy_rev == DSI_PHY_REV_30) {
		t_param->hs_rqst.mipi_min = HS_RQST_SPEC_MIN;
		t_param->hs_rqst_clk.mipi_min = HS_RQST_SPEC_MIN;

		t_clk->clk_prep_buf = 0;
		t_clk->clk_zero_buf = 0;
		t_clk->clk_trail_buf = 0;
		t_clk->hs_prep_buf = 0;
		t_clk->hs_zero_buf = 0;
		t_clk->hs_trail_buf = 0;
		t_clk->hs_rqst_buf = 0;
		t_clk->hs_exit_buf = 0;
	} else if (phy_rev == DSI_PHY_REV_20) {
		t_param->hs_rqst.mipi_min = HS_RQST_SPEC_MIN;
		t_param->hs_rqst_clk.mipi_min = HS_RQST_SPEC_MIN;

		t_clk->clk_prep_buf = 50;
		t_clk->clk_zero_buf = 2;
		t_clk->clk_trail_buf = 30;
		t_clk->hs_prep_buf = 50;
		t_clk->hs_zero_buf = 10;
		t_clk->hs_trail_buf = 30;
		t_clk->hs_rqst_buf = 0;
		t_clk->hs_exit_buf = 10;
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

/**
 * calc_clk_prepare - calculates prepare timing params for clk lane.
 */
static int calc_clk_prepare(struct dsi_phy_t_clk_param *clk_params,
			    struct dsi_phy_timing *desc,
			    s32 *actual_frac,
			    s64 *actual_intermediate)
{
	u64 const multiplier = BIT(20);
	struct timing_entry *t = &desc->clk_prepare;
	int rc = 0;
	u64 dividend, temp, temp_multiple;
	s32 frac = 0;
	s64 intermediate;
	s64 clk_prep_actual;

	if (!clk_params || !desc || !actual_frac || !actual_intermediate) {
		rc = -EINVAL;
		goto error;
	}

	t->rec_min = DIV_ROUND_UP((t->mipi_min * clk_params->bitclk_mbps),
				(8 * clk_params->tlpx_numer_ns));
	t->rec_max = rounddown(mult_frac(t->mipi_max * clk_params->bitclk_mbps,
					 1,
					(8 * clk_params->tlpx_numer_ns)),
			       1);

	dividend = ((t->rec_max - t->rec_min) *
		    clk_params->clk_prep_buf *
		    multiplier);
	temp  = roundup(div_s64(dividend, 100), multiplier);
	temp += (t->rec_min * multiplier);
	t->rec = div_s64(temp, multiplier);

	rc = mdss_dsi_phy_common_validate_and_set(t, "clk_prepare");
	if (rc)
		goto error;

	/* calculate theoretical value */
	temp_multiple = 8 * t->program_value * clk_params->tlpx_numer_ns
			 * multiplier;
	intermediate = div_s64(temp_multiple, clk_params->bitclk_mbps);
	div_s64_rem(temp_multiple, clk_params->bitclk_mbps, &frac);
	clk_prep_actual = div_s64((intermediate + frac), multiplier);

	pr_debug("CLK_PREPARE:mipi_min=%d, mipi_max=%d, rec_min=%d, rec_max=%d",
		 t->mipi_min, t->mipi_max, t->rec_min, t->rec_max);
	pr_debug(" program_value=%d, actual=%lld\n",
		 t->program_value, clk_prep_actual);

	*actual_frac = frac;
	*actual_intermediate = intermediate;

error:
	return rc;
}

/**
 * calc_clk_zero - calculates zero timing params for clk lane.
 */
static int calc_clk_zero(struct dsi_phy_t_clk_param *clk_params,
			 struct dsi_phy_timing *desc,
			 s32 actual_frac,
			 s64 actual_intermediate)
{
	u64 const multiplier = BIT(20);
	int rc = 0;
	struct timing_entry *t = &desc->clk_zero;
	s64 mipi_min, rec_temp1, rec_temp2, rec_temp3, rec_min;

	if (!clk_params || !desc || !actual_frac || !actual_intermediate) {
		rc = -EINVAL;
		goto error;
	}

	mipi_min = ((300 * multiplier) - (actual_intermediate + actual_frac));
	t->mipi_min = div_s64(mipi_min, multiplier);

	rec_temp1 = div_s64((mipi_min * clk_params->bitclk_mbps),
			    clk_params->tlpx_numer_ns);
	if (clk_params->phy_rev == DSI_PHY_REV_30) {
		rec_temp2 = (rec_temp1 - multiplier);
		rec_temp3 = roundup(div_s64(rec_temp2, 8), multiplier);
		rec_min = (div_s64(rec_temp3, multiplier) - 1);
	} else {
		rec_temp2 = (rec_temp1 - (11 * multiplier));
		rec_temp3 = roundup(div_s64(rec_temp2, 8), multiplier);
		rec_min = (div_s64(rec_temp3, multiplier) - 3);
	}
	t->rec_min = rec_min;
	t->rec_max = ((t->rec_min > 255) ? 511 : 255);

	t->rec = DIV_ROUND_UP(
			(((t->rec_max - t->rec_min) *
			  clk_params->clk_zero_buf) +
			 (t->rec_min * 100)),
			100);

	rc = mdss_dsi_phy_common_validate_and_set(t, "clk_zero");
	if (rc)
		goto error;

	pr_debug("CLK_ZERO:mipi_min=%d, mipi_max=%d, rec_min=%d, rec_max=%d, reg_val=%d\n",
		 t->mipi_min, t->mipi_max, t->rec_min, t->rec_max,
		 t->program_value);

error:
	return rc;
}

/**
 * calc_clk_trail - calculates prepare trail params for clk lane.
 */
static int calc_clk_trail(struct dsi_phy_t_clk_param *clk_params,
			  struct dsi_phy_timing *desc,
			  s64 *teot_clk_lane)
{
	u64 const multiplier = BIT(20);
	int rc = 0;
	struct timing_entry *t = &desc->clk_trail;
	u64 temp_multiple;
	s32 frac;
	s64 mipi_max_tr, rec_temp1, rec_temp2, rec_temp3, mipi_max;
	s64 teot_clk_lane1;

	if (!clk_params || !desc || !teot_clk_lane) {
		rc = -EINVAL;
		goto error;
	}

	temp_multiple = div_s64(
			(12 * multiplier * clk_params->tlpx_numer_ns),
			clk_params->bitclk_mbps);
	div_s64_rem(temp_multiple, multiplier, &frac);

	mipi_max_tr = ((105 * multiplier) +
		       (temp_multiple + frac));
	teot_clk_lane1 = div_s64(mipi_max_tr, multiplier);

	mipi_max = (mipi_max_tr - (clk_params->treot_ns * multiplier));
	t->mipi_max = div_s64(mipi_max, multiplier);

	temp_multiple = div_s64(
			(t->mipi_min * multiplier * clk_params->bitclk_mbps),
			clk_params->tlpx_numer_ns);

	div_s64_rem(temp_multiple, multiplier, &frac);
	if (clk_params->phy_rev == DSI_PHY_REV_30) {
		rec_temp1 = temp_multiple + frac;
		rec_temp2 = div_s64(rec_temp1, 8);
		rec_temp3 = roundup(rec_temp2, multiplier);
		t->rec_min = div_s64(rec_temp3, multiplier) - 1;
	} else {
		rec_temp1 = temp_multiple + frac + (3 * multiplier);
		rec_temp2 = div_s64(rec_temp1, 8);
		rec_temp3 = roundup(rec_temp2, multiplier);
		t->rec_min = div_s64(rec_temp3, multiplier);
	}


	/* recommended max */
	rec_temp1 = div_s64((mipi_max * clk_params->bitclk_mbps),
			    clk_params->tlpx_numer_ns);
	if (clk_params->phy_rev == DSI_PHY_REV_30) {
		rec_temp3 = rec_temp1 / 8;
		t->rec_max = div_s64(rec_temp3, multiplier) - 1;
	} else {
		rec_temp2 = rec_temp1 + (3 * multiplier);
		rec_temp3 = rec_temp2 / 8;
		t->rec_max = div_s64(rec_temp3, multiplier);
	}

	t->rec = DIV_ROUND_UP(
		(((t->rec_max - t->rec_min) * clk_params->clk_trail_buf) +
		 (t->rec_min * 100)),
		 100);

	rc = mdss_dsi_phy_common_validate_and_set(t, "clk_trail");
	if (rc)
		goto error;

	*teot_clk_lane = teot_clk_lane1;
	pr_debug("CLK_TRAIL:mipi_min=%d, mipi_max=%d, rec_min=%d, rec_max=%d, reg_val=%d\n",
		 t->mipi_min, t->mipi_max, t->rec_min, t->rec_max,
		 t->program_value);

error:
	return rc;
}

/**
 * calc_hs_prepare - calculates prepare timing params for data lanes in HS.
 */
static int calc_hs_prepare(struct dsi_phy_t_clk_param *clk_params,
			   struct dsi_phy_timing *desc,
			   u64 *temp_mul)
{
	u64 const multiplier = BIT(20);
	int rc = 0;
	struct timing_entry *t = &desc->hs_prepare;
	u64 temp_multiple, dividend, temp;
	s32 frac;
	s64 rec_temp1, rec_temp2, mipi_max, mipi_min;
	u32 low_clk_multiplier = 0;

	if (!clk_params || !desc || !temp_mul) {
		rc = -EINVAL;
		goto error;
	}

	if (clk_params->bitclk_mbps <= 120)
		low_clk_multiplier = 2;
	/* mipi min */
	temp_multiple = div_s64((4 * multiplier * clk_params->tlpx_numer_ns),
				clk_params->bitclk_mbps);
	div_s64_rem(temp_multiple, multiplier, &frac);
	mipi_min = (40 * multiplier) + (temp_multiple + frac);
	t->mipi_min = div_s64(mipi_min, multiplier);

	/* mipi_max */
	temp_multiple = div_s64(
			(6 * multiplier * clk_params->tlpx_numer_ns),
			clk_params->bitclk_mbps);
	div_s64_rem(temp_multiple, multiplier, &frac);
	mipi_max = (85 * multiplier) + temp_multiple;
	t->mipi_max = div_s64(mipi_max, multiplier);

	/* recommended min */
	temp_multiple = div_s64((mipi_min * clk_params->bitclk_mbps),
				clk_params->tlpx_numer_ns);
	temp_multiple -= (low_clk_multiplier * multiplier);
	div_s64_rem(temp_multiple, multiplier, &frac);
	rec_temp1 = roundup(((temp_multiple + frac) / 8), multiplier);
	t->rec_min = div_s64(rec_temp1, multiplier);

	/* recommended max */
	temp_multiple = div_s64((mipi_max * clk_params->bitclk_mbps),
				clk_params->tlpx_numer_ns);
	temp_multiple -= (low_clk_multiplier * multiplier);
	div_s64_rem(temp_multiple, multiplier, &frac);
	rec_temp2 = rounddown((temp_multiple / 8), multiplier);
	t->rec_max = div_s64(rec_temp2, multiplier);

	/* register value */
	dividend = ((rec_temp2 - rec_temp1) * clk_params->hs_prep_buf);
	temp = roundup(div_u64(dividend, 100), multiplier);
	t->rec = div_s64((temp + rec_temp1), multiplier);

	rc = mdss_dsi_phy_common_validate_and_set(t, "hs_prepare");
	if (rc)
		goto error;

	temp_multiple = div_s64(
			(8 * (temp + rec_temp1) * clk_params->tlpx_numer_ns),
			clk_params->bitclk_mbps);

	*temp_mul = temp_multiple;
	pr_debug("HS_PREP:mipi_min=%d, mipi_max=%d, rec_min=%d, rec_max=%d, reg_val=%d\n",
		 t->mipi_min, t->mipi_max, t->rec_min, t->rec_max,
		 t->program_value);

error:
	return rc;
}

/**
 * calc_hs_zero - calculates zero timing params for data lanes in HS.
 */
static int calc_hs_zero(struct dsi_phy_t_clk_param *clk_params,
			struct dsi_phy_timing *desc,
			u64 temp_multiple)
{
	u64 const multiplier = BIT(20);
	int rc = 0;
	struct timing_entry *t = &desc->hs_zero;
	s64 rec_temp1, rec_temp2, rec_temp3, mipi_min;
	s64 rec_min;

	if (!clk_params || !desc) {
		rc = -EINVAL;
		goto error;
	}

	mipi_min = div_s64((10 * clk_params->tlpx_numer_ns * multiplier),
			   clk_params->bitclk_mbps);
	rec_temp1 = (145 * multiplier) + mipi_min - temp_multiple;
	t->mipi_min = div_s64(rec_temp1, multiplier);

	/* recommended min */
	rec_temp1 = div_s64((rec_temp1 * clk_params->bitclk_mbps),
			    clk_params->tlpx_numer_ns);
	if (clk_params->phy_rev == DSI_PHY_REV_30) {
		rec_temp3 = roundup((rec_temp1 / 8), multiplier);
		rec_min = rec_temp3 - (1 * multiplier);
	} else {
		rec_temp2 = rec_temp1 - (11 * multiplier);
		rec_temp3 = roundup((rec_temp2 / 8), multiplier);
		rec_min = rec_temp3 - (3 * multiplier);
	}
	t->rec_min =  div_s64(rec_min, multiplier);
	t->rec_max = ((t->rec_min > 255) ? 511 : 255);

	t->rec = DIV_ROUND_UP(
			(((t->rec_max - t->rec_min) * clk_params->hs_zero_buf) +
			 (t->rec_min * 100)),
			100);

	rc = mdss_dsi_phy_common_validate_and_set(t, "hs_zero");
	if (rc)
		goto error;

	pr_debug("HS_ZERO:mipi_min=%d, mipi_max=%d, rec_min=%d, rec_max=%d, reg_val=%d\n",
		 t->mipi_min, t->mipi_max, t->rec_min, t->rec_max,
		 t->program_value);

error:
	return rc;
}

/**
 * calc_hs_trail - calculates trail timing params for data lanes in HS.
 */
static int calc_hs_trail(struct dsi_phy_t_clk_param *clk_params,
			 struct dsi_phy_timing *desc,
			 u64 teot_clk_lane)
{
	int rc = 0;
	struct timing_entry *t = &desc->hs_trail;
	s64 rec_temp1;

	if (!clk_params || !desc) {
		rc = -EINVAL;
		goto error;
	}

	t->mipi_min = 60 +
			mult_frac(clk_params->tlpx_numer_ns, 4,
				  clk_params->bitclk_mbps);

	t->mipi_max = teot_clk_lane - clk_params->treot_ns;

	if (clk_params->phy_rev == DSI_PHY_REV_30) {
		t->rec_min = DIV_ROUND_UP(
			(t->mipi_min * clk_params->bitclk_mbps),
			(8 * clk_params->tlpx_numer_ns)) - 1;

		rec_temp1 = (t->mipi_max * clk_params->bitclk_mbps);
		t->rec_max =
		     (div_s64(rec_temp1, (8 * clk_params->tlpx_numer_ns))) - 1;
	} else {
		t->rec_min = DIV_ROUND_UP(
			((t->mipi_min * clk_params->bitclk_mbps) +
			 (3 * clk_params->tlpx_numer_ns)),
			(8 * clk_params->tlpx_numer_ns));
		rec_temp1 = ((t->mipi_max * clk_params->bitclk_mbps) +
			     (3 * clk_params->tlpx_numer_ns));
		t->rec_max =
			div_s64(rec_temp1, (8 * clk_params->tlpx_numer_ns));
	}

	rec_temp1 = DIV_ROUND_UP(
			((t->rec_max - t->rec_min) * clk_params->hs_trail_buf),
			100);
	t->rec = rec_temp1 + t->rec_min;

	rc = mdss_dsi_phy_common_validate_and_set(t, "hs_trail");
	if (rc)
		goto error;

	pr_debug("HS_TRAIL:mipi_min=%d, mipi_max=%d, rec_min=%d, rec_max=%d, reg_val=%d\n",
		 t->mipi_min, t->mipi_max, t->rec_min, t->rec_max,
		 t->program_value);

error:
	return rc;
}

/**
 * calc_hs_rqst - calculates rqst timing params for data lanes in HS.
 */
static int calc_hs_rqst(struct dsi_phy_t_clk_param *clk_params,
			struct dsi_phy_timing *desc)
{
	int rc = 0;
	struct timing_entry *t = &desc->hs_rqst;

	if (!clk_params || !desc) {
		rc = -EINVAL;
		goto error;
	}

	t->rec = DIV_ROUND_UP(
		((t->mipi_min * clk_params->bitclk_mbps) -
		 (8 * clk_params->tlpx_numer_ns)),
		(8 * clk_params->tlpx_numer_ns));

	rc = mdss_dsi_phy_common_validate_and_set(t, "hs_rqst");
	if (rc)
		goto error;

	pr_debug("HS_RQST:mipi_min=%d, mipi_max=%d, rec_min=%d, rec_max=%d, reg_val=%d\n",
		 t->mipi_min, t->mipi_max, t->rec_min, t->rec_max,
		 t->program_value);

error:
	return rc;
}

/**
 * calc_hs_exit - calculates exit timing params for data lanes in HS.
 */
static int calc_hs_exit(struct dsi_phy_t_clk_param *clk_params,
			struct dsi_phy_timing *desc)
{
	int rc = 0;
	struct timing_entry *t = &desc->hs_exit;

	if (!clk_params || !desc) {
		rc = -EINVAL;
		goto error;
	}

	t->rec_min = (DIV_ROUND_UP(
			(t->mipi_min * clk_params->bitclk_mbps),
			(8 * clk_params->tlpx_numer_ns)) - 1);

	t->rec = DIV_ROUND_UP(
		(((t->rec_max - t->rec_min) * clk_params->hs_exit_buf) +
		 (t->rec_min * 100)),
		100);

	rc = mdss_dsi_phy_common_validate_and_set(t, "hs_exit");
	if (rc)
		goto error;

	pr_debug("HS_EXIT:mipi_min=%d, mipi_max=%d, rec_min=%d, rec_max=%d, reg_val=%d\n",
		 t->mipi_min, t->mipi_max, t->rec_min, t->rec_max,
		 t->program_value);

error:
	return rc;
}

/**
 * calc_hs_rqst_clk - calculates rqst timing params for clock lane..
 */
static int calc_hs_rqst_clk(struct dsi_phy_t_clk_param *clk_params,
			    struct dsi_phy_timing *desc)
{
	int rc = 0;
	struct timing_entry *t = &desc->hs_rqst_clk;

	if (!clk_params || !desc) {
		rc = -EINVAL;
		goto error;
	}

	t->rec = DIV_ROUND_UP(
		((t->mipi_min * clk_params->bitclk_mbps) -
		 (8 * clk_params->tlpx_numer_ns)),
		(8 * clk_params->tlpx_numer_ns));

	rc = mdss_dsi_phy_common_validate_and_set(t, "hs_rqst_clk");
	if (rc)
		goto error;

	pr_debug("HS_RQST_CLK:mipi_min=%d, mipi_max=%d, rec_min=%d, rec_max=%d, reg_val=%d\n",
		 t->mipi_min, t->mipi_max, t->rec_min, t->rec_max,
		 t->program_value);

error:
	return rc;
}

static int mdss_dsi_phy_calc_param_phy_cmn(
					struct dsi_phy_t_clk_param *clk_params,
					struct dsi_phy_timing *desc)
{
	int rc = 0;
	s32 actual_frac = 0;
	s64 actual_intermediate = 0;
	u64 temp_multiple;
	s64 teot_clk_lane;

	if (!clk_params || !desc) {
		rc = -EINVAL;
		goto error;
	}

	rc = calc_clk_prepare(clk_params, desc, &actual_frac,
			      &actual_intermediate);
	if (rc) {
		pr_err("clk_prepare calculations failed, rc=%d\n", rc);
		goto error;
	}

	rc = calc_clk_zero(clk_params, desc, actual_frac, actual_intermediate);
	if (rc) {
		pr_err("clk_zero calculations failed, rc=%d\n", rc);
		goto error;
	}

	rc = calc_clk_trail(clk_params, desc, &teot_clk_lane);
	if (rc) {
		pr_err("clk_trail calculations failed, rc=%d\n", rc);
		goto error;
	}

	rc = calc_hs_prepare(clk_params, desc, &temp_multiple);
	if (rc) {
		pr_err("hs_prepare calculations failed, rc=%d\n", rc);
		goto error;
	}

	rc = calc_hs_zero(clk_params, desc, temp_multiple);
	if (rc) {
		pr_err("hs_zero calculations failed, rc=%d\n", rc);
		goto error;
	}

	rc = calc_hs_trail(clk_params, desc, teot_clk_lane);
	if (rc) {
		pr_err("hs_trail calculations failed, rc=%d\n", rc);
		goto error;
	}

	rc = calc_hs_rqst(clk_params, desc);
	if (rc) {
		pr_err("hs_rqst calculations failed, rc=%d\n", rc);
		goto error;
	}

	rc = calc_hs_exit(clk_params, desc);
	if (rc) {
		pr_err("hs_exit calculations failed, rc=%d\n", rc);
		goto error;
	}

	rc = calc_hs_rqst_clk(clk_params, desc);
	if (rc) {
		pr_err("hs_rqst_clk calculations failed, rc=%d\n", rc);
		goto error;
	}
error:
	return rc;
}

static int mdss_dsi_phy_calc_hs_param_phy_rev_1(
		struct dsi_phy_t_clk_param *t_clk,
		struct dsi_phy_timing *t_param)
{
	int percent_min = 10;
	int percent_allowable_phy = 0;
	int percent_min_ths;
	int tmp, rc = 0;
	int b6, h10, h11, h17;

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
	h17 = (t_param->hs_exit.program_value / 2) + 1;
	tmp = ((60 * (int)t_clk->bitclk_mbps) + (52 * 1000)
			- (24 * 1000) - (h17 * 2 * 1000));
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

	h10 = (t_param->clk_prepare.program_value / 2) + 1;
	h11 = (t_param->clk_zero.program_value / 2) + 1;
	b6 = 10000/t_clk->escclk_numer;

	t_param->clk_pre.rec_min =
		DIV_ROUND_UP((b6 * t_clk->bitclk_mbps) + (8 * 1000)
			+ (h10 * 2 * 1000) + (h11 * 2 * 1000), 8 * 1000) - 1;
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

static void mdss_dsi_phy_update_timing_param_v2(struct mdss_panel_info *pinfo,
		struct dsi_phy_timing *t_param)
{
	struct mdss_dsi_phy_ctrl *reg;
	int i = 0;

	reg = &(pinfo->mipi.dsi_phy_db);

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

static void mdss_dsi_phy_update_timing_param_v3(struct mdss_panel_info *pinfo,
		struct dsi_phy_timing *t_param)
{
	struct mdss_dsi_phy_ctrl *pd;

	pd = &(pinfo->mipi.dsi_phy_db);

	pd->timing[0] = 0x00;
	pd->timing[1] = t_param->clk_zero.program_value;
	pd->timing[2] = t_param->clk_prepare.program_value;
	pd->timing[3] = t_param->clk_trail.program_value;
	pd->timing[4] = t_param->hs_exit.program_value;
	pd->timing[5] = t_param->hs_zero.program_value;
	pd->timing[6] = t_param->hs_prepare.program_value;
	pd->timing[7] = t_param->hs_trail.program_value;
	pd->timing[8] = t_param->hs_rqst.program_value;
	pd->timing[9] = 0x03;
	pd->timing[10] = 0x04;
	pd->timing[11] = 0x00;
}

int mdss_dsi_phy_calc_timing_param(struct mdss_panel_info *pinfo, u32 phy_rev,
		u64 clk_rate)
{
	struct dsi_phy_t_clk_param t_clk;
	struct dsi_phy_timing t_param;
	int rc = 0;

	if (!pinfo) {
		pr_err("invalid panel info\n");
		return -EINVAL;
	}

	t_clk.bitclk_mbps = rounddown((uint32_t) div_u64(clk_rate, 1000000), 1);
	t_clk.escclk_numer = ESC_CLK_MHZ;
	t_clk.escclk_denom = ESCCLK_MMSS_CC_PREDIV;
	t_clk.tlpx_numer_ns = TLPX_NUMER;
	t_clk.treot_ns = TR_EOT;
	pr_debug("bitrate=%d\n", t_clk.bitclk_mbps);

	rc = mdss_dsi_phy_initialize_defaults(&t_clk, &t_param, phy_rev);
	if (rc) {
		pr_err("phy%d initialization failed\n", phy_rev);
		goto timing_calc_end;
	}

	switch (phy_rev) {
	case DSI_PHY_REV_10:
		mdss_dsi_phy_calc_param_phy_rev_1(&t_clk, &t_param);
		mdss_dsi_phy_update_timing_param(pinfo, &t_param);
		break;
	case DSI_PHY_REV_20:
		rc = mdss_dsi_phy_calc_param_phy_cmn(&t_clk, &t_param);
		if (rc) {
			pr_err("Phy timing calculations failed\n");
			goto timing_calc_end;
		}
		mdss_dsi_phy_update_timing_param_v2(pinfo, &t_param);
		break;
	case DSI_PHY_REV_30:
		rc = mdss_dsi_phy_calc_param_phy_cmn(&t_clk, &t_param);
		if (rc) {
			pr_err("Phy timing calculations failed\n");
			goto timing_calc_end;
		}
		mdss_dsi_phy_update_timing_param_v3(pinfo, &t_param);
		break;
	default:
		pr_err("phy rev %d not supported\n", phy_rev);
		return -EINVAL;
	}

timing_calc_end:
	return rc;
}
