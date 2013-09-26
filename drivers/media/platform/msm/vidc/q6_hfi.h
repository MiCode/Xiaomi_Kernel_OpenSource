/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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
#ifndef __Q6_HFI_H__
#define __Q6_HFI_H__

#include <mach/qdsp6v2/apr.h>
#include "vidc_hfi.h"
#include "vidc_hfi_helper.h"
#include "msm_vidc_resources.h"

#define Q6_IFACEQ_QUEUE_SIZE (8 * 1024)

/* client to Q6 communication path : forward path */
#define VIDEO_HFI_CMD_ID  0x00012ECC

/* Q6 to client ACK msg: reverse path*/
#define VIDEO_HFI_MSG_ID  0x00012ECD

/* Q6 to client event notifications */
#define VIDEO_HFI_EVT_ID  0x00012ECE

struct q6_resources {
	struct msm_vidc_fw fw;
};

struct q6_iface_q_info {
	spinlock_t lock;
	u32 q_size;
	u32 read_idx;
	u32 write_idx;
	u8 *buffer;
};

struct q6_hfi_device {
	struct list_head list;
	struct list_head sess_head;
	struct q6_iface_q_info event_queue;
	struct workqueue_struct *vidc_workq;
	struct work_struct vidc_worker;
	u32 device_id;
	msm_vidc_callback callback;
	struct q6_resources resources;
	struct msm_vidc_platform_resources *res;
	void *apr;
	struct mutex session_lock;
};

struct q6_apr_cmd_sys_init_packet {
	struct apr_hdr hdr;
	struct hfi_cmd_sys_init_packet pkt;
};

struct q6_apr_cmd_sys_session_init_packet {
	struct apr_hdr hdr;
	struct hfi_cmd_sys_session_init_packet pkt;
};

struct q6_apr_session_cmd_pkt {
	struct apr_hdr hdr;
	struct vidc_hal_session_cmd_pkt pkt;
};

struct q6_apr_cmd_session_set_buffers_packet {
	struct apr_hdr hdr;
	struct hfi_cmd_session_set_buffers_packet pkt;
};

struct q6_apr_cmd_session_release_buffer_packet {
	struct apr_hdr hdr;
	struct hfi_cmd_session_release_buffer_packet pkt;
};

struct q6_apr_cmd_session_empty_buffer_compressed_packet {
	struct apr_hdr hdr;
	struct hfi_cmd_session_empty_buffer_compressed_packet pkt;
};

struct q6_apr_cmd_session_empty_buffer_uncompressed_plane0_packet {
	struct apr_hdr hdr;
	struct hfi_cmd_session_empty_buffer_uncompressed_plane0_packet pkt;
};

struct q6_apr_cmd_session_fill_buffer_packet {
	struct apr_hdr hdr;
	struct hfi_cmd_session_fill_buffer_packet pkt;
};

struct q6_apr_cmd_session_parse_sequence_header_packet {
	struct apr_hdr hdr;
	struct hfi_cmd_session_parse_sequence_header_packet pkt;
};

struct q6_apr_cmd_session_get_sequence_header_packet {
	struct apr_hdr hdr;
	struct hfi_cmd_session_get_sequence_header_packet pkt;
};

struct q6_apr_cmd_session_get_property_packet {
	struct apr_hdr hdr;
	struct hfi_cmd_session_get_property_packet pkt;
};

struct q6_apr_cmd_session_flush_packet {
	struct apr_hdr hdr;
	struct hfi_cmd_session_flush_packet pkt;
};

struct q6_apr_cmd_session_set_property_packet {
	struct apr_hdr hdr;
	struct hfi_cmd_session_set_property_packet pkt;
};

int q6_hfi_initialize(struct hfi_device *hdev, u32 device_id,
		struct msm_vidc_platform_resources *res,
		hfi_cmd_response_callback callback);

void q6_hfi_delete_device(void *device);

#endif /*#ifndef  __Q6_HFI_H__ */
