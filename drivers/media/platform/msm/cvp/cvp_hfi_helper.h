/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#ifndef __H_CVP_HFI_HELPER_H__
#define __H_CVP_HFI_HELPER_H__

#define HFI_COMMON_BASE				(0)
#define HFI_DOMAIN_BASE_COMMON		(HFI_COMMON_BASE + 0)
#define HFI_DOMAIN_BASE_CVP			(HFI_COMMON_BASE + 0x04000000)

#define HFI_ARCH_COMMON_OFFSET		(0)

#define  HFI_CMD_START_OFFSET		(0x00010000)
#define  HFI_MSG_START_OFFSET		(0x00020000)

#define HFI_ERR_NONE						HFI_COMMON_BASE
#define HFI_ERR_SYS_FATAL				(HFI_COMMON_BASE + 0x1)
#define HFI_ERR_SYS_INVALID_PARAMETER		(HFI_COMMON_BASE + 0x2)
#define HFI_ERR_SYS_VERSION_MISMATCH		(HFI_COMMON_BASE + 0x3)
#define HFI_ERR_SYS_INSUFFICIENT_RESOURCES	(HFI_COMMON_BASE + 0x4)
#define HFI_ERR_SYS_MAX_SESSIONS_REACHED	(HFI_COMMON_BASE + 0x5)
#define HFI_ERR_SYS_UNSUPPORTED_CODEC		(HFI_COMMON_BASE + 0x6)
#define HFI_ERR_SYS_SESSION_IN_USE			(HFI_COMMON_BASE + 0x7)
#define HFI_ERR_SYS_SESSION_ID_OUT_OF_RANGE	(HFI_COMMON_BASE + 0x8)
#define HFI_ERR_SYS_UNSUPPORTED_DOMAIN		(HFI_COMMON_BASE + 0x9)
#define HFI_ERR_SYS_NOC_ERROR			(HFI_COMMON_BASE + 0x11)
#define HFI_ERR_SESSION_FATAL			(HFI_COMMON_BASE + 0x1001)
#define HFI_ERR_SESSION_INVALID_PARAMETER	(HFI_COMMON_BASE + 0x1002)
#define HFI_ERR_SESSION_BAD_POINTER		(HFI_COMMON_BASE + 0x1003)
#define HFI_ERR_SESSION_INVALID_SESSION_ID	(HFI_COMMON_BASE + 0x1004)
#define HFI_ERR_SESSION_INVALID_STREAM_ID	(HFI_COMMON_BASE + 0x1005)
#define HFI_ERR_SESSION_INCORRECT_STATE_OPERATION		\
	(HFI_COMMON_BASE + 0x1006)
#define HFI_ERR_SESSION_UNSUPPORTED_PROPERTY	(HFI_COMMON_BASE + 0x1007)

#define HFI_ERR_SESSION_UNSUPPORTED_SETTING	(HFI_COMMON_BASE + 0x1008)

#define HFI_ERR_SESSION_INSUFFICIENT_RESOURCES	(HFI_COMMON_BASE + 0x1009)

#define HFI_ERR_SESSION_STREAM_CORRUPT		(HFI_COMMON_BASE + 0x100B)
#define HFI_ERR_SESSION_ENC_OVERFLOW		(HFI_COMMON_BASE + 0x100C)
#define HFI_ERR_SESSION_UNSUPPORTED_STREAM	(HFI_COMMON_BASE + 0x100D)
#define HFI_ERR_SESSION_CMDSIZE			(HFI_COMMON_BASE + 0x100E)
#define HFI_ERR_SESSION_UNSUPPORT_CMD		(HFI_COMMON_BASE + 0x100F)
#define HFI_ERR_SESSION_UNSUPPORT_BUFFERTYPE	(HFI_COMMON_BASE + 0x1010)
#define HFI_ERR_SESSION_BUFFERCOUNT_TOOSMALL	(HFI_COMMON_BASE + 0x1011)
#define HFI_ERR_SESSION_INVALID_SCALE_FACTOR	(HFI_COMMON_BASE + 0x1012)
#define HFI_ERR_SESSION_UPSCALE_NOT_SUPPORTED	(HFI_COMMON_BASE + 0x1013)

#define HFI_EVENT_SYS_ERROR				(HFI_COMMON_BASE + 0x1)
#define HFI_EVENT_SESSION_ERROR			(HFI_COMMON_BASE + 0x2)

#define  HFI_TME_PROFILE_DEFAULT	0x00000001
#define  HFI_TME_PROFILE_FRC		0x00000002
#define  HFI_TME_PROFILE_ASW		0x00000004
#define  HFI_TME_PROFILE_DFS_BOKEH	0x00000008

#define HFI_TME_LEVEL_INTEGER		0x00000001

#define HFI_BUFFER_INPUT				(HFI_COMMON_BASE + 0x1)
#define HFI_BUFFER_OUTPUT				(HFI_COMMON_BASE + 0x2)
#define HFI_BUFFER_OUTPUT2				(HFI_COMMON_BASE + 0x3)
#define HFI_BUFFER_INTERNAL_PERSIST		(HFI_COMMON_BASE + 0x4)
#define HFI_BUFFER_INTERNAL_PERSIST_1		(HFI_COMMON_BASE + 0x5)
#define HFI_BUFFER_COMMON_INTERNAL_SCRATCH	(HFI_COMMON_BASE + 0x6)
#define HFI_BUFFER_COMMON_INTERNAL_SCRATCH_1	(HFI_COMMON_BASE + 0x7)
#define HFI_BUFFER_COMMON_INTERNAL_SCRATCH_2	(HFI_COMMON_BASE + 0x8)
#define HFI_BUFFER_COMMON_INTERNAL_RECON	(HFI_COMMON_BASE + 0x9)
#define HFI_BUFFER_EXTRADATA_OUTPUT		(HFI_COMMON_BASE + 0xA)
#define HFI_BUFFER_EXTRADATA_OUTPUT2		(HFI_COMMON_BASE + 0xB)
#define HFI_BUFFER_EXTRADATA_INPUT		(HFI_COMMON_BASE + 0xC)


#define HFI_PROPERTY_SYS_COMMON_START		\
	(HFI_DOMAIN_BASE_COMMON + HFI_ARCH_COMMON_OFFSET + 0x0000)
#define HFI_PROPERTY_SYS_DEBUG_CONFIG		\
	(HFI_PROPERTY_SYS_COMMON_START + 0x001)
#define HFI_PROPERTY_SYS_RESOURCE_OCMEM_REQUIREMENT_INFO	\
	(HFI_PROPERTY_SYS_COMMON_START + 0x002)
#define HFI_PROPERTY_SYS_CONFIG_VCODEC_CLKFREQ				\
	(HFI_PROPERTY_SYS_COMMON_START + 0x003)
#define HFI_PROPERTY_SYS_IDLE_INDICATOR         \
	(HFI_PROPERTY_SYS_COMMON_START + 0x004)
#define  HFI_PROPERTY_SYS_CODEC_POWER_PLANE_CTRL     \
	(HFI_PROPERTY_SYS_COMMON_START + 0x005)
#define  HFI_PROPERTY_SYS_IMAGE_VERSION    \
	(HFI_PROPERTY_SYS_COMMON_START + 0x006)
#define  HFI_PROPERTY_SYS_CONFIG_COVERAGE    \
	(HFI_PROPERTY_SYS_COMMON_START + 0x007)
#define  HFI_PROPERTY_SYS_UBWC_CONFIG    \
	(HFI_PROPERTY_SYS_COMMON_START + 0x008)

#define HFI_DEBUG_MSG_LOW					0x00000001
#define HFI_DEBUG_MSG_MEDIUM					0x00000002
#define HFI_DEBUG_MSG_HIGH					0x00000004
#define HFI_DEBUG_MSG_ERROR					0x00000008
#define HFI_DEBUG_MSG_FATAL					0x00000010
#define HFI_DEBUG_MSG_PERF					0x00000020

#define HFI_DEBUG_MODE_QUEUE					0x00000001
#define HFI_DEBUG_MODE_QDSS					0x00000002

struct cvp_hfi_debug_config {
	u32 debug_config;
	u32 debug_mode;
};

struct cvp_hfi_enable {
	u32 enable;
};

#define HFI_RESOURCE_SYSCACHE 0x00000002

struct cvp_hfi_resource_subcache_type {
	u32 size;
	u32 sc_id;
};

struct cvp_hfi_resource_syscache_info_type {
	u32 num_entries;
	struct cvp_hfi_resource_subcache_type rg_subcache_entries[1];
};

#define HFI_CMD_SYS_COMMON_START			\
(HFI_DOMAIN_BASE_COMMON + HFI_ARCH_COMMON_OFFSET + HFI_CMD_START_OFFSET \
	+ 0x0000)
#define HFI_CMD_SYS_INIT		(HFI_CMD_SYS_COMMON_START + 0x001)
#define HFI_CMD_SYS_PC_PREP		(HFI_CMD_SYS_COMMON_START + 0x002)
#define HFI_CMD_SYS_SET_RESOURCE	(HFI_CMD_SYS_COMMON_START + 0x003)
#define HFI_CMD_SYS_RELEASE_RESOURCE (HFI_CMD_SYS_COMMON_START + 0x004)
#define HFI_CMD_SYS_SET_PROPERTY	(HFI_CMD_SYS_COMMON_START + 0x005)
#define HFI_CMD_SYS_GET_PROPERTY	(HFI_CMD_SYS_COMMON_START + 0x006)
#define HFI_CMD_SYS_SESSION_INIT	(HFI_CMD_SYS_COMMON_START + 0x007)
#define HFI_CMD_SYS_SESSION_END		(HFI_CMD_SYS_COMMON_START + 0x008)
#define HFI_CMD_SYS_SET_BUFFERS		(HFI_CMD_SYS_COMMON_START + 0x009)
#define HFI_CMD_SYS_SESSION_ABORT	(HFI_CMD_SYS_COMMON_START + 0x00A)
#define HFI_CMD_SYS_TEST_START		(HFI_CMD_SYS_COMMON_START + 0x100)

#define HFI_MSG_SYS_COMMON_START			\
	(HFI_DOMAIN_BASE_COMMON + HFI_ARCH_COMMON_OFFSET +	\
	HFI_MSG_START_OFFSET + 0x0000)
#define HFI_MSG_SYS_INIT_DONE			(HFI_MSG_SYS_COMMON_START + 0x1)
#define HFI_MSG_SYS_PC_PREP_DONE		(HFI_MSG_SYS_COMMON_START + 0x2)
#define HFI_MSG_SYS_RELEASE_RESOURCE	(HFI_MSG_SYS_COMMON_START + 0x3)
#define HFI_MSG_SYS_DEBUG			(HFI_MSG_SYS_COMMON_START + 0x4)
#define HFI_MSG_SYS_SESSION_INIT_DONE	(HFI_MSG_SYS_COMMON_START + 0x6)
#define HFI_MSG_SYS_SESSION_END_DONE	(HFI_MSG_SYS_COMMON_START + 0x7)
#define HFI_MSG_SYS_IDLE		(HFI_MSG_SYS_COMMON_START + 0x8)
#define HFI_MSG_SYS_COV                 (HFI_MSG_SYS_COMMON_START + 0x9)
#define HFI_MSG_SYS_PROPERTY_INFO	(HFI_MSG_SYS_COMMON_START + 0xA)
#define HFI_MSG_SYS_SESSION_ABORT_DONE	(HFI_MSG_SYS_COMMON_START + 0xC)
#define HFI_MSG_SESSION_SYNC_DONE      (HFI_MSG_SESSION_OX_START + 0xD)

#define HFI_MSG_SESSION_COMMON_START		\
	(HFI_DOMAIN_BASE_COMMON + HFI_ARCH_COMMON_OFFSET +	\
	HFI_MSG_START_OFFSET + 0x1000)
#define HFI_MSG_EVENT_NOTIFY	(HFI_MSG_SESSION_COMMON_START + 0x1)
#define HFI_MSG_SESSION_GET_SEQUENCE_HEADER_DONE	\
	(HFI_MSG_SESSION_COMMON_START + 0x2)

#define HFI_CMD_SYS_TEST_SSR	(HFI_CMD_SYS_TEST_START + 0x1)
#define HFI_TEST_SSR_SW_ERR_FATAL	0x1
#define HFI_TEST_SSR_SW_DIV_BY_ZERO	0x2
#define HFI_TEST_SSR_HW_WDOG_IRQ	0x3

struct cvp_hal_cmd_pkt_hdr {
	u32 size;
	u32 packet_type;
};

struct cvp_hal_msg_pkt_hdr {
	u32 size;
	u32 packet;
};

struct cvp_hal_session_cmd_pkt {
	u32 size;
	u32 packet_type;
	u32 session_id;
};

struct cvp_hfi_cmd_sys_init_packet {
	u32 size;
	u32 packet_type;
	u32 arch_type;
};

struct cvp_hfi_cmd_sys_pc_prep_packet {
	u32 size;
	u32 packet_type;
};

struct cvp_hfi_cmd_sys_set_resource_packet {
	u32 size;
	u32 packet_type;
	u32 resource_handle;
	u32 resource_type;
	u32 rg_resource_data[1];
};

struct cvp_hfi_cmd_sys_release_resource_packet {
	u32 size;
	u32 packet_type;
	u32 resource_type;
	u32 resource_handle;
};

struct cvp_hfi_cmd_sys_set_property_packet {
	u32 size;
	u32 packet_type;
	u32 num_properties;
	u32 rg_property_data[1];
};

struct cvp_hfi_cmd_sys_get_property_packet {
	u32 size;
	u32 packet_type;
	u32 num_properties;
	u32 rg_property_data[1];
};

enum HFI_SESSION_TYPE {
	HFI_SESSION_CV = 1,
	HFI_SESSION_DME,
	HFI_SESSION_ODT,
	HFI_SESSION_FD
};

struct cvp_hfi_cmd_sys_session_init_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	u32 session_type;
	u32 session_kmask;
	u32 session_prio;
	u32 is_secure;
	u32 dsp_ac_mask;
};

struct cvp_hfi_cmd_sys_session_end_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
};

struct cvp_hfi_cmd_sys_set_buffers_packet {
	u32 size;
	u32 packet_type;
	u32 buffer_type;
	u32 buffer_size;
	u32 num_buffers;
	u32 rg_buffer_addr[1];
};

struct cvp_hfi_cmd_sys_set_ubwc_config_packet_type {
	u32 size;
	u32 packet_type;
	struct {
		u32 max_channel_override : 1;
		u32 mal_length_override : 1;
		u32 hb_override : 1;
		u32 bank_swzl_level_override : 1;
		u32 bank_spreading_override : 1;
		u32 reserved : 27;
	} override_bit_info;
	u32 max_channels;
	u32 mal_length;
	u32 highest_bank_bit;
	u32 bank_swzl_level;
	u32 bank_spreading;
	u32 reserved[2];
};

struct cvp_hfi_cmd_session_set_property_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	u32 num_properties;
	u32 rg_property_data[1];
};

struct cvp_hfi_client {
	u32 transaction_id;
	u32 data1;
	u32 data2;
	u32 kdata1;
	u32 kdata2;
	u32 reserved1;
	u32 reserved2;
};

struct cvp_hfi_client_d {
	u32 transaction_id;
	u32 data1;
	u32 data2;
};

struct cvp_buf_desc {
	u32 fd;
	u32 size;
};

struct cvp_hfi_buf_type {
	s32 fd;
	u32 size;
	u32 offset;
	u32 flags;
	u32 reserved1;
	u32 reserved2;
};

struct cvp_hfi_cmd_session_set_buffers_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	struct cvp_hfi_client client_data;
	struct cvp_hfi_buf_type buf_type;
};

struct cvp_hfi_cmd_session_set_buffers_packet_d {
	u32 size;
	u32 packet_type;
	u32 session_id;
	struct cvp_hfi_client_d client_data;
	u32 buffer_addr;
	u32 buffer_size;
};

struct cvp_session_release_buffers_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	struct cvp_hfi_client client_data;
	u32 kernel_type;
	u32 buffer_type;
	u32 num_buffers;
	u32 buffer_idx;
};

struct cvp_session_release_buffers_packet_d {
	u32 size;
	u32 packet_type;
	u32 session_id;
	struct cvp_hfi_client_d client_data;
	u32 buffer_type;
	u32 num_buffers;
	u32 buffer_idx;
};

struct cvp_hfi_cmd_session_hdr {
	u32 size;
	u32 packet_type;
	u32 session_id;
	struct cvp_hfi_client client_data;
	u32 stream_idx;
};

struct cvp_hfi_msg_session_hdr {
	u32 size;
	u32 packet_type;
	u32 session_id;
	u32 error_type;
	struct cvp_hfi_client client_data;
	u32 stream_idx;
};

struct cvp_hfi_msg_session_hdr_d {
	u32 size;
	u32 packet_type;
	u32 session_id;
	struct cvp_hfi_client_d client_data;
	u32 error_type;
};

struct cvp_hfi_buffer_mapping_type {
	u32 index;
	u32 device_addr;
	u32 size;
};

struct cvp_hfi_cmd_session_sync_process_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	u32 sync_id;
	u32 rg_data[1];
};

struct cvp_hfi_msg_event_notify_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	u32 event_id;
	u32 event_data1;
	u32 event_data2;
	u32 rg_ext_event_data[1];
};

struct cvp_hfi_msg_session_op_cfg_packet_d {
	u32 size;
	u32 packet_type;
	u32 session_id;
	struct cvp_hfi_client_d  client_data;
	u32 op_conf_id;
	u32 error_type;
};

struct cvp_hfi_msg_session_op_cfg_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	u32 error_type;
	struct cvp_hfi_client client_data;
	u32 stream_idx;
	u32 op_conf_id;
};

struct cvp_hfi_msg_release_buffer_ref_event_packet {
	u32 packet_buffer;
	u32 extra_data_buffer;
	u32 output_tag;
};

struct cvp_hfi_msg_sys_init_done_packet {
	u32 size;
	u32 packet_type;
	u32 error_type;
	u32 num_properties;
	u32 rg_property_data[1];
};

struct cvp_hfi_msg_sys_pc_prep_done_packet {
	u32 size;
	u32 packet_type;
	u32 error_type;
};

struct cvp_hfi_msg_sys_release_resource_done_packet {
	u32 size;
	u32 packet_type;
	u32 resource_handle;
	u32 error_type;
};

struct cvp_hfi_msg_sys_session_init_done_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	u32 error_type;
	u32 num_properties;
	u32 rg_property_data[1];
};

struct cvp_hfi_msg_sys_session_end_done_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	u32 error_type;
};

struct cvp_hfi_msg_session_get_sequence_header_done_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	u32 error_type;
	u32 header_len;
	u32 sequence_header;
};

struct cvp_hfi_msg_sys_debug_packet {
	u32 size;
	u32 packet_type;
	u32 msg_type;
	u32 msg_size;
	u32 time_stamp_hi;
	u32 time_stamp_lo;
	u8 rg_msg_data[1];
};

struct cvp_hfi_packet_header {
	u32 size;
	u32 packet_type;
};

struct cvp_hfi_sfr_struct {
	u32 bufSize;
	u8 rg_data[1];
};

struct cvp_hfi_cmd_sys_test_ssr_packet {
	u32 size;
	u32 packet_type;
	u32 trigger_type;
};

#endif
