/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
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

#ifndef _DSI_PANEL_MI_COUNT_H_
#define _DSI_PANEL_MI_COUNT_H_

#include "dsi_panel.h"

enum PANEL_FPS {
	FPS_30,
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

void dsi_panel_state_count(struct dsi_panel *panel, int enable);
void dsi_panel_HBM_count(struct dsi_panel *panel, int enable, int off);
void dsi_panel_backlight_count(struct dsi_panel *panel, u32 bl_lvl);
void dsi_panel_fps_count(struct dsi_panel *panel, u32 fps, u32 enable);
void dsi_panel_fps_count_lock(struct dsi_panel *panel, u32 fps, u32 enable);
void dsi_panel_count_init(struct dsi_panel *panel);

int dsi_panel_disp_count_set(struct dsi_panel *panel, const char *buf);
ssize_t dsi_panel_disp_count_get(struct dsi_panel *panel, char *buf);
#endif /* _DSI_PANEL_MI_COUNT_H_ */