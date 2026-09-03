/*
 * Copyright (C) 2019 Unisoc Technologies Inc.
 *
 * Author:	xiaodong.bi
 * File:	wcn_swd_dap.h
 * Description:	Marlin Debug System main file. Dump arm registers
 * or access other address by swd dap method.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __WCN_SWD_DAP_H__
#define __WCN_SWD_DAP_H__

#include "edma_engine.h"
#include "pcie.h"
#include "../sdio/sdiohal.h"
#include "wcn_glb_reg.h"

#define LOWEST_BIT_OF_VALUE		BIT(0)
#define SWD_DEVICE_EN_1			BIT(1)
#define SWD_DEVICE_EN_6			BIT(6)

/* DAP Transfer Request */
/* DAP_TRANSFER_AP_DP: AP : 1 DP: 0 */
#define DAP_TRANSFER_AP_DP		BIT(0)
/* DAP_TRANSFER_RW: R:1 W:0 */
#define DAP_TRANSFER_RW			BIT(1)
#define DAP_TRANSFER_A2			BIT(2)
#define DAP_TRANSFER_A3			BIT(3)
#define DAP_TRANSFER_MATCH_VALUE	BIT(4)
#define DAP_TRANSFER_MATCH_MASK		BIT(5)

/* DAP Transfer Response */
#define DAP_TRANSFER_OK			BIT(0)
#define DAP_TRANSFER_WAIT		BIT(1)
#define DAP_TRANSFER_FAULT		BIT(2)
#define DAP_TRANSFER_ERROR		BIT(3)
#define DAP_TRANSFER_MISMATCH		BIT(4)

/* Debug Port Register Addresses */
/* IDCODE Register (SW Read only) */
#define DP_IDCODE	0x0
/* Abort Register (SW Write only) */
#define DP_ABORT	0x0
/* Control & Status */
#define DP_CTRL_STAT	0x4
/* Wire Control Register (SW Only) */
#define DP_WCR		0x4
/* Select Register (JTAG R/W & SW W) */
#define DP_SELECT	0x8
/* Resend (SW Read Only) */
#define DP_RESEND	0x8
/* Read Buffer (Read Only) */
#define DP_RDBUFF	0xC
/* Targetsel (Read Only) */
#define DP_TARGETSEL	0xC

#define AP_CTRL		0x0
#define AP_STAT		0x0
/* APIDR 0xFC, bank:0xc */
#define AP_IDCODE       0xc

#define TARGETSEL_AON	0X22000001
#define TARGETSEL_AP	0X12000001
#define TARGETSEL_CP	0X02000001
#define SWD_POWERUP	0x50000000

#define DEBUG_HALTING_CTRL_STATUS_REG		0xe000edf0
#define DEBUG_HALTING_CTRL_STATUS_VAL		0xa05f0003
#define DEBUG_EXCEPTION_MONITOR_CTRL_REG	0xe000edfC
#define DEBUG_EXCEPTION_MONITOR_CTRL_VAL	0x010007f1

#define CM33_MPUNSDISABLE	BIT(0)
#define CM33_SPNIDEN		BIT(1)
#define CM33_SPIDEN		BIT(2)
#define CM33_NIDEN		BIT(3)
#define CM33_DBGEN		BIT(4)
#define CM33_DAPEN		BIT(5)
#define CM33_SAUDISABLE		BIT(6)
#define CM33_MPUSDISABLE	BIT(7)
#define CM33_CFGSECEXT		BIT(8)
#define CM33_CFGDSP		BIT(9)
#define CM33_CFGFPU		BIT(10)

#define CM33_AHB_CTRL3_ADDR	0x401303e8
#define CM33_AHB_CTRL3_VALUE	(CM33_SPNIDEN | CM33_SPIDEN | CM33_NIDEN |\
				CM33_DBGEN | CM33_DAPEN | CM33_CFGSECEXT |\
				CM33_CFGDSP | CM33_CFGFPU)

#define DAP_ADDR_PCIE	0x1C
#define DAP_ACK_ADDR_PCIE	0x0C
#define BIT_SEL_MTCKMS_PCIE	BIT(15)
#define BIT_IN_MTCK_PCIE	BIT(14)
#define BIT_IN_MTMS_PCIE	BIT(13)

#define DAP_ADDR	(0x1A0 + 0xE)
#define DAP_ACK_ADDR	(0x140 + 0xF)
#define BIT_SEL_MTCKMS	BIT(7)
#define BIT_IN_MTCK	BIT(6)
#define BIT_IN_MTMS	BIT(5)

#define BIT_OUT_MTMS	BIT(7)

int swd_dump_arm_reg(void);

#endif
