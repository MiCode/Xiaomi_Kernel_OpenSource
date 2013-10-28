/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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
#include <soc/qcom/msm_qmi_interface.h>
#include <soc/qcom/scm.h>
#include "msm_memshare.h"
#include "heap_mem_ext_v01.h"
#define MEM_SHARE_SERVICE_SVC_ID 0x00000034
#define MEM_SHARE_SERVICE_INS_ID 1
#define MEM_SHARE_SERVICE_VERS 1

static struct qmi_handle *mem_share_svc_handle;
static void mem_share_svc_recv_msg(struct work_struct *work);
static DECLARE_DELAYED_WORK(work_recv_msg, mem_share_svc_recv_msg);
static struct workqueue_struct *mem_share_svc_workqueue;
static void *curr_conn;
struct mutex connection;
static struct mem_blocks memblock;
struct mutex mem_share;
struct mutex mem_free;
static uint32_t size;
static struct work_struct memshare_init_work;

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


static int handle_alloc_req(void *req_h, void *req)
{
	struct mem_alloc_req_msg_v01 *alloc_req;
	struct mem_alloc_resp_msg_v01 alloc_resp;
	int rc;

	alloc_req = (struct mem_alloc_req_msg_v01 *)req;
	pr_debug("%s: Received Alloc Request\n", __func__);
	pr_debug("%s: req->num_bytes = %d\n", __func__, alloc_req->num_bytes);
	alloc_resp.resp = QMI_RESULT_FAILURE_V01;
	mutex_lock(&mem_share);
	if (!size) {
		memset(&alloc_resp, 0, sizeof(struct mem_alloc_resp_msg_v01));
		rc = memshare_alloc(alloc_req->num_bytes,
					alloc_req->block_alignment,
					&memblock);
		if (rc) {
			mutex_unlock(&mem_share);
			return -ENOMEM;
		}
	}
	alloc_resp.num_bytes_valid = 1;
	alloc_resp.num_bytes =  alloc_req->num_bytes;
	size = alloc_req->num_bytes;
	alloc_resp.handle_valid = 1;
	alloc_resp.handle = memblock.phy_addr;
	alloc_resp.resp = QMI_RESULT_SUCCESS_V01;
	mutex_unlock(&mem_share);

	pr_debug("alloc_resp.num_bytes :%d, alloc_resp.handle :%lx, alloc_resp.mem_req_result :%lx\n",
			  alloc_resp.num_bytes,
			  (unsigned long int)alloc_resp.handle,
			  (unsigned long int)alloc_resp.resp);
	rc = qmi_send_resp_from_cb(mem_share_svc_handle, curr_conn, req_h,
			&mem_share_svc_alloc_resp_desc, &alloc_resp,
			sizeof(alloc_resp));
	return rc;
}
static int handle_free_req(void *req_h, void *req)
{
	struct mem_free_req_msg_v01 *free_req;
	struct mem_free_resp_msg_v01 free_resp;
	int rc;

	free_req = (struct mem_free_req_msg_v01 *)req;
	pr_debug("%s: Received Free Request\n", __func__);
	mutex_lock(&mem_free);
	free_resp.resp = QMI_RESULT_FAILURE_V01;

	memset(&free_resp, 0, sizeof(struct mem_free_resp_msg_v01));
	pr_debug("In %s: pblk->virtual_addr :%lx, pblk->phy_addr %lx\n,size: %d",
			__func__,
			(unsigned long int)memblock.virtual_addr,
			(unsigned long int)free_req->handle, size);
	dma_free_coherent(NULL, size,
		memblock.virtual_addr, free_req->handle);
	size = 0;
	mutex_unlock(&mem_free);
	free_resp.resp = QMI_RESULT_SUCCESS_V01;
	rc = qmi_send_resp_from_cb(mem_share_svc_handle, curr_conn, req_h,
			&mem_share_svc_free_resp_desc, &free_resp,
			sizeof(free_resp));

	return rc;
}

static int mem_share_svc_connect_cb(struct qmi_handle *handle,
			       void *conn_h)
{
	if (mem_share_svc_handle != handle || !conn_h)
		return -EINVAL;
	mutex_lock(&connection);
	if (curr_conn) {
		pr_err("%s: Service is busy\n", __func__);
		mutex_unlock(&connection);
		return -EBUSY;
	}
	curr_conn = conn_h;
	mutex_unlock(&connection);
	return 0;
}

static int mem_share_svc_disconnect_cb(struct qmi_handle *handle,
				  void *conn_h)
{
	mutex_lock(&connection);
	if (mem_share_svc_handle != handle || curr_conn != conn_h) {
		mutex_unlock(&connection);
		return -EINVAL;
	}
	curr_conn = NULL;
	mutex_unlock(&connection);
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
	if (mem_share_svc_handle != handle || curr_conn != conn_h)
		return -EINVAL;

	switch (msg_id) {
	case MEM_ALLOC_REQ_MSG_V01:
		rc = handle_alloc_req(req_h, req);
		break;

	case MEM_FREE_REQ_MSG_V01:
		rc = handle_free_req(req_h, req);
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

int memshare_alloc(unsigned int block_size,
					unsigned int block_algn,
					struct mem_blocks *pblk)
{

	int ret;

	pr_debug("%s: memshare_alloc called", __func__);
	if (!pblk) {
		pr_err("%s: Failed to alloc\n", __func__);
		return -ENOMEM;
	}

	pblk->virtual_addr = dma_alloc_coherent(NULL, block_size,
						&pblk->phy_addr, GFP_KERNEL);
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
	mutex_init(&connection);
	mutex_init(&mem_share);
	mutex_init(&mem_free);
	pr_info("memshare: memshare_init successful\n");
}

static int __init memshare_init(void)
{
	INIT_WORK(&memshare_init_work, memshare_init_worker);
	schedule_work(&memshare_init_work);
	return 0;
}

static void __exit memshare_exit(void)
{
	qmi_svc_unregister(mem_share_svc_handle);
	flush_workqueue(mem_share_svc_workqueue);
	qmi_handle_destroy(mem_share_svc_handle);
	destroy_workqueue(mem_share_svc_workqueue);
}

module_init(memshare_init);
module_exit(memshare_exit);

MODULE_DESCRIPTION("Mem Share QMI Service Driver");
MODULE_LICENSE("GPL v2");
