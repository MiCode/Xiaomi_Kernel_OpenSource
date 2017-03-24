/*
 * Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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
#include <linux/debugfs.h>

#include <soc/qcom/msm_qmi_interface.h>
#include <soc/qcom/service-locator.h>
#include "service-locator-private.h"

#define SERVREG_LOC_SERVICE_INSTANCE_ID			1

#define QMI_RESP_BIT_SHIFT(x)				(x << 16)
#define QMI_SERVREG_LOC_SERVER_INITIAL_TIMEOUT		2000
#define QMI_SERVREG_LOC_SERVER_TIMEOUT			2000
#define INITIAL_TIMEOUT					100000

#define LOCATOR_NOT_PRESENT	0
#define LOCATOR_PRESENT		1

static u32 locator_status = LOCATOR_NOT_PRESENT;
static bool service_inited;

module_param_named(enable, locator_status, uint, S_IRUGO | S_IWUSR);

static void service_locator_svc_arrive(struct work_struct *work);
static void service_locator_svc_exit(struct work_struct *work);
static void service_locator_recv_msg(struct work_struct *work);
static void pd_locator_work(struct work_struct *work);

struct workqueue_struct *servloc_wq;

struct pd_qmi_data {
	struct work_struct svc_arrive;
	struct work_struct svc_exit;
	struct work_struct svc_rcv_msg;
	struct notifier_block notifier;
	struct completion service_available;
	struct mutex service_mutex;
	struct qmi_handle *clnt_handle;
};

struct pd_qmi_work {
	struct work_struct pd_loc_work;
	struct pd_qmi_client_data *pdc;
	struct notifier_block *notifier;
};
DEFINE_MUTEX(service_init_mutex);
struct pd_qmi_data service_locator;

/* Please refer soc/qcom/service-locator.h for use about APIs defined here */

static int service_locator_svc_event_notify(struct notifier_block *this,
				      unsigned long code,
				      void *_cmd)
{
	switch (code) {
	case QMI_SERVER_ARRIVE:
		queue_work(servloc_wq, &service_locator.svc_arrive);
		break;
	case QMI_SERVER_EXIT:
		queue_work(servloc_wq, &service_locator.svc_exit);
		break;
	default:
		break;
	}
	return 0;
}

static void service_locator_clnt_notify(struct qmi_handle *handle,
			     enum qmi_event_type event, void *notify_priv)
{
	switch (event) {
	case QMI_RECV_MSG:
		schedule_work(&service_locator.svc_rcv_msg);
		break;
	default:
		break;
	}
}

static void service_locator_svc_arrive(struct work_struct *work)
{
	int rc = 0;

	/* Create a Local client port for QMI communication */
	mutex_lock(&service_locator.service_mutex);
	service_locator.clnt_handle =
			qmi_handle_create(service_locator_clnt_notify, NULL);
	if (!service_locator.clnt_handle) {
		service_locator.clnt_handle = NULL;
		complete_all(&service_locator.service_available);
		mutex_unlock(&service_locator.service_mutex);
		pr_err("Service locator QMI client handle alloc failed!\n");
		return;
	}

	/* Connect to service */
	rc = qmi_connect_to_service(service_locator.clnt_handle,
		SERVREG_LOC_SERVICE_ID_V01, SERVREG_LOC_SERVICE_VERS_V01,
		SERVREG_LOC_SERVICE_INSTANCE_ID);
	if (rc) {
		qmi_handle_destroy(service_locator.clnt_handle);
		service_locator.clnt_handle = NULL;
		complete_all(&service_locator.service_available);
		mutex_unlock(&service_locator.service_mutex);
		pr_err("Unable to connnect to service rc:%d\n", rc);
		return;
	}
	if (!service_inited)
		complete_all(&service_locator.service_available);
	mutex_unlock(&service_locator.service_mutex);
	pr_info("Connection established with the Service locator\n");
}

static void service_locator_svc_exit(struct work_struct *work)
{
	mutex_lock(&service_locator.service_mutex);
	qmi_handle_destroy(service_locator.clnt_handle);
	service_locator.clnt_handle = NULL;
	complete_all(&service_locator.service_available);
	mutex_unlock(&service_locator.service_mutex);
	pr_info("Connection with service locator lost\n");
}

static void service_locator_recv_msg(struct work_struct *work)
{
	int ret;

	do {
		pr_debug("Notified about a Receive event\n");
		ret = qmi_recv_msg(service_locator.clnt_handle);
		if (ret < 0)
			pr_err("Error receiving message rc:%d. Retrying...\n",
								ret);
	} while (ret == 0);

}

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

static int servreg_loc_send_msg(struct msg_desc *req_desc,
		struct msg_desc *resp_desc,
		struct qmi_servreg_loc_get_domain_list_req_msg_v01 *req,
		struct qmi_servreg_loc_get_domain_list_resp_msg_v01 *resp,
		struct pd_qmi_client_data *pd)
{
	int rc;

	/*
	 * Send msg and get response. There is a chance that the service went
	 * away since the time we last checked for it to be available and
	 * actually made this call. In that case the call just fails.
	 */
	rc = qmi_send_req_wait(service_locator.clnt_handle, req_desc, req,
		sizeof(*req), resp_desc, resp, sizeof(*resp),
		msecs_to_jiffies(QMI_SERVREG_LOC_SERVER_TIMEOUT));
	if (rc < 0) {
		pr_err("QMI send req failed for client %s, ret - %d\n",
			pd->client_name, rc);
		return rc;
	}

	/* Check the response */
	if (QMI_RESP_BIT_SHIFT(resp->resp.result) != QMI_RESULT_SUCCESS_V01) {
		pr_err("QMI request for client %s failed 0x%x\n",
			pd->client_name, QMI_RESP_BIT_SHIFT(resp->resp.error));
		return -EREMOTEIO;
	}
	return rc;
}

static int service_locator_send_msg(struct pd_qmi_client_data *pd)
{
	struct msg_desc req_desc, resp_desc;
	struct qmi_servreg_loc_get_domain_list_resp_msg_v01 *resp = NULL;
	struct qmi_servreg_loc_get_domain_list_req_msg_v01 *req = NULL;
	int rc;
	int db_rev_count = 0, domains_read = 0;

	if (!service_locator.clnt_handle) {
		pr_err("Service locator not available!\n");
		return -EAGAIN;
	}

	req = kmalloc(sizeof(
		struct qmi_servreg_loc_get_domain_list_req_msg_v01),
		GFP_KERNEL);
	if (!req) {
		pr_err("Unable to allocate memory for req message\n");
		rc = -ENOMEM;
		goto out;
	}
	resp = kmalloc(sizeof(
		struct qmi_servreg_loc_get_domain_list_resp_msg_v01),
		GFP_KERNEL);
	if (!resp) {
		pr_err("Unable to allocate memory for resp message\n");
		rc = -ENOMEM;
		goto out;
	}
	/* Prepare req and response message formats */
	req_desc.msg_id = QMI_SERVREG_LOC_GET_DOMAIN_LIST_REQ_V01;
	req_desc.max_msg_len =
		QMI_SERVREG_LOC_GET_DOMAIN_LIST_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.ei_array = qmi_servreg_loc_get_domain_list_req_msg_v01_ei;

	resp_desc.msg_id = QMI_SERVREG_LOC_GET_DOMAIN_LIST_RESP_V01;
	resp_desc.max_msg_len =
		QMI_SERVREG_LOC_GET_DOMAIN_LIST_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.ei_array = qmi_servreg_loc_get_domain_list_resp_msg_v01_ei;

	/* Prepare req and response message */
	strlcpy(req->service_name, pd->service_name,
		QMI_SERVREG_LOC_NAME_LENGTH_V01 + 1);
	req->domain_offset_valid = true;
	req->domain_offset = 0;

	pd->domain_list = NULL;
	do {
		req->domain_offset += domains_read;
		rc = servreg_loc_send_msg(&req_desc, &resp_desc, req, resp,
					pd);
		if (rc < 0) {
			pr_err("send msg failed rc:%d\n", rc);
			goto out;
		}
		if (!domains_read) {
			db_rev_count = pd->db_rev_count = resp->db_rev_count;
			pd->total_domains = resp->total_domains;
			if (!resp->total_domains)
				pr_info("No matching domains found\n");

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

	mutex_lock(&service_init_mutex);
	if (locator_status == LOCATOR_NOT_PRESENT) {
		pr_err("Service Locator not enabled\n");
		rc = -ENODEV;
		goto inited;
	}
	if (service_inited)
		goto inited;

	service_locator.notifier.notifier_call =
					service_locator_svc_event_notify;
	init_completion(&service_locator.service_available);
	mutex_init(&service_locator.service_mutex);

	servloc_wq = create_singlethread_workqueue("servloc_wq");
	if (!servloc_wq) {
		rc = -ENOMEM;
		pr_err("Could not create workqueue\n");
		goto inited;
	}

	INIT_WORK(&service_locator.svc_arrive, service_locator_svc_arrive);
	INIT_WORK(&service_locator.svc_exit, service_locator_svc_exit);
	INIT_WORK(&service_locator.svc_rcv_msg, service_locator_recv_msg);

	rc = qmi_svc_event_notifier_register(SERVREG_LOC_SERVICE_ID_V01,
		SERVREG_LOC_SERVICE_VERS_V01, SERVREG_LOC_SERVICE_INSTANCE_ID,
		&service_locator.notifier);
	if (rc < 0) {
		pr_err("Notifier register failed rc:%d\n", rc);
		goto inited;
	}

	wait_for_completion(&service_locator.service_available);
	service_inited = true;
	mutex_unlock(&service_init_mutex);
	pr_info("Service locator initialized\n");
	return 0;

inited:
	mutex_unlock(&service_init_mutex);
	return rc;
}

int get_service_location(char *client_name, char *service_name,
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

	pqcd = kmalloc(sizeof(struct pd_qmi_client_data), GFP_KERNEL);
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
		goto err;
	}
	rc = service_locator_send_msg(data);
	if (rc) {
		pr_err("Failed to get process domains for %s for client %s rc:%d\n",
			data->service_name, data->client_name, rc);
		pdqw->notifier->notifier_call(pdqw->notifier,
			LOCATOR_DOWN, NULL);
		goto err;
	}
	pdqw->notifier->notifier_call(pdqw->notifier, LOCATOR_UP, data);

err:
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

static struct pd_qmi_client_data test_data;

static int servloc_test_pdr_cb(struct notifier_block *this,
			unsigned long opcode, void *ptr)
{
	int i, rc = 0;
	char subsys[QMI_SERVREG_LOC_NAME_LENGTH_V01];
	struct pd_qmi_client_data *return_data;

	return_data = (struct pd_qmi_client_data *)ptr;

	if (opcode) {
		pr_err("%s: Failed to get process domain!, opcode = %lu\n",
			__func__, opcode);
		return -EIO;
	}

		pr_err("Service Name: %s\tTotal Domains: %d\n",
			return_data->service_name, return_data->total_domains);

		for (i = 0; i < return_data->total_domains; i++) {
			pr_err("Instance ID: %d\t ",
				return_data->domain_list[i].instance_id);
			pr_err("Domain Name: %s\n",
				return_data->domain_list[i].name);
			rc = find_subsys(return_data->domain_list[i].name,
					subsys);
		if (rc < 0)
			pr_err("No valid subsys found for %s!\n",
				return_data->domain_list[i].name);
		else
			pr_err("Subsys: %s\n", subsys);
		}
	return 0;
}

static struct notifier_block pdr_service_nb = {
		.notifier_call  = servloc_test_pdr_cb,
};

static ssize_t servloc_read(struct file *filp, char __user *ubuf,
		size_t cnt, loff_t *ppos)
{
	int rc = 0;
	char *node_name = filp->private_data;

	if (!strcmp(node_name, "test_servloc_get"))
		rc = get_service_location(test_data.client_name,
				test_data.service_name, &pdr_service_nb);

	return rc;
}

static ssize_t servloc_write(struct file *fp, const char __user *buf,
				size_t count, loff_t *unused)
{
	char *node_name = fp->private_data;

	if (!buf)
		return -EIO;
	if (!strcmp(node_name, "service_name")) {
		snprintf(test_data.service_name, sizeof(test_data.service_name),
			"%.*s", (int) min((size_t)count - 1,
			(sizeof(test_data.service_name) - 1)), buf);
	} else {
		snprintf(test_data.client_name, sizeof(test_data.client_name),
			"%.*s", (int) min((size_t)count - 1,
			(sizeof(test_data.client_name) - 1)), buf);
	}
	return count;
}

static const struct file_operations servloc_fops = {
	.open	= simple_open,
	.read	= servloc_read,
	.write	= servloc_write,
};

static struct dentry *servloc_base_dir;
static struct dentry *test_servloc_file;

static int __init servloc_debugfs_init(void)
{
	servloc_base_dir = debugfs_create_dir("test_servloc", NULL);
	return !servloc_base_dir ? -ENOMEM : 0;
}

static void servloc_debugfs_exit(void)
{
	debugfs_remove_recursive(servloc_base_dir);
}

static int servloc_debugfs_add(void)
{
	int rc;

	if (!servloc_base_dir)
		return -ENOMEM;

	test_servloc_file = debugfs_create_file("client_name",
				S_IRUGO | S_IWUSR, servloc_base_dir,
				"client_name", &servloc_fops);
	rc = !test_servloc_file ? -ENOMEM : 0;

	if (rc == 0) {
		test_servloc_file = debugfs_create_file("service_name",
				S_IRUGO | S_IWUSR, servloc_base_dir,
				"service_name", &servloc_fops);
		rc = !test_servloc_file ? -ENOMEM : 0;
	}

	if (rc == 0) {
		test_servloc_file = debugfs_create_file("test_servloc_get",
				S_IRUGO | S_IWUSR, servloc_base_dir,
				"test_servloc_get", &servloc_fops);
		rc = !test_servloc_file ? -ENOMEM : 0;
	}
	return rc;
}

static int __init service_locator_init(void)
{
	pr_debug("service_locator_status = %d\n", locator_status);
	if (servloc_debugfs_init())
		pr_err("Could not create test_servloc base directory!");
	if (servloc_debugfs_add())
		pr_err("Could not create test_servloc node entries!");
	return 0;
}

static void __exit service_locator_exit(void)
{
	servloc_debugfs_exit();
}
module_init(service_locator_init);
module_exit(service_locator_exit);
