// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
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

#define BUS_RD_VER1_DEFAULT_LATENCY_BUF_ALLOC 512

enum cam_vfe_bus_rd_ver1_unpacker_format {
	BUS_RD_VER1_UNPACKER_FMT_PLAIN_128             = 0x0,
	BUS_RD_VER1_UNPACKER_FMT_PLAIN_8               = 0x1,
	BUS_RD_VER1_UNPACKER_FMT_PLAIN_16_10BPP        = 0x2,
	BUS_RD_VER1_UNPACKER_FMT_PLAIN_16_12BPP        = 0x3,
	BUS_RD_VER1_UNPACKER_FMT_PLAIN_16_14BPP        = 0x4,
	BUS_RD_VER1_UNPACKER_FMT_PLAIN_32_20BPP        = 0x5,
	BUS_RD_VER1_UNPACKER_FMT_ARGB_10               = 0x6,
	BUS_RD_VER1_UNPACKER_FMT_ARGB_12               = 0x7,
	BUS_RD_VER1_UNPACKER_FMT_ARGB_14               = 0x8,
	BUS_RD_VER1_UNPACKER_FMT_PLAIN_32              = 0x9,
	BUS_RD_VER1_UNPACKER_FMT_PLAIN_64              = 0xA,
	BUS_RD_VER1_UNPACKER_FMT_TP_10                 = 0xB,
	BUS_RD_VER1_UNPACKER_FMT_MIPI8                 = 0xC,
	BUS_RD_VER1_UNPACKER_FMT_MIPI10                = 0xD,
	BUS_RD_VER1_UNPACKER_FMT_MIPI12                = 0xE,
	BUS_RD_VER1_UNPACKER_FMT_MIPI14                = 0xF,
	BUS_RD_VER1_UNPACKER_FMT_PLAIN_16_16BPP        = 0x10,
	BUS_RD_VER1_UNPACKER_FMT_PLAIN_128_ODD_EVEN    = 0x11,
	BUS_RD_VER1_UNPACKER_FMT_PLAIN_8_ODD_EVEN      = 0x12,
	BUS_RD_VER1_UNPACKER_FMT_MAX                   = 0x13,
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
	cam_hw_mgr_event_cb_func                    event_cb;
};

struct cam_vfe_bus_rd_ver1_rm_resource_data {
	uint32_t             index;
	struct cam_vfe_bus_rd_ver1_common_data            *common_data;
	struct cam_vfe_bus_rd_ver1_reg_offset_bus_client  *hw_regs;
	void                *ctx;

	bool                 init_cfg_done;

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
	int                              irq_handle;
	void                            *priv;
	uint32_t                         status;
	bool                             is_offline;
};

struct cam_vfe_bus_rd_ver1_priv {
	struct cam_vfe_bus_rd_ver1_common_data common_data;
	uint32_t                               num_client;
	uint32_t                               num_bus_rd_resc;

	struct cam_isp_resource_node  bus_client[
		CAM_VFE_BUS_RD_VER1_MAX_CLIENTS];
	struct cam_isp_resource_node  vfe_bus_rd[
		CAM_VFE_BUS_RD_VER1_VFE_BUSRD_MAX];

	int                                 irq_handle;
	void                               *tasklet_info;
	uint32_t                            top_irq_shift;
};

static int cam_vfe_bus_rd_process_cmd(
	struct cam_isp_resource_node *priv,
	uint32_t cmd_type, void *cmd_args, uint32_t arg_size);

static void cam_vfe_bus_rd_pxls_to_bytes(uint32_t pxls, uint32_t fmt,
	uint32_t *bytes)
{
	switch (fmt) {
	case BUS_RD_VER1_UNPACKER_FMT_PLAIN_128:
		*bytes = pxls * 16;
		break;
	case BUS_RD_VER1_UNPACKER_FMT_PLAIN_8:
	case BUS_RD_VER1_UNPACKER_FMT_PLAIN_8_ODD_EVEN:
	case BUS_RD_VER1_UNPACKER_FMT_PLAIN_16_10BPP:
	case BUS_RD_VER1_UNPACKER_FMT_PLAIN_16_12BPP:
	case BUS_RD_VER1_UNPACKER_FMT_PLAIN_16_14BPP:
	case BUS_RD_VER1_UNPACKER_FMT_PLAIN_16_16BPP:
	case BUS_RD_VER1_UNPACKER_FMT_ARGB_10:
	case BUS_RD_VER1_UNPACKER_FMT_ARGB_12:
	case BUS_RD_VER1_UNPACKER_FMT_ARGB_14:
		*bytes = pxls * 2;
		break;
	case BUS_RD_VER1_UNPACKER_FMT_PLAIN_32_20BPP:
	case BUS_RD_VER1_UNPACKER_FMT_PLAIN_32:
		*bytes = pxls * 4;
		break;
	case BUS_RD_VER1_UNPACKER_FMT_PLAIN_64:
		*bytes = pxls * 8;
		break;
	case BUS_RD_VER1_UNPACKER_FMT_TP_10:
		*bytes = ALIGNUP(pxls, 3) * 4 / 3;
		break;
	case BUS_RD_VER1_UNPACKER_FMT_MIPI8:
		*bytes = pxls;
		break;
	case BUS_RD_VER1_UNPACKER_FMT_MIPI10:
		*bytes = ALIGNUP(pxls * 5, 4) / 4;
		break;
	case BUS_RD_VER1_UNPACKER_FMT_MIPI12:
		*bytes = ALIGNUP(pxls * 3, 2) / 2;
		break;
	case BUS_RD_VER1_UNPACKER_FMT_MIPI14:
		*bytes = ALIGNUP(pxls * 7, 4) / 4;
		break;
	default:
		CAM_ERR(CAM_ISP, "Invalid unpacker_fmt:%d", fmt);
		break;
	}
}

static enum cam_vfe_bus_rd_ver1_unpacker_format
	cam_vfe_bus_get_unpacker_fmt(uint32_t unpack_fmt)
{
	switch (unpack_fmt) {
	case CAM_FORMAT_PLAIN128:
		return BUS_RD_VER1_UNPACKER_FMT_PLAIN_128;
	case CAM_FORMAT_PLAIN8:
		return BUS_RD_VER1_UNPACKER_FMT_PLAIN_8;
	case CAM_FORMAT_PLAIN16_10:
		return BUS_RD_VER1_UNPACKER_FMT_PLAIN_16_10BPP;
	case CAM_FORMAT_PLAIN16_12:
		return BUS_RD_VER1_UNPACKER_FMT_PLAIN_16_12BPP;
	case CAM_FORMAT_PLAIN16_14:
		return BUS_RD_VER1_UNPACKER_FMT_PLAIN_16_14BPP;
	case CAM_FORMAT_PLAIN32_20:
		return BUS_RD_VER1_UNPACKER_FMT_PLAIN_32_20BPP;
	case CAM_FORMAT_ARGB_10:
		return BUS_RD_VER1_UNPACKER_FMT_ARGB_10;
	case CAM_FORMAT_ARGB_12:
		return BUS_RD_VER1_UNPACKER_FMT_ARGB_12;
	case CAM_FORMAT_ARGB_14:
		return BUS_RD_VER1_UNPACKER_FMT_ARGB_14;
	case CAM_FORMAT_PLAIN32:
	case CAM_FORMAT_ARGB:
		return BUS_RD_VER1_UNPACKER_FMT_PLAIN_32;
	case CAM_FORMAT_PLAIN64:
	case CAM_FORMAT_ARGB_16:
	case CAM_FORMAT_PD10:
		return BUS_RD_VER1_UNPACKER_FMT_PLAIN_64;
	case CAM_FORMAT_TP10:
		return BUS_RD_VER1_UNPACKER_FMT_TP_10;
	case CAM_FORMAT_MIPI_RAW_8:
		return BUS_RD_VER1_UNPACKER_FMT_MIPI8;
	case CAM_FORMAT_MIPI_RAW_10:
		return BUS_RD_VER1_UNPACKER_FMT_MIPI10;
	case CAM_FORMAT_MIPI_RAW_12:
		return BUS_RD_VER1_UNPACKER_FMT_MIPI12;
	case CAM_FORMAT_MIPI_RAW_14:
		return BUS_RD_VER1_UNPACKER_FMT_MIPI14;
	case CAM_FORMAT_PLAIN16_16:
		return BUS_RD_VER1_UNPACKER_FMT_PLAIN_16_16BPP;
	case CAM_FORMAT_PLAIN8_SWAP:
		return BUS_RD_VER1_UNPACKER_FMT_PLAIN_8_ODD_EVEN;
	default:
		return BUS_RD_VER1_UNPACKER_FMT_MAX;
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
		CAM_ERR(CAM_ISP, "Unsupported resource_type %u", res_type);
		return -EINVAL;
	}
}

static int cam_vfe_bus_rd_handle_irq_top_half(uint32_t evt_id,
	struct cam_irq_th_payload *th_payload)
{
	struct cam_isp_resource_node                *bus_rd = NULL;
	struct cam_vfe_bus_rd_ver1_vfe_bus_rd_data  *rsrc_data = NULL;

	bus_rd = th_payload->handler_priv;
	if (!bus_rd) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "No resource");
		return -ENODEV;
	}
	rsrc_data = bus_rd->res_priv;
	rsrc_data->status = th_payload->evt_status_arr[0];

	CAM_DBG(CAM_ISP, "VFE:%d Bus RD IRQ status_0: 0x%X",
		rsrc_data->common_data->core_index,
		th_payload->evt_status_arr[0]);

	return 0;
}

static int cam_vfe_bus_rd_handle_irq_bottom_half(
	void  *handler_priv, void *evt_payload_priv)
{
	struct cam_isp_resource_node                *bus_rd = NULL;
	struct cam_vfe_bus_rd_ver1_vfe_bus_rd_data  *rsrc_data = NULL;

	bus_rd = (struct cam_isp_resource_node *)handler_priv;
	if (!bus_rd) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "No resource");
		return -ENODEV;
	}
	rsrc_data = bus_rd->res_priv;

	if (rsrc_data->status & 0x2)
		CAM_DBG(CAM_ISP, "Received VFE:%d BUS RD RUP",
			rsrc_data->common_data->core_index);
	else if (rsrc_data->status & 0x4)
		CAM_DBG(CAM_ISP, "Received VFE:%d BUS RD BUF DONE",
			rsrc_data->common_data->core_index);
	else if (rsrc_data->status & 0x10000) {
		CAM_ERR(CAM_ISP, "Received VFE:%d BUS RD CCIF Violation",
			rsrc_data->common_data->core_index);
		return CAM_VFE_IRQ_STATUS_VIOLATION;
	}

	return CAM_VFE_IRQ_STATUS_SUCCESS;
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
	struct cam_vfe_bus_rd_ver1_priv             *ver1_bus_rd_priv,
	void                                        *tasklet,
	void                                        *ctx,
	enum cam_vfe_bus_rd_ver1_vfe_bus_rd_type     vfe_bus_rd_res_id,
	enum cam_vfe_bus_plane_type                  plane,
	struct cam_isp_resource_node               **rm_res,
	uint32_t                                    *client_done_mask,
	uint32_t                                     is_dual,
	uint32_t                                     unpacker_fmt)
{
	uint32_t                                     rm_idx = 0;
	struct cam_isp_resource_node                *rm_res_local = NULL;
	struct cam_vfe_bus_rd_ver1_rm_resource_data *rsrc_data = NULL;

	*rm_res = NULL;
	*client_done_mask = 0;

	/* No need to allocate for BUS VER2. VFE OUT to RM is fixed. */
	rm_idx = cam_vfe_bus_get_rm_idx(vfe_bus_rd_res_id, plane);
	if (rm_idx < 0 || rm_idx >= ver1_bus_rd_priv->num_client) {
		CAM_ERR(CAM_ISP, "Unsupported VFE RM:%d plane:%d",
			vfe_bus_rd_res_id, plane);
		return -EINVAL;
	}

	rm_res_local = &ver1_bus_rd_priv->bus_client[rm_idx];
	if (rm_res_local->res_state != CAM_ISP_RESOURCE_STATE_AVAILABLE) {
		CAM_ERR(CAM_ISP, "VFE:%d RM:%d res not available state:%d",
			ver1_bus_rd_priv->common_data.core_index, rm_idx,
			rm_res_local->res_state);
		return -EALREADY;
	}
	rm_res_local->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;
	rm_res_local->tasklet_info = tasklet;

	rsrc_data = rm_res_local->res_priv;
	rsrc_data->ctx = ctx;
	rsrc_data->is_dual = is_dual;
	rsrc_data->unpacker_cfg = cam_vfe_bus_get_unpacker_fmt(unpacker_fmt);
	rsrc_data->latency_buf_allocation =
		BUS_RD_VER1_DEFAULT_LATENCY_BUF_ALLOC;
	/* Set RM offset value to default */
	rsrc_data->offset  = 0;

	*client_done_mask = (1 << (rm_idx + 2));
	*rm_res = rm_res_local;

	CAM_DBG(CAM_ISP, "VFE:%d RM:%d Acquired",
		rsrc_data->common_data->core_index, rsrc_data->index);
	return 0;
}

static int cam_vfe_bus_release_rm(void              *bus_priv,
	struct cam_isp_resource_node                *rm_res)
{
	struct cam_vfe_bus_rd_ver1_rm_resource_data *rsrc_data =
		rm_res->res_priv;

	rsrc_data->offset = 0;
	rsrc_data->width = 0;
	rsrc_data->height = 0;
	rsrc_data->stride = 0;
	rsrc_data->format = 0;
	rsrc_data->unpacker_cfg = 0;
	rsrc_data->burst_len = 0;
	rsrc_data->init_cfg_done = false;
	rsrc_data->en_cfg = 0;
	rsrc_data->is_dual = 0;

	rm_res->tasklet_info = NULL;
	rm_res->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;

	CAM_DBG(CAM_ISP, "VFE:%d RM:%d released",
		rsrc_data->common_data->core_index, rsrc_data->index);
	return 0;
}

static int cam_vfe_bus_start_rm(struct cam_isp_resource_node *rm_res)
{
	struct cam_vfe_bus_rd_ver1_rm_resource_data *rm_data;
	struct cam_vfe_bus_rd_ver1_common_data      *common_data;
	uint32_t                                     buf_size;

	rm_data = rm_res->res_priv;
	common_data = rm_data->common_data;

	buf_size = ((rm_data->width)&(0x0000FFFF)) |
		((rm_data->height<<16)&(0xFFFF0000));
	cam_io_w_mb(buf_size, common_data->mem_base +
		rm_data->hw_regs->buf_size);
	cam_io_w_mb(rm_data->width, common_data->mem_base +
		rm_data->hw_regs->stride);
	cam_io_w_mb(rm_data->unpacker_cfg, common_data->mem_base +
		rm_data->hw_regs->unpacker_cfg);
	cam_io_w_mb(rm_data->latency_buf_allocation, common_data->mem_base +
		rm_data->hw_regs->latency_buf_allocation);
	cam_io_w_mb(0x1, common_data->mem_base + rm_data->hw_regs->cfg);

	rm_res->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;

	CAM_DBG(CAM_ISP,
		"Start VFE:%d RM:%d offset:0x%X en_cfg:0x%X width:%d height:%d",
		rm_data->common_data->core_index, rm_data->index,
		(uint32_t) rm_data->hw_regs->cfg, rm_data->en_cfg,
		rm_data->width, rm_data->height);
	CAM_DBG(CAM_ISP, "RM:%d pk_fmt:%d stride:%d", rm_data->index,
		rm_data->unpacker_cfg, rm_data->stride);

	return 0;
}

static int cam_vfe_bus_stop_rm(struct cam_isp_resource_node *rm_res)
{
	int rc = 0;
	struct cam_vfe_bus_rd_ver1_rm_resource_data *rsrc_data =
		rm_res->res_priv;
	struct cam_vfe_bus_rd_ver1_common_data        *common_data =
		rsrc_data->common_data;

	/* Disable RM */
	cam_io_w_mb(0x0, common_data->mem_base + rsrc_data->hw_regs->cfg);

	rm_res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;
	rsrc_data->init_cfg_done = false;

	CAM_DBG(CAM_ISP, "VFE:%d RM:%d stopped",
		rsrc_data->common_data->core_index, rsrc_data->index);

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
		CAM_DBG(CAM_ISP, "Failed to alloc VFE:%d RM res priv",
			ver1_bus_rd_priv->common_data.core_index);
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
	uint32_t                                      client_done_mask;
	struct cam_vfe_bus_rd_ver1_priv              *ver1_bus_rd_priv =
		bus_priv;
	struct cam_vfe_acquire_args                  *acq_args = acquire_args;
	struct cam_vfe_hw_vfe_bus_rd_acquire_args    *bus_rd_acquire_args;
	struct cam_isp_resource_node                 *rsrc_node = NULL;
	struct cam_vfe_bus_rd_ver1_vfe_bus_rd_data   *rsrc_data = NULL;

	if (!bus_priv || !acquire_args) {
		CAM_ERR(CAM_ISP, "Invalid Param");
		return -EINVAL;
	}

	bus_rd_acquire_args = &acq_args->vfe_bus_rd;

	bus_rd_res_id = cam_vfe_bus_get_bus_rd_res_id(
		acq_args->rsrc_type);
	if (bus_rd_res_id == CAM_VFE_BUS_RD_VER1_VFE_BUSRD_MAX)
		return -ENODEV;

	num_rm = cam_vfe_bus_get_num_rm(bus_rd_res_id);
	if (num_rm < 1)
		return -EINVAL;

	rsrc_node = &ver1_bus_rd_priv->vfe_bus_rd[bus_rd_res_id];
	if (rsrc_node->res_state != CAM_ISP_RESOURCE_STATE_AVAILABLE) {
		CAM_ERR(CAM_ISP, "VFE:%d RM:0x%x not available state:%d",
			ver1_bus_rd_priv->common_data.core_index,
			acq_args->rsrc_type, rsrc_node->res_state);
		return -EBUSY;
	}

	rsrc_node->res_id = acq_args->rsrc_type;
	rsrc_data = rsrc_node->res_priv;

	CAM_DBG(CAM_ISP, "VFE:%d acquire RD type:0x%x",
		rsrc_data->common_data->core_index, acq_args->rsrc_type);

	rsrc_data->num_rm = num_rm;
	rsrc_node->tasklet_info = acq_args->tasklet;
	ver1_bus_rd_priv->tasklet_info = acq_args->tasklet;
	rsrc_node->cdm_ops = bus_rd_acquire_args->cdm_ops;
	rsrc_data->cdm_util_ops = bus_rd_acquire_args->cdm_ops;
	rsrc_data->common_data->event_cb = acq_args->event_cb;
	rsrc_data->priv = acq_args->priv;
	rsrc_data->is_offline = bus_rd_acquire_args->is_offline;

	for (i = 0; i < num_rm; i++) {
		rc = cam_vfe_bus_acquire_rm(ver1_bus_rd_priv,
			acq_args->tasklet,
			acq_args->priv,
			bus_rd_res_id,
			i,
			&rsrc_data->rm_res[i],
			&client_done_mask,
			bus_rd_acquire_args->is_dual,
			bus_rd_acquire_args->unpacker_fmt);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"VFE:%d RM:%d acquire failed rc:%d",
				rsrc_data->common_data->core_index,
				bus_rd_res_id, rc);
			goto release_rm;
		}
	}

	rsrc_node->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;
	bus_rd_acquire_args->rsrc_node = rsrc_node;

	CAM_DBG(CAM_ISP, "VFE:%d acquire RD 0x%x successful",
		rsrc_data->common_data->core_index, acq_args->rsrc_type);
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

	if (!bus_priv || !release_args) {
		CAM_ERR(CAM_ISP, "Invalid input bus_priv %pK release_args %pK",
			bus_priv, release_args);
		return -EINVAL;
	}

	vfe_bus_rd = release_args;
	rsrc_data = vfe_bus_rd->res_priv;

	if (vfe_bus_rd->res_state != CAM_ISP_RESOURCE_STATE_RESERVED) {
		CAM_ERR(CAM_ISP,
			"VFE:%d RD type:0x%x invalid resource state:%d",
			rsrc_data->common_data->core_index,
			vfe_bus_rd->res_id, vfe_bus_rd->res_state);
	}

	for (i = 0; i < rsrc_data->num_rm; i++)
		cam_vfe_bus_release_rm(bus_priv, rsrc_data->rm_res[i]);
	rsrc_data->num_rm = 0;

	vfe_bus_rd->tasklet_info = NULL;
	vfe_bus_rd->cdm_ops = NULL;
	rsrc_data->cdm_util_ops = NULL;

	if (vfe_bus_rd->res_state == CAM_ISP_RESOURCE_STATE_RESERVED)
		vfe_bus_rd->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;

	return 0;
}

static int cam_vfe_bus_start_vfe_bus_rd(
	struct cam_isp_resource_node          *vfe_bus_rd)
{
	int rc = 0, i;
	struct cam_vfe_bus_rd_ver1_vfe_bus_rd_data *rsrc_data = NULL;
	struct cam_vfe_bus_rd_ver1_common_data *common_data = NULL;
	uint32_t irq_reg_mask[1] = {0x6}, val = 0;

	if (!vfe_bus_rd) {
		CAM_ERR(CAM_ISP, "Invalid input");
		return -EINVAL;
	}

	rsrc_data = vfe_bus_rd->res_priv;
	common_data = rsrc_data->common_data;

	CAM_DBG(CAM_ISP, "VFE:%d start RD type:0x%x", vfe_bus_rd->res_id);

	if (vfe_bus_rd->res_state != CAM_ISP_RESOURCE_STATE_RESERVED) {
		CAM_ERR(CAM_ISP, "Invalid resource state:%d",
			vfe_bus_rd->res_state);
		return -EACCES;
	}

	if (!rsrc_data->is_offline) {
		val = (common_data->fs_sync_enable << 5) |
			(common_data->go_cmd_sel << 4);
		cam_io_w_mb(val, common_data->mem_base +
			common_data->common_reg->input_if_cmd);
	}

	for (i = 0; i < rsrc_data->num_rm; i++)
		rc = cam_vfe_bus_start_rm(rsrc_data->rm_res[i]);

	rsrc_data->irq_handle = cam_irq_controller_subscribe_irq(
		common_data->bus_irq_controller,
		CAM_IRQ_PRIORITY_1,
		irq_reg_mask,
		vfe_bus_rd,
		cam_vfe_bus_rd_handle_irq_top_half,
		cam_vfe_bus_rd_handle_irq_bottom_half,
		vfe_bus_rd->tasklet_info,
		&tasklet_bh_api);

	if (rsrc_data->irq_handle < 1) {
		CAM_ERR(CAM_ISP, "Failed to subscribe BUS RD IRQ");
		rsrc_data->irq_handle = 0;
		return -EFAULT;
	}

	vfe_bus_rd->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;
	return rc;
}

static int cam_vfe_bus_stop_vfe_bus_rd(
	struct cam_isp_resource_node          *vfe_bus_rd)
{
	int rc = 0, i;
	struct cam_vfe_bus_rd_ver1_vfe_bus_rd_data  *rsrc_data = NULL;

	if (!vfe_bus_rd) {
		CAM_ERR(CAM_ISP, "Invalid input");
		return -EINVAL;
	}

	rsrc_data = vfe_bus_rd->res_priv;

	if (vfe_bus_rd->res_state == CAM_ISP_RESOURCE_STATE_AVAILABLE ||
		vfe_bus_rd->res_state == CAM_ISP_RESOURCE_STATE_RESERVED) {
		CAM_DBG(CAM_ISP, "VFE:%d Bus RD 0x%x state: %d",
			rsrc_data->common_data->core_index, vfe_bus_rd->res_id,
			vfe_bus_rd->res_state);
		return rc;
	}
	for (i = 0; i < rsrc_data->num_rm; i++)
		rc = cam_vfe_bus_stop_rm(rsrc_data->rm_res[i]);

	if (rsrc_data->irq_handle) {
		rc = cam_irq_controller_unsubscribe_irq(
			rsrc_data->common_data->bus_irq_controller,
			rsrc_data->irq_handle);
		rsrc_data->irq_handle = 0;
	}

	vfe_bus_rd->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;

	CAM_DBG(CAM_ISP, "VFE:%d stopped Bus RD:0x%x",
		rsrc_data->common_data->core_index,
		vfe_bus_rd->res_id);
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
		vfe_bus_rd_resc_type >= CAM_VFE_BUS_RD_VER1_VFE_BUSRD_MAX) {
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
	vfe_bus_rd->process_cmd = cam_vfe_bus_rd_process_cmd;
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
	struct cam_irq_th_payload *th_payload)
{
	struct cam_vfe_bus_rd_ver1_priv *bus_priv;
	int rc = 0;

	bus_priv = th_payload->handler_priv;
	CAM_DBG(CAM_ISP, "Top Bus RD IRQ Received");

	rc = cam_irq_controller_handle_irq(evt_id,
		bus_priv->common_data.bus_irq_controller);

	return (rc == IRQ_HANDLED) ? 0 : -EINVAL;
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
		cam_vfe_bus_rd_pxls_to_bytes(io_cfg->planes[i].width,
			rm_data->unpacker_cfg, &rm_data->width);
		rm_data->height = io_cfg->planes[i].height;

		buf_size = ((rm_data->width)&(0x0000FFFF)) |
			((rm_data->height<<16)&(0xFFFF0000));

		CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
			rm_data->hw_regs->buf_size, buf_size);
		CAM_DBG(CAM_ISP, "VFE:%d RM:%d image_size:0x%X",
			rm_data->common_data->core_index,
			rm_data->index, reg_val_pair[j-1]);

		rm_data->stride = io_cfg->planes[i].plane_stride;
		CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
			rm_data->hw_regs->stride, rm_data->stride);
		CAM_DBG(CAM_ISP, "VFE:%d RM:%d image_stride:0x%X",
			rm_data->common_data->core_index,
			rm_data->index, reg_val_pair[j-1]);

		CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
			rm_data->hw_regs->image_addr,
			update_buf->rm_update->image_buf[i] +
				rm_data->offset);
		CAM_DBG(CAM_ISP, "VFE:%d RM:%d image_address:0x%X",
			rm_data->common_data->core_index,
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
		rm_data->unpacker_cfg = fe_cfg->unpacker_cfg;
		rm_data->latency_buf_allocation = fe_cfg->latency_buf_size;
		rm_data->stride = fe_cfg->stride;
		rm_data->go_cmd_sel = fe_cfg->go_cmd_sel;
		rm_data->fs_sync_enable = fe_cfg->fs_sync_enable;
		rm_data->hbi_count = fe_cfg->hbi_count;
		rm_data->fs_line_sync_en = fe_cfg->fs_line_sync_en;
		rm_data->fs_mode = fe_cfg->fs_mode;
		rm_data->min_vbi = fe_cfg->min_vbi;

		CAM_DBG(CAM_ISP,
			"VFE:%d RM:%d format:0x%x unpacker_cfg:0x%x",
			rm_data->format, rm_data->unpacker_cfg);
		CAM_DBG(CAM_ISP,
			"latency_buf_alloc:0x%x stride:0x%x go_cmd_sel:0x%x",
			rm_data->latency_buf_allocation, rm_data->stride,
			rm_data->go_cmd_sel);
		CAM_DBG(CAM_ISP,
			"fs_sync_en:%d hbi_cnt:0x%x fs_mode:0x%x min_vbi:0x%x",
			rm_data->fs_sync_enable, rm_data->hbi_count,
			rm_data->fs_mode, rm_data->min_vbi);
	}
	bus_priv->common_data.fs_sync_enable = fe_cfg->fs_sync_enable;
	bus_priv->common_data.go_cmd_sel = fe_cfg->go_cmd_sel;
	return 0;
}

static int cam_vfe_bus_rd_add_go_cmd(void *priv, void *cmd_args,
	uint32_t arg_size)
{
	struct cam_vfe_bus_rd_ver1_vfe_bus_rd_data  *rd_data;
	struct cam_isp_hw_get_cmd_update            *cdm_args = cmd_args;
	struct cam_cdm_utils_ops                    *cdm_util_ops = NULL;
	uint32_t reg_val_pair[2 * CAM_VFE_BUS_RD_VER1_VFE_BUSRD_MAX];
	struct cam_vfe_bus_rd_ver1_rm_resource_data *rsrc_data;
	int i = 0;
	uint32_t val = 0, size = 0, offset = 0;

	if (arg_size != sizeof(struct cam_isp_hw_get_cmd_update)) {
		CAM_ERR(CAM_ISP, "invalid ars size");
		return -EINVAL;
	}

	if (!cdm_args || !cdm_args->res) {
		CAM_ERR(CAM_ISP, "Invalid args");
		return -EINVAL;
	}

	rd_data = (struct cam_vfe_bus_rd_ver1_vfe_bus_rd_data  *) priv;

	cdm_util_ops = (struct cam_cdm_utils_ops *)cdm_args->res->cdm_ops;

	if (!cdm_util_ops) {
		CAM_ERR(CAM_ISP, "Invalid CDM ops");
		return -EINVAL;
	}

	for (i = 0; i < rd_data->num_rm; i++) {
		size = cdm_util_ops->cdm_required_size_reg_random(1);
		/* since cdm returns dwords, we need to convert it into bytes */
		if ((size * 4) > cdm_args->cmd.size) {
			CAM_ERR(CAM_ISP,
				"buf size:%d is not sufficient, expected: %d",
				cdm_args->cmd.size, size);
			return -EINVAL;
		}

		rsrc_data = rd_data->rm_res[i]->res_priv;
		offset = rsrc_data->common_data->common_reg->input_if_cmd;
		val = cam_io_r_mb(rsrc_data->common_data->mem_base + offset);
		val |= 0x1;
		reg_val_pair[i * 2] = offset;
		reg_val_pair[i * 2 + 1] = val;
		CAM_DBG(CAM_ISP, "VFE:%d Bus RD go_cmd: 0x%x offset 0x%x",
			rd_data->common_data->core_index,
			reg_val_pair[i * 2 + 1], reg_val_pair[i * 2]);
	}

	cdm_util_ops->cdm_write_regrandom(cdm_args->cmd.cmd_buf_addr,
		1, reg_val_pair);

	cdm_args->cmd.used_bytes = size * 4;

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
	uint32_t                            top_irq_reg_mask[3] = {0};
	uint32_t                            offset = 0;
	struct cam_vfe_bus_rd_ver1_reg_offset_common  *common_reg;

	if (!bus_priv) {
		CAM_ERR(CAM_ISP, "Invalid args");
		return -EINVAL;
	}
	common_reg = bus_priv->common_data.common_reg;
	top_irq_reg_mask[0] = (1 << bus_priv->top_irq_shift);

	bus_priv->irq_handle = cam_irq_controller_subscribe_irq(
		bus_priv->common_data.vfe_irq_controller,
		CAM_IRQ_PRIORITY_2,
		top_irq_reg_mask,
		bus_priv,
		cam_vfe_bus_rd_ver1_handle_irq,
		NULL,
		NULL,
		NULL);

	if (bus_priv->irq_handle < 1) {
		CAM_ERR(CAM_ISP, "Failed to subscribe BUS IRQ");
		bus_priv->irq_handle = 0;
		return -EFAULT;
	}

	/* no clock gating at bus input */
	offset = common_reg->cgc_ovd;
	cam_io_w_mb(0x1, bus_priv->common_data.mem_base + offset);

	/* BUS_RD_TEST_BUS_CTRL */
	offset = common_reg->test_bus_ctrl;
	cam_io_w_mb(0x0, bus_priv->common_data.mem_base + offset);

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

	if (bus_priv->irq_handle) {
		rc = cam_irq_controller_unsubscribe_irq(
			bus_priv->common_data.vfe_irq_controller,
			bus_priv->irq_handle);
		bus_priv->irq_handle = 0;
	}

	return rc;
}

static int __cam_vfe_bus_rd_process_cmd(void *priv,
	uint32_t cmd_type, void *cmd_args, uint32_t arg_size)
{
	return cam_vfe_bus_rd_process_cmd(priv, cmd_type, cmd_args, arg_size);
}

static int cam_vfe_bus_rd_process_cmd(
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
	case CAM_ISP_HW_CMD_FE_TRIGGER_CMD:
		rc = cam_vfe_bus_rd_add_go_cmd(priv, cmd_args, arg_size);
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
	bus_priv->top_irq_shift                 = bus_rd_hw_info->top_irq_shift;

	mutex_init(&bus_priv->common_data.bus_mutex);

	rc = cam_irq_controller_init(drv_name, bus_priv->common_data.mem_base,
		&bus_rd_hw_info->common_reg.irq_reg_info,
		&bus_priv->common_data.bus_irq_controller, true);
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
	vfe_bus_local->hw_ops.process_cmd  = __cam_vfe_bus_rd_process_cmd;

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
