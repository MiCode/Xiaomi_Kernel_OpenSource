/* Copyright (c) 2012-2016, 2018 The Linux Foundation. All rights reserved.
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
#ifndef __H_VIDC_HFI_H__
#define __H_VIDC_HFI_H__

#include <media/msm_media_info.h>
#include "vidc_hfi_helper.h"
#include "vidc_hfi_api.h"

#define HFI_EVENT_SESSION_SEQUENCE_CHANGED (HFI_OX_BASE + 0x3)
#define HFI_EVENT_SESSION_PROPERTY_CHANGED (HFI_OX_BASE + 0x4)
#define HFI_EVENT_SESSION_LTRUSE_FAILED (HFI_OX_BASE + 0x5)
#define HFI_EVENT_RELEASE_BUFFER_REFERENCE (HFI_OX_BASE + 0x6)

#define HFI_EVENT_DATA_SEQUENCE_CHANGED_SUFFICIENT_BUFFER_RESOURCES	\
	(HFI_OX_BASE + 0x1)
#define HFI_EVENT_DATA_SEQUENCE_CHANGED_INSUFFICIENT_BUFFER_RESOURCES	\
	(HFI_OX_BASE + 0x2)

#define HFI_BUFFERFLAG_EOS			0x00000001
#define HFI_BUFFERFLAG_STARTTIME		0x00000002
#define HFI_BUFFERFLAG_DECODEONLY		0x00000004
#define HFI_BUFFERFLAG_DATACORRUPT		0x00000008
#define HFI_BUFFERFLAG_ENDOFFRAME		0x00000010
#define HFI_BUFFERFLAG_SYNCFRAME		0x00000020
#define HFI_BUFFERFLAG_EXTRADATA		0x00000040
#define HFI_BUFFERFLAG_CODECCONFIG		0x00000080
#define HFI_BUFFERFLAG_TIMESTAMPINVALID		0x00000100
#define HFI_BUFFERFLAG_READONLY			0x00000200
#define HFI_BUFFERFLAG_ENDOFSUBFRAME		0x00000400
#define HFI_BUFFERFLAG_EOSEQ			0x00200000
#define HFI_BUFFER_FLAG_MBAFF			0x08000000
#define HFI_BUFFERFLAG_VPE_YUV_601_709_CSC_CLAMP \
						0x10000000
#define HFI_BUFFERFLAG_DROP_FRAME               0x20000000
#define HFI_BUFFERFLAG_TEI			0x40000000
#define HFI_BUFFERFLAG_DISCONTINUITY		0x80000000


#define HFI_ERR_SESSION_EMPTY_BUFFER_DONE_OUTPUT_PENDING	\
	(HFI_OX_BASE + 0x1001)
#define HFI_ERR_SESSION_SAME_STATE_OPERATION		\
	(HFI_OX_BASE + 0x1002)
#define HFI_ERR_SESSION_SYNC_FRAME_NOT_DETECTED		\
	(HFI_OX_BASE + 0x1003)
#define  HFI_ERR_SESSION_START_CODE_NOT_FOUND		\
	(HFI_OX_BASE + 0x1004)

#define HFI_BUFFER_INTERNAL_SCRATCH (HFI_OX_BASE + 0x1)
#define HFI_BUFFER_EXTRADATA_INPUT (HFI_OX_BASE + 0x2)
#define HFI_BUFFER_EXTRADATA_OUTPUT (HFI_OX_BASE + 0x3)
#define HFI_BUFFER_EXTRADATA_OUTPUT2 (HFI_OX_BASE + 0x4)
#define HFI_BUFFER_INTERNAL_SCRATCH_1 (HFI_OX_BASE + 0x5)
#define HFI_BUFFER_INTERNAL_SCRATCH_2 (HFI_OX_BASE + 0x6)

#define HFI_BUFFER_MODE_STATIC (HFI_OX_BASE + 0x1)
#define HFI_BUFFER_MODE_RING (HFI_OX_BASE + 0x2)
#define HFI_BUFFER_MODE_DYNAMIC (HFI_OX_BASE + 0x3)

#define HFI_FLUSH_INPUT (HFI_OX_BASE + 0x1)
#define HFI_FLUSH_OUTPUT (HFI_OX_BASE + 0x2)
#define HFI_FLUSH_ALL (HFI_OX_BASE + 0x4)

#define HFI_EXTRADATA_NONE					0x00000000
#define HFI_EXTRADATA_MB_QUANTIZATION		0x00000001
#define HFI_EXTRADATA_INTERLACE_VIDEO		0x00000002
#define HFI_EXTRADATA_VC1_FRAMEDISP			0x00000003
#define HFI_EXTRADATA_VC1_SEQDISP			0x00000004
#define HFI_EXTRADATA_TIMESTAMP				0x00000005
#define HFI_EXTRADATA_S3D_FRAME_PACKING		0x00000006
#define HFI_EXTRADATA_FRAME_RATE			0x00000007
#define HFI_EXTRADATA_PANSCAN_WINDOW		0x00000008
#define HFI_EXTRADATA_RECOVERY_POINT_SEI	0x00000009
#define HFI_EXTRADATA_MPEG2_SEQDISP		0x0000000D
#define HFI_EXTRADATA_STREAM_USERDATA		0x0000000E
#define HFI_EXTRADATA_FRAME_QP			0x0000000F
#define HFI_EXTRADATA_FRAME_BITS_INFO		0x00000010
#define HFI_EXTRADATA_VPX_COLORSPACE		0x00000014
#define HFI_EXTRADATA_MULTISLICE_INFO		0x7F100000
#define HFI_EXTRADATA_NUM_CONCEALED_MB		0x7F100001
#define HFI_EXTRADATA_INDEX					0x7F100002
#define HFI_EXTRADATA_METADATA_LTR			0x7F100004
#define HFI_EXTRADATA_METADATA_FILLER		0x7FE00002

#define HFI_INDEX_EXTRADATA_INPUT_CROP		0x0700000E
#define HFI_INDEX_EXTRADATA_OUTPUT_CROP		0x0700000F
#define HFI_INDEX_EXTRADATA_ASPECT_RATIO	0x7F100003

struct hfi_buffer_alloc_mode {
	u32 buffer_type;
	u32 buffer_mode;
};


struct hfi_index_extradata_config {
	int enable;
	u32 index_extra_data_id;
};

struct hfi_extradata_header {
	u32 size;
	u32 version;
	u32 port_index;
	u32 type;
	u32 data_size;
	u8 rg_data[1];
};

#define HFI_INTERLACE_FRAME_PROGRESSIVE					0x01
#define HFI_INTERLACE_INTERLEAVE_FRAME_TOPFIELDFIRST	0x02
#define HFI_INTERLACE_INTERLEAVE_FRAME_BOTTOMFIELDFIRST	0x04
#define HFI_INTERLACE_FRAME_TOPFIELDFIRST				0x08
#define HFI_INTERLACE_FRAME_BOTTOMFIELDFIRST			0x10

#define HFI_PROPERTY_SYS_OX_START			\
	(HFI_DOMAIN_BASE_COMMON + HFI_ARCH_OX_OFFSET + 0x0000)

#define HFI_PROPERTY_PARAM_OX_START				\
	(HFI_DOMAIN_BASE_COMMON + HFI_ARCH_OX_OFFSET + 0x1000)
#define HFI_PROPERTY_PARAM_BUFFER_COUNT_ACTUAL			\
	(HFI_PROPERTY_PARAM_OX_START + 0x001)
#define HFI_PROPERTY_PARAM_UNCOMPRESSED_PLANE_ACTUAL_CONSTRAINTS_INFO	\
	(HFI_PROPERTY_PARAM_OX_START + 0x002)
#define HFI_PROPERTY_PARAM_INTERLACE_FORMAT_SUPPORTED	\
	(HFI_PROPERTY_PARAM_OX_START + 0x003)
#define HFI_PROPERTY_PARAM_CHROMA_SITE					\
(HFI_PROPERTY_PARAM_OX_START + 0x004)
#define HFI_PROPERTY_PARAM_EXTRA_DATA_HEADER_CONFIG		\
	(HFI_PROPERTY_PARAM_OX_START + 0x005)
#define HFI_PROPERTY_PARAM_INDEX_EXTRADATA             \
	(HFI_PROPERTY_PARAM_OX_START + 0x006)
#define HFI_PROPERTY_PARAM_DIVX_FORMAT					\
	(HFI_PROPERTY_PARAM_OX_START + 0x007)
#define HFI_PROPERTY_PARAM_BUFFER_ALLOC_MODE			\
	(HFI_PROPERTY_PARAM_OX_START + 0x008)
#define HFI_PROPERTY_PARAM_S3D_FRAME_PACKING_EXTRADATA	\
	(HFI_PROPERTY_PARAM_OX_START + 0x009)
#define HFI_PROPERTY_PARAM_ERR_DETECTION_CODE_EXTRADATA \
	(HFI_PROPERTY_PARAM_OX_START + 0x00A)
#define HFI_PROPERTY_PARAM_BUFFER_ALLOC_MODE_SUPPORTED	\
	(HFI_PROPERTY_PARAM_OX_START + 0x00B)
#define HFI_PROPERTY_PARAM_BUFFER_SIZE_MINIMUM			\
	(HFI_PROPERTY_PARAM_OX_START + 0x00C)
#define HFI_PROPERTY_PARAM_SYNC_BASED_INTERRUPT			\
	(HFI_PROPERTY_PARAM_OX_START + 0x00E)

#define HFI_PROPERTY_CONFIG_OX_START					\
	(HFI_DOMAIN_BASE_COMMON + HFI_ARCH_OX_OFFSET + 0x02000)
#define HFI_PROPERTY_CONFIG_BUFFER_REQUIREMENTS			\
	(HFI_PROPERTY_CONFIG_OX_START + 0x001)
#define HFI_PROPERTY_CONFIG_REALTIME					\
	(HFI_PROPERTY_CONFIG_OX_START + 0x002)
#define HFI_PROPERTY_CONFIG_PRIORITY					\
	(HFI_PROPERTY_CONFIG_OX_START + 0x003)
#define HFI_PROPERTY_CONFIG_BATCH_INFO					\
	(HFI_PROPERTY_CONFIG_OX_START + 0x004)

#define HFI_PROPERTY_PARAM_VDEC_OX_START				\
	(HFI_DOMAIN_BASE_VDEC + HFI_ARCH_OX_OFFSET + 0x3000)
#define HFI_PROPERTY_PARAM_VDEC_CONTINUE_DATA_TRANSFER	\
	(HFI_PROPERTY_PARAM_VDEC_OX_START + 0x001)
#define HFI_PROPERTY_PARAM_VDEC_DISPLAY_PICTURE_BUFFER_COUNT\
	(HFI_PROPERTY_PARAM_VDEC_OX_START + 0x002)
#define HFI_PROPERTY_PARAM_VDEC_MULTI_VIEW_SELECT		\
	(HFI_PROPERTY_PARAM_VDEC_OX_START + 0x003)
#define HFI_PROPERTY_PARAM_VDEC_PICTURE_TYPE_DECODE		\
	(HFI_PROPERTY_PARAM_VDEC_OX_START + 0x004)
#define HFI_PROPERTY_PARAM_VDEC_OUTPUT_ORDER			\
	(HFI_PROPERTY_PARAM_VDEC_OX_START + 0x005)
#define HFI_PROPERTY_PARAM_VDEC_MB_QUANTIZATION			\
	(HFI_PROPERTY_PARAM_VDEC_OX_START + 0x006)
#define HFI_PROPERTY_PARAM_VDEC_NUM_CONCEALED_MB		\
	(HFI_PROPERTY_PARAM_VDEC_OX_START + 0x007)
#define HFI_PROPERTY_PARAM_VDEC_H264_ENTROPY_SWITCHING	\
	(HFI_PROPERTY_PARAM_VDEC_OX_START + 0x008)
#define HFI_PROPERTY_PARAM_VDEC_OUTPUT2_KEEP_ASPECT_RATIO\
	(HFI_PROPERTY_PARAM_VDEC_OX_START + 0x009)
#define HFI_PROPERTY_PARAM_VDEC_FRAME_RATE_EXTRADATA  \
	(HFI_PROPERTY_PARAM_VDEC_OX_START + 0x00A)
#define HFI_PROPERTY_PARAM_VDEC_PANSCAN_WNDW_EXTRADATA \
	(HFI_PROPERTY_PARAM_VDEC_OX_START + 0x00B)
#define HFI_PROPERTY_PARAM_VDEC_RECOVERY_POINT_SEI_EXTRADATA \
	(HFI_PROPERTY_PARAM_VDEC_OX_START + 0x00C)
#define HFI_PROPERTY_PARAM_VDEC_THUMBNAIL_MODE   \
	(HFI_PROPERTY_PARAM_VDEC_OX_START + 0x00D)

#define HFI_PROPERTY_PARAM_VDEC_FRAME_ASSEMBLY		\
	(HFI_PROPERTY_PARAM_VDEC_OX_START + 0x00E)
#define HFI_PROPERTY_PARAM_VDEC_VC1_FRAMEDISP_EXTRADATA		\
	(HFI_PROPERTY_PARAM_VDEC_OX_START + 0x011)
#define HFI_PROPERTY_PARAM_VDEC_VC1_SEQDISP_EXTRADATA		\
	(HFI_PROPERTY_PARAM_VDEC_OX_START + 0x012)
#define HFI_PROPERTY_PARAM_VDEC_TIMESTAMP_EXTRADATA			\
	(HFI_PROPERTY_PARAM_VDEC_OX_START + 0x013)
#define HFI_PROPERTY_PARAM_VDEC_INTERLACE_VIDEO_EXTRADATA	\
	(HFI_PROPERTY_PARAM_VDEC_OX_START + 0x014)
#define HFI_PROPERTY_PARAM_VDEC_AVC_SESSION_SELECT \
	(HFI_PROPERTY_PARAM_VDEC_OX_START + 0x015)
#define HFI_PROPERTY_PARAM_VDEC_MPEG2_SEQDISP_EXTRADATA \
	(HFI_PROPERTY_PARAM_VDEC_OX_START + 0x016)
#define HFI_PROPERTY_PARAM_VDEC_STREAM_USERDATA_EXTRADATA \
	(HFI_PROPERTY_PARAM_VDEC_OX_START + 0x017)
#define HFI_PROPERTY_PARAM_VDEC_FRAME_QP_EXTRADATA \
	(HFI_PROPERTY_PARAM_VDEC_OX_START + 0x018)
#define HFI_PROPERTY_PARAM_VDEC_FRAME_BITS_INFO_EXTRADATA \
	(HFI_PROPERTY_PARAM_VDEC_OX_START + 0x019)
#define HFI_PROPERTY_PARAM_VDEC_SCS_THRESHOLD \
	(HFI_PROPERTY_PARAM_VDEC_OX_START + 0x01A)
#define HFI_PROPERTY_PARAM_VUI_DISPLAY_INFO_EXTRADATA \
	(HFI_PROPERTY_PARAM_VDEC_OX_START + 0x01B)
#define HFI_PROPERTY_PARAM_VDEC_VQZIP_SEI_EXTRADATA \
	(HFI_PROPERTY_PARAM_VDEC_OX_START + 0x001C)
#define HFI_PROPERTY_PARAM_VDEC_VPX_COLORSPACE_EXTRADATA \
	(HFI_PROPERTY_PARAM_VDEC_OX_START + 0x001D)
#define HFI_PROPERTY_PARAM_VDEC_MASTERING_DISPLAY_COLOUR_SEI_EXTRADATA \
	(HFI_PROPERTY_PARAM_VDEC_OX_START + 0x001E)
#define HFI_PROPERTY_PARAM_VDEC_CONTENT_LIGHT_LEVEL_SEI_EXTRADATA \
	(HFI_PROPERTY_PARAM_VDEC_OX_START + 0x001F)

#define HFI_PROPERTY_CONFIG_VDEC_OX_START				\
	(HFI_DOMAIN_BASE_VDEC + HFI_ARCH_OX_OFFSET + 0x4000)
#define HFI_PROPERTY_CONFIG_VDEC_POST_LOOP_DEBLOCKER	\
	(HFI_PROPERTY_CONFIG_VDEC_OX_START + 0x001)
#define HFI_PROPERTY_CONFIG_VDEC_MB_ERROR_MAP_REPORTING	\
	(HFI_PROPERTY_CONFIG_VDEC_OX_START + 0x002)
#define HFI_PROPERTY_CONFIG_VDEC_MB_ERROR_MAP			\
	(HFI_PROPERTY_CONFIG_VDEC_OX_START + 0x003)
#define HFI_PROPERTY_CONFIG_VDEC_ENTROPY \
	(HFI_PROPERTY_CONFIG_VDEC_OX_START + 0x004)

#define HFI_PROPERTY_PARAM_VENC_OX_START				\
	(HFI_DOMAIN_BASE_VENC + HFI_ARCH_OX_OFFSET + 0x5000)
#define  HFI_PROPERTY_PARAM_VENC_MULTI_SLICE_INFO       \
	(HFI_PROPERTY_PARAM_VENC_OX_START + 0x001)
#define  HFI_PROPERTY_PARAM_VENC_H264_IDR_S3D_FRAME_PACKING_NAL \
	(HFI_PROPERTY_PARAM_VENC_OX_START + 0x002)
#define  HFI_PROPERTY_PARAM_VENC_LTR_INFO			\
	(HFI_PROPERTY_PARAM_VENC_OX_START + 0x003)
#define  HFI_PROPERTY_PARAM_VENC_MBI_DUMPING				\
	(HFI_PROPERTY_PARAM_VENC_OX_START + 0x005)
#define HFI_PROPERTY_PARAM_VENC_FRAME_QP_EXTRADATA		\
	(HFI_PROPERTY_PARAM_VENC_OX_START + 0x006)
#define  HFI_PROPERTY_PARAM_VENC_YUVSTAT_INFO_EXTRADATA		\
	(HFI_PROPERTY_PARAM_VENC_OX_START + 0x007)
#define  HFI_PROPERTY_PARAM_VENC_ROI_QP_EXTRADATA		\
	(HFI_PROPERTY_PARAM_VENC_OX_START + 0x008)
#define  HFI_PROPERTY_PARAM_VENC_OVERRIDE_QP_EXTRADATA		\
	(HFI_PROPERTY_PARAM_VENC_OX_START + 0x009)

#define HFI_PROPERTY_CONFIG_VENC_OX_START				\
	(HFI_DOMAIN_BASE_VENC + HFI_ARCH_OX_OFFSET + 0x6000)
#define  HFI_PROPERTY_CONFIG_VENC_FRAME_QP				\
	(HFI_PROPERTY_CONFIG_VENC_OX_START + 0x001)

#define HFI_PROPERTY_PARAM_VPE_OX_START					\
	(HFI_DOMAIN_BASE_VPE + HFI_ARCH_OX_OFFSET + 0x7000)
#define HFI_PROPERTY_PARAM_VPE_COLOR_SPACE_CONVERSION			\
	(HFI_PROPERTY_PARAM_VPE_OX_START + 0x001)

#define HFI_PROPERTY_CONFIG_VPE_OX_START				\
	(HFI_DOMAIN_BASE_VPE + HFI_ARCH_OX_OFFSET + 0x8000)

struct hfi_batch_info {
	u32 input_batch_count;
	u32 output_batch_count;
};

struct hfi_buffer_count_actual {
	u32 buffer_type;
	u32 buffer_count_actual;
};

struct hfi_buffer_size_minimum {
	u32 buffer_type;
	u32 buffer_size;
};

struct hfi_buffer_requirements {
	u32 buffer_type;
	u32 buffer_size;
	u32 buffer_region_size;
	u32 buffer_hold_count;
	u32 buffer_count_min;
	u32 buffer_count_actual;
	u32 contiguous;
	u32 buffer_alignment;
};

#define HFI_CHROMA_SITE_0			(HFI_OX_BASE + 0x1)
#define HFI_CHROMA_SITE_1			(HFI_OX_BASE + 0x2)
#define HFI_CHROMA_SITE_2			(HFI_OX_BASE + 0x3)
#define HFI_CHROMA_SITE_3			(HFI_OX_BASE + 0x4)
#define HFI_CHROMA_SITE_4			(HFI_OX_BASE + 0x5)
#define HFI_CHROMA_SITE_5			(HFI_OX_BASE + 0x6)

struct hfi_data_payload {
	u32 size;
	u8 rg_data[1];
};

struct hfi_enable_picture {
	u32 picture_type;
};

struct hfi_display_picture_buffer_count {
	int enable;
	u32 count;
};

struct hfi_extra_data_header_config {
	u32 type;
	u32 buffer_type;
	u32 version;
	u32 port_index;
	u32 client_extra_data_id;
};

struct hfi_interlace_format_supported {
	u32 buffer_type;
	u32 format;
};

struct hfi_buffer_alloc_mode_supported {
	u32 buffer_type;
	u32 num_entries;
	u32 rg_data[1];
};

struct hfi_mb_error_map {
	u32 error_map_size;
	u8 rg_error_map[1];
};

struct hfi_metadata_pass_through {
	int enable;
	u32 size;
};

struct hfi_multi_view_select {
	u32 view_index;
};

struct hfi_hybrid_hierp {
	u32 layers;
};

#define HFI_PRIORITY_LOW		10
#define HFI_PRIOIRTY_MEDIUM		20
#define HFI_PRIORITY_HIGH		30

#define HFI_OUTPUT_ORDER_DISPLAY	(HFI_OX_BASE + 0x1)
#define HFI_OUTPUT_ORDER_DECODE		(HFI_OX_BASE + 0x2)

#define HFI_RATE_CONTROL_OFF		(HFI_OX_BASE + 0x1)
#define HFI_RATE_CONTROL_VBR_VFR	(HFI_OX_BASE + 0x2)
#define HFI_RATE_CONTROL_VBR_CFR	(HFI_OX_BASE + 0x3)
#define HFI_RATE_CONTROL_CBR_VFR	(HFI_OX_BASE + 0x4)
#define HFI_RATE_CONTROL_CBR_CFR	(HFI_OX_BASE + 0x5)
#define HFI_RATE_CONTROL_MBR_CFR	(HFI_OX_BASE + 0x6)
#define HFI_RATE_CONTROL_MBR_VFR	(HFI_OX_BASE + 0x7)


struct hfi_uncompressed_plane_actual_constraints_info {
	u32 buffer_type;
	u32 num_planes;
	struct hfi_uncompressed_plane_constraints rg_plane_format[1];
};

#define HFI_CMD_SYS_OX_START		\
(HFI_DOMAIN_BASE_COMMON + HFI_ARCH_OX_OFFSET + HFI_CMD_START_OFFSET + 0x0000)
#define HFI_CMD_SYS_SESSION_ABORT	(HFI_CMD_SYS_OX_START + 0x001)
#define HFI_CMD_SYS_PING		(HFI_CMD_SYS_OX_START + 0x002)

#define HFI_CMD_SESSION_OX_START	\
(HFI_DOMAIN_BASE_COMMON + HFI_ARCH_OX_OFFSET + HFI_CMD_START_OFFSET + 0x1000)
#define HFI_CMD_SESSION_LOAD_RESOURCES	(HFI_CMD_SESSION_OX_START + 0x001)
#define HFI_CMD_SESSION_START		(HFI_CMD_SESSION_OX_START + 0x002)
#define HFI_CMD_SESSION_STOP		(HFI_CMD_SESSION_OX_START + 0x003)
#define HFI_CMD_SESSION_EMPTY_BUFFER	(HFI_CMD_SESSION_OX_START + 0x004)
#define HFI_CMD_SESSION_FILL_BUFFER	(HFI_CMD_SESSION_OX_START + 0x005)
#define HFI_CMD_SESSION_SUSPEND		(HFI_CMD_SESSION_OX_START + 0x006)
#define HFI_CMD_SESSION_RESUME		(HFI_CMD_SESSION_OX_START + 0x007)
#define HFI_CMD_SESSION_FLUSH		(HFI_CMD_SESSION_OX_START + 0x008)
#define HFI_CMD_SESSION_GET_PROPERTY	(HFI_CMD_SESSION_OX_START + 0x009)
#define HFI_CMD_SESSION_PARSE_SEQUENCE_HEADER	\
	(HFI_CMD_SESSION_OX_START + 0x00A)
#define HFI_CMD_SESSION_RELEASE_BUFFERS		\
	(HFI_CMD_SESSION_OX_START + 0x00B)
#define HFI_CMD_SESSION_RELEASE_RESOURCES	\
	(HFI_CMD_SESSION_OX_START + 0x00C)
#define HFI_CMD_SESSION_CONTINUE	(HFI_CMD_SESSION_OX_START + 0x00D)
#define HFI_CMD_SESSION_SYNC		(HFI_CMD_SESSION_OX_START + 0x00E)

#define HFI_MSG_SYS_OX_START			\
(HFI_DOMAIN_BASE_COMMON + HFI_ARCH_OX_OFFSET + HFI_MSG_START_OFFSET + 0x0000)
#define HFI_MSG_SYS_PING_ACK	(HFI_MSG_SYS_OX_START + 0x2)
#define HFI_MSG_SYS_SESSION_ABORT_DONE	(HFI_MSG_SYS_OX_START + 0x4)

#define HFI_MSG_SESSION_OX_START		\
(HFI_DOMAIN_BASE_COMMON + HFI_ARCH_OX_OFFSET + HFI_MSG_START_OFFSET + 0x1000)
#define HFI_MSG_SESSION_LOAD_RESOURCES_DONE	(HFI_MSG_SESSION_OX_START + 0x1)
#define HFI_MSG_SESSION_START_DONE		(HFI_MSG_SESSION_OX_START + 0x2)
#define HFI_MSG_SESSION_STOP_DONE		(HFI_MSG_SESSION_OX_START + 0x3)
#define HFI_MSG_SESSION_SUSPEND_DONE	(HFI_MSG_SESSION_OX_START + 0x4)
#define HFI_MSG_SESSION_RESUME_DONE		(HFI_MSG_SESSION_OX_START + 0x5)
#define HFI_MSG_SESSION_FLUSH_DONE		(HFI_MSG_SESSION_OX_START + 0x6)
#define HFI_MSG_SESSION_EMPTY_BUFFER_DONE	(HFI_MSG_SESSION_OX_START + 0x7)
#define HFI_MSG_SESSION_FILL_BUFFER_DONE	(HFI_MSG_SESSION_OX_START + 0x8)
#define HFI_MSG_SESSION_PROPERTY_INFO		(HFI_MSG_SESSION_OX_START + 0x9)
#define HFI_MSG_SESSION_RELEASE_RESOURCES_DONE	\
	(HFI_MSG_SESSION_OX_START + 0xA)
#define HFI_MSG_SESSION_PARSE_SEQUENCE_HEADER_DONE		\
	(HFI_MSG_SESSION_OX_START + 0xB)
#define  HFI_MSG_SESSION_RELEASE_BUFFERS_DONE			\
	(HFI_MSG_SESSION_OX_START + 0xC)

#define VIDC_IFACEQ_MAX_PKT_SIZE                        1024
#define VIDC_IFACEQ_MED_PKT_SIZE                        768
#define VIDC_IFACEQ_MIN_PKT_SIZE                        8
#define VIDC_IFACEQ_VAR_SMALL_PKT_SIZE          100
#define VIDC_IFACEQ_VAR_LARGE_PKT_SIZE          512
#define VIDC_IFACEQ_VAR_HUGE_PKT_SIZE          (1024*12)


struct hfi_cmd_sys_session_abort_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
};

struct hfi_cmd_sys_ping_packet {
	u32 size;
	u32 packet_type;
	u32 client_data;
};

struct hfi_cmd_session_load_resources_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
};

struct hfi_cmd_session_start_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
};

struct hfi_cmd_session_stop_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
};

struct hfi_cmd_session_empty_buffer_compressed_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	u32 time_stamp_hi;
	u32 time_stamp_lo;
	u32 flags;
	u32 mark_target;
	u32 mark_data;
	u32 offset;
	u32 alloc_len;
	u32 filled_len;
	u32 input_tag;
	u32 packet_buffer;
	u32 extra_data_buffer;
	u32 rgData[1];
};

struct hfi_cmd_session_empty_buffer_uncompressed_plane0_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	u32 view_id;
	u32 time_stamp_hi;
	u32 time_stamp_lo;
	u32 flags;
	u32 mark_target;
	u32 mark_data;
	u32 alloc_len;
	u32 filled_len;
	u32 offset;
	u32 input_tag;
	u32 packet_buffer;
	u32 extra_data_buffer;
	u32 rgData[1];
};

struct hfi_cmd_session_empty_buffer_uncompressed_plane1_packet {
	u32 flags;
	u32 alloc_len;
	u32 filled_len;
	u32 offset;
	u32 packet_buffer2;
	u32 rgData[1];
};

struct hfi_cmd_session_empty_buffer_uncompressed_plane2_packet {
	u32 flags;
	u32 alloc_len;
	u32 filled_len;
	u32 offset;
	u32 packet_buffer3;
	u32 rgData[1];
};

struct hfi_cmd_session_fill_buffer_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	u32 stream_id;
	u32 offset;
	u32 alloc_len;
	u32 filled_len;
	u32 output_tag;
	u32 packet_buffer;
	u32 extra_data_buffer;
	u32 rgData[1];
};

struct hfi_cmd_session_flush_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	u32 flush_type;
};

struct hfi_cmd_session_suspend_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
};

struct hfi_cmd_session_resume_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
};

struct hfi_cmd_session_get_property_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	u32 num_properties;
	u32 rg_property_data[1];
};

struct hfi_cmd_session_release_buffer_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	u32 buffer_type;
	u32 buffer_size;
	u32 extra_data_size;
	int response_req;
	u32 num_buffers;
	u32 rg_buffer_info[1];
};

struct hfi_cmd_session_release_resources_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
};

struct hfi_cmd_session_parse_sequence_header_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	u32 header_len;
	u32 packet_buffer;
};

struct hfi_msg_sys_session_abort_done_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	u32 error_type;
};

struct hfi_msg_sys_idle_packet {
	u32 size;
	u32 packet_type;
};

struct hfi_msg_sys_ping_ack_packet {
	u32 size;
	u32 packet_type;
	u32 client_data;
};

struct hfi_msg_sys_property_info_packet {
	u32 size;
	u32 packet_type;
	u32 num_properties;
	u32 rg_property_data[1];
};

struct hfi_msg_session_load_resources_done_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	u32 error_type;
};

struct hfi_msg_session_start_done_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	u32 error_type;
};

struct hfi_msg_session_stop_done_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	u32 error_type;
};

struct hfi_msg_session_suspend_done_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	u32 error_type;
};

struct hfi_msg_session_resume_done_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	u32 error_type;
};

struct hfi_msg_session_flush_done_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	u32 error_type;
	u32 flush_type;
};

struct hfi_msg_session_empty_buffer_done_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	u32 error_type;
	u32 offset;
	u32 filled_len;
	u32 input_tag;
	u32 packet_buffer;
	u32 extra_data_buffer;
	u32 rgData[0];
};

struct hfi_msg_session_fill_buffer_done_compressed_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	u32 time_stamp_hi;
	u32 time_stamp_lo;
	u32 error_type;
	u32 flags;
	u32 mark_target;
	u32 mark_data;
	u32 stats;
	u32 offset;
	u32 alloc_len;
	u32 filled_len;
	u32 input_tag;
	u32 output_tag;
	u32 picture_type;
	u32 packet_buffer;
	u32 extra_data_buffer;
	u32 rgData[0];
};

struct hfi_msg_session_fbd_uncompressed_plane0_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	u32 stream_id;
	u32 view_id;
	u32 error_type;
	u32 time_stamp_hi;
	u32 time_stamp_lo;
	u32 flags;
	u32 mark_target;
	u32 mark_data;
	u32 stats;
	u32 alloc_len;
	u32 filled_len;
	u32 offset;
	u32 frame_width;
	u32 frame_height;
	u32 start_x_coord;
	u32 start_y_coord;
	u32 input_tag;
	u32 input_tag2;
	u32 output_tag;
	u32 picture_type;
	u32 packet_buffer;
	u32 extra_data_buffer;
	u32 rgData[0];
};

struct hfi_msg_session_fill_buffer_done_uncompressed_plane1_packet {
	u32 flags;
	u32 alloc_len;
	u32 filled_len;
	u32 offset;
	u32 packet_buffer2;
	u32 rgData[0];
};

struct hfi_msg_session_fill_buffer_done_uncompressed_plane2_packet {
	u32 flags;
	u32 alloc_len;
	u32 filled_len;
	u32 offset;
	u32 packet_buffer3;
	u32 rgData[0];
};

struct hfi_msg_session_parse_sequence_header_done_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	u32 error_type;
	u32 num_properties;
	u32 rg_property_data[1];
};

struct hfi_msg_session_property_info_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	u32 num_properties;
	u32 rg_property_data[1];
};

struct hfi_msg_session_release_resources_done_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	u32 error_type;
};

struct hfi_msg_session_release_buffers_done_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	u32 error_type;
	u32 num_buffers;
	u32 rg_buffer_info[1];
};

struct hfi_extradata_mb_quantization_payload {
	u8 rg_mb_qp[1];
};

struct hfi_extradata_vc1_pswnd {
	u32 ps_wnd_h_offset;
	u32 ps_wnd_v_offset;
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
	u32 time_stamp_low;
	u32 time_stamp_high;
};


struct hfi_extradata_s3d_frame_packing_payload {
	u32 fpa_id;
	int cancel_flag;
	u32 fpa_type;
	int quin_cunx_flag;
	u32 content_interprtation_type;
	int spatial_flipping_flag;
	int frame0_flipped_flag;
	int field_views_flag;
	int current_frame_isFrame0_flag;
	int frame0_self_contained_flag;
	int frame1_self_contained_flag;
	u32 frame0_graid_pos_x;
	u32 frame0_graid_pos_y;
	u32 frame1_graid_pos_x;
	u32 frame1_graid_pos_y;
	u32 fpa_reserved_byte;
	u32 fpa_repetition_period;
	int fpa_extension_flag;
};

struct hfi_extradata_interlace_video_payload {
	u32 format;
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

struct hfi_index_extradata_output_crop_payload {
	u32 size;
	u32 version;
	u32 port_index;
	u32 left;
	u32 top;
	u32 display_width;
	u32 display_height;
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

struct hfi_index_extradata_aspect_ratio_payload {
	u32 size;
	u32 version;
	u32 port_index;
	u32 aspect_width;
	u32 aspect_height;
};
struct hfi_extradata_panscan_wndw_payload {
	u32 num_window;
	struct hfi_extradata_vc1_pswnd wnd[1];
};

struct hfi_extradata_frame_type_payload {
	u32 frame_rate;
};

struct hfi_extradata_recovery_point_sei_payload {
	u32 flag;
};

struct hfi_cmd_session_continue_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
};

struct hal_session {
	struct list_head list;
	void *session_id;
	bool is_decoder;
	enum hal_video_codec codec;
	enum hal_domain domain;
	void *device;
};

struct hal_device_data {
	struct list_head dev_head;
	int dev_count;
};

struct msm_vidc_fw {
	void *cookie;
};

int hfi_process_msg_packet(u32 device_id, struct vidc_hal_msg_pkt_hdr *msg_hdr,
		struct msm_vidc_cb_info *info);

enum vidc_status hfi_process_sys_init_done_prop_read(
	struct hfi_msg_sys_init_done_packet *pkt,
	struct vidc_hal_sys_init_done *sys_init_done);

enum vidc_status hfi_process_session_init_done_prop_read(
	struct hfi_msg_sys_session_init_done_packet *pkt,
	struct vidc_hal_session_init_done *session_init_done);

#endif

