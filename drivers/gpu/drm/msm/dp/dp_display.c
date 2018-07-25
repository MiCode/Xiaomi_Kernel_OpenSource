/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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
#include <linux/soc/qcom/fsa4480-i2c.h>

#include "sde_connector.h"

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

#define DP_MST_DEBUG(fmt, ...) pr_debug(fmt, ##__VA_ARGS__)

static struct dp_display *g_dp_display;
#define HPD_STRING_SIZE 30

struct dp_hdcp {
	void *data;
	struct sde_hdcp_ops *ops;

	void *hdcp1;
	void *hdcp2;

	bool feature_enabled;
};

struct dp_mst {
	bool mst_active;

	bool drm_registered;
	struct dp_mst_drm_cbs cbs;
};

struct dp_display_private {
	char *name;
	int irq;

	/* state variables */
	bool core_initialized;
	bool power_on;

	atomic_t aborted;

	struct platform_device *pdev;
	struct device_node *aux_switch_node;
	struct dentry *root;
	struct completion notification_comp;
	struct completion disconnect_comp;

	struct dp_usbpd   *usbpd;
	struct dp_parser  *parser;
	struct dp_power   *power;
	struct dp_catalog *catalog;
	struct dp_aux     *aux;
	struct dp_link    *link;
	struct dp_panel   *panel;
	struct dp_ctrl    *ctrl;
	struct dp_debug   *debug;

	struct dp_panel *active_panels[DP_STREAM_MAX];
	struct dp_hdcp hdcp;

	struct dp_usbpd_cb usbpd_cb;
	struct dp_display_mode mode;
	struct dp_display dp_display;
	struct msm_drm_private *priv;

	struct workqueue_struct *wq;
	struct delayed_work hdcp_cb_work;
	struct delayed_work connect_work;
	struct work_struct attention_work;
	struct mutex hdcp_mutex;
	struct mutex session_lock;

	u32 active_stream_cnt;
	struct dp_mst mst;
};

static const struct of_device_id dp_dt_match[] = {
	{.compatible = "qcom,dp-display"},
	{}
};

static bool dp_display_framework_ready(struct dp_display_private *dp)
{
	return dp->dp_display.post_open ? false : true;
}

static inline bool dp_display_is_hdcp_enabled(struct dp_display_private *dp)
{
	return dp->hdcp.feature_enabled && dp->link->hdcp_status.hdcp_version
		&& dp->hdcp.ops;
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

	pr_debug("%s: %s\n",
		sde_hdcp_version(dp->link->hdcp_status.hdcp_version),
		sde_hdcp_state_name(dp->link->hdcp_status.hdcp_state));

	switch (dp->link->hdcp_status.hdcp_state) {
	case HDCP_STATE_AUTHENTICATING:
		if (dp->hdcp.ops && dp->hdcp.ops->authenticate)
			rc = dp->hdcp.ops->authenticate(dp->hdcp.data);
		break;
	case HDCP_STATE_AUTH_FAIL:
		if (dp->power_on) {
			if (ops && ops->reauthenticate) {
				rc = ops->reauthenticate(dp->hdcp.data);
				if (rc)
					pr_err("failed rc=%d\n", rc);
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
		enum sde_hdcp_state state)
{
	struct dp_display_private *dp = ptr;

	if (!dp) {
		pr_err("invalid input\n");
		return;
	}

	dp->link->hdcp_status.hdcp_state = state;

	if (dp->dp_display.is_connected)
		queue_delayed_work(dp->wq, &dp->hdcp_cb_work, HZ/4);
}

static void dp_display_update_hdcp_info(struct dp_display_private *dp)
{
	void *fd = NULL;
	struct sde_hdcp_ops *ops = NULL;
	bool hdcp2_present = false, hdcp1_present = false;

	if (!dp) {
		pr_err("invalid input\n");
		return;
	}

	dp->link->hdcp_status.hdcp_state = HDCP_STATE_INACTIVE;
	dp->link->hdcp_status.hdcp_version = HDCP_VERSION_NONE;

	if (!dp->hdcp.feature_enabled) {
		pr_debug("feature not enabled\n");
		return;
	}

	if (dp->debug->sim_mode) {
		pr_debug("skip HDCP version checks for simulation mode\n");
		return;
	}

	fd = dp->hdcp.hdcp2;
	if (fd)
		ops = sde_dp_hdcp2p2_start(fd);

	if (ops && ops->feature_supported)
		hdcp2_present = ops->feature_supported(fd);

	if (hdcp2_present)
		dp->link->hdcp_status.hdcp_version = HDCP_VERSION_2P2;

	if (!hdcp2_present) {
		fd = dp->hdcp.hdcp1;
		if (fd)
			ops = sde_hdcp_1x_start(fd);

		if (ops && ops->feature_supported)
			hdcp1_present = ops->feature_supported(fd);

		if (hdcp1_present)
			dp->link->hdcp_status.hdcp_version = HDCP_VERSION_1X;
	}

	if (hdcp2_present || hdcp1_present) {
		dp->hdcp.data = fd;
		dp->hdcp.ops = ops;
	} else {
		dp->hdcp.data = NULL;
		dp->hdcp.ops = NULL;
	}

	pr_debug("HDCP version supported: %s\n",
		sde_hdcp_version(dp->link->hdcp_status.hdcp_version));
}

static void dp_display_deinitialize_hdcp(struct dp_display_private *dp)
{
	if (!dp) {
		pr_err("invalid input\n");
		return;
	}

	sde_dp_hdcp2p2_deinit(dp->hdcp.data);
	mutex_destroy(&dp->hdcp_mutex);
}

static int dp_display_initialize_hdcp(struct dp_display_private *dp)
{
	struct sde_hdcp_init_data hdcp_init_data;
	struct dp_parser *parser;
	int rc = 0;

	if (!dp) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	parser = dp->parser;

	mutex_init(&dp->hdcp_mutex);

	hdcp_init_data.client_id     = HDCP_CLIENT_DP;
	hdcp_init_data.drm_aux       = dp->aux->drm_aux;
	hdcp_init_data.cb_data       = (void *)dp;
	hdcp_init_data.workq         = dp->wq;
	hdcp_init_data.sec_access    = true;
	hdcp_init_data.notify_status = dp_display_notify_hdcp_status_cb;
	hdcp_init_data.dp_ahb        = &parser->get_io(parser, "dp_ahb")->io;
	hdcp_init_data.dp_aux        = &parser->get_io(parser, "dp_aux")->io;
	hdcp_init_data.dp_link       = &parser->get_io(parser, "dp_link")->io;
	hdcp_init_data.dp_p0         = &parser->get_io(parser, "dp_p0")->io;
	hdcp_init_data.qfprom_io     = &parser->get_io(parser,
						"qfprom_physical")->io;
	hdcp_init_data.hdcp_io       = &parser->get_io(parser,
						"hdcp_physical")->io;
	hdcp_init_data.revision      = &dp->panel->link_info.revision;
	hdcp_init_data.msm_hdcp_dev  = dp->parser->msm_hdcp_dev;

	dp->hdcp.hdcp1 = sde_hdcp_1x_init(&hdcp_init_data);
	if (IS_ERR_OR_NULL(dp->hdcp.hdcp1)) {
		pr_err("Error initializing HDCP 1.x\n");
		rc = -EINVAL;
		goto error;
	}

	pr_debug("HDCP 1.3 initialized\n");

	dp->hdcp.hdcp2 = sde_dp_hdcp2p2_init(&hdcp_init_data);
	if (IS_ERR_OR_NULL(dp->hdcp.hdcp2)) {
		pr_err("Error initializing HDCP 2.x\n");
		rc = -EINVAL;
		goto error;
	}

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
	dp->priv = drm->dev_private;
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

	if (dp->power)
		(void)dp->power->power_client_deinit(dp->power);
	if (dp->aux)
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

static void dp_display_send_hpd_event(struct dp_display_private *dp)
{
	struct drm_device *dev = NULL;
	struct drm_connector *connector;
	char name[HPD_STRING_SIZE], status[HPD_STRING_SIZE],
		bpp[HPD_STRING_SIZE], pattern[HPD_STRING_SIZE];
	char *envp[5];

	if (dp->mst.mst_active) {
		pr_debug("skip notification for mst mode\n");
		return;
	}

	connector = dp->dp_display.base_connector;

	if (!connector) {
		pr_err("connector not set\n");
		return;
	}

	connector->status = connector->funcs->detect(connector, false);

	dev = connector->dev;

	snprintf(name, HPD_STRING_SIZE, "name=%s", connector->name);
	snprintf(status, HPD_STRING_SIZE, "status=%s",
		drm_get_connector_status_name(connector->status));
	snprintf(bpp, HPD_STRING_SIZE, "bpp=%d",
		dp_link_bit_depth_to_bpp(
		dp->link->test_video.test_bit_depth));
	snprintf(pattern, HPD_STRING_SIZE, "pattern=%d",
		dp->link->test_video.test_video_pattern);

	pr_debug("[%s]:[%s] [%s] [%s]\n", name, status, bpp, pattern);
	envp[0] = name;
	envp[1] = status;
	envp[2] = bpp;
	envp[3] = pattern;
	envp[4] = NULL;
	kobject_uevent_env(&dev->primary->kdev->kobj, KOBJ_CHANGE,
			envp);
}

static void dp_display_post_open(struct dp_display *dp_display)
{
	struct drm_connector *connector;
	struct dp_display_private *dp;

	if (!dp_display) {
		pr_err("invalid input\n");
		return;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);
	if (IS_ERR_OR_NULL(dp)) {
		pr_err("invalid params\n");
		return;
	}

	connector = dp->dp_display.base_connector;

	if (!connector) {
		pr_err("base connector not set\n");
		return;
	}

	/* if cable is already connected, send notification */
	if (dp->usbpd->hpd_high)
		queue_delayed_work(dp->wq, &dp->connect_work, HZ * 10);
	else
		dp_display->post_open = NULL;
}

static int dp_display_send_hpd_notification(struct dp_display_private *dp,
		bool hpd)
{
	u32 timeout_sec;
	int ret = 0;

	dp->dp_display.is_connected = hpd;

	if  (dp_display_framework_ready(dp))
		timeout_sec = 5;
	else
		timeout_sec = 10;

	dp->aux->state |= DP_STATE_NOTIFICATION_SENT;

	if (!dp->mst.mst_active)
		dp->dp_display.is_sst_connected = hpd;

	reinit_completion(&dp->notification_comp);
	dp_display_send_hpd_event(dp);

	if (hpd && dp->mst.mst_active)
		goto skip_wait;

	if (!wait_for_completion_timeout(&dp->notification_comp,
						HZ * timeout_sec)) {
		pr_warn("%s timeout\n", hpd ? "connect" : "disconnect");
		ret = -EINVAL;
	}

skip_wait:
	dp->aux->state &= ~DP_STATE_NOTIFICATION_SENT;

	return ret;
}

static void dp_display_process_mst_hpd_high(struct dp_display_private *dp)
{
	bool is_mst_receiver;
	struct dp_mst_hdp_info info;

	if (dp->parser->has_mst && dp->mst.drm_registered) {
		DP_MST_DEBUG("mst_hpd_high work\n");

		is_mst_receiver = dp->panel->read_mst_cap(dp->panel);

		if (is_mst_receiver && !dp->mst.mst_active) {
			dp->mst.mst_active = true;

			info.mst_protocol = dp->parser->has_mst_sideband;
			info.edid = dp->debug->get_edid(dp->debug);

			if (dp->mst.cbs.hpd)
				dp->mst.cbs.hpd(&dp->dp_display, true, &info);
		}
	}

	DP_MST_DEBUG("mst_hpd_high. mst_active:%d\n", dp->mst.mst_active);
}

static void dp_display_host_init(struct dp_display_private *dp)
{
	bool flip = false;
	bool reset;

	if (dp->core_initialized) {
		pr_debug("DP core already initialized\n");
		return;
	}

	if (dp->usbpd->orientation == ORIENTATION_CC2)
		flip = true;

	reset = dp->debug->sim_mode ? false : !dp->usbpd->multi_func;

	dp->power->init(dp->power, flip);
	dp->ctrl->init(dp->ctrl, flip, reset);
	enable_irq(dp->irq);
	dp->core_initialized = true;
}

static int dp_display_process_hpd_high(struct dp_display_private *dp)
{
	int rc = 0;

	if (!dp->debug->sim_mode) {
		rc = dp->aux->aux_switch(dp->aux, true, dp->usbpd->orientation);
		if (rc)
			goto end;
	}

	dp_display_host_init(dp);

	dp->aux->init(dp->aux, dp->parser->aux_cfg);

	if (dp->debug->psm_enabled) {
		dp->link->psm_config(dp->link, &dp->panel->link_info, false);
		dp->debug->psm_enabled = false;
	}

	if (!dp->dp_display.base_connector)
		return 0;

	rc = dp->panel->read_sink_caps(dp->panel,
			dp->dp_display.base_connector, dp->usbpd->multi_func);
	if (rc) {
		/*
		 * ETIMEDOUT --> cable may have been removed
		 * ENOTCONN --> no downstream device connected
		 */
		if (rc == -ETIMEDOUT || rc == -ENOTCONN)
			goto end;
		else
			goto notify;
	}

	dp->link->process_request(dp->link);
	dp->panel->handle_sink_request(dp->panel);

	if (dp->debug->max_pclk_khz)
		dp->dp_display.max_pclk_khz = dp->debug->max_pclk_khz;
	else
		dp->dp_display.max_pclk_khz = dp->parser->max_pclk_khz;

	pr_debug("dp max_pclk_khz = %d\n", dp->dp_display.max_pclk_khz);

	dp_display_process_mst_hpd_high(dp);
notify:
	dp_display_send_hpd_notification(dp, true);

end:
	return rc;
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
	dp->aux->state = 0;
}

static void dp_display_process_mst_hpd_low(struct dp_display_private *dp)
{
	struct dp_mst_hdp_info info = {0};

	if (dp->mst.mst_active) {
		DP_MST_DEBUG("mst_hpd_low work\n");

		if (dp->mst.cbs.hpd) {
			info.mst_protocol = dp->parser->has_mst_sideband;
			dp->mst.cbs.hpd(&dp->dp_display, false, &info);
		}

		dp->mst.mst_active = false;
	}

	DP_MST_DEBUG("mst_hpd_low. mst_active:%d\n", dp->mst.mst_active);
}

static int dp_display_process_hpd_low(struct dp_display_private *dp)
{
	int rc = 0, idx;
	struct dp_panel *dp_panel;

	mutex_lock(&dp->session_lock);

	if (!dp->dp_display.is_connected) {
		pr_debug("HPD already off\n");
		mutex_unlock(&dp->session_lock);
		return 0;
	}

	if (dp_display_is_hdcp_enabled(dp) && dp->hdcp.ops->off)
		dp->hdcp.ops->off(dp->hdcp.data);

	for (idx = DP_STREAM_0; idx < DP_STREAM_MAX; idx++) {
		if (!dp->active_panels[idx])
			continue;

		dp_panel = dp->active_panels[idx];

		if (dp_panel->audio_supported) {
			dp_panel->audio->off(dp_panel->audio);
			dp_panel->audio_supported = false;
		}
	}

	mutex_unlock(&dp->session_lock);

	dp_display_process_mst_hpd_low(dp);

	rc = dp_display_send_hpd_notification(dp, false);

	dp->panel->video_test = false;

	return rc;
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

	/* check for hpd high and framework ready */
	if  (dp->usbpd->hpd_high && dp_display_framework_ready(dp))
		queue_delayed_work(dp->wq, &dp->connect_work, 0);
end:
	return rc;
}

static void dp_display_clean(struct dp_display_private *dp)
{
	if (dp_display_is_hdcp_enabled(dp)) {
		dp->link->hdcp_status.hdcp_state = HDCP_STATE_INACTIVE;

		cancel_delayed_work_sync(&dp->hdcp_cb_work);
		if (dp->hdcp.ops->off)
			dp->hdcp.ops->off(dp->hdcp.data);
	}

	dp->ctrl->push_idle(dp->ctrl, DP_STREAM_0);
	dp->ctrl->off(dp->ctrl);
	dp->panel->deinit(dp->panel);
	dp->aux->deinit(dp->aux);
	dp->power_on = false;
}

static int dp_display_handle_disconnect(struct dp_display_private *dp)
{
	int rc;

	rc = dp_display_process_hpd_low(dp);
	if (rc) {
		/* cancel any pending request */
		dp->ctrl->abort(dp->ctrl);
		dp->aux->abort(dp->aux);
	}

	mutex_lock(&dp->session_lock);
	if (rc && dp->power_on)
		dp_display_clean(dp);

	/*
	 * De-initialize the display core only if the display controller has
	 * been turned off either through the DRM bridge disable call or
	 * through the error handling code. This ensures that the power
	 * resource vote is still present in cases when the bridge disable is
	 * delayed.
	 */
	if (!dp->power_on && !dp->usbpd->alt_mode_cfg_done)
		dp_display_host_deinit(dp);

	complete_all(&dp->disconnect_comp);
	mutex_unlock(&dp->session_lock);

	return rc;
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

	/*
	 * In case cable/dongle is disconnected during adb shell stop,
	 * reset psm_enabled flag to false since it is no more needed
	 */
	if (dp->dp_display.post_open)
		dp->debug->psm_enabled = false;

	if (dp->debug->psm_enabled)
		dp->link->psm_config(dp->link, &dp->panel->link_info, true);

	/* cancel any pending request */
	atomic_set(&dp->aborted, 1);
	dp->ctrl->abort(dp->ctrl);
	dp->aux->abort(dp->aux);

	/* wait for idle state */
	cancel_delayed_work(&dp->connect_work);
	cancel_work(&dp->attention_work);
	flush_workqueue(dp->wq);

	dp_display_handle_disconnect(dp);

	if (!dp->debug->sim_mode)
		dp->aux->aux_switch(dp->aux, false, ORIENTATION_NONE);

	/* Reset abort value to allow future connections */
	atomic_set(&dp->aborted, 0);
end:
	return rc;
}

static void dp_display_handle_maintenance_req(struct dp_display_private *dp)
{
	int idx;
	struct dp_panel *dp_panel;

	mutex_lock(&dp->session_lock);

	for (idx = DP_STREAM_0; idx < DP_STREAM_MAX; idx++) {
		if (!dp->active_panels[idx])
			continue;

		dp_panel = dp->active_panels[idx];

		dp->ctrl->stream_pre_off(dp->ctrl, dp_panel);
		dp->ctrl->stream_off(dp->ctrl, dp_panel);

		mutex_lock(&dp_panel->audio->ops_lock);

		if (dp_panel->audio_supported)
			dp_panel->audio->off(dp_panel->audio);
	}

	dp->ctrl->link_maintenance(dp->ctrl);

	for (idx = DP_STREAM_0; idx < DP_STREAM_MAX; idx++) {
		if (!dp->active_panels[idx])
			continue;

		dp_panel = dp->active_panels[idx];

		dp->ctrl->stream_on(dp->ctrl, dp_panel);

		if (dp_panel->audio_supported)
			dp_panel->audio->on(dp_panel->audio);

		mutex_unlock(&dp_panel->audio->ops_lock);
	}

	mutex_unlock(&dp->session_lock);
}

static void dp_display_mst_attention(struct dp_display_private *dp)
{
	if (dp->mst.mst_active && dp->mst.cbs.hpd_irq)
		dp->mst.cbs.hpd_irq(&dp->dp_display);

	DP_MST_DEBUG("mst_attention_work. mst_active:%d\n", dp->mst.mst_active);
}

static void dp_display_attention_work(struct work_struct *work)
{
	struct dp_display_private *dp = container_of(work,
			struct dp_display_private, attention_work);

	if (!dp->power_on)
		goto mst_attention;

	if (dp->link->sink_request & DS_PORT_STATUS_CHANGED) {
		dp_display_handle_disconnect(dp);

		if (dp_display_is_sink_count_zero(dp)) {
			pr_debug("sink count is zero, nothing to do\n");
			goto mst_attention;
		}

		queue_delayed_work(dp->wq, &dp->connect_work, 0);
		goto mst_attention;
	}

	if (dp->link->sink_request & DP_TEST_LINK_VIDEO_PATTERN) {
		dp_display_handle_disconnect(dp);

		dp->panel->video_test = true;
		dp_display_send_hpd_notification(dp, true);
		dp->link->send_test_response(dp->link);

		goto mst_attention;
	}

	if (dp->link->sink_request & DP_TEST_LINK_PHY_TEST_PATTERN) {
		dp->ctrl->process_phy_test_request(dp->ctrl);
		goto mst_attention;
	}

	if (dp->link->sink_request & DP_TEST_LINK_TRAINING) {
		dp->link->send_test_response(dp->link);
		dp_display_handle_maintenance_req(dp);
		goto mst_attention;
	}

	if (dp->link->sink_request & DP_LINK_STATUS_UPDATED)
		dp_display_handle_maintenance_req(dp);

	if (dp_display_is_hdcp_enabled(dp) && dp->hdcp.ops->cp_irq)
		dp->hdcp.ops->cp_irq(dp->hdcp.data);
mst_attention:
	dp_display_mst_attention(dp);
}

static int dp_display_usbpd_attention_cb(struct device *dev)
{
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

	/* check if framework is ready */
	if (!dp_display_framework_ready(dp)) {
		pr_err("framework not ready\n");
		return -ENODEV;
	}

	DP_MST_DEBUG("mst: hpd_irq:%d, hpd_high:%d, power_on:%d\n",
			dp->usbpd->hpd_irq, dp->usbpd->hpd_high,
			dp->power_on);

	if (dp->usbpd->hpd_irq && dp->usbpd->hpd_high &&
	    dp->power_on) {
		dp->link->process_request(dp->link);
		queue_work(dp->wq, &dp->attention_work);
	} else if (dp->usbpd->hpd_high && dp->usbpd->hpd_irq) {
		queue_delayed_work(dp->wq, &dp->connect_work, 0);
		queue_work(dp->wq, &dp->attention_work);
	} else if (dp->usbpd->hpd_high) {
		queue_delayed_work(dp->wq, &dp->connect_work, 0);
	} else if (dp->usbpd->hpd_irq) {
		queue_work(dp->wq, &dp->attention_work);
	} else {
		/* cancel any pending request */
		atomic_set(&dp->aborted, 1);
		dp->ctrl->abort(dp->ctrl);
		dp->aux->abort(dp->aux);

		/* wait for idle state */
		cancel_delayed_work(&dp->connect_work);
		cancel_work(&dp->attention_work);
		flush_workqueue(dp->wq);

		dp_display_handle_disconnect(dp);
		atomic_set(&dp->aborted, 0);
	}

	return 0;
}

static void dp_display_connect_work(struct work_struct *work)
{
	struct delayed_work *dw = to_delayed_work(work);
	struct dp_display_private *dp = container_of(dw,
			struct dp_display_private, connect_work);
	u32 const disconnect_timeout_sec = 20;


	if (dp->dp_display.is_connected && dp_display_framework_ready(dp)) {
		pr_debug("HPD already on\n");
		return;
	}

	if (atomic_read(&dp->aborted)) {
		/*
		 * If we receive a connection event while processing a
		 * disconnect event from the previous session then we have to
		 * wait until that disconnect event completes or expires. We
		 * give the highest priority to handling disconnect events
		 * since they represent a state change triggered by a physical
		 * removal of the cable/plug.
		 */
		pr_warn("disconnect pending, waiting for %d sec\n",
				disconnect_timeout_sec);
		reinit_completion(&dp->disconnect_comp);
		if (!wait_for_completion_timeout(&dp->disconnect_comp,
					HZ * disconnect_timeout_sec)) {
			pr_warn("disconnect completion timeout\n");
			return;
		}
	}

	if (atomic_read(&dp->aborted)) {
		pr_warn("HPD off requested\n");
		return;
	}

	if (!dp->usbpd->hpd_high) {
		pr_warn("Sink disconnected\n");
		return;
	}

	dp_display_process_hpd_high(dp);
}

static void dp_display_deinit_sub_modules(struct dp_display_private *dp)
{
	dp_audio_put(dp->panel->audio);
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

	mutex_init(&dp->session_lock);

	dp->parser = dp_parser_get(dp->pdev);
	if (IS_ERR(dp->parser)) {
		rc = PTR_ERR(dp->parser);
		pr_err("failed to initialize parser, rc = %d\n", rc);
		dp->parser = NULL;
		goto error;
	}

	rc = dp->parser->parse(dp->parser);
	if (rc) {
		pr_err("device tree parsing failed\n");
		goto error_catalog;
	}

	dp->catalog = dp_catalog_get(dev, dp->parser);
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

	rc = dp->power->power_client_init(dp->power, &dp->priv->phandle);
	if (rc) {
		pr_err("Power client create failed\n");
		goto error_aux;
	}

	dp->aux = dp_aux_get(dev, &dp->catalog->aux, dp->parser->aux_cfg,
			dp->aux_switch_node);
	if (IS_ERR(dp->aux)) {
		rc = PTR_ERR(dp->aux);
		pr_err("failed to initialize aux, rc = %d\n", rc);
		dp->aux = NULL;
		goto error_aux;
	}

	rc = dp->aux->drm_aux_register(dp->aux);
	if (rc) {
		pr_err("DRM DP AUX register failed\n");
		goto error_link;
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
	panel_in.connector = dp->dp_display.base_connector;
	panel_in.base_panel = NULL;

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

	dp->panel->audio = dp_audio_get(dp->pdev, dp->panel,
						&dp->catalog->audio);
	if (IS_ERR(dp->panel->audio)) {
		rc = PTR_ERR(dp->panel->audio);
		pr_err("failed to initialize audio, rc = %d\n", rc);
		dp->panel->audio = NULL;
		goto error_audio;
	}

	memset(&dp->mst, 0, sizeof(dp->mst));
	dp->active_stream_cnt = 0;

	cb->configure  = dp_display_usbpd_configure_cb;
	cb->disconnect = dp_display_usbpd_disconnect_cb;
	cb->attention  = dp_display_usbpd_attention_cb;

	dp->usbpd = dp_usbpd_get(dev, cb);
	if (IS_ERR(dp->usbpd)) {
		rc = PTR_ERR(dp->usbpd);
		pr_err("failed to initialize usbpd, rc = %d\n", rc);
		dp->usbpd = NULL;
		goto error_usbpd;
	}

	dp->debug = dp_debug_get(dev, dp->panel, dp->usbpd,
				dp->link, dp->aux,
				&dp->dp_display.base_connector,
				dp->catalog, dp->parser);

	if (IS_ERR(dp->debug)) {
		rc = PTR_ERR(dp->debug);
		pr_err("failed to initialize debug, rc = %d\n", rc);
		dp->debug = NULL;
		goto error_debug;
	}

	return rc;
error_debug:
	dp_usbpd_put(dp->usbpd);
error_usbpd:
	dp_audio_put(dp->panel->audio);
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
error:
	mutex_destroy(&dp->session_lock);
	return rc;
}

static int dp_display_post_init(struct dp_display *dp_display)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!dp_display) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto end;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);
	if (IS_ERR_OR_NULL(dp)) {
		pr_err("invalid params\n");
		rc = -EINVAL;
		goto end;
	}

	rc = dp_init_sub_modules(dp);
	if (rc)
		goto end;

	dp_display_initialize_hdcp(dp);

	dp_display->post_init = NULL;
end:
	pr_debug("%s\n", rc ? "failed" : "success");
	return rc;
}

static int dp_display_set_mode(struct dp_display *dp_display, void *panel,
		struct dp_display_mode *mode)
{
	const u32 num_components = 3, default_bpp = 24;
	struct dp_display_private *dp;
	struct dp_panel *dp_panel;

	if (!dp_display || !panel) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	dp_panel = panel;
	if (!dp_panel->connector) {
		pr_err("invalid connector input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	mutex_lock(&dp->session_lock);
	mode->timing.bpp =
		dp_panel->connector->display_info.bpc * num_components;
	if (!mode->timing.bpp)
		mode->timing.bpp = default_bpp;

	mode->timing.bpp = dp->panel->get_mode_bpp(dp->panel,
			mode->timing.bpp, mode->timing.pixel_clk_khz);

	dp_panel->pinfo = mode->timing;
	dp_panel->init(dp_panel);
	mutex_unlock(&dp->session_lock);

	return 0;
}

static int dp_display_prepare(struct dp_display *dp_display, void *panel)
{
	struct dp_display_private *dp;
	struct dp_panel *dp_panel;

	if (!dp_display || !panel) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	dp_panel = panel;
	if (!dp_panel->connector) {
		pr_err("invalid connector input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	mutex_lock(&dp->session_lock);

	if (atomic_read(&dp->aborted))
		goto end;

	dp->aux->init(dp->aux, dp->parser->aux_cfg);

	if (dp->debug->psm_enabled) {
		dp->link->psm_config(dp->link, &dp->panel->link_info, false);
		dp->debug->psm_enabled = false;
	}

end:
	mutex_unlock(&dp->session_lock);

	return 0;
}

static int dp_display_set_stream_info(struct dp_display *dp_display,
			void *panel, u32 ch_id, u32 ch_start_slot,
			u32 ch_tot_slots, u32 pbn)
{
	int rc = 0;
	struct dp_panel *dp_panel;

	if (!dp_display || !panel) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	dp_panel = panel;
	dp_panel->set_stream_info(dp_panel, ch_id,
			ch_start_slot, ch_tot_slots, pbn);

	return rc;
}

static int dp_display_stream_enable(struct dp_display_private *dp,
			struct dp_panel *dp_panel)
{
	int rc = 0;

	rc = dp->ctrl->stream_on(dp->ctrl, dp_panel);

	if (dp->debug->tpg_state)
		dp_panel->tpg_config(dp_panel, true);

	if (!rc) {
		dp->active_panels[dp_panel->stream_id] = dp_panel;
		dp->active_stream_cnt++;
	}

	pr_debug("dp active_stream_cnt:%d\n", dp->active_stream_cnt);

	return rc;
}

static int dp_display_enable(struct dp_display *dp_display, void *panel)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!dp_display || !panel) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	mutex_lock(&dp->session_lock);

	if (dp->power_on) {
		pr_debug("Link already setup, perform setup stream\n");
		goto stream_setup;
	}

	if (atomic_read(&dp->aborted)) {
		pr_err("aborted\n");
		goto end;
	}

	rc = dp->ctrl->on(dp->ctrl, dp->mst.mst_active);
	if (rc)
		goto end;

	dp->power_on = true;

stream_setup:
	rc = dp_display_stream_enable(dp, panel);
	if (rc && (dp->active_stream_cnt == 0)) {
		dp->ctrl->off(dp->ctrl);
		dp->power_on = false;
	}

end:
	mutex_unlock(&dp->session_lock);
	return rc;
}

static void dp_display_stream_post_enable(struct dp_display_private *dp,
			struct dp_panel *dp_panel)
{
	dp_panel->spd_config(dp_panel);
	dp_panel->setup_hdr(dp_panel, NULL);

	dp_panel->audio->register_ext_disp(dp_panel->audio);
}

static int dp_display_post_enable(struct dp_display *dp_display, void *panel)
{
	struct dp_display_private *dp;
	struct dp_panel *dp_panel;
	struct edid *edid;

	if (!dp_display || !panel) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);
	dp_panel = panel;

	mutex_lock(&dp->session_lock);

	if (!dp->power_on) {
		pr_debug("Link not setup, return\n");
		goto end;
	}

	if (atomic_read(&dp->aborted)) {
		pr_err("aborted\n");
		goto end;
	}

	dp_display_stream_post_enable(dp, dp_panel);

	edid = dp_panel->edid_ctrl->edid;
	dp_panel->audio_supported = drm_detect_monitor_audio(edid);

	if (dp_panel->audio_supported) {
		dp_panel->audio->bw_code = dp->link->link_params.bw_code;
		dp_panel->audio->lane_count = dp->link->link_params.lane_count;
		dp_panel->audio->on(dp_panel->audio);
	}

	dp_display_update_hdcp_info(dp);

	if (dp_display_is_hdcp_enabled(dp)) {
		cancel_delayed_work_sync(&dp->hdcp_cb_work);

		dp->link->hdcp_status.hdcp_state = HDCP_STATE_AUTHENTICATING;
		queue_delayed_work(dp->wq, &dp->hdcp_cb_work, HZ / 2);
	}

end:
	/* clear framework event notifier */
	dp_display->post_open = NULL;
	dp->aux->state |= DP_STATE_CTRL_POWERED_ON;

	complete_all(&dp->notification_comp);
	mutex_unlock(&dp->session_lock);
	return 0;
}

static int dp_display_stream_pre_disable(struct dp_display_private *dp,
			struct dp_panel *dp_panel)
{
	dp_panel->audio->deregister_ext_disp(dp_panel->audio);
	dp->ctrl->stream_pre_off(dp->ctrl, dp_panel);

	return 0;
}

static int dp_display_pre_disable(struct dp_display *dp_display, void *panel)
{
	struct dp_display_private *dp;
	struct dp_panel *dp_panel = panel;
	int rc = 0;

	if (!dp_display || !panel) {
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
		dp->link->hdcp_status.hdcp_state = HDCP_STATE_INACTIVE;

		cancel_delayed_work_sync(&dp->hdcp_cb_work);
		if (dp->hdcp.ops->off)
			dp->hdcp.ops->off(dp->hdcp.data);
	}

	if (dp_panel->audio_supported)
		dp_panel->audio->off(dp_panel->audio);

	rc = dp_display_stream_pre_disable(dp, dp_panel);

	if (dp->usbpd->hpd_high && !dp_display_is_sink_count_zero(dp) &&
			dp->usbpd->alt_mode_cfg_done && !dp->mst.mst_active) {
		dp->link->psm_config(dp->link, &dp->panel->link_info, true);
		dp->debug->psm_enabled = true;
	}

end:
	mutex_unlock(&dp->session_lock);
	return 0;
}

static void dp_display_stream_disable(struct dp_display_private *dp,
			struct dp_panel *dp_panel)
{
	if (!dp->active_stream_cnt) {
		pr_err("invalid active_stream_cnt (%d)\n");
		return;
	}

	pr_debug("stream_id=%d, active_stream_cnt=%d\n",
			dp_panel->stream_id, dp->active_stream_cnt);

	dp->ctrl->stream_off(dp->ctrl, dp_panel);
	dp->active_panels[dp_panel->stream_id] = NULL;
	dp->active_stream_cnt--;
}

static int dp_display_disable(struct dp_display *dp_display, void *panel)
{
	struct dp_display_private *dp = NULL;
	struct dp_panel *dp_panel = NULL;

	if (!dp_display || !panel) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);
	dp_panel = panel;

	mutex_lock(&dp->session_lock);

	if (!dp->power_on || !dp->core_initialized) {
		pr_debug("Link already powered off, return\n");
		goto end;
	}

	dp_display_stream_disable(dp, dp_panel);

	if (dp->active_stream_cnt) {
		pr_debug("active stream present\n");
		goto end;
	}

	dp->ctrl->off(dp->ctrl);

	/*
	 * In case of framework reboot, the DP off sequence is executed without
	 * any notification from driver. Initialize post_open callback to notify
	 * DP connection once framework restarts.
	 */
	if (dp->usbpd->hpd_high && !dp_display_is_sink_count_zero(dp) &&
			dp->usbpd->alt_mode_cfg_done && !dp->mst.mst_active) {
		dp_display->post_open = dp_display_post_open;
		dp->dp_display.is_sst_connected = false;
	}

	dp->power_on = false;
end:
	dp_panel->deinit(dp_panel);
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

static int dp_display_unprepare(struct dp_display *dp_display, void *panel)
{
	struct dp_display_private *dp;

	if (!dp_display || !panel) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	mutex_lock(&dp->session_lock);

	if (dp->active_stream_cnt)
		goto end;

	if (!dp->mst.mst_active) {
		dp->aux->deinit(dp->aux);
		dp->aux->state = DP_STATE_CTRL_POWERED_OFF;
	}

	complete_all(&dp->notification_comp);

end:
	mutex_unlock(&dp->session_lock);

	return 0;
}

static int dp_display_validate_mode(struct dp_display *dp, void *panel,
		u32 mode_pclk_khz)
{
	const u32 num_components = 3, default_bpp = 24;
	struct dp_display_private *dp_display;
	struct drm_dp_link *link_info;
	u32 mode_rate_khz = 0, supported_rate_khz = 0, mode_bpp = 0;
	struct dp_panel *dp_panel;

	if (!dp || !mode_pclk_khz || !panel) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	dp_panel = panel;
	if (!dp_panel->connector) {
		pr_err("invalid connector\n");
		return -EINVAL;
	}

	dp_display = container_of(dp, struct dp_display_private, dp_display);
	link_info = &dp_display->panel->link_info;

	mode_bpp = dp_panel->connector->display_info.bpc * num_components;
	if (!mode_bpp)
		mode_bpp = default_bpp;

	mode_bpp = dp_panel->get_mode_bpp(dp_panel, mode_bpp, mode_pclk_khz);

	mode_rate_khz = mode_pclk_khz * mode_bpp;
	supported_rate_khz = link_info->num_lanes * link_info->rate * 8;

	if (mode_rate_khz > supported_rate_khz) {
		DP_MST_DEBUG("pclk:%d, supported_rate:%d\n",
				mode_pclk_khz, supported_rate_khz);

		return MODE_BAD;
	}

	return MODE_OK;
}

static int dp_display_get_modes(struct dp_display *dp, void *panel,
	struct dp_display_mode *dp_mode)
{
	struct dp_display_private *dp_display;
	struct dp_panel *dp_panel;
	int ret = 0;

	if (!dp || !panel) {
		pr_err("invalid params\n");
		return 0;
	}

	dp_panel = panel;
	if (!dp_panel->connector) {
		pr_err("invalid connector\n");
		return 0;
	}

	dp_display = container_of(dp, struct dp_display_private, dp_display);

	ret = dp_panel->get_modes(dp_panel, dp_panel->connector, dp_mode);
	if (dp_mode->timing.pixel_clk_khz)
		dp->max_pclk_khz = dp_mode->timing.pixel_clk_khz;
	return ret;
}

static int dp_display_config_hdr(struct dp_display *dp_display, void *panel,
			struct drm_msm_ext_hdr_metadata *hdr)
{
	int rc = 0;
	struct dp_display_private *dp;
	struct dp_panel *dp_panel;

	if (!dp_display || !panel) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);
	dp_panel = panel;

	rc = dp_panel->setup_hdr(dp_panel, hdr);

	return rc;
}

static int dp_display_create_workqueue(struct dp_display_private *dp)
{
	dp->wq = create_singlethread_workqueue("drm_dp");
	if (IS_ERR_OR_NULL(dp->wq)) {
		pr_err("Error creating wq\n");
		return -EPERM;
	}

	INIT_DELAYED_WORK(&dp->hdcp_cb_work, dp_display_hdcp_cb_work);
	INIT_DELAYED_WORK(&dp->connect_work, dp_display_connect_work);
	INIT_WORK(&dp->attention_work, dp_display_attention_work);

	return 0;
}

static int dp_display_fsa4480_callback(struct notifier_block *self,
		unsigned long event, void *data)
{
	return 0;
}

static int dp_display_init_aux_switch(struct dp_display_private *dp)
{
	int rc = 0;
	const char *phandle = "qcom,dp-aux-switch";
	struct notifier_block nb;

	if (!dp->pdev->dev.of_node) {
		pr_err("cannot find dev.of_node\n");
		rc = -ENODEV;
		goto end;
	}

	dp->aux_switch_node = of_parse_phandle(dp->pdev->dev.of_node,
			phandle, 0);
	if (!dp->aux_switch_node) {
		pr_warn("cannot parse %s handle\n", phandle);
		goto end;
	}

	nb.notifier_call = dp_display_fsa4480_callback;
	nb.priority = 0;

	rc = fsa4480_reg_notifier(&nb, dp->aux_switch_node);
	if (rc) {
		pr_err("failed to register notifier (%d)\n", rc);
		goto end;
	}

	fsa4480_unreg_notifier(&nb, dp->aux_switch_node);
end:
	return rc;
}

static int dp_display_mst_install(struct dp_display *dp_display,
			struct dp_mst_drm_install_info *mst_install_info)
{
	struct dp_display_private *dp;

	if (!dp_display || !mst_install_info) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	if (!mst_install_info->cbs->hpd || !mst_install_info->cbs->hpd_irq) {
		pr_err("invalid mst cbs\n");
		return -EINVAL;
	}

	dp_display->dp_mst_prv_info = mst_install_info->dp_mst_prv_info;

	if (!dp->parser->has_mst) {
		pr_debug("mst not enabled\n");
		return -EPERM;
	}

	memcpy(&dp->mst.cbs, mst_install_info->cbs, sizeof(dp->mst.cbs));
	dp->mst.drm_registered = true;

	DP_MST_DEBUG("dp mst drm installed\n");

	return 0;
}

static int dp_display_mst_uninstall(struct dp_display *dp_display)
{
	struct dp_display_private *dp;

	if (!dp_display) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	if (!dp->mst.drm_registered) {
		pr_debug("drm mst not registered\n");
		return -EPERM;
	}

	dp = container_of(dp_display, struct dp_display_private,
				dp_display);
	memset(&dp->mst.cbs, 0, sizeof(dp->mst.cbs));
	dp->mst.drm_registered = false;

	DP_MST_DEBUG("dp mst drm uninstalled\n");

	return 0;
}

static int dp_display_mst_connector_install(struct dp_display *dp_display,
		struct drm_connector *connector)
{
	int rc = 0;
	struct dp_panel_in panel_in;
	struct dp_panel *dp_panel;
	struct dp_display_private *dp;

	if (!dp_display || !connector) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	if (!dp->mst.drm_registered) {
		pr_debug("drm mst not registered\n");
		return -EPERM;
	}

	panel_in.dev = &dp->pdev->dev;
	panel_in.aux = dp->aux;
	panel_in.catalog = &dp->catalog->panel;
	panel_in.link = dp->link;
	panel_in.connector = connector;
	panel_in.base_panel = dp->panel;

	dp_panel = dp_panel_get(&panel_in);
	if (IS_ERR(dp_panel)) {
		rc = PTR_ERR(dp_panel);
		pr_err("failed to initialize panel, rc = %d\n", rc);
		return rc;
	}

	dp_panel->audio = dp_audio_get(dp->pdev, dp_panel, &dp->catalog->audio);
	if (IS_ERR(dp_panel->audio)) {
		rc = PTR_ERR(dp_panel->audio);
		pr_err("[mst] failed to initialize audio, rc = %d\n", rc);
		dp_panel->audio = NULL;
		return rc;
	}

	DP_MST_DEBUG("dp mst connector installed. conn:%d\n",
			connector->base.id);

	return 0;
}

static int dp_display_mst_connector_uninstall(struct dp_display *dp_display,
			struct drm_connector *connector)
{
	int rc = 0;
	struct sde_connector *sde_conn;
	struct dp_panel *dp_panel;
	struct dp_display_private *dp;

	if (!dp_display || !connector) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	if (!dp->mst.drm_registered) {
		pr_debug("drm mst not registered\n");
		return -EPERM;
	}

	sde_conn = to_sde_connector(connector);
	if (!sde_conn->drv_panel) {
		pr_err("invalid panel for connector:%d\n", connector->base.id);
		return -EINVAL;
	}

	dp_panel = sde_conn->drv_panel;
	dp_audio_put(dp_panel->audio);
	dp_panel_put(dp_panel);

	DP_MST_DEBUG("dp mst connector uninstalled. conn:%d\n",
			connector->base.id);

	return rc;
}

static int dp_display_mst_connector_update_edid(struct dp_display *dp_display,
			struct drm_connector *connector,
			struct edid *edid)
{
	int rc = 0;
	struct sde_connector *sde_conn;
	struct dp_panel *dp_panel;
	struct dp_display_private *dp;

	if (!dp_display || !connector || !edid) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	if (!dp->mst.drm_registered) {
		pr_debug("drm mst not registered\n");
		return -EPERM;
	}

	sde_conn = to_sde_connector(connector);
	if (!sde_conn->drv_panel) {
		pr_err("invalid panel for connector:%d\n", connector->base.id);
		return -EINVAL;
	}

	dp_panel = sde_conn->drv_panel;
	rc = dp_panel->update_edid(dp_panel, edid);

	DP_MST_DEBUG("dp mst connector:%d edid updated. mode_cnt:%d\n",
			connector->base.id, rc);

	return rc;
}

static int dp_display_get_mst_caps(struct dp_display *dp_display,
			struct dp_mst_caps *mst_caps)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!dp_display || !mst_caps) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	mst_caps->has_mst = dp->parser->has_mst;
	mst_caps->max_streams_supported = (mst_caps->has_mst) ? 2 : 0;
	mst_caps->max_dpcd_transaction_bytes = (mst_caps->has_mst) ? 16 : 0;
	mst_caps->drm_aux = dp->aux->drm_aux;

	return rc;
}

static int dp_display_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!pdev || !pdev->dev.of_node) {
		pr_err("pdev not found\n");
		rc = -ENODEV;
		goto bail;
	}

	dp = devm_kzalloc(&pdev->dev, sizeof(*dp), GFP_KERNEL);
	if (!dp) {
		rc = -ENOMEM;
		goto bail;
	}

	init_completion(&dp->notification_comp);
	init_completion(&dp->disconnect_comp);

	dp->pdev = pdev;
	dp->name = "drm_dp";

	memset(&dp->mst, 0, sizeof(dp->mst));
	atomic_set(&dp->aborted, 0);

	rc = dp_display_init_aux_switch(dp);
	if (rc) {
		rc = -EPROBE_DEFER;
		goto error;
	}

	rc = dp_display_create_workqueue(dp);
	if (rc) {
		pr_err("Failed to create workqueue\n");
		goto error;
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
	g_dp_display->post_open     = dp_display_post_open;
	g_dp_display->post_init     = dp_display_post_init;
	g_dp_display->config_hdr    = dp_display_config_hdr;
	g_dp_display->mst_install   = dp_display_mst_install;
	g_dp_display->mst_uninstall = dp_display_mst_uninstall;
	g_dp_display->mst_connector_install = dp_display_mst_connector_install;
	g_dp_display->mst_connector_uninstall =
					dp_display_mst_connector_uninstall;
	g_dp_display->mst_connector_update_edid =
					dp_display_mst_connector_update_edid;
	g_dp_display->get_mst_caps = dp_display_get_mst_caps;
	g_dp_display->set_stream_info = dp_display_set_stream_info;

	rc = component_add(&pdev->dev, &dp_display_comp_ops);
	if (rc) {
		pr_err("component add failed, rc=%d\n", rc);
		goto error;
	}

	return 0;
error:
	devm_kfree(&pdev->dev, dp);
bail:
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

int dp_display_get_num_of_streams(void)
{
	return 2;
}

static int dp_display_remove(struct platform_device *pdev)
{
	struct dp_display_private *dp;

	if (!pdev)
		return -EINVAL;

	dp = platform_get_drvdata(pdev);

	dp_display_deinit_sub_modules(dp);

	if (dp->wq)
		destroy_workqueue(dp->wq);

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
		.suppress_bind_attrs = true,
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
late_initcall(dp_display_init);

static void __exit dp_display_cleanup(void)
{
	platform_driver_unregister(&dp_display_driver);
}
module_exit(dp_display_cleanup);

