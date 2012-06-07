/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
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

#ifndef _VCD_DDL_CORE_H_
#define _VCD_DDL_CORE_H_

#define DDL_LINEAR_BUF_ALIGN_MASK         0xFFFFF800U
#define DDL_LINEAR_BUF_ALIGN_GUARD_BYTES  0x7FF
#define DDL_LINEAR_BUFFER_ALIGN_BYTES     2048
#define DDL_TILE_BUF_ALIGN_MASK           0xFFFFE000U
#define DDL_TILE_BUF_ALIGN_GUARD_BYTES    0x1FFF
#define DDL_TILE_BUFFER_ALIGN_BYTES       8192

#define DDL_YUV_BUF_TYPE_LINEAR 0
#define DDL_YUV_BUF_TYPE_TILE   1

#define DDL_NO_OF_MB(nWidth, nHeight) \
	((((nWidth) + 15) >> 4) * (((nHeight) + 15) >> 4))

#define DDL_MAX_FRAME_WIDTH   1920
#define DDL_MAX_FRAME_HEIGHT  1088

#define MAX_DPB_SIZE_L4PT0_MBS    DDL_KILO_BYTE(32)
#define MAX_FRAME_SIZE_L4PT0_MBS  DDL_KILO_BYTE(8)

#define DDL_MAX_MB_PER_FRAME (DDL_NO_OF_MB(DDL_MAX_FRAME_WIDTH,\
	DDL_MAX_FRAME_HEIGHT))

#define DDL_DB_LINE_BUF_SIZE\
	(((((DDL_MAX_FRAME_WIDTH * 4) - 1) / 256) + 1) * 8 * 1024)

#define DDL_MAX_FRAME_RATE               120
#define DDL_INITIAL_FRAME_RATE            30

#define DDL_MAX_BIT_RATE    (20*1024*1024)
#define DDL_MAX_MB_PER_SEC  (DDL_MAX_MB_PER_FRAME * DDL_INITIAL_FRAME_RATE)

#define DDL_SW_RESET_SLEEP               1
#define VCD_MAX_NO_CLIENT                4
#define VCD_SINGLE_FRAME_COMMAND_CHANNEL 1
#define VCD_DUAL_FRAME_COMMAND_CHANNEL   2
#define VCD_FRAME_COMMAND_DEPTH          VCD_SINGLE_FRAME_COMMAND_CHANNEL
#define VCD_GENEVIDC_COMMAND_DEPTH        1
#define VCD_COMMAND_EXCLUSIVE            true
#define DDL_HW_TIMEOUT_IN_MS             1000
#define DDL_STREAMBUF_ALIGN_GUARD_BYTES  0x7FF

#define DDL_VIDC_1080P_48MHZ			(48000000)
#define DDL_VIDC_1080P_133MHZ			(133330000)
#define DDL_VIDC_1080P_200MHZ			(200000000)
#define DDL_VIDC_1080P_48MHZ_TIMEOUT_VALUE	(0xCB8)
#define DDL_VIDC_1080P_133MHZ_TIMEOUT_VALUE	(0x2355)
#define DDL_VIDC_1080P_200MHZ_TIMEOUT_VALUE	(0x3500)

#define DDL_CONTEXT_MEMORY (1024 * 15 * (VCD_MAX_NO_CLIENT + 1))

#define DDL_ENC_MIN_DPB_BUFFERS           2
#define DDL_ENC_MAX_DPB_BUFFERS           4

#define DDL_FW_AUX_HOST_CMD_SPACE_SIZE         (DDL_KILO_BYTE(4))
#define DDL_FW_INST_GLOBAL_CONTEXT_SPACE_SIZE  (DDL_KILO_BYTE(800))
#define DDL_FW_H264DEC_CONTEXT_SPACE_SIZE      (DDL_KILO_BYTE(800))
#define DDL_FW_H264ENC_CONTEXT_SPACE_SIZE      (DDL_KILO_BYTE(20))
#define DDL_FW_OTHER_CONTEXT_SPACE_SIZE        (DDL_KILO_BYTE(20))

#define VCD_DEC_CPB_SIZE         (DDL_KILO_BYTE(512))
#define DDL_DBG_CORE_DUMP_SIZE   (DDL_KILO_BYTE(10))
#define DDL_VIDC_1080P_BASE_OFFSET_SHIFT        11

#define DDL_BUFEND_PAD                    256
#define DDL_ENC_SEQHEADER_SIZE            (512+DDL_BUFEND_PAD)
#define DDL_ENC_SLICE_BATCH_FACTOR         5
#define DDL_MAX_NUM_BFRS_FOR_SLICE_BATCH   8
#define DDL_ENC_SLICE_BATCH_INPSTRUCT_SIZE (128 + \
				32 * DDL_MAX_NUM_BFRS_FOR_SLICE_BATCH)
#define DDL_ENC_SLICE_BATCH_OUTSTRUCT_SIZE (64 + \
				64 * DDL_MAX_NUM_BFRS_FOR_SLICE_BATCH)
#define DDL_MAX_BUFFER_COUNT              32
#define DDL_MIN_BUFFER_COUNT              1

#define DDL_MPEG_REFBUF_COUNT             2
#define DDL_MPEG_COMV_BUF_NO              2
#define DDL_H263_COMV_BUF_NO              0
#define DDL_COMV_BUFLINE_NO               128
#define DDL_VC1_COMV_BUFLINE_NO           32

#define DDL_MAX_H264_QP            51
#define DDL_MAX_MPEG4_QP           31

#define DDL_CONCEALMENT_Y_COLOR                 16
#define DDL_CONCEALMENT_C_COLOR                 128

#define DDL_ALLOW_DEC_FRAMESIZE(width, height) \
	((DDL_NO_OF_MB(width, height) <= \
	MAX_FRAME_SIZE_L4PT0_MBS) && \
	(width <= DDL_MAX_FRAME_WIDTH) && \
	(height <= DDL_MAX_FRAME_WIDTH) && \
	((width >= 32 && height >= 16) || \
	(width >= 16 && height >= 32)))

#define DDL_ALLOW_ENC_FRAMESIZE(width, height) \
	((DDL_NO_OF_MB(width, height) <= \
	MAX_FRAME_SIZE_L4PT0_MBS) && \
	(width <= DDL_MAX_FRAME_WIDTH) && \
	(height <= DDL_MAX_FRAME_WIDTH) && \
	((width >= 32 && height >= 32)))

#define DDL_LINEAR_ALIGN_WIDTH      16
#define DDL_LINEAR_ALIGN_HEIGHT     16
#define DDL_LINEAR_MULTIPLY_FACTOR  2048
#define DDL_TILE_ALIGN_WIDTH        128
#define DDL_TILE_ALIGN_HEIGHT       32
#define DDL_TILE_MULTIPLY_FACTOR    8192
#define DDL_TILE_ALIGN(val, grid) \
	(((val) + (grid) - 1) / (grid) * (grid))

#define VCD_DDL_720P_YUV_BUF_SIZE     ((1280*720*3) >> 1)
#define VCD_DDL_WVGA_BUF_SIZE         (800*480)

#define VCD_DDL_TEST_MAX_WIDTH        (DDL_MAX_FRAME_WIDTH)
#define VCD_DDL_TEST_MAX_HEIGHT       (DDL_MAX_FRAME_HEIGHT)

#define VCD_DDL_TEST_MAX_NUM_H264_DPB  8

#define VCD_DDL_TEST_NUM_ENC_INPUT_BUFS   6
#define VCD_DDL_TEST_NUM_ENC_OUTPUT_BUFS  4

#define VCD_DDL_TEST_DEFAULT_WIDTH       176
#define VCD_DDL_TEST_DEFAULT_HEIGHT      144

#define DDL_PIXEL_CACHE_NOT_IDLE          0x4000
#define DDL_PIXEL_CACHE_STATUS_READ_RETRY 10
#define DDL_PIXEL_CACHE_STATUS_READ_SLEEP 200

#define DDL_RESL_CHANGE_NO_CHANGE               0
#define DDL_RESL_CHANGE_INCREASED               1
#define DDL_RESL_CHANGE_DECREASED               2

#define VIDC_SM_ERR_CONCEALMENT_ENABLE				1
#define VIDC_SM_ERR_CONCEALMENT_INTER_SLICE_MB_COPY		2
#define VIDC_SM_ERR_CONCEALMENT_INTRA_SLICE_COLOR_CONCEALMENT	1

#endif
