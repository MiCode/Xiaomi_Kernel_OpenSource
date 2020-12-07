/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
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

#ifndef _DSI_PANEL_MI_H_
#define _DSI_PANEL_MI_H_

#include <linux/of_device.h>
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/backlight.h>
#include <drm/drm_panel.h>
#include <drm/msm_drm.h>

#include "dsi_defs.h"
#include "dsi_ctrl_hw.h"
#include "dsi_clk.h"
#include "dsi_pwr.h"
#include "dsi_parser.h"
#include "msm_drv.h"

#define DEFAULT_FOD_OFF_DIMMING_DELAY   170
#define DOZE_MIN_BRIGHTNESS_LEVEL       5

#define DOZE_MIN_BRIGHTNESS_LEVEL       5
#define BUF_LEN_MAX    256

enum {
	DOZE_BRIGHTNESS_INVALID = 0,
	DOZE_BRIGHTNESS_HBM,
	DOZE_BRIGHTNESS_LBM,
};

struct dsi_read_config {
	bool is_read;
	struct dsi_panel_cmd_set read_cmd;
	u32 cmds_rlen;
	u32 valid_bits;
	u8 rbuf[BUF_LEN_MAX];
};

struct dsi_panel_mi_cfg {
	struct dsi_panel *dsi_panel;

	/* xiaomi feature enable flag */
	bool mi_feature_enabled;

	/* bl_is_big_endian indicate brightness value
	 * high byte to 1st parameter, low byte to 2nd parameter
	 * eg: 0x51 { 0x03, 0xFF } ->
	 * u8 payload[2] = { brightness >> 8, brightness & 0xff}
	 */
	bool bl_is_big_endian;
	u32 bl_last_level;

	u32 skip_dimmingon;
	u32 panel_on_dimming_delay;
	struct delayed_work cmds_work;

	bool fod_hbm_enabled;
	u32 doze_backlight_threshold;
	u32 fod_off_dimming_delay;
	ktime_t fod_backlight_off_time;
	ktime_t fod_hbm_off_time;

	bool fod_backlight_flag;
	u32 fod_target_backlight;
	bool fod_flag;
	bool in_aod; /* set  DISPPARAM_DOZE_BRIGHTNESS_HBM/LBM only in AOD */
	bool is_tddi_flag;
	bool tddi_doubleclick_flag;
	bool panel_dead_flag;
	bool panel_max_frame_rate;
	u8 panel_read_data[BUF_LEN_MAX];
	struct dsi_read_config xy_coordinate_cmds;
	struct dsi_read_config max_luminance_cmds;
	bool esd_skip_reg_read;
	u32 idle_time;
	u32 idle_fps;
	u32 post_off_msleep;
};

int dsi_panel_parse_mi_config(struct dsi_panel *panel,
				struct device_node *of_node);

int dsi_panel_write_cmd_set(struct dsi_panel *panel,
				struct dsi_panel_cmd_set *cmd_sets);

int dsi_panel_read_cmd_set(struct dsi_panel *panel,
				struct dsi_read_config *read_config);

int dsi_panel_disp_param_set(struct dsi_panel *panel, u32 param);

#endif /* _DSI_PANEL_MI_H_ */
