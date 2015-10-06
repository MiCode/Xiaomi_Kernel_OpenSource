/* Copyright (c) 2015 The Linux Foundation. All rights reserved.
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
#include "mdss_hdmi_hdcp.h"
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

/*
 * HDCP 2.2 encryption requires the data encryption block that is present in
 * HDMI controller version 4.0.0 and above
 */
#define MIN_HDMI_TX_MAJOR_VERSION 4

enum hdmi_hdcp2p2_sink_status {
	SINK_DISCONNECTED,
	SINK_CONNECTED
};

struct hdmi_hdcp2p2_ctrl {
	atomic_t auth_state;
	enum hdmi_hdcp2p2_sink_status sink_status; /* Is sink connected */
	struct hdmi_hdcp_init_data init_data; /* Feature data from HDMI drv */
	struct mutex mutex; /* mutex to protect access to ctrl */
	struct mutex wakeup_mutex; /* mutex to protect access to wakeup call*/
	struct hdmi_hdcp_ops *ops;
	void *lib_ctx; /* Handle to HDCP 2.2 Trustzone library */
	struct hdcp_txmtr_ops *lib; /* Ops for driver to call into TZ */
	struct completion rxstatus_completion; /* Rx status interrupt */

	enum hdmi_hdcp_wakeup_cmd wakeup_cmd;
	char *send_msg_buf;
	uint32_t send_msg_len;
	uint32_t timeout;

	struct task_struct *thread;
	struct kthread_worker worker;
	struct kthread_work status;
	struct kthread_work auth;
	struct kthread_work send_msg;
	struct kthread_work recv_msg;
};

static int hdmi_hdcp2p2_wakeup(struct hdmi_hdcp_wakeup_data *data)
{
	struct hdmi_hdcp2p2_ctrl *ctrl;

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

	pr_debug("cmd: %s\n", hdmi_hdcp_cmd_to_str(data->cmd));

	ctrl->wakeup_cmd = data->cmd;
	ctrl->timeout = data->timeout;

	if (data->send_msg_len)	{
		ctrl->send_msg_len = data->send_msg_len;

		ctrl->send_msg_buf = kzalloc(
			data->send_msg_len, GFP_KERNEL);

		if (!ctrl->send_msg_buf)
			goto exit;

		memcpy(ctrl->send_msg_buf, data->send_msg_buf,
			ctrl->send_msg_len);
	}

	switch (ctrl->wakeup_cmd) {
	case HDMI_HDCP_WKUP_CMD_SEND_MESSAGE:
		if (atomic_read(&ctrl->auth_state) != HDCP_STATE_INACTIVE)
			queue_kthread_work(&ctrl->worker, &ctrl->send_msg);
		break;
	case HDMI_HDCP_WKUP_CMD_RECV_MESSAGE:
		if (atomic_read(&ctrl->auth_state) != HDCP_STATE_INACTIVE)
			queue_kthread_work(&ctrl->worker, &ctrl->recv_msg);
		break;
	case HDMI_HDCP_WKUP_CMD_STATUS_SUCCESS:
	case HDMI_HDCP_WKUP_CMD_STATUS_FAILED:
		if (atomic_read(&ctrl->auth_state) != HDCP_STATE_INACTIVE)
			queue_kthread_work(&ctrl->worker, &ctrl->status);
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

static inline void hdmi_hdcp2p2_wakeup_lib(struct hdmi_hdcp2p2_ctrl *ctrl,
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
}

static void hdmi_hdcp2p2_reset(struct hdmi_hdcp2p2_ctrl *ctrl)
{
	if (!ctrl) {
		pr_err("invalid input\n");
		return;
	}

	ctrl->sink_status = SINK_DISCONNECTED;
	atomic_set(&ctrl->auth_state, HDCP_STATE_INACTIVE);
}

static void hdmi_hdcp2p2_off(void *input)
{
	struct hdmi_hdcp2p2_ctrl *ctrl = (struct hdmi_hdcp2p2_ctrl *)input;
	struct hdmi_hdcp_wakeup_data cdata = {HDMI_HDCP_WKUP_CMD_AUTHENTICATE};

	if (!ctrl) {
		pr_err("invalid input\n");
		return;
	}

	hdmi_hdcp2p2_reset(ctrl);

	flush_kthread_worker(&ctrl->worker);

	cdata.context = input;

	hdmi_hdcp2p2_wakeup(&cdata);
}

static int hdmi_hdcp2p2_authenticate(void *input)
{
	struct hdmi_hdcp2p2_ctrl *ctrl = input;
	struct hdmi_hdcp_wakeup_data cdata = {HDMI_HDCP_WKUP_CMD_AUTHENTICATE};
	int rc = 0;

	flush_kthread_worker(&ctrl->worker);

	ctrl->sink_status = SINK_CONNECTED;
	atomic_set(&ctrl->auth_state, HDCP_STATE_AUTHENTICATING);

	cdata.context = input;
	hdmi_hdcp2p2_wakeup(&cdata);

	return rc;
}

static int hdmi_hdcp2p2_reauthenticate(void *input)
{
	int rc = 0;
	struct hdmi_hdcp2p2_ctrl *ctrl = (struct hdmi_hdcp2p2_ctrl *)input;

	if (!ctrl) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	hdmi_hdcp2p2_reset((struct hdmi_hdcp2p2_ctrl *)input);

	rc = hdmi_hdcp2p2_authenticate(input);

	return rc;
}

static ssize_t hdmi_hdcp2p2_sysfs_rda_sink_status(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct hdmi_hdcp2p2_ctrl *ctrl =
		hdmi_get_featuredata_from_sysfs_dev(dev, HDMI_TX_FEAT_HDCP2P2);
	ssize_t ret;

	if (!ctrl) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	mutex_lock(&ctrl->mutex);
	if (ctrl->sink_status == SINK_CONNECTED)
		ret = scnprintf(buf, PAGE_SIZE, "Connected\n");
	else
		ret = scnprintf(buf, PAGE_SIZE, "Disconnected\n");
	mutex_unlock(&ctrl->mutex);
	return ret;
}

static ssize_t hdmi_hdcp2p2_sysfs_rda_trigger(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	struct hdmi_hdcp2p2_ctrl *ctrl =
		hdmi_get_featuredata_from_sysfs_dev(dev, HDMI_TX_FEAT_HDCP2P2);

	if (!ctrl) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	mutex_lock(&ctrl->mutex);
	if (ctrl->sink_status == SINK_CONNECTED)
		ret = scnprintf(buf, PAGE_SIZE, "Triggered\n");
	else
		ret = scnprintf(buf, PAGE_SIZE, "Not triggered\n");
	mutex_unlock(&ctrl->mutex);

	return ret;
}

static ssize_t hdmi_hdcp2p2_sysfs_wta_trigger(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct hdmi_hdcp2p2_ctrl *ctrl =
		hdmi_get_featuredata_from_sysfs_dev(dev, HDMI_TX_FEAT_HDCP2P2);

	if (!ctrl) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	mutex_lock(&ctrl->mutex);
	ctrl->sink_status = SINK_CONNECTED;
	mutex_unlock(&ctrl->mutex);

	pr_debug("HDCP 2.2 authentication triggered\n");
	hdmi_hdcp2p2_authenticate(ctrl);
	return count;
}

static ssize_t hdmi_hdcp2p2_sysfs_wta_min_level_change(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct hdmi_hdcp2p2_ctrl *ctrl =
		hdmi_get_featuredata_from_sysfs_dev(dev, HDMI_TX_FEAT_HDCP2P2);
	struct hdcp_lib_wakeup_data cdata = {
		HDCP_LIB_WKUP_CMD_QUERY_STREAM_TYPE};

	if (!ctrl) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	pr_debug("notification of minimum level change received\n");

	cdata.context = ctrl->lib_ctx;
	hdmi_hdcp2p2_wakeup_lib(ctrl, &cdata);

	return  count;
}

static void hdmi_hdcp2p2_auth_failed(struct hdmi_hdcp2p2_ctrl *ctrl)
{
	if (!ctrl) {
		pr_err("invalid input\n");
		return;
	}

	atomic_set(&ctrl->auth_state, HDCP_STATE_AUTH_FAIL);

	/* notify hdmi tx about HDCP failure */
	ctrl->init_data.notify_status(
		ctrl->init_data.cb_data,
		HDCP_STATE_AUTH_FAIL);
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

	rc = hdmi_ddc_read(ctrl->init_data.ddc_ctrl);
	if (rc)
		pr_err("Cannot read HDCP message register\n");
	return rc;
}

static int hdmi_hdcp2p2_ddc_write_message(struct hdmi_hdcp2p2_ctrl *ctrl,
	u8 *buf, size_t size)
{
	struct hdmi_tx_ddc_data ddc_data;
	int rc;

	memset(&ddc_data, 0, sizeof(ddc_data));
	ddc_data.dev_addr = HDCP_SINK_DDC_SLAVE_ADDR;
	ddc_data.offset = HDCP_SINK_DDC_HDCP2_WRITE_MESSAGE;
	ddc_data.data_buf = buf;
	ddc_data.data_len = size;
	ddc_data.retry = 1;
	ddc_data.what = "HDCP2WriteMessage";

	ctrl->init_data.ddc_ctrl->ddc_data = ddc_data;

	rc = hdmi_ddc_write(ctrl->init_data.ddc_ctrl);
	if (rc)
		pr_err("Cannot write HDCP message register");
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

static ssize_t hdmi_hdcp2p2_sysfs_rda_hdcp2_version(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	u8 hdcp2version;
	ssize_t ret;
	struct hdmi_hdcp2p2_ctrl *ctrl =
		hdmi_get_featuredata_from_sysfs_dev(dev, HDMI_TX_FEAT_HDCP2P2);

	if (!ctrl) {
		pr_err("invalid input\n");
		return -EINVAL;
	}
	ret = hdmi_hdcp2p2_read_version(ctrl, &hdcp2version);
	if (ret < 0)
		return ret;
	return snprintf(buf, PAGE_SIZE, "%u\n", hdcp2version);
}


static DEVICE_ATTR(trigger, S_IRUGO | S_IWUSR, hdmi_hdcp2p2_sysfs_rda_trigger,
		hdmi_hdcp2p2_sysfs_wta_trigger);
static DEVICE_ATTR(min_level_change, S_IWUSR, NULL,
		hdmi_hdcp2p2_sysfs_wta_min_level_change);
static DEVICE_ATTR(sink_status, S_IRUGO, hdmi_hdcp2p2_sysfs_rda_sink_status,
		NULL);
static DEVICE_ATTR(hdcp2_version, S_IRUGO, hdmi_hdcp2p2_sysfs_rda_hdcp2_version,
		NULL);

static struct attribute *hdmi_hdcp2p2_fs_attrs[] = {
	&dev_attr_trigger.attr,
	&dev_attr_min_level_change.attr,
	&dev_attr_sink_status.attr,
	&dev_attr_hdcp2_version.attr,
	NULL,
};

static struct attribute_group hdmi_hdcp2p2_fs_attr_group = {
	.name = "hdcp2p2",
	.attrs = hdmi_hdcp2p2_fs_attrs,
};

static bool hdmi_hdcp2p2_feature_supported(void *input)
{
	struct hdmi_hdcp2p2_ctrl *ctrl = input;
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

static void hdmi_hdcp2p2_send_msg_work(struct kthread_work *work)
{
	int rc = 0;
	struct hdmi_hdcp2p2_ctrl *ctrl = container_of(work,
		struct hdmi_hdcp2p2_ctrl, send_msg);
	struct hdcp_lib_wakeup_data cdata = {HDCP_LIB_WKUP_CMD_INVALID};

	if (!ctrl) {
		pr_err("invalid input\n");
		return;
	}

	mutex_lock(&ctrl->mutex);

	cdata.context = ctrl->lib_ctx;

	if (atomic_read(&ctrl->auth_state) == HDCP_STATE_INACTIVE) {
		pr_err("hdcp is off\n");
		goto exit;
	}

	/* Forward the message to the sink */
	rc = hdmi_hdcp2p2_ddc_write_message(ctrl,
		ctrl->send_msg_buf, (size_t)ctrl->send_msg_len);
	if (rc) {
		pr_err("Error sending msg to sink %d\n", rc);
		cdata.cmd = HDCP_LIB_WKUP_CMD_MSG_SEND_FAILED;
	} else {
		cdata.cmd = HDCP_LIB_WKUP_CMD_MSG_SEND_SUCCESS;
	}
exit:
	kzfree(ctrl->send_msg_buf);
	mutex_unlock(&ctrl->mutex);

	hdmi_hdcp2p2_wakeup_lib(ctrl, &cdata);
}

static void hdmi_hdcp2p2_recv_msg_work(struct kthread_work *work)
{
	int rc = 0;
	u64 mult;
	char *recvd_msg_buf = NULL;
	struct hdmi_hdcp2p2_ctrl *ctrl = container_of(work,
		struct hdmi_hdcp2p2_ctrl, recv_msg);
	struct hdmi_tx_hdcp2p2_ddc_data *ddc_data;
	struct hdmi_tx_ddc_ctrl *ddc_ctrl;
	struct msm_hdmi_mode_timing_info *timing;
	struct hdcp_lib_wakeup_data cdata = {HDCP_LIB_WKUP_CMD_INVALID};

	if (!ctrl) {
		pr_err("invalid input\n");
		return;
	}

	mutex_lock(&ctrl->mutex);

	cdata.context = ctrl->lib_ctx;

	if (atomic_read(&ctrl->auth_state) == HDCP_STATE_INACTIVE) {
		pr_err("hdcp is off\n");
		goto exit;
	}

	ddc_ctrl = ctrl->init_data.ddc_ctrl;
	if (!ddc_ctrl)
		goto exit;

	hdmi_ddc_config(ddc_ctrl);

	ddc_data = &ddc_ctrl->hdcp2p2_ddc_data;

	memset(ddc_data, 0, sizeof(*ddc_data));

	timing = ctrl->init_data.timing;
	mult = hdmi_tx_get_v_total(timing) / 20;
	ddc_data->intr_mask = RXSTATUS_MESSAGE_SIZE;
	ddc_data->timer_delay_lines = (u32)mult;
	ddc_data->read_method = HDCP2P2_RXSTATUS_HW_DDC_SW_TRIGGER;

	rc = hdmi_hdcp2p2_ddc_read_rxstatus(ddc_ctrl);
	if (rc) {
		pr_err("error reading rxstatus %d\n", rc);
		goto exit;
	}

	if (!ddc_data->message_size) {
		pr_err("recvd invalid message size\n");
		rc = -EINVAL;
		goto exit;
	}

	pr_debug("rxstatus msg size %d\n", ddc_data->message_size);

	recvd_msg_buf = kzalloc(ddc_data->message_size, GFP_KERNEL);
	if (!recvd_msg_buf)
		goto exit;

	rc = hdmi_hdcp2p2_ddc_read_message(ctrl, recvd_msg_buf,
		ddc_data->message_size, ctrl->timeout);
	if (rc)
		pr_err("error reading message %d\n", rc);

	cdata.cmd = HDCP_LIB_WKUP_CMD_MSG_RECV_SUCCESS;
	cdata.recvd_msg_buf = recvd_msg_buf;
	cdata.recvd_msg_len = ddc_data->message_size;
exit:
	if (rc == -ETIMEDOUT)
		cdata.cmd = HDCP_LIB_WKUP_CMD_MSG_RECV_TIMEOUT;
	else if (rc)
		cdata.cmd = HDCP_LIB_WKUP_CMD_MSG_RECV_FAILED;

	mutex_unlock(&ctrl->mutex);

	hdmi_hdcp2p2_wakeup_lib(ctrl, &cdata);
	kfree(recvd_msg_buf);
}

static void hdmi_hdcp2p2_auth_status_work(struct kthread_work *work)
{
	struct hdmi_hdcp2p2_ctrl *ctrl = container_of(work,
		struct hdmi_hdcp2p2_ctrl, status);

	if (!ctrl) {
		pr_err("invalid input\n");
		return;
	}

	mutex_lock(&ctrl->mutex);

	if (atomic_read(&ctrl->auth_state) == HDCP_STATE_INACTIVE) {
		pr_err("hdcp is off\n");
		goto exit;
	}

	if (ctrl->wakeup_cmd == HDMI_HDCP_WKUP_CMD_STATUS_FAILED) {
		hdmi_hdcp2p2_auth_failed(ctrl);
	} else if (ctrl->wakeup_cmd == HDMI_HDCP_WKUP_CMD_STATUS_SUCCESS) {
		/* Enable interrupts */
		u32 regval = DSS_REG_R(ctrl->init_data.core_io,
				HDMI_HDCP_INT_CTRL2);
		pr_debug("Now authenticated. Enabling interrupts\n");
		regval |= BIT(1);
		regval |= BIT(2);
		regval |= BIT(5);

		DSS_REG_W(ctrl->init_data.core_io,
			HDMI_HDCP_INT_CTRL2, regval);

		ctrl->init_data.notify_status(
			ctrl->init_data.cb_data,
			HDCP_STATE_AUTHENTICATED);
	}
exit:
	mutex_unlock(&ctrl->mutex);
}

static void hdmi_hdcp2p2_auth_work(struct kthread_work *work)
{
	struct hdmi_hdcp2p2_ctrl *ctrl = container_of(work,
		struct hdmi_hdcp2p2_ctrl, auth);
	struct hdcp_lib_wakeup_data cdata = {HDCP_LIB_WKUP_CMD_INVALID};

	if (!ctrl) {
		pr_err("invalid input\n");
		return;
	}

	mutex_lock(&ctrl->mutex);

	cdata.context = ctrl->lib_ctx;

	if (atomic_read(&ctrl->auth_state) == HDCP_STATE_AUTHENTICATING)
		cdata.cmd = HDCP_LIB_WKUP_CMD_START;
	else
		cdata.cmd = HDCP_LIB_WKUP_CMD_STOP;

	mutex_unlock(&ctrl->mutex);

	hdmi_hdcp2p2_wakeup_lib(ctrl, &cdata);
}

void hdmi_hdcp2p2_deinit(void *input)
{
	struct hdmi_hdcp2p2_ctrl *ctrl = (struct hdmi_hdcp2p2_ctrl *)input;
	struct hdcp_lib_wakeup_data cdata = {HDCP_LIB_WKUP_CMD_INVALID};

	if (!ctrl) {
		pr_err("invalid input\n");
		return;
	}

	cdata.cmd = HDCP_LIB_WKUP_CMD_STOP;
	cdata.context = ctrl->lib_ctx;
	hdmi_hdcp2p2_wakeup_lib(ctrl, &cdata);

	kthread_stop(ctrl->thread);

	sysfs_remove_group(ctrl->init_data.sysfs_kobj,
				&hdmi_hdcp2p2_fs_attr_group);

	mutex_destroy(&ctrl->mutex);
	kfree(ctrl);
}

void *hdmi_hdcp2p2_init(struct hdmi_hdcp_init_data *init_data)
{
	struct hdmi_hdcp2p2_ctrl *ctrl;
	int rc;
	static struct hdmi_hdcp_ops ops = {
		.hdmi_hdcp_reauthenticate = hdmi_hdcp2p2_reauthenticate,
		.hdmi_hdcp_authenticate = hdmi_hdcp2p2_authenticate,
		.feature_supported = hdmi_hdcp2p2_feature_supported,
		.hdmi_hdcp_off = hdmi_hdcp2p2_off
	};

	static struct hdcp_client_ops client_ops = {
		.wakeup = hdmi_hdcp2p2_wakeup,
	};

	static struct hdcp_txmtr_ops txmtr_ops;

	pr_debug("HDCP2P2 feature initialization\n");

	if (!init_data || !init_data->core_io || !init_data->mutex ||
		!init_data->ddc_ctrl || !init_data->notify_status ||
		!init_data->workq || !init_data->cb_data) {
		pr_err("invalid input\n");
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

	rc = sysfs_create_group(init_data->sysfs_kobj,
				&hdmi_hdcp2p2_fs_attr_group);
	if (rc) {
		pr_err("hdcp2p2 sysfs group creation failed\n");
		goto error;
	}

	init_completion(&ctrl->rxstatus_completion);

	ctrl->sink_status = SINK_DISCONNECTED;

	atomic_set(&ctrl->auth_state, HDCP_STATE_INACTIVE);

	ctrl->ops = &ops;
	mutex_init(&ctrl->mutex);
	mutex_init(&ctrl->wakeup_mutex);

	rc = hdcp_library_register(&ctrl->lib_ctx,
			&client_ops, ctrl->lib, ctrl);
	if (rc) {
		pr_err("Unable to register with HDCP 2.2 library\n");
		goto error;
	}

	init_kthread_worker(&ctrl->worker);

	init_kthread_work(&ctrl->auth,     hdmi_hdcp2p2_auth_work);
	init_kthread_work(&ctrl->send_msg, hdmi_hdcp2p2_send_msg_work);
	init_kthread_work(&ctrl->recv_msg, hdmi_hdcp2p2_recv_msg_work);
	init_kthread_work(&ctrl->status,   hdmi_hdcp2p2_auth_status_work);

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
	u8 hdcp2version;

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

struct hdmi_hdcp_ops *hdmi_hdcp2p2_start(void *input)
{
	struct hdmi_hdcp2p2_ctrl *ctrl = input;

	pr_debug("Checking sink capability\n");
	if (hdmi_hdcp2p2_supported(ctrl))
		return ctrl->ops;
	else
		return NULL;

}

