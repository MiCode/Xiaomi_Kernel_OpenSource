/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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
#include "cam_vfe_soc.h"
#include "cam_vfe_top.h"
#include "cam_vfe_top_ver2.h"
#include "cam_vfe_camif_lite_ver2.h"
#include "cam_debug_util.h"
#include "cam_cdm_util.h"

struct cam_vfe_mux_camif_lite_data {
	void __iomem                                *mem_base;
	struct cam_hw_intf                          *hw_intf;
	struct cam_vfe_camif_lite_ver2_reg          *camif_lite_reg;
	struct cam_vfe_top_ver2_reg_offset_common   *common_reg;
	struct cam_vfe_camif_lite_ver2_reg_data     *reg_data;
	struct cam_hw_soc_info                      *soc_info;
	enum cam_isp_hw_sync_mode                    sync_mode;
};

static int cam_vfe_camif_lite_get_reg_update(
	struct cam_isp_resource_node          *camif_lite_res,
	void                                  *cmd_args,
	uint32_t                               arg_size)
{
	uint32_t                               size = 0;
	uint32_t                               reg_val_pair[2];
	struct cam_isp_hw_get_cmd_update      *cdm_args = cmd_args;
	struct cam_cdm_utils_ops              *cdm_util_ops = NULL;
	struct cam_vfe_mux_camif_lite_data    *rsrc_data = NULL;

	if (arg_size != sizeof(struct cam_isp_hw_get_cmd_update)) {
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
	if ((size * 4) > cdm_args->cmd.size) {
		CAM_ERR(CAM_ISP, "buf size:%d is not sufficient, expected: %d",
			cdm_args->cmd.size, size);
		return -EINVAL;
	}

	rsrc_data = camif_lite_res->res_priv;
	reg_val_pair[0] = rsrc_data->camif_lite_reg->reg_update_cmd;
	reg_val_pair[1] = rsrc_data->reg_data->dual_pd_reg_update_cmd_data;
	CAM_DBG(CAM_ISP, "CAMIF Lite reg_update_cmd %x offset %x",
		reg_val_pair[1], reg_val_pair[0]);

	cdm_util_ops->cdm_write_regrandom(cdm_args->cmd.cmd_buf_addr,
		1, reg_val_pair);

	cdm_args->cmd.used_bytes = size * 4;

	return 0;
}

int cam_vfe_camif_lite_ver2_acquire_resource(
	struct cam_isp_resource_node          *camif_lite_res,
	void                                  *acquire_param)
{
	struct cam_vfe_mux_camif_lite_data    *camif_lite_data;
	struct cam_vfe_acquire_args           *acquire_data;
	int rc = 0;

	if (!camif_lite_res) {
		CAM_ERR(CAM_ISP, "Error Invalid input arguments");
		return -EINVAL;
	}

	camif_lite_data = (struct cam_vfe_mux_camif_lite_data *)
		camif_lite_res->res_priv;
	acquire_data = (struct cam_vfe_acquire_args *)acquire_param;

	camif_lite_data->sync_mode   = acquire_data->vfe_in.sync_mode;
	camif_lite_res->rdi_only_ctx = 0;

	CAM_DBG(CAM_ISP, "hw id:%d sync_mode=%d",
		camif_lite_res->hw_intf->hw_idx,
		camif_lite_data->sync_mode);
	return rc;
}

static int cam_vfe_camif_lite_resource_start(
	struct cam_isp_resource_node         *camif_lite_res)
{
	struct cam_vfe_mux_camif_lite_data   *rsrc_data;
	uint32_t                              val = 0;

	if (!camif_lite_res) {
		CAM_ERR(CAM_ISP, "Error! Invalid input arguments");
		return -EINVAL;
	}

	if (camif_lite_res->res_state != CAM_ISP_RESOURCE_STATE_RESERVED) {
		CAM_ERR(CAM_ISP, "Error! Invalid camif lite res res_state:%d",
			camif_lite_res->res_state);
		return -EINVAL;
	}

	rsrc_data = (struct cam_vfe_mux_camif_lite_data *)
		camif_lite_res->res_priv;

	/* vfe core config */
	val = cam_io_r_mb(rsrc_data->mem_base +
		rsrc_data->common_reg->core_cfg);

	if (rsrc_data->sync_mode == CAM_ISP_HW_SYNC_SLAVE)
		val |= (1 << rsrc_data->reg_data->extern_reg_update_shift);

	val |= (1 << rsrc_data->reg_data->dual_pd_path_sel_shift);

	cam_io_w_mb(val, rsrc_data->mem_base +
		rsrc_data->common_reg->core_cfg);

	CAM_DBG(CAM_ISP, "hw id:%d core_cfg val:%d",
		camif_lite_res->hw_intf->hw_idx, val);

	/* epoch config with 20 line */
	cam_io_w_mb(rsrc_data->reg_data->lite_epoch_line_cfg,
		rsrc_data->mem_base +
		rsrc_data->camif_lite_reg->lite_epoch_irq);

	camif_lite_res->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;

	/* Reg Update */
	cam_io_w_mb(rsrc_data->reg_data->dual_pd_reg_update_cmd_data,
		rsrc_data->mem_base +
		rsrc_data->camif_lite_reg->reg_update_cmd);
	CAM_DBG(CAM_ISP, "hw id:%d RUP val:%d",
		camif_lite_res->hw_intf->hw_idx,
		rsrc_data->reg_data->dual_pd_reg_update_cmd_data);

	CAM_DBG(CAM_ISP, "Start Camif Lite IFE %d Done",
		camif_lite_res->hw_intf->hw_idx);
	return 0;
}

static int cam_vfe_camif_lite_resource_stop(
	struct cam_isp_resource_node             *camif_lite_res)
{
	int                                       rc = 0;

	if (!camif_lite_res) {
		CAM_ERR(CAM_ISP, "Error! Invalid input arguments");
		return -EINVAL;
	}

	if ((camif_lite_res->res_state == CAM_ISP_RESOURCE_STATE_RESERVED) ||
		(camif_lite_res->res_state == CAM_ISP_RESOURCE_STATE_AVAILABLE))
		return 0;

	if (camif_lite_res->res_state == CAM_ISP_RESOURCE_STATE_STREAMING)
		camif_lite_res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;

	return rc;
}

static int cam_vfe_camif_lite_process_cmd(
	struct cam_isp_resource_node *rsrc_node,
	uint32_t cmd_type, void *cmd_args, uint32_t arg_size)
{
	int rc = -EINVAL;

	if (!rsrc_node || !cmd_args) {
		CAM_ERR(CAM_ISP, "Invalid input arguments");
		return -EINVAL;
	}

	switch (cmd_type) {
	case CAM_ISP_HW_CMD_GET_REG_UPDATE:
		rc = cam_vfe_camif_lite_get_reg_update(rsrc_node, cmd_args,
			arg_size);
		break;
	default:
		CAM_ERR(CAM_ISP,
			"unsupported process command:%d", cmd_type);
		break;
	}

	return rc;
}

static int cam_vfe_camif_lite_handle_irq_top_half(uint32_t evt_id,
	struct cam_irq_th_payload *th_payload)
{
	return -EPERM;
}

static int cam_vfe_camif_lite_handle_irq_bottom_half(
	void                                    *handler_priv,
	void                                    *evt_payload_priv)
{
	int                                      ret = CAM_VFE_IRQ_STATUS_ERR;
	struct cam_isp_resource_node            *camif_lite_node;
	struct cam_vfe_mux_camif_lite_data      *camif_lite_priv;
	struct cam_vfe_top_irq_evt_payload      *payload;
	uint32_t                                 irq_status0;
	uint32_t                                 irq_status1;

	if (!handler_priv || !evt_payload_priv) {
		CAM_ERR(CAM_ISP, "Invalid params");
		return ret;
	}

	camif_lite_node = handler_priv;
	camif_lite_priv = camif_lite_node->res_priv;
	payload         = evt_payload_priv;
	irq_status0     = payload->irq_reg_val[CAM_IFE_IRQ_CAMIF_REG_STATUS0];
	irq_status1     = payload->irq_reg_val[CAM_IFE_IRQ_CAMIF_REG_STATUS1];

	CAM_DBG(CAM_ISP, "event ID:%d", payload->evt_id);
	CAM_DBG(CAM_ISP, "irq_status_0 = %x", irq_status0);

	switch (payload->evt_id) {
	case CAM_ISP_HW_EVENT_SOF:
		if (irq_status0 &
			camif_lite_priv->reg_data->lite_sof_irq_mask) {
			CAM_DBG(CAM_ISP, "Received SOF");
			ret = CAM_VFE_IRQ_STATUS_SUCCESS;
		}
		break;
	case CAM_ISP_HW_EVENT_EPOCH:
		if (irq_status0 &
			camif_lite_priv->reg_data->lite_epoch0_irq_mask) {
			CAM_DBG(CAM_ISP, "Received EPOCH");
			ret = CAM_VFE_IRQ_STATUS_SUCCESS;
		}
		break;
	case CAM_ISP_HW_EVENT_REG_UPDATE:
		if (irq_status0 &
			camif_lite_priv->reg_data->dual_pd_reg_upd_irq_mask) {
			CAM_DBG(CAM_ISP, "Received REG_UPDATE_ACK");
			ret = CAM_VFE_IRQ_STATUS_SUCCESS;
		}
		break;
	case CAM_ISP_HW_EVENT_EOF:
		if (irq_status0 &
			camif_lite_priv->reg_data->lite_eof_irq_mask) {
			CAM_DBG(CAM_ISP, "Received EOF\n");
			ret = CAM_VFE_IRQ_STATUS_SUCCESS;
		}
		break;
	case CAM_ISP_HW_EVENT_ERROR:
		if (irq_status1 &
			camif_lite_priv->reg_data->lite_error_irq_mask1) {
			CAM_DBG(CAM_ISP, "Received ERROR\n");
			ret = CAM_ISP_HW_ERROR_OVERFLOW;
		} else {
			ret = CAM_ISP_HW_ERROR_NONE;
		}
		break;
	default:
		break;
	}

	CAM_DBG(CAM_ISP, "returning status = %d", ret);
	return ret;
}

int cam_vfe_camif_lite_ver2_init(
	struct cam_hw_intf            *hw_intf,
	struct cam_hw_soc_info        *soc_info,
	void                          *camif_lite_hw_info,
	struct cam_isp_resource_node  *camif_lite_node)
{
	struct cam_vfe_mux_camif_lite_data       *camif_lite_priv = NULL;
	struct cam_vfe_camif_lite_ver2_hw_info   *camif_lite_info =
		camif_lite_hw_info;

	camif_lite_priv = kzalloc(sizeof(*camif_lite_priv),
		GFP_KERNEL);
	if (!camif_lite_priv)
		return -ENOMEM;

	camif_lite_node->res_priv = camif_lite_priv;

	camif_lite_priv->mem_base         =
		soc_info->reg_map[VFE_CORE_BASE_IDX].mem_base;
	camif_lite_priv->camif_lite_reg   = camif_lite_info->camif_lite_reg;
	camif_lite_priv->common_reg       = camif_lite_info->common_reg;
	camif_lite_priv->reg_data         = camif_lite_info->reg_data;
	camif_lite_priv->hw_intf          = hw_intf;
	camif_lite_priv->soc_info         = soc_info;

	camif_lite_node->init    = NULL;
	camif_lite_node->deinit  = NULL;
	camif_lite_node->start   = cam_vfe_camif_lite_resource_start;
	camif_lite_node->stop    = cam_vfe_camif_lite_resource_stop;
	camif_lite_node->process_cmd = cam_vfe_camif_lite_process_cmd;
	camif_lite_node->top_half_handler =
		cam_vfe_camif_lite_handle_irq_top_half;
	camif_lite_node->bottom_half_handler =
		cam_vfe_camif_lite_handle_irq_bottom_half;

	return 0;
}

int cam_vfe_camif_lite_ver2_deinit(
	struct cam_isp_resource_node  *camif_lite_node)
{
	struct cam_vfe_mux_camif_data *camif_lite_priv =
		camif_lite_node->res_priv;

	camif_lite_node->start = NULL;
	camif_lite_node->stop  = NULL;
	camif_lite_node->process_cmd = NULL;
	camif_lite_node->top_half_handler = NULL;
	camif_lite_node->bottom_half_handler = NULL;

	camif_lite_node->res_priv = NULL;

	if (!camif_lite_priv) {
		CAM_ERR(CAM_ISP, "Error! camif_priv is NULL");
		return -ENODEV;
	}

	kfree(camif_lite_priv);

	return 0;
}
