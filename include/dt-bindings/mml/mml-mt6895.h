/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _DT_BINDINGS_MML_MT6895_H
#define _DT_BINDINGS_MML_MT6895_H

/* MML engines in mt6895 */
/* The id 0 leaves empty, do not use. */
#define MML_MMLSYS		1
#define MML_MUTEX		2
#define MML_RDMA0		3
#define MML_RDMA1		4
#define MML_DLI0		5
#define MML_DLI1		6
#define MML_DLI0_SEL		7
#define MML_DLI1_SEL		8
#define MML_HDR0		9
#define MML_HDR1		10
#define MML_AAL0		11
#define MML_AAL1		12
#define MML_RSZ0		13
#define MML_RSZ1		14
#define MML_RSZ2		15
#define MML_RSZ3		16
#define MML_TDSHP0		17
#define MML_TDSHP1		18
#define MML_COLOR0		19
#define MML_COLOR1		20
#define MML_DLO0_SOUT		21
#define MML_DLO1_SOUT		22
#define MML_WROT0		23
#define MML_WROT1		24
#define MML_WROT2		25
#define MML_WROT3		26
#define MML_DLO0		27
#define MML_DLO1		28
#define MML_ENGINE_TOTAL	29

/* MML component types. See mtk-mml-sys.c */
#define MML_CT_SYS		1
#define MML_CT_PATH		2
#define MML_CT_DL_IN		3
#define MML_CT_DL_OUT		4

/* MML SYS registers */
#define CG_CON0			0x100
#define CG_SET0			0x104
#define CG_CLR0			0x108
#define CG_CON1			0x110
#define CG_SET1			0x114
#define CG_CLR1			0x118
#define CG_CON2			0x120
#define CG_SET2			0x124
#define CG_CLR2			0x128
#define CG_CON3			0x130
#define CG_SET3			0x134
#define CG_CLR3			0x138
#define CG_CON4			0x140
#define CG_SET4			0x144
#define CG_CLR4			0x148
#define SW0_RST_B		0x700
#define SW1_RST_B		0x704
#define SW2_RST_B		0x708
#define SW3_RST_B		0x70c
#define SW4_RST_B		0x710
#define EVENT_GCEM_EN		0x7f4
#define EVENT_GCED_EN		0x7f8
#define IN_LINE_READY_SEL	0x7fc
#define SMI_LARB_GREQ		0x8dc
#define BYPASS_MUX_SHADOW	0xf00
#define MOUT_RST		0xf04
/* MML DL IN/OUT registers in mt6895 */
#define DL_IN_RELAY0_SIZE	0x220
#define DL_IN_RELAY1_SIZE	0x224
#define DL_OUT_RELAY0_SIZE	0x228
#define DL_OUT_RELAY1_SIZE	0x22c
#define DLO_ASYNC0_STATUS0	0x230
#define DLO_ASYNC0_STATUS1	0x234
#define DLO_ASYNC1_STATUS0	0x238
#define DLO_ASYNC1_STATUS1	0x23c
#define DLI_ASYNC0_STATUS0	0x240
#define DLI_ASYNC0_STATUS1	0x244
#define DLI_ASYNC1_STATUS0	0x248
#define DLI_ASYNC1_STATUS1	0x24c
/* MML MUX registers in mt6895 */
#define DLI0_SEL_IN		0xf14
#define DLI1_SEL_IN		0xf18
#define RDMA0_MOUT_EN		0xf20
#define RDMA1_MOUT_EN		0xf24
#define PQ0_SEL_IN		0xf30
#define PQ1_SEL_IN		0xf34
#define WROT0_SEL_IN		0xf70
#define WROT1_SEL_IN		0xf74
#define PQ0_SOUT_SEL		0xf80
#define PQ1_SOUT_SEL		0xf84
#define DLO0_SOUT_SEL		0xf88
#define DLO1_SOUT_SEL		0xf8c
#define BYP0_MOUT_EN		0xf90
#define BYP1_MOUT_EN		0xf94
#define BYP0_SEL_IN		0xf98
#define BYP1_SEL_IN		0xf9c
#define RSZ2_SEL_IN		0xfa0
#define RSZ3_SEL_IN		0xfa4
#define AID_SEL			0xfa8
#define MOUT_MASK0		0xfd0
#define MOUT_MASK1		0xfd4
#define MOUT_MASK2		0xfd8
#define DL_VALID0		0xfe0
#define DL_VALID1		0xfe4
#define DL_VALID2		0xfe8
#define DL_READY0		0xff0
#define DL_READY1		0xff4
#define DL_READY2		0xff8

/* MML SYS mux types. See mtk-mml-sys.c */
#define MML_MUX_MOUT		1
#define MML_MUX_SOUT		2
#define MML_MUX_SLIN		3

#endif	/* _DT_BINDINGS_MML_MT6895_H */
