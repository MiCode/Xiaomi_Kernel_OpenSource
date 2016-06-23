/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#ifndef _DSI_PHY_H_
#define _DSI_PHY_H_

#include "dsi_defs.h"
#include "dsi_clk_pwr.h"
#include "dsi_phy_hw.h"

struct dsi_ver_spec_info {
	enum dsi_phy_version version;
	u32 lane_cfg_count;
	u32 strength_cfg_count;
	u32 regulator_cfg_count;
	u32 timing_cfg_count;
};

/**
 * struct dsi_phy_clk_info - clock information for DSI controller
 * @core_clks:         Core clocks needed to access PHY registers.
 */
struct dsi_phy_clk_info {
	struct dsi_core_clk_info core_clks;
};

/**
 * struct dsi_phy_power_info - digital and analog power supplies for DSI PHY
 * @digital:       Digital power supply for DSI PHY.
 * @phy_pwr:       Analog power supplies for DSI PHY to work.
 */
struct dsi_phy_power_info {
	struct dsi_regulator_info digital;
	struct dsi_regulator_info phy_pwr;
};

/**
 * struct msm_dsi_phy - DSI PHY object
 * @pdev:              Pointer to platform device.
 * @index:             Instance id.
 * @name:              Name of the PHY instance.
 * @refcount:          Reference count.
 * @phy_lock:          Mutex for hardware and object access.
 * @ver_info:          Version specific phy parameters.
 * @hw:                DSI PHY hardware object.
 * @cfg:               DSI phy configuration.
 * @power_state:       True if PHY is powered on.
 * @mode:              Current mode.
 * @data_lanes:        Number of data lanes used.
 * @dst_format:        Destination format.
 * @lane_map:          Map between logical and physical lanes.
 */
struct msm_dsi_phy {
	struct platform_device *pdev;
	int index;
	const char *name;
	u32 refcount;
	struct mutex phy_lock;

	const struct dsi_ver_spec_info *ver_info;
	struct dsi_phy_hw hw;

	struct dsi_phy_clk_info clks;
	struct dsi_phy_power_info pwr_info;

	struct dsi_phy_cfg cfg;

	bool power_state;
	struct dsi_mode_info mode;
	enum dsi_data_lanes data_lanes;
	enum dsi_pixel_format dst_format;
	struct dsi_lane_mapping lane_map;
};

/**
 * dsi_phy_get() - get a dsi phy handle from device node
 * @of_node:           device node for dsi phy controller
 *
 * Gets the DSI PHY handle for the corresponding of_node. The ref count is
 * incremented to one all subsequents get will fail until the original client
 * calls a put.
 *
 * Return: DSI PHY handle or an error code.
 */
struct msm_dsi_phy *dsi_phy_get(struct device_node *of_node);

/**
 * dsi_phy_put() - release dsi phy handle
 * @dsi_phy:              DSI PHY handle.
 *
 * Release the DSI PHY hardware. Driver will clean up all resources and puts
 * back the DSI PHY into reset state.
 */
void dsi_phy_put(struct msm_dsi_phy *dsi_phy);

/**
 * dsi_phy_drv_init() - initialize dsi phy driver
 * @dsi_phy:         DSI PHY handle.
 *
 * Initializes DSI PHY driver. Should be called after dsi_phy_get().
 *
 * Return: error code.
 */
int dsi_phy_drv_init(struct msm_dsi_phy *dsi_phy);

/**
 * dsi_phy_drv_deinit() - de-initialize dsi phy driver
 * @dsi_phy:          DSI PHY handle.
 *
 * Release all resources acquired by dsi_phy_drv_init().
 *
 * Return: error code.
 */
int dsi_phy_drv_deinit(struct msm_dsi_phy *dsi_phy);

/**
 * dsi_phy_validate_mode() - validate a display mode
 * @dsi_phy:            DSI PHY handle.
 * @mode:               Mode information.
 *
 * Validation will fail if the mode cannot be supported by the PHY driver or
 * hardware.
 *
 * Return: error code.
 */
int dsi_phy_validate_mode(struct msm_dsi_phy *dsi_phy,
			  struct dsi_mode_info *mode);

/**
 * dsi_phy_set_power_state() - enable/disable dsi phy power supplies
 * @dsi_phy:               DSI PHY handle.
 * @enable:                Boolean flag to enable/disable.
 *
 * Return: error code.
 */
int dsi_phy_set_power_state(struct msm_dsi_phy *dsi_phy, bool enable);

/**
 * dsi_phy_enable() - enable DSI PHY hardware
 * @dsi_phy:            DSI PHY handle.
 * @config:             DSI host configuration.
 * @pll_source:         Source PLL for PHY clock.
 * @skip_validation:    Validation will not be performed on parameters.
 *
 * Validates and enables DSI PHY.
 *
 * Return: error code.
 */
int dsi_phy_enable(struct msm_dsi_phy *dsi_phy,
		   struct dsi_host_config *config,
		   enum dsi_phy_pll_source pll_source,
		   bool skip_validation);

/**
 * dsi_phy_disable() - disable DSI PHY hardware.
 * @phy:        DSI PHY handle.
 *
 * Return: error code.
 */
int dsi_phy_disable(struct msm_dsi_phy *phy);

/**
 * dsi_phy_set_timing_params() - timing parameters for the panel
 * @phy:          DSI PHY handle
 * @timing:       array holding timing params.
 * @size:         size of the array.
 *
 * When PHY timing calculator is not implemented, this array will be used to
 * pass PHY timing information.
 *
 * Return: error code.
 */
int dsi_phy_set_timing_params(struct msm_dsi_phy *phy,
			      u8 *timing, u32 size);

/**
 * dsi_phy_drv_register() - register platform driver for dsi phy
 */
void dsi_phy_drv_register(void);

/**
 * dsi_phy_drv_unregister() - unregister platform driver
 */
void dsi_phy_drv_unregister(void);

#endif /* _DSI_PHY_H_ */
