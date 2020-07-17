/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
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

#ifndef _DRM_INTERFACE_MI_H_
#define _DRM_INTERFACE_MI_H_

/* dsi_display_mi.c */
int dsi_display_disp_param_get(struct drm_connector *connector,
				char *buf);
int dsi_display_disp_param_set(struct drm_connector *connector,
				u32 param_type);

ssize_t dsi_display_mipi_reg_write(struct drm_connector *connector,
				char *buf, size_t count);
ssize_t dsi_display_mipi_reg_read(struct drm_connector *connector,
				char *buf);

ssize_t dsi_display_panel_info_read(struct drm_connector *connector,
				char *buf);

ssize_t dsi_display_fod_get(struct drm_connector *connector,
				char *buf);

int dsi_display_set_doze_brightness(struct drm_connector *connector,
				int doze_brightness);
ssize_t dsi_display_get_doze_brightness(struct drm_connector *connector,
				char *buf);

#endif /*_DRM_INTERFACE_MI_H_*/
