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

#include <video/msm_dba.h>
#include "drm_edid.h"
#include "sde_kms.h"
#include "dba_bridge.h"

#define pr_fmt(fmt)	"dba_bridge:[%s] " fmt, __func__

/**
 * struct dba_bridge - DBA bridge information
 * @base:               drm_bridge base
 * @client_name:        Client's name who calls the init
 * @chip_name:          Bridge chip name
 * @name:               Bridge chip name
 * @id:                 Bridge driver index
 * @display:            Private display handle
 * @list:               Bridge chip driver list node
 * @ops:                DBA operation container
 * @dba_ctx:            DBA context
 * @mode:               DRM mode info
 * @hdmi_mode:          HDMI or DVI mode for the sink
 * @num_of_input_lanes: Number of input lanes in case of DSI/LVDS
 * @pluggable:          If it's pluggable
 * @panel_count:        Number of panels attached to this display
 */
struct dba_bridge {
	struct drm_bridge base;
	char client_name[MSM_DBA_CLIENT_NAME_LEN];
	char chip_name[MSM_DBA_CHIP_NAME_MAX_LEN];
	u32 id;
	void *display;
	struct list_head list;
	struct msm_dba_ops ops;
	void *dba_ctx;
	struct drm_display_mode mode;
	bool hdmi_mode;
	u32 num_of_input_lanes;
	bool pluggable;
	u32 panel_count;
};
#define to_dba_bridge(x)     container_of((x), struct dba_bridge, base)

static void _dba_bridge_cb(void *data, enum msm_dba_callback_event event)
{
	struct dba_bridge *d_bridge = data;

	if (!d_bridge) {
		SDE_ERROR("Invalid data\n");
		return;
	}

	DRM_DEBUG("event: %d\n", event);

	switch (event) {
	case MSM_DBA_CB_HPD_CONNECT:
		DRM_DEBUG("HPD CONNECT\n");
		break;
	case MSM_DBA_CB_HPD_DISCONNECT:
		DRM_DEBUG("HPD DISCONNECT\n");
		break;
	default:
		DRM_DEBUG("event:%d is not supported\n", event);
		break;
	}
}

static int _dba_bridge_attach(struct drm_bridge *bridge)
{
	struct dba_bridge *d_bridge = to_dba_bridge(bridge);
	struct msm_dba_reg_info info;
	int ret = 0;

	if (!bridge) {
		SDE_ERROR("Invalid params\n");
		return -EINVAL;
	}

	memset(&info, 0, sizeof(info));
	/* initialize DBA registration data */
	strlcpy(info.client_name, d_bridge->client_name,
					MSM_DBA_CLIENT_NAME_LEN);
	strlcpy(info.chip_name, d_bridge->chip_name,
					MSM_DBA_CHIP_NAME_MAX_LEN);
	info.instance_id = d_bridge->id;
	info.cb = _dba_bridge_cb;
	info.cb_data = d_bridge;

	/* register client with DBA and get device's ops*/
	if (IS_ENABLED(CONFIG_MSM_DBA)) {
		d_bridge->dba_ctx = msm_dba_register_client(&info,
							&d_bridge->ops);
		if (IS_ERR_OR_NULL(d_bridge->dba_ctx)) {
			SDE_ERROR("dba register failed\n");
			ret = PTR_ERR(d_bridge->dba_ctx);
			goto error;
		}
	} else {
		SDE_ERROR("DBA not enabled\n");
		ret = -ENODEV;
		goto error;
	}

	DRM_INFO("client:%s bridge:[%s:%d] attached\n",
		d_bridge->client_name, d_bridge->chip_name, d_bridge->id);

error:
	return ret;
}

static void _dba_bridge_pre_enable(struct drm_bridge *bridge)
{
	if (!bridge) {
		SDE_ERROR("Invalid params\n");
		return;
	}
}

static void _dba_bridge_enable(struct drm_bridge *bridge)
{
	int rc = 0;
	struct dba_bridge *d_bridge = to_dba_bridge(bridge);
	struct msm_dba_video_cfg video_cfg;
	struct drm_display_mode *mode;
	struct hdmi_avi_infoframe avi_frame;

	if (!bridge) {
		SDE_ERROR("Invalid params\n");
		return;
	}

	memset(&video_cfg, 0, sizeof(video_cfg));
	memset(&avi_frame, 0, sizeof(avi_frame));
	mode = &d_bridge->mode;
	video_cfg.h_active = mode->hdisplay;
	video_cfg.v_active = mode->vdisplay;
	video_cfg.h_front_porch = mode->hsync_start - mode->hdisplay;
	video_cfg.v_front_porch = mode->vsync_start - mode->vdisplay;
	video_cfg.h_back_porch = mode->htotal - mode->hsync_end;
	video_cfg.v_back_porch = mode->vtotal - mode->vsync_end;
	video_cfg.h_pulse_width = mode->hsync_end - mode->hsync_start;
	video_cfg.v_pulse_width = mode->vsync_end - mode->vsync_start;
	video_cfg.pclk_khz = mode->clock;
	video_cfg.hdmi_mode = d_bridge->hdmi_mode;
	video_cfg.num_of_input_lanes = d_bridge->num_of_input_lanes;

	SDE_DEBUG(
		"video=h[%d,%d,%d,%d] v[%d,%d,%d,%d] pclk=%d hdmi=%d lane=%d\n",
		video_cfg.h_active, video_cfg.h_front_porch,
		video_cfg.h_pulse_width, video_cfg.h_back_porch,
		video_cfg.v_active, video_cfg.v_front_porch,
		video_cfg.v_pulse_width, video_cfg.v_back_porch,
		video_cfg.pclk_khz, video_cfg.hdmi_mode,
		video_cfg.num_of_input_lanes);

	rc = drm_hdmi_avi_infoframe_from_display_mode(&avi_frame, mode);
	if (rc) {
		SDE_ERROR("get avi frame failed ret=%d\n", rc);
	} else {
		video_cfg.scaninfo = avi_frame.scan_mode;
		switch (avi_frame.picture_aspect) {
		case HDMI_PICTURE_ASPECT_4_3:
			video_cfg.ar = MSM_DBA_AR_4_3;
			break;
		case HDMI_PICTURE_ASPECT_16_9:
			video_cfg.ar = MSM_DBA_AR_16_9;
			break;
		default:
			break;
		}
		video_cfg.vic = avi_frame.video_code;
		DRM_INFO("scaninfo=%d ar=%d vic=%d\n",
			video_cfg.scaninfo, video_cfg.ar, video_cfg.vic);
	}

	if (d_bridge->ops.video_on) {
		rc = d_bridge->ops.video_on(d_bridge->dba_ctx, true,
						&video_cfg, 0);
		if (rc)
			SDE_ERROR("video on failed ret=%d\n", rc);
	}
}

static void _dba_bridge_disable(struct drm_bridge *bridge)
{
	int rc = 0;
	struct dba_bridge *d_bridge = to_dba_bridge(bridge);

	if (!bridge) {
		SDE_ERROR("Invalid params\n");
		return;
	}

	if (d_bridge->ops.video_on) {
		rc = d_bridge->ops.video_on(d_bridge->dba_ctx, false, NULL, 0);
		if (rc)
			SDE_ERROR("video off failed ret=%d\n", rc);
	}
}

static void _dba_bridge_post_disable(struct drm_bridge *bridge)
{
	if (!bridge) {
		SDE_ERROR("Invalid params\n");
		return;
	}
}

static void _dba_bridge_mode_set(struct drm_bridge *bridge,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode)
{
	struct dba_bridge *d_bridge = to_dba_bridge(bridge);

	if (!bridge || !mode || !adjusted_mode || !d_bridge) {
		SDE_ERROR("Invalid params\n");
		return;
	} else if (!d_bridge->panel_count) {
		SDE_ERROR("Panel count is 0\n");
		return;
	}

	d_bridge->mode = *adjusted_mode;
	/* Adjust mode according to number of panels */
	d_bridge->mode.hdisplay /= d_bridge->panel_count;
	d_bridge->mode.hsync_start /= d_bridge->panel_count;
	d_bridge->mode.hsync_end /= d_bridge->panel_count;
	d_bridge->mode.htotal /= d_bridge->panel_count;
	d_bridge->mode.clock /= d_bridge->panel_count;
}

static bool _dba_bridge_mode_fixup(struct drm_bridge *bridge,
				  const struct drm_display_mode *mode,
				  struct drm_display_mode *adjusted_mode)
{
	bool ret = true;

	if (!bridge || !mode || !adjusted_mode) {
		SDE_ERROR("Invalid params\n");
		return false;
	}

	return ret;
}

static const struct drm_bridge_funcs _dba_bridge_ops = {
	.attach       = _dba_bridge_attach,
	.mode_fixup   = _dba_bridge_mode_fixup,
	.pre_enable   = _dba_bridge_pre_enable,
	.enable       = _dba_bridge_enable,
	.disable      = _dba_bridge_disable,
	.post_disable = _dba_bridge_post_disable,
	.mode_set     = _dba_bridge_mode_set,
};

struct drm_bridge *dba_bridge_init(struct drm_device *dev,
				struct drm_encoder *encoder,
				struct dba_bridge_init *data)
{
	int rc = 0;
	struct dba_bridge *bridge;
	struct msm_drm_private *priv = NULL;

	if (!dev || !encoder || !data) {
		SDE_ERROR("dev=%p or encoder=%p or data=%p is NULL\n",
				dev, encoder, data);
		rc = -EINVAL;
		goto error;
	}

	priv = dev->dev_private;
	if (!priv) {
		SDE_ERROR("Private data is not present\n");
		rc = -EINVAL;
		goto error;
	}

	bridge = kzalloc(sizeof(*bridge), GFP_KERNEL);
	if (!bridge) {
		SDE_ERROR("out of memory\n");
		rc = -ENOMEM;
		goto error;
	}

	INIT_LIST_HEAD(&bridge->list);
	strlcpy(bridge->client_name, data->client_name,
					MSM_DBA_CLIENT_NAME_LEN);
	strlcpy(bridge->chip_name, data->chip_name,
					MSM_DBA_CHIP_NAME_MAX_LEN);
	bridge->id = data->id;
	bridge->display = data->display;
	bridge->hdmi_mode = data->hdmi_mode;
	bridge->num_of_input_lanes = data->num_of_input_lanes;
	bridge->pluggable = data->pluggable;
	bridge->panel_count = data->panel_count;
	bridge->base.funcs = &_dba_bridge_ops;
	bridge->base.encoder = encoder;

	rc = drm_bridge_attach(dev, &bridge->base);
	if (rc) {
		SDE_ERROR("failed to attach bridge, rc=%d\n", rc);
		goto error_free_bridge;
	}

	if (data->precede_bridge) {
		/* Insert current bridge */
		bridge->base.next = data->precede_bridge->next;
		data->precede_bridge->next = &bridge->base;
	} else {
		encoder->bridge = &bridge->base;
	}

	if (!bridge->pluggable) {
		if (bridge->ops.power_on)
			bridge->ops.power_on(bridge->dba_ctx, true, 0);
		if (bridge->ops.check_hpd)
			bridge->ops.check_hpd(bridge->dba_ctx, 0);
	}

	return &bridge->base;

error_free_bridge:
	kfree(bridge);
error:
	return ERR_PTR(rc);
}

void dba_bridge_cleanup(struct drm_bridge *bridge)
{
	struct dba_bridge *d_bridge = to_dba_bridge(bridge);

	if (!bridge)
		return;

	if (IS_ENABLED(CONFIG_MSM_DBA)) {
		if (!IS_ERR_OR_NULL(d_bridge->dba_ctx))
			msm_dba_deregister_client(d_bridge->dba_ctx);
	}

	if (d_bridge->base.encoder)
		d_bridge->base.encoder->bridge = NULL;

	kfree(bridge);
}
