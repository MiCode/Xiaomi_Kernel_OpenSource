/* Copyright (c) 2012-2015, 2018 The Linux Foundation. All rights reserved.
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
#ifndef __HFI_PACKETIZATION__
#define __HFI_PACKETIZATION__

#include <linux/types.h>
#include "vidc_hfi_helper.h"
#include "vidc_hfi.h"
#include "vidc_hfi_api.h"

#define call_hfi_pkt_op(q, op, args...)			\
	(((q) && (q)->pkt_ops && (q)->pkt_ops->op) ?	\
	((q)->pkt_ops->op(args)) : 0)

enum hfi_packetization_type {
	HFI_PACKETIZATION_LEGACY,
	HFI_PACKETIZATION_3XX,
};

struct hfi_packetization_ops {
	int (*sys_init)(struct hfi_cmd_sys_init_packet *pkt, u32 arch_type);
	int (*sys_pc_prep)(struct hfi_cmd_sys_pc_prep_packet *pkt);
	int (*sys_idle_indicator)(struct hfi_cmd_sys_set_property_packet *pkt,
		u32 enable);
	int (*sys_power_control)(struct hfi_cmd_sys_set_property_packet *pkt,
		u32 enable);
	int (*sys_set_resource)(
		struct hfi_cmd_sys_set_resource_packet *pkt,
		struct vidc_resource_hdr *resource_hdr,
		void *resource_value);
	int (*sys_debug_config)(struct hfi_cmd_sys_set_property_packet *pkt,
			u32 mode);
	int (*sys_coverage_config)(struct hfi_cmd_sys_set_property_packet *pkt,
			u32 mode);
	int (*sys_release_resource)(
		struct hfi_cmd_sys_release_resource_packet *pkt,
		struct vidc_resource_hdr *resource_hdr);
	int (*sys_ping)(struct hfi_cmd_sys_ping_packet *pkt);
	int (*sys_image_version)(struct hfi_cmd_sys_get_property_packet *pkt);
	int (*ssr_cmd)(enum hal_ssr_trigger_type type,
		struct hfi_cmd_sys_test_ssr_packet *pkt);
	int (*session_init)(
		struct hfi_cmd_sys_session_init_packet *pkt,
		struct hal_session *session,
		u32 session_domain, u32 session_codec);
	int (*session_cmd)(struct vidc_hal_session_cmd_pkt *pkt,
		int pkt_type, struct hal_session *session);
	int (*session_set_buffers)(
		struct hfi_cmd_session_set_buffers_packet *pkt,
		struct hal_session *session,
		struct vidc_buffer_addr_info *buffer_info);
	int (*session_release_buffers)(
		struct hfi_cmd_session_release_buffer_packet *pkt,
		struct hal_session *session,
		struct vidc_buffer_addr_info *buffer_info);
	int (*session_etb_decoder)(
		struct hfi_cmd_session_empty_buffer_compressed_packet *pkt,
		struct hal_session *session,
		struct vidc_frame_data *input_frame);
	int (*session_etb_encoder)(
		struct hfi_cmd_session_empty_buffer_uncompressed_plane0_packet
		*pkt, struct hal_session *session,
		struct vidc_frame_data *input_frame);
	int (*session_ftb)(struct hfi_cmd_session_fill_buffer_packet *pkt,
		struct hal_session *session,
		struct vidc_frame_data *output_frame);
	int (*session_parse_seq_header)(
		struct hfi_cmd_session_parse_sequence_header_packet *pkt,
		struct hal_session *session, struct vidc_seq_hdr *seq_hdr);
	int (*session_get_seq_hdr)(
		struct hfi_cmd_session_get_sequence_header_packet *pkt,
		struct hal_session *session, struct vidc_seq_hdr *seq_hdr);
	int (*session_get_buf_req)(
		struct hfi_cmd_session_get_property_packet *pkt,
		struct hal_session *session);
	int (*session_flush)(struct hfi_cmd_session_flush_packet *pkt,
		struct hal_session *session, enum hal_flush flush_mode);
	int (*session_get_property)(
		struct hfi_cmd_session_get_property_packet *pkt,
		struct hal_session *session, enum hal_property ptype);
	int (*session_set_property)(
		struct hfi_cmd_session_set_property_packet *pkt,
		struct hal_session *session,
		enum hal_property ptype, void *pdata);
	int (*session_sync_process)(
		struct hfi_cmd_session_sync_process_packet *pkt,
		struct hal_session *session);
};

struct hfi_packetization_ops *hfi_get_pkt_ops_handle(
		enum hfi_packetization_type);
#endif
