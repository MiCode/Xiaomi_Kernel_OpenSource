/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_VFE_TOP_COMMON_H_
#define _CAM_VFE_TOP_COMMON_H_

#define CAM_VFE_TOP_MUX_MAX 6

#include "cam_cpas_api.h"
#include "cam_vfe_hw_intf.h"
#include "cam_vfe_soc.h"

#define CAM_VFE_TOP_MAX_REG_DUMP_ENTRIES 70

#define CAM_VFE_TOP_MAX_LUT_DUMP_ENTRIES 6

struct cam_vfe_top_priv_common {
	struct cam_isp_resource_node    mux_rsrc[CAM_VFE_TOP_MUX_MAX];
	uint32_t                        num_mux;
	uint32_t                        hw_idx;
	struct cam_axi_vote             applied_axi_vote;
	struct cam_axi_vote             agg_incoming_vote;
	struct cam_axi_vote             req_axi_vote[CAM_VFE_TOP_MUX_MAX];
	struct cam_axi_vote             last_bw_vote[CAM_DELAY_CLK_BW_REDUCTION_NUM_REQ];
	uint64_t                        last_total_bw_vote[CAM_DELAY_CLK_BW_REDUCTION_NUM_REQ];
	uint32_t                        last_bw_counter;
	uint64_t                        last_clk_vote[CAM_DELAY_CLK_BW_REDUCTION_NUM_REQ];
	uint32_t                        last_clk_counter;
	uint64_t                        total_bw_applied;
	enum cam_clk_bw_state           clk_state;
	enum cam_clk_bw_state           bw_state;
	uint32_t                        hw_version;
	enum cam_isp_bw_control_action  axi_vote_control[CAM_VFE_TOP_MUX_MAX];
	struct cam_hw_soc_info         *soc_info;
	unsigned long                   applied_clk_rate;
	unsigned long                   req_clk_rate[CAM_VFE_TOP_MUX_MAX];
	bool                            skip_data_rst_on_stop;

};

struct cam_vfe_top_reg_dump_entry {
	uint32_t reg_dump_start;
	uint32_t reg_dump_end;
};

struct cam_vfe_top_lut_dump_entry {
	uint32_t lut_word_size;
	uint32_t lut_bank_sel;
	uint32_t lut_addr_size;
};

struct cam_vfe_top_dump_data {
	uint32_t num_reg_dump_entries;
	uint32_t num_lut_dump_entries;
	uint32_t dmi_cfg;
	uint32_t dmi_addr;
	uint32_t dmi_data_path_hi;
	uint32_t dmi_data_path_lo;
	struct cam_vfe_top_reg_dump_entry
		reg_entry[CAM_VFE_TOP_MAX_REG_DUMP_ENTRIES];
	struct cam_vfe_top_lut_dump_entry
		lut_entry[CAM_VFE_TOP_MAX_LUT_DUMP_ENTRIES];
};

int cam_vfe_top_clock_update(struct cam_vfe_top_priv_common *top_common,
	void *cmd_args, uint32_t arg_size);

int cam_vfe_top_bw_update_v2(struct cam_vfe_soc_private *soc_private,
	struct cam_vfe_top_priv_common *top_common, void *cmd_args,
	uint32_t arg_size);

int cam_vfe_top_bw_update(struct cam_vfe_soc_private *soc_private,
	struct cam_vfe_top_priv_common *top_common, void *cmd_args,
	uint32_t arg_size);

int cam_vfe_top_bw_control(struct cam_vfe_soc_private *soc_private,
	struct cam_vfe_top_priv_common *top_common, void *cmd_args,
	uint32_t arg_size);

int cam_vfe_top_apply_clk_bw_update(
	struct cam_vfe_top_priv_common *top_common, void *cmd_args,
	uint32_t arg_size);

int cam_vfe_top_apply_clock_start_stop(struct cam_vfe_top_priv_common *top_common);

int cam_vfe_top_apply_bw_start_stop(struct cam_vfe_top_priv_common *top_common);

#endif /* _CAM_VFE_TOP_COMMON_H_ */
