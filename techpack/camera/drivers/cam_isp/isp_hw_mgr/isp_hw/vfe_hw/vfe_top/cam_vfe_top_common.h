/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_VFE_TOP_COMMON_H_
#define _CAM_VFE_TOP_COMMON_H_

#define CAM_VFE_TOP_MUX_MAX 6
#define CAM_VFE_DELAY_BW_REDUCTION_NUM_FRAMES 18

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
	struct cam_axi_vote             req_axi_vote[CAM_VFE_TOP_MUX_MAX];
	struct cam_axi_vote             last_vote[
					CAM_VFE_DELAY_BW_REDUCTION_NUM_FRAMES];
	uint32_t                        last_counter;
	uint64_t                        total_bw_applied;
	uint32_t                        hw_version;
	enum cam_vfe_bw_control_action  axi_vote_control[CAM_VFE_TOP_MUX_MAX];
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

int cam_vfe_top_set_axi_bw_vote(struct cam_vfe_soc_private *soc_private,
	struct cam_vfe_top_priv_common *top_common, bool start_stop);

int cam_vfe_top_bw_update_v2(struct cam_vfe_soc_private *soc_private,
	struct cam_vfe_top_priv_common *top_common, void *cmd_args,
	uint32_t arg_size);

int cam_vfe_top_bw_update(struct cam_vfe_soc_private *soc_private,
	struct cam_vfe_top_priv_common *top_common, void *cmd_args,
	uint32_t arg_size);

int cam_vfe_top_bw_control(struct cam_vfe_soc_private *soc_private,
	struct cam_vfe_top_priv_common *top_common, void *cmd_args,
	uint32_t arg_size);

#endif /* _CAM_VFE_TOP_COMMON_H_ */
