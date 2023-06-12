// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */


#include <linux/ratelimit.h>
#include <linux/slab.h>

#include <media/cam_isp.h>

#include "cam_io_util.h"
#include "cam_debug_util.h"
#include "cam_cdm_util.h"
#include "cam_hw_intf.h"
#include "cam_ife_hw_mgr.h"
#include "cam_sfe_hw_intf.h"
#include "cam_irq_controller.h"
#include "cam_tasklet_util.h"
#include "cam_sfe_bus.h"
#include "cam_sfe_bus_wr.h"
#include "cam_sfe_core.h"
#include "cam_sfe_soc.h"
#include "cam_debug_util.h"
#include "cam_cpas_api.h"
#include "cam_trace.h"

static const char drv_name[] = "sfe_bus_wr";

#define CAM_SFE_BUS_WR_PAYLOAD_MAX             256

#define CAM_SFE_RDI_BUS_DEFAULT_WIDTH          0xFFFF
#define CAM_SFE_RDI_BUS_DEFAULT_STRIDE         0xFFFF

#define MAX_BUF_UPDATE_REG_NUM   \
	(sizeof(struct cam_sfe_bus_reg_offset_bus_client) / 4)
#define MAX_REG_VAL_PAIR_SIZE    \
	(MAX_BUF_UPDATE_REG_NUM * 2 * CAM_PACKET_MAX_PLANES)

static uint32_t bus_wr_error_irq_mask[1] = {
	0xD0000000,
};

enum cam_sfe_bus_wr_packer_format {
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
	PACKER_FMT_MIPI10,
	PACKER_FMT_MIPI12,
	PACKER_FMT_MIPI14,
	PACKER_FMT_MIPI20,
	PACKER_FMT_PLAIN_32_20BPP,
	PACKER_FMT_MAX,
};

struct cam_sfe_bus_wr_common_data {
	uint32_t                                    core_index;
	uint32_t                                    hw_version;
	void __iomem                               *mem_base;
	struct cam_hw_intf                         *hw_intf;
	void                                       *sfe_irq_controller;
	void                                       *buf_done_controller;
	void                                       *bus_irq_controller;
	struct cam_sfe_bus_reg_offset_common       *common_reg;
	uint32_t                                    io_buf_update[
		MAX_REG_VAL_PAIR_SIZE];
	struct list_head                            free_payload_list;
	spinlock_t                                  spin_lock;
	struct cam_sfe_bus_wr_irq_evt_payload       evt_payload[
		CAM_SFE_BUS_WR_PAYLOAD_MAX];
	struct mutex                                bus_mutex;
	uint32_t                                    secure_mode;
	uint32_t                                    num_sec_out;
	uint32_t                                    addr_no_sync;
	uint32_t                                    comp_done_shift;
	bool                                        hw_init;
	cam_hw_mgr_event_cb_func                    event_cb;
};

struct cam_sfe_wr_scratch_buf_info {
	uint32_t     width;
	uint32_t     height;
	uint32_t     stride;
	uint32_t     slice_height;
	dma_addr_t   io_addr;
};

struct cam_sfe_bus_wr_wm_resource_data {
	uint32_t             index;
	struct cam_sfe_bus_wr_common_data         *common_data;
	struct cam_sfe_bus_reg_offset_bus_client  *hw_regs;
	struct cam_sfe_wr_scratch_buf_info scratch_buf_info;

	bool                 init_cfg_done;
	bool                 hfr_cfg_done;

	uint32_t             offset;
	uint32_t             width;
	uint32_t             height;
	uint32_t             stride;
	uint32_t             format;
	enum cam_sfe_bus_wr_packer_format pack_fmt;

	uint32_t             packer_cfg;
	uint32_t             h_init;

	uint32_t             irq_subsample_period;
	uint32_t             irq_subsample_pattern;
	uint32_t             framedrop_period;
	uint32_t             framedrop_pattern;

	uint32_t             en_cfg;
	uint32_t             is_dual;

	uint32_t             acquired_width;
	uint32_t             acquired_height;
};

struct cam_sfe_bus_wr_comp_grp_data {
	enum cam_sfe_bus_wr_comp_grp_type          comp_grp_type;
	struct cam_sfe_bus_wr_common_data         *common_data;

	uint32_t                                   is_master;
	uint32_t                                   is_dual;
	uint32_t                                   dual_slave_core;
	uint32_t                                   intra_client_mask;
	uint32_t                                   addr_sync_mode;
	uint32_t                                   composite_mask;

	uint32_t                                   acquire_dev_cnt;
	uint32_t                                   irq_trigger_cnt;
};

struct cam_sfe_bus_wr_out_data {
	uint32_t                              out_type;
	uint32_t                              source_group;
	struct cam_sfe_bus_wr_common_data    *common_data;

	uint32_t                           num_wm;
	struct cam_isp_resource_node      *wm_res;

	struct cam_isp_resource_node      *comp_grp;
	struct list_head                   sfe_out_list;

	uint32_t                           is_master;
	uint32_t                           is_dual;

	uint32_t                           format;
	uint32_t                           max_width;
	uint32_t                           max_height;
	struct cam_cdm_utils_ops          *cdm_util_ops;
	uint32_t                           secure_mode;
	void                              *priv;
};

struct cam_sfe_bus_wr_priv {
	struct cam_sfe_bus_wr_common_data   common_data;
	uint32_t                            num_client;
	uint32_t                            num_out;
	uint32_t                            num_comp_grp;
	uint32_t                            top_irq_shift;

	struct cam_isp_resource_node       *comp_grp;
	struct cam_isp_resource_node       *sfe_out;

	struct list_head                    free_comp_grp;
	struct list_head                    used_comp_grp;

	int                                 bus_irq_handle;
	int                                 error_irq_handle;
	void                               *tasklet_info;
};

static int cam_sfe_bus_wr_process_cmd(
	struct cam_isp_resource_node *priv,
	uint32_t cmd_type, void *cmd_args,
	uint32_t arg_size);

static bool cam_sfe_bus_can_be_secure(uint32_t out_type)
{
	switch (out_type) {
	case CAM_SFE_BUS_SFE_OUT_RAW_DUMP:
	case CAM_SFE_BUS_SFE_OUT_RDI0:
	case CAM_SFE_BUS_SFE_OUT_RDI1:
	case CAM_SFE_BUS_SFE_OUT_RDI2:
	case CAM_SFE_BUS_SFE_OUT_RDI3:
	case CAM_SFE_BUS_SFE_OUT_RDI4:
		return true;
	case CAM_SFE_BUS_SFE_OUT_LCR:
	case CAM_SFE_BUS_SFE_OUT_BE_0:
	case CAM_SFE_BUS_SFE_OUT_BHIST_0:
	case CAM_SFE_BUS_SFE_OUT_BE_1:
	case CAM_SFE_BUS_SFE_OUT_BHIST_1:
	case CAM_SFE_BUS_SFE_OUT_BE_2:
	case CAM_SFE_BUS_SFE_OUT_BHIST_2:
	default:
		return false;
	}
}

static enum cam_sfe_bus_sfe_out_type
	cam_sfe_bus_wr_get_out_res_id(uint32_t res_type)
{
	switch (res_type) {
	case CAM_ISP_SFE_OUT_RES_RAW_DUMP:
		return CAM_SFE_BUS_SFE_OUT_RAW_DUMP;
	case CAM_ISP_SFE_OUT_RES_RDI_0:
		return CAM_SFE_BUS_SFE_OUT_RDI0;
	case CAM_ISP_SFE_OUT_RES_RDI_1:
		return CAM_SFE_BUS_SFE_OUT_RDI1;
	case CAM_ISP_SFE_OUT_RES_RDI_2:
		return CAM_SFE_BUS_SFE_OUT_RDI2;
	case CAM_ISP_SFE_OUT_RES_RDI_3:
		return CAM_SFE_BUS_SFE_OUT_RDI3;
	case CAM_ISP_SFE_OUT_RES_RDI_4:
		return CAM_SFE_BUS_SFE_OUT_RDI4;
	case CAM_ISP_SFE_OUT_BE_STATS_0:
		return CAM_SFE_BUS_SFE_OUT_BE_0;
	case CAM_ISP_SFE_OUT_BHIST_STATS_0:
		return CAM_SFE_BUS_SFE_OUT_BHIST_0;
	case CAM_ISP_SFE_OUT_BE_STATS_1:
		return CAM_SFE_BUS_SFE_OUT_BE_1;
	case CAM_ISP_SFE_OUT_BHIST_STATS_1:
		return CAM_SFE_BUS_SFE_OUT_BHIST_1;
	case CAM_ISP_SFE_OUT_BE_STATS_2:
		return CAM_SFE_BUS_SFE_OUT_BE_2;
	case CAM_ISP_SFE_OUT_BHIST_STATS_2:
		return CAM_SFE_BUS_SFE_OUT_BHIST_2;
	case CAM_ISP_SFE_OUT_RES_LCR:
		return CAM_SFE_BUS_SFE_OUT_LCR;
	default:
		return CAM_SFE_BUS_SFE_OUT_MAX;
	}
}

static int cam_sfe_bus_get_comp_sfe_out_res_id_list(
	uint32_t comp_mask, uint32_t *out_list, int *num_out)
{
	int count = 0;

	if (comp_mask & (1 << CAM_SFE_BUS_SFE_OUT_RDI0))
		out_list[count++] = CAM_ISP_SFE_OUT_RES_RDI_0;

	if (comp_mask & (1 << CAM_SFE_BUS_SFE_OUT_RDI1))
		out_list[count++] = CAM_ISP_SFE_OUT_RES_RDI_1;

	if (comp_mask & (1 << CAM_SFE_BUS_SFE_OUT_RDI2))
		out_list[count++] = CAM_ISP_SFE_OUT_RES_RDI_2;

	if (comp_mask & (1 << CAM_SFE_BUS_SFE_OUT_RDI3))
		out_list[count++] = CAM_ISP_SFE_OUT_RES_RDI_3;

	if (comp_mask & (1 << CAM_SFE_BUS_SFE_OUT_RDI4))
		out_list[count++] = CAM_ISP_SFE_OUT_RES_RDI_4;

	if (comp_mask & (1 << CAM_SFE_BUS_SFE_OUT_RAW_DUMP))
		out_list[count++] = CAM_ISP_SFE_OUT_RES_RAW_DUMP;

	if (comp_mask & (1 << CAM_SFE_BUS_SFE_OUT_BE_0))
		out_list[count++] = CAM_ISP_SFE_OUT_BE_STATS_0;

	if (comp_mask & (1 << CAM_SFE_BUS_SFE_OUT_BHIST_0))
		out_list[count++] = CAM_ISP_SFE_OUT_BHIST_STATS_0;

	if (comp_mask & (1 << CAM_SFE_BUS_SFE_OUT_BE_1))
		out_list[count++] = CAM_ISP_SFE_OUT_BE_STATS_1;

	if (comp_mask & (1 << CAM_SFE_BUS_SFE_OUT_BHIST_1))
		out_list[count++] = CAM_ISP_SFE_OUT_BHIST_STATS_1;

	if (comp_mask & (1 << CAM_SFE_BUS_SFE_OUT_BE_2))
		out_list[count++] = CAM_ISP_SFE_OUT_BE_STATS_2;

	if (comp_mask & (1 << CAM_SFE_BUS_SFE_OUT_BHIST_2))
		out_list[count++] = CAM_ISP_SFE_OUT_BHIST_STATS_2;

	if (comp_mask & (1 << CAM_SFE_BUS_SFE_OUT_LCR))
		out_list[count++] = CAM_ISP_SFE_OUT_RES_LCR;

	*num_out = count;
	return 0;
}

static enum cam_sfe_bus_wr_packer_format
	cam_sfe_bus_get_packer_fmt(uint32_t out_fmt, int wm_index)
{
	switch (out_fmt) {
	case CAM_FORMAT_MIPI_RAW_6:
	case CAM_FORMAT_MIPI_RAW_8:
	case CAM_FORMAT_MIPI_RAW_16:
	case CAM_FORMAT_PLAIN16_8:
	case CAM_FORMAT_PLAIN128:
	case CAM_FORMAT_PD8:
		return PACKER_FMT_PLAIN_128;
	case CAM_FORMAT_MIPI_RAW_10:
		if (wm_index == 0)
			return PACKER_FMT_MIPI10;
		else
			return PACKER_FMT_PLAIN_128;
	case CAM_FORMAT_MIPI_RAW_12:
		if (wm_index == 0)
			return PACKER_FMT_MIPI12;
		else
			return PACKER_FMT_PLAIN_128;
	case CAM_FORMAT_MIPI_RAW_14:
		if (wm_index == 0)
			return PACKER_FMT_MIPI14;
		else
			return PACKER_FMT_PLAIN_128;
	case CAM_FORMAT_MIPI_RAW_20:
		if (wm_index == 0)
			return PACKER_FMT_MIPI20;
		else
			return PACKER_FMT_PLAIN_128;
	case CAM_FORMAT_PLAIN8:
		return PACKER_FMT_PLAIN_8;
	case CAM_FORMAT_NV12:
	case CAM_FORMAT_UBWC_NV12:
	case CAM_FORMAT_UBWC_NV12_4R:
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
	case CAM_FORMAT_PLAIN32:
	case CAM_FORMAT_ARGB:
		return PACKER_FMT_PLAIN_32;
	case CAM_FORMAT_PLAIN64:
	case CAM_FORMAT_ARGB_16:
	case CAM_FORMAT_PD10:
		return PACKER_FMT_PLAIN_64;
	case CAM_FORMAT_UBWC_TP10:
	case CAM_FORMAT_TP10:
		return PACKER_FMT_TP_10;
	default:
		return PACKER_FMT_MAX;
	}
}

static int cam_sfe_bus_acquire_wm(
	struct cam_sfe_bus_wr_priv            *bus_priv,
	struct cam_isp_out_port_generic_info  *out_port_info,
	void                                  *tasklet,
	enum cam_sfe_bus_sfe_out_type          sfe_out_res_id,
	enum cam_sfe_bus_plane_type            plane,
	struct cam_isp_resource_node          *wm_res,
	uint32_t                              *comp_done_mask,
	uint32_t                               is_dual,
	enum cam_sfe_bus_wr_comp_grp_type     *comp_grp_id)
{
	int32_t wm_idx = 0;
	struct cam_sfe_bus_wr_wm_resource_data  *rsrc_data = NULL;
	char wm_mode[50] = {0};

	if (wm_res->res_state != CAM_ISP_RESOURCE_STATE_AVAILABLE) {
		CAM_ERR(CAM_SFE, "WM:%d not available state:%d",
			wm_idx, wm_res->res_state);
		return -EALREADY;
	}

	wm_res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;
	wm_res->tasklet_info = tasklet;

	rsrc_data = wm_res->res_priv;
	wm_idx = rsrc_data->index;
	rsrc_data->format = out_port_info->format;
	rsrc_data->pack_fmt = cam_sfe_bus_get_packer_fmt(rsrc_data->format,
		wm_idx);

	rsrc_data->width = out_port_info->width;
	rsrc_data->height = out_port_info->height;
	rsrc_data->is_dual = is_dual;
	/* Set WM offset value to default */
	rsrc_data->offset  = 0;
	CAM_DBG(CAM_SFE, "WM:%d width %d height %d", rsrc_data->index,
		rsrc_data->width, rsrc_data->height);

	if ((sfe_out_res_id >= CAM_SFE_BUS_SFE_OUT_RDI0) &&
		(sfe_out_res_id <= CAM_SFE_BUS_SFE_OUT_RDI4)) {

		rsrc_data->pack_fmt = 0x0;
		switch (rsrc_data->format) {
		case CAM_FORMAT_MIPI_RAW_6:
		case CAM_FORMAT_MIPI_RAW_8:
		case CAM_FORMAT_MIPI_RAW_10:
		case CAM_FORMAT_MIPI_RAW_12:
		case CAM_FORMAT_MIPI_RAW_14:
		case CAM_FORMAT_MIPI_RAW_16:
		case CAM_FORMAT_MIPI_RAW_20:
		case CAM_FORMAT_PLAIN128:
		case CAM_FORMAT_PLAIN32_20:
			rsrc_data->width = CAM_SFE_RDI_BUS_DEFAULT_WIDTH;
			rsrc_data->height = 0;
			rsrc_data->stride = CAM_SFE_RDI_BUS_DEFAULT_STRIDE;
			rsrc_data->en_cfg = (0x1 << 16) | 0x1;
			break;
		case CAM_FORMAT_PLAIN8:
			rsrc_data->en_cfg = 0x1;
			rsrc_data->stride = rsrc_data->width * 2;
			break;
		case CAM_FORMAT_PLAIN16_10:
		case CAM_FORMAT_PLAIN16_12:
		case CAM_FORMAT_PLAIN16_14:
		case CAM_FORMAT_PLAIN16_16:
			rsrc_data->width =
				ALIGNUP(rsrc_data->width * 2, 16) / 16;
			rsrc_data->en_cfg = 0x1;
			break;
		case CAM_FORMAT_PLAIN64:
			rsrc_data->width =
				ALIGNUP(rsrc_data->width * 8, 16) / 16;
			rsrc_data->en_cfg = 0x1;
			break;
		default:
			CAM_ERR(CAM_SFE, "Unsupported RDI format %d",
				rsrc_data->format);
			return -EINVAL;
		}

	} else if (sfe_out_res_id == CAM_SFE_BUS_SFE_OUT_RAW_DUMP) {
		rsrc_data->stride = rsrc_data->width;
		rsrc_data->en_cfg = 0x1;
	} else if ((sfe_out_res_id >= CAM_SFE_BUS_SFE_OUT_BE_0) &&
		(sfe_out_res_id <= CAM_SFE_BUS_SFE_OUT_BHIST_2)) {

		rsrc_data->width = 0;
		rsrc_data->height = 0;
		rsrc_data->stride = 1;
		rsrc_data->en_cfg = (0x1 << 16) | 0x1;

	} else if (sfe_out_res_id == CAM_SFE_BUS_SFE_OUT_LCR) {

		switch (rsrc_data->format) {
		case CAM_FORMAT_PLAIN16_10:
		case CAM_FORMAT_PLAIN16_12:
		case CAM_FORMAT_PLAIN16_14:
		case CAM_FORMAT_PLAIN16_16:
			rsrc_data->stride = ALIGNUP(rsrc_data->width * 2, 8);
			rsrc_data->en_cfg = 0x1;
			/* LSB aligned */
			rsrc_data->pack_fmt |= 0x20;
			break;
		default:
			CAM_ERR(CAM_SFE, "Invalid format %d out_type:%d",
				rsrc_data->format, sfe_out_res_id);
			return -EINVAL;
		}

	} else {
		CAM_ERR(CAM_SFE, "Invalid out_type:%d requested",
			sfe_out_res_id);
		return -EINVAL;
	}

	*comp_grp_id = rsrc_data->hw_regs->comp_group;
	*comp_done_mask |= (1 << sfe_out_res_id);

	switch (rsrc_data->en_cfg) {
	case 0x1:
		strlcpy(wm_mode, "line-based", sizeof(wm_mode));
		break;
	case ((0x1 << 16) | 0x1):
		strlcpy(wm_mode, "frame-based", sizeof(wm_mode));
		break;
	case ((0x2 << 16) | 0x1):
		strlcpy(wm_mode, "index-based", sizeof(wm_mode));
		break;
	}

	CAM_DBG(CAM_SFE,
		"SFE:%d WM:%d processed width:%d height:%d format:0x%X pack_fmt 0x%x %s",
		rsrc_data->common_data->core_index, rsrc_data->index,
		rsrc_data->width, rsrc_data->height, rsrc_data->format,
		rsrc_data->pack_fmt, wm_mode);
	return 0;
}

static int cam_sfe_bus_release_wm(void   *bus_priv,
	struct cam_isp_resource_node     *wm_res)
{
	struct cam_sfe_bus_wr_wm_resource_data   *rsrc_data =
		wm_res->res_priv;

	rsrc_data->offset = 0;
	rsrc_data->width = 0;
	rsrc_data->height = 0;
	rsrc_data->stride = 0;
	rsrc_data->format = 0;
	rsrc_data->pack_fmt = 0;
	rsrc_data->irq_subsample_period = 0;
	rsrc_data->irq_subsample_pattern = 0;
	rsrc_data->framedrop_period = 0;
	rsrc_data->framedrop_pattern = 0;
	rsrc_data->packer_cfg = 0;
	rsrc_data->h_init = 0;
	rsrc_data->init_cfg_done = false;
	rsrc_data->hfr_cfg_done = false;
	rsrc_data->en_cfg = 0;
	rsrc_data->is_dual = 0;

	wm_res->tasklet_info = NULL;
	wm_res->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;

	CAM_DBG(CAM_SFE, "SFE:%d Release WM:%d",
		rsrc_data->common_data->core_index, rsrc_data->index);

	return 0;
}

static int cam_sfe_bus_start_wm(struct cam_isp_resource_node *wm_res)
{
	const uint32_t image_cfg_height_shift_val = 16;
	struct cam_sfe_bus_wr_wm_resource_data   *rsrc_data =
		wm_res->res_priv;
	struct cam_sfe_bus_wr_common_data        *common_data =
		rsrc_data->common_data;

	cam_io_w((rsrc_data->height << image_cfg_height_shift_val)
		| rsrc_data->width, common_data->mem_base +
		rsrc_data->hw_regs->image_cfg_0);
	cam_io_w(rsrc_data->pack_fmt,
		common_data->mem_base + rsrc_data->hw_regs->packer_cfg);

	/* Enable WM */
	cam_io_w_mb(rsrc_data->en_cfg, common_data->mem_base +
		rsrc_data->hw_regs->cfg);

	CAM_DBG(CAM_SFE,
		"Start SFE:%d WM:%d offset:0x%X en_cfg:0x%X width:%d height:%d",
		rsrc_data->common_data->core_index, rsrc_data->index,
		(uint32_t) rsrc_data->hw_regs->cfg, rsrc_data->en_cfg,
		rsrc_data->width, rsrc_data->height);
	CAM_DBG(CAM_SFE, "WM:%d pk_fmt:%d stride:%d",
		rsrc_data->index, rsrc_data->pack_fmt & PACKER_FMT_MAX,
		rsrc_data->stride);

	wm_res->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;

	return 0;
}

static int cam_sfe_bus_stop_wm(struct cam_isp_resource_node *wm_res)
{
	struct cam_sfe_bus_wr_wm_resource_data   *rsrc_data =
		wm_res->res_priv;
	struct cam_sfe_bus_wr_common_data        *common_data =
		rsrc_data->common_data;

	/* Disable WM */
	cam_io_w_mb(0x0, common_data->mem_base + rsrc_data->hw_regs->cfg);
	CAM_DBG(CAM_SFE, "Stop SFE:%d WM:%d",
		rsrc_data->common_data->core_index, rsrc_data->index);

	wm_res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;
	rsrc_data->init_cfg_done = false;
	rsrc_data->hfr_cfg_done = false;

	return 0;
}

static int cam_sfe_bus_handle_wm_done_top_half(uint32_t evt_id,
	struct cam_irq_th_payload *th_payload)
{
	return -EPERM;
}

static int cam_sfe_bus_handle_wm_done_bottom_half(void *wm_node,
	void *evt_payload_priv)
{
	return -EPERM;
}

static int cam_sfe_bus_init_wm_resource(uint32_t index,
	struct cam_sfe_bus_wr_priv      *bus_priv,
	struct cam_sfe_bus_wr_hw_info   *hw_info,
	struct cam_isp_resource_node    *wm_res)
{
	struct cam_sfe_bus_wr_wm_resource_data *rsrc_data;

	rsrc_data = kzalloc(sizeof(struct cam_sfe_bus_wr_wm_resource_data),
		GFP_KERNEL);
	if (!rsrc_data) {
		CAM_DBG(CAM_SFE, "Failed to alloc for WM res priv");
		return -ENOMEM;
	}
	wm_res->res_priv = rsrc_data;

	rsrc_data->index = index;
	rsrc_data->hw_regs = &hw_info->bus_client_reg[index];
	rsrc_data->common_data = &bus_priv->common_data;

	wm_res->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;
	INIT_LIST_HEAD(&wm_res->list);

	wm_res->start = cam_sfe_bus_start_wm;
	wm_res->stop = cam_sfe_bus_stop_wm;
	wm_res->top_half_handler = cam_sfe_bus_handle_wm_done_top_half;
	wm_res->bottom_half_handler =
		cam_sfe_bus_handle_wm_done_bottom_half;
	wm_res->hw_intf = bus_priv->common_data.hw_intf;

	return 0;
}

static int cam_sfe_bus_deinit_wm_resource(
	struct cam_isp_resource_node    *wm_res)
{
	struct cam_sfe_bus_wr_wm_resource_data *rsrc_data;

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

static void cam_sfe_bus_add_wm_to_comp_grp(
	struct cam_isp_resource_node    *comp_grp,
	uint32_t                         composite_mask)
{
	struct cam_sfe_bus_wr_comp_grp_data  *rsrc_data = comp_grp->res_priv;

	rsrc_data->composite_mask |= composite_mask;
}

static bool cam_sfe_bus_match_comp_grp(
	struct cam_sfe_bus_wr_priv           *bus_priv,
	struct cam_isp_resource_node        **comp_grp,
	uint32_t                              comp_grp_id)
{
	struct cam_sfe_bus_wr_comp_grp_data  *rsrc_data = NULL;
	struct cam_isp_resource_node         *comp_grp_local = NULL;

	list_for_each_entry(comp_grp_local,
		&bus_priv->used_comp_grp, list) {
		rsrc_data = comp_grp_local->res_priv;
		if (rsrc_data->comp_grp_type == comp_grp_id) {
			/* Match found */
			*comp_grp = comp_grp_local;
			return true;
		}
	}

	list_for_each_entry(comp_grp_local,
		&bus_priv->free_comp_grp, list) {
		rsrc_data = comp_grp_local->res_priv;
		if (rsrc_data->comp_grp_type == comp_grp_id) {
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

static int cam_sfe_bus_acquire_comp_grp(
	struct cam_sfe_bus_wr_priv           *bus_priv,
	struct cam_isp_out_port_generic_info *out_port_info,
	void                                 *tasklet,
	uint32_t                              is_dual,
	uint32_t                              is_master,
	struct cam_isp_resource_node        **comp_grp,
	enum cam_sfe_bus_wr_comp_grp_type     comp_grp_id)
{
	int rc = 0;
	struct cam_isp_resource_node         *comp_grp_local = NULL;
	struct cam_sfe_bus_wr_comp_grp_data  *rsrc_data = NULL;
	bool previously_acquired = false;

	/* Check if matching comp_grp has already been acquired */
	previously_acquired = cam_sfe_bus_match_comp_grp(
		bus_priv, &comp_grp_local, comp_grp_id);

	if (!comp_grp_local) {
		CAM_ERR(CAM_SFE, "Invalid comp_grp:%d", comp_grp_id);
		return -ENODEV;
	}

	rsrc_data = comp_grp_local->res_priv;

	if (!previously_acquired) {
		rsrc_data->intra_client_mask = 0x1;
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
			CAM_ERR(CAM_SFE, "Invalid State %d comp_grp:%u",
				comp_grp_local->res_state,
				rsrc_data->comp_grp_type);
			return -EBUSY;
		}
	}

	CAM_DBG(CAM_SFE, "Acquire SFE:%d comp_grp:%u",
		rsrc_data->common_data->core_index, rsrc_data->comp_grp_type);

	rsrc_data->acquire_dev_cnt++;
	*comp_grp = comp_grp_local;

	return rc;
}

static int cam_sfe_bus_release_comp_grp(
	struct cam_sfe_bus_wr_priv         *bus_priv,
	struct cam_isp_resource_node       *in_comp_grp)
{
	struct cam_isp_resource_node         *comp_grp = NULL;
	struct cam_sfe_bus_wr_comp_grp_data  *in_rsrc_data = NULL;
	int match_found = 0;

	if (!in_comp_grp) {
		CAM_ERR(CAM_SFE, "Invalid Params comp_grp %pK", in_comp_grp);
		return -EINVAL;
	}

	if (in_comp_grp->res_state == CAM_ISP_RESOURCE_STATE_AVAILABLE) {
		CAM_ERR(CAM_SFE, "Already released comp_grp");
		return 0;
	}

	if (in_comp_grp->res_state == CAM_ISP_RESOURCE_STATE_STREAMING) {
		CAM_ERR(CAM_SFE, "Invalid State %d",
			in_comp_grp->res_state);
		return -EBUSY;
	}

	in_rsrc_data = in_comp_grp->res_priv;
	CAM_DBG(CAM_SFE, "Release SFE:%d comp_grp:%u",
		bus_priv->common_data.core_index,
		in_rsrc_data->comp_grp_type);

	list_for_each_entry(comp_grp, &bus_priv->used_comp_grp, list) {
		if (comp_grp == in_comp_grp) {
			match_found = 1;
			break;
		}
	}

	if (!match_found) {
		CAM_ERR(CAM_SFE, "Could not find comp_grp:%u",
			in_rsrc_data->comp_grp_type);
		return -ENODEV;
	}

	in_rsrc_data->acquire_dev_cnt--;
	if (in_rsrc_data->acquire_dev_cnt == 0) {
		list_del(&comp_grp->list);

		in_rsrc_data->dual_slave_core = CAM_SFE_BUS_SFE_CORE_MAX;
		in_rsrc_data->addr_sync_mode = 0;
		in_rsrc_data->composite_mask = 0;

		comp_grp->tasklet_info = NULL;
		comp_grp->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;

		list_add_tail(&comp_grp->list, &bus_priv->free_comp_grp);
	}

	return 0;
}

static int cam_sfe_bus_start_comp_grp(
	struct cam_isp_resource_node *comp_grp,
	uint32_t *bus_irq_reg_mask)
{
	int rc = 0;
	struct cam_sfe_bus_wr_comp_grp_data *rsrc_data = NULL;
	struct cam_sfe_bus_wr_common_data   *common_data = NULL;

	rsrc_data = comp_grp->res_priv;
	common_data = rsrc_data->common_data;

	CAM_DBG(CAM_SFE,
		"Start SFE:%d comp_grp:%d streaming state:%d comp_mask:0x%X",
		rsrc_data->common_data->core_index,
		rsrc_data->comp_grp_type, comp_grp->res_state,
		rsrc_data->composite_mask);

	if (comp_grp->res_state == CAM_ISP_RESOURCE_STATE_STREAMING)
		return 0;

	bus_irq_reg_mask[CAM_SFE_IRQ_BUS_REG_STATUS0] =
		(0x1 << (rsrc_data->comp_grp_type +
		rsrc_data->common_data->comp_done_shift));

	CAM_DBG(CAM_SFE, "Start Done SFE:%d comp_grp:%d",
		rsrc_data->common_data->core_index,
		rsrc_data->comp_grp_type,
		bus_irq_reg_mask[0]);

	comp_grp->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;

	return rc;
}

static inline int cam_sfe_bus_stop_comp_grp(
	struct cam_isp_resource_node          *comp_grp)
{
	comp_grp->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;

	return 0;
}

static int cam_sfe_bus_wr_init_comp_grp(uint32_t index,
	struct cam_hw_soc_info        *soc_info,
	struct cam_sfe_bus_wr_priv    *bus_priv,
	struct cam_sfe_bus_wr_hw_info *hw_info,
	struct cam_isp_resource_node  *comp_grp)
{
	struct cam_sfe_bus_wr_comp_grp_data *rsrc_data = NULL;

	rsrc_data = kzalloc(sizeof(struct cam_sfe_bus_wr_comp_grp_data),
		GFP_KERNEL);
	if (!rsrc_data)
		return -ENOMEM;

	comp_grp->res_priv = rsrc_data;

	comp_grp->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;
	INIT_LIST_HEAD(&comp_grp->list);

	rsrc_data->comp_grp_type   = index;
	rsrc_data->common_data     = &bus_priv->common_data;
	rsrc_data->dual_slave_core = CAM_SFE_BUS_SFE_CORE_MAX;

	list_add_tail(&comp_grp->list, &bus_priv->free_comp_grp);

	/* TO DO set top half */
	comp_grp->top_half_handler = NULL;
	comp_grp->hw_intf = bus_priv->common_data.hw_intf;

	return 0;
}

static int cam_sfe_bus_deinit_comp_grp(
	struct cam_isp_resource_node    *comp_grp)
{
	struct cam_sfe_bus_wr_comp_grp_data *rsrc_data =
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
		CAM_ERR(CAM_SFE, "comp_grp_priv is NULL");
		return -ENODEV;
	}
	kfree(rsrc_data);

	return 0;
}

static int cam_sfe_bus_wr_get_secure_mode(void *priv, void *cmd_args,
	uint32_t arg_size)
{
	struct cam_isp_hw_get_cmd_update  *secure_mode = cmd_args;
	struct cam_sfe_bus_wr_out_data    *rsrc_data;
	uint32_t                          *mode;

	rsrc_data = (struct cam_sfe_bus_wr_out_data *)
		secure_mode->res->res_priv;
	mode = (uint32_t *)secure_mode->data;
	*mode = (rsrc_data->secure_mode == CAM_SECURE_MODE_SECURE) ?
		true : false;

	return 0;
}

static int cam_sfe_bus_acquire_sfe_out(void *priv, void *acquire_args,
	uint32_t args_size)
{
	int                                     rc = -ENODEV;
	int                                     i;
	enum cam_sfe_bus_sfe_out_type           sfe_out_res_id;
	uint32_t                                format;
	struct cam_sfe_bus_wr_priv             *bus_priv = priv;
	struct cam_sfe_acquire_args            *acq_args = acquire_args;
	struct cam_sfe_hw_sfe_out_acquire_args *out_acquire_args;
	struct cam_isp_resource_node           *rsrc_node = NULL;
	struct cam_sfe_bus_wr_out_data         *rsrc_data = NULL;
	uint32_t                                secure_caps = 0, mode;
	enum cam_sfe_bus_wr_comp_grp_type       comp_grp_id;
	uint32_t                                client_done_mask = 0;

	if (!bus_priv || !acquire_args) {
		CAM_ERR(CAM_SFE, "Invalid Param");
		return -EINVAL;
	}

	comp_grp_id = CAM_SFE_BUS_WR_COMP_GRP_MAX;
	out_acquire_args = &acq_args->sfe_out;
	format = out_acquire_args->out_port_info->format;

	CAM_DBG(CAM_SFE, "SFE:%d Acquire out_type:0x%X",
		bus_priv->common_data.core_index,
		out_acquire_args->out_port_info->res_type);

	sfe_out_res_id = cam_sfe_bus_wr_get_out_res_id(
		out_acquire_args->out_port_info->res_type);
	if (sfe_out_res_id == CAM_SFE_BUS_SFE_OUT_MAX)
		return -ENODEV;

	rsrc_node = &bus_priv->sfe_out[sfe_out_res_id];
	if (rsrc_node->res_state != CAM_ISP_RESOURCE_STATE_AVAILABLE) {
		CAM_ERR(CAM_SFE,
			"SFE:%d out_type:0x%X resource not available state:%d",
			bus_priv->common_data.core_index,
			sfe_out_res_id, rsrc_node->res_state);
		return -EBUSY;
	}

	if (!acq_args->buf_done_controller) {
		CAM_ERR(CAM_SFE, "Invalid buf done controller");
		return -EINVAL;
	}

	rsrc_data = rsrc_node->res_priv;
	rsrc_data->common_data->buf_done_controller =
		acq_args->buf_done_controller;
	rsrc_data->common_data->event_cb = acq_args->event_cb;
	rsrc_data->priv = acq_args->priv;
	secure_caps = cam_sfe_bus_can_be_secure(
		rsrc_data->out_type);
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
				CAM_ERR_RATE_LIMIT(CAM_SFE,
					"Mismatch: Acquire mode[%d], drvr mode[%d]",
					rsrc_data->common_data->secure_mode,
					mode);
				mutex_unlock(
					&rsrc_data->common_data->bus_mutex);
				return rc;
			}
		}
		rsrc_data->common_data->num_sec_out++;
	}
	mutex_unlock(&rsrc_data->common_data->bus_mutex);

	//bus_priv->tasklet_info = acq_args->tasklet;
	rsrc_node->rdi_only_ctx = 0;
	rsrc_node->res_id = out_acquire_args->out_port_info->res_type;
	rsrc_node->tasklet_info = acq_args->tasklet;
	rsrc_node->cdm_ops = out_acquire_args->cdm_ops;
	rsrc_data->cdm_util_ops = out_acquire_args->cdm_ops;
	rsrc_data->format = out_acquire_args->out_port_info->format;

	/* Acquire WM and retrieve COMP GRP ID */
	for (i = 0; i < rsrc_data->num_wm; i++) {
		rc = cam_sfe_bus_acquire_wm(bus_priv,
			out_acquire_args->out_port_info,
			acq_args->tasklet,
			sfe_out_res_id,
			i,
			&rsrc_data->wm_res[i],
			&client_done_mask,
			out_acquire_args->is_dual,
			&comp_grp_id);
		if (rc) {
			CAM_ERR(CAM_SFE,
				"Failed to acquire WM SFE:%d out_type:%d rc:%d",
				rsrc_data->common_data->core_index,
				sfe_out_res_id, rc);
			goto release_wm;
		}
	}

	/* Acquire composite group using COMP GRP ID */
	rc = cam_sfe_bus_acquire_comp_grp(bus_priv,
		out_acquire_args->out_port_info,
		acq_args->tasklet,
		out_acquire_args->is_dual,
		out_acquire_args->is_master,
		&rsrc_data->comp_grp,
		comp_grp_id);
	if (rc) {
		CAM_ERR(CAM_SFE,
			"Failed to acquire comp_grp SFE:%d out_type:%d rc:%d",
			rsrc_data->common_data->core_index,
			sfe_out_res_id, rc);
		return rc;
	}

	rsrc_data->is_dual = out_acquire_args->is_dual;
	rsrc_data->is_master = out_acquire_args->is_master;

	cam_sfe_bus_add_wm_to_comp_grp(rsrc_data->comp_grp,
		client_done_mask);

	rsrc_node->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;
	out_acquire_args->rsrc_node = rsrc_node;

	CAM_DBG(CAM_SFE, "Acquire successful");
	return rc;

release_wm:
	for (i--; i >= 0; i--)
		cam_sfe_bus_release_wm(bus_priv,
			&rsrc_data->wm_res[i]);

	cam_sfe_bus_release_comp_grp(bus_priv, rsrc_data->comp_grp);

	return rc;
}

static int cam_sfe_bus_release_sfe_out(void *bus_priv, void *release_args,
	uint32_t args_size)
{
	uint32_t i;
	struct cam_isp_resource_node         *sfe_out = NULL;
	struct cam_sfe_bus_wr_out_data       *rsrc_data = NULL;
	uint32_t                              secure_caps = 0;

	if (!bus_priv || !release_args) {
		CAM_ERR(CAM_SFE, "Invalid input bus_priv %pK release_args %pK",
			bus_priv, release_args);
		return -EINVAL;
	}

	sfe_out = release_args;
	rsrc_data = sfe_out->res_priv;

	if (sfe_out->res_state != CAM_ISP_RESOURCE_STATE_RESERVED) {
		CAM_ERR(CAM_SFE,
			"Invalid resource state:%d SFE:%d out_type:0x%X",
			sfe_out->res_state, rsrc_data->common_data->core_index,
			sfe_out->res_id);
	}

	for (i = 0; i < rsrc_data->num_wm; i++)
		cam_sfe_bus_release_wm(bus_priv, &rsrc_data->wm_res[i]);
	rsrc_data->num_wm = 0;

	if (rsrc_data->comp_grp)
		cam_sfe_bus_release_comp_grp(bus_priv,
			rsrc_data->comp_grp);
	rsrc_data->comp_grp = NULL;

	sfe_out->tasklet_info = NULL;
	sfe_out->cdm_ops = NULL;
	rsrc_data->cdm_util_ops = NULL;

	secure_caps = cam_sfe_bus_can_be_secure(rsrc_data->out_type);
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
			CAM_ERR(CAM_SFE, "driver[%d],resource[%d] mismatch",
				rsrc_data->common_data->secure_mode,
				rsrc_data->secure_mode);
		}

		if (!rsrc_data->common_data->num_sec_out)
			rsrc_data->common_data->secure_mode =
				CAM_SECURE_MODE_NON_SECURE;
	}
	mutex_unlock(&rsrc_data->common_data->bus_mutex);

	if (sfe_out->res_state == CAM_ISP_RESOURCE_STATE_RESERVED)
		sfe_out->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;

	return 0;
}

static int cam_sfe_bus_wr_get_evt_payload(
	struct cam_sfe_bus_wr_common_data       *common_data,
	struct cam_sfe_bus_wr_irq_evt_payload  **evt_payload)
{
	int rc;

	spin_lock(&common_data->spin_lock);

	if (!common_data->hw_init) {
		CAM_ERR_RATE_LIMIT(CAM_SFE, "SFE:%d Bus uninitialized",
			common_data->core_index);
		*evt_payload = NULL;
		rc = -EPERM;
		goto done;
	}

	if (list_empty(&common_data->free_payload_list)) {
		CAM_ERR_RATE_LIMIT(CAM_SFE, "No free BUS event payload");
		*evt_payload = NULL;
		rc = -ENODEV;
		goto done;
	}

	*evt_payload = list_first_entry(&common_data->free_payload_list,
		struct cam_sfe_bus_wr_irq_evt_payload, list);
	list_del_init(&(*evt_payload)->list);
	rc = 0;
done:
	spin_unlock(&common_data->spin_lock);
	return rc;
}

static int cam_sfe_bus_wr_put_evt_payload(
	struct cam_sfe_bus_wr_common_data      *common_data,
	struct cam_sfe_bus_wr_irq_evt_payload **evt_payload)
{
	unsigned long flags;

	if (!common_data) {
		CAM_ERR(CAM_SFE, "Invalid param common_data NULL");
		return -EINVAL;
	}

	if (*evt_payload == NULL) {
		CAM_ERR(CAM_SFE, "No payload to put");
		return -EINVAL;
	}

	spin_lock_irqsave(&common_data->spin_lock, flags);
	if (common_data->hw_init)
		list_add_tail(&(*evt_payload)->list,
			&common_data->free_payload_list);
	spin_unlock_irqrestore(&common_data->spin_lock, flags);

	*evt_payload = NULL;

	CAM_DBG(CAM_SFE, "Done");
	return 0;
}

static int cam_sfe_bus_start_sfe_out(
	struct cam_isp_resource_node          *sfe_out)
{
	int rc = 0, i;
	struct cam_sfe_bus_wr_out_data      *rsrc_data = NULL;
	struct cam_sfe_bus_wr_common_data   *common_data = NULL;
	uint32_t bus_irq_reg_mask[CAM_SFE_IRQ_REGISTERS_MAX];
	uint32_t source_group = 0;

	if (!sfe_out) {
		CAM_ERR(CAM_SFE, "Invalid input");
		return -EINVAL;
	}

	rsrc_data = sfe_out->res_priv;
	common_data = rsrc_data->common_data;
	source_group = rsrc_data->source_group;

	if (sfe_out->res_state != CAM_ISP_RESOURCE_STATE_RESERVED) {
		CAM_ERR(CAM_SFE,
			"Invalid resource state:%d SFE:%d out_type:0x%X",
			sfe_out->res_state, rsrc_data->common_data->core_index,
			rsrc_data->out_type);
		return -EACCES;
	}

	CAM_DBG(CAM_SFE, "Start SFE:%d out_type:0x%X",
		rsrc_data->common_data->core_index, rsrc_data->out_type);

	for (i = 0; i < rsrc_data->num_wm; i++)
		rc = cam_sfe_bus_start_wm(&rsrc_data->wm_res[i]);

	memset(bus_irq_reg_mask, 0, sizeof(bus_irq_reg_mask));
	rc = cam_sfe_bus_start_comp_grp(rsrc_data->comp_grp,
		bus_irq_reg_mask);

	if (rsrc_data->is_dual && !rsrc_data->is_master)
		goto end;

	sfe_out->irq_handle =  cam_irq_controller_subscribe_irq(
		common_data->buf_done_controller,
		CAM_IRQ_PRIORITY_1,
		bus_irq_reg_mask,
		sfe_out,
		sfe_out->top_half_handler,
		sfe_out->bottom_half_handler,
		sfe_out->tasklet_info,
		&tasklet_bh_api);
	if (sfe_out->irq_handle < 1) {
		CAM_ERR(CAM_SFE, "Subscribe IRQ failed for sfe out_res: %d",
			sfe_out->res_id);
		sfe_out->irq_handle = 0;
		return -EFAULT;
	}
end:
	sfe_out->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;
	return rc;
}

static int cam_sfe_bus_stop_sfe_out(
	struct cam_isp_resource_node          *sfe_out)
{
	int rc = 0, i;
	struct cam_sfe_bus_wr_out_data      *rsrc_data = NULL;
	struct cam_sfe_bus_wr_common_data   *common_data = NULL;

	if (!sfe_out) {
		CAM_ERR(CAM_SFE, "Invalid input");
		return -EINVAL;
	}

	rsrc_data = sfe_out->res_priv;
	common_data = rsrc_data->common_data;

	if (sfe_out->res_state == CAM_ISP_RESOURCE_STATE_AVAILABLE ||
		sfe_out->res_state == CAM_ISP_RESOURCE_STATE_RESERVED) {
		CAM_DBG(CAM_SFE, "Stop SFE:%d out_type:0x%X state:%d",
			rsrc_data->common_data->core_index, rsrc_data->out_type,
			sfe_out->res_state);
		return rc;
	}

	rc = cam_sfe_bus_stop_comp_grp(rsrc_data->comp_grp);

	for (i = 0; i < rsrc_data->num_wm; i++)
		rc = cam_sfe_bus_stop_wm(&rsrc_data->wm_res[i]);

	/* TO DO any IRQ handling */
	if (sfe_out->irq_handle) {
		cam_irq_controller_unsubscribe_irq(
			common_data->buf_done_controller,
			sfe_out->irq_handle);
		sfe_out->irq_handle = 0;
	}

	sfe_out->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;
	return rc;
}

static int cam_sfe_bus_handle_sfe_out_done_top_half(
	uint32_t evt_id,
	struct cam_irq_th_payload *th_payload)
{
	int32_t                                     rc;
	int                                         i;
	struct cam_isp_resource_node               *sfe_out = NULL;
	struct cam_sfe_bus_wr_out_data             *rsrc_data = NULL;
	struct cam_sfe_bus_wr_irq_evt_payload      *evt_payload;
	struct cam_sfe_bus_wr_comp_grp_data        *resource_data;
	uint32_t                                    status_0;

	sfe_out = th_payload->handler_priv;
	if (!sfe_out) {
		CAM_ERR_RATE_LIMIT(CAM_SFE, "No resource");
		return -ENODEV;
	}

	rsrc_data = sfe_out->res_priv;
	resource_data = rsrc_data->comp_grp->res_priv;

	CAM_DBG(CAM_SFE, "SFE:%d Bus IRQ status_0: 0x%X status_1: 0x%X",
		rsrc_data->common_data->core_index,
		th_payload->evt_status_arr[0],
		th_payload->evt_status_arr[1]);

	rc  = cam_sfe_bus_wr_get_evt_payload(rsrc_data->common_data,
			&evt_payload);
	if (rc) {
		CAM_INFO_RATE_LIMIT(CAM_SFE,
			"SFE:%d Bus IRQ status_0: 0x%X status_1: 0x%X",
			rsrc_data->common_data->core_index,
			th_payload->evt_status_arr[0],
			th_payload->evt_status_arr[1]);
		return rc;
	}

	cam_isp_hw_get_timestamp(&evt_payload->ts);

	evt_payload->core_index = rsrc_data->common_data->core_index;
	evt_payload->evt_id = evt_id;

	for (i = 0; i < th_payload->num_registers; i++)
		evt_payload->irq_reg_val[i] = th_payload->evt_status_arr[i];

	th_payload->evt_payload_priv = evt_payload;

	status_0 = th_payload->evt_status_arr[CAM_SFE_IRQ_BUS_REG_STATUS0];

	if (status_0 & BIT(resource_data->comp_grp_type +
		rsrc_data->common_data->comp_done_shift)) {
		trace_cam_log_event("bufdone", "bufdone_IRQ",
			status_0, resource_data->comp_grp_type);
	}

	return rc;
}

static int cam_sfe_bus_handle_comp_done_bottom_half(
	void                *handler_priv,
	void                *evt_payload_priv,
	uint32_t            *comp_mask)
{
	int rc = CAM_SFE_IRQ_STATUS_ERR;
	struct cam_isp_resource_node          *comp_grp = handler_priv;
	struct cam_sfe_bus_wr_irq_evt_payload *evt_payload = evt_payload_priv;
	struct cam_sfe_bus_wr_comp_grp_data   *rsrc_data = comp_grp->res_priv;
	uint32_t                              *cam_ife_irq_regs;
	uint32_t                               status_0;

	if (!evt_payload)
		return rc;

	if (rsrc_data->is_dual && (!rsrc_data->is_master)) {
		CAM_ERR(CAM_SFE, "Invalid comp_grp:%u is_master:%u",
			rsrc_data->comp_grp_type, rsrc_data->is_master);
		return rc;
	}

	cam_ife_irq_regs = evt_payload->irq_reg_val;
	status_0 = cam_ife_irq_regs[CAM_SFE_IRQ_BUS_REG_STATUS0];

	if (status_0 & BIT(rsrc_data->comp_grp_type +
		rsrc_data->common_data->comp_done_shift)) {
		evt_payload->evt_id = CAM_ISP_HW_EVENT_DONE;
		rc = CAM_SFE_IRQ_STATUS_SUCCESS;
	}

	CAM_DBG(CAM_SFE, "SFE:%d comp_grp:%d Bus IRQ status_0: 0x%X rc:%d",
		rsrc_data->common_data->core_index, rsrc_data->comp_grp_type,
		status_0, rc);

	*comp_mask = rsrc_data->composite_mask;

	return rc;
}

static int cam_sfe_bus_handle_sfe_out_done_bottom_half(
	void                *handler_priv,
	void                *evt_payload_priv)
{
	int                                    rc = -EINVAL, num_out = 0, i = 0;
	struct cam_isp_resource_node          *sfe_out = handler_priv;
	struct cam_sfe_bus_wr_out_data        *rsrc_data = sfe_out->res_priv;
	struct cam_sfe_bus_wr_irq_evt_payload *evt_payload = evt_payload_priv;
	struct cam_isp_hw_event_info           evt_info;
	void                                  *ctx = NULL;
	uint32_t                               evt_id = 0, comp_mask = 0;
	uint32_t                       out_list[CAM_SFE_BUS_SFE_OUT_MAX];

	rc = cam_sfe_bus_handle_comp_done_bottom_half(
		rsrc_data->comp_grp, evt_payload_priv, &comp_mask);
	CAM_DBG(CAM_SFE, "SFE:%d out_type:0x%X rc:%d",
		rsrc_data->common_data->core_index, rsrc_data->out_type,
		rsrc_data->out_type, rc);

	ctx = rsrc_data->priv;
	memset(out_list, 0, sizeof(out_list));

	switch (rc) {
	case CAM_SFE_IRQ_STATUS_SUCCESS:
		evt_id = evt_payload->evt_id;

		evt_info.res_type = sfe_out->res_type;
		evt_info.hw_idx   = sfe_out->hw_intf->hw_idx;

		rc = cam_sfe_bus_get_comp_sfe_out_res_id_list(
			comp_mask, out_list, &num_out);
		for (i = 0; i < num_out; i++) {
			evt_info.res_id = out_list[i];
			if (rsrc_data->common_data->event_cb)
				rsrc_data->common_data->event_cb(ctx, evt_id,
					(void *)&evt_info);
		}
		break;
	default:
		break;
	}

	cam_sfe_bus_wr_put_evt_payload(rsrc_data->common_data, &evt_payload);

	return rc;
}

static int cam_sfe_bus_init_sfe_out_resource(
	uint32_t                              index,
	struct cam_sfe_bus_wr_priv           *bus_priv,
	struct cam_sfe_bus_wr_hw_info        *hw_info)
{
	struct cam_isp_resource_node         *sfe_out = NULL;
	struct cam_sfe_bus_wr_out_data       *rsrc_data = NULL;
	int rc = 0, i = 0;
	int32_t sfe_out_type =
		hw_info->sfe_out_hw_info[index].sfe_out_type;

	if (sfe_out_type < 0 ||
		sfe_out_type >= CAM_SFE_BUS_SFE_OUT_MAX) {
		CAM_ERR(CAM_SFE, "Init SFE Out failed, Invalid type=%d",
			sfe_out_type);
		return -EINVAL;
	}

	sfe_out = &bus_priv->sfe_out[sfe_out_type];
	if (sfe_out->res_state != CAM_ISP_RESOURCE_STATE_UNAVAILABLE ||
		sfe_out->res_priv) {
		CAM_ERR(CAM_SFE, "sfe_out_type %d has already been initialized",
			sfe_out_type);
		return -EFAULT;
	}

	rsrc_data = kzalloc(sizeof(struct cam_sfe_bus_wr_out_data),
		GFP_KERNEL);
	if (!rsrc_data) {
		rc = -ENOMEM;
		return rc;
	}

	sfe_out->res_priv = rsrc_data;

	sfe_out->res_type = CAM_ISP_RESOURCE_SFE_OUT;
	sfe_out->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;
	INIT_LIST_HEAD(&sfe_out->list);

	rsrc_data->source_group =
		hw_info->sfe_out_hw_info[index].source_group;
	rsrc_data->out_type     =
		hw_info->sfe_out_hw_info[index].sfe_out_type;
	rsrc_data->common_data  = &bus_priv->common_data;
	rsrc_data->max_width    =
		hw_info->sfe_out_hw_info[index].max_width;
	rsrc_data->max_height   =
		hw_info->sfe_out_hw_info[index].max_height;
	rsrc_data->secure_mode  = CAM_SECURE_MODE_NON_SECURE;
	rsrc_data->num_wm       = hw_info->sfe_out_hw_info[index].num_wm;

	rsrc_data->wm_res = kzalloc((sizeof(struct cam_isp_resource_node) *
		rsrc_data->num_wm), GFP_KERNEL);
	if (!rsrc_data->wm_res) {
		CAM_ERR(CAM_SFE, "Failed to alloc for wm_res");
		return -ENOMEM;
	}

	rc = cam_sfe_bus_init_wm_resource(
			hw_info->sfe_out_hw_info[index].wm_idx,
			bus_priv, hw_info,
			&rsrc_data->wm_res[i]);
	if (rc < 0) {
		CAM_ERR(CAM_SFE, "SFE:%d init WM:%d failed rc:%d",
			bus_priv->common_data.core_index, i, rc);
		return rc;
	}

	sfe_out->start = cam_sfe_bus_start_sfe_out;
	sfe_out->stop = cam_sfe_bus_stop_sfe_out;
	sfe_out->top_half_handler =
		cam_sfe_bus_handle_sfe_out_done_top_half;
	sfe_out->bottom_half_handler =
		cam_sfe_bus_handle_sfe_out_done_bottom_half;
	sfe_out->process_cmd = cam_sfe_bus_wr_process_cmd;
	sfe_out->hw_intf = bus_priv->common_data.hw_intf;
	sfe_out->irq_handle = 0;

	return 0;
}

static int cam_sfe_bus_deinit_sfe_out_resource(
	struct cam_isp_resource_node    *sfe_out)
{
	struct cam_sfe_bus_wr_out_data *rsrc_data = sfe_out->res_priv;
	int rc = 0, i = 0;

	if (sfe_out->res_state == CAM_ISP_RESOURCE_STATE_UNAVAILABLE) {
		/*
		 * This is not error. It can happen if the resource is
		 * never supported in the HW.
		 */
		return 0;
	}

	sfe_out->start = NULL;
	sfe_out->stop = NULL;
	sfe_out->top_half_handler = NULL;
	sfe_out->bottom_half_handler = NULL;
	sfe_out->hw_intf = NULL;
	sfe_out->irq_handle = 0;

	sfe_out->res_state = CAM_ISP_RESOURCE_STATE_UNAVAILABLE;
	INIT_LIST_HEAD(&sfe_out->list);
	sfe_out->res_priv = NULL;

	if (!rsrc_data)
		return -ENOMEM;

	for (i = 0; i < rsrc_data->num_wm; i++) {
		rc = cam_sfe_bus_deinit_wm_resource(&rsrc_data->wm_res[i]);
		if (rc < 0)
			CAM_ERR(CAM_SFE,
				"SFE:%d deinit WM:%d failed rc:%d",
				rsrc_data->common_data->core_index, i, rc);
	}

	kfree(rsrc_data);

	return 0;
}

static int cam_sfe_bus_wr_print_dimensions(
	enum cam_sfe_bus_sfe_out_type        sfe_out_res_id,
	struct cam_sfe_bus_wr_priv          *bus_priv)
{
	struct cam_isp_resource_node            *rsrc_node = NULL;
	struct cam_sfe_bus_wr_out_data          *rsrc_data = NULL;
	struct cam_sfe_bus_wr_wm_resource_data  *wm_data   = NULL;
	int                                      i, wm_idx;

	rsrc_node = &bus_priv->sfe_out[sfe_out_res_id];
	rsrc_data = rsrc_node->res_priv;
	for (i = 0; i < rsrc_data->num_wm; i++) {
		wm_data = (struct cam_sfe_bus_wr_wm_resource_data  *)
			&rsrc_data->wm_res[i].res_priv;
		wm_idx = wm_data->index;
		if (wm_idx < 0 || wm_idx >= bus_priv->num_client) {
			CAM_ERR(CAM_SFE, "Unsupported SFE out %d",
				sfe_out_res_id);
			return -EINVAL;
		}

		CAM_INFO(CAM_SFE,
			"SFE:%d WM:%d width:%u height:%u stride:%u x_init:%u en_cfg:%u acquired width:%u height:%u",
			wm_data->common_data->core_index, wm_idx,
			wm_data->width,
			wm_data->height,
			wm_data->stride, wm_data->h_init,
			wm_data->en_cfg,
			wm_data->acquired_width,
			wm_data->acquired_height);
	}
	return 0;
}

static int cam_sfe_bus_wr_handle_bus_irq(uint32_t    evt_id,
	struct cam_irq_th_payload                 *th_payload)
{
	struct cam_sfe_bus_wr_priv          *bus_priv;
	int rc = 0;

	bus_priv = th_payload->handler_priv;
	rc = cam_irq_controller_handle_irq(evt_id,
		bus_priv->common_data.bus_irq_controller);
	return (rc == IRQ_HANDLED) ? 0 : -EINVAL;
}

static int cam_sfe_bus_wr_err_irq_top_half(uint32_t evt_id,
	struct cam_irq_th_payload *th_payload)
{

	int i = 0, rc = 0;
	struct cam_sfe_bus_wr_priv *bus_priv =
		th_payload->handler_priv;
	struct cam_sfe_bus_wr_irq_evt_payload *evt_payload;

	CAM_ERR_RATE_LIMIT(CAM_ISP, "SFE:%d BUS Err IRQ",
		bus_priv->common_data.core_index);

	for (i = 0; i < th_payload->num_registers; i++) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "SFE:%d BUS IRQ status_%d: 0x%X",
		bus_priv->common_data.core_index, i,
			th_payload->evt_status_arr[i]);
	}
	cam_irq_controller_disable_irq(
		bus_priv->common_data.bus_irq_controller,
		bus_priv->error_irq_handle);

	rc  = cam_sfe_bus_wr_get_evt_payload(&bus_priv->common_data,
		&evt_payload);
	if (rc)
		return rc;

	for (i = 0; i < th_payload->num_registers; i++)
		evt_payload->irq_reg_val[i] = th_payload->evt_status_arr[i];

	evt_payload->core_index = bus_priv->common_data.core_index;

	evt_payload->ccif_violation_status = cam_io_r_mb(
		bus_priv->common_data.mem_base +
		bus_priv->common_data.common_reg->ccif_violation_status);

	evt_payload->image_size_violation_status = cam_io_r_mb(
		bus_priv->common_data.mem_base +
		bus_priv->common_data.common_reg->image_size_violation_status);

	th_payload->evt_payload_priv = evt_payload;

	return rc;
}

static int cam_sfe_bus_wr_irq_bottom_half(
	void *handler_priv, void *evt_payload_priv)
{
	uint32_t status = 0;
	struct cam_sfe_bus_wr_priv            *bus_priv = handler_priv;
	struct cam_sfe_bus_wr_common_data     *common_data;
	struct cam_isp_hw_event_info           evt_info;
	struct cam_sfe_bus_wr_irq_evt_payload *evt_payload = evt_payload_priv;

	if (!handler_priv || !evt_payload_priv)
		return -EINVAL;

	common_data = &bus_priv->common_data;

	status = evt_payload->irq_reg_val[CAM_SFE_IRQ_BUS_REG_STATUS0];

	CAM_ERR(CAM_SFE,
		"SFE:%d status 0x%x Image Size violation status 0x%x CCIF violation status 0x%x",
		bus_priv->common_data.core_index, status,
		evt_payload->image_size_violation_status,
		evt_payload->ccif_violation_status);

	cam_sfe_bus_wr_put_evt_payload(common_data, &evt_payload);

	evt_info.hw_idx = common_data->core_index;
	evt_info.res_type = CAM_ISP_RESOURCE_SFE_OUT;
	evt_info.res_id = CAM_SFE_BUS_SFE_OUT_MAX;
	evt_info.err_type = CAM_SFE_IRQ_STATUS_VIOLATION;

	if (common_data->event_cb)
		common_data->event_cb(NULL, CAM_ISP_HW_EVENT_ERROR,
			(void *)&evt_info);
	return 0;
}

static int cam_sfe_bus_wr_update_wm(void *priv, void *cmd_args,
	uint32_t arg_size)
{
	struct cam_sfe_bus_wr_priv             *bus_priv;
	struct cam_isp_hw_get_cmd_update       *update_buf;
	struct cam_buf_io_cfg                  *io_cfg = NULL;
	struct cam_sfe_bus_wr_out_data         *sfe_out_data = NULL;
	struct cam_sfe_bus_wr_wm_resource_data *wm_data = NULL;
	uint32_t *reg_val_pair;
	uint32_t  i, j, k, size = 0;
	uint32_t  frame_inc = 0, val;
	uint32_t loop_size = 0, stride = 0, slice_h = 0;

	bus_priv = (struct cam_sfe_bus_wr_priv  *) priv;
	update_buf =  (struct cam_isp_hw_get_cmd_update *) cmd_args;

	sfe_out_data = (struct cam_sfe_bus_wr_out_data *)
		update_buf->res->res_priv;

	if (!sfe_out_data || !sfe_out_data->cdm_util_ops) {
		CAM_ERR(CAM_SFE, "Failed! Invalid data");
		return -EINVAL;
	}

	if (update_buf->wm_update->num_buf != sfe_out_data->num_wm) {
		CAM_ERR(CAM_SFE,
			"Failed! Invalid number buffers:%d required:%d",
			update_buf->wm_update->num_buf, sfe_out_data->num_wm);
		return -EINVAL;
	}

	reg_val_pair = &sfe_out_data->common_data->io_buf_update[0];
	if (update_buf->use_scratch_cfg)
		CAM_DBG(CAM_SFE, "Using scratch buf config");
	else
		io_cfg = update_buf->wm_update->io_cfg;

	for (i = 0, j = 0; i < sfe_out_data->num_wm; i++) {
		if (j >= (MAX_REG_VAL_PAIR_SIZE - MAX_BUF_UPDATE_REG_NUM * 2)) {
			CAM_ERR(CAM_SFE,
				"reg_val_pair %d exceeds the array limit %zu",
				j, MAX_REG_VAL_PAIR_SIZE);
			return -ENOMEM;
		}

		wm_data = (struct cam_sfe_bus_wr_wm_resource_data *)
			sfe_out_data->wm_res[i].res_priv;

		CAM_SFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
			wm_data->hw_regs->cfg, wm_data->en_cfg);
		CAM_DBG(CAM_SFE, "WM:%d en_cfg 0x%X",
			wm_data->index, reg_val_pair[j-1]);

		val = (wm_data->height << 16) | wm_data->width;
		CAM_SFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
			wm_data->hw_regs->image_cfg_0, val);
		CAM_DBG(CAM_SFE, "WM:%d image height and width 0x%X",
			wm_data->index, reg_val_pair[j-1]);

		/* For initial configuration program all bus registers */
		if (update_buf->use_scratch_cfg) {
			stride = update_buf->wm_update->stride;
			slice_h = update_buf->wm_update->slice_height;
		} else {
			stride = io_cfg->planes[i].plane_stride;
			slice_h = io_cfg->planes[i].slice_height;
		}

		val = stride;
		CAM_DBG(CAM_SFE, "before stride %d", val);
		val = ALIGNUP(val, 16);
		if (val != stride &&
			val != wm_data->stride)
			CAM_WARN(CAM_SFE, "Warning stride %u expected %u",
				stride, val);

		if (wm_data->stride != val || !wm_data->init_cfg_done) {
			CAM_SFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
				wm_data->hw_regs->image_cfg_2,
				stride);
			wm_data->stride = val;
			CAM_DBG(CAM_SFE, "WM:%d image stride 0x%X",
				wm_data->index, reg_val_pair[j-1]);
		}

		frame_inc = stride * slice_h;

		if (!(wm_data->en_cfg & (0x3 << 16))) {
			CAM_SFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
				wm_data->hw_regs->image_cfg_1, wm_data->h_init);
			CAM_DBG(CAM_SFE, "WM:%d h_init 0x%X",
				wm_data->index, reg_val_pair[j-1]);
		}

		if (wm_data->index > 7)
			loop_size = wm_data->irq_subsample_period + 1;
		else
			loop_size = 1;

		/* WM Image address */
		for (k = 0; k < loop_size; k++) {
			if (wm_data->en_cfg & (0x3 << 16))
				CAM_SFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
					wm_data->hw_regs->image_addr,
					(update_buf->wm_update->image_buf[i] +
					wm_data->offset + k * frame_inc));
			else
				CAM_SFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
					wm_data->hw_regs->image_addr,
					(update_buf->wm_update->image_buf[i] +
					k * frame_inc));

			CAM_DBG(CAM_SFE, "WM:%d image address 0x%X",
				wm_data->index, reg_val_pair[j-1]);
		}

		CAM_SFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
			wm_data->hw_regs->frame_incr, frame_inc);
		CAM_DBG(CAM_SFE, "WM:%d frame_inc %d",
			wm_data->index, reg_val_pair[j-1]);

		/* enable the WM */
		CAM_SFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
			wm_data->hw_regs->cfg,
			wm_data->en_cfg);

		/* set initial configuration done */
		if (!wm_data->init_cfg_done)
			wm_data->init_cfg_done = true;
	}

	size = sfe_out_data->cdm_util_ops->cdm_required_size_reg_random(j/2);

	/* cdm util returns dwords, need to convert to bytes */
	if ((size * 4) > update_buf->cmd.size) {
		CAM_ERR(CAM_SFE,
			"Failed! Buf size:%d insufficient, expected size:%d",
			update_buf->cmd.size, size);
		return -ENOMEM;
	}

	sfe_out_data->cdm_util_ops->cdm_write_regrandom(
		update_buf->cmd.cmd_buf_addr, j/2, reg_val_pair);

	/* cdm util returns dwords, need to convert to bytes */
	update_buf->cmd.used_bytes = size * 4;

	return 0;
}

static int cam_sfe_bus_wr_update_hfr(void *priv, void *cmd_args,
	uint32_t arg_size)
{
	struct cam_sfe_bus_wr_priv             *bus_priv;
	struct cam_isp_hw_get_cmd_update       *update_hfr;
	struct cam_sfe_bus_wr_out_data         *sfe_out_data = NULL;
	struct cam_sfe_bus_wr_wm_resource_data *wm_data = NULL;
	struct cam_isp_port_hfr_config         *hfr_cfg = NULL;
	uint32_t *reg_val_pair;
	uint32_t  i, j, size = 0;

	bus_priv = (struct cam_sfe_bus_wr_priv  *) priv;
	update_hfr =  (struct cam_isp_hw_get_cmd_update *) cmd_args;

	sfe_out_data = (struct cam_sfe_bus_wr_out_data *)
		update_hfr->res->res_priv;

	if (!sfe_out_data || !sfe_out_data->cdm_util_ops) {
		CAM_ERR(CAM_SFE, "Failed! Invalid data");
		return -EINVAL;
	}

	reg_val_pair = &sfe_out_data->common_data->io_buf_update[0];
	hfr_cfg = (struct cam_isp_port_hfr_config *)update_hfr->data;

	for (i = 0, j = 0; i < sfe_out_data->num_wm; i++) {
		if (j >= (MAX_REG_VAL_PAIR_SIZE - MAX_BUF_UPDATE_REG_NUM * 2)) {
			CAM_ERR(CAM_SFE,
				"reg_val_pair %d exceeds the array limit %zu",
				j, MAX_REG_VAL_PAIR_SIZE);
			return -ENOMEM;
		}

		wm_data = sfe_out_data->wm_res[i].res_priv;
		if ((wm_data->framedrop_pattern !=
			hfr_cfg->framedrop_pattern) ||
			!wm_data->hfr_cfg_done) {
			CAM_SFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
				wm_data->hw_regs->framedrop_pattern,
				hfr_cfg->framedrop_pattern);
			wm_data->framedrop_pattern = hfr_cfg->framedrop_pattern;
			CAM_DBG(CAM_SFE, "WM:%d framedrop pattern 0x%X",
				wm_data->index, wm_data->framedrop_pattern);
		}

		if (wm_data->framedrop_period != hfr_cfg->framedrop_period ||
			!wm_data->hfr_cfg_done) {
			CAM_SFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
				wm_data->hw_regs->framedrop_period,
				hfr_cfg->framedrop_period);
			wm_data->framedrop_period = hfr_cfg->framedrop_period;
			CAM_DBG(CAM_SFE, "WM:%d framedrop period 0x%X",
				wm_data->index, wm_data->framedrop_period);
		}

		if (wm_data->irq_subsample_period != hfr_cfg->subsample_period
			|| !wm_data->hfr_cfg_done) {
			CAM_SFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
				wm_data->hw_regs->irq_subsample_period,
				hfr_cfg->subsample_period);
			wm_data->irq_subsample_period =
				hfr_cfg->subsample_period;
			CAM_DBG(CAM_SFE, "WM:%d irq subsample period 0x%X",
				wm_data->index, wm_data->irq_subsample_period);
		}

		if (wm_data->irq_subsample_pattern != hfr_cfg->subsample_pattern
			|| !wm_data->hfr_cfg_done) {
			CAM_SFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
				wm_data->hw_regs->irq_subsample_pattern,
				hfr_cfg->subsample_pattern);
			wm_data->irq_subsample_pattern =
				hfr_cfg->subsample_pattern;
			CAM_DBG(CAM_SFE, "WM:%d irq subsample pattern 0x%X",
				wm_data->index, wm_data->irq_subsample_pattern);
		}

		/* set initial configuration done */
		if (!wm_data->hfr_cfg_done)
			wm_data->hfr_cfg_done = true;
	}

	size = sfe_out_data->cdm_util_ops->cdm_required_size_reg_random(j/2);

	/* cdm util returns dwords, need to convert to bytes */
	if ((size * 4) > update_hfr->cmd.size) {
		CAM_ERR(CAM_SFE,
			"Failed! Buf size:%d insufficient, expected size:%d",
			update_hfr->cmd.size, size);
		return -ENOMEM;
	}

	sfe_out_data->cdm_util_ops->cdm_write_regrandom(
		update_hfr->cmd.cmd_buf_addr, j/2, reg_val_pair);

	/* cdm util returns dwords, need to convert to bytes */
	update_hfr->cmd.used_bytes = size * 4;

	return 0;
}

static int cam_sfe_bus_wr_update_stripe_cfg(void *priv, void *cmd_args,
	uint32_t arg_size)
{
	struct cam_sfe_bus_wr_priv                *bus_priv;
	struct cam_isp_hw_dual_isp_update_args    *stripe_args;
	struct cam_sfe_bus_wr_out_data            *sfe_out_data = NULL;
	struct cam_sfe_bus_wr_wm_resource_data    *wm_data = NULL;
	struct cam_isp_dual_stripe_config         *stripe_config;
	uint32_t outport_id, ports_plane_idx, i;

	bus_priv = (struct cam_sfe_bus_wr_priv  *) priv;
	stripe_args = (struct cam_isp_hw_dual_isp_update_args *)cmd_args;

	sfe_out_data = (struct cam_sfe_bus_wr_out_data *)
		stripe_args->res->res_priv;

	if (!sfe_out_data) {
		CAM_ERR(CAM_SFE, "Failed! Invalid data");
		return -EINVAL;
	}

	outport_id = stripe_args->res->res_id & 0xFF;
	if (stripe_args->res->res_id < CAM_ISP_SFE_OUT_RES_BASE ||
		stripe_args->res->res_id >= CAM_ISP_SFE_OUT_RES_MAX)
		return 0;

	ports_plane_idx = (stripe_args->split_id *
	(stripe_args->dual_cfg->num_ports * CAM_PACKET_MAX_PLANES)) +
	(outport_id * CAM_PACKET_MAX_PLANES);
	for (i = 0; i < sfe_out_data->num_wm; i++) {
		wm_data = sfe_out_data->wm_res[i].res_priv;
		stripe_config = (struct cam_isp_dual_stripe_config  *)
			&stripe_args->dual_cfg->stripes[ports_plane_idx + i];
		wm_data->width = stripe_config->width;

		/*
		 * UMD sends buffer offset address as offset for clients
		 * programmed to operate in frame/index based mode and h_init
		 * value as offset for clients programmed to operate in line
		 * based mode.
		 */

		if (wm_data->en_cfg & (0x3 << 16))
			wm_data->offset = stripe_config->offset;
		else
			wm_data->h_init = stripe_config->offset;

		CAM_DBG(CAM_SFE,
			"out_type:0x%X WM:%d width:%d offset:0x%X h_init:%d",
			stripe_args->res->res_id, wm_data->index,
			wm_data->width, wm_data->offset, wm_data->h_init);
	}

	return 0;
}

static int cam_sfe_bus_wr_update_wm_config(
	void                                        *cmd_args)
{
	int                                          i;
	struct cam_isp_hw_get_cmd_update            *wm_config_update;
	struct cam_sfe_bus_wr_out_data              *sfe_out_data = NULL;
	struct cam_sfe_bus_wr_wm_resource_data      *wm_data = NULL;
	struct cam_isp_vfe_wm_config                *wm_config = NULL;

	if (!cmd_args) {
		CAM_ERR(CAM_SFE, "Invalid args");
		return -EINVAL;
	}

	wm_config_update = cmd_args;
	sfe_out_data = wm_config_update->res->res_priv;
	wm_config = (struct cam_isp_vfe_wm_config  *)
		wm_config_update->data;

	if (!sfe_out_data || !sfe_out_data->cdm_util_ops || !wm_config) {
		CAM_ERR(CAM_SFE, "Invalid data");
		return -EINVAL;
	}

	for (i = 0; i < sfe_out_data->num_wm; i++) {
		wm_data = sfe_out_data->wm_res[i].res_priv;

		if (wm_config->wm_mode > 0x2) {
			CAM_ERR(CAM_SFE, "Invalid wm_mode: 0x%X WM:%d",
				wm_config->wm_mode, wm_data->index);
			return -EINVAL;
		}

		wm_data->en_cfg = (wm_config->wm_mode << 16) | 0x1;
		wm_data->width  = wm_config->width;

		if (i == PLANE_C)
			wm_data->height = wm_config->height / 2;
		else
			wm_data->height = wm_config->height;

		CAM_DBG(CAM_SFE,
			"WM:%d en_cfg:0x%X height:%d width:%d",
			wm_data->index, wm_data->en_cfg, wm_data->height,
			wm_data->width);
	}

	return 0;
}

static int cam_sfe_bus_wr_start_hw(void *hw_priv,
	void *start_hw_args, uint32_t arg_size)
{
	return cam_sfe_bus_start_sfe_out(hw_priv);
}

static int cam_sfe_bus_wr_stop_hw(void *hw_priv,
	void *stop_hw_args, uint32_t arg_size)
{
	return cam_sfe_bus_stop_sfe_out(hw_priv);
}

static int cam_sfe_bus_wr_init_hw(void *hw_priv,
	void *init_hw_args, uint32_t arg_size)
{
	struct cam_sfe_bus_wr_priv    *bus_priv = hw_priv;
	uint32_t top_irq_reg_mask[CAM_SFE_IRQ_REGISTERS_MAX] = {0};

	if (!bus_priv) {
		CAM_ERR(CAM_SFE, "Invalid args");
		return -EINVAL;
	}

	if (bus_priv->common_data.hw_init)
		return 0;

	/* Subscribe top IRQ */
	top_irq_reg_mask[0] = (1 << bus_priv->top_irq_shift);

	bus_priv->bus_irq_handle = cam_irq_controller_subscribe_irq(
		bus_priv->common_data.sfe_irq_controller,
		CAM_IRQ_PRIORITY_4,
		top_irq_reg_mask,
		bus_priv,
		cam_sfe_bus_wr_handle_bus_irq,
		NULL,
		NULL,
		NULL);

	if (bus_priv->bus_irq_handle < 1) {
		CAM_ERR(CAM_SFE, "Failed to subscribe BUS (buf_done) IRQ");
		bus_priv->bus_irq_handle = 0;
		return -EFAULT;
	}

	if (bus_priv->tasklet_info != NULL) {
		bus_priv->error_irq_handle = cam_irq_controller_subscribe_irq(
			bus_priv->common_data.bus_irq_controller,
			CAM_IRQ_PRIORITY_0,
			bus_wr_error_irq_mask,
			bus_priv,
			cam_sfe_bus_wr_err_irq_top_half,
			cam_sfe_bus_wr_irq_bottom_half,
			bus_priv->tasklet_info,
			&tasklet_bh_api);

		if (bus_priv->error_irq_handle < 1) {
			CAM_ERR(CAM_SFE, "Failed to subscribe BUS Error IRQ");
			bus_priv->error_irq_handle = 0;
			return -EFAULT;
		}
	}

	/* BUS_WR_TEST_BUS_CTRL */
	cam_io_w_mb(0x0, bus_priv->common_data.mem_base +
		bus_priv->common_data.common_reg->test_bus_ctrl);

	bus_priv->common_data.hw_init = true;

	return 0;
}

static int cam_sfe_bus_wr_deinit_hw(void *hw_priv,
	void *deinit_hw_args, uint32_t arg_size)
{
	struct cam_sfe_bus_wr_priv    *bus_priv = hw_priv;
	int                            rc = 0;

	if (!bus_priv) {
		CAM_ERR(CAM_SFE, "Error: Invalid args");
		return -EINVAL;
	}

	if (!bus_priv->common_data.hw_init)
		return 0;

	/* To Do Unsubscribe IRQs */

	return rc;
}

static int __cam_sfe_bus_wr_process_cmd(
	void *priv, uint32_t cmd_type,
	void *cmd_args, uint32_t arg_size)
{
	return cam_sfe_bus_wr_process_cmd(priv, cmd_type,
		cmd_args, arg_size);
}

static int cam_sfe_bus_wr_process_cmd(
	struct cam_isp_resource_node *priv,
	uint32_t cmd_type, void *cmd_args,
	uint32_t arg_size)
{
	int rc = -EINVAL;
	struct cam_sfe_bus_wr_priv *bus_priv;

	if (!priv || !cmd_args) {
		CAM_ERR_RATE_LIMIT(CAM_SFE, "Invalid input arguments");
		return -EINVAL;
	}

	switch (cmd_type) {
	case CAM_ISP_HW_CMD_GET_BUF_UPDATE:
		rc = cam_sfe_bus_wr_update_wm(priv, cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_GET_HFR_UPDATE:
		rc = cam_sfe_bus_wr_update_hfr(priv, cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_GET_SECURE_MODE:
		rc = cam_sfe_bus_wr_get_secure_mode(priv, cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_STRIPE_UPDATE:
		rc = cam_sfe_bus_wr_update_stripe_cfg(priv,
			cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_STOP_BUS_ERR_IRQ:
		bus_priv = (struct cam_sfe_bus_wr_priv  *) priv;
		/* Handle bus err IRQ */
		break;
	case CAM_ISP_HW_CMD_DUMP_BUS_INFO: {
		struct cam_isp_hw_event_info  *event_info;
		enum cam_sfe_bus_sfe_out_type  sfe_out_res_id;

		event_info =
			(struct cam_isp_hw_event_info *)cmd_args;
		bus_priv = (struct cam_sfe_bus_wr_priv  *) priv;
		sfe_out_res_id =
			cam_sfe_bus_wr_get_out_res_id(event_info->res_id);
		rc = cam_sfe_bus_wr_print_dimensions(
			sfe_out_res_id, bus_priv);
		break;
		}
	case CAM_ISP_HW_CMD_WM_CONFIG_UPDATE:
		rc = cam_sfe_bus_wr_update_wm_config(cmd_args);
		break;
	default:
		CAM_ERR_RATE_LIMIT(CAM_SFE, "Invalid HW command type:%d",
			cmd_type);
		break;
	}

	return rc;
}

int cam_sfe_bus_wr_init(
	struct cam_hw_soc_info               *soc_info,
	struct cam_hw_intf                   *hw_intf,
	void                                 *bus_hw_info,
	void                                 *sfe_irq_controller,
	struct cam_sfe_bus                  **sfe_bus)
{
	int i, rc = 0;
	struct cam_sfe_bus_wr_priv    *bus_priv = NULL;
	struct cam_sfe_bus            *sfe_bus_local;
	struct cam_sfe_bus_wr_hw_info *hw_info = bus_hw_info;

	CAM_DBG(CAM_SFE, "Enter");

	if (!soc_info || !hw_intf || !bus_hw_info) {
		CAM_ERR(CAM_SFE,
			"Inval_prms soc_info:%pK hw_intf:%pK hw_info%pK",
			soc_info, hw_intf, bus_hw_info);
		rc = -EINVAL;
		goto end;
	}

	sfe_bus_local = kzalloc(sizeof(struct cam_sfe_bus), GFP_KERNEL);
	if (!sfe_bus_local) {
		CAM_DBG(CAM_SFE, "Failed to alloc for sfe_bus");
		rc = -ENOMEM;
		goto end;
	}

	bus_priv = kzalloc(sizeof(struct cam_sfe_bus_wr_priv),
		GFP_KERNEL);
	if (!bus_priv) {
		CAM_DBG(CAM_SFE, "Failed to alloc for sfe_bus_priv");
		rc = -ENOMEM;
		goto free_bus_local;
	}
	sfe_bus_local->bus_priv = bus_priv;

	bus_priv->num_client                     = hw_info->num_client;
	bus_priv->num_out                        = hw_info->num_out;
	bus_priv->num_comp_grp                   = hw_info->num_comp_grp;
	bus_priv->top_irq_shift                  = hw_info->top_irq_shift;
	bus_priv->common_data.num_sec_out        = 0;
	bus_priv->common_data.secure_mode        = CAM_SECURE_MODE_NON_SECURE;
	bus_priv->common_data.core_index         = soc_info->index;
	bus_priv->common_data.mem_base           =
		CAM_SOC_GET_REG_MAP_START(soc_info, SFE_CORE_BASE_IDX);
	bus_priv->common_data.hw_intf            = hw_intf;
	bus_priv->common_data.common_reg         = &hw_info->common_reg;
	bus_priv->common_data.comp_done_shift    = hw_info->comp_done_shift;
	bus_priv->common_data.hw_init            = false;
	bus_priv->common_data.sfe_irq_controller = sfe_irq_controller;
	rc = cam_cpas_get_cpas_hw_version(&bus_priv->common_data.hw_version);
	if (rc) {
		CAM_ERR(CAM_SFE, "Failed to get hw_version rc:%d", rc);
		goto free_bus_priv;
	}

	bus_priv->comp_grp = kzalloc((sizeof(struct cam_isp_resource_node) *
		bus_priv->num_comp_grp), GFP_KERNEL);
	if (!bus_priv->comp_grp) {
		CAM_ERR(CAM_SFE, "Failed to alloc for bus comp groups");
		rc = -ENOMEM;
		goto free_bus_priv;
	}

	bus_priv->sfe_out = kzalloc((sizeof(struct cam_isp_resource_node) *
		CAM_SFE_BUS_SFE_OUT_MAX), GFP_KERNEL);
	if (!bus_priv->sfe_out) {
		CAM_ERR(CAM_SFE, "Failed to alloc for bus out res");
		rc = -ENOMEM;
		goto free_comp_grp;
	}

	mutex_init(&bus_priv->common_data.bus_mutex);
	rc = cam_irq_controller_init(drv_name,
		bus_priv->common_data.mem_base,
		&hw_info->common_reg.irq_reg_info,
		&bus_priv->common_data.bus_irq_controller,
		false);
	if (rc) {
		CAM_ERR(CAM_SFE, "Init bus_irq_controller failed");
		goto free_bus_priv;
	}

	INIT_LIST_HEAD(&bus_priv->free_comp_grp);
	INIT_LIST_HEAD(&bus_priv->used_comp_grp);

	for (i = 0; i < bus_priv->num_comp_grp; i++) {
		rc = cam_sfe_bus_wr_init_comp_grp(i, soc_info,
			bus_priv, bus_hw_info,
			&bus_priv->comp_grp[i]);
		if (rc < 0) {
			CAM_ERR(CAM_SFE, "SFE:%d init comp_grp:%d failed rc:%d",
				bus_priv->common_data.core_index, i, rc);
			goto deinit_comp_grp;
		}
	}

	for (i = 0; i < bus_priv->num_out; i++) {
		rc = cam_sfe_bus_init_sfe_out_resource(i, bus_priv,
			bus_hw_info);
		if (rc < 0) {
			CAM_ERR(CAM_SFE,
				"SFE:%d init out_type:0x%X failed rc:%d",
				bus_priv->common_data.core_index, i, rc);
			goto deinit_sfe_out;
		}
	}

	spin_lock_init(&bus_priv->common_data.spin_lock);
	INIT_LIST_HEAD(&bus_priv->common_data.free_payload_list);

	for (i = 0; i < CAM_SFE_BUS_WR_PAYLOAD_MAX; i++) {
		INIT_LIST_HEAD(&bus_priv->common_data.evt_payload[i].list);
		list_add_tail(&bus_priv->common_data.evt_payload[i].list,
			&bus_priv->common_data.free_payload_list);
	}

	sfe_bus_local->hw_ops.reserve      = cam_sfe_bus_acquire_sfe_out;
	sfe_bus_local->hw_ops.release      = cam_sfe_bus_release_sfe_out;
	sfe_bus_local->hw_ops.start        = cam_sfe_bus_wr_start_hw;
	sfe_bus_local->hw_ops.stop         = cam_sfe_bus_wr_stop_hw;
	sfe_bus_local->hw_ops.init         = cam_sfe_bus_wr_init_hw;
	sfe_bus_local->hw_ops.deinit       = cam_sfe_bus_wr_deinit_hw;
	sfe_bus_local->top_half_handler    = NULL;
	sfe_bus_local->bottom_half_handler = NULL;
	sfe_bus_local->hw_ops.process_cmd  = __cam_sfe_bus_wr_process_cmd;

	*sfe_bus = sfe_bus_local;

	CAM_DBG(CAM_SFE, "Exit");
	return rc;

deinit_sfe_out:
	if (i < 0)
		i = CAM_SFE_BUS_SFE_OUT_MAX;
	for (--i; i >= 0; i--)
		cam_sfe_bus_deinit_sfe_out_resource(&bus_priv->sfe_out[i]);

deinit_comp_grp:
	if (i < 0)
		i = bus_priv->num_comp_grp;
	for (--i; i >= 0; i--)
		cam_sfe_bus_deinit_comp_grp(&bus_priv->comp_grp[i]);
	kfree(bus_priv->sfe_out);

free_comp_grp:
	kfree(bus_priv->comp_grp);

free_bus_priv:
	kfree(sfe_bus_local->bus_priv);

free_bus_local:
	kfree(sfe_bus_local);

end:
	return rc;
}

int cam_sfe_bus_wr_deinit(
	struct cam_sfe_bus                  **sfe_bus)
{
	int i, rc = 0;
	struct cam_sfe_bus_wr_priv    *bus_priv = NULL;
	struct cam_sfe_bus            *sfe_bus_local;
	unsigned long                  flags;

	if (!sfe_bus || !*sfe_bus) {
		CAM_ERR(CAM_SFE, "Invalid input");
		return -EINVAL;
	}
	sfe_bus_local = *sfe_bus;

	bus_priv = sfe_bus_local->bus_priv;
	if (!bus_priv) {
		CAM_ERR(CAM_SFE, "bus_priv is NULL");
		rc = -ENODEV;
		goto free_bus_local;
	}

	spin_lock_irqsave(&bus_priv->common_data.spin_lock, flags);
	INIT_LIST_HEAD(&bus_priv->common_data.free_payload_list);
	for (i = 0; i < CAM_SFE_BUS_WR_PAYLOAD_MAX; i++)
		INIT_LIST_HEAD(&bus_priv->common_data.evt_payload[i].list);
	bus_priv->common_data.hw_init = false;
	spin_unlock_irqrestore(&bus_priv->common_data.spin_lock, flags);

	for (i = 0; i < CAM_SFE_BUS_WR_COMP_GRP_MAX; i++) {
		rc = cam_sfe_bus_deinit_comp_grp(&bus_priv->comp_grp[i]);
		if (rc < 0)
			CAM_ERR(CAM_SFE,
				"SFE:%d deinit comp_grp:%d failed rc:%d",
				bus_priv->common_data.core_index, i, rc);
	}

	for (i = 0; i < CAM_SFE_BUS_SFE_OUT_MAX; i++) {
		rc = cam_sfe_bus_deinit_sfe_out_resource(
			&bus_priv->sfe_out[i]);
		if (rc < 0)
			CAM_ERR(CAM_SFE,
				"SFE:%d deinit out_type:0x%X failed rc:%d",
				bus_priv->common_data.core_index, i, rc);
	}

	INIT_LIST_HEAD(&bus_priv->free_comp_grp);
	INIT_LIST_HEAD(&bus_priv->used_comp_grp);

	mutex_destroy(&bus_priv->common_data.bus_mutex);
	kfree(sfe_bus_local->bus_priv);

free_bus_local:
	kfree(sfe_bus_local);

	*sfe_bus = NULL;

	return rc;
}
