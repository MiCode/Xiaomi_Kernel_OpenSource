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

#include <linux/slab.h>
#include <uapi/media/cam_isp.h>
#include "cam_io_util.h"
#include "cam_isp_hw_mgr_intf.h"
#include "cam_isp_hw.h"
#include "cam_vfe_hw_intf.h"
#include "cam_vfe_top.h"
#include "cam_vfe_top_ver2.h"
#include "cam_vfe_camif_ver2.h"
#include "cam_debug_util.h"
#include "cam_cdm_util.h"

struct cam_vfe_mux_camif_data {
	void __iomem                                *mem_base;
	struct cam_hw_intf                          *hw_intf;
	struct cam_vfe_camif_ver2_reg               *camif_reg;
	struct cam_vfe_top_ver2_reg_offset_common   *common_reg;
	struct cam_vfe_camif_reg_data               *reg_data;

	enum cam_isp_hw_sync_mode          sync_mode;
	uint32_t                           pix_pattern;
	uint32_t                           first_pixel;
	uint32_t                           first_line;
	uint32_t                           last_pixel;
	uint32_t                           last_line;
};

static int cam_vfe_camif_validate_pix_pattern(uint32_t pattern)
{
	int rc;

	switch (pattern) {
	case CAM_ISP_PATTERN_BAYER_RGRGRG:
	case CAM_ISP_PATTERN_BAYER_GRGRGR:
	case CAM_ISP_PATTERN_BAYER_BGBGBG:
	case CAM_ISP_PATTERN_BAYER_GBGBGB:
	case CAM_ISP_PATTERN_YUV_YCBYCR:
	case CAM_ISP_PATTERN_YUV_YCRYCB:
	case CAM_ISP_PATTERN_YUV_CBYCRY:
	case CAM_ISP_PATTERN_YUV_CRYCBY:
		rc = 0;
		break;
	default:
		CAM_ERR(CAM_ISP, "Error! Invalid pix pattern:%d", pattern);
		rc = -EINVAL;
		break;
	}
	return rc;
}

static int cam_vfe_camif_get_reg_update(
	struct cam_isp_resource_node  *camif_res,
	void *cmd_args, uint32_t arg_size)
{
	uint32_t                          size = 0;
	uint32_t                          reg_val_pair[2];
	struct cam_isp_hw_get_cdm_args   *cdm_args = cmd_args;
	struct cam_cdm_utils_ops         *cdm_util_ops = NULL;
	struct cam_vfe_mux_camif_data    *rsrc_data = NULL;

	if (arg_size != sizeof(struct cam_isp_hw_get_cdm_args)) {
		CAM_ERR(CAM_ISP, "Invalid cmd size");
		return -EINVAL;
	}

	if (!cdm_args || !cdm_args->res) {
		CAM_ERR(CAM_ISP, "Invalid args");
		return -EINVAL;
	}

	cdm_util_ops = (struct cam_cdm_utils_ops *)cdm_args->res->cdm_ops;

	if (!cdm_util_ops) {
		CAM_ERR(CAM_ISP, "Invalid CDM ops");
		return -EINVAL;
	}

	size = cdm_util_ops->cdm_required_size_reg_random(1);
	/* since cdm returns dwords, we need to convert it into bytes */
	if ((size * 4) > cdm_args->size) {
		CAM_ERR(CAM_ISP, "buf size:%d is not sufficient, expected: %d",
			cdm_args->size, size);
		return -EINVAL;
	}

	rsrc_data = camif_res->res_priv;
	reg_val_pair[0] = rsrc_data->camif_reg->reg_update_cmd;
	reg_val_pair[1] = rsrc_data->reg_data->reg_update_cmd_data;
	CAM_DBG(CAM_ISP, "CAMIF reg_update_cmd %x offset %x",
		reg_val_pair[1], reg_val_pair[0]);

	cdm_util_ops->cdm_write_regrandom(cdm_args->cmd_buf_addr,
		1, reg_val_pair);

	cdm_args->used_bytes = size * 4;

	return 0;
}

int cam_vfe_camif_ver2_acquire_resource(
	struct cam_isp_resource_node  *camif_res,
	void                          *acquire_param)
{
	struct cam_vfe_mux_camif_data      *camif_data;
	struct cam_vfe_acquire_args        *acquire_data;

	int rc = 0;

	camif_data   = (struct cam_vfe_mux_camif_data *)camif_res->res_priv;
	acquire_data = (struct cam_vfe_acquire_args   *)acquire_param;

	rc = cam_vfe_camif_validate_pix_pattern(
			acquire_data->vfe_in.in_port->test_pattern);
	if (rc)
		return rc;

	camif_data->sync_mode   = acquire_data->vfe_in.sync_mode;
	camif_data->pix_pattern = acquire_data->vfe_in.in_port->test_pattern;
	camif_data->first_pixel = acquire_data->vfe_in.in_port->left_start;
	camif_data->last_pixel  = acquire_data->vfe_in.in_port->left_stop;
	camif_data->first_line  = acquire_data->vfe_in.in_port->line_start;
	camif_data->last_line   = acquire_data->vfe_in.in_port->line_stop;

	return rc;
}

static int cam_vfe_camif_resource_start(
	struct cam_isp_resource_node        *camif_res)
{
	struct cam_vfe_mux_camif_data       *rsrc_data;
	uint32_t                             val = 0;

	if (!camif_res) {
		CAM_ERR(CAM_ISP, "Error! Invalid input arguments");
		return -EINVAL;
	}

	if (camif_res->res_state != CAM_ISP_RESOURCE_STATE_RESERVED) {
		CAM_ERR(CAM_ISP, "Error! Invalid camif res res_state:%d",
			camif_res->res_state);
		return -EINVAL;
	}

	rsrc_data = (struct cam_vfe_mux_camif_data  *)camif_res->res_priv;

	/*config vfe core*/
	val = (rsrc_data->pix_pattern <<
			rsrc_data->reg_data->pixel_pattern_shift);
	if (rsrc_data->sync_mode == CAM_ISP_HW_SYNC_SLAVE)
		val |= (1 << rsrc_data->reg_data->extern_reg_update_shift);

	cam_io_w_mb(val, rsrc_data->mem_base + rsrc_data->common_reg->core_cfg);

	CAM_DBG(CAM_ISP, "hw id:%d core_cfg val:%d", camif_res->hw_intf->hw_idx,
		val);

	cam_io_w_mb(0x00400040, rsrc_data->mem_base +
		rsrc_data->camif_reg->camif_config);
	cam_io_w_mb(0x1, rsrc_data->mem_base +
			rsrc_data->camif_reg->line_skip_pattern);
	cam_io_w_mb(0x1, rsrc_data->mem_base +
			rsrc_data->camif_reg->pixel_skip_pattern);

	/* epoch config with 20 line */
	cam_io_w_mb(rsrc_data->reg_data->epoch_line_cfg,
		rsrc_data->mem_base + rsrc_data->camif_reg->epoch_irq);

	camif_res->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;

	/* Reg Update */
	cam_io_w_mb(rsrc_data->reg_data->reg_update_cmd_data,
		rsrc_data->mem_base + rsrc_data->camif_reg->reg_update_cmd);

	CAM_DBG(CAM_ISP, "Start Camif IFE %d Done", camif_res->hw_intf->hw_idx);
	return 0;
}


static int cam_vfe_camif_resource_stop(
	struct cam_isp_resource_node        *camif_res)
{
	struct cam_vfe_mux_camif_data       *camif_priv;
	struct cam_vfe_camif_ver2_reg       *camif_reg;
	int rc = 0;

	if (!camif_res) {
		CAM_ERR(CAM_ISP, "Error! Invalid input arguments");
		return -EINVAL;
	}

	if (camif_res->res_state == CAM_ISP_RESOURCE_STATE_RESERVED ||
		camif_res->res_state == CAM_ISP_RESOURCE_STATE_AVAILABLE)
		return 0;

	camif_priv = (struct cam_vfe_mux_camif_data *)camif_res->res_priv;
	camif_reg = camif_priv->camif_reg;

	if (camif_res->res_state == CAM_ISP_RESOURCE_STATE_STREAMING)
		camif_res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;

	return rc;
}

static int cam_vfe_camif_process_cmd(struct cam_isp_resource_node *rsrc_node,
	uint32_t cmd_type, void *cmd_args, uint32_t arg_size)
{
	int rc = -EINVAL;

	if (!rsrc_node || !cmd_args) {
		CAM_ERR(CAM_ISP, "Invalid input arguments");
		return -EINVAL;
	}

	switch (cmd_type) {
	case CAM_VFE_HW_CMD_GET_REG_UPDATE:
		rc = cam_vfe_camif_get_reg_update(rsrc_node, cmd_args,
			arg_size);
		break;
	default:
		CAM_ERR(CAM_ISP,
			"unsupported process command:%d", cmd_type);
		break;
	}

	return rc;
}

static int cam_vfe_camif_handle_irq_top_half(uint32_t evt_id,
	struct cam_irq_th_payload *th_payload)
{
	return -EPERM;
}

static int cam_vfe_camif_handle_irq_bottom_half(void *handler_priv,
	void *evt_payload_priv)
{
	int                                   ret = CAM_VFE_IRQ_STATUS_ERR;
	struct cam_isp_resource_node         *camif_node;
	struct cam_vfe_mux_camif_data        *camif_priv;
	struct cam_vfe_top_irq_evt_payload   *payload;
	uint32_t                              irq_status0;

	if (!handler_priv || !evt_payload_priv)
		return ret;

	camif_node = handler_priv;
	camif_priv = camif_node->res_priv;
	payload = evt_payload_priv;
	irq_status0 = payload->irq_reg_val[CAM_IFE_IRQ_CAMIF_REG_STATUS0];

	CAM_DBG(CAM_ISP, "event ID:%d", payload->evt_id);
	CAM_DBG(CAM_ISP, "irq_status_0 = %x", irq_status0);

	switch (payload->evt_id) {
	case CAM_ISP_HW_EVENT_SOF:
		if (irq_status0 & camif_priv->reg_data->sof_irq_mask) {
			CAM_DBG(CAM_ISP, "Received SOF");
			ret = CAM_VFE_IRQ_STATUS_SUCCESS;
		}
		break;
	case CAM_ISP_HW_EVENT_EPOCH:
		if (irq_status0 & camif_priv->reg_data->epoch0_irq_mask) {
			CAM_DBG(CAM_ISP, "Received EPOCH");
			ret = CAM_VFE_IRQ_STATUS_SUCCESS;
		}
		cam_vfe_put_evt_payload(payload->core_info, &payload);
		break;
	case CAM_ISP_HW_EVENT_REG_UPDATE:
		if (irq_status0 & camif_priv->reg_data->reg_update_irq_mask) {
			CAM_DBG(CAM_ISP, "Received REG_UPDATE_ACK");
			ret = CAM_VFE_IRQ_STATUS_SUCCESS;
		}
		break;
	case CAM_ISP_HW_EVENT_EOF:
		if (irq_status0 & camif_priv->reg_data->eof_irq_mask) {
			CAM_DBG(CAM_ISP, "Received EOF\n");
			ret = CAM_VFE_IRQ_STATUS_SUCCESS;
		}
		break;
	default:
		break;
	}

	CAM_DBG(CAM_ISP, "returing status = %d", ret);
	return ret;
}

int cam_vfe_camif_ver2_init(
	struct cam_hw_intf            *hw_intf,
	struct cam_hw_soc_info        *soc_info,
	void                          *camif_hw_info,
	struct cam_isp_resource_node  *camif_node)
{
	struct cam_vfe_mux_camif_data     *camif_priv = NULL;
	struct cam_vfe_camif_ver2_hw_info *camif_info = camif_hw_info;

	camif_priv = kzalloc(sizeof(struct cam_vfe_mux_camif_data),
		GFP_KERNEL);
	if (!camif_priv) {
		CAM_DBG(CAM_ISP, "Error! Failed to alloc for camif_priv");
		return -ENOMEM;
	}

	camif_node->res_priv = camif_priv;

	camif_priv->mem_base   = soc_info->reg_map[VFE_CORE_BASE_IDX].mem_base;
	camif_priv->camif_reg  = camif_info->camif_reg;
	camif_priv->common_reg = camif_info->common_reg;
	camif_priv->reg_data   = camif_info->reg_data;
	camif_priv->hw_intf    = hw_intf;

	camif_node->start = cam_vfe_camif_resource_start;
	camif_node->stop  = cam_vfe_camif_resource_stop;
	camif_node->process_cmd = cam_vfe_camif_process_cmd;
	camif_node->top_half_handler = cam_vfe_camif_handle_irq_top_half;
	camif_node->bottom_half_handler = cam_vfe_camif_handle_irq_bottom_half;

	return 0;
}

int cam_vfe_camif_ver2_deinit(
	struct cam_isp_resource_node  *camif_node)
{
	struct cam_vfe_mux_camif_data *camif_priv = camif_node->res_priv;

	camif_node->start = NULL;
	camif_node->stop  = NULL;
	camif_node->process_cmd = NULL;
	camif_node->top_half_handler = NULL;
	camif_node->bottom_half_handler = NULL;

	camif_node->res_priv = NULL;

	if (!camif_priv) {
		CAM_ERR(CAM_ISP, "Error! camif_priv is NULL");
		return -ENODEV;
	}

	kfree(camif_priv);

	return 0;
}
