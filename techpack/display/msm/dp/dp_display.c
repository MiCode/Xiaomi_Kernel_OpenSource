// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/component.h>
#include <linux/of_irq.h>
#include <linux/extcon.h>
#include <linux/soc/qcom/fsa4480-i2c.h>
#include <linux/usb/usbpd.h>

#include "sde_connector.h"

#include "msm_drv.h"
#include "dp_hpd.h"
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
#include "sde_dbg.h"

#define DP_MST_DEBUG(fmt, ...) DP_DEBUG(fmt, ##__VA_ARGS__)

#define dp_display_state_show(x) { \
	DP_ERR("%s: state (0x%x): %s\n", x, dp->state, \
		dp_display_state_name(dp->state)); \
	SDE_EVT32_EXTERNAL(dp->state); }

#define dp_display_state_log(x) { \
	DP_DEBUG("%s: state (0x%x): %s\n", x, dp->state, \
		dp_display_state_name(dp->state)); \
	SDE_EVT32_EXTERNAL(dp->state); }

#define dp_display_state_is(x) (dp->state & (x))
#define dp_display_state_add(x) { \
	(dp->state |= (x)); \
	dp_display_state_log("add "#x); }
#define dp_display_state_remove(x) { \
	(dp->state &= ~(x)); \
	dp_display_state_log("remove "#x); }

enum dp_display_states {
	DP_STATE_DISCONNECTED           = 0,
	DP_STATE_CONFIGURED             = BIT(0),
	DP_STATE_INITIALIZED            = BIT(1),
	DP_STATE_READY                  = BIT(2),
	DP_STATE_CONNECTED              = BIT(3),
	DP_STATE_CONNECT_NOTIFIED       = BIT(4),
	DP_STATE_DISCONNECT_NOTIFIED    = BIT(5),
	DP_STATE_ENABLED                = BIT(6),
	DP_STATE_SUSPENDED              = BIT(7),
	DP_STATE_ABORTED                = BIT(8),
	DP_STATE_HDCP_ABORTED           = BIT(9),
	DP_STATE_SRC_PWRDN              = BIT(10),
};

static char *dp_display_state_name(enum dp_display_states state)
{
	static char buf[SZ_1K];
	u32 len = 0;

	memset(buf, 0, SZ_1K);

	if (state & DP_STATE_CONFIGURED)
		len += scnprintf(buf + len, sizeof(buf) - len, "|%s|",
			"CONFIGURED");

	if (state & DP_STATE_INITIALIZED)
		len += scnprintf(buf + len, sizeof(buf) - len, "|%s|",
			"INITIALIZED");

	if (state & DP_STATE_READY)
		len += scnprintf(buf + len, sizeof(buf) - len, "|%s|",
			"READY");

	if (state & DP_STATE_CONNECTED)
		len += scnprintf(buf + len, sizeof(buf) - len, "|%s|",
			"CONNECTED");

	if (state & DP_STATE_CONNECT_NOTIFIED)
		len += scnprintf(buf + len, sizeof(buf) - len, "|%s|",
			"CONNECT_NOTIFIED");

	if (state & DP_STATE_DISCONNECT_NOTIFIED)
		len += scnprintf(buf + len, sizeof(buf) - len, "|%s|",
			"DISCONNECT_NOTIFIED");

	if (state & DP_STATE_ENABLED)
		len += scnprintf(buf + len, sizeof(buf) - len, "|%s|",
			"ENABLED");

	if (state & DP_STATE_SUSPENDED)
		len += scnprintf(buf + len, sizeof(buf) - len, "|%s|",
			"SUSPENDED");

	if (state & DP_STATE_ABORTED)
		len += scnprintf(buf + len, sizeof(buf) - len, "|%s|",
			"ABORTED");

	if (state & DP_STATE_HDCP_ABORTED)
		len += scnprintf(buf + len, sizeof(buf) - len, "|%s|",
			"HDCP_ABORTED");

	if (state & DP_STATE_SRC_PWRDN)
		len += scnprintf(buf + len, sizeof(buf) - len, "|%s|",
			"SRC_PWRDN");

	if (!strlen(buf))
		return "DISCONNECTED";

	return buf;
}

static struct dp_display *g_dp_display;
#define HPD_STRING_SIZE 30

struct dp_hdcp_dev {
	void *fd;
	struct sde_hdcp_ops *ops;
	enum sde_hdcp_version ver;
};

struct dp_hdcp {
	void *data;
	struct sde_hdcp_ops *ops;

	u32 source_cap;

	struct dp_hdcp_dev dev[HDCP_VERSION_MAX];
};

struct dp_mst {
	bool mst_active;

	bool drm_registered;
	struct dp_mst_drm_cbs cbs;
};

struct dp_display_private {
	char *name;
	int irq;

	enum drm_connector_status cached_connector_status;
	enum dp_display_states state;

	struct platform_device *pdev;
	struct device_node *aux_switch_node;
	struct dentry *root;
	struct completion notification_comp;

	struct dp_hpd     *hpd;
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

	struct dp_hpd_cb hpd_cb;
	struct dp_display_mode mode;
	struct dp_display dp_display;
	struct msm_drm_private *priv;

	struct workqueue_struct *wq;
	struct delayed_work hdcp_cb_work;
	struct work_struct connect_work;
	struct work_struct attention_work;
	struct mutex session_lock;
	bool hdcp_delayed_off;

	u32 active_stream_cnt;
	struct dp_mst mst;

	u32 tot_dsc_blks_in_use;

	bool process_hpd_connect;

	struct notifier_block usb_nb;
};

static const struct of_device_id dp_dt_match[] = {
	{.compatible = "qcom,dp-display"},
	{}
};

static inline bool dp_display_is_hdcp_enabled(struct dp_display_private *dp)
{
	return dp->link->hdcp_status.hdcp_version && dp->hdcp.ops;
}

static irqreturn_t dp_display_irq(int irq, void *dev_id)
{
	struct dp_display_private *dp = dev_id;

	if (!dp) {
		DP_ERR("invalid data\n");
		return IRQ_NONE;
	}

	/* DP HPD isr */
	if (dp->hpd->type ==  DP_HPD_LPHW)
		dp->hpd->isr(dp->hpd);

	/* DP controller isr */
	dp->ctrl->isr(dp->ctrl);

	/* DP aux isr */
	dp->aux->isr(dp->aux);

	/* HDCP isr */
	if (dp_display_is_hdcp_enabled(dp) && dp->hdcp.ops->isr) {
		if (dp->hdcp.ops->isr(dp->hdcp.data))
			DP_ERR("dp_hdcp_isr failed\n");
	}

	return IRQ_HANDLED;
}
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

static bool dp_display_is_ready(struct dp_display_private *dp)
{
	return dp->hpd->hpd_high && dp_display_state_is(DP_STATE_CONNECTED) &&
		!dp_display_is_sink_count_zero(dp) &&
		dp->hpd->alt_mode_cfg_done;
}

static void dp_audio_enable(struct dp_display_private *dp, bool enable)
{
	struct dp_panel *dp_panel;
	int idx;

	for (idx = DP_STREAM_0; idx < DP_STREAM_MAX; idx++) {
		if (!dp->active_panels[idx])
			continue;
		dp_panel = dp->active_panels[idx];

		if (dp_panel->audio_supported) {
			if (enable) {
				dp_panel->audio->bw_code =
					dp->link->link_params.bw_code;
				dp_panel->audio->lane_count =
					dp->link->link_params.lane_count;
				dp_panel->audio->on(dp_panel->audio);
			} else {
				dp_panel->audio->off(dp_panel->audio);
			}
		}
	}
}

static void dp_display_update_hdcp_status(struct dp_display_private *dp,
					bool reset)
{
	if (reset) {
		dp->link->hdcp_status.hdcp_state = HDCP_STATE_INACTIVE;
		dp->link->hdcp_status.hdcp_version = HDCP_VERSION_NONE;
	}

	memset(dp->debug->hdcp_status, 0, sizeof(dp->debug->hdcp_status));

	snprintf(dp->debug->hdcp_status, sizeof(dp->debug->hdcp_status),
		"%s: %s\ncaps: %d\n",
		sde_hdcp_version(dp->link->hdcp_status.hdcp_version),
		sde_hdcp_state_name(dp->link->hdcp_status.hdcp_state),
		dp->hdcp.source_cap);
}

static void dp_display_update_hdcp_info(struct dp_display_private *dp)
{
	void *fd = NULL;
	struct dp_hdcp_dev *dev = NULL;
	struct sde_hdcp_ops *ops = NULL;
	int i = HDCP_VERSION_2P2;

	dp_display_update_hdcp_status(dp, true);

	dp->hdcp.data = NULL;
	dp->hdcp.ops = NULL;

	if (dp->debug->hdcp_disabled || dp->debug->sim_mode)
		return;

	while (i) {
		dev = &dp->hdcp.dev[i];
		ops = dev->ops;
		fd = dev->fd;

		i >>= 1;

		if (!(dp->hdcp.source_cap & dev->ver))
			continue;

		if (ops->sink_support(fd)) {
			dp->hdcp.data = fd;
			dp->hdcp.ops = ops;
			dp->link->hdcp_status.hdcp_version = dev->ver;
			break;
		}
	}

	DP_DEBUG("HDCP version supported: %s\n",
		sde_hdcp_version(dp->link->hdcp_status.hdcp_version));
}

static void dp_display_check_source_hdcp_caps(struct dp_display_private *dp)
{
	int i;
	struct dp_hdcp_dev *hdcp_dev = dp->hdcp.dev;

	if (dp->debug->hdcp_disabled) {
		DP_DEBUG("hdcp disabled\n");
		return;
	}

	for (i = 0; i < HDCP_VERSION_MAX; i++) {
		struct dp_hdcp_dev *dev = &hdcp_dev[i];
		struct sde_hdcp_ops *ops = dev->ops;
		void *fd = dev->fd;

		if (!fd || !ops)
			continue;

		if (ops->set_mode && ops->set_mode(fd, dp->mst.mst_active))
			continue;

		if (!(dp->hdcp.source_cap & dev->ver) &&
				ops->feature_supported &&
				ops->feature_supported(fd))
			dp->hdcp.source_cap |= dev->ver;
	}

	dp_display_update_hdcp_status(dp, false);
}

static void dp_display_hdcp_register_streams(struct dp_display_private *dp)
{
	int rc;
	size_t i;
	struct sde_hdcp_ops *ops = dp->hdcp.ops;
	void *data = dp->hdcp.data;

	if (dp_display_is_ready(dp) && dp->mst.mst_active && ops &&
			ops->register_streams){
		struct stream_info streams[DP_STREAM_MAX];
		int index = 0;

		DP_DEBUG("Registering all active panel streams with HDCP\n");
		for (i = DP_STREAM_0; i < DP_STREAM_MAX; i++) {
			if (!dp->active_panels[i])
				continue;
			streams[index].stream_id = i;
			streams[index].virtual_channel =
				dp->active_panels[i]->vcpi;
			index++;
		}

		if (index > 0) {
			rc = ops->register_streams(data, index, streams);
			if (rc)
				DP_ERR("failed to register streams. rc = %d\n",
					rc);
		}
	}
}

static void dp_display_hdcp_deregister_stream(struct dp_display_private *dp,
		enum dp_stream_id stream_id)
{
	if (dp->hdcp.ops->deregister_streams) {
		struct stream_info stream = {stream_id,
				dp->active_panels[stream_id]->vcpi};

		DP_DEBUG("Deregistering stream within HDCP library\n");
		dp->hdcp.ops->deregister_streams(dp->hdcp.data, 1, &stream);
	}
}

static void dp_display_abort_hdcp(struct dp_display_private *dp,
		bool abort)
{
	u32 i = HDCP_VERSION_2P2;
	struct dp_hdcp_dev *dev = NULL;

	while (i) {
		dev = &dp->hdcp.dev[i];
		i >>= 1;
		if (!(dp->hdcp.source_cap & dev->ver))
			continue;

		dev->ops->abort(dev->fd, abort);
	}
}

static void dp_display_hdcp_cb_work(struct work_struct *work)
{
	struct dp_display_private *dp;
	struct delayed_work *dw = to_delayed_work(work);
	struct sde_hdcp_ops *ops;
	struct dp_link_hdcp_status *status;
	void *data;
	int rc = 0;
	u32 hdcp_auth_state;
	u8 sink_status = 0;

	dp = container_of(dw, struct dp_display_private, hdcp_cb_work);

	if (!dp_display_state_is(DP_STATE_ENABLED | DP_STATE_CONNECTED) ||
	     dp_display_state_is(DP_STATE_ABORTED | DP_STATE_HDCP_ABORTED))
		return;

	if (dp_display_state_is(DP_STATE_SUSPENDED)) {
		DP_DEBUG("System suspending. Delay HDCP operations\n");
		queue_delayed_work(dp->wq, &dp->hdcp_cb_work, HZ);
		return;
	}

	if (dp->hdcp_delayed_off) {
		if (dp->hdcp.ops && dp->hdcp.ops->off)
			dp->hdcp.ops->off(dp->hdcp.data);
		dp_display_update_hdcp_status(dp, true);
		dp->hdcp_delayed_off = false;
	}

	if (dp->debug->hdcp_wait_sink_sync) {
		drm_dp_dpcd_readb(dp->aux->drm_aux, DP_SINK_STATUS,
				&sink_status);
		sink_status &= (DP_RECEIVE_PORT_0_STATUS |
				DP_RECEIVE_PORT_1_STATUS);
		if (sink_status < 1) {
			DP_DEBUG("Sink not synchronized. Queuing again then exiting\n");
			queue_delayed_work(dp->wq, &dp->hdcp_cb_work, HZ);
			return;
		}
	}

	status = &dp->link->hdcp_status;

	if (status->hdcp_state == HDCP_STATE_INACTIVE) {
		dp_display_check_source_hdcp_caps(dp);
		dp_display_update_hdcp_info(dp);

		if (dp_display_is_hdcp_enabled(dp)) {
			if (dp->hdcp.ops && dp->hdcp.ops->on &&
					dp->hdcp.ops->on(dp->hdcp.data)) {
				dp_display_update_hdcp_status(dp, true);
				return;
			}
		} else {
			dp_display_update_hdcp_status(dp, true);
			return;
		}
	}

	rc = dp->catalog->ctrl.read_hdcp_status(&dp->catalog->ctrl);
	if (rc >= 0) {
		hdcp_auth_state = (rc >> 20) & 0x3;
		DP_DEBUG("hdcp auth state %d\n", hdcp_auth_state);
	}

	ops = dp->hdcp.ops;
	data = dp->hdcp.data;

	DP_DEBUG("%s: %s\n", sde_hdcp_version(status->hdcp_version),
		sde_hdcp_state_name(status->hdcp_state));

	dp_display_update_hdcp_status(dp, false);

	if (status->hdcp_state != HDCP_STATE_AUTHENTICATED &&
		dp->debug->force_encryption && ops && ops->force_encryption)
		ops->force_encryption(data, dp->debug->force_encryption);

	switch (status->hdcp_state) {
	case HDCP_STATE_INACTIVE:
		dp_display_hdcp_register_streams(dp);
		if (dp->hdcp.ops && dp->hdcp.ops->authenticate)
			rc = dp->hdcp.ops->authenticate(data);
		if (!rc)
			status->hdcp_state = HDCP_STATE_AUTHENTICATING;
		break;
	case HDCP_STATE_AUTH_FAIL:
		if (dp_display_is_ready(dp) &&
		    dp_display_state_is(DP_STATE_ENABLED)) {
			if (ops && ops->on && ops->on(data)) {
				dp_display_update_hdcp_status(dp, true);
				return;
			}
			dp_display_hdcp_register_streams(dp);
			if (ops && ops->reauthenticate) {
				rc = ops->reauthenticate(data);
				if (rc)
					DP_ERR("failed rc=%d\n", rc);
			}
			status->hdcp_state = HDCP_STATE_AUTHENTICATING;
		} else {
			DP_DEBUG("not reauthenticating, cable disconnected\n");
		}
		break;
	default:
		dp_display_hdcp_register_streams(dp);
		break;
	}
}

static void dp_display_notify_hdcp_status_cb(void *ptr,
		enum sde_hdcp_state state)
{
	struct dp_display_private *dp = ptr;

	if (!dp) {
		DP_ERR("invalid input\n");
		return;
	}

	dp->link->hdcp_status.hdcp_state = state;

	queue_delayed_work(dp->wq, &dp->hdcp_cb_work, HZ/4);
}

static void dp_display_deinitialize_hdcp(struct dp_display_private *dp)
{
	if (!dp) {
		DP_ERR("invalid input\n");
		return;
	}

	sde_dp_hdcp2p2_deinit(dp->hdcp.data);
}

static int dp_display_initialize_hdcp(struct dp_display_private *dp)
{
	struct sde_hdcp_init_data hdcp_init_data;
	struct dp_parser *parser;
	void *fd;
	int rc = 0;

	if (!dp) {
		DP_ERR("invalid input\n");
		return -EINVAL;
	}

	parser = dp->parser;

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
	hdcp_init_data.hdcp_io       = &parser->get_io(parser,
						"hdcp_physical")->io;
	hdcp_init_data.revision      = &dp->panel->link_info.revision;
	hdcp_init_data.msm_hdcp_dev  = dp->parser->msm_hdcp_dev;

	fd = sde_hdcp_1x_init(&hdcp_init_data);
	if (IS_ERR_OR_NULL(fd)) {
		DP_ERR("Error initializing HDCP 1.x\n");
		rc = -EINVAL;
		goto error;
	}

	dp->hdcp.dev[HDCP_VERSION_1X].fd = fd;
	dp->hdcp.dev[HDCP_VERSION_1X].ops = sde_hdcp_1x_get(fd);
	dp->hdcp.dev[HDCP_VERSION_1X].ver = HDCP_VERSION_1X;
	DP_DEBUG("HDCP 1.3 initialized\n");

	fd = sde_dp_hdcp2p2_init(&hdcp_init_data);
	if (IS_ERR_OR_NULL(fd)) {
		DP_ERR("Error initializing HDCP 2.x\n");
		rc = -EINVAL;
		goto error;
	}

	dp->hdcp.dev[HDCP_VERSION_2P2].fd = fd;
	dp->hdcp.dev[HDCP_VERSION_2P2].ops = sde_dp_hdcp2p2_get(fd);
	dp->hdcp.dev[HDCP_VERSION_2P2].ver = HDCP_VERSION_2P2;
	DP_DEBUG("HDCP 2.2 initialized\n");

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
		DP_ERR("invalid param(s), dev %pK, pdev %pK, master %pK\n",
				dev, pdev, master);
		rc = -EINVAL;
		goto end;
	}

	drm = dev_get_drvdata(master);
	dp = platform_get_drvdata(pdev);
	if (!drm || !dp) {
		DP_ERR("invalid param(s), drm %pK, dp %pK\n",
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
		DP_ERR("invalid param(s)\n");
		return;
	}

	dp = platform_get_drvdata(pdev);
	if (!dp) {
		DP_ERR("Invalid params\n");
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

static void dp_display_send_hpd_event(struct dp_display_private *dp)
{
	struct drm_device *dev = NULL;
	struct drm_connector *connector;
	char name[HPD_STRING_SIZE], status[HPD_STRING_SIZE],
		bpp[HPD_STRING_SIZE], pattern[HPD_STRING_SIZE];
	char *envp[5];

	if (dp->mst.mst_active) {
		DP_DEBUG("skip notification for mst mode\n");
		dp_display_state_remove(DP_STATE_DISCONNECT_NOTIFIED);
		return;
	}

	connector = dp->dp_display.base_connector;

	if (!connector) {
		DP_ERR("connector not set\n");
		return;
	}

	connector->status = connector->funcs->detect(connector, false);
	if (dp->cached_connector_status == connector->status) {
		DP_DEBUG("connector status (%d) unchanged, skipping uevent\n",
				dp->cached_connector_status);
		return;
	}

	dp->cached_connector_status = connector->status;

	dev = connector->dev;

	snprintf(name, HPD_STRING_SIZE, "name=%s", connector->name);
	snprintf(status, HPD_STRING_SIZE, "status=%s",
		drm_get_connector_status_name(connector->status));
	snprintf(bpp, HPD_STRING_SIZE, "bpp=%d",
		dp_link_bit_depth_to_bpp(
		dp->link->test_video.test_bit_depth));
	snprintf(pattern, HPD_STRING_SIZE, "pattern=%d",
		dp->link->test_video.test_video_pattern);

	DP_DEBUG("[%s]:[%s] [%s] [%s]\n", name, status, bpp, pattern);
	envp[0] = name;
	envp[1] = status;
	envp[2] = bpp;
	envp[3] = pattern;
	envp[4] = NULL;
	kobject_uevent_env(&dev->primary->kdev->kobj, KOBJ_CHANGE,
			envp);

	if (connector->status == connector_status_connected) {
		dp_display_state_add(DP_STATE_CONNECT_NOTIFIED);
		dp_display_state_remove(DP_STATE_DISCONNECT_NOTIFIED);
	} else {
		dp_display_state_add(DP_STATE_DISCONNECT_NOTIFIED);
		dp_display_state_remove(DP_STATE_CONNECT_NOTIFIED);
	}
}

static int dp_display_send_hpd_notification(struct dp_display_private *dp)
{
	int ret = 0;
	bool hpd = !!dp_display_state_is(DP_STATE_CONNECTED);

	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY, dp->state, hpd);

	/*
	 * Send the notification only if there is any change. This check is
	 * necessary since it is possible that the connect_work may or may not
	 * skip sending the notification in order to respond to a pending
	 * attention message. Attention work thread will always attempt to
	 * send the notification after successfully handling the attention
	 * message. This check here will avoid any unintended duplicate
	 * notifications.
	 */
	if (dp_display_state_is(DP_STATE_CONNECT_NOTIFIED) && hpd) {
		DP_DEBUG("connection notified already, skip notification\n");
		goto skip_wait;
	} else if (dp_display_state_is(DP_STATE_DISCONNECT_NOTIFIED) && !hpd) {
		DP_DEBUG("disonnect notified already, skip notification\n");
		goto skip_wait;
	}

	dp->aux->state |= DP_STATE_NOTIFICATION_SENT;

	if (!dp->mst.mst_active)
		dp->dp_display.is_sst_connected = hpd;
	else
		dp->dp_display.is_sst_connected = false;

	reinit_completion(&dp->notification_comp);
	dp_display_send_hpd_event(dp);

	if (hpd && dp->mst.mst_active)
		goto skip_wait;

	if (!dp->mst.mst_active &&
			(!!dp_display_state_is(DP_STATE_ENABLED) == hpd))
		goto skip_wait;

	if (!wait_for_completion_timeout(&dp->notification_comp,
						HZ * 5)) {
		DP_WARN("%s timeout\n", hpd ? "connect" : "disconnect");
		ret = -EINVAL;
	}
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_EXIT, dp->state, hpd, ret);
	return ret;
skip_wait:
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_EXIT, dp->state, hpd, ret);
	return 0;
}

static void dp_display_update_mst_state(struct dp_display_private *dp,
					bool state)
{
	dp->mst.mst_active = state;
	dp->panel->mst_state = state;
}

static void dp_display_process_mst_hpd_high(struct dp_display_private *dp,
						bool mst_probe)
{
	bool is_mst_receiver;
	struct dp_mst_hpd_info info;
	const unsigned long clear_mstm_ctrl_timeout_us = 100000;
	u8 old_mstm_ctrl;
	int ret;

	if (!dp->parser->has_mst || !dp->mst.drm_registered) {
		DP_MST_DEBUG("mst not enabled. has_mst:%d, registered:%d\n",
				dp->parser->has_mst, dp->mst.drm_registered);
		return;
	}

	DP_MST_DEBUG("mst_hpd_high work. mst_probe:%d\n", mst_probe);

	if (!dp->mst.mst_active) {
		is_mst_receiver = dp->panel->read_mst_cap(dp->panel);

		if (!is_mst_receiver) {
			DP_MST_DEBUG("sink doesn't support mst\n");
			return;
		}

		/* clear sink mst state */
		drm_dp_dpcd_readb(dp->aux->drm_aux, DP_MSTM_CTRL,
				&old_mstm_ctrl);
		drm_dp_dpcd_writeb(dp->aux->drm_aux, DP_MSTM_CTRL, 0);

		/* add extra delay if MST state is not cleared */
		if (old_mstm_ctrl) {
			DP_MST_DEBUG("MSTM_CTRL is not cleared, wait %dus\n",
					clear_mstm_ctrl_timeout_us);
			usleep_range(clear_mstm_ctrl_timeout_us,
				clear_mstm_ctrl_timeout_us + 1000);
		}

		ret = drm_dp_dpcd_writeb(dp->aux->drm_aux, DP_MSTM_CTRL,
				 DP_MST_EN | DP_UP_REQ_EN | DP_UPSTREAM_IS_SRC);
		if (ret < 0) {
			DP_ERR("sink mst enablement failed\n");
			return;
		}

		dp_display_update_mst_state(dp, true);
	} else if (dp->mst.mst_active && mst_probe) {
		info.mst_protocol = dp->parser->has_mst_sideband;
		info.mst_port_cnt = dp->debug->mst_port_cnt;
		info.edid = dp->debug->get_edid(dp->debug);

		if (dp->mst.cbs.hpd)
			dp->mst.cbs.hpd(&dp->dp_display, true, &info);
	}

	DP_MST_DEBUG("mst_hpd_high. mst_active:%d\n", dp->mst.mst_active);
}

static void dp_display_host_init(struct dp_display_private *dp)
{
	bool flip = false;
	bool reset;

	if (dp_display_state_is(DP_STATE_INITIALIZED)) {
		dp_display_state_log("[already initialized]");
		return;
	}

	if (dp->hpd->orientation == ORIENTATION_CC2)
		flip = true;

	reset = dp->debug->sim_mode ? false :
		(!dp->hpd->multi_func || !dp->hpd->peer_usb_comm);

	dp->power->init(dp->power, flip);
	dp->hpd->host_init(dp->hpd, &dp->catalog->hpd);
	dp->ctrl->init(dp->ctrl, flip, reset);
	enable_irq(dp->irq);
	dp_display_abort_hdcp(dp, false);

	dp_display_state_add(DP_STATE_INITIALIZED);

	/* log this as it results from user action of cable connection */
	DP_INFO("[OK]\n");
}

static void dp_display_host_ready(struct dp_display_private *dp)
{
	if (!dp_display_state_is(DP_STATE_INITIALIZED)) {
		dp_display_state_show("[not initialized]");
		return;
	}

	if (dp_display_state_is(DP_STATE_READY)) {
		dp_display_state_log("[already ready]");
		return;
	}

	/*
	 * Reset the aborted state for AUX and CTRL modules. This will
	 * allow these modules to execute normally in response to the
	 * cable connection event.
	 *
	 * One corner case still exists. While the execution flow ensures
	 * that cable disconnection flushes all pending work items on the DP
	 * workqueue, and waits for the user module to clean up the DP
	 * connection session, it is possible that the system delays can
	 * lead to timeouts in the connect path. As a result, the actual
	 * connection callback from user modules can come in late and can
	 * race against a subsequent connection event here which would have
	 * reset the aborted flags. There is no clear solution for this since
	 * the connect/disconnect notifications do not currently have any
	 * sessions IDs.
	 */
	dp->aux->abort(dp->aux, false);
	dp->ctrl->abort(dp->ctrl, false);

	dp->aux->init(dp->aux, dp->parser->aux_cfg);
	dp->panel->init(dp->panel);

	dp_display_state_add(DP_STATE_READY);
	/* log this as it results from user action of cable connection */
	DP_INFO("[OK]\n");
}

static void dp_display_host_unready(struct dp_display_private *dp)
{
	if (!dp_display_state_is(DP_STATE_INITIALIZED)) {
		dp_display_state_show("[not initialized]");
		return;
	}

	if (!dp_display_state_is(DP_STATE_READY)) {
		dp_display_state_show("[not ready]");
		return;
	}

	dp_display_state_remove(DP_STATE_READY);
	dp->aux->deinit(dp->aux);
	/* log this as it results from user action of cable connection */
	DP_INFO("[OK]\n");
}

static void dp_display_host_deinit(struct dp_display_private *dp)
{
	if (dp->active_stream_cnt) {
		SDE_EVT32_EXTERNAL(dp->state, dp->active_stream_cnt);
		DP_DEBUG("active stream present\n");
		return;
	}

	if (!dp_display_state_is(DP_STATE_INITIALIZED)) {
		dp_display_state_show("[not initialized]");
		return;
	}

	dp_display_abort_hdcp(dp, true);
	dp->ctrl->deinit(dp->ctrl);
	dp->hpd->host_deinit(dp->hpd, &dp->catalog->hpd);
	dp->power->deinit(dp->power);
	disable_irq(dp->irq);
	dp->aux->state = 0;

	dp_display_state_remove(DP_STATE_INITIALIZED);

	/* log this as it results from user action of cable dis-connection */
	DP_INFO("[OK]\n");
}

static int dp_display_process_hpd_high(struct dp_display_private *dp)
{
	int rc = -EINVAL;

	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY, dp->state);
	mutex_lock(&dp->session_lock);

	if (dp_display_state_is(DP_STATE_CONNECTED)) {
		DP_DEBUG("dp already connected, skipping hpd high\n");
		mutex_unlock(&dp->session_lock);
		return -EISCONN;
	}

	dp_display_state_add(DP_STATE_CONNECTED);

	dp->dp_display.max_pclk_khz = min(dp->parser->max_pclk_khz,
					dp->debug->max_pclk_khz);

	/*
	 * If dp video session is not restored from a previous session teardown
	 * by userspace, ensure the host_init is executed, in such a scenario,
	 * so that all the required DP resources are enabled.
	 *
	 * Below is one of the sequences of events which describe the above
	 * scenario:
	 *  a. Source initiated power down resulting in host_deinit.
	 *  b. Sink issues hpd low attention without physical cable disconnect.
	 *  c. Source initiated power up sequence returns early because hpd is
	 *     not high.
	 *  d. Sink issues a hpd high attention event.
	 */
	if (dp_display_state_is(DP_STATE_SRC_PWRDN) &&
			dp_display_state_is(DP_STATE_CONFIGURED)) {
		dp_display_host_init(dp);
		dp_display_state_remove(DP_STATE_SRC_PWRDN);
	}

	dp_display_host_ready(dp);

	dp->link->psm_config(dp->link, &dp->panel->link_info, false);
	dp->debug->psm_enabled = false;

	if (!dp->dp_display.base_connector)
		goto end;

	rc = dp->panel->read_sink_caps(dp->panel,
			dp->dp_display.base_connector, dp->hpd->multi_func);
	/*
	 * ETIMEDOUT --> cable may have been removed
	 * ENOTCONN --> no downstream device connected
	 */
	if (rc == -ETIMEDOUT || rc == -ENOTCONN) {
		dp_display_state_remove(DP_STATE_CONNECTED);
		goto end;
	}

	dp->link->process_request(dp->link);
	dp->panel->handle_sink_request(dp->panel);

	dp_display_process_mst_hpd_high(dp, false);

	rc = dp->ctrl->on(dp->ctrl, dp->mst.mst_active,
			dp->panel->fec_en, dp->panel->dsc_en, false);
	if (rc) {
		dp_display_state_remove(DP_STATE_CONNECTED);
		goto end;
	}

	dp->process_hpd_connect = false;

	dp_display_process_mst_hpd_high(dp, true);
end:
	mutex_unlock(&dp->session_lock);

	/*
	 * Delay the HPD connect notification to see if sink generates any
	 * IRQ HPDs immediately after the HPD high.
	 */
	usleep_range(10000, 10100);

	/*
	 * If an IRQ HPD is pending, then do not send a connect notification.
	 * Once this work returns, the IRQ HPD would be processed and any
	 * required actions (such as link maintenance) would be done which
	 * will subsequently send the HPD notification. To keep things simple,
	 * do this only for SST use-cases. MST use cases require additional
	 * care in order to handle the side-band communications as well.
	 *
	 * One of the main motivations for this is DP LL 1.4 CTS use case
	 * where it is possible that we could get a test request right after
	 * a connection, and the strict timing requriements of the test can
	 * only be met if we do not wait for the e2e connection to be set up.
	 */
	if (!dp->mst.mst_active &&
		(work_busy(&dp->attention_work) == WORK_BUSY_PENDING)) {
		SDE_EVT32_EXTERNAL(dp->state, 99);
		DP_DEBUG("Attention pending, skip HPD notification\n");
		goto skip_notify;
	}

	if (!rc && !dp_display_state_is(DP_STATE_ABORTED))
		dp_display_send_hpd_notification(dp);

skip_notify:
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_EXIT, dp->state, rc);
	return rc;
}

static void dp_display_process_mst_hpd_low(struct dp_display_private *dp)
{
	struct dp_mst_hpd_info info = {0};

	if (dp->mst.mst_active) {
		DP_MST_DEBUG("mst_hpd_low work\n");

		if (dp->mst.cbs.hpd) {
			info.mst_protocol = dp->parser->has_mst_sideband;
			dp->mst.cbs.hpd(&dp->dp_display, false, &info);
		}
		dp_display_update_mst_state(dp, false);
	}

	DP_MST_DEBUG("mst_hpd_low. mst_active:%d\n", dp->mst.mst_active);
}

static int dp_display_process_hpd_low(struct dp_display_private *dp)
{
	int rc = 0;

	dp_display_state_remove(DP_STATE_CONNECTED);
	dp->process_hpd_connect = false;
	dp_audio_enable(dp, false);
	dp_display_process_mst_hpd_low(dp);

	if ((dp_display_state_is(DP_STATE_CONNECT_NOTIFIED) ||
			dp_display_state_is(DP_STATE_ENABLED)) &&
			!dp->mst.mst_active)
		rc = dp_display_send_hpd_notification(dp);

	mutex_lock(&dp->session_lock);
	if (!dp->active_stream_cnt)
		dp->ctrl->off(dp->ctrl);
	mutex_unlock(&dp->session_lock);

	dp->panel->video_test = false;

	return rc;
}

static int dp_display_usbpd_configure_cb(struct device *dev)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!dev) {
		DP_ERR("invalid dev\n");
		rc = -EINVAL;
		goto end;
	}

	dp = dev_get_drvdata(dev);
	if (!dp) {
		DP_ERR("no driver data found\n");
		rc = -ENODEV;
		goto end;
	}

	if (!dp->debug->sim_mode && !dp->parser->no_aux_switch
	    && !dp->parser->gpio_aux_switch) {
		rc = dp->aux->aux_switch(dp->aux, true, dp->hpd->orientation);
		if (rc)
			goto end;
	}

	mutex_lock(&dp->session_lock);

	dp_display_state_remove(DP_STATE_ABORTED);
	dp_display_state_add(DP_STATE_CONFIGURED);

	dp_display_host_init(dp);

	/* check for hpd high */
	if (dp->hpd->hpd_high)
		queue_work(dp->wq, &dp->connect_work);
	else
		dp->process_hpd_connect = true;
	mutex_unlock(&dp->session_lock);
end:
	return rc;
}

static int dp_display_stream_pre_disable(struct dp_display_private *dp,
			struct dp_panel *dp_panel)
{
	dp->ctrl->stream_pre_off(dp->ctrl, dp_panel);

	return 0;
}

static void dp_display_stream_disable(struct dp_display_private *dp,
			struct dp_panel *dp_panel)
{
	if (!dp->active_stream_cnt) {
		DP_ERR("invalid active_stream_cnt (%d)\n",
				dp->active_stream_cnt);
		return;
	}

	DP_DEBUG("stream_id=%d, active_stream_cnt=%d\n",
			dp_panel->stream_id, dp->active_stream_cnt);

	dp->ctrl->stream_off(dp->ctrl, dp_panel);
	dp->active_panels[dp_panel->stream_id] = NULL;
	dp->active_stream_cnt--;
}

static void dp_display_clean(struct dp_display_private *dp)
{
	int idx;
	struct dp_panel *dp_panel;
	struct dp_link_hdcp_status *status = &dp->link->hdcp_status;

	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY, dp->state);

	if (dp_display_is_hdcp_enabled(dp) &&
			status->hdcp_state != HDCP_STATE_INACTIVE) {
		cancel_delayed_work_sync(&dp->hdcp_cb_work);
		if (dp->hdcp.ops->off)
			dp->hdcp.ops->off(dp->hdcp.data);

		dp_display_update_hdcp_status(dp, true);
	}

	for (idx = DP_STREAM_0; idx < DP_STREAM_MAX; idx++) {
		if (!dp->active_panels[idx])
			continue;

		dp_panel = dp->active_panels[idx];
		if (dp_panel->audio_supported)
			dp_panel->audio->off(dp_panel->audio);

		dp_display_stream_pre_disable(dp, dp_panel);
		dp_display_stream_disable(dp, dp_panel);
		dp_panel->deinit(dp_panel, 0);
	}

	dp_display_state_remove(DP_STATE_ENABLED | DP_STATE_CONNECTED);

	dp->ctrl->off(dp->ctrl);
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_EXIT, dp->state);
}

static int dp_display_handle_disconnect(struct dp_display_private *dp)
{
	int rc;

	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY, dp->state);
	rc = dp_display_process_hpd_low(dp);
	if (rc) {
		/* cancel any pending request */
		dp->ctrl->abort(dp->ctrl, true);
		dp->aux->abort(dp->aux, true);
	}

	mutex_lock(&dp->session_lock);
	if (rc && dp_display_state_is(DP_STATE_ENABLED))
		dp_display_clean(dp);

	dp_display_host_unready(dp);

	mutex_unlock(&dp->session_lock);

	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_EXIT, dp->state);
	return rc;
}

static void dp_display_disconnect_sync(struct dp_display_private *dp)
{
	/* cancel any pending request */
	dp_display_state_add(DP_STATE_ABORTED);

	dp->ctrl->abort(dp->ctrl, true);
	dp->aux->abort(dp->aux, true);

	/* wait for idle state */
	cancel_work_sync(&dp->connect_work);
	cancel_work_sync(&dp->attention_work);
	flush_workqueue(dp->wq);

	dp_display_handle_disconnect(dp);
}

static int dp_display_usbpd_disconnect_cb(struct device *dev)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!dev) {
		DP_ERR("invalid dev\n");
		rc = -EINVAL;
		goto end;
	}

	dp = dev_get_drvdata(dev);
	if (!dp) {
		DP_ERR("no driver data found\n");
		rc = -ENODEV;
		goto end;
	}

	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY, dp->state,
			dp->debug->psm_enabled);

	if (dp->debug->psm_enabled && dp_display_state_is(DP_STATE_READY))
		dp->link->psm_config(dp->link, &dp->panel->link_info, true);

	dp_display_disconnect_sync(dp);

	mutex_lock(&dp->session_lock);
	dp_display_host_deinit(dp);
	dp_display_state_remove(DP_STATE_CONFIGURED);
	mutex_unlock(&dp->session_lock);

	if (!dp->debug->sim_mode && !dp->parser->no_aux_switch
	    && !dp->parser->gpio_aux_switch)
		dp->aux->aux_switch(dp->aux, false, ORIENTATION_NONE);

	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_EXIT, dp->state);
end:
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_EXIT, dp->state);
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

	DP_DEBUG("dp active_stream_cnt:%d\n", dp->active_stream_cnt);

	return rc;
}

static void dp_display_mst_attention(struct dp_display_private *dp)
{
	struct dp_mst_hpd_info hpd_irq = {0};

	if (dp->mst.mst_active && dp->mst.cbs.hpd_irq) {
		hpd_irq.mst_hpd_sim = dp->debug->mst_hpd_sim;
		hpd_irq.mst_sim_add_con = dp->debug->mst_sim_add_con;
		hpd_irq.mst_sim_remove_con = dp->debug->mst_sim_remove_con;
		hpd_irq.mst_sim_remove_con_id = dp->debug->mst_sim_remove_con_id;
		hpd_irq.edid = dp->debug->get_edid(dp->debug);
		dp->mst.cbs.hpd_irq(&dp->dp_display, &hpd_irq);
		dp->debug->mst_hpd_sim = false;
		dp->debug->mst_sim_add_con = false;
		dp->debug->mst_sim_remove_con = false;
	}

	DP_MST_DEBUG("mst_attention_work. mst_active:%d\n", dp->mst.mst_active);
}

static void dp_display_attention_work(struct work_struct *work)
{
	struct dp_display_private *dp = container_of(work,
			struct dp_display_private, attention_work);

	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY, dp->state);
	mutex_lock(&dp->session_lock);
	SDE_EVT32_EXTERNAL(dp->state);

	if (dp->debug->mst_hpd_sim || !dp_display_state_is(DP_STATE_READY)) {
		mutex_unlock(&dp->session_lock);
		goto mst_attention;
	}

	if (dp->link->process_request(dp->link)) {
		mutex_unlock(&dp->session_lock);
		goto cp_irq;
	}

	mutex_unlock(&dp->session_lock);
	SDE_EVT32_EXTERNAL(dp->state, dp->link->sink_request);

	if (dp->link->sink_request & DS_PORT_STATUS_CHANGED) {
		SDE_EVT32_EXTERNAL(dp->state, DS_PORT_STATUS_CHANGED);
		if (dp_display_is_sink_count_zero(dp)) {
			dp_display_handle_disconnect(dp);
		} else {
			/*
			 * connect work should take care of sending
			 * the HPD notification.
			 */
			if (!dp->mst.mst_active)
				queue_work(dp->wq, &dp->connect_work);
		}

		goto mst_attention;
	}

	if (dp->link->sink_request & DP_TEST_LINK_VIDEO_PATTERN) {
		SDE_EVT32_EXTERNAL(dp->state, DP_TEST_LINK_VIDEO_PATTERN);
		dp_display_handle_disconnect(dp);

		dp->panel->video_test = true;
		/*
		 * connect work should take care of sending
		 * the HPD notification.
		 */
		queue_work(dp->wq, &dp->connect_work);

		goto mst_attention;
	}

	if (dp->link->sink_request & (DP_TEST_LINK_PHY_TEST_PATTERN |
		DP_TEST_LINK_TRAINING | DP_LINK_STATUS_UPDATED)) {

		mutex_lock(&dp->session_lock);
		dp_audio_enable(dp, false);

		if (dp->link->sink_request & DP_TEST_LINK_PHY_TEST_PATTERN) {
			SDE_EVT32_EXTERNAL(dp->state,
					DP_TEST_LINK_PHY_TEST_PATTERN);
			dp->ctrl->process_phy_test_request(dp->ctrl);
		}

		if (dp->link->sink_request & DP_TEST_LINK_TRAINING) {
			SDE_EVT32_EXTERNAL(dp->state, DP_TEST_LINK_TRAINING);
			dp->link->send_test_response(dp->link);
			dp->ctrl->link_maintenance(dp->ctrl);
		}

		if (dp->link->sink_request & DP_LINK_STATUS_UPDATED) {
			SDE_EVT32_EXTERNAL(dp->state, DP_LINK_STATUS_UPDATED);
			dp->ctrl->link_maintenance(dp->ctrl);
		}

		dp_audio_enable(dp, true);
		mutex_unlock(&dp->session_lock);

		if (dp->link->sink_request & (DP_TEST_LINK_PHY_TEST_PATTERN |
			DP_TEST_LINK_TRAINING))
			goto mst_attention;
	}

cp_irq:
	if (dp_display_is_hdcp_enabled(dp) && dp->hdcp.ops->cp_irq)
		dp->hdcp.ops->cp_irq(dp->hdcp.data);

	if (!dp->mst.mst_active) {
		/*
		 * It is possible that the connect_work skipped sending
		 * the HPD notification if the attention message was
		 * already pending. Send the notification here to
		 * account for that. This is not needed if this
		 * attention work was handling a test request
		 */
		dp_display_send_hpd_notification(dp);
	}

mst_attention:
	dp_display_mst_attention(dp);
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_EXIT, dp->state);
}

static int dp_display_usbpd_attention_cb(struct device *dev)
{
	struct dp_display_private *dp;

	if (!dev) {
		DP_ERR("invalid dev\n");
		return -EINVAL;
	}

	dp = dev_get_drvdata(dev);
	if (!dp) {
		DP_ERR("no driver data found\n");
		return -ENODEV;
	}

	DP_DEBUG("hpd_irq:%d, hpd_high:%d, power_on:%d, is_connected:%d\n",
			dp->hpd->hpd_irq, dp->hpd->hpd_high,
			!!dp_display_state_is(DP_STATE_ENABLED),
			!!dp_display_state_is(DP_STATE_CONNECTED));
	SDE_EVT32_EXTERNAL(dp->state, dp->hpd->hpd_irq, dp->hpd->hpd_high,
			!!dp_display_state_is(DP_STATE_ENABLED),
			!!dp_display_state_is(DP_STATE_CONNECTED));

	if (!dp->hpd->hpd_high) {
		dp_display_disconnect_sync(dp);
	} else if ((dp->hpd->hpd_irq && dp_display_state_is(DP_STATE_READY)) ||
			dp->debug->mst_hpd_sim) {
		queue_work(dp->wq, &dp->attention_work);
	} else if (dp->process_hpd_connect ||
			 !dp_display_state_is(DP_STATE_CONNECTED)) {
		dp_display_state_remove(DP_STATE_ABORTED);
		queue_work(dp->wq, &dp->connect_work);
	} else {
		DP_DEBUG("ignored\n");
	}

	return 0;
}

static void dp_display_connect_work(struct work_struct *work)
{
	int rc = 0;
	struct dp_display_private *dp = container_of(work,
			struct dp_display_private, connect_work);

	if (dp_display_state_is(DP_STATE_ABORTED)) {
		DP_WARN("HPD off requested\n");
		return;
	}

	if (!dp->hpd->hpd_high) {
		DP_WARN("Sink disconnected\n");
		return;
	}

	rc = dp_display_process_hpd_high(dp);

	if (!rc && dp->panel->video_test)
		dp->link->send_test_response(dp->link);
}

static int dp_display_usb_notifier(struct notifier_block *nb,
	unsigned long event, void *ptr)
{
	struct extcon_dev *edev = ptr;
	struct dp_display_private *dp = container_of(nb,
			struct dp_display_private, usb_nb);
	if (!edev)
		goto end;

	if (!event && dp->debug->sim_mode) {
		dp_display_disconnect_sync(dp);
		dp->debug->abort(dp->debug);
	}
end:
	return NOTIFY_DONE;
}

static int dp_display_get_usb_extcon(struct dp_display_private *dp)
{
	struct extcon_dev *edev;
	int rc;

	edev = extcon_get_edev_by_phandle(&dp->pdev->dev, 0);
	if (IS_ERR(edev))
		return PTR_ERR(edev);

	dp->usb_nb.notifier_call = dp_display_usb_notifier;
	dp->usb_nb.priority = 2;
	rc = extcon_register_notifier(edev, EXTCON_USB, &dp->usb_nb);
	if (rc)
		DP_ERR("failed to register for usb event: %d\n", rc);

	return rc;
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
	dp_hpd_put(dp->hpd);
	mutex_destroy(&dp->session_lock);
	dp_debug_put(dp->debug);
}

static int dp_init_sub_modules(struct dp_display_private *dp)
{
	int rc = 0;
	bool hdcp_disabled;
	struct device *dev = &dp->pdev->dev;
	struct dp_hpd_cb *cb = &dp->hpd_cb;
	struct dp_ctrl_in ctrl_in = {
		.dev = dev,
	};
	struct dp_panel_in panel_in = {
		.dev = dev,
	};
	struct dp_debug_in debug_in = {
		.dev = dev,
	};

	mutex_init(&dp->session_lock);

	dp->parser = dp_parser_get(dp->pdev);
	if (IS_ERR(dp->parser)) {
		rc = PTR_ERR(dp->parser);
		DP_ERR("failed to initialize parser, rc = %d\n", rc);
		dp->parser = NULL;
		goto error;
	}

	rc = dp->parser->parse(dp->parser);
	if (rc) {
		DP_ERR("device tree parsing failed\n");
		goto error_catalog;
	}

	g_dp_display->is_mst_supported = dp->parser->has_mst;
	g_dp_display->no_mst_encoder = dp->parser->no_mst_encoder;

	dp->catalog = dp_catalog_get(dev, dp->parser);
	if (IS_ERR(dp->catalog)) {
		rc = PTR_ERR(dp->catalog);
		DP_ERR("failed to initialize catalog, rc = %d\n", rc);
		dp->catalog = NULL;
		goto error_catalog;
	}

	dp->power = dp_power_get(dp->parser);
	if (IS_ERR(dp->power)) {
		rc = PTR_ERR(dp->power);
		DP_ERR("failed to initialize power, rc = %d\n", rc);
		dp->power = NULL;
		goto error_power;
	}

	rc = dp->power->power_client_init(dp->power, &dp->priv->phandle,
		dp->dp_display.drm_dev);
	if (rc) {
		DP_ERR("Power client create failed\n");
		goto error_aux;
	}

	dp->aux = dp_aux_get(dev, &dp->catalog->aux, dp->parser,
			dp->aux_switch_node);
	if (IS_ERR(dp->aux)) {
		rc = PTR_ERR(dp->aux);
		DP_ERR("failed to initialize aux, rc = %d\n", rc);
		dp->aux = NULL;
		goto error_aux;
	}

	rc = dp->aux->drm_aux_register(dp->aux);
	if (rc) {
		DP_ERR("DRM DP AUX register failed\n");
		goto error_link;
	}

	dp->link = dp_link_get(dev, dp->aux);
	if (IS_ERR(dp->link)) {
		rc = PTR_ERR(dp->link);
		DP_ERR("failed to initialize link, rc = %d\n", rc);
		dp->link = NULL;
		goto error_link;
	}

	panel_in.aux = dp->aux;
	panel_in.catalog = &dp->catalog->panel;
	panel_in.link = dp->link;
	panel_in.connector = dp->dp_display.base_connector;
	panel_in.base_panel = NULL;
	panel_in.parser = dp->parser;

	dp->panel = dp_panel_get(&panel_in);
	if (IS_ERR(dp->panel)) {
		rc = PTR_ERR(dp->panel);
		DP_ERR("failed to initialize panel, rc = %d\n", rc);
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
		DP_ERR("failed to initialize ctrl, rc = %d\n", rc);
		dp->ctrl = NULL;
		goto error_ctrl;
	}

	dp->panel->audio = dp_audio_get(dp->pdev, dp->panel,
						&dp->catalog->audio);
	if (IS_ERR(dp->panel->audio)) {
		rc = PTR_ERR(dp->panel->audio);
		DP_ERR("failed to initialize audio, rc = %d\n", rc);
		dp->panel->audio = NULL;
		goto error_audio;
	}

	memset(&dp->mst, 0, sizeof(dp->mst));
	dp->active_stream_cnt = 0;

	cb->configure  = dp_display_usbpd_configure_cb;
	cb->disconnect = dp_display_usbpd_disconnect_cb;
	cb->attention  = dp_display_usbpd_attention_cb;

	dp->hpd = dp_hpd_get(dev, dp->parser, &dp->catalog->hpd, cb);
	if (IS_ERR(dp->hpd)) {
		rc = PTR_ERR(dp->hpd);
		DP_ERR("failed to initialize hpd, rc = %d\n", rc);
		dp->hpd = NULL;
		goto error_hpd;
	}

	hdcp_disabled = !!dp_display_initialize_hdcp(dp);

	debug_in.panel = dp->panel;
	debug_in.hpd = dp->hpd;
	debug_in.link = dp->link;
	debug_in.aux = dp->aux;
	debug_in.connector = &dp->dp_display.base_connector;
	debug_in.catalog = dp->catalog;
	debug_in.parser = dp->parser;
	debug_in.ctrl = dp->ctrl;

	dp->debug = dp_debug_get(&debug_in);
	if (IS_ERR(dp->debug)) {
		rc = PTR_ERR(dp->debug);
		DP_ERR("failed to initialize debug, rc = %d\n", rc);
		dp->debug = NULL;
		goto error_debug;
	}

	dp->cached_connector_status = connector_status_disconnected;
	dp->tot_dsc_blks_in_use = 0;

	dp->debug->hdcp_disabled = hdcp_disabled;
	dp_display_update_hdcp_status(dp, true);

	dp_display_get_usb_extcon(dp);

	rc = dp->hpd->register_hpd(dp->hpd);
	if (rc) {
		DP_ERR("failed register hpd\n");
		goto error_hpd_reg;
	}

	return rc;
error_hpd_reg:
	dp_debug_put(dp->debug);
error_debug:
	dp_hpd_put(dp->hpd);
error_hpd:
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
		DP_ERR("invalid input\n");
		rc = -EINVAL;
		goto end;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);
	if (IS_ERR_OR_NULL(dp)) {
		DP_ERR("invalid params\n");
		rc = -EINVAL;
		goto end;
	}

	rc = dp_init_sub_modules(dp);
	if (rc)
		goto end;

	dp_display->post_init = NULL;
end:
	DP_DEBUG("%s\n", rc ? "failed" : "success");
	return rc;
}

static int dp_display_set_mode(struct dp_display *dp_display, void *panel,
		struct dp_display_mode *mode)
{
	const u32 num_components = 3, default_bpp = 24;
	struct dp_display_private *dp;
	struct dp_panel *dp_panel;

	if (!dp_display || !panel) {
		DP_ERR("invalid input\n");
		return -EINVAL;
	}

	dp_panel = panel;
	if (!dp_panel->connector) {
		DP_ERR("invalid connector input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	mutex_lock(&dp->session_lock);

	if (dp_panel->connector->display_info.max_tmds_clock > 0)
		dp->panel->connector->display_info.max_tmds_clock =
			dp_panel->connector->display_info.max_tmds_clock;

	mode->timing.bpp =
		dp_panel->connector->display_info.bpc * num_components;
	if (!mode->timing.bpp)
		mode->timing.bpp = default_bpp;

	mode->timing.bpp = dp->panel->get_mode_bpp(dp->panel,
			mode->timing.bpp, mode->timing.pixel_clk_khz);

	dp_panel->pinfo = mode->timing;
	mutex_unlock(&dp->session_lock);

	return 0;
}

static int dp_display_prepare(struct dp_display *dp_display, void *panel)
{
	struct dp_display_private *dp;
	struct dp_panel *dp_panel;
	int rc = 0;

	if (!dp_display || !panel) {
		DP_ERR("invalid input\n");
		return -EINVAL;
	}

	dp_panel = panel;
	if (!dp_panel->connector) {
		DP_ERR("invalid connector input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY, dp->state);
	mutex_lock(&dp->session_lock);

	/*
	 * If DP video session is restored by the userspace after display
	 * disconnect notification from dongle i.e. typeC cable connected to
	 * source but disconnected at the display side, the DP controller is
	 * not restored to the desired configured state. So, ensure host_init
	 * is executed in such a scenario so that all the DP controller
	 * resources are enabled for the next connection event.
	 */
	if (dp_display_state_is(DP_STATE_SRC_PWRDN) &&
			dp_display_state_is(DP_STATE_CONFIGURED)) {
		dp_display_host_init(dp);
		dp_display_state_remove(DP_STATE_SRC_PWRDN);
	}

	/*
	 * If the physical connection to the sink is already lost by the time
	 * we try to set up the connection, we can just skip all the steps
	 * here safely.
	 */
	if (dp_display_state_is(DP_STATE_ABORTED)) {
		dp_display_state_log("[aborted]");
		goto end;
	}

	/*
	 * If DP_STATE_ENABLED, there is nothing left to do.
	 * However, this should not happen ideally. So, log this.
	 */
	if (dp_display_state_is(DP_STATE_ENABLED)) {
		dp_display_state_show("[already enabled]");
		goto end;
	}

	if (!dp_display_is_ready(dp)) {
		dp_display_state_show("[not ready]");
		goto end;
	}

	/* For supporting DP_PANEL_SRC_INITIATED_POWER_DOWN case */
	dp_display_host_ready(dp);

	if (dp->debug->psm_enabled) {
		dp->link->psm_config(dp->link, &dp->panel->link_info, false);
		dp->debug->psm_enabled = false;
	}

	/*
	 * Execute the dp controller power on in shallow mode here.
	 * In normal cases, controller should have been powered on
	 * by now. In some cases like suspend/resume or framework
	 * reboot, we end up here without a powered on controller.
	 * Cable may have been removed in suspended state. In that
	 * case, link training is bound to fail on system resume.
	 * So, we execute in shallow mode here to do only minimal
	 * and required things.
	 */
	rc = dp->ctrl->on(dp->ctrl, dp->mst.mst_active, dp_panel->fec_en,
			dp_panel->dsc_en, true);
	if (rc)
		goto end;

end:
	mutex_unlock(&dp->session_lock);

	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_EXIT, dp->state);
	return rc;
}

static int dp_display_set_stream_info(struct dp_display *dp_display,
			void *panel, u32 strm_id, u32 start_slot,
			u32 num_slots, u32 pbn, int vcpi)
{
	int rc = 0;
	struct dp_panel *dp_panel;
	struct dp_display_private *dp;
	const int max_slots = 64;

	if (!dp_display) {
		DP_ERR("invalid input\n");
		return -EINVAL;
	}

	if (strm_id >= DP_STREAM_MAX) {
		DP_ERR("invalid stream id:%d\n", strm_id);
		return -EINVAL;
	}

	if (start_slot + num_slots > max_slots) {
		DP_ERR("invalid channel info received. start:%d, slots:%d\n",
				start_slot, num_slots);
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	mutex_lock(&dp->session_lock);

	dp->ctrl->set_mst_channel_info(dp->ctrl, strm_id,
			start_slot, num_slots);

	if (panel) {
		dp_panel = panel;
		dp_panel->set_stream_info(dp_panel, strm_id, start_slot,
				num_slots, pbn, vcpi);
	}

	mutex_unlock(&dp->session_lock);

	return rc;
}

static void dp_display_update_dsc_resources(struct dp_display_private *dp,
		struct dp_panel *panel, bool enable)
{
	u32 dsc_blk_cnt = 0;

	if (panel->pinfo.comp_info.comp_type == MSM_DISPLAY_COMPRESSION_DSC &&
		panel->pinfo.comp_info.comp_ratio) {
		dsc_blk_cnt = panel->pinfo.h_active /
				dp->parser->max_dp_dsc_input_width_pixs;
		if (panel->pinfo.h_active %
				dp->parser->max_dp_dsc_input_width_pixs)
			dsc_blk_cnt++;
	}

	if (enable) {
		dp->tot_dsc_blks_in_use += dsc_blk_cnt;
		panel->tot_dsc_blks_in_use += dsc_blk_cnt;
	} else {
		dp->tot_dsc_blks_in_use -= dsc_blk_cnt;
		panel->tot_dsc_blks_in_use -= dsc_blk_cnt;
	}
}

static int dp_display_enable(struct dp_display *dp_display, void *panel)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!dp_display || !panel) {
		DP_ERR("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY, dp->state);
	mutex_lock(&dp->session_lock);

	/*
	 * If DP_STATE_READY is not set, we should not do any HW
	 * programming.
	 */
	if (!dp_display_state_is(DP_STATE_READY)) {
		dp_display_state_show("[host not ready]");
		goto end;
	}

	/*
	 * It is possible that by the time we get call back to establish
	 * the DP pipeline e2e, the physical DP connection to the sink is
	 * already lost. In such cases, the DP_STATE_ABORTED would be set.
	 * However, it is necessary to NOT abort the display setup here so as
	 * to ensure that the rest of the system is in a stable state prior to
	 * handling the disconnect notification.
	 */
	if (dp_display_state_is(DP_STATE_ABORTED))
		dp_display_state_log("[aborted, but continue on]");

	rc = dp_display_stream_enable(dp, panel);
	if (rc)
		goto end;

	dp_display_update_dsc_resources(dp, panel, true);
	dp_display_state_add(DP_STATE_ENABLED);
end:
	mutex_unlock(&dp->session_lock);
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_EXIT, dp->state);
	return rc;
}

static void dp_display_stream_post_enable(struct dp_display_private *dp,
			struct dp_panel *dp_panel)
{
	dp_panel->spd_config(dp_panel);
	dp_panel->setup_hdr(dp_panel, NULL, false, 0, true);
}

static int dp_display_post_enable(struct dp_display *dp_display, void *panel)
{
	struct dp_display_private *dp;
	struct dp_panel *dp_panel;

	if (!dp_display || !panel) {
		DP_ERR("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);
	dp_panel = panel;

	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY, dp->state);
	mutex_lock(&dp->session_lock);

	/*
	 * If DP_STATE_READY is not set, we should not do any HW
	 * programming.
	 */
	if (!dp_display_state_is(DP_STATE_ENABLED)) {
		dp_display_state_show("[not enabled]");
		goto end;
	}

	/*
	 * If the physical connection to the sink is already lost by the time
	 * we try to set up the connection, we can just skip all the steps
	 * here safely.
	 */
	if (dp_display_state_is(DP_STATE_ABORTED)) {
		dp_display_state_log("[aborted]");
		goto end;
	}

	if (!dp_display_is_ready(dp) || !dp_display_state_is(DP_STATE_READY)) {
		dp_display_state_show("[not ready]");
		goto end;
	}

	dp_display_stream_post_enable(dp, dp_panel);

	cancel_delayed_work_sync(&dp->hdcp_cb_work);
	queue_delayed_work(dp->wq, &dp->hdcp_cb_work, HZ);

	if (dp_panel->audio_supported) {
		dp_panel->audio->bw_code = dp->link->link_params.bw_code;
		dp_panel->audio->lane_count = dp->link->link_params.lane_count;
		dp_panel->audio->on(dp_panel->audio);
	}
end:
	dp->aux->state |= DP_STATE_CTRL_POWERED_ON;

	complete_all(&dp->notification_comp);
	mutex_unlock(&dp->session_lock);
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_EXIT, dp->state);
	return 0;
}

static void dp_display_clear_colorspaces(struct dp_display *dp_display)
{
	struct drm_connector *connector;

	connector = dp_display->base_connector;
	connector->color_enc_fmt = 0;
}

static int dp_display_pre_disable(struct dp_display *dp_display, void *panel)
{
	struct dp_display_private *dp;
	struct dp_panel *dp_panel = panel;
	struct dp_link_hdcp_status *status;
	int rc = 0;
	size_t i;

	if (!dp_display || !panel) {
		DP_ERR("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY, dp->state);
	mutex_lock(&dp->session_lock);

	status = &dp->link->hdcp_status;

	if (!dp_display_state_is(DP_STATE_ENABLED)) {
		dp_display_state_show("[not enabled]");
		goto end;
	}

	dp_display_state_add(DP_STATE_HDCP_ABORTED);
	cancel_delayed_work_sync(&dp->hdcp_cb_work);
	if (dp_display_is_hdcp_enabled(dp) &&
			status->hdcp_state != HDCP_STATE_INACTIVE) {
		bool off = true;

		if (dp_display_state_is(DP_STATE_SUSPENDED)) {
			DP_DEBUG("Can't perform HDCP cleanup while suspended. Defer\n");
			dp->hdcp_delayed_off = true;
			goto clean;
		}

		flush_delayed_work(&dp->hdcp_cb_work);
		if (dp->mst.mst_active) {
			dp_display_hdcp_deregister_stream(dp,
				dp_panel->stream_id);
			for (i = DP_STREAM_0; i < DP_STREAM_MAX; i++) {
				if (i != dp_panel->stream_id &&
						dp->active_panels[i]) {
					DP_DEBUG("Streams are still active. Skip disabling HDCP\n");
					off = false;
				}
			}
		}

		if (off) {
			if (dp->hdcp.ops->off)
				dp->hdcp.ops->off(dp->hdcp.data);
			dp_display_update_hdcp_status(dp, true);
		}
	}

	dp_display_clear_colorspaces(dp_display);

clean:
	if (dp_panel->audio_supported)
		dp_panel->audio->off(dp_panel->audio);

	rc = dp_display_stream_pre_disable(dp, dp_panel);

end:
	mutex_unlock(&dp->session_lock);
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_EXIT, dp->state);
	return 0;
}

static int dp_display_disable(struct dp_display *dp_display, void *panel)
{
	int i;
	struct dp_display_private *dp = NULL;
	struct dp_panel *dp_panel = NULL;
	struct dp_link_hdcp_status *status;

	if (!dp_display || !panel) {
		DP_ERR("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);
	dp_panel = panel;
	status = &dp->link->hdcp_status;

	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY, dp->state);
	mutex_lock(&dp->session_lock);

	if (!dp_display_state_is(DP_STATE_ENABLED)) {
		dp_display_state_show("[not enabled]");
		goto end;
	}

	if (!dp_display_state_is(DP_STATE_READY)) {
		dp_display_state_show("[not ready]");
		goto end;
	}

	dp_display_stream_disable(dp, dp_panel);
	dp_display_update_dsc_resources(dp, dp_panel, false);

	dp_display_state_remove(DP_STATE_HDCP_ABORTED);
	for (i = DP_STREAM_0; i < DP_STREAM_MAX; i++) {
		if (dp->active_panels[i]) {
			if (status->hdcp_state != HDCP_STATE_AUTHENTICATED)
				queue_delayed_work(dp->wq, &dp->hdcp_cb_work,
						HZ/4);
			break;
		}
	}
end:
	mutex_unlock(&dp->session_lock);
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_EXIT, dp->state);
	return 0;
}

static int dp_request_irq(struct dp_display *dp_display)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!dp_display) {
		DP_ERR("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	dp->irq = irq_of_parse_and_map(dp->pdev->dev.of_node, 0);
	if (dp->irq < 0) {
		rc = dp->irq;
		DP_ERR("failed to get irq: %d\n", rc);
		return rc;
	}

	rc = devm_request_irq(&dp->pdev->dev, dp->irq, dp_display_irq,
		IRQF_TRIGGER_HIGH, "dp_display_isr", dp);
	if (rc < 0) {
		DP_ERR("failed to request IRQ%u: %d\n",
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
		DP_ERR("invalid input\n");
		return ERR_PTR(-EINVAL);
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	return dp->debug;
}

static int dp_display_unprepare(struct dp_display *dp_display, void *panel)
{
	struct dp_display_private *dp;
	struct dp_panel *dp_panel = panel;
	u32 flags = 0;

	if (!dp_display || !panel) {
		DP_ERR("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY, dp->state);
	mutex_lock(&dp->session_lock);

	/*
	 * Check if the power off sequence was triggered
	 * by a source initialated action like framework
	 * reboot or suspend-resume but not from normal
	 * hot plug. If connector is in MST mode, skip
	 * powering down host as aux needs to be kept
	 * alive to handle hot-plug sideband message.
	 */
	if (dp_display_is_ready(dp) &&
		(dp_display_state_is(DP_STATE_SUSPENDED) ||
		!dp->mst.mst_active))
		flags |= DP_PANEL_SRC_INITIATED_POWER_DOWN;

	if (dp->active_stream_cnt)
		goto end;

	if (flags & DP_PANEL_SRC_INITIATED_POWER_DOWN) {
		dp->link->psm_config(dp->link, &dp->panel->link_info, true);
		dp->debug->psm_enabled = true;

		dp->ctrl->off(dp->ctrl);
		dp_display_host_unready(dp);
		dp_display_host_deinit(dp);
		dp_display_state_add(DP_STATE_SRC_PWRDN);
	}

	dp_display_state_remove(DP_STATE_ENABLED);
	dp->aux->state = DP_STATE_CTRL_POWERED_OFF;

	complete_all(&dp->notification_comp);

	/* log this as it results from user action of cable dis-connection */
	DP_INFO("[OK]\n");
end:
	dp_panel->deinit(dp_panel, flags);
	mutex_unlock(&dp->session_lock);
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_EXIT, dp->state);

	return 0;
}

static enum drm_mode_status dp_display_validate_mode(
		struct dp_display *dp_display,
		void *panel, struct drm_display_mode *mode,
		const struct msm_resource_caps_info *avail_res)
{
	struct dp_display_private *dp;
	struct drm_dp_link *link_info;
	u32 mode_rate_khz = 0, supported_rate_khz = 0, mode_bpp = 0;
	struct dp_panel *dp_panel;
	struct dp_debug *debug;
	enum drm_mode_status mode_status = MODE_BAD;
	bool in_list = false;
	struct dp_mst_connector *mst_connector;
	int hdis, vdis, vref, ar, _hdis, _vdis, _vref, _ar, rate;
	struct dp_display_mode dp_mode;
	bool dsc_en;
	u32 num_lm = 0;
	int rc = 0, tmds_max_clock = 0;

	if (!dp_display || !mode || !panel ||
			!avail_res || !avail_res->max_mixer_width) {
		DP_ERR("invalid params\n");
		return mode_status;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	mutex_lock(&dp->session_lock);

	dp_panel = panel;
	if (!dp_panel->connector) {
		DP_ERR("invalid connector\n");
		goto end;
	}

	link_info = &dp->panel->link_info;

	debug = dp->debug;
	if (!debug)
		goto end;

	dp_display->convert_to_dp_mode(dp_display, panel, mode, &dp_mode);

	dsc_en = dp_mode.timing.comp_info.comp_ratio ? true : false;
	mode_bpp = dsc_en ? dp_mode.timing.comp_info.dsc_info.bpp :
			dp_mode.timing.bpp;

	mode_rate_khz = mode->clock * mode_bpp;
	rate = drm_dp_bw_code_to_link_rate(dp->link->link_params.bw_code);
	supported_rate_khz = link_info->num_lanes * rate * 8;
	tmds_max_clock = dp_panel->connector->display_info.max_tmds_clock;

	if (mode_rate_khz > supported_rate_khz) {
		DP_MST_DEBUG("pclk:%d, supported_rate:%d\n",
				mode->clock, supported_rate_khz);
		goto end;
	}

	if (mode->clock > dp_display->max_pclk_khz) {
		DP_MST_DEBUG("clk:%d, max:%d\n", mode->clock,
				dp_display->max_pclk_khz);
		goto end;
	}

	if (tmds_max_clock > 0 && mode->clock > tmds_max_clock) {
		DP_MST_DEBUG("clk:%d, max tmds:%d\n", mode->clock,
				tmds_max_clock);
		goto end;
	}

	rc = msm_get_mixer_count(dp->priv, mode, avail_res, &num_lm);
	if (rc) {
		DP_ERR("error getting mixer count. rc:%d\n", rc);
		goto end;
	}

	if (num_lm > avail_res->num_lm ||
			(num_lm == 2 && !avail_res->num_3dmux)) {
		DP_MST_DEBUG("num_lm:%d, req lm:%d 3dmux:%d\n", num_lm,
				avail_res->num_lm, avail_res->num_3dmux);
		goto end;
	}

	/*
	 * If the connector exists in the mst connector list and if debug is
	 * enabled for that connector, use the mst connector settings from the
	 * list for validation. Otherwise, use non-mst default settings.
	 */
	mutex_lock(&debug->dp_mst_connector_list.lock);

	if (list_empty(&debug->dp_mst_connector_list.list)) {
		mutex_unlock(&debug->dp_mst_connector_list.lock);
		goto verify_default;
	}

	list_for_each_entry(mst_connector, &debug->dp_mst_connector_list.list,
			list) {
		if (mst_connector->con_id == dp_panel->connector->base.id) {
			in_list = true;

			if (!mst_connector->debug_en) {
				mode_status = MODE_OK;
				mutex_unlock(
				&debug->dp_mst_connector_list.lock);
				goto end;
			}

			hdis = mst_connector->hdisplay;
			vdis = mst_connector->vdisplay;
			vref = mst_connector->vrefresh;
			ar = mst_connector->aspect_ratio;

			_hdis = mode->hdisplay;
			_vdis = mode->vdisplay;
			_vref = mode->vrefresh;
			_ar = mode->picture_aspect_ratio;

			if (hdis == _hdis && vdis == _vdis && vref == _vref &&
					ar == _ar) {
				mode_status = MODE_OK;
				mutex_unlock(
				&debug->dp_mst_connector_list.lock);
				goto end;
			}

			break;
		}
	}

	mutex_unlock(&debug->dp_mst_connector_list.lock);

	if (in_list)
		goto end;

verify_default:
	if (debug->debug_en && (mode->hdisplay != debug->hdisplay ||
			mode->vdisplay != debug->vdisplay ||
			mode->vrefresh != debug->vrefresh ||
			mode->picture_aspect_ratio != debug->aspect_ratio))
		goto end;

	mode_status = MODE_OK;
end:
	mutex_unlock(&dp->session_lock);
	return mode_status;
}

static int dp_display_get_modes(struct dp_display *dp, void *panel,
	struct dp_display_mode *dp_mode)
{
	struct dp_display_private *dp_display;
	struct dp_panel *dp_panel;
	int ret = 0;

	if (!dp || !panel) {
		DP_ERR("invalid params\n");
		return 0;
	}

	dp_panel = panel;
	if (!dp_panel->connector) {
		DP_ERR("invalid connector\n");
		return 0;
	}

	dp_display = container_of(dp, struct dp_display_private, dp_display);

	ret = dp_panel->get_modes(dp_panel, dp_panel->connector, dp_mode);
	if (dp_mode->timing.pixel_clk_khz)
		dp->max_pclk_khz = dp_mode->timing.pixel_clk_khz;
	return ret;
}

static void dp_display_convert_to_dp_mode(struct dp_display *dp_display,
		void *panel,
		const struct drm_display_mode *drm_mode,
		struct dp_display_mode *dp_mode)
{
	struct dp_display_private *dp;
	struct dp_panel *dp_panel;
	u32 free_dsc_blks = 0, required_dsc_blks = 0;

	if (!dp_display || !drm_mode || !dp_mode || !panel) {
		DP_ERR("invalid input\n");
		return;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);
	dp_panel = panel;

	memset(dp_mode, 0, sizeof(*dp_mode));

	free_dsc_blks = dp->parser->max_dp_dsc_blks -
				dp->tot_dsc_blks_in_use +
				dp_panel->tot_dsc_blks_in_use;
	required_dsc_blks = drm_mode->hdisplay /
				dp->parser->max_dp_dsc_input_width_pixs;
	if (drm_mode->hdisplay % dp->parser->max_dp_dsc_input_width_pixs)
		required_dsc_blks++;

	if (free_dsc_blks >= required_dsc_blks)
		dp_mode->capabilities |= DP_PANEL_CAPS_DSC;

	if (dp_mode->capabilities & DP_PANEL_CAPS_DSC)
		DP_DEBUG("in_use:%d, max:%d, free:%d, req:%d, caps:0x%x, width:%d\n",
			dp->tot_dsc_blks_in_use, dp->parser->max_dp_dsc_blks,
			free_dsc_blks, required_dsc_blks, dp_mode->capabilities,
			dp->parser->max_dp_dsc_input_width_pixs);

	dp_panel->convert_to_dp_mode(dp_panel, drm_mode, dp_mode);
}

static int dp_display_config_hdr(struct dp_display *dp_display, void *panel,
			struct drm_msm_ext_hdr_metadata *hdr, bool dhdr_update)
{
	struct dp_panel *dp_panel;
	struct sde_connector *sde_conn;
	struct dp_display_private *dp;
	u64 core_clk_rate;
	bool flush_hdr;

	if (!dp_display || !panel) {
		DP_ERR("invalid input\n");
		return -EINVAL;
	}

	dp_panel = panel;
	dp = container_of(dp_display, struct dp_display_private, dp_display);
	sde_conn =  to_sde_connector(dp_panel->connector);

	core_clk_rate = dp->power->clk_get_rate(dp->power, "core_clk");
	if (!core_clk_rate) {
		DP_ERR("invalid rate for core_clk\n");
		return -EINVAL;
	}

	if (!dp_display_state_is(DP_STATE_ENABLED)) {
		dp_display_state_show("[not enabled]");
		return 0;
	}

	/*
	 * In rare cases where HDR metadata is updated independently
	 * flush the HDR metadata immediately instead of relying on
	 * the colorspace
	 */
	flush_hdr = !sde_conn->colorspace_updated;

	if (flush_hdr)
		DP_DEBUG("flushing the HDR metadata\n");
	else
		DP_DEBUG("piggy-backing with colorspace\n");

	return dp_panel->setup_hdr(dp_panel, hdr, dhdr_update,
		core_clk_rate, flush_hdr);
}

static int dp_display_setup_colospace(struct dp_display *dp_display,
		void *panel,
		u32 colorspace)
{
	struct dp_panel *dp_panel;
	struct dp_display_private *dp;

	if (!dp_display || !panel) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	if (!dp_display_state_is(DP_STATE_ENABLED)) {
		dp_display_state_show("[not enabled]");
		return 0;
	}

	dp_panel = panel;

	return dp_panel->set_colorspace(dp_panel, colorspace);
}

static int dp_display_create_workqueue(struct dp_display_private *dp)
{
	dp->wq = create_singlethread_workqueue("drm_dp");
	if (IS_ERR_OR_NULL(dp->wq)) {
		DP_ERR("Error creating wq\n");
		return -EPERM;
	}

	INIT_DELAYED_WORK(&dp->hdcp_cb_work, dp_display_hdcp_cb_work);
	INIT_WORK(&dp->connect_work, dp_display_connect_work);
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
		DP_ERR("cannot find dev.of_node\n");
		rc = -ENODEV;
		goto end;
	}

	dp->aux_switch_node = of_parse_phandle(dp->pdev->dev.of_node,
			phandle, 0);
	if (!dp->aux_switch_node) {
		DP_WARN("cannot parse %s handle\n", phandle);
		rc = -ENODEV;
		goto end;
	}

	nb.notifier_call = dp_display_fsa4480_callback;
	nb.priority = 0;

	rc = fsa4480_reg_notifier(&nb, dp->aux_switch_node);
	if (rc) {
		DP_ERR("failed to register notifier (%d)\n", rc);
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
		DP_ERR("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	if (!mst_install_info->cbs->hpd || !mst_install_info->cbs->hpd_irq) {
		DP_ERR("invalid mst cbs\n");
		return -EINVAL;
	}

	dp_display->dp_mst_prv_info = mst_install_info->dp_mst_prv_info;

	if (!dp->parser->has_mst) {
		DP_DEBUG("mst not enabled\n");
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
		DP_ERR("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	if (!dp->mst.drm_registered) {
		DP_DEBUG("drm mst not registered\n");
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
	struct dp_mst_connector *mst_connector;

	if (!dp_display || !connector) {
		DP_ERR("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	mutex_lock(&dp->session_lock);

	if (!dp->mst.drm_registered) {
		DP_DEBUG("drm mst not registered\n");
		mutex_unlock(&dp->session_lock);
		return -EPERM;
	}

	panel_in.dev = &dp->pdev->dev;
	panel_in.aux = dp->aux;
	panel_in.catalog = &dp->catalog->panel;
	panel_in.link = dp->link;
	panel_in.connector = connector;
	panel_in.base_panel = dp->panel;
	panel_in.parser = dp->parser;

	dp_panel = dp_panel_get(&panel_in);
	if (IS_ERR(dp_panel)) {
		rc = PTR_ERR(dp_panel);
		DP_ERR("failed to initialize panel, rc = %d\n", rc);
		mutex_unlock(&dp->session_lock);
		return rc;
	}

	dp_panel->audio = dp_audio_get(dp->pdev, dp_panel, &dp->catalog->audio);
	if (IS_ERR(dp_panel->audio)) {
		rc = PTR_ERR(dp_panel->audio);
		DP_ERR("[mst] failed to initialize audio, rc = %d\n", rc);
		dp_panel->audio = NULL;
		mutex_unlock(&dp->session_lock);
		return rc;
	}

	DP_MST_DEBUG("dp mst connector installed. conn:%d\n",
			connector->base.id);

	mutex_lock(&dp->debug->dp_mst_connector_list.lock);

	mst_connector = kmalloc(sizeof(struct dp_mst_connector),
			GFP_KERNEL);
	if (!mst_connector) {
		mutex_unlock(&dp->debug->dp_mst_connector_list.lock);
		mutex_unlock(&dp->session_lock);
		return -ENOMEM;
	}

	mst_connector->debug_en = false;
	mst_connector->conn = connector;
	mst_connector->con_id = connector->base.id;
	mst_connector->state = connector_status_unknown;
	INIT_LIST_HEAD(&mst_connector->list);

	list_add(&mst_connector->list,
			&dp->debug->dp_mst_connector_list.list);

	mutex_unlock(&dp->debug->dp_mst_connector_list.lock);
	mutex_unlock(&dp->session_lock);

	return 0;
}

static int dp_display_mst_connector_uninstall(struct dp_display *dp_display,
			struct drm_connector *connector)
{
	int rc = 0;
	struct sde_connector *sde_conn;
	struct dp_panel *dp_panel;
	struct dp_display_private *dp;
	struct dp_mst_connector *con_to_remove, *temp_con;

	if (!dp_display || !connector) {
		DP_ERR("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	mutex_lock(&dp->session_lock);

	if (!dp->mst.drm_registered) {
		DP_DEBUG("drm mst not registered\n");
		mutex_unlock(&dp->session_lock);
		return -EPERM;
	}

	sde_conn = to_sde_connector(connector);
	if (!sde_conn->drv_panel) {
		DP_ERR("invalid panel for connector:%d\n", connector->base.id);
		mutex_unlock(&dp->session_lock);
		return -EINVAL;
	}

	dp_panel = sde_conn->drv_panel;
	dp_audio_put(dp_panel->audio);
	dp_panel_put(dp_panel);

	DP_MST_DEBUG("dp mst connector uninstalled. conn:%d\n",
			connector->base.id);

	mutex_lock(&dp->debug->dp_mst_connector_list.lock);

	list_for_each_entry_safe(con_to_remove, temp_con,
			&dp->debug->dp_mst_connector_list.list, list) {
		if (con_to_remove->conn == connector) {
			list_del(&con_to_remove->list);
			kfree(con_to_remove);
		}
	}

	mutex_unlock(&dp->debug->dp_mst_connector_list.lock);
	mutex_unlock(&dp->session_lock);

	return rc;
}

static int dp_display_mst_get_connector_info(struct dp_display *dp_display,
			struct drm_connector *connector,
			struct dp_mst_connector *mst_conn)
{
	struct dp_display_private *dp;
	struct dp_mst_connector *conn, *temp_conn;

	if (!connector || !mst_conn) {
		DP_ERR("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	mutex_lock(&dp->session_lock);
	if (!dp->mst.drm_registered) {
		DP_DEBUG("drm mst not registered\n");
		mutex_unlock(&dp->session_lock);
		return -EPERM;
	}

	mutex_lock(&dp->debug->dp_mst_connector_list.lock);
	list_for_each_entry_safe(conn, temp_conn,
			&dp->debug->dp_mst_connector_list.list, list) {
		if (conn->con_id == connector->base.id)
			memcpy(mst_conn, conn, sizeof(*mst_conn));
	}
	mutex_unlock(&dp->debug->dp_mst_connector_list.lock);
	mutex_unlock(&dp->session_lock);
	return 0;
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
		DP_ERR("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	if (!dp->mst.drm_registered) {
		DP_DEBUG("drm mst not registered\n");
		return -EPERM;
	}

	sde_conn = to_sde_connector(connector);
	if (!sde_conn->drv_panel) {
		DP_ERR("invalid panel for connector:%d\n", connector->base.id);
		return -EINVAL;
	}

	dp_panel = sde_conn->drv_panel;
	rc = dp_panel->update_edid(dp_panel, edid);

	DP_MST_DEBUG("dp mst connector:%d edid updated. mode_cnt:%d\n",
			connector->base.id, rc);

	return rc;
}

static int dp_display_update_pps(struct dp_display *dp_display,
		struct drm_connector *connector, char *pps_cmd)
{
	struct sde_connector *sde_conn;
	struct dp_panel *dp_panel;
	struct dp_display_private *dp;

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	sde_conn = to_sde_connector(connector);
	if (!sde_conn->drv_panel) {
		DP_ERR("invalid panel for connector:%d\n", connector->base.id);
		return -EINVAL;
	}

	if (!dp_display_state_is(DP_STATE_ENABLED)) {
		dp_display_state_show("[not enabled]");
		return 0;
	}

	dp_panel = sde_conn->drv_panel;
	dp_panel->update_pps(dp_panel, pps_cmd);
	return 0;
}

static int dp_display_mst_connector_update_link_info(
			struct dp_display *dp_display,
			struct drm_connector *connector)
{
	int rc = 0;
	struct sde_connector *sde_conn;
	struct dp_panel *dp_panel;
	struct dp_display_private *dp;

	if (!dp_display || !connector) {
		DP_ERR("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	if (!dp->mst.drm_registered) {
		DP_DEBUG("drm mst not registered\n");
		return -EPERM;
	}

	sde_conn = to_sde_connector(connector);
	if (!sde_conn->drv_panel) {
		DP_ERR("invalid panel for connector:%d\n", connector->base.id);
		return -EINVAL;
	}

	dp_panel = sde_conn->drv_panel;

	memcpy(dp_panel->dpcd, dp->panel->dpcd,
			DP_RECEIVER_CAP_SIZE + 1);
	memcpy(dp_panel->dsc_dpcd, dp->panel->dsc_dpcd,
			DP_RECEIVER_DSC_CAP_SIZE + 1);
	memcpy(&dp_panel->link_info, &dp->panel->link_info,
			sizeof(dp_panel->link_info));

	DP_MST_DEBUG("dp mst connector:%d link info updated\n",
		connector->base.id);

	return rc;
}

static int dp_display_mst_get_fixed_topology_port(
			struct dp_display *dp_display,
			u32 strm_id, u32 *port_num)
{
	struct dp_display_private *dp;
	u32 port;

	if (!dp_display) {
		DP_ERR("invalid input\n");
		return -EINVAL;
	}

	if (strm_id >= DP_STREAM_MAX) {
		DP_ERR("invalid stream id:%d\n", strm_id);
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	port = dp->parser->mst_fixed_port[strm_id];

	if (!port || port > 255)
		return -ENOENT;

	if (port_num)
		*port_num = port;

	return 0;
}

static int dp_display_get_mst_caps(struct dp_display *dp_display,
			struct dp_mst_caps *mst_caps)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!dp_display || !mst_caps) {
		DP_ERR("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	mst_caps->has_mst = dp->parser->has_mst;
	mst_caps->max_streams_supported = (mst_caps->has_mst) ? 2 : 0;
	mst_caps->max_dpcd_transaction_bytes = (mst_caps->has_mst) ? 16 : 0;
	mst_caps->drm_aux = dp->aux->drm_aux;

	return rc;
}

static void dp_display_wakeup_phy_layer(struct dp_display *dp_display,
		bool wakeup)
{
	struct dp_display_private *dp;
	struct dp_hpd *hpd;

	if (!dp_display) {
		DP_ERR("invalid input\n");
		return;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);
	if (!dp->mst.drm_registered) {
		DP_DEBUG("drm mst not registered\n");
		return;
	}

	hpd = dp->hpd;
	if (hpd && hpd->wakeup_phy)
		hpd->wakeup_phy(hpd, wakeup);
}

static int dp_display_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!pdev || !pdev->dev.of_node) {
		DP_ERR("pdev not found\n");
		rc = -ENODEV;
		goto bail;
	}

	dp = devm_kzalloc(&pdev->dev, sizeof(*dp), GFP_KERNEL);
	if (!dp) {
		rc = -ENOMEM;
		goto bail;
	}

	init_completion(&dp->notification_comp);

	dp->pdev = pdev;
	dp->name = "drm_dp";

	memset(&dp->mst, 0, sizeof(dp->mst));

	rc = dp_display_init_aux_switch(dp);
	if (rc) {
		rc = -EPROBE_DEFER;
		goto error;
	}

	rc = dp_display_create_workqueue(dp);
	if (rc) {
		DP_ERR("Failed to create workqueue\n");
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
	g_dp_display->post_open     = NULL;
	g_dp_display->post_init     = dp_display_post_init;
	g_dp_display->config_hdr    = dp_display_config_hdr;
	g_dp_display->mst_install   = dp_display_mst_install;
	g_dp_display->mst_uninstall = dp_display_mst_uninstall;
	g_dp_display->mst_connector_install = dp_display_mst_connector_install;
	g_dp_display->mst_connector_uninstall =
					dp_display_mst_connector_uninstall;
	g_dp_display->mst_connector_update_edid =
					dp_display_mst_connector_update_edid;
	g_dp_display->mst_connector_update_link_info =
				dp_display_mst_connector_update_link_info;
	g_dp_display->get_mst_caps = dp_display_get_mst_caps;
	g_dp_display->set_stream_info = dp_display_set_stream_info;
	g_dp_display->update_pps = dp_display_update_pps;
	g_dp_display->convert_to_dp_mode = dp_display_convert_to_dp_mode;
	g_dp_display->mst_get_connector_info =
					dp_display_mst_get_connector_info;
	g_dp_display->mst_get_fixed_topology_port =
					dp_display_mst_get_fixed_topology_port;
	g_dp_display->wakeup_phy_layer =
					dp_display_wakeup_phy_layer;
	g_dp_display->set_colorspace = dp_display_setup_colospace;

	rc = component_add(&pdev->dev, &dp_display_comp_ops);
	if (rc) {
		DP_ERR("component add failed, rc=%d\n", rc);
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
		DP_ERR("invalid data\n");
		return -EINVAL;
	}

	if (count != 1) {
		DP_ERR("invalid number of displays\n");
		return -EINVAL;
	}

	displays[0] = g_dp_display;
	return count;
}

int dp_display_get_num_of_displays(void)
{
	if (!g_dp_display)
		return 0;

	return 1;
}

int dp_display_get_num_of_streams(void)
{
	if (g_dp_display->no_mst_encoder)
		return 0;

	return DP_STREAM_MAX;
}

static void dp_display_set_mst_state(void *dp_display,
		enum dp_drv_state mst_state)
{
	struct dp_display_private *dp;

	if (!g_dp_display) {
		DP_DEBUG("dp display not initialized\n");
		return;
	}

	dp = container_of(g_dp_display, struct dp_display_private, dp_display);
	if (dp->mst.mst_active && dp->mst.cbs.set_drv_state)
		dp->mst.cbs.set_drv_state(g_dp_display, mst_state);
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

static int dp_pm_prepare(struct device *dev)
{
	struct dp_display_private *dp = container_of(g_dp_display,
			struct dp_display_private, dp_display);

	mutex_lock(&dp->session_lock);
	dp_display_set_mst_state(g_dp_display, PM_SUSPEND);

	/*
	 * There are a few instances where the DP is hotplugged when the device
	 * is in PM suspend state. After hotplug, it is observed the device
	 * enters and exits the PM suspend multiple times while aux transactions
	 * are taking place. This may sometimes cause an unclocked register
	 * access error. So, abort aux transactions when such a situation
	 * arises i.e. when DP is connected but display not enabled yet.
	 */
	if (dp_display_state_is(DP_STATE_CONNECTED) &&
			!dp_display_state_is(DP_STATE_ENABLED)) {
		dp->aux->abort(dp->aux, true);
		dp->ctrl->abort(dp->ctrl, true);
	}

	dp_display_state_add(DP_STATE_SUSPENDED);
	mutex_unlock(&dp->session_lock);

	return 0;
}

static void dp_pm_complete(struct device *dev)
{
	struct dp_display_private *dp = container_of(g_dp_display,
			struct dp_display_private, dp_display);

	mutex_lock(&dp->session_lock);
	dp_display_set_mst_state(g_dp_display, PM_DEFAULT);

	/*
	 * There are multiple PM suspend entry and exits observed before
	 * the connect uevent is issued to userspace. The aux transactions are
	 * aborted during PM suspend entry in dp_pm_prepare to prevent unclocked
	 * register access. On PM suspend exit, there will be no host_init call
	 * to reset the abort flags for ctrl and aux incase DP is connected
	 * but display not enabled. So, resetting abort flags for aux and ctrl.
	 */
	if (dp_display_state_is(DP_STATE_CONNECTED) &&
			!dp_display_state_is(DP_STATE_ENABLED)) {
		dp->aux->abort(dp->aux, false);
		dp->ctrl->abort(dp->ctrl, false);
	}

	dp_display_state_remove(DP_STATE_SUSPENDED);
	mutex_unlock(&dp->session_lock);
}

static const struct dev_pm_ops dp_pm_ops = {
	.prepare = dp_pm_prepare,
	.complete = dp_pm_complete,
};

static struct platform_driver dp_display_driver = {
	.probe  = dp_display_probe,
	.remove = dp_display_remove,
	.driver = {
		.name = "msm-dp-display",
		.of_match_table = dp_dt_match,
		.suppress_bind_attrs = true,
		.pm = &dp_pm_ops,
	},
};

static int __init dp_display_init(void)
{
	int ret;

	ret = platform_driver_register(&dp_display_driver);
	if (ret) {
		DP_ERR("driver register failed\n");
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
