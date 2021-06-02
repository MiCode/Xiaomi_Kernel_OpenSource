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

#if IS_ENABLED(CONFIG_MTK_IOMMU_V2)
#include <dt-bindings/memory/mt6885-larb-port.h>
#else
#define MTK_M4U_ID(larb, port)	(((larb) << 5) | (port))
#define MTK_IOMMU_TO_LARB(id)	(((id) >> 5) & 0x1f)
#define MTK_IOMMU_TO_PORT(id)	((id) & 0x1f)
#endif

#define SMI_OSTD_MAX		(0x3f)

#define SMI_COMM_MASTER_NUM	(8)
#define SMI_LARB_NUM		(21)
#define SMI_LARB0_PORT_NUM	(15)	/* SYS_DIS */
#define SMI_LARB1_PORT_NUM	(15)	/* SYS_DIS */
#define SMI_LARB2_PORT_NUM	(6)	/* SYS_DIS */
#define SMI_LARB3_PORT_NUM	(6)	/* SYS_DIS */
#define SMI_LARB4_PORT_NUM	(11)	/* SYS_VDE */
#define SMI_LARB5_PORT_NUM	(8)	/* SYS_VDE */
#define SMI_LARB6_PORT_NUM	(0)	/* SYS_VDE */
#define SMI_LARB7_PORT_NUM	(27)	/* SYS_VEN */
#define SMI_LARB8_PORT_NUM	(27)	/* SYS_VEN */
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
#define SMI_COMM_NUM		(3 + 8)
#define SMI_DEV_NUM		((SMI_LARB_NUM) + (SMI_COMM_NUM))

#define SMI_LARB_CCU0		(13)
#define SMI_LARB_CCU1		(14)

static const bool
SMI_COMM_BUS_SEL[SMI_COMM_MASTER_NUM] = {0, 1, 1, 0, 1, 1, 0, 1,};

static const u32
SMI_LARB_L1ARB[SMI_LARB_NUM] = {
	0, 1, 0x10000, 0x10001, 0x10002, 2, SMI_COMM_MASTER_NUM, 3,
	0x10003, 0x10004, SMI_COMM_MASTER_NUM, 4,
	SMI_COMM_MASTER_NUM, 0x10006, 6, SMI_COMM_MASTER_NUM,
	0x10007, 7, 0x10005, 5, 5};

static const u8
SMI_LARB_PORT_NUM[SMI_LARB_NUM] = {15, 15, 6, 6, 11, 8, 0, 27,
	27, 29, 0, 29, 0, 12, 6, 5, 17, 17, 17, 4, 6};


#endif
