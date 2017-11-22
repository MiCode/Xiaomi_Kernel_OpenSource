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

#include <uapi/media/cam_defs.h>
#include <uapi/media/cam_isp.h>
#include "cam_mem_mgr.h"
#include "cam_isp_hw.h"
#include "cam_vfe_hw_intf.h"
#include "cam_isp_packet_parser.h"
#include "cam_debug_util.h"

int cam_isp_add_change_base(
	struct cam_hw_prepare_update_args      *prepare,
	struct list_head                       *res_list_isp_src,
	uint32_t                                base_idx,
	struct cam_kmd_buf_info                *kmd_buf_info)
{
	int rc = -EINVAL;
	struct cam_ife_hw_mgr_res       *hw_mgr_res;
	struct cam_isp_resource_node    *res;
	struct cam_isp_hw_get_cmd_update get_base;
	struct cam_hw_update_entry      *hw_entry;
	uint32_t                         num_ent, i;

	hw_entry = prepare->hw_update_entries;
	num_ent = prepare->num_hw_update_entries;

	/* Max one hw entries required for each base */
	if (num_ent + 1 >= prepare->max_hw_update_entries) {
		CAM_ERR(CAM_ISP, "Insufficient  HW entries :%d %d",
			num_ent, prepare->max_hw_update_entries);
		return -EINVAL;
	}

	list_for_each_entry(hw_mgr_res, res_list_isp_src, list) {
		if (hw_mgr_res->res_type == CAM_IFE_HW_MGR_RES_UNINIT)
			continue;

		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!hw_mgr_res->hw_res[i])
				continue;

			res = hw_mgr_res->hw_res[i];
			if (res->hw_intf->hw_idx != base_idx)
				continue;

			get_base.res  = res;
			get_base.cmd_type = CAM_ISP_HW_CMD_GET_CHANGE_BASE;
			get_base.cmd.cmd_buf_addr = kmd_buf_info->cpu_addr +
				kmd_buf_info->used_bytes/4;
			get_base.cmd.size  = kmd_buf_info->size -
					kmd_buf_info->used_bytes;

			rc = res->hw_intf->hw_ops.process_cmd(
				res->hw_intf->hw_priv,
				CAM_ISP_HW_CMD_GET_CHANGE_BASE, &get_base,
				sizeof(struct cam_isp_hw_get_cmd_update));
			if (rc)
				return rc;

			hw_entry[num_ent].handle = kmd_buf_info->handle;
			hw_entry[num_ent].len    = get_base.cmd.used_bytes;
			hw_entry[num_ent].offset = kmd_buf_info->offset;

			kmd_buf_info->used_bytes += get_base.cmd.used_bytes;
			kmd_buf_info->offset     += get_base.cmd.used_bytes;
			num_ent++;
			prepare->num_hw_update_entries = num_ent;

			/* return success */
			return 0;
		}
	}

	return rc;
}

static int cam_isp_update_dual_config(
	struct cam_hw_prepare_update_args  *prepare,
	struct cam_cmd_buf_desc            *cmd_desc,
	uint32_t                            split_id,
	uint32_t                            base_idx,
	struct cam_ife_hw_mgr_res          *res_list_isp_out,
	uint32_t                            size_isp_out)
{
	int rc = -EINVAL;
	struct cam_isp_dual_config                 *dual_config;
	struct cam_ife_hw_mgr_res                  *hw_mgr_res;
	struct cam_isp_resource_node               *res;
	struct cam_isp_hw_dual_isp_update_args      dual_isp_update_args;
	uint32_t                                    outport_id;
	uint32_t                                    ports_plane_idx;
	size_t                                      len = 0;
	uint32_t                                   *cpu_addr;
	uint32_t                                    i, j;

	CAM_DBG(CAM_UTIL, "cmd des size %d, length: %d",
		cmd_desc->size, cmd_desc->length);

	rc = cam_packet_util_get_cmd_mem_addr(
		cmd_desc->mem_handle, &cpu_addr, &len);
	if (rc)
		return rc;

	cpu_addr += (cmd_desc->offset / 4);
	dual_config = (struct cam_isp_dual_config *)cpu_addr;

	for (i = 0; i < dual_config->num_ports; i++) {

		if (i >= CAM_ISP_IFE_OUT_RES_MAX) {
			CAM_ERR(CAM_UTIL,
				"failed update for i:%d > size_isp_out:%d",
				i, size_isp_out);
			return -EINVAL;
		}

		hw_mgr_res = &res_list_isp_out[i];
		for (j = 0; j < CAM_ISP_HW_SPLIT_MAX; j++) {
			if (!hw_mgr_res->hw_res[j])
				continue;

			if (hw_mgr_res->hw_res[j]->hw_intf->hw_idx != base_idx)
				continue;

			res = hw_mgr_res->hw_res[j];

			if (res->res_id < CAM_ISP_IFE_OUT_RES_BASE ||
				res->res_id >= CAM_ISP_IFE_OUT_RES_MAX)
				continue;

			outport_id = res->res_id & 0xFF;

			ports_plane_idx = (j * (dual_config->num_ports *
				CAM_PACKET_MAX_PLANES)) +
				(outport_id * CAM_PACKET_MAX_PLANES);

			if (dual_config->stripes[ports_plane_idx].port_id == 0)
				continue;

			dual_isp_update_args.split_id = j;
			dual_isp_update_args.res      = res;
			dual_isp_update_args.dual_cfg = dual_config;
			rc = res->hw_intf->hw_ops.process_cmd(
				res->hw_intf->hw_priv,
				CAM_ISP_HW_CMD_STRIPE_UPDATE,
				&dual_isp_update_args,
				sizeof(struct cam_isp_hw_dual_isp_update_args));
			if (rc)
				return rc;
		}
	}

	return rc;
}

int cam_isp_add_cmd_buf_update(
	struct cam_ife_hw_mgr_res            *hw_mgr_res,
	uint32_t                              cmd_type,
	uint32_t                              hw_cmd_type,
	uint32_t                              base_idx,
	uint32_t                             *cmd_buf_addr,
	uint32_t                              kmd_buf_remain_size,
	void                                 *cmd_update_data,
	uint32_t                             *bytes_used)
{
	int rc = 0;
	struct cam_isp_resource_node       *res;
	struct cam_isp_hw_get_cmd_update    cmd_update;
	uint32_t                            i;
	uint32_t                            total_used_bytes = 0;

	if (hw_mgr_res->res_type == CAM_IFE_HW_MGR_RES_UNINIT) {
		CAM_ERR(CAM_ISP, "io res id:%d not valid",
			hw_mgr_res->res_type);
		return -EINVAL;
	}

	for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
		if (!hw_mgr_res->hw_res[i])
			continue;

		if (hw_mgr_res->hw_res[i]->hw_intf->hw_idx != base_idx)
			continue;

		res = hw_mgr_res->hw_res[i];
		cmd_update.res = res;
		cmd_update.cmd_type = hw_cmd_type;
		cmd_update.cmd.cmd_buf_addr = cmd_buf_addr;
		cmd_update.cmd.size = kmd_buf_remain_size;
		cmd_update.data = cmd_update_data;

		CAM_DBG(CAM_ISP, "cmd buffer 0x%pK, size %d",
			cmd_update.cmd.cmd_buf_addr,
			cmd_update.cmd.size);
		rc = res->hw_intf->hw_ops.process_cmd(
			res->hw_intf->hw_priv,
			cmd_update.cmd_type, &cmd_update,
			sizeof(struct cam_isp_hw_get_cmd_update));

		if (rc) {
			CAM_ERR(CAM_ISP, "get buf cmd error:%d",
				res->res_id);
			rc = -ENOMEM;
			return rc;
		}

		total_used_bytes += cmd_update.cmd.used_bytes;
	}
	*bytes_used = total_used_bytes;
	CAM_DBG(CAM_ISP, "total_used_bytes %u", total_used_bytes);
	return rc;
}

int cam_isp_add_command_buffers(
	struct cam_hw_prepare_update_args  *prepare,
	struct cam_kmd_buf_info            *kmd_buf_info,
	struct ctx_base_info               *base_info,
	cam_packet_generic_blob_handler     blob_handler_cb,
	struct cam_ife_hw_mgr_res          *res_list_isp_out,
	uint32_t                            size_isp_out)
{
	int rc = 0;
	uint32_t                           cmd_meta_data, num_ent, i;
	uint32_t                           base_idx;
	enum cam_isp_hw_split_id           split_id;
	struct cam_cmd_buf_desc           *cmd_desc = NULL;
	struct cam_hw_update_entry        *hw_entry;

	hw_entry = prepare->hw_update_entries;
	split_id = base_info->split_id;
	base_idx = base_info->idx;

	/*
	 * set the cmd_desc to point the first command descriptor in the
	 * packet
	 */
	cmd_desc = (struct cam_cmd_buf_desc *)
			((uint8_t *)&prepare->packet->payload +
			prepare->packet->cmd_buf_offset);

	CAM_DBG(CAM_ISP, "split id = %d, number of command buffers:%d",
		split_id, prepare->packet->num_cmd_buf);

	for (i = 0; i < prepare->packet->num_cmd_buf; i++) {
		num_ent = prepare->num_hw_update_entries;
		if (!cmd_desc[i].length)
			continue;

		/* One hw entry space required for left or right or common */
		if (num_ent + 1 >= prepare->max_hw_update_entries) {
			CAM_ERR(CAM_ISP, "Insufficient  HW entries :%d %d",
				num_ent, prepare->max_hw_update_entries);
			return -EINVAL;
		}

		rc = cam_packet_util_validate_cmd_desc(&cmd_desc[i]);
		if (rc)
			return rc;

		cmd_meta_data = cmd_desc[i].meta_data;

		CAM_DBG(CAM_ISP, "meta type: %d, split_id: %d",
			cmd_meta_data, split_id);

		switch (cmd_meta_data) {
		case CAM_ISP_PACKET_META_BASE:
		case CAM_ISP_PACKET_META_LEFT:
		case CAM_ISP_PACKET_META_DMI_LEFT:
			if (split_id == CAM_ISP_HW_SPLIT_LEFT) {
				hw_entry[num_ent].len = cmd_desc[i].length;
				hw_entry[num_ent].handle =
					cmd_desc[i].mem_handle;
				hw_entry[num_ent].offset = cmd_desc[i].offset;

				if (cmd_meta_data ==
					CAM_ISP_PACKET_META_DMI_LEFT)
					hw_entry[num_ent].flags = 0x1;

				num_ent++;
			}
			break;
		case CAM_ISP_PACKET_META_RIGHT:
		case CAM_ISP_PACKET_META_DMI_RIGHT:
			if (split_id == CAM_ISP_HW_SPLIT_RIGHT) {
				hw_entry[num_ent].len = cmd_desc[i].length;
				hw_entry[num_ent].handle =
					cmd_desc[i].mem_handle;
				hw_entry[num_ent].offset = cmd_desc[i].offset;

				if (cmd_meta_data ==
					CAM_ISP_PACKET_META_DMI_RIGHT)
					hw_entry[num_ent].flags = 0x1;
				num_ent++;
			}
			break;
		case CAM_ISP_PACKET_META_COMMON:
		case CAM_ISP_PACKET_META_DMI_COMMON:
			hw_entry[num_ent].len = cmd_desc[i].length;
			hw_entry[num_ent].handle =
				cmd_desc[i].mem_handle;
			hw_entry[num_ent].offset = cmd_desc[i].offset;

			if (cmd_meta_data == CAM_ISP_PACKET_META_DMI_COMMON)
				hw_entry[num_ent].flags = 0x1;

			num_ent++;
			break;
		case CAM_ISP_PACKET_META_DUAL_CONFIG:
			rc = cam_isp_update_dual_config(prepare,
				&cmd_desc[i], split_id, base_idx,
				res_list_isp_out, size_isp_out);

			if (rc)
				return rc;
			break;
		case CAM_ISP_PACKET_META_GENERIC_BLOB_LEFT:
			if (split_id == CAM_ISP_HW_SPLIT_LEFT) {
				struct cam_isp_generic_blob_info   blob_info;

				prepare->num_hw_update_entries = num_ent;
				blob_info.prepare = prepare;
				blob_info.base_info = base_info;
				blob_info.kmd_buf_info = kmd_buf_info;

				rc = cam_packet_util_process_generic_cmd_buffer(
					&cmd_desc[i],
					blob_handler_cb,
					&blob_info);
				if (rc) {
					CAM_ERR(CAM_ISP,
						"Failed in processing blobs %d",
						rc);
					return rc;
				}
				num_ent = prepare->num_hw_update_entries;
			}
			break;
		case CAM_ISP_PACKET_META_GENERIC_BLOB_RIGHT:
			if (split_id == CAM_ISP_HW_SPLIT_RIGHT) {
				struct cam_isp_generic_blob_info   blob_info;

				prepare->num_hw_update_entries = num_ent;
				blob_info.prepare = prepare;
				blob_info.base_info = base_info;
				blob_info.kmd_buf_info = kmd_buf_info;

				rc = cam_packet_util_process_generic_cmd_buffer(
					&cmd_desc[i],
					blob_handler_cb,
					&blob_info);
				if (rc) {
					CAM_ERR(CAM_ISP,
						"Failed in processing blobs %d",
						rc);
					return rc;
				}
				num_ent = prepare->num_hw_update_entries;
			}
			break;
		case CAM_ISP_PACKET_META_GENERIC_BLOB_COMMON: {
			struct cam_isp_generic_blob_info   blob_info;

			prepare->num_hw_update_entries = num_ent;
			blob_info.prepare = prepare;
			blob_info.base_info = base_info;
			blob_info.kmd_buf_info = kmd_buf_info;

			rc = cam_packet_util_process_generic_cmd_buffer(
				&cmd_desc[i],
				blob_handler_cb,
				&blob_info);
			if (rc) {
				CAM_ERR(CAM_ISP,
					"Failed in processing blobs %d", rc);
				return rc;
			}
			num_ent = prepare->num_hw_update_entries;
		}
			break;
		default:
			CAM_ERR(CAM_ISP, "invalid cdm command meta data %d",
				cmd_meta_data);
			return -EINVAL;
		}
		prepare->num_hw_update_entries = num_ent;
	}

	return rc;
}

int cam_isp_add_io_buffers(
	int                                   iommu_hdl,
	int                                   sec_iommu_hdl,
	struct cam_hw_prepare_update_args    *prepare,
	uint32_t                              base_idx,
	struct cam_kmd_buf_info              *kmd_buf_info,
	struct cam_ife_hw_mgr_res            *res_list_isp_out,
	uint32_t                              size_isp_out,
	bool                                  fill_fence)
{
	int rc = 0;
	uint64_t                            io_addr[CAM_PACKET_MAX_PLANES];
	struct cam_buf_io_cfg              *io_cfg;
	struct cam_isp_resource_node       *res;
	struct cam_ife_hw_mgr_res          *hw_mgr_res;
	struct cam_isp_hw_get_cmd_update    update_buf;
	struct cam_isp_hw_get_wm_update     wm_update;
	uint32_t                            kmd_buf_remain_size;
	uint32_t                            i, j, num_out_buf, num_in_buf;
	uint32_t                            res_id_out, res_id_in, plane_id;
	uint32_t                            io_cfg_used_bytes, num_ent;
	size_t                              size;
	int32_t                             hdl;
	int                                 mmu_hdl;
	bool                                mode, is_buf_secure;

	io_cfg = (struct cam_buf_io_cfg *) ((uint8_t *)
			&prepare->packet->payload +
			prepare->packet->io_configs_offset);
	num_out_buf = 0;
	num_in_buf  = 0;
	io_cfg_used_bytes = 0;

	/* Max one hw entries required for each base */
	if (prepare->num_hw_update_entries + 1 >=
			prepare->max_hw_update_entries) {
		CAM_ERR(CAM_ISP, "Insufficient  HW entries :%d %d",
			prepare->num_hw_update_entries,
			prepare->max_hw_update_entries);
		return -EINVAL;
	}

	for (i = 0; i < prepare->packet->num_io_configs; i++) {
		CAM_DBG(CAM_ISP, "======= io config idx %d ============", i);
		CAM_DBG(CAM_ISP, "i %d resource_type:%d fence:%d",
			i, io_cfg[i].resource_type, io_cfg[i].fence);
		CAM_DBG(CAM_ISP, "format: %d", io_cfg[i].format);
		CAM_DBG(CAM_ISP, "direction %d",
			io_cfg[i].direction);

		if (io_cfg[i].direction == CAM_BUF_OUTPUT) {
			res_id_out = io_cfg[i].resource_type & 0xFF;
			if (res_id_out >= size_isp_out) {
				CAM_ERR(CAM_ISP, "invalid out restype:%x",
					io_cfg[i].resource_type);
				return -EINVAL;
			}

			CAM_DBG(CAM_ISP,
				"configure output io with fill fence %d",
				fill_fence);
			if (fill_fence) {
				if (num_out_buf <
					prepare->max_out_map_entries) {
					prepare->out_map_entries[num_out_buf].
						resource_handle =
							io_cfg[i].resource_type;
					prepare->out_map_entries[num_out_buf].
						sync_id = io_cfg[i].fence;
					num_out_buf++;
				} else {
					CAM_ERR(CAM_ISP, "ln_out:%d max_ln:%d",
						num_out_buf,
						prepare->max_out_map_entries);
					return -EINVAL;
				}
			}

			hw_mgr_res = &res_list_isp_out[res_id_out];
			if (hw_mgr_res->res_type == CAM_IFE_HW_MGR_RES_UNINIT) {
				CAM_ERR(CAM_ISP, "io res id:%d not valid",
					io_cfg[i].resource_type);
				return -EINVAL;
			}
		} else if (io_cfg[i].direction == CAM_BUF_INPUT) {
			res_id_in = io_cfg[i].resource_type & 0xFF;
			CAM_DBG(CAM_ISP,
				"configure input io with fill fence %d",
				fill_fence);
			if (fill_fence) {
				if (num_in_buf < prepare->max_in_map_entries) {
					prepare->in_map_entries[num_in_buf].
						resource_handle =
							io_cfg[i].resource_type;
					prepare->in_map_entries[num_in_buf].
						sync_id =
							io_cfg[i].fence;
					num_in_buf++;
				} else {
					CAM_ERR(CAM_ISP, "ln_in:%d imax_ln:%d",
						num_in_buf,
						prepare->max_in_map_entries);
					return -EINVAL;
				}
			}
			continue;
		} else {
			CAM_ERR(CAM_ISP, "Invalid io config direction :%d",
				io_cfg[i].direction);
			return -EINVAL;
		}

		CAM_DBG(CAM_ISP, "setup mem io");
		for (j = 0; j < CAM_ISP_HW_SPLIT_MAX; j++) {
			if (!hw_mgr_res->hw_res[j])
				continue;

			if (hw_mgr_res->hw_res[j]->hw_intf->hw_idx != base_idx)
				continue;

			res = hw_mgr_res->hw_res[j];
			if (res->res_id != io_cfg[i].resource_type) {
				CAM_ERR(CAM_ISP,
					"wm err res id:%d io res id:%d",
					res->res_id, io_cfg[i].resource_type);
				return -EINVAL;
			}

			memset(io_addr, 0, sizeof(io_addr));

			for (plane_id = 0; plane_id < CAM_PACKET_MAX_PLANES;
						plane_id++) {
				if (!io_cfg[i].mem_handle[plane_id])
					break;

				hdl = io_cfg[i].mem_handle[plane_id];
				if (res->process_cmd(res,
						CAM_ISP_HW_CMD_GET_SECURE_MODE,
						&mode,
						sizeof(bool)))
					return -EINVAL;

				is_buf_secure = cam_mem_is_secure_buf(hdl);
				if ((mode == CAM_SECURE_MODE_SECURE) &&
					is_buf_secure) {
					mmu_hdl = sec_iommu_hdl;
				} else if (
					(mode == CAM_SECURE_MODE_NON_SECURE) &&
					(!is_buf_secure)) {
					mmu_hdl = iommu_hdl;
				} else {
					CAM_ERR_RATE_LIMIT(CAM_ISP,
						"Invalid hdl: port mode[%u], buf mode[%u]",
						mode, is_buf_secure);
					return -EINVAL;
				}

				rc = cam_mem_get_io_buf(
					io_cfg[i].mem_handle[plane_id],
					mmu_hdl, &io_addr[plane_id], &size);
				if (rc) {
					CAM_ERR(CAM_ISP,
						"no io addr for plane%d",
						plane_id);
					rc = -ENOMEM;
					return rc;
				}

				if (io_addr[plane_id] >> 32) {
					CAM_ERR(CAM_ISP,
						"Invalid mapped address");
					rc = -EINVAL;
					return rc;
				}

				/* need to update with offset */
				io_addr[plane_id] +=
						io_cfg[i].offsets[plane_id];
				CAM_DBG(CAM_ISP,
					"get io_addr for plane %d: 0x%llx",
					plane_id, io_addr[plane_id]);
			}
			if (!plane_id) {
				CAM_ERR(CAM_ISP, "No valid planes for res%d",
					res->res_id);
				rc = -ENOMEM;
				return rc;
			}

			if ((kmd_buf_info->used_bytes + io_cfg_used_bytes) <
				kmd_buf_info->size) {
				kmd_buf_remain_size = kmd_buf_info->size -
					(kmd_buf_info->used_bytes +
					io_cfg_used_bytes);
			} else {
				CAM_ERR(CAM_ISP,
					"no free kmd memory for base %d",
					base_idx);
				rc = -ENOMEM;
				return rc;
			}
			update_buf.res = res;
			update_buf.cmd_type = CAM_ISP_HW_CMD_GET_BUF_UPDATE;
			update_buf.cmd.cmd_buf_addr = kmd_buf_info->cpu_addr +
				kmd_buf_info->used_bytes/4 +
					io_cfg_used_bytes/4;
			wm_update.image_buf = io_addr;
			wm_update.num_buf   = plane_id;
			wm_update.io_cfg    = &io_cfg[i];
			update_buf.cmd.size = kmd_buf_remain_size;
			update_buf.wm_update = &wm_update;

			CAM_DBG(CAM_ISP, "cmd buffer 0x%pK, size %d",
				update_buf.cmd.cmd_buf_addr,
				update_buf.cmd.size);
			rc = res->hw_intf->hw_ops.process_cmd(
				res->hw_intf->hw_priv,
				CAM_ISP_HW_CMD_GET_BUF_UPDATE, &update_buf,
				sizeof(struct cam_isp_hw_get_cmd_update));

			if (rc) {
				CAM_ERR(CAM_ISP, "get buf cmd error:%d",
					res->res_id);
				rc = -ENOMEM;
				return rc;
			}
			io_cfg_used_bytes += update_buf.cmd.used_bytes;
		}
	}

	CAM_DBG(CAM_ISP, "io_cfg_used_bytes %d, fill_fence %d",
		io_cfg_used_bytes, fill_fence);
	if (io_cfg_used_bytes) {
		/* Update the HW entries */
		num_ent = prepare->num_hw_update_entries;
		prepare->hw_update_entries[num_ent].handle =
					kmd_buf_info->handle;
		prepare->hw_update_entries[num_ent].len = io_cfg_used_bytes;
		prepare->hw_update_entries[num_ent].offset =
			kmd_buf_info->offset;
		num_ent++;

		kmd_buf_info->used_bytes += io_cfg_used_bytes;
		kmd_buf_info->offset     += io_cfg_used_bytes;
		prepare->num_hw_update_entries = num_ent;
	}

	if (fill_fence) {
		prepare->num_out_map_entries = num_out_buf;
		prepare->num_in_map_entries  = num_in_buf;
	}

	return rc;
}


int cam_isp_add_reg_update(
	struct cam_hw_prepare_update_args    *prepare,
	struct list_head                     *res_list_isp_src,
	uint32_t                              base_idx,
	struct cam_kmd_buf_info              *kmd_buf_info)
{
	int rc = -EINVAL;
	struct cam_isp_resource_node         *res;
	struct cam_ife_hw_mgr_res            *hw_mgr_res;
	struct cam_hw_update_entry           *hw_entry;
	struct cam_isp_hw_get_cmd_update      get_regup;
	uint32_t kmd_buf_remain_size, num_ent, i, reg_update_size;

	hw_entry = prepare->hw_update_entries;
	/* Max one hw entries required for each base */
	if (prepare->num_hw_update_entries + 1 >=
				prepare->max_hw_update_entries) {
		CAM_ERR(CAM_ISP, "Insufficient  HW entries :%d %d",
			prepare->num_hw_update_entries,
			prepare->max_hw_update_entries);
		return -EINVAL;
	}

	reg_update_size = 0;
	list_for_each_entry(hw_mgr_res, res_list_isp_src, list) {
		if (hw_mgr_res->res_type == CAM_IFE_HW_MGR_RES_UNINIT)
			continue;

		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!hw_mgr_res->hw_res[i])
				continue;

			res = hw_mgr_res->hw_res[i];
			if (res->hw_intf->hw_idx != base_idx)
				continue;

			if (kmd_buf_info->size > (kmd_buf_info->used_bytes +
				reg_update_size)) {
				kmd_buf_remain_size =  kmd_buf_info->size -
					(kmd_buf_info->used_bytes +
					reg_update_size);
			} else {
				CAM_ERR(CAM_ISP, "no free mem %d %d %d",
					base_idx, kmd_buf_info->size,
					kmd_buf_info->used_bytes +
					reg_update_size);
				rc = -EINVAL;
				return rc;
			}

			get_regup.cmd.cmd_buf_addr = kmd_buf_info->cpu_addr +
				kmd_buf_info->used_bytes/4 +
				reg_update_size/4;
			get_regup.cmd.size = kmd_buf_remain_size;
			get_regup.cmd_type = CAM_ISP_HW_CMD_GET_REG_UPDATE;
			get_regup.res = res;

			rc = res->hw_intf->hw_ops.process_cmd(
				res->hw_intf->hw_priv,
				CAM_ISP_HW_CMD_GET_REG_UPDATE, &get_regup,
				sizeof(struct cam_isp_hw_get_cmd_update));
			if (rc)
				return rc;

			CAM_DBG(CAM_ISP, "Reg update added for res %d hw_id %d",
				res->res_id, res->hw_intf->hw_idx);
			reg_update_size += get_regup.cmd.used_bytes;
		}
	}

	if (reg_update_size) {
		/* Update the HW entries */
		num_ent = prepare->num_hw_update_entries;
		prepare->hw_update_entries[num_ent].handle =
					kmd_buf_info->handle;
		prepare->hw_update_entries[num_ent].len = reg_update_size;
		prepare->hw_update_entries[num_ent].offset =
			kmd_buf_info->offset;
		num_ent++;

		kmd_buf_info->used_bytes += reg_update_size;
		kmd_buf_info->offset     += reg_update_size;
		prepare->num_hw_update_entries = num_ent;
		/* reg update is success return status 0 */
		rc = 0;
	}

	return rc;
}

