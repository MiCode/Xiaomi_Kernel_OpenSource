/*
 * Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _DSI_CATALOG_H_
#define _DSI_CATALOG_H_

#include "dsi_ctrl_hw.h"
#include "dsi_phy_hw.h"

/**
 * dsi_catalog_ctrl_setup() - return catalog info for dsi controller
 * @ctrl:        Pointer to DSI controller hw object.
 * @version:     DSI controller version.
 * @index:       DSI controller instance ID.
 *
 * This function setups the catalog information in the dsi_ctrl_hw object.
 *
 * return: error code for failure and 0 for success.
 */
int dsi_catalog_ctrl_setup(struct dsi_ctrl_hw *ctrl,
			   enum dsi_ctrl_version version,
			   u32 index);

/**
 * dsi_catalog_phy_setup() - return catalog info for dsi phy hardware
 * @ctrl:        Pointer to DSI PHY hw object.
 * @version:     DSI PHY version.
 * @index:       DSI PHY instance ID.
 *
 * This function setups the catalog information in the dsi_phy_hw object.
 *
 * return: error code for failure and 0 for success.
 */
int dsi_catalog_phy_setup(struct dsi_phy_hw *phy,
			  enum dsi_phy_version version,
			  u32 index);

/* Definitions for 4.0 PHY hardware driver */
void dsi_phy_hw_v4_0_regulator_enable(struct dsi_phy_hw *phy,
				      struct dsi_phy_per_lane_cfgs *cfg);
void dsi_phy_hw_v4_0_regulator_disable(struct dsi_phy_hw *phy);
void dsi_phy_hw_v4_0_enable(struct dsi_phy_hw *phy, struct dsi_phy_cfg *cfg);
void dsi_phy_hw_v4_0_disable(struct dsi_phy_hw *phy);
int dsi_phy_hw_v4_0_calculate_timing_params(struct dsi_phy_hw *phy,
					    struct dsi_mode_info *mode,
					    struct dsi_host_common_cfg *cfg,
					   struct dsi_phy_per_lane_cfgs
					   *timing);

/* Definitions for 1.4 controller hardware driver */
void dsi_ctrl_hw_14_host_setup(struct dsi_ctrl_hw *ctrl,
			       struct dsi_host_common_cfg *config);
void dsi_ctrl_hw_14_video_engine_en(struct dsi_ctrl_hw *ctrl, bool on);
void dsi_ctrl_hw_14_video_engine_setup(struct dsi_ctrl_hw *ctrl,
				       struct dsi_host_common_cfg *common_cfg,
				       struct dsi_video_engine_cfg *cfg);
void dsi_ctrl_hw_14_set_video_timing(struct dsi_ctrl_hw *ctrl,
			 struct dsi_mode_info *mode);

void dsi_ctrl_hw_14_cmd_engine_setup(struct dsi_ctrl_hw *ctrl,
				     struct dsi_host_common_cfg *common_cfg,
				     struct dsi_cmd_engine_cfg *cfg);

void dsi_ctrl_hw_14_ctrl_en(struct dsi_ctrl_hw *ctrl, bool on);
void dsi_ctrl_hw_14_cmd_engine_en(struct dsi_ctrl_hw *ctrl, bool on);

void dsi_ctrl_hw_14_setup_cmd_stream(struct dsi_ctrl_hw *ctrl,
				     u32 width_in_pixels,
				     u32 h_stride,
				     u32 height_in_lines,
				     u32 vc_id);
void dsi_ctrl_hw_14_phy_sw_reset(struct dsi_ctrl_hw *ctrl);
void dsi_ctrl_hw_14_soft_reset(struct dsi_ctrl_hw *ctrl);

void dsi_ctrl_hw_14_setup_lane_map(struct dsi_ctrl_hw *ctrl,
		       struct dsi_lane_mapping *lane_map);
void dsi_ctrl_hw_14_kickoff_command(struct dsi_ctrl_hw *ctrl,
			struct dsi_ctrl_cmd_dma_info *cmd,
			u32 flags);

void dsi_ctrl_hw_14_kickoff_fifo_command(struct dsi_ctrl_hw *ctrl,
			     struct dsi_ctrl_cmd_dma_fifo_info *cmd,
			     u32 flags);
void dsi_ctrl_hw_14_reset_cmd_fifo(struct dsi_ctrl_hw *ctrl);
void dsi_ctrl_hw_14_trigger_command_dma(struct dsi_ctrl_hw *ctrl);

void dsi_ctrl_hw_14_ulps_request(struct dsi_ctrl_hw *ctrl, u32 lanes);
void dsi_ctrl_hw_14_ulps_exit(struct dsi_ctrl_hw *ctrl, u32 lanes);
void dsi_ctrl_hw_14_clear_ulps_request(struct dsi_ctrl_hw *ctrl, u32 lanes);
u32 dsi_ctrl_hw_14_get_lanes_in_ulps(struct dsi_ctrl_hw *ctrl);

void dsi_ctrl_hw_14_clamp_enable(struct dsi_ctrl_hw *ctrl,
				 u32 lanes,
				 bool enable_ulps);

void dsi_ctrl_hw_14_clamp_disable(struct dsi_ctrl_hw *ctrl,
				  u32 lanes,
				  bool disable_ulps);
u32 dsi_ctrl_hw_14_get_interrupt_status(struct dsi_ctrl_hw *ctrl);
void dsi_ctrl_hw_14_clear_interrupt_status(struct dsi_ctrl_hw *ctrl, u32 ints);
void dsi_ctrl_hw_14_enable_status_interrupts(struct dsi_ctrl_hw *ctrl,
					     u32 ints);

u64 dsi_ctrl_hw_14_get_error_status(struct dsi_ctrl_hw *ctrl);
void dsi_ctrl_hw_14_clear_error_status(struct dsi_ctrl_hw *ctrl, u64 errors);
void dsi_ctrl_hw_14_enable_error_interrupts(struct dsi_ctrl_hw *ctrl,
					    u64 errors);

void dsi_ctrl_hw_14_video_test_pattern_setup(struct dsi_ctrl_hw *ctrl,
				 enum dsi_test_pattern type,
				 u32 init_val);
void dsi_ctrl_hw_14_cmd_test_pattern_setup(struct dsi_ctrl_hw *ctrl,
			       enum dsi_test_pattern  type,
			       u32 init_val,
			       u32 stream_id);
void dsi_ctrl_hw_14_test_pattern_enable(struct dsi_ctrl_hw *ctrl, bool enable);
void dsi_ctrl_hw_14_trigger_cmd_test_pattern(struct dsi_ctrl_hw *ctrl,
				 u32 stream_id);
ssize_t dsi_ctrl_hw_14_reg_dump_to_buffer(struct dsi_ctrl_hw *ctrl,
					  char *buf,
					  u32 size);
#endif /* _DSI_CATALOG_H_ */
