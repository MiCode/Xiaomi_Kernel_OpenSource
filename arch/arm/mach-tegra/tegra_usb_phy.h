/*
 * arch/arm/mach-tegra/include/mach/tegra_usb_phy.h
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MACH_TEGRA_USB_PHY_H
#define __MACH_TEGRA_USB_PHY_H

#include <linux/usb/otg.h>

/**
 * defines USB port speeds supported in USB2.0
 */
enum usb_phy_port_speed {
	USB_PHY_PORT_SPEED_FULL = 0,
	USB_PHY_PORT_SPEED_LOW,
	USB_PHY_PORT_SPEED_HIGH,
	USB_PHY_PORT_SPEED_UNKNOWN,
};

/**
 * defines structure for oscillator dependent parameters
 */
struct tegra_xtal_freq {
	int freq;
	u8 enable_delay;
	u8 stable_count;
	u8 active_delay;
	u16 xtal_freq_count;
	u16 debounce;
	u8 pdtrk_count;
};

/**
 * pre decleration of the usb phy data structure
 */
struct tegra_usb_phy;

/**
 * defines function pointers used for differnt phy interfaces
 */
struct tegra_usb_phy_ops {
	int (*open)(struct tegra_usb_phy *phy);
	void (*close)(struct tegra_usb_phy *phy);
	int (*irq)(struct tegra_usb_phy *phy);
	int (*init)(struct tegra_usb_phy *phy);
	int (*reset)(struct tegra_usb_phy *phy);
	int (*pre_suspend)(struct tegra_usb_phy *phy);
	int (*suspend)(struct tegra_usb_phy *phy);
	int (*post_suspend)(struct tegra_usb_phy *phy);
	int (*pre_resume)(struct tegra_usb_phy *phy, bool remote_wakeup);
	int (*resume)(struct tegra_usb_phy *phy);
	int (*post_resume)(struct tegra_usb_phy *phy);
	int (*port_power)(struct tegra_usb_phy *phy);
	int (*bus_reset)(struct tegra_usb_phy *phy);
	int (*power_off)(struct tegra_usb_phy *phy);
	int (*power_on)(struct tegra_usb_phy *phy);
	bool (*charger_detect)(struct tegra_usb_phy *phy);
	bool (*nv_charger_detect)(struct tegra_usb_phy *phy);
	void (*pmc_disable) (struct tegra_usb_phy *phy);
};

/**
 * defines usb phy data structure
 */
struct tegra_usb_phy {
	/* Don't move below variable 'phy', from first place*/
	struct usb_phy phy;
	struct platform_device *pdev;
	struct tegra_usb_platform_data *pdata;
	struct clk *pllu_clk;
	struct clk *ctrlr_clk;
	struct clk *ulpi_clk;
	struct clk *utmi_pad_clk;
	struct clk *emc_clk;
	struct clk *sys_clk;
	struct regulator *vdd_reg;
	struct regulator *hsic_reg;
	struct regulator *vbus_reg;
	struct regulator *pllu_reg;
	struct tegra_usb_phy_ops *ops;
	struct tegra_xtal_freq *freq;
	struct usb_phy *ulpi_vp;
	enum usb_phy_port_speed port_speed;
	signed char utmi_xcvr_setup;
	void __iomem *regs;
	int inst;
	bool phy_clk_on;
	bool ctrl_clk_on;
	bool vdd_reg_on;
	bool phy_power_on;
	bool pmc_remote_wakeup;
	bool pmc_hotplug_wakeup;
	bool hw_accessible;
	bool ulpi_clk_padout_ena;
	bool pmc_sleepwalk;
	bool bus_reseting;
	bool linkphy_init;
	bool hot_plug;
	bool ctrlr_suspended;
};

int usb_phy_reg_status_wait(void __iomem *reg, u32 mask,
		u32 result, u32 timeout);

int tegra3_usb_phy_init_ops(struct tegra_usb_phy *phy);
int tegra2_usb_phy_init_ops(struct tegra_usb_phy *phy);
int tegra11x_usb_phy_init_ops(struct tegra_usb_phy *phy);


#endif /* __MACH_TEGRA_USB_PHY_H */
