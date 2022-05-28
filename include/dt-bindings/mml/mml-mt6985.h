/* SPDX-License-Identifier: GPL-2.0-only OR BSD-2-Clause */
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>
 */

#ifndef _DT_BINDINGS_MML_MT6985_H
#define _DT_BINDINGS_MML_MT6985_H

/* MML engines in mt6985 */
/* The id 0 leaves empty, do not use. */
#define MML_MMLSYS		1
#define MML_MUTEX		2
#define MML_RDMA0		3
#define MML_RDMA1		4
#define MML_RDMA2		5
#define MML_RDMA3		6
#define MML_DLI0		7
#define MML_DLI1		8
#define MML_DLI0_SEL		9
#define MML_DLI1_SEL		10
#define MML_HDR0		11
#define MML_HDR1		12
#define MML_AAL0		13
#define MML_AAL1		14
#define MML_RSZ0		15
#define MML_RSZ1		16
#define MML_RSZ2		17
#define MML_RSZ3		18
#define MML_BIRSZ0		19
#define MML_BIRSZ1		20
#define MML_TDSHP0		21
#define MML_TDSHP1		22
#define MML_COLOR0		23
#define MML_COLOR1		24
#define MML_PQ0_SOUT		25
#define MML_PQ1_SOUT		26
#define MML_DLO0_SOUT		27
#define MML_DLO1_SOUT		28
#define MML_WROT0		29
#define MML_WROT1		30
#define MML_WROT2		31
#define MML_WROT3		32
#define MML_DLO0		33
#define MML_DLO1		34
#define MML_ENGINE_TOTAL	35

/* MML component types. See mtk-mml-sys.c */
#define MML_CT_SYS		1
#define MML_CT_PATH		2
#define MML_CT_DL_IN		3
#define MML_CT_DL_OUT		4

/* MML SYS registers */
#define MMLSYS_MISC		0x0f0
#define MML_CG_CON0		0x100
#define MML_CG_SET0		0x104
#define MML_CG_CLR0		0x108
#define MML_CG_CON1		0x110
#define MML_CG_SET1		0x114
#define MML_CG_CLR1		0x118
#define MML_CG_CON2		0x120
#define MML_CG_SET2		0x124
#define MML_CG_CLR2		0x128
#define MML_CG_CON3		0x130
#define MML_CG_SET3		0x134
#define MML_CG_CLR3		0x138
#define MML_CG_CON4		0x140
#define MML_CG_SET4		0x144
#define MML_CG_CLR4		0x148
#define MML_SW0_RST_B		0x700
#define MML_SW1_RST_B		0x704
#define MML_SW2_RST_B		0x708
#define MML_SW3_RST_B		0x70c
#define MML_SW4_RST_B		0x710
#define MML_EVENT_GCEM_EN	0x7f4
#define MML_EVENT_GCED_EN	0x7f8
#define MML_IN_LINE_READY_SEL	0x7fc
#define MML_SMI_LARB_GREQ	0x8dc
#define MML_BYPASS_MUX_SHADOW	0xf00
#define MML_MOUT_RST		0xf04
/* MML DL IN/OUT registers in mt6985 */
#define MML_DL_IN_RELAY0_SIZE	0x220
#define MML_DL_IN_RELAY1_SIZE	0x224
#define MML_DL_OUT_RELAY0_SIZE	0x228
#define MML_DL_OUT_RELAY1_SIZE	0x22c
#define MML_DLO_ASYNC0_STATUS0	0x230
#define MML_DLO_ASYNC0_STATUS1	0x234
#define MML_DLO_ASYNC1_STATUS0	0x238
#define MML_DLO_ASYNC1_STATUS1	0x23c
#define MML_DLI_ASYNC0_STATUS0	0x240
#define MML_DLI_ASYNC0_STATUS1	0x244
#define MML_DLI_ASYNC1_STATUS0	0x248
#define MML_DLI_ASYNC1_STATUS1	0x24c
#define MML_DL_IN_RELAY2_SIZE	0x250
#define MML_DL_IN_RELAY3_SIZE   0x254
#define MML_DL_OUT_RELAY2_SIZE  0x258
#define MML_DL_OUT_RELAY3_SIZE  0x25c
#define MML_DLO_ASYNC2_STATUS0  0x260
#define MML_DLO_ASYNC2_STATUS1  0x264
#define MML_DLO_ASYNC3_STATUS0  0x268
#define MML_DLO_ASYNC3_STATUS1  0x26c
#define MML_DLI_ASYNC2_STATUS0  0x270
#define MML_DLI_ASYNC2_STATUS1  0x274
#define MML_DLI_ASYNC3_STATUS0  0x278
#define MML_DLI_ASYNC3_STATUS1  0x27c

/* MML MUX registers in mt6985 */
#define MML_DLI0_SEL_IN		0xf14
#define MML_DLI1_SEL_IN		0xf18
#define MML_RDMA0_MOUT_EN	0xf20
#define MML_RDMA1_MOUT_EN	0xf24
#define MML_PQ0_SEL_IN		0xf30
#define MML_PQ1_SEL_IN		0xf34
#define MML_WROT0_SEL_IN	0xf70
#define MML_WROT1_SEL_IN	0xf74
#define MML_PQ0_SOUT_SEL	0xf80
#define MML_PQ1_SOUT_SEL	0xf84
#define MML_DLO0_SOUT_SEL	0xf88
#define MML_DLO1_SOUT_SEL	0xf8c
#define MML_BYP0_MOUT_EN	0xf90
#define MML_BYP1_MOUT_EN	0xf94
#define MML_BYP0_SEL_IN		0xf98
#define MML_BYP1_SEL_IN		0xf9c
#define MML_RSZ2_SEL_IN		0xfa0
#define MML_RSZ3_SEL_IN		0xfa4
#define MML_HDR0_SOUT_SEL	0xfa8
#define MML_HDR1_SOUT_SEL	0xfac
#define MML_AAL0_SEL_IN		0xf40
#define MML_AAL1_SEL_IN		0xf44
#define MML_TDSHP0_SOUT_SEL	0xf48
#define MML_TDSHP1_SOUT_SEL	0xf4c
#define MML_COLOR0_SEL_IN	0xf50
#define MML_COLOR1_SEL_IN	0xf54
#define MML_COLOR0_SOUT_SEL	0xf58
#define MML_COLOR1_SOUT_SEL	0xf5c
#define MML_TDSHP0_SEL_IN	0xf60
#define MML_TDSHP1_SEL_IN	0xf64
#define MML_AAL0_MOUT_EN	0xf68
#define MML_AAL1_MOUT_EN	0xf6c
#define MML_MOUT_MASK0		0xfd0
#define MML_MOUT_MASK1		0xfd4
#define MML_MOUT_MASK2		0xfd8
/* MML AID for secure */
#define MML_RDMA0_AIDSEL	0x500
#define MML_RDMA1_AIDSEL	0x504
#define MML_WROT0_AIDSEL	0x508
#define MML_WROT1_AIDSEL	0x50C
#define MML_WROT2_AIDSEL	0x510
#define MML_WROT3_AIDSEL	0x514
#define MML_FAKE0_AIDSEL	0x518
#define MML_RDMA2_AIDSEL	0x51C
#define MML_RDMA3_AIDSEL	0x520

/* MMLSys debug valid/ready */
#define MML_DL_VALID0		0xfe0
#define MML_DL_VALID1		0xfe4
#define MML_DL_VALID2		0xfe8
#define MML_DL_VALID3		0xfec
#define MML_DL_READY0		0xff0
#define MML_DL_READY1		0xff4
#define MML_DL_READY2		0xff8
#define MML_DL_READY3		0xfdc

/* MML SYS mux types. See mtk-mml-sys.c */
#define MML_MUX_MOUT		1
#define MML_MUX_SOUT		2
#define MML_MUX_SLIN		3

#endif	/* _DT_BINDINGS_MML_MT6985_H */
