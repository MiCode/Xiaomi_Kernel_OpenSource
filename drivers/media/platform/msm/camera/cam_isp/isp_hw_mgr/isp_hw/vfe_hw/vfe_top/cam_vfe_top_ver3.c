// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#include <linux/slab.h>
#include "cam_io_util.h"
#include "cam_cdm_util.h"
#include "cam_vfe_hw_intf.h"
#include "cam_vfe_top.h"
#include "cam_vfe_top_ver3.h"
#include "cam_debug_util.h"
#include "cam_cpas_api.h"
#include "cam_vfe_soc.h"

#define CAM_VFE_HW_RESET_HW_AND_REG_VAL       0x00000003
#define CAM_VFE_HW_RESET_HW_VAL               0x007F0000
#define CAM_VFE_LITE_HW_RESET_AND_REG_VAL     0x00000002
#define CAM_VFE_LITE_HW_RESET_HW_VAL          0x0000003D
#define CAM_VFE_DELAY_BW_REDUCTION_NUM_FRAMES 3

struct cam_vfe_top_ver3_common_data {
	struct cam_hw_soc_info                     *soc_info;
	struct cam_hw_intf                         *hw_intf;
	struct cam_vfe_top_ver3_reg_offset_common  *common_reg;
};

struct cam_vfe_top_ver3_priv {
	struct cam_vfe_top_ver3_common_data common_data;
	struct cam_isp_resource_node        mux_rsrc[CAM_VFE_TOP_VER3_MUX_MAX];
	unsigned long                       hw_clk_rate;
	struct cam_axi_vote                 applied_axi_vote;
	struct cam_axi_vote             req_axi_vote[CAM_VFE_TOP_VER3_MUX_MAX];
	unsigned long                   req_clk_rate[CAM_VFE_TOP_VER3_MUX_MAX];
	struct cam_axi_vote             last_vote[CAM_VFE_TOP_VER3_MUX_MAX *
					CAM_VFE_DELAY_BW_REDUCTION_NUM_FRAMES];
	uint32_t                        last_counter;
	uint64_t                        total_bw_applied;
	enum cam_vfe_bw_control_action
		axi_vote_control[CAM_VFE_TOP_VER3_MUX_MAX];
};

static int cam_vfe_top_ver3_mux_get_base(struct cam_vfe_top_ver3_priv *top_priv,
	void *cmd_args, uint32_t arg_size)
{
	uint32_t                          size = 0;
	uint32_t                          mem_base = 0;
	struct cam_isp_hw_get_cmd_update *cdm_args  = cmd_args;
	struct cam_cdm_utils_ops         *cdm_util_ops = NULL;

	if (arg_size != sizeof(struct cam_isp_hw_get_cmd_update)) {
		CAM_ERR(CAM_ISP, "Error, Invalid cmd size");
		return -EINVAL;
	}

	if (!cdm_args || !cdm_args->res || !top_priv ||
		!top_priv->common_data.soc_info) {
		CAM_ERR(CAM_ISP, "Error, Invalid args");
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

static int cam_vfe_top_ver3_set_hw_clk_rate(
	struct cam_vfe_top_ver3_priv *top_priv)
{
	struct cam_hw_soc_info        *soc_info = NULL;
	int                            i, rc = 0;
	unsigned long                  max_clk_rate = 0;

	soc_info = top_priv->common_data.soc_info;

	for (i = 0; i < CAM_VFE_TOP_VER3_MUX_MAX; i++) {
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

static struct cam_axi_vote *cam_vfe_top_delay_bw_reduction(
	struct cam_vfe_top_ver3_priv *top_priv,
	uint64_t *to_be_applied_bw)
{
	uint32_t i, j;
	int vote_idx = -1;
	uint64_t max_bw = 0;
	uint64_t total_bw;
	struct cam_axi_vote *curr_l_vote;

	for (i = 0; i < (CAM_VFE_TOP_VER3_MUX_MAX *
		CAM_VFE_DELAY_BW_REDUCTION_NUM_FRAMES); i++) {
		total_bw = 0;
		curr_l_vote = &top_priv->last_vote[i];
		for (j = 0; j < curr_l_vote->num_paths; j++) {
			if (total_bw >
				(U64_MAX -
				curr_l_vote->axi_path[j].camnoc_bw)) {
				CAM_ERR(CAM_ISP, "Overflow at idx: %d", j);
				return NULL;
			}

			total_bw += curr_l_vote->axi_path[j].camnoc_bw;
		}

		if (total_bw > max_bw) {
			vote_idx = i;
			max_bw = total_bw;
		}
	}

	if (vote_idx < 0)
		return NULL;

	*to_be_applied_bw = max_bw;

	return &top_priv->last_vote[vote_idx];
}

static int cam_vfe_top_ver3_set_axi_bw_vote(
	struct cam_vfe_top_ver3_priv *top_priv,
	bool start_stop)
{
	struct cam_axi_vote agg_vote = {0};
	struct cam_axi_vote *to_be_applied_axi_vote = NULL;
	int rc = 0;
	uint32_t i;
	uint32_t num_paths = 0;
	uint64_t total_bw_new_vote = 0;
	bool bw_unchanged = true;
	struct cam_hw_soc_info   *soc_info =
		top_priv->common_data.soc_info;
	struct cam_vfe_soc_private *soc_private =
		soc_info->soc_private;
	bool apply_bw_update = false;

	if (!soc_private) {
		CAM_ERR(CAM_ISP, "Error soc_private NULL");
		return -EINVAL;
	}

	for (i = 0; i < CAM_VFE_TOP_VER3_MUX_MAX; i++) {
		if (top_priv->axi_vote_control[i] ==
			CAM_VFE_BW_CONTROL_INCLUDE) {
			if (num_paths +
				top_priv->req_axi_vote[i].num_paths >
				CAM_CPAS_MAX_PATHS_PER_CLIENT) {
				CAM_ERR(CAM_ISP,
					"Required paths(%d) more than max(%d)",
					num_paths +
					top_priv->req_axi_vote[i].num_paths,
					CAM_CPAS_MAX_PATHS_PER_CLIENT);
				return -EINVAL;
			}

			memcpy(&agg_vote.axi_path[num_paths],
				&top_priv->req_axi_vote[i].axi_path[0],
				top_priv->req_axi_vote[i].num_paths *
				sizeof(
				struct cam_axi_per_path_bw_vote));
			num_paths += top_priv->req_axi_vote[i].num_paths;
		}
	}

	agg_vote.num_paths = num_paths;

	for (i = 0; i < agg_vote.num_paths; i++) {
		CAM_DBG(CAM_PERF,
			"ife[%d] : New BW Vote : counter[%d] [%s][%s] [%llu %llu %llu]",
			top_priv->common_data.hw_intf->hw_idx,
			top_priv->last_counter,
			cam_cpas_axi_util_path_type_to_string(
			agg_vote.axi_path[i].path_data_type),
			cam_cpas_axi_util_trans_type_to_string(
			agg_vote.axi_path[i].transac_type),
			agg_vote.axi_path[i].camnoc_bw,
			agg_vote.axi_path[i].mnoc_ab_bw,
			agg_vote.axi_path[i].mnoc_ib_bw);

		total_bw_new_vote += agg_vote.axi_path[i].camnoc_bw;
	}

	memcpy(&top_priv->last_vote[top_priv->last_counter], &agg_vote,
		sizeof(struct cam_axi_vote));
	top_priv->last_counter = (top_priv->last_counter + 1) %
		(CAM_VFE_TOP_VER3_MUX_MAX *
		CAM_VFE_DELAY_BW_REDUCTION_NUM_FRAMES);

	if ((agg_vote.num_paths != top_priv->applied_axi_vote.num_paths) ||
		(total_bw_new_vote != top_priv->total_bw_applied))
		bw_unchanged = false;

	CAM_DBG(CAM_PERF,
		"ife[%d] : applied_total=%lld, new_total=%lld unchanged=%d, start_stop=%d",
		top_priv->common_data.hw_intf->hw_idx,
		top_priv->total_bw_applied, total_bw_new_vote,
		bw_unchanged, start_stop);

	if (bw_unchanged) {
		CAM_DBG(CAM_ISP, "BW config unchanged");
		return 0;
	}

	if (start_stop) {
		/* need to vote current request immediately */
		to_be_applied_axi_vote = &agg_vote;
		/* Reset everything, we can start afresh */
		memset(top_priv->last_vote, 0x0, sizeof(struct cam_axi_vote) *
			(CAM_VFE_TOP_VER3_MUX_MAX *
			CAM_VFE_DELAY_BW_REDUCTION_NUM_FRAMES));
		top_priv->last_counter = 0;
		top_priv->last_vote[top_priv->last_counter] = agg_vote;
		top_priv->last_counter = (top_priv->last_counter + 1) %
			(CAM_VFE_TOP_VER3_MUX_MAX *
			CAM_VFE_DELAY_BW_REDUCTION_NUM_FRAMES);
	} else {
		/*
		 * Find max bw request in last few frames. This will the bw
		 * that we want to vote to CPAS now.
		 */
		to_be_applied_axi_vote =
			cam_vfe_top_delay_bw_reduction(top_priv,
			&total_bw_new_vote);
		if (!to_be_applied_axi_vote) {
			CAM_ERR(CAM_ISP, "to_be_applied_axi_vote is NULL");
			return -EINVAL;
		}
	}

	for (i = 0; i < to_be_applied_axi_vote->num_paths; i++) {
		CAM_DBG(CAM_PERF,
			"ife[%d] : Apply BW Vote : [%s][%s] [%llu %llu %llu]",
			top_priv->common_data.hw_intf->hw_idx,
			cam_cpas_axi_util_path_type_to_string(
			to_be_applied_axi_vote->axi_path[i].path_data_type),
			cam_cpas_axi_util_trans_type_to_string(
			to_be_applied_axi_vote->axi_path[i].transac_type),
			to_be_applied_axi_vote->axi_path[i].camnoc_bw,
			to_be_applied_axi_vote->axi_path[i].mnoc_ab_bw,
			to_be_applied_axi_vote->axi_path[i].mnoc_ib_bw);
	}

	if ((to_be_applied_axi_vote->num_paths !=
		top_priv->applied_axi_vote.num_paths) ||
		(total_bw_new_vote != top_priv->total_bw_applied))
		apply_bw_update = true;

	CAM_DBG(CAM_PERF,
		"ife[%d] : Delayed update: applied_total=%lld, new_total=%lld apply_bw_update=%d, start_stop=%d",
		top_priv->common_data.hw_intf->hw_idx,
		top_priv->total_bw_applied, total_bw_new_vote,
		apply_bw_update, start_stop);

	if (apply_bw_update) {
		rc = cam_cpas_update_axi_vote(soc_private->cpas_handle,
			to_be_applied_axi_vote);
		if (!rc) {
			memcpy(&top_priv->applied_axi_vote,
				to_be_applied_axi_vote,
				sizeof(struct cam_axi_vote));
			top_priv->total_bw_applied = total_bw_new_vote;
		} else {
			CAM_ERR(CAM_ISP, "BW request failed, rc=%d", rc);
		}
	}

	return rc;
}

static int cam_vfe_top_fs_update(
	struct cam_vfe_top_ver3_priv *top_priv,
	void *cmd_args, uint32_t arg_size)
{
	struct cam_vfe_fe_update_args *cmd_update = cmd_args;

	if (cmd_update->node_res->process_cmd)
		return cmd_update->node_res->process_cmd(cmd_update->node_res,
			CAM_ISP_HW_CMD_FE_UPDATE_IN_RD, cmd_args, arg_size);

	return 0;
}

static int cam_vfe_top_ver3_clock_update(
	struct cam_vfe_top_ver3_priv *top_priv,
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

	for (i = 0; i < CAM_VFE_TOP_VER3_MUX_MAX; i++) {
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
		rc = cam_vfe_top_ver3_set_hw_clk_rate(top_priv);

	return rc;
}

static int cam_vfe_top_ver3_bw_update_v2(
	struct cam_vfe_top_ver3_priv *top_priv,
	void *cmd_args, uint32_t arg_size)
{
	struct cam_vfe_bw_update_args_v2        *bw_update = NULL;
	struct cam_isp_resource_node         *res = NULL;
	struct cam_hw_info                   *hw_info = NULL;
	int                                   rc = 0;
	int                                   i;

	bw_update = (struct cam_vfe_bw_update_args_v2 *)cmd_args;
	res = bw_update->node_res;

	if (!res || !res->hw_intf || !res->hw_intf->hw_priv)
		return -EINVAL;

	hw_info = res->hw_intf->hw_priv;

	if (res->res_type != CAM_ISP_RESOURCE_VFE_IN ||
		res->res_id >= CAM_ISP_HW_VFE_IN_MAX) {
		CAM_ERR(CAM_ISP, "VFE:%d Invalid res_type:%d res id%d",
			res->hw_intf->hw_idx, res->res_type,
			res->res_id);
		return -EINVAL;
	}

	for (i = 0; i < CAM_VFE_TOP_VER3_MUX_MAX; i++) {
		if (top_priv->mux_rsrc[i].res_id == res->res_id) {
			memcpy(&top_priv->req_axi_vote[i], &bw_update->isp_vote,
				sizeof(struct cam_axi_vote));
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
	} else {
		rc = cam_vfe_top_ver3_set_axi_bw_vote(top_priv, false);
	}

	return rc;
}

static int cam_vfe_top_ver3_bw_update(
	struct cam_vfe_top_ver3_priv *top_priv,
	void *cmd_args, uint32_t arg_size)
{
	struct cam_vfe_bw_update_args        *bw_update = NULL;
	struct cam_isp_resource_node         *res = NULL;
	struct cam_hw_info                   *hw_info = NULL;
	int                                   rc = 0;
	int                                   i;
	struct cam_axi_vote                  *mux_axi_vote;
	bool                                  vid_exists = false;
	bool                                  rdi_exists = false;

	bw_update = (struct cam_vfe_bw_update_args *)cmd_args;
	res = bw_update->node_res;

	if (!res || !res->hw_intf || !res->hw_intf->hw_priv)
		return -EINVAL;

	hw_info = res->hw_intf->hw_priv;

	CAM_DBG(CAM_ISP, "res_id=%d, BW=[%lld %lld]",
		res->res_id, bw_update->camnoc_bw_bytes,
		bw_update->external_bw_bytes);

	if (res->res_type != CAM_ISP_RESOURCE_VFE_IN ||
		res->res_id >= CAM_ISP_HW_VFE_IN_MAX) {
		CAM_ERR(CAM_ISP, "VFE:%d Invalid res_type:%d res id%d",
			res->hw_intf->hw_idx, res->res_type,
			res->res_id);
		return -EINVAL;
	}

	for (i = 0; i < CAM_VFE_TOP_VER3_MUX_MAX; i++) {
		mux_axi_vote = &top_priv->req_axi_vote[i];
		if (top_priv->mux_rsrc[i].res_id == res->res_id) {
			mux_axi_vote->num_paths = 1;
			if ((res->res_id >= CAM_ISP_HW_VFE_IN_RDI0) &&
				(res->res_id <= CAM_ISP_HW_VFE_IN_RDI3)) {
				mux_axi_vote->axi_path[0].path_data_type =
					CAM_AXI_PATH_DATA_IFE_RDI0 +
					(res->res_id - CAM_ISP_HW_VFE_IN_RDI0);
			} else {
				/*
				 * Vote all bw into VIDEO path as we cannot
				 * differentiate to which path this has to go
				 */
				mux_axi_vote->axi_path[0].path_data_type =
					CAM_AXI_PATH_DATA_IFE_VID;
			}

			mux_axi_vote->axi_path[0].transac_type =
				CAM_AXI_TRANSACTION_WRITE;
			mux_axi_vote->axi_path[0].camnoc_bw =
				bw_update->camnoc_bw_bytes;
			mux_axi_vote->axi_path[0].mnoc_ab_bw =
				bw_update->external_bw_bytes;
			mux_axi_vote->axi_path[0].mnoc_ib_bw =
				bw_update->external_bw_bytes;
			/* Make ddr bw same as mnoc bw */
			mux_axi_vote->axi_path[0].ddr_ab_bw =
				bw_update->external_bw_bytes;
			mux_axi_vote->axi_path[0].ddr_ib_bw =
				bw_update->external_bw_bytes;

			top_priv->axi_vote_control[i] =
				CAM_VFE_BW_CONTROL_INCLUDE;
			break;
		}

		if (mux_axi_vote->num_paths == 1) {
			if (mux_axi_vote->axi_path[0].path_data_type ==
				CAM_AXI_PATH_DATA_IFE_VID)
				vid_exists = true;
			else if ((mux_axi_vote->axi_path[0].path_data_type >=
				CAM_AXI_PATH_DATA_IFE_RDI0) &&
				(mux_axi_vote->axi_path[0].path_data_type <=
				CAM_AXI_PATH_DATA_IFE_RDI3))
				rdi_exists = true;
		}
	}

	if (hw_info->hw_state != CAM_HW_STATE_POWER_UP) {
		CAM_ERR_RATE_LIMIT(CAM_ISP,
			"VFE:%d Not ready to set BW yet :%d",
			res->hw_intf->hw_idx,
			hw_info->hw_state);
	} else {
		rc = cam_vfe_top_ver3_set_axi_bw_vote(top_priv, false);
	}

	return rc;
}

static int cam_vfe_core_config_control(
	struct cam_vfe_top_ver3_priv *top_priv,
	 void *cmd_args, uint32_t arg_size)
{
	struct cam_vfe_core_config_args  *core_config = cmd_args;

	if (core_config->node_res->process_cmd)
		return core_config->node_res->process_cmd(core_config->node_res,
			CAM_ISP_HW_CMD_CORE_CONFIG, cmd_args, arg_size);

	return -EINVAL;
}

static int cam_vfe_top_ver3_bw_control(
	struct cam_vfe_top_ver3_priv *top_priv,
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

	for (i = 0; i < CAM_VFE_TOP_VER3_MUX_MAX; i++) {
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
		rc = cam_vfe_top_ver3_set_axi_bw_vote(top_priv, true);
	}

	return rc;
}

static int cam_vfe_top_ver3_mux_get_reg_update(
	struct cam_vfe_top_ver3_priv *top_priv,
	void *cmd_args, uint32_t arg_size)
{
	struct cam_isp_hw_get_cmd_update  *cmd_update = cmd_args;

	if (cmd_update->res->process_cmd)
		return cmd_update->res->process_cmd(cmd_update->res,
			CAM_ISP_HW_CMD_GET_REG_UPDATE, cmd_args, arg_size);

	return -EINVAL;
}

int cam_vfe_top_ver3_get_hw_caps(void *device_priv,
	void *get_hw_cap_args, uint32_t arg_size)
{
	return -EPERM;
}

int cam_vfe_top_ver3_init_hw(void *device_priv,
	void *init_hw_args, uint32_t arg_size)
{
	struct cam_vfe_top_ver3_priv   *top_priv = device_priv;

	top_priv->hw_clk_rate = 0;

	return 0;
}

int cam_vfe_top_ver3_reset(void *device_priv,
	void *reset_core_args, uint32_t arg_size)
{
	struct cam_vfe_top_ver3_priv   *top_priv = device_priv;
	struct cam_hw_soc_info         *soc_info = NULL;
	struct cam_vfe_soc_private     *soc_private = NULL;
	struct cam_vfe_top_ver3_reg_offset_common *reg_common = NULL;
	uint32_t *reset_reg_args = reset_core_args;
	uint32_t reset_reg_val;

	if (!top_priv || !reset_reg_args) {
		CAM_ERR(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}

	soc_info = top_priv->common_data.soc_info;
	reg_common = top_priv->common_data.common_reg;

	soc_private = soc_info->soc_private;
	if (!soc_private) {
		CAM_ERR(CAM_ISP, "Invalid soc_private");
		return -ENODEV;
	}

	switch (*reset_reg_args) {
	case CAM_VFE_HW_RESET_HW_AND_REG:
		if (!soc_private->is_ife_lite)
			reset_reg_val = CAM_VFE_HW_RESET_HW_AND_REG_VAL;
		else
			reset_reg_val = CAM_VFE_LITE_HW_RESET_AND_REG_VAL;
		break;
	default:
		if (!soc_private->is_ife_lite)
			reset_reg_val = CAM_VFE_HW_RESET_HW_VAL;
		else
			reset_reg_val = CAM_VFE_LITE_HW_RESET_HW_VAL;
		break;
	}
	/* override due to hw limitation */
	if (!soc_private->is_ife_lite)
		reset_reg_val = CAM_VFE_HW_RESET_HW_AND_REG_VAL;
	else
		reset_reg_val = CAM_VFE_LITE_HW_RESET_AND_REG_VAL;

	CAM_DBG(CAM_ISP, "reset reg value: 0x%x", reset_reg_val);

	/* Mask All the IRQs except RESET */
	if (!soc_private->is_ife_lite)
		cam_io_w_mb(0x00000001,
			CAM_SOC_GET_REG_MAP_START(soc_info, VFE_CORE_BASE_IDX)
			+ 0x3C);
	else
		cam_io_w_mb(0x00020000,
			CAM_SOC_GET_REG_MAP_START(soc_info, VFE_CORE_BASE_IDX)
			+ 0x28);

	/* Reset HW */
	cam_io_w_mb(reset_reg_val,
		CAM_SOC_GET_REG_MAP_START(soc_info, VFE_CORE_BASE_IDX) +
		reg_common->global_reset_cmd);

	CAM_DBG(CAM_ISP, "Reset HW exit");
	return 0;
}

int cam_vfe_top_ver3_reserve(void *device_priv,
	void *reserve_args, uint32_t arg_size)
{
	struct cam_vfe_top_ver3_priv            *top_priv;
	struct cam_vfe_acquire_args             *args;
	struct cam_vfe_hw_vfe_in_acquire_args   *acquire_args;
	uint32_t i;
	int rc = -EINVAL;

	if (!device_priv || !reserve_args) {
		CAM_ERR(CAM_ISP, "Error, Invalid input arguments");
		return -EINVAL;
	}

	top_priv = (struct cam_vfe_top_ver3_priv   *)device_priv;
	args = (struct cam_vfe_acquire_args *)reserve_args;
	acquire_args = &args->vfe_in;

	CAM_DBG(CAM_ISP, "res id %d", acquire_args->res_id);


	for (i = 0; i < CAM_VFE_TOP_VER3_MUX_MAX; i++) {
		if (top_priv->mux_rsrc[i].res_id ==  acquire_args->res_id &&
			top_priv->mux_rsrc[i].res_state ==
			CAM_ISP_RESOURCE_STATE_AVAILABLE) {

			if (acquire_args->res_id == CAM_ISP_HW_VFE_IN_CAMIF) {
				rc = cam_vfe_camif_ver3_acquire_resource(
					&top_priv->mux_rsrc[i],
					args);
				if (rc)
					break;
			}

			if (acquire_args->res_id >= CAM_ISP_HW_VFE_IN_RDI0 &&
				acquire_args->res_id < CAM_ISP_HW_VFE_IN_MAX) {
				rc = cam_vfe_camif_lite_ver3_acquire_resource(
					&top_priv->mux_rsrc[i],
					args);
				if (rc)
					break;
			}

			if (acquire_args->res_id == CAM_ISP_HW_VFE_IN_RD) {
				rc = cam_vfe_fe_ver1_acquire_resource(
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

int cam_vfe_top_ver3_release(void *device_priv,
	void *release_args, uint32_t arg_size)
{
	struct cam_vfe_top_ver3_priv            *top_priv;
	struct cam_isp_resource_node            *mux_res;

	if (!device_priv || !release_args) {
		CAM_ERR(CAM_ISP, "Error, Invalid input arguments");
		return -EINVAL;
	}

	top_priv = (struct cam_vfe_top_ver3_priv   *)device_priv;
	mux_res = (struct cam_isp_resource_node *)release_args;

	CAM_DBG(CAM_ISP, "Resource in state %d", mux_res->res_state);
	if (mux_res->res_state < CAM_ISP_RESOURCE_STATE_RESERVED) {
		CAM_ERR(CAM_ISP, "Error, Resource in Invalid res_state :%d",
			mux_res->res_state);
		return -EINVAL;
	}
	mux_res->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;

	return 0;
}

int cam_vfe_top_ver3_start(void *device_priv,
	void *start_args, uint32_t arg_size)
{
	struct cam_vfe_top_ver3_priv            *top_priv;
	struct cam_isp_resource_node            *mux_res;
	struct cam_hw_info                      *hw_info = NULL;
	int rc = 0;

	if (!device_priv || !start_args) {
		CAM_ERR(CAM_ISP, "Error, Invalid input arguments");
		return -EINVAL;
	}

	top_priv = (struct cam_vfe_top_ver3_priv *)device_priv;
	mux_res = (struct cam_isp_resource_node *)start_args;
	hw_info = (struct cam_hw_info  *)mux_res->hw_intf->hw_priv;

	if (hw_info->hw_state == CAM_HW_STATE_POWER_UP) {
		rc = cam_vfe_top_ver3_set_hw_clk_rate(top_priv);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"set_hw_clk_rate failed, rc=%d", rc);
			return rc;
		}

		rc = cam_vfe_top_ver3_set_axi_bw_vote(top_priv, true);
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

int cam_vfe_top_ver3_stop(void *device_priv,
	void *stop_args, uint32_t arg_size)
{
	struct cam_vfe_top_ver3_priv            *top_priv;
	struct cam_isp_resource_node            *mux_res;
	struct cam_hw_info                      *hw_info = NULL;
	int i, rc = 0;

	if (!device_priv || !stop_args) {
		CAM_ERR(CAM_ISP, "Error, Invalid input arguments");
		return -EINVAL;
	}

	top_priv = (struct cam_vfe_top_ver3_priv   *)device_priv;
	mux_res = (struct cam_isp_resource_node *)stop_args;
	hw_info = (struct cam_hw_info  *)mux_res->hw_intf->hw_priv;

	if (mux_res->res_id < CAM_ISP_HW_VFE_IN_MAX) {
		rc = mux_res->stop(mux_res);
	} else {
		CAM_ERR(CAM_ISP, "Invalid res id:%d", mux_res->res_id);
		return -EINVAL;
	}

	if (!rc) {
		for (i = 0; i < CAM_VFE_TOP_VER3_MUX_MAX; i++) {
			if (top_priv->mux_rsrc[i].res_id == mux_res->res_id) {
				top_priv->req_clk_rate[i] = 0;
				memset(&top_priv->req_axi_vote[i], 0,
					sizeof(struct cam_axi_vote));
				top_priv->axi_vote_control[i] =
					CAM_VFE_BW_CONTROL_EXCLUDE;
				break;
			}
		}
	}

	return rc;
}

int cam_vfe_top_ver3_read(void *device_priv,
	void *read_args, uint32_t arg_size)
{
	return -EPERM;
}

int cam_vfe_top_ver3_write(void *device_priv,
	void *write_args, uint32_t arg_size)
{
	return -EPERM;
}

int cam_vfe_top_ver3_process_cmd(void *device_priv, uint32_t cmd_type,
	void *cmd_args, uint32_t arg_size)
{
	int rc = 0;
	struct cam_vfe_top_ver3_priv            *top_priv;

	if (!device_priv || !cmd_args) {
		CAM_ERR(CAM_ISP, "Error, Invalid arguments");
		return -EINVAL;
	}
	top_priv = (struct cam_vfe_top_ver3_priv *)device_priv;

	switch (cmd_type) {
	case CAM_ISP_HW_CMD_GET_CHANGE_BASE:
		rc = cam_vfe_top_ver3_mux_get_base(top_priv,
			cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_GET_REG_UPDATE:
		rc = cam_vfe_top_ver3_mux_get_reg_update(top_priv, cmd_args,
			arg_size);
		break;
	case CAM_ISP_HW_CMD_CLOCK_UPDATE:
		rc = cam_vfe_top_ver3_clock_update(top_priv, cmd_args,
			arg_size);
		break;
	case CAM_ISP_HW_CMD_FE_UPDATE_IN_RD:
		rc = cam_vfe_top_fs_update(top_priv, cmd_args,
			arg_size);
		break;
	case CAM_ISP_HW_CMD_BW_UPDATE:
		rc = cam_vfe_top_ver3_bw_update(top_priv, cmd_args,
			arg_size);
		break;
	case CAM_ISP_HW_CMD_BW_UPDATE_V2:
		rc = cam_vfe_top_ver3_bw_update_v2(top_priv, cmd_args,
			arg_size);
		break;
	case CAM_ISP_HW_CMD_BW_CONTROL:
		rc = cam_vfe_top_ver3_bw_control(top_priv, cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_CORE_CONFIG:
		rc = cam_vfe_core_config_control(top_priv, cmd_args, arg_size);
		break;
	default:
		rc = -EINVAL;
		CAM_ERR(CAM_ISP, "Error, Invalid cmd:%d", cmd_type);
		break;
	}

	return rc;
}

int cam_vfe_top_ver3_init(
	struct cam_hw_soc_info                 *soc_info,
	struct cam_hw_intf                     *hw_intf,
	void                                   *top_hw_info,
	void                                   *vfe_irq_controller,
	struct cam_vfe_top                    **vfe_top_ptr)
{
	int i, j, rc = 0;
	struct cam_vfe_top_ver3_priv           *top_priv = NULL;
	struct cam_vfe_top_ver3_hw_info        *ver3_hw_info = top_hw_info;
	struct cam_vfe_top                     *vfe_top;

	vfe_top = kzalloc(sizeof(struct cam_vfe_top), GFP_KERNEL);
	if (!vfe_top) {
		CAM_DBG(CAM_ISP, "Error, Failed to alloc for vfe_top");
		rc = -ENOMEM;
		goto end;
	}

	top_priv = kzalloc(sizeof(struct cam_vfe_top_ver3_priv),
		GFP_KERNEL);
	if (!top_priv) {
		CAM_DBG(CAM_ISP, "Error, Failed to alloc for vfe_top_priv");
		rc = -ENOMEM;
		goto free_vfe_top;
	}
	vfe_top->top_priv = top_priv;
	top_priv->hw_clk_rate = 0;

	for (i = 0, j = 0; i < CAM_VFE_TOP_VER3_MUX_MAX &&
		j < CAM_VFE_RDI_VER2_MAX; i++) {
		top_priv->mux_rsrc[i].res_type = CAM_ISP_RESOURCE_VFE_IN;
		top_priv->mux_rsrc[i].hw_intf = hw_intf;
		top_priv->mux_rsrc[i].res_state =
			CAM_ISP_RESOURCE_STATE_AVAILABLE;
		top_priv->req_clk_rate[i] = 0;

		if (ver3_hw_info->mux_type[i] == CAM_VFE_CAMIF_VER_3_0) {
			top_priv->mux_rsrc[i].res_id =
				CAM_ISP_HW_VFE_IN_CAMIF;

			rc = cam_vfe_camif_ver3_init(hw_intf, soc_info,
				&ver3_hw_info->camif_hw_info,
				&top_priv->mux_rsrc[i], vfe_irq_controller);
			if (rc)
				goto deinit_resources;
		} else if (ver3_hw_info->mux_type[i] ==
			CAM_VFE_PDLIB_VER_1_0) {
			/* set the PDLIB resource id */
			top_priv->mux_rsrc[i].res_id =
				CAM_ISP_HW_VFE_IN_PDLIB;

			rc = cam_vfe_camif_lite_ver3_init(hw_intf, soc_info,
				&ver3_hw_info->pdlib_hw_info,
				&top_priv->mux_rsrc[i], vfe_irq_controller);
			if (rc)
				goto deinit_resources;
		} else if (ver3_hw_info->mux_type[i] ==
			CAM_VFE_IN_RD_VER_1_0) {
			/* set the RD resource id */
			top_priv->mux_rsrc[i].res_id =
				CAM_ISP_HW_VFE_IN_RD;

			rc = cam_vfe_fe_ver1_init(hw_intf, soc_info,
				&ver3_hw_info->fe_hw_info,
				&top_priv->mux_rsrc[i]);
			if (rc)
				goto deinit_resources;
		} else if (ver3_hw_info->mux_type[i] ==
			CAM_VFE_RDI_VER_1_0) {
			/* set the RDI resource id */
			top_priv->mux_rsrc[i].res_id =
				CAM_ISP_HW_VFE_IN_RDI0 + j;

			rc = cam_vfe_camif_lite_ver3_init(hw_intf, soc_info,
				ver3_hw_info->rdi_hw_info[j++],
				&top_priv->mux_rsrc[i], vfe_irq_controller);
			if (rc)
				goto deinit_resources;
		} else if (ver3_hw_info->mux_type[i] ==
			CAM_VFE_LCR_VER_1_0) {
			/* set the LCR resource id */
			top_priv->mux_rsrc[i].res_id =
				CAM_ISP_HW_VFE_IN_LCR;

			rc = cam_vfe_camif_lite_ver3_init(hw_intf, soc_info,
				&ver3_hw_info->lcr_hw_info,
				&top_priv->mux_rsrc[i], vfe_irq_controller);
			if (rc)
				goto deinit_resources;
		} else {
			CAM_WARN(CAM_ISP, "Invalid mux type: %u",
				ver3_hw_info->mux_type[i]);
		}
	}

	vfe_top->hw_ops.get_hw_caps = cam_vfe_top_ver3_get_hw_caps;
	vfe_top->hw_ops.init        = cam_vfe_top_ver3_init_hw;
	vfe_top->hw_ops.reset       = cam_vfe_top_ver3_reset;
	vfe_top->hw_ops.reserve     = cam_vfe_top_ver3_reserve;
	vfe_top->hw_ops.release     = cam_vfe_top_ver3_release;
	vfe_top->hw_ops.start       = cam_vfe_top_ver3_start;
	vfe_top->hw_ops.stop        = cam_vfe_top_ver3_stop;
	vfe_top->hw_ops.read        = cam_vfe_top_ver3_read;
	vfe_top->hw_ops.write       = cam_vfe_top_ver3_write;
	vfe_top->hw_ops.process_cmd = cam_vfe_top_ver3_process_cmd;
	*vfe_top_ptr = vfe_top;

	top_priv->common_data.soc_info     = soc_info;
	top_priv->common_data.hw_intf      = hw_intf;
	top_priv->common_data.common_reg   = ver3_hw_info->common_reg;

	return rc;

deinit_resources:
	for (--i; i >= 0; i--) {
		if (ver3_hw_info->mux_type[i] == CAM_VFE_CAMIF_VER_3_0) {
			if (cam_vfe_camif_ver3_deinit(&top_priv->mux_rsrc[i]))
				CAM_ERR(CAM_ISP, "Camif Deinit failed");
		} else if (ver3_hw_info->mux_type[i] == CAM_VFE_IN_RD_VER_1_0) {
			if (cam_vfe_fe_ver1_deinit(&top_priv->mux_rsrc[i]))
				CAM_ERR(CAM_ISP, "Camif fe Deinit failed");
		} else {
			if (cam_vfe_camif_lite_ver3_deinit(
				&top_priv->mux_rsrc[i]))
				CAM_ERR(CAM_ISP,
					"Camif lite res id %d Deinit failed",
					top_priv->mux_rsrc[i].res_id);
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

int cam_vfe_top_ver3_deinit(struct cam_vfe_top  **vfe_top_ptr)
{
	int i, rc = 0;
	struct cam_vfe_top_ver3_priv           *top_priv = NULL;
	struct cam_vfe_top                     *vfe_top;

	if (!vfe_top_ptr) {
		CAM_ERR(CAM_ISP, "Error, Invalid input");
		return -EINVAL;
	}

	vfe_top = *vfe_top_ptr;
	if (!vfe_top) {
		CAM_ERR(CAM_ISP, "Error, vfe_top NULL");
		return -ENODEV;
	}

	top_priv = vfe_top->top_priv;
	if (!top_priv) {
		CAM_ERR(CAM_ISP, "Error, vfe_top_priv NULL");
		rc = -ENODEV;
		goto free_vfe_top;
	}

	for (i = 0; i < CAM_VFE_TOP_VER3_MUX_MAX; i++) {
		top_priv->mux_rsrc[i].res_state =
			CAM_ISP_RESOURCE_STATE_UNAVAILABLE;
		if (top_priv->mux_rsrc[i].res_type ==
			CAM_VFE_CAMIF_VER_3_0) {
			rc = cam_vfe_camif_ver3_deinit(&top_priv->mux_rsrc[i]);
			if (rc)
				CAM_ERR(CAM_ISP, "Camif deinit failed rc=%d",
					rc);
		} else if (top_priv->mux_rsrc[i].res_type ==
			CAM_VFE_IN_RD_VER_1_0) {
			rc = cam_vfe_fe_ver1_deinit(&top_priv->mux_rsrc[i]);
			if (rc)
				CAM_ERR(CAM_ISP, "Camif deinit failed rc=%d",
					rc);
		} else {
			rc = cam_vfe_camif_lite_ver3_deinit(
				&top_priv->mux_rsrc[i]);
			if (rc)
				CAM_ERR(CAM_ISP,
					"Camif lite res id %d Deinit failed",
					top_priv->mux_rsrc[i].res_id);
		}
	}

	kfree(vfe_top->top_priv);

free_vfe_top:
	kfree(vfe_top);
	*vfe_top_ptr = NULL;

	return rc;
}
