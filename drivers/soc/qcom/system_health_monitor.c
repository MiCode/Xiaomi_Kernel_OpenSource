/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/ipc_logging.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/qmi_encdec.h>
#include <linux/ratelimit.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/srcu.h>
#include <linux/thread_info.h>
#include <linux/uaccess.h>

#include <soc/qcom/msm_qmi_interface.h>
#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/subsystem_restart.h>

#include "system_health_monitor_v01.h"

#define MODULE_NAME "system_health_monitor"

#define SUBSYS_NAME_LEN 256
#define SSRESTART_STRLEN 256

enum {
	SHM_INFO_FLAG = 0x1,
	SHM_DEBUG_FLAG = 0x2,
};
static int shm_debug_mask = SHM_INFO_FLAG;
module_param_named(debug_mask, shm_debug_mask,
		   int, S_IRUGO | S_IWUSR | S_IWGRP);
static int shm_default_timeout_ms = 2000;
module_param_named(default_timeout_ms, shm_default_timeout_ms,
		   int, S_IRUGO | S_IWUSR | S_IWGRP);

#define DEFAULT_SHM_RATELIMIT_INTERVAL (HZ / 5)
#define DEFAULT_SHM_RATELIMIT_BURST 2

#define SHM_ILCTXT_NUM_PAGES 2
static void *shm_ilctxt;
#define SHM_INFO(x...) do { \
	if ((shm_debug_mask & SHM_INFO_FLAG) && shm_ilctxt) \
		ipc_log_string(shm_ilctxt, x); \
} while (0)

#define SHM_DEBUG(x...) do { \
	if ((shm_debug_mask & SHM_DEBUG_FLAG) && shm_ilctxt) \
		ipc_log_string(shm_ilctxt, x); \
} while (0)

#define SHM_ERR(x...) do { \
	if (shm_ilctxt) \
		ipc_log_string(shm_ilctxt, x); \
	pr_err(x); \
} while (0)

struct class *system_health_monitor_classp;
static dev_t system_health_monitor_dev;
static struct cdev system_health_monitor_cdev;
static struct device *system_health_monitor_devp;

#define SYSTEM_HEALTH_MONITOR_IOCTL_MAGIC (0xC3)

#define CHECK_SYSTEM_HEALTH_IOCTL \
	_IOR(SYSTEM_HEALTH_MONITOR_IOCTL_MAGIC, 0, unsigned int)

static struct workqueue_struct *shm_svc_workqueue;
static void shm_svc_recv_msg(struct work_struct *work);
static DECLARE_DELAYED_WORK(work_recv_msg, shm_svc_recv_msg);
static struct qmi_handle *shm_svc_handle;

struct disconnect_work {
	struct work_struct work;
	void *conn_h;
};
static void shm_svc_disconnect_worker(struct work_struct *work);

struct req_work {
	struct work_struct work;
	void *conn_h;
	void *req_h;
	unsigned int msg_id;
	void *req;
};
static void shm_svc_req_worker(struct work_struct *work);

/**
 * struct hma_info - Information about a Health Monitor Agent(HMA)
 * @list:		List to chain up the hma to the hma_list.
 * @subsys_name:	Name of the remote subsystem that hosts this HMA.
 * @ssrestart_string:	String to restart the subsystem that hosts this HMA.
 * @conn_h:		Opaque connection handle to the HMA.
 * @timeout:		Timeout as registered by the HMA.
 * @check_count:	Count of the health check attempts.
 * @report_count:	Count of the health reports handled.
 * @reset_srcu:		Sleepable RCU to protect the reset state.
 * @is_in_reset:	Flag to identify if the remote subsystem is in reset.
 * @restart_nb:		Notifier block to receive subsystem restart events.
 * @restart_nb_h:	Handle to subsystem restart notifier block.
 * @rs:			Rate-limit the health check.
 */
struct hma_info {
	struct list_head list;
	char subsys_name[SUBSYS_NAME_LEN];
	char ssrestart_string[SSRESTART_STRLEN];
	void *conn_h;
	uint32_t timeout;
	atomic_t check_count;
	atomic_t report_count;
	struct srcu_struct reset_srcu;
	atomic_t is_in_reset;
	struct notifier_block restart_nb;
	void *restart_nb_h;
	struct ratelimit_state rs;
};

struct restart_work {
	struct delayed_work dwork;
	struct hma_info *hmap;
	void *conn_h;
	int check_count;
};
static void shm_svc_restart_worker(struct work_struct *work);

static DEFINE_MUTEX(hma_info_list_lock);
static LIST_HEAD(hma_info_list);

static struct msg_desc shm_svc_register_req_desc = {
	.max_msg_len = HMON_REGISTER_REQ_MSG_V01_MAX_MSG_LEN,
	.msg_id = QMI_HEALTH_MON_REG_REQ_V01,
	.ei_array = hmon_register_req_msg_v01_ei,
};

static struct msg_desc shm_svc_register_resp_desc = {
	.max_msg_len = HMON_REGISTER_RESP_MSG_V01_MAX_MSG_LEN,
	.msg_id = QMI_HEALTH_MON_REG_RESP_V01,
	.ei_array = hmon_register_resp_msg_v01_ei,
};

static struct msg_desc shm_svc_health_check_ind_desc = {
	.max_msg_len = HMON_HEALTH_CHECK_IND_MSG_V01_MAX_MSG_LEN,
	.msg_id = QMI_HEALTH_MON_HEALTH_CHECK_IND_V01,
	.ei_array = hmon_health_check_ind_msg_v01_ei,
};

static struct msg_desc shm_svc_health_check_complete_req_desc = {
	.max_msg_len = HMON_HEALTH_CHECK_COMPLETE_REQ_MSG_V01_MAX_MSG_LEN,
	.msg_id = QMI_HEALTH_MON_HEALTH_CHECK_COMPLETE_REQ_V01,
	.ei_array = hmon_health_check_complete_req_msg_v01_ei,
};

static struct msg_desc shm_svc_health_check_complete_resp_desc = {
	.max_msg_len = HMON_HEALTH_CHECK_COMPLETE_RESP_MSG_V01_MAX_MSG_LEN,
	.msg_id = QMI_HEALTH_MON_HEALTH_CHECK_COMPLETE_RESP_V01,
	.ei_array = hmon_health_check_complete_resp_msg_v01_ei,
};

/**
 * restart_notifier_cb() - Callback to handle SSR events
 * @this:	Reference to the notifier block.
 * @code:	Type of SSR event.
 * @data:	Data that needs to be handled as part of SSR event.
 *
 * This function is used to identify if a subsystem which hosts an HMA
 * is already in reset, so that a duplicate subsystem restart is not
 * triggered.
 *
 * Return: 0 on success, standard Linux error codes on failure.
 */
static int restart_notifier_cb(struct notifier_block *this,
			       unsigned long code, void *data)
{
	struct hma_info *tmp_hma_info =
		container_of(this, struct hma_info, restart_nb);

	if (code == SUBSYS_BEFORE_SHUTDOWN) {
		atomic_set(&tmp_hma_info->is_in_reset, 1);
		synchronize_srcu(&tmp_hma_info->reset_srcu);
		SHM_INFO("%s: %s going to shutdown\n",
			 __func__, tmp_hma_info->ssrestart_string);
	} else if (code == SUBSYS_AFTER_POWERUP) {
		atomic_set(&tmp_hma_info->is_in_reset, 0);
		SHM_INFO("%s: %s powered up\n",
			 __func__, tmp_hma_info->ssrestart_string);
	}
	return 0;
}

/**
 * shm_svc_restart_worker() - Worker to restart a subsystem
 * @work:	Reference to the work item being handled.
 *
 * This function restarts the subsystem which hosts an HMA. This function
 * checks the following before triggering a restart:
 * 1) Health check report is not received.
 * 2) The subsystem has not undergone a reset.
 * 3) The subsystem is not undergoing a reset.
 */
static void shm_svc_restart_worker(struct work_struct *work)
{
	int rc;
	struct delayed_work *dwork = to_delayed_work(work);
	struct restart_work *rwp =
		container_of(dwork, struct restart_work, dwork);
	struct hma_info *tmp_hma_info = rwp->hmap;
	int rcu_id;

	if (rwp->check_count <= atomic_read(&tmp_hma_info->report_count)) {
		SHM_INFO("%s: No Action on Health Check Attempt %d to %s\n",
			 __func__, rwp->check_count,
			 tmp_hma_info->subsys_name);
		kfree(rwp);
		return;
	}

	if (!tmp_hma_info->conn_h || rwp->conn_h != tmp_hma_info->conn_h) {
		SHM_INFO("%s: Connection to %s is reset. No further action\n",
			 __func__, tmp_hma_info->subsys_name);
		kfree(rwp);
		return;
	}

	rcu_id = srcu_read_lock(&tmp_hma_info->reset_srcu);
	if (atomic_read(&tmp_hma_info->is_in_reset)) {
		SHM_INFO("%s: %s is going thru restart. No further action\n",
			 __func__, tmp_hma_info->subsys_name);
		srcu_read_unlock(&tmp_hma_info->reset_srcu, rcu_id);
		kfree(rwp);
		return;
	}

	SHM_ERR("%s: HMA in %s failed to respond in time. Restarting %s...\n",
		__func__, tmp_hma_info->subsys_name,
		tmp_hma_info->ssrestart_string);
	rc = subsystem_restart(tmp_hma_info->ssrestart_string);
	if (rc < 0)
		SHM_ERR("%s: Error %d restarting %s\n",
			__func__, rc, tmp_hma_info->ssrestart_string);
	srcu_read_unlock(&tmp_hma_info->reset_srcu, rcu_id);
	kfree(rwp);
}

/**
 * shm_send_health_check_ind() - Initiate a subsystem health check
 * @tmp_hma_info:	Info about an HMA which resides in a subsystem.
 *
 * This function initiates a health check of a subsytem, which hosts the
 * HMA, by sending a health check QMI indication message.
 *
 * Return: 0 on success, standard Linux error codes on failure.
 */
static int shm_send_health_check_ind(struct hma_info *tmp_hma_info)
{
	int rc;
	struct restart_work *rwp;

	if (!tmp_hma_info->conn_h)
		return 0;

	/* Rate limit the health check as configured by the subsystem */
	if (!__ratelimit(&tmp_hma_info->rs))
		return 0;

	rwp = kzalloc(sizeof(*rwp), GFP_KERNEL);
	if (!rwp) {
		SHM_ERR("%s: Error allocating restart work\n", __func__);
		return -ENOMEM;
	}

	INIT_DELAYED_WORK(&rwp->dwork, shm_svc_restart_worker);
	rwp->hmap = tmp_hma_info;
	rwp->conn_h = tmp_hma_info->conn_h;

	rc = qmi_send_ind(shm_svc_handle, tmp_hma_info->conn_h,
			  &shm_svc_health_check_ind_desc, NULL, 0);
	if (rc < 0) {
		SHM_ERR("%s: Send Error %d to %s\n",
			__func__, rc, tmp_hma_info->subsys_name);
		kfree(rwp);
		return rc;
	}

	rwp->check_count = atomic_inc_return(&tmp_hma_info->check_count);
	queue_delayed_work(shm_svc_workqueue, &rwp->dwork,
			   msecs_to_jiffies(tmp_hma_info->timeout));
	return 0;
}

/**
 * kern_check_system_health() - Check the system health
 *
 * This function is used by the kernel drivers to initiate the
 * system health check. This function in turn triggers SHM to send
 * QMI message to all the HMAs connected to it.
 *
 * Return: 0 on success, standard Linux error codes on failure.
 */
int kern_check_system_health(void)
{
	int rc;
	int final_rc = 0;
	struct hma_info *tmp_hma_info;

	mutex_lock(&hma_info_list_lock);
	list_for_each_entry(tmp_hma_info, &hma_info_list, list) {
		rc = shm_send_health_check_ind(tmp_hma_info);
		if (rc < 0) {
			SHM_ERR("%s by %s failed for %s - rc %d\n", __func__,
				current->comm, tmp_hma_info->subsys_name, rc);
			final_rc = rc;
		}
	}
	mutex_unlock(&hma_info_list_lock);
	return final_rc;
}
EXPORT_SYMBOL(kern_check_system_health);

/**
 * shm_svc_connect_cb() - Callback to handle connect event from an HMA
 * @handle:	QMI Service handle in which a connect event is received.
 * @conn_h:	Opaque reference to the connection handle.
 *
 * Return: 0 on success, standard Linux error codes on failure.
 */
static int shm_svc_connect_cb(struct qmi_handle *handle, void *conn_h)
{
	SHM_DEBUG("%s: conn_h %p\n", __func__, conn_h);
	return 0;
}

/**
 * shm_svc_disconnect_worker() - Worker to handle disconnect event from an HMA
 * @work:	Reference to the work item.
 *
 * This function handles the disconnect event from an HMA in a deferred manner.
 */
static void shm_svc_disconnect_worker(struct work_struct *work)
{
	struct hma_info *tmp_hma_info;
	struct disconnect_work *dwp =
		container_of(work, struct disconnect_work, work);

	mutex_lock(&hma_info_list_lock);
	list_for_each_entry(tmp_hma_info, &hma_info_list, list) {
		if (dwp->conn_h == tmp_hma_info->conn_h) {
			SHM_INFO("%s: conn_h %p to HMA in %s exited\n",
				 __func__, dwp->conn_h,
				 tmp_hma_info->subsys_name);
			tmp_hma_info->conn_h = NULL;
			atomic_set(&tmp_hma_info->report_count,
				   atomic_read(&tmp_hma_info->check_count));
			break;
		}
	}
	mutex_unlock(&hma_info_list_lock);
	kfree(dwp);
}

/**
 * shm_svc_disconnect_cb() - Callback to handle disconnect event from an HMA
 * @handle:	QMI Service handle in which a disconnect event is received.
 * @conn_h:	Opaque reference to the connection handle.
 *
 * Return: 0 on success, standard Linux error codes on failure.
 */
static int shm_svc_disconnect_cb(struct qmi_handle *handle, void *conn_h)
{
	struct disconnect_work *dwp;

	dwp = kzalloc(sizeof(*dwp), GFP_ATOMIC);
	if (!dwp) {
		SHM_ERR("%s: Error allocating work item\n", __func__);
		return -ENOMEM;
	}

	INIT_WORK(&dwp->work, shm_svc_disconnect_worker);
	dwp->conn_h = conn_h;
	queue_work(shm_svc_workqueue, &dwp->work);
	return 0;
}

/**
 * shm_svc_req_desc_cb() - Callback to identify the request descriptor
 * @msg_id:	Message ID of the QMI request.
 * @req_desc:	Request Descriptor of the QMI request.
 *
 * Return: 0 on success, standard Linux error codes on failure.
 */
static int shm_svc_req_desc_cb(unsigned int msg_id,
			       struct msg_desc **req_desc)
{
	int rc;
	SHM_DEBUG("%s: called for msg_id %d\n", __func__, msg_id);
	switch (msg_id) {
	case QMI_HEALTH_MON_REG_REQ_V01:
		*req_desc = &shm_svc_register_req_desc;
		rc = sizeof(struct hmon_register_req_msg_v01);
		break;

	case QMI_HEALTH_MON_HEALTH_CHECK_COMPLETE_REQ_V01:
		*req_desc = &shm_svc_health_check_complete_req_desc;
		rc = sizeof(struct hmon_health_check_complete_req_msg_v01);
		break;

	default:
		SHM_ERR("%s: Invalid msg_id %d\n", __func__, msg_id);
		rc = -ENOTSUPP;
	}
	return rc;
}

/**
 * handle_health_mon_reg_req() - Handle the HMA register QMI request
 * @conn_h:	Opaque reference to the connection handle to an HMA.
 * @req_h:	Opaque reference to the request handle.
 * @buf:	Pointer to the QMI request structure.
 *
 * This function handles the register request from an HMA. The request
 * contains the subsystem name which hosts the HMA and health check
 * timeout for the HMA.
 *
 * Return: 0 on success, standard Linux error codes on failure.
 */
static int handle_health_mon_reg_req(void *conn_h, void *req_h, void *buf)
{
	int rc;
	struct hma_info *tmp_hma_info;
	struct hmon_register_req_msg_v01 *req =
		(struct hmon_register_req_msg_v01 *)buf;
	struct hmon_register_resp_msg_v01 resp;
	bool hma_info_found = false;

	if (!req->name_valid) {
		SHM_ERR("%s: host name invalid\n", __func__);
		goto send_reg_resp;
	}

	mutex_lock(&hma_info_list_lock);
	list_for_each_entry(tmp_hma_info, &hma_info_list, list) {
		if (!strcmp(tmp_hma_info->subsys_name, req->name) &&
		    !tmp_hma_info->conn_h) {
			tmp_hma_info->conn_h = conn_h;
			if (req->timeout_valid)
				tmp_hma_info->timeout = req->timeout;
			else
				tmp_hma_info->timeout = shm_default_timeout_ms;
			ratelimit_state_init(&tmp_hma_info->rs,
					     DEFAULT_SHM_RATELIMIT_INTERVAL,
					     DEFAULT_SHM_RATELIMIT_BURST);
			SHM_INFO("%s: from %s timeout_ms %d\n",
				 __func__, req->name, tmp_hma_info->timeout);
			hma_info_found = true;
		} else if (!strcmp(tmp_hma_info->subsys_name, req->name)) {
			SHM_ERR("%s: Duplicate HMA from %s - cur %p, new %p\n",
				__func__, req->name, tmp_hma_info->conn_h,
				conn_h);
		}
	}
	mutex_unlock(&hma_info_list_lock);

send_reg_resp:
	if (hma_info_found) {
		memset(&resp, 0, sizeof(resp));
	} else {
		resp.resp.result = QMI_RESULT_FAILURE_V01;
		resp.resp.error = QMI_ERR_INVALID_ID_V01;
	}
	rc = qmi_send_resp(shm_svc_handle, conn_h, req_h,
			   &shm_svc_register_resp_desc, &resp, sizeof(resp));
	if (rc < 0)
		SHM_ERR("%s: send_resp failed to %s - rc %d\n",
			__func__, req->name, rc);
	return rc;
}

/**
 * handle_health_mon_health_check_complete_req() - Handle the HMA health report
 * @conn_h:	Opaque reference to the connection handle to an HMA.
 * @req_h:	Opaque reference to the request handle.
 * @buf:	Pointer to the QMI request structure.
 *
 * This function handles health reports from an HMA. The health report is sent
 * in response to a health check QMI indication sent by SHM.
 *
 * Return: 0 on success, standard Linux error codes on failure.
 */
static int handle_health_mon_health_check_complete_req(void *conn_h,
						void *req_h, void *buf)
{
	int rc;
	struct hma_info *tmp_hma_info;
	struct hmon_health_check_complete_req_msg_v01 *req =
		(struct hmon_health_check_complete_req_msg_v01 *)buf;
	struct hmon_health_check_complete_resp_msg_v01 resp;
	bool hma_info_found = false;

	if (!req->result_valid) {
		SHM_ERR("%s: Invalid result\n", __func__);
		goto send_resp;
	}

	mutex_lock(&hma_info_list_lock);
	list_for_each_entry(tmp_hma_info, &hma_info_list, list) {
		if (tmp_hma_info->conn_h != conn_h)
			continue;
		hma_info_found = true;
		if (req->result == HEALTH_MONITOR_CHECK_SUCCESS_V01) {
			atomic_inc(&tmp_hma_info->report_count);
			SHM_INFO("%s: %s Health Check Success\n",
				 __func__, tmp_hma_info->subsys_name);
		} else {
			SHM_INFO("%s: %s Health Check Failure\n",
				 __func__, tmp_hma_info->subsys_name);
		}
	}
	mutex_unlock(&hma_info_list_lock);

send_resp:
	if (hma_info_found) {
		memset(&resp, 0, sizeof(resp));
	} else {
		resp.resp.result = QMI_RESULT_FAILURE_V01;
		resp.resp.error = QMI_ERR_INVALID_ID_V01;
	}
	rc = qmi_send_resp(shm_svc_handle, conn_h, req_h,
			   &shm_svc_health_check_complete_resp_desc,
			   &resp, sizeof(resp));
	if (rc < 0)
		SHM_ERR("%s: send_resp failed - rc %d\n",
			__func__, rc);
	return rc;
}

/**
 * shm_svc_req_worker() - Worker to handle QMI requests
 * @work:	Reference to the work item.
 *
 * This function handles QMI requests from HMAs in a deferred manner.
 */
static void shm_svc_req_worker(struct work_struct *work)
{
	struct req_work *rwp =
		container_of(work, struct req_work, work);

	switch (rwp->msg_id) {
	case QMI_HEALTH_MON_REG_REQ_V01:
		handle_health_mon_reg_req(rwp->conn_h, rwp->req_h, rwp->req);
		break;

	case QMI_HEALTH_MON_HEALTH_CHECK_COMPLETE_REQ_V01:
		handle_health_mon_health_check_complete_req(rwp->conn_h,
						rwp->req_h, rwp->req);
		break;
	default:
		SHM_ERR("%s: Invalid msg_id %d\n", __func__, rwp->msg_id);
	}
	kfree(rwp->req);
	kfree(rwp);
}

/**
 * shm_svc_req_cb() - Callback to notify about QMI requests from HMA
 * @handle;	QMI Service handle in which the request is received.
 * @conn_h:	Opaque reference to the connection handle to an HMA.
 * @req_h:	Opaque reference to the request handle.
 * @msg_id:	Message ID of the request.
 * @req:	Pointer to the request structure.
 *
 * This function is called by kernel QMI Service Interface to notify the
 * incoming QMI request on the SHM service handle.
 *
 * Return: 0 on success, standard Linux error codes on failure.
 */
static int shm_svc_req_cb(struct qmi_handle *handle, void *conn_h,
			  void *req_h, unsigned int msg_id, void *req)
{
	struct req_work *rwp;
	void *req_buf;
	uint32_t req_sz = 0;

	rwp = kzalloc(sizeof(*rwp), GFP_KERNEL);
	if (!rwp) {
		SHM_ERR("%s: Error allocating work item\n", __func__);
		return -ENOMEM;
	}

	switch (msg_id) {
	case QMI_HEALTH_MON_REG_REQ_V01:
		req_sz = sizeof(struct hmon_register_req_msg_v01);
		break;

	case QMI_HEALTH_MON_HEALTH_CHECK_COMPLETE_REQ_V01:
		req_sz = sizeof(struct hmon_health_check_complete_req_msg_v01);
		break;

	default:
		SHM_ERR("%s: Invalid msg_id %d\n", __func__, msg_id);
		kfree(rwp);
		return -ENOTSUPP;
	}

	req_buf = kzalloc(req_sz, GFP_KERNEL);
	if (!req_buf) {
		SHM_ERR("%s: Error allocating request buffer\n", __func__);
		kfree(rwp);
		return -ENOMEM;
	}
	memcpy(req_buf, req, req_sz);

	INIT_WORK(&rwp->work, shm_svc_req_worker);
	rwp->conn_h = conn_h;
	rwp->req_h = req_h;
	rwp->msg_id = msg_id;
	rwp->req = req_buf;
	queue_work(shm_svc_workqueue, &rwp->work);
	return 0;
}

/**
 * shm_svc_recv_msg() - Worker to receive a QMI message
 * @work:	Reference to the work item.
 *
 * This function handles any incoming QMI messages to the SHM QMI service.
 */
static void shm_svc_recv_msg(struct work_struct *work)
{
	int rc;

	do {
		SHM_DEBUG("%s: Notified about a receive event\n", __func__);
	} while ((rc = qmi_recv_msg(shm_svc_handle)) == 0);

	if (rc != -ENOMSG)
		SHM_ERR("%s: Error %d receiving message\n", __func__, rc);
}

/**
 * shm_svc_notify() - Callback function to receive SHM QMI service events
 * @handle:	QMI handle in which the event is received.
 * @event:	Type of the QMI event.
 * @priv:	Opaque reference to the private data as registered by the
 *		service.
 */
static void shm_svc_notify(struct qmi_handle *handle,
			   enum qmi_event_type event, void *priv)
{
	switch (event) {
	case QMI_RECV_MSG:
		queue_delayed_work(shm_svc_workqueue, &work_recv_msg, 0);
		break;
	default:
		break;
	}
}

static struct qmi_svc_ops_options shm_svc_ops_options = {
	.version = 1,
	.service_id = HMON_SERVICE_ID_V01,
	.service_vers = HMON_SERVICE_VERS_V01,
	.service_ins = 0,
	.connect_cb = shm_svc_connect_cb,
	.disconnect_cb = shm_svc_disconnect_cb,
	.req_desc_cb = shm_svc_req_desc_cb,
	.req_cb = shm_svc_req_cb,
};

static int system_health_monitor_open(struct inode *inode, struct file *file)
{
	SHM_DEBUG("%s by %s\n", __func__, current->comm);
	return 0;
}

static int system_health_monitor_release(struct inode *inode,
					  struct file *file)
{
	SHM_DEBUG("%s by %s\n", __func__, current->comm);
	return 0;
}

static ssize_t system_health_monitor_write(struct file *file,
	const char __user *buf, size_t count, loff_t *ppos)
{
	SHM_ERR("%s by %s\n", __func__, current->comm);
	return -ENOTSUPP;
}

static ssize_t system_health_monitor_read(struct file *file, char __user *buf,
			    size_t count, loff_t *ppos)
{
	SHM_ERR("%s by %s\n", __func__, current->comm);
	return -ENOTSUPP;
}

static long system_health_monitor_ioctl(struct file *file, unsigned int cmd,
			  unsigned long arg)
{
	int rc;

	switch (cmd) {
	case CHECK_SYSTEM_HEALTH_IOCTL:
		SHM_INFO("%s by %s\n", __func__, current->comm);
		rc = kern_check_system_health();
		break;
	default:
		SHM_ERR("%s: Invalid cmd %d by %s\n",
			__func__, cmd, current->comm);
		rc = -EINVAL;
	}
	return rc;
}

static const struct file_operations system_health_monitor_fops = {
	.owner = THIS_MODULE,
	.open = system_health_monitor_open,
	.release = system_health_monitor_release,
	.read = system_health_monitor_read,
	.write = system_health_monitor_write,
	.unlocked_ioctl = system_health_monitor_ioctl,
	.compat_ioctl = system_health_monitor_ioctl,
};

/**
 * start_system_health_monitor_service() - Start the SHM QMI service
 *
 * This function registers the SHM QMI service, if it is not already
 * registered.
 */
static int start_system_health_monitor_service(void)
{
	int rc;

	shm_svc_workqueue = create_singlethread_workqueue("shm_svc");
	if (!shm_svc_workqueue) {
		SHM_ERR("%s: Error creating workqueue\n", __func__);
		return -EFAULT;
	}

	shm_svc_handle = qmi_handle_create(shm_svc_notify, NULL);
	if (!shm_svc_handle) {
		SHM_ERR("%s: Creating shm_svc_handle failed\n", __func__);
		rc = -ENOMEM;
		goto start_svc_error1;
	}

	rc = qmi_svc_register(shm_svc_handle, &shm_svc_ops_options);
	if (rc < 0) {
		SHM_ERR("%s: Registering shm svc failed - %d\n", __func__, rc);
		goto start_svc_error2;
	}
	return 0;
start_svc_error2:
	qmi_handle_destroy(shm_svc_handle);
start_svc_error1:
	destroy_workqueue(shm_svc_workqueue);
	return rc;
}

/**
 * parse_devicetree() - Parse the device tree for HMA information
 * @node:	Pointer to the device tree node.
 * @hma:	HMA information which needs to be extracted.
 *
 * This function parses the device tree, extracts the HMA information.
 *
 * Return: 0 on success, standard Linux error codes on failure.
 */
static int parse_devicetree(struct device_node *node,
			    struct hma_info *hma)
{
	char *key;
	const char *subsys_name;
	const char *ssrestart_string;

	key = "qcom,subsys-name";
	subsys_name = of_get_property(node, key, NULL);
	if (!subsys_name)
		goto error;
	strlcpy(hma->subsys_name, subsys_name, SUBSYS_NAME_LEN);

	key = "qcom,ssrestart-string";
	ssrestart_string = of_get_property(node, key, NULL);
	if (!ssrestart_string)
		goto error;
	strlcpy(hma->ssrestart_string, ssrestart_string, SSRESTART_STRLEN);
	return 0;
error:
	SHM_ERR("%s: missing key: %s\n", __func__, key);
	return -ENODEV;
}

/**
 * system_health_monitor_probe() - Probe function to construct HMA info
 * @pdev:	Platform device pointing to a device tree node.
 *
 * This function extracts the HMA information from the device tree, constructs
 * it and adds it to the global list.
 *
 * Return: 0 on success, standard Linux error codes on failure.
 */
static int system_health_monitor_probe(struct platform_device *pdev)
{
	int rc;
	struct hma_info *hma, *tmp_hma;
	struct device_node *node;

	mutex_lock(&hma_info_list_lock);
	for_each_child_of_node(pdev->dev.of_node, node) {
		hma = kzalloc(sizeof(*hma), GFP_KERNEL);
		if (!hma) {
			SHM_ERR("%s: Error allocation hma_info\n", __func__);
			rc = -ENOMEM;
			goto probe_err;
		}

		rc = parse_devicetree(node, hma);
		if (rc) {
			SHM_ERR("%s Failed to parse Device Tree\n", __func__);
			kfree(hma);
			goto probe_err;
		}

		init_srcu_struct(&hma->reset_srcu);
		hma->restart_nb.notifier_call = restart_notifier_cb;
		hma->restart_nb_h = subsys_notif_register_notifier(
				hma->ssrestart_string, &hma->restart_nb);
		if (IS_ERR_OR_NULL(hma->restart_nb_h)) {
			cleanup_srcu_struct(&hma->reset_srcu);
			kfree(hma);
			rc = -EFAULT;
			SHM_ERR("%s: Error registering restart notif for %s\n",
				__func__, hma->ssrestart_string);
			goto probe_err;
		}

		list_add_tail(&hma->list, &hma_info_list);
		SHM_INFO("%s: Added HMA info for %s\n",
			 __func__, hma->subsys_name);
	}

	rc = start_system_health_monitor_service();
	if (rc) {
		SHM_ERR("%s Failed to start service %d\n", __func__, rc);
		goto probe_err;
	}
	mutex_unlock(&hma_info_list_lock);
	return 0;
probe_err:
	list_for_each_entry_safe(hma, tmp_hma, &hma_info_list, list) {
		list_del(&hma->list);
		subsys_notif_unregister_notifier(hma->restart_nb_h,
						 &hma->restart_nb);
		cleanup_srcu_struct(&hma->reset_srcu);
		kfree(hma);
	}
	mutex_unlock(&hma_info_list_lock);
	return rc;
}

static struct of_device_id system_health_monitor_match_table[] = {
	{ .compatible = "qcom,system-health-monitor" },
	{},
};

static struct platform_driver system_health_monitor_driver = {
	.probe = system_health_monitor_probe,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = system_health_monitor_match_table,
	},
};

/**
 * system_health_monitor_init() - Initialize the system health monitor module
 *
 * This functions registers a platform driver to probe for and extract the HMA
 * information. This function registers the character device interface to the
 * user-space.
 *
 * Return: 0 on success, standard Linux error codes on failure.
 */
static int __init system_health_monitor_init(void)
{
	int rc;

	shm_ilctxt = ipc_log_context_create(SHM_ILCTXT_NUM_PAGES, "shm", 0);
	if (!shm_ilctxt) {
		SHM_ERR("%s: Unable to create SHM logging context\n", __func__);
		shm_debug_mask = 0;
	}

	rc = platform_driver_register(&system_health_monitor_driver);
	if (rc) {
		SHM_ERR("%s: system_health_monitor_driver register failed %d\n",
			__func__, rc);
		return rc;
	}

	rc = alloc_chrdev_region(&system_health_monitor_dev,
				 0, 1, "system_health_monitor");
	if (rc < 0) {
		SHM_ERR("%s: alloc_chrdev_region() failed %d\n", __func__, rc);
		return rc;
	}

	system_health_monitor_classp = class_create(THIS_MODULE,
						"system_health_monitor");
	if (IS_ERR_OR_NULL(system_health_monitor_classp)) {
		SHM_ERR("%s: class_create() failed\n", __func__);
		rc = -ENOMEM;
		goto init_error1;
	}

	cdev_init(&system_health_monitor_cdev, &system_health_monitor_fops);
	system_health_monitor_cdev.owner = THIS_MODULE;
	rc = cdev_add(&system_health_monitor_cdev,
		      system_health_monitor_dev , 1);
	if (rc < 0) {
		SHM_ERR("%s: cdev_add() failed - rc %d\n",
			__func__, rc);
		goto init_error2;
	}

	system_health_monitor_devp = device_create(system_health_monitor_classp,
					NULL, system_health_monitor_dev, NULL,
					"system_health_monitor");
	if (IS_ERR_OR_NULL(system_health_monitor_devp)) {
		SHM_ERR("%s: device_create() failed - rc %d\n",
			__func__, rc);
		rc = PTR_ERR(system_health_monitor_devp);
		goto init_error3;
	}
	SHM_INFO("%s: Complete\n", __func__);
	return 0;
init_error3:
	cdev_del(&system_health_monitor_cdev);
init_error2:
	class_destroy(system_health_monitor_classp);
init_error1:
	unregister_chrdev_region(MAJOR(system_health_monitor_dev), 1);
	return rc;
}

module_init(system_health_monitor_init);
MODULE_DESCRIPTION("System Health Monitor");
MODULE_LICENSE("GPL v2");
