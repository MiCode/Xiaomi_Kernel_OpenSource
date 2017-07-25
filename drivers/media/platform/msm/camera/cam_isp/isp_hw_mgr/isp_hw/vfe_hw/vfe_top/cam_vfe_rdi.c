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

#define pr_fmt(fmt) "%s:%d " fmt, __func__, __LINE__

#include <linux/slab.h>
#include "cam_vfe_rdi.h"
#include "cam_isp_hw_mgr_intf.h"
#include "cam_vfe_hw_intf.h"
#include "cam_io_util.h"

#undef CDBG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)

struct cam_vfe_mux_rdi_data {
	void __iomem                                *mem_base;
	struct cam_hw_intf                          *hw_intf;
	struct cam_vfe_top_ver2_reg_offset_common   *common_reg;

	enum cam_isp_hw_sync_mode          sync_mode;
};

int cam_vfe_rdi_ver2_acquire_resource(
	struct cam_isp_resource_node  *rdi_res,
	void                          *acquire_param)
{
	struct cam_vfe_mux_rdi_data   *rdi_data;
	struct cam_vfe_acquire_args   *acquire_data;

	rdi_data     = (struct cam_vfe_mux_rdi_data *)rdi_res->res_priv;
	acquire_data = (struct cam_vfe_acquire_args *)acquire_param;

	rdi_data->sync_mode   = acquire_data->vfe_in.sync_mode;

	return 0;
}

static int cam_vfe_rdi_resource_start(
	struct cam_isp_resource_node  *rdi_res)
{
	struct cam_vfe_mux_rdi_data   *rsrc_data;
	int                            rc = 0;

	if (!rdi_res) {
		pr_err("Error! Invalid input arguments\n");
		return -EINVAL;
	}

	if (rdi_res->res_state != CAM_ISP_RESOURCE_STATE_RESERVED) {
		pr_err("Error! Invalid rdi res res_state:%d\n",
			rdi_res->res_state);
		return -EINVAL;
	}

	rsrc_data = (struct cam_vfe_mux_rdi_data  *)rdi_res->res_priv;
	rdi_res->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;

	/* Reg Update */
	cam_io_w_mb(0x2, rsrc_data->mem_base + 0x4AC);

	CDBG("Exit\n");

	return rc;
}


static int cam_vfe_rdi_resource_stop(
	struct cam_isp_resource_node        *rdi_res)
{
	struct cam_vfe_mux_rdi_data           *rdi_priv;
	int rc = 0;

	if (!rdi_res) {
		pr_err("Error! Invalid input arguments\n");
		return -EINVAL;
	}

	if (rdi_res->res_state == CAM_ISP_RESOURCE_STATE_RESERVED ||
		rdi_res->res_state == CAM_ISP_RESOURCE_STATE_AVAILABLE)
		return 0;

	rdi_priv = (struct cam_vfe_mux_rdi_data *)rdi_res->res_priv;

	if (rdi_res->res_state == CAM_ISP_RESOURCE_STATE_STREAMING)
		rdi_res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;


	return rc;
}

int cam_vfe_rdi_process_cmd(void *priv,
	uint32_t cmd_type, void *cmd_args, uint32_t arg_size)
{
	int rc = -EINVAL;

	if (!priv || !cmd_args) {
		pr_err("Error! Invalid input arguments\n");
		return -EINVAL;
	}

	switch (cmd_type) {
	default:
		pr_err("Error! unsupported RDI process command:%d\n", cmd_type);
		break;
	}

	return rc;
}

static int cam_vfe_rdi_handle_irq_top_half(uint32_t evt_id,
	struct cam_irq_th_payload *th_payload)
{
	return -EPERM;
}

static int cam_vfe_rdi_handle_irq_bottom_half(void *handler_priv,
	void *evt_payload_priv)
{
	int                                  ret = CAM_VFE_IRQ_STATUS_ERR;
	struct cam_isp_resource_node        *rdi_node;
	struct cam_vfe_mux_rdi_data         *rdi_priv;
	struct cam_vfe_top_irq_evt_payload  *payload;
	uint32_t                             irq_status0;

	if (!handler_priv || !evt_payload_priv)
		return ret;

	rdi_node = handler_priv;
	rdi_priv = rdi_node->res_priv;
	payload = evt_payload_priv;
	irq_status0 = payload->irq_reg_val[CAM_IFE_IRQ_CAMIF_REG_STATUS0];

	CDBG("event ID:%d\n", payload->evt_id);
	CDBG("irq_status_0 = %x\n", irq_status0);

	switch (payload->evt_id) {
	case CAM_ISP_HW_EVENT_SOF:
		if (irq_status0 & 0x8000000)
			ret = CAM_VFE_IRQ_STATUS_SUCCESS;
		break;
	case CAM_ISP_HW_EVENT_REG_UPDATE:
		if (irq_status0 & 0x20)
			ret = CAM_VFE_IRQ_STATUS_SUCCESS;
		break;
	default:
		break;
	}

	CDBG("returing status = %d\n", ret);
	return ret;
}

int cam_vfe_rdi_ver2_init(
	struct cam_hw_intf            *hw_intf,
	struct cam_hw_soc_info        *soc_info,
	void                          *rdi_hw_info,
	struct cam_isp_resource_node  *rdi_node)
{
	struct cam_vfe_mux_rdi_data     *rdi_priv = NULL;

	rdi_priv = kzalloc(sizeof(struct cam_vfe_mux_rdi_data),
			GFP_KERNEL);
	if (!rdi_priv) {
		CDBG("Error! Failed to alloc for rdi_priv\n");
		return -ENOMEM;
	}

	rdi_node->res_priv = rdi_priv;

	rdi_priv->mem_base   = soc_info->reg_map[VFE_CORE_BASE_IDX].mem_base;
	rdi_priv->hw_intf    = hw_intf;

	rdi_node->start = cam_vfe_rdi_resource_start;
	rdi_node->stop  = cam_vfe_rdi_resource_stop;
	rdi_node->top_half_handler = cam_vfe_rdi_handle_irq_top_half;
	rdi_node->bottom_half_handler = cam_vfe_rdi_handle_irq_bottom_half;

	return 0;
}

int cam_vfe_rdi_ver2_deinit(
	struct cam_isp_resource_node  *rdi_node)
{
	struct cam_vfe_mux_rdi_data *rdi_priv = rdi_node->res_priv;

	rdi_node->start = NULL;
	rdi_node->stop  = NULL;
	rdi_node->top_half_handler = NULL;
	rdi_node->bottom_half_handler = NULL;

	rdi_node->res_priv = NULL;

	if (!rdi_priv) {
		pr_err("Error! rdi_priv NULL\n");
		return -ENODEV;
	}
	kfree(rdi_priv);

	return 0;
}
