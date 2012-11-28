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

#ifndef _VCD_DDL_H_
#define _VCD_DDL_H_

#include <mach/msm_subsystem_map.h>
#include "vcd_ddl_api.h"
#include "vcd_ddl_core.h"
#include "vcd_ddl_utils.h"
#include "vidc.h"
#include "vidc_hwio.h"
#include "vidc_pix_cache.h"
#include "vidc.h"

#define DDL_IDLE_STATE  0
#define DDL_BUSY_STATE  1
#define DDL_ERROR_STATE 2
#define DDL_RUN_STATE   3

#define DDL_IS_BUSY(ddl_context) \
		((ddl_context)->ddl_busy == DDL_BUSY_STATE)
#define DDL_IS_IDLE(ddl_context) \
		((ddl_context)->ddl_busy == DDL_IDLE_STATE)
#define DDL_BUSY(ddl_context) \
		((ddl_context)->ddl_busy = DDL_BUSY_STATE)
#define DDL_IDLE(ddl_context) \
		((ddl_context)->ddl_busy = DDL_IDLE_STATE)
#define DDL_ERROR(ddl_context) \
		((ddl_context)->ddl_busy = DDL_ERROR_STATE)
#define DDL_RUN(ddl_context) \
	((ddl_context)->ddl_busy = DDL_RUN_STATE)

#define DDL_DEVICE_NOTINIT  0
#define DDL_DEVICE_INITED   1
#define DDL_DEVICE_HWFATAL  2

#define DDL_IS_INITIALIZED(ddl_context) \
	(ddl_context->device_state == DDL_DEVICE_INITED)
#define DDLCOMMAND_STATE_IS(ddl_context, command_state) \
	(command_state == (ddl_context)->cmd_state)
#define DDLCLIENT_STATE_IS(ddl, state) \
	(state == (ddl)->client_state)

#define DDL_DPB_OP_INIT       1
#define DDL_DPB_OP_MARK_FREE  2
#define DDL_DPB_OP_MARK_BUSY  3
#define DDL_DPB_OP_SET_MASK   4
#define DDL_DPB_OP_RETRIEVE   5

#define DDL_INIT_CLIENTS     0
#define DDL_GET_CLIENT       1
#define DDL_FREE_CLIENT      2
#define DDL_ACTIVE_CLIENT    3

#define DDL_INVALID_CHANNEL_ID  ((u32)~0)
#define DDL_INVALID_CODEC_TYPE  ((u32)~0)
#define DDL_INVALID_INTR_STATUS ((u32)~0)

#define DDL_ENC_REQ_IFRAME        0x01
#define DDL_ENC_CHANGE_IPERIOD    0x02
#define DDL_ENC_CHANGE_BITRATE    0x04
#define DDL_ENC_CHANGE_FRAMERATE  0x08
#define DDL_ENC_CHANGE_CIR        0x10

#define DDL_DEC_REQ_OUTPUT_FLUSH  0x1

#define DDL_MIN_NUM_OF_B_FRAME  0
#define DDL_MAX_NUM_OF_B_FRAME  1
#define DDL_DEFAULT_NUM_OF_B_FRAME  DDL_MIN_NUM_OF_B_FRAME

#define DDL_MIN_NUM_REF_FOR_P_FRAME             1
#define DDL_MAX_NUM_REF_FOR_P_FRAME             2

#define DDL_MAX_NUM_IN_INPUTFRAME_POOL          (DDL_MAX_NUM_OF_B_FRAME + 1)

#define MDP_MIN_TILE_HEIGHT			96

enum ddl_mem_area {
	DDL_FW_MEM	= 0x0,
	DDL_MM_MEM	= 0x1,
	DDL_CMD_MEM	= 0x2
};

struct ddl_buf_addr{
	u8  *virtual_base_addr;
	u8  *physical_base_addr;
	u8  *align_physical_addr;
	u8  *align_virtual_addr;
	phys_addr_t alloced_phys_addr;
	struct msm_mapped_buffer *mapped_buffer;
	struct ion_handle *alloc_handle;
	u32 buffer_size;
	enum ddl_mem_area mem_type;
	void *pil_cookie;
};
enum ddl_cmd_state{
	DDL_CMD_INVALID         = 0x0,
	DDL_CMD_DMA_INIT        = 0x1,
	DDL_CMD_CPU_RESET       = 0x2,
	DDL_CMD_CHANNEL_SET     = 0x3,
	DDL_CMD_INIT_CODEC      = 0x4,
	DDL_CMD_HEADER_PARSE    = 0x5,
	DDL_CMD_DECODE_SET_DPB  = 0x6,
	DDL_CMD_DECODE_FRAME    = 0x7,
	DDL_CMD_ENCODE_FRAME    = 0x8,
	DDL_CMD_EOS             = 0x9,
	DDL_CMD_CHANNEL_END     = 0xA,
	DDL_CMD_ENCODE_CONTINUE = 0xB,
	DDL_CMD_32BIT           = 0x7FFFFFFF
};
enum ddl_client_state{
	DDL_CLIENT_INVALID                 = 0x0,
	DDL_CLIENT_OPEN                    = 0x1,
	DDL_CLIENT_WAIT_FOR_CHDONE         = 0x2,
	DDL_CLIENT_WAIT_FOR_INITCODEC      = 0x3,
	DDL_CLIENT_WAIT_FOR_INITCODECDONE  = 0x4,
	DDL_CLIENT_WAIT_FOR_DPB            = 0x5,
	DDL_CLIENT_WAIT_FOR_DPBDONE        = 0x6,
	DDL_CLIENT_WAIT_FOR_FRAME          = 0x7,
	DDL_CLIENT_WAIT_FOR_FRAME_DONE     = 0x8,
	DDL_CLIENT_WAIT_FOR_EOS_DONE       = 0x9,
	DDL_CLIENT_WAIT_FOR_CHEND          = 0xA,
	DDL_CLIENT_FATAL_ERROR             = 0xB,
	DDL_CLIENT_FAVIDC_ERROR            = 0xC,
	DDL_CLIENT_WAIT_FOR_CONTINUE       = 0xD,
	DDL_CLIENT_32BIT                   = 0x7FFFFFFF
};
struct ddl_hw_interface{
	u32 cmd;
	u32 arg1;
	u32 arg2;
	u32 arg3;
	u32 arg4;
};
struct ddl_mask{
	u32  client_mask;
	u32  hw_mask;
};
struct ddl_yuv_buffer_size{
	u32  size_yuv;
	u32  size_y;
	u32  size_c;
};
struct ddl_dec_buffer_size{
	u32  sz_dpb0;
	u32  sz_dpb1;
	u32  sz_mv;
	u32  sz_vert_nb_mv;
	u32  sz_nb_ip;
	u32  sz_luma;
	u32  sz_chroma;
	u32  sz_nb_dcac;
	u32  sz_upnb_mv;
	u32  sz_sub_anchor_mv;
	u32  sz_overlap_xform;
	u32  sz_bit_plane3;
	u32  sz_bit_plane2;
	u32  sz_bit_plane1;
	u32  sz_stx_parser;
	u32  sz_desc;
	u32  sz_cpb;
	u32  sz_context;
	u32  sz_extnuserdata;
};
struct ddl_dec_buffers{
	struct ddl_buf_addr desc;
	struct ddl_buf_addr nb_dcac;
	struct ddl_buf_addr upnb_mv;
	struct ddl_buf_addr sub_anchor_mv;
	struct ddl_buf_addr overlay_xform;
	struct ddl_buf_addr bit_plane3;
	struct ddl_buf_addr bit_plane2;
	struct ddl_buf_addr bit_plane1;
	struct ddl_buf_addr stx_parser;
	struct ddl_buf_addr h264_mv[DDL_MAX_BUFFER_COUNT];
	struct ddl_buf_addr h264_vert_nb_mv;
	struct ddl_buf_addr h264_nb_ip;
	struct ddl_buf_addr context;
	struct ddl_buf_addr extnuserdata;
};
struct ddl_enc_buffer_size{
	u32  sz_cur_y;
	u32  sz_cur_c;
	u32  sz_dpb_y;
	u32  sz_dpb_c;
	u32  sz_strm;
	u32  sz_mv;
	u32  sz_col_zero;
	u32  sz_md;
	u32  sz_pred;
	u32  sz_nbor_info;
	u32  sz_acdc_coef;
	u32  sz_mb_info;
	u32  sz_context;
};
struct ddl_enc_buffers{
	struct ddl_buf_addr dpb_y[4];
	struct ddl_buf_addr dpb_c[4];
	struct ddl_buf_addr mv;
	struct ddl_buf_addr col_zero;
	struct ddl_buf_addr md;
	struct ddl_buf_addr pred;
	struct ddl_buf_addr nbor_info;
	struct ddl_buf_addr acdc_coef;
	struct ddl_buf_addr mb_info;
	struct ddl_buf_addr context;
	u32  dpb_count;
	u32  sz_dpb_y;
	u32  sz_dpb_c;
};
struct ddl_codec_data_hdr{
	u32  decoding;
};
struct ddl_batch_frame_data {
	struct ddl_buf_addr slice_batch_in;
	struct ddl_buf_addr slice_batch_out;
	struct ddl_frame_data_tag input_frame;
	struct ddl_frame_data_tag output_frame
			[DDL_MAX_NUM_BFRS_FOR_SLICE_BATCH];
	u32 num_output_frames;
	u32 out_frm_next_frmindex;
};
struct ddl_mp2_datadumpenabletype {
	u32 userdatadump_enable;
	u32 pictempscalable_extdump_enable;
	u32 picspat_extdump_enable;
	u32 picdisp_extdump_enable;
	u32 copyright_extdump_enable;
	u32 quantmatrix_extdump_enable;
	u32 seqscalable_extdump_enable;
	u32 seqdisp_extdump_enable;
	u32 seq_extdump_enable;
};
struct ddl_encoder_data{
	struct ddl_codec_data_hdr   hdr;
	struct vcd_property_codec   codec;
	struct vcd_property_frame_size  frame_size;
	struct vcd_property_frame_rate  frame_rate;
	struct vcd_property_target_bitrate  target_bit_rate;
	struct vcd_property_profile  profile;
	struct vcd_property_level  level;
	struct vcd_property_rate_control  rc;
	struct vcd_property_multi_slice  multi_slice;
	struct ddl_buf_addr  meta_data_input;
	struct vcd_property_short_header  short_header;
	struct vcd_property_vop_timing  vop_timing;
	struct vcd_property_db_config  db_control;
	struct vcd_property_entropy_control  entropy_control;
	struct vcd_property_i_period  i_period;
	struct vcd_property_session_qp  session_qp;
	struct vcd_property_qp_range  qp_range;
	struct vcd_property_rc_level  rc_level;
	struct vcd_property_frame_level_rc_params  frame_level_rc;
	struct vcd_property_adaptive_rc_params  adaptive_rc;
	struct vcd_property_intra_refresh_mb_number  intra_refresh;
	struct vcd_property_buffer_format  buf_format;
	struct vcd_property_buffer_format  recon_buf_format;
	struct vcd_property_sps_pps_for_idr_enable sps_pps;
	struct ddl_buf_addr  seq_header;
	struct vcd_buffer_requirement  input_buf_req;
	struct vcd_buffer_requirement  output_buf_req;
	struct vcd_buffer_requirement  client_input_buf_req;
	struct vcd_buffer_requirement  client_output_buf_req;
	struct ddl_enc_buffers  hw_bufs;
	struct ddl_yuv_buffer_size  input_buf_size;
	struct vidc_1080p_enc_frame_info enc_frame_info;
	u32  meta_data_enable_flag;
	u32  suffix;
	u32  meta_data_offset;
	u32  hdr_ext_control;
	u32  r_cframe_skip;
	u32  vb_vbuffer_size;
	u32  dynamic_prop_change;
	u32  dynmic_prop_change_req;
	u32  seq_header_length;
	u32  intra_frame_insertion;
	u32  mb_info_enable;
	u32  ext_enc_control_val;
	u32  num_references_for_p_frame;
	u32  closed_gop;
	u32  num_slices_comp;
	struct vcd_property_slice_delivery_info slice_delivery_info;
	struct ddl_batch_frame_data batch_frame;
	u32 avc_delimiter_enable;
	u32 vui_timinginfo_enable;
};
struct ddl_decoder_data {
	struct ddl_codec_data_hdr  hdr;
	struct vcd_property_codec  codec;
	struct vcd_property_buffer_format  buf_format;
	struct vcd_property_frame_size  frame_size;
	struct vcd_property_frame_size  client_frame_size;
	struct vcd_property_profile  profile;
	struct vcd_property_level  level;
	struct ddl_buf_addr  meta_data_input;
	struct vcd_property_post_filter  post_filter;
	struct vcd_sequence_hdr  decode_config;
	struct ddl_property_dec_pic_buffers  dp_buf;
	struct ddl_mask  dpb_mask;
	struct vcd_buffer_requirement  actual_input_buf_req;
	struct vcd_buffer_requirement  min_input_buf_req;
	struct vcd_buffer_requirement  client_input_buf_req;
	struct vcd_buffer_requirement  actual_output_buf_req;
	struct vcd_buffer_requirement  min_output_buf_req;
	struct vcd_buffer_requirement  client_output_buf_req;
	struct ddl_dec_buffers  hw_bufs;
	struct ddl_yuv_buffer_size  dpb_buf_size;
	struct vidc_1080p_dec_disp_info dec_disp_info;
	u32  progressive_only;
	u32  output_order;
	u32  meta_data_enable_flag;
	u32  suffix;
	u32  meta_data_offset;
	u32  header_in_start;
	u32  min_dpb_num;
	u32  y_cb_cr_size;
	u32  yuv_size;
	u32  dynamic_prop_change;
	u32  dynmic_prop_change_req;
	u32  flush_pending;
	u32  meta_data_exists;
	u32  idr_only_decoding;
	u32  field_needed_for_prev_ip;
	u32  prev_ip_frm_tag;
	u32  cont_mode;
	u32  reconfig_detected;
	u32  dmx_disable;
	int avg_dec_time;
	int dec_time_sum;
	struct ddl_mp2_datadumpenabletype mp2_datadump_enable;
	u32 mp2_datadump_status;
	u32 extn_user_data_enable;
};
union ddl_codec_data{
	struct ddl_codec_data_hdr  hdr;
	struct ddl_decoder_data   decoder;
	struct ddl_encoder_data   encoder;
};
struct ddl_context{
	u8 *core_virtual_base_addr;
	void *client_data;
	u32 device_state;
	u32 ddl_busy;
	u32 cmd_err_status;
	u32 disp_pic_err_status;
	u32 pix_cache_enable;
	u32 fw_version;
	u32 fw_memory_size;
	u32 cmd_seq_num;
	u32 response_cmd_ch_id;
	enum ddl_cmd_state cmd_state;
	struct ddl_client_context *current_ddl[2];
	struct ddl_buf_addr metadata_shared_input;
	struct ddl_client_context *ddl_clients[VCD_MAX_NO_CLIENT];
	struct ddl_buf_addr dram_base_a;
	struct ddl_buf_addr dram_base_b;
	struct ddl_hw_interface ddl_hw_response;
	struct ion_client *video_ion_client;
	void (*ddl_callback) (u32 event, u32 status, void *payload,
		size_t sz, u32 *ddl_handle, void *const client_data);
	void (*interrupt_clr) (void);
	void (*vidc_decode_seq_start[2])
		(struct vidc_1080p_dec_seq_start_param *param);
	void (*vidc_set_dec_resolution[2])
		(u32 width, u32 height);
	void(*vidc_decode_init_buffers[2])
		(struct vidc_1080p_dec_init_buffers_param *param);
	void(*vidc_decode_frame_start[2])
		(struct vidc_1080p_dec_frame_start_param *param);
	void(*vidc_encode_seq_start[2])
		(struct vidc_1080p_enc_seq_start_param *param);
	void(*vidc_encode_frame_start[2])
		(struct vidc_1080p_enc_frame_start_param *param);
	void(*vidc_encode_slice_batch_start[2])
		(struct vidc_1080p_enc_frame_start_param *param);
	u32 frame_channel_depth;
};
struct ddl_client_context{
	struct ddl_context  *ddl_context;
	enum ddl_client_state  client_state;
	struct ddl_frame_data_tag  first_output_frame;
	struct ddl_frame_data_tag
		extra_output_frame[DDL_MAX_NUM_OF_B_FRAME];
	struct ddl_frame_data_tag  input_frame;
	struct ddl_frame_data_tag  output_frame;
	struct ddl_frame_data_tag
		input_frame_pool[DDL_MAX_NUM_IN_INPUTFRAME_POOL];
	union ddl_codec_data  codec_data;
	enum ddl_cmd_state  cmd_state;
	struct ddl_buf_addr  shared_mem[2];
	void *client_data;
	u32  decoding;
	u32  channel_id;
	u32  command_channel;
	u32  b_count;
	s32  extra_output_buf_count;
	u32  instance_id;
};

struct ddl_context *ddl_get_context(void);
void ddl_vidc_core_init(struct ddl_context *);
void ddl_vidc_core_term(struct ddl_context *);
void ddl_vidc_channel_set(struct ddl_client_context *);
void ddl_vidc_channel_end(struct ddl_client_context *);
void ddl_vidc_encode_init_codec(struct ddl_client_context *);
void ddl_vidc_decode_init_codec(struct ddl_client_context *);
void ddl_vidc_encode_frame_continue(struct ddl_client_context *);
void ddl_vidc_encode_frame_run(struct ddl_client_context *);
void ddl_vidc_encode_slice_batch_run(struct ddl_client_context *);
void ddl_vidc_decode_frame_run(struct ddl_client_context *);
void ddl_vidc_decode_eos_run(struct ddl_client_context *ddl);
void ddl_vidc_encode_eos_run(struct ddl_client_context *ddl);
void ddl_release_context_buffers(struct ddl_context *);
void ddl_release_client_internal_buffers(struct ddl_client_context *ddl);
u32  ddl_vidc_decode_set_buffers(struct ddl_client_context *);
u32  ddl_decoder_dpb_transact(struct ddl_decoder_data *decoder,
	struct ddl_frame_data_tag *in_out_frame, u32 operation);
u32  ddl_decoder_dpb_init(struct ddl_client_context *ddl);
u32  ddl_client_transact(u32 , struct ddl_client_context **);
u32  ddl_set_default_decoder_buffer_req(struct ddl_decoder_data *decoder,
	u32 estimate);
void ddl_set_default_encoder_buffer_req(struct ddl_encoder_data
	*encoder);
void ddl_set_default_dec_property(struct ddl_client_context *);
u32  ddl_encoder_ready_to_start(struct ddl_client_context *);
u32  ddl_decoder_ready_to_start(struct ddl_client_context *,
	struct vcd_sequence_hdr *);
u32  ddl_get_yuv_buffer_size(struct vcd_property_frame_size *frame_size,
	struct vcd_property_buffer_format *buf_format, u32 interlace,
	u32 decoding, u32 *pn_c_offset);
void ddl_calculate_stride(struct vcd_property_frame_size *frame_size,
	u32 interlace);
u32  ddl_codec_type_transact(struct ddl_client_context *ddl,
	u32 remove, enum vcd_codec requested_codec);
void ddl_vidc_encode_dynamic_property(struct ddl_client_context *ddl,
	u32 enable);
void ddl_vidc_decode_dynamic_property(struct ddl_client_context *ddl,
	u32 enable);
void ddl_set_initial_default_values(struct ddl_client_context *ddl);

u32  ddl_take_command_channel(struct ddl_context *ddl_context,
	struct ddl_client_context *ddl, void *client_data);
void ddl_release_command_channel(struct ddl_context  *ddl_context,
	u32 command_channel);
struct ddl_client_context *ddl_get_current_ddl_client_for_channel_id(
	struct ddl_context *ddl_context, u32 channel_id);
struct ddl_client_context *ddl_get_current_ddl_client_for_command(
	struct ddl_context *ddl_context,
	enum ddl_cmd_state cmd_state);

u32  ddl_get_yuv_buf_size(u32 width, u32 height, u32 format);
void ddl_free_dec_hw_buffers(struct ddl_client_context *ddl);
void ddl_free_enc_hw_buffers(struct ddl_client_context *ddl);
void ddl_calc_dec_hw_buffers_size(enum vcd_codec codec, u32 width,
	u32 height, u32 h264_dpb,
	struct ddl_dec_buffer_size *buf_size);
u32  ddl_allocate_dec_hw_buffers(struct ddl_client_context *ddl);
u32  ddl_calc_enc_hw_buffers_size(enum vcd_codec codec, u32 width,
	u32 height, enum vcd_yuv_buffer_format  input_format,
	struct ddl_client_context *ddl,
	struct ddl_enc_buffer_size *buf_size);
u32  ddl_allocate_enc_hw_buffers(struct ddl_client_context *ddl);

u32  ddl_handle_core_errors(struct ddl_context *ddl_context);
void ddl_client_fatal_cb(struct ddl_client_context *ddl);
void ddl_hw_fatal_cb(struct ddl_client_context *ddl);

void *ddl_pmem_alloc(struct ddl_buf_addr *addr, size_t sz, u32 alignment);
void ddl_pmem_free(struct ddl_buf_addr *addr);

u32 ddl_get_input_frame_from_pool(struct ddl_client_context *ddl,
	u8 *input_buffer_address);
u32 ddl_get_stream_buf_from_batch_pool(struct ddl_client_context *ddl,
	struct ddl_frame_data_tag *stream_buffer);
u32 ddl_insert_input_frame_to_pool(struct ddl_client_context *ddl,
	struct ddl_frame_data_tag *ddl_input_frame);
void ddl_decoder_chroma_dpb_change(struct ddl_client_context *ddl);
u32  ddl_check_reconfig(struct ddl_client_context *ddl);
void ddl_handle_reconfig(u32 res_change, struct ddl_client_context *ddl);
void ddl_fill_dec_desc_buffer(struct ddl_client_context *ddl);
void ddl_set_vidc_timeout(struct ddl_client_context *ddl);


#ifdef DDL_BUF_LOG
void ddl_list_buffers(struct ddl_client_context *ddl);
#endif
#ifdef DDL_MSG_LOG
s8 *ddl_get_state_string(enum ddl_client_state client_state);
#endif
extern unsigned char *vidc_video_codec_fw;
extern u32 vidc_video_codec_fw_size;

u32 ddl_fw_init(struct ddl_buf_addr *dram_base);
void ddl_get_fw_info(const unsigned char **fw_array_addr,
	unsigned int *fw_size);
void ddl_fw_release(struct ddl_buf_addr *);
int ddl_vidc_decode_get_avg_time(struct ddl_client_context *ddl);
void ddl_vidc_decode_reset_avg_time(struct ddl_client_context *ddl);
void ddl_calc_core_proc_time(const char *func_name, u32 index,
		struct ddl_client_context *ddl);
#endif
