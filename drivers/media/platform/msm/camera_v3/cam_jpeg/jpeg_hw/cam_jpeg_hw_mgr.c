/* Copyright (c) 2017-2019, The Linux Foundation. All rights reserved
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

#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/debugfs.h>
#include <media/cam_defs.h>
#include <media/cam_jpeg.h>

#include "cam_sync_api.h"
#include "cam_packet_util.h"
#include "cam_hw.h"
#include "cam_hw_mgr_intf.h"
#include "cam_jpeg_hw_mgr_intf.h"
#include "cam_jpeg_hw_mgr.h"
#include "cam_smmu_api.h"
#include "cam_mem_mgr.h"
#include "cam_req_mgr_workq.h"
#include "cam_mem_mgr.h"
#include "cam_cdm_intf_api.h"
#include "cam_debug_util.h"
#include "cam_common_util.h"

#define CAM_JPEG_HW_ENTRIES_MAX  20
#define CAM_JPEG_CHBASE          0
#define CAM_JPEG_CFG             1
#define CAM_JPEG_PARAM           2

static struct cam_jpeg_hw_mgr g_jpeg_hw_mgr;

static int32_t cam_jpeg_hw_mgr_cb(uint32_t irq_status,
	int32_t result_size, void *data);
static int cam_jpeg_mgr_process_cmd(void *priv, void *data);

static int cam_jpeg_mgr_process_irq(void *priv, void *data)
{
	int rc = 0;
	struct cam_jpeg_process_irq_work_data_t *task_data;
	struct cam_jpeg_hw_mgr *hw_mgr;
	int32_t i;
	struct cam_jpeg_hw_ctx_data *ctx_data = NULL;
	struct cam_hw_done_event_data buf_data;
	struct cam_jpeg_set_irq_cb irq_cb;
	uintptr_t dev_type = 0;
	uintptr_t kaddr;
	uint32_t *cmd_buf_kaddr;
	size_t cmd_buf_len;
	struct cam_jpeg_config_inout_param_info *p_params;
	struct cam_jpeg_hw_cfg_req *p_cfg_req = NULL;
	struct crm_workq_task *task;
	struct cam_jpeg_process_frame_work_data_t *wq_task_data;

	if (!data || !priv) {
		CAM_ERR(CAM_JPEG, "Invalid data");
		return -EINVAL;
	}

	task_data = data;
	hw_mgr = &g_jpeg_hw_mgr;

	ctx_data = (struct cam_jpeg_hw_ctx_data *)task_data->data;
	if (!ctx_data->in_use) {
		CAM_ERR(CAM_JPEG, "ctx is not in use");
		return -EINVAL;
	}

	dev_type = ctx_data->jpeg_dev_acquire_info.dev_type;

	mutex_lock(&g_jpeg_hw_mgr.hw_mgr_mutex);

	p_cfg_req = hw_mgr->dev_hw_cfg_args[dev_type][0];

	if (hw_mgr->device_in_use[dev_type][0] == false ||
		p_cfg_req == NULL) {
		CAM_ERR(CAM_JPEG, "irq for old request %d", rc);
		mutex_unlock(&g_jpeg_hw_mgr.hw_mgr_mutex);
		return -EINVAL;
	}

	irq_cb.jpeg_hw_mgr_cb = cam_jpeg_hw_mgr_cb;
	irq_cb.data = NULL;
	irq_cb.b_set_cb = false;
	if (!hw_mgr->devices[dev_type][0]->hw_ops.process_cmd) {
		CAM_ERR(CAM_JPEG, "process_cmd null ");
		mutex_unlock(&g_jpeg_hw_mgr.hw_mgr_mutex);
		return -EINVAL;
	}
	rc = hw_mgr->devices[dev_type][0]->hw_ops.process_cmd(
		hw_mgr->devices[dev_type][0]->hw_priv,
		CAM_JPEG_CMD_SET_IRQ_CB,
		&irq_cb, sizeof(irq_cb));
	if (rc) {
		CAM_ERR(CAM_JPEG, "CMD_SET_IRQ_CB failed %d", rc);
		mutex_unlock(&g_jpeg_hw_mgr.hw_mgr_mutex);
		return rc;
	}

	if (hw_mgr->devices[dev_type][0]->hw_ops.deinit) {
		rc = hw_mgr->devices[dev_type][0]->hw_ops.deinit(
			hw_mgr->devices[dev_type][0]->hw_priv, NULL, 0);
		if (rc)
			CAM_ERR(CAM_JPEG, "Failed to Deinit %lu HW", dev_type);
	}

	hw_mgr->device_in_use[dev_type][0] = false;
	hw_mgr->dev_hw_cfg_args[dev_type][0] = NULL;
	mutex_unlock(&g_jpeg_hw_mgr.hw_mgr_mutex);

	task = cam_req_mgr_workq_get_task(
		g_jpeg_hw_mgr.work_process_frame);
	if (!task) {
		CAM_ERR(CAM_JPEG, "no empty task");
		return -EINVAL;
	}

	wq_task_data = (struct cam_jpeg_process_frame_work_data_t *)
		task->payload;
	if (!task_data) {
		CAM_ERR(CAM_JPEG, "task_data is NULL");
		return -EINVAL;
	}
	wq_task_data->data = (void *)dev_type;
	wq_task_data->request_id = 0;
	wq_task_data->type = CAM_JPEG_WORKQ_TASK_CMD_TYPE;
	task->process_cb = cam_jpeg_mgr_process_cmd;
	rc = cam_req_mgr_workq_enqueue_task(task, &g_jpeg_hw_mgr,
		CRM_TASK_PRIORITY_0);
	if (rc) {
		CAM_ERR(CAM_JPEG, "could not enque task %d", rc);
		return rc;
	}

	rc = cam_mem_get_cpu_buf(
		p_cfg_req->hw_cfg_args.hw_update_entries[CAM_JPEG_PARAM].handle,
		&kaddr, &cmd_buf_len);
	if (rc) {
		CAM_ERR(CAM_JPEG, "unable to get info for cmd buf: %x %d",
			hw_mgr->iommu_hdl, rc);
		return rc;
	}

	cmd_buf_kaddr = (uint32_t *)kaddr;

	cmd_buf_kaddr =
		(cmd_buf_kaddr +
		(p_cfg_req->hw_cfg_args.hw_update_entries[CAM_JPEG_PARAM].offset
			/ sizeof(uint32_t)));

	p_params = (struct cam_jpeg_config_inout_param_info *)cmd_buf_kaddr;

	p_params->output_size = task_data->result_size;
	CAM_DBG(CAM_JPEG, "Encoded Size %d", task_data->result_size);

	buf_data.num_handles =
		p_cfg_req->hw_cfg_args.num_out_map_entries;
	for (i = 0; i < buf_data.num_handles; i++) {
		buf_data.resource_handle[i] =
		p_cfg_req->hw_cfg_args.out_map_entries[i].resource_handle;
	}
	buf_data.request_id =
		PTR_TO_U64(p_cfg_req->hw_cfg_args.priv);
	ctx_data->ctxt_event_cb(ctx_data->context_priv, 0, &buf_data);

	list_add_tail(&p_cfg_req->list, &hw_mgr->free_req_list);

	return rc;
}

static int cam_jpeg_hw_mgr_cb(
	uint32_t irq_status, int32_t result_size, void *data)
{
	int32_t rc;
	unsigned long flags;
	struct cam_jpeg_hw_mgr *hw_mgr = &g_jpeg_hw_mgr;
	struct crm_workq_task *task;
	struct cam_jpeg_process_irq_work_data_t *task_data;

	spin_lock_irqsave(&hw_mgr->hw_mgr_lock, flags);
	task = cam_req_mgr_workq_get_task(
		g_jpeg_hw_mgr.work_process_irq_cb);
	if (!task) {
		CAM_ERR(CAM_JPEG, "no empty task");
		spin_unlock_irqrestore(&hw_mgr->hw_mgr_lock, flags);
		return -ENOMEM;
	}

	task_data = (struct cam_jpeg_process_irq_work_data_t *)task->payload;
	task_data->data = data;
	task_data->irq_status = irq_status;
	task_data->result_size = result_size;
	task_data->type = CAM_JPEG_WORKQ_TASK_MSG_TYPE;
	task->process_cb = cam_jpeg_mgr_process_irq;

	rc = cam_req_mgr_workq_enqueue_task(task, &g_jpeg_hw_mgr,
		CRM_TASK_PRIORITY_0);
	spin_unlock_irqrestore(&hw_mgr->hw_mgr_lock, flags);

	return rc;
}

static int cam_jpeg_mgr_get_free_ctx(struct cam_jpeg_hw_mgr *hw_mgr)
{
	int i = 0;
	int num_ctx = CAM_JPEG_CTX_MAX;

	for (i = 0; i < num_ctx; i++) {
		mutex_lock(&hw_mgr->ctx_data[i].ctx_mutex);
		if (hw_mgr->ctx_data[i].in_use == false) {
			hw_mgr->ctx_data[i].in_use = true;
			mutex_unlock(&hw_mgr->ctx_data[i].ctx_mutex);
			break;
		}
		mutex_unlock(&hw_mgr->ctx_data[i].ctx_mutex);
	}

	return i;
}


static int cam_jpeg_mgr_release_ctx(
	struct cam_jpeg_hw_mgr *hw_mgr, struct cam_jpeg_hw_ctx_data *ctx_data)
{
	if (!ctx_data) {
		CAM_ERR(CAM_JPEG, "invalid ctx_data %pK", ctx_data);
		return -EINVAL;
	}

	mutex_lock(&ctx_data->ctx_mutex);
	if (!ctx_data->in_use) {
		CAM_ERR(CAM_JPEG, "ctx is already un-used: %pK", ctx_data);
		mutex_unlock(&ctx_data->ctx_mutex);
		return -EINVAL;
	}

	ctx_data->in_use = false;
	mutex_unlock(&ctx_data->ctx_mutex);

	return 0;
}

static int cam_jpeg_insert_cdm_change_base(
	struct cam_hw_config_args *config_args,
	struct cam_jpeg_hw_ctx_data *ctx_data,
	struct cam_jpeg_hw_mgr *hw_mgr)
{
	int rc = 0;
	uint32_t dev_type;
	struct cam_cdm_bl_request *cdm_cmd;
	uint32_t size;
	uint32_t mem_cam_base;
	uintptr_t iova_addr;
	uint32_t *ch_base_iova_addr;
	size_t ch_base_len;

	rc = cam_mem_get_cpu_buf(
		config_args->hw_update_entries[CAM_JPEG_CHBASE].handle,
		&iova_addr, &ch_base_len);
	if (rc) {
		CAM_ERR(CAM_JPEG,
			"unable to get src buf info for cmd buf: %d", rc);
		return rc;
	}

	if (config_args->hw_update_entries[CAM_JPEG_CHBASE].offset >=
		ch_base_len) {
		CAM_ERR(CAM_JPEG, "Not enough buf");
		return -EINVAL;
	}
	CAM_DBG(CAM_JPEG, "iova %pK len %zu offset %d",
		(void *)iova_addr, ch_base_len,
		config_args->hw_update_entries[CAM_JPEG_CHBASE].offset);
	ch_base_iova_addr = (uint32_t *)iova_addr;
	ch_base_iova_addr = (ch_base_iova_addr +
		(config_args->hw_update_entries[CAM_JPEG_CHBASE].offset /
		sizeof(uint32_t)));

	dev_type = ctx_data->jpeg_dev_acquire_info.dev_type;
	mem_cam_base = hw_mgr->cdm_reg_map[dev_type][0]->mem_cam_base;
	size =
	hw_mgr->cdm_info[dev_type][0].cdm_ops->cdm_required_size_changebase();
	hw_mgr->cdm_info[dev_type][0].cdm_ops->cdm_write_changebase(
		ch_base_iova_addr, mem_cam_base);

	cdm_cmd = ctx_data->cdm_cmd;
	cdm_cmd->cmd[cdm_cmd->cmd_arrary_count].bl_addr.mem_handle =
		config_args->hw_update_entries[CAM_JPEG_CHBASE].handle;
	cdm_cmd->cmd[cdm_cmd->cmd_arrary_count].offset =
		config_args->hw_update_entries[CAM_JPEG_CHBASE].offset;
	cdm_cmd->cmd[cdm_cmd->cmd_arrary_count].len = size * sizeof(uint32_t);
	cdm_cmd->cmd_arrary_count++;

	ch_base_iova_addr += size;
	*ch_base_iova_addr = 0;
	ch_base_iova_addr += size;
	*ch_base_iova_addr = 0;

	return rc;
}

static int cam_jpeg_mgr_process_cmd(void *priv, void *data)
{
	int rc;
	int i = 0;
	struct cam_jpeg_hw_mgr *hw_mgr = priv;
	struct cam_hw_update_entry *cmd;
	struct cam_cdm_bl_request *cdm_cmd;
	struct cam_hw_config_args *config_args = NULL;
	struct cam_jpeg_hw_ctx_data *ctx_data = NULL;
	uintptr_t request_id = 0;
	struct cam_jpeg_process_frame_work_data_t *task_data =
		(struct cam_jpeg_process_frame_work_data_t *)data;
	uint32_t dev_type;
	struct cam_jpeg_set_irq_cb irq_cb;
	struct cam_jpeg_hw_cfg_req *p_cfg_req = NULL;
	struct cam_hw_done_event_data buf_data;
	struct cam_hw_config_args *hw_cfg_args = NULL;

	if (!hw_mgr || !task_data) {
		CAM_ERR(CAM_JPEG, "Invalid arguments %pK %pK",
			hw_mgr, task_data);
		return -EINVAL;
	}

	mutex_lock(&hw_mgr->hw_mgr_mutex);

	if (list_empty(&hw_mgr->hw_config_req_list)) {
		CAM_DBG(CAM_JPEG, "no available request");
		rc = -EFAULT;
		goto end;
	}

	p_cfg_req = list_first_entry(&hw_mgr->hw_config_req_list,
		struct cam_jpeg_hw_cfg_req, list);
	if (!p_cfg_req) {
		CAM_ERR(CAM_JPEG, "no request");
		rc = -EFAULT;
		goto end;
	}

	if (false == hw_mgr->device_in_use[p_cfg_req->dev_type][0]) {
		hw_mgr->device_in_use[p_cfg_req->dev_type][0] = true;
		hw_mgr->dev_hw_cfg_args[p_cfg_req->dev_type][0] = p_cfg_req;
		list_del_init(&p_cfg_req->list);
	} else {
		CAM_DBG(CAM_JPEG, "Not dequeing, just return");
		rc = -EFAULT;
		goto end;
	}

	config_args = (struct cam_hw_config_args *)&p_cfg_req->hw_cfg_args;
	request_id = task_data->request_id;
	if (request_id != (uintptr_t)config_args->priv) {
		CAM_DBG(CAM_JPEG, "not a recent req %zd %zd",
			request_id, (uintptr_t)config_args->priv);
	}

	if (!config_args->num_hw_update_entries) {
		CAM_ERR(CAM_JPEG, "No hw update enteries are available");
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		rc = -EINVAL;
		goto end_unusedev;
	}

	ctx_data = (struct cam_jpeg_hw_ctx_data *)config_args->ctxt_to_hw_map;
	if (!ctx_data->in_use) {
		CAM_ERR(CAM_JPEG, "ctx is not in use");
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		rc = -EINVAL;
		goto end_unusedev;
	}

	dev_type = ctx_data->jpeg_dev_acquire_info.dev_type;

	if (dev_type != p_cfg_req->dev_type)
		CAM_WARN(CAM_JPEG, "dev types not same something wrong");

	if (!hw_mgr->devices[dev_type][0]->hw_ops.init) {
		CAM_ERR(CAM_JPEG, "hw op init null ");
		rc = -EFAULT;
		goto end;
	}
	rc = hw_mgr->devices[dev_type][0]->hw_ops.init(
		hw_mgr->devices[dev_type][0]->hw_priv,
		ctx_data,
		sizeof(ctx_data));
	if (rc) {
		CAM_ERR(CAM_JPEG, "Failed to Init %d HW", dev_type);
		goto end;
	}

	irq_cb.jpeg_hw_mgr_cb = cam_jpeg_hw_mgr_cb;
	irq_cb.data = (void *)ctx_data;
	irq_cb.b_set_cb = true;
	if (!hw_mgr->devices[dev_type][0]->hw_ops.process_cmd) {
		CAM_ERR(CAM_JPEG, "op process_cmd null ");
		rc = -EFAULT;
		goto end_callcb;
	}
	rc = hw_mgr->devices[dev_type][0]->hw_ops.process_cmd(
		hw_mgr->devices[dev_type][0]->hw_priv,
		CAM_JPEG_CMD_SET_IRQ_CB,
		&irq_cb, sizeof(irq_cb));
	if (rc) {
		CAM_ERR(CAM_JPEG, "SET_IRQ_CB failed %d", rc);
		goto end_callcb;
	}

	if (!hw_mgr->devices[dev_type][0]->hw_ops.reset) {
		CAM_ERR(CAM_JPEG, "op reset null ");
		rc = -EFAULT;
		goto end_callcb;
	}
	rc = hw_mgr->devices[dev_type][0]->hw_ops.reset(
		hw_mgr->devices[dev_type][0]->hw_priv,
		NULL, 0);
	if (rc) {
		CAM_ERR(CAM_JPEG, "jpeg hw reset failed %d", rc);
		goto end_callcb;
	}

	cdm_cmd = ctx_data->cdm_cmd;
	cdm_cmd->type = CAM_CDM_BL_CMD_TYPE_MEM_HANDLE;
	cdm_cmd->flag = false;
	cdm_cmd->userdata = NULL;
	cdm_cmd->cookie = 0;
	cdm_cmd->cmd_arrary_count = 0;

	rc = cam_jpeg_insert_cdm_change_base(config_args,
		ctx_data, hw_mgr);
	if (rc) {
		CAM_ERR(CAM_JPEG, "insert change base failed %d", rc);
		goto end_callcb;
	}

	CAM_DBG(CAM_JPEG, "num hw up %d", config_args->num_hw_update_entries);
	for (i = CAM_JPEG_CFG; i < (config_args->num_hw_update_entries - 1);
		i++) {
		cmd = (config_args->hw_update_entries + i);
		cdm_cmd->cmd[cdm_cmd->cmd_arrary_count].bl_addr.mem_handle
			= cmd->handle;
		cdm_cmd->cmd[cdm_cmd->cmd_arrary_count].offset =
			cmd->offset;
		cdm_cmd->cmd[cdm_cmd->cmd_arrary_count].len =
			cmd->len;
		CAM_DBG(CAM_JPEG, "i %d entry h %d o %d l %d",
			i, cmd->handle, cmd->offset, cmd->len);
		cdm_cmd->cmd_arrary_count++;
	}

	rc = cam_cdm_submit_bls(
		hw_mgr->cdm_info[dev_type][0].cdm_handle, cdm_cmd);
	if (rc) {
		CAM_ERR(CAM_JPEG, "Failed to apply the configs %d", rc);
		goto end_callcb;
	}

	if (!hw_mgr->devices[dev_type][0]->hw_ops.start) {
		CAM_ERR(CAM_JPEG, "op start null ");
		rc = -EINVAL;
		goto end_callcb;
	}
	rc = hw_mgr->devices[dev_type][0]->hw_ops.start(
		hw_mgr->devices[dev_type][0]->hw_priv,
		NULL, 0);
	if (rc) {
		CAM_ERR(CAM_JPEG, "Failed to start hw %d",
			rc);
		goto end_callcb;
	}

	mutex_unlock(&hw_mgr->hw_mgr_mutex);
	return rc;

end_callcb:
	mutex_unlock(&hw_mgr->hw_mgr_mutex);
	if (p_cfg_req) {
		hw_cfg_args = &p_cfg_req->hw_cfg_args;
		buf_data.num_handles =
			hw_cfg_args->num_out_map_entries;
		for (i = 0; i < buf_data.num_handles; i++) {
			buf_data.resource_handle[i] =
			hw_cfg_args->out_map_entries[i].resource_handle;
		}
		buf_data.request_id =
			(uintptr_t)p_cfg_req->hw_cfg_args.priv;
		ctx_data->ctxt_event_cb(ctx_data->context_priv, 0, &buf_data);
	}

end_unusedev:
	mutex_lock(&hw_mgr->hw_mgr_mutex);
	hw_mgr->device_in_use[p_cfg_req->dev_type][0] = false;
	hw_mgr->dev_hw_cfg_args[p_cfg_req->dev_type][0] = NULL;

end:
	mutex_unlock(&hw_mgr->hw_mgr_mutex);
	return rc;
}

static int cam_jpeg_mgr_config_hw(void *hw_mgr_priv, void *config_hw_args)
{
	int rc;
	struct cam_jpeg_hw_mgr *hw_mgr = hw_mgr_priv;
	struct cam_hw_config_args *config_args = config_hw_args;
	struct cam_jpeg_hw_ctx_data *ctx_data = NULL;
	uintptr_t request_id = 0;
	struct cam_hw_update_entry *hw_update_entries;
	struct crm_workq_task *task;
	struct cam_jpeg_process_frame_work_data_t *task_data;
	struct cam_jpeg_hw_cfg_req *p_cfg_req = NULL;

	if (!hw_mgr || !config_args) {
		CAM_ERR(CAM_JPEG, "Invalid arguments %pK %pK",
			hw_mgr, config_args);
		return -EINVAL;
	}

	if (!config_args->num_hw_update_entries) {
		CAM_ERR(CAM_JPEG, "No hw update enteries are available");
		return -EINVAL;
	}

	mutex_lock(&hw_mgr->hw_mgr_mutex);

	ctx_data = (struct cam_jpeg_hw_ctx_data *)config_args->ctxt_to_hw_map;
	if (!ctx_data->in_use) {
		CAM_ERR(CAM_JPEG, "ctx is not in use");
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		return -EINVAL;
	}

	if (list_empty(&hw_mgr->free_req_list)) {
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		CAM_ERR(CAM_JPEG, "list empty");
		return -ENOMEM;
	}

	p_cfg_req = list_first_entry(&hw_mgr->free_req_list,
		struct cam_jpeg_hw_cfg_req, list);
	list_del_init(&p_cfg_req->list);

	/* Update Currently Processing Config Request */
	p_cfg_req->hw_cfg_args = *config_args;
	p_cfg_req->dev_type = ctx_data->jpeg_dev_acquire_info.dev_type;

	request_id = (uintptr_t)config_args->priv;
	p_cfg_req->req_id = request_id;
	hw_update_entries = config_args->hw_update_entries;
	CAM_DBG(CAM_JPEG, "ctx_data = %pK req_id = %lld %zd",
		ctx_data, request_id, (uintptr_t)config_args->priv);
	task = cam_req_mgr_workq_get_task(g_jpeg_hw_mgr.work_process_frame);
	if (!task) {
		CAM_ERR(CAM_JPEG, "no empty task");
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		rc = -ENOMEM;
		goto err_after_dq_free_list;
	}


	task_data = (struct cam_jpeg_process_frame_work_data_t *)
		task->payload;
	if (!task_data) {
		CAM_ERR(CAM_JPEG, "task_data is NULL");
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		rc = -EINVAL;
		goto err_after_dq_free_list;
	}
	CAM_DBG(CAM_JPEG, "cfge %pK num %d",
		p_cfg_req->hw_cfg_args.hw_update_entries,
		p_cfg_req->hw_cfg_args.num_hw_update_entries);

	list_add_tail(&p_cfg_req->list, &hw_mgr->hw_config_req_list);
	mutex_unlock(&hw_mgr->hw_mgr_mutex);

	task_data->data = (void *)(uintptr_t)p_cfg_req->dev_type;
	task_data->request_id = request_id;
	task_data->type = CAM_JPEG_WORKQ_TASK_CMD_TYPE;
	task->process_cb = cam_jpeg_mgr_process_cmd;

	rc = cam_req_mgr_workq_enqueue_task(task, &g_jpeg_hw_mgr,
		CRM_TASK_PRIORITY_0);
	if (rc) {
		CAM_ERR(CAM_JPEG, "failed to enqueue task %d", rc);
		goto err_after_get_task;
	}

	return rc;

err_after_get_task:
	list_del_init(&p_cfg_req->list);
err_after_dq_free_list:
	list_add_tail(&p_cfg_req->list, &hw_mgr->free_req_list);

	return rc;
}

static void cam_jpeg_mgr_print_io_bufs(struct cam_packet *packet,
	int32_t iommu_hdl, int32_t sec_mmu_hdl, uint32_t pf_buf_info,
	bool *mem_found)
{
	uint64_t   iova_addr;
	size_t     src_buf_size;
	int        i;
	int        j;
	int        rc = 0;
	int32_t    mmu_hdl;
	struct cam_buf_io_cfg  *io_cfg = NULL;

	if (mem_found)
		*mem_found = false;

	io_cfg = (struct cam_buf_io_cfg *)((uint32_t *)&packet->payload +
		packet->io_configs_offset / 4);

	for (i = 0; i < packet->num_io_configs; i++) {
		for (j = 0; j < CAM_PACKET_MAX_PLANES; j++) {
			if (!io_cfg[i].mem_handle[j])
				break;

			if (GET_FD_FROM_HANDLE(io_cfg[i].mem_handle[j]) ==
				GET_FD_FROM_HANDLE(pf_buf_info)) {
				CAM_INFO(CAM_JPEG,
					"Found PF at port: %d mem %x fd: %x",
					io_cfg[i].resource_type,
					io_cfg[i].mem_handle[j],
					pf_buf_info);
				if (mem_found)
					*mem_found = true;
			}

			CAM_INFO(CAM_JPEG, "port: %d f: %u format: %d dir %d",
				io_cfg[i].resource_type,
				io_cfg[i].fence,
				io_cfg[i].format,
				io_cfg[i].direction);

			mmu_hdl = cam_mem_is_secure_buf(
				io_cfg[i].mem_handle[j]) ? sec_mmu_hdl :
				iommu_hdl;
			rc = cam_mem_get_io_buf(io_cfg[i].mem_handle[j],
				mmu_hdl, &iova_addr, &src_buf_size);
			if (rc < 0) {
				CAM_ERR(CAM_UTIL, "get src buf address fail");
				continue;
			}
			if (iova_addr >> 32) {
				CAM_ERR(CAM_JPEG, "Invalid mapped address");
				rc = -EINVAL;
				continue;
			}

			CAM_INFO(CAM_JPEG,
				"pln %d w %d h %d size %d addr 0x%x offset 0x%x memh %x",
				j, io_cfg[i].planes[j].width,
				io_cfg[i].planes[j].height,
				(int32_t)src_buf_size,
				(unsigned int)iova_addr,
				io_cfg[i].offsets[j],
				io_cfg[i].mem_handle[j]);

			iova_addr += io_cfg[i].offsets[j];
		}
	}
}

static int cam_jpeg_mgr_prepare_hw_update(void *hw_mgr_priv,
	void *prepare_hw_update_args)
{
	int rc, i, j, k;
	struct cam_hw_prepare_update_args *prepare_args =
		prepare_hw_update_args;
	struct cam_jpeg_hw_mgr *hw_mgr = hw_mgr_priv;
	struct cam_jpeg_hw_ctx_data *ctx_data = NULL;
	struct cam_packet *packet = NULL;
	struct cam_cmd_buf_desc *cmd_desc = NULL;
	struct cam_buf_io_cfg *io_cfg_ptr = NULL;
	struct cam_kmd_buf_info kmd_buf;

	if (!prepare_args || !hw_mgr) {
		CAM_ERR(CAM_JPEG, "Invalid args %pK %pK",
			prepare_args, hw_mgr);
		return -EINVAL;
	}

	mutex_lock(&hw_mgr->hw_mgr_mutex);
	ctx_data = (struct cam_jpeg_hw_ctx_data *)prepare_args->ctxt_to_hw_map;
	if (!ctx_data->in_use) {
		CAM_ERR(CAM_JPEG, "ctx is not in use");
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		return -EINVAL;
	}
	mutex_unlock(&hw_mgr->hw_mgr_mutex);

	packet = prepare_args->packet;
	if (!packet) {
		CAM_ERR(CAM_JPEG, "received packet is NULL");
		return -EINVAL;
	}

	if (((packet->header.op_code & 0xff) != CAM_JPEG_OPCODE_ENC_UPDATE) &&
		((packet->header.op_code
		& 0xff) != CAM_JPEG_OPCODE_DMA_UPDATE)) {
		CAM_ERR(CAM_JPEG, "Invalid Opcode in pkt: %d",
			packet->header.op_code & 0xff);
		return -EINVAL;
	}

	rc = cam_packet_util_validate_packet(packet, prepare_args->remain_len);
	if (rc) {
		CAM_ERR(CAM_JPEG, "invalid packet %d", rc);
		return rc;
	}

	if ((packet->num_cmd_buf > 5) || !packet->num_patches ||
		!packet->num_io_configs) {
		CAM_ERR(CAM_JPEG, "wrong number of cmd/patch info: %u %u",
			packet->num_cmd_buf,
			packet->num_patches);
		return -EINVAL;
	}

	cmd_desc = (struct cam_cmd_buf_desc *)
		((uint32_t *)&packet->payload +
		(packet->cmd_buf_offset / 4));
	CAM_DBG(CAM_JPEG, "packet = %pK cmd_desc = %pK size = %lu",
		(void *)packet, (void *)cmd_desc,
		sizeof(struct cam_cmd_buf_desc));

	rc = cam_packet_util_process_patches(packet, hw_mgr->iommu_hdl, -1);
	if (rc) {
		CAM_ERR(CAM_JPEG, "Patch processing failed %d", rc);
		return rc;
	}

	io_cfg_ptr = (struct cam_buf_io_cfg *)((uint32_t *)&packet->payload +
		packet->io_configs_offset / 4);
	CAM_DBG(CAM_JPEG, "packet = %pK io_cfg_ptr = %pK size = %lu",
		(void *)packet, (void *)io_cfg_ptr,
		sizeof(struct cam_buf_io_cfg));
	prepare_args->pf_data->packet = packet;

	prepare_args->num_out_map_entries = 0;

	for (i = 0, j = 0, k = 0; i < packet->num_io_configs; i++) {
		if (io_cfg_ptr[i].direction == CAM_BUF_INPUT) {
			prepare_args->in_map_entries[j].resource_handle =
				io_cfg_ptr[i].resource_type;
			prepare_args->in_map_entries[j++].sync_id =
				io_cfg_ptr[i].fence;
			prepare_args->num_in_map_entries++;
		} else {
			prepare_args->in_map_entries[k].resource_handle =
				io_cfg_ptr[i].resource_type;
			prepare_args->out_map_entries[k++].sync_id =
				io_cfg_ptr[i].fence;
			prepare_args->num_out_map_entries++;
		}
		CAM_DBG(CAM_JPEG, "dir[%d]: %u, fence: %u",
			i, io_cfg_ptr[i].direction, io_cfg_ptr[i].fence);
	}


	j = prepare_args->num_hw_update_entries;
	rc = cam_packet_util_get_kmd_buffer(packet, &kmd_buf);
	if (rc) {
		CAM_ERR(CAM_JPEG, "get kmd buf failed %d", rc);
		return rc;
	}
	/* fill kmd buf info into 1st hw update entry */
	prepare_args->hw_update_entries[j].len =
		(uint32_t)kmd_buf.used_bytes;
	prepare_args->hw_update_entries[j].handle =
		(uint32_t)kmd_buf.handle;
	prepare_args->hw_update_entries[j].offset =
		(uint32_t)kmd_buf.offset;
	j++;

	for (i = 0; i < packet->num_cmd_buf;  i++, j++) {
		prepare_args->hw_update_entries[j].len =
			(uint32_t)cmd_desc[i].length;
		prepare_args->hw_update_entries[j].handle =
			(uint32_t)cmd_desc[i].mem_handle;
		prepare_args->hw_update_entries[j].offset =
			(uint32_t)cmd_desc[i].offset;
	}
	prepare_args->num_hw_update_entries = j;
	prepare_args->priv = (void *)(uintptr_t)packet->header.request_id;

	CAM_DBG(CAM_JPEG, "will wait on input sync sync_id %d",
		prepare_args->in_map_entries[0].sync_id);

	return rc;
}

static void cam_jpeg_mgr_stop_deinit_dev(struct cam_jpeg_hw_mgr *hw_mgr,
	struct cam_jpeg_hw_cfg_req *p_cfg_req, uint32_t dev_type)
{
	int rc = 0;
	struct cam_jpeg_set_irq_cb irq_cb;

	/* stop reset Unregister CB and deinit */
	irq_cb.jpeg_hw_mgr_cb = cam_jpeg_hw_mgr_cb;
	irq_cb.data = NULL;
	irq_cb.b_set_cb = false;
	if (hw_mgr->devices[dev_type][0]->hw_ops.process_cmd) {
		rc = hw_mgr->devices[dev_type][0]->hw_ops.process_cmd(
			hw_mgr->devices[dev_type][0]->hw_priv,
			CAM_JPEG_CMD_SET_IRQ_CB,
			&irq_cb, sizeof(irq_cb));
		if (rc)
			CAM_ERR(CAM_JPEG, "SET_IRQ_CB fail %d", rc);
	} else {
		CAM_ERR(CAM_JPEG, "process_cmd null %d", dev_type);
	}

	if (hw_mgr->devices[dev_type][0]->hw_ops.stop) {
		rc = hw_mgr->devices[dev_type][0]->hw_ops.stop(
			hw_mgr->devices[dev_type][0]->hw_priv,
			NULL, 0);
		if (rc)
			CAM_ERR(CAM_JPEG, "stop fail %d", rc);
	} else {
		CAM_ERR(CAM_JPEG, "op stop null %d", dev_type);
	}

	if (hw_mgr->devices[dev_type][0]->hw_ops.deinit) {
		rc = hw_mgr->devices[dev_type][0]->hw_ops.deinit(
			hw_mgr->devices[dev_type][0]->hw_priv,
			NULL, 0);
		if (rc)
			CAM_ERR(CAM_JPEG, "Failed to Deinit %d HW %d",
				dev_type, rc);
	} else {
		CAM_ERR(CAM_JPEG, "op deinit null %d", dev_type);
	}

	hw_mgr->device_in_use[dev_type][0] = false;
	hw_mgr->dev_hw_cfg_args[dev_type][0] = NULL;
}

static int cam_jpeg_mgr_flush(void *hw_mgr_priv,
	struct cam_jpeg_hw_ctx_data *ctx_data)
{
	struct cam_jpeg_hw_mgr *hw_mgr = hw_mgr_priv;
	uint32_t dev_type;
	struct cam_jpeg_hw_cfg_req *p_cfg_req = NULL;
	struct cam_jpeg_hw_cfg_req *cfg_req = NULL, *req_temp = NULL;

	CAM_DBG(CAM_JPEG, "E: JPEG flush ctx");

	if (!hw_mgr || !ctx_data) {
		CAM_ERR(CAM_JPEG, "Invalid args");
		return -EINVAL;
	}

	dev_type = ctx_data->jpeg_dev_acquire_info.dev_type;

	p_cfg_req = hw_mgr->dev_hw_cfg_args[dev_type][0];
	if (hw_mgr->device_in_use[dev_type][0] == true &&
		p_cfg_req != NULL) {
		if ((struct cam_jpeg_hw_ctx_data *)
			p_cfg_req->hw_cfg_args.ctxt_to_hw_map == ctx_data) {
			cam_jpeg_mgr_stop_deinit_dev(hw_mgr, p_cfg_req,
				dev_type);
			list_del_init(&p_cfg_req->list);
			list_add_tail(&p_cfg_req->list,
				&hw_mgr->free_req_list);
		}
	}

	list_for_each_entry_safe(cfg_req, req_temp,
		&hw_mgr->hw_config_req_list, list) {
		if ((struct cam_jpeg_hw_ctx_data *)
			cfg_req->hw_cfg_args.ctxt_to_hw_map != ctx_data)
			continue;

		list_del_init(&cfg_req->list);
		list_add_tail(&cfg_req->list, &hw_mgr->free_req_list);
	}

	CAM_DBG(CAM_JPEG, "X: JPEG flush ctx");

	return 0;
}

static int cam_jpeg_mgr_flush_req(void *hw_mgr_priv,
	struct cam_jpeg_hw_ctx_data *ctx_data,
	struct cam_hw_flush_args *flush_args)
{
	struct cam_jpeg_hw_mgr *hw_mgr = hw_mgr_priv;
	struct cam_jpeg_hw_cfg_req *cfg_req = NULL;
	struct cam_jpeg_hw_cfg_req *req_temp = NULL;
	long request_id = 0;
	uint32_t dev_type;
	struct cam_jpeg_hw_cfg_req *p_cfg_req = NULL;
	bool b_req_found = false;

	CAM_DBG(CAM_JPEG, "E: JPEG flush req");

	if (!hw_mgr || !ctx_data || !flush_args) {
		CAM_ERR(CAM_JPEG, "Invalid args");
		return -EINVAL;
	}

	if (flush_args->num_req_pending)
		return 0;

	request_id = (uintptr_t)flush_args->flush_req_active[0];

	if (!flush_args->num_req_active)
		return 0;

	if (request_id <= 0) {
		CAM_ERR(CAM_JPEG, "Invalid red id %ld", request_id);
		return -EINVAL;
	}

	dev_type = ctx_data->jpeg_dev_acquire_info.dev_type;

	p_cfg_req = hw_mgr->dev_hw_cfg_args[dev_type][0];
	if (hw_mgr->device_in_use[dev_type][0] == true &&
		p_cfg_req != NULL) {
		if (((struct cam_jpeg_hw_ctx_data *)
			p_cfg_req->hw_cfg_args.ctxt_to_hw_map == ctx_data) &&
			(p_cfg_req->req_id == request_id)) {
			cam_jpeg_mgr_stop_deinit_dev(hw_mgr, p_cfg_req,
				dev_type);
			list_del_init(&p_cfg_req->list);
			list_add_tail(&p_cfg_req->list,
				&hw_mgr->free_req_list);
			b_req_found = true;
		}
	}

	list_for_each_entry_safe(cfg_req, req_temp,
		&hw_mgr->hw_config_req_list, list) {
		if ((struct cam_jpeg_hw_ctx_data *)
			cfg_req->hw_cfg_args.ctxt_to_hw_map != ctx_data)
			continue;

		if (cfg_req->req_id != request_id)
			continue;

		list_del_init(&cfg_req->list);
		list_add_tail(&cfg_req->list, &hw_mgr->free_req_list);
		b_req_found = true;
		break;
	}

	if (!b_req_found) {
		CAM_ERR(CAM_JPEG, "req not found %ld", request_id);
		return -EINVAL;
	}

	CAM_DBG(CAM_JPEG, "X: JPEG flush req");
	return 0;
}

static int cam_jpeg_mgr_hw_flush(void *hw_mgr_priv, void *flush_hw_args)
{
	int rc = 0;
	struct cam_hw_flush_args *flush_args = flush_hw_args;
	struct cam_jpeg_hw_mgr *hw_mgr = hw_mgr_priv;
	struct cam_jpeg_hw_ctx_data *ctx_data = NULL;

	if (!hw_mgr || !flush_args || !flush_args->ctxt_to_hw_map) {
		CAM_ERR(CAM_JPEG, "Invalid args");
		return -EINVAL;
	}
	mutex_lock(&hw_mgr->hw_mgr_mutex);

	ctx_data = (struct cam_jpeg_hw_ctx_data *)flush_args->ctxt_to_hw_map;
	if (!ctx_data->in_use) {
		CAM_ERR(CAM_JPEG, "ctx is not in use");
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		return -EINVAL;
	}

	if ((flush_args->flush_type >= CAM_FLUSH_TYPE_MAX) ||
		(flush_args->flush_type < CAM_FLUSH_TYPE_REQ)) {
		CAM_ERR(CAM_JPEG, "Invalid flush type: %d",
			flush_args->flush_type);
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		return -EINVAL;
	}

	switch (flush_args->flush_type) {
	case CAM_FLUSH_TYPE_ALL:
		rc = cam_jpeg_mgr_flush(hw_mgr_priv, ctx_data);
		if ((rc))
			CAM_ERR(CAM_JPEG, "Flush failed %d", rc);
		break;
	case CAM_FLUSH_TYPE_REQ:
		rc = cam_jpeg_mgr_flush_req(hw_mgr_priv, ctx_data, flush_args);
		break;
	default:
		CAM_ERR(CAM_JPEG, "Invalid flush type: %d",
			flush_args->flush_type);
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		return -EINVAL;
	}

	mutex_unlock(&hw_mgr->hw_mgr_mutex);

	return rc;
}

static int cam_jpeg_mgr_hw_stop(void *hw_mgr_priv, void *stop_hw_args)
{
	int rc;
	struct cam_hw_stop_args *stop_args =
		(struct cam_hw_stop_args *)stop_hw_args;
	struct cam_jpeg_hw_mgr *hw_mgr = hw_mgr_priv;
	struct cam_jpeg_hw_ctx_data *ctx_data = NULL;

	if (!hw_mgr || !stop_args || !stop_args->ctxt_to_hw_map) {
		CAM_ERR(CAM_JPEG, "Invalid args");
		return -EINVAL;
	}
	mutex_lock(&hw_mgr->hw_mgr_mutex);

	ctx_data = (struct cam_jpeg_hw_ctx_data *)stop_args->ctxt_to_hw_map;
	if (!ctx_data->in_use) {
		CAM_ERR(CAM_JPEG, "ctx is not in use");
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		return -EINVAL;
	}

	rc = cam_jpeg_mgr_flush(hw_mgr_priv, ctx_data);
	if ((rc))
		CAM_ERR(CAM_JPEG, "flush failed %d", rc);

	mutex_unlock(&hw_mgr->hw_mgr_mutex);

	return rc;
}

static int cam_jpeg_mgr_release_hw(void *hw_mgr_priv, void *release_hw_args)
{
	int rc;
	struct cam_hw_release_args *release_hw = release_hw_args;
	struct cam_jpeg_hw_mgr *hw_mgr = hw_mgr_priv;
	struct cam_jpeg_hw_ctx_data *ctx_data = NULL;
	uint32_t dev_type;

	if (!hw_mgr || !release_hw || !release_hw->ctxt_to_hw_map) {
		CAM_ERR(CAM_JPEG, "Invalid args");
		return -EINVAL;
	}

	ctx_data = (struct cam_jpeg_hw_ctx_data *)release_hw->ctxt_to_hw_map;
	if (!ctx_data->in_use) {
		CAM_ERR(CAM_JPEG, "ctx is not in use");
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		return -EINVAL;
	}
	dev_type = ctx_data->jpeg_dev_acquire_info.dev_type;

	mutex_lock(&hw_mgr->hw_mgr_mutex);
	if (hw_mgr->cdm_info[dev_type][0].ref_cnt == 0) {
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		CAM_ERR(CAM_JPEG, "Error Unbalanced deinit");
		return -EFAULT;
	}

	hw_mgr->cdm_info[dev_type][0].ref_cnt--;
	if (!(hw_mgr->cdm_info[dev_type][0].ref_cnt)) {
		if (cam_cdm_stream_off(
			hw_mgr->cdm_info[dev_type][0].cdm_handle)) {
			CAM_ERR(CAM_JPEG, "CDM stream off failed %d",
				hw_mgr->cdm_info[dev_type][0].cdm_handle);
		}
		/* release cdm handle */
		cam_cdm_release(hw_mgr->cdm_info[dev_type][0].cdm_handle);
	}

	mutex_unlock(&hw_mgr->hw_mgr_mutex);

	rc = cam_jpeg_mgr_release_ctx(hw_mgr, ctx_data);
	if (rc) {
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		CAM_ERR(CAM_JPEG, "JPEG release ctx failed");
		kfree(ctx_data->cdm_cmd);
		ctx_data->cdm_cmd = NULL;

		return -EINVAL;
	}

	kfree(ctx_data->cdm_cmd);
	ctx_data->cdm_cmd = NULL;
	CAM_DBG(CAM_JPEG, "handle %llu", ctx_data);

	return rc;
}

static int cam_jpeg_mgr_acquire_hw(void *hw_mgr_priv, void *acquire_hw_args)
{
	int rc = 0;
	int32_t ctx_id = 0;
	struct cam_jpeg_hw_mgr *hw_mgr = hw_mgr_priv;
	struct cam_jpeg_hw_ctx_data *ctx_data = NULL;
	struct cam_hw_acquire_args *args = acquire_hw_args;
	struct cam_jpeg_acquire_dev_info jpeg_dev_acquire_info;
	struct cam_cdm_acquire_data cdm_acquire;
	uint32_t dev_type;
	uint32_t size = 0;

	if ((!hw_mgr_priv) || (!acquire_hw_args)) {
		CAM_ERR(CAM_JPEG, "Invalid params: %pK %pK", hw_mgr_priv,
			acquire_hw_args);
		return -EINVAL;
	}

	if (args->num_acq > 1) {
		CAM_ERR(CAM_JPEG,
			"number of resources are wrong: %u",
			args->num_acq);
		return -EINVAL;
	}

	if (copy_from_user(&jpeg_dev_acquire_info,
			(void __user *)args->acquire_info,
			sizeof(jpeg_dev_acquire_info))) {
		CAM_ERR(CAM_JPEG, "copy failed");
		return -EFAULT;
	}

	mutex_lock(&hw_mgr->hw_mgr_mutex);
	ctx_id = cam_jpeg_mgr_get_free_ctx(hw_mgr);
	if (ctx_id >= CAM_JPEG_CTX_MAX) {
		CAM_ERR(CAM_JPEG, "No free ctx space in hw_mgr");
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		return -EFAULT;
	}

	ctx_data = &hw_mgr->ctx_data[ctx_id];

	ctx_data->cdm_cmd =
		kzalloc(((sizeof(struct cam_cdm_bl_request)) +
			((CAM_JPEG_HW_ENTRIES_MAX - 1) *
			sizeof(struct cam_cdm_bl_cmd))), GFP_KERNEL);
	if (!ctx_data->cdm_cmd) {
		rc = -ENOMEM;
		goto jpeg_release_ctx;
	}

	mutex_lock(&ctx_data->ctx_mutex);
	ctx_data->jpeg_dev_acquire_info = jpeg_dev_acquire_info;
	mutex_unlock(&ctx_data->ctx_mutex);

	if (ctx_data->jpeg_dev_acquire_info.dev_type >=
		CAM_JPEG_RES_TYPE_MAX) {
		rc = -EINVAL;
		goto acq_cdm_hdl_failed;
	}
	dev_type = ctx_data->jpeg_dev_acquire_info.dev_type;
	if (!hw_mgr->cdm_info[dev_type][0].ref_cnt) {

		if (dev_type == CAM_JPEG_RES_TYPE_ENC) {
			memcpy(cdm_acquire.identifier,
				"jpegenc", sizeof("jpegenc"));
		} else {
			memcpy(cdm_acquire.identifier,
				"jpegdma", sizeof("jpegdma"));
		}
		cdm_acquire.cell_index = 0;
		cdm_acquire.handle = 0;
		cdm_acquire.userdata = ctx_data;
		if (hw_mgr->cdm_reg_map[dev_type][0]) {
			cdm_acquire.base_array[0] =
				hw_mgr->cdm_reg_map[dev_type][0];
		}
		cdm_acquire.base_array_cnt = 1;
		cdm_acquire.id = CAM_CDM_VIRTUAL;
		cdm_acquire.cam_cdm_callback = NULL;

		rc = cam_cdm_acquire(&cdm_acquire);
		if (rc) {
			CAM_ERR(CAM_JPEG, "Failed to acquire the CDM HW %d",
				rc);
			rc = -EFAULT;
			goto acq_cdm_hdl_failed;
		}
		hw_mgr->cdm_info[dev_type][0].cdm_handle = cdm_acquire.handle;
		hw_mgr->cdm_info[dev_type][0].cdm_ops = cdm_acquire.ops;
		hw_mgr->cdm_info[dev_type][0].ref_cnt++;
	} else {
		hw_mgr->cdm_info[dev_type][0].ref_cnt++;
	}

	size =
	hw_mgr->cdm_info[dev_type][0].cdm_ops->cdm_required_size_changebase();

	if (hw_mgr->cdm_info[dev_type][0].ref_cnt == 1)
		if (cam_cdm_stream_on(
			hw_mgr->cdm_info[dev_type][0].cdm_handle)) {
			CAM_ERR(CAM_JPEG, "Can not start cdm (%d)!",
				hw_mgr->cdm_info[dev_type][0].cdm_handle);
			rc = -EFAULT;
			goto start_cdm_hdl_failed;
		}

	mutex_lock(&ctx_data->ctx_mutex);
	ctx_data->context_priv = args->context_data;

	args->ctxt_to_hw_map = (void *)&(hw_mgr->ctx_data[ctx_id]);

	mutex_unlock(&ctx_data->ctx_mutex);

	hw_mgr->ctx_data[ctx_id].ctxt_event_cb = args->event_cb;


	if (copy_to_user((void __user *)args->acquire_info,
		&jpeg_dev_acquire_info,
		sizeof(jpeg_dev_acquire_info))) {
		rc = -EFAULT;
		goto copy_to_user_failed;
	}
	mutex_unlock(&hw_mgr->hw_mgr_mutex);

	CAM_DBG(CAM_JPEG, "success ctx_data= %pK", ctx_data);

	return rc;

copy_to_user_failed:
	if (hw_mgr->cdm_info[dev_type][0].ref_cnt == 1)
		cam_cdm_stream_off(hw_mgr->cdm_info[dev_type][0].cdm_handle);
start_cdm_hdl_failed:
	if (hw_mgr->cdm_info[dev_type][0].ref_cnt == 1)
		cam_cdm_release(hw_mgr->cdm_info[dev_type][0].cdm_handle);
	hw_mgr->cdm_info[dev_type][0].ref_cnt--;
acq_cdm_hdl_failed:
	kfree(ctx_data->cdm_cmd);
jpeg_release_ctx:
	cam_jpeg_mgr_release_ctx(hw_mgr, ctx_data);
	mutex_unlock(&hw_mgr->hw_mgr_mutex);

	return rc;
}

static int cam_jpeg_mgr_get_hw_caps(void *hw_mgr_priv, void *hw_caps_args)
{
	int rc;
	struct cam_jpeg_hw_mgr *hw_mgr = hw_mgr_priv;
	struct cam_query_cap_cmd *query_cap = hw_caps_args;

	if (!hw_mgr_priv || !hw_caps_args) {
		CAM_ERR(CAM_JPEG, "Invalid params: %pK %pK",
			hw_mgr_priv, hw_caps_args);
		return -EINVAL;
	}

	mutex_lock(&hw_mgr->hw_mgr_mutex);

	if (copy_to_user(u64_to_user_ptr(query_cap->caps_handle),
		&g_jpeg_hw_mgr.jpeg_caps,
		sizeof(struct cam_jpeg_query_cap_cmd))) {
		CAM_ERR(CAM_JPEG, "copy_to_user failed");
		rc = -EFAULT;
		goto copy_error;
	}
	CAM_DBG(CAM_JPEG, "Success");
	mutex_unlock(&hw_mgr->hw_mgr_mutex);

	return 0;

copy_error:
	mutex_unlock(&hw_mgr->hw_mgr_mutex);
	return rc;
}

static int cam_jpeg_setup_workqs(void)
{
	int rc, i;

	rc = cam_req_mgr_workq_create(
		"jpeg_command_queue",
		CAM_JPEG_WORKQ_NUM_TASK,
		&g_jpeg_hw_mgr.work_process_frame,
		CRM_WORKQ_USAGE_NON_IRQ, 0);
	if (rc) {
		CAM_ERR(CAM_JPEG, "unable to create a worker %d", rc);
		goto work_process_frame_failed;
	}

	rc = cam_req_mgr_workq_create(
		"jpeg_message_queue",
		CAM_JPEG_WORKQ_NUM_TASK,
		&g_jpeg_hw_mgr.work_process_irq_cb,
		CRM_WORKQ_USAGE_IRQ, 0);
	if (rc) {
		CAM_ERR(CAM_JPEG, "unable to create a worker %d", rc);
		goto work_process_irq_cb_failed;
	}

	g_jpeg_hw_mgr.process_frame_work_data =
		(struct cam_jpeg_process_frame_work_data_t *)
		kzalloc(sizeof(struct cam_jpeg_process_frame_work_data_t) *
			CAM_JPEG_WORKQ_NUM_TASK, GFP_KERNEL);
	if (!g_jpeg_hw_mgr.process_frame_work_data) {
		rc = -ENOMEM;
		goto work_process_frame_data_failed;
	}

	g_jpeg_hw_mgr.process_irq_cb_work_data =
		(struct cam_jpeg_process_irq_work_data_t *)
		kzalloc(sizeof(struct cam_jpeg_process_irq_work_data_t) *
			CAM_JPEG_WORKQ_NUM_TASK, GFP_KERNEL);
	if (!g_jpeg_hw_mgr.process_irq_cb_work_data) {
		rc = -ENOMEM;
		goto work_process_irq_cb_data_failed;
	}

	for (i = 0; i < CAM_JPEG_WORKQ_NUM_TASK; i++)
		g_jpeg_hw_mgr.work_process_irq_cb->task.pool[i].payload =
			&g_jpeg_hw_mgr.process_irq_cb_work_data[i];

	for (i = 0; i < CAM_JPEG_WORKQ_NUM_TASK; i++)
		g_jpeg_hw_mgr.work_process_frame->task.pool[i].payload =
			&g_jpeg_hw_mgr.process_frame_work_data[i];

	INIT_LIST_HEAD(&g_jpeg_hw_mgr.hw_config_req_list);
	INIT_LIST_HEAD(&g_jpeg_hw_mgr.free_req_list);
	for (i = 0; i < CAM_JPEG_HW_CFG_Q_MAX; i++) {
		INIT_LIST_HEAD(&(g_jpeg_hw_mgr.req_list[i].list));
		list_add_tail(&(g_jpeg_hw_mgr.req_list[i].list),
			&(g_jpeg_hw_mgr.free_req_list));
	}

	return rc;

work_process_irq_cb_data_failed:
	kfree(g_jpeg_hw_mgr.process_frame_work_data);
work_process_frame_data_failed:
	cam_req_mgr_workq_destroy(&g_jpeg_hw_mgr.work_process_irq_cb);
work_process_irq_cb_failed:
	cam_req_mgr_workq_destroy(&g_jpeg_hw_mgr.work_process_frame);
work_process_frame_failed:

	return rc;
}

static int cam_jpeg_init_devices(struct device_node *of_node,
	uint32_t *p_num_enc_dev,
	uint32_t *p_num_dma_dev)
{
	int count, i, rc;
	uint32_t num_dev;
	uint32_t num_dma_dev;
	const char *name = NULL;
	struct device_node *child_node = NULL;
	struct platform_device *child_pdev = NULL;
	struct cam_hw_intf *child_dev_intf = NULL;
	struct cam_hw_info *enc_hw = NULL;
	struct cam_hw_info *dma_hw = NULL;
	struct cam_hw_soc_info *enc_soc_info = NULL;
	struct cam_hw_soc_info *dma_soc_info = NULL;

	if (!p_num_enc_dev || !p_num_dma_dev) {
		rc = -EINVAL;
		goto num_dev_failed;
	}
	count = of_property_count_strings(of_node, "compat-hw-name");
	if (!count) {
		CAM_ERR(CAM_JPEG,
			"no compat hw found in dev tree, count = %d",
			count);
		rc = -EINVAL;
		goto num_dev_failed;
	}

	rc = of_property_read_u32(of_node, "num-jpeg-enc", &num_dev);
	if (rc) {
		CAM_ERR(CAM_JPEG, "read num enc devices failed %d", rc);
		goto num_enc_failed;
	}
	g_jpeg_hw_mgr.devices[CAM_JPEG_DEV_ENC] = kzalloc(
		sizeof(struct cam_hw_intf *) * num_dev, GFP_KERNEL);
	if (!g_jpeg_hw_mgr.devices[CAM_JPEG_DEV_ENC]) {
		rc = -ENOMEM;
		CAM_ERR(CAM_JPEG, "getting number of dma dev nodes failed");
		goto num_enc_failed;
	}

	rc = of_property_read_u32(of_node, "num-jpeg-dma", &num_dma_dev);
	if (rc) {
		CAM_ERR(CAM_JPEG, "get num dma dev nodes failed %d", rc);
		goto num_dma_failed;
	}

	g_jpeg_hw_mgr.devices[CAM_JPEG_DEV_DMA] = kzalloc(
		sizeof(struct cam_hw_intf *) * num_dma_dev, GFP_KERNEL);
	if (!g_jpeg_hw_mgr.devices[CAM_JPEG_DEV_DMA]) {
		rc = -ENOMEM;
		goto num_dma_failed;
	}

	for (i = 0; i < count; i++) {
		rc = of_property_read_string_index(of_node, "compat-hw-name",
			i, &name);
		if (rc) {
			CAM_ERR(CAM_JPEG, "getting dev object name failed");
			goto compat_hw_name_failed;
		}

		child_node = of_find_node_by_name(NULL, name);
		if (!child_node) {
			CAM_ERR(CAM_JPEG,
				"error! Cannot find node in dtsi %s", name);
			rc = -ENODEV;
			goto compat_hw_name_failed;
		}

		child_pdev = of_find_device_by_node(child_node);
		if (!child_pdev) {
			CAM_ERR(CAM_JPEG, "failed to find device on bus %s",
				child_node->name);
			rc = -ENODEV;
			of_node_put(child_node);
			goto compat_hw_name_failed;
		}

		child_dev_intf = (struct cam_hw_intf *)platform_get_drvdata(
			child_pdev);
		if (!child_dev_intf) {
			CAM_ERR(CAM_JPEG, "no child device");
			of_node_put(child_node);
			rc = -ENODEV;
			goto compat_hw_name_failed;
		}
		CAM_DBG(CAM_JPEG, "child_intf %pK type %d id %d",
			child_dev_intf,
			child_dev_intf->hw_type,
			child_dev_intf->hw_idx);

		if ((child_dev_intf->hw_type == CAM_JPEG_DEV_ENC &&
			child_dev_intf->hw_idx >= num_dev) ||
			(child_dev_intf->hw_type == CAM_JPEG_DEV_DMA &&
			child_dev_intf->hw_idx >= num_dma_dev)) {
			CAM_ERR(CAM_JPEG, "index out of range");
			rc = -ENODEV;
			goto compat_hw_name_failed;
		}
		g_jpeg_hw_mgr.devices[child_dev_intf->hw_type]
			[child_dev_intf->hw_idx] = child_dev_intf;

		of_node_put(child_node);
	}

	enc_hw = (struct cam_hw_info *)
		g_jpeg_hw_mgr.devices[CAM_JPEG_DEV_ENC][0]->hw_priv;
	enc_soc_info = &enc_hw->soc_info;
	g_jpeg_hw_mgr.cdm_reg_map[CAM_JPEG_DEV_ENC][0] =
		&enc_soc_info->reg_map[0];
	dma_hw = (struct cam_hw_info *)
		g_jpeg_hw_mgr.devices[CAM_JPEG_DEV_DMA][0]->hw_priv;
	dma_soc_info = &dma_hw->soc_info;
	g_jpeg_hw_mgr.cdm_reg_map[CAM_JPEG_DEV_DMA][0] =
		&dma_soc_info->reg_map[0];

	*p_num_enc_dev = num_dev;
	*p_num_dma_dev = num_dma_dev;

	return rc;

compat_hw_name_failed:
	kfree(g_jpeg_hw_mgr.devices[CAM_JPEG_DEV_DMA]);
num_dma_failed:
	kfree(g_jpeg_hw_mgr.devices[CAM_JPEG_DEV_ENC]);
num_enc_failed:
num_dev_failed:

	return rc;
}

static int cam_jpeg_mgr_cmd(void *hw_mgr_priv, void *cmd_args)
{
	int rc = 0;
	struct cam_hw_cmd_args *hw_cmd_args = cmd_args;
	struct cam_jpeg_hw_mgr *hw_mgr = hw_mgr_priv;

	if (!hw_mgr_priv || !cmd_args) {
		CAM_ERR(CAM_JPEG, "Invalid arguments");
		return -EINVAL;
	}

	switch (hw_cmd_args->cmd_type) {
	case CAM_HW_MGR_CMD_DUMP_PF_INFO:
		cam_jpeg_mgr_print_io_bufs(
			hw_cmd_args->u.pf_args.pf_data.packet,
			hw_mgr->iommu_hdl,
			hw_mgr->iommu_sec_hdl,
			hw_cmd_args->u.pf_args.buf_info,
			hw_cmd_args->u.pf_args.mem_found);
		break;
	default:
		CAM_ERR(CAM_JPEG, "Invalid cmd");
	}

	return rc;
}

int cam_jpeg_hw_mgr_init(struct device_node *of_node, uint64_t *hw_mgr_hdl,
	int *iommu_hdl)
{
	int i, rc;
	uint32_t num_dev;
	uint32_t num_dma_dev;
	struct cam_hw_mgr_intf *hw_mgr_intf;
	struct cam_iommu_handle cdm_handles;

	hw_mgr_intf = (struct cam_hw_mgr_intf *)hw_mgr_hdl;
	if (!of_node || !hw_mgr_intf) {
		CAM_ERR(CAM_JPEG, "Invalid args of_node %pK hw_mgr %pK",
			of_node, hw_mgr_intf);
		return -EINVAL;
	}

	memset(hw_mgr_hdl, 0x0, sizeof(struct cam_hw_mgr_intf));
	hw_mgr_intf->hw_mgr_priv = &g_jpeg_hw_mgr;
	hw_mgr_intf->hw_get_caps = cam_jpeg_mgr_get_hw_caps;
	hw_mgr_intf->hw_acquire = cam_jpeg_mgr_acquire_hw;
	hw_mgr_intf->hw_release = cam_jpeg_mgr_release_hw;
	hw_mgr_intf->hw_prepare_update = cam_jpeg_mgr_prepare_hw_update;
	hw_mgr_intf->hw_config = cam_jpeg_mgr_config_hw;
	hw_mgr_intf->hw_flush = cam_jpeg_mgr_hw_flush;
	hw_mgr_intf->hw_stop = cam_jpeg_mgr_hw_stop;
	hw_mgr_intf->hw_cmd = cam_jpeg_mgr_cmd;

	mutex_init(&g_jpeg_hw_mgr.hw_mgr_mutex);
	spin_lock_init(&g_jpeg_hw_mgr.hw_mgr_lock);

	for (i = 0; i < CAM_JPEG_CTX_MAX; i++)
		mutex_init(&g_jpeg_hw_mgr.ctx_data[i].ctx_mutex);

	rc = cam_jpeg_init_devices(of_node, &num_dev, &num_dma_dev);
	if (rc) {
		CAM_ERR(CAM_JPEG, "jpeg init devices %d", rc);
		goto smmu_get_failed;
	}

	rc = cam_smmu_get_handle("jpeg", &g_jpeg_hw_mgr.iommu_hdl);
	if (rc) {
		CAM_ERR(CAM_JPEG, "jpeg get iommu handle failed %d", rc);
		goto smmu_get_failed;
	}

	CAM_DBG(CAM_JPEG, "mmu handle :%d", g_jpeg_hw_mgr.iommu_hdl);
	rc = cam_smmu_ops(g_jpeg_hw_mgr.iommu_hdl, CAM_SMMU_ATTACH);
	if (rc) {
		CAM_ERR(CAM_JPEG, "jpeg attach failed: %d", rc);
		goto jpeg_attach_failed;
	}

	rc = cam_cdm_get_iommu_handle("jpegenc", &cdm_handles);
	if (rc) {
		CAM_ERR(CAM_JPEG, "acquire cdm iommu handle Fail  %d", rc);
		g_jpeg_hw_mgr.cdm_iommu_hdl = -1;
		g_jpeg_hw_mgr.cdm_iommu_hdl_secure = -1;
		goto cdm_iommu_failed;
	}
	g_jpeg_hw_mgr.cdm_iommu_hdl = cdm_handles.non_secure;
	g_jpeg_hw_mgr.cdm_iommu_hdl_secure = cdm_handles.secure;

	g_jpeg_hw_mgr.jpeg_caps.dev_iommu_handle.non_secure =
		g_jpeg_hw_mgr.iommu_hdl;
	g_jpeg_hw_mgr.jpeg_caps.dev_iommu_handle.secure =
		g_jpeg_hw_mgr.iommu_sec_hdl;
	g_jpeg_hw_mgr.jpeg_caps.cdm_iommu_handle.non_secure =
		g_jpeg_hw_mgr.cdm_iommu_hdl;
	g_jpeg_hw_mgr.jpeg_caps.cdm_iommu_handle.secure =
		g_jpeg_hw_mgr.cdm_iommu_hdl_secure;
	g_jpeg_hw_mgr.jpeg_caps.num_enc = num_dev;
	g_jpeg_hw_mgr.jpeg_caps.num_dma = num_dma_dev;
	g_jpeg_hw_mgr.jpeg_caps.dev_ver[CAM_JPEG_DEV_ENC].hw_ver.major = 4;
	g_jpeg_hw_mgr.jpeg_caps.dev_ver[CAM_JPEG_DEV_ENC].hw_ver.minor = 2;
	g_jpeg_hw_mgr.jpeg_caps.dev_ver[CAM_JPEG_DEV_ENC].hw_ver.incr  = 0;
	g_jpeg_hw_mgr.jpeg_caps.dev_ver[CAM_JPEG_DEV_ENC].hw_ver.reserved = 0;
	g_jpeg_hw_mgr.jpeg_caps.dev_ver[CAM_JPEG_DEV_DMA].hw_ver.major = 4;
	g_jpeg_hw_mgr.jpeg_caps.dev_ver[CAM_JPEG_DEV_DMA].hw_ver.minor = 2;
	g_jpeg_hw_mgr.jpeg_caps.dev_ver[CAM_JPEG_DEV_DMA].hw_ver.incr  = 0;
	g_jpeg_hw_mgr.jpeg_caps.dev_ver[CAM_JPEG_DEV_DMA].hw_ver.reserved = 0;

	rc = cam_jpeg_setup_workqs();
	if (rc) {
		CAM_ERR(CAM_JPEG, "setup work qs failed  %d", rc);
		goto cdm_iommu_failed;
	}

	if (iommu_hdl)
		*iommu_hdl = g_jpeg_hw_mgr.iommu_hdl;

	return rc;

cdm_iommu_failed:
	cam_smmu_ops(g_jpeg_hw_mgr.iommu_hdl, CAM_SMMU_DETACH);
	cam_smmu_destroy_handle(g_jpeg_hw_mgr.iommu_hdl);
jpeg_attach_failed:
	g_jpeg_hw_mgr.iommu_hdl = 0;
smmu_get_failed:
	mutex_destroy(&g_jpeg_hw_mgr.hw_mgr_mutex);
	for (i = 0; i < CAM_JPEG_CTX_MAX; i++)
		mutex_destroy(&g_jpeg_hw_mgr.ctx_data[i].ctx_mutex);

	return rc;
}
