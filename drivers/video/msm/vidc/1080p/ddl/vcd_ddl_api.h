/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
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

#ifndef _VCD_DDL_API_H_
#define _VCD_DDL_API_H_

#include <media/msm/vcd_api.h>
#include "vidc.h"

#define VCD_EVT_RESP_DDL_BASE             0x3000
#define VCD_EVT_RESP_DEVICE_INIT          (VCD_EVT_RESP_DDL_BASE + 0x1)
#define VCD_EVT_RESP_OUTPUT_REQ           (VCD_EVT_RESP_DDL_BASE + 0x2)
#define VCD_EVT_RESP_EOS_DONE             (VCD_EVT_RESP_DDL_BASE + 0x3)
#define VCD_EVT_RESP_TRANSACTION_PENDING  (VCD_EVT_RESP_DDL_BASE + 0x4)

#define VCD_S_DDL_ERR_BASE       0x90000000
#define VCD_ERR_MAX_NO_CODEC     (VCD_S_DDL_ERR_BASE + 0x1)
#define VCD_ERR_CLIENT_PRESENT   (VCD_S_DDL_ERR_BASE + 0x2)
#define VCD_ERR_CLIENT_FATAL     (VCD_S_DDL_ERR_BASE + 0x3)
#define VCD_ERR_NO_SEQ_HDR       (VCD_S_DDL_ERR_BASE + 0x4)

#define VCD_I_CUSTOM_BASE        (VCD_I_RESERVED_BASE)
#define VCD_I_RC_LEVEL_CONFIG    (VCD_I_CUSTOM_BASE + 0x1)
#define VCD_I_FRAME_LEVEL_RC     (VCD_I_CUSTOM_BASE + 0x2)
#define VCD_I_ADAPTIVE_RC        (VCD_I_CUSTOM_BASE + 0x3)
#define VCD_I_CUSTOM_DDL_BASE    (VCD_I_RESERVED_BASE + 0x100)
#define DDL_I_INPUT_BUF_REQ      (VCD_I_CUSTOM_DDL_BASE + 0x1)
#define DDL_I_OUTPUT_BUF_REQ     (VCD_I_CUSTOM_DDL_BASE + 0x2)
#define DDL_I_DPB                (VCD_I_CUSTOM_DDL_BASE + 0x3)
#define DDL_I_DPB_RELEASE        (VCD_I_CUSTOM_DDL_BASE + 0x4)
#define DDL_I_DPB_RETRIEVE       (VCD_I_CUSTOM_DDL_BASE + 0x5)
#define DDL_I_REQ_OUTPUT_FLUSH   (VCD_I_CUSTOM_DDL_BASE + 0x6)
#define DDL_I_SEQHDR_ALIGN_BYTES (VCD_I_CUSTOM_DDL_BASE + 0x7)
#define DDL_I_CAPABILITY         (VCD_I_CUSTOM_DDL_BASE + 0x8)
#define DDL_I_FRAME_PROC_UNITS   (VCD_I_CUSTOM_DDL_BASE + 0x9)
#define DDL_I_SEQHDR_PRESENT     (VCD_I_CUSTOM_DDL_BASE + 0xA)

#define DDL_FRAME_VGA_SIZE     (640*480)
#define DDL_FRAME_720P_WIDTH   1280
#define DDL_FRAME_720P_HEIGHT  720

struct vcd_property_rc_level{
	u32 frame_level_rc;
	u32 mb_level_rc;
};
struct vcd_property_frame_level_rc_params{
	u32 reaction_coeff;
};
struct vcd_property_adaptive_rc_params{
	u32 disable_dark_region_as_flag;
	u32 disable_smooth_region_as_flag;
	u32 disable_static_region_as_flag;
	u32 disable_activity_region_flag;
};
struct ddl_property_dec_pic_buffers{
	struct ddl_frame_data_tag *dec_pic_buffers;
	u32 no_of_dec_pic_buf;
};
struct ddl_property_capability{
	u32 max_num_client;
	u32 general_command_depth;
	u32 exclusive;
	u32 frame_command_depth;
	u32 ddl_time_out_in_ms;
};
struct ddl_init_config{
	int memtype;
	u8 *core_virtual_base_addr;
	void (*interrupt_clr) (void);
	void (*ddl_callback) (u32 event, u32 status, void *payload, size_t sz,
		u32 *ddl_handle, void *const client_data);
};
struct ddl_frame_data_tag{
	struct vcd_frame_data vcd_frm;
	u32 frm_trans_end;
	u32 frm_delta;
};
u32 ddl_device_init(struct ddl_init_config *ddl_init_config,
	void *client_data);
u32 ddl_device_release(void *client_data);
u32 ddl_open(u32 **ddl_handle, u32 decoding);
u32 ddl_close(u32 **ddl_handle);
u32 ddl_encode_start(u32 *ddl_handle, void *client_data);
u32 ddl_encode_frame(u32 *ddl_handle,
	struct ddl_frame_data_tag *input_frame,
	struct ddl_frame_data_tag *output_bit, void *client_data);
u32 ddl_encode_end(u32 *ddl_handle, void *client_data);
u32 ddl_decode_start(u32 *ddl_handle, struct vcd_sequence_hdr *header,
	void *client_data);
u32 ddl_decode_frame(u32 *ddl_handle,
	struct ddl_frame_data_tag *input_bits, void *client_data);
u32 ddl_decode_end(u32 *ddl_handle, void *client_data);
u32 ddl_set_property(u32 *ddl_handle,
	struct vcd_property_hdr *property_hdr, void *property_value);
u32 ddl_get_property(u32 *ddl_handle,
	struct vcd_property_hdr *property_hdr, void *property_value);
u32 ddl_process_core_response(void);
u32 ddl_reset_hw(u32 mode);
void ddl_read_and_clear_interrupt(void);
#endif
