/* Copyright (c) 2002,2007-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __A200_REG_H
#define __A200_REG_H

enum VGT_EVENT_TYPE {
	VS_DEALLOC = 0,
	PS_DEALLOC = 1,
	VS_DONE_TS = 2,
	PS_DONE_TS = 3,
	CACHE_FLUSH_TS = 4,
	CONTEXT_DONE = 5,
	CACHE_FLUSH = 6,
	VIZQUERY_START = 7,
	VIZQUERY_END = 8,
	SC_WAIT_WC = 9,
	RST_PIX_CNT = 13,
	RST_VTX_CNT = 14,
	TILE_FLUSH = 15,
	CACHE_FLUSH_AND_INV_TS_EVENT = 20,
	ZPASS_DONE = 21,
	CACHE_FLUSH_AND_INV_EVENT = 22,
	PERFCOUNTER_START = 23,
	PERFCOUNTER_STOP = 24,
	VS_FETCH_DONE = 27,
	FACENESS_FLUSH = 28,
};

enum COLORFORMATX {
	COLORX_4_4_4_4 = 0,
	COLORX_1_5_5_5 = 1,
	COLORX_5_6_5 = 2,
	COLORX_8 = 3,
	COLORX_8_8 = 4,
	COLORX_8_8_8_8 = 5,
	COLORX_S8_8_8_8 = 6,
	COLORX_16_FLOAT = 7,
	COLORX_16_16_FLOAT = 8,
	COLORX_16_16_16_16_FLOAT = 9,
	COLORX_32_FLOAT = 10,
	COLORX_32_32_FLOAT = 11,
	COLORX_32_32_32_32_FLOAT = 12,
	COLORX_2_3_3 = 13,
	COLORX_8_8_8 = 14,
};

enum SURFACEFORMAT {
	FMT_1_REVERSE                  = 0,
	FMT_1                          = 1,
	FMT_8                          = 2,
	FMT_1_5_5_5                    = 3,
	FMT_5_6_5                      = 4,
	FMT_6_5_5                      = 5,
	FMT_8_8_8_8                    = 6,
	FMT_2_10_10_10                 = 7,
	FMT_8_A                        = 8,
	FMT_8_B                        = 9,
	FMT_8_8                        = 10,
	FMT_Cr_Y1_Cb_Y0                = 11,
	FMT_Y1_Cr_Y0_Cb                = 12,
	FMT_5_5_5_1                    = 13,
	FMT_8_8_8_8_A                  = 14,
	FMT_4_4_4_4                    = 15,
	FMT_10_11_11                   = 16,
	FMT_11_11_10                   = 17,
	FMT_DXT1                       = 18,
	FMT_DXT2_3                     = 19,
	FMT_DXT4_5                     = 20,
	FMT_24_8                       = 22,
	FMT_24_8_FLOAT                 = 23,
	FMT_16                         = 24,
	FMT_16_16                      = 25,
	FMT_16_16_16_16                = 26,
	FMT_16_EXPAND                  = 27,
	FMT_16_16_EXPAND               = 28,
	FMT_16_16_16_16_EXPAND         = 29,
	FMT_16_FLOAT                   = 30,
	FMT_16_16_FLOAT                = 31,
	FMT_16_16_16_16_FLOAT          = 32,
	FMT_32                         = 33,
	FMT_32_32                      = 34,
	FMT_32_32_32_32                = 35,
	FMT_32_FLOAT                   = 36,
	FMT_32_32_FLOAT                = 37,
	FMT_32_32_32_32_FLOAT          = 38,
	FMT_32_AS_8                    = 39,
	FMT_32_AS_8_8                  = 40,
	FMT_16_MPEG                    = 41,
	FMT_16_16_MPEG                 = 42,
	FMT_8_INTERLACED               = 43,
	FMT_32_AS_8_INTERLACED         = 44,
	FMT_32_AS_8_8_INTERLACED       = 45,
	FMT_16_INTERLACED              = 46,
	FMT_16_MPEG_INTERLACED         = 47,
	FMT_16_16_MPEG_INTERLACED      = 48,
	FMT_DXN                        = 49,
	FMT_8_8_8_8_AS_16_16_16_16     = 50,
	FMT_DXT1_AS_16_16_16_16        = 51,
	FMT_DXT2_3_AS_16_16_16_16      = 52,
	FMT_DXT4_5_AS_16_16_16_16      = 53,
	FMT_2_10_10_10_AS_16_16_16_16  = 54,
	FMT_10_11_11_AS_16_16_16_16    = 55,
	FMT_11_11_10_AS_16_16_16_16    = 56,
	FMT_32_32_32_FLOAT             = 57,
	FMT_DXT3A                      = 58,
	FMT_DXT5A                      = 59,
	FMT_CTX1                       = 60,
	FMT_DXT3A_AS_1_1_1_1           = 61
};

#define REG_PERF_MODE_CNT	0x0
#define REG_PERF_STATE_RESET	0x0
#define REG_PERF_STATE_ENABLE	0x1
#define REG_PERF_STATE_FREEZE	0x2

#define RB_EDRAM_INFO_EDRAM_SIZE_SIZE                      4
#define RB_EDRAM_INFO_EDRAM_MAPPING_MODE_SIZE              2
#define RB_EDRAM_INFO_UNUSED0_SIZE                         8
#define RB_EDRAM_INFO_EDRAM_RANGE_SIZE                     18

struct rb_edram_info_t {
	unsigned int edram_size:RB_EDRAM_INFO_EDRAM_SIZE_SIZE;
	unsigned int edram_mapping_mode:RB_EDRAM_INFO_EDRAM_MAPPING_MODE_SIZE;
	unsigned int unused0:RB_EDRAM_INFO_UNUSED0_SIZE;
	unsigned int edram_range:RB_EDRAM_INFO_EDRAM_RANGE_SIZE;
};

union reg_rb_edram_info {
	unsigned int val;
	struct rb_edram_info_t f;
};

#define RBBM_READ_ERROR_UNUSED0_SIZE		2
#define RBBM_READ_ERROR_READ_ADDRESS_SIZE	15
#define RBBM_READ_ERROR_UNUSED1_SIZE		13
#define RBBM_READ_ERROR_READ_REQUESTER_SIZE	1
#define RBBM_READ_ERROR_READ_ERROR_SIZE		1

struct rbbm_read_error_t {
	unsigned int unused0:RBBM_READ_ERROR_UNUSED0_SIZE;
	unsigned int read_address:RBBM_READ_ERROR_READ_ADDRESS_SIZE;
	unsigned int unused1:RBBM_READ_ERROR_UNUSED1_SIZE;
	unsigned int read_requester:RBBM_READ_ERROR_READ_REQUESTER_SIZE;
	unsigned int read_error:RBBM_READ_ERROR_READ_ERROR_SIZE;
};

union rbbm_read_error_u {
	unsigned int val:32;
	struct rbbm_read_error_t f;
};

#define CP_RB_CNTL_RB_BUFSZ_SIZE                           6
#define CP_RB_CNTL_UNUSED0_SIZE                            2
#define CP_RB_CNTL_RB_BLKSZ_SIZE                           6
#define CP_RB_CNTL_UNUSED1_SIZE                            2
#define CP_RB_CNTL_BUF_SWAP_SIZE                           2
#define CP_RB_CNTL_UNUSED2_SIZE                            2
#define CP_RB_CNTL_RB_POLL_EN_SIZE                         1
#define CP_RB_CNTL_UNUSED3_SIZE                            6
#define CP_RB_CNTL_RB_NO_UPDATE_SIZE                       1
#define CP_RB_CNTL_UNUSED4_SIZE                            3
#define CP_RB_CNTL_RB_RPTR_WR_ENA_SIZE                     1

struct cp_rb_cntl_t {
	unsigned int rb_bufsz:CP_RB_CNTL_RB_BUFSZ_SIZE;
	unsigned int unused0:CP_RB_CNTL_UNUSED0_SIZE;
	unsigned int rb_blksz:CP_RB_CNTL_RB_BLKSZ_SIZE;
	unsigned int unused1:CP_RB_CNTL_UNUSED1_SIZE;
	unsigned int buf_swap:CP_RB_CNTL_BUF_SWAP_SIZE;
	unsigned int unused2:CP_RB_CNTL_UNUSED2_SIZE;
	unsigned int rb_poll_en:CP_RB_CNTL_RB_POLL_EN_SIZE;
	unsigned int unused3:CP_RB_CNTL_UNUSED3_SIZE;
	unsigned int rb_no_update:CP_RB_CNTL_RB_NO_UPDATE_SIZE;
	unsigned int unused4:CP_RB_CNTL_UNUSED4_SIZE;
	unsigned int rb_rptr_wr_ena:CP_RB_CNTL_RB_RPTR_WR_ENA_SIZE;
};

union reg_cp_rb_cntl {
	unsigned int val:32;
	struct cp_rb_cntl_t f;
};

#define RB_COLOR_INFO__COLOR_FORMAT_MASK                   0x0000000fL
#define RB_COPY_DEST_INFO__COPY_DEST_FORMAT__SHIFT         0x00000004


#define SQ_INT_CNTL__PS_WATCHDOG_MASK                      0x00000001L
#define SQ_INT_CNTL__VS_WATCHDOG_MASK                      0x00000002L

#define RBBM_INT_CNTL__RDERR_INT_MASK                      0x00000001L
#define RBBM_INT_CNTL__DISPLAY_UPDATE_INT_MASK             0x00000002L
#define RBBM_INT_CNTL__GUI_IDLE_INT_MASK                   0x00080000L

#define RBBM_STATUS__CMDFIFO_AVAIL_MASK                    0x0000001fL
#define RBBM_STATUS__TC_BUSY_MASK                          0x00000020L
#define RBBM_STATUS__HIRQ_PENDING_MASK                     0x00000100L
#define RBBM_STATUS__CPRQ_PENDING_MASK                     0x00000200L
#define RBBM_STATUS__CFRQ_PENDING_MASK                     0x00000400L
#define RBBM_STATUS__PFRQ_PENDING_MASK                     0x00000800L
#define RBBM_STATUS__VGT_BUSY_NO_DMA_MASK                  0x00001000L
#define RBBM_STATUS__RBBM_WU_BUSY_MASK                     0x00004000L
#define RBBM_STATUS__CP_NRT_BUSY_MASK                      0x00010000L
#define RBBM_STATUS__MH_BUSY_MASK                          0x00040000L
#define RBBM_STATUS__MH_COHERENCY_BUSY_MASK                0x00080000L
#define RBBM_STATUS__SX_BUSY_MASK                          0x00200000L
#define RBBM_STATUS__TPC_BUSY_MASK                         0x00400000L
#define RBBM_STATUS__SC_CNTX_BUSY_MASK                     0x01000000L
#define RBBM_STATUS__PA_BUSY_MASK                          0x02000000L
#define RBBM_STATUS__VGT_BUSY_MASK                         0x04000000L
#define RBBM_STATUS__SQ_CNTX17_BUSY_MASK                   0x08000000L
#define RBBM_STATUS__SQ_CNTX0_BUSY_MASK                    0x10000000L
#define RBBM_STATUS__RB_CNTX_BUSY_MASK                     0x40000000L
#define RBBM_STATUS__GUI_ACTIVE_MASK                       0x80000000L

#define CP_INT_CNTL__SW_INT_MASK                           0x00080000L
#define CP_INT_CNTL__T0_PACKET_IN_IB_MASK                  0x00800000L
#define CP_INT_CNTL__OPCODE_ERROR_MASK                     0x01000000L
#define CP_INT_CNTL__PROTECTED_MODE_ERROR_MASK             0x02000000L
#define CP_INT_CNTL__RESERVED_BIT_ERROR_MASK               0x04000000L
#define CP_INT_CNTL__IB_ERROR_MASK                         0x08000000L
#define CP_INT_CNTL__IB2_INT_MASK                          0x20000000L
#define CP_INT_CNTL__IB1_INT_MASK                          0x40000000L
#define CP_INT_CNTL__RB_INT_MASK                           0x80000000L

#define MASTER_INT_SIGNAL__MH_INT_STAT                     0x00000020L
#define MASTER_INT_SIGNAL__SQ_INT_STAT                     0x04000000L
#define MASTER_INT_SIGNAL__CP_INT_STAT                     0x40000000L
#define MASTER_INT_SIGNAL__RBBM_INT_STAT                   0x80000000L

#define RB_EDRAM_INFO__EDRAM_SIZE_MASK                     0x0000000fL
#define RB_EDRAM_INFO__EDRAM_RANGE_MASK                    0xffffc000L

#define MH_ARBITER_CONFIG__SAME_PAGE_GRANULARITY__SHIFT    0x00000006
#define MH_ARBITER_CONFIG__L1_ARB_ENABLE__SHIFT            0x00000007
#define MH_ARBITER_CONFIG__L1_ARB_HOLD_ENABLE__SHIFT       0x00000008
#define MH_ARBITER_CONFIG__L2_ARB_CONTROL__SHIFT           0x00000009
#define MH_ARBITER_CONFIG__PAGE_SIZE__SHIFT                0x0000000a
#define MH_ARBITER_CONFIG__TC_REORDER_ENABLE__SHIFT        0x0000000d
#define MH_ARBITER_CONFIG__TC_ARB_HOLD_ENABLE__SHIFT       0x0000000e
#define MH_ARBITER_CONFIG__IN_FLIGHT_LIMIT_ENABLE__SHIFT   0x0000000f
#define MH_ARBITER_CONFIG__IN_FLIGHT_LIMIT__SHIFT          0x00000010
#define MH_ARBITER_CONFIG__CP_CLNT_ENABLE__SHIFT           0x00000016
#define MH_ARBITER_CONFIG__VGT_CLNT_ENABLE__SHIFT          0x00000017
#define MH_ARBITER_CONFIG__TC_CLNT_ENABLE__SHIFT           0x00000018
#define MH_ARBITER_CONFIG__RB_CLNT_ENABLE__SHIFT           0x00000019
#define MH_ARBITER_CONFIG__PA_CLNT_ENABLE__SHIFT           0x0000001a

#define CP_RB_CNTL__RB_BUFSZ__SHIFT                        0x00000000
#define CP_RB_CNTL__RB_BLKSZ__SHIFT                        0x00000008
#define CP_RB_CNTL__RB_POLL_EN__SHIFT                      0x00000014
#define CP_RB_CNTL__RB_NO_UPDATE__SHIFT                    0x0000001b

#define RB_COLOR_INFO__COLOR_FORMAT__SHIFT                 0x00000000
#define RB_EDRAM_INFO__EDRAM_MAPPING_MODE__SHIFT           0x00000004
#define RB_EDRAM_INFO__EDRAM_RANGE__SHIFT                  0x0000000e

#define REG_CP_CSQ_IB1_STAT              0x01FE
#define REG_CP_CSQ_IB2_STAT              0x01FF
#define REG_CP_CSQ_RB_STAT               0x01FD
#define REG_CP_DEBUG                     0x01FC
#define REG_CP_IB1_BASE                  0x0458
#define REG_CP_IB1_BUFSZ                 0x0459
#define REG_CP_IB2_BASE                  0x045A
#define REG_CP_IB2_BUFSZ                 0x045B
#define REG_CP_INT_ACK                   0x01F4
#define REG_CP_INT_CNTL                  0x01F2
#define REG_CP_INT_STATUS                0x01F3
#define REG_CP_ME_CNTL                   0x01F6
#define REG_CP_ME_RAM_DATA               0x01FA
#define REG_CP_ME_RAM_WADDR              0x01F8
#define REG_CP_ME_STATUS                 0x01F7
#define REG_CP_PFP_UCODE_ADDR            0x00C0
#define REG_CP_PFP_UCODE_DATA            0x00C1
#define REG_CP_QUEUE_THRESHOLDS          0x01D5
#define REG_CP_RB_BASE                   0x01C0
#define REG_CP_RB_CNTL                   0x01C1
#define REG_CP_RB_RPTR                   0x01C4
#define REG_CP_RB_RPTR_ADDR              0x01C3
#define REG_CP_RB_RPTR_WR                0x01C7
#define REG_CP_RB_WPTR                   0x01C5
#define REG_CP_RB_WPTR_BASE              0x01C8
#define REG_CP_RB_WPTR_DELAY             0x01C6
#define REG_CP_STAT                      0x047F
#define REG_CP_STATE_DEBUG_DATA          0x01ED
#define REG_CP_STATE_DEBUG_INDEX         0x01EC
#define REG_CP_ST_BASE                   0x044D
#define REG_CP_ST_BUFSZ                  0x044E

#define REG_CP_PERFMON_CNTL              0x0444
#define REG_CP_PERFCOUNTER_SELECT        0x0445
#define REG_CP_PERFCOUNTER_LO            0x0446
#define REG_CP_PERFCOUNTER_HI            0x0447

#define REG_RBBM_PERFCOUNTER1_SELECT     0x0395
#define REG_RBBM_PERFCOUNTER1_HI         0x0398
#define REG_RBBM_PERFCOUNTER1_LO         0x0397

#define REG_MASTER_INT_SIGNAL            0x03B7

#define REG_PA_CL_VPORT_XSCALE           0x210F
#define REG_PA_CL_VPORT_ZOFFSET          0x2114
#define REG_PA_CL_VPORT_ZSCALE           0x2113
#define REG_PA_CL_CLIP_CNTL              0x2204
#define REG_PA_CL_VTE_CNTL               0x2206
#define REG_PA_SC_AA_MASK                0x2312
#define REG_PA_SC_LINE_CNTL              0x2300
#define REG_PA_SC_SCREEN_SCISSOR_BR      0x200F
#define REG_PA_SC_SCREEN_SCISSOR_TL      0x200E
#define REG_PA_SC_VIZ_QUERY              0x2293
#define REG_PA_SC_VIZ_QUERY_STATUS       0x0C44
#define REG_PA_SC_WINDOW_OFFSET          0x2080
#define REG_PA_SC_WINDOW_SCISSOR_BR      0x2082
#define REG_PA_SC_WINDOW_SCISSOR_TL      0x2081
#define REG_PA_SU_FACE_DATA              0x0C86
#define REG_PA_SU_POINT_SIZE             0x2280
#define REG_PA_SU_LINE_CNTL              0x2282
#define REG_PA_SU_POLY_OFFSET_BACK_OFFSET 0x2383
#define REG_PA_SU_POLY_OFFSET_FRONT_SCALE 0x2380
#define REG_PA_SU_SC_MODE_CNTL           0x2205

#define REG_PC_INDEX_OFFSET              0x2102

#define REG_RBBM_CNTL                    0x003B
#define REG_RBBM_INT_ACK                 0x03B6
#define REG_RBBM_INT_CNTL                0x03B4
#define REG_RBBM_INT_STATUS              0x03B5
#define REG_RBBM_PATCH_RELEASE           0x0001
#define REG_RBBM_PERIPHID1               0x03F9
#define REG_RBBM_PERIPHID2               0x03FA
#define REG_RBBM_DEBUG                   0x039B
#define REG_RBBM_DEBUG_OUT               0x03A0
#define REG_RBBM_DEBUG_CNTL              0x03A1
#define REG_RBBM_PM_OVERRIDE1            0x039C
#define REG_RBBM_PM_OVERRIDE2            0x039D
#define REG_RBBM_READ_ERROR              0x03B3
#define REG_RBBM_SOFT_RESET              0x003C
#define REG_RBBM_STATUS                  0x05D0

#define REG_RB_COLORCONTROL              0x2202
#define REG_RB_COLOR_DEST_MASK           0x2326
#define REG_RB_COLOR_MASK                0x2104
#define REG_RB_COPY_CONTROL              0x2318
#define REG_RB_DEPTHCONTROL              0x2200
#define REG_RB_EDRAM_INFO                0x0F02
#define REG_RB_MODECONTROL               0x2208
#define REG_RB_SURFACE_INFO              0x2000
#define REG_RB_SAMPLE_POS                0x220a

#define REG_SCRATCH_ADDR                 0x01DD
#define REG_SCRATCH_REG0                 0x0578
#define REG_SCRATCH_REG2                 0x057A
#define REG_SCRATCH_UMSK                 0x01DC

#define REG_SQ_CF_BOOLEANS               0x4900
#define REG_SQ_CF_LOOP                   0x4908
#define REG_SQ_GPR_MANAGEMENT            0x0D00
#define REG_SQ_FLOW_CONTROL              0x0D01
#define REG_SQ_INST_STORE_MANAGMENT      0x0D02
#define REG_SQ_INT_ACK                   0x0D36
#define REG_SQ_INT_CNTL                  0x0D34
#define REG_SQ_INT_STATUS                0x0D35
#define REG_SQ_PROGRAM_CNTL              0x2180
#define REG_SQ_PS_PROGRAM                0x21F6
#define REG_SQ_VS_PROGRAM                0x21F7
#define REG_SQ_WRAPPING_0                0x2183
#define REG_SQ_WRAPPING_1                0x2184

#define REG_VGT_ENHANCE                  0x2294
#define REG_VGT_INDX_OFFSET              0x2102
#define REG_VGT_MAX_VTX_INDX             0x2100
#define REG_VGT_MIN_VTX_INDX             0x2101

#define REG_TP0_CHICKEN                  0x0E1E
#define REG_TC_CNTL_STATUS               0x0E00
#define REG_PA_SC_AA_CONFIG              0x2301
#define REG_VGT_VERTEX_REUSE_BLOCK_CNTL  0x2316
#define REG_SQ_INTERPOLATOR_CNTL         0x2182
#define REG_RB_DEPTH_INFO                0x2002
#define REG_COHER_DEST_BASE_0            0x2006
#define REG_RB_FOG_COLOR                 0x2109
#define REG_RB_STENCILREFMASK_BF         0x210C
#define REG_PA_SC_LINE_STIPPLE           0x2283
#define REG_SQ_PS_CONST                  0x2308
#define REG_RB_DEPTH_CLEAR               0x231D
#define REG_RB_SAMPLE_COUNT_CTL          0x2324
#define REG_SQ_CONSTANT_0                0x4000
#define REG_SQ_FETCH_0                   0x4800

#define REG_COHER_BASE_PM4               0xA2A
#define REG_COHER_STATUS_PM4             0xA2B
#define REG_COHER_SIZE_PM4               0xA29

/*registers added in adreno220*/
#define REG_A220_PC_INDX_OFFSET          REG_VGT_INDX_OFFSET
#define REG_A220_PC_VERTEX_REUSE_BLOCK_CNTL REG_VGT_VERTEX_REUSE_BLOCK_CNTL
#define REG_A220_PC_MAX_VTX_INDX         REG_VGT_MAX_VTX_INDX
#define REG_A220_RB_LRZ_VSC_CONTROL	 0x2209
#define REG_A220_GRAS_CONTROL            0x2210
#define REG_A220_VSC_BIN_SIZE            0x0C01
#define REG_A220_VSC_PIPE_DATA_LENGTH_7  0x0C1D

/*registers added in adreno225*/
#define REG_A225_RB_COLOR_INFO3          0x2005
#define REG_A225_PC_MULTI_PRIM_IB_RESET_INDX 0x2103
#define REG_A225_GRAS_UCP0X              0x2340
#define REG_A225_GRAS_UCP_ENABLED        0x2360

#endif /* __A200_REG_H */
