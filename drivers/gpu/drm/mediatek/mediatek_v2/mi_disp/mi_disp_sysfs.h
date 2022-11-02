// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */

#ifndef _MI_DRM_SYSFS_H_
#define _MI_DRM_SYSFS_H_

#include <linux/device.h>

/* mi_drm_sysfs.c */
int mi_disp_create_device_attributes(struct device *dev);
void mi_disp_remove_device_attributes(struct device *dev);
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
