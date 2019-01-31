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

/* This is used to for host and peripheral modes of the typec driver. */
#ifndef USB_TYPEC_H
#define USB_TYPEC_H

/* ConnSide */
#define DONT_CARE 0
#define UP_SIDE 1
#define DOWN_SIDE 2
#define CC1_SIDE 1
#define CC2_SIDE 2

/* Stat */
#define DISABLE 0
#define ENABLE 1

/* DriverType */
#define DEVICE_TYPE 1
#define HOST_TYPE	2
#define DONT_CARE_TYPE 3

/* USBRdCtrlPin */
#define U3_EQ_C1 0
#define U3_EQ_C2 1

/* USBRdStat */
#define U3_EQ_LOW 0
#define U3_EQ_HZ 1
#define U3_EQ_HIGH 2

struct typec_switch_data {
	char *name;
	int type;
	int on;
	int (*enable)(void *);
	int (*disable)(void *);
	int (*vbus_enable)(void *);
	int (*vbus_disable)(void *);
	void *priv_data;
};


extern int register_typec_switch_callback(struct typec_switch_data *new_driver);
extern int unregister_typec_switch_callback(
	struct typec_switch_data *new_driver);
#ifdef CONFIG_USB_C_SWITCH_U3_MUX
extern void usb3_switch_ctrl_sel(int sel);
extern void usb3_switch_ctrl_en(bool en);
#endif
#endif	/* USB_TYPEC_H */
