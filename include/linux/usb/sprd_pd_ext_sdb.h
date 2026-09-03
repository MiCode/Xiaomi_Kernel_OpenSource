// SPDX-License-Identifier: GPL-2.0-only
/*
 * sprd_pd_ext_sdb.h - Unisoc platform header
 *
 * Copyright 2022 Unisoc(Shanghai) Technologies Co.Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __LINUX_USB_SPRD_PD_EXT_SDB_H
#define __LINUX_USB_SPRD_PD_EXT_SDB_H

/* SDB : Status Data Block */
enum sprd_usb_pd_ext_sdb_fields {
	SPRD_USB_PD_EXT_SDB_INTERNAL_TEMP = 0,
	SPRD_USB_PD_EXT_SDB_PRESENT_INPUT,
	SPRD_USB_PD_EXT_SDB_PRESENT_BATT_INPUT,
	SPRD_USB_PD_EXT_SDB_EVENT_FLAGS,
	SPRD_USB_PD_EXT_SDB_TEMP_STATUS,
	SPRD_USB_PD_EXT_SDB_DATA_SIZE,
};

/* Event Flags */
#define SPRD_USB_PD_EXT_SDB_EVENT_OCP			BIT(1)
#define SPRD_USB_PD_EXT_SDB_EVENT_OTP			BIT(2)
#define SPRD_USB_PD_EXT_SDB_EVENT_OVP			BIT(3)
#define SPRD_USB_PD_EXT_SDB_EVENT_CF_CV_MODE		BIT(4)

#define SPRD_USB_PD_EXT_SDB_PPS_EVENTS	(SPRD_USB_PD_EXT_SDB_EVENT_OCP |	\
					 SPRD_USB_PD_EXT_SDB_EVENT_OTP |	\
					 SPRD_USB_PD_EXT_SDB_EVENT_OVP)

#endif /* __LINUX_USB_PD_EXT_SDB_H */
