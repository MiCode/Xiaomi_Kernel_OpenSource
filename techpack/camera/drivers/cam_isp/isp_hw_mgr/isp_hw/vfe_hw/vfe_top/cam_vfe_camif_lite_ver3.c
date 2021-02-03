// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#include <linux/slab.h>
#include <media/cam_isp.h>
#include "cam_io_util.h"
#include "cam_isp_hw_mgr_intf.h"
#include "cam_isp_hw.h"
#include "cam_vfe_hw_intf.h"
#include "cam_vfe_soc.h"
#include "cam_vfe_top.h"
#include "cam_vfe_top_ver3.h"
#include "cam_irq_controller.h"
#include "cam_tasklet_util.h"
#include "cam_vfe_camif_lite_ver3.h"
#include "cam_debug_util.h"
#include "cam_cdm_util.h"
#include "cam_cpas_api.h"

struct cam_vfe_mux_camif_lite_data {
	void __iomem                                *mem_base;
	struct cam_hw_intf                          *hw_intf;
	struct cam_vfe_camif_lite_ver3_reg          *camif_lite_reg;
	struct cam_vfe_top_ver3_reg_offset_common   *common_reg;
	struct cam_vfe_camif_lite_ver3_reg_data     *reg_data;
	struct cam_hw_soc_info                      *soc_info;
	enum cam_isp_hw_sync_mode                    sync_mode;
	struct cam_vfe_camif_common_cfg              cam_common_cfg;

	cam_hw_mgr_event_cb_func                     event_cb;
	void                                        *priv;
	int                                          irq_err_handle;
	int                                          irq_handle;
	int                                          sof_irq_handle;
	void                                        *vfe_irq_controller;
	struct list_head                             free_payload_list;
	spinlock_t                                   spin_lock;
	uint32_t                                     camif_debug;
	struct cam_vfe_top_irq_evt_payload
		evt_payload[CAM_VFE_CAMIF_LITE_EVT_MAX];
};

static int cam_vfe_camif_lite_get_evt_payload(
	struct cam_vfe_mux_camif_lite_data     *camif_lite_priv,
	struct cam_vfe_top_irq_evt_payload    **evt_payload)
{
	int rc = 0;

	spin_lock(&camif_lite_priv->spin_lock);
	if (list_empty(&camif_lite_priv->free_payload_list)) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "No free CAMIF LITE event payload");
		rc = -ENODEV;
		goto done;
	}

	*evt_payload = list_first_entry(&camif_lite_priv->free_payload_list,
		struct cam_vfe_top_irq_evt_payload, list);
	list_del_init(&(*evt_payload)->list);
	rc = 0;
done:
	spin_unlock(&camif_lite_priv->spin_lock);
	return rc;
}

static int cam_vfe_camif_lite_put_evt_payload(
	struct cam_vfe_mux_camif_lite_data     *camif_lite_priv,
	struct cam_vfe_top_irq_evt_payload    **evt_payload)
{
	unsigned long flags;

	if (!camif_lite_priv) {
		CAM_ERR(CAM_ISP, "Invalid param core_info NULL");
		return -EINVAL;
	}
	if (*evt_payload == NULL) {
		CAM_ERR(CAM_ISP, "No payload to put");
		return -EINVAL;
	}

	spin_lock_irqsave(&camif_lite_priv->spin_lock, flags);
	list_add_tail(&(*evt_payload)->list,
		&camif_lite_priv->free_payload_list);
	*evt_payload = NULL;
	spin_unlock_irqrestore(&camif_lite_priv->spin_lock, flags);

	CAM_DBG(CAM_ISP, "Done");
	return 0;
}

static int cam_vfe_camif_lite_err_irq_top_half(
	uint32_t                               evt_id,
	struct cam_irq_th_payload             *th_payload)
{
	int32_t                                rc;
	int                                    i;
	struct cam_isp_resource_node          *camif_lite_node;
	struct cam_vfe_mux_camif_lite_data    *camif_lite_priv;
	struct cam_vfe_top_irq_evt_payload    *evt_payload;
	struct cam_vfe_soc_private            *soc_private = NULL;
	bool                                   error_flag = false;

	camif_lite_node = th_payload->handler_priv;
	camif_lite_priv = camif_lite_node->res_priv;

	soc_private = camif_lite_priv->soc_info->soc_private;
	if (!soc_private) {
		CAM_ERR(CAM_ISP, "Invalid soc_private");
		return -ENODEV;
	}

	/*
	 *  need to handle overflow condition here, otherwise irq storm
	 *  will block everything
	 */
	if (th_payload->evt_status_arr[2] || (th_payload->evt_status_arr[0] &
		camif_lite_priv->reg_data->error_irq_mask0)) {
		CAM_ERR(CAM_ISP,
			"VFE:%d CAMIF LITE:%d Err IRQ status_1: 0x%X status_2: 0x%X",
			camif_lite_node->hw_intf->hw_idx,
			camif_lite_node->res_id,
			th_payload->evt_status_arr[0],
			th_payload->evt_status_arr[2]);
		CAM_ERR(CAM_ISP, "Stopping further IRQ processing from VFE:%d",
			camif_lite_node->hw_intf->hw_idx);
		cam_irq_controller_disable_irq(
			camif_lite_priv->vfe_irq_controller,
			camif_lite_priv->irq_err_handle);
		cam_irq_controller_clear_and_mask(evt_id,
			camif_lite_priv->vfe_irq_controller);
		error_flag = true;
	}

	rc  = cam_vfe_camif_lite_get_evt_payload(camif_lite_priv, &evt_payload);
	if (rc)
		return rc;

	cam_isp_hw_get_timestamp(&evt_payload->ts);

	for (i = 0; i < th_payload->num_registers; i++)
		evt_payload->irq_reg_val[i] = th_payload->evt_status_arr[i];

	evt_payload->irq_reg_val[i] = cam_io_r(camif_lite_priv->mem_base +
		camif_lite_priv->common_reg->violation_status);

	evt_payload->irq_reg_val[++i] = cam_io_r(camif_lite_priv->mem_base +
		camif_lite_priv->common_reg->bus_overflow_status);

	th_payload->evt_payload_priv = evt_payload;

	return rc;
}


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
		CAM_ERR(CAM_ISP,
			"Invalid args: cdm args %pK", cdm_args);
		return -EINVAL;
	}

	CAM_DBG(CAM_ISP, "CAMIF LITE:%d get RUP", camif_lite_res->res_id);

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
	reg_val_pair[1] = rsrc_data->reg_data->reg_update_cmd_data;
	CAM_DBG(CAM_ISP, "CAMIF LITE:%d reg_update_cmd 0x%X offset 0x%X",
		camif_lite_res->res_id, reg_val_pair[1], reg_val_pair[0]);

	cdm_util_ops->cdm_write_regrandom(cdm_args->cmd.cmd_buf_addr,
		1, reg_val_pair);

	cdm_args->cmd.used_bytes = size * 4;

	return 0;
}

int cam_vfe_camif_lite_ver3_acquire_resource(
	struct cam_isp_resource_node          *camif_lite_res,
	void                                  *acquire_param)
{
	struct cam_vfe_mux_camif_lite_data    *camif_lite_data;
	struct cam_vfe_acquire_args           *acquire_data;

	if (!camif_lite_res) {
		CAM_ERR(CAM_ISP, "Error Invalid input arguments");
		return -EINVAL;
	}

	camif_lite_data = (struct cam_vfe_mux_camif_lite_data *)
		camif_lite_res->res_priv;
	acquire_data = (struct cam_vfe_acquire_args *)acquire_param;

	camif_lite_data->sync_mode   = acquire_data->vfe_in.sync_mode;
	camif_lite_data->event_cb    = acquire_data->event_cb;
	camif_lite_data->priv        = acquire_data->priv;
	camif_lite_res->rdi_only_ctx = 0;
	CAM_DBG(CAM_ISP, "Acquired VFE:%d CAMIF LITE:%d sync_mode=%d",
		camif_lite_res->hw_intf->hw_idx,
		camif_lite_res->res_id,
		camif_lite_data->sync_mode);
	return 0;
}

static int cam_vfe_camif_lite_resource_start(
	struct cam_isp_resource_node         *camif_lite_res)
{
	struct cam_vfe_mux_camif_lite_data   *rsrc_data;
	struct cam_vfe_soc_private           *soc_private = NULL;
	uint32_t                              val = 0;
	int                                   rc = 0;
	uint32_t err_irq_mask[CAM_IFE_IRQ_REGISTERS_MAX];
	uint32_t irq_mask[CAM_IFE_IRQ_REGISTERS_MAX];

	if (!camif_lite_res) {
		CAM_ERR(CAM_ISP, "Invalid input arguments");
		return -EINVAL;
	}

	if (camif_lite_res->res_state != CAM_ISP_RESOURCE_STATE_RESERVED) {
		CAM_ERR(CAM_ISP, "Invalid camif lite res res_state:%d",
			camif_lite_res->res_state);
		return -EINVAL;
	}

	CAM_DBG(CAM_ISP, "CAMIF LITE:%d Start", camif_lite_res->res_id);

	rsrc_data = (struct cam_vfe_mux_camif_lite_data *)
		camif_lite_res->res_priv;

	soc_private = rsrc_data->soc_info->soc_private;
	if (!soc_private) {
		CAM_ERR(CAM_ISP, "Invalid soc_private");
		return -ENODEV;
	}

	if (soc_private->is_ife_lite)
		goto skip_core_cfg;

	/* vfe core config */
	val = cam_io_r_mb(rsrc_data->mem_base +
		rsrc_data->common_reg->core_cfg_0);

	if (camif_lite_res->res_id == CAM_ISP_HW_VFE_IN_LCR &&
		rsrc_data->sync_mode == CAM_ISP_HW_SYNC_SLAVE)
		val |= (1 << rsrc_data->reg_data->extern_reg_update_shift);

	if (camif_lite_res->res_id == CAM_ISP_HW_VFE_IN_PDLIB) {
		val |= (1 << rsrc_data->reg_data->operating_mode_shift);
		val |= (rsrc_data->cam_common_cfg.input_mux_sel_pdaf & 0x1) <<
			CAM_SHIFT_TOP_CORE_CFG_MUXSEL_PDAF;
	}

	cam_io_w_mb(val, rsrc_data->mem_base +
		rsrc_data->common_reg->core_cfg_0);

	CAM_DBG(CAM_ISP, "VFE:%d core_cfg val:%d",
		camif_lite_res->hw_intf->hw_idx, val);

	/* epoch config */
	cam_io_w_mb(rsrc_data->reg_data->epoch_line_cfg,
		rsrc_data->mem_base +
		rsrc_data->camif_lite_reg->lite_epoch_irq);

skip_core_cfg:

	camif_lite_res->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;

	/* Reg Update */
	cam_io_w_mb(rsrc_data->reg_data->reg_update_cmd_data,
		rsrc_data->mem_base +
		rsrc_data->camif_lite_reg->reg_update_cmd);

	memset(err_irq_mask, 0, sizeof(err_irq_mask));
	memset(irq_mask, 0, sizeof(irq_mask));

	/* config debug status registers */
	cam_io_w_mb(rsrc_data->reg_data->top_debug_cfg_en, rsrc_data->mem_base +
		rsrc_data->common_reg->top_debug_cfg);

	if (!camif_lite_res->rdi_only_ctx)
		goto subscribe_err;

	irq_mask[CAM_IFE_IRQ_CAMIF_REG_STATUS1] =
		rsrc_data->reg_data->epoch0_irq_mask |
		rsrc_data->reg_data->eof_irq_mask;

	if (!rsrc_data->irq_handle) {
		rsrc_data->irq_handle = cam_irq_controller_subscribe_irq(
			rsrc_data->vfe_irq_controller,
			CAM_IRQ_PRIORITY_3,
			irq_mask,
			camif_lite_res,
			camif_lite_res->top_half_handler,
			camif_lite_res->bottom_half_handler,
			camif_lite_res->tasklet_info,
			&tasklet_bh_api);
		if (rsrc_data->irq_handle < 1) {
			CAM_ERR(CAM_ISP, "IRQ handle subscribe failure");
			rc = -ENOMEM;
			rsrc_data->irq_handle = 0;
		}
	}

	irq_mask[CAM_IFE_IRQ_CAMIF_REG_STATUS1] =
			rsrc_data->reg_data->sof_irq_mask;

	if (!rsrc_data->sof_irq_handle) {
		rsrc_data->sof_irq_handle = cam_irq_controller_subscribe_irq(
			rsrc_data->vfe_irq_controller,
			CAM_IRQ_PRIORITY_1,
			irq_mask,
			camif_lite_res,
			camif_lite_res->top_half_handler,
			camif_lite_res->bottom_half_handler,
			camif_lite_res->tasklet_info,
			&tasklet_bh_api);
		if (rsrc_data->sof_irq_handle < 1) {
			CAM_ERR(CAM_ISP, "IRQ handle subscribe failure");
			rc = -ENOMEM;
			rsrc_data->sof_irq_handle = 0;
		}
	}

subscribe_err:

	err_irq_mask[CAM_IFE_IRQ_CAMIF_REG_STATUS0] =
		rsrc_data->reg_data->error_irq_mask0;
	err_irq_mask[CAM_IFE_IRQ_CAMIF_REG_STATUS2] =
		rsrc_data->reg_data->error_irq_mask2;

	if (!rsrc_data->irq_err_handle) {
		rsrc_data->irq_err_handle = cam_irq_controller_subscribe_irq(
			rsrc_data->vfe_irq_controller,
			CAM_IRQ_PRIORITY_0,
			err_irq_mask,
			camif_lite_res,
			cam_vfe_camif_lite_err_irq_top_half,
			camif_lite_res->bottom_half_handler,
			camif_lite_res->tasklet_info,
			&tasklet_bh_api);

		if (rsrc_data->irq_err_handle < 1) {
			CAM_ERR(CAM_ISP, "Error IRQ handle subscribe failure");
			rc = -ENOMEM;
			rsrc_data->irq_err_handle = 0;
		}
	}

	CAM_DBG(CAM_ISP, "VFE:%d CAMIF LITE:%d Start Done",
		camif_lite_res->hw_intf->hw_idx,
		camif_lite_res->res_id);
	return rc;
}

static int cam_vfe_camif_lite_reg_dump(
	struct cam_isp_resource_node *camif_lite_res)
{
	struct cam_vfe_mux_camif_lite_data *camif_lite_priv;
	struct cam_vfe_soc_private         *soc_private = NULL;
	uint32_t offset, val, wm_idx;

	if (!camif_lite_res) {
		CAM_ERR(CAM_ISP, "Error, Invalid input arguments");
		return -EINVAL;
	}

	if ((camif_lite_res->res_state == CAM_ISP_RESOURCE_STATE_RESERVED) ||
		(camif_lite_res->res_state == CAM_ISP_RESOURCE_STATE_AVAILABLE))
		return 0;

	camif_lite_priv =
		(struct cam_vfe_mux_camif_lite_data *)camif_lite_res->res_priv;

	soc_private = camif_lite_priv->soc_info->soc_private;
	if (!soc_private) {
		CAM_ERR(CAM_ISP, "Invalid soc_private");
		return -ENODEV;
	}

	CAM_INFO(CAM_ISP, "IFE:%d TOP", camif_lite_priv->hw_intf->hw_idx);
	if (!soc_private->is_ife_lite) {
		for (offset = 0x0; offset <= 0x1FC; offset += 0x4) {
			if (offset == 0x1C || offset == 0x34 ||
				offset == 0x38 || offset == 0x90)
				continue;
			val = cam_soc_util_r(camif_lite_priv->soc_info,
				0, offset);
			CAM_INFO(CAM_ISP, "offset 0x%X value 0x%X",
				offset, val);
		}
	} else {
		for (offset = 0x0; offset <= 0x74; offset += 0x4) {
			if (offset == 0xC || offset == 0x20 || offset == 0x24)
				continue;
			val = cam_soc_util_r(camif_lite_priv->soc_info,
				0, offset);
			CAM_INFO(CAM_ISP, "offset 0x%X value 0x%X",
				offset, val);
		}
	}

	if (camif_lite_res->res_id != CAM_ISP_HW_VFE_IN_RDI0)
		goto dump_rdi_1;

	CAM_INFO(CAM_ISP, "IFE:%d RDI0 CAMIF",
		camif_lite_priv->hw_intf->hw_idx);
	if (!soc_private->is_ife_lite) {
		for (offset = 0x9A00; offset <= 0x9BFC; offset += 0x4) {
			if (offset == 0x9A08)
				offset = 0x9A60;
			val = cam_soc_util_r(camif_lite_priv->soc_info,
				0, offset);
			CAM_INFO(CAM_ISP, "offset 0x%X value 0x%X",
				offset, val);
			if (offset == 0x9A60)
				offset = 0x9A64;
			else if (offset == 0x9A70)
				offset = 0x9AEC;
		}
	} else {
		for (offset = 0x1200; offset <= 0x13FC; offset += 0x4) {
			if (offset == 0x1208)
				offset = 0x1260;
			val = cam_soc_util_r(camif_lite_priv->soc_info,
				0, offset);
			CAM_INFO(CAM_ISP, "offset 0x%X value 0x%X",
				offset, val);
			if (offset == 0x1260)
				offset = 0x1264;
			else if (offset == 0x1270)
				offset = 0x12EC;
		}
	}

	goto wr_dump;

dump_rdi_1:
	if (camif_lite_res->res_id != CAM_ISP_HW_VFE_IN_RDI1)
		goto dump_rdi_2;

	CAM_INFO(CAM_ISP, "IFE:%d RDI1 CAMIF",
		camif_lite_priv->hw_intf->hw_idx);
	if (!soc_private->is_ife_lite) {
		for (offset = 0x9C00; offset <= 0x9DFC; offset += 0x4) {
			if (offset == 0x9A08)
				offset = 0x9A60;
			val = cam_soc_util_r(camif_lite_priv->soc_info,
				0, offset);
			CAM_INFO(CAM_ISP, "offset 0x%X value 0x%X",
				offset, val);
			if (offset == 0x9A60)
				offset = 0x9A64;
			else if (offset == 0x9A70)
				offset = 0x9BEC;
		}
	} else {
		for (offset = 0x1400; offset <= 0x15FC; offset += 0x4) {
			if (offset == 0x1408)
				offset = 0x1460;
			val = cam_soc_util_r(camif_lite_priv->soc_info,
				0, offset);
			CAM_INFO(CAM_ISP, "offset 0x%X value 0x%X",
				offset, val);
			if (offset == 0x1460)
				offset = 0x1464;
			else if (offset == 0x1470)
				offset = 0x15EC;
		}
	}

	goto wr_dump;

dump_rdi_2:
	if (camif_lite_res->res_id != CAM_ISP_HW_VFE_IN_RDI2)
		goto dump_rdi_3;

	CAM_INFO(CAM_ISP, "IFE:%d RDI2 CAMIF",
		camif_lite_priv->hw_intf->hw_idx);
	if (!soc_private->is_ife_lite) {
		for (offset = 0x9E00; offset <= 0x9FFC; offset += 0x4) {
			if (offset == 0x9E08)
				offset = 0x9E60;
			val = cam_soc_util_r(camif_lite_priv->soc_info,
				0, offset);
			CAM_INFO(CAM_ISP, "offset 0x%X value 0x%X",
				offset, val);
			if (offset == 0x9E60)
				offset = 0x9E64;
			else if (offset == 0x9E80)
				offset = 0x9FEC;
		}
	} else {
		for (offset = 0x1600; offset <= 0x17FC; offset += 0x4) {
			if (offset == 0x1608)
				offset = 0x1660;
			val = cam_soc_util_r(camif_lite_priv->soc_info,
				0, offset);
			CAM_INFO(CAM_ISP, "offset 0x%X value 0x%X",
				offset, val);
			if (offset == 0x1660)
				offset = 0x1664;
			else if (offset == 0x1670)
				offset = 0x17EC;
		}
	}

	goto wr_dump;

dump_rdi_3:
	if (camif_lite_res->res_id != CAM_ISP_HW_VFE_IN_RDI3)
		goto dump_pdlib;

	CAM_INFO(CAM_ISP, "IFE:%d RDI3 CAMIF",
		camif_lite_priv->hw_intf->hw_idx);
	if (soc_private->is_ife_lite) {
		for (offset = 0x1800; offset <= 0x19FC; offset += 0x4) {
			if (offset == 0x1808)
				offset = 0x1860;
			val = cam_soc_util_r(camif_lite_priv->soc_info,
				0, offset);
			CAM_INFO(CAM_ISP, "offset 0x%X value 0x%X",
				offset, val);
			if (offset == 0x1860)
				offset = 0x1864;
			else if (offset == 0x1870)
				offset = 0x19EC;
		}
	}

	goto wr_dump;

dump_pdlib:
	if (camif_lite_res->res_id != CAM_ISP_HW_VFE_IN_PDLIB)
		goto dump_lcr;

	CAM_INFO(CAM_ISP, "IFE:%d PDLIB CAMIF",
		camif_lite_priv->hw_intf->hw_idx);
	for (offset = 0xA400; offset <= 0xA5FC; offset += 0x4) {
		if (offset == 0xA408)
			offset = 0xA460;
		val = cam_soc_util_r(camif_lite_priv->soc_info, 0, offset);
		CAM_INFO(CAM_ISP, "offset 0x%X value 0x%X",
			offset, val);
		if (offset == 0xA460)
			offset = 0xA464;
		else if (offset == 0xA470)
			offset = 0xA5EC;
	}

	CAM_INFO(CAM_ISP, "IFE:%d CLC PDLIB",
		camif_lite_priv->hw_intf->hw_idx);
	for (offset = 0xA600; offset <= 0xA718; offset += 0x4) {
		val = cam_soc_util_r(camif_lite_priv->soc_info, 0, offset);
		CAM_INFO(CAM_ISP, "offset 0x%X value 0x%X", offset, val);
	}

	goto wr_dump;

dump_lcr:
	CAM_INFO(CAM_ISP, "IFE:%d LCR CAMIF", camif_lite_priv->hw_intf->hw_idx);
	for (offset = 0xA000; offset <= 0xA1FC; offset += 0x4) {
		if (offset == 0xA008)
			offset = 0xA060;
		val = cam_soc_util_r(camif_lite_priv->soc_info, 0, offset);
		CAM_INFO(CAM_ISP, "offset 0x%X value 0x%X",
			offset, val);
		if (offset == 0xA060)
			offset = 0xA064;
		else if (offset == 0xA070)
			offset = 0xA1EC;
	}

	CAM_INFO(CAM_ISP, "IFE:%d CLC LCR", camif_lite_priv->hw_intf->hw_idx);
	for (offset = 0xA200; offset <= 0xA3FC; offset += 0x4) {
		if (offset == 0xA208)
			offset = 0xA260;
		else if (offset == 0xA288)
			offset = 0xA3F8;
		val = cam_soc_util_r(camif_lite_priv->soc_info, 0, offset);
		CAM_INFO(CAM_ISP, "offset 0x%X value 0x%X",
			offset, val);
		if (offset == 0xA260)
			offset = 0xA264;
		else if (offset == 0xA280)
			offset = 0xA1EC;
	}

wr_dump:
	if (!soc_private->is_ife_lite)
		goto end_dump;

	CAM_INFO(CAM_ISP, "IFE:%d LITE BUS WR",
		camif_lite_priv->hw_intf->hw_idx);
	for (offset = 0x1A00; offset <= 0x1AE0; offset += 0x4) {
		val = cam_soc_util_r(camif_lite_priv->soc_info, 0, offset);
		CAM_DBG(CAM_ISP, "offset 0x%X value 0x%X", offset, val);
	}

	for (wm_idx = 0; wm_idx <= 3; wm_idx++) {
		for (offset = 0x1C00 + 0x100 * wm_idx; offset < (0x1C00 +
			0x100 * wm_idx + 0x84); offset += 0x4) {
			val = cam_soc_util_r(camif_lite_priv->soc_info,
				0, offset);
			CAM_INFO(CAM_ISP, "offset 0x%X value 0x%X",
				offset, val);
		}
	}

end_dump:
	return 0;
}

static int cam_vfe_camif_lite_resource_stop(
	struct cam_isp_resource_node             *camif_lite_res)
{
	struct cam_vfe_mux_camif_lite_data       *rsrc_data;
	int                                       rc = 0;

	if (!camif_lite_res) {
		CAM_ERR(CAM_ISP, "Invalid input arguments");
		return -EINVAL;
	}

	CAM_DBG(CAM_ISP, "VFE:%d CAMIF LITE:%d Stop",
		camif_lite_res->hw_intf->hw_idx,
		camif_lite_res->res_id);

	if ((camif_lite_res->res_state == CAM_ISP_RESOURCE_STATE_RESERVED) ||
		(camif_lite_res->res_state == CAM_ISP_RESOURCE_STATE_AVAILABLE))
		return 0;

	rsrc_data =
		(struct cam_vfe_mux_camif_lite_data *)camif_lite_res->res_priv;

	/* Disable Camif */
	cam_io_w_mb(0x0, rsrc_data->mem_base +
		rsrc_data->camif_lite_reg->lite_module_config);

	if (camif_lite_res->res_state == CAM_ISP_RESOURCE_STATE_STREAMING)
		camif_lite_res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;

	if (rsrc_data->irq_handle > 0) {
		cam_irq_controller_unsubscribe_irq(
			rsrc_data->vfe_irq_controller,
			rsrc_data->irq_handle);
		rsrc_data->irq_handle = 0;
	}

	if (rsrc_data->sof_irq_handle > 0) {
		cam_irq_controller_unsubscribe_irq(
			rsrc_data->vfe_irq_controller,
			rsrc_data->sof_irq_handle);
		rsrc_data->sof_irq_handle = 0;
	}

	if (rsrc_data->irq_err_handle > 0) {
		cam_irq_controller_unsubscribe_irq(
			rsrc_data->vfe_irq_controller,
			rsrc_data->irq_err_handle);
		rsrc_data->irq_err_handle = 0;
	}

	return rc;
}

static int cam_vfe_camif_lite_ver3_core_config(
	struct cam_isp_resource_node *rsrc_node, void *cmd_args)
{
	struct cam_vfe_mux_camif_lite_data *camif_lite_priv;
	struct cam_vfe_core_config_args *vfe_core_cfg =
		(struct cam_vfe_core_config_args *)cmd_args;

	camif_lite_priv =
		(struct cam_vfe_mux_camif_lite_data *)rsrc_node->res_priv;
	camif_lite_priv->cam_common_cfg.vid_ds16_r2pd =
		vfe_core_cfg->core_config.vid_ds16_r2pd;
	camif_lite_priv->cam_common_cfg.vid_ds4_r2pd =
		vfe_core_cfg->core_config.vid_ds4_r2pd;
	camif_lite_priv->cam_common_cfg.disp_ds16_r2pd =
		vfe_core_cfg->core_config.disp_ds16_r2pd;
	camif_lite_priv->cam_common_cfg.disp_ds4_r2pd =
		vfe_core_cfg->core_config.disp_ds4_r2pd;
	camif_lite_priv->cam_common_cfg.dsp_streaming_tap_point =
		vfe_core_cfg->core_config.dsp_streaming_tap_point;
	camif_lite_priv->cam_common_cfg.ihist_src_sel =
		vfe_core_cfg->core_config.ihist_src_sel;
	camif_lite_priv->cam_common_cfg.hdr_be_src_sel =
		vfe_core_cfg->core_config.hdr_be_src_sel;
	camif_lite_priv->cam_common_cfg.hdr_bhist_src_sel =
		vfe_core_cfg->core_config.hdr_bhist_src_sel;
	camif_lite_priv->cam_common_cfg.input_mux_sel_pdaf =
		vfe_core_cfg->core_config.input_mux_sel_pdaf;
	camif_lite_priv->cam_common_cfg.input_mux_sel_pp =
		vfe_core_cfg->core_config.input_mux_sel_pp;

	return 0;
}

static int cam_vfe_camif_lite_process_cmd(
	struct cam_isp_resource_node *rsrc_node,
	uint32_t cmd_type, void *cmd_args, uint32_t arg_size)
{
	int rc = -EINVAL;
	struct cam_vfe_mux_camif_lite_data *camif_lite_priv = NULL;

	if (!rsrc_node || !cmd_args) {
		CAM_ERR(CAM_ISP, "Invalid input arguments");
		return -EINVAL;
	}

	switch (cmd_type) {
	case CAM_ISP_HW_CMD_GET_REG_UPDATE:
		rc = cam_vfe_camif_lite_get_reg_update(rsrc_node, cmd_args,
			arg_size);
		break;
	case CAM_ISP_HW_CMD_CORE_CONFIG:
		rc = cam_vfe_camif_lite_ver3_core_config(rsrc_node, cmd_args);
		break;
	case CAM_ISP_HW_CMD_SET_CAMIF_DEBUG:
		camif_lite_priv = (struct cam_vfe_mux_camif_lite_data *)
			rsrc_node->res_priv;
		camif_lite_priv->camif_debug = *((uint32_t *)cmd_args);
		break;
	default:
		CAM_ERR(CAM_ISP,
			"unsupported process command:%d", cmd_type);
		break;
	}

	return rc;
}

static void cam_vfe_camif_lite_overflow_debug_info(
	struct cam_vfe_mux_camif_lite_data *camif_lite_priv)
{
	struct cam_vfe_soc_private *soc_private = NULL;
	uint32_t val0, val1, val2, val3;

	soc_private = camif_lite_priv->soc_info->soc_private;

	val0 = cam_io_r(camif_lite_priv->mem_base +
		camif_lite_priv->common_reg->top_debug_0);
	val1 = cam_io_r(camif_lite_priv->mem_base +
		camif_lite_priv->common_reg->top_debug_1);
	val2 = cam_io_r(camif_lite_priv->mem_base +
		camif_lite_priv->common_reg->top_debug_2);
	val3 = cam_io_r(camif_lite_priv->mem_base +
		camif_lite_priv->common_reg->top_debug_3);
	CAM_INFO(CAM_ISP,
		"status_0: 0x%X status_1: 0x%X status_2: 0x%X status_3: 0x%X",
		val0, val1, val2, val3);

	if (soc_private->is_ife_lite)
		return;

	val0 = cam_io_r(camif_lite_priv->mem_base +
		camif_lite_priv->common_reg->top_debug_4);
	val1 = cam_io_r(camif_lite_priv->mem_base +
		camif_lite_priv->common_reg->top_debug_5);
	val2 = cam_io_r(camif_lite_priv->mem_base +
		camif_lite_priv->common_reg->top_debug_6);
	val3 = cam_io_r(camif_lite_priv->mem_base +
		camif_lite_priv->common_reg->top_debug_7);
	CAM_INFO(CAM_ISP,
		"status_4: 0x%X status_5: 0x%X status_6: 0x%X status_7: 0x%X",
		val0, val1, val2, val3);
	val0 = cam_io_r(camif_lite_priv->mem_base +
		camif_lite_priv->common_reg->top_debug_8);
	val1 = cam_io_r(camif_lite_priv->mem_base +
		camif_lite_priv->common_reg->top_debug_9);
	val2 = cam_io_r(camif_lite_priv->mem_base +
		camif_lite_priv->common_reg->top_debug_10);
	val3 = cam_io_r(camif_lite_priv->mem_base +
		camif_lite_priv->common_reg->top_debug_11);
	CAM_INFO(CAM_ISP,
		"status_8: 0x%X status_9: 0x%X status_10: 0x%X status_11: 0x%X",
		val0, val1, val2, val3);
	val0 = cam_io_r(camif_lite_priv->mem_base +
		camif_lite_priv->common_reg->top_debug_12);
	val1 = cam_io_r(camif_lite_priv->mem_base +
		camif_lite_priv->common_reg->top_debug_13);
	CAM_INFO(CAM_ISP, "status_12: 0x%X status_13: 0x%X",
		val0, val1);
}

static void cam_vfe_camif_lite_print_status(uint32_t *status,
	int err_type, struct cam_vfe_mux_camif_lite_data *camif_lite_priv)
{
	uint32_t violation_mask = 0x3F00, violation_status = 0;
	uint32_t bus_overflow_status = 0, status_0 = 0, status_2 = 0;
	struct cam_vfe_soc_private *soc_private = NULL;
	uint32_t val0, val1, val2;

	if (!status) {
		CAM_ERR(CAM_ISP, "Invalid params");
		return;
	}

	bus_overflow_status = status[CAM_IFE_IRQ_BUS_OVERFLOW_STATUS];
	violation_status = status[CAM_IFE_IRQ_VIOLATION_STATUS];
	status_0 = status[CAM_IFE_IRQ_CAMIF_REG_STATUS0];
	status_2 = status[CAM_IFE_IRQ_CAMIF_REG_STATUS2];
	soc_private = camif_lite_priv->soc_info->soc_private;

	if (soc_private->is_ife_lite)
		goto ife_lite;

	if (err_type == CAM_VFE_IRQ_STATUS_OVERFLOW) {
		if (status_0 & 0x200000)
			CAM_INFO(CAM_ISP, "RDI2 FRAME DROP");

		if (status_0 & 0x400000)
			CAM_INFO(CAM_ISP, "RDI1 FRAME DROP");

		if (status_0 & 0x800000)
			CAM_INFO(CAM_ISP, "RDI0 FRAME DROP");

		if (status_0 & 0x1000000)
			CAM_INFO(CAM_ISP, "PD PIPE FRAME DROP");

		if (status_0 & 0x8000000)
			CAM_INFO(CAM_ISP, "RDI2 OVERFLOW");

		if (status_0 & 0x10000000)
			CAM_INFO(CAM_ISP, "RDI1 OVERFLOW");

		if (status_0 & 0x20000000)
			CAM_INFO(CAM_ISP, "RDI0 OVERFLOW");

		if (status_0 & 0x40000000)
			CAM_INFO(CAM_ISP, "PD PIPE OVERFLOW");
	}

	if (err_type == CAM_VFE_IRQ_STATUS_OVERFLOW && bus_overflow_status) {
		if (bus_overflow_status & 0x0800)
			CAM_INFO(CAM_ISP, "CAMIF PD BUS OVERFLOW");

		if (bus_overflow_status & 0x0400000)
			CAM_INFO(CAM_ISP, "LCR BUS OVERFLOW");

		if (bus_overflow_status & 0x0800000)
			CAM_INFO(CAM_ISP, "RDI0 BUS OVERFLOW");

		if (bus_overflow_status & 0x01000000)
			CAM_INFO(CAM_ISP, "RDI1 BUS OVERFLOW");

		if (bus_overflow_status & 0x02000000)
			CAM_INFO(CAM_ISP, "RDI2 BUS OVERFLOW");

		cam_cpas_reg_read(soc_private->cpas_handle,
			CAM_CPAS_REG_CAMNOC, 0xA20, true, &val0);
		cam_cpas_reg_read(soc_private->cpas_handle,
			CAM_CPAS_REG_CAMNOC, 0x1420, true, &val1);
		cam_cpas_reg_read(soc_private->cpas_handle,
			CAM_CPAS_REG_CAMNOC, 0x1A20, true, &val2);
		CAM_INFO(CAM_ISP,
			"CAMNOC REG ife_linear: 0x%X ife_rdi_wr: 0x%X ife_ubwc_stats: 0x%X",
			val0, val1, val2);
	}

	if (err_type == CAM_VFE_IRQ_STATUS_OVERFLOW && !bus_overflow_status) {
		CAM_INFO(CAM_ISP, "PDLIB / LCR Module hang");
		/* print debug registers */
		cam_vfe_camif_lite_overflow_debug_info(camif_lite_priv);
		return;
	}

	if (err_type == CAM_VFE_IRQ_STATUS_VIOLATION) {
		if (status_2 & 0x02000)
			CAM_INFO(CAM_ISP, "PD CAMIF VIOLATION");

		if (status_2 & 0x04000)
			CAM_INFO(CAM_ISP, "PD VIOLATION");

		if (status_2 & 0x08000)
			CAM_INFO(CAM_ISP, "LCR CAMIF VIOLATION");

		if (status_2 & 0x010000)
			CAM_INFO(CAM_ISP, "LCR VIOLATION");

		if (status_2 & 0x020000)
			CAM_INFO(CAM_ISP, "RDI0 CAMIF VIOLATION");

		if (status_2 & 0x040000)
			CAM_INFO(CAM_ISP, "RDI1 CAMIF VIOLATION");

		if (status_2 & 0x080000)
			CAM_INFO(CAM_ISP, "RDI2 CAMIF VIOLATION");
	}

	if (err_type == CAM_VFE_IRQ_STATUS_VIOLATION && violation_status) {
		if (violation_mask & violation_status)
			CAM_INFO(CAM_ISP, "LCR VIOLATION Module ID:%d",
				violation_mask & violation_status);

		violation_mask = 0x0F0000;
		if (violation_mask & violation_status)
			CAM_INFO(CAM_ISP, "PD VIOLATION Module ID:%d",
				violation_mask & violation_status);

	}

	return;

ife_lite:
	if (err_type == CAM_VFE_IRQ_STATUS_OVERFLOW) {
		if (status_0 & 0x100)
			CAM_INFO(CAM_ISP, "RDI3 FRAME DROP");

		if (status_0 & 0x80)
			CAM_INFO(CAM_ISP, "RDI2 FRAME DROP");

		if (status_0 & 0x40)
			CAM_INFO(CAM_ISP, "RDI1 FRAME DROP");

		if (status_0 & 0x20)
			CAM_INFO(CAM_ISP, "RDI0 FRAME DROP");

		if (status_0 & 0x8)
			CAM_INFO(CAM_ISP, "RDI3 OVERFLOW");

		if (status_0 & 0x4)
			CAM_INFO(CAM_ISP, "RDI2 OVERFLOW");

		if (status_0 & 0x2)
			CAM_INFO(CAM_ISP, "RDI1 OVERFLOW");

		if (status_0 & 0x1)
			CAM_INFO(CAM_ISP, "RDI0 OVERFLOW");
	}

	if (err_type == CAM_VFE_IRQ_STATUS_OVERFLOW && bus_overflow_status) {
		if (bus_overflow_status & 0x01)
			CAM_INFO(CAM_ISP, "RDI0 BUS OVERFLOW");

		if (bus_overflow_status & 0x02)
			CAM_INFO(CAM_ISP, "RDI1 BUS OVERFLOW");

		if (bus_overflow_status & 0x04)
			CAM_INFO(CAM_ISP, "RDI2 BUS OVERFLOW");

		if (bus_overflow_status & 0x08)
			CAM_INFO(CAM_ISP, "RDI3 BUS OVERFLOW");

		cam_cpas_reg_read(soc_private->cpas_handle,
			CAM_CPAS_REG_CAMNOC, 0xA20, true, &val0);
		cam_cpas_reg_read(soc_private->cpas_handle,
			CAM_CPAS_REG_CAMNOC, 0x1420, true, &val1);
		cam_cpas_reg_read(soc_private->cpas_handle,
			CAM_CPAS_REG_CAMNOC, 0x1A20, true, &val2);
		CAM_INFO(CAM_ISP,
			"CAMNOC REG ife_linear: 0x%X ife_rdi_wr: 0x%X ife_ubwc_stats: 0x%X",
			val0, val1, val2);
	}

	if (err_type == CAM_VFE_IRQ_STATUS_OVERFLOW && !bus_overflow_status) {
		CAM_INFO(CAM_ISP, "RDI hang");
		/* print debug registers */
		cam_vfe_camif_lite_overflow_debug_info(camif_lite_priv);
		return;
	}

	if (err_type == CAM_VFE_IRQ_STATUS_VIOLATION) {
		if (status_2 & 0x100)
			CAM_INFO(CAM_ISP, "RDI0 CAMIF VIOLATION");

		if (status_2 & 0x200)
			CAM_INFO(CAM_ISP, "RDI1 CAMIF VIOLATION");

		if (status_2 & 0x400)
			CAM_INFO(CAM_ISP, "RDI2 CAMIF VIOLATION");

		if (status_2 & 0x800)
			CAM_INFO(CAM_ISP, "RDI3 CAMIF VIOLATION");
	}
}

static int cam_vfe_camif_lite_handle_irq_top_half(uint32_t evt_id,
	struct cam_irq_th_payload *th_payload)
{
	int32_t                                rc;
	int                                    i;
	struct cam_isp_resource_node          *camif_lite_node;
	struct cam_vfe_mux_camif_lite_data    *camif_lite_priv;
	struct cam_vfe_top_irq_evt_payload    *evt_payload;

	camif_lite_node = th_payload->handler_priv;
	camif_lite_priv = camif_lite_node->res_priv;

	CAM_DBG(CAM_ISP,
		"VFE:%d CAMIF LITE:%d IRQ status_0: 0x%X status_1: 0x%X status_2: 0x%X",
		camif_lite_node->hw_intf->hw_idx,
		camif_lite_node->res_id,
		th_payload->evt_status_arr[0],
		th_payload->evt_status_arr[1],
		th_payload->evt_status_arr[2]);

	rc  = cam_vfe_camif_lite_get_evt_payload(camif_lite_priv, &evt_payload);
	if (rc) {
		CAM_INFO_RATE_LIMIT(CAM_ISP,
			"VFE:%d CAMIF LITE:%d IRQ status_0: 0x%X status_1: 0x%X status_2: 0x%X",
			camif_lite_node->hw_intf->hw_idx,
			camif_lite_node->res_id,
			th_payload->evt_status_arr[0],
			th_payload->evt_status_arr[1],
			th_payload->evt_status_arr[2]);
		return rc;
	}

	cam_isp_hw_get_timestamp(&evt_payload->ts);

	for (i = 0; i < th_payload->num_registers; i++)
		evt_payload->irq_reg_val[i] = th_payload->evt_status_arr[i];

	th_payload->evt_payload_priv = evt_payload;

	CAM_DBG(CAM_ISP, "Exit");
	return rc;
}

static int cam_vfe_camif_lite_handle_irq_bottom_half(
	void                                    *handler_priv,
	void                                    *evt_payload_priv)
{
	int ret = CAM_VFE_IRQ_STATUS_MAX;
	struct cam_isp_resource_node *camif_lite_node;
	struct cam_vfe_mux_camif_lite_data *camif_lite_priv;
	struct cam_vfe_top_irq_evt_payload *payload;
	struct cam_isp_hw_event_info evt_info;
	struct cam_vfe_soc_private *soc_private = NULL;
	uint32_t irq_status[CAM_IFE_IRQ_REGISTERS_MAX] = {0};
	int i = 0;

	if (!handler_priv || !evt_payload_priv) {
		CAM_ERR(CAM_ISP, "Invalid params");
		return ret;
	}

	camif_lite_node = handler_priv;
	camif_lite_priv = camif_lite_node->res_priv;
	payload         = evt_payload_priv;

	soc_private = camif_lite_priv->soc_info->soc_private;
	if (!soc_private) {
		CAM_ERR(CAM_ISP, "Invalid soc_private");
		return -ENODEV;
	}

	for (i = 0; i < CAM_IFE_IRQ_REGISTERS_MAX; i++)
		irq_status[i] = payload->irq_reg_val[i];

	evt_info.hw_idx   = camif_lite_node->hw_intf->hw_idx;
	evt_info.res_id   = camif_lite_node->res_id;
	evt_info.res_type = camif_lite_node->res_type;

	CAM_DBG(CAM_ISP,
		"VFE:%d CAMIF LITE:%d IRQ status_0: 0x%X status_1: 0x%X status_2: 0x%X",
		evt_info.hw_idx, evt_info.res_id,
		irq_status[CAM_IFE_IRQ_CAMIF_REG_STATUS0],
		irq_status[CAM_IFE_IRQ_CAMIF_REG_STATUS1],
		irq_status[CAM_IFE_IRQ_CAMIF_REG_STATUS2]);

	if (irq_status[CAM_IFE_IRQ_CAMIF_REG_STATUS1]
		& camif_lite_priv->reg_data->sof_irq_mask) {
		CAM_DBG(CAM_ISP, "VFE:%d CAMIF LITE:%d Received SOF",
			evt_info.hw_idx, evt_info.res_id);
		ret = CAM_VFE_IRQ_STATUS_SUCCESS;

		if (camif_lite_priv->event_cb)
			camif_lite_priv->event_cb(camif_lite_priv->priv,
				CAM_ISP_HW_EVENT_SOF, (void *)&evt_info);
	}

	if (irq_status[CAM_IFE_IRQ_CAMIF_REG_STATUS1]
		& camif_lite_priv->reg_data->epoch0_irq_mask) {
		CAM_DBG(CAM_ISP, "VFE:%d CAMIF LITE:%d Received EPOCH",
			evt_info.hw_idx, evt_info.res_id);
		ret = CAM_VFE_IRQ_STATUS_SUCCESS;

		if (camif_lite_priv->event_cb)
			camif_lite_priv->event_cb(camif_lite_priv->priv,
				CAM_ISP_HW_EVENT_EPOCH, (void *)&evt_info);
	}

	if (irq_status[CAM_IFE_IRQ_CAMIF_REG_STATUS1]
		& camif_lite_priv->reg_data->eof_irq_mask) {
		CAM_DBG(CAM_ISP, "VFE:%d CAMIF LITE:%d Received EOF",
			evt_info.hw_idx, evt_info.res_id);
		ret = CAM_VFE_IRQ_STATUS_SUCCESS;

		if (camif_lite_priv->event_cb)
			camif_lite_priv->event_cb(camif_lite_priv->priv,
				CAM_ISP_HW_EVENT_EOF, (void *)&evt_info);
	}

	if (irq_status[CAM_IFE_IRQ_CAMIF_REG_STATUS0]
		& camif_lite_priv->reg_data->error_irq_mask0) {
		CAM_ERR(CAM_ISP, "VFE:%d Overflow",
			camif_lite_node->hw_intf->hw_idx);

		evt_info.err_type = CAM_VFE_IRQ_STATUS_OVERFLOW;

		if (camif_lite_priv->event_cb)
			camif_lite_priv->event_cb(camif_lite_priv->priv,
				CAM_ISP_HW_EVENT_ERROR, (void *)&evt_info);

		ret = CAM_VFE_IRQ_STATUS_OVERFLOW;

		cam_vfe_camif_lite_print_status(irq_status, ret,
			camif_lite_priv);

		if (camif_lite_priv->camif_debug & CAMIF_DEBUG_ENABLE_REG_DUMP)
			cam_vfe_camif_lite_reg_dump(camif_lite_node);
	}

	if (irq_status[CAM_IFE_IRQ_CAMIF_REG_STATUS2]) {
		CAM_ERR(CAM_ISP, "VFE:%d Violation",
			camif_lite_node->hw_intf->hw_idx);

		evt_info.err_type = CAM_VFE_IRQ_STATUS_VIOLATION;

		if (camif_lite_priv->event_cb)
			camif_lite_priv->event_cb(camif_lite_priv->priv,
				CAM_ISP_HW_EVENT_ERROR, (void *)&evt_info);

		ret = CAM_VFE_IRQ_STATUS_VIOLATION;

		cam_vfe_camif_lite_print_status(irq_status, ret,
			camif_lite_priv);

		if (camif_lite_priv->camif_debug & CAMIF_DEBUG_ENABLE_REG_DUMP)
			cam_vfe_camif_lite_reg_dump(camif_lite_node);
	}

	cam_vfe_camif_lite_put_evt_payload(camif_lite_priv, &payload);

	CAM_DBG(CAM_ISP, "returning status = %d", ret);
	return ret;
}

int cam_vfe_camif_lite_ver3_init(
	struct cam_hw_intf            *hw_intf,
	struct cam_hw_soc_info        *soc_info,
	void                          *camif_lite_hw_info,
	struct cam_isp_resource_node  *camif_lite_node,
	void                          *vfe_irq_controller)
{
	struct cam_vfe_mux_camif_lite_data       *camif_lite_priv = NULL;
	struct cam_vfe_camif_lite_ver3_hw_info   *camif_lite_info =
		camif_lite_hw_info;
	int                                       i = 0;

	CAM_DBG(CAM_ISP, "VFE:%d CAMIF LITE:%d Init",
		camif_lite_node->res_id, camif_lite_node->res_id);

	camif_lite_priv = kzalloc(sizeof(*camif_lite_priv),
		GFP_KERNEL);
	if (!camif_lite_priv)
		return -ENOMEM;

	camif_lite_node->res_priv = camif_lite_priv;

	camif_lite_priv->mem_base         =
		soc_info->reg_map[VFE_CORE_BASE_IDX].mem_base;
	camif_lite_priv->camif_lite_reg     = camif_lite_info->camif_lite_reg;
	camif_lite_priv->common_reg         = camif_lite_info->common_reg;
	camif_lite_priv->reg_data           = camif_lite_info->reg_data;
	camif_lite_priv->hw_intf            = hw_intf;
	camif_lite_priv->soc_info           = soc_info;
	camif_lite_priv->vfe_irq_controller = vfe_irq_controller;

	camif_lite_node->init    = NULL;
	camif_lite_node->deinit  = NULL;
	camif_lite_node->start   = cam_vfe_camif_lite_resource_start;
	camif_lite_node->stop    = cam_vfe_camif_lite_resource_stop;
	camif_lite_node->process_cmd = cam_vfe_camif_lite_process_cmd;
	camif_lite_node->top_half_handler =
		cam_vfe_camif_lite_handle_irq_top_half;
	camif_lite_node->bottom_half_handler =
		cam_vfe_camif_lite_handle_irq_bottom_half;

	spin_lock_init(&camif_lite_priv->spin_lock);
	INIT_LIST_HEAD(&camif_lite_priv->free_payload_list);
	for (i = 0; i < CAM_VFE_CAMIF_LITE_EVT_MAX; i++) {
		INIT_LIST_HEAD(&camif_lite_priv->evt_payload[i].list);
		list_add_tail(&camif_lite_priv->evt_payload[i].list,
			&camif_lite_priv->free_payload_list);
	}

	return 0;
}

int cam_vfe_camif_lite_ver3_deinit(
	struct cam_isp_resource_node  *camif_lite_node)
{
	struct cam_vfe_mux_camif_lite_data *camif_lite_priv =
		camif_lite_node->res_priv;
	int                                 i = 0;

	CAM_DBG(CAM_ISP, "VFE:%d CAMIF LITE:%d Deinit",
		camif_lite_node->hw_intf->hw_idx, camif_lite_node->res_id);

	INIT_LIST_HEAD(&camif_lite_priv->free_payload_list);
	for (i = 0; i < CAM_VFE_CAMIF_LITE_EVT_MAX; i++)
		INIT_LIST_HEAD(&camif_lite_priv->evt_payload[i].list);

	camif_lite_node->start = NULL;
	camif_lite_node->stop  = NULL;
	camif_lite_node->process_cmd = NULL;
	camif_lite_node->top_half_handler = NULL;
	camif_lite_node->bottom_half_handler = NULL;

	camif_lite_node->res_priv = NULL;

	if (!camif_lite_priv) {
		CAM_ERR(CAM_ISP, "Error. camif_priv is NULL");
		return -ENODEV;
	}

	kfree(camif_lite_priv);

	return 0;
}

