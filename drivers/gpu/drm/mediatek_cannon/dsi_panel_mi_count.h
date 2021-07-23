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

#include <linux/backlight.h>
#include <linux/delay.h>
#include <drm/drmP.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "mtk_panel_ext.h"
#include "mtk_log.h"
#include "mtk_drm_graphics_base.h"
#endif

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
	u64 fps_times[FPS_MAX_NUM];
	u64 panel_active;
	u64 boottime;
	u64 bootRTCtime;
	u64 bootdays;
	u64 kickoff_count;
	u64 hbm_duration;
	u64 hbm_times;
};

void dsi_panel_fps_count(struct drm_panel *panel, u32 fps, u32 enable);
void dsi_panel_state_count(struct drm_panel *panel, int enable);
void dsi_panel_HBM_count(struct drm_panel *panel, int enable, int off);
void dsi_panel_count_init(struct drm_panel *panel);
int dsi_panel_disp_count_set(struct drm_panel *panel, const char *buf);
ssize_t dsi_panel_disp_count_get(struct drm_panel *panel, char *buf);
#endif /* _DSI_PANEL_MI_COUNT_H_ */

