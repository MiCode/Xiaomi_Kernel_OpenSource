/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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
#include "mdss_hdcp.h"
#include "mdss_dp_util.h"

enum dp_hdcp2p2_sink_status {
	SINK_DISCONNECTED,
	SINK_CONNECTED
};

enum dp_auth_status {
	DP_HDCP_AUTH_STATUS_FAILURE,
	DP_HDCP_AUTH_STATUS_SUCCESS
};

struct dp_hdcp2p2_ctrl {
	atomic_t auth_state;
	enum dp_hdcp2p2_sink_status sink_status; /* Is sink connected */
	struct hdcp_init_data init_data;
	struct mutex mutex; /* mutex to protect access to ctrl */
	struct mutex msg_lock; /* mutex to protect access to msg buffer */
	struct mutex wakeup_mutex; /* mutex to protect access to wakeup call*/
	struct hdcp_ops *ops;
	void *lib_ctx; /* Handle to HDCP 2.2 Trustzone library */
	struct hdcp_txmtr_ops *lib; /* Ops for driver to call into TZ */
	enum hdmi_hdcp_wakeup_cmd wakeup_cmd;
	enum dp_auth_status auth_status;

	struct task_struct *thread;
	struct kthread_worker worker;
	struct kthread_work status;
	struct kthread_work auth;
	struct kthread_work send_msg;
	struct kthread_work recv_msg;
	struct kthread_work link;
	struct kthread_work poll;
	char *msg_buf;
	uint32_t send_msg_len; /* length of all parameters in msg */
	uint32_t timeout;
	uint32_t num_messages;
	struct hdcp_msg_part msg_part[HDCP_MAX_MESSAGE_PARTS];
	u8 sink_rx_status;
	u8 rx_status;
	char abort_mask;

	bool cp_irq_done;
	bool polling;
};

static inline char *dp_hdcp_cmd_to_str(uint32_t cmd)
{
	switch (cmd) {
	case HDMI_HDCP_WKUP_CMD_SEND_MESSAGE:
		return "HDMI_HDCP_WKUP_CMD_SEND_MESSAGE";
	case HDMI_HDCP_WKUP_CMD_RECV_MESSAGE:
		return "HDMI_HDCP_WKUP_CMD_RECV_MESSAGE";
	case HDMI_HDCP_WKUP_CMD_STATUS_SUCCESS:
		return "HDMI_HDCP_WKUP_CMD_STATUS_SUCCESS";
	case HDMI_HDCP_WKUP_CMD_STATUS_FAILED:
		return "DP_HDCP_WKUP_CMD_STATUS_FAIL";
	case HDMI_HDCP_WKUP_CMD_LINK_POLL:
		return "HDMI_HDCP_WKUP_CMD_LINK_POLL";
	case HDMI_HDCP_WKUP_CMD_AUTHENTICATE:
		return "HDMI_HDCP_WKUP_CMD_AUTHENTICATE";
	default:
		return "???";
	}
}

static inline bool dp_hdcp2p2_is_valid_state(struct dp_hdcp2p2_ctrl *ctrl)
{
	if (ctrl->wakeup_cmd == HDMI_HDCP_WKUP_CMD_AUTHENTICATE)
		return true;

	if (atomic_read(&ctrl->auth_state) != HDCP_STATE_INACTIVE)
		return true;

	return false;
}

static int dp_hdcp2p2_copy_buf(struct dp_hdcp2p2_ctrl *ctrl,
	struct hdmi_hdcp_wakeup_data *data)
{
	int i = 0;

	if (!data || !data->message_data)
		return 0;

	mutex_lock(&ctrl->msg_lock);

	ctrl->timeout = data->timeout;
	ctrl->num_messages = data->message_data->num_messages;
	ctrl->send_msg_len = 0; /* Total len of all messages */

	for (i = 0; i < ctrl->num_messages ; i++)
		ctrl->send_msg_len += data->message_data->messages[i].length;

	memcpy(ctrl->msg_part, data->message_data->messages,
		sizeof(data->message_data->messages));

	ctrl->rx_status = data->message_data->rx_status;
	ctrl->abort_mask = data->abort_mask;

	if (!data->send_msg_len) {
		mutex_unlock(&ctrl->msg_lock);
		return 0;
	}

	kzfree(ctrl->msg_buf);

	ctrl->msg_buf = kzalloc(ctrl->send_msg_len, GFP_KERNEL);

	if (!ctrl->msg_buf) {
		mutex_unlock(&ctrl->msg_lock);
		return -ENOMEM;
	}

	/* ignore first byte as it contains message id */
	memcpy(ctrl->msg_buf, data->send_msg_buf + 1, ctrl->send_msg_len);

	mutex_unlock(&ctrl->msg_lock);

	return 0;
}

static int dp_hdcp2p2_wakeup(struct hdmi_hdcp_wakeup_data *data)
{
	struct dp_hdcp2p2_ctrl *ctrl;
	u32 const default_timeout_us = 500;

	if (!data) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	ctrl = data->context;
	if (!ctrl) {
		pr_err("invalid ctrl\n");
		return -EINVAL;
	}

	mutex_lock(&ctrl->wakeup_mutex);

	ctrl->wakeup_cmd = data->cmd;

	if (data->timeout)
		ctrl->timeout = (data->timeout) * 2;
	else
		ctrl->timeout = default_timeout_us;

	if (!dp_hdcp2p2_is_valid_state(ctrl)) {
		pr_err("invalid state\n");
		goto exit;
	}

	if (dp_hdcp2p2_copy_buf(ctrl, data))
		goto exit;

	if (ctrl->wakeup_cmd == HDMI_HDCP_WKUP_CMD_STATUS_SUCCESS)
		ctrl->auth_status = DP_HDCP_AUTH_STATUS_SUCCESS;
	else if (ctrl->wakeup_cmd == HDMI_HDCP_WKUP_CMD_STATUS_FAILED)
		ctrl->auth_status = DP_HDCP_AUTH_STATUS_FAILURE;

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
		pr_err("invalid wakeup command %d\n", ctrl->wakeup_cmd);
	}
exit:
	mutex_unlock(&ctrl->wakeup_mutex);
	return 0;
}

static inline int dp_hdcp2p2_wakeup_lib(struct dp_hdcp2p2_ctrl *ctrl,
	struct hdcp_lib_wakeup_data *data)
{
	int rc = 0;

	if (ctrl && ctrl->lib && ctrl->lib->wakeup &&
		data && (data->cmd != HDCP_LIB_WKUP_CMD_INVALID)) {
		rc = ctrl->lib->wakeup(data);
		if (rc)
			pr_err("error sending %s to lib\n",
				hdcp_lib_cmd_to_str(data->cmd));
	}

	return rc;
}

static void dp_hdcp2p2_reset(struct dp_hdcp2p2_ctrl *ctrl)
{
	if (!ctrl) {
		pr_err("invalid input\n");
		return;
	}

	ctrl->sink_status = SINK_DISCONNECTED;
	atomic_set(&ctrl->auth_state, HDCP_STATE_INACTIVE);
}

static void dp_hdcp2p2_off(void *input)
{
	struct dp_hdcp2p2_ctrl *ctrl = (struct dp_hdcp2p2_ctrl *)input;
	struct hdmi_hdcp_wakeup_data cdata = {HDMI_HDCP_WKUP_CMD_AUTHENTICATE};

	if (!ctrl) {
		pr_err("invalid input\n");
		return;
	}

	dp_hdcp2p2_reset(ctrl);

	flush_kthread_worker(&ctrl->worker);

	cdata.context = input;
	dp_hdcp2p2_wakeup(&cdata);
}

static int dp_hdcp2p2_authenticate(void *input)
{
	struct dp_hdcp2p2_ctrl *ctrl = input;
	struct hdmi_hdcp_wakeup_data cdata = {HDMI_HDCP_WKUP_CMD_AUTHENTICATE};
	int rc = 0;

	flush_kthread_worker(&ctrl->worker);

	ctrl->sink_status = SINK_CONNECTED;
	atomic_set(&ctrl->auth_state, HDCP_STATE_AUTHENTICATING);

	cdata.context = input;
	dp_hdcp2p2_wakeup(&cdata);

	return rc;
}

static int dp_hdcp2p2_reauthenticate(void *input)
{
	struct dp_hdcp2p2_ctrl *ctrl = (struct dp_hdcp2p2_ctrl *)input;

	if (!ctrl) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	dp_hdcp2p2_reset((struct dp_hdcp2p2_ctrl *)input);

	return  dp_hdcp2p2_authenticate(input);
}

static ssize_t dp_hdcp2p2_sysfs_wta_min_level_change(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct dp_hdcp2p2_ctrl *ctrl = mdss_dp_get_hdcp_data(dev);
	struct hdcp_lib_wakeup_data cdata = {
		HDCP_LIB_WKUP_CMD_QUERY_STREAM_TYPE};
	bool enc_notify = true;
	int enc_lvl;
	int min_enc_lvl;
	int rc;

	if (!ctrl) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto exit;
	}

	rc = kstrtoint(buf, 10, &min_enc_lvl);
	if (rc) {
		pr_err("kstrtoint failed. rc=%d\n", rc);
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
	dp_hdcp2p2_wakeup_lib(ctrl, &cdata);

	if (enc_notify && ctrl->init_data.notify_status)
		ctrl->init_data.notify_status(ctrl->init_data.cb_data, enc_lvl);

	rc = count;
exit:
	return  rc;
}

static void dp_hdcp2p2_auth_failed(struct dp_hdcp2p2_ctrl *ctrl)
{
	if (!ctrl) {
		pr_err("invalid input\n");
		return;
	}

	atomic_set(&ctrl->auth_state, HDCP_STATE_AUTH_FAIL);

	/* notify DP about HDCP failure */
	ctrl->init_data.notify_status(ctrl->init_data.cb_data,
		HDCP_STATE_AUTH_FAIL);
}

static int dp_hdcp2p2_aux_read_message(struct dp_hdcp2p2_ctrl *ctrl,
	u8 *buf, int size, int offset, u32 timeout)
{
	int rc, max_size = 16, read_size, len = size;
	u8 *buf_start = buf;

	if (atomic_read(&ctrl->auth_state) == HDCP_STATE_INACTIVE) {
		pr_err("hdcp is off\n");
		return -EINVAL;
	}

	do {
		struct edp_cmd cmd = {0};

		read_size = min(size, max_size);

		cmd.read = 1;
		cmd.addr = offset;
		cmd.len = read_size;
		cmd.out_buf = buf;

		rc = dp_aux_read(ctrl->init_data.cb_data, &cmd);
		if (rc) {
			pr_err("Aux read failed\n");
			break;
		}

		buf += read_size;
		offset += read_size;
		size -= read_size;
	} while (size > 0);

	print_hex_dump(KERN_DEBUG, "hdcp2p2: ", DUMP_PREFIX_NONE,
			16, 1, buf_start, len, false);
	return rc;
}

static int dp_hdcp2p2_aux_write_message(struct dp_hdcp2p2_ctrl *ctrl,
	u8 *buf, int size, uint offset, uint timeout)
{
	int rc, max_size = 16, write_size;

	do {
		struct edp_cmd cmd = {0};

		write_size = min(size, max_size);

		cmd.read = 0;
		cmd.addr = offset;
		cmd.len = write_size;
		cmd.datap = buf;

		rc = dp_aux_write(ctrl->init_data.cb_data, &cmd);
		if (rc) {
			pr_err("Aux write failed\n");
			break;
		}

		buf += write_size;
		offset += write_size;
		size -= write_size;
	} while (size > 0);

	return rc;
}

static DEVICE_ATTR(min_level_change, S_IWUSR, NULL,
		dp_hdcp2p2_sysfs_wta_min_level_change);

static struct attribute *dp_hdcp2p2_fs_attrs[] = {
	&dev_attr_min_level_change.attr,
	NULL,
};

static struct attribute_group dp_hdcp2p2_fs_attr_group = {
	.name = "dp_hdcp2p2",
	.attrs = dp_hdcp2p2_fs_attrs,
};

static bool dp_hdcp2p2_feature_supported(void *input)
{
	struct dp_hdcp2p2_ctrl *ctrl = input;
	struct hdcp_txmtr_ops *lib = NULL;
	bool supported = false;

	if (!ctrl) {
		pr_err("invalid input\n");
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

static void dp_hdcp2p2_send_msg_work(struct kthread_work *work)
{
	int rc = 0;
	int i;
	int sent_bytes = 0;
	struct dp_hdcp2p2_ctrl *ctrl = container_of(work,
		struct dp_hdcp2p2_ctrl, send_msg);
	struct hdcp_lib_wakeup_data cdata = {HDCP_LIB_WKUP_CMD_INVALID};
	char *buf = NULL;

	if (!ctrl) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto exit;
	}

	cdata.context = ctrl->lib_ctx;

	if (atomic_read(&ctrl->auth_state) == HDCP_STATE_INACTIVE) {
		pr_err("hdcp is off\n");
		goto exit;
	}

	mutex_lock(&ctrl->msg_lock);

	/* Loop through number of parameters in the messages. */
	for (i = 0; i < ctrl->num_messages; i++) {
		buf = ctrl->msg_buf + sent_bytes;

		/* Forward the message to the sink */
		rc = dp_hdcp2p2_aux_write_message(ctrl, buf,
			(size_t)ctrl->msg_part[i].length,
			ctrl->msg_part[i].offset, ctrl->timeout);
		if (rc) {
			pr_err("Error sending msg to sink %d\n", rc);
			mutex_unlock(&ctrl->msg_lock);
			goto exit;
		}
		sent_bytes += ctrl->msg_part[i].length;
	}

	cdata.cmd = HDCP_LIB_WKUP_CMD_MSG_SEND_SUCCESS;
	cdata.timeout = ctrl->timeout;
	mutex_unlock(&ctrl->msg_lock);

exit:
	if (rc == -ETIMEDOUT)
		cdata.cmd = HDCP_LIB_WKUP_CMD_MSG_RECV_TIMEOUT;
	else if (rc)
		cdata.cmd = HDCP_LIB_WKUP_CMD_MSG_RECV_FAILED;

	dp_hdcp2p2_wakeup_lib(ctrl, &cdata);
}

static int dp_hdcp2p2_get_msg_from_sink(struct dp_hdcp2p2_ctrl *ctrl)
{
	int i, rc = 0;
	char *recvd_msg_buf = NULL;
	struct hdcp_lib_wakeup_data cdata = { HDCP_LIB_WKUP_CMD_INVALID };
	int bytes_read = 0;

	cdata.context = ctrl->lib_ctx;

	recvd_msg_buf = kzalloc(ctrl->send_msg_len, GFP_KERNEL);
	if (!recvd_msg_buf) {
		rc = -ENOMEM;
		goto exit;
	}

	for (i = 0; i < ctrl->num_messages; i++) {
		rc = dp_hdcp2p2_aux_read_message(
			ctrl, recvd_msg_buf + bytes_read,
			ctrl->msg_part[i].length,
			ctrl->msg_part[i].offset,
			ctrl->timeout);
		if (rc) {
			pr_err("error reading message %d\n", rc);
			goto exit;
		}
		bytes_read += ctrl->msg_part[i].length;
	}

	cdata.recvd_msg_buf = recvd_msg_buf;
	cdata.recvd_msg_len = ctrl->send_msg_len;
	cdata.timeout = ctrl->timeout;
exit:
	if (rc == -ETIMEDOUT)
		cdata.cmd = HDCP_LIB_WKUP_CMD_MSG_RECV_TIMEOUT;
	else if (rc)
		cdata.cmd = HDCP_LIB_WKUP_CMD_MSG_RECV_FAILED;
	else
		cdata.cmd = HDCP_LIB_WKUP_CMD_MSG_RECV_SUCCESS;

	dp_hdcp2p2_wakeup_lib(ctrl, &cdata);
	kfree(recvd_msg_buf);

	return rc;
}

static void dp_hdcp2p2_recv_msg_work(struct kthread_work *work)
{
	int rc = 0;
	struct hdcp_lib_wakeup_data cdata = { HDCP_LIB_WKUP_CMD_INVALID };
	struct dp_hdcp2p2_ctrl *ctrl = container_of(work,
		struct dp_hdcp2p2_ctrl, recv_msg);

	cdata.context = ctrl->lib_ctx;

	if (atomic_read(&ctrl->auth_state) == HDCP_STATE_INACTIVE) {
		pr_err("hdcp is off\n");
		goto exit;
	}

	if (ctrl->sink_rx_status & ctrl->abort_mask) {
		pr_err("reauth or Link fail triggered by sink\n");

		ctrl->sink_rx_status = 0;
		rc = -ENOLINK;
		cdata.cmd = HDCP_LIB_WKUP_CMD_STOP;

		goto exit;
	}

	if (ctrl->rx_status && !ctrl->sink_rx_status) {
		pr_debug("Recv msg for RxStatus, but no CP_IRQ yet\n");
		ctrl->polling = true;
		goto exit;
	}

	dp_hdcp2p2_get_msg_from_sink(ctrl);

	return;
exit:
	if (rc)
		dp_hdcp2p2_wakeup_lib(ctrl, &cdata);
}

static void dp_hdcp2p2_poll_work(struct kthread_work *work)
{
	struct dp_hdcp2p2_ctrl *ctrl = container_of(work,
		struct dp_hdcp2p2_ctrl, poll);

	if (ctrl->cp_irq_done) {
		ctrl->cp_irq_done = false;
		dp_hdcp2p2_get_msg_from_sink(ctrl);
	} else {
		ctrl->polling = true;
	}
}

static void dp_hdcp2p2_auth_status_work(struct kthread_work *work)
{
	struct dp_hdcp2p2_ctrl *ctrl = container_of(work,
		struct dp_hdcp2p2_ctrl, status);

	if (!ctrl) {
		pr_err("invalid input\n");
		return;
	}

	if (atomic_read(&ctrl->auth_state) == HDCP_STATE_INACTIVE) {
		pr_err("hdcp is off\n");
		return;
	}

	if (ctrl->auth_status == DP_HDCP_AUTH_STATUS_SUCCESS) {
		ctrl->init_data.notify_status(ctrl->init_data.cb_data,
			HDCP_STATE_AUTHENTICATED);

		atomic_set(&ctrl->auth_state, HDCP_STATE_AUTHENTICATED);
	} else {
		dp_hdcp2p2_auth_failed(ctrl);
	}
}

static void dp_hdcp2p2_link_work(struct kthread_work *work)
{
	int rc = 0;
	struct dp_hdcp2p2_ctrl *ctrl = container_of(work,
		struct dp_hdcp2p2_ctrl, link);
	struct hdcp_lib_wakeup_data cdata = {HDCP_LIB_WKUP_CMD_INVALID};

	if (!ctrl) {
		pr_err("invalid input\n");
		return;
	}

	cdata.context = ctrl->lib_ctx;

	ctrl->sink_rx_status = 0;
	rc = mdss_dp_aux_read_rx_status(ctrl->init_data.cb_data,
		&ctrl->sink_rx_status);

	if (rc) {
		pr_err("failed to read rx status\n");

		cdata.cmd = HDCP_LIB_WKUP_CMD_STOP;
		goto exit;
	}

	if (ctrl->sink_rx_status & ctrl->abort_mask) {
		pr_err("reauth or Link fail triggered by sink\n");

		ctrl->sink_rx_status = 0;
		ctrl->rx_status = 0;

		rc = -ENOLINK;
		cdata.cmd = HDCP_LIB_WKUP_CMD_STOP;
		goto exit;
	}

	/* if polling, get message from sink else let polling start */
	if (ctrl->polling && (ctrl->sink_rx_status & ctrl->rx_status)) {
		ctrl->sink_rx_status = 0;
		ctrl->rx_status = 0;

		rc = dp_hdcp2p2_get_msg_from_sink(ctrl);

		ctrl->polling = false;
	} else {
		ctrl->cp_irq_done = true;
	}
exit:
	dp_hdcp2p2_wakeup_lib(ctrl, &cdata);

	if (rc) {
		dp_hdcp2p2_auth_failed(ctrl);
		return;
	}
}

static void dp_hdcp2p2_auth_work(struct kthread_work *work)
{
	int rc = 0;
	struct hdcp_lib_wakeup_data cdata = {HDCP_LIB_WKUP_CMD_INVALID};
	struct dp_hdcp2p2_ctrl *ctrl = container_of(work,
		struct dp_hdcp2p2_ctrl, auth);

	cdata.context = ctrl->lib_ctx;

	if (atomic_read(&ctrl->auth_state) == HDCP_STATE_AUTHENTICATING)
		cdata.cmd = HDCP_LIB_WKUP_CMD_START;
	else
		cdata.cmd = HDCP_LIB_WKUP_CMD_STOP;

	rc = dp_hdcp2p2_wakeup_lib(ctrl, &cdata);
	if (rc)
		dp_hdcp2p2_auth_failed(ctrl);
}

static int dp_hdcp2p2_isr(void *input)
{
	struct dp_hdcp2p2_ctrl *ctrl = input;

	if (!ctrl) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	queue_kthread_work(&ctrl->worker, &ctrl->link);

	return 0;
}

void dp_hdcp2p2_deinit(void *input)
{
	struct dp_hdcp2p2_ctrl *ctrl = (struct dp_hdcp2p2_ctrl *)input;
	struct hdcp_lib_wakeup_data cdata = {HDCP_LIB_WKUP_CMD_INVALID};

	if (!ctrl) {
		pr_err("invalid input\n");
		return;
	}

	cdata.cmd = HDCP_LIB_WKUP_CMD_STOP;
	cdata.context = ctrl->lib_ctx;
	dp_hdcp2p2_wakeup_lib(ctrl, &cdata);

	kthread_stop(ctrl->thread);

	sysfs_remove_group(ctrl->init_data.sysfs_kobj,
				&dp_hdcp2p2_fs_attr_group);

	mutex_destroy(&ctrl->mutex);
	mutex_destroy(&ctrl->msg_lock);
	mutex_destroy(&ctrl->wakeup_mutex);
	kzfree(ctrl->msg_buf);
	kfree(ctrl);
}

void *dp_hdcp2p2_init(struct hdcp_init_data *init_data)
{
	int rc;
	struct dp_hdcp2p2_ctrl *ctrl;
	static struct hdcp_ops ops = {
		.reauthenticate = dp_hdcp2p2_reauthenticate,
		.authenticate = dp_hdcp2p2_authenticate,
		.feature_supported = dp_hdcp2p2_feature_supported,
		.off = dp_hdcp2p2_off,
		.isr = dp_hdcp2p2_isr
	};

	static struct hdcp_client_ops client_ops = {
		.wakeup = dp_hdcp2p2_wakeup,
	};

	static struct hdcp_txmtr_ops txmtr_ops;
	struct hdcp_register_data register_data = {0};

	if (!init_data || !init_data->cb_data ||
			!init_data->notify_status) {
		pr_err("invalid input\n");
		return ERR_PTR(-EINVAL);
	}

	ctrl = kzalloc(sizeof(*ctrl), GFP_KERNEL);
	if (!ctrl)
		return ERR_PTR(-ENOMEM);

	ctrl->init_data = *init_data;
	ctrl->lib = &txmtr_ops;
	ctrl->msg_buf = NULL;
	rc = sysfs_create_group(init_data->sysfs_kobj,
				&dp_hdcp2p2_fs_attr_group);
	if (rc) {
		pr_err("dp_hdcp2p2 sysfs group creation failed\n");
		goto error;
	}

	ctrl->sink_status = SINK_DISCONNECTED;

	atomic_set(&ctrl->auth_state, HDCP_STATE_INACTIVE);

	ctrl->ops = &ops;
	mutex_init(&ctrl->mutex);
	mutex_init(&ctrl->msg_lock);
	mutex_init(&ctrl->wakeup_mutex);

	register_data.hdcp_ctx = &ctrl->lib_ctx;
	register_data.client_ops = &client_ops;
	register_data.txmtr_ops = &txmtr_ops;
	register_data.device_type = HDCP_TXMTR_DP;
	register_data.client_ctx = ctrl;

	rc = hdcp_library_register(&register_data);
	if (rc) {
		pr_err("Unable to register with HDCP 2.2 library\n");
		goto error;
	}

	init_kthread_worker(&ctrl->worker);

	init_kthread_work(&ctrl->auth,     dp_hdcp2p2_auth_work);
	init_kthread_work(&ctrl->send_msg, dp_hdcp2p2_send_msg_work);
	init_kthread_work(&ctrl->recv_msg, dp_hdcp2p2_recv_msg_work);
	init_kthread_work(&ctrl->status,   dp_hdcp2p2_auth_status_work);
	init_kthread_work(&ctrl->link,     dp_hdcp2p2_link_work);
	init_kthread_work(&ctrl->poll,     dp_hdcp2p2_poll_work);

	ctrl->thread = kthread_run(kthread_worker_fn,
		&ctrl->worker, "dp_hdcp2p2");

	if (IS_ERR(ctrl->thread)) {
		pr_err("unable to start DP hdcp2p2 thread\n");
		rc = PTR_ERR(ctrl->thread);
		ctrl->thread = NULL;
		goto error;
	}

	return ctrl;
error:
	kfree(ctrl);
	return ERR_PTR(rc);
}

static bool dp_hdcp2p2_supported(struct dp_hdcp2p2_ctrl *ctrl)
{
	struct edp_cmd cmd = {0};
	const u32 offset = 0x6921d;
	u8 buf;

	cmd.read = 1;
	cmd.addr = offset;
	cmd.len = sizeof(buf);
	cmd.out_buf = &buf;

	if (dp_aux_read(ctrl->init_data.cb_data, &cmd)) {
		pr_err("RxCaps read failed\n");
		goto error;
	}

	pr_debug("rxcaps 0x%x\n", buf);

	if (buf & BIT(1))
		return true;
error:
	return false;
}

struct hdcp_ops *dp_hdcp2p2_start(void *input)
{
	struct dp_hdcp2p2_ctrl *ctrl = input;

	pr_debug("Checking sink capability\n");
	if (dp_hdcp2p2_supported(ctrl))
		return ctrl->ops;
	else
		return NULL;
}

