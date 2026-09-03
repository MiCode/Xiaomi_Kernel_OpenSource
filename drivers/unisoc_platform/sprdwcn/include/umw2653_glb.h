// SPDX-License-Identifier: GPL-2.0-only
/*
 * umw2653_glb.h - Unisoc platform header
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

#ifndef __UMW2653_GLB_H__
#define __UMW2653_GLB_H__

#include "../sleep/slp_mgr.h"
#include "mem_pd_mgr.h"
//#include "rdc_debug.h"
#include "marlin3_base_glb.h"

#define M3E_CP_RESET_REG		0x40930004

#define M3E_FIRMWARE_MAX_SIZE 0xf0c00

#define M3E_CHIPID_REG 0x4082c208

#define MARLIN_AA_CHIPID 0x23550000
#define MARLIN_AB_CHIPID 0x23550001
#define MARLIN_AC_CHIPID 0x23550002
#define MARLIN_AD_CHIPID 0x23550003
#define MARLIN3E_AA_CHIPID 0x56630000
#define MARLIN3E_AB_CHIPID 0x56630001
#define MARLIN3E_AC_CHIPID 0x56630002
#define MARLIN3E_AD_CHIPID 0x56630003


/* for wifi */
#define M3E_WIFI_AON_MAC_SIZE		0x108

#define M3E_WIFI_RAM_SIZE		0x58000
#define M3E_WIFI_GLB_REG_SIZE	0x4c

#define WIFI_ENABLE				(0x40130004)



/* for BT */
#define M3E_BT_ACC_SIZE			(0x8d8)
#define M3E_BT_MODEM_SIZE			(0x300)

/* For GNSS */
#define M3E_GNSS_CP_START_ADDR	0x40A20000
#define M3E_GNSS_CP_RESET_REG	0x40BC8280
#define M3E_GNSS_FIRMWARE_MAX_SIZE 0x58000

#define M3E_ARM_DAP_BASE_ADDR 0X4085C000
#define M3E_ARM_DAP_REG1 0X4085C000
#define M3E_ARM_DAP_REG2 0X4085C004
#define M3E_ARM_DAP_REG3 0X4085C00C

#define M3E_BTWF_STATUS_REG 0x4085c0fc

#define M3E_SYNC_ADDR		0x40525FA0

/* for sleep/wakeup */
#define M3E_REG_CP_SLP_CTL		0x1aa
#define REG_CP_PMU_SEL_CTL	0x1a3
/* BIT4~7, if value 0, stand for in deepsleep */
#define M3E_REG_BTWF_SLP_STS	0x143
/* fm playing in deep, and xtl on */
#define M3E_BTWF_IN_DEEPSLEEP_XLT_ON	0x3
#define BTWF_XLT_WAIT		0x1
#define BTWF_XLTBUF_WAIT	0x2
#define BTWF_PLL_PWR_WAIT	0x4
#define  SLEEP_STATUS_FLAG     0x0F

/*
 * For SPI interface
 * bit[15]:1'b0: TCXO mode, outside clock
 * bit[15]:1'b1: Crystal/TSX mode
 */
#define tsx_mode		(1 << 15)

#endif
