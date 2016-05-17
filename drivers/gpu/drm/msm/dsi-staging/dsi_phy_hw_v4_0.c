/*
 * Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "dsi-phy-hw:" fmt
#include <linux/math64.h>
#include <linux/delay.h>
#include "dsi_hw.h"
#include "dsi_phy_hw.h"

#define DSIPHY_CMN_REVISION_ID0                   0x0000
#define DSIPHY_CMN_REVISION_ID1                   0x0004
#define DSIPHY_CMN_REVISION_ID2                   0x0008
#define DSIPHY_CMN_REVISION_ID3                   0x000C
#define DSIPHY_CMN_CLK_CFG0                       0x0010
#define DSIPHY_CMN_CLK_CFG1                       0x0014
#define DSIPHY_CMN_GLBL_TEST_CTRL                 0x0018
#define DSIPHY_CMN_CTRL_0                         0x001C
#define DSIPHY_CMN_CTRL_1                         0x0020
#define DSIPHY_CMN_CAL_HW_TRIGGER                 0x0024
#define DSIPHY_CMN_CAL_SW_CFG0                    0x0028
#define DSIPHY_CMN_CAL_SW_CFG1                    0x002C
#define DSIPHY_CMN_CAL_SW_CFG2                    0x0030
#define DSIPHY_CMN_CAL_HW_CFG0                    0x0034
#define DSIPHY_CMN_CAL_HW_CFG1                    0x0038
#define DSIPHY_CMN_CAL_HW_CFG2                    0x003C
#define DSIPHY_CMN_CAL_HW_CFG3                    0x0040
#define DSIPHY_CMN_CAL_HW_CFG4                    0x0044
#define DSIPHY_CMN_PLL_CNTRL                      0x0048
#define DSIPHY_CMN_LDO_CNTRL                      0x004C

#define DSIPHY_CMN_REGULATOR_CAL_STATUS0          0x0064
#define DSIPHY_CMN_REGULATOR_CAL_STATUS1          0x0068

/* n = 0..3 for data lanes and n = 4 for clock lane */
#define DSIPHY_DLNX_CFG0(n)                     (0x100 + ((n) * 0x80))
#define DSIPHY_DLNX_CFG1(n)                     (0x104 + ((n) * 0x80))
#define DSIPHY_DLNX_CFG2(n)                     (0x108 + ((n) * 0x80))
#define DSIPHY_DLNX_CFG3(n)                     (0x10C + ((n) * 0x80))
#define DSIPHY_DLNX_TEST_DATAPATH(n)            (0x110 + ((n) * 0x80))
#define DSIPHY_DLNX_TEST_STR(n)                 (0x114 + ((n) * 0x80))
#define DSIPHY_DLNX_TIMING_CTRL_4(n)            (0x118 + ((n) * 0x80))
#define DSIPHY_DLNX_TIMING_CTRL_5(n)            (0x11C + ((n) * 0x80))
#define DSIPHY_DLNX_TIMING_CTRL_6(n)            (0x120 + ((n) * 0x80))
#define DSIPHY_DLNX_TIMING_CTRL_7(n)            (0x124 + ((n) * 0x80))
#define DSIPHY_DLNX_TIMING_CTRL_8(n)            (0x128 + ((n) * 0x80))
#define DSIPHY_DLNX_TIMING_CTRL_9(n)            (0x12C + ((n) * 0x80))
#define DSIPHY_DLNX_TIMING_CTRL_10(n)           (0x130 + ((n) * 0x80))
#define DSIPHY_DLNX_TIMING_CTRL_11(n)           (0x134 + ((n) * 0x80))
#define DSIPHY_DLNX_STRENGTH_CTRL_0(n)          (0x138 + ((n) * 0x80))
#define DSIPHY_DLNX_STRENGTH_CTRL_1(n)          (0x13C + ((n) * 0x80))
#define DSIPHY_DLNX_BIST_POLY(n)                (0x140 + ((n) * 0x80))
#define DSIPHY_DLNX_BIST_SEED0(n)               (0x144 + ((n) * 0x80))
#define DSIPHY_DLNX_BIST_SEED1(n)               (0x148 + ((n) * 0x80))
#define DSIPHY_DLNX_BIST_HEAD(n)                (0x14C + ((n) * 0x80))
#define DSIPHY_DLNX_BIST_SOT(n)                 (0x150 + ((n) * 0x80))
#define DSIPHY_DLNX_BIST_CTRL0(n)               (0x154 + ((n) * 0x80))
#define DSIPHY_DLNX_BIST_CTRL1(n)               (0x158 + ((n) * 0x80))
#define DSIPHY_DLNX_BIST_CTRL2(n)               (0x15C + ((n) * 0x80))
#define DSIPHY_DLNX_BIST_CTRL3(n)               (0x160 + ((n) * 0x80))
#define DSIPHY_DLNX_VREG_CNTRL(n)               (0x164 + ((n) * 0x80))
#define DSIPHY_DLNX_HSTX_STR_STATUS(n)          (0x168 + ((n) * 0x80))
#define DSIPHY_DLNX_BIST_STATUS0(n)             (0x16C + ((n) * 0x80))
#define DSIPHY_DLNX_BIST_STATUS1(n)             (0x170 + ((n) * 0x80))
#define DSIPHY_DLNX_BIST_STATUS2(n)             (0x174 + ((n) * 0x80))
#define DSIPHY_DLNX_BIST_STATUS3(n)             (0x178 + ((n) * 0x80))
#define DSIPHY_DLNX_MISR_STATUS(n)              (0x17C + ((n) * 0x80))

#define DSIPHY_PLL_CLKBUFLR_EN                  0x041C
#define DSIPHY_PLL_PLL_BANDGAP                  0x0508

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
};

/**
 * regulator_enable() - enable regulators for DSI PHY
 * @phy:      Pointer to DSI PHY hardware object.
 * @reg_cfg:  Regulator configuration for all DSI lanes.
 */
void dsi_phy_hw_v4_0_regulator_enable(struct dsi_phy_hw *phy,
				      struct dsi_phy_per_lane_cfgs *reg_cfg)
{
	int i;

	for (i = DSI_LOGICAL_LANE_0; i < DSI_LANE_MAX; i++)
		DSI_W32(phy, DSIPHY_DLNX_VREG_CNTRL(i), reg_cfg->lane[i][0]);

	/* make sure all values are written to hardware */
	wmb();

	pr_debug("[DSI_%d] Phy regulators enabled\n", phy->index);
}

/**
 * regulator_disable() - disable regulators
 * @phy:      Pointer to DSI PHY hardware object.
 */
void dsi_phy_hw_v4_0_regulator_disable(struct dsi_phy_hw *phy)
{
	pr_debug("[DSI_%d] Phy regulators disabled\n", phy->index);
}

/**
 * enable() - Enable PHY hardware
 * @phy:      Pointer to DSI PHY hardware object.
 * @cfg:      Per lane configurations for timing, strength and lane
 *	      configurations.
 */
void dsi_phy_hw_v4_0_enable(struct dsi_phy_hw *phy,
			    struct dsi_phy_cfg *cfg)
{
	int i;
	struct dsi_phy_per_lane_cfgs *timing = &cfg->timing;
	u32 data;

	DSI_W32(phy, DSIPHY_CMN_LDO_CNTRL, 0x1C);

	DSI_W32(phy, DSIPHY_CMN_GLBL_TEST_CTRL, 0x1);
	for (i = DSI_LOGICAL_LANE_0; i < DSI_LANE_MAX; i++) {

		DSI_W32(phy, DSIPHY_DLNX_CFG0(i), cfg->lanecfg.lane[i][0]);
		DSI_W32(phy, DSIPHY_DLNX_CFG1(i), cfg->lanecfg.lane[i][1]);
		DSI_W32(phy, DSIPHY_DLNX_CFG2(i), cfg->lanecfg.lane[i][2]);
		DSI_W32(phy, DSIPHY_DLNX_CFG3(i), cfg->lanecfg.lane[i][3]);

		DSI_W32(phy, DSIPHY_DLNX_TEST_STR(i), 0x88);

		DSI_W32(phy, DSIPHY_DLNX_TIMING_CTRL_4(i), timing->lane[i][0]);
		DSI_W32(phy, DSIPHY_DLNX_TIMING_CTRL_5(i), timing->lane[i][1]);
		DSI_W32(phy, DSIPHY_DLNX_TIMING_CTRL_6(i), timing->lane[i][2]);
		DSI_W32(phy, DSIPHY_DLNX_TIMING_CTRL_7(i), timing->lane[i][3]);
		DSI_W32(phy, DSIPHY_DLNX_TIMING_CTRL_8(i), timing->lane[i][4]);
		DSI_W32(phy, DSIPHY_DLNX_TIMING_CTRL_9(i), timing->lane[i][5]);
		DSI_W32(phy, DSIPHY_DLNX_TIMING_CTRL_10(i), timing->lane[i][6]);
		DSI_W32(phy, DSIPHY_DLNX_TIMING_CTRL_11(i), timing->lane[i][7]);

		DSI_W32(phy, DSIPHY_DLNX_STRENGTH_CTRL_0(i),
			cfg->strength.lane[i][0]);
		DSI_W32(phy, DSIPHY_DLNX_STRENGTH_CTRL_1(i),
			cfg->strength.lane[i][1]);
	}

	/* make sure all values are written to hardware before enabling phy */
	wmb();

	DSI_W32(phy, DSIPHY_CMN_CTRL_1, 0x80);
	udelay(100);
	DSI_W32(phy, DSIPHY_CMN_CTRL_1, 0x00);

	data = DSI_R32(phy, DSIPHY_CMN_GLBL_TEST_CTRL);

	switch (cfg->pll_source) {
	case DSI_PLL_SOURCE_STANDALONE:
		DSI_W32(phy, DSIPHY_PLL_CLKBUFLR_EN, 0x01);
		data &= ~BIT(2);
		break;
	case DSI_PLL_SOURCE_NATIVE:
		DSI_W32(phy, DSIPHY_PLL_CLKBUFLR_EN, 0x03);
		data &= ~BIT(2);
		break;
	case DSI_PLL_SOURCE_NON_NATIVE:
		DSI_W32(phy, DSIPHY_PLL_CLKBUFLR_EN, 0x00);
		data |= BIT(2);
		break;
	default:
		break;
	}

	DSI_W32(phy, DSIPHY_CMN_GLBL_TEST_CTRL, data);

	/* Enable bias current for pll1 during split display case */
	if (cfg->pll_source == DSI_PLL_SOURCE_NON_NATIVE)
		DSI_W32(phy, DSIPHY_PLL_PLL_BANDGAP, 0x3);

	pr_debug("[DSI_%d]Phy enabled ", phy->index);
}

/**
 * disable() - Disable PHY hardware
 * @phy:      Pointer to DSI PHY hardware object.
 */
void dsi_phy_hw_v4_0_disable(struct dsi_phy_hw *phy)
{
	DSI_W32(phy, DSIPHY_PLL_CLKBUFLR_EN, 0);
	DSI_W32(phy, DSIPHY_CMN_GLBL_TEST_CTRL, 0);
	DSI_W32(phy, DSIPHY_CMN_CTRL_0, 0);
	pr_debug("[DSI_%d]Phy disabled ", phy->index);
}

static const u32 bits_per_pixel[DSI_PIXEL_FORMAT_MAX] = {
	16, 18, 18, 24, 3, 8, 12 };

/**
 * calc_clk_prepare - calculates prepare timing params for clk lane.
 */
static int calc_clk_prepare(struct phy_clk_params *clk_params,
			    struct phy_timing_desc *desc,
			    s32 *actual_frac,
			    s64 *actual_intermediate)
{
	u32 const min_prepare_frac = 50;
	u64 const multiplier = BIT(20);

	struct timing_entry *t = &desc->clk_prepare;
	int rc = 0;
	u64 dividend, temp, temp_multiple;
	s32 frac = 0;
	s64 intermediate;
	s64 clk_prep_actual;

	dividend = ((t->rec_max - t->rec_min) * min_prepare_frac * multiplier);
	temp  = roundup(div_s64(dividend, 100), multiplier);
	temp += (t->rec_min * multiplier);
	t->rec = div_s64(temp, multiplier);

	if (t->rec & 0xffffff00) {
		pr_err("Incorrect rec valuefor clk_prepare\n");
		rc = -EINVAL;
	} else {
		t->reg_value = t->rec;
	}

	/* calculate theoretical value */
	temp_multiple = 8 * t->reg_value * clk_params->tlpx_numer_ns
			 * multiplier;
	intermediate = div_s64(temp_multiple, clk_params->bitclk_mbps);
	div_s64_rem(temp_multiple, clk_params->bitclk_mbps, &frac);
	clk_prep_actual = div_s64((intermediate + frac), multiplier);

	pr_debug("CLK_PREPARE:mipi_min=%d, mipi_max=%d, rec_min=%d, rec_max=%d",
		 t->mipi_min, t->mipi_max, t->rec_min, t->rec_max);
	pr_debug(" reg_value=%d, actual=%lld\n", t->reg_value, clk_prep_actual);

	*actual_frac = frac;
	*actual_intermediate = intermediate;

	return rc;
}

/**
 * calc_clk_zero - calculates zero timing params for clk lane.
 */
static int calc_clk_zero(struct phy_clk_params *clk_params,
			 struct phy_timing_desc *desc,
			 s32 actual_frac,
			 s64 actual_intermediate)
{
	u32 const clk_zero_min_frac = 2;
	u64 const multiplier = BIT(20);

	int rc = 0;
	struct timing_entry *t = &desc->clk_zero;
	s64 mipi_min, rec_temp1, rec_temp2, rec_temp3, rec_min;

	mipi_min = ((300 * multiplier) - (actual_intermediate + actual_frac));
	t->mipi_min = div_s64(mipi_min, multiplier);

	rec_temp1 = div_s64((mipi_min * clk_params->bitclk_mbps),
			    clk_params->tlpx_numer_ns);
	rec_temp2 = (rec_temp1 - (11 * multiplier));
	rec_temp3 = roundup(div_s64(rec_temp2, 8), multiplier);
	rec_min = (div_s64(rec_temp3, multiplier) - 3);
	t->rec_min = rec_min;
	t->rec_max = ((t->rec_min > 255) ? 511 : 255);

	t->rec = DIV_ROUND_UP(
			(((t->rec_max - t->rec_min) * clk_zero_min_frac) +
			 (t->rec_min * 100)),
			100);

	if (t->rec & 0xffffff00) {
		pr_err("Incorrect rec valuefor clk_zero\n");
		rc = -EINVAL;
	} else {
		t->reg_value = t->rec;
	}

	pr_debug("CLK_ZERO:mipi_min=%d, mipi_max=%d, rec_min=%d, rec_max=%d, reg_val=%d\n",
		 t->mipi_min, t->mipi_max, t->rec_min, t->rec_max,
		 t->reg_value);
	return rc;
}

/**
 * calc_clk_trail - calculates prepare trail params for clk lane.
 */
static int calc_clk_trail(struct phy_clk_params *clk_params,
			  struct phy_timing_desc *desc,
			  s64 *teot_clk_lane)
{
	u64 const multiplier = BIT(20);
	u32 const phy_timing_frac = 30;

	int rc = 0;
	struct timing_entry *t = &desc->clk_trail;
	u64 temp_multiple;
	s32 frac;
	s64 mipi_max_tr, rec_temp1, rec_temp2, rec_temp3, mipi_max;
	s64 teot_clk_lane1;

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
	rec_temp1 = temp_multiple + frac + (3 * multiplier);
	rec_temp2 = div_s64(rec_temp1, 8);
	rec_temp3 = roundup(rec_temp2, multiplier);

	t->rec_min = div_s64(rec_temp3, multiplier);

	/* recommended max */
	rec_temp1 = div_s64((mipi_max * clk_params->bitclk_mbps),
			    clk_params->tlpx_numer_ns);
	rec_temp2 = rec_temp1 + (3 * multiplier);
	rec_temp3 = rec_temp2 / 8;
	t->rec_max = div_s64(rec_temp3, multiplier);

	t->rec = DIV_ROUND_UP(
		(((t->rec_max - t->rec_min) * phy_timing_frac) +
		 (t->rec_min * 100)),
		 100);

	if (t->rec & 0xffffff00) {
		pr_err("Incorrect rec valuefor clk_zero\n");
		rc = -EINVAL;
	} else {
		t->reg_value = t->rec;
	}

	*teot_clk_lane = teot_clk_lane1;
	pr_debug("CLK_TRAIL:mipi_min=%d, mipi_max=%d, rec_min=%d, rec_max=%d, reg_val=%d\n",
		 t->mipi_min, t->mipi_max, t->rec_min, t->rec_max,
		 t->reg_value);
	return rc;

}

/**
 * calc_hs_prepare - calculates prepare timing params for data lanes in HS.
 */
static int calc_hs_prepare(struct phy_clk_params *clk_params,
			   struct phy_timing_desc *desc,
			   u64 *temp_mul)
{
	u64 const multiplier = BIT(20);
	u32 const min_prepare_frac = 50;
	int rc = 0;
	struct timing_entry *t = &desc->hs_prepare;
	u64 temp_multiple, dividend, temp;
	s32 frac;
	s64 rec_temp1, rec_temp2, mipi_max, mipi_min;
	u32 low_clk_multiplier = 0;

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
	dividend = ((rec_temp2 - rec_temp1) * min_prepare_frac);
	temp = roundup(div_u64(dividend, 100), multiplier);
	t->rec = div_s64((temp + rec_temp1), multiplier);

	if (t->rec & 0xffffff00) {
		pr_err("Incorrect rec valuefor hs_prepare\n");
		rc = -EINVAL;
	} else {
		t->reg_value = t->rec;
	}

	temp_multiple = div_s64(
			(8 * (temp + rec_temp1) * clk_params->tlpx_numer_ns),
			clk_params->bitclk_mbps);

	*temp_mul = temp_multiple;
	pr_debug("HS_PREP:mipi_min=%d, mipi_max=%d, rec_min=%d, rec_max=%d, reg_val=%d\n",
		 t->mipi_min, t->mipi_max, t->rec_min, t->rec_max,
		 t->reg_value);
	return rc;
}

/**
 * calc_hs_zero - calculates zero timing params for data lanes in HS.
 */
static int calc_hs_zero(struct phy_clk_params *clk_params,
			struct phy_timing_desc *desc,
			u64 temp_multiple)
{
	u32 const hs_zero_min_frac = 10;
	u64 const multiplier = BIT(20);
	int rc = 0;
	struct timing_entry *t = &desc->hs_zero;
	s64 rec_temp1, rec_temp2, rec_temp3, mipi_min;
	s64 rec_min;

	mipi_min = div_s64((10 * clk_params->tlpx_numer_ns * multiplier),
			   clk_params->bitclk_mbps);
	rec_temp1 = (145 * multiplier) + mipi_min - temp_multiple;
	t->mipi_min = div_s64(rec_temp1, multiplier);

	/* recommended min */
	rec_temp1 = div_s64((rec_temp1 * clk_params->bitclk_mbps),
			    clk_params->tlpx_numer_ns);
	rec_temp2 = rec_temp1 - (11 * multiplier);
	rec_temp3 = roundup((rec_temp2 / 8), multiplier);
	rec_min = rec_temp3 - (3 * multiplier);
	t->rec_min =  div_s64(rec_min, multiplier);
	t->rec_max = ((t->rec_min > 255) ? 511 : 255);

	t->rec = DIV_ROUND_UP(
			(((t->rec_max - t->rec_min) * hs_zero_min_frac) +
			 (t->rec_min * 100)),
			100);

	if (t->rec & 0xffffff00) {
		pr_err("Incorrect rec valuefor hs_zero\n");
		rc = -EINVAL;
	} else {
		t->reg_value = t->rec;
	}

	pr_debug("HS_ZERO:mipi_min=%d, mipi_max=%d, rec_min=%d, rec_max=%d, reg_val=%d\n",
		 t->mipi_min, t->mipi_max, t->rec_min, t->rec_max,
		 t->reg_value);

	return rc;
}

/**
 * calc_hs_trail - calculates trail timing params for data lanes in HS.
 */
static int calc_hs_trail(struct phy_clk_params *clk_params,
			 struct phy_timing_desc *desc,
			 u64 teot_clk_lane)
{
	u32 const phy_timing_frac = 30;
	int rc = 0;
	struct timing_entry *t = &desc->hs_trail;
	s64 rec_temp1;

	t->mipi_min = 60 +
			mult_frac(clk_params->tlpx_numer_ns, 4,
				  clk_params->bitclk_mbps);

	t->mipi_max = teot_clk_lane - clk_params->treot_ns;

	t->rec_min = DIV_ROUND_UP(
		((t->mipi_min * clk_params->bitclk_mbps) +
		 (3 * clk_params->tlpx_numer_ns)),
		(8 * clk_params->tlpx_numer_ns));

	rec_temp1 = ((t->mipi_max * clk_params->bitclk_mbps) +
		     (3 * clk_params->tlpx_numer_ns));
	t->rec_max = (rec_temp1 / (8 * clk_params->tlpx_numer_ns));
	rec_temp1 = DIV_ROUND_UP(
			((t->rec_max - t->rec_min) * phy_timing_frac),
			100);
	t->rec = rec_temp1 + t->rec_min;

	if (t->rec & 0xffffff00) {
		pr_err("Incorrect rec valuefor hs_trail\n");
		rc = -EINVAL;
	} else {
		t->reg_value = t->rec;
	}

	pr_debug("HS_TRAIL:mipi_min=%d, mipi_max=%d, rec_min=%d, rec_max=%d, reg_val=%d\n",
		 t->mipi_min, t->mipi_max, t->rec_min, t->rec_max,
		 t->reg_value);

	return rc;
}

/**
 * calc_hs_rqst - calculates rqst timing params for data lanes in HS.
 */
static int calc_hs_rqst(struct phy_clk_params *clk_params,
			struct phy_timing_desc *desc)
{
	int rc = 0;
	struct timing_entry *t = &desc->hs_rqst;

	t->rec = DIV_ROUND_UP(
		((t->mipi_min * clk_params->bitclk_mbps) -
		 (8 * clk_params->tlpx_numer_ns)),
		(8 * clk_params->tlpx_numer_ns));

	if (t->rec & 0xffffff00) {
		pr_err("Incorrect rec valuefor hs_rqst, %d\n", t->rec);
		rc = -EINVAL;
	} else {
		t->reg_value = t->rec;
	}

	pr_debug("HS_RQST:mipi_min=%d, mipi_max=%d, rec_min=%d, rec_max=%d, reg_val=%d\n",
		 t->mipi_min, t->mipi_max, t->rec_min, t->rec_max,
		 t->reg_value);

	return rc;
}

/**
 * calc_hs_exit - calculates exit timing params for data lanes in HS.
 */
static int calc_hs_exit(struct phy_clk_params *clk_params,
			struct phy_timing_desc *desc)
{
	u32 const hs_exit_min_frac = 10;
	int rc = 0;
	struct timing_entry *t = &desc->hs_exit;

	t->rec_min = (DIV_ROUND_UP(
			(t->mipi_min * clk_params->bitclk_mbps),
			(8 * clk_params->tlpx_numer_ns)) - 1);

	t->rec = DIV_ROUND_UP(
		(((t->rec_max - t->rec_min) * hs_exit_min_frac) +
		 (t->rec_min * 100)),
		100);

	if (t->rec & 0xffffff00) {
		pr_err("Incorrect rec valuefor hs_exit\n");
		rc = -EINVAL;
	} else {
		t->reg_value = t->rec;
	}

	pr_debug("HS_EXIT:mipi_min=%d, mipi_max=%d, rec_min=%d, rec_max=%d, reg_val=%d\n",
		 t->mipi_min, t->mipi_max, t->rec_min, t->rec_max,
		 t->reg_value);

	return rc;
}

/**
 * calc_hs_rqst_clk - calculates rqst timing params for clock lane..
 */
static int calc_hs_rqst_clk(struct phy_clk_params *clk_params,
			    struct phy_timing_desc *desc)
{
	int rc = 0;
	struct timing_entry *t = &desc->hs_rqst_clk;

	t->rec = DIV_ROUND_UP(
		((t->mipi_min * clk_params->bitclk_mbps) -
		 (8 * clk_params->tlpx_numer_ns)),
		(8 * clk_params->tlpx_numer_ns));

	if (t->rec & 0xffffff00) {
		pr_err("Incorrect rec valuefor hs_rqst_clk\n");
		rc = -EINVAL;
	} else {
		t->reg_value = t->rec;
	}

	pr_debug("HS_RQST_CLK:mipi_min=%d, mipi_max=%d, rec_min=%d, rec_max=%d, reg_val=%d\n",
		 t->mipi_min, t->mipi_max, t->rec_min, t->rec_max,
		 t->reg_value);

	return rc;
}

/**
 * dsi_phy_calc_timing_params - calculates timing paramets for a given bit clock
 */
static int dsi_phy_calc_timing_params(struct phy_clk_params *clk_params,
				      struct phy_timing_desc *desc)
{
	int rc = 0;
	s32 actual_frac = 0;
	s64 actual_intermediate = 0;
	u64 temp_multiple;
	s64 teot_clk_lane;

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

/**
 * calculate_timing_params() - calculates timing parameters.
 * @phy:      Pointer to DSI PHY hardware object.
 * @mode:     Mode information for which timing has to be calculated.
 * @config:   DSI host configuration for this mode.
 * @timing:   Timing parameters for each lane which will be returned.
 */
int dsi_phy_hw_v4_0_calculate_timing_params(struct dsi_phy_hw *phy,
					    struct dsi_mode_info *mode,
					    struct dsi_host_common_cfg *host,
					   struct dsi_phy_per_lane_cfgs *timing)
{
	/* constants */
	u32 const esc_clk_mhz = 192; /* TODO: esc clock is hardcoded */
	u32 const esc_clk_mmss_cc_prediv = 10;
	u32 const tlpx_numer = 1000;
	u32 const tr_eot = 20;
	u32 const clk_prepare_spec_min = 38;
	u32 const clk_prepare_spec_max = 95;
	u32 const clk_trail_spec_min = 60;
	u32 const hs_exit_spec_min = 100;
	u32 const hs_exit_reco_max = 255;
	u32 const hs_rqst_spec_min = 50;

	/* local vars */
	int rc = 0;
	int i;
	u32 h_total, v_total;
	u64 inter_num;
	u32 num_of_lanes = 0;
	u32 bpp;
	u64 x, y;
	struct phy_timing_desc desc;
	struct phy_clk_params clk_params = {0};

	memset(&desc, 0x0, sizeof(desc));
	h_total = DSI_H_TOTAL(mode);
	v_total = DSI_V_TOTAL(mode);

	bpp = bits_per_pixel[host->dst_format];

	inter_num = bpp * mode->refresh_rate;

	if (host->data_lanes & DSI_DATA_LANE_0)
		num_of_lanes++;
	if (host->data_lanes & DSI_DATA_LANE_1)
		num_of_lanes++;
	if (host->data_lanes & DSI_DATA_LANE_2)
		num_of_lanes++;
	if (host->data_lanes & DSI_DATA_LANE_3)
		num_of_lanes++;


	x = mult_frac(v_total * h_total, inter_num, num_of_lanes);
	y = rounddown(x, 1);

	clk_params.bitclk_mbps = rounddown(mult_frac(y, 1, 1000000), 1);
	clk_params.escclk_numer = esc_clk_mhz;
	clk_params.escclk_denom = esc_clk_mmss_cc_prediv;
	clk_params.tlpx_numer_ns = tlpx_numer;
	clk_params.treot_ns = tr_eot;


	/* Setup default parameters */
	desc.clk_prepare.mipi_min = clk_prepare_spec_min;
	desc.clk_prepare.mipi_max = clk_prepare_spec_max;
	desc.clk_trail.mipi_min = clk_trail_spec_min;
	desc.hs_exit.mipi_min = hs_exit_spec_min;
	desc.hs_exit.rec_max = hs_exit_reco_max;

	desc.clk_prepare.rec_min = DIV_ROUND_UP(
			(desc.clk_prepare.mipi_min * clk_params.bitclk_mbps),
			(8 * clk_params.tlpx_numer_ns)
			);

	desc.clk_prepare.rec_max = rounddown(
		mult_frac((desc.clk_prepare.mipi_max * clk_params.bitclk_mbps),
			  1, (8 * clk_params.tlpx_numer_ns)),
		1);

	desc.hs_rqst.mipi_min = hs_rqst_spec_min;
	desc.hs_rqst_clk.mipi_min = hs_rqst_spec_min;

	pr_debug("BIT CLOCK = %d, tlpx_numer_ns=%d, treot_ns=%d\n",
	       clk_params.bitclk_mbps, clk_params.tlpx_numer_ns,
	       clk_params.treot_ns);
	rc = dsi_phy_calc_timing_params(&clk_params, &desc);
	if (rc) {
		pr_err("Timing calc failed, rc=%d\n", rc);
		goto error;
	}


	for (i = DSI_LOGICAL_LANE_0; i < DSI_LANE_MAX; i++) {
		timing->lane[i][0] = desc.hs_exit.reg_value;

		if (i == DSI_LOGICAL_CLOCK_LANE)
			timing->lane[i][1] = desc.clk_zero.reg_value;
		else
			timing->lane[i][1] = desc.hs_zero.reg_value;

		if (i == DSI_LOGICAL_CLOCK_LANE)
			timing->lane[i][2] = desc.clk_prepare.reg_value;
		else
			timing->lane[i][2] = desc.hs_prepare.reg_value;

		if (i == DSI_LOGICAL_CLOCK_LANE)
			timing->lane[i][3] = desc.clk_trail.reg_value;
		else
			timing->lane[i][3] = desc.hs_trail.reg_value;

		if (i == DSI_LOGICAL_CLOCK_LANE)
			timing->lane[i][4] = desc.hs_rqst_clk.reg_value;
		else
			timing->lane[i][4] = desc.hs_rqst.reg_value;

		timing->lane[i][5] = 0x3;
		timing->lane[i][6] = 0x4;
		timing->lane[i][7] = 0xA0;
		pr_debug("[%d][%d %d %d %d %d]\n", i, timing->lane[i][0],
						    timing->lane[i][1],
						    timing->lane[i][2],
						    timing->lane[i][3],
						    timing->lane[i][4]);
	}
	timing->count_per_lane = 8;

error:
	return rc;
}
