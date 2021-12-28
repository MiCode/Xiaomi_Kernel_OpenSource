/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _DP_DRM_H_
#define _DP_DRM_H_

#include <linux/types.h>
#include <drm/drm_crtc.h>
#include <drm/drm_bridge.h>

#include "msm_drv.h"
#include "dp_display.h"

struct dp_bridge {
	struct drm_bridge base;
	u32 id;

	struct drm_connector *connector;
	struct dp_display *display;
	struct dp_display_mode dp_mode;
	void *dp_panel;
};


#if IS_ENABLED(CONFIG_DRM_MSM_DP)
/**
 * dp_connector_config_hdr - callback to configure HDR
 * @connector: Pointer to drm connector structure
 * @display: Pointer to private display handle
 * @c_state: connect state data
 * Returns: Zero on success
 */
int dp_connector_config_hdr(struct drm_connector *connector,
		void *display,
		struct sde_connector_state *c_state);

/**
 * dp_connector_atomic_check - callback to perform atomic
 * check for DP
 * @connector: Pointer to drm connector structure
 * @display: Pointer to private display handle
 * @c_state: connect state data
 * Returns: Zero on success
 */
int dp_connector_atomic_check(struct drm_connector *connector,
	void *display,
	struct drm_atomic_state *state);

/**
 * dp_connector_set_colorspace - callback to set new colorspace
 * @connector: Pointer to drm connector structure
 * @display: Pointer to private display handle
 * Returns: Zero on success
 */
int dp_connector_set_colorspace(struct drm_connector *connector,
	void *display);

/**
 * dp_connector_post_init - callback to perform additional initialization steps
 * @connector: Pointer to drm connector structure
 * @display: Pointer to private display handle
 * Returns: Zero on success
 */
int dp_connector_post_init(struct drm_connector *connector, void *display);

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
 * @avail_res: Pointer with curr available resources
 * Returns: Number of modes added
 */
int dp_connector_get_modes(struct drm_connector *connector,
		void *display, const struct msm_resource_caps_info *avail_res);

/**
 * dp_connector_mode_valid - callback to determine if specified mode is valid
 * @connector: Pointer to drm connector structure
 * @mode: Pointer to drm mode structure
 * @display: Pointer to private display handle
 * @avail_res: Pointer with curr available resources
 * Returns: Validity status for specified mode
 */
enum drm_mode_status dp_connector_mode_valid(struct drm_connector *connector,
		struct drm_display_mode *mode,
		void *display, const struct msm_resource_caps_info *avail_res);

/**
 * dp_connector_get_mode_info - retrieve information of the mode selected
 * @connector: Pointer to drm connector structure
 * @drm_mode: Display mode set for the display
 * @mode_info: Out parameter. Information of the mode
 * @sub_mode: Additional mode info to drm display mode
 * @display: Pointer to private display structure
 * @avail_res: Pointer with curr available resources
 * Returns: zero on success
 */
int dp_connector_get_mode_info(struct drm_connector *connector,
		const struct drm_display_mode *drm_mode,
		struct msm_sub_mode *sub_mode,
		struct msm_mode_info *mode_info,
		void *display, const struct msm_resource_caps_info *avail_res);

/**
 * dp_connector_get_info - retrieve connector display info
 * @connector: Pointer to drm connector structure
 * @info: Out parameter. Information of the connected display
 * @display: Pointer to private display structure
 * Returns: zero on success
 */
int dp_connector_get_info(struct drm_connector *connector,
		struct msm_display_info *info, void *display);

/**
 * dp_connector_post_open - handle the post open functionalities
 * @connector: Pointer to drm connector structure
 * @display: Pointer to private display structure
 */
void dp_connector_post_open(struct drm_connector *connector, void *display);

/**
 * dp_drm_bridge_init- drm dp bridge initialize
 * @display: Pointer to private display structure
 * @encoder: encoder for this dp bridge
 * @max_mixer_count: max available mixers for dp display
 * @max_dsc_count: max available dsc for dp display
 */
int dp_drm_bridge_init(void *display, struct drm_encoder *encoder,
	u32 max_mixer_count, u32 max_dsc_count);

void dp_drm_bridge_deinit(void *display);

/**
 * convert_to_drm_mode - convert dp mode to drm mode
 * @dp_mode: Point to dp mode
 * @drm_mode: Pointer to drm mode
 */
void convert_to_drm_mode(const struct dp_display_mode *dp_mode,
				struct drm_display_mode *drm_mode);

/**
 * dp_connector_update_pps - update pps for given connector
 * @dp_mode: Point to dp mode
 * @pps_cmd: PPS packet
 * @display: Pointer to private display structure
 */
int dp_connector_update_pps(struct drm_connector *connector,
		char *pps_cmd, void *display);

/**
 * dp_connector_install_properties - install drm properties
 * @display: Pointer to private display structure
 * @conn: Pointer to connector
 */
int dp_connector_install_properties(void *display,
		struct drm_connector *conn);

#else
static inline int dp_connector_config_hdr(struct drm_connector *connector,
		void *display, struct sde_connector_state *c_state)
{
	return 0;
}

static inline int dp_connector_atomic_check(struct drm_connector *connector,
		void *display, struct drm_atomic_state *state)
{
	return 0;
}

static inline int dp_connector_set_colorspace(struct drm_connector *connector,
		void *display)
{
	return 0;
}

static inline int dp_connector_post_init(struct drm_connector *connector,
		void *display)
{
	return 0;
}

static inline enum drm_connector_status dp_connector_detect(
		struct drm_connector *conn,
		bool force,
		void *display)
{
	return 0;
}


static inline int dp_connector_get_modes(struct drm_connector *connector,
		void *display, const struct msm_resource_caps_info *avail_res)
{
	return 0;
}

static inline enum drm_mode_status dp_connector_mode_valid(
		struct drm_connector *connector,
		struct drm_display_mode *mode,
		void *display, const struct msm_resource_caps_info *avail_res)
{
	return MODE_OK;
}

static inline int dp_connector_get_mode_info(struct drm_connector *connector,
		const struct drm_display_mode *drm_mode,
		struct msm_sub_mode *sub_mode,
		struct msm_mode_info *mode_info,
		void *display, const struct msm_resource_caps_info *avail_res)
{
	return 0;
}

static inline int dp_connector_get_info(struct drm_connector *connector,
		struct msm_display_info *info, void *display)
{
	return 0;
}

static inline void dp_connector_post_open(struct drm_connector *connector,
		void *display)
{
}

static inline int dp_drm_bridge_init(void *display, struct drm_encoder *encoder,
		u32 max_mixer_count, u32 max_dsc_count)
{
	return 0;
}

static inline void dp_drm_bridge_deinit(void *display)
{
}

static inline void convert_to_drm_mode(const struct dp_display_mode *dp_mode,
				struct drm_display_mode *drm_mode)
{
}

static int dp_connector_install_properties(void *display,
		struct drm_connector *conn)
{
	return 0;
}
#endif /* CONFIG_DRM_MSM_DP */

#endif /* _DP_DRM_H_ */
