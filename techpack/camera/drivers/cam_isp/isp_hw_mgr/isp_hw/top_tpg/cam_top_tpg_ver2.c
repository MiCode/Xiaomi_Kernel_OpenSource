// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/iopoll.h>
#include <linux/slab.h>
#include <media/cam_defs.h>

#include "cam_top_tpg_core.h"
#include "cam_soc_util.h"
#include "cam_io_util.h"
#include "cam_debug_util.h"
#include "cam_top_tpg_ver2.h"

#define CAM_TOP_TPG_VER2_MAX_SUPPORTED_DT 1

static int cam_top_tpg_ver2_get_hw_caps(
	void                                         *hw_priv,
	void                                         *get_hw_cap_args,
	uint32_t                                      arg_size)
{
	int                                           rc = 0;
	struct cam_top_tpg_hw_caps                   *hw_caps;
	struct cam_top_tpg_hw                        *tpg_hw;
	const struct cam_top_tpg_ver2_reg_offset     *tpg_reg;
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

static int cam_top_tpg_ver2_reserve(
	void                                         *hw_priv,
	void                                         *reserve_args,
	uint32_t                                      arg_size)
{
	int                                           rc = 0;
	struct cam_top_tpg_hw                        *tpg_hw;
	struct cam_hw_info                           *tpg_hw_info;
	struct cam_top_tpg_ver2_reserve_args         *reserv;
	struct cam_top_tpg_cfg                       *tpg_data;

	if (!hw_priv || !reserve_args || (arg_size !=
		sizeof(struct cam_top_tpg_ver2_reserve_args))) {
		CAM_ERR(CAM_ISP, "TPG: Invalid args");
		return -EINVAL;
	}

	tpg_hw_info = (struct cam_hw_info *)hw_priv;
	tpg_hw = (struct cam_top_tpg_hw *)tpg_hw_info->core_info;
	reserv = (struct cam_top_tpg_ver2_reserve_args  *)reserve_args;

	if (reserv->num_inport <= 0 ||
		reserv->num_inport > CAM_TOP_TPG_VER2_MAX_SUPPORTED_DT) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "TPG: %u invalid input num port:%d",
			tpg_hw->hw_intf->hw_idx, reserv->num_inport);
		return -EINVAL;
	}

	mutex_lock(&tpg_hw->hw_info->hw_mutex);
	if (tpg_hw->tpg_res.res_state != CAM_ISP_RESOURCE_STATE_AVAILABLE) {
		CAM_ERR(CAM_ISP, "TPG:%d resource not available state:%d",
			tpg_hw->hw_intf->hw_idx, tpg_hw->tpg_res.res_state);
		mutex_unlock(&tpg_hw->hw_info->hw_mutex);
		return -EINVAL;
	}

	tpg_data = (struct cam_top_tpg_cfg *)tpg_hw->tpg_res.res_priv;
	tpg_data->h_blank_count = reserv->in_port->hbi_cnt;
	tpg_data->v_blank_count = 600;
	tpg_data->dt_cfg[0].data_type = reserv->in_port->dt[0];
	tpg_data->dt_cfg[0].frame_height = reserv->in_port->height;
	if (reserv->in_port->usage_type)
		tpg_data->dt_cfg[0].frame_width =
			((reserv->in_port->right_stop -
				reserv->in_port->left_start) + 1);
	else
		tpg_data->dt_cfg[0].frame_width =
			reserv->in_port->left_width;
	tpg_data->num_active_dts = 1;

	CAM_DBG(CAM_ISP, "TPG:%u dt:0x%x h:%d w:%d hbi:%d vbi:%d reserved",
		tpg_hw->hw_intf->hw_idx,
		tpg_data->dt_cfg[0].data_type,
		tpg_data->dt_cfg[0].frame_height,
		tpg_data->dt_cfg[0].frame_width,
		tpg_data->h_blank_count,
		tpg_data->v_blank_count);

	reserv->node_res = &tpg_hw->tpg_res;
	tpg_hw->tpg_res.res_state = CAM_ISP_RESOURCE_STATE_RESERVED;

	mutex_unlock(&tpg_hw->hw_info->hw_mutex);

	return rc;
}

static int cam_top_tpg_ver2_start(
	void                                         *hw_priv,
	void                                         *start_args,
	uint32_t                                      arg_size)
{
	int                                           rc = 0;
	struct cam_top_tpg_hw                        *tpg_hw;
	struct cam_hw_info                           *tpg_hw_info;
	struct cam_hw_soc_info                       *soc_info;
	struct cam_isp_resource_node                 *tpg_res;
	struct cam_top_tpg_ver2_reg_offset           *tpg_reg;
	struct cam_top_tpg_cfg                       *tpg_data;
	uint32_t                                      val;
	uint32_t                                      mux_sel_shift = 0;

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

	if (tpg_hw->hw_intf->hw_idx == CAM_TOP_TPG_ID_0)
		mux_sel_shift = tpg_reg->tpg_mux_sel_tpg_0_shift;
	else if (tpg_hw->hw_intf->hw_idx == CAM_TOP_TPG_ID_1)
		mux_sel_shift = tpg_reg->tpg_mux_sel_tpg_1_shift;

	cam_io_w_mb((tpg_reg->tpg_mux_sel_en << mux_sel_shift),
		soc_info->reg_map[1].mem_base + tpg_reg->top_mux_sel);

	val = (((tpg_data->dt_cfg[0].frame_width & 0x3FFF) << 16) |
		(tpg_data->dt_cfg[0].frame_height & 0x3FFF));

	cam_io_w_mb(val, soc_info->reg_map[0].mem_base + tpg_reg->tpg_cfg_0);

	val = (tpg_data->h_blank_count & 0x7FF) << 20;
	val |= (tpg_data->v_blank_count & 0x3FFFF);

	cam_io_w_mb(val, soc_info->reg_map[0].mem_base + tpg_reg->tpg_cfg_1);

	val = tpg_data->dt_cfg[0].data_type << 11;
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base + tpg_reg->tpg_cfg_2);

	/* program number of frames */
	cam_io_w_mb(0xFFFFF, soc_info->reg_map[0].mem_base + tpg_reg->tpg_cfg_3);

	cam_io_w_mb(tpg_reg->tpg_module_en, soc_info->reg_map[0].mem_base +
		tpg_reg->tpg_module_cfg);

	tpg_res->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;
	CAM_DBG(CAM_ISP, "TPG:%d started", tpg_hw->hw_intf->hw_idx);

end:
	return rc;
}

static int cam_top_tpg_ver2_stop(
	void                                         *hw_priv,
	void                                         *stop_args,
	uint32_t                                      arg_size)
{
	int                                           rc = 0;
	struct cam_top_tpg_hw                        *tpg_hw;
	struct cam_hw_info                           *tpg_hw_info;
	struct cam_hw_soc_info                       *soc_info;
	struct cam_isp_resource_node                 *tpg_res;
	const struct cam_top_tpg_ver2_reg_offset     *tpg_reg;
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

	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
		tpg_reg->tpg_module_cfg);

	tpg_res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;

	CAM_DBG(CAM_ISP, "TPG:%d stopped", tpg_hw->hw_intf->hw_idx);
end:
	return rc;
}

int cam_top_tpg_ver2_init(
	struct cam_top_tpg_hw                        *tpg_hw)
{
	tpg_hw->hw_intf->hw_ops.get_hw_caps = cam_top_tpg_ver2_get_hw_caps;
	tpg_hw->hw_intf->hw_ops.reserve     = cam_top_tpg_ver2_reserve;
	tpg_hw->hw_intf->hw_ops.start       = cam_top_tpg_ver2_start;
	tpg_hw->hw_intf->hw_ops.stop        = cam_top_tpg_ver2_stop;

	return 0;
}
