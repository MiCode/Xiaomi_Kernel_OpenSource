/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __SMI_PORT_H__
#define __SMI_PORT_H__

#define SMI_OSTD_MAX		(0x1f)
#define SMI_LARB_NUM		(3)
#define SMI_LARB0_PORT_NUM	(7)	/* SYS_MM0 */
#define SMI_LARB1_PORT_NUM	(11)	/* SYS_VEN */
#define SMI_LARB2_PORT_NUM	(11)	/* SYS_ISP */
#define SMI_COMM_MASTER_NUM	(SMI_LARB_NUM)
#define SMI_COMM_NUM		(1)
#define SMI_DEV_NUM		((SMI_LARB_NUM) + (SMI_COMM_NUM))

static const bool
SMI_COMM_BUS_SEL[SMI_COMM_MASTER_NUM] = {0, 0, 0,};

static const u32
SMI_LARB_L1ARB[SMI_LARB_NUM] = {0, 1, 2,};
#endif
