// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/iopoll.h>
#include <linux/slab.h>
#include <media/cam_tfe.h>
#include <media/cam_defs.h>

#include "cam_top_tpg_core.h"
#include "cam_soc_util.h"
#include "cam_io_util.h"
#include "cam_debug_util.h"
#include "cam_top_tpg_ver3.h"

static int cam_top_tpg_ver3_get_hw_caps(
	void                                         *hw_priv,
	void                                         *get_hw_cap_args,
	uint32_t                                      arg_size)
{
	int                                           rc = 0;
	struct cam_top_tpg_hw_caps                   *hw_caps;
	struct cam_top_tpg_hw                        *tpg_hw;
	const struct cam_top_tpg_ver3_reg_offset     *tpg_reg;
	struct cam_hw_info                           *tpg_hw_info;

	if (!hw_priv || !get_hw_cap_args) {
		CAM_ERR(CAM_ISP, "TPG: Invalid args");
		return -EINVAL;
	}

	tpg_hw_info = (struct cam_hw_info  *)hw_priv;
	tpg_hw = (struct cam_top_tpg_hw   *)tpg_hw_info->core_info;
	hw_caps = (struct cam_top_tpg_hw_caps *) get_hw_cap_args;
	tpg_reg = tpg_hw->tpg_info->tpg_reg;

	hw_caps->major_version = tpg_reg->major_version;
	hw_caps->minor_version = tpg_reg->minor_version;
	hw_caps->version_incr = tpg_reg->version_incr;

	CAM_DBG(CAM_ISP,
		"TPG:%d major:%d minor:%d ver :%d",
		tpg_hw->hw_intf->hw_idx, hw_caps->major_version,
		hw_caps->minor_version, hw_caps->version_incr);

	return rc;
}

static int cam_top_tpg_ver3_process_cmd(void *hw_priv,
	uint32_t cmd_type, void *cmd_args, uint32_t arg_size)
{
	int                                     rc = 0;
	struct cam_top_tpg_hw                  *tpg_hw;
	struct cam_hw_info                     *tpg_hw_info;
	struct cam_isp_tpg_core_config         *core_cfg;
	struct cam_top_tpg_cfg                 *tpg_data;

	if (!hw_priv || !cmd_args) {
		CAM_ERR(CAM_ISP, "TPG: Invalid args");
		return -EINVAL;
	}

	tpg_hw_info = (struct cam_hw_info *)hw_priv;
	tpg_hw = (struct cam_top_tpg_hw *)tpg_hw_info->core_info;
	tpg_data = (struct cam_top_tpg_cfg *)tpg_hw->tpg_res.res_priv;

	switch (cmd_type) {
	case CAM_ISP_HW_CMD_TPG_CORE_CFG_CMD:
		if (arg_size != sizeof(struct cam_isp_tpg_core_config)) {
			CAM_ERR(CAM_ISP, "Invalid size %u expected %u",
				arg_size,
				sizeof(struct cam_isp_tpg_core_config));
			rc = -EINVAL;
			break;
		}

		core_cfg = (struct cam_isp_tpg_core_config *)cmd_args;
		tpg_data->pix_pattern = core_cfg->pix_pattern;
		tpg_data->vc_dt_pattern_id = core_cfg->vc_dt_pattern_id;
		tpg_data->qcfa_en = core_cfg->qcfa_en;
		CAM_DBG(CAM_ISP,
			"pattern_id: 0x%x pix_pattern: 0x%x qcfa_en: 0x%x",
			tpg_data->vc_dt_pattern_id, tpg_data->pix_pattern,
			tpg_data->qcfa_en);
		break;
	default:
		CAM_ERR(CAM_ISP, "Invalid TPG cmd type %u", cmd_type);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int cam_top_tpg_ver3_reserve(
	void                                         *hw_priv,
	void                                         *reserve_args,
	uint32_t                                      arg_size)
{
	int                                           rc = 0;
	struct cam_top_tpg_hw                        *tpg_hw;
	struct cam_hw_info                           *tpg_hw_info;
	struct cam_top_tpg_ver3_reserve_args         *reserv;
	struct cam_top_tpg_cfg                       *tpg_data;
	uint32_t                                      encode_format = 0;
	uint32_t                                      i;

	if (!hw_priv || !reserve_args || (arg_size !=
		sizeof(struct cam_top_tpg_ver3_reserve_args))) {
		CAM_ERR(CAM_ISP, "TPG: Invalid args");
		return -EINVAL;
	}

	tpg_hw_info = (struct cam_hw_info *)hw_priv;
	tpg_hw = (struct cam_top_tpg_hw *)tpg_hw_info->core_info;
	reserv = (struct cam_top_tpg_ver3_reserve_args  *)reserve_args;

	if (reserv->num_inport <= 0 ||
		reserv->num_inport > CAM_TOP_TPG_MAX_SUPPORTED_DT) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "TPG: %u invalid input num port:%d",
			tpg_hw->hw_intf->hw_idx, reserv->num_inport);
		return -EINVAL;
	}

	mutex_lock(&tpg_hw->hw_info->hw_mutex);
	if (tpg_hw->tpg_res.res_state != CAM_ISP_RESOURCE_STATE_AVAILABLE) {
		rc = -EINVAL;
		goto error;
	}

	if ((reserv->in_port->lane_num <= 0 ||
		reserv->in_port->lane_num > 4) ||
		(reserv->in_port->lane_type >= 2)) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "TPG:%u invalid input %d %d",
			tpg_hw->hw_intf->hw_idx,
			reserv->in_port->lane_num,
			reserv->in_port->lane_type);
		rc = -EINVAL;
		goto error;
	}

	tpg_data = (struct cam_top_tpg_cfg *)tpg_hw->tpg_res.res_priv;
	memset(tpg_data, 0, sizeof(*tpg_data));
	for (i = 0; i < reserv->in_port->num_valid_vc_dt; i++) {
		if (reserv->in_port->dt[i] > 0x3f ||
			reserv->in_port->vc[i] > 0x1f) {
			CAM_ERR(CAM_ISP, "TPG:%u Invalid vc:%d dt %d",
				tpg_hw->hw_intf->hw_idx,
				reserv->in_port->vc[i],
				reserv->in_port->dt[i]);
			rc = -EINVAL;
			goto error;
		}
		tpg_data->vc_num[i] = reserv->in_port->vc[i];
		tpg_data->dt_cfg[i].data_type = reserv->in_port->dt[i];
	}

	rc = cam_top_tpg_get_format(reserv->in_port->format,
		&encode_format);
	if (rc)
		goto error;


	CAM_DBG(CAM_ISP, "TPG: %u enter", tpg_hw->hw_intf->hw_idx);

	tpg_data = (struct cam_top_tpg_cfg *)tpg_hw->tpg_res.res_priv;
	tpg_data->phy_sel = reserv->in_port->lane_type;
	tpg_data->num_active_lanes = reserv->in_port->lane_num;
	tpg_data->h_blank_count = reserv->in_port->hbi_cnt;
	tpg_data->v_blank_count = 600;
	tpg_data->num_active_dts = reserv->in_port->num_valid_vc_dt;

	for (i = 0; i < reserv->in_port->num_valid_vc_dt; i++) {
		tpg_data->dt_cfg[i].encode_format = encode_format;
		tpg_data->dt_cfg[i].frame_height = reserv->in_port->height;

		if (reserv->in_port->usage_type)
			tpg_data->dt_cfg[i].frame_width =
				((reserv->in_port->right_stop -
					reserv->in_port->left_start) + 1);
		else
			tpg_data->dt_cfg[i].frame_width =
				reserv->in_port->left_width;
	}

	CAM_DBG(CAM_ISP,
		"TPG:%u vc_num:%d dt:%d phy:%d lines:%d pattern:%d format:%d",
		tpg_hw->hw_intf->hw_idx,
		tpg_data->vc_num, tpg_data->dt_cfg[0].data_type,
		tpg_data->phy_sel, tpg_data->num_active_lanes,
		tpg_data->pix_pattern,
		tpg_data->dt_cfg[0].encode_format);

	CAM_DBG(CAM_ISP, "TPG:%u height:%d width:%d h blank:%d v blank:%d",
		tpg_hw->hw_intf->hw_idx,
		tpg_data->dt_cfg[0].frame_height,
		tpg_data->dt_cfg[0].frame_width,
		tpg_data->h_blank_count,
		tpg_data->v_blank_count);

	reserv->node_res = &tpg_hw->tpg_res;
	tpg_hw->tpg_res.res_state = CAM_ISP_RESOURCE_STATE_RESERVED;
error:
	mutex_unlock(&tpg_hw->hw_info->hw_mutex);
	CAM_DBG(CAM_ISP, "exit rc %u", rc);

	return rc;
}

static int cam_top_tpg_ver3_start(
	void                                         *hw_priv,
	void                                         *start_args,
	uint32_t                                      arg_size)
{
	int                                           rc = 0;
	struct cam_top_tpg_hw                        *tpg_hw;
	struct cam_hw_info                           *tpg_hw_info;
	struct cam_hw_soc_info                       *soc_info;
	struct cam_isp_resource_node                 *tpg_res;
	struct cam_top_tpg_ver3_reg_offset           *tpg_reg;
	struct cam_top_tpg_cfg                       *tpg_data;
	uint32_t                                      i, val;

	if (!hw_priv || !start_args ||
		(arg_size != sizeof(struct cam_isp_resource_node))) {
		CAM_ERR(CAM_ISP, "TPG: Invalid args");
		return -EINVAL;
	}

	tpg_hw_info = (struct cam_hw_info  *)hw_priv;
	tpg_hw = (struct cam_top_tpg_hw   *)tpg_hw_info->core_info;
	tpg_reg = tpg_hw->tpg_info->tpg_reg;
	tpg_res = (struct cam_isp_resource_node *)start_args;
	tpg_data = (struct cam_top_tpg_cfg  *)tpg_res->res_priv;
	soc_info = &tpg_hw->hw_info->soc_info;

	if ((tpg_res->res_type != CAM_ISP_RESOURCE_TPG) ||
		(tpg_res->res_state != CAM_ISP_RESOURCE_STATE_RESERVED)) {
		CAM_ERR(CAM_ISP, "TPG:%d Invalid Res type:%d res_state:%d",
			tpg_hw->hw_intf->hw_idx,
			tpg_res->res_type, tpg_res->res_state);
		rc = -EINVAL;
		goto end;
	}

	cam_io_w_mb(1, soc_info->reg_map[0].mem_base + tpg_reg->tpg_top_clear);

	for (i = 0; i < tpg_data->num_active_dts; i++) {
		val = (((tpg_data->dt_cfg[i].frame_width & 0xFFFF) << 16) |
			(tpg_data->dt_cfg[i].frame_height & 0xFFFF));
		cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			tpg_reg->tpg_vc0_dt_0_cfg_0 + 0x60 * i);

		cam_io_w_mb(tpg_data->dt_cfg[i].data_type,
			soc_info->reg_map[0].mem_base +
			tpg_reg->tpg_vc0_dt_0_cfg_1 + 0x60 * i);
		val = ((tpg_data->dt_cfg[i].encode_format & 0xF) <<
			tpg_reg->tpg_dt_encode_format_shift) |
			tpg_reg->tpg_payload_mode_color;

		cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			tpg_reg->tpg_vc0_dt_0_cfg_2 + 0x60 * i);

		val = (1 << tpg_reg->tpg_split_en_shift);
		val |= tpg_data->pix_pattern;
		if (tpg_data->qcfa_en)
			val |= (1 << tpg_reg->tpg_color_bar_qcfa_en_shift);
		cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			tpg_reg->tpg_vc0_color_bar_cfg + 0x60 * i);

		/*
		 * if hblank is notset configureHBlank count 500 and
		 * V blank count is 600
		 */

		if (tpg_data->h_blank_count)
			cam_io_w_mb(tpg_data->h_blank_count,
				soc_info->reg_map[0].mem_base +
				tpg_reg->tpg_vc0_hbi_cfg + 0x60 * i);
		else
			cam_io_w_mb(0x1F4,
				soc_info->reg_map[0].mem_base +
				tpg_reg->tpg_vc0_hbi_cfg + 0x60 * i);

		if (tpg_data->v_blank_count)
			cam_io_w_mb(tpg_data->v_blank_count,
				soc_info->reg_map[0].mem_base +
				tpg_reg->tpg_vc0_vbi_cfg + 0x60 * i);
		else
			cam_io_w_mb(0x258,
				soc_info->reg_map[0].mem_base +
				tpg_reg->tpg_vc0_vbi_cfg + 0x60 * i);

		cam_io_w_mb(0x12345678, soc_info->reg_map[0].mem_base +
			tpg_reg->tpg_vc0_lfsr_seed + 0x60 * i);

		val = (((tpg_data->num_active_dts-1) <<
			 tpg_reg->tpg_num_dts_shift_val) | tpg_data->vc_num[i]);
		cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			tpg_reg->tpg_vc0_cfg0 + 0x60 * i);
	}

	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		tpg_reg->tpg_top_irq_mask);

	val = ((tpg_data->num_active_dts - 1) <<
		(tpg_reg->tpg_num_active_vcs_shift) |
		(tpg_data->num_active_lanes - 1) <<
		tpg_reg->tpg_num_active_lanes_shift) |
		(tpg_data->vc_dt_pattern_id) <<
		(tpg_reg->tpg_vc_dt_pattern_id_shift) |
		(tpg_data->phy_sel << tpg_reg->tpg_cphy_dphy_sel_shift_val) |
		(1 << tpg_reg->tpg_en_shift_val);
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base + tpg_reg->tpg_ctrl);


	tpg_res->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;

	val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
		tpg_reg->tpg_hw_version);
	CAM_DBG(CAM_ISP, "TPG:%d TPG HW version: 0x%x started",
		tpg_hw->hw_intf->hw_idx, val);

end:
	return rc;
}

static int cam_top_tpg_ver3_stop(
	void                                         *hw_priv,
	void                                         *stop_args,
	uint32_t                                      arg_size)
{
	int                                           rc = 0;
	struct cam_top_tpg_hw                        *tpg_hw;
	struct cam_hw_info                           *tpg_hw_info;
	struct cam_hw_soc_info                       *soc_info;
	struct cam_isp_resource_node                 *tpg_res;
	const struct cam_top_tpg_ver3_reg_offset     *tpg_reg;
	struct cam_top_tpg_cfg                       *tpg_data;

	if (!hw_priv || !stop_args ||
		(arg_size != sizeof(struct cam_isp_resource_node))) {
		CAM_ERR(CAM_ISP, "TPG: Invalid args");
		return -EINVAL;
	}

	tpg_hw_info = (struct cam_hw_info  *)hw_priv;
	tpg_hw = (struct cam_top_tpg_hw   *)tpg_hw_info->core_info;
	tpg_reg = tpg_hw->tpg_info->tpg_reg;
	tpg_res = (struct cam_isp_resource_node  *) stop_args;
	tpg_data = (struct cam_top_tpg_cfg  *)tpg_res->res_state;
	soc_info = &tpg_hw->hw_info->soc_info;

	if ((tpg_res->res_type != CAM_ISP_RESOURCE_TPG) ||
		(tpg_res->res_state != CAM_ISP_RESOURCE_STATE_STREAMING)) {
		CAM_DBG(CAM_ISP, "TPG:%d Invalid Res type:%d res_state:%d",
			tpg_hw->hw_intf->hw_idx,
			tpg_res->res_type, tpg_res->res_state);
		rc = -EINVAL;
		goto end;
	}

	cam_io_w_mb(0, soc_info->reg_map[0].mem_base + tpg_reg->tpg_ctrl);

	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		tpg_reg->tpg_top_irq_mask);

	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		tpg_reg->tpg_top_irq_clear);

	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
		tpg_reg->tpg_top_irq_cmd);

	cam_io_w_mb(1, soc_info->reg_map[0].mem_base + tpg_reg->tpg_top_clear);

	tpg_res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;

	CAM_DBG(CAM_ISP, "TPG:%d stopped", tpg_hw->hw_intf->hw_idx);
end:
	return rc;
}

int cam_top_tpg_ver3_init(
	struct cam_top_tpg_hw                        *tpg_hw)
{
	tpg_hw->hw_intf->hw_ops.get_hw_caps = cam_top_tpg_ver3_get_hw_caps;
	tpg_hw->hw_intf->hw_ops.reserve     = cam_top_tpg_ver3_reserve;
	tpg_hw->hw_intf->hw_ops.start       = cam_top_tpg_ver3_start;
	tpg_hw->hw_intf->hw_ops.stop        = cam_top_tpg_ver3_stop;
	tpg_hw->hw_intf->hw_ops.process_cmd = cam_top_tpg_ver3_process_cmd;
	return 0;
}
