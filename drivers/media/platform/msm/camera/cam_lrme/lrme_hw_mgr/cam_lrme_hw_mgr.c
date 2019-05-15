/* Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <media/cam_cpas.h>
#include <media/cam_req_mgr.h>

#include "cam_io_util.h"
#include "cam_soc_util.h"
#include "cam_mem_mgr_api.h"
#include "cam_smmu_api.h"
#include "cam_packet_util.h"
#include "cam_lrme_context.h"
#include "cam_lrme_hw_intf.h"
#include "cam_lrme_hw_core.h"
#include "cam_lrme_hw_soc.h"
#include "cam_lrme_hw_mgr_intf.h"
#include "cam_lrme_hw_mgr.h"

static struct cam_lrme_hw_mgr g_lrme_hw_mgr;

static int cam_lrme_mgr_util_reserve_device(struct cam_lrme_hw_mgr *hw_mgr,
	struct cam_lrme_acquire_args *lrme_acquire_args)
{
	int i, index = 0;
	uint32_t min_ctx = UINT_MAX;
	struct cam_lrme_device *hw_device = NULL;

	mutex_lock(&hw_mgr->hw_mgr_mutex);
	if (!hw_mgr->device_count) {
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		CAM_ERR(CAM_LRME, "No device is registered");
		return -EINVAL;
	}

	for (i = 0; i < hw_mgr->device_count && i < CAM_LRME_HW_MAX; i++) {
		hw_device = &hw_mgr->hw_device[i];
		if (!hw_device->num_context) {
			index = i;
			break;
		}
		if (hw_device->num_context < min_ctx) {
			min_ctx = hw_device->num_context;
			index = i;
		}
	}

	hw_device = &hw_mgr->hw_device[index];
	hw_device->num_context++;

	mutex_unlock(&hw_mgr->hw_mgr_mutex);

	CAM_DBG(CAM_LRME, "reserve device index %d", index);

	return index;
}

static int cam_lrme_mgr_util_get_device(struct cam_lrme_hw_mgr *hw_mgr,
	uint32_t device_index, struct cam_lrme_device **hw_device)
{
	if (!hw_mgr) {
		CAM_ERR(CAM_LRME, "invalid params hw_mgr %pK", hw_mgr);
		return -EINVAL;
	}

	if (device_index >= CAM_LRME_HW_MAX) {
		CAM_ERR(CAM_LRME, "Wrong device index %d", device_index);
		return -EINVAL;
	}

	*hw_device = &hw_mgr->hw_device[device_index];

	return 0;
}

static int cam_lrme_mgr_util_packet_validate(struct cam_packet *packet,
	size_t remain_len)
{
	struct cam_cmd_buf_desc *cmd_desc = NULL;
	int i, rc;

	if (!packet) {
		CAM_ERR(CAM_LRME, "Invalid args");
		return -EINVAL;
	}

	CAM_DBG(CAM_LRME, "Packet request=%d, op_code=0x%x, size=%d, flags=%d",
		packet->header.request_id, packet->header.op_code,
		packet->header.size, packet->header.flags);
	CAM_DBG(CAM_LRME,
		"Packet cmdbuf(offset=%d, num=%d) io(offset=%d, num=%d)",
		packet->cmd_buf_offset, packet->num_cmd_buf,
		packet->io_configs_offset, packet->num_io_configs);
	CAM_DBG(CAM_LRME,
		"Packet Patch(offset=%d, num=%d) kmd(offset=%d, num=%d)",
		packet->patch_offset, packet->num_patches,
		packet->kmd_cmd_buf_offset, packet->kmd_cmd_buf_index);

	if (cam_packet_util_validate_packet(packet, remain_len)) {
		CAM_ERR(CAM_LRME, "invalid packet:%d %d %d %d %d",
			packet->kmd_cmd_buf_index,
			packet->num_cmd_buf, packet->cmd_buf_offset,
			packet->io_configs_offset, packet->header.size);
		return -EINVAL;
	}

	if (!packet->num_io_configs) {
		CAM_ERR(CAM_LRME, "no io configs");
		return -EINVAL;
	}

	cmd_desc = (struct cam_cmd_buf_desc *)((uint8_t *)&packet->payload +
		packet->cmd_buf_offset);

	for (i = 0; i < packet->num_cmd_buf; i++) {
		if (!cmd_desc[i].length)
			continue;

		CAM_DBG(CAM_LRME,
			"CmdBuf[%d] hdl=%d, offset=%d, size=%d, len=%d, type=%d, meta_data=%d",
			i,
			cmd_desc[i].mem_handle, cmd_desc[i].offset,
			cmd_desc[i].size, cmd_desc[i].length, cmd_desc[i].type,
			cmd_desc[i].meta_data);

		rc = cam_packet_util_validate_cmd_desc(&cmd_desc[i]);
		if (rc) {
			CAM_ERR(CAM_LRME, "Invalid cmd buffer %d", i);
			return rc;
		}
	}

	return 0;
}

static int cam_lrme_mgr_util_prepare_io_buffer(int32_t iommu_hdl,
	struct cam_hw_prepare_update_args *prepare,
	struct cam_lrme_hw_io_buffer *input_buf,
	struct cam_lrme_hw_io_buffer *output_buf, uint32_t io_buf_size)
{
	int rc = -EINVAL;
	uint32_t num_in_buf, num_out_buf, i, j, plane;
	struct cam_buf_io_cfg *io_cfg;
	uint64_t io_addr[CAM_PACKET_MAX_PLANES];
	size_t size;

	num_in_buf = 0;
	num_out_buf = 0;
	io_cfg = (struct cam_buf_io_cfg *)((uint8_t *)
		 &prepare->packet->payload +
		 prepare->packet->io_configs_offset);

	for (i = 0; i < prepare->packet->num_io_configs; i++) {
		CAM_DBG(CAM_LRME,
			"IOConfig[%d] : handle[%d] Dir[%d] Res[%d] Fence[%d], Format[%d]",
			i, io_cfg[i].mem_handle[0], io_cfg[i].direction,
			io_cfg[i].resource_type,
			io_cfg[i].fence, io_cfg[i].format);

		memset(io_addr, 0, sizeof(io_addr));
		for (plane = 0; plane < CAM_PACKET_MAX_PLANES; plane++) {
			if (!io_cfg[i].mem_handle[plane])
				break;

			rc = cam_mem_get_io_buf(io_cfg[i].mem_handle[plane],
				iommu_hdl, &io_addr[plane], &size);
			if (rc) {
				CAM_ERR(CAM_LRME, "Cannot get io buf for %d %d",
					plane, rc);
				return -ENOMEM;
			}

			if ((size_t)io_cfg[i].offsets[plane] >= size) {
				CAM_ERR(CAM_LRME, "Invalid plane offset: %zu",
					(size_t)io_cfg[i].offsets[plane]);
				return -EINVAL;
			}

			io_addr[plane] += io_cfg[i].offsets[plane];

			CAM_DBG(CAM_LRME, "IO Address[%d][%d] : %llu",
				io_cfg[i].direction, plane, io_addr[plane]);
		}

		switch (io_cfg[i].direction) {
		case CAM_BUF_INPUT: {
			if (num_in_buf >= io_buf_size) {
				CAM_ERR(CAM_LRME,
					"Invalid number of buffers %d %d %d",
					num_in_buf, num_out_buf, io_buf_size);
				return -EINVAL;
			}
			prepare->in_map_entries[num_in_buf].resource_handle =
				io_cfg[i].resource_type;
			prepare->in_map_entries[num_in_buf].sync_id =
				io_cfg[i].fence;

			input_buf[num_in_buf].valid = true;
			for (j = 0; j < plane; j++)
				input_buf[num_in_buf].io_addr[j] = io_addr[j];
			input_buf[num_in_buf].num_plane = plane;
			input_buf[num_in_buf].io_cfg = &io_cfg[i];

			num_in_buf++;
			break;
		}
		case CAM_BUF_OUTPUT: {
			if (num_out_buf >= io_buf_size) {
				CAM_ERR(CAM_LRME,
					"Invalid number of buffers %d %d %d",
					num_in_buf, num_out_buf, io_buf_size);
				return -EINVAL;
			}
			prepare->out_map_entries[num_out_buf].resource_handle =
				io_cfg[i].resource_type;
			prepare->out_map_entries[num_out_buf].sync_id =
				io_cfg[i].fence;

			output_buf[num_out_buf].valid = true;
			for (j = 0; j < plane; j++)
				output_buf[num_out_buf].io_addr[j] = io_addr[j];
			output_buf[num_out_buf].num_plane = plane;
			output_buf[num_out_buf].io_cfg = &io_cfg[i];

			num_out_buf++;
			break;
		}
		default:
			CAM_ERR(CAM_LRME, "Unsupported io direction %d",
				io_cfg[i].direction);
			return -EINVAL;
		}
	}
	prepare->num_in_map_entries = num_in_buf;
	prepare->num_out_map_entries = num_out_buf;

	return 0;
}

static int cam_lrme_mgr_util_prepare_hw_update_entries(
	struct cam_lrme_hw_mgr *hw_mgr,
	struct cam_hw_prepare_update_args *prepare,
	struct cam_lrme_hw_cmd_config_args *config_args,
	struct cam_kmd_buf_info *kmd_buf_info)
{
	int i, rc = 0;
	struct cam_lrme_device *hw_device = NULL;
	uint32_t *kmd_buf_addr;
	uint32_t num_entry;
	uint32_t kmd_buf_max_size;
	uint32_t kmd_buf_used_bytes = 0;
	struct cam_hw_update_entry *hw_entry;
	struct cam_cmd_buf_desc *cmd_desc = NULL;

	hw_device = config_args->hw_device;
	if (!hw_device) {
		CAM_ERR(CAM_LRME, "Invalid hw_device");
		return -EINVAL;
	}

	kmd_buf_addr = (uint32_t *)((uint8_t *)kmd_buf_info->cpu_addr +
		kmd_buf_info->used_bytes);
	kmd_buf_max_size = kmd_buf_info->size - kmd_buf_info->used_bytes;

	config_args->cmd_buf_addr = kmd_buf_addr;
	config_args->size = kmd_buf_max_size;
	config_args->config_buf_size = 0;

	if (hw_device->hw_intf.hw_ops.process_cmd) {
		rc = hw_device->hw_intf.hw_ops.process_cmd(
			hw_device->hw_intf.hw_priv,
			CAM_LRME_HW_CMD_PREPARE_HW_UPDATE,
			config_args,
			sizeof(struct cam_lrme_hw_cmd_config_args));
		if (rc) {
			CAM_ERR(CAM_LRME,
				"Failed in CMD_PREPARE_HW_UPDATE %d", rc);
			return rc;
		}
	} else {
		CAM_ERR(CAM_LRME, "Can't find handle function");
		return -EINVAL;
	}

	kmd_buf_used_bytes += config_args->config_buf_size;

	if (!kmd_buf_used_bytes || (kmd_buf_used_bytes > kmd_buf_max_size)) {
		CAM_ERR(CAM_LRME, "Invalid kmd used bytes %d (%d)",
			kmd_buf_used_bytes, kmd_buf_max_size);
		return -ENOMEM;
	}

	hw_entry = prepare->hw_update_entries;
	num_entry = 0;

	if (config_args->config_buf_size) {
		if ((num_entry + 1) >= prepare->max_hw_update_entries) {
			CAM_ERR(CAM_LRME, "Insufficient  HW entries :%d %d",
				num_entry, prepare->max_hw_update_entries);
			return -EINVAL;
		}

		hw_entry[num_entry].handle = kmd_buf_info->handle;
		hw_entry[num_entry].len = config_args->config_buf_size;
		hw_entry[num_entry].offset = kmd_buf_info->offset;

		kmd_buf_info->used_bytes += config_args->config_buf_size;
		kmd_buf_info->offset += config_args->config_buf_size;
		num_entry++;
	}

	cmd_desc = (struct cam_cmd_buf_desc *)((uint8_t *)
		&prepare->packet->payload + prepare->packet->cmd_buf_offset);

	for (i = 0; i < prepare->packet->num_cmd_buf; i++) {
		if (!cmd_desc[i].length)
			continue;

		if ((num_entry + 1) >= prepare->max_hw_update_entries) {
			CAM_ERR(CAM_LRME, "Exceed max num of entry");
			return -EINVAL;
		}

		hw_entry[num_entry].handle = cmd_desc[i].mem_handle;
		hw_entry[num_entry].len = cmd_desc[i].length;
		hw_entry[num_entry].offset = cmd_desc[i].offset;
		num_entry++;
	}
	prepare->num_hw_update_entries = num_entry;

	CAM_DBG(CAM_LRME, "FinalConfig : hw_entries=%d, Sync(in=%d, out=%d)",
		prepare->num_hw_update_entries, prepare->num_in_map_entries,
		prepare->num_out_map_entries);

	return rc;
}

static void cam_lrme_mgr_util_put_frame_req(
	struct list_head *src_list,
	struct list_head *list,
	spinlock_t *lock)
{
	spin_lock(lock);
	list_add_tail(list, src_list);
	spin_unlock(lock);
}

static int cam_lrme_mgr_util_get_frame_req(
	struct list_head *src_list,
	struct cam_lrme_frame_request **frame_req,
	spinlock_t *lock)
{
	int rc = 0;
	struct cam_lrme_frame_request *req_ptr = NULL;

	spin_lock(lock);
	if (!list_empty(src_list)) {
		req_ptr = list_first_entry(src_list,
			struct cam_lrme_frame_request, frame_list);
		list_del_init(&req_ptr->frame_list);
	} else {
		rc = -ENOENT;
	}
	*frame_req = req_ptr;
	spin_unlock(lock);

	return rc;
}


static int cam_lrme_mgr_util_submit_req(void *priv, void *data)
{
	struct cam_lrme_device *hw_device;
	struct cam_lrme_hw_mgr *hw_mgr;
	struct cam_lrme_frame_request *frame_req = NULL;
	struct cam_lrme_hw_submit_args submit_args;
	struct cam_lrme_mgr_work_data *work_data;
	int rc;
	int req_prio = 0;

	if (!priv) {
		CAM_ERR(CAM_LRME, "worker doesn't have private data");
		return -EINVAL;
	}

	hw_mgr = (struct cam_lrme_hw_mgr *)priv;
	work_data = (struct cam_lrme_mgr_work_data *)data;
	hw_device = work_data->hw_device;

	rc = cam_lrme_mgr_util_get_frame_req(
		&hw_device->frame_pending_list_high, &frame_req,
		&hw_device->high_req_lock);

	if (!frame_req) {
		rc = cam_lrme_mgr_util_get_frame_req(
			&hw_device->frame_pending_list_normal, &frame_req,
			&hw_device->normal_req_lock);
		if (frame_req)
			req_prio = 1;
	}

	if (!frame_req) {
		CAM_DBG(CAM_LRME, "No pending request");
		return 0;
	}

	if (hw_device->hw_intf.hw_ops.process_cmd) {
		submit_args.hw_update_entries = frame_req->hw_update_entries;
		submit_args.num_hw_update_entries =
			frame_req->num_hw_update_entries;
		submit_args.frame_req = frame_req;

		rc = hw_device->hw_intf.hw_ops.process_cmd(
			hw_device->hw_intf.hw_priv,
			CAM_LRME_HW_CMD_SUBMIT,
			&submit_args, sizeof(struct cam_lrme_hw_submit_args));

		if (rc == -EBUSY)
			CAM_DBG(CAM_LRME, "device busy");
		else if (rc)
			CAM_ERR(CAM_LRME, "submit request failed rc %d", rc);
		if (rc) {
			req_prio == 0 ? spin_lock(&hw_device->high_req_lock) :
				spin_lock(&hw_device->normal_req_lock);
			list_add(&frame_req->frame_list,
				(req_prio == 0 ?
				 &hw_device->frame_pending_list_high :
				 &hw_device->frame_pending_list_normal));
			req_prio == 0 ? spin_unlock(&hw_device->high_req_lock) :
				spin_unlock(&hw_device->normal_req_lock);
		}
		if (rc == -EBUSY)
			rc = 0;
	} else {
		req_prio == 0 ? spin_lock(&hw_device->high_req_lock) :
			spin_lock(&hw_device->normal_req_lock);
		list_add(&frame_req->frame_list,
			(req_prio == 0 ?
			 &hw_device->frame_pending_list_high :
			 &hw_device->frame_pending_list_normal));
		req_prio == 0 ? spin_unlock(&hw_device->high_req_lock) :
			spin_unlock(&hw_device->normal_req_lock);
		rc = -EINVAL;
	}

	CAM_DBG(CAM_LRME, "End of submit, rc %d", rc);

	return rc;
}

static int cam_lrme_mgr_util_schedule_frame_req(
	struct cam_lrme_hw_mgr *hw_mgr, struct cam_lrme_device *hw_device)
{
	int rc = 0;
	struct crm_workq_task *task;
	struct cam_lrme_mgr_work_data *work_data;

	task = cam_req_mgr_workq_get_task(hw_device->work);
	if (!task) {
		CAM_ERR(CAM_LRME, "Can not get task for worker");
		return -ENOMEM;
	}

	work_data = (struct cam_lrme_mgr_work_data *)task->payload;
	work_data->hw_device = hw_device;

	task->process_cb = cam_lrme_mgr_util_submit_req;
	CAM_DBG(CAM_LRME, "enqueue submit task");
	rc = cam_req_mgr_workq_enqueue_task(task, hw_mgr, CRM_TASK_PRIORITY_0);

	return rc;
}

static int cam_lrme_mgr_util_release(struct cam_lrme_hw_mgr *hw_mgr,
	uint32_t device_index)
{
	int rc = 0;
	struct cam_lrme_device *hw_device;

	rc = cam_lrme_mgr_util_get_device(hw_mgr, device_index, &hw_device);
	if (rc) {
		CAM_ERR(CAM_LRME, "Error in getting device %d", rc);
		return rc;
	}

	mutex_lock(&hw_mgr->hw_mgr_mutex);
	hw_device->num_context--;
	mutex_unlock(&hw_mgr->hw_mgr_mutex);

	return rc;
}

static int cam_lrme_mgr_cb(void *data,
	struct cam_lrme_hw_cb_args *cb_args)
{
	struct cam_lrme_hw_mgr *hw_mgr = &g_lrme_hw_mgr;
	int rc = 0;
	bool frame_abort = true;
	struct cam_lrme_frame_request *frame_req;
	struct cam_lrme_device *hw_device;

	if (!data || !cb_args) {
		CAM_ERR(CAM_LRME, "Invalid input args");
		return -EINVAL;
	}

	hw_device = (struct cam_lrme_device *)data;
	frame_req = cb_args->frame_req;

	if (cb_args->cb_type & CAM_LRME_CB_PUT_FRAME) {
		memset(frame_req, 0x0, sizeof(*frame_req));
		INIT_LIST_HEAD(&frame_req->frame_list);
		cam_lrme_mgr_util_put_frame_req(&hw_mgr->frame_free_list,
				&frame_req->frame_list,
				&hw_mgr->free_req_lock);
		cb_args->cb_type &= ~CAM_LRME_CB_PUT_FRAME;
		frame_req = NULL;
	}

	if (cb_args->cb_type & CAM_LRME_CB_COMP_REG_UPDATE) {
		cb_args->cb_type &= ~CAM_LRME_CB_COMP_REG_UPDATE;
		CAM_DBG(CAM_LRME, "Reg update");
	}

	if (!frame_req)
		return rc;

	if (cb_args->cb_type & CAM_LRME_CB_BUF_DONE) {
		cb_args->cb_type &= ~CAM_LRME_CB_BUF_DONE;
		frame_abort = false;
	} else if (cb_args->cb_type & CAM_LRME_CB_ERROR) {
		cb_args->cb_type &= ~CAM_LRME_CB_ERROR;
		frame_abort = true;
	} else {
		CAM_ERR(CAM_LRME, "Wrong cb type %d, req %lld",
			cb_args->cb_type, frame_req->req_id);
		return -EINVAL;
	}

	if (hw_mgr->event_cb) {
		struct cam_hw_done_event_data buf_data;

		buf_data.request_id = frame_req->req_id;
		CAM_DBG(CAM_LRME, "frame req %llu, frame_abort %d",
			frame_req->req_id, frame_abort);
		rc = hw_mgr->event_cb(frame_req->ctxt_to_hw_map,
			frame_abort, &buf_data);
	} else {
		CAM_ERR(CAM_LRME, "No cb function");
	}
	memset(frame_req, 0x0, sizeof(*frame_req));
	INIT_LIST_HEAD(&frame_req->frame_list);
	cam_lrme_mgr_util_put_frame_req(&hw_mgr->frame_free_list,
				&frame_req->frame_list,
				&hw_mgr->free_req_lock);

	rc = cam_lrme_mgr_util_schedule_frame_req(hw_mgr, hw_device);

	return rc;
}

static int cam_lrme_mgr_get_caps(void *hw_mgr_priv, void *hw_get_caps_args)
{
	int rc = 0;
	struct cam_lrme_hw_mgr *hw_mgr = hw_mgr_priv;
	struct cam_query_cap_cmd *args = hw_get_caps_args;

	if (sizeof(struct cam_lrme_query_cap_cmd) != args->size) {
		CAM_ERR(CAM_LRME,
			"sizeof(struct cam_query_cap_cmd) = %zu, args->size = %d",
			sizeof(struct cam_query_cap_cmd), args->size);
		return -EFAULT;
	}

	if (copy_to_user(u64_to_user_ptr(args->caps_handle),
		&(hw_mgr->lrme_caps),
		sizeof(struct cam_lrme_query_cap_cmd))) {
		CAM_ERR(CAM_LRME, "copy to user failed");
		return -EFAULT;
	}

	return rc;
}

static int cam_lrme_mgr_hw_acquire(void *hw_mgr_priv, void *hw_acquire_args)
{
	struct cam_lrme_hw_mgr *hw_mgr = hw_mgr_priv;
	struct cam_hw_acquire_args *args =
		(struct cam_hw_acquire_args *)hw_acquire_args;
	struct cam_lrme_acquire_args lrme_acquire_args;
	uintptr_t device_index;

	if (!hw_mgr_priv || !args) {
		CAM_ERR(CAM_LRME,
		"Invalid input params hw_mgr_priv %pK, acquire_args %pK",
		hw_mgr_priv, args);
		return -EINVAL;
	}

	if (copy_from_user(&lrme_acquire_args,
		(void __user *)args->acquire_info,
		sizeof(struct cam_lrme_acquire_args))) {
		CAM_ERR(CAM_LRME, "Failed to copy acquire args from user");
		return -EFAULT;
	}

	device_index = cam_lrme_mgr_util_reserve_device(hw_mgr,
		&lrme_acquire_args);
	CAM_DBG(CAM_LRME, "Get device id %llu", device_index);

	if (device_index >= hw_mgr->device_count) {
		CAM_ERR(CAM_LRME, "Get wrong device id %lu", device_index);
		return -EINVAL;
	}

	/* device_index is the right 4 bit in ctxt_to_hw_map */
	args->ctxt_to_hw_map = (void *)device_index;

	return 0;
}

static int cam_lrme_mgr_hw_release(void *hw_mgr_priv, void *hw_release_args)
{
	int rc = 0;
	struct cam_lrme_hw_mgr *hw_mgr = hw_mgr_priv;
	struct cam_hw_release_args *args =
		(struct cam_hw_release_args *)hw_release_args;
	uint64_t device_index;

	if (!hw_mgr_priv || !hw_release_args) {
		CAM_ERR(CAM_LRME, "Invalid arguments %pK, %pK",
			hw_mgr_priv, hw_release_args);
		return -EINVAL;
	}

	device_index = CAM_LRME_DECODE_DEVICE_INDEX(args->ctxt_to_hw_map);
	if (device_index >= hw_mgr->device_count) {
		CAM_ERR(CAM_LRME, "Invalid device index %llu", device_index);
		return -EPERM;
	}

	rc = cam_lrme_mgr_util_release(hw_mgr, device_index);
	if (rc)
		CAM_ERR(CAM_LRME, "Failed in release device, rc=%d", rc);

	return rc;
}

static int cam_lrme_mgr_hw_flush(void *hw_mgr_priv, void *hw_flush_args)
{	int rc = 0, i;
	struct cam_lrme_hw_mgr *hw_mgr = hw_mgr_priv;
	struct cam_hw_flush_args *args;
	struct cam_lrme_device *hw_device;
	struct cam_lrme_frame_request *frame_req = NULL, *req_to_flush = NULL;
	struct cam_lrme_frame_request **req_list = NULL;
	uint32_t device_index;
	struct cam_lrme_hw_flush_args lrme_flush_args;
	uint32_t priority;

	if (!hw_mgr_priv || !hw_flush_args) {
		CAM_ERR(CAM_LRME, "Invalid args %pK %pK",
			hw_mgr_priv, hw_flush_args);
		return -EINVAL;
	}

	args = (struct cam_hw_flush_args *)hw_flush_args;
	device_index = ((uintptr_t)args->ctxt_to_hw_map & 0xF);
	if (device_index >= hw_mgr->device_count) {
		CAM_ERR(CAM_LRME, "Invalid device index %d", device_index);
		return -EPERM;
	}

	rc = cam_lrme_mgr_util_get_device(hw_mgr, device_index, &hw_device);
	if (rc) {
		CAM_ERR(CAM_LRME, "Error in getting device %d", rc);
		goto end;
	}

	req_list = (struct cam_lrme_frame_request **)args->flush_req_pending;
	for (i = 0; i < args->num_req_pending; i++) {
		frame_req = req_list[i];
		memset(frame_req, 0x0, sizeof(*frame_req));
		cam_lrme_mgr_util_put_frame_req(&hw_mgr->frame_free_list,
			&frame_req->frame_list, &hw_mgr->free_req_lock);
	}

	req_list = (struct cam_lrme_frame_request **)args->flush_req_active;
	for (i = 0; i < args->num_req_active; i++) {
		frame_req = req_list[i];
		priority = CAM_LRME_DECODE_PRIORITY(args->ctxt_to_hw_map);
		spin_lock((priority == CAM_LRME_PRIORITY_HIGH) ?
			&hw_device->high_req_lock :
			&hw_device->normal_req_lock);
		if (!list_empty(&frame_req->frame_list)) {
			list_del_init(&frame_req->frame_list);
			cam_lrme_mgr_util_put_frame_req(
				&hw_mgr->frame_free_list,
				&frame_req->frame_list,
				&hw_mgr->free_req_lock);
		} else
			req_to_flush = frame_req;
		spin_unlock((priority == CAM_LRME_PRIORITY_HIGH) ?
			&hw_device->high_req_lock :
			&hw_device->normal_req_lock);
	}
	if (!req_to_flush)
		goto end;
	if (hw_device->hw_intf.hw_ops.flush) {
		lrme_flush_args.ctxt_to_hw_map = req_to_flush->ctxt_to_hw_map;
		lrme_flush_args.flush_type = args->flush_type;
		lrme_flush_args.req_to_flush = req_to_flush;
		rc = hw_device->hw_intf.hw_ops.flush(hw_device->hw_intf.hw_priv,
			&lrme_flush_args,
			sizeof(lrme_flush_args));
		if (rc) {
			CAM_ERR(CAM_LRME, "Failed in HW Stop %d", rc);
			goto end;
		}
	} else {
		CAM_ERR(CAM_LRME, "No stop ops");
		goto end;
	}

end:
	return rc;
}


static int cam_lrme_mgr_hw_start(void *hw_mgr_priv, void *hw_start_args)
{
	int rc = 0;
	struct cam_lrme_hw_mgr *hw_mgr = hw_mgr_priv;
	struct cam_hw_start_args *args =
		(struct cam_hw_start_args *)hw_start_args;
	struct cam_lrme_device *hw_device;
	uint32_t device_index;

	if (!hw_mgr || !args) {
		CAM_ERR(CAM_LRME, "Invalid input params");
		return -EINVAL;
	}

	device_index = CAM_LRME_DECODE_DEVICE_INDEX(args->ctxt_to_hw_map);
	if (device_index >= hw_mgr->device_count) {
		CAM_ERR(CAM_LRME, "Invalid device index %d", device_index);
		return -EPERM;
	}

	CAM_DBG(CAM_LRME, "Start device index %d", device_index);

	rc = cam_lrme_mgr_util_get_device(hw_mgr, device_index, &hw_device);
	if (rc) {
		CAM_ERR(CAM_LRME, "Failed to get hw device");
		return rc;
	}

	if (hw_device->hw_intf.hw_ops.start) {
		rc = hw_device->hw_intf.hw_ops.start(
			hw_device->hw_intf.hw_priv, NULL, 0);
	} else {
		CAM_ERR(CAM_LRME, "Invalid start function");
		return -EINVAL;
	}

	rc = hw_device->hw_intf.hw_ops.process_cmd(
			hw_device->hw_intf.hw_priv,
			CAM_LRME_HW_CMD_DUMP_REGISTER,
			&g_lrme_hw_mgr.debugfs_entry.dump_register,
			sizeof(bool));

	return rc;
}

static int cam_lrme_mgr_hw_stop(void *hw_mgr_priv, void *stop_args)
{
	int rc = 0;
	struct cam_lrme_hw_mgr *hw_mgr = hw_mgr_priv;
	struct cam_hw_stop_args *args =
		(struct cam_hw_stop_args *)stop_args;
	struct cam_lrme_device *hw_device;
	uint32_t device_index;

	if (!hw_mgr_priv || !stop_args) {
		CAM_ERR(CAM_LRME, "Invalid arguments");
		return -EINVAL;
	}

	device_index = CAM_LRME_DECODE_DEVICE_INDEX(args->ctxt_to_hw_map);
	if (device_index >= hw_mgr->device_count) {
		CAM_ERR(CAM_LRME, "Invalid device index %d", device_index);
		return -EPERM;
	}

	CAM_DBG(CAM_LRME, "Stop device index %d", device_index);

	rc = cam_lrme_mgr_util_get_device(hw_mgr, device_index, &hw_device);
	if (rc) {
		CAM_ERR(CAM_LRME, "Failed to get hw device");
		return rc;
	}

	if (hw_device->hw_intf.hw_ops.stop) {
		rc = hw_device->hw_intf.hw_ops.stop(
			hw_device->hw_intf.hw_priv, NULL, 0);
		if (rc) {
			CAM_ERR(CAM_LRME, "Failed in HW stop %d", rc);
			goto end;
		}
	}

end:
	return rc;
}

static int cam_lrme_mgr_hw_prepare_update(void *hw_mgr_priv,
	void *hw_prepare_update_args)
{
	int rc = 0, i;
	struct cam_lrme_hw_mgr *hw_mgr = hw_mgr_priv;
	struct cam_hw_prepare_update_args *args =
		(struct cam_hw_prepare_update_args *)hw_prepare_update_args;
	struct cam_lrme_device *hw_device;
	struct cam_kmd_buf_info kmd_buf;
	struct cam_lrme_hw_cmd_config_args config_args;
	struct cam_lrme_frame_request *frame_req = NULL;
	uint32_t device_index;

	if (!hw_mgr_priv || !hw_prepare_update_args) {
		CAM_ERR(CAM_LRME, "Invalid args %pK %pK",
			hw_mgr_priv, hw_prepare_update_args);
		return -EINVAL;
	}

	device_index = CAM_LRME_DECODE_DEVICE_INDEX(args->ctxt_to_hw_map);
	if (device_index >= hw_mgr->device_count) {
		CAM_ERR(CAM_LRME, "Invalid device index %d", device_index);
		return -EPERM;
	}

	rc = cam_lrme_mgr_util_get_device(hw_mgr, device_index, &hw_device);
	if (rc) {
		CAM_ERR(CAM_LRME, "Error in getting device %d", rc);
		goto error;
	}

	rc = cam_lrme_mgr_util_packet_validate(args->packet, args->remain_len);
	if (rc) {
		CAM_ERR(CAM_LRME, "Error in packet validation %d", rc);
		goto error;
	}

	rc = cam_packet_util_get_kmd_buffer(args->packet, &kmd_buf);
	if (rc) {
		CAM_ERR(CAM_LRME, "Error in get kmd buf buffer %d", rc);
		goto error;
	}

	CAM_DBG(CAM_LRME,
		"KMD Buf : hdl=%d, cpu_addr=%pK, offset=%d, size=%d, used=%d",
		kmd_buf.handle, kmd_buf.cpu_addr, kmd_buf.offset,
		kmd_buf.size, kmd_buf.used_bytes);

	rc = cam_packet_util_process_patches(args->packet,
		hw_mgr->device_iommu.non_secure,
		hw_mgr->device_iommu.secure, 0);
	if (rc) {
		CAM_ERR(CAM_LRME, "Patch packet failed, rc=%d", rc);
		return rc;
	}

	memset(&config_args, 0, sizeof(config_args));
	config_args.hw_device = hw_device;

	rc = cam_lrme_mgr_util_prepare_io_buffer(
		hw_mgr->device_iommu.non_secure, args,
		config_args.input_buf, config_args.output_buf,
		CAM_LRME_MAX_IO_BUFFER);
	if (rc) {
		CAM_ERR(CAM_LRME, "Error in prepare IO Buf %d", rc);
		goto error;
	}
	/* Check port number */
	if (args->num_in_map_entries == 0 || args->num_out_map_entries == 0) {
		CAM_ERR(CAM_LRME, "Error in port number in %d, out %d",
			args->num_in_map_entries, args->num_out_map_entries);
		rc = -EINVAL;
		goto error;
	}

	rc = cam_lrme_mgr_util_prepare_hw_update_entries(hw_mgr, args,
		&config_args, &kmd_buf);
	if (rc) {
		CAM_ERR(CAM_LRME, "Error in hw update entries %d", rc);
		goto error;
	}

	rc = cam_lrme_mgr_util_get_frame_req(&hw_mgr->frame_free_list,
		&frame_req, &hw_mgr->free_req_lock);
	if (rc || !frame_req) {
		CAM_ERR(CAM_LRME, "Can not get free frame request");
		goto error;
	}

	frame_req->ctxt_to_hw_map = args->ctxt_to_hw_map;
	frame_req->req_id = args->packet->header.request_id;
	frame_req->hw_device = hw_device;
	frame_req->num_hw_update_entries = args->num_hw_update_entries;
	for (i = 0; i < args->num_hw_update_entries; i++)
		frame_req->hw_update_entries[i] = args->hw_update_entries[i];

	args->priv = frame_req;

	CAM_DBG(CAM_LRME, "FramePrepare : Frame[%lld]", frame_req->req_id);

	return 0;

error:
	return rc;
}

static int cam_lrme_mgr_hw_config(void *hw_mgr_priv,
	void *hw_config_args)
{
	int rc = 0;
	struct cam_lrme_hw_mgr *hw_mgr = hw_mgr_priv;
	struct cam_hw_config_args *args =
		(struct cam_hw_config_args *)hw_config_args;
	struct cam_lrme_frame_request *frame_req;
	struct cam_lrme_device *hw_device = NULL;
	enum cam_lrme_hw_mgr_ctx_priority priority;

	if (!hw_mgr_priv || !hw_config_args) {
		CAM_ERR(CAM_LRME, "Invalid arguments, hw_mgr %pK, config %pK",
			hw_mgr_priv, hw_config_args);
		return -EINVAL;
	}

	if (!args->num_hw_update_entries) {
		CAM_ERR(CAM_LRME, "No hw update entries");
		return -EINVAL;
	}

	frame_req = (struct cam_lrme_frame_request *)args->priv;
	if (!frame_req) {
		CAM_ERR(CAM_LRME, "No frame request");
		return -EINVAL;
	}

	hw_device = frame_req->hw_device;
	if (!hw_device)
		return -EINVAL;

	priority = CAM_LRME_DECODE_PRIORITY(args->ctxt_to_hw_map);
	if (priority == CAM_LRME_PRIORITY_HIGH) {
		cam_lrme_mgr_util_put_frame_req(
			&hw_device->frame_pending_list_high,
			&frame_req->frame_list, &hw_device->high_req_lock);
	} else {
		cam_lrme_mgr_util_put_frame_req(
			&hw_device->frame_pending_list_normal,
			&frame_req->frame_list, &hw_device->normal_req_lock);
	}

	CAM_DBG(CAM_LRME, "schedule req %llu", frame_req->req_id);
	rc = cam_lrme_mgr_util_schedule_frame_req(hw_mgr, hw_device);

	return rc;
}

static int cam_lrme_mgr_create_debugfs_entry(void)
{
	int rc = 0;

	g_lrme_hw_mgr.debugfs_entry.dentry =
		debugfs_create_dir("camera_lrme", NULL);
	if (!g_lrme_hw_mgr.debugfs_entry.dentry) {
		CAM_ERR(CAM_LRME, "failed to create dentry");
		return -ENOMEM;
	}

	if (!debugfs_create_bool("dump_register",
		0644,
		g_lrme_hw_mgr.debugfs_entry.dentry,
		&g_lrme_hw_mgr.debugfs_entry.dump_register)) {
		CAM_ERR(CAM_LRME, "failed to create dump register entry");
		rc = -ENOMEM;
		goto err;
	}

	return rc;

err:
	debugfs_remove_recursive(g_lrme_hw_mgr.debugfs_entry.dentry);
	g_lrme_hw_mgr.debugfs_entry.dentry = NULL;
	return rc;
}


int cam_lrme_mgr_register_device(
	struct cam_hw_intf *lrme_hw_intf,
	struct cam_iommu_handle *device_iommu,
	struct cam_iommu_handle *cdm_iommu)
{
	struct cam_lrme_device *hw_device;
	char buf[128];
	int i, rc;

	hw_device = &g_lrme_hw_mgr.hw_device[lrme_hw_intf->hw_idx];

	g_lrme_hw_mgr.device_iommu = *device_iommu;
	g_lrme_hw_mgr.cdm_iommu = *cdm_iommu;

	memcpy(&hw_device->hw_intf, lrme_hw_intf, sizeof(struct cam_hw_intf));

	spin_lock_init(&hw_device->high_req_lock);
	spin_lock_init(&hw_device->normal_req_lock);
	INIT_LIST_HEAD(&hw_device->frame_pending_list_high);
	INIT_LIST_HEAD(&hw_device->frame_pending_list_normal);

	rc = snprintf(buf, sizeof(buf), "cam_lrme_device_submit_worker%d",
		lrme_hw_intf->hw_idx);
	CAM_DBG(CAM_LRME, "Create submit workq for %s", buf);
	rc = cam_req_mgr_workq_create(buf,
		CAM_LRME_WORKQ_NUM_TASK,
		&hw_device->work, CRM_WORKQ_USAGE_NON_IRQ,
		0);
	if (rc) {
		CAM_ERR(CAM_LRME,
			"Unable to create a worker, rc=%d", rc);
		return rc;
	}

	for (i = 0; i < CAM_LRME_WORKQ_NUM_TASK; i++)
		hw_device->work->task.pool[i].payload =
			&hw_device->work_data[i];

	if (hw_device->hw_intf.hw_ops.process_cmd) {
		struct cam_lrme_hw_cmd_set_cb cb_args;

		cb_args.cam_lrme_hw_mgr_cb = cam_lrme_mgr_cb;
		cb_args.data = hw_device;

		rc = hw_device->hw_intf.hw_ops.process_cmd(
			hw_device->hw_intf.hw_priv,
			CAM_LRME_HW_CMD_REGISTER_CB,
			&cb_args, sizeof(cb_args));
		if (rc) {
			CAM_ERR(CAM_LRME, "Register cb failed");
			goto destroy_workqueue;
		}
		CAM_DBG(CAM_LRME, "cb registered");
	}

	if (hw_device->hw_intf.hw_ops.get_hw_caps) {
		rc = hw_device->hw_intf.hw_ops.get_hw_caps(
			hw_device->hw_intf.hw_priv, &hw_device->hw_caps,
			sizeof(hw_device->hw_caps));
		if (rc)
			CAM_ERR(CAM_LRME, "Get caps failed");
	} else {
		CAM_ERR(CAM_LRME, "No get_hw_caps function");
		goto destroy_workqueue;
	}
	g_lrme_hw_mgr.lrme_caps.dev_caps[lrme_hw_intf->hw_idx] =
		hw_device->hw_caps;
	g_lrme_hw_mgr.device_count++;
	g_lrme_hw_mgr.lrme_caps.device_iommu = g_lrme_hw_mgr.device_iommu;
	g_lrme_hw_mgr.lrme_caps.cdm_iommu = g_lrme_hw_mgr.cdm_iommu;
	g_lrme_hw_mgr.lrme_caps.num_devices = g_lrme_hw_mgr.device_count;

	hw_device->valid = true;

	CAM_DBG(CAM_LRME, "device registration done");
	return 0;

destroy_workqueue:
	cam_req_mgr_workq_destroy(&hw_device->work);

	return rc;
}

int cam_lrme_mgr_deregister_device(int device_index)
{
	struct cam_lrme_device *hw_device;

	hw_device = &g_lrme_hw_mgr.hw_device[device_index];
	cam_req_mgr_workq_destroy(&hw_device->work);
	memset(hw_device, 0x0, sizeof(struct cam_lrme_device));
	g_lrme_hw_mgr.device_count--;

	return 0;
}

int cam_lrme_hw_mgr_deinit(void)
{
	mutex_destroy(&g_lrme_hw_mgr.hw_mgr_mutex);
	memset(&g_lrme_hw_mgr, 0x0, sizeof(g_lrme_hw_mgr));

	return 0;
}

int cam_lrme_hw_mgr_init(struct cam_hw_mgr_intf *hw_mgr_intf,
	cam_hw_event_cb_func cam_lrme_dev_buf_done_cb)
{
	int i, rc = 0;
	struct cam_lrme_frame_request *frame_req;

	if (!hw_mgr_intf)
		return -EINVAL;

	CAM_DBG(CAM_LRME, "device count %d", g_lrme_hw_mgr.device_count);
	if (g_lrme_hw_mgr.device_count > CAM_LRME_HW_MAX) {
		CAM_ERR(CAM_LRME, "Invalid count of devices");
		return -EINVAL;
	}

	memset(hw_mgr_intf, 0, sizeof(*hw_mgr_intf));

	mutex_init(&g_lrme_hw_mgr.hw_mgr_mutex);
	spin_lock_init(&g_lrme_hw_mgr.free_req_lock);
	INIT_LIST_HEAD(&g_lrme_hw_mgr.frame_free_list);

	/* Init hw mgr frame requests and add to free list */
	for (i = 0; i < CAM_CTX_REQ_MAX * CAM_CTX_MAX; i++) {
		frame_req = &g_lrme_hw_mgr.frame_req[i];

		memset(frame_req, 0x0, sizeof(*frame_req));
		INIT_LIST_HEAD(&frame_req->frame_list);

		list_add_tail(&frame_req->frame_list,
			&g_lrme_hw_mgr.frame_free_list);
	}

	hw_mgr_intf->hw_mgr_priv = &g_lrme_hw_mgr;
	hw_mgr_intf->hw_get_caps = cam_lrme_mgr_get_caps;
	hw_mgr_intf->hw_acquire = cam_lrme_mgr_hw_acquire;
	hw_mgr_intf->hw_release = cam_lrme_mgr_hw_release;
	hw_mgr_intf->hw_start = cam_lrme_mgr_hw_start;
	hw_mgr_intf->hw_stop = cam_lrme_mgr_hw_stop;
	hw_mgr_intf->hw_prepare_update = cam_lrme_mgr_hw_prepare_update;
	hw_mgr_intf->hw_config = cam_lrme_mgr_hw_config;
	hw_mgr_intf->hw_read = NULL;
	hw_mgr_intf->hw_write = NULL;
	hw_mgr_intf->hw_close = NULL;
	hw_mgr_intf->hw_flush = cam_lrme_mgr_hw_flush;

	g_lrme_hw_mgr.event_cb = cam_lrme_dev_buf_done_cb;

	cam_lrme_mgr_create_debugfs_entry();

	CAM_DBG(CAM_LRME, "Hw mgr init done");
	return rc;
}
