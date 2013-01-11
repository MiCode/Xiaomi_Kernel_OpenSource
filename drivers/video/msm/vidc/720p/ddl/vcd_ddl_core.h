/* Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
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

#define DDL_LINEAR_BUF_ALIGN_MASK   0xFFFFFFF8U
#define DDL_LINEAR_BUF_ALIGN_GUARD_BYTES 0x7
#define DDL_LINEAR_BUFFER_ALIGN_BYTES  8

#define DDL_TILE_BUF_ALIGN_MASK   0xFFFFE000U
#define DDL_TILE_BUF_ALIGN_GUARD_BYTES 0x1FFF
#define DDL_TILE_BUFFER_ALIGN_BYTES  8192

#define DDL_MAX_FRAME_WIDTH   (1280)
#define DDL_MAX_FRAME_HEIGHT  (720)

#define DDL_MAX_DP_FRAME_WIDTH  352
#define DDL_MAX_DP_FRAME_HEIGHT 288

#define DDL_MAX_BIT_RATE (14*1000*1000)

#define DDL_SW_RESET_SLEEP 10

#define VCD_MAX_NO_CLIENT  4
#define VCD_FRAME_COMMAND_DEPTH 1
#define VCD_GENERAL_COMMAND_DEPTH 1
#define VCD_COMMAND_EXCLUSIVE true

#define DDL_HW_TIMEOUT_IN_MS  1000

#define DDL_STREAMBUF_ALIGN_GUARD_BYTES 0x7

#define DDL_CONTEXT_MEMORY (1024 * 15 * (VCD_MAX_NO_CLIENT + 1))
#define DDL_DB_LINE_BUF_SIZE \
(((((DDL_MAX_FRAME_WIDTH * 4) - 1) / 256) + 1) * 8 * 1024)
#define DDL_MPEG4_DATA_PARTITION_BUF_SIZE (64 * 1024)
#define DDL_DECODE_H264_VSPTEMP_BUFSIZE 0x59c00
#define DDL_ENC_NUM_DPB_BUFFERS 2

#define DDL_DBG_CORE_DUMP_SIZE (10 * 1024)

#define DDL_BUFEND_PAD    256
#define DDL_ENC_SEQHEADER_SIZE (256+DDL_BUFEND_PAD)
#define DDL_MAX_BUFFER_COUNT  32

#define DDL_MPEG_REFBUF_COUNT  2

#define DDL_MPEG_COMV_BUF_NO 2
#define DDL_H263_COMV_BUF_NO 2
#define DDL_COMV_BUFLINE_NO  128
#define DDL_VC1_COMV_BUFLINE_NO  32
#define DDL_MINIMUM_BYTE_PER_SLICE  1920

#define DDL_MAX_H264_QP   51
#define DDL_MAX_MPEG4_QP  31

#define DDL_PADDING_HACK(addr) \
 (addr) = (u32)((((u32)(addr) + DDL_STREAMBUF_ALIGN_GUARD_BYTES) & \
			 ~(DDL_STREAMBUF_ALIGN_GUARD_BYTES)) + DDL_BUFEND_PAD)

#define DDL_QCIF_MBS 99
#define DDL_CIF_MBS  396
#define DDL_QVGA_MBS 300
#define DDL_VGA_MBS  1200
#define DDL_WVGA_MBS 1500
#define DDL_720P_MBS 3600

#define DDL_FRAMESIZE_DIV_FACTOR   (0xF)

#define DDL_NO_OF_MB(width, height) \
	(((width + 15) >> 4) * ((height + 15) >> 4))

#define DDL_ALLOW_ENC_FRAMESIZE(width, height) \
((DDL_NO_OF_MB(width, height) <= DDL_720P_MBS) \
 && (((width) <= DDL_MAX_FRAME_WIDTH) &&            \
     ((height) <= DDL_MAX_FRAME_WIDTH))            \
 && ((width) >= 32 && (height) >= 32))

#define DDL_VALIDATE_ENC_FRAMESIZE(width, height) \
	(!((width) & DDL_FRAMESIZE_DIV_FACTOR) &&     \
     !((height) & DDL_FRAMESIZE_DIV_FACTOR))

#define DDL_TILE_ALIGN_WIDTH     128
#define DDL_TILE_ALIGN_HEIGHT    32
#define DDL_TILE_MULTIPLY_FACTOR 8192
#define DDL_TILE_ALIGN(val, grid) \
   (((val) + (grid) - 1) / (grid) * (grid))

#endif
