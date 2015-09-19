/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "service-notifier: %s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/completion.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/err.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>

#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/sysmon.h>
#include <soc/qcom/service-locator.h>
#include <soc/qcom/service-notifier.h>
#include "service-notifier-private.h"

#define QMI_RESP_BIT_SHIFT(x)			(x << 16)
#define SERVREG_NOTIF_NAME_LENGTH	QMI_SERVREG_NOTIF_NAME_LENGTH_V01
#define SERVREG_NOTIF_SERVICE_ID	SERVREG_NOTIF_SERVICE_ID_V01
#define SERVREG_NOTIF_SERVICE_VERS	SERVREG_NOTIF_SERVICE_VERS_V01

#define SERVREG_NOTIF_SET_ACK_REQ		\
			QMI_SERVREG_NOTIF_STATE_UPDATED_IND_ACK_REQ_V01
#define SERVREG_NOTIF_SET_ACK_REQ_MSG_LEN	\
			QMI_SERVREG_NOTIF_SET_ACK_REQ_MSG_V01_MAX_MSG_LEN
#define SERVREG_NOTIF_SET_ACK_RESP		\
			QMI_SERVREG_NOTIF_STATE_UPDATED_IND_ACK_RESP_V01
#define SERVREG_NOTIF_SET_ACK_RESP_MSG_LEN	\
			QMI_SERVREG_NOTIF_SET_ACK_RESP_MSG_V01_MAX_MSG_LEN
#define SERVREG_NOTIF_STATE_UPDATED_IND_MSG	\
			QMI_SERVREG_NOTIF_STATE_UPDATED_IND_V01
#define SERVREG_NOTIF_STATE_UPDATED_IND_MSG_LEN	\
			QMI_SERVREG_NOTIF_STATE_UPDATED_IND_MSG_V01_MAX_MSG_LEN

#define SERVREG_NOTIF_REGISTER_LISTENER_REQ	\
			QMI_SERVREG_NOTIF_REGISTER_LISTENER_REQ_V01
#define SERVREG_NOTIF_REGISTER_LISTENER_REQ_MSG_LEN \
		QMI_SERVREG_NOTIF_REGISTER_LISTENER_REQ_MSG_V01_MAX_MSG_LEN
#define SERVREG_NOTIF_REGISTER_LISTENER_RESP	\
			QMI_SERVREG_NOTIF_REGISTER_LISTENER_RESP_V01
#define SERVREG_NOTIF_REGISTER_LISTENER_RESP_MSG_LEN \
		QMI_SERVREG_NOTIF_REGISTER_LISTENER_RESP_MSG_V01_MAX_MSG_LEN

#define QMI_STATE_MIN_VAL QMI_SERVREG_NOTIF_SERVICE_STATE_ENUM_TYPE_MIN_VAL_V01
#define QMI_STATE_MAX_VAL QMI_SERVREG_NOTIF_SERVICE_STATE_ENUM_TYPE_MAX_VAL_V01

#define SERVER_TIMEOUT				500
#define MAX_STRING_LEN				100

/*
 * Per user service data structure
 * struct service_notif_info - notifier struct for each unique service path
 * service_path - service provider path/location
 * instance_id - service instance id specific to a subsystem
 * service_notif_rcvr_list - list of clients interested in this service
 *                           providers notifications
 * curr_state: Current state of the service
 */
struct service_notif_info {
	char service_path[SERVREG_NOTIF_NAME_LENGTH];
	int instance_id;
	struct srcu_notifier_head service_notif_rcvr_list;
	struct list_head list;
	int curr_state;
};
static LIST_HEAD(service_list);
static DEFINE_MUTEX(service_list_lock);

struct ind_req_resp {
	char service_path[SERVREG_NOTIF_NAME_LENGTH];
	int transaction_id;
};

/*
 * Per Root Process Domain (Root service) data structure
 * struct qmi_client_info - QMI client info for each subsystem/instance id
 * instance_id - service instance id specific to a subsystem (Root PD)
 * clnt_handle - unique QMI client handle
 * service_connected - indicates if QMI service is up on the subsystem
 * ind_recv - completion variable to record receiving an indication
 * ssr_handle - The SSR handle provided by the SSR driver for the subsystem
 *		on which the remote root PD runs.
 */
struct qmi_client_info {
	int instance_id;
	struct work_struct svc_arrive;
	struct work_struct svc_exit;
	struct work_struct svc_rcv_msg;
	struct work_struct ind_ack;
	struct workqueue_struct *svc_event_wq;
	struct qmi_handle *clnt_handle;
	struct notifier_block notifier;
	void *ssr_handle;
	struct notifier_block ssr_notifier;
	bool service_connected;
	struct completion ind_recv;
	struct list_head list;
	struct ind_req_resp ind_msg;
};
static LIST_HEAD(qmi_client_list);
static DEFINE_MUTEX(qmi_list_lock);

static DEFINE_MUTEX(notif_add_lock);

static void root_service_clnt_recv_msg(struct work_struct *work);
static void root_service_service_arrive(struct work_struct *work);
static void root_service_exit_work(struct work_struct *work);

static struct service_notif_info *_find_service_info(const char *service_path)
{
	struct service_notif_info *service_notif;

	mutex_lock(&service_list_lock);
	list_for_each_entry(service_notif, &service_list, list)
		if (!strcmp(service_notif->service_path, service_path)) {
			mutex_unlock(&service_list_lock);
			return service_notif;
		}
	mutex_unlock(&service_list_lock);
	return NULL;
}

static int service_notif_queue_notification(struct service_notif_info
		*service_notif,
		enum qmi_servreg_notif_service_state_enum_type_v01 notif_type,
		void *info)
{
	int ret = 0;

	if (!service_notif)
		return -EINVAL;

	if ((int) notif_type < QMI_STATE_MIN_VAL ||
					(int) notif_type > QMI_STATE_MAX_VAL)
		return -EINVAL;

	if (service_notif->curr_state == notif_type)
		return 0;

	if (!service_notif->service_notif_rcvr_list.head)
		return 0;

	ret = srcu_notifier_call_chain(&service_notif->service_notif_rcvr_list,
							notif_type, info);
	return ret;
}

static void root_service_clnt_recv_msg(struct work_struct *work)
{
	int ret;
	struct qmi_client_info *data = container_of(work,
					struct qmi_client_info, svc_rcv_msg);

	do {
		pr_debug("Notified about a Receive event (instance-id: %d)\n",
							data->instance_id);
	} while ((ret = qmi_recv_msg(data->clnt_handle)) == 0);

	if (ret != -ENOMSG)
		pr_err("Error receiving message (instance-id: %d)\n",
							data->instance_id);
}

static void root_service_clnt_notify(struct qmi_handle *handle,
			     enum qmi_event_type event, void *notify_priv)
{
	struct qmi_client_info *data = container_of(notify_priv,
					struct qmi_client_info, svc_arrive);

	switch (event) {
	case QMI_RECV_MSG:
		schedule_work(&data->svc_rcv_msg);
		break;
	default:
		break;
	}
}

static void send_ind_ack(struct work_struct *work)
{
	struct qmi_client_info *data = container_of(work,
					struct qmi_client_info, ind_ack);
	struct qmi_servreg_notif_set_ack_req_msg_v01 req;
	struct msg_desc req_desc, resp_desc;
	struct qmi_servreg_notif_set_ack_resp_msg_v01 resp = { { 0, 0 } };
	int rc;

	req.transaction_id = data->ind_msg.transaction_id;
	snprintf(req.service_name, ARRAY_SIZE(req.service_name), "%s",
						data->ind_msg.service_path);

	req_desc.msg_id = SERVREG_NOTIF_SET_ACK_REQ;
	req_desc.max_msg_len = SERVREG_NOTIF_SET_ACK_REQ_MSG_LEN;
	req_desc.ei_array = qmi_servreg_notif_set_ack_req_msg_v01_ei;

	resp_desc.msg_id = SERVREG_NOTIF_SET_ACK_RESP;
	resp_desc.max_msg_len = SERVREG_NOTIF_SET_ACK_RESP_MSG_LEN;
	resp_desc.ei_array = qmi_servreg_notif_set_ack_resp_msg_v01_ei;

	rc = qmi_send_req_wait(data->clnt_handle, &req_desc,
				&req, sizeof(req), &resp_desc, &resp,
				sizeof(resp), SERVER_TIMEOUT);
	if (rc < 0) {
		pr_err("%s: Sending Ack failed/server timeout, ret - %d\n",
						data->ind_msg.service_path, rc);
		goto exit;
	}

	/* Check the response */
	if (QMI_RESP_BIT_SHIFT(resp.resp.result) != QMI_RESULT_SUCCESS_V01)
		pr_err("QMI request failed 0x%x\n",
			QMI_RESP_BIT_SHIFT(resp.resp.error));
	pr_debug("Indication ACKed for transid %d, service %s, instance %d!\n",
		data->ind_msg.transaction_id, data->ind_msg.service_path,
		data->instance_id);
exit:
	complete(&data->ind_recv);
}

static void root_service_service_ind_cb(struct qmi_handle *handle,
				unsigned int msg_id, void *msg,
				unsigned int msg_len, void *ind_cb_priv)
{
	struct qmi_client_info *data = (struct qmi_client_info *)ind_cb_priv;
	struct service_notif_info *service_notif;
	struct msg_desc ind_desc;
	struct qmi_servreg_notif_state_updated_ind_msg_v01 ind_msg;
	int rc;

	ind_desc.msg_id = SERVREG_NOTIF_STATE_UPDATED_IND_MSG;
	ind_desc.max_msg_len = SERVREG_NOTIF_STATE_UPDATED_IND_MSG_LEN;
	ind_desc.ei_array = qmi_servreg_notif_state_updated_ind_msg_v01_ei;
	rc = qmi_kernel_decode(&ind_desc, &ind_msg, msg, msg_len);
	if (rc < 0) {
		pr_err("Failed to decode message!\n");
		goto send_ind_resp;
	}

	pr_debug("Indication received from %s, state: 0x%x, trans-id: %d\n",
		ind_msg.service_name, ind_msg.curr_state,
		ind_msg.transaction_id);

	service_notif = _find_service_info(ind_msg.service_name);
	if (!service_notif)
		return;

	if ((int)ind_msg.curr_state < QMI_STATE_MIN_VAL ||
			(int)ind_msg.curr_state > QMI_STATE_MAX_VAL)
		pr_err("Unexpected indication notification state %d\n",
							ind_msg.curr_state);
	else {
		mutex_lock(&notif_add_lock);
		mutex_lock(&service_list_lock);
		if (service_notif_queue_notification(service_notif,
						ind_msg.curr_state, NULL))
			pr_err("Nnotification failed for %s\n",
							ind_msg.service_name);
		service_notif->curr_state = ind_msg.curr_state;
		mutex_unlock(&service_list_lock);
		mutex_unlock(&notif_add_lock);
	}
send_ind_resp:
	data->ind_msg.transaction_id = ind_msg.transaction_id;
	snprintf(data->ind_msg.service_path,
		ARRAY_SIZE(data->ind_msg.service_path), "%s",
		ind_msg.service_name);
	schedule_work(&data->ind_ack);
	rc = wait_for_completion_timeout(&data->ind_recv, SERVER_TIMEOUT);
	if (rc < 0) {
		pr_err("Timeout waiting for sending indication ACK!");
		return;
	}

}

static int send_notif_listener_msg_req(struct service_notif_info *service_notif,
					struct qmi_client_info *data,
					bool register_notif, int *curr_state)
{
	struct qmi_servreg_notif_register_listener_req_msg_v01 req;
	struct qmi_servreg_notif_register_listener_resp_msg_v01
						resp = { { 0, 0 } };
	struct msg_desc req_desc, resp_desc;
	int rc;

	snprintf(req.service_name, ARRAY_SIZE(req.service_name), "%s",
						service_notif->service_path);
	req.enable = register_notif;

	req_desc.msg_id = SERVREG_NOTIF_REGISTER_LISTENER_REQ;
	req_desc.max_msg_len = SERVREG_NOTIF_REGISTER_LISTENER_REQ_MSG_LEN;
	req_desc.ei_array = qmi_servreg_notif_register_listener_req_msg_v01_ei;

	resp_desc.msg_id = SERVREG_NOTIF_REGISTER_LISTENER_RESP;
	resp_desc.max_msg_len = SERVREG_NOTIF_REGISTER_LISTENER_RESP_MSG_LEN;
	resp_desc.ei_array =
			qmi_servreg_notif_register_listener_resp_msg_v01_ei;

	rc = qmi_send_req_wait(data->clnt_handle, &req_desc, &req, sizeof(req),
				&resp_desc, &resp, sizeof(resp),
				SERVER_TIMEOUT);
	if (rc < 0) {
		pr_err("%s: Message sending failed/server timeout, ret - %d\n",
					service_notif->service_path, rc);
		return rc;
	}

	/* Check the response */
	if (QMI_RESP_BIT_SHIFT(resp.resp.result) != QMI_RESULT_SUCCESS_V01) {
		pr_err("QMI request failed 0x%x\n",
					QMI_RESP_BIT_SHIFT(resp.resp.error));
		return -EREMOTEIO;
	}

	if ((int) resp.curr_state < QMI_STATE_MIN_VAL ||
				(int) resp.curr_state > QMI_STATE_MAX_VAL) {
		pr_err("Invalid notif info 0x%x\n", resp.curr_state);
		rc = -EINVAL;
	}
	*curr_state = resp.curr_state;
	return rc;
}

static int register_notif_listener(struct service_notif_info *service_notif,
					struct qmi_client_info *data,
					int *curr_state)
{
	return send_notif_listener_msg_req(service_notif, data, true,
								curr_state);
}

static void root_service_service_arrive(struct work_struct *work)
{
	struct service_notif_info *service_notif = NULL;
	struct qmi_client_info *data = container_of(work,
					struct qmi_client_info, svc_arrive);
	int rc;
	int curr_state;

	/* Create a Local client port for QMI communication */
	data->clnt_handle = qmi_handle_create(root_service_clnt_notify, work);
	if (!data->clnt_handle) {
		pr_err("QMI client handle alloc failed (instance-id: %d)\n",
							data->instance_id);
		return;
	}

	/* Connect to the service on the root PD service */
	rc = qmi_connect_to_service(data->clnt_handle,
			SERVREG_NOTIF_SERVICE_ID, SERVREG_NOTIF_SERVICE_VERS,
			data->instance_id);
	if (rc < 0) {
		pr_err("Could not connect handle to service(instance-id: %d)\n",
							data->instance_id);
		qmi_handle_destroy(data->clnt_handle);
		data->clnt_handle = NULL;
		return;
	}
	data->service_connected = true;
	pr_info("Connection established between QMI handle and %d service\n",
							data->instance_id);
	/* Register for indication messages about service */
	rc = qmi_register_ind_cb(data->clnt_handle, root_service_service_ind_cb,
							(void *)data);
	if (rc < 0)
		pr_err("Indication callback register failed(instance-id: %d)\n",
							data->instance_id);

	mutex_lock(&notif_add_lock);
	mutex_lock(&service_list_lock);
	list_for_each_entry(service_notif, &service_list, list) {
		if (service_notif->instance_id == data->instance_id) {
			rc = register_notif_listener(service_notif, data,
								&curr_state);
			if (rc) {
				pr_err("Notifier registration failed for %s\n",
						service_notif->service_path);
			} else {
				rc = service_notif_queue_notification(
							service_notif,
							curr_state, NULL);
				if (rc)
					pr_err("Notifier failed for %s\n",
						service_notif->service_path);
				service_notif->curr_state = curr_state;
			}
		}
	}
	mutex_unlock(&service_list_lock);
	mutex_unlock(&notif_add_lock);
}

static void root_service_service_exit(struct qmi_client_info *data)
{
	struct service_notif_info *service_notif = NULL;
	int rc;

	/*
	 * Send service down notifications to all clients
	 * of registered for notifications for that service.
	 */
	mutex_lock(&notif_add_lock);
	mutex_lock(&service_list_lock);
	list_for_each_entry(service_notif, &service_list, list) {
		if (service_notif->instance_id == data->instance_id) {
			rc = service_notif_queue_notification(service_notif,
					SERVREG_NOTIF_SERVICE_STATE_DOWN_V01,
					NULL);
			if (rc)
				pr_err("Notification failed for %s\n",
						service_notif->service_path);
			service_notif->curr_state =
					SERVREG_NOTIF_SERVICE_STATE_DOWN_V01;
		}
	}
	mutex_unlock(&service_list_lock);
	mutex_unlock(&notif_add_lock);

	/*
	 * Destroy client handle and try connecting when
	 * service comes up again.
	 */
	data->service_connected = false;
	qmi_handle_destroy(data->clnt_handle);
	data->clnt_handle = NULL;
}

static void root_service_exit_work(struct work_struct *work)
{
	struct qmi_client_info *data = container_of(work,
					struct qmi_client_info, svc_exit);
	root_service_service_exit(data);
}

static int service_event_notify(struct notifier_block *this,
				      unsigned long code,
				      void *_cmd)
{
	struct qmi_client_info *data = container_of(this,
					struct qmi_client_info, notifier);

	switch (code) {
	case QMI_SERVER_ARRIVE:
		pr_debug("Root PD service UP\n");
		queue_work(data->svc_event_wq, &data->svc_arrive);
		break;
	case QMI_SERVER_EXIT:
		pr_debug("Root PD service DOWN\n");
		queue_work(data->svc_event_wq, &data->svc_exit);
		break;
	default:
		break;
	}
	return 0;
}

static int ssr_event_notify(struct notifier_block *this,
				  unsigned long code,
				  void *data)
{
	struct qmi_client_info *info = container_of(this,
					struct qmi_client_info, ssr_notifier);
	switch (code) {
	case	SUBSYS_BEFORE_SHUTDOWN:
		pr_debug("Root PD service Down (SSR notification)\n");
		root_service_service_exit(info);
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}

static void *add_service_notif(const char *service_path, int instance_id,
							int *curr_state)
{
	struct service_notif_info *service_notif;
	struct qmi_client_info *tmp, *qmi_data;
	long int rc;
	char subsys[SERVREG_NOTIF_NAME_LENGTH];

	rc = find_subsys(service_path, subsys);
	if (rc < 0) {
		pr_err("Could not find subsys for %s\n", service_path);
		return ERR_PTR(rc);
	}

	service_notif = kzalloc(sizeof(struct service_notif_info), GFP_KERNEL);
	if (!service_notif)
		return ERR_PTR(-ENOMEM);

	strlcpy(service_notif->service_path, service_path,
		ARRAY_SIZE(service_notif->service_path));
	service_notif->instance_id = instance_id;

	/* If we already have a connection to the root PD on which the remote
	 * service we are interested in notifications about runs, then use
	 * the existing QMI connection.
	 */
	mutex_lock(&qmi_list_lock);
	list_for_each_entry(tmp, &qmi_client_list, list) {
		if (tmp->instance_id == instance_id) {
			if (tmp->service_connected) {
				rc = register_notif_listener(service_notif, tmp,
								curr_state);
				if (rc) {
					mutex_unlock(&qmi_list_lock);
					pr_err("Register notifier failed: %s",
						service_path);
					kfree(service_notif);
					return ERR_PTR(rc);
				}
				service_notif->curr_state = *curr_state;
			}
			mutex_unlock(&qmi_list_lock);
			goto add_service_list;
		}
	}
	mutex_unlock(&qmi_list_lock);

	qmi_data = kzalloc(sizeof(struct qmi_client_info), GFP_KERNEL);
	if (!qmi_data) {
		kfree(service_notif);
		return ERR_PTR(-ENOMEM);
	}

	qmi_data->instance_id = instance_id;
	qmi_data->clnt_handle = NULL;
	qmi_data->notifier.notifier_call = service_event_notify;
	init_completion(&qmi_data->ind_recv);

	qmi_data->svc_event_wq = create_singlethread_workqueue(subsys);
	if (!qmi_data->svc_event_wq) {
		rc = -ENOMEM;
		goto exit;
	}

	INIT_WORK(&qmi_data->svc_arrive, root_service_service_arrive);
	INIT_WORK(&qmi_data->svc_exit, root_service_exit_work);
	INIT_WORK(&qmi_data->svc_rcv_msg, root_service_clnt_recv_msg);
	INIT_WORK(&qmi_data->ind_ack, send_ind_ack);

	*curr_state = service_notif->curr_state =
				SERVREG_NOTIF_SERVICE_STATE_UNINIT_V01;

	rc = qmi_svc_event_notifier_register(SERVREG_NOTIF_SERVICE_ID,
			SERVREG_NOTIF_SERVICE_VERS, qmi_data->instance_id,
			&qmi_data->notifier);
	if (rc < 0) {
		pr_err("Notifier register failed (instance-id: %d)\n",
							qmi_data->instance_id);
		goto exit;
	}
	qmi_data->ssr_notifier.notifier_call = ssr_event_notify;
	qmi_data->ssr_handle = subsys_notif_register_notifier(subsys,
						&qmi_data->ssr_notifier);
	if (IS_ERR(qmi_data->ssr_handle)) {
		pr_err("SSR notif register for %s failed(instance-id: %d)\n",
			subsys, qmi_data->instance_id);
		rc = PTR_ERR(qmi_data->ssr_handle);
		goto exit;
	}

	mutex_lock(&qmi_list_lock);
	INIT_LIST_HEAD(&qmi_data->list);
	list_add_tail(&qmi_data->list, &qmi_client_list);
	mutex_unlock(&qmi_list_lock);

add_service_list:
	srcu_init_notifier_head(&service_notif->service_notif_rcvr_list);

	mutex_lock(&service_list_lock);
	INIT_LIST_HEAD(&service_notif->list);
	list_add_tail(&service_notif->list, &service_list);
	mutex_unlock(&service_list_lock);

	return service_notif;
exit:
	if (qmi_data->svc_event_wq)
		destroy_workqueue(qmi_data->svc_event_wq);
	kfree(qmi_data);
	kfree(service_notif);
	return ERR_PTR(rc);
}

/* service_notif_register_notifier() - Register a notifier for a service
 * On success, it returns back a handle. It takes the following arguments:
 * service_path: Individual service identifier path for which a client
 *		registers for notifications.
 * instance_id: Instance id specific to a subsystem.
 * current_state: Current state of service returned by the registration
 *		 process.
 * notifier block: notifier callback for service events.
 */
void *service_notif_register_notifier(const char *service_path, int instance_id,
				struct notifier_block *nb, int *curr_state)
{
	struct service_notif_info *service_notif;
	int ret = 0;

	if (!service_path || !instance_id || !nb)
		return ERR_PTR(-EINVAL);

	service_notif = _find_service_info(service_path);
	mutex_lock(&notif_add_lock);
	if (!service_notif) {
		service_notif = (struct service_notif_info *)add_service_notif(
								service_path,
								instance_id,
								curr_state);
		if (IS_ERR(service_notif))
			goto exit;
	}

	ret = srcu_notifier_chain_register(
				&service_notif->service_notif_rcvr_list, nb);
	*curr_state = service_notif->curr_state;
	if (ret < 0)
		service_notif = ERR_PTR(ret);
exit:
	mutex_unlock(&notif_add_lock);
	return service_notif;
}
EXPORT_SYMBOL(service_notif_register_notifier);

/* service_notif_unregister_notifier() - Unregister a notifier for a service.
 * service_notif_handle - The notifier handler that was provided by the
 *			  service_notif_register_notifier function when the
 *			  client registered for notifications.
 * nb - The notifier block that was previously used during the registration.
 */
int service_notif_unregister_notifier(void *service_notif_handle,
					struct notifier_block *nb)
{
	struct service_notif_info *service_notif;

	if (!service_notif_handle || !nb)
		return -EINVAL;

	service_notif = (struct service_notif_info *)service_notif_handle;
	if (service_notif < 0)
		return -EINVAL;

	return srcu_notifier_chain_unregister(
				&service_notif->service_notif_rcvr_list, nb);
}
EXPORT_SYMBOL(service_notif_unregister_notifier);

struct service_notifier_test_data {
	char service_path[MAX_STRING_LEN];
	int instance_id;
	struct notifier_block nb;
	void *service_notif_handle;
};

static struct service_notifier_test_data test_data;

static void print_service_provider_state(int notification, char *type)
{
	if (notification == SERVREG_NOTIF_SERVICE_STATE_DOWN_V01)
		pr_info("%s: Service %s down!\n", type, test_data.service_path);
	else if (notification == SERVREG_NOTIF_SERVICE_STATE_UP_V01)
		pr_info("%s: Service %s up!\n", type, test_data.service_path);
	else if (notification == SERVREG_NOTIF_SERVICE_STATE_UNINIT_V01)
		pr_info("%s: Service %s state uninit!\n", type,
						test_data.service_path);
	else
		pr_info("%s: Service %s state Unknown 0x%x!\n", type,
					test_data.service_path, notification);
}

static int nb_callback(struct notifier_block *nb,
				  unsigned long notification,
				  void *data)
{
	print_service_provider_state((int)notification, "Notification:");
	return 0;
}

static ssize_t show_service_path(struct seq_file *f, void *unused)
{
	if (test_data.service_notif_handle)
		seq_printf(f, "Service Path: %s\n", test_data.service_path);
	else
		seq_puts(f, "No existing notifier\n");
	return 0;
}


static ssize_t set_service_notifier_register(struct file *fp,
						const char __user *buf,
						size_t count, loff_t *ppos)
{
	int curr_state = INT_MAX, rc;

	if (!buf)
		return -EIO;
	if (test_data.service_notif_handle) {
		service_notif_unregister_notifier(
						test_data.service_notif_handle,
						&test_data.nb);
		test_data.service_notif_handle = NULL;
		pr_info("Unregistering existing notifier for %s\n",
							test_data.service_path);
	}
	rc = simple_write_to_buffer(test_data.service_path, MAX_STRING_LEN,
							ppos, buf, count - 1);
	if (rc != count - 1) {
		pr_err("Unable to read data into kernel buffer\n");
		goto err;
	}
	test_data.nb.notifier_call = nb_callback;
	test_data.service_notif_handle = service_notif_register_notifier(
					test_data.service_path,
					test_data.instance_id, &test_data.nb,
					&curr_state);
	if (!IS_ERR(test_data.service_notif_handle)) {
		pr_info("Notifier Registered for service %s\n",
						test_data.service_path);
		print_service_provider_state(curr_state, "Initial State");
		return count;
	}
err:
	test_data.service_notif_handle = NULL;
	pr_err("Unable to register notifier for %s\n", test_data.service_path);
	return -EIO;
}

static int open_service_notifier_register(struct inode *inode, struct file *f)
{
	return single_open(f, (void *) show_service_path,
							inode->i_private);
}

static const struct file_operations service_notifier_register_fops = {
	.open = open_service_notifier_register,
	.read = seq_read,
	.write = set_service_notifier_register,
	.llseek = seq_lseek,
	.release = seq_release,
};

static ssize_t show_service_notifier_id(struct seq_file *f, void *unused)
{
	seq_printf(f, "Service instance ID: %d\n", test_data.instance_id);
	return 0;
}

static ssize_t set_service_notifier_id(struct file *fp,
						const char __user *buf,
						size_t count, loff_t *unused)
{
	int val, rc;
	char kbuf[MAX_STRING_LEN];

	if (MAX_STRING_LEN < count) {
		rc = -EIO;
		goto err;
	}
	rc = copy_from_user(kbuf, buf, count);
	if (rc != 0) {
		rc = -EFAULT;
		goto err;
	}

	kbuf[count - 1] = '\0';
	rc = kstrtoint(kbuf, 0, &val);
	if (rc < 0)
		goto err;

	test_data.instance_id = val;
	return count;
err:
	pr_err("Invalid input parameters: rc = %d\n", rc);
	return rc;
}

static int open_service_notifier_id(struct inode *inode, struct file *f)
{
	return single_open(f, (void *) show_service_notifier_id,
							inode->i_private);
}

static const struct file_operations service_notifier_id_fops = {
	.open = open_service_notifier_id,
	.read = seq_read,
	.write = set_service_notifier_id,
	.llseek = seq_lseek,
	.release = seq_release,
};

static struct dentry *service_notifier_dir;
static struct dentry *service_path_file;
static struct dentry *service_id_file;

static int __init service_notifier_init(void)
{
	service_notifier_dir = debugfs_create_dir("service_notifier", NULL);
	if (service_notifier_dir) {
		service_path_file = debugfs_create_file("service_path",
				S_IRUGO | S_IWUSR, service_notifier_dir, NULL,
				&service_notifier_register_fops);
		if (!service_path_file)
			goto err;
		service_id_file = debugfs_create_file("service_id",
				S_IRUGO | S_IWUSR, service_notifier_dir, NULL,
				&service_notifier_id_fops);
		if (!service_id_file)
			goto err;
	}
	return 0;
err:
	debugfs_remove_recursive(service_notifier_dir);
	return 0;
}

static void __exit service_notifier_exit(void)
{
	debugfs_remove_recursive(service_notifier_dir);
	test_data.nb.notifier_call = nb_callback;
}
module_init(service_notifier_init);
module_exit(service_notifier_exit);
