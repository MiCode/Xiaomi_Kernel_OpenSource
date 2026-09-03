/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _SPRD_DPHY_HAL_H_
#define _SPRD_DPHY_HAL_H_

#include "sprd_dphy.h"

/*
 * Reset D-PHY module
 * @param dphy: pointer to structure
 *  which holds information about the d-dphy module
 * @param reset
 */
static inline void dphy_hal_rstz(struct sprd_dphy *dphy, int level)
{
	const struct dphy_ppi_ops *ppi = dphy->ppi;
	struct dphy_context *ctx = &dphy->ctx;

	if (ppi->rstz)
		ppi->rstz(ctx, level);
}

/*
 * Power up/down D-PHY module
 * @param dphy: pointer to structure
 *  which holds information about the d-dphy module
 * @param enable (1: shutdown)
 */
static inline void dphy_hal_shutdownz(struct sprd_dphy *dphy, int level)
{
	const struct dphy_ppi_ops *ppi = dphy->ppi;
	struct dphy_context *ctx = &dphy->ctx;

	if (ppi->shutdownz)
		ppi->shutdownz(ctx, level);
}

/*
 * Force D-PHY PLL to stay on while in ULPS
 * @param dphy: pointer to structure
 *  which holds information about the d-dphy module
 * @param force (1) disable (0)
 * @note To follow the programming model, use wakeup_pll function
 */
static inline void dphy_hal_force_pll(struct sprd_dphy *dphy, int en)
{
	const struct dphy_ppi_ops *ppi = dphy->ppi;
	struct dphy_context *ctx = &dphy->ctx;

	if (ppi->force_pll)
		ppi->force_pll(ctx, en);
}

static inline void dphy_hal_clklane_ulps_rqst(struct sprd_dphy *dphy, int en)
{
	const struct dphy_ppi_ops *ppi = dphy->ppi;
	struct dphy_context *ctx = &dphy->ctx;

	if (ppi->clklane_ulps_rqst)
		ppi->clklane_ulps_rqst(ctx, en);
}

static inline void dphy_hal_clklane_ulps_exit(struct sprd_dphy *dphy, int en)
{
	const struct dphy_ppi_ops *ppi = dphy->ppi;
	struct dphy_context *ctx = &dphy->ctx;

	if (ppi->clklane_ulps_exit)
		ppi->clklane_ulps_exit(ctx, en);
}

static inline void dphy_hal_datalane_ulps_rqst(struct sprd_dphy *dphy, int en)
{
	const struct dphy_ppi_ops *ppi = dphy->ppi;
	struct dphy_context *ctx = &dphy->ctx;

	if (ppi->datalane_ulps_rqst)
		ppi->datalane_ulps_rqst(ctx, en);
}

static inline void dphy_hal_datalane_ulps_exit(struct sprd_dphy *dphy, int en)
{
	const struct dphy_ppi_ops *ppi = dphy->ppi;
	struct dphy_context *ctx = &dphy->ctx;

	if (ppi->datalane_ulps_exit)
		ppi->datalane_ulps_exit(ctx, en);
}

/*
 * Configure minimum wait period for HS transmission request after a stop state
 * @param dphy pointer to structure which holds information about the d-dphy
 * module
 * @param no_of_byte_cycles [in byte (lane) clock cycles]
 */
static inline void dphy_hal_stop_wait_time(struct sprd_dphy *dphy, u8 cycles)
{
	const struct dphy_ppi_ops *ppi = dphy->ppi;
	struct dphy_context *ctx = &dphy->ctx;

	if (ppi->stop_wait_time)
		ppi->stop_wait_time(ctx, cycles);
}

/*
 * Set number of active lanes
 * @param dphy: pointer to structure
 *  which holds information about the d-dphy module
 * @param no_of_lanes
 */
static inline void dphy_hal_datalane_en(struct sprd_dphy *dphy)
{
	const struct dphy_ppi_ops *ppi = dphy->ppi;
	struct dphy_context *ctx = &dphy->ctx;

	if (ppi->datalane_en)
		ppi->datalane_en(ctx);
}

/*
 * Enable clock lane module
 * @param dphy pointer to structure
 *  which holds information about the d-dphy module
 * @param en
 */
static inline void dphy_hal_clklane_en(struct sprd_dphy *dphy, int en)
{
	const struct dphy_ppi_ops *ppi = dphy->ppi;
	struct dphy_context *ctx = &dphy->ctx;

	if (ppi->clklane_en)
		ppi->clklane_en(ctx, en);
}

/*
 * Request the PHY module to start transmission of high speed clock.
 * This causes the clock lane to start transmitting DDR clock on the
 * lane interconnect.
 * @param dphy pointer to structure which holds information about the d-dphy
 * module
 * @param enable
 * @note this function should be called explicitly by user always except for
 * transmitting
 */
static inline void dphy_hal_clk_hs_rqst(struct sprd_dphy *dphy, int en)
{
	const struct dphy_ppi_ops *ppi = dphy->ppi;
	struct dphy_context *ctx = &dphy->ctx;

	if (ppi->clk_hs_rqst)
		ppi->clk_hs_rqst(ctx, en);
}

/*
 * Get D-PHY PPI status
 * @param dphy pointer to structure which holds information about the d-dphy
 * module
 * @param mask
 * @return status
 */
static inline u8 dphy_hal_is_pll_locked(struct sprd_dphy *dphy)
{
	const struct dphy_ppi_ops *ppi = dphy->ppi;
	struct dphy_context *ctx = &dphy->ctx;

	if (!ppi->is_pll_locked)
		return 1;

	return ppi->is_pll_locked(ctx);
}

static inline u8 dphy_hal_is_rx_direction(struct sprd_dphy *dphy)
{
	const struct dphy_ppi_ops *ppi = dphy->ppi;
	struct dphy_context *ctx = &dphy->ctx;

	if (!ppi->is_rx_direction)
		return 0;

	return ppi->is_rx_direction(ctx);
}

static inline u8 dphy_hal_is_rx_ulps_esc_lane0(struct sprd_dphy *dphy)
{
	const struct dphy_ppi_ops *ppi = dphy->ppi;
	struct dphy_context *ctx = &dphy->ctx;

	if (!ppi->is_rx_ulps_esc_lane0)
		return 0;

	return ppi->is_rx_ulps_esc_lane0(ctx);
}

static inline u8 dphy_hal_is_stop_state_clklane(struct sprd_dphy *dphy)
{
	const struct dphy_ppi_ops *ppi = dphy->ppi;
	struct dphy_context *ctx = &dphy->ctx;

	if (!ppi->is_stop_state_clklane)
		return 1;

	return ppi->is_stop_state_clklane(ctx);
}

static inline u8 dphy_hal_is_stop_state_datalane(struct sprd_dphy *dphy)
{
	const struct dphy_ppi_ops *ppi = dphy->ppi;
	struct dphy_context *ctx = &dphy->ctx;

	if (!ppi->is_stop_state_datalane)
		return 1;

	return ppi->is_stop_state_datalane(ctx);
}

static inline u8 dphy_hal_is_ulps_active_clklane(struct sprd_dphy *dphy)
{
	const struct dphy_ppi_ops *ppi = dphy->ppi;
	struct dphy_context *ctx = &dphy->ctx;

	if (!ppi->is_ulps_active_clklane)
		return 1;

	return ppi->is_ulps_active_clklane(ctx);
}

static inline u8 dphy_hal_is_ulps_active_datalane(struct sprd_dphy *dphy)
{
	const struct dphy_ppi_ops *ppi = dphy->ppi;
	struct dphy_context *ctx = &dphy->ctx;

	if (!ppi->is_ulps_active_datalane)
		return 1;

	return ppi->is_ulps_active_datalane(ctx);
}

/*
 * @param dphy pointer to structure which holds information about the d-dphy
 * module
 * @param value
 */
static inline void dphy_hal_test_clk(struct sprd_dphy *dphy, u8 level)
{
	const struct dphy_ppi_ops *ppi = dphy->ppi;
	struct dphy_context *ctx = &dphy->ctx;

	if (ppi->tst_clk)
		ppi->tst_clk(ctx, level);
}

/*
 * @param dphy pointer to structure which holds information about the d-dphy
 * module
 * @param value
 */
static inline void dphy_hal_test_clr(struct sprd_dphy *dphy, u8 level)
{
	const struct dphy_ppi_ops *ppi = dphy->ppi;
	struct dphy_context *ctx = &dphy->ctx;

	if (ppi->tst_clr)
		ppi->tst_clr(ctx, level);
}

/*
 * @param dphy pointer to structure which holds information about the d-dphy
 * module
 * @param on_falling_edge
 */
static inline void dphy_hal_test_en(struct sprd_dphy *dphy, u8 level)
{
	const struct dphy_ppi_ops *ppi = dphy->ppi;
	struct dphy_context *ctx = &dphy->ctx;

	if (ppi->tst_en)
		ppi->tst_en(ctx, level);
}

/*
 * @param dphy pointer to structure which holds information about the d-dphy
 * module
 */
static inline u8 dphy_hal_test_dout(struct sprd_dphy *dphy)
{
	const struct dphy_ppi_ops *ppi = dphy->ppi;
	struct dphy_context *ctx = &dphy->ctx;

	if (ppi->tst_dout)
		return ppi->tst_dout(ctx);

	return 0;
}

/*
 * @param dphy pointer to structure which holds information about the d-dphy
 * module
 * @param test_data
 */
static inline void dphy_hal_test_din(struct sprd_dphy *dphy, u8 data)
{
	const struct dphy_ppi_ops *ppi = dphy->ppi;
	struct dphy_context *ctx = &dphy->ctx;

	if (ppi->tst_din)
		ppi->tst_din(ctx, data);
}

static inline void dphy_hal_bist_en(struct sprd_dphy *dphy, int en)
{
	const struct dphy_ppi_ops *ppi = dphy->ppi;
	struct dphy_context *ctx = &dphy->ctx;

	if (ppi->bist_en)
		ppi->bist_en(ctx, en);
}

static inline u8 dphy_hal_is_bist_ok(struct sprd_dphy *dphy)
{
	const struct dphy_ppi_ops *ppi = dphy->ppi;
	struct dphy_context *ctx = &dphy->ctx;

	if (!ppi->is_bist_ok)
		return 1;

	return ppi->is_bist_ok(ctx);
}

#endif /* _SPRD_DPHY_HAL_H_ */
