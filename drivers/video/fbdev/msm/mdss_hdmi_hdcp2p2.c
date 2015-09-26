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
};

enum hdcp2p2_tx_originated_message {
	AKE_INIT_MESSAGE = 2,
	AKE_NO_STORED_KM_MESSAGE = 4,
	AKE_STORED_KM_MESSAGE = 5,
	LC_INIT_MESSAGE = 9,
	SKE_SEND_EKS_MESSAGE = 11,
};

/* Stores one single message from sink */
struct hdcp2p2_message {
	u8 *message_bytes; /* Message buffer */
	size_t msg_size; /* Byte size of the message buffer */
};

struct hdcp2p2_ctrl {
	enum hdmi_hdcp_state auth_state; /* Current auth message st */
	enum hdcp2p2_sink_status sink_status; /* Is sink connected */
	struct hdmi_hdcp_init_data init_data; /* Feature data from HDMI drv */
	enum hdcp2p2_sink_originated_message next_sink_message;
	enum hdcp2p2_tx_originated_message next_tx_message;
	bool tx_has_master_key; /* true when TX has a stored Km for sink */
	struct mutex mutex; /* mutex to protect access to hdcp2p2_ctrl */
	struct hdmi_hdcp_ops *ops;
	void *hdcp_lib_handle; /* Handle to HDCP 2.2 Trustzone library */
	struct hdcp_txmtr_ops *txmtr_ops; /* Ops for driver to call into TZ */
	struct hdcp_client_ops *client_ops; /* Ops for driver to export to TZ */
	struct completion rxstatus_completion; /* Rx status interrupt */
};

static int hdcp2p2_authenticate(void *input);

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

static ssize_t hdcp2p2_sysfs_wta_min_level_change(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct hdcp2p2_ctrl *hdcp2p2_ctrl =
		hdmi_get_featuredata_from_sysfs_dev(dev, HDMI_TX_FEAT_HDCP2P2);
	int res;

	if (!hdcp2p2_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	DEV_DBG("%s: notification of minimum level change received\n",
						__func__);
	res = hdcp2p2_ctrl->txmtr_ops->
		hdcp_txmtr_query_stream_type(hdcp2p2_ctrl->hdcp_lib_handle);

	return  count;
}

static void hdcp2p2_auth_failed(struct hdcp2p2_ctrl *hdcp2p2_ctrl)
{
	if (!hdcp2p2_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	hdcp2p2_ctrl->auth_state = HDCP_STATE_AUTH_FAIL;
	hdcp2p2_ctrl->next_sink_message = START_AUTHENTICATION;
	hdcp2p2_ctrl->next_tx_message = AKE_INIT_MESSAGE;
	hdcp2p2_ctrl->tx_has_master_key = false;

	/* notify hdmi tx about HDCP failure */
	hdcp2p2_ctrl->init_data.notify_status(
		hdcp2p2_ctrl->init_data.cb_data,
		HDCP_STATE_AUTH_FAIL);
}

static void hdcp2p2_advance_next_tx_message(
		struct hdcp2p2_ctrl *hdcp2p2_ctrl)
{
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
}

static void hdcp2p2_advance_next_sink_message(
		struct hdcp2p2_ctrl *hdcp2p2_ctrl)
{
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
}

static int hdcp2p2_ddc_read_message(struct hdcp2p2_ctrl *hdcp2p2_ctrl, u8 *buf,
		int size, u32 timeout)
{
	struct hdmi_tx_ddc_data ddc_data;
	int rc;

	memset(&ddc_data, 0, sizeof(ddc_data));
	ddc_data.dev_addr = HDCP_SINK_DDC_SLAVE_ADDR;
	ddc_data.offset = HDCP_SINK_DDC_HDCP2_READ_MESSAGE;
	ddc_data.data_buf = buf;
	ddc_data.data_len = size;
	ddc_data.request_len = size;
	ddc_data.retry = 0;
	ddc_data.hard_timeout = timeout;
	ddc_data.what = "HDCP2ReadMessage";

	rc = hdmi_ddc_read(hdcp2p2_ctrl->init_data.ddc_ctrl, &ddc_data);
	if (rc)
		DEV_ERR("%s: Cannot read HDCP message register", __func__);
	return rc;
}

static int hdcp2p2_ddc_write_message(struct hdcp2p2_ctrl *hdcp2p2_ctrl, u8 *buf,
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

	rc = hdmi_ddc_write(hdcp2p2_ctrl->init_data.ddc_ctrl, &ddc_data);
	if (rc)
		DEV_ERR("%s: Cannot write HDCP message register", __func__);
	return rc;
}

static void hdcp2p2_ddc_abort(struct hdcp2p2_ctrl *hdcp2p2_ctrl)
{
	/* Abort any ongoing DDC transactions */
	struct hdmi_tx_ddc_data ddc_data;

	memset(&ddc_data, 0, sizeof(ddc_data));
	ddc_data.retry = 1;
	ddc_data.what = "HDCPAbortTransaction";
	hdmi_ddc_abort_transaction(hdcp2p2_ctrl->init_data.ddc_ctrl,
		&ddc_data);
}

static int hdcp2p2_read_version(struct hdcp2p2_ctrl *hdcp2p2_ctrl,
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

	rc = hdmi_ddc_read(hdcp2p2_ctrl->init_data.ddc_ctrl, &ddc_data);
	if (rc) {
		DEV_ERR("%s: Cannot read HDCP2Version register", __func__);
		return rc;
	}

	DEV_DBG("%s: Read HDCP2Version as %u\n", __func__, *hdcp2version);
	return rc;
}

static int hdcp2p2_read_message_from_sink(struct hdcp2p2_ctrl *hdcp2p2_ctrl,
	int msg_size, u32 timeout)
{
	bool read_next_message = false;
	int rc = 0;
	struct hdcp2p2_message *message = kmalloc(sizeof(*message),
			GFP_KERNEL);

	if (!message) {
		DEV_ERR("%s: Could not allocate memory\n", __func__);
		goto exit;
	}

	message->message_bytes = kmalloc(msg_size, GFP_KERNEL);
	if (!message->message_bytes)
		goto exit;

	DEV_DBG("%s: reading message from sink\n", __func__);
	rc = hdcp2p2_ddc_read_message(hdcp2p2_ctrl, message->message_bytes,
		msg_size, timeout);
	if (rc) {
		DEV_ERR("%s: ERROR reading message from sink\n", __func__);
		goto exit;
	}

	message->msg_size = msg_size;
	DEV_DBG("%s: msg recvd: %s, size: %d\n", __func__,
		hdcp2p2_message_name((int)message->message_bytes[0]),
		msg_size);

	if (hdcp2p2_ctrl->txmtr_ops->process_message) {
		rc = hdcp2p2_ctrl->txmtr_ops->process_message(
			hdcp2p2_ctrl->hdcp_lib_handle, message->message_bytes,
			(u32)message->msg_size);
		if (rc)
			goto exit;
	} else {
		DEV_ERR("%s: process message not defined\n", __func__);
		rc = -EINVAL;
		goto exit;
	}

	if (hdcp2p2_ctrl->next_sink_message == AKE_SEND_H_PRIME_MESSAGE) {
		if (!hdcp2p2_ctrl->tx_has_master_key) {
			DEV_DBG("%s: Listening for second message\n", __func__);
			hdcp2p2_advance_next_sink_message(hdcp2p2_ctrl);
			read_next_message = true;
		}
	}

	if (read_next_message)
		rc = 1;
exit:
	kfree(message->message_bytes);
	kfree(message);

	return rc;
}

static int hdcp2p2_sink_message_read(struct hdcp2p2_ctrl *hdcp2p2_ctrl,
		u32 timeout)
{
	struct hdmi_tx_hdcp2p2_ddc_data hdcp2p2_ddc_data;
	struct hdmi_tx_ddc_ctrl *ddc_ctrl;
	int rc = 0;
	int msg_size;
	bool read_next_message = false;
	u64 mult;
	struct msm_hdmi_mode_timing_info *timing;

	if (!hdcp2p2_ctrl) {
		DEV_ERR("%s: invalid hdcp2p2 data\n", __func__);
		return -EINVAL;
	}

	ddc_ctrl = hdcp2p2_ctrl->init_data.ddc_ctrl;

	do {
		timing = hdcp2p2_ctrl->init_data.timing;

		mult = hdmi_tx_get_v_total(timing) / 20;
		memset(&hdcp2p2_ddc_data, 0, sizeof(hdcp2p2_ddc_data));
		hdcp2p2_ddc_data.ddc_data.what = "HDCP2RxStatus";
		hdcp2p2_ddc_data.ddc_data.data_buf = (u8 *)&msg_size;
		hdcp2p2_ddc_data.ddc_data.data_len = sizeof(msg_size);
		hdcp2p2_ddc_data.rxstatus_field = RXSTATUS_MESSAGE_SIZE;
		hdcp2p2_ddc_data.timer_delay_lines = (u32)mult;
		hdcp2p2_ddc_data.irq_wait_count = 100;
		hdcp2p2_ddc_data.poll_sink = false;

		hdmi_ddc_config(ddc_ctrl);
		DEV_DBG("%s: Reading rxstatus, timer delay %u\n",
			__func__, (u32)mult);

		rc = hdmi_hdcp2p2_ddc_read_rxstatus(
			ddc_ctrl, &hdcp2p2_ddc_data,
			&hdcp2p2_ctrl->rxstatus_completion);
		if (rc) {
			DEV_ERR("%s: Could not read rxstatus from sink\n",
				__func__);
			goto exit;
		} else {
			DEV_DBG("%s: SUCCESS reading rxstatus\n", __func__);
		}

		if (!msg_size) {
			DEV_ERR("%s: recvd invalid message size\n", __func__);
			rc = -EINVAL;
			goto exit;
		}

		rc = hdcp2p2_read_message_from_sink(
			hdcp2p2_ctrl, msg_size, timeout);
		if (rc > 0) {
			read_next_message = true;
			rc = 0;
		}

		hdcp2p2_ddc_abort(hdcp2p2_ctrl);
		hdmi_hdcp2p2_ddc_reset(ddc_ctrl);
		hdmi_hdcp2p2_ddc_disable(ddc_ctrl);
	} while (read_next_message);
exit:
	return rc;
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
	ret = hdcp2p2_read_version(hdcp2p2_ctrl, &hdcp2version);
	if (ret < 0)
		return ret;
	return snprintf(buf, PAGE_SIZE, "%u\n", hdcp2version);
}


static DEVICE_ATTR(trigger, S_IRUGO | S_IWUSR, hdcp2p2_sysfs_rda_trigger,
		hdcp2p2_sysfs_wta_trigger);
static DEVICE_ATTR(min_level_change, S_IWUSR, NULL,
		hdcp2p2_sysfs_wta_min_level_change);
static DEVICE_ATTR(sink_status, S_IRUGO, hdcp2p2_sysfs_rda_sink_status,
		NULL);
static DEVICE_ATTR(hdcp2_version, S_IRUGO,
		hdcp2p2_sysfs_rda_hdcp2_version,
		NULL);


static struct attribute *hdcp2p2_fs_attrs[] = {
	&dev_attr_trigger.attr,
	&dev_attr_min_level_change.attr,
	&dev_attr_sink_status.attr,
	&dev_attr_hdcp2_version.attr,
	NULL,
};

static struct attribute_group hdcp2p2_fs_attr_group = {
	.name = "hdcp2p2",
	.attrs = hdcp2p2_fs_attrs,
};


static int hdcp2p2_isr(void *input)
{
	struct hdcp2p2_ctrl *hdcp2p2_ctrl = input;
	u32 reg_val;

	if (!hdcp2p2_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	DEV_DBG("%s\n INT_CTRL0 is 0x%x\n", __func__,
		DSS_REG_R(hdcp2p2_ctrl->init_data.core_io, HDMI_DDC_INT_CTRL0));
	reg_val = DSS_REG_R(hdcp2p2_ctrl->init_data.core_io,
			HDMI_HDCP_INT_CTRL2);
	if (reg_val & BIT(0)) {
		DEV_DBG("%s: HDCP 2.2 Encryption is enabled\n", __func__);
		reg_val |= BIT(1);
		DSS_REG_W(hdcp2p2_ctrl->init_data.core_io, HDMI_HDCP_INT_CTRL2,
			reg_val);
	}

	reg_val = DSS_REG_R(hdcp2p2_ctrl->init_data.core_io,
							HDMI_DDC_INT_CTRL0);
	if (reg_val & HDCP2P2_RXSTATUS_MESSAGE_SIZE_MASK) {
		DSS_REG_W(hdcp2p2_ctrl->init_data.core_io, HDMI_DDC_INT_CTRL0,
						reg_val & ~(BIT(31)));
		if (!completion_done(&hdcp2p2_ctrl->rxstatus_completion))
			complete_all(&hdcp2p2_ctrl->rxstatus_completion);
	} else if (reg_val & BIT(8)) {
		DSS_REG_W(hdcp2p2_ctrl->init_data.core_io, HDMI_DDC_INT_CTRL0,
						reg_val & ~(BIT(9) | BIT(10)));
		if (!completion_done(&hdcp2p2_ctrl->rxstatus_completion))
			complete_all(&hdcp2p2_ctrl->rxstatus_completion);
	}

	return 0;
}

static bool hdcp2p2_feature_supported(void *input)
{
	struct hdcp2p2_ctrl *hdcp2p2_ctrl = input;
	struct hdcp_txmtr_ops *lib = NULL;
	bool supported = false;

	if (!hdcp2p2_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		goto end;
	}

	lib = hdcp2p2_ctrl->txmtr_ops;
	if (!lib) {
		DEV_ERR("%s: invalid lib ops data\n", __func__);
		goto end;
	}

	if (lib->feature_supported)
		supported = lib->feature_supported(
			hdcp2p2_ctrl->hdcp_lib_handle);
end:
	return supported;
}

static void hdcp2p2_reset(struct hdcp2p2_ctrl *hdcp2p2_ctrl)
{
	if (!hdcp2p2_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	hdcp2p2_ctrl->sink_status = SINK_DISCONNECTED;
	hdcp2p2_ctrl->next_sink_message = START_AUTHENTICATION;
	hdcp2p2_ctrl->next_tx_message = AKE_INIT_MESSAGE;
	hdcp2p2_ctrl->auth_state = HDCP_STATE_INACTIVE;
	hdcp2p2_ctrl->tx_has_master_key = false;
}

static int hdcp2p2_authenticate(void *input)
{
	struct hdcp2p2_ctrl *hdcp2p2_ctrl = input;
	struct hdcp_txmtr_ops *lib = NULL;
	int rc = 0;

	mutex_lock(&hdcp2p2_ctrl->mutex);
	hdcp2p2_ctrl->sink_status = SINK_CONNECTED;
	hdcp2p2_ctrl->auth_state = HDCP_STATE_AUTHENTICATING;

	lib = hdcp2p2_ctrl->txmtr_ops;
	if (!lib) {
		DEV_ERR("%s: invalid lib ops data\n", __func__);
		goto done;
	}

	if (!lib->start) {
		DEV_ERR("%s: lib start not defined\n", __func__);
		goto done;
	}

	rc = lib->start(hdcp2p2_ctrl->hdcp_lib_handle);
	if (rc) {
		DEV_ERR("%s: lib start failed\n", __func__);
		hdcp2p2_ctrl->auth_state = HDCP_STATE_AUTH_FAIL;

		goto done;
	}
done:
	mutex_unlock(&hdcp2p2_ctrl->mutex);
	return rc;
}

static int hdcp2p2_reauthenticate(void *input)
{
	struct hdcp2p2_ctrl *hdcp2p2_ctrl = (struct hdcp2p2_ctrl *)input;

	if (!hdcp2p2_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	hdcp2p2_reset((struct hdcp2p2_ctrl *)input);

	return hdcp2p2_authenticate(input);
}

static void hdcp2p2_off(void *input)
{
	struct hdcp2p2_ctrl *hdcp2p2_ctrl = (struct hdcp2p2_ctrl *)input;
	struct hdcp_txmtr_ops *lib = NULL;

	if (!hdcp2p2_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	mutex_lock(&hdcp2p2_ctrl->mutex);
	hdcp2p2_reset(hdcp2p2_ctrl);

	lib = hdcp2p2_ctrl->txmtr_ops;

	if (lib && lib->stop)
		lib->stop(hdcp2p2_ctrl->hdcp_lib_handle);

	mutex_unlock(&hdcp2p2_ctrl->mutex);
}

static int hdcp2p2_send_message_to_sink(void *client_ctx,
		char *message, u32 msg_size)
{
	struct hdcp2p2_ctrl *hdcp2p2_ctrl = client_ctx;
	int rc = 0;

	if (!hdcp2p2_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		goto exit;
	}

	if (hdcp2p2_ctrl->auth_state == HDCP_STATE_INACTIVE) {
		DEV_DBG("%s: hdcp is off\n", __func__);
		goto exit;
	}

	DEV_DBG("%s: Sending %s message from tx to sink\n", __func__,
			hdcp2p2_message_name((int)message[0]));

	switch (message[0]) {
	case AKE_STORED_KM_MESSAGE:
		hdcp2p2_ctrl->tx_has_master_key = true;
		hdcp2p2_advance_next_sink_message(hdcp2p2_ctrl);
		hdcp2p2_advance_next_tx_message(hdcp2p2_ctrl);
		break;
	case SKE_SEND_EKS_MESSAGE:
		/* Send EKS message comes from TX when we are authenticated */
		hdcp2p2_ctrl->auth_state = HDCP_STATE_AUTHENTICATED;
		hdcp2p2_ctrl->next_sink_message = START_AUTHENTICATION;
		hdcp2p2_ctrl->next_tx_message = AKE_INIT_MESSAGE;
		hdcp2p2_ctrl->tx_has_master_key = false;
		hdcp2p2_ctrl->init_data.notify_status(
				hdcp2p2_ctrl->init_data.cb_data,
				HDCP_STATE_AUTHENTICATED);
		break;
	case AKE_NO_STORED_KM_MESSAGE:
		/*
		 * We need a delay to allow sink time to be ready to receive the
		 * message
		 */
		msleep(100);
		/* fall through */
	default:
		hdcp2p2_advance_next_sink_message(hdcp2p2_ctrl);
		hdcp2p2_advance_next_tx_message(hdcp2p2_ctrl);
		break;
	}

	/* Forward the message to the sink */
	rc = hdcp2p2_ddc_write_message(hdcp2p2_ctrl, message, (size_t)msg_size);
	if (rc)
		DEV_ERR("%s: Error in writing to sink %d\n", __func__, rc);
	else
		DEV_DBG("%s: SUCCESS\n", __func__);
exit:
	mutex_unlock(&hdcp2p2_ctrl->mutex);

	return rc;
}

static int hdcp2p2_recv_message_from_sink(void *client_ctx,
		char *message, u32 msg_size, u32 timeout)
{
	struct hdcp2p2_ctrl *hdcp2p2_ctrl = client_ctx;
	int rc = 0;

	if (!hdcp2p2_ctrl) {
		DEV_ERR("%s: invalid ctrl\n", __func__);
		return -EINVAL;
	}

	if (!message) {
		DEV_ERR("%s: invalid message\n", __func__);
		return -EINVAL;
	}

	if (hdcp2p2_ctrl->auth_state == HDCP_STATE_INACTIVE) {
		DEV_DBG("%s: hdcp is off\n", __func__);
		return 0;
	}

	/* Start polling sink for the next expected message in the protocol */
	if (message[0] != SKE_SEND_EKS_MESSAGE) {
		rc = hdcp2p2_sink_message_read(hdcp2p2_ctrl, timeout);
	} else {
		/* Enable interrupts */
		u32 regval = DSS_REG_R(hdcp2p2_ctrl->init_data.core_io,
				HDMI_HDCP_INT_CTRL2);
		DEV_DBG("%s: Now authenticated. Enabling interrupts\n",
				__func__);
		regval |= BIT(1);
		regval |= BIT(2);
		regval |= BIT(5);
		DSS_REG_W(hdcp2p2_ctrl->init_data.core_io, HDMI_HDCP_INT_CTRL2,
			regval);
	}

	return rc;
}

static int hdcp2p2_tz_error(void *client_ctx)
{
	struct hdcp2p2_ctrl *hdcp2p2_ctrl = (struct hdcp2p2_ctrl *)client_ctx;

	if (!hdcp2p2_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	hdcp2p2_auth_failed(hdcp2p2_ctrl);

	return 0;
}

void hdmi_hdcp2p2_deinit(void *input)
{
	struct hdcp2p2_ctrl *hdcp2p2_ctrl = (struct hdcp2p2_ctrl *)input;
	struct hdcp_txmtr_ops *lib = NULL;

	if (!hdcp2p2_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	lib = hdcp2p2_ctrl->txmtr_ops;

	if (lib && lib->stop)
		lib->stop(hdcp2p2_ctrl->hdcp_lib_handle);

	sysfs_remove_group(hdcp2p2_ctrl->init_data.sysfs_kobj,
				&hdcp2p2_fs_attr_group);

	mutex_destroy(&hdcp2p2_ctrl->mutex);
	kfree(hdcp2p2_ctrl);
}

void *hdmi_hdcp2p2_init(struct hdmi_hdcp_init_data *init_data)
{
	struct hdcp2p2_ctrl *hdcp2p2_ctrl;
	int rc;
	static struct hdmi_hdcp_ops ops = {
		.hdmi_hdcp_isr = hdcp2p2_isr,
		.hdmi_hdcp_reauthenticate = hdcp2p2_reauthenticate,
		.hdmi_hdcp_authenticate = hdcp2p2_authenticate,
		.feature_supported = hdcp2p2_feature_supported,
		.hdmi_hdcp_off = hdcp2p2_off
	};

	static struct hdcp_client_ops client_ops = {
		.hdcp_send_message = hdcp2p2_send_message_to_sink,
		.hdcp_recv_message = hdcp2p2_recv_message_from_sink,
		.hdcp_tz_error = hdcp2p2_tz_error,
	};

	static struct hdcp_txmtr_ops txmtr_ops;

	DEV_DBG("%s: HDCP2P2 feature initialization\n", __func__);

	if (!init_data || !init_data->core_io || !init_data->mutex ||
		!init_data->ddc_ctrl || !init_data->notify_status ||
		!init_data->workq || !init_data->cb_data) {
		DEV_ERR("%s: invalid input\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	if (init_data->hdmi_tx_ver < MIN_HDMI_TX_MAJOR_VERSION) {
		DEV_ERR("%s: HDMI Tx does not support HDCP 2.2\n", __func__);
		return ERR_PTR(-ENODEV);
	}

	hdcp2p2_ctrl = kzalloc(sizeof(*hdcp2p2_ctrl), GFP_KERNEL);
	if (!hdcp2p2_ctrl) {
		return ERR_PTR(-ENOMEM);
	}

	hdcp2p2_ctrl->init_data = *init_data;
	hdcp2p2_ctrl->client_ops = &client_ops;
	hdcp2p2_ctrl->txmtr_ops = &txmtr_ops;

	rc = sysfs_create_group(init_data->sysfs_kobj,
				&hdcp2p2_fs_attr_group);
	if (rc) {
		DEV_ERR("%s: hdcp2p2 sysfs group creation failed\n", __func__);
		goto error;
	}

	init_completion(&hdcp2p2_ctrl->rxstatus_completion);

	hdcp2p2_ctrl->sink_status = SINK_DISCONNECTED;

	hdcp2p2_ctrl->next_sink_message = START_AUTHENTICATION;
	hdcp2p2_ctrl->next_tx_message = AKE_INIT_MESSAGE;
	hdcp2p2_ctrl->tx_has_master_key = false;
	hdcp2p2_ctrl->auth_state = HDCP_STATE_INACTIVE;
	hdcp2p2_ctrl->ops = &ops;
	mutex_init(&hdcp2p2_ctrl->mutex);

	rc = hdcp_library_register(&hdcp2p2_ctrl->hdcp_lib_handle,
			hdcp2p2_ctrl->client_ops,
			hdcp2p2_ctrl->txmtr_ops, hdcp2p2_ctrl);
	if (rc) {
		DEV_ERR("%s: Unable to register with HDCP 2.2 library\n",
			__func__);
		goto error;
	}

	return hdcp2p2_ctrl;

error:
	kfree(hdcp2p2_ctrl);
	return ERR_PTR(rc);
}

static bool hdcp2p2_supported(struct hdcp2p2_ctrl *hdcp2p2_ctrl)
{
	u8 hdcp2version;

	int rc = hdcp2p2_read_version(hdcp2p2_ctrl, &hdcp2version);
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
	struct hdcp2p2_ctrl *hdcp2p2_ctrl = input;

	DEV_DBG("%s: Checking sink capability\n", __func__);
	if (hdcp2p2_supported(hdcp2p2_ctrl))
		return hdcp2p2_ctrl->ops;
	else
		return NULL;

}

