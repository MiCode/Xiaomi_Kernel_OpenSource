/* Copyright (c) 2018, 2019, The Linux Foundation. All rights reserved.
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

#include <linux/ratelimit.h>
#include <linux/slab.h>
#include <uapi/media/cam_isp.h>
#include "cam_io_util.h"
#include "cam_debug_util.h"
#include "cam_cdm_util.h"
#include "cam_hw_intf.h"
#include "cam_ife_hw_mgr.h"
#include "cam_vfe_hw_intf.h"
#include "cam_irq_controller.h"
#include "cam_tasklet_util.h"
#include "cam_vfe_bus.h"
#include "cam_vfe_bus_rd_ver1.h"
#include "cam_vfe_core.h"
#include "cam_debug_util.h"
#include "cam_cpas_api.h"

static const char drv_name[] = "vfe_bus_rd";

#define ALIGNUP(value, alignment) \
	((value + alignment - 1) / alignment * alignment)

#define MAX_BUF_UPDATE_REG_NUM   \
	(sizeof(struct cam_vfe_bus_rd_ver1_reg_offset_bus_client)/4)

#define MAX_REG_VAL_PAIR_SIZE    \
	(MAX_BUF_UPDATE_REG_NUM * 2 * CAM_PACKET_MAX_PLANES)

#define CAM_VFE_ADD_REG_VAL_PAIR(buf_array, index, offset, val)    \
	do {                                               \
		buf_array[(index)++] = offset;             \
		buf_array[(index)++] = val;                \
	} while (0)

enum cam_vfe_bus_rd_ver1_unpacker_format {
	BUS_RD_VER1_PACKER_FMT_PLAIN_128                   = 0x0,
	BUS_RD_VER1_PACKER_FMT_PLAIN_8                     = 0x1,
	BUS_RD_VER1_PACKER_FMT_PLAIN_16_10BPP              = 0x2,
	BUS_RD_VER1_PACKER_FMT_PLAIN_16_12BPP              = 0x3,
	BUS_RD_VER1_PACKER_FMT_PLAIN_16_14BPP              = 0x4,
	BUS_RD_VER1_PACKER_FMT_PLAIN_16_16BPP              = 0x5,
	BUS_RD_VER1_PACKER_FMT_ARGB_10                     = 0x6,
	BUS_RD_VER1_PACKER_FMT_ARGB_12                     = 0x7,
	BUS_RD_VER1_PACKER_FMT_ARGB_14                     = 0x8,
	BUS_RD_VER1_PACKER_FMT_PLAIN_32_20BPP              = 0x9,
	BUS_RD_VER1_PACKER_FMT_PLAIN_64                    = 0xA,
	BUS_RD_VER1_PACKER_FMT_TP_10                       = 0xB,
	BUS_RD_VER1_PACKER_FMT_PLAIN_32_32BPP              = 0xC,
	BUS_RD_VER1_PACKER_FMT_PLAIN_8_ODD_EVEN            = 0xD,
	BUS_RD_VER1_PACKER_FMT_PLAIN_8_LSB_MSB_10          = 0xE,
	BUS_RD_VER1_PACKER_FMT_PLAIN_8_LSB_MSB_10_ODD_EVEN = 0xF,
	BUS_RD_VER1_PACKER_FMT_MAX                         = 0xF,
};

struct cam_vfe_bus_rd_ver1_common_data {
	uint32_t                                    core_index;
	void __iomem                               *mem_base;
	struct cam_hw_intf                         *hw_intf;
	void                                       *bus_irq_controller;
	void                                       *vfe_irq_controller;
	struct cam_vfe_bus_rd_ver1_reg_offset_common  *common_reg;
	uint32_t                                    io_buf_update[
		MAX_REG_VAL_PAIR_SIZE];

	struct list_head                            free_payload_list;
	spinlock_t                                  spin_lock;
	struct mutex                                bus_mutex;
	uint32_t                                    secure_mode;
	uint32_t                                    num_sec_out;
	uint32_t                                    fs_sync_enable;
	uint32_t                                    go_cmd_sel;
};

struct cam_vfe_bus_rd_ver1_rm_resource_data {
	uint32_t             index;
	struct cam_vfe_bus_rd_ver1_common_data            *common_data;
	struct cam_vfe_bus_rd_ver1_reg_offset_bus_client  *hw_regs;
	void                *ctx;

	uint32_t             irq_enabled;
	uint32_t             offset;

	uint32_t             min_vbi;
	uint32_t             fs_mode;
	uint32_t             hbi_count;
	uint32_t             width;
	uint32_t             height;
	uint32_t             stride;
	uint32_t             format;
	uint32_t             latency_buf_allocation;
	uint32_t             unpacker_cfg;
	uint32_t             burst_len;

	uint32_t             go_cmd_sel;
	uint32_t             fs_sync_enable;
	uint32_t             fs_line_sync_en;

	uint32_t             en_cfg;
	uint32_t             is_dual;
	uint32_t             img_addr;
	uint32_t             input_if_cmd;
};

struct cam_vfe_bus_rd_ver1_vfe_bus_rd_data {
	uint32_t                              bus_rd_type;
	struct cam_vfe_bus_rd_ver1_common_data  *common_data;

	uint32_t                         num_rm;
	struct cam_isp_resource_node    *rm_res[PLANE_MAX];

	struct cam_isp_resource_node    *comp_grp;
	enum cam_isp_hw_sync_mode        dual_comp_sync_mode;
	uint32_t                         dual_hw_alternate_vfe_id;
	struct list_head                 vfe_bus_rd_list;

	uint32_t                         format;
	uint32_t                         max_width;
	uint32_t                         max_height;
	struct cam_cdm_utils_ops        *cdm_util_ops;
	uint32_t                         secure_mode;
};

struct cam_vfe_bus_rd_ver1_priv {
	struct cam_vfe_bus_rd_ver1_common_data common_data;
	uint32_t                            num_client;
	uint32_t                            num_bus_rd_resc;

	struct cam_isp_resource_node  bus_client[
		CAM_VFE_BUS_RD_VER1_MAX_CLIENTS];
	struct cam_isp_resource_node  vfe_bus_rd[
		CAM_VFE_BUS_RD_VER1_VFE_BUSRD_MAX];

	uint32_t                            irq_handle;
	uint32_t                            error_irq_handle;
};

static int cam_vfe_bus_process_cmd(
	struct cam_isp_resource_node *priv,
	uint32_t cmd_type, void *cmd_args, uint32_t arg_size);

static enum cam_vfe_bus_rd_ver1_unpacker_format
	cam_vfe_bus_get_unpacker_fmt(uint32_t unpack_fmt)
{
	switch (unpack_fmt) {
	case CAM_FORMAT_MIPI_RAW_10:
		return BUS_RD_VER1_PACKER_FMT_PLAIN_8_ODD_EVEN;
	default:
		return BUS_RD_VER1_PACKER_FMT_MAX;
	}
}

static bool cam_vfe_bus_can_be_secure(uint32_t out_type)
{
	switch (out_type) {
	case CAM_VFE_BUS_RD_VER1_VFE_BUSRD_RDI0:
		return false;

	default:
		return false;
	}
}

static enum cam_vfe_bus_rd_ver1_vfe_bus_rd_type
	cam_vfe_bus_get_bus_rd_res_id(uint32_t res_type)
{
	switch (res_type) {
	case CAM_ISP_RESOURCE_VFE_BUS_RD:
		return CAM_VFE_BUS_RD_VER1_VFE_BUSRD_RDI0;
	default:
		return CAM_VFE_BUS_RD_VER1_VFE_BUSRD_MAX;
	}
}

static int cam_vfe_bus_get_num_rm(
	enum cam_vfe_bus_rd_ver1_vfe_bus_rd_type res_type)
{
	switch (res_type) {
	case CAM_VFE_BUS_RD_VER1_VFE_BUSRD_RDI0:
		return 1;
	default:
		break;
	}

	CAM_ERR(CAM_ISP, "Unsupported resource_type %u",
		res_type);
	return -EINVAL;
}

static int cam_vfe_bus_get_rm_idx(
	enum cam_vfe_bus_rd_ver1_vfe_bus_rd_type vfe_bus_rd_res_id,
	enum cam_vfe_bus_plane_type plane)
{
	int rm_idx = -1;

	switch (vfe_bus_rd_res_id) {
	case CAM_VFE_BUS_RD_VER1_VFE_BUSRD_RDI0:
		switch (plane) {
		case PLANE_Y:
			rm_idx = 0;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}

	return rm_idx;
}

static int cam_vfe_bus_acquire_rm(
	struct cam_vfe_bus_rd_ver1_priv          *ver1_bus_rd_priv,
	struct cam_isp_out_port_info             *out_port_info,
	void                                     *tasklet,
	void                                     *ctx,
	enum cam_vfe_bus_rd_ver1_vfe_bus_rd_type  vfe_bus_rd_res_id,
	enum cam_vfe_bus_plane_type               plane,
	uint32_t                                  subscribe_irq,
	struct cam_isp_resource_node            **rm_res,
	uint32_t                                 *client_done_mask,
	uint32_t                                  is_dual)
{
	uint32_t rm_idx = 0;
	struct cam_isp_resource_node              *rm_res_local = NULL;
	struct cam_vfe_bus_rd_ver1_rm_resource_data *rsrc_data = NULL;

	*rm_res = NULL;
	*client_done_mask = 0;

	/* No need to allocate for BUS VER2. VFE OUT to RM is fixed. */
	rm_idx = cam_vfe_bus_get_rm_idx(vfe_bus_rd_res_id, plane);
	if (rm_idx < 0 || rm_idx >= ver1_bus_rd_priv->num_client) {
		CAM_ERR(CAM_ISP, "Unsupported VFE out %d plane %d",
			vfe_bus_rd_res_id, plane);
		return -EINVAL;
	}

	rm_res_local = &ver1_bus_rd_priv->bus_client[rm_idx];
	if (rm_res_local->res_state != CAM_ISP_RESOURCE_STATE_AVAILABLE) {
		CAM_ERR(CAM_ISP, "RM res not available state:%d",
			rm_res_local->res_state);
		return -EALREADY;
	}
	rm_res_local->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;
	rm_res_local->tasklet_info = tasklet;

	rsrc_data = rm_res_local->res_priv;
	rsrc_data->irq_enabled = subscribe_irq;
	rsrc_data->ctx = ctx;
	rsrc_data->is_dual = is_dual;
	/* Set RM offset value to default */
	rsrc_data->offset  = 0;

	*client_done_mask = (1 << rm_idx);
	*rm_res = rm_res_local;

	CAM_DBG(CAM_ISP, "RM %d: Acquired");
	return 0;
}

static int cam_vfe_bus_release_rm(void   *bus_priv,
	struct cam_isp_resource_node     *rm_res)
{
	struct cam_vfe_bus_rd_ver1_rm_resource_data *rsrc_data =
		rm_res->res_priv;

	rsrc_data->irq_enabled = 0;
	rsrc_data->offset = 0;
	rsrc_data->width = 0;
	rsrc_data->height = 0;
	rsrc_data->stride = 0;
	rsrc_data->format = 0;
	rsrc_data->unpacker_cfg = 0;
	rsrc_data->burst_len = 0;
	rsrc_data->en_cfg = 0;
	rsrc_data->is_dual = 0;

	rm_res->tasklet_info = NULL;
	rm_res->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;

	return 0;
}

static int cam_vfe_bus_start_rm(struct cam_isp_resource_node *rm_res)
{
	int rc = 0;
	struct cam_vfe_bus_rd_ver1_rm_resource_data *rm_data =
		rm_res->res_priv;
	struct cam_vfe_bus_rd_ver1_common_data        *common_data =
		rm_data->common_data;
	uint32_t buf_size;
	uint32_t val;
	uint32_t offset;

	CAM_DBG(CAM_ISP, "w: 0x%x", rm_data->width);
	CAM_DBG(CAM_ISP, "h: 0x%x", rm_data->height);
	CAM_DBG(CAM_ISP, "format: 0x%x", rm_data->format);
	CAM_DBG(CAM_ISP, "unpacker_cfg: 0x%x", rm_data->unpacker_cfg);
	CAM_DBG(CAM_ISP, "latency_buf_allocation: 0x%x",
		rm_data->latency_buf_allocation);
	CAM_DBG(CAM_ISP, "stride: 0x%x", rm_data->stride);
	CAM_DBG(CAM_ISP, "go_cmd_sel: 0x%x", rm_data->go_cmd_sel);
	CAM_DBG(CAM_ISP, "fs_sync_enable: 0x%x", rm_data->fs_sync_enable);
	CAM_DBG(CAM_ISP, "hbi_count: 0x%x", rm_data->hbi_count);
	CAM_DBG(CAM_ISP, "fs_line_sync_en: 0x%x", rm_data->fs_line_sync_en);
	CAM_DBG(CAM_ISP, "fs_mode: 0x%x", rm_data->fs_mode);
	CAM_DBG(CAM_ISP, "min_vbi: 0x%x", rm_data->min_vbi);

	/* Write All the values*/
	offset = rm_data->hw_regs->buffer_width_cfg;
	buf_size = ((rm_data->width)&(0x0000FFFF)) |
		((rm_data->height<<16)&(0xFFFF0000));
	cam_io_w_mb(buf_size, common_data->mem_base + offset);
	CAM_DBG(CAM_ISP, "buf_size: 0x%x", buf_size);

	val = rm_data->width;
	offset = rm_data->hw_regs->stride;
	CAM_DBG(CAM_ISP, "offset:0x%x, value:0x%x", offset, val);
	cam_io_w_mb(val, common_data->mem_base + offset);

	CAM_DBG(CAM_ISP, "rm_data->unpacker_cfg:0x%x", rm_data->unpacker_cfg);
	val = cam_vfe_bus_get_unpacker_fmt(rm_data->unpacker_cfg);
	CAM_DBG(CAM_ISP, " value:0x%x", val);
	offset = rm_data->hw_regs->unpacker_cfg;
	CAM_DBG(CAM_ISP, "offset:0x%x, value:0x%x", offset, val);
	cam_io_w_mb(val, common_data->mem_base + offset);

	val = rm_data->latency_buf_allocation;
	offset = rm_data->hw_regs->latency_buf_allocation;
	CAM_DBG(CAM_ISP, "offset:0x%x, value:0x%x", offset, val);
	cam_io_w_mb(val, common_data->mem_base + offset);

	cam_io_w_mb(0x1, common_data->mem_base +
		rm_data->hw_regs->cfg);
	return rc;
}

static int cam_vfe_bus_stop_rm(struct cam_isp_resource_node *rm_res)
{
	int rc = 0;
	struct cam_vfe_bus_rd_ver1_rm_resource_data *rsrc_data =
		rm_res->res_priv;
	struct cam_vfe_bus_rd_ver1_common_data        *common_data =
		rsrc_data->common_data;

	/* Disable RM */
	cam_io_w_mb(0x0,
		common_data->mem_base + rsrc_data->hw_regs->cfg);

	rm_res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;

	return rc;
}

static int cam_vfe_bus_init_rm_resource(uint32_t index,
	struct cam_vfe_bus_rd_ver1_priv   *ver1_bus_rd_priv,
	struct cam_vfe_bus_rd_ver1_hw_info *bus_rd_hw_info,
	struct cam_isp_resource_node    *rm_res)
{
	struct cam_vfe_bus_rd_ver1_rm_resource_data *rsrc_data;

	rsrc_data = kzalloc(sizeof(struct cam_vfe_bus_rd_ver1_rm_resource_data),
		GFP_KERNEL);
	if (!rsrc_data) {
		CAM_DBG(CAM_ISP, "Failed to alloc for RM res priv");
		return -ENOMEM;
	}
	rm_res->res_priv = rsrc_data;

	rsrc_data->index = index;
	rsrc_data->hw_regs = &bus_rd_hw_info->bus_client_reg[index];
	rsrc_data->common_data = &ver1_bus_rd_priv->common_data;

	rm_res->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;
	INIT_LIST_HEAD(&rm_res->list);

	rm_res->start = cam_vfe_bus_start_rm;
	rm_res->stop = cam_vfe_bus_stop_rm;
	rm_res->hw_intf = ver1_bus_rd_priv->common_data.hw_intf;


	return 0;
}

static int cam_vfe_bus_deinit_rm_resource(
	struct cam_isp_resource_node    *rm_res)
{
	struct cam_vfe_bus_rd_ver1_rm_resource_data *rsrc_data;

	rm_res->res_state = CAM_ISP_RESOURCE_STATE_UNAVAILABLE;
	INIT_LIST_HEAD(&rm_res->list);

	rm_res->start = NULL;
	rm_res->stop = NULL;
	rm_res->top_half_handler = NULL;
	rm_res->bottom_half_handler = NULL;
	rm_res->hw_intf = NULL;

	rsrc_data = rm_res->res_priv;
	rm_res->res_priv = NULL;
	if (!rsrc_data)
		return -ENOMEM;
	kfree(rsrc_data);

	return 0;
}

static int cam_vfe_bus_rd_get_secure_mode(void *priv, void *cmd_args,
	uint32_t arg_size)
{
	return 0;
}

static int cam_vfe_bus_acquire_vfe_bus_rd(void *bus_priv, void *acquire_args,
	uint32_t args_size)
{
	int                                           rc = -ENODEV;
	int                                           i;
	enum cam_vfe_bus_rd_ver1_vfe_bus_rd_type      bus_rd_res_id;
	int                                           num_rm;
	uint32_t                                      subscribe_irq;
	uint32_t                                      client_done_mask;
	struct cam_vfe_bus_rd_ver1_priv              *ver1_bus_rd_priv =
		bus_priv;
	struct cam_vfe_acquire_args                  *acq_args = acquire_args;
	struct cam_vfe_hw_vfe_out_acquire_args       *bus_rd_acquire_args;
	struct cam_isp_resource_node                 *rsrc_node = NULL;
	struct cam_vfe_bus_rd_ver1_vfe_bus_rd_data   *rsrc_data = NULL;
	uint32_t                                      secure_caps = 0, mode;

	if (!bus_priv || !acquire_args) {
		CAM_ERR(CAM_ISP, "Invalid Param");
		return -EINVAL;
	}

	bus_rd_acquire_args = &acq_args->vfe_bus_rd;

	CAM_DBG(CAM_ISP, "Acquiring resource type 0x%x",
		acq_args->rsrc_type);

	bus_rd_res_id = cam_vfe_bus_get_bus_rd_res_id(
		acq_args->rsrc_type);
	if (bus_rd_res_id == CAM_VFE_BUS_RD_VER1_VFE_BUSRD_MAX)
		return -ENODEV;

	num_rm = cam_vfe_bus_get_num_rm(bus_rd_res_id);
	if (num_rm < 1)
		return -EINVAL;

	rsrc_node = &ver1_bus_rd_priv->vfe_bus_rd[bus_rd_res_id];
	if (rsrc_node->res_state != CAM_ISP_RESOURCE_STATE_AVAILABLE) {
		CAM_ERR(CAM_ISP, "Resource not available: Res_id %d state:%d",
			bus_rd_res_id, rsrc_node->res_state);
		return -EBUSY;
	}

	rsrc_data = rsrc_node->res_priv;
	secure_caps = cam_vfe_bus_can_be_secure(
		rsrc_data->bus_rd_type);

	mode = bus_rd_acquire_args->out_port_info->secure_mode;
	mutex_lock(&rsrc_data->common_data->bus_mutex);
	if (secure_caps) {
		if (!rsrc_data->common_data->num_sec_out) {
			rsrc_data->secure_mode = mode;
			rsrc_data->common_data->secure_mode = mode;
		} else {
			if (mode == rsrc_data->common_data->secure_mode) {
				rsrc_data->secure_mode =
					rsrc_data->common_data->secure_mode;
			} else {
				rc = -EINVAL;
				CAM_ERR_RATE_LIMIT(CAM_ISP,
					"Mismatch: Acquire mode[%d], drvr mode[%d]",
					rsrc_data->common_data->secure_mode,
					mode);
				mutex_unlock(
					&rsrc_data->common_data->bus_mutex);
				return -EINVAL;
			}
		}
		rsrc_data->common_data->num_sec_out++;
	}
	mutex_unlock(&rsrc_data->common_data->bus_mutex);

	rsrc_data->num_rm = num_rm;
	rsrc_node->tasklet_info = acq_args->tasklet;
	rsrc_node->cdm_ops = bus_rd_acquire_args->cdm_ops;
	rsrc_data->cdm_util_ops = bus_rd_acquire_args->cdm_ops;

	subscribe_irq = 1;

	for (i = 0; i < num_rm; i++) {
		rc = cam_vfe_bus_acquire_rm(ver1_bus_rd_priv,
			bus_rd_acquire_args->out_port_info,
			acq_args->tasklet,
			bus_rd_acquire_args->ctx,
			bus_rd_res_id,
			i,
			subscribe_irq,
			&rsrc_data->rm_res[i],
			&client_done_mask,
			bus_rd_acquire_args->is_dual);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"VFE%d RM acquire failed for Out %d rc=%d",
				rsrc_data->common_data->core_index,
				bus_rd_res_id, rc);
			goto release_rm;
		}
	}

	rsrc_node->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;
	bus_rd_acquire_args->rsrc_node = rsrc_node;

	CAM_DBG(CAM_ISP, "Acquire successful");
	return rc;

release_rm:
	for (i--; i >= 0; i--)
		cam_vfe_bus_release_rm(ver1_bus_rd_priv, rsrc_data->rm_res[i]);
	return rc;
}

static int cam_vfe_bus_release_vfe_bus_rd(void *bus_priv, void *release_args,
	uint32_t args_size)
{
	uint32_t i;
	struct cam_isp_resource_node          *vfe_bus_rd = NULL;
	struct cam_vfe_bus_rd_ver1_vfe_bus_rd_data  *rsrc_data = NULL;
	uint32_t                               secure_caps = 0;

	if (!bus_priv || !release_args) {
		CAM_ERR(CAM_ISP, "Invalid input bus_priv %pK release_args %pK",
			bus_priv, release_args);
		return -EINVAL;
	}

	vfe_bus_rd = release_args;
	rsrc_data = vfe_bus_rd->res_priv;

	if (vfe_bus_rd->res_state != CAM_ISP_RESOURCE_STATE_RESERVED) {
		CAM_ERR(CAM_ISP, "Invalid resource state:%d",
			vfe_bus_rd->res_state);
	}

	for (i = 0; i < rsrc_data->num_rm; i++)
		cam_vfe_bus_release_rm(bus_priv, rsrc_data->rm_res[i]);
	rsrc_data->num_rm = 0;

	vfe_bus_rd->tasklet_info = NULL;
	vfe_bus_rd->cdm_ops = NULL;
	rsrc_data->cdm_util_ops = NULL;

	secure_caps = cam_vfe_bus_can_be_secure(rsrc_data->bus_rd_type);
	mutex_lock(&rsrc_data->common_data->bus_mutex);
	if (secure_caps) {
		if (rsrc_data->secure_mode ==
			rsrc_data->common_data->secure_mode) {
			rsrc_data->common_data->num_sec_out--;
			rsrc_data->secure_mode =
				CAM_SECURE_MODE_NON_SECURE;
		} else {
			/*
			 * The validity of the mode is properly
			 * checked while acquiring the output port.
			 * not expected to reach here, unless there is
			 * some corruption.
			 */
			CAM_ERR(CAM_ISP, "driver[%d],resource[%d] mismatch",
				rsrc_data->common_data->secure_mode,
				rsrc_data->secure_mode);
		}

		if (!rsrc_data->common_data->num_sec_out)
			rsrc_data->common_data->secure_mode =
				CAM_SECURE_MODE_NON_SECURE;
	}
	mutex_unlock(&rsrc_data->common_data->bus_mutex);

	if (vfe_bus_rd->res_state == CAM_ISP_RESOURCE_STATE_RESERVED)
		vfe_bus_rd->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;

	return 0;
}

static int cam_vfe_bus_start_vfe_bus_rd(
	struct cam_isp_resource_node          *vfe_out)
{
	int rc = 0, i;
	struct cam_vfe_bus_rd_ver1_vfe_bus_rd_data  *rsrc_data = NULL;
	struct cam_vfe_bus_rd_ver1_common_data   *common_data = NULL;

	if (!vfe_out) {
		CAM_ERR(CAM_ISP, "Invalid input");
		return -EINVAL;
	}

	rsrc_data = vfe_out->res_priv;
	common_data = rsrc_data->common_data;

	CAM_DBG(CAM_ISP, "Start resource type: %x", rsrc_data->bus_rd_type);

	if (vfe_out->res_state != CAM_ISP_RESOURCE_STATE_RESERVED) {
		CAM_ERR(CAM_ISP, "Invalid resource state:%d",
			vfe_out->res_state);
		return -EACCES;
	}

	for (i = 0; i < rsrc_data->num_rm; i++)
		rc = cam_vfe_bus_start_rm(rsrc_data->rm_res[i]);
	vfe_out->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;
	return rc;
}

static int cam_vfe_bus_stop_vfe_bus_rd(
	struct cam_isp_resource_node          *vfe_bus_rd)
{
	int rc = 0, i;
	struct cam_vfe_bus_rd_ver1_vfe_bus_rd_data  *rsrc_data = NULL;

	CAM_DBG(CAM_ISP, "E:Stop rd Res");
	if (!vfe_bus_rd) {
		CAM_ERR(CAM_ISP, "Invalid input");
		return -EINVAL;
	}

	rsrc_data = vfe_bus_rd->res_priv;

	if (vfe_bus_rd->res_state == CAM_ISP_RESOURCE_STATE_AVAILABLE ||
		vfe_bus_rd->res_state == CAM_ISP_RESOURCE_STATE_RESERVED) {
		CAM_DBG(CAM_ISP, "vfe_out res_state is %d",
			vfe_bus_rd->res_state);
		return rc;
	}
	for (i = 0; i < rsrc_data->num_rm; i++)
		rc = cam_vfe_bus_stop_rm(rsrc_data->rm_res[i]);

	vfe_bus_rd->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;
	return rc;
}

static int cam_vfe_bus_init_vfe_bus_read_resource(uint32_t  index,
	struct cam_vfe_bus_rd_ver1_priv                  *bus_rd_priv,
	struct cam_vfe_bus_rd_ver1_hw_info               *bus_rd_hw_info)
{
	struct cam_isp_resource_node         *vfe_bus_rd = NULL;
	struct cam_vfe_bus_rd_ver1_vfe_bus_rd_data *rsrc_data = NULL;
	int rc = 0;
	int32_t vfe_bus_rd_resc_type =
		bus_rd_hw_info->vfe_bus_rd_hw_info[index].vfe_bus_rd_type;

	if (vfe_bus_rd_resc_type < 0 ||
		vfe_bus_rd_resc_type > CAM_VFE_BUS_RD_VER1_VFE_BUSRD_RDI0) {
		CAM_ERR(CAM_ISP, "Init VFE Out failed, Invalid type=%d",
			vfe_bus_rd_resc_type);
		return -EINVAL;
	}

	vfe_bus_rd = &bus_rd_priv->vfe_bus_rd[vfe_bus_rd_resc_type];
	if (vfe_bus_rd->res_state != CAM_ISP_RESOURCE_STATE_UNAVAILABLE ||
		vfe_bus_rd->res_priv) {
		CAM_ERR(CAM_ISP,
			"Error. Looks like same resource is init again");
		return -EFAULT;
	}

	rsrc_data = kzalloc(sizeof(struct cam_vfe_bus_rd_ver1_vfe_bus_rd_data),
		GFP_KERNEL);
	if (!rsrc_data) {
		rc = -ENOMEM;
		return rc;
	}

	vfe_bus_rd->res_priv = rsrc_data;

	vfe_bus_rd->res_type = CAM_ISP_RESOURCE_VFE_BUS_RD;
	vfe_bus_rd->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;
	INIT_LIST_HEAD(&vfe_bus_rd->list);

	rsrc_data->bus_rd_type    =
		bus_rd_hw_info->vfe_bus_rd_hw_info[index].vfe_bus_rd_type;
	rsrc_data->common_data = &bus_rd_priv->common_data;
	rsrc_data->max_width   =
		bus_rd_hw_info->vfe_bus_rd_hw_info[index].max_width;
	rsrc_data->max_height  =
		bus_rd_hw_info->vfe_bus_rd_hw_info[index].max_height;
	rsrc_data->secure_mode = CAM_SECURE_MODE_NON_SECURE;

	vfe_bus_rd->start = cam_vfe_bus_start_vfe_bus_rd;
	vfe_bus_rd->stop = cam_vfe_bus_stop_vfe_bus_rd;
	vfe_bus_rd->process_cmd = cam_vfe_bus_process_cmd;
	vfe_bus_rd->hw_intf = bus_rd_priv->common_data.hw_intf;

	return 0;
}

static int cam_vfe_bus_deinit_vfe_bus_rd_resource(
	struct cam_isp_resource_node    *vfe_bus_rd_res)
{
	struct cam_vfe_bus_ver2_vfe_out_data *rsrc_data =
		vfe_bus_rd_res->res_priv;

	if (vfe_bus_rd_res->res_state == CAM_ISP_RESOURCE_STATE_UNAVAILABLE) {
		/*
		 * This is not error. It can happen if the resource is
		 * never supported in the HW.
		 */
		CAM_DBG(CAM_ISP, "HW%d Res %d already deinitialized");
		return 0;
	}

	vfe_bus_rd_res->start = NULL;
	vfe_bus_rd_res->stop = NULL;
	vfe_bus_rd_res->top_half_handler = NULL;
	vfe_bus_rd_res->bottom_half_handler = NULL;
	vfe_bus_rd_res->hw_intf = NULL;

	vfe_bus_rd_res->res_state = CAM_ISP_RESOURCE_STATE_UNAVAILABLE;
	INIT_LIST_HEAD(&vfe_bus_rd_res->list);
	vfe_bus_rd_res->res_priv = NULL;

	if (!rsrc_data)
		return -ENOMEM;
	kfree(rsrc_data);

	return 0;
}

static int cam_vfe_bus_rd_ver1_handle_irq(uint32_t    evt_id,
	struct cam_irq_th_payload                 *th_payload)
{
	struct cam_vfe_bus_rd_ver1_priv          *bus_priv;

	bus_priv     = th_payload->handler_priv;
	CAM_DBG(CAM_ISP, "BUS READ IRQ Received");
	return 0;
}

static int cam_vfe_bus_rd_update_rm(void *priv, void *cmd_args,
	uint32_t arg_size)
{
	struct cam_vfe_bus_rd_ver1_priv          *bus_priv;
	struct cam_isp_hw_get_cmd_update         *update_buf;
	struct cam_buf_io_cfg                    *io_cfg;
	struct cam_vfe_bus_rd_ver1_vfe_bus_rd_data     *vfe_bus_rd_data = NULL;
	struct cam_vfe_bus_rd_ver1_rm_resource_data *rm_data = NULL;
	uint32_t *reg_val_pair;
	uint32_t  i, j, size = 0;
	uint32_t  val;
	uint32_t buf_size = 0;

	bus_priv = (struct cam_vfe_bus_rd_ver1_priv  *) priv;
	update_buf =  (struct cam_isp_hw_get_cmd_update *) cmd_args;

	vfe_bus_rd_data = (struct cam_vfe_bus_rd_ver1_vfe_bus_rd_data *)
		update_buf->res->res_priv;

	if (!vfe_bus_rd_data || !vfe_bus_rd_data->cdm_util_ops) {
		CAM_ERR(CAM_ISP, "Failed! Invalid data");
		return -EINVAL;
	}

	CAM_DBG(CAM_ISP, "#of RM: %d",  vfe_bus_rd_data->num_rm);
	if (update_buf->rm_update->num_buf != vfe_bus_rd_data->num_rm) {
		CAM_ERR(CAM_ISP,
			"Failed! Invalid number buffers:%d required:%d",
			update_buf->rm_update->num_buf,
			vfe_bus_rd_data->num_rm);
		return -EINVAL;
	}

	reg_val_pair = &vfe_bus_rd_data->common_data->io_buf_update[0];
	io_cfg = update_buf->rm_update->io_cfg;

	for (i = 0, j = 0; i < vfe_bus_rd_data->num_rm; i++) {
		if (j >= (MAX_REG_VAL_PAIR_SIZE - MAX_BUF_UPDATE_REG_NUM * 2)) {
			CAM_ERR(CAM_ISP,
				"reg_val_pair %d exceeds the array limit %lu",
				j, MAX_REG_VAL_PAIR_SIZE);
			return -ENOMEM;
		}

		rm_data = vfe_bus_rd_data->rm_res[i]->res_priv;

		/* update size register */
		rm_data->width = io_cfg->planes[i].width;
		rm_data->height = io_cfg->planes[i].height;
		CAM_DBG(CAM_ISP, "RM %d image w 0x%x h 0x%x image size 0x%x",
			rm_data->index, rm_data->width, rm_data->height,
			buf_size);

		buf_size = ((rm_data->width)&(0x0000FFFF)) |
			((rm_data->height<<16)&(0xFFFF0000));

		CAM_DBG(CAM_ISP, "size offset 0x%x buf_size 0x%x",
			rm_data->hw_regs->buf_size, buf_size);
		CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
			rm_data->hw_regs->buffer_width_cfg,
			buf_size);
		CAM_DBG(CAM_ISP, "RM %d image size 0x%x",
			rm_data->index, reg_val_pair[j-1]);

		val = rm_data->width;
		CAM_DBG(CAM_ISP, "io_cfg stride 0x%x", val);
		CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
			rm_data->hw_regs->stride,
			val);
		rm_data->stride = val;
		CAM_DBG(CAM_ISP, "RM %d image stride 0x%x",
			rm_data->index, reg_val_pair[j-1]);

		/* RM Image address */
		CAM_DBG(CAM_ISP, "image_addr offset %x",
			rm_data->hw_regs->image_addr);
		CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
			rm_data->hw_regs->image_addr,
			update_buf->rm_update->image_buf[i] +
				rm_data->offset);
		CAM_DBG(CAM_ISP, "RM %d image address 0x%x",
			rm_data->index, reg_val_pair[j-1]);
		rm_data->img_addr = reg_val_pair[j-1];

	}

	size = vfe_bus_rd_data->cdm_util_ops->cdm_required_size_reg_random(j/2);

	/* cdm util returns dwords, need to convert to bytes */
	if ((size * 4) > update_buf->cmd.size) {
		CAM_ERR(CAM_ISP,
			"Failed! Buf size:%d insufficient, expected size:%d",
			update_buf->cmd.size, size);
		return -ENOMEM;
	}

	vfe_bus_rd_data->cdm_util_ops->cdm_write_regrandom(
		update_buf->cmd.cmd_buf_addr, j/2, reg_val_pair);

	/* cdm util returns dwords, need to convert to bytes */
	update_buf->cmd.used_bytes = size * 4;

	return 0;
}

static int cam_vfe_bus_rd_update_hfr(void *priv, void *cmd_args,
	uint32_t arg_size)
{
	return 0;
}

static int cam_vfe_bus_rd_update_fs_cfg(void *priv, void *cmd_args,
	uint32_t arg_size)
{
	struct cam_vfe_bus_rd_ver1_priv              *bus_priv;
	struct cam_vfe_bus_rd_ver1_vfe_bus_rd_data   *vfe_bus_rd_data = NULL;
	struct cam_vfe_bus_rd_ver1_rm_resource_data  *rm_data = NULL;
	struct cam_vfe_fe_update_args                *fe_upd_args;
	struct cam_fe_config                         *fe_cfg;
	struct cam_vfe_bus_rd_ver1_common_data        *common_data;
	int i = 0;

	bus_priv = (struct cam_vfe_bus_rd_ver1_priv  *) priv;
	fe_upd_args = (struct cam_vfe_fe_update_args *)cmd_args;

	vfe_bus_rd_data = (struct cam_vfe_bus_rd_ver1_vfe_bus_rd_data *)
		fe_upd_args->node_res->res_priv;

	if (!vfe_bus_rd_data || !vfe_bus_rd_data->cdm_util_ops) {
		CAM_ERR(CAM_ISP, "Failed! Invalid data");
		return -EINVAL;
	}

	fe_cfg = &fe_upd_args->fe_config;

	for (i = 0; i < vfe_bus_rd_data->num_rm; i++) {

		rm_data = vfe_bus_rd_data->rm_res[i]->res_priv;
		common_data = rm_data->common_data;

		rm_data->format = fe_cfg->format;
		CAM_DBG(CAM_ISP, "format: 0x%x", rm_data->format);

		rm_data->unpacker_cfg = fe_cfg->unpacker_cfg;
		CAM_DBG(CAM_ISP, "unpacker_cfg: 0x%x", rm_data->unpacker_cfg);

		rm_data->latency_buf_allocation = fe_cfg->latency_buf_size;
		CAM_DBG(CAM_ISP, "latency_buf_allocation: 0x%x",
			rm_data->latency_buf_allocation);

		rm_data->stride = fe_cfg->stride;
		CAM_DBG(CAM_ISP, "stride: 0x%x", rm_data->stride);

		rm_data->go_cmd_sel = fe_cfg->go_cmd_sel;
		CAM_DBG(CAM_ISP, "go_cmd_sel: 0x%x", rm_data->go_cmd_sel);

		rm_data->fs_sync_enable = fe_cfg->fs_sync_enable;
		CAM_DBG(CAM_ISP, "fs_sync_enable: 0x%x",
			rm_data->fs_sync_enable);

		rm_data->hbi_count = fe_cfg->hbi_count;
		CAM_DBG(CAM_ISP, "hbi_count: 0x%x", rm_data->hbi_count);

		rm_data->fs_line_sync_en = fe_cfg->fs_line_sync_en;
		CAM_DBG(CAM_ISP, "fs_line_sync_en: 0x%x",
			rm_data->fs_line_sync_en);

		rm_data->fs_mode = fe_cfg->fs_mode;
		CAM_DBG(CAM_ISP, "fs_mode: 0x%x", rm_data->fs_mode);

		rm_data->min_vbi = fe_cfg->min_vbi;
		CAM_DBG(CAM_ISP, "min_vbi: 0x%x", rm_data->min_vbi);
	}
	bus_priv->common_data.fs_sync_enable = fe_cfg->fs_sync_enable;
	bus_priv->common_data.go_cmd_sel = fe_cfg->go_cmd_sel;
	return 0;
}

static int cam_vfe_bus_start_hw(void *hw_priv,
	void *start_hw_args, uint32_t arg_size)
{
	return cam_vfe_bus_start_vfe_bus_rd(hw_priv);
}

static int cam_vfe_bus_stop_hw(void *hw_priv,
	void *stop_hw_args, uint32_t arg_size)
{
	return cam_vfe_bus_stop_vfe_bus_rd(hw_priv);
}

static int cam_vfe_bus_init_hw(void *hw_priv,
	void *init_hw_args, uint32_t arg_size)
{
	struct cam_vfe_bus_rd_ver1_priv    *bus_priv = hw_priv;
	uint32_t                            top_irq_reg_mask[2] = {0};
	uint32_t                            offset = 0, val = 0;
	struct cam_vfe_bus_rd_ver1_reg_offset_common  *common_reg;

	if (!bus_priv) {
		CAM_ERR(CAM_ISP, "Invalid args");
		return -EINVAL;
	}
	common_reg = bus_priv->common_data.common_reg;
	top_irq_reg_mask[0] = (1 << 23);

	bus_priv->irq_handle = cam_irq_controller_subscribe_irq(
		bus_priv->common_data.vfe_irq_controller,
		CAM_IRQ_PRIORITY_2,
		top_irq_reg_mask,
		bus_priv,
		cam_vfe_bus_rd_ver1_handle_irq,
		NULL,
		NULL,
		NULL);

	if (bus_priv->irq_handle <= 0) {
		CAM_ERR(CAM_ISP, "Failed to subscribe BUS IRQ");
		return -EFAULT;
	}
	/* no clock gating at bus input */
	offset = common_reg->cgc_ovd;
	cam_io_w_mb(0x0, bus_priv->common_data.mem_base + offset);

	/* BUS_RD_TEST_BUS_CTRL */
	offset = common_reg->test_bus_ctrl;
	cam_io_w_mb(0x0, bus_priv->common_data.mem_base + offset);

	/* Read irq mask */
	offset = common_reg->irq_reg_info.irq_reg_set->mask_reg_offset;
	cam_io_w_mb(0x5, bus_priv->common_data.mem_base + offset);

	/* INPUT_IF_CMD */
	val = (bus_priv->common_data.fs_sync_enable << 5) |
		(bus_priv->common_data.go_cmd_sel << 4);
	offset = common_reg->input_if_cmd;
	cam_io_w_mb(val, bus_priv->common_data.mem_base + offset);
	return 0;
}

static int cam_vfe_bus_deinit_hw(void *hw_priv,
	void *deinit_hw_args, uint32_t arg_size)
{
	struct cam_vfe_bus_rd_ver1_priv    *bus_priv = hw_priv;
	int                              rc = 0;

	if (!bus_priv) {
		CAM_ERR(CAM_ISP, "Error: Invalid args");
		return -EINVAL;
	}

	if (bus_priv->error_irq_handle) {
		rc = cam_irq_controller_unsubscribe_irq(
			bus_priv->common_data.bus_irq_controller,
			bus_priv->error_irq_handle);
		if (rc)
			CAM_ERR(CAM_ISP,
				"Failed to unsubscribe error irq rc=%d", rc);

		bus_priv->error_irq_handle = 0;
	}

	if (bus_priv->irq_handle) {
		rc = cam_irq_controller_unsubscribe_irq(
			bus_priv->common_data.vfe_irq_controller,
			bus_priv->irq_handle);
		if (rc)
			CAM_ERR(CAM_ISP,
				"Failed to unsubscribe irq rc=%d", rc);

		bus_priv->irq_handle = 0;
	}

	return rc;
}

static int __cam_vfe_bus_process_cmd(void *priv,
	uint32_t cmd_type, void *cmd_args, uint32_t arg_size)
{
	return cam_vfe_bus_process_cmd(priv, cmd_type, cmd_args, arg_size);
}

static int cam_vfe_bus_process_cmd(
	struct cam_isp_resource_node *priv,
	uint32_t cmd_type, void *cmd_args, uint32_t arg_size)
{
	int rc = -EINVAL;

	if (!priv || !cmd_args) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "Invalid input arguments");
		return -EINVAL;
	}

	switch (cmd_type) {
	case CAM_ISP_HW_CMD_GET_BUF_UPDATE_RM:
		rc = cam_vfe_bus_rd_update_rm(priv, cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_GET_HFR_UPDATE_RM:
		rc = cam_vfe_bus_rd_update_hfr(priv, cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_GET_SECURE_MODE:
		rc = cam_vfe_bus_rd_get_secure_mode(priv, cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_FE_UPDATE_BUS_RD:
		rc = cam_vfe_bus_rd_update_fs_cfg(priv, cmd_args, arg_size);
		break;
	default:
		CAM_ERR_RATE_LIMIT(CAM_ISP, "Invalid camif process command:%d",
			cmd_type);
		break;
	}

	return rc;
}

int cam_vfe_bus_rd_ver1_init(
	struct cam_hw_soc_info               *soc_info,
	struct cam_hw_intf                   *hw_intf,
	void                                 *bus_hw_info,
	void                                 *vfe_irq_controller,
	struct cam_vfe_bus                  **vfe_bus)
{
	int i, rc = 0;
	struct cam_vfe_bus_rd_ver1_priv    *bus_priv = NULL;
	struct cam_vfe_bus                 *vfe_bus_local;
	struct cam_vfe_bus_rd_ver1_hw_info *bus_rd_hw_info = bus_hw_info;

	if (!soc_info || !hw_intf || !bus_hw_info || !vfe_irq_controller) {
		CAM_ERR(CAM_ISP,
			"Inval_prms soc_info:%pK hw_intf:%pK hw_info%pK",
			soc_info, hw_intf, bus_rd_hw_info);
		CAM_ERR(CAM_ISP, "controller: %pK", vfe_irq_controller);
		rc = -EINVAL;
		goto end;
	}

	vfe_bus_local = kzalloc(sizeof(struct cam_vfe_bus), GFP_KERNEL);
	if (!vfe_bus_local) {
		CAM_DBG(CAM_ISP, "Failed to alloc for vfe_bus");
		rc = -ENOMEM;
		goto end;
	}

	bus_priv = kzalloc(sizeof(struct cam_vfe_bus_rd_ver1_priv),
		GFP_KERNEL);
	if (!bus_priv) {
		CAM_DBG(CAM_ISP, "Failed to alloc for vfe_bus_priv");
		rc = -ENOMEM;
		goto free_bus_local;
	}

	vfe_bus_local->bus_priv = bus_priv;

	bus_priv->num_client                     = bus_rd_hw_info->num_client;
	bus_priv->num_bus_rd_resc                =
		bus_rd_hw_info->num_bus_rd_resc;
	bus_priv->common_data.num_sec_out        = 0;
	bus_priv->common_data.secure_mode        = CAM_SECURE_MODE_NON_SECURE;
	bus_priv->common_data.core_index         = soc_info->index;
	bus_priv->common_data.mem_base           =
		CAM_SOC_GET_REG_MAP_START(soc_info, VFE_CORE_BASE_IDX);
	bus_priv->common_data.hw_intf            = hw_intf;
	bus_priv->common_data.vfe_irq_controller = vfe_irq_controller;
	bus_priv->common_data.common_reg         = &bus_rd_hw_info->common_reg;

	mutex_init(&bus_priv->common_data.bus_mutex);

	rc = cam_irq_controller_init(drv_name, bus_priv->common_data.mem_base,
		&bus_rd_hw_info->common_reg.irq_reg_info,
		&bus_priv->common_data.bus_irq_controller);
	if (rc) {
		CAM_ERR(CAM_ISP, "cam_irq_controller_init failed");
		goto free_bus_priv;
	}

	for (i = 0; i < bus_priv->num_client; i++) {
		rc = cam_vfe_bus_init_rm_resource(i, bus_priv, bus_hw_info,
			&bus_priv->bus_client[i]);
		if (rc < 0) {
			CAM_ERR(CAM_ISP, "Init RM failed rc=%d", rc);
			goto deinit_rm;
		}
	}

	for (i = 0; i < bus_priv->num_bus_rd_resc; i++) {
		rc = cam_vfe_bus_init_vfe_bus_read_resource(i, bus_priv,
			bus_rd_hw_info);
		if (rc < 0) {
			CAM_ERR(CAM_ISP, "Init VFE Out failed rc=%d", rc);
			goto deinit_vfe_bus_rd;
		}
	}

	spin_lock_init(&bus_priv->common_data.spin_lock);

	vfe_bus_local->hw_ops.reserve      = cam_vfe_bus_acquire_vfe_bus_rd;
	vfe_bus_local->hw_ops.release      = cam_vfe_bus_release_vfe_bus_rd;
	vfe_bus_local->hw_ops.start        = cam_vfe_bus_start_hw;
	vfe_bus_local->hw_ops.stop         = cam_vfe_bus_stop_hw;
	vfe_bus_local->hw_ops.init         = cam_vfe_bus_init_hw;
	vfe_bus_local->hw_ops.deinit       = cam_vfe_bus_deinit_hw;
	vfe_bus_local->top_half_handler    = cam_vfe_bus_rd_ver1_handle_irq;
	vfe_bus_local->bottom_half_handler = NULL;
	vfe_bus_local->hw_ops.process_cmd  = __cam_vfe_bus_process_cmd;

	*vfe_bus = vfe_bus_local;

	return rc;

deinit_vfe_bus_rd:
	if (i < 0)
		i = CAM_VFE_BUS_RD_VER1_VFE_BUSRD_MAX;
	for (--i; i >= 0; i--)
		cam_vfe_bus_deinit_vfe_bus_rd_resource(
			&bus_priv->vfe_bus_rd[i]);
deinit_rm:
	if (i < 0)
		i = bus_priv->num_client;
	for (--i; i >= 0; i--)
		cam_vfe_bus_deinit_rm_resource(&bus_priv->bus_client[i]);

free_bus_priv:
	kfree(vfe_bus_local->bus_priv);

free_bus_local:
	kfree(vfe_bus_local);

end:
	return rc;
}

int cam_vfe_bus_rd_bus_ver1_deinit(
	struct cam_vfe_bus                  **vfe_bus)
{
	int i, rc = 0;
	struct cam_vfe_bus_rd_ver1_priv    *bus_priv = NULL;
	struct cam_vfe_bus                 *vfe_bus_local;

	if (!vfe_bus || !*vfe_bus) {
		CAM_ERR(CAM_ISP, "Invalid input");
		return -EINVAL;
	}
	vfe_bus_local = *vfe_bus;

	bus_priv = vfe_bus_local->bus_priv;
	if (!bus_priv) {
		CAM_ERR(CAM_ISP, "bus_priv is NULL");
		rc = -ENODEV;
		goto free_bus_local;
	}

	for (i = 0; i < bus_priv->num_client; i++) {
		rc = cam_vfe_bus_deinit_rm_resource(&bus_priv->bus_client[i]);
		if (rc < 0)
			CAM_ERR(CAM_ISP,
				"Deinit RM failed rc=%d", rc);
	}
	for (i = 0; i < CAM_VFE_BUS_RD_VER1_VFE_BUSRD_MAX; i++) {
		rc = cam_vfe_bus_deinit_vfe_bus_rd_resource(
			&bus_priv->vfe_bus_rd[i]);
		if (rc < 0)
			CAM_ERR(CAM_ISP,
				"Deinit VFE Out failed rc=%d", rc);
	}

	rc = cam_irq_controller_deinit(
		&bus_priv->common_data.bus_irq_controller);
	if (rc)
		CAM_ERR(CAM_ISP,
			"Deinit IRQ Controller failed rc=%d", rc);

	mutex_destroy(&bus_priv->common_data.bus_mutex);
	kfree(vfe_bus_local->bus_priv);

free_bus_local:
	kfree(vfe_bus_local);

	*vfe_bus = NULL;

	return rc;
}
