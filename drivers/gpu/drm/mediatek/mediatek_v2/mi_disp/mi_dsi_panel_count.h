// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */

#ifndef _DSI_PANEL_MI_COUNT_H_
#define _DSI_PANEL_MI_COUNT_H_

#include <drm/drm_panel.h>

enum PANEL_FPS {
	FPS_1,
	FPS_10,
	FPS_24,
	FPS_30,
	FPS_40,
	FPS_48,
	FPS_50,
	FPS_60,
	FPS_90,
	FPS_120,
	FPS_144,
	FPS_MAX_NUM,
};

struct dsi_panel_mi_count {
	/* Display count */
	bool panel_active_count_enable;
	u64 boottime;
	u64 bootRTCtime;
	u64 bootdays;
	u64 panel_active;
	u64 kickoff_count;
	u64 bl_duration;
	u64 bl_level_integral;
	u64 bl_highlevel_duration;
	u64 bl_lowlevel_duration;
	u64 hbm_duration;
	u64 hbm_times;
	u64 fps_times[FPS_MAX_NUM];
};

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_pos;
	struct gpio_desc *dvdd_gpio;
	struct gpio_desc *cam_gpio;
	struct gpio_desc *leden_gpio;
	struct gpio_desc *vddi_gpio;

	bool prepared;
	bool enabled;
	bool hbm_en;
	bool wqhd_en;
	bool dc_status;
	bool hbm_enabled;
	bool lhbm_en;
	bool doze_suspend;

	int error;
	const char *panel_info;
	int dynamic_fps;
	u32 doze_brightness_state;

	struct pinctrl *pinctrl_gpios;
	struct pinctrl_state *err_flag_irq;

	u32 max_brightness_clone;
	u32 factory_max_brightness;
	struct mutex panel_lock;
	struct dsi_panel_mi_count mi_count;
	int bl_max_level;
	int gir_status;
	int spr_status;
	int crc_level;
};

void dsi_panel_state_count(struct lcm *lcm, int enable);
void dsi_panel_HBM_count(struct lcm *lcm, int enable, int off);
void dsi_panel_backlight_count(struct lcm *lcm, u32 bl_lvl);
void dsi_panel_fps_count(struct lcm *lcm, u32 fps, u32 enable);
void dsi_panel_count_init(struct lcm *lcm);

int dsi_panel_disp_count_set(struct lcm *lcm, const char *buf);
ssize_t dsi_panel_disp_count_get(struct lcm *lcm, char *buf);
#endif /* _DSI_PANEL_MI_COUNT_H_ */
