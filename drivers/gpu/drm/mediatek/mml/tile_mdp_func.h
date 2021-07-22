/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __TILE_MDP_FUNC_H__
#define __TILE_MDP_FUNC_H__

enum tile_func_id {
	NULL_TILE_ID = LAST_MODULE_ID_OF_START,
	TILE_FUNC_MDP_BASE = 0,
	TILE_FUNC_CAMIN_ID = MML_CAMIN,
	TILE_FUNC_CAMIN2_ID,
	TILE_FUNC_CAMIN3_ID,
	TILE_FUNC_CAMIN4_ID,
	TILE_FUNC_RDMA0_ID,
	TILE_FUNC_RDMA1_ID,
	TILE_FUNC_RDMA2_ID,
	TILE_FUNC_RDMA3_ID,
	TILE_FUNC_FG0_ID,
	TILE_FUNC_FG1_ID,
	TILE_FUNC_PQ0_SOUT_ID,
	TILE_FUNC_PQ1_SOUT_ID,
	TILE_FUNC_HDR0_ID,
	TILE_FUNC_HDR1_ID,
	TILE_FUNC_COLOR0_ID,
	TILE_FUNC_COLOR1_ID,
	TILE_FUNC_AAL0_ID,
	TILE_FUNC_AAL1_ID,
	TILE_FUNC_AAL2_ID,
	TILE_FUNC_AAL3_ID,
	TILE_FUNC_PRZ0_ID,
	TILE_FUNC_PRZ1_ID,
	TILE_FUNC_PRZ2_ID,
	TILE_FUNC_PRZ3_ID,
	TILE_FUNC_TDSHP0_ID,
	TILE_FUNC_TDSHP1_ID,
	TILE_FUNC_TDSHP2_ID,
	TILE_FUNC_TDSHP3_ID,
	TILE_FUNC_TCC0_ID,
	TILE_FUNC_TCC1_ID,
	TILE_FUNC_TCC2_ID,
	TILE_FUNC_TCC3_ID,
	TILE_FUNC_WROT0_ID,
	TILE_FUNC_WROT1_ID,
	TILE_FUNC_WROT2_ID,
	TILE_FUNC_WROT3_ID,
};

/* a, b c, d, e reserved */
/* function id */
/* function name */
/* tile type: 0x1 non-fixed func to configure, 0x2 rdma, 0x4 wdma, 0x8 crop_en, 0x10 backward update by direct-link */
/* tile group, 0: ISP group, 1: CDP group 2: resizer with offset & crop */
/* tile group except for 2 will restrict last end < current end (to ensure WDMA end at same time) */
/* tile loss, l_loss, r_loss, t_loss, b_loss, in_x, int_y, out_x, out_y */
/* init function name, default NULL */
/* forward function name, default NULL */
/* back function name, default NULL */
/* calculate tile reg function name, default NULL */
/* input tile constraint, 0: no check, 1: to clip when enabled */
/* output tile constraint, 0: no check, 1: to clip when enabled */

/* TILE_TYPE_LOSS (0x1) post process by c model */
/* TILE_TYPE_RDMA (0x2) */
/* TILE_TYPE_WDMA (0x4) */
/* TILE_TYPE_CROP_EN (0x8) */
/* TILE_TYPE_DONT_CARE_END (0x10) */
#define MDP_TILE_INIT_PROPERTY_LUT(CMD, a, b, c, d, e, f, g, h) \
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_CAMIN_ID,       CAMIN, 0,              0, 0, 0, 0, 0, 1, 1, 1, 1, NULL, NULL, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_CAMIN2_ID,     CAMIN2, 0,              0, 0, 0, 0, 0, 1, 1, 1, 1, NULL, NULL, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_CAMIN3_ID,     CAMIN3, 0,              0, 0, 0, 0, 0, 1, 1, 1, 1, NULL, NULL, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_CAMIN4_ID,     CAMIN4, 0,              0, 0, 0, 0, 0, 1, 1, 1, 1, NULL, NULL, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_RDMA0_ID,       RDMA0, TILE_TYPE_RDMA, 0, 0, 0, 0, 0, 1, 1, 1, 1, tile_rdma_init, tile_rdma_for, tile_rdma_back, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_RDMA1_ID,       RDMA1, TILE_TYPE_RDMA, 0, 0, 0, 0, 0, 1, 1, 1, 1, tile_rdma_init, tile_rdma_for, tile_rdma_back, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_RDMA2_ID,       RDMA2, TILE_TYPE_RDMA, 0, 0, 0, 0, 0, 1, 1, 1, 1, tile_rdma_init, tile_rdma_for, tile_rdma_back, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_RDMA3_ID,       RDMA3, TILE_TYPE_RDMA, 0, 0, 0, 0, 0, 1, 1, 1, 1, tile_rdma_init, tile_rdma_for, tile_rdma_back, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_FG0_ID,           FG0, 0,              0, 0, 0, 0, 0, 1, 1, 1, 1, NULL, NULL, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_FG1_ID,           FG1, 0,              0, 0, 0, 0, 0, 1, 1, 1, 1, NULL, NULL, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_PQ0_SOUT_ID, PQ0_SOUT, 0,              0, 0, 0, 0, 0, 1, 1, 1, 1, NULL, NULL, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_PQ1_SOUT_ID, PQ1_SOUT, 0,              0, 0, 0, 0, 0, 1, 1, 1, 1, NULL, NULL, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_HDR0_ID,         HDR0, 0,              0, 0, 0, 0, 0, 1, 1, 1, 1, tile_hdr_init, tile_hdr_for, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_HDR1_ID,         HDR1, 0,              0, 0, 0, 0, 0, 1, 1, 1, 1, tile_hdr_init, tile_hdr_for, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_COLOR0_ID,     COLOR0, 0,              0, 0, 0, 0, 0, 1, 1, 1, 1, NULL, NULL, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_COLOR1_ID,     COLOR1, 0,              0, 0, 0, 0, 0, 1, 1, 1, 1, NULL, NULL, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_AAL0_ID,         AAL0, 0,              0, 0, 0, 0, 0, 1, 1, 1, 1, tile_aal_init, tile_aal_for, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_AAL1_ID,         AAL1, 0,              0, 0, 0, 0, 0, 1, 1, 1, 1, tile_aal_init, tile_aal_for, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_AAL2_ID,         AAL2, 0,              0, 0, 0, 0, 0, 1, 1, 1, 1, tile_aal_init, tile_aal_for, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_AAL3_ID,         AAL3, 0,              0, 0, 0, 0, 0, 1, 1, 1, 1, tile_aal_init, tile_aal_for, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_PRZ0_ID,         SCL0, 0,              0, 0, 0, 0, 0, 1, 1, 1, 1, tile_prz_init, tile_prz_for, tile_prz_back, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_PRZ1_ID,         SCL1, 0,              0, 0, 0, 0, 0, 1, 1, 1, 1, tile_prz_init, tile_prz_for, tile_prz_back, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_PRZ2_ID,         SCL2, 0,              0, 0, 0, 0, 0, 1, 1, 1, 1, tile_prz_init, tile_prz_for, tile_prz_back, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_PRZ3_ID,         SCL3, 0,              0, 0, 0, 0, 0, 1, 1, 1, 1, tile_prz_init, tile_prz_for, tile_prz_back, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_TDSHP0_ID,     TDSHP0, 0,              0, 0, 0, 0, 0, 1, 1, 1, 1, tile_tdshp_init, NULL, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_TDSHP1_ID,     TDSHP1, 0,              0, 0, 0, 0, 0, 1, 1, 1, 1, tile_tdshp_init, NULL, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_TDSHP2_ID,     TDSHP2, 0,              0, 0, 0, 0, 0, 1, 1, 1, 1, tile_tdshp_init, NULL, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_TDSHP3_ID,     TDSHP3, 0,              0, 0, 0, 0, 0, 1, 1, 1, 1, tile_tdshp_init, NULL, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_TCC0_ID,         TCC0, 0,              0, 0, 0, 0, 0, 1, 1, 1, 1, NULL, NULL, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_TCC1_ID,         TCC1, 0,              0, 0, 0, 0, 0, 1, 1, 1, 1, NULL, NULL, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_TCC2_ID,         TCC2, 0,              0, 0, 0, 0, 0, 1, 1, 1, 1, NULL, NULL, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_TCC3_ID,         TCC3, 0,              0, 0, 0, 0, 0, 1, 1, 1, 1, NULL, NULL, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_WROT0_ID,       WROT0, TILE_TYPE_WDMA, 0, 0, 0, 0, 0, 1, 1, 1, 1, tile_wrot_init, tile_wrot_for, tile_wrot_back, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_WROT1_ID,       WROT1, TILE_TYPE_WDMA, 0, 0, 0, 0, 0, 1, 1, 1, 1, tile_wrot_init, tile_wrot_for, tile_wrot_back, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_WROT2_ID,       WROT2, TILE_TYPE_WDMA, 0, 0, 0, 0, 0, 1, 1, 1, 1, tile_wrot_init, tile_wrot_for, tile_wrot_back, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_WROT3_ID,       WROT3, TILE_TYPE_WDMA, 0, 0, 0, 0, 0, 1, 1, 1, 1, tile_wrot_init, tile_wrot_for, tile_wrot_back, NULL, 0, 0, 0, 0)\

#endif
