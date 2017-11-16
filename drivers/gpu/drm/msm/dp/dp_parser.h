/*
 * Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
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

#ifndef _DP_PARSER_H_
#define _DP_PARSER_H_

#include <linux/sde_io_util.h>

#define DP_LABEL "MDSS DP DISPLAY"
#define AUX_CFG_LEN	10
#define DP_MAX_PIXEL_CLK_KHZ	675000

enum dp_pm_type {
	DP_CORE_PM,
	DP_CTRL_PM,
	DP_PHY_PM,
	DP_MAX_PM
};

static inline const char *dp_parser_pm_name(enum dp_pm_type module)
{
	switch (module) {
	case DP_CORE_PM:	return "DP_CORE_PM";
	case DP_CTRL_PM:	return "DP_CTRL_PM";
	case DP_PHY_PM:		return "DP_PHY_PM";
	default:		return "???";
	}
}

/**
 * struct dp_display_data  - display related device tree data.
 *
 * @ctrl_node: referece to controller device
 * @phy_node:  reference to phy device
 * @is_active: is the controller currently active
 * @name: name of the display
 * @display_type: type of the display
 */
struct dp_display_data {
	struct device_node *ctrl_node;
	struct device_node *phy_node;
	bool is_active;
	const char *name;
	const char *display_type;
};

/**
 * struct dp_ctrl_resource - controller's IO related data
 *
 * @dp_ahb: controller's ahb mapped memory address
 * @dp_aux: controller's aux mapped memory address
 * @dp_link: controller's link mapped memory address
 * @dp_p0: controller's p0 mapped memory address
 * @phy_io: phy's mapped memory address
 * @ln_tx0_io: USB-DP lane TX0's mapped memory address
 * @ln_tx1_io: USB-DP lane TX1's mapped memory address
 * @dp_cc_io: DP cc's mapped memory address
 * @qfprom_io: qfprom's mapped memory address
 * @dp_pll_io: DP PLL mapped memory address
 * @usb3_dp_com: USB3 DP PHY combo mapped memory address
 * @hdcp_io: hdcp's mapped memory address
 */
struct dp_io {
	struct dss_io_data ctrl_io;
	struct dss_io_data dp_ahb;
	struct dss_io_data dp_aux;
	struct dss_io_data dp_link;
	struct dss_io_data dp_p0;
	struct dss_io_data phy_io;
	struct dss_io_data ln_tx0_io;
	struct dss_io_data ln_tx1_io;
	struct dss_io_data dp_cc_io;
	struct dss_io_data qfprom_io;
	struct dss_io_data dp_pll_io;
	struct dss_io_data usb3_dp_com;
	struct dss_io_data hdcp_io;
};

/**
 * struct dp_pinctrl - DP's pin control
 *
 * @pin: pin-controller's instance
 * @state_active: active state pin control
 * @state_hpd_active: hpd active state pin control
 * @state_suspend: suspend state pin control
 */
struct dp_pinctrl {
	struct pinctrl *pin;
	struct pinctrl_state *state_active;
	struct pinctrl_state *state_hpd_active;
	struct pinctrl_state *state_suspend;
};

#define DP_ENUM_STR(x)	#x
#define DP_AUX_CFG_MAX_VALUE_CNT 3
/**
 * struct dp_aux_cfg - DP's AUX configuration settings
 *
 * @cfg_cnt: count of the configurable settings for the AUX register
 * @current_index: current index of the AUX config lut
 * @offset: register offset of the AUX config register
 * @lut: look up table for the AUX config values for this register
 */
struct dp_aux_cfg {
	u32 cfg_cnt;
	u32 current_index;
	u32 offset;
	u32 lut[DP_AUX_CFG_MAX_VALUE_CNT];
};

/* PHY AUX config registers */
enum dp_phy_aux_config_type {
	PHY_AUX_CFG0,
	PHY_AUX_CFG1,
	PHY_AUX_CFG2,
	PHY_AUX_CFG3,
	PHY_AUX_CFG4,
	PHY_AUX_CFG5,
	PHY_AUX_CFG6,
	PHY_AUX_CFG7,
	PHY_AUX_CFG8,
	PHY_AUX_CFG9,
	PHY_AUX_CFG_MAX,
};

static inline char *dp_phy_aux_config_type_to_string(u32 cfg_type)
{
	switch (cfg_type) {
	case PHY_AUX_CFG0:
		return DP_ENUM_STR(PHY_AUX_CFG0);
	case PHY_AUX_CFG1:
		return DP_ENUM_STR(PHY_AUX_CFG1);
	case PHY_AUX_CFG2:
		return DP_ENUM_STR(PHY_AUX_CFG2);
	case PHY_AUX_CFG3:
		return DP_ENUM_STR(PHY_AUX_CFG3);
	case PHY_AUX_CFG4:
		return DP_ENUM_STR(PHY_AUX_CFG4);
	case PHY_AUX_CFG5:
		return DP_ENUM_STR(PHY_AUX_CFG5);
	case PHY_AUX_CFG6:
		return DP_ENUM_STR(PHY_AUX_CFG6);
	case PHY_AUX_CFG7:
		return DP_ENUM_STR(PHY_AUX_CFG7);
	case PHY_AUX_CFG8:
		return DP_ENUM_STR(PHY_AUX_CFG8);
	case PHY_AUX_CFG9:
		return DP_ENUM_STR(PHY_AUX_CFG9);
	default:
		return "unknown";
	}
}

/**
 * struct dp_parser - DP parser's data exposed to clients
 *
 * @pdev: platform data of the client
 * @mp: gpio, regulator and clock related data
 * @pinctrl: pin-control related data
 * @ctrl_resouce: controller's register address realated data
 * @disp_data: controller's display related data
 * @parse: function to be called by client to parse device tree.
 */
struct dp_parser {
	struct platform_device *pdev;
	struct dss_module_power mp[DP_MAX_PM];
	struct dp_pinctrl pinctrl;
	struct dp_io io;
	struct dp_display_data disp_data;

	u8 l_map[4];
	struct dp_aux_cfg aux_cfg[AUX_CFG_LEN];
	u32 max_pclk_khz;

	int (*parse)(struct dp_parser *parser);
};

/**
 * dp_parser_get() - get the DP's device tree parser module
 *
 * @pdev: platform data of the client
 * return: pointer to dp_parser structure.
 *
 * This function provides client capability to parse the
 * device tree and populate the data structures. The data
 * related to clock, regulators, pin-control and other
 * can be parsed using this module.
 */
struct dp_parser *dp_parser_get(struct platform_device *pdev);

/**
 * dp_parser_put() - cleans the dp_parser module
 *
 * @parser: pointer to the parser's data.
 */
void dp_parser_put(struct dp_parser *parser);
#endif
