/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifndef USB_DP_H
#define USB_DP_H

const u8 eq_fg_sw_evb[8][6] = {
	/* Safe State */
	{0x0D, 0x0, 0x0, 0x0, 0x0, 0x58},
	/* Safe State */
	{0x0D, 0x0, 0x0, 0x0, 0x0, 0x58},
	/* 4 lane DP1.4 + AUX */
	{0x0D, 0x25, 0x25, 0x25, 0x25, 0x5C},
	/* 4 lane DP1.4 + AUX (flipped) */
	{0x0D, 0x25, 0x25, 0x25, 0x25, 0x5C},
	/* 1 lane USB3.x (AP_CH1) */
	{0x0D, 0x0, 0x0, 0x35, 0x25, 0x58},
	/* 1 lane USB3.x (AP_CH2) flipped */
	{0x0D, 0x25, 0x35, 0x0, 0x0, 0x58},
	/* USB3 (AP_CH1) + 2 lane DP1.4 (AP_CH2) + AUX */
	{0x0D, 0x25, 0x25, 0x35, 0x25, 0x5C},
	/* USB3 (AP_CH2) + 2 lane DP1.4 (AP_CH1) + AUX (flipped) */
	{0x0D, 0x25, 0x35, 0x25, 0x25, 0x5C}
};

const u8 eq_fg_sw_phone[8][6] = {
	/* Safe State */
	{0x0D, 0x0, 0x0, 0x0, 0x0, 0x58},
	/* Safe State */
	{0x0D, 0x0, 0x0, 0x0, 0x0, 0x58},
	/* 4 lane DP1.4 + AUX */
	{0x0D, 0x65, 0x65, 0x65, 0x65, 0x5C},
	/* 4 lane DP1.4 + AUX (flipped) */
	{0x0D, 0x65, 0x65, 0x65, 0x65, 0x5C},
	/* 1 lane USB3.x (AP_CH1) */
	{0x0D, 0x0, 0x0, 0x75, 0x25, 0x58},
	/* 1 lane USB3.x (AP_CH2) flipped */
	{0x0D, 0x25, 0x75, 0x0, 0x0, 0x58},
	/* USB3 (AP_CH1) + 2 lane DP1.4 (AP_CH2) + AUX */
	{0x0D, 0x65, 0x65, 0x75, 0x25, 0x5C},
	/* USB3 (AP_CH2) + 2 lane DP1.4 (AP_CH1) + AUX (flipped) */
	{0x0D, 0x25, 0x75, 0x65, 0x65, 0x5C}
};

struct usbdp_pin_ctrl {
	struct pinctrl *pinctrl;
	struct pinctrl_state *pwr_en;
	struct pinctrl_state *ext_pwr_en;
	char *platform;
};

#endif
extern void usb3_switch_ctrl_sel(int sel);
extern void usb3_switch_ctrl_en(bool en);
extern void usb3_switch_dps_en(bool en);
extern void mtk_dp_SWInterruptSet(int bstatus);
