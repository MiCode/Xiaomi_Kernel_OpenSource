/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __SMI_PORT_H__
#define __SMI_PORT_H__

#include <dt-bindings/memory/mt6877-larb-port.h>

#define SMI_OSTD_MAX		(0x3f)

#define SMI_COMM_MASTER_NUM	(8)
#define SMI_LARB_NUM		(21)
#define SMI_LARB0_PORT_NUM	(5)	/* SYS_DIS */
#define SMI_LARB1_PORT_NUM	(7)	/* SYS_DIS */
#define SMI_LARB2_PORT_NUM	(5)	/* SYS_MDP */
#define SMI_LARB3_PORT_NUM	(0)
#define SMI_LARB4_PORT_NUM	(12)	/* SYS_VDE */
#define SMI_LARB5_PORT_NUM	(0)
#define SMI_LARB6_PORT_NUM	(0)
#define SMI_LARB7_PORT_NUM	(15)	/* SYS_VEN */
#define SMI_LARB8_PORT_NUM	(0)
#define SMI_LARB9_PORT_NUM	(29)	/* SYS_IMG1 */
#define SMI_LARB10_PORT_NUM	(0)
#define SMI_LARB11_PORT_NUM	(29)	/* SYS_IMG2 */
#define SMI_LARB12_PORT_NUM	(0)
#define SMI_LARB13_PORT_NUM	(15)	/* SYS_CAM1 */
#define SMI_LARB14_PORT_NUM	(10)	/* SYS_CAM1 */
#define SMI_LARB15_PORT_NUM	(0)
#define SMI_LARB16_PORT_NUM	(17)	/* SYS_CAM2 */
#define SMI_LARB17_PORT_NUM	(17)	/* SYS_CAM3 */
#define SMI_LARB18_PORT_NUM	(0)
#define SMI_LARB19_PORT_NUM	(4)	/* SYS_IPE */
#define SMI_LARB20_PORT_NUM	(6)	/* SYS_IPE */
#define SMI_COMM_NUM		(1 + 7) /* include sub_common */
#define SMI_DEV_NUM		((SMI_LARB_NUM) + (SMI_COMM_NUM))

static const bool
SMI_COMM_BUS_SEL[SMI_COMM_MASTER_NUM] = {0, 1, 1, 0, 0, 1, 1, 0,};

static const u32
SMI_LARB_L1ARB[SMI_LARB_NUM] = {
	0, 1, 4, SMI_COMM_MASTER_NUM, 2, SMI_COMM_MASTER_NUM,
	SMI_COMM_MASTER_NUM, 3, SMI_COMM_MASTER_NUM, 5, SMI_COMM_MASTER_NUM,
	5, SMI_COMM_MASTER_NUM, 7, 6, SMI_COMM_MASTER_NUM, 6, 7,
	SMI_COMM_MASTER_NUM, 5, 5};

static const u8
SMI_LARB_PORT_NUM[SMI_LARB_NUM] = {
	5, 7, 5, 0, 12, 0, 0, 15, 0, 29, 0, 29, 0, 15, 10, 0, 17, 17, 0, 4, 6};

#endif

