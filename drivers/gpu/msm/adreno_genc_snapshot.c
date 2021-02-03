// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include "adreno.h"
#include "adreno_genc.h"
#include "adreno_snapshot.h"

#define GENC_NUM_CTXTS 2
#define GENC_NUM_AXI_ARB_BLOCKS 2
#define GENC_NUM_XIN_AXI_BLOCKS 5
#define GENC_NUM_XIN_CORE_BLOCKS 4

static const unsigned int genc_gras_cluster[] = {
	0x8000, 0x8006, 0x8010, 0x8092, 0x8094, 0x809d, 0x80a0, 0x80a6,
	0x80af, 0x80f1, 0x8100, 0x8107, 0x8109, 0x8109, 0x8110, 0x8110,
	0x8400, 0x840b,
};

static const unsigned int genc_ps_cluster_rac[] = {
	0x8800, 0x8806, 0x8809, 0x8811, 0x8818, 0x881e, 0x8820, 0x8865,
	0x8870, 0x8879, 0x8880, 0x8889, 0x8890, 0x8891, 0x8898, 0x8898,
	0x88c0, 0x88c1, 0x88d0, 0x88e3, 0x8900, 0x890c, 0x890f, 0x891a,
	0x8c00, 0x8c01, 0x8c08, 0x8c10, 0x8c17, 0x8c1f, 0x8c26, 0x8c33,
};

static const unsigned int genc_ps_cluster_rbp[] = {
	0x88f0, 0x88f3, 0x890d, 0x890e, 0x8927, 0x8928, 0x8bf0, 0x8bf1,
	0x8c02, 0x8c07, 0x8c11, 0x8c16, 0x8c20, 0x8c25,
};

static const unsigned int genc_vpc_ps_cluster[] = {
	0x9200, 0x9216, 0x9218, 0x9236, 0x9300, 0x9306,
};

static const unsigned int genc_fe_cluster[] = {
	0x9300, 0x9306, 0x9800, 0x9807, 0x9b00, 0x9b07, 0xa000, 0xa009,
	0xa00e, 0xa0ef, 0xa0f8, 0xa0f8,
};

static const unsigned int genc_pc_vs_cluster[] = {
	0x9100, 0x9108, 0x9300, 0x9306, 0x9980, 0x9981, 0x9b00, 0x9b07,
};

static const unsigned int genc_isense_registers[] = {
	0x22c00, 0x22c19, 0x22c26, 0x22c2d, 0x22c2f, 0x22c36, 0x22c40, 0x22c44,
	0x22c50, 0x22c57, 0x22c60, 0x22c67, 0x22c80, 0x22c87, 0x22d25, 0x22d2a,
	0x22d2c, 0x22d32, 0x22d3e, 0x22d3f, 0x22d4e, 0x22d55, 0x22d58, 0x22d60,
	0x22d64, 0x22d64, 0x22d66, 0x22d66, 0x22d68, 0x22d6b, 0x22d6e, 0x22d76,
	0x22d78, 0x22d78, 0x22d80, 0x22d87, 0x22d90, 0x22d97, 0x22da0, 0x22da0,
	0x22db0, 0x22db7, 0x22dc0, 0x22dc2, 0x22dc4, 0x22de3, 0x2301a, 0x2301a,
	0x2301d, 0x2302a, 0x23120, 0x23121, 0x23133, 0x23133, 0x23156, 0x23157,
	0x23165, 0x23165, 0x2316d, 0x2316d, 0x23180, 0x23191,
};

static const struct sel_reg {
	unsigned int host_reg;
	unsigned int cd_reg;
	unsigned int val;
} _genc_rb_rac_aperture = {
	.host_reg = GENC_RB_RB_SUB_BLOCK_SEL_CNTL_HOST,
	.cd_reg = GENC_RB_RB_SUB_BLOCK_SEL_CNTL_CD,
	.val = 0x0,
},
_genc_rb_rbp_aperture = {
	.host_reg = GENC_RB_RB_SUB_BLOCK_SEL_CNTL_HOST,
	.cd_reg = GENC_RB_RB_SUB_BLOCK_SEL_CNTL_CD,
	.val = 0x9,
};

#define GENC_CP_CLUSTER_FE	0x1
#define GENC_CP_CLUSTER_SP_VS	0x2
#define GENC_CP_CLUSTER_PC_VS	0x3
#define GENC_CP_CLUSTER_GRAS	0x4
#define GENC_CP_CLUSTER_SP_PS	0x5
#define GENC_CP_CLUSTER_VPC_PS	0x6
#define GENC_CP_CLUSTER_PS	0x7

static struct genc_cluster_registers {
	unsigned int id;
	const unsigned int *regs;
	unsigned int num_sets;
	const struct sel_reg *sel;
	unsigned int offset0;
	unsigned int offset1;
} genc_clusters[] = {
	{ GENC_CP_CLUSTER_GRAS, genc_gras_cluster,
		ARRAY_SIZE(genc_gras_cluster)/2, NULL },
	{ GENC_CP_CLUSTER_PS, genc_ps_cluster_rac,
		ARRAY_SIZE(genc_ps_cluster_rac)/2, &_genc_rb_rac_aperture },
	{ GENC_CP_CLUSTER_PS, genc_ps_cluster_rbp,
		ARRAY_SIZE(genc_ps_cluster_rbp)/2, &_genc_rb_rbp_aperture },
	{ GENC_CP_CLUSTER_PS, genc_vpc_ps_cluster,
		ARRAY_SIZE(genc_vpc_ps_cluster)/2, NULL },
	{ GENC_CP_CLUSTER_FE, genc_fe_cluster,
		ARRAY_SIZE(genc_fe_cluster)/2, NULL },
	{ GENC_CP_CLUSTER_PC_VS, genc_pc_vs_cluster,
		ARRAY_SIZE(genc_pc_vs_cluster)/2, NULL },
};

struct genc_cluster_regs_info {
	struct genc_cluster_registers *cluster;
	unsigned int ctxt_id;
};

static const unsigned int genc_sp_vs_hlsq_cluster[] = {
	0xb800, 0xb803, 0xb820, 0xb822,
};

static const unsigned int genc_sp_vs_sp_cluster[] = {
	0xa800, 0xa824, 0xa830, 0xa83c, 0xa840, 0xa864, 0xa870, 0xa895,
	0xa8a0, 0xa8af, 0xa8c0, 0xa8c3,
};

static const unsigned int genc_hlsq_duplicate_cluster[] = {
	0xbb10, 0xbb11, 0xbb20, 0xbb29,
};

static const unsigned int genc_hlsq_2d_duplicate_cluster[] = {
	0xbd80, 0xbd80,
};

static const unsigned int genc_sp_duplicate_cluster[] = {
	0xab00, 0xab00, 0xab04, 0xab05, 0xab10, 0xab1b, 0xab20, 0xab20,
};

static const unsigned int genc_tp_duplicate_cluster[] = {
	0xb300, 0xb307, 0xb309, 0xb309, 0xb380, 0xb382,
};

static const unsigned int genc_sp_ps_hlsq_cluster[] = {
	0xb980, 0xb980, 0xb982, 0xb987, 0xb990, 0xb99b, 0xb9a0, 0xb9a2,
	0xb9c0, 0xb9c9,
};

static const unsigned int genc_sp_ps_hlsq_2d_cluster[] = {
	0xbd80, 0xbd80,
};

static const unsigned int genc_sp_ps_sp_cluster[] = {
	0xa980, 0xa9a8, 0xa9b0, 0xa9bc, 0xa9d0, 0xa9d3, 0xa9e0, 0xa9f3,
	0xaa00, 0xaa00, 0xaa30, 0xaa31, 0xaaf2, 0xaaf2,
};

static const unsigned int genc_sp_ps_sp_2d_cluster[] = {
	0xacc0, 0xacc0,
};

static const unsigned int genc_sp_ps_tp_cluster[] = {
	0xb180, 0xb183, 0xb190, 0xb191,
};

static const unsigned int genc_sp_ps_tp_2d_cluster[] = {
	0xb4c0, 0xb4d1,
};

static struct genc_cluster_dbgahb_registers {
	unsigned int id;
	unsigned int regbase;
	unsigned int statetype;
	const unsigned int *regs;
	unsigned int num_sets;
	unsigned int offset0;
	unsigned int offset1;
} genc_dbgahb_ctx_clusters[] = {
	{ GENC_CP_CLUSTER_SP_VS, 0x0002e000, 0x41, genc_sp_vs_hlsq_cluster,
		ARRAY_SIZE(genc_sp_vs_hlsq_cluster) / 2 },
	{ GENC_CP_CLUSTER_SP_VS, 0x0002a000, 0x21, genc_sp_vs_sp_cluster,
		ARRAY_SIZE(genc_sp_vs_sp_cluster) / 2 },
	{ GENC_CP_CLUSTER_SP_VS, 0x0002e000, 0x41, genc_hlsq_duplicate_cluster,
		ARRAY_SIZE(genc_hlsq_duplicate_cluster) / 2 },
	{ GENC_CP_CLUSTER_SP_VS, 0x0002f000, 0x45,
		genc_hlsq_2d_duplicate_cluster,
		ARRAY_SIZE(genc_hlsq_2d_duplicate_cluster) / 2 },
	{ GENC_CP_CLUSTER_SP_VS, 0x0002a000, 0x21, genc_sp_duplicate_cluster,
		ARRAY_SIZE(genc_sp_duplicate_cluster) / 2 },
	{ GENC_CP_CLUSTER_SP_VS, 0x0002c000, 0x1, genc_tp_duplicate_cluster,
		ARRAY_SIZE(genc_tp_duplicate_cluster) / 2 },
	{ GENC_CP_CLUSTER_SP_PS, 0x0002e000, 0x42, genc_sp_ps_hlsq_cluster,
		ARRAY_SIZE(genc_sp_ps_hlsq_cluster) / 2 },
	{ GENC_CP_CLUSTER_SP_PS, 0x0002f000, 0x46, genc_sp_ps_hlsq_2d_cluster,
		ARRAY_SIZE(genc_sp_ps_hlsq_2d_cluster) / 2 },
	{ GENC_CP_CLUSTER_SP_PS, 0x0002a000, 0x22, genc_sp_ps_sp_cluster,
		ARRAY_SIZE(genc_sp_ps_sp_cluster) / 2 },
	{ GENC_CP_CLUSTER_SP_PS, 0x0002b000, 0x26, genc_sp_ps_sp_2d_cluster,
		ARRAY_SIZE(genc_sp_ps_sp_2d_cluster) / 2 },
	{ GENC_CP_CLUSTER_SP_PS, 0x0002c000, 0x2, genc_sp_ps_tp_cluster,
		ARRAY_SIZE(genc_sp_ps_tp_cluster) / 2 },
	{ GENC_CP_CLUSTER_SP_PS, 0x0002d000, 0x6, genc_sp_ps_tp_2d_cluster,
		ARRAY_SIZE(genc_sp_ps_tp_2d_cluster) / 2 },
	{ GENC_CP_CLUSTER_SP_PS, 0x0002e000, 0x42, genc_hlsq_duplicate_cluster,
		ARRAY_SIZE(genc_hlsq_duplicate_cluster) / 2 },
	{ GENC_CP_CLUSTER_SP_PS, 0x0002a000, 0x22, genc_sp_duplicate_cluster,
		ARRAY_SIZE(genc_sp_duplicate_cluster) / 2 },
	{ GENC_CP_CLUSTER_SP_PS, 0x0002c000, 0x2, genc_tp_duplicate_cluster,
		ARRAY_SIZE(genc_tp_duplicate_cluster) / 2 },
};

struct genc_cluster_dbgahb_regs_info {
	struct genc_cluster_dbgahb_registers *cluster;
	unsigned int ctxt_id;
};

static const unsigned int genc_hlsq_non_ctx_registers[] = {
	0xbe00, 0xbe01, 0xbe04, 0xbe05, 0xbe08, 0xbe09, 0xbe10, 0xbe15,
	0xbe20, 0xbe23,
};

static const unsigned int genc_sp_non_ctx_registers[] = {
	0xae00, 0xae04, 0xae0c, 0xae0c, 0xae0f, 0xae2b, 0xae30, 0xae32,
	0xae35, 0xae35, 0xae3a, 0xae3f, 0xae50, 0xae52,
};

static const unsigned int genc_tp_non_ctx_registers[] = {
	0xb600, 0xb601, 0xb604, 0xb605, 0xb610, 0xb61b, 0xb620, 0xb623,
};

static struct genc_non_ctx_dbgahb_registers {
	unsigned int regbase;
	unsigned int statetype;
	const unsigned int *regs;
	unsigned int num_sets;
	unsigned int offset;
} genc_non_ctx_dbgahb[] = {
	{ 0x0002f800, 0x40, genc_hlsq_non_ctx_registers,
		ARRAY_SIZE(genc_hlsq_non_ctx_registers) / 2 },
	{ 0x0002b800, 0x20, genc_sp_non_ctx_registers,
		ARRAY_SIZE(genc_sp_non_ctx_registers) / 2 },
	{ 0x0002d800, 0x0, genc_tp_non_ctx_registers,
		ARRAY_SIZE(genc_tp_non_ctx_registers) / 2 },
};

static const unsigned int genc_gbif_registers[] = {
	/* GBIF */
	0x3c00, 0x3c0b, 0x3c40, 0x3c47, 0x3cc0, 0x3cd1, 0x0e3a, 0x0e3a,
};

static const unsigned int genc_rb_rac_registers[] = {
	0x8e04, 0x8e05, 0x8e07, 0x8e08, 0x8e10, 0x8e1c, 0x8e20, 0x8e25,
	0x8e28, 0x8e28, 0x8e2c, 0x8e2f, 0x8e50, 0x8e52,
};

static const unsigned int genc_rb_rbp_registers[] = {
	0x8e01, 0x8e01, 0x8e0c, 0x8e0c, 0x8e3b, 0x8e3e, 0x8e40, 0x8e43,
	0x8e53, 0x8e5f, 0x8e70, 0x8e77,
};

/*
 * Set of registers to dump for GENC on snapshot.
 * Registers in pairs - first value is the start offset, second
 * is the stop offset (inclusive)
 */

static const unsigned int genc_registers[] = {
	/* RBBM */
	0x0000, 0x0002, 0x0010, 0x0010, 0x0012, 0x0012, 0x0018, 0x001b,
	0x001e, 0x0032, 0x0038, 0x003c, 0x0042, 0x0042, 0x0044, 0x0044,
	0x0047, 0x0047, 0x0056, 0x0056, 0x00ad, 0x00ae, 0x00b0, 0x00fb,
	0x0100, 0x011d, 0x0200, 0x020d, 0x0218, 0x023d, 0x0400, 0x04f9,
	0x0500, 0x0500, 0x0505, 0x050b, 0x050e, 0x0511, 0x0533, 0x0533,
	0x0540, 0x0555, 0x05fc, 0x05ff,
	/* CP */
	0x0800, 0x0803, 0x0806, 0x0808, 0x0810, 0x0813, 0x0820, 0x0821,
	0x0823, 0x0824, 0x0826, 0x0827, 0x0830, 0x0833, 0x0840, 0x0845,
	0x084f, 0x086f, 0x0880, 0x088a, 0x08a0, 0x08ab, 0x08c0, 0x08c4,
	0x08d0, 0x08dd, 0x08f0, 0x08f3, 0x0900, 0x0903, 0x0908, 0x0911,
	0x0928, 0x093e, 0x0942, 0x094d, 0x0980, 0x0984, 0x098d, 0x0996,
	0x0998, 0x099e, 0x09a0, 0x09a6, 0x09a8, 0x09ae, 0x09b0, 0x09b1,
	0x09c2, 0x09c8, 0x0a00, 0x0a03, 0x0b00, 0x0b40, 0x0b80, 0x0b83,
	/* vsc */
	0x0c00, 0x0c04, 0x0c06, 0x0c06, 0x0c10, 0x0cd9, 0x0e00, 0x0e0e,
	/* UCHE */
	0x0e10, 0x0e13, 0x0e17, 0x0e19, 0x0e1c, 0x0e2b, 0x0e30, 0x0e32,
	0x0e38, 0x0e39, 0x0e3c, 0x0e3c,
	/* GRAS */
	0x8600, 0x8601, 0x8610, 0x861b, 0x8620, 0x8620, 0x8628, 0x862b,
	0x8630, 0x8637,
	/* VPC */
	0x9600, 0x9604, 0x9624, 0x9637,
	/* PC */
	0x9e00, 0x9e01, 0x9e03, 0x9e0e, 0x9e11, 0x9e16, 0x9e19, 0x9e19,
	0x9e1c, 0x9e1c, 0x9e20, 0x9e23, 0x9e30, 0x9e31, 0x9e34, 0x9e34,
	0x9e70, 0x9e72, 0x9e78, 0x9e79, 0x9e80, 0x9fff,
	/* VFD */
	0xa600, 0xa601, 0xa603, 0xa603, 0xa60a, 0xa60a, 0xa610, 0xa617,
	0xa630, 0xa630,
};

/*
 * Set of registers to dump for GENC before actually triggering crash dumper.
 * Registers in pairs - first value is the start offset, second
 * is the stop offset (inclusive)
 */
static const unsigned int genc_pre_crashdumper_registers[] = {
	/* RBBM: RBBM_STATUS - RBBM_STATUS3 */
	0x210, 0x213,
	/* CP: CP_STATUS_1 */
	0x825, 0x825,
};

enum genc_debugbus_id {
	GENC_DBGBUS_CP           = 0x1,
	GENC_DBGBUS_RBBM         = 0x2,
	GENC_DBGBUS_VBIF         = 0x3,
	GENC_DBGBUS_HLSQ         = 0x4,
	GENC_DBGBUS_UCHE         = 0x5,
	GENC_DBGBUS_DPM          = 0x6,
	GENC_DBGBUS_TESS         = 0x7,
	GENC_DBGBUS_PC           = 0x8,
	GENC_DBGBUS_VFDP         = 0x9,
	GENC_DBGBUS_VPC          = 0xa,
	GENC_DBGBUS_TSE          = 0xb,
	GENC_DBGBUS_RAS          = 0xc,
	GENC_DBGBUS_VSC          = 0xd,
	GENC_DBGBUS_COM          = 0xe,
	GENC_DBGBUS_LRZ          = 0x10,
	GENC_DBGBUS_A2D          = 0x11,
	GENC_DBGBUS_CCUFCHE      = 0x12,
	GENC_DBGBUS_GMU_CX       = 0x13,
	GENC_DBGBUS_RBP          = 0x14,
	GENC_DBGBUS_DCS          = 0x15,
	GENC_DBGBUS_RBBM_CFG     = 0x16,
	GENC_DBGBUS_CX           = 0x17,
	GENC_DBGBUS_GMU_GX       = 0x18,
	GENC_DBGBUS_TPFCHE       = 0x19,
	GENC_DBGBUS_GBIF_GX      = 0x1a,
	GENC_DBGBUS_GPC          = 0x1d,
	GENC_DBGBUS_LARC         = 0x1e,
	GENC_DBGBUS_HLSQ_SPTP    = 0x1f,
	GENC_DBGBUS_RB_0         = 0x20,
	GENC_DBGBUS_RB_1         = 0x21,
	GENC_DBGBUS_RB_2         = 0x22,
	GENC_DBGBUS_UCHE_WRAPPER = 0x24,
	GENC_DBGBUS_CCU_0        = 0x28,
	GENC_DBGBUS_CCU_1        = 0x29,
	GENC_DBGBUS_CCU_2        = 0x2a,
	GENC_DBGBUS_VFD_0        = 0x38,
	GENC_DBGBUS_VFD_1        = 0x39,
	GENC_DBGBUS_VFD_2        = 0x3a,
	GENC_DBGBUS_VFD_3        = 0x3b,
	GENC_DBGBUS_VFD_4        = 0x3c,
	GENC_DBGBUS_VFD_5        = 0x3d,
	GENC_DBGBUS_SP_0         = 0x40,
	GENC_DBGBUS_SP_1         = 0x41,
	GENC_DBGBUS_SP_2         = 0x42,
	GENC_DBGBUS_TPL1_0       = 0x48,
	GENC_DBGBUS_TPL1_1       = 0x49,
	GENC_DBGBUS_TPL1_2       = 0x4a,
	GENC_DBGBUS_TPL1_3       = 0x4b,
	GENC_DBGBUS_TPL1_4       = 0x4c,
	GENC_DBGBUS_TPL1_5       = 0x4d,
	GENC_DBGBUS_SPTP_0       = 0x58,
	GENC_DBGBUS_SPTP_1       = 0x59,
	GENC_DBGBUS_SPTP_2       = 0x5a,
	GENC_DBGBUS_SPTP_3       = 0x5b,
	GENC_DBGBUS_SPTP_4       = 0x5c,
	GENC_DBGBUS_SPTP_5       = 0x5d,
};

static const struct adreno_debugbus_block genc_dbgc_debugbus_blocks[] = {
	{ GENC_DBGBUS_CP, 0x100, },
	{ GENC_DBGBUS_RBBM, 0x100, },
	{ GENC_DBGBUS_HLSQ, 0x100, },
	{ GENC_DBGBUS_UCHE, 0x100, },
	{ GENC_DBGBUS_DPM, 0x100, },
	{ GENC_DBGBUS_TESS, 0x100, },
	{ GENC_DBGBUS_PC, 0x100, },
	{ GENC_DBGBUS_VFDP, 0x100, },
	{ GENC_DBGBUS_VPC, 0x100, },
	{ GENC_DBGBUS_TSE, 0x100, },
	{ GENC_DBGBUS_RAS, 0x100, },
	{ GENC_DBGBUS_VSC, 0x100, },
	{ GENC_DBGBUS_COM, 0x100, },
	{ GENC_DBGBUS_LRZ, 0x100, },
	{ GENC_DBGBUS_A2D, 0x100, },
	{ GENC_DBGBUS_CCUFCHE, 0x100, },
	{ GENC_DBGBUS_RBP, 0x100, },
	{ GENC_DBGBUS_DCS, 0x100, },
	{ GENC_DBGBUS_RBBM_CFG, 0x100, },
	{ GENC_DBGBUS_GMU_GX, 0x100, },
	{ GENC_DBGBUS_TPFCHE, 0x100, },
	{ GENC_DBGBUS_GPC, 0x100, },
	{ GENC_DBGBUS_LARC, 0x100, },
	{ GENC_DBGBUS_HLSQ_SPTP, 0x100, },
	{ GENC_DBGBUS_RB_0, 0x100, },
	{ GENC_DBGBUS_RB_1, 0x100, },
	{ GENC_DBGBUS_RB_2, 0x100, },
	{ GENC_DBGBUS_UCHE_WRAPPER, 0x100, },
	{ GENC_DBGBUS_CCU_0, 0x100, },
	{ GENC_DBGBUS_CCU_1, 0x100, },
	{ GENC_DBGBUS_CCU_2, 0x100, },
	{ GENC_DBGBUS_VFD_0, 0x100, },
	{ GENC_DBGBUS_VFD_1, 0x100, },
	{ GENC_DBGBUS_VFD_2, 0x100, },
	{ GENC_DBGBUS_VFD_3, 0x100, },
	{ GENC_DBGBUS_VFD_4, 0x100, },
	{ GENC_DBGBUS_VFD_5, 0x100, },
	{ GENC_DBGBUS_SP_0, 0x100, },
	{ GENC_DBGBUS_SP_1, 0x100, },
	{ GENC_DBGBUS_SP_2, 0x100, },
	{ GENC_DBGBUS_TPL1_0, 0x100, },
	{ GENC_DBGBUS_TPL1_1, 0x100, },
	{ GENC_DBGBUS_TPL1_2, 0x100, },
	{ GENC_DBGBUS_TPL1_3, 0x100, },
	{ GENC_DBGBUS_TPL1_4, 0x100, },
	{ GENC_DBGBUS_TPL1_5, 0x100, },
	{ GENC_DBGBUS_SPTP_0, 0x100, },
	{ GENC_DBGBUS_SPTP_1, 0x100, },
	{ GENC_DBGBUS_SPTP_2, 0x100, },
	{ GENC_DBGBUS_SPTP_3, 0x100, },
	{ GENC_DBGBUS_SPTP_4, 0x100, },
	{ GENC_DBGBUS_SPTP_5, 0x100, },
};

static const struct adreno_debugbus_block genc_vbif_debugbus_blocks = {
	GENC_DBGBUS_VBIF, 0x100,
};

static const struct adreno_debugbus_block genc_cx_dbgc_debugbus_blocks[] = {
	{ GENC_DBGBUS_GMU_CX, 0x100, },
	{ GENC_DBGBUS_CX, 0x100, },
};

#define GENC_NUM_SHADER_BANKS 3
#define GENC_SHADER_STATETYPE_SHIFT 8

enum genc_shader_obj {
	GENC_TP0_TMO_DATA               = 0x9,
	GENC_TP0_SMO_DATA               = 0xa,
	GENC_TP0_MIPMAP_BASE_DATA       = 0xb,
	GENC_TP1_TMO_DATA               = 0x19,
	GENC_TP1_SMO_DATA               = 0x1a,
	GENC_TP1_MIPMAP_BASE_DATA       = 0x1b,
	GENC_SP_INST_DATA               = 0x29,
	GENC_SP_LB_0_DATA               = 0x2a,
	GENC_SP_LB_1_DATA               = 0x2b,
	GENC_SP_LB_2_DATA               = 0x2c,
	GENC_SP_LB_3_DATA               = 0x2d,
	GENC_SP_LB_4_DATA               = 0x2e,
	GENC_SP_LB_5_DATA               = 0x2f,
	GENC_SP_CB_BINDLESS_DATA        = 0x30,
	GENC_SP_CB_LEGACY_DATA          = 0x31,
	GENC_SP_UAV_DATA                = 0x32,
	GENC_SP_INST_TAG                = 0x33,
	GENC_SP_CB_BINDLESS_TAG         = 0x34,
	GENC_SP_TMO_UMO_TAG             = 0x35,
	GENC_SP_SMO_TAG                 = 0x36,
	GENC_SP_STATE_DATA              = 0x37,
	GENC_HLSQ_CHUNK_CVS_RAM         = 0x49,
	GENC_HLSQ_CHUNK_CPS_RAM         = 0x4a,
	GENC_HLSQ_CHUNK_CVS_RAM_TAG     = 0x4b,
	GENC_HLSQ_CHUNK_CPS_RAM_TAG     = 0x4c,
	GENC_HLSQ_ICB_CVS_CB_BASE_TAG   = 0x4d,
	GENC_HLSQ_ICB_CPS_CB_BASE_TAG   = 0x4e,
	GENC_HLSQ_CVS_MISC_RAM          = 0x50,
	GENC_HLSQ_CPS_MISC_RAM          = 0x51,
	GENC_HLSQ_INST_RAM              = 0x52,
	GENC_HLSQ_GFX_CVS_CONST_RAM     = 0x53,
	GENC_HLSQ_GFX_CPS_CONST_RAM     = 0x54,
	GENC_HLSQ_CVS_MISC_RAM_TAG      = 0x55,
	GENC_HLSQ_CPS_MISC_RAM_TAG      = 0x56,
	GENC_HLSQ_INST_RAM_TAG          = 0x57,
	GENC_HLSQ_GFX_CVS_CONST_RAM_TAG = 0x58,
	GENC_HLSQ_GFX_CPS_CONST_RAM_TAG = 0x59,
	GENC_HLSQ_PWR_REST_RAM          = 0x5a,
	GENC_HLSQ_PWR_REST_TAG          = 0x5b,
	GENC_HLSQ_DATAPATH_META         = 0x60,
	GENC_HLSQ_FRONTEND_META         = 0x61,
	GENC_HLSQ_INDIRECT_META         = 0x62,
	GENC_HLSQ_BACKEND_META          = 0x63
};

struct genc_shader_block {
	u32 statetype;
	u32 sz;
	u64 offset;
};

struct genc_shader_block_info {
	struct genc_shader_block *block;
	u32 bank;
	u64 offset;
};

static struct genc_shader_block genc_shader_blocks[] = {
	{GENC_TP0_TMO_DATA,               0x200},
	{GENC_TP0_SMO_DATA,               0x80,},
	{GENC_TP0_MIPMAP_BASE_DATA,       0x3c0},
	{GENC_TP1_TMO_DATA,               0x200},
	{GENC_TP1_SMO_DATA,               0x80,},
	{GENC_TP1_MIPMAP_BASE_DATA,       0x3c0},
	{GENC_SP_INST_DATA,               0x800},
	{GENC_SP_LB_0_DATA,               0x800},
	{GENC_SP_LB_1_DATA,               0x800},
	{GENC_SP_LB_2_DATA,               0x800},
	{GENC_SP_LB_3_DATA,               0x800},
	{GENC_SP_LB_4_DATA,               0x800},
	{GENC_SP_LB_5_DATA,               0x200},
	{GENC_SP_CB_BINDLESS_DATA,        0x800},
	{GENC_SP_CB_LEGACY_DATA,          0x280,},
	{GENC_SP_UAV_DATA,                0x80,},
	{GENC_SP_INST_TAG,                0x80,},
	{GENC_SP_CB_BINDLESS_TAG,         0x80,},
	{GENC_SP_TMO_UMO_TAG,             0x80,},
	{GENC_SP_SMO_TAG,                 0x80},
	{GENC_SP_STATE_DATA,              0x3f},
	{GENC_HLSQ_CHUNK_CVS_RAM,         0x1c0},
	{GENC_HLSQ_CHUNK_CPS_RAM,         0x280},
	{GENC_HLSQ_CHUNK_CVS_RAM_TAG,     0x40,},
	{GENC_HLSQ_CHUNK_CPS_RAM_TAG,     0x40,},
	{GENC_HLSQ_ICB_CVS_CB_BASE_TAG,   0x4,},
	{GENC_HLSQ_ICB_CPS_CB_BASE_TAG,   0x4,},
	{GENC_HLSQ_CVS_MISC_RAM,          0x1c0},
	{GENC_HLSQ_CPS_MISC_RAM,          0x580},
	{GENC_HLSQ_INST_RAM,              0x800},
	{GENC_HLSQ_GFX_CVS_CONST_RAM,     0x800},
	{GENC_HLSQ_GFX_CPS_CONST_RAM,     0x800},
	{GENC_HLSQ_CVS_MISC_RAM_TAG,      0x8,},
	{GENC_HLSQ_CPS_MISC_RAM_TAG,      0x4,},
	{GENC_HLSQ_INST_RAM_TAG,          0x80,},
	{GENC_HLSQ_GFX_CVS_CONST_RAM_TAG, 0xc,},
	{GENC_HLSQ_GFX_CPS_CONST_RAM_TAG, 0x10},
	{GENC_HLSQ_PWR_REST_RAM,          0x28},
	{GENC_HLSQ_PWR_REST_TAG,          0x14},
	{GENC_HLSQ_DATAPATH_META,         0x40,},
	{GENC_HLSQ_FRONTEND_META,         0x40},
	{GENC_HLSQ_INDIRECT_META,         0x40,}
};

static struct kgsl_memdesc *genc_capturescript;
static struct kgsl_memdesc *genc_crashdump_registers;
static bool crash_dump_valid;

static struct reg_list {
	const unsigned int *regs;
	u32 count;
	const struct sel_reg *sel;
	u64 offset;
} genc_reg_list[] = {
	{ genc_registers, ARRAY_SIZE(genc_registers) / 2, NULL },
	{ genc_rb_rac_registers, ARRAY_SIZE(genc_rb_rac_registers) / 2,
		&_genc_rb_rac_aperture },
	{ genc_rb_rbp_registers, ARRAY_SIZE(genc_rb_rbp_registers) / 2,
		&_genc_rb_rbp_aperture },
};

#define REG_PAIR_COUNT(_a, _i) \
	(((_a)[(2 * (_i)) + 1] - (_a)[2 * (_i)]) + 1)

static size_t genc_legacy_snapshot_registers(struct kgsl_device *device,
		u8 *buf, size_t remain, struct reg_list *regs)
{
	struct kgsl_snapshot_registers snapshot_regs = {
		.regs = regs->regs,
		.count = regs->count,
	};

	if (regs->sel)
		kgsl_regwrite(device, regs->sel->host_reg, regs->sel->val);

	return kgsl_snapshot_dump_registers(device, buf, remain,
		&snapshot_regs);
}

static size_t genc_snapshot_registers(struct kgsl_device *device, u8 *buf,
		size_t remain, void *priv)
{
	struct kgsl_snapshot_regs *header = (struct kgsl_snapshot_regs *)buf;
	struct reg_list *regs = (struct reg_list *)priv;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	unsigned int *src;
	unsigned int j, k;
	unsigned int count = 0;

	if (!crash_dump_valid)
		return genc_legacy_snapshot_registers(device, buf, remain,
			regs);

	if (remain < sizeof(*header)) {
		SNAPSHOT_ERR_NOMEM(device, "REGISTERS");
		return 0;
	}

	src = genc_crashdump_registers->hostptr + regs->offset;
	remain -= sizeof(*header);

	for (j = 0; j < regs->count; j++) {
		unsigned int start = regs->regs[2 * j];
		unsigned int end = regs->regs[(2 * j) + 1];

		if (remain < ((end - start) + 1) * 8) {
			SNAPSHOT_ERR_NOMEM(device, "REGISTERS");
			goto out;
		}

		remain -= ((end - start) + 1) * 8;

		for (k = start; k <= end; k++, count++) {
			*data++ = k;
			*data++ = *src++;
		}
	}

out:
	header->count = count;

	/* Return the size of the section */
	return (count * 8) + sizeof(*header);
}

static size_t genc_snapshot_pre_crashdump_regs(struct kgsl_device *device,
		u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_registers pre_cdregs = {
			.regs = genc_pre_crashdumper_registers,
			.count = ARRAY_SIZE(genc_pre_crashdumper_registers)/2,
	};

	return kgsl_snapshot_dump_registers(device, buf, remain, &pre_cdregs);
}

static size_t genc_legacy_snapshot_shader(struct kgsl_device *device,
				u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_shader *header =
		(struct kgsl_snapshot_shader *) buf;
	struct genc_shader_block_info *info =
		(struct genc_shader_block_info *) priv;
	struct genc_shader_block *block = info->block;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	unsigned int read_sel, val = 0;
	int i;

	if (!device->snapshot_legacy)
		return 0;

	if (remain < SHADER_SECTION_SZ(block->sz)) {
		SNAPSHOT_ERR_NOMEM(device, "SHADER MEMORY");
		return 0;
	}

	header->type = block->statetype;
	header->index = info->bank;
	header->size = block->sz;

	read_sel = (block->statetype << GENC_SHADER_STATETYPE_SHIFT) |
		info->bank;

	/*
	 * An explicit barrier is needed so that reads do not happen before
	 * the register write.
	 */
	mb();

	for (i = 0; i < block->sz; i++)
		*data++ = val;

	return SHADER_SECTION_SZ(block->sz);
}

static size_t genc_snapshot_shader_memory(struct kgsl_device *device,
		u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_shader *header =
		(struct kgsl_snapshot_shader *) buf;
	struct genc_shader_block_info *info =
		(struct genc_shader_block_info *) priv;
	struct genc_shader_block *block = info->block;
	unsigned int *data = (unsigned int *) (buf + sizeof(*header));

	if (!crash_dump_valid)
		return genc_legacy_snapshot_shader(device, buf, remain, priv);

	if (remain < SHADER_SECTION_SZ(block->sz)) {
		SNAPSHOT_ERR_NOMEM(device, "SHADER MEMORY");
		return 0;
	}

	header->type = block->statetype;
	header->index = info->bank;
	header->size = block->sz;

	memcpy(data, genc_crashdump_registers->hostptr + info->offset,
		block->sz * sizeof(unsigned int));

	return SHADER_SECTION_SZ(block->sz);
}

static void genc_snapshot_shader(struct kgsl_device *device,
				struct kgsl_snapshot *snapshot)
{
	unsigned int i, j;
	struct genc_shader_block_info info;

	for (i = 0; i < ARRAY_SIZE(genc_shader_blocks); i++) {
		for (j = 0; j < GENC_NUM_SHADER_BANKS; j++) {
			info.block = &genc_shader_blocks[i];
			info.bank = j;
			info.offset = genc_shader_blocks[i].offset +
				(j * genc_shader_blocks[i].sz);

			/* Shader working/shadow memory */
			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_SHADER,
				snapshot, genc_snapshot_shader_memory, &info);
		}
	}
}

static void genc_snapshot_mempool(struct kgsl_device *device,
				struct kgsl_snapshot *snapshot)
{
	u32 val;

	/* set CP_CHICKEN_DBG[StabilizeMVC] to stabilize it while dumping */
	/* FIXME: read-modify-write */
	kgsl_regread(device, GENC_CP_CHICKEN_DBG, &val);
	kgsl_regwrite(device, GENC_CP_CHICKEN_DBG, val | BIT(2));

	kgsl_snapshot_indexed_registers(device, snapshot,
		GENC_CP_MEM_POOL_DBG_ADDR, GENC_CP_MEM_POOL_DBG_DATA,
		0, 0x2100);

	kgsl_regwrite(device, GENC_CP_CHICKEN_DBG, val);
}

static inline unsigned int genc_read_dbgahb(struct kgsl_device *device,
				unsigned int regbase, unsigned int reg)
{
	unsigned int read_reg = 0;
	unsigned int val;

	kgsl_regread(device, read_reg, &val);
	return val;
}

static size_t genc_legacy_snapshot_cluster_dbgahb(struct kgsl_device *device,
				u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_mvc_regs *header =
				(struct kgsl_snapshot_mvc_regs *)buf;
	struct genc_cluster_dbgahb_regs_info *info =
				(struct genc_cluster_dbgahb_regs_info *)priv;
	struct genc_cluster_dbgahb_registers *cur_cluster = info->cluster;
	unsigned int read_sel;
	unsigned int data_size = 0;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	int i, j;

	if (!device->snapshot_legacy)
		return 0;

	if (remain < sizeof(*header)) {
		SNAPSHOT_ERR_NOMEM(device, "REGISTERS");
		return 0;
	}

	remain -= sizeof(*header);

	header->ctxt_id = info->ctxt_id;
	header->cluster_id = cur_cluster->id;

	read_sel = ((cur_cluster->statetype + info->ctxt_id * 2) & 0xff) << 8;

	for (i = 0; i < cur_cluster->num_sets; i++) {
		unsigned int start = cur_cluster->regs[2 * i];
		unsigned int end = cur_cluster->regs[2 * i + 1];

		if (remain < (end - start + 3) * 4) {
			SNAPSHOT_ERR_NOMEM(device, "MVC REGISTERS");
			goto out;
		}

		remain -= (end - start + 3) * 4;
		data_size += (end - start + 3) * 4;

		*data++ = start | (1 << 31);
		*data++ = end;

		for (j = start; j <= end; j++) {
			unsigned int val;

			val = genc_read_dbgahb(device, cur_cluster->regbase, j);
			*data++ = val;

		}
	}

out:
	return data_size + sizeof(*header);
}

static size_t genc_snapshot_cluster_dbgahb(struct kgsl_device *device, u8 *buf,
				size_t remain, void *priv)
{
	struct kgsl_snapshot_mvc_regs *header =
				(struct kgsl_snapshot_mvc_regs *)buf;
	struct genc_cluster_dbgahb_regs_info *info =
				(struct genc_cluster_dbgahb_regs_info *)priv;
	struct genc_cluster_dbgahb_registers *cluster = info->cluster;
	unsigned int data_size = 0;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	int i, j;
	unsigned int *src;


	if (!crash_dump_valid)
		return genc_legacy_snapshot_cluster_dbgahb(device, buf, remain,
				info);

	if (remain < sizeof(*header)) {
		SNAPSHOT_ERR_NOMEM(device, "REGISTERS");
		return 0;
	}

	remain -= sizeof(*header);

	header->ctxt_id = info->ctxt_id;
	header->cluster_id = cluster->id;

	src = genc_crashdump_registers->hostptr +
		(header->ctxt_id ? cluster->offset1 : cluster->offset0);

	for (i = 0; i < cluster->num_sets; i++) {
		unsigned int start;
		unsigned int end;

		start = cluster->regs[2 * i];
		end = cluster->regs[2 * i + 1];

		if (remain < (end - start + 3) * 4) {
			SNAPSHOT_ERR_NOMEM(device, "MVC REGISTERS");
			goto out;
		}

		remain -= (end - start + 3) * 4;
		data_size += (end - start + 3) * 4;

		*data++ = start | (1 << 31);
		*data++ = end;
		for (j = start; j <= end; j++)
			*data++ = *src++;
	}
out:
	return data_size + sizeof(*header);
}

static size_t genc_legacy_snapshot_non_ctx_dbgahb(struct kgsl_device *device,
				u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_regs *header =
				(struct kgsl_snapshot_regs *)buf;
	struct genc_non_ctx_dbgahb_registers *regs =
				(struct genc_non_ctx_dbgahb_registers *)priv;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	int count = 0;
	unsigned int read_sel;
	int i, j;

	if (!device->snapshot_legacy)
		return 0;

	/* Figure out how many registers we are going to dump */
	for (i = 0; i < regs->num_sets; i++) {
		int start = regs->regs[i * 2];
		int end = regs->regs[i * 2 + 1];

		count += (end - start + 1);
	}

	if (remain < (count * 8) + sizeof(*header)) {
		SNAPSHOT_ERR_NOMEM(device, "REGISTERS");
		return 0;
	}

	header->count = count;

	read_sel = (regs->statetype & 0xff) << 8;

	for (i = 0; i < regs->num_sets; i++) {
		unsigned int start = regs->regs[2 * i];
		unsigned int end = regs->regs[2 * i + 1];

		for (j = start; j <= end; j++) {
			unsigned int val;

			val = genc_read_dbgahb(device, regs->regbase, j);
			*data++ = j;
			*data++ = val;

		}
	}
	return (count * 8) + sizeof(*header);
}

static size_t genc_snapshot_non_ctx_dbgahb(struct kgsl_device *device, u8 *buf,
				size_t remain, void *priv)
{
	struct kgsl_snapshot_regs *header =
				(struct kgsl_snapshot_regs *)buf;
	struct genc_non_ctx_dbgahb_registers *regs =
				(struct genc_non_ctx_dbgahb_registers *)priv;
	unsigned int count = 0;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	unsigned int i, k;
	unsigned int *src;

	if (!crash_dump_valid)
		return genc_legacy_snapshot_non_ctx_dbgahb(device, buf, remain,
				regs);

	if (remain < sizeof(*header)) {
		SNAPSHOT_ERR_NOMEM(device, "REGISTERS");
		return 0;
	}

	remain -= sizeof(*header);

	src = genc_crashdump_registers->hostptr + regs->offset;

	for (i = 0; i < regs->num_sets; i++) {
		unsigned int start;
		unsigned int end;

		start = regs->regs[2 * i];
		end = regs->regs[(2 * i) + 1];

		if (remain < (end - start + 1) * 8) {
			SNAPSHOT_ERR_NOMEM(device, "REGISTERS");
			goto out;
		}

		remain -= ((end - start) + 1) * 8;

		for (k = start; k <= end; k++, count++) {
			*data++ = k;
			*data++ = *src++;
		}
	}
out:
	header->count = count;

	/* Return the size of the section */
	return (count * 8) + sizeof(*header);
}

static void genc_snapshot_dbgahb_regs(struct kgsl_device *device,
				struct kgsl_snapshot *snapshot)
{
	int i, j;

	for (i = 0; i < ARRAY_SIZE(genc_dbgahb_ctx_clusters); i++) {
		struct genc_cluster_dbgahb_registers *cluster =
				&genc_dbgahb_ctx_clusters[i];
		struct genc_cluster_dbgahb_regs_info info;

		info.cluster = cluster;
		for (j = 0; j < GENC_NUM_CTXTS; j++) {
			info.ctxt_id = j;

			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_MVC, snapshot,
				genc_snapshot_cluster_dbgahb, &info);
		}
	}

	for (i = 0; i < ARRAY_SIZE(genc_non_ctx_dbgahb); i++) {
		kgsl_snapshot_add_section(device,
			KGSL_SNAPSHOT_SECTION_REGS, snapshot,
			genc_snapshot_non_ctx_dbgahb, &genc_non_ctx_dbgahb[i]);
	}
}

static size_t genc_legacy_snapshot_mvc(struct kgsl_device *device, u8 *buf,
				size_t remain, void *priv)
{
	struct kgsl_snapshot_mvc_regs *header =
					(struct kgsl_snapshot_mvc_regs *)buf;
	struct genc_cluster_regs_info *info =
					(struct genc_cluster_regs_info *)priv;
	struct genc_cluster_registers *cur_cluster = info->cluster;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	unsigned int ctxt = info->ctxt_id;
	unsigned int start, end, i, j, aperture_cntl = 0;
	unsigned int data_size = 0;

	if (remain < sizeof(*header)) {
		SNAPSHOT_ERR_NOMEM(device, "MVC REGISTERS");
		return 0;
	}

	remain -= sizeof(*header);

	header->ctxt_id = info->ctxt_id;
	header->cluster_id = cur_cluster->id;

	/*
	 * Set the AHB control for the Host to read from the
	 * cluster/context for this iteration.
	 */
	aperture_cntl = ((cur_cluster->id & 0x7) << 8) | (ctxt << 4) | ctxt;
	kgsl_regwrite(device, GENC_CP_APERTURE_CNTL_HOST, aperture_cntl);

	if (cur_cluster->sel)
		kgsl_regwrite(device, cur_cluster->sel->host_reg,
			cur_cluster->sel->val);

	for (i = 0; i < cur_cluster->num_sets; i++) {
		start = cur_cluster->regs[2 * i];
		end = cur_cluster->regs[2 * i + 1];

		if (remain < (end - start + 3) * 4) {
			SNAPSHOT_ERR_NOMEM(device, "MVC REGISTERS");
			goto out;
		}

		remain -= (end - start + 3) * 4;
		data_size += (end - start + 3) * 4;

		*data++ = start | (1 << 31);
		*data++ = end;
		for (j = start; j <= end; j++) {
			unsigned int val;

			kgsl_regread(device, j, &val);
			*data++ = val;
		}
	}
out:
	return data_size + sizeof(*header);
}

static size_t genc_snapshot_mvc(struct kgsl_device *device, u8 *buf,
				size_t remain, void *priv)
{
	struct kgsl_snapshot_mvc_regs *header =
				(struct kgsl_snapshot_mvc_regs *)buf;
	struct genc_cluster_regs_info *info =
				(struct genc_cluster_regs_info *)priv;
	struct genc_cluster_registers *cluster = info->cluster;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	unsigned int *src;
	int i, j;
	unsigned int start, end;
	size_t data_size = 0;

	if (!crash_dump_valid)
		return genc_legacy_snapshot_mvc(device, buf, remain, info);

	if (remain < sizeof(*header)) {
		SNAPSHOT_ERR_NOMEM(device, "MVC REGISTERS");
		return 0;
	}

	remain -= sizeof(*header);

	header->ctxt_id = info->ctxt_id;
	header->cluster_id = cluster->id;

	src = genc_crashdump_registers->hostptr +
		(header->ctxt_id ? cluster->offset1 : cluster->offset0);

	for (i = 0; i < cluster->num_sets; i++) {
		start = cluster->regs[2 * i];
		end = cluster->regs[2 * i + 1];

		if (remain < (end - start + 3) * 4) {
			SNAPSHOT_ERR_NOMEM(device, "MVC REGISTERS");
			goto out;
		}

		remain -= (end - start + 3) * 4;
		data_size += (end - start + 3) * 4;

		*data++ = start | (1 << 31);
		*data++ = end;
		for (j = start; j <= end; j++)
			*data++ = *src++;
	}

out:
	return data_size + sizeof(*header);

}

static void genc_snapshot_mvc_regs(struct kgsl_device *device,
				struct kgsl_snapshot *snapshot)
{
	int i, j;
	struct genc_cluster_regs_info info;

	for (i = 0; i < ARRAY_SIZE(genc_clusters); i++) {
		struct genc_cluster_registers *cluster = &genc_clusters[i];

		info.cluster = cluster;
		for (j = 0; j < GENC_NUM_CTXTS; j++) {
			info.ctxt_id = j;

			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_MVC, snapshot,
				genc_snapshot_mvc, &info);
		}
	}
}

/* genc_dbgc_debug_bus_read() - Read data from trace bus */
static void genc_dbgc_debug_bus_read(struct kgsl_device *device,
	unsigned int block_id, unsigned int index, unsigned int *val)
{
	unsigned int reg;

	reg = FIELD_PREP(GENMASK(7, 0), index) |
		FIELD_PREP(GENMASK(24, 16), block_id);

	kgsl_regwrite(device, GENC_DBGC_CFG_DBGBUS_SEL_A, reg);
	kgsl_regwrite(device, GENC_DBGC_CFG_DBGBUS_SEL_B, reg);
	kgsl_regwrite(device, GENC_DBGC_CFG_DBGBUS_SEL_C, reg);
	kgsl_regwrite(device, GENC_DBGC_CFG_DBGBUS_SEL_D, reg);

	/*
	 * There needs to be a delay of 1 us to ensure enough time for correct
	 * data is funneled into the trace buffer
	 */
	udelay(1);

	kgsl_regread(device, GENC_DBGC_CFG_DBGBUS_TRACE_BUF2, val);
	val++;
	kgsl_regread(device, GENC_DBGC_CFG_DBGBUS_TRACE_BUF1, val);
}

/* genc_snapshot_dbgc_debugbus_block() - Capture debug data for a gpu block */
static size_t genc_snapshot_dbgc_debugbus_block(struct kgsl_device *device,
	u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_debugbus *header =
		(struct kgsl_snapshot_debugbus *)buf;
	struct adreno_debugbus_block *block = priv;
	int i;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	unsigned int dwords;
	size_t size;

	dwords = block->dwords;

	/* For genc each debug bus data unit is 2 DWORDS */
	size = (dwords * sizeof(unsigned int) * 2) + sizeof(*header);

	if (remain < size) {
		SNAPSHOT_ERR_NOMEM(device, "DEBUGBUS");
		return 0;
	}

	header->id = block->block_id;
	if (block->block_id == GENC_DBGBUS_VBIF)
		header->id = GENC_DBGBUS_GBIF_GX;
	header->count = dwords * 2;

	for (i = 0; i < dwords; i++)
		genc_dbgc_debug_bus_read(device, block->block_id,
				i, &data[i*2]);

	return size;
}

/* genc_cx_dbgc_debug_bus_read() - Read data from trace bus */
static void genc_cx_debug_bus_read(struct kgsl_device *device,
	unsigned int block_id, unsigned int index, unsigned int *val)
{
	unsigned int reg;

	reg = FIELD_PREP(GENMASK(7, 0), index) |
		FIELD_PREP(GENMASK(24, 16), block_id);

	adreno_cx_dbgc_regwrite(device, GENC_CX_DBGC_CFG_DBGBUS_SEL_A, reg);
	adreno_cx_dbgc_regwrite(device, GENC_CX_DBGC_CFG_DBGBUS_SEL_B, reg);
	adreno_cx_dbgc_regwrite(device, GENC_CX_DBGC_CFG_DBGBUS_SEL_C, reg);
	adreno_cx_dbgc_regwrite(device, GENC_CX_DBGC_CFG_DBGBUS_SEL_D, reg);

	/*
	 * There needs to be a delay of 1 us to ensure enough time for correct
	 * data is funneled into the trace buffer
	 */
	udelay(1);

	adreno_cx_dbgc_regread(device, GENC_CX_DBGC_CFG_DBGBUS_TRACE_BUF2, val);
	val++;
	adreno_cx_dbgc_regread(device, GENC_CX_DBGC_CFG_DBGBUS_TRACE_BUF1, val);
}

/*
 * genc_snapshot_cx_dbgc_debugbus_block() - Capture debug data for a gpu
 * block from the CX DBGC block
 */
static size_t genc_snapshot_cx_dbgc_debugbus_block(struct kgsl_device *device,
	u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_debugbus *header =
		(struct kgsl_snapshot_debugbus *)buf;
	struct adreno_debugbus_block *block = priv;
	int i;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	unsigned int dwords;
	size_t size;

	dwords = block->dwords;

	/* For genc each debug bus data unit is 2 DWRODS */
	size = (dwords * sizeof(unsigned int) * 2) + sizeof(*header);

	if (remain < size) {
		SNAPSHOT_ERR_NOMEM(device, "DEBUGBUS");
		return 0;
	}

	header->id = block->block_id;
	header->count = dwords * 2;

	for (i = 0; i < dwords; i++)
		genc_cx_debug_bus_read(device, block->block_id, i,
					&data[i*2]);

	return size;
}

/* genc_snapshot_debugbus() - Capture debug bus data */
static void genc_snapshot_debugbus(struct adreno_device *adreno_dev,
		struct kgsl_snapshot *snapshot)
{
	int i;
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	kgsl_regwrite(device, GENC_DBGC_CFG_DBGBUS_CNTLT,
			FIELD_PREP(GENMASK(31, 28), 0xf));

	kgsl_regwrite(device, GENC_DBGC_CFG_DBGBUS_CNTLM,
			FIELD_PREP(GENMASK(27, 24), 0xf));

	kgsl_regwrite(device, GENC_DBGC_CFG_DBGBUS_IVTL_0, 0);
	kgsl_regwrite(device, GENC_DBGC_CFG_DBGBUS_IVTL_1, 0);
	kgsl_regwrite(device, GENC_DBGC_CFG_DBGBUS_IVTL_2, 0);
	kgsl_regwrite(device, GENC_DBGC_CFG_DBGBUS_IVTL_3, 0);

	kgsl_regwrite(device, GENC_DBGC_CFG_DBGBUS_BYTEL_0,
			FIELD_PREP(GENMASK(3, 0), 0x0) |
			FIELD_PREP(GENMASK(7, 4), 0x1) |
			FIELD_PREP(GENMASK(11, 8), 0x2) |
			FIELD_PREP(GENMASK(15, 12), 0x3) |
			FIELD_PREP(GENMASK(19, 16), 0x4) |
			FIELD_PREP(GENMASK(23, 20), 0x5) |
			FIELD_PREP(GENMASK(27, 24), 0x6) |
			FIELD_PREP(GENMASK(31, 28), 0x7));
	kgsl_regwrite(device, GENC_DBGC_CFG_DBGBUS_BYTEL_1,
			FIELD_PREP(GENMASK(3, 0), 0x8) |
			FIELD_PREP(GENMASK(7, 4), 0x9) |
			FIELD_PREP(GENMASK(11, 8), 0xa) |
			FIELD_PREP(GENMASK(15, 12), 0xb) |
			FIELD_PREP(GENMASK(19, 16), 0xc) |
			FIELD_PREP(GENMASK(23, 20), 0xd) |
			FIELD_PREP(GENMASK(27, 24), 0xe) |
			FIELD_PREP(GENMASK(31, 28), 0xf));

	kgsl_regwrite(device, GENC_DBGC_CFG_DBGBUS_MASKL_0, 0);
	kgsl_regwrite(device, GENC_DBGC_CFG_DBGBUS_MASKL_1, 0);
	kgsl_regwrite(device, GENC_DBGC_CFG_DBGBUS_MASKL_2, 0);
	kgsl_regwrite(device, GENC_DBGC_CFG_DBGBUS_MASKL_3, 0);

	adreno_cx_dbgc_regwrite(device, GENC_CX_DBGC_CFG_DBGBUS_CNTLT,
			FIELD_PREP(GENMASK(31, 28), 0xf));

	adreno_cx_dbgc_regwrite(device, GENC_CX_DBGC_CFG_DBGBUS_CNTLM,
			FIELD_PREP(GENMASK(27, 24), 0xf));

	adreno_cx_dbgc_regwrite(device, GENC_CX_DBGC_CFG_DBGBUS_IVTL_0, 0);
	adreno_cx_dbgc_regwrite(device, GENC_CX_DBGC_CFG_DBGBUS_IVTL_1, 0);
	adreno_cx_dbgc_regwrite(device, GENC_CX_DBGC_CFG_DBGBUS_IVTL_2, 0);
	adreno_cx_dbgc_regwrite(device, GENC_CX_DBGC_CFG_DBGBUS_IVTL_3, 0);

	adreno_cx_dbgc_regwrite(device, GENC_CX_DBGC_CFG_DBGBUS_BYTEL_0,
			FIELD_PREP(GENMASK(3, 0), 0x0) |
			FIELD_PREP(GENMASK(7, 4), 0x1) |
			FIELD_PREP(GENMASK(11, 8), 0x2) |
			FIELD_PREP(GENMASK(15, 12), 0x3) |
			FIELD_PREP(GENMASK(19, 16), 0x4) |
			FIELD_PREP(GENMASK(23, 20), 0x5) |
			FIELD_PREP(GENMASK(27, 24), 0x6) |
			FIELD_PREP(GENMASK(31, 28), 0x7));
	adreno_cx_dbgc_regwrite(device, GENC_CX_DBGC_CFG_DBGBUS_BYTEL_1,
			FIELD_PREP(GENMASK(3, 0), 0x8) |
			FIELD_PREP(GENMASK(7, 4), 0x9) |
			FIELD_PREP(GENMASK(11, 8), 0xa) |
			FIELD_PREP(GENMASK(15, 12), 0xb) |
			FIELD_PREP(GENMASK(19, 16), 0xc) |
			FIELD_PREP(GENMASK(23, 20), 0xd) |
			FIELD_PREP(GENMASK(27, 24), 0xe) |
			FIELD_PREP(GENMASK(31, 28), 0xf));

	adreno_cx_dbgc_regwrite(device, GENC_CX_DBGC_CFG_DBGBUS_MASKL_0, 0);
	adreno_cx_dbgc_regwrite(device, GENC_CX_DBGC_CFG_DBGBUS_MASKL_1, 0);
	adreno_cx_dbgc_regwrite(device, GENC_CX_DBGC_CFG_DBGBUS_MASKL_2, 0);
	adreno_cx_dbgc_regwrite(device, GENC_CX_DBGC_CFG_DBGBUS_MASKL_3, 0);

	for (i = 0; i < ARRAY_SIZE(genc_dbgc_debugbus_blocks); i++) {
		kgsl_snapshot_add_section(device,
			KGSL_SNAPSHOT_SECTION_DEBUGBUS,
			snapshot, genc_snapshot_dbgc_debugbus_block,
			(void *) &genc_dbgc_debugbus_blocks[i]);
	}

	/*
	 * GBIF has same debugbus as of other GPU blocks hence fall back to
	 * default path if GPU uses GBIF.
	 * GBIF uses exactly same ID as of VBIF so use it as it is.
	 */
	kgsl_snapshot_add_section(device,
			KGSL_SNAPSHOT_SECTION_DEBUGBUS,
			snapshot, genc_snapshot_dbgc_debugbus_block,
			(void *) &genc_vbif_debugbus_blocks);

	/* Dump the CX debugbus data if the block exists */
	if (adreno_is_cx_dbgc_register(device, GENC_CX_DBGC_CFG_DBGBUS_SEL_A)) {
		for (i = 0; i < ARRAY_SIZE(genc_cx_dbgc_debugbus_blocks); i++) {
			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_DEBUGBUS,
				snapshot, genc_snapshot_cx_dbgc_debugbus_block,
				(void *) &genc_cx_dbgc_debugbus_blocks[i]);
		}
		/*
		 * Get debugbus for GBIF CX part if GPU has GBIF block
		 * GBIF uses exactly same ID as of VBIF so use
		 * it as it is.
		 */
		kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_DEBUGBUS, snapshot,
				genc_snapshot_cx_dbgc_debugbus_block,
				(void *) &genc_vbif_debugbus_blocks);
	}
}



/* genc_snapshot_sqe() - Dump SQE data in snapshot */
static size_t genc_snapshot_sqe(struct kgsl_device *device, u8 *buf,
		size_t remain, void *priv)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_snapshot_debug *header = (struct kgsl_snapshot_debug *)buf;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	struct adreno_firmware *fw = ADRENO_FW(adreno_dev, ADRENO_FW_SQE);

	if (remain < DEBUG_SECTION_SZ(1)) {
		SNAPSHOT_ERR_NOMEM(device, "SQE VERSION DEBUG");
		return 0;
	}

	/* Dump the SQE firmware version */
	header->type = SNAPSHOT_DEBUG_SQE_VERSION;
	header->size = 1;
	*data = fw->version;

	return DEBUG_SECTION_SZ(1);
}

static void _genc_do_crashdump(struct kgsl_device *device)
{
	unsigned long wait_time;
	unsigned int reg = 0;

	crash_dump_valid = false;

	if (!device->snapshot_crashdumper)
		return;

	if (IS_ERR_OR_NULL(genc_capturescript) ||
		IS_ERR_OR_NULL(genc_crashdump_registers))
		return;

	/* IF the SMMU is stalled we cannot do a crash dump */
	if (genc_is_smmu_stalled(device))
		return;

	kgsl_regwrite(device, GENC_CP_CRASH_SCRIPT_BASE_LO,
			lower_32_bits(genc_capturescript->gpuaddr));
	kgsl_regwrite(device, GENC_CP_CRASH_SCRIPT_BASE_HI,
			upper_32_bits(genc_capturescript->gpuaddr));
	kgsl_regwrite(device, GENC_CP_CRASH_DUMP_CNTL, 1);

	wait_time = jiffies + msecs_to_jiffies(CP_CRASH_DUMPER_TIMEOUT);
	while (!time_after(jiffies, wait_time)) {
		kgsl_regread(device, GENC_CP_CRASH_DUMP_STATUS, &reg);
		if (reg & 0x2)
			break;
		cpu_relax();
	}

	if (!(reg & 0x2)) {
		dev_err(device->dev, "Crash dump timed out: 0x%X\n", reg);
		return;
	}

	crash_dump_valid = true;
}

static size_t genc_snapshot_isense_registers(struct kgsl_device *device,
		u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_regs *header = (struct kgsl_snapshot_regs *)buf;
	struct kgsl_snapshot_registers *regs = priv;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	int count = 0, j, k;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	/* Figure out how many registers we are going to dump */

	for (j = 0; j < regs->count; j++) {
		int start = regs->regs[j * 2];
		int end = regs->regs[j * 2 + 1];

		count += (end - start + 1);
	}

	if (remain < (count * 8) + sizeof(*header)) {
		SNAPSHOT_ERR_NOMEM(device, "ISENSE REGISTERS");
		return 0;
	}

	for (j = 0; j < regs->count; j++) {
		unsigned int start = regs->regs[j * 2];
		unsigned int end = regs->regs[j * 2 + 1];

		for (k = start; k <= end; k++) {
			unsigned int val;

			adreno_isense_regread(adreno_dev,
				k - (adreno_dev->isense_base >> 2), &val);
			*data++ = k;
			*data++ = val;
		}
	}

	header->count = count;

	/* Return the size of the section */
	return (count * 8) + sizeof(*header);
}

/* Snapshot the preemption related buffers */
static size_t snapshot_preemption_record(struct kgsl_device *device,
	u8 *buf, size_t remain, void *priv)
{
	struct kgsl_memdesc *memdesc = priv;
	struct kgsl_snapshot_gpu_object_v2 *header =
		(struct kgsl_snapshot_gpu_object_v2 *)buf;
	u8 *ptr = buf + sizeof(*header);

	if (remain < (GENC_SNAPSHOT_CP_CTXRECORD_SIZE_IN_BYTES +
						sizeof(*header))) {
		SNAPSHOT_ERR_NOMEM(device, "PREEMPTION RECORD");
		return 0;
	}

	header->size = GENC_SNAPSHOT_CP_CTXRECORD_SIZE_IN_BYTES >> 2;
	header->gpuaddr = memdesc->gpuaddr;
	header->ptbase =
		kgsl_mmu_pagetable_get_ttbr0(device->mmu.defaultpagetable);
	header->type = SNAPSHOT_GPU_OBJECT_GLOBAL;

	memcpy(ptr, memdesc->hostptr, GENC_SNAPSHOT_CP_CTXRECORD_SIZE_IN_BYTES);

	return GENC_SNAPSHOT_CP_CTXRECORD_SIZE_IN_BYTES + sizeof(*header);
}

/*
 * genc_snapshot() - GENC GPU snapshot function
 * @adreno_dev: Device being snapshotted
 * @snapshot: Pointer to the snapshot instance
 *
 * This is where all of the GENC specific bits and pieces are grabbed
 * into the snapshot memory
 */
void genc_snapshot(struct adreno_device *adreno_dev,
		struct kgsl_snapshot *snapshot)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_ringbuffer *rb;
	struct kgsl_snapshot_registers r;
	unsigned int i, roq_size = 0;
	u32 hi, lo, val;

	/*
	 * Dump debugbus data here to capture it for both
	 * GMU and GPU snapshot. Debugbus data can be accessed
	 * even if the gx headswitch is off. If gx
	 * headswitch is off, data for gx blocks will show as
	 * 0x5c00bd00.
	 */
	genc_snapshot_debugbus(adreno_dev, snapshot);

	/* RSCC registers are on cx */
	r.regs = genc_isense_registers;
	r.count = ARRAY_SIZE(genc_isense_registers) / 2;

	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_REGS,
			snapshot, genc_snapshot_isense_registers, &r);

	if (!gmu_core_dev_gx_is_on(device))
		return;

	kgsl_regread(device, GENC_CP_IB1_BASE, &lo);
	kgsl_regread(device, GENC_CP_IB1_BASE_HI, &hi);

	snapshot->ib1base = (((u64) hi) << 32) | lo;

	kgsl_regread(device, GENC_CP_IB2_BASE, &lo);
	kgsl_regread(device, GENC_CP_IB2_BASE_HI, &hi);

	snapshot->ib2base = (((u64) hi) << 32) | lo;

	kgsl_regread(device, GENC_CP_IB1_REM_SIZE, &snapshot->ib1size);
	kgsl_regread(device, GENC_CP_IB2_REM_SIZE, &snapshot->ib2size);

	/* Assert the isStatic bit before triggering snapshot */
	kgsl_regwrite(device, GENC_RBBM_SNAPSHOT_STATUS, 0x1);

	/* Dump the registers which get affected by crash dumper trigger */
	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_REGS,
		snapshot, genc_snapshot_pre_crashdump_regs, NULL);

	/* Dump gbif registers as well which get affected by crash dumper */
	adreno_snapshot_registers(device, snapshot,
			genc_gbif_registers,
			ARRAY_SIZE(genc_gbif_registers) / 2);

	/* Try to run the crash dumper */
	_genc_do_crashdump(device);

	for (i = 0; i < ARRAY_SIZE(genc_reg_list); i++) {
		kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_REGS,
			snapshot, genc_snapshot_registers, &genc_reg_list[i]);
	}

	/* CP_SQE indexed registers */
	kgsl_snapshot_indexed_registers(device, snapshot,
		GENC_CP_SQE_STAT_ADDR, GENC_CP_SQE_STAT_DATA, 0, 0x33);

	/* CP_DRAW_STATE */
	kgsl_snapshot_indexed_registers(device, snapshot,
		GENC_CP_DRAW_STATE_ADDR, GENC_CP_DRAW_STATE_DATA,
		0, 0x100);

	 /* SQE_UCODE Cache */
	kgsl_snapshot_indexed_registers(device, snapshot,
		GENC_CP_SQE_UCODE_DBG_ADDR, GENC_CP_SQE_UCODE_DBG_DATA,
		0, 0x8000);

	/* CP LPAC indexed registers */
	kgsl_snapshot_indexed_registers(device, snapshot,
			GENC_CP_SQE_AC_STAT_ADDR, GENC_CP_SQE_AC_STAT_DATA,
			0, 0x33);
	kgsl_snapshot_indexed_registers(device, snapshot,
			GENC_CP_LPAC_DRAW_STATE_ADDR,
			GENC_CP_LPAC_DRAW_STATE_DATA, 0, 0x100);
	kgsl_snapshot_indexed_registers(device, snapshot,
			GENC_CP_SQE_AC_UCODE_DBG_ADDR,
			GENC_CP_SQE_AC_UCODE_DBG_DATA, 0, 0x8000);

	roq_size = roq_size >> 14;
	kgsl_snapshot_indexed_registers(device, snapshot,
			GENC_CP_LPAC_ROQ_DBG_ADDR,
			GENC_CP_LPAC_ROQ_DBG_DATA, 0, roq_size);

	kgsl_snapshot_indexed_registers(device, snapshot,
			GENC_CP_LPAC_FIFO_DBG_ADDR, GENC_CP_LPAC_FIFO_DBG_DATA,
			0, 0x40);

	/* SQE Firmware */
	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_DEBUG,
		snapshot, genc_snapshot_sqe, NULL);

	/* Mempool debug data */
	genc_snapshot_mempool(device, snapshot);

	/* Shader memory */
	genc_snapshot_shader(device, snapshot);

	/* MVC register section */
	genc_snapshot_mvc_regs(device, snapshot);

	/* registers dumped through DBG AHB */
	genc_snapshot_dbgahb_regs(device, snapshot);

	kgsl_regread(device, GENC_RBBM_SNAPSHOT_STATUS, &val);
	if (!val)
		dev_err(device->dev,
			"Interface signals may have changed during snapshot\n");

	kgsl_regwrite(device, GENC_RBBM_SNAPSHOT_STATUS, 0x0);

	/* Preemption record */
	if (adreno_is_preemption_enabled(adreno_dev)) {
		FOR_EACH_RINGBUFFER(adreno_dev, rb, i) {
			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_GPU_OBJECT_V2,
				snapshot, snapshot_preemption_record,
				rb->preemption_desc);
		}
	}
}

static int _genc_crashdump_init_mvc(struct adreno_device *adreno_dev,
	u64 *ptr, u64 *offset)
{
	int qwords = 0;
	unsigned int i, j, k;
	unsigned int count;

	for (i = 0; i < ARRAY_SIZE(genc_clusters); i++) {
		struct genc_cluster_registers *cluster = &genc_clusters[i];

		/* The VPC registers are driven by VPC_PS cluster */
		if (cluster->regs == genc_vpc_ps_cluster)
			cluster->id = GENC_CP_CLUSTER_VPC_PS;

		if (cluster->sel) {
			ptr[qwords++] = cluster->sel->val;
			ptr[qwords++] = ((u64)cluster->sel->cd_reg << 44) |
				(1 << 21) | 1;
		}

		cluster->offset0 = *offset;
		for (j = 0; j < GENC_NUM_CTXTS; j++) {

			if (j == 1)
				cluster->offset1 = *offset;

			ptr[qwords++] = (cluster->id << 8) | (j << 4) | j;
			ptr[qwords++] =
				((u64)GENC_CP_APERTURE_CNTL_CD << 44) |
				(1 << 21) | 1;

			for (k = 0; k < cluster->num_sets; k++) {
				count = REG_PAIR_COUNT(cluster->regs, k);
				ptr[qwords++] =
				genc_crashdump_registers->gpuaddr + *offset;
				ptr[qwords++] =
				(((u64)cluster->regs[2 * k]) << 44) | count;

				*offset += count * sizeof(unsigned int);
			}
		}
	}

	return qwords;
}

static int _genc_crashdump_init_shader(struct genc_shader_block *block,
		u64 *ptr, u64 *offset)
{
	int qwords = 0;
	unsigned int j;

	/* Capture each bank in the block */
	for (j = 0; j < GENC_NUM_SHADER_BANKS; j++) {
		/* Program the aperture */
		ptr[qwords++] =
			(block->statetype << GENC_SHADER_STATETYPE_SHIFT) | j;

		/* Read all the data in one chunk */
		ptr[qwords++] = genc_crashdump_registers->gpuaddr + *offset;

		/* Remember the offset of the first bank for easy access */
		if (j == 0)
			block->offset = *offset;

		*offset += block->sz * sizeof(unsigned int);
	}

	return qwords;
}

static int _genc_crashdump_init_ctx_dbgahb(u64 *ptr, u64 *offset)
{
	int qwords = 0;
	unsigned int i, j, k;
	unsigned int count;

	for (i = 0; i < ARRAY_SIZE(genc_dbgahb_ctx_clusters); i++) {
		struct genc_cluster_dbgahb_registers *cluster =
				&genc_dbgahb_ctx_clusters[i];

		cluster->offset0 = *offset;

		for (j = 0; j < GENC_NUM_CTXTS; j++) {
			if (j == 1)
				cluster->offset1 = *offset;

			/* Program the aperture */
			ptr[qwords++] =
				((cluster->statetype + j * 2) & 0xff) << 8;

			for (k = 0; k < cluster->num_sets; k++) {
				count = REG_PAIR_COUNT(cluster->regs, k);
				ptr[qwords++] =
				genc_crashdump_registers->gpuaddr + *offset;
				*offset += count * sizeof(unsigned int);
			}
		}
	}
	return qwords;
}

static int _genc_crashdump_init_non_ctx_dbgahb(u64 *ptr, u64 *offset)
{
	int qwords = 0;
	unsigned int i, k;
	unsigned int count;

	for (i = 0; i < ARRAY_SIZE(genc_non_ctx_dbgahb); i++) {
		struct genc_non_ctx_dbgahb_registers *regs =
				&genc_non_ctx_dbgahb[i];

		regs->offset = *offset;

		/* Program the aperture */
		ptr[qwords++] = (regs->statetype & 0xff) << 8;

		for (k = 0; k < regs->num_sets; k++) {
			count = REG_PAIR_COUNT(regs->regs, k);
			ptr[qwords++] =
				genc_crashdump_registers->gpuaddr + *offset;
			*offset += count * sizeof(unsigned int);
		}
	}
	return qwords;
}

void genc_crashdump_init(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	u32 script_size = 0;
	u32 data_size = 0;
	u32 i, j, k;
	u64 *ptr;
	u64 offset = 0;

	if (!IS_ERR_OR_NULL(genc_capturescript) &&
		!IS_ERR_OR_NULL(genc_crashdump_registers))
		return;

	/*
	 * We need to allocate two buffers:
	 * 1 - the buffer to hold the draw script
	 * 2 - the buffer to hold the data
	 */

	/*
	 * To save the registers, we need 16 bytes per register pair for the
	 * script and a dword for each register in the data
	 */
	for (i = 0; i < ARRAY_SIZE(genc_reg_list); i++) {
		struct reg_list *regs = &genc_reg_list[i];

		/* 16 bytes for programming the aperture */
		if (regs->sel)
			script_size += 16;

		/* Each pair needs 16 bytes (2 qwords) */
		script_size += regs->count * 16;

		/* Each register needs a dword in the data */
		for (j = 0; j < regs->count; j++)
			data_size += REG_PAIR_COUNT(regs->regs, j) *
				sizeof(unsigned int);

	}

	/*
	 * To save the shader blocks for each block in each type we need 32
	 * bytes for the script (16 bytes to program the aperture and 16 to
	 * read the data) and then a block specific number of bytes to hold
	 * the data
	 */
	for (i = 0; i < ARRAY_SIZE(genc_shader_blocks); i++) {
		script_size += 32 * GENC_NUM_SHADER_BANKS;
		data_size += genc_shader_blocks[i].sz * sizeof(unsigned int) *
			GENC_NUM_SHADER_BANKS;
	}

	/* Calculate the script and data size for MVC registers */
	for (i = 0; i < ARRAY_SIZE(genc_clusters); i++) {
		struct genc_cluster_registers *cluster = &genc_clusters[i];

		/* 16 bytes if cluster sel exists */
		if (cluster->sel)
			script_size += 16;

		for (j = 0; j < GENC_NUM_CTXTS; j++) {

			/* 16 bytes for programming the aperture */
			script_size += 16;

			/* Reading each pair of registers takes 16 bytes */
			script_size += 16 * cluster->num_sets;

			/* A dword per register read from the cluster list */
			for (k = 0; k < cluster->num_sets; k++)
				data_size += REG_PAIR_COUNT(cluster->regs, k) *
						sizeof(unsigned int);
		}
	}

	/* Calculate the script and data size for debug AHB registers */
	for (i = 0; i < ARRAY_SIZE(genc_dbgahb_ctx_clusters); i++) {
		struct genc_cluster_dbgahb_registers *cluster =
				&genc_dbgahb_ctx_clusters[i];

		for (j = 0; j < GENC_NUM_CTXTS; j++) {

			/* 16 bytes for programming the aperture */
			script_size += 16;

			/* Reading each pair of registers takes 16 bytes */
			script_size += 16 * cluster->num_sets;

			/* A dword per register read from the cluster list */
			for (k = 0; k < cluster->num_sets; k++)
				data_size += REG_PAIR_COUNT(cluster->regs, k) *
						sizeof(unsigned int);
		}
	}

	/*
	 * Calculate the script and data size for non context debug
	 * AHB registers
	 */
	for (i = 0; i < ARRAY_SIZE(genc_non_ctx_dbgahb); i++) {
		struct genc_non_ctx_dbgahb_registers *regs =
				&genc_non_ctx_dbgahb[i];

		/* 16 bytes for programming the aperture */
		script_size += 16;

		/* Reading each pair of registers takes 16 bytes */
		script_size += 16 * regs->num_sets;

		/* A dword per register read from the cluster list */
		for (k = 0; k < regs->num_sets; k++)
			data_size += REG_PAIR_COUNT(regs->regs, k) *
				sizeof(unsigned int);
	}

	/* Now allocate the script and data buffers */

	/* The script buffers needs 2 extra qwords on the end */
	if (IS_ERR_OR_NULL(genc_capturescript))
		genc_capturescript = kgsl_allocate_global(device,
			script_size + 16, 0, KGSL_MEMFLAGS_GPUREADONLY,
			KGSL_MEMDESC_PRIVILEGED, "capturescript");

	if (IS_ERR(genc_capturescript))
		return;

	if (IS_ERR_OR_NULL(genc_crashdump_registers))
		genc_crashdump_registers = kgsl_allocate_global(device,
			data_size, 0, 0, KGSL_MEMDESC_PRIVILEGED,
			"capturescript_regs");

	if (IS_ERR(genc_crashdump_registers))
		return;

	/* Build the crash script */

	ptr = (u64 *)genc_capturescript->hostptr;

	/* For the registers, program a read command for each pair */
	for (i = 0; i < ARRAY_SIZE(genc_reg_list); i++) {
		struct reg_list *regs = &genc_reg_list[i];

		regs->offset = offset;

		/* Program the SEL_CNTL_CD register appropriately */
		if (regs->sel) {
			*ptr++ = regs->sel->val;
			*ptr++ = (((u64)regs->sel->cd_reg << 44)) |
					(1 << 21) | 1;
		}

		for (j = 0; j < regs->count; j++) {
			u32 r = REG_PAIR_COUNT(regs->regs, j);
			*ptr++ = genc_crashdump_registers->gpuaddr + offset;
			*ptr++ = (((u64) regs->regs[2 * j]) << 44) | r;
			offset += r * sizeof(u32);
		}
	}

	/* Program each shader block */
	for (i = 0; i < ARRAY_SIZE(genc_shader_blocks); i++) {
		ptr += _genc_crashdump_init_shader(&genc_shader_blocks[i], ptr,
							&offset);
	}

	/* Program the capturescript for the MVC regsiters */
	ptr += _genc_crashdump_init_mvc(adreno_dev, ptr, &offset);

	ptr += _genc_crashdump_init_ctx_dbgahb(ptr, &offset);

	ptr += _genc_crashdump_init_non_ctx_dbgahb(ptr, &offset);

	*ptr++ = 0;
	*ptr++ = 0;
}
