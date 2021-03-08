// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#include "cam_vfe_top_common.h"
#include "cam_debug_util.h"

static struct cam_axi_vote *cam_vfe_top_delay_bw_reduction(
	struct cam_vfe_top_priv_common *top_common,
	uint64_t *to_be_applied_bw)
{
	uint32_t i, j;
	int vote_idx = -1;
	uint64_t max_bw = 0;
	uint64_t total_bw;
	struct cam_axi_vote *curr_l_vote;

	for (i = 0; i < CAM_VFE_DELAY_BW_REDUCTION_NUM_FRAMES; i++) {
		total_bw = 0;
		curr_l_vote = &top_common->last_vote[i];
		for (j = 0; j < curr_l_vote->num_paths; j++) {
			if (total_bw >
				(U64_MAX -
				curr_l_vote->axi_path[j].camnoc_bw)) {
				CAM_ERR(CAM_PERF,
					"ife[%d] : Integer overflow at hist idx: %d, path: %d, total_bw = %llu, camnoc_bw = %llu",
					top_common->hw_idx, i, j, total_bw,
					curr_l_vote->axi_path[j].camnoc_bw);
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

	return &top_common->last_vote[vote_idx];
}

int cam_vfe_top_set_axi_bw_vote(struct cam_vfe_soc_private *soc_private,
	struct cam_vfe_top_priv_common *top_common, bool start_stop)
{
	struct cam_axi_vote agg_vote = {0};
	struct cam_axi_vote *to_be_applied_axi_vote = NULL;
	int rc = 0;
	uint32_t i;
	uint32_t num_paths = 0;
	uint64_t total_bw_new_vote = 0;
	bool bw_unchanged = true;
	bool apply_bw_update = false;

	for (i = 0; i < top_common->num_mux; i++) {
		if (top_common->axi_vote_control[i] ==
			CAM_VFE_BW_CONTROL_INCLUDE) {
			if (num_paths +
				top_common->req_axi_vote[i].num_paths >
				CAM_CPAS_MAX_PATHS_PER_CLIENT) {
				CAM_ERR(CAM_PERF,
					"Required paths(%d) more than max(%d)",
					num_paths +
					top_common->req_axi_vote[i].num_paths,
					CAM_CPAS_MAX_PATHS_PER_CLIENT);
				return -EINVAL;
			}

			memcpy(&agg_vote.axi_path[num_paths],
				&top_common->req_axi_vote[i].axi_path[0],
				top_common->req_axi_vote[i].num_paths *
				sizeof(
				struct cam_axi_per_path_bw_vote));
			num_paths += top_common->req_axi_vote[i].num_paths;
		}
	}

	agg_vote.num_paths = num_paths;

	for (i = 0; i < agg_vote.num_paths; i++) {
		CAM_DBG(CAM_PERF,
			"ife[%d] : New BW Vote : counter[%d] [%s][%s] [%llu %llu %llu]",
			top_common->hw_idx,
			top_common->last_counter,
			cam_cpas_axi_util_path_type_to_string(
			agg_vote.axi_path[i].path_data_type),
			cam_cpas_axi_util_trans_type_to_string(
			agg_vote.axi_path[i].transac_type),
			agg_vote.axi_path[i].camnoc_bw,
			agg_vote.axi_path[i].mnoc_ab_bw,
			agg_vote.axi_path[i].mnoc_ib_bw);

		total_bw_new_vote += agg_vote.axi_path[i].camnoc_bw;
	}

	memcpy(&top_common->last_vote[top_common->last_counter], &agg_vote,
		sizeof(struct cam_axi_vote));
	top_common->last_counter = (top_common->last_counter + 1) %
		CAM_VFE_DELAY_BW_REDUCTION_NUM_FRAMES;

	if ((agg_vote.num_paths != top_common->applied_axi_vote.num_paths) ||
		(total_bw_new_vote != top_common->total_bw_applied))
		bw_unchanged = false;

	CAM_DBG(CAM_PERF,
		"ife[%d] : applied_total=%lld, new_total=%lld unchanged=%d, start_stop=%d",
		top_common->hw_idx, top_common->total_bw_applied,
		total_bw_new_vote, bw_unchanged, start_stop);

	if (bw_unchanged) {
		CAM_DBG(CAM_PERF, "BW config unchanged");
		return 0;
	}

	if (start_stop) {
		/* need to vote current request immediately */
		to_be_applied_axi_vote = &agg_vote;
		/* Reset everything, we can start afresh */
		memset(top_common->last_vote, 0x0, sizeof(struct cam_axi_vote) *
			CAM_VFE_DELAY_BW_REDUCTION_NUM_FRAMES);
		top_common->last_counter = 0;
		top_common->last_vote[top_common->last_counter] = agg_vote;
		top_common->last_counter = (top_common->last_counter + 1) %
			CAM_VFE_DELAY_BW_REDUCTION_NUM_FRAMES;
	} else {
		/*
		 * Find max bw request in last few frames. This will the bw
		 * that we want to vote to CPAS now.
		 */
		to_be_applied_axi_vote =
			cam_vfe_top_delay_bw_reduction(top_common,
			&total_bw_new_vote);
		if (!to_be_applied_axi_vote) {
			CAM_ERR(CAM_PERF, "to_be_applied_axi_vote is NULL");
			return -EINVAL;
		}
	}

	for (i = 0; i < to_be_applied_axi_vote->num_paths; i++) {
		CAM_DBG(CAM_PERF,
			"ife[%d] : Apply BW Vote : [%s][%s] [%llu %llu %llu]",
			top_common->hw_idx,
			cam_cpas_axi_util_path_type_to_string(
			to_be_applied_axi_vote->axi_path[i].path_data_type),
			cam_cpas_axi_util_trans_type_to_string(
			to_be_applied_axi_vote->axi_path[i].transac_type),
			to_be_applied_axi_vote->axi_path[i].camnoc_bw,
			to_be_applied_axi_vote->axi_path[i].mnoc_ab_bw,
			to_be_applied_axi_vote->axi_path[i].mnoc_ib_bw);
	}

	if ((to_be_applied_axi_vote->num_paths !=
		top_common->applied_axi_vote.num_paths) ||
		(total_bw_new_vote != top_common->total_bw_applied))
		apply_bw_update = true;

	CAM_DBG(CAM_PERF,
		"ife[%d] : Delayed update: applied_total=%lld, new_total=%lld apply_bw_update=%d, start_stop=%d",
		top_common->hw_idx, top_common->total_bw_applied,
		total_bw_new_vote, apply_bw_update, start_stop);

	if (apply_bw_update) {
		rc = cam_cpas_update_axi_vote(soc_private->cpas_handle,
			to_be_applied_axi_vote);
		if (!rc) {
			memcpy(&top_common->applied_axi_vote,
				to_be_applied_axi_vote,
				sizeof(struct cam_axi_vote));
			top_common->total_bw_applied = total_bw_new_vote;
		} else {
			CAM_ERR(CAM_PERF, "BW request failed, rc=%d", rc);
		}
	}

	return rc;
}

int cam_vfe_top_bw_update_v2(struct cam_vfe_soc_private *soc_private,
	struct cam_vfe_top_priv_common *top_common, void *cmd_args,
	uint32_t arg_size)
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

	for (i = 0; i < top_common->num_mux; i++) {
		if (top_common->mux_rsrc[i].res_id == res->res_id) {
			memcpy(&top_common->req_axi_vote[i],
				&bw_update->isp_vote,
				sizeof(struct cam_axi_vote));
			top_common->axi_vote_control[i] =
				CAM_VFE_BW_CONTROL_INCLUDE;
			break;
		}
	}

	if (hw_info->hw_state != CAM_HW_STATE_POWER_UP) {
		CAM_ERR_RATE_LIMIT(CAM_PERF,
			"VFE:%d Not ready to set BW yet :%d",
			res->hw_intf->hw_idx,
			hw_info->hw_state);
	} else {
		rc = cam_vfe_top_set_axi_bw_vote(soc_private, top_common,
			false);
	}

	return rc;
}

int cam_vfe_top_bw_update(struct cam_vfe_soc_private *soc_private,
	struct cam_vfe_top_priv_common *top_common, void *cmd_args,
	uint32_t arg_size)
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

	CAM_DBG(CAM_PERF, "res_id=%d, BW=[%lld %lld]",
		res->res_id, bw_update->camnoc_bw_bytes,
		bw_update->external_bw_bytes);

	if (res->res_type != CAM_ISP_RESOURCE_VFE_IN ||
		res->res_id >= CAM_ISP_HW_VFE_IN_MAX) {
		CAM_ERR(CAM_ISP, "VFE:%d Invalid res_type:%d res id%d",
			res->hw_intf->hw_idx, res->res_type,
			res->res_id);
		return -EINVAL;
	}

	for (i = 0; i < top_common->num_mux; i++) {
		mux_axi_vote = &top_common->req_axi_vote[i];
		if (top_common->mux_rsrc[i].res_id == res->res_id) {
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

			top_common->axi_vote_control[i] =
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
		CAM_ERR_RATE_LIMIT(CAM_PERF,
			"VFE:%d Not ready to set BW yet :%d",
			res->hw_intf->hw_idx,
			hw_info->hw_state);
	} else {
		rc = cam_vfe_top_set_axi_bw_vote(soc_private, top_common,
			false);
	}

	return rc;
}

int cam_vfe_top_bw_control(struct cam_vfe_soc_private *soc_private,
	struct cam_vfe_top_priv_common *top_common, void *cmd_args,
	uint32_t arg_size)
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

	for (i = 0; i < top_common->num_mux; i++) {
		if (top_common->mux_rsrc[i].res_id == res->res_id) {
			top_common->axi_vote_control[i] = bw_ctrl->action;
			break;
		}
	}

	if (hw_info->hw_state != CAM_HW_STATE_POWER_UP) {
		CAM_ERR_RATE_LIMIT(CAM_PERF,
			"VFE:%d Not ready to set BW yet :%d",
			res->hw_intf->hw_idx,
			hw_info->hw_state);
	} else {
		rc = cam_vfe_top_set_axi_bw_vote(soc_private, top_common, true);
	}

	return rc;
}
