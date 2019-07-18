/*
 * Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
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
	struct mutex lock;
};

/**
 * dsi_conn_set_info_blob - callback to perform info blob initialization
 * @connector: Pointer to drm connector structure
 * @info: Pointer to sde connector info structure
 * @display: Pointer to private display handle
 * @mode_info: Pointer to mode info structure
 * Returns: Zero on success
 */
int dsi_conn_set_info_blob(struct drm_connector *connector,
		void *info,
		void *display,
		struct msm_mode_info *mode_info);

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
 * dsi_connector_put_modes - callback to free up drm modes of the connector
 * @connector: Pointer to drm connector structure
 * @display: Pointer to private display handle
 */
void dsi_connector_put_modes(struct drm_connector *connector,
	void *display);

/**
 * dsi_conn_get_mode_info - retrieve information on the mode selected
 * @drm_mode: Display mode set for the display
 * @mode_info: Out parameter. information of the mode.
 * @max_mixer_width: max width supported by HW layer mixer
 * @display: Pointer to private display structure
 * Returns: Zero on success
 */
int dsi_conn_get_mode_info(const struct drm_display_mode *drm_mode,
	struct msm_mode_info *mode_info, u32 max_mixer_width,
	void *display);

/**
 * dsi_conn_ext_bridge_get_mode_info - retrieve information on the mode selected
 * @drm_mode: Display mode set for the display
 * @mode_info: Out parameter. information of the mode.
 * @max_mixer_width: max width supported by HW layer mixer
 * @display: Pointer to private display structure
 * Returns: Zero on success
 */
int dsi_conn_ext_bridge_get_mode_info(const struct drm_display_mode *drm_mode,
	struct msm_mode_info *mode_info, u32 max_mixer_width,
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

/**
 * dsi_conn_enable_event - callback to notify DSI driver of event registeration
 * @connector: Pointer to drm connector structure
 * @event_idx: Connector event index
 * @enable: Whether or not the event is enabled
 * @display: Pointer to private display handle
 */
void dsi_conn_enable_event(struct drm_connector *connector,
		uint32_t event_idx, bool enable, void *display);

struct dsi_bridge *dsi_drm_bridge_init(struct dsi_display *display,
		struct drm_device *dev,
		struct drm_encoder *encoder);

void dsi_drm_bridge_cleanup(struct dsi_bridge *bridge);

/**
 * dsi_display_pre_kickoff - program kickoff-time features
 * @connector: Pointer to drm connector structure
 * @display: Pointer to private display structure
 * @params: Parameters for kickoff-time programming
 * Returns: Zero on success
 */
int dsi_conn_pre_kickoff(struct drm_connector *connector,
		void *display,
		struct msm_display_kickoff_params *params);

/**
 * dsi_display_post_kickoff - program post kickoff-time features
 * @connector: Pointer to drm connector structure
 * Returns: Zero on success
 */
int dsi_conn_post_kickoff(struct drm_connector *connector);

/**
 * dsi_convert_to_drm_mode - Update drm mode with dsi mode information
 * @dsi_mode: input parameter. structure having dsi mode information.
 * @drm_mode: output parameter. DRM mode set for the display
 */
void dsi_convert_to_drm_mode(const struct dsi_display_mode *dsi_mode,
				struct drm_display_mode *drm_mode);

u64 dsi_drm_find_bit_clk_rate(void *display,
			      const struct drm_display_mode *drm_mode);
#endif /* _DSI_DRM_H_ */
