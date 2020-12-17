// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#include <linux/iopoll.h>
#include <linux/slab.h>
#include <media/cam_tfe.h>
#include <media/cam_defs.h>

#include "cam_top_tpg_core.h"
#include "cam_soc_util.h"
#include "cam_io_util.h"
#include "cam_debug_util.h"
#include "cam_cpas_api.h"


static uint32_t tpg_num_dt_map[CAM_TOP_TPG_MAX_SUPPORTED_DT] = {
	0,
	3,
	1,
	2
};

static int cam_top_tpg_get_format(uint32_t in_format,
	uint32_t *tpg_encode_format)
{
	int rc = 0;

	switch (in_format) {
	case CAM_FORMAT_MIPI_RAW_6:
		*tpg_encode_format = 0;
		break;
	case CAM_FORMAT_MIPI_RAW_8:
		*tpg_encode_format = 1;
		break;
	case CAM_FORMAT_MIPI_RAW_10:
		*tpg_encode_format = 2;
		break;
	case CAM_FORMAT_MIPI_RAW_12:
		*tpg_encode_format = 3;
		break;
	case CAM_FORMAT_MIPI_RAW_14:
		*tpg_encode_format = 4;
		break;
	case CAM_FORMAT_MIPI_RAW_16:
		*tpg_encode_format = 4;
		break;
	default:
		CAM_ERR(CAM_ISP, "Unsupported input encode format %d",
			in_format);
		rc = -EINVAL;
	}
	return rc;
}

static int cam_top_tpg_get_hw_caps(void *hw_priv,
	void *get_hw_cap_args, uint32_t arg_size)
{
	int rc = 0;
	struct cam_top_tpg_hw_caps           *hw_caps;
	struct cam_top_tpg_hw                *tpg_hw;
	struct cam_hw_info                   *tpg_hw_info;

	if (!hw_priv || !get_hw_cap_args) {
		CAM_ERR(CAM_ISP, "TPG: Invalid args");
		return -EINVAL;
	}

	tpg_hw_info = (struct cam_hw_info  *)hw_priv;
	tpg_hw = (struct cam_top_tpg_hw   *)tpg_hw_info->core_info;
	hw_caps = (struct cam_top_tpg_hw_caps *) get_hw_cap_args;

	hw_caps->major_version = tpg_hw->tpg_info->tpg_reg->major_version;
	hw_caps->minor_version = tpg_hw->tpg_info->tpg_reg->minor_version;
	hw_caps->version_incr = tpg_hw->tpg_info->tpg_reg->version_incr;

	CAM_DBG(CAM_ISP,
		"TPG:%d major:%d minor:%d ver :%d",
		tpg_hw->hw_intf->hw_idx, hw_caps->major_version,
		hw_caps->minor_version, hw_caps->version_incr);

	return rc;
}

static int cam_top_tpg_reserve(void *hw_priv,
	void *reserve_args, uint32_t arg_size)
{
	int rc = 0;
	struct cam_top_tpg_hw                        *tpg_hw;
	struct cam_hw_info                           *tpg_hw_info;
	struct cam_top_tpg_hw_reserve_resource_args  *reserv;
	struct cam_top_tpg_cfg                       *tpg_data;
	uint32_t                                      encode_format = 0;
	uint32_t i;

	if (!hw_priv || !reserve_args || (arg_size !=
		sizeof(struct cam_top_tpg_hw_reserve_resource_args))) {
		CAM_ERR(CAM_ISP, "TPG: Invalid args");
		return -EINVAL;
	}

	tpg_hw_info = (struct cam_hw_info *)hw_priv;
	tpg_hw = (struct cam_top_tpg_hw *)tpg_hw_info->core_info;
	reserv = (struct cam_top_tpg_hw_reserve_resource_args  *)reserve_args;

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
		return rc;

	CAM_DBG(CAM_ISP, "TPG: %u enter", tpg_hw->hw_intf->hw_idx);

	tpg_data = (struct cam_top_tpg_cfg *)tpg_hw->tpg_res.res_priv;
	tpg_data->vc_num = reserv->in_port[0]->vc;
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

	if (reserv->num_inport == 1)
		goto end;

	for (i = 1; i < reserv->num_inport; i++) {
		if ((tpg_data->vc_num != reserv->in_port[i]->vc) ||
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

static int cam_top_tpg_release(void *hw_priv,
	void *release_args, uint32_t arg_size)
{
	int rc = 0;
	struct cam_top_tpg_hw           *tpg_hw;
	struct cam_hw_info              *tpg_hw_info;
	struct cam_top_tpg_cfg          *tpg_data;
	struct cam_isp_resource_node    *tpg_res;

	if (!hw_priv || !release_args ||
		(arg_size != sizeof(struct cam_isp_resource_node))) {
		CAM_ERR(CAM_ISP, "TPG: Invalid args");
		return -EINVAL;
	}

	tpg_hw_info = (struct cam_hw_info  *)hw_priv;
	tpg_hw = (struct cam_top_tpg_hw   *)tpg_hw_info->core_info;
	tpg_res = (struct cam_isp_resource_node *)release_args;

	mutex_lock(&tpg_hw->hw_info->hw_mutex);
	if ((tpg_res->res_type != CAM_ISP_RESOURCE_TPG) ||
		(tpg_res->res_state <= CAM_ISP_RESOURCE_STATE_AVAILABLE)) {
		CAM_ERR(CAM_ISP, "TPG:%d Invalid res type:%d res_state:%d",
			tpg_hw->hw_intf->hw_idx, tpg_res->res_type,
			tpg_res->res_state);
		rc = -EINVAL;
		goto end;
	}

	CAM_DBG(CAM_ISP, "TPG:%d res type :%d",
		tpg_hw->hw_intf->hw_idx, tpg_res->res_type);

	tpg_res->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;
	tpg_data = (struct cam_top_tpg_cfg *)tpg_res->res_priv;
	memset(tpg_data, 0, sizeof(struct cam_top_tpg_cfg));

end:
	mutex_unlock(&tpg_hw->hw_info->hw_mutex);
	return rc;
}

static int cam_top_tpg_init_hw(void *hw_priv,
	void *init_args, uint32_t arg_size)
{
	int rc = 0;
	struct cam_top_tpg_hw                  *tpg_hw;
	struct cam_hw_info                     *tpg_hw_info;
	struct cam_isp_resource_node           *tpg_res;
	const struct cam_top_tpg_reg_offset    *tpg_reg;
	struct cam_hw_soc_info                 *soc_info;
	uint32_t val, clk_lvl;

	if (!hw_priv || !init_args ||
		(arg_size != sizeof(struct cam_isp_resource_node))) {
		CAM_ERR(CAM_ISP, "TPG: Invalid args");
		return -EINVAL;
	}

	tpg_hw_info = (struct cam_hw_info  *)hw_priv;
	tpg_hw = (struct cam_top_tpg_hw   *)tpg_hw_info->core_info;
	tpg_res      = (struct cam_isp_resource_node *)init_args;
	tpg_reg = tpg_hw->tpg_info->tpg_reg;
	soc_info = &tpg_hw->hw_info->soc_info;

	if (tpg_res->res_type != CAM_ISP_RESOURCE_TPG) {
		CAM_ERR(CAM_ISP, "TPG:%d Invalid res type state %d",
			tpg_hw->hw_intf->hw_idx,
			tpg_res->res_type);
		return -EINVAL;
	}

	CAM_DBG(CAM_ISP, "TPG:%d init HW res type :%d",
		tpg_hw->hw_intf->hw_idx, tpg_res->res_type);
	mutex_lock(&tpg_hw->hw_info->hw_mutex);
	/* overflow check before increment */
	if (tpg_hw->hw_info->open_count == UINT_MAX) {
		CAM_ERR(CAM_ISP, "TPG:%d Open count reached max",
			tpg_hw->hw_intf->hw_idx);
		mutex_unlock(&tpg_hw->hw_info->hw_mutex);
		return -EINVAL;
	}

	/* Increment ref Count */
	tpg_hw->hw_info->open_count++;
	if (tpg_hw->hw_info->open_count > 1) {
		CAM_DBG(CAM_ISP, "TPG hw has already been enabled");
		mutex_unlock(&tpg_hw->hw_info->hw_mutex);
		return rc;
	}

	rc = cam_soc_util_get_clk_level(soc_info, tpg_hw->clk_rate,
		soc_info->src_clk_idx, &clk_lvl);
	CAM_DBG(CAM_ISP, "TPG phy clock level %u", clk_lvl);

	rc = cam_top_tpg_enable_soc_resources(soc_info, clk_lvl);
	if (rc) {
		CAM_ERR(CAM_ISP, "TPG:%d Enable SOC failed",
			tpg_hw->hw_intf->hw_idx);
		goto err;
	}

	tpg_hw->hw_info->hw_state = CAM_HW_STATE_POWER_UP;

	val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			tpg_reg->tpg_hw_version);
	CAM_DBG(CAM_ISP, "TPG:%d TPG HW version: 0x%x",
		tpg_hw->hw_intf->hw_idx, val);

	mutex_unlock(&tpg_hw->hw_info->hw_mutex);
	return rc;

err:
	tpg_hw->hw_info->open_count--;
	mutex_unlock(&tpg_hw->hw_info->hw_mutex);
	return rc;
}

static int cam_top_tpg_deinit_hw(void *hw_priv,
	void *deinit_args, uint32_t arg_size)
{
	int rc = 0;
	struct cam_top_tpg_hw                 *tpg_hw;
	struct cam_hw_info                    *tpg_hw_info;
	struct cam_isp_resource_node          *tpg_res;
	struct cam_hw_soc_info                *soc_info;

	if (!hw_priv || !deinit_args ||
		(arg_size != sizeof(struct cam_isp_resource_node))) {
		CAM_ERR(CAM_ISP, "TPG:Invalid arguments");
		return -EINVAL;
	}

	tpg_res = (struct cam_isp_resource_node *)deinit_args;
	tpg_hw_info = (struct cam_hw_info  *)hw_priv;
	tpg_hw = (struct cam_top_tpg_hw   *)tpg_hw_info->core_info;

	if (tpg_res->res_type != CAM_ISP_RESOURCE_TPG) {
		CAM_ERR(CAM_ISP, "TPG:%d Invalid Res type %d",
			tpg_hw->hw_intf->hw_idx,
			tpg_res->res_type);
		return -EINVAL;
	}

	mutex_lock(&tpg_hw->hw_info->hw_mutex);
	/* Check for refcount */
	if (!tpg_hw->hw_info->open_count) {
		CAM_WARN(CAM_ISP, "Unbalanced disable_hw");
		goto end;
	}

	/* Decrement ref Count */
	tpg_hw->hw_info->open_count--;
	if (tpg_hw->hw_info->open_count) {
		rc = 0;
		goto end;
	}

	soc_info = &tpg_hw->hw_info->soc_info;
	rc = cam_top_tpg_disable_soc_resources(soc_info);
	if (rc)
		CAM_ERR(CAM_ISP, "TPG:%d Disable SOC failed",
			tpg_hw->hw_intf->hw_idx);

	tpg_hw->hw_info->hw_state = CAM_HW_STATE_POWER_DOWN;
	CAM_DBG(CAM_ISP, "TPG:%d deint completed", tpg_hw->hw_intf->hw_idx);

end:
	mutex_unlock(&tpg_hw->hw_info->hw_mutex);
	return rc;
}

static int cam_top_tpg_start(void *hw_priv, void *start_args,
			uint32_t arg_size)
{
	int rc = 0;
	struct cam_top_tpg_hw                  *tpg_hw;
	struct cam_hw_info                     *tpg_hw_info;
	struct cam_hw_soc_info                 *soc_info;
	struct cam_isp_resource_node           *tpg_res;
	const struct cam_top_tpg_reg_offset    *tpg_reg;
	struct cam_top_tpg_cfg                 *tpg_data;
	uint32_t i, val;

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
		 tpg_reg->tpg_num_dts_shift_val) | tpg_data->vc_num;
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
	cam_io_w_mb((1 << tpg_hw->hw_intf->hw_idx),
		soc_info->reg_map[1].mem_base + tpg_reg->top_mux_reg_offset);

	val = ((tpg_data->num_active_lanes - 1) <<
		tpg_reg->tpg_num_active_lines_shift) |
		(1 << tpg_reg->tpg_fe_pkt_en_shift) |
		(1 << tpg_reg->tpg_fs_pkt_en_shift) |
		(tpg_data->phy_sel << tpg_reg->tpg_phy_sel_shift_val) |
		(1 << tpg_reg->tpg_en_shift_val);
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base + tpg_reg->tpg_ctrl);

	tpg_res->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;

	CAM_DBG(CAM_ISP, "TPG:%d started", tpg_hw->hw_intf->hw_idx);

end:
	return rc;
}

static int cam_top_tpg_stop(void *hw_priv,
	void *stop_args, uint32_t arg_size)
{
	int rc = 0;
	struct cam_top_tpg_hw                  *tpg_hw;
	struct cam_hw_info                     *tpg_hw_info;
	struct cam_hw_soc_info                 *soc_info;
	struct cam_isp_resource_node           *tpg_res;
	const struct cam_top_tpg_reg_offset    *tpg_reg;
	struct cam_top_tpg_cfg                 *tpg_data;

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

	tpg_res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;

	CAM_DBG(CAM_ISP, "TPG:%d stopped", tpg_hw->hw_intf->hw_idx);
end:
	return rc;
}

static int cam_top_tpg_read(void *hw_priv,
	void *read_args, uint32_t arg_size)
{
	CAM_ERR(CAM_ISP, "TPG: un supported");

	return -EINVAL;
}

static int cam_top_tpg_write(void *hw_priv,
	void *write_args, uint32_t arg_size)
{
	CAM_ERR(CAM_ISP, "TPG: un supported");
	return -EINVAL;
}

static int cam_top_tpg_set_phy_clock(
	struct cam_top_tpg_hw *csid_hw, void *cmd_args)
{
	struct cam_top_tpg_clock_update_args *clk_update = NULL;

	if (!csid_hw)
		return -EINVAL;

	clk_update =
		(struct cam_top_tpg_clock_update_args *)cmd_args;

	csid_hw->clk_rate = clk_update->clk_rate;
	CAM_DBG(CAM_ISP, "CSI PHY clock rate %llu", csid_hw->clk_rate);

	return 0;
}

static int cam_top_tpg_process_cmd(void *hw_priv,
	uint32_t cmd_type, void *cmd_args, uint32_t arg_size)
{
	int rc = 0;
	struct cam_top_tpg_hw               *tpg_hw;
	struct cam_hw_info                  *tpg_hw_info;

	if (!hw_priv || !cmd_args) {
		CAM_ERR(CAM_ISP, "CSID: Invalid arguments");
		return -EINVAL;
	}

	tpg_hw_info = (struct cam_hw_info  *)hw_priv;
	tpg_hw = (struct cam_top_tpg_hw   *)tpg_hw_info->core_info;

	switch (cmd_type) {
	case CAM_ISP_HW_CMD_TPG_PHY_CLOCK_UPDATE:
		rc = cam_top_tpg_set_phy_clock(tpg_hw, cmd_args);
		break;
	default:
		CAM_ERR(CAM_ISP, "TPG:%d unsupported cmd:%d",
			tpg_hw->hw_intf->hw_idx, cmd_type);
		rc = -EINVAL;
		break;
	}

	return 0;
}

int cam_top_tpg_hw_probe_init(struct cam_hw_intf  *tpg_hw_intf,
	uint32_t tpg_idx)
{
	int rc = -EINVAL;
	struct cam_top_tpg_cfg             *tpg_data;
	struct cam_hw_info                 *tpg_hw_info;
	struct cam_top_tpg_hw              *tpg_hw = NULL;
	uint32_t val = 0;

	if (tpg_idx >= CAM_TOP_TPG_HW_NUM_MAX) {
		CAM_ERR(CAM_ISP, "Invalid tpg index:%d", tpg_idx);
		return rc;
	}

	tpg_hw_info = (struct cam_hw_info  *)tpg_hw_intf->hw_priv;
	tpg_hw      = (struct cam_top_tpg_hw  *)tpg_hw_info->core_info;

	tpg_hw->hw_intf = tpg_hw_intf;
	tpg_hw->hw_info = tpg_hw_info;

	CAM_DBG(CAM_ISP, "type %d index %d",
		tpg_hw->hw_intf->hw_type, tpg_idx);

	tpg_hw->hw_info->hw_state = CAM_HW_STATE_POWER_DOWN;
	mutex_init(&tpg_hw->hw_info->hw_mutex);
	spin_lock_init(&tpg_hw->hw_info->hw_lock);
	spin_lock_init(&tpg_hw->lock_state);
	init_completion(&tpg_hw->hw_info->hw_complete);

	init_completion(&tpg_hw->tpg_complete);

	rc = cam_top_tpg_init_soc_resources(&tpg_hw->hw_info->soc_info,
			tpg_hw);
	if (rc < 0) {
		CAM_ERR(CAM_ISP, "TPG:%d Failed to init_soc", tpg_idx);
		goto err;
	}

	tpg_hw->hw_intf->hw_ops.get_hw_caps = cam_top_tpg_get_hw_caps;
	tpg_hw->hw_intf->hw_ops.init        = cam_top_tpg_init_hw;
	tpg_hw->hw_intf->hw_ops.deinit      = cam_top_tpg_deinit_hw;
	tpg_hw->hw_intf->hw_ops.reset       = NULL;
	tpg_hw->hw_intf->hw_ops.reserve     = cam_top_tpg_reserve;
	tpg_hw->hw_intf->hw_ops.release     = cam_top_tpg_release;
	tpg_hw->hw_intf->hw_ops.start       = cam_top_tpg_start;
	tpg_hw->hw_intf->hw_ops.stop        = cam_top_tpg_stop;
	tpg_hw->hw_intf->hw_ops.read        = cam_top_tpg_read;
	tpg_hw->hw_intf->hw_ops.write       = cam_top_tpg_write;
	tpg_hw->hw_intf->hw_ops.process_cmd = cam_top_tpg_process_cmd;

	tpg_hw->tpg_res.res_type = CAM_ISP_RESOURCE_TPG;
	tpg_hw->tpg_res.res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;
	tpg_hw->tpg_res.hw_intf = tpg_hw->hw_intf;
	tpg_data = kzalloc(sizeof(*tpg_data), GFP_KERNEL);
	if (!tpg_data) {
		rc = -ENOMEM;
		goto err;
	}
	tpg_hw->tpg_res.res_priv = tpg_data;

	cam_top_tpg_enable_soc_resources(&tpg_hw->hw_info->soc_info,
		CAM_SVS_VOTE);

	val = cam_io_r_mb(tpg_hw->hw_info->soc_info.reg_map[0].mem_base +
			tpg_hw->tpg_info->tpg_reg->tpg_hw_version);
	CAM_DBG(CAM_ISP, "TPG:%d TPG HW version: 0x%x",
		tpg_hw->hw_intf->hw_idx, val);

	cam_top_tpg_disable_soc_resources(&tpg_hw->hw_info->soc_info);
err:

	return rc;
}

int cam_top_tpg_hw_deinit(struct cam_top_tpg_hw *top_tpg_hw)
{
	int rc = -EINVAL;

	if (!top_tpg_hw) {
		CAM_ERR(CAM_ISP, "Invalid param");
		return rc;
	}

	/* release the privdate data memory from resources */
	kfree(top_tpg_hw->tpg_res.res_priv);
	cam_top_tpg_deinit_soc_resources(&top_tpg_hw->hw_info->soc_info);

	return 0;
}
