/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
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

#ifndef _MI_DRM_SYSFS_H_
#define _MI_DRM_SYSFS_H_

#include <linux/device.h>

/* mi_drm_sysfs.c */
int mi_disp_create_device_attributes(struct device *dev);
void mi_disp_remove_device_attributes(struct device *dev);

ssize_t mi_drm_sysfs_set_disp_param(struct drm_connector *connector,
				u32 param_type);
ssize_t mi_drm_sysfs_get_disp_param(struct drm_connector *connector,
				char *buf);

ssize_t mi_drm_sysfs_write_mipi_reg(struct drm_connector *connector,
				char *buf, ssize_t count);
ssize_t mi_drm_sysfs_read_mipi_reg(struct drm_connector *connector,
				char *buf);

ssize_t mi_drm_sysfs_led_i2c_reg_write(struct drm_connector *connector,
			char *buf, size_t count);
ssize_t mi_drm_sysfs_led_i2c_reg_read(struct drm_connector *connector,
			char *buf);

ssize_t mi_drm_sysfs_read_panel_info(struct drm_connector *connector,
			char *buf);
#endif /*_MI_DRM_SYSFS_H_*/
