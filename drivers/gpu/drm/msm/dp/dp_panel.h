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

#include <drm/msm_drm.h>

#include "dp_aux.h"
#include "dp_link.h"
#include "dp_usbpd.h"
#include "sde_edid_parser.h"

enum dp_lane_count {
	DP_LANE_COUNT_1	= 1,
	DP_LANE_COUNT_2	= 2,
	DP_LANE_COUNT_4	= 4,
};

#define DP_MAX_DOWNSTREAM_PORTS 0x10

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

struct dp_display_mode {
	struct dp_panel_info timing;
	u32 capabilities;
};

struct dp_panel_in {
	struct device *dev;
	struct dp_aux *aux;
	struct dp_link *link;
	struct dp_catalog_panel *catalog;
};

struct dp_panel {
	/* dpcd raw data */
	u8 dpcd[DP_RECEIVER_CAP_SIZE + 1];
	u8 ds_ports[DP_MAX_DOWNSTREAM_PORTS];

	struct drm_dp_link link_info;
	struct sde_edid_ctrl *edid_ctrl;
	struct dp_panel_info pinfo;
	bool video_test;
	bool spd_enabled;

	u32 vic;
	u32 max_pclk_khz;

	/* debug */
	u32 max_bw_code;

	int (*init)(struct dp_panel *dp_panel);
	int (*deinit)(struct dp_panel *dp_panel);
	int (*timing_cfg)(struct dp_panel *dp_panel);
	int (*read_sink_caps)(struct dp_panel *dp_panel,
		struct drm_connector *connector);
	u32 (*get_min_req_link_rate)(struct dp_panel *dp_panel);
	u32 (*get_mode_bpp)(struct dp_panel *dp_panel, u32 mode_max_bpp,
			u32 mode_pclk_khz);
	int (*get_modes)(struct dp_panel *dp_panel,
		struct drm_connector *connector, struct dp_display_mode *mode);
	void (*handle_sink_request)(struct dp_panel *dp_panel);
	int (*set_edid)(struct dp_panel *dp_panel, u8 *edid);
	int (*set_dpcd)(struct dp_panel *dp_panel, u8 *dpcd);
	int (*setup_hdr)(struct dp_panel *dp_panel,
		struct drm_msm_ext_hdr_metadata *hdr_meta);
	void (*tpg_config)(struct dp_panel *dp_panel, bool enable);
	int (*spd_config)(struct dp_panel *dp_panel);
	bool (*hdr_supported)(struct dp_panel *dp_panel);
};

/**
 * is_link_rate_valid() - validates the link rate
 * @lane_rate: link rate requested by the sink
 *
 * Returns true if the requested link rate is supported.
 */
static inline bool is_link_rate_valid(u32 bw_code)
{
	return ((bw_code == DP_LINK_BW_1_62) ||
		(bw_code == DP_LINK_BW_2_7) ||
		(bw_code == DP_LINK_BW_5_4) ||
		(bw_code == DP_LINK_BW_8_1));
}

/**
 * dp_link_is_lane_count_valid() - validates the lane count
 * @lane_count: lane count requested by the sink
 *
 * Returns true if the requested lane count is supported.
 */
static inline bool is_lane_count_valid(u32 lane_count)
{
	return (lane_count == DP_LANE_COUNT_1) ||
		(lane_count == DP_LANE_COUNT_2) ||
		(lane_count == DP_LANE_COUNT_4);
}

struct dp_panel *dp_panel_get(struct dp_panel_in *in);
void dp_panel_put(struct dp_panel *dp_panel);
#endif /* _DP_PANEL_H_ */
