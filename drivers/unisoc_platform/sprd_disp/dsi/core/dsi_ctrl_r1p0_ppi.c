// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/io.h>
#include <linux/init.h>
#include <linux/module.h>

#include "dsi_ctrl_r1p0.h"
#include "sprd_dphy.h"

/**
 * Reset D-PHY module
 * @param phy: pointer to structure
 *  which holds information about the d-phy module
 * @param reset
 */
static void dsi_phy_rstz(struct dphy_context *ctx, int level)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0x78 phy_interface_ctrl;

	phy_interface_ctrl.val = readl(&reg->PHY_INTERFACE_CTRL);
	phy_interface_ctrl.bits.rf_phy_reset_n = level;

	writel(phy_interface_ctrl.val, &reg->PHY_INTERFACE_CTRL);
}

/**
 * Power up/down D-PHY module
 * @param phy: pointer to structure
 *  which holds information about the d-phy module
 * @param enable (1: shutdown)
 */
static void dsi_phy_shutdownz(struct dphy_context *ctx, int level)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0x78 phy_interface_ctrl;

	phy_interface_ctrl.val = readl(&reg->PHY_INTERFACE_CTRL);
	phy_interface_ctrl.bits.rf_phy_shutdown = level;

	writel(phy_interface_ctrl.val, &reg->PHY_INTERFACE_CTRL);
}

/**
 * Force D-PHY PLL to stay on while in ULPS
 * @param phy: pointer to structure
 *  which holds information about the d-phy module
 * @param force (1) disable (0)
 * @note To follow the programming model, use wakeup_pll function
 */
static void dsi_phy_force_pll(struct dphy_context *ctx, int force)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0x78 phy_interface_ctrl;

	phy_interface_ctrl.val = readl(&reg->PHY_INTERFACE_CTRL);
	phy_interface_ctrl.bits.rf_phy_force_pll = force;

	writel(phy_interface_ctrl.val, &reg->PHY_INTERFACE_CTRL);
}

static void dsi_phy_clklane_ulps_rqst(struct dphy_context *ctx, int en)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0x78 phy_interface_ctrl;

	phy_interface_ctrl.val = readl(&reg->PHY_INTERFACE_CTRL);
	phy_interface_ctrl.bits.rf_phy_clk_txrequlps = en;

	writel(phy_interface_ctrl.val, &reg->PHY_INTERFACE_CTRL);
}

static void dsi_phy_clklane_ulps_exit(struct dphy_context *ctx, int en)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0x78 phy_interface_ctrl;

	phy_interface_ctrl.val = readl(&reg->PHY_INTERFACE_CTRL);
	phy_interface_ctrl.bits.rf_phy_clk_txexitulps = en;

	writel(phy_interface_ctrl.val, &reg->PHY_INTERFACE_CTRL);
}

static void dsi_phy_datalane_ulps_rqst(struct dphy_context *ctx, int en)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0x78 phy_interface_ctrl;

	phy_interface_ctrl.val = readl(&reg->PHY_INTERFACE_CTRL);
	phy_interface_ctrl.bits.rf_phy_data_txrequlps = en;

	writel(phy_interface_ctrl.val, &reg->PHY_INTERFACE_CTRL);
}

static void dsi_phy_datalane_ulps_exit(struct dphy_context *ctx, int en)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0x78 phy_interface_ctrl;

	phy_interface_ctrl.val = readl(&reg->PHY_INTERFACE_CTRL);
	phy_interface_ctrl.bits.rf_phy_data_txexitulps = en;

	writel(phy_interface_ctrl.val, &reg->PHY_INTERFACE_CTRL);
}

/**
 * Configure minimum wait period for HS transmission request after a stop state
 * @param phy pointer to structure which holds information about the d-phy
 * module
 * @param no_of_byte_cycles [in byte (lane) clock cycles]
 */
static void dsi_phy_stop_wait_time(struct dphy_context *ctx, u8 byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;

	writel(byte_cycle, &reg->PHY_MIN_STOP_TIME);
}

/**
 * Set number of active lanes
 * @param phy: pointer to structure
 *  which holds information about the d-phy module
 * @param no_of_lanes
 */
static void dsi_phy_datalane_en(struct dphy_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;

	writel(ctx->lanes - 1, &reg->PHY_LANE_NUM_CONFIG);
}

/**
 * Enable clock lane module
 * @param phy pointer to structure
 *  which holds information about the d-phy module
 * @param en
 */
static void dsi_phy_clklane_en(struct dphy_context *ctx, int en)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0x78 phy_interface_ctrl;

	phy_interface_ctrl.val = readl(&reg->PHY_INTERFACE_CTRL);
	phy_interface_ctrl.bits.rf_phy_clk_en = en;

	writel(phy_interface_ctrl.val, &reg->PHY_INTERFACE_CTRL);
}

/**
 * Request the PHY module to start transmission of high speed clock.
 * This causes the clock lane to start transmitting DDR clock on the
 * lane interconnect.
 * @param phy pointer to structure which holds information about the d-phy
 * module
 * @param enable
 * @note this function should be called explicitly by user always except for
 * transmitting
 */
static void dsi_phy_clk_hs_rqst(struct dphy_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0x74 phy_clk_lane_lp_ctrl;

	phy_clk_lane_lp_ctrl.val = readl(&reg->PHY_CLK_LANE_LP_CTRL);
	phy_clk_lane_lp_ctrl.bits.auto_clklane_ctrl_en = 0;
	phy_clk_lane_lp_ctrl.bits.phy_clklane_tx_req_hs = enable;

	writel(phy_clk_lane_lp_ctrl.val, &reg->PHY_CLK_LANE_LP_CTRL);
}

/**
 * Get D-PHY PPI status
 * @param phy pointer to structure which holds information about the d-phy
 * module
 * @param mask
 * @return status
 */
static u8 dsi_phy_is_pll_locked(struct dphy_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0x9C phy_status;

	phy_status.val = readl(&reg->PHY_STATUS);

	return phy_status.bits.phy_lock;
}

static u8 dsi_phy_is_rx_direction(struct dphy_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0x9C phy_status;

	phy_status.val = readl(&reg->PHY_STATUS);

	return phy_status.bits.phy_direction;
}

static u8 dsi_phy_is_rx_ulps_esc_lane0(struct dphy_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0x9C phy_status;

	phy_status.val = readl(&reg->PHY_STATUS);

	return phy_status.bits.phy_rxulpsesc0lane;
}

static u8 dsi_phy_is_stop_state_datalane(struct dphy_context *ctx)
{
	u8 state = 0;
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0x9C phy_status;

	phy_status.val = readl(&reg->PHY_STATUS);

	if (phy_status.bits.phy_stopstate0lane)
		state |= BIT(0);
	if (phy_status.bits.phy_stopstate1lane)
		state |= BIT(1);
	if (phy_status.bits.phy_stopstate2lane)
		state |= BIT(2);
	if (phy_status.bits.phy_stopstate3lane)
		state |= BIT(3);

	return state;
}

static u8 dsi_phy_is_stop_state_clklane(struct dphy_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0x9C phy_status;

	phy_status.val = readl(&reg->PHY_STATUS);

	return phy_status.bits.phy_stopstateclklane;
}

static u8 dsi_phy_is_ulps_active_datalane(struct dphy_context *ctx)
{
	u8 state = 0;
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0x9C phy_status;

	phy_status.val = readl(&reg->PHY_STATUS);

	if (!phy_status.bits.phy_ulpsactivenot0lane)
		state |= BIT(0);
	if (!phy_status.bits.phy_ulpsactivenot1lane)
		state |= BIT(1);
	if (!phy_status.bits.phy_ulpsactivenot2lane)
		state |= BIT(2);
	if (!phy_status.bits.phy_ulpsactivenot3lane)
		state |= BIT(3);

	return state;
}

static u8 dsi_phy_is_ulps_active_clklane(struct dphy_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0x9C phy_status;

	phy_status.val = readl(&reg->PHY_STATUS);

	return !phy_status.bits.phy_ulpsactivenotclk;
}

/**
 * @param phy pointer to structure which holds information about the d-phy
 * module
 * @param value
 */
static void dsi_phy_test_clk(struct dphy_context *ctx, u8 value)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0xF0 phy_tst_ctrl0;

	phy_tst_ctrl0.val = readl(&reg->PHY_TST_CTRL0);
	phy_tst_ctrl0.bits.phy_testclk = value;

	writel(phy_tst_ctrl0.val, &reg->PHY_TST_CTRL0);
}

/**
 * @param phy pointer to structure which holds information about the d-phy
 * module
 * @param value
 */
static void dsi_phy_test_clr(struct dphy_context *ctx, u8 value)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0xF0 phy_tst_ctrl0;

	phy_tst_ctrl0.val = readl(&reg->PHY_TST_CTRL0);
	phy_tst_ctrl0.bits.phy_testclr = value;

	writel(phy_tst_ctrl0.val, &reg->PHY_TST_CTRL0);
}

/**
 * @param phy pointer to structure which holds information about the d-phy
 * module
 * @param on_falling_edge
 */
static void dsi_phy_test_en(struct dphy_context *ctx, u8 value)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0xF4 phy_tst_ctrl1;

	phy_tst_ctrl1.val = readl(&reg->PHY_TST_CTRL1);
	phy_tst_ctrl1.bits.phy_testen = value;

	writel(phy_tst_ctrl1.val, &reg->PHY_TST_CTRL1);
}

/**
 * @param phy pointer to structure which holds information about the d-phy
 * module
 */
static u8 dsi_phy_test_dout(struct dphy_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0xF4 phy_tst_ctrl1;

	phy_tst_ctrl1.val = readl(&reg->PHY_TST_CTRL1);

	return phy_tst_ctrl1.bits.phy_testdout;
}

/**
 * @param phy pointer to structure which holds information about the d-phy
 * module
 * @param test_data
 */
static void dsi_phy_test_din(struct dphy_context *ctx, u8 data)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0xF4 phy_tst_ctrl1;

	phy_tst_ctrl1.val = readl(&reg->PHY_TST_CTRL1);
	phy_tst_ctrl1.bits.phy_testdin = data;

	writel(phy_tst_ctrl1.val, &reg->PHY_TST_CTRL1);
}

const struct dphy_ppi_ops dsi_ctrl_ppi_ops = {
	.rstz                     = dsi_phy_rstz,
	.shutdownz                = dsi_phy_shutdownz,
	.force_pll                = dsi_phy_force_pll,
	.clklane_ulps_rqst        = dsi_phy_clklane_ulps_rqst,
	.clklane_ulps_exit        = dsi_phy_clklane_ulps_exit,
	.datalane_ulps_rqst       = dsi_phy_datalane_ulps_rqst,
	.datalane_ulps_exit       = dsi_phy_datalane_ulps_exit,
	.stop_wait_time           = dsi_phy_stop_wait_time,
	.datalane_en              = dsi_phy_datalane_en,
	.clklane_en               = dsi_phy_clklane_en,
	.clk_hs_rqst              = dsi_phy_clk_hs_rqst,
	.is_pll_locked            = dsi_phy_is_pll_locked,
	.is_rx_direction          = dsi_phy_is_rx_direction,
	.is_rx_ulps_esc_lane0     = dsi_phy_is_rx_ulps_esc_lane0,
	.is_stop_state_datalane   = dsi_phy_is_stop_state_datalane,
	.is_stop_state_clklane    = dsi_phy_is_stop_state_clklane,
	.is_ulps_active_datalane  = dsi_phy_is_ulps_active_datalane,
	.is_ulps_active_clklane   = dsi_phy_is_ulps_active_clklane,
	.tst_clk                  = dsi_phy_test_clk,
	.tst_clr                  = dsi_phy_test_clr,
	.tst_en                   = dsi_phy_test_en,
	.tst_dout                 = dsi_phy_test_dout,
	.tst_din                  = dsi_phy_test_din,
	.bist_en                  = NULL,
	.is_bist_ok               = NULL,
};
