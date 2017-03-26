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

#define DPCD_ENHANCED_FRAME     BIT(0)
#define DPCD_TPS3               BIT(1)
#define DPCD_MAX_DOWNSPREAD_0_5 BIT(2)
#define DPCD_NO_AUX_HANDSHAKE   BIT(3)
#define DPCD_PORT_0_EDID_PRESENTED BIT(4)

#define EDID_START_ADDRESS	0x50
#define EDID_BLOCK_SIZE		0x80


#define DP_LINK_RATE_162	6	/* 1.62G = 270M * 6 */
#define DP_LINK_RATE_270	10	/* 2.70G = 270M * 10 */
#define DP_LINK_RATE_540	20	/* 5.40G = 270M * 20 */
#define DP_LINK_RATE_810	30	/* 8.10G = 270M * 30 */
#define DP_LINK_RATE_MAX	DP_LINK_RATE_810

struct downstream_port_config {
	/* Byte 02205h */
	bool dfp_present;
	u32 dfp_type;
	bool format_conversion;
	bool detailed_cap_info_available;
	/* Byte 02207h */
	u32 dfp_count;
	bool msa_timing_par_ignored;
	bool oui_support;
};

struct dp_panel_dpcd {
	u8 major;
	u8 minor;
	u8 max_lane_count;
	u8 num_rx_port;
	u8 i2c_speed_ctrl;
	u8 scrambler_reset;
	u8 enhanced_frame;
	u32 max_link_rate;  /* 162, 270 and 540 Mb, divided by 10 */
	u32 flags;
	u32 rx_port0_buf_size;
	u32 training_read_interval;/* us */
	struct downstream_port_config downstream_port;
};

struct dp_panel_edid {
	u8 *buf;
	u8 id_name[4];
	u8 id_product;
	u8 version;
	u8 revision;
	u8 video_intf;	/* dp == 0x5 */
	u8 color_depth;	/* 6, 8, 10, 12 and 14 bits */
	u8 color_format;	/* RGB 4:4:4, YCrCb 4:4:4, Ycrcb 4:2:2 */
	u8 dpm;		/* display power management */
	u8 sync_digital;	/* 1 = digital */
	u8 sync_separate;	/* 1 = separate */
	u8 vsync_pol;		/* 0 = negative, 1 = positive */
	u8 hsync_pol;		/* 0 = negative, 1 = positive */
	u8 ext_block_cnt;
};

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
	struct dp_panel_dpcd dpcd;
	struct dp_panel_edid edid;
	struct dp_panel_info pinfo;

	u32 vic;

	int (*init_info)(struct dp_panel *dp_panel);
	int (*timing_cfg)(struct dp_panel *dp_panel);
	int (*read_edid)(struct dp_panel *dp_panel);
	int (*read_dpcd)(struct dp_panel *dp_panel);
	u8 (*get_link_rate)(struct dp_panel *dp_panel);
};

struct dp_panel *dp_panel_get(struct device *dev, struct dp_aux *aux,
				struct dp_catalog_panel *catalog);
void dp_panel_put(struct dp_panel *dp_panel);
#endif /* _DP_PANEL_H_ */
