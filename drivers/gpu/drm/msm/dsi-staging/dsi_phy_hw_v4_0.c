/*
 * Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "dsi-phy-hw-v4: %s:" fmt, __func__
#include <linux/math64.h>
#include <linux/delay.h>
#include <linux/iopoll.h>
#include "dsi_hw.h"
#include "dsi_phy_hw.h"
#include "dsi_catalog.h"

#define DSIPHY_CMN_REVISION_ID0						0x000
#define DSIPHY_CMN_REVISION_ID1						0x004
#define DSIPHY_CMN_REVISION_ID2						0x008
#define DSIPHY_CMN_REVISION_ID3						0x00C
#define DSIPHY_CMN_CLK_CFG0						0x010
#define DSIPHY_CMN_CLK_CFG1						0x014
#define DSIPHY_CMN_GLBL_CTRL						0x018
#define DSIPHY_CMN_RBUF_CTRL						0x01C
#define DSIPHY_CMN_VREG_CTRL_0						0x020
#define DSIPHY_CMN_CTRL_0						0x024
#define DSIPHY_CMN_CTRL_1						0x028
#define DSIPHY_CMN_CTRL_2						0x02C
#define DSIPHY_CMN_CTRL_3						0x030
#define DSIPHY_CMN_LANE_CFG0						0x034
#define DSIPHY_CMN_LANE_CFG1						0x038
#define DSIPHY_CMN_PLL_CNTRL						0x03C
#define DSIPHY_CMN_DPHY_SOT						0x040
#define DSIPHY_CMN_LANE_CTRL0						0x0A0
#define DSIPHY_CMN_LANE_CTRL1						0x0A4
#define DSIPHY_CMN_LANE_CTRL2						0x0A8
#define DSIPHY_CMN_LANE_CTRL3						0x0AC
#define DSIPHY_CMN_LANE_CTRL4						0x0B0
#define DSIPHY_CMN_TIMING_CTRL_0					0x0B4
#define DSIPHY_CMN_TIMING_CTRL_1					0x0B8
#define DSIPHY_CMN_TIMING_CTRL_2					0x0Bc
#define DSIPHY_CMN_TIMING_CTRL_3					0x0C0
#define DSIPHY_CMN_TIMING_CTRL_4					0x0C4
#define DSIPHY_CMN_TIMING_CTRL_5					0x0C8
#define DSIPHY_CMN_TIMING_CTRL_6					0x0CC
#define DSIPHY_CMN_TIMING_CTRL_7					0x0D0
#define DSIPHY_CMN_TIMING_CTRL_8					0x0D4
#define DSIPHY_CMN_TIMING_CTRL_9					0x0D8
#define DSIPHY_CMN_TIMING_CTRL_10					0x0DC
#define DSIPHY_CMN_TIMING_CTRL_11					0x0E0
#define DSIPHY_CMN_TIMING_CTRL_12					0x0E4
#define DSIPHY_CMN_TIMING_CTRL_13					0x0E8
#define DSIPHY_CMN_GLBL_HSTX_STR_CTRL_0					0x0EC
#define DSIPHY_CMN_GLBL_HSTX_STR_CTRL_1					0x0F0
#define DSIPHY_CMN_GLBL_RESCODE_OFFSET_TOP_CTRL				0x0F4
#define DSIPHY_CMN_GLBL_RESCODE_OFFSET_BOT_CTRL				0x0F8
#define DSIPHY_CMN_GLBL_RESCODE_OFFSET_MID_CTRL				0x0FC
#define DSIPHY_CMN_GLBL_LPTX_STR_CTRL					0x100
#define DSIPHY_CMN_GLBL_PEMPH_CTRL_0					0x104
#define DSIPHY_CMN_GLBL_PEMPH_CTRL_1					0x108
#define DSIPHY_CMN_GLBL_STR_SWI_CAL_SEL_CTRL				0x10C
#define DSIPHY_CMN_VREG_CTRL_1						0x110
#define DSIPHY_CMN_CTRL_4						0x114
#define DSIPHY_CMN_PHY_STATUS						0x140
#define DSIPHY_CMN_LANE_STATUS0						0x148
#define DSIPHY_CMN_LANE_STATUS1						0x14C


/* n = 0..3 for data lanes and n = 4 for clock lane */
#define DSIPHY_LNX_CFG0(n)                         (0x200 + (0x80 * (n)))
#define DSIPHY_LNX_CFG1(n)                         (0x204 + (0x80 * (n)))
#define DSIPHY_LNX_CFG2(n)                         (0x208 + (0x80 * (n)))
#define DSIPHY_LNX_TEST_DATAPATH(n)                (0x20C + (0x80 * (n)))
#define DSIPHY_LNX_PIN_SWAP(n)                     (0x210 + (0x80 * (n)))
#define DSIPHY_LNX_LPRX_CTRL(n)                    (0x214 + (0x80 * (n)))
#define DSIPHY_LNX_TX_DCTRL(n)                     (0x218 + (0x80 * (n)))

static int dsi_phy_hw_v4_0_is_pll_on(struct dsi_phy_hw *phy)
{
	u32 data = 0;

	data = DSI_R32(phy, DSIPHY_CMN_PLL_CNTRL);
	mb(); /*make sure read happened */
	return (data & BIT(0));
}

static void dsi_phy_hw_v4_0_config_lpcdrx(struct dsi_phy_hw *phy,
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

static void dsi_phy_hw_v4_0_lane_swap_config(struct dsi_phy_hw *phy,
		struct dsi_lane_map *lane_map)
{
	DSI_W32(phy, DSIPHY_CMN_LANE_CFG0,
		(lane_map->lane_map_v2[DSI_LOGICAL_LANE_0] |
		(lane_map->lane_map_v2[DSI_LOGICAL_LANE_1] << 4)));
	DSI_W32(phy, DSIPHY_CMN_LANE_CFG1,
		(lane_map->lane_map_v2[DSI_LOGICAL_LANE_2] |
		(lane_map->lane_map_v2[DSI_LOGICAL_LANE_3] << 4)));
}

static void dsi_phy_hw_v4_0_lane_settings(struct dsi_phy_hw *phy,
			    struct dsi_phy_cfg *cfg)
{
	int i;
	u8 tx_dctrl[] = {0x00, 0x00, 0x00, 0x04, 0x01};

	/* Strength ctrl settings */
	for (i = DSI_LOGICAL_LANE_0; i < DSI_LANE_MAX; i++) {
		/*
		 * Disable LPRX and CDRX for all lanes. And later on, it will
		 * be only enabled for the physical data lane corresponding
		 * to the logical data lane 0
		 */
		DSI_W32(phy, DSIPHY_LNX_LPRX_CTRL(i), 0);
	}
	dsi_phy_hw_v4_0_config_lpcdrx(phy, cfg, true);

	/* other settings */
	for (i = DSI_LOGICAL_LANE_0; i < DSI_LANE_MAX; i++) {
		DSI_W32(phy, DSIPHY_LNX_CFG0(i), cfg->lanecfg.lane[i][0]);
		DSI_W32(phy, DSIPHY_LNX_CFG1(i), cfg->lanecfg.lane[i][1]);
		DSI_W32(phy, DSIPHY_LNX_CFG2(i), cfg->lanecfg.lane[i][2]);
		DSI_W32(phy, DSIPHY_LNX_TX_DCTRL(i), tx_dctrl[i]);
		DSI_W32(phy, DSIPHY_LNX_PIN_SWAP(i),
					(cfg->lane_pnswap >> i) & 0x1);
	}

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
	int rc = 0;
	u32 status;
	u32 const delay_us = 5;
	u32 const timeout_us = 1000;
	struct dsi_phy_per_lane_cfgs *timing = &cfg->timing;
	u32 data;
	bool less_than_1500_mhz = false;
	u32 vreg_ctrl_0 = 0;
	u32 glbl_str_swi_cal_sel_ctrl = 0;
	u32 glbl_hstx_str_ctrl_0 = 0;

	if (dsi_phy_hw_v4_0_is_pll_on(phy))
		pr_warn("PLL turned on before configuring PHY\n");

	/* wait for REFGEN READY */
	rc = readl_poll_timeout_atomic(phy->base + DSIPHY_CMN_PHY_STATUS,
		status, (status & BIT(0)), delay_us, timeout_us);
	if (rc) {
		pr_err("Ref gen not ready. Aborting\n");
		return;
	}

	/* Alter PHY configurations if data rate less than 1.5GHZ*/
	if (cfg->bit_clk_rate_hz < 1500000000)
		less_than_1500_mhz = true;
	vreg_ctrl_0 = less_than_1500_mhz ? 0x5B : 0x59;
	glbl_str_swi_cal_sel_ctrl = less_than_1500_mhz ? 0x03 : 0x00;
	glbl_hstx_str_ctrl_0 = less_than_1500_mhz ? 0x66 : 0x88;

	/* de-assert digital and pll power down */
	data = BIT(6) | BIT(5);
	DSI_W32(phy, DSIPHY_CMN_CTRL_0, data);

	/* Assert PLL core reset */
	DSI_W32(phy, DSIPHY_CMN_PLL_CNTRL, 0x00);

	/* turn off resync FIFO */
	DSI_W32(phy, DSIPHY_CMN_RBUF_CTRL, 0x00);

	/* Configure PHY lane swap */
	dsi_phy_hw_v4_0_lane_swap_config(phy, &cfg->lane_map);

	/* Enable LDO */
	DSI_W32(phy, DSIPHY_CMN_VREG_CTRL_0, vreg_ctrl_0);
	DSI_W32(phy, DSIPHY_CMN_VREG_CTRL_1, 0x5c);
	DSI_W32(phy, DSIPHY_CMN_CTRL_3, 0x00);
	DSI_W32(phy, DSIPHY_CMN_GLBL_STR_SWI_CAL_SEL_CTRL,
					glbl_str_swi_cal_sel_ctrl);
	DSI_W32(phy, DSIPHY_CMN_GLBL_HSTX_STR_CTRL_0, glbl_hstx_str_ctrl_0);
	DSI_W32(phy, DSIPHY_CMN_GLBL_PEMPH_CTRL_0, 0x00);
	DSI_W32(phy, DSIPHY_CMN_GLBL_RESCODE_OFFSET_TOP_CTRL, 0x03);
	DSI_W32(phy, DSIPHY_CMN_GLBL_RESCODE_OFFSET_BOT_CTRL, 0x3c);
	DSI_W32(phy, DSIPHY_CMN_GLBL_LPTX_STR_CTRL, 0x55);

	/* Remove power down from all blocks */
	DSI_W32(phy, DSIPHY_CMN_CTRL_0, 0x7f);

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

	/* DSI PHY timings */
	DSI_W32(phy, DSIPHY_CMN_TIMING_CTRL_0, timing->lane_v4[0]);
	DSI_W32(phy, DSIPHY_CMN_TIMING_CTRL_1, timing->lane_v4[1]);
	DSI_W32(phy, DSIPHY_CMN_TIMING_CTRL_2, timing->lane_v4[2]);
	DSI_W32(phy, DSIPHY_CMN_TIMING_CTRL_3, timing->lane_v4[3]);
	DSI_W32(phy, DSIPHY_CMN_TIMING_CTRL_4, timing->lane_v4[4]);
	DSI_W32(phy, DSIPHY_CMN_TIMING_CTRL_5, timing->lane_v4[5]);
	DSI_W32(phy, DSIPHY_CMN_TIMING_CTRL_6, timing->lane_v4[6]);
	DSI_W32(phy, DSIPHY_CMN_TIMING_CTRL_7, timing->lane_v4[7]);
	DSI_W32(phy, DSIPHY_CMN_TIMING_CTRL_8, timing->lane_v4[8]);
	DSI_W32(phy, DSIPHY_CMN_TIMING_CTRL_9, timing->lane_v4[9]);
	DSI_W32(phy, DSIPHY_CMN_TIMING_CTRL_10, timing->lane_v4[10]);
	DSI_W32(phy, DSIPHY_CMN_TIMING_CTRL_11, timing->lane_v4[11]);
	DSI_W32(phy, DSIPHY_CMN_TIMING_CTRL_12, timing->lane_v4[12]);
	DSI_W32(phy, DSIPHY_CMN_TIMING_CTRL_13, timing->lane_v4[13]);

	/* DSI lane settings */
	dsi_phy_hw_v4_0_lane_settings(phy, cfg);

	pr_debug("[DSI_%d]Phy enabled ", phy->index);
}

/**
 * disable() - Disable PHY hardware
 * @phy:      Pointer to DSI PHY hardware object.
 */
void dsi_phy_hw_v4_0_disable(struct dsi_phy_hw *phy,
			    struct dsi_phy_cfg *cfg)
{
	u32 data = 0;

	if (dsi_phy_hw_v4_0_is_pll_on(phy))
		pr_warn("Turning OFF PHY while PLL is on\n");

	dsi_phy_hw_v4_0_config_lpcdrx(phy, cfg, false);

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

void dsi_phy_hw_v4_0_toggle_resync_fifo(struct dsi_phy_hw *phy)
{
	DSI_W32(phy, DSIPHY_CMN_RBUF_CTRL, 0x00);
	/* ensure that the FIFO is off */
	wmb();
	DSI_W32(phy, DSIPHY_CMN_RBUF_CTRL, 0x1);
	/* ensure that the FIFO is toggled back on */
	wmb();
}

void dsi_phy_hw_v4_0_reset_clk_en_sel(struct dsi_phy_hw *phy)
{
	u32 data = 0;

	/*Turning off CLK_EN_SEL after retime buffer sync */
	data = DSI_R32(phy, DSIPHY_CMN_CLK_CFG1);
	data &= ~BIT(4);
	DSI_W32(phy, DSIPHY_CMN_CLK_CFG1, data);
	/* ensure that clk_en_sel bit is turned off */
	wmb();
}

int dsi_phy_hw_v4_0_wait_for_lane_idle(
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

void dsi_phy_hw_v4_0_ulps_request(struct dsi_phy_hw *phy,
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

	if (cfg->force_clk_lane_hs)
		reg |= BIT(5) | BIT(6);

	/*
	 * ULPS entry request. Wait for short time to make sure
	 * that the lanes enter ULPS. Recommended as per HPG.
	 */
	DSI_W32(phy, DSIPHY_CMN_LANE_CTRL1, reg);
	usleep_range(100, 110);

	/* disable LPRX and CDRX */
	dsi_phy_hw_v4_0_config_lpcdrx(phy, cfg, false);

	pr_debug("[DSI_PHY%d] ULPS requested for lanes 0x%x\n", phy->index,
		 lanes);
}

int dsi_phy_hw_v4_0_lane_reset(struct dsi_phy_hw *phy)
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

void dsi_phy_hw_v4_0_ulps_exit(struct dsi_phy_hw *phy,
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

	/* enable LPRX and CDRX */
	dsi_phy_hw_v4_0_config_lpcdrx(phy, cfg, true);

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

	if (cfg->force_clk_lane_hs) {
		reg = BIT(5) | BIT(6);
		DSI_W32(phy, DSIPHY_CMN_LANE_CTRL1, reg);
	}
}

u32 dsi_phy_hw_v4_0_get_lanes_in_ulps(struct dsi_phy_hw *phy)
{
	u32 lanes = 0;

	lanes = DSI_R32(phy, DSIPHY_CMN_LANE_STATUS0);
	pr_debug("[DSI_PHY%d] lanes in ulps = 0x%x\n", phy->index, lanes);
	return lanes;
}

bool dsi_phy_hw_v4_0_is_lanes_in_ulps(u32 lanes, u32 ulps_lanes)
{
	if (lanes & ulps_lanes)
		return false;

	return true;
}

int dsi_phy_hw_timing_val_v4_0(struct dsi_phy_per_lane_cfgs *timing_cfg,
		u32 *timing_val, u32 size)
{
	int i = 0;

	if (size != DSI_PHY_TIMING_V4_SIZE) {
		pr_err("Unexpected timing array size %d\n", size);
		return -EINVAL;
	}

	for (i = 0; i < size; i++)
		timing_cfg->lane_v4[i] = timing_val[i];
	return 0;
}

void dsi_phy_hw_v4_0_set_continuous_clk(struct dsi_phy_hw *phy, bool enable)
{
	u32 reg = 0;

	reg = DSI_R32(phy, DSIPHY_CMN_LANE_CTRL1);

	if (enable)
		reg |= BIT(5) | BIT(6);
	else
		reg &= ~(BIT(5) | BIT(6));

	DSI_W32(phy, DSIPHY_CMN_LANE_CTRL1, reg);
	wmb(); /* make sure request is set */
}
