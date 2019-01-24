/* Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"[dp-hdcp2p2] %s: " fmt, __func__

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/types.h>
#include <linux/kthread.h>
#include <linux/msm_hdcp.h>
#include <drm/drm_dp_helper.h>

#include "sde_hdcp_2x.h"

#define DP_INTR_STATUS2				(0x00000024)
#define DP_INTR_STATUS3				(0x00000028)
#define dp_read(offset) readl_relaxed((offset))
#define dp_write(offset, data) writel_relaxed((data), (offset))
#define DP_HDCP_RXCAPS_LENGTH 3

enum dp_hdcp2p2_sink_status {
	SINK_DISCONNECTED,
	SINK_CONNECTED
};

struct dp_hdcp2p2_ctrl {
	atomic_t auth_state;
	enum dp_hdcp2p2_sink_status sink_status; /* Is sink connected */
	struct dp_hdcp2p2_interrupts *intr;
	struct sde_hdcp_init_data init_data;
	struct mutex mutex; /* mutex to protect access to ctrl */
	struct mutex msg_lock; /* mutex to protect access to msg buffer */
	struct mutex wakeup_mutex; /* mutex to protect access to wakeup call*/
	struct sde_hdcp_ops *ops;
	void *lib_ctx; /* Handle to HDCP 2.2 Trustzone library */
	struct sde_hdcp_2x_ops *lib; /* Ops for driver to call into TZ */
	enum hdcp_transport_wakeup_cmd wakeup_cmd;

	struct task_struct *thread;
	struct kthread_worker worker;
	struct kthread_work auth;
	struct kthread_work send_msg;
	struct kthread_work recv_msg;
	struct kthread_work link;
	struct hdcp2_buffer response;
	struct hdcp2_buffer request;
	uint32_t total_message_length;
	uint32_t timeout;
	struct sde_hdcp_2x_msg_part msg_part[HDCP_MAX_MESSAGE_PARTS];
	u8 sink_rx_status;
	u8 rx_status;
	char abort_mask;

	bool polling;
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

static inline int dp_hdcp2p2_valid_handle(struct dp_hdcp2p2_ctrl *ctrl)
{
	if (!ctrl) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	if (!ctrl->lib_ctx) {
		pr_err("HDCP library needs to be acquired\n");
		return -EINVAL;
	}

	if (!ctrl->lib) {
		pr_err("invalid lib ops data\n");
		return -EINVAL;
	}
	return 0;
}

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
	uint32_t num_messages = 0;

	if (!data || !data->message_data)
		return 0;

	mutex_lock(&ctrl->msg_lock);

	ctrl->timeout = data->timeout;
	num_messages = data->message_data->num_messages;
	ctrl->total_message_length = 0; /* Total length of all messages */

	for (i = 0; i < num_messages; i++)
		ctrl->total_message_length +=
			data->message_data->messages[i].length;

	memcpy(ctrl->msg_part, data->message_data->messages,
		sizeof(data->message_data->messages));

	ctrl->rx_status = data->message_data->rx_status;
	ctrl->abort_mask = data->abort_mask;

	if (!ctrl->total_message_length) {
		mutex_unlock(&ctrl->msg_lock);
		return 0;
	}

	ctrl->response.data = data->buf;
	ctrl->response.length = ctrl->total_message_length;
	ctrl->request.data = data->buf;
	ctrl->request.length = ctrl->total_message_length;

	mutex_unlock(&ctrl->msg_lock);

	return 0;
}

static void dp_hdcp2p2_send_auth_status(struct dp_hdcp2p2_ctrl *ctrl)
{
	ctrl->init_data.notify_status(ctrl->init_data.cb_data,
		atomic_read(&ctrl->auth_state));
}

static void dp_hdcp2p2_set_interrupts(struct dp_hdcp2p2_ctrl *ctrl, bool enable)
{
	void __iomem *base = ctrl->init_data.dp_ahb->base;
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

	ctrl->polling = false;

	pr_debug("%s\n", hdcp_transport_cmd_to_str(ctrl->wakeup_cmd));

	switch (ctrl->wakeup_cmd) {
	case HDCP_TRANSPORT_CMD_SEND_MESSAGE:
		kthread_queue_work(&ctrl->worker, &ctrl->send_msg);
		break;
	case HDCP_TRANSPORT_CMD_RECV_MESSAGE:
		if (ctrl->rx_status)
			ctrl->polling = true;
		else
			kthread_queue_work(&ctrl->worker, &ctrl->recv_msg);
		break;
	case HDCP_TRANSPORT_CMD_STATUS_SUCCESS:
		atomic_set(&ctrl->auth_state, HDCP_STATE_AUTHENTICATED);
		dp_hdcp2p2_send_auth_status(ctrl);
		break;
	case HDCP_TRANSPORT_CMD_STATUS_FAILED:
		atomic_set(&ctrl->auth_state, HDCP_STATE_AUTH_FAIL);
		kthread_cancel_work_sync(&ctrl->link);
		kthread_cancel_work_sync(&ctrl->recv_msg);
		dp_hdcp2p2_set_interrupts(ctrl, false);
		dp_hdcp2p2_send_auth_status(ctrl);
		break;
	case HDCP_TRANSPORT_CMD_LINK_POLL:
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
	struct sde_hdcp_2x_wakeup_data *data)
{
	int rc = 0;

	if (ctrl && ctrl->lib && ctrl->lib->wakeup &&
		data && (data->cmd != HDCP_2X_CMD_INVALID)) {
		rc = ctrl->lib->wakeup(data);
		if (rc)
			pr_err("error sending %s to lib\n",
				sde_hdcp_2x_cmd_to_str(data->cmd));
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

static int dp_hdcp2p2_register(void *input, bool mst_enabled)
{
	int rc;
	enum sde_hdcp_2x_device_type device_type;
	struct dp_hdcp2p2_ctrl *ctrl = (struct dp_hdcp2p2_ctrl *)input;

	rc = dp_hdcp2p2_valid_handle(ctrl);
	if (rc)
		return rc;

	if (mst_enabled)
		device_type = HDCP_TXMTR_DP_MST;
	else
		device_type = HDCP_TXMTR_DP;

	return sde_hdcp_2x_enable(ctrl->lib_ctx, device_type);
}

static int dp_hdcp2p2_on(void *input)
{
	int rc = 0;
	struct dp_hdcp2p2_ctrl *ctrl = input;
	struct sde_hdcp_2x_wakeup_data cdata = {HDCP_2X_CMD_INVALID};

	rc = dp_hdcp2p2_valid_handle(ctrl);
	if (rc)
		return rc;

	cdata.cmd = HDCP_2X_CMD_START;
	cdata.context = ctrl->lib_ctx;
	rc = ctrl->lib->wakeup(&cdata);
	if (rc)
		pr_err("Unable to start the HDCP 2.2 library. Error - %d", rc);

	return rc;
}

static void dp_hdcp2p2_off(void *input)
{
	int rc;
	struct dp_hdcp2p2_ctrl *ctrl = (struct dp_hdcp2p2_ctrl *)input;
	struct sde_hdcp_2x_wakeup_data cdata = {HDCP_2X_CMD_INVALID};

	rc = dp_hdcp2p2_valid_handle(ctrl);
	if (rc)
		return;

	if (atomic_read(&ctrl->auth_state) != HDCP_STATE_AUTH_FAIL) {
		cdata.cmd = HDCP_2X_CMD_STOP;
		cdata.context = ctrl->lib_ctx;
		dp_hdcp2p2_wakeup_lib(ctrl, &cdata);
	}

	dp_hdcp2p2_set_interrupts(ctrl, false);

	dp_hdcp2p2_reset(ctrl);

	kthread_flush_worker(&ctrl->worker);

	sde_hdcp_2x_disable(ctrl->lib_ctx);
}

static int dp_hdcp2p2_authenticate(void *input)
{
	int rc;
	struct dp_hdcp2p2_ctrl *ctrl = input;
	struct hdcp_transport_wakeup_data cdata = {
					HDCP_TRANSPORT_CMD_AUTHENTICATE};
	rc = dp_hdcp2p2_valid_handle(ctrl);
	if (rc)
		return rc;

	kthread_flush_worker(&ctrl->worker);

	dp_hdcp2p2_set_interrupts(ctrl, true);

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

static void dp_hdcp2p2_min_level_change(void *client_ctx,
		u8 min_enc_level)
{
	struct dp_hdcp2p2_ctrl *ctrl = (struct dp_hdcp2p2_ctrl *)client_ctx;
	struct sde_hdcp_2x_wakeup_data cdata = {
		HDCP_2X_CMD_MIN_ENC_LEVEL};

	if (!ctrl) {
		pr_err("invalid input\n");
		return;
	}

	if (!dp_hdcp2p2_is_valid_state(ctrl)) {
		pr_err("invalid state\n");
		return;
	}

	cdata.context = ctrl->lib_ctx;
	cdata.min_enc_level = min_enc_level;
	dp_hdcp2p2_wakeup_lib(ctrl, &cdata);
}

static int dp_hdcp2p2_aux_read_message(struct dp_hdcp2p2_ctrl *ctrl)
{
	int rc = 0, max_size = 16, read_size = 0, bytes_read = 0;
	int size = ctrl->request.length, offset = ctrl->msg_part->offset;
	u8 *buf = ctrl->request.data;

	if (atomic_read(&ctrl->auth_state) == HDCP_STATE_INACTIVE ||
		atomic_read(&ctrl->auth_state) == HDCP_STATE_AUTH_FAIL) {
		pr_err("invalid hdcp state\n");
		rc = -EINVAL;
		goto exit;
	}

	if (!buf) {
		pr_err("invalid request buffer\n");
		rc = -EINVAL;
		goto exit;
	}

	pr_debug("request: offset(0x%x), size(%d)\n", offset, size);

	do {
		read_size = min(size, max_size);

		bytes_read = drm_dp_dpcd_read(ctrl->init_data.drm_aux,
				offset, buf, read_size);
		if (bytes_read != read_size) {
			pr_err("fail: offset(0x%x), size(0x%x), rc(0x%x)\n",
					offset, read_size, bytes_read);
			rc = -EINVAL;
			break;
		}

		buf += read_size;
		offset += read_size;
		size -= read_size;
	} while (size > 0);

exit:
	return rc;
}

static int dp_hdcp2p2_aux_write_message(struct dp_hdcp2p2_ctrl *ctrl,
	u8 *buf, int size, uint offset, uint timeout)
{
	int const max_size = 16;
	int rc = 0, write_size = 0, bytes_written = 0;

	do {
		write_size = min(size, max_size);

		bytes_written = drm_dp_dpcd_write(ctrl->init_data.drm_aux,
				offset, buf, write_size);
		if (bytes_written != write_size) {
			pr_err("fail: offset(0x%x), size(0x%x), rc(0x%x)\n",
					offset, write_size, bytes_written);
			rc = -EINVAL;
			break;
		}

		buf += write_size;
		offset += write_size;
		size -= write_size;
	} while (size > 0);

	return rc;
}

static bool dp_hdcp2p2_feature_supported(void *input)
{
	int rc;
	struct dp_hdcp2p2_ctrl *ctrl = input;
	struct sde_hdcp_2x_ops *lib = NULL;
	bool supported = false;

	rc = dp_hdcp2p2_valid_handle(ctrl);
	if (rc)
		return supported;

	lib = ctrl->lib;
	if (lib->feature_supported)
		supported = lib->feature_supported(
			ctrl->lib_ctx);

	return supported;
}

static void dp_hdcp2p2_force_encryption(void *data, bool enable)
{
	int rc;
	struct dp_hdcp2p2_ctrl *ctrl = data;
	struct sde_hdcp_2x_ops *lib = NULL;

	rc = dp_hdcp2p2_valid_handle(ctrl);
	if (rc)
		return;

	lib = ctrl->lib;
	if (lib->force_encryption)
		lib->force_encryption(ctrl->lib_ctx, enable);
}

static void dp_hdcp2p2_send_msg_work(struct kthread_work *work)
{
	int rc = 0;
	struct dp_hdcp2p2_ctrl *ctrl = container_of(work,
		struct dp_hdcp2p2_ctrl, send_msg);
	struct sde_hdcp_2x_wakeup_data cdata = {HDCP_2X_CMD_INVALID};

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

	rc = dp_hdcp2p2_aux_write_message(ctrl, ctrl->response.data,
			ctrl->response.length, ctrl->msg_part->offset,
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
		cdata.cmd = HDCP_2X_CMD_MSG_SEND_TIMEOUT;
	else if (rc)
		cdata.cmd = HDCP_2X_CMD_MSG_SEND_FAILED;

	dp_hdcp2p2_wakeup_lib(ctrl, &cdata);
}

static int dp_hdcp2p2_get_msg_from_sink(struct dp_hdcp2p2_ctrl *ctrl)
{
	int rc = 0;
	struct sde_hdcp_2x_wakeup_data cdata = { HDCP_2X_CMD_INVALID };

	cdata.context = ctrl->lib_ctx;

	rc = dp_hdcp2p2_aux_read_message(ctrl);
	if (rc) {
		pr_err("error reading message %d\n", rc);
		goto exit;
	}

	cdata.total_message_length = ctrl->total_message_length;
	cdata.timeout = ctrl->timeout;
exit:
	if (rc == -ETIMEDOUT)
		cdata.cmd = HDCP_2X_CMD_MSG_RECV_TIMEOUT;
	else if (rc)
		cdata.cmd = HDCP_2X_CMD_MSG_RECV_FAILED;
	else
		cdata.cmd = HDCP_2X_CMD_MSG_RECV_SUCCESS;

	dp_hdcp2p2_wakeup_lib(ctrl, &cdata);

	return rc;
}

static void dp_hdcp2p2_recv_msg_work(struct kthread_work *work)
{
	struct sde_hdcp_2x_wakeup_data cdata = { HDCP_2X_CMD_INVALID };
	struct dp_hdcp2p2_ctrl *ctrl = container_of(work,
		struct dp_hdcp2p2_ctrl, recv_msg);

	cdata.context = ctrl->lib_ctx;

	if (atomic_read(&ctrl->auth_state) == HDCP_STATE_INACTIVE) {
		pr_err("hdcp is off\n");
		return;
	}

	dp_hdcp2p2_get_msg_from_sink(ctrl);
}

static void dp_hdcp2p2_link_work(struct kthread_work *work)
{
	int rc = 0, retries = 10;
	struct dp_hdcp2p2_ctrl *ctrl = container_of(work,
		struct dp_hdcp2p2_ctrl, link);
	struct sde_hdcp_2x_wakeup_data cdata = {HDCP_2X_CMD_INVALID};

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

	/* wait for polling to start till spec allowed timeout */
	while (!ctrl->polling && retries--)
		msleep(20);

	/* check if sink has made a message available */
	if (ctrl->polling && (ctrl->sink_rx_status & ctrl->rx_status)) {
		ctrl->sink_rx_status = 0;
		ctrl->rx_status = 0;

		dp_hdcp2p2_get_msg_from_sink(ctrl);

		ctrl->polling = false;
	}
exit:
	if (rc)
		dp_hdcp2p2_wakeup_lib(ctrl, &cdata);
}

static void dp_hdcp2p2_auth_work(struct kthread_work *work)
{
	struct dp_hdcp2p2_ctrl *ctrl = container_of(work,
		struct dp_hdcp2p2_ctrl, auth);

	if (atomic_read(&ctrl->auth_state) == HDCP_STATE_AUTHENTICATING) {
		struct sde_hdcp_2x_wakeup_data cdata = {HDCP_2X_CMD_INVALID};

		cdata.context = ctrl->lib_ctx;
		cdata.cmd = HDCP_2X_CMD_START_AUTH;
		dp_hdcp2p2_wakeup_lib(ctrl, &cdata);
	}
}

static int dp_hdcp2p2_read_rx_status(struct dp_hdcp2p2_ctrl *ctrl,
		u8 *rx_status)
{
	u32 const cp_irq_dpcd_offset = 0x201;
	u32 const rxstatus_dpcd_offset = 0x69493;
	ssize_t const bytes_to_read = 1;
	ssize_t bytes_read = 0;
	u8 buf = 0;
	int rc = 0;
	bool cp_irq = 0;

	*rx_status = 0;

	bytes_read = drm_dp_dpcd_read(ctrl->init_data.drm_aux,
			cp_irq_dpcd_offset, &buf, bytes_to_read);
	if (bytes_read != bytes_to_read) {
		pr_err("cp irq read failed\n");
		rc = bytes_read;
		goto error;
	}

	cp_irq = buf & BIT(2);
	pr_debug("cp_irq=0x%x\n", cp_irq);
	buf = 0;

	if (cp_irq) {
		bytes_read = drm_dp_dpcd_read(ctrl->init_data.drm_aux,
				rxstatus_dpcd_offset, &buf, bytes_to_read);
		if (bytes_read != bytes_to_read) {
			pr_err("rxstatus read failed\n");
			rc = bytes_read;
			goto error;
		}
		*rx_status = buf;
		pr_debug("rx_status=0x%x\n", *rx_status);
	}

error:
	return rc;
}

static int dp_hdcp2p2_cp_irq(void *input)
{
	int rc;
	struct dp_hdcp2p2_ctrl *ctrl = input;

	rc = dp_hdcp2p2_valid_handle(ctrl);
	if (rc)
		return rc;

	if (atomic_read(&ctrl->auth_state) == HDCP_STATE_AUTH_FAIL ||
		atomic_read(&ctrl->auth_state) == HDCP_STATE_INACTIVE) {
		pr_err("invalid hdcp state\n");
		return -EINVAL;
	}

	ctrl->sink_rx_status = 0;
	rc = dp_hdcp2p2_read_rx_status(ctrl, &ctrl->sink_rx_status);
	if (rc) {
		pr_err("failed to read rx status\n");
		return rc;
	}

	pr_debug("sink_rx_status=0x%x\n", ctrl->sink_rx_status);

	if (!ctrl->sink_rx_status) {
		pr_debug("not a hdcp 2.2 irq\n");
		return -EINVAL;
	}

	kthread_queue_work(&ctrl->worker, &ctrl->link);

	return 0;
}

static int dp_hdcp2p2_isr(void *input)
{
	struct dp_hdcp2p2_ctrl *ctrl = (struct dp_hdcp2p2_ctrl *)input;
	int rc = 0;
	struct dss_io_data *io;
	struct dp_hdcp2p2_interrupts *intr;
	u32 hdcp_int_val = 0;

	if (!ctrl || !ctrl->init_data.dp_ahb) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto end;
	}

	io = ctrl->init_data.dp_ahb;
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

static bool dp_hdcp2p2_supported(void *input)
{
	struct dp_hdcp2p2_ctrl *ctrl = input;
	u32 const rxcaps_dpcd_offset = 0x6921d;
	ssize_t bytes_read = 0;
	u8 buf[DP_HDCP_RXCAPS_LENGTH];

	pr_debug("Checking sink capability\n");

	bytes_read = drm_dp_dpcd_read(ctrl->init_data.drm_aux,
			rxcaps_dpcd_offset, &buf, DP_HDCP_RXCAPS_LENGTH);
	if (bytes_read != DP_HDCP_RXCAPS_LENGTH) {
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

static int dp_hdcp2p2_change_streams(struct dp_hdcp2p2_ctrl *ctrl,
		struct sde_hdcp_2x_wakeup_data *cdata)
{
	if (!ctrl || cdata->num_streams == 0 || !cdata->streams) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	if (!ctrl->lib_ctx) {
		pr_err("HDCP library needs to be acquired\n");
		return -EINVAL;
	}

	if (!ctrl->lib) {
		pr_err("invalid lib ops data\n");
		return -EINVAL;
	}

	cdata->context = ctrl->lib_ctx;
	return ctrl->lib->wakeup(cdata);
}


static int dp_hdcp2p2_register_streams(void *input, u8 num_streams,
			struct stream_info *streams)
{
	struct dp_hdcp2p2_ctrl *ctrl = input;
	struct sde_hdcp_2x_wakeup_data cdata = {HDCP_2X_CMD_OPEN_STREAMS};

	cdata.streams = streams;
	cdata.num_streams = num_streams;
	return dp_hdcp2p2_change_streams(ctrl, &cdata);
}

static int dp_hdcp2p2_deregister_streams(void *input, u8 num_streams,
			struct stream_info *streams)
{
	struct dp_hdcp2p2_ctrl *ctrl = input;
	struct sde_hdcp_2x_wakeup_data cdata = {HDCP_2X_CMD_CLOSE_STREAMS};

	cdata.streams = streams;
	cdata.num_streams = num_streams;
	return dp_hdcp2p2_change_streams(ctrl, &cdata);
}

void sde_dp_hdcp2p2_deinit(void *input)
{
	struct dp_hdcp2p2_ctrl *ctrl = (struct dp_hdcp2p2_ctrl *)input;
	struct sde_hdcp_2x_wakeup_data cdata = {HDCP_2X_CMD_INVALID};

	if (!ctrl) {
		pr_err("invalid input\n");
		return;
	}

	if (atomic_read(&ctrl->auth_state) != HDCP_STATE_AUTH_FAIL) {
		cdata.cmd = HDCP_2X_CMD_STOP;
		cdata.context = ctrl->lib_ctx;
		dp_hdcp2p2_wakeup_lib(ctrl, &cdata);
	}

	sde_hdcp_2x_deregister(ctrl->lib_ctx);

	kthread_stop(ctrl->thread);

	mutex_destroy(&ctrl->mutex);
	mutex_destroy(&ctrl->msg_lock);
	mutex_destroy(&ctrl->wakeup_mutex);
	kfree(ctrl);
}

void *sde_dp_hdcp2p2_init(struct sde_hdcp_init_data *init_data)
{
	int rc;
	struct dp_hdcp2p2_ctrl *ctrl;
	static struct sde_hdcp_ops ops = {
		.isr = dp_hdcp2p2_isr,
		.reauthenticate = dp_hdcp2p2_reauthenticate,
		.authenticate = dp_hdcp2p2_authenticate,
		.feature_supported = dp_hdcp2p2_feature_supported,
		.force_encryption = dp_hdcp2p2_force_encryption,
		.sink_support = dp_hdcp2p2_supported,
		.set_mode = dp_hdcp2p2_register,
		.on = dp_hdcp2p2_on,
		.off = dp_hdcp2p2_off,
		.cp_irq = dp_hdcp2p2_cp_irq,
		.register_streams = dp_hdcp2p2_register_streams,
		.deregister_streams = dp_hdcp2p2_deregister_streams,
	};

	static struct hdcp_transport_ops client_ops = {
		.wakeup = dp_hdcp2p2_wakeup,
	};
	static struct dp_hdcp2p2_int_set int_set1[] = {
		{BIT(17), "authentication successful", NULL},
		{BIT(20), "authentication failed", NULL},
		{BIT(24), "encryption enabled", NULL},
		{BIT(27), "encryption disabled", NULL},
		{0},
	};
	static struct dp_hdcp2p2_int_set int_set2[] = {
		{BIT(2),  "key fifo underflow", NULL},
		{0},
	};
	static struct dp_hdcp2p2_interrupts intr[] = {
		{DP_INTR_STATUS2, int_set1},
		{DP_INTR_STATUS3, int_set2},
		{0}
	};
	static struct sde_hdcp_2x_ops hdcp2x_ops;
	struct sde_hdcp_2x_register_data register_data = {0};

	if (!init_data || !init_data->cb_data ||
			!init_data->notify_status || !init_data->drm_aux) {
		pr_err("invalid input\n");
		return ERR_PTR(-EINVAL);
	}

	ctrl = kzalloc(sizeof(*ctrl), GFP_KERNEL);
	if (!ctrl)
		return ERR_PTR(-ENOMEM);

	ctrl->init_data = *init_data;
	ctrl->lib = &hdcp2x_ops;
	ctrl->response.data = NULL;
	ctrl->request.data = NULL;

	ctrl->sink_status = SINK_DISCONNECTED;
	ctrl->intr = intr;

	atomic_set(&ctrl->auth_state, HDCP_STATE_INACTIVE);

	ctrl->ops = &ops;
	mutex_init(&ctrl->mutex);
	mutex_init(&ctrl->msg_lock);
	mutex_init(&ctrl->wakeup_mutex);

	register_data.hdcp_data = &ctrl->lib_ctx;
	register_data.client_ops = &client_ops;
	register_data.ops = &hdcp2x_ops;
	register_data.client_data = ctrl;

	rc = sde_hdcp_2x_register(&register_data);
	if (rc) {
		pr_err("Unable to register with HDCP 2.2 library\n");
		goto error;
	}

	if (IS_ENABLED(CONFIG_HDCP_QSEECOM))
		msm_hdcp_register_cb(init_data->msm_hdcp_dev, ctrl,
				dp_hdcp2p2_min_level_change);

	kthread_init_worker(&ctrl->worker);

	kthread_init_work(&ctrl->auth,     dp_hdcp2p2_auth_work);
	kthread_init_work(&ctrl->send_msg, dp_hdcp2p2_send_msg_work);
	kthread_init_work(&ctrl->recv_msg, dp_hdcp2p2_recv_msg_work);
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

struct sde_hdcp_ops *sde_dp_hdcp2p2_get(void *input)
{
	return ((struct dp_hdcp2p2_ctrl *)input)->ops;
}

