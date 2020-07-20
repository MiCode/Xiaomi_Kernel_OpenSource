// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#include "adreno.h"
#include "adreno_a6xx.h"
#include "adreno_snapshot.h"

#define A6XX_NUM_CTXTS 2
#define A6XX_NUM_AXI_ARB_BLOCKS 2
#define A6XX_NUM_XIN_AXI_BLOCKS 5
#define A6XX_NUM_XIN_CORE_BLOCKS 4

/* Snapshot section size of each CP preemption record for A6XX  */
#define A6XX_SNAPSHOT_CP_CTXRECORD_SIZE_IN_BYTES (64 * 1024)

static const unsigned int a6xx_gras_cluster[] = {
	0x8000, 0x8006, 0x8010, 0x8092, 0x8094, 0x809D, 0x80A0, 0x80A6,
	0x80AF, 0x80F1, 0x8100, 0x8107, 0x8109, 0x8109, 0x8110, 0x8110,
	0x8400, 0x840B,
};

static const unsigned int a6xx_ps_cluster_rac[] = {
	0x8800, 0x8806, 0x8809, 0x8811, 0x8818, 0x881E, 0x8820, 0x8865,
	0x8870, 0x8879, 0x8880, 0x8889, 0x8890, 0x8891, 0x8898, 0x8898,
	0x88C0, 0x88C1, 0x88D0, 0x88E3, 0x8900, 0x890C, 0x890F, 0x891A,
	0x8C00, 0x8C01, 0x8C08, 0x8C10, 0x8C17, 0x8C1F, 0x8C26, 0x8C33,
};

static const unsigned int a6xx_ps_cluster_rbp[] = {
	0x88F0, 0x88F3, 0x890D, 0x890E, 0x8927, 0x8928, 0x8BF0, 0x8BF1,
	0x8C02, 0x8C07, 0x8C11, 0x8C16, 0x8C20, 0x8C25,
};

static const unsigned int a6xx_vpc_ps_cluster[] = {
	0x9200, 0x9216, 0x9218, 0x9236, 0x9300, 0x9306,
};

static const unsigned int a6xx_fe_cluster[] = {
	0x9300, 0x9306, 0x9800, 0x9806, 0x9B00, 0x9B07, 0xA000, 0xA009,
	0xA00E, 0xA0EF, 0xA0F8, 0xA0F8,
};

static const unsigned int a6xx_pc_vs_cluster[] = {
	0x9100, 0x9108, 0x9300, 0x9306, 0x9980, 0x9981, 0x9B00, 0x9B07,
};

static const unsigned int a630_rscc_snapshot_registers[] = {
	0x23400, 0x23434, 0x23436, 0x23436, 0x23480, 0x23484, 0x23489, 0x2348C,
	0x23491, 0x23494, 0x23499, 0x2349C, 0x234A1, 0x234A4, 0x234A9, 0x234AC,
	0x23500, 0x23502, 0x23504, 0x23507, 0x23514, 0x23519, 0x23524, 0x2352B,
	0x23580, 0x23597, 0x23740, 0x23741, 0x23744, 0x23747, 0x2374C, 0x23787,
	0x237EC, 0x237EF, 0x237F4, 0x2382F, 0x23894, 0x23897, 0x2389C, 0x238D7,
	0x2393C, 0x2393F, 0x23944, 0x2397F,
};

static const unsigned int a6xx_rscc_snapshot_registers[] = {
	0x23400, 0x23434, 0x23436, 0x23436, 0x23440, 0x23440, 0x23480, 0x23484,
	0x23489, 0x2348C, 0x23491, 0x23494, 0x23499, 0x2349C, 0x234A1, 0x234A4,
	0x234A9, 0x234AC, 0x23500, 0x23502, 0x23504, 0x23507, 0x23514, 0x23519,
	0x23524, 0x2352B, 0x23580, 0x23597, 0x23740, 0x23741, 0x23744, 0x23747,
	0x2374C, 0x23787, 0x237EC, 0x237EF, 0x237F4, 0x2382F, 0x23894, 0x23897,
	0x2389C, 0x238D7, 0x2393C, 0x2393F, 0x23944, 0x2397F,
};

static const unsigned int a650_rscc_registers[] = {
	0x38000, 0x38034, 0x38036, 0x38036, 0x38040, 0x38042, 0x38080, 0x38084,
	0x38089, 0x3808C, 0x38091, 0x38094, 0x38099, 0x3809C, 0x380A1, 0x380A4,
	0x380A9, 0x380AC, 0x38100, 0x38102, 0x38104, 0x38107, 0x38114, 0x38119,
	0x38124, 0x3812E, 0x38180, 0x38197, 0x38340, 0x38341, 0x38344, 0x38347,
	0x3834C, 0x3834F, 0x38351, 0x38354, 0x38356, 0x38359, 0x3835B, 0x3835E,
	0x38360, 0x38363, 0x38365, 0x38368, 0x3836A, 0x3836D, 0x3836F, 0x38372,
	0x383EC, 0x383EF, 0x383F4, 0x383F7, 0x383F9, 0x383FC, 0x383FE, 0x38401,
	0x38403, 0x38406, 0x38408, 0x3840B, 0x3840D, 0x38410, 0x38412, 0x38415,
	0x38417, 0x3841A, 0x38494, 0x38497, 0x3849C, 0x3849F, 0x384A1, 0x384A4,
	0x384A6, 0x384A9, 0x384AB, 0x384AE, 0x384B0, 0x384B3, 0x384B5, 0x384B8,
	0x384BA, 0x384BD, 0x384BF, 0x384C2, 0x3853C, 0x3853F, 0x38544, 0x38547,
	0x38549, 0x3854C, 0x3854E, 0x38551, 0x38553, 0x38556, 0x38558, 0x3855B,
	0x3855D, 0x38560, 0x38562, 0x38565, 0x38567, 0x3856A, 0x385E4, 0x385E7,
	0x385EC, 0x385EF, 0x385F1, 0x385F4, 0x385F6, 0x385F9, 0x385FB, 0x385FE,
	0x38600, 0x38603, 0x38605, 0x38608, 0x3860A, 0x3860D, 0x3860F, 0x38612,
	0x3868C, 0x3868F, 0x38694, 0x38697, 0x38699, 0x3869C, 0x3869E, 0x386A1,
	0x386A3, 0x386A6, 0x386A8, 0x386AB, 0x386AD, 0x386B0, 0x386B2, 0x386B5,
	0x386B7, 0x386BA, 0x38734, 0x38737, 0x3873C, 0x3873F, 0x38741, 0x38744,
	0x38746, 0x38749, 0x3874B, 0x3874E, 0x38750, 0x38753, 0x38755, 0x38758,
	0x3875A, 0x3875D, 0x3875F, 0x38762, 0x387DC, 0x387DF, 0x387E4, 0x387E7,
	0x387E9, 0x387EC, 0x387EE, 0x387F1, 0x387F3, 0x387F6, 0x387F8, 0x387FB,
	0x387FD, 0x38800, 0x38802, 0x38805, 0x38807, 0x3880A, 0x38884, 0x38887,
	0x3888C, 0x3888F, 0x38891, 0x38894, 0x38896, 0x38899, 0x3889B, 0x3889E,
	0x388A0, 0x388A3, 0x388A5, 0x388A8, 0x388AA, 0x388AD, 0x388AF, 0x388B2,
	0x3892C, 0x3892F, 0x38934, 0x38937, 0x38939, 0x3893C, 0x3893E, 0x38941,
	0x38943, 0x38946, 0x38948, 0x3894B, 0x3894D, 0x38950, 0x38952, 0x38955,
	0x38957, 0x3895A, 0x38B50, 0x38B51, 0x38B53, 0x38B55, 0x38B5A, 0x38B5A,
	0x38B5F, 0x38B5F, 0x38B64, 0x38B64, 0x38B69, 0x38B69, 0x38B6E, 0x38B6E,
	0x38B73, 0x38B73, 0x38BF8, 0x38BF8, 0x38BFD, 0x38BFD, 0x38C02, 0x38C02,
	0x38C07, 0x38C07, 0x38C0C, 0x38C0C, 0x38C11, 0x38C11, 0x38C16, 0x38C16,
	0x38C1B, 0x38C1B, 0x38CA0, 0x38CA0, 0x38CA5, 0x38CA5, 0x38CAA, 0x38CAA,
	0x38CAF, 0x38CAF, 0x38CB4, 0x38CB4, 0x38CB9, 0x38CB9, 0x38CBE, 0x38CBE,
	0x38CC3, 0x38CC3, 0x38D48, 0x38D48, 0x38D4D, 0x38D4D, 0x38D52, 0x38D52,
	0x38D57, 0x38D57, 0x38D5C, 0x38D5C, 0x38D61, 0x38D61, 0x38D66, 0x38D66,
	0x38D6B, 0x38D6B, 0x38DF0, 0x38DF0, 0x38DF5, 0x38DF5, 0x38DFA, 0x38DFA,
	0x38DFF, 0x38DFF, 0x38E04, 0x38E04, 0x38E09, 0x38E09, 0x38E0E, 0x38E0E,
	0x38E13, 0x38E13, 0x38E98, 0x38E98, 0x38E9D, 0x38E9D, 0x38EA2, 0x38EA2,
	0x38EA7, 0x38EA7, 0x38EAC, 0x38EAC, 0x38EB1, 0x38EB1, 0x38EB6, 0x38EB6,
	0x38EBB, 0x38EBB, 0x38F40, 0x38F40, 0x38F45, 0x38F45, 0x38F4A, 0x38F4A,
	0x38F4F, 0x38F4F, 0x38F54, 0x38F54, 0x38F59, 0x38F59, 0x38F5E, 0x38F5E,
	0x38F63, 0x38F63, 0x38FE8, 0x38FE8, 0x38FED, 0x38FED, 0x38FF2, 0x38FF2,
	0x38FF7, 0x38FF7, 0x38FFC, 0x38FFC, 0x39001, 0x39001, 0x39006, 0x39006,
	0x3900B, 0x3900B, 0x39090, 0x39090, 0x39095, 0x39095, 0x3909A, 0x3909A,
	0x3909F, 0x3909F, 0x390A4, 0x390A4, 0x390A9, 0x390A9, 0x390AE, 0x390AE,
	0x390B3, 0x390B3, 0x39138, 0x39138, 0x3913D, 0x3913D, 0x39142, 0x39142,
	0x39147, 0x39147, 0x3914C, 0x3914C, 0x39151, 0x39151, 0x39156, 0x39156,
	0x3915B, 0x3915B,
};

static const unsigned int a650_isense_registers[] = {
	0x22C00, 0x22C19, 0x22C26, 0x22C2D, 0x22C2F, 0x22C36, 0x22C40, 0x22C44,
	0x22C50, 0x22C57, 0x22C60, 0x22C67, 0x22C80, 0x22C87, 0x22D25, 0x22D2A,
	0x22D2C, 0x22D32, 0x22D3E, 0x22D3F, 0x22D4E, 0x22D55, 0x22D58, 0x22D60,
	0x22D64, 0x22D64, 0x22D66, 0x22D66, 0x22D68, 0x22D6B, 0x22D6E, 0x22D76,
	0x22D78, 0x22D78, 0x22D80, 0x22D87, 0x22D90, 0x22D97, 0x22DA0, 0x22DA0,
	0x22DB0, 0x22DB7, 0x22DC0, 0x22DC2, 0x22DC4, 0x22DE3, 0x2301A, 0x2301A,
	0x2301D, 0x2302A, 0x23120, 0x23121, 0x23133, 0x23133, 0x23156, 0x23157,
	0x23165, 0x23165, 0x2316D, 0x2316D, 0x23180, 0x23191,
};

static const struct sel_reg {
	unsigned int host_reg;
	unsigned int cd_reg;
	unsigned int val;
} _a6xx_rb_rac_aperture = {
	.host_reg = A6XX_RB_RB_SUB_BLOCK_SEL_CNTL_HOST,
	.cd_reg = A6XX_RB_RB_SUB_BLOCK_SEL_CNTL_CD,
	.val = 0x0,
},
_a6xx_rb_rbp_aperture = {
	.host_reg = A6XX_RB_RB_SUB_BLOCK_SEL_CNTL_HOST,
	.cd_reg = A6XX_RB_RB_SUB_BLOCK_SEL_CNTL_CD,
	.val = 0x9,
};

static struct a6xx_cluster_registers {
	unsigned int id;
	const unsigned int *regs;
	unsigned int num_sets;
	const struct sel_reg *sel;
	unsigned int offset0;
	unsigned int offset1;
} a6xx_clusters[] = {
	{ CP_CLUSTER_GRAS, a6xx_gras_cluster, ARRAY_SIZE(a6xx_gras_cluster)/2,
		NULL },
	{ CP_CLUSTER_PS, a6xx_ps_cluster_rac, ARRAY_SIZE(a6xx_ps_cluster_rac)/2,
		&_a6xx_rb_rac_aperture },
	{ CP_CLUSTER_PS, a6xx_ps_cluster_rbp, ARRAY_SIZE(a6xx_ps_cluster_rbp)/2,
		&_a6xx_rb_rbp_aperture },
	{ CP_CLUSTER_PS, a6xx_vpc_ps_cluster, ARRAY_SIZE(a6xx_vpc_ps_cluster)/2,
		NULL },
	{ CP_CLUSTER_FE, a6xx_fe_cluster, ARRAY_SIZE(a6xx_fe_cluster)/2,
		NULL },
	{ CP_CLUSTER_PC_VS, a6xx_pc_vs_cluster,
		ARRAY_SIZE(a6xx_pc_vs_cluster)/2, NULL },
};

struct a6xx_cluster_regs_info {
	struct a6xx_cluster_registers *cluster;
	unsigned int ctxt_id;
};

static const unsigned int a6xx_sp_vs_hlsq_cluster[] = {
	0xB800, 0xB803, 0xB820, 0xB822,
};

static const unsigned int a6xx_sp_vs_sp_cluster[] = {
	0xA800, 0xA824, 0xA830, 0xA83C, 0xA840, 0xA864, 0xA870, 0xA895,
	0xA8A0, 0xA8AF, 0xA8C0, 0xA8C3,
};

static const unsigned int a6xx_hlsq_duplicate_cluster[] = {
	0xBB10, 0xBB11, 0xBB20, 0xBB29,
};

static const unsigned int a6xx_hlsq_2d_duplicate_cluster[] = {
	0xBD80, 0xBD80,
};

static const unsigned int a6xx_sp_duplicate_cluster[] = {
	0xAB00, 0xAB00, 0xAB04, 0xAB05, 0xAB10, 0xAB1B, 0xAB20, 0xAB20,
};

static const unsigned int a6xx_tp_duplicate_cluster[] = {
	0xB300, 0xB307, 0xB309, 0xB309, 0xB380, 0xB382,
};

static const unsigned int a6xx_sp_ps_hlsq_cluster[] = {
	0xB980, 0xB980, 0xB982, 0xB987, 0xB990, 0xB99B, 0xB9A0, 0xB9A2,
	0xB9C0, 0xB9C9,
};

static const unsigned int a6xx_sp_ps_hlsq_2d_cluster[] = {
	0xBD80, 0xBD80,
};

static const unsigned int a6xx_sp_ps_sp_cluster[] = {
	0xA980, 0xA9A8, 0xA9B0, 0xA9BC, 0xA9D0, 0xA9D3, 0xA9E0, 0xA9F3,
	0xAA00, 0xAA00, 0xAA30, 0xAA31, 0xAAF2, 0xAAF2,
};

static const unsigned int a6xx_sp_ps_sp_2d_cluster[] = {
	0xACC0, 0xACC0,
};

static const unsigned int a6xx_sp_ps_tp_cluster[] = {
	0xB180, 0xB183, 0xB190, 0xB191,
};

static const unsigned int a6xx_sp_ps_tp_2d_cluster[] = {
	0xB4C0, 0xB4D1,
};

static struct a6xx_cluster_dbgahb_registers {
	unsigned int id;
	unsigned int regbase;
	unsigned int statetype;
	const unsigned int *regs;
	unsigned int num_sets;
	unsigned int offset0;
	unsigned int offset1;
} a6xx_dbgahb_ctx_clusters[] = {
	{ CP_CLUSTER_SP_VS, 0x0002E000, 0x41, a6xx_sp_vs_hlsq_cluster,
		ARRAY_SIZE(a6xx_sp_vs_hlsq_cluster) / 2 },
	{ CP_CLUSTER_SP_VS, 0x0002A000, 0x21, a6xx_sp_vs_sp_cluster,
		ARRAY_SIZE(a6xx_sp_vs_sp_cluster) / 2 },
	{ CP_CLUSTER_SP_VS, 0x0002E000, 0x41, a6xx_hlsq_duplicate_cluster,
		ARRAY_SIZE(a6xx_hlsq_duplicate_cluster) / 2 },
	{ CP_CLUSTER_SP_VS, 0x0002F000, 0x45, a6xx_hlsq_2d_duplicate_cluster,
		ARRAY_SIZE(a6xx_hlsq_2d_duplicate_cluster) / 2 },
	{ CP_CLUSTER_SP_VS, 0x0002A000, 0x21, a6xx_sp_duplicate_cluster,
		ARRAY_SIZE(a6xx_sp_duplicate_cluster) / 2 },
	{ CP_CLUSTER_SP_VS, 0x0002C000, 0x1, a6xx_tp_duplicate_cluster,
		ARRAY_SIZE(a6xx_tp_duplicate_cluster) / 2 },
	{ CP_CLUSTER_SP_PS, 0x0002E000, 0x42, a6xx_sp_ps_hlsq_cluster,
		ARRAY_SIZE(a6xx_sp_ps_hlsq_cluster) / 2 },
	{ CP_CLUSTER_SP_PS, 0x0002F000, 0x46, a6xx_sp_ps_hlsq_2d_cluster,
		ARRAY_SIZE(a6xx_sp_ps_hlsq_2d_cluster) / 2 },
	{ CP_CLUSTER_SP_PS, 0x0002A000, 0x22, a6xx_sp_ps_sp_cluster,
		ARRAY_SIZE(a6xx_sp_ps_sp_cluster) / 2 },
	{ CP_CLUSTER_SP_PS, 0x0002B000, 0x26, a6xx_sp_ps_sp_2d_cluster,
		ARRAY_SIZE(a6xx_sp_ps_sp_2d_cluster) / 2 },
	{ CP_CLUSTER_SP_PS, 0x0002C000, 0x2, a6xx_sp_ps_tp_cluster,
		ARRAY_SIZE(a6xx_sp_ps_tp_cluster) / 2 },
	{ CP_CLUSTER_SP_PS, 0x0002D000, 0x6, a6xx_sp_ps_tp_2d_cluster,
		ARRAY_SIZE(a6xx_sp_ps_tp_2d_cluster) / 2 },
	{ CP_CLUSTER_SP_PS, 0x0002E000, 0x42, a6xx_hlsq_duplicate_cluster,
		ARRAY_SIZE(a6xx_hlsq_duplicate_cluster) / 2 },
	{ CP_CLUSTER_SP_PS, 0x0002A000, 0x22, a6xx_sp_duplicate_cluster,
		ARRAY_SIZE(a6xx_sp_duplicate_cluster) / 2 },
	{ CP_CLUSTER_SP_PS, 0x0002C000, 0x2, a6xx_tp_duplicate_cluster,
		ARRAY_SIZE(a6xx_tp_duplicate_cluster) / 2 },
};

struct a6xx_cluster_dbgahb_regs_info {
	struct a6xx_cluster_dbgahb_registers *cluster;
	unsigned int ctxt_id;
};

static const unsigned int a6xx_hlsq_non_ctx_registers[] = {
	0xBE00, 0xBE01, 0xBE04, 0xBE05, 0xBE08, 0xBE09, 0xBE10, 0xBE15,
	0xBE20, 0xBE23,
};

static const unsigned int a6xx_sp_non_ctx_registers[] = {
	0xAE00, 0xAE04, 0xAE0C, 0xAE0C, 0xAE0F, 0xAE2B, 0xAE30, 0xAE32,
	0xAE35, 0xAE35, 0xAE3A, 0xAE3F, 0xAE50, 0xAE52,
};

static const unsigned int a6xx_tp_non_ctx_registers[] = {
	0xB600, 0xB601, 0xB604, 0xB605, 0xB610, 0xB61B, 0xB620, 0xB623,
};

static struct a6xx_non_ctx_dbgahb_registers {
	unsigned int regbase;
	unsigned int statetype;
	const unsigned int *regs;
	unsigned int num_sets;
	unsigned int offset;
} a6xx_non_ctx_dbgahb[] = {
	{ 0x0002F800, 0x40, a6xx_hlsq_non_ctx_registers,
		ARRAY_SIZE(a6xx_hlsq_non_ctx_registers) / 2 },
	{ 0x0002B800, 0x20, a6xx_sp_non_ctx_registers,
		ARRAY_SIZE(a6xx_sp_non_ctx_registers) / 2 },
	{ 0x0002D800, 0x0, a6xx_tp_non_ctx_registers,
		ARRAY_SIZE(a6xx_tp_non_ctx_registers) / 2 },
};

static const unsigned int a6xx_vbif_ver_20xxxxxx_registers[] = {
	/* VBIF */
	0x3000, 0x3007, 0x300C, 0x3014, 0x3018, 0x302D, 0x3030, 0x3031,
	0x3034, 0x3036, 0x303C, 0x303D, 0x3040, 0x3040, 0x3042, 0x3042,
	0x3049, 0x3049, 0x3058, 0x3058, 0x305A, 0x3061, 0x3064, 0x3068,
	0x306C, 0x306D, 0x3080, 0x3088, 0x308B, 0x308C, 0x3090, 0x3094,
	0x3098, 0x3098, 0x309C, 0x309C, 0x30C0, 0x30C0, 0x30C8, 0x30C8,
	0x30D0, 0x30D0, 0x30D8, 0x30D8, 0x30E0, 0x30E0, 0x3100, 0x3100,
	0x3108, 0x3108, 0x3110, 0x3110, 0x3118, 0x3118, 0x3120, 0x3120,
	0x3124, 0x3125, 0x3129, 0x3129, 0x3131, 0x3131, 0x3154, 0x3154,
	0x3156, 0x3156, 0x3158, 0x3158, 0x315A, 0x315A, 0x315C, 0x315C,
	0x315E, 0x315E, 0x3160, 0x3160, 0x3162, 0x3162, 0x340C, 0x340C,
	0x3410, 0x3410, 0x3800, 0x3801,
};

static const unsigned int a6xx_gbif_registers[] = {
	/* GBIF */
	0x3C00, 0X3C0B, 0X3C40, 0X3C47, 0X3CC0, 0X3CD1, 0xE3A, 0xE3A,
};

static const unsigned int a6xx_rb_rac_registers[] = {
	0x8E04, 0x8E05, 0x8E07, 0x8E08, 0x8E10, 0x8E1C, 0x8E20, 0x8E25,
	0x8E28, 0x8E28, 0x8E2C, 0x8E2F, 0x8E50, 0x8E52,
};

static const unsigned int a6xx_rb_rbp_registers[] = {
	0x8E01, 0x8E01, 0x8E0C, 0x8E0C, 0x8E3B, 0x8E3E, 0x8E40, 0x8E43,
	0x8E53, 0x8E5F, 0x8E70, 0x8E77,
};

static const struct adreno_vbif_snapshot_registers
a6xx_vbif_snapshot_registers[] = {
	{ 0x20040000, 0xFF000000, a6xx_vbif_ver_20xxxxxx_registers,
				ARRAY_SIZE(a6xx_vbif_ver_20xxxxxx_registers)/2},
};

/*
 * Set of registers to dump for A6XX on snapshot.
 * Registers in pairs - first value is the start offset, second
 * is the stop offset (inclusive)
 */

static const unsigned int a6xx_registers[] = {
	/* RBBM */
	0x0000, 0x0002, 0x0010, 0x0010, 0x0012, 0x0012, 0x0018, 0x001B,
	0x001e, 0x0032, 0x0038, 0x003C, 0x0042, 0x0042, 0x0044, 0x0044,
	0x0047, 0x0047, 0x0056, 0x0056, 0x00AD, 0x00AE, 0x00B0, 0x00FB,
	0x0100, 0x011D, 0x0200, 0x020D, 0x0218, 0x023D, 0x0400, 0x04F9,
	0x0500, 0x0500, 0x0505, 0x050B, 0x050E, 0x0511, 0x0533, 0x0533,
	0x0540, 0x0555,
	/* CP */
	0x0800, 0x0803, 0x0806, 0x0808, 0x0810, 0x0813, 0x0820, 0x0821,
	0x0823, 0x0824, 0x0826, 0x0827, 0x0830, 0x0833, 0x0840, 0x0845,
	0x084F, 0x088A, 0x08A0, 0x08AB, 0x08C0, 0x08C4, 0x08D0, 0x08DD,
	0x08F0, 0x08F3, 0x0900, 0x0903, 0x0908, 0x0911, 0x0928, 0x093E,
	0x0942, 0x094D, 0x0980, 0x0984, 0x098D, 0x0996, 0x0998, 0x099E,
	0x09A0, 0x09A6, 0x09A8, 0x09AE, 0x09B0, 0x09B4, 0x09C2, 0x09C9,
	0x0A00, 0x0A04,
	/* VSC */
	0x0C00, 0x0C04, 0x0C06, 0x0C06, 0x0C10, 0x0CD9, 0x0E00, 0x0E0E,
	/* UCHE */
	0x0E10, 0x0E13, 0x0E17, 0x0E19, 0x0E1C, 0x0E2B, 0x0E30, 0x0E32,
	0x0E38, 0x0E39,
	/* GRAS */
	0x8600, 0x8601, 0x8610, 0x861B, 0x8620, 0x8620, 0x8628, 0x862B,
	0x8630, 0x8637,
	/* VPC */
	0x9600, 0x9604, 0x9624, 0x9637,
	/* PC */
	0x9E00, 0x9E01, 0x9E03, 0x9E0E, 0x9E11, 0x9E16, 0x9E19, 0x9E19,
	0x9E1C, 0x9E1C, 0x9E20, 0x9E23, 0x9E30, 0x9E31, 0x9E34, 0x9E34,
	0x9E70, 0x9E72, 0x9E78, 0x9E79, 0x9E80, 0x9FFF,
	/* VFD */
	0xA600, 0xA601, 0xA603, 0xA603, 0xA60A, 0xA60A, 0xA610, 0xA617,
	0xA630, 0xA630,
};

/*
 * Set of registers to dump for A6XX before actually triggering crash dumper.
 * Registers in pairs - first value is the start offset, second
 * is the stop offset (inclusive)
 */
static const unsigned int a6xx_pre_crashdumper_registers[] = {
	/* RBBM: RBBM_STATUS - RBBM_STATUS3 */
	0x210, 0x213,
	/* CP: CP_STATUS_1 */
	0x825, 0x825,
};

static const unsigned int a6xx_gmu_wrapper_registers[] = {
	/* GMU CX */
	0x1f840, 0x1f840, 0x1f844, 0x1f845, 0x1f887, 0x1f889,
	/* GMU AO*/
	0x23b0C, 0x23b0E, 0x23b15, 0x23b15,
	/* GPU CC */
	0x24000, 0x24012, 0x24040, 0x24052, 0x24400, 0x24404, 0x24407, 0x2440B,
	0x24415, 0x2441C, 0x2441E, 0x2442D, 0x2443C, 0x2443D, 0x2443F, 0x24440,
	0x24442, 0x24449, 0x24458, 0x2445A, 0x24540, 0x2455E, 0x24800, 0x24802,
	0x24C00, 0x24C02, 0x25400, 0x25402, 0x25800, 0x25802, 0x25C00, 0x25C02,
	0x26000, 0x26002,
};

enum a6xx_debugbus_id {
	A6XX_DBGBUS_CP           = 0x1,
	A6XX_DBGBUS_RBBM         = 0x2,
	A6XX_DBGBUS_VBIF         = 0x3,
	A6XX_DBGBUS_HLSQ         = 0x4,
	A6XX_DBGBUS_UCHE         = 0x5,
	A6XX_DBGBUS_DPM          = 0x6,
	A6XX_DBGBUS_TESS         = 0x7,
	A6XX_DBGBUS_PC           = 0x8,
	A6XX_DBGBUS_VFDP         = 0x9,
	A6XX_DBGBUS_VPC          = 0xa,
	A6XX_DBGBUS_TSE          = 0xb,
	A6XX_DBGBUS_RAS          = 0xc,
	A6XX_DBGBUS_VSC          = 0xd,
	A6XX_DBGBUS_COM          = 0xe,
	A6XX_DBGBUS_COM_1        = 0xf,
	A6XX_DBGBUS_LRZ          = 0x10,
	A6XX_DBGBUS_A2D          = 0x11,
	A6XX_DBGBUS_CCUFCHE      = 0x12,
	A6XX_DBGBUS_GMU_CX       = 0x13,
	A6XX_DBGBUS_RBP          = 0x14,
	A6XX_DBGBUS_DCS          = 0x15,
	A6XX_DBGBUS_RBBM_CFG     = 0x16,
	A6XX_DBGBUS_CX           = 0x17,
	A6XX_DBGBUS_GMU_GX       = 0x18,
	A6XX_DBGBUS_TPFCHE       = 0x19,
	A6XX_DBGBUS_GBIF_GX      = 0x1a,
	A6XX_DBGBUS_GPC          = 0x1d,
	A6XX_DBGBUS_LARC         = 0x1e,
	A6XX_DBGBUS_HLSQ_SPTP    = 0x1f,
	A6XX_DBGBUS_RB_0         = 0x20,
	A6XX_DBGBUS_RB_1         = 0x21,
	A6XX_DBGBUS_RB_2         = 0x22,
	A6XX_DBGBUS_UCHE_WRAPPER = 0x24,
	A6XX_DBGBUS_CCU_0        = 0x28,
	A6XX_DBGBUS_CCU_1        = 0x29,
	A6XX_DBGBUS_CCU_2        = 0x2a,
	A6XX_DBGBUS_VFD_0        = 0x38,
	A6XX_DBGBUS_VFD_1        = 0x39,
	A6XX_DBGBUS_VFD_2        = 0x3a,
	A6XX_DBGBUS_VFD_3        = 0x3b,
	A6XX_DBGBUS_VFD_4        = 0x3c,
	A6XX_DBGBUS_VFD_5        = 0x3d,
	A6XX_DBGBUS_SP_0         = 0x40,
	A6XX_DBGBUS_SP_1         = 0x41,
	A6XX_DBGBUS_SP_2         = 0x42,
	A6XX_DBGBUS_TPL1_0       = 0x48,
	A6XX_DBGBUS_TPL1_1       = 0x49,
	A6XX_DBGBUS_TPL1_2       = 0x4a,
	A6XX_DBGBUS_TPL1_3       = 0x4b,
	A6XX_DBGBUS_TPL1_4       = 0x4c,
	A6XX_DBGBUS_TPL1_5       = 0x4d,
	A6XX_DBGBUS_SPTP_0       = 0x58,
	A6XX_DBGBUS_SPTP_1       = 0x59,
	A6XX_DBGBUS_SPTP_2       = 0x5a,
	A6XX_DBGBUS_SPTP_3       = 0x5b,
	A6XX_DBGBUS_SPTP_4       = 0x5c,
	A6XX_DBGBUS_SPTP_5       = 0x5d,
};

static const struct adreno_debugbus_block a6xx_dbgc_debugbus_blocks[] = {
	{ A6XX_DBGBUS_CP, 0x100, },
	{ A6XX_DBGBUS_RBBM, 0x100, },
	{ A6XX_DBGBUS_HLSQ, 0x100, },
	{ A6XX_DBGBUS_UCHE, 0x100, },
	{ A6XX_DBGBUS_DPM, 0x100, },
	{ A6XX_DBGBUS_TESS, 0x100, },
	{ A6XX_DBGBUS_PC, 0x100, },
	{ A6XX_DBGBUS_VFDP, 0x100, },
	{ A6XX_DBGBUS_VPC, 0x100, },
	{ A6XX_DBGBUS_TSE, 0x100, },
	{ A6XX_DBGBUS_RAS, 0x100, },
	{ A6XX_DBGBUS_VSC, 0x100, },
	{ A6XX_DBGBUS_COM, 0x100, },
	{ A6XX_DBGBUS_LRZ, 0x100, },
	{ A6XX_DBGBUS_A2D, 0x100, },
	{ A6XX_DBGBUS_CCUFCHE, 0x100, },
	{ A6XX_DBGBUS_RBP, 0x100, },
	{ A6XX_DBGBUS_DCS, 0x100, },
	{ A6XX_DBGBUS_RBBM_CFG, 0x100, },
	{ A6XX_DBGBUS_GMU_GX, 0x100, },
	{ A6XX_DBGBUS_TPFCHE, 0x100, },
	{ A6XX_DBGBUS_GPC, 0x100, },
	{ A6XX_DBGBUS_LARC, 0x100, },
	{ A6XX_DBGBUS_HLSQ_SPTP, 0x100, },
	{ A6XX_DBGBUS_RB_0, 0x100, },
	{ A6XX_DBGBUS_RB_1, 0x100, },
	{ A6XX_DBGBUS_UCHE_WRAPPER, 0x100, },
	{ A6XX_DBGBUS_CCU_0, 0x100, },
	{ A6XX_DBGBUS_CCU_1, 0x100, },
	{ A6XX_DBGBUS_VFD_0, 0x100, },
	{ A6XX_DBGBUS_VFD_1, 0x100, },
	{ A6XX_DBGBUS_VFD_2, 0x100, },
	{ A6XX_DBGBUS_VFD_3, 0x100, },
	{ A6XX_DBGBUS_SP_0, 0x100, },
	{ A6XX_DBGBUS_SP_1, 0x100, },
	{ A6XX_DBGBUS_TPL1_0, 0x100, },
	{ A6XX_DBGBUS_TPL1_1, 0x100, },
	{ A6XX_DBGBUS_TPL1_2, 0x100, },
	{ A6XX_DBGBUS_TPL1_3, 0x100, },
};

static const struct adreno_debugbus_block a6xx_vbif_debugbus_blocks = {
	A6XX_DBGBUS_VBIF, 0x100,
};

static const struct adreno_debugbus_block a6xx_cx_dbgc_debugbus_blocks[] = {
	{ A6XX_DBGBUS_GMU_CX, 0x100, },
	{ A6XX_DBGBUS_CX, 0x100, },
};

static const struct adreno_debugbus_block a650_dbgc_debugbus_blocks[] = {
	{ A6XX_DBGBUS_RB_2, 0x100, },
	{ A6XX_DBGBUS_CCU_2, 0x100, },
	{ A6XX_DBGBUS_VFD_4, 0x100, },
	{ A6XX_DBGBUS_VFD_5, 0x100, },
	{ A6XX_DBGBUS_SP_2, 0x100, },
	{ A6XX_DBGBUS_TPL1_4, 0x100, },
	{ A6XX_DBGBUS_TPL1_5, 0x100, },
	{ A6XX_DBGBUS_SPTP_0, 0x100, },
	{ A6XX_DBGBUS_SPTP_1, 0x100, },
	{ A6XX_DBGBUS_SPTP_2, 0x100, },
	{ A6XX_DBGBUS_SPTP_3, 0x100, },
	{ A6XX_DBGBUS_SPTP_4, 0x100, },
	{ A6XX_DBGBUS_SPTP_5, 0x100, },
};

static const struct adreno_debugbus_block a702_dbgc_debugbus_blocks[] = {
	{ A6XX_DBGBUS_COM_1, 0x100, },
	{ A6XX_DBGBUS_SPTP_0, 0x100, },
};

#define A6XX_NUM_SHADER_BANKS 3
#define A6XX_SHADER_STATETYPE_SHIFT 8

enum a6xx_shader_obj {
	A6XX_TP0_TMO_DATA               = 0x9,
	A6XX_TP0_SMO_DATA               = 0xa,
	A6XX_TP0_MIPMAP_BASE_DATA       = 0xb,
	A6XX_TP1_TMO_DATA               = 0x19,
	A6XX_TP1_SMO_DATA               = 0x1a,
	A6XX_TP1_MIPMAP_BASE_DATA       = 0x1b,
	A6XX_SP_INST_DATA               = 0x29,
	A6XX_SP_LB_0_DATA               = 0x2a,
	A6XX_SP_LB_1_DATA               = 0x2b,
	A6XX_SP_LB_2_DATA               = 0x2c,
	A6XX_SP_LB_3_DATA               = 0x2d,
	A6XX_SP_LB_4_DATA               = 0x2e,
	A6XX_SP_LB_5_DATA               = 0x2f,
	A6XX_SP_CB_BINDLESS_DATA        = 0x30,
	A6XX_SP_CB_LEGACY_DATA          = 0x31,
	A6XX_SP_UAV_DATA                = 0x32,
	A6XX_SP_INST_TAG                = 0x33,
	A6XX_SP_CB_BINDLESS_TAG         = 0x34,
	A6XX_SP_TMO_UMO_TAG             = 0x35,
	A6XX_SP_SMO_TAG                 = 0x36,
	A6XX_SP_STATE_DATA              = 0x37,
	A6XX_HLSQ_CHUNK_CVS_RAM         = 0x49,
	A6XX_HLSQ_CHUNK_CPS_RAM         = 0x4a,
	A6XX_HLSQ_CHUNK_CVS_RAM_TAG     = 0x4b,
	A6XX_HLSQ_CHUNK_CPS_RAM_TAG     = 0x4c,
	A6XX_HLSQ_ICB_CVS_CB_BASE_TAG   = 0x4d,
	A6XX_HLSQ_ICB_CPS_CB_BASE_TAG   = 0x4e,
	A6XX_HLSQ_CVS_MISC_RAM          = 0x50,
	A6XX_HLSQ_CPS_MISC_RAM          = 0x51,
	A6XX_HLSQ_INST_RAM              = 0x52,
	A6XX_HLSQ_GFX_CVS_CONST_RAM     = 0x53,
	A6XX_HLSQ_GFX_CPS_CONST_RAM     = 0x54,
	A6XX_HLSQ_CVS_MISC_RAM_TAG      = 0x55,
	A6XX_HLSQ_CPS_MISC_RAM_TAG      = 0x56,
	A6XX_HLSQ_INST_RAM_TAG          = 0x57,
	A6XX_HLSQ_GFX_CVS_CONST_RAM_TAG = 0x58,
	A6XX_HLSQ_GFX_CPS_CONST_RAM_TAG = 0x59,
	A6XX_HLSQ_PWR_REST_RAM          = 0x5a,
	A6XX_HLSQ_PWR_REST_TAG          = 0x5b,
	A6XX_HLSQ_DATAPATH_META         = 0x60,
	A6XX_HLSQ_FRONTEND_META         = 0x61,
	A6XX_HLSQ_INDIRECT_META         = 0x62,
	A6XX_HLSQ_BACKEND_META          = 0x63
};

struct a6xx_shader_block {
	unsigned int statetype;
	unsigned int sz;
	uint64_t offset;
};

struct a6xx_shader_block_info {
	struct a6xx_shader_block *block;
	unsigned int bank;
	uint64_t offset;
};

static struct a6xx_shader_block a6xx_shader_blocks[] = {
	{A6XX_TP0_TMO_DATA,               0x200},
	{A6XX_TP0_SMO_DATA,               0x80,},
	{A6XX_TP0_MIPMAP_BASE_DATA,       0x3C0},
	{A6XX_TP1_TMO_DATA,               0x200},
	{A6XX_TP1_SMO_DATA,               0x80,},
	{A6XX_TP1_MIPMAP_BASE_DATA,       0x3C0},
	{A6XX_SP_INST_DATA,               0x800},
	{A6XX_SP_LB_0_DATA,               0x800},
	{A6XX_SP_LB_1_DATA,               0x800},
	{A6XX_SP_LB_2_DATA,               0x800},
	{A6XX_SP_LB_3_DATA,               0x800},
	{A6XX_SP_LB_4_DATA,               0x800},
	{A6XX_SP_LB_5_DATA,               0x200},
	{A6XX_SP_CB_BINDLESS_DATA,        0x800},
	{A6XX_SP_CB_LEGACY_DATA,          0x280,},
	{A6XX_SP_UAV_DATA,                0x80,},
	{A6XX_SP_INST_TAG,                0x80,},
	{A6XX_SP_CB_BINDLESS_TAG,         0x80,},
	{A6XX_SP_TMO_UMO_TAG,             0x80,},
	{A6XX_SP_SMO_TAG,                 0x80},
	{A6XX_SP_STATE_DATA,              0x3F},
	{A6XX_HLSQ_CHUNK_CVS_RAM,         0x1C0},
	{A6XX_HLSQ_CHUNK_CPS_RAM,         0x280},
	{A6XX_HLSQ_CHUNK_CVS_RAM_TAG,     0x40,},
	{A6XX_HLSQ_CHUNK_CPS_RAM_TAG,     0x40,},
	{A6XX_HLSQ_ICB_CVS_CB_BASE_TAG,   0x4,},
	{A6XX_HLSQ_ICB_CPS_CB_BASE_TAG,   0x4,},
	{A6XX_HLSQ_CVS_MISC_RAM,          0x1C0},
	{A6XX_HLSQ_CPS_MISC_RAM,          0x580},
	{A6XX_HLSQ_INST_RAM,              0x800},
	{A6XX_HLSQ_GFX_CVS_CONST_RAM,     0x800},
	{A6XX_HLSQ_GFX_CPS_CONST_RAM,     0x800},
	{A6XX_HLSQ_CVS_MISC_RAM_TAG,      0x8,},
	{A6XX_HLSQ_CPS_MISC_RAM_TAG,      0x4,},
	{A6XX_HLSQ_INST_RAM_TAG,          0x80,},
	{A6XX_HLSQ_GFX_CVS_CONST_RAM_TAG, 0xC,},
	{A6XX_HLSQ_GFX_CPS_CONST_RAM_TAG, 0x10},
	{A6XX_HLSQ_PWR_REST_RAM,          0x28},
	{A6XX_HLSQ_PWR_REST_TAG,          0x14},
	{A6XX_HLSQ_DATAPATH_META,         0x40,},
	{A6XX_HLSQ_FRONTEND_META,         0x40},
	{A6XX_HLSQ_INDIRECT_META,         0x40,}
};

static struct kgsl_memdesc a6xx_capturescript;
static struct kgsl_memdesc a6xx_crashdump_registers;
static bool crash_dump_valid;

static struct reg_list {
	const unsigned int *regs;
	unsigned int count;
	const struct sel_reg *sel;
	uint64_t offset;
} a6xx_reg_list[] = {
	{ a6xx_registers, ARRAY_SIZE(a6xx_registers) / 2, NULL },
	{ a6xx_rb_rac_registers, ARRAY_SIZE(a6xx_rb_rac_registers) / 2,
		&_a6xx_rb_rac_aperture },
	{ a6xx_rb_rbp_registers, ARRAY_SIZE(a6xx_rb_rbp_registers) / 2,
		&_a6xx_rb_rbp_aperture },
};

#define REG_PAIR_COUNT(_a, _i) \
	(((_a)[(2 * (_i)) + 1] - (_a)[2 * (_i)]) + 1)

static size_t a6xx_legacy_snapshot_registers(struct kgsl_device *device,
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

static size_t a6xx_snapshot_registers(struct kgsl_device *device, u8 *buf,
		size_t remain, void *priv)
{
	struct kgsl_snapshot_regs *header = (struct kgsl_snapshot_regs *)buf;
	struct reg_list *regs = (struct reg_list *)priv;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	unsigned int *src;
	unsigned int j, k;
	unsigned int count = 0;

	if (!crash_dump_valid)
		return a6xx_legacy_snapshot_registers(device, buf, remain,
			regs);

	if (remain < sizeof(*header)) {
		SNAPSHOT_ERR_NOMEM(device, "REGISTERS");
		return 0;
	}

	src = (unsigned int *)(a6xx_crashdump_registers.hostptr + regs->offset);
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

static size_t a6xx_snapshot_pre_crashdump_regs(struct kgsl_device *device,
		u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_registers pre_cdregs = {
			.regs = a6xx_pre_crashdumper_registers,
			.count = ARRAY_SIZE(a6xx_pre_crashdumper_registers)/2,
	};

	return kgsl_snapshot_dump_registers(device, buf, remain, &pre_cdregs);
}

static size_t a6xx_legacy_snapshot_shader(struct kgsl_device *device,
				u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_shader *header =
		(struct kgsl_snapshot_shader *) buf;
	struct a6xx_shader_block_info *info =
		(struct a6xx_shader_block_info *) priv;
	struct a6xx_shader_block *block = info->block;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	unsigned int read_sel, val;
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

	read_sel = (block->statetype << A6XX_SHADER_STATETYPE_SHIFT) |
		info->bank;
	kgsl_regwrite(device, A6XX_HLSQ_DBG_READ_SEL, read_sel);

	/*
	 * An explicit barrier is needed so that reads do not happen before
	 * the register write.
	 */
	mb();

	for (i = 0; i < block->sz; i++) {
		kgsl_regread(device, (A6XX_HLSQ_DBG_AHB_READ_APERTURE + i),
			&val);
		*data++ = val;
	}

	return SHADER_SECTION_SZ(block->sz);
}

static size_t a6xx_snapshot_shader_memory(struct kgsl_device *device,
		u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_shader *header =
		(struct kgsl_snapshot_shader *) buf;
	struct a6xx_shader_block_info *info =
		(struct a6xx_shader_block_info *) priv;
	struct a6xx_shader_block *block = info->block;
	unsigned int *data = (unsigned int *) (buf + sizeof(*header));

	if (!crash_dump_valid)
		return a6xx_legacy_snapshot_shader(device, buf, remain, priv);

	if (remain < SHADER_SECTION_SZ(block->sz)) {
		SNAPSHOT_ERR_NOMEM(device, "SHADER MEMORY");
		return 0;
	}

	header->type = block->statetype;
	header->index = info->bank;
	header->size = block->sz;

	memcpy(data, a6xx_crashdump_registers.hostptr + info->offset,
		block->sz * sizeof(unsigned int));

	return SHADER_SECTION_SZ(block->sz);
}

static void a6xx_snapshot_shader(struct kgsl_device *device,
				struct kgsl_snapshot *snapshot)
{
	unsigned int i, j;
	struct a6xx_shader_block_info info;

	for (i = 0; i < ARRAY_SIZE(a6xx_shader_blocks); i++) {
		for (j = 0; j < A6XX_NUM_SHADER_BANKS; j++) {
			info.block = &a6xx_shader_blocks[i];
			info.bank = j;
			info.offset = a6xx_shader_blocks[i].offset +
				(j * a6xx_shader_blocks[i].sz);

			/* Shader working/shadow memory */
			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_SHADER,
				snapshot, a6xx_snapshot_shader_memory, &info);
		}
	}
}

static void a6xx_snapshot_mempool(struct kgsl_device *device,
				struct kgsl_snapshot *snapshot)
{
	unsigned int pool_size;
	u8 *buf = snapshot->ptr;

	/* Set the mempool size to 0 to stabilize it while dumping */
	kgsl_regread(device, A6XX_CP_MEM_POOL_SIZE, &pool_size);
	kgsl_regwrite(device, A6XX_CP_MEM_POOL_SIZE, 0);

	kgsl_snapshot_indexed_registers(device, snapshot,
		A6XX_CP_MEM_POOL_DBG_ADDR, A6XX_CP_MEM_POOL_DBG_DATA,
		0, 0x2060);

	/*
	 * Data at offset 0x2000 in the mempool section is the mempool size.
	 * Since we set it to 0, patch in the original size so that the data
	 * is consistent.
	 */
	if (buf < snapshot->ptr) {
		unsigned int *data;

		/* Skip over the headers */
		buf += sizeof(struct kgsl_snapshot_section_header) +
				sizeof(struct kgsl_snapshot_indexed_regs);

		data = (unsigned int *)buf + 0x2000;
		*data = pool_size;
	}

	/* Restore the saved mempool size */
	kgsl_regwrite(device, A6XX_CP_MEM_POOL_SIZE, pool_size);
}

static inline unsigned int a6xx_read_dbgahb(struct kgsl_device *device,
				unsigned int regbase, unsigned int reg)
{
	unsigned int read_reg = A6XX_HLSQ_DBG_AHB_READ_APERTURE +
				reg - regbase / 4;
	unsigned int val;

	kgsl_regread(device, read_reg, &val);
	return val;
}

static size_t a6xx_legacy_snapshot_cluster_dbgahb(struct kgsl_device *device,
				u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_mvc_regs *header =
				(struct kgsl_snapshot_mvc_regs *)buf;
	struct a6xx_cluster_dbgahb_regs_info *info =
				(struct a6xx_cluster_dbgahb_regs_info *)priv;
	struct a6xx_cluster_dbgahb_registers *cur_cluster = info->cluster;
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
	kgsl_regwrite(device, A6XX_HLSQ_DBG_READ_SEL, read_sel);

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

			val = a6xx_read_dbgahb(device, cur_cluster->regbase, j);
			*data++ = val;

		}
	}

out:
	return data_size + sizeof(*header);
}

static size_t a6xx_snapshot_cluster_dbgahb(struct kgsl_device *device, u8 *buf,
				size_t remain, void *priv)
{
	struct kgsl_snapshot_mvc_regs *header =
				(struct kgsl_snapshot_mvc_regs *)buf;
	struct a6xx_cluster_dbgahb_regs_info *info =
				(struct a6xx_cluster_dbgahb_regs_info *)priv;
	struct a6xx_cluster_dbgahb_registers *cluster = info->cluster;
	unsigned int data_size = 0;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	int i, j;
	unsigned int *src;


	if (!crash_dump_valid)
		return a6xx_legacy_snapshot_cluster_dbgahb(device, buf, remain,
				info);

	if (remain < sizeof(*header)) {
		SNAPSHOT_ERR_NOMEM(device, "REGISTERS");
		return 0;
	}

	remain -= sizeof(*header);

	header->ctxt_id = info->ctxt_id;
	header->cluster_id = cluster->id;

	src = (unsigned int *)(a6xx_crashdump_registers.hostptr +
		(header->ctxt_id ? cluster->offset1 : cluster->offset0));

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

static size_t a6xx_legacy_snapshot_non_ctx_dbgahb(struct kgsl_device *device,
				u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_regs *header =
				(struct kgsl_snapshot_regs *)buf;
	struct a6xx_non_ctx_dbgahb_registers *regs =
				(struct a6xx_non_ctx_dbgahb_registers *)priv;
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
	kgsl_regwrite(device, A6XX_HLSQ_DBG_READ_SEL, read_sel);

	for (i = 0; i < regs->num_sets; i++) {
		unsigned int start = regs->regs[2 * i];
		unsigned int end = regs->regs[2 * i + 1];

		for (j = start; j <= end; j++) {
			unsigned int val;

			val = a6xx_read_dbgahb(device, regs->regbase, j);
			*data++ = j;
			*data++ = val;

		}
	}
	return (count * 8) + sizeof(*header);
}

static size_t a6xx_snapshot_non_ctx_dbgahb(struct kgsl_device *device, u8 *buf,
				size_t remain, void *priv)
{
	struct kgsl_snapshot_regs *header =
				(struct kgsl_snapshot_regs *)buf;
	struct a6xx_non_ctx_dbgahb_registers *regs =
				(struct a6xx_non_ctx_dbgahb_registers *)priv;
	unsigned int count = 0;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	unsigned int i, k;
	unsigned int *src;

	if (!crash_dump_valid)
		return a6xx_legacy_snapshot_non_ctx_dbgahb(device, buf, remain,
				regs);

	if (remain < sizeof(*header)) {
		SNAPSHOT_ERR_NOMEM(device, "REGISTERS");
		return 0;
	}

	remain -= sizeof(*header);

	src = (unsigned int *)(a6xx_crashdump_registers.hostptr + regs->offset);

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

static void a6xx_snapshot_dbgahb_regs(struct kgsl_device *device,
				struct kgsl_snapshot *snapshot)
{
	int i, j;

	for (i = 0; i < ARRAY_SIZE(a6xx_dbgahb_ctx_clusters); i++) {
		struct a6xx_cluster_dbgahb_registers *cluster =
				&a6xx_dbgahb_ctx_clusters[i];
		struct a6xx_cluster_dbgahb_regs_info info;

		info.cluster = cluster;
		for (j = 0; j < A6XX_NUM_CTXTS; j++) {
			info.ctxt_id = j;

			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_MVC, snapshot,
				a6xx_snapshot_cluster_dbgahb, &info);
		}
	}

	for (i = 0; i < ARRAY_SIZE(a6xx_non_ctx_dbgahb); i++) {
		kgsl_snapshot_add_section(device,
			KGSL_SNAPSHOT_SECTION_REGS, snapshot,
			a6xx_snapshot_non_ctx_dbgahb, &a6xx_non_ctx_dbgahb[i]);
	}
}

static size_t a6xx_legacy_snapshot_mvc(struct kgsl_device *device, u8 *buf,
				size_t remain, void *priv)
{
	struct kgsl_snapshot_mvc_regs *header =
					(struct kgsl_snapshot_mvc_regs *)buf;
	struct a6xx_cluster_regs_info *info =
					(struct a6xx_cluster_regs_info *)priv;
	struct a6xx_cluster_registers *cur_cluster = info->cluster;
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
	kgsl_regwrite(device, A6XX_CP_APERTURE_CNTL_HOST, aperture_cntl);

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

static size_t a6xx_snapshot_mvc(struct kgsl_device *device, u8 *buf,
				size_t remain, void *priv)
{
	struct kgsl_snapshot_mvc_regs *header =
				(struct kgsl_snapshot_mvc_regs *)buf;
	struct a6xx_cluster_regs_info *info =
				(struct a6xx_cluster_regs_info *)priv;
	struct a6xx_cluster_registers *cluster = info->cluster;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	unsigned int *src;
	int i, j;
	unsigned int start, end;
	size_t data_size = 0;

	if (!crash_dump_valid)
		return a6xx_legacy_snapshot_mvc(device, buf, remain, info);

	if (remain < sizeof(*header)) {
		SNAPSHOT_ERR_NOMEM(device, "MVC REGISTERS");
		return 0;
	}

	remain -= sizeof(*header);

	header->ctxt_id = info->ctxt_id;
	header->cluster_id = cluster->id;

	src = (unsigned int *)(a6xx_crashdump_registers.hostptr +
		(header->ctxt_id ? cluster->offset1 : cluster->offset0));

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

static void a6xx_snapshot_mvc_regs(struct kgsl_device *device,
				struct kgsl_snapshot *snapshot)
{
	int i, j;
	struct a6xx_cluster_regs_info info;

	for (i = 0; i < ARRAY_SIZE(a6xx_clusters); i++) {
		struct a6xx_cluster_registers *cluster = &a6xx_clusters[i];

		info.cluster = cluster;
		for (j = 0; j < A6XX_NUM_CTXTS; j++) {
			info.ctxt_id = j;

			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_MVC, snapshot,
				a6xx_snapshot_mvc, &info);
		}
	}
}

/* a6xx_dbgc_debug_bus_read() - Read data from trace bus */
static void a6xx_dbgc_debug_bus_read(struct kgsl_device *device,
	unsigned int block_id, unsigned int index, unsigned int *val)
{
	unsigned int reg;

	reg = (block_id << A6XX_DBGC_CFG_DBGBUS_SEL_PING_BLK_SEL_SHIFT) |
			(index << A6XX_DBGC_CFG_DBGBUS_SEL_PING_INDEX_SHIFT);

	kgsl_regwrite(device, A6XX_DBGC_CFG_DBGBUS_SEL_A, reg);
	kgsl_regwrite(device, A6XX_DBGC_CFG_DBGBUS_SEL_B, reg);
	kgsl_regwrite(device, A6XX_DBGC_CFG_DBGBUS_SEL_C, reg);
	kgsl_regwrite(device, A6XX_DBGC_CFG_DBGBUS_SEL_D, reg);

	/*
	 * There needs to be a delay of 1 us to ensure enough time for correct
	 * data is funneled into the trace buffer
	 */
	udelay(1);

	kgsl_regread(device, A6XX_DBGC_CFG_DBGBUS_TRACE_BUF2, val);
	val++;
	kgsl_regread(device, A6XX_DBGC_CFG_DBGBUS_TRACE_BUF1, val);
}

/* a6xx_snapshot_dbgc_debugbus_block() - Capture debug data for a gpu block */
static size_t a6xx_snapshot_dbgc_debugbus_block(struct kgsl_device *device,
	u8 *buf, size_t remain, void *priv)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_snapshot_debugbus *header =
		(struct kgsl_snapshot_debugbus *)buf;
	struct adreno_debugbus_block *block = priv;
	int i;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	unsigned int dwords;
	unsigned int block_id;
	size_t size;

	dwords = block->dwords;

	/* For a6xx each debug bus data unit is 2 DWORDS */
	size = (dwords * sizeof(unsigned int) * 2) + sizeof(*header);

	if (remain < size) {
		SNAPSHOT_ERR_NOMEM(device, "DEBUGBUS");
		return 0;
	}

	header->id = block->block_id;
	if ((block->block_id == A6XX_DBGBUS_VBIF) &&
		adreno_has_gbif(adreno_dev))
		header->id = A6XX_DBGBUS_GBIF_GX;
	header->count = dwords * 2;

	block_id = block->block_id;
	/* GMU_GX data is read using the GMU_CX block id on A630 */
	if ((adreno_is_a630(adreno_dev) || adreno_is_a615_family(adreno_dev)) &&
		(block_id == A6XX_DBGBUS_GMU_GX))
		block_id = A6XX_DBGBUS_GMU_CX;

	for (i = 0; i < dwords; i++)
		a6xx_dbgc_debug_bus_read(device, block_id, i, &data[i*2]);

	return size;
}

/* a6xx_snapshot_vbif_debugbus_block() - Capture debug data for VBIF block */
static size_t a6xx_snapshot_vbif_debugbus_block(struct kgsl_device *device,
			u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_debugbus *header =
		(struct kgsl_snapshot_debugbus *)buf;
	struct adreno_debugbus_block *block = priv;
	int i, j;
	/*
	 * Total number of VBIF data words considering 3 sections:
	 * 2 arbiter blocks of 16 words
	 * 5 AXI XIN blocks of 18 dwords each
	 * 4 core clock side XIN blocks of 12 dwords each
	 */
	unsigned int dwords = (16 * A6XX_NUM_AXI_ARB_BLOCKS) +
			(18 * A6XX_NUM_XIN_AXI_BLOCKS) +
			(12 * A6XX_NUM_XIN_CORE_BLOCKS);
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	size_t size;
	unsigned int reg_clk;

	size = (dwords * sizeof(unsigned int)) + sizeof(*header);

	if (remain < size) {
		SNAPSHOT_ERR_NOMEM(device, "DEBUGBUS");
		return 0;
	}
	header->id = block->block_id;
	header->count = dwords;

	kgsl_regread(device, A6XX_VBIF_CLKON, &reg_clk);
	kgsl_regwrite(device, A6XX_VBIF_CLKON, reg_clk |
			(A6XX_VBIF_CLKON_FORCE_ON_TESTBUS_MASK <<
			A6XX_VBIF_CLKON_FORCE_ON_TESTBUS_SHIFT));
	kgsl_regwrite(device, A6XX_VBIF_TEST_BUS1_CTRL0, 0);
	kgsl_regwrite(device, A6XX_VBIF_TEST_BUS_OUT_CTRL,
			(A6XX_VBIF_TEST_BUS_OUT_CTRL_EN_MASK <<
			A6XX_VBIF_TEST_BUS_OUT_CTRL_EN_SHIFT));

	for (i = 0; i < A6XX_NUM_AXI_ARB_BLOCKS; i++) {
		kgsl_regwrite(device, A6XX_VBIF_TEST_BUS2_CTRL0,
			(1 << (i + 16)));
		for (j = 0; j < 16; j++) {
			kgsl_regwrite(device, A6XX_VBIF_TEST_BUS2_CTRL1,
				((j & A6XX_VBIF_TEST_BUS2_CTRL1_DATA_SEL_MASK)
				<< A6XX_VBIF_TEST_BUS2_CTRL1_DATA_SEL_SHIFT));
			kgsl_regread(device, A6XX_VBIF_TEST_BUS_OUT,
					data);
			data++;
		}
	}

	/* XIN blocks AXI side */
	for (i = 0; i < A6XX_NUM_XIN_AXI_BLOCKS; i++) {
		kgsl_regwrite(device, A6XX_VBIF_TEST_BUS2_CTRL0, 1 << i);
		for (j = 0; j < 18; j++) {
			kgsl_regwrite(device, A6XX_VBIF_TEST_BUS2_CTRL1,
				((j & A6XX_VBIF_TEST_BUS2_CTRL1_DATA_SEL_MASK)
				<< A6XX_VBIF_TEST_BUS2_CTRL1_DATA_SEL_SHIFT));
			kgsl_regread(device, A6XX_VBIF_TEST_BUS_OUT,
				data);
			data++;
		}
	}
	kgsl_regwrite(device, A6XX_VBIF_TEST_BUS2_CTRL0, 0);

	/* XIN blocks core clock side */
	for (i = 0; i < A6XX_NUM_XIN_CORE_BLOCKS; i++) {
		kgsl_regwrite(device, A6XX_VBIF_TEST_BUS1_CTRL0, 1 << i);
		for (j = 0; j < 12; j++) {
			kgsl_regwrite(device, A6XX_VBIF_TEST_BUS1_CTRL1,
				((j & A6XX_VBIF_TEST_BUS1_CTRL1_DATA_SEL_MASK)
				<< A6XX_VBIF_TEST_BUS1_CTRL1_DATA_SEL_SHIFT));
			kgsl_regread(device, A6XX_VBIF_TEST_BUS_OUT,
				data);
			data++;
		}
	}
	/* restore the clock of VBIF */
	kgsl_regwrite(device, A6XX_VBIF_CLKON, reg_clk);
	return size;
}

/* a6xx_cx_dbgc_debug_bus_read() - Read data from trace bus */
static void a6xx_cx_debug_bus_read(struct kgsl_device *device,
	unsigned int block_id, unsigned int index, unsigned int *val)
{
	unsigned int reg;

	reg = (block_id << A6XX_CX_DBGC_CFG_DBGBUS_SEL_PING_BLK_SEL_SHIFT) |
			(index << A6XX_CX_DBGC_CFG_DBGBUS_SEL_PING_INDEX_SHIFT);

	adreno_cx_dbgc_regwrite(device, A6XX_CX_DBGC_CFG_DBGBUS_SEL_A, reg);
	adreno_cx_dbgc_regwrite(device, A6XX_CX_DBGC_CFG_DBGBUS_SEL_B, reg);
	adreno_cx_dbgc_regwrite(device, A6XX_CX_DBGC_CFG_DBGBUS_SEL_C, reg);
	adreno_cx_dbgc_regwrite(device, A6XX_CX_DBGC_CFG_DBGBUS_SEL_D, reg);

	/*
	 * There needs to be a delay of 1 us to ensure enough time for correct
	 * data is funneled into the trace buffer
	 */
	udelay(1);

	adreno_cx_dbgc_regread(device, A6XX_CX_DBGC_CFG_DBGBUS_TRACE_BUF2, val);
	val++;
	adreno_cx_dbgc_regread(device, A6XX_CX_DBGC_CFG_DBGBUS_TRACE_BUF1, val);
}

/*
 * a6xx_snapshot_cx_dbgc_debugbus_block() - Capture debug data for a gpu
 * block from the CX DBGC block
 */
static size_t a6xx_snapshot_cx_dbgc_debugbus_block(struct kgsl_device *device,
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

	/* For a6xx each debug bus data unit is 2 DWRODS */
	size = (dwords * sizeof(unsigned int) * 2) + sizeof(*header);

	if (remain < size) {
		SNAPSHOT_ERR_NOMEM(device, "DEBUGBUS");
		return 0;
	}

	header->id = block->block_id;
	header->count = dwords * 2;

	for (i = 0; i < dwords; i++)
		a6xx_cx_debug_bus_read(device, block->block_id, i,
					&data[i*2]);

	return size;
}

/* a6xx_snapshot_debugbus() - Capture debug bus data */
static void a6xx_snapshot_debugbus(struct adreno_device *adreno_dev,
		struct kgsl_snapshot *snapshot)
{
	int i;
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	kgsl_regwrite(device, A6XX_DBGC_CFG_DBGBUS_CNTLT,
		(0xf << A6XX_DBGC_CFG_DBGBUS_CNTLT_SEGT_SHIFT) |
		(0x0 << A6XX_DBGC_CFG_DBGBUS_CNTLT_GRANU_SHIFT) |
		(0x0 << A6XX_DBGC_CFG_DBGBUS_CNTLT_TRACEEN_SHIFT));

	kgsl_regwrite(device, A6XX_DBGC_CFG_DBGBUS_CNTLM,
		0xf << A6XX_DBGC_CFG_DBGBUS_CTLTM_ENABLE_SHIFT);

	kgsl_regwrite(device, A6XX_DBGC_CFG_DBGBUS_IVTL_0, 0);
	kgsl_regwrite(device, A6XX_DBGC_CFG_DBGBUS_IVTL_1, 0);
	kgsl_regwrite(device, A6XX_DBGC_CFG_DBGBUS_IVTL_2, 0);
	kgsl_regwrite(device, A6XX_DBGC_CFG_DBGBUS_IVTL_3, 0);

	kgsl_regwrite(device, A6XX_DBGC_CFG_DBGBUS_BYTEL_0,
		(0 << A6XX_DBGC_CFG_DBGBUS_BYTEL0_SHIFT) |
		(1 << A6XX_DBGC_CFG_DBGBUS_BYTEL1_SHIFT) |
		(2 << A6XX_DBGC_CFG_DBGBUS_BYTEL2_SHIFT) |
		(3 << A6XX_DBGC_CFG_DBGBUS_BYTEL3_SHIFT) |
		(4 << A6XX_DBGC_CFG_DBGBUS_BYTEL4_SHIFT) |
		(5 << A6XX_DBGC_CFG_DBGBUS_BYTEL5_SHIFT) |
		(6 << A6XX_DBGC_CFG_DBGBUS_BYTEL6_SHIFT) |
		(7 << A6XX_DBGC_CFG_DBGBUS_BYTEL7_SHIFT));
	kgsl_regwrite(device, A6XX_DBGC_CFG_DBGBUS_BYTEL_1,
		(8 << A6XX_DBGC_CFG_DBGBUS_BYTEL8_SHIFT) |
		(9 << A6XX_DBGC_CFG_DBGBUS_BYTEL9_SHIFT) |
		(10 << A6XX_DBGC_CFG_DBGBUS_BYTEL10_SHIFT) |
		(11 << A6XX_DBGC_CFG_DBGBUS_BYTEL11_SHIFT) |
		(12 << A6XX_DBGC_CFG_DBGBUS_BYTEL12_SHIFT) |
		(13 << A6XX_DBGC_CFG_DBGBUS_BYTEL13_SHIFT) |
		(14 << A6XX_DBGC_CFG_DBGBUS_BYTEL14_SHIFT) |
		(15 << A6XX_DBGC_CFG_DBGBUS_BYTEL15_SHIFT));

	kgsl_regwrite(device, A6XX_DBGC_CFG_DBGBUS_MASKL_0, 0);
	kgsl_regwrite(device, A6XX_DBGC_CFG_DBGBUS_MASKL_1, 0);
	kgsl_regwrite(device, A6XX_DBGC_CFG_DBGBUS_MASKL_2, 0);
	kgsl_regwrite(device, A6XX_DBGC_CFG_DBGBUS_MASKL_3, 0);

	adreno_cx_dbgc_regwrite(device, A6XX_CX_DBGC_CFG_DBGBUS_CNTLT,
		(0xf << A6XX_DBGC_CFG_DBGBUS_CNTLT_SEGT_SHIFT) |
		(0x0 << A6XX_DBGC_CFG_DBGBUS_CNTLT_GRANU_SHIFT) |
		(0x0 << A6XX_DBGC_CFG_DBGBUS_CNTLT_TRACEEN_SHIFT));

	adreno_cx_dbgc_regwrite(device, A6XX_CX_DBGC_CFG_DBGBUS_CNTLM,
		0xf << A6XX_CX_DBGC_CFG_DBGBUS_CNTLM_ENABLE_SHIFT);

	adreno_cx_dbgc_regwrite(device, A6XX_CX_DBGC_CFG_DBGBUS_IVTL_0, 0);
	adreno_cx_dbgc_regwrite(device, A6XX_CX_DBGC_CFG_DBGBUS_IVTL_1, 0);
	adreno_cx_dbgc_regwrite(device, A6XX_CX_DBGC_CFG_DBGBUS_IVTL_2, 0);
	adreno_cx_dbgc_regwrite(device, A6XX_CX_DBGC_CFG_DBGBUS_IVTL_3, 0);

	adreno_cx_dbgc_regwrite(device, A6XX_CX_DBGC_CFG_DBGBUS_BYTEL_0,
		(0 << A6XX_CX_DBGC_CFG_DBGBUS_BYTEL0_SHIFT) |
		(1 << A6XX_CX_DBGC_CFG_DBGBUS_BYTEL1_SHIFT) |
		(2 << A6XX_CX_DBGC_CFG_DBGBUS_BYTEL2_SHIFT) |
		(3 << A6XX_CX_DBGC_CFG_DBGBUS_BYTEL3_SHIFT) |
		(4 << A6XX_CX_DBGC_CFG_DBGBUS_BYTEL4_SHIFT) |
		(5 << A6XX_CX_DBGC_CFG_DBGBUS_BYTEL5_SHIFT) |
		(6 << A6XX_CX_DBGC_CFG_DBGBUS_BYTEL6_SHIFT) |
		(7 << A6XX_CX_DBGC_CFG_DBGBUS_BYTEL7_SHIFT));
	adreno_cx_dbgc_regwrite(device, A6XX_CX_DBGC_CFG_DBGBUS_BYTEL_1,
		(8 << A6XX_CX_DBGC_CFG_DBGBUS_BYTEL8_SHIFT) |
		(9 << A6XX_CX_DBGC_CFG_DBGBUS_BYTEL9_SHIFT) |
		(10 << A6XX_CX_DBGC_CFG_DBGBUS_BYTEL10_SHIFT) |
		(11 << A6XX_CX_DBGC_CFG_DBGBUS_BYTEL11_SHIFT) |
		(12 << A6XX_CX_DBGC_CFG_DBGBUS_BYTEL12_SHIFT) |
		(13 << A6XX_CX_DBGC_CFG_DBGBUS_BYTEL13_SHIFT) |
		(14 << A6XX_CX_DBGC_CFG_DBGBUS_BYTEL14_SHIFT) |
		(15 << A6XX_CX_DBGC_CFG_DBGBUS_BYTEL15_SHIFT));

	adreno_cx_dbgc_regwrite(device, A6XX_CX_DBGC_CFG_DBGBUS_MASKL_0, 0);
	adreno_cx_dbgc_regwrite(device, A6XX_CX_DBGC_CFG_DBGBUS_MASKL_1, 0);
	adreno_cx_dbgc_regwrite(device, A6XX_CX_DBGC_CFG_DBGBUS_MASKL_2, 0);
	adreno_cx_dbgc_regwrite(device, A6XX_CX_DBGC_CFG_DBGBUS_MASKL_3, 0);

	for (i = 0; i < ARRAY_SIZE(a6xx_dbgc_debugbus_blocks); i++) {
		kgsl_snapshot_add_section(device,
			KGSL_SNAPSHOT_SECTION_DEBUGBUS,
			snapshot, a6xx_snapshot_dbgc_debugbus_block,
			(void *) &a6xx_dbgc_debugbus_blocks[i]);
	}

	if (adreno_is_a650_family(adreno_dev)) {
		for (i = 0; i < ARRAY_SIZE(a650_dbgc_debugbus_blocks); i++) {
			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_DEBUGBUS,
				snapshot, a6xx_snapshot_dbgc_debugbus_block,
				(void *) &a650_dbgc_debugbus_blocks[i]);
		}
	}

	if (adreno_is_a702(adreno_dev)) {
		for (i = 0; i < ARRAY_SIZE(a702_dbgc_debugbus_blocks); i++) {
			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_DEBUGBUS,
				snapshot, a6xx_snapshot_dbgc_debugbus_block,
				(void *) &a702_dbgc_debugbus_blocks[i]);
		}
	}

	/*
	 * GBIF has same debugbus as of other GPU blocks hence fall back to
	 * default path if GPU uses GBIF.
	 * GBIF uses exactly same ID as of VBIF so use it as it is.
	 */
	if (adreno_has_gbif(adreno_dev))
		kgsl_snapshot_add_section(device,
			KGSL_SNAPSHOT_SECTION_DEBUGBUS,
			snapshot, a6xx_snapshot_dbgc_debugbus_block,
			(void *) &a6xx_vbif_debugbus_blocks);
	else
		kgsl_snapshot_add_section(device,
			KGSL_SNAPSHOT_SECTION_DEBUGBUS,
			snapshot, a6xx_snapshot_vbif_debugbus_block,
			(void *) &a6xx_vbif_debugbus_blocks);

	/* Dump the CX debugbus data if the block exists */
	if (adreno_is_cx_dbgc_register(device, A6XX_CX_DBGC_CFG_DBGBUS_SEL_A)) {
		for (i = 0; i < ARRAY_SIZE(a6xx_cx_dbgc_debugbus_blocks); i++) {
			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_DEBUGBUS,
				snapshot, a6xx_snapshot_cx_dbgc_debugbus_block,
				(void *) &a6xx_cx_dbgc_debugbus_blocks[i]);
		}
		/*
		 * Get debugbus for GBIF CX part if GPU has GBIF block
		 * GBIF uses exactly same ID as of VBIF so use
		 * it as it is.
		 */
		if (adreno_has_gbif(adreno_dev))
			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_DEBUGBUS,
				snapshot,
				a6xx_snapshot_cx_dbgc_debugbus_block,
				(void *) &a6xx_vbif_debugbus_blocks);
	}
}



/* a6xx_snapshot_sqe() - Dump SQE data in snapshot */
static size_t a6xx_snapshot_sqe(struct kgsl_device *device, u8 *buf,
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

static void _a6xx_do_crashdump(struct kgsl_device *device)
{
	unsigned long wait_time;
	unsigned int reg = 0;
	unsigned int val;

	crash_dump_valid = false;

	if (!device->snapshot_crashdumper)
		return;
	if (a6xx_capturescript.gpuaddr == 0 ||
		a6xx_crashdump_registers.gpuaddr == 0)
		return;
	if (!test_bit(KGSL_MMU_STARTED, &device->mmu.flags))
		return;

	/* IF the SMMU is stalled we cannot do a crash dump */
	kgsl_regread(device, A6XX_RBBM_STATUS3, &val);
	if (val & BIT(24))
		return;

	/* Turn on APRIV for legacy targets so we can access the buffers */
	if (!ADRENO_FEATURE(ADRENO_DEVICE(device), ADRENO_APRIV))
		kgsl_regwrite(device, A6XX_CP_MISC_CNTL, 1);

	kgsl_regwrite(device, A6XX_CP_CRASH_SCRIPT_BASE_LO,
			lower_32_bits(a6xx_capturescript.gpuaddr));
	kgsl_regwrite(device, A6XX_CP_CRASH_SCRIPT_BASE_HI,
			upper_32_bits(a6xx_capturescript.gpuaddr));
	kgsl_regwrite(device, A6XX_CP_CRASH_DUMP_CNTL, 1);

	wait_time = jiffies + msecs_to_jiffies(CP_CRASH_DUMPER_TIMEOUT);
	while (!time_after(jiffies, wait_time)) {
		kgsl_regread(device, A6XX_CP_CRASH_DUMP_STATUS, &reg);
		if (reg & 0x2)
			break;
		cpu_relax();
	}

	if (!ADRENO_FEATURE(ADRENO_DEVICE(device), ADRENO_APRIV))
		kgsl_regwrite(device, A6XX_CP_MISC_CNTL, 0);

	if (!(reg & 0x2)) {
		dev_err(device->dev, "Crash dump timed out: 0x%X\n", reg);
		return;
	}

	crash_dump_valid = true;
}

size_t a6xx_snapshot_rscc_registers(struct kgsl_device *device, u8 *buf,
	size_t remain, void *priv)
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
		SNAPSHOT_ERR_NOMEM(device, "RSCC REGISTERS");
		return 0;
	}

	for (j = 0; j < regs->count; j++) {
		unsigned int start = regs->regs[j * 2];
		unsigned int end = regs->regs[j * 2 + 1];

		for (k = start; k <= end; k++) {
			unsigned int val;

			adreno_rscc_regread(adreno_dev,
				k - (adreno_dev->rscc_base >> 2), &val);
			*data++ = k;
			*data++ = val;
		}
	}

	header->count = count;

	/* Return the size of the section */
	return (count * 8) + sizeof(*header);
}

size_t a6xx_snapshot_isense_registers(struct kgsl_device *device, u8 *buf,
	size_t remain, void *priv)
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

	if (remain < (A6XX_SNAPSHOT_CP_CTXRECORD_SIZE_IN_BYTES +
						sizeof(*header))) {
		SNAPSHOT_ERR_NOMEM(device, "PREEMPTION RECORD");
		return 0;
	}

	header->size = A6XX_SNAPSHOT_CP_CTXRECORD_SIZE_IN_BYTES >> 2;
	header->gpuaddr = memdesc->gpuaddr;
	header->ptbase =
		kgsl_mmu_pagetable_get_ttbr0(device->mmu.defaultpagetable);
	header->type = SNAPSHOT_GPU_OBJECT_GLOBAL;

	memcpy(ptr, memdesc->hostptr, A6XX_SNAPSHOT_CP_CTXRECORD_SIZE_IN_BYTES);

	return A6XX_SNAPSHOT_CP_CTXRECORD_SIZE_IN_BYTES + sizeof(*header);
}

/*
 * a6xx_snapshot() - A6XX GPU snapshot function
 * @adreno_dev: Device being snapshotted
 * @snapshot: Pointer to the snapshot instance
 *
 * This is where all of the A6XX specific bits and pieces are grabbed
 * into the snapshot memory
 */
void a6xx_snapshot(struct adreno_device *adreno_dev,
		struct kgsl_snapshot *snapshot)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	struct adreno_ringbuffer *rb;
	bool sptprac_on;
	unsigned int i, roq_size, ucode_dbg_size;

	/* GMU TCM data dumped through AHB */
	gmu_core_dev_snapshot(device, snapshot);

	/*
	 * Dump debugbus data here to capture it for both
	 * GMU and GPU snapshot. Debugbus data can be accessed
	 * even if the gx headswitch or sptprac is off. If gx
	 * headswitch is off, data for gx blocks will show as
	 * 0x5c00bd00.
	 */
	a6xx_snapshot_debugbus(adreno_dev, snapshot);

	/* RSCC registers are on cx */
	if (adreno_is_a650_family(adreno_dev)) {
		struct kgsl_snapshot_registers r;

		r.regs = a650_rscc_registers;
		r.count = ARRAY_SIZE(a650_rscc_registers) / 2;

		kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_REGS,
			snapshot, a6xx_snapshot_rscc_registers, &r);

		r.regs = a650_isense_registers;
		r.count = ARRAY_SIZE(a650_isense_registers) / 2;

		kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_REGS,
			snapshot, a6xx_snapshot_isense_registers, &r);
	} else if (adreno_is_a615_family(adreno_dev) ||
			adreno_is_a630(adreno_dev)) {
		adreno_snapshot_registers(device, snapshot,
			a630_rscc_snapshot_registers,
			ARRAY_SIZE(a630_rscc_snapshot_registers) / 2);
	} else if (adreno_is_a640(adreno_dev) || adreno_is_a680(adreno_dev)) {
		adreno_snapshot_registers(device, snapshot,
			a6xx_rscc_snapshot_registers,
			ARRAY_SIZE(a6xx_rscc_snapshot_registers) / 2);
	}

	if (!gmu_core_isenabled(device))
		adreno_snapshot_registers(device, snapshot,
			a6xx_gmu_wrapper_registers,
			ARRAY_SIZE(a6xx_gmu_wrapper_registers) / 2);

	sptprac_on = gpudev->sptprac_is_on(adreno_dev);

	if (!gmu_core_dev_gx_is_on(device))
		return;

	/* Dump the registers which get affected by crash dumper trigger */
	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_REGS,
		snapshot, a6xx_snapshot_pre_crashdump_regs, NULL);

	/* Dump vbif registers as well which get affected by crash dumper */
	if (!adreno_has_gbif(adreno_dev))
		adreno_snapshot_vbif_registers(device, snapshot,
			a6xx_vbif_snapshot_registers,
			ARRAY_SIZE(a6xx_vbif_snapshot_registers));
	else
		adreno_snapshot_registers(device, snapshot,
			a6xx_gbif_registers,
			ARRAY_SIZE(a6xx_gbif_registers) / 2);

	/* Try to run the crash dumper */
	if (sptprac_on)
		_a6xx_do_crashdump(device);

	for (i = 0; i < ARRAY_SIZE(a6xx_reg_list); i++) {
		kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_REGS,
			snapshot, a6xx_snapshot_registers, &a6xx_reg_list[i]);
	}

	/* CP_SQE indexed registers */
	kgsl_snapshot_indexed_registers(device, snapshot,
		A6XX_CP_SQE_STAT_ADDR, A6XX_CP_SQE_STAT_DATA, 0, 0x33);

	/* CP_DRAW_STATE */
	kgsl_snapshot_indexed_registers(device, snapshot,
		A6XX_CP_DRAW_STATE_ADDR, A6XX_CP_DRAW_STATE_DATA,
		0, 0x100);

	ucode_dbg_size = adreno_is_a650_family(adreno_dev)
			? 0x7000 : 0x6000;

	 /* SQE_UCODE Cache */
	kgsl_snapshot_indexed_registers(device, snapshot,
		A6XX_CP_SQE_UCODE_DBG_ADDR, A6XX_CP_SQE_UCODE_DBG_DATA,
		0, ucode_dbg_size);

	/*
	 * CP ROQ dump units is 4dwords. The number of units is stored
	 * in CP_ROQ_THRESHOLDS_2[31:16]. Read the value and convert to
	 * dword units.
	 */
	kgsl_regread(device, A6XX_CP_ROQ_THRESHOLDS_2, &roq_size);
	roq_size = roq_size >> 14;
	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_DEBUG,
		snapshot, adreno_snapshot_cp_roq, &roq_size);

	/* SQE Firmware */
	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_DEBUG,
		snapshot, a6xx_snapshot_sqe, NULL);

	/* Mempool debug data */
	a6xx_snapshot_mempool(device, snapshot);

	if (sptprac_on) {
		/* Shader memory */
		a6xx_snapshot_shader(device, snapshot);

		/* MVC register section */
		a6xx_snapshot_mvc_regs(device, snapshot);

		/* registers dumped through DBG AHB */
		a6xx_snapshot_dbgahb_regs(device, snapshot);
	}

	/* Preemption record */
	if (adreno_is_preemption_enabled(adreno_dev)) {
		FOR_EACH_RINGBUFFER(adreno_dev, rb, i) {
			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_GPU_OBJECT_V2,
				snapshot, snapshot_preemption_record,
				&rb->preemption_desc);
		}
	}
}

static int _a6xx_crashdump_init_mvc(struct adreno_device *adreno_dev,
	uint64_t *ptr, uint64_t *offset)
{
	int qwords = 0;
	unsigned int i, j, k;
	unsigned int count;

	for (i = 0; i < ARRAY_SIZE(a6xx_clusters); i++) {
		struct a6xx_cluster_registers *cluster = &a6xx_clusters[i];

		/* The VPC registers are driven by VPC_PS cluster on a650 */
		if (adreno_is_a650_family(adreno_dev) &&
				(cluster->regs == a6xx_vpc_ps_cluster))
			cluster->id = CP_CLUSTER_VPC_PS;

		if (cluster->sel) {
			ptr[qwords++] = cluster->sel->val;
			ptr[qwords++] = ((uint64_t)cluster->sel->cd_reg << 44) |
				(1 << 21) | 1;
		}

		cluster->offset0 = *offset;
		for (j = 0; j < A6XX_NUM_CTXTS; j++) {

			if (j == 1)
				cluster->offset1 = *offset;

			ptr[qwords++] = (cluster->id << 8) | (j << 4) | j;
			ptr[qwords++] =
				((uint64_t)A6XX_CP_APERTURE_CNTL_CD << 44) |
				(1 << 21) | 1;

			for (k = 0; k < cluster->num_sets; k++) {
				count = REG_PAIR_COUNT(cluster->regs, k);
				ptr[qwords++] =
				a6xx_crashdump_registers.gpuaddr + *offset;
				ptr[qwords++] =
				(((uint64_t)cluster->regs[2 * k]) << 44) |
						count;

				*offset += count * sizeof(unsigned int);
			}
		}
	}

	return qwords;
}

static int _a6xx_crashdump_init_shader(struct a6xx_shader_block *block,
		uint64_t *ptr, uint64_t *offset)
{
	int qwords = 0;
	unsigned int j;

	/* Capture each bank in the block */
	for (j = 0; j < A6XX_NUM_SHADER_BANKS; j++) {
		/* Program the aperture */
		ptr[qwords++] =
			(block->statetype << A6XX_SHADER_STATETYPE_SHIFT) | j;
		ptr[qwords++] = (((uint64_t) A6XX_HLSQ_DBG_READ_SEL << 44)) |
			(1 << 21) | 1;

		/* Read all the data in one chunk */
		ptr[qwords++] = a6xx_crashdump_registers.gpuaddr + *offset;
		ptr[qwords++] =
			(((uint64_t) A6XX_HLSQ_DBG_AHB_READ_APERTURE << 44)) |
			block->sz;

		/* Remember the offset of the first bank for easy access */
		if (j == 0)
			block->offset = *offset;

		*offset += block->sz * sizeof(unsigned int);
	}

	return qwords;
}

static int _a6xx_crashdump_init_ctx_dbgahb(uint64_t *ptr, uint64_t *offset)
{
	int qwords = 0;
	unsigned int i, j, k;
	unsigned int count;

	for (i = 0; i < ARRAY_SIZE(a6xx_dbgahb_ctx_clusters); i++) {
		struct a6xx_cluster_dbgahb_registers *cluster =
				&a6xx_dbgahb_ctx_clusters[i];

		cluster->offset0 = *offset;

		for (j = 0; j < A6XX_NUM_CTXTS; j++) {
			if (j == 1)
				cluster->offset1 = *offset;

			/* Program the aperture */
			ptr[qwords++] =
				((cluster->statetype + j * 2) & 0xff) << 8;
			ptr[qwords++] =
				(((uint64_t)A6XX_HLSQ_DBG_READ_SEL << 44)) |
					(1 << 21) | 1;

			for (k = 0; k < cluster->num_sets; k++) {
				unsigned int start = cluster->regs[2 * k];

				count = REG_PAIR_COUNT(cluster->regs, k);
				ptr[qwords++] =
				a6xx_crashdump_registers.gpuaddr + *offset;
				ptr[qwords++] =
				(((uint64_t)(A6XX_HLSQ_DBG_AHB_READ_APERTURE +
					start - cluster->regbase / 4) << 44)) |
							count;

				*offset += count * sizeof(unsigned int);
			}
		}
	}
	return qwords;
}

static int _a6xx_crashdump_init_non_ctx_dbgahb(uint64_t *ptr, uint64_t *offset)
{
	int qwords = 0;
	unsigned int i, k;
	unsigned int count;

	for (i = 0; i < ARRAY_SIZE(a6xx_non_ctx_dbgahb); i++) {
		struct a6xx_non_ctx_dbgahb_registers *regs =
				&a6xx_non_ctx_dbgahb[i];

		regs->offset = *offset;

		/* Program the aperture */
		ptr[qwords++] = (regs->statetype & 0xff) << 8;
		ptr[qwords++] =	(((uint64_t)A6XX_HLSQ_DBG_READ_SEL << 44)) |
					(1 << 21) | 1;

		for (k = 0; k < regs->num_sets; k++) {
			unsigned int start = regs->regs[2 * k];

			count = REG_PAIR_COUNT(regs->regs, k);
			ptr[qwords++] =
				a6xx_crashdump_registers.gpuaddr + *offset;
			ptr[qwords++] =
				(((uint64_t)(A6XX_HLSQ_DBG_AHB_READ_APERTURE +
					start - regs->regbase / 4) << 44)) |
							count;

			*offset += count * sizeof(unsigned int);
		}
	}
	return qwords;
}

void a6xx_crashdump_init(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int script_size = 0;
	unsigned int data_size = 0;
	unsigned int i, j, k;
	uint64_t *ptr;
	uint64_t offset = 0;

	if (a6xx_capturescript.gpuaddr != 0 &&
		a6xx_crashdump_registers.gpuaddr != 0)
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
	for (i = 0; i < ARRAY_SIZE(a6xx_reg_list); i++) {
		struct reg_list *regs = &a6xx_reg_list[i];

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
	for (i = 0; i < ARRAY_SIZE(a6xx_shader_blocks); i++) {
		script_size += 32 * A6XX_NUM_SHADER_BANKS;
		data_size += a6xx_shader_blocks[i].sz * sizeof(unsigned int) *
			A6XX_NUM_SHADER_BANKS;
	}

	/* Calculate the script and data size for MVC registers */
	for (i = 0; i < ARRAY_SIZE(a6xx_clusters); i++) {
		struct a6xx_cluster_registers *cluster = &a6xx_clusters[i];

		/* 16 bytes if cluster sel exists */
		if (cluster->sel)
			script_size += 16;

		for (j = 0; j < A6XX_NUM_CTXTS; j++) {

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
	for (i = 0; i < ARRAY_SIZE(a6xx_dbgahb_ctx_clusters); i++) {
		struct a6xx_cluster_dbgahb_registers *cluster =
				&a6xx_dbgahb_ctx_clusters[i];

		for (j = 0; j < A6XX_NUM_CTXTS; j++) {

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
	for (i = 0; i < ARRAY_SIZE(a6xx_non_ctx_dbgahb); i++) {
		struct a6xx_non_ctx_dbgahb_registers *regs =
				&a6xx_non_ctx_dbgahb[i];

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
	if (kgsl_allocate_global(device, &a6xx_capturescript,
		script_size + 16, KGSL_MEMFLAGS_GPUREADONLY,
		KGSL_MEMDESC_PRIVILEGED, "capturescript"))
		return;

	if (kgsl_allocate_global(device, &a6xx_crashdump_registers, data_size,
		0, KGSL_MEMDESC_PRIVILEGED, "capturescript_regs")) {
		kgsl_free_global(KGSL_DEVICE(adreno_dev), &a6xx_capturescript);
		return;
	}

	/* Build the crash script */

	ptr = (uint64_t *)a6xx_capturescript.hostptr;

	/* For the registers, program a read command for each pair */
	for (i = 0; i < ARRAY_SIZE(a6xx_reg_list); i++) {
		struct reg_list *regs = &a6xx_reg_list[i];

		regs->offset = offset;

		/* Program the SEL_CNTL_CD register appropriately */
		if (regs->sel) {
			*ptr++ = regs->sel->val;
			*ptr++ = (((uint64_t)regs->sel->cd_reg << 44)) |
					(1 << 21) | 1;
		}

		for (j = 0; j < regs->count; j++) {
			unsigned int r = REG_PAIR_COUNT(regs->regs, j);
			*ptr++ = a6xx_crashdump_registers.gpuaddr + offset;
			*ptr++ = (((uint64_t) regs->regs[2 * j]) << 44) | r;
			offset += r * sizeof(unsigned int);
		}
	}

	/* Program each shader block */
	for (i = 0; i < ARRAY_SIZE(a6xx_shader_blocks); i++) {
		ptr += _a6xx_crashdump_init_shader(&a6xx_shader_blocks[i], ptr,
							&offset);
	}

	/* Program the capturescript for the MVC regsiters */
	ptr += _a6xx_crashdump_init_mvc(adreno_dev, ptr, &offset);

	ptr += _a6xx_crashdump_init_ctx_dbgahb(ptr, &offset);

	ptr += _a6xx_crashdump_init_non_ctx_dbgahb(ptr, &offset);

	*ptr++ = 0;
	*ptr++ = 0;
}
