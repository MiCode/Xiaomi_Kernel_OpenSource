// SPDX-License-Identifier: GPL-2.0-only
/*
 * sprd_pd_ado.h - Unisoc platform header
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

#ifndef __LINUX_USB_SPRD_PD_ADO_H
#define __LINUX_USB_SPRD_PD_ADO_H

/* ADO : Alert Data Object */
#define SPRD_USB_PD_ADO_TYPE_SHIFT			24
#define SPRD_USB_PD_ADO_TYPE_MASK			0xff
#define SPRD_USB_PD_ADO_FIXED_BATT_SHIFT		20
#define SPRD_USB_PD_ADO_FIXED_BATT_MASK			0xf
#define SPRD_USB_PD_ADO_HOT_SWAP_BATT_SHIFT		16
#define SPRD_USB_PD_ADO_HOT_SWAP_BATT_MASK		0xf

#define SPRD_USB_PD_ADO_TYPE_BATT_STATUS_CHANGE		BIT(1)
#define SPRD_USB_PD_ADO_TYPE_OCP			BIT(2)
#define SPRD_USB_PD_ADO_TYPE_OTP			BIT(3)
#define SPRD_USB_PD_ADO_TYPE_OP_COND_CHANGE		BIT(4)
#define SPRD_USB_PD_ADO_TYPE_SRC_INPUT_CHANGE		BIT(5)
#define SPRD_USB_PD_ADO_TYPE_OVP			BIT(6)

static inline unsigned int sprd_usb_pd_ado_type(u32 ado)
{
	return (ado >> SPRD_USB_PD_ADO_TYPE_SHIFT) & SPRD_USB_PD_ADO_TYPE_MASK;
}

static inline unsigned int sprd_usb_pd_ado_fixed_batt(u32 ado)
{
	return (ado >> SPRD_USB_PD_ADO_FIXED_BATT_SHIFT) & SPRD_USB_PD_ADO_FIXED_BATT_MASK;
}

static inline unsigned int sprd_usb_pd_ado_hot_swap_batt(u32 ado)
{
	return (ado >> SPRD_USB_PD_ADO_HOT_SWAP_BATT_SHIFT) & SPRD_USB_PD_ADO_HOT_SWAP_BATT_MASK;
}
#endif /* __LINUX_USB_PD_ADO_H */
