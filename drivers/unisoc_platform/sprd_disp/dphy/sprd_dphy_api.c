// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/kernel.h>
#include <linux/delay.h>

#include "sprd_dphy_hal.h"

static int dphy_wait_pll_locked(struct sprd_dphy *dphy)
{
	u32 i = 0;

	for (i = 0; i < 50000; i++) {
		if (dphy_hal_is_pll_locked(dphy))
			return 0;
		udelay(3);
	}

	pr_err("error: dphy pll can not be locked\n");
	return -ETIMEDOUT;
}

static int dphy_wait_datalane_stop_state(struct sprd_dphy *dphy, u8 mask)
{
	u32 i = 0;

	for (i = 0; i < 5000; i++) {
		if (dphy_hal_is_stop_state_datalane(dphy) == mask)
			return 0;
		udelay(10);
	}

	pr_err("wait datalane stop-state time out\n");
	return -ETIMEDOUT;
}

static int dphy_wait_datalane_ulps_active(struct sprd_dphy *dphy, u8 mask)
{
	u32 i = 0;

	for (i = 0; i < 5000; i++) {
		if (dphy_hal_is_ulps_active_datalane(dphy) == mask)
			return 0;
		udelay(10);
	}

	pr_err("wait datalane ulps-active time out\n");
	return -ETIMEDOUT;
}

static int dphy_wait_clklane_stop_state(struct sprd_dphy *dphy)
{
	u32 i = 0;

	for (i = 0; i < 5000; i++) {
		if (dphy_hal_is_stop_state_clklane(dphy))
			return 0;
		udelay(10);
	}

	pr_err("wait clklane stop-state time out\n");
	return -ETIMEDOUT;
}

static int dphy_wait_clklane_ulps_active(struct sprd_dphy *dphy)
{
	u32 i = 0;

	for (i = 0; i < 5000; i++) {
		if (dphy_hal_is_ulps_active_clklane(dphy))
			return 0;
		udelay(10);
	}

	pr_err("wait clklane ulps-active time out\n");
	return -ETIMEDOUT;
}

int sprd_dphy_init(struct sprd_dphy *dphy)
{
	const struct dphy_pll_ops *pll = dphy->pll;
	struct dphy_context *ctx = &dphy->ctx;
	int ret;

	pr_info("lanes : %d\n", ctx->lanes);
	pr_info("freq : %d\n", ctx->freq);

	dphy_hal_rstz(dphy, 0);
	dphy_hal_shutdownz(dphy, 0);
	dphy_hal_clklane_en(dphy, 0);

	dphy_hal_test_clr(dphy, 0);
	dphy_hal_test_clr(dphy, 1);
	dphy_hal_test_clr(dphy, 0);

	pll->pll_config(ctx);
	pll->timing_config(ctx);

	dphy_hal_shutdownz(dphy, 1);
	dphy_hal_rstz(dphy, 1);
	dphy_hal_stop_wait_time(dphy, 0x1C);
	dphy_hal_clklane_en(dphy, 1);
	dphy_hal_datalane_en(dphy);

	ret = dphy_wait_pll_locked(dphy);
	if (ret)
		return ret;

	return 0;
}

void sprd_dphy_reset(struct sprd_dphy *dphy)
{
	dphy_hal_rstz(dphy, 0);
	udelay(10);
	dphy_hal_rstz(dphy, 1);
}

void sprd_dphy_shutdown(struct sprd_dphy *dphy)
{
	dphy_hal_shutdownz(dphy, 0);
	udelay(10);
	dphy_hal_shutdownz(dphy, 1);
}

int sprd_dphy_hop_config(struct sprd_dphy *dphy, int delta, int period)
{
	const struct dphy_pll_ops *pll = dphy->pll;
	struct dphy_context *ctx = &dphy->ctx;

	if (pll->hop_config)
		return pll->hop_config(ctx, delta, period);

	return 0;
}

int sprd_dphy_ssc_en(struct sprd_dphy *dphy, bool en)
{
	const struct dphy_pll_ops *pll = dphy->pll;
	struct dphy_context *ctx = &dphy->ctx;

	if (pll->ssc_en)
		return pll->ssc_en(ctx, en);

	return 0;
}

void sprd_dphy_fini(struct sprd_dphy *dphy)
{
	dphy_hal_rstz(dphy, 0);
	dphy_hal_shutdownz(dphy, 0);
	dphy_hal_rstz(dphy, 1);
}

void sprd_dphy_data_ulps_enter(struct sprd_dphy *dphy)
{
	u8 lane_mask = (1 << dphy->ctx.lanes) - 1;

	dphy_hal_datalane_ulps_rqst(dphy, 1);
	dphy_wait_datalane_ulps_active(dphy, lane_mask);
	dphy_hal_datalane_ulps_rqst(dphy, 0);
}

void sprd_dphy_data_ulps_exit(struct sprd_dphy *dphy)
{
	u8 lane_mask = (1 << dphy->ctx.lanes) - 1;

	dphy_hal_datalane_ulps_exit(dphy, 1);
	dphy_wait_datalane_stop_state(dphy, lane_mask);
	dphy_hal_datalane_ulps_exit(dphy, 0);
}

void sprd_dphy_clk_ulps_enter(struct sprd_dphy *dphy)
{
	dphy_hal_clklane_ulps_rqst(dphy, 1);
	dphy_wait_clklane_ulps_active(dphy);
	dphy_hal_clklane_ulps_rqst(dphy, 0);
}

void sprd_dphy_clk_ulps_exit(struct sprd_dphy *dphy)
{
	dphy_hal_clklane_ulps_exit(dphy, 1);
	dphy_wait_clklane_stop_state(dphy);
	dphy_hal_clklane_ulps_exit(dphy, 0);
}

void sprd_dphy_force_pll(struct sprd_dphy *dphy, bool enable)
{
	dphy_hal_force_pll(dphy, enable);
}

void sprd_dphy_hs_clk_en(struct sprd_dphy *dphy, bool enable)
{
	dphy_hal_clk_hs_rqst(dphy, enable);

	dphy_wait_pll_locked(dphy);
}

void sprd_dphy_test_write(struct sprd_dphy *dphy, u8 address, u8 data)
{
	dphy_hal_test_en(dphy, 1);

	dphy_hal_test_din(dphy, address);

	dphy_hal_test_clk(dphy, 1);
	dphy_hal_test_clk(dphy, 0);

	dphy_hal_test_en(dphy, 0);

	dphy_hal_test_din(dphy, data);

	dphy_hal_test_clk(dphy, 1);
	dphy_hal_test_clk(dphy, 0);
}

u8 sprd_dphy_test_read(struct sprd_dphy *dphy, u8 address)
{
	dphy_hal_test_en(dphy, 1);

	dphy_hal_test_din(dphy, address);

	dphy_hal_test_clk(dphy, 1);
	dphy_hal_test_clk(dphy, 0);

	dphy_hal_test_en(dphy, 0);

	udelay(1);

	return dphy_hal_test_dout(dphy);
}

void sprd_dphy_ulps_enter(struct sprd_dphy *dphy)
{
	sprd_dphy_hs_clk_en(dphy, false);
	sprd_dphy_data_ulps_enter(dphy);
	sprd_dphy_clk_ulps_enter(dphy);
}

void sprd_dphy_ulps_exit(struct sprd_dphy *dphy)
{
	sprd_dphy_force_pll(dphy, true);
	sprd_dphy_clk_ulps_exit(dphy);
	sprd_dphy_data_ulps_exit(dphy);
	sprd_dphy_force_pll(dphy, false);
}
