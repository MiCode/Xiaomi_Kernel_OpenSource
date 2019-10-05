/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MTK_USB_PHY_NEW_H
#define __MTK_USB_PHY_NEW_H
#include <linux/types.h>
#include <linux/clk.h>

/* helpers for direct access thru low-level io interface */

#ifdef CONFIG_PHY_MTK_USB
extern int usb_mtkphy_switch_to_usb(struct phy *phy);
extern int usb_mtkphy_switch_to_uart(struct phy *phy);
extern int usb_mtkphy_check_in_uart_mode(struct phy *phy);
extern int usb_mtkphy_dump_usb2uart_reg(struct phy *phy);
extern int usb_mtkphy_u3_loop_back_test(struct phy *phy);
extern int usb_mtkphy_sib_enable_switch(struct phy *phy, bool enable);
extern int usb_mtkphy_sib_enable_switch_status(struct phy *phy);
extern int usb_mtkphy_switch_to_bc11(struct phy *phy, bool on);
extern int usb_mtkphy_dpdm_pulldown(struct phy *phy, bool enable);
extern int usb_mtkphy_lpm_enable(struct phy *phy, bool on);
extern int usb_mtkphy_host_mode(struct phy *phy, bool on);
extern int usb_mtkphy_io_read(struct phy *phy, u32 reg);
extern int usb_mtkphy_io_write(struct phy *phy, u32 val, u32 reg);
#else
static inline int usb_mtkphy_switch_to_usb(struct phy *phy)
{
	return -ENODEV;
}
static inline int usb_mtkphy_switch_to_uart(struct phy *phy)
{
	return -ENODEV;
}

static inline int usb_mtkphy_check_in_uart_mode(struct phy *phy)
{
	return -ENODEV;
}

static inline int usb_mtkphy_dump_usb2uart_reg(struct phy *phy)
{
	return -ENODEV;
}

static inline int usb_mtkphy_u3_loop_back_test(struct phy *phy)
{
	return -ENODEV;
}

static inline int usb_mtkphy_sib_enable_switch(struct phy *phy,
	bool enable)
{
	return -ENODEV;
}

static inline int usb_mtkphy_sib_enable_switch_status(struct phy *phy)
{
	return -ENODEV;
}

static inline int usb_mtkphy_switch_to_bc11(struct phy *phy, bool on)
{
	return -ENODEV;
}
static inline int usb_mtkphy_lpm_enable(struct phy *phy, bool on)
{
	return -ENODEV;
}
static inline int usb_mtkphy_host_mode(struct phy *phy, bool on)
{
	return -ENODEV;
}
static inline int usb_mtkphy_io_read(struct phy *phy, u32 reg)
{
	return -ENODEV;
}
static inline int usb_mtkphy_io_write(struct phy *phy,
	u32 val, u32 reg)
{
	return -ENODEV;
}

#endif

#ifdef CONFIG_MTK_UART_USB_SWITCH
enum PORT_MODE {
	PORT_MODE_USB = 0,
	PORT_MODE_UART,
	PORT_MODE_MAX
};

#define RG_GPIO_SELECT (0x600)
#define GPIO_SEL_OFFSET (0)
#define GPIO_SEL_MASK (0x3 << GPIO_SEL_OFFSET)
#define GPIO_SEL_UART0 (0x1 << GPIO_SEL_OFFSET)
#endif
#endif
