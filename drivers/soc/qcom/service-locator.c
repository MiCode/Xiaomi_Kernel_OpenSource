// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "servloc: %s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/completion.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/workqueue.h>

#include <linux/soc/qcom/qmi.h>
#include <soc/qcom/service-locator.h>
#include "service-locator-private.h"

#define SERVREG_LOC_SERVICE_INSTANCE_ID			1

#define QMI_SERVREG_LOC_SERVER_INITIAL_TIMEOUT		2000
#define QMI_SERVREG_LOC_SERVER_TIMEOUT			2000
#define INITIAL_TIMEOUT					100000
#define LOCATOR_SERVICE_TIMEOUT				1200000

#define LOCATOR_NOT_PRESENT	0
#define LOCATOR_PRESENT		1

static u32 locator_status = LOCATOR_PRESENT;
static bool service_inited;

module_param_named(enable, locator_status, uint, 0644);

static void pd_locator_work(struct work_struct *work);

struct pd_qmi_data {
	struct notifier_block notifier;
	struct completion service_available;
	struct qmi_handle clnt_handle;
	bool connected;
	struct sockaddr_qrtr s_addr;
};

struct pd_qmi_work {
	struct work_struct pd_loc_work;
	struct pd_qmi_client_data *pdc;
	struct notifier_block *notifier;
};

static DEFINE_MUTEX(service_init_mutex);
static struct pd_qmi_data service_locator;

/* Please refer soc/qcom/service-locator.h for use about APIs defined here */

static int service_locator_new_server(struct qmi_handle *qmi,
		struct qmi_service *svc)
{

	/* Create a Local client port for QMI communication */
	service_locator.s_addr.sq_family = AF_QIPCRTR;
	service_locator.s_addr.sq_node = svc->node;
	service_locator.s_addr.sq_port = svc->port;
	service_locator.connected = true;
	if (!service_inited)
		complete_all(&service_locator.service_available);
	pr_info("Connection established with the Service locator\n");
	return 0;
}

static void service_locator_del_server(struct qmi_handle *qmi,
		struct qmi_service *svc)
{
	service_locator.connected = false;
	complete_all(&service_locator.service_available);
	pr_info("Connection with service locator lost\n");
}

static struct qmi_ops server_ops = {
	.new_server = service_locator_new_server,
	.del_server = service_locator_del_server,
};

static void store_get_domain_list_response(struct pd_qmi_client_data *pd,
		struct qmi_servreg_loc_get_domain_list_resp_msg_v01 *resp,
		int offset)
{
	int i;

	for (i = offset; i < resp->domain_list_len; i++) {
		pd->domain_list[i].instance_id =
					resp->domain_list[i].instance_id;
		strlcpy(pd->domain_list[i].name, resp->domain_list[i].name,
			QMI_SERVREG_LOC_NAME_LENGTH_V01 + 1);
		pd->domain_list[i].service_data_valid =
					resp->domain_list[i].service_data_valid;
		pd->domain_list[i].service_data =
					resp->domain_list[i].service_data;
	}
}

static int servreg_loc_send_msg(
		struct qmi_servreg_loc_get_domain_list_req_msg_v01 *req,
		struct qmi_servreg_loc_get_domain_list_resp_msg_v01 *resp,
		struct pd_qmi_client_data *pd)
{
	int rc;
	struct qmi_txn txn;
	/*
	 * Send msg and get response. There is a chance that the service went
	 * away since the time we last checked for it to be available and
	 * actually made this call. In that case the call just fails.
	 */
	rc = qmi_txn_init(&service_locator.clnt_handle, &txn,
			qmi_servreg_loc_get_domain_list_resp_msg_v01_ei, resp);
	if (rc < 0) {
		pr_err("QMI tx init failed for client %s, ret - %d\n",
			pd->client_name, rc);
		return rc;
	}

	rc = qmi_send_request(&service_locator.clnt_handle,
			&service_locator.s_addr,
			&txn, QMI_SERVREG_LOC_GET_DOMAIN_LIST_REQ_V01,
			QMI_SERVREG_LOC_GET_DOMAIN_LIST_REQ_MSG_V01_MAX_MSG_LEN,
			qmi_servreg_loc_get_domain_list_req_msg_v01_ei,
			req);
	if (rc < 0) {
		pr_err("QMI send req failed for client %s, ret - %d\n",
			pd->client_name, rc);
		qmi_txn_cancel(&txn);
		return rc;
	}

	rc = qmi_txn_wait(&txn,
			msecs_to_jiffies(QMI_SERVREG_LOC_SERVER_TIMEOUT));
	if (rc < 0) {
		pr_err("QMI qmi txn wait failed for client %s, ret - %d\n",
			pd->client_name, rc);
		return rc;
	}

	/* Check the response */
	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		pr_err("QMI request for client %s failed 0x%x\n",
			pd->client_name, resp->resp.error);
		return -EREMOTEIO;
	}
	return rc;
}

static int service_locator_send_msg(struct pd_qmi_client_data *pd)
{
	struct qmi_servreg_loc_get_domain_list_resp_msg_v01 *resp = NULL;
	struct qmi_servreg_loc_get_domain_list_req_msg_v01 *req = NULL;
	int rc;
	int db_rev_count = 0, domains_read = 0;

	if (!service_locator.connected) {
		pr_err("Service locator not available!\n");
		return -EAGAIN;
	}

	req = kzalloc(sizeof(
		struct qmi_servreg_loc_get_domain_list_req_msg_v01),
		GFP_KERNEL);
	if (!req) {
		pr_err("Unable to allocate memory for req message\n");
		rc = -ENOMEM;
		goto out;
	}
	resp = kzalloc(sizeof(
		struct qmi_servreg_loc_get_domain_list_resp_msg_v01),
		GFP_KERNEL);
	if (!resp) {
		pr_err("Unable to allocate memory for resp message\n");
		rc = -ENOMEM;
		goto out;
	}

	/* Prepare req and response message */
	strlcpy(req->service_name, pd->service_name,
		QMI_SERVREG_LOC_NAME_LENGTH_V01 + 1);
	req->domain_offset_valid = true;
	req->domain_offset = 0;

	do {
		req->domain_offset += domains_read;
		rc = servreg_loc_send_msg(req, resp, pd);
		if (rc < 0) {
			pr_err("send msg failed rc:%d\n", rc);
			goto out;
		}
		if (!domains_read) {
			db_rev_count = pd->db_rev_count = resp->db_rev_count;
			pd->total_domains = resp->total_domains;
			if (!resp->total_domains) {
				pr_err("No matching domains found\n");
				goto out;
			}

			pd->domain_list = kmalloc(
					sizeof(struct servreg_loc_entry_v01) *
					resp->total_domains, GFP_KERNEL);
			if (!pd->domain_list) {
				pr_err("Cannot allocate domain list\n");
				rc = -ENOMEM;
				goto out;
			}
		}
		if (db_rev_count != resp->db_rev_count) {
			pr_err("Service Locator DB updated for client %s\n",
				pd->client_name);
			kfree(pd->domain_list);
			pd->domain_list = NULL;
			rc = -EAGAIN;
			goto out;
		}
		if (resp->domain_list_len >  resp->total_domains) {
			/* Always read total_domains from the response msg */
			resp->domain_list_len = resp->total_domains;
		}
		/* Copy the response*/
		store_get_domain_list_response(pd, resp, domains_read);
		domains_read += resp->domain_list_len;
	} while (domains_read < resp->total_domains);
	rc = 0;
out:
	kfree(req);
	kfree(resp);
	return rc;
}

static int init_service_locator(void)
{
	int rc = 0;
	static bool service_timedout;

	rc = mutex_lock_interruptible(&service_init_mutex);
	if (rc)
		return rc;
	if (locator_status == LOCATOR_NOT_PRESENT) {
		pr_err("Service Locator not enabled\n");
		rc = -ENODEV;
		goto inited;
	}
	if (service_timedout) {
		rc = -ETIME;
		goto inited;
	}
	if (service_inited)
		goto inited;

	init_completion(&service_locator.service_available);

	service_locator.connected = false;

	rc = qmi_handle_init(&service_locator.clnt_handle,
		QMI_SERVREG_LOC_GET_DOMAIN_LIST_RESP_MSG_V01_MAX_MSG_LEN,
			&server_ops, NULL);
	if (rc < 0) {
		pr_err("Service locator QMI handle init failed rc:%d\n", rc);
		goto inited;
	}

	qmi_add_lookup(&service_locator.clnt_handle,
			SERVREG_LOC_SERVICE_ID_V01,
			SERVREG_LOC_SERVICE_VERS_V01,
			SERVREG_LOC_SERVICE_INSTANCE_ID);

	rc = wait_for_completion_interruptible_timeout(
				&service_locator.service_available,
				msecs_to_jiffies(LOCATOR_SERVICE_TIMEOUT));
	if (rc < 0) {
		pr_err("Wait for locator service interrupted by signal\n");
		goto inited;
	}
	if (!rc) {
		pr_err("%s: wait for locator service timed out\n", __func__);
		service_timedout = true;
		rc = -ETIME;
		goto inited;
	}

	service_inited = true;
	mutex_unlock(&service_init_mutex);
	pr_info("Service locator initialized\n");
	return 0;

inited:
	mutex_unlock(&service_init_mutex);
	return rc;
}

int get_service_location(const char *client_name, const char *service_name,
				struct notifier_block *locator_nb)
{
	struct pd_qmi_client_data *pqcd;
	struct pd_qmi_work *pqw;
	int rc = 0;

	if (!locator_nb || !client_name || !service_name) {
		rc = -EINVAL;
		pr_err("Invalid input!\n");
		goto err;
	}

	pqcd = kzalloc(sizeof(struct pd_qmi_client_data), GFP_KERNEL);
	if (!pqcd) {
		rc = -ENOMEM;
		pr_err("Allocation failed\n");
		goto err;
	}
	strlcpy(pqcd->client_name, client_name, ARRAY_SIZE(pqcd->client_name));
	strlcpy(pqcd->service_name, service_name,
		ARRAY_SIZE(pqcd->service_name));

	pqw = kmalloc(sizeof(struct pd_qmi_work), GFP_KERNEL);
	if (!pqw) {
		rc = -ENOMEM;
		pr_err("Allocation failed\n");
		kfree(pqcd);
		goto err;
	}
	pqw->notifier = locator_nb;
	pqw->pdc = pqcd;

	INIT_WORK(&pqw->pd_loc_work, pd_locator_work);
	schedule_work(&pqw->pd_loc_work);

err:
	return rc;
}
EXPORT_SYMBOL(get_service_location);

static void pd_locator_work(struct work_struct *work)
{
	int rc = 0;
	struct pd_qmi_client_data *data;
	struct pd_qmi_work *pdqw = container_of(work, struct pd_qmi_work,
								pd_loc_work);

	data = pdqw->pdc;
	rc = init_service_locator();
	if (rc) {
		pr_err("Unable to connect to service locator!, rc = %d\n", rc);
		pdqw->notifier->notifier_call(pdqw->notifier,
			LOCATOR_DOWN, NULL);
		goto err_init_servloc;
	}
	rc = service_locator_send_msg(data);
	if (rc) {
		pr_err("Failed to get process domains for %s for client %s rc:%d\n",
			data->service_name, data->client_name, rc);
		pdqw->notifier->notifier_call(pdqw->notifier,
			LOCATOR_DOWN, NULL);
		goto err_servloc_send_msg;
	}
	pdqw->notifier->notifier_call(pdqw->notifier, LOCATOR_UP, data);

err_servloc_send_msg:
	kfree(data->domain_list);
err_init_servloc:
	kfree(data);
	kfree(pdqw);
}

int find_subsys(const char *pd_path, char *subsys)
{
	char *start, *end;

	if (!subsys || !pd_path)
		return -EINVAL;

	start = strnstr(pd_path, "/", QMI_SERVREG_LOC_NAME_LENGTH_V01);
	if (!start)
		return -EINVAL;
	start++;
	end = strnstr(start, "/", QMI_SERVREG_LOC_NAME_LENGTH_V01);
	if (!end || start == end)
		return -EINVAL;

	strlcpy(subsys, start, end - start + 1);
	return 0;
}
EXPORT_SYMBOL(find_subsys);

MODULE_SOFTDEP("pre: qrtr");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Qualcomm Technologies, Inc. Service Locator driver");
