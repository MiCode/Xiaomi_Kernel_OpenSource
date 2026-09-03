// SPDX-License-Identifier: GPL-2.0-only
/*
 * umw2652_glb.h - Unisoc platform header
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

#ifndef __UMW2652_glb_H__
#define __UMW2652_glb_H__

/* UMW2652 is the lite of sc2355 */
#include "marlin3_base_glb.h"

#define MARLIN3L_AA_CHIPID 0x2355B000

#define MARLIN3L_AB_CHIPID 0x2355B001

#define MARLIN3L_AC_CHIPID 0x2355B002

#define MARLIN3L_AD_CHIPID 0x2355B003

/**************GNSS BEG**************/

#define M3L_GNSS_CP_START_ADDR 0x40A50000

#define M3L_GNSS_FIRMWARE_MAX_SIZE 0x2B000

/**************GNSS END**************/

/**************WCN BEG**************/

#define M3L_FIRMWARE_MAX_SIZE 0xe7400

#define M3L_SYNC_ADDR 0x405E73B0

#define M3L_ARM_DAP_BASE_ADDR 0X40060000

#define M3L_ARM_DAP_REG1 0X40060000

#define M3L_ARM_DAP_REG2 0X40060004

#define M3L_ARM_DAP_REG3 0X4006000C

#define M3L_BTWF_STATUS_REG 0x400600fc

#define M3L_WIFI_AON_MAC_SIZE 0x120

#define M3L_WIFI_RAM_SIZE 0x4a800

#define M3L_WIFI_GLB_REG_SIZE 0x58

#define M3L_BT_ACC_SIZE 0x8f4

#define M3L_BT_MODEM_SIZE 0x310


/* for marlin3Lite */
#define M3L_APB_ENB1			0x4008801c
#define M3L_DBG_CM4_EB			BIT(10)
#define M3L_DAP_CTRL			0x4008828c
#define M3L_CM4_DAP_SEL_BTWF_LITE		BIT(1)

#define M3L_BTWF_XLT_WAIT		0x10
#define M3L_BTWF_XLTBUF_WAIT	0x20
#define M3L_BTWF_PLL_PWR_WAIT	0x40
/**************WCN END**************/

#define M3L_CP_RESET_REG		0x40088288
#define M3L_GNSS_CP_RESET_REG	0x40BC8280
#define M3L_CHIPID_REG 0x4083c208
#define M3L_REG_CP_SLP_CTL		0x1a2
#define M3L_REG_BTWF_SLP_STS	0x148
#define M3L_BTWF_IN_DEEPSLEEP_XLT_ON	0x30
#define M3L_WCN_BOUND_XO_MODE	BIT(15)

#endif
