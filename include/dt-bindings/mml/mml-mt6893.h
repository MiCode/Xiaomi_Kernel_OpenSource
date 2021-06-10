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
#define MML_PQ0_SOUT		9
#define MML_PQ1_SOUT		10
#define MML_RSZ0		11
#define MML_RSZ1		12
#define MML_WROT0		13
#define MML_WROT1		14
#define MML_WROT2		15
#define MML_WROT3		16
#define MML_ENGINE_TOTAL	17
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

/* MML mux types */
#define MOUT	1
#define SOUT	2
#define SLIN	3
/* MML mux registers in mt6893 */
#define MML_ISP0_MOUT_EN	MOUT 0xf10
#define MML_ISP1_MOUT_EN	MOUT 0xf14
#define MML_ISP2_MOUT_EN	MOUT 0xf18
#define MML_ISP3_MOUT_EN	MOUT 0xf1c
#define MML_RDMA0_MOUT_EN	MOUT 0xf20
#define MML_RDMA1_MOUT_EN	MOUT 0xf24
#define MML_RDMA2_MOUT_EN	MOUT 0xf28
#define MML_RDMA3_MOUT_EN	MOUT 0xf2c
#define MML_PQ0_SEL_IN		SLIN 0xf30
#define MML_PQ1_SEL_IN		SLIN 0xf34
#define MML_PQ2_SEL_IN		SLIN 0xf38
#define MML_PQ3_SEL_IN		SLIN 0xf3c
#define MML_PQ0_SOUT_SEL	SOUT 0xf40
#define MML_PQ1_SOUT_SEL	SOUT 0xf44
#define MML_AAL0_MOUT_EN	MOUT 0xf48
#define MML_AAL1_MOUT_EN	MOUT 0xf4c
#define MML_TCC0_SOUT_SEL	SOUT 0xf50
#define MML_TCC1_SOUT_SEL	SOUT 0xf54
#define MML_HDR0_SEL_IN		SLIN 0xf58
#define MML_HDR1_SEL_IN		SLIN 0xf5c
#define MML_RSZ0_SEL_IN		SLIN 0xf60
#define MML_RSZ1_SEL_IN		SLIN 0xf64
#define MML_RSZ2_SEL_IN		SLIN 0xf68
#define MML_RSZ3_SEL_IN		SLIN 0xf6c
#define MML_WROT0_SEL_IN	SLIN 0xf70
#define MML_WROT1_SEL_IN	SLIN 0xf74
#define MML_WROT2_SEL_IN	SLIN 0xf78
#define MML_WROT3_SEL_IN	SLIN 0xf7c

#endif	/* _DT_BINDINGS_MML_MT6893_H */
