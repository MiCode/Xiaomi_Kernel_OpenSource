// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
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
#include "cam_common_util.h"

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

enum cam_sfe_bus_wr_wm_mode {
	CAM_SFE_WM_LINE_BASED_MODE,
	CAM_SFE_WM_FRAME_BASED_MODE,
	CAM_SFE_WM_INDEX_BASED_MODE,
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
	uint32_t                                    line_done_cfg;
	uint32_t                                    pack_align_shift;
	uint32_t                                    max_bw_counter_limit;
	bool                                        err_irq_subscribe;
	cam_hw_mgr_event_cb_func                    event_cb;

	uint32_t                                    sfe_debug_cfg;
	struct cam_sfe_bus_cache_dbg_cfg            cache_dbg_cfg;
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
	enum cam_sfe_bus_wr_wm_mode wm_mode;

	uint32_t             h_init;

	uint32_t             irq_subsample_period;
	uint32_t             irq_subsample_pattern;
	uint32_t             framedrop_period;
	uint32_t             framedrop_pattern;

	uint32_t             en_cfg;
	uint32_t             is_dual;

	uint32_t             acquired_width;
	uint32_t             acquired_height;

	bool                 enable_caching;
	uint32_t             cache_cfg;
	int32_t              current_scid;
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
	struct cam_sfe_bus_wr_priv           *bus_priv;

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
	uint32_t                            num_cons_err;
	struct cam_sfe_constraint_error_info      *constraint_error_list;
	struct cam_sfe_bus_sfe_out_hw_info        *sfe_out_hw_info;
};

static int cam_sfe_bus_subscribe_error_irq(
	struct cam_sfe_bus_wr_priv  *bus_priv);

static bool cam_sfe_bus_can_be_secure(uint32_t out_type)
{
	switch (out_type) {
	case CAM_SFE_BUS_SFE_OUT_RAW_DUMP:
	case CAM_SFE_BUS_SFE_OUT_RDI0:
	case CAM_SFE_BUS_SFE_OUT_RDI1:
	case CAM_SFE_BUS_SFE_OUT_RDI2:
	case CAM_SFE_BUS_SFE_OUT_RDI3:
	case CAM_SFE_BUS_SFE_OUT_RDI4:
	case CAM_SFE_BUS_SFE_OUT_IR:
		return true;
	case CAM_SFE_BUS_SFE_OUT_LCR:
	case CAM_SFE_BUS_SFE_OUT_BE_0:
	case CAM_SFE_BUS_SFE_OUT_BHIST_0:
	case CAM_SFE_BUS_SFE_OUT_BE_1:
	case CAM_SFE_BUS_SFE_OUT_BHIST_1:
	case CAM_SFE_BUS_SFE_OUT_BE_2:
	case CAM_SFE_BUS_SFE_OUT_BHIST_2:
	case CAM_SFE_BUS_SFE_OUT_BAYER_RS_0:
	case CAM_SFE_BUS_SFE_OUT_BAYER_RS_1:
	case CAM_SFE_BUS_SFE_OUT_BAYER_RS_2:
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
	case CAM_ISP_SFE_OUT_RES_IR:
		return CAM_SFE_BUS_SFE_OUT_IR;
	case CAM_ISP_SFE_OUT_BAYER_RS_STATS_0:
		return CAM_SFE_BUS_SFE_OUT_BAYER_RS_0;
	case CAM_ISP_SFE_OUT_BAYER_RS_STATS_1:
		return CAM_SFE_BUS_SFE_OUT_BAYER_RS_1;
	case CAM_ISP_SFE_OUT_BAYER_RS_STATS_2:
		return CAM_SFE_BUS_SFE_OUT_BAYER_RS_2;
	default:
		return CAM_SFE_BUS_SFE_OUT_MAX;
	}
}

static int cam_sfe_bus_get_comp_sfe_out_res_id_list(
	uint32_t comp_mask, uint32_t *out_list, int *num_out)
{
	int count = 0;

	if (comp_mask & (BIT(CAM_SFE_BUS_SFE_OUT_RDI0)))
		out_list[count++] = CAM_ISP_SFE_OUT_RES_RDI_0;

	if (comp_mask & (BIT(CAM_SFE_BUS_SFE_OUT_RDI1)))
		out_list[count++] = CAM_ISP_SFE_OUT_RES_RDI_1;

	if (comp_mask & (BIT(CAM_SFE_BUS_SFE_OUT_RDI2)))
		out_list[count++] = CAM_ISP_SFE_OUT_RES_RDI_2;

	if (comp_mask & (BIT(CAM_SFE_BUS_SFE_OUT_RDI3)))
		out_list[count++] = CAM_ISP_SFE_OUT_RES_RDI_3;

	if (comp_mask & (BIT(CAM_SFE_BUS_SFE_OUT_RDI4)))
		out_list[count++] = CAM_ISP_SFE_OUT_RES_RDI_4;

	if (comp_mask & (BIT(CAM_SFE_BUS_SFE_OUT_RAW_DUMP)))
		out_list[count++] = CAM_ISP_SFE_OUT_RES_RAW_DUMP;

	if (comp_mask & (BIT(CAM_SFE_BUS_SFE_OUT_BE_0)))
		out_list[count++] = CAM_ISP_SFE_OUT_BE_STATS_0;

	if (comp_mask & (BIT(CAM_SFE_BUS_SFE_OUT_BHIST_0)))
		out_list[count++] = CAM_ISP_SFE_OUT_BHIST_STATS_0;

	if (comp_mask & (BIT(CAM_SFE_BUS_SFE_OUT_BE_1)))
		out_list[count++] = CAM_ISP_SFE_OUT_BE_STATS_1;

	if (comp_mask & (BIT(CAM_SFE_BUS_SFE_OUT_BHIST_1)))
		out_list[count++] = CAM_ISP_SFE_OUT_BHIST_STATS_1;

	if (comp_mask & (BIT(CAM_SFE_BUS_SFE_OUT_BE_2)))
		out_list[count++] = CAM_ISP_SFE_OUT_BE_STATS_2;

	if (comp_mask & (BIT(CAM_SFE_BUS_SFE_OUT_BHIST_2)))
		out_list[count++] = CAM_ISP_SFE_OUT_BHIST_STATS_2;

	if (comp_mask & (BIT(CAM_SFE_BUS_SFE_OUT_LCR)))
		out_list[count++] = CAM_ISP_SFE_OUT_RES_LCR;

	if (comp_mask & (BIT(CAM_SFE_BUS_SFE_OUT_IR)))
		out_list[count++] = CAM_ISP_SFE_OUT_RES_IR;

	if (comp_mask & (BIT(CAM_SFE_BUS_SFE_OUT_BAYER_RS_0)))
		out_list[count++] = CAM_ISP_SFE_OUT_BAYER_RS_STATS_0;

	if (comp_mask & (BIT(CAM_SFE_BUS_SFE_OUT_BAYER_RS_1)))
		out_list[count++] = CAM_ISP_SFE_OUT_BAYER_RS_STATS_1;

	if (comp_mask & (BIT(CAM_SFE_BUS_SFE_OUT_BAYER_RS_2)))
		out_list[count++] = CAM_ISP_SFE_OUT_BAYER_RS_STATS_2;

	*num_out = count;
	return 0;
}

bool cam_sfe_is_mipi_pcking_needed(
	struct cam_sfe_bus_wr_priv *bus_priv,
	int wm_index)
{
	int i;

	for(i = 0; i < bus_priv->num_out; i++)
	{
		if (((wm_index == bus_priv->sfe_out_hw_info[i].wm_idx) &&
			(bus_priv->sfe_out_hw_info[i].sfe_out_type ==
				CAM_SFE_BUS_SFE_OUT_RAW_DUMP)) ||
			((wm_index == bus_priv->sfe_out_hw_info[i].wm_idx) &&
				(bus_priv->sfe_out_hw_info[i].sfe_out_type ==
					CAM_SFE_BUS_SFE_OUT_IR)))
		       return true;
	}

	return false;
}

static enum cam_sfe_bus_wr_packer_format
	cam_sfe_bus_get_packer_fmt(
	struct cam_sfe_bus_wr_priv *bus_priv,
	uint32_t out_fmt,
	int wm_index)
{
	bool is_mipi_packing =
		cam_sfe_is_mipi_pcking_needed(bus_priv, wm_index);

	switch (out_fmt) {
	case CAM_FORMAT_MIPI_RAW_6:
	case CAM_FORMAT_MIPI_RAW_8:
	case CAM_FORMAT_MIPI_RAW_16:
	case CAM_FORMAT_PLAIN16_8:
	case CAM_FORMAT_PLAIN128:
	case CAM_FORMAT_PD8:
		return PACKER_FMT_PLAIN_128;
	case CAM_FORMAT_MIPI_RAW_10:
		if (is_mipi_packing)
			return PACKER_FMT_MIPI10;
		else
			return PACKER_FMT_PLAIN_128;
	case CAM_FORMAT_MIPI_RAW_12:
		if (is_mipi_packing)
			return PACKER_FMT_MIPI12;
		else
			return PACKER_FMT_PLAIN_128;
	case CAM_FORMAT_MIPI_RAW_14:
		if (is_mipi_packing)
			return PACKER_FMT_MIPI14;
		else
			return PACKER_FMT_PLAIN_128;
	case CAM_FORMAT_MIPI_RAW_20:
		if (is_mipi_packing)
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

static void cam_sfe_bus_wr_print_constraint_errors(
	struct cam_sfe_bus_wr_priv *bus_priv,
	uint8_t *wm_name,
	uint32_t constraint_errors)
{
	uint32_t i;

	CAM_INFO(CAM_ISP, "Constraint violation bitflags: 0x%X",
		constraint_errors);

	for (i = 0; i < bus_priv->num_cons_err; i++) {
		if (bus_priv->constraint_error_list[i].bitmask &
			constraint_errors) {
			CAM_INFO(CAM_ISP, "WM: %s %s",
				wm_name, bus_priv->constraint_error_list[i]
				.error_description);
		}
	}
}

static void cam_sfe_bus_wr_get_constraint_errors(
	bool                       *skip_error_notify,
	struct cam_sfe_bus_wr_priv *bus_priv)
{
	uint32_t i, j, constraint_errors;
	uint8_t *wm_name = NULL;
	struct cam_isp_resource_node              *out_rsrc_node = NULL;
	struct cam_sfe_bus_wr_out_data            *out_rsrc_data = NULL;
	struct cam_sfe_bus_wr_wm_resource_data    *wm_data   = NULL;

	for (i = 0; i < bus_priv->num_out; i++) {
		out_rsrc_node = &bus_priv->sfe_out[i];
		if (!out_rsrc_node || !out_rsrc_node->res_priv) {
			CAM_DBG(CAM_ISP,
				"SFE out:%d out rsrc node or data is NULL", i);
			continue;
		}

		out_rsrc_data = out_rsrc_node->res_priv;
		for (j = 0; j < out_rsrc_data->num_wm; j++) {
			wm_data = out_rsrc_data->wm_res[j].res_priv;
			wm_name = out_rsrc_data->wm_res[j].res_name;
			if (wm_data) {
				constraint_errors = cam_io_r_mb(
					bus_priv->common_data.mem_base +
					wm_data->hw_regs->debug_status_1);
				if (!constraint_errors)
					continue;

				/*
				 * Due to a HW bug in constraint checker skip addr unalign
				 * for RDI clients
				 */
				if ((out_rsrc_data->out_type >= CAM_SFE_BUS_SFE_OUT_RDI0) &&
					(out_rsrc_data->out_type <= CAM_SFE_BUS_SFE_OUT_RDI4) &&
					(constraint_errors >> 21)) {
					*skip_error_notify = true;
					CAM_DBG(CAM_SFE, "WM: %s constraint_error: 0x%x",
						wm_name, constraint_errors);
					continue;
				}

				cam_sfe_bus_wr_print_constraint_errors(
					bus_priv, wm_name, constraint_errors);
			}
		}
	}
}

static inline void cam_sfe_bus_config_rdi_wm_frame_based_mode(
	struct cam_sfe_bus_wr_wm_resource_data  *rsrc_data)
{
	rsrc_data->width = CAM_SFE_RDI_BUS_DEFAULT_WIDTH;
	rsrc_data->height = 0;
	rsrc_data->stride = CAM_SFE_RDI_BUS_DEFAULT_STRIDE;
	rsrc_data->en_cfg = (0x1 << 16) | 0x1;
}

static int cam_sfe_bus_config_rdi_wm(
	struct cam_sfe_bus_wr_wm_resource_data  *rsrc_data)
{
	rsrc_data->pack_fmt = 0x0;
	switch (rsrc_data->format) {
	case CAM_FORMAT_MIPI_RAW_10:
		if (rsrc_data->wm_mode == CAM_SFE_WM_LINE_BASED_MODE) {
			rsrc_data->en_cfg = 0x1;
			rsrc_data->width =
				ALIGNUP((rsrc_data->width * 5) / 4, 16) / 16;
		} else if (rsrc_data->wm_mode == CAM_SFE_WM_FRAME_BASED_MODE) {
			cam_sfe_bus_config_rdi_wm_frame_based_mode(rsrc_data);
		} else {
			CAM_WARN(CAM_SFE, "No index mode support for SFE WM: %u",
				rsrc_data->index);
		}
		break;
	case CAM_FORMAT_MIPI_RAW_6:
		if (rsrc_data->wm_mode == CAM_SFE_WM_LINE_BASED_MODE) {
			rsrc_data->en_cfg = 0x1;
			rsrc_data->width =
				ALIGNUP((rsrc_data->width * 3) / 4, 16) / 16;
		} else if (rsrc_data->wm_mode == CAM_SFE_WM_FRAME_BASED_MODE) {
			cam_sfe_bus_config_rdi_wm_frame_based_mode(rsrc_data);
		} else {
			CAM_WARN(CAM_SFE, "No index mode support for SFE WM: %u",
				rsrc_data->index);
		}
		break;
	case CAM_FORMAT_MIPI_RAW_8:
		if (rsrc_data->wm_mode == CAM_SFE_WM_LINE_BASED_MODE) {
			rsrc_data->en_cfg = 0x1;
			rsrc_data->width =
				ALIGNUP(rsrc_data->width, 16) / 16;
		} else if (rsrc_data->wm_mode == CAM_SFE_WM_FRAME_BASED_MODE) {
			cam_sfe_bus_config_rdi_wm_frame_based_mode(rsrc_data);
		} else {
			CAM_WARN(CAM_SFE, "No index mode support for SFE WM: %u",
				rsrc_data->index);
		}
		break;
	case CAM_FORMAT_MIPI_RAW_12:
		if (rsrc_data->wm_mode == CAM_SFE_WM_LINE_BASED_MODE) {
			rsrc_data->en_cfg = 0x1;
			rsrc_data->width =
				ALIGNUP((rsrc_data->width * 3) / 2, 16) / 16;
		} else if (rsrc_data->wm_mode == CAM_SFE_WM_FRAME_BASED_MODE) {
			cam_sfe_bus_config_rdi_wm_frame_based_mode(rsrc_data);
		} else {
			CAM_WARN(CAM_SFE, "No index mode support for SFE WM: %u",
				rsrc_data->index);
		}
		break;
	case CAM_FORMAT_MIPI_RAW_14:
		if (rsrc_data->wm_mode == CAM_SFE_WM_LINE_BASED_MODE) {
			rsrc_data->en_cfg = 0x1;
			rsrc_data->width =
				ALIGNUP((rsrc_data->width * 7) / 2, 16) / 16;
		} else if (rsrc_data->wm_mode == CAM_SFE_WM_FRAME_BASED_MODE) {
			cam_sfe_bus_config_rdi_wm_frame_based_mode(rsrc_data);
		} else {
			CAM_WARN(CAM_SFE, "No index mode support for SFE WM: %u",
				rsrc_data->index);
		}
		break;
	case CAM_FORMAT_MIPI_RAW_16:
		if (rsrc_data->wm_mode == CAM_SFE_WM_LINE_BASED_MODE) {
			rsrc_data->en_cfg = 0x1;
			rsrc_data->width =
				ALIGNUP((rsrc_data->width * 2), 16) / 16;
		} else if (rsrc_data->wm_mode == CAM_SFE_WM_FRAME_BASED_MODE) {
			cam_sfe_bus_config_rdi_wm_frame_based_mode(rsrc_data);
		} else {
			CAM_WARN(CAM_SFE, "No index mode support for SFE WM: %u",
				rsrc_data->index);
		}
		break;
	case CAM_FORMAT_MIPI_RAW_20:
		if (rsrc_data->wm_mode == CAM_SFE_WM_LINE_BASED_MODE) {
			rsrc_data->en_cfg = 0x1;
			rsrc_data->width =
				ALIGNUP((rsrc_data->width * 5) / 2, 16) / 16;
		} else if (rsrc_data->wm_mode == CAM_SFE_WM_FRAME_BASED_MODE) {
			cam_sfe_bus_config_rdi_wm_frame_based_mode(rsrc_data);
		} else {
			CAM_WARN(CAM_SFE, "No index mode support for SFE WM: %u",
				rsrc_data->index);
		}
		break;
	case CAM_FORMAT_PLAIN128:
		if (rsrc_data->wm_mode == CAM_SFE_WM_LINE_BASED_MODE) {
			rsrc_data->en_cfg = 0x1;
			rsrc_data->width =
				ALIGNUP((rsrc_data->width * 16), 16) / 16;
		} else if (rsrc_data->wm_mode == CAM_SFE_WM_FRAME_BASED_MODE) {
			cam_sfe_bus_config_rdi_wm_frame_based_mode(rsrc_data);
		} else {
			CAM_WARN(CAM_SFE, "No index mode support for SFE WM: %u",
				rsrc_data->index);
		}
		break;
	case CAM_FORMAT_PLAIN32_20:
		if (rsrc_data->wm_mode == CAM_SFE_WM_LINE_BASED_MODE) {
			rsrc_data->en_cfg = 0x1;
			rsrc_data->width =
				ALIGNUP((rsrc_data->width * 4), 16) / 16;
		} else if (rsrc_data->wm_mode == CAM_SFE_WM_FRAME_BASED_MODE) {
			cam_sfe_bus_config_rdi_wm_frame_based_mode(rsrc_data);
		} else {
			CAM_WARN(CAM_SFE, "No index mode support for SFE WM: %u",
				rsrc_data->index);
		}
		break;
	case CAM_FORMAT_PLAIN8:
		if (rsrc_data->wm_mode == CAM_SFE_WM_LINE_BASED_MODE) {
			rsrc_data->en_cfg = 0x1;
			rsrc_data->stride = rsrc_data->width * 2;
		} else if (rsrc_data->wm_mode == CAM_SFE_WM_FRAME_BASED_MODE) {
			cam_sfe_bus_config_rdi_wm_frame_based_mode(rsrc_data);
		} else {
			CAM_WARN(CAM_SFE, "No index mode support for SFE WM: %u",
				rsrc_data->index);
		}
		break;
	case CAM_FORMAT_PLAIN16_10:
	case CAM_FORMAT_PLAIN16_12:
	case CAM_FORMAT_PLAIN16_14:
	case CAM_FORMAT_PLAIN16_16:
		if (rsrc_data->wm_mode == CAM_SFE_WM_LINE_BASED_MODE) {
			rsrc_data->width =
				ALIGNUP(rsrc_data->width * 2, 16) / 16;
			rsrc_data->en_cfg = 0x1;
		} else if (rsrc_data->wm_mode == CAM_SFE_WM_FRAME_BASED_MODE) {
			cam_sfe_bus_config_rdi_wm_frame_based_mode(rsrc_data);
		} else {
			CAM_WARN(CAM_SFE, "No index mode support for SFE WM: %u",
				rsrc_data->index);
		}
		break;
	case CAM_FORMAT_PLAIN64:
		if (rsrc_data->wm_mode == CAM_SFE_WM_LINE_BASED_MODE) {
			rsrc_data->width =
				ALIGNUP(rsrc_data->width * 8, 16) / 16;
			rsrc_data->en_cfg = 0x1;
		} else if (rsrc_data->wm_mode == CAM_SFE_WM_FRAME_BASED_MODE) {
			cam_sfe_bus_config_rdi_wm_frame_based_mode(rsrc_data);
		} else {
			CAM_WARN(CAM_SFE, "No index mode support for SFE WM: %u",
				rsrc_data->index);
		}
		break;
	default:
		CAM_ERR(CAM_SFE, "Unsupported RDI format %d",
			rsrc_data->format);
		return -EINVAL;
	}

	return 0;
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
	int32_t wm_idx = 0, rc;
	struct cam_sfe_bus_wr_wm_resource_data  *rsrc_data = NULL;
	char wm_mode[50] = {0};

	if (wm_res->res_state != CAM_ISP_RESOURCE_STATE_AVAILABLE) {
		CAM_ERR(CAM_SFE, "WM:%d not available state:%d",
			wm_idx, wm_res->res_state);
		return -EALREADY;
	}

	rsrc_data = wm_res->res_priv;
	wm_idx = rsrc_data->index;
	rsrc_data->format = out_port_info->format;
	rsrc_data->pack_fmt = cam_sfe_bus_get_packer_fmt(bus_priv,
		rsrc_data->format, wm_idx);

	rsrc_data->width = out_port_info->width;
	rsrc_data->height = out_port_info->height;
	rsrc_data->acquired_width = out_port_info->width;
	rsrc_data->acquired_height = out_port_info->height;
	rsrc_data->is_dual = is_dual;
	rsrc_data->enable_caching =  false;
	rsrc_data->offset = 0;

	/* RDI0-4 line based mode by default */
	if (sfe_out_res_id == CAM_SFE_BUS_SFE_OUT_RDI0 ||
		sfe_out_res_id == CAM_SFE_BUS_SFE_OUT_RDI1 ||
		sfe_out_res_id == CAM_SFE_BUS_SFE_OUT_RDI2 ||
		sfe_out_res_id == CAM_SFE_BUS_SFE_OUT_RDI3 ||
		sfe_out_res_id == CAM_SFE_BUS_SFE_OUT_RDI4)
		rsrc_data->wm_mode = CAM_SFE_WM_LINE_BASED_MODE;

	/* Set WM offset value to default */
	rsrc_data->offset  = 0;
	CAM_DBG(CAM_SFE, "WM:%d width %d height %d", rsrc_data->index,
		rsrc_data->width, rsrc_data->height);

	if ((sfe_out_res_id >= CAM_SFE_BUS_SFE_OUT_RDI0) &&
		(sfe_out_res_id <= CAM_SFE_BUS_SFE_OUT_RDI4)) {
		rc = cam_sfe_bus_config_rdi_wm(rsrc_data);
		if (rc)
			return rc;
	} else if (sfe_out_res_id == CAM_SFE_BUS_SFE_OUT_RAW_DUMP) {
		rsrc_data->stride = rsrc_data->width;
		rsrc_data->en_cfg = 0x1;
		switch (rsrc_data->format) {
		case CAM_FORMAT_PLAIN16_10:
		case CAM_FORMAT_PLAIN16_12:
		case CAM_FORMAT_PLAIN16_14:
		case CAM_FORMAT_PLAIN16_16:
			/* LSB aligned */
			rsrc_data->pack_fmt |=
				(1 << bus_priv->common_data.pack_align_shift);
			break;
		default:
			break;
		}
	} else if (sfe_out_res_id == CAM_SFE_BUS_SFE_OUT_IR) {
		rsrc_data->stride = rsrc_data->width;
		rsrc_data->en_cfg = 0x1;
		switch (rsrc_data->format) {
		case CAM_FORMAT_PLAIN16_10:
		case CAM_FORMAT_PLAIN16_12:
		case CAM_FORMAT_PLAIN16_14:
		case CAM_FORMAT_PLAIN16_16:
			/* LSB aligned */
			rsrc_data->pack_fmt |=
				(1 << bus_priv->common_data.pack_align_shift);
			break;
		default:
			break;
		}
	} else if ((sfe_out_res_id >= CAM_SFE_BUS_SFE_OUT_BE_0) &&
		(sfe_out_res_id <= CAM_SFE_BUS_SFE_OUT_BAYER_RS_2)) {
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
			rsrc_data->pack_fmt |=
				(1 << bus_priv->common_data.pack_align_shift);
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

	wm_res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;
	wm_res->tasklet_info = tasklet;

	CAM_DBG(CAM_SFE,
		"SFE:%d WM:%d %s processed width:%d height:%d format:0x%X pack_fmt 0x%x %s",
		rsrc_data->common_data->core_index, rsrc_data->index,
		wm_res->res_name, rsrc_data->width, rsrc_data->height,
		rsrc_data->format, rsrc_data->pack_fmt, wm_mode);
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
	const uint32_t enable_debug_status_1 = 11 << 8;
	struct cam_sfe_bus_wr_wm_resource_data   *rsrc_data =
		wm_res->res_priv;
	struct cam_sfe_bus_wr_common_data        *common_data =
		rsrc_data->common_data;

	cam_io_w((rsrc_data->height << image_cfg_height_shift_val)
		| rsrc_data->width, common_data->mem_base +
		rsrc_data->hw_regs->image_cfg_0);
	cam_io_w(rsrc_data->pack_fmt,
		common_data->mem_base + rsrc_data->hw_regs->packer_cfg);

	/* configure line_done_cfg for RDI0-2 */
	if ((rsrc_data->index >= 8) &&
		(rsrc_data->index <= 10)) {
		CAM_DBG(CAM_SFE, "configure line_done_cfg 0x%x for WM: %d",
			rsrc_data->common_data->line_done_cfg,
			rsrc_data->index);
		cam_io_w_mb(rsrc_data->common_data->line_done_cfg,
			common_data->mem_base +
			rsrc_data->hw_regs->line_done_cfg);
	}

	if (!(common_data->sfe_debug_cfg & SFE_DEBUG_DISABLE_MMU_PREFETCH)) {
		cam_io_w_mb(1, common_data->mem_base +
			rsrc_data->hw_regs->mmu_prefetch_cfg);
		cam_io_w_mb(0xFFFFFFFF, common_data->mem_base +
			rsrc_data->hw_regs->mmu_prefetch_max_offset);
		CAM_DBG(CAM_SFE, "SFE: %u WM: %u MMU prefetch enabled",
			rsrc_data->common_data->core_index,
			rsrc_data->index);
	}

	/* Enable WM */
	cam_io_w_mb(rsrc_data->en_cfg, common_data->mem_base +
		rsrc_data->hw_regs->cfg);

	/* Enable constraint error detection */
	cam_io_w_mb(enable_debug_status_1,
		common_data->mem_base +
		rsrc_data->hw_regs->debug_status_cfg);

	CAM_DBG(CAM_SFE,
		"Start SFE:%d WM:%d %s offset:0x%X en_cfg:0x%X width:%d height:%d",
		rsrc_data->common_data->core_index, rsrc_data->index,
		wm_res->res_name, (uint32_t) rsrc_data->hw_regs->cfg,
		rsrc_data->en_cfg, rsrc_data->width, rsrc_data->height);
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
	CAM_DBG(CAM_SFE, "Stop SFE:%d WM:%d %s",
		rsrc_data->common_data->core_index, rsrc_data->index,
		wm_res->res_name);

	wm_res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;
	rsrc_data->init_cfg_done = false;
	rsrc_data->hfr_cfg_done = false;
	rsrc_data->enable_caching =  false;
	rsrc_data->offset = 0;

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
	struct cam_isp_resource_node    *wm_res,
	uint8_t                         *wm_name)
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
	if (wm_name)
		scnprintf(wm_res->res_name, CAM_ISP_RES_NAME_LEN, "%s",
			wm_name);

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

		in_rsrc_data->dual_slave_core = CAM_SFE_CORE_MAX;
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

	/* CSID buf done register */
	bus_irq_reg_mask[0] =
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
	rsrc_data->dual_slave_core = CAM_SFE_CORE_MAX;

	list_add_tail(&comp_grp->list, &bus_priv->free_comp_grp);

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
	rsrc_data->bus_priv = bus_priv;

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

	bus_priv->tasklet_info = acq_args->tasklet;
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
		goto release_wm;
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

	if (!common_data->err_irq_subscribe) {
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
	if (common_data->err_irq_subscribe)
		list_add_tail(&(*evt_payload)->list,
			&common_data->free_payload_list);
	spin_unlock_irqrestore(&common_data->spin_lock, flags);

	*evt_payload = NULL;

	CAM_DBG(CAM_SFE, "Exit");
	return 0;
}

static int cam_sfe_bus_start_sfe_out(
	struct cam_isp_resource_node          *sfe_out)
{
	int rc = 0, i;
	struct cam_sfe_bus_wr_out_data      *rsrc_data = NULL;
	struct cam_sfe_bus_wr_priv          *bus_priv;
	struct cam_sfe_bus_wr_common_data   *common_data = NULL;
	uint32_t bus_irq_reg_mask[1];
	uint32_t source_group = 0;

	if (!sfe_out) {
		CAM_ERR(CAM_SFE, "Invalid input");
		return -EINVAL;
	}

	rsrc_data = sfe_out->res_priv;
	bus_priv = rsrc_data->bus_priv;
	common_data = rsrc_data->common_data;
	source_group = rsrc_data->source_group;

	if (sfe_out->res_state != CAM_ISP_RESOURCE_STATE_RESERVED) {
		CAM_ERR(CAM_SFE,
			"Invalid resource state:%d SFE:%d out_type:0x%X",
			sfe_out->res_state, rsrc_data->common_data->core_index,
			rsrc_data->out_type);
		return -EACCES;
	}

	/* subscribe when first out rsrc is streamed on */
	if (!bus_priv->common_data.err_irq_subscribe) {
		rc = cam_sfe_bus_subscribe_error_irq(bus_priv);
		if (rc)
			return rc;
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
		&tasklet_bh_api,
		CAM_IRQ_EVT_GROUP_0);
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
	struct cam_sfe_bus_wr_priv          *bus_priv;
	struct cam_sfe_bus_wr_common_data   *common_data = NULL;

	if (!sfe_out) {
		CAM_ERR(CAM_SFE, "Invalid input");
		return -EINVAL;
	}

	rsrc_data = sfe_out->res_priv;
	bus_priv = rsrc_data->bus_priv;
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

	if (sfe_out->irq_handle) {
		cam_irq_controller_unsubscribe_irq(
			common_data->buf_done_controller,
			sfe_out->irq_handle);
		sfe_out->irq_handle = 0;
	}

	/*
	 * Unsubscribe error irq when first
	 * out rsrc is streamed off
	 */
	if (bus_priv->common_data.err_irq_subscribe) {
		if (bus_priv->error_irq_handle) {
			rc = cam_irq_controller_unsubscribe_irq(
					bus_priv->common_data.bus_irq_controller,
					bus_priv->error_irq_handle);
			if (rc)
				CAM_WARN(CAM_SFE, "failed to unsubscribe error irqs");
			bus_priv->error_irq_handle = 0;
		}

		if (bus_priv->bus_irq_handle) {
			rc = cam_irq_controller_unsubscribe_irq(
					bus_priv->common_data.sfe_irq_controller,
					bus_priv->bus_irq_handle);
			if (rc)
				CAM_WARN(CAM_SFE, "failed to unsubscribe top irq");
			bus_priv->bus_irq_handle = 0;
			cam_irq_controller_unregister_dependent(
				bus_priv->common_data.sfe_irq_controller,
				bus_priv->common_data.bus_irq_controller);
		}

		bus_priv->common_data.err_irq_subscribe = false;
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

	CAM_DBG(CAM_SFE, "SFE:%d Bus IRQ status_0: 0x%X",
		rsrc_data->common_data->core_index,
		th_payload->evt_status_arr[0]);

	rc  = cam_sfe_bus_wr_get_evt_payload(rsrc_data->common_data,
			&evt_payload);
	if (rc) {
		CAM_INFO_RATE_LIMIT(CAM_SFE,
			"Failed to get payload for SFE:%d Bus IRQ status_0: 0x%X",
			rsrc_data->common_data->core_index,
			th_payload->evt_status_arr[0]);
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
	uint32_t                              *cam_sfe_irq_regs;
	uint32_t                               status_0;

	if (!evt_payload)
		return rc;

	if (rsrc_data->is_dual && (!rsrc_data->is_master)) {
		CAM_ERR(CAM_SFE, "Invalid comp_grp:%u is_master:%u",
			rsrc_data->comp_grp_type, rsrc_data->is_master);
		return rc;
	}

	cam_sfe_irq_regs = evt_payload->irq_reg_val;
	status_0 = cam_sfe_irq_regs[CAM_SFE_IRQ_BUS_REG_STATUS0];

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

static uint32_t cam_sfe_bus_get_last_consumed_addr(
	struct cam_sfe_bus_wr_priv *bus_priv,
	uint32_t res_type,
	uint32_t *val)
{
	struct cam_isp_resource_node             *rsrc_node = NULL;
	struct cam_sfe_bus_wr_out_data           *rsrc_data = NULL;
	struct cam_sfe_bus_wr_wm_resource_data   *wm_rsrc_data = NULL;
	enum cam_sfe_bus_sfe_out_type             res_id;

	res_id = cam_sfe_bus_wr_get_out_res_id(res_type);

	if (res_id >= CAM_SFE_BUS_SFE_OUT_MAX) {
		CAM_ERR(CAM_ISP, "invalid res id:%u", res_id);
		return 0;
	}

	rsrc_node = &bus_priv->sfe_out[res_id];
	rsrc_data = rsrc_node->res_priv;

	/* All SFE out ports have single WM */
	wm_rsrc_data = rsrc_data->wm_res->res_priv;
	*val = cam_io_r_mb(
		wm_rsrc_data->common_data->mem_base +
		wm_rsrc_data->hw_regs->addr_status_0);

	return 0;
}

static int cam_sfe_bus_handle_sfe_out_done_bottom_half(
	void                *handler_priv,
	void                *evt_payload_priv)
{
	int rc = -EINVAL, num_out = 0, i;
	uint32_t evt_id = 0, comp_mask = 0, val = 0;
	struct cam_isp_resource_node           *sfe_out = handler_priv;
	struct cam_sfe_bus_wr_out_data         *rsrc_data = sfe_out->res_priv;
	struct cam_sfe_bus_wr_irq_evt_payload  *evt_payload = evt_payload_priv;
	struct cam_isp_hw_event_info            evt_info;
	struct cam_isp_hw_compdone_event_info   compdone_evt_info = {0};
	void                                   *ctx = NULL;
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
		evt_info.hw_type  = CAM_ISP_HW_TYPE_SFE;

		cam_sfe_bus_get_comp_sfe_out_res_id_list(
			comp_mask, out_list, &num_out);
		if (num_out > CAM_NUM_OUT_PER_COMP_IRQ_MAX) {
			CAM_ERR(CAM_ISP,
				"num_out: %d  exceeds max_port_per_comp_grp: %d for comp_mask: %u",
				num_out, CAM_NUM_OUT_PER_COMP_IRQ_MAX, comp_mask);
			for (i = 0; i < num_out; i++)
				CAM_ERR(CAM_ISP,
					"Skipping buf done notify for outport: %u",
					out_list[i]);
			rc = -EINVAL;
			goto end;
		}

		compdone_evt_info.num_res = num_out;
		for (i = 0; i < num_out; i++) {
			compdone_evt_info.res_id[i] = out_list[i];
			cam_sfe_bus_get_last_consumed_addr(
				rsrc_data->bus_priv,
				compdone_evt_info.res_id[i], &val);
			compdone_evt_info.last_consumed_addr[i] = val;
		}
		evt_info.event_data = (void *)&compdone_evt_info;
		if (rsrc_data->common_data->event_cb)
			rsrc_data->common_data->event_cb(ctx, evt_id,
				(void *)&evt_info);
		break;
	default:
		break;
	}

end:
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

	/* All SFE clients have 1 WM hence i = 0 always */
	rc = cam_sfe_bus_init_wm_resource(
			hw_info->sfe_out_hw_info[index].wm_idx,
			bus_priv, hw_info,
			&rsrc_data->wm_res[i],
			hw_info->sfe_out_hw_info[index].name);
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
	sfe_out->process_cmd = NULL;
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

static inline void __cam_sfe_bus_wr_print_wm_info(
	struct cam_sfe_bus_wr_wm_resource_data  *wm_data)
{
	CAM_INFO(CAM_SFE,
		"SFE:%d WM:%d width:%u height:%u stride:%u x_init:%u en_cfg:%u acquired width:%u height:%u pack_cfg: 0x%x",
		wm_data->common_data->core_index, wm_data->index,
		wm_data->width, wm_data->height,
		wm_data->stride, wm_data->h_init,
		wm_data->en_cfg, wm_data->acquired_width,
		wm_data->acquired_height, wm_data->pack_fmt);
}

static int cam_sfe_bus_wr_print_dimensions(
	enum cam_sfe_bus_sfe_out_type        sfe_out_res_id,
	struct cam_sfe_bus_wr_priv          *bus_priv)
{
	struct cam_isp_resource_node            *rsrc_node = NULL;
	struct cam_isp_resource_node            *wm_res = NULL;
	struct cam_sfe_bus_wr_out_data          *rsrc_data = NULL;
	struct cam_sfe_bus_wr_wm_resource_data  *wm_data   = NULL;
	int                                      i, wm_idx;

	rsrc_node = &bus_priv->sfe_out[sfe_out_res_id];
	rsrc_data = rsrc_node->res_priv;
	for (i = 0; i < rsrc_data->num_wm; i++) {
		wm_res = &rsrc_data->wm_res[i];
		wm_data = (struct cam_sfe_bus_wr_wm_resource_data  *)
			wm_res->res_priv;
		wm_idx = wm_data->index;
		if (wm_idx < 0 || wm_idx >= bus_priv->num_client) {
			CAM_ERR(CAM_SFE, "Unsupported SFE out %d",
				sfe_out_res_id);
			return -EINVAL;
		}

		__cam_sfe_bus_wr_print_wm_info(wm_data);
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
		bus_priv->common_data.bus_irq_controller, CAM_IRQ_EVT_GROUP_0);
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

static void cam_sfe_bus_wr_print_violation_info(
	uint32_t status, struct cam_sfe_bus_wr_priv *bus_priv)
{
	int i, j, wm_idx;
	struct cam_isp_resource_node           *sfe_out = NULL;
	struct cam_isp_resource_node           *wm_res = NULL;
	struct cam_sfe_bus_wr_out_data         *rsrc_data = NULL;
	struct cam_sfe_bus_wr_wm_resource_data *wm_data = NULL;

	for (i = 0; i < bus_priv->num_out; i++) {
		sfe_out = &bus_priv->sfe_out[i];
		rsrc_data = (struct cam_sfe_bus_wr_out_data *)
			sfe_out->res_priv;

		for (j = 0; j < rsrc_data->num_wm; j++) {
			wm_res = &rsrc_data->wm_res[j];
			wm_data = (struct cam_sfe_bus_wr_wm_resource_data  *)
				wm_res->res_priv;
			wm_idx = wm_data->index;
			if (wm_idx < 0 || wm_idx >= bus_priv->num_client) {
				CAM_ERR(CAM_SFE, "Unsupported SFE out %d",
					wm_idx);
				return;
			}

			if (status & (1 << wm_idx))
				__cam_sfe_bus_wr_print_wm_info(wm_data);
		}
	}
}

static int cam_sfe_bus_wr_irq_bottom_half(
	void *handler_priv, void *evt_payload_priv)
{
	int i;
	uint32_t status = 0, cons_violation = 0;
	bool skip_err_notify = false;
	struct cam_sfe_bus_wr_priv            *bus_priv = handler_priv;
	struct cam_sfe_bus_wr_common_data     *common_data;
	struct cam_isp_hw_event_info           evt_info;
	struct cam_sfe_bus_wr_irq_evt_payload *evt_payload = evt_payload_priv;

	if (!handler_priv || !evt_payload_priv)
		return -EINVAL;

	common_data = &bus_priv->common_data;

	status = evt_payload->irq_reg_val[CAM_SFE_IRQ_BUS_REG_STATUS0];
	cons_violation = (status >> 28) & 0x1;

	CAM_ERR(CAM_SFE,
		"SFE:%d status0 0x%x Image Size violation status 0x%x CCIF violation status 0x%x",
		bus_priv->common_data.core_index, status,
		evt_payload->image_size_violation_status,
		evt_payload->ccif_violation_status);

	if (evt_payload->image_size_violation_status)
		cam_sfe_bus_wr_print_violation_info(
			evt_payload->image_size_violation_status,
			bus_priv);

	if (cons_violation)
		cam_sfe_bus_wr_get_constraint_errors(&skip_err_notify, bus_priv);

	cam_sfe_bus_wr_put_evt_payload(common_data, &evt_payload);

	if (!skip_err_notify) {
		struct cam_isp_hw_error_event_info err_evt_info;

		evt_info.hw_idx = common_data->core_index;
		evt_info.hw_type = CAM_ISP_HW_TYPE_SFE;
		evt_info.res_type = CAM_ISP_RESOURCE_SFE_OUT;
		evt_info.res_id = CAM_SFE_BUS_SFE_OUT_MAX;
		err_evt_info.err_type = CAM_SFE_IRQ_STATUS_VIOLATION;
		evt_info.event_data = (void *)&err_evt_info;

		if (common_data->event_cb) {
			struct cam_isp_resource_node      *out_rsrc_node = NULL;
			struct cam_sfe_bus_wr_out_data    *out_rsrc_data = NULL;

			for (i = 0; i < bus_priv->num_out; i++) {
				out_rsrc_node = &bus_priv->sfe_out[i];

				if (!out_rsrc_node || !out_rsrc_node->res_priv)
					continue;

				if (out_rsrc_node->res_state != CAM_ISP_RESOURCE_STATE_STREAMING)
					continue;

				out_rsrc_data = out_rsrc_node->res_priv;
				common_data->event_cb(out_rsrc_data->priv,
					CAM_ISP_HW_EVENT_ERROR, (void *)&evt_info);
				break;
			}
		}
	}

	return 0;
}

static int cam_sfe_bus_subscribe_error_irq(
	struct cam_sfe_bus_wr_priv          *bus_priv)
{
	uint32_t top_irq_reg_mask[CAM_SFE_IRQ_REGISTERS_MAX] = {0};

	/* Subscribe top IRQ */
	top_irq_reg_mask[0] = (1 << bus_priv->top_irq_shift);

	bus_priv->bus_irq_handle = cam_irq_controller_subscribe_irq(
		bus_priv->common_data.sfe_irq_controller,
		CAM_IRQ_PRIORITY_0,
		top_irq_reg_mask,
		bus_priv,
		cam_sfe_bus_wr_handle_bus_irq,
		NULL,
		NULL,
		NULL,
		CAM_IRQ_EVT_GROUP_0);

	if (bus_priv->bus_irq_handle < 1) {
		CAM_ERR(CAM_SFE, "Failed to subscribe BUS TOP IRQ");
		bus_priv->bus_irq_handle = 0;
		return -EFAULT;
	}

	cam_irq_controller_register_dependent(bus_priv->common_data.sfe_irq_controller,
		bus_priv->common_data.bus_irq_controller, top_irq_reg_mask);

	if (bus_priv->tasklet_info != NULL) {
		bus_priv->error_irq_handle = cam_irq_controller_subscribe_irq(
			bus_priv->common_data.bus_irq_controller,
			CAM_IRQ_PRIORITY_0,
			bus_wr_error_irq_mask,
			bus_priv,
			cam_sfe_bus_wr_err_irq_top_half,
			cam_sfe_bus_wr_irq_bottom_half,
			bus_priv->tasklet_info,
			&tasklet_bh_api,
			CAM_IRQ_EVT_GROUP_0);

		if (bus_priv->error_irq_handle < 1) {
			CAM_ERR(CAM_SFE, "Failed to subscribe BUS Error IRQ");
			bus_priv->error_irq_handle = 0;
			return -EFAULT;
		}
	}

	bus_priv->common_data.err_irq_subscribe = true;
	CAM_DBG(CAM_SFE, "BUS WR error irq subscribed");
	return 0;
}

static int cam_sfe_bus_wr_update_wm(void *priv, void *cmd_args,
	uint32_t arg_size)
{
	struct cam_sfe_bus_wr_priv             *bus_priv;
	struct cam_isp_hw_get_cmd_update       *update_buf;
	struct cam_buf_io_cfg                  *io_cfg = NULL;
	struct cam_sfe_bus_wr_out_data         *sfe_out_data = NULL;
	struct cam_cdm_utils_ops               *cdm_util_ops;
	struct cam_sfe_bus_wr_wm_resource_data *wm_data = NULL;
	struct cam_sfe_bus_cache_dbg_cfg       *cache_dbg_cfg = NULL;
	uint32_t *reg_val_pair;
	uint32_t img_addr = 0, img_offset = 0;
	uint32_t num_regval_pairs = 0;
	uint32_t i, j, k, size = 0;
	uint32_t frame_inc = 0, val;
	uint32_t loop_size = 0, stride = 0, slice_h = 0;

	bus_priv = (struct cam_sfe_bus_wr_priv *) priv;
	update_buf = (struct cam_isp_hw_get_cmd_update *) cmd_args;
	cache_dbg_cfg = &bus_priv->common_data.cache_dbg_cfg;

	sfe_out_data = (struct cam_sfe_bus_wr_out_data *)
		update_buf->res->res_priv;
	if (!sfe_out_data || !sfe_out_data->cdm_util_ops) {
		CAM_ERR(CAM_SFE, "Invalid data");
		return -EINVAL;
	}

	cdm_util_ops = sfe_out_data->cdm_util_ops;
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
		CAM_DBG(CAM_SFE, "WM:%d %s en_cfg 0x%X",
			wm_data->index, sfe_out_data->wm_res[i].res_name,
			reg_val_pair[j-1]);
		CAM_SFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
			wm_data->hw_regs->packer_cfg, wm_data->pack_fmt);
		CAM_DBG(CAM_SFE, "WM:%d %s packer_fmt 0x%X",
			wm_data->index, sfe_out_data->wm_res[i].res_name,
			reg_val_pair[j-1]);

		wm_data->cache_cfg = 0;
		if (wm_data->enable_caching) {
			if ((cache_dbg_cfg->disable_for_scratch) &&
				(update_buf->use_scratch_cfg))
				goto skip_cache_cfg;

			if ((cache_dbg_cfg->disable_for_buf) &&
				(!update_buf->use_scratch_cfg))
				goto skip_cache_cfg;

			wm_data->cache_cfg =
				wm_data->current_scid << 8;
			wm_data->cache_cfg |= 3 << 4;
			if ((update_buf->use_scratch_cfg) &&
				(cache_dbg_cfg->scratch_dbg_cfg))
				wm_data->cache_cfg |= cache_dbg_cfg->scratch_alloc;
			else if ((!update_buf->use_scratch_cfg) &&
				(cache_dbg_cfg->buf_dbg_cfg))
				wm_data->cache_cfg |= cache_dbg_cfg->buf_alloc;
			else
				wm_data->cache_cfg |= CACHE_ALLOC_ALLOC;
		}

skip_cache_cfg:

		CAM_SFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
			wm_data->hw_regs->system_cache_cfg,
			wm_data->cache_cfg);
		CAM_DBG(CAM_SFE, "WM:%d cache_cfg:0x%x",
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
		if (val != stride)
			CAM_DBG(CAM_SFE, "Warning stride %u expected %u",
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
			img_addr = (update_buf->wm_update->image_buf[i] +
				wm_data->offset + k * frame_inc);
			update_buf->wm_update->image_buf_offset[i] =
				wm_data->offset;

			if (cam_smmu_is_expanded_memory()) {
				img_offset = CAM_36BIT_INTF_GET_IOVA_OFFSET(img_addr);
				img_addr = CAM_36BIT_INTF_GET_IOVA_BASE(img_addr);

				/* Only write to offset register in 36-bit enabled HW */
				CAM_SFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
					wm_data->hw_regs->addr_cfg, img_offset);
				CAM_DBG(CAM_SFE, "WM:%d image offset 0x%X",
					wm_data->index, img_offset);
			}
			CAM_SFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
				wm_data->hw_regs->image_addr, img_addr);

			CAM_DBG(CAM_SFE, "WM:%d image address 0x%X",
				wm_data->index, img_addr);
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

	num_regval_pairs = j / 2;

	if (num_regval_pairs) {
		size = cdm_util_ops->cdm_required_size_reg_random(
			num_regval_pairs);

		/* cdm util returns dwords, need to convert to bytes */
		if ((size * 4) > update_buf->cmd.size) {
			CAM_ERR(CAM_SFE,
				"Failed! Buf size:%d insufficient, expected size:%d",
				update_buf->cmd.size, size);
			return -ENOMEM;
		}

		cdm_util_ops->cdm_write_regrandom(
			update_buf->cmd.cmd_buf_addr, num_regval_pairs,
			reg_val_pair);

		/* cdm util returns dwords, need to convert to bytes */
		update_buf->cmd.used_bytes = size * 4;
	} else {
		CAM_DBG(CAM_SFE,
			"No reg val pairs. num_wms: %u",
			sfe_out_data->num_wm);
		update_buf->cmd.used_bytes = 0;
	}

	return 0;
}

/*
 * API similar to cam_sfe_bus_wr_update_wm() with the
 * only change being config is done via AHB instead of CDM
 */
static int cam_sfe_bus_wr_config_wm(void *priv, void *cmd_args,
	uint32_t arg_size)
{
	struct cam_sfe_bus_wr_priv             *bus_priv;
	struct cam_isp_hw_get_cmd_update       *update_buf;
	struct cam_sfe_bus_wr_out_data         *sfe_out_data = NULL;
	struct cam_sfe_bus_wr_wm_resource_data *wm_data = NULL;
	struct cam_sfe_bus_cache_dbg_cfg       *cache_dbg_cfg = NULL;
	uint32_t i, k;
	uint32_t frame_inc = 0, val, img_addr = 0, img_offset = 0;
	uint32_t loop_size = 0, stride = 0, slice_h = 0;

	bus_priv = (struct cam_sfe_bus_wr_priv  *) priv;
	update_buf =  (struct cam_isp_hw_get_cmd_update *) cmd_args;
	cache_dbg_cfg = &bus_priv->common_data.cache_dbg_cfg;

	sfe_out_data = (struct cam_sfe_bus_wr_out_data *)
		update_buf->res->res_priv;

	if (!sfe_out_data) {
		CAM_ERR(CAM_SFE, "Invalid data");
		return -EINVAL;
	}

	if (update_buf->wm_update->num_buf != sfe_out_data->num_wm) {
		CAM_ERR(CAM_SFE,
			"Failed! Invalid number buffers:%d required:%d",
			update_buf->wm_update->num_buf, sfe_out_data->num_wm);
		return -EINVAL;
	}

	for (i = 0; i < sfe_out_data->num_wm; i++) {
		wm_data = (struct cam_sfe_bus_wr_wm_resource_data *)
			sfe_out_data->wm_res[i].res_priv;

		val = (wm_data->height << 16) | wm_data->width;
		cam_io_w_mb(val,
			wm_data->common_data->mem_base +
			wm_data->hw_regs->image_cfg_0);
		CAM_DBG(CAM_SFE, "WM:%d image height and width 0x%X",
			wm_data->index, val);

		/* For initial configuration program all bus registers */
		stride = update_buf->wm_update->stride;
		slice_h = update_buf->wm_update->slice_height;

		val = stride;
		CAM_DBG(CAM_SFE, "before stride %d", val);
		val = ALIGNUP(val, 16);
		if (val != stride &&
			val != wm_data->stride)
			CAM_WARN(CAM_SFE, "Warning stride %u expected %u",
				stride, val);

		if (wm_data->stride != val || !wm_data->init_cfg_done) {
			cam_io_w_mb(stride,
				wm_data->common_data->mem_base +
				wm_data->hw_regs->image_cfg_2);
			wm_data->stride = val;
			CAM_DBG(CAM_SFE, "WM:%d image stride 0x%X",
				wm_data->index, val);
		}

		frame_inc = stride * slice_h;

		if (!(wm_data->en_cfg & (0x3 << 16))) {
			cam_io_w_mb(wm_data->h_init,
				wm_data->common_data->mem_base +
				wm_data->hw_regs->image_cfg_1);
			CAM_DBG(CAM_SFE, "WM:%d h_init 0x%X",
				wm_data->index, wm_data->h_init);
		}

		if (wm_data->index > 7)
			loop_size = wm_data->irq_subsample_period + 1;
		else
			loop_size = 1;

		/* WM Image address */
		for (k = 0; k < loop_size; k++) {
			img_addr = update_buf->wm_update->image_buf[i] +
				wm_data->offset + k * frame_inc;

			if (cam_smmu_is_expanded_memory()) {
				img_offset = CAM_36BIT_INTF_GET_IOVA_OFFSET(img_addr);
				img_addr = CAM_36BIT_INTF_GET_IOVA_BASE(img_addr);

				CAM_DBG(CAM_SFE, "WM:%d image address offset: 0x%x",
					wm_data->index, img_offset);
				cam_io_w_mb(img_offset,
					wm_data->common_data->mem_base + wm_data->hw_regs->addr_cfg);
			}

			CAM_DBG(CAM_SFE, "WM:%d image address: 0x%x, offset: 0x%x",
				wm_data->index, img_addr, wm_data->offset);
			cam_io_w_mb(img_addr,
				wm_data->common_data->mem_base + wm_data->hw_regs->image_addr);
		}

		cam_io_w_mb(frame_inc,
				wm_data->common_data->mem_base +
				wm_data->hw_regs->frame_incr);
		CAM_DBG(CAM_SFE, "WM:%d frame_inc %d",
			wm_data->index, frame_inc);

		wm_data->cache_cfg = 0;
		if ((!cache_dbg_cfg->disable_for_scratch) &&
			(wm_data->enable_caching)) {
			wm_data->cache_cfg =
				wm_data->current_scid << 8;
			wm_data->cache_cfg |= 3 << 4;
			if (cache_dbg_cfg->scratch_dbg_cfg)
				wm_data->cache_cfg |= cache_dbg_cfg->scratch_alloc;
			else
				wm_data->cache_cfg |= CACHE_ALLOC_ALLOC;
		}

		cam_io_w_mb(wm_data->cache_cfg,
			wm_data->common_data->mem_base +
			wm_data->hw_regs->system_cache_cfg);
		CAM_DBG(CAM_SFE, "WM:%d cache_cfg:0x%x",
			wm_data->index, wm_data->cache_cfg);

		cam_io_w_mb(wm_data->pack_fmt,
			wm_data->common_data->mem_base +
			wm_data->hw_regs->packer_cfg);
		CAM_DBG(CAM_SFE, "WM:%d packer_cfg 0x%X",
			wm_data->index, wm_data->pack_fmt);

		/* enable the WM */
		cam_io_w_mb(wm_data->en_cfg,
			wm_data->common_data->mem_base +
			wm_data->hw_regs->cfg);
		CAM_DBG(CAM_SFE, "WM:%d en_cfg 0x%X",
			wm_data->index, wm_data->en_cfg);

		/* set initial configuration done */
		if (!wm_data->init_cfg_done)
			wm_data->init_cfg_done = true;
	}

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
	struct cam_cdm_utils_ops               *cdm_util_ops;
	uint32_t *reg_val_pair;
	uint32_t num_regval_pairs = 0;
	uint32_t  i, j, size = 0;

	bus_priv = (struct cam_sfe_bus_wr_priv  *) priv;
	update_hfr =  (struct cam_isp_hw_get_cmd_update *) cmd_args;

	sfe_out_data = (struct cam_sfe_bus_wr_out_data *)
		update_hfr->res->res_priv;

	if (!sfe_out_data || !sfe_out_data->cdm_util_ops) {
		CAM_ERR(CAM_SFE, "Invalid data");
		return -EINVAL;
	}

	cdm_util_ops = sfe_out_data->cdm_util_ops;
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

	num_regval_pairs = j / 2;

	if (num_regval_pairs) {
		size = cdm_util_ops->cdm_required_size_reg_random(
			num_regval_pairs);

		/* cdm util returns dwords, need to convert to bytes */
		if ((size * 4) > update_hfr->cmd.size) {
			CAM_ERR(CAM_SFE,
				"Failed! Buf size:%d insufficient, expected size:%d",
				update_hfr->cmd.size, size);
			return -ENOMEM;
		}

		cdm_util_ops->cdm_write_regrandom(
			update_hfr->cmd.cmd_buf_addr, num_regval_pairs,
			reg_val_pair);

		/* cdm util returns dwords, need to convert to bytes */
		update_hfr->cmd.used_bytes = size * 4;
	} else {
		CAM_DBG(CAM_SFE,
			"No reg val pairs. num_wms: %u",
			sfe_out_data->num_wm);
		update_hfr->cmd.used_bytes = 0;
	}

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
	enum cam_sfe_bus_wr_packer_format            packer_fmt = PACKER_FMT_MAX;
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

		if (wm_config->wm_mode > CAM_SFE_WM_INDEX_BASED_MODE) {
			CAM_ERR(CAM_SFE, "Invalid wm_mode: 0x%X WM:%d",
				wm_config->wm_mode, wm_data->index);
			return -EINVAL;
		}

		wm_data->en_cfg = (wm_config->wm_mode << 16) |
			(wm_config->virtual_frame_en << 1) | 0x1;
		wm_data->width  = wm_config->width;

		if (wm_config->packer_format) {
			packer_fmt = cam_sfe_bus_get_packer_fmt(sfe_out_data->bus_priv,
				wm_config->packer_format, wm_data->index);

			/* Reconfigure only for valid packer fmt */
			if (packer_fmt != PACKER_FMT_MAX) {
				switch (wm_config->packer_format) {
				case CAM_FORMAT_PLAIN16_10:
				case CAM_FORMAT_PLAIN16_12:
				case CAM_FORMAT_PLAIN16_14:
				case CAM_FORMAT_PLAIN16_16:
					packer_fmt |=
						(1 << wm_data->common_data->pack_align_shift);
					break;
				default:
					break;
				}
				wm_data->pack_fmt = packer_fmt;
			}
		}

		if ((sfe_out_data->out_type >= CAM_SFE_BUS_SFE_OUT_RDI0) &&
			(sfe_out_data->out_type <= CAM_SFE_BUS_SFE_OUT_RDI4)) {
			wm_data->wm_mode = wm_config->wm_mode;

			/*
			 * Update width based on format for line based mode only
			 * Image size ignored for frame based mode
			 * Index based not supported currently
			 */
			if (wm_data->wm_mode == CAM_SFE_WM_LINE_BASED_MODE)
				cam_sfe_bus_config_rdi_wm(wm_data);
		}

		if (i == PLANE_C)
			wm_data->height = wm_config->height / 2;
		else
			wm_data->height = wm_config->height;

		wm_data->offset = wm_config->offset;
		CAM_DBG(CAM_SFE,
			"WM:%d en_cfg:0x%X height:%d width:%d offset:%u packer_fmt: 0x%x",
			wm_data->index, wm_data->en_cfg, wm_data->height,
			wm_data->width, wm_data->offset, wm_data->pack_fmt);
	}

	return 0;
}

static int cam_sfe_bus_wr_update_bw_limiter(
	void *priv, void *cmd_args, uint32_t arg_size)
{
	struct cam_sfe_bus_wr_priv             *bus_priv;
	struct cam_isp_hw_get_cmd_update       *wm_config_update;
	struct cam_sfe_bus_wr_out_data         *sfe_out_data = NULL;
	struct cam_cdm_utils_ops               *cdm_util_ops;
	struct cam_sfe_bus_wr_wm_resource_data *wm_data = NULL;
	struct cam_isp_wm_bw_limiter_config    *wm_bw_limit_cfg = NULL;
	uint32_t                                counter_limit = 0, reg_val = 0;
	uint32_t                               *reg_val_pair, num_regval_pairs = 0;
	uint32_t                                i, j, size = 0;

	bus_priv         = (struct cam_sfe_bus_wr_priv  *) priv;
	wm_config_update = (struct cam_isp_hw_get_cmd_update *) cmd_args;
	wm_bw_limit_cfg  = (struct cam_isp_wm_bw_limiter_config  *)
			wm_config_update->data;

	sfe_out_data = (struct cam_sfe_bus_wr_out_data *)
		wm_config_update->res->res_priv;
	if (!sfe_out_data || !sfe_out_data->cdm_util_ops) {
		CAM_ERR(CAM_SFE, "Invalid data");
		return -EINVAL;
	}

	cdm_util_ops = sfe_out_data->cdm_util_ops;
	reg_val_pair = &sfe_out_data->common_data->io_buf_update[0];
	for (i = 0, j = 0; i < sfe_out_data->num_wm; i++) {
		if (j >= (MAX_REG_VAL_PAIR_SIZE - (MAX_BUF_UPDATE_REG_NUM * 2))) {
			CAM_ERR(CAM_SFE,
				"reg_val_pair %d exceeds the array limit %zu for WM idx %d",
				j, MAX_REG_VAL_PAIR_SIZE, i);
			return -ENOMEM;
		}

		/* Num WMs needs to match max planes */
		if (i >= CAM_PACKET_MAX_PLANES) {
			CAM_WARN(CAM_SFE,
				"Num of WMs: %d exceeded max planes", i);
			goto add_reg_pair;
		}

		wm_data = (struct cam_sfe_bus_wr_wm_resource_data *)
			sfe_out_data->wm_res[i].res_priv;
		if (!wm_data->hw_regs->bw_limiter_addr) {
			CAM_ERR(CAM_SFE,
				"WM: %d %s has no support for bw limiter",
				wm_data->index, sfe_out_data->wm_res[i].res_name);
			return -EINVAL;
		}

		counter_limit = wm_bw_limit_cfg->counter_limit[i];

		/* Validate max counter limit */
		if (counter_limit >
			wm_data->common_data->max_bw_counter_limit) {
			CAM_WARN(CAM_SFE,
				"Invalid counter limit: 0x%x capping to max: 0x%x",
				wm_bw_limit_cfg->counter_limit[i],
				wm_data->common_data->max_bw_counter_limit);
			counter_limit = wm_data->common_data->max_bw_counter_limit;
		}

		if (wm_bw_limit_cfg->enable_limiter && counter_limit) {
			reg_val = 1;
			reg_val |= (counter_limit << 1);
		} else {
			reg_val = 0;
		}

		CAM_SFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
			wm_data->hw_regs->bw_limiter_addr, reg_val);
		CAM_DBG(CAM_SFE, "WM: %d %s bw_limter: 0x%x",
			wm_data->index, sfe_out_data->wm_res[i].res_name,
			reg_val_pair[j-1]);
	}

add_reg_pair:

	num_regval_pairs = j / 2;

	if (num_regval_pairs) {
		size = cdm_util_ops->cdm_required_size_reg_random(
			num_regval_pairs);

		/* cdm util returns dwords, need to convert to bytes */
		if ((size * 4) > wm_config_update->cmd.size) {
			CAM_ERR(CAM_SFE,
				"Failed! Buf size:%d insufficient, expected size:%d",
				wm_config_update->cmd.size, size);
			return -ENOMEM;
		}

		cdm_util_ops->cdm_write_regrandom(
			wm_config_update->cmd.cmd_buf_addr, num_regval_pairs,
			reg_val_pair);

		/* cdm util returns dwords, need to convert to bytes */
		wm_config_update->cmd.used_bytes = size * 4;
	} else {
		CAM_DBG(CAM_SFE,
			"No reg val pairs. num_wms: %u",
			sfe_out_data->num_wm);
		wm_config_update->cmd.used_bytes = 0;
	}

	return 0;
}

static int cam_sfe_bus_wr_get_res_for_mid(
	struct cam_sfe_bus_wr_priv *bus_priv,
	void *cmd_args, uint32_t arg_size)
{
	struct cam_isp_hw_get_cmd_update       *cmd_update = cmd_args;
	struct cam_isp_hw_get_res_for_mid       *get_res = NULL;
	int i, j;

	get_res = (struct cam_isp_hw_get_res_for_mid *)cmd_update->data;
	if (!get_res) {
		CAM_ERR(CAM_SFE,
			"invalid get resource for mid paramas");
		return -EINVAL;
	}

	for (i = 0; i < bus_priv->num_out; i++) {

		for (j = 0; j < CAM_SFE_BUS_MAX_MID_PER_PORT; j++) {
			if (bus_priv->sfe_out_hw_info[i].mid[j] == get_res->mid)
				goto end;
		}
	}
	/*
	 * Do not update out_res_id in case of no match.
	 * Correct value will be dumped in hw mgr
	 */
	if (i == bus_priv->num_out) {
		CAM_INFO(CAM_SFE, "mid:%d does not match with any out resource", get_res->mid);
		return 0;
	}

end:
	CAM_INFO(CAM_SFE, "match mid :%d  out resource: %s 0x%x found",
		get_res->mid, bus_priv->sfe_out_hw_info[i].name,
		bus_priv->sfe_out[i].res_id);
	get_res->out_res_id = bus_priv->sfe_out[i].res_id;
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
	return 0;
}

static int cam_sfe_bus_wr_deinit_hw(void *hw_priv,
	void *deinit_hw_args, uint32_t arg_size)
{
	return 0;
}

static int cam_sfe_bus_wr_cache_config(
	void *priv, void *cmd_args,
	uint32_t arg_size)
{
	int i;
	struct cam_sfe_bus_wr_priv              *bus_priv;
	struct cam_isp_sfe_bus_sys_cache_config *cache_cfg;
	struct cam_sfe_bus_wr_out_data          *sfe_out_data = NULL;
	struct cam_sfe_bus_wr_wm_resource_data  *wm_data = NULL;


	bus_priv = (struct cam_sfe_bus_wr_priv  *)priv;
	cache_cfg = (struct cam_isp_sfe_bus_sys_cache_config *)cmd_args;

	sfe_out_data = (struct cam_sfe_bus_wr_out_data *)
		cache_cfg->res->res_priv;

	if (!sfe_out_data) {
		CAM_ERR(CAM_SFE, "Invalid data");
		return -EINVAL;
	}

	if (bus_priv->common_data.cache_dbg_cfg.disable_all)
		return 0;

	for (i = 0; i < sfe_out_data->num_wm; i++) {
		wm_data = (struct cam_sfe_bus_wr_wm_resource_data *)
			sfe_out_data->wm_res[i].res_priv;
		wm_data->enable_caching = cache_cfg->use_cache;
		wm_data->current_scid = cache_cfg->scid;
		cache_cfg->wr_cfg_done = true;

		CAM_DBG(CAM_SFE, "SFE:%d WM:%d cache_enable:%s scid:%u",
			wm_data->common_data->core_index,
			wm_data->index,
			(wm_data->enable_caching ? "true" : "false"),
			wm_data->current_scid);
	}

	return 0;
}

static int cam_sfe_bus_wr_set_debug_cfg(
	void *priv, void *cmd_args)
{
	struct cam_sfe_bus_wr_priv *bus_priv =
		(struct cam_sfe_bus_wr_priv  *) priv;
	struct cam_sfe_debug_cfg_params *debug_cfg;

	debug_cfg = (struct cam_sfe_debug_cfg_params *)cmd_args;

	if (debug_cfg->cache_config)
		cam_sfe_bus_parse_cache_cfg(false,
			debug_cfg->u.cache_cfg.sfe_cache_dbg,
			&bus_priv->common_data.cache_dbg_cfg);
	else
		bus_priv->common_data.sfe_debug_cfg =
			debug_cfg->u.dbg_cfg.sfe_debug_cfg;

	return 0;
}

static int cam_sfe_bus_wr_process_cmd(
	void *priv, uint32_t cmd_type,
	void *cmd_args, uint32_t arg_size)
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
	case CAM_ISP_HW_CMD_BUF_UPDATE:
		rc = cam_sfe_bus_wr_config_wm(priv, cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_GET_HFR_UPDATE:
		rc = cam_sfe_bus_wr_update_hfr(priv, cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_GET_WM_SECURE_MODE:
		rc = cam_sfe_bus_wr_get_secure_mode(priv, cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_STRIPE_UPDATE:
		rc = cam_sfe_bus_wr_update_stripe_cfg(priv,
			cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_STOP_BUS_ERR_IRQ:
		rc = 0;
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
	case CAM_ISP_HW_CMD_QUERY_BUS_CAP: {
		struct cam_isp_hw_bus_cap *sfe_bus_cap;

		bus_priv = (struct cam_sfe_bus_wr_priv  *) priv;
		sfe_bus_cap = (struct cam_isp_hw_bus_cap *) cmd_args;
		sfe_bus_cap->max_out_res_type = bus_priv->num_out;
		rc = 0;
	}
		break;
	case CAM_ISP_HW_SFE_SYS_CACHE_WM_CONFIG:
		rc = cam_sfe_bus_wr_cache_config(priv, cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_SET_SFE_DEBUG_CFG:
		rc = cam_sfe_bus_wr_set_debug_cfg(priv, cmd_args);
		break;
	case CAM_ISP_HW_CMD_WM_BW_LIMIT_CONFIG:
		rc = cam_sfe_bus_wr_update_bw_limiter(priv, cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_GET_RES_FOR_MID:
		rc = cam_sfe_bus_wr_get_res_for_mid(priv, cmd_args, arg_size);
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
	struct cam_sfe_soc_private    *soc_private;
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

	soc_private = soc_info->soc_private;
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

	bus_priv->num_client                       = hw_info->num_client;
	bus_priv->num_out                          = hw_info->num_out;
	bus_priv->num_comp_grp                     = hw_info->num_comp_grp;
	bus_priv->top_irq_shift                    = hw_info->top_irq_shift;
	bus_priv->common_data.num_sec_out          = 0;
	bus_priv->common_data.secure_mode          = CAM_SECURE_MODE_NON_SECURE;
	bus_priv->common_data.core_index           = soc_info->index;
	bus_priv->common_data.mem_base             =
		CAM_SOC_GET_REG_MAP_START(soc_info, SFE_CORE_BASE_IDX);
	bus_priv->common_data.hw_intf              = hw_intf;
	bus_priv->common_data.common_reg           = &hw_info->common_reg;
	bus_priv->common_data.comp_done_shift      = hw_info->comp_done_shift;
	bus_priv->common_data.line_done_cfg        = hw_info->line_done_cfg;
	bus_priv->common_data.pack_align_shift     = hw_info->pack_align_shift;
	bus_priv->common_data.max_bw_counter_limit = hw_info->max_bw_counter_limit;
	bus_priv->common_data.err_irq_subscribe    = false;
	bus_priv->common_data.sfe_irq_controller   = sfe_irq_controller;
	bus_priv->num_cons_err = hw_info->num_cons_err;
	bus_priv->constraint_error_list = hw_info->constraint_error_list;
	bus_priv->sfe_out_hw_info = hw_info->sfe_out_hw_info;
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
		&bus_priv->common_data.bus_irq_controller);
	if (rc) {
		CAM_ERR(CAM_SFE, "Init bus_irq_controller failed");
		goto free_sfe_out;
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
	sfe_bus_local->hw_ops.process_cmd  = cam_sfe_bus_wr_process_cmd;
	bus_priv->bus_irq_handle = 0;
	bus_priv->common_data.sfe_debug_cfg = 0;
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

free_sfe_out:
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
	bus_priv->common_data.err_irq_subscribe = false;
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
