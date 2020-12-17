// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/ratelimit.h>
#include <linux/slab.h>
#include <media/cam_tfe.h>
#include <media/cam_isp_tfe.h>
#include "cam_io_util.h"
#include "cam_debug_util.h"
#include "cam_cdm_util.h"
#include "cam_hw_intf.h"
#include "cam_isp_hw_mgr_intf.h"
#include "cam_tfe_hw_intf.h"
#include "cam_irq_controller.h"
#include "cam_tasklet_util.h"
#include "cam_tfe_bus.h"
#include "cam_tfe_irq.h"
#include "cam_tfe_soc.h"
#include "cam_debug_util.h"
#include "cam_cpas_api.h"


static const char drv_name[] = "tfe_bus";

#define CAM_TFE_BUS_IRQ_REG0                0
#define CAM_TFE_BUS_IRQ_REG1                1

#define CAM_TFE_BUS_PAYLOAD_MAX             256

#define CAM_TFE_RDI_BUS_DEFAULT_WIDTH               0xFFFF
#define CAM_TFE_RDI_BUS_DEFAULT_STRIDE              0xFFFF

#define CAM_TFE_MAX_OUT_RES_PER_COMP_GRP    2

#define MAX_BUF_UPDATE_REG_NUM   \
	(sizeof(struct cam_tfe_bus_reg_offset_bus_client) / 4)
#define MAX_REG_VAL_PAIR_SIZE    \
	(MAX_BUF_UPDATE_REG_NUM * 2 * CAM_PACKET_MAX_PLANES)

enum cam_tfe_bus_packer_format {
	PACKER_FMT_PLAIN_128,
	PACKER_FMT_PLAIN_8,
	PACKER_FMT_PLAIN_8_ODD_EVEN,
	PACKER_FMT_PLAIN_8_LSB_MSB_10,
	PACKER_FMT_PLAIN_8_LSB_MSB_10_ODD_EVEN,
	PACKER_FMT_PLAIN_16_10BPP,
	PACKER_FMT_PLAIN_16_12BPP,
	PACKER_FMT_PLAIN_16_14BPP,
	PACKER_FMT_PLAIN_16_16BPP,
	PACKER_FMT_PLAIN_32,
	PACKER_FMT_PLAIN_64,
	PACKER_FMT_TP_10,
	PACKET_FMT_MIPI10,
	PACKET_FMT_MIPI12,
	PACKER_FMT_MAX,
};

struct cam_tfe_bus_common_data {
	uint32_t                                    core_index;
	void __iomem                               *mem_base;
	struct cam_hw_intf                         *hw_intf;
	void                                       *tfe_core_data;
	struct cam_tfe_bus_reg_offset_common       *common_reg;
	uint32_t       io_buf_update[MAX_REG_VAL_PAIR_SIZE];

	spinlock_t                                  spin_lock;
	struct mutex                                bus_mutex;
	uint32_t                                    secure_mode;
	uint32_t                                    num_sec_out;
	uint32_t                                    comp_done_shift;
	bool                                        is_lite;
	cam_hw_mgr_event_cb_func                    event_cb;
	bool                        rup_irq_enable[CAM_TFE_BUS_RUP_GRP_MAX];
};

struct cam_tfe_bus_wm_resource_data {
	uint32_t             index;
	uint32_t             out_id;
	struct cam_tfe_bus_common_data            *common_data;
	struct cam_tfe_bus_reg_offset_bus_client  *hw_regs;

	uint32_t             offset;
	uint32_t             width;
	uint32_t             height;
	uint32_t             stride;
	uint32_t             format;
	uint32_t             pack_fmt;
	uint32_t             burst_len;
	uint32_t             mode;

	uint32_t             irq_subsample_period;
	uint32_t             irq_subsample_pattern;
	uint32_t             framedrop_period;
	uint32_t             framedrop_pattern;

	uint32_t             en_cfg;
	uint32_t             is_dual;

	uint32_t             acquired_width;
	uint32_t             acquired_height;
	uint32_t             acquired_stride;
};

struct cam_tfe_bus_comp_grp_data {
	enum cam_tfe_bus_comp_grp_id            comp_grp_id;
	struct cam_tfe_bus_common_data         *common_data;

	uint32_t                                is_master;
	uint32_t                                is_dual;
	uint32_t                                addr_sync_mode;
	uint32_t                                composite_mask;

	uint32_t                                acquire_dev_cnt;
	uint32_t                                source_grp;

	struct cam_isp_resource_node
		*out_rsrc[CAM_TFE_MAX_OUT_RES_PER_COMP_GRP];
};

struct cam_tfe_bus_tfe_out_data {
	uint32_t                         out_id;
	uint32_t                         composite_group;
	uint32_t                         rup_group_id;
	uint32_t                         source_group;
	struct cam_tfe_bus_common_data  *common_data;

	uint32_t                         num_wm;
	struct cam_isp_resource_node    *wm_res[PLANE_MAX];

	struct cam_isp_resource_node    *comp_grp;
	struct list_head                 tfe_out_list;

	uint32_t                         is_master;
	uint32_t                         is_dual;

	uint32_t                         format;
	uint32_t                         max_width;
	uint32_t                         max_height;
	struct cam_cdm_utils_ops        *cdm_util_ops;
	uint32_t                         secure_mode;
	void                            *priv;
	cam_hw_mgr_event_cb_func         event_cb;
};

struct cam_tfe_bus_priv {
	struct cam_tfe_bus_common_data      common_data;
	uint32_t                            num_client;
	uint32_t                            num_out;
	uint32_t                            top_bus_wr_irq_shift;

	struct cam_isp_resource_node  bus_client[CAM_TFE_BUS_MAX_CLIENTS];
	struct cam_isp_resource_node  comp_grp[CAM_TFE_BUS_COMP_GRP_MAX];
	struct cam_isp_resource_node  tfe_out[CAM_TFE_BUS_TFE_OUT_MAX];

	struct list_head                    free_comp_grp;
	struct list_head                    used_comp_grp;

	void                               *tasklet_info;
	uint32_t                            comp_buf_done_mask;
	uint32_t                            comp_rup_done_mask;
	uint32_t           bus_irq_error_mask[CAM_TFE_BUS_IRQ_REGISTERS_MAX];
};

static bool cam_tfe_bus_can_be_secure(uint32_t out_id)
{
	switch (out_id) {
	case CAM_TFE_BUS_TFE_OUT_FULL:
	case CAM_TFE_BUS_TFE_OUT_RAW_DUMP:
	case CAM_TFE_BUS_TFE_OUT_RDI0:
	case CAM_TFE_BUS_TFE_OUT_RDI1:
	case CAM_TFE_BUS_TFE_OUT_RDI2:
		return true;

	case CAM_TFE_BUS_TFE_OUT_STATS_HDR_BE:
	case CAM_TFE_BUS_TFE_OUT_STATS_HDR_BHIST:
	case CAM_TFE_BUS_TFE_OUT_STATS_TL_BG:
	case CAM_TFE_BUS_TFE_OUT_STATS_BF:
	case CAM_TFE_BUS_TFE_OUT_STATS_AWB_BG:
	default:
		return false;
	}
}

static enum cam_tfe_bus_tfe_out_id
	cam_tfe_bus_get_out_res_id(uint32_t out_res_id)
{
	switch (out_res_id) {
	case CAM_ISP_TFE_OUT_RES_FULL:
		return CAM_TFE_BUS_TFE_OUT_FULL;
	case CAM_ISP_TFE_OUT_RES_RAW_DUMP:
		return CAM_TFE_BUS_TFE_OUT_RAW_DUMP;
	case CAM_ISP_TFE_OUT_RES_PDAF:
		return CAM_TFE_BUS_TFE_OUT_PDAF;
	case CAM_ISP_TFE_OUT_RES_RDI_0:
		return CAM_TFE_BUS_TFE_OUT_RDI0;
	case CAM_ISP_TFE_OUT_RES_RDI_1:
		return CAM_TFE_BUS_TFE_OUT_RDI1;
	case CAM_ISP_TFE_OUT_RES_RDI_2:
		return CAM_TFE_BUS_TFE_OUT_RDI2;
	case CAM_ISP_TFE_OUT_RES_STATS_HDR_BE:
		return CAM_TFE_BUS_TFE_OUT_STATS_HDR_BE;
	case CAM_ISP_TFE_OUT_RES_STATS_HDR_BHIST:
		return CAM_TFE_BUS_TFE_OUT_STATS_HDR_BHIST;
	case CAM_ISP_TFE_OUT_RES_STATS_TL_BG:
		return CAM_TFE_BUS_TFE_OUT_STATS_TL_BG;
	case CAM_ISP_TFE_OUT_RES_STATS_BF:
		return CAM_TFE_BUS_TFE_OUT_STATS_BF;
	case CAM_ISP_TFE_OUT_RES_STATS_AWB_BG:
		return CAM_TFE_BUS_TFE_OUT_STATS_AWB_BG;
	default:
		return CAM_TFE_BUS_TFE_OUT_MAX;
	}
}

static int cam_tfe_bus_get_num_wm(
	enum cam_tfe_bus_tfe_out_id           out_res_id,
	uint32_t                              format)
{
	switch (out_res_id) {
	case CAM_TFE_BUS_TFE_OUT_RDI0:
	case CAM_TFE_BUS_TFE_OUT_RDI1:
	case CAM_TFE_BUS_TFE_OUT_RDI2:
		switch (format) {
		case CAM_FORMAT_MIPI_RAW_8:
		case CAM_FORMAT_MIPI_RAW_10:
		case CAM_FORMAT_MIPI_RAW_12:
		case CAM_FORMAT_MIPI_RAW_14:
		case CAM_FORMAT_MIPI_RAW_16:
		case CAM_FORMAT_PLAIN8:
		case CAM_FORMAT_PLAIN16_10:
		case CAM_FORMAT_PLAIN16_12:
		case CAM_FORMAT_PLAIN16_14:
		case CAM_FORMAT_PLAIN16_16:
		case CAM_FORMAT_PLAIN128:
			return 1;
		default:
			break;
		}
		break;
	case CAM_TFE_BUS_TFE_OUT_PDAF:
		switch (format) {
		case CAM_FORMAT_PLAIN8:
		case CAM_FORMAT_PLAIN16_10:
		case CAM_FORMAT_PLAIN16_12:
		case CAM_FORMAT_PLAIN16_14:
			return 1;
		default:
			break;
		}
		break;

	case CAM_TFE_BUS_TFE_OUT_FULL:
		switch (format) {
		case CAM_FORMAT_MIPI_RAW_8:
		case CAM_FORMAT_MIPI_RAW_10:
		case CAM_FORMAT_MIPI_RAW_12:
		case CAM_FORMAT_PLAIN8:
		case CAM_FORMAT_PLAIN16_10:
		case CAM_FORMAT_PLAIN16_12:
			return 1;
		default:
			break;
		}
		break;
	case CAM_TFE_BUS_TFE_OUT_RAW_DUMP:
		switch (format) {
		case CAM_FORMAT_ARGB_14:
		case CAM_FORMAT_PLAIN8:
		case CAM_FORMAT_PLAIN16_10:
		case CAM_FORMAT_PLAIN16_12:
		case CAM_FORMAT_PLAIN16_14:
		case CAM_FORMAT_MIPI_RAW_8:
		case CAM_FORMAT_MIPI_RAW_10:
		case CAM_FORMAT_MIPI_RAW_12:
			return 1;
		default:
			break;
		}
		break;
	case CAM_TFE_BUS_TFE_OUT_STATS_HDR_BE:
	case CAM_TFE_BUS_TFE_OUT_STATS_HDR_BHIST:
	case CAM_TFE_BUS_TFE_OUT_STATS_TL_BG:
	case CAM_TFE_BUS_TFE_OUT_STATS_BF:
	case CAM_TFE_BUS_TFE_OUT_STATS_AWB_BG:
		switch (format) {
		case CAM_FORMAT_PLAIN64:
			return 1;
		default:
			break;
		}
		break;
	default:
		break;
	}

	CAM_ERR(CAM_ISP, "Unsupported format %u for resource id %u",
		format, out_res_id);

	return -EINVAL;
}

static int cam_tfe_bus_get_wm_idx(
	enum cam_tfe_bus_tfe_out_id tfe_out_res_id,
	enum cam_tfe_bus_plane_type plane)
{
	int wm_idx = -1;

	switch (tfe_out_res_id) {
	case CAM_TFE_BUS_TFE_OUT_RDI0:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 7;
			break;
		default:
			break;
		}
		break;
	case CAM_TFE_BUS_TFE_OUT_RDI1:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 8;
			break;
		default:
			break;
		}
		break;
	case CAM_TFE_BUS_TFE_OUT_RDI2:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 9;
			break;
		default:
			break;
		}
		break;
	case CAM_TFE_BUS_TFE_OUT_PDAF:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 9;
			break;
		default:
			break;
		}
		break;

	case CAM_TFE_BUS_TFE_OUT_FULL:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 0;
			break;
		default:
			break;
		}
		break;
	case CAM_TFE_BUS_TFE_OUT_RAW_DUMP:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 1;
			break;
		default:
			break;
		}
		break;
	case CAM_TFE_BUS_TFE_OUT_STATS_HDR_BE:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 5;
			break;
		default:
			break;
		}
		break;
	case CAM_TFE_BUS_TFE_OUT_STATS_HDR_BHIST:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 3;
			break;
		default:
			break;
		}
		break;
	case CAM_TFE_BUS_TFE_OUT_STATS_AWB_BG:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 4;
			break;
		default:
			break;
		}
		break;
	case CAM_TFE_BUS_TFE_OUT_STATS_TL_BG:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 2;
			break;
		default:
			break;
		}
		break;
	case CAM_TFE_BUS_TFE_OUT_STATS_BF:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 6;
			break;
		default:
			break;
		}
		break;

	default:
		break;
	}

	return wm_idx;
}

static enum cam_tfe_bus_packer_format
	cam_tfe_bus_get_packer_fmt(uint32_t out_fmt, int wm_index)
{
	switch (out_fmt) {
	case CAM_FORMAT_MIPI_RAW_6:
	case CAM_FORMAT_MIPI_RAW_8:
	case CAM_FORMAT_MIPI_RAW_10:
	case CAM_FORMAT_MIPI_RAW_12:
	case CAM_FORMAT_MIPI_RAW_14:
	case CAM_FORMAT_MIPI_RAW_16:
	case CAM_FORMAT_MIPI_RAW_20:
	case CAM_FORMAT_PLAIN16_8:
	case CAM_FORMAT_PLAIN128:
	case CAM_FORMAT_PD8:
		return PACKER_FMT_PLAIN_128;
	case CAM_FORMAT_PLAIN8:
		return PACKER_FMT_PLAIN_8;
	case CAM_FORMAT_Y_ONLY:
		return PACKER_FMT_PLAIN_8_LSB_MSB_10;
	case CAM_FORMAT_PLAIN16_10:
		return PACKER_FMT_PLAIN_16_10BPP;
	case CAM_FORMAT_PLAIN16_12:
		return PACKER_FMT_PLAIN_16_12BPP;
	case CAM_FORMAT_PLAIN16_14:
		return PACKER_FMT_PLAIN_16_14BPP;
	case CAM_FORMAT_PLAIN16_16:
		return PACKER_FMT_PLAIN_16_16BPP;
	case CAM_FORMAT_ARGB:
		return PACKER_FMT_PLAIN_32;
	case CAM_FORMAT_PLAIN64:
	case CAM_FORMAT_PD10:
		return PACKER_FMT_PLAIN_64;
	case CAM_FORMAT_TP10:
		return PACKER_FMT_TP_10;
	default:
		return PACKER_FMT_MAX;
	}
}

static int cam_tfe_bus_acquire_rdi_wm(
	struct cam_tfe_bus_wm_resource_data  *rsrc_data)
{
	switch (rsrc_data->format) {
	case CAM_FORMAT_MIPI_RAW_6:
		rsrc_data->pack_fmt = 0xA;
		if (rsrc_data->mode == CAM_ISP_TFE_WM_LINE_BASED_MODE) {
			rsrc_data->width =
				ALIGNUP(rsrc_data->width * 6, 64) / 64;
			rsrc_data->en_cfg = 0x1;
		} else {
			rsrc_data->width =
				CAM_TFE_RDI_BUS_DEFAULT_WIDTH;
			rsrc_data->height = 0;
			rsrc_data->stride =
				CAM_TFE_RDI_BUS_DEFAULT_STRIDE;
			rsrc_data->en_cfg = (0x1 << 16) | 0x1;
		}
		break;
	case CAM_FORMAT_MIPI_RAW_8:
	case CAM_FORMAT_PLAIN8:
		rsrc_data->pack_fmt = 0xA;
		if (rsrc_data->mode == CAM_ISP_TFE_WM_LINE_BASED_MODE) {
			rsrc_data->width =
				ALIGNUP(rsrc_data->width * 8, 64) / 64;

			rsrc_data->en_cfg = 0x1;
		} else {
			rsrc_data->width =
				CAM_TFE_RDI_BUS_DEFAULT_WIDTH;
			rsrc_data->height = 0;
			rsrc_data->stride =
				CAM_TFE_RDI_BUS_DEFAULT_STRIDE;
			rsrc_data->en_cfg = (0x1 << 16) | 0x1;
		}
		break;
	case CAM_FORMAT_MIPI_RAW_10:
		rsrc_data->pack_fmt = 0xA;
		if (rsrc_data->mode == CAM_ISP_TFE_WM_LINE_BASED_MODE) {
			rsrc_data->width =
				ALIGNUP(rsrc_data->width * 10, 64) / 64;

			rsrc_data->en_cfg = 0x1;
		} else {
			rsrc_data->width =
				CAM_TFE_RDI_BUS_DEFAULT_WIDTH;
			rsrc_data->height = 0;
			rsrc_data->stride =
				CAM_TFE_RDI_BUS_DEFAULT_STRIDE;
			rsrc_data->en_cfg = (0x1 << 16) | 0x1;
		}
		break;
	case CAM_FORMAT_MIPI_RAW_12:
		rsrc_data->pack_fmt = 0xA;
		if (rsrc_data->mode == CAM_ISP_TFE_WM_LINE_BASED_MODE) {
			rsrc_data->width =
				ALIGNUP(rsrc_data->width * 12, 64) / 64;

			rsrc_data->en_cfg = 0x1;
		} else {
			rsrc_data->width =
				CAM_TFE_RDI_BUS_DEFAULT_WIDTH;
			rsrc_data->height = 0;
			rsrc_data->stride =
				CAM_TFE_RDI_BUS_DEFAULT_STRIDE;
			rsrc_data->en_cfg = (0x1 << 16) | 0x1;
		}
		break;
	case CAM_FORMAT_MIPI_RAW_14:
		rsrc_data->pack_fmt = 0xA;
		if (rsrc_data->mode == CAM_ISP_TFE_WM_LINE_BASED_MODE) {
			rsrc_data->width =
				ALIGNUP(rsrc_data->width * 14, 64) / 64;

			rsrc_data->en_cfg = 0x1;
		} else {
			rsrc_data->width =
				CAM_TFE_RDI_BUS_DEFAULT_WIDTH;
			rsrc_data->height = 0;
			rsrc_data->stride =
				CAM_TFE_RDI_BUS_DEFAULT_STRIDE;
			rsrc_data->en_cfg = (0x1 << 16) | 0x1;
		}
		break;
	case CAM_FORMAT_PLAIN16_10:
	case CAM_FORMAT_PLAIN16_12:
	case CAM_FORMAT_PLAIN16_14:
	case CAM_FORMAT_MIPI_RAW_16:
	case CAM_FORMAT_PLAIN16_16:
		rsrc_data->pack_fmt = 0xA;
		if (rsrc_data->mode == CAM_ISP_TFE_WM_LINE_BASED_MODE) {
			rsrc_data->width =
				ALIGNUP(rsrc_data->width * 16, 64) / 64;

			rsrc_data->en_cfg = 0x1;
		} else {
			rsrc_data->width =
				CAM_TFE_RDI_BUS_DEFAULT_WIDTH;
			rsrc_data->height = 0;
			rsrc_data->stride =
				CAM_TFE_RDI_BUS_DEFAULT_STRIDE;
			rsrc_data->en_cfg = (0x1 << 16) | 0x1;
		}
		break;

	case CAM_FORMAT_PLAIN128:
	case CAM_FORMAT_PLAIN64:
		rsrc_data->pack_fmt = 0xA;
		if (rsrc_data->mode == CAM_ISP_TFE_WM_LINE_BASED_MODE) {
			rsrc_data->width =
				ALIGNUP(rsrc_data->width * 64, 64) / 64;

			rsrc_data->en_cfg = 0x1;
		} else {
			rsrc_data->width =
				CAM_TFE_RDI_BUS_DEFAULT_WIDTH;
			rsrc_data->height = 0;
			rsrc_data->stride =
				CAM_TFE_RDI_BUS_DEFAULT_STRIDE;
			rsrc_data->en_cfg = (0x1 << 16) | 0x1;
		}
		break;
	default:
		CAM_ERR(CAM_ISP, "Unsupported RDI:%d format %d",
			rsrc_data->index, rsrc_data->format);
		return -EINVAL;
	}

	return 0;
}

static int cam_tfe_bus_acquire_wm(
	struct cam_tfe_bus_priv               *bus_priv,
	struct cam_isp_tfe_out_port_info      *out_port_info,
	struct cam_isp_resource_node         **wm_res,
	void                                  *tasklet,
	enum cam_tfe_bus_tfe_out_id            tfe_out_res_id,
	enum cam_tfe_bus_plane_type            plane,
	uint32_t                              *client_done_mask,
	uint32_t                               is_dual,
	enum cam_tfe_bus_comp_grp_id          *comp_grp_id)
{
	struct cam_isp_resource_node         *wm_res_local = NULL;
	struct cam_tfe_bus_wm_resource_data  *rsrc_data = NULL;
	uint32_t wm_idx = 0;
	int rc = 0;

	*wm_res = NULL;
	/* No need to allocate for BUS TFE OUT to WM is fixed. */
	wm_idx = cam_tfe_bus_get_wm_idx(tfe_out_res_id, plane);
	if (wm_idx < 0 || wm_idx >= bus_priv->num_client) {
		CAM_ERR(CAM_ISP, "Unsupported TFE out %d plane %d",
			tfe_out_res_id, plane);
		return -EINVAL;
	}

	wm_res_local = &bus_priv->bus_client[wm_idx];
	if (wm_res_local->res_state != CAM_ISP_RESOURCE_STATE_AVAILABLE) {
		CAM_ERR(CAM_ISP, "WM:%d not available state:%d",
			wm_idx, wm_res_local->res_state);
		return -EALREADY;
	}
	wm_res_local->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;
	wm_res_local->tasklet_info = tasklet;

	rsrc_data = wm_res_local->res_priv;
	rsrc_data->format = out_port_info->format;
	rsrc_data->pack_fmt = cam_tfe_bus_get_packer_fmt(rsrc_data->format,
		wm_idx);

	rsrc_data->width = out_port_info->width;
	rsrc_data->height = out_port_info->height;
	rsrc_data->stride = out_port_info->stride;
	rsrc_data->mode = out_port_info->wm_mode;
	rsrc_data->out_id = tfe_out_res_id;

	/*
	 * Store the acquire width, height separately. For frame based ports
	 * width and height modified again
	 */
	rsrc_data->acquired_width = out_port_info->width;
	rsrc_data->acquired_height = out_port_info->height;
	rsrc_data->acquired_stride = out_port_info->stride;

	rsrc_data->is_dual = is_dual;
	/* Set WM offset value to default */
	rsrc_data->offset  = 0;

	if ((rsrc_data->index > 6) &&
		(tfe_out_res_id != CAM_TFE_BUS_TFE_OUT_PDAF)) {
		/* WM 7-9 refers to RDI 0/ RDI 1/RDI 2 */
		rc = cam_tfe_bus_acquire_rdi_wm(rsrc_data);
		if (rc)
			return rc;

	} else if (rsrc_data->index == 0 || rsrc_data->index == 1 ||
		(tfe_out_res_id == CAM_TFE_BUS_TFE_OUT_PDAF)) {
	/*  WM 0 FULL_OUT WM 1 IDEAL RAW WM9 for pdaf */
		switch (rsrc_data->format) {
		case CAM_FORMAT_MIPI_RAW_8:
			rsrc_data->pack_fmt = 0x1;
			break;
		case CAM_FORMAT_MIPI_RAW_10:
			rsrc_data->pack_fmt = 0xc;
			break;
		case CAM_FORMAT_MIPI_RAW_12:
			rsrc_data->pack_fmt = 0xd;
			break;
		case CAM_FORMAT_PLAIN8:
			rsrc_data->pack_fmt = 0x1;
			break;
		case CAM_FORMAT_PLAIN16_10:
			rsrc_data->pack_fmt = 0x5;
			rsrc_data->pack_fmt |= 0x10;
			break;
		case CAM_FORMAT_PLAIN16_12:
			rsrc_data->pack_fmt = 0x6;
			rsrc_data->pack_fmt |= 0x10;
			break;
		default:
			CAM_ERR(CAM_ISP, "Invalid format %d",
				rsrc_data->format);
			return -EINVAL;
		}

		rsrc_data->en_cfg = 0x1;
	} else if (rsrc_data->index  >= 2 && rsrc_data->index <= 6) {
		/* WM 2-6 stats */
		rsrc_data->width = 0;
		rsrc_data->height = 0;
		rsrc_data->stride = 1;
		rsrc_data->en_cfg = (0x1 << 16) | 0x1;
	} else {
		CAM_ERR(CAM_ISP, "Invalid WM:%d requested", rsrc_data->index);
		return -EINVAL;
	}

	*wm_res = wm_res_local;
	*comp_grp_id = rsrc_data->hw_regs->comp_group;
	*client_done_mask |= (1 << wm_idx);

	CAM_DBG(CAM_ISP,
		"WM:%d processed width:%d height:%d format:0x%x comp_group:%d packt format:0x%x wm mode:%d",
		rsrc_data->index, rsrc_data->width, rsrc_data->height,
		rsrc_data->format, *comp_grp_id, rsrc_data->pack_fmt,
		rsrc_data->mode);
	return 0;
}

static int cam_tfe_bus_release_wm(void   *bus_priv,
	struct cam_isp_resource_node     *wm_res)
{
	struct cam_tfe_bus_wm_resource_data   *rsrc_data = wm_res->res_priv;

	rsrc_data->offset = 0;
	rsrc_data->width = 0;
	rsrc_data->height = 0;
	rsrc_data->stride = 0;
	rsrc_data->format = 0;
	rsrc_data->pack_fmt = 0;
	rsrc_data->burst_len = 0;
	rsrc_data->irq_subsample_period = 0;
	rsrc_data->irq_subsample_pattern = 0;
	rsrc_data->framedrop_period = 0;
	rsrc_data->framedrop_pattern = 0;
	rsrc_data->en_cfg = 0;
	rsrc_data->is_dual = 0;

	wm_res->tasklet_info = NULL;
	wm_res->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;

	CAM_DBG(CAM_ISP, "TFE:%dRelease WM:%d",
		rsrc_data->common_data->core_index, rsrc_data->index);

	return 0;
}

static int cam_tfe_bus_start_wm(struct cam_isp_resource_node *wm_res)
{
	struct cam_tfe_bus_wm_resource_data   *rsrc_data =
		wm_res->res_priv;
	struct cam_tfe_bus_common_data        *common_data =
		rsrc_data->common_data;

	cam_io_w(0xf, common_data->mem_base + rsrc_data->hw_regs->bw_limit);

	cam_io_w((rsrc_data->height << 16) | rsrc_data->width,
		common_data->mem_base + rsrc_data->hw_regs->image_cfg_0);
	cam_io_w(rsrc_data->pack_fmt,
		common_data->mem_base + rsrc_data->hw_regs->packer_cfg);

	/* Configure stride for RDIs on full TFE and TFE lite  */
	if ((rsrc_data->index > 6) &&
		((rsrc_data->mode != CAM_ISP_TFE_WM_LINE_BASED_MODE) &&
		(rsrc_data->out_id != CAM_TFE_BUS_TFE_OUT_PDAF))) {
		cam_io_w_mb(rsrc_data->stride, (common_data->mem_base +
			rsrc_data->hw_regs->image_cfg_2));
		CAM_DBG(CAM_ISP, "WM:%d configure stride reg :0x%x",
			rsrc_data->index,
			rsrc_data->stride);
	}

	/* Enable WM */
	cam_io_w_mb(rsrc_data->en_cfg, common_data->mem_base +
		rsrc_data->hw_regs->cfg);

	CAM_DBG(CAM_ISP, "TFE:%d WM:%d width = %d, height = %d",
		common_data->core_index, rsrc_data->index,
		rsrc_data->width, rsrc_data->height);
	CAM_DBG(CAM_ISP, "WM:%d pk_fmt = %d", rsrc_data->index,
		rsrc_data->pack_fmt);
	CAM_DBG(CAM_ISP, "WM:%d stride = %d, burst len = %d",
		rsrc_data->index, rsrc_data->stride, 0xf);
	CAM_DBG(CAM_ISP, "TFE:%d Start WM:%d offset 0x%x val 0x%x",
		common_data->core_index, rsrc_data->index,
		(uint32_t) rsrc_data->hw_regs->cfg, rsrc_data->en_cfg);

	wm_res->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;

	return 0;
}

static int cam_tfe_bus_stop_wm(struct cam_isp_resource_node *wm_res)
{
	struct cam_tfe_bus_wm_resource_data   *rsrc_data =
		wm_res->res_priv;
	struct cam_tfe_bus_common_data        *common_data =
		rsrc_data->common_data;

	/* Disable WM */
	cam_io_w_mb(0x0, common_data->mem_base + rsrc_data->hw_regs->cfg);
	CAM_DBG(CAM_ISP, "TFE:%d Stop WM:%d",
		rsrc_data->common_data->core_index, rsrc_data->index);

	wm_res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;

	return 0;
}

static int cam_tfe_bus_init_wm_resource(uint32_t index,
	struct cam_tfe_bus_priv    *bus_priv,
	struct cam_tfe_bus_hw_info *hw_info,
	struct cam_isp_resource_node    *wm_res)
{
	struct cam_tfe_bus_wm_resource_data *rsrc_data;

	rsrc_data = kzalloc(sizeof(struct cam_tfe_bus_wm_resource_data),
		GFP_KERNEL);
	if (!rsrc_data) {
		CAM_DBG(CAM_ISP, "Failed to alloc for WM res priv");
		return -ENOMEM;
	}
	wm_res->res_priv = rsrc_data;

	rsrc_data->index = index;
	rsrc_data->hw_regs = &hw_info->bus_client_reg[index];
	rsrc_data->common_data = &bus_priv->common_data;

	wm_res->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;
	INIT_LIST_HEAD(&wm_res->list);

	wm_res->start = cam_tfe_bus_start_wm;
	wm_res->stop = cam_tfe_bus_stop_wm;
	wm_res->hw_intf = bus_priv->common_data.hw_intf;

	return 0;
}

static int cam_tfe_bus_deinit_wm_resource(
	struct cam_isp_resource_node    *wm_res)
{
	struct cam_tfe_bus_wm_resource_data *rsrc_data;

	wm_res->res_state = CAM_ISP_RESOURCE_STATE_UNAVAILABLE;
	INIT_LIST_HEAD(&wm_res->list);

	wm_res->start = NULL;
	wm_res->stop = NULL;
	wm_res->top_half_handler = NULL;
	wm_res->bottom_half_handler = NULL;
	wm_res->hw_intf = NULL;

	rsrc_data = wm_res->res_priv;
	wm_res->res_priv = NULL;
	if (!rsrc_data)
		return -ENOMEM;
	kfree(rsrc_data);

	return 0;
}

static void cam_tfe_bus_add_wm_to_comp_grp(
	struct cam_isp_resource_node    *comp_grp,
	uint32_t                         composite_mask)
{
	struct cam_tfe_bus_comp_grp_data  *rsrc_data = comp_grp->res_priv;

	rsrc_data->composite_mask |= composite_mask;
}

static bool cam_tfe_bus_match_comp_grp(
	struct cam_tfe_bus_priv                *bus_priv,
	struct cam_isp_resource_node          **comp_grp,
	uint32_t                                comp_grp_id)
{
	struct cam_tfe_bus_comp_grp_data       *rsrc_data = NULL;
	struct cam_isp_resource_node           *comp_grp_local = NULL;

	list_for_each_entry(comp_grp_local,
		&bus_priv->used_comp_grp, list) {
		rsrc_data = comp_grp_local->res_priv;
		if (rsrc_data->comp_grp_id == comp_grp_id) {
			/* Match found */
			*comp_grp = comp_grp_local;
			return true;
		}
	}

	list_for_each_entry(comp_grp_local,
		&bus_priv->free_comp_grp, list) {
		rsrc_data = comp_grp_local->res_priv;
		if (rsrc_data->comp_grp_id == comp_grp_id) {
			/* Match found */
			*comp_grp = comp_grp_local;
			list_del(&comp_grp_local->list);
			list_add_tail(&comp_grp_local->list,
			&bus_priv->used_comp_grp);
			return false;
		}
	}

	*comp_grp = NULL;
	return false;
}

static int cam_tfe_bus_acquire_comp_grp(
	struct cam_tfe_bus_priv             *bus_priv,
	struct cam_isp_tfe_out_port_info    *out_port_info,
	void                                *tasklet,
	uint32_t                             is_dual,
	uint32_t                             is_master,
	struct cam_isp_resource_node       **comp_grp,
	enum cam_tfe_bus_comp_grp_id         comp_grp_id,
	struct cam_isp_resource_node        *out_rsrc,
	uint32_t                             source_group)
{
	int rc = 0;
	struct cam_isp_resource_node      *comp_grp_local = NULL;
	struct cam_tfe_bus_comp_grp_data  *rsrc_data = NULL;
	bool previously_acquired  = false;

	if (comp_grp_id >= CAM_TFE_BUS_COMP_GRP_0 &&
		comp_grp_id <= CAM_TFE_BUS_COMP_GRP_7) {
		/* Check if matching comp_grp has already been acquired */
		previously_acquired = cam_tfe_bus_match_comp_grp(
			bus_priv, &comp_grp_local, comp_grp_id);
	}

	if (!comp_grp_local) {
		CAM_ERR(CAM_ISP, "Invalid comp_grp_id:%d", comp_grp_id);
		return -ENODEV;
	}

	rsrc_data = comp_grp_local->res_priv;
	if (rsrc_data->acquire_dev_cnt > CAM_TFE_MAX_OUT_RES_PER_COMP_GRP) {
		CAM_ERR(CAM_ISP, "Many acquires comp_grp_id:%d", comp_grp_id);
		return -ENODEV;
	}

	if (!previously_acquired) {
		comp_grp_local->tasklet_info = tasklet;
		comp_grp_local->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;

		rsrc_data->is_master = is_master;
		rsrc_data->is_dual = is_dual;

		if (is_master)
			rsrc_data->addr_sync_mode = 0;
		else
			rsrc_data->addr_sync_mode = 1;
	} else {
		rsrc_data = comp_grp_local->res_priv;
		/* Do not support runtime change in composite mask */
		if (comp_grp_local->res_state ==
			CAM_ISP_RESOURCE_STATE_STREAMING) {
			CAM_ERR(CAM_ISP, "Invalid State %d Comp Grp %u",
				comp_grp_local->res_state,
				rsrc_data->comp_grp_id);
			return -EBUSY;
		}
	}

	CAM_DBG(CAM_ISP, "Acquire comp_grp id:%u", rsrc_data->comp_grp_id);
	rsrc_data->source_grp = source_group;
	rsrc_data->out_rsrc[rsrc_data->acquire_dev_cnt] = out_rsrc;
	rsrc_data->acquire_dev_cnt++;
	*comp_grp = comp_grp_local;

	return rc;
}

static int cam_tfe_bus_release_comp_grp(
	struct cam_tfe_bus_priv              *bus_priv,
	struct cam_isp_resource_node         *comp_grp)
{
	struct cam_isp_resource_node      *comp_grp_local = NULL;
	struct cam_tfe_bus_comp_grp_data  *comp_rsrc_data = NULL;
	int match_found = 0;

	if (!comp_grp) {
		CAM_ERR(CAM_ISP, "Invalid Params Comp Grp %pK", comp_grp);
		return -EINVAL;
	}

	if (comp_grp->res_state == CAM_ISP_RESOURCE_STATE_AVAILABLE) {
		CAM_ERR(CAM_ISP, "Already released Comp Grp");
		return 0;
	}

	if (comp_grp->res_state == CAM_ISP_RESOURCE_STATE_STREAMING) {
		CAM_ERR(CAM_ISP, "Invalid State %d",
			comp_grp->res_state);
		return -EBUSY;
	}

	comp_rsrc_data = comp_grp->res_priv;
	CAM_DBG(CAM_ISP, "Comp Grp id %u", comp_rsrc_data->comp_grp_id);

	list_for_each_entry(comp_grp_local, &bus_priv->used_comp_grp, list) {
		if (comp_grp_local == comp_grp) {
			match_found = 1;
			break;
		}
	}

	if (!match_found) {
		CAM_ERR(CAM_ISP, "Could not find comp_grp_id:%u",
			comp_rsrc_data->comp_grp_id);
		return -ENODEV;
	}

	comp_rsrc_data->acquire_dev_cnt--;
	if (comp_rsrc_data->acquire_dev_cnt == 0) {
		list_del(&comp_grp_local->list);

		comp_rsrc_data->addr_sync_mode = 0;
		comp_rsrc_data->composite_mask = 0;

		comp_grp_local->tasklet_info = NULL;
		comp_grp_local->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;

		list_add_tail(&comp_grp_local->list, &bus_priv->free_comp_grp);
		CAM_DBG(CAM_ISP, "Comp Grp id %u released",
			comp_rsrc_data->comp_grp_id);
	}

	return 0;
}

static int cam_tfe_bus_start_comp_grp(
	struct cam_isp_resource_node *comp_grp)
{
	int rc = 0;
	uint32_t val;
	struct cam_tfe_bus_comp_grp_data *rsrc_data = NULL;
	struct cam_tfe_bus_common_data   *common_data = NULL;
	uint32_t     bus_irq_reg_mask_0  = 0;

	rsrc_data = comp_grp->res_priv;
	common_data = rsrc_data->common_data;

	CAM_DBG(CAM_ISP, "TFE:%d comp_grp_id:%d streaming state:%d mask:0x%x",
		common_data->core_index, rsrc_data->comp_grp_id,
		comp_grp->res_state, rsrc_data->composite_mask);

	if (comp_grp->res_state == CAM_ISP_RESOURCE_STATE_STREAMING)
		return 0;

	if (rsrc_data->is_dual) {
		if (rsrc_data->is_master) {
			val = cam_io_r(common_data->mem_base +
				common_data->common_reg->comp_cfg_0);
			val |= (0x1 << (rsrc_data->comp_grp_id + 16));
			cam_io_w_mb(val, common_data->mem_base +
				common_data->common_reg->comp_cfg_0);

			val = cam_io_r(common_data->mem_base +
				common_data->common_reg->comp_cfg_1);
			val |= (0x1 << rsrc_data->comp_grp_id);
			cam_io_w_mb(val, common_data->mem_base +
				common_data->common_reg->comp_cfg_1);
		} else {
			val = cam_io_r(common_data->mem_base +
				common_data->common_reg->comp_cfg_0);
			val |= (0x1 << rsrc_data->comp_grp_id);
			val |= (0x1 << (rsrc_data->comp_grp_id + 16));
			cam_io_w(val, common_data->mem_base +
				common_data->common_reg->comp_cfg_0);

			val = cam_io_r(common_data->mem_base +
				common_data->common_reg->comp_cfg_1);
			val |= (0x1 << rsrc_data->comp_grp_id);
			cam_io_w(val, common_data->mem_base +
				common_data->common_reg->comp_cfg_1);
		}
	}

	if (rsrc_data->is_dual && !rsrc_data->is_master)
		goto end;

	/* Update the composite done mask in bus irq mask*/
	bus_irq_reg_mask_0 = cam_io_r(common_data->mem_base +
		common_data->common_reg->irq_mask[CAM_TFE_BUS_IRQ_REG0]);
	bus_irq_reg_mask_0 |= (0x1 << (rsrc_data->comp_grp_id +
		rsrc_data->common_data->comp_done_shift));
	cam_io_w_mb(bus_irq_reg_mask_0, common_data->mem_base +
		common_data->common_reg->irq_mask[CAM_TFE_BUS_IRQ_REG0]);

	CAM_DBG(CAM_ISP, "TFE:%d start COMP_GRP:%d bus_irq_mask_0 0x%x",
		common_data->core_index, rsrc_data->comp_grp_id,
		bus_irq_reg_mask_0);

end:
	comp_grp->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;

	return rc;
}

static int cam_tfe_bus_stop_comp_grp(
	struct cam_isp_resource_node          *comp_grp)
{
	struct cam_tfe_bus_comp_grp_data *rsrc_data = NULL;
	struct cam_tfe_bus_common_data *common_data = NULL;
	uint32_t      bus_irq_reg_mask_0 = 0;

	if (comp_grp->res_state == CAM_ISP_RESOURCE_STATE_RESERVED)
		return 0;

	rsrc_data = (struct cam_tfe_bus_comp_grp_data *)comp_grp->res_priv;
	common_data = rsrc_data->common_data;

	/* Update the composite done mask in bus irq mask*/
	bus_irq_reg_mask_0  = cam_io_r(common_data->mem_base +
		common_data->common_reg->irq_mask[CAM_TFE_BUS_IRQ_REG0]);
	bus_irq_reg_mask_0 &= ~(0x1 << (rsrc_data->comp_grp_id +
		rsrc_data->common_data->comp_done_shift));
	cam_io_w_mb(bus_irq_reg_mask_0, common_data->mem_base +
		common_data->common_reg->irq_mask[CAM_TFE_BUS_IRQ_REG0]);
	comp_grp->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;

	return 0;
}

static int cam_tfe_bus_init_comp_grp(uint32_t index,
	struct cam_hw_soc_info          *soc_info,
	struct cam_tfe_bus_priv         *bus_priv,
	struct cam_tfe_bus_hw_info      *hw_info,
	struct cam_isp_resource_node    *comp_grp)
{
	struct cam_tfe_bus_comp_grp_data *rsrc_data = NULL;

	rsrc_data = kzalloc(sizeof(struct cam_tfe_bus_comp_grp_data),
		GFP_KERNEL);
	if (!rsrc_data)
		return -ENOMEM;

	comp_grp->res_priv = rsrc_data;

	comp_grp->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;
	INIT_LIST_HEAD(&comp_grp->list);

	comp_grp->res_id = index;
	rsrc_data->comp_grp_id   = index;
	rsrc_data->common_data     = &bus_priv->common_data;

	list_add_tail(&comp_grp->list, &bus_priv->free_comp_grp);

	comp_grp->hw_intf = bus_priv->common_data.hw_intf;

	return 0;
}

static int cam_tfe_bus_deinit_comp_grp(
	struct cam_isp_resource_node    *comp_grp)
{
	struct cam_tfe_bus_comp_grp_data *rsrc_data =
		comp_grp->res_priv;

	comp_grp->start = NULL;
	comp_grp->stop = NULL;
	comp_grp->top_half_handler = NULL;
	comp_grp->bottom_half_handler = NULL;
	comp_grp->hw_intf = NULL;

	list_del_init(&comp_grp->list);
	comp_grp->res_state = CAM_ISP_RESOURCE_STATE_UNAVAILABLE;

	comp_grp->res_priv = NULL;

	if (!rsrc_data) {
		CAM_ERR(CAM_ISP, "comp_grp_priv is NULL");
		return -ENODEV;
	}
	kfree(rsrc_data);

	return 0;
}

static int cam_tfe_bus_get_secure_mode(void *priv, void *cmd_args,
	uint32_t arg_size)
{
	struct cam_isp_hw_get_cmd_update    *secure_mode = cmd_args;
	struct cam_tfe_bus_tfe_out_data     *rsrc_data;
	uint32_t                            *mode;

	rsrc_data = (struct cam_tfe_bus_tfe_out_data *)
		secure_mode->res->res_priv;
	mode = (uint32_t *)secure_mode->data;
	*mode = (rsrc_data->secure_mode == CAM_SECURE_MODE_SECURE) ?
		true : false;

	return 0;
}

static int cam_tfe_bus_acquire_tfe_out(void *priv, void *acquire_args,
	uint32_t args_size)
{
	struct cam_tfe_bus_priv                *bus_priv = priv;
	struct cam_tfe_acquire_args            *acq_args = acquire_args;
	struct cam_tfe_hw_tfe_out_acquire_args *out_acquire_args;
	struct cam_isp_resource_node           *rsrc_node = NULL;
	struct cam_tfe_bus_tfe_out_data        *rsrc_data = NULL;
	enum cam_tfe_bus_tfe_out_id             tfe_out_res_id;
	enum cam_tfe_bus_comp_grp_id            comp_grp_id;
	int                                     i, rc = -ENODEV;
	uint32_t                                secure_caps = 0, mode;
	uint32_t  format, num_wm, client_done_mask = 0;

	if (!bus_priv || !acquire_args) {
		CAM_ERR(CAM_ISP, "Invalid Param");
		return -EINVAL;
	}

	out_acquire_args = &acq_args->tfe_out;
	format = out_acquire_args->out_port_info->format;

	CAM_DBG(CAM_ISP, "resid 0x%x fmt:%d, sec mode:%d wm mode:%d",
		out_acquire_args->out_port_info->res_id, format,
		out_acquire_args->out_port_info->secure_mode,
		out_acquire_args->out_port_info->wm_mode);
	CAM_DBG(CAM_ISP, "width:%d, height:%d stride:%d",
		out_acquire_args->out_port_info->width,
		out_acquire_args->out_port_info->height,
		out_acquire_args->out_port_info->stride);

	tfe_out_res_id = cam_tfe_bus_get_out_res_id(
		out_acquire_args->out_port_info->res_id);
	if (tfe_out_res_id == CAM_TFE_BUS_TFE_OUT_MAX)
		return -ENODEV;

	num_wm = cam_tfe_bus_get_num_wm(tfe_out_res_id, format);
	if (num_wm < 1)
		return -EINVAL;

	rsrc_node = &bus_priv->tfe_out[tfe_out_res_id];
	if (rsrc_node->res_state != CAM_ISP_RESOURCE_STATE_AVAILABLE) {
		CAM_ERR(CAM_ISP, "Resource not available: Res_id %d state:%d",
			tfe_out_res_id, rsrc_node->res_state);
		return -EBUSY;
	}

	rsrc_data = rsrc_node->res_priv;
	rsrc_data->common_data->event_cb = acq_args->event_cb;
	rsrc_data->event_cb = acq_args->event_cb;
	rsrc_data->priv = acq_args->priv;

	secure_caps = cam_tfe_bus_can_be_secure(rsrc_data->out_id);
	mode = out_acquire_args->out_port_info->secure_mode;
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

	bus_priv->tasklet_info = acq_args->tasklet;
	rsrc_data->num_wm = num_wm;
	rsrc_node->rdi_only_ctx = 0;
	rsrc_node->res_id = out_acquire_args->out_port_info->res_id;
	rsrc_node->cdm_ops = out_acquire_args->cdm_ops;
	rsrc_data->cdm_util_ops = out_acquire_args->cdm_ops;

	/* Acquire WM and retrieve COMP GRP ID */
	for (i = 0; i < num_wm; i++) {
		rc = cam_tfe_bus_acquire_wm(bus_priv,
			out_acquire_args->out_port_info,
			&rsrc_data->wm_res[i],
			acq_args->tasklet,
			tfe_out_res_id,
			i,
			&client_done_mask,
			out_acquire_args->is_dual,
			&comp_grp_id);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"TFE:%d WM acquire failed for Out %d rc=%d",
				rsrc_data->common_data->core_index,
				tfe_out_res_id, rc);
			goto release_wm;
		}
	}

	/* Acquire composite group using COMP GRP ID */
	rc = cam_tfe_bus_acquire_comp_grp(bus_priv,
		out_acquire_args->out_port_info,
		acq_args->tasklet,
		out_acquire_args->is_dual,
		out_acquire_args->is_master,
		&rsrc_data->comp_grp,
		comp_grp_id,
		rsrc_node,
		rsrc_data->source_group);
	if (rc) {
		CAM_ERR(CAM_ISP,
			"TFE%d Comp_Grp acquire fail for Out %d rc=%d",
			rsrc_data->common_data->core_index,
			tfe_out_res_id, rc);
		return rc;
	}

	rsrc_data->is_dual = out_acquire_args->is_dual;
	rsrc_data->is_master = out_acquire_args->is_master;

	cam_tfe_bus_add_wm_to_comp_grp(rsrc_data->comp_grp,
		client_done_mask);

	rsrc_node->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;
	out_acquire_args->rsrc_node = rsrc_node;

	return rc;

release_wm:
	for (i--; i >= 0; i--)
		cam_tfe_bus_release_wm(bus_priv, rsrc_data->wm_res[i]);

	if (rsrc_data->comp_grp)
		cam_tfe_bus_release_comp_grp(bus_priv, rsrc_data->comp_grp);

	return rc;
}

static int cam_tfe_bus_release_tfe_out(void *priv, void *release_args,
	uint32_t args_size)
{
	struct cam_tfe_bus_priv               *bus_priv = priv;
	struct cam_isp_resource_node          *tfe_out = NULL;
	struct cam_tfe_bus_tfe_out_data       *rsrc_data = NULL;
	uint32_t                               secure_caps = 0;
	uint32_t i;

	if (!bus_priv || !release_args) {
		CAM_ERR(CAM_ISP, "Invalid input bus_priv %pK release_args %pK",
			bus_priv, release_args);
		return -EINVAL;
	}

	tfe_out = (struct cam_isp_resource_node *)release_args;
	rsrc_data = (struct cam_tfe_bus_tfe_out_data *)tfe_out->res_priv;

	if (tfe_out->res_state != CAM_ISP_RESOURCE_STATE_RESERVED) {
		CAM_ERR(CAM_ISP, "Invalid resource state:%d res id:%d",
			tfe_out->res_state, tfe_out->res_id);
	}

	for (i = 0; i < rsrc_data->num_wm; i++)
		cam_tfe_bus_release_wm(bus_priv, rsrc_data->wm_res[i]);

	rsrc_data->num_wm = 0;

	if (rsrc_data->comp_grp)
		cam_tfe_bus_release_comp_grp(bus_priv, rsrc_data->comp_grp);

	rsrc_data->comp_grp = NULL;

	tfe_out->tasklet_info = NULL;
	tfe_out->cdm_ops = NULL;
	rsrc_data->cdm_util_ops = NULL;

	secure_caps = cam_tfe_bus_can_be_secure(rsrc_data->out_id);
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

	if (tfe_out->res_state == CAM_ISP_RESOURCE_STATE_RESERVED)
		tfe_out->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;

	return 0;
}

static int cam_tfe_bus_start_tfe_out(void *hw_priv,
	void *start_hw_args, uint32_t arg_size)
{
	struct cam_isp_resource_node     *tfe_out = hw_priv;
	struct cam_tfe_bus_tfe_out_data  *rsrc_data = NULL;
	struct cam_tfe_bus_common_data   *common_data = NULL;
	uint32_t bus_irq_reg_mask_0 = 0;
	uint32_t rup_group_id = 0;
	int rc = 0, i;

	if (!tfe_out) {
		CAM_ERR(CAM_ISP, "Invalid input");
		return -EINVAL;
	}

	rsrc_data = tfe_out->res_priv;
	common_data = rsrc_data->common_data;
	rup_group_id = rsrc_data->rup_group_id;

	CAM_DBG(CAM_ISP, "TFE:%d Start resource index %d",
		common_data->core_index, rsrc_data->out_id);

	if (tfe_out->res_state != CAM_ISP_RESOURCE_STATE_RESERVED) {
		CAM_ERR(CAM_ISP, "TFE:%d Invalid resource state:%d",
			common_data->core_index, tfe_out->res_state);
		return -EACCES;
	}

	for (i = 0; i < rsrc_data->num_wm; i++)
		rc = cam_tfe_bus_start_wm(rsrc_data->wm_res[i]);

	rc = cam_tfe_bus_start_comp_grp(rsrc_data->comp_grp);

	if (rsrc_data->is_dual && !rsrc_data->is_master &&
		!tfe_out->rdi_only_ctx)
		goto end;

	if (common_data->rup_irq_enable[rup_group_id])
		goto end;

	/* Update the composite regupdate mask in bus irq mask*/
	bus_irq_reg_mask_0 = cam_io_r(common_data->mem_base +
		common_data->common_reg->irq_mask[CAM_TFE_BUS_IRQ_REG0]);
	bus_irq_reg_mask_0 |= (0x1 << rup_group_id);
	cam_io_w_mb(bus_irq_reg_mask_0, common_data->mem_base +
		common_data->common_reg->irq_mask[CAM_TFE_BUS_IRQ_REG0]);
	common_data->rup_irq_enable[rup_group_id] = true;

end:
	tfe_out->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;
	return rc;
}

static int cam_tfe_bus_stop_tfe_out(void *hw_priv,
	void *stop_hw_args, uint32_t arg_size)
{
	struct cam_isp_resource_node      *tfe_out = hw_priv;
	struct cam_tfe_bus_tfe_out_data   *rsrc_data = NULL;
	struct cam_tfe_bus_common_data    *common_data = NULL;
	uint32_t bus_irq_reg_mask_0 = 0,  rup_group = 0;
	int rc = 0, i;

	if (!tfe_out) {
		CAM_ERR(CAM_ISP, "Invalid input");
		return -EINVAL;
	}

	rsrc_data = tfe_out->res_priv;
	common_data = rsrc_data->common_data;
	rup_group = rsrc_data->rup_group_id;

	if (tfe_out->res_state == CAM_ISP_RESOURCE_STATE_AVAILABLE ||
		tfe_out->res_state == CAM_ISP_RESOURCE_STATE_RESERVED) {
		CAM_DBG(CAM_ISP, "tfe_out res_state is %d", tfe_out->res_state);
		return rc;
	}

	rc = cam_tfe_bus_stop_comp_grp(rsrc_data->comp_grp);

	for (i = 0; i < rsrc_data->num_wm; i++)
		rc = cam_tfe_bus_stop_wm(rsrc_data->wm_res[i]);


	if (!common_data->rup_irq_enable[rup_group])
		goto end;

	/* disable composite regupdate mask in bus irq mask register*/
	bus_irq_reg_mask_0 = cam_io_r(common_data->mem_base +
		common_data->common_reg->irq_mask[CAM_TFE_BUS_IRQ_REG0]);
	bus_irq_reg_mask_0 &= ~(0x1 << rup_group);
	cam_io_w_mb(bus_irq_reg_mask_0, common_data->mem_base +
		common_data->common_reg->irq_mask[CAM_TFE_BUS_IRQ_REG0]);
	common_data->rup_irq_enable[rup_group] = false;

end:
	tfe_out->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;
	return rc;
}

static int cam_tfe_bus_init_tfe_out_resource(uint32_t  index,
	struct cam_tfe_bus_priv                  *bus_priv,
	struct cam_tfe_bus_hw_info               *hw_info)
{
	struct cam_isp_resource_node         *tfe_out = NULL;
	struct cam_tfe_bus_tfe_out_data *rsrc_data = NULL;
	int rc = 0;
	int32_t tfe_out_id = hw_info->tfe_out_hw_info[index].tfe_out_id;

	if (tfe_out_id < 0 ||
		tfe_out_id >= CAM_TFE_BUS_TFE_OUT_MAX) {
		CAM_ERR(CAM_ISP, "Init TFE Out failed, Invalid type=%d",
			tfe_out_id);
		return -EINVAL;
	}

	tfe_out = &bus_priv->tfe_out[tfe_out_id];
	if (tfe_out->res_state != CAM_ISP_RESOURCE_STATE_UNAVAILABLE ||
		tfe_out->res_priv) {
		CAM_ERR(CAM_ISP, "tfe_out_id %d has already been initialized",
			tfe_out_id);
		return -EFAULT;
	}

	rsrc_data = kzalloc(sizeof(struct cam_tfe_bus_tfe_out_data),
		GFP_KERNEL);
	if (!rsrc_data) {
		rc = -ENOMEM;
		return rc;
	}

	tfe_out->res_priv = rsrc_data;

	tfe_out->res_type = CAM_ISP_RESOURCE_TFE_OUT;
	tfe_out->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;
	INIT_LIST_HEAD(&tfe_out->list);

	rsrc_data->composite_group =
		hw_info->tfe_out_hw_info[index].composite_group;
	rsrc_data->rup_group_id    =
		hw_info->tfe_out_hw_info[index].rup_group_id;
	rsrc_data->out_id          =
		hw_info->tfe_out_hw_info[index].tfe_out_id;
	rsrc_data->common_data     = &bus_priv->common_data;
	rsrc_data->max_width       =
		hw_info->tfe_out_hw_info[index].max_width;
	rsrc_data->max_height      =
		hw_info->tfe_out_hw_info[index].max_height;
	rsrc_data->secure_mode  = CAM_SECURE_MODE_NON_SECURE;

	tfe_out->hw_intf = bus_priv->common_data.hw_intf;

	return 0;
}

static int cam_tfe_bus_deinit_tfe_out_resource(
	struct cam_isp_resource_node    *tfe_out)
{
	struct cam_tfe_bus_tfe_out_data *rsrc_data = tfe_out->res_priv;

	if (tfe_out->res_state == CAM_ISP_RESOURCE_STATE_UNAVAILABLE) {
		/*
		 * This is not error. It can happen if the resource is
		 * never supported in the HW.
		 */
		CAM_DBG(CAM_ISP, "HW%d Res %d already deinitialized");
		return 0;
	}

	tfe_out->start = NULL;
	tfe_out->stop = NULL;
	tfe_out->top_half_handler = NULL;
	tfe_out->bottom_half_handler = NULL;
	tfe_out->hw_intf = NULL;

	tfe_out->res_state = CAM_ISP_RESOURCE_STATE_UNAVAILABLE;
	INIT_LIST_HEAD(&tfe_out->list);
	tfe_out->res_priv = NULL;

	if (!rsrc_data)
		return -ENOMEM;
	kfree(rsrc_data);

	return 0;
}

static const char *cam_tfe_bus_rup_type(
	uint32_t group_id)
{
	switch (group_id) {
	case CAM_ISP_HW_TFE_IN_CAMIF:
		return "CAMIF RUP";
	case CAM_ISP_HW_TFE_IN_RDI0:
		return "RDI0 RUP";
	case CAM_ISP_HW_TFE_IN_RDI1:
		return "RDI1 RUP";
	case CAM_ISP_HW_TFE_IN_RDI2:
		return "RDI2 RUP";
	default:
		return "invalid rup group";
	}
}
static int cam_tfe_bus_rup_bottom_half(
	struct cam_tfe_bus_priv            *bus_priv,
	struct cam_tfe_irq_evt_payload *evt_payload)
{
	struct cam_tfe_bus_common_data     *common_data;
	struct cam_tfe_bus_tfe_out_data    *out_rsrc_data;
	struct cam_isp_hw_event_info        evt_info;
	uint32_t i, j;

	common_data = &bus_priv->common_data;
	evt_info.hw_idx = bus_priv->common_data.core_index;
	evt_info.res_type = CAM_ISP_RESOURCE_TFE_OUT;

	for (i = 0; i < CAM_TFE_BUS_RUP_GRP_MAX; i++) {
		if (!(evt_payload->bus_irq_val[0] &
			bus_priv->comp_rup_done_mask))
			break;

		if (evt_payload->bus_irq_val[0] & BIT(i)) {
			for (j = 0; j < CAM_TFE_BUS_TFE_OUT_MAX; j++) {
				out_rsrc_data =
					(struct cam_tfe_bus_tfe_out_data *)
					bus_priv->tfe_out[j].res_priv;
				if ((out_rsrc_data->rup_group_id == i) &&
					(bus_priv->tfe_out[j].res_state ==
					CAM_ISP_RESOURCE_STATE_STREAMING))
					break;
			}

			if (j == CAM_TFE_BUS_TFE_OUT_MAX) {
				CAM_ERR(CAM_ISP,
					"TFE:%d out rsc active status[0]:0x%x",
					bus_priv->common_data.core_index,
					evt_payload->bus_irq_val[0]);
				continue;
			}

			CAM_DBG(CAM_ISP, "TFE:%d Received %s",
				bus_priv->common_data.core_index,
				cam_tfe_bus_rup_type(i));
			evt_info.res_id = i;
			if (out_rsrc_data->event_cb) {
				out_rsrc_data->event_cb(
					out_rsrc_data->priv,
					CAM_ISP_HW_EVENT_REG_UPDATE,
					(void *)&evt_info);
				/* reset the rup bit */
				evt_payload->bus_irq_val[0] &= ~BIT(i);
			} else
				CAM_ERR(CAM_ISP,
					"TFE:%d No event cb id:%lld evt id:%d",
					bus_priv->common_data.core_index,
					out_rsrc_data->out_id, evt_info.res_id);
		}
	}

	return 0;
}

static int cam_tfe_bus_bufdone_bottom_half(
	struct cam_tfe_bus_priv            *bus_priv,
	struct cam_tfe_irq_evt_payload *evt_payload)
{
	struct cam_tfe_bus_common_data     *common_data;
	struct cam_tfe_bus_tfe_out_data    *out_rsrc_data;
	struct cam_isp_hw_event_info        evt_info;
	struct cam_isp_resource_node       *out_rsrc = NULL;
	struct cam_tfe_bus_comp_grp_data   *comp_rsrc_data;
	uint32_t i, j;

	common_data = &bus_priv->common_data;

	for (i = 0; i < CAM_TFE_BUS_COMP_GRP_MAX; i++) {
		if (!(evt_payload->bus_irq_val[0] &
			bus_priv->comp_buf_done_mask))
			break;

		comp_rsrc_data = (struct cam_tfe_bus_comp_grp_data  *)
			bus_priv->comp_grp[i].res_priv;

		if (evt_payload->bus_irq_val[0] &
			BIT(comp_rsrc_data->comp_grp_id +
			bus_priv->common_data.comp_done_shift)) {
			for (j = 0; j < comp_rsrc_data->acquire_dev_cnt; j++) {
				out_rsrc = comp_rsrc_data->out_rsrc[j];
				out_rsrc_data = out_rsrc->res_priv;
				evt_info.res_type = out_rsrc->res_type;
				evt_info.hw_idx = out_rsrc->hw_intf->hw_idx;
				evt_info.res_id = out_rsrc->res_id;
				out_rsrc_data->event_cb(out_rsrc_data->priv,
					CAM_ISP_HW_EVENT_DONE,
					(void *)&evt_info);
			}

			evt_payload->bus_irq_val[0] &=
				~BIT(comp_rsrc_data->comp_grp_id +
				bus_priv->common_data.comp_done_shift);
		}
	}

	return 0;
}

static void cam_tfe_bus_error_bottom_half(
	struct cam_tfe_bus_priv            *bus_priv,
	struct cam_tfe_irq_evt_payload     *evt_payload)
{
	struct cam_tfe_bus_wm_resource_data   *rsrc_data;
	struct cam_tfe_bus_reg_offset_common  *common_reg;
	uint32_t i, overflow_status, image_size_violation_status;
	uint32_t ccif_violation_status;

	common_reg = bus_priv->common_data.common_reg;

	CAM_INFO(CAM_ISP, "BUS IRQ[0]:0x%x BUS IRQ[1]:0x%x",
		evt_payload->bus_irq_val[0], evt_payload->bus_irq_val[1]);

	overflow_status = cam_io_r_mb(bus_priv->common_data.mem_base +
		bus_priv->common_data.common_reg->overflow_status);

	image_size_violation_status  = cam_io_r_mb(
		bus_priv->common_data.mem_base +
		bus_priv->common_data.common_reg->image_size_violation_status);

	ccif_violation_status = cam_io_r_mb(bus_priv->common_data.mem_base +
		bus_priv->common_data.common_reg->ccif_violation_status);

	CAM_INFO(CAM_ISP,
		"ccif violation status:0x%x image size violation:0x%x overflow status:0x%x",
		ccif_violation_status,
		image_size_violation_status,
		overflow_status);

	/* Check the bus errors */
	if (evt_payload->bus_irq_val[0] & BIT(common_reg->cons_violation_shift))
		CAM_INFO(CAM_ISP, "CONS_VIOLATION");

	if (evt_payload->bus_irq_val[0] & BIT(common_reg->violation_shift))
		CAM_INFO(CAM_ISP, "VIOLATION");

	if (evt_payload->bus_irq_val[0] &
		BIT(common_reg->image_size_violation)) {
		CAM_INFO(CAM_ISP, "IMAGE_SIZE_VIOLATION val :0x%x",
			evt_payload->image_size_violation_status);

		for (i = 0; i < CAM_TFE_BUS_MAX_CLIENTS; i++) {
			if (!(evt_payload->image_size_violation_status >> i))
				break;

			if (evt_payload->image_size_violation_status & BIT(i)) {
				rsrc_data = bus_priv->bus_client[i].res_priv;
				CAM_INFO(CAM_ISP,
					"WM:%d width 0x%x height:0x%x format:%d stride:0x%x offset:0x%x encfg:0x%x",
					i,
					rsrc_data->acquired_width,
					rsrc_data->acquired_height,
					rsrc_data->format,
					rsrc_data->acquired_stride,
					rsrc_data->offset,
					rsrc_data->en_cfg);

			CAM_INFO(CAM_ISP,
				"WM:%d current width 0x%x height:0x%x stride:0x%x",
				i,
				rsrc_data->width,
				rsrc_data->height,
				rsrc_data->stride);

			}
		}
	}

	if (overflow_status) {
		for (i = 0; i < CAM_TFE_BUS_MAX_CLIENTS; i++) {

			if (!(evt_payload->overflow_status >> i))
				break;

			if (evt_payload->overflow_status & BIT(i)) {
				rsrc_data = bus_priv->bus_client[i].res_priv;
				CAM_INFO(CAM_ISP,
					"WM:%d %s BUS OVERFLOW width0x%x height:0x%x format:%d stride:0x%x offset:0x%x encfg:%x",
					i,
					rsrc_data->hw_regs->client_name,
					rsrc_data->acquired_width,
					rsrc_data->acquired_height,
					rsrc_data->format,
					rsrc_data->acquired_stride,
					rsrc_data->offset,
					rsrc_data->en_cfg);

				CAM_INFO(CAM_ISP,
					"WM:%d current width:0x%x height:0x%x stride:0x%x",
					i,
					rsrc_data->width,
					rsrc_data->height,
					rsrc_data->stride);
			}
		}
	}
}

static int cam_tfe_bus_bottom_half(void   *priv,
	bool rup_process, struct cam_tfe_irq_evt_payload   *evt_payload,
	bool error_process)
{
	struct cam_tfe_bus_priv          *bus_priv;
	uint32_t val;

	if (!priv || !evt_payload) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "Invalid priv param");
		return -EINVAL;
	}
	bus_priv = (struct cam_tfe_bus_priv   *) priv;

	if (error_process) {
		cam_tfe_bus_error_bottom_half(bus_priv, evt_payload);
		goto end;
	}

	/* if bus errors are there, mask all bus errors */
	if (evt_payload->bus_irq_val[0] & bus_priv->bus_irq_error_mask[0]) {
		val = cam_io_r(bus_priv->common_data.mem_base +
			bus_priv->common_data.common_reg->irq_mask[0]);
		val &= ~bus_priv->bus_irq_error_mask[0];
		cam_io_w(val, bus_priv->common_data.mem_base +
			bus_priv->common_data.common_reg->irq_mask[0]);

	}

	if (rup_process) {
		if (evt_payload->bus_irq_val[0] &
			bus_priv->comp_rup_done_mask)
			cam_tfe_bus_rup_bottom_half(bus_priv, evt_payload);
	} else {
		if (evt_payload->bus_irq_val[0] &
			bus_priv->comp_buf_done_mask)
			cam_tfe_bus_bufdone_bottom_half(bus_priv, evt_payload);
	}

end:
	return 0;

}

static int cam_tfe_bus_update_wm(void *priv, void *cmd_args,
	uint32_t arg_size)
{
	struct cam_tfe_bus_priv              *bus_priv;
	struct cam_isp_hw_get_cmd_update     *update_buf;
	struct cam_buf_io_cfg                *io_cfg;
	struct cam_tfe_bus_tfe_out_data      *tfe_out_data = NULL;
	struct cam_tfe_bus_wm_resource_data  *wm_data = NULL;
	uint32_t *reg_val_pair;
	uint32_t i, j, size = 0;
	uint32_t frame_inc = 0, val;

	bus_priv = (struct cam_tfe_bus_priv  *) priv;
	update_buf = (struct cam_isp_hw_get_cmd_update *) cmd_args;

	tfe_out_data = (struct cam_tfe_bus_tfe_out_data *)
		update_buf->res->res_priv;

	if (!tfe_out_data || !tfe_out_data->cdm_util_ops) {
		CAM_ERR(CAM_ISP, "Failed! Invalid data");
		return -EINVAL;
	}

	if (update_buf->wm_update->num_buf != tfe_out_data->num_wm) {
		CAM_ERR(CAM_ISP,
			"Failed! Invalid number buffers:%d required:%d",
			update_buf->wm_update->num_buf, tfe_out_data->num_wm);
		return -EINVAL;
	}

	reg_val_pair = &tfe_out_data->common_data->io_buf_update[0];
	io_cfg = update_buf->wm_update->io_cfg;

	for (i = 0, j = 0; i < tfe_out_data->num_wm; i++) {
		if (j >= (MAX_REG_VAL_PAIR_SIZE - MAX_BUF_UPDATE_REG_NUM * 2)) {
			CAM_ERR(CAM_ISP,
				"reg_val_pair %d exceeds the array limit %zu",
				j, MAX_REG_VAL_PAIR_SIZE);
			return -ENOMEM;
		}

		wm_data = tfe_out_data->wm_res[i]->res_priv;
		/* update width register */
		val = ((wm_data->height << 16) | (wm_data->width & 0xFFFF));
		CAM_TFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
			wm_data->hw_regs->image_cfg_0, val);
		CAM_DBG(CAM_ISP, "WM:%d image height and width 0x%x",
			wm_data->index, reg_val_pair[j-1]);

		val = wm_data->offset;
		CAM_TFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
			wm_data->hw_regs->image_cfg_1, val);
		CAM_DBG(CAM_ISP, "WM:%d xinit 0x%x",
			wm_data->index, reg_val_pair[j-1]);

		if ((wm_data->index < 7) || ((wm_data->index >= 7) &&
			(wm_data->mode == CAM_ISP_TFE_WM_LINE_BASED_MODE)) ||
			(wm_data->out_id == CAM_TFE_BUS_TFE_OUT_PDAF)) {
			CAM_TFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
				wm_data->hw_regs->image_cfg_2,
				io_cfg->planes[i].plane_stride);
			wm_data->stride = io_cfg->planes[i].plane_stride;
			CAM_DBG(CAM_ISP, "WM %d image stride 0x%x",
				wm_data->index, reg_val_pair[j-1]);
		}

		frame_inc = io_cfg->planes[i].plane_stride *
			io_cfg->planes[i].slice_height;

		CAM_TFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
			wm_data->hw_regs->image_addr,
			update_buf->wm_update->image_buf[i]);
		CAM_DBG(CAM_ISP, "WM %d image address 0x%x",
			wm_data->index, reg_val_pair[j-1]);

		CAM_TFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
			wm_data->hw_regs->frame_incr, frame_inc);
		CAM_DBG(CAM_ISP, "WM %d frame_inc %d",
			wm_data->index, reg_val_pair[j-1]);

		/* enable the WM */
		CAM_TFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
			wm_data->hw_regs->cfg,
			wm_data->en_cfg);
	}

	size = tfe_out_data->cdm_util_ops->cdm_required_size_reg_random(j/2);

	/* cdm util returns dwords, need to convert to bytes */
	if ((size * 4) > update_buf->cmd.size) {
		CAM_ERR(CAM_ISP,
			"Failed! Buf size:%d insufficient, expected size:%d",
			update_buf->cmd.size, size);
		return -ENOMEM;
	}

	tfe_out_data->cdm_util_ops->cdm_write_regrandom(
		update_buf->cmd.cmd_buf_addr, j/2, reg_val_pair);

	/* cdm util returns dwords, need to convert to bytes */
	update_buf->cmd.used_bytes = size * 4;

	return 0;
}

static int cam_tfe_bus_update_hfr(void *priv, void *cmd_args,
	uint32_t arg_size)
{
	struct cam_tfe_bus_priv                  *bus_priv;
	struct cam_isp_hw_get_cmd_update         *update_hfr;
	struct cam_tfe_bus_tfe_out_data          *tfe_out_data = NULL;
	struct cam_tfe_bus_wm_resource_data      *wm_data = NULL;
	struct cam_isp_tfe_port_hfr_config       *hfr_cfg = NULL;
	uint32_t *reg_val_pair;
	uint32_t  i, j, size = 0;

	bus_priv = (struct cam_tfe_bus_priv  *) priv;
	update_hfr =  (struct cam_isp_hw_get_cmd_update *) cmd_args;

	tfe_out_data = (struct cam_tfe_bus_tfe_out_data *)
		update_hfr->res->res_priv;

	if (!tfe_out_data || !tfe_out_data->cdm_util_ops) {
		CAM_ERR(CAM_ISP, "Failed! Invalid data");
		return -EINVAL;
	}

	reg_val_pair = &tfe_out_data->common_data->io_buf_update[0];
	hfr_cfg = (struct cam_isp_tfe_port_hfr_config *)update_hfr->data;

	for (i = 0, j = 0; i < tfe_out_data->num_wm; i++) {
		if (j >= (MAX_REG_VAL_PAIR_SIZE - MAX_BUF_UPDATE_REG_NUM * 2)) {
			CAM_ERR(CAM_ISP,
				"reg_val_pair %d exceeds the array limit %zu",
				j, MAX_REG_VAL_PAIR_SIZE);
			return -ENOMEM;
		}

		wm_data = tfe_out_data->wm_res[i]->res_priv;
		CAM_TFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
			wm_data->hw_regs->framedrop_pattern,
			hfr_cfg->framedrop_pattern);
		wm_data->framedrop_pattern = hfr_cfg->framedrop_pattern;
		CAM_DBG(CAM_ISP, "WM:%d framedrop pattern 0x%x",
			wm_data->index, wm_data->framedrop_pattern);

		CAM_TFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
			wm_data->hw_regs->framedrop_period,
			hfr_cfg->framedrop_period);
		wm_data->framedrop_period = hfr_cfg->framedrop_period;
		CAM_DBG(CAM_ISP, "WM:%d framedrop period 0x%x",
			wm_data->index, wm_data->framedrop_period);

		CAM_TFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
			wm_data->hw_regs->irq_subsample_period,
			hfr_cfg->subsample_period);
		wm_data->irq_subsample_period = hfr_cfg->subsample_period;
		CAM_DBG(CAM_ISP, "WM:%d irq subsample period 0x%x",
			wm_data->index, wm_data->irq_subsample_period);

		CAM_TFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
			wm_data->hw_regs->irq_subsample_pattern,
			hfr_cfg->subsample_pattern);
		wm_data->irq_subsample_pattern = hfr_cfg->subsample_pattern;
		CAM_DBG(CAM_ISP, "WM:%d irq subsample pattern 0x%x",
			wm_data->index, wm_data->irq_subsample_pattern);
	}

	size = tfe_out_data->cdm_util_ops->cdm_required_size_reg_random(j/2);

	/* cdm util returns dwords, need to convert to bytes */
	if ((size * 4) > update_hfr->cmd.size) {
		CAM_ERR(CAM_ISP,
			"Failed! Buf size:%d insufficient, expected size:%d",
			update_hfr->cmd.size, size);
		return -ENOMEM;
	}

	tfe_out_data->cdm_util_ops->cdm_write_regrandom(
		update_hfr->cmd.cmd_buf_addr, j/2, reg_val_pair);

	/* cdm util returns dwords, need to convert to bytes */
	update_hfr->cmd.used_bytes = size * 4;

	return 0;
}

static int cam_tfe_bus_update_stripe_cfg(void *priv, void *cmd_args,
	uint32_t arg_size)
{
	struct cam_tfe_bus_priv                     *bus_priv;
	struct cam_tfe_dual_update_args             *stripe_args;
	struct cam_tfe_bus_tfe_out_data             *tfe_out_data = NULL;
	struct cam_tfe_bus_wm_resource_data         *wm_data = NULL;
	struct cam_isp_tfe_dual_stripe_config       *stripe_config;
	uint32_t i;

	bus_priv = (struct cam_tfe_bus_priv  *) priv;
	stripe_args = (struct cam_tfe_dual_update_args *)cmd_args;

	tfe_out_data = (struct cam_tfe_bus_tfe_out_data *)
		stripe_args->res->res_priv;

	if (!tfe_out_data) {
		CAM_ERR(CAM_ISP, "Failed! Invalid data");
		return -EINVAL;
	}

	if (stripe_args->res->res_id < CAM_ISP_TFE_OUT_RES_BASE ||
		stripe_args->res->res_id >= CAM_ISP_TFE_OUT_RES_MAX)
		return 0;

	stripe_config = (struct cam_isp_tfe_dual_stripe_config *)
		stripe_args->stripe_config;

	for (i = 0; i < tfe_out_data->num_wm; i++) {
		stripe_config = &stripe_args->stripe_config[i];
		wm_data = tfe_out_data->wm_res[i]->res_priv;
		wm_data->width = stripe_config->width;
		wm_data->offset = stripe_config->offset;
		CAM_DBG(CAM_ISP, "id:%x WM:%d width:0x%x offset:0x%x",
			stripe_args->res->res_id, wm_data->index,
			wm_data->width, wm_data->offset);
	}

	return 0;
}

static int cam_tfe_bus_init_hw(void *hw_priv,
	void *init_hw_args, uint32_t arg_size)
{
	struct cam_tfe_bus_priv    *bus_priv = hw_priv;
	uint32_t                   i, top_irq_reg_mask[3] = {0};
	int rc = -EINVAL;

	if (!bus_priv) {
		CAM_ERR(CAM_ISP, "Invalid args");
		return -EINVAL;
	}

	top_irq_reg_mask[0] = (1 << bus_priv->top_bus_wr_irq_shift);

	rc  = cam_tfe_irq_config(bus_priv->common_data.tfe_core_data,
		top_irq_reg_mask, CAM_TFE_TOP_IRQ_REG_NUM, true);
	if (rc)
		return rc;

	/* configure the error irq */
	for (i = 0; i < CAM_TFE_BUS_IRQ_REGISTERS_MAX; i++)
		cam_io_w(bus_priv->bus_irq_error_mask[i],
			bus_priv->common_data.mem_base +
			bus_priv->common_data.common_reg->irq_mask[i]);

	return 0;
}

static int cam_tfe_bus_deinit_hw(void *hw_priv,
	void *deinit_hw_args, uint32_t arg_size)
{
	struct cam_tfe_bus_priv    *bus_priv = hw_priv;
	uint32_t                    top_irq_reg_mask[3] = {0};
	int                              rc = 0;

	if (!bus_priv) {
		CAM_ERR(CAM_ISP, "Error: Invalid args");
		return -EINVAL;
	}
	top_irq_reg_mask[0] = (1 << bus_priv->top_bus_wr_irq_shift);
	rc  = cam_tfe_irq_config(bus_priv->common_data.tfe_core_data,
		top_irq_reg_mask, CAM_TFE_TOP_IRQ_REG_NUM, false);
	if (rc)
		return rc;

	/* configure the error irq */
	cam_io_w(0, bus_priv->common_data.mem_base +
		bus_priv->common_data.common_reg->irq_mask[0]);

	cam_io_w_mb(0, bus_priv->common_data.mem_base +
		bus_priv->common_data.common_reg->irq_mask[1]);

	return rc;
}

static int cam_tfe_bus_process_cmd(void *priv,
	uint32_t cmd_type, void *cmd_args, uint32_t arg_size)
{
	struct cam_tfe_bus_priv      *bus_priv;
	int rc = -EINVAL;
	uint32_t i, val;

	if (!priv || !cmd_args) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "Invalid input arguments");
		return -EINVAL;
	}

	switch (cmd_type) {
	case CAM_ISP_HW_CMD_GET_BUF_UPDATE:
		rc = cam_tfe_bus_update_wm(priv, cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_GET_HFR_UPDATE:
		rc = cam_tfe_bus_update_hfr(priv, cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_GET_SECURE_MODE:
		rc = cam_tfe_bus_get_secure_mode(priv, cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_STRIPE_UPDATE:
		rc = cam_tfe_bus_update_stripe_cfg(priv,
			cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_STOP_BUS_ERR_IRQ:
		bus_priv = (struct cam_tfe_bus_priv  *) priv;
		/* disable the bus error interrupts */
		for (i = 0; i < CAM_TFE_BUS_IRQ_REGISTERS_MAX; i++) {
			val = cam_io_r(bus_priv->common_data.mem_base +
				bus_priv->common_data.common_reg->irq_mask[i]);
			val &= ~bus_priv->bus_irq_error_mask[i];
			cam_io_w(val, bus_priv->common_data.mem_base +
				bus_priv->common_data.common_reg->irq_mask[i]);
		}
		break;
	default:
		CAM_ERR_RATE_LIMIT(CAM_ISP, "Invalid camif process command:%d",
			cmd_type);
		break;
	}

	return rc;
}

int cam_tfe_bus_init(
	struct cam_hw_soc_info               *soc_info,
	struct cam_hw_intf                   *hw_intf,
	void                                 *bus_hw_info,
	void                                 *core_data,
	struct cam_tfe_bus                  **tfe_bus)
{
	int i, rc = 0;
	struct cam_tfe_bus_priv    *bus_priv = NULL;
	struct cam_tfe_bus         *tfe_bus_local;
	struct cam_tfe_bus_hw_info *hw_info = bus_hw_info;

	if (!soc_info || !hw_intf || !bus_hw_info) {
		CAM_ERR(CAM_ISP,
			"Invalid params soc_info:%pK hw_intf:%pK hw_info%pK",
			soc_info, hw_intf, bus_hw_info);
		rc = -EINVAL;
		goto end;
	}

	tfe_bus_local = kzalloc(sizeof(struct cam_tfe_bus), GFP_KERNEL);
	if (!tfe_bus_local) {
		CAM_DBG(CAM_ISP, "Failed to alloc for tfe_bus");
		rc = -ENOMEM;
		goto end;
	}

	bus_priv = kzalloc(sizeof(struct cam_tfe_bus_priv),
		GFP_KERNEL);
	if (!bus_priv) {
		CAM_DBG(CAM_ISP, "Failed to alloc for tfe_bus_priv");
		rc = -ENOMEM;
		goto free_bus_local;
	}
	tfe_bus_local->bus_priv = bus_priv;

	bus_priv->num_client                   = hw_info->num_client;
	bus_priv->num_out                      = hw_info->num_out;
	bus_priv->top_bus_wr_irq_shift         = hw_info->top_bus_wr_irq_shift;
	bus_priv->common_data.comp_done_shift  = hw_info->comp_done_shift;

	bus_priv->common_data.num_sec_out      = 0;
	bus_priv->common_data.secure_mode      = CAM_SECURE_MODE_NON_SECURE;
	bus_priv->common_data.core_index       = soc_info->index;
	bus_priv->common_data.mem_base         =
		CAM_SOC_GET_REG_MAP_START(soc_info, TFE_CORE_BASE_IDX);
	bus_priv->common_data.hw_intf          = hw_intf;
	bus_priv->common_data.tfe_core_data    = core_data;
	bus_priv->common_data.common_reg       = &hw_info->common_reg;
	bus_priv->comp_buf_done_mask      = hw_info->comp_buf_done_mask;
	bus_priv->comp_rup_done_mask      = hw_info->comp_rup_done_mask;

	for (i = 0; i < CAM_TFE_BUS_IRQ_REGISTERS_MAX; i++)
		bus_priv->bus_irq_error_mask[i] =
			hw_info->bus_irq_error_mask[i];

	if (strnstr(soc_info->compatible, "lite",
		strlen(soc_info->compatible)) != NULL)
		bus_priv->common_data.is_lite = true;
	else
		bus_priv->common_data.is_lite = false;

	for (i = 0; i < CAM_TFE_BUS_RUP_GRP_MAX; i++)
		bus_priv->common_data.rup_irq_enable[i] = false;

	mutex_init(&bus_priv->common_data.bus_mutex);

	INIT_LIST_HEAD(&bus_priv->free_comp_grp);
	INIT_LIST_HEAD(&bus_priv->used_comp_grp);

	for (i = 0; i < bus_priv->num_client; i++) {
		rc = cam_tfe_bus_init_wm_resource(i, bus_priv, bus_hw_info,
			&bus_priv->bus_client[i]);
		if (rc < 0) {
			CAM_ERR(CAM_ISP, "Init WM failed rc=%d", rc);
			goto deinit_wm;
		}
	}

	for (i = 0; i < CAM_TFE_BUS_COMP_GRP_MAX; i++) {
		rc = cam_tfe_bus_init_comp_grp(i, soc_info,
			bus_priv, bus_hw_info,
			&bus_priv->comp_grp[i]);
		if (rc < 0) {
			CAM_ERR(CAM_ISP, "Init Comp Grp failed rc=%d", rc);
			goto deinit_comp_grp;
		}
	}

	for (i = 0; i < bus_priv->num_out; i++) {
		rc = cam_tfe_bus_init_tfe_out_resource(i, bus_priv,
			bus_hw_info);
		if (rc < 0) {
			CAM_ERR(CAM_ISP, "Init TFE Out failed rc=%d", rc);
			goto deinit_tfe_out;
		}
	}

	spin_lock_init(&bus_priv->common_data.spin_lock);

	tfe_bus_local->hw_ops.reserve      = cam_tfe_bus_acquire_tfe_out;
	tfe_bus_local->hw_ops.release      = cam_tfe_bus_release_tfe_out;
	tfe_bus_local->hw_ops.start        = cam_tfe_bus_start_tfe_out;
	tfe_bus_local->hw_ops.stop         = cam_tfe_bus_stop_tfe_out;
	tfe_bus_local->hw_ops.init         = cam_tfe_bus_init_hw;
	tfe_bus_local->hw_ops.deinit       = cam_tfe_bus_deinit_hw;
	tfe_bus_local->bottom_half_handler = cam_tfe_bus_bottom_half;
	tfe_bus_local->hw_ops.process_cmd  = cam_tfe_bus_process_cmd;

	*tfe_bus = tfe_bus_local;

	return rc;

deinit_tfe_out:
	if (i < 0)
		i = CAM_TFE_BUS_TFE_OUT_MAX;
	for (--i; i >= 0; i--)
		cam_tfe_bus_deinit_tfe_out_resource(&bus_priv->tfe_out[i]);

deinit_comp_grp:
	if (i < 0)
		i = CAM_TFE_BUS_COMP_GRP_MAX;
	for (--i; i >= 0; i--)
		cam_tfe_bus_deinit_comp_grp(&bus_priv->comp_grp[i]);

deinit_wm:
	if (i < 0)
		i = bus_priv->num_client;
	for (--i; i >= 0; i--)
		cam_tfe_bus_deinit_wm_resource(&bus_priv->bus_client[i]);

	kfree(tfe_bus_local->bus_priv);

free_bus_local:
	kfree(tfe_bus_local);

end:
	return rc;
}

int cam_tfe_bus_deinit(
	struct cam_tfe_bus                  **tfe_bus)
{
	int i, rc = 0;
	struct cam_tfe_bus_priv         *bus_priv = NULL;
	struct cam_tfe_bus              *tfe_bus_local;

	if (!tfe_bus || !*tfe_bus) {
		CAM_ERR(CAM_ISP, "Invalid input");
		return -EINVAL;
	}
	tfe_bus_local = *tfe_bus;
	bus_priv = tfe_bus_local->bus_priv;

	if (!bus_priv) {
		CAM_ERR(CAM_ISP, "bus_priv is NULL");
		rc = -ENODEV;
		goto free_bus_local;
	}

	for (i = 0; i < bus_priv->num_client; i++) {
		rc = cam_tfe_bus_deinit_wm_resource(
			&bus_priv->bus_client[i]);
		if (rc < 0)
			CAM_ERR(CAM_ISP,
				"Deinit WM failed rc=%d", rc);
	}

	for (i = 0; i < CAM_TFE_BUS_COMP_GRP_MAX; i++) {
		rc = cam_tfe_bus_deinit_comp_grp(&bus_priv->comp_grp[i]);
		if (rc < 0)
			CAM_ERR(CAM_ISP,
				"Deinit Comp Grp failed rc=%d", rc);
	}

	for (i = 0; i < CAM_TFE_BUS_TFE_OUT_MAX; i++) {
		rc = cam_tfe_bus_deinit_tfe_out_resource(
			&bus_priv->tfe_out[i]);
		if (rc < 0)
			CAM_ERR(CAM_ISP,
				"Deinit TFE Out failed rc=%d", rc);
	}

	INIT_LIST_HEAD(&bus_priv->free_comp_grp);
	INIT_LIST_HEAD(&bus_priv->used_comp_grp);

	mutex_destroy(&bus_priv->common_data.bus_mutex);
	kfree(tfe_bus_local->bus_priv);

free_bus_local:
	kfree(tfe_bus_local);

	*tfe_bus = NULL;

	return rc;
}
