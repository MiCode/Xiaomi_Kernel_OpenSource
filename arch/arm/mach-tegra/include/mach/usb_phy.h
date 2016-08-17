/*
 * arch/arm/mach-tegra/include/mach/usb_phy.h
 *
 * Copyright (C) 2010 Google, Inc.
 * Copyright (C) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __MACH_USB_PHY_H
#define __MACH_USB_PHY_H

/**
 * Tegra USB phy opaque handle
 */
struct tegra_usb_phy;

/**
 * Opens the usb phy associated to the USB platform device
 * tegra usb phy open must be called before accessing any phy APIs
 */
struct tegra_usb_phy *tegra_usb_phy_open(struct platform_device *pdev);

/**
 * Handles interrupts specific to the phy interface
 * Note: udc or ehci driver will handle the controller interrupts
 */
irqreturn_t tegra_usb_phy_irq(struct tegra_usb_phy *phy);

/**
 * Handles phy interface specific functionality after driver reset
 */
int tegra_usb_phy_reset(struct tegra_usb_phy *phy);

/**
 * Handles phy interface specific functionality before driver suspend
 * Also, handles platform specific pre suspend functions
 */
int tegra_usb_phy_pre_suspend(struct tegra_usb_phy *phy);

/**
 * Handles phy interface specific functionality after driver suspend
 */
int tegra_usb_phy_post_suspend(struct tegra_usb_phy *phy);

/**
 * Handles phy interface specific functionality before driver resume
 * Also, handles platform specific pre resume functions
 */
int tegra_usb_phy_pre_resume(struct tegra_usb_phy *phy, bool remote_wakeup);

/**
 * Handles phy interface specific functionality after driver resume
 */
int tegra_usb_phy_post_resume(struct tegra_usb_phy *phy);

/**
 * Handles phy interface specific functionality during port power on
 */
int tegra_usb_phy_port_power(struct tegra_usb_phy *phy);

/**
 * Handles phy interface specific functionality during bus reset
 */
int tegra_usb_phy_bus_reset(struct tegra_usb_phy *phy);

/**
 * Handles phy interface specific functionality for turning off the phy to
 * put the phy in low power mode
 */
int tegra_usb_phy_power_off(struct tegra_usb_phy *phy);

/**
 * Handles phy interface specific functionality for turning on the phy to
 * bring phy out of low power mode
 */
int tegra_usb_phy_power_on(struct tegra_usb_phy *phy);

/**
 * Indicates whether phy registers are accessible or not
 * if phy is powered off then returns false else true
 */
bool tegra_usb_phy_hw_accessible(struct tegra_usb_phy *phy);

/**
 * Indicates whether compliance charger is connected or not
 * if compliance charger is detected then returns true else false
 */
bool tegra_usb_phy_charger_detected(struct tegra_usb_phy *phy);

/**
 * Indicates whether nvidia proprietary charger is connected or not
 * if nvidia proprietary charger is detected then returns true else false
 */
bool tegra_usb_phy_nv_charger_detected(struct tegra_usb_phy *phy);

/**
 * Indicates whether phy resumed due to the pmc remote/hotplug wake event
 *  or not, returns true if remote/hotplug wake is detected.
 */
bool tegra_usb_phy_pmc_wakeup(struct tegra_usb_phy *phy);

void tegra_usb_phy_memory_prefetch_on(struct tegra_usb_phy *phy);

void tegra_usb_phy_memory_prefetch_off(struct tegra_usb_phy *phy);

void tegra_usb_enable_vbus(struct tegra_usb_phy *phy, bool enable);

void tegra_usb_phy_pmc_disable(struct tegra_usb_phy *phy);

#endif /* __MACH_USB_PHY_H */
