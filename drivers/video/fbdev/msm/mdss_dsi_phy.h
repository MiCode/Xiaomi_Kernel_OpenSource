/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

enum phy_rev {
	DSI_PHY_REV_UNKNOWN = 0x00,
	DSI_PHY_REV_10 = 0x01,	/* REV 1.0 - 20nm, 28nm */
	DSI_PHY_REV_20 = 0x02,	/* REV 2.0 - 14nm */
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

#endif /* MDSS_DSI_PHY_H */
