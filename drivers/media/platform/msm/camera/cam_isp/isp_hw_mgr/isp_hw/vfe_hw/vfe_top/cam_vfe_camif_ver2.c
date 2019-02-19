/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
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

	enum cam_isp_hw_sync_mode          sync_mode;
	uint32_t                           dsp_mode;
	uint32_t                           pix_pattern;
	uint32_t                           first_pixel;
	uint32_t                           first_line;
	uint32_t                           last_pixel;
	uint32_t                           last_line;
	bool                               enable_sof_irq_debug;
	uint32_t                           irq_debug_cnt;
	uint32_t                           camif_debug;
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
	struct cam_isp_resource_node        *camif_res)
{
	struct cam_vfe_mux_camif_data       *rsrc_data;
	uint32_t                             val = 0;
	uint32_t                             epoch0_irq_mask;
	uint32_t                             epoch1_irq_mask;
	uint32_t                             computed_epoch_line_cfg;
	struct cam_vfe_soc_private          *soc_private;

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
		cam_io_w_mb(rsrc_data->reg_data->epoch_line_cfg,
				rsrc_data->mem_base +
				rsrc_data->camif_reg->epoch_irq);
		break;
	default:
		epoch0_irq_mask = ((rsrc_data->last_line -
				rsrc_data->first_line) / 2) +
				rsrc_data->first_line;
		epoch1_irq_mask = rsrc_data->reg_data->epoch_line_cfg &
				0xFFFF;
		computed_epoch_line_cfg = (epoch0_irq_mask << 16) |
				epoch1_irq_mask;
		cam_io_w_mb(computed_epoch_line_cfg,
				rsrc_data->mem_base +
				rsrc_data->camif_reg->epoch_irq);
		CAM_DBG(CAM_ISP, "first_line: %u\n"
				"last_line: %u\n"
				"epoch_line_cfg: 0x%x",
				rsrc_data->first_line,
				rsrc_data->last_line,
				computed_epoch_line_cfg);
		break;
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

	CAM_DBG(CAM_ISP, "Start Camif IFE %d Done", camif_res->hw_intf->hw_idx);
	return 0;
}

static int cam_vfe_camif_reg_dump(
	struct cam_isp_resource_node *camif_res)
{
	struct cam_vfe_mux_camif_data *camif_priv;
	struct cam_vfe_soc_private *soc_private;
	int rc = 0, i;
	uint32_t val = 0;

	if (!camif_res) {
		CAM_ERR(CAM_ISP, "Error! Invalid input arguments");
		return -EINVAL;
	}

	if ((camif_res->res_state == CAM_ISP_RESOURCE_STATE_RESERVED) ||
		(camif_res->res_state == CAM_ISP_RESOURCE_STATE_AVAILABLE))
		return 0;

	camif_priv = (struct cam_vfe_mux_camif_data *)camif_res->res_priv;
	soc_private = camif_priv->soc_info->soc_private;
	for (i = 0xA3C; i <= 0xA90; i += 4) {
		val = cam_io_r_mb(camif_priv->mem_base + i);
		CAM_INFO(CAM_ISP, "offset 0x%x val 0x%x", i, val);
	}

	for (i = 0xE0C; i <= 0xE3C; i += 4) {
		val = cam_io_r_mb(camif_priv->mem_base + i);
		CAM_INFO(CAM_ISP, "offset 0x%x val 0x%x", i, val);
	}

	for (i = 0x2000; i <= 0x20B8; i += 4) {
		val = cam_io_r_mb(camif_priv->mem_base + i);
		CAM_INFO(CAM_ISP, "offset 0x%x val 0x%x", i, val);
	}

	for (i = 0x2500; i <= 0x255C; i += 4) {
		val = cam_io_r_mb(camif_priv->mem_base + i);
		CAM_INFO(CAM_ISP, "offset 0x%x val 0x%x", i, val);
	}

	for (i = 0x2600; i <= 0x265C; i += 4) {
		val = cam_io_r_mb(camif_priv->mem_base + i);
		CAM_INFO(CAM_ISP, "offset 0x%x val 0x%x", i, val);
	}

	if (soc_private->cpas_version == CAM_CPAS_TITAN_175_V120) {
		cam_cpas_reg_read(soc_private->cpas_handle[0],
			CAM_CPAS_REG_CAMNOC, 0x3A20, true, &val);
		CAM_INFO(CAM_ISP, "IFE0_nRDI_MAXWR_LOW offset 0x3A20 val 0x%x",
			val);

		cam_cpas_reg_read(soc_private->cpas_handle[0],
			CAM_CPAS_REG_CAMNOC, 0x5420, true, &val);
		CAM_INFO(CAM_ISP, "IFE1_nRDI_MAXWR_LOW offset 0x5420 val 0x%x",
			val);

		cam_cpas_reg_read(soc_private->cpas_handle[1],
			CAM_CPAS_REG_CAMNOC, 0x3620, true, &val);
		CAM_INFO(CAM_ISP,
			"IFE0123_RDI_WR_MAXWR_LOW offset 0x3620 val 0x%x", val);
	} else {
		cam_cpas_reg_read(soc_private->cpas_handle[0],
			CAM_CPAS_REG_CAMNOC, 0x420, true, &val);
		CAM_INFO(CAM_ISP, "IFE02_MAXWR_LOW offset 0x420 val 0x%x", val);

		cam_cpas_reg_read(soc_private->cpas_handle[0],
			CAM_CPAS_REG_CAMNOC, 0x820, true, &val);
		CAM_INFO(CAM_ISP, "IFE13_MAXWR_LOW offset 0x820 val 0x%x", val);
	}
	return rc;
}

static int cam_vfe_camif_reg_dump_bh(struct cam_vfe_mux_camif_data *camif_priv)
{
	uint32_t offset, val, wm_idx;

	for (offset = 0x0; offset < 0x1000; offset += 0x4) {
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

	offset = 0x420;
	val = cam_soc_util_r(camif_priv->soc_info, 1, offset);
	CAM_INFO(CAM_ISP, "CAMNOC IFE02 MaxWR_LOW offset 0x%x value 0x%x",
		offset, val);

	offset = 0x820;
	val = cam_soc_util_r(camif_priv->soc_info, 1, offset);
	CAM_INFO(CAM_ISP, "CAMNOC IFE13 MaxWR_LOW offset 0x%x value 0x%x",
		offset, val);

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
	case CAM_ISP_HW_CMD_GET_REG_DUMP:
		rc = cam_vfe_camif_reg_dump(rsrc_node);
		break;
	case CAM_ISP_HW_CMD_SOF_IRQ_DEBUG:
		rc = cam_vfe_camif_sof_irq_debug(rsrc_node, cmd_args);
		break;
	case CAM_ISP_HW_CMD_SET_CAMIF_DEBUG:
		camif_priv =
			(struct cam_vfe_mux_camif_data *)rsrc_node->res_priv;
		camif_priv->camif_debug = *((uint32_t *)cmd_args);
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
	uint32_t                              irq_status1;
	uint32_t                              val;

	if (!handler_priv || !evt_payload_priv) {
		CAM_ERR(CAM_ISP, "Invalid params");
		return ret;
	}

	camif_node = handler_priv;
	camif_priv = camif_node->res_priv;
	payload = evt_payload_priv;
	irq_status0 = payload->irq_reg_val[CAM_IFE_IRQ_CAMIF_REG_STATUS0];
	irq_status1 = payload->irq_reg_val[CAM_IFE_IRQ_CAMIF_REG_STATUS1];

	CAM_DBG(CAM_ISP, "event ID:%d", payload->evt_id);
	CAM_DBG(CAM_ISP, "irq_status_0 = %x", irq_status0);

	switch (payload->evt_id) {
	case CAM_ISP_HW_EVENT_SOF:
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
			}
			ret = CAM_VFE_IRQ_STATUS_SUCCESS;
		}
		break;
	case CAM_ISP_HW_EVENT_EPOCH:
		if (irq_status0 & camif_priv->reg_data->epoch0_irq_mask) {
			CAM_DBG(CAM_ISP, "Received EPOCH");
			ret = CAM_VFE_IRQ_STATUS_SUCCESS;
		}
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
	case CAM_ISP_HW_EVENT_ERROR:
		if (irq_status1 & camif_priv->reg_data->error_irq_mask1) {
			CAM_DBG(CAM_ISP, "Received ERROR\n");
			ret = CAM_ISP_HW_ERROR_OVERFLOW;
			cam_vfe_camif_reg_dump_bh(camif_node->res_priv);
		} else {
			ret = CAM_ISP_HW_ERROR_NONE;
		}

		if (camif_priv->camif_debug &
			CAMIF_DEBUG_ENABLE_SENSOR_DIAG_STATUS) {
			val = cam_io_r(camif_priv->mem_base +
				camif_priv->camif_reg->vfe_diag_sensor_status);
			CAM_DBG(CAM_ISP, "VFE_DIAG_SENSOR_STATUS: 0x%x",
				camif_priv->mem_base, val);
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

	camif_priv->mem_base    = soc_info->reg_map[VFE_CORE_BASE_IDX].mem_base;
	camif_priv->camif_reg   = camif_info->camif_reg;
	camif_priv->common_reg  = camif_info->common_reg;
	camif_priv->reg_data    = camif_info->reg_data;
	camif_priv->hw_intf     = hw_intf;
	camif_priv->soc_info    = soc_info;

	camif_node->init    = cam_vfe_camif_resource_init;
	camif_node->deinit  = cam_vfe_camif_resource_deinit;
	camif_node->start   = cam_vfe_camif_resource_start;
	camif_node->stop    = cam_vfe_camif_resource_stop;
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
