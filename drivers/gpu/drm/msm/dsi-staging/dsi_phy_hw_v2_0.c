/*
 * Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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
 * regulator_enable() - enable regulators for DSI PHY
 * @phy:      Pointer to DSI PHY hardware object.
 * @reg_cfg:  Regulator configuration for all DSI lanes.
 */
void dsi_phy_hw_v2_0_regulator_enable(struct dsi_phy_hw *phy,
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
	int i = 0;

	for (i = DSI_LOGICAL_LANE_0; i < DSI_LANE_MAX; i++) {
		DSI_W32(phy, DSIPHY_DLNX_STRENGTH_CTRL_0(i),
			cfg->strength.lane[i][0]);
		DSI_W32(phy, DSIPHY_DLNX_STRENGTH_CTRL_1(i),
			cfg->strength.lane[i][1]);
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

	DSI_W32(phy, DSIPHY_CMN_CTRL_0, 0x7f);
	for (i = DSI_LOGICAL_LANE_0; i < DSI_LANE_MAX; i++)
		DSI_W32(phy, DSIPHY_DLNX_VREG_CNTRL(i), 0x1c);
	DSI_W32(phy, DSIPHY_CMN_LDO_CNTRL, 0x1C);

	for (i = DSI_LOGICAL_LANE_0; i < DSI_LANE_MAX; i++)
		DSI_W32(phy, DSIPHY_DLNX_STRENGTH_CTRL_1(i), 0x0);
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
