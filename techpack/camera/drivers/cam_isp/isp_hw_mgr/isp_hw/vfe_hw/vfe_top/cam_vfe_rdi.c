// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/slab.h>
#include "cam_vfe_rdi.h"
#include "cam_isp_hw_mgr_intf.h"
#include "cam_isp_hw.h"
#include "cam_vfe_hw_intf.h"
#include "cam_vfe_top_ver2.h"
#include "cam_io_util.h"
#include "cam_debug_util.h"
#include "cam_cdm_util.h"
#include "cam_irq_controller.h"
#include "cam_tasklet_util.h"

struct cam_vfe_mux_rdi_data {
	void __iomem                                *mem_base;
	struct cam_hw_intf                          *hw_intf;
	struct cam_vfe_top_ver2_reg_offset_common   *common_reg;
	struct cam_vfe_rdi_ver2_reg                 *rdi_reg;
	struct cam_vfe_rdi_common_reg_data          *rdi_common_reg_data;
	struct cam_vfe_rdi_overflow_status          *rdi_irq_status;
	struct cam_vfe_rdi_reg_data                 *reg_data;
	struct cam_hw_soc_info                      *soc_info;

	cam_hw_mgr_event_cb_func              event_cb;
	void                                 *priv;
	int                                   irq_err_handle;
	int                                   irq_handle;
	void                                 *vfe_irq_controller;
	struct cam_vfe_top_irq_evt_payload    evt_payload[CAM_VFE_RDI_EVT_MAX];
	struct list_head                      free_payload_list;
	spinlock_t                            spin_lock;

	enum cam_isp_hw_sync_mode          sync_mode;
	struct timeval                     sof_ts;
	struct timeval                     error_ts;
};

static int cam_vfe_rdi_get_evt_payload(
	struct cam_vfe_mux_rdi_data              *rdi_priv,
	struct cam_vfe_top_irq_evt_payload     **evt_payload)
{
	int rc = 0;

	spin_lock(&rdi_priv->spin_lock);
	if (list_empty(&rdi_priv->free_payload_list)) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "No free payload");
		rc = -ENODEV;
		goto done;
	}

	*evt_payload = list_first_entry(&rdi_priv->free_payload_list,
		struct cam_vfe_top_irq_evt_payload, list);
	list_del_init(&(*evt_payload)->list);
	rc = 0;
done:
	spin_unlock(&rdi_priv->spin_lock);
	return rc;
}

static int cam_vfe_rdi_put_evt_payload(
	struct cam_vfe_mux_rdi_data              *rdi_priv,
	struct cam_vfe_top_irq_evt_payload      **evt_payload)
{
	unsigned long flags;

	if (!rdi_priv) {
		CAM_ERR(CAM_ISP, "Invalid param core_info NULL");
		return -EINVAL;
	}
	if (*evt_payload == NULL) {
		CAM_ERR(CAM_ISP, "No payload to put");
		return -EINVAL;
	}

	spin_lock_irqsave(&rdi_priv->spin_lock, flags);
	list_add_tail(&(*evt_payload)->list, &rdi_priv->free_payload_list);
	*evt_payload = NULL;
	spin_unlock_irqrestore(&rdi_priv->spin_lock, flags);

	CAM_DBG(CAM_ISP, "Done");
	return 0;
}

static int cam_vfe_rdi_cpas_reg_dump(
struct cam_vfe_mux_rdi_data *rdi_priv)
{
	struct cam_vfe_soc_private *soc_private =
		rdi_priv->soc_info->soc_private;
	uint32_t  val;

	if (soc_private->cpas_version == CAM_CPAS_TITAN_175_V120 ||
		soc_private->cpas_version == CAM_CPAS_TITAN_175_V130) {
		cam_cpas_reg_read(soc_private->cpas_handle,
			CAM_CPAS_REG_CAMNOC, 0x3A20, true, &val);
		CAM_INFO(CAM_ISP, "IFE0_nRDI_MAXWR_LOW offset 0x3A20 val 0x%x",
			val);

		cam_cpas_reg_read(soc_private->cpas_handle,
			CAM_CPAS_REG_CAMNOC, 0x5420, true, &val);
		CAM_INFO(CAM_ISP, "IFE1_nRDI_MAXWR_LOW offset 0x5420 val 0x%x",
			val);

		cam_cpas_reg_read(soc_private->cpas_handle,
			CAM_CPAS_REG_CAMNOC, 0x3620, true, &val);
		CAM_INFO(CAM_ISP,
			"IFE0123_RDI_WR_MAXWR_LOW offset 0x3620 val 0x%x", val);

	} else if (soc_private->cpas_version < CAM_CPAS_TITAN_175_V120) {
		cam_cpas_reg_read(soc_private->cpas_handle,
			CAM_CPAS_REG_CAMNOC, 0x420, true, &val);
		CAM_INFO(CAM_ISP, "IFE02_MAXWR_LOW offset 0x420 val 0x%x", val);

		cam_cpas_reg_read(soc_private->cpas_handle,
			CAM_CPAS_REG_CAMNOC, 0x820, true, &val);
		CAM_INFO(CAM_ISP, "IFE13_MAXWR_LOW offset 0x820 val 0x%x", val);
	}

	return 0;

}

static int cam_vfe_rdi_err_irq_top_half(
	uint32_t                               evt_id,
	struct cam_irq_th_payload             *th_payload)
{
	int32_t                                rc;
	int                                    i;
	struct cam_isp_resource_node          *rdi_node;
	struct cam_vfe_mux_rdi_data           *rdi_priv;
	struct cam_vfe_top_irq_evt_payload    *evt_payload;
	bool                                   error_flag = false;

	CAM_DBG(CAM_ISP, "IRQ status_1 = %x", th_payload->evt_status_arr[1]);

	rdi_node = th_payload->handler_priv;
	rdi_priv = rdi_node->res_priv;
	/*
	 *  need to handle overflow condition here, otherwise irq storm
	 *  will block everything
	 */
	if (th_payload->evt_status_arr[1]) {
		CAM_ERR(CAM_ISP,
			"RDI Error: vfe:%d: STATUS_1=0x%x",
			rdi_node->hw_intf->hw_idx,
			th_payload->evt_status_arr[1]);
		CAM_ERR(CAM_ISP, "Stopping further IRQ processing from vfe=%d",
			rdi_node->hw_intf->hw_idx);
		cam_irq_controller_disable_irq(rdi_priv->vfe_irq_controller,
			rdi_priv->irq_err_handle);
		cam_irq_controller_clear_and_mask(evt_id,
			rdi_priv->vfe_irq_controller);
		error_flag = true;
	}

	rc  = cam_vfe_rdi_get_evt_payload(rdi_priv, &evt_payload);
	if (rc) {
		CAM_ERR_RATE_LIMIT(CAM_ISP,
			"No tasklet_cmd is free in queue");
		CAM_ERR_RATE_LIMIT(CAM_ISP, "STATUS_1=0x%x",
			th_payload->evt_status_arr[1]);
		return rc;
	}

	cam_isp_hw_get_timestamp(&evt_payload->ts);
	if (error_flag) {
		rdi_priv->error_ts.tv_sec =
			evt_payload->ts.mono_time.tv_sec;
		rdi_priv->error_ts.tv_usec =
			evt_payload->ts.mono_time.tv_usec;
	}

	for (i = 0; i < th_payload->num_registers; i++)
		evt_payload->irq_reg_val[i] = th_payload->evt_status_arr[i];

	evt_payload->irq_reg_val[i] = cam_io_r(rdi_priv->mem_base +
			rdi_priv->common_reg->violation_status);

	if (error_flag)
		CAM_INFO(CAM_ISP, "Violation status = 0x%x",
			evt_payload->irq_reg_val[i]);

	th_payload->evt_payload_priv = evt_payload;

	return rc;
}

static int cam_vfe_rdi_get_reg_update(
	struct cam_isp_resource_node  *rdi_res,
	void *cmd_args, uint32_t arg_size)
{
	uint32_t                          size = 0;
	uint32_t                          reg_val_pair[2];
	struct cam_isp_hw_get_cmd_update *cdm_args = cmd_args;
	struct cam_cdm_utils_ops         *cdm_util_ops = NULL;
	struct cam_vfe_mux_rdi_data      *rsrc_data = NULL;

	if (arg_size != sizeof(struct cam_isp_hw_get_cmd_update)) {
		CAM_ERR(CAM_ISP, "Error - Invalid cmd size");
		return -EINVAL;
	}

	if (!cdm_args || !cdm_args->res) {
		CAM_ERR(CAM_ISP, "Error - Invalid args");
		return -EINVAL;
	}

	cdm_util_ops = (struct cam_cdm_utils_ops *)cdm_args->res->cdm_ops;
	if (!cdm_util_ops) {
		CAM_ERR(CAM_ISP, "Error - Invalid CDM ops");
		return -EINVAL;
	}

	size = cdm_util_ops->cdm_required_size_reg_random(1);
	/* since cdm returns dwords, we need to convert it into bytes */
	if ((size * 4) > cdm_args->cmd.size) {
		CAM_ERR(CAM_ISP,
			"Error - buf size:%d is not sufficient, expected: %d",
			cdm_args->cmd.size, size * 4);
		return -EINVAL;
	}

	rsrc_data = rdi_res->res_priv;
	reg_val_pair[0] = rsrc_data->rdi_reg->reg_update_cmd;
	reg_val_pair[1] = rsrc_data->reg_data->reg_update_cmd_data;
	CAM_DBG(CAM_ISP, "RDI%d reg_update_cmd %x",
		rdi_res->res_id - CAM_ISP_HW_VFE_IN_RDI0, reg_val_pair[1]);

	cdm_util_ops->cdm_write_regrandom(cdm_args->cmd.cmd_buf_addr,
		1, reg_val_pair);
	cdm_args->cmd.used_bytes = size * 4;

	return 0;
}

int cam_vfe_rdi_ver2_acquire_resource(
	struct cam_isp_resource_node  *rdi_res,
	void                          *acquire_param)
{
	struct cam_vfe_mux_rdi_data   *rdi_data;
	struct cam_vfe_acquire_args   *acquire_data;

	rdi_data     = (struct cam_vfe_mux_rdi_data *)rdi_res->res_priv;
	acquire_data = (struct cam_vfe_acquire_args *)acquire_param;

	rdi_data->event_cb    = acquire_data->event_cb;
	rdi_data->priv        = acquire_data->priv;
	rdi_data->sync_mode   = acquire_data->vfe_in.sync_mode;
	rdi_res->rdi_only_ctx = 0;

	return 0;
}

static int cam_vfe_rdi_resource_start(
	struct cam_isp_resource_node  *rdi_res)
{
	struct cam_vfe_mux_rdi_data   *rsrc_data;
	int                            rc = 0;
	uint32_t                       err_irq_mask[CAM_IFE_IRQ_REGISTERS_MAX];
	uint32_t                 rdi_irq_mask[CAM_IFE_IRQ_REGISTERS_MAX] = {0};

	if (!rdi_res) {
		CAM_ERR(CAM_ISP, "Error! Invalid input arguments");
		return -EINVAL;
	}

	if (rdi_res->res_state != CAM_ISP_RESOURCE_STATE_RESERVED) {
		CAM_ERR(CAM_ISP, "Error! Invalid rdi res res_state:%d",
			rdi_res->res_state);
		return -EINVAL;
	}

	rsrc_data = (struct cam_vfe_mux_rdi_data  *)rdi_res->res_priv;
	err_irq_mask[CAM_IFE_IRQ_CAMIF_REG_STATUS0] =
		rsrc_data->rdi_common_reg_data->error_irq_mask0;
	err_irq_mask[CAM_IFE_IRQ_CAMIF_REG_STATUS1] =
		rsrc_data->rdi_common_reg_data->error_irq_mask1;

	rdi_res->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;

	/* Reg Update */
	cam_io_w_mb(rsrc_data->reg_data->reg_update_cmd_data,
		rsrc_data->mem_base + rsrc_data->rdi_reg->reg_update_cmd);

	if (!rsrc_data->irq_err_handle) {
		rsrc_data->irq_err_handle = cam_irq_controller_subscribe_irq(
			rsrc_data->vfe_irq_controller,
			CAM_IRQ_PRIORITY_0,
			err_irq_mask,
			rdi_res,
			cam_vfe_rdi_err_irq_top_half,
			rdi_res->bottom_half_handler,
			rdi_res->tasklet_info,
			&tasklet_bh_api);
		if (rsrc_data->irq_err_handle < 1) {
			CAM_ERR(CAM_ISP, "Error IRQ handle subscribe failure");
			rc = -ENOMEM;
			rsrc_data->irq_err_handle = 0;
		}
	}

	if (!rdi_res->rdi_only_ctx)
		goto end;

	rdi_irq_mask[0] =
		(rsrc_data->reg_data->reg_update_irq_mask |
			rsrc_data->reg_data->sof_irq_mask);

	CAM_DBG(CAM_ISP, "RDI%d irq_mask 0x%x",
		rdi_res->res_id - CAM_ISP_HW_VFE_IN_RDI0,
		rdi_irq_mask[0]);

	if (!rsrc_data->irq_handle) {
		rsrc_data->irq_handle = cam_irq_controller_subscribe_irq(
			rsrc_data->vfe_irq_controller,
			CAM_IRQ_PRIORITY_1,
			rdi_irq_mask,
			rdi_res,
			rdi_res->top_half_handler,
			rdi_res->bottom_half_handler,
			rdi_res->tasklet_info,
			&tasklet_bh_api);
		if (rsrc_data->irq_handle < 1) {
			CAM_ERR(CAM_ISP, "IRQ handle subscribe failure");
			rc = -ENOMEM;
			rsrc_data->irq_handle = 0;
		}
	}

	rsrc_data->sof_ts.tv_sec = 0;
	rsrc_data->sof_ts.tv_usec = 0;
	rsrc_data->error_ts.tv_sec = 0;
	rsrc_data->error_ts.tv_usec = 0;

	CAM_DBG(CAM_ISP, "Start RDI %d",
		rdi_res->res_id - CAM_ISP_HW_VFE_IN_RDI0);
end:
	return rc;
}


static int cam_vfe_rdi_resource_stop(
	struct cam_isp_resource_node        *rdi_res)
{
	struct cam_vfe_mux_rdi_data         *rdi_priv;
	int rc = 0;

	if (!rdi_res) {
		CAM_ERR(CAM_ISP, "Error! Invalid input arguments");
		return -EINVAL;
	}

	if (rdi_res->res_state == CAM_ISP_RESOURCE_STATE_RESERVED ||
		rdi_res->res_state == CAM_ISP_RESOURCE_STATE_AVAILABLE)
		return 0;

	rdi_priv = (struct cam_vfe_mux_rdi_data *)rdi_res->res_priv;

	if (rdi_res->res_state == CAM_ISP_RESOURCE_STATE_STREAMING)
		rdi_res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;

	if (rdi_priv->irq_handle) {
		cam_irq_controller_unsubscribe_irq(
			rdi_priv->vfe_irq_controller, rdi_priv->irq_handle);
		rdi_priv->irq_handle = 0;
	}

	if (rdi_priv->irq_err_handle) {
		cam_irq_controller_unsubscribe_irq(
			rdi_priv->vfe_irq_controller, rdi_priv->irq_err_handle);
		rdi_priv->irq_err_handle = 0;
	}

	return rc;
}

static int cam_vfe_rdi_process_cmd(struct cam_isp_resource_node *rsrc_node,
	uint32_t cmd_type, void *cmd_args, uint32_t arg_size)
{
	int rc = -EINVAL;

	if (!rsrc_node || !cmd_args) {
		CAM_ERR(CAM_ISP, "Error! Invalid input arguments");
		return -EINVAL;
	}

	switch (cmd_type) {
	case CAM_ISP_HW_CMD_GET_REG_UPDATE:
		rc = cam_vfe_rdi_get_reg_update(rsrc_node, cmd_args,
			arg_size);
		break;
	default:
		CAM_ERR(CAM_ISP,
			"unsupported RDI process command:%d", cmd_type);
		break;
	}

	return rc;
}

static int cam_vfe_rdi_handle_irq_top_half(uint32_t evt_id,
	struct cam_irq_th_payload *th_payload)
{
	int32_t                                rc;
	int                                    i;
	struct cam_isp_resource_node          *rdi_node;
	struct cam_vfe_mux_rdi_data           *rdi_priv;
	struct cam_vfe_top_irq_evt_payload    *evt_payload;

	rdi_node = th_payload->handler_priv;
	rdi_priv = rdi_node->res_priv;

	CAM_DBG(CAM_ISP, "IRQ status_0 = %x", th_payload->evt_status_arr[0]);
	CAM_DBG(CAM_ISP, "IRQ status_1 = %x", th_payload->evt_status_arr[1]);

	rc  = cam_vfe_rdi_get_evt_payload(rdi_priv, &evt_payload);
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

static int cam_vfe_rdi_handle_irq_bottom_half(void *handler_priv,
	void *evt_payload_priv)
{
	int                                  ret = CAM_VFE_IRQ_STATUS_ERR;
	struct cam_isp_resource_node        *rdi_node;
	struct cam_vfe_mux_rdi_data         *rdi_priv;
	struct cam_vfe_top_irq_evt_payload  *payload;
	struct cam_isp_hw_event_info         evt_info;
	uint32_t                             irq_status0;
	uint32_t                             irq_status1;
	uint32_t                             irq_rdi_status;
	struct cam_hw_soc_info              *soc_info = NULL;
	struct cam_vfe_soc_private          *soc_private = NULL;
	struct timespec64                    ts;

	if (!handler_priv || !evt_payload_priv) {
		CAM_ERR(CAM_ISP, "Invalid params");
		return ret;
	}

	rdi_node = handler_priv;
	rdi_priv = rdi_node->res_priv;
	payload = evt_payload_priv;
	soc_info = rdi_priv->soc_info;
	soc_private =
		(struct cam_vfe_soc_private *)soc_info->soc_private;

	irq_status0 = payload->irq_reg_val[CAM_IFE_IRQ_CAMIF_REG_STATUS0];
	irq_status1 = payload->irq_reg_val[CAM_IFE_IRQ_CAMIF_REG_STATUS1];

	evt_info.hw_idx   = rdi_node->hw_intf->hw_idx;
	evt_info.res_id   = rdi_node->res_id;
	evt_info.res_type = rdi_node->res_type;

	CAM_DBG(CAM_ISP, "irq_status_0 = %x", irq_status0);

	if (irq_status0 & rdi_priv->reg_data->sof_irq_mask) {
		CAM_DBG(CAM_ISP, "Received SOF");
		rdi_priv->sof_ts.tv_sec =
			payload->ts.mono_time.tv_sec;
		rdi_priv->sof_ts.tv_usec =
			payload->ts.mono_time.tv_usec;
		if (rdi_priv->event_cb)
			rdi_priv->event_cb(rdi_priv->priv,
				CAM_ISP_HW_EVENT_SOF, (void *)&evt_info);

		ret = CAM_VFE_IRQ_STATUS_SUCCESS;
	}

	if (irq_status0 & rdi_priv->reg_data->reg_update_irq_mask) {
		CAM_DBG(CAM_ISP, "Received REG UPDATE");

		if (rdi_priv->event_cb)
			rdi_priv->event_cb(rdi_priv->priv,
				CAM_ISP_HW_EVENT_REG_UPDATE, (void *)&evt_info);

		ret = CAM_VFE_IRQ_STATUS_SUCCESS;
	}

	if (!rdi_priv->rdi_irq_status)
		goto end;

	irq_rdi_status =
		(irq_status1 &
		rdi_priv->rdi_irq_status->rdi_overflow_mask);
	if (irq_rdi_status) {
		ktime_get_boottime_ts64(&ts);
		CAM_INFO(CAM_ISP,
			"current monotonic time stamp seconds %lld:%lld",
			ts.tv_sec, ts.tv_nsec/1000);

		cam_vfe_rdi_cpas_reg_dump(rdi_priv);

		CAM_INFO(CAM_ISP, "ife_clk_src:%lld",
			soc_private->ife_clk_src);
		CAM_INFO(CAM_ISP,
			"ERROR time %lld:%lld SOF %lld:%lld",
			rdi_priv->error_ts.tv_sec,
			rdi_priv->error_ts.tv_usec,
			rdi_priv->sof_ts.tv_sec,
			rdi_priv->sof_ts.tv_usec);

		if (irq_rdi_status &
			rdi_priv->rdi_irq_status->rdi0_overflow_mask) {
			evt_info.res_id = CAM_ISP_IFE_OUT_RES_RDI_0;
			}
		else if (irq_rdi_status &
			rdi_priv->rdi_irq_status->rdi1_overflow_mask) {
			evt_info.res_id = CAM_ISP_IFE_OUT_RES_RDI_1;
			}
		else if (irq_rdi_status &
			rdi_priv->rdi_irq_status->rdi2_overflow_mask) {
			evt_info.res_id = CAM_ISP_IFE_OUT_RES_RDI_2;
			}
		else if (irq_rdi_status &
			rdi_priv->rdi_irq_status->rdi3_overflow_mask) {
			evt_info.res_id = CAM_ISP_IFE_OUT_RES_RDI_3;
			}

		if (rdi_priv->event_cb)
			rdi_priv->event_cb(rdi_priv->priv,
			CAM_ISP_HW_EVENT_ERROR,
			(void *)&evt_info);
		cam_cpas_log_votes();
	}
end:
	cam_vfe_rdi_put_evt_payload(rdi_priv, &payload);
	CAM_DBG(CAM_ISP, "returing status = %d", ret);
	return ret;
}

int cam_vfe_rdi_ver2_init(
	struct cam_hw_intf            *hw_intf,
	struct cam_hw_soc_info        *soc_info,
	void                          *rdi_hw_info,
	struct cam_isp_resource_node  *rdi_node,
	void                          *vfe_irq_controller)
{
	struct cam_vfe_mux_rdi_data     *rdi_priv = NULL;
	struct cam_vfe_rdi_ver2_hw_info *rdi_info = rdi_hw_info;
	int                              i = 0;

	rdi_priv = kzalloc(sizeof(struct cam_vfe_mux_rdi_data),
			GFP_KERNEL);
	if (!rdi_priv) {
		CAM_DBG(CAM_ISP, "Error! Failed to alloc for rdi_priv");
		return -ENOMEM;
	}

	rdi_node->res_priv = rdi_priv;

	rdi_priv->mem_base   = soc_info->reg_map[VFE_CORE_BASE_IDX].mem_base;
	rdi_priv->hw_intf    = hw_intf;
	rdi_priv->common_reg = rdi_info->common_reg;
	rdi_priv->rdi_reg    = rdi_info->rdi_reg;
	rdi_priv->vfe_irq_controller  = vfe_irq_controller;
	rdi_priv->rdi_common_reg_data = rdi_info->common_reg_data;
	rdi_priv->soc_info = soc_info;
	rdi_priv->rdi_irq_status = rdi_info->rdi_irq_status;

	switch (rdi_node->res_id) {
	case CAM_ISP_HW_VFE_IN_RDI0:
		rdi_priv->reg_data = rdi_info->reg_data[0];
		break;
	case CAM_ISP_HW_VFE_IN_RDI1:
		rdi_priv->reg_data = rdi_info->reg_data[1];
		break;
	case CAM_ISP_HW_VFE_IN_RDI2:
		rdi_priv->reg_data = rdi_info->reg_data[2];
		break;
	case CAM_ISP_HW_VFE_IN_RDI3:
		if (rdi_info->reg_data[3]) {
			rdi_priv->reg_data = rdi_info->reg_data[3];
		} else {
			CAM_ERR(CAM_ISP, "Error! RDI3 is not supported");
			goto err_init;
		}
		break;
	default:
		CAM_DBG(CAM_ISP, "invalid Resource id:%d", rdi_node->res_id);
		goto err_init;
	}

	rdi_node->start = cam_vfe_rdi_resource_start;
	rdi_node->stop  = cam_vfe_rdi_resource_stop;
	rdi_node->process_cmd = cam_vfe_rdi_process_cmd;
	rdi_node->top_half_handler = cam_vfe_rdi_handle_irq_top_half;
	rdi_node->bottom_half_handler = cam_vfe_rdi_handle_irq_bottom_half;

	spin_lock_init(&rdi_priv->spin_lock);
	INIT_LIST_HEAD(&rdi_priv->free_payload_list);
	for (i = 0; i < CAM_VFE_RDI_EVT_MAX; i++) {
		INIT_LIST_HEAD(&rdi_priv->evt_payload[i].list);
		list_add_tail(&rdi_priv->evt_payload[i].list,
			&rdi_priv->free_payload_list);
	}

	return 0;
err_init:
	kfree(rdi_priv);
	return -EINVAL;
}

int cam_vfe_rdi_ver2_deinit(
	struct cam_isp_resource_node  *rdi_node)
{
	struct cam_vfe_mux_rdi_data *rdi_priv = rdi_node->res_priv;
	int                          i = 0;

	INIT_LIST_HEAD(&rdi_priv->free_payload_list);
	for (i = 0; i < CAM_VFE_RDI_EVT_MAX; i++)
		INIT_LIST_HEAD(&rdi_priv->evt_payload[i].list);

	rdi_node->start = NULL;
	rdi_node->stop  = NULL;
	rdi_node->process_cmd = NULL;
	rdi_node->top_half_handler = NULL;
	rdi_node->bottom_half_handler = NULL;

	rdi_node->res_priv = NULL;

	if (!rdi_priv) {
		CAM_ERR(CAM_ISP, "Error! rdi_priv NULL");
		return -ENODEV;
	}
	kfree(rdi_priv);

	return 0;
}
