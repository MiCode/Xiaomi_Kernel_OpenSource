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

#define pr_fmt(fmt) "dsi-phy-hw:" fmt
#include <linux/math64.h>
#include <linux/delay.h>
#include <linux/iopoll.h>
#include "dsi_hw.h"
#include "dsi_phy_hw.h"
#include "dsi_catalog.h"

#define DSIPHY_CMN_CLK_CFG0						0x010
#define DSIPHY_CMN_CLK_CFG1						0x014
#define DSIPHY_CMN_GLBL_CTRL						0x018
#define DSIPHY_CMN_RBUF_CTRL						0x01C
#define DSIPHY_CMN_VREG_CTRL						0x020
#define DSIPHY_CMN_CTRL_0						0x024
#define DSIPHY_CMN_CTRL_1						0x028
#define DSIPHY_CMN_CTRL_2						0x02C
#define DSIPHY_CMN_LANE_CFG0						0x030
#define DSIPHY_CMN_LANE_CFG1						0x034
#define DSIPHY_CMN_PLL_CNTRL						0x038
#define DSIPHY_CMN_LANE_CTRL0						0x098
#define DSIPHY_CMN_LANE_CTRL1						0x09C
#define DSIPHY_CMN_LANE_CTRL2						0x0A0
#define DSIPHY_CMN_LANE_CTRL3						0x0A4
#define DSIPHY_CMN_LANE_CTRL4						0x0A8
#define DSIPHY_CMN_TIMING_CTRL_0					0x0AC
#define DSIPHY_CMN_TIMING_CTRL_1					0x0B0
#define DSIPHY_CMN_TIMING_CTRL_2					0x0B4
#define DSIPHY_CMN_TIMING_CTRL_3					0x0B8
#define DSIPHY_CMN_TIMING_CTRL_4					0x0BC
#define DSIPHY_CMN_TIMING_CTRL_5					0x0C0
#define DSIPHY_CMN_TIMING_CTRL_6					0x0C4
#define DSIPHY_CMN_TIMING_CTRL_7					0x0C8
#define DSIPHY_CMN_TIMING_CTRL_8					0x0CC
#define DSIPHY_CMN_TIMING_CTRL_9					0x0D0
#define DSIPHY_CMN_TIMING_CTRL_10					0x0D4
#define DSIPHY_CMN_TIMING_CTRL_11					0x0D8
#define DSIPHY_CMN_PHY_STATUS						0x0EC
#define DSIPHY_CMN_LANE_STATUS0						0x0F4
#define DSIPHY_CMN_LANE_STATUS1						0x0F8


/* n = 0..3 for data lanes and n = 4 for clock lane */
#define DSIPHY_LNX_CFG0(n)                         (0x200 + (0x80 * (n)))
#define DSIPHY_LNX_CFG1(n)                         (0x204 + (0x80 * (n)))
#define DSIPHY_LNX_CFG2(n)                         (0x208 + (0x80 * (n)))
#define DSIPHY_LNX_CFG3(n)                         (0x20C + (0x80 * (n)))
#define DSIPHY_LNX_TEST_DATAPATH(n)                (0x210 + (0x80 * (n)))
#define DSIPHY_LNX_PIN_SWAP(n)                     (0x214 + (0x80 * (n)))
#define DSIPHY_LNX_HSTX_STR_CTRL(n)                (0x218 + (0x80 * (n)))
#define DSIPHY_LNX_OFFSET_TOP_CTRL(n)              (0x21C + (0x80 * (n)))
#define DSIPHY_LNX_OFFSET_BOT_CTRL(n)              (0x220 + (0x80 * (n)))
#define DSIPHY_LNX_LPTX_STR_CTRL(n)                (0x224 + (0x80 * (n)))
#define DSIPHY_LNX_LPRX_CTRL(n)                    (0x228 + (0x80 * (n)))
#define DSIPHY_LNX_TX_DCTRL(n)                     (0x22C + (0x80 * (n)))

/* dynamic refresh control registers */
#define DSI_DYN_REFRESH_CTRL                   (0x000)
#define DSI_DYN_REFRESH_PIPE_DELAY             (0x004)
#define DSI_DYN_REFRESH_PIPE_DELAY2            (0x008)
#define DSI_DYN_REFRESH_PLL_DELAY              (0x00C)
#define DSI_DYN_REFRESH_STATUS                 (0x010)
#define DSI_DYN_REFRESH_PLL_CTRL0              (0x014)
#define DSI_DYN_REFRESH_PLL_CTRL1              (0x018)
#define DSI_DYN_REFRESH_PLL_CTRL2              (0x01C)
#define DSI_DYN_REFRESH_PLL_CTRL3              (0x020)
#define DSI_DYN_REFRESH_PLL_CTRL4              (0x024)
#define DSI_DYN_REFRESH_PLL_CTRL5              (0x028)
#define DSI_DYN_REFRESH_PLL_CTRL6              (0x02C)
#define DSI_DYN_REFRESH_PLL_CTRL7              (0x030)
#define DSI_DYN_REFRESH_PLL_CTRL8              (0x034)
#define DSI_DYN_REFRESH_PLL_CTRL9              (0x038)
#define DSI_DYN_REFRESH_PLL_CTRL10             (0x03C)
#define DSI_DYN_REFRESH_PLL_CTRL11             (0x040)
#define DSI_DYN_REFRESH_PLL_CTRL12             (0x044)
#define DSI_DYN_REFRESH_PLL_CTRL13             (0x048)
#define DSI_DYN_REFRESH_PLL_CTRL14             (0x04C)
#define DSI_DYN_REFRESH_PLL_CTRL15             (0x050)
#define DSI_DYN_REFRESH_PLL_CTRL16             (0x054)
#define DSI_DYN_REFRESH_PLL_CTRL17             (0x058)
#define DSI_DYN_REFRESH_PLL_CTRL18             (0x05C)
#define DSI_DYN_REFRESH_PLL_CTRL19             (0x060)
#define DSI_DYN_REFRESH_PLL_CTRL20             (0x064)
#define DSI_DYN_REFRESH_PLL_CTRL21             (0x068)
#define DSI_DYN_REFRESH_PLL_CTRL22             (0x06C)
#define DSI_DYN_REFRESH_PLL_CTRL23             (0x070)
#define DSI_DYN_REFRESH_PLL_CTRL24             (0x074)
#define DSI_DYN_REFRESH_PLL_CTRL25             (0x078)
#define DSI_DYN_REFRESH_PLL_CTRL26             (0x07C)
#define DSI_DYN_REFRESH_PLL_CTRL27             (0x080)
#define DSI_DYN_REFRESH_PLL_CTRL28             (0x084)
#define DSI_DYN_REFRESH_PLL_CTRL29             (0x088)
#define DSI_DYN_REFRESH_PLL_CTRL30             (0x08C)
#define DSI_DYN_REFRESH_PLL_CTRL31             (0x090)
#define DSI_DYN_REFRESH_PLL_UPPER_ADDR         (0x094)
#define DSI_DYN_REFRESH_PLL_UPPER_ADDR2        (0x098)

/**
 * regulator_enable() - enable regulators for DSI PHY
 * @phy:      Pointer to DSI PHY hardware object.
 * @reg_cfg:  Regulator configuration for all DSI lanes.
 */
void dsi_phy_hw_v3_0_regulator_enable(struct dsi_phy_hw *phy,
				      struct dsi_phy_per_lane_cfgs *reg_cfg)
{
	pr_debug("[DSI_%d] Phy regulators enabled\n", phy->index);
	/* Nothing to be done for DSI PHY regulator enable */
}

/**
 * regulator_disable() - disable regulators
 * @phy:      Pointer to DSI PHY hardware object.
 */
void dsi_phy_hw_v3_0_regulator_disable(struct dsi_phy_hw *phy)
{
	pr_debug("[DSI_%d] Phy regulators disabled\n", phy->index);
	/* Nothing to be done for DSI PHY regulator disable */
}

void dsi_phy_hw_v3_0_toggle_resync_fifo(struct dsi_phy_hw *phy)
{
	DSI_W32(phy, DSIPHY_CMN_RBUF_CTRL, 0x00);
	/* ensure that the FIFO is off */
	wmb();
	DSI_W32(phy, DSIPHY_CMN_RBUF_CTRL, 0x1);
	/* ensure that the FIFO is toggled back on */
	wmb();
}

static int dsi_phy_hw_v3_0_is_pll_on(struct dsi_phy_hw *phy)
{
	u32 data = 0;

	data = DSI_R32(phy, DSIPHY_CMN_PLL_CNTRL);
	mb(); /*make sure read happened */
	return (data & BIT(0));
}

static void dsi_phy_hw_v3_0_config_lpcdrx(struct dsi_phy_hw *phy,
	struct dsi_phy_cfg *cfg, bool enable)
{
	int phy_lane_0 = dsi_phy_conv_logical_to_phy_lane(&cfg->lane_map,
			DSI_LOGICAL_LANE_0);
	/*
	 * LPRX and CDRX need to enabled only for physical data lane
	 * corresponding to the logical data lane 0
	 */

	if (enable)
		DSI_W32(phy, DSIPHY_LNX_LPRX_CTRL(phy_lane_0),
			cfg->strength.lane[phy_lane_0][1]);
	else
		DSI_W32(phy, DSIPHY_LNX_LPRX_CTRL(phy_lane_0), 0);
}

static void dsi_phy_hw_v3_0_lane_swap_config(struct dsi_phy_hw *phy,
		struct dsi_lane_map *lane_map)
{
	DSI_W32(phy, DSIPHY_CMN_LANE_CFG0,
		(lane_map->lane_map_v2[DSI_LOGICAL_LANE_0] |
		(lane_map->lane_map_v2[DSI_LOGICAL_LANE_1] << 4)));
	DSI_W32(phy, DSIPHY_CMN_LANE_CFG1,
		(lane_map->lane_map_v2[DSI_LOGICAL_LANE_2] |
		(lane_map->lane_map_v2[DSI_LOGICAL_LANE_3] << 4)));
}

static void dsi_phy_hw_v3_0_cphy_lane_settings(struct dsi_phy_hw *phy,
			    struct dsi_phy_cfg *cfg)
{
	int i;

	/* Strength ctrl settings */
	for (i = DSI_LOGICAL_LANE_0; i < DSI_LOGICAL_LANE_3; i++) {
		DSI_W32(phy, DSIPHY_LNX_LPTX_STR_CTRL(i),
			cfg->strength.lane[i][0]);
		/*
		 * Disable LPRX and CDRX for all lanes. And later on, it will
		 * be only enabled for the physical data lane corresponding
		 * to the logical data lane 0
		 */
		DSI_W32(phy, DSIPHY_LNX_LPRX_CTRL(i), 0);
		DSI_W32(phy, DSIPHY_LNX_PIN_SWAP(i), 0x0);
		DSI_W32(phy, DSIPHY_LNX_HSTX_STR_CTRL(i), 0x88);
	}
	dsi_phy_hw_v3_0_config_lpcdrx(phy, cfg, true);

	/* other settings */
	for (i = DSI_LOGICAL_LANE_0; i < DSI_LOGICAL_LANE_3; i++) {
		DSI_W32(phy, DSIPHY_LNX_CFG0(i), cfg->lanecfg.lane[i][0]);
		DSI_W32(phy, DSIPHY_LNX_CFG1(i), cfg->lanecfg.lane[i][1]);
		DSI_W32(phy, DSIPHY_LNX_CFG2(i), cfg->lanecfg.lane[i][2]);
		DSI_W32(phy, DSIPHY_LNX_CFG3(i), 0x0A);
		DSI_W32(phy, DSIPHY_LNX_OFFSET_TOP_CTRL(i), 0x0);
		DSI_W32(phy, DSIPHY_LNX_OFFSET_BOT_CTRL(i), 0x0);
	}

	DSI_W32(phy, DSIPHY_LNX_TX_DCTRL(3), 0x02);
}

static void dsi_phy_hw_v3_0_dphy_lane_settings(struct dsi_phy_hw *phy,
			    struct dsi_phy_cfg *cfg)
{
	int i;
	u8 tx_dctrl[] = {0x00, 0x00, 0x00, 0x04, 0x01};

	/* Strength ctrl settings */
	for (i = DSI_LOGICAL_LANE_0; i < DSI_LANE_MAX; i++) {
		DSI_W32(phy, DSIPHY_LNX_LPTX_STR_CTRL(i),
			cfg->strength.lane[i][0]);
		/*
		 * Disable LPRX and CDRX for all lanes. And later on, it will
		 * be only enabled for the physical data lane corresponding
		 * to the logical data lane 0
		 */
		DSI_W32(phy, DSIPHY_LNX_LPRX_CTRL(i), 0);
		DSI_W32(phy, DSIPHY_LNX_PIN_SWAP(i), 0x0);
		DSI_W32(phy, DSIPHY_LNX_HSTX_STR_CTRL(i), 0x88);
	}
	dsi_phy_hw_v3_0_config_lpcdrx(phy, cfg, true);

	/* other settings */
	for (i = DSI_LOGICAL_LANE_0; i < DSI_LANE_MAX; i++) {
		DSI_W32(phy, DSIPHY_LNX_CFG0(i), cfg->lanecfg.lane[i][0]);
		DSI_W32(phy, DSIPHY_LNX_CFG1(i), cfg->lanecfg.lane[i][1]);
		DSI_W32(phy, DSIPHY_LNX_CFG2(i), cfg->lanecfg.lane[i][2]);
		DSI_W32(phy, DSIPHY_LNX_CFG3(i), cfg->lanecfg.lane[i][3]);
		DSI_W32(phy, DSIPHY_LNX_OFFSET_TOP_CTRL(i), 0x0);
		DSI_W32(phy, DSIPHY_LNX_OFFSET_BOT_CTRL(i), 0x0);
		DSI_W32(phy, DSIPHY_LNX_TX_DCTRL(i), tx_dctrl[i]);
	}
}

void dsi_phy_hw_v3_0_clamp_ctrl(struct dsi_phy_hw *phy, bool enable)
{
	u32 reg;

	pr_debug("enable=%s\n", enable ? "true" : "false");

	/*
	 * DSI PHY lane clamps, also referred to as PHY FreezeIO is
	 * enalbed by default as part of the initialization sequnce.
	 * This would get triggered anytime the chip FreezeIO is asserted.
	 */
	if (enable)
		return;

	/*
	 * Toggle BIT 0 to exlplictly release PHY freeze I/0 to disable
	 * the clamps.
	 */
	reg = DSI_R32(phy, DSIPHY_LNX_TX_DCTRL(3));
	DSI_W32(phy, DSIPHY_LNX_TX_DCTRL(3), reg | BIT(0));
	wmb(); /* Ensure that the freezeio bit is toggled */
	DSI_W32(phy, DSIPHY_LNX_TX_DCTRL(3), reg & ~BIT(0));
	wmb(); /* Ensure that the freezeio bit is toggled */
}

/**
 * cphy_enable() - Enable CPHY hardware
 * @phy:      Pointer to DSI PHY hardware object.
 * @cfg:      Per lane configurations for timing, strength and lane
 *	      configurations.
 */
static void dsi_phy_hw_cphy_enable(struct dsi_phy_hw *phy,
			    struct dsi_phy_cfg *cfg)
{
	struct dsi_phy_per_lane_cfgs *timing = &cfg->timing;
	u32 data;

	/* de-assert digital and pll power down */
	data = BIT(6) | BIT(5);
	DSI_W32(phy, DSIPHY_CMN_CTRL_0, data);

	/* Assert PLL core reset */
	DSI_W32(phy, DSIPHY_CMN_PLL_CNTRL, 0x00);

	/* turn off resync FIFO */
	DSI_W32(phy, DSIPHY_CMN_RBUF_CTRL, 0x00);

	DSI_W32(phy, DSIPHY_CMN_GLBL_CTRL, 0x40);

	/* Enable LDO */
	DSI_W32(phy, DSIPHY_CMN_VREG_CTRL, 0x59);

	/* Configure PHY lane swap */
	dsi_phy_hw_v3_0_lane_swap_config(phy, &cfg->lane_map);

	/* DSI PHY timings */
	DSI_W32(phy, DSIPHY_CMN_TIMING_CTRL_0, timing->lane_v3[0]);
	DSI_W32(phy, DSIPHY_CMN_TIMING_CTRL_4, timing->lane_v3[4]);
	DSI_W32(phy, DSIPHY_CMN_TIMING_CTRL_5, timing->lane_v3[5]);
	DSI_W32(phy, DSIPHY_CMN_TIMING_CTRL_6, timing->lane_v3[6]);
	DSI_W32(phy, DSIPHY_CMN_TIMING_CTRL_7, timing->lane_v3[7]);
	DSI_W32(phy, DSIPHY_CMN_TIMING_CTRL_8, timing->lane_v3[8]);
	DSI_W32(phy, DSIPHY_CMN_TIMING_CTRL_9, timing->lane_v3[9]);
	DSI_W32(phy, DSIPHY_CMN_TIMING_CTRL_10, timing->lane_v3[10]);
	DSI_W32(phy, DSIPHY_CMN_TIMING_CTRL_11, timing->lane_v3[11]);

	/* Remove power down from all blocks */
	DSI_W32(phy, DSIPHY_CMN_CTRL_0, 0x7f);

	DSI_W32(phy, DSIPHY_CMN_LANE_CTRL0, 0x07);

	switch (cfg->pll_source) {
	case DSI_PLL_SOURCE_STANDALONE:
	case DSI_PLL_SOURCE_NATIVE:
		data = 0x0; /* internal PLL */
		break;
	case DSI_PLL_SOURCE_NON_NATIVE:
		data = 0x1; /* external PLL */
		break;
	default:
		break;
	}
	DSI_W32(phy, DSIPHY_CMN_CLK_CFG1, (data << 2)); /* set PLL src */

	/* DSI lane settings */
	dsi_phy_hw_v3_0_cphy_lane_settings(phy, cfg);

	pr_debug("[DSI_%d]C-Phy enabled ", phy->index);
}


/**
 * dphy_enable() - Enable DPHY hardware
 * @phy:      Pointer to DSI PHY hardware object.
 * @cfg:      Per lane configurations for timing, strength and lane
 *	      configurations.
 */
static void dsi_phy_hw_dphy_enable(struct dsi_phy_hw *phy,
			    struct dsi_phy_cfg *cfg)
{
	struct dsi_phy_per_lane_cfgs *timing = &cfg->timing;
	u32 data;
	/* de-assert digital and pll power down */
	data = BIT(6) | BIT(5);
	DSI_W32(phy, DSIPHY_CMN_CTRL_0, data);

	/* Assert PLL core reset */
	DSI_W32(phy, DSIPHY_CMN_PLL_CNTRL, 0x00);

	/* turn off resync FIFO */
	DSI_W32(phy, DSIPHY_CMN_RBUF_CTRL, 0x00);

	/* Select MS1 byte-clk */
	DSI_W32(phy, DSIPHY_CMN_GLBL_CTRL, 0x10);

	/* Enable LDO */
	DSI_W32(phy, DSIPHY_CMN_VREG_CTRL, 0x59);

	/* Configure PHY lane swap */
	dsi_phy_hw_v3_0_lane_swap_config(phy, &cfg->lane_map);

	/* DSI PHY timings */
	DSI_W32(phy, DSIPHY_CMN_TIMING_CTRL_0, timing->lane_v3[0]);
	DSI_W32(phy, DSIPHY_CMN_TIMING_CTRL_1, timing->lane_v3[1]);
	DSI_W32(phy, DSIPHY_CMN_TIMING_CTRL_2, timing->lane_v3[2]);
	DSI_W32(phy, DSIPHY_CMN_TIMING_CTRL_3, timing->lane_v3[3]);
	DSI_W32(phy, DSIPHY_CMN_TIMING_CTRL_4, timing->lane_v3[4]);
	DSI_W32(phy, DSIPHY_CMN_TIMING_CTRL_5, timing->lane_v3[5]);
	DSI_W32(phy, DSIPHY_CMN_TIMING_CTRL_6, timing->lane_v3[6]);
	DSI_W32(phy, DSIPHY_CMN_TIMING_CTRL_7, timing->lane_v3[7]);
	DSI_W32(phy, DSIPHY_CMN_TIMING_CTRL_8, timing->lane_v3[8]);
	DSI_W32(phy, DSIPHY_CMN_TIMING_CTRL_9, timing->lane_v3[9]);
	DSI_W32(phy, DSIPHY_CMN_TIMING_CTRL_10, timing->lane_v3[10]);
	DSI_W32(phy, DSIPHY_CMN_TIMING_CTRL_11, timing->lane_v3[11]);

	/* Remove power down from all blocks */
	DSI_W32(phy, DSIPHY_CMN_CTRL_0, 0x7f);

	/*power up lanes */
	data = DSI_R32(phy, DSIPHY_CMN_CTRL_0);
	/* TODO: only power up lanes that are used */
	data |= 0x1F;
	DSI_W32(phy, DSIPHY_CMN_CTRL_0, data);
	DSI_W32(phy, DSIPHY_CMN_LANE_CTRL0, 0x1F);

	/* Select full-rate mode */
	DSI_W32(phy, DSIPHY_CMN_CTRL_2, 0x40);

	switch (cfg->pll_source) {
	case DSI_PLL_SOURCE_STANDALONE:
	case DSI_PLL_SOURCE_NATIVE:
		data = 0x0; /* internal PLL */
		break;
	case DSI_PLL_SOURCE_NON_NATIVE:
		data = 0x1; /* external PLL */
		break;
	default:
		break;
	}
	DSI_W32(phy, DSIPHY_CMN_CLK_CFG1, (data << 2)); /* set PLL src */

	/* DSI lane settings */
	dsi_phy_hw_v3_0_dphy_lane_settings(phy, cfg);

	pr_debug("[DSI_%d]D-Phy enabled", phy->index);
}

/**
 * enable() - Enable PHY hardware
 * @phy:      Pointer to DSI PHY hardware object.
 * @cfg:      Per lane configurations for timing, strength and lane
 *	      configurations.
 */
void dsi_phy_hw_v3_0_enable(struct dsi_phy_hw *phy,
			    struct dsi_phy_cfg *cfg)
{
	int rc = 0;
	u32 status;
	u32 const delay_us = 5;
	u32 const timeout_us = 1000;

	if (dsi_phy_hw_v3_0_is_pll_on(phy))
		pr_warn("PLL turned on before configuring PHY\n");

	/* wait for REFGEN READY */
	rc = readl_poll_timeout_atomic(phy->base + DSIPHY_CMN_PHY_STATUS,
		status, (status & BIT(0)), delay_us, timeout_us);
	if (rc) {
		pr_err("Ref gen not ready. Aborting\n");
		return;
	}
	if (cfg->phy_type == DSI_PHY_TYPE_CPHY)
		dsi_phy_hw_cphy_enable(phy, cfg);
	else /* Default PHY type is DPHY */
		dsi_phy_hw_dphy_enable(phy, cfg);

}

/**
 * disable() - Disable PHY hardware
 * @phy:      Pointer to DSI PHY hardware object.
 */
void dsi_phy_hw_v3_0_disable(struct dsi_phy_hw *phy,
			    struct dsi_phy_cfg *cfg)
{
	u32 data = 0;

	if (dsi_phy_hw_v3_0_is_pll_on(phy))
		pr_warn("Turning OFF PHY while PLL is on\n");

	dsi_phy_hw_v3_0_config_lpcdrx(phy, cfg, false);

	data = DSI_R32(phy, DSIPHY_CMN_CTRL_0);
	/* disable all lanes */
	data &= ~0x1F;
	DSI_W32(phy, DSIPHY_CMN_CTRL_0, data);
	DSI_W32(phy, DSIPHY_CMN_LANE_CTRL0, 0);

	/* Turn off all PHY blocks */
	DSI_W32(phy, DSIPHY_CMN_CTRL_0, 0x00);
	/* make sure phy is turned off */
	wmb();
	pr_debug("[DSI_%d]Phy disabled ", phy->index);
}

int dsi_phy_hw_v3_0_wait_for_lane_idle(
		struct dsi_phy_hw *phy, u32 lanes)
{
	int rc = 0, val = 0;
	u32 stop_state_mask = 0;
	u32 const sleep_us = 10;
	u32 const timeout_us = 100;

	stop_state_mask = BIT(4); /* clock lane */
	if (lanes & DSI_DATA_LANE_0)
		stop_state_mask |= BIT(0);
	if (lanes & DSI_DATA_LANE_1)
		stop_state_mask |= BIT(1);
	if (lanes & DSI_DATA_LANE_2)
		stop_state_mask |= BIT(2);
	if (lanes & DSI_DATA_LANE_3)
		stop_state_mask |= BIT(3);

	pr_debug("%s: polling for lanes to be in stop state, mask=0x%08x\n",
		__func__, stop_state_mask);
	rc = readl_poll_timeout(phy->base + DSIPHY_CMN_LANE_STATUS1, val,
				((val & stop_state_mask) == stop_state_mask),
				sleep_us, timeout_us);
	if (rc) {
		pr_err("%s: lanes not in stop state, LANE_STATUS=0x%08x\n",
			__func__, val);
		return rc;
	}

	return 0;
}

void dsi_phy_hw_v3_0_ulps_request(struct dsi_phy_hw *phy,
		struct dsi_phy_cfg *cfg, u32 lanes)
{
	u32 reg = 0;

	if (lanes & DSI_CLOCK_LANE)
		reg = BIT(4);
	if (lanes & DSI_DATA_LANE_0)
		reg |= BIT(0);
	if (lanes & DSI_DATA_LANE_1)
		reg |= BIT(1);
	if (lanes & DSI_DATA_LANE_2)
		reg |= BIT(2);
	if (lanes & DSI_DATA_LANE_3)
		reg |= BIT(3);

	/*
	 * ULPS entry request. Wait for short time to make sure
	 * that the lanes enter ULPS. Recommended as per HPG.
	 */
	DSI_W32(phy, DSIPHY_CMN_LANE_CTRL1, reg);
	usleep_range(100, 110);

	/* disable LPRX and CDRX */
	dsi_phy_hw_v3_0_config_lpcdrx(phy, cfg, false);
	/* disable lane LDOs */
	DSI_W32(phy, DSIPHY_CMN_VREG_CTRL, 0x19);
	pr_debug("[DSI_PHY%d] ULPS requested for lanes 0x%x\n", phy->index,
		 lanes);
}

int dsi_phy_hw_v3_0_lane_reset(struct dsi_phy_hw *phy)
{
	int ret = 0, loop = 10, u_dly = 200;
	u32 ln_status = 0;

	while ((ln_status != 0x1f) && loop) {
		DSI_W32(phy, DSIPHY_CMN_LANE_CTRL3, 0x1f);
		wmb(); /* ensure register is committed */
		loop--;
		udelay(u_dly);
		ln_status = DSI_R32(phy, DSIPHY_CMN_LANE_STATUS1);
		pr_debug("trial no: %d\n", loop);
	}

	if (!loop)
		pr_debug("could not reset phy lanes\n");

	DSI_W32(phy, DSIPHY_CMN_LANE_CTRL3, 0x0);
	wmb(); /* ensure register is committed */

	return ret;
}

void dsi_phy_hw_v3_0_ulps_exit(struct dsi_phy_hw *phy,
			struct dsi_phy_cfg *cfg, u32 lanes)
{
	u32 reg = 0;

	if (lanes & DSI_CLOCK_LANE)
		reg = BIT(4);
	if (lanes & DSI_DATA_LANE_0)
		reg |= BIT(0);
	if (lanes & DSI_DATA_LANE_1)
		reg |= BIT(1);
	if (lanes & DSI_DATA_LANE_2)
		reg |= BIT(2);
	if (lanes & DSI_DATA_LANE_3)
		reg |= BIT(3);

	/* enable lane LDOs */
	DSI_W32(phy, DSIPHY_CMN_VREG_CTRL, 0x59);
	/* enable LPRX and CDRX */
	dsi_phy_hw_v3_0_config_lpcdrx(phy, cfg, true);

	/* ULPS exit request */
	DSI_W32(phy, DSIPHY_CMN_LANE_CTRL2, reg);
	usleep_range(1000, 1010);

	/* Clear ULPS request flags on all lanes */
	DSI_W32(phy, DSIPHY_CMN_LANE_CTRL1, 0);
	/* Clear ULPS exit flags on all lanes */
	DSI_W32(phy, DSIPHY_CMN_LANE_CTRL2, 0);

	/*
	 * Sometimes when exiting ULPS, it is possible that some DSI
	 * lanes are not in the stop state which could lead to DSI
	 * commands not going through. To avoid this, force the lanes
	 * to be in stop state.
	 */
	DSI_W32(phy, DSIPHY_CMN_LANE_CTRL3, reg);
	DSI_W32(phy, DSIPHY_CMN_LANE_CTRL3, 0);
	usleep_range(100, 110);
}

u32 dsi_phy_hw_v3_0_get_lanes_in_ulps(struct dsi_phy_hw *phy)
{
	u32 lanes = 0;

	lanes = DSI_R32(phy, DSIPHY_CMN_LANE_STATUS0);
	pr_debug("[DSI_PHY%d] lanes in ulps = 0x%x\n", phy->index, lanes);
	return lanes;
}

bool dsi_phy_hw_v3_0_is_lanes_in_ulps(u32 lanes, u32 ulps_lanes)
{
	if (lanes & ulps_lanes)
		return false;

	return true;
}

int dsi_phy_hw_timing_val_v3_0(struct dsi_phy_per_lane_cfgs *timing_cfg,
		u32 *timing_val, u32 size)
{
	int i = 0;

	if (size != DSI_PHY_TIMING_V3_SIZE) {
		pr_err("Unexpected timing array size %d\n", size);
		return -EINVAL;
	}

	for (i = 0; i < size; i++)
		timing_cfg->lane_v3[i] = timing_val[i];
	return 0;
}

void dsi_phy_hw_v3_0_dyn_refresh_config(struct dsi_phy_hw *phy,
			struct dsi_phy_cfg *cfg, bool is_master, bool is_cphy)
{
	u32 reg;

	if (is_master) {
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL9,
			  DSIPHY_CMN_GLBL_CTRL, DSIPHY_CMN_VREG_CTRL,
			  is_cphy ? 0x40 : 0x10, 0x59);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL10,
			  DSIPHY_CMN_TIMING_CTRL_0, DSIPHY_CMN_TIMING_CTRL_1,
			  cfg->timing.lane_v3[0], cfg->timing.lane_v3[1]);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL11,
			  DSIPHY_CMN_TIMING_CTRL_2, DSIPHY_CMN_TIMING_CTRL_3,
			  cfg->timing.lane_v3[2], cfg->timing.lane_v3[3]);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL12,
			  DSIPHY_CMN_TIMING_CTRL_4, DSIPHY_CMN_TIMING_CTRL_5,
			  cfg->timing.lane_v3[4], cfg->timing.lane_v3[5]);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL13,
			  DSIPHY_CMN_TIMING_CTRL_6, DSIPHY_CMN_TIMING_CTRL_7,
			  cfg->timing.lane_v3[6], cfg->timing.lane_v3[7]);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL14,
			  DSIPHY_CMN_TIMING_CTRL_8, DSIPHY_CMN_TIMING_CTRL_9,
			  cfg->timing.lane_v3[8], cfg->timing.lane_v3[9]);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL15,
			  DSIPHY_CMN_TIMING_CTRL_10, DSIPHY_CMN_TIMING_CTRL_11,
			  cfg->timing.lane_v3[10], cfg->timing.lane_v3[11]);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL16,
			  DSIPHY_CMN_CTRL_0, DSIPHY_CMN_LANE_CTRL0,
			  0x7f, is_cphy ? 0x07 : 0x1f);
	} else {
		reg = DSI_R32(phy, DSIPHY_CMN_CLK_CFG0);
		reg &= ~BIT(5);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL0,
			  DSIPHY_CMN_CLK_CFG0, DSIPHY_CMN_PLL_CNTRL,
			  reg, 0x0);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL1,
			  DSIPHY_CMN_RBUF_CTRL, DSIPHY_CMN_GLBL_CTRL,
			  0x0, is_cphy ? 0x40 : 0x10);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL2,
			  DSIPHY_CMN_VREG_CTRL, DSIPHY_CMN_TIMING_CTRL_0,
			  0x59, cfg->timing.lane_v3[0]);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL3,
			  DSIPHY_CMN_TIMING_CTRL_1, DSIPHY_CMN_TIMING_CTRL_2,
			  cfg->timing.lane_v3[1], cfg->timing.lane_v3[2]);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL4,
			  DSIPHY_CMN_TIMING_CTRL_3, DSIPHY_CMN_TIMING_CTRL_4,
			  cfg->timing.lane_v3[3], cfg->timing.lane_v3[4]);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL5,
			  DSIPHY_CMN_TIMING_CTRL_5, DSIPHY_CMN_TIMING_CTRL_6,
			  cfg->timing.lane_v3[5], cfg->timing.lane_v3[6]);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL6,
			  DSIPHY_CMN_TIMING_CTRL_7, DSIPHY_CMN_TIMING_CTRL_8,
			  cfg->timing.lane_v3[7], cfg->timing.lane_v3[8]);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL7,
			  DSIPHY_CMN_TIMING_CTRL_9, DSIPHY_CMN_TIMING_CTRL_10,
			  cfg->timing.lane_v3[9], cfg->timing.lane_v3[10]);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL8,
			  DSIPHY_CMN_TIMING_CTRL_11, DSIPHY_CMN_CTRL_0,
			  cfg->timing.lane_v3[11], 0x7f);
		DSI_DYN_REF_REG_W(phy->dyn_pll_base, DSI_DYN_REFRESH_PLL_CTRL9,
			  DSIPHY_CMN_LANE_CTRL0, DSIPHY_CMN_CTRL_2,
			  is_cphy ? 0x07 : 0x1f, 0x40);
		/*
		 * fill with dummy register writes since controller will blindly
		 * send these values to DSI PHY.
		 */
		reg = DSI_DYN_REFRESH_PLL_CTRL11;
		while (reg <= DSI_DYN_REFRESH_PLL_CTRL29) {
			DSI_DYN_REF_REG_W(phy->dyn_pll_base, reg,
				  DSIPHY_CMN_LANE_CTRL0, DSIPHY_CMN_CTRL_0,
				  is_cphy ? 0x07 : 0x1f, 0x7f);
			reg += 0x4;
		}

		DSI_GEN_W32(phy->dyn_pll_base,
			    DSI_DYN_REFRESH_PLL_UPPER_ADDR, 0);
		DSI_GEN_W32(phy->dyn_pll_base,
			    DSI_DYN_REFRESH_PLL_UPPER_ADDR2, 0);
	}

	wmb(); /* make sure all registers are updated */
}

void dsi_phy_hw_v3_0_dyn_refresh_pipe_delay(struct dsi_phy_hw *phy,
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

void dsi_phy_hw_v3_0_dyn_refresh_helper(struct dsi_phy_hw *phy, u32 offset)
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

	if (offset & BIT(DYN_REFRESH_SYNC_MODE)) {
		reg = DSI_GEN_R32(phy->dyn_pll_base, DSI_DYN_REFRESH_CTRL);
		reg |= BIT(16);
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

int dsi_phy_hw_v3_0_cache_phy_timings(struct dsi_phy_per_lane_cfgs *timings,
				      u32 *dst, u32 size)
{
	int i;

	if (!timings || !dst || !size)
		return -EINVAL;

	if (size != DSI_PHY_TIMING_V3_SIZE) {
		pr_err("size mis-match\n");
		return -EINVAL;
	}

	for (i = 0; i < size; i++)
		dst[i] = timings->lane_v3[i];

	return 0;
}
