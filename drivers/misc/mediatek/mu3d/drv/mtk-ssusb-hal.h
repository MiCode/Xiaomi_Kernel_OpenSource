/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef __MTK_SSUSB_HAL__H
#define __MTK_SSUSB_HAL__H

enum {
	USB_DPIDLE_ALLOWED = 0,
	USB_DPIDLE_FORBIDDEN,
	USB_DPIDLE_SRAM,
	USB_DPIDLE_TIMER,
	USB_DPIDLE_AUDIO_SRAM,
};

void usb20_rev6_setting(int value, bool is_update);
extern void enable_ipsleep_wakeup(void);
extern void disable_ipsleep_wakeup(void);
extern void usb_hal_dpidle_request(int mode);
extern void usb_audio_req(bool on);

#ifdef CONFIG_MTK_SIB_USB_SWITCH
extern void usb_phy_sib_enable_switch(bool enable);
extern bool usb_phy_sib_enable_switch_status(void);
#endif /*CONFIG_MTK_SIB_USB_SWITCH*/

#ifdef CONFIG_MTK_UART_USB_SWITCH
enum PORT_MODE {
	PORT_MODE_USB = 0,
	PORT_MODE_UART,

	PORT_MODE_MAX
};

extern bool in_uart_mode;
extern void uart_usb_switch_dump_register(void);
extern bool usb_phy_check_in_uart_mode(void);
extern void usb_phy_switch_to_usb(void);
extern void usb_phy_switch_to_uart(void);
extern u32 usb_phy_get_uart_path(void);
#endif
#endif
