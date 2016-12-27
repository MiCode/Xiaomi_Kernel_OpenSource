/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#ifndef _DSI_DRM_H_
#define _DSI_DRM_H_

#include <linux/types.h>
#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>

#include "msm_drv.h"

#include "dsi_display.h"

struct dsi_bridge {
	struct drm_bridge base;
	u32 id;

	struct dsi_display *display;
	struct dsi_display_mode dsi_mode;
};

/**
 * dsi_conn_post_init - callback to perform additional initialization steps
 * @connector: Pointer to drm connector structure
 * @info: Pointer to sde connector info structure
 * @display: Pointer to private display handle
 * Returns: Zero on success
 */
int dsi_conn_post_init(struct drm_connector *connector,
		void *info,
		void *display);

/**
 * dsi_conn_detect - callback to determine if connector is connected
 * @connector: Pointer to drm connector structure
 * @force: Force detect setting from drm framework
 * @display: Pointer to private display handle
 * Returns: Connector 'is connected' status
 */
enum drm_connector_status dsi_conn_detect(struct drm_connector *conn,
		bool force,
		void *display);

/**
 * dsi_connector_get_modes - callback to add drm modes via drm_mode_probed_add()
 * @connector: Pointer to drm connector structure
 * @display: Pointer to private display handle
 * Returns: Number of modes added
 */
int dsi_connector_get_modes(struct drm_connector *connector,
		void *display);

/**
 * dsi_conn_mode_valid - callback to determine if specified mode is valid
 * @connector: Pointer to drm connector structure
 * @mode: Pointer to drm mode structure
 * @display: Pointer to private display handle
 * Returns: Validity status for specified mode
 */
enum drm_mode_status dsi_conn_mode_valid(struct drm_connector *connector,
		struct drm_display_mode *mode,
		void *display);

struct dsi_bridge *dsi_drm_bridge_init(struct dsi_display *display,
		struct drm_device *dev,
		struct drm_encoder *encoder);

void dsi_drm_bridge_cleanup(struct dsi_bridge *bridge);

#endif /* _DSI_DRM_H_ */
