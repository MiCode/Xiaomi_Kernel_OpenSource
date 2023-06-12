// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/slab.h>
#include <media/cam_isp.h>
#include "cam_io_util.h"
#include "cam_isp_hw_mgr_intf.h"
#include "cam_isp_hw.h"
#include "cam_vfe_hw_intf.h"
#include "cam_vfe_soc.h"
#include "cam_vfe_top.h"
#include "cam_vfe_top_ver2.h"
#include "cam_irq_controller.h"
#include "cam_tasklet_util.h"
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

	cam_hw_mgr_event_cb_func              event_cb;
	void                                 *priv;
	int                                   irq_err_handle;
	int                                   irq_handle;
	void                                 *vfe_irq_controller;
	struct cam_vfe_top_irq_evt_payload
		evt_payload[CAM_VFE_CAMIF_LITE_EVT_MAX];
	struct list_head                      free_payload_list;
	spinlock_t                            spin_lock;
	struct timeval                        error_ts;
};

static int cam_vfe_camif_lite_get_evt_payload(
	struct cam_vfe_mux_camif_lite_data       *camif_lite_priv,
	struct cam_vfe_top_irq_evt_payload    **evt_payload)
{
	int rc = 0;

	spin_lock(&camif_lite_priv->spin_lock);
	if (list_empty(&camif_lite_priv->free_payload_list)) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "No free payload");
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
	struct cam_vfe_mux_camif_lite_data       *camif_lite_priv,
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
	bool                                   error_flag = false;

	CAM_DBG(CAM_ISP, "IRQ status_0 = %x, IRQ status_1 = %x",
		th_payload->evt_status_arr[0], th_payload->evt_status_arr[1]);

	camif_lite_node = th_payload->handler_priv;
	camif_lite_priv = camif_lite_node->res_priv;
	/*
	 *  need to handle overflow condition here, otherwise irq storm
	 *  will block everything
	 */
	if (th_payload->evt_status_arr[1] || (th_payload->evt_status_arr[0] &
		camif_lite_priv->reg_data->lite_err_irq_mask0)) {
		CAM_ERR(CAM_ISP,
			"CAMIF LITE ERR VFE:%d IRQ STATUS_0=0x%x STATUS_1=0x%x",
			camif_lite_node->hw_intf->hw_idx,
			th_payload->evt_status_arr[0],
			th_payload->evt_status_arr[1]);
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
	if (rc) {
		CAM_ERR_RATE_LIMIT(CAM_ISP,
			"No tasklet_cmd is free in queue");
		CAM_ERR_RATE_LIMIT(CAM_ISP, "IRQ STATUS_0=0x%x STATUS_1=0x%x",
			th_payload->evt_status_arr[0],
			th_payload->evt_status_arr[1]);
		return rc;
	}

	cam_isp_hw_get_timestamp(&evt_payload->ts);
	if (error_flag) {
		camif_lite_priv->error_ts.tv_sec =
			evt_payload->ts.mono_time.tv_sec;
		camif_lite_priv->error_ts.tv_usec =
			evt_payload->ts.mono_time.tv_usec;
	}

	for (i = 0; i < th_payload->num_registers; i++)
		evt_payload->irq_reg_val[i] = th_payload->evt_status_arr[i];

	evt_payload->irq_reg_val[i] = cam_io_r(camif_lite_priv->mem_base +
		camif_lite_priv->common_reg->violation_status);

	if (error_flag)
		CAM_INFO(CAM_ISP, "Violation status = 0x%x",
			evt_payload->irq_reg_val[i]);

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
	camif_lite_data->event_cb    = acquire_data->event_cb;
	camif_lite_data->priv        = acquire_data->priv;

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
	int                                   rc = 0;
	uint32_t err_irq_mask[CAM_IFE_IRQ_REGISTERS_MAX];

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

	err_irq_mask[CAM_IFE_IRQ_CAMIF_REG_STATUS0] =
		rsrc_data->reg_data->lite_err_irq_mask0;
	err_irq_mask[CAM_IFE_IRQ_CAMIF_REG_STATUS1] =
		rsrc_data->reg_data->lite_err_irq_mask1;

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

	rsrc_data->error_ts.tv_sec = 0;
	rsrc_data->error_ts.tv_usec = 0;

	CAM_DBG(CAM_ISP, "Start Camif Lite IFE %d Done",
		camif_lite_res->hw_intf->hw_idx);
	return rc;
}

static int cam_vfe_camif_lite_resource_stop(
	struct cam_isp_resource_node             *camif_lite_res)
{
	struct cam_vfe_mux_camif_lite_data       *camif_lite_priv;
	int                                       rc = 0;

	if (!camif_lite_res) {
		CAM_ERR(CAM_ISP, "Error! Invalid input arguments");
		return -EINVAL;
	}

	if ((camif_lite_res->res_state == CAM_ISP_RESOURCE_STATE_RESERVED) ||
		(camif_lite_res->res_state == CAM_ISP_RESOURCE_STATE_AVAILABLE))
		return 0;

	camif_lite_priv = (struct cam_vfe_mux_camif_lite_data *)camif_lite_res;

	if (camif_lite_res->res_state == CAM_ISP_RESOURCE_STATE_STREAMING)
		camif_lite_res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;

	if (camif_lite_priv->irq_handle) {
		cam_irq_controller_unsubscribe_irq(
			camif_lite_priv->vfe_irq_controller,
			camif_lite_priv->irq_handle);
		camif_lite_priv->irq_handle = 0;
	}

	if (camif_lite_priv->irq_err_handle) {
		cam_irq_controller_unsubscribe_irq(
			camif_lite_priv->vfe_irq_controller,
			camif_lite_priv->irq_err_handle);
		camif_lite_priv->irq_err_handle = 0;
	}

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
	case CAM_ISP_HW_CMD_SET_CAMIF_DEBUG:
		break;
	case CAM_ISP_HW_CMD_BLANKING_UPDATE:
		rc = 0;
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
	int32_t                                rc;
	int                                    i;
	struct cam_isp_resource_node          *camif_lite_node;
	struct cam_vfe_mux_camif_lite_data    *camif_lite_priv;
	struct cam_vfe_top_irq_evt_payload  *evt_payload;

	camif_lite_node = th_payload->handler_priv;
	camif_lite_priv = camif_lite_node->res_priv;

	CAM_DBG(CAM_ISP, "IRQ status_0 = %x", th_payload->evt_status_arr[0]);
	CAM_DBG(CAM_ISP, "IRQ status_1 = %x", th_payload->evt_status_arr[1]);

	rc  = cam_vfe_camif_lite_get_evt_payload(camif_lite_priv, &evt_payload);
	if (rc) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "No tasklet_cmd is free in queue");
		CAM_ERR_RATE_LIMIT(CAM_ISP, "IRQ status0=0x%x status1=0x%x",
			th_payload->evt_status_arr[0],
			th_payload->evt_status_arr[1]);
		return rc;
	}

	cam_isp_hw_get_timestamp(&evt_payload->ts);

	for (i = 0; i < th_payload->num_registers; i++)
		evt_payload->irq_reg_val[i] = th_payload->evt_status_arr[i];

	th_payload->evt_payload_priv = evt_payload;

	CAM_DBG(CAM_ISP, "Exit");
	return rc;
}

static int cam_vfe_camif_lite_cpas_fifo_levels_reg_dump(
	struct cam_vfe_mux_camif_lite_data *camif_lite_priv)
{
	int rc = 0;
	struct cam_vfe_soc_private *soc_private =
		camif_lite_priv->soc_info->soc_private;
	uint32_t  val;

	if (soc_private->cpas_version == CAM_CPAS_TITAN_175_V120 ||
		soc_private->cpas_version == CAM_CPAS_TITAN_175_V130 ||
		soc_private->cpas_version == CAM_CPAS_TITAN_165_V100) {
		rc = cam_cpas_reg_read(soc_private->cpas_handle,
				CAM_CPAS_REG_CAMNOC, 0x3A20, true, &val);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"IFE0_nRDI_MAXWR_LOW read failed rc=%d",
				rc);
			return rc;
		}
		CAM_INFO(CAM_ISP, "IFE0_nRDI_MAXWR_LOW offset 0x3A20 val 0x%x",
			val);

		rc = cam_cpas_reg_read(soc_private->cpas_handle,
				CAM_CPAS_REG_CAMNOC, 0x5420, true, &val);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"IFE1_nRDI_MAXWR_LOW read failed rc=%d",
				rc);
			return rc;
		}
		CAM_INFO(CAM_ISP, "IFE1_nRDI_MAXWR_LOW offset 0x5420 val 0x%x",
			val);

		rc = cam_cpas_reg_read(soc_private->cpas_handle,
				CAM_CPAS_REG_CAMNOC, 0x3620, true, &val);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"IFE0123_RDI_WR_MAXWR_LOW read failed rc=%d",
				rc);
			return rc;
		}
		CAM_INFO(CAM_ISP,
			"IFE0123_RDI_WR_MAXWR_LOW offset 0x3620 val 0x%x", val);

	} else if (soc_private->cpas_version < CAM_CPAS_TITAN_175_V120) {
		rc = cam_cpas_reg_read(soc_private->cpas_handle,
				CAM_CPAS_REG_CAMNOC, 0x420, true, &val);
		if (rc) {
			CAM_ERR(CAM_ISP, "IFE02_MAXWR_LOW read failed rc=%d",
				rc);
			return rc;
		}
		CAM_INFO(CAM_ISP, "IFE02_MAXWR_LOW offset 0x420 val 0x%x", val);

		rc = cam_cpas_reg_read(soc_private->cpas_handle,
				CAM_CPAS_REG_CAMNOC, 0x820, true, &val);
		if (rc) {
			CAM_ERR(CAM_ISP, "IFE13_MAXWR_LOW read failed rc=%d",
				rc);
			return rc;
		}
		CAM_INFO(CAM_ISP, "IFE13_MAXWR_LOW offset 0x820 val 0x%x", val);
	}

	return 0;
}

static int cam_vfe_camif_lite_handle_irq_bottom_half(
	void                                 *handler_priv,
	void                                 *evt_payload_priv)
{
	int                                   ret = CAM_VFE_IRQ_STATUS_MAX;
	struct cam_isp_resource_node         *camif_lite_node;
	struct cam_vfe_mux_camif_lite_data   *camif_lite_priv;
	struct cam_vfe_top_irq_evt_payload   *payload;
	struct cam_isp_hw_event_info          evt_info;
	uint32_t                              irq_status0;
	uint32_t                              irq_status1;
	struct cam_hw_soc_info               *soc_info = NULL;
	struct cam_vfe_soc_private           *soc_private = NULL;
	struct timespec64                     ts;

	if (!handler_priv || !evt_payload_priv) {
		CAM_ERR(CAM_ISP, "Invalid params");
		return ret;
	}

	camif_lite_node = handler_priv;
	camif_lite_priv = camif_lite_node->res_priv;
	payload = evt_payload_priv;
	irq_status0 = payload->irq_reg_val[CAM_IFE_IRQ_CAMIF_REG_STATUS0];
	irq_status1 = payload->irq_reg_val[CAM_IFE_IRQ_CAMIF_REG_STATUS1];
	soc_info = camif_lite_priv->soc_info;
	soc_private =
		(struct cam_vfe_soc_private *)soc_info->soc_private;

	evt_info.hw_idx   = camif_lite_node->hw_intf->hw_idx;
	evt_info.res_id   = camif_lite_node->res_id;
	evt_info.res_type = camif_lite_node->res_type;

	CAM_DBG(CAM_ISP, "irq_status_0 = 0x%x irq_status_1 = 0x%x",
		irq_status0, irq_status1);

	if (irq_status0 & camif_lite_priv->reg_data->lite_sof_irq_mask) {
		CAM_DBG(CAM_ISP, "VFE:%d CAMIF Lite Received SOF",
			evt_info.hw_idx);
		ret = CAM_VFE_IRQ_STATUS_SUCCESS;
	}

	if (irq_status0 & camif_lite_priv->reg_data->lite_epoch0_irq_mask) {
		CAM_DBG(CAM_ISP, "VFE:%d CAMIF Lite Received EPOCH",
			evt_info.hw_idx);
		ret = CAM_VFE_IRQ_STATUS_SUCCESS;
	}

	if (irq_status0 & camif_lite_priv->reg_data->dual_pd_reg_upd_irq_mask) {
		CAM_DBG(CAM_ISP, "VFE:%d CAMIF Lite Received REG_UPDATE_ACK",
			evt_info.hw_idx);
		ret = CAM_VFE_IRQ_STATUS_SUCCESS;
	}

	if (irq_status0 & camif_lite_priv->reg_data->lite_eof_irq_mask) {
		CAM_DBG(CAM_ISP, "VFE:%d CAMIF Lite Received EOF",
			evt_info.hw_idx);
		ret = CAM_VFE_IRQ_STATUS_SUCCESS;
	}

	if ((irq_status0 & camif_lite_priv->reg_data->lite_err_irq_mask0) ||
		(irq_status1 & camif_lite_priv->reg_data->lite_err_irq_mask1)) {
		CAM_DBG(CAM_ISP, "VFE:%d CAMIF LITE Received ERROR",
			evt_info.hw_idx);

		cam_vfe_camif_lite_cpas_fifo_levels_reg_dump(camif_lite_priv);

		ktime_get_boottime_ts64(&ts);
		CAM_INFO(CAM_ISP,
			"current monotonic time stamp seconds %lld:%lld",
			ts.tv_sec, ts.tv_nsec/1000);
		CAM_INFO(CAM_ISP,
			"ERROR time %lld:%lld",
			camif_lite_priv->error_ts.tv_sec,
			camif_lite_priv->error_ts.tv_usec);
		CAM_INFO(CAM_ISP, "ife_clk_src:%lld",
			soc_private->ife_clk_src);

		if (camif_lite_priv->event_cb)
			camif_lite_priv->event_cb(camif_lite_priv->priv,
				CAM_ISP_HW_EVENT_ERROR, (void *)&evt_info);

		ret = CAM_VFE_IRQ_STATUS_OVERFLOW;

		cam_cpas_log_votes();

	}

	cam_vfe_camif_lite_put_evt_payload(camif_lite_priv, &payload);

	CAM_DBG(CAM_ISP, "returning status = %d", ret);
	return ret;
}

int cam_vfe_camif_lite_ver2_init(
	struct cam_hw_intf            *hw_intf,
	struct cam_hw_soc_info        *soc_info,
	void                          *camif_lite_hw_info,
	struct cam_isp_resource_node  *camif_lite_node,
	void                          *vfe_irq_controller)
{
	struct cam_vfe_mux_camif_lite_data       *camif_lite_priv = NULL;
	struct cam_vfe_camif_lite_ver2_hw_info   *camif_lite_info =
		camif_lite_hw_info;
	int                                       i = 0;

	camif_lite_priv = kzalloc(sizeof(*camif_lite_priv),
		GFP_KERNEL);
	if (!camif_lite_priv)
		return -ENOMEM;

	camif_lite_node->res_priv = camif_lite_priv;

	camif_lite_priv->mem_base           =
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

int cam_vfe_camif_lite_ver2_deinit(
	struct cam_isp_resource_node  *camif_lite_node)
{
	struct cam_vfe_mux_camif_lite_data *camif_lite_priv =
		camif_lite_node->res_priv;
	int                                 i = 0;

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
		CAM_ERR(CAM_ISP, "Error! camif_priv is NULL");
		return -ENODEV;
	}

	kfree(camif_lite_priv);

	return 0;
}
