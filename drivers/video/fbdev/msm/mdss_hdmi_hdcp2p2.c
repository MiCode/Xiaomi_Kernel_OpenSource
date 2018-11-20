/* Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/types.h>
#include <linux/kthread.h>

#include <linux/hdcp_qseecom.h>
#include "mdss_hdcp_2x.h"
#include "video/msm_hdmi_hdcp_mgr.h"
#include "mdss_hdmi_util.h"

/*
 * Defined addresses and offsets of standard HDCP 2.2 sink registers
 * for DDC, as defined in HDCP 2.2 spec section 2.14 table 2.7
 */
#define HDCP_SINK_DDC_SLAVE_ADDR 0x74            /* Sink DDC slave address */
#define HDCP_SINK_DDC_HDCP2_VERSION 0x50         /* Does sink support HDCP2.2 */
#define HDCP_SINK_DDC_HDCP2_WRITE_MESSAGE 0x60   /* HDCP Tx writes here */
#define HDCP_SINK_DDC_HDCP2_RXSTATUS 0x70        /* RxStatus, 2 bytes */
#define HDCP_SINK_DDC_HDCP2_READ_MESSAGE 0x80    /* HDCP Tx reads here */

#define HDCP2P2_DEFAULT_TIMEOUT 500

/*
 * HDCP 2.2 encryption requires the data encryption block that is present in
 * HDMI controller version 4.0.0 and above
 */
#define MIN_HDMI_TX_MAJOR_VERSION 4

enum hdmi_hdcp2p2_sink_status {
	SINK_DISCONNECTED,
	SINK_CONNECTED
};

enum hdmi_auth_status {
	HDMI_HDCP_AUTH_STATUS_FAILURE,
	HDMI_HDCP_AUTH_STATUS_SUCCESS
};

struct hdmi_hdcp2p2_ctrl {
	atomic_t auth_state;
	bool tethered;
	enum hdmi_hdcp2p2_sink_status sink_status; /* Is sink connected */
	struct hdcp_init_data init_data; /* Feature data from HDMI drv */
	struct mutex mutex; /* mutex to protect access to ctrl */
	struct mutex msg_lock; /* mutex to protect access to msg buffer */
	struct mutex wakeup_mutex; /* mutex to protect access to wakeup call*/
	struct hdcp_ops *ops;
	void *lib_ctx; /* Handle to HDCP 2.2 Trustzone library */
	struct mdss_hdcp_2x_ops *lib; /* Ops for driver to call into TZ */

	enum hdcp_transport_wakeup_cmd wakeup_cmd;
	enum hdmi_auth_status auth_status;
	char *buf;
	uint32_t buf_len;
	uint32_t timeout;
	uint32_t timeout_left;

	struct task_struct *thread;
	struct kthread_worker worker;
	struct kthread_work status;
	struct kthread_work auth;
	struct kthread_work send_msg;
	struct kthread_work recv_msg;
	struct kthread_work link;
	struct kthread_work poll;
};

static int hdmi_hdcp2p2_auth(struct hdmi_hdcp2p2_ctrl *ctrl);
static void hdmi_hdcp2p2_send_msg(struct hdmi_hdcp2p2_ctrl *ctrl);
static void hdmi_hdcp2p2_recv_msg(struct hdmi_hdcp2p2_ctrl *ctrl);
static void hdmi_hdcp2p2_auth_status(struct hdmi_hdcp2p2_ctrl *ctrl);
static int hdmi_hdcp2p2_link_check(struct hdmi_hdcp2p2_ctrl *ctrl);

static inline bool hdmi_hdcp2p2_is_valid_state(struct hdmi_hdcp2p2_ctrl *ctrl)
{
	if (ctrl->wakeup_cmd == HDCP_TRANSPORT_CMD_AUTHENTICATE)
		return true;

	if (atomic_read(&ctrl->auth_state) != HDCP_STATE_INACTIVE)
		return true;

	return false;
}

static int hdmi_hdcp2p2_copy_buf(struct hdmi_hdcp2p2_ctrl *ctrl,
	struct hdcp_transport_wakeup_data *data)
{
	mutex_lock(&ctrl->msg_lock);

	if (!data->buf_len) {
		mutex_unlock(&ctrl->msg_lock);
		return 0;
	}

	ctrl->buf_len = data->buf_len;

	kzfree(ctrl->buf);

	ctrl->buf = kzalloc(data->buf_len, GFP_KERNEL);

	if (!ctrl->buf) {
		mutex_unlock(&ctrl->msg_lock);
		return -ENOMEM;
	}

	memcpy(ctrl->buf, data->buf, ctrl->buf_len);

	mutex_unlock(&ctrl->msg_lock);

	return 0;
}

static int hdmi_hdcp2p2_wakeup(struct hdcp_transport_wakeup_data *data)
{
	struct hdmi_hdcp2p2_ctrl *ctrl;

	if (!data) {
		pr_err("invalid wakeup data\n");
		return -EINVAL;
	}

	ctrl = data->context;
	if (!ctrl) {
		pr_err("invalid hdcp ctrl\n");
		return -EINVAL;
	}

	mutex_lock(&ctrl->wakeup_mutex);

	pr_debug("cmd: %s, timeout %dms, tethered %d\n",
		hdcp_transport_cmd_to_str(data->cmd),
		data->timeout, ctrl->tethered);

	ctrl->wakeup_cmd = data->cmd;

	if (data->timeout)
		ctrl->timeout = data->timeout * 2;
	else
		ctrl->timeout = HDCP2P2_DEFAULT_TIMEOUT;

	if (!hdmi_hdcp2p2_is_valid_state(ctrl)) {
		pr_err("invalid hdcp2p2 state\n");
		goto exit;
	}

	if (hdmi_hdcp2p2_copy_buf(ctrl, data))
		goto exit;

	if (ctrl->wakeup_cmd == HDCP_TRANSPORT_CMD_STATUS_SUCCESS)
		ctrl->auth_status = HDMI_HDCP_AUTH_STATUS_SUCCESS;
	else if (ctrl->wakeup_cmd == HDCP_TRANSPORT_CMD_STATUS_FAILED)
		ctrl->auth_status = HDMI_HDCP_AUTH_STATUS_FAILURE;

	if (ctrl->tethered)
		goto exit;

	switch (ctrl->wakeup_cmd) {
	case HDCP_TRANSPORT_CMD_SEND_MESSAGE:
		kthread_queue_work(&ctrl->worker, &ctrl->send_msg);
		break;
	case HDCP_TRANSPORT_CMD_RECV_MESSAGE:
		kthread_queue_work(&ctrl->worker, &ctrl->recv_msg);
		break;
	case HDCP_TRANSPORT_CMD_STATUS_SUCCESS:
	case HDCP_TRANSPORT_CMD_STATUS_FAILED:
		kthread_queue_work(&ctrl->worker, &ctrl->status);
		break;
	case HDCP_TRANSPORT_CMD_LINK_POLL:
		kthread_queue_work(&ctrl->worker, &ctrl->poll);
		break;
	case HDCP_TRANSPORT_CMD_AUTHENTICATE:
		kthread_queue_work(&ctrl->worker, &ctrl->auth);
		break;
	default:
		pr_err("invalid wakeup command %d\n", ctrl->wakeup_cmd);
	}
exit:
	mutex_unlock(&ctrl->wakeup_mutex);
	return 0;
}

static inline int hdmi_hdcp2p2_wakeup_lib(struct hdmi_hdcp2p2_ctrl *ctrl,
	struct mdss_hdcp_2x_wakeup_data *data)
{
	int rc = 0;

	if (ctrl && ctrl->lib && ctrl->lib->wakeup &&
		data && (data->cmd != HDCP_2X_CMD_INVALID)) {
		rc = ctrl->lib->wakeup(data);
		if (rc)
			pr_err("error sending %s to lib\n",
				mdss_hdcp_2x_cmd_to_str(data->cmd));
	}

	return rc;
}

static void hdmi_hdcp2p2_run(struct hdmi_hdcp2p2_ctrl *ctrl)
{
	if (!ctrl) {
		pr_err("invalid hdcp ctrl\n");
		return;
	}

	while (1) {
		switch (ctrl->wakeup_cmd) {
		case HDCP_TRANSPORT_CMD_SEND_MESSAGE:
			ctrl->wakeup_cmd = HDCP_TRANSPORT_CMD_INVALID;
			hdmi_hdcp2p2_send_msg(ctrl);
			break;
		case HDCP_TRANSPORT_CMD_RECV_MESSAGE:
			ctrl->wakeup_cmd = HDCP_TRANSPORT_CMD_INVALID;
			hdmi_hdcp2p2_recv_msg(ctrl);
			break;
		case HDCP_TRANSPORT_CMD_STATUS_SUCCESS:
		case HDCP_TRANSPORT_CMD_STATUS_FAILED:
			hdmi_hdcp2p2_auth_status(ctrl);
			goto exit;
		case HDCP_TRANSPORT_CMD_LINK_POLL:
			hdmi_hdcp2p2_link_check(ctrl);
			goto exit;
		default:
			goto exit;
		}
	}
exit:
	ctrl->wakeup_cmd = HDCP_TRANSPORT_CMD_INVALID;
}

int hdmi_hdcp2p2_authenticate_tethered(struct hdmi_hdcp2p2_ctrl *ctrl)
{
	int rc = 0;

	if (!ctrl) {
		pr_err("invalid hdcp ctrl\n");
		rc = -EINVAL;
		goto exit;
	}

	rc = hdmi_hdcp2p2_auth(ctrl);
	if (rc) {
		pr_err("auth failed %d\n", rc);
		goto exit;
	}

	hdmi_hdcp2p2_run(ctrl);
exit:
	return rc;
}

static void hdmi_hdcp2p2_reset(struct hdmi_hdcp2p2_ctrl *ctrl)
{
	if (!ctrl) {
		pr_err("invalid hdcp ctrl\n");
		return;
	}

	ctrl->sink_status = SINK_DISCONNECTED;
	atomic_set(&ctrl->auth_state, HDCP_STATE_INACTIVE);
}

static void hdmi_hdcp2p2_off(void *input)
{
	struct hdmi_hdcp2p2_ctrl *ctrl = (struct hdmi_hdcp2p2_ctrl *)input;
	struct hdcp_transport_wakeup_data cdata = {
			HDCP_TRANSPORT_CMD_AUTHENTICATE};

	if (!ctrl) {
		pr_err("invalid hdcp ctrl\n");
		return;
	}

	hdmi_hdcp2p2_reset(ctrl);

	kthread_flush_worker(&ctrl->worker);

	hdmi_hdcp2p2_ddc_disable(ctrl->init_data.ddc_ctrl);

	if (ctrl->tethered) {
		hdmi_hdcp2p2_auth(ctrl);
	} else {
		cdata.context = input;
		hdmi_hdcp2p2_wakeup(&cdata);
	}
}

static int hdmi_hdcp2p2_authenticate(void *input)
{
	struct hdmi_hdcp2p2_ctrl *ctrl = input;
	struct hdcp_transport_wakeup_data cdata = {
			HDCP_TRANSPORT_CMD_AUTHENTICATE};
	u32 regval;
	int rc = 0;

	/* Enable authentication success interrupt */
	regval = DSS_REG_R(ctrl->init_data.core_io, HDMI_HDCP_INT_CTRL2);
	regval |= BIT(1) | BIT(2);

	DSS_REG_W(ctrl->init_data.core_io, HDMI_HDCP_INT_CTRL2, regval);

	kthread_flush_worker(&ctrl->worker);

	ctrl->sink_status = SINK_CONNECTED;
	atomic_set(&ctrl->auth_state, HDCP_STATE_AUTHENTICATING);

	/* make sure ddc is idle before starting hdcp 2.2 authentication */
	hdmi_scrambler_ddc_disable(ctrl->init_data.ddc_ctrl);
	hdmi_hdcp2p2_ddc_disable(ctrl->init_data.ddc_ctrl);

	if (ctrl->tethered) {
		hdmi_hdcp2p2_authenticate_tethered(ctrl);
	} else {
		cdata.context = input;
		hdmi_hdcp2p2_wakeup(&cdata);
	}

	return rc;
}

static int hdmi_hdcp2p2_reauthenticate(void *input)
{
	struct hdmi_hdcp2p2_ctrl *ctrl = (struct hdmi_hdcp2p2_ctrl *)input;

	if (!ctrl) {
		pr_err("invalid hdcp ctrl\n");
		return -EINVAL;
	}

	hdmi_hdcp2p2_reset((struct hdmi_hdcp2p2_ctrl *)input);

	return  hdmi_hdcp2p2_authenticate(input);
}

static ssize_t hdmi_hdcp2p2_sysfs_rda_tethered(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	struct hdmi_hdcp2p2_ctrl *ctrl =
		hdmi_get_featuredata_from_sysfs_dev(dev, HDMI_TX_FEAT_HDCP2P2);

	if (!ctrl) {
		pr_err("invalid hdcp ctrl\n");
		return -EINVAL;
	}

	mutex_lock(&ctrl->mutex);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", ctrl->tethered);
	mutex_unlock(&ctrl->mutex);

	return ret;
}

static ssize_t hdmi_hdcp2p2_sysfs_wta_tethered(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct hdmi_hdcp2p2_ctrl *ctrl =
		hdmi_get_featuredata_from_sysfs_dev(dev, HDMI_TX_FEAT_HDCP2P2);
	int rc, tethered;

	if (!ctrl) {
		pr_err("invalid hdcp ctrl\n");
		return -EINVAL;
	}

	mutex_lock(&ctrl->mutex);
	rc = kstrtoint(buf, 10, &tethered);
	if (rc) {
		pr_err("kstrtoint failed. rc=%d\n", rc);
		goto exit;
	}

	ctrl->tethered = !!tethered;

	if (ctrl->lib && ctrl->lib->update_exec_type && ctrl->lib_ctx)
		ctrl->lib->update_exec_type(ctrl->lib_ctx, ctrl->tethered);
exit:
	mutex_unlock(&ctrl->mutex);

	return count;
}

static ssize_t hdmi_hdcp2p2_sysfs_wta_min_level_change(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct hdmi_hdcp2p2_ctrl *ctrl =
		hdmi_get_featuredata_from_sysfs_dev(dev, HDMI_TX_FEAT_HDCP2P2);
	struct mdss_hdcp_2x_wakeup_data cdata = {
		HDCP_2X_CMD_QUERY_STREAM_TYPE};
	bool enc_notify = true;
	enum hdcp_states enc_lvl;
	int min_enc_lvl;
	int rc;

	if (!ctrl) {
		pr_err("invalid hdcp ctrl\n");
		rc = -EINVAL;
		goto exit;
	}

	rc = kstrtoint(buf, 10, &min_enc_lvl);
	if (rc) {
		DEV_ERR("%s: kstrtoint failed. rc=%d\n", __func__, rc);
		goto exit;
	}

	switch (min_enc_lvl) {
	case 0:
		enc_lvl = HDCP_STATE_AUTH_ENC_NONE;
		break;
	case 1:
		enc_lvl = HDCP_STATE_AUTH_ENC_1X;
		break;
	case 2:
		enc_lvl = HDCP_STATE_AUTH_ENC_2P2;
		break;
	default:
		enc_notify = false;
	}

	pr_debug("enc level changed %d\n", min_enc_lvl);

	cdata.context = ctrl->lib_ctx;
	hdmi_hdcp2p2_wakeup_lib(ctrl, &cdata);

	if (ctrl->tethered)
		hdmi_hdcp2p2_run(ctrl);

	if (enc_notify && ctrl->init_data.notify_status)
		ctrl->init_data.notify_status(ctrl->init_data.cb_data, enc_lvl);

	rc = count;
exit:
	return  rc;
}

static void hdmi_hdcp2p2_auth_failed(struct hdmi_hdcp2p2_ctrl *ctrl)
{
	if (!ctrl) {
		pr_err("invalid hdcp ctrl\n");
		return;
	}

	atomic_set(&ctrl->auth_state, HDCP_STATE_AUTH_FAIL);

	hdmi_hdcp2p2_ddc_disable(ctrl->init_data.ddc_ctrl);

	/* notify hdmi tx about HDCP failure */
	ctrl->init_data.notify_status(ctrl->init_data.cb_data,
		HDCP_STATE_AUTH_FAIL);
}

static void hdmi_hdcp2p2_fail_noreauth(struct hdmi_hdcp2p2_ctrl *ctrl)
{
	if (!ctrl) {
		pr_err("invalid hdcp ctrl\n");
		return;
	}

	atomic_set(&ctrl->auth_state, HDCP_STATE_AUTH_FAIL);

	hdmi_hdcp2p2_ddc_disable(ctrl->init_data.cb_data);

	/* notify hdmi tx about HDCP failure */
	ctrl->init_data.notify_status(ctrl->init_data.cb_data,
		HDCP_STATE_AUTH_FAIL_NOREAUTH);
}

static void hdmi_hdcp2p2_srm_cb(void *client_ctx)
{
	struct hdmi_hdcp2p2_ctrl *ctrl =
		(struct hdmi_hdcp2p2_ctrl *)client_ctx;
	struct mdss_hdcp_2x_wakeup_data cdata = {
		HDCP_2X_CMD_INVALID};

	if (!ctrl) {
		pr_err("invalid hdcp ctrl\n");
		return;
	}

	cdata.context = ctrl->lib_ctx;
	cdata.cmd = HDCP_2X_CMD_STOP;
	hdmi_hdcp2p2_wakeup_lib(ctrl, &cdata);

	hdmi_hdcp2p2_fail_noreauth(ctrl);
}

static int hdmi_hdcp2p2_ddc_read_message(struct hdmi_hdcp2p2_ctrl *ctrl,
	u8 *buf, int size, u32 timeout)
{
	struct hdmi_tx_ddc_data ddc_data;
	int rc;

	if (atomic_read(&ctrl->auth_state) == HDCP_STATE_INACTIVE) {
		pr_err("hdcp is off\n");
		return -EINVAL;
	}

	memset(&ddc_data, 0, sizeof(ddc_data));
	ddc_data.dev_addr = HDCP_SINK_DDC_SLAVE_ADDR;
	ddc_data.offset = HDCP_SINK_DDC_HDCP2_READ_MESSAGE;
	ddc_data.data_buf = buf;
	ddc_data.data_len = size;
	ddc_data.request_len = size;
	ddc_data.retry = 0;
	ddc_data.hard_timeout = timeout;
	ddc_data.what = "HDCP2ReadMessage";

	ctrl->init_data.ddc_ctrl->ddc_data = ddc_data;

	pr_debug("read msg timeout %dms\n", timeout);

	rc = hdmi_ddc_read(ctrl->init_data.ddc_ctrl);
	if (rc)
		pr_err("Cannot read HDCP message register\n");

	ctrl->timeout_left = ctrl->init_data.ddc_ctrl->ddc_data.timeout_left;

	return rc;
}

int hdmi_hdcp2p2_ddc_write_message(struct hdmi_hdcp2p2_ctrl *ctrl,
	u8 *buf, size_t size)
{
	struct hdmi_tx_ddc_data ddc_data;
	int rc;

	memset(&ddc_data, 0, sizeof(ddc_data));
	ddc_data.dev_addr = HDCP_SINK_DDC_SLAVE_ADDR;
	ddc_data.offset = HDCP_SINK_DDC_HDCP2_WRITE_MESSAGE;
	ddc_data.data_buf = buf;
	ddc_data.data_len = size;
	ddc_data.hard_timeout = ctrl->timeout;
	ddc_data.what = "HDCP2WriteMessage";

	ctrl->init_data.ddc_ctrl->ddc_data = ddc_data;

	rc = hdmi_ddc_write(ctrl->init_data.ddc_ctrl);
	if (rc)
		pr_err("Cannot write HDCP message register\n");

	ctrl->timeout_left = ctrl->init_data.ddc_ctrl->ddc_data.timeout_left;

	return rc;
}

static int hdmi_hdcp2p2_read_version(struct hdmi_hdcp2p2_ctrl *ctrl,
		u8 *hdcp2version)
{
	struct hdmi_tx_ddc_data ddc_data;
	int rc;

	memset(&ddc_data, 0, sizeof(ddc_data));
	ddc_data.dev_addr = HDCP_SINK_DDC_SLAVE_ADDR;
	ddc_data.offset = HDCP_SINK_DDC_HDCP2_VERSION;
	ddc_data.data_buf = hdcp2version;
	ddc_data.data_len = 1;
	ddc_data.request_len = 1;
	ddc_data.retry = 1;
	ddc_data.what = "HDCP2Version";

	ctrl->init_data.ddc_ctrl->ddc_data = ddc_data;

	rc = hdmi_ddc_read(ctrl->init_data.ddc_ctrl);
	if (rc) {
		pr_err("Cannot read HDCP2Version register");
		return rc;
	}

	pr_debug("Read HDCP2Version as %u\n", *hdcp2version);
	return rc;
}

static DEVICE_ATTR(min_level_change, 0200, NULL,
		hdmi_hdcp2p2_sysfs_wta_min_level_change);
static DEVICE_ATTR(tethered, 0644, hdmi_hdcp2p2_sysfs_rda_tethered,
		hdmi_hdcp2p2_sysfs_wta_tethered);

static struct attribute *hdmi_hdcp2p2_fs_attrs[] = {
	&dev_attr_min_level_change.attr,
	&dev_attr_tethered.attr,
	NULL,
};

static struct attribute_group hdmi_hdcp2p2_fs_attr_group = {
	.name = "hdcp2p2",
	.attrs = hdmi_hdcp2p2_fs_attrs,
};

static bool hdmi_hdcp2p2_feature_supported(void *input)
{
	struct hdmi_hdcp2p2_ctrl *ctrl = input;
	struct mdss_hdcp_2x_ops *lib = NULL;
	bool supported = false;

	if (!ctrl) {
		pr_err("invalid hdcp ctrl\n");
		goto end;
	}

	lib = ctrl->lib;
	if (!lib) {
		pr_err("invalid lib ops data\n");
		goto end;
	}

	if (lib->feature_supported)
		supported = lib->feature_supported(
			ctrl->lib_ctx);
end:
	return supported;
}

static void hdmi_hdcp2p2_send_msg(struct hdmi_hdcp2p2_ctrl *ctrl)
{
	int rc = 0;
	struct mdss_hdcp_2x_wakeup_data cdata = {HDCP_2X_CMD_INVALID};
	uint32_t msglen;
	char *msg = NULL;

	if (!ctrl) {
		pr_err("invalid hdcp ctrl\n");
		rc = -EINVAL;
		goto exit;
	}

	cdata.context = ctrl->lib_ctx;

	if (atomic_read(&ctrl->auth_state) == HDCP_STATE_INACTIVE) {
		pr_err("hdcp is off\n");
		goto exit;
	}

	mutex_lock(&ctrl->msg_lock);
	msglen = ctrl->buf_len;

	if (!msglen) {
		mutex_unlock(&ctrl->msg_lock);
		rc = -EINVAL;
		goto exit;
	}

	msg = kzalloc(msglen, GFP_KERNEL);
	if (!msg) {
		mutex_unlock(&ctrl->msg_lock);
		rc = -ENOMEM;
		goto exit;
	}

	memcpy(msg, ctrl->buf, msglen);
	mutex_unlock(&ctrl->msg_lock);

	/* Forward the message to the sink */
	rc = hdmi_hdcp2p2_ddc_write_message(ctrl, msg, (size_t)msglen);
	if (rc) {
		pr_err("Error sending msg to sink %d\n", rc);
		cdata.cmd = HDCP_2X_CMD_MSG_SEND_FAILED;
	} else {
		cdata.cmd = HDCP_2X_CMD_MSG_SEND_SUCCESS;
		cdata.timeout = ctrl->timeout_left;
	}
exit:
	kfree(msg);

	hdmi_hdcp2p2_wakeup_lib(ctrl, &cdata);
}

static void hdmi_hdcp2p2_send_msg_work(struct kthread_work *work)
{
	struct hdmi_hdcp2p2_ctrl *ctrl = container_of(work,
		struct hdmi_hdcp2p2_ctrl, send_msg);

	hdmi_hdcp2p2_send_msg(ctrl);
}

static void hdmi_hdcp2p2_link_cb(void *data)
{
	struct hdmi_hdcp2p2_ctrl *ctrl = data;

	if (!ctrl) {
		pr_debug("invalid hdcp ctrl\n");
		return;
	}

	if (atomic_read(&ctrl->auth_state) != HDCP_STATE_INACTIVE)
		kthread_queue_work(&ctrl->worker, &ctrl->link);
}

static void hdmi_hdcp2p2_recv_msg(struct hdmi_hdcp2p2_ctrl *ctrl)
{
	int timeout_hsync = 0, rc = 0;
	char *recvd_msg_buf = NULL;
	struct hdmi_tx_hdcp2p2_ddc_data *ddc_data;
	struct hdmi_tx_ddc_ctrl *ddc_ctrl;
	struct mdss_hdcp_2x_wakeup_data cdata = {HDCP_2X_CMD_INVALID};

	if (!ctrl) {
		pr_debug("invalid hdcp ctrl\n");
		rc = -EINVAL;
		goto exit;
	}

	cdata.context = ctrl->lib_ctx;

	ddc_ctrl = ctrl->init_data.ddc_ctrl;
	if (!ddc_ctrl) {
		pr_err("invalid ddc ctrl\n");
		rc = -EINVAL;
		goto exit;
	}

	if (atomic_read(&ctrl->auth_state) == HDCP_STATE_INACTIVE) {
		pr_err("hdcp is off\n");
		goto exit;
	}
	hdmi_ddc_config(ddc_ctrl);

	ddc_data = &ddc_ctrl->hdcp2p2_ddc_data;

	memset(ddc_data, 0, sizeof(*ddc_data));

	timeout_hsync = hdmi_utils_get_timeout_in_hysnc(
		ctrl->init_data.timing, ctrl->timeout);

	if (timeout_hsync <= 0) {
		pr_err("err in timeout hsync calc\n");
		timeout_hsync = HDMI_DEFAULT_TIMEOUT_HSYNC;
	}

	pr_debug("timeout for rxstatus %dms, %d hsync\n",
		ctrl->timeout, timeout_hsync);

	ddc_data->intr_mask = RXSTATUS_MESSAGE_SIZE | RXSTATUS_REAUTH_REQ;
	ddc_data->timeout_ms = ctrl->timeout;
	ddc_data->timeout_hsync = timeout_hsync;
	ddc_data->periodic_timer_hsync = timeout_hsync / 20;
	ddc_data->read_method = HDCP2P2_RXSTATUS_HW_DDC_SW_TRIGGER;
	ddc_data->wait = true;

	rc = hdmi_hdcp2p2_ddc_read_rxstatus(ddc_ctrl);
	if (rc) {
		pr_err("error reading rxstatus %d\n", rc);
		goto exit;
	}

	if (ddc_data->reauth_req) {
		ddc_data->reauth_req = false;

		pr_debug("reauth triggered by sink\n");
		rc = -EINVAL;
		goto exit;
	}

	ctrl->timeout_left = ddc_data->timeout_left;

	pr_debug("timeout left after rxstatus %dms, msg size %d\n",
		ctrl->timeout_left, ddc_data->message_size);

	if (!ddc_data->message_size) {
		pr_err("recvd invalid message size\n");
		rc = -EINVAL;
		goto exit;
	}

	recvd_msg_buf = kzalloc(ddc_data->message_size, GFP_KERNEL);
	if (!recvd_msg_buf) {
		rc = -ENOMEM;
		goto exit;
	}

	rc = hdmi_hdcp2p2_ddc_read_message(ctrl, recvd_msg_buf,
		ddc_data->message_size, ctrl->timeout_left);
	if (rc) {
		pr_err("error reading message %d\n", rc);
		goto exit;
	}

	cdata.cmd = HDCP_2X_CMD_MSG_RECV_SUCCESS;
	cdata.recvd_msg_buf = recvd_msg_buf;
	cdata.recvd_msg_len = ddc_data->message_size;
	cdata.timeout = ctrl->timeout_left;
exit:
	if (rc == -ETIMEDOUT)
		cdata.cmd = HDCP_2X_CMD_MSG_RECV_TIMEOUT;
	else if (rc)
		cdata.cmd = HDCP_2X_CMD_MSG_RECV_FAILED;

	hdmi_hdcp2p2_wakeup_lib(ctrl, &cdata);
	kfree(recvd_msg_buf);
}

static void hdmi_hdcp2p2_recv_msg_work(struct kthread_work *work)
{
	struct hdmi_hdcp2p2_ctrl *ctrl = container_of(work,
		struct hdmi_hdcp2p2_ctrl, recv_msg);

	hdmi_hdcp2p2_recv_msg(ctrl);
}

static int hdmi_hdcp2p2_link_check(struct hdmi_hdcp2p2_ctrl *ctrl)
{
	struct hdmi_tx_ddc_ctrl *ddc_ctrl;
	struct hdmi_tx_hdcp2p2_ddc_data *ddc_data;
	int timeout_hsync;

	ddc_ctrl = ctrl->init_data.ddc_ctrl;
	if (!ddc_ctrl)
		return -EINVAL;

	hdmi_ddc_config(ddc_ctrl);

	ddc_data = &ddc_ctrl->hdcp2p2_ddc_data;

	memset(ddc_data, 0, sizeof(*ddc_data));

	timeout_hsync = hdmi_utils_get_timeout_in_hysnc(
		ctrl->init_data.timing, jiffies_to_msecs(HZ / 2));

	if (timeout_hsync <= 0) {
		pr_err("err in timeout hsync calc\n");
		timeout_hsync = HDMI_DEFAULT_TIMEOUT_HSYNC;
	}
	pr_debug("timeout for rxstatus %d hsyncs\n", timeout_hsync);

	ddc_data->intr_mask = RXSTATUS_READY | RXSTATUS_MESSAGE_SIZE |
		RXSTATUS_REAUTH_REQ;
	ddc_data->timeout_hsync = timeout_hsync;
	ddc_data->periodic_timer_hsync = timeout_hsync;
	ddc_data->read_method = HDCP2P2_RXSTATUS_HW_DDC_SW_TRIGGER;
	ddc_data->link_cb = hdmi_hdcp2p2_link_cb;
	ddc_data->link_data = ctrl;

	msleep(100);
	return hdmi_hdcp2p2_ddc_read_rxstatus(ddc_ctrl);
}

static void hdmi_hdcp2p2_poll_work(struct kthread_work *work)
{
	struct hdmi_hdcp2p2_ctrl *ctrl = container_of(work,
		struct hdmi_hdcp2p2_ctrl, poll);

	hdmi_hdcp2p2_link_check(ctrl);
}

static void hdmi_hdcp2p2_auth_status(struct hdmi_hdcp2p2_ctrl *ctrl)
{
	if (!ctrl) {
		pr_debug("invalid hdcp ctrl\n");
		return;
	}

	if (atomic_read(&ctrl->auth_state) == HDCP_STATE_INACTIVE) {
		pr_err("hdcp is off\n");
		return;
	}

	if (ctrl->auth_status == HDMI_HDCP_AUTH_STATUS_SUCCESS) {
		ctrl->init_data.notify_status(ctrl->init_data.cb_data,
			HDCP_STATE_AUTHENTICATED);

		atomic_set(&ctrl->auth_state, HDCP_STATE_AUTHENTICATED);

		if (ctrl->tethered)
			hdmi_hdcp2p2_link_check(ctrl);
	} else {
		hdmi_hdcp2p2_auth_failed(ctrl);
	}
}

static void hdmi_hdcp2p2_auth_status_work(struct kthread_work *work)
{
	struct hdmi_hdcp2p2_ctrl *ctrl = container_of(work,
		struct hdmi_hdcp2p2_ctrl, status);

	hdmi_hdcp2p2_auth_status(ctrl);
}

static void hdmi_hdcp2p2_link_work(struct kthread_work *work)
{
	int rc = 0;
	struct hdmi_hdcp2p2_ctrl *ctrl = container_of(work,
		struct hdmi_hdcp2p2_ctrl, link);
	struct mdss_hdcp_2x_wakeup_data cdata = {HDCP_2X_CMD_INVALID};
	char *recvd_msg_buf = NULL;
	struct hdmi_tx_hdcp2p2_ddc_data *ddc_data;
	struct hdmi_tx_ddc_ctrl *ddc_ctrl;

	if (!ctrl) {
		pr_debug("invalid hdcp ctrl\n");
		return;
	}

	cdata.context = ctrl->lib_ctx;

	ddc_ctrl = ctrl->init_data.ddc_ctrl;
	if (!ddc_ctrl) {
		rc = -EINVAL;
		cdata.cmd = HDCP_2X_CMD_STOP;
		goto exit;
	}

	ddc_data = &ddc_ctrl->hdcp2p2_ddc_data;

	if (ddc_data->reauth_req) {
		pr_debug("reauth triggered by sink\n");

		ddc_data->reauth_req = false;
		rc = -ENOLINK;
		cdata.cmd = HDCP_2X_CMD_STOP;
		goto exit;
	}

	if (ddc_data->ready && ddc_data->message_size) {
		pr_debug("topology changed. rxstatus msg size %d\n",
			ddc_data->message_size);

		ddc_data->ready  = false;

		recvd_msg_buf = kzalloc(ddc_data->message_size, GFP_KERNEL);
		if (!recvd_msg_buf) {
			cdata.cmd = HDCP_2X_CMD_STOP;
			goto exit;
		}

		rc = hdmi_hdcp2p2_ddc_read_message(ctrl, recvd_msg_buf,
			ddc_data->message_size, HDCP2P2_DEFAULT_TIMEOUT);
		if (rc) {
			cdata.cmd = HDCP_2X_CMD_STOP;
			pr_err("error reading message %d\n", rc);
		} else {
			cdata.cmd = HDCP_2X_CMD_MSG_RECV_SUCCESS;
			cdata.recvd_msg_buf = recvd_msg_buf;
			cdata.recvd_msg_len = ddc_data->message_size;
		}

		ddc_data->message_size = 0;
	}
exit:
	hdmi_hdcp2p2_wakeup_lib(ctrl, &cdata);
	kfree(recvd_msg_buf);

	if (ctrl->tethered)
		hdmi_hdcp2p2_run(ctrl);

	if (rc) {
		hdmi_hdcp2p2_auth_failed(ctrl);
		return;
	}
}

static int hdmi_hdcp2p2_auth(struct hdmi_hdcp2p2_ctrl *ctrl)
{
	struct mdss_hdcp_2x_wakeup_data cdata = {HDCP_2X_CMD_INVALID};
	int rc = 0;

	if (!ctrl) {
		pr_debug("invalid hdcp ctrl\n");
		return -EINVAL;
	}

	cdata.context = ctrl->lib_ctx;

	if (atomic_read(&ctrl->auth_state) == HDCP_STATE_AUTHENTICATING)
		cdata.cmd = HDCP_2X_CMD_START;
	else
		cdata.cmd = HDCP_2X_CMD_STOP;

	rc = hdmi_hdcp2p2_wakeup_lib(ctrl, &cdata);
	if (rc)
		hdmi_hdcp2p2_auth_failed(ctrl);

	return rc;
}

static void hdmi_hdcp2p2_auth_work(struct kthread_work *work)
{
	struct hdmi_hdcp2p2_ctrl *ctrl = container_of(work,
		struct hdmi_hdcp2p2_ctrl, auth);

	hdmi_hdcp2p2_auth(ctrl);
}

void hdmi_hdcp2p2_deinit(void *input)
{
	struct hdmi_hdcp2p2_ctrl *ctrl = (struct hdmi_hdcp2p2_ctrl *)input;
	struct mdss_hdcp_2x_wakeup_data cdata = {HDCP_2X_CMD_INVALID};

	if (!ctrl) {
		pr_debug("invalid hdcp ctrl\n");
		return;
	}

	cdata.cmd = HDCP_2X_CMD_STOP;
	cdata.context = ctrl->lib_ctx;
	hdmi_hdcp2p2_wakeup_lib(ctrl, &cdata);

	kthread_stop(ctrl->thread);

	sysfs_remove_group(ctrl->init_data.sysfs_kobj,
				&hdmi_hdcp2p2_fs_attr_group);

	mutex_destroy(&ctrl->mutex);
	mutex_destroy(&ctrl->msg_lock);
	mutex_destroy(&ctrl->wakeup_mutex);
	kfree(ctrl);
}

void *hdmi_hdcp2p2_init(struct hdcp_init_data *init_data)
{
	int rc;
	struct hdmi_hdcp2p2_ctrl *ctrl;
	static struct hdcp_ops ops = {
		.reauthenticate = hdmi_hdcp2p2_reauthenticate,
		.authenticate = hdmi_hdcp2p2_authenticate,
		.feature_supported = hdmi_hdcp2p2_feature_supported,
		.off = hdmi_hdcp2p2_off
	};

	static struct hdcp_transport_ops client_ops = {
		.wakeup = hdmi_hdcp2p2_wakeup,
		.srm_cb = hdmi_hdcp2p2_srm_cb,
	};

	static struct mdss_hdcp_2x_ops txmtr_ops;
	struct mdss_hdcp_2x_register_data register_data;

	pr_debug("HDCP2P2 feature initialization\n");

	if (!init_data || !init_data->core_io || !init_data->mutex ||
		!init_data->ddc_ctrl || !init_data->notify_status ||
		!init_data->workq || !init_data->cb_data) {
		pr_err("invalid hdcp init data\n");
		return ERR_PTR(-EINVAL);
	}

	if (init_data->hdmi_tx_ver < MIN_HDMI_TX_MAJOR_VERSION) {
		pr_err("HDMI Tx does not support HDCP 2.2\n");
		return ERR_PTR(-ENODEV);
	}

	ctrl = kzalloc(sizeof(*ctrl), GFP_KERNEL);
	if (!ctrl)
		return ERR_PTR(-ENOMEM);

	ctrl->init_data = *init_data;
	ctrl->lib = &txmtr_ops;
	ctrl->tethered = init_data->tethered;

	rc = sysfs_create_group(init_data->sysfs_kobj,
				&hdmi_hdcp2p2_fs_attr_group);
	if (rc) {
		pr_err("hdcp2p2 sysfs group creation failed\n");
		goto error;
	}

	ctrl->sink_status = SINK_DISCONNECTED;

	atomic_set(&ctrl->auth_state, HDCP_STATE_INACTIVE);

	ctrl->ops = &ops;
	mutex_init(&ctrl->mutex);
	mutex_init(&ctrl->msg_lock);
	mutex_init(&ctrl->wakeup_mutex);

	register_data.hdcp_data = &ctrl->lib_ctx;
	register_data.client_ops = &client_ops;
	register_data.ops = &txmtr_ops;
	register_data.device_type = HDCP_TXMTR_HDMI;
	register_data.client_data = ctrl;
	register_data.tethered = ctrl->tethered;

	rc = mdss_hdcp_2x_register(&register_data);
	if (rc) {
		pr_err("Unable to register with HDCP 2.2 library\n");
		goto error;
	}

	kthread_init_worker(&ctrl->worker);

	kthread_init_work(&ctrl->auth,     hdmi_hdcp2p2_auth_work);
	kthread_init_work(&ctrl->send_msg, hdmi_hdcp2p2_send_msg_work);
	kthread_init_work(&ctrl->recv_msg, hdmi_hdcp2p2_recv_msg_work);
	kthread_init_work(&ctrl->status,   hdmi_hdcp2p2_auth_status_work);
	kthread_init_work(&ctrl->link,     hdmi_hdcp2p2_link_work);
	kthread_init_work(&ctrl->poll,     hdmi_hdcp2p2_poll_work);

	ctrl->thread = kthread_run(kthread_worker_fn,
		&ctrl->worker, "hdmi_hdcp2p2");

	if (IS_ERR(ctrl->thread)) {
		pr_err("unable to start hdcp2p2 thread\n");
		rc = PTR_ERR(ctrl->thread);
		ctrl->thread = NULL;
		goto error;
	}

	return ctrl;
error:
	kfree(ctrl);
	return ERR_PTR(rc);
}

static bool hdmi_hdcp2p2_supported(struct hdmi_hdcp2p2_ctrl *ctrl)
{
	u8 hdcp2version = 0;

	int rc = hdmi_hdcp2p2_read_version(ctrl, &hdcp2version);
	if (rc)
		goto error;

	if (hdcp2version & BIT(2)) {
		pr_debug("Sink is HDCP 2.2 capable\n");
		return true;
	}

error:
	pr_debug("Sink is not HDCP 2.2 capable\n");
	return false;
}

struct hdcp_ops *hdmi_hdcp2p2_start(void *input)
{
	struct hdmi_hdcp2p2_ctrl *ctrl = input;

	pr_debug("Checking sink capability\n");
	if (hdmi_hdcp2p2_supported(ctrl))
		return ctrl->ops;
	else
		return NULL;

}

