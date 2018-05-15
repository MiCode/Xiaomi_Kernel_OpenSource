/*
 * Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
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

static inline int dsi_conv_phy_to_logical_lane(
	struct dsi_lane_map *lane_map, enum dsi_phy_data_lanes phy_lane)
{
	int i = 0;

	if (phy_lane > DSI_PHYSICAL_LANE_3)
		return -EINVAL;

	for (i = DSI_LOGICAL_LANE_0; i < (DSI_LANE_MAX - 1); i++) {
		if (lane_map->lane_map_v2[i] == phy_lane)
			break;
	}
	return i;
}

static inline int dsi_conv_logical_to_phy_lane(
	struct dsi_lane_map *lane_map, enum dsi_logical_lane lane)
{
	int i = 0;

	if (lane > (DSI_LANE_MAX - 1))
		return -EINVAL;

	for (i = DSI_LOGICAL_LANE_0; i < (DSI_LANE_MAX - 1); i++) {
		if (BIT(i) == lane_map->lane_map_v2[lane])
			break;
	}
	return i;
}

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
	int phy_lane_0 = dsi_conv_logical_to_phy_lane(&cfg->lane_map,
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

static void dsi_phy_hw_v3_0_lane_settings(struct dsi_phy_hw *phy,
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
	struct dsi_phy_per_lane_cfgs *timing = &cfg->timing;
	u32 data;

	if (dsi_phy_hw_v3_0_is_pll_on(phy))
		pr_warn("PLL turned on before configuring PHY\n");

	/* wait for REFGEN READY */
	rc = readl_poll_timeout_atomic(phy->base + DSIPHY_CMN_PHY_STATUS,
		status, (status & BIT(0)), delay_us, timeout_us);
	if (rc) {
		pr_err("Ref gen not ready. Aborting\n");
		return;
	}

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
	dsi_phy_hw_v3_0_lane_settings(phy, cfg);

	pr_debug("[DSI_%d]Phy enabled ", phy->index);
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
			(val == stop_state_mask), sleep_us, timeout_us);
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
