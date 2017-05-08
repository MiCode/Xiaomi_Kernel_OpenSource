/*
 * Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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
 * @phy_isolation_enabled:       DSI controller works isolated from phy.
 *
 * This function setups the catalog information in the dsi_ctrl_hw object.
 *
 * return: error code for failure and 0 for success.
 */
int dsi_catalog_ctrl_setup(struct dsi_ctrl_hw *ctrl,
		   enum dsi_ctrl_version version, u32 index,
		   bool phy_isolation_enabled);

/**
 * dsi_catalog_phy_setup() - return catalog info for dsi phy hardware
 * @phy:        Pointer to DSI PHY hw object.
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

/**
 * dsi_phy_timing_calc_init() - initialize info for DSI PHY timing calculations
 * @phy:        Pointer to DSI PHY hw object.
 * @version:     DSI PHY version.
 *
 * This function setups the catalog information in the dsi_phy_hw object.
 *
 * return: error code for failure and 0 for success.
 */
int dsi_phy_timing_calc_init(struct dsi_phy_hw *phy,
	enum dsi_phy_version version);

/**
 * dsi_phy_hw_calculate_timing_params() - DSI PHY timing parameter calculations
 * @phy:        Pointer to DSI PHY hw object.
 * @mode:       DSI mode information.
 * @host:       DSI host configuration.
 * @timing:     DSI phy lane configurations.
 *
 * This function setups the catalog information in the dsi_phy_hw object.
 *
 * return: error code for failure and 0 for success.
 */
int dsi_phy_hw_calculate_timing_params(struct dsi_phy_hw *phy,
					    struct dsi_mode_info *mode,
	struct dsi_host_common_cfg *host,
	struct dsi_phy_per_lane_cfgs *timing);

/* Definitions for 14nm PHY hardware driver */
void dsi_phy_hw_v2_0_regulator_enable(struct dsi_phy_hw *phy,
				      struct dsi_phy_per_lane_cfgs *cfg);
void dsi_phy_hw_v2_0_regulator_disable(struct dsi_phy_hw *phy);
void dsi_phy_hw_v2_0_enable(struct dsi_phy_hw *phy, struct dsi_phy_cfg *cfg);
void dsi_phy_hw_v2_0_disable(struct dsi_phy_hw *phy, struct dsi_phy_cfg *cfg);
void dsi_phy_hw_v2_0_idle_on(struct dsi_phy_hw *phy, struct dsi_phy_cfg *cfg);
void dsi_phy_hw_v2_0_idle_off(struct dsi_phy_hw *phy);
int dsi_phy_hw_timing_val_v2_0(struct dsi_phy_per_lane_cfgs *timing_cfg,
		u32 *timing_val, u32 size);

/* Definitions for 10nm PHY hardware driver */
void dsi_phy_hw_v3_0_regulator_enable(struct dsi_phy_hw *phy,
				      struct dsi_phy_per_lane_cfgs *cfg);
void dsi_phy_hw_v3_0_regulator_disable(struct dsi_phy_hw *phy);
void dsi_phy_hw_v3_0_enable(struct dsi_phy_hw *phy, struct dsi_phy_cfg *cfg);
void dsi_phy_hw_v3_0_disable(struct dsi_phy_hw *phy, struct dsi_phy_cfg *cfg);
int dsi_phy_hw_v3_0_wait_for_lane_idle(struct dsi_phy_hw *phy, u32 lanes);
void dsi_phy_hw_v3_0_ulps_request(struct dsi_phy_hw *phy,
		struct dsi_phy_cfg *cfg, u32 lanes);
void dsi_phy_hw_v3_0_ulps_exit(struct dsi_phy_hw *phy,
			struct dsi_phy_cfg *cfg, u32 lanes);
u32 dsi_phy_hw_v3_0_get_lanes_in_ulps(struct dsi_phy_hw *phy);
bool dsi_phy_hw_v3_0_is_lanes_in_ulps(u32 lanes, u32 ulps_lanes);
int dsi_phy_hw_timing_val_v3_0(struct dsi_phy_per_lane_cfgs *timing_cfg,
		u32 *timing_val, u32 size);

/* DSI controller common ops */
u32 dsi_ctrl_hw_cmn_get_interrupt_status(struct dsi_ctrl_hw *ctrl);
void dsi_ctrl_hw_cmn_debug_bus(struct dsi_ctrl_hw *ctrl);
void dsi_ctrl_hw_cmn_clear_interrupt_status(struct dsi_ctrl_hw *ctrl, u32 ints);
void dsi_ctrl_hw_cmn_enable_status_interrupts(struct dsi_ctrl_hw *ctrl,
					     u32 ints);

u64 dsi_ctrl_hw_cmn_get_error_status(struct dsi_ctrl_hw *ctrl);
void dsi_ctrl_hw_cmn_clear_error_status(struct dsi_ctrl_hw *ctrl, u64 errors);
void dsi_ctrl_hw_cmn_enable_error_interrupts(struct dsi_ctrl_hw *ctrl,
					    u64 errors);

void dsi_ctrl_hw_cmn_video_test_pattern_setup(struct dsi_ctrl_hw *ctrl,
				 enum dsi_test_pattern type,
				 u32 init_val);
void dsi_ctrl_hw_cmn_cmd_test_pattern_setup(struct dsi_ctrl_hw *ctrl,
			       enum dsi_test_pattern  type,
			       u32 init_val,
			       u32 stream_id);
void dsi_ctrl_hw_cmn_test_pattern_enable(struct dsi_ctrl_hw *ctrl, bool enable);
void dsi_ctrl_hw_cmn_trigger_cmd_test_pattern(struct dsi_ctrl_hw *ctrl,
				 u32 stream_id);

void dsi_ctrl_hw_cmn_host_setup(struct dsi_ctrl_hw *ctrl,
			       struct dsi_host_common_cfg *config);
void dsi_ctrl_hw_cmn_video_engine_en(struct dsi_ctrl_hw *ctrl, bool on);
void dsi_ctrl_hw_cmn_video_engine_setup(struct dsi_ctrl_hw *ctrl,
				       struct dsi_host_common_cfg *common_cfg,
				       struct dsi_video_engine_cfg *cfg);
void dsi_ctrl_hw_cmn_set_video_timing(struct dsi_ctrl_hw *ctrl,
			 struct dsi_mode_info *mode);
void dsi_ctrl_hw_cmn_set_timing_db(struct dsi_ctrl_hw *ctrl,
				     bool enable);
void dsi_ctrl_hw_cmn_cmd_engine_setup(struct dsi_ctrl_hw *ctrl,
				     struct dsi_host_common_cfg *common_cfg,
				     struct dsi_cmd_engine_cfg *cfg);

void dsi_ctrl_hw_cmn_ctrl_en(struct dsi_ctrl_hw *ctrl, bool on);
void dsi_ctrl_hw_cmn_cmd_engine_en(struct dsi_ctrl_hw *ctrl, bool on);

void dsi_ctrl_hw_cmn_setup_cmd_stream(struct dsi_ctrl_hw *ctrl,
				     struct dsi_mode_info *mode,
				     u32 h_stride,
				     u32 vc_id,
				     struct dsi_rect *roi);
void dsi_ctrl_hw_cmn_phy_sw_reset(struct dsi_ctrl_hw *ctrl);
void dsi_ctrl_hw_cmn_soft_reset(struct dsi_ctrl_hw *ctrl);

void dsi_ctrl_hw_cmn_setup_misr(struct dsi_ctrl_hw *ctrl,
			enum dsi_op_mode panel_mode,
			bool enable, u32 frame_count);
u32 dsi_ctrl_hw_cmn_collect_misr(struct dsi_ctrl_hw *ctrl,
			enum dsi_op_mode panel_mode);

void dsi_ctrl_hw_cmn_kickoff_command(struct dsi_ctrl_hw *ctrl,
			struct dsi_ctrl_cmd_dma_info *cmd,
			u32 flags);

void dsi_ctrl_hw_cmn_kickoff_fifo_command(struct dsi_ctrl_hw *ctrl,
			     struct dsi_ctrl_cmd_dma_fifo_info *cmd,
			     u32 flags);
void dsi_ctrl_hw_cmn_reset_cmd_fifo(struct dsi_ctrl_hw *ctrl);
void dsi_ctrl_hw_cmn_trigger_command_dma(struct dsi_ctrl_hw *ctrl);
void dsi_ctrl_hw_dln0_phy_err(struct dsi_ctrl_hw *ctrl);
void dsi_ctrl_hw_cmn_phy_reset_config(struct dsi_ctrl_hw *ctrl,
			bool enable);
void dsi_ctrl_hw_22_phy_reset_config(struct dsi_ctrl_hw *ctrl,
			bool enable);
u32 dsi_ctrl_hw_cmn_get_cmd_read_data(struct dsi_ctrl_hw *ctrl,
				     u8 *rd_buf,
				     u32 read_offset,
				     u32 rx_byte,
				     u32 pkt_size, u32 *hw_read_cnt);
void dsi_ctrl_hw_cmn_clear_rdbk_reg(struct dsi_ctrl_hw *ctrl);

/* Definitions specific to 1.4 DSI controller hardware */
int dsi_ctrl_hw_14_wait_for_lane_idle(struct dsi_ctrl_hw *ctrl, u32 lanes);
void dsi_ctrl_hw_14_setup_lane_map(struct dsi_ctrl_hw *ctrl,
		       struct dsi_lane_map *lane_map);
void dsi_ctrl_hw_14_ulps_request(struct dsi_ctrl_hw *ctrl, u32 lanes);
void dsi_ctrl_hw_14_ulps_exit(struct dsi_ctrl_hw *ctrl, u32 lanes);
u32 dsi_ctrl_hw_14_get_lanes_in_ulps(struct dsi_ctrl_hw *ctrl);

void dsi_ctrl_hw_14_clamp_enable(struct dsi_ctrl_hw *ctrl,
				 u32 lanes,
				 bool enable_ulps);

void dsi_ctrl_hw_14_clamp_disable(struct dsi_ctrl_hw *ctrl,
				  u32 lanes,
				  bool disable_ulps);
ssize_t dsi_ctrl_hw_14_reg_dump_to_buffer(struct dsi_ctrl_hw *ctrl,
					  char *buf,
					  u32 size);

/* Definitions specific to 2.0 DSI controller hardware */
void dsi_ctrl_hw_20_setup_lane_map(struct dsi_ctrl_hw *ctrl,
		       struct dsi_lane_map *lane_map);
int dsi_ctrl_hw_20_wait_for_lane_idle(struct dsi_ctrl_hw *ctrl, u32 lanes);
ssize_t dsi_ctrl_hw_20_reg_dump_to_buffer(struct dsi_ctrl_hw *ctrl,
					  char *buf,
					  u32 size);

#endif /* _DSI_CATALOG_H_ */
