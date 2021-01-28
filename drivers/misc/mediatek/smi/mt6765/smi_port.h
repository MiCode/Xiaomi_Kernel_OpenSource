/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __SMI_PORT_H__
#define __SMI_PORT_H__
#define SMI_OSTD_MAX		(0x3f)
#define SMI_COMM_MASTER_NUM	(4)
#define SMI_LARB_NUM		(4)
#define SMI_LARB0_PORT_NUM	(8)	/* MMSYS */
#define SMI_LARB1_PORT_NUM	(11)	/* VCODEC */
#define SMI_LARB2_PORT_NUM	(12)	/* IMGSYS */
#define SMI_LARB3_PORT_NUM	(21)	/* CAMSYS */
#define SMI_COMM_NUM		(1)
#define SMI_DEV_NUM		((SMI_LARB_NUM) + (SMI_COMM_NUM))

static const bool
SMI_COMM_BUS_SEL[SMI_COMM_MASTER_NUM] = {0, 1, 0, 1,};
static const u32
SMI_LARB_L1ARB[SMI_LARB_NUM] = {
	0, 1, 2, 3};
static const u8
SMI_LARB_PORT_NUM[SMI_LARB_NUM] = {
	8, 11, 12, 21};
#endif

