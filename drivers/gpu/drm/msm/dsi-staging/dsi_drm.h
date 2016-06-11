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

#include "dsi_display.h"

struct dsi_connector_state {
	struct drm_connector_state base;
};

struct dsi_connector_helper_funcs {
	struct drm_connector_helper_funcs base;
};

struct dsi_connector {
	struct drm_connector base;
	u32 id;

	struct dsi_display *display;
	struct drm_panel *panel;
	struct drm_encoder *encoder;

	struct drm_property *display_type;
	struct drm_property_blob *display_type_blob;

	enum dsi_display_type type;
	bool has_tile;
	u32 h_tile_id;
	bool is_master;
};

struct dsi_bridge {
	struct drm_bridge base;
	u32 id;

	struct dsi_display *display;
	struct dsi_display_mode dsi_mode;
};

struct dsi_connector *dsi_drm_connector_init(struct dsi_display *display,
					     struct drm_device *dev,
					     struct dsi_bridge *bridge);
void dsi_drm_connector_cleanup(struct dsi_connector *conn);

struct dsi_bridge *dsi_drm_bridge_init(struct dsi_display *display,
				       struct drm_device *dev,
				       struct drm_encoder *encoder);

void dsi_drm_bridge_cleanup(struct dsi_bridge *bridge);
#endif /* _DSI_DRM_H_ */
