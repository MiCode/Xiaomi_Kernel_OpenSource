/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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

#ifdef CONFIG_USB_C_SWITCH_U3_MUX
extern void usb3_switch_ctrl_sel(int sel);
extern void usb3_switch_ctrl_en(bool en);
#endif
#endif	/* USB_TYPEC_H */
