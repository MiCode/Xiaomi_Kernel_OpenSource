/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "edrm_connector.h"

struct edrm_connector {
	struct drm_connector base;
	struct drm_encoder *encoder;
	struct msm_edrm_display *display;
};

#define to_edrm_connector(x) container_of(x, struct edrm_connector, base)

static enum drm_connector_status
edrm_connector_detect(struct drm_connector *conn, bool force)
{
	return connector_status_connected;
}

static int
edrm_connector_get_modes(struct drm_connector *connector)
{
	struct edrm_connector *edrm_conn = to_edrm_connector(connector);
	struct drm_display_mode *m;

	m = drm_mode_duplicate(connector->dev, &edrm_conn->display->mode);
	if (m == NULL) {
		pr_err("edrm drm_mode_duplicate failed\n");
		return 0;
	}
	drm_mode_set_name(m);
	drm_mode_probed_add(connector, m);

	return 1;
}

static enum drm_mode_status
edrm_mode_valid(struct drm_connector *connector, struct drm_display_mode *mode)
{
	return MODE_OK;
}

static struct drm_encoder *
edrm_connector_best_encoder(struct drm_connector *connector)
{
	struct edrm_connector *edrm_conn = to_edrm_connector(connector);

	return edrm_conn->encoder;
}

void edrm_connector_destroy(struct drm_connector *connector)
{
	struct edrm_connector *edrm_conn = to_edrm_connector(connector);

	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
	kfree(edrm_conn);
}

static const struct drm_connector_helper_funcs edrm_connector_helper_funcs = {
	.get_modes =    edrm_connector_get_modes,
	.mode_valid =   edrm_mode_valid,
	.best_encoder = edrm_connector_best_encoder,
};

static const struct drm_connector_funcs edrm_connector_funcs = {
	.fill_modes = drm_helper_probe_single_connector_modes,
	.detect = edrm_connector_detect,
	.destroy = edrm_connector_destroy,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

struct drm_connector *edrm_connector_init(struct drm_device *dev,
			struct drm_encoder *encoder,
			struct msm_edrm_display *display)
{
	struct edrm_connector *edrm_conn;
	struct drm_connector *connector;
	int ret;

	edrm_conn = kzalloc(sizeof(*edrm_conn), GFP_KERNEL);
	if (!edrm_conn)
		return ERR_PTR(-ENOMEM);
	connector = &edrm_conn->base;

	ret = drm_connector_init(dev, connector,
			&edrm_connector_funcs,
			display->connector_type);
	if (ret) {
		pr_err("edrm drm_connector_init failed\n");
		goto fail;
	}

	drm_connector_helper_add(connector, &edrm_connector_helper_funcs);

	edrm_conn->display = display;
	edrm_conn->encoder = encoder;

	ret = drm_connector_register(&edrm_conn->base);
	if (ret) {
		pr_err("failed to register drm connector, %d\n", ret);
		goto fail;
	}

	ret = drm_mode_connector_attach_encoder(&edrm_conn->base, encoder);
	if (ret) {
		pr_err("failed to attach encoder to connector, %d\n", ret);
		goto fail;
	}

	return connector;
fail:
	kfree(edrm_conn);
	return ERR_PTR(ret);

}
