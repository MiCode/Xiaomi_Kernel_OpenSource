/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __SMI_PORT_H__
#define __SMI_PORT_H__

#include <dt-bindings/memory/mt6853-larb-port.h>

#define SMI_OSTD_MAX		(0x3f)

#define SMI_COMM_MASTER_NUM	(8)
#define SMI_LARB_NUM		(21)
#define SMI_LARB0_PORT_NUM	(4)	/* SYS_DIS */
#define SMI_LARB1_PORT_NUM	(5)	/* SYS_DIS */
#define SMI_LARB2_PORT_NUM	(5)	/* SYS_MDP */
#define SMI_LARB3_PORT_NUM	(0)
#define SMI_LARB4_PORT_NUM	(12)	/* SYS_VDE */
#define SMI_LARB5_PORT_NUM	(0)
#define SMI_LARB6_PORT_NUM	(0)
#define SMI_LARB7_PORT_NUM	(13)	/* SYS_VEN */
#define SMI_LARB8_PORT_NUM	(0)
#define SMI_LARB9_PORT_NUM	(29)	/* SYS_IMG1 */
#define SMI_LARB10_PORT_NUM	(0)	/* SYS_IMG1 */
#define SMI_LARB11_PORT_NUM	(29)	/* SYS_IMG2 */
#define SMI_LARB12_PORT_NUM	(0)	/* SYS_IMG2 */
#define SMI_LARB13_PORT_NUM	(12)	/* SYS_CAM1 */
#define SMI_LARB14_PORT_NUM	(6)	/* SYS_CAM1 */
#define SMI_LARB15_PORT_NUM	(5)	/* SYS_CAM1 */
#define SMI_LARB16_PORT_NUM	(17)	/* SYS_CAM2 */
#define SMI_LARB17_PORT_NUM	(17)	/* SYS_CAM3 */
#define SMI_LARB18_PORT_NUM	(17)	/* SYS_CAM4 */
#define SMI_LARB19_PORT_NUM	(4)	/* SYS_IPE */
#define SMI_LARB20_PORT_NUM	(6)	/* SYS_IPE */
#define SMI_COMM_NUM		(1 + 7)
#define SMI_DEV_NUM		((SMI_LARB_NUM) + (SMI_COMM_NUM))

#define SMI_LARB_CCU0		(13)
#define SMI_LARB_CCU1		(14)

static const bool
SMI_COMM_BUS_SEL[SMI_COMM_MASTER_NUM] = {0, 1, 1, 0, 1, 1, 0, 1,};

static const u32
SMI_LARB_L1ARB[SMI_LARB_NUM] = {
	0, 1, 4, SMI_COMM_MASTER_NUM, 2, 2, SMI_COMM_MASTER_NUM,
	3, SMI_COMM_MASTER_NUM, 5, SMI_COMM_MASTER_NUM, 5, SMI_COMM_MASTER_NUM,
	7, 6, SMI_COMM_MASTER_NUM, 6, 7, 7, 5, 5};

static const u8
SMI_LARB_PORT_NUM[SMI_LARB_NUM] = {
	4, 5, 5, 0, 12, 0, 0, 13, 0, 29, 0, 29, 0, 12, 6, 5, 17, 17, 17, 4, 6};

#endif

