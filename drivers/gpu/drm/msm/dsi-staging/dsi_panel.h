/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#ifndef _DSI_PANEL_H_
#define _DSI_PANEL_H_

#include <linux/of_device.h>
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/leds.h>
#include <drm/drm_panel.h>
#include <drm/drm_mipi_dsi.h>

#include "dsi_defs.h"
#include "dsi_ctrl_hw.h"
#include "dsi_clk_pwr.h"

#define MAX_BL_LEVEL 4096

enum dsi_panel_rotation {
	DSI_PANEL_ROTATE_NONE = 0,
	DSI_PANEL_ROTATE_HV_FLIP,
	DSI_PANEL_ROTATE_H_FLIP,
	DSI_PANEL_ROTATE_V_FLIP
};

enum dsi_cmd_set_type {
	DSI_CMD_SET_PRE_ON = 0,
	DSI_CMD_SET_ON,
	DSI_CMD_SET_POST_ON,
	DSI_CMD_SET_PRE_OFF,
	DSI_CMD_SET_OFF,
	DSI_CMD_SET_POST_OFF,
	DSI_CMD_SET_PRE_RES_SWITCH,
	DSI_CMD_SET_RES_SWITCH,
	DSI_CMD_SET_POST_RES_SWITCH,
	DSI_CMD_SET_CMD_TO_VID_SWITCH,
	DSI_CMD_SET_POST_CMD_TO_VID_SWITCH,
	DSI_CMD_SET_VID_TO_CMD_SWITCH,
	DSI_CMD_SET_POST_VID_TO_CMD_SWITCH,
	DSI_CMD_SET_PANEL_STATUS,
	DSI_CMD_SET_MAX
};

enum dsi_cmd_set_state {
	DSI_CMD_SET_STATE_LP = 0,
	DSI_CMD_SET_STATE_HS,
	DSI_CMD_SET_STATE_MAX
};

enum dsi_backlight_type {
	DSI_BACKLIGHT_PWM = 0,
	DSI_BACKLIGHT_WLED,
	DSI_BACKLIGHT_DCS,
	DSI_BACKLIGHT_UNKNOWN,
	DSI_BACKLIGHT_MAX,
};

struct dsi_dfps_capabilities {
	bool dfps_support;
	enum dsi_dfps_type type;
	u32 min_refresh_rate;
	u32 max_refresh_rate;
};

struct dsi_pinctrl_info {
	struct pinctrl *pinctrl;
	struct pinctrl_state *active;
	struct pinctrl_state *suspend;
};

struct dsi_panel_phy_props {
	u32 panel_width_mm;
	u32 panel_height_mm;
	enum dsi_panel_rotation rotation;
};

struct dsi_cmd_desc {
	struct mipi_dsi_msg msg;
	bool last_command;
	u32  post_wait_ms;
};

struct dsi_panel_cmd_set {
	enum dsi_cmd_set_type type;
	enum dsi_cmd_set_state state;
	u32 count;
	struct dsi_cmd_desc *cmds;
};

struct dsi_backlight_config {
	enum dsi_backlight_type type;

	u32 bl_min_level;
	u32 bl_max_level;
	u32 brightness_max_level;

	int en_gpio;
	/* PWM params */
	bool pwm_pmi_control;
	u32 pwm_pmic_bank;
	u32 pwm_period_usecs;
	int pwm_gpio;

	/* WLED params */
	struct led_trigger *wled;
	struct backlight_device *bd;
};

struct dsi_reset_seq {
	u32 level;
	u32 sleep_ms;
};

struct dsi_panel_reset_config {
	struct dsi_reset_seq *sequence;
	u32 count;

	int reset_gpio;
	int disp_en_gpio;
};

/**
 * struct dsi_panel_dba - DSI DBA panel information
 * @dba_panel:          Indicate if it's DBA panel
 * @bridge_name:        Bridge chip name
 * @hdmi_mode:          If bridge chip is in hdmi mode.
 */
struct dsi_panel_dba {
	bool dba_panel;
	const char *bridge_name;
	bool hdmi_mode;
};

struct dsi_panel {
	const char *name;
	struct device_node *panel_of_node;
	struct mipi_dsi_device mipi_device;

	struct mutex panel_lock;
	struct drm_panel drm_panel;
	struct mipi_dsi_host *host;
	struct device *parent;

	struct dsi_host_common_cfg host_config;
	struct dsi_video_engine_cfg video_config;
	struct dsi_cmd_engine_cfg cmd_config;

	struct dsi_dfps_capabilities dfps_caps;

	struct dsi_panel_cmd_set cmd_sets[DSI_CMD_SET_MAX];
	struct dsi_panel_phy_props phy_props;

	struct dsi_regulator_info power_info;
	struct dsi_display_mode mode;

	struct dsi_backlight_config bl_config;
	struct dsi_panel_reset_config reset_config;
	struct dsi_pinctrl_info pinctrl;

	struct dsi_panel_dba dba_config;

	bool lp11_init;
};

struct dsi_panel *dsi_panel_get(struct device *parent,
				struct device_node *of_node);
void dsi_panel_put(struct dsi_panel *panel);

int dsi_panel_drv_init(struct dsi_panel *panel, struct mipi_dsi_host *host);
int dsi_panel_drv_deinit(struct dsi_panel *panel);

int dsi_panel_get_mode_count(struct dsi_panel *panel, u32 *count);
int dsi_panel_get_mode(struct dsi_panel *panel,
		       u32 index,
		       struct dsi_display_mode *mode);
int dsi_panel_validate_mode(struct dsi_panel *panel,
			    struct dsi_display_mode *mode);
int dsi_panel_get_host_cfg_for_mode(struct dsi_panel *panel,
				    struct dsi_display_mode *mode,
				    struct dsi_host_config *config);

int dsi_panel_get_phy_props(struct dsi_panel *panel,
			    struct dsi_panel_phy_props *phy_props);
int dsi_panel_get_dfps_caps(struct dsi_panel *panel,
			    struct dsi_dfps_capabilities *dfps_caps);

int dsi_panel_pre_prepare(struct dsi_panel *panel);

int dsi_panel_prepare(struct dsi_panel *panel);

int dsi_panel_enable(struct dsi_panel *panel);

int dsi_panel_post_enable(struct dsi_panel *panel);

int dsi_panel_pre_disable(struct dsi_panel *panel);

int dsi_panel_disable(struct dsi_panel *panel);

int dsi_panel_unprepare(struct dsi_panel *panel);

int dsi_panel_post_unprepare(struct dsi_panel *panel);

int dsi_panel_set_backlight(struct dsi_panel *panel, u32 bl_lvl);
#endif /* _DSI_PANEL_H_ */
