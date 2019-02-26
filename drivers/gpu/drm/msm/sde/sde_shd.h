/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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

#ifndef _SDE_SHD_H_
#define _SDE_SHD_H_

#include <linux/types.h>
#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include "msm_drv.h"

struct shd_mode_info {
	int x_offset;
	int y_offset;
	int width;
	int height;
};

struct shd_stage_range {
	u32 start;
	u32 size;
};

struct shd_display_base {
	struct mutex           base_mutex;
	struct drm_display_mode mode;
	struct drm_crtc       *crtc;
	struct drm_encoder    *encoder;
	struct drm_connector  *connector;
	struct list_head       head;
	struct list_head       disp_list;
	struct device_node    *of_node;
	struct sde_connector_ops ops;

	int intf_idx;
	int connector_type;
	bool enabled;
};

struct shd_display {
	struct drm_device *drm_dev;
	const char *name;
	const char *display_type;

	struct shd_display_base *base;
	struct drm_bridge *bridge;
	struct drm_connector *connector;

	struct device_node *base_of;
	struct sde_rect src;
	struct sde_rect roi;
	struct shd_stage_range stage_range;

	struct platform_device *pdev;
	struct completion vsync_comp;
	struct list_head head;

	bool enabled;
	bool cont_splash_enabled;
};

#ifdef CONFIG_DRM_SDE_SHD
int shd_display_get_num_of_displays(void);
int shd_display_get_displays(void **displays, int count);
int shd_display_post_init(struct sde_kms *sde_kms);
void shd_display_prepare_commit(struct sde_kms *sde_kms,
		struct drm_atomic_state *state);
void shd_display_complete_commit(struct sde_kms *sde_kms,
		struct drm_atomic_state *state);

/**
 * shd_connector_post_init - callback to perform additional initialization steps
 * @connector: Pointer to drm connector structure
 * @display: Pointer to private display handle
 * Returns: Zero on success
 */
int shd_connector_post_init(struct drm_connector *connector,
		void *info,
		void *display);

/**
 * shd_connector_detect - callback to determine if connector is connected
 * @connector: Pointer to drm connector structure
 * @force: Force detect setting from drm framework
 * @display: Pointer to private display handle
 * Returns: Connector 'is connected' status
 */
enum drm_connector_status shd_connector_detect(struct drm_connector *conn,
		bool force,
		void *display);

/**
 * shd_connector_get_modes - callback to add drm modes via drm_mode_probed_add()
 * @connector: Pointer to drm connector structure
 * @display: Pointer to private display handle
 * Returns: Number of modes added
 */
int shd_connector_get_modes(struct drm_connector *connector,
		void *display);

/**
 * shd_connector_mode_valid - callback to determine if specified mode is valid
 * @connector: Pointer to drm connector structure
 * @mode: Pointer to drm mode structure
 * @display: Pointer to private display handle
 * Returns: Validity status for specified mode
 */
enum drm_mode_status shd_connector_mode_valid(struct drm_connector *connector,
		struct drm_display_mode *mode,
		void *display);

/**
 * shd_connector_get_info - retrieve connector display info
 * @connector: Pointer to drm connector structure
 * @info: Out parameter. Information of the connected display
 * @display: Pointer to private display structure
 * Returns: zero on success
 */
int shd_connector_get_info(struct msm_display_info *info, void *display);

/**
 * shd_display_drm_bridge_init() - initializes DRM bridge object
 * for shared display
 * @display:            Handle to the display.
 * @encoder:            Pointer to the encoder object which is connected to the
 *			display.
 * Return: error code.
 */
int shd_drm_bridge_init(void *display,
	struct drm_encoder *encoder);

/**
 * shd_display_drm_bridge_deinit() - destroys DRM bridge for the display
 * @display:        Handle to the display.
 * Return: error code.
 */
void shd_drm_bridge_deinit(void *display);
#else
static inline
int shd_display_get_num_of_displays(void)
{
	return 0;
}

static inline
int shd_display_get_displays(void **displays, int count)
{
	return 0;
}

static inline
int shd_display_post_init(struct sde_kms *sde_kms)
{
	return 0;
}

static inline
void shd_display_prepare_commit(struct sde_kms *sde_kms,
		struct drm_atomic_state *state)
{
}

static inline
void shd_display_complete_commit(struct sde_kms *sde_kms,
		struct drm_atomic_state *state)
{
}

static inline
int shd_connector_post_init(struct drm_connector *connector,
		void *info,
		void *display)
{
	return 0;
}

static inline
enum drm_connector_status shd_connector_detect(struct drm_connector *conn,
		bool force,
		void *display)
{
	return connector_status_unknown;
}

static inline
int shd_connector_get_modes(struct drm_connector *connector,
		void *display)
{
	return 0;
}

static inline
enum drm_mode_status shd_connector_mode_valid(struct drm_connector *connector,
		struct drm_display_mode *mode,
		void *display)
{
	return MODE_ERROR;
}

static inline
int shd_connector_get_info(struct msm_display_info *info, void *display)
{
	return -EINVAL;
}

static inline
int shd_drm_bridge_init(void *display,
	struct drm_encoder *encoder)
{
	return 0;
}

static inline
void shd_drm_bridge_deinit(void *display)
{
}
#endif

#endif /* _SDE_SHD_H_ */
