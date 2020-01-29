// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2013-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/err.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/notifier.h>
#include <linux/soc/qcom/qmi.h>
#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/scm.h>
#include "msm_memshare.h"
#include "heap_mem_ext_v01.h"

#include <soc/qcom/secure_buffer.h>
#include <soc/qcom/ramdump.h>

/* Macros */
#define MEMSHARE_DEV_NAME "memshare"
#define MEMSHARE_CHILD_DEV_NAME "memshare_child"
static unsigned long(attrs);

static struct qmi_handle *mem_share_svc_handle;
static struct workqueue_struct *mem_share_svc_workqueue;
static uint64_t bootup_request;
static bool ramdump_event;
static void *memshare_ramdump_dev[MAX_CLIENTS];
static struct device *memshare_dev[MAX_CLIENTS];

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

/*
 *  This API creates ramdump dev handlers
 *  for each of the memshare clients.
 *  These dev handlers will be used for
 *  extracting the ramdump for loaned memory
 *  segments.
 */

static int mem_share_configure_ramdump(int client)
{
	char client_name[18];
	const char *clnt = NULL;

	switch (client) {
	case 0:
		clnt = "GPS";
		break;
	case 1:
		clnt = "FTM";
		break;
	case 2:
		clnt = "DIAG";
		break;
	default:
		dev_err(memsh_child->dev, "memshare: no memshare clients registered\n");
		return -EINVAL;
	}

	snprintf(client_name, sizeof(client_name),
		"memshare_%s", clnt);
	if (memshare_dev[client]) {
		memshare_ramdump_dev[client] =
			create_ramdump_device(client_name,
				memshare_dev[client]);
	} else {
		dev_err(memsh_child->dev,
			"memshare: invalid memshare device for creating ramdump device\n");
		return -ENODEV;
	}
	if (IS_ERR_OR_NULL(memshare_ramdump_dev[client])) {
		dev_err(memsh_child->dev,
			"memshare: unable to create memshare ramdump device\n");
		memshare_ramdump_dev[client] = NULL;
		return -ENOMEM;
	}

	return 0;
}

static int check_client(int client_id, int proc, int request)
{
	int i = 0, rc;
	int found = DHMS_MEM_CLIENT_INVALID;

	for (i = 0; i < MAX_CLIENTS; i++) {
		if (memblock[i].client_id == client_id &&
				memblock[i].peripheral == proc) {
			found = i;
			break;
		}
	}
	if ((found == DHMS_MEM_CLIENT_INVALID) && !request) {
		dev_dbg(memsh_child->dev,
			"memshare: No registered client for the client_id: %d, adding a new client\n",
			client_id);
		/* Add a new client */
		for (i = 0; i < MAX_CLIENTS; i++) {
			if (memblock[i].client_id == DHMS_MEM_CLIENT_INVALID) {
				memblock[i].client_id = client_id;
				memblock[i].allotted = 0;
				memblock[i].guarantee = 0;
				memblock[i].peripheral = proc;
				found = i;

				if (!memblock[i].file_created) {
					rc = mem_share_configure_ramdump(i);
					if (rc)
						dev_err(memsh_child->dev,
							"memshare_check_client: cannot create ramdump for client with id: %d\n",
							client_id);
					else
						memblock[i].file_created = 1;
				}

				break;
			}
		}
	}

	return found;
}

static void free_client(int id)
{
	memblock[id].phy_addr = 0;
	memblock[id].virtual_addr = 0;
	memblock[id].allotted = 0;
	memblock[id].guarantee = 0;
	memblock[id].sequence_id = -1;
	memblock[id].memory_type = MEMORY_CMA;

}

static void fill_alloc_response(struct mem_alloc_generic_resp_msg_v01 *resp,
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

static void initialize_client(void)
{
	int i;

	for (i = 0; i < MAX_CLIENTS; i++) {
		memblock[i].allotted = 0;
		memblock[i].size = 0;
		memblock[i].guarantee = 0;
		memblock[i].phy_addr = 0;
		memblock[i].virtual_addr = 0;
		memblock[i].client_id = DHMS_MEM_CLIENT_INVALID;
		memblock[i].peripheral = -1;
		memblock[i].sequence_id = -1;
		memblock[i].memory_type = MEMORY_CMA;
		memblock[i].free_memory = 0;
		memblock[i].hyp_mapping = 0;
		memblock[i].file_created = 0;
	}
	attrs |= DMA_ATTR_NO_KERNEL_MAPPING;
}

/*
 *  mem_share_do_ramdump() function initializes the
 *  ramdump segments with the physical address and
 *  size of the memshared clients. Extraction of ramdump
 *  is skipped if memshare client is not allotted
 *  This calls the ramdump api in extracting the
 *  ramdump in elf format.
 */

static int mem_share_do_ramdump(void)
{
	int i = 0, ret;
	char *client_name = NULL;
	u32 source_vmlist[1] = {VMID_MSS_MSA};
	int dest_vmids[1] = {VMID_HLOS};
	int dest_perms[1] = {PERM_READ|PERM_WRITE|PERM_EXEC};

	for (i = 0; i < num_clients; i++) {

		struct ramdump_segment *ramdump_segments_tmp = NULL;

		switch (i) {
		case 0:
			client_name = "GPS";
			break;
		case 1:
			client_name = "FTM";
			break;
		case 2:
			client_name = "DIAG";
			break;
		default:
			dev_err(memsh_child->dev,
				"memshare: no memshare clients registered for client_id: %d\n",
				i);
			return -EINVAL;
		}

		if (!memblock[i].allotted) {
			dev_err(memsh_child->dev, "memshare: %s: memblock is not allotted\n",
			client_name);
			continue;
		}

		if (memblock[i].hyp_mapping &&
			memblock[i].peripheral ==
			DHMS_MEM_PROC_MPSS_V01) {
			dev_dbg(memsh_child->dev,
				"memshare: %s: hypervisor unmapping for client before elf dump\n",
				client_name);
			if (memblock[i].alloc_request)
				continue;
			ret = hyp_assign_phys(
					memblock[i].phy_addr,
					memblock[i].size,
					source_vmlist,
					1, dest_vmids,
					dest_perms, 1);
			if (ret) {
				/*
				 * This is an error case as hyp
				 * mapping was successful
				 * earlier but during unmap
				 * it lead to failure.
				 */
				dev_err(memsh_child->dev,
					"memshare: %s: failed to map the memory region to APPS\n",
					client_name);
				continue;
			} else {
				memblock[i].hyp_mapping = 0;
			}
		}

		if (!memblock[i].hyp_mapping) {
			ramdump_segments_tmp = kcalloc(1,
				sizeof(struct ramdump_segment),
				GFP_KERNEL);
			if (!ramdump_segments_tmp)
				return -ENOMEM;

			ramdump_segments_tmp[0].size = memblock[i].size;
			ramdump_segments_tmp[0].address = memblock[i].phy_addr;

			dev_dbg(memsh_child->dev, "memshare: %s: Begin elf dump for size = %d\n",
				client_name, memblock[i].size);

			ret = do_elf_ramdump(memshare_ramdump_dev[i],
						ramdump_segments_tmp, 1);
			kfree(ramdump_segments_tmp);
			if (ret < 0) {
				dev_err(memsh_child->dev,
					"memshare: %s: Unable to elf dump with failure: %d\n",
					client_name, ret);
				return ret;
			}
		}
	}
	return 0;
}

static int modem_notifier_cb(struct notifier_block *this, unsigned long code,
					void *_cmd)
{
	int i, ret, size = 0;
	u32 source_vmlist[1] = {VMID_MSS_MSA};
	int dest_vmids[1] = {VMID_HLOS};
	int dest_perms[1] = {PERM_READ|PERM_WRITE|PERM_EXEC};
	struct notif_data *notifdata = NULL;

	mutex_lock(&memsh_drv->mem_share);

	switch (code) {

	case SUBSYS_BEFORE_SHUTDOWN:
		bootup_request++;
		for (i = 0; i < MAX_CLIENTS; i++)
			memblock[i].alloc_request = 0;
		break;

	case SUBSYS_RAMDUMP_NOTIFICATION:
		ramdump_event = true;
		break;

	case SUBSYS_BEFORE_POWERUP:
		if (_cmd) {
			notifdata = (struct notif_data *) _cmd;
		} else {
			ramdump_event = false;
			break;
		}

		if (notifdata->enable_ramdump && ramdump_event) {
			dev_info(memsh_child->dev, "memshare: Ramdump collection is enabled\n");
			ret = mem_share_do_ramdump();
			if (ret)
				dev_err(memsh_child->dev, "memshare: Ramdump collection failed\n");
			ramdump_event = false;
		}
		break;

	case SUBSYS_AFTER_POWERUP:
		dev_dbg(memsh_child->dev, "memshare: Modem has booted up\n");
		for (i = 0; i < MAX_CLIENTS; i++) {
			size = memblock[i].size;
			if (memblock[i].free_memory > 0 &&
					bootup_request >= 2) {
				memblock[i].free_memory -= 1;
				dev_dbg(memsh_child->dev, "memshare: free_memory count: %d for client id: %d\n",
					memblock[i].free_memory,
					memblock[i].client_id);
			}

			if (memblock[i].free_memory == 0 &&
				memblock[i].peripheral ==
				DHMS_MEM_PROC_MPSS_V01 &&
				!memblock[i].guarantee &&
				!memblock[i].client_request &&
				memblock[i].allotted &&
				!memblock[i].alloc_request) {
				dev_info(memsh_child->dev,
					"memshare: hypervisor unmapping for allocated memory with client id: %d\n",
					memblock[i].client_id);
				if (memblock[i].hyp_mapping) {
					ret = hyp_assign_phys(
							memblock[i].phy_addr,
							memblock[i].size,
							source_vmlist,
							1, dest_vmids,
							dest_perms, 1);
					if (ret &&
						memblock[i].hyp_mapping == 1) {
						/*
						 * This is an error case as hyp
						 * mapping was successful
						 * earlier but during unmap
						 * it lead to failure.
						 */
						dev_err(memsh_child->dev,
							"memshare: failed to hypervisor unmap the memory region for client id: %d\n",
							memblock[i].client_id);
					} else {
						memblock[i].hyp_mapping = 0;
					}
				}
				if (memblock[i].guard_band) {
				/*
				 *	Check if the client required guard band
				 *	support so the memory region of client's
				 *	size + guard bytes of 4K can be freed.
				 */
					size += MEMSHARE_GUARD_BYTES;
				}
				dma_free_attrs(memsh_drv->dev,
					size, memblock[i].virtual_addr,
					memblock[i].phy_addr,
					attrs);
				free_client(i);
			}
		}
		bootup_request++;
		break;

	default:
		break;
	}

	mutex_unlock(&memsh_drv->mem_share);
	return NOTIFY_DONE;
}

static struct notifier_block nb = {
	.notifier_call = modem_notifier_cb,
};

static void shared_hyp_mapping(int client_id)
{
	int ret;
	u32 source_vmlist[1] = {VMID_HLOS};
	int dest_vmids[1] = {VMID_MSS_MSA};
	int dest_perms[1] = {PERM_READ|PERM_WRITE};

	if (client_id == DHMS_MEM_CLIENT_INVALID) {
		dev_err(memsh_child->dev,
			"memshare: hypervisor mapping failure for invalid client\n");
		return;
	}

	ret = hyp_assign_phys(memblock[client_id].phy_addr,
			memblock[client_id].size,
			source_vmlist, 1, dest_vmids,
			dest_perms, 1);

	if (ret != 0) {
		dev_err(memsh_child->dev, "memshare: hyp_assign_phys failed size=%u err=%d\n",
				memblock[client_id].size, ret);
		return;
	}
	memblock[client_id].hyp_mapping = 1;
}

static void handle_alloc_generic_req(struct qmi_handle *handle,
	struct sockaddr_qrtr *sq, struct qmi_txn *txn, const void *decoded_msg)
{
	struct mem_alloc_generic_req_msg_v01 *alloc_req;
	struct mem_alloc_generic_resp_msg_v01 *alloc_resp;
	int rc, resp = 0;
	int client_id;
	uint32_t size = 0;

	mutex_lock(&memsh_drv->mem_share);
	alloc_req = (struct mem_alloc_generic_req_msg_v01 *)decoded_msg;
	dev_info(memsh_child->dev,
		"memshare_alloc: memory alloc request received for client id: %d, proc_id: %d, request size: %d\n",
		alloc_req->client_id, alloc_req->proc_id, alloc_req->num_bytes);
	alloc_resp = kzalloc(sizeof(*alloc_resp),
					GFP_KERNEL);
	if (!alloc_resp) {
		mutex_unlock(&memsh_drv->mem_share);
		return;
	}
	alloc_resp->resp.result = QMI_RESULT_FAILURE_V01;
	alloc_resp->resp.error = QMI_ERR_NO_MEMORY_V01;
	client_id = check_client(alloc_req->client_id, alloc_req->proc_id,
								CHECK);

	if (client_id >= MAX_CLIENTS) {
		dev_err(memsh_child->dev,
			"memshare_alloc: client not found, requested client: %d, proc_id: %d\n",
			alloc_req->client_id, alloc_req->proc_id);
		kfree(alloc_resp);
		alloc_resp = NULL;
		mutex_unlock(&memsh_drv->mem_share);
		return;
	}

	if (!memblock[client_id].allotted) {
		if (memblock[client_id].guard_band && alloc_req->num_bytes > 0)
			size = alloc_req->num_bytes + MEMSHARE_GUARD_BYTES;
		else
			size = alloc_req->num_bytes;
		rc = memshare_alloc(memsh_drv->dev, size,
					&memblock[client_id]);
		if (rc) {
			dev_err(memsh_child->dev,
				"memshare_alloc: unable to allocate memory of size: %d for requested client\n",
				size);
			resp = 1;
		}
		if (!resp) {
			memblock[client_id].free_memory += 1;
			memblock[client_id].allotted = 1;
			memblock[client_id].size = alloc_req->num_bytes;
			memblock[client_id].peripheral = alloc_req->proc_id;
		}
	}
	dev_dbg(memsh_child->dev,
		"memshare_alloc: free memory count for client id: %d = %d\n",
		memblock[client_id].client_id, memblock[client_id].free_memory);

	memblock[client_id].sequence_id = alloc_req->sequence_id;
	memblock[client_id].alloc_request = 1;

	fill_alloc_response(alloc_resp, client_id, &resp);
	/*
	 * Perform the Hypervisor mapping in order to avoid XPU viloation
	 * to the allocated region for Modem Clients
	 */
	if (!memblock[client_id].hyp_mapping &&
		memblock[client_id].allotted)
		shared_hyp_mapping(client_id);
	mutex_unlock(&memsh_drv->mem_share);
	dev_info(memsh_child->dev,
		"memshare_alloc: client_id: %d, alloc_resp.num_bytes: %d, alloc_resp.resp.result: %lx\n",
		alloc_req->client_id,
		alloc_resp->dhms_mem_alloc_addr_info[0].num_bytes,
		(unsigned long)alloc_resp->resp.result);

	rc = qmi_send_response(mem_share_svc_handle, sq, txn,
			  MEM_ALLOC_GENERIC_RESP_MSG_V01,
			  sizeof(struct mem_alloc_generic_resp_msg_v01),
			  mem_alloc_generic_resp_msg_data_v01_ei, alloc_resp);
	if (rc < 0)
		dev_err(memsh_child->dev,
		"memshare_alloc: Error sending the alloc response: %d\n",
		rc);

	kfree(alloc_resp);
	alloc_resp = NULL;
}

static void handle_free_generic_req(struct qmi_handle *handle,
	struct sockaddr_qrtr *sq, struct qmi_txn *txn, const void *decoded_msg)
{
	struct mem_free_generic_req_msg_v01 *free_req;
	struct mem_free_generic_resp_msg_v01 free_resp;
	int rc, flag = 0, ret = 0, size = 0;
	uint32_t client_id;
	u32 source_vmlist[1] = {VMID_MSS_MSA};
	int dest_vmids[1] = {VMID_HLOS};
	int dest_perms[1] = {PERM_READ|PERM_WRITE|PERM_EXEC};

	mutex_lock(&memsh_drv->mem_free);
	free_req = (struct mem_free_generic_req_msg_v01 *)decoded_msg;
	memset(&free_resp, 0, sizeof(free_resp));
	free_resp.resp.error = QMI_ERR_INTERNAL_V01;
	free_resp.resp.result = QMI_RESULT_FAILURE_V01;
	dev_info(memsh_child->dev,
		"memshare_free: handling memory free request with client id: %d, proc_id: %d\n",
		free_req->client_id, free_req->proc_id);
	client_id = check_client(free_req->client_id, free_req->proc_id, FREE);
	if (client_id == DHMS_MEM_CLIENT_INVALID) {
		dev_err(memsh_child->dev, "memshare_free: invalid client request to free memory\n");
		flag = 1;
	} else if (!memblock[client_id].guarantee &&
				!memblock[client_id].client_request &&
				memblock[client_id].allotted) {
		dev_dbg(memsh_child->dev,
			"memshare_free: hypervisor unmapping for client_id:%d - size: %d\n",
			client_id, memblock[client_id].size);
		ret = hyp_assign_phys(memblock[client_id].phy_addr,
				memblock[client_id].size, source_vmlist, 1,
				dest_vmids, dest_perms, 1);
		if (ret && memblock[client_id].hyp_mapping == 1) {
		/*
		 * This is an error case as hyp mapping was successful
		 * earlier but during unmap it lead to failure.
		 */
			dev_err(memsh_child->dev,
				"memshare_free: failed to unmap the region for client id:%d\n",
				client_id);
		}
		size = memblock[client_id].size;
		if (memblock[client_id].guard_band) {
		/*
		 *	Check if the client required guard band support so
		 *	the memory region of client's size + guard
		 *	bytes of 4K can be freed
		 */
			size += MEMSHARE_GUARD_BYTES;
		}
		dma_free_attrs(memsh_drv->dev, size,
			memblock[client_id].virtual_addr,
			memblock[client_id].phy_addr,
			attrs);
		free_client(client_id);
	} else {
		dev_err(memsh_child->dev,
			"memshare_free: cannot free the memory for a guaranteed client (client_id: %d)\n",
			client_id);
	}

	if (flag) {
		free_resp.resp.result = QMI_RESULT_FAILURE_V01;
		free_resp.resp.error = QMI_ERR_INVALID_ID_V01;
	} else {
		free_resp.resp.result = QMI_RESULT_SUCCESS_V01;
		free_resp.resp.error = QMI_ERR_NONE_V01;
	}

	mutex_unlock(&memsh_drv->mem_free);
	rc = qmi_send_response(mem_share_svc_handle, sq, txn,
			  MEM_FREE_GENERIC_RESP_MSG_V01,
			  MEM_FREE_REQ_MAX_MSG_LEN_V01,
			  mem_free_generic_resp_msg_data_v01_ei, &free_resp);
	if (rc < 0)
		dev_err(memsh_child->dev,
		"memshare_free: error sending the free response: %d\n", rc);

}

static void handle_query_size_req(struct qmi_handle *handle,
	struct sockaddr_qrtr *sq, struct qmi_txn *txn, const void *decoded_msg)
{
	int rc, client_id;
	struct mem_query_size_req_msg_v01 *query_req;
	struct mem_query_size_rsp_msg_v01 *query_resp;

	mutex_lock(&memsh_drv->mem_share);
	query_req = (struct mem_query_size_req_msg_v01 *)decoded_msg;
	query_resp = kzalloc(sizeof(*query_resp),
					GFP_KERNEL);
	if (!query_resp) {
		mutex_unlock(&memsh_drv->mem_share);
		return;
	}
	dev_dbg(memsh_child->dev,
		"memshare_query: query on availalbe memory size for client id: %d, proc_id: %d\n",
		query_req->client_id, query_req->proc_id);
	client_id = check_client(query_req->client_id, query_req->proc_id,
								CHECK);

	if (client_id >= MAX_CLIENTS) {
		dev_err(memsh_child->dev,
			"memshare_query: client not found, requested client: %d, proc_id: %d\n",
			query_req->client_id, query_req->proc_id);
		kfree(query_resp);
		query_resp = NULL;
		mutex_unlock(&memsh_drv->mem_share);
		return;
	}

	if (memblock[client_id].init_size) {
		query_resp->size_valid = 1;
		query_resp->size = memblock[client_id].init_size;
	} else {
		query_resp->size_valid = 1;
		query_resp->size = 0;
	}
	query_resp->resp.result = QMI_RESULT_SUCCESS_V01;
	query_resp->resp.error = QMI_ERR_NONE_V01;
	mutex_unlock(&memsh_drv->mem_share);

	dev_info(memsh_child->dev,
		"memshare_query: client_id : %d, query_resp.size :%d, query_resp.resp.result :%lx\n",
		query_req->client_id, query_resp->size,
		(unsigned long)query_resp->resp.result);
	rc = qmi_send_response(mem_share_svc_handle, sq, txn,
			  MEM_QUERY_SIZE_RESP_MSG_V01,
			  MEM_QUERY_MAX_MSG_LEN_V01,
			  mem_query_size_resp_msg_data_v01_ei, query_resp);
	if (rc < 0)
		dev_err(memsh_child->dev,
		"memshare_query: Error sending the query response: %d\n", rc);

	kfree(query_resp);
	query_resp = NULL;
}

static void mem_share_svc_disconnect_cb(struct qmi_handle *qmi,
				  unsigned int node, unsigned int port)
{
}

static struct qmi_ops server_ops = {
	.del_client = mem_share_svc_disconnect_cb,
};

static struct qmi_msg_handler qmi_memshare_handlers[] = {
	{
		.type = QMI_REQUEST,
		.msg_id = MEM_ALLOC_GENERIC_REQ_MSG_V01,
		.ei = mem_alloc_generic_req_msg_data_v01_ei,
		.decoded_size = MEM_ALLOC_REQ_MAX_MSG_LEN_V01,
		.fn = handle_alloc_generic_req,
	},
	{
		.type = QMI_REQUEST,
		.msg_id = MEM_FREE_GENERIC_REQ_MSG_V01,
		.ei = mem_free_generic_req_msg_data_v01_ei,
		.decoded_size = MEM_FREE_REQ_MAX_MSG_LEN_V01,
		.fn = handle_free_generic_req,
	},
	{
		.type = QMI_REQUEST,
		.msg_id = MEM_QUERY_SIZE_REQ_MSG_V01,
		.ei = mem_query_size_req_msg_data_v01_ei,
		.decoded_size = MEM_QUERY_MAX_MSG_LEN_V01,
		.fn = handle_query_size_req,
	},
};

int memshare_alloc(struct device *dev,
					unsigned int block_size,
					struct mem_blocks *pblk)
{
	dev_dbg(memsh_child->dev,
		"memshare: allocation request for size: %d", block_size);

	if (!pblk) {
		dev_err(memsh_child->dev,
			"memshare: Failed memory block allocation\n");
		return -ENOMEM;
	}

	pblk->virtual_addr = dma_alloc_attrs(dev, block_size,
						&pblk->phy_addr, GFP_KERNEL,
						attrs);
	if (pblk->virtual_addr == NULL)
		return -ENOMEM;

	return 0;
}

static void memshare_init_worker(struct work_struct *work)
{
	int rc;

	mem_share_svc_workqueue =
		create_singlethread_workqueue("mem_share_svc");
	if (!mem_share_svc_workqueue)
		return;

	mem_share_svc_handle = kzalloc(sizeof(struct qmi_handle),
					  GFP_KERNEL);
	if (!mem_share_svc_handle) {
		destroy_workqueue(mem_share_svc_workqueue);
		return;
	}

	rc = qmi_handle_init(mem_share_svc_handle,
		sizeof(struct qmi_elem_info),
		&server_ops, qmi_memshare_handlers);
	if (rc < 0) {
		dev_err(memsh_child->dev,
			"memshare: Creating mem_share_svc qmi handle failed\n");
		kfree(mem_share_svc_handle);
		destroy_workqueue(mem_share_svc_workqueue);
		return;
	}
	rc = qmi_add_server(mem_share_svc_handle, MEM_SHARE_SERVICE_SVC_ID,
		MEM_SHARE_SERVICE_VERS, MEM_SHARE_SERVICE_INS_ID);
	if (rc < 0) {
		dev_err(memsh_child->dev,
			"memshare: Registering mem share svc failed %d\n", rc);
		qmi_handle_release(mem_share_svc_handle);
		kfree(mem_share_svc_handle);
		destroy_workqueue(mem_share_svc_workqueue);
		return;
	}
	dev_dbg(memsh_child->dev, "memshare: memshare_init successful\n");
}

static int memshare_child_probe(struct platform_device *pdev)
{
	int rc;
	uint32_t size, client_id;
	const char *name;
	struct memshare_child *drv;

	drv = devm_kzalloc(&pdev->dev, sizeof(struct memshare_child),
							GFP_KERNEL);

	if (!drv)
		return -ENOMEM;

	drv->dev = &pdev->dev;
	memsh_child = drv;
	platform_set_drvdata(pdev, memsh_child);

	rc = of_property_read_u32(pdev->dev.of_node, "qcom,peripheral-size",
						&size);
	if (rc) {
		dev_err(memsh_child->dev, "memshare: Error reading size of clients, rc: %d\n",
				rc);
		return rc;
	}

	rc = of_property_read_u32(pdev->dev.of_node, "qcom,client-id",
						&client_id);
	if (rc) {
		dev_err(memsh_child->dev, "memshare: Error reading client id, rc: %d\n",
				rc);
		return rc;
	}

	memblock[num_clients].guarantee = of_property_read_bool(
							pdev->dev.of_node,
							"qcom,allocate-boot-time");

	memblock[num_clients].client_request = of_property_read_bool(
							pdev->dev.of_node,
							"qcom,allocate-on-request");

	memblock[num_clients].guard_band = of_property_read_bool(
							pdev->dev.of_node,
							"qcom,guard-band");

	rc = of_property_read_string(pdev->dev.of_node, "label",
						&name);
	if (rc) {
		dev_err(memsh_child->dev, "memshare: Error reading peripheral info for client, rc: %d\n",
					rc);
		return rc;
	}

	if (strcmp(name, "modem") == 0)
		memblock[num_clients].peripheral = DHMS_MEM_PROC_MPSS_V01;
	else if (strcmp(name, "adsp") == 0)
		memblock[num_clients].peripheral = DHMS_MEM_PROC_ADSP_V01;
	else if (strcmp(name, "wcnss") == 0)
		memblock[num_clients].peripheral = DHMS_MEM_PROC_WCNSS_V01;

	memblock[num_clients].init_size = size;
	memblock[num_clients].client_id = client_id;

  /*
   *	Memshare allocation for guaranteed clients
   */
	if (memblock[num_clients].guarantee && size > 0) {
		if (memblock[num_clients].guard_band)
			size += MEMSHARE_GUARD_BYTES;
		rc = memshare_alloc(memsh_child->dev,
				size,
				&memblock[num_clients]);
		if (rc) {
			dev_err(memsh_child->dev,
				"memshare_child: Unable to allocate memory for guaranteed clients, rc: %d\n",
				rc);
			return rc;
		}
		memblock[num_clients].size = size;
		memblock[num_clients].allotted = 1;
		shared_hyp_mapping(num_clients);
	}

	/*
	 *  call for creating ramdump dev handlers for
	 *  memshare clients
	 */

	memshare_dev[num_clients] = &pdev->dev;

	if (!memblock[num_clients].file_created) {
		rc = mem_share_configure_ramdump(num_clients);
		if (rc)
			dev_err(memsh_child->dev,
			"memshare_child: cannot create ramdump for client with id: %d\n",
			memblock[num_clients].client_id);
		else
			memblock[num_clients].file_created = 1;
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

	if (!drv)
		return -ENOMEM;

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

	rc = of_platform_populate(pdev->dev.of_node, NULL, NULL,
				&pdev->dev);

	if (rc) {
		dev_err(memsh_child->dev,
			"memshare: error populating the devices\n");
		return rc;
	}

	subsys_notif_register_notifier("modem", &nb);
	dev_dbg(memsh_child->dev, "memshare: Memshare inited\n");

	return 0;
}

static int memshare_remove(struct platform_device *pdev)
{
	if (!memsh_drv)
		return 0;

	flush_workqueue(mem_share_svc_workqueue);
	qmi_handle_release(mem_share_svc_handle);
	kfree(mem_share_svc_handle);
	destroy_workqueue(mem_share_svc_workqueue);
	return 0;
}

static int memshare_child_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id memshare_match_table[] = {
	{
		.compatible = "qcom,memshare",
	},
	{}
};

static const struct of_device_id memshare_match_table1[] = {
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
		.of_match_table = memshare_match_table,
	},
};

static struct platform_driver memshare_pchild = {
	.probe          = memshare_child_probe,
	.remove         = memshare_child_remove,
	.driver = {
		.name   = MEMSHARE_CHILD_DEV_NAME,
		.of_match_table = memshare_match_table1,
	},
};

module_platform_driver(memshare_pdriver);
module_platform_driver(memshare_pchild);

MODULE_DESCRIPTION("Mem Share QMI Service Driver");
MODULE_LICENSE("GPL v2");
