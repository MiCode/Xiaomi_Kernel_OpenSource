// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/io.h>
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
#include <media/cam_ope.h>
#include <media/cam_cpas.h>

#include "cam_sync_api.h"
#include "cam_packet_util.h"
#include "cam_hw.h"
#include "cam_hw_mgr_intf.h"
#include "cam_ope_hw_mgr_intf.h"
#include "cam_ope_hw_mgr.h"
#include "ope_hw.h"
#include "cam_smmu_api.h"
#include "cam_mem_mgr.h"
#include "cam_req_mgr_workq.h"
#include "cam_mem_mgr.h"
#include "cam_debug_util.h"
#include "cam_soc_util.h"
#include "cam_trace.h"
#include "cam_cpas_api.h"
#include "cam_common_util.h"
#include "cam_cdm_intf_api.h"
#include "cam_cdm_util.h"
#include "cam_cdm.h"
#include "ope_dev_intf.h"

static struct cam_ope_hw_mgr *ope_hw_mgr;

static int cam_ope_req_timer_reset(struct cam_ope_ctx *ctx_data);

static int cam_ope_mgr_get_rsc_idx(struct cam_ope_ctx *ctx_data,
	struct ope_io_buf_info *in_io_buf)
{
	int k = 0;
	int rsc_idx = -EINVAL;

	if (in_io_buf->direction == CAM_BUF_INPUT) {
		for (k = 0; k < OPE_IN_RES_MAX; k++) {
			if (ctx_data->ope_acquire.in_res[k].res_id ==
				in_io_buf->resource_type)
				break;
		}
		if (k == OPE_IN_RES_MAX) {
			CAM_ERR(CAM_OPE, "Invalid res_id %d",
				in_io_buf->resource_type);
			goto end;
		}
		rsc_idx = k;
	} else if (in_io_buf->direction == CAM_BUF_OUTPUT) {
		for (k = 0; k < OPE_OUT_RES_MAX; k++) {
			if (ctx_data->ope_acquire.out_res[k].res_id ==
				in_io_buf->resource_type)
				break;
		}
		if (k == OPE_OUT_RES_MAX) {
			CAM_ERR(CAM_OPE, "Invalid res_id %d",
				in_io_buf->resource_type);
			goto end;
		}
		rsc_idx = k;
	}

end:
	return rsc_idx;
}

static int cam_ope_mgr_process_cmd(void *priv, void *data)
{
	int rc;
	struct ope_cmd_work_data *task_data = NULL;
	struct cam_ope_ctx *ctx_data;
	struct cam_cdm_bl_request *cdm_cmd;
	struct cam_ope_hw_mgr *hw_mgr = ope_hw_mgr;

	if (!data || !priv) {
		CAM_ERR(CAM_OPE, "Invalid params%pK %pK", data, priv);
		return -EINVAL;
	}

	ctx_data = priv;
	task_data = (struct ope_cmd_work_data *)data;
	cdm_cmd = task_data->data;

	if (!cdm_cmd) {
		CAM_ERR(CAM_OPE, "Invalid params%pK", cdm_cmd);
		return -EINVAL;
	}

	mutex_lock(&hw_mgr->hw_mgr_mutex);
	if (ctx_data->ctx_state != OPE_CTX_STATE_ACQUIRED) {
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		CAM_ERR(CAM_OPE, "ctx id :%u is not in use",
			ctx_data->ctx_id);
		return -EINVAL;
	}

	if (task_data->req_id <= ctx_data->last_flush_req) {
		CAM_WARN(CAM_OPE,
			"request %lld has been flushed, reject packet",
			task_data->req_id, ctx_data->last_flush_req);
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		return -EINVAL;
	}

	CAM_DBG(CAM_OPE,
		"cam_cdm_submit_bls: handle 0x%x, ctx_id %d req %d cookie %d",
		ctx_data->ope_cdm.cdm_handle, ctx_data->ctx_id,
		task_data->req_id, cdm_cmd->cookie);

	if (task_data->req_id > ctx_data->last_flush_req)
		ctx_data->last_flush_req = 0;

	cam_ope_req_timer_reset(ctx_data);

	rc = cam_cdm_submit_bls(ctx_data->ope_cdm.cdm_handle, cdm_cmd);

	if (!rc)
		ctx_data->req_cnt++;
	else
		CAM_ERR(CAM_OPE, "submit failed for %lld", cdm_cmd->cookie);

	mutex_unlock(&hw_mgr->hw_mgr_mutex);

	return rc;
}

static int cam_ope_mgr_reset_hw(void)
{
	struct cam_ope_hw_mgr *hw_mgr = ope_hw_mgr;
	int i, rc = 0;

	for (i = 0; i < ope_hw_mgr->num_ope; i++) {
		rc = hw_mgr->ope_dev_intf[i]->hw_ops.process_cmd(
			hw_mgr->ope_dev_intf[i]->hw_priv, OPE_HW_RESET,
			NULL, 0);
		if (rc) {
			CAM_ERR(CAM_OPE, "OPE Reset failed: %d", rc);
			return rc;
		}
	}

	return rc;
}

static void cam_ope_free_io_config(struct cam_ope_request *req)
{
	int i, j;

	for (i = 0; i < OPE_MAX_BATCH_SIZE; i++) {
		for (j = 0; j < OPE_MAX_IO_BUFS; j++) {
			if (req->io_buf[i][j]) {
				kzfree(req->io_buf[i][j]);
				req->io_buf[i][j] = NULL;
			}
		}
	}
}

static void cam_ope_device_timer_stop(struct cam_ope_hw_mgr *hw_mgr)
{
	if (hw_mgr->clk_info.watch_dog) {
		hw_mgr->clk_info.watch_dog_reset_counter = 0;
		crm_timer_exit(&hw_mgr->clk_info.watch_dog);
		hw_mgr->clk_info.watch_dog = NULL;
	}
}

static void cam_ope_device_timer_reset(struct cam_ope_hw_mgr *hw_mgr)
{

	if (hw_mgr->clk_info.watch_dog) {
		CAM_DBG(CAM_OPE, "reset timer");
		crm_timer_reset(hw_mgr->clk_info.watch_dog);
			hw_mgr->clk_info.watch_dog_reset_counter++;
	}
}

static int cam_ope_req_timer_modify(struct cam_ope_ctx *ctx_data,
	int32_t expires)
{
	if (ctx_data->req_watch_dog) {
		CAM_DBG(CAM_OPE, "stop timer : ctx_id = %d", ctx_data->ctx_id);
		crm_timer_modify(ctx_data->req_watch_dog, expires);
	}
	return 0;
}

static int cam_ope_req_timer_stop(struct cam_ope_ctx *ctx_data)
{
	if (ctx_data->req_watch_dog) {
		CAM_DBG(CAM_OPE, "stop timer : ctx_id = %d", ctx_data->ctx_id);
		ctx_data->req_watch_dog_reset_counter = 0;
		crm_timer_exit(&ctx_data->req_watch_dog);
		ctx_data->req_watch_dog = NULL;
	}
	return 0;
}

static int cam_ope_req_timer_reset(struct cam_ope_ctx *ctx_data)
{
	if (ctx_data && ctx_data->req_watch_dog) {
		ctx_data->req_watch_dog_reset_counter++;
		CAM_DBG(CAM_OPE, "reset timer : ctx_id = %d, counter=%d",
			ctx_data->ctx_id,
			ctx_data->req_watch_dog_reset_counter);
		crm_timer_reset(ctx_data->req_watch_dog);
	}

	return 0;
}


static int cam_ope_mgr_reapply_config(struct cam_ope_hw_mgr *hw_mgr,
	struct cam_ope_ctx *ctx_data,
	struct cam_ope_request *ope_req)
{
	int rc = 0;
	uint64_t request_id = 0;
	struct crm_workq_task *task;
	struct ope_cmd_work_data *task_data;

	request_id = ope_req->request_id;
	CAM_DBG(CAM_OPE, "reapply req_id = %lld", request_id);

	task = cam_req_mgr_workq_get_task(ope_hw_mgr->cmd_work);
	if (!task) {
		CAM_ERR(CAM_OPE, "no empty task");
		return -ENOMEM;
	}

	task_data = (struct ope_cmd_work_data *)task->payload;
	task_data->data = (void *)ope_req->cdm_cmd;
	task_data->req_id = request_id;
	task_data->type = OPE_WORKQ_TASK_CMD_TYPE;
	task->process_cb = cam_ope_mgr_process_cmd;
	rc = cam_req_mgr_workq_enqueue_task(task, ctx_data,
		CRM_TASK_PRIORITY_0);

	return rc;
}

static bool cam_ope_is_pending_request(struct cam_ope_ctx *ctx_data)
{
	return !bitmap_empty(ctx_data->bitmap, CAM_CTX_REQ_MAX);
}

static int cam_get_valid_ctx_id(void)
{
	struct cam_ope_hw_mgr *hw_mgr = ope_hw_mgr;
	int i;

	for (i = 0; i < OPE_CTX_MAX; i++) {
		if (hw_mgr->ctx[i].ctx_state == OPE_CTX_STATE_ACQUIRED)
			break;
	}

	if (i == OPE_CTX_MAX)
		return -EINVAL;

	return i;
}

static int32_t cam_ope_mgr_process_msg(void *priv, void *data)
{
	struct ope_msg_work_data *task_data;
	struct cam_ope_hw_mgr *hw_mgr;
	struct cam_ope_ctx *ctx;
	uint32_t irq_status;
	int32_t ctx_id;
	int rc = 0, i;

	if (!data || !priv) {
		CAM_ERR(CAM_OPE, "Invalid data");
		return -EINVAL;
	}

	task_data = data;
	hw_mgr = priv;
	irq_status = task_data->irq_status;
	ctx_id = cam_get_valid_ctx_id();
	if (ctx_id < 0) {
		CAM_ERR(CAM_OPE, "No valid context to handle error");
		return ctx_id;
	}

	ctx = &hw_mgr->ctx[ctx_id];

	/* Indicate about this error to CDM and reset OPE*/
	rc = cam_cdm_handle_error(ctx->ope_cdm.cdm_handle);

	mutex_lock(&ctx->ctx_mutex);
	if (ctx->ctx_state != OPE_CTX_STATE_ACQUIRED) {
		CAM_DBG(CAM_OPE, "ctx id: %d not in right state: %d",
			ctx_id, ctx->ctx_state);
		mutex_unlock(&ctx->ctx_mutex);
		return -EINVAL;
	}

	for (i = 0; i < hw_mgr->num_ope; i++) {
		rc = hw_mgr->ope_dev_intf[i]->hw_ops.process_cmd(
			hw_mgr->ope_dev_intf[i]->hw_priv, OPE_HW_RESET,
			NULL, 0);
		if (rc)
			CAM_ERR(CAM_OPE, "OPE Dev acquire failed: %d", rc);
	}

	mutex_unlock(&ctx->ctx_mutex);
	return rc;
}

static int cam_ope_dump_hang_patches(struct cam_packet *packet,
	struct cam_ope_hang_dump *dump)
{
	struct cam_patch_desc *patch_desc = NULL;
	dma_addr_t iova_addr;
	size_t     src_buf_size;
	int        i, rc = 0;
	int32_t iommu_hdl = ope_hw_mgr->iommu_hdl;

	/* process patch descriptor */
	patch_desc = (struct cam_patch_desc *)
		((uint32_t *) &packet->payload +
		packet->patch_offset/4);

	for (i = 0; i < packet->num_patches; i++) {
		rc = cam_mem_get_io_buf(patch_desc[i].src_buf_hdl,
			iommu_hdl, &iova_addr, &src_buf_size);
		if (rc < 0) {
			CAM_ERR(CAM_UTIL,
				"get src buf address failed for handle 0x%x",
				patch_desc[i].src_buf_hdl);
			return rc;
		}
		dump->entries[dump->num_bufs].memhdl =
			patch_desc[i].src_buf_hdl;
		dump->entries[dump->num_bufs].iova   = iova_addr;
		dump->entries[dump->num_bufs].offset = patch_desc[i].src_offset;
		dump->entries[dump->num_bufs].len    = 0;
		dump->entries[dump->num_bufs].size   = src_buf_size;
		dump->num_bufs++;
	}
	return rc;
}

static int cam_ope_dump_direct(struct ope_cmd_buf_info *cmd_buf_info,
	struct cam_ope_hang_dump *dump)
{
	dma_addr_t iova_addr;
	size_t size;
	int rc = 0;

	rc = cam_mem_get_io_buf(cmd_buf_info->mem_handle,
		ope_hw_mgr->iommu_hdl, &iova_addr, &size);
	if (rc < 0) {
		CAM_ERR(CAM_UTIL, "get cmd buf addressfailed for handle 0x%x",
			cmd_buf_info->mem_handle);
		return rc;
	}
	dump->entries[dump->num_bufs].memhdl = cmd_buf_info->mem_handle;
	dump->entries[dump->num_bufs].iova   = iova_addr;
	dump->entries[dump->num_bufs].offset = cmd_buf_info->offset;
	dump->entries[dump->num_bufs].len    = cmd_buf_info->length;
	dump->entries[dump->num_bufs].size   = size;
	dump->num_bufs++;
	return 0;
}

static void cam_ope_dump_dmi(struct cam_ope_hang_dump *dump, uint32_t addr,
	uint32_t length)
{
	int i;
	uint32_t memhdl = 0, iova = 0, size;

	for (i = 0; i < dump->num_bufs; i++) {
		if (dump->entries[i].iova + dump->entries[i].offset == addr) {
			if (dump->entries[i].len == length)
				goto end;
			else if (dump->entries[i].len == 0) {
				dump->entries[i].len = length;
				goto end;
			} else {
				iova = dump->entries[i].iova;
				memhdl = dump->entries[i].memhdl;
				size = dump->entries[i].size;
			}
		}
	}
	if (memhdl && iova) {
		dump->entries[dump->num_bufs].memhdl = memhdl;
		dump->entries[dump->num_bufs].iova = iova;
		dump->entries[dump->num_bufs].offset = addr - iova;
		dump->entries[dump->num_bufs].len = length;
		dump->entries[dump->num_bufs].size = size;
		dump->num_bufs++;
	}
end:
	return;
}

static int cam_ope_dump_indirect(struct ope_cmd_buf_info *cmd_buf_info,
	struct cam_ope_hang_dump *dump)
{
	int rc = 0;
	uintptr_t cpu_addr;
	size_t buf_len;
	uint32_t num_dmi;
	struct cdm_dmi_cmd dmi_cmd;
	uint32_t *print_ptr, print_idx;

	rc = cam_mem_get_cpu_buf(cmd_buf_info->mem_handle,
		&cpu_addr, &buf_len);
	if (rc || !cpu_addr) {
		CAM_ERR(CAM_OPE, "get cmd buf fail 0x%x",
			cmd_buf_info->mem_handle);
		return rc;
	}
	cpu_addr = cpu_addr + cmd_buf_info->offset;

	num_dmi = cmd_buf_info->length /
		sizeof(struct cdm_dmi_cmd);
	print_ptr = (uint32_t *)cpu_addr;
	for (print_idx = 0; print_idx < num_dmi; print_idx++) {
		memcpy(&dmi_cmd, (const void *)print_ptr,
			sizeof(struct cdm_dmi_cmd));
		cam_ope_dump_dmi(dump, dmi_cmd.addr, dmi_cmd.length+1);
			print_ptr += sizeof(struct cdm_dmi_cmd) /
				sizeof(uint32_t);
	}
	return rc;
}

static int cam_ope_mgr_dump_cmd_buf(uintptr_t frame_process_addr,
	struct cam_ope_hang_dump *dump)
{
	int rc = 0;
	int i, j;
	struct ope_frame_process *frame_process;
	struct ope_cmd_buf_info *cmd_buf;

	frame_process = (struct ope_frame_process *)frame_process_addr;
	for (i = 0; i < frame_process->batch_size; i++) {
		for (j = 0; j < frame_process->num_cmd_bufs[i]; j++) {
			cmd_buf = &frame_process->cmd_buf[i][j];
			if (cmd_buf->type == OPE_CMD_BUF_TYPE_DIRECT) {
				if (cmd_buf->cmd_buf_usage == OPE_CMD_BUF_DEBUG)
					continue;
				cam_ope_dump_direct(cmd_buf, dump);
			} else if (cmd_buf->type == OPE_CMD_BUF_TYPE_INDIRECT)
				cam_ope_dump_indirect(cmd_buf, dump);
		}
	}
	return rc;
}

static int cam_ope_mgr_dump_frame_set(uintptr_t frame_process_addr,
	struct cam_ope_hang_dump *dump)
{
	int i, j, rc = 0;
	dma_addr_t iova_addr;
	size_t size;
	struct ope_frame_process *frame_process;
	struct ope_io_buf_info *io_buf;
	struct cam_ope_buf_entry *buf_entry;
	struct cam_ope_output_info *output_info;

	frame_process = (struct ope_frame_process *)frame_process_addr;
	for (j = 0; j < frame_process->batch_size; j++) {
		for (i = 0; i < frame_process->frame_set[j].num_io_bufs; i++) {
			io_buf = &frame_process->frame_set[j].io_buf[i];
			rc = cam_mem_get_io_buf(io_buf->mem_handle[0],
				ope_hw_mgr->iommu_hdl, &iova_addr, &size);
			if (rc) {
				CAM_ERR(CAM_OPE, "get io buf fail 0x%x",
					io_buf->mem_handle[0]);
				return rc;
			}
			buf_entry = &dump->entries[dump->num_bufs];
			buf_entry->memhdl = io_buf->mem_handle[0];
			buf_entry->iova = iova_addr;
			buf_entry->offset = io_buf->plane_offset[0];
			buf_entry->len = size - io_buf->plane_offset[0];
			buf_entry->size = size;
			dump->num_bufs++;
			if (io_buf->direction == 2) {
				output_info =
					&dump->outputs[dump->num_outputs];
				output_info->iova = iova_addr;
				output_info->offset = io_buf->plane_offset[0];
				output_info->len = size -
					io_buf->plane_offset[0];
				dump->num_outputs++;
			}
		}
	}
	return rc;
}

static int cam_ope_dump_frame_process(struct cam_packet *packet,
	struct cam_ope_hang_dump *dump)
{
	int rc = 0;
	int i;
	size_t len;
	struct cam_cmd_buf_desc *cmd_desc = NULL;
	uintptr_t cpu_addr = 0;

	cmd_desc = (struct cam_cmd_buf_desc *)
		((uint32_t *) &packet->payload + packet->cmd_buf_offset/4);
	for (i = 0; i < packet->num_cmd_buf; i++) {
		if (cmd_desc[i].type != CAM_CMD_BUF_GENERIC ||
			cmd_desc[i].meta_data == OPE_CMD_META_GENERIC_BLOB)
			continue;
		rc = cam_mem_get_cpu_buf(cmd_desc[i].mem_handle,
			&cpu_addr, &len);
		if (rc || !cpu_addr) {
			CAM_ERR(CAM_OPE, "get cmd buf failed %x",
				cmd_desc[i].mem_handle);
			return rc;
		}
		cpu_addr = cpu_addr + cmd_desc[i].offset;
		break;
	}

	if (!cpu_addr) {
		CAM_ERR(CAM_OPE, "invalid number of cmd buf");
		return -EINVAL;
	}

	cam_ope_mgr_dump_cmd_buf(cpu_addr, dump);
	cam_ope_mgr_dump_frame_set(cpu_addr, dump);
	return rc;
}

static int cam_ope_dump_bls(struct cam_ope_request *ope_req,
	struct cam_ope_hang_dump *dump)
{
	struct cam_cdm_bl_request *cdm_cmd;
	size_t size;
	int i, rc;
	dma_addr_t iova_addr;

	cdm_cmd = ope_req->cdm_cmd;
	for (i = 0; i < cdm_cmd->cmd_arrary_count; i++) {
		rc = cam_mem_get_io_buf(cdm_cmd->cmd[i].bl_addr.mem_handle,
				ope_hw_mgr->iommu_hdl, &iova_addr, &size);
		if (rc) {
			CAM_ERR(CAM_OPE, "get io buf fail 0x%x",
				cdm_cmd->cmd[i].bl_addr.mem_handle);
			return rc;
		}
		dump->bl_entries[dump->num_bls].base =
			(uint32_t)iova_addr + cdm_cmd->cmd[i].offset;
		dump->bl_entries[dump->num_bls].len = cdm_cmd->cmd[i].len;
		dump->bl_entries[dump->num_bls].arbitration =
			cdm_cmd->cmd[i].arbitrate;
		dump->num_bls++;
	}
	return 0;
}

static void cam_ope_dump_req_data(struct cam_ope_request *ope_req)
{
	struct cam_ope_hang_dump *dump;
	struct cam_packet *packet =
		(struct cam_packet *)ope_req->hang_data.packet;

	if (!ope_req->ope_debug_buf.cpu_addr ||
		ope_req->ope_debug_buf.len < sizeof(struct cam_ope_hang_dump) ||
		(ope_req->ope_debug_buf.offset + ope_req->ope_debug_buf.len)
			> ope_req->ope_debug_buf.size) {
		CAM_ERR(CAM_OPE, "Invalid debug buf, size %d %d len %d off %d",
				sizeof(struct cam_ope_hang_dump),
				ope_req->ope_debug_buf.size,
				ope_req->ope_debug_buf.len,
				ope_req->ope_debug_buf.offset);
		return;
	}

	dump = (struct cam_ope_hang_dump *)ope_req->ope_debug_buf.cpu_addr;
	memset(dump, 0, sizeof(struct cam_ope_hang_dump));
	dump->num_bufs = 0;
	cam_ope_dump_hang_patches(packet, dump);
	cam_ope_dump_frame_process(packet, dump);
	cam_ope_dump_bls(ope_req, dump);
}

static bool cam_ope_check_req_delay(struct cam_ope_ctx *ctx_data,
	uint64_t req_time)
{
	struct timespec64 ts;
	uint64_t ts_ns;

	ktime_get_boottime_ts64(&ts);
	ts_ns = (uint64_t)((ts.tv_sec * 1000000000) +
		ts.tv_nsec);

	if (ts_ns - req_time <
		((OPE_REQUEST_TIMEOUT -
			OPE_REQUEST_TIMEOUT / 10) * 1000000)) {
		CAM_INFO(CAM_OPE, "ctx: %d, ts_ns : %llu",
		ctx_data->ctx_id, ts_ns);
		cam_ope_req_timer_reset(ctx_data);
		return true;
	}

	return false;
}

static int32_t cam_ope_process_request_timer(void *priv, void *data)
{
	struct ope_clk_work_data *clk_data = (struct ope_clk_work_data *)data;
	struct cam_ope_ctx *ctx_data = (struct cam_ope_ctx *)clk_data->data;
	struct cam_ope_hw_mgr *hw_mgr = ope_hw_mgr;
	uint32_t id;
	struct cam_hw_intf *dev_intf = NULL;
	struct cam_ope_clk_info *clk_info;
	struct cam_ope_dev_bw_update clk_update;
	int i = 0;
	int device_share_ratio = 1;
	int path_index;
	struct crm_workq_task *task;
	struct ope_msg_work_data *task_data;

	if (!ctx_data) {
		CAM_ERR(CAM_OPE, "ctx_data is NULL, failed to update clk");
		return -EINVAL;
	}

	mutex_lock(&ctx_data->ctx_mutex);
	if ((ctx_data->ctx_state != OPE_CTX_STATE_ACQUIRED) ||
		(ctx_data->req_watch_dog_reset_counter == 0)) {
		CAM_DBG(CAM_OPE, "state %d counter = %d", ctx_data->ctx_state,
			ctx_data->req_watch_dog_reset_counter);
		mutex_unlock(&ctx_data->ctx_mutex);
		return 0;
	}

	if (cam_ope_is_pending_request(ctx_data)) {

		if (cam_ope_check_req_delay(ctx_data,
			ctx_data->last_req_time)) {
			mutex_unlock(&ctx_data->ctx_mutex);
			return 0;
		}

		if (cam_ope_check_req_delay(ctx_data,
			ope_hw_mgr->last_callback_time)) {
			CAM_WARN(CAM_OPE,
				"ope ctx: %d stuck due to other contexts",
				ctx_data->ctx_id);
			mutex_unlock(&ctx_data->ctx_mutex);
			return 0;
		}

		if (!cam_cdm_detect_hang_error(ctx_data->ope_cdm.cdm_handle)) {
			cam_ope_req_timer_reset(ctx_data);
			mutex_unlock(&ctx_data->ctx_mutex);
			return 0;
		}

		/* Try checking ctx struck again */
		if (cam_ope_check_req_delay(ctx_data,
			ope_hw_mgr->last_callback_time)) {
			CAM_WARN(CAM_OPE,
				"ope ctx: %d stuck due to other contexts",
				ctx_data->ctx_id);
			mutex_unlock(&ctx_data->ctx_mutex);
			return 0;
		}

		CAM_ERR(CAM_OPE,
			"pending requests means, issue is with HW for ctx %d",
			ctx_data->ctx_id);
		CAM_ERR(CAM_OPE, "ctx: %d, lrt: %llu, lct: %llu",
			ctx_data->ctx_id, ctx_data->last_req_time,
			ope_hw_mgr->last_callback_time);
		hw_mgr->ope_dev_intf[i]->hw_ops.process_cmd(
				hw_mgr->ope_dev_intf[i]->hw_priv,
				OPE_HW_DUMP_DEBUG,
				NULL, 0);
		task = cam_req_mgr_workq_get_task(ope_hw_mgr->msg_work);
		if (!task) {
			CAM_ERR(CAM_OPE, "no empty task");
			mutex_unlock(&ctx_data->ctx_mutex);
			return 0;
		}
		task_data = (struct ope_msg_work_data *)task->payload;
		task_data->data = hw_mgr;
		task_data->irq_status = 1;
		task_data->type = OPE_WORKQ_TASK_MSG_TYPE;
		task->process_cb = cam_ope_mgr_process_msg;
		cam_req_mgr_workq_enqueue_task(task, ope_hw_mgr,
			CRM_TASK_PRIORITY_0);
		cam_ope_req_timer_reset(ctx_data);
		mutex_unlock(&ctx_data->ctx_mutex);
		return 0;
	}

	cam_ope_req_timer_modify(ctx_data, ~0);
	/* Remove context BW */
	dev_intf = hw_mgr->ope_dev_intf[0];
	if (!dev_intf) {
		CAM_ERR(CAM_OPE, "OPE dev intf is NULL");
		mutex_unlock(&ctx_data->ctx_mutex);
		return -EINVAL;
	}

	clk_info = &hw_mgr->clk_info;
	id = OPE_HW_BW_UPDATE;
	device_share_ratio = hw_mgr->num_ope;

	clk_update.ahb_vote.type = CAM_VOTE_DYNAMIC;
	clk_update.ahb_vote.vote.freq = 0;
	clk_update.ahb_vote_valid = false;

	/*
	 * Remove previous vote of this context from hw mgr first.
	 * hw_mgr_clk_info has all valid paths, with each path in its
	 * own index. BW that we wanted to vote now is after removing
	 * current context's vote from hw mgr consolidated vote
	 */
	for (i = 0; i < ctx_data->clk_info.num_paths; i++) {
		path_index = ctx_data->clk_info.axi_path[i]
			.path_data_type -
			CAM_AXI_PATH_DATA_OPE_START_OFFSET;

		if (path_index >= CAM_OPE_MAX_PER_PATH_VOTES) {
			CAM_WARN(CAM_OPE,
				"Invalid path %d, start offset=%d, max=%d",
				ctx_data->clk_info.axi_path[i]
				.path_data_type,
				CAM_AXI_PATH_DATA_OPE_START_OFFSET,
				CAM_OPE_MAX_PER_PATH_VOTES);
				continue;
		}

		clk_info->axi_path[path_index].camnoc_bw -=
			ctx_data->clk_info.axi_path[i].camnoc_bw;
		clk_info->axi_path[path_index].mnoc_ab_bw -=
			ctx_data->clk_info.axi_path[i].mnoc_ab_bw;
		clk_info->axi_path[path_index].mnoc_ib_bw -=
			ctx_data->clk_info.axi_path[i].mnoc_ib_bw;
		clk_info->axi_path[path_index].ddr_ab_bw -=
			ctx_data->clk_info.axi_path[i].ddr_ab_bw;
		clk_info->axi_path[path_index].ddr_ib_bw -=
			ctx_data->clk_info.axi_path[i].ddr_ib_bw;
	}

	memset(&ctx_data->clk_info.axi_path[0], 0,
		CAM_OPE_MAX_PER_PATH_VOTES *
		sizeof(struct cam_axi_per_path_bw_vote));
	ctx_data->clk_info.curr_fc = 0;
	ctx_data->clk_info.base_clk = 0;

	clk_update.axi_vote.num_paths = clk_info->num_paths;
	memcpy(&clk_update.axi_vote.axi_path[0],
		&clk_info->axi_path[0],
		clk_update.axi_vote.num_paths *
		sizeof(struct cam_axi_per_path_bw_vote));

	if (device_share_ratio > 1) {
		for (i = 0; i < clk_update.axi_vote.num_paths; i++) {
			do_div(
			clk_update.axi_vote.axi_path[i].camnoc_bw,
				device_share_ratio);
			do_div(
			clk_update.axi_vote.axi_path[i].mnoc_ab_bw,
				device_share_ratio);
			do_div(
			clk_update.axi_vote.axi_path[i].mnoc_ib_bw,
				device_share_ratio);
			do_div(
			clk_update.axi_vote.axi_path[i].ddr_ab_bw,
				device_share_ratio);
			do_div(
			clk_update.axi_vote.axi_path[i].ddr_ib_bw,
				device_share_ratio);
		}
	}

	clk_update.axi_vote_valid = true;
	dev_intf->hw_ops.process_cmd(dev_intf->hw_priv, id,
		&clk_update, sizeof(clk_update));

	CAM_DBG(CAM_OPE, "X :ctx_id = %d curr_fc = %u bc = %u",
		ctx_data->ctx_id, ctx_data->clk_info.curr_fc,
		ctx_data->clk_info.base_clk);
	mutex_unlock(&ctx_data->ctx_mutex);

	return 0;
}

static void cam_ope_req_timer_cb(struct timer_list *timer_data)
{
	unsigned long flags;
	struct crm_workq_task *task;
	struct ope_clk_work_data *task_data;
	struct cam_req_mgr_timer *timer =
	container_of(timer_data, struct cam_req_mgr_timer, sys_timer);

	spin_lock_irqsave(&ope_hw_mgr->hw_mgr_lock, flags);
	task = cam_req_mgr_workq_get_task(ope_hw_mgr->timer_work);
	if (!task) {
		CAM_ERR(CAM_OPE, "no empty task");
		spin_unlock_irqrestore(&ope_hw_mgr->hw_mgr_lock, flags);
		return;
	}

	task_data = (struct ope_clk_work_data *)task->payload;
	task_data->data = timer->parent;
	task_data->type = OPE_WORKQ_TASK_MSG_TYPE;
	task->process_cb = cam_ope_process_request_timer;
	cam_req_mgr_workq_enqueue_task(task, ope_hw_mgr,
		CRM_TASK_PRIORITY_0);
	spin_unlock_irqrestore(&ope_hw_mgr->hw_mgr_lock, flags);
}

static int cam_ope_start_req_timer(struct cam_ope_ctx *ctx_data)
{
	int rc = 0;

	rc = crm_timer_init(&ctx_data->req_watch_dog,
		OPE_REQUEST_TIMEOUT, ctx_data, &cam_ope_req_timer_cb);
	if (rc)
		CAM_ERR(CAM_OPE, "Failed to start timer");

	ctx_data->req_watch_dog_reset_counter = 0;

	return rc;
}

static int cam_ope_supported_clk_rates(struct cam_ope_hw_mgr *hw_mgr,
	struct cam_ope_ctx *ctx_data)
{
	int i;
	struct cam_hw_soc_info *soc_info;
	struct cam_hw_intf *dev_intf = NULL;
	struct cam_hw_info *dev = NULL;

	dev_intf = hw_mgr->ope_dev_intf[0];
	if (!dev_intf) {
		CAM_ERR(CAM_OPE, "dev_intf is invalid");
		return -EINVAL;
	}

	dev = (struct cam_hw_info *)dev_intf->hw_priv;
	soc_info = &dev->soc_info;

	for (i = 0; i < CAM_MAX_VOTE; i++) {
		ctx_data->clk_info.clk_rate[i] =
			soc_info->clk_rate[i][soc_info->src_clk_idx];
		CAM_DBG(CAM_OPE, "clk_info[%d] = %d",
			i, ctx_data->clk_info.clk_rate[i]);
	}

	return 0;
}

static int cam_ope_ctx_clk_info_init(struct cam_ope_ctx *ctx_data)
{
	int i;

	ctx_data->clk_info.curr_fc = 0;
	ctx_data->clk_info.base_clk = 0;
	ctx_data->clk_info.uncompressed_bw = 0;
	ctx_data->clk_info.compressed_bw = 0;

	for (i = 0; i < CAM_OPE_MAX_PER_PATH_VOTES; i++) {
		ctx_data->clk_info.axi_path[i].camnoc_bw = 0;
		ctx_data->clk_info.axi_path[i].mnoc_ab_bw = 0;
		ctx_data->clk_info.axi_path[i].mnoc_ib_bw = 0;
	}

	cam_ope_supported_clk_rates(ope_hw_mgr, ctx_data);

	return 0;
}

static int32_t cam_ope_deinit_idle_clk(void *priv, void *data)
{
	struct cam_ope_hw_mgr *hw_mgr = (struct cam_ope_hw_mgr *)priv;
	struct ope_clk_work_data *task_data = (struct ope_clk_work_data *)data;
	struct cam_ope_clk_info *clk_info =
		(struct cam_ope_clk_info *)task_data->data;
	uint32_t id;
	uint32_t i;
	struct cam_ope_ctx *ctx_data;
	struct cam_hw_intf *dev_intf = NULL;
	int rc = 0;
	bool busy = false;

	clk_info->base_clk = 0;
	clk_info->curr_clk = 0;
	clk_info->over_clked = 0;

	mutex_lock(&hw_mgr->hw_mgr_mutex);

	for (i = 0; i < OPE_CTX_MAX; i++) {
		ctx_data = &hw_mgr->ctx[i];
		mutex_lock(&ctx_data->ctx_mutex);
		if (ctx_data->ctx_state == OPE_CTX_STATE_ACQUIRED) {
			busy = cam_ope_is_pending_request(ctx_data);
			if (busy) {
				mutex_unlock(&ctx_data->ctx_mutex);
				break;
			}
			cam_ope_ctx_clk_info_init(ctx_data);
		}
		mutex_unlock(&ctx_data->ctx_mutex);
	}

	if (busy) {
		cam_ope_device_timer_reset(hw_mgr);
		rc = -EBUSY;
		goto done;
	}

	dev_intf = hw_mgr->ope_dev_intf[0];
	id = OPE_HW_CLK_DISABLE;

	CAM_DBG(CAM_OPE, "Disable %d", clk_info->hw_type);

	dev_intf->hw_ops.process_cmd(dev_intf->hw_priv, id,	NULL, 0);

done:
	mutex_unlock(&hw_mgr->hw_mgr_mutex);
	return rc;
}

static void cam_ope_device_timer_cb(struct timer_list *timer_data)
{
	unsigned long flags;
	struct crm_workq_task *task;
	struct ope_clk_work_data *task_data;
	struct cam_req_mgr_timer *timer =
		container_of(timer_data, struct cam_req_mgr_timer, sys_timer);

	spin_lock_irqsave(&ope_hw_mgr->hw_mgr_lock, flags);
	task = cam_req_mgr_workq_get_task(ope_hw_mgr->timer_work);
	if (!task) {
		CAM_ERR(CAM_OPE, "no empty task");
		spin_unlock_irqrestore(&ope_hw_mgr->hw_mgr_lock, flags);
		return;
	}

	task_data = (struct ope_clk_work_data *)task->payload;
	task_data->data = timer->parent;
	task_data->type = OPE_WORKQ_TASK_MSG_TYPE;
	task->process_cb = cam_ope_deinit_idle_clk;
	cam_req_mgr_workq_enqueue_task(task, ope_hw_mgr,
		CRM_TASK_PRIORITY_0);
	spin_unlock_irqrestore(&ope_hw_mgr->hw_mgr_lock, flags);
}

static int cam_ope_device_timer_start(struct cam_ope_hw_mgr *hw_mgr)
{
	int rc = 0;
	int i;

	for (i = 0; i < CLK_HW_MAX; i++)  {
		if (!hw_mgr->clk_info.watch_dog) {
			rc = crm_timer_init(&hw_mgr->clk_info.watch_dog,
				OPE_DEVICE_IDLE_TIMEOUT, &hw_mgr->clk_info,
				&cam_ope_device_timer_cb);

			if (rc)
				CAM_ERR(CAM_OPE, "Failed to start timer %d", i);

			hw_mgr->clk_info.watch_dog_reset_counter = 0;
		}
	}

	return rc;
}

static int cam_ope_get_actual_clk_rate_idx(
	struct cam_ope_ctx *ctx_data, uint32_t base_clk)
{
	int i;

	for (i = 0; i < CAM_MAX_VOTE; i++)
		if (ctx_data->clk_info.clk_rate[i] >= base_clk)
			return i;

	/*
	 * Caller has to ensure returned index is within array
	 * size bounds while accessing that index.
	 */

	return i;
}

static bool cam_ope_is_over_clk(struct cam_ope_hw_mgr *hw_mgr,
	struct cam_ope_ctx *ctx_data,
	struct cam_ope_clk_info *hw_mgr_clk_info)
{
	int base_clk_idx;
	int curr_clk_idx;

	base_clk_idx = cam_ope_get_actual_clk_rate_idx(ctx_data,
		hw_mgr_clk_info->base_clk);

	curr_clk_idx = cam_ope_get_actual_clk_rate_idx(ctx_data,
		hw_mgr_clk_info->curr_clk);

	CAM_DBG(CAM_OPE, "bc_idx = %d cc_idx = %d %d %d",
		base_clk_idx, curr_clk_idx, hw_mgr_clk_info->base_clk,
		hw_mgr_clk_info->curr_clk);

	if (curr_clk_idx > base_clk_idx)
		return true;

	return false;
}


static int cam_ope_get_lower_clk_rate(struct cam_ope_hw_mgr *hw_mgr,
	struct cam_ope_ctx *ctx_data, uint32_t base_clk)
{
	int i;

	i = cam_ope_get_actual_clk_rate_idx(ctx_data, base_clk);

	while (i > 0) {
		if (ctx_data->clk_info.clk_rate[i - 1])
			return ctx_data->clk_info.clk_rate[i - 1];
		i--;
	}

	CAM_DBG(CAM_OPE, "Already clk at lower level");

	return base_clk;
}

static int cam_ope_get_next_clk_rate(struct cam_ope_hw_mgr *hw_mgr,
	struct cam_ope_ctx *ctx_data, uint32_t base_clk)
{
	int i;

	i = cam_ope_get_actual_clk_rate_idx(ctx_data, base_clk);

	while (i < CAM_MAX_VOTE - 1) {
		if (ctx_data->clk_info.clk_rate[i + 1])
			return ctx_data->clk_info.clk_rate[i + 1];
		i++;
	}

	CAM_DBG(CAM_OPE, "Already clk at higher level");

	return base_clk;
}

static int cam_ope_get_actual_clk_rate(struct cam_ope_hw_mgr *hw_mgr,
	struct cam_ope_ctx *ctx_data, uint32_t base_clk)
{
	int i;

	for (i = 0; i < CAM_MAX_VOTE; i++)
		if (ctx_data->clk_info.clk_rate[i] >= base_clk)
			return ctx_data->clk_info.clk_rate[i];

	return base_clk;
}

static int cam_ope_calc_total_clk(struct cam_ope_hw_mgr *hw_mgr,
	struct cam_ope_clk_info *hw_mgr_clk_info, uint32_t dev_type)
{
	int i;
	struct cam_ope_ctx *ctx_data;

	hw_mgr_clk_info->base_clk = 0;
	for (i = 0; i < OPE_CTX_MAX; i++) {
		ctx_data = &hw_mgr->ctx[i];
		if (ctx_data->ctx_state == OPE_CTX_STATE_ACQUIRED)
			hw_mgr_clk_info->base_clk +=
				ctx_data->clk_info.base_clk;
	}

	return 0;
}

static uint32_t cam_ope_mgr_calc_base_clk(uint32_t frame_cycles,
	uint64_t budget)
{
	uint64_t mul = 1000000000;
	uint64_t base_clk = frame_cycles * mul;

	do_div(base_clk, budget);

	CAM_DBG(CAM_OPE, "budget = %lld fc = %d ib = %lld base_clk = %lld",
		budget, frame_cycles,
		(long long)(frame_cycles * mul), base_clk);

	return base_clk;
}

static bool cam_ope_update_clk_overclk_free(struct cam_ope_hw_mgr *hw_mgr,
	struct cam_ope_ctx *ctx_data,
	struct cam_ope_clk_info *hw_mgr_clk_info,
	struct cam_ope_clk_bw_request *clk_info,
	uint32_t base_clk)
{
	int rc = false;

	/*
	 * In caseof no pending packets case
	 *    1. In caseof overclk cnt is less than threshold, increase
	 *       overclk count and no update in the clock rate
	 *    2. In caseof overclk cnt is greater than or equal to threshold
	 *       then lower clock rate by one level and update hw_mgr current
	 *       clock value.
	 *        a. In case of new clock rate greater than sum of clock
	 *           rates, reset overclk count value to zero if it is
	 *           overclock
	 *        b. if it is less than sum of base clocks then go to next
	 *           level of clock and make overclk count to zero
	 *        c. if it is same as sum of base clock rates update overclock
	 *           cnt to 0
	 */
	if (hw_mgr_clk_info->over_clked < hw_mgr_clk_info->threshold) {
		hw_mgr_clk_info->over_clked++;
		rc = false;
	} else {
		hw_mgr_clk_info->curr_clk =
			cam_ope_get_lower_clk_rate(hw_mgr, ctx_data,
			hw_mgr_clk_info->curr_clk);
		if (hw_mgr_clk_info->curr_clk > hw_mgr_clk_info->base_clk) {
			if (cam_ope_is_over_clk(hw_mgr, ctx_data,
				hw_mgr_clk_info))
				hw_mgr_clk_info->over_clked = 0;
		} else if (hw_mgr_clk_info->curr_clk <
			hw_mgr_clk_info->base_clk) {
			hw_mgr_clk_info->curr_clk =
			cam_ope_get_next_clk_rate(hw_mgr, ctx_data,
				hw_mgr_clk_info->curr_clk);
				hw_mgr_clk_info->over_clked = 0;
		} else if (hw_mgr_clk_info->curr_clk ==
			hw_mgr_clk_info->base_clk) {
			hw_mgr_clk_info->over_clked = 0;
		}
		rc = true;
	}

	return rc;
}

static bool cam_ope_update_clk_free(struct cam_ope_hw_mgr *hw_mgr,
	struct cam_ope_ctx *ctx_data,
	struct cam_ope_clk_info *hw_mgr_clk_info,
	struct cam_ope_clk_bw_request *clk_info,
	uint32_t base_clk)
{
	int rc = false;
	bool over_clocked = false;

	ctx_data->clk_info.curr_fc = clk_info->frame_cycles;
	ctx_data->clk_info.base_clk = base_clk;
	cam_ope_calc_total_clk(hw_mgr, hw_mgr_clk_info,
		ctx_data->ope_acquire.dev_type);

	/*
	 * Current clock is not always sum of base clocks, due to
	 * clock scales update to next higher or lower levels, it
	 * equals to one of discrete clock values supported by hardware.
	 * So even current clock is higher than sum of base clocks, we
	 * can not consider it is over clocked. if it is greater than
	 * discrete clock level then only it is considered as over clock.
	 * 1. Handle over clock case
	 * 2. If current clock is less than sum of base clocks
	 *    update current clock
	 * 3. If current clock is same as sum of base clocks no action
	 */

	over_clocked = cam_ope_is_over_clk(hw_mgr, ctx_data,
		hw_mgr_clk_info);

	if (hw_mgr_clk_info->curr_clk > hw_mgr_clk_info->base_clk &&
		over_clocked) {
		rc = cam_ope_update_clk_overclk_free(hw_mgr, ctx_data,
			hw_mgr_clk_info, clk_info, base_clk);
	} else if (hw_mgr_clk_info->curr_clk > hw_mgr_clk_info->base_clk) {
		hw_mgr_clk_info->over_clked = 0;
		rc = false;
	}  else if (hw_mgr_clk_info->curr_clk < hw_mgr_clk_info->base_clk) {
		hw_mgr_clk_info->curr_clk = cam_ope_get_actual_clk_rate(hw_mgr,
			ctx_data, hw_mgr_clk_info->base_clk);
		rc = true;
	}

	return rc;
}

static bool cam_ope_update_clk_busy(struct cam_ope_hw_mgr *hw_mgr,
	struct cam_ope_ctx *ctx_data,
	struct cam_ope_clk_info *hw_mgr_clk_info,
	struct cam_ope_clk_bw_request *clk_info,
	uint32_t base_clk)
{
	uint32_t next_clk_level;
	uint32_t actual_clk;
	bool rc = false;

	/* 1. if current request frame cycles(fc) are more than previous
	 *      frame fc
	 *      Calculate the new base clock.
	 *      if sum of base clocks are more than next available clk level
	 *       Update clock rate, change curr_clk_rate to sum of base clock
	 *       rates and make over_clked to zero
	 *      else
	 *       Update clock rate to next level, update curr_clk_rate and make
	 *       overclked cnt to zero
	 * 2. if current fc is less than or equal to previous  frame fc
	 *      Still Bump up the clock to next available level
	 *      if it is available, then update clock, make overclk cnt to
	 *      zero. If the clock is already at highest clock rate then
	 *      no need to update the clock
	 */
	ctx_data->clk_info.base_clk = base_clk;
	hw_mgr_clk_info->over_clked = 0;
	if (clk_info->frame_cycles > ctx_data->clk_info.curr_fc) {
		cam_ope_calc_total_clk(hw_mgr, hw_mgr_clk_info,
			ctx_data->ope_acquire.dev_type);
		actual_clk = cam_ope_get_actual_clk_rate(hw_mgr,
			ctx_data, base_clk);
		if (hw_mgr_clk_info->base_clk > actual_clk) {
			hw_mgr_clk_info->curr_clk = hw_mgr_clk_info->base_clk;
		} else {
			next_clk_level = cam_ope_get_next_clk_rate(hw_mgr,
				ctx_data, hw_mgr_clk_info->curr_clk);
			hw_mgr_clk_info->curr_clk = next_clk_level;
		}
		rc = true;
	} else {
		next_clk_level =
			cam_ope_get_next_clk_rate(hw_mgr, ctx_data,
				hw_mgr_clk_info->curr_clk);
		if (hw_mgr_clk_info->curr_clk < next_clk_level) {
			hw_mgr_clk_info->curr_clk = next_clk_level;
			rc = true;
		}
	}
	ctx_data->clk_info.curr_fc = clk_info->frame_cycles;

	return rc;
}

static bool cam_ope_check_clk_update(struct cam_ope_hw_mgr *hw_mgr,
	struct cam_ope_ctx *ctx_data, int idx)
{
	bool busy = false, rc = false;
	uint32_t base_clk;
	struct cam_ope_clk_bw_request *clk_info;
	uint64_t req_id;
	struct cam_ope_clk_info *hw_mgr_clk_info;

	/* TODO: Have default clock rates update */
	/* TODO: Add support for debug clock updates */
	cam_ope_req_timer_reset(ctx_data);
	cam_ope_device_timer_reset(hw_mgr);
	hw_mgr_clk_info = &hw_mgr->clk_info;
	req_id = ctx_data->req_list[idx]->request_id;
	if (ctx_data->req_cnt > 1)
		busy = true;

	CAM_DBG(CAM_OPE, "busy = %d req_id = %lld", busy, req_id);

	clk_info = &ctx_data->req_list[idx]->clk_info;

	/* Calculate base clk rate */
	base_clk = cam_ope_mgr_calc_base_clk(
		clk_info->frame_cycles, clk_info->budget_ns);
	ctx_data->clk_info.rt_flag = clk_info->rt_flag;

	if (busy)
		rc = cam_ope_update_clk_busy(hw_mgr, ctx_data,
			hw_mgr_clk_info, clk_info, base_clk);
	else
		rc = cam_ope_update_clk_free(hw_mgr, ctx_data,
			hw_mgr_clk_info, clk_info, base_clk);

	CAM_DBG(CAM_OPE, "bc = %d cc = %d busy = %d overclk = %d uc = %d",
		hw_mgr_clk_info->base_clk, hw_mgr_clk_info->curr_clk,
		busy, hw_mgr_clk_info->over_clked, rc);

	return rc;
}

static int cam_ope_mgr_update_clk_rate(struct cam_ope_hw_mgr *hw_mgr,
	struct cam_ope_ctx *ctx_data)
{
	struct cam_ope_dev_clk_update clk_upd_cmd;
	int i;

	clk_upd_cmd.clk_rate = hw_mgr->clk_info.curr_clk;

	CAM_DBG(CAM_PERF, "clk_rate %u for dev_type %d", clk_upd_cmd.clk_rate,
		ctx_data->ope_acquire.dev_type);

	for (i = 0; i < ope_hw_mgr->num_ope; i++) {
		hw_mgr->ope_dev_intf[i]->hw_ops.process_cmd(
			hw_mgr->ope_dev_intf[i]->hw_priv,
			OPE_HW_CLK_UPDATE,
			&clk_upd_cmd, sizeof(clk_upd_cmd));
	}

	return 0;
}

static int cam_ope_mgr_calculate_num_path(
	struct cam_ope_clk_bw_req_internal_v2 *clk_info,
	struct cam_ope_ctx *ctx_data)
{
	int i, path_index = 0;

	for (i = 0; i < CAM_OPE_MAX_PER_PATH_VOTES; i++) {
		if ((clk_info->axi_path[i].path_data_type <
			CAM_AXI_PATH_DATA_OPE_START_OFFSET) ||
			(clk_info->axi_path[i].path_data_type >
			CAM_AXI_PATH_DATA_OPE_MAX_OFFSET) ||
			((clk_info->axi_path[i].path_data_type -
			CAM_AXI_PATH_DATA_OPE_START_OFFSET) >=
			CAM_OPE_MAX_PER_PATH_VOTES)) {
			CAM_DBG(CAM_OPE,
				"Invalid path %d, start offset=%d, max=%d",
				ctx_data->clk_info.axi_path[i].path_data_type,
				CAM_AXI_PATH_DATA_OPE_START_OFFSET,
				CAM_OPE_MAX_PER_PATH_VOTES);
			continue;
		}

		path_index = clk_info->axi_path[i].path_data_type -
			CAM_AXI_PATH_DATA_OPE_START_OFFSET;

		CAM_DBG(CAM_OPE,
			"clk_info: i[%d]: [%s %s] bw [%lld %lld] num_path: %d",
			i,
			cam_cpas_axi_util_trans_type_to_string(
			clk_info->axi_path[i].transac_type),
			cam_cpas_axi_util_path_type_to_string(
			clk_info->axi_path[i].path_data_type),
			clk_info->axi_path[i].camnoc_bw,
			clk_info->axi_path[i].mnoc_ab_bw,
			clk_info->num_paths);
	}

	return (path_index+1);
}

static bool cam_ope_update_bw_v2(struct cam_ope_hw_mgr *hw_mgr,
	struct cam_ope_ctx *ctx_data,
	struct cam_ope_clk_info *hw_mgr_clk_info,
	struct cam_ope_clk_bw_req_internal_v2 *clk_info,
	bool busy)
{
	int i, path_index;
	bool update_required = true;

	/*
	 * If current request bandwidth is different from previous frames, then
	 * recalculate bandwidth of all contexts of same hardware and update
	 * voting of bandwidth
	 */

	for (i = 0; i < clk_info->num_paths; i++)
		CAM_DBG(CAM_OPE, "clk_info camnoc = %lld busy = %d",
			clk_info->axi_path[i].camnoc_bw, busy);

	if (clk_info->num_paths == ctx_data->clk_info.num_paths) {
		update_required = false;
		for (i = 0; i < clk_info->num_paths; i++) {
			if ((clk_info->axi_path[i].transac_type ==
			ctx_data->clk_info.axi_path[i].transac_type) &&
				(clk_info->axi_path[i].path_data_type ==
			ctx_data->clk_info.axi_path[i].path_data_type) &&
				(clk_info->axi_path[i].camnoc_bw ==
			ctx_data->clk_info.axi_path[i].camnoc_bw) &&
				(clk_info->axi_path[i].mnoc_ab_bw ==
			ctx_data->clk_info.axi_path[i].mnoc_ab_bw)) {
				continue;
			} else {
				update_required = true;
				break;
			}
		}
	}

	if (!update_required) {
		CAM_DBG(CAM_OPE,
		"Incoming BW hasn't changed, no update required");
		return false;
	}

	if (busy) {
		for (i = 0; i < clk_info->num_paths; i++) {
			if (ctx_data->clk_info.axi_path[i].camnoc_bw >
				clk_info->axi_path[i].camnoc_bw)
				return false;
		}
	}

	/*
	 * Remove previous vote of this context from hw mgr first.
	 * hw_mgr_clk_info has all valid paths, with each path in its own index
	 */
	for (i = 0; i < ctx_data->clk_info.num_paths; i++) {
		path_index =
		ctx_data->clk_info.axi_path[i].path_data_type -
		CAM_AXI_PATH_DATA_OPE_START_OFFSET;

		if (path_index >= CAM_OPE_MAX_PER_PATH_VOTES) {
			CAM_WARN(CAM_OPE,
				"Invalid path %d, start offset=%d, max=%d",
				ctx_data->clk_info.axi_path[i].path_data_type,
				CAM_AXI_PATH_DATA_OPE_START_OFFSET,
				CAM_OPE_MAX_PER_PATH_VOTES);
			continue;
		}

	hw_mgr_clk_info->axi_path[path_index].camnoc_bw -=
		ctx_data->clk_info.axi_path[i].camnoc_bw;
	hw_mgr_clk_info->axi_path[path_index].mnoc_ab_bw -=
		ctx_data->clk_info.axi_path[i].mnoc_ab_bw;
	hw_mgr_clk_info->axi_path[path_index].mnoc_ib_bw -=
		ctx_data->clk_info.axi_path[i].mnoc_ib_bw;
	hw_mgr_clk_info->axi_path[path_index].ddr_ab_bw -=
		ctx_data->clk_info.axi_path[i].ddr_ab_bw;
	hw_mgr_clk_info->axi_path[path_index].ddr_ib_bw -=
		ctx_data->clk_info.axi_path[i].ddr_ib_bw;
	}

	ctx_data->clk_info.num_paths =
		cam_ope_mgr_calculate_num_path(clk_info, ctx_data);

	memcpy(&ctx_data->clk_info.axi_path[0],
		&clk_info->axi_path[0],
		clk_info->num_paths * sizeof(struct cam_axi_per_path_bw_vote));

	/*
	 * Add new vote of this context in hw mgr.
	 * hw_mgr_clk_info has all paths, with each path in its own index
	 */
	for (i = 0; i < ctx_data->clk_info.num_paths; i++) {
		path_index =
		ctx_data->clk_info.axi_path[i].path_data_type -
			CAM_AXI_PATH_DATA_OPE_START_OFFSET;

		if (path_index >= CAM_OPE_MAX_PER_PATH_VOTES) {
			CAM_WARN(CAM_OPE,
				"Invalid path %d, start offset=%d, max=%d",
				ctx_data->clk_info.axi_path[i].path_data_type,
				CAM_AXI_PATH_DATA_OPE_START_OFFSET,
				CAM_OPE_MAX_PER_PATH_VOTES);
			continue;
		}

		hw_mgr_clk_info->axi_path[path_index].path_data_type =
			ctx_data->clk_info.axi_path[i].path_data_type;
		hw_mgr_clk_info->axi_path[path_index].transac_type =
			ctx_data->clk_info.axi_path[i].transac_type;
		hw_mgr_clk_info->axi_path[path_index].camnoc_bw +=
			ctx_data->clk_info.axi_path[i].camnoc_bw;
		hw_mgr_clk_info->axi_path[path_index].mnoc_ab_bw +=
			ctx_data->clk_info.axi_path[i].mnoc_ab_bw;
		hw_mgr_clk_info->axi_path[path_index].mnoc_ib_bw +=
			ctx_data->clk_info.axi_path[i].mnoc_ib_bw;
		hw_mgr_clk_info->axi_path[path_index].ddr_ab_bw +=
			ctx_data->clk_info.axi_path[i].ddr_ab_bw;
		hw_mgr_clk_info->axi_path[path_index].ddr_ib_bw +=
			ctx_data->clk_info.axi_path[i].ddr_ib_bw;
		CAM_DBG(CAM_OPE,
			"Consolidate Path Vote : Dev[%s] i[%d] path_idx[%d] : [%s %s] [%lld %lld]",
			ctx_data->ope_acquire.dev_name,
			i, path_index,
			cam_cpas_axi_util_trans_type_to_string(
			hw_mgr_clk_info->axi_path[path_index].transac_type),
			cam_cpas_axi_util_path_type_to_string(
			hw_mgr_clk_info->axi_path[path_index].path_data_type),
			hw_mgr_clk_info->axi_path[path_index].camnoc_bw,
			hw_mgr_clk_info->axi_path[path_index].mnoc_ab_bw);
	}

	if (hw_mgr_clk_info->num_paths < ctx_data->clk_info.num_paths)
		hw_mgr_clk_info->num_paths = ctx_data->clk_info.num_paths;

	return true;
}

static bool cam_ope_check_bw_update(struct cam_ope_hw_mgr *hw_mgr,
	struct cam_ope_ctx *ctx_data, int idx)
{
	bool busy = false, bw_updated = false;
	int i;
	struct cam_ope_clk_bw_req_internal_v2 *clk_info_v2;
	struct cam_ope_clk_info *hw_mgr_clk_info;
	uint64_t req_id;

	hw_mgr_clk_info = &hw_mgr->clk_info;
	req_id = ctx_data->req_list[idx]->request_id;
	if (ctx_data->req_cnt > 1)
		busy = true;

	clk_info_v2 = &ctx_data->req_list[idx]->clk_info_v2;

	bw_updated = cam_ope_update_bw_v2(hw_mgr, ctx_data,
		hw_mgr_clk_info, clk_info_v2, busy);

	for (i = 0; i < hw_mgr_clk_info->num_paths; i++) {
		CAM_DBG(CAM_OPE,
			"Final path_type: %s, transac_type: %s, camnoc_bw = %lld mnoc_ab_bw = %lld, mnoc_ib_bw = %lld, device: %s",
			cam_cpas_axi_util_path_type_to_string(
			hw_mgr_clk_info->axi_path[i].path_data_type),
			cam_cpas_axi_util_trans_type_to_string(
			hw_mgr_clk_info->axi_path[i].transac_type),
			hw_mgr_clk_info->axi_path[i].camnoc_bw,
			hw_mgr_clk_info->axi_path[i].mnoc_ab_bw,
			hw_mgr_clk_info->axi_path[i].mnoc_ib_bw,
			ctx_data->ope_acquire.dev_name);
	}

	return bw_updated;
}

static int cam_ope_update_cpas_vote(struct cam_ope_hw_mgr *hw_mgr,
	struct cam_ope_ctx *ctx_data)
{
	int i = 0;
	struct cam_ope_clk_info *clk_info;
	struct cam_ope_dev_bw_update bw_update = {{0}, {0}, 0, 0};

	clk_info = &hw_mgr->clk_info;

	bw_update.ahb_vote.type = CAM_VOTE_DYNAMIC;
	bw_update.ahb_vote.vote.freq = 0;
	bw_update.ahb_vote_valid = false;


	bw_update.axi_vote.num_paths = clk_info->num_paths;
	memcpy(&bw_update.axi_vote.axi_path[0],
		&clk_info->axi_path[0],
		bw_update.axi_vote.num_paths *
		sizeof(struct cam_axi_per_path_bw_vote));

	bw_update.axi_vote_valid = true;
	for (i = 0; i < ope_hw_mgr->num_ope; i++) {
		hw_mgr->ope_dev_intf[i]->hw_ops.process_cmd(
			hw_mgr->ope_dev_intf[i]->hw_priv,
			OPE_HW_BW_UPDATE,
			&bw_update, sizeof(bw_update));
	}

	return 0;
}

static int cam_ope_mgr_ope_clk_update(struct cam_ope_hw_mgr *hw_mgr,
	struct cam_ope_ctx *ctx_data, int idx)
{
	int rc = 0;

	if (cam_ope_check_clk_update(hw_mgr, ctx_data, idx))
		rc = cam_ope_mgr_update_clk_rate(hw_mgr, ctx_data);

	if (cam_ope_check_bw_update(hw_mgr, ctx_data, idx))
		rc |= cam_ope_update_cpas_vote(hw_mgr, ctx_data);

	return rc;
}

static void cam_ope_ctx_cdm_callback(uint32_t handle, void *userdata,
	enum cam_cdm_cb_status status, uint64_t cookie)
{
	int rc = 0;
	struct cam_ope_ctx *ctx;
	struct cam_ope_request *ope_req;
	struct cam_hw_done_event_data buf_data;
	struct timespec64 ts;
	uint32_t evt_id = CAM_CTX_EVT_ID_SUCCESS;
	bool dump_flag = true;

	if (!userdata) {
		CAM_ERR(CAM_OPE, "Invalid ctx from CDM callback");
		return;
	}

	if (cookie >= CAM_CTX_REQ_MAX) {
		CAM_ERR(CAM_OPE, "Invalid reqIdx = %llu", cookie);
		return;
	}

	ctx = userdata;
	mutex_lock(&ctx->ctx_mutex);

	if (!test_bit(cookie, ctx->bitmap)) {
		CAM_ERR(CAM_OPE, "Req not present reqIdx = %d for ctx_id = %d",
			cookie, ctx->ctx_id);
		goto end;
	}

	ope_req = ctx->req_list[cookie];

	ktime_get_boottime_ts64(&ts);
	ope_hw_mgr->last_callback_time = (uint64_t)((ts.tv_sec * 1000000000) +
		ts.tv_nsec);

	CAM_DBG(CAM_REQ,
		"hdl=%x, udata=%pK, status=%d, cookie=%d",
		handle, userdata, status, cookie);
	CAM_DBG(CAM_REQ, "req_id= %llu ctx_id= %d lcb=%llu",
		ope_req->request_id, ctx->ctx_id,
		ope_hw_mgr->last_callback_time);

	if (ctx->ctx_state != OPE_CTX_STATE_ACQUIRED) {
		CAM_ERR(CAM_OPE, "ctx %u is in %d state",
			ctx->ctx_id, ctx->ctx_state);
		mutex_unlock(&ctx->ctx_mutex);
		return;
	}

	if (status == CAM_CDM_CB_STATUS_BL_SUCCESS) {
		CAM_DBG(CAM_OPE,
			"hdl=%x, udata=%pK, status=%d, cookie=%d  req_id=%llu ctx_id=%d",
			handle, userdata, status, cookie,
			ope_req->request_id, ctx->ctx_id);
		cam_ope_req_timer_reset(ctx);
		cam_ope_device_timer_reset(ope_hw_mgr);
		buf_data.evt_param = CAM_SYNC_COMMON_EVENT_SUCCESS;
	} else if (status == CAM_CDM_CB_STATUS_HW_RESUBMIT) {
		CAM_INFO(CAM_OPE, "After reset of CDM and OPE, reapply req");
		buf_data.evt_param = CAM_SYNC_OPE_EVENT_HW_RESUBMIT;
		rc = cam_ope_mgr_reapply_config(ope_hw_mgr, ctx, ope_req);
		if (!rc)
			goto end;
	} else {
		CAM_INFO(CAM_OPE,
			"CDM hdl=%x, udata=%pK, status=%d, cookie=%d req_id = %llu ctx_id=%d",
			 handle, userdata, status, cookie,
			 ope_req->request_id, ctx->ctx_id);
		CAM_INFO(CAM_OPE, "Rst of CDM and OPE for error reqid = %lld",
			ope_req->request_id);
		if (status != CAM_CDM_CB_STATUS_HW_FLUSH) {
			cam_ope_dump_req_data(ope_req);
			dump_flag = false;
		}
		rc = cam_ope_mgr_reset_hw();
		evt_id = CAM_CTX_EVT_ID_ERROR;

		if (status == CAM_CDM_CB_STATUS_PAGEFAULT)
			buf_data.evt_param = CAM_SYNC_OPE_EVENT_PAGE_FAULT;
		else if (status == CAM_CDM_CB_STATUS_HW_FLUSH)
			buf_data.evt_param = CAM_SYNC_OPE_EVENT_HW_FLUSH;
		else if (status == CAM_CDM_CB_STATUS_HW_RESET_DONE)
			buf_data.evt_param = CAM_SYNC_OPE_EVENT_HW_RESET_DONE;
		else if (status == CAM_CDM_CB_STATUS_HW_ERROR)
			buf_data.evt_param = CAM_SYNC_OPE_EVENT_HW_ERROR;
		else
			buf_data.evt_param = CAM_SYNC_OPE_EVENT_UNKNOWN;
	}

	if (ope_hw_mgr->dump_req_data_enable && dump_flag)
		cam_ope_dump_req_data(ope_req);

	ctx->req_cnt--;

	buf_data.request_id = ope_req->request_id;
	ope_req->request_id = 0;
	kzfree(ctx->req_list[cookie]->cdm_cmd);
	ctx->req_list[cookie]->cdm_cmd = NULL;
	cam_ope_free_io_config(ctx->req_list[cookie]);
	kzfree(ctx->req_list[cookie]);
	ctx->req_list[cookie] = NULL;
	clear_bit(cookie, ctx->bitmap);
	ctx->ctxt_event_cb(ctx->context_priv, evt_id, &buf_data);

end:
	mutex_unlock(&ctx->ctx_mutex);
}

int32_t cam_ope_hw_mgr_cb(uint32_t irq_status, void *data)
{
	int32_t rc = 0;
	unsigned long flags;
	struct cam_ope_hw_mgr *hw_mgr = data;
	struct crm_workq_task *task;
	struct ope_msg_work_data *task_data;

	if (!data) {
		CAM_ERR(CAM_OPE, "irq cb data is NULL");
		return rc;
	}

	spin_lock_irqsave(&hw_mgr->hw_mgr_lock, flags);
	task = cam_req_mgr_workq_get_task(ope_hw_mgr->msg_work);
	if (!task) {
		CAM_ERR(CAM_OPE, "no empty task");
		spin_unlock_irqrestore(&hw_mgr->hw_mgr_lock, flags);
		return -ENOMEM;
	}

	task_data = (struct ope_msg_work_data *)task->payload;
	task_data->data = hw_mgr;
	task_data->irq_status = irq_status;
	task_data->type = OPE_WORKQ_TASK_MSG_TYPE;
	task->process_cb = cam_ope_mgr_process_msg;
	rc = cam_req_mgr_workq_enqueue_task(task, ope_hw_mgr,
		CRM_TASK_PRIORITY_0);
	spin_unlock_irqrestore(&hw_mgr->hw_mgr_lock, flags);

	return rc;
}

static int cam_ope_mgr_create_kmd_buf(struct cam_ope_hw_mgr *hw_mgr,
	struct cam_packet *packet,
	struct cam_hw_prepare_update_args *prepare_args,
	struct cam_ope_ctx *ctx_data, uint32_t req_idx,
	uintptr_t   ope_cmd_buf_addr)
{
	int i, rc = 0;
	struct cam_ope_dev_prepare_req prepare_req;

	prepare_req.ctx_data = ctx_data;
	prepare_req.hw_mgr = hw_mgr;
	prepare_req.packet = packet;
	prepare_req.prepare_args = prepare_args;
	prepare_req.req_idx = req_idx;
	prepare_req.kmd_buf_offset = 0;
	prepare_req.frame_process =
		(struct ope_frame_process *)ope_cmd_buf_addr;

	for (i = 0; i < ope_hw_mgr->num_ope; i++)
		rc = hw_mgr->ope_dev_intf[i]->hw_ops.process_cmd(
			hw_mgr->ope_dev_intf[i]->hw_priv,
			OPE_HW_PREPARE, &prepare_req, sizeof(prepare_req));
		if (rc) {
			CAM_ERR(CAM_OPE, "OPE Dev prepare failed: %d", rc);
			goto end;
		}

end:
	return rc;
}

static int cam_ope_mgr_process_io_cfg(struct cam_ope_hw_mgr *hw_mgr,
	struct cam_packet *packet,
	struct cam_hw_prepare_update_args *prep_args,
	struct cam_ope_ctx *ctx_data, uint32_t req_idx)
{

	int i, j = 0, k = 0, l, rc = 0;
	struct ope_io_buf *io_buf;
	int32_t sync_in_obj[CAM_MAX_IN_RES];
	int32_t merged_sync_in_obj;
	struct cam_ope_request *ope_request;

	ope_request = ctx_data->req_list[req_idx];
	prep_args->num_out_map_entries = 0;
	prep_args->num_in_map_entries = 0;

	ope_request = ctx_data->req_list[req_idx];
	CAM_DBG(CAM_OPE, "E: req_idx = %u %x", req_idx, packet);

	for (i = 0; i < ope_request->num_batch; i++) {
		for (l = 0; l < ope_request->num_io_bufs[i]; l++) {
			io_buf = ope_request->io_buf[i][l];
			if (io_buf->direction == CAM_BUF_INPUT) {
				if (io_buf->fence != -1) {
					sync_in_obj[j++] = io_buf->fence;
					prep_args->num_in_map_entries++;
				} else {
					CAM_ERR(CAM_OPE, "Invalid fence %d %d",
						io_buf->resource_type,
						ope_request->request_id);
				}
			} else {
				if (io_buf->fence != -1) {
					prep_args->out_map_entries[k].sync_id =
						io_buf->fence;
					k++;
					prep_args->num_out_map_entries++;
				} else {
					if (io_buf->resource_type
						!= OPE_OUT_RES_STATS_LTM) {
						CAM_ERR(CAM_OPE,
						"Invalid fence %d %d",
						io_buf->resource_type,
						ope_request->request_id);
					}
				}
			}
			CAM_DBG(CAM_REQ,
				"ctx_id: %u req_id: %llu dir[%d] %u, fence: %d",
				ctx_data->ctx_id, packet->header.request_id, i,
				io_buf->direction, io_buf->fence);
			CAM_DBG(CAM_REQ, "rsc_type = %u fmt = %d",
				io_buf->resource_type,
				io_buf->format);
		}
	}

	if (prep_args->num_in_map_entries > 1 &&
		prep_args->num_in_map_entries <= CAM_MAX_IN_RES)
		prep_args->num_in_map_entries =
			cam_common_util_remove_duplicate_arr(
			sync_in_obj, prep_args->num_in_map_entries);

	if (prep_args->num_in_map_entries > 1 &&
		prep_args->num_in_map_entries <= CAM_MAX_IN_RES) {
		rc = cam_sync_merge(&sync_in_obj[0],
			prep_args->num_in_map_entries, &merged_sync_in_obj);
		if (rc) {
			prep_args->num_out_map_entries = 0;
			prep_args->num_in_map_entries = 0;
			return rc;
		}

		ope_request->in_resource = merged_sync_in_obj;

		prep_args->in_map_entries[0].sync_id = merged_sync_in_obj;
		prep_args->num_in_map_entries = 1;
		CAM_DBG(CAM_REQ, "ctx_id: %u req_id: %llu Merged Sync obj: %d",
			ctx_data->ctx_id, packet->header.request_id,
			merged_sync_in_obj);
	} else if (prep_args->num_in_map_entries == 1) {
		prep_args->in_map_entries[0].sync_id = sync_in_obj[0];
		prep_args->num_in_map_entries = 1;
		ope_request->in_resource = 0;
		CAM_DBG(CAM_OPE, "fence = %d", sync_in_obj[0]);
	} else {
		CAM_DBG(CAM_OPE, "Invalid count of input fences, count: %d",
			prep_args->num_in_map_entries);
		prep_args->num_in_map_entries = 0;
		ope_request->in_resource = 0;
		rc = -EINVAL;
	}
	return rc;
}

static void cam_ope_mgr_print_stripe_info(uint32_t batch,
	uint32_t io_buf, uint32_t plane, uint32_t stripe,
	struct ope_stripe_io *stripe_info, uint64_t iova_addr)
{
	CAM_DBG(CAM_OPE, "b:%d io:%d p:%d s:%d: E",
		batch, io_buf, plane, stripe);
	CAM_DBG(CAM_OPE, "width: %d s_h: %u s_s: %u",
		stripe_info->width, stripe_info->height,
		stripe_info->stride);
	CAM_DBG(CAM_OPE, "s_xinit = %u iova = %x s_loc = %u",
		stripe_info->x_init, iova_addr, stripe_info->s_location);
	CAM_DBG(CAM_OPE, "s_off = %u s_format = %u s_len = %u d_bus %d",
		stripe_info->offset, stripe_info->format,
		stripe_info->len, stripe_info->disable_bus);
	CAM_DBG(CAM_OPE, "s_align = %u s_pack = %u s_unpack = %u",
		stripe_info->alignment, stripe_info->pack_format,
		stripe_info->unpack_format);
	CAM_DBG(CAM_OPE, "b:%d io:%d p:%d s:%d: E",
		batch, io_buf, plane, stripe);
}

static int cam_ope_mgr_process_cmd_io_buf_req(struct cam_ope_hw_mgr *hw_mgr,
	struct cam_packet *packet, struct cam_ope_ctx *ctx_data,
	uintptr_t frame_process_addr, size_t length, uint32_t req_idx)
{
	int rc = 0;
	int i, j, k, l;
	dma_addr_t iova_addr;
	size_t len;
	struct ope_frame_process *in_frame_process;
	struct ope_frame_set *in_frame_set;
	struct ope_io_buf_info *in_io_buf;
	struct ope_stripe_info *in_stripe_info;
	struct cam_ope_request *ope_request;
	struct ope_io_buf *io_buf;
	struct ope_stripe_io *stripe_info;
	uint32_t alignment;
	uint32_t rsc_idx;
	uint32_t pack_format;
	uint32_t unpack_format;
	struct ope_in_res_info *in_res;
	struct ope_out_res_info *out_res;
	bool is_secure;

	in_frame_process = (struct ope_frame_process *)frame_process_addr;

	ope_request = ctx_data->req_list[req_idx];
	ope_request->num_batch = in_frame_process->batch_size;

	for (i = 0; i < in_frame_process->batch_size; i++) {
		in_frame_set = &in_frame_process->frame_set[i];
		for (j = 0; j < in_frame_set->num_io_bufs; j++) {
			in_io_buf = &in_frame_set->io_buf[j];
			for (k = 0; k < in_io_buf->num_planes; k++) {
				if (!in_io_buf->num_stripes[k]) {
					CAM_ERR(CAM_OPE, "Null num_stripes");
					return -EINVAL;
				}
				for (l = 0; l < in_io_buf->num_stripes[k];
					l++) {
					in_stripe_info =
						&in_io_buf->stripe_info[k][l];
				}
			}
		}
	}

	for (i = 0; i < ope_request->num_batch; i++) {
		in_frame_set = &in_frame_process->frame_set[i];
		ope_request->num_io_bufs[i] = in_frame_set->num_io_bufs;
		if (in_frame_set->num_io_bufs > OPE_MAX_IO_BUFS) {
			CAM_ERR(CAM_OPE, "Wrong number of io buffers: %d",
				in_frame_set->num_io_bufs);
			return -EINVAL;
		}

		for (j = 0; j < in_frame_set->num_io_bufs; j++) {
			in_io_buf = &in_frame_set->io_buf[j];
			ope_request->io_buf[i][j] =
				kzalloc(sizeof(struct ope_io_buf), GFP_KERNEL);
			if (!ope_request->io_buf[i][j]) {
				CAM_ERR(CAM_OPE,
					"IO config allocation failure");
				cam_ope_free_io_config(ope_request);
				return -ENOMEM;
			}
			io_buf = ope_request->io_buf[i][j];
			if (in_io_buf->num_planes > OPE_MAX_PLANES) {
				CAM_ERR(CAM_OPE, "wrong number of planes: %u",
					in_io_buf->num_planes);
				return -EINVAL;
			}

			io_buf->num_planes = in_io_buf->num_planes;
			io_buf->resource_type = in_io_buf->resource_type;
			io_buf->direction = in_io_buf->direction;
			io_buf->fence = in_io_buf->fence;
			io_buf->format = in_io_buf->format;

			rc = cam_ope_mgr_get_rsc_idx(ctx_data, in_io_buf);
			if (rc < 0) {
				CAM_ERR(CAM_OPE, "Invalid rsc idx = %d", rc);
				return rc;
			}
			rsc_idx = rc;
			if (in_io_buf->direction == CAM_BUF_INPUT) {
				in_res =
					&ctx_data->ope_acquire.in_res[rsc_idx];
				alignment = in_res->alignment;
				unpack_format = in_res->unpacker_format;
				pack_format = 0;
			} else if (in_io_buf->direction == CAM_BUF_OUTPUT) {
				out_res =
					&ctx_data->ope_acquire.out_res[rsc_idx];
				alignment = out_res->alignment;
				pack_format = out_res->packer_format;
				unpack_format = 0;
			}

			for (k = 0; k < in_io_buf->num_planes; k++) {
				io_buf->num_stripes[k] =
					in_io_buf->num_stripes[k];
				is_secure = cam_mem_is_secure_buf(
					in_io_buf->mem_handle[k]);
				if (is_secure)
					rc = cam_mem_get_io_buf(
						in_io_buf->mem_handle[k],
						hw_mgr->iommu_sec_hdl,
						&iova_addr, &len);
				else
					rc = cam_mem_get_io_buf(
						in_io_buf->mem_handle[k],
						hw_mgr->iommu_hdl,
						&iova_addr, &len);

				if (rc) {
					CAM_ERR(CAM_OPE, "get buf failed: %d",
						rc);
					return -EINVAL;
				}
				if (len < in_io_buf->length[k]) {
					CAM_ERR(CAM_OPE, "Invalid length");
					return -EINVAL;
				}
				iova_addr += in_io_buf->plane_offset[k];
				CAM_DBG(CAM_OPE,
					"E rsc %d stripes %d dir %d plane %d",
					in_io_buf->resource_type,
					in_io_buf->direction,
					in_io_buf->num_stripes[k], k);
				for (l = 0; l < in_io_buf->num_stripes[k];
					l++) {
					in_stripe_info =
						&in_io_buf->stripe_info[k][l];
					stripe_info = &io_buf->s_io[k][l];
					stripe_info->offset =
						in_stripe_info->offset;
					stripe_info->format = in_io_buf->format;
					stripe_info->s_location =
						in_stripe_info->stripe_location;
					stripe_info->iova_addr =
						iova_addr + stripe_info->offset;
					stripe_info->width =
						in_stripe_info->width;
					stripe_info->height =
						in_stripe_info->height;
					stripe_info->stride =
						in_io_buf->plane_stride[k];
					stripe_info->x_init =
						in_stripe_info->x_init;
					stripe_info->len = len;
					stripe_info->alignment = alignment;
					stripe_info->pack_format = pack_format;
					stripe_info->unpack_format =
						unpack_format;
					stripe_info->disable_bus =
						in_stripe_info->disable_bus;
					cam_ope_mgr_print_stripe_info(i, j,
						k, l, stripe_info, iova_addr);
				}
				CAM_DBG(CAM_OPE,
					"X rsc %d stripes %d dir %d plane %d",
					in_io_buf->resource_type,
					in_io_buf->direction,
					in_io_buf->num_stripes[k], k);
			}
		}
	}

	return rc;
}

static int cam_ope_mgr_process_cmd_buf_req(struct cam_ope_hw_mgr *hw_mgr,
	struct cam_packet *packet, struct cam_ope_ctx *ctx_data,
	uintptr_t frame_process_addr, size_t length, uint32_t req_idx)
{
	int rc = 0;
	int i, j;
	dma_addr_t iova_addr;
	dma_addr_t iova_cdm_addr;
	uintptr_t cpu_addr;
	size_t len;
	struct ope_frame_process *frame_process;
	struct ope_cmd_buf_info *cmd_buf;
	struct cam_ope_request *ope_request;
	bool is_kmd_buf_valid = false;

	frame_process = (struct ope_frame_process *)frame_process_addr;

	if (frame_process->batch_size > OPE_MAX_BATCH_SIZE) {
		CAM_ERR(CAM_OPE, "Invalid batch: %d",
			frame_process->batch_size);
		return -EINVAL;
	}

	for (i = 0; i < frame_process->batch_size; i++) {
		if (frame_process->num_cmd_bufs[i] > OPE_MAX_CMD_BUFS) {
			CAM_ERR(CAM_OPE, "Invalid cmd bufs for batch %d %d",
				i, frame_process->num_cmd_bufs[i]);
			return -EINVAL;
		}
	}

	CAM_DBG(CAM_OPE, "cmd buf for req id = %lld b_size = %d",
		packet->header.request_id, frame_process->batch_size);

	for (i = 0; i < frame_process->batch_size; i++) {
		CAM_DBG(CAM_OPE, "batch: %d count %d", i,
			frame_process->num_cmd_bufs[i]);
		for (j = 0; j < frame_process->num_cmd_bufs[i]; j++) {
			CAM_DBG(CAM_OPE, "batch: %d cmd_buf_idx :%d mem_hdl:%x",
				i, j, frame_process->cmd_buf[i][j].mem_handle);
			CAM_DBG(CAM_OPE, "size = %u scope = %d buf_type = %d",
				frame_process->cmd_buf[i][j].size,
				frame_process->cmd_buf[i][j].cmd_buf_scope,
				frame_process->cmd_buf[i][j].type);
			CAM_DBG(CAM_OPE, "usage = %d buffered = %d s_idx = %d",
			frame_process->cmd_buf[i][j].cmd_buf_usage,
			frame_process->cmd_buf[i][j].cmd_buf_buffered,
			frame_process->cmd_buf[i][j].stripe_idx);
		}
	}

	ope_request = ctx_data->req_list[req_idx];
	ope_request->num_batch = frame_process->batch_size;

	for (i = 0; i < frame_process->batch_size; i++) {
		for (j = 0; j < frame_process->num_cmd_bufs[i]; j++) {
			cmd_buf = &frame_process->cmd_buf[i][j];

			switch (cmd_buf->cmd_buf_scope) {
			case OPE_CMD_BUF_SCOPE_FRAME: {
				rc = cam_mem_get_io_buf(cmd_buf->mem_handle,
					hw_mgr->iommu_hdl, &iova_addr, &len);
				if (rc) {
					CAM_ERR(CAM_OPE, "get cmd buffailed %x",
						hw_mgr->iommu_hdl);
					goto end;
				}
				iova_addr = iova_addr + cmd_buf->offset;

				rc = cam_mem_get_io_buf(cmd_buf->mem_handle,
					hw_mgr->iommu_cdm_hdl,
					&iova_cdm_addr, &len);
				if (rc) {
					CAM_ERR(CAM_OPE, "get cmd buffailed %x",
						hw_mgr->iommu_hdl);
					goto end;
				}
				iova_cdm_addr = iova_cdm_addr + cmd_buf->offset;

				rc = cam_mem_get_cpu_buf(cmd_buf->mem_handle,
					&cpu_addr, &len);
				if (rc || !cpu_addr) {
					CAM_ERR(CAM_OPE, "get cmd buffailed %x",
						hw_mgr->iommu_hdl);
					goto end;
				}
				cpu_addr = cpu_addr +
					frame_process->cmd_buf[i][j].offset;
				CAM_DBG(CAM_OPE, "Hdl %x size %d len %d off %d",
					cmd_buf->mem_handle, cmd_buf->size,
					cmd_buf->length,
					cmd_buf->offset);
				if (cmd_buf->cmd_buf_usage == OPE_CMD_BUF_KMD) {
					ope_request->ope_kmd_buf.mem_handle =
						cmd_buf->mem_handle;
					ope_request->ope_kmd_buf.cpu_addr =
						cpu_addr;
					ope_request->ope_kmd_buf.iova_addr =
						iova_addr;
					ope_request->ope_kmd_buf.iova_cdm_addr =
						iova_cdm_addr;
					ope_request->ope_kmd_buf.len = len;
					ope_request->ope_kmd_buf.offset =
						cmd_buf->offset;
					ope_request->ope_kmd_buf.size =
						cmd_buf->size;
					is_kmd_buf_valid = true;
					CAM_DBG(CAM_OPE, "kbuf:%x io:%x cdm:%x",
					ope_request->ope_kmd_buf.cpu_addr,
					ope_request->ope_kmd_buf.iova_addr,
					ope_request->ope_kmd_buf.iova_cdm_addr);
					break;
				} else if (cmd_buf->cmd_buf_usage ==
					OPE_CMD_BUF_DEBUG) {
					ope_request->ope_debug_buf.cpu_addr =
						cpu_addr;
					ope_request->ope_debug_buf.iova_addr =
						iova_addr;
					ope_request->ope_debug_buf.len =
						cmd_buf->length;
					ope_request->ope_debug_buf.size =
						len;
					ope_request->ope_debug_buf.offset =
						cmd_buf->offset;
					CAM_DBG(CAM_OPE, "dbg buf = %x",
					ope_request->ope_debug_buf.cpu_addr);
					break;
				}
				break;
			}
			case OPE_CMD_BUF_SCOPE_STRIPE: {
				uint32_t num_cmd_bufs = 0;
				uint32_t s_idx = 0;

				s_idx = cmd_buf->stripe_idx;
				num_cmd_bufs =
				ope_request->num_stripe_cmd_bufs[i][s_idx];

				if (!num_cmd_bufs)
					ope_request->num_stripes[i]++;

				ope_request->num_stripe_cmd_bufs[i][s_idx]++;
				break;
			}

			default:
				break;
			}
		}
	}


	for (i = 0; i < frame_process->batch_size; i++) {
		CAM_DBG(CAM_OPE, "num of stripes for batch %d is %d",
			i, ope_request->num_stripes[i]);
		for (j = 0; j < ope_request->num_stripes[i]; j++) {
			CAM_DBG(CAM_OPE, "cmd buffers for stripe: %d:%d is %d",
				i, j, ope_request->num_stripe_cmd_bufs[i][j]);
		}
	}

	if (!is_kmd_buf_valid) {
		CAM_DBG(CAM_OPE, "Invalid kmd buffer");
		rc = -EINVAL;
	}
end:
	return rc;
}

static int cam_ope_mgr_process_cmd_desc(struct cam_ope_hw_mgr *hw_mgr,
	struct cam_packet *packet, struct cam_ope_ctx *ctx_data,
	uintptr_t *ope_cmd_buf_addr, uint32_t req_idx)
{
	int rc = 0;
	int i;
	int num_cmd_buf = 0;
	size_t len;
	struct cam_cmd_buf_desc *cmd_desc = NULL;
	uintptr_t cpu_addr = 0;
	struct cam_ope_request *ope_request;

	cmd_desc = (struct cam_cmd_buf_desc *)
		((uint32_t *) &packet->payload + packet->cmd_buf_offset/4);

	*ope_cmd_buf_addr = 0;
	for (i = 0; i < packet->num_cmd_buf; i++, num_cmd_buf++) {
		if (cmd_desc[i].type != CAM_CMD_BUF_GENERIC ||
			cmd_desc[i].meta_data == OPE_CMD_META_GENERIC_BLOB)
			continue;

		rc = cam_mem_get_cpu_buf(cmd_desc[i].mem_handle,
			&cpu_addr, &len);
		if (rc || !cpu_addr) {
			CAM_ERR(CAM_OPE, "get cmd buf failed %x",
				hw_mgr->iommu_hdl);
			goto end;
		}
		if ((len <= cmd_desc[i].offset) ||
			(cmd_desc[i].size < cmd_desc[i].length) ||
			((len - cmd_desc[i].offset) <
			cmd_desc[i].length)) {
			CAM_ERR(CAM_OPE, "Invalid offset or length");
			goto end;
		}
		cpu_addr = cpu_addr + cmd_desc[i].offset;
		*ope_cmd_buf_addr = cpu_addr;
	}

	if (!cpu_addr) {
		CAM_ERR(CAM_OPE, "invalid number of cmd buf");
		*ope_cmd_buf_addr = 0;
		return -EINVAL;
	}

	ope_request = ctx_data->req_list[req_idx];
	ope_request->request_id = packet->header.request_id;
	ope_request->req_idx = req_idx;

	rc = cam_ope_mgr_process_cmd_buf_req(hw_mgr, packet, ctx_data,
		cpu_addr, len, req_idx);
	if (rc) {
		CAM_ERR(CAM_OPE, "Process OPE cmd request is failed: %d", rc);
		goto end;
	}

	rc = cam_ope_mgr_process_cmd_io_buf_req(hw_mgr, packet, ctx_data,
		cpu_addr, len, req_idx);
	if (rc) {
		CAM_ERR(CAM_OPE, "Process OPE cmd io request is failed: %d",
			rc);
		goto end;
	}

	return rc;

end:
	*ope_cmd_buf_addr = 0;
	return rc;
}

static bool cam_ope_mgr_is_valid_inconfig(struct cam_packet *packet)
{
	int i, num_in_map_entries = 0;
	bool in_config_valid = false;
	struct cam_buf_io_cfg *io_cfg_ptr = NULL;

	io_cfg_ptr = (struct cam_buf_io_cfg *) ((uint32_t *) &packet->payload +
					packet->io_configs_offset/4);

	for (i = 0 ; i < packet->num_io_configs; i++)
		if (io_cfg_ptr[i].direction == CAM_BUF_INPUT)
			num_in_map_entries++;

	if (num_in_map_entries <= OPE_IN_RES_MAX) {
		in_config_valid = true;
	} else {
		CAM_ERR(CAM_OPE, "In config entries(%u) more than allowed(%u)",
				num_in_map_entries, OPE_IN_RES_MAX);
	}

	CAM_DBG(CAM_OPE, "number of in_config info: %u %u %u %u",
			packet->num_io_configs, OPE_MAX_IO_BUFS,
			num_in_map_entries, OPE_IN_RES_MAX);

	return in_config_valid;
}

static bool cam_ope_mgr_is_valid_outconfig(struct cam_packet *packet)
{
	int i, num_out_map_entries = 0;
	bool out_config_valid = false;
	struct cam_buf_io_cfg *io_cfg_ptr = NULL;

	io_cfg_ptr = (struct cam_buf_io_cfg *) ((uint32_t *) &packet->payload +
					packet->io_configs_offset/4);

	for (i = 0 ; i < packet->num_io_configs; i++)
		if (io_cfg_ptr[i].direction == CAM_BUF_OUTPUT)
			num_out_map_entries++;

	if (num_out_map_entries <= OPE_OUT_RES_MAX) {
		out_config_valid = true;
	} else {
		CAM_ERR(CAM_OPE, "Out config entries(%u) more than allowed(%u)",
				num_out_map_entries, OPE_OUT_RES_MAX);
	}

	CAM_DBG(CAM_OPE, "number of out_config info: %u %u %u %u",
			packet->num_io_configs, OPE_MAX_IO_BUFS,
			num_out_map_entries, OPE_OUT_RES_MAX);

	return out_config_valid;
}

static int cam_ope_mgr_pkt_validation(struct cam_packet *packet)
{
	if ((packet->header.op_code & 0xff) !=
		OPE_OPCODE_CONFIG) {
		CAM_ERR(CAM_OPE, "Invalid Opcode in pkt: %d",
			packet->header.op_code & 0xff);
		return -EINVAL;
	}

	if (packet->num_io_configs > OPE_MAX_IO_BUFS) {
		CAM_ERR(CAM_OPE, "Invalid number of io configs: %d %d",
			OPE_MAX_IO_BUFS, packet->num_io_configs);
		return -EINVAL;
	}

	if (packet->num_cmd_buf > OPE_PACKET_MAX_CMD_BUFS) {
		CAM_ERR(CAM_OPE, "Invalid number of cmd buffers: %d %d",
			OPE_PACKET_MAX_CMD_BUFS, packet->num_cmd_buf);
		return -EINVAL;
	}

	if (!cam_ope_mgr_is_valid_inconfig(packet) ||
		!cam_ope_mgr_is_valid_outconfig(packet)) {
		return -EINVAL;
	}

	CAM_DBG(CAM_OPE, "number of cmd/patch info: %u %u %u %u",
			packet->num_cmd_buf,
			packet->num_io_configs, OPE_MAX_IO_BUFS,
			packet->num_patches);
	return 0;
}

static int cam_ope_get_acquire_info(struct cam_ope_hw_mgr *hw_mgr,
	struct cam_hw_acquire_args *args,
	struct cam_ope_ctx *ctx)
{
	int i = 0;

	if (args->num_acq > 1) {
		CAM_ERR(CAM_OPE, "Invalid number of resources: %d",
			args->num_acq);
		return -EINVAL;
	}

	if (copy_from_user(&ctx->ope_acquire,
		(void __user *)args->acquire_info,
		sizeof(struct ope_acquire_dev_info))) {
		CAM_ERR(CAM_OPE, "Failed in acquire");
		return -EFAULT;
	}

	if (ctx->ope_acquire.secure_mode > CAM_SECURE_MODE_SECURE) {
		CAM_ERR(CAM_OPE, "Invalid mode:%d",
			ctx->ope_acquire.secure_mode);
		return -EINVAL;
	}

	if (ctx->ope_acquire.num_out_res > OPE_OUT_RES_MAX) {
		CAM_ERR(CAM_OPE, "num of out resources exceeding : %u",
			ctx->ope_acquire.num_out_res);
		return -EINVAL;
	}

	if (ctx->ope_acquire.num_in_res > OPE_IN_RES_MAX) {
		CAM_ERR(CAM_OPE, "num of in resources exceeding : %u",
			ctx->ope_acquire.num_in_res);
		return -EINVAL;
	}

	if (ctx->ope_acquire.dev_type >= OPE_DEV_TYPE_MAX) {
		CAM_ERR(CAM_OPE, "Invalid device type: %d",
			ctx->ope_acquire.dev_type);
		return -EFAULT;
	}

	if (ctx->ope_acquire.hw_type >= OPE_HW_TYPE_MAX) {
		CAM_ERR(CAM_OPE, "Invalid HW type: %d",
			ctx->ope_acquire.hw_type);
		return -EFAULT;
	}

	CAM_DBG(CAM_OPE, "top: %u %u %s %u %u %u %u %u",
		ctx->ope_acquire.hw_type, ctx->ope_acquire.dev_type,
		ctx->ope_acquire.dev_name,
		ctx->ope_acquire.nrt_stripes_for_arb,
		ctx->ope_acquire.secure_mode, ctx->ope_acquire.batch_size,
		ctx->ope_acquire.num_in_res, ctx->ope_acquire.num_out_res);

	for (i = 0; i < ctx->ope_acquire.num_in_res; i++) {
		CAM_DBG(CAM_OPE, "IN: %u %u %u %u %u %u %u %u",
		ctx->ope_acquire.in_res[i].res_id,
		ctx->ope_acquire.in_res[i].format,
		ctx->ope_acquire.in_res[i].width,
		ctx->ope_acquire.in_res[i].height,
		ctx->ope_acquire.in_res[i].alignment,
		ctx->ope_acquire.in_res[i].unpacker_format,
		ctx->ope_acquire.in_res[i].max_stripe_size,
		ctx->ope_acquire.in_res[i].fps);
	}

	for (i = 0; i < ctx->ope_acquire.num_out_res; i++) {
		CAM_DBG(CAM_OPE, "OUT: %u %u %u %u %u %u %u %u",
		ctx->ope_acquire.out_res[i].res_id,
		ctx->ope_acquire.out_res[i].format,
		ctx->ope_acquire.out_res[i].width,
		ctx->ope_acquire.out_res[i].height,
		ctx->ope_acquire.out_res[i].alignment,
		ctx->ope_acquire.out_res[i].packer_format,
		ctx->ope_acquire.out_res[i].subsample_period,
		ctx->ope_acquire.out_res[i].subsample_pattern);
	}

	return 0;
}

static int cam_ope_get_free_ctx(struct cam_ope_hw_mgr *hw_mgr)
{
	int i;

	i = find_first_zero_bit(hw_mgr->ctx_bitmap, hw_mgr->ctx_bits);
	if (i >= OPE_CTX_MAX || i < 0) {
		CAM_ERR(CAM_OPE, "Invalid ctx id = %d", i);
		return -EINVAL;
	}

	mutex_lock(&hw_mgr->ctx[i].ctx_mutex);
	if (hw_mgr->ctx[i].ctx_state != OPE_CTX_STATE_FREE) {
		CAM_ERR(CAM_OPE, "Invalid ctx %d state %d",
			i, hw_mgr->ctx[i].ctx_state);
		mutex_unlock(&hw_mgr->ctx[i].ctx_mutex);
		return -EINVAL;
	}
	set_bit(i, hw_mgr->ctx_bitmap);
	mutex_unlock(&hw_mgr->ctx[i].ctx_mutex);

	return i;
}


static int cam_ope_put_free_ctx(struct cam_ope_hw_mgr *hw_mgr, uint32_t ctx_id)
{
	if (ctx_id >= OPE_CTX_MAX) {
		CAM_ERR(CAM_OPE, "Invalid ctx_id: %d", ctx_id);
		return 0;
	}

	hw_mgr->ctx[ctx_id].ctx_state = OPE_CTX_STATE_FREE;
	clear_bit(ctx_id, hw_mgr->ctx_bitmap);

	return 0;
}

static int cam_ope_mgr_get_hw_caps(void *hw_priv, void *hw_caps_args)
{
	struct cam_ope_hw_mgr *hw_mgr;
	struct cam_query_cap_cmd *query_cap = hw_caps_args;
	struct ope_hw_ver hw_ver;
	int rc = 0, i;

	if (!hw_priv || !hw_caps_args) {
		CAM_ERR(CAM_OPE, "Invalid args: %x %x", hw_priv, hw_caps_args);
		return -EINVAL;
	}

	hw_mgr = hw_priv;
	mutex_lock(&hw_mgr->hw_mgr_mutex);
	if (copy_from_user(&hw_mgr->ope_caps,
		u64_to_user_ptr(query_cap->caps_handle),
		sizeof(struct ope_query_cap_cmd))) {
		CAM_ERR(CAM_OPE, "copy_from_user failed: size = %d",
			sizeof(struct ope_query_cap_cmd));
		rc = -EFAULT;
		goto end;
	}

	for (i = 0; i < hw_mgr->num_ope; i++) {
		rc = hw_mgr->ope_dev_intf[i]->hw_ops.get_hw_caps(
			hw_mgr->ope_dev_intf[i]->hw_priv,
			&hw_ver, sizeof(hw_ver));
		if (rc)
			goto end;

		hw_mgr->ope_caps.hw_ver[i] = hw_ver;
	}

	hw_mgr->ope_caps.dev_iommu_handle.non_secure = hw_mgr->iommu_hdl;
	hw_mgr->ope_caps.dev_iommu_handle.secure = hw_mgr->iommu_sec_hdl;
	hw_mgr->ope_caps.cdm_iommu_handle.non_secure = hw_mgr->iommu_cdm_hdl;
	hw_mgr->ope_caps.cdm_iommu_handle.secure = hw_mgr->iommu_sec_cdm_hdl;
	hw_mgr->ope_caps.num_ope = hw_mgr->num_ope;

	CAM_DBG(CAM_OPE, "iommu sec %d iommu ns %d cdm s %d cdm ns %d",
		hw_mgr->ope_caps.dev_iommu_handle.secure,
		hw_mgr->ope_caps.dev_iommu_handle.non_secure,
		hw_mgr->ope_caps.cdm_iommu_handle.secure,
		hw_mgr->ope_caps.cdm_iommu_handle.non_secure);

	if (copy_to_user(u64_to_user_ptr(query_cap->caps_handle),
		&hw_mgr->ope_caps, sizeof(struct ope_query_cap_cmd))) {
		CAM_ERR(CAM_OPE, "copy_to_user failed: size = %d",
			sizeof(struct ope_query_cap_cmd));
		rc = -EFAULT;
	}

end:
	mutex_unlock(&hw_mgr->hw_mgr_mutex);
	return rc;
}

static int cam_ope_mgr_acquire_hw(void *hw_priv, void *hw_acquire_args)
{
	int rc = 0, i;
	int ctx_id;
	struct cam_ope_hw_mgr *hw_mgr = hw_priv;
	struct cam_ope_ctx *ctx;
	struct cam_hw_acquire_args *args = hw_acquire_args;
	struct cam_ope_dev_acquire ope_dev_acquire;
	struct cam_ope_dev_release ope_dev_release;
	struct cam_cdm_acquire_data *cdm_acquire;
	struct cam_ope_dev_init init;
	struct cam_ope_dev_clk_update clk_update;
	struct cam_ope_dev_bw_update *bw_update;
	struct cam_ope_set_irq_cb irq_cb;
	struct cam_hw_info *dev = NULL;
	struct cam_hw_soc_info *soc_info = NULL;
	int32_t idx;

	if ((!hw_priv) || (!hw_acquire_args)) {
		CAM_ERR(CAM_OPE, "Invalid args: %x %x",
			hw_priv, hw_acquire_args);
		return -EINVAL;
	}

	mutex_lock(&hw_mgr->hw_mgr_mutex);
	ctx_id = cam_ope_get_free_ctx(hw_mgr);
	if (ctx_id < 0) {
		CAM_ERR(CAM_OPE, "No free ctx");
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		return ctx_id;
	}

	ctx = &hw_mgr->ctx[ctx_id];
	ctx->ctx_id = ctx_id;
	mutex_lock(&ctx->ctx_mutex);
	rc = cam_ope_get_acquire_info(hw_mgr, args, ctx);
	if (rc < 0) {
		CAM_ERR(CAM_OPE, "get_acquire info failed: %d", rc);
		goto end;
	}

	cdm_acquire = kzalloc(sizeof(struct cam_cdm_acquire_data), GFP_KERNEL);
	if (!cdm_acquire) {
		CAM_ERR(CAM_ISP, "Out of memory");
		goto end;
	}
	strlcpy(cdm_acquire->identifier, "ope", sizeof("ope"));
	if (ctx->ope_acquire.dev_type == OPE_DEV_TYPE_OPE_RT)
		cdm_acquire->priority = CAM_CDM_BL_FIFO_3;
	else if (ctx->ope_acquire.dev_type ==
		OPE_DEV_TYPE_OPE_NRT)
		cdm_acquire->priority = CAM_CDM_BL_FIFO_0;
	else
		goto free_cdm_acquire;

	cdm_acquire->cell_index = 0;
	cdm_acquire->handle = 0;
	cdm_acquire->userdata = ctx;
	cdm_acquire->cam_cdm_callback = cam_ope_ctx_cdm_callback;
	cdm_acquire->id = CAM_CDM_VIRTUAL;
	cdm_acquire->base_array_cnt = 1;
	cdm_acquire->base_array[0] = hw_mgr->cdm_reg_map[OPE_DEV_OPE][0];

	rc = cam_cdm_acquire(cdm_acquire);
	if (rc) {
		CAM_ERR(CAM_OPE, "cdm_acquire is failed: %d", rc);
		goto cdm_acquire_failed;
	}

	ctx->ope_cdm.cdm_ops = cdm_acquire->ops;
	ctx->ope_cdm.cdm_handle = cdm_acquire->handle;

	rc = cam_cdm_stream_on(cdm_acquire->handle);
	if (rc) {
		CAM_ERR(CAM_OPE, "cdm stream on failure: %d", rc);
		goto cdm_stream_on_failure;
	}

	if (!hw_mgr->ope_ctx_cnt) {
		for (i = 0; i < ope_hw_mgr->num_ope; i++) {
			init.hfi_en = ope_hw_mgr->hfi_en;
			rc = hw_mgr->ope_dev_intf[i]->hw_ops.init(
				hw_mgr->ope_dev_intf[i]->hw_priv, &init,
				sizeof(init));
			if (rc) {
				CAM_ERR(CAM_OPE, "OPE Dev init failed: %d", rc);
				goto ope_dev_init_failure;
			}
		}

		/* Install IRQ CB */
		irq_cb.ope_hw_mgr_cb = cam_ope_hw_mgr_cb;
		irq_cb.data = hw_mgr;
		for (i = 0; i < ope_hw_mgr->num_ope; i++) {
			init.hfi_en = ope_hw_mgr->hfi_en;
			rc = hw_mgr->ope_dev_intf[i]->hw_ops.process_cmd(
				hw_mgr->ope_dev_intf[i]->hw_priv,
				OPE_HW_SET_IRQ_CB,
				&irq_cb, sizeof(irq_cb));
			if (rc) {
				CAM_ERR(CAM_OPE, "OPE Dev init failed: %d", rc);
				goto ope_irq_set_failed;
			}
		}

		dev = (struct cam_hw_info *)hw_mgr->ope_dev_intf[0]->hw_priv;
		soc_info = &dev->soc_info;
		idx = soc_info->src_clk_idx;

		hw_mgr->clk_info.base_clk =
			soc_info->clk_rate[CAM_TURBO_VOTE][idx];
		hw_mgr->clk_info.threshold = 5;
		hw_mgr->clk_info.over_clked = 0;

		for (i = 0; i < CAM_OPE_MAX_PER_PATH_VOTES; i++) {
			hw_mgr->clk_info.axi_path[i].camnoc_bw = 0;
			hw_mgr->clk_info.axi_path[i].mnoc_ab_bw = 0;
			hw_mgr->clk_info.axi_path[i].mnoc_ib_bw = 0;
			hw_mgr->clk_info.axi_path[i].ddr_ab_bw = 0;
			hw_mgr->clk_info.axi_path[i].ddr_ib_bw = 0;
		}
	}

	ope_dev_acquire.ctx_id = ctx_id;
	ope_dev_acquire.ope_acquire = &ctx->ope_acquire;

	for (i = 0; i < ope_hw_mgr->num_ope; i++) {
		rc = hw_mgr->ope_dev_intf[i]->hw_ops.process_cmd(
			hw_mgr->ope_dev_intf[i]->hw_priv, OPE_HW_ACQUIRE,
			&ope_dev_acquire, sizeof(ope_dev_acquire));
		if (rc) {
			CAM_ERR(CAM_OPE, "OPE Dev acquire failed: %d", rc);
			goto ope_dev_acquire_failed;
		}
	}

	for (i = 0; i < ope_hw_mgr->num_ope; i++) {
		dev = (struct cam_hw_info *)hw_mgr->ope_dev_intf[i]->hw_priv;
		soc_info = &dev->soc_info;
		idx = soc_info->src_clk_idx;
		clk_update.clk_rate = soc_info->clk_rate[CAM_TURBO_VOTE][idx];
		hw_mgr->clk_info.curr_clk =
			soc_info->clk_rate[CAM_TURBO_VOTE][idx];

		rc = hw_mgr->ope_dev_intf[i]->hw_ops.process_cmd(
			hw_mgr->ope_dev_intf[i]->hw_priv, OPE_HW_CLK_UPDATE,
			&clk_update, sizeof(clk_update));
		if (rc) {
			CAM_ERR(CAM_OPE, "OPE Dev clk update failed: %d", rc);
			goto ope_clk_update_failed;
		}
	}

	bw_update = kzalloc(sizeof(struct cam_ope_dev_bw_update), GFP_KERNEL);
	if (!bw_update) {
		CAM_ERR(CAM_ISP, "Out of memory");
		goto ope_clk_update_failed;
	}
	bw_update->ahb_vote_valid = false;
	for (i = 0; i < ope_hw_mgr->num_ope; i++) {
		bw_update->axi_vote.num_paths = 1;
		bw_update->axi_vote_valid = true;
		bw_update->axi_vote.axi_path[0].camnoc_bw = 600000000;
		bw_update->axi_vote.axi_path[0].mnoc_ab_bw = 600000000;
		bw_update->axi_vote.axi_path[0].mnoc_ib_bw = 600000000;
		bw_update->axi_vote.axi_path[0].ddr_ab_bw = 600000000;
		bw_update->axi_vote.axi_path[0].ddr_ib_bw = 600000000;
		bw_update->axi_vote.axi_path[0].transac_type =
			CAM_AXI_TRANSACTION_WRITE;
		bw_update->axi_vote.axi_path[0].path_data_type =
			CAM_AXI_PATH_DATA_ALL;
		rc = hw_mgr->ope_dev_intf[i]->hw_ops.process_cmd(
			hw_mgr->ope_dev_intf[i]->hw_priv, OPE_HW_BW_UPDATE,
			bw_update, sizeof(*bw_update));
		if (rc) {
			CAM_ERR(CAM_OPE, "OPE Dev clk update failed: %d", rc);
			goto free_bw_update;
		}
	}

	cam_ope_start_req_timer(ctx);
	cam_ope_device_timer_start(hw_mgr);
	hw_mgr->ope_ctx_cnt++;
	ctx->context_priv = args->context_data;
	args->ctxt_to_hw_map = ctx;
	ctx->ctxt_event_cb = args->event_cb;
	cam_ope_ctx_clk_info_init(ctx);
	ctx->ctx_state = OPE_CTX_STATE_ACQUIRED;

	mutex_unlock(&ctx->ctx_mutex);
	mutex_unlock(&hw_mgr->hw_mgr_mutex);

	CAM_INFO(CAM_OPE, "OPE: %d acquire succesfull rc %d", ctx_id, rc);
	return rc;

free_bw_update:
	kzfree(bw_update);
	bw_update = NULL;
ope_clk_update_failed:
	ope_dev_release.ctx_id = ctx_id;
	for (i = 0; i < ope_hw_mgr->num_ope; i++) {
		if (hw_mgr->ope_dev_intf[i]->hw_ops.process_cmd(
			hw_mgr->ope_dev_intf[i]->hw_priv, OPE_HW_RELEASE,
			&ope_dev_release, sizeof(ope_dev_release)))
			CAM_ERR(CAM_OPE, "OPE Dev release failed");
	}
ope_dev_acquire_failed:
	if (!hw_mgr->ope_ctx_cnt) {
		irq_cb.ope_hw_mgr_cb = NULL;
		irq_cb.data = hw_mgr;
		for (i = 0; i < ope_hw_mgr->num_ope; i++) {
			init.hfi_en = ope_hw_mgr->hfi_en;
			if (hw_mgr->ope_dev_intf[i]->hw_ops.process_cmd(
				hw_mgr->ope_dev_intf[i]->hw_priv,
				OPE_HW_SET_IRQ_CB,
				&irq_cb, sizeof(irq_cb)))
				CAM_ERR(CAM_OPE,
					"OPE IRQ de register failed");
		}
	}
ope_irq_set_failed:
	if (!hw_mgr->ope_ctx_cnt) {
		for (i = 0; i < ope_hw_mgr->num_ope; i++) {
			if (hw_mgr->ope_dev_intf[i]->hw_ops.deinit(
				hw_mgr->ope_dev_intf[i]->hw_priv, NULL, 0))
				CAM_ERR(CAM_OPE, "OPE deinit fail");
			if (hw_mgr->ope_dev_intf[i]->hw_ops.stop(
				hw_mgr->ope_dev_intf[i]->hw_priv,
				NULL, 0))
				CAM_ERR(CAM_OPE, "OPE stop fail");
		}
	}
ope_dev_init_failure:
cdm_stream_on_failure:
	cam_cdm_release(cdm_acquire->handle);
	ctx->ope_cdm.cdm_ops = NULL;
	ctx->ope_cdm.cdm_handle = 0;

cdm_acquire_failed:
free_cdm_acquire:
	kzfree(cdm_acquire);
	cdm_acquire = NULL;
end:
	args->ctxt_to_hw_map = NULL;
	cam_ope_put_free_ctx(hw_mgr, ctx_id);
	mutex_unlock(&ctx->ctx_mutex);
	mutex_unlock(&hw_mgr->hw_mgr_mutex);
	return rc;
}

static int cam_ope_mgr_remove_bw(struct cam_ope_hw_mgr *hw_mgr, int ctx_id)
{
	int i, path_index, rc = 0;
	struct cam_ope_ctx *ctx_data = NULL;
	struct cam_ope_clk_info *hw_mgr_clk_info;

	ctx_data = &hw_mgr->ctx[ctx_id];
	hw_mgr_clk_info = &hw_mgr->clk_info;

	for (i = 0; i < ctx_data->clk_info.num_paths; i++) {
		path_index =
		ctx_data->clk_info.axi_path[i].path_data_type -
		CAM_AXI_PATH_DATA_OPE_START_OFFSET;

		if (path_index >= CAM_OPE_MAX_PER_PATH_VOTES) {
			CAM_WARN(CAM_OPE,
				"Invalid path %d, start offset=%d, max=%d",
				ctx_data->clk_info.axi_path[i].path_data_type,
				CAM_AXI_PATH_DATA_OPE_START_OFFSET,
				CAM_OPE_MAX_PER_PATH_VOTES);
			continue;
		}

		hw_mgr_clk_info->axi_path[path_index].camnoc_bw -=
			ctx_data->clk_info.axi_path[i].camnoc_bw;
		hw_mgr_clk_info->axi_path[path_index].mnoc_ab_bw -=
			ctx_data->clk_info.axi_path[i].mnoc_ab_bw;
		hw_mgr_clk_info->axi_path[path_index].mnoc_ib_bw -=
			ctx_data->clk_info.axi_path[i].mnoc_ib_bw;
		hw_mgr_clk_info->axi_path[path_index].ddr_ab_bw -=
			ctx_data->clk_info.axi_path[i].ddr_ab_bw;
		hw_mgr_clk_info->axi_path[path_index].ddr_ib_bw -=
			ctx_data->clk_info.axi_path[i].ddr_ib_bw;
	}

	rc = cam_ope_update_cpas_vote(hw_mgr, ctx_data);

	return rc;
}

static int cam_ope_mgr_ope_clk_remove(struct cam_ope_hw_mgr *hw_mgr, int ctx_id)
{
	struct cam_ope_ctx *ctx_data = NULL;
	struct cam_ope_clk_info *hw_mgr_clk_info;

	ctx_data = &hw_mgr->ctx[ctx_id];
	hw_mgr_clk_info = &hw_mgr->clk_info;

	if (hw_mgr_clk_info->base_clk >= ctx_data->clk_info.base_clk)
		hw_mgr_clk_info->base_clk -= ctx_data->clk_info.base_clk;

	/* reset clock info */
	ctx_data->clk_info.curr_fc = 0;
	ctx_data->clk_info.base_clk = 0;

	return 0;
}

static int cam_ope_mgr_release_ctx(struct cam_ope_hw_mgr *hw_mgr, int ctx_id)
{
	int i = 0, rc = 0;
	struct cam_ope_dev_release ope_dev_release;

	if (ctx_id >= OPE_CTX_MAX) {
		CAM_ERR(CAM_OPE, "ctx_id is wrong: %d", ctx_id);
		return -EINVAL;
	}

	mutex_lock(&hw_mgr->ctx[ctx_id].ctx_mutex);
	if (hw_mgr->ctx[ctx_id].ctx_state !=
		OPE_CTX_STATE_ACQUIRED) {
		mutex_unlock(&hw_mgr->ctx[ctx_id].ctx_mutex);
		CAM_DBG(CAM_OPE, "ctx id: %d not in right state: %d",
			ctx_id, hw_mgr->ctx[ctx_id].ctx_state);
		return 0;
	}

	hw_mgr->ctx[ctx_id].ctx_state = OPE_CTX_STATE_RELEASE;

	for (i = 0; i < ope_hw_mgr->num_ope; i++) {
		ope_dev_release.ctx_id = ctx_id;
		rc = hw_mgr->ope_dev_intf[i]->hw_ops.process_cmd(
			hw_mgr->ope_dev_intf[i]->hw_priv, OPE_HW_RELEASE,
			&ope_dev_release, sizeof(ope_dev_release));
		if (rc)
			CAM_ERR(CAM_OPE, "OPE Dev release failed: %d", rc);
	}

	rc = cam_cdm_stream_off(hw_mgr->ctx[ctx_id].ope_cdm.cdm_handle);
	if (rc)
		CAM_ERR(CAM_OPE, "OPE CDM streamoff failed: %d", rc);

	rc = cam_cdm_release(hw_mgr->ctx[ctx_id].ope_cdm.cdm_handle);
	if (rc)
		CAM_ERR(CAM_OPE, "OPE CDM relase failed: %d", rc);


	for (i = 0; i < CAM_CTX_REQ_MAX; i++) {
		if (!hw_mgr->ctx[ctx_id].req_list[i])
			continue;

		if (hw_mgr->ctx[ctx_id].req_list[i]->cdm_cmd) {
			kzfree(hw_mgr->ctx[ctx_id].req_list[i]->cdm_cmd);
			hw_mgr->ctx[ctx_id].req_list[i]->cdm_cmd = NULL;
		}
		cam_ope_free_io_config(hw_mgr->ctx[ctx_id].req_list[i]);
		kzfree(hw_mgr->ctx[ctx_id].req_list[i]);
		hw_mgr->ctx[ctx_id].req_list[i] = NULL;
		clear_bit(i, hw_mgr->ctx[ctx_id].bitmap);
	}

	cam_ope_req_timer_stop(&hw_mgr->ctx[ctx_id]);
	hw_mgr->ctx[ctx_id].ope_cdm.cdm_handle = 0;
	hw_mgr->ctx[ctx_id].req_cnt = 0;
	hw_mgr->ctx[ctx_id].last_flush_req = 0;
	cam_ope_put_free_ctx(hw_mgr, ctx_id);

	rc = cam_ope_mgr_ope_clk_remove(hw_mgr, ctx_id);
	if (rc)
		CAM_ERR(CAM_OPE, "OPE clk update failed: %d", rc);

	hw_mgr->ope_ctx_cnt--;
	mutex_unlock(&hw_mgr->ctx[ctx_id].ctx_mutex);
	CAM_DBG(CAM_OPE, "X: ctx_id = %d", ctx_id);

	return 0;
}

static int cam_ope_mgr_release_hw(void *hw_priv, void *hw_release_args)
{
	int i, rc = 0;
	int ctx_id = 0;
	struct cam_hw_release_args *release_hw = hw_release_args;
	struct cam_ope_hw_mgr *hw_mgr = hw_priv;
	struct cam_ope_ctx *ctx_data = NULL;
	struct cam_ope_set_irq_cb irq_cb;
	struct cam_hw_intf *dev_intf;

	if (!release_hw || !hw_mgr) {
		CAM_ERR(CAM_OPE, "Invalid args: %pK %pK", release_hw, hw_mgr);
		return -EINVAL;
	}

	ctx_data = release_hw->ctxt_to_hw_map;
	if (!ctx_data) {
		CAM_ERR(CAM_OPE, "NULL ctx data");
		return -EINVAL;
	}

	ctx_id = ctx_data->ctx_id;
	if (ctx_id < 0 || ctx_id >= OPE_CTX_MAX) {
		CAM_ERR(CAM_OPE, "Invalid ctx id: %d", ctx_id);
		return -EINVAL;
	}

	mutex_lock(&hw_mgr->ctx[ctx_id].ctx_mutex);
	if (hw_mgr->ctx[ctx_id].ctx_state != OPE_CTX_STATE_ACQUIRED) {
		CAM_DBG(CAM_OPE, "ctx is not in use: %d", ctx_id);
		mutex_unlock(&hw_mgr->ctx[ctx_id].ctx_mutex);
		return -EINVAL;
	}
	mutex_unlock(&hw_mgr->ctx[ctx_id].ctx_mutex);

	mutex_lock(&hw_mgr->hw_mgr_mutex);
	rc = cam_ope_mgr_release_ctx(hw_mgr, ctx_id);
	if (!hw_mgr->ope_ctx_cnt) {
		CAM_DBG(CAM_OPE, "Last Release");
		for (i = 0; i < ope_hw_mgr->num_ope; i++) {
			dev_intf = hw_mgr->ope_dev_intf[i];
			irq_cb.ope_hw_mgr_cb = NULL;
			irq_cb.data = NULL;
			rc = dev_intf->hw_ops.process_cmd(
				hw_mgr->ope_dev_intf[i]->hw_priv,
				OPE_HW_SET_IRQ_CB,
				&irq_cb, sizeof(irq_cb));
			if (rc)
				CAM_ERR(CAM_OPE, "IRQ dereg failed: %d", rc);
		}
		for (i = 0; i < ope_hw_mgr->num_ope; i++) {
			dev_intf = hw_mgr->ope_dev_intf[i];
			rc = dev_intf->hw_ops.deinit(
				hw_mgr->ope_dev_intf[i]->hw_priv,
				NULL, 0);
			if (rc)
				CAM_ERR(CAM_OPE, "deinit failed: %d", rc);
		}
		cam_ope_device_timer_stop(hw_mgr);
	}

	rc = cam_ope_mgr_remove_bw(hw_mgr, ctx_id);
	if (rc)
		CAM_ERR(CAM_OPE, "OPE remove bw failed: %d", rc);

	if (!hw_mgr->ope_ctx_cnt) {
		for (i = 0; i < ope_hw_mgr->num_ope; i++) {
			dev_intf = hw_mgr->ope_dev_intf[i];
			rc = dev_intf->hw_ops.stop(
				hw_mgr->ope_dev_intf[i]->hw_priv,
				NULL, 0);
			if (rc)
				CAM_ERR(CAM_OPE, "stop failed: %d", rc);
		}
	}

	mutex_unlock(&hw_mgr->hw_mgr_mutex);

	CAM_DBG(CAM_OPE, "Release done for ctx_id %d", ctx_id);
	return rc;
}

static int cam_ope_packet_generic_blob_handler(void *user_data,
	uint32_t blob_type, uint32_t blob_size, uint8_t *blob_data)
{
	struct cam_ope_clk_bw_request *clk_info;
	struct ope_clk_bw_request_v2 *soc_req_v2;
	struct cam_ope_clk_bw_req_internal_v2 *clk_info_v2;
	struct ope_cmd_generic_blob *blob;
	struct cam_ope_ctx *ctx_data;
	uint32_t index;
	size_t clk_update_size;
	int rc = 0;

	if (!blob_data || (blob_size == 0)) {
		CAM_ERR(CAM_OPE, "Invalid blob info %pK %d", blob_data,
		blob_size);
		return -EINVAL;
	}

	blob = (struct ope_cmd_generic_blob *)user_data;
	ctx_data = blob->ctx;
	index = blob->req_idx;

	switch (blob_type) {
	case OPE_CMD_GENERIC_BLOB_CLK_V2:
		if (blob_size < sizeof(struct ope_clk_bw_request_v2)) {
			CAM_ERR(CAM_OPE, "Mismatch blob size %d expected %lu",
				blob_size,
				sizeof(struct ope_clk_bw_request_v2));
			return -EINVAL;
		}

		soc_req_v2 = (struct ope_clk_bw_request_v2 *)blob_data;
		if (soc_req_v2->num_paths > CAM_OPE_MAX_PER_PATH_VOTES) {
			CAM_ERR(CAM_OPE, "Invalid num paths: %d",
				soc_req_v2->num_paths);
			return -EINVAL;
		}

		/* Check for integer overflow */
		if (soc_req_v2->num_paths != 1) {
			if (sizeof(struct cam_axi_per_path_bw_vote) >
				((UINT_MAX -
				sizeof(struct ope_clk_bw_request_v2)) /
				(soc_req_v2->num_paths - 1))) {
				CAM_ERR(CAM_OPE,
					"Size exceeds limit paths:%u size per path:%lu",
					soc_req_v2->num_paths - 1,
					sizeof(
					struct cam_axi_per_path_bw_vote));
			return -EINVAL;
			}
		}

		clk_update_size = sizeof(struct ope_clk_bw_request_v2) +
			((soc_req_v2->num_paths - 1) *
			sizeof(struct cam_axi_per_path_bw_vote));
		if (blob_size < clk_update_size) {
			CAM_ERR(CAM_OPE, "Invalid blob size: %u",
				blob_size);
			return -EINVAL;
		}

		clk_info = &ctx_data->req_list[index]->clk_info;
		clk_info_v2 = &ctx_data->req_list[index]->clk_info_v2;

		memcpy(clk_info_v2, soc_req_v2, clk_update_size);

		/* Use v1 structure for clk fields */
		clk_info->budget_ns = clk_info_v2->budget_ns;
		clk_info->frame_cycles = clk_info_v2->frame_cycles;
		clk_info->rt_flag = clk_info_v2->rt_flag;

		CAM_DBG(CAM_OPE, "budget=%llu, frame_cycle=%llu, rt_flag=%d",
			clk_info_v2->budget_ns, clk_info_v2->frame_cycles,
			clk_info_v2->rt_flag);
		break;

	default:
		CAM_WARN(CAM_OPE, "Invalid blob type %d", blob_type);
		break;
	}
	return rc;
}

static int cam_ope_process_generic_cmd_buffer(
	struct cam_packet *packet,
	struct cam_ope_ctx *ctx_data,
	int32_t index,
	uint64_t *io_buf_addr)
{
	int i, rc = 0;
	struct cam_cmd_buf_desc *cmd_desc = NULL;
	struct ope_cmd_generic_blob cmd_generic_blob;

	cmd_generic_blob.ctx = ctx_data;
	cmd_generic_blob.req_idx = index;
	cmd_generic_blob.io_buf_addr = io_buf_addr;

	cmd_desc = (struct cam_cmd_buf_desc *)
		((uint32_t *) &packet->payload + packet->cmd_buf_offset/4);

	for (i = 0; i < packet->num_cmd_buf; i++) {
		if (!cmd_desc[i].length)
			continue;

	if (cmd_desc[i].meta_data != OPE_CMD_META_GENERIC_BLOB)
		continue;

	rc = cam_packet_util_process_generic_cmd_buffer(&cmd_desc[i],
		cam_ope_packet_generic_blob_handler, &cmd_generic_blob);
	if (rc)
		CAM_ERR(CAM_OPE, "Failed in processing blobs %d", rc);
	}

	return rc;
}

static int cam_ope_mgr_prepare_hw_update(void *hw_priv,
	void *hw_prepare_update_args)
{
	int rc = 0;
	struct cam_packet *packet = NULL;
	struct cam_ope_hw_mgr *hw_mgr = hw_priv;
	struct cam_hw_prepare_update_args *prepare_args =
		hw_prepare_update_args;
	struct cam_ope_ctx *ctx_data = NULL;
	uintptr_t   ope_cmd_buf_addr;
	uint32_t request_idx = 0;
	struct cam_ope_request *ope_req;
	struct timespec64 ts;

	if ((!prepare_args) || (!hw_mgr) || (!prepare_args->packet)) {
		CAM_ERR(CAM_OPE, "Invalid args: %x %x",
			prepare_args, hw_mgr);
		return -EINVAL;
	}

	ctx_data = prepare_args->ctxt_to_hw_map;
	if (!ctx_data) {
		CAM_ERR(CAM_OPE, "Invalid Context");
		return -EINVAL;
	}

	mutex_lock(&ctx_data->ctx_mutex);
	if (ctx_data->ctx_state != OPE_CTX_STATE_ACQUIRED) {
		mutex_unlock(&ctx_data->ctx_mutex);
		CAM_ERR(CAM_OPE, "ctx id %u is not acquired state: %d",
			ctx_data->ctx_id, ctx_data->ctx_state);
		return -EINVAL;
	}

	packet = prepare_args->packet;
	rc = cam_packet_util_validate_packet(packet, prepare_args->remain_len);
	if (rc) {
		mutex_unlock(&ctx_data->ctx_mutex);
		CAM_ERR(CAM_OPE,
			"packet validation failed: %d req_id: %d ctx: %d",
			rc, packet->header.request_id, ctx_data->ctx_id);
		return rc;
	}

	rc = cam_ope_mgr_pkt_validation(packet);
	if (rc) {
		mutex_unlock(&ctx_data->ctx_mutex);
		CAM_ERR(CAM_OPE,
			"ope packet validation failed: %d req_id: %d ctx: %d",
			rc, packet->header.request_id, ctx_data->ctx_id);
		return -EINVAL;
	}

	rc = cam_packet_util_process_patches(packet, hw_mgr->iommu_cdm_hdl,
		hw_mgr->iommu_sec_cdm_hdl);
	if (rc) {
		mutex_unlock(&ctx_data->ctx_mutex);
		CAM_ERR(CAM_OPE, "Patching failed: %d req_id: %d ctx: %d",
			rc, packet->header.request_id, ctx_data->ctx_id);
		return -EINVAL;
	}

	request_idx  = find_first_zero_bit(ctx_data->bitmap, ctx_data->bits);
	if (request_idx >= CAM_CTX_REQ_MAX || request_idx < 0) {
		mutex_unlock(&ctx_data->ctx_mutex);
		CAM_ERR(CAM_OPE, "Invalid ctx req slot = %d", request_idx);
		return -EINVAL;
	}

	ctx_data->req_list[request_idx] =
		kzalloc(sizeof(struct cam_ope_request), GFP_KERNEL);
	if (!ctx_data->req_list[request_idx]) {
		CAM_ERR(CAM_OPE, "mem allocation failed ctx:%d req_idx:%d",
			ctx_data->ctx_id, request_idx);
		rc = -ENOMEM;
		goto req_mem_alloc_failed;
	}

	ope_req = ctx_data->req_list[request_idx];
	ope_req->cdm_cmd =
		kzalloc(((sizeof(struct cam_cdm_bl_request)) +
			((OPE_MAX_CDM_BLS - 1) *
			sizeof(struct cam_cdm_bl_cmd))),
			GFP_KERNEL);
	if (!ope_req->cdm_cmd) {
		CAM_ERR(CAM_OPE, "Cdm mem alloc failed ctx:%d req_idx:%d",
			ctx_data->ctx_id, request_idx);
		rc = -ENOMEM;
		goto req_cdm_mem_alloc_failed;
	}

	rc = cam_ope_mgr_process_cmd_desc(hw_mgr, packet,
		ctx_data, &ope_cmd_buf_addr, request_idx);
	if (rc) {
		CAM_ERR(CAM_OPE,
			"cmd desc processing failed :%d ctx: %d req_id:%d",
			rc, ctx_data->ctx_id, packet->header.request_id);
		goto end;
	}

	rc = cam_ope_mgr_process_io_cfg(hw_mgr, packet, prepare_args,
		ctx_data, request_idx);
	if (rc) {
		CAM_ERR(CAM_OPE,
			"IO cfg processing failed: %d ctx: %d req_id:%d",
			rc, ctx_data->ctx_id, packet->header.request_id);
		goto end;
	}

	rc = cam_ope_mgr_create_kmd_buf(hw_mgr, packet, prepare_args,
		ctx_data, request_idx, ope_cmd_buf_addr);
	if (rc) {
		CAM_ERR(CAM_OPE,
			"create kmd buf failed: %d ctx: %d request_id:%d",
			rc, ctx_data->ctx_id, packet->header.request_id);
		goto end;
	}

	rc = cam_ope_process_generic_cmd_buffer(packet, ctx_data,
		request_idx, NULL);
	if (rc) {
		CAM_ERR(CAM_OPE, "Failed: %d ctx: %d req_id: %d req_idx: %d",
			rc, ctx_data->ctx_id, packet->header.request_id,
			request_idx);
		goto end;
	}
	prepare_args->num_hw_update_entries = 1;
	prepare_args->hw_update_entries[0].addr =
		(uintptr_t)ctx_data->req_list[request_idx]->cdm_cmd;
	prepare_args->priv = ctx_data->req_list[request_idx];
	prepare_args->pf_data->packet = packet;
	ope_req->hang_data.packet = packet;
	ktime_get_boottime_ts64(&ts);
	ctx_data->last_req_time = (uint64_t)((ts.tv_sec * 1000000000) +
		ts.tv_nsec);
	CAM_DBG(CAM_REQ, "req_id= %llu ctx_id= %d lrt=%llu",
		packet->header.request_id, ctx_data->ctx_id,
		ctx_data->last_req_time);
	cam_ope_req_timer_modify(ctx_data, OPE_REQUEST_TIMEOUT);
	set_bit(request_idx, ctx_data->bitmap);
	mutex_unlock(&ctx_data->ctx_mutex);

	CAM_DBG(CAM_REQ, "Prepare Hw update Successful request_id: %d  ctx: %d",
		packet->header.request_id, ctx_data->ctx_id);
	return rc;

end:
	kzfree(ctx_data->req_list[request_idx]->cdm_cmd);
	ctx_data->req_list[request_idx]->cdm_cmd = NULL;
req_cdm_mem_alloc_failed:
	kzfree(ctx_data->req_list[request_idx]);
	ctx_data->req_list[request_idx] = NULL;
req_mem_alloc_failed:
	clear_bit(request_idx, ctx_data->bitmap);
	mutex_unlock(&ctx_data->ctx_mutex);
	return rc;
}

static int cam_ope_mgr_handle_config_err(
	struct cam_hw_config_args *config_args,
	struct cam_ope_ctx *ctx_data)
{
	struct cam_hw_done_event_data buf_data;
	struct cam_ope_request *ope_req;
	uint32_t req_idx;

	ope_req = config_args->priv;

	buf_data.request_id = ope_req->request_id;
	buf_data.evt_param = CAM_SYNC_OPE_EVENT_CONFIG_ERR;
	ctx_data->ctxt_event_cb(ctx_data->context_priv, CAM_CTX_EVT_ID_ERROR,
		&buf_data);

	req_idx = ope_req->req_idx;
	ope_req->request_id = 0;
	kzfree(ctx_data->req_list[req_idx]->cdm_cmd);
	ctx_data->req_list[req_idx]->cdm_cmd = NULL;
	cam_ope_free_io_config(ctx_data->req_list[req_idx]);
	kzfree(ctx_data->req_list[req_idx]);
	ctx_data->req_list[req_idx] = NULL;
	clear_bit(req_idx, ctx_data->bitmap);

	return 0;
}

static int cam_ope_mgr_enqueue_config(struct cam_ope_hw_mgr *hw_mgr,
	struct cam_ope_ctx *ctx_data,
	struct cam_hw_config_args *config_args)
{
	int rc = 0;
	uint64_t request_id = 0;
	struct crm_workq_task *task;
	struct ope_cmd_work_data *task_data;
	struct cam_hw_update_entry *hw_update_entries;
	struct cam_ope_request *ope_req = NULL;

	ope_req = config_args->priv;
	request_id = config_args->request_id;
	hw_update_entries = config_args->hw_update_entries;

	CAM_DBG(CAM_OPE, "req_id = %lld %pK", request_id, config_args->priv);

	task = cam_req_mgr_workq_get_task(ope_hw_mgr->cmd_work);
	if (!task) {
		CAM_ERR(CAM_OPE, "no empty task");
		return -ENOMEM;
	}

	task_data = (struct ope_cmd_work_data *)task->payload;
	task_data->data = (void *)hw_update_entries->addr;
	task_data->req_id = request_id;
	task_data->type = OPE_WORKQ_TASK_CMD_TYPE;
	task->process_cb = cam_ope_mgr_process_cmd;
	rc = cam_req_mgr_workq_enqueue_task(task, ctx_data,
		CRM_TASK_PRIORITY_0);

	return rc;
}

static int cam_ope_mgr_config_hw(void *hw_priv, void *hw_config_args)
{
	int rc = 0;
	struct cam_ope_hw_mgr *hw_mgr = hw_priv;
	struct cam_hw_config_args *config_args = hw_config_args;
	struct cam_ope_ctx *ctx_data = NULL;
	struct cam_ope_request *ope_req = NULL;
	struct cam_cdm_bl_request *cdm_cmd;

	CAM_DBG(CAM_OPE, "E");
	if (!hw_mgr || !config_args) {
		CAM_ERR(CAM_OPE, "Invalid arguments %pK %pK",
			hw_mgr, config_args);
		return -EINVAL;
	}

	if (!config_args->num_hw_update_entries) {
		CAM_ERR(CAM_OPE, "No hw update enteries are available");
		return -EINVAL;
	}

	ctx_data = config_args->ctxt_to_hw_map;
	mutex_lock(&hw_mgr->hw_mgr_mutex);
	mutex_lock(&ctx_data->ctx_mutex);
	if (ctx_data->ctx_state != OPE_CTX_STATE_ACQUIRED) {
		mutex_unlock(&ctx_data->ctx_mutex);
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		CAM_ERR(CAM_OPE, "ctx id :%u is not in use",
			ctx_data->ctx_id);
		return -EINVAL;
	}

	ope_req = config_args->priv;
	cdm_cmd = (struct cam_cdm_bl_request *)
		config_args->hw_update_entries->addr;
	cdm_cmd->cookie = ope_req->req_idx;

	cam_ope_mgr_ope_clk_update(hw_mgr, ctx_data, ope_req->req_idx);
	ctx_data->req_list[ope_req->req_idx]->submit_timestamp = ktime_get();

	if (ope_req->request_id <= ctx_data->last_flush_req)
		CAM_WARN(CAM_OPE,
			"Anomaly submitting flushed req %llu [last_flush %llu] in ctx %u",
			ope_req->request_id, ctx_data->last_flush_req,
			ctx_data->ctx_id);

	rc = cam_ope_mgr_enqueue_config(hw_mgr, ctx_data, config_args);
	if (rc)
		goto config_err;

	CAM_DBG(CAM_REQ, "req_id %llu, ctx_id %u io config",
		ope_req->request_id, ctx_data->ctx_id);

	mutex_unlock(&ctx_data->ctx_mutex);
	mutex_unlock(&hw_mgr->hw_mgr_mutex);

	return rc;
config_err:
	cam_ope_mgr_handle_config_err(config_args, ctx_data);
	mutex_unlock(&ctx_data->ctx_mutex);
	mutex_unlock(&hw_mgr->hw_mgr_mutex);
	return rc;
}

static void cam_ope_mgr_print_io_bufs(struct cam_packet *packet,
	int32_t iommu_hdl, int32_t sec_mmu_hdl, uint32_t pf_buf_info,
	bool *mem_found)
{
	dma_addr_t   iova_addr;
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
				CAM_INFO(CAM_OPE,
					"Found PF at port: %d mem %x fd: %x",
					io_cfg[i].resource_type,
					io_cfg[i].mem_handle[j],
					pf_buf_info);
				if (mem_found)
					*mem_found = true;
			}

			CAM_INFO(CAM_OPE, "port: %d f: %u format: %d dir %d",
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
				CAM_ERR(CAM_UTIL,
					"get src buf address fail rc %d mem %x",
					rc, io_cfg[i].mem_handle[j]);
				continue;
			}
			if ((iova_addr & 0xFFFFFFFF) != iova_addr) {
				CAM_ERR(CAM_OPE, "Invalid mapped address");
				rc = -EINVAL;
				continue;
			}

			CAM_INFO(CAM_OPE,
				"pln %d dir %d w %d h %d s %u sh %u sz %d addr 0x%x off 0x%x memh %x",
				j, io_cfg[i].direction,
				io_cfg[i].planes[j].width,
				io_cfg[i].planes[j].height,
				io_cfg[i].planes[j].plane_stride,
				io_cfg[i].planes[j].slice_height,
				(int32_t)src_buf_size,
				(unsigned int)iova_addr,
				io_cfg[i].offsets[j],
				io_cfg[i].mem_handle[j]);

			iova_addr += io_cfg[i].offsets[j];

		}
	}
	cam_packet_dump_patch_info(packet, ope_hw_mgr->iommu_hdl,
		ope_hw_mgr->iommu_sec_hdl);
}

static int cam_ope_mgr_cmd(void *hw_mgr_priv, void *cmd_args)
{
	int rc = 0;
	struct cam_hw_cmd_args *hw_cmd_args = cmd_args;
	struct cam_ope_hw_mgr  *hw_mgr = hw_mgr_priv;

	if (!hw_mgr_priv || !cmd_args) {
		CAM_ERR(CAM_OPE, "Invalid arguments");
		return -EINVAL;
	}

	switch (hw_cmd_args->cmd_type) {
	case CAM_HW_MGR_CMD_DUMP_PF_INFO:
		cam_ope_mgr_print_io_bufs(
			hw_cmd_args->u.pf_args.pf_data.packet,
			hw_mgr->iommu_hdl,
			hw_mgr->iommu_sec_hdl,
			hw_cmd_args->u.pf_args.buf_info,
			hw_cmd_args->u.pf_args.mem_found);

		break;
	default:
		CAM_ERR(CAM_OPE, "Invalid cmd");
	}

	return rc;
}

static int cam_ope_mgr_hw_open_u(void *hw_priv, void *fw_download_args)
{
	struct cam_ope_hw_mgr *hw_mgr;
	int rc = 0;

	if (!hw_priv) {
		CAM_ERR(CAM_OPE, "Invalid args: %pK", hw_priv);
		return -EINVAL;
	}

	hw_mgr = hw_priv;
	if (!hw_mgr->open_cnt) {
		hw_mgr->open_cnt++;
	} else {
		rc = -EBUSY;
		CAM_ERR(CAM_OPE, "Multiple opens are not supported");
	}

	return rc;
}

static cam_ope_mgr_hw_close_u(void *hw_priv, void *hw_close_args)
{
	struct cam_ope_hw_mgr *hw_mgr;
	int rc = 0;

	if (!hw_priv) {
		CAM_ERR(CAM_OPE, "Invalid args: %pK", hw_priv);
		return -EINVAL;
	}

	hw_mgr = hw_priv;
	if (!hw_mgr->open_cnt) {
		rc = -EINVAL;
		CAM_ERR(CAM_OPE, "device is already closed");
	} else {
		hw_mgr->open_cnt--;
	}

	return rc;
}

static int cam_ope_mgr_flush_req(struct cam_ope_ctx *ctx_data,
	struct cam_hw_flush_args *flush_args)
{
	int idx;
	int64_t request_id;

	request_id = *(int64_t *)flush_args->flush_req_pending[0];
	for (idx = 0; idx < CAM_CTX_REQ_MAX; idx++) {
		if (!ctx_data->req_list[idx])
			continue;

		if (ctx_data->req_list[idx]->request_id != request_id)
			continue;

		ctx_data->req_list[idx]->request_id = 0;
		kzfree(ctx_data->req_list[idx]->cdm_cmd);
		ctx_data->req_list[idx]->cdm_cmd = NULL;
		cam_ope_free_io_config(ctx_data->req_list[idx]);
		kzfree(ctx_data->req_list[idx]);
		ctx_data->req_list[idx] = NULL;
		clear_bit(idx, ctx_data->bitmap);
	}

	return 0;
}

static int cam_ope_mgr_flush_all(struct cam_ope_ctx *ctx_data,
	struct cam_hw_flush_args *flush_args)
{
	int i, rc;
	struct cam_ope_hw_mgr *hw_mgr = ope_hw_mgr;

	rc = cam_cdm_flush_hw(ctx_data->ope_cdm.cdm_handle);

	mutex_lock(&ctx_data->ctx_mutex);
	for (i = 0; i < hw_mgr->num_ope; i++) {
		rc = hw_mgr->ope_dev_intf[i]->hw_ops.process_cmd(
			hw_mgr->ope_dev_intf[i]->hw_priv, OPE_HW_RESET,
			NULL, 0);
		if (rc)
			CAM_ERR(CAM_OPE, "OPE Dev reset failed: %d", rc);
	}

	for (i = 0; i < CAM_CTX_REQ_MAX; i++) {
		if (!ctx_data->req_list[i])
			continue;

		ctx_data->req_list[i]->request_id = 0;
		kzfree(ctx_data->req_list[i]->cdm_cmd);
		ctx_data->req_list[i]->cdm_cmd = NULL;
		cam_ope_free_io_config(ctx_data->req_list[i]);
		kzfree(ctx_data->req_list[i]);
		ctx_data->req_list[i] = NULL;
		clear_bit(i, ctx_data->bitmap);
	}
	mutex_unlock(&ctx_data->ctx_mutex);

	return rc;
}

static int cam_ope_mgr_hw_dump(void *hw_priv, void *hw_dump_args)
{
	struct cam_ope_ctx *ctx_data;
	struct cam_ope_hw_mgr *hw_mgr = hw_priv;
	struct cam_hw_dump_args  *dump_args;
	int idx;
	ktime_t cur_time;
	struct timespec64 cur_ts, req_ts;
	uint64_t diff;

	if ((!hw_priv) || (!hw_dump_args)) {
		CAM_ERR(CAM_OPE, "Invalid params %pK %pK",
			hw_priv, hw_dump_args);
		return -EINVAL;
	}

	dump_args = (struct cam_hw_dump_args *)hw_dump_args;
	ctx_data = dump_args->ctxt_to_hw_map;

	if (!ctx_data) {
		CAM_ERR(CAM_OPE, "Invalid context");
		return -EINVAL;
	}

	mutex_lock(&hw_mgr->hw_mgr_mutex);
	mutex_lock(&ctx_data->ctx_mutex);

	CAM_INFO(CAM_OPE, "Req %lld", dump_args->request_id);
	for (idx = 0; idx < CAM_CTX_REQ_MAX; idx++) {
		if (!ctx_data->req_list[idx])
			continue;

		if (ctx_data->req_list[idx]->request_id ==
			dump_args->request_id)
			break;
	}

	/* no matching request found */
	if (idx == CAM_CTX_REQ_MAX) {
		mutex_unlock(&ctx_data->ctx_mutex);
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		return 0;
	}

	cur_time = ktime_get();
	diff = ktime_us_delta(cur_time,
			ctx_data->req_list[idx]->submit_timestamp);
	cur_ts = ktime_to_timespec64(cur_time);
	req_ts = ktime_to_timespec64(ctx_data->req_list[idx]->submit_timestamp);

	if (diff < (OPE_REQUEST_TIMEOUT * 1000)) {
		CAM_INFO(CAM_OPE, "No Error req %llu %ld:%06ld %ld:%06ld",
			dump_args->request_id,
			req_ts.tv_sec,
			req_ts.tv_nsec/NSEC_PER_USEC,
			cur_ts.tv_sec,
			cur_ts.tv_nsec/NSEC_PER_USEC);
		mutex_unlock(&ctx_data->ctx_mutex);
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		return 0;
	}

	CAM_ERR(CAM_OPE, "Error req %llu %ld:%06ld %ld:%06ld",
		dump_args->request_id,
		req_ts.tv_sec,
		req_ts.tv_nsec/NSEC_PER_USEC,
		cur_ts.tv_sec,
		cur_ts.tv_nsec/NSEC_PER_USEC);

	mutex_unlock(&ctx_data->ctx_mutex);
	mutex_unlock(&hw_mgr->hw_mgr_mutex);
	return 0;
}

static int cam_ope_mgr_hw_flush(void *hw_priv, void *hw_flush_args)
{
	struct cam_hw_flush_args *flush_args = hw_flush_args;
	struct cam_ope_ctx *ctx_data;
	struct cam_ope_hw_mgr *hw_mgr = ope_hw_mgr;

	if ((!hw_priv) || (!hw_flush_args)) {
		CAM_ERR(CAM_OPE, "Input params are Null");
		return -EINVAL;
	}

	ctx_data = flush_args->ctxt_to_hw_map;
	if (!ctx_data) {
		CAM_ERR(CAM_OPE, "Ctx data is NULL");
		return -EINVAL;
	}

	if ((flush_args->flush_type >= CAM_FLUSH_TYPE_MAX) ||
		(flush_args->flush_type < CAM_FLUSH_TYPE_REQ)) {
		CAM_ERR(CAM_OPE, "Invalid flush type: %d",
			flush_args->flush_type);
		return -EINVAL;
	}

	switch (flush_args->flush_type) {
	case CAM_FLUSH_TYPE_ALL:
		mutex_lock(&hw_mgr->hw_mgr_mutex);
		ctx_data->last_flush_req = flush_args->last_flush_req;

		CAM_DBG(CAM_REQ, "ctx_id %d Flush type %d last_flush_req %u",
				ctx_data->ctx_id, flush_args->flush_type,
				ctx_data->last_flush_req);

		cam_ope_mgr_flush_all(ctx_data, flush_args);
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		break;
	case CAM_FLUSH_TYPE_REQ:
		mutex_lock(&ctx_data->ctx_mutex);
		if (flush_args->num_req_active) {
			CAM_ERR(CAM_OPE, "Flush request is not supported");
			mutex_unlock(&ctx_data->ctx_mutex);
			return -EINVAL;
		}
		if (flush_args->num_req_pending)
			cam_ope_mgr_flush_req(ctx_data, flush_args);
		mutex_unlock(&ctx_data->ctx_mutex);
		break;
	default:
		CAM_ERR(CAM_OPE, "Invalid flush type: %d",
				flush_args->flush_type);
		return -EINVAL;
	}

	return 0;
}

static int cam_ope_mgr_alloc_devs(struct device_node *of_node)
{
	int rc;
	uint32_t num_dev;

	rc = of_property_read_u32(of_node, "num-ope", &num_dev);
	if (rc) {
		CAM_ERR(CAM_OPE, "getting num of ope failed: %d", rc);
		return -EINVAL;
	}

	ope_hw_mgr->devices[OPE_DEV_OPE] = kzalloc(
		sizeof(struct cam_hw_intf *) * num_dev, GFP_KERNEL);
	if (!ope_hw_mgr->devices[OPE_DEV_OPE])
		return -ENOMEM;

	return 0;
}

static int cam_ope_mgr_init_devs(struct device_node *of_node)
{
	int rc = 0;
	int count, i;
	const char *name = NULL;
	struct device_node *child_node = NULL;
	struct platform_device *child_pdev = NULL;
	struct cam_hw_intf *child_dev_intf = NULL;
	struct cam_hw_info *ope_dev;
	struct cam_hw_soc_info *soc_info = NULL;

	rc = cam_ope_mgr_alloc_devs(of_node);
	if (rc)
		return rc;

	count = of_property_count_strings(of_node, "compat-hw-name");
	if (!count) {
		CAM_ERR(CAM_OPE, "no compat hw found in dev tree, cnt = %d",
			count);
		rc = -EINVAL;
		goto compat_hw_name_failed;
	}

	for (i = 0; i < count; i++) {
		rc = of_property_read_string_index(of_node, "compat-hw-name",
			i, &name);
		if (rc) {
			CAM_ERR(CAM_OPE, "getting dev object name failed");
			goto compat_hw_name_failed;
		}

		child_node = of_find_node_by_name(NULL, name);
		if (!child_node) {
			CAM_ERR(CAM_OPE, "Cannot find node in dtsi %s", name);
			rc = -ENODEV;
			goto compat_hw_name_failed;
		}

		child_pdev = of_find_device_by_node(child_node);
		if (!child_pdev) {
			CAM_ERR(CAM_OPE, "failed to find device on bus %s",
				child_node->name);
			rc = -ENODEV;
			of_node_put(child_node);
			goto compat_hw_name_failed;
		}

		child_dev_intf = (struct cam_hw_intf *)platform_get_drvdata(
			child_pdev);
		if (!child_dev_intf) {
			CAM_ERR(CAM_OPE, "no child device");
			of_node_put(child_node);
			goto compat_hw_name_failed;
		}
		ope_hw_mgr->devices[child_dev_intf->hw_type]
			[child_dev_intf->hw_idx] = child_dev_intf;

		if (!child_dev_intf->hw_ops.process_cmd)
			goto compat_hw_name_failed;

		of_node_put(child_node);
	}

	ope_hw_mgr->num_ope = count;
	for (i = 0; i < count; i++) {
		ope_hw_mgr->ope_dev_intf[i] =
			ope_hw_mgr->devices[OPE_DEV_OPE][i];
			ope_dev = ope_hw_mgr->ope_dev_intf[i]->hw_priv;
			soc_info = &ope_dev->soc_info;
			ope_hw_mgr->cdm_reg_map[i][0] =
				soc_info->reg_map[0].mem_base;
	}

	ope_hw_mgr->hfi_en = of_property_read_bool(of_node, "hfi_en");

	return 0;
compat_hw_name_failed:
	kfree(ope_hw_mgr->devices[OPE_DEV_OPE]);
	ope_hw_mgr->devices[OPE_DEV_OPE] = NULL;
	return rc;
}

static void cam_req_mgr_process_ope_command_queue(struct work_struct *w)
{
	cam_req_mgr_process_workq(w);
}

static void cam_req_mgr_process_ope_msg_queue(struct work_struct *w)
{
	cam_req_mgr_process_workq(w);
}

static void cam_req_mgr_process_ope_timer_queue(struct work_struct *w)
{
	cam_req_mgr_process_workq(w);
}

static int cam_ope_mgr_create_wq(void)
{

	int rc;
	int i;

	rc = cam_req_mgr_workq_create("ope_command_queue", OPE_WORKQ_NUM_TASK,
		&ope_hw_mgr->cmd_work, CRM_WORKQ_USAGE_NON_IRQ,
		0, cam_req_mgr_process_ope_command_queue);
	if (rc) {
		CAM_ERR(CAM_OPE, "unable to create a command worker");
		goto cmd_work_failed;
	}

	rc = cam_req_mgr_workq_create("ope_message_queue", OPE_WORKQ_NUM_TASK,
		&ope_hw_mgr->msg_work, CRM_WORKQ_USAGE_IRQ, 0,
		cam_req_mgr_process_ope_msg_queue);
	if (rc) {
		CAM_ERR(CAM_OPE, "unable to create a message worker");
		goto msg_work_failed;
	}

	rc = cam_req_mgr_workq_create("ope_timer_queue", OPE_WORKQ_NUM_TASK,
		&ope_hw_mgr->timer_work, CRM_WORKQ_USAGE_IRQ, 0,
		cam_req_mgr_process_ope_timer_queue);
	if (rc) {
		CAM_ERR(CAM_OPE, "unable to create a timer worker");
		goto timer_work_failed;
	}

	ope_hw_mgr->cmd_work_data =
		kzalloc(sizeof(struct ope_cmd_work_data) * OPE_WORKQ_NUM_TASK,
		GFP_KERNEL);
	if (!ope_hw_mgr->cmd_work_data) {
		rc = -ENOMEM;
		goto cmd_work_data_failed;
	}

	ope_hw_mgr->msg_work_data =
		kzalloc(sizeof(struct ope_msg_work_data) * OPE_WORKQ_NUM_TASK,
		GFP_KERNEL);
	if (!ope_hw_mgr->msg_work_data) {
		rc = -ENOMEM;
		goto msg_work_data_failed;
	}

	ope_hw_mgr->timer_work_data =
		kzalloc(sizeof(struct ope_clk_work_data) * OPE_WORKQ_NUM_TASK,
		GFP_KERNEL);
	if (!ope_hw_mgr->timer_work_data) {
		rc = -ENOMEM;
		goto timer_work_data_failed;
	}

	for (i = 0; i < OPE_WORKQ_NUM_TASK; i++)
		ope_hw_mgr->msg_work->task.pool[i].payload =
				&ope_hw_mgr->msg_work_data[i];

	for (i = 0; i < OPE_WORKQ_NUM_TASK; i++)
		ope_hw_mgr->cmd_work->task.pool[i].payload =
				&ope_hw_mgr->cmd_work_data[i];

	for (i = 0; i < OPE_WORKQ_NUM_TASK; i++)
		ope_hw_mgr->timer_work->task.pool[i].payload =
				&ope_hw_mgr->timer_work_data[i];
	return 0;


timer_work_data_failed:
	kfree(ope_hw_mgr->msg_work_data);
msg_work_data_failed:
	kfree(ope_hw_mgr->cmd_work_data);
cmd_work_data_failed:
	cam_req_mgr_workq_destroy(&ope_hw_mgr->timer_work);
timer_work_failed:
	cam_req_mgr_workq_destroy(&ope_hw_mgr->msg_work);
msg_work_failed:
	cam_req_mgr_workq_destroy(&ope_hw_mgr->cmd_work);
cmd_work_failed:
	return rc;
}

static int cam_ope_create_debug_fs(void)
{
	ope_hw_mgr->dentry = debugfs_create_dir("camera_ope",
		NULL);

	if (!ope_hw_mgr->dentry) {
		CAM_ERR(CAM_OPE, "failed to create dentry");
		return -ENOMEM;
	}

	if (!debugfs_create_bool("frame_dump_enable",
		0644,
		ope_hw_mgr->dentry,
		&ope_hw_mgr->frame_dump_enable)) {
		CAM_ERR(CAM_OPE,
			"failed to create dump_enable_debug");
		goto err;
	}

	if (!debugfs_create_bool("dump_req_data_enable",
		0644,
		ope_hw_mgr->dentry,
		&ope_hw_mgr->dump_req_data_enable)) {
		CAM_ERR(CAM_OPE,
			"failed to create dump_enable_debug");
		goto err;
	}

	return 0;
err:
	debugfs_remove_recursive(ope_hw_mgr->dentry);
	return -ENOMEM;
}


int cam_ope_hw_mgr_init(struct device_node *of_node, uint64_t *hw_mgr_hdl,
	int *iommu_hdl)
{
	int i, rc = 0, j;
	struct cam_hw_mgr_intf *hw_mgr_intf;
	struct cam_iommu_handle cdm_handles;

	if (!of_node || !hw_mgr_hdl) {
		CAM_ERR(CAM_OPE, "Invalid args of_node %pK hw_mgr %pK",
			of_node, hw_mgr_hdl);
		return -EINVAL;
	}
	hw_mgr_intf = (struct cam_hw_mgr_intf *)hw_mgr_hdl;

	ope_hw_mgr = kzalloc(sizeof(struct cam_ope_hw_mgr), GFP_KERNEL);
	if (!ope_hw_mgr) {
		CAM_ERR(CAM_OPE, "Unable to allocate mem for: size = %d",
			sizeof(struct cam_ope_hw_mgr));
		return -ENOMEM;
	}

	hw_mgr_intf->hw_mgr_priv = ope_hw_mgr;
	hw_mgr_intf->hw_get_caps = cam_ope_mgr_get_hw_caps;
	hw_mgr_intf->hw_acquire = cam_ope_mgr_acquire_hw;
	hw_mgr_intf->hw_release = cam_ope_mgr_release_hw;
	hw_mgr_intf->hw_start   = NULL;
	hw_mgr_intf->hw_stop    = NULL;
	hw_mgr_intf->hw_prepare_update = cam_ope_mgr_prepare_hw_update;
	hw_mgr_intf->hw_config_stream_settings = NULL;
	hw_mgr_intf->hw_config = cam_ope_mgr_config_hw;
	hw_mgr_intf->hw_read   = NULL;
	hw_mgr_intf->hw_write  = NULL;
	hw_mgr_intf->hw_cmd = cam_ope_mgr_cmd;
	hw_mgr_intf->hw_open = cam_ope_mgr_hw_open_u;
	hw_mgr_intf->hw_close = cam_ope_mgr_hw_close_u;
	hw_mgr_intf->hw_flush = cam_ope_mgr_hw_flush;
	hw_mgr_intf->hw_dump = cam_ope_mgr_hw_dump;

	ope_hw_mgr->secure_mode = false;
	mutex_init(&ope_hw_mgr->hw_mgr_mutex);
	spin_lock_init(&ope_hw_mgr->hw_mgr_lock);

	for (i = 0; i < OPE_CTX_MAX; i++) {
		ope_hw_mgr->ctx[i].bitmap_size =
			BITS_TO_LONGS(CAM_CTX_REQ_MAX) *
			sizeof(long);
		ope_hw_mgr->ctx[i].bitmap = kzalloc(
			ope_hw_mgr->ctx[i].bitmap_size, GFP_KERNEL);
		if (!ope_hw_mgr->ctx[i].bitmap) {
			CAM_ERR(CAM_OPE, "bitmap allocation failed: size = %d",
				ope_hw_mgr->ctx[i].bitmap_size);
			rc = -ENOMEM;
			goto ope_ctx_bitmap_failed;
		}
		ope_hw_mgr->ctx[i].bits = ope_hw_mgr->ctx[i].bitmap_size *
			BITS_PER_BYTE;
		mutex_init(&ope_hw_mgr->ctx[i].ctx_mutex);
	}

	rc = cam_ope_mgr_init_devs(of_node);
	if (rc)
		goto dev_init_failed;

	ope_hw_mgr->ctx_bitmap_size =
		BITS_TO_LONGS(OPE_CTX_MAX) * sizeof(long);
	ope_hw_mgr->ctx_bitmap = kzalloc(ope_hw_mgr->ctx_bitmap_size,
		GFP_KERNEL);
	if (!ope_hw_mgr->ctx_bitmap) {
		rc = -ENOMEM;
		goto ctx_bitmap_alloc_failed;
	}

	ope_hw_mgr->ctx_bits = ope_hw_mgr->ctx_bitmap_size *
		BITS_PER_BYTE;

	rc = cam_smmu_get_handle("ope", &ope_hw_mgr->iommu_hdl);
	if (rc) {
		CAM_ERR(CAM_OPE, "get mmu handle failed: %d", rc);
		goto ope_get_hdl_failed;
	}

	rc = cam_smmu_get_handle("cam-secure", &ope_hw_mgr->iommu_sec_hdl);
	if (rc) {
		CAM_ERR(CAM_OPE, "get secure mmu handle failed: %d", rc);
		goto secure_hdl_failed;
	}

	rc = cam_cdm_get_iommu_handle("ope", &cdm_handles);
	if (rc) {
		CAM_ERR(CAM_OPE, "ope cdm handle get is failed: %d", rc);
		goto ope_cdm_hdl_failed;
	}

	ope_hw_mgr->iommu_cdm_hdl = cdm_handles.non_secure;
	ope_hw_mgr->iommu_sec_cdm_hdl = cdm_handles.secure;
	CAM_DBG(CAM_OPE, "iommu hdls %x %x cdm %x %x",
		ope_hw_mgr->iommu_hdl, ope_hw_mgr->iommu_sec_hdl,
		ope_hw_mgr->iommu_cdm_hdl,
		ope_hw_mgr->iommu_sec_cdm_hdl);

	rc = cam_ope_mgr_create_wq();
	if (rc)
		goto ope_wq_create_failed;

	cam_ope_create_debug_fs();

	if (iommu_hdl)
		*iommu_hdl = ope_hw_mgr->iommu_hdl;

	return rc;

ope_wq_create_failed:
	ope_hw_mgr->iommu_cdm_hdl = -1;
	ope_hw_mgr->iommu_sec_cdm_hdl = -1;
ope_cdm_hdl_failed:
	cam_smmu_destroy_handle(ope_hw_mgr->iommu_sec_hdl);
	ope_hw_mgr->iommu_sec_hdl = -1;
secure_hdl_failed:
	cam_smmu_destroy_handle(ope_hw_mgr->iommu_hdl);
	ope_hw_mgr->iommu_hdl = -1;
ope_get_hdl_failed:
	kzfree(ope_hw_mgr->ctx_bitmap);
	ope_hw_mgr->ctx_bitmap = NULL;
	ope_hw_mgr->ctx_bitmap_size = 0;
	ope_hw_mgr->ctx_bits = 0;
ctx_bitmap_alloc_failed:
	kzfree(ope_hw_mgr->devices[OPE_DEV_OPE]);
	ope_hw_mgr->devices[OPE_DEV_OPE] = NULL;
dev_init_failed:
ope_ctx_bitmap_failed:
	mutex_destroy(&ope_hw_mgr->hw_mgr_mutex);
	for (j = i - 1; j >= 0; j--) {
		mutex_destroy(&ope_hw_mgr->ctx[j].ctx_mutex);
		kzfree(ope_hw_mgr->ctx[j].bitmap);
		ope_hw_mgr->ctx[j].bitmap = NULL;
		ope_hw_mgr->ctx[j].bitmap_size = 0;
		ope_hw_mgr->ctx[j].bits = 0;
	}
	kzfree(ope_hw_mgr);
	ope_hw_mgr = NULL;

	return rc;
}

