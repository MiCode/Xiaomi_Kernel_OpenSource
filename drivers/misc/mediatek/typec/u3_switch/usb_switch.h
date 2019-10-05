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
#ifndef USB_SWITCH_H
#define USB_SWITCH_H

struct usb3_switch {
	int sel_gpio;
	int en_gpio;
	int sel;
	int en;
};

struct usb_redriver {
	int c1_gpio;
	int c2_gpio;
	int eq_c1;
	int eq_c2;
};

struct usbc_pin_ctrl {
	struct pinctrl_state *re_c1_init;
	struct pinctrl_state *re_c1_low;
	struct pinctrl_state *re_c1_hiz;
	struct pinctrl_state *re_c1_high;

	struct pinctrl_state *re_c2_init;
	struct pinctrl_state *re_c2_low;
	struct pinctrl_state *re_c2_hiz;
	struct pinctrl_state *re_c2_high;

	struct pinctrl_state *u3_switch_sel1;
	struct pinctrl_state *u3_switch_sel2;

	struct pinctrl_state *u3_switch_enable;
	struct pinctrl_state *u3_switch_disable;
};

/*
 * struct usbtypc - Driver instance data.
 */

struct usbtypc {
	struct pinctrl *pinctrl;
	struct usbc_pin_ctrl *pin_cfg;
	struct typec_switch_data *host_driver;
	struct typec_switch_data *device_driver;
	struct usb3_switch *u3_sw;
	struct usb_redriver *u_rd;
};

#endif	/* USB_SWITCH_H */

extern void usb3_switch_ctrl_sel(int sel);
extern void usb3_switch_ctrl_en(bool en);
extern void usb3_switch_dps_en(bool en);
