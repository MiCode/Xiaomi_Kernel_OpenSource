/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2015-2018, 2020-2021, The Linux Foundation. All rights reserved. */

#ifndef MDSS_DSI_PHY_H
#define MDSS_DSI_PHY_H

#include <linux/types.h>

#include "mdss_panel.h"
#include "mdss_dsi.h"

enum phy_rev {
	DSI_PHY_REV_UNKNOWN = 0x00,
	DSI_PHY_REV_10 = 0x01,	/* REV 1.0 - 20nm, 28nm */
	DSI_PHY_REV_20 = 0x02,	/* REV 2.0 - 14nm */
	DSI_PHY_REV_30 = 0x03,  /* REV 3.0 */
	DSI_PHY_REV_12NM = 0x04, /* 12nm PHY */
	DSI_PHY_REV_MAX,
};

enum phy_mode {
	DSI_PHY_MODE_DPHY = 0x00,
	DSI_PHY_MODE_CPHY,
	DSI_PHY_MODE_MAX,
};

/*
 * mdss_dsi_phy_calc_timing_param() - calculates clock timing and hs timing
 *				parameters for the given phy revision.
 *
 * @pinfo - structure containing panel specific information which will be
 *		used in calculating the phy timing parameters.
 * @phy_rev - phy revision for which phy timings need to be calculated.
 * @frate_hz - Frame rate for which phy timing parameters are to be calculated.
 */
int mdss_dsi_phy_calc_timing_param(struct mdss_panel_info *pinfo, u32 phy_rev,
		u64 clk_rate);

/*
 * mdss_dsi_phy_v3_init() - initialization sequence for DSI PHY rev v3
 *
 * @ctrl: pointer to DSI controller structure
 * @phy_mode - DSI phy operating mode (CPHY or DPHY)
 *
 * This function performs a sequence of register writes to initialize DSI
 * phy revision 3.0 in either the C-PHY or the D-PHY operating mode. This
 * function assumes that the DSI bus clocks are turned on. This function should
 * only be called prior to enabling the DSI link clocks.
 */
int mdss_dsi_phy_v3_init(struct mdss_dsi_ctrl_pdata *ctrl,
			       enum phy_mode phy_mode);

/*
 * mdss_dsi_phy_v3_shutdown() - shutdown sequence for DSI PHY rev v3
 *
 * @ctrl: pointer to DSI controller structure
 *
 * Perform a sequence of register writes to completely shut down DSI PHY
 * revision 3.0. This function assumes that the DSI bus clocks are turned on.
 */
int mdss_dsi_phy_v3_shutdown(struct mdss_dsi_ctrl_pdata *ctrl);

/*
 * mdss_dsi_phy_v3_regulator_enable() - enable lane regulators for DSI PHY v3
 *
 * @ctrl: pointer to DSI controller structure
 */
int mdss_dsi_phy_v3_regulator_enable(struct mdss_dsi_ctrl_pdata *ctrl);

/*
 * mdss_dsi_phy_v3_regulator_disable() - disable lane regulators for DSI PHY v3
 *
 * @ctrl: pointer to DSI controller structure
 */
int mdss_dsi_phy_v3_regulator_disable(struct mdss_dsi_ctrl_pdata *ctrl);

/*
 * mdss_dsi_phy_v3_toggle_resync_fifo() - toggle resync re-time FIFO
 *
 * @ctrl: pointer to DSI controller structure
 *
 * Resync the re-time FIFO in the DSI PHY by turning it off and turning
 * it back on.
 */
void mdss_dsi_phy_v3_toggle_resync_fifo(struct mdss_dsi_ctrl_pdata *ctrl);

/**
 * mdss_dsi_phy_v3_wait_for_lanes_stop_state() - Wait for DSI lanes to be in
 *						 stop state
 * @ctrl: pointer to DSI controller structure
 * @lane_status: value of lane status register at the end of the poll
 *
 * This function waits for all the active DSI lanes to be in stop state by
 * polling the lane status register. This function assumes that the bus clocks
 * required to access the registers are already turned on.
 */
int mdss_dsi_phy_v3_wait_for_lanes_stop_state(struct mdss_dsi_ctrl_pdata *ctrl,
	u32 *lane_status);

/**
 * mdss_dsi_phy_v3_ulps_config() - Program DSI lanes to enter/exit ULPS mode
 * @ctrl: pointer to DSI controller structure
 * @enable: true to enter ULPS, false to exit ULPS
 *
 * This function executes the necessary hardware programming sequence to
 * enter/exit DSI Ultra-Low Power State (ULPS) for DSI PHY v3. This function
 * assumes that the link and core clocks are already on.
 */
int mdss_dsi_phy_v3_ulps_config(struct mdss_dsi_ctrl_pdata *ctrl, bool enable);

/**
 * mdss_dsi_phy_v3_idle_pc_exit() - Called after Idle Power Collapse exit
 * @ctrl: pointer to DSI controller structure
 *
 * This function is called after Idle Power Collapse, so driver
 * can perform any sequence required after the Idle PC exit.
 */
void mdss_dsi_phy_v3_idle_pc_exit(struct mdss_dsi_ctrl_pdata *ctrl);

/**
 * mdss_dsi_12nm_phy_regulator_enable() - enable lane reg for DSI 12nm PHY
 *
 * @ctrl: pointer to DSI controller structure
 */
int mdss_dsi_12nm_phy_regulator_enable(struct mdss_dsi_ctrl_pdata *ctrl);

/*
 * mdss_dsi_12nm_phy_regulator_disable() - disable lane reg for DSI 12nm PHY
 *
 * @ctrl: pointer to DSI controller structure
 */
int mdss_dsi_12nm_phy_regulator_disable(struct mdss_dsi_ctrl_pdata *ctrl);

/*
 * mdss_dsi_12nm_phy_config() - initialization sequence for DSI 12nm PHY
 *
 * @ctrl: pointer to DSI controller structure
 *
 * This function performs a sequence of register writes to initialize DSI
 * 12nm phy. This function assumes that the DSI bus clocks are turned on.
 * This function should only be called prior to enabling the DSI link clocks.
 */
int mdss_dsi_12nm_phy_config(struct mdss_dsi_ctrl_pdata *ctrl);

/*
 * mdss_dsi_12nm_phy_shutdown() - shutdown sequence for DSI 12nm PHY
 *
 * @ctrl: pointer to DSI controller structure
 *
 * Perform a sequence of register writes to completely shut down DSI 12nm PHY.
 * This function assumes that the DSI bus clocks are turned on.
 */
int mdss_dsi_12nm_phy_shutdown(struct mdss_dsi_ctrl_pdata *ctrl);

/*
 * mdss_dsi_12nm_phy_hstx_drv_ctrl() - enable/disable HSTX drivers
 *
 * @ctrl: pointer to DSI controller structure
 * @enable: boolean to specify enable/disable the HSTX drivers
 *
 * Perform a sequence of register writes to enable/disable HSTX drivers.
 * This function assumes that the DSI bus clocks are turned on.
 */

void mdss_dsi_12nm_phy_hstx_drv_ctrl(
	struct mdss_dsi_ctrl_pdata *ctrl, bool enable);


#endif /* MDSS_DSI_PHY_H */
