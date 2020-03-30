// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
 */

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

	DSI_PHY_DBG(phy, "Phy regulators enabled\n");
}

/**
 * regulator_disable() - disable regulators
 * @phy:      Pointer to DSI PHY hardware object.
 */
void dsi_phy_hw_v2_0_regulator_disable(struct dsi_phy_hw *phy)
{
	DSI_PHY_DBG(phy, "Phy regulators disabled\n");
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

	DSI_PHY_DBG(phy, "Phy enabled\n");
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
	DSI_PHY_DBG(phy, "Phy disabled\n");
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
	DSI_PHY_DBG(phy, "Phy enabled out of idle screen\n");
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
	DSI_PHY_DBG(phy, "Phy disabled during idle screen\n");
}

int dsi_phy_hw_timing_val_v2_0(struct dsi_phy_per_lane_cfgs *timing_cfg,
		u32 *timing_val, u32 size)
{
	int i = 0, j = 0;

	if (size != (DSI_LANE_MAX * DSI_MAX_SETTINGS)) {
		DSI_ERR("Unexpected timing array size %d\n", size);
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
		DSI_PHY_DBG(phy, "phy_clamp_base NULL\n");
		return;
	}

	if (enable) {
		clamp_reg |= BIT(0);
		DSI_MISC_W32(phy, DSI_MDP_ULPS_CLAMP_ENABLE_OFF,
				clamp_reg);
		DSI_PHY_DBG(phy, "clamp enabled\n");
	} else {
		clamp_reg &= ~BIT(0);
		DSI_MISC_W32(phy, DSI_MDP_ULPS_CLAMP_ENABLE_OFF,
				clamp_reg);
		DSI_PHY_DBG(phy, "clamp disabled\n");
	}
}
