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

#include <linux/module.h>
#include <linux/kernel.h>
#include <media/cam_cpas.h>
#include <media/cam_req_mgr.h>

#include "cam_io_util.h"
#include "cam_soc_util.h"
#include "cam_mem_mgr_api.h"
#include "cam_smmu_api.h"
#include "cam_packet_util.h"
#include "cam_fd_context.h"
#include "cam_fd_hw_intf.h"
#include "cam_fd_hw_core.h"
#include "cam_fd_hw_soc.h"
#include "cam_fd_hw_mgr_intf.h"
#include "cam_fd_hw_mgr.h"
#include "cam_trace.h"

static struct cam_fd_hw_mgr g_fd_hw_mgr;

static int cam_fd_mgr_util_packet_validate(struct cam_packet *packet)
{
	struct cam_cmd_buf_desc *cmd_desc = NULL;
	int i, rc;

	if (!packet)
		return -EINVAL;

	CAM_DBG(CAM_FD, "Packet request=%d, op_code=0x%x, size=%d, flags=%d",
		packet->header.request_id, packet->header.op_code,
		packet->header.size, packet->header.flags);
	CAM_DBG(CAM_FD,
		"Packet cmdbuf(offset=%d, num=%d) io(offset=%d, num=%d)",
		packet->cmd_buf_offset, packet->num_cmd_buf,
		packet->io_configs_offset, packet->num_io_configs);
	CAM_DBG(CAM_FD,
		"Packet Patch(offset=%d, num=%d) kmd(offset=%d, num=%d)",
		packet->patch_offset, packet->num_patches,
		packet->kmd_cmd_buf_offset, packet->kmd_cmd_buf_index);

	if (cam_packet_util_validate_packet(packet)) {
		CAM_ERR(CAM_FD, "invalid packet:%d %d %d %d %d",
			packet->kmd_cmd_buf_index,
			packet->num_cmd_buf, packet->cmd_buf_offset,
			packet->io_configs_offset, packet->header.size);
		return -EINVAL;
	}

	/* All buffers must come through io config, do not support patching */
	if (packet->num_patches || !packet->num_io_configs) {
		CAM_ERR(CAM_FD, "wrong number of cmd/patch info: %u %u",
			packet->num_cmd_buf, packet->num_patches);
		return -EINVAL;
	}

	/* KMD Buf index can never be greater than or equal to num cmd bufs */
	if (packet->kmd_cmd_buf_index >= packet->num_cmd_buf) {
		CAM_ERR(CAM_FD, "Invalid kmd index %d (%d)",
			packet->kmd_cmd_buf_index, packet->num_cmd_buf);
		return -EINVAL;
	}

	if ((packet->header.op_code & 0xff) !=
		CAM_PACKET_OPCODES_FD_FRAME_UPDATE) {
		CAM_ERR(CAM_FD, "Invalid op_code %u",
			packet->header.op_code & 0xff);
		return -EINVAL;
	}

	cmd_desc = (struct cam_cmd_buf_desc *) ((uint8_t *)&packet->payload +
		packet->cmd_buf_offset);

	for (i = 0; i < packet->num_cmd_buf; i++) {
		/*
		 * We can allow 0 length cmd buffer. This can happen in case
		 * umd gives an empty cmd buffer as kmd buffer
		 */
		if (!cmd_desc[i].length)
			continue;

		if ((cmd_desc[i].meta_data != CAM_FD_CMD_BUFFER_ID_GENERIC) &&
			(cmd_desc[i].meta_data != CAM_FD_CMD_BUFFER_ID_CDM)) {
			CAM_ERR(CAM_FD, "Invalid meta_data [%d] %u", i,
				cmd_desc[i].meta_data);
			return -EINVAL;
		}

		CAM_DBG(CAM_FD,
			"CmdBuf[%d] hdl=%d, offset=%d, size=%d, len=%d, type=%d, meta_data=%d",
			i,
			cmd_desc[i].mem_handle, cmd_desc[i].offset,
			cmd_desc[i].size, cmd_desc[i].length, cmd_desc[i].type,
			cmd_desc[i].meta_data);

		rc = cam_packet_util_validate_cmd_desc(&cmd_desc[i]);
		if (rc) {
			CAM_ERR(CAM_FD, "Invalid cmd buffer %d", i);
			return rc;
		}
	}

	return 0;
}

static int cam_fd_mgr_util_put_ctx(
	struct list_head *src_list,
	struct cam_fd_hw_mgr_ctx **fd_ctx)
{
	int rc = 0;
	struct cam_fd_hw_mgr_ctx *ctx_ptr = NULL;

	mutex_lock(&g_fd_hw_mgr.ctx_mutex);
	ctx_ptr = *fd_ctx;
	if (ctx_ptr)
		list_add_tail(&ctx_ptr->list, src_list);
	*fd_ctx = NULL;
	mutex_unlock(&g_fd_hw_mgr.ctx_mutex);

	return rc;
}

static int cam_fd_mgr_util_get_ctx(
	struct list_head *src_list,
	struct cam_fd_hw_mgr_ctx **fd_ctx)
{
	int rc = 0;
	struct cam_fd_hw_mgr_ctx *ctx_ptr = NULL;

	mutex_lock(&g_fd_hw_mgr.ctx_mutex);
	if (!list_empty(src_list)) {
		ctx_ptr = list_first_entry(src_list,
			struct cam_fd_hw_mgr_ctx, list);
		list_del_init(&ctx_ptr->list);
	} else {
		CAM_ERR(CAM_FD, "No more free fd hw mgr ctx");
		rc = -1;
	}
	*fd_ctx = ctx_ptr;
	mutex_unlock(&g_fd_hw_mgr.ctx_mutex);

	return rc;
}

static int cam_fd_mgr_util_put_frame_req(
	struct list_head *src_list,
	struct cam_fd_mgr_frame_request **frame_req)
{
	int rc = 0;
	struct cam_fd_mgr_frame_request *req_ptr = NULL;

	mutex_lock(&g_fd_hw_mgr.frame_req_mutex);
	req_ptr = *frame_req;
	if (req_ptr)
		list_add_tail(&req_ptr->list, src_list);
	*frame_req = NULL;
	mutex_unlock(&g_fd_hw_mgr.frame_req_mutex);

	return rc;
}

static int cam_fd_mgr_util_get_frame_req(
	struct list_head *src_list,
	struct cam_fd_mgr_frame_request **frame_req)
{
	int rc = 0;
	struct cam_fd_mgr_frame_request *req_ptr = NULL;

	mutex_lock(&g_fd_hw_mgr.frame_req_mutex);
	if (!list_empty(src_list)) {
		req_ptr = list_first_entry(src_list,
			struct cam_fd_mgr_frame_request, list);
		list_del_init(&req_ptr->list);
	} else {
		CAM_DBG(CAM_FD, "Frame req not available");
		rc = -EPERM;
	}
	*frame_req = req_ptr;
	mutex_unlock(&g_fd_hw_mgr.frame_req_mutex);

	return rc;
}

static int cam_fd_mgr_util_get_device(struct cam_fd_hw_mgr *hw_mgr,
	struct cam_fd_hw_mgr_ctx *hw_ctx, struct cam_fd_device **hw_device)
{
	if (!hw_mgr || !hw_ctx || !hw_device) {
		CAM_ERR(CAM_FD, "Invalid input %pK %pK %pK", hw_mgr, hw_ctx,
			hw_device);
		return -EINVAL;
	}

	if ((hw_ctx->device_index < 0) ||
		(hw_ctx->device_index >= CAM_FD_HW_MAX)) {
		CAM_ERR(CAM_FD, "Invalid device indx %d", hw_ctx->device_index);
		return -EINVAL;
	}

	CAM_DBG(CAM_FD, "ctx_index=%u, hw_ctx=%d", hw_ctx->ctx_index,
		hw_ctx->device_index);

	*hw_device = &hw_mgr->hw_device[hw_ctx->device_index];

	return 0;
}

static int cam_fd_mgr_util_release_device(struct cam_fd_hw_mgr *hw_mgr,
	struct cam_fd_hw_mgr_ctx *hw_ctx)
{
	struct cam_fd_device *hw_device;
	struct cam_fd_hw_release_args hw_release_args;
	int rc;

	rc = cam_fd_mgr_util_get_device(hw_mgr, hw_ctx, &hw_device);
	if (rc) {
		CAM_ERR(CAM_FD, "Error in getting device %d", rc);
		return rc;
	}

	if (hw_device->hw_intf->hw_ops.release) {
		hw_release_args.hw_ctx = hw_ctx;
		hw_release_args.ctx_hw_private = hw_ctx->ctx_hw_private;
		rc = hw_device->hw_intf->hw_ops.release(
			hw_device->hw_intf->hw_priv, &hw_release_args,
			sizeof(hw_release_args));
		if (rc) {
			CAM_ERR(CAM_FD, "Failed in HW release %d", rc);
			return rc;
		}
	} else {
		CAM_ERR(CAM_FD, "Invalid release function");
	}

	mutex_lock(&hw_mgr->hw_mgr_mutex);
	hw_device->num_ctxts--;
	mutex_unlock(&hw_mgr->hw_mgr_mutex);

	hw_ctx->device_index = -1;

	return rc;
}

static int cam_fd_mgr_util_select_device(struct cam_fd_hw_mgr *hw_mgr,
	struct cam_fd_hw_mgr_ctx *hw_ctx,
	struct cam_fd_acquire_dev_info *fd_acquire_args)
{
	int i, rc;
	struct cam_fd_hw_reserve_args hw_reserve_args;
	struct cam_fd_device *hw_device = NULL;

	if (!hw_mgr || !hw_ctx || !fd_acquire_args) {
		CAM_ERR(CAM_FD, "Invalid input %pK %pK %pK", hw_mgr, hw_ctx,
			fd_acquire_args);
		return -EINVAL;
	}

	mutex_lock(&hw_mgr->hw_mgr_mutex);

	/* Check if a device is free which can satisfy the requirements */
	for (i = 0; i < hw_mgr->num_devices; i++) {
		hw_device = &hw_mgr->hw_device[i];
		CAM_DBG(CAM_FD,
			"[%d] : num_ctxts=%d, modes=%d, raw_results=%d",
			i, hw_device->num_ctxts,
			hw_device->hw_caps.supported_modes,
			hw_device->hw_caps.raw_results_available);
		if ((hw_device->num_ctxts == 0) &&
			(fd_acquire_args->mode &
			hw_device->hw_caps.supported_modes) &&
			(!fd_acquire_args->get_raw_results ||
			hw_device->hw_caps.raw_results_available)) {
			CAM_DBG(CAM_FD, "Found dedicated HW Index=%d", i);
			hw_device->num_ctxts++;
			break;
		}
	}

	/*
	 * We couldn't find a free HW which meets requirement, now check if
	 * there is a HW which meets acquire requirements
	 */
	if (i == hw_mgr->num_devices) {
		for (i = 0; i < hw_mgr->num_devices; i++) {
			hw_device = &hw_mgr->hw_device[i];
			if ((fd_acquire_args->mode &
				hw_device->hw_caps.supported_modes) &&
				(!fd_acquire_args->get_raw_results ||
				hw_device->hw_caps.raw_results_available)) {
				hw_device->num_ctxts++;
				CAM_DBG(CAM_FD, "Found sharing HW Index=%d", i);
				break;
			}
		}
	}

	mutex_unlock(&hw_mgr->hw_mgr_mutex);

	if ((i == hw_mgr->num_devices) || !hw_device) {
		CAM_ERR(CAM_FD, "Couldn't acquire HW %d %d",
			fd_acquire_args->mode,
			fd_acquire_args->get_raw_results);
		return -EBUSY;
	}

	CAM_DBG(CAM_FD, "Device index %d selected for this acquire", i);

	/* Check if we can reserve this HW */
	if (hw_device->hw_intf->hw_ops.reserve) {
		hw_reserve_args.hw_ctx = hw_ctx;
		hw_reserve_args.mode = fd_acquire_args->mode;
		rc = hw_device->hw_intf->hw_ops.reserve(
			hw_device->hw_intf->hw_priv, &hw_reserve_args,
			sizeof(hw_reserve_args));
		if (rc) {
			CAM_ERR(CAM_FD, "Failed in HW reserve %d", rc);
			return rc;
		}
		hw_ctx->ctx_hw_private = hw_reserve_args.ctx_hw_private;
	} else {
		CAM_ERR(CAM_FD, "Invalid reserve function");
		return -EPERM;
	}

	/* Update required info in hw context */
	hw_ctx->device_index = i;

	CAM_DBG(CAM_FD, "ctx index=%u, device_index=%d", hw_ctx->ctx_index,
		hw_ctx->device_index);

	return 0;
}

static int cam_fd_mgr_util_pdev_get_hw_intf(struct device_node *of_node,
	int i, struct cam_hw_intf **device_hw_intf)
{
	struct device_node *device_node = NULL;
	struct platform_device *child_pdev = NULL;
	struct cam_hw_intf *hw_intf = NULL;
	const char *name = NULL;
	int rc;

	rc = of_property_read_string_index(of_node, "compat-hw-name", i, &name);
	if (rc) {
		CAM_ERR(CAM_FD, "Getting dev object name failed %d %d", i, rc);
		goto put_node;
	}

	device_node = of_find_node_by_name(NULL, name);
	if (!device_node) {
		CAM_ERR(CAM_FD, "Cannot find node in dtsi %s", name);
		return -ENODEV;
	}

	child_pdev = of_find_device_by_node(device_node);
	if (!child_pdev) {
		CAM_ERR(CAM_FD, "Failed to find device on bus %s",
			device_node->name);
		rc = -ENODEV;
		goto put_node;
	}

	hw_intf = (struct cam_hw_intf *)platform_get_drvdata(child_pdev);
	if (!hw_intf) {
		CAM_ERR(CAM_FD, "No driver data for child device");
		rc = -ENODEV;
		goto put_node;
	}

	CAM_DBG(CAM_FD, "child type %d index %d child_intf %pK",
		hw_intf->hw_type, hw_intf->hw_idx, hw_intf);

	if (hw_intf->hw_idx >= CAM_FD_HW_MAX) {
		CAM_ERR(CAM_FD, "hw_idx invalid %d", hw_intf->hw_idx);
		rc = -ENODEV;
		goto put_node;
	}

	rc = 0;
	*device_hw_intf = hw_intf;

put_node:
	of_node_put(device_node);

	return rc;
}

static int cam_fd_packet_generic_blob_handler(void *user_data,
	uint32_t blob_type, uint32_t blob_size, uint8_t *blob_data)
{
	struct cam_fd_hw_cmd_prestart_args *prestart_args =
		(struct cam_fd_hw_cmd_prestart_args *)user_data;

	if (!blob_data || (blob_size == 0)) {
		CAM_ERR(CAM_FD, "Invalid blob info %pK %u", blob_data,
			blob_size);
		return -EINVAL;
	}

	if (!prestart_args) {
		CAM_ERR(CAM_FD, "Invalid user data");
		return -EINVAL;
	}

	switch (blob_type) {
	case CAM_FD_BLOB_TYPE_RAW_RESULTS_REQUIRED: {
		uint32_t *get_raw_results = (uint32_t *)blob_data;

		if (sizeof(uint32_t) != blob_size) {
			CAM_ERR(CAM_FD, "Invalid blob size %lu %u",
				sizeof(uint32_t), blob_size);
			return -EINVAL;
		}

		prestart_args->get_raw_results = *get_raw_results;
		break;
	}
	case CAM_FD_BLOB_TYPE_SOC_CLOCK_BW_REQUEST: {
		struct cam_fd_soc_clock_bw_request *clk_req =
			(struct cam_fd_soc_clock_bw_request *)blob_data;

		if (sizeof(struct cam_fd_soc_clock_bw_request) != blob_size) {
			CAM_ERR(CAM_FD, "Invalid blob size %lu %u",
				sizeof(struct cam_fd_soc_clock_bw_request),
				blob_size);
			return -EINVAL;
		}

		CAM_DBG(CAM_FD, "SOC Clk Request clock=%lld, bw=%lld",
			clk_req->clock_rate, clk_req->bandwidth);

		break;
	}
	default:
		CAM_WARN(CAM_FD, "Unknown blob type %d", blob_type);
		break;
	}

	return 0;
}

static int cam_fd_mgr_util_parse_generic_cmd_buffer(
	struct cam_fd_hw_mgr_ctx *hw_ctx, struct cam_packet *packet,
	struct cam_fd_hw_cmd_prestart_args *prestart_args)
{
	struct cam_cmd_buf_desc *cmd_desc = NULL;
	int i, rc = 0;

	cmd_desc = (struct cam_cmd_buf_desc *) ((uint8_t *)&packet->payload +
		packet->cmd_buf_offset);

	for (i = 0; i < packet->num_cmd_buf; i++) {
		if (!cmd_desc[i].length)
			continue;

		if (cmd_desc[i].meta_data == CAM_FD_CMD_BUFFER_ID_CDM)
			continue;

		rc = cam_packet_util_validate_cmd_desc(&cmd_desc[i]);
		if (rc)
			return rc;

		rc = cam_packet_util_process_generic_cmd_buffer(&cmd_desc[i],
			cam_fd_packet_generic_blob_handler, prestart_args);
		if (rc)
			CAM_ERR(CAM_FD, "Failed in processing blobs %d", rc);

		break;
	}

	return rc;
}

static int cam_fd_mgr_util_get_buf_map_requirement(uint32_t direction,
	uint32_t resource_type, bool *need_io_map, bool *need_cpu_map)
{
	if (!need_io_map || !need_cpu_map) {
		CAM_ERR(CAM_FD, "Invalid input pointers %pK %pK", need_io_map,
			need_cpu_map);
		return -EINVAL;
	}

	if (direction == CAM_BUF_INPUT) {
		switch (resource_type) {
		case CAM_FD_INPUT_PORT_ID_IMAGE:
			*need_io_map = true;
			*need_cpu_map = false;
			break;
		default:
			CAM_WARN(CAM_FD, "Invalid port: dir %d, id %d",
				direction, resource_type);
			return -EINVAL;
		}
	} else if (direction == CAM_BUF_OUTPUT) {
		switch (resource_type) {
		case CAM_FD_OUTPUT_PORT_ID_RESULTS:
			*need_io_map = true;
			*need_cpu_map = true;
			break;
		case CAM_FD_OUTPUT_PORT_ID_RAW_RESULTS:
			*need_io_map = true;
			*need_cpu_map = true;
			break;
		case CAM_FD_OUTPUT_PORT_ID_WORK_BUFFER:
			*need_io_map = true;
			*need_cpu_map = false;
			break;
		default:
			CAM_WARN(CAM_FD, "Invalid port: dir %d, id %d",
				direction, resource_type);
			return -EINVAL;
		}
	} else {
		CAM_WARN(CAM_FD, "Invalid direction %d", direction);
		return -EINVAL;
	}

	return 0;
}

static int cam_fd_mgr_util_prepare_io_buf_info(int32_t iommu_hdl,
	struct cam_hw_prepare_update_args *prepare,
	struct cam_fd_hw_io_buffer *input_buf,
	struct cam_fd_hw_io_buffer *output_buf, uint32_t io_buf_size)
{
	int rc = -EINVAL;
	uint32_t i, j, plane, num_out_buf, num_in_buf;
	struct cam_buf_io_cfg *io_cfg;
	uint64_t io_addr[CAM_PACKET_MAX_PLANES];
	uint64_t cpu_addr[CAM_PACKET_MAX_PLANES];
	size_t size;
	bool need_io_map, need_cpu_map;

	/* Get IO Buf information */
	num_out_buf = 0;
	num_in_buf  = 0;
	io_cfg = (struct cam_buf_io_cfg *) ((uint8_t *)
		&prepare->packet->payload + prepare->packet->io_configs_offset);

	for (i = 0; i < prepare->packet->num_io_configs; i++) {
		CAM_DBG(CAM_FD,
			"IOConfig[%d] : handle[%d] Dir[%d] Res[%d] Fence[%d], Format[%d]",
			i, io_cfg[i].mem_handle[0], io_cfg[i].direction,
			io_cfg[i].resource_type,
			io_cfg[i].fence, io_cfg[i].format);

		if ((num_in_buf >= io_buf_size) ||
			(num_out_buf >= io_buf_size)) {
			CAM_ERR(CAM_FD, "Invalid number of buffers %d %d %d",
				num_in_buf, num_out_buf, io_buf_size);
			return -EINVAL;
		}

		rc = cam_fd_mgr_util_get_buf_map_requirement(
			io_cfg[i].direction, io_cfg[i].resource_type,
			&need_io_map, &need_cpu_map);
		if (rc) {
			CAM_WARN(CAM_FD, "Invalid io buff [%d] : %d %d %d",
				i, io_cfg[i].direction,
				io_cfg[i].resource_type, rc);
			continue;
		}

		memset(io_addr, 0x0, sizeof(io_addr));
		for (plane = 0; plane < CAM_PACKET_MAX_PLANES; plane++) {
			if (!io_cfg[i].mem_handle[plane])
				break;

			io_addr[plane] = 0x0;
			cpu_addr[plane] = 0x0;

			if (need_io_map) {
				rc = cam_mem_get_io_buf(
					io_cfg[i].mem_handle[plane],
					iommu_hdl, &io_addr[plane], &size);
				if ((rc) || (io_addr[plane] >> 32)) {
					CAM_ERR(CAM_FD,
						"Invalid io buf %d %d %d %d",
						io_cfg[i].direction,
						io_cfg[i].resource_type, plane,
						rc);
					return -ENOMEM;
				}

				io_addr[plane] += io_cfg[i].offsets[plane];
			}

			if (need_cpu_map) {
				rc = cam_mem_get_cpu_buf(
					io_cfg[i].mem_handle[plane],
					&cpu_addr[plane], &size);
				if (rc) {
					CAM_ERR(CAM_FD,
						"Invalid cpu buf %d %d %d %d",
						io_cfg[i].direction,
						io_cfg[i].resource_type, plane,
						rc);
					return rc;
				}

				cpu_addr[plane] += io_cfg[i].offsets[plane];
			}

			CAM_DBG(CAM_FD, "IO Address[%d][%d] : %pK, %pK",
				io_cfg[i].direction, plane, io_addr[plane],
				cpu_addr[plane]);
		}

		switch (io_cfg[i].direction) {
		case CAM_BUF_INPUT: {
			prepare->in_map_entries[num_in_buf].resource_handle =
				io_cfg[i].resource_type;
			prepare->in_map_entries[num_in_buf].sync_id =
				io_cfg[i].fence;

			input_buf[num_in_buf].valid = true;
			for (j = 0; j < plane; j++) {
				input_buf[num_in_buf].io_addr[j] = io_addr[j];
				input_buf[num_in_buf].cpu_addr[j] = cpu_addr[j];
			}
			input_buf[num_in_buf].num_buf = plane;
			input_buf[num_in_buf].io_cfg = &io_cfg[i];

			num_in_buf++;
			break;
		}
		case CAM_BUF_OUTPUT: {
			prepare->out_map_entries[num_out_buf].resource_handle =
				io_cfg[i].resource_type;
			prepare->out_map_entries[num_out_buf].sync_id =
				io_cfg[i].fence;

			output_buf[num_out_buf].valid = true;
			for (j = 0; j < plane; j++) {
				output_buf[num_out_buf].io_addr[j] = io_addr[j];
				output_buf[num_out_buf].cpu_addr[j] =
					cpu_addr[j];
			}
			output_buf[num_out_buf].num_buf = plane;
			output_buf[num_out_buf].io_cfg = &io_cfg[i];

			num_out_buf++;
			break;
		}
		default:
			CAM_ERR(CAM_FD, "Unsupported io direction %d",
				io_cfg[i].direction);
			return -EINVAL;
		}
	}

	prepare->num_in_map_entries  = num_in_buf;
	prepare->num_out_map_entries = num_out_buf;

	return 0;
}

static int cam_fd_mgr_util_prepare_hw_update_entries(
	struct cam_fd_hw_mgr *hw_mgr,
	struct cam_hw_prepare_update_args *prepare,
	struct cam_fd_hw_cmd_prestart_args *prestart_args,
	struct cam_kmd_buf_info *kmd_buf_info)
{
	int i, rc;
	struct cam_hw_update_entry *hw_entry;
	uint32_t num_ent;
	struct cam_fd_hw_mgr_ctx *hw_ctx =
		(struct cam_fd_hw_mgr_ctx *)prepare->ctxt_to_hw_map;
	struct cam_fd_device *hw_device;
	uint32_t kmd_buf_max_size, kmd_buf_used_bytes = 0;
	uint32_t *kmd_buf_addr;
	struct cam_cmd_buf_desc *cmd_desc = NULL;

	rc = cam_fd_mgr_util_get_device(hw_mgr, hw_ctx, &hw_device);
	if (rc) {
		CAM_ERR(CAM_FD, "Error in getting device %d", rc);
		return rc;
	}

	kmd_buf_addr = (uint32_t *)((uint8_t *)kmd_buf_info->cpu_addr +
		kmd_buf_info->used_bytes);
	kmd_buf_max_size = kmd_buf_info->size - kmd_buf_info->used_bytes;

	prestart_args->cmd_buf_addr = kmd_buf_addr;
	prestart_args->size  = kmd_buf_max_size;
	prestart_args->pre_config_buf_size = 0;
	prestart_args->post_config_buf_size = 0;

	if (hw_device->hw_intf->hw_ops.process_cmd) {
		rc = hw_device->hw_intf->hw_ops.process_cmd(
			hw_device->hw_intf->hw_priv, CAM_FD_HW_CMD_PRESTART,
			prestart_args,
			sizeof(struct cam_fd_hw_cmd_prestart_args));
		if (rc) {
			CAM_ERR(CAM_FD, "Failed in CMD_PRESTART %d", rc);
			return rc;
		}
	}

	kmd_buf_used_bytes += prestart_args->pre_config_buf_size;
	kmd_buf_used_bytes += prestart_args->post_config_buf_size;

	/* HW layer is expected to add commands */
	if (!kmd_buf_used_bytes || (kmd_buf_used_bytes > kmd_buf_max_size)) {
		CAM_ERR(CAM_FD, "Invalid kmd used bytes %d (%d)",
			kmd_buf_used_bytes, kmd_buf_max_size);
		return -ENOMEM;
	}

	hw_entry = prepare->hw_update_entries;
	num_ent = 0;

	if (prestart_args->pre_config_buf_size) {
		if ((num_ent + 1) >= prepare->max_hw_update_entries) {
			CAM_ERR(CAM_FD, "Insufficient  HW entries :%d %d",
				num_ent, prepare->max_hw_update_entries);
			return -EINVAL;
		}

		hw_entry[num_ent].handle = kmd_buf_info->handle;
		hw_entry[num_ent].len  = prestart_args->pre_config_buf_size;
		hw_entry[num_ent].offset = kmd_buf_info->offset;

		kmd_buf_info->used_bytes += prestart_args->pre_config_buf_size;
		kmd_buf_info->offset += prestart_args->pre_config_buf_size;
		num_ent++;
	}

	/*
	 * set the cmd_desc to point the first command descriptor in the
	 * packet and update hw entries with CDM command buffers
	 */
	cmd_desc = (struct cam_cmd_buf_desc *)((uint8_t *)
		&prepare->packet->payload + prepare->packet->cmd_buf_offset);

	for (i = 0; i < prepare->packet->num_cmd_buf; i++) {
		if (!cmd_desc[i].length)
			continue;

		if (cmd_desc[i].meta_data != CAM_FD_CMD_BUFFER_ID_CDM)
			continue;

		if (num_ent + 1 >= prepare->max_hw_update_entries) {
			CAM_ERR(CAM_FD, "Insufficient  HW entries :%d %d",
				num_ent, prepare->max_hw_update_entries);
			return -EINVAL;
		}

		hw_entry[num_ent].handle = cmd_desc[i].mem_handle;
		hw_entry[num_ent].len = cmd_desc[i].length;
		hw_entry[num_ent].offset = cmd_desc[i].offset;
		num_ent++;
	}

	if (prestart_args->post_config_buf_size) {
		if (num_ent + 1 >= prepare->max_hw_update_entries) {
			CAM_ERR(CAM_FD, "Insufficient  HW entries :%d %d",
				num_ent, prepare->max_hw_update_entries);
			return -EINVAL;
		}

		hw_entry[num_ent].handle = kmd_buf_info->handle;
		hw_entry[num_ent].len    = prestart_args->post_config_buf_size;
		hw_entry[num_ent].offset = kmd_buf_info->offset;

		kmd_buf_info->used_bytes += prestart_args->post_config_buf_size;
		kmd_buf_info->offset     += prestart_args->post_config_buf_size;

		num_ent++;
	}

	prepare->num_hw_update_entries = num_ent;

	CAM_DBG(CAM_FD, "FinalConfig : hw_entries=%d, Sync(in=%d, out=%d)",
		prepare->num_hw_update_entries, prepare->num_in_map_entries,
		prepare->num_out_map_entries);

	return rc;
}

static int cam_fd_mgr_util_submit_frame(void *priv, void *data)
{
	struct cam_fd_device *hw_device;
	struct cam_fd_hw_mgr *hw_mgr;
	struct cam_fd_mgr_frame_request *frame_req;
	struct cam_fd_hw_mgr_ctx *hw_ctx;
	struct cam_fd_hw_cmd_start_args start_args;
	int rc;

	if (!priv) {
		CAM_ERR(CAM_FD, "Invalid data");
		return -EINVAL;
	}

	hw_mgr = (struct cam_fd_hw_mgr *)priv;
	mutex_lock(&hw_mgr->frame_req_mutex);

	/* Check if we have any frames pending in high priority list */
	if (!list_empty(&hw_mgr->frame_pending_list_high)) {
		CAM_DBG(CAM_FD, "Pending frames in high priority list");
		frame_req = list_first_entry(&hw_mgr->frame_pending_list_high,
			struct cam_fd_mgr_frame_request, list);
	} else if (!list_empty(&hw_mgr->frame_pending_list_normal)) {
		CAM_DBG(CAM_FD, "Pending frames in normal priority list");
		frame_req = list_first_entry(&hw_mgr->frame_pending_list_normal,
			struct cam_fd_mgr_frame_request, list);
	} else {
		mutex_unlock(&hw_mgr->frame_req_mutex);
		CAM_DBG(CAM_FD, "No pending frames");
		return 0;
	}

	CAM_DBG(CAM_FD, "FrameSubmit : Frame[%lld]", frame_req->request_id);
	hw_ctx = frame_req->hw_ctx;
	rc = cam_fd_mgr_util_get_device(hw_mgr, hw_ctx, &hw_device);
	if (rc) {
		mutex_unlock(&hw_mgr->frame_req_mutex);
		CAM_ERR(CAM_FD, "Error in getting device %d", rc);
		return rc;
	}

	mutex_lock(&hw_device->lock);
	if (hw_device->ready_to_process == false) {
		mutex_unlock(&hw_device->lock);
		mutex_unlock(&hw_mgr->frame_req_mutex);
		return -EBUSY;
	}

	trace_cam_submit_to_hw("FD", frame_req->request_id);

	list_del_init(&frame_req->list);
	mutex_unlock(&hw_mgr->frame_req_mutex);

	if (hw_device->hw_intf->hw_ops.start) {
		start_args.hw_ctx = hw_ctx;
		start_args.ctx_hw_private = hw_ctx->ctx_hw_private;
		start_args.hw_req_private = &frame_req->hw_req_private;
		start_args.hw_update_entries = frame_req->hw_update_entries;
		start_args.num_hw_update_entries =
			frame_req->num_hw_update_entries;

		rc = hw_device->hw_intf->hw_ops.start(
			hw_device->hw_intf->hw_priv, &start_args,
			sizeof(start_args));
		if (rc) {
			CAM_ERR(CAM_FD, "Failed in HW Start %d", rc);
			mutex_unlock(&hw_device->lock);
			goto put_req_into_free_list;
		}
	} else {
		CAM_ERR(CAM_FD, "Invalid hw_ops.start");
		mutex_unlock(&hw_device->lock);
		rc = -EPERM;
		goto put_req_into_free_list;
	}

	hw_device->ready_to_process = false;
	hw_device->cur_hw_ctx = hw_ctx;
	hw_device->req_id = frame_req->request_id;
	mutex_unlock(&hw_device->lock);

	rc = cam_fd_mgr_util_put_frame_req(
		&hw_mgr->frame_processing_list, &frame_req);
	if (rc) {
		CAM_ERR(CAM_FD,
			"Failed in putting frame req in processing list");
		goto stop_unlock;
	}

	return rc;

stop_unlock:
	if (hw_device->hw_intf->hw_ops.stop) {
		struct cam_fd_hw_stop_args stop_args;

		stop_args.hw_ctx = hw_ctx;
		stop_args.ctx_hw_private = hw_ctx->ctx_hw_private;
		stop_args.hw_req_private = &frame_req->hw_req_private;
		if (hw_device->hw_intf->hw_ops.stop(
			hw_device->hw_intf->hw_priv, &stop_args,
			sizeof(stop_args)))
			CAM_ERR(CAM_FD, "Failed in HW Stop %d", rc);
	}
put_req_into_free_list:
	cam_fd_mgr_util_put_frame_req(&hw_mgr->frame_free_list, &frame_req);

	return rc;
}

static int cam_fd_mgr_util_schedule_frame_worker_task(
	struct cam_fd_hw_mgr *hw_mgr)
{
	int32_t rc = 0;
	struct crm_workq_task *task;
	struct cam_fd_mgr_work_data *work_data;

	task = cam_req_mgr_workq_get_task(hw_mgr->work);
	if (!task) {
		CAM_ERR(CAM_FD, "no empty task available");
		return -ENOMEM;
	}

	work_data = (struct cam_fd_mgr_work_data *)task->payload;
	work_data->type = CAM_FD_WORK_FRAME;

	task->process_cb = cam_fd_mgr_util_submit_frame;
	rc = cam_req_mgr_workq_enqueue_task(task, hw_mgr, CRM_TASK_PRIORITY_0);

	return rc;
}

static int32_t cam_fd_mgr_workq_irq_cb(void *priv, void *data)
{
	struct cam_fd_device *hw_device = NULL;
	struct cam_fd_hw_mgr *hw_mgr;
	struct cam_fd_mgr_work_data *work_data;
	struct cam_fd_mgr_frame_request *frame_req = NULL;
	enum cam_fd_hw_irq_type irq_type;
	bool frame_abort = true;
	int rc;

	if (!data || !priv) {
		CAM_ERR(CAM_FD, "Invalid data %pK %pK", data, priv);
		return -EINVAL;
	}

	hw_mgr = (struct cam_fd_hw_mgr *)priv;
	work_data = (struct cam_fd_mgr_work_data *)data;
	irq_type = work_data->irq_type;

	CAM_DBG(CAM_FD, "FD IRQ type=%d", irq_type);

	if (irq_type == CAM_FD_IRQ_HALT_DONE) {
		/* HALT would be followed by a RESET, ignore this */
		CAM_DBG(CAM_FD, "HALT IRQ callback");
		return 0;
	}

	/* Get the frame from processing list */
	rc = cam_fd_mgr_util_get_frame_req(&hw_mgr->frame_processing_list,
		&frame_req);
	if (rc || !frame_req) {
		/*
		 * This can happen if reset is triggered while no frames
		 * were pending, so not an error, just continue to check if
		 * there are any pending frames and submit
		 */
		CAM_DBG(CAM_FD, "No Frame in processing list, rc=%d", rc);
		goto submit_next_frame;
	}

	if (!frame_req->hw_ctx) {
		CAM_ERR(CAM_FD, "Invalid Frame request %lld",
			frame_req->request_id);
		goto put_req_in_free_list;
	}

	rc = cam_fd_mgr_util_get_device(hw_mgr, frame_req->hw_ctx, &hw_device);
	if (rc) {
		CAM_ERR(CAM_FD, "Error in getting device %d", rc);
		goto put_req_in_free_list;
	}

	/* Read frame results first */
	if (irq_type == CAM_FD_IRQ_FRAME_DONE) {
		struct cam_fd_hw_frame_done_args frame_done_args;

		CAM_DBG(CAM_FD, "FrameDone : Frame[%lld]",
			frame_req->request_id);

		frame_done_args.hw_ctx = frame_req->hw_ctx;
		frame_done_args.ctx_hw_private =
			frame_req->hw_ctx->ctx_hw_private;
		frame_done_args.request_id = frame_req->request_id;
		frame_done_args.hw_req_private = &frame_req->hw_req_private;

		if (hw_device->hw_intf->hw_ops.process_cmd) {
			rc = hw_device->hw_intf->hw_ops.process_cmd(
				hw_device->hw_intf->hw_priv,
				CAM_FD_HW_CMD_FRAME_DONE,
				&frame_done_args, sizeof(frame_done_args));
			if (rc) {
				CAM_ERR(CAM_FD, "Failed in CMD_PRESTART %d",
					rc);
				frame_abort = true;
				goto notify_context;
			}
		}

		frame_abort = false;
	}

	trace_cam_irq_handled("FD", irq_type);

notify_context:
	/* Do a callback to inform frame done or stop done */
	if (frame_req->hw_ctx->event_cb) {
		struct cam_hw_done_event_data buf_data;

		CAM_DBG(CAM_FD, "FrameHALT : Frame[%lld]",
			frame_req->request_id);

		buf_data.num_handles = frame_req->num_hw_update_entries;
		buf_data.request_id = frame_req->request_id;

		rc = frame_req->hw_ctx->event_cb(frame_req->hw_ctx->cb_priv,
			frame_abort, &buf_data);
		if (rc)
			CAM_ERR(CAM_FD, "Error in event cb handling %d", rc);
	}

	/*
	 * Now we can set hw device is free to process further frames.
	 * Note - Do not change state to IDLE until we read the frame results,
	 * Otherwise, other thread may schedule frame processing before
	 * reading current frame's results. Also, we need to set to IDLE state
	 * in case some error happens after getting this irq callback
	 */
	mutex_lock(&hw_device->lock);
	hw_device->ready_to_process = true;
	hw_device->req_id = -1;
	hw_device->cur_hw_ctx = NULL;
	CAM_DBG(CAM_FD, "ready_to_process=%d", hw_device->ready_to_process);
	mutex_unlock(&hw_device->lock);

put_req_in_free_list:
	rc = cam_fd_mgr_util_put_frame_req(&hw_mgr->frame_free_list,
		&frame_req);
	if (rc) {
		CAM_ERR(CAM_FD, "Failed in putting frame req in free list");
		/* continue */
	}

submit_next_frame:
	/* Check if there are any frames pending for processing and submit */
	rc = cam_fd_mgr_util_submit_frame(hw_mgr, NULL);
	if (rc) {
		CAM_ERR(CAM_FD, "Error while submit frame, rc=%d", rc);
		return rc;
	}

	return rc;
}

static int cam_fd_mgr_irq_cb(void *data, enum cam_fd_hw_irq_type irq_type)
{
	struct cam_fd_hw_mgr *hw_mgr = &g_fd_hw_mgr;
	int rc = 0;
	unsigned long flags;
	struct crm_workq_task *task;
	struct cam_fd_mgr_work_data *work_data;

	spin_lock_irqsave(&hw_mgr->hw_mgr_slock, flags);
	task = cam_req_mgr_workq_get_task(hw_mgr->work);
	if (!task) {
		CAM_ERR(CAM_FD, "no empty task available");
		spin_unlock_irqrestore(&hw_mgr->hw_mgr_slock, flags);
		return -ENOMEM;
	}

	work_data = (struct cam_fd_mgr_work_data *)task->payload;
	work_data->type = CAM_FD_WORK_IRQ;
	work_data->irq_type = irq_type;

	task->process_cb = cam_fd_mgr_workq_irq_cb;
	rc = cam_req_mgr_workq_enqueue_task(task, hw_mgr, CRM_TASK_PRIORITY_0);
	if (rc)
		CAM_ERR(CAM_FD, "Failed in enqueue work task, rc=%d", rc);

	spin_unlock_irqrestore(&hw_mgr->hw_mgr_slock, flags);

	return rc;
}

static int cam_fd_mgr_hw_get_caps(void *hw_mgr_priv, void *hw_get_caps_args)
{
	int rc = 0;
	struct cam_fd_hw_mgr *hw_mgr = hw_mgr_priv;
	struct cam_query_cap_cmd *query = hw_get_caps_args;
	struct cam_fd_query_cap_cmd query_fd;

	if (copy_from_user(&query_fd, (void __user *)query->caps_handle,
		sizeof(struct cam_fd_query_cap_cmd))) {
		CAM_ERR(CAM_FD, "Failed in copy from user, rc=%d", rc);
		return -EFAULT;
	}

	query_fd  = hw_mgr->fd_caps;

	CAM_DBG(CAM_FD,
		"IOMMU device(%d, %d), CDM(%d, %d), versions %d.%d, %d.%d",
		query_fd.device_iommu.secure, query_fd.device_iommu.non_secure,
		query_fd.cdm_iommu.secure, query_fd.cdm_iommu.non_secure,
		query_fd.hw_caps.core_version.major,
		query_fd.hw_caps.core_version.minor,
		query_fd.hw_caps.wrapper_version.major,
		query_fd.hw_caps.wrapper_version.minor);

	if (copy_to_user((void __user *)query->caps_handle, &query_fd,
		sizeof(struct cam_fd_query_cap_cmd)))
		rc = -EFAULT;

	return rc;
}

static int cam_fd_mgr_hw_acquire(void *hw_mgr_priv, void *hw_acquire_args)
{
	struct cam_fd_hw_mgr *hw_mgr = (struct cam_fd_hw_mgr *)hw_mgr_priv;
	struct cam_hw_acquire_args *acquire_args =
		(struct cam_hw_acquire_args *)hw_acquire_args;
	struct cam_fd_hw_mgr_ctx *hw_ctx;
	struct cam_fd_acquire_dev_info fd_acquire_args;
	int rc;

	if (!acquire_args || acquire_args->num_acq <= 0) {
		CAM_ERR(CAM_FD, "Invalid acquire args %pK", acquire_args);
		return -EINVAL;
	}

	if (copy_from_user(&fd_acquire_args,
		(void __user *)acquire_args->acquire_info,
		sizeof(struct cam_fd_acquire_dev_info))) {
		CAM_ERR(CAM_FD, "Copy from user failed");
		return -EFAULT;
	}

	CAM_DBG(CAM_FD, "Acquire : mode=%d, get_raw_results=%d, priority=%d",
		fd_acquire_args.mode, fd_acquire_args.get_raw_results,
		fd_acquire_args.priority);

	/* get a free fd hw mgr ctx */
	rc = cam_fd_mgr_util_get_ctx(&hw_mgr->free_ctx_list, &hw_ctx);
	if (rc || !hw_ctx) {
		CAM_ERR(CAM_FD, "Get hw context failed, rc=%d, hw_ctx=%pK",
			rc, hw_ctx);
		return -EINVAL;
	}

	if (fd_acquire_args.get_raw_results && !hw_mgr->raw_results_available) {
		CAM_ERR(CAM_FD, "HW cannot support raw results %d (%d)",
			fd_acquire_args.get_raw_results,
			hw_mgr->raw_results_available);
		goto put_ctx;
	}

	if (!(fd_acquire_args.mode & hw_mgr->supported_modes)) {
		CAM_ERR(CAM_FD, "HW cannot support requested mode 0x%x (0x%x)",
			fd_acquire_args.mode, hw_mgr->supported_modes);
		rc = -EPERM;
		goto put_ctx;
	}

	rc = cam_fd_mgr_util_select_device(hw_mgr, hw_ctx, &fd_acquire_args);
	if (rc) {
		CAM_ERR(CAM_FD, "Failed in selecting device, rc=%d", rc);
		goto put_ctx;
	}

	hw_ctx->ctx_in_use = true;
	hw_ctx->hw_mgr = hw_mgr;
	hw_ctx->get_raw_results = fd_acquire_args.get_raw_results;
	hw_ctx->mode = fd_acquire_args.mode;

	/* Save incoming cam core info into hw ctx*/
	hw_ctx->cb_priv = acquire_args->context_data;
	hw_ctx->event_cb = acquire_args->event_cb;

	/* Update out args */
	acquire_args->ctxt_to_hw_map = hw_ctx;

	cam_fd_mgr_util_put_ctx(&hw_mgr->used_ctx_list, &hw_ctx);

	return 0;
put_ctx:
	list_del_init(&hw_ctx->list);
	cam_fd_mgr_util_put_ctx(&hw_mgr->free_ctx_list, &hw_ctx);
	return rc;
}

static int cam_fd_mgr_hw_release(void *hw_mgr_priv, void *hw_release_args)
{
	struct cam_fd_hw_mgr *hw_mgr = (struct cam_fd_hw_mgr *)hw_mgr_priv;
	struct cam_hw_release_args *release_args = hw_release_args;
	struct cam_fd_hw_mgr_ctx *hw_ctx;
	int rc;

	if (!hw_mgr_priv || !hw_release_args) {
		CAM_ERR(CAM_FD, "Invalid arguments %pK, %pK",
			hw_mgr_priv, hw_release_args);
		return -EINVAL;
	}

	hw_ctx = (struct cam_fd_hw_mgr_ctx *)release_args->ctxt_to_hw_map;
	if (!hw_ctx || !hw_ctx->ctx_in_use) {
		CAM_ERR(CAM_FD, "Invalid context is used, hw_ctx=%pK", hw_ctx);
		return -EPERM;
	}

	rc = cam_fd_mgr_util_release_device(hw_mgr, hw_ctx);
	if (rc)
		CAM_ERR(CAM_FD, "Failed in release device, rc=%d", rc);

	hw_ctx->ctx_in_use = false;
	list_del_init(&hw_ctx->list);
	cam_fd_mgr_util_put_ctx(&hw_mgr->free_ctx_list, &hw_ctx);

	return 0;
}

static int cam_fd_mgr_hw_start(void *hw_mgr_priv, void *mgr_start_args)
{
	int rc = 0;
	struct cam_fd_hw_mgr *hw_mgr = (struct cam_fd_hw_mgr *)hw_mgr_priv;
	struct cam_hw_start_args *hw_mgr_start_args =
		(struct cam_hw_start_args *)mgr_start_args;
	struct cam_fd_hw_mgr_ctx *hw_ctx;
	struct cam_fd_device *hw_device;
	struct cam_fd_hw_init_args hw_init_args;

	if (!hw_mgr_priv || !hw_mgr_start_args) {
		CAM_ERR(CAM_FD, "Invalid arguments %pK %pK",
			hw_mgr_priv, hw_mgr_start_args);
		return -EINVAL;
	}

	hw_ctx = (struct cam_fd_hw_mgr_ctx *)hw_mgr_start_args->ctxt_to_hw_map;
	if (!hw_ctx || !hw_ctx->ctx_in_use) {
		CAM_ERR(CAM_FD, "Invalid context is used, hw_ctx=%pK", hw_ctx);
		return -EPERM;
	}

	CAM_DBG(CAM_FD, "ctx index=%u, device_index=%d", hw_ctx->ctx_index,
		hw_ctx->device_index);

	rc = cam_fd_mgr_util_get_device(hw_mgr, hw_ctx, &hw_device);
	if (rc) {
		CAM_ERR(CAM_FD, "Error in getting device %d", rc);
		return rc;
	}

	if (hw_device->hw_intf->hw_ops.init) {
		hw_init_args.hw_ctx = hw_ctx;
		hw_init_args.ctx_hw_private = hw_ctx->ctx_hw_private;
		rc = hw_device->hw_intf->hw_ops.init(
			hw_device->hw_intf->hw_priv, &hw_init_args,
			sizeof(hw_init_args));
		if (rc) {
			CAM_ERR(CAM_FD, "Failed in HW Init %d", rc);
			return rc;
		}
	} else {
		CAM_ERR(CAM_FD, "Invalid init function");
		return -EINVAL;
	}

	return rc;
}

static int cam_fd_mgr_hw_flush_req(void *hw_mgr_priv,
	struct cam_hw_flush_args *flush_args)
{
	int rc = 0;
	struct cam_fd_mgr_frame_request *frame_req, *req_temp, *flush_req;
	struct cam_fd_hw_mgr *hw_mgr = (struct cam_fd_hw_mgr *)hw_mgr_priv;
	struct cam_fd_device *hw_device;
	struct cam_fd_hw_stop_args hw_stop_args;
	struct cam_fd_hw_mgr_ctx *hw_ctx;
	uint32_t i = 0;

	hw_ctx = (struct cam_fd_hw_mgr_ctx *)flush_args->ctxt_to_hw_map;

	if (!hw_ctx || !hw_ctx->ctx_in_use) {
		CAM_ERR(CAM_FD, "Invalid context is used, hw_ctx=%pK", hw_ctx);
		return -EPERM;
	}
	CAM_DBG(CAM_FD, "ctx index=%u, hw_ctx=%d", hw_ctx->ctx_index,
		hw_ctx->device_index);

	rc = cam_fd_mgr_util_get_device(hw_mgr, hw_ctx, &hw_device);
	if (rc) {
		CAM_ERR(CAM_FD, "Error in getting device %d", rc);
		return rc;
	}

	mutex_lock(&hw_mgr->frame_req_mutex);
	for (i = 0; i < flush_args->num_req_active; i++) {
		flush_req = (struct cam_fd_mgr_frame_request *)
			flush_args->flush_req_active[i];

		list_for_each_entry_safe(frame_req, req_temp,
			&hw_mgr->frame_pending_list_high, list) {
			if (frame_req->hw_ctx != hw_ctx)
				continue;

			if (frame_req->request_id != flush_req->request_id)
				continue;

			list_del_init(&frame_req->list);
			break;
		}

		list_for_each_entry_safe(frame_req, req_temp,
			&hw_mgr->frame_pending_list_normal, list) {
			if (frame_req->hw_ctx != hw_ctx)
				continue;

			if (frame_req->request_id != flush_req->request_id)
				continue;

			list_del_init(&frame_req->list);
			break;
		}

		list_for_each_entry_safe(frame_req, req_temp,
			&hw_mgr->frame_processing_list, list) {
			if (frame_req->hw_ctx != hw_ctx)
				continue;

			if (frame_req->request_id != flush_req->request_id)
				continue;

			list_del_init(&frame_req->list);

			mutex_lock(&hw_device->lock);
			if ((hw_device->ready_to_process == true) ||
				(hw_device->cur_hw_ctx != hw_ctx))
				goto unlock_dev_flush_req;

			if (hw_device->hw_intf->hw_ops.stop) {
				hw_stop_args.hw_ctx = hw_ctx;
				rc = hw_device->hw_intf->hw_ops.stop(
					hw_device->hw_intf->hw_priv,
					&hw_stop_args,
					sizeof(hw_stop_args));
				if (rc) {
					CAM_ERR(CAM_FD,
						"Failed in HW Stop %d", rc);
					goto unlock_dev_flush_req;
				}
				hw_device->ready_to_process = true;
			}

unlock_dev_flush_req:
			mutex_unlock(&hw_device->lock);
			break;
		}
	}
	mutex_unlock(&hw_mgr->frame_req_mutex);

	for (i = 0; i < flush_args->num_req_pending; i++) {
		flush_req = (struct cam_fd_mgr_frame_request *)
			flush_args->flush_req_pending[i];
		cam_fd_mgr_util_put_frame_req(&hw_mgr->frame_free_list,
			&flush_req);
	}

	return rc;
}

static int cam_fd_mgr_hw_flush_ctx(void *hw_mgr_priv,
	struct cam_hw_flush_args *flush_args)
{
	int rc = 0;
	struct cam_fd_mgr_frame_request *frame_req, *req_temp, *flush_req;
	struct cam_fd_hw_mgr *hw_mgr = (struct cam_fd_hw_mgr *)hw_mgr_priv;
	struct cam_fd_device *hw_device;
	struct cam_fd_hw_stop_args hw_stop_args;
	struct cam_fd_hw_mgr_ctx *hw_ctx;
	uint32_t i = 0;

	hw_ctx = (struct cam_fd_hw_mgr_ctx *)flush_args->ctxt_to_hw_map;

	if (!hw_ctx || !hw_ctx->ctx_in_use) {
		CAM_ERR(CAM_FD, "Invalid context is used, hw_ctx=%pK", hw_ctx);
		return -EPERM;
	}
	CAM_DBG(CAM_FD, "ctx index=%u, hw_ctx=%d", hw_ctx->ctx_index,
		hw_ctx->device_index);

	rc = cam_fd_mgr_util_get_device(hw_mgr, hw_ctx, &hw_device);
	if (rc) {
		CAM_ERR(CAM_FD, "Error in getting device %d", rc);
		return rc;
	}

	mutex_lock(&hw_mgr->frame_req_mutex);
	list_for_each_entry_safe(frame_req, req_temp,
		&hw_mgr->frame_pending_list_high, list) {
		if (frame_req->hw_ctx != hw_ctx)
			continue;

		list_del_init(&frame_req->list);
	}

	list_for_each_entry_safe(frame_req, req_temp,
		&hw_mgr->frame_pending_list_normal, list) {
		if (frame_req->hw_ctx != hw_ctx)
			continue;

		list_del_init(&frame_req->list);
	}

	list_for_each_entry_safe(frame_req, req_temp,
		&hw_mgr->frame_processing_list, list) {
		if (frame_req->hw_ctx != hw_ctx)
			continue;

		list_del_init(&frame_req->list);
		mutex_lock(&hw_device->lock);
		if ((hw_device->ready_to_process == true) ||
			(hw_device->cur_hw_ctx != hw_ctx))
			goto unlock_dev_flush_ctx;

		if (hw_device->hw_intf->hw_ops.stop) {
			hw_stop_args.hw_ctx = hw_ctx;
			rc = hw_device->hw_intf->hw_ops.stop(
				hw_device->hw_intf->hw_priv, &hw_stop_args,
				sizeof(hw_stop_args));
			if (rc) {
				CAM_ERR(CAM_FD, "Failed in HW Stop %d", rc);
				goto unlock_dev_flush_ctx;
			}
			hw_device->ready_to_process = true;
		}

unlock_dev_flush_ctx:
	mutex_unlock(&hw_device->lock);
	}
	mutex_unlock(&hw_mgr->frame_req_mutex);

	for (i = 0; i < flush_args->num_req_pending; i++) {
		flush_req = (struct cam_fd_mgr_frame_request *)
			flush_args->flush_req_pending[i];
		cam_fd_mgr_util_put_frame_req(&hw_mgr->frame_free_list,
			&flush_req);
	}

	return rc;
}

static int cam_fd_mgr_hw_flush(void *hw_mgr_priv,
	void *hw_flush_args)
{
	int rc = 0;
	struct cam_hw_flush_args *flush_args =
		(struct cam_hw_flush_args *)hw_flush_args;

	if (!hw_mgr_priv || !hw_flush_args) {
		CAM_ERR(CAM_FD, "Invalid arguments %pK %pK",
			hw_mgr_priv, hw_flush_args);
		return -EINVAL;
	}

	switch (flush_args->flush_type) {
	case CAM_FLUSH_TYPE_REQ:
		rc = cam_fd_mgr_hw_flush_req(hw_mgr_priv, flush_args);
		break;
	case CAM_FLUSH_TYPE_ALL:
		rc = cam_fd_mgr_hw_flush_ctx(hw_mgr_priv, flush_args);
		break;
	default:
		rc = -EINVAL;
		CAM_ERR(CAM_FD, "Invalid flush type %d",
			flush_args->flush_type);
		break;
	}
	return rc;
}

static int cam_fd_mgr_hw_stop(void *hw_mgr_priv, void *mgr_stop_args)
{
	struct cam_fd_hw_mgr *hw_mgr = (struct cam_fd_hw_mgr *)hw_mgr_priv;
	struct cam_hw_stop_args *hw_mgr_stop_args =
		(struct cam_hw_stop_args *)mgr_stop_args;
	struct cam_fd_hw_mgr_ctx *hw_ctx;
	struct cam_fd_device *hw_device;
	struct cam_fd_hw_deinit_args hw_deinit_args;
	int rc = 0;

	if (!hw_mgr_priv || !hw_mgr_stop_args) {
		CAM_ERR(CAM_FD, "Invalid arguments %pK %pK",
			hw_mgr_priv, hw_mgr_stop_args);
		return -EINVAL;
	}

	hw_ctx = (struct cam_fd_hw_mgr_ctx *)hw_mgr_stop_args->ctxt_to_hw_map;
	if (!hw_ctx || !hw_ctx->ctx_in_use) {
		CAM_ERR(CAM_FD, "Invalid context is used, hw_ctx=%pK", hw_ctx);
		return -EPERM;
	}
	CAM_DBG(CAM_FD, "ctx index=%u, hw_ctx=%d", hw_ctx->ctx_index,
		hw_ctx->device_index);

	rc = cam_fd_mgr_util_get_device(hw_mgr, hw_ctx, &hw_device);
	if (rc) {
		CAM_ERR(CAM_FD, "Error in getting device %d", rc);
		return rc;
	}

	CAM_DBG(CAM_FD, "FD Device ready_to_process = %d",
		hw_device->ready_to_process);

	if (hw_device->hw_intf->hw_ops.deinit) {
		hw_deinit_args.hw_ctx = hw_ctx;
		hw_deinit_args.ctx_hw_private = hw_ctx->ctx_hw_private;
		rc = hw_device->hw_intf->hw_ops.deinit(
			hw_device->hw_intf->hw_priv, &hw_deinit_args,
			sizeof(hw_deinit_args));
		if (rc) {
			CAM_ERR(CAM_FD, "Failed in HW DeInit %d", rc);
			return rc;
		}
	}

	return rc;
}

static int cam_fd_mgr_hw_prepare_update(void *hw_mgr_priv,
	void *hw_prepare_update_args)
{
	struct cam_fd_hw_mgr *hw_mgr = (struct cam_fd_hw_mgr *)hw_mgr_priv;
	struct cam_hw_prepare_update_args *prepare =
		(struct cam_hw_prepare_update_args *) hw_prepare_update_args;
	struct cam_fd_hw_mgr_ctx *hw_ctx;
	struct cam_fd_device *hw_device;
	struct cam_kmd_buf_info kmd_buf;
	int rc;
	struct cam_fd_hw_cmd_prestart_args prestart_args;
	struct cam_fd_mgr_frame_request *frame_req;

	if (!hw_mgr_priv || !hw_prepare_update_args) {
		CAM_ERR(CAM_FD, "Invalid args %pK %pK",
			hw_mgr_priv, hw_prepare_update_args);
		return -EINVAL;
	}

	hw_ctx = (struct cam_fd_hw_mgr_ctx *)prepare->ctxt_to_hw_map;
	if (!hw_ctx || !hw_ctx->ctx_in_use) {
		CAM_ERR(CAM_FD, "Invalid context is used, hw_ctx=%pK", hw_ctx);
		return -EPERM;
	}

	rc = cam_fd_mgr_util_get_device(hw_mgr, hw_ctx, &hw_device);
	if (rc) {
		CAM_ERR(CAM_FD, "Error in getting device %d", rc);
		goto error;
	}

	rc = cam_fd_mgr_util_packet_validate(prepare->packet);
	if (rc) {
		CAM_ERR(CAM_FD, "Error in packet validation %d", rc);
		goto error;
	}

	rc = cam_packet_util_get_kmd_buffer(prepare->packet, &kmd_buf);
	if (rc) {
		CAM_ERR(CAM_FD, "Error in get kmd buf buffer %d", rc);
		goto error;
	}

	CAM_DBG(CAM_FD,
		"KMD Buf : hdl=%d, cpu_addr=%pK, offset=%d, size=%d, used=%d",
		kmd_buf.handle, kmd_buf.cpu_addr, kmd_buf.offset,
		kmd_buf.size, kmd_buf.used_bytes);

	/* We do not expect any patching, but just do it anyway */
	rc = cam_packet_util_process_patches(prepare->packet,
		hw_mgr->device_iommu.non_secure, -1);
	if (rc) {
		CAM_ERR(CAM_FD, "Patch FD packet failed, rc=%d", rc);
		return rc;
	}

	memset(&prestart_args, 0x0, sizeof(prestart_args));
	prestart_args.ctx_hw_private = hw_ctx->ctx_hw_private;
	prestart_args.hw_ctx = hw_ctx;
	prestart_args.request_id = prepare->packet->header.request_id;

	rc = cam_fd_mgr_util_parse_generic_cmd_buffer(hw_ctx, prepare->packet,
		&prestart_args);
	if (rc) {
		CAM_ERR(CAM_FD, "Error in parsing gerneric cmd buffer %d", rc);
		goto error;
	}

	rc = cam_fd_mgr_util_prepare_io_buf_info(
		hw_mgr->device_iommu.non_secure, prepare,
		prestart_args.input_buf, prestart_args.output_buf,
		CAM_FD_MAX_IO_BUFFERS);
	if (rc) {
		CAM_ERR(CAM_FD, "Error in prepare IO Buf %d", rc);
		goto error;
	}

	rc = cam_fd_mgr_util_prepare_hw_update_entries(hw_mgr, prepare,
		&prestart_args, &kmd_buf);
	if (rc) {
		CAM_ERR(CAM_FD, "Error in hw update entries %d", rc);
		goto error;
	}

	/* get a free frame req from free list */
	rc = cam_fd_mgr_util_get_frame_req(&hw_mgr->frame_free_list,
		&frame_req);
	if (rc || !frame_req) {
		CAM_ERR(CAM_FD, "Get frame_req failed, rc=%d, hw_ctx=%pK",
			rc, hw_ctx);
		return -ENOMEM;
	}

	/* Setup frame request info and queue to pending list */
	frame_req->hw_ctx = hw_ctx;
	frame_req->request_id = prepare->packet->header.request_id;
	/* This has to be passed to HW while calling hw_ops->start */
	frame_req->hw_req_private = prestart_args.hw_req_private;

	/*
	 * Save the current frame_req into priv,
	 * this will come as priv while hw_config
	 */
	prepare->priv = frame_req;

	CAM_DBG(CAM_FD, "FramePrepare : Frame[%lld]", frame_req->request_id);

	return 0;
error:
	return rc;
}

static int cam_fd_mgr_hw_config(void *hw_mgr_priv, void *hw_config_args)
{
	struct cam_fd_hw_mgr *hw_mgr = (struct cam_fd_hw_mgr *)hw_mgr_priv;
	struct cam_hw_config_args *config =
		(struct cam_hw_config_args *) hw_config_args;
	struct cam_fd_hw_mgr_ctx *hw_ctx;
	struct cam_fd_mgr_frame_request *frame_req;
	int rc;
	int i;

	if (!hw_mgr || !config) {
		CAM_ERR(CAM_FD, "Invalid arguments %pK %pK", hw_mgr, config);
		return -EINVAL;
	}

	if (!config->num_hw_update_entries) {
		CAM_ERR(CAM_FD, "No hw update enteries are available");
		return -EINVAL;
	}

	hw_ctx = (struct cam_fd_hw_mgr_ctx *)config->ctxt_to_hw_map;
	if (!hw_ctx || !hw_ctx->ctx_in_use) {
		CAM_ERR(CAM_FD, "Invalid context is used, hw_ctx=%pK", hw_ctx);
		return -EPERM;
	}

	frame_req = config->priv;

	trace_cam_apply_req("FD", frame_req->request_id);
	CAM_DBG(CAM_FD, "FrameHWConfig : Frame[%lld]", frame_req->request_id);

	frame_req->num_hw_update_entries = config->num_hw_update_entries;
	for (i = 0; i < config->num_hw_update_entries; i++) {
		frame_req->hw_update_entries[i] = config->hw_update_entries[i];
		CAM_DBG(CAM_FD, "PreStart HWEntry[%d] : %d %d %d %d %pK",
			frame_req->hw_update_entries[i].handle,
			frame_req->hw_update_entries[i].offset,
			frame_req->hw_update_entries[i].len,
			frame_req->hw_update_entries[i].flags,
			frame_req->hw_update_entries[i].addr);
	}

	if (hw_ctx->priority == CAM_FD_PRIORITY_HIGH) {
		CAM_DBG(CAM_FD, "Insert frame into prio0 queue");
		rc = cam_fd_mgr_util_put_frame_req(
			&hw_mgr->frame_pending_list_high, &frame_req);
	} else {
		CAM_DBG(CAM_FD, "Insert frame into prio1 queue");
		rc = cam_fd_mgr_util_put_frame_req(
			&hw_mgr->frame_pending_list_normal, &frame_req);
	}
	if (rc) {
		CAM_ERR(CAM_FD, "Failed in queuing frame req, rc=%d", rc);
		goto put_free_list;
	}

	rc = cam_fd_mgr_util_schedule_frame_worker_task(hw_mgr);
	if (rc) {
		CAM_ERR(CAM_FD, "Worker task scheduling failed %d", rc);
		goto remove_and_put_free_list;
	}

	return 0;

remove_and_put_free_list:

	if (hw_ctx->priority == CAM_FD_PRIORITY_HIGH) {
		CAM_DBG(CAM_FD, "Removing frame into prio0 queue");
		cam_fd_mgr_util_get_frame_req(
			&hw_mgr->frame_pending_list_high, &frame_req);
	} else {
		CAM_DBG(CAM_FD, "Removing frame into prio1 queue");
		cam_fd_mgr_util_get_frame_req(
			&hw_mgr->frame_pending_list_normal, &frame_req);
	}
put_free_list:
	cam_fd_mgr_util_put_frame_req(&hw_mgr->frame_free_list,
		&frame_req);

	return rc;
}

int cam_fd_hw_mgr_deinit(struct device_node *of_node)
{
	CAM_DBG(CAM_FD, "HW Mgr Deinit");

	cam_req_mgr_workq_destroy(&g_fd_hw_mgr.work);

	cam_smmu_ops(g_fd_hw_mgr.device_iommu.non_secure, CAM_SMMU_DETACH);
	cam_smmu_destroy_handle(g_fd_hw_mgr.device_iommu.non_secure);
	g_fd_hw_mgr.device_iommu.non_secure = -1;

	mutex_destroy(&g_fd_hw_mgr.ctx_mutex);
	mutex_destroy(&g_fd_hw_mgr.frame_req_mutex);
	mutex_destroy(&g_fd_hw_mgr.hw_mgr_mutex);

	return 0;
}

int cam_fd_hw_mgr_init(struct device_node *of_node,
	struct cam_hw_mgr_intf *hw_mgr_intf)
{
	int count, i, rc = 0;
	struct cam_hw_intf *hw_intf = NULL;
	struct cam_fd_hw_mgr_ctx *hw_mgr_ctx;
	struct cam_fd_device *hw_device;
	struct cam_fd_mgr_frame_request *frame_req;

	if (!of_node || !hw_mgr_intf) {
		CAM_ERR(CAM_FD, "Invalid args of_node %pK hw_mgr_intf %pK",
			of_node, hw_mgr_intf);
		return -EINVAL;
	}

	memset(&g_fd_hw_mgr, 0x0, sizeof(g_fd_hw_mgr));
	memset(hw_mgr_intf, 0x0, sizeof(*hw_mgr_intf));

	mutex_init(&g_fd_hw_mgr.ctx_mutex);
	mutex_init(&g_fd_hw_mgr.frame_req_mutex);
	mutex_init(&g_fd_hw_mgr.hw_mgr_mutex);
	spin_lock_init(&g_fd_hw_mgr.hw_mgr_slock);

	count = of_property_count_strings(of_node, "compat-hw-name");
	if (!count || (count > CAM_FD_HW_MAX)) {
		CAM_ERR(CAM_FD, "Invalid compat names in dev tree %d", count);
		return -EINVAL;
	}
	g_fd_hw_mgr.num_devices = count;

	g_fd_hw_mgr.raw_results_available = false;
	g_fd_hw_mgr.supported_modes = 0;

	for (i = 0; i < count; i++) {
		hw_device = &g_fd_hw_mgr.hw_device[i];

		rc = cam_fd_mgr_util_pdev_get_hw_intf(of_node, i, &hw_intf);
		if (rc) {
			CAM_ERR(CAM_FD, "hw intf from pdev failed, rc=%d", rc);
			return rc;
		}

		mutex_init(&hw_device->lock);

		hw_device->valid = true;
		hw_device->hw_intf = hw_intf;
		hw_device->ready_to_process = true;

		if (hw_device->hw_intf->hw_ops.process_cmd) {
			struct cam_fd_hw_cmd_set_irq_cb irq_cb_args;

			irq_cb_args.cam_fd_hw_mgr_cb = cam_fd_mgr_irq_cb;
			irq_cb_args.data = hw_device;

			rc = hw_device->hw_intf->hw_ops.process_cmd(
				hw_device->hw_intf->hw_priv,
				CAM_FD_HW_CMD_REGISTER_CALLBACK,
				&irq_cb_args, sizeof(irq_cb_args));
			if (rc) {
				CAM_ERR(CAM_FD,
					"Failed in REGISTER_CALLBACK %d", rc);
				return rc;
			}
		}

		if (hw_device->hw_intf->hw_ops.get_hw_caps) {
			rc = hw_device->hw_intf->hw_ops.get_hw_caps(
				hw_intf->hw_priv, &hw_device->hw_caps,
				sizeof(hw_device->hw_caps));
			if (rc) {
				CAM_ERR(CAM_FD, "Failed in get_hw_caps %d", rc);
				return rc;
			}

			g_fd_hw_mgr.raw_results_available |=
				hw_device->hw_caps.raw_results_available;
			g_fd_hw_mgr.supported_modes |=
				hw_device->hw_caps.supported_modes;

			CAM_DBG(CAM_FD,
				"Device[mode=%d, raw=%d], Mgr[mode=%d, raw=%d]",
				hw_device->hw_caps.supported_modes,
				hw_device->hw_caps.raw_results_available,
				g_fd_hw_mgr.supported_modes,
				g_fd_hw_mgr.raw_results_available);
		}
	}

	INIT_LIST_HEAD(&g_fd_hw_mgr.free_ctx_list);
	INIT_LIST_HEAD(&g_fd_hw_mgr.used_ctx_list);
	INIT_LIST_HEAD(&g_fd_hw_mgr.frame_free_list);
	INIT_LIST_HEAD(&g_fd_hw_mgr.frame_pending_list_high);
	INIT_LIST_HEAD(&g_fd_hw_mgr.frame_pending_list_normal);
	INIT_LIST_HEAD(&g_fd_hw_mgr.frame_processing_list);

	g_fd_hw_mgr.device_iommu.non_secure = -1;
	g_fd_hw_mgr.device_iommu.secure = -1;
	g_fd_hw_mgr.cdm_iommu.non_secure = -1;
	g_fd_hw_mgr.cdm_iommu.secure = -1;

	rc = cam_smmu_get_handle("fd",
		&g_fd_hw_mgr.device_iommu.non_secure);
	if (rc) {
		CAM_ERR(CAM_FD, "Get iommu handle failed, rc=%d", rc);
		goto destroy_mutex;
	}

	rc = cam_smmu_ops(g_fd_hw_mgr.device_iommu.non_secure, CAM_SMMU_ATTACH);
	if (rc) {
		CAM_ERR(CAM_FD, "FD attach iommu handle failed, rc=%d", rc);
		goto destroy_smmu;
	}

	rc = cam_cdm_get_iommu_handle("fd", &g_fd_hw_mgr.cdm_iommu);
	if (rc)
		CAM_DBG(CAM_FD, "Failed to acquire the CDM iommu handles");

	CAM_DBG(CAM_FD, "iommu handles : device(%d, %d), cdm(%d, %d)",
		g_fd_hw_mgr.device_iommu.non_secure,
		g_fd_hw_mgr.device_iommu.secure,
		g_fd_hw_mgr.cdm_iommu.non_secure,
		g_fd_hw_mgr.cdm_iommu.secure);

	/* Init hw mgr contexts and add to free list */
	for (i = 0; i < CAM_CTX_MAX; i++) {
		hw_mgr_ctx = &g_fd_hw_mgr.ctx_pool[i];

		memset(hw_mgr_ctx, 0x0, sizeof(*hw_mgr_ctx));
		INIT_LIST_HEAD(&hw_mgr_ctx->list);

		hw_mgr_ctx->ctx_index = i;
		hw_mgr_ctx->device_index = -1;
		hw_mgr_ctx->hw_mgr = &g_fd_hw_mgr;

		list_add_tail(&hw_mgr_ctx->list, &g_fd_hw_mgr.free_ctx_list);
	}

	/* Init hw mgr frame requests and add to free list */
	for (i = 0; i < CAM_CTX_REQ_MAX; i++) {
		frame_req = &g_fd_hw_mgr.frame_req[i];

		memset(frame_req, 0x0, sizeof(*frame_req));
		INIT_LIST_HEAD(&frame_req->list);

		list_add_tail(&frame_req->list, &g_fd_hw_mgr.frame_free_list);
	}

	rc = cam_req_mgr_workq_create("cam_fd_worker", CAM_FD_WORKQ_NUM_TASK,
		&g_fd_hw_mgr.work, CRM_WORKQ_USAGE_IRQ);
	if (rc) {
		CAM_ERR(CAM_FD, "Unable to create a worker, rc=%d", rc);
		goto detach_smmu;
	}

	for (i = 0; i < CAM_FD_WORKQ_NUM_TASK; i++)
		g_fd_hw_mgr.work->task.pool[i].payload =
			&g_fd_hw_mgr.work_data[i];

	/* Setup hw cap so that we can just return the info when requested */
	memset(&g_fd_hw_mgr.fd_caps, 0, sizeof(g_fd_hw_mgr.fd_caps));
	g_fd_hw_mgr.fd_caps.device_iommu = g_fd_hw_mgr.device_iommu;
	g_fd_hw_mgr.fd_caps.cdm_iommu = g_fd_hw_mgr.cdm_iommu;
	g_fd_hw_mgr.fd_caps.hw_caps = g_fd_hw_mgr.hw_device[0].hw_caps;

	CAM_DBG(CAM_FD,
		"IOMMU device(%d, %d), CDM(%d, %d) versions core[%d.%d], wrapper[%d.%d]",
		g_fd_hw_mgr.fd_caps.device_iommu.secure,
		g_fd_hw_mgr.fd_caps.device_iommu.non_secure,
		g_fd_hw_mgr.fd_caps.cdm_iommu.secure,
		g_fd_hw_mgr.fd_caps.cdm_iommu.non_secure,
		g_fd_hw_mgr.fd_caps.hw_caps.core_version.major,
		g_fd_hw_mgr.fd_caps.hw_caps.core_version.minor,
		g_fd_hw_mgr.fd_caps.hw_caps.wrapper_version.major,
		g_fd_hw_mgr.fd_caps.hw_caps.wrapper_version.minor);

	hw_mgr_intf->hw_mgr_priv = &g_fd_hw_mgr;
	hw_mgr_intf->hw_get_caps = cam_fd_mgr_hw_get_caps;
	hw_mgr_intf->hw_acquire = cam_fd_mgr_hw_acquire;
	hw_mgr_intf->hw_release = cam_fd_mgr_hw_release;
	hw_mgr_intf->hw_start = cam_fd_mgr_hw_start;
	hw_mgr_intf->hw_stop = cam_fd_mgr_hw_stop;
	hw_mgr_intf->hw_prepare_update = cam_fd_mgr_hw_prepare_update;
	hw_mgr_intf->hw_config = cam_fd_mgr_hw_config;
	hw_mgr_intf->hw_read = NULL;
	hw_mgr_intf->hw_write = NULL;
	hw_mgr_intf->hw_close = NULL;
	hw_mgr_intf->hw_flush = cam_fd_mgr_hw_flush;

	return rc;

detach_smmu:
	cam_smmu_ops(g_fd_hw_mgr.device_iommu.non_secure, CAM_SMMU_DETACH);
destroy_smmu:
	cam_smmu_destroy_handle(g_fd_hw_mgr.device_iommu.non_secure);
	g_fd_hw_mgr.device_iommu.non_secure = -1;
destroy_mutex:
	mutex_destroy(&g_fd_hw_mgr.ctx_mutex);
	mutex_destroy(&g_fd_hw_mgr.frame_req_mutex);
	mutex_destroy(&g_fd_hw_mgr.hw_mgr_mutex);

	return rc;
}

