/*
 * Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _DP_PANEL_H_
#define _DP_PANEL_H_

#include "dp_aux.h"
#include "sde_edid_parser.h"

#define DP_LINK_RATE_810	30	/* 8.10G = 270M * 30 */

struct dp_panel_info {
	u32 h_active;
	u32 v_active;
	u32 h_back_porch;
	u32 h_front_porch;
	u32 h_sync_width;
	u32 h_active_low;
	u32 v_back_porch;
	u32 v_front_porch;
	u32 v_sync_width;
	u32 v_active_low;
	u32 h_skew;
	u32 refresh_rate;
	u32 pixel_clk_khz;
	u32 bpp;
};

struct dp_panel {
	/* dpcd raw data */
	u8 dpcd[DP_RECEIVER_CAP_SIZE];
	struct drm_dp_link dp_link;

	struct sde_edid_ctrl *edid_ctrl;
	struct dp_panel_info pinfo;

	u32 vic;

	int (*sde_edid_register)(struct dp_panel *dp_panel);
	void (*sde_edid_deregister)(struct dp_panel *dp_panel);
	int (*init_info)(struct dp_panel *dp_panel);
	int (*timing_cfg)(struct dp_panel *dp_panel);
	int (*read_dpcd)(struct dp_panel *dp_panel);
	u32 (*get_link_rate)(struct dp_panel *dp_panel);
};

struct dp_panel *dp_panel_get(struct device *dev, struct dp_aux *aux,
				struct dp_catalog_panel *catalog);
void dp_panel_put(struct dp_panel *dp_panel);
#endif /* _DP_PANEL_H_ */
