// SPDX-License-Identifier: GPL-2.0-only
/*
 * wcn_integrate_base_glb.h - Unisoc platform header
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

#ifndef __WCN_INTEGRATE_BASE_GLB_H__
#define __WCN_INTEGRATE_BASE_GLB_H__

/* ap cp sync flag */
#define MARLIN_CP_INIT_READY_MAGIC	(0xababbaba)
#define MARLIN_CP_INIT_START_MAGIC	(0x5a5a5a5a)
#define MARLIN_CP_INIT_SUCCESS_MAGIC	(0x13579bdf)
#define MARLIN_CP_INIT_FAILED_MAGIC	(0x88888888)

#define INTEG_CP_START_ADDR		0
#define INTEG_CP_RESET_REG		0x40060288
//#define CP_SDIO_PRIORITY_ADDR 0x60300150
/* set sdio higher priority to visit iram */
//#define M6_TO_S0_HIGH_PRIORITY 0X80000000

#define INTEG_CHIPID_REG 0x603003fc

#define MARLIN2_AA_CHIPID 0x23490000
#define MARLIN2_AB_CHIPID 0x23490001

//#define DUMP_WIFI_ADDR			0x70000000
//#define DUMP_WIFI_ADDR_SIZE		0x70000

//#define DUMP_BT_CMD_ADDR		0X50000000
//#define DUMP_BT_CMD_ADDR_SIZE		0x400
//#define DUMP_BT_ADDR			0X50040000
//#define DUMP_BT_ADDR_SIZE		0xa400

//#define DUMP_FM_ADDR			0X400B0000
//#define DUMP_INTC_ADDR			0X40010000

//#define DUMP_SYSTIMER_ADDR		0X40020000
//#define DUMP_WDG_ADDR			0X40040000
//#define DUMP_APB_ADDR			0X40060000
//#define DUMP_DMA_ADDR			0X60200000
//#define DUMP_AHB_ADDR			0X60300000
//#define DUMP_REG_SIZE			0X10000
//#define DUMP_SDIO_ADDR			0x60400000
//#define DUMP_SDIO_ADDR_SIZE		0x10000

/* need check, not need to dump it */
//#define DUMP_WIFI_1_ADDR            0
//#define DUMP_WIFI_1_ADDR_SIZE        0
//#define DUMP_WIFI_2_ADDR            0
//#define DUMP_WIFI_2_ADDR_SIZE        0
//#define DUMP_WIFI_3_ADDR            0
//#define DUMP_WIFI_3_ADDR_SIZE        0

/* For GNSS */
#define INTEG_GNSS_CP_RESET_REG	0x40BC8280
#define INTEG_FIRMWARE_MAX_SIZE 0x90c00

#define INTEG_GNSS_CP_START_ADDR	0x40A20000
#define INTEG_GNSS_FIRMWARE_MAX_SIZE 0x58000

#endif
