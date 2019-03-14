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

#define pr_fmt(fmt)	"[drm-shd] %s: " fmt, __func__

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/component.h>
#include <linux/of_irq.h>
#include "sde_connector.h"
#include <drm/drm_atomic_helper.h>
#include <drm/drm_atomic.h>
#include <drm/drm_crtc.h>

#include "msm_drv.h"
#include "msm_kms.h"
#include "sde_connector.h"
#include "sde_encoder.h"
#include "sde_crtc.h"
#include "sde_shd.h"
#include "sde_splash.h"

#define SHD_DEBUG(fmt, ...) pr_debug(fmt, ##__VA_ARGS__)

static LIST_HEAD(g_base_list);

static const struct of_device_id shd_dt_match[] = {
	{.compatible = "qcom,shared-display"},
	{}
};

struct shd_bridge {
	struct drm_bridge base;
	struct shd_display *display;
};

int shd_display_get_num_of_displays(void)
{
	int display_num = 0;
	struct shd_display *disp;
	struct shd_display_base *base;

	list_for_each_entry(base, &g_base_list, head) {
		list_for_each_entry(disp, &base->disp_list, head)
			++display_num;
	}

	return display_num;
}

int shd_display_get_displays(void **displays, int count)
{
	int display_num = 0;
	struct shd_display *disp;
	struct shd_display_base *base;

	list_for_each_entry(base, &g_base_list, head)
		list_for_each_entry(disp, &base->disp_list, head)
			displays[display_num++] = disp;

	return display_num;
}

static enum drm_connector_status shd_display_base_detect(
		struct drm_connector *connector,
		bool force,
		void *disp)
{
	return connector_status_disconnected;
}

static int shd_display_init_base_connector(struct drm_device *dev,
						struct shd_display_base *base)
{
	struct drm_encoder *encoder;
	struct drm_connector *connector;
	struct sde_connector *sde_conn;
	int rc = 0;

	list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
		sde_conn = to_sde_connector(connector);
		encoder = sde_conn->encoder;
		if (encoder == base->encoder) {
			base->connector = connector;
			break;
		}
	}

	if (!base->connector) {
		SDE_ERROR("failed to find connector\n");
		rc = -ENOENT;
		goto error;
	}

	/* set base connector disconnected*/
	sde_conn = to_sde_connector(base->connector);
	base->ops = sde_conn->ops;
	sde_conn->ops.detect = shd_display_base_detect;

	SHD_DEBUG("found base connector %d\n", base->connector->base.id);

error:
	return rc;
}

static int shd_display_init_base_encoder(struct drm_device *dev,
						struct shd_display_base *base)
{
	struct drm_encoder *encoder;
	struct sde_encoder_hw_resources hw_res;
	struct sde_connector_state conn_state = {};
	int i, rc = 0;

	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		sde_encoder_get_hw_resources(encoder,
				&hw_res, &conn_state.base);
		for (i = 0; i < INTF_MAX; i++) {
			if (hw_res.intfs[i] != INTF_MODE_NONE &&
					base->intf_idx == i) {
				base->encoder = encoder;
				goto found;
			}
		}
	}

	if (!base->encoder) {
		pr_err("can't find base encoder for intf %d\n",
			base->intf_idx);
		rc = -ENOENT;
		goto error;
	}

found:
	switch (base->encoder->encoder_type) {
	case DRM_MODE_ENCODER_DSI:
		base->connector_type = DRM_MODE_CONNECTOR_DSI;
		break;
	case DRM_MODE_ENCODER_TMDS:
		base->connector_type = DRM_MODE_CONNECTOR_HDMIA;
		break;
	default:
		base->connector_type = DRM_MODE_CONNECTOR_Unknown;
		break;
	}

	SHD_DEBUG("found base encoder %d, type %d, connect type %d\n",
			base->encoder->base.id,
			base->encoder->encoder_type,
			base->connector_type);

error:
	return rc;
}

static int shd_display_init_base_crtc(struct drm_device *dev,
						struct shd_display_base *base)
{
	struct drm_crtc *crtc;
	struct drm_display_mode *drm_mode;
	int rc = 0;

	crtc = list_last_entry(&dev->mode_config.crtc_list,
		struct drm_crtc, head);

	base->crtc = crtc;
	base->encoder->crtc = crtc;
	SHD_DEBUG("found base crtc %d\n", crtc->base.id);

	/* hide crtc from user */
	list_del_init(&crtc->head);

	/* fixed mode is used */
	drm_mode = &base->mode;

	/* update crtc drm structure */
	crtc->state->active = true;
	rc = drm_atomic_set_mode_for_crtc(crtc->state, drm_mode);
	if (rc) {
		SDE_ERROR("Failed: set mode for crtc. rc = %d\n", rc);
		goto error;
	}
	drm_mode_copy(&crtc->state->adjusted_mode, drm_mode);
	drm_mode_copy(&crtc->mode, drm_mode);

	crtc->state->active_changed = true;
	crtc->state->mode_changed = true;
	crtc->state->connectors_changed = true;

	if (base->connector) {
		base->connector->state->crtc = crtc;
		base->connector->state->best_encoder = base->encoder;
		base->connector->encoder = base->encoder;
	}

error:
	return rc;
}

static void shd_display_enable_base(struct drm_device *dev,
						struct shd_display_base *base)
{
	const struct drm_encoder_helper_funcs *enc_funcs;
	const struct drm_crtc_helper_funcs *crtc_funcs;
	struct drm_display_mode *adjusted_mode;
	struct sde_crtc *sde_crtc;
	struct sde_hw_mixer_cfg lm_cfg;
	struct sde_hw_mixer *hw_lm;
	int rc, i;

	SHD_DEBUG("enable base display %d\n", base->intf_idx);

	enc_funcs = base->encoder->helper_private;
	if (!enc_funcs) {
		SDE_ERROR("failed to find encoder helper\n");
		return;
	}

	crtc_funcs = base->crtc->helper_private;
	if (!crtc_funcs) {
		SDE_ERROR("failed to find crtc helper\n");
		return;
	}

	if (!base->connector) {
		SDE_ERROR("failed to find base connector\n");
		return;
	}

	adjusted_mode = drm_mode_duplicate(dev, &base->mode);
	if (!adjusted_mode) {
		SDE_ERROR("failed to create adjusted mode\n");
		return;
	}

	drm_bridge_mode_fixup(base->encoder->bridge,
		&base->mode,
		adjusted_mode);

	if (enc_funcs->atomic_check) {
		rc = enc_funcs->atomic_check(base->encoder,
			base->crtc->state,
			base->connector->state);
		if (rc) {
			SDE_ERROR("encoder atomic check failed\n");
			goto state_fail;
		}
	}

	if (enc_funcs->mode_fixup) {
		enc_funcs->mode_fixup(base->encoder,
			&base->mode,
			adjusted_mode);
	}

	if (enc_funcs->mode_set) {
		enc_funcs->mode_set(base->encoder,
			&base->mode,
			adjusted_mode);
	}

	if (crtc_funcs->atomic_begin) {
		crtc_funcs->atomic_begin(base->crtc,
			base->crtc->state);
	}

	sde_crtc = to_sde_crtc(base->crtc);
	if (!sde_crtc->num_mixers) {
		SDE_ERROR("no layer mixer found\n");
		goto state_fail;
	}

	lm_cfg.out_width = base->mode.hdisplay / sde_crtc->num_mixers;
	lm_cfg.out_height = base->mode.vdisplay;
	lm_cfg.flags = 0;
	for (i = 0; i < sde_crtc->num_mixers; i++) {
		lm_cfg.right_mixer = i;
		hw_lm = sde_crtc->mixers[i].hw_lm;
		hw_lm->ops.setup_mixer_out(hw_lm, &lm_cfg);
	}

	drm_bridge_mode_set(base->encoder->bridge,
		&base->mode,
		adjusted_mode);

	drm_bridge_pre_enable(base->encoder->bridge);

	if (enc_funcs->enable)
		enc_funcs->enable(base->encoder);

	sde_encoder_kickoff(base->encoder);

	drm_bridge_enable(base->encoder->bridge);

	base->enabled = true;

state_fail:
	drm_mode_destroy(dev, adjusted_mode);
}

static void shd_display_disable_base(struct drm_device *dev,
						struct shd_display_base *base)
{
	const struct drm_encoder_helper_funcs *enc_funcs;

	SHD_DEBUG("disable base display %d\n", base->intf_idx);

	enc_funcs = base->encoder->helper_private;
	if (!enc_funcs) {
		SDE_ERROR("failed to find encoder helper\n");
		return;
	}

	drm_bridge_disable(base->encoder->bridge);

	if (enc_funcs->disable)
		enc_funcs->disable(base->encoder);

	drm_bridge_post_disable(base->encoder->bridge);

	base->enabled = false;
}

static void shd_display_enable(struct shd_display *display)
{
	struct drm_device *dev = display->drm_dev;
	struct shd_display_base *base = display->base;

	SHD_DEBUG("enable %s conn %d\n", display->name,
					DRMID(display->connector));

	mutex_lock(&base->base_mutex);

	display->enabled = true;

	if (!base->enabled) {
		shd_display_enable_base(dev, base);
		/*
		 * Since base display is enabled, and it's marked to have
		 * splash on, but it's not available to user. So for early
		 * splash case, it's needed to update total registered
		 * connector number to reflect the true case to make handoff
		 * can finish.
		 */
		sde_splash_decrease_connector_cnt(dev, base->connector_type,
						display->cont_splash_enabled);
	}

	mutex_unlock(&base->base_mutex);
}

static void shd_display_disable(struct shd_display *display)
{
	struct drm_device *dev = display->drm_dev;
	struct shd_display_base *base = display->base;
	struct shd_display *p;
	bool enabled = false;

	SHD_DEBUG("disable %s conn %d\n", display->name,
					DRMID(display->connector));

	mutex_lock(&base->base_mutex);

	display->enabled = false;

	if (!base->enabled)
		goto end;

	list_for_each_entry(p, &base->disp_list, head) {
		if (p->enabled) {
			enabled = true;
			break;
		}
	}

	if (!enabled)
		shd_display_disable_base(dev, base);

end:
	mutex_unlock(&base->base_mutex);
}

void shd_display_prepare_commit(struct sde_kms *sde_kms,
		struct drm_atomic_state *state)
{
	struct drm_connector *connector;
	struct drm_connector_state *old_conn_state;
	int i;

	if (!sde_kms->shd_display_count)
		return;

	for_each_connector_in_state(state, connector, old_conn_state, i) {
		struct sde_connector *sde_conn;

		sde_conn = to_sde_connector(connector);
		if (!sde_conn->is_shared)
			continue;

		if (!connector->state->best_encoder)
			continue;

		if (!connector->state->crtc->state->active ||
		    !drm_atomic_crtc_needs_modeset(
				    connector->state->crtc->state))
			continue;

		shd_display_enable(sde_conn->display);
	}
}

void shd_display_complete_commit(struct sde_kms *sde_kms,
		struct drm_atomic_state *state)
{
	struct drm_connector *connector;
	struct drm_connector_state *old_conn_state;
	int i;

	if (!sde_kms->shd_display_count)
		return;

	for_each_connector_in_state(state, connector, old_conn_state, i) {
		struct sde_connector *sde_conn;
		struct drm_crtc_state *old_crtc_state;
		unsigned int crtc_idx;

		sde_conn = to_sde_connector(connector);
		if (!sde_conn->is_shared)
			continue;

		if (!old_conn_state->crtc)
			continue;

		crtc_idx = drm_crtc_index(old_conn_state->crtc);
		old_crtc_state = state->crtc_states[crtc_idx];

		if (!old_crtc_state->active ||
		    !drm_atomic_crtc_needs_modeset(old_conn_state->crtc->state))
			continue;

		if (old_conn_state->crtc->state->active)
			continue;

		shd_display_disable(sde_conn->display);
	}
}

int shd_display_post_init(struct sde_kms *sde_kms)
{
	struct shd_display *disp;
	struct shd_display_base *base;
	int rc = 0, i;

	for (i = 0; i < sde_kms->shd_display_count; i++) {
		disp = sde_kms->shd_displays[i];
		base = disp->base;

		if (base->crtc)
			continue;

		rc = shd_display_init_base_crtc(disp->drm_dev, base);
		if (rc) {
			SDE_ERROR("failed initialize base crtc\n");
			break;
		}
	}

	return rc;
}

int shd_connector_get_info(struct msm_display_info *info, void *data)
{
	struct shd_display *display = data;
	int rc;

	if (!info || !data || !display->base) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	if (!display->base->encoder) {
		rc = shd_display_init_base_encoder(display->drm_dev,
						display->base);
		if (rc) {
			SDE_ERROR("failed to find base encoder\n");
			return rc;
		}

		rc = shd_display_init_base_connector(display->drm_dev,
						display->base);
		if (rc) {
			SDE_ERROR("failed to find base connector\n");
			return rc;
		}
	}

	info->intf_type = display->base->connector_type;
	info->capabilities = MSM_DISPLAY_CAP_VID_MODE |
				MSM_DISPLAY_CAP_HOT_PLUG;
	info->is_connected = true;
	info->num_of_h_tiles = 1;
	info->h_tile_instance[0] = display->base->intf_idx;
	info->capabilities |= MSM_DISPLAY_CAP_SHARED;

	return 0;
}

int shd_connector_post_init(struct drm_connector *connector,
		void *info,
		void *display)
{
	struct shd_display *disp = display;
	struct sde_connector *conn;

	disp->connector = connector;
	conn = to_sde_connector(connector);
	conn->is_shared = true;
	conn->shared_roi = disp->roi;

	sde_kms_info_add_keyint(info, "max_blendstages",
				disp->stage_range.size);

	sde_kms_info_add_keystr(info, "display type",
				disp->display_type);

	if (disp->src.h != disp->roi.h) {
		sde_kms_info_add_keyint(info, "padding height",
				disp->roi.h);
	}

	return 0;
}

enum drm_connector_status shd_connector_detect(struct drm_connector *conn,
		bool force,
		void *display)
{
	struct shd_display *disp = display;
	struct sde_connector *sde_conn;
	enum drm_connector_status status = connector_status_disconnected;

	if (!conn || !display || !disp->base) {
		pr_err("invalid params\n");
		goto end;
	}

	mutex_lock(&disp->base->base_mutex);
	if (disp->base->connector) {
		sde_conn = to_sde_connector(disp->base->connector);
		status = disp->base->ops.detect(disp->base->connector,
						force, sde_conn->display);
	}
	mutex_unlock(&disp->base->base_mutex);

end:
	return status;
}

int shd_connector_get_modes(struct drm_connector *connector,
		void *display)
{
	struct drm_display_mode drm_mode;
	struct shd_display *disp = display;
	struct drm_display_mode *m;

	memcpy(&drm_mode, &disp->base->mode, sizeof(drm_mode));

	drm_mode.hdisplay = disp->src.w;
	drm_mode.hsync_start = drm_mode.hdisplay;
	drm_mode.hsync_end = drm_mode.hsync_start;
	drm_mode.htotal = drm_mode.hsync_end;

	drm_mode.vdisplay = disp->src.h;
	drm_mode.vsync_start = drm_mode.vdisplay;
	drm_mode.vsync_end = drm_mode.vsync_start;
	drm_mode.vtotal = drm_mode.vsync_end;

	m = drm_mode_duplicate(disp->drm_dev, &drm_mode);
	drm_mode_set_name(m);
	drm_mode_probed_add(connector, m);

	return 1;
}

enum drm_mode_status shd_connector_mode_valid(struct drm_connector *connector,
		struct drm_display_mode *mode,
		void *display)
{
	return MODE_OK;
}

static int shd_bridge_attach(struct drm_bridge *shd_bridge)
{
	return 0;
}

static void shd_bridge_pre_enable(struct drm_bridge *drm_bridge)
{
}

static void shd_bridge_enable(struct drm_bridge *drm_bridge)
{
}

static void shd_bridge_disable(struct drm_bridge *drm_bridge)
{
}

static void shd_bridge_post_disable(struct drm_bridge *drm_bridge)
{
}


static void shd_bridge_mode_set(struct drm_bridge *drm_bridge,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode)
{
}

static bool shd_bridge_mode_fixup(struct drm_bridge *drm_bridge,
				  const struct drm_display_mode *mode,
				  struct drm_display_mode *adjusted_mode)
{
	return true;
}

static const struct drm_bridge_funcs shd_bridge_ops = {
	.attach       = shd_bridge_attach,
	.mode_fixup   = shd_bridge_mode_fixup,
	.pre_enable   = shd_bridge_pre_enable,
	.enable       = shd_bridge_enable,
	.disable      = shd_bridge_disable,
	.post_disable = shd_bridge_post_disable,
	.mode_set     = shd_bridge_mode_set,
};

int shd_drm_bridge_init(void *data, struct drm_encoder *encoder)
{
	int rc = 0;
	struct shd_bridge *bridge;
	struct drm_device *dev;
	struct shd_display *display = data;
	struct msm_drm_private *priv = NULL;

	bridge = kzalloc(sizeof(*bridge), GFP_KERNEL);
	if (!bridge) {
		rc = -ENOMEM;
		goto error;
	}

	dev = display->drm_dev;
	bridge->display = display;
	bridge->base.funcs = &shd_bridge_ops;
	bridge->base.encoder = encoder;

	priv = dev->dev_private;

	rc = drm_bridge_attach(dev, &bridge->base);
	if (rc) {
		SDE_ERROR("failed to attach bridge, rc=%d\n", rc);
		goto error_free_bridge;
	}

	encoder->bridge = &bridge->base;
	priv->bridges[priv->num_bridges++] = &bridge->base;
	display->bridge = &bridge->base;

	return 0;

error_free_bridge:
	kfree(bridge);
error:
	return rc;
}

void shd_drm_bridge_deinit(void *data)
{
	struct shd_display *display = data;
	struct shd_bridge *bridge = container_of(display->bridge,
		struct shd_bridge, base);

	if (bridge && bridge->base.encoder)
		bridge->base.encoder->bridge = NULL;

	kfree(bridge);
}

/**
 * sde_shd_bind - bind writeback device with controlling device
 * @dev:        Pointer to base of platform device
 * @master:     Pointer to container of drm device
 * @data:       Pointer to private data
 * Returns:     Zero on success
 */
static int sde_shd_bind(struct device *dev, struct device *master, void *data)
{
	struct shd_display *shd_dev;

	shd_dev = platform_get_drvdata(to_platform_device(dev));
	if (!shd_dev) {
		SDE_ERROR("invalid shd device\n");
		return -EINVAL;
	}

	shd_dev->drm_dev = dev_get_drvdata(master);

	return 0;
}

/**
 * sde_shd_unbind - unbind writeback from controlling device
 * @dev:        Pointer to base of platform device
 * @master:     Pointer to container of drm device
 * @data:       Pointer to private data
 */
static void sde_shd_unbind(struct device *dev,
		struct device *master, void *data)
{
	struct shd_display *shd_dev;

	shd_dev = platform_get_drvdata(to_platform_device(dev));
	if (!shd_dev) {
		SDE_ERROR("invalid shd device\n");
		return;
	}

	shd_dev->drm_dev = NULL;
}

static const struct component_ops sde_shd_comp_ops = {
	.bind = sde_shd_bind,
	.unbind = sde_shd_unbind,
};

static int sde_shd_parse_display(struct shd_display *display)
{
	struct device_node *of_node = display->pdev->dev.of_node;
	struct device_node *of_src, *of_roi;
	u32 src_w, src_h, dst_x, dst_y, dst_w, dst_h;
	u32 range[2];
	int rc;

	display->name = of_node->full_name;

	display->display_type = of_get_property(of_node,
						"qcom,display-type", NULL);
	if (!display->display_type)
		display->display_type = "unknown";

	display->base_of = of_parse_phandle(of_node,
		"qcom,shared-display-base", 0);
	if (!display->base_of) {
		pr_err("No base device present\n");
		rc = -ENODEV;
		goto error;
	}

	of_src = of_get_child_by_name(of_node, "qcom,shared-display-src-mode");
	if (!of_src) {
		pr_err("No src mode present\n");
		rc = -ENODEV;
		goto error;
	}

	rc = of_property_read_u32(of_src, "qcom,mode-h-active",
		&src_w);
	if (rc) {
		pr_err("Failed to parse h active\n");
		goto error;
	}

	rc = of_property_read_u32(of_src, "qcom,mode-v-active",
		&src_h);
	if (rc) {
		pr_err("Failed to parse v active\n");
		goto error;
	}

	of_roi = of_get_child_by_name(of_node, "qcom,shared-display-dst-mode");
	if (!of_roi) {
		pr_err("No roi mode present\n");
		rc = -ENODEV;
		goto error;
	}

	rc = of_property_read_u32(of_roi, "qcom,mode-x-offset",
		&dst_x);
	if (rc) {
		pr_err("Failed to parse x offset\n");
		goto error;
	}

	rc = of_property_read_u32(of_roi, "qcom,mode-y-offset",
		&dst_y);
	if (rc) {
		pr_err("Failed to parse y offset\n");
		goto error;
	}

	rc = of_property_read_u32(of_roi, "qcom,mode-width",
		&dst_w);
	if (rc) {
		pr_err("Failed to parse roi width\n");
		goto error;
	}

	rc = of_property_read_u32(of_roi, "qcom,mode-height",
		&dst_h);
	if (rc) {
		pr_err("Failed to parse roi height\n");
		goto error;
	}

	rc = of_property_read_u32_array(of_node, "qcom,blend-stage-range",
		range, 2);
	if (rc)
		pr_err("Failed to parse blend stage range\n");

	display->src.w = src_w;
	display->src.h = src_h;
	display->roi.x = dst_x;
	display->roi.y = dst_y;
	display->roi.w = dst_w;
	display->roi.h = dst_h;
	display->stage_range.start = range[0];
	display->stage_range.size = range[1];

	SHD_DEBUG("%s src %dx%d dst %d,%d %dx%d range %d-%d\n", display->name,
		display->src.w, display->src.h,
		display->roi.x, display->roi.y,
		display->roi.w, display->roi.h,
		display->stage_range.start,
		display->stage_range.size);

error:
	return rc;
}

static int sde_shd_parse_base(struct shd_display_base *base)
{
	struct device_node *of_node = base->of_node;
	struct device_node *node;
	struct drm_display_mode *mode = &base->mode;
	u32 h_front_porch, h_pulse_width, h_back_porch;
	u32 v_front_porch, v_pulse_width, v_back_porch;
	bool h_active_high, v_active_high;
	u32 flags = 0;
	int rc;

	rc = of_property_read_u32(of_node, "qcom,shared-display-base-intf",
					&base->intf_idx);
	if (rc) {
		pr_err("failed to read base intf, rc=%d\n", rc);
		goto fail;
	}

	node = of_get_child_by_name(of_node, "qcom,shared-display-base-mode");
	if (!node) {
		pr_err("No base mode present\n");
		rc = -ENODEV;
		goto fail;
	}

	rc = of_property_read_u32(node, "qcom,mode-h-active",
					&mode->hdisplay);
	if (rc) {
		SDE_ERROR("failed to read h-active, rc=%d\n", rc);
		goto fail;
	}

	rc = of_property_read_u32(node, "qcom,mode-h-front-porch",
					&h_front_porch);
	if (rc) {
		SDE_ERROR("failed to read h-front-porch, rc=%d\n", rc);
		goto fail;
	}

	rc = of_property_read_u32(node, "qcom,mode-h-pulse-width",
					&h_pulse_width);
	if (rc) {
		SDE_ERROR("failed to read h-pulse-width, rc=%d\n", rc);
		goto fail;
	}

	rc = of_property_read_u32(node, "qcom,mode-h-back-porch",
					&h_back_porch);
	if (rc) {
		SDE_ERROR("failed to read h-back-porch, rc=%d\n", rc);
		goto fail;
	}

	h_active_high = of_property_read_bool(node,
					"qcom,mode-h-active-high");

	rc = of_property_read_u32(node, "qcom,mode-v-active",
					&mode->vdisplay);
	if (rc) {
		SDE_ERROR("failed to read v-active, rc=%d\n", rc);
		goto fail;
	}

	rc = of_property_read_u32(node, "qcom,mode-v-front-porch",
					&v_front_porch);
	if (rc) {
		SDE_ERROR("failed to read v-front-porch, rc=%d\n", rc);
		goto fail;
	}

	rc = of_property_read_u32(node, "qcom,mode-v-pulse-width",
					&v_pulse_width);
	if (rc) {
		SDE_ERROR("failed to read v-pulse-width, rc=%d\n", rc);
		goto fail;
	}

	rc = of_property_read_u32(node, "qcom,mode-v-back-porch",
					&v_back_porch);
	if (rc) {
		SDE_ERROR("failed to read v-back-porch, rc=%d\n", rc);
		goto fail;
	}

	v_active_high = of_property_read_bool(node,
					"qcom,mode-v-active-high");

	rc = of_property_read_u32(node, "qcom,mode-refresh-rate",
					&mode->vrefresh);
	if (rc) {
		SDE_ERROR("failed to read refresh-rate, rc=%d\n", rc);
		goto fail;
	}

	rc = of_property_read_u32(node, "qcom,mode-clock-in-khz",
					&mode->clock);
	if (rc) {
		SDE_ERROR("failed to read clock, rc=%d\n", rc);
		goto fail;
	}

	mode->hsync_start = mode->hdisplay + h_front_porch;
	mode->hsync_end = mode->hsync_start + h_pulse_width;
	mode->htotal = mode->hsync_end + h_back_porch;
	mode->vsync_start = mode->vdisplay + v_front_porch;
	mode->vsync_end = mode->vsync_start + v_pulse_width;
	mode->vtotal = mode->vsync_end + v_back_porch;
	if (h_active_high)
		flags |= DRM_MODE_FLAG_PHSYNC;
	else
		flags |= DRM_MODE_FLAG_NHSYNC;
	if (v_active_high)
		flags |= DRM_MODE_FLAG_PVSYNC;
	else
		flags |= DRM_MODE_FLAG_NVSYNC;
	mode->flags = flags;

	SHD_DEBUG("base mode h[%d,%d,%d,%d] v[%d,%d,%d,%d] %d %xH %d\n",
		mode->hdisplay, mode->hsync_start,
		mode->hsync_end, mode->htotal, mode->vdisplay,
		mode->vsync_start, mode->vsync_end, mode->vtotal,
		mode->vrefresh, mode->flags, mode->clock);

fail:
	return rc;
}

/**
 * sde_shd_probe - load shared display module
 * @pdev:	Pointer to platform device
 */
static int sde_shd_probe(struct platform_device *pdev)
{
	struct shd_display *shd_dev;
	struct shd_display_base *base;
	int ret;

	shd_dev = devm_kzalloc(&pdev->dev, sizeof(*shd_dev), GFP_KERNEL);
	if (!shd_dev)
		return -ENOMEM;

	shd_dev->pdev = pdev;

	ret = sde_shd_parse_display(shd_dev);
	if (ret) {
		SDE_ERROR("failed to parse shared display\n");
		goto error;
	}

	platform_set_drvdata(pdev, shd_dev);

	list_for_each_entry(base, &g_base_list, head) {
		if (base->of_node == shd_dev->base_of)
			goto next;
	}

	base = devm_kzalloc(&pdev->dev, sizeof(*base), GFP_KERNEL);
	if (!base) {
		ret = -ENOMEM;
		goto error;
	}

	mutex_init(&base->base_mutex);
	INIT_LIST_HEAD(&base->disp_list);
	base->of_node = shd_dev->base_of;

	ret = sde_shd_parse_base(base);
	if (ret) {
		SDE_ERROR("failed to parse shared display base\n");
		goto base_error;
	}

	list_add_tail(&base->head, &g_base_list);

next:
	shd_dev->base = base;
	list_add_tail(&shd_dev->head, &base->disp_list);
	SHD_DEBUG("add shd to intf %d\n", base->intf_idx);

	ret = component_add(&pdev->dev, &sde_shd_comp_ops);
	if (ret) {
		goto base_error;
		pr_err("component add failed\n");
	}

	return 0;

base_error:
	devm_kfree(&pdev->dev, base);
error:
	devm_kfree(&pdev->dev, shd_dev);
	return ret;
}

/**
 * sde_shd_remove - unload shared display module
 * @pdev:	Pointer to platform device
 */
static int sde_shd_remove(struct platform_device *pdev)
{
	struct shd_display *shd_dev;

	shd_dev = platform_get_drvdata(pdev);
	if (!shd_dev)
		return 0;

	SHD_DEBUG("\n");

	mutex_lock(&shd_dev->base->base_mutex);
	list_del_init(&shd_dev->head);
	if (list_empty(&shd_dev->base->disp_list)) {
		list_del_init(&shd_dev->base->head);
		mutex_unlock(&shd_dev->base->base_mutex);
		devm_kfree(&pdev->dev, shd_dev->base);
	} else
		mutex_unlock(&shd_dev->base->base_mutex);

	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, shd_dev);

	return 0;
}

static const struct of_device_id dt_match[] = {
	{ .compatible = "qcom,shared-display"},
	{}
};

static struct platform_driver sde_shd_driver = {
	.probe = sde_shd_probe,
	.remove = sde_shd_remove,
	.driver = {
		.name = "sde_shd",
		.of_match_table = dt_match,
		.suppress_bind_attrs = true,
	},
};

static int __init sde_shd_register(void)
{
	return platform_driver_register(&sde_shd_driver);
}

static void __exit sde_shd_unregister(void)
{
	platform_driver_unregister(&sde_shd_driver);
}

module_init(sde_shd_register);
module_exit(sde_shd_unregister);
MODULE_LICENSE("GPL v2");
