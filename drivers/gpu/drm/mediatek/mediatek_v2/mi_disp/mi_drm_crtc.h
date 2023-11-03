/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */

#ifndef _MI_DRM_CRTC_H_
#define _MI_DRM_CRTC_H_

#include <linux/types.h>

enum mi_layer_type {
	MI_LAYER_FOD_HBM_OVERLAY = BIT(0),
	MI_LAYER_FOD_ICON = BIT(1),
	MI_LAYER_AOD = BIT(2),
	MI_LAYER_FOD_ANIM = BIT(3),
};

struct mi_layer_flags {
	bool fod_overlay_flag;
	bool fod_icon_flag;
	bool aod_flag;
	bool fod_anim_flag;
};

enum PANEL_ID{
	PANEL_1ST = 1,
	PANEL_2SD,
	PANEL_3RD,
	PANEL_4TH,
	PANEL_MAX_NUM,
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
	struct gpio_desc *vddio18_gpio;
	struct gpio_desc *vci30_gpio;

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
	u32 doze_state;

	struct pinctrl *pinctrl_gpios;
	struct pinctrl_state *err_flag_irq;
	struct drm_connector *connector;

	u32 max_brightness_clone;
	u32 factory_max_brightness;
	struct mutex panel_lock;
	int bl_max_level;
	int gir_status;
	int spr_status;
	int crc_level;
	int mode_index;
	unsigned int gate_ic;
	int panel_id;
	int gray_level;

	/* DDIC auto update gamma */
	u32 last_refresh_rate;
	bool need_auto_update_gamma;
	ktime_t last_mode_switch_time;
	int peak_hdr_status;
};

struct mi_layer_state {
	struct mi_layer_flags layer_flags;
	u32 current_backlight;
};

int mi_drm_crtc_update_layer_state(struct drm_crtc *crtc);
int mi_drm_bl_wait_for_completion(struct drm_crtc *crtc, unsigned int level);

#endif /* _MI_DRM_CRTC_H_ */

