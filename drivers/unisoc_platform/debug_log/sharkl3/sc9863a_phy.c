// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <dt-bindings/soc/sprd,sharkl3-mask.h>
#include <dt-bindings/soc/sprd,sharkl3-regs.h>

#include "../core.h"
#include "phy.h"

#ifdef pr_fmt
#undef pr_fmt
#define pr_fmt(fmt) "[dsiphy] "fmt
#endif

static u32 s_phy_div1_map[] = {18, 1, 10};

static int dbg_phy_testclr(struct phy_ctx *phy, int h)
{
	if (h) {
		regmap_update_bits(phy->dsi_apb,
				   REG_ANLG_PHY_G1_ANALOG_MIPI_CSI_4LANE_MIPI_PHY_BIST_TEST,
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_4LANE_CSI_2P2L_TESTCLR_M,
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_4LANE_CSI_2P2L_TESTCLR_M);
		regmap_update_bits(phy->dsi_apb,
				   REG_ANLG_PHY_G1_ANALOG_MIPI_CSI_4LANE_MIPI_PHY_BIST_TEST,
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_4LANE_CSI_2P2L_TESTCLR_S,
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_4LANE_CSI_2P2L_TESTCLR_S);
		regmap_update_bits(phy->dsi_apb,
				   REG_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_TEST_DB,
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_DSI_TESTCLR_DB,
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_DSI_TESTCLR_DB);
	} else {
		regmap_update_bits(phy->dsi_apb,
				   REG_ANLG_PHY_G1_ANALOG_MIPI_CSI_4LANE_MIPI_PHY_BIST_TEST,
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_4LANE_CSI_2P2L_TESTCLR_M,
				   ~MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_4LANE_CSI_2P2L_TESTCLR_M);
		regmap_update_bits(phy->dsi_apb,
				   REG_ANLG_PHY_G1_ANALOG_MIPI_CSI_4LANE_MIPI_PHY_BIST_TEST,
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_4LANE_CSI_2P2L_TESTCLR_S,
				   ~MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_4LANE_CSI_2P2L_TESTCLR_S);
		regmap_update_bits(phy->dsi_apb,
				   REG_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_TEST_DB,
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_DSI_TESTCLR_DB,
				   ~MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_DSI_TESTCLR_DB);
	}
	return 0;
}

static int dbg_phy_shutdownz(struct phy_ctx *phy, int h)
{
	if (h) {
		regmap_update_bits(phy->dsi_apb,
				   REG_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_CTRL_DB,
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_DSI_SHUTDOWNZ_DB,
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_DSI_SHUTDOWNZ_DB);
		regmap_update_bits(phy->dsi_apb,
				   REG_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_CTRL_M,
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_CSI_SHUTDOWNZ_M,
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_CSI_SHUTDOWNZ_M);
		regmap_update_bits(phy->dsi_apb,
				   REG_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_CTRL_S,
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_CSI_SHUTDOWNZ_S,
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_CSI_SHUTDOWNZ_S);
	} else {
		regmap_update_bits(phy->dsi_apb,
				   REG_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_CTRL_DB,
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_DSI_SHUTDOWNZ_DB,
				   ~MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_DSI_SHUTDOWNZ_DB);
		regmap_update_bits(phy->dsi_apb,
				   REG_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_CTRL_M,
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_CSI_SHUTDOWNZ_M,
				   ~MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_CSI_SHUTDOWNZ_M);
		regmap_update_bits(phy->dsi_apb,
				   REG_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_CTRL_S,
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_CSI_SHUTDOWNZ_S,
				   ~MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_CSI_SHUTDOWNZ_S);
	}
	return 0;
}

static int dbg_phy_resetz(struct phy_ctx *phy, int h)
{
	if (h) {
		regmap_update_bits(phy->dsi_apb,
				   REG_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_CTRL_DB,
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_DSI_RSTZ_DB,
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_DSI_RSTZ_DB);
		regmap_update_bits(phy->dsi_apb,
				   REG_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_CTRL_M,
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_CSI_RSTZ_M,
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_CSI_RSTZ_M);
		regmap_update_bits(phy->dsi_apb,
				   REG_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_CTRL_S,
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_CSI_RSTZ_S,
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_CSI_RSTZ_S);
	} else {
		regmap_update_bits(phy->dsi_apb,
				   REG_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_CTRL_DB,
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_DSI_RSTZ_DB,
				   ~MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_DSI_RSTZ_DB);
		regmap_update_bits(phy->dsi_apb,
				   REG_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_CTRL_M,
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_CSI_RSTZ_M,
				   ~MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_CSI_RSTZ_M);
		regmap_update_bits(phy->dsi_apb,
				   REG_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_CTRL_S,
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_CSI_RSTZ_S,
				   ~MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_CSI_RSTZ_S);
	}
	return 0;
}

static int dbg_phy_enable_db(struct phy_ctx *phy, int h)
{
	if (h)
		regmap_update_bits(phy->dsi_apb,
				   REG_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_CTRL_DB,
				   (MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_DSI_ENABLE_3_DB |
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_DSI_ENABLE_2_DB |
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_DSI_ENABLE_1_DB |
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_DSI_ENABLE_0_DB),
				   (MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_DSI_ENABLE_3_DB |
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_DSI_ENABLE_2_DB |
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_DSI_ENABLE_1_DB |
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_DSI_ENABLE_0_DB));
	else
		regmap_update_bits(phy->dsi_apb,
				   REG_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_CTRL_DB,
				   (MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_DSI_ENABLE_3_DB |
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_DSI_ENABLE_2_DB |
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_DSI_ENABLE_1_DB |
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_DSI_ENABLE_0_DB),
				   ~(MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_DSI_ENABLE_3_DB |
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_DSI_ENABLE_2_DB |
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_DSI_ENABLE_1_DB |
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_DSI_ENABLE_0_DB));
	return 0;
}

static int dbg_phy_enableclk(struct phy_ctx *phy, int h)
{
	if (h)
		regmap_update_bits(phy->dsi_apb,
				   REG_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_CTRL_DB,
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_DSI_ENABLECLK_DB,
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_DSI_ENABLECLK_DB);
	else
		regmap_update_bits(phy->dsi_apb,
				   REG_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_CTRL_DB,
				   MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_DSI_ENABLECLK_DB,
				   ~MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_DSI_ENABLECLK_DB);
	return 0;
}

static int dbg_phy_clk_source(struct phy_ctx *phy, int h)
{
	if (h) {
		regmap_update_bits(phy->pll_apb,
				   REG_ANLG_PHY_G2_ANALOG_PLL_TOP_PLL_TO_CSI_CLK_CRTL,
				   ((0x3) << 1), ~((0x3) << 1));
		regmap_update_bits(phy->pll_apb,
				   REG_ANLG_PHY_G2_ANALOG_PLL_TOP_PLL_TO_CSI_CLK_CRTL,
				   ((phy->clk_sel & 0x3) << 1), ((phy->clk_sel & 0x3) << 1));
		regmap_update_bits(phy->pll_apb,
				   REG_ANLG_PHY_G2_ANALOG_PLL_TOP_REG_SEL_CFG_0,
				   BIT(s_phy_div1_map[phy->clk_sel]),
				   BIT(s_phy_div1_map[phy->clk_sel]));
		regmap_update_bits(phy->pll_apb,
				   REG_ANLG_PHY_G2_ANALOG_PLL_TOP_PLL_TO_CSI_CLK_CRTL,
				   MASK_ANLG_PHY_G2_ANALOG_PLL_TOP_PLL_CK2CSI_EN,
				   MASK_ANLG_PHY_G2_ANALOG_PLL_TOP_PLL_CK2CSI_EN);
	} else {
		regmap_update_bits(phy->pll_apb,
				   REG_ANLG_PHY_G2_ANALOG_PLL_TOP_REG_SEL_CFG_0,
				   BIT(s_phy_div1_map[phy->clk_sel]),
				   ~BIT(s_phy_div1_map[phy->clk_sel]));
		regmap_update_bits(phy->pll_apb,
				   REG_ANLG_PHY_G2_ANALOG_PLL_TOP_PLL_TO_CSI_CLK_CRTL,
				   MASK_ANLG_PHY_G2_ANALOG_PLL_TOP_PLL_CK2CSI_EN,
				   ~MASK_ANLG_PHY_G2_ANALOG_PLL_TOP_PLL_CK2CSI_EN);
	}
	return 0;
}

int dbg_phy_init(struct phy_ctx *phy)
{
	dbg_phy_clk_source(phy, 1);
	dbg_phy_testclr(phy, 0);
	regmap_update_bits(phy->dsi_apb,
			   REG_ANLG_PHY_G1_ANALOG_MIPI_CSI_4LANE_MIPI_PHY_BIST_TEST,
			   (MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_4LANE_CSI_2P2L_TESTCLR_S_SEL
			   | MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_4LANE_CSI_2P2L_TESTCLR_M_SEL),
			   (MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_4LANE_CSI_2P2L_TESTCLR_S_SEL
			   | MASK_ANLG_PHY_G1_ANALOG_MIPI_CSI_4LANE_CSI_2P2L_TESTCLR_M_SEL));
	dbg_phy_shutdownz(phy, 1);
	regmap_update_bits(phy->dsi_apb,
			   REG_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_REG_SEL_CFG_0,
			   (MASK_ANLG_PHY_G1_DBG_SEL_ANALOG_MIPI_CSI_2P2LANE_CSI_SHUTDOWNZ_M
			   | MASK_ANLG_PHY_G1_DBG_SEL_ANALOG_MIPI_CSI_2P2LANE_CSI_SHUTDOWNZ_S
			   | MASK_ANLG_PHY_G1_DBG_SEL_ANALOG_MIPI_CSI_2P2LANE_DSI_SHUTDOWNZ_DB),
			   (MASK_ANLG_PHY_G1_DBG_SEL_ANALOG_MIPI_CSI_2P2LANE_CSI_SHUTDOWNZ_M
			   | MASK_ANLG_PHY_G1_DBG_SEL_ANALOG_MIPI_CSI_2P2LANE_CSI_SHUTDOWNZ_S
			   | MASK_ANLG_PHY_G1_DBG_SEL_ANALOG_MIPI_CSI_2P2LANE_DSI_SHUTDOWNZ_DB));
	dbg_phy_resetz(phy, 1);
	regmap_update_bits(phy->dsi_apb,
			   REG_ANLG_PHY_G1_ANALOG_MIPI_CSI_2P2LANE_REG_SEL_CFG_0,
			   (MASK_ANLG_PHY_G1_DBG_SEL_ANALOG_MIPI_CSI_2P2LANE_CSI_RSTZ_M
			   | MASK_ANLG_PHY_G1_DBG_SEL_ANALOG_MIPI_CSI_2P2LANE_CSI_RSTZ_S),
			   (MASK_ANLG_PHY_G1_DBG_SEL_ANALOG_MIPI_CSI_2P2LANE_CSI_RSTZ_M
			   | MASK_ANLG_PHY_G1_DBG_SEL_ANALOG_MIPI_CSI_2P2LANE_CSI_RSTZ_S));
	dbg_phy_enableclk(phy, 0);
	dbg_phy_enable_db(phy, 0);
	return 0;
}

int dbg_phy_exit(struct phy_ctx *phy)
{
	dbg_phy_clk_source(phy, 0);
	return 0;
}

int dbg_phy_enable(struct phy_ctx *phy, int enable, void (*ext_ctrl) (void *),
		   void *ext_para)
{
	static int is_enabled;

	if (is_enabled == enable)
		return 0;
	is_enabled = enable;

	if (enable) {
		dbg_phy_shutdownz(phy, 0);
		dbg_phy_resetz(phy, 0);
		/* here need wait till cmd complete*/
		usleep_range(1000, 1100); /* Wait for 1mS */
		dbg_phy_testclr(phy, 1);
		/* here need wait till cmd complete*/
		usleep_range(5000, 5100); /* Wait for 5mS */
		ext_ctrl(ext_para);
		dbg_phy_testclr(phy, 0);
		/* here need wait till cmd complete*/
		usleep_range(5000, 5100); /* Wait for 5mS */
		dbg_phy_shutdownz(phy, 1);
		dbg_phy_resetz(phy, 1);
		/* here need wait till cmd complete*/
		usleep_range(5000, 5100); /* Wait for 5mS */
		dbg_phy_enableclk(phy, 1);
		dbg_phy_enable_db(phy, 1);
	} else {
		dbg_phy_enable_db(phy, 0);
		dbg_phy_enableclk(phy, 0);
	}
	return 0;
}
