/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#ifndef __H_VIDC_HAL_H__
#define __H_VIDC_HAL_H__

#include <linux/spinlock.h>
#include <linux/mutex.h>
#include "vidc_hal_api.h"
#include "msm_smem.h"
#include "vidc_hal_helper.h"

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

#define VIDC_IFACEQ_QUEUE_SIZE	(VIDC_IFACEQ_MAX_PKT_SIZE *  \
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

#define HFI_EVENT_SESSION_SEQUENCE_CHANGED (HFI_OX_BASE + 0x3)
#define HFI_EVENT_SESSION_PROPERTY_CHANGED (HFI_OX_BASE + 0x4)

#define HFI_EVENT_DATA_SEQUENCE_CHANGED_SUFFICIENT_BUFFER_RESOURCES	\
	(HFI_OX_BASE + 0x1)
#define HFI_EVENT_DATA_SEQUENCE_CHANGED_INSUFFICIENT_BUFFER_RESOURCES	\
	(HFI_OX_BASE + 0x2)

#define HFI_BUFFERFLAG_EOS				0x00000001
#define HFI_BUFFERFLAG_STARTTIME		0x00000002
#define HFI_BUFFERFLAG_DECODEONLY		0x00000004
#define HFI_BUFFERFLAG_DATACORRUPT		0x00000008
#define HFI_BUFFERFLAG_ENDOFFRAME		0x00000010
#define HFI_BUFFERFLAG_SYNCFRAME		0x00000020
#define HFI_BUFFERFLAG_EXTRADATA		0x00000040
#define HFI_BUFFERFLAG_CODECCONFIG		0x00000080
#define HFI_BUFFERFLAG_TIMESTAMPINVALID	0x00000100
#define HFI_BUFFERFLAG_READONLY			0x00000200
#define HFI_BUFFERFLAG_ENDOFSUBFRAME	0x00000400

#define HFI_ERR_SESSION_EMPTY_BUFFER_DONE_OUTPUT_PENDING	\
	(HFI_OX_BASE + 0x1001)
#define HFI_ERR_SESSION_SAME_STATE_OPERATION		\
	(HFI_OX_BASE + 0x1002)
#define HFI_ERR_SESSION_SYNC_FRAME_NOT_DETECTED		\
	(HFI_OX_BASE + 0x1003)

#define HFI_BUFFER_INTERNAL_SCRATCH (HFI_OX_BASE + 0x1)
#define HFI_BUFFER_EXTRADATA_INPUT (HFI_OX_BASE + 0x2)
#define HFI_BUFFER_EXTRADATA_OUTPUT (HFI_OX_BASE + 0x3)
#define HFI_BUFFER_EXTRADATA_OUTPUT2 (HFI_OX_BASE + 0x4)

#define HFI_BUFFER_MODE_STATIC (HFI_OX_BASE + 0x1)
#define HFI_BUFFER_MODE_RING (HFI_OX_BASE + 0x2)

#define HFI_FLUSH_INPUT (HFI_OX_BASE + 0x1)
#define HFI_FLUSH_OUTPUT (HFI_OX_BASE + 0x2)
#define HFI_FLUSH_OUTPUT2 (HFI_OX_BASE + 0x3)
#define HFI_FLUSH_ALL (HFI_OX_BASE + 0x4)

#define HFI_EXTRADATA_NONE					0x00000000
#define HFI_EXTRADATA_MB_QUANTIZATION		0x00000001
#define HFI_EXTRADATA_INTERLACE_VIDEO		0x00000002
#define HFI_EXTRADATA_VC1_FRAMEDISP			0x00000003
#define HFI_EXTRADATA_VC1_SEQDISP			0x00000004
#define HFI_EXTRADATA_TIMESTAMP				0x00000005
#define HFI_EXTRADATA_S3D_FRAME_PACKING		0x00000006
#define  HFI_EXTRADATA_EOSNAL_DETECTED      0x00000007
#define HFI_EXTRADATA_MULTISLICE_INFO		0x7F100000
#define HFI_EXTRADATA_NUM_CONCEALED_MB		0x7F100001
#define HFI_EXTRADATA_INDEX					0x7F100002
#define HFI_EXTRADATA_METADATA_FILLER		0x7FE00002

#define HFI_INDEX_EXTRADATA_INPUT_CROP		0x0700000E
#define HFI_INDEX_EXTRADATA_DIGITAL_ZOOM	0x07000010
#define HFI_INDEX_EXTRADATA_ASPECT_RATIO	0x7F100003

struct HFI_INDEX_EXTRADATA_CONFIG_TYPE {
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
#define HFI_PROPERTY_SYS_IDLE_INDICATOR		\
	(HFI_PROPERTY_SYS_OX_START + 0x001)

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
#define  HFI_PROPERTY_PARAM_INDEX_EXTRADATA             \
	(HFI_PROPERTY_PARAM_OX_START + 0x006)
#define HFI_PROPERTY_PARAM_DIVX_FORMAT					\
	(HFI_PROPERTY_PARAM_OX_START + 0x007)

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

#define HFI_PROPERTY_CONFIG_VDEC_OX_START				\
	(HFI_DOMAIN_BASE_VDEC + HFI_ARCH_OX_OFFSET + 0x0000)
#define HFI_PROPERTY_CONFIG_VDEC_POST_LOOP_DEBLOCKER	\
	(HFI_PROPERTY_CONFIG_VDEC_OX_START + 0x001)
#define HFI_PROPERTY_CONFIG_VDEC_MB_ERROR_MAP_REPORTING	\
	(HFI_PROPERTY_CONFIG_VDEC_OX_START + 0x002)
#define HFI_PROPERTY_CONFIG_VDEC_MB_ERROR_MAP			\
	(HFI_PROPERTY_CONFIG_VDEC_OX_START + 0x003)

#define HFI_PROPERTY_PARAM_VENC_OX_START				\
	(HFI_DOMAIN_BASE_VENC + HFI_ARCH_OX_OFFSET + 0x5000)
#define  HFI_PROPERTY_PARAM_VENC_MULTI_SLICE_INFO       \
	(HFI_PROPERTY_PARAM_VENC_OX_START + 0x001)
#define  HFI_PROPERTY_PARAM_VENC_H264_IDR_S3D_FRAME_PACKING_NAL \
	(HFI_PROPERTY_PARAM_VENC_OX_START + 0x002)
#define HFI_PROPERTY_CONFIG_VENC_OX_START				\
	(HFI_DOMAIN_BASE_VENC + HFI_ARCH_OX_OFFSET + 0x6000)

#define HFI_PROPERTY_PARAM_VPE_OX_START					\
	(HFI_DOMAIN_BASE_VPE + HFI_ARCH_OX_OFFSET + 0x7000)
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

struct hfi_uncompressed_plane_actual_constraints_info {
	u32 buffer_type;
	u32 num_planes;
	struct hfi_uncompressed_plane_constraints rg_plane_format[1];
};

#define HFI_CMD_SYS_OX_START		\
	(HFI_DOMAIN_BASE_COMMON + HFI_ARCH_OX_OFFSET + 0x0000)
#define HFI_CMD_SYS_SESSION_ABORT	(HFI_CMD_SYS_OX_START + 0x001)
#define HFI_CMD_SYS_PING		(HFI_CMD_SYS_OX_START + 0x002)

#define HFI_CMD_SESSION_OX_START	\
	(HFI_DOMAIN_BASE_COMMON + HFI_ARCH_OX_OFFSET + 0x1000)
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

#define HFI_MSG_SYS_OX_START			\
	(HFI_DOMAIN_BASE_COMMON + HFI_ARCH_OX_OFFSET + 0x0000)
#define HFI_MSG_SYS_IDLE		(HFI_MSG_SYS_OX_START + 0x1)
#define HFI_MSG_SYS_PING_ACK	(HFI_MSG_SYS_OX_START + 0x2)
#define HFI_MSG_SYS_PROPERTY_INFO	(HFI_MSG_SYS_OX_START + 0x3)
#define HFI_MSG_SYS_SESSION_ABORT_DONE	(HFI_MSG_SYS_OX_START + 0x4)

#define HFI_MSG_SESSION_OX_START		\
	(HFI_DOMAIN_BASE_COMMON + HFI_ARCH_OX_OFFSET + 0x1000)
#define HFI_MSG_SESSION_LOAD_RESOURCES_DONE	(HFI_MSG_SESSION_OX_START + 0x1)
#define HFI_MSG_SESSION_START_DONE		(HFI_MSG_SESSION_OX_START + 0x2)
#define HFI_MSG_SESSION_STOP_DONE		(HFI_MSG_SESSION_OX_START + 0x3)
#define HFI_MSG_SESSION_SUSPEND_DONE	(HFI_MSG_SESSION_OX_START + 0x4)
#define HFI_MSG_SESSION_RESUME_DONE		(HFI_MSG_SESSION_OX_START + 0x5)
#define HFI_MSG_SESSION_FLUSH_DONE		(HFI_MSG_SESSION_OX_START + 0x6)
#define HFI_MSG_SESSION_EMPTY_BUFFER_DONE	(HFI_MSG_SESSION_OX_START + 0x7)
#define HFI_MSG_SESSION_FILL_BUFFER_DONE	(HFI_MSG_SESSION_OX_START + 0x8)
#define HFI_MSG_SESSION_PROPERTY_INFO		(HFI_MSG_SESSION_OX_START + 0x9)
#define HFI_MSG_SESSION_RELEASE_RESOURCES_DONE	(HFI_MSG_SESSION_OX_START + 0xA)
#define HFI_MSG_SESSION_PARSE_SEQUENCE_HEADER_DONE		\
	(HFI_MSG_SESSION_OX_START + 0xB)

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
	u8 *packet_buffer;
	u8 *extra_data_buffer;
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
	u8 *packet_buffer;
	u8 *extra_data_buffer;
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
	u32 packet_type;
	u32 session_id;
	u32 stream_id;
	u32 output_tag;
	u8 *packet_buffer;
	u8 *extra_data_buffer;
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
	u8 *packet_buffer;
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
	u8 *packet_buffer;
	u8 *extra_data_buffer;
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
	u8 *packet_buffer;
	u8 *extra_data_buffer;
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
	u8 *packet_buffer;
	u8 *extra_data_buffer;
};

struct hfi_msg_session_fill_buffer_done_uncompressed_plane1_packet {
	u32 flags;
	u32 alloc_len;
	u32 filled_len;
	u32 offset;
	u8 *packet_buffer2;
};

struct hfi_msg_session_fill_buffer_done_uncompressed_plane2_packet {
	u32 flags;
	u32 alloc_len;
	u32 filled_len;
	u32 offset;
	u8 *packet_buffer3;
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

enum HFI_S3D_FP_LAYOUT {
	HFI_S3D_FP_LAYOUT_NONE,
	HFI_S3D_FP_LAYOUT_INTRLV_CHECKERBOARD,
	HFI_S3D_FP_LAYOUT_INTRLV_COLUMN,
	HFI_S3D_FP_LAYOUT_INTRLV_ROW,
	HFI_S3D_FP_LAYOUT_SIDEBYSIDE,
	HFI_S3D_FP_LAYOUT_TOPBOTTOM,
	HFI_S3D_FP_LAYOUT_UNUSED = 0x10000000
};

enum HFI_S3D_FP_VIEW_ORDER {
	HFI_S3D_FP_LEFTVIEW_FIRST,
	HFI_S3D_FP_RIGHTVIEW_FIRST,
	HFI_S3D_FP_UNKNOWN,
	HFI_S3D_FP_VIEWORDER_UNUSED = 0x10000000
};

enum HFI_S3D_FP_FLIP {
	HFI_S3D_FP_FLIP_NONE,
	HFI_S3D_FP_FLIP_LEFT_HORIZ,
	HFI_S3D_FP_FLIP_LEFT_VERT,
	HFI_S3D_FP_FLIP_RIGHT_HORIZ,
	HFI_S3D_FP_FLIP_RIGHT_VERT,
	HFI_S3D_FP_FLIP_UNUSED = 0x10000000
};

struct hfi_extradata_s3d_frame_packing_payload {
	enum HFI_S3D_FP_LAYOUT layout;
	enum HFI_S3D_FP_VIEW_ORDER order;
	enum HFI_S3D_FP_FLIP flip;
	int quin_cunx;
	u32 left_view_luma_site_x;
	u32 left_view_luma_site_y;
	u32 right_view_luma_site_x;
	u32 right_view_luma_site_y;
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
	u32 device_base_addr;
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

struct hfi_index_extradata_aspect_ratio_payload {
	u32 size;
	u32 version;
	u32 port_index;
	u32 saspect_width;
	u32  saspect_height;
};

extern struct hal_device_data hal_ctxt;

int vidc_hal_iface_msgq_read(struct hal_device *device, void *pkt);
int vidc_hal_iface_dbgq_read(struct hal_device *device, void *pkt);

/* Interrupt Processing:*/
void vidc_hal_response_handler(struct hal_device *device);

#endif
