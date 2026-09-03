// SPDX-License-Identifier: GPL-2.0-only
/*
 * sprd_pd_bdo.h - Unisoc platform header
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

#ifndef __LINUX_USB_SPRD_PD_BDO_H
#define __LINUX_USB_SPRD_PD_BDO_H

/* BDO : BIST Data Object */
#define SPRD_BDO_MODE_RECV		(0 << 28)
#define SPRD_BDO_MODE_TRANSMIT		(1 << 28)
#define SPRD_BDO_MODE_COUNTERS		(2 << 28)
#define SPRD_BDO_MODE_CARRIER0		(3 << 28)
#define SPRD_BDO_MODE_CARRIER1		(4 << 28)
#define SPRD_BDO_MODE_CARRIER2		(5 << 28)
#define SPRD_BDO_MODE_CARRIER3		(6 << 28)
#define SPRD_BDO_MODE_EYE		(7 << 28)
#define SPRD_BDO_MODE_TESTDATA		(8 << 28)

#define SPRD_BDO_MODE_MASK(mode)	((mode) & 0xf0000000)

#endif
