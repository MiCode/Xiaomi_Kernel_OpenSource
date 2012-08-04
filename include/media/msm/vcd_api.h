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
#ifndef _VCD_API_H_
#define _VCD_API_H_
#include "vcd_property.h"
#include "vcd_status.h"

#define VCD_FRAME_FLAG_EOS 0x00000001
#define VCD_FRAME_FLAG_DECODEONLY   0x00000004
#define VCD_FRAME_FLAG_DATACORRUPT 0x00000008
#define VCD_FRAME_FLAG_ENDOFFRAME 0x00000010
#define VCD_FRAME_FLAG_SYNCFRAME 0x00000020
#define VCD_FRAME_FLAG_EXTRADATA 0x00000040
#define VCD_FRAME_FLAG_CODECCONFIG  0x00000080
#define VCD_FRAME_FLAG_BFRAME 0x00100000
#define VCD_FRAME_FLAG_EOSEQ 0x00200000

#define VCD_FLUSH_INPUT   0x0001
#define VCD_FLUSH_OUTPUT  0x0002
#define VCD_FLUSH_ALL     0x0003

#define VCD_FRAMETAG_INVALID  0xffffffff

struct vcd_handle_container {
	void *handle;
};
struct vcd_flush_cmd {
	u32 mode;
};

enum vcd_frame {
	VCD_FRAME_YUV = 1,
	VCD_FRAME_I,
	VCD_FRAME_P,
	VCD_FRAME_B,
	VCD_FRAME_NOTCODED,
	VCD_FRAME_IDR,
	VCD_FRAME_32BIT = 0x7fffffff
};

enum vcd_power_state {
	VCD_PWR_STATE_ON = 1,
	VCD_PWR_STATE_SLEEP,
};

struct vcd_aspect_ratio {
	u32 aspect_ratio;
	u32 par_width;
	u32 par_height;
};

struct vcd_frame_data {
	u8 *virtual;
	u8 *physical;
	u32 ion_flag;
	u32 alloc_len;
	u32 data_len;
	u32 offset;
	s64 time_stamp; /* in usecs*/
	u32 flags;
	u32 frm_clnt_data;
	struct vcd_property_dec_output_buffer dec_op_prop;
	u32 interlaced;
	enum vcd_frame frame;
	u32 ip_frm_tag;
	u32 intrlcd_ip_frm_tag;
	u8 *desc_buf;
	u32 desc_size;
	struct ion_handle *buff_ion_handle;
	struct vcd_aspect_ratio aspect_ratio_info;
};

struct vcd_sequence_hdr {
	u8 *sequence_header;
	u32 sequence_header_len;

};

enum vcd_buffer_type {
	VCD_BUFFER_INPUT = 0x1,
	VCD_BUFFER_OUTPUT = 0x2,
	VCD_BUFFER_INVALID = 0x3,
	VCD_BUFFER_32BIT = 0x7FFFFFFF
};

struct vcd_buffer_requirement {
	u32 min_count;
	u32 actual_count;
	u32 max_count;
	size_t sz;
	u32 align;
	u32 buf_pool_id;
};

struct vcd_init_config {
	void *device_name;
	void *(*map_dev_base_addr) (void *device_name);
	void (*un_map_dev_base_addr) (void);
	void (*interrupt_clr) (void);
	void (*register_isr) (void *device_name);
	void (*deregister_isr) (void);
	u32  (*timer_create) (void (*timer_handler)(void *),
		void *user_data, void **timer_handle);
	void (*timer_release) (void *timer_handle);
	void (*timer_start) (void *timer_handle, u32 time_out);
	void (*timer_stop) (void *timer_handle);
};

/*Flags passed to vcd_open*/
#define VCD_CP_SESSION 0x00000001

u32 vcd_init(struct vcd_init_config *config, s32 *driver_handle);
u32 vcd_term(s32 driver_handle);
u32 vcd_open(s32 driver_handle, u32 decoding,
	void (*callback) (u32 event, u32 status, void *info, size_t sz,
	void *handle, void *const client_data), void *client_data, int flags);
u32 vcd_close(void *handle);
u32 vcd_encode_start(void *handle);
u32 vcd_encode_frame(void *handle, struct vcd_frame_data *input_frame);
u32 vcd_decode_start(void *handle, struct vcd_sequence_hdr *seq_hdr);
u32 vcd_decode_frame(void *handle, struct vcd_frame_data *input_frame);
u32 vcd_pause(void *handle);
u32 vcd_resume(void *handle);
u32 vcd_flush(void *handle, u32 mode);
u32 vcd_stop(void *handle);
u32 vcd_set_property(void *handle, struct vcd_property_hdr *prop_hdr,
					void *prop_val);
u32 vcd_get_property(void *handle, struct vcd_property_hdr *prop_hdr,
					 void *prop_val);
u32 vcd_set_buffer_requirements(void *handle, enum vcd_buffer_type buffer,
		struct vcd_buffer_requirement *buffer_req);
u32 vcd_get_buffer_requirements(void *handle, enum vcd_buffer_type buffer,
		struct vcd_buffer_requirement *buffer_req);
u32 vcd_set_buffer(void *handle, enum vcd_buffer_type buffer_type,
		u8 *buffer, u32 buf_size);
u32 vcd_allocate_buffer(void *handle, enum vcd_buffer_type buffer,
		u32 buf_size, u8 **vir_buf_addr, u8 **phy_buf_addr);

u32 vcd_free_buffer(void *handle, enum vcd_buffer_type buffer_type, u8 *buffer);
u32 vcd_fill_output_buffer(void *handle, struct vcd_frame_data *buffer);
u32 vcd_set_device_power(s32 driver_handle,
		enum vcd_power_state pwr_state);
void vcd_read_and_clear_interrupt(void);
void vcd_response_handler(void);
u8 vcd_get_num_of_clients(void);
u32 vcd_get_ion_status(void);
struct ion_client *vcd_get_ion_client(void);
#endif
