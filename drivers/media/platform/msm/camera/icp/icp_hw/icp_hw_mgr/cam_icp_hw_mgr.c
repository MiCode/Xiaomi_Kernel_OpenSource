/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "ICP-HW-MGR %s:%d " fmt, __func__, __LINE__

#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <media/cam_defs.h>
#include <media/cam_icp.h>
#include <linux/debugfs.h>

#include "cam_sync_api.h"
#include "cam_packet_util.h"
#include "cam_hw.h"
#include "cam_hw_mgr_intf.h"
#include "cam_icp_hw_mgr_intf.h"
#include "cam_icp_hw_mgr.h"
#include "cam_a5_hw_intf.h"
#include "cam_bps_hw_intf.h"
#include "cam_ipe_hw_intf.h"
#include "cam_smmu_api.h"
#include "cam_mem_mgr.h"
#include "hfi_intf.h"
#include "hfi_reg.h"
#include "hfi_session_defs.h"
#include "hfi_sys_defs.h"
#include "cam_req_mgr_workq.h"
#include "cam_mem_mgr.h"
#include "a5_core.h"
#include "hfi_sys_defs.h"

#undef  ICP_DBG
#define ICP_DBG(fmt, args...) pr_debug(fmt, ##args)

#define ICP_WORKQ_NUM_TASK 30
#define ICP_WORKQ_TASK_CMD_TYPE 1
#define ICP_WORKQ_TASK_MSG_TYPE 2

static struct cam_icp_hw_mgr icp_hw_mgr;

static int cam_icp_hw_mgr_create_debugfs_entry(void)
{
	icp_hw_mgr.dentry = debugfs_create_dir("camera_icp", NULL);
	if (!icp_hw_mgr.dentry)
		return -ENOMEM;

	if (!debugfs_create_bool("a5_debug",
		0644,
		icp_hw_mgr.dentry,
		&icp_hw_mgr.a5_debug)) {
		debugfs_remove_recursive(icp_hw_mgr.dentry);
		return -ENOMEM;
	}

	return 0;
}

static int cam_icp_stop_cpas(struct cam_icp_hw_mgr *hw_mgr_priv)
{
	struct cam_hw_intf *a5_dev_intf = NULL;
	struct cam_hw_intf *ipe0_dev_intf = NULL;
	struct cam_hw_intf *ipe1_dev_intf = NULL;
	struct cam_hw_intf *bps_dev_intf = NULL;
	struct cam_icp_hw_mgr *hw_mgr = hw_mgr_priv;
	struct cam_icp_cpas_vote cpas_vote;
	int rc = 0;

	if (!hw_mgr) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	a5_dev_intf = hw_mgr->devices[CAM_ICP_DEV_A5][0];
	bps_dev_intf = hw_mgr->devices[CAM_ICP_DEV_BPS][0];
	ipe0_dev_intf = hw_mgr->devices[CAM_ICP_DEV_IPE][0];

	if ((!a5_dev_intf) || (!bps_dev_intf) || (!ipe0_dev_intf)) {
		pr_err("dev intfs are NULL\n");
		return -EINVAL;
	}

	rc = a5_dev_intf->hw_ops.process_cmd(
		a5_dev_intf->hw_priv,
		CAM_ICP_A5_CMD_CPAS_STOP,
		&cpas_vote,
		sizeof(struct cam_icp_cpas_vote));
	if (rc < 0)
		pr_err("CAM_ICP_A5_CMD_CPAS_STOP is failed: %d\n", rc);

	rc = bps_dev_intf->hw_ops.process_cmd(
		bps_dev_intf->hw_priv,
		CAM_ICP_BPS_CMD_CPAS_STOP,
		&cpas_vote,
		sizeof(struct cam_icp_cpas_vote));
	if (rc < 0)
		pr_err("CAM_ICP_BPS_CMD_CPAS_STOP is failed: %d\n", rc);

	rc = ipe0_dev_intf->hw_ops.process_cmd(
		ipe0_dev_intf->hw_priv,
		CAM_ICP_IPE_CMD_CPAS_STOP,
		&cpas_vote,
		sizeof(struct cam_icp_cpas_vote));
	if (rc < 0)
		pr_err("CAM_ICP_IPE_CMD_CPAS_STOP is failed: %d\n", rc);

	ipe1_dev_intf = hw_mgr->devices[CAM_ICP_DEV_IPE][1];
	if (!ipe1_dev_intf)
		return rc;

	rc = ipe1_dev_intf->hw_ops.process_cmd(
		ipe1_dev_intf->hw_priv,
		CAM_ICP_IPE_CMD_CPAS_STOP,
		&cpas_vote,
		sizeof(struct cam_icp_cpas_vote));
	if (rc < 0)
		pr_err("CAM_ICP_IPE_CMD_CPAS_STOP is failed: %d\n", rc);

	return rc;
}

static int cam_icp_start_cpas(struct cam_icp_hw_mgr *hw_mgr_priv)
{
	struct cam_hw_intf *a5_dev_intf = NULL;
	struct cam_hw_intf *ipe0_dev_intf = NULL;
	struct cam_hw_intf *ipe1_dev_intf = NULL;
	struct cam_hw_intf *bps_dev_intf = NULL;
	struct cam_icp_hw_mgr *hw_mgr = hw_mgr_priv;
	struct cam_icp_cpas_vote cpas_vote;
	int rc = 0;

	if (!hw_mgr) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	a5_dev_intf = hw_mgr->devices[CAM_ICP_DEV_A5][0];
	bps_dev_intf = hw_mgr->devices[CAM_ICP_DEV_BPS][0];
	ipe0_dev_intf = hw_mgr->devices[CAM_ICP_DEV_IPE][0];

	if ((!a5_dev_intf) || (!bps_dev_intf) || (!ipe0_dev_intf)) {
		pr_err("dev intfs are null\n");
		return -EINVAL;
	}

	cpas_vote.ahb_vote.type = CAM_VOTE_ABSOLUTE;
	cpas_vote.ahb_vote.vote.level = CAM_TURBO_VOTE;
	cpas_vote.axi_vote.compressed_bw = 640000000;
	cpas_vote.axi_vote.uncompressed_bw = 640000000;

	rc = a5_dev_intf->hw_ops.process_cmd(
		a5_dev_intf->hw_priv,
		CAM_ICP_A5_CMD_CPAS_START,
		&cpas_vote,
		sizeof(struct cam_icp_cpas_vote));
	if (rc) {
		pr_err("CAM_ICP_A5_CMD_CPAS_START is failed: %d\n", rc);
		goto a5_cpas_start_failed;
	}

	rc = bps_dev_intf->hw_ops.process_cmd(
		bps_dev_intf->hw_priv,
		CAM_ICP_BPS_CMD_CPAS_START,
		&cpas_vote,
		sizeof(struct cam_icp_cpas_vote));
	if (rc < 0) {
		pr_err("CAM_ICP_BPS_CMD_CPAS_START is failed: %d\n", rc);
		goto bps_cpas_start_failed;
	}

	rc = ipe0_dev_intf->hw_ops.process_cmd(
		ipe0_dev_intf->hw_priv,
		CAM_ICP_IPE_CMD_CPAS_START,
		&cpas_vote,
		sizeof(struct cam_icp_cpas_vote));
	if (rc < 0) {
		pr_err("CAM_ICP_IPE_CMD_CPAS_START is failed: %d\n", rc);
		goto ipe0_cpas_start_failed;
	}

	ipe1_dev_intf = hw_mgr->devices[CAM_ICP_DEV_IPE][1];
	if (!ipe1_dev_intf)
		return rc;

	rc = ipe1_dev_intf->hw_ops.process_cmd(
		ipe1_dev_intf->hw_priv,
		CAM_ICP_IPE_CMD_CPAS_START,
		&cpas_vote,
		sizeof(struct cam_icp_cpas_vote));
	if (rc < 0) {
		pr_err("CAM_ICP_IPE_CMD_CPAS_START is failed: %d\n", rc);
		goto ipe1_cpas_start_failed;
	}

	return rc;

ipe1_cpas_start_failed:
	rc = ipe0_dev_intf->hw_ops.process_cmd(
		ipe0_dev_intf->hw_priv,
		CAM_ICP_IPE_CMD_CPAS_STOP,
		&cpas_vote,
		sizeof(struct cam_icp_cpas_vote));
ipe0_cpas_start_failed:
	rc = bps_dev_intf->hw_ops.process_cmd(
		bps_dev_intf->hw_priv,
		CAM_ICP_BPS_CMD_CPAS_STOP,
		&cpas_vote,
		sizeof(struct cam_icp_cpas_vote));
bps_cpas_start_failed:
	rc = a5_dev_intf->hw_ops.process_cmd(
		a5_dev_intf->hw_priv,
		CAM_ICP_A5_CMD_CPAS_STOP,
		&cpas_vote,
		sizeof(struct cam_icp_cpas_vote));
a5_cpas_start_failed:
	return rc;
}

static int cam_icp_mgr_process_cmd(void *priv, void *data)
{
	int rc;
	struct hfi_cmd_work_data *task_data = NULL;
	struct cam_icp_hw_mgr *hw_mgr;

	if (!data || !priv) {
		pr_err("Invalid params%pK %pK\n", data, priv);
		return -EINVAL;
	}

	hw_mgr = priv;
	task_data = (struct hfi_cmd_work_data *)data;

	rc = hfi_write_cmd(task_data->data);
	if (rc < 0)
		pr_err("unable to write\n");

	ICP_DBG("task type : %u, rc : %d\n", task_data->type, rc);
	return rc;
}

static int cam_icp_mgr_process_msg_frame_process(uint32_t *msg_ptr)
{
	int i;
	uint32_t idx;
	uint32_t request_id;
	struct cam_icp_hw_ctx_data *ctx_data = NULL;
	struct hfi_msg_ipebps_async_ack *ioconfig_ack = NULL;
	struct hfi_msg_frame_process_done *frame_done;
	struct hfi_frame_process_info *hfi_frame_process;
	struct cam_hw_done_event_data   buf_data;

	ioconfig_ack = (struct hfi_msg_ipebps_async_ack *)msg_ptr;
	if (ioconfig_ack->err_type != HFI_ERR_SYS_NONE) {
		pr_err("failed with error : %u\n", ioconfig_ack->err_type);
		return -EIO;
	}

	frame_done =
		(struct hfi_msg_frame_process_done *)ioconfig_ack->msg_data;
	if (frame_done->result) {
		pr_err("result : %u\n", frame_done->result);
		return -EIO;
	}
	ICP_DBG("result : %u\n", frame_done->result);

	ctx_data = (struct cam_icp_hw_ctx_data *)ioconfig_ack->user_data1;
	request_id = ioconfig_ack->user_data2;
	ICP_DBG("ctx : %pK, request_id :%d\n",
		(void *)ctx_data->context_priv, request_id);

	hfi_frame_process = &ctx_data->hfi_frame_process;
	for (i = 0; i < CAM_FRAME_CMD_MAX; i++)
		if (hfi_frame_process->request_id[i] == request_id)
			break;

	if (i >= CAM_FRAME_CMD_MAX) {
		pr_err("unable to find pkt in ctx data for req_id =%d\n",
			request_id);
		return -EINVAL;
	}
	idx = i;

	/* send event to ctx this needs to be done in msg handler */
	buf_data.num_handles = hfi_frame_process->num_out_resources[idx];
	for (i = 0; i < buf_data.num_handles; i++)
		buf_data.resource_handle[i] =
			hfi_frame_process->out_resource[idx][i];

	ctx_data->ctxt_event_cb(ctx_data->context_priv, 0, &buf_data);

	/* now release memory for hfi frame process command */
	ICP_DBG("matching request id: %d\n",
			hfi_frame_process->request_id[idx]);
	mutex_lock(&ctx_data->hfi_frame_process.lock);
	hfi_frame_process->request_id[idx] = 0;
	clear_bit(idx, ctx_data->hfi_frame_process.bitmap);
	mutex_unlock(&ctx_data->hfi_frame_process.lock);
	return 0;
}

static int cam_icp_mgr_process_msg_config_io(uint32_t *msg_ptr)
{
	struct cam_icp_hw_ctx_data *ctx_data = NULL;
	struct hfi_msg_ipebps_async_ack *ioconfig_ack = NULL;
	struct hfi_msg_ipe_config *ipe_config_ack = NULL;
	struct hfi_msg_bps_common *bps_config_ack = NULL;

	ioconfig_ack = (struct hfi_msg_ipebps_async_ack *)msg_ptr;
	ICP_DBG("opcode : %u\n", ioconfig_ack->opcode);

	if (ioconfig_ack->opcode == HFI_IPEBPS_CMD_OPCODE_IPE_CONFIG_IO) {
		ipe_config_ack =
			(struct hfi_msg_ipe_config *)(ioconfig_ack->msg_data);
		if (ipe_config_ack->rc) {
			pr_err("rc = %d err = %u\n",
				ipe_config_ack->rc, ioconfig_ack->err_type);
			return -EIO;
		}
		ctx_data =
			(struct cam_icp_hw_ctx_data *)ioconfig_ack->user_data1;
		mutex_lock(&ctx_data->ctx_mutex);
		ctx_data->scratch_mem_size = ipe_config_ack->scratch_mem_size;
		mutex_unlock(&ctx_data->ctx_mutex);
		ICP_DBG("scratch_mem_size = %u\n",
			ipe_config_ack->scratch_mem_size);
	} else {
		bps_config_ack =
			(struct hfi_msg_bps_common *)(ioconfig_ack->msg_data);
		if (bps_config_ack->rc) {
			pr_err("rc : %u, opcode :%u\n",
				bps_config_ack->rc, ioconfig_ack->opcode);
			return -EIO;
		}
		ctx_data =
			(struct cam_icp_hw_ctx_data *)ioconfig_ack->user_data1;
	}
	complete(&ctx_data->wait_complete);

	return 0;
}

static int cam_icp_mgr_process_msg_create_handle(uint32_t *msg_ptr)
{
	struct hfi_msg_create_handle_ack *create_handle_ack = NULL;
	struct cam_icp_hw_ctx_data *ctx_data = NULL;

	create_handle_ack = (struct hfi_msg_create_handle_ack *)msg_ptr;
	if (!create_handle_ack) {
		pr_err("Invalid create_handle_ack\n");
		return -EINVAL;
	}

	ICP_DBG("err type : %u\n", create_handle_ack->err_type);

	ctx_data = (struct cam_icp_hw_ctx_data *)create_handle_ack->user_data1;
	if (!ctx_data) {
		pr_err("Invalid ctx_data\n");
		return -EINVAL;
	}

	mutex_lock(&ctx_data->ctx_mutex);
	ctx_data->fw_handle = create_handle_ack->fw_handle;
	mutex_unlock(&ctx_data->ctx_mutex);
	ICP_DBG("fw_handle = %x\n", ctx_data->fw_handle);
	complete(&ctx_data->wait_complete);

	return 0;
}

static int cam_icp_mgr_process_msg_ping_ack(uint32_t *msg_ptr)
{
	struct hfi_msg_ping_ack *ping_ack = NULL;
	struct cam_icp_hw_ctx_data *ctx_data = NULL;

	ping_ack = (struct hfi_msg_ping_ack *)msg_ptr;
	if (!ping_ack) {
		pr_err("Empty ping ack message\n");
		return -EINVAL;
	}

	ctx_data = (struct cam_icp_hw_ctx_data *)ping_ack->user_data;
	if (!ctx_data) {
		pr_err("Invalid ctx_data\n");
		return -EINVAL;
	}

	ICP_DBG("%x %x %pK\n", ping_ack->size, ping_ack->pkt_type,
		(void *)ping_ack->user_data);
	complete(&ctx_data->wait_complete);

	return 0;
}

static int cam_icp_mgr_process_indirect_ack_msg(uint32_t *msg_ptr)
{
	int rc;

	switch (msg_ptr[ICP_PACKET_IPCODE]) {
	case HFI_IPEBPS_CMD_OPCODE_IPE_CONFIG_IO:
	case HFI_IPEBPS_CMD_OPCODE_BPS_CONFIG_IO:
		ICP_DBG("received HFI_IPEBPS_CMD_OPCODE_IPE/BPS_CONFIG_IO:\n");
		rc = cam_icp_mgr_process_msg_config_io(msg_ptr);
		if (rc < 0) {
			pr_err("error in process_msg_config_io\n");
			return rc;
		}
		break;

	case HFI_IPEBPS_CMD_OPCODE_IPE_FRAME_PROCESS:
	case HFI_IPEBPS_CMD_OPCODE_BPS_FRAME_PROCESS:
		ICP_DBG("received OPCODE_IPE/BPS_FRAME_PROCESS:\n");
		rc = cam_icp_mgr_process_msg_frame_process(msg_ptr);
		if (rc < 0) {
			pr_err("error in msg_frame_process\n");
			return rc;
		}
		break;
	default:
		pr_err("Invalid opcode : %u\n",
			msg_ptr[ICP_PACKET_IPCODE]);
		break;
	}

	return 0;
}

static int cam_icp_mgr_process_direct_ack_msg(uint32_t *msg_ptr)
{
	struct cam_icp_hw_ctx_data *ctx_data = NULL;
	struct hfi_msg_ipebps_async_ack *ioconfig_ack = NULL;

	if (msg_ptr[ICP_PACKET_IPCODE] ==
		HFI_IPEBPS_CMD_OPCODE_IPE_DESTROY ||
		msg_ptr[ICP_PACKET_IPCODE] ==
		HFI_IPEBPS_CMD_OPCODE_BPS_DESTROY) {
		ICP_DBG("received HFI_IPEBPS_CMD_OPCODE_IPE/BPS_DESTROY:\n");
		ioconfig_ack = (struct hfi_msg_ipebps_async_ack *)msg_ptr;
		ctx_data =
			(struct cam_icp_hw_ctx_data *)ioconfig_ack->user_data1;
		complete(&ctx_data->wait_complete);

	} else {
		pr_err("Invalid opcode : %u\n", msg_ptr[ICP_PACKET_IPCODE]);
		return -EINVAL;
	}

	return 0;
}

static int32_t cam_icp_mgr_process_msg(void *priv, void *data)
{
	int rc = 0;
	uint32_t *msg_ptr = NULL;
	struct hfi_msg_work_data *task_data;
	struct cam_icp_hw_mgr *hw_mgr;
	int read_len;

	if (!data || !priv) {
		pr_err("Invalid data\n");
		return -EINVAL;
	}

	task_data = data;
	hw_mgr = priv;
	ICP_DBG("irq status : %u\n", task_data->irq_status);

	read_len = hfi_read_message(icp_hw_mgr.msg_buf, Q_MSG);
	if (read_len < 0) {
		ICP_DBG("Unable to read msg q\n");
		return read_len;
	}

	msg_ptr = (uint32_t *)icp_hw_mgr.msg_buf;
	ICP_DBG("packet type: %x\n", msg_ptr[ICP_PACKET_TYPE]);

	mutex_lock(&hw_mgr->hw_mgr_mutex);
	switch (msg_ptr[ICP_PACKET_TYPE]) {
	case HFI_MSG_SYS_INIT_DONE:
		ICP_DBG("received HFI_MSG_SYS_INIT_DONE\n");
		complete(&hw_mgr->a5_complete);
		break;

	case HFI_MSG_SYS_PING_ACK:
		ICP_DBG("received HFI_MSG_SYS_PING_ACK\n");
		rc = cam_icp_mgr_process_msg_ping_ack(msg_ptr);
		if (rc)
			pr_err("fail process PING_ACK\n");
		break;

	case HFI_MSG_IPEBPS_CREATE_HANDLE_ACK:
		ICP_DBG("received HFI_MSG_IPEBPS_CREATE_HANDLE_ACK\n");
		rc = cam_icp_mgr_process_msg_create_handle(msg_ptr);
		if (rc)
			pr_err("fail process CREATE_HANDLE_ACK\n");
		break;

	case HFI_MSG_IPEBPS_ASYNC_COMMAND_INDIRECT_ACK:
		rc = cam_icp_mgr_process_indirect_ack_msg(msg_ptr);
		if (rc)
			pr_err("fail process INDIRECT_ACK\n");
		break;

	case  HFI_MSG_IPEBPS_ASYNC_COMMAND_DIRECT_ACK:
		rc = cam_icp_mgr_process_direct_ack_msg(msg_ptr);
		if (rc)
			pr_err("fail process DIRECT_ACK\n");
		break;

	case HFI_MSG_EVENT_NOTIFY:
		ICP_DBG("received HFI_MSG_EVENT_NOTIFY\n");
		break;

	default:
		pr_err("invalid msg : %u\n", msg_ptr[ICP_PACKET_TYPE]);
		break;
	}

	mutex_unlock(&icp_hw_mgr.hw_mgr_mutex);

	return rc;
}

int32_t cam_icp_hw_mgr_cb(uint32_t irq_status, void *data)
{
	int32_t rc = 0;
	unsigned long flags;
	struct cam_icp_hw_mgr *hw_mgr = data;
	struct crm_workq_task *task;
	struct hfi_msg_work_data *task_data;

	spin_lock_irqsave(&hw_mgr->hw_mgr_lock, flags);
	task = cam_req_mgr_workq_get_task(icp_hw_mgr.msg_work);
	if (!task) {
		pr_err("no empty task\n");
		spin_unlock_irqrestore(&hw_mgr->hw_mgr_lock, flags);
		return -ENOMEM;
	}

	task_data = (struct hfi_msg_work_data *)task->payload;
	task_data->data = hw_mgr;
	task_data->irq_status = irq_status;
	task_data->type = ICP_WORKQ_TASK_MSG_TYPE;
	task->process_cb = cam_icp_mgr_process_msg;
	rc = cam_req_mgr_workq_enqueue_task(task, &icp_hw_mgr,
		CRM_TASK_PRIORITY_0);
	spin_unlock_irqrestore(&hw_mgr->hw_mgr_lock, flags);

	return rc;
}

static int cam_icp_free_hfi_mem(void)
{
	cam_smmu_dealloc_firmware(icp_hw_mgr.iommu_hdl);
	cam_mem_mgr_release_mem(&icp_hw_mgr.hfi_mem.qtbl);
	cam_mem_mgr_release_mem(&icp_hw_mgr.hfi_mem.cmd_q);
	cam_mem_mgr_release_mem(&icp_hw_mgr.hfi_mem.msg_q);
	cam_mem_mgr_release_mem(&icp_hw_mgr.hfi_mem.dbg_q);
	cam_mem_mgr_release_mem(&icp_hw_mgr.hfi_mem.sec_heap);

	return 0;
}

static int cam_icp_allocate_hfi_mem(void)
{
	int rc;
	struct cam_mem_mgr_request_desc alloc;
	struct cam_mem_mgr_memory_desc out;
	dma_addr_t iova;
	uint64_t kvaddr;
	size_t len;

	rc = cam_smmu_get_region_info(icp_hw_mgr.iommu_hdl,
		CAM_MEM_MGR_REGION_SHARED,
		&icp_hw_mgr.hfi_mem.shmem);
	if (rc)
		return -ENOMEM;

	rc = cam_smmu_alloc_firmware(icp_hw_mgr.iommu_hdl,
		&iova, &kvaddr, &len);
	if (rc < 0) {
		pr_err("Unable to allocate FW memory\n");
		return -ENOMEM;
	}

	icp_hw_mgr.hfi_mem.fw_buf.len = len;
	icp_hw_mgr.hfi_mem.fw_buf.kva = kvaddr;
	icp_hw_mgr.hfi_mem.fw_buf.iova = iova;
	icp_hw_mgr.hfi_mem.fw_buf.smmu_hdl = icp_hw_mgr.iommu_hdl;

	ICP_DBG("kva = %llX\n", kvaddr);
	ICP_DBG("IOVA = %llX\n", iova);
	ICP_DBG("length = %zu\n", len);

	memset(&alloc, 0, sizeof(alloc));
	memset(&out, 0, sizeof(out));
	alloc.size = SZ_1M;
	alloc.align = 0;
	alloc.region = CAM_MEM_MGR_REGION_SHARED;
	alloc.smmu_hdl = icp_hw_mgr.iommu_hdl;
	rc = cam_mem_mgr_request_mem(&alloc, &out);
	if (rc < 0) {
		pr_err("Unable to allocate qtbl memory\n");
		goto qtbl_alloc_failed;
	}
	icp_hw_mgr.hfi_mem.qtbl = out;

	ICP_DBG("kva = %llX\n", out.kva);
	ICP_DBG("qtbl IOVA = %X\n", out.iova);
	ICP_DBG("SMMU HDL = %X\n", out.smmu_hdl);
	ICP_DBG("MEM HDL = %X\n", out.mem_handle);
	ICP_DBG("length = %lld\n", out.len);
	ICP_DBG("region = %d\n", out.region);

	/* Allocate memory for cmd queue */
	memset(&alloc, 0, sizeof(alloc));
	memset(&out, 0, sizeof(out));
	alloc.size = SZ_1M;
	alloc.align = 0;
	alloc.region = CAM_MEM_MGR_REGION_SHARED;
	alloc.smmu_hdl = icp_hw_mgr.iommu_hdl;
	rc = cam_mem_mgr_request_mem(&alloc, &out);
	if (rc < 0) {
		pr_err("Unable to allocate cmd q memory\n");
		goto cmd_q_alloc_failed;
	}
	icp_hw_mgr.hfi_mem.cmd_q = out;

	ICP_DBG("kva = %llX\n", out.kva);
	ICP_DBG("cmd_q IOVA = %X\n", out.iova);
	ICP_DBG("SMMU HDL = %X\n", out.smmu_hdl);
	ICP_DBG("MEM HDL = %X\n", out.mem_handle);
	ICP_DBG("length = %lld\n", out.len);
	ICP_DBG("region = %d\n", out.region);

	/* Allocate memory for msg queue */
	memset(&alloc, 0, sizeof(alloc));
	memset(&out, 0, sizeof(out));
	alloc.size = SZ_1M;
	alloc.align = 0;
	alloc.region = CAM_MEM_MGR_REGION_SHARED;
	alloc.smmu_hdl = icp_hw_mgr.iommu_hdl;
	rc = cam_mem_mgr_request_mem(&alloc, &out);
	if (rc < 0) {
		pr_err("Unable to allocate msg q memory\n");
		goto msg_q_alloc_failed;
	}
	icp_hw_mgr.hfi_mem.msg_q = out;

	ICP_DBG("kva = %llX\n", out.kva);
	ICP_DBG("msg_q IOVA = %X\n", out.iova);
	ICP_DBG("SMMU HDL = %X\n", out.smmu_hdl);
	ICP_DBG("MEM HDL = %X\n", out.mem_handle);
	ICP_DBG("length = %lld\n", out.len);
	ICP_DBG("region = %d\n", out.region);

	/* Allocate memory for dbg queue */
	memset(&alloc, 0, sizeof(alloc));
	memset(&out, 0, sizeof(out));
	alloc.size = SZ_1M;
	alloc.align = 0;
	alloc.region = CAM_MEM_MGR_REGION_SHARED;
	alloc.smmu_hdl = icp_hw_mgr.iommu_hdl;
	rc = cam_mem_mgr_request_mem(&alloc, &out);
	if (rc < 0) {
		pr_err("Unable to allocate dbg q memory\n");
		goto dbg_q_alloc_failed;
	}
	icp_hw_mgr.hfi_mem.dbg_q = out;

	ICP_DBG("kva = %llX\n", out.kva);
	ICP_DBG("dbg_q IOVA = %X\n", out.iova);
	ICP_DBG("SMMU HDL = %X\n",  out.smmu_hdl);
	ICP_DBG("MEM HDL = %X\n", out.mem_handle);
	ICP_DBG("length = %lld\n", out.len);
	ICP_DBG("region = %d\n", out.region);

	/* Allocate memory for sec heap queue */
	memset(&alloc, 0, sizeof(alloc));
	memset(&out, 0, sizeof(out));
	alloc.size = SZ_1M;
	alloc.align = 0;
	alloc.region = CAM_MEM_MGR_REGION_SHARED;
	alloc.smmu_hdl = icp_hw_mgr.iommu_hdl;
	rc = cam_mem_mgr_request_mem(&alloc, &out);
	if (rc < 0) {
		pr_err("Unable to allocate sec heap q memory\n");
		goto sec_heap_alloc_failed;
	}
	icp_hw_mgr.hfi_mem.sec_heap = out;

	ICP_DBG("kva = %llX\n", out.kva);
	ICP_DBG("sec_heap IOVA = %X\n", out.iova);
	ICP_DBG("SMMU HDL = %X\n", out.smmu_hdl);
	ICP_DBG("MEM HDL = %X\n", out.mem_handle);
	ICP_DBG("length = %lld\n", out.len);
	ICP_DBG("region = %d\n", out.region);

	return rc;

sec_heap_alloc_failed:
	cam_mem_mgr_release_mem(&icp_hw_mgr.hfi_mem.dbg_q);
dbg_q_alloc_failed:
	cam_mem_mgr_release_mem(&icp_hw_mgr.hfi_mem.msg_q);
msg_q_alloc_failed:
	cam_mem_mgr_release_mem(&icp_hw_mgr.hfi_mem.cmd_q);
cmd_q_alloc_failed:
	cam_mem_mgr_release_mem(&icp_hw_mgr.hfi_mem.qtbl);
qtbl_alloc_failed:
	cam_smmu_dealloc_firmware(icp_hw_mgr.iommu_hdl);
	pr_err("returned with error : %d\n", rc);

	return rc;
}

static int cam_icp_mgr_get_free_ctx(struct cam_icp_hw_mgr *hw_mgr)
{
	int i = 0;
	int num_ctx = CAM_ICP_CTX_MAX;

	for (i = 0; i < num_ctx; i++) {
		mutex_lock(&hw_mgr->ctx_data[i].ctx_mutex);
		if (hw_mgr->ctx_data[i].in_use == 0) {
			hw_mgr->ctx_data[i].in_use = 1;
			mutex_unlock(&hw_mgr->ctx_data[i].ctx_mutex);
			break;
		}
		mutex_unlock(&hw_mgr->ctx_data[i].ctx_mutex);
	}

	return i;
}

static int cam_icp_mgr_destroy_handle(
		struct cam_icp_hw_ctx_data *ctx_data,
		struct crm_workq_task *task)
{
	int rc = 0;
	int timeout = 5000;
	struct hfi_cmd_work_data *task_data;
	struct hfi_cmd_ipebps_async destroy_cmd;
	unsigned long rem_jiffies;

	destroy_cmd.size =
		sizeof(struct hfi_cmd_ipebps_async) +
		sizeof(struct ipe_bps_destroy) -
		sizeof(destroy_cmd.payload.direct);
	destroy_cmd.pkt_type = HFI_CMD_IPEBPS_ASYNC_COMMAND_DIRECT;
	if (ctx_data->icp_dev_acquire_info.dev_type == CAM_ICP_RES_TYPE_BPS)
		destroy_cmd.opcode = HFI_IPEBPS_CMD_OPCODE_BPS_DESTROY;
	else
		destroy_cmd.opcode = HFI_IPEBPS_CMD_OPCODE_IPE_DESTROY;

	reinit_completion(&ctx_data->wait_complete);
	destroy_cmd.num_fw_handles = 1;
	destroy_cmd.fw_handles[0] = ctx_data->fw_handle;
	destroy_cmd.user_data1 = (uint64_t)ctx_data;
	destroy_cmd.user_data2 = (uint64_t)0x0;
	memcpy(destroy_cmd.payload.direct, &ctx_data->temp_payload,
						sizeof(uint32_t));

	task_data = (struct hfi_cmd_work_data *)task->payload;
	task_data->data = (void *)&destroy_cmd;
	task_data->request_id = 0;
	task_data->type = ICP_WORKQ_TASK_CMD_TYPE;
	task->process_cb = cam_icp_mgr_process_cmd;
	cam_req_mgr_workq_enqueue_task(task, &icp_hw_mgr, CRM_TASK_PRIORITY_0);
	ICP_DBG("fw_handle = %x ctx_data = %pK\n",
		ctx_data->fw_handle, ctx_data);
	rem_jiffies = wait_for_completion_timeout(&ctx_data->wait_complete,
			msecs_to_jiffies((timeout)));
	if (!rem_jiffies) {
		rc = -ETIMEDOUT;
		pr_err("FW response timeout: %d\n", rc);
	}

	return rc;
}

static int cam_icp_mgr_release_ctx(struct cam_icp_hw_mgr *hw_mgr, int ctx_id)
{
	struct crm_workq_task *task;
	int i = 0;

	if (ctx_id >= CAM_ICP_CTX_MAX) {
		pr_err("ctx_id is wrong: %d\n", ctx_id);
		return -EINVAL;
	}

	mutex_lock(&hw_mgr->ctx_data[ctx_id].ctx_mutex);
	if (!hw_mgr->ctx_data[ctx_id].in_use) {
		pr_err("ctx is already in use: %d\n", ctx_id);
		mutex_unlock(&hw_mgr->ctx_data[ctx_id].ctx_mutex);
		return -EINVAL;
	}
	mutex_unlock(&hw_mgr->ctx_data[ctx_id].ctx_mutex);

	mutex_lock(&hw_mgr->hw_mgr_mutex);
	task = cam_req_mgr_workq_get_task(icp_hw_mgr.cmd_work);
	mutex_unlock(&hw_mgr->hw_mgr_mutex);
	if (task)
		cam_icp_mgr_destroy_handle(&hw_mgr->ctx_data[ctx_id], task);

	mutex_lock(&hw_mgr->ctx_data[ctx_id].ctx_mutex);
	hw_mgr->ctx_data[ctx_id].in_use = 0;
	hw_mgr->ctx_data[ctx_id].fw_handle = 0;
	hw_mgr->ctx_data[ctx_id].scratch_mem_size = 0;
	mutex_lock(&hw_mgr->ctx_data[ctx_id].hfi_frame_process.lock);
	for (i = 0; i < CAM_FRAME_CMD_MAX; i++)
		clear_bit(i, hw_mgr->ctx_data[ctx_id].hfi_frame_process.bitmap);
	mutex_unlock(&hw_mgr->ctx_data[ctx_id].hfi_frame_process.lock);
	mutex_destroy(&hw_mgr->ctx_data[ctx_id].hfi_frame_process.lock);
	mutex_unlock(&hw_mgr->ctx_data[ctx_id].ctx_mutex);
	kfree(hw_mgr->ctx_data[ctx_id].hfi_frame_process.bitmap);

	return 0;
}

static int cam_icp_mgr_get_ctx_from_fw_handle(struct cam_icp_hw_mgr *hw_mgr,
							uint32_t fw_handle)
{
	int ctx_id;

	for (ctx_id = 0; ctx_id < CAM_ICP_CTX_MAX; ctx_id++) {
		mutex_lock(&hw_mgr->ctx_data[ctx_id].ctx_mutex);
		if (hw_mgr->ctx_data[ctx_id].in_use) {
			if (hw_mgr->ctx_data[ctx_id].fw_handle == fw_handle) {
				mutex_unlock(
					&hw_mgr->ctx_data[ctx_id].ctx_mutex);
				return ctx_id;
			}
		}
		mutex_unlock(&hw_mgr->ctx_data[ctx_id].ctx_mutex);
	}
	ICP_DBG("Invalid fw handle to get ctx\n");

	return -EINVAL;
}

static int cam_icp_mgr_hw_close(void *hw_priv, void *hw_close_args)
{
	struct cam_icp_hw_mgr *hw_mgr = hw_priv;
	struct cam_hw_intf *a5_dev_intf = NULL;
	struct cam_hw_intf *ipe0_dev_intf = NULL;
	struct cam_hw_intf *ipe1_dev_intf = NULL;
	struct cam_hw_intf *bps_dev_intf = NULL;
	int rc = 0;

	a5_dev_intf = hw_mgr->devices[CAM_ICP_DEV_A5][0];
	ipe0_dev_intf = hw_mgr->devices[CAM_ICP_DEV_IPE][0];
	bps_dev_intf = hw_mgr->devices[CAM_ICP_DEV_BPS][0];

	if ((!a5_dev_intf) || (!ipe0_dev_intf) || (!bps_dev_intf)) {
		pr_err("dev intfs are wrong\n");
		return rc;
	}
	mutex_lock(&hw_mgr->hw_mgr_mutex);
	rc = a5_dev_intf->hw_ops.deinit(a5_dev_intf->hw_priv, NULL, 0);
	if (rc < 0)
		pr_err("a5 dev de-init failed\n");

	rc = bps_dev_intf->hw_ops.deinit(bps_dev_intf->hw_priv, NULL, 0);
	if (rc < 0)
		pr_err("bps dev de-init failed\n");

	rc = ipe0_dev_intf->hw_ops.deinit(ipe0_dev_intf->hw_priv, NULL, 0);
	if (rc < 0)
		pr_err("ipe0 dev de-init failed\n");

	ipe1_dev_intf = hw_mgr->devices[CAM_ICP_DEV_IPE][1];
	if (ipe1_dev_intf) {
		rc = ipe1_dev_intf->hw_ops.deinit(ipe1_dev_intf->hw_priv,
						NULL, 0);
		if (rc < 0)
			pr_err("ipe1 dev de-init failed\n");
	}

	cam_icp_free_hfi_mem();
	hw_mgr->fw_download = false;
	debugfs_remove_recursive(icp_hw_mgr.dentry);
	mutex_unlock(&hw_mgr->hw_mgr_mutex);

	return 0;
}

static int cam_icp_mgr_download_fw(void *hw_mgr_priv, void *download_fw_args)
{
	struct cam_hw_intf *a5_dev_intf = NULL;
	struct cam_hw_intf *ipe0_dev_intf = NULL;
	struct cam_hw_intf *ipe1_dev_intf = NULL;
	struct cam_hw_intf *bps_dev_intf = NULL;
	struct cam_hw_info *a5_dev = NULL;
	struct cam_icp_hw_mgr *hw_mgr = hw_mgr_priv;
	struct cam_icp_a5_set_irq_cb irq_cb;
	struct cam_icp_a5_set_fw_buf_info fw_buf_info;
	struct hfi_mem_info hfi_mem;
	unsigned long rem_jiffies;
	int timeout = 5000;
	int rc = 0;

	if (!hw_mgr) {
		pr_err("hw_mgr is NULL\n");
		return -EINVAL;
	}

	mutex_lock(&hw_mgr->hw_mgr_mutex);
	if (hw_mgr->fw_download) {
		ICP_DBG("FW already downloaded\n");
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		return rc;
	}

	/* Allocate memory for FW and shared memory */
	rc = cam_icp_allocate_hfi_mem();
	if (rc < 0) {
		pr_err("hfi mem alloc failed\n");
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		return rc;
	}

	a5_dev_intf = hw_mgr->devices[CAM_ICP_DEV_A5][0];
	ipe0_dev_intf = hw_mgr->devices[CAM_ICP_DEV_IPE][0];
	ipe1_dev_intf = hw_mgr->devices[CAM_ICP_DEV_IPE][1];
	bps_dev_intf = hw_mgr->devices[CAM_ICP_DEV_BPS][0];

	if ((!a5_dev_intf) || (!ipe0_dev_intf) || (!bps_dev_intf)) {
		pr_err("dev intfs are wrong\n");
		goto dev_intf_fail;
	}

	a5_dev = (struct cam_hw_info *)a5_dev_intf->hw_priv;

	rc = a5_dev_intf->hw_ops.init(a5_dev_intf->hw_priv, NULL, 0);
	if (rc < 0) {
		pr_err("a5 dev init failed\n");
		goto a5_dev_init_failed;
	}
	rc = bps_dev_intf->hw_ops.init(bps_dev_intf->hw_priv, NULL, 0);
	if (rc < 0) {
		pr_err("bps dev init failed\n");
		goto bps_dev_init_failed;
	}
	rc = ipe0_dev_intf->hw_ops.init(ipe0_dev_intf->hw_priv, NULL, 0);
	if (rc < 0) {
		pr_err("ipe0 dev init failed\n");
		goto ipe0_dev_init_failed;
	}

	if (ipe1_dev_intf) {
		rc = ipe1_dev_intf->hw_ops.init(ipe1_dev_intf->hw_priv,
						NULL, 0);
		if (rc < 0) {
			pr_err("ipe1 dev init failed\n");
			goto ipe1_dev_init_failed;
		}
	}
	/* Set IRQ callback */
	irq_cb.icp_hw_mgr_cb = cam_icp_hw_mgr_cb;
	irq_cb.data = hw_mgr_priv;
	rc = a5_dev_intf->hw_ops.process_cmd(
				a5_dev_intf->hw_priv,
				CAM_ICP_A5_SET_IRQ_CB,
				&irq_cb, sizeof(irq_cb));
	if (rc < 0) {
		pr_err("CAM_ICP_A5_SET_IRQ_CB failed\n");
		rc = -EINVAL;
		goto set_irq_failed;
	}

	fw_buf_info.kva = icp_hw_mgr.hfi_mem.fw_buf.kva;
	fw_buf_info.iova = icp_hw_mgr.hfi_mem.fw_buf.iova;
	fw_buf_info.len = icp_hw_mgr.hfi_mem.fw_buf.len;

	rc = a5_dev_intf->hw_ops.process_cmd(
			a5_dev_intf->hw_priv,
			CAM_ICP_A5_CMD_SET_FW_BUF,
			&fw_buf_info,
			sizeof(fw_buf_info));
	if (rc < 0) {
		pr_err("CAM_ICP_A5_CMD_SET_FW_BUF failed\n");
		goto set_irq_failed;
	}

	cam_hfi_enable_cpu(a5_dev->soc_info.reg_map[A5_SIERRA_BASE].mem_base);

	rc = a5_dev_intf->hw_ops.process_cmd(
			a5_dev_intf->hw_priv,
			CAM_ICP_A5_CMD_FW_DOWNLOAD,
			NULL, 0);
	if (rc < 0) {
		pr_err("FW download is failed\n");
		goto set_irq_failed;
	}

	hfi_mem.qtbl.kva = icp_hw_mgr.hfi_mem.qtbl.kva;
	hfi_mem.qtbl.iova = icp_hw_mgr.hfi_mem.qtbl.iova;
	hfi_mem.qtbl.len = icp_hw_mgr.hfi_mem.qtbl.len;
	ICP_DBG("kva = %llX\n", hfi_mem.qtbl.kva);
	ICP_DBG("IOVA = %X\n", hfi_mem.qtbl.iova);
	ICP_DBG("length = %lld\n", hfi_mem.qtbl.len);

	hfi_mem.cmd_q.kva = icp_hw_mgr.hfi_mem.cmd_q.kva;
	hfi_mem.cmd_q.iova = icp_hw_mgr.hfi_mem.cmd_q.iova;
	hfi_mem.cmd_q.len = icp_hw_mgr.hfi_mem.cmd_q.len;
	ICP_DBG("kva = %llX\n", hfi_mem.cmd_q.kva);
	ICP_DBG("IOVA = %X\n", hfi_mem.cmd_q.iova);
	ICP_DBG("length = %lld\n", hfi_mem.cmd_q.len);

	hfi_mem.msg_q.kva = icp_hw_mgr.hfi_mem.msg_q.kva;
	hfi_mem.msg_q.iova = icp_hw_mgr.hfi_mem.msg_q.iova;
	hfi_mem.msg_q.len = icp_hw_mgr.hfi_mem.msg_q.len;
	ICP_DBG("kva = %llX\n", hfi_mem.msg_q.kva);
	ICP_DBG("IOVA = %X\n", hfi_mem.msg_q.iova);
	ICP_DBG("length = %lld\n", hfi_mem.msg_q.len);

	hfi_mem.dbg_q.kva = icp_hw_mgr.hfi_mem.dbg_q.kva;
	hfi_mem.dbg_q.iova = icp_hw_mgr.hfi_mem.dbg_q.iova;
	hfi_mem.dbg_q.len = icp_hw_mgr.hfi_mem.dbg_q.len;
	ICP_DBG("kva = %llX\n", hfi_mem.dbg_q.kva);
	ICP_DBG("IOVA = %X\n",  hfi_mem.dbg_q.iova);
	ICP_DBG("length = %lld\n", hfi_mem.dbg_q.len);

	hfi_mem.sec_heap.kva = icp_hw_mgr.hfi_mem.sec_heap.kva;
	hfi_mem.sec_heap.iova = icp_hw_mgr.hfi_mem.sec_heap.iova;
	hfi_mem.sec_heap.len = icp_hw_mgr.hfi_mem.sec_heap.len;

	hfi_mem.shmem.iova = icp_hw_mgr.hfi_mem.shmem.iova_start;
	hfi_mem.shmem.len = icp_hw_mgr.hfi_mem.shmem.iova_len;

	rc = cam_hfi_init(0, &hfi_mem,
		a5_dev->soc_info.reg_map[A5_SIERRA_BASE].mem_base,
		hw_mgr->a5_debug);
	if (rc < 0) {
		pr_err("hfi_init is failed\n");
		goto set_irq_failed;
	}

	mutex_unlock(&hw_mgr->hw_mgr_mutex);

	ICP_DBG("Sending HFI init command\n");
	reinit_completion(&hw_mgr->a5_complete);

	rc = a5_dev_intf->hw_ops.process_cmd(
		a5_dev_intf->hw_priv,
		CAM_ICP_A5_SEND_INIT,
		NULL, 0);

	ICP_DBG("Wait for INIT DONE Message\n");
	rem_jiffies = wait_for_completion_timeout(&icp_hw_mgr.a5_complete,
			msecs_to_jiffies((timeout)));
	if (!rem_jiffies) {
		rc = -ETIMEDOUT;
		pr_err("FW response timed out %d\n", rc);
		goto set_irq_failed;
	}

	ICP_DBG("Done Waiting for INIT DONE Message\n");

	rc = a5_dev_intf->hw_ops.process_cmd(
		a5_dev_intf->hw_priv,
		CAM_ICP_A5_CMD_POWER_COLLAPSE,
		NULL, 0);
	if (rc) {
		pr_err("icp power collapse failed\n");
		goto set_irq_failed;
	}

	hw_mgr->fw_download = true;

	rc = cam_icp_stop_cpas(hw_mgr);
	if (rc) {
		pr_err("cpas stop failed\n");
		goto set_irq_failed;
	}

	hw_mgr->ctxt_cnt = 0;

	return rc;

set_irq_failed:
	if (ipe1_dev_intf)
		rc = ipe1_dev_intf->hw_ops.deinit(ipe1_dev_intf->hw_priv,
			NULL, 0);
ipe1_dev_init_failed:
	rc = ipe0_dev_intf->hw_ops.deinit(ipe0_dev_intf->hw_priv, NULL, 0);
ipe0_dev_init_failed:
	rc = bps_dev_intf->hw_ops.deinit(bps_dev_intf->hw_priv, NULL, 0);
bps_dev_init_failed:
	rc = a5_dev_intf->hw_ops.deinit(a5_dev_intf->hw_priv, NULL, 0);
a5_dev_init_failed:
dev_intf_fail:
	cam_icp_free_hfi_mem();
	mutex_unlock(&hw_mgr->hw_mgr_mutex);
	return rc;
}

static int cam_icp_mgr_config_hw(void *hw_mgr_priv, void *config_hw_args)
{
	int rc = 0;
	struct cam_icp_hw_mgr *hw_mgr = hw_mgr_priv;
	struct cam_hw_config_args *config_args = config_hw_args;
	uint32_t fw_handle;
	int ctx_id = 0;
	struct cam_icp_hw_ctx_data *ctx_data = NULL;
	int32_t request_id = 0;
	struct cam_hw_update_entry *hw_update_entries;
	struct crm_workq_task *task;
	struct hfi_cmd_work_data *task_data;
	struct hfi_cmd_ipebps_async *hfi_cmd;

	if (!hw_mgr || !config_args) {
		pr_err("Invalid arguments %pK %pK\n",
			hw_mgr, config_args);
		return -EINVAL;
	}

	if (!config_args->num_hw_update_entries) {
		pr_err("No hw update enteries are available\n");
		return -EINVAL;
	}

	mutex_lock(&hw_mgr->hw_mgr_mutex);
	fw_handle = *(uint32_t *)config_args->ctxt_to_hw_map;
	ctx_id = cam_icp_mgr_get_ctx_from_fw_handle(hw_mgr, fw_handle);
	if (ctx_id < 0) {
		pr_err("Fw handle to ctx mapping is failed\n");
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		return -EINVAL;
	}

	ctx_data = &hw_mgr->ctx_data[ctx_id];
	if (!ctx_data->in_use) {
		pr_err("ctx is not in use\n");
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		return -EINVAL;
	}

	request_id = *(uint32_t *)config_args->priv;
	hw_update_entries = config_args->hw_update_entries;
	ICP_DBG("req_id = %d\n", request_id);
	ICP_DBG("fw_handle = %x req_id = %d %pK\n",
		fw_handle, request_id, config_args->priv);
	task = cam_req_mgr_workq_get_task(icp_hw_mgr.cmd_work);
	if (!task) {
		pr_err("no empty task\n");
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		return -ENOMEM;
	}

	mutex_unlock(&hw_mgr->hw_mgr_mutex);

	task_data = (struct hfi_cmd_work_data *)task->payload;
	if (!task_data) {
		pr_err("task_data is NULL\n");
		return -EINVAL;
	}

	task_data->data = (void *)hw_update_entries->addr;
	hfi_cmd = (struct hfi_cmd_ipebps_async *)hw_update_entries->addr;
	ICP_DBG("request from hfi_cmd :%llu, hfi_cmd: %pK\n",
		hfi_cmd->user_data2, hfi_cmd);
	task_data->request_id = request_id;
	task_data->type = ICP_WORKQ_TASK_CMD_TYPE;
	task->process_cb = cam_icp_mgr_process_cmd;
	rc = cam_req_mgr_workq_enqueue_task(task, &icp_hw_mgr,
			CRM_TASK_PRIORITY_0);
	return rc;
}

static int cam_icp_mgr_prepare_frame_process_cmd(
			struct cam_icp_hw_ctx_data *ctx_data,
			struct hfi_cmd_ipebps_async *hfi_cmd,
			uint32_t request_id,
			uint32_t fw_cmd_buf_iova_addr)
{
	hfi_cmd->size = sizeof(struct hfi_cmd_ipebps_async);
	hfi_cmd->pkt_type = HFI_CMD_IPEBPS_ASYNC_COMMAND_INDIRECT;
	if (ctx_data->icp_dev_acquire_info.dev_type == CAM_ICP_RES_TYPE_BPS)
		hfi_cmd->opcode = HFI_IPEBPS_CMD_OPCODE_BPS_FRAME_PROCESS;
	else
		hfi_cmd->opcode = HFI_IPEBPS_CMD_OPCODE_IPE_FRAME_PROCESS;
	hfi_cmd->num_fw_handles = 1;
	hfi_cmd->fw_handles[0] = ctx_data->fw_handle;
	hfi_cmd->payload.indirect = fw_cmd_buf_iova_addr;
	hfi_cmd->user_data1 = (uint64_t)ctx_data;
	hfi_cmd->user_data2 = request_id;

	ICP_DBG("ctx_data : %pK, request_id :%d cmd_buf %x\n",
		(void *)ctx_data->context_priv,
		request_id, fw_cmd_buf_iova_addr);

	return 0;
}

static int cam_icp_mgr_prepare_hw_update(void *hw_mgr_priv,
				void *prepare_hw_update_args)
{
	int        rc = 0, i, j;
	int        ctx_id = 0;
	uint32_t   fw_handle;
	int32_t    idx;
	uint64_t   iova_addr;
	uint32_t   fw_cmd_buf_iova_addr;
	size_t     fw_cmd_buf_len;
	int32_t    sync_in_obj[CAM_ICP_IPE_IMAGE_MAX];
	int32_t    merged_sync_in_obj;


	struct cam_hw_prepare_update_args *prepare_args =
		prepare_hw_update_args;
	struct cam_icp_hw_mgr *hw_mgr = hw_mgr_priv;
	struct cam_icp_hw_ctx_data *ctx_data = NULL;
	struct cam_packet *packet = NULL;
	struct cam_cmd_buf_desc *cmd_desc = NULL;
	struct cam_buf_io_cfg *io_cfg_ptr = NULL;
	struct hfi_cmd_ipebps_async *hfi_cmd = NULL;

	if ((!prepare_args) || (!hw_mgr)) {
		pr_err("Invalid args\n");
		return -EINVAL;
	}

	mutex_lock(&hw_mgr->hw_mgr_mutex);
	fw_handle = *(uint32_t *)prepare_args->ctxt_to_hw_map;
	ctx_id = cam_icp_mgr_get_ctx_from_fw_handle(hw_mgr, fw_handle);
	if (ctx_id < 0) {
		pr_err("Fw handle to ctx mapping is failed\n");
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		return -EINVAL;
	}
	mutex_unlock(&hw_mgr->hw_mgr_mutex);

	ctx_data = &hw_mgr->ctx_data[ctx_id];
	if (!ctx_data->in_use) {
		pr_err("ctx is not in use\n");
		return -EINVAL;
	}

	packet = prepare_args->packet;
	if (!packet) {
		pr_err("received packet is NULL\n");
		return -EINVAL;
	}

	ICP_DBG("packet header : opcode = %x size = %x",
			packet->header.op_code,
			packet->header.size);

	ICP_DBG(" req_id = %x flags = %x\n",
			(uint32_t)packet->header.request_id,
			packet->header.flags);

	ICP_DBG("packet data : c_off = %x c_num = %x\n",
			packet->cmd_buf_offset,
			packet->num_cmd_buf);

	ICP_DBG("io_off = %x io_num = %x p_off = %x p_num = %x %x %x\n",
			packet->io_configs_offset,
			packet->num_io_configs, packet->patch_offset,
			packet->num_patches, packet->kmd_cmd_buf_index,
			packet->kmd_cmd_buf_offset);

	if (((packet->header.op_code & 0xff) !=
		CAM_ICP_OPCODE_IPE_UPDATE) &&
		((packet->header.op_code & 0xff) !=
		CAM_ICP_OPCODE_BPS_UPDATE)) {
		pr_err("Invalid Opcode in pkt: %d\n",
			packet->header.op_code & 0xff);
		return -EINVAL;
	}

	if ((packet->num_cmd_buf > 1) || (!packet->num_patches) ||
					(!packet->num_io_configs)) {
		pr_err("wrong number of cmd/patch info: %u %u\n",
				packet->num_cmd_buf,
				packet->num_patches);
		return -EINVAL;
	}

	/* process command buffer descriptors */
	cmd_desc = (struct cam_cmd_buf_desc *)
			((uint32_t *) &packet->payload +
				packet->cmd_buf_offset/4);
	ICP_DBG("packet = %pK cmd_desc = %pK size = %lu\n",
			(void *)packet, (void *)cmd_desc,
			sizeof(struct cam_cmd_buf_desc));

	rc = cam_mem_get_io_buf(cmd_desc->mem_handle,
		hw_mgr->iommu_hdl, &iova_addr, &fw_cmd_buf_len);
	if (rc < 0) {
		pr_err("unable to get src buf info for cmd buf: %x\n",
						hw_mgr->iommu_hdl);
		return rc;
	}
	ICP_DBG("cmd_buf desc cpu and iova address: %pK %zu\n",
				(void *)iova_addr, fw_cmd_buf_len);
	fw_cmd_buf_iova_addr = iova_addr;
	fw_cmd_buf_iova_addr = (fw_cmd_buf_iova_addr + cmd_desc->offset);

	/* Update Buffer Address from handles and patch information */
	rc = cam_packet_util_process_patches(packet, hw_mgr->iommu_hdl);
	if (rc) {
		pr_err("Patch processing failed\n");
		return rc;
	}

	/* process io config out descriptors */
	io_cfg_ptr = (struct cam_buf_io_cfg *) ((uint32_t *) &packet->payload +
				packet->io_configs_offset/4);
	ICP_DBG("packet = %pK io_cfg_ptr = %pK size = %lu\n",
			(void *)packet, (void *)io_cfg_ptr,
			sizeof(struct cam_buf_io_cfg));

	prepare_args->num_out_map_entries = 0;
	for (i = 0, j = 0; i < packet->num_io_configs; i++) {
		if (io_cfg_ptr[i].direction == CAM_BUF_INPUT) {
			ICP_DBG("direction is i : %d :%u\n",
					i, io_cfg_ptr[i].direction);
			ICP_DBG("fence is i : %d :%d\n",
					i, io_cfg_ptr[i].fence);
			continue;
		}

		prepare_args->out_map_entries[j].sync_id = io_cfg_ptr[i].fence;
		prepare_args->out_map_entries[j++].resource_handle =
							io_cfg_ptr[i].fence;
		prepare_args->num_out_map_entries++;
		ICP_DBG(" out fence = %x index = %d\n", io_cfg_ptr[i].fence, i);
	}
	ICP_DBG("out buf entries processing is done\n");

	/* process io config in descriptors */
	for (i = 0, j = 0; i < packet->num_io_configs; i++) {
		if (io_cfg_ptr[i].direction == CAM_BUF_INPUT) {
			sync_in_obj[j++] = io_cfg_ptr[i].fence;
			ICP_DBG(" in fence = %x index = %d\n",
					io_cfg_ptr[i].fence, i);
		}
	}

	if (j == 1)
		merged_sync_in_obj = sync_in_obj[j - 1];
	else if (j > 1) {
		rc = cam_sync_merge(&sync_in_obj[0], j, &merged_sync_in_obj);
		if (rc < 0) {
			pr_err("unable to create in merged object: %d\n",
								rc);
			return rc;
		}
	} else {
		pr_err("no input fence provided %u\n", j);
		return -EINVAL;
	}

	prepare_args->in_map_entries[0].sync_id = merged_sync_in_obj;
	prepare_args->in_map_entries[0].resource_handle =
			ctx_data->icp_dev_acquire_info.dev_type;
	prepare_args->num_in_map_entries = 1;
	ICP_DBG("out buf entries processing is done\n");

	mutex_lock(&ctx_data->hfi_frame_process.lock);
	idx = find_first_zero_bit(ctx_data->hfi_frame_process.bitmap,
			ctx_data->hfi_frame_process.bits);
	if (idx < 0 || idx >= CAM_FRAME_CMD_MAX) {
		pr_err("request idx is wrong: %d\n", idx);
		mutex_unlock(&ctx_data->hfi_frame_process.lock);
		return -EINVAL;
	}
	set_bit(idx, ctx_data->hfi_frame_process.bitmap);
	mutex_unlock(&ctx_data->hfi_frame_process.lock);

	ctx_data->hfi_frame_process.request_id[idx] = packet->header.request_id;
	ICP_DBG("slot[%d]: %d\n", idx,
		ctx_data->hfi_frame_process.request_id[idx]);
	ctx_data->hfi_frame_process.num_out_resources[idx] =
				prepare_args->num_out_map_entries;
	for (i = 0; i < prepare_args->num_out_map_entries; i++)
		ctx_data->hfi_frame_process.out_resource[idx][i] =
			prepare_args->out_map_entries[i].resource_handle;

	hfi_cmd = (struct hfi_cmd_ipebps_async *)
			&ctx_data->hfi_frame_process.hfi_frame_cmd[idx];

	cam_icp_mgr_prepare_frame_process_cmd(
			ctx_data, hfi_cmd, packet->header.request_id,
			fw_cmd_buf_iova_addr);

	prepare_args->num_hw_update_entries = 1;
	prepare_args->hw_update_entries[0].addr = (uint64_t)hfi_cmd;

	prepare_args->priv = &ctx_data->hfi_frame_process.request_id[idx];

	ICP_DBG("slot : %d, hfi_cmd : %pK, request : %d\n",	idx,
		(void *)hfi_cmd,
		ctx_data->hfi_frame_process.request_id[idx]);

	return rc;
}

static int cam_icp_mgr_release_hw(void *hw_mgr_priv, void *release_hw_args)
{
	int rc = 0;
	int ctx_id = 0;
	int i;
	uint32_t fw_handle;
	struct cam_hw_release_args *release_hw = release_hw_args;
	struct cam_icp_hw_mgr *hw_mgr = hw_mgr_priv;
	struct cam_icp_hw_ctx_data *ctx_data = NULL;

	if (!release_hw || !hw_mgr) {
		pr_err("Invalid args\n");
		return -EINVAL;
	}

	for (i = 0; i < CAM_ICP_CTX_MAX; i++) {
		ctx_data = &hw_mgr->ctx_data[i];
		ICP_DBG("i = %d in_use = %u fw_handle = %u\n", i,
				ctx_data->in_use, ctx_data->fw_handle);
	}

	mutex_lock(&hw_mgr->hw_mgr_mutex);
	fw_handle = *(uint32_t *)release_hw->ctxt_to_hw_map;
	ctx_id = cam_icp_mgr_get_ctx_from_fw_handle(hw_mgr, fw_handle);
	if (ctx_id < 0) {
		pr_err("Invalid ctx id\n");
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		return -EINVAL;
	}

	rc = cam_icp_mgr_release_ctx(hw_mgr, ctx_id);
	if (rc) {
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		return -EINVAL;
	}

	--hw_mgr->ctxt_cnt;
	if (!hw_mgr->ctxt_cnt) {
		ICP_DBG("stop cpas for last context\n");
		cam_icp_stop_cpas(hw_mgr);
	}
	ICP_DBG("context count : %u\n", hw_mgr->ctxt_cnt);
	mutex_unlock(&hw_mgr->hw_mgr_mutex);

	ICP_DBG("fw handle %d\n", fw_handle);
	return rc;
}

static int cam_icp_mgr_send_config_io(struct cam_icp_hw_ctx_data *ctx_data,
			struct crm_workq_task *task, uint32_t io_buf_addr)
{
	int rc = 0;
	struct hfi_cmd_work_data *task_data;
	struct hfi_cmd_ipebps_async ioconfig_cmd;
	unsigned long rem_jiffies;
	int timeout = 5000;

	ioconfig_cmd.size = sizeof(struct hfi_cmd_ipebps_async);
	ioconfig_cmd.pkt_type = HFI_CMD_IPEBPS_ASYNC_COMMAND_INDIRECT;
	if (ctx_data->icp_dev_acquire_info.dev_type == CAM_ICP_RES_TYPE_BPS)
		ioconfig_cmd.opcode = HFI_IPEBPS_CMD_OPCODE_BPS_CONFIG_IO;
	else
		ioconfig_cmd.opcode = HFI_IPEBPS_CMD_OPCODE_IPE_CONFIG_IO;

	reinit_completion(&ctx_data->wait_complete);
	ICP_DBG("Sending HFI_CMD_IPEBPS_ASYNC_COMMAND: opcode :%u\n",
						ioconfig_cmd.opcode);
	ioconfig_cmd.num_fw_handles = 1;
	ioconfig_cmd.fw_handles[0] = ctx_data->fw_handle;
	ioconfig_cmd.payload.indirect = io_buf_addr;
	ioconfig_cmd.user_data1 = (uint64_t)ctx_data;
	ioconfig_cmd.user_data2 = (uint64_t)0x0;
	task_data = (struct hfi_cmd_work_data *)task->payload;
	task_data->data = (void *)&ioconfig_cmd;
	task_data->request_id = 0;
	task_data->type = ICP_WORKQ_TASK_CMD_TYPE;
	task->process_cb = cam_icp_mgr_process_cmd;
	cam_req_mgr_workq_enqueue_task(task, &icp_hw_mgr, CRM_TASK_PRIORITY_0);
	ICP_DBG("fw_hdl = %x ctx_data = %pK\n", ctx_data->fw_handle, ctx_data);

	rem_jiffies = wait_for_completion_timeout(&ctx_data->wait_complete,
			msecs_to_jiffies((timeout)));
	if (!rem_jiffies) {
		rc = -ETIMEDOUT;
		pr_err("FW response timed out %d\n", rc);
	}

	return rc;
}

static int cam_icp_mgr_create_handle(uint32_t dev_type,
	struct cam_icp_hw_ctx_data *ctx_data,
	struct crm_workq_task *task)
{
	struct hfi_cmd_create_handle create_handle;
	struct hfi_cmd_work_data *task_data;
	unsigned long rem_jiffies;
	int timeout = 5000;
	int rc = 0;

	create_handle.size = sizeof(struct hfi_cmd_create_handle);
	create_handle.pkt_type = HFI_CMD_IPEBPS_CREATE_HANDLE;
	create_handle.handle_type = dev_type;
	create_handle.user_data1 = (uint64_t)ctx_data;
	ICP_DBG("%x %x %x %pK\n", create_handle.size,	create_handle.pkt_type,
		create_handle.handle_type, (void *)create_handle.user_data1);
	ICP_DBG("Sending HFI_CMD_IPEBPS_CREATE_HANDLE\n");

	reinit_completion(&ctx_data->wait_complete);
	task_data = (struct hfi_cmd_work_data *)task->payload;
	task_data->data = (void *)&create_handle;
	task_data->request_id = 0;
	task_data->type = ICP_WORKQ_TASK_CMD_TYPE;
	task->process_cb = cam_icp_mgr_process_cmd;
	cam_req_mgr_workq_enqueue_task(task, &icp_hw_mgr, CRM_TASK_PRIORITY_0);

	rem_jiffies = wait_for_completion_timeout(&ctx_data->wait_complete,
			msecs_to_jiffies((timeout)));
	if (!rem_jiffies) {
		rc = -ETIMEDOUT;
		pr_err("FW response timed out %d\n", rc);
	}

	return rc;
}

static int cam_icp_mgr_send_ping(struct cam_icp_hw_ctx_data *ctx_data,
	struct crm_workq_task *task)
{
	struct hfi_cmd_ping_pkt ping_pkt;
	struct hfi_cmd_work_data *task_data;
	unsigned long rem_jiffies;
	int timeout = 5000;
	int rc = 0;

	ping_pkt.size = sizeof(struct hfi_cmd_ping_pkt);
	ping_pkt.pkt_type = HFI_CMD_SYS_PING;
	ping_pkt.user_data = (uint64_t)ctx_data;
	ICP_DBG("Sending HFI_CMD_SYS_PING\n");
	ICP_DBG("%x %x %pK\n", ping_pkt.size,	ping_pkt.pkt_type,
		(void *)ping_pkt.user_data);

	init_completion(&ctx_data->wait_complete);
	task_data = (struct hfi_cmd_work_data *)task->payload;
	task_data->data = (void *)&ping_pkt;
	task_data->request_id = 0;
	task_data->type = ICP_WORKQ_TASK_CMD_TYPE;
	task->process_cb = cam_icp_mgr_process_cmd;
	cam_req_mgr_workq_enqueue_task(task, &icp_hw_mgr, CRM_TASK_PRIORITY_0);

	rem_jiffies = wait_for_completion_timeout(&ctx_data->wait_complete,
			msecs_to_jiffies((timeout)));
	if (!rem_jiffies) {
		rc = -ETIMEDOUT;
		pr_err("FW response timed out %d\n", rc);
	}


	return rc;
}

static int cam_icp_mgr_acquire_hw(void *hw_mgr_priv, void *acquire_hw_args)
{
	int rc = 0, i, bitmap_size = 0, tmp_size;
	uint32_t ctx_id = 0;
	uint64_t io_buf_addr;
	size_t io_buf_size;
	struct cam_icp_hw_mgr *hw_mgr = hw_mgr_priv;
	struct cam_icp_hw_ctx_data *ctx_data = NULL;
	struct cam_hw_acquire_args *args = acquire_hw_args;
	struct cam_icp_acquire_dev_info icp_dev_acquire_info;
	struct cam_icp_res_info *p_icp_out = NULL;
	struct crm_workq_task *task;
	uint8_t *tmp_acquire;

	if ((!hw_mgr_priv) || (!acquire_hw_args)) {
		pr_err("Invalid params: %pK %pK\n", hw_mgr_priv,
			acquire_hw_args);
		return -EINVAL;
	}

	if (args->num_acq > 1) {
		pr_err("number of resources are wrong: %u\n", args->num_acq);
		return -EINVAL;
	}

	if (copy_from_user(&icp_dev_acquire_info,
			(void __user *)args->acquire_info,
			sizeof(icp_dev_acquire_info)))
		return -EFAULT;

	if (icp_dev_acquire_info.num_out_res > ICP_IPE_MAX_OUTPUT_SUPPORTED) {
		pr_err("num of out resources exceeding : %u\n",
			icp_dev_acquire_info.num_out_res);
		return -EINVAL;
	}

	ICP_DBG("%x %x %x %x %x %x %x\n",
		icp_dev_acquire_info.dev_type,
		icp_dev_acquire_info.in_res.format,
		icp_dev_acquire_info.in_res.width,
		icp_dev_acquire_info.in_res.height,
		icp_dev_acquire_info.in_res.fps,
		icp_dev_acquire_info.num_out_res,
		icp_dev_acquire_info.scratch_mem_size);

	tmp_size = sizeof(icp_dev_acquire_info) +
			icp_dev_acquire_info.num_out_res *
			sizeof(struct cam_icp_res_info);

	tmp_acquire = kzalloc(tmp_size, GFP_KERNEL);
	if (!tmp_acquire)
		return -EINVAL;

	if (copy_from_user(tmp_acquire,
			(void __user *)args->acquire_info,
			tmp_size)) {
		kfree(tmp_acquire);
		return -EFAULT;
	}

	p_icp_out =
		(struct cam_icp_res_info *)(tmp_acquire +
		sizeof(icp_dev_acquire_info)-
		sizeof(struct cam_icp_res_info));
	ICP_DBG("out[0] %x %x %x %x\n",
		p_icp_out[0].format,
		p_icp_out[0].width,
		p_icp_out[0].height,
		p_icp_out[0].fps);

	ICP_DBG("out[1] %x %x %x %x\n",
		p_icp_out[1].format,
		p_icp_out[1].width,
		p_icp_out[1].height,
		p_icp_out[1].fps);

	mutex_lock(&hw_mgr->hw_mgr_mutex);
	ctx_id = cam_icp_mgr_get_free_ctx(hw_mgr);
	if (ctx_id >= CAM_ICP_CTX_MAX) {
		pr_err("No free ctx space in hw_mgr\n");
		kfree(tmp_acquire);
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		return -EFAULT;
	}

	/* Fill ctx with acquire info */
	ctx_data = &hw_mgr->ctx_data[ctx_id];

	if (!hw_mgr->ctxt_cnt++) {
		ICP_DBG("starting cpas\n");
		cam_icp_start_cpas(hw_mgr);
	}
	ICP_DBG("context count : %u\n", hw_mgr->ctxt_cnt);

	mutex_unlock(&hw_mgr->hw_mgr_mutex);

	/* Fill ctx with acquire info */
	mutex_lock(&ctx_data->ctx_mutex);
	ctx_data->icp_dev_acquire_info = icp_dev_acquire_info;
	for (i = 0; i < icp_dev_acquire_info.num_out_res; i++)
		ctx_data->icp_out_acquire_info[i] = p_icp_out[i];
	mutex_unlock(&ctx_data->ctx_mutex);

	/* Get IOCONFIG command info */
	if (ctx_data->icp_dev_acquire_info.secure_mode)
		rc = cam_mem_get_io_buf(
			icp_dev_acquire_info.io_config_cmd_handle,
			hw_mgr->iommu_sec_hdl,
			&io_buf_addr, &io_buf_size);
	else
		rc = cam_mem_get_io_buf(
			icp_dev_acquire_info.io_config_cmd_handle,
			hw_mgr->iommu_hdl,
			&io_buf_addr, &io_buf_size);

	ICP_DBG("io_config_cmd_handle : %d\n",
		icp_dev_acquire_info.io_config_cmd_handle);
	ICP_DBG("io_buf_addr : %pK\n", (void *)io_buf_addr);
	ICP_DBG("io_buf_size : %zu\n", io_buf_size);
	if (rc < 0) {
		pr_err("unable to get src buf info from io desc\n");
		goto cmd_cpu_buf_failed;
	}

	mutex_lock(&icp_hw_mgr.hw_mgr_mutex);
	task = cam_req_mgr_workq_get_task(icp_hw_mgr.cmd_work);
	if (!task) {
		pr_err("no free task\n");
		mutex_unlock(&icp_hw_mgr.hw_mgr_mutex);
		goto get_create_task_failed;
	}
	mutex_unlock(&icp_hw_mgr.hw_mgr_mutex);

	rc = cam_icp_mgr_send_ping(ctx_data, task);
	if (rc) {
		pr_err("ping ack not received\n");
		goto create_handle_failed;
	}
	mutex_lock(&icp_hw_mgr.hw_mgr_mutex);
	task = cam_req_mgr_workq_get_task(icp_hw_mgr.cmd_work);
	if (!task) {
		pr_err("no free task\n");
		mutex_unlock(&icp_hw_mgr.hw_mgr_mutex);
		goto get_create_task_failed;
	}
	mutex_unlock(&icp_hw_mgr.hw_mgr_mutex);

	/* Send create fw handle command */
	rc = cam_icp_mgr_create_handle(icp_dev_acquire_info.dev_type,
			ctx_data, task);
	if (rc) {
		pr_err("create handle failed\n");
		goto create_handle_failed;
	}

	/* Send IOCONFIG command */
	mutex_lock(&icp_hw_mgr.hw_mgr_mutex);
	task = cam_req_mgr_workq_get_task(icp_hw_mgr.cmd_work);
	if (!task) {
		pr_err("no empty task\n");
		mutex_unlock(&icp_hw_mgr.hw_mgr_mutex);
		goto get_ioconfig_task_failed;
	}
	mutex_unlock(&icp_hw_mgr.hw_mgr_mutex);

	rc = cam_icp_mgr_send_config_io(ctx_data, task, io_buf_addr);
	if (rc) {
		pr_err("IO Config command failed\n");
		goto ioconfig_failed;
	}

	mutex_lock(&ctx_data->ctx_mutex);
	ctx_data->context_priv = args->context_data;
	args->ctxt_to_hw_map = &ctx_data->fw_handle;

	bitmap_size = BITS_TO_LONGS(CAM_FRAME_CMD_MAX) * sizeof(long);
	ctx_data->hfi_frame_process.bitmap =
			kzalloc(sizeof(bitmap_size), GFP_KERNEL);
	ctx_data->hfi_frame_process.bits = bitmap_size * BITS_PER_BYTE;
	mutex_init(&ctx_data->hfi_frame_process.lock);
	mutex_unlock(&ctx_data->ctx_mutex);

	hw_mgr->ctx_data[ctx_id].ctxt_event_cb = args->event_cb;

	icp_dev_acquire_info.scratch_mem_size = ctx_data->scratch_mem_size;
	if (copy_to_user((void __user *)args->acquire_info,
				&icp_dev_acquire_info,
			sizeof(icp_dev_acquire_info)))
		goto copy_to_user_failed;

	ICP_DBG("scratch mem size = %x fw_handle = %x\n",
			(unsigned int)icp_dev_acquire_info.scratch_mem_size,
			(unsigned int)ctx_data->fw_handle);
	kfree(tmp_acquire);
	return 0;

copy_to_user_failed:
ioconfig_failed:
get_ioconfig_task_failed:
	mutex_lock(&icp_hw_mgr.hw_mgr_mutex);
	task = cam_req_mgr_workq_get_task(icp_hw_mgr.cmd_work);
	mutex_unlock(&icp_hw_mgr.hw_mgr_mutex);
	if (task)
		cam_icp_mgr_destroy_handle(ctx_data, task);
create_handle_failed:
get_create_task_failed:
cmd_cpu_buf_failed:
	--hw_mgr->ctxt_cnt;
	if (!hw_mgr->ctxt_cnt)
		cam_icp_stop_cpas(hw_mgr);
	cam_icp_mgr_release_ctx(hw_mgr, ctx_id);
	kfree(tmp_acquire);
	return rc;
}

static int cam_icp_mgr_get_hw_caps(void *hw_mgr_priv, void *hw_caps_args)
{
	int rc = 0;
	struct cam_icp_hw_mgr *hw_mgr = hw_mgr_priv;
	struct cam_query_cap_cmd *query_cap = hw_caps_args;

	if ((!hw_mgr_priv) || (!hw_caps_args)) {
		pr_err("Invalid params: %pK %pK\n", hw_mgr_priv, hw_caps_args);
		return -EINVAL;
	}

	if (copy_from_user(&icp_hw_mgr.icp_caps,
			(void __user *)query_cap->caps_handle,
			sizeof(struct cam_icp_query_cap_cmd))) {
		pr_err("copy_from_user failed\n");
		return -EFAULT;
	}

	mutex_lock(&hw_mgr->hw_mgr_mutex);
	rc = hfi_get_hw_caps(&icp_hw_mgr.icp_caps);
	if (rc < 0) {
		pr_err("Unable to get caps from HFI: %d\n", rc);
		goto hfi_get_caps_fail;
	}

	icp_hw_mgr.icp_caps.dev_iommu_handle.non_secure = hw_mgr->iommu_hdl;
	icp_hw_mgr.icp_caps.dev_iommu_handle.secure = hw_mgr->iommu_sec_hdl;

	if (copy_to_user((void __user *)query_cap->caps_handle,
			&icp_hw_mgr.icp_caps,
			sizeof(struct cam_icp_query_cap_cmd))) {
		pr_err("copy_to_user failed\n");
		rc = -EFAULT;
		goto hfi_get_caps_fail;
	}

hfi_get_caps_fail:
	mutex_unlock(&hw_mgr->hw_mgr_mutex);
	return rc;
}

int cam_icp_hw_mgr_init(struct device_node *of_node, uint64_t *hw_mgr_hdl)
{
	int count, i, rc = 0;
	uint32_t num_dev;
	uint32_t num_ipe_dev;
	const char *name = NULL;
	struct device_node *child_node = NULL;
	struct platform_device *child_pdev = NULL;
	struct cam_hw_intf *child_dev_intf = NULL;
	struct cam_hw_mgr_intf *hw_mgr_intf;


	hw_mgr_intf = (struct cam_hw_mgr_intf *)hw_mgr_hdl;
	if (!of_node || !hw_mgr_intf) {
		pr_err("Invalid args of_node %pK hw_mgr %pK\n",
			of_node, hw_mgr_intf);
		return -EINVAL;
	}

	hw_mgr_intf->hw_mgr_priv = &icp_hw_mgr;
	hw_mgr_intf->hw_get_caps = cam_icp_mgr_get_hw_caps;
	hw_mgr_intf->hw_acquire = cam_icp_mgr_acquire_hw;
	hw_mgr_intf->hw_release = cam_icp_mgr_release_hw;
	hw_mgr_intf->hw_prepare_update = cam_icp_mgr_prepare_hw_update;
	hw_mgr_intf->hw_config = cam_icp_mgr_config_hw;
	hw_mgr_intf->download_fw = cam_icp_mgr_download_fw;
	hw_mgr_intf->hw_close = cam_icp_mgr_hw_close;

	mutex_init(&icp_hw_mgr.hw_mgr_mutex);
	spin_lock_init(&icp_hw_mgr.hw_mgr_lock);

	for (i = 0; i < CAM_ICP_CTX_MAX; i++)
		mutex_init(&icp_hw_mgr.ctx_data[i].ctx_mutex);

	/* Get number of device objects */
	count = of_property_count_strings(of_node, "compat-hw-name");
	if (!count) {
		pr_err("no compat hw found in dev tree, count = %d\n", count);
		rc = -EINVAL;
		goto num_dev_failed;
	}

	/* Get number of a5 device nodes and a5 mem allocation */
	rc = of_property_read_u32(of_node, "num-a5", &num_dev);
	if (rc < 0) {
		pr_err("getting num of a5 failed\n");
		goto num_dev_failed;
	}

	icp_hw_mgr.devices[CAM_ICP_DEV_A5] = kzalloc(
		sizeof(struct cam_hw_intf *) * num_dev, GFP_KERNEL);
	if (!icp_hw_mgr.devices[CAM_ICP_DEV_A5]) {
		rc = -ENOMEM;
		goto num_dev_failed;
	}

	/* Get number of ipe device nodes and ipe mem allocation */
	rc = of_property_read_u32(of_node, "num-ipe", &num_ipe_dev);
	if (rc < 0) {
		pr_err("getting number of ipe dev nodes failed\n");
		goto num_ipe_failed;
	}

	icp_hw_mgr.devices[CAM_ICP_DEV_IPE] = kzalloc(
		sizeof(struct cam_hw_intf *) * num_ipe_dev, GFP_KERNEL);
	if (!icp_hw_mgr.devices[CAM_ICP_DEV_IPE]) {
		rc = -ENOMEM;
		goto num_ipe_failed;
	}

	/* Get number of bps device nodes and bps mem allocation */
	rc = of_property_read_u32(of_node, "num-bps", &num_dev);
	if (rc < 0) {
		pr_err("read num bps devices failed\n");
		goto num_bps_failed;
	}
	icp_hw_mgr.devices[CAM_ICP_DEV_BPS] = kzalloc(
		sizeof(struct cam_hw_intf *) * num_dev, GFP_KERNEL);
	if (!icp_hw_mgr.devices[CAM_ICP_DEV_BPS]) {
		rc = -ENOMEM;
		goto num_bps_failed;
	}

	for (i = 0; i < count; i++) {
		rc = of_property_read_string_index(of_node, "compat-hw-name",
								i, &name);
		if (rc < 0) {
			pr_err("getting dev object name failed\n");
			goto compat_hw_name_failed;
		}

		child_node = of_find_node_by_name(NULL, name);
		if (!child_node) {
			pr_err("error! Cannot find node in dtsi %s\n", name);
			rc = -ENODEV;
			goto compat_hw_name_failed;
		}

		child_pdev = of_find_device_by_node(child_node);
		if (!child_pdev) {
			pr_err("failed to find device on bus %s\n",
				child_node->name);
			rc = -ENODEV;
			of_node_put(child_node);
			goto compat_hw_name_failed;
		}

		child_dev_intf = (struct cam_hw_intf *)platform_get_drvdata(
								child_pdev);
		if (!child_dev_intf) {
			pr_err("no child device\n");
			of_node_put(child_node);
			goto compat_hw_name_failed;
		}
		ICP_DBG("child_intf %pK\n", child_dev_intf);
		ICP_DBG("child type %d index %d\n",	child_dev_intf->hw_type,
				child_dev_intf->hw_idx);

		icp_hw_mgr.devices[child_dev_intf->hw_type]
			[child_dev_intf->hw_idx] = child_dev_intf;

		of_node_put(child_node);
	}

	rc = cam_smmu_get_handle("icp", &icp_hw_mgr.iommu_hdl);
	if (rc < 0) {
		pr_err("icp get iommu handle failed\n");
		goto compat_hw_name_failed;
	}

	pr_err("mmu handle :%d\n", icp_hw_mgr.iommu_hdl);
	rc = cam_smmu_ops(icp_hw_mgr.iommu_hdl, CAM_SMMU_ATTACH);
	if (rc < 0) {
		pr_err("icp attach failed: %d\n", rc);
		goto icp_attach_failed;
	}

	rc = cam_req_mgr_workq_create("icp_command_queue", ICP_WORKQ_NUM_TASK,
		&icp_hw_mgr.cmd_work, CRM_WORKQ_USAGE_NON_IRQ);
	if (rc < 0) {
		pr_err("unable to create a worker\n");
		goto cmd_work_failed;
	}

	rc = cam_req_mgr_workq_create("icp_message_queue", ICP_WORKQ_NUM_TASK,
		&icp_hw_mgr.msg_work, CRM_WORKQ_USAGE_IRQ);
	if (rc < 0) {
		pr_err("unable to create a worker\n");
		goto msg_work_failed;
	}

	icp_hw_mgr.cmd_work_data = (struct hfi_cmd_work_data *)
		kzalloc(sizeof(struct hfi_cmd_work_data) * ICP_WORKQ_NUM_TASK,
		GFP_KERNEL);
	if (!icp_hw_mgr.cmd_work_data)
		goto cmd_work_data_failed;

	icp_hw_mgr.msg_work_data = (struct hfi_msg_work_data *)
		kzalloc(sizeof(struct hfi_msg_work_data) * ICP_WORKQ_NUM_TASK,
		GFP_KERNEL);
	if (!icp_hw_mgr.msg_work_data)
		goto msg_work_data_failed;

	rc = cam_icp_hw_mgr_create_debugfs_entry();
	if (rc)
		goto msg_work_data_failed;

	for (i = 0; i < ICP_WORKQ_NUM_TASK; i++)
		icp_hw_mgr.msg_work->task.pool[i].payload =
				&icp_hw_mgr.msg_work_data[i];

	for (i = 0; i < ICP_WORKQ_NUM_TASK; i++)
		icp_hw_mgr.cmd_work->task.pool[i].payload =
				&icp_hw_mgr.cmd_work_data[i];

	init_completion(&icp_hw_mgr.a5_complete);

	return rc;

msg_work_data_failed:
	kfree(icp_hw_mgr.cmd_work_data);
cmd_work_data_failed:
	cam_req_mgr_workq_destroy(&icp_hw_mgr.msg_work);
msg_work_failed:
	cam_req_mgr_workq_destroy(&icp_hw_mgr.cmd_work);
cmd_work_failed:
	cam_smmu_ops(icp_hw_mgr.iommu_hdl, CAM_SMMU_DETACH);
icp_attach_failed:
	icp_hw_mgr.iommu_hdl = 0;
compat_hw_name_failed:
	kfree(icp_hw_mgr.devices[CAM_ICP_DEV_BPS]);
num_bps_failed:
	kfree(icp_hw_mgr.devices[CAM_ICP_DEV_IPE]);
num_ipe_failed:
	kfree(icp_hw_mgr.devices[CAM_ICP_DEV_A5]);
num_dev_failed:
	mutex_destroy(&icp_hw_mgr.hw_mgr_mutex);
	for (i = 0; i < CAM_ICP_CTX_MAX; i++)
		mutex_destroy(&icp_hw_mgr.ctx_data[i].ctx_mutex);

	return rc;
}
