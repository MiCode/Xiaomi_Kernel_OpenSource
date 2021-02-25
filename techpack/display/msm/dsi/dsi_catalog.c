// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/errno.h>

#include "dsi_catalog.h"

/**
 * dsi_catalog_cmn_init() - catalog init for dsi controller v1.4
 */
static void dsi_catalog_cmn_init(struct dsi_ctrl_hw *ctrl,
		enum dsi_ctrl_version version)
{
	/* common functions */
	ctrl->ops.host_setup             = dsi_ctrl_hw_cmn_host_setup;
	ctrl->ops.video_engine_en        = dsi_ctrl_hw_cmn_video_engine_en;
	ctrl->ops.video_engine_setup     = dsi_ctrl_hw_cmn_video_engine_setup;
	ctrl->ops.set_video_timing       = dsi_ctrl_hw_cmn_set_video_timing;
	ctrl->ops.set_timing_db          = dsi_ctrl_hw_cmn_set_timing_db;
	ctrl->ops.cmd_engine_setup       = dsi_ctrl_hw_cmn_cmd_engine_setup;
	ctrl->ops.setup_cmd_stream       = dsi_ctrl_hw_cmn_setup_cmd_stream;
	ctrl->ops.ctrl_en                = dsi_ctrl_hw_cmn_ctrl_en;
	ctrl->ops.cmd_engine_en          = dsi_ctrl_hw_cmn_cmd_engine_en;
	ctrl->ops.phy_sw_reset           = dsi_ctrl_hw_cmn_phy_sw_reset;
	ctrl->ops.soft_reset             = dsi_ctrl_hw_cmn_soft_reset;
	ctrl->ops.kickoff_command        = dsi_ctrl_hw_cmn_kickoff_command;
	ctrl->ops.kickoff_fifo_command   = dsi_ctrl_hw_cmn_kickoff_fifo_command;
	ctrl->ops.reset_cmd_fifo         = dsi_ctrl_hw_cmn_reset_cmd_fifo;
	ctrl->ops.trigger_command_dma    = dsi_ctrl_hw_cmn_trigger_command_dma;
	ctrl->ops.get_interrupt_status   = dsi_ctrl_hw_cmn_get_interrupt_status;
	ctrl->ops.get_error_status       = dsi_ctrl_hw_cmn_get_error_status;
	ctrl->ops.clear_error_status     = dsi_ctrl_hw_cmn_clear_error_status;
	ctrl->ops.clear_interrupt_status =
		dsi_ctrl_hw_cmn_clear_interrupt_status;
	ctrl->ops.enable_status_interrupts =
		dsi_ctrl_hw_cmn_enable_status_interrupts;
	ctrl->ops.enable_error_interrupts =
		dsi_ctrl_hw_cmn_enable_error_interrupts;
	ctrl->ops.video_test_pattern_setup =
		dsi_ctrl_hw_cmn_video_test_pattern_setup;
	ctrl->ops.cmd_test_pattern_setup =
		dsi_ctrl_hw_cmn_cmd_test_pattern_setup;
	ctrl->ops.test_pattern_enable    = dsi_ctrl_hw_cmn_test_pattern_enable;
	ctrl->ops.trigger_cmd_test_pattern =
		dsi_ctrl_hw_cmn_trigger_cmd_test_pattern;
	ctrl->ops.clear_phy0_ln_err = dsi_ctrl_hw_dln0_phy_err;
	ctrl->ops.phy_reset_config = dsi_ctrl_hw_cmn_phy_reset_config;
	ctrl->ops.setup_misr = dsi_ctrl_hw_cmn_setup_misr;
	ctrl->ops.collect_misr = dsi_ctrl_hw_cmn_collect_misr;
	ctrl->ops.get_cmd_read_data = dsi_ctrl_hw_cmn_get_cmd_read_data;
	ctrl->ops.clear_rdbk_register = dsi_ctrl_hw_cmn_clear_rdbk_reg;
	ctrl->ops.ctrl_reset = dsi_ctrl_hw_cmn_ctrl_reset;
	ctrl->ops.mask_error_intr = dsi_ctrl_hw_cmn_mask_error_intr;
	ctrl->ops.error_intr_ctrl = dsi_ctrl_hw_cmn_error_intr_ctrl;
	ctrl->ops.get_error_mask = dsi_ctrl_hw_cmn_get_error_mask;
	ctrl->ops.get_hw_version = dsi_ctrl_hw_cmn_get_hw_version;
	ctrl->ops.wait_for_cmd_mode_mdp_idle =
		dsi_ctrl_hw_cmn_wait_for_cmd_mode_mdp_idle;
	ctrl->ops.setup_avr = dsi_ctrl_hw_cmn_setup_avr;
	ctrl->ops.set_continuous_clk = dsi_ctrl_hw_cmn_set_continuous_clk;
	ctrl->ops.wait4dynamic_refresh_done =
		dsi_ctrl_hw_cmn_wait4dynamic_refresh_done;
	ctrl->ops.hs_req_sel = dsi_ctrl_hw_cmn_hs_req_sel;
	ctrl->ops.vid_engine_busy = dsi_ctrl_hw_cmn_vid_engine_busy;

	switch (version) {
	case DSI_CTRL_VERSION_1_4:
		ctrl->ops.setup_lane_map = dsi_ctrl_hw_14_setup_lane_map;
		ctrl->ops.ulps_ops.ulps_request = dsi_ctrl_hw_cmn_ulps_request;
		ctrl->ops.ulps_ops.ulps_exit = dsi_ctrl_hw_cmn_ulps_exit;
		ctrl->ops.wait_for_lane_idle =
			dsi_ctrl_hw_14_wait_for_lane_idle;
		ctrl->ops.ulps_ops.get_lanes_in_ulps =
			dsi_ctrl_hw_cmn_get_lanes_in_ulps;
		ctrl->ops.clamp_enable = dsi_ctrl_hw_14_clamp_enable;
		ctrl->ops.clamp_disable = dsi_ctrl_hw_14_clamp_disable;
		ctrl->ops.reg_dump_to_buffer =
			dsi_ctrl_hw_14_reg_dump_to_buffer;
		ctrl->ops.schedule_dma_cmd = NULL;
		ctrl->ops.kickoff_command_non_embedded_mode = NULL;
		ctrl->ops.config_clk_gating = NULL;
		ctrl->ops.configure_cmddma_window = NULL;
		ctrl->ops.reset_trig_ctrl = NULL;
		ctrl->ops.log_line_count = NULL;
		break;
	case DSI_CTRL_VERSION_2_0:
		ctrl->ops.setup_lane_map = dsi_ctrl_hw_20_setup_lane_map;
		ctrl->ops.wait_for_lane_idle =
			dsi_ctrl_hw_20_wait_for_lane_idle;
		ctrl->ops.reg_dump_to_buffer =
			dsi_ctrl_hw_20_reg_dump_to_buffer;
		ctrl->ops.ulps_ops.ulps_request = NULL;
		ctrl->ops.ulps_ops.ulps_exit = NULL;
		ctrl->ops.ulps_ops.get_lanes_in_ulps = NULL;
		ctrl->ops.clamp_enable = NULL;
		ctrl->ops.clamp_disable = NULL;
		ctrl->ops.schedule_dma_cmd = NULL;
		ctrl->ops.kickoff_command_non_embedded_mode = NULL;
		ctrl->ops.config_clk_gating = NULL;
		ctrl->ops.configure_cmddma_window = NULL;
		ctrl->ops.reset_trig_ctrl = NULL;
		ctrl->ops.log_line_count = NULL;
		break;
	case DSI_CTRL_VERSION_2_2:
	case DSI_CTRL_VERSION_2_3:
	case DSI_CTRL_VERSION_2_4:
	case DSI_CTRL_VERSION_2_5:
		ctrl->ops.phy_reset_config = dsi_ctrl_hw_22_phy_reset_config;
		ctrl->ops.config_clk_gating = dsi_ctrl_hw_22_config_clk_gating;
		ctrl->ops.setup_lane_map = dsi_ctrl_hw_22_setup_lane_map;
		ctrl->ops.wait_for_lane_idle =
			dsi_ctrl_hw_22_wait_for_lane_idle;
		ctrl->ops.reg_dump_to_buffer =
			dsi_ctrl_hw_22_reg_dump_to_buffer;
		ctrl->ops.ulps_ops.ulps_request = dsi_ctrl_hw_cmn_ulps_request;
		ctrl->ops.ulps_ops.ulps_exit = dsi_ctrl_hw_cmn_ulps_exit;
		ctrl->ops.ulps_ops.get_lanes_in_ulps =
			dsi_ctrl_hw_cmn_get_lanes_in_ulps;
		ctrl->ops.clamp_enable = NULL;
		ctrl->ops.clamp_disable = NULL;
		ctrl->ops.schedule_dma_cmd = dsi_ctrl_hw_22_schedule_dma_cmd;
		ctrl->ops.kickoff_command_non_embedded_mode =
			dsi_ctrl_hw_kickoff_non_embedded_mode;
		ctrl->ops.configure_cmddma_window =
			dsi_ctrl_hw_22_configure_cmddma_window;
		ctrl->ops.reset_trig_ctrl =
			dsi_ctrl_hw_22_reset_trigger_controls;
		ctrl->ops.log_line_count = dsi_ctrl_hw_22_log_line_count;
		break;
	default:
		break;
	}
}

/**
 * dsi_catalog_ctrl_setup() - return catalog info for dsi controller
 * @ctrl:        Pointer to DSI controller hw object.
 * @version:     DSI controller version.
 * @index:       DSI controller instance ID.
 * @phy_isolation_enabled:       DSI controller works isolated from phy.
 * @null_insertion_enabled:      DSI controller inserts null packet.
 *
 * This function setups the catalog information in the dsi_ctrl_hw object.
 *
 * return: error code for failure and 0 for success.
 */
int dsi_catalog_ctrl_setup(struct dsi_ctrl_hw *ctrl,
		   enum dsi_ctrl_version version, u32 index,
		   bool phy_isolation_enabled, bool null_insertion_enabled)
{
	int rc = 0;

	if (version == DSI_CTRL_VERSION_UNKNOWN ||
	    version >= DSI_CTRL_VERSION_MAX) {
		DSI_ERR("Unsupported version: %d\n", version);
		return -ENOTSUPP;
	}

	ctrl->index = index;
	ctrl->null_insertion_enabled = null_insertion_enabled;
	set_bit(DSI_CTRL_VIDEO_TPG, ctrl->feature_map);
	set_bit(DSI_CTRL_CMD_TPG, ctrl->feature_map);
	set_bit(DSI_CTRL_VARIABLE_REFRESH_RATE, ctrl->feature_map);
	set_bit(DSI_CTRL_DYNAMIC_REFRESH, ctrl->feature_map);
	set_bit(DSI_CTRL_DESKEW_CALIB, ctrl->feature_map);
	set_bit(DSI_CTRL_DPHY, ctrl->feature_map);

	switch (version) {
	case DSI_CTRL_VERSION_1_4:
		dsi_catalog_cmn_init(ctrl, version);
		break;
	case DSI_CTRL_VERSION_2_0:
	case DSI_CTRL_VERSION_2_2:
	case DSI_CTRL_VERSION_2_3:
	case DSI_CTRL_VERSION_2_4:
		ctrl->phy_isolation_enabled = phy_isolation_enabled;
		dsi_catalog_cmn_init(ctrl, version);
		break;
	case DSI_CTRL_VERSION_2_5:
		ctrl->widebus_support = true;
		ctrl->phy_isolation_enabled = phy_isolation_enabled;
		dsi_catalog_cmn_init(ctrl, version);
		break;
	default:
		return -ENOTSUPP;
	}

	return rc;
}

/**
 * dsi_catalog_phy_2_0_init() - catalog init for DSI PHY 14nm
 */
static void dsi_catalog_phy_2_0_init(struct dsi_phy_hw *phy)
{
	phy->ops.regulator_enable = dsi_phy_hw_v2_0_regulator_enable;
	phy->ops.regulator_disable = dsi_phy_hw_v2_0_regulator_disable;
	phy->ops.enable = dsi_phy_hw_v2_0_enable;
	phy->ops.disable = dsi_phy_hw_v2_0_disable;
	phy->ops.calculate_timing_params =
		dsi_phy_hw_calculate_timing_params;
	phy->ops.phy_idle_on = dsi_phy_hw_v2_0_idle_on;
	phy->ops.phy_idle_off = dsi_phy_hw_v2_0_idle_off;
	phy->ops.calculate_timing_params =
		dsi_phy_hw_calculate_timing_params;
	phy->ops.phy_timing_val = dsi_phy_hw_timing_val_v2_0;
	phy->ops.clamp_ctrl = dsi_phy_hw_v2_0_clamp_ctrl;
	phy->ops.dyn_refresh_ops.dyn_refresh_config =
		dsi_phy_hw_v2_0_dyn_refresh_config;
	phy->ops.dyn_refresh_ops.dyn_refresh_pipe_delay =
		dsi_phy_hw_v2_0_dyn_refresh_pipe_delay;
	phy->ops.dyn_refresh_ops.dyn_refresh_helper =
		dsi_phy_hw_v2_0_dyn_refresh_helper;
	phy->ops.dyn_refresh_ops.dyn_refresh_trigger_sel = NULL;
	phy->ops.dyn_refresh_ops.cache_phy_timings =
		dsi_phy_hw_v2_0_cache_phy_timings;
}

/**
 * dsi_catalog_phy_3_0_init() - catalog init for DSI PHY 10nm
 */
static void dsi_catalog_phy_3_0_init(struct dsi_phy_hw *phy)
{
	phy->ops.regulator_enable = dsi_phy_hw_v3_0_regulator_enable;
	phy->ops.regulator_disable = dsi_phy_hw_v3_0_regulator_disable;
	phy->ops.enable = dsi_phy_hw_v3_0_enable;
	phy->ops.disable = dsi_phy_hw_v3_0_disable;
	phy->ops.calculate_timing_params =
		dsi_phy_hw_calculate_timing_params;
	phy->ops.ulps_ops.wait_for_lane_idle =
		dsi_phy_hw_v3_0_wait_for_lane_idle;
	phy->ops.ulps_ops.ulps_request =
		dsi_phy_hw_v3_0_ulps_request;
	phy->ops.ulps_ops.ulps_exit =
		dsi_phy_hw_v3_0_ulps_exit;
	phy->ops.ulps_ops.get_lanes_in_ulps =
		dsi_phy_hw_v3_0_get_lanes_in_ulps;
	phy->ops.ulps_ops.is_lanes_in_ulps =
		dsi_phy_hw_v3_0_is_lanes_in_ulps;
	phy->ops.phy_timing_val = dsi_phy_hw_timing_val_v3_0;
	phy->ops.clamp_ctrl = dsi_phy_hw_v3_0_clamp_ctrl;
	phy->ops.phy_lane_reset = dsi_phy_hw_v3_0_lane_reset;
	phy->ops.toggle_resync_fifo = dsi_phy_hw_v3_0_toggle_resync_fifo;
	phy->ops.dyn_refresh_ops.dyn_refresh_config =
		dsi_phy_hw_v3_0_dyn_refresh_config;
	phy->ops.dyn_refresh_ops.dyn_refresh_pipe_delay =
		dsi_phy_hw_v3_0_dyn_refresh_pipe_delay;
	phy->ops.dyn_refresh_ops.dyn_refresh_helper =
		dsi_phy_hw_v3_0_dyn_refresh_helper;
	phy->ops.dyn_refresh_ops.dyn_refresh_trigger_sel = NULL;
	phy->ops.dyn_refresh_ops.cache_phy_timings =
		dsi_phy_hw_v3_0_cache_phy_timings;
}

/**
 * dsi_catalog_phy_4_0_init() - catalog init for DSI PHY 7nm
 */
static void dsi_catalog_phy_4_0_init(struct dsi_phy_hw *phy)
{
	phy->ops.regulator_enable = NULL;
	phy->ops.regulator_disable = NULL;
	phy->ops.enable = dsi_phy_hw_v4_0_enable;
	phy->ops.disable = dsi_phy_hw_v4_0_disable;
	phy->ops.calculate_timing_params =
		dsi_phy_hw_calculate_timing_params;
	phy->ops.ulps_ops.wait_for_lane_idle =
		dsi_phy_hw_v4_0_wait_for_lane_idle;
	phy->ops.ulps_ops.ulps_request =
		dsi_phy_hw_v4_0_ulps_request;
	phy->ops.ulps_ops.ulps_exit =
		dsi_phy_hw_v4_0_ulps_exit;
	phy->ops.ulps_ops.get_lanes_in_ulps =
		dsi_phy_hw_v4_0_get_lanes_in_ulps;
	phy->ops.ulps_ops.is_lanes_in_ulps =
		dsi_phy_hw_v4_0_is_lanes_in_ulps;
	phy->ops.phy_timing_val = dsi_phy_hw_timing_val_v4_0;
	phy->ops.phy_lane_reset = dsi_phy_hw_v4_0_lane_reset;
	phy->ops.toggle_resync_fifo = dsi_phy_hw_v4_0_toggle_resync_fifo;
	phy->ops.reset_clk_en_sel = dsi_phy_hw_v4_0_reset_clk_en_sel;

	phy->ops.dyn_refresh_ops.dyn_refresh_config =
		dsi_phy_hw_v4_0_dyn_refresh_config;
	phy->ops.dyn_refresh_ops.dyn_refresh_pipe_delay =
		dsi_phy_hw_v4_0_dyn_refresh_pipe_delay;
	phy->ops.dyn_refresh_ops.dyn_refresh_helper =
		dsi_phy_hw_v4_0_dyn_refresh_helper;
	phy->ops.dyn_refresh_ops.dyn_refresh_trigger_sel =
		dsi_phy_hw_v4_0_dyn_refresh_trigger_sel;
	phy->ops.dyn_refresh_ops.cache_phy_timings =
		dsi_phy_hw_v4_0_cache_phy_timings;
	phy->ops.set_continuous_clk = dsi_phy_hw_v4_0_set_continuous_clk;
	phy->ops.commit_phy_timing = dsi_phy_hw_v4_0_commit_phy_timing;
}

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
			  u32 index)
{
	int rc = 0;

	if (version == DSI_PHY_VERSION_UNKNOWN ||
	    version >= DSI_PHY_VERSION_MAX) {
		DSI_ERR("Unsupported version: %d\n", version);
		return -ENOTSUPP;
	}

	phy->index = index;
	phy->version = version;
	set_bit(DSI_PHY_DPHY, phy->feature_map);

	dsi_phy_timing_calc_init(phy, version);

	switch (version) {
	case DSI_PHY_VERSION_2_0:
		dsi_catalog_phy_2_0_init(phy);
		break;
	case DSI_PHY_VERSION_3_0:
		dsi_catalog_phy_3_0_init(phy);
		break;
	case DSI_PHY_VERSION_4_0:
	case DSI_PHY_VERSION_4_1:
	case DSI_PHY_VERSION_4_2:
		dsi_catalog_phy_4_0_init(phy);
		break;
	case DSI_PHY_VERSION_0_0_HPM:
	case DSI_PHY_VERSION_0_0_LPM:
	case DSI_PHY_VERSION_1_0:
	default:
		return -ENOTSUPP;
	}

	return rc;
}
