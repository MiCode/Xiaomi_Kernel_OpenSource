/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _DT_BINDINGS_MML_MT6893_H
#define _DT_BINDINGS_MML_MT6893_H

/* MML engines in mt6893 */
/* Temp for early development. The id 0 leaves empty, do not use. */
#define MML_MMLSYS		1
#define MML_MUTEX		2
#define MML_CAMIN		3 /* to verify sub-component framework */
#define MML_CAMIN2		4 /* to verify sub-component framework */
#define MML_CAMIN3		5 /* to verify sub-component framework */
#define MML_CAMIN4		6 /* to verify sub-component framework */
#define MML_RDMA0		7
#define MML_RDMA1		8
#define MML_RDMA2		9
#define MML_RDMA3		10
#define MML_FG0			11
#define MML_FG1			12
#define MML_PQ0_SOUT		13
#define MML_PQ1_SOUT		14
#define MML_HDR0		15
#define MML_HDR1		16
#define MML_COLOR0		17
#define MML_COLOR1		18
#define MML_AAL0		19
#define MML_AAL1		20
#define MML_AAL2		21
#define MML_AAL3		22
#define MML_RSZ0		23
#define MML_RSZ1		24
#define MML_RSZ2		25
#define MML_RSZ3		26
#define MML_TDSHP0		27
#define MML_TDSHP1		28
#define MML_TDSHP2		29
#define MML_TDSHP3		30
#define MML_TCC0		31
#define MML_TCC1		32
#define MML_TCC2		33
#define MML_TCC3		34
#define MML_WROT0		35
#define MML_WROT1		36
#define MML_WROT2		37
#define MML_WROT3		38
#define MML_ENGINE_TOTAL	39
/*
#define MML_MMLSYS		0
#define MML_MUTEX		1
#define MML_RDMA0		2
#define MML_RDMA1		3
#define MML_RDMA2		4
#define MML_RDMA3		5
#define MML_FG0			6
#define MML_FG1			7
#define MML_PQ0_SOUT		8
#define MML_PQ1_SOUT		9
#define MML_HDR0		10
#define MML_HDR1		11
#define MML_COLOR0		12
#define MML_COLOR1		13
#define MML_AAL0		14
#define MML_AAL1		15
#define MML_AAL2		16
#define MML_AAL3		17
#define MML_RSZ0		18
#define MML_RSZ1		19
#define MML_RSZ2		20
#define MML_RSZ3		21
#define MML_TDSHP0		22
#define MML_TDSHP1		23
#define MML_TDSHP2		24
#define MML_TDSHP3		25
#define MML_TCC0		26
#define MML_TCC1		27
#define MML_TCC2		28
#define MML_TCC3		29
#define MML_WROT0		30
#define MML_WROT1		31
#define MML_WROT2		32
#define MML_WROT3		33
#define MML_ENGINE_TOTAL	34
*/

/* MML component types. See mtk-mml-sys.c */
#define MML_CT_SYS	1
#define MML_CT_PATH	2
#define MML_CT_DL_IN	3
/* MML SYS mux registers offset. See mtk-mml-sys.c */
#define CG_CON0		0x100
#define CG_SET0		0x104
#define CG_CLR0		0x108
#define CG_CON1		0x110
#define CG_SET1		0x114
#define CG_CLR1		0x118
#define CG_CON2		0x120
#define CG_SET2		0x124
#define CG_CLR2		0x128
#define CG_CON3		0x130
#define CG_SET3		0x134
#define CG_CLR3		0x138
#define CG_CON4		0x140
#define CG_SET4		0x144
#define CG_CLR4		0x148
#define ISP0_MOUT_EN	0xf10
#define ISP1_MOUT_EN	0xf14
#define ISP2_MOUT_EN	0xf18
#define ISP3_MOUT_EN	0xf1c
#define RDMA0_MOUT_EN	0xf20
#define RDMA1_MOUT_EN	0xf24
#define RDMA2_MOUT_EN	0xf28
#define RDMA3_MOUT_EN	0xf2c
#define PQ0_SEL_IN	0xf30
#define PQ1_SEL_IN	0xf34
#define PQ2_SEL_IN	0xf38
#define PQ3_SEL_IN	0xf3c
#define PQ0_SOUT_SEL	0xf40
#define PQ1_SOUT_SEL	0xf44
#define AAL0_MOUT_EN	0xf48
#define AAL1_MOUT_EN	0xf4c
#define TCC0_SOUT_SEL	0xf50
#define TCC1_SOUT_SEL	0xf54
#define HDR0_SEL_IN	0xf58
#define HDR1_SEL_IN	0xf5c
#define RSZ0_SEL_IN	0xf60
#define RSZ1_SEL_IN	0xf64
#define RSZ2_SEL_IN	0xf68
#define RSZ3_SEL_IN	0xf6c
#define WROT0_SEL_IN	0xf70
#define WROT1_SEL_IN	0xf74
#define WROT2_SEL_IN	0xf78
#define WROT3_SEL_IN	0xf7c
#define MOUT_MASK0	0xfd0
#define MOUT_MASK1	0xfd4
#define MOUT_MASK2	0xfd8
#define DL_VALID0	0xfe0
#define DL_VALID1	0xfe4
#define DL_VALID2	0xfe8
#define DL_READY0	0xff0
#define DL_READY1	0xff4
#define DL_READY2	0xff8
/* MML SYS mux types. See mtk-mml-sys.c */
#define MML_MUX_MOUT	1
#define MML_MUX_SOUT	2
#define MML_MUX_SLIN	3
/* MML SYS mux registers in mt6893 */
#define MML_ISP0_MOUT_EN	MML_MUX_MOUT ISP0_MOUT_EN
#define MML_ISP1_MOUT_EN	MML_MUX_MOUT ISP1_MOUT_EN
#define MML_ISP2_MOUT_EN	MML_MUX_MOUT ISP2_MOUT_EN
#define MML_ISP3_MOUT_EN	MML_MUX_MOUT ISP3_MOUT_EN
#define MML_RDMA0_MOUT_EN	MML_MUX_MOUT RDMA0_MOUT_EN
#define MML_RDMA1_MOUT_EN	MML_MUX_MOUT RDMA1_MOUT_EN
#define MML_RDMA2_MOUT_EN	MML_MUX_MOUT RDMA2_MOUT_EN
#define MML_RDMA3_MOUT_EN	MML_MUX_MOUT RDMA3_MOUT_EN
#define MML_PQ0_SEL_IN		MML_MUX_SLIN PQ0_SEL_IN
#define MML_PQ1_SEL_IN		MML_MUX_SLIN PQ1_SEL_IN
#define MML_PQ2_SEL_IN		MML_MUX_SLIN PQ2_SEL_IN
#define MML_PQ3_SEL_IN		MML_MUX_SLIN PQ3_SEL_IN
#define MML_PQ0_SOUT_SEL	MML_MUX_SOUT PQ0_SOUT_SEL
#define MML_PQ1_SOUT_SEL	MML_MUX_SOUT PQ1_SOUT_SEL
#define MML_AAL0_MOUT_EN	MML_MUX_MOUT AAL0_MOUT_EN
#define MML_AAL1_MOUT_EN	MML_MUX_MOUT AAL1_MOUT_EN
#define MML_TCC0_SOUT_SEL	MML_MUX_SOUT TCC0_SOUT_SEL
#define MML_TCC1_SOUT_SEL	MML_MUX_SOUT TCC1_SOUT_SEL
#define MML_HDR0_SEL_IN		MML_MUX_SLIN HDR0_SEL_IN
#define MML_HDR1_SEL_IN		MML_MUX_SLIN HDR1_SEL_IN
#define MML_RSZ0_SEL_IN		MML_MUX_SLIN RSZ0_SEL_IN
#define MML_RSZ1_SEL_IN		MML_MUX_SLIN RSZ1_SEL_IN
#define MML_RSZ2_SEL_IN		MML_MUX_SLIN RSZ2_SEL_IN
#define MML_RSZ3_SEL_IN		MML_MUX_SLIN RSZ3_SEL_IN
#define MML_WROT0_SEL_IN	MML_MUX_SLIN WROT0_SEL_IN
#define MML_WROT1_SEL_IN	MML_MUX_SLIN WROT1_SEL_IN
#define MML_WROT2_SEL_IN	MML_MUX_SLIN WROT2_SEL_IN
#define MML_WROT3_SEL_IN	MML_MUX_SLIN WROT3_SEL_IN
/* MML DL IN registers in mt6893 */
#define MML_DL_RELAY0_CFG_WD	0x920
#define MML_DL_RELAY1_CFG_WD	0x924
#define MML_DL_RELAY2_CFG_WD	0x928
#define MML_DL_RELAY3_CFG_WD	0x92c

#endif	/* _DT_BINDINGS_MML_MT6893_H */
