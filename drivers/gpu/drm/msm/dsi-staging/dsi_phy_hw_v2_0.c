/*
 * Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
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
#define DSI_MDP_ULPS_CLAMP_ENABLE_OFF             0x0054

/* n = 0..3 for data lanes and n = 4 for clock lane
 * t for count per lane
 */
#define DSIPHY_DLNX_CFG(n, t) \
			(0x100 + ((t) * 0x04) + ((n) * 0x80))
#define DSIPHY_DLNX_TIMING_CTRL(n, t) \
			(0x118 + ((t) * 0x04) + ((n) * 0x80))
#define DSIPHY_DLNX_STRENGTH_CTRL(n, t) \
			(0x138 + ((t) * 0x04) + ((n) * 0x80))
#define DSIPHY_DLNX_TEST_DATAPATH(n)            (0x110 + ((n) * 0x80))
#define DSIPHY_DLNX_TEST_STR(n)                 (0x114 + ((n) * 0x80))
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

/* dynamic refresh control registers */
#define DSI_DYN_REFRESH_CTRL			0x000
#define DSI_DYN_REFRESH_PIPE_DELAY		0x004
#define DSI_DYN_REFRESH_PIPE_DELAY2		0x008
#define DSI_DYN_REFRESH_PLL_DELAY		0x00C
#define DSI_DYN_REFRESH_STATUS			0x010
#define DSI_DYN_REFRESH_PLL_CTRL0		0x014
#define DSI_DYN_REFRESH_PLL_CTRL1		0x018
#define DSI_DYN_REFRESH_PLL_CTRL2		0x01C
#define DSI_DYN_REFRESH_PLL_CTRL3		0x020
#define DSI_DYN_REFRESH_PLL_CTRL4		0x024
#define DSI_DYN_REFRESH_PLL_CTRL5		0x028
#define DSI_DYN_REFRESH_PLL_CTRL6		0x02C
#define DSI_DYN_REFRESH_PLL_CTRL7		0x030
#define DSI_DYN_REFRESH_PLL_CTRL8		0x034
#define DSI_DYN_REFRESH_PLL_CTRL9		0x038
#define DSI_DYN_REFRESH_PLL_CTRL10		0x03C
#define DSI_DYN_REFRESH_PLL_CTRL11		0x040
#define DSI_DYN_REFRESH_PLL_CTRL12		0x044
#define DSI_DYN_REFRESH_PLL_CTRL13		0x048
#define DSI_DYN_REFRESH_PLL_CTRL14		0x04C
#define DSI_DYN_REFRESH_PLL_CTRL15		0x050
#define DSI_DYN_REFRESH_PLL_CTRL16		0x054
#define DSI_DYN_REFRESH_PLL_CTRL17		0x058
#define DSI_DYN_REFRESH_PLL_CTRL18		0x05C
#define DSI_DYN_REFRESH_PLL_CTRL19		0x060
#define DSI_DYN_REFRESH_PLL_CTRL20		0x064
#define DSI_DYN_REFRESH_PLL_CTRL21		0x068
#define DSI_DYN_REFRESH_PLL_CTRL22		0x06C
#define DSI_DYN_REFRESH_PLL_CTRL23		0x070
#define DSI_DYN_REFRESH_PLL_CTRL24		0x074
#define DSI_DYN_REFRESH_PLL_CTRL25		0x078
#define DSI_DYN_REFRESH_PLL_CTRL26		0x07C
#define DSI_DYN_REFRESH_PLL_CTRL27		0x080
#define DSI_DYN_REFRESH_PLL_CTRL28		0x084
#define DSI_DYN_REFRESH_PLL_CTRL29		0x088
#define DSI_DYN_REFRESH_PLL_CTRL30		0x08C
#define DSI_DYN_REFRESH_PLL_CTRL31		0x090
#define DSI_DYN_REFRESH_PLL_UPPER_ADDR		0x094
#define DSI_DYN_REFRESH_PLL_UPPER_ADDR2		0x098

#define DSIPHY_DLN0_CFG1			0x0104
#define DSIPHY_DLN0_TIMING_CTRL_4		0x0118
#define DSIPHY_DLN0_TIMING_CTRL_5		0x011C
#define DSIPHY_DLN0_TIMING_CTRL_6		0x0120
#define DSIPHY_DLN0_TIMING_CTRL_7		0x0124
#define DSIPHY_DLN0_TIMING_CTRL_8		0x0128

#define DSIPHY_DLN1_CFG1			0x0184
#define DSIPHY_DLN1_TIMING_CTRL_4		0x0198
#define DSIPHY_DLN1_TIMING_CTRL_5		0x019C
#define DSIPHY_DLN1_TIMING_CTRL_6		0x01A0
#define DSIPHY_DLN1_TIMING_CTRL_7		0x01A4
#define DSIPHY_DLN1_TIMING_CTRL_8		0x01A8

#define DSIPHY_DLN2_CFG1			0x0204
#define DSIPHY_DLN2_TIMING_CTRL_4		0x0218
#define DSIPHY_DLN2_TIMING_CTRL_5		0x021C
#define DSIPHY_DLN2_TIMING_CTRL_6		0x0220
#define DSIPHY_DLN2_TIMING_CTRL_7		0x0224
#define DSIPHY_DLN2_TIMING_CTRL_8		0x0228

#define DSIPHY_DLN3_CFG1			0x0284
#define DSIPHY_DLN3_TIMING_CTRL_4		0x0298
#define DSIPHY_DLN3_TIMING_CTRL_5		0x029C
#define DSIPHY_DLN3_TIMING_CTRL_6		0x02A0
#define DSIPHY_DLN3_TIMING_CTRL_7		0x02A4
#define DSIPHY_DLN3_TIMING_CTRL_8		0x02A8

#define DSIPHY_CKLN_CFG1			0x0304
#define DSIPHY_CKLN_TIMING_CTRL_4		0x0318
#define DSIPHY_CKLN_TIMING_CTRL_5		0x031C
#define DSIPHY_CKLN_TIMING_CTRL_6		0x0320
#define DSIPHY_CKLN_TIMING_CTRL_7		0x0324
#define DSIPHY_CKLN_TIMING_CTRL_8		0x0328

#define DSIPHY_PLL_RESETSM_CNTRL5               0x043c

/**
 * regulator_enable() - enable regulators for DSI PHY
 * @phy:      Pointer to DSI PHY hardware object.
 * @reg_cfg:  Regulator configuration for all DSI lanes.
 */
void dsi_phy_hw_v2_0_regulator_enable(struct dsi_phy_hw *phy,
				      struct dsi_phy_per_lane_cfgs *reg_cfg)
{
	int i;
	bool is_split_link = test_bit(DSI_PHY_SPLIT_LINK, phy->feature_map);

	for (i = DSI_LOGICAL_LANE_0; i < DSI_LANE_MAX; i++)
		DSI_W32(phy, DSIPHY_DLNX_VREG_CNTRL(i), reg_cfg->lane[i][0]);

	if (is_split_link)
		DSI_W32(phy, DSIPHY_DLNX_VREG_CNTRL(DSI_LOGICAL_CLOCK_LANE+1),
				reg_cfg->lane[DSI_LOGICAL_CLOCK_LANE][0]);

	/* make sure all values are written to hardware */
	wmb();

	pr_debug("[DSI_%d] Phy regulators enabled\n", phy->index);
}

/**
 * regulator_disable() - disable regulators
 * @phy:      Pointer to DSI PHY hardware object.
 */
void dsi_phy_hw_v2_0_regulator_disable(struct dsi_phy_hw *phy)
{
	pr_debug("[DSI_%d] Phy regulators disabled\n", phy->index);
}

/**
 * enable() - Enable PHY hardware
 * @phy:      Pointer to DSI PHY hardware object.
 * @cfg:      Per lane configurations for timing, strength and lane
 *	      configurations.
 */
void dsi_phy_hw_v2_0_enable(struct dsi_phy_hw *phy,
			    struct dsi_phy_cfg *cfg)
{
	int i, j;
	struct dsi_phy_per_lane_cfgs *lanecfg = &cfg->lanecfg;
	struct dsi_phy_per_lane_cfgs *timing = &cfg->timing;
	struct dsi_phy_per_lane_cfgs *strength = &cfg->strength;
	u32 data;
	bool is_split_link = test_bit(DSI_PHY_SPLIT_LINK, phy->feature_map);

	DSI_W32(phy, DSIPHY_CMN_LDO_CNTRL, 0x1C);

	DSI_W32(phy, DSIPHY_CMN_GLBL_TEST_CTRL, 0x1);
	for (i = DSI_LOGICAL_LANE_0; i < DSI_LANE_MAX; i++) {
		for (j = 0; j < lanecfg->count_per_lane; j++)
			DSI_W32(phy, DSIPHY_DLNX_CFG(i, j),
				lanecfg->lane[i][j]);

		DSI_W32(phy, DSIPHY_DLNX_TEST_STR(i), 0x88);

		for (j = 0; j < timing->count_per_lane; j++)
			DSI_W32(phy, DSIPHY_DLNX_TIMING_CTRL(i, j),
				timing->lane[i][j]);

		for (j = 0; j < strength->count_per_lane; j++)
			DSI_W32(phy, DSIPHY_DLNX_STRENGTH_CTRL(i, j),
				strength->lane[i][j]);
	}

	if (is_split_link) {
		i = DSI_LOGICAL_CLOCK_LANE;

		for (j = 0; j < lanecfg->count_per_lane; j++)
			DSI_W32(phy, DSIPHY_DLNX_CFG(i+1, j),
				lanecfg->lane[i][j]);

		DSI_W32(phy, DSIPHY_DLNX_TEST_STR(i+1), 0x0);
		DSI_W32(phy, DSIPHY_DLNX_TEST_DATAPATH(i+1), 0x88);

		for (j = 0; j < timing->count_per_lane; j++)
			DSI_W32(phy, DSIPHY_DLNX_TIMING_CTRL(i+1, j),
				timing->lane[i][j]);

		for (j = 0; j < strength->count_per_lane; j++)
			DSI_W32(phy, DSIPHY_DLNX_STRENGTH_CTRL(i+1, j),
				strength->lane[i][j]);

		/* enable split link for cmn clk cfg1 */
		data = DSI_R32(phy, DSIPHY_CMN_CLK_CFG1);
		data |= BIT(1);
		DSI_W32(phy, DSIPHY_CMN_CLK_CFG1, data);

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
void dsi_phy_hw_v2_0_disable(struct dsi_phy_hw *phy,
			    struct dsi_phy_cfg *cfg)
{
	DSI_W32(phy, DSIPHY_PLL_CLKBUFLR_EN, 0);
	DSI_W32(phy, DSIPHY_CMN_GLBL_TEST_CTRL, 0);
	DSI_W32(phy, DSIPHY_CMN_CTRL_0, 0);
	pr_debug("[DSI_%d]Phy disabled ", phy->index);
}

/**
 * dsi_phy_hw_v2_0_idle_on() - Enable DSI PHY hardware during idle screen
 * @phy:      Pointer to DSI PHY hardware object.
 */
void dsi_phy_hw_v2_0_idle_on(struct dsi_phy_hw *phy, struct dsi_phy_cfg *cfg)
{
	int i = 0, j;
	struct dsi_phy_per_lane_cfgs *strength = &cfg->strength;
	bool is_split_link = test_bit(DSI_PHY_SPLIT_LINK, phy->feature_map);

	for (i = DSI_LOGICAL_LANE_0; i < DSI_LANE_MAX; i++) {
		for (j = 0; j < strength->count_per_lane; j++)
			DSI_W32(phy, DSIPHY_DLNX_STRENGTH_CTRL(i, j),
				strength->lane[i][j]);
	}
	if (is_split_link) {
		i = DSI_LOGICAL_CLOCK_LANE;
		for (j = 0; j < strength->count_per_lane; j++)
			DSI_W32(phy, DSIPHY_DLNX_STRENGTH_CTRL(i+1, j),
				strength->lane[i][j]);
	}

	wmb(); /* make sure write happens */
	pr_debug("[DSI_%d]Phy enabled out of idle screen\n", phy->index);
}

/**
 * dsi_phy_hw_v2_0_idle_off() - Disable DSI PHY hardware during idle screen
 * @phy:      Pointer to DSI PHY hardware object.
 */
void dsi_phy_hw_v2_0_idle_off(struct dsi_phy_hw *phy)
{
	int i = 0;
	bool is_split_link = test_bit(DSI_PHY_SPLIT_LINK, phy->feature_map);

	DSI_W32(phy, DSIPHY_CMN_CTRL_0, 0x7f);

	for (i = DSI_LOGICAL_LANE_0; i < DSI_LANE_MAX; i++)
		DSI_W32(phy, DSIPHY_DLNX_VREG_CNTRL(i), 0x1c);
	if (is_split_link)
		DSI_W32(phy, DSIPHY_DLNX_VREG_CNTRL(DSI_LOGICAL_CLOCK_LANE+1),
									0x1c);

	DSI_W32(phy, DSIPHY_CMN_LDO_CNTRL, 0x1C);

	for (i = DSI_LOGICAL_LANE_0; i < DSI_LANE_MAX; i++)
		DSI_W32(phy, DSIPHY_DLNX_STRENGTH_CTRL(i, 1), 0x0);
	if (is_split_link)
		DSI_W32(phy,
		DSIPHY_DLNX_STRENGTH_CTRL(DSI_LOGICAL_CLOCK_LANE+1, 1), 0x0);

	wmb(); /* make sure write happens */
	pr_debug("[DSI_%d]Phy disabled during idle screen\n", phy->index);
}

int dsi_phy_hw_timing_val_v2_0(struct dsi_phy_per_lane_cfgs *timing_cfg,
		u32 *timing_val, u32 size)
{
	int i = 0, j = 0;

	if (size != (DSI_LANE_MAX * DSI_MAX_SETTINGS)) {
		pr_err("Unexpected timing array size %d\n", size);
		return -EINVAL;
	}

	for (i = DSI_LOGICAL_LANE_0; i < DSI_LANE_MAX; i++) {
		for (j = 0; j < DSI_MAX_SETTINGS; j++) {
			timing_cfg->lane[i][j] = *timing_val;
			timing_val++;
		}
	}
	return 0;
}

void dsi_phy_hw_v2_0_clamp_ctrl(struct dsi_phy_hw *phy, bool enable)
{
	u32 clamp_reg = 0;

	if (!phy->phy_clamp_base) {
		pr_debug("phy_clamp_base NULL\n");
		return;
	}

	if (enable) {
		clamp_reg |= BIT(0);
		DSI_MISC_W32(phy, DSI_MDP_ULPS_CLAMP_ENABLE_OFF,
				clamp_reg);
		pr_debug("clamp enabled\n");
	} else {
		clamp_reg &= ~BIT(0);
		DSI_MISC_W32(phy, DSI_MDP_ULPS_CLAMP_ENABLE_OFF,
				clamp_reg);
		pr_debug("clamp disabled\n");
	}
}

void dsi_phy_hw_v2_0_dyn_refresh_config(struct dsi_phy_hw *phy,
		struct dsi_phy_cfg *cfg, bool is_master)
{
	u32 glbl_tst_cntrl;


	if (is_master) {
		glbl_tst_cntrl = DSI_R32(phy, DSIPHY_CMN_GLBL_TEST_CTRL);

		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL0,
				DSIPHY_CMN_GLBL_TEST_CTRL,
				DSIPHY_PLL_PLL_BANDGAP,
				glbl_tst_cntrl | BIT(1), 0x1);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL1,
				DSIPHY_PLL_RESETSM_CNTRL5,
				DSIPHY_PLL_PLL_BANDGAP, 0x0D, 0x03);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL2,
				DSIPHY_PLL_RESETSM_CNTRL5,
				DSIPHY_CMN_PLL_CNTRL, 0x1D, 0x00);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL3,
				DSIPHY_CMN_CTRL_1, DSIPHY_DLN0_CFG1, 0x20, 0);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL4,
				DSIPHY_DLN1_CFG1, DSIPHY_DLN2_CFG1, 0, 0);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL5,
				DSIPHY_DLN3_CFG1, DSIPHY_CKLN_CFG1, 0, 0);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL6,
				DSIPHY_DLN0_TIMING_CTRL_4,
				DSIPHY_DLN1_TIMING_CTRL_4,
				cfg->timing.lane[0][0], cfg->timing.lane[1][0]);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL7,
				DSIPHY_DLN2_TIMING_CTRL_4,
				DSIPHY_DLN3_TIMING_CTRL_4,
				cfg->timing.lane[2][0], cfg->timing.lane[3][0]);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL8,
				DSIPHY_CKLN_TIMING_CTRL_4,
				DSIPHY_DLN0_TIMING_CTRL_5,
				cfg->timing.lane[4][0], cfg->timing.lane[0][1]);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL9,
				DSIPHY_DLN1_TIMING_CTRL_5,
				DSIPHY_DLN2_TIMING_CTRL_5,
				cfg->timing.lane[1][1], cfg->timing.lane[2][1]);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL10,
				DSIPHY_DLN3_TIMING_CTRL_5,
				DSIPHY_CKLN_TIMING_CTRL_5,
				cfg->timing.lane[3][1], cfg->timing.lane[4][1]);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL11,
				DSIPHY_DLN0_TIMING_CTRL_6,
				DSIPHY_DLN1_TIMING_CTRL_6,
				cfg->timing.lane[0][2], cfg->timing.lane[1][2]);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL12,
				DSIPHY_DLN2_TIMING_CTRL_6,
				DSIPHY_DLN3_TIMING_CTRL_6,
				cfg->timing.lane[2][2], cfg->timing.lane[3][2]);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL13,
				DSIPHY_CKLN_TIMING_CTRL_6,
				DSIPHY_DLN0_TIMING_CTRL_7,
				cfg->timing.lane[4][2], cfg->timing.lane[0][3]);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL14,
				DSIPHY_DLN1_TIMING_CTRL_7,
				DSIPHY_DLN2_TIMING_CTRL_7,
				cfg->timing.lane[1][3], cfg->timing.lane[2][3]);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL15,
				DSIPHY_DLN3_TIMING_CTRL_7,
				DSIPHY_CKLN_TIMING_CTRL_7,
				cfg->timing.lane[3][3], cfg->timing.lane[4][3]);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL16,
				DSIPHY_DLN0_TIMING_CTRL_8,
				DSIPHY_DLN1_TIMING_CTRL_8,
				cfg->timing.lane[0][4], cfg->timing.lane[1][4]);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL17,
				DSIPHY_DLN2_TIMING_CTRL_8,
				DSIPHY_DLN3_TIMING_CTRL_8,
				cfg->timing.lane[2][4], cfg->timing.lane[3][4]);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL18,
				DSIPHY_CKLN_TIMING_CTRL_8, DSIPHY_CMN_CTRL_1,
				cfg->timing.lane[4][4], 0);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL30,
				DSIPHY_CMN_GLBL_TEST_CTRL,
				DSIPHY_CMN_GLBL_TEST_CTRL,
				((glbl_tst_cntrl) & (~BIT(2))),
				((glbl_tst_cntrl) & (~BIT(2))));
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL31,
				DSIPHY_CMN_GLBL_TEST_CTRL,
				DSIPHY_CMN_GLBL_TEST_CTRL,
				((glbl_tst_cntrl) & (~BIT(2))),
				((glbl_tst_cntrl) & (~BIT(2))));
	} else {

		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL0,
				DSIPHY_DLN0_CFG1, DSIPHY_DLN1_CFG1, 0, 0);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL1,
				DSIPHY_DLN2_CFG1, DSIPHY_DLN3_CFG1, 0x0, 0x0);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL2,
				DSIPHY_CKLN_CFG1, DSIPHY_DLN0_TIMING_CTRL_4,
				0x0, cfg->timing.lane[0][0]);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL3,
				DSIPHY_DLN1_TIMING_CTRL_4,
				DSIPHY_DLN2_TIMING_CTRL_4,
				cfg->timing.lane[1][0], cfg->timing.lane[2][0]);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL4,
				DSIPHY_DLN3_TIMING_CTRL_4,
				DSIPHY_CKLN_TIMING_CTRL_4,
				cfg->timing.lane[3][0], cfg->timing.lane[4][0]);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL5,
				DSIPHY_DLN0_TIMING_CTRL_5,
				DSIPHY_DLN1_TIMING_CTRL_5,
				cfg->timing.lane[0][1], cfg->timing.lane[1][1]);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL6,
				DSIPHY_DLN2_TIMING_CTRL_5,
				DSIPHY_DLN3_TIMING_CTRL_5,
				cfg->timing.lane[2][1], cfg->timing.lane[3][1]);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL7,
				DSIPHY_CKLN_TIMING_CTRL_5,
				DSIPHY_DLN0_TIMING_CTRL_6,
				cfg->timing.lane[4][1], cfg->timing.lane[0][2]);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL8,
				DSIPHY_DLN1_TIMING_CTRL_6,
				DSIPHY_DLN2_TIMING_CTRL_6,
				cfg->timing.lane[1][2], cfg->timing.lane[2][2]);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL9,
				DSIPHY_DLN3_TIMING_CTRL_6,
				DSIPHY_CKLN_TIMING_CTRL_6,
				cfg->timing.lane[3][2], cfg->timing.lane[4][2]);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL10,
				DSIPHY_DLN0_TIMING_CTRL_7,
				DSIPHY_DLN1_TIMING_CTRL_7,
				cfg->timing.lane[0][3], cfg->timing.lane[1][3]);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL11,
				DSIPHY_DLN2_TIMING_CTRL_7,
				DSIPHY_DLN3_TIMING_CTRL_7,
				cfg->timing.lane[2][3], cfg->timing.lane[3][3]);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL12,
				DSIPHY_CKLN_TIMING_CTRL_7,
				DSIPHY_DLN0_TIMING_CTRL_8,
				cfg->timing.lane[4][3], cfg->timing.lane[0][4]);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL13,
				DSIPHY_DLN1_TIMING_CTRL_8,
				DSIPHY_DLN2_TIMING_CTRL_8,
				cfg->timing.lane[1][4], cfg->timing.lane[2][4]);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL14,
				DSIPHY_DLN3_TIMING_CTRL_8,
				DSIPHY_CKLN_TIMING_CTRL_8,
				cfg->timing.lane[3][4], cfg->timing.lane[4][4]);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL15,
				0x0110, 0x0110, 0, 0);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL16,
				0x0110, 0x0110, 0, 0);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL17,
				0x0110, 0x0110, 0, 0);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL18,
				0x0110, 0x0110, 0, 0);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL19,
				0x0110, 0x0110, 0, 0);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL20,
				0x110, 0x110, 0, 0);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL21,
				0x110, 0x110, 0, 0);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL22,
				0x110, 0x110, 0, 0);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL23,
				0x110, 0x110, 0, 0);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL24,
				0x110, 0x110, 0, 0);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL25,
				0x110, 0x110, 0, 0);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL26,
				0x110, 0x110, 0, 0);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL27,
				0x110, 0x110, 0, 0);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL28,
				0x110, 0x110, 0, 0);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL29,
				0x110, 0x110, 0, 0);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL30,
				0x110, 0x110, 0, 0);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL31,
				0x110, 0x110, 0, 0);
		DSI_GEN_W32(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_UPPER_ADDR,
				0x0);
		DSI_GEN_W32(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_UPPER_ADDR2,
				0x0);
	}

	wmb(); /* make sure phy timings are updated*/
}

void dsi_phy_hw_v2_0_dyn_refresh_pipe_delay(struct dsi_phy_hw *phy,
		struct dsi_dyn_clk_delay *delay)
{
	if (!delay)
		return;

	DSI_GEN_W32(phy->dyn_pll_base, DSI_DYN_REFRESH_PIPE_DELAY,
			delay->pipe_delay);
	DSI_GEN_W32(phy->dyn_pll_base, DSI_DYN_REFRESH_PIPE_DELAY2,
			delay->pipe_delay2);
	DSI_GEN_W32(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_DELAY,
			delay->pll_delay);
}

void dsi_phy_hw_v2_0_dyn_refresh_helper(struct dsi_phy_hw *phy, u32 offset)
{

	u32 reg;

	/*
	 * if no offset is mentioned then this means we want to clear
	 * the dynamic refresh ctrl register which is the last step
	 * of dynamic refresh sequence.
	 */
	if (!offset) {
		reg = DSI_GEN_R32(phy->dyn_pll_base, DSI_DYN_REFRESH_CTRL);
		reg &= ~(BIT(0) | BIT(8));
		DSI_GEN_W32(phy->dyn_pll_base, DSI_DYN_REFRESH_CTRL, reg);
		wmb(); /* ensure dynamic fps is cleared */
		return;
	}

	if (offset & BIT(DYN_REFRESH_INTF_SEL)) {
		reg = DSI_GEN_R32(phy->dyn_pll_base, DSI_DYN_REFRESH_CTRL);
		reg |= BIT(13);
		DSI_GEN_W32(phy->dyn_pll_base, DSI_DYN_REFRESH_CTRL, reg);
	}

	if (offset & BIT(DYN_REFRESH_SWI_CTRL)) {
		reg = DSI_GEN_R32(phy->dyn_pll_base, DSI_DYN_REFRESH_CTRL);
		reg |= BIT(0);
		DSI_GEN_W32(phy->dyn_pll_base, DSI_DYN_REFRESH_CTRL, reg);
	}

	if (offset & BIT(DYN_REFRESH_SW_TRIGGER)) {
		reg = DSI_GEN_R32(phy->dyn_pll_base, DSI_DYN_REFRESH_CTRL);
		reg |= BIT(8);
		DSI_GEN_W32(phy->dyn_pll_base, DSI_DYN_REFRESH_CTRL, reg);
		wmb(); /* ensure dynamic fps is triggered */
	}
}

int dsi_phy_hw_v2_0_cache_phy_timings(struct dsi_phy_per_lane_cfgs *timings,
		u32 *dst, u32 size)
{
	int i, j, count = 0;

	if (!timings || !dst || !size)
		return -EINVAL;

	if (size != (DSI_LANE_MAX * DSI_MAX_SETTINGS)) {
		pr_err("size mis-match\n");
		return -EINVAL;
	}

	for (i = DSI_LOGICAL_LANE_0; i < DSI_LANE_MAX; i++) {
		for (j = 0; j < DSI_MAX_SETTINGS; j++) {
			dst[count] = timings->lane[i][j];
			count++;
		}
	}

	return 0;
}
