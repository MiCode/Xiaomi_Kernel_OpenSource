/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2020, The Linux Foundation. All rights reserved.
 */
#ifndef __HFI_PACKETIZATION_H__
#define __HFI_PACKETIZATION_H__

#include "vidc_hfi_helper.h"
#include "vidc_hfi.h"
#include "vidc_hfi_api.h"

#define call_hfi_pkt_op(q, op, ...)			\
	(((q) && (q)->pkt_ops && (q)->pkt_ops->op) ?	\
	((q)->pkt_ops->op(__VA_ARGS__)) : 0)

enum hfi_packetization_type {
	HFI_PACKETIZATION_4XX,
};

struct hfi_packetization_ops {
	int (*sys_init)(struct hfi_cmd_sys_init_packet *pkt, u32 arch_type);
	int (*sys_pc_prep)(struct hfi_cmd_sys_pc_prep_packet *pkt);
	int (*sys_power_control)(struct hfi_cmd_sys_set_property_packet *pkt,
		u32 enable);
	int (*sys_set_resource)(
		struct hfi_cmd_sys_set_resource_packet *pkt,
		struct vidc_resource_hdr *resource_hdr,
		void *resource_value);
	int (*sys_debug_config)(struct hfi_cmd_sys_set_property_packet *pkt,
			u32 mode);
	int (*sys_coverage_config)(struct hfi_cmd_sys_set_property_packet *pkt,
			u32 mode, u32 sid);
	int (*sys_release_resource)(
		struct hfi_cmd_sys_release_resource_packet *pkt,
		struct vidc_resource_hdr *resource_hdr);
	int (*sys_image_version)(struct hfi_cmd_sys_get_property_packet *pkt);
	int (*sys_ubwc_config)(struct hfi_cmd_sys_set_property_packet *pkt,
		struct msm_vidc_ubwc_config_data *ubwc_config);
	int (*ssr_cmd)(struct hfi_cmd_sys_test_ssr_packet *pkt,
		enum hal_ssr_trigger_type ssr_type, u32 sub_client_id,
		u32 test_addr);
	int (*session_init)(
		struct hfi_cmd_sys_session_init_packet *pkt,
		u32 sid, u32 session_domain, u32 session_codec);
	int (*session_cmd)(struct vidc_hal_session_cmd_pkt *pkt,
		int pkt_type, u32 sid);
	int (*session_set_buffers)(
		struct hfi_cmd_session_set_buffers_packet *pkt,
		u32 sid, struct vidc_buffer_addr_info *buffer_info);
	int (*session_release_buffers)(
		struct hfi_cmd_session_release_buffer_packet *pkt,
		u32 sid, struct vidc_buffer_addr_info *buffer_info);
	int (*session_etb_decoder)(
		struct hfi_cmd_session_empty_buffer_compressed_packet *pkt,
		u32 sid, struct vidc_frame_data *input_frame);
	int (*session_etb_encoder)(
		struct hfi_cmd_session_empty_buffer_uncompressed_plane0_packet
		*pkt, u32 sid, struct vidc_frame_data *input_frame);
	int (*session_ftb)(struct hfi_cmd_session_fill_buffer_packet *pkt,
		u32 sid, struct vidc_frame_data *output_frame);
	int (*session_get_buf_req)(
		struct hfi_cmd_session_get_property_packet *pkt, u32 sid);
	int (*session_flush)(struct hfi_cmd_session_flush_packet *pkt,
		u32 sid, enum hal_flush flush_mode);
	int (*session_set_property)(
		struct hfi_cmd_session_set_property_packet *pkt,
		u32 sid, u32 ptype, void *pdata, u32 size);
};

struct hfi_packetization_ops *hfi_get_pkt_ops_handle(
		enum hfi_packetization_type);
#endif
