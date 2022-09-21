/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */

#ifndef _MI_DSI_PANEL_COUNT_H_
#define _MI_DSI_PANEL_COUNT_H_

#include "dsi_panel.h"

typedef enum panel_count_event {
	PANEL_ACTIVE,
	PANEL_BACKLIGHT,
	PANEL_HBM,
	PANEL_FPS,
	PANEL_EVENT_MAX,
} PANEL_COUNT_EVENT;

enum PANEL_FPS {
	FPS_30,
	FPS_50,
	FPS_60,
	FPS_90,
	FPS_120,
	FPS_144,
	FPS_MAX_NUM,
};

struct mi_dsi_panel_count {
	/* display count */
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

void mi_dsi_panel_HBM_count(struct dsi_panel *panel, int enable, int off);
void mi_dsi_panel_HBM_count_bl(struct dsi_panel *panel, u32 bl_lvl);
void mi_dsi_panel_backlight_count(struct dsi_panel *panel, u32 bl_lvl);
void mi_dsi_panel_fps_count(struct dsi_panel *panel, u32 fps, u32 enable);
void mi_dsi_panel_fps_count_lock(struct dsi_panel *panel, u32 fps, u32 enable);
void mi_dsi_panel_state_count(struct dsi_panel *panel, PANEL_COUNT_EVENT event, int value);
void mi_dsi_panel_count_init(struct dsi_panel *panel);

int mi_dsi_panel_set_disp_count(struct dsi_panel *panel, const char *buf);
ssize_t mi_dsi_panel_get_disp_count(struct dsi_panel *panel, char *buf, size_t size);
#endif /* _MI_DSI_PANEL_COUNT_H_ */
