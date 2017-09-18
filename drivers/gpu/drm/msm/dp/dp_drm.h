/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#ifndef _DP_DRM_H_
#define _DP_DRM_H_

#include <linux/types.h>
#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>

#include "msm_drv.h"
#include "dp_display.h"

struct dp_bridge {
	struct drm_bridge base;
	u32 id;

	struct dp_display *display;
	struct dp_display_mode dp_mode;
};

/**
 * dp_connector_post_init - callback to perform additional initialization steps
 * @connector: Pointer to drm connector structure
 * @info: Pointer to sde connector info structure
 * @display: Pointer to private display handle
 * Returns: Zero on success
 */
int dp_connector_post_init(struct drm_connector *connector,
		void *info,
		void *display);

/**
 * dp_connector_detect - callback to determine if connector is connected
 * @connector: Pointer to drm connector structure
 * @force: Force detect setting from drm framework
 * @display: Pointer to private display handle
 * Returns: Connector 'is connected' status
 */
enum drm_connector_status dp_connector_detect(struct drm_connector *conn,
		bool force,
		void *display);

/**
 * dp_connector_get_modes - callback to add drm modes via drm_mode_probed_add()
 * @connector: Pointer to drm connector structure
 * @display: Pointer to private display handle
 * Returns: Number of modes added
 */
int dp_connector_get_modes(struct drm_connector *connector,
		void *display);

/**
 * dp_connector_mode_valid - callback to determine if specified mode is valid
 * @connector: Pointer to drm connector structure
 * @mode: Pointer to drm mode structure
 * @display: Pointer to private display handle
 * Returns: Validity status for specified mode
 */
enum drm_mode_status dp_connector_mode_valid(struct drm_connector *connector,
		struct drm_display_mode *mode,
		void *display);

/**
 * dp_connector_get_mode_info - retrieve information of the mode selected
 * @drm_mode: Display mode set for the display
 * @mode_info: Out parameter. Information of the mode
 * @max_mixer_width: max width supported by HW layer mixer
 * @display: Pointer to private display structure
 * Returns: zero on success
 */
int dp_connector_get_mode_info(const struct drm_display_mode *drm_mode,
		struct msm_mode_info *mode_info,
		u32 max_mixer_width, void *display);

int dp_connector_get_info(struct msm_display_info *info, void *display);

void dp_connector_send_hpd_event(void *display);

int dp_drm_bridge_init(void *display,
	struct drm_encoder *encoder);

void dp_drm_bridge_deinit(void *display);
#endif /* _DP_DRM_H_ */

