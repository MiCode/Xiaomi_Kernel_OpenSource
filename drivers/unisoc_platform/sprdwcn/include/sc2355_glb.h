// SPDX-License-Identifier: GPL-2.0-only
/*
 * sc2355_glb.h - Unisoc platform header
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

#ifndef __SC2355_GLB_H__
#define __SC2355_GLB_H__

#include <linux/kernel.h>
#include "../sleep/slp_mgr.h"
#include "mem_pd_mgr.h"
//#include "rdc_debug.h"
#include "marlin3_base_glb.h"


#define M3_CP_RESET_REG		0x40088288

#define M3_FIRMWARE_MAX_SIZE 0xf0c00

#define M3_CHIPID_REG 0x4083c208

#define MARLIN3_AA_CHIPID 0x23550000
#define MARLIN3_AB_CHIPID 0x23550001
#define MARLIN3_AC_CHIPID 0x23550002
#define MARLIN3_AD_CHIPID 0x23550003


/* for wifi */

#define M3_WIFI_AON_MAC_SIZE		0x108

#define M3_WIFI_RAM_SIZE		0x58000
#define M3_WIFI_GLB_REG_SIZE	0x4c

/* for BT */
#define M3_BT_ACC_SIZE			(0x8d8)

#define M3_BT_MODEM_SIZE			(0x300)



/* For GNSS */
#define M3_GNSS_CP_START_ADDR	0x40A20000
#define M3_GNSS_CP_RESET_REG	0x40BC8280
#define M3_GNSS_FIRMWARE_MAX_SIZE 0x58000

/* for dump arm register */

#define M3_ARM_DAP_BASE_ADDR 0X4085C000
#define M3_ARM_DAP_REG1 0X4085C000
#define M3_ARM_DAP_REG2 0X4085C004
#define M3_ARM_DAP_REG3 0X4085C00C

#define M3_BTWF_STATUS_REG 0x4085c0fc

#define M3_SYNC_ADDR		0x405F0BB0

/* for sleep/wakeup */
#define M3_REG_CP_SLP_CTL		0x1a2

/* BIT4~7, if value 0, stand for in deepsleep */
#define M3_REG_BTWF_SLP_STS	0x148

/* fm playing in deep, and xtl on */
#define M3_BTWF_IN_DEEPSLEEP_XLT_ON	0x30

/*
 * For SPI interface
 * bit[15]:1'b0: BUFFER mode,outside clock
 * bit[15]:1'b1: XO mode,Crystal/TSX mode
 */
#define M3_WCN_BOUND_XO_MODE	BIT(15)

#endif
