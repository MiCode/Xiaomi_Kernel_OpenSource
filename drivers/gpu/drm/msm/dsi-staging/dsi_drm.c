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


#define pr_fmt(fmt)	"dsi-drm:[%s] " fmt, __func__
#include <drm/drm_atomic_helper.h>
#include <drm/drm_atomic.h>

#include "dsi_drm.h"
#include "msm_kms.h"

#define to_dsi_bridge(x)     container_of((x), struct dsi_bridge, base)
#define to_dsi_connector(x)  container_of((x), struct dsi_connector, base)
#define to_dsi_state(x)      container_of((x), struct dsi_connector_state, base)

static void convert_to_dsi_mode(const struct drm_display_mode *drm_mode,
				struct dsi_display_mode *dsi_mode)
{
	memset(dsi_mode, 0, sizeof(*dsi_mode));

	dsi_mode->timing.h_active = drm_mode->hdisplay;
	dsi_mode->timing.h_back_porch = drm_mode->htotal - drm_mode->hsync_end;
	dsi_mode->timing.h_sync_width = drm_mode->htotal -
			(drm_mode->hsync_start + dsi_mode->timing.h_back_porch);
	dsi_mode->timing.h_front_porch = drm_mode->hsync_start -
					 drm_mode->hdisplay;
	dsi_mode->timing.h_skew = drm_mode->hskew;

	dsi_mode->timing.v_active = drm_mode->vdisplay;
	dsi_mode->timing.v_back_porch = drm_mode->vtotal - drm_mode->vsync_end;
	dsi_mode->timing.v_sync_width = drm_mode->vtotal -
		(drm_mode->vsync_start + dsi_mode->timing.v_back_porch);

	dsi_mode->timing.v_front_porch = drm_mode->vsync_start -
					 drm_mode->vdisplay;

	dsi_mode->timing.refresh_rate = drm_mode->vrefresh;

	dsi_mode->pixel_clk_khz = drm_mode->clock;
	dsi_mode->panel_mode = 0; /* TODO: Panel Mode */

	if (msm_is_mode_seamless(drm_mode))
		dsi_mode->flags |= DSI_MODE_FLAG_SEAMLESS;
	if (msm_is_mode_dynamic_fps(drm_mode))
		dsi_mode->flags |= DSI_MODE_FLAG_DFPS;
	if (msm_needs_vblank_pre_modeset(drm_mode))
		dsi_mode->flags |= DSI_MODE_FLAG_VBLANK_PRE_MODESET;
}

static void convert_to_drm_mode(const struct dsi_display_mode *dsi_mode,
				struct drm_display_mode *drm_mode)
{
	memset(drm_mode, 0, sizeof(*drm_mode));

	drm_mode->hdisplay = dsi_mode->timing.h_active;
	drm_mode->hsync_start = drm_mode->hdisplay +
				dsi_mode->timing.h_front_porch;
	drm_mode->hsync_end = drm_mode->hsync_start +
			      dsi_mode->timing.h_sync_width;
	drm_mode->htotal = drm_mode->hsync_end + dsi_mode->timing.h_back_porch;
	drm_mode->hskew = dsi_mode->timing.h_skew;

	drm_mode->vdisplay = dsi_mode->timing.v_active;
	drm_mode->vsync_start = drm_mode->vdisplay +
				dsi_mode->timing.v_front_porch;
	drm_mode->vsync_end = drm_mode->vsync_start +
			      dsi_mode->timing.v_sync_width;
	drm_mode->vtotal = drm_mode->vsync_end + dsi_mode->timing.v_back_porch;

	drm_mode->vrefresh = dsi_mode->timing.refresh_rate;
	drm_mode->clock = dsi_mode->pixel_clk_khz;

	if (dsi_mode->flags & DSI_MODE_FLAG_SEAMLESS)
		drm_mode->flags |= DRM_MODE_FLAG_SEAMLESS;
	if (dsi_mode->flags & DSI_MODE_FLAG_DFPS)
		drm_mode->private_flags |= MSM_MODE_FLAG_SEAMLESS_DYNAMIC_FPS;
	if (dsi_mode->flags & DSI_MODE_FLAG_VBLANK_PRE_MODESET)
		drm_mode->private_flags |= MSM_MODE_FLAG_VBLANK_PRE_MODESET;

	drm_mode_set_name(drm_mode);
}

static int dsi_bridge_attach(struct drm_bridge *bridge)
{
	struct dsi_bridge *c_bridge = to_dsi_bridge(bridge);

	if (!bridge) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	pr_debug("[%d] attached\n", c_bridge->id);

	return 0;

}

static void dsi_bridge_pre_enable(struct drm_bridge *bridge)
{
	int rc = 0;
	struct dsi_bridge *c_bridge = to_dsi_bridge(bridge);

	if (!bridge) {
		pr_err("Invalid params\n");
		return;
	}

	/* By this point mode should have been validated through mode_fixup */
	rc = dsi_display_set_mode(c_bridge->display,
			&(c_bridge->dsi_mode), 0x0);
	if (rc) {
		pr_err("[%d] failed to perform a mode set, rc=%d\n",
		       c_bridge->id, rc);
		return;
	}

	if (c_bridge->dsi_mode.flags & DSI_MODE_FLAG_SEAMLESS) {
		pr_debug("[%d] seamless pre-enable\n", c_bridge->id);
		return;
	}

	rc = dsi_display_prepare(c_bridge->display);
	if (rc) {
		pr_err("[%d] DSI display prepare failed, rc=%d\n",
		       c_bridge->id, rc);
		return;
	}

	rc = dsi_display_enable(c_bridge->display);
	if (rc) {
		pr_err("[%d] DSI display enable failed, rc=%d\n",
		       c_bridge->id, rc);
		(void)dsi_display_unprepare(c_bridge->display);
	}
}

static void dsi_bridge_enable(struct drm_bridge *bridge)
{
	int rc = 0;
	struct dsi_bridge *c_bridge = to_dsi_bridge(bridge);

	if (!bridge) {
		pr_err("Invalid params\n");
		return;
	}

	if (c_bridge->dsi_mode.flags & DSI_MODE_FLAG_SEAMLESS) {
		pr_debug("[%d] seamless enable\n", c_bridge->id);
		return;
	}

	rc = dsi_display_post_enable(c_bridge->display);
	if (rc)
		pr_err("[%d] DSI display post enabled failed, rc=%d\n",
		       c_bridge->id, rc);
}

static void dsi_bridge_disable(struct drm_bridge *bridge)
{
	int rc = 0;
	struct dsi_bridge *c_bridge = to_dsi_bridge(bridge);

	if (!bridge) {
		pr_err("Invalid params\n");
		return;
	}

	rc = dsi_display_pre_disable(c_bridge->display);
	if (rc) {
		pr_err("[%d] DSI display pre disable failed, rc=%d\n",
		       c_bridge->id, rc);
	}
}

static void dsi_bridge_post_disable(struct drm_bridge *bridge)
{
	int rc = 0;
	struct dsi_bridge *c_bridge = to_dsi_bridge(bridge);

	if (!bridge) {
		pr_err("Invalid params\n");
		return;
	}

	rc = dsi_display_disable(c_bridge->display);
	if (rc) {
		pr_err("[%d] DSI display disable failed, rc=%d\n",
		       c_bridge->id, rc);
		return;
	}

	rc = dsi_display_unprepare(c_bridge->display);
	if (rc) {
		pr_err("[%d] DSI display unprepare failed, rc=%d\n",
		       c_bridge->id, rc);
		return;
	}
}

static void dsi_bridge_mode_set(struct drm_bridge *bridge,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode)
{
	struct dsi_bridge *c_bridge = to_dsi_bridge(bridge);

	if (!bridge || !mode || !adjusted_mode) {
		pr_err("Invalid params\n");
		return;
	}

	memset(&(c_bridge->dsi_mode), 0x0, sizeof(struct dsi_display_mode));
	convert_to_dsi_mode(adjusted_mode, &(c_bridge->dsi_mode));

	pr_debug("note: using panel cmd/vid mode instead of user val\n");
	c_bridge->dsi_mode.panel_mode =
		c_bridge->display->panel->mode.panel_mode;
}

static bool dsi_bridge_mode_fixup(struct drm_bridge *bridge,
				  const struct drm_display_mode *mode,
				  struct drm_display_mode *adjusted_mode)
{
	int rc = 0;
	bool ret = true;
	struct dsi_bridge *c_bridge = to_dsi_bridge(bridge);
	struct dsi_display_mode dsi_mode;

	if (!bridge || !mode || !adjusted_mode) {
		pr_err("Invalid params\n");
		return false;
	}

	convert_to_dsi_mode(mode, &dsi_mode);

	rc = dsi_display_validate_mode(c_bridge->display, &dsi_mode,
			DSI_VALIDATE_FLAG_ALLOW_ADJUST);
	if (rc) {
		pr_err("[%d] mode is not valid, rc=%d\n", c_bridge->id, rc);
		ret = false;
	} else {
		convert_to_drm_mode(&dsi_mode, adjusted_mode);
	}

	return ret;
}

static const struct drm_bridge_funcs dsi_bridge_ops = {
	.attach       = dsi_bridge_attach,
	.mode_fixup   = dsi_bridge_mode_fixup,
	.pre_enable   = dsi_bridge_pre_enable,
	.enable       = dsi_bridge_enable,
	.disable      = dsi_bridge_disable,
	.post_disable = dsi_bridge_post_disable,
	.mode_set     = dsi_bridge_mode_set,
};

static enum drm_connector_status dsi_conn_detect(struct drm_connector *conn,
						 bool force)
{
	int rc = 0;
	enum drm_connector_status status = connector_status_connected;
	struct dsi_connector *c_conn = to_dsi_connector(conn);
	struct dsi_display_info info;
	struct drm_property_blob *blob;
	size_t len;

	memset(&info, 0x0, sizeof(info));
	rc = dsi_display_get_info(c_conn->display, &info);
	if (rc) {
		pr_err("[%d] failed to get display info, rc=%d\n",
		       c_conn->id, rc);
		status = connector_status_disconnected;
		goto error;
	}

	if (info.is_hot_pluggable) {
		status = (info.is_connected ? connector_status_connected :
					      connector_status_disconnected);
	}

	conn->display_info.width_mm = info.width_mm;
	conn->display_info.height_mm = info.height_mm;

	len = strnlen(info.display_type, sizeof(info.display_type));

	blob = drm_property_create_blob(conn->dev,
					len,
					info.display_type);
	if (IS_ERR_OR_NULL(blob)) {
		rc = PTR_ERR(blob);
		pr_err("failed to create blob, rc=%d\n", rc);
		status = connector_status_connected;
		goto error;
	}

	rc = drm_object_property_set_value(&conn->base,
					   c_conn->display_type,
					   blob->base.id);
	if (rc) {
		pr_err("failed to update display_type prop, rc=%d\n", rc);
		status = connector_status_disconnected;
		drm_property_unreference_blob(blob);
		goto error;
	}

	c_conn->display_type_blob = blob;
error:
	return status;
}

static void dsi_connector_destroy(struct drm_connector *connector)
{
	struct dsi_connector *c_conn = to_dsi_connector(connector);

	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
	kfree(c_conn);
}

static struct drm_connector_state *
dsi_connector_atomic_dup_state(struct drm_connector *connector)
{
	struct dsi_connector_state *state = to_dsi_state(connector->state);
	struct dsi_connector_state *duplicate;

	duplicate = kmemdup(state, sizeof(*state), GFP_KERNEL);
	if (!duplicate)
		return NULL;

	return &duplicate->base;
}

static void dsi_connector_atomic_destroy_state(struct drm_connector *conn,
					      struct drm_connector_state *state)
{
	struct dsi_connector_state *c_state = to_dsi_state(state);

	kfree(c_state);
}

static int dsi_connector_get_modes(struct drm_connector *connector)
{
	int rc = 0;
	u32 count = 0;
	u32 size = 0;
	int i = 0;
	struct dsi_connector *c_conn = to_dsi_connector(connector);
	struct dsi_display_mode *modes;
	struct drm_display_mode drm_mode;

	if (c_conn->panel) {
		/*
		 * TODO: If drm_panel is attached, query modes from the panel.
		 * This is complicated in split dsi cases because panel is not
		 * attached to both connectors.
		 */
		goto end;
	}

	rc = dsi_display_get_modes(c_conn->display, NULL, &count);
	if (rc) {
		pr_err("[%d] failed to get num of modes, rc=%d\n",
		       c_conn->id, rc);
		goto error;
	}

	size = count * sizeof(*modes);
	modes = kzalloc(size,  GFP_KERNEL);
	if (!modes) {
		count = 0;
		goto end;
	}

	rc = dsi_display_get_modes(c_conn->display, modes, &count);
	if (rc) {
		pr_err("[%d] failed to get modes, rc=%d\n",
		       c_conn->id, rc);
		count = 0;
		goto error;
	}

	for (i = 0; i < count; i++) {
		struct drm_display_mode *m;

		memset(&drm_mode, 0x0, sizeof(drm_mode));
		convert_to_drm_mode(&modes[i], &drm_mode);
		m = drm_mode_duplicate(connector->dev, &drm_mode);
		if (!m) {
			pr_err("[%d] failed to add mode %ux%u\n",
			       c_conn->id, drm_mode.hdisplay,
			       drm_mode.vdisplay);
			count = -ENOMEM;
			goto error;
		}
		m->width_mm = connector->display_info.width_mm;
		m->height_mm = connector->display_info.height_mm;
		drm_mode_probed_add(connector, m);
	}
error:
	kfree(modes);
end:
	pr_debug("MODE COUNT =%d\n\n", count);
	return count;
}

static enum drm_mode_status dsi_conn_mode_valid(struct drm_connector *connector,
						struct drm_display_mode *mode)
{
	int rc = 0;
	struct dsi_display_mode dsi_mode;
	struct dsi_connector *c_conn = to_dsi_connector(connector);

	if (!connector || !mode) {
		pr_err("Invalid params\n");
		return MODE_ERROR;
	}

	convert_to_dsi_mode(mode, &dsi_mode);

	rc = dsi_display_validate_mode(c_conn->display, &dsi_mode,
			DSI_VALIDATE_FLAG_ALLOW_ADJUST);
	if (rc) {
		pr_err("[%d] mode not supported, rc=%d\n", c_conn->id, rc);
		return MODE_BAD;
	}

	return MODE_OK;
}

static struct drm_encoder *dsi_conn_best_encoder(struct drm_connector *conn)
{
	struct dsi_connector *connector = to_dsi_connector(conn);
	/*
	 * This is true for now, revisit this code when multiple encoders are
	 * supported.
	 */
	return connector->encoder;
}


static const struct drm_connector_funcs dsi_conn_ops = {
	.dpms =                   drm_atomic_helper_connector_dpms,
	.reset =                  drm_atomic_helper_connector_reset,

	.detect =                 dsi_conn_detect,
	.destroy =                dsi_connector_destroy,
	.fill_modes =             drm_helper_probe_single_connector_modes,
	.atomic_duplicate_state = dsi_connector_atomic_dup_state,
	.atomic_destroy_state =   dsi_connector_atomic_destroy_state,
};

static const struct dsi_connector_helper_funcs dsi_conn_helper_ops = {
	.base = {
		.get_modes    = dsi_connector_get_modes,
		.mode_valid   = dsi_conn_mode_valid,
		.best_encoder = dsi_conn_best_encoder,
	},
};

struct dsi_connector *dsi_drm_connector_init(struct dsi_display *display,
					     struct drm_device *dev,
					     struct dsi_bridge *bridge)
{
	int rc = 0;
	struct dsi_connector *conn;
	struct drm_property *blob;

	conn = kzalloc(sizeof(*conn), GFP_KERNEL);
	if (!conn) {
		rc = -ENOMEM;
		goto error;
	}

	conn->display = display;

	rc = drm_connector_init(dev,
				&conn->base,
				&dsi_conn_ops,
				DRM_MODE_CONNECTOR_DSI);
	if (rc) {
		pr_err("failed to initialize drm connector, rc=%d\n", rc);
		goto error_free_conn;
	}

	conn->base.helper_private = &dsi_conn_helper_ops;

	conn->base.polled = DRM_CONNECTOR_POLL_HPD;
	conn->base.interlace_allowed = 0;
	conn->base.doublescan_allowed = 0;

	rc = drm_connector_register(&conn->base);
	if (rc) {
		pr_err("failed to register drm connector, rc=%d\n", rc);
		goto error_cleanup_conn;
	}

	rc = drm_mode_connector_attach_encoder(&conn->base,
					       bridge->base.encoder);
	if (rc) {
		pr_err("failed to attach encoder to connector, rc=%d\n", rc);
		goto error_unregister_conn;
	}

	conn->encoder = bridge->base.encoder;

	blob = drm_property_create(dev,
				   DRM_MODE_PROP_BLOB | DRM_MODE_PROP_IMMUTABLE,
				   "DISPLAY_TYPE", 0);
	if (!blob) {
		pr_err("failed to create DISPLAY_TYPE property\n");
		goto error_unregister_conn;
	}

	drm_object_attach_property(&conn->base.base, blob, 0);
	conn->display_type = blob;

	return conn;

error_unregister_conn:
	drm_connector_unregister(&conn->base);
error_cleanup_conn:
	drm_connector_cleanup(&conn->base);
error_free_conn:
	kfree(conn);
error:
	return ERR_PTR(rc);
}

void dsi_drm_connector_cleanup(struct dsi_connector *conn)
{
	drm_connector_unregister(&conn->base);
	drm_connector_cleanup(&conn->base);
	kfree(conn);
}

struct dsi_bridge *dsi_drm_bridge_init(struct dsi_display *display,
				       struct drm_device *dev,
				       struct drm_encoder *encoder)
{
	int rc = 0;
	struct dsi_bridge *bridge;

	bridge = kzalloc(sizeof(*bridge), GFP_KERNEL);
	if (!bridge) {
		rc = -ENOMEM;
		goto error;
	}

	bridge->display = display;
	bridge->base.funcs = &dsi_bridge_ops;
	bridge->base.encoder = encoder;

	rc = drm_bridge_attach(dev, &bridge->base);
	if (rc) {
		pr_err("failed to attach bridge, rc=%d\n", rc);
		goto error_free_bridge;
	}

	encoder->bridge = &bridge->base;
	return bridge;
error_free_bridge:
	kfree(bridge);
error:
	return ERR_PTR(rc);
}

void dsi_drm_bridge_cleanup(struct dsi_bridge *bridge)
{
	if (bridge && bridge->base.encoder)
		bridge->base.encoder->bridge = NULL;

	kfree(bridge);
}
