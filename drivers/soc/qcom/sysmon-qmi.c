/*
 * Copyright (c) 2014-2018, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "sysmon-qmi: %s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/completion.h>
#include <linux/slab.h>
#include <linux/of.h>

#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/subsystem_notif.h>
#include <linux/soc/qcom/qmi.h>
#include <soc/qcom/sysmon.h>

#define QMI_RESP_BIT_SHIFT(x)			(x << 16)

#define QMI_SSCTL_RESTART_REQ_V02		0x0020
#define QMI_SSCTL_RESTART_RESP_V02		0x0020
#define QMI_SSCTL_RESTART_READY_IND_V02		0x0020
#define QMI_SSCTL_SHUTDOWN_REQ_V02		0x0021
#define QMI_SSCTL_SHUTDOWN_RESP_V02		0x0021
#define QMI_SSCTL_SHUTDOWN_READY_IND_V02	0x0021
#define QMI_SSCTL_GET_FAILURE_REASON_REQ_V02	0x0022
#define QMI_SSCTL_GET_FAILURE_REASON_RESP_V02	0x0022
#define QMI_SSCTL_GET_FAILURE_REASON_IND_V02	0x0022
#define QMI_SSCTL_SUBSYS_EVENT_REQ_V02		0x0023
#define QMI_SSCTL_SUBSYS_EVENT_RESP_V02		0x0023
#define QMI_SSCTL_SUBSYS_EVENT_READY_IND_V02	0x0023

#define QMI_SSCTL_ERROR_MSG_LENGTH		90
#define QMI_SSCTL_SUBSYS_NAME_LENGTH		15
#define QMI_SSCTL_SUBSYS_EVENT_REQ_LENGTH	40
#define QMI_SSCTL_RESP_MSG_LENGTH		7
#define QMI_SSCTL_EMPTY_MSG_LENGTH		0

#define SSCTL_SERVICE_ID			0x2B
#define SSCTL_VER_2				2
#define SERVER_TIMEOUT				500
#define SHUTDOWN_TIMEOUT			10000

#define QMI_EOTI_DATA_TYPE	\
{				\
	.data_type = QMI_EOTI,	\
	.elem_len  = 0,		\
	.elem_size = 0,		\
	.is_array  = NO_ARRAY,	\
	.tlv_type  = 0x00,	\
	.offset    = 0,		\
	.ei_array  = NULL,	\
},

struct sysmon_qmi_data {
	const char *name;
	int instance_id;
	bool connected;
	struct qmi_handle clnt_handle;
	struct notifier_block notifier;
	void *notif_handle;
	bool legacy_version;
	struct sockaddr_qrtr ssctl;
	struct list_head list;
};

static LIST_HEAD(sysmon_list);
static DEFINE_MUTEX(sysmon_list_lock);

static const int notif_map[SUBSYS_NOTIF_TYPE_COUNT] = {
	[0 ... SUBSYS_NOTIF_TYPE_COUNT - 1] = SSCTL_SSR_EVENT_INVALID,
	[SUBSYS_BEFORE_POWERUP] = SSCTL_SSR_EVENT_BEFORE_POWERUP,
	[SUBSYS_AFTER_POWERUP] = SSCTL_SSR_EVENT_AFTER_POWERUP,
	[SUBSYS_BEFORE_SHUTDOWN] = SSCTL_SSR_EVENT_BEFORE_SHUTDOWN,
	[SUBSYS_AFTER_SHUTDOWN] = SSCTL_SSR_EVENT_AFTER_SHUTDOWN,
};

struct qmi_ssctl_shutdown_indication {
};

static struct qmi_elem_info qmi_ssctl_indication_ei[] = {
	QMI_EOTI_DATA_TYPE
};

static void sysmon_ind_cb(struct qmi_handle *qmi, struct sockaddr_qrtr *sq,
			struct qmi_txn *txn, const void *data)
{
	struct sysmon_qmi_data *qmi_data = container_of(qmi,
					struct sysmon_qmi_data, clnt_handle);

	struct subsys_device *subsys_dev = find_subsys_device(qmi_data->name);
	pr_info("%s: Indication received from subsystem\n", qmi_data->name);
	if (subsys_dev)
		complete_shutdown_ack(subsys_dev);
	else
		pr_err("Failed to find subsystem: %s for indication\n",
		       qmi_data->name);
}

static struct qmi_msg_handler qmi_indication_handler[] = {
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_SSCTL_SHUTDOWN_READY_IND_V02,
		.ei = qmi_ssctl_indication_ei,
		.decoded_size = 0,
		.fn = sysmon_ind_cb
	},
	{}
};

static bool is_ssctl_event(enum subsys_notif_type notif)
{
	return notif_map[notif] != SSCTL_SSR_EVENT_INVALID;
}

static int ssctl_new_server(struct qmi_handle *qmi, struct qmi_service *svc)
{
	struct sysmon_qmi_data *data = container_of(qmi,
					struct sysmon_qmi_data, clnt_handle);

	pr_info("Connection established between QMI handle and %s's SSCTL service\n"
								, data->name);

	data->ssctl.sq_family = AF_QIPCRTR;
	data->ssctl.sq_node = svc->node;
	data->ssctl.sq_port = svc->port;
	data->connected = true;
	return 0;
}

static void ssctl_del_server(struct qmi_handle *qmi, struct qmi_service *svc)
{
	struct sysmon_qmi_data *data = container_of(qmi,
					struct sysmon_qmi_data, clnt_handle);

	pr_info("Connection lost between QMI handle and %s's SSCTL service\n"
								, data->name);
	data->connected = false;
}

struct qmi_ops ssctl_ops = {
	.new_server = ssctl_new_server,
	.del_server = ssctl_del_server,
};

struct qmi_ssctl_subsys_event_req_msg {
	uint8_t subsys_name_len;
	char subsys_name[QMI_SSCTL_SUBSYS_NAME_LENGTH];
	enum ssctl_ssr_event_enum_type event;
	uint8_t evt_driven_valid;
	enum ssctl_ssr_event_driven_enum_type evt_driven;
};

struct qmi_ssctl_subsys_event_resp_msg {
	struct qmi_response_type_v01 resp;
};

static struct qmi_elem_info qmi_ssctl_subsys_event_req_msg_ei[] = {
	{
		.data_type = QMI_DATA_LEN,
		.elem_len  = 1,
		.elem_size = sizeof(uint8_t),
		.is_array  = NO_ARRAY,
		.tlv_type  = 0x01,
		.offset    = offsetof(struct qmi_ssctl_subsys_event_req_msg,
				      subsys_name_len),
		.ei_array  = NULL,
	},
	{
		.data_type = QMI_UNSIGNED_1_BYTE,
		.elem_len  = QMI_SSCTL_SUBSYS_NAME_LENGTH,
		.elem_size = sizeof(char),
		.is_array  = VAR_LEN_ARRAY,
		.tlv_type  = 0x01,
		.offset    = offsetof(struct qmi_ssctl_subsys_event_req_msg,
				      subsys_name),
		.ei_array  = NULL,
	},
	{
		.data_type = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len  = 1,
		.elem_size = sizeof(uint32_t),
		.is_array  = NO_ARRAY,
		.tlv_type  = 0x02,
		.offset    = offsetof(struct qmi_ssctl_subsys_event_req_msg,
				      event),
		.ei_array  = NULL,
	},
	{
		.data_type = QMI_OPT_FLAG,
		.elem_len  = 1,
		.elem_size = sizeof(uint8_t),
		.is_array  = NO_ARRAY,
		.tlv_type  = 0x10,
		.offset    = offsetof(struct qmi_ssctl_subsys_event_req_msg,
				      evt_driven_valid),
		.ei_array  = NULL,
	},
	{
		.data_type = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len  = 1,
		.elem_size = sizeof(uint32_t),
		.is_array  = NO_ARRAY,
		.tlv_type  = 0x10,
		.offset    = offsetof(struct qmi_ssctl_subsys_event_req_msg,
				      evt_driven),
		.ei_array  = NULL,
	},
	QMI_EOTI_DATA_TYPE
};

static struct qmi_elem_info qmi_ssctl_subsys_event_resp_msg_ei[] = {
	{
		.data_type = QMI_STRUCT,
		.elem_len  = 1,
		.elem_size = sizeof(struct qmi_response_type_v01),
		.is_array  = NO_ARRAY,
		.tlv_type  = 0x02,
		.offset    = offsetof(struct qmi_ssctl_subsys_event_resp_msg,
				      resp),
		.ei_array  = qmi_response_type_v01_ei,
	},
	QMI_EOTI_DATA_TYPE
};

/**
 * sysmon_send_event() - Notify a subsystem of another's state change
 * @dest_desc:	Subsystem descriptor of the subsystem the notification
 * should be sent to
 * @event_desc:	Subsystem descriptor of the subsystem that generated the
 * notification
 * @notif:	ID of the notification type (ex. SUBSYS_BEFORE_SHUTDOWN)
 *
 * Reverts to using legacy sysmon API (sysmon_send_event_no_qmi()) if
 * client handle is not set.
 *
 * Returns 0 for success, -EINVAL for invalid destination or notification IDs,
 * -ENODEV if the transport channel is not open, -ETIMEDOUT if the destination
 * subsystem does not respond, and -EPROTO if the destination subsystem
 * responds, but with something other than an acknowledgment.
 *
 * If CONFIG_MSM_SYSMON_COMM is not defined, always return success (0).
 */
int sysmon_send_event(struct subsys_desc *dest_desc,
			struct subsys_desc *event_desc,
			enum subsys_notif_type notif)
{
	struct qmi_ssctl_subsys_event_req_msg req;
	struct qmi_ssctl_subsys_event_resp_msg resp = { { 0, 0 } };
	struct sysmon_qmi_data *data = NULL, *temp;
	const char *event_ss = event_desc->name;
	const char *dest_ss = dest_desc->name;
	int ret;
	struct qmi_txn txn;

	if (notif < 0 || notif >= SUBSYS_NOTIF_TYPE_COUNT ||
	    !is_ssctl_event(notif) || event_ss == NULL || dest_ss == NULL)
		return -EINVAL;

	mutex_lock(&sysmon_list_lock);
	list_for_each_entry(temp, &sysmon_list, list)
		if (!strcmp(temp->name, dest_desc->name))
			data = temp;
	mutex_unlock(&sysmon_list_lock);

	if (!data)
		return -EINVAL;

	if (data->instance_id < 0) {
		pr_debug("No SSCTL_V2 support for %s. Revert to SSCTL_V0\n",
								dest_ss);
		ret = sysmon_send_event_no_qmi(dest_desc, event_desc, notif);
		if (ret)
			pr_debug("SSCTL_V0 implementation failed - %d\n", ret);

		return ret;
	}

	if (!data->connected)
		return -EAGAIN;

	snprintf(req.subsys_name, ARRAY_SIZE(req.subsys_name), "%s", event_ss);
	req.subsys_name_len = strlen(req.subsys_name);
	req.event = notif_map[notif];
	req.evt_driven_valid = 1;
	req.evt_driven = SSCTL_SSR_EVENT_FORCED;

	ret = qmi_txn_init(&data->clnt_handle, &txn,
			qmi_ssctl_subsys_event_resp_msg_ei,
			&resp);

	if (ret < 0) {
		pr_err("SYSMON QMI tx init failed to dest %s, ret - %d\n",
			dest_ss, ret);
		goto out;
	}

	ret = qmi_send_request(&data->clnt_handle, &data->ssctl, &txn,
			QMI_SSCTL_SUBSYS_EVENT_REQ_V02,
			QMI_SSCTL_SUBSYS_EVENT_REQ_LENGTH,
			qmi_ssctl_subsys_event_req_msg_ei,
			&req);
	if (ret < 0) {
		pr_err("SYSMON QMI send req failed to dest %s, ret - %d\n",
			 dest_ss, ret);
		qmi_txn_cancel(&txn);
		goto out;
	}

	ret = qmi_txn_wait(&txn, msecs_to_jiffies(SERVER_TIMEOUT));
	if (ret < 0) {
		pr_err("SYSMON QMI qmi txn wait failed for client %s, ret - %d\n",
			dest_ss, ret);
		goto out;
	}

	/* Check the response */
	if (QMI_RESP_BIT_SHIFT(resp.resp.result) != QMI_RESULT_SUCCESS_V01) {
		pr_err("SYSMON QMI request failed 0x%x\n",
					QMI_RESP_BIT_SHIFT(resp.resp.error));
		ret = -EREMOTEIO;
	}
out:
	return ret;
}
EXPORT_SYMBOL(sysmon_send_event);

struct qmi_ssctl_shutdown_req_msg {
};

struct qmi_ssctl_shutdown_resp_msg {
	struct qmi_response_type_v01 resp;
};

static struct qmi_elem_info qmi_ssctl_shutdown_req_msg_ei[] = {
	QMI_EOTI_DATA_TYPE
};

static struct qmi_elem_info qmi_ssctl_shutdown_resp_msg_ei[] = {
	{
		.data_type = QMI_STRUCT,
		.elem_len  = 1,
		.elem_size = sizeof(struct qmi_response_type_v01),
		.is_array  = NO_ARRAY,
		.tlv_type  = 0x02,
		.offset    = offsetof(struct qmi_ssctl_shutdown_resp_msg,
				      resp),
		.ei_array  = qmi_response_type_v01_ei,
	},
	QMI_EOTI_DATA_TYPE
};

/**
 * sysmon_send_shutdown() - send shutdown command to a
 * subsystem.
 * @dest_desc:	Subsystem descriptor of the subsystem to send to
 *
 * Reverts to using legacy sysmon API (sysmon_send_shutdown_no_qmi()) if
 * client handle is not set.
 *
 * Returns 0 for success, -EINVAL for an invalid destination, -ENODEV if
 * the SMD transport channel is not open, -ETIMEDOUT if the destination
 * subsystem does not respond, and -EPROTO if the destination subsystem
 * responds with something unexpected.
 *
 * If CONFIG_MSM_SYSMON_COMM is not defined, always return success (0).
 */
int sysmon_send_shutdown(struct subsys_desc *dest_desc)
{
	struct qmi_ssctl_shutdown_resp_msg resp = { { 0, 0 } };
	struct sysmon_qmi_data *data = NULL, *temp;
	const char *dest_ss = dest_desc->name;
	char req = 0;
	int ret, shutdown_ack_ret;
	struct qmi_txn txn;

	if (dest_ss == NULL)
		return -EINVAL;

	mutex_lock(&sysmon_list_lock);
	list_for_each_entry(temp, &sysmon_list, list)
		if (!strcmp(temp->name, dest_desc->name))
			data = temp;
	mutex_unlock(&sysmon_list_lock);

	if (!data)
		return -EINVAL;

	if (data->instance_id < 0) {
		pr_debug("No SSCTL_V2 support for %s. Revert to SSCTL_V0\n",
								dest_ss);
		ret = sysmon_send_shutdown_no_qmi(dest_desc);
		if (ret)
			pr_debug("SSCTL_V0 implementation failed - %d\n", ret);

		return ret;
	}

	if (!data->connected)
		return -EAGAIN;

	ret = qmi_txn_init(&data->clnt_handle, &txn,
			qmi_ssctl_shutdown_resp_msg_ei,
			&resp);

	if (ret < 0) {
		pr_err("SYSMON QMI tx init failed to dest %s, ret - %d\n",
			dest_ss, ret);
		goto out;
	}

	ret = qmi_send_request(&data->clnt_handle, &data->ssctl, &txn,
			QMI_SSCTL_SHUTDOWN_REQ_V02,
			QMI_SSCTL_EMPTY_MSG_LENGTH,
			qmi_ssctl_shutdown_req_msg_ei,
			&req);
	if (ret < 0) {
		pr_err("SYSMON QMI send req failed to dest %s, ret - %d\n",
			 dest_ss, ret);
		qmi_txn_cancel(&txn);
		goto out;
	}

	ret = qmi_txn_wait(&txn, msecs_to_jiffies(SERVER_TIMEOUT));
	if (ret < 0) {
		pr_err("SYSMON QMI txn wait failed to dest %s, ret - %d\n",
			dest_ss, ret);
	}

	/* Check the response */
	if (ret != -ETIMEDOUT && QMI_RESP_BIT_SHIFT(resp.resp.result) !=
	    QMI_RESULT_SUCCESS_V01) {
		pr_err("SYSMON QMI request failed 0x%x\n",
					QMI_RESP_BIT_SHIFT(resp.resp.error));
		ret = -EREMOTEIO;
		goto out;
	}

	shutdown_ack_ret = wait_for_shutdown_ack(dest_desc);
	if (shutdown_ack_ret > 0) {
		ret = 0;
		goto out;
	} else if (shutdown_ack_ret < 0) {
		pr_err("shutdown acknowledgment not received for %s\n",
		       data->name);
		ret = shutdown_ack_ret;
	}
out:
	return ret;
}
EXPORT_SYMBOL(sysmon_send_shutdown);

struct qmi_ssctl_get_failure_reason_req_msg {
};

struct qmi_ssctl_get_failure_reason_resp_msg {
	struct qmi_response_type_v01 resp;
	uint8_t error_message_valid;
	uint32_t error_message_len;
	char error_message[QMI_SSCTL_ERROR_MSG_LENGTH];
};

static struct qmi_elem_info qmi_ssctl_get_failure_reason_req_msg_ei[] = {
	QMI_EOTI_DATA_TYPE
};

static struct qmi_elem_info qmi_ssctl_get_failure_reason_resp_msg_ei[] = {
	{
		.data_type = QMI_STRUCT,
		.elem_len  = 1,
		.elem_size = sizeof(struct qmi_response_type_v01),
		.is_array  = NO_ARRAY,
		.tlv_type  = 0x02,
		.offset    = offsetof(
			struct qmi_ssctl_get_failure_reason_resp_msg,
							resp),
		.ei_array  = qmi_response_type_v01_ei,
	},
	{
		.data_type = QMI_OPT_FLAG,
		.elem_len  = 1,
		.elem_size = sizeof(uint8_t),
		.is_array  = NO_ARRAY,
		.tlv_type  = 0x10,
		.offset    = offsetof(
			struct qmi_ssctl_get_failure_reason_resp_msg,
						error_message_valid),
		.ei_array  = NULL,
	},
	{
		.data_type = QMI_DATA_LEN,
		.elem_len  = 1,
		.elem_size = sizeof(uint8_t),
		.is_array  = NO_ARRAY,
		.tlv_type  = 0x10,
		.offset    = offsetof(
			struct qmi_ssctl_get_failure_reason_resp_msg,
						error_message_len),
		.ei_array  = NULL,
	},
	{
		.data_type = QMI_UNSIGNED_1_BYTE,
		.elem_len  = QMI_SSCTL_ERROR_MSG_LENGTH,
		.elem_size = sizeof(char),
		.is_array  = VAR_LEN_ARRAY,
		.tlv_type  = 0x10,
		.offset    = offsetof(
			struct qmi_ssctl_get_failure_reason_resp_msg,
						error_message),
		.ei_array  = NULL,
	},
	QMI_EOTI_DATA_TYPE
};

/**
 * sysmon_get_reason() - Retrieve failure reason from a subsystem.
 * @dest_desc:	Subsystem descriptor of the subsystem to query
 * @buf:	Caller-allocated buffer for the returned NUL-terminated reason
 * @len:	Length of @buf
 *
 * Reverts to using legacy sysmon API (sysmon_get_reason_no_qmi()) if client
 * handle is not set.
 *
 * Returns 0 for success, -EINVAL for an invalid destination, -ENODEV if
 * the SMD transport channel is not open, -ETIMEDOUT if the destination
 * subsystem does not respond, and -EPROTO if the destination subsystem
 * responds with something unexpected.
 *
 * If CONFIG_MSM_SYSMON_COMM is not defined, always return success (0).
 */
int sysmon_get_reason(struct subsys_desc *dest_desc, char *buf, size_t len)
{
	struct qmi_ssctl_get_failure_reason_resp_msg resp;
	struct sysmon_qmi_data *data = NULL, *temp;
	struct qmi_txn txn;
	const char *dest_ss = dest_desc->name;
	const char expect[] = "ssr:return:";
	char req = 0;
	int ret;

	if (dest_ss == NULL || buf == NULL || len == 0)
		return -EINVAL;

	mutex_lock(&sysmon_list_lock);
	list_for_each_entry(temp, &sysmon_list, list)
		if (!strcmp(temp->name, dest_desc->name))
			data = temp;
	mutex_unlock(&sysmon_list_lock);

	if (!data)
		return -EINVAL;

	if (data->instance_id < 0) {
		pr_debug("No SSCTL_V2 support for %s. Revert to SSCTL_V0\n",
								dest_ss);
		ret = sysmon_get_reason_no_qmi(dest_desc, buf, len);
		if (ret)
			pr_debug("SSCTL_V0 implementation failed - %d\n", ret);

		return ret;
	}

	if (!data->connected)
		return -EAGAIN;

	ret = qmi_txn_init(&data->clnt_handle, &txn,
			qmi_ssctl_get_failure_reason_resp_msg_ei,
			&resp);
	if (ret < 0) {
		pr_err("SYSMON QMI tx init failed to dest %s, ret - %d\n",
			dest_ss, ret);
		goto out;
	}

	ret = qmi_send_request(&data->clnt_handle, &data->ssctl, &txn,
			QMI_SSCTL_GET_FAILURE_REASON_REQ_V02,
			QMI_SSCTL_EMPTY_MSG_LENGTH,
			qmi_ssctl_get_failure_reason_req_msg_ei,
			&req);
	if (ret < 0) {
		pr_err("SYSMON QMI send req failed to dest %s, ret - %d\n",
			 dest_ss, ret);
		qmi_txn_cancel(&txn);
		goto out;
	}

	ret = qmi_txn_wait(&txn, msecs_to_jiffies(SERVER_TIMEOUT));
	if (ret < 0) {
		pr_err("SYSMON QMI qmi txn wait failed to dest %s, ret - %d\n",
			dest_ss, ret);
		goto out;
	}

	/* Check the response */
	if (QMI_RESP_BIT_SHIFT(resp.resp.result) != QMI_RESULT_SUCCESS_V01) {
		pr_err("SYSMON QMI request failed 0x%x\n",
					QMI_RESP_BIT_SHIFT(resp.resp.error));
		ret = -EREMOTEIO;
		goto out;
	}

	if (!strcmp(resp.error_message, expect)) {
		pr_err("Unexpected response %s\n", resp.error_message);
		ret = -EPROTO;
		goto out;
	}
	strlcpy(buf, resp.error_message, resp.error_message_len);
out:
	return ret;
}
EXPORT_SYMBOL(sysmon_get_reason);

/**
 * sysmon_notifier_register() - Initialize sysmon data for a subsystem.
 * @dest_desc:	Subsystem descriptor of the subsystem
 *
 * Returns 0 for success. If the subsystem does not support SSCTL v2, a
 * value of 0 is returned after adding the subsystem entry to the sysmon_list.
 * In addition, if the SSCTL v2 support exists, the notifier block to receive
 * events from the SSCTL service on the subsystem is registered.
 *
 * If CONFIG_MSM_SYSMON_COMM is not defined, always return success (0).
 */
int sysmon_notifier_register(struct subsys_desc *desc)
{
	struct sysmon_qmi_data *data;
	int rc = 0;

	data = kmalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->name = desc->name;
	data->instance_id = desc->ssctl_instance_id;
	data->legacy_version = false;
	data->connected = false;

	if (data->instance_id <= 0) {
		pr_debug("SSCTL instance id not defined\n");
		goto add_list;
	}

	rc = qmi_handle_init(&data->clnt_handle,
			QMI_SSCTL_RESP_MSG_LENGTH, &ssctl_ops,
			qmi_indication_handler);
	if (rc < 0) {
		pr_err("Sysmon QMI handle init failed rc:%d\n", rc);
		kfree(data);
		return rc;
	}

	qmi_add_lookup(&data->clnt_handle, SSCTL_SERVICE_ID,
			SSCTL_VER_2, data->instance_id);
add_list:
	mutex_lock(&sysmon_list_lock);
	INIT_LIST_HEAD(&data->list);
	list_add_tail(&data->list, &sysmon_list);
	mutex_unlock(&sysmon_list_lock);
	return rc;
}
EXPORT_SYMBOL(sysmon_notifier_register);

/**
 * sysmon_notifier_unregister() - Cleanup the subsystem's sysmon data.
 * @dest_desc:	Subsystem descriptor of the subsystem
 *
 * If the subsystem does not support SSCTL v2, its entry is simply removed from
 * the sysmon_list. In addition, if the SSCTL v2 support exists, the notifier
 * block to receive events from the SSCTL service is unregistered.
 */
void sysmon_notifier_unregister(struct subsys_desc *desc)
{
	struct sysmon_qmi_data *data = NULL, *sysmon_data, *tmp;

	mutex_lock(&sysmon_list_lock);
	list_for_each_entry_safe(sysmon_data, tmp, &sysmon_list, list)
		if (!strcmp(sysmon_data->name, desc->name)) {
			data = sysmon_data;
			list_del(&data->list);
		}
	mutex_unlock(&sysmon_list_lock);

	if (data == NULL)
		return;

	if (data->instance_id > 0)
		qmi_handle_release(&data->clnt_handle);
	kfree(data);
}
EXPORT_SYMBOL(sysmon_notifier_unregister);
