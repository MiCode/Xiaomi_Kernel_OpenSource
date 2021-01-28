/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __SMI_PORT_H__
#define __SMI_PORT_H__
#include <dt-bindings/memory/mt6761-larb-port.h>
#define SMI_OSTD_MAX		(0x3f)
#define SMI_COMM_MASTER_NUM	(3)
#define SMI_LARB_NUM		(3)
#define SMI_LARB0_PORT_NUM	(8)	/* SYS_DIS */
#define SMI_LARB1_PORT_NUM	(11)	/* SYS_VCODEC */
#define SMI_LARB2_PORT_NUM	(24)	/* SYS_CAM */
#define SMI_COMM_NUM		(1)
#define SMI_DEV_NUM		((SMI_LARB_NUM) + (SMI_COMM_NUM))
//#define SMI_LARB_CCU0		(13)
//#define SMI_LARB_CCU1		(14)
static const bool
SMI_COMM_BUS_SEL[SMI_COMM_MASTER_NUM] = {0, 1, 1,};
static const u32
SMI_LARB_L1ARB[SMI_LARB_NUM] = {
	0, 1, 2};
static const u8
SMI_LARB_PORT_NUM[SMI_LARB_NUM] = {
	8, 11, 24};
#endif

