/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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
#include "sde_hdcp.h"
#include "video/msm_hdmi_hdcp_mgr.h"
#include "sde_hdmi_util.h"

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

enum sde_hdmi_hdcp2p2_sink_status {
	SINK_DISCONNECTED,
	SINK_CONNECTED
};

enum sde_hdmi_auth_status {
	HDMI_HDCP_AUTH_STATUS_FAILURE,
	HDMI_HDCP_AUTH_STATUS_SUCCESS
};

struct sde_hdmi_hdcp2p2_ctrl {
	atomic_t auth_state;
	enum sde_hdmi_hdcp2p2_sink_status sink_status; /* Is sink connected */
	struct sde_hdcp_init_data init_data; /* Feature data from HDMI drv */
	struct mutex mutex; /* mutex to protect access to ctrl */
	struct mutex msg_lock; /* mutex to protect access to msg buffer */
	struct mutex wakeup_mutex; /* mutex to protect access to wakeup call*/
	struct sde_hdcp_ops *ops;
	void *lib_ctx; /* Handle to HDCP 2.2 Trustzone library */
	struct hdcp_txmtr_ops *lib; /* Ops for driver to call into TZ */

	enum hdmi_hdcp_wakeup_cmd wakeup_cmd;
	enum sde_hdmi_auth_status auth_status;
	char *send_msg_buf;
	uint32_t send_msg_len;
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

static int sde_hdmi_hdcp2p2_auth(struct sde_hdmi_hdcp2p2_ctrl *ctrl);
static void sde_hdmi_hdcp2p2_send_msg(struct sde_hdmi_hdcp2p2_ctrl *ctrl);
static void sde_hdmi_hdcp2p2_recv_msg(struct sde_hdmi_hdcp2p2_ctrl *ctrl);
static void sde_hdmi_hdcp2p2_auth_status(struct sde_hdmi_hdcp2p2_ctrl *ctrl);
static int sde_hdmi_hdcp2p2_link_check(struct sde_hdmi_hdcp2p2_ctrl *ctrl);

static bool sde_hdcp2p2_is_valid_state(struct sde_hdmi_hdcp2p2_ctrl *ctrl)
{
	if (ctrl->wakeup_cmd == HDMI_HDCP_WKUP_CMD_AUTHENTICATE)
		return true;

	if (atomic_read(&ctrl->auth_state) != HDCP_STATE_INACTIVE)
		return true;

	return false;
}

static int sde_hdmi_hdcp2p2_copy_buf(struct sde_hdmi_hdcp2p2_ctrl *ctrl,
	struct hdmi_hdcp_wakeup_data *data)
{
	mutex_lock(&ctrl->msg_lock);

	if (!data->send_msg_len) {
		mutex_unlock(&ctrl->msg_lock);
		return 0;
	}

	ctrl->send_msg_len = data->send_msg_len;

	kzfree(ctrl->send_msg_buf);

	ctrl->send_msg_buf = kzalloc(data->send_msg_len, GFP_KERNEL);

	if (!ctrl->send_msg_buf) {
		mutex_unlock(&ctrl->msg_lock);
		return -ENOMEM;
	}

	memcpy(ctrl->send_msg_buf, data->send_msg_buf, ctrl->send_msg_len);

	mutex_unlock(&ctrl->msg_lock);

	return 0;
}

static int sde_hdmi_hdcp2p2_wakeup(struct hdmi_hdcp_wakeup_data *data)
{
	struct sde_hdmi_hdcp2p2_ctrl *ctrl;

	if (!data) {
		SDE_ERROR("invalid input\n");
		return -EINVAL;
	}

	ctrl = data->context;
	if (!ctrl) {
		SDE_ERROR("invalid ctrl\n");
		return -EINVAL;
	}

	mutex_lock(&ctrl->wakeup_mutex);

	SDE_HDCP_DEBUG("cmd: %s, timeout %dms\n",
	hdmi_hdcp_cmd_to_str(data->cmd),
	data->timeout);

	ctrl->wakeup_cmd = data->cmd;

	if (data->timeout)
		ctrl->timeout = data->timeout * 2;
	else
		ctrl->timeout = HDCP2P2_DEFAULT_TIMEOUT;

	if (!sde_hdcp2p2_is_valid_state(ctrl)) {
		SDE_ERROR("invalid state\n");
		goto exit;
	}

	if (sde_hdmi_hdcp2p2_copy_buf(ctrl, data))
		goto exit;

	if (ctrl->wakeup_cmd == HDMI_HDCP_WKUP_CMD_STATUS_SUCCESS)
		ctrl->auth_status = HDMI_HDCP_AUTH_STATUS_SUCCESS;
	else if (ctrl->wakeup_cmd == HDMI_HDCP_WKUP_CMD_STATUS_FAILED)
		ctrl->auth_status = HDMI_HDCP_AUTH_STATUS_FAILURE;

	switch (ctrl->wakeup_cmd) {
	case HDMI_HDCP_WKUP_CMD_SEND_MESSAGE:
		queue_kthread_work(&ctrl->worker, &ctrl->send_msg);
		break;
	case HDMI_HDCP_WKUP_CMD_RECV_MESSAGE:
		queue_kthread_work(&ctrl->worker, &ctrl->recv_msg);
		break;
	case HDMI_HDCP_WKUP_CMD_STATUS_SUCCESS:
	case HDMI_HDCP_WKUP_CMD_STATUS_FAILED:
		queue_kthread_work(&ctrl->worker, &ctrl->status);
		break;
	case HDMI_HDCP_WKUP_CMD_LINK_POLL:
		queue_kthread_work(&ctrl->worker, &ctrl->poll);
		break;
	case HDMI_HDCP_WKUP_CMD_AUTHENTICATE:
		queue_kthread_work(&ctrl->worker, &ctrl->auth);
		break;
	default:
		SDE_ERROR("invalid wakeup command %d\n", ctrl->wakeup_cmd);
	}
exit:
	mutex_unlock(&ctrl->wakeup_mutex);
	return 0;
}

static int sde_hdmi_hdcp2p2_wakeup_lib(struct sde_hdmi_hdcp2p2_ctrl *ctrl,
	struct hdcp_lib_wakeup_data *data)
{
	int rc = 0;

	if (ctrl && ctrl->lib && ctrl->lib->wakeup &&
		data && (data->cmd != HDCP_LIB_WKUP_CMD_INVALID)) {
		rc = ctrl->lib->wakeup(data);
		if (rc)
			SDE_ERROR("error sending %s to lib\n",
				hdcp_lib_cmd_to_str(data->cmd));
	}

	return rc;
}

static void sde_hdmi_hdcp2p2_reset(struct sde_hdmi_hdcp2p2_ctrl *ctrl)
{
	if (!ctrl) {
		SDE_ERROR("invalid input\n");
		return;
	}

	ctrl->sink_status = SINK_DISCONNECTED;
	atomic_set(&ctrl->auth_state, HDCP_STATE_INACTIVE);
}

static void sde_hdmi_hdcp2p2_off(void *input)
{
	struct sde_hdmi_hdcp2p2_ctrl *ctrl;
	struct hdmi_hdcp_wakeup_data cdata = {HDMI_HDCP_WKUP_CMD_AUTHENTICATE};

	ctrl = (struct sde_hdmi_hdcp2p2_ctrl *)input;

	if (!ctrl) {
		SDE_ERROR("invalid input\n");
		return;
	}

	sde_hdmi_hdcp2p2_reset(ctrl);

	flush_kthread_worker(&ctrl->worker);

	cdata.context = input;
	sde_hdmi_hdcp2p2_wakeup(&cdata);

	/* There could be upto one frame delay
	 * between the time encryption disable is
	 * requested till the time we get encryption
	 * disabled interrupt
	 */
	msleep(20);
	sde_hdmi_hdcp2p2_ddc_disable((void *)ctrl->init_data.cb_data);
}

static int sde_hdmi_hdcp2p2_authenticate(void *input)
{
	struct sde_hdmi_hdcp2p2_ctrl *ctrl = input;
	struct hdmi_hdcp_wakeup_data cdata = {HDMI_HDCP_WKUP_CMD_AUTHENTICATE};
	u32 regval;
	int rc = 0;

	/* Enable authentication success interrupt */
	regval = DSS_REG_R(ctrl->init_data.core_io, HDMI_HDCP_INT_CTRL2);
	regval |= BIT(1) | BIT(2);

	DSS_REG_W(ctrl->init_data.core_io, HDMI_HDCP_INT_CTRL2, regval);

	flush_kthread_worker(&ctrl->worker);

	ctrl->sink_status = SINK_CONNECTED;
	atomic_set(&ctrl->auth_state, HDCP_STATE_AUTHENTICATING);

	/* make sure ddc is idle before starting hdcp 2.2 authentication */
	_sde_hdmi_scrambler_ddc_disable((void *)ctrl->init_data.cb_data);
	sde_hdmi_hdcp2p2_ddc_disable((void *)ctrl->init_data.cb_data);

	cdata.context = input;
	sde_hdmi_hdcp2p2_wakeup(&cdata);

	return rc;
}

static int sde_hdmi_hdcp2p2_reauthenticate(void *input)
{
	struct sde_hdmi_hdcp2p2_ctrl *ctrl;

	ctrl = (struct sde_hdmi_hdcp2p2_ctrl *)input;

	if (!ctrl) {
		SDE_ERROR("invalid input\n");
		return -EINVAL;
	}

	sde_hdmi_hdcp2p2_reset(ctrl);

	return  sde_hdmi_hdcp2p2_authenticate(input);
}

static void sde_hdmi_hdcp2p2_min_level_change(void *client_ctx,
int min_enc_lvl)
{
	struct sde_hdmi_hdcp2p2_ctrl *ctrl =
		(struct sde_hdmi_hdcp2p2_ctrl *)client_ctx;
	struct hdcp_lib_wakeup_data cdata = {
		HDCP_LIB_WKUP_CMD_QUERY_STREAM_TYPE};
	bool enc_notify = true;
	enum sde_hdcp_states enc_lvl;

	if (!ctrl) {
		SDE_ERROR("invalid input\n");
		return;
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

	SDE_HDCP_DEBUG("enc level changed %d\n", min_enc_lvl);

	/* notify the client first about the new level */
	if (enc_notify && ctrl->init_data.notify_status)
		ctrl->init_data.notify_status(ctrl->init_data.cb_data, enc_lvl);

	cdata.context = ctrl->lib_ctx;
	sde_hdmi_hdcp2p2_wakeup_lib(ctrl, &cdata);
}

static void sde_hdmi_hdcp2p2_mute_sink(void *client_ctx)
{
	struct sde_hdmi_hdcp2p2_ctrl *ctrl =
		(struct sde_hdmi_hdcp2p2_ctrl *)client_ctx;

	if (!ctrl) {
		SDE_ERROR("invalid input\n");
		return;
	}

	/* call into client to send avmute to the sink */
	if (ctrl->init_data.avmute_sink)
		ctrl->init_data.avmute_sink(ctrl->init_data.cb_data);
}

static void sde_hdmi_hdcp2p2_auth_failed(struct sde_hdmi_hdcp2p2_ctrl *ctrl)
{
	if (!ctrl) {
		SDE_ERROR("invalid input\n");
		return;
	}

	atomic_set(&ctrl->auth_state, HDCP_STATE_AUTH_FAIL);

	sde_hdmi_hdcp2p2_ddc_disable(ctrl->init_data.cb_data);

	/* notify hdmi tx about HDCP failure */
	ctrl->init_data.notify_status(ctrl->init_data.cb_data,
		HDCP_STATE_AUTH_FAIL);
}

static void sde_hdmi_hdcp2p2_fail_noreauth(struct sde_hdmi_hdcp2p2_ctrl *ctrl)
{
	if (!ctrl) {
		SDE_ERROR("invalid input\n");
		return;
	}

	atomic_set(&ctrl->auth_state, HDCP_STATE_AUTH_FAIL);

	sde_hdmi_hdcp2p2_ddc_disable(ctrl->init_data.cb_data);

	/* notify hdmi tx about HDCP failure */
	ctrl->init_data.notify_status(ctrl->init_data.cb_data,
		HDCP_STATE_AUTH_FAIL_NOREAUTH);
}

static void sde_hdmi_hdcp2p2_srm_cb(void *client_ctx)
{
	struct sde_hdmi_hdcp2p2_ctrl *ctrl =
		(struct sde_hdmi_hdcp2p2_ctrl *)client_ctx;
	struct hdcp_lib_wakeup_data cdata = {
		HDCP_LIB_WKUP_CMD_INVALID};

	if (!ctrl) {
		SDE_ERROR("invalid input\n");
		return;
	}

	cdata.context = ctrl->lib_ctx;
	cdata.cmd = HDCP_LIB_WKUP_CMD_STOP;
	sde_hdmi_hdcp2p2_wakeup_lib(ctrl, &cdata);

	sde_hdmi_hdcp2p2_fail_noreauth(ctrl);
}

static int sde_hdmi_hdcp2p2_ddc_rd_message(struct sde_hdmi_hdcp2p2_ctrl *ctrl,
	u8 *buf, int size, u32 timeout)
{
	struct sde_hdmi_tx_ddc_data *ddc_data;
	struct sde_hdmi_tx_ddc_ctrl *ddc_ctrl;

	int rc;

	if (!ctrl) {
		SDE_ERROR("invalid ctrl\n");
		return -EINVAL;
	}

	ddc_ctrl = ctrl->init_data.ddc_ctrl;
	ddc_data = &ddc_ctrl->ddc_data;

	if (!ddc_data) {
		SDE_ERROR("invalid ddc data\n");
		return -EINVAL;
	}

	if (atomic_read(&ctrl->auth_state) == HDCP_STATE_INACTIVE) {
		SDE_ERROR("hdcp is off\n");
		return -EINVAL;
	}

	memset(ddc_data, 0, sizeof(*ddc_data));
	ddc_data->dev_addr = HDCP_SINK_DDC_SLAVE_ADDR;
	ddc_data->offset = HDCP_SINK_DDC_HDCP2_READ_MESSAGE;
	ddc_data->data_buf = buf;
	ddc_data->data_len = size;
	ddc_data->request_len = size;
	ddc_data->retry = 0;
	ddc_data->hard_timeout = timeout;
	ddc_data->what = "HDCP2ReadMessage";

	rc = sde_hdmi_ddc_read(ctrl->init_data.cb_data);
	if (rc)
		SDE_ERROR("Cannot read HDCP message register\n");

	ctrl->timeout_left = ddc_data->timeout_left;

	return rc;
}

static int sde_hdmi_hdcp2p2_ddc_wt_message(struct sde_hdmi_hdcp2p2_ctrl *ctrl,
	u8 *buf, size_t size)
{
	struct sde_hdmi_tx_ddc_data *ddc_data;
	struct sde_hdmi_tx_ddc_ctrl *ddc_ctrl;

	int rc;

	if (!ctrl) {
		SDE_ERROR("invalid ctrl\n");
		return -EINVAL;
	}

	ddc_ctrl = ctrl->init_data.ddc_ctrl;
	ddc_data = &ddc_ctrl->ddc_data;

	if (!ddc_data) {
		SDE_ERROR("invalid ddc data\n");
		return -EINVAL;
	}

	memset(ddc_data, 0, sizeof(*ddc_data));
	ddc_data->dev_addr = HDCP_SINK_DDC_SLAVE_ADDR;
	ddc_data->offset = HDCP_SINK_DDC_HDCP2_WRITE_MESSAGE;
	ddc_data->data_buf = buf;
	ddc_data->data_len = size;
	ddc_data->hard_timeout = ctrl->timeout;
	ddc_data->what = "HDCP2WriteMessage";

	rc = sde_hdmi_ddc_write((void *)ctrl->init_data.cb_data);
	if (rc)
		SDE_ERROR("Cannot write HDCP message register\n");

	ctrl->timeout_left = ddc_data->timeout_left;

	return rc;
}

static int sde_hdmi_hdcp2p2_read_version(struct sde_hdmi_hdcp2p2_ctrl *ctrl,
		u8 *hdcp2version)
{
	struct sde_hdmi_tx_ddc_data *ddc_data;
	struct sde_hdmi_tx_ddc_ctrl *ddc_ctrl;
	int rc;

	if (!ctrl) {
		SDE_ERROR("invalid ctrl\n");
		return -EINVAL;
	}

	ddc_ctrl = ctrl->init_data.ddc_ctrl;
	ddc_data = &ddc_ctrl->ddc_data;

	if (!ddc_data) {
		SDE_ERROR("invalid ddc data\n");
		return -EINVAL;
	}
	memset(ddc_data, 0, sizeof(*ddc_data));
	ddc_data->dev_addr = HDCP_SINK_DDC_SLAVE_ADDR;
	ddc_data->offset = HDCP_SINK_DDC_HDCP2_VERSION;
	ddc_data->data_buf = hdcp2version;
	ddc_data->data_len = 1;
	ddc_data->request_len = 1;
	ddc_data->retry = 1;
	ddc_data->what = "HDCP2Version";

	rc = sde_hdmi_ddc_read((void *)ctrl->init_data.cb_data);
	if (rc) {
		SDE_ERROR("Cannot read HDCP2Version register");
		return rc;
	}

	SDE_HDCP_DEBUG("Read HDCP2Version as %u\n", *hdcp2version);
	return rc;
}

static bool sde_hdmi_hdcp2p2_feature_supported(void *input)
{
	struct sde_hdmi_hdcp2p2_ctrl *ctrl = input;
	struct hdcp_txmtr_ops *lib = NULL;
	bool supported = false;

	if (!ctrl) {
		SDE_ERROR("invalid input\n");
		goto end;
	}

	lib = ctrl->lib;
	if (!lib) {
		SDE_ERROR("invalid lib ops data\n");
		goto end;
	}

	if (lib->feature_supported) {
		supported = lib->feature_supported(
			ctrl->lib_ctx);
	}

end:
	return supported;
}

static void sde_hdmi_hdcp2p2_send_msg(struct sde_hdmi_hdcp2p2_ctrl *ctrl)
{
	int rc = 0;
	struct hdcp_lib_wakeup_data cdata = {HDCP_LIB_WKUP_CMD_INVALID};
	uint32_t msglen;
	char *msg = NULL;

	if (!ctrl) {
		SDE_ERROR("invalid input\n");
		rc = -EINVAL;
		goto exit;
	}

	cdata.context = ctrl->lib_ctx;

	if (atomic_read(&ctrl->auth_state) == HDCP_STATE_INACTIVE) {
		SDE_ERROR("hdcp is off\n");
		goto exit;
	}

	mutex_lock(&ctrl->msg_lock);
	msglen = ctrl->send_msg_len;

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

	memcpy(msg, ctrl->send_msg_buf, msglen);
	mutex_unlock(&ctrl->msg_lock);

	/* Forward the message to the sink */
	rc = sde_hdmi_hdcp2p2_ddc_wt_message(ctrl,
			msg, (size_t)msglen);
	if (rc) {
		SDE_ERROR("Error sending msg to sink %d\n", rc);
		cdata.cmd = HDCP_LIB_WKUP_CMD_MSG_SEND_FAILED;
	} else {
		cdata.cmd = HDCP_LIB_WKUP_CMD_MSG_SEND_SUCCESS;
		cdata.timeout = ctrl->timeout_left;
	}
exit:
	kfree(msg);

	sde_hdmi_hdcp2p2_wakeup_lib(ctrl, &cdata);
}

static void sde_hdmi_hdcp2p2_send_msg_work(struct kthread_work *work)
{
	struct sde_hdmi_hdcp2p2_ctrl *ctrl = container_of(work,
		struct sde_hdmi_hdcp2p2_ctrl, send_msg);

	sde_hdmi_hdcp2p2_send_msg(ctrl);
}

static void sde_hdmi_hdcp2p2_link_cb(void *data)
{
	struct sde_hdmi_hdcp2p2_ctrl *ctrl = data;

	if (!ctrl) {
		SDE_HDCP_DEBUG("invalid input\n");
		return;
	}

	if (atomic_read(&ctrl->auth_state) != HDCP_STATE_INACTIVE)
		queue_kthread_work(&ctrl->worker, &ctrl->link);
}

static void sde_hdmi_hdcp2p2_recv_msg(struct sde_hdmi_hdcp2p2_ctrl *ctrl)
{
	int timeout_hsync = 0, rc = 0;
	char *recvd_msg_buf = NULL;
	struct sde_hdmi_tx_hdcp2p2_ddc_data *ddc_data;
	struct sde_hdmi_tx_ddc_ctrl *ddc_ctrl;
	struct hdcp_lib_wakeup_data cdata = {HDCP_LIB_WKUP_CMD_INVALID};

	if (!ctrl) {
		SDE_ERROR("invalid input\n");
		rc = -EINVAL;
		goto exit;
	}

	cdata.context = ctrl->lib_ctx;

	if (atomic_read(&ctrl->auth_state) == HDCP_STATE_INACTIVE) {
		SDE_ERROR("hdcp is off\n");
		goto exit;
	}

	ddc_ctrl = ctrl->init_data.ddc_ctrl;
	if (!ddc_ctrl) {
		pr_err("invalid ddc ctrl\n");
		rc = -EINVAL;
		goto exit;
	}

	ddc_data = &ddc_ctrl->sde_hdcp2p2_ddc_data;
	memset(ddc_data, 0, sizeof(*ddc_data));

	timeout_hsync = _sde_hdmi_get_timeout_in_hysnc(
	(void *)ctrl->init_data.cb_data, ctrl->timeout);

	if (timeout_hsync <= 0) {
		SDE_ERROR("err in timeout hsync calc\n");
		timeout_hsync = HDMI_DEFAULT_TIMEOUT_HSYNC;
	}

	SDE_HDCP_DEBUG("timeout for rxstatus %dms, %d hsync\n",
	ctrl->timeout, timeout_hsync);

	ddc_data->intr_mask = RXSTATUS_MESSAGE_SIZE | RXSTATUS_REAUTH_REQ;
	ddc_data->timeout_ms = ctrl->timeout;
	ddc_data->timeout_hsync = timeout_hsync;
	ddc_data->periodic_timer_hsync = timeout_hsync / 20;
	ddc_data->read_method = HDCP2P2_RXSTATUS_HW_DDC_SW_TRIGGER;
	ddc_data->wait = true;

	rc = sde_hdmi_hdcp2p2_read_rxstatus(ctrl->init_data.cb_data);
	if (rc) {
		SDE_ERROR("error reading rxstatus %d\n", rc);
		goto exit;
	}

	if (ddc_data->reauth_req) {
		ddc_data->reauth_req = false;

		SDE_HDCP_DEBUG("reauth triggered by sink\n");
		rc = -EINVAL;
		goto exit;
	}

	ctrl->timeout_left = ddc_data->timeout_left;

	SDE_HDCP_DEBUG("timeout left after rxstatus %dms, msg size %d\n",
	ctrl->timeout_left, ddc_data->message_size);

	if (!ddc_data->message_size) {
		SDE_ERROR("recvd invalid message size\n");
		rc = -EINVAL;
		goto exit;
	}

	recvd_msg_buf = kzalloc(ddc_data->message_size, GFP_KERNEL);
	if (!recvd_msg_buf) {
		rc = -ENOMEM;
		goto exit;
	}

	rc = sde_hdmi_hdcp2p2_ddc_rd_message(ctrl, recvd_msg_buf,
		ddc_data->message_size, ctrl->timeout_left);
	if (rc) {
		SDE_ERROR("error reading message %d\n", rc);
		goto exit;
	}

	cdata.cmd = HDCP_LIB_WKUP_CMD_MSG_RECV_SUCCESS;
	cdata.recvd_msg_buf = recvd_msg_buf;
	cdata.recvd_msg_len = ddc_data->message_size;
	cdata.timeout = ctrl->timeout_left;
exit:
	if (rc == -ETIMEDOUT)
		cdata.cmd = HDCP_LIB_WKUP_CMD_MSG_RECV_TIMEOUT;
	else if (rc)
		cdata.cmd = HDCP_LIB_WKUP_CMD_MSG_RECV_FAILED;

	sde_hdmi_hdcp2p2_wakeup_lib(ctrl, &cdata);
	kfree(recvd_msg_buf);
}

static void sde_hdmi_hdcp2p2_recv_msg_work(struct kthread_work *work)
{
	struct sde_hdmi_hdcp2p2_ctrl *ctrl = container_of(work,
		struct sde_hdmi_hdcp2p2_ctrl, recv_msg);

	sde_hdmi_hdcp2p2_recv_msg(ctrl);
}

static int sde_hdmi_hdcp2p2_link_check(struct sde_hdmi_hdcp2p2_ctrl *ctrl)
{
	struct sde_hdmi_tx_ddc_ctrl *ddc_ctrl;
	struct sde_hdmi_tx_hdcp2p2_ddc_data *ddc_data;
	int timeout_hsync;
	int ret;

	ddc_ctrl = ctrl->init_data.ddc_ctrl;

	if (!ddc_ctrl)
		return -EINVAL;

	sde_hdmi_ddc_config(ctrl->init_data.cb_data);

	ddc_data = &ddc_ctrl->sde_hdcp2p2_ddc_data;

	memset(ddc_data, 0, sizeof(*ddc_data));

	timeout_hsync = _sde_hdmi_get_timeout_in_hysnc(
					(void *)ctrl->init_data.cb_data,
					jiffies_to_msecs(HZ / 2));

	if (timeout_hsync <= 0) {
		SDE_ERROR("err in timeout hsync calc\n");
		timeout_hsync = HDMI_DEFAULT_TIMEOUT_HSYNC;
	}
	SDE_HDCP_DEBUG("timeout for rxstatus %d hsyncs\n", timeout_hsync);

	ddc_data->intr_mask = RXSTATUS_READY | RXSTATUS_MESSAGE_SIZE |
		RXSTATUS_REAUTH_REQ;
	ddc_data->timeout_hsync = timeout_hsync;
	ddc_data->periodic_timer_hsync = timeout_hsync;
	ddc_data->read_method = HDCP2P2_RXSTATUS_HW_DDC_SW_TRIGGER;
	ddc_data->link_cb = sde_hdmi_hdcp2p2_link_cb;
	ddc_data->link_data = ctrl;

	ret = sde_hdmi_hdcp2p2_read_rxstatus((void *)ctrl->init_data.cb_data);
	return ret;
}

static void sde_hdmi_hdcp2p2_poll_work(struct kthread_work *work)
{
	struct sde_hdmi_hdcp2p2_ctrl *ctrl = container_of(work,
		struct sde_hdmi_hdcp2p2_ctrl, poll);

	sde_hdmi_hdcp2p2_link_check(ctrl);
}

static void sde_hdmi_hdcp2p2_auth_status(struct sde_hdmi_hdcp2p2_ctrl *ctrl)
{
	if (!ctrl) {
		SDE_ERROR("invalid input\n");
		return;
	}

	if (atomic_read(&ctrl->auth_state) == HDCP_STATE_INACTIVE) {
		SDE_ERROR("hdcp is off\n");
		return;
	}

	if (ctrl->auth_status == HDMI_HDCP_AUTH_STATUS_SUCCESS) {
		ctrl->init_data.notify_status(ctrl->init_data.cb_data,
			HDCP_STATE_AUTHENTICATED);

		atomic_set(&ctrl->auth_state, HDCP_STATE_AUTHENTICATED);
	} else {
		sde_hdmi_hdcp2p2_auth_failed(ctrl);
	}
}

static void sde_hdmi_hdcp2p2_auth_status_work(struct kthread_work *work)
{
	struct sde_hdmi_hdcp2p2_ctrl *ctrl = container_of(work,
		struct sde_hdmi_hdcp2p2_ctrl, status);

	sde_hdmi_hdcp2p2_auth_status(ctrl);
}

static void sde_hdmi_hdcp2p2_link_work(struct kthread_work *work)
{
	int rc = 0;
	struct sde_hdmi_hdcp2p2_ctrl *ctrl = container_of(work,
		struct sde_hdmi_hdcp2p2_ctrl, link);
	struct hdcp_lib_wakeup_data cdata = {HDCP_LIB_WKUP_CMD_INVALID};
	char *recvd_msg_buf = NULL;
	struct sde_hdmi_tx_hdcp2p2_ddc_data *ddc_data;
	struct sde_hdmi_tx_ddc_ctrl *ddc_ctrl;

	if (!ctrl) {
		SDE_ERROR("invalid input\n");
		return;
	}

	cdata.context = ctrl->lib_ctx;

	ddc_ctrl = ctrl->init_data.ddc_ctrl;
	if (!ddc_ctrl) {
		rc = -EINVAL;
		cdata.cmd = HDCP_LIB_WKUP_CMD_STOP;
		goto exit;
	}

	ddc_data = &ddc_ctrl->sde_hdcp2p2_ddc_data;

	if (ddc_data->reauth_req) {
		SDE_HDCP_DEBUG("reauth triggered by sink\n");

		ddc_data->reauth_req = false;
		rc = -ENOLINK;
		cdata.cmd = HDCP_LIB_WKUP_CMD_STOP;
		goto exit;
	}

	if (ddc_data->ready && ddc_data->message_size) {
		SDE_HDCP_DEBUG("topology changed. rxstatus msg size %d\n",
			ddc_data->message_size);

		ddc_data->ready  = false;

		recvd_msg_buf = kzalloc(ddc_data->message_size, GFP_KERNEL);
		if (!recvd_msg_buf) {
			cdata.cmd = HDCP_LIB_WKUP_CMD_STOP;
			goto exit;
		}

		rc = sde_hdmi_hdcp2p2_ddc_rd_message(ctrl, recvd_msg_buf,
			ddc_data->message_size, HDCP2P2_DEFAULT_TIMEOUT);
		if (rc) {
			cdata.cmd = HDCP_LIB_WKUP_CMD_STOP;
			SDE_ERROR("error reading message %d\n", rc);
		} else {
			cdata.cmd = HDCP_LIB_WKUP_CMD_MSG_RECV_SUCCESS;
			cdata.recvd_msg_buf = recvd_msg_buf;
			cdata.recvd_msg_len = ddc_data->message_size;
		}

		ddc_data->message_size = 0;
	}
exit:
	sde_hdmi_hdcp2p2_wakeup_lib(ctrl, &cdata);
	kfree(recvd_msg_buf);

	if (rc) {
		sde_hdmi_hdcp2p2_auth_failed(ctrl);
		return;
	}
}

static int sde_hdmi_hdcp2p2_auth(struct sde_hdmi_hdcp2p2_ctrl *ctrl)
{
	struct hdcp_lib_wakeup_data cdata = {HDCP_LIB_WKUP_CMD_INVALID};
	int rc = 0;

	if (!ctrl) {
		SDE_ERROR("invalid input\n");
		return -EINVAL;
	}

	cdata.context = ctrl->lib_ctx;

	if (atomic_read(&ctrl->auth_state) == HDCP_STATE_AUTHENTICATING)
		cdata.cmd = HDCP_LIB_WKUP_CMD_START;
	else
		cdata.cmd = HDCP_LIB_WKUP_CMD_STOP;

	rc = sde_hdmi_hdcp2p2_wakeup_lib(ctrl, &cdata);
	if (rc)
		sde_hdmi_hdcp2p2_auth_failed(ctrl);

	return rc;
}

static void sde_hdmi_hdcp2p2_auth_work(struct kthread_work *work)
{
	struct sde_hdmi_hdcp2p2_ctrl *ctrl = container_of(work,
		struct sde_hdmi_hdcp2p2_ctrl, auth);

	sde_hdmi_hdcp2p2_auth(ctrl);
}

void sde_hdmi_hdcp2p2_deinit(void *input)
{
	struct sde_hdmi_hdcp2p2_ctrl *ctrl;
	struct hdcp_lib_wakeup_data cdata = {HDCP_LIB_WKUP_CMD_INVALID};

	ctrl = (struct sde_hdmi_hdcp2p2_ctrl *)input;

	if (!ctrl) {
		SDE_ERROR("invalid input\n");
		return;
	}

	cdata.cmd = HDCP_LIB_WKUP_CMD_STOP;
	cdata.context = ctrl->lib_ctx;
	sde_hdmi_hdcp2p2_wakeup_lib(ctrl, &cdata);

	kthread_stop(ctrl->thread);

	mutex_destroy(&ctrl->mutex);
	mutex_destroy(&ctrl->msg_lock);
	mutex_destroy(&ctrl->wakeup_mutex);
	kfree(ctrl);
}

void *sde_hdmi_hdcp2p2_init(struct sde_hdcp_init_data *init_data)
{
	int rc;
	struct sde_hdmi_hdcp2p2_ctrl *ctrl;
	static struct sde_hdcp_ops ops = {
		.reauthenticate = sde_hdmi_hdcp2p2_reauthenticate,
		.authenticate = sde_hdmi_hdcp2p2_authenticate,
		.feature_supported = sde_hdmi_hdcp2p2_feature_supported,
		.off = sde_hdmi_hdcp2p2_off
	};

	static struct hdcp_client_ops client_ops = {
		.wakeup = sde_hdmi_hdcp2p2_wakeup,
		.notify_lvl_change = sde_hdmi_hdcp2p2_min_level_change,
		.srm_cb = sde_hdmi_hdcp2p2_srm_cb,
		.mute_sink = sde_hdmi_hdcp2p2_mute_sink,
	};

	static struct hdcp_txmtr_ops txmtr_ops;
	struct hdcp_register_data register_data;

	SDE_HDCP_DEBUG("HDCP2P2 feature initialization\n");

	if (!init_data || !init_data->core_io || !init_data->mutex ||
		!init_data->ddc_ctrl || !init_data->notify_status ||
		!init_data->workq || !init_data->cb_data) {
		SDE_ERROR("invalid input\n");
		return ERR_PTR(-EINVAL);
	}

	if (init_data->hdmi_tx_ver < MIN_HDMI_TX_MAJOR_VERSION) {
		SDE_ERROR("HDMI Tx does not support HDCP 2.2\n");
		return ERR_PTR(-ENODEV);
	}

	ctrl = kzalloc(sizeof(*ctrl), GFP_KERNEL);
	if (!ctrl)
		return ERR_PTR(-ENOMEM);

	ctrl->init_data = *init_data;
	ctrl->lib = &txmtr_ops;

	ctrl->sink_status = SINK_DISCONNECTED;

	atomic_set(&ctrl->auth_state, HDCP_STATE_INACTIVE);

	ctrl->ops = &ops;
	mutex_init(&ctrl->mutex);
	mutex_init(&ctrl->msg_lock);
	mutex_init(&ctrl->wakeup_mutex);

	register_data.hdcp_ctx = &ctrl->lib_ctx;
	register_data.client_ops = &client_ops;
	register_data.txmtr_ops = &txmtr_ops;
	register_data.device_type = HDCP_TXMTR_HDMI;
	register_data.client_ctx = ctrl;

	rc = hdcp_library_register(&register_data);
	if (rc) {
		SDE_ERROR("Unable to register with HDCP 2.2 library\n");
		goto error;
	}

	init_kthread_worker(&ctrl->worker);

	init_kthread_work(&ctrl->auth,     sde_hdmi_hdcp2p2_auth_work);
	init_kthread_work(&ctrl->send_msg, sde_hdmi_hdcp2p2_send_msg_work);
	init_kthread_work(&ctrl->recv_msg, sde_hdmi_hdcp2p2_recv_msg_work);
	init_kthread_work(&ctrl->status,   sde_hdmi_hdcp2p2_auth_status_work);
	init_kthread_work(&ctrl->link,     sde_hdmi_hdcp2p2_link_work);
	init_kthread_work(&ctrl->poll,     sde_hdmi_hdcp2p2_poll_work);

	ctrl->thread = kthread_run(kthread_worker_fn,
		&ctrl->worker, "hdmi_hdcp2p2");

	if (IS_ERR(ctrl->thread)) {
		SDE_ERROR("unable to start hdcp2p2 thread\n");
		rc = PTR_ERR(ctrl->thread);
		ctrl->thread = NULL;
		goto error;
	}

	return ctrl;
error:
	kfree(ctrl);
	return ERR_PTR(rc);
}

static bool sde_hdmi_hdcp2p2_supported(struct sde_hdmi_hdcp2p2_ctrl *ctrl)
{
	u8 hdcp2version = 0;
	int rc = sde_hdmi_hdcp2p2_read_version(ctrl, &hdcp2version);

	if (rc)
		goto error;

	if (hdcp2version & BIT(2)) {
		SDE_HDCP_DEBUG("Sink is HDCP 2.2 capable\n");
		return true;
	}

error:
	SDE_HDCP_DEBUG("Sink is not HDCP 2.2 capable\n");
	return false;
}

struct sde_hdcp_ops *sde_hdmi_hdcp2p2_start(void *input)
{
	struct sde_hdmi_hdcp2p2_ctrl *ctrl;

	ctrl = (struct sde_hdmi_hdcp2p2_ctrl *)input;

	SDE_HDCP_DEBUG("Checking sink capability\n");
	if (sde_hdmi_hdcp2p2_supported(ctrl))
		return ctrl->ops;
	else
		return NULL;

}

