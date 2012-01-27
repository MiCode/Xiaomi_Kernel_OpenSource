/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#ifndef __VIDC_HAL_H__
#define __VIDC_HAL_H__

#include <linux/spinlock.h>
#include <linux/mutex.h>
#include "vidc_hal_api.h"
#include "msm_smem.h"

#ifdef HAL_MSG_LOG
#define HAL_MSG_LOW(x...) pr_debug(KERN_INFO x)
#define HAL_MSG_MEDIUM(x...) pr_debug(KERN_INFO x)
#define HAL_MSG_HIGH(x...) pr_debug(KERN_INFO x)
#else
#define HAL_MSG_LOW(x...)
#define HAL_MSG_MEDIUM(x...)
#define HAL_MSG_HIGH(x...)
#endif

#define HAL_MSG_ERROR(x...) pr_err(KERN_INFO x)
#define HAL_MSG_FATAL(x...) pr_err(KERN_INFO x)
#define HAL_MSG_INFO(x...) pr_info(KERN_INFO x)

#define HFI_MASK_QHDR_TX_TYPE			0xFF000000
#define HFI_MASK_QHDR_RX_TYPE			0x00FF0000
#define HFI_MASK_QHDR_PRI_TYPE			0x0000FF00
#define HFI_MASK_QHDR_Q_ID_TYPE			0x000000FF
#define HFI_Q_ID_HOST_TO_CTRL_CMD_Q		0x00
#define HFI_Q_ID_CTRL_TO_HOST_MSG_Q		0x01
#define HFI_Q_ID_CTRL_TO_HOST_DEBUG_Q	0x02
#define HFI_MASK_QHDR_STATUS			0x000000FF

#define VIDC_MAX_UNCOMPRESSED_FMT_PLANES	3

#define VIDC_IFACEQ_NUMQ					3
#define VIDC_IFACEQ_CMDQ_IDX				0
#define VIDC_IFACEQ_MSGQ_IDX				1
#define VIDC_IFACEQ_DBGQ_IDX				2

#define VIDC_IFACEQ_MAX_PKT_SIZE			1024
#define VIDC_IFACEQ_MED_PKT_SIZE			768
#define VIDC_IFACEQ_MIN_PKT_SIZE			8
#define VIDC_IFACEQ_VAR_SMALL_PKT_SIZE		100
#define VIDC_IFACEQ_VAR_LARGE_PKT_SIZE		512
#define VIDC_IFACEQ_MAX_BUF_COUNT			50
#define VIDC_IFACE_MAX_PARALLEL_CLNTS		16
#define VIDC_IFACEQ_DFLT_QHDR				0x01010000

struct hfi_queue_table_header {
	u32 qtbl_version;
	u32 qtbl_size;
	u32 qtbl_qhdr0_offset;
	u32 qtbl_qhdr_size;
	u32 qtbl_num_q;
	u32 qtbl_num_active_q;
};

struct hfi_queue_header {
	u32 qhdr_status;
	u32 qhdr_start_addr;
	u32 qhdr_type;
	u32 qhdr_q_size;
	u32 qhdr_pkt_size;
	u32 qhdr_pkt_drop_cnt;
	u32 qhdr_rx_wm;
	u32 qhdr_tx_wm;
	u32 qhdr_rx_req;
	u32 qhdr_tx_req;
	u32 qhdr_rx_irq_status;
	u32 qhdr_tx_irq_status;
	u32 qhdr_read_idx;
	u32 qhdr_write_idx;
};

#define VIDC_IFACEQ_TABLE_SIZE (sizeof(struct hfi_queue_table_header) \
	+ sizeof(struct hfi_queue_header) * VIDC_IFACEQ_NUMQ)

#define VIDC_IFACEQ_QUEUE_SIZE		(VIDC_IFACEQ_MAX_PKT_SIZE *  \
	VIDC_IFACEQ_MAX_BUF_COUNT * VIDC_IFACE_MAX_PARALLEL_CLNTS)

#define VIDC_IFACEQ_GET_QHDR_START_ADDR(ptr, i)     \
	(void *)((((u32)ptr) + sizeof(struct hfi_queue_table_header)) + \
		(i * sizeof(struct hfi_queue_header)))

enum vidc_hw_reg {
	VIDC_HWREG_CTRL_STATUS =  0x1,
	VIDC_HWREG_QTBL_INFO =  0x2,
	VIDC_HWREG_QTBL_ADDR =  0x3,
	VIDC_HWREG_CTRLR_RESET =  0x4,
	VIDC_HWREG_IFACEQ_FWRXREQ =  0x5,
	VIDC_HWREG_IFACEQ_FWTXREQ =  0x6,
	VIDC_HWREG_VHI_SOFTINTEN =  0x7,
	VIDC_HWREG_VHI_SOFTINTSTATUS =  0x8,
	VIDC_HWREG_VHI_SOFTINTCLR =  0x9,
	VIDC_HWREG_HVI_SOFTINTEN =  0xA,
};

enum HFI_EVENT {
	HFI_EVENT_SYS_ERROR,
	HFI_EVENT_SESSION_ERROR,
	HFI_EVENT_SESSION_SEQUENCE_CHANGED,
	HFI_EVENT_SESSION_PROPERTY_CHANGED,
	HFI_UNUSED_EVENT = 0x10000000,
};

enum HFI_EVENT_DATA_SEQUENCE_CHANGED {
	HFI_EVENT_DATA_SEQUENCE_CHANGED_SUFFICIENT_BUFFER_RESOURCES,
	HFI_EVENT_DATA_SEQUENCE_CHANGED_INSUFFICIENT_BUFFER_RESOURCES,
	HFI_UNUSED_SEQCHG = 0x10000000,
};

#define HFI_BUFFERFLAG_EOS              0x00000001
#define HFI_BUFFERFLAG_STARTTIME        0x00000002
#define HFI_BUFFERFLAG_DECODEONLY       0x00000004
#define HFI_BUFFERFLAG_DATACORRUPT      0x00000008
#define HFI_BUFFERFLAG_ENDOFFRAME       0x00000010
#define HFI_BUFFERFLAG_SYNCFRAME        0x00000020
#define HFI_BUFFERFLAG_EXTRADATA        0x00000040
#define HFI_BUFFERFLAG_CODECCONFIG      0x00000080
#define HFI_BUFFERFLAG_TIMESTAMPINVALID 0x00000100
#define HFI_BUFFERFLAG_READONLY         0x00000200
#define HFI_BUFFERFLAG_ENDOFSUBFRAME    0x00000400

enum HFI_ERROR {
	HFI_ERR_NONE                              = 0,
	HFI_ERR_SYS_UNKNOWN                       = 0x80000001,
	HFI_ERR_SYS_FATAL                         = 0x80000002,
	HFI_ERR_SYS_INVALID_PARAMETER             = 0x80000003,
	HFI_ERR_SYS_VERSION_MISMATCH              = 0x80000004,
	HFI_ERR_SYS_INSUFFICIENT_RESOURCES        = 0x80000005,
	HFI_ERR_SYS_MAX_SESSIONS_REACHED          = 0x80000006,
	HFI_ERR_SYS_UNSUPPORTED_CODEC             = 0x80000007,
	HFI_ERR_SYS_SESSION_IN_USE                = 0x80000008,
	HFI_ERR_SYS_SESSION_ID_OUT_OF_RANGE       = 0x80000009,
	HFI_ERR_SYS_UNSUPPORTED_DOMAIN            = 0x8000000A,
	HFI_ERR_SESSION_START_UNUSED              = 0x80001000,
	HFI_ERR_SESSION_UNKNOWN                   = 0x80001001,
	HFI_ERR_SESSION_FATAL                     = 0x80001002,
	HFI_ERR_SESSION_INVALID_PARAMETER         = 0x80001003,
	HFI_ERR_SESSION_BAD_POINTER               = 0x80001004,
	HFI_ERR_SESSION_INVALID_SESSION_ID        = 0x80001005,
	HFI_ERR_SESSION_INVALID_STREAM_ID         = 0x80001006,
	HFI_ERR_SESSION_INCORRECT_STATE_OPERATION = 0x80001007,
	HFI_ERR_SESSION_UNSUPPORTED_PROPERTY      = 0x80001008,
	HFI_ERR_SESSION_UNSUPPORTED_SETTING       = 0x80001009,
	HFI_ERR_SESSION_INSUFFICIENT_RESOURCES    = 0x8000100A,
	HFI_ERR_SESSION_STREAM_CORRUPT            = 0x8000100B,
	HFI_ERR_SESSION_STREAM_CORRUPT_OUTPUT_STALLED    =  0x8000100C,
	HFI_ERR_SESSION_SYNC_FRAME_NOT_DETECTED          =  0x8000100D,
	HFI_ERR_SESSION_EMPTY_BUFFER_DONE_OUTPUT_PENDING =  0x8000100E,
	HFI_ERR_SESSION_SAME_STATE_OPERATION		= 0x8000100F,
	HFI_UNUSED_ERR = 0x10000000,
};

enum HFI_DOMAIN {
	HFI_VIDEO_DOMAIN_VPE,
	HFI_VIDEO_DOMAIN_ENCODER,
	HFI_VIDEO_DOMAIN_DECODER,
	HFI_UNUSED_DOMAIN = 0x10000000,
};

enum HFI_VIDEO_CODEC {
	HFI_VIDEO_CODEC_UNKNOWN  = 0x00000000,
	HFI_VIDEO_CODEC_H264     = 0x00000002,
	HFI_VIDEO_CODEC_H263     = 0x00000004,
	HFI_VIDEO_CODEC_MPEG1    = 0x00000008,
	HFI_VIDEO_CODEC_MPEG2    = 0x00000010,
	HFI_VIDEO_CODEC_MPEG4    = 0x00000020,
	HFI_VIDEO_CODEC_DIVX_311 = 0x00000040,
	HFI_VIDEO_CODEC_DIVX     = 0x00000080,
	HFI_VIDEO_CODEC_VC1      = 0x00000100,
	HFI_VIDEO_CODEC_SPARK    = 0x00000200,
	HFI_VIDEO_CODEC_VP6      = 0x00000400,
	HFI_VIDEO_CODEC_VP7		 = 0x00000800,
	HFI_VIDEO_CODEC_VP8		 = 0x00001000,
	HFI_UNUSED_CODEC		 = 0x10000000,
};

enum HFI_H263_PROFILE {
	HFI_H263_PROFILE_BASELINE           = 0x00000001,
	HFI_H263_PROFILE_H320CODING         = 0x00000002,
	HFI_H263_PROFILE_BACKWARDCOMPATIBLE = 0x00000004,
	HFI_H263_PROFILE_ISWV2              = 0x00000008,
	HFI_H263_PROFILE_ISWV3              = 0x00000010,
	HFI_H263_PROFILE_HIGHCOMPRESSION    = 0x00000020,
	HFI_H263_PROFILE_INTERNET           = 0x00000040,
	HFI_H263_PROFILE_INTERLACE          = 0x00000080,
	HFI_H263_PROFILE_HIGHLATENCY        = 0x00000100,
	HFI_UNUSED_H263_PROFILE = 0x10000000,
};

enum HFI_H263_LEVEL {
	HFI_H263_LEVEL_10 = 0x00000001,
	HFI_H263_LEVEL_20 = 0x00000002,
	HFI_H263_LEVEL_30 = 0x00000004,
	HFI_H263_LEVEL_40 = 0x00000008,
	HFI_H263_LEVEL_45 = 0x00000010,
	HFI_H263_LEVEL_50 = 0x00000020,
	HFI_H263_LEVEL_60 = 0x00000040,
	HFI_H263_LEVEL_70 = 0x00000080,
	HFI_UNUSED_H263_LEVEL = 0x10000000,
};

enum HFI_MPEG2_PROFILE {
	HFI_MPEG2_PROFILE_SIMPLE  = 0x00000001,
	HFI_MPEG2_PROFILE_MAIN    = 0x00000002,
	HFI_MPEG2_PROFILE_422     = 0x00000004,
	HFI_MPEG2_PROFILE_SNR     = 0x00000008,
	HFI_MPEG2_PROFILE_SPATIAL = 0x00000010,
	HFI_MPEG2_PROFILE_HIGH    = 0x00000020,
	HFI_UNUSED_MPEG2_PROFILE = 0x10000000,
};

enum HFI_MPEG2_LEVEL {
	HFI_MPEG2_LEVEL_LL  = 0x00000001,
	HFI_MPEG2_LEVEL_ML  = 0x00000002,
	HFI_MPEG2_LEVEL_H14 = 0x00000004,
	HFI_MPEG2_LEVEL_HL  = 0x00000008,
	HFI_UNUSED_MEPG2_LEVEL = 0x10000000,
};

enum HFI_MPEG4_PROFILE {
	HFI_MPEG4_PROFILE_SIMPLE           = 0x00000001,
	HFI_MPEG4_PROFILE_SIMPLESCALABLE   = 0x00000002,
	HFI_MPEG4_PROFILE_CORE             = 0x00000004,
	HFI_MPEG4_PROFILE_MAIN             = 0x00000008,
	HFI_MPEG4_PROFILE_NBIT             = 0x00000010,
	HFI_MPEG4_PROFILE_SCALABLETEXTURE  = 0x00000020,
	HFI_MPEG4_PROFILE_SIMPLEFACE       = 0x00000040,
	HFI_MPEG4_PROFILE_SIMPLEFBA        = 0x00000080,
	HFI_MPEG4_PROFILE_BASICANIMATED    = 0x00000100,
	HFI_MPEG4_PROFILE_HYBRID           = 0x00000200,
	HFI_MPEG4_PROFILE_ADVANCEDREALTIME = 0x00000400,
	HFI_MPEG4_PROFILE_CORESCALABLE     = 0x00000800,
	HFI_MPEG4_PROFILE_ADVANCEDCODING   = 0x00001000,
	HFI_MPEG4_PROFILE_ADVANCEDCORE     = 0x00002000,
	HFI_MPEG4_PROFILE_ADVANCEDSCALABLE = 0x00004000,
	HFI_MPEG4_PROFILE_ADVANCEDSIMPLE   = 0x00008000,
	HFI_UNUSED_MPEG4_PROFILE = 0x10000000,
};

enum HFI_MPEG4_LEVEL {
	HFI_MPEG4_LEVEL_0  = 0x00000001,
	HFI_MPEG4_LEVEL_0b = 0x00000002,
	HFI_MPEG4_LEVEL_1  = 0x00000004,
	HFI_MPEG4_LEVEL_2  = 0x00000008,
	HFI_MPEG4_LEVEL_3  = 0x00000010,
	HFI_MPEG4_LEVEL_4  = 0x00000020,
	HFI_MPEG4_LEVEL_4a = 0x00000040,
	HFI_MPEG4_LEVEL_5  = 0x00000080,
	HFI_MPEG4_LEVEL_VENDOR_START_UNUSED = 0x7F000000,
	HFI_MPEG4_LEVEL_6  = 0x7F000001,
	HFI_MPEG4_LEVEL_7  = 0x7F000002,
	HFI_MPEG4_LEVEL_8  = 0x7F000003,
	HFI_MPEG4_LEVEL_9  = 0x7F000004,
	HFI_MPEG4_LEVEL_3b = 0x7F000005,
	HFI_UNUSED_MPEG4_LEVEL = 0x10000000,
};

enum HFI_H264_PROFILE {
	HFI_H264_PROFILE_BASELINE = 0x00000001,
	HFI_H264_PROFILE_MAIN     = 0x00000002,
	HFI_H264_PROFILE_EXTENDED = 0x00000004,
	HFI_H264_PROFILE_HIGH     = 0x00000008,
	HFI_H264_PROFILE_HIGH10   = 0x00000010,
	HFI_H264_PROFILE_HIGH422  = 0x00000020,
	HFI_H264_PROFILE_HIGH444  = 0x00000040,
	HFI_H264_PROFILE_STEREO_HIGH = 0x00000080,
	HFI_H264_PROFILE_MV_HIGH  = 0x00000100,
	HFI_UNUSED_H264_PROFILE   = 0x10000000,
};

enum HFI_H264_LEVEL {
	HFI_H264_LEVEL_1  = 0x00000001,
	HFI_H264_LEVEL_1b = 0x00000002,
	HFI_H264_LEVEL_11 = 0x00000004,
	HFI_H264_LEVEL_12 = 0x00000008,
	HFI_H264_LEVEL_13 = 0x00000010,
	HFI_H264_LEVEL_2  = 0x00000020,
	HFI_H264_LEVEL_21 = 0x00000040,
	HFI_H264_LEVEL_22 = 0x00000080,
	HFI_H264_LEVEL_3  = 0x00000100,
	HFI_H264_LEVEL_31 = 0x00000200,
	HFI_H264_LEVEL_32 = 0x00000400,
	HFI_H264_LEVEL_4  = 0x00000800,
	HFI_H264_LEVEL_41 = 0x00001000,
	HFI_H264_LEVEL_42 = 0x00002000,
	HFI_H264_LEVEL_5  = 0x00004000,
	HFI_H264_LEVEL_51 = 0x00008000,
	HFI_UNUSED_H264_LEVEL = 0x10000000,
};

enum HFI_VPX_PROFILE {
	HFI_VPX_PROFILE_SIMPLE    = 0x00000001,
	HFI_VPX_PROFILE_ADVANCED  = 0x00000002,
	HFI_VPX_PROFILE_VERSION_0 = 0x00000004,
	HFI_VPX_PROFILE_VERSION_1 = 0x00000008,
	HFI_VPX_PROFILE_VERSION_2 = 0x00000010,
	HFI_VPX_PROFILE_VERSION_3 = 0x00000020,
	HFI_VPX_PROFILE_UNUSED = 0x10000000,
};

enum HFI_VC1_PROFILE {
	HFI_VC1_PROFILE_SIMPLE   = 0x00000001,
	HFI_VC1_PROFILE_MAIN     = 0x00000002,
	HFI_VC1_PROFILE_ADVANCED = 0x00000004,
	HFI_UNUSED_VC1_PROFILE = 0x10000000,
};

enum HFI_VC1_LEVEL {
	HFI_VC1_LEVEL_LOW    = 0x00000001,
	HFI_VC1_LEVEL_MEDIUM = 0x00000002,
	HFI_VC1_LEVEL_HIGH   = 0x00000004,
	HFI_VC1_LEVEL_0      = 0x00000008,
	HFI_VC1_LEVEL_1      = 0x00000010,
	HFI_VC1_LEVEL_2      = 0x00000020,
	HFI_VC1_LEVEL_3      = 0x00000040,
	HFI_VC1_LEVEL_4      = 0x00000080,
	HFI_UNUSED_VC1_LEVEL = 0x10000000,
};

enum HFI_DIVX_FORMAT {
	HFI_DIVX_FORMAT_4,
	HFI_DIVX_FORMAT_5,
	HFI_DIVX_FORMAT_6,
	HFI_UNUSED_DIVX_FORMAT = 0x10000000,
};

enum HFI_DIVX_PROFILE {
	HFI_DIVX_PROFILE_QMOBILE  = 0x00000001,
	HFI_DIVX_PROFILE_MOBILE   = 0x00000002,
	HFI_DIVX_PROFILE_MT       = 0x00000004,
	HFI_DIVX_PROFILE_HT       = 0x00000008,
	HFI_DIVX_PROFILE_HD       = 0x00000010,
	HFI_UNUSED_DIVX_PROFILE = 0x10000000,
};

enum HFI_BUFFER {
	HFI_BUFFER_INPUT,
	HFI_BUFFER_OUTPUT,
	HFI_BUFFER_OUTPUT2,
	HFI_BUFFER_EXTRADATA_INPUT,
	HFI_BUFFER_EXTRADATA_OUTPUT,
	HFI_BUFFER_EXTRADATA_OUTPUT2,
	HFI_BUFFER_INTERNAL_SCRATCH = 0x7F000001,
	HFI_BUFFER_INTERNAL_PERSIST = 0x7F000002,
	HFI_UNUSED_BUFFER = 0x10000000,
};

enum HFI_BUFFER_MODE {
	HFI_BUFFER_MODE_STATIC,
	HFI_BUFFER_MODE_RING,
	HFI_UNUSED_BUFFER_MODE = 0x10000000,
};

enum HFI_FLUSH {
	HFI_FLUSH_INPUT,
	HFI_FLUSH_OUTPUT,
	HFI_FLUSH_OUTPUT2,
	HFI_FLUSH_ALL,
	HFI_UNUSED_FLUSH = 0x10000000,
};

enum HFI_EXTRADATA {
	HFI_EXTRADATA_NONE                 = 0x00000000,
	HFI_EXTRADATA_MB_QUANTIZATION      = 0x00000001,
	HFI_EXTRADATA_INTERLACE_VIDEO      = 0x00000002,
	HFI_EXTRADATA_VC1_FRAMEDISP        = 0x00000003,
	HFI_EXTRADATA_VC1_SEQDISP          = 0x00000004,
	HFI_EXTRADATA_TIMESTAMP            = 0x00000005,
	HFI_EXTRADATA_MULTISLICE_INFO      = 0x7F100000,
	HFI_EXTRADATA_NUM_CONCEALED_MB     = 0x7F100001,
	HFI_EXTRADATA_INDEX                = 0x7F100002,
	HFI_EXTRADATA_METADATA_FILLER      = 0x7FE00002,
	HFI_UNUSED_EXTRADATA = 0x10000000,
};

enum HFI_EXTRADATA_INDEX_TYPE {
	HFI_INDEX_EXTRADATA_INPUT_CROP    = 0x0700000E,
	HFI_INDEX_EXTRADATA_DIGITAL_ZOOM  = 0x07000010,
	HFI_INDEX_EXTRADATA_ASPECT_RATIO  = 0x7F100003,
};

struct hfi_extradata_header {
	u32 size;
	u32 version;
	u32 port_tndex;
	enum HFI_EXTRADATA type;
	u32 data_size;
	u8 rg_data[1];
};

enum HFI_INTERLACE_FORMAT {
	HFI_INTERLACE_FRAME_PROGRESSIVE                 = 0x01,
	HFI_INTERLACE_INTERLEAVE_FRAME_TOPFIELDFIRST    = 0x02,
	HFI_INTERLACE_INTERLEAVE_FRAME_BOTTOMFIELDFIRST = 0x04,
	HFI_INTERLACE_FRAME_TOPFIELDFIRST               = 0x08,
	HFI_INTERLACE_FRAME_BOTTOMFIELDFIRST            = 0x10,
	HFI_UNUSED_INTERLACE = 0x10000000,
};

enum HFI_PROPERTY {
	HFI_PROPERTY_SYS_UNUSED = 0x08000000,
	HFI_PROPERTY_SYS_IDLE_INDICATOR,
	HFI_PROPERTY_SYS_DEBUG_CONFIG,
	HFI_PROPERTY_SYS_RESOURCE_OCMEM_REQUIREMENT_INFO,
	HFI_PROPERTY_PARAM_UNUSED = 0x04000000,
	HFI_PROPERTY_PARAM_BUFFER_COUNT_ACTUAL,
	HFI_PROPERTY_PARAM_FRAME_SIZE,
	HFI_PROPERTY_PARAM_UNCOMPRESSED_FORMAT_SELECT,
	HFI_PROPERTY_PARAM_UNCOMPRESSED_FORMAT_SUPPORTED,
	HFI_PROPERTY_PARAM_UNCOMPRESSED_PLANE_ACTUAL_INFO,
	HFI_PROPERTY_PARAM_UNCOMPRESSED_PLANE_ACTUAL_CONSTRAINTS_INFO,
	HFI_PROPERTY_PARAM_INTERLACE_FORMAT_SUPPORTED,
	HFI_PROPERTY_PARAM_CHROMA_SITE,
	HFI_PROPERTY_PARAM_EXTRA_DATA_HEADER_CONFIG,
	HFI_PROPERTY_PARAM_PROFILE_LEVEL_CURRENT,
	HFI_PROPERTY_PARAM_PROFILE_LEVEL_SUPPORTED,
	HFI_PROPERTY_PARAM_CAPABILITY_SUPPORTED,
	HFI_PROPERTY_PARAM_NAL_STREAM_FORMAT_SUPPORTED,
	HFI_PROPERTY_PARAM_NAL_STREAM_FORMAT_SELECT,
	HFI_PROPERTY_PARAM_MULTI_VIEW_FORMAT,
	HFI_PROPERTY_PARAM_PROPERTIES_SUPPORTED,
	HFI_PROPERTY_PARAM_MAX_SEQUENCE_HEADER_SIZE,
	HFI_PROPERTY_PARAM_CODEC_SUPPORTED,
	HFI_PROPERTY_PARAM_DIVX_FORMAT,

	HFI_PROPERTY_CONFIG_UNUSED = 0x02000000,
	HFI_PROPERTY_CONFIG_BUFFER_REQUIREMENTS,
	HFI_PROPERTY_CONFIG_REALTIME,
	HFI_PROPERTY_CONFIG_PRIORITY,
	HFI_PROPERTY_CONFIG_BATCH_INFO,
	HFI_PROPERTY_CONFIG_FRAME_RATE,

	HFI_PROPERTY_PARAM_VDEC_UNUSED = 0x01000000,
	HFI_PROPERTY_PARAM_VDEC_CONTINUE_DATA_TRANSFER,
	HFI_PROPERTY_PARAM_VDEC_DISPLAY_PICTURE_BUFFER_COUNT,
	HFI_PROPERTY_PARAM_VDEC_MULTI_VIEW_SELECT,
	HFI_PROPERTY_PARAM_VDEC_PICTURE_DECODE,
	HFI_PROPERTY_PARAM_VDEC_MULTI_STREAM,
	HFI_PROPERTY_PARAM_VDEC_OUTPUT_ORDER,
	HFI_PROPERTY_PARAM_VDEC_MB_QUANTIZATION,
	HFI_PROPERTY_PARAM_VDEC_NUM_CONCEALED_MB,
	HFI_PROPERTY_PARAM_VDEC_H264_ENTROPY_SWITCHING,
	HFI_PROPERTY_PARAM_VDEC_OUTPUT2_KEEP_ASPECT_RATIO,

	HFI_PROPERTY_CONFIG_VDEC_UNUSED = 0x00800000,
	HFI_PROPERTY_CONFIG_VDEC_POST_LOOP_DEBLOCKER,
	HFI_PROPERTY_CONFIG_VDEC_MB_ERROR_MAP_REPORTING,
	HFI_PROPERTY_CONFIG_VDEC_MB_ERROR_MAP,

	HFI_PROPERTY_PARAM_VENC_UNUSED = 0x00400000,
	HFI_PROPERTY_PARAM_VENC_SLICE_DELIVERY_MODE,
	HFI_PROPERTY_PARAM_VENC_H264_ENTROPY_CONTROL,
	HFI_PROPERTY_PARAM_VENC_H264_DEBLOCK_CONTROL,
	HFI_PROPERTY_PARAM_VENC_RATE_CONTROL,
	HFI_PROPERTY_PARAM_VENC_TEMPORAL_SPATIAL_TRADEOFF,
	HFI_PROPERTY_PARAM_VENC_SESSION_QP,
	HFI_PROPERTY_PARAM_VENC_MPEG4_AC_PREDICTION,
	HFI_PROPERTY_PARAM_VENC_MPEG4_DATA_PARTITIONING,
	HFI_PROPERTY_PARAM_VENC_MPEG4_TIME_RESOLUTION,
	HFI_PROPERTY_PARAM_VENC_MPEG4_SHORT_HEADER,
	HFI_PROPERTY_PARAM_VENC_MPEG4_HEADER_EXTENSION,
	HFI_PROPERTY_PARAM_VENC_MULTI_SLICE_INFO,
	HFI_PROPERTY_PARAM_VENC_INTRA_REFRESH,
	HFI_PROPERTY_PARAM_VENC_MULTI_SLICE_CONTROL,

	HFI_PROPERTY_CONFIG_VENC_UNUSED = 0x00200000,
	HFI_PROPERTY_CONFIG_VENC_TARGET_BITRATE,
	HFI_PROPERTY_CONFIG_VENC_IDR_PERIOD,
	HFI_PROPERTY_CONFIG_VENC_INTRA_PERIOD,
	HFI_PROPERTY_CONFIG_VENC_REQUEST_IFRAME,
	HFI_PROPERTY_CONFIG_VENC_TIMESTAMP_SCALE,
	HFI_PROPERTY_PARAM_VENC_MPEG4_QPEL,
	HFI_PROPERTY_PARAM_VENC_ADVANCED,

	HFI_PROPERTY_PARAM_VPE_UNUSED = 0x00100000,

	HFI_PROPERTY_CONFIG_VPE_UNUSED = 0x00080000,
	HFI_PROPERTY_CONFIG_VPE_DEINTERLACE,
	HFI_PROPERTY_CONFIG_VPE_OPERATIONS,
	HFI_PROPERTY_UNUSED = 0x10000000,
};

struct hfi_batch_info {
	u32 input_batch_count;
	u32 output_batch_count;
};

struct hfi_bitrate {
	u32 bit_rate;
};

struct hfi_buffer_count_actual {
	enum HFI_BUFFER buffer;
	u32 buffer_count_actual;
};

struct hfi_buffer_requirements {
	enum HFI_BUFFER buffer;
	u32 buffer_size;
	u32 buffer_region_size;
	u32 buffer_hold_count;
	u32 buffer_count_min;
	u32 buffer_count_actual;
	u32 contiguous;
	u32 buffer_alignment;
};

enum HFI_CAPABILITY {
	HFI_CAPABILITY_FRAME_WIDTH,
	HFI_CAPABILITY_FRAME_HEIGHT,
	HFI_CAPABILITY_MBS_PER_FRAME,
	HFI_CAPABILITY_MBS_PER_SECOND,
	HFI_CAPABILITY_FRAMERATE,
	HFI_CAPABILITY_SCALE_X,
	HFI_CAPABILITY_SCALE_Y,
	HFI_CAPABILITY_BITRATE,
	HFI_UNUSED_CAPABILITY = 0x10000000,
};

struct hfi_capability_supported {
	enum HFI_CAPABILITY eCapabilityType;
	u32 min;
	u32 max;
	u32 step_size;
};

struct hfi_capability_supported_INFO {
	u32 num_capabilities;
	struct hfi_capability_supported rg_data[1];
};

enum HFI_CHROMA_SITE {
	HFI_CHROMA_SITE_0,
	HFI_CHROMA_SITE_1,
	HFI_UNUSED_CHROMA = 0x10000000,
};

struct hfi_data_payload {
	u32 size;
	u8 rg_data[1];
};

struct hfi_seq_header_info {
	u32 max_header_len;
};

struct hfi_enable_picture {
	u32 picture_type;
};

struct hfi_display_picture_buffer_count {
	int enable;
	u32 count;
};

struct hfi_enable {
	int enable;
};

enum HFI_H264_DB_MODE {
	HFI_H264_DB_MODE_DISABLE,
	HFI_H264_DB_MODE_SKIP_SLICE_BOUNDARY,
	HFI_H264_DB_MODE_ALL_BOUNDARY,
	HFI_UNUSED_H264_DB = 0x10000000,
};

struct hfi_h264_db_control {
	enum HFI_H264_DB_MODE mode;
	int slice_alpha_offset;
	int slice_beta_offset;
};

enum HFI_H264_ENTROPY {
	HFI_H264_ENTROPY_CAVLC,
	HFI_H264_ENTROPY_CABAC,
	HFI_UNUSED_ENTROPY = 0x10000000,
};

enum HFI_H264_CABAC_MODEL {
	HFI_H264_CABAC_MODEL_0,
	HFI_H264_CABAC_MODEL_1,
	HFI_H264_CABAC_MODEL_2,
	HFI_UNUSED_CABAC = 0x10000000,
};

struct hfi_h264_entropy_control {
	enum HFI_H264_ENTROPY entropy_mode;
	enum HFI_H264_CABAC_MODEL cabac_model;
};

struct hfi_extra_data_header_config {
	u32 type;
	enum HFI_BUFFER buffer_type;
	u32 version;
	u32 port_index;
	u32 client_extradata_id;
};

struct hfi_frame_rate {
	enum HFI_BUFFER buffer_type;
	u32 frame_rate;
};

struct hfi_interlace_format_supported {
	enum HFI_BUFFER buffer;
	enum HFI_INTERLACE_FORMAT format;
};

enum hfi_intra_refresh_mode {
	HFI_INTRA_REFRESH_NONE,
	HFI_INTRA_REFRESH_CYCLIC,
	HFI_INTRA_REFRESH_ADAPTIVE,
	HFI_INTRA_REFRESH_CYCLIC_ADAPTIVE,
	HFI_INTRA_REFRESH_RANDOM,
	HFI_UNUSED_INTRA = 0x10000000,
};

struct hfi_intra_refresh {
	enum hfi_intra_refresh_mode mode;
	u32 air_mbs;
	u32 air_ref;
	u32 cir_mbs;
};

struct hfi_idr_period {
	u32 idr_period;
};

struct hfi_intra_period {
	u32 pframes;
	u32 bframes;
};

struct hfi_timestamp_scale {
	u32 time_stamp_scale;
};

struct hfi_mb_error_map {
	u32 error_map_size;
	u8 rg_error_map[1];
};

struct hfi_metadata_pass_through {
	int enable;
	u32 size;
};

struct hfi_mpeg4_header_extension {
	u32 header_extension;
};

struct hfi_mpeg4_time_resolution {
	u32 time_increment_resolution;
};

enum HFI_MULTI_SLICE {
	HFI_MULTI_SLICE_OFF,
	HFI_MULTI_SLICE_BY_MB_COUNT,
	HFI_MULTI_SLICE_BY_BYTE_COUNT,
	HFI_MULTI_SLICE_GOB,
	HFI_UNUSED_SLICE = 0x10000000,
};

struct hfi_multi_slice_control {
	enum HFI_MULTI_SLICE multi_slice;
	u32 slice_size;
};

struct hfi_multi_stream {
	enum HFI_BUFFER buffer;
	u32 enable;
	u32 width;
	u32 height;
};

struct hfi_multi_view_format {
	u32 views;
	u32 rg_view_order[1];
};

struct hfi_multi_view_select {
	u32 view_index;
};

enum HFI_NAL_STREAM_FORMAT {
	HFI_NAL_FORMAT_STARTCODES         = 0x00000001,
	HFI_NAL_FORMAT_ONE_NAL_PER_BUFFER = 0x00000002,
	HFI_NAL_FORMAT_ONE_BYTE_LENGTH    = 0x00000004,
	HFI_NAL_FORMAT_TWO_BYTE_LENGTH    = 0x00000008,
	HFI_NAL_FORMAT_FOUR_BYTE_LENGTH   = 0x00000010,
	HFI_UNUSED_NAL = 0x10000000,
};

struct hfi_nal_stream_format_supported {
	u32 nal_stream_format_supported;
};

enum HFI_PICTURE {
	HFI_PICTURE_I   = 0x01,
	HFI_PICTURE_P   = 0x02,
	HFI_PICTURE_B   = 0x04,
	HFI_PICTURE_IDR = 0x7F001000,
	HFI_UNUSED_PICT = 0x10000000,
};

enum HFI_PRIORITY {
	HFI_PRIORITY_LOW = 10,
	HFI_PRIOIRTY_MEDIUM = 20,
	HFI_PRIORITY_HIGH = 30,
	HFI_UNUSED_PRIORITY = 0x10000000,
};

struct hfi_profile_level {
	u32 profile;
	u32 level;
};

struct hfi_profile_level_supported {
	u32 profile_count;
	struct hfi_profile_level rg_profile_level[1];
};

enum HFI_ROTATE {
	HFI_ROTATE_NONE,
	HFI_ROTATE_90,
	HFI_ROTATE_180,
	HFI_ROTATE_270,
	HFI_UNUSED_ROTATE = 0x10000000,
};

enum HFI_FLIP {
	HFI_FLIP_NONE,
	HFI_FLIP_HORIZONTAL,
	HFI_FLIP_VERTICAL,
	HFI_UNUSED_FLIP = 0x10000000,
};

struct hfi_operations {
	enum HFI_ROTATE rotate;
	enum HFI_FLIP flip;
};

enum HFI_OUTPUT_ORDER {
	HFI_OUTPUT_ORDER_DISPLAY,
	HFI_OUTPUT_ORDER_DECODE,
	HFI_UNUSED_OUTPUT = 0x10000000,
};

struct hfi_quantization {
	u32 qp_i;
	u32 qp_p;
	u32 qp_b;
};

enum HFI_RATE_CONTROL {
	HFI_RATE_CONTROL_OFF,
	HFI_RATE_CONTROL_VBR_VFR,
	HFI_RATE_CONTROL_VBR_CFR,
	HFI_RATE_CONTROL_CBR_VFR,
	HFI_RATE_CONTROL_CBR_CFR,
	HFI_UNUSED_RC = 0x10000000,
};

struct hfi_slice_delivery_mode {
	int enable;
};

struct hfi_temporal_spatial_tradeoff {
	u32 ts_factor;
};

struct hfi_frame_size {
	enum HFI_BUFFER buffer;
	u32 width;
	u32 height;
};

enum HFI_UNCOMPRESSED_FORMAT {
	HFI_COLOR_FORMAT_MONOCHROME,
	HFI_COLOR_FORMAT_NV12,
	HFI_COLOR_FORMAT_NV21,
	HFI_COLOR_FORMAT_NV12_4x4TILE,
	HFI_COLOR_FORMAT_NV21_4x4TILE,
	HFI_COLOR_FORMAT_YUYV,
	HFI_COLOR_FORMAT_YVYU,
	HFI_COLOR_FORMAT_UYVY,
	HFI_COLOR_FORMAT_VYUY,
	HFI_COLOR_FORMAT_RGB565,
	HFI_COLOR_FORMAT_BGR565,
	HFI_COLOR_FORMAT_RGB888,
	HFI_COLOR_FORMAT_BGR888,
	HFI_UNUSED_COLOR = 0x10000000,
};

struct hfi_uncompressed_format_select {
	enum HFI_BUFFER buffer;
	enum HFI_UNCOMPRESSED_FORMAT format;
};

struct hfi_uncompressed_format_supported {
	enum HFI_BUFFER buffer;
	u32 format_entries;
	u32 rg_format_info[1];
};

struct hfi_uncompressed_plane_actual {
	int actual_stride;
	u32 actual_plane_buffer_height;
};

struct hfi_uncompressed_plane_actual_info {
	enum HFI_BUFFER buffer;
	u32 num_planes;
	struct hfi_uncompressed_plane_actual rg_plane_format[1];
};

struct hfi_uncompressed_plane_constraints {
	u32 stride_multiples;
	u32 max_stride;
	u32 min_plane_buffer_height_multiple;
	u32 buffer_alignment;
};

struct hfi_uncompressed_plane_info {
	enum HFI_UNCOMPRESSED_FORMAT format;
	u32 num_planes;
	struct hfi_uncompressed_plane_constraints rg_plane_format[1];
};

struct hfi_uncompressed_plane_actual_constraints_info {
	enum HFI_BUFFER buffer;
	u32 num_planes;
	struct hfi_uncompressed_plane_constraints rg_plane_format[1];
};

struct hfi_codec_supported {
	u32 decoder_codec_supported;
	u32 encoder_codec_supported;
};

enum HFI_DEBUG_MSG {
	HFI_DEBUG_MSG_LOW     = 0x00000001,
	HFI_DEBUG_MSG_MEDIUM  = 0x00000002,
	HFI_DEBUG_MSG_HIGH    = 0x00000004,
	HFI_DEBUG_MSG_ERROR   = 0x00000008,
	HFI_DEBUG_MSG_FATAL   = 0x00000010,
	HFI_UNUSED_DEBUG_MSG = 0x10000000,
};

struct hfi_debug_config {
	u32 debug_config;
};

struct hfi_properties_supported {
	u32 num_properties;
	u32 rg_properties[1];
};

enum HFI_RESOURCE {
	HFI_RESOURCE_OCMEM    = 0x00000001,
	HFI_UNUSED_RESOURCE = 0x10000000,
};

struct hfi_resource_ocmem_type {
	u32 size;
	u8 *mem;
};

struct hfi_resource_ocmem_requirement {
	enum HFI_DOMAIN session_domain;
	u32 width;
	u32 height;
	u32 size;
};

struct hfi_resource_ocmem_requirement_info {
	u32 num_entries;
	struct hfi_resource_ocmem_requirement rg_requirements[1];
};

struct hfi_venc_config_advanced {
	u8 pipe2d;
	u8 hw_mode;
	u8 low_delay_enforce;
	int h264_constrain_intra_pred;
	int h264_transform_8x8_flag;
	int mpeg4_qpel_enable;
	int multi_refP_en;
	int qmatrix_en;
	u8 vpp_info_packet_mode;
	u8 ref_tile_mode;
	u8 bitstream_flush_mode;
	u32 ds_display_frame_width;
	u32 ds_display_frame_height;
	u32 perf_tune_param_ptr;
};

enum HFI_COMMAND {
	HFI_CMD_SYS_UNUSED = 0x01000000,
	HFI_CMD_SYS_INIT,
	HFI_CMD_SYS_SESSION_INIT,
	HFI_CMD_SYS_SESSION_END,
	HFI_CMD_SYS_SESSION_ABORT,
	HFI_CMD_SYS_SET_RESOURCE,
	HFI_CMD_SYS_RELEASE_RESOURCE,
	HFI_CMD_SYS_PING,
	HFI_CMD_SYS_PC_PREP,
	HFI_CMD_SYS_SET_PROPERTY,
	HFI_CMD_SYS_GET_PROPERTY,

	HFI_CMD_SESSION_UNUSED = 0x02000000,
	HFI_CMD_SESSION_LOAD_RESOURCES,
	HFI_CMD_SESSION_START,
	HFI_CMD_SESSION_STOP,
	HFI_CMD_SESSION_EMPTY_BUFFER,
	HFI_CMD_SESSION_FILL_BUFFER,
	HFI_CMD_SESSION_FLUSH,
	HFI_CMD_SESSION_SUSPEND,
	HFI_CMD_SESSION_RESUME,
	HFI_CMD_SESSION_SET_PROPERTY,
	HFI_CMD_SESSION_GET_PROPERTY,
	HFI_CMD_SESSION_PARSE_SEQUENCE_HEADER,
	HFI_CMD_SESSION_GET_SEQUENCE_HEADER,
	HFI_CMD_SESSION_SET_BUFFERS,
	HFI_CMD_SESSION_RELEASE_BUFFERS,
	HFI_CMD_SESSION_RELEASE_RESOURCES,

	HFI_CMD_UNUSED = 0x10000000,
};

enum HFI_MESSAGE {
	HFI_MSG_SYS_UNUSED = 0x01000000,
	HFI_MSG_SYS_IDLE,
	HFI_MSG_SYS_PC_PREP_DONE,
	HFI_MSG_SYS_RELEASE_RESOURCE,
	HFI_MSG_SYS_PING_ACK,
	HFI_MSG_SYS_DEBUG,
	HFI_MSG_SYS_INIT_DONE,
	HFI_MSG_SYS_PROPERTY_INFO,
	HFI_MSG_SESSION_UNUSED = 0x02000000,
	HFI_MSG_EVENT_NOTIFY,
	HFI_MSG_SYS_SESSION_INIT_DONE,
	HFI_MSG_SYS_SESSION_END_DONE,
	HFI_MSG_SYS_SESSION_ABORT_DONE,
	HFI_MSG_SESSION_LOAD_RESOURCES_DONE,
	HFI_MSG_SESSION_START_DONE,
	HFI_MSG_SESSION_STOP_DONE,
	HFI_MSG_SESSION_SUSPEND_DONE,
	HFI_MSG_SESSION_RESUME_DONE,
	HFI_MSG_SESSION_EMPTY_BUFFER_DONE,
	HFI_MSG_SESSION_FILL_BUFFER_DONE,
	HFI_MSG_SESSION_FLUSH_DONE,
	HFI_MSG_SESSION_PROPERTY_INFO,
	HFI_MSG_SESSION_RELEASE_RESOURCES_DONE,
	HFI_MSG_SESSION_PARSE_SEQUENCE_HEADER_DONE,
	HFI_MSG_SESSION_GET_SEQUENCE_HEADER_DONE,
	HFI_MSG_UNUSED = 0x10000000,
};

struct vidc_hal_msg_pkt_hdr {
	u32 size;
	enum HFI_MESSAGE packet;
};

struct vidc_hal_session_cmd_pkt {
	u32 size;
	enum HFI_COMMAND packet_type;
	u32 session_id;
};

enum HFI_STATUS {
	HFI_FAIL = 0,
	HFI_SUCCESS,
	HFI_UNUSED_STATUS = 0x10000000,
};

struct hfi_cmd_sys_init_packet {
	u32 size;
	enum HFI_COMMAND packet;
};

struct hfi_cmd_sys_session_init_packet {
	u32 size;
	enum HFI_COMMAND packet;
	u32 session_id;
	enum HFI_DOMAIN session_domain;
	enum HFI_VIDEO_CODEC session_codec;
};

struct hfi_cmd_sys_session_end_packet {
	u32 size;
	enum HFI_COMMAND packet_type;
	u32 session_id;
};

struct hfi_cmd_sys_session_abort_packet {
	u32 size;
	enum HFI_COMMAND packet_type;
	u32 session_id;
};

struct hfi_cmd_sys_pc_prep_packet {
	u32 size;
	enum HFI_COMMAND packet_type;
};

struct hfi_cmd_sys_set_resource_packet {
	u32 size;
	enum HFI_COMMAND packet_type;
	u32 resource_handle;
	enum HFI_RESOURCE resource_type;
	u32 rg_resource_data[1];
};

struct hfi_cmd_sys_release_resource_packet {
	u32 size;
	enum HFI_COMMAND packet_type;
	enum HFI_RESOURCE resource_type;
	u32 resource_handle;
};

struct hfi_cmd_sys_ping_packet {
	u32 size;
	enum HFI_COMMAND packet_type;
	u32 client_data;
};

struct hfi_cmd_sys_set_property_packet {
	u32 size;
	enum HFI_COMMAND packet_type;
	u32 num_properties;
	u32 rg_property_data[1];
};

struct hfi_cmd_sys_get_property_packet {
	u32 size;
	enum HFI_COMMAND packet_type;
	u32 num_properties;
	enum HFI_PROPERTY rg_property_data[1];
};

struct hfi_cmd_session_load_resources_packet {
	u32 size;
	enum HFI_COMMAND packet_type;
	u32 session_id;
};

struct hfi_cmd_session_start_packet {
	u32 size;
	enum HFI_COMMAND packet_type;
	u32 session_id;
};

struct hfi_cmd_session_stop_packet {
	u32 size;
	enum HFI_COMMAND packet_type;
	u32 session_id;
};

struct hfi_cmd_session_empty_buffer_compressed_packet {
	u32 size;
	enum HFI_COMMAND packet_type;
	u32 session_id;
	u32 timestamp_hi;
	u32 timestamp_lo;
	u32 flags;
	u32 mark_target;
	u32 mark_data;
	u32 offset;
	u32 alloc_len;
	u32 filled_len;
	u32 input_tag;
	u8 *packet_buffer;
};

struct hfi_cmd_session_empty_buffer_uncompressed_plane0_packet {
	u32 size;
	enum HFI_COMMAND packet;
	u32 session_id;
	u32 view_id;
	u32 timestamp_hi;
	u32 timestamp_lo;
	u32 flags;
	u32 mark_target;
	u32 mark_data;
	u32 alloc_len;
	u32 filled_len;
	u32 offset;
	u32 input_tag;
	u8 *packet_buffer;
};

struct hfi_cmd_session_empty_buffer_uncompressed_plane1_packet {
	u32 flags;
	u32 alloc_len;
	u32 filled_len;
	u32 offset;
	u8 *packet_buffer2;
};

struct hfi_cmd_session_empty_buffer_uncompressed_plane2_packet {
	u32 flags;
	u32 alloc_len;
	u32 filled_len;
	u32 offset;
	u8 *packet_buffer3;
};

struct hfi_cmd_session_fill_buffer_packet {
	u32 size;
	enum HFI_COMMAND packet_type;
	u32 session_id;
	u32 stream_id;
	u8 *packet_buffer;
	u8 *extra_data_buffer;
};

struct hfi_cmd_session_flush_packet {
	u32 size;
	enum HFI_COMMAND packet_type;
	u32 session_id;
	enum HFI_FLUSH flush_type;
};

struct hfi_cmd_session_suspend_packet {
	u32 size;
	enum HFI_COMMAND packet;
	u32 session_id;
};

struct hfi_cmd_session_resume_packet {
	u32 size;
	enum HFI_COMMAND packet_type;
	u32 session_id;
};

struct hfi_cmd_session_set_property_packet {
	u32 size;
	enum HFI_COMMAND packet_type;
	u32 session_id;
	u32 num_properties;
	u32 rg_property_data[1];
};

struct hfi_cmd_session_get_property_packet {
	u32 size;
	enum HFI_COMMAND packet_type;
	u32 session_id;
	u32 num_properties;
	enum HFI_PROPERTY rg_property_data[1];
};

struct hfi_buffer_info {
	u32 buffer_addr;
	u32 extradata_addr;
};

struct hfi_cmd_session_set_buffers_packet {
	u32 size;
	enum HFI_COMMAND packet_type;
	u32 session_id;
	enum HFI_BUFFER buffer_type;
	enum HFI_BUFFER_MODE buffer_mode;
	u32 buffer_size;
	u32 extradata_size;
	u32 min_buffer_size;
	u32 num_buffers;
	u32 rg_buffer_info[1];
};

struct hfi_cmd_session_release_buffer_packet {
	u32 size;
	enum HFI_COMMAND packet_type;
	u32 session_id;
	enum HFI_BUFFER buffer_type;
	u32 buffer_size;
	u32 extradata_size;
	u32 num_buffers;
	u32 rg_buffer_info[1];
};

struct hfi_cmd_session_release_resources_packet {
	u32 size;
	enum HFI_COMMAND packet_type;
	u32 session_id;
};

struct hfi_cmd_session_parse_sequence_header_packet {
	u32 size;
	enum HFI_COMMAND packet_type;
	u32 session_id;
	u32 header_len;
	u8 *packet_buffer;
};

struct hfi_cmd_session_get_sequence_header_packet {
	u32 size;
	enum HFI_COMMAND packet_type;
	u32 session_id;
	u32 buffer_len;
	u8 *packet_buffer;
};

struct hfi_msg_event_notify_packet {
	u32 size;
	enum HFI_MESSAGE packet_type;
	u32 session_id;
	enum HFI_EVENT event_id;
	u32 event_data1;
	u32 event_data2;
	u32 rg_ext_event_data[1];
};

struct hfi_msg_sys_init_done_packet {
	u32 size;
	enum HFI_MESSAGE packet_type;
	enum HFI_ERROR error_type;
	u32 num_properties;
	u32 rg_property_data[1];
};

struct hfi_msg_sys_session_init_done_packet {
	u32 size;
	enum HFI_MESSAGE packet_type;
	u32 session_id;
	enum HFI_ERROR error_type;
	u32 num_properties;
	u32 rg_property_data[1];
};

struct hfi_msg_sys_session_end_done_packet {
	u32 size;
	enum HFI_MESSAGE packet_type;
	u32 session_id;
	enum HFI_ERROR error_type;
};

struct hfi_msg_sys_session_abort_done_packet {
	u32 size;
	enum HFI_MESSAGE packet_type;
	u32 session_id;
	enum HFI_ERROR error_type;
};

struct hfi_msg_sys_idle_packet {
	u32 size;
	enum HFI_MESSAGE packet_type;
};

struct hfi_msg_sys_pc_prep_done_packet {
	u32 size;
	enum HFI_MESSAGE packet_type;
	enum HFI_ERROR error_type;
};

struct hfi_msg_sys_release_resource_done_packet {
	u32 size;
	enum HFI_MESSAGE packet_type;
	u32 resource_handle;
	enum HFI_ERROR error_type;
};

struct hfi_msg_sys_ping_ack_packet {
	u32 size;
	enum HFI_MESSAGE packet_type;
	u32 client_data;
};

struct hfi_msg_sys_debug_packet {
	u32 size;
	enum HFI_MESSAGE packet_type;
	enum HFI_DEBUG_MSG msg_type;
	u32 msg_size;
	u32 timestamp_hi;
	u32 timestamp_lo;
	u8 rg_msg_data[1];
};

struct hfi_msg_sys_property_info_packet {
	u32 nsize;
	enum HFI_MESSAGE packet_type;
	u32 num_properties;
	u32 rg_property_data[1];
};

struct hfi_msg_session_load_resources_done_packet {
	u32 size;
	enum HFI_MESSAGE packet_type;
	u32 session_id;
	enum HFI_ERROR error_type;
};

struct hfi_msg_session_start_done_packet {
	u32 size;
	enum HFI_MESSAGE packet_type;
	u32 session_id;
	enum HFI_ERROR error_type;
};

struct hfi_msg_session_stop_done_packet {
	u32 size;
	enum HFI_MESSAGE packet_type;
	u32 session_id;
	enum HFI_ERROR error_type;
};

struct hfi_msg_session_suspend_done_packet {
	u32 size;
	enum HFI_MESSAGE packet_type;
	u32 session_id;
	enum HFI_ERROR error_type;
};

struct hfi_msg_session_resume_done_packet {
	u32 size;
	enum HFI_MESSAGE packet_type;
	u32 session_id;
	enum HFI_ERROR error_type;
};

struct hfi_msg_session_empty_buffer_done_packet {
	u32 size;
	enum HFI_MESSAGE packet_type;
	u32 session_id;
	enum HFI_ERROR error_type;
	u32 offset;
	u32 filled_len;
	u32 input_tag;
	u8 *packet_buffer;
};

struct hfi_msg_session_fill_buffer_done_compressed_packet {
	u32 size;
	enum HFI_MESSAGE packet_type;
	u32 session_id;
	u32 timestamp_hi;
	u32 timestamp_lo;
	enum HFI_ERROR error_type;
	u32 flags;
	u32 mark_target;
	u32 mark_data;
	u32 stats;
	u32 offset;
	u32 alloc_len;
	u32 filled_len;
	u32 input_tag;
	enum HFI_PICTURE picture_type;
	u8 *packet_buffer;
	u8 *extra_data_buffer;
};

struct hfi_msg_session_fbd_uncompressed_plane0_packet {
	u32 size;
	enum HFI_MESSAGE packet_type;
	u32 session_id;
	u32 stream_id;
	u32 view_id;
	enum HFI_ERROR error_type;
	u32 timestamp_hi;
	u32 timestamp_lo;
	u32 flags;
	u32 mark_target;
	u32 mark_data;
	u32 stats;
	u32 alloc_len;
	u32 filled_len;
	u32 oofset;
	u32 frame_width;
	u32 frame_height;
	u32 start_xCoord;
	u32 start_yCoord;
	u32 input_tag;
	u32 input_tag1;
	enum HFI_PICTURE picture_type;
	u8 *packet_buffer;
	u8 *extra_data_buffer;
};

struct hfi_msg_session_fill_buffer_done_uncompressed_plane1_packet {
	u32 flags;
	u32 alloc_len;
	u32 filled_len;
	u32 offset;
	u8 *packet_buffer;
};

struct hfi_msg_session_fill_buffer_done_uncompressed_plane2_packet {
	u32 flags;
	u32 alloc_len;
	u32 filled_len;
	u32 offset;
	u8 *packet_buffer;
};

struct hfi_msg_session_flush_done_packet {
	u32 size;
	enum HFI_MESSAGE packet_type;
	u32 session_id;
	enum HFI_ERROR error_type;
	enum HFI_FLUSH flush_type;
};

struct hfi_msg_session_parse_sequence_header_done_packet {
	u32 size;
	enum HFI_MESSAGE packet_type;
	u32 session_id;
	enum HFI_ERROR error_type;
	u32 num_properties;
	u32 rg_property_data[1];
};

struct hfi_msg_session_get_sequence_header_done_packet {
	u32 size;
	enum HFI_MESSAGE packet_type;
	u32 session_id;
	enum HFI_ERROR error_type;
	u32 header_len;
	u8 *sequence_header;
};

struct hfi_msg_session_property_info_packet {
	u32 size;
	enum HFI_MESSAGE packet_type;
	u32 session_id;
	u32 num_properties;
	u32 rg_property_data[1];
};

struct hfi_msg_session_release_resources_done_packet {
	u32 size;
	enum HFI_MESSAGE packet_type;
	u32 session_id;
	enum HFI_ERROR error_type;
};

struct hfi_extradata_mb_quantization_payload {
	u8 rg_mb_qp[1];
};

struct hfi_extradata_vc1_pswnd {
	u32 ps_wnd_h_offset;
	u32 ps_wndv_offset;
	u32 ps_wnd_width;
	u32 ps_wnd_height;
};

struct hfi_extradata_vc1_framedisp_payload {
	u32 res_pic;
	u32 ref;
	u32 range_map_present;
	u32 range_map_y;
	u32 range_map_uv;
	u32 num_pan_scan_wnds;
	struct hfi_extradata_vc1_pswnd rg_ps_wnd[1];
};

struct hfi_extradata_vc1_seqdisp_payload {
	u32 prog_seg_frm;
	u32 uv_sampling_fmt;
	u32 color_fmt_flag;
	u32 color_primaries;
	u32 transfer_char;
	u32 mat_coeff;
	u32 aspect_ratio;
	u32 aspect_horiz;
	u32 aspect_vert;
};

struct hfi_extradata_timestamp_payload {
	u32 timestamp_low;
	u32 timestamp_high;
};

struct hfi_extradata_interlace_video_payload {
	enum HFI_INTERLACE_FORMAT format;
};

enum HFI_S3D_FP_LAYOUT {
	HFI_S3D_FP_LAYOUT_NONE,
	HFI_S3D_FP_LAYOUT_INTRLV_CHECKERBOARD,
	HFI_S3D_FP_LAYOUT_INTRLV_COLUMN,
	HFI_S3D_FP_LAYOUT_INTRLV_ROW,
	HFI_S3D_FP_LAYOUT_SIDEBYSIDE,
	HFI_S3D_FP_LAYOUT_TOPBOTTOM,
	HFI_S3D_FP_LAYOUT_UNUSED = 0x10000000,
};

enum HFI_S3D_FP_VIEW_ORDER {
	HFI_S3D_FP_LEFTVIEW_FIRST,
	HFI_S3D_FP_RIGHTVIEW_FIRST,
	HFI_S3D_FP_UNKNOWN,
	HFI_S3D_FP_VIEWORDER_UNUSED = 0x10000000,
};

enum HFI_S3D_FP_FLIP {
	HFI_S3D_FP_FLIP_NONE,
	HFI_S3D_FP_FLIP_LEFT_HORIZ,
	HFI_S3D_FP_FLIP_LEFT_VERT,
	HFI_S3D_FP_FLIP_RIGHT_HORIZ,
	HFI_S3D_FP_FLIP_RIGHT_VERT,
	HFI_S3D_FP_FLIP_UNUSED = 0x10000000,
};

struct hfi_extradata_s3d_frame_packing_payload {
	enum HFI_S3D_FP_LAYOUT eLayout;
	enum HFI_S3D_FP_VIEW_ORDER eOrder;
	enum HFI_S3D_FP_FLIP eFlip;
	int bQuinCunx;
	u32 nLeftViewLumaSiteX;
	u32 nLeftViewLumaSiteY;
	u32 nRightViewLumaSiteX;
	u32 nRightViewLumaSiteY;
};

struct hfi_extradata_num_concealed_mb_payload {
	u32 num_mb_concealed;
};

struct hfi_extradata_sliceinfo {
	u32 offset_in_stream;
	u32 slice_length;
};

struct hfi_extradata_multislice_info_payload {
	u32 num_slices;
	struct hfi_extradata_sliceinfo rg_slice_info[1];
};

struct hfi_index_extradata_input_crop_payload {
	u32 size;
	u32 version;
	u32 port_index;
	u32 left;
	u32 top;
	u32 width;
	u32 height;
};

struct hfi_index_extradata_digital_zoom_payload {
	u32 size;
	u32 version;
	u32 port_index;
	int width;
	int height;
};

struct vidc_mem_addr {
	u8 *align_device_addr;
	u8 *align_virtual_addr;
	u32 mem_size;
	struct msm_smem *mem_data;
};

struct vidc_iface_q_info {
	void *q_hdr;
	struct vidc_mem_addr q_array;
};

/* Internal data used in vidc_hal not exposed to msm_vidc*/

struct hal_data {
	u32 irq;
	u8 *device_base_addr;
	u8 *register_base_addr;
};

struct hal_device {
	struct list_head list;
	struct list_head sess_head;
	u32 intr_status;
	u32 device_id;
	spinlock_t read_lock;
	spinlock_t write_lock;
	void (*callback) (u32 response, void *callback);
	struct vidc_mem_addr iface_q_table;
	struct vidc_iface_q_info iface_queues[VIDC_IFACEQ_NUMQ];
	struct smem_client *hal_client;
	struct hal_data *hal_data;
	struct workqueue_struct *vidc_workq;
	int spur_count;
	int reg_count;
};

struct hal_session {
	struct list_head list;
	u32 session_id;
	u32 is_decoder;
	struct hal_device *device;
};

struct hal_device_data {
	struct list_head dev_head;
	int dev_count;
};

extern struct hal_device_data hal_ctxt;

int vidc_hal_iface_msgq_read(struct hal_device *device, void *pkt);
int vidc_hal_iface_dbgq_read(struct hal_device *device, void *pkt);

/* Interrupt Processing:*/
void vidc_hal_response_handler(struct hal_device *device);

#endif /*__VIDC_HAL_H__ */
