/* Copyright (c) 2015, 2018, The Linux Foundation. All rights reserved.
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

#ifndef MDSS_DSI_PHY_H
#define MDSS_DSI_PHY_H

#include <linux/types.h>

#include "mdss_panel.h"
#include "mdss_dsi.h"

enum phy_rev {
	DSI_PHY_REV_UNKNOWN = 0x00,
	DSI_PHY_REV_10 = 0x01,	/* REV 1.0 - 20nm, 28nm */
	DSI_PHY_REV_20 = 0x02,	/* REV 2.0 - 14nm */
	DSI_PHY_REV_12NM = 0x03, /* 12nm PHY */
	DSI_PHY_REV_MAX,
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
		u32 frate_hz);

/*
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
