// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
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

struct cam_sfe_core_cfg {
	uint32_t   mode_sel;
	uint32_t   ops_mode_cfg;
	uint32_t   fs_mode_cfg;
};

struct cam_sfe_top_common_data {
	struct cam_hw_soc_info                  *soc_info;
	struct cam_hw_intf                      *hw_intf;
	struct cam_sfe_top_common_reg_offset    *common_reg;
	struct cam_sfe_top_common_reg_data      *common_reg_data;
	struct cam_irq_controller               *sfe_irq_controller;
	struct cam_sfe_top_irq_evt_payload       evt_payload[CAM_SFE_EVT_MAX];
	struct list_head                         free_payload_list;
};

struct cam_sfe_top_priv {
	struct cam_sfe_top_common_data  common_data;
	struct cam_isp_resource_node    in_rsrc[CAM_SFE_TOP_IN_PORT_MAX];
	uint32_t                        num_in_ports;
	unsigned long                   applied_clk_rate;
	unsigned long                   req_clk_rate[CAM_SFE_TOP_IN_PORT_MAX];
	uint32_t                        last_bw_counter;
	uint32_t                        last_clk_counter;
	uint64_t                        total_bw_applied;
	struct cam_axi_vote             agg_incoming_vote;
	struct cam_axi_vote             req_axi_vote[CAM_SFE_TOP_IN_PORT_MAX];
	struct cam_axi_vote             last_bw_vote[CAM_DELAY_CLK_BW_REDUCTION_NUM_REQ];
	uint64_t                        last_total_bw_vote[CAM_DELAY_CLK_BW_REDUCTION_NUM_REQ];
	uint64_t                        last_clk_vote[CAM_DELAY_CLK_BW_REDUCTION_NUM_REQ];
	enum cam_clk_bw_state           clk_state;
	enum cam_clk_bw_state           bw_state;
	enum cam_isp_bw_control_action  axi_vote_control[
		CAM_SFE_TOP_IN_PORT_MAX];
	struct cam_axi_vote             applied_axi_vote;
	struct cam_sfe_core_cfg         core_cfg;
	uint32_t                        sfe_debug_cfg;
	uint32_t                        sensor_sel_diag_cfg;
	uint32_t                        cc_testbus_sel_cfg;
	int                             error_irq_handle;
	uint16_t                        reserve_cnt;
	uint16_t                        start_stop_cnt;
	void                           *priv_per_stream;
	spinlock_t                      spin_lock;
	cam_hw_mgr_event_cb_func        event_cb;
	struct cam_sfe_wr_client_desc  *wr_client_desc;
	struct cam_sfe_top_hw_info     *hw_info;
	uint32_t                        num_clc_module;
	struct cam_sfe_top_debug_info  (*clc_dbg_mod_info)[CAM_SFE_TOP_DBG_REG_MAX][8];
	bool                            skip_clk_data_rst;
};

struct cam_sfe_path_data {
	void __iomem                             *mem_base;
	struct cam_hw_intf                       *hw_intf;
	struct cam_sfe_top_priv                  *top_priv;
	struct cam_sfe_top_common_reg_offset     *common_reg;
	struct cam_sfe_top_common_reg_data       *common_reg_data;
	struct cam_sfe_modules_common_reg_offset *modules_reg;
	struct cam_sfe_path_common_reg_data      *path_reg_data;
	struct cam_hw_soc_info                   *soc_info;
	uint32_t                                  min_hblank_cnt;
	int                                       sof_eof_handle;
};

static int cam_sfe_top_apply_clock_start_stop(struct cam_sfe_top_priv *top_priv);

static int cam_sfe_top_apply_bw_start_stop(struct cam_sfe_top_priv *top_priv);

static void cam_sfe_top_print_ipp_violation_info(struct cam_sfe_top_priv *top_priv,
	uint32_t violation_status);

static const char *cam_sfe_top_clk_bw_state_to_string(uint32_t state)
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

static int cam_sfe_top_set_axi_bw_vote(
	struct cam_sfe_top_priv *top_priv,
	struct cam_axi_vote *final_bw_vote, uint64_t total_bw_new_vote, bool start_stop,
	uint64_t request_id)
{
	int rc = 0;
	struct cam_hw_soc_info        *soc_info = NULL;
	struct cam_sfe_soc_private    *soc_private = NULL;

	soc_info = top_priv->common_data.soc_info;
	soc_private = (struct cam_sfe_soc_private *)soc_info->soc_private;

	CAM_DBG(CAM_PERF, "SFE:%d Sending final BW to cpas bw_state:%s bw_vote:%llu req_id:%ld",
		top_priv->common_data.hw_intf->hw_idx,
		cam_sfe_top_clk_bw_state_to_string(top_priv->bw_state),
		total_bw_new_vote, (start_stop ? -1 : request_id));
	rc = cam_cpas_update_axi_vote(soc_private->cpas_handle,
		final_bw_vote);
	if (!rc) {
		memcpy(&top_priv->applied_axi_vote,
			final_bw_vote,
			sizeof(struct cam_axi_vote));
		top_priv->total_bw_applied = total_bw_new_vote;
	} else {
		CAM_ERR(CAM_PERF, "BW request failed, rc=%d", rc);
	}

	return rc;

}

static int cam_sfe_top_set_hw_clk_rate(
	struct cam_sfe_top_priv *top_priv, uint64_t final_clk_rate, bool start_stop,
	uint64_t request_id)
{
	struct cam_hw_soc_info        *soc_info = NULL;
	struct cam_sfe_soc_private    *soc_private = NULL;
	struct cam_ahb_vote            ahb_vote;
	int rc = 0, clk_lvl = -1;

	soc_info = top_priv->common_data.soc_info;
	soc_private = (struct cam_sfe_soc_private *)soc_info->soc_private;

	CAM_DBG(CAM_PERF, "Applying SFE:%d Clock name=%s idx=%d clk=%llu req_id=%ld",
		top_priv->common_data.hw_intf->hw_idx, soc_info->clk_name[soc_info->src_clk_idx],
		soc_info->src_clk_idx, final_clk_rate, (start_stop ? -1 : request_id));

	rc = cam_soc_util_set_src_clk_rate(soc_info, final_clk_rate);
	if (!rc) {
		top_priv->applied_clk_rate = final_clk_rate;
		rc = cam_soc_util_get_clk_level(soc_info, final_clk_rate,
			soc_info->src_clk_idx, &clk_lvl);
		if (rc) {
			CAM_WARN(CAM_SFE,
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
		CAM_ERR(CAM_PERF, "SFE:%d Set Clock rate failed, rc=%d",
			top_priv->common_data.hw_intf->hw_idx, rc);
	}

end:
	return rc;
}

static void cam_sfe_top_check_module_status(
	uint32_t num_reg, uint32_t *reg_val,
	struct cam_sfe_top_debug_info (*status_list)[CAM_SFE_TOP_DBG_REG_MAX][8])
{
	bool found = false;
	uint32_t i, j, val = 0;
	size_t len = 0;
	uint8_t log_buf[1024];

	if (!status_list)
		return;

	for (i = 0; i < num_reg; i++) {
		/* Check for ideal values */
		if ((reg_val[i] == 0) || (reg_val[i] == 0x55555555))
			continue;

		for (j = 0; j < 8; j++) {
			val = reg_val[i] >> (*status_list)[i][j].shift;
			val &= 0xF;
			if (val == 0 || val == 5)
				continue;

			CAM_INFO_BUF(CAM_SFE, log_buf, 1024, &len, "%s [I:%u V:%u R:%u]",
				(*status_list)[i][j].clc_name,
				((val >> 2) & 1), ((val >> 1) & 1), (val & 1));
			found = true;
		}
		if (found)
			CAM_INFO_RATE_LIMIT(CAM_SFE, "Check config for Debug%u - %s",
				i, log_buf);
		len = 0;
		found = false;
		memset(log_buf, 0, sizeof(uint8_t)*1024);
	}
}

static void cam_sfe_top_print_cc_test_bus(
	struct cam_sfe_top_priv *top_priv)
{
	struct cam_sfe_top_common_data     *common_data = &top_priv->common_data;
	struct cam_hw_soc_info             *soc_info = common_data->soc_info;
	void __iomem                       *mem_base =
		soc_info->reg_map[SFE_CORE_BASE_IDX].mem_base;
	struct cam_sfe_top_cc_testbus_info *testbus_info;
	uint32_t dbg_testbus_reg = common_data->common_reg->top_debug_testbus_reg;
	uint32_t size = 0, reg_val = 0, val = 0, i = 0;
	size_t   len = 0;
	uint8_t  log_buf[1024];

	reg_val =  cam_io_r(mem_base +
		common_data->common_reg->top_debug[dbg_testbus_reg]);

	for (i = 0; i < top_priv->hw_info->num_of_testbus; i++) {
		if (top_priv->cc_testbus_sel_cfg ==
			top_priv->hw_info->test_bus_info[i].value) {
			size = top_priv->hw_info->test_bus_info[i].size;
			testbus_info =
				top_priv->hw_info->test_bus_info[i].testbus;
		       break;
		}
	}

	if (i == top_priv->hw_info->num_of_testbus) {
		CAM_WARN(CAM_SFE,
			"Unexpected value[%d] is programed in test_bus_ctrl reg",
			top_priv->cc_testbus_sel_cfg);
		return;
	}

	for (i = 0; i < size; i++) {
		val = reg_val >> testbus_info[i].shift;
		val &= testbus_info[i].mask;
		if (val)
			CAM_INFO_BUF(CAM_SFE, log_buf, 1024, &len, "%s [val:%u]",
				 testbus_info[i].clc_name, val);
	}

	CAM_INFO(CAM_SFE, "SFE_TOP_DEBUG_%d val %d  config %s",
		common_data->common_reg->top_debug_testbus_reg,
		reg_val, log_buf);
}

static void cam_sfe_top_print_debug_reg_info(
	struct cam_sfe_top_priv *top_priv)
{
	void __iomem                    *mem_base;
	struct cam_sfe_top_common_data  *common_data;
	struct cam_hw_soc_info          *soc_info;
	uint32_t                        *reg_val = NULL;
	uint32_t num_reg = 0;
	int i = 0, j;

	common_data = &top_priv->common_data;
	soc_info = common_data->soc_info;
	num_reg = common_data->common_reg->num_debug_registers;
	mem_base = soc_info->reg_map[SFE_CORE_BASE_IDX].mem_base;
	reg_val    = kcalloc(num_reg, sizeof(uint32_t), GFP_KERNEL);
	if (!reg_val)
		return;

	while (i < num_reg) {
		for (j = 0; j < 4 && i < num_reg; j++, i++) {
			reg_val[i] = cam_io_r(mem_base +
				common_data->common_reg->top_debug[i]);
		}
		CAM_INFO(CAM_SFE, "Debug%u: 0x%x Debug%u: 0x%x Debug%u: 0x%x Debug%u: 0x%x",
			(i - 4), reg_val[i - 4], (i - 3), reg_val[i - 3],
			(i - 2), reg_val[i - 2], (i - 1), reg_val[i - 1]);
	}

	cam_sfe_top_check_module_status(top_priv->num_clc_module,
		reg_val, top_priv->clc_dbg_mod_info);

	if (common_data->common_reg->top_cc_test_bus_supported &&
		top_priv->cc_testbus_sel_cfg)
		cam_sfe_top_print_cc_test_bus(top_priv);

	kfree(reg_val);
}

static struct cam_axi_vote *cam_sfe_top_delay_bw_reduction(
	struct cam_sfe_top_priv *top_priv,
	uint64_t *to_be_applied_bw)
{
	uint32_t i;
	int vote_idx = -1;
	uint64_t max_bw = 0;

	for (i = 0; i < CAM_DELAY_CLK_BW_REDUCTION_NUM_REQ; i++) {
		if (top_priv->last_total_bw_vote[i] > max_bw) {
			vote_idx = i;
			max_bw = top_priv->last_total_bw_vote[i];
		}
	}

	if (vote_idx < 0)
		return NULL;

	*to_be_applied_bw = max_bw;

	return &top_priv->last_bw_vote[vote_idx];
}

int cam_sfe_top_calc_axi_bw_vote(struct cam_sfe_top_priv *top_priv,
	bool start_stop, struct cam_axi_vote **to_be_applied_axi_vote,
	uint64_t *total_bw_new_vote, uint64_t request_id)
{
	int rc = 0;
	uint32_t i;
	uint32_t num_paths = 0;
	bool bw_unchanged = true;
	struct cam_axi_vote *final_bw_vote = NULL;

	memset(&top_priv->agg_incoming_vote, 0, sizeof(struct cam_axi_vote));
	for (i = 0; i < top_priv->num_in_ports; i++) {
		if (top_priv->axi_vote_control[i] ==
			CAM_ISP_BW_CONTROL_INCLUDE) {
			if (num_paths +
				top_priv->req_axi_vote[i].num_paths >
				CAM_CPAS_MAX_PATHS_PER_CLIENT) {
				CAM_ERR(CAM_PERF,
					"Required paths(%d) more than max(%d)",
					num_paths +
					top_priv->req_axi_vote[i].num_paths,
					CAM_CPAS_MAX_PATHS_PER_CLIENT);
				rc = -EINVAL;
				goto end;
			}

			memcpy(&top_priv->agg_incoming_vote.axi_path[num_paths],
				&top_priv->req_axi_vote[i].axi_path[0],
				top_priv->req_axi_vote[i].num_paths *
				sizeof(
				struct cam_axi_per_path_bw_vote));
			num_paths += top_priv->req_axi_vote[i].num_paths;
		}
	}

	top_priv->agg_incoming_vote.num_paths = num_paths;

	for (i = 0; i < top_priv->agg_incoming_vote.num_paths; i++) {
		CAM_DBG(CAM_PERF,
			"sfe[%d] : New BW Vote : counter[%d] [%s][%s] [%llu %llu %llu]",
			top_priv->common_data.hw_intf->hw_idx,
			top_priv->last_bw_counter,
			cam_cpas_axi_util_path_type_to_string(
			top_priv->agg_incoming_vote.axi_path[i].path_data_type),
			cam_cpas_axi_util_trans_type_to_string(
			top_priv->agg_incoming_vote.axi_path[i].transac_type),
			top_priv->agg_incoming_vote.axi_path[i].camnoc_bw,
			top_priv->agg_incoming_vote.axi_path[i].mnoc_ab_bw,
			top_priv->agg_incoming_vote.axi_path[i].mnoc_ib_bw);

		*total_bw_new_vote += top_priv->agg_incoming_vote.axi_path[i].camnoc_bw;
	}

	memcpy(&top_priv->last_bw_vote[top_priv->last_bw_counter], &top_priv->agg_incoming_vote,
		sizeof(struct cam_axi_vote));
	top_priv->last_total_bw_vote[top_priv->last_bw_counter] = *total_bw_new_vote;
	top_priv->last_bw_counter = (top_priv->last_bw_counter + 1) %
		CAM_DELAY_CLK_BW_REDUCTION_NUM_REQ;

	if (*total_bw_new_vote != top_priv->total_bw_applied)
		bw_unchanged = false;

	CAM_DBG(CAM_PERF,
		"applied_total=%lld, new_total=%lld unchanged=%d, start_stop=%d req_id=%ld",
		top_priv->total_bw_applied,
		*total_bw_new_vote, bw_unchanged, start_stop, (start_stop ? -1 : request_id));

	if (bw_unchanged) {
		CAM_DBG(CAM_PERF, "BW config unchanged");
		*to_be_applied_axi_vote = NULL;
		top_priv->bw_state = CAM_CLK_BW_STATE_UNCHANGED;
		goto end;
	}

	if (start_stop) {
		/* need to vote current request immediately */
		final_bw_vote = &top_priv->agg_incoming_vote;
		/* Reset everything, we can start afresh */
		memset(top_priv->last_bw_vote, 0, sizeof(struct cam_axi_vote) *
			CAM_DELAY_CLK_BW_REDUCTION_NUM_REQ);
		memset(top_priv->last_total_bw_vote, 0, sizeof(uint64_t) *
			CAM_DELAY_CLK_BW_REDUCTION_NUM_REQ);
		top_priv->last_bw_counter = 0;
		top_priv->last_bw_vote[top_priv->last_bw_counter] = top_priv->agg_incoming_vote;
		top_priv->last_total_bw_vote[top_priv->last_bw_counter] = *total_bw_new_vote;
		top_priv->last_bw_counter = (top_priv->last_bw_counter + 1) %
			CAM_DELAY_CLK_BW_REDUCTION_NUM_REQ;
	} else {
		/*
		 * Find max bw request in last few frames. This will the bw
		 * that we want to vote to CPAS now.
		 */
		final_bw_vote = cam_sfe_top_delay_bw_reduction(top_priv, total_bw_new_vote);
		if (!final_bw_vote) {
			CAM_ERR(CAM_PERF, "to_be_applied_axi_vote is NULL, req_id:%llu",
				request_id);
			top_priv->bw_state = CAM_CLK_BW_STATE_UNCHANGED;
			return 0;
		}
	}

	for (i = 0; i < final_bw_vote->num_paths; i++) {
		CAM_DBG(CAM_PERF,
			"sfe[%d] : Apply BW Vote : [%s][%s] [%llu %llu %llu]",
			top_priv->common_data.hw_intf->hw_idx,
			cam_cpas_axi_util_path_type_to_string(
			final_bw_vote->axi_path[i].path_data_type),
			cam_cpas_axi_util_trans_type_to_string(
			final_bw_vote->axi_path[i].transac_type),
			final_bw_vote->axi_path[i].camnoc_bw,
			final_bw_vote->axi_path[i].mnoc_ab_bw,
			final_bw_vote->axi_path[i].mnoc_ib_bw);
	}

	if (*total_bw_new_vote == top_priv->total_bw_applied) {
		CAM_DBG(CAM_PERF, "SFE:%d Final BW Unchanged after delay",
			top_priv->common_data.hw_intf->hw_idx);
		top_priv->bw_state = CAM_CLK_BW_STATE_UNCHANGED;
		*to_be_applied_axi_vote = NULL;
		goto end;
	} else if (*total_bw_new_vote > top_priv->total_bw_applied) {
		top_priv->bw_state = CAM_CLK_BW_STATE_INCREASE;
	} else {
		top_priv->bw_state = CAM_CLK_BW_STATE_DECREASE;
	}


	CAM_DBG(CAM_PERF,
		"sfe[%d] : Delayed update: applied_total=%lld new_total=%lld, start_stop=%d bw_state=%s req_id=%ld",
		top_priv->common_data.hw_intf->hw_idx, top_priv->total_bw_applied,
		total_bw_new_vote, start_stop,
		cam_sfe_top_clk_bw_state_to_string(top_priv->bw_state),
		(start_stop ? -1 : request_id));

	*to_be_applied_axi_vote = final_bw_vote;

end:
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
				CAM_ISP_BW_CONTROL_INCLUDE;
			break;
		}
	}

	return rc;
}

int cam_sfe_top_bw_control(struct cam_sfe_soc_private *soc_private,
	struct cam_sfe_top_priv *top_priv, void *cmd_args,
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
		rc = cam_sfe_top_apply_bw_start_stop(top_priv);
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

static inline void cam_sfe_top_delay_clk_reduction(
	struct cam_sfe_top_priv *top_priv,
	uint64_t *max_clk)
{
	int i;

	for (i = 0; i < CAM_DELAY_CLK_BW_REDUCTION_NUM_REQ; i++) {
		if (top_priv->last_clk_vote[i] > (*max_clk))
			*max_clk = top_priv->last_clk_vote[i];
	}
}

int cam_sfe_top_calc_hw_clk_rate(
	struct cam_sfe_top_priv *top_priv, bool start_stop,
	uint64_t                       *final_clk_rate, uint64_t request_id)
{
	int                            i, rc = 0;
	uint64_t                       max_req_clk_rate = 0;

	for (i = 0; i < top_priv->num_in_ports; i++) {
		if (top_priv->req_clk_rate[i] > max_req_clk_rate)
			max_req_clk_rate = top_priv->req_clk_rate[i];
	}

	if (start_stop && !top_priv->skip_clk_data_rst) {
		/* need to vote current clk immediately */
		*final_clk_rate = max_req_clk_rate;
		/* Reset everything, we can start afresh */
		memset(top_priv->last_clk_vote, 0, sizeof(uint64_t) *
			CAM_DELAY_CLK_BW_REDUCTION_NUM_REQ);
		top_priv->last_clk_counter = 0;
		top_priv->last_clk_vote[top_priv->last_clk_counter] =
			max_req_clk_rate;
		top_priv->last_clk_counter = (top_priv->last_clk_counter + 1) %
			CAM_DELAY_CLK_BW_REDUCTION_NUM_REQ;
	} else {
		top_priv->last_clk_vote[top_priv->last_clk_counter] =
			max_req_clk_rate;
		top_priv->last_clk_counter = (top_priv->last_clk_counter + 1) %
			CAM_DELAY_CLK_BW_REDUCTION_NUM_REQ;

		/* Find max clk request in last few requests */
		cam_sfe_top_delay_clk_reduction(top_priv, final_clk_rate);
		if (!(*final_clk_rate)) {
			CAM_ERR(CAM_PERF, "Final clock rate is zero");
			return -EINVAL;
		}
	}

	if (*final_clk_rate == top_priv->applied_clk_rate)
		top_priv->clk_state = CAM_CLK_BW_STATE_UNCHANGED;
	else if (*final_clk_rate > top_priv->applied_clk_rate)
		top_priv->clk_state = CAM_CLK_BW_STATE_INCREASE;
	else
		top_priv->clk_state = CAM_CLK_BW_STATE_DECREASE;

	CAM_DBG(CAM_PERF, "SFE:%d Clock state:%s hw_clk_rate:%llu req_id:%ld",
		top_priv->common_data.hw_intf->hw_idx,
		cam_sfe_top_clk_bw_state_to_string(top_priv->clk_state),
		top_priv->applied_clk_rate, (start_stop ? -1 : request_id));

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
	int i;

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

	return 0;
}

static int cam_sfe_set_top_debug(
	struct cam_sfe_top_priv *top_priv,
	void *cmd_args)
{
	struct cam_sfe_debug_cfg_params *debug_cfg;

	debug_cfg = (struct cam_sfe_debug_cfg_params *)cmd_args;
	if (!debug_cfg->cache_config) {
		top_priv->sfe_debug_cfg = debug_cfg->u.dbg_cfg.sfe_debug_cfg;
		top_priv->sensor_sel_diag_cfg = debug_cfg->u.dbg_cfg.sfe_sensor_sel;
	}

	return 0;
}

static int cam_sfe_top_handle_overflow(
	struct cam_sfe_top_priv *top_priv, uint32_t cmd_type)
{
	struct cam_sfe_top_common_data      *common_data;
	struct cam_hw_soc_info              *soc_info;
	uint32_t                             overflow_status, violation_status;
	uint32_t                             i = 0;

	common_data = &top_priv->common_data;
	soc_info = common_data->soc_info;

	overflow_status = cam_io_r(soc_info->reg_map[SFE_CORE_BASE_IDX].mem_base +
		top_priv->common_data.common_reg->bus_overflow_status);
	violation_status = cam_io_r(soc_info->reg_map[SFE_CORE_BASE_IDX].mem_base +
		top_priv->common_data.common_reg->ipp_violation_status);

	CAM_ERR_RATE_LIMIT(CAM_ISP,
		"SFE%d src_clk_rate:%luHz overflow:%s violation: %s",
		soc_info->index, soc_info->applied_src_clk_rate,
		CAM_BOOL_TO_YESNO(overflow_status), CAM_BOOL_TO_YESNO(violation_status));

	while (overflow_status) {
		if (overflow_status & 0x1)
			CAM_ERR(CAM_ISP, "SFE Overflow %s ",
				top_priv->wr_client_desc[i].desc);
		overflow_status = overflow_status >> 1;
		i++;
	}

	cam_sfe_top_print_ipp_violation_info(top_priv, violation_status);
	cam_sfe_top_print_debug_reg_info(top_priv);

	return 0;
}

static int cam_sfe_top_apply_clk_bw_update(struct cam_sfe_top_priv *top_priv,
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
		CAM_ERR(CAM_SFE, "Invalid arg size: %u", arg_size);
		return -EINVAL;
	}

	clk_bw_args = (struct cam_isp_apply_clk_bw_args *)cmd_args;
	request_id = clk_bw_args->request_id;
	hw_intf = clk_bw_args->hw_intf;
	if (!hw_intf) {
		CAM_ERR(CAM_SFE, "Invalid hw_intf");
		return -EINVAL;
	}

	hw_info = hw_intf->hw_priv;
	if (hw_info->hw_state != CAM_HW_STATE_POWER_UP) {
		CAM_DBG(CAM_PERF,
			"SFE:%d Not ready to set clocks yet :%d",
			hw_intf->hw_idx, hw_info->hw_state);
		goto end;
	}

	if (clk_bw_args->skip_clk_data_rst) {
		top_priv->skip_clk_data_rst = true;
		CAM_DBG(CAM_SFE, "SFE:%u requested to avoid clk data rst",
			hw_intf->hw_idx);
		return 0;
	}

	rc = cam_sfe_top_calc_hw_clk_rate(top_priv, false, &final_clk_rate, request_id);
	if (rc) {
		CAM_ERR(CAM_SFE,
			"SFE:%d Failed in calculating clock rate rc=%d",
			hw_intf->hw_idx, rc);
		goto end;
	}

	rc = cam_sfe_top_calc_axi_bw_vote(top_priv, false,
		&to_be_applied_axi_vote, &total_bw_new_vote, request_id);
	if (rc) {
		CAM_ERR(CAM_SFE, "SFE:%d Failed in calculating bw vote rc=%d",
			hw_intf->hw_idx, rc);
		goto end;
	}

	if ((!to_be_applied_axi_vote) && (top_priv->bw_state != CAM_CLK_BW_STATE_UNCHANGED)) {
		CAM_ERR(CAM_PERF, "SFE:%d Invalid BW vote for state:%s", hw_intf->hw_idx,
			cam_sfe_top_clk_bw_state_to_string(top_priv->bw_state));
	}

	CAM_DBG(CAM_PERF, "SFE:%d APPLY CLK/BW req_id:%ld clk_state:%s bw_state:%s ",
		hw_intf->hw_idx, request_id,
		cam_sfe_top_clk_bw_state_to_string(top_priv->clk_state),
		cam_sfe_top_clk_bw_state_to_string(top_priv->bw_state));

	/* Determine BW and clock voting sequence according to state */
	if ((top_priv->clk_state == CAM_CLK_BW_STATE_UNCHANGED) &&
		(top_priv->bw_state == CAM_CLK_BW_STATE_UNCHANGED)) {
		goto end;
	} else if (top_priv->clk_state == CAM_CLK_BW_STATE_UNCHANGED) {
		rc = cam_sfe_top_set_axi_bw_vote(top_priv, to_be_applied_axi_vote,
			total_bw_new_vote, false, request_id);
		if (rc) {
			CAM_ERR(CAM_SFE,
				"SFE:%d Failed in voting final bw:%llu clk_state:%s bw_state:%s",
				hw_intf->hw_idx, total_bw_new_vote,
				cam_sfe_top_clk_bw_state_to_string(top_priv->clk_state),
				cam_sfe_top_clk_bw_state_to_string(top_priv->bw_state));
			goto end;
		}
	} else if (top_priv->bw_state == CAM_CLK_BW_STATE_UNCHANGED) {
		rc = cam_sfe_top_set_hw_clk_rate(top_priv, final_clk_rate, false, request_id);
		if (rc) {
			CAM_ERR(CAM_SFE,
				"SFE:%d Failed in voting final clk:%llu clk_state:%s bw_state:%s",
				hw_intf->hw_idx, final_clk_rate,
				cam_sfe_top_clk_bw_state_to_string(top_priv->clk_state),
				cam_sfe_top_clk_bw_state_to_string(top_priv->bw_state));
			goto end;
		}
	} else if (top_priv->clk_state == CAM_CLK_BW_STATE_INCREASE) {
		/* Set BW first, followed by Clock */
		rc = cam_sfe_top_set_axi_bw_vote(top_priv, to_be_applied_axi_vote,
			total_bw_new_vote, false, request_id);
		if (rc) {
			CAM_ERR(CAM_SFE,
				"SFE:%d Failed in voting final bw:%llu clk_state:%s bw_state:%s",
				hw_intf->hw_idx, total_bw_new_vote,
				cam_sfe_top_clk_bw_state_to_string(top_priv->clk_state),
				cam_sfe_top_clk_bw_state_to_string(top_priv->bw_state));
			goto end;
		}

		rc = cam_sfe_top_set_hw_clk_rate(top_priv, final_clk_rate, false, request_id);
		if (rc) {
			CAM_ERR(CAM_SFE,
				"SFE:%d Failed in voting final clk:%llu clk_state:%s bw_state:%s",
				hw_intf->hw_idx, final_clk_rate,
				cam_sfe_top_clk_bw_state_to_string(top_priv->clk_state),
				cam_sfe_top_clk_bw_state_to_string(top_priv->bw_state));
			goto end;
		}
	} else if (top_priv->clk_state == CAM_CLK_BW_STATE_DECREASE) {
		/* Set Clock first, followed by BW */
		rc = cam_sfe_top_set_hw_clk_rate(top_priv, final_clk_rate, false, request_id);
		if (rc) {
			CAM_ERR(CAM_SFE,
				"SFE:%d Failed in voting final clk:%llu clk_state:%s bw_state:%s",
				hw_intf->hw_idx, final_clk_rate,
				cam_sfe_top_clk_bw_state_to_string(top_priv->clk_state),
				cam_sfe_top_clk_bw_state_to_string(top_priv->bw_state));
			goto end;
		}

		rc = cam_sfe_top_set_axi_bw_vote(top_priv, to_be_applied_axi_vote,
			total_bw_new_vote, false, request_id);
		if (rc) {
			CAM_ERR(CAM_SFE,
				"SFE:%d Failed in voting final bw:%llu clk_state:%s bw_state:%s",
				hw_intf->hw_idx, total_bw_new_vote,
				cam_sfe_top_clk_bw_state_to_string(top_priv->clk_state),
				cam_sfe_top_clk_bw_state_to_string(top_priv->bw_state));
			goto end;
		}
	} else {
		CAM_ERR(CAM_SFE, "Invalid state to apply CLK/BW clk_state:%s bw_state:%s",
			cam_sfe_top_clk_bw_state_to_string(top_priv->clk_state),
			cam_sfe_top_clk_bw_state_to_string(top_priv->bw_state));
		rc = -EINVAL;
		goto end;
	}

end:
	top_priv->clk_state = CAM_CLK_BW_STATE_INIT;
	top_priv->bw_state = CAM_CLK_BW_STATE_INIT;
	return rc;
}

static int cam_sfe_top_apply_clock_start_stop(struct cam_sfe_top_priv *top_priv)
{
	int rc = 0;
	uint64_t final_clk_rate = 0;

	rc = cam_sfe_top_calc_hw_clk_rate(top_priv, true, &final_clk_rate, 0);
	if (rc) {
		CAM_ERR(CAM_SFE,
			"SFE:%d Failed in calculating clock rate rc=%d",
			top_priv->common_data.hw_intf->hw_idx, rc);
		goto end;
	}

	if (top_priv->clk_state == CAM_CLK_BW_STATE_UNCHANGED)
		goto end;

	rc = cam_sfe_top_set_hw_clk_rate(top_priv, final_clk_rate, true, 0);
	if (rc) {
		CAM_ERR(CAM_SFE, "SFE:%d Failed in voting final clk:%llu clk_state:%s",
			top_priv->common_data.hw_intf->hw_idx, final_clk_rate,
			cam_sfe_top_clk_bw_state_to_string(top_priv->clk_state));
		goto end;
	}

end:
	top_priv->clk_state = CAM_CLK_BW_STATE_INIT;
	top_priv->skip_clk_data_rst = false;
	return rc;
}

static int cam_sfe_top_apply_bw_start_stop(struct cam_sfe_top_priv *top_priv)
{
	int rc = 0;
	uint64_t total_bw_new_vote = 0;
	struct cam_axi_vote *to_be_applied_axi_vote = NULL;

	rc = cam_sfe_top_calc_axi_bw_vote(top_priv, true,
		&to_be_applied_axi_vote, &total_bw_new_vote, 0);
	if (rc) {
		CAM_ERR(CAM_SFE, "SFE:%d Failed in calculating bw vote rc=%d",
			top_priv->common_data.hw_intf->hw_idx, rc);
		goto end;
	}

	if (top_priv->bw_state == CAM_CLK_BW_STATE_UNCHANGED)
		goto end;

	rc = cam_sfe_top_set_axi_bw_vote(top_priv, to_be_applied_axi_vote, total_bw_new_vote,
		true, 0);
	if (rc) {
		CAM_ERR(CAM_SFE, "SFE:%d Failed in voting final bw:%llu bw_state:%s",
			top_priv->common_data.hw_intf->hw_idx, total_bw_new_vote,
			cam_sfe_top_clk_bw_state_to_string(top_priv->bw_state));
		goto end;
	}

end:
	top_priv->bw_state = CAM_CLK_BW_STATE_INIT;
	return rc;
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
		rc = cam_sfe_top_bw_control(soc_private, top_priv, cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_CORE_CONFIG:
		rc = cam_sfe_top_core_cfg(top_priv, cmd_args,
			arg_size);
		break;
	case CAM_ISP_HW_CMD_SET_SFE_DEBUG_CFG:
		rc = cam_sfe_set_top_debug(top_priv, cmd_args);
		break;
	case CAM_ISP_HW_NOTIFY_OVERFLOW:
		rc = cam_sfe_top_handle_overflow(top_priv, cmd_type);
		break;
	case CAM_ISP_HW_CMD_APPLY_CLK_BW_UPDATE:
		rc = cam_sfe_top_apply_clk_bw_update(top_priv, cmd_args, arg_size);
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

	if (top_priv->reserve_cnt) {
		if (top_priv->priv_per_stream != args->priv) {
			CAM_ERR(CAM_SFE,
				"Acquiring same SFE[%u] HW res: %u for different streams");
			rc = -EINVAL;
			return rc;
		}
	}

	for (i = 0; i < CAM_SFE_TOP_IN_PORT_MAX; i++) {
		CAM_DBG(CAM_SFE, "i: %d res_id: %d state: %d", i,
			acquire_args->res_id, top_priv->in_rsrc[i].res_state);

		if ((top_priv->in_rsrc[i].res_id == acquire_args->res_id) &&
			(top_priv->in_rsrc[i].res_state ==
			CAM_ISP_RESOURCE_STATE_AVAILABLE)) {
			path_data = (struct cam_sfe_path_data *)
				top_priv->in_rsrc[i].res_priv;
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

	if (!rc) {
		top_priv->reserve_cnt++;
		top_priv->priv_per_stream = args->priv;
		top_priv->event_cb = args->event_cb;
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
	if (top_priv->reserve_cnt)
		top_priv->reserve_cnt--;

	if (!top_priv->reserve_cnt) {
		top_priv->priv_per_stream = NULL;
		top_priv->event_cb = NULL;
	}

	return 0;
}

static int cam_sfe_top_get_evt_payload(
	struct cam_sfe_top_priv                *top_priv,
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


static int cam_sfe_top_handle_err_irq_top_half(
	uint32_t evt_id,
	struct cam_irq_th_payload *th_payload)
{
	int rc = 0, i;
	uint32_t irq_status = 0;
	void __iomem *base = NULL;
	struct cam_sfe_top_priv            *top_priv;
	struct cam_sfe_top_irq_evt_payload *evt_payload;

	top_priv = th_payload->handler_priv;
	irq_status = th_payload->evt_status_arr[0];

	CAM_DBG(CAM_SFE, "Top error IRQ Received: 0x%x", irq_status);

	base = top_priv->common_data.soc_info->reg_map[SFE_CORE_BASE_IDX].mem_base;
	if (irq_status & top_priv->common_data.common_reg_data->error_irq_mask) {
		cam_irq_controller_disable_all(
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
		top_priv->common_data.common_reg->ipp_violation_status);

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

void cam_sfe_top_sel_frame_counter(
	uint32_t res_id, uint32_t *val,
	bool read_counter,
	struct cam_sfe_path_data *path_data)
{
	const uint32_t frame_cnt_shift = 0x4;
	uint32_t frame_cnt0 = 0, frame_cnt1 = 0;
	struct cam_sfe_top_common_reg_offset *common_reg = NULL;

	if (read_counter) {
		common_reg = path_data->common_reg;
		frame_cnt0 = cam_io_r(path_data->mem_base +
			common_reg->diag_sensor_frame_cnt_status0);
		frame_cnt1 = cam_io_r(path_data->mem_base +
			common_reg->diag_sensor_frame_cnt_status1);
	}

	switch (res_id) {
	case CAM_ISP_HW_SFE_IN_PIX:
		*val |= (1 << frame_cnt_shift);
		if (read_counter)
			CAM_INFO(CAM_SFE, "IPP frame_cnt 0x%x",
				frame_cnt0 & 0xFF);
		break;
	case CAM_ISP_HW_SFE_IN_RDI0:
		*val |= (1 << (frame_cnt_shift + 1));
		if (read_counter)
			CAM_INFO(CAM_SFE, "RDI0 frame_cnt 0x%x",
				(frame_cnt0 >> 16) & 0xFF);
		break;
	case CAM_ISP_HW_SFE_IN_RDI1:
		*val |= (1 << (frame_cnt_shift + 2));
		if (read_counter)
			CAM_INFO(CAM_SFE, "RDI1 frame_cnt 0x%x",
				(frame_cnt0 >> 24) & 0xFF);
		break;
	case CAM_ISP_HW_SFE_IN_RDI2:
		*val |= (1 << (frame_cnt_shift + 3));
		if (read_counter)
			CAM_INFO(CAM_SFE, "RDI2 frame_cnt 0x%x",
				frame_cnt1 & 0xFF);
		break;
	case CAM_ISP_HW_SFE_IN_RDI3:
		*val |= (1 << (frame_cnt_shift + 4));
		if (read_counter)
			CAM_INFO(CAM_SFE, "RDI3 frame_cnt 0x%x",
				(frame_cnt1 >> 16) & 0xFF);
		break;
	case CAM_ISP_HW_SFE_IN_RDI4:
		*val |= (1 << (frame_cnt_shift + 5));
		if (read_counter)
			CAM_INFO(CAM_SFE, "RDI4 frame_cnt 0x%x",
				(frame_cnt1 >> 24) & 0xFF);
		break;
	default:
		break;
	}
}

static void cam_sfe_top_print_ipp_violation_info(
	struct cam_sfe_top_priv *top_priv,
	uint32_t violation_status)
{
	struct cam_sfe_top_common_data *common_data = &top_priv->common_data;
	struct cam_hw_soc_info         *soc_info = common_data->soc_info;
	uint32_t val = violation_status;

	CAM_INFO(CAM_SFE, "SFE[%u] IPP Violation status 0x%x",
	     soc_info->index, val);

	if (top_priv->hw_info->ipp_module_desc)
		CAM_ERR(CAM_SFE, "SFE[%u] IPP Violation Module id: [%u %s]",
			soc_info->index,
			top_priv->hw_info->ipp_module_desc[val].id,
			top_priv->hw_info->ipp_module_desc[val].desc);

}

static void cam_sfe_top_print_top_irq_error(
	struct cam_sfe_top_priv *top_priv,
	uint32_t irq_status,
	uint32_t violation_status)
{
	uint32_t i = 0;

	for (i = 0; i < top_priv->hw_info->num_top_errors; i++) {
		if (top_priv->hw_info->top_err_desc[i].bitmask &
			irq_status) {
			CAM_ERR(CAM_SFE, "%s %s",
				top_priv->hw_info->top_err_desc[i].err_name,
				top_priv->hw_info->top_err_desc[i].desc);
		}
	}

	if (irq_status & top_priv->common_data.common_reg->ipp_violation_mask)
		cam_sfe_top_print_ipp_violation_info(top_priv, violation_status);

}

static int cam_sfe_top_handle_err_irq_bottom_half(
	void *handler_priv, void *evt_payload_priv)
{
	int i;
	uint32_t viol_sts;
	uint32_t irq_status[CAM_SFE_IRQ_REGISTERS_MAX] = {0};
	enum cam_sfe_hw_irq_status          ret = CAM_SFE_IRQ_STATUS_MAX;
	struct cam_isp_hw_event_info        evt_info;
	struct cam_sfe_top_priv            *top_priv = handler_priv;
	struct cam_sfe_top_irq_evt_payload *payload = evt_payload_priv;

	for (i = 0; i < CAM_SFE_IRQ_REGISTERS_MAX; i++)
		irq_status[i] = payload->irq_reg_val[i];

	evt_info.hw_idx = top_priv->common_data.soc_info->index;
	evt_info.hw_type = CAM_ISP_HW_TYPE_SFE;
	evt_info.res_type = CAM_ISP_RESOURCE_SFE_IN;
	evt_info.reg_val = 0;

	if (irq_status[0] &
		top_priv->common_data.common_reg_data->error_irq_mask) {
		struct cam_isp_hw_error_event_info err_evt_info;

		viol_sts = payload->violation_status;
		CAM_INFO(CAM_SFE, "Violation status 0x%x",
			viol_sts);
		cam_sfe_top_print_top_irq_error(top_priv,
			irq_status[0], viol_sts);
		err_evt_info.err_type = CAM_SFE_IRQ_STATUS_VIOLATION;
		evt_info.event_data = (void *)&err_evt_info;
		cam_sfe_top_print_debug_reg_info(top_priv);
		if (top_priv->event_cb)
			top_priv->event_cb(top_priv->priv_per_stream,
				CAM_ISP_HW_EVENT_ERROR, (void *)&evt_info);

		ret = CAM_SFE_IRQ_STATUS_VIOLATION;
	}

	cam_sfe_top_put_evt_payload(top_priv, &payload);

	return ret;
}

static int cam_sfe_top_handle_irq_bottom_half(
	void *handler_priv, void *evt_payload_priv)
{
	int i;
	uint32_t val0, val1, frame_cnt, offset0, offset1;
	uint32_t irq_status[CAM_SFE_IRQ_REGISTERS_MAX] = {0};
	enum cam_sfe_hw_irq_status          ret = CAM_SFE_IRQ_STATUS_MAX;
	struct cam_isp_resource_node       *res = handler_priv;
	struct cam_sfe_path_data           *path_data = res->res_priv;
	struct cam_sfe_top_priv            *top_priv = path_data->top_priv;
	struct cam_sfe_top_irq_evt_payload *payload = evt_payload_priv;

	for (i = 0; i < CAM_SFE_IRQ_REGISTERS_MAX; i++)
		irq_status[i] = payload->irq_reg_val[i];

	if (irq_status[0] & path_data->path_reg_data->subscribe_irq_mask) {
		if (irq_status[0] & path_data->path_reg_data->sof_irq_mask) {
			CAM_DBG(CAM_SFE, "SFE:%d Received %s SOF",
				res->hw_intf->hw_idx,
				res->res_name);
			offset0 = path_data->common_reg->diag_sensor_status_0;
			offset1 = path_data->common_reg->diag_sensor_status_1;
			/* check for any debug info at SOF */
			if (top_priv->sfe_debug_cfg &
				SFE_DEBUG_ENABLE_SENSOR_DIAG_INFO) {
				val0 =  cam_io_r(path_data->mem_base +
					offset0);
				val1 = cam_io_r(path_data->mem_base +
					offset1);
				CAM_INFO(CAM_SFE,
					"SFE:%d HBI: 0x%x VBI: 0x%x NEQ_HBI: %s HBI_MIN_ERR: %s",
					res->hw_intf->hw_idx, (val0 & 0x3FFF), val1,
					(val0 & (0x4000) ? "TRUE" : "FALSE"),
					(val0 & (0x8000) ? "TRUE" : "FALSE"));
			}

			if (top_priv->sfe_debug_cfg &
				SFE_DEBUG_ENABLE_FRAME_COUNTER)
				cam_sfe_top_sel_frame_counter(
					res->res_id, &frame_cnt,
					true, path_data);
			}

		if (irq_status[0] &
			path_data->path_reg_data->eof_irq_mask) {
			CAM_DBG(CAM_SFE, "SFE:%d Received %s EOF",
				res->hw_intf->hw_idx,
				res->res_name);
		}
		ret = CAM_SFE_IRQ_STATUS_SUCCESS;
	}

	cam_sfe_top_put_evt_payload(top_priv, &payload);

	return ret;
}

int cam_sfe_top_start(
	void *priv, void *start_args, uint32_t arg_size)
{
	int                                   rc = -EINVAL;
	uint32_t                              val = 0, diag_cfg = 0;
	bool                                  debug_cfg_enabled = false;
	struct cam_sfe_top_priv              *top_priv;
	struct cam_isp_resource_node         *sfe_res;
	struct cam_hw_info                   *hw_info = NULL;
	struct cam_sfe_path_data             *path_data;
	struct cam_hw_soc_info               *soc_info = NULL;
	struct cam_sfe_soc_private           *soc_private = NULL;
	uint32_t   error_mask[CAM_SFE_IRQ_REGISTERS_MAX];
	uint32_t   sof_eof_mask[CAM_SFE_IRQ_REGISTERS_MAX];
	uint32_t core_cfg = 0, i = 0;

	if (!priv || !start_args) {
		CAM_ERR(CAM_SFE, "Invalid args");
		return -EINVAL;
	}

	top_priv = (struct cam_sfe_top_priv *)priv;
	sfe_res = (struct cam_isp_resource_node *) start_args;
	soc_info = top_priv->common_data.soc_info;
	soc_private = soc_info->soc_private;

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
	rc = cam_sfe_top_apply_clock_start_stop(top_priv);
	if (rc) {
		CAM_ERR(CAM_SFE, "SFE:%d Failed in applying start clock rc:%d",
			hw_info->soc_info.index, rc);
		return rc;
	}

	rc = cam_sfe_top_apply_bw_start_stop(top_priv);
	if (rc) {
		CAM_ERR(CAM_SFE, "SFE:%d Failed in applying start bw rc:%d",
			hw_info->soc_info.index, rc);
		return rc;
	}

	core_cfg = cam_io_r_mb(path_data->mem_base +
			path_data->common_reg->core_cfg);

	/* core cfg updated via CDM */
	CAM_DBG(CAM_SFE, "SFE HW [%u] core_cfg: 0x%x",
		hw_info->soc_info.index, core_cfg);

	for (i = 0; i < path_data->common_reg->num_sfe_mode; i++) {
		if (path_data->common_reg->sfe_mode[i].value ==
			core_cfg) {
			CAM_DBG(CAM_SFE, "SFE HW [%u] : [%s]",
				hw_info->soc_info.index,
				path_data->common_reg->sfe_mode[i].desc);
			break;
		}
	}

	/* Enable debug cfg registers */
	cam_io_w(path_data->common_reg_data->top_debug_cfg_en,
		path_data->mem_base +
		path_data->common_reg->top_debug_cfg);

	/* Enables the context controller testbus*/
	if (path_data->common_reg->top_cc_test_bus_supported) {
		for (i = 0; i < top_priv->hw_info->num_of_testbus; i++) {
			if ((top_priv->sfe_debug_cfg &
				top_priv->hw_info->test_bus_info[i].debugfs_val) ||
				top_priv->hw_info->test_bus_info[i].enable) {
				top_priv->cc_testbus_sel_cfg =
					top_priv->hw_info->test_bus_info[i].value;
				break;
			}
		}

		if (top_priv->cc_testbus_sel_cfg)
			cam_io_w(top_priv->cc_testbus_sel_cfg,
				path_data->mem_base +
				path_data->common_reg->top_cc_test_bus_ctrl);
	}

	/* Enable sensor diag info */
	if (top_priv->sfe_debug_cfg &
		SFE_DEBUG_ENABLE_SENSOR_DIAG_INFO) {
		if ((top_priv->sensor_sel_diag_cfg) &&
			(top_priv->sensor_sel_diag_cfg <
			CAM_SFE_TOP_IN_PORT_MAX))
			val |= top_priv->sensor_sel_diag_cfg <<
				path_data->common_reg_data->sensor_sel_shift;
		debug_cfg_enabled = true;
	}

	if (top_priv->sfe_debug_cfg & SFE_DEBUG_ENABLE_FRAME_COUNTER) {
		cam_sfe_top_sel_frame_counter(sfe_res->res_id, &val,
			false, path_data);
		debug_cfg_enabled = true;
	}

	if (debug_cfg_enabled) {
		diag_cfg = cam_io_r(path_data->mem_base +
			path_data->common_reg->diag_config);
		diag_cfg |= val;
		diag_cfg |= path_data->common_reg_data->enable_diagnostic_hw;
		CAM_DBG(CAM_SFE, "Diag config 0x%x", diag_cfg);
		cam_io_w(diag_cfg,
			path_data->mem_base +
			path_data->common_reg->diag_config);
	}

	error_mask[0] = path_data->common_reg_data->error_irq_mask;
	/* Enable error IRQ by default once */
	if (!top_priv->error_irq_handle) {
		top_priv->error_irq_handle = cam_irq_controller_subscribe_irq(
			top_priv->common_data.sfe_irq_controller,
			CAM_IRQ_PRIORITY_0,
			error_mask,
			top_priv,
			cam_sfe_top_handle_err_irq_top_half,
			cam_sfe_top_handle_err_irq_bottom_half,
			sfe_res->tasklet_info,
			&tasklet_bh_api,
			CAM_IRQ_EVT_GROUP_0);

		if (top_priv->error_irq_handle < 1) {
			CAM_ERR(CAM_SFE, "Failed to subscribe Top IRQ");
			top_priv->error_irq_handle = 0;
			return -EFAULT;
		}
	}

	/* Remove after driver stabilizes */
	top_priv->sfe_debug_cfg |= SFE_DEBUG_ENABLE_SOF_EOF_IRQ;

	if ((top_priv->sfe_debug_cfg & SFE_DEBUG_ENABLE_SOF_EOF_IRQ) ||
		(debug_cfg_enabled)) {
		if (!path_data->sof_eof_handle) {
			sof_eof_mask[0] =
				path_data->path_reg_data->subscribe_irq_mask;
			path_data->sof_eof_handle =
				cam_irq_controller_subscribe_irq(
				top_priv->common_data.sfe_irq_controller,
				CAM_IRQ_PRIORITY_1,
				sof_eof_mask,
				sfe_res,
				cam_sfe_top_handle_irq_top_half,
				cam_sfe_top_handle_irq_bottom_half,
				sfe_res->tasklet_info,
				&tasklet_bh_api,
				CAM_IRQ_EVT_GROUP_0);

			if (path_data->sof_eof_handle < 1) {
				CAM_ERR(CAM_SFE,
					"Failed to subscribe SOF/EOF IRQ");
				path_data->sof_eof_handle = 0;
				return -EFAULT;
			}
		}
	}

	sfe_res->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;
	top_priv->start_stop_cnt++;
	return 0;
}

int cam_sfe_top_stop(
	void *priv, void *stop_args, uint32_t arg_size)
{
	int i;
	bool debug_cfg_disable = false;
	uint32_t val = 0, diag_cfg = 0;
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
			if (!top_priv->skip_clk_data_rst)
				top_priv->req_clk_rate[i] = 0;
			memset(&top_priv->req_axi_vote[i], 0,
				sizeof(struct cam_axi_vote));
			top_priv->axi_vote_control[i] =
				CAM_ISP_BW_CONTROL_EXCLUDE;
			break;
		}
	}

	if (path_data->sof_eof_handle > 0) {
		cam_irq_controller_unsubscribe_irq(
			top_priv->common_data.sfe_irq_controller,
			path_data->sof_eof_handle);
		path_data->sof_eof_handle = 0;
	}

	if (top_priv->sfe_debug_cfg & SFE_DEBUG_ENABLE_FRAME_COUNTER) {
		cam_sfe_top_sel_frame_counter(sfe_res->res_id, &val,
			false, path_data);
		debug_cfg_disable = true;
	}

	if (top_priv->start_stop_cnt)
		top_priv->start_stop_cnt--;

	if (!top_priv->start_stop_cnt &&
		((top_priv->sfe_debug_cfg &
		SFE_DEBUG_ENABLE_FRAME_COUNTER) ||
		(top_priv->sfe_debug_cfg &
		SFE_DEBUG_ENABLE_SENSOR_DIAG_INFO))) {
		val |= path_data->common_reg_data->enable_diagnostic_hw;
		debug_cfg_disable = true;
	}

	if (debug_cfg_disable) {
		diag_cfg = cam_io_r(path_data->mem_base +
			path_data->common_reg->diag_config);
		diag_cfg &= ~val;
		cam_io_w(diag_cfg,
			path_data->mem_base +
			path_data->common_reg->diag_config);
	}

	/*
	 * Reset clk rate & unsubscribe error irq
	 * when all resources are streamed off
	 */
	if (!top_priv->start_stop_cnt) {
		if (!top_priv->skip_clk_data_rst)
			top_priv->applied_clk_rate = 0;

		if (top_priv->error_irq_handle > 0) {
			cam_irq_controller_unsubscribe_irq(
				top_priv->common_data.sfe_irq_controller,
				top_priv->error_irq_handle);
			top_priv->error_irq_handle = 0;
		}
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

	top_priv->applied_clk_rate = 0;
	top_priv->reserve_cnt = 0;
	top_priv->start_stop_cnt = 0;
	top_priv->priv_per_stream = NULL;
	top_priv->event_cb = NULL;
	top_priv->num_in_ports = sfe_top_hw_info->num_inputs;
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
			path_data->common_reg_data =
				sfe_top_hw_info->common_reg_data;
			path_data->modules_reg =
				sfe_top_hw_info->modules_hw_info;
			path_data->top_priv = top_priv;
			path_data->hw_intf = hw_intf;
			path_data->soc_info = soc_info;
			scnprintf(top_priv->in_rsrc[i].res_name,
				CAM_ISP_RES_NAME_LEN, "PIX");
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

			scnprintf(top_priv->in_rsrc[i].res_name,
				CAM_ISP_RES_NAME_LEN, "RDI%d", j);

			top_priv->in_rsrc[i].res_priv = path_data;

			path_data->mem_base =
				soc_info->reg_map[SFE_CORE_BASE_IDX].mem_base;
			path_data->hw_intf = hw_intf;
			path_data->common_reg = sfe_top_hw_info->common_reg;
			path_data->common_reg_data =
				sfe_top_hw_info->common_reg_data;
			path_data->modules_reg =
				sfe_top_hw_info->modules_hw_info;
			path_data->soc_info = soc_info;
			path_data->top_priv = top_priv;
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
	top_priv->common_data.common_reg_data = sfe_top_hw_info->common_reg_data;
	top_priv->hw_info = sfe_top_hw_info;
	top_priv->wr_client_desc  = sfe_top_hw_info->wr_client_desc;
	top_priv->num_clc_module   = sfe_top_hw_info->num_clc_module;
	top_priv->clc_dbg_mod_info = sfe_top_hw_info->clc_dbg_mod_info;
	top_priv->sfe_debug_cfg = 0;
	top_priv->cc_testbus_sel_cfg = 0;

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


