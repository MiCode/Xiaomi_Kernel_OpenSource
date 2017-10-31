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
	struct cam_isp_hw_get_cdm_args   get_base;
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
			get_base.cmd_buf_addr = kmd_buf_info->cpu_addr +
				kmd_buf_info->used_bytes/4;
			get_base.size  = kmd_buf_info->size -
					kmd_buf_info->used_bytes;

			rc = res->hw_intf->hw_ops.process_cmd(
				res->hw_intf->hw_priv,
				CAM_VFE_HW_CMD_GET_CHANGE_BASE, &get_base,
				sizeof(struct cam_isp_hw_get_cdm_args));
			if (rc)
				return rc;

			hw_entry[num_ent].handle = kmd_buf_info->handle;
			hw_entry[num_ent].len    = get_base.used_bytes;
			hw_entry[num_ent].offset = kmd_buf_info->offset;

			kmd_buf_info->used_bytes += get_base.used_bytes;
			kmd_buf_info->offset     += get_base.used_bytes;
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
		hw_mgr_res = &res_list_isp_out[i];
		for (j = 0; j < CAM_ISP_HW_SPLIT_MAX; j++) {
			if (!hw_mgr_res->hw_res[j])
				continue;

			if (hw_mgr_res->hw_res[j]->hw_intf->hw_idx != base_idx)
				continue;

			res = hw_mgr_res->hw_res[j];
			dual_isp_update_args.split_id = j;
			dual_isp_update_args.res      = res;
			dual_isp_update_args.dual_cfg = dual_config;
			rc = res->hw_intf->hw_ops.process_cmd(
				res->hw_intf->hw_priv,
				CAM_VFE_HW_CMD_STRIPE_UPDATE,
				&dual_isp_update_args,
				sizeof(struct cam_isp_hw_dual_isp_update_args));
			if (rc)
				return rc;
		}
	}

	return rc;
}

int cam_isp_add_command_buffers(
	struct cam_hw_prepare_update_args  *prepare,
	enum cam_isp_hw_split_id            split_id,
	uint32_t                            base_idx,
	struct cam_ife_hw_mgr_res          *res_list_isp_out,
	uint32_t                            size_isp_out)
{
	int rc = 0;
	uint32_t  cmd_meta_data, num_ent, i;
	struct cam_cmd_buf_desc       *cmd_desc = NULL;
	struct cam_hw_update_entry    *hw_entry;

	hw_entry = prepare->hw_update_entries;
	num_ent = prepare->num_hw_update_entries;
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
		case CAM_ISP_PACKET_META_GENERIC_BLOB:
			break;
		default:
			CAM_ERR(CAM_ISP, "invalid cdm command meta data %d",
				cmd_meta_data);
			return -EINVAL;
		}
	}

	prepare->num_hw_update_entries = num_ent;

	return rc;
}

static int cam_isp_handle_hfr_config(
	struct cam_isp_generic_blob_info *blob_info,
	struct cam_isp_resource_hfr_config *hfr_config, uint32_t blob_size)
{
	uint32_t cal_blob_size =
		sizeof(struct cam_isp_resource_hfr_config) +
		(sizeof(struct cam_isp_port_hfr_config) *
		(hfr_config->num_io_configs - 1));

	if (cal_blob_size != blob_size) {
		CAM_ERR(CAM_ISP, "Invalid blob size %d %d",
			cal_blob_size, blob_size);
		return -EINVAL;
	}

	CAM_DBG(CAM_ISP, "HFR num_io_config = %d", hfr_config->num_io_configs);

	if (blob_info->hfr_config) {
		CAM_WARN(CAM_ISP,
			"Ignoring previous hfr_config, prev=%d, curr=%d",
			blob_info->hfr_config->num_io_configs,
			hfr_config->num_io_configs);
		kfree(blob_info->hfr_config);
	}

	blob_info->hfr_config = kzalloc(blob_size, GFP_ATOMIC);
	if (!blob_info->hfr_config)
		return -ENOMEM;

	memcpy(blob_info->hfr_config, hfr_config, blob_size);

	return 0;
}

static int cam_isp_packet_generic_blob_handler(void *user_data,
	uint32_t blob_type, uint32_t blob_size, uint8_t *blob_data)
{
	int rc = 0;

	if (!blob_data || (blob_size == 0)) {
		CAM_ERR(CAM_ISP, "Invalid blob info %pK %d", blob_data,
			blob_size);
		return -EINVAL;
	}

	switch (blob_type) {
	case CAM_ISP_GENERIC_BLOB_TYPE_HFR_CONFIG:
		rc = cam_isp_handle_hfr_config(user_data,
			(struct cam_isp_resource_hfr_config *)blob_data,
			blob_size);
		if (rc)
			CAM_ERR(CAM_ISP, "Failed in handling hfr config %d",
				rc);

		break;
	default:
		CAM_WARN(CAM_ISP, "Invalid blob type %d", blob_type);
		break;
	}

	return rc;
}

int cam_isp_process_generic_cmd_buffer(
	struct cam_hw_prepare_update_args *prepare,
	struct cam_isp_generic_blob_info  *blob_info)
{
	int i, rc = 0;
	struct cam_cmd_buf_desc *cmd_desc = NULL;

	/*
	 * set the cmd_desc to point the first command descriptor in the
	 * packet
	 */
	cmd_desc = (struct cam_cmd_buf_desc *)
			((uint8_t *)&prepare->packet->payload +
			prepare->packet->cmd_buf_offset);

	for (i = 0; i < prepare->packet->num_cmd_buf; i++) {
		if (!cmd_desc[i].length)
			continue;

		if (cmd_desc[i].meta_data != CAM_ISP_PACKET_META_GENERIC_BLOB)
			continue;

		rc = cam_packet_util_validate_cmd_desc(&cmd_desc[i]);
		if (rc)
			return rc;

		rc = cam_packet_util_process_generic_cmd_buffer(&cmd_desc[i],
			cam_isp_packet_generic_blob_handler, blob_info);
		if (rc)
			CAM_ERR(CAM_ISP, "Failed in processing blobs %d", rc);

		break;
	}

	return rc;
}

int cam_isp_add_hfr_config_hw_update(
	struct cam_isp_resource_hfr_config   *hfr_config,
	struct cam_hw_prepare_update_args    *prepare,
	uint32_t                              base_idx,
	struct cam_kmd_buf_info              *kmd_buf_info,
	struct cam_ife_hw_mgr_res            *res_list_isp_out,
	uint32_t                              size_isp_out)
{
	int rc = 0;
	struct cam_isp_resource_node       *res;
	struct cam_ife_hw_mgr_res          *hw_mgr_res;
	struct cam_isp_hw_get_hfr_update    update_hfr;
	struct cam_isp_port_hfr_config     *io_hfr_config;
	uint32_t                            kmd_buf_remain_size;
	uint32_t                            i, j;
	uint32_t                            res_id_out;
	uint32_t                            hfr_cfg_used_bytes, num_ent;

	hfr_cfg_used_bytes = 0;

	/* Max one hw entries required for hfr config update */
	if (prepare->num_hw_update_entries + 1 >=
			prepare->max_hw_update_entries) {
		CAM_ERR(CAM_ISP, "Insufficient  HW entries :%d %d",
			prepare->num_hw_update_entries,
			prepare->max_hw_update_entries);
		return -EINVAL;
	}

	CAM_DBG(CAM_ISP, "num_io_configs= %d", hfr_config->num_io_configs);

	for (i = 0; i < hfr_config->num_io_configs; i++) {
		io_hfr_config = &hfr_config->io_hfr_config[i];
		res_id_out = io_hfr_config->resource_type & 0xFF;

		CAM_DBG(CAM_ISP, "hfr config idx %d, type=%d", i, res_id_out);

		if (res_id_out >= size_isp_out) {
			CAM_ERR(CAM_ISP, "invalid out restype:%x",
				io_hfr_config->resource_type);
			return -EINVAL;
		}

		hw_mgr_res = &res_list_isp_out[res_id_out];
		if (hw_mgr_res->res_type == CAM_IFE_HW_MGR_RES_UNINIT) {
			CAM_ERR(CAM_ISP, "io res id:%d not valid",
				io_hfr_config->resource_type);
			return -EINVAL;
		}

		for (j = 0; j < CAM_ISP_HW_SPLIT_MAX; j++) {
			if (!hw_mgr_res->hw_res[j])
				continue;

			if (hw_mgr_res->hw_res[j]->hw_intf->hw_idx != base_idx)
				continue;

			res = hw_mgr_res->hw_res[j];
			if (res->res_id !=
				io_hfr_config->resource_type) {
				CAM_ERR(CAM_ISP,
					"wm err res id:%d io res id:%d",
					res->res_id,
					io_hfr_config->resource_type);
				return -EINVAL;
			}

			if ((kmd_buf_info->used_bytes + hfr_cfg_used_bytes) <
				kmd_buf_info->size) {
				kmd_buf_remain_size = kmd_buf_info->size -
					(kmd_buf_info->used_bytes +
					hfr_cfg_used_bytes);
			} else {
				CAM_ERR(CAM_ISP,
					"no free kmd memory for base %d",
					base_idx);
				rc = -ENOMEM;
				return rc;
			}

			update_hfr.cdm.res = res;
			update_hfr.cdm.cmd_buf_addr = kmd_buf_info->cpu_addr +
				kmd_buf_info->used_bytes/4 +
					hfr_cfg_used_bytes/4;
			update_hfr.cdm.size = kmd_buf_remain_size;
			update_hfr.io_hfr_cfg    = io_hfr_config;

			CAM_DBG(CAM_ISP, "cmd buffer 0x%pK, size %d",
				update_hfr.cdm.cmd_buf_addr,
				update_hfr.cdm.size);
			rc = res->hw_intf->hw_ops.process_cmd(
				res->hw_intf->hw_priv,
				CAM_VFE_HW_CMD_GET_HFR_UPDATE, &update_hfr,
				sizeof(struct cam_isp_hw_get_hfr_update));

			if (rc) {
				CAM_ERR(CAM_ISP, "get buf cmd error:%d",
					res->res_id);
				rc = -ENOMEM;
				return rc;
			}
			hfr_cfg_used_bytes += update_hfr.cdm.used_bytes;
		}
	}

	CAM_DBG(CAM_ISP, "hfr_cfg_used_bytes %d", hfr_cfg_used_bytes);
	if (hfr_cfg_used_bytes) {
		/* Update the HW entries */
		num_ent = prepare->num_hw_update_entries;
		prepare->hw_update_entries[num_ent].handle =
					kmd_buf_info->handle;
		prepare->hw_update_entries[num_ent].len = hfr_cfg_used_bytes;
		prepare->hw_update_entries[num_ent].offset =
			kmd_buf_info->offset;
		num_ent++;

		kmd_buf_info->used_bytes += hfr_cfg_used_bytes;
		kmd_buf_info->offset     += hfr_cfg_used_bytes;
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
	struct cam_isp_hw_get_buf_update    update_buf;
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
						CAM_VFE_HW_CMD_GET_SECURE_MODE,
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
			update_buf.cdm.res = res;
			update_buf.cdm.cmd_buf_addr = kmd_buf_info->cpu_addr +
				kmd_buf_info->used_bytes/4 +
					io_cfg_used_bytes/4;
			update_buf.cdm.size = kmd_buf_remain_size;
			update_buf.image_buf = io_addr;
			update_buf.num_buf   = plane_id;
			update_buf.io_cfg    = &io_cfg[i];

			CAM_DBG(CAM_ISP, "cmd buffer 0x%pK, size %d",
				update_buf.cdm.cmd_buf_addr,
				update_buf.cdm.size);
			rc = res->hw_intf->hw_ops.process_cmd(
				res->hw_intf->hw_priv,
				CAM_VFE_HW_CMD_GET_BUF_UPDATE, &update_buf,
				sizeof(struct cam_isp_hw_get_buf_update));

			if (rc) {
				CAM_ERR(CAM_ISP, "get buf cmd error:%d",
					res->res_id);
				rc = -ENOMEM;
				return rc;
			}
			io_cfg_used_bytes += update_buf.cdm.used_bytes;
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
	struct cam_isp_hw_get_cdm_args        get_regup;
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

			get_regup.cmd_buf_addr = kmd_buf_info->cpu_addr +
				kmd_buf_info->used_bytes/4 +
				reg_update_size/4;
			get_regup.size = kmd_buf_remain_size;
			get_regup.res = res;

			rc = res->hw_intf->hw_ops.process_cmd(
				res->hw_intf->hw_priv,
				CAM_VFE_HW_CMD_GET_REG_UPDATE, &get_regup,
				sizeof(struct cam_isp_hw_get_cdm_args));
			if (rc)
				return rc;

			CAM_DBG(CAM_ISP, "Reg update added for res %d hw_id %d",
				res->res_id, res->hw_intf->hw_idx);
			reg_update_size += get_regup.used_bytes;
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

