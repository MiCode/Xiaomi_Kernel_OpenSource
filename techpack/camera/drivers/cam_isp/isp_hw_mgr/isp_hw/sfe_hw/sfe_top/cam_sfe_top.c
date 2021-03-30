// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/slab.h>
#include "cam_io_util.h"
#include "cam_cdm_util.h"
#include "cam_sfe_hw_intf.h"
#include "cam_tasklet_util.h"
#include "cam_sfe_top.h"
#include "cam_debug_util.h"
#include "cam_sfe_soc.h"
#include "cam_sfe_core.h"

#define CAM_SFE_DELAY_BW_REDUCTION_NUM_FRAMES 18

struct cam_sfe_core_cfg {
	uint32_t   mode_sel;
	uint32_t   ops_mode_cfg;
	uint32_t   fs_mode_cfg;
};

struct cam_sfe_top_common_data {
	struct cam_hw_soc_info                  *soc_info;
	struct cam_hw_intf                      *hw_intf;
	struct cam_sfe_top_common_reg_offset    *common_reg;
	struct cam_irq_controller               *sfe_irq_controller;
	struct cam_sfe_top_irq_evt_payload       evt_payload[CAM_SFE_EVT_MAX];
	struct list_head                         free_payload_list;
};

struct cam_sfe_top_priv {
	struct cam_sfe_top_common_data  common_data;
	struct cam_isp_resource_node    in_rsrc[CAM_SFE_TOP_IN_PORT_MAX];
	uint32_t                        num_in_ports;
	unsigned long                   hw_clk_rate;
	unsigned long                   req_clk_rate[CAM_SFE_TOP_IN_PORT_MAX];
	uint32_t                        last_counter;
	uint64_t                        total_bw_applied;
	struct cam_axi_vote             req_axi_vote[CAM_SFE_TOP_IN_PORT_MAX];
	struct cam_axi_vote             last_vote[
			CAM_SFE_DELAY_BW_REDUCTION_NUM_FRAMES];
	enum cam_sfe_bw_control_action  axi_vote_control[
		CAM_SFE_TOP_IN_PORT_MAX];
	struct cam_axi_vote             applied_axi_vote;
	struct cam_sfe_core_cfg         core_cfg;
	uint32_t                        sfe_debug_cfg;
	spinlock_t                      spin_lock;
};

struct cam_sfe_path_data {
	void __iomem                             *mem_base;
	void                                     *priv;
	struct cam_hw_intf                       *hw_intf;
	struct cam_sfe_top_priv                  *top_priv;
	struct cam_sfe_top_common_reg_offset     *common_reg;
	struct cam_sfe_modules_common_reg_offset *modules_reg;
	struct cam_sfe_path_common_reg_data      *path_reg_data;
	struct cam_hw_soc_info                   *soc_info;
	uint32_t                                  min_hblank_cnt;
	int                                       error_irq_handle;
	int                                       sof_eof_handle;
	cam_hw_mgr_event_cb_func                  event_cb;
};

static const char *cam_sfe_top_res_id_to_string(
	uint32_t res_id)
{
	switch (res_id) {
	case CAM_ISP_HW_SFE_IN_PIX:
		return "PP";
	case CAM_ISP_HW_SFE_IN_RDI0:
		return "RDI0";
	case CAM_ISP_HW_SFE_IN_RDI1:
		return "RDI1";
	case CAM_ISP_HW_SFE_IN_RDI2:
		return "RDI2";
	case CAM_ISP_HW_SFE_IN_RDI3:
		return "RDI3";
	case CAM_ISP_HW_SFE_IN_RDI4:
		return "RDI4";
	default:
		return "";
	}
}

static struct cam_axi_vote *cam_sfe_top_delay_bw_reduction(
	struct cam_sfe_top_priv *top_priv,
	uint64_t *to_be_applied_bw)
{
	uint32_t i, j;
	int vote_idx = -1;
	uint64_t max_bw = 0;
	uint64_t total_bw;
	struct cam_axi_vote *curr_l_vote;

	for (i = 0; i < CAM_SFE_DELAY_BW_REDUCTION_NUM_FRAMES; i++) {
		total_bw = 0;
		curr_l_vote = &top_priv->last_vote[i];
		for (j = 0; j < curr_l_vote->num_paths; j++) {
			if (total_bw >
				(U64_MAX -
				curr_l_vote->axi_path[j].camnoc_bw)) {
				CAM_ERR(CAM_PERF,
					"sfe[%d] : Integer overflow at hist idx: %d, path: %d, total_bw = %llu, camnoc_bw = %llu",
					top_priv->common_data.hw_intf->hw_idx,
					i, j, total_bw,
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

	return &top_priv->last_vote[vote_idx];
}

int cam_sfe_top_set_axi_bw_vote(struct cam_sfe_soc_private *soc_private,
	struct cam_sfe_top_priv *top_priv, bool start_stop)
{
	struct cam_axi_vote agg_vote = {0};
	struct cam_axi_vote *to_be_applied_axi_vote = NULL;
	int rc = 0;
	uint32_t i;
	uint32_t num_paths = 0;
	uint64_t total_bw_new_vote = 0;
	bool bw_unchanged = true;
	bool apply_bw_update = false;

	for (i = 0; i < top_priv->num_in_ports; i++) {
		if (top_priv->axi_vote_control[i] ==
			CAM_SFE_BW_CONTROL_INCLUDE) {
			if (num_paths +
				top_priv->req_axi_vote[i].num_paths >
				CAM_CPAS_MAX_PATHS_PER_CLIENT) {
				CAM_ERR(CAM_PERF,
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
			"sfe[%d] : New BW Vote : counter[%d] [%s][%s] [%llu %llu %llu]",
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
		CAM_SFE_DELAY_BW_REDUCTION_NUM_FRAMES;

	if ((agg_vote.num_paths != top_priv->applied_axi_vote.num_paths) ||
		(total_bw_new_vote != top_priv->total_bw_applied))
		bw_unchanged = false;

	CAM_DBG(CAM_PERF,
		"applied_total=%lld, new_total=%lld unchanged=%d, start_stop=%d",
		top_priv->total_bw_applied,
		total_bw_new_vote, bw_unchanged, start_stop);

	if (bw_unchanged) {
		CAM_DBG(CAM_PERF, "BW config unchanged");
		return 0;
	}

	if (start_stop) {
		/* need to vote current request immediately */
		to_be_applied_axi_vote = &agg_vote;
		/* Reset everything, we can start afresh */
		memset(top_priv->last_vote, 0x0, sizeof(struct cam_axi_vote) *
			CAM_SFE_DELAY_BW_REDUCTION_NUM_FRAMES);
		top_priv->last_counter = 0;
		top_priv->last_vote[top_priv->last_counter] = agg_vote;
		top_priv->last_counter = (top_priv->last_counter + 1) %
			CAM_SFE_DELAY_BW_REDUCTION_NUM_FRAMES;
	} else {
		/*
		 * Find max bw request in last few frames. This will the bw
		 * that we want to vote to CPAS now.
		 */
		to_be_applied_axi_vote =
			cam_sfe_top_delay_bw_reduction(top_priv,
			&total_bw_new_vote);
		if (!to_be_applied_axi_vote) {
			CAM_ERR(CAM_PERF, "to_be_applied_axi_vote is NULL");
			return -EINVAL;
		}
	}

	for (i = 0; i < to_be_applied_axi_vote->num_paths; i++) {
		CAM_DBG(CAM_PERF,
			"sfe[%d] : Apply BW Vote : [%s][%s] [%llu %llu %llu]",
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
		"sfe[%d] : Delayed update: applied_total=%lld, new_total=%lld apply_bw_update=%d, start_stop=%d",
		top_priv->common_data.hw_intf->hw_idx,
		top_priv->total_bw_applied, total_bw_new_vote, apply_bw_update,
		start_stop);

	if (apply_bw_update) {
		rc = cam_cpas_update_axi_vote(soc_private->cpas_handle,
			to_be_applied_axi_vote);
		if (!rc) {
			memcpy(&top_priv->applied_axi_vote,
				to_be_applied_axi_vote,
				sizeof(struct cam_axi_vote));
			top_priv->total_bw_applied = total_bw_new_vote;
		} else {
			CAM_ERR(CAM_PERF, "BW request failed, rc=%d", rc);
		}
	}

	return rc;
}

int cam_sfe_top_bw_update(struct cam_sfe_soc_private *soc_private,
	struct cam_sfe_top_priv *top_priv, void *cmd_args,
	uint32_t arg_size)
{
	struct cam_sfe_bw_update_args        *bw_update = NULL;
	struct cam_isp_resource_node         *res = NULL;
	struct cam_hw_info                   *hw_info = NULL;
	int                                   rc = 0;
	int                                   i;

	bw_update = (struct cam_sfe_bw_update_args *)cmd_args;
	res = bw_update->node_res;

	if (!res || !res->hw_intf || !res->hw_intf->hw_priv)
		return -EINVAL;

	hw_info = res->hw_intf->hw_priv;

	if (res->res_type != CAM_ISP_RESOURCE_SFE_IN ||
		res->res_id >= CAM_ISP_HW_SFE_IN_MAX) {
		CAM_ERR(CAM_SFE, "SFE:%d Invalid res_type:%d res id%d",
			res->hw_intf->hw_idx, res->res_type,
			res->res_id);
		return -EINVAL;
	}

	for (i = 0; i < top_priv->num_in_ports; i++) {
		if (top_priv->in_rsrc[i].res_id == res->res_id) {
			memcpy(&top_priv->req_axi_vote[i],
				&bw_update->sfe_vote,
				sizeof(struct cam_axi_vote));
			top_priv->axi_vote_control[i] =
				CAM_SFE_BW_CONTROL_INCLUDE;
			break;
		}
	}

	if (hw_info->hw_state != CAM_HW_STATE_POWER_UP) {
		CAM_ERR_RATE_LIMIT(CAM_SFE,
			"SFE:%d Not ready to set BW yet :%d",
			res->hw_intf->hw_idx,
			hw_info->hw_state);
	} else {
		rc = cam_sfe_top_set_axi_bw_vote(soc_private, top_priv,
			false);
	}

	return rc;
}

int cam_sfe_top_bw_control(struct cam_sfe_soc_private *soc_private,
	struct cam_sfe_top_priv *top_priv, void *cmd_args,
	uint32_t arg_size)
{
	struct cam_sfe_bw_control_args       *bw_ctrl = NULL;
	struct cam_isp_resource_node         *res = NULL;
	struct cam_hw_info                   *hw_info = NULL;
	int                                   rc = 0;
	int                                   i;

	bw_ctrl = (struct cam_sfe_bw_control_args *)cmd_args;
	res = bw_ctrl->node_res;

	if (!res || !res->hw_intf->hw_priv)
		return -EINVAL;

	hw_info = res->hw_intf->hw_priv;

	if (res->res_type != CAM_ISP_RESOURCE_SFE_IN ||
		res->res_id >= CAM_ISP_HW_SFE_IN_MAX) {
		CAM_ERR(CAM_SFE, "SFE:%d Invalid res_type:%d res id%d",
			res->hw_intf->hw_idx, res->res_type,
			res->res_id);
		return -EINVAL;
	}

	for (i = 0; i < top_priv->num_in_ports; i++) {
		if (top_priv->in_rsrc[i].res_id == res->res_id) {
			top_priv->axi_vote_control[i] = bw_ctrl->action;
			break;
		}
	}

	if (hw_info->hw_state != CAM_HW_STATE_POWER_UP) {
		CAM_ERR_RATE_LIMIT(CAM_SFE,
			"SFE:%d Not ready to set BW yet :%d",
			res->hw_intf->hw_idx,
			hw_info->hw_state);
	} else {
		rc = cam_sfe_top_set_axi_bw_vote(soc_private, top_priv, true);
	}

	return rc;
}

static int cam_sfe_top_core_cfg(
	struct cam_sfe_top_priv *top_priv,
	void *cmd_args, uint32_t arg_size)
{
	struct cam_sfe_core_config_args *sfe_core_cfg = NULL;

	if ((!cmd_args) ||
		(arg_size != sizeof(struct cam_sfe_core_config_args))) {
		CAM_ERR(CAM_SFE, "Invalid inputs");
		return -EINVAL;
	}

	sfe_core_cfg = (struct cam_sfe_core_config_args *)cmd_args;
	top_priv->core_cfg.mode_sel =
		sfe_core_cfg->core_config.mode_sel;
	top_priv->core_cfg.fs_mode_cfg =
		sfe_core_cfg->core_config.fs_mode_cfg;
	top_priv->core_cfg.ops_mode_cfg =
		sfe_core_cfg->core_config.ops_mode_cfg;

	return 0;
}

static int cam_sfe_top_set_hw_clk_rate(
	struct cam_sfe_top_priv *top_priv)
{
	struct cam_hw_soc_info        *soc_info = NULL;
	struct cam_sfe_soc_private    *soc_private = NULL;
	struct cam_ahb_vote            ahb_vote;
	int                            rc, clk_lvl = -1, i;
	unsigned long                  max_clk_rate = 0;

	soc_info = top_priv->common_data.soc_info;
	for (i = 0; i < top_priv->num_in_ports; i++) {
		if (top_priv->req_clk_rate[i] > max_clk_rate)
			max_clk_rate = top_priv->req_clk_rate[i];
	}

	if (max_clk_rate == top_priv->hw_clk_rate)
		return 0;

	soc_private = (struct cam_sfe_soc_private *)
		soc_info->soc_private;
	CAM_DBG(CAM_PERF, "SFE [%u]: clk: %s idx: %d rate: %llu",
		soc_info->index,
		soc_info->clk_name[soc_info->src_clk_idx],
		soc_info->src_clk_idx, top_priv->req_clk_rate);

	rc = cam_soc_util_set_src_clk_rate(soc_info,
		max_clk_rate);

	if (!rc) {
		top_priv->hw_clk_rate = max_clk_rate;
		rc = cam_soc_util_get_clk_level(soc_info,
			max_clk_rate,
			soc_info->src_clk_idx, &clk_lvl);
		if (rc) {
			CAM_WARN(CAM_SFE,
				"Failed to get clk level for %s with clk_rate %llu src_idx %d rc: %d",
				soc_info->dev_name, max_clk_rate,
				soc_info->src_clk_idx, rc);
			rc = 0;
			goto end;
		}
		ahb_vote.type = CAM_VOTE_ABSOLUTE;
		ahb_vote.vote.level = clk_lvl;
		cam_cpas_update_ahb_vote(soc_private->cpas_handle, &ahb_vote);
	} else {
		CAM_ERR(CAM_PERF,
			"Set clk rate failed for SFE [%u] clk: %s rate: %llu rc: %d",
			soc_info->index,
			soc_info->clk_name[soc_info->src_clk_idx],
			max_clk_rate, rc);
	}

end:
	return rc;
}

static int cam_sfe_top_get_base(
	struct cam_sfe_top_priv *top_priv,
	void *cmd_args, uint32_t arg_size)
{
	uint32_t                          size = 0;
	uint32_t                          mem_base = 0;
	struct cam_isp_hw_get_cmd_update *cdm_args  = cmd_args;
	struct cam_cdm_utils_ops         *cdm_util_ops = NULL;

	if (arg_size != sizeof(struct cam_isp_hw_get_cmd_update)) {
		CAM_ERR(CAM_SFE, "Invalid cmd size");
		return -EINVAL;
	}

	if (!cdm_args || !cdm_args->res || !top_priv ||
		!top_priv->common_data.soc_info) {
		CAM_ERR(CAM_SFE, "Invalid args");
		return -EINVAL;
	}

	cdm_util_ops =
		(struct cam_cdm_utils_ops *)cdm_args->res->cdm_ops;

	if (!cdm_util_ops) {
		CAM_ERR(CAM_SFE, "Invalid CDM ops");
		return -EINVAL;
	}

	size = cdm_util_ops->cdm_required_size_changebase();
	if ((size * 4) > cdm_args->cmd.size) {
		CAM_ERR(CAM_SFE, "buf size: %d is not sufficient, expected: %d",
			cdm_args->cmd.size, size);
		return -EINVAL;
	}

	mem_base = CAM_SOC_GET_REG_MAP_CAM_BASE(
		top_priv->common_data.soc_info,
		SFE_CORE_BASE_IDX);

	if (cdm_args->cdm_id == CAM_CDM_RT)
		mem_base -= CAM_SOC_GET_REG_MAP_CAM_BASE(
			top_priv->common_data.soc_info,
			SFE_RT_CDM_BASE_IDX);

	CAM_DBG(CAM_SFE, "core %d mem_base 0x%x, CDM Id: %d",
		top_priv->common_data.soc_info->index, mem_base,
		cdm_args->cdm_id);

	cdm_util_ops->cdm_write_changebase(
	cdm_args->cmd.cmd_buf_addr, mem_base);
	cdm_args->cmd.used_bytes = (size * 4);

	return 0;
}

static int cam_sfe_top_clock_update(
	struct cam_sfe_top_priv *top_priv,
	void *cmd_args, uint32_t arg_size)
{
	struct cam_sfe_clock_update_args     *clk_update = NULL;
	struct cam_isp_resource_node         *res = NULL;
	struct cam_hw_info                   *hw_info = NULL;
	int                                   rc = 0, i;

	if (arg_size != sizeof(struct cam_sfe_clock_update_args)) {
		CAM_ERR(CAM_SFE, "Invalid cmd size");
		return -EINVAL;
	}

	clk_update =
		(struct cam_sfe_clock_update_args *)cmd_args;
	if (!clk_update || !clk_update->node_res || !top_priv ||
		!top_priv->common_data.soc_info) {
		CAM_ERR(CAM_SFE, "Invalid args");
		return -EINVAL;
	}

	res = clk_update->node_res;

	if (!res || !res->hw_intf->hw_priv) {
		CAM_ERR(CAM_PERF, "Invalid inputs");
		return -EINVAL;
	}

	hw_info = res->hw_intf->hw_priv;

	if (res->res_type != CAM_ISP_RESOURCE_SFE_IN ||
		res->res_id >= CAM_ISP_HW_SFE_IN_MAX) {
		CAM_ERR(CAM_PERF,
			"SFE: %d Invalid res_type: %d res_id: %d",
			res->hw_intf->hw_idx, res->res_type,
			res->res_id);
		return -EINVAL;
	}

	for (i = 0; i < top_priv->num_in_ports; i++) {
		if (top_priv->in_rsrc[i].res_id == res->res_id) {
			top_priv->req_clk_rate[i] = clk_update->clk_rate;
			break;
		}
	}

	if (hw_info->hw_state != CAM_HW_STATE_POWER_UP) {
		CAM_DBG(CAM_PERF,
			"SFE: %d not ready to set clocks yet :%d",
			res->hw_intf->hw_idx,
			hw_info->hw_state);
		rc = -EAGAIN;
	} else
		rc = cam_sfe_top_set_hw_clk_rate(top_priv);

	return rc;
}

static int cam_sfe_set_top_debug(
	struct cam_sfe_top_priv *top_priv,
	void *cmd_args)
{
	uint32_t *sfe_debug;

	sfe_debug = (uint32_t *)cmd_args;
	top_priv->sfe_debug_cfg = *sfe_debug;

	return 0;
}

int cam_sfe_top_process_cmd(void *priv, uint32_t cmd_type,
	void *cmd_args, uint32_t arg_size)
{
	int rc = 0;
	struct cam_sfe_top_priv           *top_priv;
	struct cam_hw_soc_info            *soc_info = NULL;
	struct cam_sfe_soc_private        *soc_private = NULL;

	if (!priv) {
		CAM_ERR(CAM_SFE, "Invalid top_priv");
		return -EINVAL;
	}

	top_priv = (struct cam_sfe_top_priv *) priv;
	soc_info = top_priv->common_data.soc_info;
	soc_private = soc_info->soc_private;

	if (!soc_private) {
		CAM_ERR(CAM_SFE, "soc private is NULL");
		return -EINVAL;
	}

	switch (cmd_type) {
	case CAM_ISP_HW_CMD_GET_CHANGE_BASE:
		rc = cam_sfe_top_get_base(top_priv, cmd_args,
			arg_size);
		break;
	case CAM_ISP_HW_CMD_CLOCK_UPDATE:
		rc = cam_sfe_top_clock_update(top_priv, cmd_args,
			arg_size);
		break;
	case CAM_ISP_HW_CMD_BW_UPDATE_V2:
		rc = cam_sfe_top_bw_update(soc_private, top_priv,
			cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_BW_CONTROL:
		break;
	case CAM_ISP_HW_CMD_CORE_CONFIG:
		rc = cam_sfe_top_core_cfg(top_priv, cmd_args,
			arg_size);
		break;
	case CAM_ISP_HW_CMD_SET_SFE_DEBUG_CFG:
		rc = cam_sfe_set_top_debug(top_priv, cmd_args);
		break;
	default:
		CAM_ERR(CAM_SFE, "Invalid cmd type: %d", cmd_type);
		rc = -EINVAL;
		break;
	}

	return rc;
}

int cam_sfe_top_reserve(void *device_priv,
	void *reserve_args, uint32_t arg_size)
{
	struct cam_sfe_top_priv                 *top_priv;
	struct cam_sfe_acquire_args             *args;
	struct cam_sfe_hw_sfe_in_acquire_args   *acquire_args;
	struct cam_sfe_path_data                *path_data;
	int rc = -EINVAL, i;

	if (!device_priv || !reserve_args) {
		CAM_ERR(CAM_SFE,
			"Error invalid input arguments");
		return rc;
	}

	top_priv = (struct cam_sfe_top_priv *)device_priv;
	args = (struct cam_sfe_acquire_args *)reserve_args;
	acquire_args = &args->sfe_in;

	for (i = 0; i < CAM_SFE_TOP_IN_PORT_MAX; i++) {
		CAM_DBG(CAM_SFE, "i: %d res_id: %d state: %d", i,
			acquire_args->res_id, top_priv->in_rsrc[i].res_state);

		if ((top_priv->in_rsrc[i].res_id == acquire_args->res_id) &&
			(top_priv->in_rsrc[i].res_state ==
			CAM_ISP_RESOURCE_STATE_AVAILABLE)) {
			path_data = (struct cam_sfe_path_data *)
				top_priv->in_rsrc[i].res_priv;
			path_data->event_cb = args->event_cb;
			path_data->priv = args->priv;
			path_data->top_priv = top_priv;
			CAM_DBG(CAM_SFE,
				"SFE [%u] for rsrc: %u acquired",
				top_priv->in_rsrc[i].hw_intf->hw_idx,
				acquire_args->res_id);

			top_priv->in_rsrc[i].cdm_ops = acquire_args->cdm_ops;
			top_priv->in_rsrc[i].tasklet_info = args->tasklet;
			top_priv->in_rsrc[i].res_state =
				CAM_ISP_RESOURCE_STATE_RESERVED;
			acquire_args->rsrc_node =
				&top_priv->in_rsrc[i];
			rc = 0;
			break;
		}
	}

	return rc;
}

int cam_sfe_top_release(void *device_priv,
	void *release_args, uint32_t arg_size)
{
	struct cam_sfe_top_priv            *top_priv;
	struct cam_isp_resource_node       *in_res;

	if (!device_priv || !release_args) {
		CAM_ERR(CAM_SFE, "Invalid input arguments");
		return -EINVAL;
	}

	top_priv = (struct cam_sfe_top_priv   *)device_priv;
	in_res = (struct cam_isp_resource_node *)release_args;

	CAM_DBG(CAM_SFE,
		"Release for SFE [%u] resource id: %u in state: %d",
		in_res->hw_intf->hw_idx, in_res->res_id,
		in_res->res_state);
	if (in_res->res_state < CAM_ISP_RESOURCE_STATE_RESERVED) {
		CAM_ERR(CAM_SFE, "SFE [%u] invalid res_state: %d",
			in_res->hw_intf->hw_idx, in_res->res_state);
		return -EINVAL;
	}

	in_res->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;
	in_res->cdm_ops = NULL;
	in_res->tasklet_info = NULL;

	return 0;
}

static int cam_sfe_top_get_evt_payload(
	struct cam_sfe_top_priv            *top_priv,
	struct cam_sfe_top_irq_evt_payload    **evt_payload)
{
	int rc = 0;

	spin_lock(&top_priv->spin_lock);
	if (list_empty(&top_priv->common_data.free_payload_list)) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "No free CAMIF LITE event payload");
		*evt_payload = NULL;
		rc = -ENODEV;
		goto done;
	}

	*evt_payload = list_first_entry(
		&top_priv->common_data.free_payload_list,
		struct cam_sfe_top_irq_evt_payload, list);
	list_del_init(&(*evt_payload)->list);

done:
	spin_unlock(&top_priv->spin_lock);
	return rc;
}

static int cam_sfe_top_put_evt_payload(
	struct cam_sfe_top_priv                *top_priv,
	struct cam_sfe_top_irq_evt_payload    **evt_payload)
{
	unsigned long flags;

	if (!top_priv) {
		CAM_ERR(CAM_SFE, "Invalid param core_info NULL");
		return -EINVAL;
	}
	if (*evt_payload == NULL) {
		CAM_ERR(CAM_SFE, "No payload to put");
		return -EINVAL;
	}

	spin_lock_irqsave(&top_priv->spin_lock, flags);
	list_add_tail(&(*evt_payload)->list,
		&top_priv->common_data.free_payload_list);
	*evt_payload = NULL;
	spin_unlock_irqrestore(&top_priv->spin_lock, flags);

	CAM_DBG(CAM_SFE, "Done");
	return 0;
}

static void cam_sfe_top_print_debug_reg_info(
	struct cam_sfe_path_data           *path_data)
{
	CAM_INFO(CAM_SFE,
		"Debug0: 0x%x Debug1: 0x%x Debug2: 0x%x Debug3: 0x%x",
		cam_io_r_mb(path_data->mem_base +
			path_data->common_reg->top_debug_0),
		cam_io_r_mb(path_data->mem_base +
			path_data->common_reg->top_debug_1),
		cam_io_r_mb(path_data->mem_base +
			path_data->common_reg->top_debug_2),
		cam_io_r_mb(path_data->mem_base +
			path_data->common_reg->top_debug_3));
	CAM_INFO(CAM_SFE,
		"Debug4: 0x%x Debug5: 0x%x Debug6: 0x%x Debug7: 0x%x",
		cam_io_r_mb(path_data->mem_base +
			path_data->common_reg->top_debug_4),
		cam_io_r_mb(path_data->mem_base +
			path_data->common_reg->top_debug_5),
		cam_io_r_mb(path_data->mem_base +
			path_data->common_reg->top_debug_6),
		cam_io_r_mb(path_data->mem_base +
			path_data->common_reg->top_debug_7));
	CAM_INFO(CAM_SFE,
		"Debug8: 0x%x Debug9: 0x%x Debug10: 0x%x Debug11: 0x%x",
		cam_io_r_mb(path_data->mem_base +
			path_data->common_reg->top_debug_8),
		cam_io_r_mb(path_data->mem_base +
			path_data->common_reg->top_debug_9),
		cam_io_r_mb(path_data->mem_base +
			path_data->common_reg->top_debug_10),
		cam_io_r_mb(path_data->mem_base +
			path_data->common_reg->top_debug_11));
}

static int cam_sfe_top_handle_err_irq_top_half(
	uint32_t evt_id,
	struct cam_irq_th_payload *th_payload)
{
	int rc = 0, i;
	uint32_t irq_status = 0;
	void __iomem *base = NULL;
	struct cam_sfe_top_priv            *top_priv;
	struct cam_isp_resource_node       *res;
	struct cam_sfe_path_data           *path_data;
	struct cam_sfe_top_irq_evt_payload *evt_payload;

	res = th_payload->handler_priv;
	path_data = res->res_priv;
	top_priv = path_data->priv;

	CAM_DBG(CAM_SFE, "Top error IRQ Received");

	irq_status = th_payload->evt_status_arr[0];

	base = path_data->mem_base;
	if (irq_status & path_data->path_reg_data->error_irq_mask) {
		CAM_ERR(CAM_SFE,
			"SFE Violation Detected irq_status: 0x%x",
			irq_status);
		cam_irq_controller_disable_irq(
			top_priv->common_data.sfe_irq_controller,
			path_data->error_irq_handle);
		cam_irq_controller_clear_and_mask(evt_id,
			top_priv->common_data.sfe_irq_controller);
	}

	rc  = cam_sfe_top_get_evt_payload(top_priv, &evt_payload);
	if (rc)
		return rc;

	for (i = 0; i < th_payload->num_registers; i++)
		evt_payload->irq_reg_val[i] =
			th_payload->evt_status_arr[i];

	cam_isp_hw_get_timestamp(&evt_payload->ts);
	evt_payload->violation_status =
	cam_io_r(base +
		top_priv->common_data.common_reg->violation_status);

	th_payload->evt_payload_priv = evt_payload;

	return rc;
}

static int cam_sfe_top_handle_irq_top_half(uint32_t evt_id,
	struct cam_irq_th_payload *th_payload)
{
	int rc = 0, i;
	uint32_t irq_status = 0;
	struct cam_sfe_top_priv             *top_priv;
	struct cam_isp_resource_node        *res;
	struct cam_sfe_path_data            *path_data;
	struct cam_sfe_top_irq_evt_payload  *evt_payload;

	res = th_payload->handler_priv;
	path_data = res->res_priv;
	top_priv = path_data->top_priv;

	rc  = cam_sfe_top_get_evt_payload(top_priv, &evt_payload);
	if (rc)
		return rc;

	irq_status = th_payload->evt_status_arr[0];
	CAM_DBG(CAM_SFE, "SFE top irq status: 0x%x",
			irq_status);
	for (i = 0; i < th_payload->num_registers; i++)
		evt_payload->irq_reg_val[i] =
			th_payload->evt_status_arr[i];

	cam_isp_hw_get_timestamp(&evt_payload->ts);
	th_payload->evt_payload_priv = evt_payload;
	return rc;
}

static int cam_sfe_top_handle_irq_bottom_half(
	void *handler_priv, void *evt_payload_priv)
{
	int i;
	uint32_t irq_status[CAM_SFE_IRQ_REGISTERS_MAX] = {0};
	enum cam_sfe_hw_irq_status          ret = CAM_SFE_IRQ_STATUS_MAX;
	struct cam_isp_hw_event_info        evt_info;
	struct cam_isp_resource_node       *res = handler_priv;
	struct cam_sfe_path_data           *path_data = res->res_priv;
	struct cam_sfe_top_priv            *top_priv = path_data->top_priv;
	struct cam_sfe_top_irq_evt_payload *payload = evt_payload_priv;

	for (i = 0; i < CAM_SFE_IRQ_REGISTERS_MAX; i++)
		irq_status[i] = payload->irq_reg_val[i];

	evt_info.hw_idx = res->hw_intf->hw_idx;
	evt_info.res_id = res->res_id;
	evt_info.res_type = res->res_type;
	evt_info.reg_val = 0;

	if (irq_status[0] &
		path_data->path_reg_data->error_irq_mask) {
		if (irq_status[0] & 0x4000)
			CAM_ERR(CAM_SFE, "PP VIOLATION");

		if (irq_status[0] & 0x8000)
			CAM_ERR(CAM_SFE, "DIAG VIOLATION");

		if (irq_status[0] & 0x10000)
			CAM_ERR(CAM_SFE, "LINE SMOOTH VIOLATION");

		CAM_INFO(CAM_SFE, "Violation status 0x%x",
			payload->violation_status);

		evt_info.err_type = CAM_SFE_IRQ_STATUS_VIOLATION;
		cam_sfe_top_print_debug_reg_info(path_data);
		if (path_data->event_cb)
			path_data->event_cb(NULL,
				CAM_ISP_HW_EVENT_ERROR, (void *)&evt_info);

		ret = CAM_SFE_IRQ_STATUS_VIOLATION;
	}

	/* To Do Debugfs entry to control the enablement */
	if (irq_status[0] & path_data->path_reg_data->subscribe_irq_mask) {
		if (irq_status[0] & path_data->path_reg_data->sof_irq_mask) {
			CAM_INFO_RATE_LIMIT(CAM_SFE, "SFE:%d Received %s SOF",
				evt_info.hw_idx,
				cam_sfe_top_res_id_to_string(res->res_id));
		}

		if (irq_status[0] &
			path_data->path_reg_data->eof_irq_mask) {
			CAM_INFO_RATE_LIMIT(CAM_SFE, "SFE:%d Received %s EOF",
				evt_info.hw_idx,
				cam_sfe_top_res_id_to_string(res->res_id));
		}
		ret = CAM_SFE_IRQ_STATUS_SUCCESS;
	}

	/* To DO frame counter under debugfs */
	CAM_DBG(CAM_SFE,
		"SFE:%d SFE_DIAG_SENSOR_STATUS0: 0x%x SFE_DIAG_SENSOR_STATUS1: 0x%x",
		evt_info.hw_idx,
		cam_io_r(path_data->mem_base +
			path_data->common_reg->diag_sensor_status_0),
		cam_io_r(path_data->mem_base +
			path_data->common_reg->diag_sensor_status_1));

	cam_sfe_top_put_evt_payload(top_priv, &payload);

	return ret;
}

int cam_sfe_top_start(
	void *priv, void *start_args, uint32_t arg_size)
{
	int                                   rc = -EINVAL;
	struct cam_sfe_top_priv              *top_priv;
	struct cam_isp_resource_node         *sfe_res;
	struct cam_hw_info                   *hw_info = NULL;
	struct cam_sfe_path_data             *path_data;
	uint32_t   error_mask[CAM_SFE_IRQ_REGISTERS_MAX];
	uint32_t   sof_eof_mask[CAM_SFE_IRQ_REGISTERS_MAX];

	if (!priv || !start_args) {
		CAM_ERR(CAM_SFE, "Invalid args");
		return -EINVAL;
	}

	top_priv = (struct cam_sfe_top_priv *)priv;
	sfe_res = (struct cam_isp_resource_node *) start_args;

	hw_info = (struct cam_hw_info  *)sfe_res->hw_intf->hw_priv;
	if (!hw_info) {
		CAM_ERR(CAM_SFE, "Invalid input");
		return rc;
	}

	if (hw_info->hw_state != CAM_HW_STATE_POWER_UP) {
		CAM_ERR(CAM_SFE, "SFE HW [%u] not powered up",
			hw_info->soc_info.index);
		rc = -EPERM;
		return rc;
	}

	path_data = (struct cam_sfe_path_data *)sfe_res->res_priv;
	rc = cam_sfe_top_set_hw_clk_rate(top_priv);
	if (rc)
		return rc;

	/* core cfg updated via CDM */
	CAM_DBG(CAM_SFE, "SFE HW [%u] core_cfg: 0x%x",
		hw_info->soc_info.index,
		cam_io_r_mb(path_data->mem_base +
			path_data->common_reg->core_cfg));

	/* Enable debug cfg registers */
	cam_io_w(path_data->path_reg_data->top_debug_cfg_en,
		path_data->mem_base +
		path_data->common_reg->top_debug_cfg);

	/* Enable sensor diag info */
	/* TO DO - debug fs to enable*/
	cam_io_w(path_data->path_reg_data->enable_diagnostic_hw,
		path_data->mem_base + path_data->common_reg->diag_config);

	error_mask[0] = path_data->path_reg_data->error_irq_mask;
	/* Enable error IRQ by default */
	if (!path_data->error_irq_handle) {
		path_data->error_irq_handle = cam_irq_controller_subscribe_irq(
			top_priv->common_data.sfe_irq_controller,
			CAM_IRQ_PRIORITY_0,
			error_mask,
			sfe_res,
			cam_sfe_top_handle_err_irq_top_half,
			cam_sfe_top_handle_irq_bottom_half,
			sfe_res->tasklet_info,
			&tasklet_bh_api);

		if (path_data->error_irq_handle < 1) {
			CAM_ERR(CAM_SFE, "Failed to subscribe Top IRQ");
			path_data->error_irq_handle = 0;
			return -EFAULT;
		}
	}

	/* TO DO define the debugfs to enable/disable sof/eof irq */
	if (!path_data->sof_eof_handle) {
		sof_eof_mask[0] = path_data->path_reg_data->subscribe_irq_mask;
		path_data->sof_eof_handle = cam_irq_controller_subscribe_irq(
			top_priv->common_data.sfe_irq_controller,
			CAM_IRQ_PRIORITY_1,
			sof_eof_mask,
			sfe_res,
			cam_sfe_top_handle_irq_top_half,
			cam_sfe_top_handle_irq_bottom_half,
			sfe_res->tasklet_info,
			&tasklet_bh_api);

		if (path_data->sof_eof_handle < 1) {
			CAM_ERR(CAM_SFE, "Failed to subscribe SOF/EOF IRQ");
			path_data->sof_eof_handle = 0;
			return -EFAULT;
		}
	}

	sfe_res->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;
	return 0;
}

int cam_sfe_top_stop(
	void *priv, void *stop_args, uint32_t arg_size)
{
	int i;
	struct cam_sfe_top_priv       *top_priv;
	struct cam_isp_resource_node  *sfe_res;
	struct cam_sfe_path_data      *path_data;

	if (!priv || !stop_args) {
		CAM_ERR(CAM_SFE, "Invalid args");
		return -EINVAL;
	}

	top_priv = (struct cam_sfe_top_priv *)priv;
	sfe_res = (struct cam_isp_resource_node *) stop_args;
	path_data = sfe_res->res_priv;

	if (sfe_res->res_state == CAM_ISP_RESOURCE_STATE_RESERVED ||
		sfe_res->res_state == CAM_ISP_RESOURCE_STATE_AVAILABLE)
		return 0;

	/* Unsubscribe for IRQs */
	sfe_res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;
	for (i = 0; i < CAM_SFE_TOP_IN_PORT_MAX; i++) {
		if (top_priv->in_rsrc[i].res_id == sfe_res->res_id) {
			top_priv->req_clk_rate[i] = 0;
			memset(&top_priv->req_axi_vote[i], 0,
				sizeof(struct cam_axi_vote));
			top_priv->axi_vote_control[i] =
				CAM_SFE_BW_CONTROL_EXCLUDE;
			break;
		}
	}

	if (path_data->error_irq_handle > 0) {
		cam_irq_controller_unsubscribe_irq(
			top_priv->common_data.sfe_irq_controller,
			path_data->error_irq_handle);
	}

	if (path_data->sof_eof_handle > 0) {
		cam_irq_controller_unsubscribe_irq(
			top_priv->common_data.sfe_irq_controller,
			path_data->sof_eof_handle);
	}

	return 0;
}

int cam_sfe_top_init(
	uint32_t                            hw_version,
	struct cam_hw_soc_info             *soc_info,
	struct cam_hw_intf                 *hw_intf,
	void                               *top_hw_info,
	void                               *sfe_irq_controller,
	struct cam_sfe_top                **sfe_top_ptr)
{
	int i, j, rc = 0;
	struct cam_sfe_top_priv           *top_priv = NULL;
	struct cam_sfe_path_data          *path_data = NULL;
	struct cam_sfe_top                *sfe_top;
	struct cam_sfe_top_hw_info        *sfe_top_hw_info =
		(struct cam_sfe_top_hw_info *)top_hw_info;

	sfe_top = kzalloc(sizeof(struct cam_sfe_top), GFP_KERNEL);
	if (!sfe_top) {
		CAM_DBG(CAM_SFE, "Error, Failed to alloc for sfe_top");
		rc = -ENOMEM;
		goto end;
	}

	top_priv = kzalloc(sizeof(struct cam_sfe_top_priv),
		GFP_KERNEL);
	if (!top_priv) {
		rc = -ENOMEM;
		goto free_sfe_top;
	}

	sfe_top->top_priv = top_priv;
	top_priv->common_data.sfe_irq_controller = sfe_irq_controller;
	if (sfe_top_hw_info->num_inputs > CAM_SFE_TOP_IN_PORT_MAX) {
		CAM_ERR(CAM_SFE,
			"Invalid number of input resources: %d max: %d",
			sfe_top_hw_info->num_inputs,
			CAM_SFE_TOP_IN_PORT_MAX);
		rc = -EINVAL;
		goto free_top_priv;
	}

	top_priv->hw_clk_rate = 0;
	top_priv->num_in_ports = sfe_top_hw_info->num_inputs;
	memset(top_priv->last_vote, 0x0, sizeof(struct cam_axi_vote) *
		CAM_SFE_DELAY_BW_REDUCTION_NUM_FRAMES);
	memset(&top_priv->core_cfg, 0x0,
		sizeof(struct cam_sfe_core_cfg));

	CAM_DBG(CAM_SFE,
		"Initializing SFE [%u] top with hw_version: 0x%x",
		hw_intf->hw_idx, hw_version);
	for (i = 0, j = 0; i < top_priv->num_in_ports &&
		j < CAM_SFE_RDI_MAX; i++) {
		top_priv->in_rsrc[i].res_type =
			CAM_ISP_RESOURCE_SFE_IN;
		top_priv->in_rsrc[i].hw_intf = hw_intf;
		top_priv->in_rsrc[i].res_state =
			CAM_ISP_RESOURCE_STATE_AVAILABLE;
		top_priv->req_clk_rate[i] = 0;

		if (sfe_top_hw_info->input_type[i] ==
			CAM_SFE_PIX_VER_1_0) {
			top_priv->in_rsrc[i].res_id =
				CAM_ISP_HW_SFE_IN_PIX;

			path_data = kzalloc(sizeof(struct cam_sfe_path_data),
				GFP_KERNEL);
			if (!path_data) {
				CAM_DBG(CAM_SFE,
					"Failed to alloc SFE [%u] pix data",
					hw_intf->hw_idx);
				goto deinit_resources;
			}
			top_priv->in_rsrc[i].res_priv = path_data;
			path_data->mem_base =
				soc_info->reg_map[SFE_CORE_BASE_IDX].mem_base;
			path_data->path_reg_data =
				sfe_top_hw_info->pix_reg_data;
			path_data->common_reg = sfe_top_hw_info->common_reg;
			path_data->modules_reg =
				sfe_top_hw_info->modules_hw_info;
			path_data->hw_intf = hw_intf;
			path_data->soc_info = soc_info;
		} else if (sfe_top_hw_info->input_type[i] ==
			CAM_SFE_RDI_VER_1_0) {
			top_priv->in_rsrc[i].res_id =
				CAM_ISP_HW_SFE_IN_RDI0 + j;

			path_data = kzalloc(sizeof(struct cam_sfe_path_data),
					GFP_KERNEL);
			if (!path_data) {
				CAM_DBG(CAM_SFE,
					"Failed to alloc SFE [%u] rdi data res_id: %u",
					hw_intf->hw_idx,
					(CAM_ISP_HW_SFE_IN_RDI0 + j));
				goto deinit_resources;
			}

			top_priv->in_rsrc[i].res_priv = path_data;

			path_data->mem_base =
				soc_info->reg_map[SFE_CORE_BASE_IDX].mem_base;
			path_data->hw_intf = hw_intf;
			path_data->common_reg = sfe_top_hw_info->common_reg;
			path_data->modules_reg =
				sfe_top_hw_info->modules_hw_info;
			path_data->soc_info = soc_info;
			path_data->path_reg_data =
				sfe_top_hw_info->rdi_reg_data[j++];
		} else {
			CAM_WARN(CAM_SFE, "Invalid SFE input type: %u",
				sfe_top_hw_info->input_type[i]);
		}
	}

	top_priv->common_data.soc_info = soc_info;
	top_priv->common_data.hw_intf = hw_intf;
	top_priv->common_data.common_reg =
		sfe_top_hw_info->common_reg;

	sfe_top->hw_ops.process_cmd = cam_sfe_top_process_cmd;
	sfe_top->hw_ops.start = cam_sfe_top_start;
	sfe_top->hw_ops.stop = cam_sfe_top_stop;
	sfe_top->hw_ops.reserve = cam_sfe_top_reserve;
	sfe_top->hw_ops.release = cam_sfe_top_release;

	spin_lock_init(&top_priv->spin_lock);
	INIT_LIST_HEAD(&top_priv->common_data.free_payload_list);
	for (i = 0; i < CAM_SFE_EVT_MAX; i++) {
		INIT_LIST_HEAD(&top_priv->common_data.evt_payload[i].list);
		list_add_tail(&top_priv->common_data.evt_payload[i].list,
			&top_priv->common_data.free_payload_list);
	}

	*sfe_top_ptr = sfe_top;

	return rc;

deinit_resources:
	for (--i; i >= 0; i--) {
		top_priv->in_rsrc[i].start = NULL;
		top_priv->in_rsrc[i].stop  = NULL;
		top_priv->in_rsrc[i].process_cmd = NULL;
		top_priv->in_rsrc[i].top_half_handler = NULL;
		top_priv->in_rsrc[i].bottom_half_handler = NULL;

		if (!top_priv->in_rsrc[i].res_priv)
			continue;

		kfree(top_priv->in_rsrc[i].res_priv);
		top_priv->in_rsrc[i].res_priv = NULL;
		top_priv->in_rsrc[i].res_state =
			CAM_ISP_RESOURCE_STATE_UNAVAILABLE;
	}
free_top_priv:
	kfree(sfe_top->top_priv);
	sfe_top->top_priv = NULL;
free_sfe_top:
	kfree(sfe_top);
end:
	*sfe_top_ptr = NULL;
	return rc;
}

int cam_sfe_top_deinit(
	uint32_t             hw_version,
	struct cam_sfe_top **sfe_top_ptr)
{
	int i, rc = 0;
	unsigned long flags;
	struct cam_sfe_top      *sfe_top;
	struct cam_sfe_top_priv *top_priv;

	if (!sfe_top_ptr) {
		CAM_ERR(CAM_SFE, "Error Invalid input");
		return -ENODEV;
	}

	sfe_top = *sfe_top_ptr;
	if (!sfe_top) {
		CAM_ERR(CAM_SFE, "Error sfe top NULL");
		return -ENODEV;
	}

	top_priv = sfe_top->top_priv;
	if (!top_priv) {
		CAM_ERR(CAM_SFE, "Error sfe top priv NULL");
		rc = -ENODEV;
		goto free_sfe_top;
	}

	CAM_DBG(CAM_SFE,
		"Deinit SFE [%u] top with hw_version 0x%x",
		top_priv->common_data.hw_intf->hw_idx,
		hw_version);

	spin_lock_irqsave(&top_priv->spin_lock, flags);
	INIT_LIST_HEAD(&top_priv->common_data.free_payload_list);
		for (i = 0; i < CAM_SFE_EVT_MAX; i++)
			INIT_LIST_HEAD(
				&top_priv->common_data.evt_payload[i].list);
	spin_unlock_irqrestore(&top_priv->spin_lock, flags);

	for (i = 0; i < CAM_SFE_TOP_IN_PORT_MAX; i++) {
		top_priv->in_rsrc[i].res_state =
			CAM_ISP_RESOURCE_STATE_UNAVAILABLE;

		top_priv->in_rsrc[i].start = NULL;
		top_priv->in_rsrc[i].stop  = NULL;
		top_priv->in_rsrc[i].process_cmd = NULL;
		top_priv->in_rsrc[i].top_half_handler = NULL;
		top_priv->in_rsrc[i].bottom_half_handler = NULL;

		if (!top_priv->in_rsrc[i].res_priv) {
			CAM_ERR(CAM_SFE, "Error res_priv is NULL");
			continue;
		}

		kfree(top_priv->in_rsrc[i].res_priv);
		top_priv->in_rsrc[i].res_priv = NULL;
	}

	kfree(sfe_top->top_priv);

free_sfe_top:
	kfree(sfe_top);
	*sfe_top_ptr = NULL;

	return rc;
}


