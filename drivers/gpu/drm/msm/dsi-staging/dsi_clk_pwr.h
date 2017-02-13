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
 *
 */

#ifndef _DSI_CLK_PWR_H_
#define _DSI_CLK_PWR_H_

#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>

/**
 * struct dsi_vreg - regulator information for DSI regulators
 * @vreg:            Handle to the regulator.
 * @vreg_name:       Regulator name.
 * @min_voltage:     Minimum voltage in uV.
 * @max_voltage:     Maximum voltage in uV.
 * @enable_load:     Load, in uA, when enabled.
 * @disable_load:    Load, in uA, when disabled.
 * @pre_on_sleep:    Sleep, in ms, before enabling the regulator.
 * @post_on_sleep:   Sleep, in ms, after enabling the regulator.
 * @pre_off_sleep:   Sleep, in ms, before disabling the regulator.
 * @post_off_sleep:  Sleep, in ms, after disabling the regulator.
 */
struct dsi_vreg {
	struct regulator *vreg;
	char vreg_name[32];
	u32 min_voltage;
	u32 max_voltage;
	u32 enable_load;
	u32 disable_load;
	u32 pre_on_sleep;
	u32 post_on_sleep;
	u32 pre_off_sleep;
	u32 post_off_sleep;
};

/**
 * struct dsi_regulator_info - set of vregs that are turned on/off together.
 * @vregs:       Array of dsi_vreg structures.
 * @count:       Number of vregs.
 * @refcount:    Reference counting for enabling.
 */
struct dsi_regulator_info {
	struct dsi_vreg *vregs;
	u32 count;
	u32 refcount;
};

/**
 * struct dsi_core_clk_info - Core clock information for DSI hardware
 * @mdp_core_clk:        Handle to MDP core clock.
 * @iface_clk:           Handle to MDP interface clock.
 * @core_mmss_clk:       Handle to MMSS core clock.
 * @bus_clk:             Handle to bus clock.
 * @refcount:            Reference count for core clocks.
 * @clk_state:           Current clock state.
 */
struct dsi_core_clk_info {
	struct clk *mdp_core_clk;
	struct clk *iface_clk;
	struct clk *core_mmss_clk;
	struct clk *bus_clk;

	u32 refcount;
	u32 clk_state;
};

/**
 * struct dsi_link_clk_info - Link clock information for DSI hardware.
 * @byte_clk:        Handle to DSI byte clock.
 * @byte_clk_rate:   Frequency of DSI byte clock in KHz.
 * @pixel_clk:       Handle to DSI pixel clock.
 * @pixel_clk_rate:  Frequency of DSI pixel clock in KHz.
 * @esc_clk:         Handle to DSI escape clock.
 * @esc_clk_rate:    Frequency of DSI escape clock in KHz.
 * @refcount:        Reference count for link clocks.
 * @clk_state:       Current clock state.
 * @set_new_rate:    private flag used by clock utility.
 */
struct dsi_link_clk_info {
	struct clk *byte_clk;
	u64 byte_clk_rate;

	struct clk *pixel_clk;
	u64 pixel_clk_rate;

	struct clk *esc_clk;
	u64 esc_clk_rate;

	u32 refcount;
	u32 clk_state;
	bool set_new_rate;
};

/**
 * struct dsi_clk_link_set - Pair of clock handles to describe link clocks
 * @byte_clk:     Handle to DSi byte clock.
 * @pixel_clk:    Handle to DSI pixel clock.
 */
struct dsi_clk_link_set {
	struct clk *byte_clk;
	struct clk *pixel_clk;
};

/**
 * dsi_clk_pwr_of_get_vreg_data - parse regulator supply information
 * @of_node:        Device of node to parse for supply information.
 * @regs:           Pointer where regulator information will be copied to.
 * @supply_name:    Name of the supply node.
 *
 * return: error code in case of failure or 0 for success.
 */
int dsi_clk_pwr_of_get_vreg_data(struct device_node *of_node,
				 struct dsi_regulator_info *regs,
				 char *supply_name);

/**
 * dsi_clk_pwr_get_dt_vreg_data - parse regulator supply information
 * @dev:            Device whose of_node needs to be parsed.
 * @regs:           Pointer where regulator information will be copied to.
 * @supply_name:    Name of the supply node.
 *
 * return: error code in case of failure or 0 for success.
 */
int dsi_clk_pwr_get_dt_vreg_data(struct device *dev,
				 struct dsi_regulator_info *regs,
				 char *supply_name);

/**
 * dsi_pwr_enable_regulator() - enable a set of regulators
 * @regs:       Pointer to set of regulators to enable or disable.
 * @enable:     Enable/Disable regulators.
 *
 * return: error code in case of failure or 0 for success.
 */
int dsi_pwr_enable_regulator(struct dsi_regulator_info *regs, bool enable);

/**
 * dsi_clk_enable_core_clks() - enable DSI core clocks
 * @clks:      DSI core clock information.
 * @enable:    enable/disable DSI core clocks.
 *
 * A ref count is maintained, so caller should make sure disable and enable
 * calls are balanced.
 *
 * return: error code in case of failure or 0 for success.
 */
int dsi_clk_enable_core_clks(struct dsi_core_clk_info *clks, bool enable);

/**
 * dsi_clk_enable_link_clks() - enable DSI link clocks
 * @clks:      DSI link clock information.
 * @enable:    enable/disable DSI link clocks.
 *
 * A ref count is maintained, so caller should make sure disable and enable
 * calls are balanced.
 *
 * return: error code in case of failure or 0 for success.
 */
int dsi_clk_enable_link_clks(struct dsi_link_clk_info *clks, bool enable);

/**
 * dsi_clk_set_link_frequencies() - set frequencies for link clks
 * @clks:         Link clock information
 * @pixel_clk:    pixel clock frequency in KHz.
 * @byte_clk:     Byte clock frequency in KHz.
 * @esc_clk:      Escape clock frequency in KHz.
 *
 * return: error code in case of failure or 0 for success.
 */
int dsi_clk_set_link_frequencies(struct dsi_link_clk_info *clks,
				 u64 pixel_clk,
				 u64 byte_clk,
				 u64 esc_clk);

/**
 * dsi_clk_set_pixel_clk_rate() - set frequency for pixel clock
 * @clks:      DSI link clock information.
 * @pixel_clk: Pixel clock rate in KHz.
 *
 * return: error code in case of failure or 0 for success.
 */
int dsi_clk_set_pixel_clk_rate(struct dsi_link_clk_info *clks, u64 pixel_clk);

/**
 * dsi_clk_set_byte_clk_rate() - set frequency for byte clock
 * @clks:      DSI link clock information.
 * @byte_clk: Byte clock rate in KHz.
 *
 * return: error code in case of failure or 0 for success.
 */
int dsi_clk_set_byte_clk_rate(struct dsi_link_clk_info *clks, u64 byte_clk);

/**
 * dsi_clk_update_parent() - update parent clocks for specified clock
 * @parent:       link clock pair which are set as parent.
 * @child:        link clock pair whose parent has to be set.
 */
int dsi_clk_update_parent(struct dsi_clk_link_set *parent,
			  struct dsi_clk_link_set *child);
#endif /* _DSI_CLK_PWR_H_ */
