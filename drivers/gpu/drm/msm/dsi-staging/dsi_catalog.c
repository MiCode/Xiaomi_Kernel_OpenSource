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

#define pr_fmt(fmt) "msm-dsi-catalog:[%s] " fmt, __func__
#include <linux/errno.h>

#include "dsi_catalog.h"

/**
 * dsi_catalog_14_init() - catalog init for dsi controller v1.4
 */
static void dsi_catalog_14_init(struct dsi_ctrl_hw *ctrl)
{
	ctrl->ops.host_setup             = dsi_ctrl_hw_14_host_setup;
	ctrl->ops.setup_lane_map         = dsi_ctrl_hw_14_setup_lane_map;
	ctrl->ops.video_engine_en        = dsi_ctrl_hw_14_video_engine_en;
	ctrl->ops.video_engine_setup     = dsi_ctrl_hw_14_video_engine_setup;
	ctrl->ops.set_video_timing       = dsi_ctrl_hw_14_set_video_timing;
	ctrl->ops.cmd_engine_setup       = dsi_ctrl_hw_14_cmd_engine_setup;
	ctrl->ops.setup_cmd_stream       = dsi_ctrl_hw_14_setup_cmd_stream;
	ctrl->ops.ctrl_en                = dsi_ctrl_hw_14_ctrl_en;
	ctrl->ops.cmd_engine_en          = dsi_ctrl_hw_14_cmd_engine_en;
	ctrl->ops.phy_sw_reset           = dsi_ctrl_hw_14_phy_sw_reset;
	ctrl->ops.soft_reset             = dsi_ctrl_hw_14_soft_reset;
	ctrl->ops.kickoff_command        = dsi_ctrl_hw_14_kickoff_command;
	ctrl->ops.kickoff_fifo_command   = dsi_ctrl_hw_14_kickoff_fifo_command;
	ctrl->ops.reset_cmd_fifo         = dsi_ctrl_hw_14_reset_cmd_fifo;
	ctrl->ops.trigger_command_dma    = dsi_ctrl_hw_14_trigger_command_dma;
	ctrl->ops.ulps_request           = dsi_ctrl_hw_14_ulps_request;
	ctrl->ops.ulps_exit              = dsi_ctrl_hw_14_ulps_exit;
	ctrl->ops.clear_ulps_request     = dsi_ctrl_hw_14_clear_ulps_request;
	ctrl->ops.get_lanes_in_ulps      = dsi_ctrl_hw_14_get_lanes_in_ulps;
	ctrl->ops.clamp_enable           = dsi_ctrl_hw_14_clamp_enable;
	ctrl->ops.clamp_disable          = dsi_ctrl_hw_14_clamp_disable;
	ctrl->ops.get_interrupt_status   = dsi_ctrl_hw_14_get_interrupt_status;
	ctrl->ops.get_error_status       = dsi_ctrl_hw_14_get_error_status;
	ctrl->ops.clear_error_status     = dsi_ctrl_hw_14_clear_error_status;
	ctrl->ops.clear_interrupt_status =
		dsi_ctrl_hw_14_clear_interrupt_status;
	ctrl->ops.enable_status_interrupts =
		dsi_ctrl_hw_14_enable_status_interrupts;
	ctrl->ops.enable_error_interrupts =
		dsi_ctrl_hw_14_enable_error_interrupts;
	ctrl->ops.video_test_pattern_setup =
		dsi_ctrl_hw_14_video_test_pattern_setup;
	ctrl->ops.cmd_test_pattern_setup =
		dsi_ctrl_hw_14_cmd_test_pattern_setup;
	ctrl->ops.test_pattern_enable    = dsi_ctrl_hw_14_test_pattern_enable;
	ctrl->ops.trigger_cmd_test_pattern =
		dsi_ctrl_hw_14_trigger_cmd_test_pattern;
	ctrl->ops.reg_dump_to_buffer    = dsi_ctrl_hw_14_reg_dump_to_buffer;
}

/**
 * dsi_catalog_20_init() - catalog init for dsi controller v2.0
 */
static void dsi_catalog_20_init(struct dsi_ctrl_hw *ctrl)
{
	set_bit(DSI_CTRL_CPHY, ctrl->feature_map);
}

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
			   u32 index)
{
	int rc = 0;

	if (version == DSI_CTRL_VERSION_UNKNOWN ||
	    version >= DSI_CTRL_VERSION_MAX) {
		pr_err("Unsupported version: %d\n", version);
		return -ENOTSUPP;
	}

	ctrl->index = index;
	set_bit(DSI_CTRL_VIDEO_TPG, ctrl->feature_map);
	set_bit(DSI_CTRL_CMD_TPG, ctrl->feature_map);
	set_bit(DSI_CTRL_VARIABLE_REFRESH_RATE, ctrl->feature_map);
	set_bit(DSI_CTRL_DYNAMIC_REFRESH, ctrl->feature_map);
	set_bit(DSI_CTRL_DESKEW_CALIB, ctrl->feature_map);
	set_bit(DSI_CTRL_DPHY, ctrl->feature_map);

	switch (version) {
	case DSI_CTRL_VERSION_1_4:
		dsi_catalog_14_init(ctrl);
		break;
	case DSI_CTRL_VERSION_2_0:
		dsi_catalog_20_init(ctrl);
		break;
	default:
		return -ENOTSUPP;
	}

	return rc;
}

/**
 * dsi_catalog_phy_4_0_init() - catalog init for DSI PHY v4.0
 */
static void dsi_catalog_phy_4_0_init(struct dsi_phy_hw *phy)
{
	phy->ops.regulator_enable = dsi_phy_hw_v4_0_regulator_enable;
	phy->ops.regulator_disable = dsi_phy_hw_v4_0_regulator_disable;
	phy->ops.enable = dsi_phy_hw_v4_0_enable;
	phy->ops.disable = dsi_phy_hw_v4_0_disable;
	phy->ops.calculate_timing_params =
		dsi_phy_hw_v4_0_calculate_timing_params;
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
		pr_err("Unsupported version: %d\n", version);
		return -ENOTSUPP;
	}

	phy->index = index;
	set_bit(DSI_PHY_DPHY, phy->feature_map);

	switch (version) {
	case DSI_PHY_VERSION_4_0:
		dsi_catalog_phy_4_0_init(phy);
		break;
	case DSI_PHY_VERSION_1_0:
	case DSI_PHY_VERSION_2_0:
	case DSI_PHY_VERSION_3_0:
	default:
		return -ENOTSUPP;
	}

	return rc;
}


