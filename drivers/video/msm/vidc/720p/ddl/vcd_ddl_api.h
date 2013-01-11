/* Copyright (c) 2010, The Linux Foundation. All rights reserved.
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
#include "vcd_ddl_internal_property.h"

struct ddl_init_config {
	int memtype;
	u8 *core_virtual_base_addr;
	void (*interrupt_clr) (void);
	void (*ddl_callback) (u32 event, u32 status, void *payload, size_t sz,
		u32 *ddl_handle, void *const client_data);
};

struct ddl_frame_data_tag {
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
void ddl_read_and_clear_interrupt(void);
u32 ddl_process_core_response(void);
u32 ddl_reset_hw(u32 mode);
#endif
