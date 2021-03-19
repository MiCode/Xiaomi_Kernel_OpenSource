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
#include "cam_vfe_camif_ver3.h"
#include "cam_debug_util.h"
#include "cam_cdm_util.h"
#include "cam_cpas_api.h"

#define CAM_VFE_CAMIF_IRQ_SOF_DEBUG_CNT_MAX 2

struct cam_vfe_mux_camif_ver3_data {
	void __iomem                                *mem_base;
	struct cam_hw_intf                          *hw_intf;
	struct cam_vfe_camif_ver3_pp_clc_reg        *camif_reg;
	struct cam_vfe_top_ver3_reg_offset_common   *common_reg;
	struct cam_vfe_camif_ver3_reg_data          *reg_data;
	struct cam_hw_soc_info                      *soc_info;
	struct cam_vfe_camif_common_cfg             cam_common_cfg;

	cam_hw_mgr_event_cb_func             event_cb;
	void                                *priv;
	int                                  irq_err_handle;
	int                                  irq_handle;
	int                                  sof_irq_handle;
	void                                *vfe_irq_controller;
	struct cam_vfe_top_irq_evt_payload   evt_payload[CAM_VFE_CAMIF_EVT_MAX];
	struct list_head                     free_payload_list;
	spinlock_t                           spin_lock;

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
	uint32_t                           horizontal_bin;
	uint32_t                           qcfa_bin;
};

static int cam_vfe_camif_ver3_get_evt_payload(
	struct cam_vfe_mux_camif_ver3_data     *camif_priv,
	struct cam_vfe_top_irq_evt_payload    **evt_payload)
{
	int rc = 0;

	spin_lock(&camif_priv->spin_lock);
	if (list_empty(&camif_priv->free_payload_list)) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "No free CAMIF event payload");
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

static int cam_vfe_camif_ver3_put_evt_payload(
	struct cam_vfe_mux_camif_ver3_data     *camif_priv,
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

static int cam_vfe_camif_ver3_err_irq_top_half(
	uint32_t                               evt_id,
	struct cam_irq_th_payload             *th_payload)
{
	int32_t                                rc;
	int                                    i;
	struct cam_isp_resource_node          *camif_node;
	struct cam_vfe_mux_camif_ver3_data    *camif_priv;
	struct cam_vfe_top_irq_evt_payload    *evt_payload;
	bool                                   error_flag = false;

	camif_node = th_payload->handler_priv;
	camif_priv = camif_node->res_priv;
	/*
	 *  need to handle overflow condition here, otherwise irq storm
	 *  will block everything
	 */
	if (th_payload->evt_status_arr[2] || (th_payload->evt_status_arr[0] &
		camif_priv->reg_data->error_irq_mask0)) {
		CAM_ERR(CAM_ISP,
			"VFE:%d CAMIF Err IRQ status_0: 0x%X status_2: 0x%X",
			camif_node->hw_intf->hw_idx,
			th_payload->evt_status_arr[0],
			th_payload->evt_status_arr[2]);
		CAM_ERR(CAM_ISP, "Stopping further IRQ processing from VFE:%d",
			camif_node->hw_intf->hw_idx);
		cam_irq_controller_disable_irq(camif_priv->vfe_irq_controller,
			camif_priv->irq_err_handle);
		cam_irq_controller_clear_and_mask(evt_id,
			camif_priv->vfe_irq_controller);
		error_flag = true;
	}

	rc  = cam_vfe_camif_ver3_get_evt_payload(camif_priv, &evt_payload);
	if (rc)
		return rc;

	cam_isp_hw_get_timestamp(&evt_payload->ts);

	for (i = 0; i < th_payload->num_registers; i++)
		evt_payload->irq_reg_val[i] = th_payload->evt_status_arr[i];

	evt_payload->irq_reg_val[i] = cam_io_r(camif_priv->mem_base +
		camif_priv->common_reg->violation_status);

	evt_payload->irq_reg_val[++i] = cam_io_r(camif_priv->mem_base +
		camif_priv->common_reg->bus_overflow_status);

	th_payload->evt_payload_priv = evt_payload;

	return rc;
}

static int cam_vfe_camif_ver3_validate_pix_pattern(uint32_t pattern)
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
		CAM_ERR(CAM_ISP, "Error, Invalid pix pattern:%d", pattern);
		rc = -EINVAL;
		break;
	}
	return rc;
}

static int cam_vfe_camif_ver3_get_reg_update(
	struct cam_isp_resource_node  *camif_res,
	void *cmd_args, uint32_t arg_size)
{
	uint32_t                           size = 0;
	uint32_t                           reg_val_pair[2];
	struct cam_isp_hw_get_cmd_update   *cdm_args = cmd_args;
	struct cam_cdm_utils_ops           *cdm_util_ops = NULL;
	struct cam_vfe_mux_camif_ver3_data *rsrc_data = NULL;

	if (arg_size != sizeof(struct cam_isp_hw_get_cmd_update)) {
		CAM_ERR(CAM_ISP, "Invalid arg size: %d expected:%ld",
			arg_size, sizeof(struct cam_isp_hw_get_cmd_update));
		return -EINVAL;
	}

	if (!cdm_args || !cdm_args->res) {
		CAM_ERR(CAM_ISP, "Invalid args: %pK", cdm_args);
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
			cdm_args->cmd.size, (size*4));
		return -EINVAL;
	}

	rsrc_data = camif_res->res_priv;
	reg_val_pair[0] = rsrc_data->camif_reg->reg_update_cmd;
	reg_val_pair[1] = rsrc_data->reg_data->reg_update_cmd_data;
	CAM_DBG(CAM_ISP, "VFE:%d CAMIF reg_update_cmd 0x%X offset 0x%X",
		camif_res->hw_intf->hw_idx,
		reg_val_pair[1], reg_val_pair[0]);

	cdm_util_ops->cdm_write_regrandom(cdm_args->cmd.cmd_buf_addr,
		1, reg_val_pair);

	cdm_args->cmd.used_bytes = size * 4;

	return 0;
}

int cam_vfe_camif_ver3_acquire_resource(
	struct cam_isp_resource_node  *camif_res,
	void                          *acquire_param)
{
	struct cam_vfe_mux_camif_ver3_data    *camif_data;
	struct cam_vfe_acquire_args           *acquire_data;
	int                                    rc = 0;

	camif_data  = (struct cam_vfe_mux_camif_ver3_data *)
		camif_res->res_priv;
	acquire_data = (struct cam_vfe_acquire_args *)acquire_param;

	rc = cam_vfe_camif_ver3_validate_pix_pattern(
		acquire_data->vfe_in.in_port->test_pattern);

	if (rc) {
		CAM_ERR(CAM_ISP, "Validate pix pattern failed, rc = %d", rc);
		return rc;
	}

	camif_data->sync_mode   = acquire_data->vfe_in.sync_mode;
	camif_data->pix_pattern = acquire_data->vfe_in.in_port->test_pattern;
	camif_data->dsp_mode    = acquire_data->vfe_in.in_port->dsp_mode;
	camif_data->first_pixel = acquire_data->vfe_in.in_port->left_start;
	camif_data->last_pixel  = acquire_data->vfe_in.in_port->left_stop;
	camif_data->first_line  = acquire_data->vfe_in.in_port->line_start;
	camif_data->last_line   = acquire_data->vfe_in.in_port->line_stop;
	camif_data->horizontal_bin =
		acquire_data->vfe_in.in_port->horizontal_bin;
	camif_data->qcfa_bin    = acquire_data->vfe_in.in_port->qcfa_bin;
	camif_data->event_cb    = acquire_data->event_cb;
	camif_data->priv        = acquire_data->priv;

	CAM_DBG(CAM_ISP, "VFE:%d CAMIF pix_pattern:%d dsp_mode=%d",
		camif_res->hw_intf->hw_idx,
		camif_data->pix_pattern, camif_data->dsp_mode);

	return rc;
}

static int cam_vfe_camif_ver3_resource_init(
	struct cam_isp_resource_node *camif_res,
	void *init_args, uint32_t arg_size)
{
	struct cam_vfe_mux_camif_ver3_data    *camif_data;
	struct cam_hw_soc_info                *soc_info;
	int                                    rc = 0;

	if (!camif_res) {
		CAM_ERR(CAM_ISP, "Error Invalid input arguments");
		return -EINVAL;
	}

	camif_data = (struct cam_vfe_mux_camif_ver3_data *)
		camif_res->res_priv;

	soc_info = camif_data->soc_info;

	if ((camif_data->dsp_mode >= CAM_ISP_DSP_MODE_ONE_WAY) &&
		(camif_data->dsp_mode <= CAM_ISP_DSP_MODE_ROUND)) {
		rc = cam_vfe_soc_enable_clk(soc_info, CAM_VFE_DSP_CLK_NAME);
		if (rc)
			CAM_ERR(CAM_ISP,
				"failed to enable dsp clk, rc = %d", rc);
	}

	/* All auto clock gating disabled by default */
	CAM_INFO(CAM_ISP, "overriding clock gating");
	cam_io_w_mb(0xFFFFFFFF, camif_data->mem_base +
		camif_data->common_reg->core_cgc_ovd_0);

	cam_io_w_mb(0xFF, camif_data->mem_base +
		camif_data->common_reg->core_cgc_ovd_1);

	cam_io_w_mb(0x1, camif_data->mem_base +
		camif_data->common_reg->ahb_cgc_ovd);

	cam_io_w_mb(0x1, camif_data->mem_base +
		camif_data->common_reg->noc_cgc_ovd);

	return rc;
}

static int cam_vfe_camif_ver3_resource_deinit(
	struct cam_isp_resource_node        *camif_res,
	void *init_args, uint32_t arg_size)
{
	struct cam_vfe_mux_camif_ver3_data    *camif_data;
	struct cam_hw_soc_info           *soc_info;
	int rc = 0;

	if (!camif_res) {
		CAM_ERR(CAM_ISP, "Error Invalid input arguments");
		return -EINVAL;
	}

	camif_data   = (struct cam_vfe_mux_camif_ver3_data *)
		camif_res->res_priv;

	soc_info = camif_data->soc_info;

	if ((camif_data->dsp_mode >= CAM_ISP_DSP_MODE_ONE_WAY) &&
		(camif_data->dsp_mode <= CAM_ISP_DSP_MODE_ROUND)) {
		rc = cam_vfe_soc_disable_clk(soc_info, CAM_VFE_DSP_CLK_NAME);
		if (rc)
			CAM_ERR(CAM_ISP, "failed to disable dsp clk");
	}

	return rc;
}

static int cam_vfe_camif_ver3_resource_start(
	struct cam_isp_resource_node *camif_res)
{
	struct cam_vfe_mux_camif_ver3_data  *rsrc_data;
	uint32_t                             val = 0;
	uint32_t                             epoch0_line_cfg;
	uint32_t                             epoch1_line_cfg;
	uint32_t                             computed_epoch_line_cfg;
	int                                  rc = 0;
	uint32_t                        err_irq_mask[CAM_IFE_IRQ_REGISTERS_MAX];
	uint32_t                        irq_mask[CAM_IFE_IRQ_REGISTERS_MAX];
	struct cam_vfe_soc_private          *soc_private;

	if (!camif_res) {
		CAM_ERR(CAM_ISP, "Error, Invalid input arguments");
		return -EINVAL;
	}

	if (camif_res->res_state != CAM_ISP_RESOURCE_STATE_RESERVED) {
		CAM_ERR(CAM_ISP, "Error, Invalid camif res res_state:%d",
			camif_res->res_state);
		return -EINVAL;
	}

	memset(err_irq_mask, 0, sizeof(err_irq_mask));
	memset(irq_mask, 0, sizeof(irq_mask));

	rsrc_data = (struct cam_vfe_mux_camif_ver3_data *)camif_res->res_priv;

	soc_private = rsrc_data->soc_info->soc_private;

	if (!soc_private) {
		CAM_ERR(CAM_ISP, "Error, soc_private NULL");
		return -ENODEV;
	}

	/* config debug status registers */
	cam_io_w_mb(rsrc_data->reg_data->top_debug_cfg_en, rsrc_data->mem_base +
		rsrc_data->common_reg->top_debug_cfg);

	val = cam_io_r_mb(rsrc_data->mem_base +
		rsrc_data->common_reg->core_cfg_0);

	/* AF stitching by hw disabled by default
	 * PP CAMIF currently operates only in offline mode
	 */

	if ((rsrc_data->dsp_mode >= CAM_ISP_DSP_MODE_ONE_WAY) &&
		(rsrc_data->dsp_mode <= CAM_ISP_DSP_MODE_ROUND)) {
		/* DSP mode reg val is CAM_ISP_DSP_MODE - 1 */
		val |= (((rsrc_data->dsp_mode - 1) &
			rsrc_data->reg_data->dsp_mode_mask) <<
			rsrc_data->reg_data->dsp_mode_shift);
		val |= (0x1 << rsrc_data->reg_data->dsp_en_shift);
	}

	if (rsrc_data->sync_mode == CAM_ISP_HW_SYNC_SLAVE)
		val |= (1 << rsrc_data->reg_data->pp_extern_reg_update_shift);

	if ((rsrc_data->sync_mode == CAM_ISP_HW_SYNC_SLAVE) ||
		(rsrc_data->sync_mode == CAM_ISP_HW_SYNC_MASTER))
		val |= (1 << rsrc_data->reg_data->dual_ife_pix_en_shift);

	val |= (~rsrc_data->cam_common_cfg.vid_ds16_r2pd & 0x1) <<
		CAM_SHIFT_TOP_CORE_CFG_VID_DS16_R2PD;
	val |= (~rsrc_data->cam_common_cfg.vid_ds4_r2pd & 0x1) <<
		CAM_SHIFT_TOP_CORE_CFG_VID_DS4_R2PD;
	val |= (~rsrc_data->cam_common_cfg.disp_ds16_r2pd & 0x1) <<
		CAM_SHIFT_TOP_CORE_CFG_DISP_DS16_R2PD;
	val |= (~rsrc_data->cam_common_cfg.disp_ds4_r2pd & 0x1) <<
		CAM_SHIFT_TOP_CORE_CFG_DISP_DS4_R2PD;
	val |= (rsrc_data->cam_common_cfg.dsp_streaming_tap_point & 0x3) <<
		CAM_SHIFT_TOP_CORE_CFG_DSP_STREAMING;
	val |= (rsrc_data->cam_common_cfg.ihist_src_sel & 0x1) <<
		CAM_SHIFT_TOP_CORE_CFG_STATS_IHIST;
	val |= (rsrc_data->cam_common_cfg.hdr_be_src_sel & 0x1) <<
		CAM_SHIFT_TOP_CORE_CFG_STATS_HDR_BE;
	val |= (rsrc_data->cam_common_cfg.hdr_bhist_src_sel & 0x1) <<
		CAM_SHIFT_TOP_CORE_CFG_STATS_HDR_BHIST;
	val |= (rsrc_data->cam_common_cfg.input_mux_sel_pp & 0x3) <<
		CAM_SHIFT_TOP_CORE_CFG_INPUTMUX_PP;

	cam_io_w_mb(val, rsrc_data->mem_base +
		rsrc_data->common_reg->core_cfg_0);

	/* epoch config */
	switch (soc_private->cpas_version) {
	case CAM_CPAS_TITAN_480_V100:
		epoch0_line_cfg = (rsrc_data->last_line -
			rsrc_data->first_line) / 4;
	/* epoch line cfg will still be configured at midpoint of the
	 * frame width. We use '/ 4' instead of '/ 2'
	 * cause it is multipixel path
	 */
		if (rsrc_data->horizontal_bin || rsrc_data->qcfa_bin)
			epoch0_line_cfg >>= 1;

		epoch1_line_cfg = rsrc_data->reg_data->epoch_line_cfg &
			0xFFFF;
		computed_epoch_line_cfg = (epoch1_line_cfg << 16) |
			epoch0_line_cfg;
		cam_io_w_mb(computed_epoch_line_cfg,
			rsrc_data->mem_base +
			rsrc_data->camif_reg->epoch_irq_cfg);
		CAM_DBG(CAM_ISP, "epoch_line_cfg: 0x%X",
			computed_epoch_line_cfg);
		break;
	default:
			CAM_ERR(CAM_ISP, "Hardware version not proper: 0x%X",
				soc_private->cpas_version);
			return -EINVAL;
		break;
	}

	camif_res->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;

	/* Reg Update */
	cam_io_w_mb(rsrc_data->reg_data->reg_update_cmd_data,
		rsrc_data->mem_base + rsrc_data->camif_reg->reg_update_cmd);
	CAM_DBG(CAM_ISP, "VFE:%d CAMIF RUP val:0x%X",
		camif_res->hw_intf->hw_idx,
		rsrc_data->reg_data->reg_update_cmd_data);

	/* disable sof irq debug flag */
	rsrc_data->enable_sof_irq_debug = false;
	rsrc_data->irq_debug_cnt = 0;

	if (rsrc_data->camif_debug &
		CAMIF_DEBUG_ENABLE_SENSOR_DIAG_STATUS) {
		val = cam_io_r_mb(rsrc_data->mem_base +
			rsrc_data->common_reg->diag_config);
		val |= rsrc_data->reg_data->enable_diagnostic_hw;
		cam_io_w_mb(val, rsrc_data->mem_base +
			rsrc_data->common_reg->diag_config);
	}

	err_irq_mask[CAM_IFE_IRQ_CAMIF_REG_STATUS0] =
		rsrc_data->reg_data->error_irq_mask0;
	err_irq_mask[CAM_IFE_IRQ_CAMIF_REG_STATUS2] =
		rsrc_data->reg_data->error_irq_mask2;

	irq_mask[CAM_IFE_IRQ_CAMIF_REG_STATUS1] =
		rsrc_data->reg_data->epoch0_irq_mask |
		rsrc_data->reg_data->eof_irq_mask;

	if (!rsrc_data->irq_handle) {
		rsrc_data->irq_handle = cam_irq_controller_subscribe_irq(
			rsrc_data->vfe_irq_controller,
			CAM_IRQ_PRIORITY_3,
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

	irq_mask[CAM_IFE_IRQ_CAMIF_REG_STATUS1] =
		rsrc_data->reg_data->sof_irq_mask;

	if (!rsrc_data->sof_irq_handle) {
		rsrc_data->sof_irq_handle = cam_irq_controller_subscribe_irq(
			rsrc_data->vfe_irq_controller,
			CAM_IRQ_PRIORITY_1,
			irq_mask,
			camif_res,
			camif_res->top_half_handler,
			camif_res->bottom_half_handler,
			camif_res->tasklet_info,
			&tasklet_bh_api);

		if (rsrc_data->sof_irq_handle < 1) {
			CAM_ERR(CAM_ISP, "SOF IRQ handle subscribe failure");
			rc = -ENOMEM;
			rsrc_data->sof_irq_handle = 0;
		}
	}

	if (!rsrc_data->irq_err_handle) {
		rsrc_data->irq_err_handle = cam_irq_controller_subscribe_irq(
			rsrc_data->vfe_irq_controller,
			CAM_IRQ_PRIORITY_0,
			err_irq_mask,
			camif_res,
			cam_vfe_camif_ver3_err_irq_top_half,
			camif_res->bottom_half_handler,
			camif_res->tasklet_info,
			&tasklet_bh_api);

		if (rsrc_data->irq_err_handle < 1) {
			CAM_ERR(CAM_ISP, "Error IRQ handle subscribe failure");
			rc = -ENOMEM;
			rsrc_data->irq_err_handle = 0;
		}
	}

	CAM_DBG(CAM_ISP, "VFE:%d CAMIF Start Done", camif_res->hw_intf->hw_idx);
	return 0;
}

static int cam_vfe_camif_ver3_reg_dump(
	struct cam_isp_resource_node *camif_res)
{
	struct cam_vfe_mux_camif_ver3_data *camif_priv;
	uint32_t offset, val, wm_idx;

	if (!camif_res) {
		CAM_ERR(CAM_ISP, "Error, Invalid input arguments");
		return -EINVAL;
	}

	if ((camif_res->res_state == CAM_ISP_RESOURCE_STATE_RESERVED) ||
		(camif_res->res_state == CAM_ISP_RESOURCE_STATE_AVAILABLE))
		return 0;

	camif_priv = (struct cam_vfe_mux_camif_ver3_data *)camif_res->res_priv;

	CAM_INFO(CAM_ISP, "IFE:%d TOP", camif_res->hw_intf->hw_idx);
	for (offset = 0x0; offset <= 0x1FC; offset += 0x4) {
		if (offset == 0x1C || offset == 0x34 ||
			offset == 0x38 || offset == 0x90)
			continue;
		val = cam_soc_util_r(camif_priv->soc_info, 0, offset);
		CAM_INFO(CAM_ISP, "offset 0x%X value 0x%X", offset, val);
	}

	CAM_INFO(CAM_ISP, "IFE:%d PP CLC PREPROCESS",
		camif_res->hw_intf->hw_idx);
	for (offset = 0x2200; offset <= 0x23FC; offset += 0x4) {
		if (offset == 0x2208)
			offset = 0x2260;
		val = cam_soc_util_r(camif_priv->soc_info, 0, offset);
		CAM_INFO(CAM_ISP, "offset 0x%X value 0x%X", offset, val);
		if (offset == 0x2260)
			offset = 0x23F4;
	}

	CAM_INFO(CAM_ISP, "IFE:%d PP CLC CAMIF", camif_res->hw_intf->hw_idx);
	for (offset = 0x2600; offset <= 0x27FC; offset += 0x4) {
		if (offset == 0x2608)
			offset = 0x2660;
		val = cam_soc_util_r(camif_priv->soc_info, 0, offset);
		CAM_INFO(CAM_ISP, "offset 0x%X value 0x%X", offset, val);
		if (offset == 0x2660)
			offset = 0x2664;
		else if (offset == 0x2680)
			offset = 0x27EC;
	}

	CAM_INFO(CAM_ISP, "IFE:%d PP CLC Modules", camif_res->hw_intf->hw_idx);
	for (offset = 0x2800; offset <= 0x8FFC; offset += 0x4) {
		val = cam_soc_util_r(camif_priv->soc_info, 0, offset);
		CAM_INFO(CAM_ISP, "offset 0x%X value 0x%X", offset, val);
	}

	CAM_INFO(CAM_ISP, "IFE:%d BUS WR", camif_res->hw_intf->hw_idx);
	for (offset = 0xAA00; offset <= 0xAADC; offset += 0x4) {
		val = cam_soc_util_r(camif_priv->soc_info, 0, offset);
		CAM_DBG(CAM_ISP, "offset 0x%X value 0x%X", offset, val);
	}

	for (wm_idx = 0; wm_idx <= 25; wm_idx++) {
		for (offset = 0xAC00 + 0x100 * wm_idx;
			offset < 0xAC84 + 0x100 * wm_idx; offset += 0x4) {
			val = cam_soc_util_r(camif_priv->soc_info, 0, offset);
			CAM_INFO(CAM_ISP, "offset 0x%X value 0x%X",
				offset, val);
		}
	}

	return 0;
}

static int cam_vfe_camif_ver3_resource_stop(
	struct cam_isp_resource_node *camif_res)
{
	struct cam_vfe_mux_camif_ver3_data        *camif_priv;
	struct cam_vfe_camif_ver3_pp_clc_reg      *camif_reg;
	int                                        rc = 0;
	uint32_t                                   val = 0;

	if (!camif_res) {
		CAM_ERR(CAM_ISP, "Error, Invalid input arguments");
		return -EINVAL;
	}

	if ((camif_res->res_state == CAM_ISP_RESOURCE_STATE_RESERVED) ||
		(camif_res->res_state == CAM_ISP_RESOURCE_STATE_AVAILABLE))
		return 0;

	camif_priv = (struct cam_vfe_mux_camif_ver3_data *)camif_res->res_priv;
	camif_reg = camif_priv->camif_reg;

	if ((camif_priv->dsp_mode >= CAM_ISP_DSP_MODE_ONE_WAY) &&
		(camif_priv->dsp_mode <= CAM_ISP_DSP_MODE_ROUND)) {
		val = cam_io_r_mb(camif_priv->mem_base +
			camif_priv->common_reg->core_cfg_0);
		val &= (~(1 << camif_priv->reg_data->dsp_en_shift));
		cam_io_w_mb(val, camif_priv->mem_base +
			camif_priv->common_reg->core_cfg_0);
	}

	if (camif_res->res_state == CAM_ISP_RESOURCE_STATE_STREAMING)
		camif_res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;

	val = cam_io_r_mb(camif_priv->mem_base +
		camif_priv->common_reg->diag_config);
	if (val & camif_priv->reg_data->enable_diagnostic_hw) {
		val &= ~camif_priv->reg_data->enable_diagnostic_hw;
		cam_io_w_mb(val, camif_priv->mem_base +
			camif_priv->common_reg->diag_config);
	}

	if (camif_priv->irq_handle) {
		cam_irq_controller_unsubscribe_irq(
			camif_priv->vfe_irq_controller, camif_priv->irq_handle);
		camif_priv->irq_handle = 0;
	}

	if (camif_priv->sof_irq_handle) {
		cam_irq_controller_unsubscribe_irq(
			camif_priv->vfe_irq_controller,
			camif_priv->sof_irq_handle);
		camif_priv->sof_irq_handle = 0;
	}

	if (camif_priv->irq_err_handle) {
		cam_irq_controller_unsubscribe_irq(
			camif_priv->vfe_irq_controller,
			camif_priv->irq_err_handle);
		camif_priv->irq_err_handle = 0;
	}

	return rc;
}

static int cam_vfe_camif_ver3_core_config(
	struct cam_isp_resource_node *rsrc_node, void *cmd_args)
{
	struct cam_vfe_mux_camif_ver3_data *camif_priv;
	struct cam_vfe_core_config_args *vfe_core_cfg =
		(struct cam_vfe_core_config_args *)cmd_args;

	camif_priv =
		(struct cam_vfe_mux_camif_ver3_data *)rsrc_node->res_priv;
	camif_priv->cam_common_cfg.vid_ds16_r2pd =
		vfe_core_cfg->core_config.vid_ds16_r2pd;
	camif_priv->cam_common_cfg.vid_ds4_r2pd =
		vfe_core_cfg->core_config.vid_ds4_r2pd;
	camif_priv->cam_common_cfg.disp_ds16_r2pd =
		vfe_core_cfg->core_config.disp_ds16_r2pd;
	camif_priv->cam_common_cfg.disp_ds4_r2pd =
		vfe_core_cfg->core_config.disp_ds4_r2pd;
	camif_priv->cam_common_cfg.dsp_streaming_tap_point =
		vfe_core_cfg->core_config.dsp_streaming_tap_point;
	camif_priv->cam_common_cfg.ihist_src_sel =
		vfe_core_cfg->core_config.ihist_src_sel;
	camif_priv->cam_common_cfg.hdr_be_src_sel =
		vfe_core_cfg->core_config.hdr_be_src_sel;
	camif_priv->cam_common_cfg.hdr_bhist_src_sel =
		vfe_core_cfg->core_config.hdr_bhist_src_sel;
	camif_priv->cam_common_cfg.input_mux_sel_pdaf =
		vfe_core_cfg->core_config.input_mux_sel_pdaf;
	camif_priv->cam_common_cfg.input_mux_sel_pp =
		vfe_core_cfg->core_config.input_mux_sel_pp;

	return 0;
}

static int cam_vfe_camif_ver3_sof_irq_debug(
	struct cam_isp_resource_node *rsrc_node, void *cmd_args)
{
	struct cam_vfe_mux_camif_ver3_data *camif_priv;
	uint32_t *enable_sof_irq = (uint32_t *)cmd_args;

	camif_priv =
		(struct cam_vfe_mux_camif_ver3_data *)rsrc_node->res_priv;

	if (*enable_sof_irq == 1)
		camif_priv->enable_sof_irq_debug = true;
	else
		camif_priv->enable_sof_irq_debug = false;

	return 0;
}

static int cam_vfe_camif_ver3_process_cmd(
	struct cam_isp_resource_node *rsrc_node,
	uint32_t cmd_type, void *cmd_args, uint32_t arg_size)
{
	int rc = -EINVAL;
	struct cam_vfe_mux_camif_ver3_data *camif_priv = NULL;

	if (!rsrc_node || !cmd_args) {
		CAM_ERR(CAM_ISP,
			"Invalid input arguments rsesource node:%pK cmd_args:%pK",
			rsrc_node, cmd_args);
		return -EINVAL;
	}

	switch (cmd_type) {
	case CAM_ISP_HW_CMD_GET_REG_UPDATE:
		rc = cam_vfe_camif_ver3_get_reg_update(rsrc_node, cmd_args,
			arg_size);
		break;
	case CAM_ISP_HW_CMD_SOF_IRQ_DEBUG:
		rc = cam_vfe_camif_ver3_sof_irq_debug(rsrc_node, cmd_args);
		break;
	case CAM_ISP_HW_CMD_CORE_CONFIG:
		rc = cam_vfe_camif_ver3_core_config(rsrc_node, cmd_args);
		break;
	case CAM_ISP_HW_CMD_SET_CAMIF_DEBUG:
		camif_priv = (struct cam_vfe_mux_camif_ver3_data *)
			rsrc_node->res_priv;
		camif_priv->camif_debug = *((uint32_t *)cmd_args);
		break;
	case CAM_ISP_HW_CMD_QUERY_REGSPACE_DATA:
		camif_priv = (struct cam_vfe_mux_camif_ver3_data *)
			rsrc_node->res_priv;
		*((struct cam_hw_soc_info **)cmd_args) = camif_priv->soc_info;
		rc = 0;
		break;
	default:
		CAM_ERR(CAM_ISP,
			"unsupported process command:%d", cmd_type);
		break;
	}

	return rc;
}


static void cam_vfe_camif_ver3_overflow_debug_info(
	struct cam_vfe_mux_camif_ver3_data *camif_priv)
{
	uint32_t val0, val1, val2, val3;

	val0 = cam_io_r(camif_priv->mem_base +
		camif_priv->common_reg->top_debug_0);
	val1 = cam_io_r(camif_priv->mem_base +
		camif_priv->common_reg->top_debug_1);
	val2 = cam_io_r(camif_priv->mem_base +
		camif_priv->common_reg->top_debug_2);
	val3 = cam_io_r(camif_priv->mem_base +
		camif_priv->common_reg->top_debug_3);
	CAM_INFO(CAM_ISP,
		"status_0: 0x%X status_1: 0x%X status_2: 0x%X status_3: 0x%X",
		val0, val1, val2, val3);

	val0 = cam_io_r(camif_priv->mem_base +
		camif_priv->common_reg->top_debug_4);
	val1 = cam_io_r(camif_priv->mem_base +
		camif_priv->common_reg->top_debug_5);
	val2 = cam_io_r(camif_priv->mem_base +
		camif_priv->common_reg->top_debug_6);
	val3 = cam_io_r(camif_priv->mem_base +
		camif_priv->common_reg->top_debug_7);
	CAM_INFO(CAM_ISP,
		"status_4: 0x%X status_5: 0x%X status_6: 0x%X status_7: 0x%X",
		val0, val1, val2, val3);

	val0 = cam_io_r(camif_priv->mem_base +
		camif_priv->common_reg->top_debug_8);
	val1 = cam_io_r(camif_priv->mem_base +
		camif_priv->common_reg->top_debug_9);
	val2 = cam_io_r(camif_priv->mem_base +
		camif_priv->common_reg->top_debug_10);
	val3 = cam_io_r(camif_priv->mem_base +
		camif_priv->common_reg->top_debug_11);
	CAM_INFO(CAM_ISP,
		"status_8: 0x%X status_9: 0x%X status_10: 0x%X status_11: 0x%X",
		val0, val1, val2, val3);

	val0 = cam_io_r(camif_priv->mem_base +
		camif_priv->common_reg->top_debug_12);
	val1 = cam_io_r(camif_priv->mem_base +
		camif_priv->common_reg->top_debug_13);
	CAM_INFO(CAM_ISP, "status_12: 0x%X status_13: 0x%X",
		val0, val1);
}

static void cam_vfe_camif_ver3_print_status(uint32_t *status,
	int err_type, struct cam_vfe_mux_camif_ver3_data *camif_priv)
{
	uint32_t violation_mask = 0x3F, module_id = 0;
	uint32_t bus_overflow_status = 0, status_0 = 0, status_2 = 0;
	struct cam_vfe_soc_private *soc_private;
	uint32_t val0, val1, val2;

	if (!status) {
		CAM_ERR(CAM_ISP, "Invalid params");
		return;
	}

	bus_overflow_status = status[CAM_IFE_IRQ_BUS_OVERFLOW_STATUS];
	status_0 = status[CAM_IFE_IRQ_CAMIF_REG_STATUS0];
	status_2 = status[CAM_IFE_IRQ_CAMIF_REG_STATUS2];

	if (err_type == CAM_VFE_IRQ_STATUS_OVERFLOW) {
		if (status_0 & 0x0200)
			CAM_INFO(CAM_ISP, "DSP OVERFLOW");

		if (status_0 & 0x2000000)
			CAM_INFO(CAM_ISP, "PIXEL PIPE FRAME DROP");

		if (status_0 & 0x80000000)
			CAM_INFO(CAM_ISP, "PIXEL PIPE OVERFLOW");
	}

	if (err_type == CAM_VFE_IRQ_STATUS_OVERFLOW && bus_overflow_status) {
		if (bus_overflow_status & 0x01)
			CAM_INFO(CAM_ISP, "VID Y 1:1 BUS OVERFLOW");

		if (bus_overflow_status & 0x02)
			CAM_INFO(CAM_ISP, "VID C 1:1 BUS OVERFLOW");

		if (bus_overflow_status & 0x04)
			CAM_INFO(CAM_ISP, "VID YC 4:1 BUS OVERFLOW");

		if (bus_overflow_status & 0x08)
			CAM_INFO(CAM_ISP, "VID YC 16:1 BUS OVERFLOW");

		if (bus_overflow_status & 0x010)
			CAM_INFO(CAM_ISP, "DISP Y 1:1 BUS OVERFLOW");

		if (bus_overflow_status & 0x020)
			CAM_INFO(CAM_ISP, "DISP C 1:1 BUS OVERFLOW");

		if (bus_overflow_status & 0x040)
			CAM_INFO(CAM_ISP, "DISP YC 4:1 BUS OVERFLOW");

		if (bus_overflow_status & 0x080)
			CAM_INFO(CAM_ISP, "DISP YC 16:1 BUS OVERFLOW");

		if (bus_overflow_status & 0x0100)
			CAM_INFO(CAM_ISP, "FD Y BUS OVERFLOW");

		if (bus_overflow_status & 0x0200)
			CAM_INFO(CAM_ISP, "FD C BUS OVERFLOW");

		if (bus_overflow_status & 0x0400)
			CAM_INFO(CAM_ISP, "PIXEL RAW DUMP BUS OVERFLOW");

		if (bus_overflow_status & 0x01000)
			CAM_INFO(CAM_ISP, "STATS HDR BE BUS OVERFLOW");

		if (bus_overflow_status & 0x02000)
			CAM_INFO(CAM_ISP, "STATS HDR BHIST BUS OVERFLOW");

		if (bus_overflow_status & 0x04000)
			CAM_INFO(CAM_ISP, "STATS TINTLESS BG BUS OVERFLOW");

		if (bus_overflow_status & 0x08000)
			CAM_INFO(CAM_ISP, "STATS AWB BG BUS OVERFLOW");

		if (bus_overflow_status & 0x010000)
			CAM_INFO(CAM_ISP, "STATS BHIST BUS OVERFLOW");

		if (bus_overflow_status & 0x020000)
			CAM_INFO(CAM_ISP, "STATS RS BUS OVERFLOW");

		if (bus_overflow_status & 0x040000)
			CAM_INFO(CAM_ISP, "STATS CS BUS OVERFLOW");

		if (bus_overflow_status & 0x080000)
			CAM_INFO(CAM_ISP, "STATS IHIST BUS OVERFLOW");

		if (bus_overflow_status & 0x0100000)
			CAM_INFO(CAM_ISP, "STATS BAF BUS OVERFLOW");

		if (bus_overflow_status & 0x0200000)
			CAM_INFO(CAM_ISP, "PDAF BUS OVERFLOW");

		soc_private = camif_priv->soc_info->soc_private;
		cam_cpas_reg_read(soc_private->cpas_handle,
			CAM_CPAS_REG_CAMNOC, 0xA20, true, &val0);
		cam_cpas_reg_read(soc_private->cpas_handle,
			CAM_CPAS_REG_CAMNOC, 0x1420, true, &val1);
		cam_cpas_reg_read(soc_private->cpas_handle,
			CAM_CPAS_REG_CAMNOC, 0x1A20, true, &val2);
		CAM_INFO(CAM_ISP,
			"CAMNOC REG ife_linear: 0x%X ife_rdi_wr: 0x%X ife_ubwc_stats: 0x%X",
			val0, val1, val2);
		return;
	}

	if (err_type == CAM_VFE_IRQ_STATUS_OVERFLOW && !bus_overflow_status) {
		CAM_INFO(CAM_ISP, "PIXEL PIPE Module hang");
		/* print debug registers */
		cam_vfe_camif_ver3_overflow_debug_info(camif_priv);
		return;
	}

	if (err_type == CAM_VFE_IRQ_STATUS_VIOLATION) {
		if (status_2 & 0x080)
			CAM_INFO(CAM_ISP, "DSP IFE PROTOCOL VIOLATION");

		if (status_2 & 0x0100)
			CAM_INFO(CAM_ISP, "IFE DSP TX PROTOCOL VIOLATION");

		if (status_2 & 0x0200)
			CAM_INFO(CAM_ISP, "DSP IFE RX PROTOCOL VIOLATION");

		if (status_2 & 0x0400)
			CAM_INFO(CAM_ISP, "PP PREPROCESS VIOLATION");

		if (status_2 & 0x0800)
			CAM_INFO(CAM_ISP, "PP CAMIF VIOLATION");

		if (status_2 & 0x01000)
			CAM_INFO(CAM_ISP, "PP VIOLATION");

		if (status_2 & 0x0100000)
			CAM_INFO(CAM_ISP,
				"DSP_TX_VIOLATION:overflow on DSP interface TX path FIFO");

		if (status_2 & 0x0200000)
			CAM_INFO(CAM_ISP,
			"DSP_RX_VIOLATION:overflow on DSP interface RX path FIFO");

		if (status_2 & 0x10000000)
			CAM_INFO(CAM_ISP, "DSP ERROR VIOLATION");

		if (status_2 & 0x20000000)
			CAM_INFO(CAM_ISP,
				"DIAG VIOLATION: HBI is less than the minimum required HBI");
	}

	if (err_type == CAM_VFE_IRQ_STATUS_VIOLATION &&
		status[CAM_IFE_IRQ_VIOLATION_STATUS]) {
		module_id =
			violation_mask & status[CAM_IFE_IRQ_VIOLATION_STATUS];
		CAM_INFO(CAM_ISP, "PIXEL PIPE VIOLATION Module ID:%d",
			module_id);

		switch (module_id) {
		case 0:
			CAM_INFO(CAM_ISP, "DEMUX");
			break;
		case 1:
			CAM_INFO(CAM_ISP, "CHROMA_UP");
			break;
		case 2:
			CAM_INFO(CAM_ISP, "PEDESTAL");
			break;
		case 3:
			CAM_INFO(CAM_ISP, "LINEARIZATION");
			break;
		case 4:
			CAM_INFO(CAM_ISP, "BPC_PDPC");
			break;
		case 5:
			CAM_INFO(CAM_ISP, "HDR_BINCORRECT");
			break;
		case 6:
			CAM_INFO(CAM_ISP, "ABF");
			break;
		case 7:
			CAM_INFO(CAM_ISP, "LSC");
			break;
		case 8:
			CAM_INFO(CAM_ISP, "DEMOSAIC");
			break;
		case 9:
			CAM_INFO(CAM_ISP, "COLOR_CORRECT");
			break;
		case 10:
			CAM_INFO(CAM_ISP, "GTM");
			break;
		case 11:
			CAM_INFO(CAM_ISP, "GLUT");
			break;
		case 12:
			CAM_INFO(CAM_ISP, "COLOR_XFORM");
			break;
		case 13:
			CAM_INFO(CAM_ISP, "CROP_RND_CLAMP_PIXEL_RAW_OUT");
			break;
		case 14:
			CAM_INFO(CAM_ISP, "DOWNSCALE_MN_Y_FD_OUT");
			break;
		case 15:
			CAM_INFO(CAM_ISP, "DOWNSCALE_MN_C_FD_OUT");
			break;
		case 16:
			CAM_INFO(CAM_ISP,
				"CROP_RND_CLAMP_POST_DOWNSCALE_MN_Y_FD_OUT");
			break;
		case 17:
			CAM_INFO(CAM_ISP,
				"CROP_RND_CLAMP_POST_DOWNSCALE_MN_C_FD_OUT");
			break;
		case 18:
			CAM_INFO(CAM_ISP, "DOWNSCALE_MN_Y_DISP_OUT");
			break;
		case 19:
			CAM_INFO(CAM_ISP, "DOWNSCALE_MN_C_DISP_OUT");
			break;
		case 20:
			CAM_INFO(CAM_ISP,
				"module: CROP_RND_CLAMP_POST_DOWNSCALE_MN_Y_DISP_OUT");
			break;
		case 21:
			CAM_INFO(CAM_ISP,
				"CROP_RND_CLAMP_POST_DOWNSCALE_MN_C_DISP_OUT");
			break;
		case 22:
			CAM_INFO(CAM_ISP, "DOWNSCALE_4TO1_Y_DISP_DS4_OUT");
			break;
		case 23:
			CAM_INFO(CAM_ISP, "DOWNSCALE_4TO1_C_DISP_DS4_OUT");
			break;
		case 24:
			CAM_INFO(CAM_ISP,
				"CROP_RND_CLAMP_POST_DOWNSCALE_4TO1_Y_DISP_DS4_OUT");
			break;
		case 25:
			CAM_INFO(CAM_ISP,
				"CROP_RND_CLAMP_POST_DOWNSCALE_4TO1_C_DISP_DS4_OUT");
			break;
		case 26:
			CAM_INFO(CAM_ISP, "DOWNSCALE_4TO1_Y_DISP_DS16_OUT");
			break;
		case 27:
			CAM_INFO(CAM_ISP, "DOWNSCALE_4TO1_C_DISP_DS16_OUT");
			break;
		case 28:
			CAM_INFO(CAM_ISP,
				"CROP_RND_CLAMP_POST_DOWNSCALE_4TO1_Y_DISP_DS16_OUT");
			break;
		case 29:
			CAM_INFO(CAM_ISP,
				"CROP_RND_CLAMP_POST_DOWNSCALE_4TO1_C_DISP_DS16_OUT");
			break;
		case 30:
			CAM_INFO(CAM_ISP, "DOWNSCALE_MN_Y_VID_OUT");
			break;
		case 31:
			CAM_INFO(CAM_ISP, "DOWNSCALE_MN_C_VID_OUT");
			break;
		case 32:
			CAM_INFO(CAM_ISP,
				"CROP_RND_CLAMP_POST_DOWNSCALE_MN_Y_VID_OUT");
			break;
		case 33:
			CAM_INFO(CAM_ISP,
				"CROP_RND_CLAMP_POST_DOWNSCALE_MN_C_VID_OUT");
			break;
		case 34:
			CAM_INFO(CAM_ISP, "DSX_Y_VID_OUT");
			break;
		case 35:
			CAM_INFO(CAM_ISP, "DSX_C_VID_OUT");
			break;
		case 36:
			CAM_INFO(CAM_ISP,
				"CROP_RND_CLAMP_POST_DSX_Y_VID_OUT");
			break;
		case 37:
			CAM_INFO(CAM_ISP,
				"CROP_RND_CLAMP_POST_DSX_C_VID_OUT");
			break;
		case 38:
			CAM_INFO(CAM_ISP,
				"DOWNSCALE_4TO1_Y_VID_DS16_OUT");
			break;
		case 39:
			CAM_INFO(CAM_ISP,
				"DOWNSCALE_4TO1_C_VID_DS16_OUT");
			break;
		case 40:
			CAM_INFO(CAM_ISP,
				"CROP_RND_CLAMP_POST_DOWNSCALE_4TO1_Y_VID_DS16_OUT");
			break;
		case 41:
			CAM_INFO(CAM_ISP,
				"CROP_RND_CLAMP_POST_DOWNSCALE_4TO1_C_VID_DS16_OUT");
			break;
		case 42:
			CAM_INFO(CAM_ISP, "BLS");
			break;
		case 43:
			CAM_INFO(CAM_ISP, "STATS_TINTLESS_BG");
			break;
		case 44:
			CAM_INFO(CAM_ISP, "STATS_HDR_BHIST");
			break;
		case 45:
			CAM_INFO(CAM_ISP, "STATS_HDR_BE");
			break;
		case 46:
			CAM_INFO(CAM_ISP, "STATS_AWB_BG");
			break;
		case 47:
			CAM_INFO(CAM_ISP, "STATS_BHIST");
			break;
		case 48:
			CAM_INFO(CAM_ISP, "STATS_BAF");
			break;
		case 49:
			CAM_INFO(CAM_ISP, "STATS_RS");
			break;
		case 50:
			CAM_INFO(CAM_ISP, "STATS_CS");
			break;
		case 51:
			CAM_INFO(CAM_ISP, "STATS_IHIST");
			break;
		default:
			CAM_ERR(CAM_ISP,
				"Invalid Module ID:%d", module_id);
			break;
		}
	}
}

static int cam_vfe_camif_ver3_handle_irq_top_half(uint32_t evt_id,
	struct cam_irq_th_payload *th_payload)
{
	int32_t                                rc;
	int                                    i;
	struct cam_isp_resource_node          *camif_node;
	struct cam_vfe_mux_camif_ver3_data    *camif_priv;
	struct cam_vfe_top_irq_evt_payload    *evt_payload;

	camif_node = th_payload->handler_priv;
	camif_priv = camif_node->res_priv;

	CAM_DBG(CAM_ISP,
		"VFE:%d CAMIF IRQ status_0: 0x%X status_1: 0x%X status_2: 0x%X",
		camif_node->hw_intf->hw_idx, th_payload->evt_status_arr[0],
		th_payload->evt_status_arr[1], th_payload->evt_status_arr[2]);

	rc  = cam_vfe_camif_ver3_get_evt_payload(camif_priv, &evt_payload);
	if (rc) {
		CAM_INFO_RATE_LIMIT(CAM_ISP,
		"VFE:%d CAMIF IRQ status_0: 0x%X status_1: 0x%X status_2: 0x%X",
		camif_node->hw_intf->hw_idx, th_payload->evt_status_arr[0],
		th_payload->evt_status_arr[1], th_payload->evt_status_arr[2]);
		return rc;
	}

	cam_isp_hw_get_timestamp(&evt_payload->ts);

	for (i = 0; i < th_payload->num_registers; i++)
		evt_payload->irq_reg_val[i] = th_payload->evt_status_arr[i];

	th_payload->evt_payload_priv = evt_payload;

	CAM_DBG(CAM_ISP, "Exit");
	return rc;
}

static int cam_vfe_camif_ver3_handle_irq_bottom_half(void *handler_priv,
	void *evt_payload_priv)
{
	int ret = CAM_VFE_IRQ_STATUS_ERR;
	struct cam_isp_resource_node *camif_node;
	struct cam_vfe_mux_camif_ver3_data *camif_priv;
	struct cam_vfe_top_irq_evt_payload *payload;
	struct cam_isp_hw_event_info evt_info;
	uint32_t irq_status[CAM_IFE_IRQ_REGISTERS_MAX] = {0};
	int i = 0;

	if (!handler_priv || !evt_payload_priv) {
		CAM_ERR(CAM_ISP,
			"Invalid params handle_priv:%pK, evt_payload_priv:%pK",
			handler_priv, evt_payload_priv);
		return ret;
	}

	camif_node = handler_priv;
	camif_priv = camif_node->res_priv;
	payload = evt_payload_priv;

	for (i = 0; i < CAM_IFE_IRQ_REGISTERS_MAX; i++)
		irq_status[i] = payload->irq_reg_val[i];

	evt_info.hw_idx   = camif_node->hw_intf->hw_idx;
	evt_info.res_id   = camif_node->res_id;
	evt_info.res_type = camif_node->res_type;

	if (irq_status[CAM_IFE_IRQ_CAMIF_REG_STATUS1]
		& camif_priv->reg_data->sof_irq_mask) {
		if ((camif_priv->enable_sof_irq_debug) &&
			(camif_priv->irq_debug_cnt <=
			CAM_VFE_CAMIF_IRQ_SOF_DEBUG_CNT_MAX)) {
			CAM_INFO_RATE_LIMIT(CAM_ISP, "VFE:%d Received SOF",
				evt_info.hw_idx);

			camif_priv->irq_debug_cnt++;
			if (camif_priv->irq_debug_cnt ==
				CAM_VFE_CAMIF_IRQ_SOF_DEBUG_CNT_MAX) {
				camif_priv->enable_sof_irq_debug =
					false;
				camif_priv->irq_debug_cnt = 0;
			}
		} else
			CAM_DBG(CAM_ISP, "VFE:%d Received SOF",
				evt_info.hw_idx);

		if (camif_priv->event_cb)
			camif_priv->event_cb(camif_priv->priv,
				CAM_ISP_HW_EVENT_SOF, (void *)&evt_info);

		ret = CAM_VFE_IRQ_STATUS_SUCCESS;
	}

	if (irq_status[CAM_IFE_IRQ_CAMIF_REG_STATUS1]
		& camif_priv->reg_data->epoch0_irq_mask) {
		CAM_DBG(CAM_ISP, "VFE:%d Received EPOCH", evt_info.hw_idx);

		if (camif_priv->event_cb)
			camif_priv->event_cb(camif_priv->priv,
				CAM_ISP_HW_EVENT_EPOCH, (void *)&evt_info);

		ret = CAM_VFE_IRQ_STATUS_SUCCESS;
	}

	if (irq_status[CAM_IFE_IRQ_CAMIF_REG_STATUS1]
		& camif_priv->reg_data->eof_irq_mask) {
		CAM_DBG(CAM_ISP, "VFE:%d Received EOF", evt_info.hw_idx);

		if (camif_priv->event_cb)
			camif_priv->event_cb(camif_priv->priv,
				CAM_ISP_HW_EVENT_EOF, (void *)&evt_info);

		ret = CAM_VFE_IRQ_STATUS_SUCCESS;
	}

	if (irq_status[CAM_IFE_IRQ_CAMIF_REG_STATUS0]
		& camif_priv->reg_data->error_irq_mask0) {
		CAM_ERR(CAM_ISP, "VFE:%d Overflow", evt_info.hw_idx);

		if (camif_priv->event_cb)
			camif_priv->event_cb(camif_priv->priv,
				CAM_ISP_HW_EVENT_ERROR, (void *)&evt_info);

		ret = CAM_VFE_IRQ_STATUS_OVERFLOW;

		cam_vfe_camif_ver3_print_status(irq_status, ret, camif_priv);

		if (camif_priv->camif_debug & CAMIF_DEBUG_ENABLE_REG_DUMP)
			cam_vfe_camif_ver3_reg_dump(camif_node);
	}

	if (irq_status[CAM_IFE_IRQ_CAMIF_REG_STATUS2]) {
		CAM_ERR(CAM_ISP, "VFE:%d Violation", evt_info.hw_idx);

		if (camif_priv->event_cb)
			camif_priv->event_cb(camif_priv->priv,
				CAM_ISP_HW_EVENT_ERROR, (void *)&evt_info);

		ret = CAM_VFE_IRQ_STATUS_VIOLATION;

		cam_vfe_camif_ver3_print_status(irq_status, ret, camif_priv);

		if (camif_priv->camif_debug & CAMIF_DEBUG_ENABLE_REG_DUMP)
			cam_vfe_camif_ver3_reg_dump(camif_node);
	}

	if (camif_priv->camif_debug & CAMIF_DEBUG_ENABLE_SENSOR_DIAG_STATUS) {
		CAM_DBG(CAM_ISP, "VFE:%d VFE_DIAG_SENSOR_STATUS: 0x%X",
			evt_info.hw_idx, camif_priv->mem_base,
			cam_io_r(camif_priv->mem_base +
			camif_priv->common_reg->diag_sensor_status_0));
	}

	cam_vfe_camif_ver3_put_evt_payload(camif_priv, &payload);

	CAM_DBG(CAM_ISP, "returning status = %d", ret);
	return ret;
}

int cam_vfe_camif_ver3_init(
	struct cam_hw_intf            *hw_intf,
	struct cam_hw_soc_info        *soc_info,
	void                          *camif_hw_info,
	struct cam_isp_resource_node  *camif_node,
	void                          *vfe_irq_controller)
{
	struct cam_vfe_mux_camif_ver3_data *camif_priv = NULL;
	struct cam_vfe_camif_ver3_hw_info *camif_info = camif_hw_info;
	int i = 0;

	camif_priv = kzalloc(sizeof(struct cam_vfe_mux_camif_ver3_data),
		GFP_KERNEL);
	if (!camif_priv)
		return -ENOMEM;

	camif_node->res_priv = camif_priv;

	camif_priv->mem_base    = soc_info->reg_map[VFE_CORE_BASE_IDX].mem_base;
	camif_priv->camif_reg   = camif_info->camif_reg;
	camif_priv->common_reg  = camif_info->common_reg;
	camif_priv->reg_data    = camif_info->reg_data;
	camif_priv->hw_intf     = hw_intf;
	camif_priv->soc_info    = soc_info;
	camif_priv->vfe_irq_controller = vfe_irq_controller;

	camif_node->init    = cam_vfe_camif_ver3_resource_init;
	camif_node->deinit  = cam_vfe_camif_ver3_resource_deinit;
	camif_node->start   = cam_vfe_camif_ver3_resource_start;
	camif_node->stop    = cam_vfe_camif_ver3_resource_stop;
	camif_node->process_cmd = cam_vfe_camif_ver3_process_cmd;
	camif_node->top_half_handler = cam_vfe_camif_ver3_handle_irq_top_half;
	camif_node->bottom_half_handler =
		cam_vfe_camif_ver3_handle_irq_bottom_half;
	spin_lock_init(&camif_priv->spin_lock);
	INIT_LIST_HEAD(&camif_priv->free_payload_list);
	for (i = 0; i < CAM_VFE_CAMIF_EVT_MAX; i++) {
		INIT_LIST_HEAD(&camif_priv->evt_payload[i].list);
		list_add_tail(&camif_priv->evt_payload[i].list,
			&camif_priv->free_payload_list);
	}

	return 0;
}

int cam_vfe_camif_ver3_deinit(
	struct cam_isp_resource_node  *camif_node)
{
	struct cam_vfe_mux_camif_ver3_data *camif_priv;
	int i = 0;

	if (!camif_node) {
		CAM_ERR(CAM_ISP, "Error, camif_node is NULL %pK", camif_node);
		return -ENODEV;
	}

	camif_priv = camif_node->res_priv;

	INIT_LIST_HEAD(&camif_priv->free_payload_list);
	for (i = 0; i < CAM_VFE_CAMIF_EVT_MAX; i++)
		INIT_LIST_HEAD(&camif_priv->evt_payload[i].list);

	camif_priv = camif_node->res_priv;

	camif_node->start = NULL;
	camif_node->stop  = NULL;
	camif_node->process_cmd = NULL;
	camif_node->top_half_handler = NULL;
	camif_node->bottom_half_handler = NULL;
	camif_node->res_priv = NULL;

	if (!camif_priv) {
		CAM_ERR(CAM_ISP, "Error, camif_priv is NULL %pK", camif_priv);
		return -ENODEV;
	}

	kfree(camif_priv);

	return 0;
}
