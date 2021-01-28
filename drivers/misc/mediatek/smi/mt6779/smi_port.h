/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __SMI_PORT_H__
#define __SMI_PORT_H__
#include <dt-bindings/memory/mt6779-larb-port.h>
#define SMI_OSTD_MAX		(0x3f)
#define SMI_COMM_MASTER_NUM	(1 << 3)
#define SMI_LARB_NUM		12
#define SMI_LARB0_PORT_NUM	9	/* DIS */
#define SMI_LARB1_PORT_NUM	14	/* DIS */
#define SMI_LARB2_PORT_NUM	12	/* VDE */
#define SMI_LARB3_PORT_NUM	19	/* VEN */
#define SMI_LARB4_PORT_NUM	0	/* DUMMY */
#define SMI_LARB5_PORT_NUM	26	/* ISP */
#define SMI_LARB6_PORT_NUM	3	/* IPU */
#define SMI_LARB7_PORT_NUM	4	/* IPE */
#define SMI_LARB8_PORT_NUM	10	/* IPE */
#define SMI_LARB9_PORT_NUM	24	/* CAM */
#define SMI_LARB10_PORT_NUM	31	/* CAM */
#define SMI_LARB11_PORT_NUM	5	/* IPU */
#define SMI_COMM_NUM		(1)
#define SMI_DEV_NUM		((SMI_LARB_NUM) + (SMI_COMM_NUM))

static const bool
SMI_COMM_BUS_SEL[SMI_COMM_MASTER_NUM] = {0, 1, 1, 0, 1, 1, 1, 1, 1, 1};
static const u32
SMI_LARB_L1ARB[SMI_LARB_NUM] = {
	0, 1, 2, 3, SMI_COMM_MASTER_NUM, 4,
	SMI_COMM_MASTER_NUM, 5, 5, 7, 6, SMI_COMM_MASTER_NUM};
static const u8
SMI_LARB_PORT_NUM[SMI_LARB_NUM] = {
	9, 14, 12, 19, 0, 26, 3, 4, 10, 24, 31, 5};
#endif

