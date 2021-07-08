/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __TILE_MDP_FUNC_H__
#define __TILE_MDP_FUNC_H__

/* output disable register LUT */
/* a, b, c, d reserved */
/* d: ptr of tile_reg_map, reserved */
/* function id */
/* function name */
/* tile module output disable reg */
#define MDP_TILE_FUNC_OUTPUT_DISABLE_LUT(CMD, a, b, c, d, e) \
	CMD(a, b, d, e, NULL_TILE_ID, NULL, false)\

#define MDP_TILE_INIT_PROPERTY_LUT(CMD, a, b, c, d, e, f, g, h) \
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_CAMIN_ID,       CAMIN, 0,              TILE_TDR_EDGE_GROUP_OTHER, 0, 0, 0, 0, 1, 1, 1, 1, NULL, NULL, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_CAMIN2_ID,     CAMIN2, 0,              TILE_TDR_EDGE_GROUP_OTHER, 0, 0, 0, 0, 1, 1, 1, 1, NULL, NULL, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_CAMIN3_ID,     CAMIN3, 0,              TILE_TDR_EDGE_GROUP_OTHER, 0, 0, 0, 0, 1, 1, 1, 1, NULL, NULL, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_CAMIN4_ID,     CAMIN4, 0,              TILE_TDR_EDGE_GROUP_OTHER, 0, 0, 0, 0, 1, 1, 1, 1, NULL, NULL, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_RDMA0_ID,       RDMA0, TILE_TYPE_RDMA, TILE_TDR_EDGE_GROUP_OTHER, 0, 0, 0, 0, 1, 1, 1, 1, tile_rdma_init, tile_rdma_for, tile_rdma_back, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_RDMA1_ID,       RDMA1, TILE_TYPE_RDMA, TILE_TDR_EDGE_GROUP_OTHER, 0, 0, 0, 0, 1, 1, 1, 1, tile_rdma_init, tile_rdma_for, tile_rdma_back, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_RDMA2_ID,       RDMA2, TILE_TYPE_RDMA, TILE_TDR_EDGE_GROUP_OTHER, 0, 0, 0, 0, 1, 1, 1, 1, tile_rdma_init, tile_rdma_for, tile_rdma_back, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_RDMA3_ID,       RDMA3, TILE_TYPE_RDMA, TILE_TDR_EDGE_GROUP_OTHER, 0, 0, 0, 0, 1, 1, 1, 1, tile_rdma_init, tile_rdma_for, tile_rdma_back, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_FG0_ID,           FG0, 0,              TILE_TDR_EDGE_GROUP_OTHER, 0, 0, 0, 0, 1, 1, 1, 1, NULL, NULL, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_FG1_ID,           FG1, 0,              TILE_TDR_EDGE_GROUP_OTHER, 0, 0, 0, 0, 1, 1, 1, 1, NULL, NULL, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_PQ0_SOUT_ID, PQ0_SOUT, 0,              TILE_TDR_EDGE_GROUP_OTHER, 0, 0, 0, 0, 1, 1, 1, 1, NULL, NULL, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_PQ1_SOUT_ID, PQ1_SOUT, 0,              TILE_TDR_EDGE_GROUP_OTHER, 0, 0, 0, 0, 1, 1, 1, 1, NULL, NULL, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_HDR0_ID,         HDR0, 0,              TILE_TDR_EDGE_GROUP_OTHER, 0, 0, 0, 0, 1, 1, 1, 1, tile_hdr_init, tile_hdr_for, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_HDR1_ID,         HDR1, 0,              TILE_TDR_EDGE_GROUP_OTHER, 0, 0, 0, 0, 1, 1, 1, 1, tile_hdr_init, tile_hdr_for, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_COLOR0_ID,     COLOR0, 0,              TILE_TDR_EDGE_GROUP_OTHER, 0, 0, 0, 0, 1, 1, 1, 1, NULL, NULL, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_COLOR1_ID,     COLOR1, 0,              TILE_TDR_EDGE_GROUP_OTHER, 0, 0, 0, 0, 1, 1, 1, 1, NULL, NULL, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_AAL0_ID,         AAL0, 0,              TILE_TDR_EDGE_GROUP_OTHER, 0, 0, 0, 0, 1, 1, 1, 1, tile_aal_init, tile_aal_for, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_AAL1_ID,         AAL1, 0,              TILE_TDR_EDGE_GROUP_OTHER, 0, 0, 0, 0, 1, 1, 1, 1, tile_aal_init, tile_aal_for, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_AAL2_ID,         AAL2, 0,              TILE_TDR_EDGE_GROUP_OTHER, 0, 0, 0, 0, 1, 1, 1, 1, tile_aal_init, tile_aal_for, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_AAL3_ID,         AAL3, 0,              TILE_TDR_EDGE_GROUP_OTHER, 0, 0, 0, 0, 1, 1, 1, 1, tile_aal_init, tile_aal_for, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_PRZ0_ID,         SCL0, 0,              TILE_TDR_EDGE_GROUP_OTHER, 0, 0, 0, 0, 1, 1, 1, 1, tile_prz_init, tile_prz_for, tile_prz_back, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_PRZ1_ID,         SCL1, 0,              TILE_TDR_EDGE_GROUP_OTHER, 0, 0, 0, 0, 1, 1, 1, 1, tile_prz_init, tile_prz_for, tile_prz_back, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_PRZ2_ID,         SCL2, 0,              TILE_TDR_EDGE_GROUP_OTHER, 0, 0, 0, 0, 1, 1, 1, 1, tile_prz_init, tile_prz_for, tile_prz_back, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_PRZ3_ID,         SCL3, 0,              TILE_TDR_EDGE_GROUP_OTHER, 0, 0, 0, 0, 1, 1, 1, 1, tile_prz_init, tile_prz_for, tile_prz_back, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_TDSHP0_ID,     TDSHP0, 0,              TILE_TDR_EDGE_GROUP_OTHER, 0, 0, 0, 0, 1, 1, 1, 1, tile_tdshp_init, NULL, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_TDSHP1_ID,     TDSHP1, 0,              TILE_TDR_EDGE_GROUP_OTHER, 0, 0, 0, 0, 1, 1, 1, 1, tile_tdshp_init, NULL, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_TDSHP2_ID,     TDSHP2, 0,              TILE_TDR_EDGE_GROUP_OTHER, 0, 0, 0, 0, 1, 1, 1, 1, tile_tdshp_init, NULL, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_TDSHP3_ID,     TDSHP3, 0,              TILE_TDR_EDGE_GROUP_OTHER, 0, 0, 0, 0, 1, 1, 1, 1, tile_tdshp_init, NULL, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_TCC0_ID,         TCC0, 0,              TILE_TDR_EDGE_GROUP_OTHER, 0, 0, 0, 0, 1, 1, 1, 1, NULL, NULL, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_TCC1_ID,         TCC1, 0,              TILE_TDR_EDGE_GROUP_OTHER, 0, 0, 0, 0, 1, 1, 1, 1, NULL, NULL, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_TCC2_ID,         TCC2, 0,              TILE_TDR_EDGE_GROUP_OTHER, 0, 0, 0, 0, 1, 1, 1, 1, NULL, NULL, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_TCC3_ID,         TCC3, 0,              TILE_TDR_EDGE_GROUP_OTHER, 0, 0, 0, 0, 1, 1, 1, 1, NULL, NULL, NULL, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_WROT0_ID,       WROT0, TILE_TYPE_WDMA, TILE_TDR_EDGE_GROUP_OTHER, 0, 0, 0, 0, 1, 1, 1, 1, tile_wrot_init, tile_wrot_for, tile_wrot_back, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_WROT1_ID,       WROT1, TILE_TYPE_WDMA, TILE_TDR_EDGE_GROUP_OTHER, 0, 0, 0, 0, 1, 1, 1, 1, tile_wrot_init, tile_wrot_for, tile_wrot_back, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_WROT2_ID,       WROT2, TILE_TYPE_WDMA, TILE_TDR_EDGE_GROUP_OTHER, 0, 0, 0, 0, 1, 1, 1, 1, tile_wrot_init, tile_wrot_for, tile_wrot_back, NULL, 0, 0, 0, 0)\
	CMD(a, b, c, d, e, f, g, h, TILE_FUNC_WROT3_ID,       WROT3, TILE_TYPE_WDMA, TILE_TDR_EDGE_GROUP_OTHER, 0, 0, 0, 0, 1, 1, 1, 1, tile_wrot_init, tile_wrot_for, tile_wrot_back, NULL, 0, 0, 0, 0)\

#endif
