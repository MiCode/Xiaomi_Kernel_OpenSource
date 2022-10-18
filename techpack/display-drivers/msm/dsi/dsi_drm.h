/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _DSI_DRM_H_
#define _DSI_DRM_H_

#include <linux/types.h>
#include <drm/drm_crtc.h>

#include "msm_drv.h"

#include "dsi_display.h"

#define NO_OVERRIDE -1

struct dsi_bridge {
	struct drm_bridge base;
	u32 id;

	struct dsi_display *display;
	struct dsi_display_mode dsi_mode;
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
 * @avail_res: Pointer with curr available resources
 * Returns: Number of modes added
 */
int dsi_connector_get_modes(struct drm_connector *connector,
		void *display, const struct msm_resource_caps_info *avail_res);

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
 * @sub_mode: Additional mode info to drm display mode
 * @mode_info: Out parameter. information of the mode.
 * @display: Pointer to private display structure
 * @avail_res: Pointer with curr available resources
 * Returns: Zero on success
 */
int dsi_conn_get_mode_info(struct drm_connector *connector,
		const struct drm_display_mode *drm_mode,
		struct msm_sub_mode *sub_mode,
		struct msm_mode_info *mode_info,
		void *display, const struct msm_resource_caps_info *avail_res);

/**
 * dsi_conn_mode_valid - callback to determine if specified mode is valid
 * @connector: Pointer to drm connector structure
 * @mode: Pointer to drm mode structure
 * @display: Pointer to private display handle
 * @avail_res: Pointer with curr available resources
 * Returns: Validity status for specified mode
 */
enum drm_mode_status dsi_conn_mode_valid(struct drm_connector *connector,
		struct drm_display_mode *mode,
		void *display, const struct msm_resource_caps_info *avail_res);

/**
 * dsi_conn_enable_event - callback to notify DSI driver of event registration
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
 * @force_update_dsi_update: Bool to force clock update
 * Returns: Zero on success
 */
int dsi_conn_pre_kickoff(struct drm_connector *connector,
		void *display,
		struct msm_display_kickoff_params *params,
		bool force_update_dsi_clocks);

/**
 * dsi_display_post_kickoff - program post kickoff-time features
 * @connector: Pointer to drm connector structure
 * @params: Parameters for post kickoff programming
 * Returns: Zero on success
 */
int dsi_conn_post_kickoff(struct drm_connector *connector,
		struct msm_display_conn_params *params);

/**
 * dsi_convert_to_drm_mode - Update drm mode with dsi mode information
 * @dsi_mode: input parameter. structure having dsi mode information.
 * @drm_mode: output parameter. DRM mode set for the display
 */
void dsi_convert_to_drm_mode(const struct dsi_display_mode *dsi_mode,
				struct drm_display_mode *drm_mode);

/**
 * dsi_conn_prepare_commit - program pre commit time features
 * @display: Pointer to private display structure
 * @params: Parameters for pre commit programming
 * Returns: Zero on success
 */
int dsi_conn_prepare_commit(void *display,
		struct msm_display_conn_params *params);

/**
 * dsi_set_allowed_mode_switch - set allowed mode switch bitmask
 * @connector: Pointer to drm connector structure
 * @display: Pointer to private display structure
 */
void dsi_conn_set_allowed_mode_switch(struct drm_connector *connector,
		void *display);

/**
 * dsi_conn_set_dyn_bit_clk - set target dynamic clock rate
 * @connector: Pointer to drm connector structure
 * @value: Target dynamic clock rate
 * Returns: Zero on success
 */
int dsi_conn_set_dyn_bit_clk(struct drm_connector *connector,
		uint64_t value);

/**
 * dsi_conn_set_submode_blob_info - populate given sub mode blob
 * @connector: Pointer to drm connector structure
 * @info: Pointer to sde connector info structure
 * @display: Pointer to private display handle
 * @drm_mode: Pointer to drm_display_mode structure
 */
void dsi_conn_set_submode_blob_info(struct drm_connector *conn,
		void *info, void *display, struct drm_display_mode *drm_mode);

/**
 * dsi_conn_get_qsync_min_fps() - get qsync min fps for given fps
 * @conn_state:         Pointer to drm_connector_state structure
 *
 * Return: Qsync min fps rate or -ve error code.
 */
int dsi_conn_get_qsync_min_fps(struct drm_connector_state *conn_state);
#endif /* _DSI_DRM_H_ */
