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
#include "cam_top_tpg_ver1.h"

static uint32_t tpg_num_dt_map[CAM_TOP_TPG_MAX_SUPPORTED_DT] = {
	0,
	3,
	1,
	2
};

static int cam_top_tpg_ver1_get_hw_caps(
	void                                         *hw_priv,
	void                                         *get_hw_cap_args,
	uint32_t                                      arg_size)
{
	int                                           rc = 0;
	struct cam_top_tpg_hw_caps                   *hw_caps;
	struct cam_top_tpg_hw                        *tpg_hw;
	const struct cam_top_tpg_ver1_reg_offset     *tpg_reg;
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

static int cam_top_tpg_ver1_reserve(
	void                                         *hw_priv,
	void                                         *reserve_args,
	uint32_t                                      arg_size)
{
	int                                           rc = 0;
	struct cam_top_tpg_hw                        *tpg_hw;
	struct cam_hw_info                           *tpg_hw_info;
	struct cam_top_tpg_ver1_reserve_args         *reserv;
	struct cam_top_tpg_cfg                       *tpg_data;
	uint32_t                                      encode_format = 0;
	uint32_t                                      i;

	if (!hw_priv || !reserve_args || (arg_size !=
		sizeof(struct cam_top_tpg_ver1_reserve_args))) {
		CAM_ERR(CAM_ISP, "TPG: Invalid args");
		return -EINVAL;
	}

	tpg_hw_info = (struct cam_hw_info *)hw_priv;
	tpg_hw = (struct cam_top_tpg_hw *)tpg_hw_info->core_info;
	reserv = (struct cam_top_tpg_ver1_reserve_args  *)reserve_args;

	if (reserv->num_inport <= 0 ||
		reserv->num_inport > CAM_TOP_TPG_MAX_SUPPORTED_DT) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "TPG: %u invalid input num port:%d",
			tpg_hw->hw_intf->hw_idx, reserv->num_inport);
		return -EINVAL;
	}

	mutex_lock(&tpg_hw->hw_info->hw_mutex);
	if (tpg_hw->tpg_res.res_state != CAM_ISP_RESOURCE_STATE_AVAILABLE) {
		mutex_unlock(&tpg_hw->hw_info->hw_mutex);
		return -EINVAL;
	}

	if ((reserv->in_port[0]->vc > 0xF) ||
		(reserv->in_port[0]->lane_num <= 0 ||
		reserv->in_port[0]->lane_num > 4) ||
		(reserv->in_port[0]->pix_pattern > 4) ||
		(reserv->in_port[0]->lane_type >= 2)) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "TPG:%u invalid input %d %d %d %d",
			tpg_hw->hw_intf->hw_idx, reserv->in_port[0]->vc,
			reserv->in_port[0]->lane_num,
			reserv->in_port[0]->pix_pattern,
			reserv->in_port[0]->lane_type);
		mutex_unlock(&tpg_hw->hw_info->hw_mutex);
		return -EINVAL;
	}
	rc = cam_top_tpg_get_format(reserv->in_port[0]->format,
		&encode_format);
	if (rc)
		goto error;

	CAM_DBG(CAM_ISP, "TPG: %u enter", tpg_hw->hw_intf->hw_idx);

	tpg_data = (struct cam_top_tpg_cfg *)tpg_hw->tpg_res.res_priv;
	tpg_data->vc_num[0] = reserv->in_port[0]->vc;
	tpg_data->phy_sel = reserv->in_port[0]->lane_type;
	tpg_data->num_active_lanes = reserv->in_port[0]->lane_num;
	tpg_data->h_blank_count = reserv->in_port[0]->sensor_hbi;
	tpg_data->v_blank_count = reserv->in_port[0]->sensor_vbi;
	tpg_data->pix_pattern = reserv->in_port[0]->pix_pattern;
	tpg_data->dt_cfg[0].data_type = reserv->in_port[0]->dt;
	tpg_data->dt_cfg[0].frame_height = reserv->in_port[0]->height;
	if (reserv->in_port[0]->usage_type)
		tpg_data->dt_cfg[0].frame_width =
			((reserv->in_port[0]->right_end -
				reserv->in_port[0]->left_start) + 1);
	else
		tpg_data->dt_cfg[0].frame_width =
			reserv->in_port[0]->left_width;
	tpg_data->dt_cfg[0].encode_format = encode_format;
	tpg_data->num_active_dts = 1;

	CAM_DBG(CAM_ISP,
		"TPG:%u vc_num:%d dt:%d phy:%d lines:%d pattern:%d format:%d",
		tpg_hw->hw_intf->hw_idx,
		tpg_data->vc_num[0], tpg_data->dt_cfg[0].data_type,
		tpg_data->phy_sel, tpg_data->num_active_lanes,
		tpg_data->pix_pattern,
		tpg_data->dt_cfg[0].encode_format);

	CAM_DBG(CAM_ISP, "TPG:%u height:%d width:%d h blank:%d v blank:%d",
		tpg_hw->hw_intf->hw_idx,
		tpg_data->dt_cfg[0].frame_height,
		tpg_data->dt_cfg[0].frame_width,
		tpg_data->h_blank_count,
		tpg_data->v_blank_count);

	if (reserv->num_inport == 1)
		goto end;

	for (i = 1; i < reserv->num_inport; i++) {
		if ((tpg_data->vc_num[0] != reserv->in_port[i]->vc) ||
			(tpg_data->phy_sel != reserv->in_port[i]->lane_type) ||
			(tpg_data->num_active_lanes !=
				reserv->in_port[i]->lane_num) ||
			(tpg_data->pix_pattern !=
			reserv->in_port[i]->pix_pattern)) {
			CAM_ERR_RATE_LIMIT(CAM_ISP,
				"TPG: %u invalid DT config for tpg",
				tpg_hw->hw_intf->hw_idx);
			rc = -EINVAL;
			goto error;
		}
		rc = cam_top_tpg_get_format(reserv->in_port[0]->format,
			&encode_format);
		if (rc)
			return rc;

		tpg_data->dt_cfg[i].data_type = reserv->in_port[i]->dt;
		tpg_data->dt_cfg[i].frame_height =
			reserv->in_port[i]->height;
		tpg_data->dt_cfg[i].frame_width =
			reserv->in_port[i]->left_width;
		tpg_data->dt_cfg[i].encode_format = encode_format;
		tpg_data->num_active_dts++;

		CAM_DBG(CAM_ISP, "TPG:%u height:%d width:%d dt:%d format:%d",
			tpg_hw->hw_intf->hw_idx,
			tpg_data->dt_cfg[i].frame_height,
			tpg_data->dt_cfg[i].frame_width,
			tpg_data->dt_cfg[i].data_type,
			tpg_data->dt_cfg[i].encode_format);

	}
end:
	reserv->node_res = &tpg_hw->tpg_res;
	tpg_hw->tpg_res.res_state = CAM_ISP_RESOURCE_STATE_RESERVED;
error:
	mutex_unlock(&tpg_hw->hw_info->hw_mutex);
	CAM_DBG(CAM_ISP, "exit rc %u", rc);

	return rc;
}

static int cam_top_tpg_ver1_start(
	void                                         *hw_priv,
	void                                         *start_args,
	uint32_t                                      arg_size)
{
	int                                           rc = 0;
	struct cam_top_tpg_hw                        *tpg_hw;
	struct cam_hw_info                           *tpg_hw_info;
	struct cam_hw_soc_info                       *soc_info;
	struct cam_isp_resource_node                 *tpg_res;
	struct cam_top_tpg_ver1_reg_offset           *tpg_reg;
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
	cam_io_w_mb(0x12345678, soc_info->reg_map[0].mem_base +
		tpg_reg->tpg_lfsr_seed);

	for (i = 0; i < tpg_data->num_active_dts; i++) {
		val = (((tpg_data->dt_cfg[i].frame_width & 0xFFFF) << 16) |
			(tpg_data->dt_cfg[i].frame_height & 0x3FFF));
		cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			tpg_reg->tpg_dt_0_cfg_0 + 0x10 * i);
		cam_io_w_mb(tpg_data->dt_cfg[i].data_type,
			soc_info->reg_map[0].mem_base +
			tpg_reg->tpg_dt_0_cfg_1 + 0x10 * i);
		val = ((tpg_data->dt_cfg[i].encode_format & 0xF) <<
			tpg_reg->tpg_dt_encode_format_shift) |
			tpg_reg->tpg_payload_mode_color;
		cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			tpg_reg->tpg_dt_0_cfg_2 + 0x10 * i);
	}

	val = (tpg_num_dt_map[tpg_data->num_active_dts-1] <<
		 tpg_reg->tpg_num_dts_shift_val) | tpg_data->vc_num[0];
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base + tpg_reg->tpg_vc_cfg0);

	/*
	 * if hblank is notset configureHBlank count 500 and
	 * V blank count is 600
	 */

	if (tpg_data->h_blank_count)
		cam_io_w_mb(tpg_data->h_blank_count,
			soc_info->reg_map[0].mem_base + tpg_reg->tpg_vc_cfg1);
	else
		cam_io_w_mb(0x2581F4,
		soc_info->reg_map[0].mem_base + tpg_reg->tpg_vc_cfg1);

	val = (1 << tpg_reg->tpg_split_en_shift);
	cam_io_w_mb(tpg_data->pix_pattern, soc_info->reg_map[0].mem_base +
		tpg_reg->tpg_common_gen_cfg);

	/* if VBI is notset configureVBI to 0xAFF */
	if (tpg_data->v_blank_count)
		cam_io_w_mb(tpg_data->v_blank_count,
			soc_info->reg_map[0].mem_base + tpg_reg->tpg_vbi_cfg);
	else
		cam_io_w_mb(0xAFFF,
			soc_info->reg_map[0].mem_base + tpg_reg->tpg_vbi_cfg);

	/* Set the TOP tpg mux sel*/
	val = cam_io_r_mb(soc_info->reg_map[1].mem_base +
			tpg_reg->top_mux_reg_offset);
	val |= (1 << tpg_hw->hw_intf->hw_idx);

	cam_io_w_mb(val,
		soc_info->reg_map[1].mem_base + tpg_reg->top_mux_reg_offset);
	CAM_DBG(CAM_ISP, "TPG:%d Set top Mux: 0x%x",
		tpg_hw->hw_intf->hw_idx, val);

	val = ((tpg_data->num_active_lanes - 1) <<
		tpg_reg->tpg_num_active_lines_shift) |
		(1 << tpg_reg->tpg_fe_pkt_en_shift) |
		(1 << tpg_reg->tpg_fs_pkt_en_shift) |
		(tpg_data->phy_sel << tpg_reg->tpg_phy_sel_shift_val) |
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

static int cam_top_tpg_ver1_stop(
	void                                         *hw_priv,
	void                                         *stop_args,
	uint32_t                                      arg_size)
{
	int                                           rc = 0;
	struct cam_top_tpg_hw                        *tpg_hw;
	struct cam_hw_info                           *tpg_hw_info;
	struct cam_hw_soc_info                       *soc_info;
	struct cam_isp_resource_node                 *tpg_res;
	const struct cam_top_tpg_ver1_reg_offset     *tpg_reg;
	struct cam_top_tpg_cfg                       *tpg_data;
	uint32_t                                      val;

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

	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		tpg_reg->tpg_ctrl);

	/* Reset the TOP tpg mux sel*/
	val = cam_io_r_mb(soc_info->reg_map[1].mem_base +
			tpg_reg->top_mux_reg_offset);
	val &= ~(1 << tpg_hw->hw_intf->hw_idx);

	cam_io_w_mb(val,
		soc_info->reg_map[1].mem_base + tpg_reg->top_mux_reg_offset);
	CAM_DBG(CAM_ISP, "TPG:%d Reset Top Mux: 0x%x",
		tpg_hw->hw_intf->hw_idx, val);

	tpg_res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;

	CAM_DBG(CAM_ISP, "TPG:%d stopped", tpg_hw->hw_intf->hw_idx);
end:
	return rc;
}

int cam_top_tpg_ver1_init(
	struct cam_top_tpg_hw                        *tpg_hw)
{
	tpg_hw->hw_intf->hw_ops.get_hw_caps = cam_top_tpg_ver1_get_hw_caps;
	tpg_hw->hw_intf->hw_ops.reserve     = cam_top_tpg_ver1_reserve;
	tpg_hw->hw_intf->hw_ops.start       = cam_top_tpg_ver1_start;
	tpg_hw->hw_intf->hw_ops.stop        = cam_top_tpg_ver1_stop;

	return 0;
}
