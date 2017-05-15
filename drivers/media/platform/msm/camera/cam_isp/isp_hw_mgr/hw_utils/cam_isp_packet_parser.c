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

#undef CDBG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)

static int cam_isp_get_cmd_mem_addr(int handle, uint32_t **buf_addr,
	size_t *len)
{
	int rc = 0;
	uint64_t kmd_buf_addr = 0;

	rc = cam_mem_get_cpu_buf(handle, &kmd_buf_addr, len);
	if (rc) {
		pr_err("%s:%d Unable to get the virtual address rc:%d\n",
			__func__, __LINE__, rc);
		rc = -ENOMEM;
	} else {
		if (kmd_buf_addr && *len)
			*buf_addr = (uint32_t *)kmd_buf_addr;
		else {
			pr_err("%s:%d Invalid addr and length :%ld\n",
				__func__, __LINE__, *len);
			rc = -ENOMEM;
		}
	}
	return rc;
}

static int cam_isp_validate_cmd_desc(
	struct cam_cmd_buf_desc *cmd_desc)
{
	if (cmd_desc->length > cmd_desc->size ||
		(cmd_desc->mem_handle <= 0)) {
		pr_err("%s:%d invalid cmd arg %d %d %d %d\n",
			__func__, __LINE__, cmd_desc->offset,
			cmd_desc->length, cmd_desc->mem_handle,
			cmd_desc->size);
		return -EINVAL;
	}

	return 0;
}

int cam_isp_validate_packet(struct cam_packet *packet)
{
	if (!packet)
		return -EINVAL;

	CDBG("%s:%d num cmd buf:%d num of io config:%d kmd buf index:%d\n",
		__func__, __LINE__, packet->num_cmd_buf,
		packet->num_io_configs, packet->kmd_cmd_buf_index);

	if (packet->kmd_cmd_buf_index >= packet->num_cmd_buf ||
		(!packet->header.size) ||
		packet->cmd_buf_offset > packet->header.size ||
		packet->io_configs_offset > packet->header.size)  {
		pr_err("%s:%d invalid packet:%d %d %d %d %d\n",
			__func__, __LINE__, packet->kmd_cmd_buf_index,
			packet->num_cmd_buf, packet->cmd_buf_offset,
			packet->io_configs_offset, packet->header.size);
		return -EINVAL;
	}

	CDBG("%s:%d exit\n", __func__, __LINE__);
	return 0;
}

int cam_isp_get_kmd_buffer(struct cam_packet *packet,
	struct cam_isp_kmd_buf_info *kmd_buf)
{
	int                      rc = 0;
	size_t                   len = 0;
	struct cam_cmd_buf_desc *cmd_desc;
	uint32_t                *cpu_addr;

	if (!packet || !kmd_buf) {
		pr_err("%s:%d Invalid arg\n", __func__, __LINE__);
		rc = -EINVAL;
		return rc;
	}

	/* Take first command descriptor and add offset to it for kmd*/
	cmd_desc = (struct cam_cmd_buf_desc *) ((uint8_t *)
			&packet->payload + packet->cmd_buf_offset);
	cmd_desc += packet->kmd_cmd_buf_index;

	CDBG("%s:%d enter\n", __func__, __LINE__);
	rc = cam_isp_validate_cmd_desc(cmd_desc);
	if (rc)
		return rc;

	CDBG("%s:%d enter\n", __func__, __LINE__);
	rc = cam_isp_get_cmd_mem_addr(cmd_desc->mem_handle, &cpu_addr,
		&len);
	if (rc)
		return rc;

	if (len < cmd_desc->size) {
		pr_err("%s:%d invalid memory len:%ld and cmd desc size:%d\n",
			__func__, __LINE__, len, cmd_desc->size);
		return -EINVAL;
	}

	cpu_addr += cmd_desc->offset/4 + packet->kmd_cmd_buf_offset/4;
	CDBG("%s:%d total size %d, cmd size: %d, KMD buffer size: %d\n",
		__func__, __LINE__, cmd_desc->size, cmd_desc->length,
		cmd_desc->size - cmd_desc->length);
	CDBG("%s:%d: handle 0x%x, cmd offset %d, kmd offset %d, addr 0x%pK\n",
		__func__, __LINE__, cmd_desc->mem_handle, cmd_desc->offset,
		packet->kmd_cmd_buf_offset, cpu_addr);

	kmd_buf->cpu_addr   = cpu_addr;
	kmd_buf->handle     = cmd_desc->mem_handle;
	kmd_buf->offset     = cmd_desc->offset + packet->kmd_cmd_buf_offset;
	kmd_buf->size       = cmd_desc->size - cmd_desc->length;
	kmd_buf->used_bytes = 0;

	return rc;
}

int cam_isp_add_change_base(
	struct cam_hw_prepare_update_args      *prepare,
	struct list_head                       *res_list_isp_src,
	uint32_t                                base_idx,
	struct cam_isp_kmd_buf_info            *kmd_buf_info)
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
		pr_err("%s:%d Insufficient  HW entries :%d %d\n",
			__func__, __LINE__, num_ent,
			prepare->max_hw_update_entries);
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


int cam_isp_add_command_buffers(
	struct cam_hw_prepare_update_args  *prepare,
	uint32_t                            split_id)
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

	CDBG("%s:%d split id = %d, number of command buffers:%d\n", __func__,
		__LINE__, split_id, prepare->packet->num_cmd_buf);

	for (i = 0; i < prepare->packet->num_cmd_buf; i++) {
		if (!cmd_desc[i].length)
			continue;

		/* One hw entry space required for left or right or common */
		if (num_ent + 1 >= prepare->max_hw_update_entries) {
			pr_err("%s:%d Insufficient  HW entries :%d %d\n",
				__func__, __LINE__, num_ent,
				prepare->max_hw_update_entries);
			return -EINVAL;
		}

		rc = cam_isp_validate_cmd_desc(&cmd_desc[i]);
		if (rc)
			return rc;

		cmd_meta_data = cmd_desc[i].meta_data;

		CDBG("%s:%d meta type: %d, split_id: %d\n", __func__, __LINE__,
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
		default:
			pr_err("%s:%d invalid cdm command meta data %d\n",
			__func__, __LINE__, cmd_meta_data);
			return -EINVAL;
		}
	}

	prepare->num_hw_update_entries = num_ent;

	return rc;
}


int cam_isp_add_io_buffers(
	int                                   iommu_hdl,
	struct cam_hw_prepare_update_args    *prepare,
	uint32_t                              base_idx,
	struct cam_isp_kmd_buf_info          *kmd_buf_info,
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
	size_t size;

	io_cfg = (struct cam_buf_io_cfg *) ((uint8_t *)
			&prepare->packet->payload +
			prepare->packet->io_configs_offset);
	num_out_buf = 0;
	num_in_buf  = 0;
	io_cfg_used_bytes = 0;

	/* Max one hw entries required for each base */
	if (prepare->num_hw_update_entries + 1 >=
			prepare->max_hw_update_entries) {
		pr_err("%s:%d Insufficient  HW entries :%d %d\n",
			__func__, __LINE__, prepare->num_hw_update_entries,
			prepare->max_hw_update_entries);
		return -EINVAL;
	}

	for (i = 0; i < prepare->packet->num_io_configs; i++) {
		CDBG("%s:%d ======= io config idx %d ============\n",
			__func__, __LINE__, i);
		CDBG("%s:%d resource_type:%d fence:%d\n", __func__, __LINE__,
			io_cfg[i].resource_type, io_cfg[i].fence);
		CDBG("%s:%d format: %d\n", __func__, __LINE__,
			io_cfg[i].format);
		CDBG("%s:%d direction %d\n", __func__, __LINE__,
			io_cfg[i].direction);

		if (io_cfg[i].direction == CAM_BUF_OUTPUT) {
			res_id_out = io_cfg[i].resource_type & 0xFF;
			if (res_id_out >= size_isp_out) {
				pr_err("%s:%d invalid out restype:%x\n",
					__func__, __LINE__,
					io_cfg[i].resource_type);
				return -EINVAL;
			}

			CDBG("%s:%d configure output io with fill fence %d\n",
				__func__, __LINE__, fill_fence);
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
					pr_err("%s:%d ln_out:%d max_ln:%d\n",
						__func__, __LINE__,
						num_out_buf,
						prepare->max_out_map_entries);
					return -EINVAL;
				}
			}

			hw_mgr_res = &res_list_isp_out[res_id_out];
			if (hw_mgr_res->res_type == CAM_IFE_HW_MGR_RES_UNINIT) {
				pr_err("%s:%d io res id:%d not valid\n",
					__func__, __LINE__,
					io_cfg[i].resource_type);
				return -EINVAL;
			}
		} else if (io_cfg[i].direction == CAM_BUF_INPUT) {
			res_id_in = io_cfg[i].resource_type & 0xFF;
			CDBG("%s:%d configure input io with fill fence %d\n",
				__func__, __LINE__, fill_fence);
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
					pr_err("%s:%d ln_in:%d imax_ln:%d\n",
						__func__, __LINE__,
						num_in_buf,
						prepare->max_in_map_entries);
					return -EINVAL;
				}
			}
			continue;
		} else {
			pr_err("%s:%d Invalid io config direction :%d\n",
				__func__, __LINE__,
				io_cfg[i].direction);
			return -EINVAL;
		}

		CDBG("%s:%d setup mem io\n", __func__, __LINE__);
		for (j = 0; j < CAM_ISP_HW_SPLIT_MAX; j++) {
			if (!hw_mgr_res->hw_res[j])
				continue;

			if (hw_mgr_res->hw_res[j]->hw_intf->hw_idx != base_idx)
				continue;

			res = hw_mgr_res->hw_res[j];
			if (res->res_id != io_cfg[i].resource_type) {
				pr_err("%s:%d wm err res id:%d io res id:%d\n",
					__func__, __LINE__, res->res_id,
					io_cfg[i].resource_type);
				return -EINVAL;
			}

			memset(io_addr, 0, sizeof(io_addr));

			for (plane_id = 0; plane_id < CAM_PACKET_MAX_PLANES;
						plane_id++) {
				if (!io_cfg[i].mem_handle[plane_id])
					break;

				rc = cam_mem_get_io_buf(
					io_cfg[i].mem_handle[plane_id],
					iommu_hdl, &io_addr[plane_id], &size);
				if (rc) {
					pr_err("%s:%d no io addr for plane%d\n",
						__func__, __LINE__, plane_id);
					rc = -ENOMEM;
					return rc;
				}

				if (io_addr[plane_id] >> 32) {
					pr_err("Invalid mapped address\n");
					rc = -EINVAL;
					return rc;
				}

				/* need to update with offset */
				io_addr[plane_id] +=
						io_cfg[i].offsets[plane_id];
				CDBG("%s: get io_addr for plane %d: 0x%llx\n",
					__func__, plane_id,
					io_addr[plane_id]);
			}
			if (!plane_id) {
				pr_err("%s:%d No valid planes for res%d\n",
					__func__, __LINE__, res->res_id);
				rc = -ENOMEM;
				return rc;
			}

			if ((kmd_buf_info->used_bytes + io_cfg_used_bytes) <
				kmd_buf_info->size) {
				kmd_buf_remain_size = kmd_buf_info->size -
					(kmd_buf_info->used_bytes +
					io_cfg_used_bytes);
			} else {
				pr_err("%s:%d no free kmd memory for base %d\n",
					__func__, __LINE__, base_idx);
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

			CDBG("%s:%d: cmd buffer 0x%pK, size %d\n", __func__,
				__LINE__, update_buf.cdm.cmd_buf_addr,
				update_buf.cdm.size);
			rc = res->hw_intf->hw_ops.process_cmd(
				res->hw_intf->hw_priv,
				CAM_VFE_HW_CMD_GET_BUF_UPDATE, &update_buf,
				sizeof(struct cam_isp_hw_get_buf_update));

			if (rc) {
				pr_err("%s:%d get buf cmd error:%d\n",
					__func__, __LINE__, res->res_id);
				rc = -ENOMEM;
				return rc;
			}
			io_cfg_used_bytes += update_buf.cdm.used_bytes;
		}
	}

	CDBG("%s: io_cfg_used_bytes %d, fill_fence %d\n", __func__,
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
	struct cam_isp_kmd_buf_info          *kmd_buf_info)
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
		pr_err("%s:%d Insufficient  HW entries :%d %d\n",
			__func__, __LINE__,
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
				pr_err("%s:%d no free mem %d %d %d\n",
					__func__, __LINE__, base_idx,
					kmd_buf_info->size,
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

