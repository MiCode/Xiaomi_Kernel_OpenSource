/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

int create_pkt_cmd_sys_init(struct hfi_cmd_sys_init_packet *pkt,
							   u32 arch_type);
int create_pkt_cmd_sys_pc_prep(struct hfi_cmd_sys_pc_prep_packet *pkt);

int create_pkt_cmd_sys_idle_indicator(
		struct hfi_cmd_sys_set_property_packet *pkt,
		u32 enable);

int create_pkt_set_cmd_sys_resource(
		struct hfi_cmd_sys_set_resource_packet *pkt,
		struct vidc_resource_hdr *resource_hdr,
		void *resource_value);

int create_pkt_cmd_sys_debug_config(
		struct hfi_cmd_sys_set_property_packet *pkt,
		u32 mode);

int create_pkt_cmd_sys_release_resource(
		struct hfi_cmd_sys_release_resource_packet *pkt,
		struct vidc_resource_hdr *resource_hdr);

int create_pkt_cmd_sys_ping(struct hfi_cmd_sys_ping_packet *pkt);

int create_pkt_cmd_sys_session_init(
	struct hfi_cmd_sys_session_init_packet *pkt,
	u32 session_id, u32 session_domain, u32 session_codec);

int create_pkt_cmd_session_cmd(struct vidc_hal_session_cmd_pkt *pkt,
			int pkt_type, u32 session_id);

int create_pkt_cmd_session_set_buffers(
		struct hfi_cmd_session_set_buffers_packet *pkt,
		u32 session_id,
		struct vidc_buffer_addr_info *buffer_info);

int create_pkt_cmd_session_release_buffers(
		struct hfi_cmd_session_release_buffer_packet *pkt,
		u32 session_id, struct vidc_buffer_addr_info *buffer_info);

int create_pkt_cmd_session_etb_decoder(
	struct hfi_cmd_session_empty_buffer_compressed_packet *pkt,
	u32 session_id, struct vidc_frame_data *input_frame);

int create_pkt_cmd_session_etb_encoder(
	struct hfi_cmd_session_empty_buffer_uncompressed_plane0_packet *pkt,
	u32 session_id, struct vidc_frame_data *input_frame);

int create_pkt_cmd_session_ftb(struct hfi_cmd_session_fill_buffer_packet *pkt,
		u32 session_id, struct vidc_frame_data *output_frame);

int create_pkt_cmd_session_parse_seq_header(
		struct hfi_cmd_session_parse_sequence_header_packet *pkt,
		u32 session_id, struct vidc_seq_hdr *seq_hdr);

int create_pkt_cmd_session_get_seq_hdr(
		struct hfi_cmd_session_get_sequence_header_packet *pkt,
		u32 session_id, struct vidc_seq_hdr *seq_hdr);

int create_pkt_cmd_session_get_buf_req(
		struct hfi_cmd_session_get_property_packet *pkt,
		u32 session_id);

int create_pkt_cmd_session_flush(struct hfi_cmd_session_flush_packet *pkt,
			u32 session_id, enum hal_flush flush_mode);

int create_pkt_cmd_session_set_property(
		struct hfi_cmd_session_set_property_packet *pkt,
		u32 session_id, enum hal_property ptype, void *pdata);

int create_pkt_ssr_cmd(enum hal_ssr_trigger_type type,
		struct hfi_cmd_sys_test_ssr_packet *pkt);

int create_pkt_cmd_sys_image_version(
		struct hfi_cmd_sys_get_property_packet *pkt);
#endif
