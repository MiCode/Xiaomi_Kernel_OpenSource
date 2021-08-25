/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */
#ifndef __ADRENO_GEN7_SNAPSHOT_H
#define __ADRENO_GEN7_SNAPSHOT_H

#include "adreno.h"
#include "adreno_gen7.h"

#define PIPE_NONE 0
#define PIPE_BR 1
#define PIPE_BV 2
#define PIPE_LPAC 3

#define CLUSTER_NONE 0
#define CLUSTER_FE 1
#define CLUSTER_SP_VS 2
#define CLUSTER_PC_VS 3
#define CLUSTER_GRAS 4
#define CLUSTER_SP_PS 5
#define CLUSTER_VPC_PS 6
#define CLUSTER_PS 7

#define HLSQ_State 0
#define HLSQ_DP 1
#define SP_TOP 2
#define USPTP 3

#define STATE_NON_CONTEXT 0
#define STATE_TOGGLE_CTXT 1
#define STATE_FORCE_CTXT_0 2
#define STATE_FORCE_CTXT_1 3

enum gen7_debugbus_ids {
	DEBUGBUS_CP_0_0           = 1,
	DEBUGBUS_CP_0_1           = 2,
	DEBUGBUS_RBBM             = 3,
	DEBUGBUS_GBIF_GX          = 5,
	DEBUGBUS_GBIF_CX          = 6,
	DEBUGBUS_HLSQ             = 7,
	DEBUGBUS_UCHE_0           = 9,
	DEBUGBUS_TESS_BR          = 13,
	DEBUGBUS_TESS_BV          = 14,
	DEBUGBUS_PC_BR            = 17,
	DEBUGBUS_PC_BV            = 18,
	DEBUGBUS_VFDP_BR          = 21,
	DEBUGBUS_VFDP_BV          = 22,
	DEBUGBUS_VPC_BR           = 25,
	DEBUGBUS_VPC_BV           = 26,
	DEBUGBUS_TSE_BR           = 29,
	DEBUGBUS_TSE_BV           = 30,
	DEBUGBUS_RAS_BR           = 33,
	DEBUGBUS_RAS_BV           = 34,
	DEBUGBUS_VSC              = 37,
	DEBUGBUS_COM_0            = 39,
	DEBUGBUS_LRZ_BR           = 43,
	DEBUGBUS_LRZ_BV           = 44,
	DEBUGBUS_UFC_0            = 47,
	DEBUGBUS_UFC_1            = 48,
	DEBUGBUS_GMU_GX           = 55,
	DEBUGBUS_DBGC             = 59,
	DEBUGBUS_CX               = 60,
	DEBUGBUS_GMU_CX           = 61,
	DEBUGBUS_GPC_BR           = 62,
	DEBUGBUS_GPC_BV           = 63,
	DEBUGBUS_LARC             = 66,
	DEBUGBUS_HLSQ_SPTP        = 68,
	DEBUGBUS_RB_0             = 70,
	DEBUGBUS_RB_1             = 71,
	DEBUGBUS_RB_2             = 72,
	DEBUGBUS_RB_3             = 73,
	DEBUGBUS_UCHE_WRAPPER     = 102,
	DEBUGBUS_CCU_0            = 106,
	DEBUGBUS_CCU_1            = 107,
	DEBUGBUS_CCU_2            = 108,
	DEBUGBUS_CCU_3            = 109,
	DEBUGBUS_VFD_BR_0         = 138,
	DEBUGBUS_VFD_BR_1         = 139,
	DEBUGBUS_VFD_BR_2         = 140,
	DEBUGBUS_VFD_BR_3         = 141,
	DEBUGBUS_VFD_BR_4         = 142,
	DEBUGBUS_VFD_BR_5         = 143,
	DEBUGBUS_VFD_BR_6         = 144,
	DEBUGBUS_VFD_BR_7         = 145,
	DEBUGBUS_VFD_BV_0         = 202,
	DEBUGBUS_VFD_BV_1         = 203,
	DEBUGBUS_VFD_BV_2         = 204,
	DEBUGBUS_VFD_BV_3         = 205,
	DEBUGBUS_USP_0            = 234,
	DEBUGBUS_USP_1            = 235,
	DEBUGBUS_USP_2            = 236,
	DEBUGBUS_USP_3            = 237,
	DEBUGBUS_TP_0             = 266,
	DEBUGBUS_TP_1             = 267,
	DEBUGBUS_TP_2             = 268,
	DEBUGBUS_TP_3             = 269,
	DEBUGBUS_TP_4             = 270,
	DEBUGBUS_TP_5             = 271,
	DEBUGBUS_TP_6             = 272,
	DEBUGBUS_TP_7             = 273,
	DEBUGBUS_USPTP_0          = 330,
	DEBUGBUS_USPTP_1          = 331,
	DEBUGBUS_USPTP_2          = 332,
	DEBUGBUS_USPTP_3          = 333,
	DEBUGBUS_USPTP_4          = 334,
	DEBUGBUS_USPTP_5          = 335,
	DEBUGBUS_USPTP_6          = 336,
	DEBUGBUS_USPTP_7          = 337,
};

static const u32 gen7_debugbus_blocks[] = {
	DEBUGBUS_CP_0_0,
	DEBUGBUS_CP_0_1,
	DEBUGBUS_RBBM,
	DEBUGBUS_HLSQ,
	DEBUGBUS_UCHE_0,
	DEBUGBUS_TESS_BR,
	DEBUGBUS_TESS_BV,
	DEBUGBUS_PC_BR,
	DEBUGBUS_PC_BV,
	DEBUGBUS_VFDP_BR,
	DEBUGBUS_VFDP_BV,
	DEBUGBUS_VPC_BR,
	DEBUGBUS_VPC_BV,
	DEBUGBUS_TSE_BR,
	DEBUGBUS_TSE_BV,
	DEBUGBUS_RAS_BR,
	DEBUGBUS_RAS_BV,
	DEBUGBUS_VSC,
	DEBUGBUS_COM_0,
	DEBUGBUS_LRZ_BR,
	DEBUGBUS_LRZ_BV,
	DEBUGBUS_UFC_0,
	DEBUGBUS_UFC_1,
	DEBUGBUS_GMU_GX,
	DEBUGBUS_DBGC,
	DEBUGBUS_GPC_BR,
	DEBUGBUS_GPC_BV,
	DEBUGBUS_LARC,
	DEBUGBUS_HLSQ_SPTP,
	DEBUGBUS_RB_0,
	DEBUGBUS_RB_1,
	DEBUGBUS_RB_2,
	DEBUGBUS_RB_3,
	DEBUGBUS_UCHE_WRAPPER,
	DEBUGBUS_CCU_0,
	DEBUGBUS_CCU_1,
	DEBUGBUS_CCU_2,
	DEBUGBUS_CCU_3,
	DEBUGBUS_VFD_BR_0,
	DEBUGBUS_VFD_BR_1,
	DEBUGBUS_VFD_BR_2,
	DEBUGBUS_VFD_BR_3,
	DEBUGBUS_VFD_BR_4,
	DEBUGBUS_VFD_BR_5,
	DEBUGBUS_VFD_BR_6,
	DEBUGBUS_VFD_BR_7,
	DEBUGBUS_VFD_BV_0,
	DEBUGBUS_VFD_BV_1,
	DEBUGBUS_VFD_BV_2,
	DEBUGBUS_VFD_BV_3,
	DEBUGBUS_USP_0,
	DEBUGBUS_USP_1,
	DEBUGBUS_USP_2,
	DEBUGBUS_USP_3,
	DEBUGBUS_TP_0,
	DEBUGBUS_TP_1,
	DEBUGBUS_TP_2,
	DEBUGBUS_TP_3,
	DEBUGBUS_TP_4,
	DEBUGBUS_TP_5,
	DEBUGBUS_TP_6,
	DEBUGBUS_TP_7,
	DEBUGBUS_USPTP_0,
	DEBUGBUS_USPTP_1,
	DEBUGBUS_USPTP_2,
	DEBUGBUS_USPTP_3,
	DEBUGBUS_USPTP_4,
	DEBUGBUS_USPTP_5,
	DEBUGBUS_USPTP_6,
	DEBUGBUS_USPTP_7,
};

enum gen7_statetype_ids {
	TP0_NCTX_REG              = 0,
	TP0_CTX0_3D_CVS_REG       = 1,
	TP0_CTX0_3D_CPS_REG       = 2,
	TP0_CTX1_3D_CVS_REG       = 3,
	TP0_CTX1_3D_CPS_REG       = 4,
	TP0_CTX2_3D_CPS_REG       = 5,
	TP0_CTX3_3D_CPS_REG       = 6,
	TP0_TMO_DATA              = 9,
	TP0_SMO_DATA              = 10,
	TP0_MIPMAP_BASE_DATA      = 11,
	SP_NCTX_REG               = 32,
	SP_CTX0_3D_CVS_REG        = 33,
	SP_CTX0_3D_CPS_REG        = 34,
	SP_CTX1_3D_CVS_REG        = 35,
	SP_CTX1_3D_CPS_REG        = 36,
	SP_CTX2_3D_CPS_REG        = 37,
	SP_CTX3_3D_CPS_REG        = 38,
	SP_INST_DATA              = 39,
	SP_INST_DATA_1            = 40,
	SP_LB_0_DATA              = 41,
	SP_LB_1_DATA              = 42,
	SP_LB_2_DATA              = 43,
	SP_LB_3_DATA              = 44,
	SP_LB_4_DATA              = 45,
	SP_LB_5_DATA              = 46,
	SP_LB_6_DATA              = 47,
	SP_LB_7_DATA              = 48,
	SP_CB_RAM                 = 49,
	SP_INST_TAG               = 52,
	SP_INST_DATA_2            = 53,
	SP_TMO_TAG                = 54,
	SP_SMO_TAG                = 55,
	SP_STATE_DATA             = 56,
	SP_HWAVE_RAM              = 57,
	SP_L0_INST_BUF            = 58,
	SP_LB_8_DATA              = 59,
	SP_LB_9_DATA              = 60,
	SP_LB_10_DATA             = 61,
	SP_LB_11_DATA             = 62,
	SP_LB_12_DATA             = 63,
	HLSQ_CVS_BE_CTXT_BUF_RAM_TAG = 69,
	HLSQ_CPS_BE_CTXT_BUF_RAM_TAG = 70,
	HLSQ_GFX_CVS_BE_CTXT_BUF_RAM = 71,
	HLSQ_GFX_CPS_BE_CTXT_BUF_RAM = 72,
	HLSQ_CHUNK_CVS_RAM        = 73,
	HLSQ_CHUNK_CPS_RAM        = 74,
	HLSQ_CHUNK_CVS_RAM_TAG    = 75,
	HLSQ_CHUNK_CPS_RAM_TAG    = 76,
	HLSQ_ICB_CVS_CB_BASE_TAG  = 77,
	HLSQ_ICB_CPS_CB_BASE_TAG  = 78,
	HLSQ_CVS_MISC_RAM         = 79,
	HLSQ_CPS_MISC_RAM         = 80,
	HLSQ_CPS_MISC_RAM_1       = 81,
	HLSQ_INST_RAM             = 82,
	HLSQ_GFX_CVS_CONST_RAM    = 83,
	HLSQ_GFX_CPS_CONST_RAM    = 84,
	HLSQ_CVS_MISC_RAM_TAG     = 85,
	HLSQ_CPS_MISC_RAM_TAG     = 86,
	HLSQ_INST_RAM_TAG         = 87,
	HLSQ_GFX_CVS_CONST_RAM_TAG = 88,
	HLSQ_GFX_CPS_CONST_RAM_TAG = 89,
	HLSQ_INST_RAM_1           = 92,
	HLSQ_STPROC_META          = 93,
	HLSQ_BV_BE_META           = 94,
	HLSQ_DATAPATH_META        = 96,
	HLSQ_FRONTEND_META        = 97,
	HLSQ_INDIRECT_META        = 98,
	HLSQ_BACKEND_META         = 99,
};

static const struct sel_reg {
	unsigned int host_reg;
	unsigned int cd_reg;
	unsigned int val;
} gen7_0_0_rb_rac_sel = {
	.host_reg = GEN7_RB_RB_SUB_BLOCK_SEL_CNTL_HOST,
	.cd_reg = GEN7_RB_RB_SUB_BLOCK_SEL_CNTL_CD,
	.val = 0x0,
},
gen7_0_0_rb_rbp_sel = {
	.host_reg = GEN7_RB_RB_SUB_BLOCK_SEL_CNTL_HOST,
	.cd_reg = GEN7_RB_RB_SUB_BLOCK_SEL_CNTL_CD,
	.val = 0x9,
};

static const u32 gen7_pre_crashdumper_registers[] = {
	0x00210, 0x00210, 0x00212, 0x00213, 0x03c00, 0x03c0b, 0x03c40, 0x03c42,
	0x03c45, 0x03c47, 0x03c49, 0x03c4a, 0x03cc0, 0x03cd1,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_pre_crashdumper_registers), 8));

static const u32 gen7_post_crashdumper_registers[] = {
	0x00535, 0x00535,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_post_crashdumper_registers), 8));

static const u32 gen7_gpu_registers[] = {
	0x00000, 0x00000, 0x00002, 0x00002, 0x00011, 0x00012, 0x00016, 0x0001b,
	0x0001f, 0x00032, 0x00038, 0x0003c, 0x00042, 0x00042, 0x00044, 0x00044,
	0x00047, 0x00047, 0x00049, 0x0004a, 0x0004c, 0x0004c, 0x00050, 0x00050,
	0x00056, 0x00056, 0x00073, 0x00075, 0x000ad, 0x000ae, 0x000b0, 0x000b0,
	0x000b4, 0x000b4, 0x000b8, 0x000b8, 0x000bc, 0x000bc, 0x000c0, 0x000c0,
	0x000c4, 0x000c4, 0x000c8, 0x000c8, 0x000cc, 0x000cc, 0x000d0, 0x000d0,
	0x000d4, 0x000d4, 0x000d8, 0x000d8, 0x000dc, 0x000dc, 0x000e0, 0x000e0,
	0x000e4, 0x000e4, 0x000e8, 0x000e8, 0x000ec, 0x000ec, 0x000f0, 0x000f0,
	0x000f4, 0x000f4, 0x000f8, 0x000f8, 0x00100, 0x00100, 0x00104, 0x0010b,
	0x0010f, 0x0011d, 0x0012f, 0x0012f, 0x00200, 0x0020d, 0x00211, 0x00211,
	0x00215, 0x00243, 0x00260, 0x00268, 0x00272, 0x00274, 0x00281, 0x0028d,
	0x00300, 0x00401, 0x00410, 0x00451, 0x00460, 0x004a3, 0x004c0, 0x004d1,
	0x00500, 0x00500, 0x00507, 0x0050b, 0x0050f, 0x0050f, 0x00511, 0x00511,
	0x00533, 0x00534, 0x00536, 0x00536, 0x00540, 0x00555, 0x00564, 0x00567,
	0x00574, 0x00577, 0x005fb, 0x005ff, 0x00800, 0x00808, 0x00810, 0x00813,
	0x00820, 0x00821, 0x00823, 0x00827, 0x00830, 0x00834, 0x0083f, 0x00841,
	0x00843, 0x00847, 0x0084f, 0x00886, 0x008a0, 0x008ab, 0x008c0, 0x008c0,
	0x008c4, 0x008c5, 0x008d0, 0x008dd, 0x008e0, 0x008e6, 0x008f0, 0x008f3,
	0x00900, 0x00903, 0x00908, 0x00911, 0x00928, 0x0093e, 0x00942, 0x0094d,
	0x00980, 0x00984, 0x0098d, 0x0098f, 0x009b0, 0x009b4, 0x009c2, 0x009c9,
	0x009ce, 0x009d7, 0x009e0, 0x009e7, 0x00a00, 0x00a00, 0x00a02, 0x00a03,
	0x00a10, 0x00a4f, 0x00a61, 0x00a9f, 0x00ad0, 0x00adb, 0x00b00, 0x00b31,
	0x00b35, 0x00b3c, 0x00b40, 0x00b40, 0x00c00, 0x00c00, 0x00c02, 0x00c04,
	0x00c06, 0x00c06, 0x00c10, 0x00cd9, 0x00ce0, 0x00d0c, 0x00df0, 0x00df4,
	0x00e01, 0x00e02, 0x00e07, 0x00e0e, 0x00e10, 0x00e13, 0x00e17, 0x00e19,
	0x00e1b, 0x00e2b, 0x00e30, 0x00e32, 0x00e38, 0x00e3c,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_gpu_registers), 8));

static const u32 gen7_cx_misc_registers[] = {
	0x27800, 0x27800, 0x27810, 0x27814, 0x27820, 0x27824, 0x27832, 0x27857,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_cx_misc_registers), 8));

static const u32 gen7_cpr_registers[] = {
	0x26800, 0x26805, 0x26808, 0x2680c, 0x26814, 0x26814, 0x2681c, 0x2681c,
	0x26820, 0x26838, 0x26840, 0x26840, 0x26848, 0x26848, 0x26850, 0x26850,
	0x26880, 0x26898, 0x26980, 0x269b0, 0x269c0, 0x269c8, 0x269e0, 0x269ee,
	0x269fb, 0x269ff, 0x26a02, 0x26a07, 0x26a09, 0x26a0b, 0x26a10, 0x26b0f,
	0x27440, 0x27441, 0x27444, 0x27444, 0x27480, 0x274a2, 0x274ac, 0x274ac,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_cpr_registers), 8));

static const u32 gen7_dpm_registers[] = {
	0x1aa00, 0x1aa06, 0x1aa09, 0x1aa0a, 0x1aa0c, 0x1aa0d, 0x1aa0f, 0x1aa12,
	0x1aa14, 0x1aa47, 0x1aa50, 0x1aa51,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_dpm_registers), 8));

static const u32 gen7_gpucc_registers[] = {
	0x24000, 0x2400e, 0x24400, 0x2440e, 0x24800, 0x24805, 0x24c00, 0x24cff,
	0x25800, 0x25804, 0x25c00, 0x25c04, 0x26000, 0x26004, 0x26400, 0x26405,
	0x26414, 0x2641d, 0x2642a, 0x26430, 0x26432, 0x26432, 0x26441, 0x26455,
	0x26466, 0x26468, 0x26478, 0x2647a, 0x26489, 0x2648a, 0x2649c, 0x2649e,
	0x264a0, 0x264a3, 0x264b3, 0x264b5, 0x264c5, 0x264c7, 0x264d6, 0x264d8,
	0x264e8, 0x264e9, 0x264f9, 0x264fc, 0x2650b, 0x2650c, 0x2651c, 0x2651e,
	0x26540, 0x26570, 0x26600, 0x26616, 0x26620, 0x2662d,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_gpucc_registers), 8));

static const u32 gen7_0_0_noncontext_pipe_br_registers[] = {
	0x00887, 0x0088c, 0x08600, 0x08600, 0x08602, 0x08602, 0x08610, 0x0861b,
	0x08620, 0x08620, 0x08630, 0x08630, 0x08637, 0x08639, 0x08640, 0x08640,
	0x09600, 0x09600, 0x09602, 0x09603, 0x0960a, 0x09616, 0x09624, 0x0963a,
	0x09640, 0x09640, 0x09e00, 0x09e00, 0x09e02, 0x09e07, 0x09e0a, 0x09e16,
	0x09e19, 0x09e19, 0x09e1c, 0x09e1c, 0x09e20, 0x09e25, 0x09e30, 0x09e31,
	0x09e40, 0x09e51, 0x09e64, 0x09e64, 0x09e70, 0x09e72, 0x09e78, 0x09e79,
	0x09e80, 0x09fff, 0x0a600, 0x0a600, 0x0a603, 0x0a603, 0x0a610, 0x0a61f,
	0x0a630, 0x0a631, 0x0a638, 0x0a638,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_noncontext_pipe_br_registers), 8));

static const u32 gen7_0_0_noncontext_pipe_bv_registers[] = {
	0x00887, 0x0088c, 0x08600, 0x08600, 0x08602, 0x08602, 0x08610, 0x0861b,
	0x08620, 0x08620, 0x08630, 0x08630, 0x08637, 0x08639, 0x08640, 0x08640,
	0x09600, 0x09600, 0x09602, 0x09603, 0x0960a, 0x09616, 0x09624, 0x0963a,
	0x09640, 0x09640, 0x09e00, 0x09e00, 0x09e02, 0x09e07, 0x09e0a, 0x09e16,
	0x09e19, 0x09e19, 0x09e1c, 0x09e1c, 0x09e20, 0x09e25, 0x09e30, 0x09e31,
	0x09e40, 0x09e51, 0x09e64, 0x09e64, 0x09e70, 0x09e72, 0x09e78, 0x09e79,
	0x09e80, 0x09fff, 0x0a600, 0x0a600, 0x0a603, 0x0a603, 0x0a610, 0x0a61f,
	0x0a630, 0x0a631, 0x0a638, 0x0a638,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_noncontext_pipe_bv_registers), 8));

static const u32 gen7_0_0_noncontext_pipe_lpac_registers[] = {
	0x00887, 0x0088c, 0x00f80, 0x00f80,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_noncontext_pipe_lpac_registers), 8));

static const u32 gen7_0_0_noncontext_rb_rac_pipe_br_registers[] = {
	0x08e10, 0x08e1c, 0x08e20, 0x08e25, 0x08e51, 0x08e5a,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_noncontext_rb_rac_pipe_br_registers), 8));

static const u32 gen7_0_0_noncontext_rb_rbp_pipe_br_registers[] = {
	0x08e01, 0x08e01, 0x08e04, 0x08e04, 0x08e06, 0x08e09, 0x08e0c, 0x08e0c,
	0x08e28, 0x08e28, 0x08e2c, 0x08e35, 0x08e3b, 0x08e3f, 0x08e50, 0x08e50,
	0x08e5b, 0x08e5d, 0x08e5f, 0x08e5f, 0x08e61, 0x08e61, 0x08e63, 0x08e65,
	0x08e68, 0x08e68, 0x08e70, 0x08e79, 0x08e80, 0x08e8f,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_noncontext_rb_rbp_pipe_br_registers), 8));

/* Block: GRAS Cluster: CLUSTER_GRAS Pipeline: PIPE_BR */
static const u32 gen7_0_0_gras_cluster_gras_pipe_br_registers[] = {
	0x08000, 0x08008, 0x08010, 0x08092, 0x08094, 0x08099, 0x0809b, 0x0809d,
	0x080a0, 0x080a7, 0x080af, 0x080f1, 0x080f4, 0x080f6, 0x080f8, 0x080fa,
	0x08100, 0x08107, 0x08109, 0x0810b, 0x08110, 0x08110, 0x08120, 0x0813f,
	0x08400, 0x08406, 0x0840a, 0x0840b,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_gras_cluster_gras_pipe_br_registers), 8));

/* Block: GRAS Cluster: CLUSTER_GRAS Pipeline: PIPE_BV */
static const u32 gen7_0_0_gras_cluster_gras_pipe_bv_registers[] = {
	0x08000, 0x08008, 0x08010, 0x08092, 0x08094, 0x08099, 0x0809b, 0x0809d,
	0x080a0, 0x080a7, 0x080af, 0x080f1, 0x080f4, 0x080f6, 0x080f8, 0x080fa,
	0x08100, 0x08107, 0x08109, 0x0810b, 0x08110, 0x08110, 0x08120, 0x0813f,
	0x08400, 0x08406, 0x0840a, 0x0840b,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_gras_cluster_gras_pipe_bv_registers), 8));

/* Block: PC Cluster: CLUSTER_FE Pipeline: PIPE_BR */
static const u32 gen7_0_0_pc_cluster_fe_pipe_br_registers[] = {
	0x09800, 0x09804, 0x09806, 0x0980a, 0x09810, 0x09811, 0x09884, 0x09886,
	0x09b00, 0x09b08,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_pc_cluster_fe_pipe_br_registers), 8));

/* Block: PC Cluster: CLUSTER_FE Pipeline: PIPE_BV */
static const u32 gen7_0_0_pc_cluster_fe_pipe_bv_registers[] = {
	0x09800, 0x09804, 0x09806, 0x0980a, 0x09810, 0x09811, 0x09884, 0x09886,
	0x09b00, 0x09b08,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_pc_cluster_fe_pipe_bv_registers), 8));

/* Block: RB_RAC Cluster: CLUSTER_PS Pipeline: PIPE_BR */
static const u32 gen7_0_0_rb_rac_cluster_ps_pipe_br_registers[] = {
	0x08802, 0x08802, 0x08804, 0x08806, 0x08809, 0x0880a, 0x0880e, 0x08811,
	0x08818, 0x0881e, 0x08821, 0x08821, 0x08823, 0x08826, 0x08829, 0x08829,
	0x0882b, 0x0882e, 0x08831, 0x08831, 0x08833, 0x08836, 0x08839, 0x08839,
	0x0883b, 0x0883e, 0x08841, 0x08841, 0x08843, 0x08846, 0x08849, 0x08849,
	0x0884b, 0x0884e, 0x08851, 0x08851, 0x08853, 0x08856, 0x08859, 0x08859,
	0x0885b, 0x0885e, 0x08860, 0x08864, 0x08870, 0x08870, 0x08873, 0x08876,
	0x08878, 0x08879, 0x08882, 0x08885, 0x08887, 0x08889, 0x08891, 0x08891,
	0x08898, 0x08898, 0x088c0, 0x088c1, 0x088e5, 0x088e5, 0x088f4, 0x088f5,
	0x08a00, 0x08a05, 0x08a10, 0x08a15, 0x08a20, 0x08a25, 0x08a30, 0x08a35,
	0x08c00, 0x08c01, 0x08c18, 0x08c1f, 0x08c26, 0x08c34,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_rb_rac_cluster_ps_pipe_br_registers), 8));

/* Block: RB_RBP Cluster: CLUSTER_PS Pipeline: PIPE_BR */
static const u32 gen7_0_0_rb_rbp_cluster_ps_pipe_br_registers[] = {
	0x08800, 0x08801, 0x08803, 0x08803, 0x0880b, 0x0880d, 0x08812, 0x08812,
	0x08820, 0x08820, 0x08822, 0x08822, 0x08827, 0x08828, 0x0882a, 0x0882a,
	0x0882f, 0x08830, 0x08832, 0x08832, 0x08837, 0x08838, 0x0883a, 0x0883a,
	0x0883f, 0x08840, 0x08842, 0x08842, 0x08847, 0x08848, 0x0884a, 0x0884a,
	0x0884f, 0x08850, 0x08852, 0x08852, 0x08857, 0x08858, 0x0885a, 0x0885a,
	0x0885f, 0x0885f, 0x08865, 0x08865, 0x08871, 0x08872, 0x08877, 0x08877,
	0x08880, 0x08881, 0x08886, 0x08886, 0x08890, 0x08890, 0x088d0, 0x088e4,
	0x088e8, 0x088ea, 0x088f0, 0x088f0, 0x08900, 0x0891a, 0x08927, 0x08928,
	0x08c17, 0x08c17, 0x08c20, 0x08c25,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_rb_rbp_cluster_ps_pipe_br_registers), 8));

/* Block: SP Cluster: CLUSTER_SP_PS Pipeline: PIPE_BR Location: HLSQ_State */
static const u32 gen7_0_0_sp_cluster_sp_ps_pipe_br_hlsq_state_registers[] = {
	0x0a980, 0x0a980, 0x0a982, 0x0a984, 0x0a99e, 0x0a99e, 0x0a9a7, 0x0a9a7,
	0x0a9aa, 0x0a9aa, 0x0a9ae, 0x0a9b0, 0x0a9b3, 0x0a9b5, 0x0a9ba, 0x0a9ba,
	0x0a9bc, 0x0a9bc, 0x0a9c4, 0x0a9c4, 0x0a9cd, 0x0a9cd, 0x0a9e0, 0x0a9fc,
	0x0aa00, 0x0aa00, 0x0aa30, 0x0aa31, 0x0aa40, 0x0aabf, 0x0ab00, 0x0ab03,
	0x0ab05, 0x0ab05, 0x0ab0a, 0x0ab1b, 0x0ab20, 0x0ab20, 0x0ab40, 0x0abbf,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_sp_cluster_sp_ps_pipe_br_hlsq_state_registers), 8));

/* Block: SP Cluster: CLUSTER_SP_PS Pipeline: PIPE_BR Location: HLSQ_DP */
static const u32 gen7_0_0_sp_cluster_sp_ps_pipe_br_hlsq_dp_registers[] = {
	0x0a9b1, 0x0a9b1, 0x0a9c6, 0x0a9cb, 0x0a9d4, 0x0a9df,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_sp_cluster_sp_ps_pipe_br_hlsq_dp_registers), 8));

/* Block: SP Cluster: CLUSTER_SP_PS Pipeline: PIPE_BR Location: SP_TOP */
static const u32 gen7_0_0_sp_cluster_sp_ps_pipe_br_sp_top_registers[] = {
	0x0a980, 0x0a980, 0x0a982, 0x0a984, 0x0a99e, 0x0a9a2, 0x0a9a7, 0x0a9a8,
	0x0a9aa, 0x0a9aa, 0x0a9ae, 0x0a9ae, 0x0a9b0, 0x0a9b1, 0x0a9b3, 0x0a9b5,
	0x0a9ba, 0x0a9bc, 0x0a9e0, 0x0a9f9, 0x0aa00, 0x0aa00, 0x0ab00, 0x0ab00,
	0x0ab02, 0x0ab02, 0x0ab04, 0x0ab05, 0x0ab0a, 0x0ab1b, 0x0ab20, 0x0ab20,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_sp_cluster_sp_ps_pipe_br_sp_top_registers), 8));

/* Block: SP Cluster: CLUSTER_SP_PS Pipeline: PIPE_BR Location: uSPTP */
static const u32 gen7_0_0_sp_cluster_sp_ps_pipe_br_usptp_registers[] = {
	0x0a980, 0x0a982, 0x0a985, 0x0a9a6, 0x0a9a8, 0x0a9a9, 0x0a9ab, 0x0a9ae,
	0x0a9b0, 0x0a9b3, 0x0a9b6, 0x0a9b9, 0x0a9bb, 0x0a9bf, 0x0a9c2, 0x0a9c3,
	0x0a9cd, 0x0a9cd, 0x0a9d0, 0x0a9d3, 0x0aa30, 0x0aa31, 0x0aa40, 0x0aabf,
	0x0ab00, 0x0ab05, 0x0ab21, 0x0ab22, 0x0ab40, 0x0abbf,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_sp_cluster_sp_ps_pipe_br_usptp_registers), 8));

/* Block: SP Cluster: CLUSTER_SP_PS Pipeline: PIPE_BV Location: HLSQ_State */
static const u32 gen7_0_0_sp_cluster_sp_ps_pipe_bv_hlsq_state_registers[] = {
	0x0ab00, 0x0ab02, 0x0ab0a, 0x0ab1b, 0x0ab20, 0x0ab20, 0x0ab40, 0x0abbf,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_sp_cluster_sp_ps_pipe_bv_hlsq_state_registers), 8));

/* Block: SP Cluster: CLUSTER_SP_PS Pipeline: PIPE_BV Location: SP_TOP */
static const u32 gen7_0_0_sp_cluster_sp_ps_pipe_bv_sp_top_registers[] = {
	0x0ab00, 0x0ab00, 0x0ab02, 0x0ab02, 0x0ab0a, 0x0ab1b, 0x0ab20, 0x0ab20,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_sp_cluster_sp_ps_pipe_bv_sp_top_registers), 8));

/* Block: SP Cluster: CLUSTER_SP_PS Pipeline: PIPE_BV Location: uSPTP */
static const u32 gen7_0_0_sp_cluster_sp_ps_pipe_bv_usptp_registers[] = {
	0x0ab00, 0x0ab02, 0x0ab21, 0x0ab22, 0x0ab40, 0x0abbf,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_sp_cluster_sp_ps_pipe_bv_usptp_registers), 8));

/* Block: SP Cluster: CLUSTER_SP_PS Pipeline: PIPE_LPAC Location: HLSQ_State */
static const u32 gen7_0_0_sp_cluster_sp_ps_pipe_lpac_hlsq_state_registers[] = {
	0x0a9b0, 0x0a9b0, 0x0a9b3, 0x0a9b5, 0x0a9ba, 0x0a9ba, 0x0a9bc, 0x0a9bc,
	0x0a9c4, 0x0a9c4, 0x0a9cd, 0x0a9cd, 0x0a9e2, 0x0a9e3, 0x0a9e6, 0x0a9fc,
	0x0aa00, 0x0aa00, 0x0aa31, 0x0aa31, 0x0ab00, 0x0ab01,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_sp_cluster_sp_ps_pipe_lpac_hlsq_state_registers), 8));

/* Block: SP Cluster: CLUSTER_SP_PS Pipeline: PIPE_LPAC Location: HLSQ_DP */
static const u32 gen7_0_0_sp_cluster_sp_ps_pipe_lpac_hlsq_dp_registers[] = {
	0x0a9b1, 0x0a9b1, 0x0a9d4, 0x0a9df,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_sp_cluster_sp_ps_pipe_lpac_hlsq_dp_registers), 8));

/* Block: SP Cluster: CLUSTER_SP_PS Pipeline: PIPE_LPAC Location: SP_TOP */
static const u32 gen7_0_0_sp_cluster_sp_ps_pipe_lpac_sp_top_registers[] = {
	0x0a9b0, 0x0a9b1, 0x0a9b3, 0x0a9b5, 0x0a9ba, 0x0a9bc, 0x0a9e2, 0x0a9e3,
	0x0a9e6, 0x0a9f9, 0x0aa00, 0x0aa00, 0x0ab00, 0x0ab00,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_sp_cluster_sp_ps_pipe_lpac_sp_top_registers), 8));

/* Block: SP Cluster: CLUSTER_SP_PS Pipeline: PIPE_LPAC Location: uSPTP */
static const u32 gen7_0_0_sp_cluster_sp_ps_pipe_lpac_usptp_registers[] = {
	0x0a9b0, 0x0a9b3, 0x0a9b6, 0x0a9b9, 0x0a9bb, 0x0a9be, 0x0a9c2, 0x0a9c3,
	0x0a9cd, 0x0a9cd, 0x0a9d0, 0x0a9d3, 0x0aa31, 0x0aa31, 0x0ab00, 0x0ab01,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_sp_cluster_sp_ps_pipe_lpac_usptp_registers), 8));

/* Block: SP Cluster: CLUSTER_SP_VS Pipeline: PIPE_BR Location: HLSQ_State */
static const u32 gen7_0_0_sp_cluster_sp_vs_pipe_br_hlsq_state_registers[] = {
	0x0a800, 0x0a800, 0x0a81b, 0x0a81d, 0x0a822, 0x0a822, 0x0a824, 0x0a824,
	0x0a827, 0x0a82a, 0x0a830, 0x0a830, 0x0a833, 0x0a835, 0x0a83a, 0x0a83a,
	0x0a83c, 0x0a83c, 0x0a83f, 0x0a840, 0x0a85b, 0x0a85d, 0x0a862, 0x0a862,
	0x0a864, 0x0a864, 0x0a867, 0x0a867, 0x0a870, 0x0a870, 0x0a88c, 0x0a88e,
	0x0a893, 0x0a893, 0x0a895, 0x0a895, 0x0a898, 0x0a898, 0x0a89a, 0x0a89d,
	0x0a8a0, 0x0a8af, 0x0a8c0, 0x0a8c3, 0x0ab00, 0x0ab03, 0x0ab05, 0x0ab05,
	0x0ab0a, 0x0ab1b, 0x0ab20, 0x0ab20, 0x0ab40, 0x0abbf,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_sp_cluster_sp_vs_pipe_br_hlsq_state_registers), 8));

/* Block: SP Cluster: CLUSTER_SP_VS Pipeline: PIPE_BR Location: SP_TOP */
static const u32 gen7_0_0_sp_cluster_sp_vs_pipe_br_sp_top_registers[] = {
	0x0a800, 0x0a800, 0x0a81c, 0x0a81d, 0x0a822, 0x0a824, 0x0a830, 0x0a831,
	0x0a834, 0x0a835, 0x0a83a, 0x0a83c, 0x0a840, 0x0a840, 0x0a85c, 0x0a85d,
	0x0a862, 0x0a864, 0x0a870, 0x0a871, 0x0a88d, 0x0a88e, 0x0a893, 0x0a895,
	0x0a8a0, 0x0a8af, 0x0ab00, 0x0ab00, 0x0ab02, 0x0ab02, 0x0ab04, 0x0ab05,
	0x0ab0a, 0x0ab1b, 0x0ab20, 0x0ab20,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_sp_cluster_sp_vs_pipe_br_sp_top_registers), 8));

/* Block: SP Cluster: CLUSTER_SP_VS Pipeline: PIPE_BR Location: uSPTP */
static const u32 gen7_0_0_sp_cluster_sp_vs_pipe_br_usptp_registers[] = {
	0x0a800, 0x0a81b, 0x0a81e, 0x0a821, 0x0a823, 0x0a827, 0x0a830, 0x0a833,
	0x0a836, 0x0a839, 0x0a83b, 0x0a85b, 0x0a85e, 0x0a861, 0x0a863, 0x0a867,
	0x0a870, 0x0a88c, 0x0a88f, 0x0a892, 0x0a894, 0x0a898, 0x0a8c0, 0x0a8c3,
	0x0ab00, 0x0ab05, 0x0ab21, 0x0ab22, 0x0ab40, 0x0abbf,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_sp_cluster_sp_vs_pipe_br_usptp_registers), 8));

/* Block: SP Cluster: CLUSTER_SP_VS Pipeline: PIPE_BV Location: HLSQ_State */
static const u32 gen7_0_0_sp_cluster_sp_vs_pipe_bv_hlsq_state_registers[] = {
	0x0a800, 0x0a800, 0x0a81b, 0x0a81d, 0x0a822, 0x0a822, 0x0a824, 0x0a824,
	0x0a827, 0x0a82a, 0x0a830, 0x0a830, 0x0a833, 0x0a835, 0x0a83a, 0x0a83a,
	0x0a83c, 0x0a83c, 0x0a83f, 0x0a840, 0x0a85b, 0x0a85d, 0x0a862, 0x0a862,
	0x0a864, 0x0a864, 0x0a867, 0x0a867, 0x0a870, 0x0a870, 0x0a88c, 0x0a88e,
	0x0a893, 0x0a893, 0x0a895, 0x0a895, 0x0a898, 0x0a898, 0x0a89a, 0x0a89d,
	0x0a8a0, 0x0a8af, 0x0a8c0, 0x0a8c3, 0x0ab00, 0x0ab02, 0x0ab0a, 0x0ab1b,
	0x0ab20, 0x0ab20, 0x0ab40, 0x0abbf,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_sp_cluster_sp_vs_pipe_bv_hlsq_state_registers), 8));

/* Block: SP Cluster: CLUSTER_SP_VS Pipeline: PIPE_BV Location: SP_TOP */
static const u32 gen7_0_0_sp_cluster_sp_vs_pipe_bv_sp_top_registers[] = {
	0x0a800, 0x0a800, 0x0a81c, 0x0a81d, 0x0a822, 0x0a824, 0x0a830, 0x0a831,
	0x0a834, 0x0a835, 0x0a83a, 0x0a83c, 0x0a840, 0x0a840, 0x0a85c, 0x0a85d,
	0x0a862, 0x0a864, 0x0a870, 0x0a871, 0x0a88d, 0x0a88e, 0x0a893, 0x0a895,
	0x0a8a0, 0x0a8af, 0x0ab00, 0x0ab00, 0x0ab02, 0x0ab02, 0x0ab0a, 0x0ab1b,
	0x0ab20, 0x0ab20,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_sp_cluster_sp_vs_pipe_bv_sp_top_registers), 8));

/* Block: SP Cluster: CLUSTER_SP_VS Pipeline: PIPE_BV Location: uSPTP */
static const u32 gen7_0_0_sp_cluster_sp_vs_pipe_bv_usptp_registers[] = {
	0x0a800, 0x0a81b, 0x0a81e, 0x0a821, 0x0a823, 0x0a827, 0x0a830, 0x0a833,
	0x0a836, 0x0a839, 0x0a83b, 0x0a85b, 0x0a85e, 0x0a861, 0x0a863, 0x0a867,
	0x0a870, 0x0a88c, 0x0a88f, 0x0a892, 0x0a894, 0x0a898, 0x0a8c0, 0x0a8c3,
	0x0ab00, 0x0ab02, 0x0ab21, 0x0ab22, 0x0ab40, 0x0abbf,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_sp_cluster_sp_vs_pipe_bv_usptp_registers), 8));

/* Block: TPL1 Cluster: CLUSTER_SP_PS Pipeline: PIPE_BR */
static const u32 gen7_0_0_tpl1_cluster_sp_ps_pipe_br_registers[] = {
	0x0b180, 0x0b183, 0x0b190, 0x0b195, 0x0b2c0, 0x0b2d5, 0x0b300, 0x0b307,
	0x0b309, 0x0b309, 0x0b310, 0x0b310,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_tpl1_cluster_sp_ps_pipe_br_registers), 8));

/* Block: TPL1 Cluster: CLUSTER_SP_PS Pipeline: PIPE_BV */
static const u32 gen7_0_0_tpl1_cluster_sp_ps_pipe_bv_registers[] = {
	0x0b300, 0x0b307, 0x0b309, 0x0b309, 0x0b310, 0x0b310,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_tpl1_cluster_sp_ps_pipe_bv_registers), 8));

/* Block: TPL1 Cluster: CLUSTER_SP_PS Pipeline: PIPE_LPAC */
static const u32 gen7_0_0_tpl1_cluster_sp_ps_pipe_lpac_registers[] = {
	0x0b180, 0x0b181, 0x0b300, 0x0b301, 0x0b307, 0x0b307, 0x0b309, 0x0b309,
	0x0b310, 0x0b310,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_tpl1_cluster_sp_ps_pipe_lpac_registers), 8));

/* Block: TPL1 Cluster: CLUSTER_SP_VS Pipeline: PIPE_BR */
static const u32 gen7_0_0_tpl1_cluster_sp_vs_pipe_br_registers[] = {
	0x0b300, 0x0b307, 0x0b309, 0x0b309, 0x0b310, 0x0b310,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_tpl1_cluster_sp_vs_pipe_br_registers), 8));

/* Block: TPL1 Cluster: CLUSTER_SP_VS Pipeline: PIPE_BV */
static const u32 gen7_0_0_tpl1_cluster_sp_vs_pipe_bv_registers[] = {
	0x0b300, 0x0b307, 0x0b309, 0x0b309, 0x0b310, 0x0b310,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_tpl1_cluster_sp_vs_pipe_bv_registers), 8));

/* Block: VFD Cluster: CLUSTER_FE Pipeline: PIPE_BR */
static const u32 gen7_0_0_vfd_cluster_fe_pipe_br_registers[] = {
	0x0a000, 0x0a009, 0x0a00e, 0x0a0ef,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_vfd_cluster_fe_pipe_br_registers), 8));

/* Block: VFD Cluster: CLUSTER_FE Pipeline: PIPE_BV */
static const u32 gen7_0_0_vfd_cluster_fe_pipe_bv_registers[] = {
	0x0a000, 0x0a009, 0x0a00e, 0x0a0ef,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_vfd_cluster_fe_pipe_bv_registers), 8));

/* Block: VPC Cluster: CLUSTER_FE Pipeline: PIPE_BR */
static const u32 gen7_0_0_vpc_cluster_fe_pipe_br_registers[] = {
	0x09300, 0x09307,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_vpc_cluster_fe_pipe_br_registers), 8));

/* Block: VPC Cluster: CLUSTER_FE Pipeline: PIPE_BV */
static const u32 gen7_0_0_vpc_cluster_fe_pipe_bv_registers[] = {
	0x09300, 0x09307,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_vpc_cluster_fe_pipe_bv_registers), 8));

/* Block: VPC Cluster: CLUSTER_PC_VS Pipeline: PIPE_BR */
static const u32 gen7_0_0_vpc_cluster_pc_vs_pipe_br_registers[] = {
	0x09101, 0x0910c, 0x09300, 0x09307,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_vpc_cluster_pc_vs_pipe_br_registers), 8));

/* Block: VPC Cluster: CLUSTER_PC_VS Pipeline: PIPE_BV */
static const u32 gen7_0_0_vpc_cluster_pc_vs_pipe_bv_registers[] = {
	0x09101, 0x0910c, 0x09300, 0x09307,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_vpc_cluster_pc_vs_pipe_bv_registers), 8));

/* Block: VPC Cluster: CLUSTER_VPC_PS Pipeline: PIPE_BR */
static const u32 gen7_0_0_vpc_cluster_vpc_ps_pipe_br_registers[] = {
	0x09200, 0x0920f, 0x09212, 0x09216, 0x09218, 0x09236, 0x09300, 0x09307,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_vpc_cluster_vpc_ps_pipe_br_registers), 8));

/* Block: VPC Cluster: CLUSTER_VPC_PS Pipeline: PIPE_BV */
static const u32 gen7_0_0_vpc_cluster_vpc_ps_pipe_bv_registers[] = {
	0x09200, 0x0920f, 0x09212, 0x09216, 0x09218, 0x09236, 0x09300, 0x09307,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_vpc_cluster_vpc_ps_pipe_bv_registers), 8));

/* Block: SP Cluster: noncontext Pipeline: PIPE_BR Location: HLSQ_State */
static const u32 gen7_0_0_sp_noncontext_pipe_br_hlsq_state_registers[] = {
	0x0ae52, 0x0ae52, 0x0ae60, 0x0ae67, 0x0ae69, 0x0ae73,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_sp_noncontext_pipe_br_hlsq_state_registers), 8));

/* Block: SP Cluster: noncontext Pipeline: PIPE_BR Location: SP_TOP */
static const u32 gen7_0_0_sp_noncontext_pipe_br_sp_top_registers[] = {
	0x0ae00, 0x0ae00, 0x0ae02, 0x0ae04, 0x0ae06, 0x0ae09, 0x0ae0c, 0x0ae0c,
	0x0ae0f, 0x0ae0f, 0x0ae28, 0x0ae2b, 0x0ae35, 0x0ae35, 0x0ae3a, 0x0ae3f,
	0x0ae50, 0x0ae52, 0x0ae80, 0x0aea3,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_sp_noncontext_pipe_br_sp_top_registers), 8));

/* Block: SP Cluster: noncontext Pipeline: PIPE_BR Location: uSPTP */
static const u32 gen7_0_0_sp_noncontext_pipe_br_usptp_registers[] = {
	0x0ae00, 0x0ae00, 0x0ae02, 0x0ae04, 0x0ae06, 0x0ae09, 0x0ae0c, 0x0ae0c,
	0x0ae0f, 0x0ae0f, 0x0ae30, 0x0ae32, 0x0ae35, 0x0ae35, 0x0ae3a, 0x0ae3b,
	0x0ae3e, 0x0ae3f, 0x0ae50, 0x0ae52,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_sp_noncontext_pipe_br_usptp_registers), 8));

/* Block: SP Cluster: noncontext Pipeline: PIPE_LPAC Location: HLSQ_State */
static const u32 gen7_0_0_sp_noncontext_pipe_lpac_hlsq_state_registers[] = {
	0x0af88, 0x0af8a,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_sp_noncontext_pipe_lpac_hlsq_state_registers), 8));

/* Block: SP Cluster: noncontext Pipeline: PIPE_LPAC Location: SP_TOP */
static const u32 gen7_0_0_sp_noncontext_pipe_lpac_sp_top_registers[] = {
	0x0af80, 0x0af84,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_sp_noncontext_pipe_lpac_sp_top_registers), 8));

/* Block: SP Cluster: noncontext Pipeline: PIPE_LPAC Location: uSPTP */
static const u32 gen7_0_0_sp_noncontext_pipe_lpac_usptp_registers[] = {
	0x0af80, 0x0af84, 0x0af90, 0x0af92,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_sp_noncontext_pipe_lpac_usptp_registers), 8));

/* Block: TPl1 Cluster: noncontext Pipeline: PIPE_BR */
static const u32 gen7_0_0_tpl1_noncontext_pipe_br_registers[] = {
	0x0b600, 0x0b600, 0x0b602, 0x0b602, 0x0b604, 0x0b604, 0x0b608, 0x0b60c,
	0x0b60f, 0x0b621, 0x0b630, 0x0b633,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_tpl1_noncontext_pipe_br_registers), 8));

/* Block: TPl1 Cluster: noncontext Pipeline: PIPE_LPAC */
static const u32 gen7_0_0_tpl1_noncontext_pipe_lpac_registers[] = {
	0x0b780, 0x0b780,
	UINT_MAX, UINT_MAX,
};
static_assert(IS_ALIGNED(sizeof(gen7_0_0_tpl1_noncontext_pipe_lpac_registers), 8));

struct gen7_cluster_registers {
	/* cluster_id: Cluster identifier */
	int cluster_id;
	/* pipe_id: Pipe Identifier */
	int pipe_id;
	/* context_id: one of STATE_ that identifies the context to dump */
	int context_id;
	/* regs: Pointer to an array of register pairs */
	const u32 *regs;
	/* sel: Pointer to a selector register to write before reading */
	const struct sel_reg *sel;
	/* offset: Internal variable to track the state of the crashdump */
	unsigned int offset;
};

static struct gen7_cluster_registers gen7_clusters[] = {
	{ CLUSTER_NONE, PIPE_BR, STATE_NON_CONTEXT,
		gen7_0_0_noncontext_pipe_br_registers, },
	{ CLUSTER_NONE, PIPE_BV, STATE_NON_CONTEXT,
		gen7_0_0_noncontext_pipe_bv_registers, },
	{ CLUSTER_NONE, PIPE_LPAC, STATE_NON_CONTEXT,
		gen7_0_0_noncontext_pipe_lpac_registers, },
	{ CLUSTER_NONE, PIPE_BR, STATE_NON_CONTEXT,
		gen7_0_0_noncontext_rb_rac_pipe_br_registers, &gen7_0_0_rb_rac_sel, },
	{ CLUSTER_NONE, PIPE_BR, STATE_NON_CONTEXT,
		gen7_0_0_noncontext_rb_rbp_pipe_br_registers, &gen7_0_0_rb_rbp_sel, },
	{ CLUSTER_GRAS, PIPE_BR, STATE_FORCE_CTXT_0,
		gen7_0_0_gras_cluster_gras_pipe_br_registers, },
	{ CLUSTER_GRAS, PIPE_BV, STATE_FORCE_CTXT_0,
		gen7_0_0_gras_cluster_gras_pipe_bv_registers, },
	{ CLUSTER_GRAS, PIPE_BR, STATE_FORCE_CTXT_1,
		gen7_0_0_gras_cluster_gras_pipe_br_registers, },
	{ CLUSTER_GRAS, PIPE_BV, STATE_FORCE_CTXT_1,
		gen7_0_0_gras_cluster_gras_pipe_bv_registers, },
	{ CLUSTER_FE, PIPE_BR, STATE_FORCE_CTXT_0,
		gen7_0_0_pc_cluster_fe_pipe_br_registers, },
	{ CLUSTER_FE, PIPE_BV, STATE_FORCE_CTXT_0,
		gen7_0_0_pc_cluster_fe_pipe_bv_registers, },
	{ CLUSTER_FE, PIPE_BR, STATE_FORCE_CTXT_1,
		gen7_0_0_pc_cluster_fe_pipe_br_registers, },
	{ CLUSTER_FE, PIPE_BV, STATE_FORCE_CTXT_1,
		gen7_0_0_pc_cluster_fe_pipe_bv_registers, },
	{ CLUSTER_PS, PIPE_BR, STATE_FORCE_CTXT_0,
		gen7_0_0_rb_rac_cluster_ps_pipe_br_registers, &gen7_0_0_rb_rac_sel, },
	{ CLUSTER_PS, PIPE_BR, STATE_FORCE_CTXT_1,
		gen7_0_0_rb_rac_cluster_ps_pipe_br_registers, &gen7_0_0_rb_rac_sel, },
	{ CLUSTER_PS, PIPE_BR, STATE_FORCE_CTXT_0,
		gen7_0_0_rb_rbp_cluster_ps_pipe_br_registers, &gen7_0_0_rb_rbp_sel, },
	{ CLUSTER_PS, PIPE_BR, STATE_FORCE_CTXT_1,
		gen7_0_0_rb_rbp_cluster_ps_pipe_br_registers, &gen7_0_0_rb_rbp_sel, },
	{ CLUSTER_FE, PIPE_BR, STATE_FORCE_CTXT_0,
		gen7_0_0_vfd_cluster_fe_pipe_br_registers, },
	{ CLUSTER_FE, PIPE_BV, STATE_FORCE_CTXT_0,
		gen7_0_0_vfd_cluster_fe_pipe_bv_registers, },
	{ CLUSTER_FE, PIPE_BR, STATE_FORCE_CTXT_1,
		gen7_0_0_vfd_cluster_fe_pipe_br_registers, },
	{ CLUSTER_FE, PIPE_BV, STATE_FORCE_CTXT_1,
		gen7_0_0_vfd_cluster_fe_pipe_bv_registers, },
	{ CLUSTER_FE, PIPE_BR, STATE_FORCE_CTXT_0,
		gen7_0_0_vpc_cluster_fe_pipe_br_registers, },
	{ CLUSTER_FE, PIPE_BV, STATE_FORCE_CTXT_0,
		gen7_0_0_vpc_cluster_fe_pipe_bv_registers, },
	{ CLUSTER_FE, PIPE_BR, STATE_FORCE_CTXT_1,
		gen7_0_0_vpc_cluster_fe_pipe_br_registers, },
	{ CLUSTER_FE, PIPE_BV, STATE_FORCE_CTXT_1,
		gen7_0_0_vpc_cluster_fe_pipe_bv_registers, },
	{ CLUSTER_PC_VS, PIPE_BR, STATE_FORCE_CTXT_0,
		gen7_0_0_vpc_cluster_pc_vs_pipe_br_registers, },
	{ CLUSTER_PC_VS, PIPE_BV, STATE_FORCE_CTXT_0,
		gen7_0_0_vpc_cluster_pc_vs_pipe_bv_registers, },
	{ CLUSTER_PC_VS, PIPE_BR, STATE_FORCE_CTXT_1,
		gen7_0_0_vpc_cluster_pc_vs_pipe_br_registers, },
	{ CLUSTER_PC_VS, PIPE_BV, STATE_FORCE_CTXT_1,
		gen7_0_0_vpc_cluster_pc_vs_pipe_bv_registers, },
	{ CLUSTER_VPC_PS, PIPE_BR, STATE_FORCE_CTXT_0,
		gen7_0_0_vpc_cluster_vpc_ps_pipe_br_registers, },
	{ CLUSTER_VPC_PS, PIPE_BV, STATE_FORCE_CTXT_0,
		gen7_0_0_vpc_cluster_vpc_ps_pipe_bv_registers, },
	{ CLUSTER_VPC_PS, PIPE_BR, STATE_FORCE_CTXT_1,
		gen7_0_0_vpc_cluster_vpc_ps_pipe_br_registers, },
	{ CLUSTER_VPC_PS, PIPE_BV, STATE_FORCE_CTXT_1,
		gen7_0_0_vpc_cluster_vpc_ps_pipe_bv_registers, },
};

struct gen7_sptp_cluster_registers {
	/* cluster_id: Cluster identifier */
	int cluster_id;
	/* cluster_id: SP block state type for the cluster */
	int statetype;
	/* pipe_id: Pipe identifier */
	int pipe_id;
	/* context_id: Context identifier */
	int context_id;
	/* location_id: Location identifier */
	int location_id;
	/* regs: Pointer to the list of register pairs to read */
	const u32 *regs;
	/* regbase: Dword offset of the register block in the GPu register space */
	unsigned int regbase;
	/* offset: Internal variable used to track the crashdump state */
	unsigned int offset;
};

static struct gen7_sptp_cluster_registers gen7_sptp_clusters[] = {
	{ CLUSTER_NONE, SP_NCTX_REG, PIPE_BR, 0, HLSQ_State,
		gen7_0_0_sp_noncontext_pipe_br_hlsq_state_registers, 0xae00 },
	{ CLUSTER_NONE, SP_NCTX_REG, PIPE_BR, 0, SP_TOP,
		gen7_0_0_sp_noncontext_pipe_br_sp_top_registers, 0xae00 },
	{ CLUSTER_NONE, SP_NCTX_REG, PIPE_BR, 0, USPTP,
		gen7_0_0_sp_noncontext_pipe_br_usptp_registers, 0xae00 },
	{ CLUSTER_NONE, SP_NCTX_REG, PIPE_LPAC, 0, HLSQ_State,
		gen7_0_0_sp_noncontext_pipe_lpac_hlsq_state_registers, 0xaf80 },
	{ CLUSTER_NONE, SP_NCTX_REG, PIPE_LPAC, 0, SP_TOP,
		gen7_0_0_sp_noncontext_pipe_lpac_sp_top_registers, 0xaf80 },
	{ CLUSTER_NONE, SP_NCTX_REG, PIPE_LPAC, 0, USPTP,
		gen7_0_0_sp_noncontext_pipe_lpac_usptp_registers, 0xaf80 },
	{ CLUSTER_NONE, TP0_NCTX_REG, PIPE_BR, 0, USPTP,
		gen7_0_0_tpl1_noncontext_pipe_br_registers, 0xb600 },
	{ CLUSTER_NONE, TP0_NCTX_REG, PIPE_LPAC, 0, USPTP,
		gen7_0_0_tpl1_noncontext_pipe_lpac_registers, 0xb780 },
	{ CLUSTER_SP_PS, SP_CTX0_3D_CPS_REG, PIPE_BR, 0, HLSQ_State,
		gen7_0_0_sp_cluster_sp_ps_pipe_br_hlsq_state_registers, 0xa800 },
	{ CLUSTER_SP_PS, SP_CTX0_3D_CPS_REG, PIPE_BR, 0, HLSQ_DP,
		gen7_0_0_sp_cluster_sp_ps_pipe_br_hlsq_dp_registers, 0xa800 },
	{ CLUSTER_SP_PS, SP_CTX0_3D_CPS_REG, PIPE_BR, 0, SP_TOP,
		gen7_0_0_sp_cluster_sp_ps_pipe_br_sp_top_registers, 0xa800 },
	{ CLUSTER_SP_PS, SP_CTX0_3D_CPS_REG, PIPE_BR, 0, USPTP,
		gen7_0_0_sp_cluster_sp_ps_pipe_br_usptp_registers, 0xa800 },
	{ CLUSTER_SP_PS, SP_CTX1_3D_CPS_REG, PIPE_BR, 1, HLSQ_State,
		gen7_0_0_sp_cluster_sp_ps_pipe_br_hlsq_state_registers, 0xa800 },
	{ CLUSTER_SP_PS, SP_CTX1_3D_CPS_REG, PIPE_BR, 1, HLSQ_DP,
		gen7_0_0_sp_cluster_sp_ps_pipe_br_hlsq_dp_registers, 0xa800 },
	{ CLUSTER_SP_PS, SP_CTX1_3D_CPS_REG, PIPE_BR, 1, SP_TOP,
		gen7_0_0_sp_cluster_sp_ps_pipe_br_sp_top_registers, 0xa800 },
	{ CLUSTER_SP_PS, SP_CTX1_3D_CPS_REG, PIPE_BR, 1, USPTP,
		gen7_0_0_sp_cluster_sp_ps_pipe_br_usptp_registers, 0xa800 },
	{ CLUSTER_SP_PS, SP_CTX2_3D_CPS_REG, PIPE_BR, 2, HLSQ_State,
		gen7_0_0_sp_cluster_sp_ps_pipe_br_hlsq_state_registers, 0xa800 },
	{ CLUSTER_SP_PS, SP_CTX2_3D_CPS_REG, PIPE_BR, 2, HLSQ_DP,
		gen7_0_0_sp_cluster_sp_ps_pipe_br_hlsq_dp_registers, 0xa800 },
	{ CLUSTER_SP_PS, SP_CTX2_3D_CPS_REG, PIPE_BR, 2, SP_TOP,
		gen7_0_0_sp_cluster_sp_ps_pipe_br_sp_top_registers, 0xa800 },
	{ CLUSTER_SP_PS, SP_CTX2_3D_CPS_REG, PIPE_BR, 2, USPTP,
		gen7_0_0_sp_cluster_sp_ps_pipe_br_usptp_registers, 0xa800 },
	{ CLUSTER_SP_PS, SP_CTX3_3D_CPS_REG, PIPE_BR, 3, HLSQ_State,
		gen7_0_0_sp_cluster_sp_ps_pipe_br_hlsq_state_registers, 0xa800 },
	{ CLUSTER_SP_PS, SP_CTX3_3D_CPS_REG, PIPE_BR, 3, HLSQ_DP,
		gen7_0_0_sp_cluster_sp_ps_pipe_br_hlsq_dp_registers, 0xa800 },
	{ CLUSTER_SP_PS, SP_CTX3_3D_CPS_REG, PIPE_BR, 3, SP_TOP,
		gen7_0_0_sp_cluster_sp_ps_pipe_br_sp_top_registers, 0xa800 },
	{ CLUSTER_SP_PS, SP_CTX3_3D_CPS_REG, PIPE_BR, 3, USPTP,
		gen7_0_0_sp_cluster_sp_ps_pipe_br_usptp_registers, 0xa800 },
	{ CLUSTER_SP_PS, SP_CTX0_3D_CPS_REG, PIPE_LPAC, 0, HLSQ_State,
		gen7_0_0_sp_cluster_sp_ps_pipe_lpac_hlsq_state_registers, 0xa800 },
	{ CLUSTER_SP_PS, SP_CTX0_3D_CPS_REG, PIPE_LPAC, 0, HLSQ_DP,
		gen7_0_0_sp_cluster_sp_ps_pipe_lpac_hlsq_dp_registers, 0xa800 },
	{ CLUSTER_SP_PS, SP_CTX0_3D_CPS_REG, PIPE_LPAC, 0, SP_TOP,
		gen7_0_0_sp_cluster_sp_ps_pipe_lpac_sp_top_registers, 0xa800 },
	{ CLUSTER_SP_PS, SP_CTX0_3D_CPS_REG, PIPE_LPAC, 0, USPTP,
		gen7_0_0_sp_cluster_sp_ps_pipe_lpac_usptp_registers, 0xa800 },
	{ CLUSTER_SP_VS, SP_CTX0_3D_CVS_REG, PIPE_BR, 0, HLSQ_State,
		gen7_0_0_sp_cluster_sp_vs_pipe_br_hlsq_state_registers, 0xa800 },
	{ CLUSTER_SP_VS, SP_CTX0_3D_CVS_REG, PIPE_BV, 0, HLSQ_State,
		gen7_0_0_sp_cluster_sp_vs_pipe_bv_hlsq_state_registers, 0xa800 },
	{ CLUSTER_SP_VS, SP_CTX0_3D_CVS_REG, PIPE_BR, 0, SP_TOP,
		gen7_0_0_sp_cluster_sp_vs_pipe_br_sp_top_registers, 0xa800 },
	{ CLUSTER_SP_VS, SP_CTX0_3D_CVS_REG, PIPE_BV, 0, SP_TOP,
		gen7_0_0_sp_cluster_sp_vs_pipe_bv_sp_top_registers, 0xa800 },
	{ CLUSTER_SP_VS, SP_CTX0_3D_CVS_REG, PIPE_BR, 0, USPTP,
		gen7_0_0_sp_cluster_sp_vs_pipe_br_usptp_registers, 0xa800 },
	{ CLUSTER_SP_VS, SP_CTX0_3D_CVS_REG, PIPE_BV, 0, USPTP,
		gen7_0_0_sp_cluster_sp_vs_pipe_bv_usptp_registers, 0xa800 },
	{ CLUSTER_SP_VS, SP_CTX1_3D_CVS_REG, PIPE_BR, 1, HLSQ_State,
		gen7_0_0_sp_cluster_sp_vs_pipe_br_hlsq_state_registers, 0xa800 },
	{ CLUSTER_SP_VS, SP_CTX1_3D_CVS_REG, PIPE_BV, 1, HLSQ_State,
		gen7_0_0_sp_cluster_sp_vs_pipe_bv_hlsq_state_registers, 0xa800 },
	{ CLUSTER_SP_VS, SP_CTX1_3D_CVS_REG, PIPE_BR, 1, SP_TOP,
		gen7_0_0_sp_cluster_sp_vs_pipe_br_sp_top_registers, 0xa800 },
	{ CLUSTER_SP_VS, SP_CTX1_3D_CVS_REG, PIPE_BV, 1, SP_TOP,
		gen7_0_0_sp_cluster_sp_vs_pipe_bv_sp_top_registers, 0xa800 },
	{ CLUSTER_SP_VS, SP_CTX1_3D_CVS_REG, PIPE_BR, 1, USPTP,
		gen7_0_0_sp_cluster_sp_vs_pipe_br_usptp_registers, 0xa800 },
	{ CLUSTER_SP_VS, SP_CTX1_3D_CVS_REG, PIPE_BV, 1, USPTP,
		gen7_0_0_sp_cluster_sp_vs_pipe_bv_usptp_registers, 0xa800 },
	{ CLUSTER_SP_PS, TP0_CTX0_3D_CPS_REG, PIPE_BR, 0, USPTP,
		gen7_0_0_tpl1_cluster_sp_ps_pipe_br_registers, 0xb000 },
	{ CLUSTER_SP_PS, TP0_CTX1_3D_CPS_REG, PIPE_BR, 1, USPTP,
		gen7_0_0_tpl1_cluster_sp_ps_pipe_br_registers, 0xb000 },
	{ CLUSTER_SP_PS, TP0_CTX2_3D_CPS_REG, PIPE_BR, 2, USPTP,
		gen7_0_0_tpl1_cluster_sp_ps_pipe_br_registers, 0xb000 },
	{ CLUSTER_SP_PS, TP0_CTX3_3D_CPS_REG, PIPE_BR, 3, USPTP,
		gen7_0_0_tpl1_cluster_sp_ps_pipe_br_registers, 0xb000 },
	{ CLUSTER_SP_PS, TP0_CTX0_3D_CPS_REG, PIPE_LPAC, 0, USPTP,
		gen7_0_0_tpl1_cluster_sp_ps_pipe_lpac_registers, 0xb000 },
	{ CLUSTER_SP_VS, TP0_CTX0_3D_CVS_REG, PIPE_BR, 0, USPTP,
		gen7_0_0_tpl1_cluster_sp_vs_pipe_br_registers, 0xb000 },
	{ CLUSTER_SP_VS, TP0_CTX0_3D_CVS_REG, PIPE_BV, 0, USPTP,
		gen7_0_0_tpl1_cluster_sp_vs_pipe_bv_registers, 0xb000 },
	{ CLUSTER_SP_VS, TP0_CTX1_3D_CVS_REG, PIPE_BR, 1, USPTP,
		gen7_0_0_tpl1_cluster_sp_vs_pipe_br_registers, 0xb000 },
	{ CLUSTER_SP_VS, TP0_CTX1_3D_CVS_REG, PIPE_BV, 1, USPTP,
		gen7_0_0_tpl1_cluster_sp_vs_pipe_bv_registers, 0xb000 },
};

struct gen7_shader_block {
	/* statetype: Type identifer for the block */
	u32 statetype;
	/* size: Size of the block (in dwords) */
	u32 size
	/* sp_id: The SP id to dump */;
	u32 sp_id;
	/* usptp: The usptp id to dump */;
	u32 usptp;
	/* pipe_id: Pipe identifier for the block data  */
	u32 pipeid;
	/* location: Location identifer for the block data */
	u32 location;
	/* offset: The offset in the snasphot dump */
	u64 offset;
};

static struct gen7_shader_block gen7_shader_blocks[] = {
	{TP0_TMO_DATA,               0x200, 0, 0, PIPE_BR, USPTP},
	{TP0_SMO_DATA,               0x80, 0, 0, PIPE_BR, USPTP},
	{TP0_MIPMAP_BASE_DATA,       0x3c0, 0, 0, PIPE_BR, USPTP},
	{SP_INST_DATA,               0x800, 0, 0, PIPE_BR, USPTP},
	{SP_INST_DATA_1,             0x800, 0, 0, PIPE_BR, USPTP},
	{SP_LB_0_DATA,               0x800, 0, 0, PIPE_BR, USPTP},
	{SP_LB_1_DATA,               0x800, 0, 0, PIPE_BR, USPTP},
	{SP_LB_2_DATA,               0x800, 0, 0, PIPE_BR, USPTP},
	{SP_LB_3_DATA,               0x800, 0, 0, PIPE_BR, USPTP},
	{SP_LB_4_DATA,               0x800, 0, 0, PIPE_BR, USPTP},
	{SP_LB_5_DATA,               0x800, 0, 0, PIPE_BR, USPTP},
	{SP_LB_6_DATA,               0x800, 0, 0, PIPE_BR, USPTP},
	{SP_LB_7_DATA,               0x800, 0, 0, PIPE_BR, USPTP},
	{SP_CB_RAM,                  0x390, 0, 0, PIPE_BR, USPTP,},
	{SP_INST_TAG,                0x90, 0, 0, PIPE_BR, USPTP},
	{SP_INST_DATA_2,             0x200, 0, 0, PIPE_BR, USPTP},
	{SP_TMO_TAG,                 0x80, 0, 0, PIPE_BR, USPTP},
	{SP_SMO_TAG,                 0x80, 0, 0, PIPE_BR, USPTP},
	{SP_STATE_DATA,              0x40, 0, 0, PIPE_BR, USPTP},
	{SP_HWAVE_RAM,               0x100, 0, 0, PIPE_BR, USPTP},
	{SP_L0_INST_BUF,             0x50, 0, 0, PIPE_BR, USPTP},
	{SP_LB_8_DATA,               0x800, 0, 0, PIPE_BR, USPTP},
	{SP_LB_9_DATA,               0x800, 0, 0, PIPE_BR, USPTP},
	{SP_LB_10_DATA,              0x800, 0, 0, PIPE_BR, USPTP},
	{SP_LB_11_DATA,              0x800, 0, 0, PIPE_BR, USPTP},
	{SP_LB_12_DATA,              0x200, 0, 0, PIPE_BR, USPTP},
	{HLSQ_CVS_BE_CTXT_BUF_RAM_TAG,    0x10, 0, 0, PIPE_BV, HLSQ_State},
	{HLSQ_CVS_BE_CTXT_BUF_RAM_TAG,    0x10, 0, 0, PIPE_BR, HLSQ_State},
	{HLSQ_CPS_BE_CTXT_BUF_RAM_TAG,    0x10, 0, 0, PIPE_BR, HLSQ_State},
	{HLSQ_GFX_CVS_BE_CTXT_BUF_RAM,        0x300, 0, 0, PIPE_BR, HLSQ_State},
	{HLSQ_GFX_CVS_BE_CTXT_BUF_RAM,        0x300, 0, 0, PIPE_BV, HLSQ_State},
	{HLSQ_GFX_CPS_BE_CTXT_BUF_RAM,        0x300, 0, 0, PIPE_BR, HLSQ_State},
	{HLSQ_CHUNK_CVS_RAM,         0x1c0, 0, 0, PIPE_BV, HLSQ_State},
	{HLSQ_CHUNK_CVS_RAM,         0x1c0, 0, 0, PIPE_BR, HLSQ_State},
	{HLSQ_CHUNK_CPS_RAM,         0x300, 0, 0, PIPE_BR, HLSQ_State},
	{HLSQ_CHUNK_CPS_RAM,         0x300, 0, 0, PIPE_LPAC, HLSQ_State},
	{HLSQ_CHUNK_CVS_RAM_TAG,     0x40, 0, 0, PIPE_BR, HLSQ_State},
	{HLSQ_CHUNK_CVS_RAM_TAG,     0x40, 0, 0, PIPE_BV, HLSQ_State},
	{HLSQ_CHUNK_CPS_RAM_TAG,     0x40, 0, 0, PIPE_BR, HLSQ_State},
	{HLSQ_CHUNK_CPS_RAM_TAG,     0x40, 0, 0, PIPE_LPAC, HLSQ_State},
	{HLSQ_ICB_CVS_CB_BASE_TAG,   0x10, 0, 0, PIPE_BR, HLSQ_State},
	{HLSQ_ICB_CVS_CB_BASE_TAG,   0x10, 0, 0, PIPE_BV, HLSQ_State},
	{HLSQ_ICB_CPS_CB_BASE_TAG,   0x10, 0, 0, PIPE_BR, HLSQ_State},
	{HLSQ_ICB_CPS_CB_BASE_TAG,   0x10, 0, 0, PIPE_LPAC, HLSQ_State},
	{HLSQ_CVS_MISC_RAM,          0x280, 0, 0, PIPE_BR, HLSQ_State},
	{HLSQ_CVS_MISC_RAM,          0x280, 0, 0, PIPE_BV, HLSQ_State},
	{HLSQ_CPS_MISC_RAM,          0x800, 0, 0, PIPE_BR, HLSQ_State},
	{HLSQ_CPS_MISC_RAM,          0x800, 0, 0, PIPE_LPAC, HLSQ_State},
	{HLSQ_CPS_MISC_RAM_1,        0x200, 0, 0, PIPE_BR, HLSQ_State},
	{HLSQ_INST_RAM,              0x800, 0, 0, PIPE_BR, HLSQ_State},
	{HLSQ_INST_RAM,              0x800, 0, 0, PIPE_BV, HLSQ_State},
	{HLSQ_INST_RAM,              0x800, 0, 0, PIPE_LPAC, HLSQ_State},
	{HLSQ_GFX_CVS_CONST_RAM,     0x800, 0, 0, PIPE_BR, HLSQ_State},
	{HLSQ_GFX_CVS_CONST_RAM,     0x800, 0, 0, PIPE_BV, HLSQ_State},
	{HLSQ_GFX_CPS_CONST_RAM,     0x800, 0, 0, PIPE_BR, HLSQ_State},
	{HLSQ_GFX_CPS_CONST_RAM,     0x800, 0, 0, PIPE_LPAC, HLSQ_State},
	{HLSQ_CVS_MISC_RAM_TAG,      0x10, 0, 0, PIPE_BR, HLSQ_State},
	{HLSQ_CVS_MISC_RAM_TAG,      0x10, 0, 0, PIPE_BV, HLSQ_State},
	{HLSQ_CPS_MISC_RAM_TAG,      0x10, 0, 0, PIPE_BR, HLSQ_State},
	{HLSQ_CPS_MISC_RAM_TAG,      0x10, 0, 0, PIPE_LPAC, HLSQ_State},
	{HLSQ_INST_RAM_TAG,          0x80, 0, 0, PIPE_BR, HLSQ_State},
	{HLSQ_INST_RAM_TAG,          0x80, 0, 0, PIPE_BV, HLSQ_State},
	{HLSQ_INST_RAM_TAG,          0x80, 0, 0, PIPE_LPAC, HLSQ_State},
	{HLSQ_GFX_CVS_CONST_RAM_TAG, 0x64, 0, 0, PIPE_BR, HLSQ_State},
	{HLSQ_GFX_CVS_CONST_RAM_TAG, 0x64, 0, 0, PIPE_BV, HLSQ_State},
	{HLSQ_GFX_CPS_CONST_RAM_TAG, 0x64, 0, 0, PIPE_BR, HLSQ_State},
	{HLSQ_GFX_CPS_CONST_RAM_TAG, 0x64, 0, 0, PIPE_LPAC, HLSQ_State},
	{HLSQ_INST_RAM_1,            0x800, 0, 0, PIPE_BR, HLSQ_State},
	{HLSQ_STPROC_META,           0x10, 0, 0, PIPE_BR, HLSQ_State},
	{HLSQ_BV_BE_META,            0x10, 0, 0, PIPE_BR, HLSQ_State},
	{HLSQ_BV_BE_META,            0x10, 0, 0, PIPE_BV, HLSQ_State},
	{HLSQ_DATAPATH_META,         0x20, 0, 0, PIPE_BR, HLSQ_State},
	{HLSQ_FRONTEND_META,         0x40, 0, 0, PIPE_BR, HLSQ_State},
	{HLSQ_FRONTEND_META,         0x40, 0, 0, PIPE_BV, HLSQ_State},
	{HLSQ_FRONTEND_META,         0x40, 0, 0, PIPE_LPAC, HLSQ_State},
	{HLSQ_INDIRECT_META,         0x10, 0, 0, PIPE_BR, HLSQ_State},
	{HLSQ_BACKEND_META,          0x40, 0, 0, PIPE_BR, HLSQ_State},
	{HLSQ_BACKEND_META,          0x40, 0, 0, PIPE_BV, HLSQ_State},
	{HLSQ_BACKEND_META,          0x40, 0, 0, PIPE_LPAC, HLSQ_State},
	/* SP 0 USPTP 1 */
	{TP0_TMO_DATA,               0x200, 0, 1, PIPE_BR, USPTP},
	{TP0_SMO_DATA,               0x80, 0, 1, PIPE_BR, USPTP},
	{TP0_MIPMAP_BASE_DATA,       0x3c0, 0, 1, PIPE_BR, USPTP},
	{SP_INST_DATA,               0x800, 0, 1, PIPE_BR, USPTP},
	{SP_INST_DATA_1,             0x800, 0, 1, PIPE_BR, USPTP},
	{SP_LB_0_DATA,               0x800, 0, 1, PIPE_BR, USPTP},
	{SP_LB_1_DATA,               0x800, 0, 1, PIPE_BR, USPTP},
	{SP_LB_2_DATA,               0x800, 0, 1, PIPE_BR, USPTP},
	{SP_LB_3_DATA,               0x800, 0, 1, PIPE_BR, USPTP},
	{SP_LB_4_DATA,               0x800, 0, 1, PIPE_BR, USPTP},
	{SP_LB_5_DATA,               0x800, 0, 1, PIPE_BR, USPTP},
	{SP_LB_6_DATA,               0x800, 0, 1, PIPE_BR, USPTP},
	{SP_LB_7_DATA,               0x800, 0, 1, PIPE_BR, USPTP},
	{SP_CB_RAM,                  0x390, 0, 1, PIPE_BR, USPTP,},
	{SP_INST_TAG,                0x90, 0, 1, PIPE_BR, USPTP},
	{SP_INST_DATA_2,             0x200, 0, 1, PIPE_BR, USPTP},
	{SP_TMO_TAG,                 0x80, 0, 1, PIPE_BR, USPTP},
	{SP_SMO_TAG,                 0x80, 0, 1, PIPE_BR, USPTP},
	{SP_STATE_DATA,              0x40, 0, 1, PIPE_BR, USPTP},
	{SP_HWAVE_RAM,               0x100, 0, 1, PIPE_BR, USPTP},
	{SP_L0_INST_BUF,             0x50, 0, 1, PIPE_BR, USPTP},
	{SP_LB_8_DATA,               0x800, 0, 1, PIPE_BR, USPTP},
	{SP_LB_9_DATA,               0x800, 0, 1, PIPE_BR, USPTP},
	{SP_LB_10_DATA,              0x800, 0, 1, PIPE_BR, USPTP},
	{SP_LB_11_DATA,              0x800, 0, 1, PIPE_BR, USPTP},
	{SP_LB_12_DATA,              0x200, 0, 1, PIPE_BR, USPTP},
	/* SP 1 USPTP 0 */
	{TP0_TMO_DATA,               0x200, 1, 0, PIPE_BR, USPTP},
	{TP0_SMO_DATA,               0x80, 1, 0, PIPE_BR, USPTP},
	{TP0_MIPMAP_BASE_DATA,       0x3c0, 1, 0, PIPE_BR, USPTP},
	{SP_INST_DATA,               0x800, 1, 0, PIPE_BR, USPTP},
	{SP_INST_DATA_1,             0x800, 1, 0, PIPE_BR, USPTP},
	{SP_LB_0_DATA,               0x800, 1, 0, PIPE_BR, USPTP},
	{SP_LB_1_DATA,               0x800, 1, 0, PIPE_BR, USPTP},
	{SP_LB_2_DATA,               0x800, 1, 0, PIPE_BR, USPTP},
	{SP_LB_3_DATA,               0x800, 1, 0, PIPE_BR, USPTP},
	{SP_LB_4_DATA,               0x800, 1, 0, PIPE_BR, USPTP},
	{SP_LB_5_DATA,               0x800, 1, 0, PIPE_BR, USPTP},
	{SP_LB_6_DATA,               0x800, 1, 0, PIPE_BR, USPTP},
	{SP_LB_7_DATA,               0x800, 1, 0, PIPE_BR, USPTP},
	{SP_CB_RAM,                  0x390, 1, 0, PIPE_BR, USPTP,},
	{SP_INST_TAG,                0x90, 1, 0, PIPE_BR, USPTP,},
	{SP_INST_DATA_2,             0x200, 1, 0, PIPE_BR, USPTP,},
	{SP_TMO_TAG,                 0x80, 1, 0, PIPE_BR, USPTP,},
	{SP_SMO_TAG,                 0x80, 1, 0, PIPE_BR, USPTP,},
	{SP_STATE_DATA,              0x40, 1, 0, PIPE_BR, USPTP,},
	{SP_HWAVE_RAM,               0x100, 1, 0, PIPE_BR, USPTP},
	{SP_L0_INST_BUF,             0x50, 1, 0, PIPE_BR, USPTP,},
	{SP_LB_8_DATA,               0x800, 1, 0, PIPE_BR, USPTP},
	{SP_LB_9_DATA,               0x800, 1, 0, PIPE_BR, USPTP},
	{SP_LB_10_DATA,              0x800, 1, 0, PIPE_BR, USPTP},
	{SP_LB_11_DATA,              0x800, 1, 0, PIPE_BR, USPTP},
	{SP_LB_12_DATA,              0x200, 1, 0, PIPE_BR, USPTP},
	/* SP 1 USPTP 1 */
	{TP0_TMO_DATA,               0x200, 1, 1, PIPE_BR, USPTP},
	{TP0_SMO_DATA,               0x80, 1, 1, PIPE_BR, USPTP},
	{TP0_MIPMAP_BASE_DATA,       0x3c0, 1, 1, PIPE_BR, USPTP},
	{SP_INST_DATA,               0x800, 1, 1, PIPE_BR, USPTP},
	{SP_INST_DATA_1,             0x800, 1, 1, PIPE_BR, USPTP},
	{SP_LB_0_DATA,               0x800, 1, 1, PIPE_BR, USPTP},
	{SP_LB_1_DATA,               0x800, 1, 1, PIPE_BR, USPTP},
	{SP_LB_2_DATA,               0x800, 1, 1, PIPE_BR, USPTP},
	{SP_LB_3_DATA,               0x800, 1, 1, PIPE_BR, USPTP},
	{SP_LB_4_DATA,               0x800, 1, 1, PIPE_BR, USPTP},
	{SP_LB_5_DATA,               0x800, 1, 1, PIPE_BR, USPTP},
	{SP_LB_6_DATA,               0x800, 1, 1, PIPE_BR, USPTP},
	{SP_LB_7_DATA,               0x800, 1, 1, PIPE_BR, USPTP},
	{SP_CB_RAM,                  0x390, 1, 1, PIPE_BR, USPTP,},
	{SP_INST_TAG,                0x90, 1, 1, PIPE_BR, USPTP,},
	{SP_INST_DATA_2,             0x200, 1, 1, PIPE_BR, USPTP,},
	{SP_TMO_TAG,                 0x80, 1, 1, PIPE_BR, USPTP,},
	{SP_SMO_TAG,                 0x80, 1, 1, PIPE_BR, USPTP,},
	{SP_STATE_DATA,              0x40, 1, 1, PIPE_BR, USPTP,},
	{SP_HWAVE_RAM,               0x100, 1, 1, PIPE_BR, USPTP},
	{SP_L0_INST_BUF,             0x50, 1, 1, PIPE_BR, USPTP,},
	{SP_LB_8_DATA,               0x800, 1, 1, PIPE_BR, USPTP},
	{SP_LB_9_DATA,               0x800, 1, 1, PIPE_BR, USPTP},
	{SP_LB_10_DATA,              0x800, 1, 1, PIPE_BR, USPTP},
	{SP_LB_11_DATA,              0x800, 1, 1, PIPE_BR, USPTP},
	{SP_LB_12_DATA,              0x200, 1, 1, PIPE_BR, USPTP},
	/* SP 2 USPTP 0 */
	{TP0_TMO_DATA,               0x200, 2, 0, PIPE_BR, USPTP},
	{TP0_SMO_DATA,               0x80, 2, 0, PIPE_BR, USPTP},
	{TP0_MIPMAP_BASE_DATA,       0x3c0, 2, 0, PIPE_BR, USPTP},
	{SP_INST_DATA,               0x800, 2, 0, PIPE_BR, USPTP},
	{SP_INST_DATA_1,             0x800, 2, 0, PIPE_BR, USPTP},
	{SP_LB_0_DATA,               0x800, 2, 0, PIPE_BR, USPTP},
	{SP_LB_1_DATA,               0x800, 2, 0, PIPE_BR, USPTP},
	{SP_LB_2_DATA,               0x800, 2, 0, PIPE_BR, USPTP},
	{SP_LB_3_DATA,               0x800, 2, 0, PIPE_BR, USPTP},
	{SP_LB_4_DATA,               0x800, 2, 0, PIPE_BR, USPTP},
	{SP_LB_5_DATA,               0x800, 2, 0, PIPE_BR, USPTP},
	{SP_LB_6_DATA,               0x800, 2, 0, PIPE_BR, USPTP},
	{SP_LB_7_DATA,               0x800, 2, 0, PIPE_BR, USPTP},
	{SP_CB_RAM,                  0x390, 2, 0, PIPE_BR, USPTP,},
	{SP_INST_TAG,                0x90, 2, 0, PIPE_BR, USPTP,},
	{SP_INST_DATA_2,             0x200, 2, 0, PIPE_BR, USPTP,},
	{SP_TMO_TAG,                 0x80, 2, 0, PIPE_BR, USPTP,},
	{SP_SMO_TAG,                 0x80, 2, 0, PIPE_BR, USPTP,},
	{SP_STATE_DATA,              0x40, 2, 0, PIPE_BR, USPTP,},
	{SP_HWAVE_RAM,               0x100, 2, 0, PIPE_BR, USPTP},
	{SP_L0_INST_BUF,             0x50, 2, 0, PIPE_BR, USPTP,},
	{SP_LB_8_DATA,               0x800, 2, 0, PIPE_BR, USPTP},
	{SP_LB_9_DATA,               0x800, 2, 0, PIPE_BR, USPTP},
	{SP_LB_10_DATA,              0x800, 2, 0, PIPE_BR, USPTP},
	{SP_LB_11_DATA,              0x800, 2, 0, PIPE_BR, USPTP},
	{SP_LB_12_DATA,              0x200, 2, 0, PIPE_BR, USPTP},
	/* SP 2 USPTP 1 */
	{TP0_TMO_DATA,               0x200, 2, 1, PIPE_BR, USPTP},
	{TP0_SMO_DATA,               0x80, 2, 1, PIPE_BR, USPTP},
	{TP0_MIPMAP_BASE_DATA,       0x3c0, 2, 1, PIPE_BR, USPTP},
	{SP_INST_DATA,               0x800, 2, 1, PIPE_BR, USPTP},
	{SP_INST_DATA_1,             0x800, 2, 1, PIPE_BR, USPTP},
	{SP_LB_0_DATA,               0x800, 2, 1, PIPE_BR, USPTP},
	{SP_LB_1_DATA,               0x800, 2, 1, PIPE_BR, USPTP},
	{SP_LB_2_DATA,               0x800, 2, 1, PIPE_BR, USPTP},
	{SP_LB_3_DATA,               0x800, 2, 1, PIPE_BR, USPTP},
	{SP_LB_4_DATA,               0x800, 2, 1, PIPE_BR, USPTP},
	{SP_LB_5_DATA,               0x800, 2, 1, PIPE_BR, USPTP},
	{SP_LB_6_DATA,               0x800, 2, 1, PIPE_BR, USPTP},
	{SP_LB_7_DATA,               0x800, 2, 1, PIPE_BR, USPTP},
	{SP_CB_RAM,                  0x390, 2, 1, PIPE_BR, USPTP,},
	{SP_INST_TAG,                0x90, 2, 1, PIPE_BR, USPTP,},
	{SP_INST_DATA_2,             0x200, 2, 1, PIPE_BR, USPTP,},
	{SP_TMO_TAG,                 0x80, 2, 1, PIPE_BR, USPTP,},
	{SP_SMO_TAG,                 0x80, 2, 1, PIPE_BR, USPTP,},
	{SP_STATE_DATA,              0x40, 2, 1, PIPE_BR, USPTP,},
	{SP_HWAVE_RAM,               0x100, 2, 1, PIPE_BR, USPTP},
	{SP_L0_INST_BUF,             0x50, 2, 1, PIPE_BR, USPTP,},
	{SP_LB_8_DATA,               0x800, 2, 1, PIPE_BR, USPTP},
	{SP_LB_9_DATA,               0x800, 2, 1, PIPE_BR, USPTP},
	{SP_LB_10_DATA,              0x800, 2, 1, PIPE_BR, USPTP},
	{SP_LB_11_DATA,              0x800, 2, 1, PIPE_BR, USPTP},
	{SP_LB_12_DATA,              0x200, 2, 1, PIPE_BR, USPTP},
	/* SP 3 USPTP 0 */
	{TP0_TMO_DATA,               0x200, 3, 0, PIPE_BR, USPTP},
	{TP0_SMO_DATA,               0x80, 3, 0, PIPE_BR, USPTP},
	{TP0_MIPMAP_BASE_DATA,       0x3c0, 3, 0, PIPE_BR, USPTP},
	{SP_INST_DATA,               0x800, 3, 0, PIPE_BR, USPTP},
	{SP_INST_DATA_1,             0x800, 3, 0, PIPE_BR, USPTP},
	{SP_LB_0_DATA,               0x800, 3, 0, PIPE_BR, USPTP},
	{SP_LB_1_DATA,               0x800, 3, 0, PIPE_BR, USPTP},
	{SP_LB_2_DATA,               0x800, 3, 0, PIPE_BR, USPTP},
	{SP_LB_3_DATA,               0x800, 3, 0, PIPE_BR, USPTP},
	{SP_LB_4_DATA,               0x800, 3, 0, PIPE_BR, USPTP},
	{SP_LB_5_DATA,               0x800, 3, 0, PIPE_BR, USPTP},
	{SP_LB_6_DATA,               0x800, 3, 0, PIPE_BR, USPTP},
	{SP_LB_7_DATA,               0x800, 3, 0, PIPE_BR, USPTP},
	{SP_CB_RAM,                  0x390, 3, 0, PIPE_BR, USPTP,},
	{SP_INST_TAG,                0x90, 3, 0, PIPE_BR, USPTP,},
	{SP_INST_DATA_2,             0x200, 3, 0, PIPE_BR, USPTP,},
	{SP_TMO_TAG,                 0x80, 3, 0, PIPE_BR, USPTP,},
	{SP_SMO_TAG,                 0x80, 3, 0, PIPE_BR, USPTP,},
	{SP_STATE_DATA,              0x40, 3, 0, PIPE_BR, USPTP,},
	{SP_HWAVE_RAM,               0x100, 3, 0, PIPE_BR, USPTP},
	{SP_L0_INST_BUF,             0x50, 3, 0, PIPE_BR, USPTP,},
	{SP_LB_8_DATA,               0x800, 3, 0, PIPE_BR, USPTP},
	{SP_LB_9_DATA,               0x800, 3, 0, PIPE_BR, USPTP},
	{SP_LB_10_DATA,              0x800, 3, 0, PIPE_BR, USPTP},
	{SP_LB_11_DATA,              0x800, 3, 0, PIPE_BR, USPTP},
	{SP_LB_12_DATA,              0x200, 3, 0, PIPE_BR, USPTP},
	/* SP 3 USPTP 1 */
	{TP0_TMO_DATA,               0x200, 3, 1, PIPE_BR, USPTP},
	{TP0_SMO_DATA,               0x80, 3, 1, PIPE_BR, USPTP},
	{TP0_MIPMAP_BASE_DATA,       0x3c0, 3, 1, PIPE_BR, USPTP},
	{SP_INST_DATA,               0x800, 3, 1, PIPE_BR, USPTP},
	{SP_INST_DATA_1,             0x800, 3, 1, PIPE_BR, USPTP},
	{SP_LB_0_DATA,               0x800, 3, 1, PIPE_BR, USPTP},
	{SP_LB_1_DATA,               0x800, 3, 1, PIPE_BR, USPTP},
	{SP_LB_2_DATA,               0x800, 3, 1, PIPE_BR, USPTP},
	{SP_LB_3_DATA,               0x800, 3, 1, PIPE_BR, USPTP},
	{SP_LB_4_DATA,               0x800, 3, 1, PIPE_BR, USPTP},
	{SP_LB_5_DATA,               0x800, 3, 1, PIPE_BR, USPTP},
	{SP_LB_6_DATA,               0x800, 3, 1, PIPE_BR, USPTP},
	{SP_LB_7_DATA,               0x800, 3, 1, PIPE_BR, USPTP},
	{SP_CB_RAM,                  0x390, 3, 1, PIPE_BR, USPTP,},
	{SP_INST_TAG,                0x90, 3, 1, PIPE_BR, USPTP,},
	{SP_INST_DATA_2,             0x200, 3, 1, PIPE_BR, USPTP,},
	{SP_TMO_TAG,                 0x80, 3, 1, PIPE_BR, USPTP,},
	{SP_SMO_TAG,                 0x80, 3, 1, PIPE_BR, USPTP,},
	{SP_STATE_DATA,              0x40, 3, 1, PIPE_BR, USPTP,},
	{SP_HWAVE_RAM,               0x100, 3, 1, PIPE_BR, USPTP},
	{SP_L0_INST_BUF,             0x50, 3, 1, PIPE_BR, USPTP,},
	{SP_LB_8_DATA,               0x800, 3, 1, PIPE_BR, USPTP},
	{SP_LB_9_DATA,               0x800, 3, 1, PIPE_BR, USPTP},
	{SP_LB_10_DATA,              0x800, 3, 1, PIPE_BR, USPTP},
	{SP_LB_11_DATA,              0x800, 3, 1, PIPE_BR, USPTP},
	{SP_LB_12_DATA,              0x200, 3, 1, PIPE_BR, USPTP},
};

static const u32 gen7_gbif_debugbus_blocks[] = {
	DEBUGBUS_GBIF_CX,
	DEBUGBUS_GBIF_GX,
};

static const u32 gen7_cx_dbgc_debugbus_blocks[] = {
	DEBUGBUS_GMU_CX,
	DEBUGBUS_CX,
};

struct gen7_shader_block_info {
	struct gen7_shader_block *block;
	u32 bank;
	u64 offset;
};

static struct reg_list {
	const u32 *regs;
	const struct sel_reg *sel;
	u64 offset;
} gen7_reg_list[] = {
	{ gen7_gpu_registers, NULL },
	{ gen7_cx_misc_registers, NULL },
	{ gen7_dpm_registers, NULL },
};

static struct cp_indexed_reg_list {
	u32 addr;
	u32 data;
	u32 size;
} gen7_cp_indexed_reg_list[] = {
	{ GEN7_CP_SQE_STAT_ADDR, GEN7_CP_SQE_STAT_DATA, 0x33},
	{ GEN7_CP_DRAW_STATE_ADDR, GEN7_CP_DRAW_STATE_DATA, 0x100},
	{ GEN7_CP_SQE_UCODE_DBG_ADDR, GEN7_CP_SQE_UCODE_DBG_DATA, 0x8000},
	{ GEN7_CP_BV_SQE_STAT_ADDR, GEN7_CP_BV_SQE_STAT_DATA, 0x33},
	{ GEN7_CP_BV_DRAW_STATE_ADDR, GEN7_CP_BV_DRAW_STATE_DATA, 0x100},
	{ GEN7_CP_BV_SQE_UCODE_DBG_ADDR, GEN7_CP_BV_SQE_UCODE_DBG_DATA, 0x8000},
	{ GEN7_CP_SQE_AC_STAT_ADDR, GEN7_CP_SQE_AC_STAT_DATA, 0x33},
	{ GEN7_CP_LPAC_DRAW_STATE_ADDR, GEN7_CP_LPAC_DRAW_STATE_DATA, 0x100},
	{ GEN7_CP_SQE_AC_UCODE_DBG_ADDR, GEN7_CP_SQE_AC_UCODE_DBG_DATA, 0x8000},
	{ GEN7_CP_LPAC_FIFO_DBG_ADDR, GEN7_CP_LPAC_FIFO_DBG_DATA, 0x40},
};
#endif /*_ADRENO_GEN7_SNAPSHOT_H */
