/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __SMI_PORT_H__
#define __SMI_PORT_H__

#define SMI_OSTD_MAX		(0x1f)
#define SMI_LARB_NUM		(5)
#define SMI_LARB0_PORT_NUM	(8)	/* SYS_DIS */
#define SMI_LARB1_PORT_NUM	(9)	/* SYS_VDE */
#define SMI_LARB2_PORT_NUM	(12)	/* SYS_ISP */
#define SMI_LARB3_PORT_NUM	(21)	/* SYS_CAM */
#define SMI_LARB4_PORT_NUM	(11)	/* SYS_VEN */
#define SMI_COMM_MASTER_NUM	(SMI_LARB_NUM)
#define SMI_COMM_NUM        (1)
#define SMI_DEV_NUM         ((SMI_LARB_NUM) + (SMI_COMM_NUM))

static const bool
SMI_COMM_BUS_SEL[SMI_COMM_MASTER_NUM] = {0, 1, 0, 0, 1,};

static const u32
SMI_LARB_L1ARB[SMI_LARB_NUM] = {0, 1, 2, 3, 4,};
#endif
