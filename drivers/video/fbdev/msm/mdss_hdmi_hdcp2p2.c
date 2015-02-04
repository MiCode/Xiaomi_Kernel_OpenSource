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

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/types.h>

#include "mdss_hdmi_hdcp.h"
#include "video/msm_hdmi_hdcp_mgr.h"
#include "mdss_hdmi_util.h"

static int hdcp2p2_authenticate(void *input);

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

struct hdcp2p2_message_map {
	int msg_id;
	const char *msg_name;
};

enum hdcp2p2_sink_status {
	SINK_DISCONNECTED,
	SINK_CONNECTED
};

enum hdcp2p2_sink_originated_message {
	START_AUTHENTICATION = 0,
	AKE_SEND_CERT_MESSAGE = 3,
	AKE_SEND_H_PRIME_MESSAGE = 7,
	AKE_SEND_PAIRING_INFO_MESSAGE = 8,
	LC_SEND_L_PRIME_MESSAGE = 10,
	ABORT_AUTHENTICATION = 99
};

enum hdcp2p2_tx_originated_message {
	AKE_INIT_MESSAGE = 2,
	AKE_NO_STORED_KM_MESSAGE = 4,
	AKE_STORED_KM_MESSAGE = 5,
	LC_INIT_MESSAGE = 9,
	SKE_SEND_EKS_MESSAGE = 11,
	AUTHENTICATION_FAILED = 100
};

/* Stores one single message from sink */
struct hdcp2p2_message {
	u8 *message_bytes; /* Message buffer */
	size_t msg_size; /* Byte size of the message buffer */
	struct list_head entry;
};

struct hdcp2p2_ctrl {
	enum hdmi_hdcp_state auth_state; /* Current auth message st */
	enum hdcp2p2_sink_status sink_status; /* Is sink connected */
	struct work_struct hdcp_sink_message_work; /* Polls sink for new msg */
	struct hdmi_hdcp_init_data init_data; /* Feature data from HDMI drv */
	enum hdcp2p2_sink_originated_message next_sink_message;
	enum hdcp2p2_tx_originated_message next_tx_message;
	struct list_head hdcp_sink_messages; /*Queue of msgs recd from sink */
	bool tx_has_master_key; /* true when TX has a stored Km for sink */
	struct mutex mutex; /* mutex to protect access to hdcp2p2_ctrl */
	struct hdmi_hdcp_ops *ops;
};

static const char *hdcp2p2_message_name(int msg_id)
{
	/*
	 * Message ID map. The first number indicates the message number
	 * assigned to the message by the HDCP 2.2 spec. This is also the first
	 * byte of every HDCP 2.2 authentication protocol message.
	 */
	static struct hdcp2p2_message_map hdcp2p2_msg_map[] = {
		{2, "AKE_INIT"},
		{3, "AKE_SEND_CERT"},
		{4, "AKE_NO_STORED_KM"},
		{5, "AKE_STORED_KM"},
		{7, "AKE_SEND_H_PRIME"},
		{8, "AKE_SEND_PAIRING_INFO"},
		{9, "LC_INIT"},
		{10, "LC_SEND_L_PRIME"},
		{11, "SKE_SEND_EKS"},
		{12, "REPEATER_AUTH_SEND_RECEIVERID_LIST"},
		{15, "REPEATER_AUTH_SEND_ACK"},
		{16, "REPEATER_AUTH_STREAM_MANAGE"},
		{17, "REPEATER_AUTH_STREAM_READY"},
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(hdcp2p2_msg_map); i++) {
		if (msg_id == hdcp2p2_msg_map[i].msg_id)
			return hdcp2p2_msg_map[i].msg_name;
	}
	return "UNKNOWN";
}

static ssize_t hdcp2p2_sysfs_rda_sink_status(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct hdcp2p2_ctrl *hdcp2p2_ctrl =
		hdmi_get_featuredata_from_sysfs_dev(dev, HDMI_TX_FEAT_HDCP2P2);
	ssize_t ret;

	if (!hdcp2p2_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&hdcp2p2_ctrl->mutex);
	if (hdcp2p2_ctrl->sink_status == SINK_CONNECTED)
		ret = scnprintf(buf, PAGE_SIZE, "Connected\n");
	else
		ret = scnprintf(buf, PAGE_SIZE, "Disconnected\n");
	mutex_unlock(&hdcp2p2_ctrl->mutex);
	return ret;
}

static ssize_t hdcp2p2_sysfs_rda_trigger(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	struct hdcp2p2_ctrl *hdcp2p2_ctrl =
		hdmi_get_featuredata_from_sysfs_dev(dev, HDMI_TX_FEAT_HDCP2P2);

	if (!hdcp2p2_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&hdcp2p2_ctrl->mutex);
	if (hdcp2p2_ctrl->sink_status == SINK_CONNECTED)
		ret = scnprintf(buf, PAGE_SIZE, "Triggered\n");
	else
		ret = scnprintf(buf, PAGE_SIZE, "Not triggered\n");
	mutex_unlock(&hdcp2p2_ctrl->mutex);

	return ret;
}

static ssize_t hdcp2p2_sysfs_wta_trigger(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct hdcp2p2_ctrl *hdcp2p2_ctrl =
		hdmi_get_featuredata_from_sysfs_dev(dev, HDMI_TX_FEAT_HDCP2P2);

	if (!hdcp2p2_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&hdcp2p2_ctrl->mutex);
	hdcp2p2_ctrl->sink_status = SINK_CONNECTED;
	mutex_unlock(&hdcp2p2_ctrl->mutex);

	DEV_DBG("%s: HDCP 2.2 authentication triggered\n", __func__);
	hdcp2p2_authenticate(hdcp2p2_ctrl);
	return count;
}

static void hdcp2p2_advance_next_tx_message(
		struct hdcp2p2_ctrl *hdcp2p2_ctrl)
{
	mutex_lock(&hdcp2p2_ctrl->mutex);
	switch (hdcp2p2_ctrl->next_tx_message) {
	case AKE_INIT_MESSAGE:
		hdcp2p2_ctrl->next_tx_message =
			AKE_NO_STORED_KM_MESSAGE;
		break;
	case AKE_NO_STORED_KM_MESSAGE:
	case AKE_STORED_KM_MESSAGE:
		hdcp2p2_ctrl->next_tx_message = LC_INIT_MESSAGE;
		break;
	case LC_INIT_MESSAGE:
		hdcp2p2_ctrl->next_tx_message = SKE_SEND_EKS_MESSAGE;
		break;
	default:
		break;
	}
	mutex_unlock(&hdcp2p2_ctrl->mutex);
}

static void hdcp2p2_advance_next_sink_message(
		struct hdcp2p2_ctrl *hdcp2p2_ctrl)
{
	mutex_lock(&hdcp2p2_ctrl->mutex);
	switch (hdcp2p2_ctrl->next_sink_message) {
	case START_AUTHENTICATION:
		hdcp2p2_ctrl->next_sink_message = AKE_SEND_CERT_MESSAGE;
		break;
	case AKE_SEND_CERT_MESSAGE:
		hdcp2p2_ctrl->next_sink_message =
			AKE_SEND_H_PRIME_MESSAGE;
		break;
	case AKE_SEND_H_PRIME_MESSAGE:
		if (hdcp2p2_ctrl->tx_has_master_key)
			hdcp2p2_ctrl->next_sink_message =
				LC_SEND_L_PRIME_MESSAGE;
		else
			hdcp2p2_ctrl->next_sink_message =
				AKE_SEND_PAIRING_INFO_MESSAGE;
		break;
	case AKE_SEND_PAIRING_INFO_MESSAGE:
		hdcp2p2_ctrl->next_sink_message =
			LC_SEND_L_PRIME_MESSAGE;
		break;
	default:
		break;
	}
	mutex_unlock(&hdcp2p2_ctrl->mutex);
}

static ssize_t hdcp2p2_sysfs_rda_message_from_sink(struct file *file,
		struct kobject *kobj, struct bin_attribute *attr, char *buffer,
		loff_t pos, size_t size)
{
	ssize_t ret;
	struct hdcp2p2_message *msg;
	struct list_head *prev, *next;
	struct device *dev = container_of(kobj, struct device, kobj);
	struct hdcp2p2_ctrl *hdcp2p2_ctrl =
		hdmi_get_featuredata_from_sysfs_dev(dev, HDMI_TX_FEAT_HDCP2P2);

	if (!hdcp2p2_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&hdcp2p2_ctrl->mutex);
	if (list_empty(&hdcp2p2_ctrl->hdcp_sink_messages)) {
		DEV_ERR("%s: No message is available from sink\n", __func__);
		mutex_unlock(&hdcp2p2_ctrl->mutex);
		return -EINVAL;
	}
	list_for_each_safe(prev, next, &hdcp2p2_ctrl->hdcp_sink_messages) {
		msg = list_entry(prev, struct hdcp2p2_message, entry);
		if (size < msg->msg_size) {
			DEV_ERR("%s: buffer size not enough\n", __func__);
			mutex_unlock(&hdcp2p2_ctrl->mutex);
			return -EINVAL;
		}
		break;
	}

	list_del(&msg->entry);
	mutex_unlock(&hdcp2p2_ctrl->mutex);

	memmove(buffer, msg->message_bytes, msg->msg_size);
	ret = msg->msg_size;
	kfree(msg->message_bytes);
	kfree(msg);

	if (hdcp2p2_ctrl->next_sink_message == AKE_SEND_H_PRIME_MESSAGE) {
		if (!hdcp2p2_ctrl->tx_has_master_key) {
			DEV_DBG("%s: Listening for second message\n", __func__);
			hdcp2p2_advance_next_sink_message(hdcp2p2_ctrl);
			schedule_work(&hdcp2p2_ctrl->hdcp_sink_message_work);
		}
	}

	return ret;
}

static int hdcp2p2_ddc_read_message(struct hdmi_tx_ddc_ctrl *ddc_ctrl, u8 *buf,
		int size)
{
	struct hdmi_tx_ddc_data ddc_data;
	int rc;

	memset(&ddc_data, 0, sizeof(ddc_data));
	ddc_data.dev_addr = HDCP_SINK_DDC_SLAVE_ADDR;
	ddc_data.offset = HDCP_SINK_DDC_HDCP2_READ_MESSAGE;
	ddc_data.data_buf = buf;
	ddc_data.data_len = size;
	ddc_data.request_len = size;
	ddc_data.retry = 1;
	ddc_data.what = "HDCP2ReadMessage";

	rc = hdmi_ddc_read(ddc_ctrl, &ddc_data);
	if (rc)
		DEV_ERR("%s: Cannot read HDCP message register", __func__);
	return rc;
}

static int hdcp2p2_ddc_write_message(struct hdmi_tx_ddc_ctrl *ddc_ctrl, u8 *buf,
		size_t size)
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

	rc = hdmi_ddc_write(ddc_ctrl, &ddc_data);
	if (rc)
		DEV_ERR("%s: Cannot write HDCP message register", __func__);
	return rc;
}

static int hdcp2p2_read_version(struct hdmi_tx_ddc_ctrl *ddc_ctrl,
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

	rc = hdmi_ddc_read(ddc_ctrl, &ddc_data);
	if (rc) {
		DEV_ERR("%s: Cannot read HDCP2Version register", __func__);
		return rc;
	}

	DEV_DBG("%s: Read HDCP2Version as %u\n", __func__, *hdcp2version);
	return rc;
}

/**
 * Work item that polls the sink for an incoming message.
 * Since messages from sink during auth are always in response to messages
 * sent by TX, we know when to expect sink messages, and thus polling is
 * required by the HDCP 2.2 spec
 */
static void hdcp2p2_sink_message_work(struct work_struct *work)
{
	struct hdcp2p2_ctrl *hdcp2p2_ctrl = container_of(work,
			struct hdcp2p2_ctrl, hdcp_sink_message_work);
	struct hdmi_tx_ddc_data ddc_data;
	struct hdmi_tx_ddc_ctrl *ddc_ctrl = hdcp2p2_ctrl->init_data.ddc_ctrl;
	int rc;
	u16 rx_status;
	int msg_size;
	int wait_cycles = 50; /* 50 20 ms cycles of wait = 1 second total */

	do {
		memset(&ddc_data, 0, sizeof(ddc_data));
		ddc_data.dev_addr = HDCP_SINK_DDC_SLAVE_ADDR;
		ddc_data.offset = HDCP_SINK_DDC_HDCP2_RXSTATUS;
		ddc_data.data_buf = (u8 *)&rx_status;
		ddc_data.data_len = 2;
		ddc_data.request_len = 2;
		ddc_data.retry = 1;
		ddc_data.what = "HDCP2RxStatus";

		rc = hdmi_ddc_read(ddc_ctrl, &ddc_data);
		if (rc) {
			DEV_ERR("%s: Error %d in read HDCP RX status register",
					__func__, rc);
			return;
		}

		msg_size = rx_status & 0x3FF;
		if (msg_size) {
			DEV_DBG("%s: Message available at sink, size %d\n",
					__func__, msg_size);
			break;
		}

		msleep(20);

	} while (--wait_cycles);

	if (!wait_cycles) {
		DEV_ERR("%s: Timeout in waiting for sink\n", __func__);
	} else {
		/* Read message from sink now */
		struct list_head *pos;
		struct hdcp2p2_message *message = kmalloc(sizeof(*message),
				GFP_KERNEL);
		if (!message) {
			DEV_ERR("%s: Could not allocate memory\n", __func__);
			return;
		}
		message->message_bytes = kmalloc(msg_size, GFP_KERNEL);
		if (!message->message_bytes) {
			DEV_ERR("%s: Could not allocate memory\n", __func__);
			kfree(message);
			return;
		}

		hdcp2p2_ddc_read_message(ddc_ctrl, message->message_bytes,
				msg_size);
		message->msg_size = msg_size;

		INIT_LIST_HEAD(&message->entry);
		list_for_each(pos, &hdcp2p2_ctrl->hdcp_sink_messages);
		list_add_tail(&message->entry, pos);
		DEV_DBG("%s: Pinging transmitter for message_available\n",
				__func__);
		sysfs_notify(hdcp2p2_ctrl->init_data.sysfs_kobj, "hdcp2p2",
				"message_available");
	}
}

static ssize_t hdcp2p2_sysfs_wta_message_to_sink(struct file *file,
		struct kobject *kobj, struct bin_attribute *attr, char *buffer,
		loff_t pos, size_t size)
{
	/* In here, we need to read the message from userspace, and write it
	 * out to the sink via DDC
	 */
	int rc;
	bool authenticated = false;
	struct device *dev = container_of(kobj, struct device, kobj);
	struct hdcp2p2_ctrl *hdcp2p2_ctrl =
		hdmi_get_featuredata_from_sysfs_dev(dev, HDMI_TX_FEAT_HDCP2P2);

	if (!hdcp2p2_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	DEV_DBG("%s: Received %s message from tx\n", __func__,
			hdcp2p2_message_name((int)buffer[0]));

	switch (buffer[0]) {
	case AKE_STORED_KM_MESSAGE:
		mutex_lock(&hdcp2p2_ctrl->mutex);
		hdcp2p2_ctrl->tx_has_master_key = true;
		mutex_unlock(&hdcp2p2_ctrl->mutex);
		break;
	case SKE_SEND_EKS_MESSAGE:
		/* Send EKS message comes from TX when we are authenticated */
		authenticated = true;
		mutex_lock(&hdcp2p2_ctrl->mutex);
		hdcp2p2_ctrl->auth_state = HDCP_STATE_AUTHENTICATED;
		hdcp2p2_ctrl->next_sink_message = START_AUTHENTICATION;
		hdcp2p2_ctrl->next_tx_message = AKE_INIT_MESSAGE;
		hdcp2p2_ctrl->tx_has_master_key = false;
		mutex_unlock(&hdcp2p2_ctrl->mutex);
		hdcp2p2_ctrl->init_data.notify_status(
				hdcp2p2_ctrl->init_data.cb_data,
				HDCP_STATE_AUTHENTICATED);
		break;
	case AUTHENTICATION_FAILED:
		mutex_lock(&hdcp2p2_ctrl->mutex);
		hdcp2p2_ctrl->auth_state = HDCP_STATE_AUTH_FAIL;
		hdcp2p2_ctrl->next_sink_message = START_AUTHENTICATION;
		hdcp2p2_ctrl->next_tx_message = AKE_INIT_MESSAGE;
		hdcp2p2_ctrl->tx_has_master_key = false;
		mutex_unlock(&hdcp2p2_ctrl->mutex);
		hdcp2p2_ctrl->init_data.notify_status(
				hdcp2p2_ctrl->init_data.cb_data,
				HDCP_STATE_AUTH_FAIL);
		break;
	default:
		hdcp2p2_advance_next_sink_message(hdcp2p2_ctrl);
		hdcp2p2_advance_next_tx_message(hdcp2p2_ctrl);
		break;
	}

	/* Forward the message to the sink */
	rc = hdcp2p2_ddc_write_message(hdcp2p2_ctrl->init_data.ddc_ctrl,
			(u8 *)buffer, size);
	if (rc) {
		DEV_ERR("%s: Error in writing to sink %d\n", __func__, rc);
		return rc;
	}

	/* Start polling sink for the next expected message in the protocol */
	if (!authenticated)
		schedule_work(&hdcp2p2_ctrl->hdcp_sink_message_work);
	return size;
}

static ssize_t hdcp2p2_sysfs_rda_message_available(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	struct hdcp2p2_ctrl *hdcp2p2_ctrl =
		hdmi_get_featuredata_from_sysfs_dev(dev, HDMI_TX_FEAT_HDCP2P2);
	mutex_lock(&hdcp2p2_ctrl->mutex);
	if (list_empty(&hdcp2p2_ctrl->hdcp_sink_messages))
		ret = snprintf(buf, PAGE_SIZE, "Unavailable\n");
	else
		ret = snprintf(buf, PAGE_SIZE, "Available\n");
	mutex_unlock(&hdcp2p2_ctrl->mutex);
	return ret;
}

static ssize_t hdcp2p2_sysfs_rda_hdcp2_version(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	u8 hdcp2version;
	ssize_t ret;
	struct hdcp2p2_ctrl *hdcp2p2_ctrl =
		hdmi_get_featuredata_from_sysfs_dev(dev, HDMI_TX_FEAT_HDCP2P2);

	if (!hdcp2p2_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}
	ret = hdcp2p2_read_version(hdcp2p2_ctrl->init_data.ddc_ctrl,
			&hdcp2version);
	if (ret < 0)
		return ret;
	return snprintf(buf, PAGE_SIZE, "%u\n", hdcp2version);
}


static DEVICE_ATTR(trigger, S_IRUGO | S_IWUSR, hdcp2p2_sysfs_rda_trigger,
		hdcp2p2_sysfs_wta_trigger);
static DEVICE_ATTR(sink_status, S_IRUGO, hdcp2p2_sysfs_rda_sink_status,
		NULL);
static DEVICE_ATTR(message_available, S_IRUGO,
		hdcp2p2_sysfs_rda_message_available,
		NULL);
static DEVICE_ATTR(hdcp2_version, S_IRUGO,
		hdcp2p2_sysfs_rda_hdcp2_version,
		NULL);

static struct bin_attribute message_attr = {
	.attr.name = "message",
	.attr.mode = 0600,
	.size = 0,
	.read = hdcp2p2_sysfs_rda_message_from_sink,
	.write = hdcp2p2_sysfs_wta_message_to_sink,
};

static struct attribute *hdcp2p2_fs_attrs[] = {
	&dev_attr_trigger.attr,
	&dev_attr_sink_status.attr,
	&dev_attr_message_available.attr,
	&dev_attr_hdcp2_version.attr,
	NULL,
};

static struct bin_attribute *hdcp2p2_fs_bin_attrs[] = {
	&message_attr
};

static struct attribute_group hdcp2p2_fs_attr_group = {
	.name = "hdcp2p2",
	.attrs = hdcp2p2_fs_attrs,
	.bin_attrs = hdcp2p2_fs_bin_attrs,
};


static int hdcp2p2_isr(void *input)
{
	struct hdcp2p2_ctrl *hdcp2p2_ctrl = input;
	if (!hdcp2p2_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}
	return 0;
}


static void hdcp2p2_reset(struct hdcp2p2_ctrl *hdcp2p2_ctrl)
{
	mutex_lock(&hdcp2p2_ctrl->mutex);
	hdcp2p2_ctrl->sink_status = SINK_DISCONNECTED;
	hdcp2p2_ctrl->next_sink_message = START_AUTHENTICATION;
	hdcp2p2_ctrl->next_tx_message = AKE_INIT_MESSAGE;
	hdcp2p2_ctrl->auth_state = HDCP_STATE_INACTIVE;
	hdcp2p2_ctrl->tx_has_master_key = false;
	mutex_unlock(&hdcp2p2_ctrl->mutex);
}


static int hdcp2p2_authenticate(void *input)
{
	struct hdcp2p2_ctrl *hdcp2p2_ctrl = input;

	mutex_lock(&hdcp2p2_ctrl->mutex);
	hdcp2p2_ctrl->sink_status = SINK_CONNECTED;
	hdcp2p2_ctrl->auth_state = HDCP_STATE_AUTHENTICATING;
	mutex_unlock(&hdcp2p2_ctrl->mutex);

	/*
	 * Notify HDCP Transmitter that an HDCP2.2 capable sink has been
	 * connected
	 */
	sysfs_notify(hdcp2p2_ctrl->init_data.sysfs_kobj, "hdcp2p2",
			"sink_status");
	return 0;
}

static int hdcp2p2_reauthenticate(void *input)
{
	hdcp2p2_reset((struct hdcp2p2_ctrl *)input);
	return hdcp2p2_authenticate(input);
}

static void hdcp2p2_off(void *input)
{
	hdcp2p2_reset((struct hdcp2p2_ctrl *)input);
}

void *hdmi_hdcp2p2_init(struct hdmi_hdcp_init_data *init_data)
{
	struct hdcp2p2_ctrl *hdcp2p2_ctrl;
	int rc;
	static struct hdmi_hdcp_ops ops = {
		.hdmi_hdcp_isr = hdcp2p2_isr,
		.hdmi_hdcp_reauthenticate = hdcp2p2_reauthenticate,
		.hdmi_hdcp_authenticate = hdcp2p2_authenticate,
		.hdmi_hdcp_off = hdcp2p2_off
	};

	DEV_DBG("%s: HDCP2P2 feature initialization\n", __func__);

	if (!init_data || !init_data->core_io || !init_data->mutex ||
		!init_data->ddc_ctrl || !init_data->notify_status ||
		!init_data->workq || !init_data->cb_data) {
		DEV_ERR("%s: invalid input\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	if (init_data->hdmi_tx_ver < MIN_HDMI_TX_MAJOR_VERSION) {
		DEV_DBG("%s: HDMI Tx does not support HDCP 2.2\n", __func__);
		return ERR_PTR(-ENODEV);
	}

	hdcp2p2_ctrl = kzalloc(sizeof(*hdcp2p2_ctrl), GFP_KERNEL);
	if (!hdcp2p2_ctrl) {
		DEV_ERR("%s: Out of memory\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	hdcp2p2_ctrl->init_data = *init_data;

	rc = sysfs_create_group(init_data->sysfs_kobj,
				&hdcp2p2_fs_attr_group);
	if (rc) {
		DEV_ERR("%s: hdcp2p2 sysfs group creation failed\n", __func__);
		goto error;
	}
	INIT_WORK(&hdcp2p2_ctrl->hdcp_sink_message_work,
			hdcp2p2_sink_message_work);
	INIT_LIST_HEAD(&hdcp2p2_ctrl->hdcp_sink_messages);

	hdcp2p2_ctrl->sink_status = SINK_DISCONNECTED;

	hdcp2p2_ctrl->next_sink_message = START_AUTHENTICATION;
	hdcp2p2_ctrl->next_tx_message = AKE_INIT_MESSAGE;
	hdcp2p2_ctrl->tx_has_master_key = false;
	hdcp2p2_ctrl->auth_state = HDCP_STATE_INACTIVE;
	hdcp2p2_ctrl->ops = &ops;
	mutex_init(&hdcp2p2_ctrl->mutex);

	return hdcp2p2_ctrl;

error:
	kfree(hdcp2p2_ctrl);
	return ERR_PTR(rc);
}

void hdmi_hdcp2p2_deinit(void *input)
{
	struct hdcp2p2_ctrl *hdcp2p2_ctrl = (struct hdcp2p2_ctrl *)input;
	struct list_head *node, *next;
	struct hdcp2p2_message *msg;

	if (!hdcp2p2_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}
	sysfs_remove_group(hdcp2p2_ctrl->init_data.sysfs_kobj,
				&hdcp2p2_fs_attr_group);

	if (!list_empty(&hdcp2p2_ctrl->hdcp_sink_messages))
		DEV_WARN("%s: Sink msg list is not empty\n", __func__);

	list_for_each_safe(node, next, &hdcp2p2_ctrl->hdcp_sink_messages) {
		msg = list_entry(node, struct hdcp2p2_message, entry);
		kfree(msg->message_bytes);
		kfree(msg);
	}

	mutex_destroy(&hdcp2p2_ctrl->mutex);
	kfree(hdcp2p2_ctrl);
}

static bool hdcp2p2_supported(struct hdmi_tx_ddc_ctrl *ddc_ctrl)
{
	u8 hdcp2version;

	int rc = hdcp2p2_read_version(ddc_ctrl, &hdcp2version);
	if (rc)
		goto error;

	if (hdcp2version & BIT(2)) {
		DEV_DBG("%s: Sink is HDCP 2.2 capable\n", __func__);
		return true;
	}

error:
	DEV_DBG("%s: Sink is not HDCP 2.2 capable\n", __func__);
	return false;
}

struct hdmi_hdcp_ops *hdmi_hdcp2p2_start(void *input)
{
	struct hdcp2p2_ctrl *ctrl = input;

	DEV_DBG("%s: Checking sink capability\n", __func__);
	if (hdcp2p2_supported(ctrl->init_data.ddc_ctrl))
		return ctrl->ops;
	else
		return NULL;
}

