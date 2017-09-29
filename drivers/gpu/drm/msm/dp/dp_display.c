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

#define pr_fmt(fmt)	"[drm-dp] %s: " fmt, __func__

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/component.h>
#include <linux/of_irq.h>
#include <linux/hdcp_qseecom.h>

#include "msm_drv.h"
#include "dp_usbpd.h"
#include "dp_parser.h"
#include "dp_power.h"
#include "dp_catalog.h"
#include "dp_aux.h"
#include "dp_link.h"
#include "dp_panel.h"
#include "dp_ctrl.h"
#include "dp_audio.h"
#include "dp_display.h"
#include "sde_hdcp.h"
#include "dp_debug.h"

static struct dp_display *g_dp_display;
#define HPD_STRING_SIZE 30

struct dp_hdcp {
	void *data;
	struct sde_hdcp_ops *ops;

	void *hdcp1;
	void *hdcp2;

	int enc_lvl;

	bool auth_state;
	bool hdcp1_present;
	bool hdcp2_present;
	bool feature_enabled;
};

struct dp_display_private {
	char *name;
	int irq;

	/* state variables */
	bool core_initialized;
	bool power_on;
	bool hpd_irq_on;
	bool audio_supported;

	struct platform_device *pdev;
	struct dentry *root;
	struct completion notification_comp;

	struct dp_usbpd   *usbpd;
	struct dp_parser  *parser;
	struct dp_power   *power;
	struct dp_catalog *catalog;
	struct dp_aux     *aux;
	struct dp_link    *link;
	struct dp_panel   *panel;
	struct dp_ctrl    *ctrl;
	struct dp_audio   *audio;
	struct dp_debug   *debug;

	struct dp_hdcp hdcp;

	struct dp_usbpd_cb usbpd_cb;
	struct dp_display_mode mode;
	struct dp_display dp_display;

	struct workqueue_struct *hdcp_workqueue;
	struct delayed_work hdcp_cb_work;
	struct mutex hdcp_mutex;
	struct mutex session_lock;
	int hdcp_status;
};

static const struct of_device_id dp_dt_match[] = {
	{.compatible = "qcom,dp-display"},
	{}
};

static inline bool dp_display_is_hdcp_enabled(struct dp_display_private *dp)
{
	return dp->hdcp.feature_enabled &&
		(dp->hdcp.hdcp1_present || dp->hdcp.hdcp2_present) &&
		dp->hdcp.ops;
}

static irqreturn_t dp_display_irq(int irq, void *dev_id)
{
	struct dp_display_private *dp = dev_id;

	if (!dp) {
		pr_err("invalid data\n");
		return IRQ_NONE;
	}

	/* DP controller isr */
	dp->ctrl->isr(dp->ctrl);

	/* DP aux isr */
	dp->aux->isr(dp->aux);

	/* HDCP isr */
	if (dp_display_is_hdcp_enabled(dp) && dp->hdcp.ops->isr) {
		if (dp->hdcp.ops->isr(dp->hdcp.data))
			pr_err("dp_hdcp_isr failed\n");
	}

	return IRQ_HANDLED;
}

static void dp_display_hdcp_cb_work(struct work_struct *work)
{
	struct dp_display_private *dp;
	struct delayed_work *dw = to_delayed_work(work);
	struct sde_hdcp_ops *ops;
	int rc = 0;
	u32 hdcp_auth_state;

	dp = container_of(dw, struct dp_display_private, hdcp_cb_work);

	rc = dp->catalog->ctrl.read_hdcp_status(&dp->catalog->ctrl);
	if (rc >= 0) {
		hdcp_auth_state = (rc >> 20) & 0x3;
		pr_debug("hdcp auth state %d\n", hdcp_auth_state);
	}

	ops = dp->hdcp.ops;

	switch (dp->hdcp_status) {
	case HDCP_STATE_AUTHENTICATING:
		pr_debug("start authenticaton\n");

		if (dp->hdcp.ops && dp->hdcp.ops->authenticate)
			rc = dp->hdcp.ops->authenticate(dp->hdcp.data);

		break;
	case HDCP_STATE_AUTHENTICATED:
		pr_debug("hdcp authenticated\n");
		dp->hdcp.auth_state = true;
		break;
	case HDCP_STATE_AUTH_FAIL:
		dp->hdcp.auth_state = false;

		if (dp->power_on) {
			pr_debug("Reauthenticating\n");
			if (ops && ops->reauthenticate) {
				rc = ops->reauthenticate(dp->hdcp.data);
				if (rc)
					pr_err("reauth failed rc=%d\n", rc);
			}
		} else {
			pr_debug("not reauthenticating, cable disconnected\n");
		}

		break;
	default:
		break;
	}
}

static void dp_display_notify_hdcp_status_cb(void *ptr,
		enum sde_hdcp_states status)
{
	struct dp_display_private *dp = ptr;

	if (!dp) {
		pr_err("invalid input\n");
		return;
	}

	dp->hdcp_status = status;

	if (dp->dp_display.is_connected)
		queue_delayed_work(dp->hdcp_workqueue, &dp->hdcp_cb_work, HZ/4);
}

static int dp_display_create_hdcp_workqueue(struct dp_display_private *dp)
{
	dp->hdcp_workqueue = create_workqueue("sdm_dp_hdcp");
	if (IS_ERR_OR_NULL(dp->hdcp_workqueue)) {
		pr_err("Error creating hdcp_workqueue\n");
		return -EPERM;
	}

	INIT_DELAYED_WORK(&dp->hdcp_cb_work, dp_display_hdcp_cb_work);

	return 0;
}

static void dp_display_destroy_hdcp_workqueue(struct dp_display_private *dp)
{
	if (dp->hdcp_workqueue)
		destroy_workqueue(dp->hdcp_workqueue);
}

static void dp_display_update_hdcp_info(struct dp_display_private *dp)
{
	void *fd = NULL;
	struct sde_hdcp_ops *ops = NULL;

	if (!dp) {
		pr_err("invalid input\n");
		return;
	}

	if (!dp->hdcp.feature_enabled) {
		pr_debug("feature not enabled\n");
		return;
	}

	fd = dp->hdcp.hdcp2;
	if (fd)
		ops = sde_dp_hdcp2p2_start(fd);

	if (ops && ops->feature_supported)
		dp->hdcp.hdcp2_present = ops->feature_supported(fd);
	else
		dp->hdcp.hdcp2_present = false;

	pr_debug("hdcp2p2: %s\n",
			dp->hdcp.hdcp2_present ? "supported" : "not supported");

	if (!dp->hdcp.hdcp2_present) {
		dp->hdcp.hdcp1_present = hdcp1_check_if_supported_load_app();

		if (dp->hdcp.hdcp1_present) {
			fd = dp->hdcp.hdcp1;
			ops = sde_hdcp_1x_start(fd);
		}
	}

	pr_debug("hdcp1x: %s\n",
			dp->hdcp.hdcp1_present ? "supported" : "not supported");

	if (dp->hdcp.hdcp2_present || dp->hdcp.hdcp1_present) {
		dp->hdcp.data = fd;
		dp->hdcp.ops = ops;
	} else {
		dp->hdcp.data = NULL;
		dp->hdcp.ops = NULL;
	}
}

static void dp_display_deinitialize_hdcp(struct dp_display_private *dp)
{
	if (!dp) {
		pr_err("invalid input\n");
		return;
	}

	sde_dp_hdcp2p2_deinit(dp->hdcp.data);
	dp_display_destroy_hdcp_workqueue(dp);
	if (&dp->hdcp_mutex)
		mutex_destroy(&dp->hdcp_mutex);
}

static int dp_display_initialize_hdcp(struct dp_display_private *dp)
{
	struct sde_hdcp_init_data hdcp_init_data;
	struct resource *res;
	int rc = 0;

	if (!dp) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	mutex_init(&dp->hdcp_mutex);

	rc = dp_display_create_hdcp_workqueue(dp);
	if (rc) {
		pr_err("Failed to create HDCP workqueue\n");
		goto error;
	}

	res = platform_get_resource_byname(dp->pdev,
		IORESOURCE_MEM, "dp_ctrl");
	if (!res) {
		pr_err("Error getting dp ctrl resource\n");
		rc = -EINVAL;
		goto error;
	}

	hdcp_init_data.phy_addr      = res->start;
	hdcp_init_data.client_id     = HDCP_CLIENT_DP;
	hdcp_init_data.drm_aux       = dp->aux->drm_aux;
	hdcp_init_data.cb_data       = (void *)dp;
	hdcp_init_data.workq         = dp->hdcp_workqueue;
	hdcp_init_data.mutex         = &dp->hdcp_mutex;
	hdcp_init_data.sec_access    = true;
	hdcp_init_data.notify_status = dp_display_notify_hdcp_status_cb;
	hdcp_init_data.core_io       = &dp->parser->io.ctrl_io;
	hdcp_init_data.qfprom_io     = &dp->parser->io.qfprom_io;
	hdcp_init_data.hdcp_io       = &dp->parser->io.hdcp_io;
	hdcp_init_data.revision      = &dp->panel->link_info.revision;

	dp->hdcp.hdcp1 = sde_hdcp_1x_init(&hdcp_init_data);
	if (IS_ERR_OR_NULL(dp->hdcp.hdcp1)) {
		pr_err("Error initializing HDCP 1.x\n");
		rc = -EINVAL;
		goto error;
	}

	pr_debug("HDCP 1.3 initialized\n");

	dp->hdcp.hdcp2 = sde_dp_hdcp2p2_init(&hdcp_init_data);
	if (!IS_ERR_OR_NULL(dp->hdcp.hdcp2))
		pr_debug("HDCP 2.2 initialized\n");

	dp->hdcp.feature_enabled = true;

	return 0;
error:
	dp_display_deinitialize_hdcp(dp);
	return rc;
}

static int dp_display_bind(struct device *dev, struct device *master,
		void *data)
{
	int rc = 0;
	struct dp_display_private *dp;
	struct drm_device *drm;
	struct msm_drm_private *priv;
	struct platform_device *pdev = to_platform_device(dev);

	if (!dev || !pdev || !master) {
		pr_err("invalid param(s), dev %pK, pdev %pK, master %pK\n",
				dev, pdev, master);
		rc = -EINVAL;
		goto end;
	}

	drm = dev_get_drvdata(master);
	dp = platform_get_drvdata(pdev);
	if (!drm || !dp) {
		pr_err("invalid param(s), drm %pK, dp %pK\n",
				drm, dp);
		rc = -EINVAL;
		goto end;
	}

	dp->dp_display.drm_dev = drm;
	priv = drm->dev_private;

	rc = dp->parser->parse(dp->parser);
	if (rc) {
		pr_err("device tree parsing failed\n");
		goto end;
	}

	rc = dp->aux->drm_aux_register(dp->aux);
	if (rc) {
		pr_err("DRM DP AUX register failed\n");
		goto end;
	}

	rc = dp->panel->sde_edid_register(dp->panel);
	if (rc) {
		pr_err("DRM DP EDID register failed\n");
		goto end;
	}

	rc = dp->power->power_client_init(dp->power, &priv->phandle);
	if (rc) {
		pr_err("Power client create failed\n");
		goto end;
	}

	rc = dp_display_initialize_hdcp(dp);
	if (rc) {
		pr_err("HDCP initialization failed\n");
		goto end;
	}
end:
	return rc;
}

static void dp_display_unbind(struct device *dev, struct device *master,
		void *data)
{
	struct dp_display_private *dp;
	struct platform_device *pdev = to_platform_device(dev);

	if (!dev || !pdev) {
		pr_err("invalid param(s)\n");
		return;
	}

	dp = platform_get_drvdata(pdev);
	if (!dp) {
		pr_err("Invalid params\n");
		return;
	}

	(void)dp->power->power_client_deinit(dp->power);
	(void)dp->panel->sde_edid_deregister(dp->panel);
	(void)dp->aux->drm_aux_deregister(dp->aux);
	dp_display_deinitialize_hdcp(dp);
}

static const struct component_ops dp_display_comp_ops = {
	.bind = dp_display_bind,
	.unbind = dp_display_unbind,
};

static bool dp_display_is_ds_bridge(struct dp_panel *panel)
{
	return (panel->dpcd[DP_DOWNSTREAMPORT_PRESENT] &
		DP_DWN_STRM_PORT_PRESENT);
}

static bool dp_display_is_sink_count_zero(struct dp_display_private *dp)
{
	return dp_display_is_ds_bridge(dp->panel) &&
		(dp->link->sink_count.count == 0);
}

static void dp_display_send_hpd_event(struct dp_display *dp_display)
{
	struct drm_device *dev = NULL;
	struct dp_display_private *dp;
	struct drm_connector *connector;
	char name[HPD_STRING_SIZE], status[HPD_STRING_SIZE],
		bpp[HPD_STRING_SIZE], pattern[HPD_STRING_SIZE];
	char *envp[5];

	if (!dp_display) {
		pr_err("invalid input\n");
		return;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);
	if (!dp) {
		pr_err("invalid params\n");
		return;
	}
	connector = dp->dp_display.connector;
	dev = dp_display->connector->dev;

	connector->status = connector->funcs->detect(connector, false);
	pr_debug("[%s] status updated to %s\n",
			      connector->name,
			      drm_get_connector_status_name(connector->status));
	snprintf(name, HPD_STRING_SIZE, "name=%s", connector->name);
	snprintf(status, HPD_STRING_SIZE, "status=%s",
		drm_get_connector_status_name(connector->status));
	snprintf(bpp, HPD_STRING_SIZE, "bpp=%d",
		dp_link_bit_depth_to_bpp(
		dp->link->test_video.test_bit_depth));
	snprintf(pattern, HPD_STRING_SIZE, "pattern=%d",
		dp->link->test_video.test_video_pattern);

	pr_debug("generating hotplug event [%s]:[%s] [%s] [%s]\n",
		name, status, bpp, pattern);
	envp[0] = name;
	envp[1] = status;
	envp[2] = bpp;
	envp[3] = pattern;
	envp[4] = NULL;
	kobject_uevent_env(&dev->primary->kdev->kobj, KOBJ_CHANGE,
			envp);
}

static int dp_display_send_hpd_notification(struct dp_display_private *dp,
		bool hpd)
{
	if ((hpd && dp->dp_display.is_connected) ||
			(!hpd && !dp->dp_display.is_connected)) {
		pr_info("HPD already %s\n", (hpd ? "on" : "off"));
		return 0;
	}

	/* reset video pattern flag on disconnect */
	if (!hpd)
		dp->panel->video_test = false;

	dp->dp_display.is_connected = hpd;
	reinit_completion(&dp->notification_comp);
	dp_display_send_hpd_event(&dp->dp_display);

	if (!wait_for_completion_timeout(&dp->notification_comp, HZ * 5)) {
		pr_warn("%s timeout\n", hpd ? "connect" : "disconnect");
		/* cancel any pending request */
		dp->ctrl->abort(dp->ctrl);
		return -EINVAL;
	}

	return 0;
}

static int dp_display_process_hpd_high(struct dp_display_private *dp)
{
	int rc = 0;
	u32 max_pclk_from_edid = 0;
	struct edid *edid;

	dp->aux->init(dp->aux, dp->parser->aux_cfg);

	if (dp->link->psm_enabled)
		goto notify;

	rc = dp->panel->read_sink_caps(dp->panel, dp->dp_display.connector);
	if (rc)
		goto notify;

	dp->link->process_request(dp->link);

	if (dp_display_is_sink_count_zero(dp)) {
		pr_debug("no downstream devices connected\n");
		rc = -EINVAL;
		goto end;
	}

	edid = dp->panel->edid_ctrl->edid;

	dp->audio_supported = drm_detect_monitor_audio(edid);

	dp->panel->handle_sink_request(dp->panel);

	max_pclk_from_edid = dp->panel->get_max_pclk(dp->panel);

	dp->dp_display.max_pclk_khz = min(max_pclk_from_edid,
		dp->parser->max_pclk_khz);

notify:
	dp_display_send_hpd_notification(dp, true);

end:
	return rc;
}

static void dp_display_host_init(struct dp_display_private *dp)
{
	bool flip = false;

	if (dp->core_initialized) {
		pr_debug("DP core already initialized\n");
		return;
	}

	if (dp->usbpd->orientation == ORIENTATION_CC2)
		flip = true;

	dp->power->init(dp->power, flip);
	dp->ctrl->init(dp->ctrl, flip);
	enable_irq(dp->irq);
	dp->core_initialized = true;
}

static void dp_display_host_deinit(struct dp_display_private *dp)
{
	if (!dp->core_initialized) {
		pr_debug("DP core already off\n");
		return;
	}

	dp->ctrl->deinit(dp->ctrl);
	dp->power->deinit(dp->power);
	disable_irq(dp->irq);
	dp->core_initialized = false;
}

static void dp_display_process_hpd_low(struct dp_display_private *dp)
{
	/* cancel any pending request */
	dp->ctrl->abort(dp->ctrl);

	if (dp_display_is_hdcp_enabled(dp) && dp->hdcp.ops->off) {
		cancel_delayed_work_sync(&dp->hdcp_cb_work);
		dp->hdcp.ops->off(dp->hdcp.data);
	}

	if (dp->audio_supported)
		dp->audio->off(dp->audio);

	dp_display_send_hpd_notification(dp, false);

	dp->aux->deinit(dp->aux);
}

static int dp_display_usbpd_configure_cb(struct device *dev)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!dev) {
		pr_err("invalid dev\n");
		rc = -EINVAL;
		goto end;
	}

	dp = dev_get_drvdata(dev);
	if (!dp) {
		pr_err("no driver data found\n");
		rc = -ENODEV;
		goto end;
	}

	dp_display_host_init(dp);

	if (dp->usbpd->hpd_high)
		dp_display_process_hpd_high(dp);
end:
	return rc;
}

static void dp_display_clean(struct dp_display_private *dp)
{
	if (dp_display_is_hdcp_enabled(dp)) {
		dp->hdcp_status = HDCP_STATE_INACTIVE;

		cancel_delayed_work_sync(&dp->hdcp_cb_work);
		if (dp->hdcp.ops->off)
			dp->hdcp.ops->off(dp->hdcp.data);
	}

	dp->ctrl->push_idle(dp->ctrl);
	dp->ctrl->off(dp->ctrl);
	dp->power_on = false;
}

static int dp_display_usbpd_disconnect_cb(struct device *dev)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!dev) {
		pr_err("invalid dev\n");
		rc = -EINVAL;
		goto end;
	}

	dp = dev_get_drvdata(dev);
	if (!dp) {
		pr_err("no driver data found\n");
		rc = -ENODEV;
		goto end;
	}

	/* cancel any pending request */
	dp->ctrl->abort(dp->ctrl);

	if (dp->audio_supported)
		dp->audio->off(dp->audio);

	rc = dp_display_send_hpd_notification(dp, false);

	mutex_lock(&dp->session_lock);

	/* if cable is disconnected, reset psm_enabled flag */
	if (!dp->usbpd->alt_mode_cfg_done)
		dp->link->psm_enabled = false;

	if ((rc < 0) && dp->power_on)
		dp_display_clean(dp);

	dp_display_host_deinit(dp);

	mutex_unlock(&dp->session_lock);
end:
	return rc;
}

static void dp_display_handle_video_request(struct dp_display_private *dp)
{
	if (dp->link->sink_request & DP_TEST_LINK_VIDEO_PATTERN) {
		/* force disconnect followed by connect */
		dp->usbpd->connect(dp->usbpd, false);
		dp->panel->video_test = true;
		dp->usbpd->connect(dp->usbpd, true);
		dp->link->send_test_response(dp->link);
	}
}

static int dp_display_handle_hpd_irq(struct dp_display_private *dp)
{
	if (dp->link->sink_request & DS_PORT_STATUS_CHANGED) {
		dp_display_send_hpd_notification(dp, false);

		if (dp_display_is_sink_count_zero(dp)) {
			pr_debug("sink count is zero, nothing to do\n");
			return 0;
		}

		return dp_display_process_hpd_high(dp);
	}

	dp->ctrl->handle_sink_request(dp->ctrl);

	dp_display_handle_video_request(dp);

	return 0;
}

static int dp_display_usbpd_attention_cb(struct device *dev)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!dev) {
		pr_err("invalid dev\n");
		return -EINVAL;
	}

	dp = dev_get_drvdata(dev);
	if (!dp) {
		pr_err("no driver data found\n");
		return -ENODEV;
	}

	if (dp->usbpd->hpd_irq) {
		dp->hpd_irq_on = true;

		if (dp_display_is_hdcp_enabled(dp) && dp->hdcp.ops->cp_irq) {
			if (!dp->hdcp.ops->cp_irq(dp->hdcp.data))
				goto end;
		}

		rc = dp->link->process_request(dp->link);
		/* check for any test request issued by sink */
		if (!rc)
			dp_display_handle_hpd_irq(dp);

		dp->hpd_irq_on = false;
		goto end;
	}

	if (!dp->usbpd->hpd_high) {
		dp_display_process_hpd_low(dp);
		goto end;
	}

	if (dp->usbpd->alt_mode_cfg_done)
		dp_display_process_hpd_high(dp);
end:
	return rc;
}

static void dp_display_deinit_sub_modules(struct dp_display_private *dp)
{
	dp_audio_put(dp->audio);
	dp_ctrl_put(dp->ctrl);
	dp_link_put(dp->link);
	dp_panel_put(dp->panel);
	dp_aux_put(dp->aux);
	dp_power_put(dp->power);
	dp_catalog_put(dp->catalog);
	dp_parser_put(dp->parser);
	dp_usbpd_put(dp->usbpd);
	mutex_destroy(&dp->session_lock);
	dp_debug_put(dp->debug);
}

static int dp_init_sub_modules(struct dp_display_private *dp)
{
	int rc = 0;
	struct device *dev = &dp->pdev->dev;
	struct dp_usbpd_cb *cb = &dp->usbpd_cb;
	struct dp_ctrl_in ctrl_in = {
		.dev = dev,
	};
	struct dp_panel_in panel_in = {
		.dev = dev,
	};

	cb->configure  = dp_display_usbpd_configure_cb;
	cb->disconnect = dp_display_usbpd_disconnect_cb;
	cb->attention  = dp_display_usbpd_attention_cb;

	dp->usbpd = dp_usbpd_get(dev, cb);
	if (IS_ERR(dp->usbpd)) {
		rc = PTR_ERR(dp->usbpd);
		pr_err("failed to initialize usbpd, rc = %d\n", rc);
		dp->usbpd = NULL;
		goto error;
	}

	mutex_init(&dp->session_lock);

	dp->parser = dp_parser_get(dp->pdev);
	if (IS_ERR(dp->parser)) {
		rc = PTR_ERR(dp->parser);
		pr_err("failed to initialize parser, rc = %d\n", rc);
		dp->parser = NULL;
		goto error_parser;
	}

	dp->catalog = dp_catalog_get(dev, &dp->parser->io);
	if (IS_ERR(dp->catalog)) {
		rc = PTR_ERR(dp->catalog);
		pr_err("failed to initialize catalog, rc = %d\n", rc);
		dp->catalog = NULL;
		goto error_catalog;
	}

	dp->power = dp_power_get(dp->parser);
	if (IS_ERR(dp->power)) {
		rc = PTR_ERR(dp->power);
		pr_err("failed to initialize power, rc = %d\n", rc);
		dp->power = NULL;
		goto error_power;
	}

	dp->aux = dp_aux_get(dev, &dp->catalog->aux, dp->parser->aux_cfg);
	if (IS_ERR(dp->aux)) {
		rc = PTR_ERR(dp->aux);
		pr_err("failed to initialize aux, rc = %d\n", rc);
		dp->aux = NULL;
		goto error_aux;
	}

	dp->link = dp_link_get(dev, dp->aux);
	if (IS_ERR(dp->link)) {
		rc = PTR_ERR(dp->link);
		pr_err("failed to initialize link, rc = %d\n", rc);
		dp->link = NULL;
		goto error_link;
	}

	panel_in.aux = dp->aux;
	panel_in.catalog = &dp->catalog->panel;
	panel_in.link = dp->link;

	dp->panel = dp_panel_get(&panel_in);
	if (IS_ERR(dp->panel)) {
		rc = PTR_ERR(dp->panel);
		pr_err("failed to initialize panel, rc = %d\n", rc);
		dp->panel = NULL;
		goto error_panel;
	}

	ctrl_in.link = dp->link;
	ctrl_in.panel = dp->panel;
	ctrl_in.aux = dp->aux;
	ctrl_in.power = dp->power;
	ctrl_in.catalog = &dp->catalog->ctrl;
	ctrl_in.parser = dp->parser;

	dp->ctrl = dp_ctrl_get(&ctrl_in);
	if (IS_ERR(dp->ctrl)) {
		rc = PTR_ERR(dp->ctrl);
		pr_err("failed to initialize ctrl, rc = %d\n", rc);
		dp->ctrl = NULL;
		goto error_ctrl;
	}

	dp->audio = dp_audio_get(dp->pdev, dp->panel, &dp->catalog->audio);
	if (IS_ERR(dp->audio)) {
		rc = PTR_ERR(dp->audio);
		pr_err("failed to initialize audio, rc = %d\n", rc);
		dp->audio = NULL;
		goto error_audio;
	}

	dp->debug = dp_debug_get(dev, dp->panel, dp->usbpd,
				dp->link, &dp->dp_display.connector);
	if (IS_ERR(dp->debug)) {
		rc = PTR_ERR(dp->debug);
		pr_err("failed to initialize debug, rc = %d\n", rc);
		dp->debug = NULL;
		goto error_debug;
	}

	return rc;
error_debug:
	dp_audio_put(dp->audio);
error_audio:
	dp_ctrl_put(dp->ctrl);
error_ctrl:
	dp_panel_put(dp->panel);
error_panel:
	dp_link_put(dp->link);
error_link:
	dp_aux_put(dp->aux);
error_aux:
	dp_power_put(dp->power);
error_power:
	dp_catalog_put(dp->catalog);
error_catalog:
	dp_parser_put(dp->parser);
error_parser:
	dp_usbpd_put(dp->usbpd);
	mutex_destroy(&dp->session_lock);
error:
	return rc;
}

static int dp_display_set_mode(struct dp_display *dp_display,
		struct dp_display_mode *mode)
{
	struct dp_display_private *dp;

	if (!dp_display) {
		pr_err("invalid input\n");
		return -EINVAL;
	}
	dp = container_of(dp_display, struct dp_display_private, dp_display);

	mutex_lock(&dp->session_lock);
	dp->panel->pinfo = mode->timing;
	dp->panel->init_info(dp->panel);
	mutex_unlock(&dp->session_lock);

	return 0;
}

static int dp_display_prepare(struct dp_display *dp)
{
	return 0;
}

static int dp_display_enable(struct dp_display *dp_display)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!dp_display) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	mutex_lock(&dp->session_lock);

	if (dp->power_on) {
		pr_debug("Link already setup, return\n");
		goto end;
	}

	rc = dp->ctrl->on(dp->ctrl);
	if (!rc)
		dp->power_on = true;
end:
	mutex_unlock(&dp->session_lock);
	return rc;
}

static int dp_display_post_enable(struct dp_display *dp_display)
{
	struct dp_display_private *dp;

	if (!dp_display) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	mutex_lock(&dp->session_lock);

	if (!dp->power_on) {
		pr_debug("Link not setup, return\n");
		goto end;
	}

	if (dp->audio_supported) {
		dp->audio->bw_code = dp->link->link_params.bw_code;
		dp->audio->lane_count = dp->link->link_params.lane_count;
		dp->audio->on(dp->audio);
	}

	complete_all(&dp->notification_comp);

	dp_display_update_hdcp_info(dp);

	if (dp_display_is_hdcp_enabled(dp)) {
		cancel_delayed_work_sync(&dp->hdcp_cb_work);

		dp->hdcp_status = HDCP_STATE_AUTHENTICATING;
		queue_delayed_work(dp->hdcp_workqueue,
				&dp->hdcp_cb_work, HZ / 2);
	}

end:
	mutex_unlock(&dp->session_lock);
	return 0;
}

static int dp_display_pre_disable(struct dp_display *dp_display)
{
	struct dp_display_private *dp;

	if (!dp_display) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	mutex_lock(&dp->session_lock);

	if (!dp->power_on) {
		pr_debug("Link already powered off, return\n");
		goto end;
	}

	if (dp_display_is_hdcp_enabled(dp)) {
		dp->hdcp_status = HDCP_STATE_INACTIVE;

		cancel_delayed_work_sync(&dp->hdcp_cb_work);
		if (dp->hdcp.ops->off)
			dp->hdcp.ops->off(dp->hdcp.data);
	}

	if (dp->usbpd->alt_mode_cfg_done && (dp->usbpd->hpd_high ||
		dp->usbpd->forced_disconnect))
		dp->link->psm_config(dp->link, &dp->panel->link_info, true);

	dp->ctrl->push_idle(dp->ctrl);

end:
	mutex_unlock(&dp->session_lock);
	return 0;
}

static int dp_display_disable(struct dp_display *dp_display)
{
	struct dp_display_private *dp;

	if (!dp_display) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	mutex_lock(&dp->session_lock);

	if (!dp->power_on || !dp->core_initialized) {
		pr_debug("Link already powered off, return\n");
		goto end;
	}

	dp->ctrl->off(dp->ctrl);

	dp->power_on = false;

end:
	complete_all(&dp->notification_comp);
	mutex_unlock(&dp->session_lock);
	return 0;
}

static int dp_request_irq(struct dp_display *dp_display)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!dp_display) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	dp->irq = irq_of_parse_and_map(dp->pdev->dev.of_node, 0);
	if (dp->irq < 0) {
		rc = dp->irq;
		pr_err("failed to get irq: %d\n", rc);
		return rc;
	}

	rc = devm_request_irq(&dp->pdev->dev, dp->irq, dp_display_irq,
		IRQF_TRIGGER_HIGH, "dp_display_isr", dp);
	if (rc < 0) {
		pr_err("failed to request IRQ%u: %d\n",
				dp->irq, rc);
		return rc;
	}
	disable_irq(dp->irq);

	return 0;
}

static struct dp_debug *dp_get_debug(struct dp_display *dp_display)
{
	struct dp_display_private *dp;

	if (!dp_display) {
		pr_err("invalid input\n");
		return ERR_PTR(-EINVAL);
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	return dp->debug;
}

static int dp_display_unprepare(struct dp_display *dp)
{
	return 0;
}

static int dp_display_validate_mode(struct dp_display *dp,
	struct dp_display_mode *mode)
{
	return 0;
}

static int dp_display_get_modes(struct dp_display *dp,
	struct dp_display_mode *dp_mode)
{
	struct dp_display_private *dp_display;
	int ret = 0;

	if (!dp) {
		pr_err("invalid params\n");
		return 0;
	}

	dp_display = container_of(dp, struct dp_display_private, dp_display);

	ret = dp_display->panel->get_modes(dp_display->panel,
		dp->connector, dp_mode);
	if (dp_mode->timing.pixel_clk_khz)
		dp->max_pclk_khz = dp_mode->timing.pixel_clk_khz;
	return ret;
}

static bool dp_display_check_video_test(struct dp_display *dp)
{
	struct dp_display_private *dp_display;

	if (!dp) {
		pr_err("invalid params\n");
		return false;
	}

	dp_display = container_of(dp, struct dp_display_private, dp_display);

	if (dp_display->panel->video_test)
		return true;

	return false;
}

static int dp_display_get_test_bpp(struct dp_display *dp)
{
	struct dp_display_private *dp_display;

	if (!dp) {
		pr_err("invalid params\n");
		return 0;
	}

	dp_display = container_of(dp, struct dp_display_private, dp_display);

	return dp_link_bit_depth_to_bpp(
		dp_display->link->test_video.test_bit_depth);
}

static int dp_display_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!pdev || !pdev->dev.of_node) {
		pr_err("pdev not found\n");
		return -ENODEV;
	}

	dp = devm_kzalloc(&pdev->dev, sizeof(*dp), GFP_KERNEL);
	if (!dp)
		return -ENOMEM;

	init_completion(&dp->notification_comp);

	dp->pdev = pdev;
	dp->name = "drm_dp";

	rc = dp_init_sub_modules(dp);
	if (rc) {
		devm_kfree(&pdev->dev, dp);
		return -EPROBE_DEFER;
	}

	platform_set_drvdata(pdev, dp);

	g_dp_display = &dp->dp_display;

	g_dp_display->enable        = dp_display_enable;
	g_dp_display->post_enable   = dp_display_post_enable;
	g_dp_display->pre_disable   = dp_display_pre_disable;
	g_dp_display->disable       = dp_display_disable;
	g_dp_display->set_mode      = dp_display_set_mode;
	g_dp_display->validate_mode = dp_display_validate_mode;
	g_dp_display->get_modes     = dp_display_get_modes;
	g_dp_display->prepare       = dp_display_prepare;
	g_dp_display->unprepare     = dp_display_unprepare;
	g_dp_display->request_irq   = dp_request_irq;
	g_dp_display->get_debug     = dp_get_debug;
	g_dp_display->send_hpd_event    = dp_display_send_hpd_event;
	g_dp_display->is_video_test = dp_display_check_video_test;
	g_dp_display->get_test_bpp = dp_display_get_test_bpp;

	rc = component_add(&pdev->dev, &dp_display_comp_ops);
	if (rc) {
		pr_err("component add failed, rc=%d\n", rc);
		dp_display_deinit_sub_modules(dp);
		devm_kfree(&pdev->dev, dp);
	}

	return rc;
}

int dp_display_get_displays(void **displays, int count)
{
	if (!displays) {
		pr_err("invalid data\n");
		return -EINVAL;
	}

	if (count != 1) {
		pr_err("invalid number of displays\n");
		return -EINVAL;
	}

	displays[0] = g_dp_display;
	return count;
}

int dp_display_get_num_of_displays(void)
{
	return 1;
}

static int dp_display_remove(struct platform_device *pdev)
{
	struct dp_display_private *dp;

	if (!pdev)
		return -EINVAL;

	dp = platform_get_drvdata(pdev);

	dp_display_deinit_sub_modules(dp);

	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, dp);

	return 0;
}

static struct platform_driver dp_display_driver = {
	.probe  = dp_display_probe,
	.remove = dp_display_remove,
	.driver = {
		.name = "msm-dp-display",
		.of_match_table = dp_dt_match,
	},
};

static int __init dp_display_init(void)
{
	int ret;

	ret = platform_driver_register(&dp_display_driver);
	if (ret) {
		pr_err("driver register failed");
		return ret;
	}

	return ret;
}
module_init(dp_display_init);

static void __exit dp_display_cleanup(void)
{
	platform_driver_unregister(&dp_display_driver);
}
module_exit(dp_display_cleanup);

