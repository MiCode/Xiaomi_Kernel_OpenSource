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

#include "vcd_ddl_shared_mem.h"

#define VIDC_SM_EXTENDED_DECODE_STATUS_ADDR    0x0000
#define VIDC_SM_EXT_DEC_STATUS_RESOLUTION_CHANGE_BMSK 0x1
#define VIDC_SM_EXT_DEC_STATUS_RESOLUTION_CHANGE_SHFT 0x0
#define VIDC_SM_EXT_DEC_STATUS_MORE_FIELD_NEEDED_BMSK 0x4
#define VIDC_SM_EXT_DEC_STATUS_MORE_FIELD_NEEDED_SHFT 0x2

#define VIDC_SM_SET_FRAME_TAG_ADDR             0x0004
#define VIDC_SM_GET_FRAME_TAG_TOP_ADDR         0x0008
#define VIDC_SM_GET_FRAME_TAG_BOTTOM_ADDR      0x000c
#define VIDC_SM_PIC_TIME_TOP_ADDR              0x0010
#define VIDC_SM_PIC_TIME_BOTTOM_ADDR           0x0014
#define VIDC_SM_START_BYTE_NUM_ADDR            0x0018

#define VIDC_SM_CROP_INFO1_ADDR                0x0020
#define VIDC_SM_CROP_INFO1_RIGHT_OFFSET_BMSK   0xffff0000
#define VIDC_SM_CROP_INFO1_RIGHT_OFFSET_SHFT   16
#define VIDC_SM_CROP_INFO1_LEFT_OFFSET_BMSK    0x0000ffff
#define VIDC_SM_CROP_INFO1_LEFT_OFFSET_SHFT    0

#define VIDC_SM_CROP_INFO2_ADDR                0x0024
#define VIDC_SM_CROP_INFO2_BOTTOM_OFFSET_BMSK  0xffff0000
#define VIDC_SM_CROP_INFO2_BOTTOM_OFFSET_SHFT  16
#define VIDC_SM_CROP_INFO2_TOP_OFFSET_BMSK     0x0000ffff
#define VIDC_SM_CROP_INFO2_TOP_OFFSET_SHFT     0

#define VIDC_SM_DISP_PIC_PROFILE_ADDR                       0x007c
#define VIDC_SM_DISP_PIC_PROFILE_DISP_PIC_LEVEL_BMASK       0x0000ff00
#define VIDC_SM_DISP_PIC_PROFILE_DISP_PIC_LEVEL_SHFT        8
#define VIDC_SM_DISP_PIC_PROFILE_DISP_PIC_PROFILE_BMASK     0x0000001f
#define VIDC_SM_DISP_PIC_PROFILE_DISP_PIC_PROFILE_SHFT      0

#define VIDC_SM_DISP_PIC_FRAME_TYPE_ADDR                    0x00c0
#define VIDC_SM_DISP_PIC_FRAME_TYPE_BMSK                    0x00000003
#define VIDC_SM_DISP_PIC_FRAME_TYPE_SHFT                    0

#define VIDC_SM_FREE_LUMA_DPB_ADDR                          0x00c4
#define VIDC_SM_FREE_LUMA_DPB_BMSK                          0xffffffff
#define VIDC_SM_FREE_LUMA_DPB_SHFT                          0

#define VIDC_SM_FREE_LUMA_DPB_DEC_ORDER_ADDR                0x00fc
#define VIDC_SM_FREE_LUMA_DPB_DEC_ORDER_BMSK                0xffffffff
#define VIDC_SM_FREE_LUMA_DPB_DEC_ORDER_SHFT                0

#define VIDC_SM_DEC_ORDER_WIDTH_ADDR                        0x00e8
#define VIDC_SM_DEC_ORDER_WIDTH_BMSK                        0xffffffff
#define VIDC_SM_DEC_ORDER_WIDTH_SHFT                        0

#define VIDC_SM_DEC_ORDER_HEIGHT_ADDR                       0x00ec
#define VIDC_SM_DEC_ORDER_HEIGHT_BMSK                       0xffffffff
#define VIDC_SM_DEC_ORDER_HEIGHT_SHFT                       0

#define VIDC_SM_DEC_CROP_INFO1_ADDR                         0x00f4
#define VIDC_SM_DEC_CROP_INFO1_RIGHT_OFFSET_BMSK            0xffff0000
#define VIDC_SM_DEC_CROP_INFO1_RIGHT_OFFSET_SHFT            16
#define VIDC_SM_DEC_CROP_INFO1_LEFT_OFFSET_BMSK             0x0000ffff
#define VIDC_SM_DEC_CROP_INFO1_LEFT_OFFSET_SHFT             0

#define VIDC_SM_DEC_CROP_INFO2_ADDR                         0x00f8
#define VIDC_SM_DEC_CROP_INFO2_BOTTOM_OFFSET_BMSK           0xffff0000
#define VIDC_SM_DEC_CROP_INFO2_BOTTOM_OFFSET_SHFT           16
#define VIDC_SM_DEC_CROP_INFO2_TOP_OFFSET_BMSK              0x0000ffff
#define VIDC_SM_DEC_CROP_INFO2_TOP_OFFSET_SHFT              0

#define VIDC_SM_IDR_DECODING_ONLY_ADDR                      0x0108
#define VIDC_SM_IDR_DECODING_ONLY_BMSK                      0x00000001
#define VIDC_SM_IDR_DECODING_ONLY_SHIFT                     0

#define VIDC_SM_ENC_EXT_CTRL_ADDR                    0x0028
#define VIDC_SM_ENC_EXT_CTRL_VBV_BUFFER_SIZE_BMSK    0xffff0000
#define VIDC_SM_ENC_EXT_CTRL_VBV_BUFFER_SIZE_SHFT    16
#define VIDC_SM_ENC_EXT_CTRL_H263_CPCFC_ENABLE_BMSK  0x80
#define VIDC_SM_ENC_EXT_CTRL_H263_CPCFC_ENABLE_SHFT  7
#define VIDC_SM_ENC_EXT_CTRL_SPS_PPS_CONTROL_BMSK    0X100
#define VIDC_SM_ENC_EXT_CTRL_SPS_PPS_CONTROL_SHFT    8
#define VIDC_SM_ENC_EXT_CTRL_SEQ_HDR_CTRL_BMSK       0x8
#define VIDC_SM_ENC_EXT_CTRL_SEQ_HDR_CTRL_SHFT       3
#define VIDC_SM_ENC_EXT_CTRL_FRAME_SKIP_ENABLE_BMSK  0x6
#define VIDC_SM_ENC_EXT_CTRL_FRAME_SKIP_ENABLE_SHFT  1
#define VIDC_SM_ENC_EXT_CTRL_HEC_ENABLE_BMSK         0x1
#define VIDC_SM_ENC_EXT_CTRL_HEC_ENABLE_SHFT         0

#define VIDC_SM_ENC_PARAM_CHANGE_ADDR                0x002c
#define VIDC_SM_ENC_PARAM_CHANGE_RC_BIT_RATE_BMSK    0x4
#define VIDC_SM_ENC_PARAM_CHANGE_RC_BIT_RATE_SHFT    2
#define VIDC_SM_ENC_PARAM_CHANGE_RC_FRAME_RATE_BMSK  0x2
#define VIDC_SM_ENC_PARAM_CHANGE_RC_FRAME_RATE_SHFT  1
#define VIDC_SM_ENC_PARAM_CHANGE_I_PERIOD_BMSK       0x1
#define VIDC_SM_ENC_PARAM_CHANGE_I_PERIOD_SHFT       0

#define VIDC_SM_ENC_VOP_TIMING_ADDR                  0x0030
#define VIDC_SM_ENC_VOP_TIMING_ENABLE_BMSK           0x80000000
#define VIDC_SM_ENC_VOP_TIMING_ENABLE_SHFT           31
#define VIDC_SM_ENC_VOP_TIMING_TIME_RESOLUTION_BMSK  0x7fff0000
#define VIDC_SM_ENC_VOP_TIMING_TIME_RESOLUTION_SHFT  16
#define VIDC_SM_ENC_VOP_TIMING_FRAME_DELTA_BMSK      0x0000ffff
#define VIDC_SM_ENC_VOP_TIMING_FRAME_DELTA_SHFT      0

#define VIDC_SM_ENC_HEC_PERIOD_ADDR                  0x0034

#define VIDC_SM_H264_REF_L0_ADDR                    0x005c
#define VIDC_SM_H264_REF_L0_CHRO_BTM_FLG_1_BMSK     0x80000000
#define VIDC_SM_H264_REF_L0_CHRO_BTM_FLG_1_SHFT     31
#define VIDC_SM_H264_REF_L0_CHRO_REF_1_BMSK         0x7f000000
#define VIDC_SM_H264_REF_L0_CHRO_REF_1_SHFT         24
#define VIDC_SM_H264_REF_L0_CHRO_BTM_FLG_0_BMSK     0x00800000
#define VIDC_SM_H264_REF_L0_CHRO_BTM_FLG_0_SHFT     23
#define VIDC_SM_H264_REF_L0_CHRO_REF_0_BMSK         0x007f0000
#define VIDC_SM_H264_REF_L0_CHRO_REF_0_SHFT         16
#define VIDC_SM_H264_REF_L0_LUMA_BTM_FLG_1_BMSK     0x00008000
#define VIDC_SM_H264_REF_L0_LUMA_BTM_FLG_1_SHFT     15
#define VIDC_SM_H264_REF_L0_LUMA_REF_1_BMSK         0x00007f00
#define VIDC_SM_H264_REF_L0_LUMA_REF_1_SHFT         8
#define VIDC_SM_H264_REF_L0_LUMA_BTM_FLG_0_BMSK     0x00000080
#define VIDC_SM_H264_REF_L0_LUMA_BTM_FLG_0_SHFT     7
#define VIDC_SM_H264_REF_L0_LUMA_REF_0_BMSK         0x0000007f
#define VIDC_SM_H264_REF_L0_LUMA_REF_0_SHFT         0

#define VIDC_SM_H264_REF_L1_ADDR                  0x0060
#define VIDC_SM_H264_REF_L1_CHRO_BTM_FLG_0_BMSK   0x00800000
#define VIDC_SM_H264_REF_L1_CHRO_BTM_FLG_0_SHFT   23
#define VIDC_SM_H264_REF_L1_CHRO_REF_0_BMSK       0x007f0000
#define VIDC_SM_H264_REF_L1_CHRO_REF_0_SHFT       16
#define VIDC_SM_H264_REF_L1_LUMA_BTM_FLG_0_BMSK   0x00000080
#define VIDC_SM_H264_REF_L1_LUMA_BTM_FLG_0_SHFT   7
#define VIDC_SM_H264_REF_L1_LUMA_REF_0_BMSK       0x0000007f
#define VIDC_SM_H264_REF_L1_LUMA_REF_0_SHFT       0

#define VIDC_SM_P_B_FRAME_QP_ADDR               0x0070
#define VIDC_SM_P_B_FRAME_QP_B_FRAME_QP_BMASK   0x00000fc0
#define VIDC_SM_P_B_FRAME_QP_B_FRAME_QP_SHFT    6
#define VIDC_SM_P_B_FRAME_QP_P_FRAME_QP_BMASK   0x0000003f
#define VIDC_SM_P_B_FRAME_QP_P_FRAME_QP_SHFT    0

#define VIDC_SM_NEW_RC_BIT_RATE_ADDR           0x0090
#define VIDC_SM_NEW_RC_BIT_RATE_VALUE_BMASK    0xffffffff
#define VIDC_SM_NEW_RC_BIT_RATE_VALUE_SHFT     0
#define VIDC_SM_NEW_RC_FRAME_RATE_ADDR         0x0094
#define VIDC_SM_NEW_RC_FRAME_RATE_VALUE_BMASK  0xffffffff
#define VIDC_SM_NEW_RC_FRAME_RATE_VALUE_SHFT   0
#define VIDC_SM_NEW_I_PERIOD_ADDR              0x0098
#define VIDC_SM_NEW_I_PERIOD_VALUE_BMASK       0xffffffff
#define VIDC_SM_NEW_I_PERIOD_VALUE_SHFT        0


#define VIDC_SM_ALLOCATED_LUMA_DPB_SIZE_ADDR               0x0064
#define VIDC_SM_ALLOCATED_CHROMA_DPB_SIZE_ADDR             0x0068
#define VIDC_SM_ALLOCATED_MV_SIZE_ADDR                     0x006c
#define VIDC_SM_FLUSH_CMD_TYPE_ADDR                        0x0080
#define VIDC_SM_FLUSH_CMD_INBUF1_ADDR                      0x0084
#define VIDC_SM_FLUSH_CMD_INBUF2_ADDR                      0x0088
#define VIDC_SM_FLUSH_CMD_OUTBUF_ADDR                      0x008c
#define VIDC_SM_MIN_LUMA_DPB_SIZE_ADDR                     0x00b0
#define VIDC_SM_MIN_CHROMA_DPB_SIZE_ADDR                   0x00bc


#define VIDC_SM_METADATA_ENABLE_ADDR                 0x0038
#define VIDC_SM_METADATA_ENABLE_EXTRADATA_BMSK       0x40
#define VIDC_SM_METADATA_ENABLE_EXTRADATA_SHFT       6
#define VIDC_SM_METADATA_ENABLE_ENC_SLICE_SIZE_BMSK  0x20
#define VIDC_SM_METADATA_ENABLE_ENC_SLICE_SIZE_SHFT  5
#define VIDC_SM_METADATA_ENABLE_VUI_BMSK             0x10
#define VIDC_SM_METADATA_ENABLE_VUI_SHFT             4
#define VIDC_SM_METADATA_ENABLE_SEI_VIDC_BMSK         0x8
#define VIDC_SM_METADATA_ENABLE_SEI_VIDC_SHFT         3
#define VIDC_SM_METADATA_ENABLE_VC1_PARAM_BMSK       0x4
#define VIDC_SM_METADATA_ENABLE_VC1_PARAM_SHFT       2
#define VIDC_SM_METADATA_ENABLE_CONCEALED_MB_BMSK    0x2
#define VIDC_SM_METADATA_ENABLE_CONCEALED_MB_SHFT    1
#define VIDC_SM_METADATA_ENABLE_QP_BMSK              0x1
#define VIDC_SM_METADATA_ENABLE_QP_SHFT              0

#define VIDC_SM_ASPECT_RATIO_INFO_ADDR               0x00c8
#define VIDC_SM_MPEG4_ASPECT_RATIO_INFO_BMSK         0xf
#define VIDC_SM_MPEG4_ASPECT_RATIO_INFO_SHFT         0x0
#define VIDC_SM_EXTENDED_PAR_ADDR                    0x00cc
#define VIDC_SM_EXTENDED_PAR_WIDTH_BMSK              0xffff0000
#define VIDC_SM_EXTENDED_PAR_WIDTH_SHFT              0xf
#define VIDC_SM_EXTENDED_PAR_HEIGHT_BMSK             0x0000ffff
#define VIDC_SM_EXTENDED_PAR_HEIGHT_SHFT             0x0

#define VIDC_SM_METADATA_STATUS_ADDR         0x003c
#define VIDC_SM_METADATA_STATUS_STATUS_BMSK  0x1
#define VIDC_SM_METADATA_STATUS_STATUS_SHFT  0

#define VIDC_SM_METADATA_DISPLAY_INDEX_ADDR   0x0040
#define VIDC_SM_EXT_METADATA_START_ADDR_ADDR  0x0044

#define VIDC_SM_PUT_EXTRADATA_ADDR      0x0048
#define VIDC_SM_PUT_EXTRADATA_PUT_BMSK  0x1
#define VIDC_SM_PUT_EXTRADATA_PUT_SHFT  0

#define VIDC_SM_EXTRADATA_ADDR_ADDR     0x004c

#define VIDC_SM_CHROMA_ADDR_CHANGE_ADDR   0x0148
#define VIDC_SM_CHROMA_ADDR_CHANGE_BMASK  0x00000001
#define VIDC_SM_CHROMA_ADDR_CHANGE_SHFT   0

#define VIDC_SM_ERROR_CONCEALMENT_CONFIG_ADDR   0x0154

#define VIDC_SM_ERROR_CONCEALMENT_CONFIG_INTER_SLICE_BMSK  0x0c
#define VIDC_SM_ERROR_CONCEALMENT_CONFIG_INTER_SLICE_SHFT 2
#define VIDC_SM_ERROR_CONCEALMENT_CONFIG_INTRA_SLICE_BMSK 0X02
#define VIDC_SM_ERROR_CONCEALMENT_CONFIG_INTRA_SLICE_SHFT 1
#define VIDC_SM_ERROR_CONCEALMENT_CONFIG_CONCEAL_ENABLE_BMSK  0x01
#define VIDC_SM_ERROR_CONCEALMENT_CONFIG_CONCEAL_ENABLE_SHFT   0

#define VIDC_SM_SEI_ENABLE_ADDR                     0x0180
#define VIDC_SM_SEI_ENABLE_RECOVERY_POINT_SEI_BMSK  0x00000001
#define VIDC_SM_SEI_ENABLE_RECOVERY_POINT_SEI_SHFT  0

#define VIDC_SM_NUM_STUFF_BYTES_CONSUME_ADDR    0X01ac

#define VIDC_SM_ENC_EXT_CTRL_CLOSED_GOP_ENABLE_BMSK	0x40
#define VIDC_SM_ENC_EXT_CTRL_CLOSED_GOP_ENABLE_SHFT	6

#define DDL_MEM_WRITE_32(base, offset, val) ddl_mem_write_32(\
	(u32 *) ((u8 *) (base)->align_virtual_addr + (offset)), (val))
#define DDL_MEM_READ_32(base, offset) ddl_mem_read_32(\
	(u32 *) ((u8 *) (base)->align_virtual_addr + (offset)))

#define DDL_SHARED_MEM_11BIT_RIGHT_SHIFT  11

static void ddl_mem_write_32(u32 *addr, u32 data)
{
	*addr = data;
}

static u32 ddl_mem_read_32(u32 *addr)
{
	return *addr;
}

void vidc_sm_get_extended_decode_status(struct ddl_buf_addr *shared_mem,
	u32 *more_field_needed,
	u32 *resl_change)
{
	u32 decode_status = DDL_MEM_READ_32(shared_mem,
					VIDC_SM_EXTENDED_DECODE_STATUS_ADDR);
	if (more_field_needed)
		*more_field_needed =
				VIDC_GETFIELD(decode_status,
				VIDC_SM_EXT_DEC_STATUS_MORE_FIELD_NEEDED_BMSK,
				VIDC_SM_EXT_DEC_STATUS_MORE_FIELD_NEEDED_SHFT);
	if (resl_change)
		*resl_change =
				VIDC_GETFIELD(decode_status,
				VIDC_SM_EXT_DEC_STATUS_RESOLUTION_CHANGE_BMSK,
				VIDC_SM_EXT_DEC_STATUS_RESOLUTION_CHANGE_SHFT);
}

void vidc_sm_set_frame_tag(struct ddl_buf_addr *shared_mem,
	u32 frame_tag)
{
	DDL_MEM_WRITE_32(shared_mem, VIDC_SM_SET_FRAME_TAG_ADDR, frame_tag);
}

void vidc_sm_get_frame_tags(struct ddl_buf_addr *shared_mem,
	u32  *pn_frame_tag_top, u32 *pn_frame_tag_bottom)
{
	*pn_frame_tag_top = DDL_MEM_READ_32(shared_mem,
				VIDC_SM_GET_FRAME_TAG_TOP_ADDR);
	*pn_frame_tag_bottom = DDL_MEM_READ_32(shared_mem,
					VIDC_SM_GET_FRAME_TAG_BOTTOM_ADDR);
}

void vidc_sm_get_picture_times(struct ddl_buf_addr *shared_mem,
	u32 *pn_time_top, u32 *pn_time_bottom)
{
	*pn_time_top = DDL_MEM_READ_32(shared_mem, VIDC_SM_PIC_TIME_TOP_ADDR);
	*pn_time_bottom = DDL_MEM_READ_32(shared_mem,
						VIDC_SM_PIC_TIME_BOTTOM_ADDR);
}

void vidc_sm_set_start_byte_number(struct ddl_buf_addr *shared_mem,
	u32 byte_num)
{
	DDL_MEM_WRITE_32(shared_mem, VIDC_SM_START_BYTE_NUM_ADDR, byte_num);
}

void vidc_sm_get_crop_info(struct ddl_buf_addr *shared_mem,
	u32 *pn_left, u32 *pn_right, u32 *pn_top, u32 *pn_bottom)
{
	u32 info1, info2;

	info1 = DDL_MEM_READ_32(shared_mem, VIDC_SM_CROP_INFO1_ADDR);

	*pn_left = VIDC_GETFIELD(info1, VIDC_SM_CROP_INFO1_LEFT_OFFSET_BMSK,
					VIDC_SM_CROP_INFO1_LEFT_OFFSET_SHFT);
	*pn_right = VIDC_GETFIELD(info1, VIDC_SM_CROP_INFO1_RIGHT_OFFSET_BMSK,
					VIDC_SM_CROP_INFO1_RIGHT_OFFSET_SHFT);
	info2 = DDL_MEM_READ_32(shared_mem, VIDC_SM_CROP_INFO2_ADDR);
	*pn_top = VIDC_GETFIELD(info2, VIDC_SM_CROP_INFO2_TOP_OFFSET_BMSK,
					VIDC_SM_CROP_INFO2_TOP_OFFSET_SHFT);
	*pn_bottom = VIDC_GETFIELD(info2,
					VIDC_SM_CROP_INFO2_BOTTOM_OFFSET_BMSK,
					VIDC_SM_CROP_INFO2_BOTTOM_OFFSET_SHFT);
}

void vidc_sm_get_displayed_picture_frame(struct ddl_buf_addr
	*shared_mem, u32  *n_disp_picture_frame)
{
	u32 disp_pict_frame;

	disp_pict_frame = DDL_MEM_READ_32(shared_mem,
					VIDC_SM_DISP_PIC_FRAME_TYPE_ADDR);
	*n_disp_picture_frame = VIDC_GETFIELD(disp_pict_frame,
			VIDC_SM_DISP_PIC_FRAME_TYPE_BMSK,
			VIDC_SM_DISP_PIC_FRAME_TYPE_SHFT);
}
void vidc_sm_get_available_luma_dpb_address(struct ddl_buf_addr
	*shared_mem, u32 *pn_free_luma_dpb_address)
{
	*pn_free_luma_dpb_address = DDL_MEM_READ_32(shared_mem,
		VIDC_SM_FREE_LUMA_DPB_ADDR);
}

void vidc_sm_get_available_luma_dpb_dec_order_address(
	struct ddl_buf_addr	*shared_mem,
	u32 *pn_free_luma_dpb_address)
{
	*pn_free_luma_dpb_address = DDL_MEM_READ_32(shared_mem,
		VIDC_SM_FREE_LUMA_DPB_DEC_ORDER_ADDR);
}

void vidc_sm_get_dec_order_resl(
	struct ddl_buf_addr *shared_mem, u32 *width, u32 *height)
{
	*width = DDL_MEM_READ_32(shared_mem,
			VIDC_SM_DEC_ORDER_WIDTH_ADDR);
	*height = DDL_MEM_READ_32(shared_mem,
			VIDC_SM_DEC_ORDER_HEIGHT_ADDR);
}

void vidc_sm_get_dec_order_crop_info(
	struct ddl_buf_addr *shared_mem, u32 *left,
	u32 *right, u32 *top, u32 *bottom)
{
	u32 crop_data;
	crop_data = DDL_MEM_READ_32(shared_mem,
		VIDC_SM_DEC_CROP_INFO1_ADDR);
	*left = VIDC_GETFIELD(crop_data,
		VIDC_SM_DEC_CROP_INFO1_LEFT_OFFSET_BMSK,
		VIDC_SM_DEC_CROP_INFO1_LEFT_OFFSET_SHFT);
	*right = VIDC_GETFIELD(crop_data,
		VIDC_SM_DEC_CROP_INFO1_RIGHT_OFFSET_BMSK,
		VIDC_SM_DEC_CROP_INFO1_RIGHT_OFFSET_SHFT);
	crop_data = DDL_MEM_READ_32(shared_mem,
		VIDC_SM_DEC_CROP_INFO2_ADDR);
	*top = VIDC_GETFIELD(crop_data,
		VIDC_SM_DEC_CROP_INFO2_TOP_OFFSET_BMSK,
		VIDC_SM_DEC_CROP_INFO2_TOP_OFFSET_SHFT);
	*bottom = VIDC_GETFIELD(crop_data,
		VIDC_SM_DEC_CROP_INFO2_BOTTOM_OFFSET_BMSK,
		VIDC_SM_DEC_CROP_INFO2_BOTTOM_OFFSET_SHFT);
}

void vidc_sm_set_extended_encoder_control(struct ddl_buf_addr
	*shared_mem, u32 hec_enable,
	enum VIDC_SM_frame_skip frame_skip_mode,
	u32 seq_hdr_in_band, u32 vbv_buffer_size, u32 cpcfc_enable,
	u32 sps_pps_control, u32 closed_gop_enable)
{
	u32 enc_ctrl;

	enc_ctrl = VIDC_SETFIELD((hec_enable) ? 1 : 0,
			VIDC_SM_ENC_EXT_CTRL_HEC_ENABLE_SHFT,
			VIDC_SM_ENC_EXT_CTRL_HEC_ENABLE_BMSK) |
			VIDC_SETFIELD((u32) frame_skip_mode,
			VIDC_SM_ENC_EXT_CTRL_FRAME_SKIP_ENABLE_SHFT,
			VIDC_SM_ENC_EXT_CTRL_FRAME_SKIP_ENABLE_BMSK) |
			VIDC_SETFIELD((seq_hdr_in_band) ? 1 : 0 ,
			VIDC_SM_ENC_EXT_CTRL_SEQ_HDR_CTRL_SHFT ,
			VIDC_SM_ENC_EXT_CTRL_SEQ_HDR_CTRL_BMSK) |
			VIDC_SETFIELD(vbv_buffer_size,
			VIDC_SM_ENC_EXT_CTRL_VBV_BUFFER_SIZE_SHFT,
			VIDC_SM_ENC_EXT_CTRL_VBV_BUFFER_SIZE_BMSK) |
			VIDC_SETFIELD((cpcfc_enable) ? 1 : 0,
			VIDC_SM_ENC_EXT_CTRL_H263_CPCFC_ENABLE_SHFT,
			VIDC_SM_ENC_EXT_CTRL_H263_CPCFC_ENABLE_BMSK) |
			VIDC_SETFIELD((sps_pps_control) ? 1 : 0,
			VIDC_SM_ENC_EXT_CTRL_SPS_PPS_CONTROL_SHFT,
			VIDC_SM_ENC_EXT_CTRL_SPS_PPS_CONTROL_BMSK) |
			VIDC_SETFIELD(closed_gop_enable,
			VIDC_SM_ENC_EXT_CTRL_CLOSED_GOP_ENABLE_SHFT,
			VIDC_SM_ENC_EXT_CTRL_CLOSED_GOP_ENABLE_BMSK);
	DDL_MEM_WRITE_32(shared_mem, VIDC_SM_ENC_EXT_CTRL_ADDR, enc_ctrl);
}

void vidc_sm_set_encoder_param_change(struct ddl_buf_addr *shared_mem,
	u32 bit_rate_chg, u32 frame_rate_chg, u32 i_period_chg)
{
	u32 enc_param_chg;

	enc_param_chg = VIDC_SETFIELD((bit_rate_chg) ? 1 : 0,
				VIDC_SM_ENC_PARAM_CHANGE_RC_BIT_RATE_SHFT,
				VIDC_SM_ENC_PARAM_CHANGE_RC_BIT_RATE_BMSK) |
				VIDC_SETFIELD((frame_rate_chg) ? 1 : 0,
				VIDC_SM_ENC_PARAM_CHANGE_RC_FRAME_RATE_SHFT,
				VIDC_SM_ENC_PARAM_CHANGE_RC_FRAME_RATE_BMSK) |
				VIDC_SETFIELD((i_period_chg) ? 1 : 0,
				VIDC_SM_ENC_PARAM_CHANGE_I_PERIOD_SHFT,
				VIDC_SM_ENC_PARAM_CHANGE_I_PERIOD_BMSK);
	DDL_MEM_WRITE_32(shared_mem, VIDC_SM_ENC_PARAM_CHANGE_ADDR,
		enc_param_chg);
}

void vidc_sm_set_encoder_vop_time(struct ddl_buf_addr *shared_mem,
	u32 vop_time_enable, u32 time_resolution, u32 frame_delta)
{
	u32 vop_time;

	vop_time = VIDC_SETFIELD((vop_time_enable) ? 1 : 0,
			VIDC_SM_ENC_VOP_TIMING_ENABLE_SHFT ,
			VIDC_SM_ENC_VOP_TIMING_ENABLE_BMSK) |
			VIDC_SETFIELD(time_resolution ,
			VIDC_SM_ENC_VOP_TIMING_TIME_RESOLUTION_SHFT,
			VIDC_SM_ENC_VOP_TIMING_TIME_RESOLUTION_BMSK) |
			VIDC_SETFIELD(frame_delta,
			VIDC_SM_ENC_VOP_TIMING_FRAME_DELTA_SHFT,
			VIDC_SM_ENC_VOP_TIMING_FRAME_DELTA_BMSK);
	DDL_MEM_WRITE_32(shared_mem, VIDC_SM_ENC_VOP_TIMING_ADDR, vop_time);
}

void vidc_sm_set_encoder_hec_period(struct ddl_buf_addr *shared_mem,
	u32 hec_period)
{
	DDL_MEM_WRITE_32(shared_mem, VIDC_SM_ENC_HEC_PERIOD_ADDR,
		hec_period);
}

void vidc_sm_get_h264_encoder_reference_list0(struct ddl_buf_addr
	*shared_mem, enum VIDC_SM_ref_picture *pe_luma_picture0,
	u32 *pn_luma_picture_index0, enum VIDC_SM_ref_picture
		*pe_luma_picture1, u32 *pn_luma_picture_index1,
	enum VIDC_SM_ref_picture *pe_chroma_picture0,
	u32 *pn_chroma_picture_index0,
	enum VIDC_SM_ref_picture *pe_chroma_picture1,
	u32 *pn_chroma_picture_index1)
{
	u32 ref_list;

	ref_list = DDL_MEM_READ_32(shared_mem, VIDC_SM_H264_REF_L0_ADDR);

	*pe_luma_picture0 = (enum VIDC_SM_ref_picture)
				VIDC_GETFIELD(ref_list,
				VIDC_SM_H264_REF_L0_LUMA_BTM_FLG_0_BMSK,
				VIDC_SM_H264_REF_L0_LUMA_BTM_FLG_0_SHFT);
	*pn_luma_picture_index0 =
				VIDC_GETFIELD(ref_list,
				VIDC_SM_H264_REF_L0_LUMA_REF_0_BMSK,
				VIDC_SM_H264_REF_L0_LUMA_REF_0_SHFT);
	*pe_luma_picture1 = (enum VIDC_SM_ref_picture)
				VIDC_GETFIELD(ref_list,
				VIDC_SM_H264_REF_L0_LUMA_BTM_FLG_1_BMSK,
				VIDC_SM_H264_REF_L0_LUMA_BTM_FLG_1_SHFT);
	*pn_luma_picture_index1 = VIDC_GETFIELD(ref_list,
				VIDC_SM_H264_REF_L0_LUMA_REF_1_BMSK,
				VIDC_SM_H264_REF_L0_LUMA_REF_1_SHFT);
	*pe_chroma_picture0 = (enum VIDC_SM_ref_picture)
				VIDC_GETFIELD(ref_list,
				VIDC_SM_H264_REF_L0_CHRO_BTM_FLG_0_BMSK,
				VIDC_SM_H264_REF_L0_CHRO_BTM_FLG_0_SHFT);
	*pn_chroma_picture_index0 = VIDC_GETFIELD(ref_list,
				VIDC_SM_H264_REF_L0_CHRO_REF_0_BMSK,
				VIDC_SM_H264_REF_L0_CHRO_REF_0_SHFT);
	*pe_chroma_picture1 = (enum VIDC_SM_ref_picture)
				VIDC_GETFIELD(ref_list,
				VIDC_SM_H264_REF_L0_CHRO_BTM_FLG_1_BMSK,
				VIDC_SM_H264_REF_L0_CHRO_BTM_FLG_1_SHFT);
	*pn_chroma_picture_index1 =
				VIDC_GETFIELD(ref_list,
				VIDC_SM_H264_REF_L0_CHRO_REF_1_BMSK,
				VIDC_SM_H264_REF_L0_CHRO_REF_1_SHFT);
}

void vidc_sm_get_h264_encoder_reference_list1(struct ddl_buf_addr
	*shared_mem, enum VIDC_SM_ref_picture *pe_luma_picture,
	u32 *pn_luma_picture_index,
	enum VIDC_SM_ref_picture *pe_chroma_picture,
	u32 *pn_chroma_picture_index)
{
	u32 ref_list;

	ref_list = DDL_MEM_READ_32(shared_mem, VIDC_SM_H264_REF_L1_ADDR);

	*pe_luma_picture = (enum VIDC_SM_ref_picture)
				VIDC_GETFIELD(ref_list,
				VIDC_SM_H264_REF_L1_LUMA_BTM_FLG_0_BMSK,
				VIDC_SM_H264_REF_L1_LUMA_BTM_FLG_0_SHFT);
	*pn_luma_picture_index =
				VIDC_GETFIELD(ref_list,
				VIDC_SM_H264_REF_L1_LUMA_REF_0_BMSK,
				VIDC_SM_H264_REF_L1_LUMA_REF_0_SHFT);
	*pe_chroma_picture = (enum VIDC_SM_ref_picture)
				VIDC_GETFIELD(ref_list,
				VIDC_SM_H264_REF_L1_CHRO_BTM_FLG_0_BMSK,
				VIDC_SM_H264_REF_L1_CHRO_BTM_FLG_0_SHFT);
	*pn_chroma_picture_index = VIDC_GETFIELD(ref_list,
				VIDC_SM_H264_REF_L1_CHRO_REF_0_BMSK,
				VIDC_SM_H264_REF_L1_CHRO_REF_0_SHFT);
}

void vidc_sm_set_allocated_dpb_size(struct ddl_buf_addr *shared_mem,
		u32 y_size, u32 c_size)
{
	DDL_MEM_WRITE_32(shared_mem, VIDC_SM_ALLOCATED_LUMA_DPB_SIZE_ADDR,
		y_size);
	DDL_MEM_WRITE_32(shared_mem, VIDC_SM_ALLOCATED_CHROMA_DPB_SIZE_ADDR,
		c_size);
}

void vidc_sm_set_allocated_h264_mv_size(struct ddl_buf_addr *shared_mem,
	u32 mv_size)
{
	DDL_MEM_WRITE_32(shared_mem, VIDC_SM_ALLOCATED_MV_SIZE_ADDR,
		mv_size);
}

void vidc_sm_get_min_yc_dpb_sizes(struct ddl_buf_addr *shared_mem,
	u32 *pn_min_luma_dpb_size, u32 *pn_min_chroma_dpb_size)
{
	*pn_min_luma_dpb_size = DDL_MEM_READ_32(shared_mem,
		VIDC_SM_MIN_LUMA_DPB_SIZE_ADDR);
	*pn_min_chroma_dpb_size = DDL_MEM_READ_32(shared_mem,
		VIDC_SM_MIN_CHROMA_DPB_SIZE_ADDR);
}

void vidc_sm_set_concealment_color(struct ddl_buf_addr *shared_mem,
	u32 conceal_ycolor, u32 conceal_ccolor)
{
	u32 conceal_color;

	conceal_color = (((conceal_ycolor << 8) & 0xff00) |
		(conceal_ccolor & 0xff));
	DDL_MEM_WRITE_32(shared_mem, 0x00f0, conceal_color);
}

void vidc_sm_set_metadata_enable(struct ddl_buf_addr *shared_mem,
	u32 extradata_enable, u32 qp_enable, u32 concealed_mb_enable,
	u32 vc1Param_enable, u32 sei_nal_enable, u32 vui_enable,
	u32 enc_slice_size_enable)
{
	u32 metadata_enable;

	metadata_enable = VIDC_SETFIELD((extradata_enable) ? 1 : 0,
				VIDC_SM_METADATA_ENABLE_EXTRADATA_SHFT,
				VIDC_SM_METADATA_ENABLE_EXTRADATA_BMSK) |
				VIDC_SETFIELD((enc_slice_size_enable) ? 1 : 0,
				VIDC_SM_METADATA_ENABLE_ENC_SLICE_SIZE_SHFT,
				VIDC_SM_METADATA_ENABLE_ENC_SLICE_SIZE_BMSK) |
				VIDC_SETFIELD((vui_enable) ? 1 : 0,
				VIDC_SM_METADATA_ENABLE_VUI_SHFT,
				VIDC_SM_METADATA_ENABLE_VUI_BMSK) |
				VIDC_SETFIELD((sei_nal_enable) ? 1 : 0,
				VIDC_SM_METADATA_ENABLE_SEI_VIDC_SHFT,
				VIDC_SM_METADATA_ENABLE_SEI_VIDC_BMSK) |
				VIDC_SETFIELD((vc1Param_enable) ? 1 : 0,
				VIDC_SM_METADATA_ENABLE_VC1_PARAM_SHFT,
				VIDC_SM_METADATA_ENABLE_VC1_PARAM_BMSK) |
				VIDC_SETFIELD((concealed_mb_enable) ? 1 : 0,
				VIDC_SM_METADATA_ENABLE_CONCEALED_MB_SHFT,
				VIDC_SM_METADATA_ENABLE_CONCEALED_MB_BMSK) |
				VIDC_SETFIELD((qp_enable) ? 1 : 0,
				VIDC_SM_METADATA_ENABLE_QP_SHFT,
				VIDC_SM_METADATA_ENABLE_QP_BMSK);
	DDL_MEM_WRITE_32(shared_mem, VIDC_SM_METADATA_ENABLE_ADDR,
		metadata_enable);
}

void vidc_sm_get_metadata_status(struct ddl_buf_addr
		*shared_mem, u32 *pb_metadata_present)
{
	u32 status;

	status = DDL_MEM_READ_32(shared_mem, VIDC_SM_METADATA_STATUS_ADDR);
	*pb_metadata_present = (u32) VIDC_GETFIELD(status,
				VIDC_SM_METADATA_STATUS_STATUS_BMSK,
				VIDC_SM_METADATA_STATUS_STATUS_SHFT);
}

void vidc_sm_get_metadata_display_index(struct ddl_buf_addr *shared_mem,
	u32 *pn_dixplay_index)
{
	*pn_dixplay_index = DDL_MEM_READ_32(shared_mem,
					VIDC_SM_METADATA_DISPLAY_INDEX_ADDR);
}

void vidc_sm_set_metadata_start_address(struct ddl_buf_addr *shared_mem,
	u32 address)
{
	u32 address_shift = address;

	DDL_MEM_WRITE_32(shared_mem, VIDC_SM_EXT_METADATA_START_ADDR_ADDR,
		address_shift);
}

void vidc_sm_set_extradata_presence(struct ddl_buf_addr *shared_mem,
	u32 extradata_present)
{
	u32 put_extradata;

	put_extradata = VIDC_SETFIELD((extradata_present) ? 1 : 0,
				VIDC_SM_PUT_EXTRADATA_PUT_SHFT,
				VIDC_SM_PUT_EXTRADATA_PUT_BMSK);
	DDL_MEM_WRITE_32(shared_mem, VIDC_SM_PUT_EXTRADATA_ADDR,
			put_extradata);
}

void vidc_sm_set_extradata_addr(struct ddl_buf_addr *shared_mem,
	u32 extradata_addr)
{
	u32 address_shift = extradata_addr;

	DDL_MEM_WRITE_32(shared_mem, VIDC_SM_EXTRADATA_ADDR_ADDR,
		address_shift);
}

void vidc_sm_set_pand_b_frame_qp(struct ddl_buf_addr *shared_mem,
	u32 b_frame_qp, u32 p_frame_qp)
{
	u32 nP_B_frame_qp;

	nP_B_frame_qp = VIDC_SETFIELD(b_frame_qp,
				VIDC_SM_P_B_FRAME_QP_B_FRAME_QP_SHFT,
				VIDC_SM_P_B_FRAME_QP_B_FRAME_QP_BMASK);
	nP_B_frame_qp |= VIDC_SETFIELD(p_frame_qp,
				VIDC_SM_P_B_FRAME_QP_P_FRAME_QP_SHFT,
				VIDC_SM_P_B_FRAME_QP_P_FRAME_QP_BMASK);
	DDL_MEM_WRITE_32(shared_mem , VIDC_SM_P_B_FRAME_QP_ADDR,
		nP_B_frame_qp);
}


void vidc_sm_get_profile_info(struct ddl_buf_addr *shared_mem,
	struct ddl_profile_info_type *ddl_profile_info)
{
	u32 disp_pic_profile;

	disp_pic_profile = DDL_MEM_READ_32(shared_mem,
		VIDC_SM_DISP_PIC_PROFILE_ADDR);
	ddl_profile_info->bit_depth_chroma_minus8 =
		(disp_pic_profile  & 0x00380000) >> 19;
	ddl_profile_info->bit_depth_luma_minus8 =
		(disp_pic_profile & 0x00070000) >> 16;
	ddl_profile_info->pic_profile = VIDC_GETFIELD(
		disp_pic_profile,
		VIDC_SM_DISP_PIC_PROFILE_DISP_PIC_PROFILE_BMASK,
		VIDC_SM_DISP_PIC_PROFILE_DISP_PIC_PROFILE_SHFT);
	ddl_profile_info->pic_level = VIDC_GETFIELD(
		disp_pic_profile,
		VIDC_SM_DISP_PIC_PROFILE_DISP_PIC_LEVEL_BMASK,
		VIDC_SM_DISP_PIC_PROFILE_DISP_PIC_LEVEL_SHFT);
	ddl_profile_info->chroma_format_idc =
		(disp_pic_profile & 0x60) >> 5;
}

void vidc_sm_set_encoder_new_bit_rate(struct ddl_buf_addr *shared_mem,
	u32 new_bit_rate)
{
	DDL_MEM_WRITE_32(shared_mem, VIDC_SM_NEW_RC_BIT_RATE_ADDR,
		new_bit_rate);
}

void vidc_sm_set_encoder_new_frame_rate(struct ddl_buf_addr *shared_mem,
	u32 new_frame_rate)
{
	DDL_MEM_WRITE_32(shared_mem, VIDC_SM_NEW_RC_FRAME_RATE_ADDR,
		new_frame_rate);
}

void vidc_sm_set_encoder_new_i_period(struct ddl_buf_addr *shared_mem,
	u32 new_i_period)
{
	DDL_MEM_WRITE_32(shared_mem, VIDC_SM_NEW_I_PERIOD_ADDR,
		new_i_period);
}
void vidc_sm_set_encoder_init_rc_value(struct ddl_buf_addr *shared_mem,
	u32 new_rc_value)
{
	DDL_MEM_WRITE_32(shared_mem, 0x011C, new_rc_value);

}
void vidc_sm_set_idr_decode_only(struct ddl_buf_addr *shared_mem,
	u32 enable)
{
	u32 idr_decode_only = VIDC_SETFIELD((enable) ? 1 : 0,
			VIDC_SM_IDR_DECODING_ONLY_SHIFT,
			VIDC_SM_IDR_DECODING_ONLY_BMSK
			);
	DDL_MEM_WRITE_32(shared_mem, VIDC_SM_IDR_DECODING_ONLY_ADDR,
			idr_decode_only);
}

void vidc_sm_set_chroma_addr_change(struct ddl_buf_addr *shared_mem,
	u32 addr_change)
{
	u32 chroma_addr_change = VIDC_SETFIELD((addr_change) ? 1 : 0,
					VIDC_SM_CHROMA_ADDR_CHANGE_SHFT,
					VIDC_SM_CHROMA_ADDR_CHANGE_BMASK);
	DDL_MEM_WRITE_32(shared_mem, VIDC_SM_CHROMA_ADDR_CHANGE_ADDR,
					 chroma_addr_change);

}

void vidc_sm_set_mpeg4_profile_override(struct ddl_buf_addr *shared_mem,
	enum vidc_sm_mpeg4_profileinfo profile_info)
{
	u32 profile_enforce = 0;
	if (shared_mem != NULL) {
		profile_enforce = 1;
		switch (profile_info) {
		case VIDC_SM_PROFILE_INFO_ASP:
			profile_enforce |= 4;
			break;
		case VIDC_SM_PROFILE_INFO_SP:
			profile_enforce |= 2;
			break;
		case VIDC_SM_PROFILE_INFO_DISABLE:
		default:
			profile_enforce = 0;
			break;
		}
		DDL_MEM_WRITE_32(shared_mem, 0x15c, profile_enforce);
	}
}
void vidc_sm_set_decoder_sei_enable(struct ddl_buf_addr *shared_mem,
	u32 sei_enable)
{
	DDL_MEM_WRITE_32(shared_mem, VIDC_SM_SEI_ENABLE_ADDR, sei_enable);
}

void vidc_sm_get_decoder_sei_enable(struct ddl_buf_addr *shared_mem,
	u32 *sei_enable)
{
	*sei_enable = DDL_MEM_READ_32(shared_mem, VIDC_SM_SEI_ENABLE_ADDR);
}

void vidc_sm_set_error_concealment_config(struct ddl_buf_addr *shared_mem,
	u32 inter_slice, u32 intra_slice, u32 conceal_config_enable)
{
	u32 error_conceal_config = 0;

	error_conceal_config = VIDC_SETFIELD(inter_slice,
			VIDC_SM_ERROR_CONCEALMENT_CONFIG_INTER_SLICE_SHFT,
			VIDC_SM_ERROR_CONCEALMENT_CONFIG_INTER_SLICE_BMSK);

	error_conceal_config |= VIDC_SETFIELD(intra_slice,
			VIDC_SM_ERROR_CONCEALMENT_CONFIG_INTRA_SLICE_SHFT,
			VIDC_SM_ERROR_CONCEALMENT_CONFIG_INTRA_SLICE_BMSK);

	error_conceal_config |= VIDC_SETFIELD(conceal_config_enable,
			VIDC_SM_ERROR_CONCEALMENT_CONFIG_CONCEAL_ENABLE_SHFT,
			VIDC_SM_ERROR_CONCEALMENT_CONFIG_CONCEAL_ENABLE_BMSK);

	DDL_MEM_WRITE_32(shared_mem, VIDC_SM_ERROR_CONCEALMENT_CONFIG_ADDR,
			error_conceal_config);
}

void vidc_sm_set_decoder_stuff_bytes_consumption(
	struct ddl_buf_addr *shared_mem,
	enum vidc_sm_num_stuff_bytes_consume_info consume_info)
{
	DDL_MEM_WRITE_32(shared_mem, VIDC_SM_NUM_STUFF_BYTES_CONSUME_ADDR,
	consume_info);
}

void vidc_sm_get_aspect_ratio_info(struct ddl_buf_addr *shared_mem,
	struct vcd_aspect_ratio *aspect_ratio_info)
{
	u32 extended_par_info = 0;
	aspect_ratio_info->aspect_ratio = DDL_MEM_READ_32(shared_mem,
				VIDC_SM_ASPECT_RATIO_INFO_ADDR);

	if (aspect_ratio_info->aspect_ratio == 0x0f) {
		extended_par_info = DDL_MEM_READ_32(shared_mem,
			VIDC_SM_EXTENDED_PAR_ADDR);
		aspect_ratio_info->extended_par_width =
			VIDC_GETFIELD(extended_par_info,
			VIDC_SM_EXTENDED_PAR_WIDTH_BMSK,
			VIDC_SM_EXTENDED_PAR_WIDTH_SHFT);
		aspect_ratio_info->extended_par_height =
			VIDC_GETFIELD(extended_par_info,
			VIDC_SM_EXTENDED_PAR_HEIGHT_BMSK,
			VIDC_SM_EXTENDED_PAR_HEIGHT_SHFT);
	}
}
