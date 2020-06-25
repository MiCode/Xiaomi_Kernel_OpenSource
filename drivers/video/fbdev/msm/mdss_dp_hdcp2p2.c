// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2016-2017, 2020, The Linux Foundation. All rights reserved. */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/types.h>
#include <linux/kthread.h>

#include "mdss_hdcp_2x.h"
#include "mdss_hdcp.h"
#include "mdss_dp_util.h"

struct dp_hdcp2p2_ctrl;

enum dp_hdcp2p2_sink_status {
	SINK_DISCONNECTED,
	SINK_CONNECTED
};

enum dp_auth_status {
	DP_HDCP_AUTH_STATUS_FAILURE,
	DP_HDCP_AUTH_STATUS_SUCCESS
};

struct dp_hdcp2p2_int_set {
	u32 interrupt;
	char *name;
	void (*func)(struct dp_hdcp2p2_ctrl *ctrl);
};

struct dp_hdcp2p2_interrupts {
	u32 reg;
	struct dp_hdcp2p2_int_set *int_set;
};

struct dp_hdcp2p2_ctrl {
	atomic_t auth_state;
	enum dp_hdcp2p2_sink_status sink_status; /* Is sink connected */
	struct dp_hdcp2p2_interrupts *intr;
	struct hdcp_init_data init_data;
	struct mutex mutex; /* mutex to protect access to ctrl */
	struct mutex msg_lock; /* mutex to protect access to msg buffer */
	struct mutex wakeup_mutex; /* mutex to protect access to wakeup call*/
	struct hdcp_ops *ops;
	void *lib_ctx; /* Handle to HDCP 2.2 Trustzone library */
	struct mdss_hdcp_2x_ops *lib; /* Ops for driver to call into TZ */
	enum hdcp_transport_wakeup_cmd wakeup_cmd;
	enum dp_auth_status auth_status;

	struct task_struct *thread;
	struct kthread_worker worker;
	struct kthread_work status;
	struct kthread_work auth;
	struct kthread_work send_msg;
	struct kthread_work recv_msg;
	struct kthread_work link;
	char *msg_buf;
	uint32_t send_msg_len; /* length of all parameters in msg */
	uint32_t timeout;
	uint32_t num_messages;
	struct mdss_hdcp_2x_msg_part msg_part[HDCP_MAX_MESSAGE_PARTS];
	u8 sink_rx_status;
	u8 rx_status;
	char abort_mask;

	bool cp_irq_done;
	bool polling;
};

static inline bool dp_hdcp2p2_is_valid_state(struct dp_hdcp2p2_ctrl *ctrl)
{
	if (ctrl->wakeup_cmd == HDCP_TRANSPORT_CMD_AUTHENTICATE)
		return true;

	if (atomic_read(&ctrl->auth_state) != HDCP_STATE_INACTIVE)
		return true;

	return false;
}

static int dp_hdcp2p2_copy_buf(struct dp_hdcp2p2_ctrl *ctrl,
	struct hdcp_transport_wakeup_data *data)
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

	if (!data->buf_len) {
		mutex_unlock(&ctrl->msg_lock);
		return 0;
	}

	kzfree(ctrl->msg_buf);

	ctrl->msg_buf = kmemdup(data->buf+1, ctrl->send_msg_len, GFP_KERNEL);

	if (!ctrl->msg_buf) {
		mutex_unlock(&ctrl->msg_lock);
		return -ENOMEM;
	}

	mutex_unlock(&ctrl->msg_lock);

	return 0;
}

static int dp_hdcp2p2_wakeup(struct hdcp_transport_wakeup_data *data)
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

	if (ctrl->wakeup_cmd == HDCP_TRANSPORT_CMD_STATUS_SUCCESS)
		ctrl->auth_status = DP_HDCP_AUTH_STATUS_SUCCESS;
	else if (ctrl->wakeup_cmd == HDCP_TRANSPORT_CMD_STATUS_FAILED)
		ctrl->auth_status = DP_HDCP_AUTH_STATUS_FAILURE;

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
		if (ctrl->cp_irq_done)
			kthread_queue_work(&ctrl->worker, &ctrl->recv_msg);
		else
			ctrl->polling = true;
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

static inline void dp_hdcp2p2_wakeup_lib(struct dp_hdcp2p2_ctrl *ctrl,
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

static void dp_hdcp2p2_set_interrupts(struct dp_hdcp2p2_ctrl *ctrl, bool enable)
{
	unsigned char *base = ctrl->init_data.core_io->base;
	struct dp_hdcp2p2_interrupts *intr = ctrl->intr;

	while (intr && intr->reg) {
		struct dp_hdcp2p2_int_set *int_set = intr->int_set;
		u32 interrupts = 0;

		while (int_set && int_set->interrupt) {
			interrupts |= int_set->interrupt;
			int_set++;
		}

		if (enable)
			dp_write(base + intr->reg,
				dp_read(base + intr->reg) | interrupts);
		else
			dp_write(base + intr->reg,
				dp_read(base + intr->reg) & ~interrupts);

		intr++;
	}
}

static void dp_hdcp2p2_off(void *input)
{
	struct dp_hdcp2p2_ctrl *ctrl = (struct dp_hdcp2p2_ctrl *)input;
	struct hdcp_transport_wakeup_data cdata = {
			HDCP_TRANSPORT_CMD_AUTHENTICATE};

	if (!ctrl) {
		pr_err("invalid input\n");
		return;
	}

	if (atomic_read(&ctrl->auth_state) == HDCP_STATE_INACTIVE) {
		pr_err("hdcp is off\n");
		return;
	}

	dp_hdcp2p2_set_interrupts(ctrl, false);

	dp_hdcp2p2_reset(ctrl);

	kthread_flush_worker(&ctrl->worker);

	cdata.context = input;
	dp_hdcp2p2_wakeup(&cdata);
}

static int dp_hdcp2p2_authenticate(void *input)
{
	struct dp_hdcp2p2_ctrl *ctrl = input;
	struct hdcp_transport_wakeup_data cdata = {
			HDCP_TRANSPORT_CMD_AUTHENTICATE};

	kthread_flush_worker(&ctrl->worker);

	dp_hdcp2p2_set_interrupts(ctrl, true);

	ctrl->sink_status = SINK_CONNECTED;
	atomic_set(&ctrl->auth_state, HDCP_STATE_AUTHENTICATING);

	cdata.context = input;
	dp_hdcp2p2_wakeup(&cdata);

	return 0;
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

static ssize_t min_level_change_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct dp_hdcp2p2_ctrl *ctrl = mdss_dp_get_hdcp_data(dev);
	struct mdss_hdcp_2x_wakeup_data cdata = {
		HDCP_2X_CMD_QUERY_STREAM_TYPE};
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

	dp_hdcp2p2_set_interrupts(ctrl, false);

	atomic_set(&ctrl->auth_state, HDCP_STATE_AUTH_FAIL);

	/* notify DP about HDCP failure */
	ctrl->init_data.notify_status(ctrl->init_data.cb_data,
		HDCP_STATE_AUTH_FAIL);
}

static int dp_hdcp2p2_aux_read_message(struct dp_hdcp2p2_ctrl *ctrl,
	u8 *buf, int size, int offset, u32 timeout)
{
	int rc, max_size = 16, read_size;

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

static DEVICE_ATTR_WO(min_level_change);

static struct attribute *dp_hdcp2p2_fs_attrs[] = {
	&dev_attr_min_level_change.attr,
	NULL,
};

static struct attribute_group dp_hdcp2p2_fs_attr_group = {
	.name = "hdcp2p2",
	.attrs = dp_hdcp2p2_fs_attrs,
};

static bool dp_hdcp2p2_feature_supported(void *input)
{
	struct dp_hdcp2p2_ctrl *ctrl = input;
	struct mdss_hdcp_2x_ops *lib = NULL;
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
	struct dp_hdcp2p2_ctrl *ctrl = container_of(work,
		struct dp_hdcp2p2_ctrl, send_msg);
	struct mdss_hdcp_2x_wakeup_data cdata = {HDCP_2X_CMD_INVALID};

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

	rc = dp_hdcp2p2_aux_write_message(ctrl, ctrl->msg_buf,
			ctrl->send_msg_len, ctrl->msg_part->offset,
			ctrl->timeout);
	if (rc) {
		pr_err("Error sending msg to sink %d\n", rc);
		mutex_unlock(&ctrl->msg_lock);
		goto exit;
	}

	cdata.cmd = HDCP_2X_CMD_MSG_SEND_SUCCESS;
	cdata.timeout = ctrl->timeout;
	mutex_unlock(&ctrl->msg_lock);

exit:
	if (rc == -ETIMEDOUT)
		cdata.cmd = HDCP_2X_CMD_MSG_RECV_TIMEOUT;
	else if (rc)
		cdata.cmd = HDCP_2X_CMD_MSG_RECV_FAILED;

	dp_hdcp2p2_wakeup_lib(ctrl, &cdata);
}

static int dp_hdcp2p2_get_msg_from_sink(struct dp_hdcp2p2_ctrl *ctrl)
{
	int rc = 0;
	char *recvd_msg_buf = NULL;
	struct mdss_hdcp_2x_wakeup_data cdata = { HDCP_2X_CMD_INVALID };

	cdata.context = ctrl->lib_ctx;

	recvd_msg_buf = kzalloc(ctrl->send_msg_len, GFP_KERNEL);
	if (!recvd_msg_buf) {
		rc = -ENOMEM;
		goto exit;
	}

	rc = dp_hdcp2p2_aux_read_message(ctrl, recvd_msg_buf,
		ctrl->send_msg_len, ctrl->msg_part->offset,
		ctrl->timeout);
	if (rc) {
		pr_err("error reading message %d\n", rc);
		goto exit;
	}

	cdata.recvd_msg_buf = recvd_msg_buf;
	cdata.recvd_msg_len = ctrl->send_msg_len;
	cdata.timeout = ctrl->timeout;
exit:
	if (rc == -ETIMEDOUT)
		cdata.cmd = HDCP_2X_CMD_MSG_RECV_TIMEOUT;
	else if (rc)
		cdata.cmd = HDCP_2X_CMD_MSG_RECV_FAILED;
	else
		cdata.cmd = HDCP_2X_CMD_MSG_RECV_SUCCESS;

	dp_hdcp2p2_wakeup_lib(ctrl, &cdata);
	kfree(recvd_msg_buf);

	return rc;
}

static void dp_hdcp2p2_recv_msg_work(struct kthread_work *work)
{
	struct mdss_hdcp_2x_wakeup_data cdata = { HDCP_2X_CMD_INVALID };
	struct dp_hdcp2p2_ctrl *ctrl = container_of(work,
		struct dp_hdcp2p2_ctrl, recv_msg);

	cdata.context = ctrl->lib_ctx;

	if (atomic_read(&ctrl->auth_state) == HDCP_STATE_INACTIVE) {
		pr_err("hdcp is off\n");
		return;
	}

	if (ctrl->rx_status) {
		if (!ctrl->cp_irq_done) {
			pr_debug("waiting for CP_IRQ\n");
			ctrl->polling = true;
			return;
		}

		if (ctrl->rx_status & ctrl->sink_rx_status) {
			ctrl->cp_irq_done = false;
			ctrl->sink_rx_status = 0;
			ctrl->rx_status = 0;
		}
	}

	dp_hdcp2p2_get_msg_from_sink(ctrl);
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
	struct mdss_hdcp_2x_wakeup_data cdata = {HDCP_2X_CMD_INVALID};

	if (!ctrl) {
		pr_err("invalid input\n");
		return;
	}

	if (atomic_read(&ctrl->auth_state) == HDCP_STATE_AUTH_FAIL ||
		atomic_read(&ctrl->auth_state) == HDCP_STATE_INACTIVE) {
		pr_err("invalid hdcp state\n");
		return;
	}

	cdata.context = ctrl->lib_ctx;

	if (ctrl->sink_rx_status & ctrl->abort_mask) {
		if (ctrl->sink_rx_status & BIT(3))
			pr_err("reauth_req set by sink\n");

		if (ctrl->sink_rx_status & BIT(4))
			pr_err("link failure reported by sink\n");

		ctrl->sink_rx_status = 0;
		ctrl->rx_status = 0;

		rc = -ENOLINK;

		cdata.cmd = HDCP_2X_CMD_LINK_FAILED;
		atomic_set(&ctrl->auth_state, HDCP_STATE_AUTH_FAIL);
		goto exit;
	}

	if (ctrl->polling && (ctrl->sink_rx_status & ctrl->rx_status)) {
		ctrl->sink_rx_status = 0;
		ctrl->rx_status = 0;

		dp_hdcp2p2_get_msg_from_sink(ctrl);

		ctrl->polling = false;
	} else {
		ctrl->cp_irq_done = true;
	}
exit:
	if (rc)
		dp_hdcp2p2_wakeup_lib(ctrl, &cdata);
}

static void dp_hdcp2p2_auth_work(struct kthread_work *work)
{
	struct mdss_hdcp_2x_wakeup_data cdata = {HDCP_2X_CMD_INVALID};
	struct dp_hdcp2p2_ctrl *ctrl = container_of(work,
		struct dp_hdcp2p2_ctrl, auth);

	cdata.context = ctrl->lib_ctx;

	if (atomic_read(&ctrl->auth_state) == HDCP_STATE_AUTHENTICATING)
		cdata.cmd = HDCP_2X_CMD_START;
	else
		cdata.cmd = HDCP_2X_CMD_STOP;

	dp_hdcp2p2_wakeup_lib(ctrl, &cdata);
}

static int dp_hdcp2p2_cp_irq(void *input)
{
	int rc = 0;
	struct dp_hdcp2p2_ctrl *ctrl = input;

	if (!ctrl) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	if (atomic_read(&ctrl->auth_state) == HDCP_STATE_AUTH_FAIL ||
		atomic_read(&ctrl->auth_state) == HDCP_STATE_INACTIVE) {
		pr_err("invalid hdcp state\n");
		rc = -EINVAL;
		goto error;
	}

	ctrl->sink_rx_status = 0;
	rc = mdss_dp_aux_read_rx_status(ctrl->init_data.cb_data,
		&ctrl->sink_rx_status);
	if (rc) {
		pr_err("failed to read rx status\n");
		goto error;
	}

	pr_debug("sink_rx_status=0x%x\n", ctrl->sink_rx_status);

	if (!ctrl->sink_rx_status) {
		pr_debug("not a hdcp 2.2 irq\n");
		rc = -EINVAL;
		goto error;
	}

	kthread_queue_work(&ctrl->worker, &ctrl->link);

	return 0;
error:
	return rc;
}

static int dp_hdcp2p2_isr(void *input)
{
	struct dp_hdcp2p2_ctrl *ctrl = (struct dp_hdcp2p2_ctrl *)input;
	int rc = 0;
	struct dss_io_data *io;
	struct dp_hdcp2p2_interrupts *intr;
	u32 hdcp_int_val;

	if (!ctrl || !ctrl->init_data.core_io) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto end;
	}

	io = ctrl->init_data.core_io;
	intr = ctrl->intr;

	while (intr && intr->reg) {
		struct dp_hdcp2p2_int_set *int_set = intr->int_set;

		hdcp_int_val = dp_read(io->base + intr->reg);

		while (int_set && int_set->interrupt) {
			if (hdcp_int_val & (int_set->interrupt >> 2)) {
				pr_debug("%s\n", int_set->name);

				if (int_set->func)
					int_set->func(ctrl);

				dp_write(io->base + intr->reg, hdcp_int_val |
					(int_set->interrupt >> 1));
			}
			int_set++;
		}
		intr++;
	}
end:
	return rc;
}

void dp_hdcp2p2_deinit(void *input)
{
	struct dp_hdcp2p2_ctrl *ctrl = (struct dp_hdcp2p2_ctrl *)input;
	struct mdss_hdcp_2x_wakeup_data cdata = {HDCP_2X_CMD_INVALID};

	if (!ctrl) {
		pr_err("invalid input\n");
		return;
	}

	cdata.cmd = HDCP_2X_CMD_STOP;
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
		.isr = dp_hdcp2p2_isr,
		.reauthenticate = dp_hdcp2p2_reauthenticate,
		.authenticate = dp_hdcp2p2_authenticate,
		.feature_supported = dp_hdcp2p2_feature_supported,
		.off = dp_hdcp2p2_off,
		.cp_irq = dp_hdcp2p2_cp_irq,
	};

	static struct hdcp_transport_ops client_ops = {
		.wakeup = dp_hdcp2p2_wakeup,
	};
	static struct dp_hdcp2p2_int_set int_set1[] = {
		{BIT(17), "authentication successful", 0},
		{BIT(20), "authentication failed", 0},
		{BIT(24), "encryption enabled", 0},
		{BIT(27), "encryption disabled", 0},
		{0},
	};
	static struct dp_hdcp2p2_int_set int_set2[] = {
		{BIT(2),  "key fifo underflow", 0},
		{0},
	};
	static struct dp_hdcp2p2_interrupts intr[] = {
		{DP_INTR_STATUS2, int_set1},
		{DP_INTR_STATUS3, int_set2},
		{0}
	};
	static struct mdss_hdcp_2x_ops txmtr_ops;
	struct mdss_hdcp_2x_register_data register_data = {0};

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
	ctrl->intr = intr;

	atomic_set(&ctrl->auth_state, HDCP_STATE_INACTIVE);

	ctrl->ops = &ops;
	mutex_init(&ctrl->mutex);
	mutex_init(&ctrl->msg_lock);
	mutex_init(&ctrl->wakeup_mutex);

	register_data.hdcp_data = &ctrl->lib_ctx;
	register_data.client_ops = &client_ops;
	register_data.ops = &txmtr_ops;
	register_data.device_type = HDCP_TXMTR_DP;
	register_data.client_data = ctrl;

	rc = mdss_hdcp_2x_register(&register_data);
	if (rc) {
		pr_err("Unable to register with HDCP 2.2 library\n");
		goto error;
	}

	kthread_init_worker(&ctrl->worker);

	kthread_init_work(&ctrl->auth,     dp_hdcp2p2_auth_work);
	kthread_init_work(&ctrl->send_msg, dp_hdcp2p2_send_msg_work);
	kthread_init_work(&ctrl->recv_msg, dp_hdcp2p2_recv_msg_work);
	kthread_init_work(&ctrl->status,   dp_hdcp2p2_auth_status_work);
	kthread_init_work(&ctrl->link,     dp_hdcp2p2_link_work);

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
	u8 buf[3];

	cmd.read = 1;
	cmd.addr = offset;
	cmd.len = ARRAY_SIZE(buf);
	cmd.out_buf = buf;

	if (dp_aux_read(ctrl->init_data.cb_data, &cmd)) {
		pr_err("RxCaps read failed\n");
		goto error;
	}

	pr_debug("HDCP_CAPABLE=%lu\n", (buf[2] & BIT(1)) >> 1);
	pr_debug("VERSION=%d\n", buf[0]);

	if ((buf[2] & BIT(1)) && (buf[0] == 0x2))
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

