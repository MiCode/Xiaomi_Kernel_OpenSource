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

#ifndef _VCD_DDL_SHARED_MEM_H_
#define _VCD_DDL_SHARED_MEM_H_

#include "vcd_ddl.h"

#define VIDC_SM_PROFILE_MPEG4_SIMPLE      (0)
#define VIDC_SM_PROFILE_MPEG4_ADV_SIMPLE  (1)

#define VIDC_SM_PROFILE_H264_BASELINE     (0)
#define VIDC_SM_PROFILE_H264_MAIN         (1)
#define VIDC_SM_PROFILE_H264_HIGH         (2)

#define VIDC_SM_PROFILE_H263_BASELINE     (0)

#define VIDC_SM_PROFILE_VC1_SIMPLE        (0)
#define VIDC_SM_PROFILE_VC1_MAIN          (1)
#define VIDC_SM_PROFILE_VC1_ADVANCED      (2)

#define VIDC_SM_PROFILE_MPEG2_MAIN        (4)
#define VIDC_SM_PROFILE_MPEG2_SIMPLE      (5)

#define VIDC_SM_LEVEL_MPEG2_LOW        (10)
#define VIDC_SM_LEVEL_MPEG2_MAIN        (8)
#define VIDC_SM_LEVEL_MPEG2_HIGH_1440   (6)
#define VIDC_SM_LEVEL_MPEG2_HIGH        (4)

#define VIDC_SM_LEVEL_VC1_LOW     (0)
#define VIDC_SM_LEVEL_VC1_MEDIUM  (2)
#define VIDC_SM_LEVEL_VC1_HIGH    (4)

#define VIDC_SM_LEVEL_VC1_ADV_0  (0)
#define VIDC_SM_LEVEL_VC1_ADV_1  (1)
#define VIDC_SM_LEVEL_VC1_ADV_2  (2)
#define VIDC_SM_LEVEL_VC1_ADV_3  (3)
#define VIDC_SM_LEVEL_VC1_ADV_4  (4)

#define VIDC_SM_RECOVERY_POINT_SEI  (1)
enum VIDC_SM_frame_skip {
	VIDC_SM_FRAME_SKIP_DISABLE      = 0,
	VIDC_SM_FRAME_SKIP_ENABLE_LEVEL = 1,
	VIDC_SM_FRAME_SKIP_ENABLE_VBV   = 2
};
enum VIDC_SM_ref_picture {
	VIDC_SM_REF_PICT_FRAME_OR_TOP_FIELD   = 0,
	VIDC_SM_REF_PICT_BOTTOM_FIELD         = 1
};

struct ddl_profile_info_type {
	u32 bit_depth_chroma_minus8;
	u32 bit_depth_luma_minus8;
	u32 pic_level;
	u32 chroma_format_idc;
	u32 pic_profile;
};

enum vidc_sm_mpeg4_profileinfo {
	VIDC_SM_PROFILE_INFO_DISABLE  = 0,
	VIDC_SM_PROFILE_INFO_SP       = 1,
	VIDC_SM_PROFILE_INFO_ASP      = 2,
	VIDC_SM_PROFILE_INFO_MAX      = 0x7fffffff
};

enum vidc_sm_num_stuff_bytes_consume_info {
	VIDC_SM_NUM_STUFF_BYTES_CONSUME_ALL  = 0x0,
	VIDC_SM_NUM_STUFF_BYTES_CONSUME_NONE = 0xffffffff
};

void vidc_sm_get_extended_decode_status(struct ddl_buf_addr *shared_mem,
	u32 *more_field_needed,
	u32 *resl_change);
void vidc_sm_set_frame_tag(struct ddl_buf_addr *shared_mem,
	u32 frame_tag);
void vidc_sm_get_frame_tags(struct ddl_buf_addr *shared_mem,
	u32 *pn_frame_tag_top, u32 *pn_frame_tag_bottom);
void vidc_sm_get_picture_times(struct ddl_buf_addr *shared_mem,
	u32 *pn_time_top, u32 *pn_time_bottom);
void vidc_sm_set_start_byte_number(struct ddl_buf_addr *shared_mem,
	u32 byte_num);
void vidc_sm_get_crop_info(struct ddl_buf_addr *shared_mem, u32 *pn_left,
	u32 *pn_right, u32 *pn_top, u32 *pn_bottom);
void vidc_sm_get_displayed_picture_frame(struct ddl_buf_addr
	*shared_mem, u32 *n_disp_picture_frame);
void vidc_sm_get_available_luma_dpb_address(
	struct ddl_buf_addr *shared_mem, u32 *pn_free_luma_dpb_address);
void vidc_sm_get_available_luma_dpb_dec_order_address(
	struct ddl_buf_addr *shared_mem, u32 *pn_free_luma_dpb_address);
void vidc_sm_get_dec_order_resl(
	struct ddl_buf_addr *shared_mem, u32 *width, u32 *height);
void vidc_sm_get_dec_order_crop_info(
	struct ddl_buf_addr *shared_mem, u32 *left,
	u32 *right, u32 *top, u32 *bottom);
void vidc_sm_set_extended_encoder_control(
	struct ddl_buf_addr *shared_mem, u32 hec_enable,
	enum VIDC_SM_frame_skip  frame_skip_mode, u32 seq_hdr_in_band,
	u32 vbv_buffer_size, u32 cpcfc_enable, u32 sps_pps_control,
	u32 closed_gop_enable);
void vidc_sm_set_encoder_param_change(struct ddl_buf_addr *shared_mem,
	u32 bit_rate_chg, u32 frame_rate_chg, u32 i_period_chg);
void vidc_sm_set_encoder_vop_time(struct ddl_buf_addr *shared_mem,
	u32 vop_time_enable, u32 time_resolution, u32 frame_delta);
void vidc_sm_set_encoder_hec_period(struct ddl_buf_addr *shared_mem,
	u32 hec_period);
void vidc_sm_get_h264_encoder_reference_list0(
	struct ddl_buf_addr *shared_mem,
	enum VIDC_SM_ref_picture *pe_luma_picture0,
	u32 *pn_luma_picture_index0,
	enum VIDC_SM_ref_picture *pe_luma_picture1,
	u32 *pn_luma_picture_index1,
	enum VIDC_SM_ref_picture *pe_chroma_picture0,
	u32 *pn_chroma_picture_index0,
	enum VIDC_SM_ref_picture *pe_chroma_picture1,
	u32 *pn_chroma_picture_index1);

void vidc_sm_get_h264_encoder_reference_list1(
	struct ddl_buf_addr *shared_mem,
	enum VIDC_SM_ref_picture *pe_luma_picture,
	u32 *pn_luma_picture_index,
	enum VIDC_SM_ref_picture *pe_chroma_picture,
	u32 *pn_chroma_picture_index);
void vidc_sm_set_allocated_dpb_size(struct ddl_buf_addr *shared_mem,
	u32 y_size, u32 c_size);
void vidc_sm_set_allocated_h264_mv_size(struct ddl_buf_addr *shared_mem,
	u32 mv_size);
void vidc_sm_get_min_yc_dpb_sizes(struct ddl_buf_addr *shared_mem,
	u32 *pn_min_luma_dpb_size, u32 *pn_min_chroma_dpb_size);
void vidc_sm_set_metadata_enable(struct ddl_buf_addr *shared_mem,
	u32 extradata_enable, u32 qp_enable, u32 concealed_mb_enable,
	u32 vc1Param_enable, u32 sei_nal_enable, u32 vui_enable,
	u32 enc_slice_size_enable);
void vidc_sm_get_metadata_status(struct ddl_buf_addr *shared_mem,
	u32 *pb_metadata_present);
void vidc_sm_get_metadata_display_index(struct ddl_buf_addr *shared_mem,
	u32 *pn_dixplay_index);
void vidc_sm_set_metadata_start_address(struct ddl_buf_addr *shared_mem,
	u32 address);
void vidc_sm_set_extradata_presence(struct ddl_buf_addr *shared_mem,
	u32 extradata_present);
void vidc_sm_set_extradata_addr(struct ddl_buf_addr *shared_mem,
	u32 extradata_addr);
void vidc_sm_set_pand_b_frame_qp(struct ddl_buf_addr *shared_mem,
	u32 b_frame_qp, u32 p_frame_qp);
void vidc_sm_get_profile_info(struct ddl_buf_addr *shared_mem,
	struct ddl_profile_info_type *ddl_profile_info);
void vidc_sm_set_encoder_new_bit_rate(struct ddl_buf_addr *shared_mem,
	u32 new_bit_rate);
void vidc_sm_set_encoder_new_frame_rate(struct ddl_buf_addr *shared_mem,
	u32 new_frame_rate);
void vidc_sm_set_encoder_new_i_period(struct ddl_buf_addr *shared_mem,
	u32 new_i_period);
void vidc_sm_set_encoder_init_rc_value(struct ddl_buf_addr *shared_mem,
	u32 new_rc_value);
void vidc_sm_set_idr_decode_only(struct ddl_buf_addr *shared_mem,
	u32 enable);
void vidc_sm_set_concealment_color(struct ddl_buf_addr *shared_mem,
	u32 conceal_ycolor, u32 conceal_ccolor);
void vidc_sm_set_chroma_addr_change(struct ddl_buf_addr *shared_mem,
	u32 addr_change);
void vidc_sm_set_mpeg4_profile_override(struct ddl_buf_addr *shared_mem,
	enum vidc_sm_mpeg4_profileinfo profile_info);
void vidc_sm_set_decoder_sei_enable(struct ddl_buf_addr *shared_mem,
	u32 sei_enable);
void vidc_sm_get_decoder_sei_enable(struct ddl_buf_addr *shared_mem,
	u32 *sei_enable);
void vidc_sm_set_error_concealment_config(struct ddl_buf_addr *shared_mem,
	u32 inter_slice, u32 intra_slice, u32 conceal_config_enable);
void vidc_sm_set_decoder_stuff_bytes_consumption(
	struct ddl_buf_addr *shared_mem,
	enum vidc_sm_num_stuff_bytes_consume_info consume_info);
void vidc_sm_get_aspect_ratio_info(struct ddl_buf_addr *shared_mem,
	struct vcd_aspect_ratio *aspect_ratio_info);
void vidc_sm_set_encoder_slice_batch_int_ctrl(struct ddl_buf_addr *shared_mem,
	u32 slice_batch_int_enable);
void vidc_sm_get_num_slices_comp(struct ddl_buf_addr *shared_mem,
	u32 *num_slices_comp);
void vidc_sm_set_encoder_batch_config(struct ddl_buf_addr *shared_mem,
				u32 num_slices,
				u32 input_addr, u32 output_addr,
				u32 output_buffer_size);
void vidc_sm_get_encoder_batch_output_size(struct ddl_buf_addr *shared_mem,
	u32 *output_buffer_size);
#endif
