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

enum doze_bkl {
	DOZE_TO_NORMAL = 0,
};

struct dsi_read_config {
	bool is_read;
	struct dsi_panel_cmd_set read_cmd;
	u32 cmds_rlen;
	u32 valid_bits;
	u8 rbuf[BUF_LEN_MAX];
};

typedef struct brightness_alpha {
	uint32_t brightness;
	uint32_t alpha;
} brightness_alpha;

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
	u32 bl_last_level_unsetted;

	/* HBM and brightness use 51 reg ctrl */
	bool hbm_51_ctrl_flag;
	bool vi_setting_enabled;
	u32 hbm_off_51_index;
	u32 fod_off_51_index;
	u32 skip_dimmingon;
	u32 panel_on_dimming_delay;
	bool dimming_on_enable;
	bool dc_on_cmd_hs_enable;
	struct delayed_work cmds_work;

	bool hbm_enabled;
	bool fod_hbm_layer_enabled;
	bool fod_hbm_overlay_panding;
	bool panel_initialized;
	bool fod_hbm_enabled;
	u32 doze_backlight_threshold;
	u32 fod_off_dimming_delay;
	ktime_t fod_backlight_off_time;
	ktime_t fod_hbm_off_time;
	u32 fod_ui_ready;
	bool layer_fod_unlock_success;
	bool sysfs_fod_unlock_success;

	bool fod_backlight_flag;
	u32 fod_target_backlight;
	bool fod_flag;
	bool in_aod; /* set  DISPPARAM_DOZE_BRIGHTNESS_HBM/LBM only in AOD */
	bool is_tddi_flag;
	u32 dc_threshold;
	bool dc_enable;
	u32 dc_type;
	u32 aod_backlight;
	bool nolp_command_set_backlight_enabled;
	u32 doze_lbm_brightness;
	u32 doze_hbm_brightness;
	uint32_t doze_brightness;
	u32 unset_doze_brightness;
	u32 doze_brightness_state;
	bool fod_dimlayer_enabled;
	bool prepare_before_fod_hbm_on;
	bool delay_before_fod_hbm_on;
	bool delay_after_fod_hbm_on;
	bool delay_before_fod_hbm_off;
	bool delay_after_fod_hbm_off;
	bool fod_hbm_off_delay;
	uint32_t brightnes_alpha_lut_item_count;
	brightness_alpha *brightness_alpha_lut;
	bool tddi_doubleclick_flag;
	bool panel_dead_flag;
	bool panel_max_frame_rate;
	bool smart_fps_restore;
	u8 panel_read_data[BUF_LEN_MAX];
	struct dsi_read_config xy_coordinate_cmds;
	struct dsi_read_config max_luminance_cmds;
	u32 idle_time;
	u32 idle_fps;
	u32 post_off_msleep;

	/* mi feature on/off state*/
	bool dimming_mode_state;
	bool flat_mode_state;
	bool crc_off_pending;
};

enum {
	MI_FEATURE_STATE_OFF = 0x0,
	MI_FEATURE_STATE_ON = 0x1,
};

int dsi_panel_parse_mi_config(struct dsi_panel *panel,
				struct device_node *of_node);

int dsi_panel_write_cmd_set(struct dsi_panel *panel,
				struct dsi_panel_cmd_set *cmd_sets);

int dsi_panel_read_cmd_set(struct dsi_panel *panel,
				struct dsi_read_config *read_config);

int dsi_panel_disp_param_set(struct dsi_panel *panel, u32 param);

int dsi_panel_set_doze_brightness(struct dsi_panel *panel,
				int doze_brightness, bool need_panel_lock);

ssize_t dsi_panel_get_doze_brightness(struct dsi_panel *panel, char *buf);


#endif /* _DSI_PANEL_MI_H_ */
