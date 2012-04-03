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
#ifndef _VCD_DRIVER_PROPERTY_H_
#define _VCD_DRIVER_PROPERTY_H_

#define VCD_START_BASE       0x0
#define VCD_I_LIVE           (VCD_START_BASE + 0x1)
#define VCD_I_CODEC          (VCD_START_BASE + 0x2)
#define VCD_I_FRAME_SIZE     (VCD_START_BASE + 0x3)
#define VCD_I_METADATA_ENABLE  (VCD_START_BASE + 0x4)
#define VCD_I_METADATA_HEADER  (VCD_START_BASE + 0x5)
#define VCD_I_PROFILE        (VCD_START_BASE + 0x6)
#define VCD_I_LEVEL          (VCD_START_BASE + 0x7)
#define VCD_I_BUFFER_FORMAT  (VCD_START_BASE + 0x8)
#define VCD_I_FRAME_RATE  (VCD_START_BASE + 0x9)
#define VCD_I_TARGET_BITRATE (VCD_START_BASE + 0xA)
#define VCD_I_MULTI_SLICE    (VCD_START_BASE + 0xB)
#define VCD_I_ENTROPY_CTRL   (VCD_START_BASE + 0xC)
#define VCD_I_DEBLOCKING     (VCD_START_BASE + 0xD)
#define VCD_I_RATE_CONTROL   (VCD_START_BASE + 0xE)
#define VCD_I_QP_RANGE      (VCD_START_BASE + 0xF)
#define VCD_I_SESSION_QP    (VCD_START_BASE + 0x10)
#define VCD_I_INTRA_PERIOD   (VCD_START_BASE + 0x11)
#define VCD_I_VOP_TIMING     (VCD_START_BASE + 0x12)
#define VCD_I_SHORT_HEADER   (VCD_START_BASE + 0x13)
#define VCD_I_SEQ_HEADER    (VCD_START_BASE + 0x14)
#define VCD_I_HEADER_EXTENSION   (VCD_START_BASE + 0x15)
#define VCD_I_INTRA_REFRESH  (VCD_START_BASE + 0x16)
#define VCD_I_POST_FILTER    (VCD_START_BASE + 0x17)
#define VCD_I_PROGRESSIVE_ONLY (VCD_START_BASE + 0x18)
#define VCD_I_OUTPUT_ORDER (VCD_START_BASE + 0x19)
#define VCD_I_RECON_BUFFERS   (VCD_START_BASE + 0x1A)
#define VCD_I_FREE_RECON_BUFFERS   (VCD_START_BASE + 0x1B)
#define VCD_I_GET_RECON_BUFFER_SIZE   (VCD_START_BASE + 0x1C)
#define VCD_I_H264_MV_BUFFER   (VCD_START_BASE + 0x1D)
#define VCD_I_FREE_H264_MV_BUFFER (VCD_START_BASE + 0x1E)
#define VCD_I_GET_H264_MV_SIZE (VCD_START_BASE + 0x1F)
#define VCD_I_DEC_PICTYPE (VCD_START_BASE + 0x20)
#define VCD_I_CONT_ON_RECONFIG (VCD_START_BASE + 0x21)
#define VCD_I_META_BUFFER_MODE (VCD_START_BASE + 0x22)
#define VCD_I_DISABLE_DMX (VCD_START_BASE + 0x23)
#define VCD_I_DISABLE_DMX_SUPPORT (VCD_START_BASE + 0x24)
#define VCD_I_ENABLE_SPS_PPS_FOR_IDR (VCD_START_BASE + 0x25)
#define VCD_REQ_PERF_LEVEL (VCD_START_BASE + 0x26)
#define VCD_I_SLICE_DELIVERY_MODE (VCD_START_BASE + 0x27)
#define VCD_I_VOP_TIMING_CONSTANT_DELTA (VCD_START_BASE + 0x28)

#define VCD_START_REQ      (VCD_START_BASE + 0x1000)
#define VCD_I_REQ_IFRAME   (VCD_START_REQ + 0x1)

#define VCD_I_RESERVED_BASE  (VCD_START_BASE + 0x10000)

struct vcd_property_hdr {
	u32    prop_id;
	size_t sz;
};

struct vcd_property_live {
	u32             live;
};

enum vcd_codec {
	VCD_CODEC_H264      = 0x1,
	VCD_CODEC_H263      = 0x2,
	VCD_CODEC_MPEG1     = 0x3,
	VCD_CODEC_MPEG2     = 0x4,
	VCD_CODEC_MPEG4     = 0x5,
	VCD_CODEC_DIVX_3    = 0x6,
	VCD_CODEC_DIVX_4    = 0x7,
	VCD_CODEC_DIVX_5    = 0x8,
	VCD_CODEC_DIVX_6    = 0x9,
	VCD_CODEC_XVID      = 0xA,
	VCD_CODEC_VC1       = 0xB,
	VCD_CODEC_VC1_RCV   = 0xC
};

struct vcd_property_codec {
	enum vcd_codec       codec;
};

struct vcd_property_frame_size {
	u32              width;
	u32              height;
	u32              stride;
	u32              scan_lines;
};

enum vcd_perf_level {
	VCD_PERF_LEVEL0,
	VCD_PERF_LEVEL1,
	VCD_PERF_LEVEL2,
};

#define VCD_METADATA_DATANONE       0x001
#define VCD_METADATA_QCOMFILLER     0x002
#define VCD_METADATA_QPARRAY        0x004
#define VCD_METADATA_CONCEALMB      0x008
#define VCD_METADATA_SEI            0x010
#define VCD_METADATA_VUI            0x020
#define VCD_METADATA_VC1            0x040
#define VCD_METADATA_PASSTHROUGH    0x080
#define VCD_METADATA_ENC_SLICE      0x100

struct vcd_property_meta_data_enable {
	u32 meta_data_enable_flag;
};

struct vcd_property_metadata_hdr {
	u32 meta_data_id;
	u32 version;
	u32 port_index;
	u32 type;
};

struct vcd_property_frame_rate {
	u32              fps_denominator;
	u32              fps_numerator;
};

struct vcd_property_target_bitrate {
	u32             target_bitrate;
};

struct vcd_property_perf_level {
	enum vcd_perf_level level;
};

enum vcd_yuv_buffer_format {
	VCD_BUFFER_FORMAT_NV12      = 0x1,
	VCD_BUFFER_FORMAT_TILE_4x2    = 0x2,
	VCD_BUFFER_FORMAT_NV12_16M2KA = 0x3,
	VCD_BUFFER_FORMAT_TILE_1x1    = 0x4
};

struct vcd_property_buffer_format {
	enum vcd_yuv_buffer_format  buffer_format;
};

struct vcd_property_post_filter {
	u32           post_filter;
};

enum vcd_codec_profile {
	VCD_PROFILE_UNKNOWN       = 0x0,
	VCD_PROFILE_MPEG4_SP      = 0x1,
	VCD_PROFILE_MPEG4_ASP     = 0x2,
	VCD_PROFILE_H264_BASELINE = 0x3,
	VCD_PROFILE_H264_MAIN     = 0x4,
	VCD_PROFILE_H264_HIGH     = 0x5,
	VCD_PROFILE_H263_BASELINE = 0x6,
	VCD_PROFILE_VC1_SIMPLE    = 0x7,
	VCD_PROFILE_VC1_MAIN      = 0x8,
	VCD_PROFILE_VC1_ADVANCE   = 0x9,
	VCD_PROFILE_MPEG2_MAIN    = 0xA,
	VCD_PROFILE_MPEG2_SIMPLE  = 0xB
};

struct vcd_property_profile {
	enum vcd_codec_profile       profile;
};

enum vcd_codec_level {
	VCD_LEVEL_UNKNOWN       = 0x0,
	VCD_LEVEL_MPEG4_0       = 0x1,
	VCD_LEVEL_MPEG4_0b      = 0x2,
	VCD_LEVEL_MPEG4_1       = 0x3,
	VCD_LEVEL_MPEG4_2       = 0x4,
	VCD_LEVEL_MPEG4_3       = 0x5,
	VCD_LEVEL_MPEG4_3b      = 0x6,
	VCD_LEVEL_MPEG4_4       = 0x7,
	VCD_LEVEL_MPEG4_4a      = 0x8,
	VCD_LEVEL_MPEG4_5       = 0x9,
	VCD_LEVEL_MPEG4_6       = 0xA,
	VCD_LEVEL_MPEG4_7       = 0xB,
	VCD_LEVEL_MPEG4_X       = 0xC,
	VCD_LEVEL_H264_1        = 0x10,
	VCD_LEVEL_H264_1b       = 0x11,
	VCD_LEVEL_H264_1p1      = 0x12,
	VCD_LEVEL_H264_1p2      = 0x13,
	VCD_LEVEL_H264_1p3      = 0x14,
	VCD_LEVEL_H264_2        = 0x15,
	VCD_LEVEL_H264_2p1      = 0x16,
	VCD_LEVEL_H264_2p2      = 0x17,
	VCD_LEVEL_H264_3        = 0x18,
	VCD_LEVEL_H264_3p1      = 0x19,
	VCD_LEVEL_H264_3p2      = 0x1A,
	VCD_LEVEL_H264_4        = 0x1B,
	VCD_LEVEL_H264_4p1      = 0x1C,
	VCD_LEVEL_H264_4p2      = 0x1D,
	VCD_LEVEL_H264_5        = 0x1E,
	VCD_LEVEL_H264_5p1      = 0x1F,
	VCD_LEVEL_H263_10       = 0x20,
	VCD_LEVEL_H263_20       = 0x21,
	VCD_LEVEL_H263_30       = 0x22,
	VCD_LEVEL_H263_40       = 0x23,
	VCD_LEVEL_H263_45       = 0x24,
	VCD_LEVEL_H263_50       = 0x25,
	VCD_LEVEL_H263_60       = 0x26,
	VCD_LEVEL_H263_70       = 0x27,
	VCD_LEVEL_H263_X        = 0x28,
	VCD_LEVEL_MPEG2_LOW     = 0x30,
	VCD_LEVEL_MPEG2_MAIN    = 0x31,
	VCD_LEVEL_MPEG2_HIGH_14 = 0x32,
	VCD_LEVEL_MPEG2_HIGH    = 0x33,
	VCD_LEVEL_MPEG2_X       = 0x34,
	VCD_LEVEL_VC1_S_LOW     = 0x40,
	VCD_LEVEL_VC1_S_MEDIUM  = 0x41,
	VCD_LEVEL_VC1_M_LOW     = 0x42,
	VCD_LEVEL_VC1_M_MEDIUM  = 0x43,
	VCD_LEVEL_VC1_M_HIGH    = 0x44,
	VCD_LEVEL_VC1_A_0       = 0x45,
	VCD_LEVEL_VC1_A_1       = 0x46,
	VCD_LEVEL_VC1_A_2       = 0x47,
	VCD_LEVEL_VC1_A_3       = 0x48,
	VCD_LEVEL_VC1_A_4       = 0x49,
	VCD_LEVEL_VC1_X         = 0x4A
};

struct vcd_property_level {
	enum vcd_codec_level   level;
};

enum vcd_m_slice_sel {
	VCD_MSLICE_OFF             = 0x1,
	VCD_MSLICE_BY_MB_COUNT     = 0x2,
	VCD_MSLICE_BY_BYTE_COUNT   = 0x3,
	VCD_MSLICE_BY_GOB          = 0x4
};

struct vcd_property_multi_slice {
	enum vcd_m_slice_sel   m_slice_sel;
	u32             m_slice_size;
};

enum vcd_entropy_sel {
	VCD_ENTROPY_SEL_CAVLC = 0x1,
	VCD_ENTROPY_SEL_CABAC = 0x2
};

enum vcd_cabac_model {
	VCD_CABAC_MODEL_NUMBER_0 = 0x1,
	VCD_CABAC_MODEL_NUMBER_1 = 0x2,
	VCD_CABAC_MODEL_NUMBER_2 = 0x3
};

struct vcd_property_entropy_control {
	enum vcd_entropy_sel  entropy_sel;
	enum vcd_cabac_model  cabac_model;
};

enum vcd_db_config {
	VCD_DB_ALL_BLOCKING_BOUNDARY = 0x1,
	VCD_DB_DISABLE               = 0x2,
	VCD_DB_SKIP_SLICE_BOUNDARY   = 0x3
};
struct vcd_property_db_config {
	enum vcd_db_config    db_config;
	u32             slice_alpha_offset;
	u32             slice_beta_offset;
};

enum vcd_rate_control {
	VCD_RATE_CONTROL_OFF      = 0x1,
	VCD_RATE_CONTROL_VBR_VFR  = 0x2,
	VCD_RATE_CONTROL_VBR_CFR  = 0x3,
	VCD_RATE_CONTROL_CBR_VFR  = 0x4,
	VCD_RATE_CONTROL_CBR_CFR  = 0x5
};

struct vcd_property_rate_control {
	enum vcd_rate_control     rate_control;
};

struct vcd_property_qp_range {
	u32              max_qp;
	u32              min_qp;
};

struct vcd_property_session_qp {
	u32 i_frame_qp;
	u32 p_frame_qp;
	u32	b_frame_qp;
};

struct vcd_property_i_period {
	u32 p_frames;
	u32 b_frames;
};

struct vcd_property_vop_timing {
	u32   vop_time_resolution;
};

struct vcd_property_vop_timing_constant_delta {
	u32 constant_delta; /*In usecs */
};

struct vcd_property_short_header {
	u32             short_header;
};

struct vcd_property_intra_refresh_mb_number {
	u32            cir_mb_number;
};

struct vcd_property_req_i_frame {
	u32        req_i_frame;
};

struct vcd_frame_rect {
	u32   left;
	u32   top;
	u32   right;
	u32   bottom;
};

struct vcd_property_dec_output_buffer {
	struct vcd_frame_rect   disp_frm;
	struct vcd_property_frame_size frm_size;
};

enum vcd_output_order {
	VCD_DEC_ORDER_DISPLAY  = 0x0,
	VCD_DEC_ORDER_DECODE   = 0x1
};

struct vcd_property_enc_recon_buffer {
	u8 *user_virtual_addr;
	u8 *kernel_virtual_addr;
	u8 *physical_addr;
	u8 *dev_addr;
	u32 buffer_size;
	u32 ysize;
	int pmem_fd;
	u32 offset;
	void *client_data;
};

struct vcd_property_h264_mv_buffer {
	u8 *kernel_virtual_addr;
	u8 *physical_addr;
	u32 size;
	u32 count;
	int pmem_fd;
	u32 offset;
	u8 *dev_addr;
	void *client_data;
};

struct vcd_property_buffer_size {
	int width;
	int height;
	int size;
	int alignment;
};

struct vcd_property_sps_pps_for_idr_enable {
	u32 sps_pps_for_idr_enable_flag;
};

#endif
