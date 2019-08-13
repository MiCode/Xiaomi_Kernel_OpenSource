/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */
#ifndef __HFI_PACKETIZATION__
#define __HFI_PACKETIZATION__

#include <linux/types.h>
#include "cvp_hfi_helper.h"
#include "cvp_hfi.h"
#include "cvp_hfi_api.h"

#define call_hfi_pkt_op(q, op, ...)			\
	(((q) && (q)->pkt_ops && (q)->pkt_ops->op) ?	\
	((q)->pkt_ops->op(__VA_ARGS__)) : 0)

enum hfi_packetization_type {
	HFI_PACKETIZATION_4XX,
};

struct cvp_hfi_packetization_ops {
	int (*sys_init)(struct cvp_hfi_cmd_sys_init_packet *pkt, u32 arch_type);
	int (*sys_pc_prep)(struct cvp_hfi_cmd_sys_pc_prep_packet *pkt);
	int (*sys_power_control)(
		struct cvp_hfi_cmd_sys_set_property_packet *pkt,
		u32 enable);
	int (*sys_set_resource)(
		struct cvp_hfi_cmd_sys_set_resource_packet *pkt,
		struct cvp_resource_hdr *resource_hdr,
		void *resource_value);
	int (*sys_debug_config)(struct cvp_hfi_cmd_sys_set_property_packet *pkt,
			u32 mode);
	int (*sys_coverage_config)(
			struct cvp_hfi_cmd_sys_set_property_packet *pkt,
			u32 mode);
	int (*sys_set_idle_indicator)(
		struct cvp_hfi_cmd_sys_set_property_packet *pkt,
		u32 mode);
	int (*sys_release_resource)(
		struct cvp_hfi_cmd_sys_release_resource_packet *pkt,
		struct cvp_resource_hdr *resource_hdr);
	int (*sys_image_version)(
			struct cvp_hfi_cmd_sys_get_property_packet *pkt);
	int (*sys_ubwc_config)(struct cvp_hfi_cmd_sys_set_property_packet *pkt,
		struct msm_cvp_ubwc_config_data *ubwc_config);
	int (*ssr_cmd)(enum hal_ssr_trigger_type type,
		struct cvp_hfi_cmd_sys_test_ssr_packet *pkt);
	int (*session_init)(
		struct cvp_hfi_cmd_sys_session_init_packet *pkt,
		struct cvp_hal_session *session);
	int (*session_cmd)(struct cvp_hal_session_cmd_pkt *pkt,
		int pkt_type, struct cvp_hal_session *session);
	int (*session_set_buffers)(
		void *pkt,
		struct cvp_hal_session *session,
		struct cvp_buffer_addr_info *buffer_info);
	int (*session_release_buffers)(
		void *pkt,
		struct cvp_hal_session *session,
		struct cvp_buffer_addr_info *buffer_info);
	int (*session_get_buf_req)(
		struct cvp_hfi_cmd_session_get_property_packet *pkt,
		struct cvp_hal_session *session);
	int (*session_flush)(struct cvp_hfi_cmd_session_flush_packet *pkt,
		struct cvp_hal_session *session, enum hal_flush flush_mode);
	int (*session_sync_process)(
		struct cvp_hfi_cmd_session_sync_process_packet *pkt,
		struct cvp_hal_session *session);
	int (*session_send)(
			struct cvp_kmd_hfi_packet *out_pkt,
			struct cvp_hal_session *session,
			struct cvp_kmd_hfi_packet *in_pkt);
};

struct cvp_hfi_packetization_ops *cvp_hfi_get_pkt_ops_handle(
		enum hfi_packetization_type);
#endif
