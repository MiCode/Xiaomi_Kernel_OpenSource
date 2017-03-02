/*
 * Copyright (c) 2014-2015, 2017, The Linux Foundation. All rights reserved.
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
#include <soc/qcom/msm_qmi_interface.h>
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
	struct work_struct svc_arrive;
	struct work_struct svc_exit;
	struct work_struct svc_rcv_msg;
	struct qmi_handle *clnt_handle;
	struct notifier_block notifier;
	void *notif_handle;
	bool legacy_version;
	struct completion server_connect;
	struct completion ind_recv;
	struct list_head list;
};

static struct workqueue_struct *sysmon_wq;

static LIST_HEAD(sysmon_list);
static DEFINE_MUTEX(sysmon_list_lock);
static DEFINE_MUTEX(sysmon_lock);

static void sysmon_clnt_recv_msg(struct work_struct *work);
static void sysmon_clnt_svc_arrive(struct work_struct *work);
static void sysmon_clnt_svc_exit(struct work_struct *work);

static const int notif_map[SUBSYS_NOTIF_TYPE_COUNT] = {
	[SUBSYS_BEFORE_POWERUP] = SSCTL_SSR_EVENT_BEFORE_POWERUP,
	[SUBSYS_AFTER_POWERUP] = SSCTL_SSR_EVENT_AFTER_POWERUP,
	[SUBSYS_BEFORE_SHUTDOWN] = SSCTL_SSR_EVENT_BEFORE_SHUTDOWN,
	[SUBSYS_AFTER_SHUTDOWN] = SSCTL_SSR_EVENT_AFTER_SHUTDOWN,
};

static void sysmon_ind_cb(struct qmi_handle *handle, unsigned int msg_id,
			void *msg, unsigned int msg_len, void *ind_cb_priv)
{
	struct sysmon_qmi_data *data = NULL, *temp;

	mutex_lock(&sysmon_list_lock);
	list_for_each_entry(temp, &sysmon_list, list)
		if (!strcmp(temp->name, (char *)ind_cb_priv))
			data = temp;
	mutex_unlock(&sysmon_list_lock);

	if (!data)
		return;

	pr_debug("%s: Indication received from subsystem\n", data->name);
	complete(&data->ind_recv);
}

static int sysmon_svc_event_notify(struct notifier_block *this,
				      unsigned long code,
				      void *_cmd)
{
	struct sysmon_qmi_data *data = container_of(this,
					struct sysmon_qmi_data, notifier);

	switch (code) {
	case QMI_SERVER_ARRIVE:
		queue_work(sysmon_wq, &data->svc_arrive);
		break;
	case QMI_SERVER_EXIT:
		queue_work(sysmon_wq, &data->svc_exit);
		break;
	default:
		break;
	}
	return 0;
}

static void sysmon_clnt_notify(struct qmi_handle *handle,
			     enum qmi_event_type event, void *notify_priv)
{
	struct sysmon_qmi_data *data = container_of(notify_priv,
					struct sysmon_qmi_data, svc_arrive);

	switch (event) {
	case QMI_RECV_MSG:
		schedule_work(&data->svc_rcv_msg);
		break;
	default:
		break;
	}
}

static void sysmon_clnt_svc_arrive(struct work_struct *work)
{
	int rc;
	struct sysmon_qmi_data *data = container_of(work,
					struct sysmon_qmi_data, svc_arrive);

	mutex_lock(&sysmon_lock);
	/* Create a Local client port for QMI communication */
	data->clnt_handle = qmi_handle_create(sysmon_clnt_notify, work);
	if (!data->clnt_handle) {
		pr_err("QMI client handle alloc failed for %s\n", data->name);
		mutex_unlock(&sysmon_lock);
		return;
	}

	rc = qmi_connect_to_service(data->clnt_handle, SSCTL_SERVICE_ID,
					SSCTL_VER_2, data->instance_id);
	if (rc < 0) {
		pr_err("%s: Could not connect handle to service\n",
								data->name);
		qmi_handle_destroy(data->clnt_handle);
		data->clnt_handle = NULL;
		mutex_unlock(&sysmon_lock);
		return;
	}
	pr_info("Connection established between QMI handle and %s's SSCTL service\n"
								, data->name);

	rc = qmi_register_ind_cb(data->clnt_handle, sysmon_ind_cb,
							(void *)data->name);
	if (rc < 0)
		pr_warn("%s: Could not register the indication callback\n",
								data->name);
	mutex_unlock(&sysmon_lock);
}

static void sysmon_clnt_svc_exit(struct work_struct *work)
{
	struct sysmon_qmi_data *data = container_of(work,
					struct sysmon_qmi_data, svc_exit);

	mutex_lock(&sysmon_lock);
	qmi_handle_destroy(data->clnt_handle);
	data->clnt_handle = NULL;
	mutex_unlock(&sysmon_lock);
}

static void sysmon_clnt_recv_msg(struct work_struct *work)
{
	int ret;
	struct sysmon_qmi_data *data = container_of(work,
					struct sysmon_qmi_data, svc_rcv_msg);

	do {
		pr_debug("%s: Notified about a Receive event\n", data->name);
	} while ((ret = qmi_recv_msg(data->clnt_handle)) == 0);

	if (ret != -ENOMSG)
		pr_err("%s: Error receiving message\n", data->name);
}

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

static struct elem_info qmi_ssctl_subsys_event_req_msg_ei[] = {
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

static struct elem_info qmi_ssctl_subsys_event_resp_msg_ei[] = {
	{
		.data_type = QMI_STRUCT,
		.elem_len  = 1,
		.elem_size = sizeof(struct qmi_response_type_v01),
		.is_array  = NO_ARRAY,
		.tlv_type  = 0x02,
		.offset    = offsetof(struct qmi_ssctl_subsys_event_resp_msg,
				      resp),
		.ei_array  = get_qmi_response_type_v01_ei(),
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
 * subsystem does not respond, and -ENOSYS if the destination subsystem
 * responds, but with something other than an acknowledgement.
 *
 * If CONFIG_MSM_SYSMON_COMM is not defined, always return success (0).
 */
int sysmon_send_event(struct subsys_desc *dest_desc,
			struct subsys_desc *event_desc,
			enum subsys_notif_type notif)
{
	struct qmi_ssctl_subsys_event_req_msg req;
	struct msg_desc req_desc, resp_desc;
	struct qmi_ssctl_subsys_event_resp_msg resp = { { 0, 0 } };
	struct sysmon_qmi_data *data = NULL, *temp;
	const char *event_ss = event_desc->name;
	const char *dest_ss = dest_desc->name;
	int ret;

	if (notif < 0 || notif >= SUBSYS_NOTIF_TYPE_COUNT || event_ss == NULL
		|| dest_ss == NULL)
		return -EINVAL;

	mutex_lock(&sysmon_list_lock);
	list_for_each_entry(temp, &sysmon_list, list)
		if (!strcmp(temp->name, dest_desc->name))
			data = temp;
	mutex_unlock(&sysmon_list_lock);

	if (!data)
		return -EINVAL;

	if (!data->clnt_handle) {
		pr_debug("No SSCTL_V2 support for %s. Revert to SSCTL_V0\n",
								dest_ss);
		ret = sysmon_send_event_no_qmi(dest_desc, event_desc, notif);
		if (ret)
			pr_debug("SSCTL_V0 implementation failed - %d\n", ret);

		return ret;
	}

	snprintf(req.subsys_name, ARRAY_SIZE(req.subsys_name), "%s", event_ss);
	req.subsys_name_len = strlen(req.subsys_name);
	req.event = notif_map[notif];
	req.evt_driven_valid = 1;
	req.evt_driven = SSCTL_SSR_EVENT_FORCED;

	req_desc.msg_id = QMI_SSCTL_SUBSYS_EVENT_REQ_V02;
	req_desc.max_msg_len = QMI_SSCTL_SUBSYS_EVENT_REQ_LENGTH;
	req_desc.ei_array = qmi_ssctl_subsys_event_req_msg_ei;

	resp_desc.msg_id = QMI_SSCTL_SUBSYS_EVENT_RESP_V02;
	resp_desc.max_msg_len = QMI_SSCTL_RESP_MSG_LENGTH;
	resp_desc.ei_array = qmi_ssctl_subsys_event_resp_msg_ei;

	mutex_lock(&sysmon_lock);
	ret = qmi_send_req_wait(data->clnt_handle, &req_desc, &req,
		sizeof(req), &resp_desc, &resp, sizeof(resp), SERVER_TIMEOUT);
	if (ret < 0) {
		pr_err("QMI send req to %s failed, ret - %d\n", dest_ss, ret);
		goto out;
	}

	/* Check the response */
	if (QMI_RESP_BIT_SHIFT(resp.resp.result) != QMI_RESULT_SUCCESS_V01) {
		pr_debug("QMI request failed 0x%x\n",
					QMI_RESP_BIT_SHIFT(resp.resp.error));
		ret = -EREMOTEIO;
	}
out:
	mutex_unlock(&sysmon_lock);
	return ret;
}
EXPORT_SYMBOL(sysmon_send_event);

struct qmi_ssctl_shutdown_req_msg {
};

struct qmi_ssctl_shutdown_resp_msg {
	struct qmi_response_type_v01 resp;
};

static struct elem_info qmi_ssctl_shutdown_req_msg_ei[] = {
	QMI_EOTI_DATA_TYPE
};

static struct elem_info qmi_ssctl_shutdown_resp_msg_ei[] = {
	{
		.data_type = QMI_STRUCT,
		.elem_len  = 1,
		.elem_size = sizeof(struct qmi_response_type_v01),
		.is_array  = NO_ARRAY,
		.tlv_type  = 0x02,
		.offset    = offsetof(struct qmi_ssctl_shutdown_resp_msg,
				      resp),
		.ei_array  = get_qmi_response_type_v01_ei(),
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
 * subsystem does not respond, and -ENOSYS if the destination subsystem
 * responds with something unexpected.
 *
 * If CONFIG_MSM_SYSMON_COMM is not defined, always return success (0).
 */
int sysmon_send_shutdown(struct subsys_desc *dest_desc)
{
	struct msg_desc req_desc, resp_desc;
	struct qmi_ssctl_shutdown_resp_msg resp = { { 0, 0 } };
	struct sysmon_qmi_data *data = NULL, *temp;
	const char *dest_ss = dest_desc->name;
	char req = 0;
	int ret, shutdown_ack_ret;

	if (dest_ss == NULL)
		return -EINVAL;

	mutex_lock(&sysmon_list_lock);
	list_for_each_entry(temp, &sysmon_list, list)
		if (!strcmp(temp->name, dest_desc->name))
			data = temp;
	mutex_unlock(&sysmon_list_lock);

	if (!data)
		return -EINVAL;

	if (!data->clnt_handle) {
		pr_debug("No SSCTL_V2 support for %s. Revert to SSCTL_V0\n",
								dest_ss);
		ret = sysmon_send_shutdown_no_qmi(dest_desc);
		if (ret)
			pr_debug("SSCTL_V0 implementation failed - %d\n", ret);

		return ret;
	}

	req_desc.msg_id = QMI_SSCTL_SHUTDOWN_REQ_V02;
	req_desc.max_msg_len = QMI_SSCTL_EMPTY_MSG_LENGTH;
	req_desc.ei_array = qmi_ssctl_shutdown_req_msg_ei;

	resp_desc.msg_id = QMI_SSCTL_SHUTDOWN_RESP_V02;
	resp_desc.max_msg_len = QMI_SSCTL_RESP_MSG_LENGTH;
	resp_desc.ei_array = qmi_ssctl_shutdown_resp_msg_ei;

	reinit_completion(&data->ind_recv);
	mutex_lock(&sysmon_lock);
	ret = qmi_send_req_wait(data->clnt_handle, &req_desc, &req,
		sizeof(req), &resp_desc, &resp, sizeof(resp), SERVER_TIMEOUT);
	if (ret < 0) {
		pr_err("QMI send req to %s failed, ret - %d\n", dest_ss, ret);
		goto out;
	}

	/* Check the response */
	if (QMI_RESP_BIT_SHIFT(resp.resp.result) != QMI_RESULT_SUCCESS_V01) {
		pr_err("QMI request failed 0x%x\n",
					QMI_RESP_BIT_SHIFT(resp.resp.error));
		ret = -EREMOTEIO;
		goto out;
	}

	shutdown_ack_ret = wait_for_shutdown_ack(dest_desc);
	if (shutdown_ack_ret < 0) {
		pr_err("shutdown_ack SMP2P bit for %s not set\n", data->name);
		if (!&data->ind_recv.done) {
			pr_err("QMI shutdown indication not received\n");
			ret = shutdown_ack_ret;
		}
		goto out;
	} else if (shutdown_ack_ret > 0)
		goto out;

	if (!wait_for_completion_timeout(&data->ind_recv,
					msecs_to_jiffies(SHUTDOWN_TIMEOUT))) {
		pr_err("Timed out waiting for shutdown indication from %s\n",
							data->name);
		ret = -ETIMEDOUT;
	}
out:
	mutex_unlock(&sysmon_lock);
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

static struct elem_info qmi_ssctl_get_failure_reason_req_msg_ei[] = {
	QMI_EOTI_DATA_TYPE
};

static struct elem_info qmi_ssctl_get_failure_reason_resp_msg_ei[] = {
	{
		.data_type = QMI_STRUCT,
		.elem_len  = 1,
		.elem_size = sizeof(struct qmi_response_type_v01),
		.is_array  = NO_ARRAY,
		.tlv_type  = 0x02,
		.offset    = offsetof(
			struct qmi_ssctl_get_failure_reason_resp_msg,
							resp),
		.ei_array  = get_qmi_response_type_v01_ei(),
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
 * subsystem does not respond, and -ENOSYS if the destination subsystem
 * responds with something unexpected.
 *
 * If CONFIG_MSM_SYSMON_COMM is not defined, always return success (0).
 */
int sysmon_get_reason(struct subsys_desc *dest_desc, char *buf, size_t len)
{
	struct msg_desc req_desc, resp_desc;
	struct qmi_ssctl_get_failure_reason_resp_msg resp;
	struct sysmon_qmi_data *data = NULL, *temp;
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

	if (!data->clnt_handle) {
		pr_debug("No SSCTL_V2 support for %s. Revert to SSCTL_V0\n",
								dest_ss);
		ret = sysmon_get_reason_no_qmi(dest_desc, buf, len);
		if (ret)
			pr_debug("SSCTL_V0 implementation failed - %d\n", ret);

		return ret;
	}

	req_desc.msg_id = QMI_SSCTL_GET_FAILURE_REASON_REQ_V02;
	req_desc.max_msg_len = QMI_SSCTL_EMPTY_MSG_LENGTH;
	req_desc.ei_array = qmi_ssctl_get_failure_reason_req_msg_ei;

	resp_desc.msg_id = QMI_SSCTL_GET_FAILURE_REASON_RESP_V02;
	resp_desc.max_msg_len = QMI_SSCTL_ERROR_MSG_LENGTH;
	resp_desc.ei_array = qmi_ssctl_get_failure_reason_resp_msg_ei;

	mutex_lock(&sysmon_lock);
	ret = qmi_send_req_wait(data->clnt_handle, &req_desc, &req,
		sizeof(req), &resp_desc, &resp, sizeof(resp), SERVER_TIMEOUT);
	if (ret < 0) {
		pr_err("QMI send req to %s failed, ret - %d\n", dest_ss, ret);
		goto out;
	}

	/* Check the response */
	if (QMI_RESP_BIT_SHIFT(resp.resp.result) != QMI_RESULT_SUCCESS_V01) {
		pr_err("QMI request failed 0x%x\n",
					QMI_RESP_BIT_SHIFT(resp.resp.error));
		ret = -EREMOTEIO;
		goto out;
	}

	if (!strcmp(resp.error_message, expect)) {
		pr_err("Unexpected response %s\n", resp.error_message);
		ret = -ENOSYS;
		goto out;
	}
	strlcpy(buf, resp.error_message, resp.error_message_len);
out:
	mutex_unlock(&sysmon_lock);
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
	data->clnt_handle = NULL;
	data->legacy_version = false;

	mutex_lock(&sysmon_list_lock);
	if (data->instance_id <= 0) {
		pr_debug("SSCTL instance id not defined\n");
		goto add_list;
	}

	if (sysmon_wq)
		goto notif_register;

	sysmon_wq = create_singlethread_workqueue("sysmon_wq");
	if (!sysmon_wq) {
		mutex_unlock(&sysmon_list_lock);
		pr_err("Could not create workqueue\n");
		kfree(data);
		return -ENOMEM;
	}

notif_register:
	data->notifier.notifier_call = sysmon_svc_event_notify;
	init_completion(&data->ind_recv);

	INIT_WORK(&data->svc_arrive, sysmon_clnt_svc_arrive);
	INIT_WORK(&data->svc_exit, sysmon_clnt_svc_exit);
	INIT_WORK(&data->svc_rcv_msg, sysmon_clnt_recv_msg);

	rc = qmi_svc_event_notifier_register(SSCTL_SERVICE_ID, SSCTL_VER_2,
					data->instance_id, &data->notifier);
	if (rc < 0)
		pr_err("Notifier register failed for %s\n", data->name);
add_list:
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

	if (data == NULL)
		goto exit;

	if (data->instance_id > 0)
		qmi_svc_event_notifier_unregister(SSCTL_SERVICE_ID,
			SSCTL_VER_2, data->instance_id, &data->notifier);

	if (sysmon_wq && list_empty(&sysmon_list))
		destroy_workqueue(sysmon_wq);
exit:
	mutex_unlock(&sysmon_list_lock);
	kfree(data);
}
EXPORT_SYMBOL(sysmon_notifier_unregister);
