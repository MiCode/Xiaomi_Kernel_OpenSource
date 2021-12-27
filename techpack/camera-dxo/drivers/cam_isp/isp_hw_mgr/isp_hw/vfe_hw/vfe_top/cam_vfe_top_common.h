/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#ifndef _CAM_VFE_TOP_COMMON_H_
#define _CAM_VFE_TOP_COMMON_H_

#define CAM_VFE_TOP_MUX_MAX 6
#define CAM_VFE_DELAY_BW_REDUCTION_NUM_FRAMES 18

#include "cam_cpas_api.h"
#include "cam_vfe_hw_intf.h"
#include "cam_vfe_soc.h"

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
	enum cam_vfe_bw_control_action  axi_vote_control[CAM_VFE_TOP_MUX_MAX];
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
