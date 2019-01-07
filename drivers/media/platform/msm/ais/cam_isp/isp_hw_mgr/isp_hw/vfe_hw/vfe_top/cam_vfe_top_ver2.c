/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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
#include "cam_io_util.h"
#include "cam_cdm_util.h"
#include "cam_vfe_hw_intf.h"
#include "cam_vfe_top.h"
#include "cam_vfe_top_ver2.h"
#include "cam_debug_util.h"
#include "cam_cpas_api.h"
#include "cam_vfe_soc.h"

#define CAM_VFE_HW_RESET_HW_AND_REG_VAL       0x00003F9F
#define CAM_VFE_HW_RESET_HW_VAL               0x00003F87
#define CAM_VFE_DELAY_BW_REDUCTION_NUM_FRAMES 3

struct cam_vfe_top_ver2_common_data {
	struct cam_hw_soc_info                     *soc_info;
	struct cam_hw_intf                         *hw_intf;
	struct cam_vfe_top_ver2_reg_offset_common  *common_reg;
};

struct cam_vfe_top_ver2_priv {
	struct cam_vfe_top_ver2_common_data common_data;
	struct cam_isp_resource_node        mux_rsrc[CAM_VFE_TOP_VER2_MUX_MAX];
	unsigned long                       hw_clk_rate;
	struct cam_axi_vote                 applied_axi_vote;
	struct cam_axi_vote             req_axi_vote[CAM_VFE_TOP_VER2_MUX_MAX];
	unsigned long                   req_clk_rate[CAM_VFE_TOP_VER2_MUX_MAX];
	struct cam_axi_vote             last_vote[CAM_VFE_TOP_VER2_MUX_MAX *
					CAM_VFE_DELAY_BW_REDUCTION_NUM_FRAMES];
	uint32_t                        last_counter;
	enum cam_vfe_bw_control_action
		axi_vote_control[CAM_VFE_TOP_VER2_MUX_MAX];
};

static int cam_vfe_top_mux_get_base(struct cam_vfe_top_ver2_priv *top_priv,
	void *cmd_args, uint32_t arg_size)
{
	uint32_t                          size = 0;
	uint32_t                          mem_base = 0;
	struct cam_isp_hw_get_cmd_update *cdm_args  = cmd_args;
	struct cam_cdm_utils_ops         *cdm_util_ops = NULL;

	if (arg_size != sizeof(struct cam_isp_hw_get_cmd_update)) {
		CAM_ERR(CAM_ISP, "Error! Invalid cmd size");
		return -EINVAL;
	}

	if (!cdm_args || !cdm_args->res || !top_priv ||
		!top_priv->common_data.soc_info) {
		CAM_ERR(CAM_ISP, "Error! Invalid args");
		return -EINVAL;
	}

	cdm_util_ops =
		(struct cam_cdm_utils_ops *)cdm_args->res->cdm_ops;

	if (!cdm_util_ops) {
		CAM_ERR(CAM_ISP, "Invalid CDM ops");
		return -EINVAL;
	}

	size = cdm_util_ops->cdm_required_size_changebase();
	/* since cdm returns dwords, we need to convert it into bytes */
	if ((size * 4) > cdm_args->cmd.size) {
		CAM_ERR(CAM_ISP, "buf size:%d is not sufficient, expected: %d",
			cdm_args->cmd.size, size);
		return -EINVAL;
	}

	mem_base = CAM_SOC_GET_REG_MAP_CAM_BASE(
		top_priv->common_data.soc_info, VFE_CORE_BASE_IDX);
	CAM_DBG(CAM_ISP, "core %d mem_base 0x%x",
		top_priv->common_data.soc_info->index, mem_base);

	cdm_util_ops->cdm_write_changebase(
	cdm_args->cmd.cmd_buf_addr, mem_base);
	cdm_args->cmd.used_bytes = (size * 4);

	return 0;
}

static int cam_vfe_top_set_hw_clk_rate(
	struct cam_vfe_top_ver2_priv *top_priv)
{
	struct cam_hw_soc_info        *soc_info = NULL;
	int                            i, rc = 0;
	unsigned long                  max_clk_rate = 0;

	soc_info = top_priv->common_data.soc_info;

	for (i = 0; i < CAM_VFE_TOP_VER2_MUX_MAX; i++) {
		if (top_priv->req_clk_rate[i] > max_clk_rate)
			max_clk_rate = top_priv->req_clk_rate[i];
	}
	if (max_clk_rate == top_priv->hw_clk_rate)
		return 0;

	CAM_DBG(CAM_ISP, "VFE: Clock name=%s idx=%d clk=%llu",
		soc_info->clk_name[soc_info->src_clk_idx],
		soc_info->src_clk_idx, max_clk_rate);

	rc = cam_soc_util_set_src_clk_rate(soc_info, max_clk_rate);

	if (!rc)
		top_priv->hw_clk_rate = max_clk_rate;
	else
		CAM_ERR(CAM_ISP, "Set Clock rate failed, rc=%d", rc);

	return rc;
}

static int cam_vfe_top_set_axi_bw_vote(
	struct cam_vfe_top_ver2_priv *top_priv,
	bool start_stop)
{
	struct cam_axi_vote sum = {0, 0};
	struct cam_axi_vote to_be_applied_axi_vote = {0, 0};
	int i, rc = 0;
	struct cam_hw_soc_info   *soc_info =
		top_priv->common_data.soc_info;
	struct cam_vfe_soc_private *soc_private =
		soc_info->soc_private;
	bool apply_bw_update = false;

	if (!soc_private) {
		CAM_ERR(CAM_ISP, "Error soc_private NULL");
		return -EINVAL;
	}

	for (i = 0; i < CAM_VFE_TOP_VER2_MUX_MAX; i++) {
		if (top_priv->axi_vote_control[i] ==
			CAM_VFE_BW_CONTROL_INCLUDE) {
			sum.uncompressed_bw +=
				top_priv->req_axi_vote[i].uncompressed_bw;
			sum.compressed_bw +=
				top_priv->req_axi_vote[i].compressed_bw;
		}
	}

	CAM_DBG(CAM_ISP, "Updating BW from (%llu %llu) to (%llu %llu)",
		top_priv->applied_axi_vote.uncompressed_bw,
		top_priv->applied_axi_vote.compressed_bw,
		sum.uncompressed_bw,
		sum.compressed_bw);

	top_priv->last_vote[top_priv->last_counter] = sum;
	top_priv->last_counter = (top_priv->last_counter + 1) %
		(CAM_VFE_TOP_VER2_MUX_MAX *
		CAM_VFE_DELAY_BW_REDUCTION_NUM_FRAMES);

	if ((top_priv->applied_axi_vote.uncompressed_bw ==
		sum.uncompressed_bw) &&
		(top_priv->applied_axi_vote.compressed_bw ==
		sum.compressed_bw)) {
		CAM_DBG(CAM_ISP, "BW config unchanged %llu %llu",
			top_priv->applied_axi_vote.uncompressed_bw,
			top_priv->applied_axi_vote.compressed_bw);
		return 0;
	}

	if (start_stop == true) {
		/* need to vote current request immediately */
		to_be_applied_axi_vote = sum;
		/* Reset everything, we can start afresh */
		memset(top_priv->last_vote, 0x0, sizeof(struct cam_axi_vote) *
			(CAM_VFE_TOP_VER2_MUX_MAX *
			CAM_VFE_DELAY_BW_REDUCTION_NUM_FRAMES));
		top_priv->last_counter = 0;
		top_priv->last_vote[top_priv->last_counter] = sum;
		top_priv->last_counter = (top_priv->last_counter + 1) %
			(CAM_VFE_TOP_VER2_MUX_MAX *
			CAM_VFE_DELAY_BW_REDUCTION_NUM_FRAMES);
	} else {
		/*
		 * Find max bw request in last few frames. This will the bw
		 *that we want to vote to CPAS now.
		 */
		for (i = 0; i < (CAM_VFE_TOP_VER2_MUX_MAX *
			CAM_VFE_DELAY_BW_REDUCTION_NUM_FRAMES); i++) {
			if (to_be_applied_axi_vote.compressed_bw <
				top_priv->last_vote[i].compressed_bw)
				to_be_applied_axi_vote.compressed_bw =
					top_priv->last_vote[i].compressed_bw;

			if (to_be_applied_axi_vote.uncompressed_bw <
				top_priv->last_vote[i].uncompressed_bw)
				to_be_applied_axi_vote.uncompressed_bw =
					top_priv->last_vote[i].uncompressed_bw;
		}
	}

	if ((to_be_applied_axi_vote.uncompressed_bw !=
		top_priv->applied_axi_vote.uncompressed_bw) ||
		(to_be_applied_axi_vote.compressed_bw !=
		top_priv->applied_axi_vote.compressed_bw))
		apply_bw_update = true;

	CAM_DBG(CAM_ISP, "apply_bw_update=%d", apply_bw_update);

	if (apply_bw_update == true) {
		rc = cam_cpas_update_axi_vote(
			soc_private->cpas_handle,
			&to_be_applied_axi_vote);
		if (!rc) {
			top_priv->applied_axi_vote.uncompressed_bw =
				to_be_applied_axi_vote.uncompressed_bw;
			top_priv->applied_axi_vote.compressed_bw =
				to_be_applied_axi_vote.compressed_bw;
		} else {
			CAM_ERR(CAM_ISP, "BW request failed, rc=%d", rc);
		}
	}

	return rc;
}

static int cam_vfe_top_clock_update(
	struct cam_vfe_top_ver2_priv *top_priv,
	void *cmd_args, uint32_t arg_size)
{
	struct cam_vfe_clock_update_args     *clk_update = NULL;
	struct cam_isp_resource_node         *res = NULL;
	struct cam_hw_info                   *hw_info = NULL;
	int                                   i, rc = 0;

	clk_update =
		(struct cam_vfe_clock_update_args *)cmd_args;
	res = clk_update->node_res;

	if (!res || !res->hw_intf->hw_priv) {
		CAM_ERR(CAM_ISP, "Invalid input res %pK", res);
		return -EINVAL;
	}

	hw_info = res->hw_intf->hw_priv;

	if (res->res_type != CAM_ISP_RESOURCE_VFE_IN ||
		res->res_id >= CAM_ISP_HW_VFE_IN_MAX) {
		CAM_ERR(CAM_ISP, "VFE:%d Invalid res_type:%d res id%d",
			res->hw_intf->hw_idx, res->res_type,
			res->res_id);
		return -EINVAL;
	}

	for (i = 0; i < CAM_VFE_TOP_VER2_MUX_MAX; i++) {
		if (top_priv->mux_rsrc[i].res_id == res->res_id) {
			top_priv->req_clk_rate[i] = clk_update->clk_rate;
			break;
		}
	}

	if (hw_info->hw_state != CAM_HW_STATE_POWER_UP) {
		CAM_DBG(CAM_ISP,
			"VFE:%d Not ready to set clocks yet :%d",
			res->hw_intf->hw_idx,
			hw_info->hw_state);
	} else
		rc = cam_vfe_top_set_hw_clk_rate(top_priv);

	return rc;
}

static int cam_vfe_top_bw_update(
	struct cam_vfe_top_ver2_priv *top_priv,
	void *cmd_args, uint32_t arg_size)
{
	struct cam_vfe_bw_update_args        *bw_update = NULL;
	struct cam_isp_resource_node         *res = NULL;
	struct cam_hw_info                   *hw_info = NULL;
	int                                   rc = 0;
	int                                   i;

	bw_update = (struct cam_vfe_bw_update_args *)cmd_args;
	res = bw_update->node_res;

	if (!res || !res->hw_intf->hw_priv)
		return -EINVAL;

	hw_info = res->hw_intf->hw_priv;

	if (res->res_type != CAM_ISP_RESOURCE_VFE_IN ||
		res->res_id >= CAM_ISP_HW_VFE_IN_MAX) {
		CAM_ERR(CAM_ISP, "VFE:%d Invalid res_type:%d res id%d",
			res->hw_intf->hw_idx, res->res_type,
			res->res_id);
		return -EINVAL;
	}

	for (i = 0; i < CAM_VFE_TOP_VER2_MUX_MAX; i++) {
		if (top_priv->mux_rsrc[i].res_id == res->res_id) {
			top_priv->req_axi_vote[i].uncompressed_bw =
				bw_update->camnoc_bw_bytes;
			top_priv->req_axi_vote[i].compressed_bw =
				bw_update->external_bw_bytes;
			top_priv->axi_vote_control[i] =
				CAM_VFE_BW_CONTROL_INCLUDE;
			break;
		}
	}

	if (hw_info->hw_state != CAM_HW_STATE_POWER_UP) {
		CAM_ERR_RATE_LIMIT(CAM_ISP,
			"VFE:%d Not ready to set BW yet :%d",
			res->hw_intf->hw_idx,
			hw_info->hw_state);
	} else
		rc = cam_vfe_top_set_axi_bw_vote(top_priv, false);

	return rc;
}

static int cam_vfe_top_bw_control(
	struct cam_vfe_top_ver2_priv *top_priv,
	 void *cmd_args, uint32_t arg_size)
{
	struct cam_vfe_bw_control_args       *bw_ctrl = NULL;
	struct cam_isp_resource_node         *res = NULL;
	struct cam_hw_info                   *hw_info = NULL;
	int                                   rc = 0;
	int                                   i;

	bw_ctrl = (struct cam_vfe_bw_control_args *)cmd_args;
	res = bw_ctrl->node_res;

	if (!res || !res->hw_intf->hw_priv)
		return -EINVAL;

	hw_info = res->hw_intf->hw_priv;

	if (res->res_type != CAM_ISP_RESOURCE_VFE_IN ||
		res->res_id >= CAM_ISP_HW_VFE_IN_MAX) {
		CAM_ERR(CAM_ISP, "VFE:%d Invalid res_type:%d res id%d",
			res->hw_intf->hw_idx, res->res_type,
			res->res_id);
		return -EINVAL;
	}

	for (i = 0; i < CAM_VFE_TOP_VER2_MUX_MAX; i++) {
		if (top_priv->mux_rsrc[i].res_id == res->res_id) {
			top_priv->axi_vote_control[i] = bw_ctrl->action;
			break;
		}
	}

	if (hw_info->hw_state != CAM_HW_STATE_POWER_UP) {
		CAM_ERR_RATE_LIMIT(CAM_ISP,
			"VFE:%d Not ready to set BW yet :%d",
			res->hw_intf->hw_idx,
			hw_info->hw_state);
	} else {
		rc = cam_vfe_top_set_axi_bw_vote(top_priv, true);
	}

	return rc;
}

static int cam_vfe_top_mux_get_reg_update(
	struct cam_vfe_top_ver2_priv *top_priv,
	void *cmd_args, uint32_t arg_size)
{
	struct cam_isp_hw_get_cmd_update  *cmd_update = cmd_args;

	if (cmd_update->res->process_cmd)
		return cmd_update->res->process_cmd(cmd_update->res,
			CAM_ISP_HW_CMD_GET_REG_UPDATE, cmd_args, arg_size);

	return -EINVAL;
}

int cam_vfe_top_get_hw_caps(void *device_priv,
	void *get_hw_cap_args, uint32_t arg_size)
{
	return -EPERM;
}

int cam_vfe_top_init_hw(void *device_priv,
	void *init_hw_args, uint32_t arg_size)
{
	struct cam_vfe_top_ver2_priv   *top_priv = device_priv;

	top_priv->hw_clk_rate = 0;

	return 0;
}

int cam_vfe_top_reset(void *device_priv,
	void *reset_core_args, uint32_t arg_size)
{
	struct cam_vfe_top_ver2_priv   *top_priv = device_priv;
	struct cam_hw_soc_info         *soc_info = NULL;
	struct cam_vfe_top_ver2_reg_offset_common *reg_common = NULL;
	uint32_t *reset_reg_args = reset_core_args;
	uint32_t reset_reg_val;

	if (!top_priv || !reset_reg_args) {
		CAM_ERR(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}

	switch (*reset_reg_args) {
	case CAM_VFE_HW_RESET_HW_AND_REG:
		reset_reg_val = CAM_VFE_HW_RESET_HW_AND_REG_VAL;
		break;
	default:
		reset_reg_val = CAM_VFE_HW_RESET_HW_VAL;
		break;
	}

	CAM_DBG(CAM_ISP, "reset reg value: %x", reset_reg_val);
	soc_info = top_priv->common_data.soc_info;
	reg_common = top_priv->common_data.common_reg;

	/* Mask All the IRQs except RESET */
	cam_io_w_mb((1 << 31),
		CAM_SOC_GET_REG_MAP_START(soc_info, VFE_CORE_BASE_IDX) + 0x5C);

	/* Reset HW */
	cam_io_w_mb(reset_reg_val,
		CAM_SOC_GET_REG_MAP_START(soc_info, VFE_CORE_BASE_IDX) +
		reg_common->global_reset_cmd);

	CAM_DBG(CAM_ISP, "Reset HW exit");
	return 0;
}

int cam_vfe_top_reserve(void *device_priv,
	void *reserve_args, uint32_t arg_size)
{
	struct cam_vfe_top_ver2_priv            *top_priv;
	struct cam_vfe_acquire_args             *args;
	struct cam_vfe_hw_vfe_in_acquire_args   *acquire_args;
	uint32_t i;
	int rc = -EINVAL;

	if (!device_priv || !reserve_args) {
		CAM_ERR(CAM_ISP, "Error! Invalid input arguments");
		return -EINVAL;
	}

	top_priv = (struct cam_vfe_top_ver2_priv   *)device_priv;
	args = (struct cam_vfe_acquire_args *)reserve_args;
	acquire_args = &args->vfe_in;


	for (i = 0; i < CAM_VFE_TOP_VER2_MUX_MAX; i++) {
		if (top_priv->mux_rsrc[i].res_id ==  acquire_args->res_id &&
			top_priv->mux_rsrc[i].res_state ==
			CAM_ISP_RESOURCE_STATE_AVAILABLE) {

			if (acquire_args->res_id == CAM_ISP_HW_VFE_IN_CAMIF) {
				rc = cam_vfe_camif_ver2_acquire_resource(
					&top_priv->mux_rsrc[i],
					args);
				if (rc)
					break;
			}

			if (acquire_args->res_id ==
				CAM_ISP_HW_VFE_IN_CAMIF_LITE) {
				rc = cam_vfe_camif_lite_ver2_acquire_resource(
					&top_priv->mux_rsrc[i],
					args);
				if (rc)
					break;
			}

			top_priv->mux_rsrc[i].cdm_ops = acquire_args->cdm_ops;
			top_priv->mux_rsrc[i].tasklet_info = args->tasklet;
			top_priv->mux_rsrc[i].res_state =
				CAM_ISP_RESOURCE_STATE_RESERVED;
			acquire_args->rsrc_node =
				&top_priv->mux_rsrc[i];

			rc = 0;
			break;
		}
	}

	return rc;

}

int cam_vfe_top_release(void *device_priv,
	void *release_args, uint32_t arg_size)
{
	struct cam_vfe_top_ver2_priv            *top_priv;
	struct cam_isp_resource_node            *mux_res;

	if (!device_priv || !release_args) {
		CAM_ERR(CAM_ISP, "Error! Invalid input arguments");
		return -EINVAL;
	}

	top_priv = (struct cam_vfe_top_ver2_priv   *)device_priv;
	mux_res = (struct cam_isp_resource_node *)release_args;

	CAM_DBG(CAM_ISP, "Resource in state %d", mux_res->res_state);
	if (mux_res->res_state < CAM_ISP_RESOURCE_STATE_RESERVED) {
		CAM_ERR(CAM_ISP, "Error! Resource in Invalid res_state :%d",
			mux_res->res_state);
		return -EINVAL;
	}
	mux_res->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;

	return 0;
}

int cam_vfe_top_start(void *device_priv,
	void *start_args, uint32_t arg_size)
{
	struct cam_vfe_top_ver2_priv            *top_priv;
	struct cam_isp_resource_node            *mux_res;
	struct cam_hw_info                      *hw_info = NULL;
	int rc = 0;

	if (!device_priv || !start_args) {
		CAM_ERR(CAM_ISP, "Error! Invalid input arguments");
		return -EINVAL;
	}

	top_priv = (struct cam_vfe_top_ver2_priv *)device_priv;
	mux_res = (struct cam_isp_resource_node *)start_args;
	hw_info = (struct cam_hw_info  *)mux_res->hw_intf->hw_priv;

	if (hw_info->hw_state == CAM_HW_STATE_POWER_UP) {
		rc = cam_vfe_top_set_hw_clk_rate(top_priv);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"set_hw_clk_rate failed, rc=%d", rc);
			return rc;
		}

		rc = cam_vfe_top_set_axi_bw_vote(top_priv, true);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"set_axi_bw_vote failed, rc=%d", rc);
			return rc;
		}

		if (mux_res->start) {
			rc = mux_res->start(mux_res);
		} else {
			CAM_ERR(CAM_ISP,
				"Invalid res id:%d", mux_res->res_id);
			rc = -EINVAL;
		}
	} else {
		CAM_ERR(CAM_ISP, "VFE HW not powered up");
		rc = -EPERM;
	}

	return rc;
}

int cam_vfe_top_stop(void *device_priv,
	void *stop_args, uint32_t arg_size)
{
	struct cam_vfe_top_ver2_priv            *top_priv;
	struct cam_isp_resource_node            *mux_res;
	struct cam_hw_info                      *hw_info = NULL;
	int i, rc = 0;

	if (!device_priv || !stop_args) {
		CAM_ERR(CAM_ISP, "Error! Invalid input arguments");
		return -EINVAL;
	}

	top_priv = (struct cam_vfe_top_ver2_priv   *)device_priv;
	mux_res = (struct cam_isp_resource_node *)stop_args;
	hw_info = (struct cam_hw_info  *)mux_res->hw_intf->hw_priv;

	if ((mux_res->res_id == CAM_ISP_HW_VFE_IN_CAMIF) ||
		(mux_res->res_id == CAM_ISP_HW_VFE_IN_CAMIF_LITE) ||
		((mux_res->res_id >= CAM_ISP_HW_VFE_IN_RDI0) &&
		(mux_res->res_id <= CAM_ISP_HW_VFE_IN_RDI3))) {
		rc = mux_res->stop(mux_res);
	} else {
		CAM_ERR(CAM_ISP, "Invalid res id:%d", mux_res->res_id);
		return -EINVAL;
	}

	if (!rc) {
		for (i = 0; i < CAM_VFE_TOP_VER2_MUX_MAX; i++) {
			if (top_priv->mux_rsrc[i].res_id == mux_res->res_id) {
				top_priv->req_clk_rate[i] = 0;
				top_priv->req_axi_vote[i].compressed_bw = 0;
				top_priv->req_axi_vote[i].uncompressed_bw = 0;
				top_priv->axi_vote_control[i] =
					CAM_VFE_BW_CONTROL_EXCLUDE;
				break;
			}
		}
	}

	return rc;
}

int cam_vfe_top_read(void *device_priv,
	void *read_args, uint32_t arg_size)
{
	return -EPERM;
}

int cam_vfe_top_write(void *device_priv,
	void *write_args, uint32_t arg_size)
{
	return -EPERM;
}

int cam_vfe_top_process_cmd(void *device_priv, uint32_t cmd_type,
	void *cmd_args, uint32_t arg_size)
{
	int rc = 0;
	struct cam_vfe_top_ver2_priv            *top_priv;

	if (!device_priv || !cmd_args) {
		CAM_ERR(CAM_ISP, "Error! Invalid arguments");
		return -EINVAL;
	}
	top_priv = (struct cam_vfe_top_ver2_priv *)device_priv;

	switch (cmd_type) {
	case CAM_ISP_HW_CMD_GET_CHANGE_BASE:
		rc = cam_vfe_top_mux_get_base(top_priv, cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_GET_REG_UPDATE:
		rc = cam_vfe_top_mux_get_reg_update(top_priv, cmd_args,
			arg_size);
		break;
	case CAM_ISP_HW_CMD_CLOCK_UPDATE:
		rc = cam_vfe_top_clock_update(top_priv, cmd_args,
			arg_size);
		break;
	case CAM_ISP_HW_CMD_BW_UPDATE:
		rc = cam_vfe_top_bw_update(top_priv, cmd_args,
			arg_size);
		break;
	case CAM_ISP_HW_CMD_BW_CONTROL:
		rc = cam_vfe_top_bw_control(top_priv, cmd_args, arg_size);
		break;
	default:
		rc = -EINVAL;
		CAM_ERR(CAM_ISP, "Error! Invalid cmd:%d", cmd_type);
		break;
	}

	return rc;
}

int cam_vfe_top_ver2_init(
	struct cam_hw_soc_info                 *soc_info,
	struct cam_hw_intf                     *hw_intf,
	void                                   *top_hw_info,
	struct cam_vfe_top                    **vfe_top_ptr)
{
	int i, j, rc = 0;
	struct cam_vfe_top_ver2_priv           *top_priv = NULL;
	struct cam_vfe_top_ver2_hw_info        *ver2_hw_info = top_hw_info;
	struct cam_vfe_top                     *vfe_top;

	vfe_top = kzalloc(sizeof(struct cam_vfe_top), GFP_KERNEL);
	if (!vfe_top) {
		CAM_DBG(CAM_ISP, "Error! Failed to alloc for vfe_top");
		rc = -ENOMEM;
		goto end;
	}

	top_priv = kzalloc(sizeof(struct cam_vfe_top_ver2_priv),
		GFP_KERNEL);
	if (!top_priv) {
		CAM_DBG(CAM_ISP, "Error! Failed to alloc for vfe_top_priv");
		rc = -ENOMEM;
		goto free_vfe_top;
	}
	vfe_top->top_priv = top_priv;
	top_priv->hw_clk_rate = 0;
	top_priv->applied_axi_vote.compressed_bw = 0;
	top_priv->applied_axi_vote.uncompressed_bw = 0;
	memset(top_priv->last_vote, 0x0, sizeof(struct cam_axi_vote) *
		(CAM_VFE_TOP_VER2_MUX_MAX *
		CAM_VFE_DELAY_BW_REDUCTION_NUM_FRAMES));
	top_priv->last_counter = 0;

	for (i = 0, j = 0; i < CAM_VFE_TOP_VER2_MUX_MAX; i++) {
		top_priv->mux_rsrc[i].res_type = CAM_ISP_RESOURCE_VFE_IN;
		top_priv->mux_rsrc[i].hw_intf = hw_intf;
		top_priv->mux_rsrc[i].res_state =
			CAM_ISP_RESOURCE_STATE_AVAILABLE;
		top_priv->req_clk_rate[i] = 0;
		top_priv->req_axi_vote[i].compressed_bw = 0;
		top_priv->req_axi_vote[i].uncompressed_bw = 0;
		top_priv->axi_vote_control[i] = CAM_VFE_BW_CONTROL_EXCLUDE;


		if (ver2_hw_info->mux_type[i] == CAM_VFE_CAMIF_VER_2_0) {
			top_priv->mux_rsrc[i].res_id =
				CAM_ISP_HW_VFE_IN_CAMIF;

			rc = cam_vfe_camif_ver2_init(hw_intf, soc_info,
				&ver2_hw_info->camif_hw_info,
				&top_priv->mux_rsrc[i]);
			if (rc)
				goto deinit_resources;
		} else if (ver2_hw_info->mux_type[i] ==
			CAM_VFE_CAMIF_LITE_VER_2_0) {
			top_priv->mux_rsrc[i].res_id =
				CAM_ISP_HW_VFE_IN_CAMIF_LITE;

			rc = cam_vfe_camif_lite_ver2_init(hw_intf, soc_info,
				&ver2_hw_info->camif_lite_hw_info,
				&top_priv->mux_rsrc[i]);

			if (rc)
				goto deinit_resources;
		} else if (ver2_hw_info->mux_type[i] ==
			CAM_VFE_RDI_VER_1_0) {
			/* set the RDI resource id */
			top_priv->mux_rsrc[i].res_id =
				CAM_ISP_HW_VFE_IN_RDI0 + j++;

			rc = cam_vfe_rdi_ver2_init(hw_intf, soc_info,
				&ver2_hw_info->rdi_hw_info,
				&top_priv->mux_rsrc[i]);
			if (rc)
				goto deinit_resources;
		} else {
			CAM_WARN(CAM_ISP, "Invalid mux type: %u",
				ver2_hw_info->mux_type[i]);
		}
	}

	vfe_top->hw_ops.get_hw_caps = cam_vfe_top_get_hw_caps;
	vfe_top->hw_ops.init        = cam_vfe_top_init_hw;
	vfe_top->hw_ops.reset       = cam_vfe_top_reset;
	vfe_top->hw_ops.reserve     = cam_vfe_top_reserve;
	vfe_top->hw_ops.release     = cam_vfe_top_release;
	vfe_top->hw_ops.start       = cam_vfe_top_start;
	vfe_top->hw_ops.stop        = cam_vfe_top_stop;
	vfe_top->hw_ops.read        = cam_vfe_top_read;
	vfe_top->hw_ops.write       = cam_vfe_top_write;
	vfe_top->hw_ops.process_cmd = cam_vfe_top_process_cmd;
	*vfe_top_ptr = vfe_top;

	top_priv->common_data.soc_info     = soc_info;
	top_priv->common_data.hw_intf      = hw_intf;
	top_priv->common_data.common_reg   = ver2_hw_info->common_reg;

	return rc;

deinit_resources:
	for (--i; i >= 0; i--) {
		if (ver2_hw_info->mux_type[i] == CAM_VFE_CAMIF_VER_2_0) {
			if (cam_vfe_camif_ver2_deinit(&top_priv->mux_rsrc[i]))
				CAM_ERR(CAM_ISP, "Camif Deinit failed");
		} else if (ver2_hw_info->mux_type[i] ==
			CAM_VFE_CAMIF_LITE_VER_2_0) {
			if (cam_vfe_camif_lite_ver2_deinit(
				&top_priv->mux_rsrc[i]))
				CAM_ERR(CAM_ISP, "Camif lite deinit failed");
		} else {
			if (cam_vfe_rdi_ver2_deinit(&top_priv->mux_rsrc[i]))
				CAM_ERR(CAM_ISP, "RDI Deinit failed");
		}
		top_priv->mux_rsrc[i].res_state =
			CAM_ISP_RESOURCE_STATE_UNAVAILABLE;
	}

	kfree(vfe_top->top_priv);
free_vfe_top:
	kfree(vfe_top);
end:
	return rc;
}

int cam_vfe_top_ver2_deinit(struct cam_vfe_top  **vfe_top_ptr)
{
	int i, rc = 0;
	struct cam_vfe_top_ver2_priv           *top_priv = NULL;
	struct cam_vfe_top                     *vfe_top;

	if (!vfe_top_ptr) {
		CAM_ERR(CAM_ISP, "Error! Invalid input");
		return -EINVAL;
	}

	vfe_top = *vfe_top_ptr;
	if (!vfe_top) {
		CAM_ERR(CAM_ISP, "Error! vfe_top NULL");
		return -ENODEV;
	}

	top_priv = vfe_top->top_priv;
	if (!top_priv) {
		CAM_ERR(CAM_ISP, "Error! vfe_top_priv NULL");
		rc = -ENODEV;
		goto free_vfe_top;
	}

	for (i = 0; i < CAM_VFE_TOP_VER2_MUX_MAX; i++) {
		top_priv->mux_rsrc[i].res_state =
			CAM_ISP_RESOURCE_STATE_UNAVAILABLE;
		if (top_priv->mux_rsrc[i].res_type ==
			CAM_VFE_CAMIF_VER_2_0) {
			rc = cam_vfe_camif_ver2_deinit(&top_priv->mux_rsrc[i]);
			if (rc)
				CAM_ERR(CAM_ISP, "Camif deinit failed rc=%d",
					rc);
		} else if (top_priv->mux_rsrc[i].res_type ==
			CAM_VFE_CAMIF_LITE_VER_2_0) {
			rc = cam_vfe_camif_lite_ver2_deinit(
				&top_priv->mux_rsrc[i]);
			if (rc)
				CAM_ERR(CAM_ISP,
					"Camif lite deinit failed rc=%d", rc);
		} else if (top_priv->mux_rsrc[i].res_type ==
			CAM_VFE_RDI_VER_1_0) {
			rc = cam_vfe_rdi_ver2_deinit(&top_priv->mux_rsrc[i]);
			if (rc)
				CAM_ERR(CAM_ISP, "RDI deinit failed rc=%d", rc);
		}
	}

	kfree(vfe_top->top_priv);

free_vfe_top:
	kfree(vfe_top);
	*vfe_top_ptr = NULL;

	return rc;
}

