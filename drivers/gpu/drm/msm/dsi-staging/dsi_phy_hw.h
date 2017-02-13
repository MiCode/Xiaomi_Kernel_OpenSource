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
 */

#ifndef _DSI_PHY_HW_H_
#define _DSI_PHY_HW_H_

#include "dsi_defs.h"

#define DSI_MAX_SETTINGS 8

/**
 * enum dsi_phy_version - DSI PHY version enumeration
 * @DSI_PHY_VERSION_UNKNOWN:    Unknown version.
 * @DSI_PHY_VERSION_1_0:        28nm-HPM.
 * @DSI_PHY_VERSION_2_0:        28nm-LPM.
 * @DSI_PHY_VERSION_3_0:        20nm.
 * @DSI_PHY_VERSION_4_0:        14nm.
 * @DSI_PHY_VERSION_MAX:
 */
enum dsi_phy_version {
	DSI_PHY_VERSION_UNKNOWN,
	DSI_PHY_VERSION_1_0, /* 28nm-HPM */
	DSI_PHY_VERSION_2_0, /* 28nm-LPM */
	DSI_PHY_VERSION_3_0, /* 20nm */
	DSI_PHY_VERSION_4_0, /* 14nm */
	DSI_PHY_VERSION_MAX
};

/**
 * enum dsi_phy_hw_features - features supported by DSI PHY hardware
 * @DSI_PHY_DPHY:        Supports DPHY
 * @DSI_PHY_CPHY:        Supports CPHY
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
 * @count_per_lane: Number of values per each lane.
 */
struct dsi_phy_per_lane_cfgs {
	u8 lane[DSI_LANE_MAX][DSI_MAX_SETTINGS];
	u32 count_per_lane;
};

/**
 * struct dsi_phy_cfg - DSI PHY configuration
 * @lanecfg:          Lane configuration settings.
 * @strength:         Strength settings for lanes.
 * @timing:           Timing parameters for lanes.
 * @regulators:       Regulator settings for lanes.
 * @pll_source:       PLL source.
 */
struct dsi_phy_cfg {
	struct dsi_phy_per_lane_cfgs lanecfg;
	struct dsi_phy_per_lane_cfgs strength;
	struct dsi_phy_per_lane_cfgs timing;
	struct dsi_phy_per_lane_cfgs regulators;
	enum dsi_phy_pll_source pll_source;
};

struct dsi_phy_hw;

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
	 */
	void (*disable)(struct dsi_phy_hw *phy);

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

#endif /* _DSI_PHY_HW_H_ */
