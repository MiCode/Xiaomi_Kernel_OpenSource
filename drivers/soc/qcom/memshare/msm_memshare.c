/* Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
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
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/notifier.h>
#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/msm_qmi_interface.h>
#include <soc/qcom/scm.h>
#include "msm_memshare.h"
#include "heap_mem_ext_v01.h"

/* Macros */
#define MEMSHARE_DEV_NAME "memshare"
#define MEMSHARE_CHILD_DEV_NAME "memshare_child"
static DEFINE_DMA_ATTRS(attrs);

static struct qmi_handle *mem_share_svc_handle;
static void mem_share_svc_recv_msg(struct work_struct *work);
static DECLARE_DELAYED_WORK(work_recv_msg, mem_share_svc_recv_msg);
static struct workqueue_struct *mem_share_svc_workqueue;
static uint64_t bootup_request;

/* Memshare Driver Structure */
struct memshare_driver {
	struct device *dev;
	struct mutex mem_share;
	struct mutex mem_free;
	struct work_struct memshare_init_work;
};

struct memshare_child {
	struct device *dev;
};

static struct memshare_driver *memsh_drv;
static struct memshare_child *memsh_child;
static struct mem_blocks memblock[MAX_CLIENTS];
static uint32_t num_clients;
static struct msg_desc mem_share_svc_alloc_req_desc = {
	.max_msg_len = MEM_ALLOC_REQ_MAX_MSG_LEN_V01,
	.msg_id = MEM_ALLOC_REQ_MSG_V01,
	.ei_array = mem_alloc_req_msg_data_v01_ei,
};

static struct msg_desc mem_share_svc_alloc_resp_desc = {
	.max_msg_len = MEM_ALLOC_REQ_MAX_MSG_LEN_V01,
	.msg_id = MEM_ALLOC_RESP_MSG_V01,
	.ei_array = mem_alloc_resp_msg_data_v01_ei,
};

static struct msg_desc mem_share_svc_free_req_desc = {
	.max_msg_len = MEM_FREE_REQ_MAX_MSG_LEN_V01,
	.msg_id = MEM_FREE_REQ_MSG_V01,
	.ei_array = mem_free_req_msg_data_v01_ei,
};

static struct msg_desc mem_share_svc_free_resp_desc = {
	.max_msg_len = MEM_FREE_REQ_MAX_MSG_LEN_V01,
	.msg_id = MEM_FREE_RESP_MSG_V01,
	.ei_array = mem_free_resp_msg_data_v01_ei,
};

static struct msg_desc mem_share_svc_alloc_generic_req_desc = {
	.max_msg_len = MEM_ALLOC_REQ_MAX_MSG_LEN_V01,
	.msg_id = MEM_ALLOC_GENERIC_REQ_MSG_V01,
	.ei_array = mem_alloc_generic_req_msg_data_v01_ei,
};

static struct msg_desc mem_share_svc_alloc_generic_resp_desc = {
	.max_msg_len = MEM_ALLOC_REQ_MAX_MSG_LEN_V01,
	.msg_id = MEM_ALLOC_GENERIC_RESP_MSG_V01,
	.ei_array = mem_alloc_generic_resp_msg_data_v01_ei,
};

static struct msg_desc mem_share_svc_free_generic_req_desc = {
	.max_msg_len = MEM_FREE_REQ_MAX_MSG_LEN_V01,
	.msg_id = MEM_FREE_GENERIC_REQ_MSG_V01,
	.ei_array = mem_free_generic_req_msg_data_v01_ei,
};

static struct msg_desc mem_share_svc_free_generic_resp_desc = {
	.max_msg_len = MEM_FREE_REQ_MAX_MSG_LEN_V01,
	.msg_id = MEM_FREE_GENERIC_RESP_MSG_V01,
	.ei_array = mem_free_generic_resp_msg_data_v01_ei,
};

static struct msg_desc mem_share_svc_size_query_req_desc = {
	.max_msg_len = MEM_FREE_REQ_MAX_MSG_LEN_V01,
	.msg_id = MEM_QUERY_SIZE_REQ_MSG_V01,
	.ei_array = mem_query_size_req_msg_data_v01_ei,
};

static struct msg_desc mem_share_svc_size_query_resp_desc = {
	.max_msg_len = MEM_FREE_REQ_MAX_MSG_LEN_V01,
	.msg_id = MEM_QUERY_SIZE_RESP_MSG_V01,
	.ei_array = mem_query_size_resp_msg_data_v01_ei,
};

static int check_client(int client_id, int proc, int request)
{
	int i = 0;
	int found = DHMS_MEM_CLIENT_INVALID;

	for (i = 0; i < MAX_CLIENTS; i++) {
		if (memblock[i].client_id == client_id &&
				memblock[i].peripheral == proc) {
			found = i;
			break;
		}
	}
	if ((found == DHMS_MEM_CLIENT_INVALID) && !request) {
		pr_debug("No registered client, adding a new client\n");
		/* Add a new client */
		for (i = 0; i < MAX_CLIENTS; i++) {
			if (memblock[i].client_id == DHMS_MEM_CLIENT_INVALID) {
				memblock[i].client_id = client_id;
				memblock[i].alloted = 0;
				memblock[i].guarantee = 0;
				memblock[i].peripheral = proc;
				found = i;
				break;
			}
		}
	}

	return found;
}

void free_client(int id)
{

	memblock[id].size = 0;
	memblock[id].phy_addr = 0;
	memblock[id].virtual_addr = 0;
	memblock[id].alloted = 0;
	memblock[id].client_id = DHMS_MEM_CLIENT_INVALID;
	memblock[id].guarantee = 0;
	memblock[id].peripheral = -1;
	memblock[id].sequence_id = -1;
	memblock[id].memory_type = MEMORY_CMA;

}

void fill_alloc_response(struct mem_alloc_generic_resp_msg_v01 *resp,
						int id, int *flag)
{
	resp->sequence_id_valid = 1;
	resp->sequence_id = memblock[id].sequence_id;
	resp->dhms_mem_alloc_addr_info_valid = 1;
	resp->dhms_mem_alloc_addr_info_len = 1;
	resp->dhms_mem_alloc_addr_info[0].phy_addr = memblock[id].phy_addr;
	resp->dhms_mem_alloc_addr_info[0].num_bytes = memblock[id].size;
	if (!*flag) {
		resp->resp.result = QMI_RESULT_SUCCESS_V01;
		resp->resp.error = QMI_ERR_NONE_V01;
	} else {
		resp->resp.result = QMI_RESULT_FAILURE_V01;
		resp->resp.error = QMI_ERR_NO_MEMORY_V01;
	}

}

void initialize_client(void)
{
	int i;

	for (i = 0; i < MAX_CLIENTS; i++) {
		memblock[i].alloted = 0;
		memblock[i].size = 0;
		memblock[i].guarantee = 0;
		memblock[i].phy_addr = 0;
		memblock[i].virtual_addr = 0;
		memblock[i].client_id = DHMS_MEM_CLIENT_INVALID;
		memblock[i].peripheral = -1;
		memblock[i].sequence_id = -1;
		memblock[i].memory_type = MEMORY_CMA;
		memblock[i].free_memory = 0;
	}
	dma_set_attr(DMA_ATTR_NO_KERNEL_MAPPING, &attrs);
}

static int modem_notifier_cb(struct notifier_block *this, unsigned long code,
					void *_cmd)
{
	int i;

	switch (code) {

	case SUBSYS_BEFORE_SHUTDOWN:
		bootup_request++;
		break;

	case SUBSYS_AFTER_POWERUP:
		pr_debug("memshare: Modem has booted up\n");
		for (i = 0; i < MAX_CLIENTS; i++) {
			if (memblock[i].free_memory > 0 &&
					bootup_request >= 2) {
				memblock[i].free_memory -= 1;
				pr_debug("memshare: free_memory count: %d for clinet id: %d\n",
					memblock[i].free_memory,
					memblock[i].client_id);
			}

			if (memblock[i].free_memory == 0) {
				if (memblock[i].peripheral ==
					DHMS_MEM_PROC_MPSS_V01 &&
					!memblock[i].guarantee &&
					memblock[i].alloted) {
					pr_err("memshare: Freeing memory for client id: %d\n",
						memblock[i].client_id);
					dma_free_attrs(memsh_drv->dev,
						memblock[i].size,
						memblock[i].virtual_addr,
						memblock[i].phy_addr,
						&attrs);
					free_client(i);
				}
			}
		}
		bootup_request++;
		break;

	default:
		pr_debug("Memshare: code: %lu\n", code);
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block nb = {
	.notifier_call = modem_notifier_cb,
};

static int handle_alloc_req(void *req_h, void *req, void *conn_h)
{
	struct mem_alloc_req_msg_v01 *alloc_req;
	struct mem_alloc_resp_msg_v01 alloc_resp;
	int rc = 0;

	alloc_req = (struct mem_alloc_req_msg_v01 *)req;
	pr_debug("%s: Received Alloc Request\n", __func__);
	pr_debug("%s: req->num_bytes = %d\n", __func__, alloc_req->num_bytes);
	mutex_lock(&memsh_drv->mem_share);
	if (!memblock[GPS].size) {
		memset(&alloc_resp, 0, sizeof(struct mem_alloc_resp_msg_v01));
		alloc_resp.resp = QMI_RESULT_FAILURE_V01;
		rc = memshare_alloc(memsh_drv->dev, alloc_req->num_bytes,
					&memblock[GPS]);
	}
	alloc_resp.num_bytes_valid = 1;
	alloc_resp.num_bytes =  alloc_req->num_bytes;
	alloc_resp.handle_valid = 1;
	alloc_resp.handle = memblock[GPS].phy_addr;
	if (rc) {
		alloc_resp.resp = QMI_RESULT_FAILURE_V01;
		memblock[GPS].size = 0;
	} else {
		alloc_resp.resp = QMI_RESULT_SUCCESS_V01;
	}

	mutex_unlock(&memsh_drv->mem_share);

	pr_debug("alloc_resp.num_bytes :%d, alloc_resp.handle :%lx, alloc_resp.mem_req_result :%lx\n",
			  alloc_resp.num_bytes,
			  (unsigned long int)alloc_resp.handle,
			  (unsigned long int)alloc_resp.resp);
	rc = qmi_send_resp_from_cb(mem_share_svc_handle, conn_h, req_h,
			&mem_share_svc_alloc_resp_desc, &alloc_resp,
			sizeof(alloc_resp));
	if (rc < 0)
		pr_err("In %s, Error sending the alloc request: %d\n",
					__func__, rc);

	return rc;
}

static int handle_alloc_generic_req(void *req_h, void *req, void *conn_h)
{
	struct mem_alloc_generic_req_msg_v01 *alloc_req;
	struct mem_alloc_generic_resp_msg_v01 *alloc_resp;
	int rc, resp = 0;
	int client_id;

	alloc_req = (struct mem_alloc_generic_req_msg_v01 *)req;
	pr_debug("memshare: alloc request client id: %d proc _id: %d\n",
			alloc_req->client_id, alloc_req->proc_id);
	mutex_lock(&memsh_drv->mem_share);
	alloc_resp = kzalloc(sizeof(struct mem_alloc_generic_resp_msg_v01),
					GFP_KERNEL);
	if (!alloc_resp) {
		mutex_unlock(&memsh_drv->mem_share);
		return -ENOMEM;
	}
	alloc_resp->resp.result = QMI_RESULT_FAILURE_V01;
	alloc_resp->resp.error = QMI_ERR_NO_MEMORY_V01;
	client_id = check_client(alloc_req->client_id, alloc_req->proc_id,
								CHECK);

	if (client_id >= MAX_CLIENTS) {
		pr_err("memshare: %s client not found, requested client: %d, proc_id: %d\n",
				__func__, alloc_req->client_id,
				alloc_req->proc_id);
		return -EINVAL;
	}

	memblock[client_id].free_memory += 1;
	pr_debug("memshare: In %s, free memory count for client id: %d = %d",
		__func__, memblock[client_id].client_id,
			memblock[client_id].free_memory);
	if (!memblock[client_id].alloted) {
		rc = memshare_alloc(memsh_drv->dev, alloc_req->num_bytes,
					&memblock[client_id]);
		if (rc) {
			pr_err("In %s,Unable to allocate memory for requested client\n",
							__func__);
			resp = 1;
		}
		if (!resp) {
			memblock[client_id].alloted = 1;
			memblock[client_id].size = alloc_req->num_bytes;
			memblock[client_id].peripheral = alloc_req->proc_id;
		}
	}
	memblock[client_id].sequence_id = alloc_req->sequence_id;

	fill_alloc_response(alloc_resp, client_id, &resp);

	mutex_unlock(&memsh_drv->mem_share);
	pr_debug("alloc_resp.num_bytes :%d, alloc_resp.handle :%lx, alloc_resp.mem_req_result :%lx\n",
			  alloc_resp->dhms_mem_alloc_addr_info[0].num_bytes,
			  (unsigned long int)
			  alloc_resp->dhms_mem_alloc_addr_info[0].phy_addr,
			  (unsigned long int)alloc_resp->resp.result);
	rc = qmi_send_resp_from_cb(mem_share_svc_handle, conn_h, req_h,
			&mem_share_svc_alloc_generic_resp_desc, alloc_resp,
			sizeof(alloc_resp));

	if (rc < 0)
		pr_err("In %s, Error sending the alloc request: %d\n",
							__func__, rc);

	return rc;
}

static int handle_free_req(void *req_h, void *req, void *conn_h)
{
	struct mem_free_req_msg_v01 *free_req;
	struct mem_free_resp_msg_v01 free_resp;
	int rc;

	mutex_lock(&memsh_drv->mem_free);
	if (!memblock[GPS].guarantee) {
		free_req = (struct mem_free_req_msg_v01 *)req;
		pr_debug("%s: Received Free Request\n", __func__);
		memset(&free_resp, 0, sizeof(struct mem_free_resp_msg_v01));
		pr_debug("In %s: pblk->virtual_addr :%lx, pblk->phy_addr: %lx\n,size: %d",
				__func__,
			(unsigned long int)memblock[GPS].virtual_addr,
			(unsigned long int)free_req->handle,
			memblock[GPS].size);
		dma_free_coherent(memsh_drv->dev, memblock[GPS].size,
			memblock[GPS].virtual_addr,
				free_req->handle);
	}
	free_resp.resp = QMI_RESULT_SUCCESS_V01;
	mutex_unlock(&memsh_drv->mem_free);
	rc = qmi_send_resp_from_cb(mem_share_svc_handle, conn_h, req_h,
			&mem_share_svc_free_resp_desc, &free_resp,
			sizeof(free_resp));
	if (rc < 0)
		pr_err("In %s, Error sending the free request: %d\n",
					__func__, rc);

	return rc;
}

static int handle_free_generic_req(void *req_h, void *req, void *conn_h)
{
	struct mem_free_generic_req_msg_v01 *free_req;
	struct mem_free_generic_resp_msg_v01 free_resp;
	int rc;
	int flag = 0;
	uint32_t client_id;

	free_req = (struct mem_free_generic_req_msg_v01 *)req;
	pr_debug("memshare: %s: Received Free Request\n", __func__);
	mutex_lock(&memsh_drv->mem_free);
	memset(&free_resp, 0, sizeof(struct mem_free_generic_resp_msg_v01));
	free_resp.resp.error = QMI_ERR_INTERNAL_V01;
	free_resp.resp.result = QMI_RESULT_FAILURE_V01;
	pr_debug("Client id: %d proc id: %d\n", free_req->client_id,
				free_req->proc_id);
	client_id = check_client(free_req->client_id, free_req->proc_id, FREE);
	if (client_id == DHMS_MEM_CLIENT_INVALID) {
		pr_err("In %s, Invalid client request to free memory\n",
					__func__);
		flag = 1;
	} else if (!memblock[client_id].guarantee &&
					memblock[client_id].alloted) {
		pr_debug("In %s: pblk->virtual_addr :%lx, pblk->phy_addr: %lx\n,size: %d",
				__func__,
				(unsigned long int)
				memblock[client_id].virtual_addr,
				(unsigned long int)memblock[client_id].phy_addr,
				memblock[client_id].size);
		dma_free_attrs(memsh_drv->dev, memblock[client_id].size,
			memblock[client_id].virtual_addr,
			memblock[client_id].phy_addr,
			&attrs);
		free_client(client_id);
	} else {
		pr_err("In %s, Request came for a guaranteed client cannot free up the memory\n",
						__func__);
	}

	if (flag) {
		free_resp.resp.result = QMI_RESULT_FAILURE_V01;
		free_resp.resp.error = QMI_ERR_INVALID_ID_V01;
	} else {
		free_resp.resp.result = QMI_RESULT_SUCCESS_V01;
		free_resp.resp.error = QMI_ERR_NONE_V01;
	}

	mutex_unlock(&memsh_drv->mem_free);
	rc = qmi_send_resp_from_cb(mem_share_svc_handle, conn_h, req_h,
		&mem_share_svc_free_generic_resp_desc, &free_resp,
		sizeof(free_resp));

	if (rc < 0)
		pr_err("In %s, Error sending the free request: %d\n",
					__func__, rc);

	return rc;
}

static int handle_query_size_req(void *req_h, void *req, void *conn_h)
{
	int rc, client_id;
	struct mem_query_size_req_msg_v01 *query_req;
	struct mem_query_size_rsp_msg_v01 *query_resp;

	query_req = (struct mem_query_size_req_msg_v01 *)req;
	mutex_lock(&memsh_drv->mem_share);
	query_resp = kzalloc(sizeof(struct mem_query_size_rsp_msg_v01),
					GFP_KERNEL);
	if (!query_resp) {
		mutex_unlock(&memsh_drv->mem_share);
		return -ENOMEM;
	}
	pr_debug("memshare: query request client id: %d proc _id: %d\n",
		query_req->client_id, query_req->proc_id);
	client_id = check_client(query_req->client_id, query_req->proc_id,
								CHECK);

	if (client_id >= MAX_CLIENTS) {
		pr_err("memshare: %s client not found, requested client: %d, proc_id: %d\n",
				__func__, query_req->client_id,
				query_req->proc_id);
		return -EINVAL;
	}

	if (memblock[client_id].size) {
		query_resp->size_valid = 1;
		query_resp->size = memblock[client_id].size;
	} else {
		query_resp->size_valid = 1;
		query_resp->size = 0;
	}
	query_resp->resp.result = QMI_RESULT_SUCCESS_V01;
	query_resp->resp.error = QMI_ERR_NONE_V01;
	mutex_unlock(&memsh_drv->mem_share);

	pr_debug("memshare: query_resp.size :%d, alloc_resp.mem_req_result :%lx\n",
			  query_resp->size,
			  (unsigned long int)query_resp->resp.result);
	rc = qmi_send_resp_from_cb(mem_share_svc_handle, conn_h, req_h,
			&mem_share_svc_size_query_resp_desc, query_resp,
			sizeof(query_resp));

	if (rc < 0)
		pr_err("In %s, Error sending the query request: %d\n",
							__func__, rc);

	return rc;
}

static int mem_share_svc_connect_cb(struct qmi_handle *handle,
			       void *conn_h)
{
	if (mem_share_svc_handle != handle || !conn_h)
		return -EINVAL;

	return 0;
}

static int mem_share_svc_disconnect_cb(struct qmi_handle *handle,
				  void *conn_h)
{
	if (mem_share_svc_handle != handle || !conn_h)
		return -EINVAL;

	return 0;
}

static int mem_share_svc_req_desc_cb(unsigned int msg_id,
				struct msg_desc **req_desc)
{
	int rc;

	pr_debug("memshare: In %s\n", __func__);
	switch (msg_id) {
	case MEM_ALLOC_REQ_MSG_V01:
		*req_desc = &mem_share_svc_alloc_req_desc;
		rc = sizeof(struct mem_alloc_req_msg_v01);
		break;

	case MEM_FREE_REQ_MSG_V01:
		*req_desc = &mem_share_svc_free_req_desc;
		rc = sizeof(struct mem_free_req_msg_v01);
		break;

	case MEM_ALLOC_GENERIC_REQ_MSG_V01:
		*req_desc = &mem_share_svc_alloc_generic_req_desc;
		rc = sizeof(struct mem_alloc_generic_req_msg_v01);
		break;

	case MEM_FREE_GENERIC_REQ_MSG_V01:
		*req_desc = &mem_share_svc_free_generic_req_desc;
		rc = sizeof(struct mem_free_generic_req_msg_v01);
		break;

	case MEM_QUERY_SIZE_REQ_MSG_V01:
		*req_desc = &mem_share_svc_size_query_req_desc;
		rc = sizeof(struct mem_query_size_req_msg_v01);
		break;

	default:
		rc = -ENOTSUPP;
		break;
	}
	return rc;
}

static int mem_share_svc_req_cb(struct qmi_handle *handle, void *conn_h,
			void *req_h, unsigned int msg_id, void *req)
{
	int rc;

	pr_debug("memshare: In %s\n", __func__);
	if (mem_share_svc_handle != handle || !conn_h)
		return -EINVAL;

	switch (msg_id) {
	case MEM_ALLOC_REQ_MSG_V01:
		rc = handle_alloc_req(req_h, req, conn_h);
		break;

	case MEM_FREE_REQ_MSG_V01:
		rc = handle_free_req(req_h, req, conn_h);
		break;

	case MEM_ALLOC_GENERIC_REQ_MSG_V01:
		rc = handle_alloc_generic_req(req_h, req, conn_h);
		break;

	case MEM_FREE_GENERIC_REQ_MSG_V01:
		rc = handle_free_generic_req(req_h, req, conn_h);
		break;

	case MEM_QUERY_SIZE_REQ_MSG_V01:
		rc = handle_query_size_req(req_h, req, conn_h);
		break;

	default:
		rc = -ENOTSUPP;
		break;
	}
	return rc;
}

static void mem_share_svc_recv_msg(struct work_struct *work)
{
	int rc;

	pr_debug("memshare: In %s\n", __func__);
	do {
		pr_debug("%s: Notified about a Receive Event", __func__);
	} while ((rc = qmi_recv_msg(mem_share_svc_handle)) == 0);

	if (rc != -ENOMSG)
		pr_err("%s: Error receiving message\n", __func__);
}

static void qmi_mem_share_svc_ntfy(struct qmi_handle *handle,
		enum qmi_event_type event, void *priv)
{
	pr_debug("memshare: In %s\n", __func__);
	switch (event) {
	case QMI_RECV_MSG:
		queue_delayed_work(mem_share_svc_workqueue,
				   &work_recv_msg, 0);
		break;
	default:
		break;
	}
}

static struct qmi_svc_ops_options mem_share_svc_ops_options = {
	.version = 1,
	.service_id = MEM_SHARE_SERVICE_SVC_ID,
	.service_vers = MEM_SHARE_SERVICE_VERS,
	.service_ins = MEM_SHARE_SERVICE_INS_ID,
	.connect_cb = mem_share_svc_connect_cb,
	.disconnect_cb = mem_share_svc_disconnect_cb,
	.req_desc_cb = mem_share_svc_req_desc_cb,
	.req_cb = mem_share_svc_req_cb,
};

int memshare_alloc(struct device *dev,
					unsigned int block_size,
					struct mem_blocks *pblk)
{

	int ret;

	pr_debug("%s: memshare_alloc called", __func__);
	if (!pblk) {
		pr_err("%s: Failed to alloc\n", __func__);
		return -ENOMEM;
	}

	pblk->virtual_addr = dma_alloc_attrs(dev, block_size,
						&pblk->phy_addr, GFP_KERNEL,
						&attrs);
	if (pblk->virtual_addr == NULL) {
		pr_err("allocation failed, %d\n", block_size);
		ret = -ENOMEM;
		return ret;
	}
	pr_debug("pblk->phy_addr :%lx, pblk->virtual_addr %lx\n",
		  (unsigned long int)pblk->phy_addr,
		  (unsigned long int)pblk->virtual_addr);
	return 0;
}

static void memshare_init_worker(struct work_struct *work)
{
	int rc;

	mem_share_svc_workqueue =
		create_singlethread_workqueue("mem_share_svc");
	if (!mem_share_svc_workqueue)
		return;

	mem_share_svc_handle = qmi_handle_create(qmi_mem_share_svc_ntfy, NULL);
	if (!mem_share_svc_handle) {
		pr_err("%s: Creating mem_share_svc qmi handle failed\n",
			__func__);
		destroy_workqueue(mem_share_svc_workqueue);
		return;
	}
	rc = qmi_svc_register(mem_share_svc_handle, &mem_share_svc_ops_options);
	if (rc < 0) {
		pr_err("%s: Registering mem share svc failed %d\n",
			__func__, rc);
		qmi_handle_destroy(mem_share_svc_handle);
		destroy_workqueue(mem_share_svc_workqueue);
		return;
	}
	pr_debug("memshare: memshare_init successful\n");
}

static int memshare_child_probe(struct platform_device *pdev)
{
	int rc;
	uint32_t size, client_id;
	const char *name;
	struct memshare_child *drv;

	drv = devm_kzalloc(&pdev->dev, sizeof(struct memshare_child),
							GFP_KERNEL);

	if (!drv) {
		pr_err("Unable to allocate memory to driver\n");
		return -ENOMEM;
	}

	drv->dev = &pdev->dev;
	memsh_child = drv;
	platform_set_drvdata(pdev, memsh_child);

	rc = of_property_read_u32(pdev->dev.of_node, "qcom,peripheral-size",
						&size);
	if (rc) {
		pr_err("In %s, Error reading size of clients, rc: %d\n",
				__func__, rc);
		return rc;
	}

	rc = of_property_read_u32(pdev->dev.of_node, "qcom,client-id",
						&client_id);
	if (rc) {
		pr_err("In %s, Error reading client id, rc: %d\n",
				__func__, rc);
		return rc;
	}

	memblock[num_clients].guarantee = of_property_read_bool(
							pdev->dev.of_node,
							"qcom,allocate-boot-time");

	rc = of_property_read_string(pdev->dev.of_node, "label",
						&name);
	if (rc) {
		pr_err("In %s, Error reading peripheral info for client, rc: %d\n",
					__func__, rc);
		return rc;
	}

	if (strcmp(name, "modem") == 0)
		memblock[num_clients].peripheral = DHMS_MEM_PROC_MPSS_V01;
	else if (strcmp(name, "adsp") == 0)
		memblock[num_clients].peripheral = DHMS_MEM_PROC_ADSP_V01;
	else if (strcmp(name, "wcnss") == 0)
		memblock[num_clients].peripheral = DHMS_MEM_PROC_WCNSS_V01;

	memblock[num_clients].size = size;
	memblock[num_clients].client_id = client_id;

	if (memblock[num_clients].guarantee) {
		rc = memshare_alloc(memsh_child->dev,
				memblock[num_clients].size,
				&memblock[num_clients]);
		if (rc) {
			pr_err("In %s, Unable to allocate memory for guaranteed clients, rc: %d\n",
							__func__, rc);
			return rc;
		}
		memblock[num_clients].alloted = 1;
	}

	num_clients++;

	return 0;
}

static int memshare_probe(struct platform_device *pdev)
{
	int rc;
	struct memshare_driver *drv;

	drv = devm_kzalloc(&pdev->dev, sizeof(struct memshare_driver),
							GFP_KERNEL);

	if (!drv) {
		pr_err("Unable to allocate memory to driver\n");
		return -ENOMEM;
	}

	/* Memory allocation has been done successfully */
	mutex_init(&drv->mem_free);
	mutex_init(&drv->mem_share);

	INIT_WORK(&drv->memshare_init_work, memshare_init_worker);
	schedule_work(&drv->memshare_init_work);

	drv->dev = &pdev->dev;
	memsh_drv = drv;
	platform_set_drvdata(pdev, memsh_drv);
	initialize_client();
	num_clients = 0;

	rc = of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);

	if (rc) {
		pr_err("In %s, error populating the devices\n", __func__);
		return rc;
	}

	subsys_notif_register_notifier("modem", &nb);
	pr_info("In %s, Memshare probe success\n", __func__);

	return 0;
}

static int memshare_remove(struct platform_device *pdev)
{
	if (!memsh_drv)
		return 0;

	qmi_svc_unregister(mem_share_svc_handle);
	flush_workqueue(mem_share_svc_workqueue);
	qmi_handle_destroy(mem_share_svc_handle);
	destroy_workqueue(mem_share_svc_workqueue);

	return 0;
}

static int memshare_child_remove(struct platform_device *pdev)
{
	if (!memsh_child)
		return 0;

	return 0;
}

static struct of_device_id memshare_match_table[] = {
	{
		.compatible = "qcom,memshare",
	},
	{}
};

static struct of_device_id memshare_match_table1[] = {
	{
		.compatible = "qcom,memshare-peripheral",
	},
	{}
};


static struct platform_driver memshare_pdriver = {
	.probe          = memshare_probe,
	.remove         = memshare_remove,
	.driver = {
		.name   = MEMSHARE_DEV_NAME,
		.owner  = THIS_MODULE,
		.of_match_table = memshare_match_table,
	},
};

static struct platform_driver memshare_pchild = {
	.probe          = memshare_child_probe,
	.remove         = memshare_child_remove,
	.driver = {
		.name   = MEMSHARE_CHILD_DEV_NAME,
		.owner  = THIS_MODULE,
		.of_match_table = memshare_match_table1,
	},
};

module_platform_driver(memshare_pdriver);
module_platform_driver(memshare_pchild);

MODULE_DESCRIPTION("Mem Share QMI Service Driver");
MODULE_LICENSE("GPL v2");
