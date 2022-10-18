// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, 2021, The Linux Foundation. All rights reserved.
 */

#include "cam_vfe_top_common.h"
#include "cam_debug_util.h"

static const char *cam_vfe_top_clk_bw_state_to_string(uint32_t state)
{
	switch (state) {
	case CAM_CLK_BW_STATE_UNCHANGED:
		return "UNCHANGED";
	case CAM_CLK_BW_STATE_INCREASE:
		return "INCREASE";
	case CAM_CLK_BW_STATE_DECREASE:
		return "DECREASE";
	default:
		return "Invalid State";
	}
}
static int cam_vfe_top_set_axi_bw_vote(struct cam_vfe_top_priv_common *top_common,
	struct cam_axi_vote *final_bw_vote, uint64_t total_bw_new_vote, bool start_stop,
	uint64_t request_id)
{
	int rc = 0;
	struct cam_hw_soc_info        *soc_info = NULL;
	struct cam_vfe_soc_private    *soc_private = NULL;
	int i, j;

	soc_info = top_common->soc_info;
	soc_private = (struct cam_vfe_soc_private *)soc_info->soc_private;

	CAM_DBG(CAM_PERF, "VFE:%d Sending final BW to cpas bw_state:%s bw_vote:%llu req_id:%ld",
		top_common->hw_idx, cam_vfe_top_clk_bw_state_to_string(top_common->bw_state),
		total_bw_new_vote, (start_stop ? -1 : request_id));
	rc = cam_cpas_update_axi_vote(soc_private->cpas_handle,
		final_bw_vote);
	if (!rc) {
		memcpy(&top_common->applied_axi_vote,
			final_bw_vote,
			sizeof(struct cam_axi_vote));
		top_common->total_bw_applied = total_bw_new_vote;
	} else {
		CAM_ERR(CAM_PERF,
			"VFE:%d BW request failed, req_id: %ld, final num_paths: %d, rc=%d",
			top_common->hw_idx, (start_stop ? -1 : request_id),
			final_bw_vote->num_paths, rc);
		for (i = 0; i < final_bw_vote->num_paths; i++) {
			CAM_INFO(CAM_PERF,
				"ife[%d] : Applied BW Vote : [%s][%s] [%llu %llu %llu]",
				top_common->hw_idx,
				cam_cpas_axi_util_path_type_to_string(
				final_bw_vote->axi_path[i].path_data_type),
				cam_cpas_axi_util_trans_type_to_string(
				final_bw_vote->axi_path[i].transac_type),
				final_bw_vote->axi_path[i].camnoc_bw,
				final_bw_vote->axi_path[i].mnoc_ab_bw,
				final_bw_vote->axi_path[i].mnoc_ib_bw);
		}

		for (i = 0; i < CAM_DELAY_CLK_BW_REDUCTION_NUM_REQ; i++) {
			for (j = 0; j < top_common->last_bw_vote[i].num_paths; j++) {
				CAM_INFO(CAM_PERF,
					"ife[%d] : History[%d] BW Vote : [%s][%s] [%llu %llu %llu]",
					top_common->hw_idx, i,
					cam_cpas_axi_util_path_type_to_string(
					top_common->last_bw_vote[i].axi_path[j].path_data_type),
					cam_cpas_axi_util_trans_type_to_string(
					top_common->last_bw_vote[i].axi_path[j].transac_type),
					top_common->last_bw_vote[i].axi_path[j].camnoc_bw,
					top_common->last_bw_vote[i].axi_path[j].mnoc_ab_bw,
					top_common->last_bw_vote[i].axi_path[j].mnoc_ib_bw);
			}
		}
	}

	return rc;

}

static int cam_vfe_top_set_hw_clk_rate(struct cam_vfe_top_priv_common *top_common,
	uint64_t final_clk_rate, bool start_stop, uint64_t request_id)
{
	struct cam_hw_soc_info        *soc_info = NULL;
	struct cam_vfe_soc_private    *soc_private = NULL;
	struct cam_ahb_vote            ahb_vote;
	int rc = 0, clk_lvl = -1;

	soc_info = top_common->soc_info;
	soc_private = (struct cam_vfe_soc_private *)soc_info->soc_private;

	CAM_DBG(CAM_PERF, "Applying VFE:%d Clock name=%s idx=%d clk=%llu req_id=%ld",
		top_common->hw_idx, soc_info->clk_name[soc_info->src_clk_idx],
		soc_info->src_clk_idx, final_clk_rate, (start_stop ? -1 : request_id));

	rc = cam_soc_util_set_src_clk_rate(soc_info, final_clk_rate);
	if (!rc) {
		soc_private->ife_clk_src = final_clk_rate;

		top_common->applied_clk_rate = final_clk_rate;
		rc = cam_soc_util_get_clk_level(soc_info, final_clk_rate,
			soc_info->src_clk_idx, &clk_lvl);
		if (rc) {
			CAM_WARN(CAM_ISP,
				"Failed to get clk level for %s with clk_rate %llu src_idx %d rc %d",
				soc_info->dev_name, final_clk_rate,
				soc_info->src_clk_idx, rc);
			rc = 0;
			goto end;
		}

		ahb_vote.type = CAM_VOTE_ABSOLUTE;
		ahb_vote.vote.level = clk_lvl;
		cam_cpas_update_ahb_vote(soc_private->cpas_handle, &ahb_vote);
	} else {
		CAM_ERR(CAM_PERF, "VFE:%d Set Clock rate failed, rc=%d",
			top_common->hw_idx, rc);
	}

end:
	return rc;
}

static inline void cam_vfe_top_delay_clk_reduction(
	struct cam_vfe_top_priv_common *top_common,
	uint64_t *max_clk)
{
	int i;

	for (i = 0; i < CAM_DELAY_CLK_BW_REDUCTION_NUM_REQ; i++) {
		if (top_common->last_clk_vote[i] > *max_clk)
			*max_clk = top_common->last_clk_vote[i];
	}
}

static int cam_vfe_top_calc_hw_clk_rate(
	struct cam_vfe_top_priv_common *top_common, bool start_stop,
	uint64_t                       *final_clk_rate, uint64_t request_id)
{
	int                            i, rc = 0;
	uint64_t                       max_req_clk_rate = 0;

	for (i = 0; i < top_common->num_mux; i++) {
		if (top_common->req_clk_rate[i] > max_req_clk_rate)
			max_req_clk_rate = top_common->req_clk_rate[i];
	}

	if (start_stop && !top_common->skip_data_rst_on_stop) {
		/* need to vote current clk immediately */
		*final_clk_rate = max_req_clk_rate;
		/* Reset everything, we can start afresh */
		memset(top_common->last_clk_vote, 0, sizeof(uint64_t) *
			CAM_DELAY_CLK_BW_REDUCTION_NUM_REQ);
		top_common->last_clk_counter = 0;
		top_common->last_clk_vote[top_common->last_clk_counter] =
			max_req_clk_rate;
		top_common->last_clk_counter = (top_common->last_clk_counter + 1) %
			CAM_DELAY_CLK_BW_REDUCTION_NUM_REQ;
	} else {
		top_common->last_clk_vote[top_common->last_clk_counter] =
			max_req_clk_rate;
		top_common->last_clk_counter =
			(top_common->last_clk_counter + 1) %
			CAM_DELAY_CLK_BW_REDUCTION_NUM_REQ;

		/* Find max clk request in last few requests */
		cam_vfe_top_delay_clk_reduction(top_common, final_clk_rate);
		if (!(*final_clk_rate)) {
			CAM_ERR(CAM_PERF, "Final clock rate is zero");
			return -EINVAL;
		}
	}

	if (*final_clk_rate == top_common->applied_clk_rate)
		top_common->clk_state = CAM_CLK_BW_STATE_UNCHANGED;
	else if (*final_clk_rate > top_common->applied_clk_rate)
		top_common->clk_state = CAM_CLK_BW_STATE_INCREASE;
	else
		top_common->clk_state = CAM_CLK_BW_STATE_DECREASE;

	CAM_DBG(CAM_PERF, "VFE:%d Clock state:%s applied_clk_rate:%llu req_id:%ld",
		top_common->hw_idx, cam_vfe_top_clk_bw_state_to_string(top_common->clk_state),
		top_common->applied_clk_rate, (start_stop ? -1 : request_id));

	return rc;
}

int cam_vfe_top_clock_update(struct cam_vfe_top_priv_common *top_common,
	void *cmd_args, uint32_t arg_size)
{
	struct cam_vfe_clock_update_args     *clk_update = NULL;
	struct cam_isp_resource_node         *res = NULL;
	struct cam_hw_info                   *hw_info = NULL;
	int                                   i;

	clk_update =
		(struct cam_vfe_clock_update_args *)cmd_args;
	res = clk_update->node_res;

	if (!res || !res->hw_intf->hw_priv) {
		CAM_ERR(CAM_PERF, "Invalid input res %pK", res);
		return -EINVAL;
	}

	hw_info = res->hw_intf->hw_priv;

	if (res->res_type != CAM_ISP_RESOURCE_VFE_IN ||
		res->res_id >= CAM_ISP_HW_VFE_IN_MAX) {
		CAM_ERR(CAM_PERF, "VFE:%d Invalid res_type:%d res id%d",
			res->hw_intf->hw_idx, res->res_type,
			res->res_id);
		return -EINVAL;
	}

	for (i = 0; i < top_common->num_mux; i++) {
		if (top_common->mux_rsrc[i].res_id == res->res_id) {
			top_common->req_clk_rate[i] = clk_update->clk_rate;
			break;
		}
	}

	return 0;
}

static struct cam_axi_vote *cam_vfe_top_delay_bw_reduction(
	struct cam_vfe_top_priv_common *top_common,
	uint64_t *to_be_applied_bw)
{
	uint32_t i;
	int vote_idx = -1;
	uint64_t max_bw = 0;

	for (i = 0; i < CAM_DELAY_CLK_BW_REDUCTION_NUM_REQ; i++) {
		if (top_common->last_total_bw_vote[i] > max_bw) {
			vote_idx = i;
			max_bw = top_common->last_total_bw_vote[i];
		}
	}

	if (vote_idx < 0)
		return NULL;

	*to_be_applied_bw = max_bw;

	return &top_common->last_bw_vote[vote_idx];
}

static int cam_vfe_top_calc_axi_bw_vote(
	struct cam_vfe_top_priv_common *top_common, bool start_stop,
	struct cam_axi_vote **to_be_applied_axi_vote, uint64_t *total_bw_new_vote,
	uint64_t request_id)
{
	int rc = 0;
	uint32_t i;
	uint32_t num_paths = 0;
	bool bw_unchanged = true;
	struct cam_axi_vote *final_bw_vote = NULL;

	if (top_common->num_mux > CAM_VFE_TOP_MUX_MAX) {
		CAM_ERR(CAM_PERF,
			"Number of Mux exceeds max, # Mux: %d > Limit: %d",
			top_common->num_mux, CAM_VFE_TOP_MUX_MAX);
		return -EINVAL;
	}

	memset(&top_common->agg_incoming_vote, 0, sizeof(struct cam_axi_vote));
	for (i = 0; i < top_common->num_mux; i++) {
		if (top_common->axi_vote_control[i] ==
			CAM_ISP_BW_CONTROL_INCLUDE) {
			if (num_paths +
				top_common->req_axi_vote[i].num_paths >
				CAM_CPAS_MAX_PATHS_PER_CLIENT) {
				CAM_ERR(CAM_PERF,
					"Required paths(%d) more than max(%d)",
					num_paths +
					top_common->req_axi_vote[i].num_paths,
					CAM_CPAS_MAX_PATHS_PER_CLIENT);
				rc = -EINVAL;
				goto end;
			}

			memcpy(&top_common->agg_incoming_vote.axi_path[num_paths],
				&top_common->req_axi_vote[i].axi_path[0],
				top_common->req_axi_vote[i].num_paths *
				sizeof(
				struct cam_axi_per_path_bw_vote));
			num_paths += top_common->req_axi_vote[i].num_paths;
		}
	}

	top_common->agg_incoming_vote.num_paths = num_paths;

	for (i = 0; i < top_common->agg_incoming_vote.num_paths; i++) {
		CAM_DBG(CAM_PERF,
			"ife[%d] : New BW Vote : counter[%d] [%s][%s] [%llu %llu %llu]",
			top_common->hw_idx,
			top_common->last_bw_counter,
			cam_cpas_axi_util_path_type_to_string(
			top_common->agg_incoming_vote.axi_path[i].path_data_type),
			cam_cpas_axi_util_trans_type_to_string(
			top_common->agg_incoming_vote.axi_path[i].transac_type),
			top_common->agg_incoming_vote.axi_path[i].camnoc_bw,
			top_common->agg_incoming_vote.axi_path[i].mnoc_ab_bw,
			top_common->agg_incoming_vote.axi_path[i].mnoc_ib_bw);

		*total_bw_new_vote += top_common->agg_incoming_vote.axi_path[i].camnoc_bw;
	}

	memcpy(&top_common->last_bw_vote[top_common->last_bw_counter],
		&top_common->agg_incoming_vote, sizeof(struct cam_axi_vote));
	top_common->last_total_bw_vote[top_common->last_bw_counter] = *total_bw_new_vote;
	top_common->last_bw_counter = (top_common->last_bw_counter + 1) %
		CAM_DELAY_CLK_BW_REDUCTION_NUM_REQ;

	if (*total_bw_new_vote != top_common->total_bw_applied)
		bw_unchanged = false;

	CAM_DBG(CAM_PERF,
		"ife[%d] : applied_total=%lld, new_total=%lld unchanged=%d, start_stop=%d req_id=%ld",
		top_common->hw_idx, top_common->total_bw_applied,
		*total_bw_new_vote, bw_unchanged, start_stop, (start_stop ? -1 : request_id));

	if (bw_unchanged) {
		CAM_DBG(CAM_PERF, "BW config unchanged");
		*to_be_applied_axi_vote = NULL;
		top_common->bw_state = CAM_CLK_BW_STATE_UNCHANGED;
		goto end;
	}

	if (start_stop) {
		/* need to vote current request immediately */
		final_bw_vote = &top_common->agg_incoming_vote;
		/* Reset everything, we can start afresh */
		memset(top_common->last_bw_vote, 0, sizeof(struct cam_axi_vote) *
			CAM_DELAY_CLK_BW_REDUCTION_NUM_REQ);
		memset(top_common->last_total_bw_vote, 0, sizeof(uint64_t) *
			CAM_DELAY_CLK_BW_REDUCTION_NUM_REQ);
		top_common->last_bw_counter = 0;
		top_common->last_bw_vote[top_common->last_bw_counter] =
			top_common->agg_incoming_vote;
		top_common->last_total_bw_vote[top_common->last_bw_counter] = *total_bw_new_vote;
		top_common->last_bw_counter = (top_common->last_bw_counter + 1) %
			CAM_DELAY_CLK_BW_REDUCTION_NUM_REQ;
	} else {
		/*
		 * Find max bw request in last few frames. This will the bw
		 * that we want to vote to CPAS now.
		 */
		final_bw_vote =
			cam_vfe_top_delay_bw_reduction(top_common,
				total_bw_new_vote);
		if (!final_bw_vote) {
			CAM_ERR(CAM_PERF, "to_be_applied_axi_vote is NULL");
			rc = -EINVAL;
			goto end;
		}
	}

	for (i = 0; i < final_bw_vote->num_paths; i++) {
		CAM_DBG(CAM_PERF,
			"ife[%d] : Apply BW Vote : [%s][%s] [%llu %llu %llu]",
			top_common->hw_idx,
			cam_cpas_axi_util_path_type_to_string(
			final_bw_vote->axi_path[i].path_data_type),
			cam_cpas_axi_util_trans_type_to_string(
			final_bw_vote->axi_path[i].transac_type),
			final_bw_vote->axi_path[i].camnoc_bw,
			final_bw_vote->axi_path[i].mnoc_ab_bw,
			final_bw_vote->axi_path[i].mnoc_ib_bw);
	}

	if (*total_bw_new_vote == top_common->total_bw_applied) {
		CAM_DBG(CAM_PERF, "VFE:%d Final BW Unchanged after delay", top_common->hw_idx);
		top_common->bw_state = CAM_CLK_BW_STATE_UNCHANGED;
		*to_be_applied_axi_vote = NULL;
		goto end;
	} else if (*total_bw_new_vote > top_common->total_bw_applied) {
		top_common->bw_state = CAM_CLK_BW_STATE_INCREASE;
	} else {
		top_common->bw_state = CAM_CLK_BW_STATE_DECREASE;
	}

	CAM_DBG(CAM_PERF,
		"ife[%d] : Delayed update: applied_total=%lld new_total=%lld start_stop=%d bw_state=%s req_id=%ld",
		top_common->hw_idx, top_common->total_bw_applied,
		*total_bw_new_vote, start_stop,
		cam_vfe_top_clk_bw_state_to_string(top_common->bw_state),
		(start_stop ? -1 : request_id));

	*to_be_applied_axi_vote = final_bw_vote;

end:
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
				CAM_ISP_BW_CONTROL_INCLUDE;
			break;
		}
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
				CAM_ISP_BW_CONTROL_INCLUDE;
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

	return rc;
}

int cam_vfe_top_bw_control(struct cam_vfe_soc_private *soc_private,
	struct cam_vfe_top_priv_common *top_common, void *cmd_args,
	uint32_t arg_size)
{
	struct cam_isp_bw_control_args       *bw_ctrl = NULL;
	struct cam_isp_resource_node         *res = NULL;
	struct cam_hw_info                   *hw_info = NULL;
	int                                   rc = 0;
	int                                   i;

	bw_ctrl = (struct cam_isp_bw_control_args *)cmd_args;
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
		rc = cam_vfe_top_apply_bw_start_stop(top_common);
	}

	return rc;
}

int cam_vfe_top_apply_clk_bw_update(struct cam_vfe_top_priv_common *top_common,
	void *cmd_args, uint32_t arg_size)
{
	struct cam_hw_info                   *hw_info = NULL;
	struct cam_hw_intf                   *hw_intf = NULL;
	struct cam_axi_vote *to_be_applied_axi_vote = NULL;
	struct cam_isp_apply_clk_bw_args *clk_bw_args = NULL;
	uint64_t                              final_clk_rate = 0;
	uint64_t                              total_bw_new_vote = 0;
	uint64_t                              request_id;
	int rc = 0;

	if (arg_size != sizeof(struct cam_isp_apply_clk_bw_args)) {
		CAM_ERR(CAM_ISP, "Invalid arg size: %u", arg_size);
		return -EINVAL;
	}

	clk_bw_args = (struct cam_isp_apply_clk_bw_args *)cmd_args;
	request_id = clk_bw_args->request_id;
	hw_intf = clk_bw_args->hw_intf;
	if (!hw_intf) {
		CAM_ERR(CAM_PERF, "Invalid hw_intf");
		return -EINVAL;
	}

	hw_info = hw_intf->hw_priv;
	if (hw_info->hw_state != CAM_HW_STATE_POWER_UP) {
		CAM_DBG(CAM_PERF,
			"VFE:%d Not ready to set clocks yet :%d",
			hw_intf->hw_idx, hw_info->hw_state);
		goto end;
	}

	if (clk_bw_args->skip_clk_data_rst) {
		top_common->skip_data_rst_on_stop = true;
		CAM_DBG(CAM_ISP, "VFE:%u requested to avoid clk data rst", hw_intf->hw_idx);
		return 0;
	}

	rc = cam_vfe_top_calc_hw_clk_rate(top_common, false, &final_clk_rate, request_id);
	if (rc) {
		CAM_ERR(CAM_ISP,
			"VFE:%d Failed in calculating clock rate rc=%d",
			hw_intf->hw_idx, rc);
		goto end;
	}

	rc = cam_vfe_top_calc_axi_bw_vote(top_common, false,
		&to_be_applied_axi_vote, &total_bw_new_vote, request_id);
	if (rc) {
		CAM_ERR(CAM_ISP, "VFE:%d Failed in calculating bw vote rc=%d",
			hw_intf->hw_idx, rc);
		goto end;
	}

	if ((!to_be_applied_axi_vote) && (top_common->bw_state != CAM_CLK_BW_STATE_UNCHANGED)) {
		CAM_ERR(CAM_PERF, "VFE:%d Invalid BW vote for state:%s", hw_intf->hw_idx,
			cam_vfe_top_clk_bw_state_to_string(top_common->bw_state));
		rc = -EINVAL;
		goto end;
	}

	CAM_DBG(CAM_PERF, "VFE:%d APPLY CLK/BW req_id:%ld clk_state:%s bw_state:%s ",
		hw_intf->hw_idx, request_id,
		cam_vfe_top_clk_bw_state_to_string(top_common->clk_state),
		cam_vfe_top_clk_bw_state_to_string(top_common->bw_state));

	/* Determine BW and clock voting sequence according to state */
	if ((top_common->clk_state == CAM_CLK_BW_STATE_UNCHANGED) &&
		(top_common->bw_state == CAM_CLK_BW_STATE_UNCHANGED)) {
		goto end;
	} else if (top_common->clk_state == CAM_CLK_BW_STATE_UNCHANGED) {
		rc = cam_vfe_top_set_axi_bw_vote(top_common, to_be_applied_axi_vote,
			total_bw_new_vote, false, request_id);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"VFE:%d Failed in voting final bw:%llu clk_state:%s bw_state:%s",
				hw_intf->hw_idx, total_bw_new_vote,
				cam_vfe_top_clk_bw_state_to_string(top_common->clk_state),
				cam_vfe_top_clk_bw_state_to_string(top_common->bw_state));
			goto end;
		}
	} else if (top_common->bw_state == CAM_CLK_BW_STATE_UNCHANGED) {
		rc = cam_vfe_top_set_hw_clk_rate(top_common, final_clk_rate, false, request_id);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"VFE:%d Failed in voting final clk:%llu clk_state:%s bw_state:%s",
				hw_intf->hw_idx, final_clk_rate,
				cam_vfe_top_clk_bw_state_to_string(top_common->clk_state),
				cam_vfe_top_clk_bw_state_to_string(top_common->bw_state));
			goto end;
		}
	} else if (top_common->clk_state == CAM_CLK_BW_STATE_INCREASE) {
		/* Set BW first, followed by Clock */
		rc = cam_vfe_top_set_axi_bw_vote(top_common, to_be_applied_axi_vote,
			total_bw_new_vote, false, request_id);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"VFE:%d Failed in voting final bw:%llu clk_state:%s bw_state:%s",
				hw_intf->hw_idx, total_bw_new_vote,
				cam_vfe_top_clk_bw_state_to_string(top_common->clk_state),
				cam_vfe_top_clk_bw_state_to_string(top_common->bw_state));
			goto end;
		}

		rc = cam_vfe_top_set_hw_clk_rate(top_common, final_clk_rate, false, 0);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"VFE:%d Failed in voting final clk:%llu clk_state:%s bw_state:%s",
				hw_intf->hw_idx, final_clk_rate,
				cam_vfe_top_clk_bw_state_to_string(top_common->clk_state),
				cam_vfe_top_clk_bw_state_to_string(top_common->bw_state));
			goto end;
		}
	} else if (top_common->clk_state == CAM_CLK_BW_STATE_DECREASE) {
		/* Set Clock first, followed by BW */
		rc = cam_vfe_top_set_hw_clk_rate(top_common, final_clk_rate, false, request_id);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"VFE:%d Failed in voting final clk:%llu clk_state:%s bw_state:%s",
				hw_intf->hw_idx, final_clk_rate,
				cam_vfe_top_clk_bw_state_to_string(top_common->clk_state),
				cam_vfe_top_clk_bw_state_to_string(top_common->bw_state));
			goto end;
		}

		rc = cam_vfe_top_set_axi_bw_vote(top_common, to_be_applied_axi_vote,
			total_bw_new_vote, false, request_id);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"VFE:%d Failed in voting final bw:%llu clk_state:%s bw_state:%s",
				hw_intf->hw_idx, total_bw_new_vote,
				cam_vfe_top_clk_bw_state_to_string(top_common->clk_state),
				cam_vfe_top_clk_bw_state_to_string(top_common->bw_state));
			goto end;
		}
	} else {
		CAM_ERR(CAM_ISP, "Invalid state to apply CLK/BW clk_state:%s bw_state:%s",
			cam_vfe_top_clk_bw_state_to_string(top_common->clk_state),
			cam_vfe_top_clk_bw_state_to_string(top_common->bw_state));
		rc = -EINVAL;
		goto end;
	}

end:
	top_common->clk_state = CAM_CLK_BW_STATE_INIT;
	top_common->bw_state = CAM_CLK_BW_STATE_INIT;
	return rc;
}

int cam_vfe_top_apply_clock_start_stop(struct cam_vfe_top_priv_common *top_common)
{
	int rc = 0;
	uint64_t final_clk_rate = 0;

	rc = cam_vfe_top_calc_hw_clk_rate(top_common, true, &final_clk_rate, 0);
	if (rc) {
		CAM_ERR(CAM_ISP,
			"VFE:%d Failed in calculating clock rate rc=%d",
			top_common->hw_idx, rc);
		goto end;
	}

	if (top_common->clk_state == CAM_CLK_BW_STATE_UNCHANGED)
		goto end;

	rc = cam_vfe_top_set_hw_clk_rate(top_common, final_clk_rate, true, 0);
	if (rc) {
		CAM_ERR(CAM_ISP, "VFE:%d Failed in voting final clk:%llu clk_state:%s",
			top_common->hw_idx, final_clk_rate,
			cam_vfe_top_clk_bw_state_to_string(top_common->clk_state));
		goto end;
	}

end:
	top_common->clk_state = CAM_CLK_BW_STATE_INIT;
	top_common->skip_data_rst_on_stop = false;
	return rc;
}

int cam_vfe_top_apply_bw_start_stop(struct cam_vfe_top_priv_common *top_common)
{
	int rc = 0;
	uint64_t total_bw_new_vote = 0;
	struct cam_axi_vote *to_be_applied_axi_vote = NULL;

	rc = cam_vfe_top_calc_axi_bw_vote(top_common, true, &to_be_applied_axi_vote,
		&total_bw_new_vote, 0);
	if (rc) {
		CAM_ERR(CAM_ISP, "VFE:%d Failed in calculating bw vote rc=%d",
			top_common->hw_idx, rc);
		goto end;
	}

	if (top_common->bw_state == CAM_CLK_BW_STATE_UNCHANGED)
		goto end;

	rc = cam_vfe_top_set_axi_bw_vote(top_common, to_be_applied_axi_vote, total_bw_new_vote,
		true, 0);
	if (rc) {
		CAM_ERR(CAM_ISP, "VFE:%d Failed in voting final bw:%llu bw_state:%s",
			top_common->hw_idx, total_bw_new_vote,
			cam_vfe_top_clk_bw_state_to_string(top_common->bw_state));
		goto end;
	}

end:
	top_common->bw_state = CAM_CLK_BW_STATE_INIT;
	return rc;
}

