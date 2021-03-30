// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
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
#include "cam_vfe_camif_ver2.h"
#include "cam_debug_util.h"
#include "cam_cdm_util.h"
#include "cam_cpas_api.h"

#define CAM_VFE_CAMIF_IRQ_SOF_DEBUG_CNT_MAX 2

struct cam_vfe_mux_camif_data {
	void __iomem                                *mem_base;
	struct cam_hw_intf                          *hw_intf;
	struct cam_vfe_camif_ver2_reg               *camif_reg;
	struct cam_vfe_top_ver2_reg_offset_common   *common_reg;
	struct cam_vfe_camif_reg_data               *reg_data;
	struct cam_hw_soc_info                      *soc_info;

	cam_hw_mgr_event_cb_func             event_cb;
	void                                *priv;
	int                                  irq_err_handle;
	int                                  irq_handle;
	void                                *vfe_irq_controller;
	struct cam_vfe_top_irq_evt_payload evt_payload[CAM_VFE_CAMIF_EVT_MAX];
	struct list_head                     free_payload_list;
	spinlock_t                           spin_lock;

	enum cam_isp_hw_sync_mode          sync_mode;
	uint32_t                           dsp_mode;
	uint32_t                           pix_pattern;
	uint32_t                           first_pixel;
	uint32_t                           first_line;
	uint32_t                           last_pixel;
	uint32_t                           last_line;
	uint32_t                           hbi_value;
	uint32_t                           vbi_value;
	bool                               enable_sof_irq_debug;
	uint32_t                           irq_debug_cnt;
	uint32_t                           camif_debug;
	uint32_t                           dual_hw_idx;
	uint32_t                           is_dual;
	struct timeval                     sof_ts;
	struct timeval                     epoch_ts;
	struct timeval                     eof_ts;
	struct timeval                     error_ts;
};

static int cam_vfe_camif_get_evt_payload(
	struct cam_vfe_mux_camif_data            *camif_priv,
	struct cam_vfe_top_irq_evt_payload    **evt_payload)
{
	int rc = 0;

	spin_lock(&camif_priv->spin_lock);
	if (list_empty(&camif_priv->free_payload_list)) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "No free payload");
		rc = -ENODEV;
		goto done;
	}

	*evt_payload = list_first_entry(&camif_priv->free_payload_list,
		struct cam_vfe_top_irq_evt_payload, list);
	list_del_init(&(*evt_payload)->list);
done:
	spin_unlock(&camif_priv->spin_lock);
	return rc;
}

static int cam_vfe_camif_put_evt_payload(
	struct cam_vfe_mux_camif_data            *camif_priv,
	struct cam_vfe_top_irq_evt_payload    **evt_payload)
{
	unsigned long flags;

	if (!camif_priv) {
		CAM_ERR(CAM_ISP, "Invalid param core_info NULL");
		return -EINVAL;
	}
	if (*evt_payload == NULL) {
		CAM_ERR(CAM_ISP, "No payload to put");
		return -EINVAL;
	}

	spin_lock_irqsave(&camif_priv->spin_lock, flags);
	list_add_tail(&(*evt_payload)->list, &camif_priv->free_payload_list);
	*evt_payload = NULL;
	spin_unlock_irqrestore(&camif_priv->spin_lock, flags);

	CAM_DBG(CAM_ISP, "Done");
	return 0;
}

static int cam_vfe_camif_err_irq_top_half(
	uint32_t                               evt_id,
	struct cam_irq_th_payload             *th_payload)
{
	int32_t                                rc;
	int                                    i;
	struct cam_isp_resource_node          *camif_node;
	struct cam_vfe_mux_camif_data         *camif_priv;
	struct cam_vfe_top_irq_evt_payload  *evt_payload;
	bool                                   error_flag = false;

	CAM_DBG(CAM_ISP, "IRQ status_0 = %x, IRQ status_1 = %x",
		th_payload->evt_status_arr[0], th_payload->evt_status_arr[1]);

	camif_node = th_payload->handler_priv;
	camif_priv = camif_node->res_priv;
	/*
	 *  need to handle overflow condition here, otherwise irq storm
	 *  will block everything
	 */
	if (th_payload->evt_status_arr[1] || (th_payload->evt_status_arr[0] &
		camif_priv->reg_data->error_irq_mask0)) {
		CAM_ERR(CAM_ISP,
			"Camif Error: vfe:%d: IRQ STATUS_0=0x%x STATUS_1=0x%x",
			camif_node->hw_intf->hw_idx,
			th_payload->evt_status_arr[0],
			th_payload->evt_status_arr[1]);
		CAM_ERR(CAM_ISP, "Stopping further IRQ processing from vfe=%d",
			camif_node->hw_intf->hw_idx);
		cam_irq_controller_disable_irq(camif_priv->vfe_irq_controller,
			camif_priv->irq_err_handle);
		cam_irq_controller_clear_and_mask(evt_id,
			camif_priv->vfe_irq_controller);
		error_flag = true;
	}

	rc  = cam_vfe_camif_get_evt_payload(camif_priv, &evt_payload);
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
		camif_priv->error_ts.tv_sec =
			evt_payload->ts.mono_time.tv_sec;
		camif_priv->error_ts.tv_usec =
			evt_payload->ts.mono_time.tv_usec;
	}

	for (i = 0; i < th_payload->num_registers; i++)
		evt_payload->irq_reg_val[i] = th_payload->evt_status_arr[i];

	evt_payload->irq_reg_val[i] = cam_io_r(camif_priv->mem_base +
		camif_priv->common_reg->violation_status);

	if (error_flag)
		CAM_INFO(CAM_ISP, "Violation status = 0x%x",
			evt_payload->irq_reg_val[2]);

	th_payload->evt_payload_priv = evt_payload;

	return rc;
}

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
	struct cam_isp_hw_get_cmd_update *cdm_args = cmd_args;
	struct cam_cdm_utils_ops         *cdm_util_ops = NULL;
	struct cam_vfe_mux_camif_data    *rsrc_data = NULL;

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

	rsrc_data = camif_res->res_priv;
	reg_val_pair[0] = rsrc_data->camif_reg->reg_update_cmd;
	reg_val_pair[1] = rsrc_data->reg_data->reg_update_cmd_data;
	CAM_DBG(CAM_ISP, "CAMIF reg_update_cmd %x offset %x",
		reg_val_pair[1], reg_val_pair[0]);

	cdm_util_ops->cdm_write_regrandom(cdm_args->cmd.cmd_buf_addr,
		1, reg_val_pair);

	cdm_args->cmd.used_bytes = size * 4;

	return 0;
}

int cam_vfe_camif_ver2_acquire_resource(
	struct cam_isp_resource_node  *camif_res,
	void                          *acquire_param)
{
	struct cam_vfe_mux_camif_data    *camif_data;
	struct cam_vfe_acquire_args      *acquire_data;

	int rc = 0;

	camif_data   = (struct cam_vfe_mux_camif_data *)camif_res->res_priv;
	acquire_data = (struct cam_vfe_acquire_args   *)acquire_param;

	rc = cam_vfe_camif_validate_pix_pattern(
			acquire_data->vfe_in.in_port->test_pattern);
	if (rc)
		return rc;

	camif_data->sync_mode   = acquire_data->vfe_in.sync_mode;
	camif_data->pix_pattern = acquire_data->vfe_in.in_port->test_pattern;
	camif_data->dsp_mode    = acquire_data->vfe_in.in_port->dsp_mode;
	camif_data->first_pixel = acquire_data->vfe_in.in_port->left_start;
	camif_data->last_pixel  = acquire_data->vfe_in.in_port->left_stop;
	camif_data->first_line  = acquire_data->vfe_in.in_port->line_start;
	camif_data->last_line   = acquire_data->vfe_in.in_port->line_stop;
	camif_data->event_cb    = acquire_data->event_cb;
	camif_data->priv        = acquire_data->priv;
	camif_data->is_dual     = acquire_data->vfe_in.is_dual;
	camif_data->hbi_value   = 0;
	camif_data->vbi_value   = 0;

	if (acquire_data->vfe_in.is_dual)
		camif_data->dual_hw_idx =
			acquire_data->vfe_in.dual_hw_idx;

	CAM_DBG(CAM_ISP, "hw id:%d pix_pattern:%d dsp_mode=%d",
		camif_res->hw_intf->hw_idx,
		camif_data->pix_pattern, camif_data->dsp_mode);
	return rc;
}

static int cam_vfe_camif_resource_init(
	struct cam_isp_resource_node        *camif_res,
	void *init_args, uint32_t arg_size)
{
	struct cam_vfe_mux_camif_data    *camif_data;
	struct cam_hw_soc_info           *soc_info;
	int rc = 0;

	if (!camif_res) {
		CAM_ERR(CAM_ISP, "Error Invalid input arguments");
		return -EINVAL;
	}

	camif_data   = (struct cam_vfe_mux_camif_data *)camif_res->res_priv;

	soc_info = camif_data->soc_info;

	if ((camif_data->dsp_mode >= CAM_ISP_DSP_MODE_ONE_WAY) &&
		(camif_data->dsp_mode <= CAM_ISP_DSP_MODE_ROUND)) {
		rc = cam_vfe_soc_enable_clk(soc_info, CAM_VFE_DSP_CLK_NAME);
		if (rc)
			CAM_ERR(CAM_ISP, "failed to enable dsp clk");
	}
	camif_data->sof_ts.tv_sec = 0;
	camif_data->sof_ts.tv_usec = 0;
	camif_data->epoch_ts.tv_sec = 0;
	camif_data->epoch_ts.tv_usec = 0;
	camif_data->eof_ts.tv_sec = 0;
	camif_data->eof_ts.tv_usec = 0;
	camif_data->error_ts.tv_sec = 0;
	camif_data->error_ts.tv_usec = 0;

	return rc;
}

static int cam_vfe_camif_resource_deinit(
	struct cam_isp_resource_node        *camif_res,
	void *init_args, uint32_t arg_size)
{
	struct cam_vfe_mux_camif_data    *camif_data;
	struct cam_hw_soc_info           *soc_info;
	int rc = 0;

	if (!camif_res) {
		CAM_ERR(CAM_ISP, "Error Invalid input arguments");
		return -EINVAL;
	}

	camif_data   = (struct cam_vfe_mux_camif_data *)camif_res->res_priv;

	soc_info = camif_data->soc_info;

	if ((camif_data->dsp_mode >= CAM_ISP_DSP_MODE_ONE_WAY) &&
		(camif_data->dsp_mode <= CAM_ISP_DSP_MODE_ROUND)) {
		rc = cam_vfe_soc_disable_clk(soc_info, CAM_VFE_DSP_CLK_NAME);
		if (rc)
			CAM_ERR(CAM_ISP, "failed to disable dsp clk");
	}

	return rc;
}

static int cam_vfe_camif_resource_start(
	struct cam_isp_resource_node   *camif_res)
{
	struct cam_vfe_mux_camif_data  *rsrc_data;
	uint32_t                        val = 0;
	uint32_t                        epoch0_irq_mask;
	uint32_t                        epoch1_irq_mask;
	uint32_t                        computed_epoch_line_cfg;
	int                             rc = 0;
	uint32_t                        err_irq_mask[CAM_IFE_IRQ_REGISTERS_MAX];
	uint32_t                        irq_mask[CAM_IFE_IRQ_REGISTERS_MAX];
	uint32_t                        dual_vfe_sync_val;
	struct cam_vfe_soc_private     *soc_private;

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
	err_irq_mask[CAM_IFE_IRQ_CAMIF_REG_STATUS0] =
		rsrc_data->reg_data->error_irq_mask0;
	err_irq_mask[CAM_IFE_IRQ_CAMIF_REG_STATUS1] =
		rsrc_data->reg_data->error_irq_mask1;
	irq_mask[CAM_IFE_IRQ_CAMIF_REG_STATUS0] =
		rsrc_data->reg_data->subscribe_irq_mask0;
	irq_mask[CAM_IFE_IRQ_CAMIF_REG_STATUS1] =
		rsrc_data->reg_data->subscribe_irq_mask1;

	soc_private = rsrc_data->soc_info->soc_private;

	if (!soc_private) {
		CAM_ERR(CAM_ISP, "Error! soc_private NULL");
		return -ENODEV;
	}

	/*config vfe core*/
	val = (rsrc_data->pix_pattern <<
			rsrc_data->reg_data->pixel_pattern_shift);
	if (rsrc_data->sync_mode == CAM_ISP_HW_SYNC_SLAVE)
		val |= (1 << rsrc_data->reg_data->extern_reg_update_shift);

	if ((rsrc_data->dsp_mode >= CAM_ISP_DSP_MODE_ONE_WAY) &&
		(rsrc_data->dsp_mode <= CAM_ISP_DSP_MODE_ROUND)) {
		/* DSP mode reg val is CAM_ISP_DSP_MODE - 1 */
		val |= (((rsrc_data->dsp_mode - 1) &
			rsrc_data->reg_data->dsp_mode_mask) <<
			rsrc_data->reg_data->dsp_mode_shift);
		val |= (0x1 << rsrc_data->reg_data->dsp_en_shift);
	}

	cam_io_w_mb(val, rsrc_data->mem_base + rsrc_data->common_reg->core_cfg);

	CAM_DBG(CAM_ISP, "hw id:%d core_cfg val:%d", camif_res->hw_intf->hw_idx,
		val);

	/* disable the CGC for stats */
	cam_io_w_mb(0xFFFFFFFF, rsrc_data->mem_base +
		rsrc_data->common_reg->module_ctrl[
		CAM_VFE_TOP_VER2_MODULE_STATS]->cgc_ovd);

	/* epoch config */
	switch (soc_private->cpas_version) {
	case CAM_CPAS_TITAN_170_V100:
	case CAM_CPAS_TITAN_170_V110:
	case CAM_CPAS_TITAN_170_V120:
	case CAM_CPAS_TITAN_170_V200:
		cam_io_w_mb(rsrc_data->reg_data->epoch_line_cfg,
				rsrc_data->mem_base +
				rsrc_data->camif_reg->epoch_irq);
		break;
	default:
		epoch0_irq_mask = (((rsrc_data->last_line +
				rsrc_data->vbi_value) -
				rsrc_data->first_line) / 2);
		if ((epoch0_irq_mask) >
			(rsrc_data->last_line - rsrc_data->first_line))
			epoch0_irq_mask = rsrc_data->last_line -
				rsrc_data->first_line;

		epoch1_irq_mask = rsrc_data->reg_data->epoch_line_cfg &
				0xFFFF;
		computed_epoch_line_cfg = (epoch0_irq_mask << 16) |
				epoch1_irq_mask;
		cam_io_w_mb(computed_epoch_line_cfg,
				rsrc_data->mem_base +
				rsrc_data->camif_reg->epoch_irq);
		CAM_DBG(CAM_ISP, "first_line: %u\n"
				"last_line: %u vbi: %u\n"
				"epoch_line_cfg: 0x%x",
				rsrc_data->first_line,
				rsrc_data->last_line,
				rsrc_data->vbi_value,
				computed_epoch_line_cfg);
		break;
	}

	if (rsrc_data->is_dual && rsrc_data->reg_data->dual_vfe_sync_mask) {
		dual_vfe_sync_val = (rsrc_data->dual_hw_idx &
			rsrc_data->reg_data->dual_vfe_sync_mask) + 1;
		cam_io_w_mb(dual_vfe_sync_val, rsrc_data->mem_base +
			rsrc_data->camif_reg->dual_vfe_sync);
	}
	camif_res->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;

	/* Reg Update */
	cam_io_w_mb(rsrc_data->reg_data->reg_update_cmd_data,
		rsrc_data->mem_base + rsrc_data->camif_reg->reg_update_cmd);
	CAM_DBG(CAM_ISP, "hw id:%d RUP val:%d", camif_res->hw_intf->hw_idx,
		rsrc_data->reg_data->reg_update_cmd_data);

	/* disable sof irq debug flag */
	rsrc_data->enable_sof_irq_debug = false;
	rsrc_data->irq_debug_cnt = 0;

	if (rsrc_data->camif_debug &
		CAMIF_DEBUG_ENABLE_SENSOR_DIAG_STATUS) {
		val = cam_io_r_mb(rsrc_data->mem_base +
			rsrc_data->camif_reg->vfe_diag_config);
		val |= rsrc_data->reg_data->enable_diagnostic_hw;
		cam_io_w_mb(val, rsrc_data->mem_base +
			rsrc_data->camif_reg->vfe_diag_config);
	}

	if ((rsrc_data->sync_mode == CAM_ISP_HW_SYNC_SLAVE) &&
		rsrc_data->is_dual)
		goto subscribe_err;

	if (!rsrc_data->irq_handle) {
		rsrc_data->irq_handle = cam_irq_controller_subscribe_irq(
			rsrc_data->vfe_irq_controller,
			CAM_IRQ_PRIORITY_1,
			irq_mask,
			camif_res,
			camif_res->top_half_handler,
			camif_res->bottom_half_handler,
			camif_res->tasklet_info,
			&tasklet_bh_api);
		if (rsrc_data->irq_handle < 1) {
			CAM_ERR(CAM_ISP, "IRQ handle subscribe failure");
			rc = -ENOMEM;
			rsrc_data->irq_handle = 0;
		}
	}

subscribe_err:
	if (!rsrc_data->irq_err_handle) {
		rsrc_data->irq_err_handle = cam_irq_controller_subscribe_irq(
			rsrc_data->vfe_irq_controller,
			CAM_IRQ_PRIORITY_0,
			err_irq_mask,
			camif_res,
			cam_vfe_camif_err_irq_top_half,
			camif_res->bottom_half_handler,
			camif_res->tasklet_info,
			&tasklet_bh_api);
		if (rsrc_data->irq_err_handle < 1) {
			CAM_ERR(CAM_ISP, "Error IRQ handle subscribe failure");
			rc = -ENOMEM;
			rsrc_data->irq_err_handle = 0;
		}
	}

	CAM_DBG(CAM_ISP, "Start Camif IFE %d Done", camif_res->hw_intf->hw_idx);
	return 0;
}

static int cam_vfe_camif_reg_dump(
	struct cam_isp_resource_node *camif_res)
{
	struct cam_vfe_mux_camif_data *camif_priv;
	struct cam_vfe_soc_private *soc_private;
	uint32_t offset, val, wm_idx;

	if (!camif_res) {
		CAM_ERR(CAM_ISP, "Error! Invalid input arguments");
		return -EINVAL;
	}

	if ((camif_res->res_state == CAM_ISP_RESOURCE_STATE_RESERVED) ||
		(camif_res->res_state == CAM_ISP_RESOURCE_STATE_AVAILABLE))
		return 0;

	camif_priv = (struct cam_vfe_mux_camif_data *)camif_res->res_priv;
	for (offset = 0x0; offset < 0x1160; offset += 0x4) {
		val = cam_soc_util_r(camif_priv->soc_info, 0, offset);
		CAM_INFO(CAM_ISP, "offset 0x%x value 0x%x", offset, val);
	}

	for (offset = 0x2000; offset <= 0x20B8; offset += 0x4) {
		val = cam_soc_util_r(camif_priv->soc_info, 0, offset);
		CAM_INFO(CAM_ISP, "offset 0x%x value 0x%x", offset, val);
	}

	for (wm_idx = 0; wm_idx <= 23; wm_idx++) {
		for (offset = 0x2200 + 0x100 * wm_idx;
			offset < 0x2278 + 0x100 * wm_idx; offset += 0x4) {
			val = cam_soc_util_r(camif_priv->soc_info, 0, offset);
			CAM_INFO(CAM_ISP,
				"offset 0x%x value 0x%x", offset, val);
		}
	}

	soc_private = camif_priv->soc_info->soc_private;
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

static int cam_vfe_camif_resource_stop(
	struct cam_isp_resource_node        *camif_res)
{
	struct cam_vfe_mux_camif_data       *camif_priv;
	struct cam_vfe_camif_ver2_reg       *camif_reg;
	int rc = 0;
	uint32_t val = 0;

	if (!camif_res) {
		CAM_ERR(CAM_ISP, "Error! Invalid input arguments");
		return -EINVAL;
	}

	if (camif_res->res_state == CAM_ISP_RESOURCE_STATE_RESERVED ||
		camif_res->res_state == CAM_ISP_RESOURCE_STATE_AVAILABLE)
		return 0;

	camif_priv = (struct cam_vfe_mux_camif_data *)camif_res->res_priv;
	camif_reg = camif_priv->camif_reg;

	if ((camif_priv->dsp_mode >= CAM_ISP_DSP_MODE_ONE_WAY) &&
		(camif_priv->dsp_mode <= CAM_ISP_DSP_MODE_ROUND)) {
		val = cam_io_r_mb(camif_priv->mem_base +
				camif_priv->common_reg->core_cfg);
		val &= (~(1 << camif_priv->reg_data->dsp_en_shift));
		cam_io_w_mb(val, camif_priv->mem_base +
			camif_priv->common_reg->core_cfg);
	}

	if (camif_res->res_state == CAM_ISP_RESOURCE_STATE_STREAMING)
		camif_res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;

	val = cam_io_r_mb(camif_priv->mem_base +
			camif_priv->camif_reg->vfe_diag_config);
	if (val & camif_priv->reg_data->enable_diagnostic_hw) {
		val &= ~camif_priv->reg_data->enable_diagnostic_hw;
		cam_io_w_mb(val, camif_priv->mem_base +
			camif_priv->camif_reg->vfe_diag_config);
	}

	if (camif_priv->irq_handle) {
		cam_irq_controller_unsubscribe_irq(
			camif_priv->vfe_irq_controller, camif_priv->irq_handle);
		camif_priv->irq_handle = 0;
	}

	if (camif_priv->irq_err_handle) {
		cam_irq_controller_unsubscribe_irq(
			camif_priv->vfe_irq_controller,
			camif_priv->irq_err_handle);
		camif_priv->irq_err_handle = 0;
	}

	return rc;
}

static int cam_vfe_camif_sof_irq_debug(
	struct cam_isp_resource_node *rsrc_node, void *cmd_args)
{
	struct cam_vfe_mux_camif_data *camif_priv;
	uint32_t *enable_sof_irq = (uint32_t *)cmd_args;

	camif_priv =
		(struct cam_vfe_mux_camif_data *)rsrc_node->res_priv;

	if (*enable_sof_irq == 1)
		camif_priv->enable_sof_irq_debug = true;
	else
		camif_priv->enable_sof_irq_debug = false;

	return 0;
}

int cam_vfe_camif_dump_timestamps(
	struct cam_isp_resource_node *rsrc_node, void *cmd_args)
{
	struct cam_vfe_mux_camif_data *camif_priv =
		(struct cam_vfe_mux_camif_data *)rsrc_node->res_priv;

	CAM_INFO(CAM_ISP,
		"CAMIF ERROR time %lld:%lld SOF %lld:%lld EPOCH %lld:%lld EOF %lld:%lld",
		camif_priv->error_ts.tv_sec,
		camif_priv->error_ts.tv_usec,
		camif_priv->sof_ts.tv_sec,
		camif_priv->sof_ts.tv_usec,
		camif_priv->epoch_ts.tv_sec,
		camif_priv->epoch_ts.tv_usec,
		camif_priv->eof_ts.tv_sec,
		camif_priv->eof_ts.tv_usec);

	return 0;
}

static int cam_vfe_camif_blanking_update(
	struct cam_isp_resource_node *rsrc_node, void *cmd_args)
{
	struct cam_vfe_mux_camif_data *camif_priv =
		(struct cam_vfe_mux_camif_data *)rsrc_node->res_priv;

	struct cam_isp_blanking_config  *blanking_config =
		(struct cam_isp_blanking_config *)cmd_args;

	camif_priv->hbi_value = blanking_config->hbi;
	camif_priv->vbi_value = blanking_config->vbi;

	CAM_DBG(CAM_ISP, "hbi:%d vbi:%d",
		camif_priv->hbi_value, camif_priv->vbi_value);
	return 0;
}

static int cam_vfe_camif_process_cmd(struct cam_isp_resource_node *rsrc_node,
	uint32_t cmd_type, void *cmd_args, uint32_t arg_size)
{
	int rc = -EINVAL;
	struct cam_vfe_mux_camif_data *camif_priv = NULL;

	if (!rsrc_node || !cmd_args) {
		CAM_ERR(CAM_ISP, "Invalid input arguments");
		return -EINVAL;
	}

	switch (cmd_type) {
	case CAM_ISP_HW_CMD_GET_REG_UPDATE:
		rc = cam_vfe_camif_get_reg_update(rsrc_node, cmd_args,
			arg_size);
		break;
	case CAM_ISP_HW_CMD_SOF_IRQ_DEBUG:
		rc = cam_vfe_camif_sof_irq_debug(rsrc_node, cmd_args);
		break;
	case CAM_ISP_HW_CMD_SET_CAMIF_DEBUG:
		camif_priv =
			(struct cam_vfe_mux_camif_data *)rsrc_node->res_priv;
		camif_priv->camif_debug = *((uint32_t *)cmd_args);
		break;
	case CAM_ISP_HW_CMD_CAMIF_DATA:
		rc = cam_vfe_camif_dump_timestamps(rsrc_node, cmd_args);
		break;
	case CAM_ISP_HW_CMD_BLANKING_UPDATE:
		rc = cam_vfe_camif_blanking_update(rsrc_node, cmd_args);
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
	int32_t                                rc;
	int                                    i;
	struct cam_isp_resource_node          *camif_node;
	struct cam_vfe_mux_camif_data         *camif_priv;
	struct cam_vfe_top_irq_evt_payload  *evt_payload;

	camif_node = th_payload->handler_priv;
	camif_priv = camif_node->res_priv;

	CAM_DBG(CAM_ISP, "IRQ status_0 = %x", th_payload->evt_status_arr[0]);
	CAM_DBG(CAM_ISP, "IRQ status_1 = %x", th_payload->evt_status_arr[1]);

	rc  = cam_vfe_camif_get_evt_payload(camif_priv, &evt_payload);
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

static int cam_vfe_camif_handle_irq_bottom_half(void *handler_priv,
	void *evt_payload_priv)
{
	int                                   ret = CAM_VFE_IRQ_STATUS_MAX;
	struct cam_isp_resource_node         *camif_node;
	struct cam_vfe_mux_camif_data        *camif_priv;
	struct cam_vfe_top_irq_evt_payload *payload;
	struct cam_isp_hw_event_info          evt_info;
	struct cam_hw_soc_info               *soc_info = NULL;
	struct cam_vfe_soc_private           *soc_private = NULL;
	uint32_t                              irq_status0;
	uint32_t                              irq_status1;
	uint32_t                              val;
	struct timespec64                     ts;

	if (!handler_priv || !evt_payload_priv) {
		CAM_ERR(CAM_ISP, "Invalid params");
		return ret;
	}

	camif_node = handler_priv;
	camif_priv = camif_node->res_priv;
	payload = evt_payload_priv;
	irq_status0 = payload->irq_reg_val[CAM_IFE_IRQ_CAMIF_REG_STATUS0];
	irq_status1 = payload->irq_reg_val[CAM_IFE_IRQ_CAMIF_REG_STATUS1];

	soc_info = camif_priv->soc_info;
	soc_private = (struct cam_vfe_soc_private *)soc_info->soc_private;

	evt_info.hw_idx   = camif_node->hw_intf->hw_idx;
	evt_info.res_id   = camif_node->res_id;
	evt_info.res_type = camif_node->res_type;

	CAM_DBG(CAM_ISP, "irq_status_0 = 0x%x irq_status_1 = 0x%x",
		irq_status0, irq_status1);

	if (irq_status0 & camif_priv->reg_data->eof_irq_mask) {
		CAM_DBG(CAM_ISP, "Received EOF");
		camif_priv->eof_ts.tv_sec =
			payload->ts.mono_time.tv_sec;
		camif_priv->eof_ts.tv_usec =
			payload->ts.mono_time.tv_usec;

		if (camif_priv->event_cb)
			camif_priv->event_cb(camif_priv->priv,
				CAM_ISP_HW_EVENT_EOF, (void *)&evt_info);

		ret = CAM_VFE_IRQ_STATUS_SUCCESS;
	}

	if (irq_status0 & camif_priv->reg_data->sof_irq_mask) {
		if ((camif_priv->enable_sof_irq_debug) &&
			(camif_priv->irq_debug_cnt <=
			CAM_VFE_CAMIF_IRQ_SOF_DEBUG_CNT_MAX)) {
			CAM_INFO_RATE_LIMIT(CAM_ISP, "Received SOF");

			camif_priv->irq_debug_cnt++;
			if (camif_priv->irq_debug_cnt ==
				CAM_VFE_CAMIF_IRQ_SOF_DEBUG_CNT_MAX) {
				camif_priv->enable_sof_irq_debug =
					false;
				camif_priv->irq_debug_cnt = 0;
			}
		} else {
			CAM_DBG(CAM_ISP, "Received SOF");
			camif_priv->sof_ts.tv_sec =
				payload->ts.mono_time.tv_sec;
			camif_priv->sof_ts.tv_usec =
				payload->ts.mono_time.tv_usec;
		}

		if (camif_priv->event_cb)
			camif_priv->event_cb(camif_priv->priv,
				CAM_ISP_HW_EVENT_SOF, (void *)&evt_info);

		ret = CAM_VFE_IRQ_STATUS_SUCCESS;
	}

	if (irq_status0 & camif_priv->reg_data->reg_update_irq_mask) {
		CAM_DBG(CAM_ISP, "Received REG_UPDATE_ACK");

		if (camif_priv->event_cb)
			camif_priv->event_cb(camif_priv->priv,
				CAM_ISP_HW_EVENT_REG_UPDATE, (void *)&evt_info);

		ret = CAM_VFE_IRQ_STATUS_SUCCESS;
	}

	if (irq_status0 & camif_priv->reg_data->epoch0_irq_mask) {
		CAM_DBG(CAM_ISP, "Received EPOCH");
		camif_priv->epoch_ts.tv_sec =
			payload->ts.mono_time.tv_sec;
		camif_priv->epoch_ts.tv_usec =
			payload->ts.mono_time.tv_usec;

		if (camif_priv->event_cb)
			camif_priv->event_cb(camif_priv->priv,
				CAM_ISP_HW_EVENT_EPOCH, (void *)&evt_info);

		ret = CAM_VFE_IRQ_STATUS_SUCCESS;
	}

	if (irq_status0 & camif_priv->reg_data->error_irq_mask0) {
		CAM_DBG(CAM_ISP, "Received ERROR");

		ktime_get_boottime_ts64(&ts);
		CAM_INFO(CAM_ISP,
			"current monotonic time stamp seconds %lld:%lld",
			ts.tv_sec, ts.tv_nsec/1000);

		if (camif_priv->event_cb)
			camif_priv->event_cb(camif_priv->priv,
				CAM_ISP_HW_EVENT_ERROR, (void *)&evt_info);

		CAM_INFO(CAM_ISP, "Violation status = %x",
			payload->irq_reg_val[2]);

		ret = CAM_VFE_IRQ_STATUS_OVERFLOW;

		CAM_INFO(CAM_ISP, "ife_clk_src:%lld",
			soc_private->ife_clk_src);

		cam_cpas_log_votes();

		if (camif_priv->camif_debug & CAMIF_DEBUG_ENABLE_REG_DUMP)
			cam_vfe_camif_reg_dump(camif_node->res_priv);
	}

	if (irq_status1 & camif_priv->reg_data->error_irq_mask1) {
		CAM_DBG(CAM_ISP, "Received ERROR");

		ktime_get_boottime_ts64(&ts);
		CAM_INFO(CAM_ISP,
			"current monotonic time stamp seconds %lld:%lld",
			ts.tv_sec, ts.tv_nsec/1000);

		if (camif_priv->event_cb)
			camif_priv->event_cb(camif_priv->priv,
				CAM_ISP_HW_EVENT_ERROR, (void *)&evt_info);

		CAM_INFO(CAM_ISP, "Violation status = %x",
			payload->irq_reg_val[2]);

		ret = CAM_VFE_IRQ_STATUS_OVERFLOW;

		CAM_INFO(CAM_ISP, "ife_clk_src:%lld",
			soc_private->ife_clk_src);

		cam_cpas_log_votes();

		if (camif_priv->camif_debug & CAMIF_DEBUG_ENABLE_REG_DUMP)
			cam_vfe_camif_reg_dump(camif_node->res_priv);
	}

	if (camif_priv->camif_debug & CAMIF_DEBUG_ENABLE_SENSOR_DIAG_STATUS) {
		val = cam_io_r(camif_priv->mem_base +
			camif_priv->camif_reg->vfe_diag_sensor_status);
		CAM_DBG(CAM_ISP, "VFE_DIAG_SENSOR_STATUS: 0x%x",
			camif_priv->mem_base, val);
	}

	cam_vfe_camif_put_evt_payload(camif_priv, &payload);

	CAM_DBG(CAM_ISP, "returning status = %d", ret);
	return ret;
}

int cam_vfe_camif_ver2_init(
	struct cam_hw_intf            *hw_intf,
	struct cam_hw_soc_info        *soc_info,
	void                          *camif_hw_info,
	struct cam_isp_resource_node  *camif_node,
	void                          *vfe_irq_controller)
{
	struct cam_vfe_mux_camif_data     *camif_priv = NULL;
	struct cam_vfe_camif_ver2_hw_info *camif_info = camif_hw_info;
	int                                i = 0;

	camif_priv = kzalloc(sizeof(struct cam_vfe_mux_camif_data),
		GFP_KERNEL);
	if (!camif_priv) {
		CAM_DBG(CAM_ISP, "Error! Failed to alloc for camif_priv");
		return -ENOMEM;
	}

	camif_node->res_priv = camif_priv;

	camif_priv->mem_base    = soc_info->reg_map[VFE_CORE_BASE_IDX].mem_base;
	camif_priv->camif_reg   = camif_info->camif_reg;
	camif_priv->common_reg  = camif_info->common_reg;
	camif_priv->reg_data    = camif_info->reg_data;
	camif_priv->hw_intf     = hw_intf;
	camif_priv->soc_info    = soc_info;
	camif_priv->vfe_irq_controller = vfe_irq_controller;

	camif_node->init    = cam_vfe_camif_resource_init;
	camif_node->deinit  = cam_vfe_camif_resource_deinit;
	camif_node->start   = cam_vfe_camif_resource_start;
	camif_node->stop    = cam_vfe_camif_resource_stop;
	camif_node->process_cmd = cam_vfe_camif_process_cmd;
	camif_node->top_half_handler = cam_vfe_camif_handle_irq_top_half;
	camif_node->bottom_half_handler = cam_vfe_camif_handle_irq_bottom_half;

	spin_lock_init(&camif_priv->spin_lock);
	INIT_LIST_HEAD(&camif_priv->free_payload_list);
	for (i = 0; i < CAM_VFE_CAMIF_EVT_MAX; i++) {
		INIT_LIST_HEAD(&camif_priv->evt_payload[i].list);
		list_add_tail(&camif_priv->evt_payload[i].list,
			&camif_priv->free_payload_list);
	}

	return 0;
}

int cam_vfe_camif_ver2_deinit(
	struct cam_isp_resource_node  *camif_node)
{
	struct cam_vfe_mux_camif_data *camif_priv = camif_node->res_priv;
	int                            i = 0;

	INIT_LIST_HEAD(&camif_priv->free_payload_list);
	for (i = 0; i < CAM_VFE_CAMIF_EVT_MAX; i++)
		INIT_LIST_HEAD(&camif_priv->evt_payload[i].list);

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
