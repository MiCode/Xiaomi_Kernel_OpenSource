/*
 * Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _DSI_PHY_HW_H_
#define _DSI_PHY_HW_H_

#include "dsi_defs.h"

#define DSI_MAX_SETTINGS 8
#define DSI_PHY_TIMING_V3_SIZE 12
#define DSI_PHY_TIMING_V4_SIZE 14

/**
 * enum dsi_phy_version - DSI PHY version enumeration
 * @DSI_PHY_VERSION_UNKNOWN:    Unknown version.
 * @DSI_PHY_VERSION_0_0_HPM:    28nm-HPM.
 * @DSI_PHY_VERSION_0_0_LPM:    28nm-HPM.
 * @DSI_PHY_VERSION_1_0:        20nm
 * @DSI_PHY_VERSION_2_0:        14nm
 * @DSI_PHY_VERSION_3_0:        10nm
 * @DSI_PHY_VERSION_4_0:        7nm
 * @DSI_PHY_VERSION_MAX:
 */
enum dsi_phy_version {
	DSI_PHY_VERSION_UNKNOWN,
	DSI_PHY_VERSION_0_0_HPM, /* 28nm-HPM */
	DSI_PHY_VERSION_0_0_LPM, /* 28nm-LPM */
	DSI_PHY_VERSION_1_0, /* 20nm */
	DSI_PHY_VERSION_2_0, /* 14nm */
	DSI_PHY_VERSION_3_0, /* 10nm */
	DSI_PHY_VERSION_4_0, /* 7nm  */
	DSI_PHY_VERSION_MAX
};

/**
 * enum dsi_phy_hw_features - features supported by DSI PHY hardware
 * @DSI_PHY_DPHY:        Supports DPHY
 * @DSI_PHY_CPHY:        Supports CPHY
 * @DSI_PHY_MAX_FEATURES:
 */
enum dsi_phy_hw_features {
	DSI_PHY_DPHY,
	DSI_PHY_CPHY,
	DSI_PHY_MAX_FEATURES
};

/**
 * enum dsi_phy_pll_source - pll clock source for PHY.
 * @DSI_PLL_SOURCE_STANDALONE:    Clock is sourced from native PLL and is not
 *				  shared by other PHYs.
 * @DSI_PLL_SOURCE_NATIVE:        Clock is sourced from native PLL and is
 *				  shared by other PHYs.
 * @DSI_PLL_SOURCE_NON_NATIVE:    Clock is sourced from other PHYs.
 * @DSI_PLL_SOURCE_MAX:
 */
enum dsi_phy_pll_source {
	DSI_PLL_SOURCE_STANDALONE = 0,
	DSI_PLL_SOURCE_NATIVE,
	DSI_PLL_SOURCE_NON_NATIVE,
	DSI_PLL_SOURCE_MAX
};

/**
 * struct dsi_phy_per_lane_cfgs - Holds register values for PHY parameters
 * @lane:           A set of maximum 8 values for each lane.
 * @lane_v3:        A set of maximum 12 values for each lane.
 * @count_per_lane: Number of values per each lane.
 */
struct dsi_phy_per_lane_cfgs {
	u8 lane[DSI_LANE_MAX][DSI_MAX_SETTINGS];
	u8 lane_v3[DSI_PHY_TIMING_V3_SIZE];
	u8 lane_v4[DSI_PHY_TIMING_V4_SIZE];
	u32 count_per_lane;
};

/**
 * struct dsi_phy_cfg - DSI PHY configuration
 * @lanecfg:          Lane configuration settings.
 * @strength:         Strength settings for lanes.
 * @timing:           Timing parameters for lanes.
 * @is_phy_timing_present:	Boolean whether phy timings are defined.
 * @regulators:       Regulator settings for lanes.
 * @pll_source:       PLL source.
 * @lane_map:         DSI logical to PHY lane mapping.
 * @force_clk_lane_hs:Boolean whether to force clock lane in HS mode.
 */
struct dsi_phy_cfg {
	struct dsi_phy_per_lane_cfgs lanecfg;
	struct dsi_phy_per_lane_cfgs strength;
	struct dsi_phy_per_lane_cfgs timing;
	bool is_phy_timing_present;
	struct dsi_phy_per_lane_cfgs regulators;
	enum dsi_phy_pll_source pll_source;
	struct dsi_lane_map lane_map;
	bool force_clk_lane_hs;
};

struct dsi_phy_hw;

struct phy_ulps_config_ops {
	/**
	 * wait_for_lane_idle() - wait for DSI lanes to go to idle state
	 * @phy:           Pointer to DSI PHY hardware instance.
	 * @lanes:         ORed list of lanes (enum dsi_data_lanes) which need
	 *                 to be checked to be in idle state.
	 */
	int (*wait_for_lane_idle)(struct dsi_phy_hw *phy, u32 lanes);

	/**
	 * ulps_request() - request ulps entry for specified lanes
	 * @phy:           Pointer to DSI PHY hardware instance.
	 * @cfg:           Per lane configurations for timing, strength and lane
	 *	           configurations.
	 * @lanes:         ORed list of lanes (enum dsi_data_lanes) which need
	 *                 to enter ULPS.
	 *
	 * Caller should check if lanes are in ULPS mode by calling
	 * get_lanes_in_ulps() operation.
	 */
	void (*ulps_request)(struct dsi_phy_hw *phy,
			struct dsi_phy_cfg *cfg, u32 lanes);

	/**
	 * ulps_exit() - exit ULPS on specified lanes
	 * @phy:           Pointer to DSI PHY hardware instance.
	 * @cfg:           Per lane configurations for timing, strength and lane
	 *                 configurations.
	 * @lanes:         ORed list of lanes (enum dsi_data_lanes) which need
	 *                 to exit ULPS.
	 *
	 * Caller should check if lanes are in active mode by calling
	 * get_lanes_in_ulps() operation.
	 */
	void (*ulps_exit)(struct dsi_phy_hw *phy,
			struct dsi_phy_cfg *cfg, u32 lanes);

	/**
	 * get_lanes_in_ulps() - returns the list of lanes in ULPS mode
	 * @phy:           Pointer to DSI PHY hardware instance.
	 *
	 * Returns an ORed list of lanes (enum dsi_data_lanes) that are in ULPS
	 * state.
	 *
	 * Return: List of lanes in ULPS state.
	 */
	u32 (*get_lanes_in_ulps)(struct dsi_phy_hw *phy);

	/**
	 * is_lanes_in_ulps() - checks if the given lanes are in ulps
	 * @lanes:           lanes to be checked.
	 * @ulps_lanes:	   lanes in ulps currenly.
	 *
	 * Return: true if all the given lanes are in ulps; false otherwise.
	 */
	bool (*is_lanes_in_ulps)(u32 ulps, u32 ulps_lanes);
};

/**
 * struct dsi_phy_hw_ops - Operations for DSI PHY hardware.
 * @regulator_enable:          Enable PHY regulators.
 * @regulator_disable:         Disable PHY regulators.
 * @enable:                    Enable PHY.
 * @disable:                   Disable PHY.
 * @calculate_timing_params:   Calculate PHY timing params from mode information
 */
struct dsi_phy_hw_ops {
	/**
	 * regulator_enable() - enable regulators for DSI PHY
	 * @phy:      Pointer to DSI PHY hardware object.
	 * @reg_cfg:  Regulator configuration for all DSI lanes.
	 */
	void (*regulator_enable)(struct dsi_phy_hw *phy,
				 struct dsi_phy_per_lane_cfgs *reg_cfg);

	/**
	 * regulator_disable() - disable regulators
	 * @phy:      Pointer to DSI PHY hardware object.
	 */
	void (*regulator_disable)(struct dsi_phy_hw *phy);

	/**
	 * enable() - Enable PHY hardware
	 * @phy:      Pointer to DSI PHY hardware object.
	 * @cfg:      Per lane configurations for timing, strength and lane
	 *	      configurations.
	 */
	void (*enable)(struct dsi_phy_hw *phy, struct dsi_phy_cfg *cfg);

	/**
	 * disable() - Disable PHY hardware
	 * @phy:      Pointer to DSI PHY hardware object.
	 * @cfg:      Per lane configurations for timing, strength and lane
	 *	      configurations.
	 */
	void (*disable)(struct dsi_phy_hw *phy, struct dsi_phy_cfg *cfg);

	/**
	 * phy_idle_on() - Enable PHY hardware when entering idle screen
	 * @phy:      Pointer to DSI PHY hardware object.
	 * @cfg:      Per lane configurations for timing, strength and lane
	 *	      configurations.
	 */
	void (*phy_idle_on)(struct dsi_phy_hw *phy, struct dsi_phy_cfg *cfg);

	/**
	 * phy_idle_off() - Disable PHY hardware when exiting idle screen
	 * @phy:      Pointer to DSI PHY hardware object.
	 */
	void (*phy_idle_off)(struct dsi_phy_hw *phy);

	/**
	 * calculate_timing_params() - calculates timing parameters.
	 * @phy:      Pointer to DSI PHY hardware object.
	 * @mode:     Mode information for which timing has to be calculated.
	 * @config:   DSI host configuration for this mode.
	 * @timing:   Timing parameters for each lane which will be returned.
	 */
	int (*calculate_timing_params)(struct dsi_phy_hw *phy,
				       struct dsi_mode_info *mode,
				       struct dsi_host_common_cfg *config,
				       struct dsi_phy_per_lane_cfgs *timing);

	/**
	 * phy_timing_val() - Gets PHY timing values.
	 * @timing_val: Timing parameters for each lane which will be returned.
	 * @timing: Array containing PHY timing values
	 * @size: Size of the array
	 */
	int (*phy_timing_val)(struct dsi_phy_per_lane_cfgs *timing_val,
				u32 *timing, u32 size);

	/**
	 * phy_lane_reset() - Reset dsi phy lanes in case of error.
	 * @phy:      Pointer to DSI PHY hardware object.
	 * Return:    error code.
	 */
	int (*phy_lane_reset)(struct dsi_phy_hw *phy);

	/**
	 * toggle_resync_fifo() - toggle resync retime FIFO to sync data paths
	 * @phy:      Pointer to DSI PHY hardware object.
	 * Return:    error code.
	 */
	void (*toggle_resync_fifo)(struct dsi_phy_hw *phy);

	/**
	 * reset_clk_en_sel() - reset clk_en_sel on phy cmn_clk_cfg1 register
	 * @phy:      Pointer to DSI PHY hardware object.
	 */
	void (*reset_clk_en_sel)(struct dsi_phy_hw *phy);

	void *timing_ops;
	struct phy_ulps_config_ops ulps_ops;
};

/**
 * struct dsi_phy_hw - DSI phy hardware object specific to an instance
 * @base:                  VA for the DSI PHY base address.
 * @length:                Length of the DSI PHY register base map.
 * @index:                 Instance ID of the controller.
 * @version:               DSI PHY version.
 * @feature_map:           Features supported by DSI PHY.
 * @ops:                   Function pointer to PHY operations.
 */
struct dsi_phy_hw {
	void __iomem *base;
	u32 length;
	u32 index;

	enum dsi_phy_version version;

	DECLARE_BITMAP(feature_map, DSI_PHY_MAX_FEATURES);
	struct dsi_phy_hw_ops ops;
};

/**
 * dsi_phy_conv_phy_to_logical_lane() - Convert physical to logical lane
 * @lane_map:     logical lane
 * @phy_lane:     physical lane
 *
 * Return: Error code on failure. Lane number on success.
 */
int dsi_phy_conv_phy_to_logical_lane(
	struct dsi_lane_map *lane_map, enum dsi_phy_data_lanes phy_lane);

/**
 * dsi_phy_conv_logical_to_phy_lane() - Convert logical to physical lane
 * @lane_map:     physical lane
 * @lane:         logical lane
 *
 * Return: Error code on failure. Lane number on success.
 */
int dsi_phy_conv_logical_to_phy_lane(
	struct dsi_lane_map *lane_map, enum dsi_logical_lane lane);

#endif /* _DSI_PHY_HW_H_ */
