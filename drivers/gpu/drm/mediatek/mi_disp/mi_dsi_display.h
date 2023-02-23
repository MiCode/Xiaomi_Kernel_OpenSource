// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */

#ifndef _MI_DSI_DISPLAY_H_
#define _MI_DSI_DISPLAY_H_

#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/debugfs.h>
#include <linux/of_device.h>
#include <linux/firmware.h>
//#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_connector.h>
#include "mi_disp_feature.h"

ssize_t dsi_display_get_disp_param(struct drm_connector *connector,
			char *buf);
ssize_t dsi_display_set_disp_param(struct drm_connector *connector,
			u32 param_type);

ssize_t mi_dsi_display_write_led_i2c_reg(void *display,
			char *buf, size_t count);

ssize_t mi_dsi_display_read_led_i2c_reg(void *display,
			char *buf);

int mi_dsi_display_write_dsi_cmd(void *display,
			struct dsi_cmd_rw_ctl *ctl);

int mi_dsi_display_read_dsi_cmd(void *display,
			struct dsi_cmd_rw_ctl *ctl);

int mi_dsi_display_get_fps(void *display,
			u32 *fps);

int mi_dsi_display_set_disp_param(void *display,
			struct disp_feature_ctl *ctl);
ssize_t mi_dsi_display_get_disp_param(void *display,
			char *buf, size_t size);

ssize_t mi_dsi_display_read_wp_info(void *display,
			char *buf, size_t size);

int mi_dsi_display_set_doze_brightness(void *display, int doze_brightness);

int mi_dsi_display_get_doze_brightness(void *display,
			u32 *brightness);

int mi_dsi_display_set_brightness(void *display, int doze_brightness);

int mi_dsi_display_get_brightness(void *display,
			u32 *brightness);

int mi_dsi_display_write_mipi_reg(void *display, char *buf);

ssize_t mi_dsi_display_enable_gir(void *display, char *buf);

ssize_t mi_dsi_display_disable_gir(void *display, char *buf);

int mi_dsi_display_get_gir_status(void *display);

ssize_t mi_dsi_display_read_mipi_reg(void *display, char *buf);

ssize_t mi_dsi_display_read_panel_info(void *display,
			char *buf);

int mi_dsi_display_set_brightness_clone(void *display,
			u32 brightness_clone);

int mi_dsi_display_get_brightness_clone(void *display,
			u32 *brightness_clone);

int mi_dsi_display_get_brightness(void *display,
			u32 *brightness);
void mi_dsi_display_wakeup_pending_doze_work(void *display);

int mi_dsi_display_get_max_brightness_clone(void *display,
			u32 *max_brightness_clone);

int mi_dsi_display_set_thermal_limit_brightness_clone(void *display,
			u32 brightness_clone);

int mi_dsi_display_get_thermal_limit_brightness_clone(void *display,
			u32 *thermal_limit_brightness_clone);

#endif /*_MI_DSI_DISPLAY_H_*/
