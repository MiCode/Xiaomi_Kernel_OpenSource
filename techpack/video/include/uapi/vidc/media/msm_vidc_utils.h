/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#ifndef __MSM_VIDC_UTILS_H__
#define __MSM_VIDC_UTILS_H__

#include <linux/types.h>
#include <linux/v4l2-controls.h>

/* vendor color format start */
/* UBWC 8-bit Y/CbCr 4:2:0  */
#define V4L2_PIX_FMT_NV12_UBWC                  v4l2_fourcc('Q', '1', '2', '8')
/* NV12_512 8-bit Y/CbCr 4:2:0  */
#define V4L2_PIX_FMT_NV12_512                   v4l2_fourcc('Q', '5', '1', '2')
/* NV12_128 8-bit Y/CbCr 4:2:0  */
#define V4L2_PIX_FMT_NV12_128                   v4l2_fourcc('N', '1', '2', '8')
/* NV12 10-bit Y/CbCr 4:2:0 */
#define V4L2_PIX_FMT_NV12_P010_UBWC             v4l2_fourcc('Q', '1', '2', 'B')
/* UBWC 10-bit Y/CbCr 4:2:0 */
#define V4L2_PIX_FMT_NV12_TP10_UBWC             v4l2_fourcc('Q', '1', '2', 'A')
#define V4L2_PIX_FMT_RGBA8888_UBWC              v4l2_fourcc('Q', 'R', 'G', 'B')
#define V4L2_PIX_FMT_SDE_Y_CBCR_H2V2_P010_VENUS	\
	v4l2_fourcc('Q', 'P', '1', '0') /* Y/CbCr 4:2:0 P10 Venus*/
/* vendor color format end */

/* Vendor buffer flags start */
#define V4L2_BUF_FLAG_CODECCONFIG                   0x01000000
#define V4L2_BUF_FLAG_END_OF_SUBFRAME               0x02000000
#define V4L2_BUF_FLAG_DATA_CORRUPT                  0x04000000
#define V4L2_BUF_INPUT_UNSUPPORTED                  0x08000000
#define V4L2_BUF_FLAG_EOS                           0x10000000
#define V4L2_BUF_FLAG_READONLY                      0x20000000
#define V4L2_BUF_FLAG_PERF_MODE                     0x40000000
#define V4L2_BUF_FLAG_CVPMETADATA_SKIP              0x80000000
/* Vendor buffer flags end */

/* Vendor commands start */
#define V4L2_CMD_FLUSH                              (4)
/* flags for flush cmd */
#define V4L2_CMD_FLUSH_OUTPUT                       (1 << 0)
#define V4L2_CMD_FLUSH_CAPTURE                      (1 << 1)
/* Vendor commands end */

/* Vendor events start */
#define V4L2_EVENT_MSM_VIDC_START \
		(V4L2_EVENT_PRIVATE_START + 0x00001000)
#define V4L2_EVENT_MSM_VIDC_FLUSH_DONE \
		(V4L2_EVENT_MSM_VIDC_START + 1)
#define V4L2_EVENT_MSM_VIDC_PORT_SETTINGS_CHANGED_SUFFICIENT \
		(V4L2_EVENT_MSM_VIDC_START + 2)
#define V4L2_EVENT_MSM_VIDC_PORT_SETTINGS_CHANGED_INSUFFICIENT \
		(V4L2_EVENT_MSM_VIDC_START + 3)
#define V4L2_EVENT_MSM_VIDC_SYS_ERROR \
		(V4L2_EVENT_MSM_VIDC_START + 5)
#define V4L2_EVENT_MSM_VIDC_RELEASE_BUFFER_REFERENCE \
		(V4L2_EVENT_MSM_VIDC_START + 6)
#define V4L2_EVENT_MSM_VIDC_RELEASE_UNQUEUED_BUFFER \
		(V4L2_EVENT_MSM_VIDC_START + 7)
#define V4L2_EVENT_MSM_VIDC_HW_OVERLOAD \
		(V4L2_EVENT_MSM_VIDC_START + 8)
#define V4L2_EVENT_MSM_VIDC_MAX_CLIENTS \
		(V4L2_EVENT_MSM_VIDC_START + 9)
#define V4L2_EVENT_MSM_VIDC_HW_UNSUPPORTED \
		(V4L2_EVENT_MSM_VIDC_START + 10)
/* Vendor events end */

/* missing v4l2 entries start */
enum v4l2_mpeg_vidc_video_bitrate_mode {
	V4L2_MPEG_VIDEO_BITRATE_MODE_CBR_VFR =
		V4L2_MPEG_VIDEO_BITRATE_MODE_CBR + 1,
	V4L2_MPEG_VIDEO_BITRATE_MODE_MBR,
	V4L2_MPEG_VIDEO_BITRATE_MODE_MBR_VFR,
	V4L2_MPEG_VIDEO_BITRATE_MODE_CQ,
};
/* missing v4l2 entries end */

/* vendor controls start */
#define V4L2_CID_MPEG_MSM_VIDC_BASE             (V4L2_CTRL_CLASS_MPEG | 0x2000)

#define V4L2_MPEG_MSM_VIDC_DISABLE 0
#define V4L2_MPEG_MSM_VIDC_ENABLE 1

#define V4L2_CID_MPEG_VIDC_VIDEO_PICTYPE_DEC_MODE \
		(V4L2_CID_MPEG_MSM_VIDC_BASE+0)
enum v4l2_mpeg_vidc_video_pictype_dec_mode {
	V4L2_MPEG_VIDC_VIDEO_PICTYPE_DECODE_I = 1,
	V4L2_MPEG_VIDC_VIDEO_PICTYPE_DECODE_P = 2,
	V4L2_MPEG_VIDC_VIDEO_PICTYPE_DECODE_B = 4,
};

#define V4L2_CID_MPEG_VIDC_VIDEO_STREAM_FORMAT \
		(V4L2_CID_MPEG_MSM_VIDC_BASE+2)
enum v4l2_mpeg_vidc_video_stream_format {
	V4L2_MPEG_VIDC_VIDEO_NAL_FORMAT_STARTCODES         = 0,
	V4L2_MPEG_VIDC_VIDEO_NAL_FORMAT_FOUR_BYTE_LENGTH   = 4,
};

#define V4L2_CID_MPEG_VIDC_VIDEO_DECODE_ORDER \
		(V4L2_CID_MPEG_MSM_VIDC_BASE+3)
#define V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_RANDOM \
		(V4L2_CID_MPEG_MSM_VIDC_BASE+13)
#define V4L2_CID_MPEG_VIDC_VIDEO_AU_DELIMITER \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 14)
#define V4L2_CID_MPEG_VIDC_VIDEO_SYNC_FRAME_DECODE \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 15)
#define V4L2_CID_MPEG_VIDC_VIDEO_SECURE \
		(V4L2_CID_MPEG_MSM_VIDC_BASE+16)
#define V4L2_CID_MPEG_VIDC_VIDEO_EXTRADATA \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 17)
enum v4l2_mpeg_vidc_extradata {
	EXTRADATA_NONE = 0,
	EXTRADATA_DEFAULT = 1,
	EXTRADATA_ADVANCED = 2,
	EXTRADATA_ENC_INPUT_ROI = 4,
	EXTRADATA_ENC_INPUT_HDR10PLUS = 8,
	EXTRADATA_ENC_INPUT_CVP = 16,
	EXTRADATA_ENC_FRAME_QP = 32,
	EXTRADATA_ENC_INPUT_CROP = 64,
};
#define V4L2_CID_MPEG_VIDC_VIDEO_VUI_TIMING_INFO \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 19)
#define V4L2_CID_MPEG_VIDC_VIDEO_VP8_PROFILE_LEVEL \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 20)
enum v4l2_mpeg_vidc_video_vp8_profile_level {
	V4L2_MPEG_VIDC_VIDEO_VP8_UNUSED,
	V4L2_MPEG_VIDC_VIDEO_VP8_VERSION_0,
	V4L2_MPEG_VIDC_VIDEO_VP8_VERSION_1,
	V4L2_MPEG_VIDC_VIDEO_VP8_VERSION_2,
	V4L2_MPEG_VIDC_VIDEO_VP8_VERSION_3,
};
#define V4L2_CID_MPEG_VIDC_VIDEO_MPEG2_LEVEL \
		(V4L2_CID_MPEG_MSM_VIDC_BASE+23)
enum v4l2_mpeg_vidc_video_mpeg2_level {
	V4L2_MPEG_VIDC_VIDEO_MPEG2_LEVEL_0	= 0,
	V4L2_MPEG_VIDC_VIDEO_MPEG2_LEVEL_1	= 1,
	V4L2_MPEG_VIDC_VIDEO_MPEG2_LEVEL_2	= 2,
	V4L2_MPEG_VIDC_VIDEO_MPEG2_LEVEL_3	= 3,
};
#define V4L2_CID_MPEG_VIDC_VIDEO_MPEG2_PROFILE \
		(V4L2_CID_MPEG_MSM_VIDC_BASE+24)
enum v4l2_mpeg_vidc_video_mpeg2_profile {
	V4L2_MPEG_VIDC_VIDEO_MPEG2_PROFILE_SIMPLE		= 0,
	V4L2_MPEG_VIDC_VIDEO_MPEG2_PROFILE_MAIN			= 1,
	V4L2_MPEG_VIDC_VIDEO_MPEG2_PROFILE_HIGH			= 2,
};
#define V4L2_CID_MPEG_VIDC_VIDEO_LTRCOUNT \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 27)
#define V4L2_CID_MPEG_VIDC_VIDEO_USELTRFRAME \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 28)
#define V4L2_CID_MPEG_VIDC_VIDEO_MARKLTRFRAME \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 29)
#define V4L2_CID_MPEG_VIDC_VIDEO_VPX_ERROR_RESILIENCE \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 38)
#define V4L2_CID_MPEG_VIDC_VIDEO_BUFFER_SIZE_LIMIT \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 39)
#define V4L2_CID_MPEG_VIDC_VIDEO_BASELAYER_ID \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 47)
#define V4L2_CID_MPEG_VIDC_VIDEO_PRIORITY \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 52)
#define V4L2_CID_MPEG_VIDC_VIDEO_OPERATING_RATE \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 53)
#define V4L2_CID_MPEG_VIDC_VIDEO_VPE_CSC \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 55)
#define V4L2_CID_MPEG_VIDC_VIDEO_LOWLATENCY_MODE \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 56)
#define V4L2_CID_MPEG_VIDC_VIDEO_BLUR_DIMENSIONS \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 57)
#define V4L2_CID_MPEG_VIDC_VIDEO_COLOR_SPACE \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 60)
#define V4L2_CID_MPEG_VIDC_VIDEO_FULL_RANGE \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 61)
#define V4L2_CID_MPEG_VIDC_VIDEO_TRANSFER_CHARS \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 62)
#define V4L2_CID_MPEG_VIDC_VIDEO_MATRIX_COEFFS \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 63)
#define V4L2_CID_MPEG_VIDC_VIDEO_VP9_LEVEL \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 67)
enum v4l2_mpeg_vidc_video_vp9_level {
	V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_UNUSED = 0,
	V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_1 = 1,
	V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_11 = 2,
	V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_2 = 3,
	V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_21 = 4,
	V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_3 = 5,
	V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_31 = 6,
	V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_4 = 7,
	V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_41 = 8,
	V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_5 = 9,
	V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_51 = 10,
	V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_6 = 11,
	V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_61 = 12,
};
#define V4L2_CID_MPEG_VIDC_VIDEO_DYN_QP \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 108)
#define V4L2_CID_MPEG_VIDC_VIDEO_CONCEAL_COLOR_8BIT	\
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 109)
#define V4L2_CID_MPEG_VIDC_VIDEO_CONCEAL_COLOR_10BIT \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 110)
#define V4L2_CID_MPEG_VIDC_VIDEO_VPE_CSC_CUSTOM_MATRIX \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 114)
#define V4L2_CID_MPEG_VIDC_VENC_HDR_INFO \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 116)
#define V4L2_CID_MPEG_VIDC_IMG_GRID_SIZE \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 117)
#define V4L2_CID_MPEG_VIDC_COMPRESSION_QUALITY \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 118)
#define V4L2_CID_MPEG_VIDC_VIDEO_FRAME_RATE \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 119)
#define V4L2_CID_MPEG_VIDC_VIDEO_HEVC_MAX_HIER_CODING_LAYER \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 120)
enum v4l2_mpeg_vidc_video_hevc_max_hier_coding_layer {
	V4L2_MPEG_VIDC_VIDEO_HEVC_MAX_HIER_CODING_LAYER_0 = 0,
	V4L2_MPEG_VIDC_VIDEO_HEVC_MAX_HIER_CODING_LAYER_1 = 1,
	V4L2_MPEG_VIDC_VIDEO_HEVC_MAX_HIER_CODING_LAYER_2 = 2,
	V4L2_MPEG_VIDC_VIDEO_HEVC_MAX_HIER_CODING_LAYER_3 = 3,
	V4L2_MPEG_VIDC_VIDEO_HEVC_MAX_HIER_CODING_LAYER_4 = 4,
	V4L2_MPEG_VIDC_VIDEO_HEVC_MAX_HIER_CODING_LAYER_5 = 5,
	V4L2_MPEG_VIDC_VIDEO_HEVC_MAX_HIER_CODING_LAYER_6 = 6,
};
#define V4L2_CID_MPEG_VIDC_VENC_CVP_DISABLE \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 121)
#define V4L2_CID_MPEG_VIDC_VENC_NATIVE_RECORDER \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 122)
#define V4L2_CID_MPEG_VIDC_VENC_RC_TIMESTAMP_DISABLE \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 123)
#define V4L2_CID_MPEG_VIDC_SUPERFRAME \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 124)
#define V4L2_CID_MPEG_VIDC_CAPTURE_FRAME_RATE \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 125)
#define V4L2_CID_MPEG_VIDC_CVP_FRAME_RATE \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 126)
#define V4L2_CID_MPEG_VIDC_VIDEO_STREAM_OUTPUT_MODE \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 127)
enum v4l2_mpeg_vidc_video_stream_output_mode {
	V4L2_CID_MPEG_VIDC_VIDEO_STREAM_OUTPUT_PRIMARY = 0,
	V4L2_CID_MPEG_VIDC_VIDEO_STREAM_OUTPUT_SECONDARY = 1,
};
#define V4L2_CID_MPEG_VIDC_VIDEO_ROI_TYPE \
	(V4L2_CID_MPEG_MSM_VIDC_BASE + 128)
enum v4l2_mpeg_vidc_video_roi_type {
	V4L2_CID_MPEG_VIDC_VIDEO_ROI_TYPE_NONE = 0,
	V4L2_CID_MPEG_VIDC_VIDEO_ROI_TYPE_2BIT = 1,
	V4L2_CID_MPEG_VIDC_VIDEO_ROI_TYPE_2BYTE = 2,
};
#define V4L2_CID_MPEG_VIDC_VENC_BITRATE_SAVINGS \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 131)
enum v4l2_mpeg_vidc_video_bitrate_savings_type {
	V4L2_MPEG_VIDC_VIDEO_BRS_DISABLE = 0,
	V4L2_MPEG_VIDC_VIDEO_BRS_ENABLE_8BIT = 1,
	V4L2_MPEG_VIDC_VIDEO_BRS_ENABLE_10BIT = 2,
	V4L2_MPEG_VIDC_VIDEO_BRS_ENABLE_ALL = 3,
};
#define V4L2_CID_MPEG_VIDC_VENC_BITRATE_BOOST \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 132)
#define V4L2_CID_MPEG_VIDC_VIDEO_LOWLATENCY_HINT \
	(V4L2_CID_MPEG_MSM_VIDC_BASE + 133)
#define V4L2_CID_MPEG_VIDC_VDEC_HEIF_MODE \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 134)
#define V4L2_CID_MPEG_VIDC_VENC_QPRANGE_BOOST \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 135)
#define V4L2_CID_MPEG_VIDC_VIDEO_DISABLE_TIMESTAMP_REORDER \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 136)
#define V4L2_CID_MPEG_VIDC_VENC_COMPLEXITY \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 137)

#define V4L2_CID_MPEG_VIDC_VIDEO_UNKNOWN \
		(V4L2_CID_MPEG_MSM_VIDC_BASE + 0xFFF)
/* vendor controls end */

#define MSM_VIDC_EXTRADATA_NONE 0x00000000
struct msm_vidc_extradata_header {
	__u32 size;
	__u32 version; /** Keeping binary compatibility */
	__u32 port_index; /* with firmware and OpenMAX IL **/
	__u32 type; /* msm_vidc_extradata_type */
	__u32 data_size;
	__u32 data[1];
};

/* msm_vidc_interlace_type */
#define MSM_VIDC_INTERLACE_FRAME_PROGRESSIVE 0x01
#define MSM_VIDC_INTERLACE_INTERLEAVE_FRAME_TOPFIELDFIRST 0x02
#define MSM_VIDC_INTERLACE_INTERLEAVE_FRAME_BOTTOMFIELDFIRST 0x04
#define MSM_VIDC_INTERLACE_FRAME_TOPFIELDFIRST 0x08
#define MSM_VIDC_INTERLACE_FRAME_BOTTOMFIELDFIRST 0x10
#define MSM_VIDC_INTERLACE_FRAME_MBAFF 0x20
/* Color formats */
#define MSM_VIDC_HAL_INTERLACE_COLOR_FORMAT_NV12	0x2
#define MSM_VIDC_HAL_INTERLACE_COLOR_FORMAT_NV12_UBWC	0x8002
#define MSM_VIDC_EXTRADATA_INTERLACE_VIDEO 0x00000002
struct msm_vidc_interlace_payload {
	__u32 format; /* Interlace format */
	__u32 color_format;
};

#define MSM_VIDC_EXTRADATA_FRAME_RATE 0x00000007
struct msm_vidc_framerate_payload {
	__u32 frame_rate; /*In Q16 format */
};

#define MSM_VIDC_EXTRADATA_TIMESTAMP 0x00000005
struct msm_vidc_ts_payload {
	__u32 timestamp_lo;
	__u32 timestamp_hi;
};

#define MSM_VIDC_EXTRADATA_NUM_CONCEALED_MB 0x7F100001
struct msm_vidc_concealmb_payload {
	__u32 num_mbs;
};


#define MSM_VIDC_FRAME_RECONSTRUCTION_INCORRECT 0x0
#define MSM_VIDC_FRAME_RECONSTRUCTION_CORRECT 0x01
#define MSM_VIDC_FRAME_RECONSTRUCTION_APPROXIMATELY_CORRECT 0x02
#define MSM_VIDC_EXTRADATA_RECOVERY_POINT_SEI 0x00000009
struct msm_vidc_recoverysei_payload {
	__u32 flags;
};

#define MSM_VIDC_EXTRADATA_ASPECT_RATIO 0x7F100003
struct msm_vidc_aspect_ratio_payload {
	__u32 size;
	__u32 version;
	__u32 port_index;
	__u32 aspect_width;
	__u32 aspect_height;
};

#define MSM_VIDC_EXTRADATA_INPUT_CROP 0x0700000E
struct msm_vidc_input_crop_payload {
	__u32 size;
	__u32 version;
	__u32 port_index;
	__u32 left;
	__u32 top;
	__u32 width;
	__u32 height;
};

struct msm_vidc_misr_info {
	__u32 misr_set;
	__u32 misr_dpb_luma[8];
	__u32 misr_dpb_chroma[8];
	__u32 misr_opb_luma[8];
	__u32 misr_opb_chroma[8];
};
#define MSM_VIDC_EXTRADATA_OUTPUT_CROP 0x0700000F
struct msm_vidc_output_crop_payload {
	__u32 size;
	__u32 version;
	__u32 port_index;
	__u32 left;
	__u32 top;
	__u32 display_width;
	__u32 display_height;
	__u32 width;
	__u32 height;
	__u32 frame_num;
	__u32 bit_depth_y;
	__u32 bit_depth_c;
	struct msm_vidc_misr_info misr_info[2];
};

#define MSM_VIDC_EXTRADATA_INDEX 0x7F100002
struct msm_vidc_extradata_index {
	__u32 type;
	union {
		struct msm_vidc_input_crop_payload input_crop;
		struct msm_vidc_aspect_ratio_payload aspect_ratio;
	};
};

#define MSM_VIDC_EXTRADATA_PANSCAN_WINDOW 0x00000008
struct msm_vidc_panscan_window {
	__u32 panscan_height_offset;
	__u32 panscan_width_offset;
	__u32 panscan_window_width;
	__u32 panscan_window_height;
};

struct msm_vidc_panscan_window_payload {
	__u32 num_panscan_windows;
	struct msm_vidc_panscan_window wnd[1];
};

#define MSM_VIDC_USERDATA_TYPE_FRAME 0x1
#define MSM_VIDC_USERDATA_TYPE_TOP_FIELD 0x2
#define MSM_VIDC_USERDATA_TYPE_BOTTOM_FIELD 0x3
#define MSM_VIDC_EXTRADATA_STREAM_USERDATA 0x0000000E
struct msm_vidc_stream_userdata_payload {
	__u32 type;
	__u32 data[1];
};

#define MSM_VIDC_EXTRADATA_FRAME_QP 0x0000000F
struct msm_vidc_frame_qp_payload {
	__u32 frame_qp;
	__u32 qp_sum;
	__u32 skip_qp_sum;
	__u32 skip_num_blocks;
	__u32 total_num_blocks;
};

#define MSM_VIDC_EXTRADATA_FRAME_BITS_INFO 0x00000010
struct msm_vidc_frame_bits_info_payload {
	__u32 frame_bits;
	__u32 header_bits;
};

#define MSM_VIDC_EXTRADATA_S3D_FRAME_PACKING 0x00000006
struct msm_vidc_s3d_frame_packing_payload {
	__u32 fpa_id;
	__u32 cancel_flag;
	__u32 fpa_type;
	__u32 quin_cunx_flag;
	__u32 content_interprtation_type;
	__u32 spatial_flipping_flag;
	__u32 frame0_flipped_flag;
	__u32 field_views_flag;
	__u32 current_frame_is_frame0_flag;
	__u32 frame0_self_contained_flag;
	__u32 frame1_self_contained_flag;
	__u32 frame0_graid_pos_x;
	__u32 frame0_graid_pos_y;
	__u32 frame1_graid_pos_x;
	__u32 frame1_graid_pos_y;
	__u32 fpa_reserved_byte;
	__u32 fpa_repetition_period;
	__u32 fpa_extension_flag;
};

#define MSM_VIDC_EXTRADATA_ROI_QP 0x00000013
struct msm_vidc_roi_deltaqp_payload {
	__u32 b_roi_info; /*Enable/Disable*/
	__u32 mbi_info_size; /*Size of QP data*/
	__u32 data[1];
};

struct msm_vidc_roi_qp_payload {
	__s32 upper_qp_offset;
	__s32 lower_qp_offset;
	__u32 b_roi_info;
	__u32 mbi_info_size;
	__u32 data[1];
};

#define MSM_VIDC_EXTRADATA_MASTERING_DISPLAY_COLOUR_SEI 0x00000015
struct msm_vidc_mastering_display_colour_sei_payload {
	__u32 nDisplayPrimariesX[3];
	__u32 nDisplayPrimariesY[3];
	__u32 nWhitePointX;
	__u32 nWhitePointY;
	__u32 nMaxDisplayMasteringLuminance;
	__u32 nMinDisplayMasteringLuminance;
};

#define MSM_VIDC_EXTRADATA_CONTENT_LIGHT_LEVEL_SEI 0x00000016
struct msm_vidc_content_light_level_sei_payload {
	__u32 nMaxContentLight;
	__u32 nMaxPicAverageLight;
};

#define MSM_VIDC_EXTRADATA_HDR10PLUS_METADATA 0x0000001A
struct msm_vidc_hdr10plus_metadata_payload {
	__u32 size;
	__u32 data[1];
};

#define  MSM_VIDC_EXTRADATA_CVP_METADATA 0x0000001B
struct msm_vidc_enc_cvp_metadata_payload {
	__u32 data[256];
};

/* video_format */
#define MSM_VIDC_COMPONENT 0
#define MSM_VIDC_PAL 1
#define MSM_VIDC_NTSC 2
#define MSM_VIDC_SECAM 3
#define MSM_VIDC_MAC 4
#define MSM_VIDC_UNSPECIFIED_FORMAT 5
#define MSM_VIDC_RESERVED_1_FORMAT 6
#define MSM_VIDC_RESERVED_2_FORMAT 7

/* See colour_primaries of ISO/IEC 14496 for significance */
/* color_primaries values */
#define MSM_VIDC_RESERVED_1 0
#define MSM_VIDC_BT709_5 1
#define MSM_VIDC_UNSPECIFIED 2
#define MSM_VIDC_RESERVED_2 3
#define MSM_VIDC_BT470_6_M 4
#define MSM_VIDC_BT601_6_625 5
#define MSM_VIDC_BT470_6_BG MSM_VIDC_BT601_6_625
#define MSM_VIDC_BT601_6_525 6
#define MSM_VIDC_SMPTE_240M 7
#define MSM_VIDC_GENERIC_FILM 8
#define MSM_VIDC_BT2020 9

/* matrix_coeffs values */
#define MSM_VIDC_MATRIX_RGB 0
#define MSM_VIDC_MATRIX_BT_709_5 1
#define MSM_VIDC_MATRIX_UNSPECIFIED 2
#define MSM_VIDC_MATRIX_RESERVED 3
#define MSM_VIDC_MATRIX_FCC_47 4
#define MSM_VIDC_MATRIX_601_6_625 5
#define MSM_VIDC_MATRIX_BT470_BG MSM_VIDC_MATRIX_601_6_625
#define MSM_VIDC_MATRIX_601_6_525 6
#define MSM_VIDC_MATRIX_SMPTE_170M MSM_VIDC_MATRIX_601_6_525
#define MSM_VIDC_MATRIX_SMPTE_240M 7
#define MSM_VIDC_MATRIX_Y_CG_CO 8
#define MSM_VIDC_MATRIX_BT_2020 9
#define MSM_VIDC_MATRIX_BT_2020_CONST 10

/* transfer_char values */
#define MSM_VIDC_TRANSFER_RESERVED_1 0
#define MSM_VIDC_TRANSFER_BT709_5 1
#define MSM_VIDC_TRANSFER_UNSPECIFIED 2
#define MSM_VIDC_TRANSFER_RESERVED_2 3
#define MSM_VIDC_TRANSFER_BT_470_6_M 4
#define MSM_VIDC_TRANSFER_BT_470_6_BG 5
#define MSM_VIDC_TRANSFER_601_6_625 6
#define MSM_VIDC_TRANSFER_601_6_525 MSM_VIDC_TRANSFER_601_6_625
#define MSM_VIDC_TRANSFER_SMPTE_240M 7
#define MSM_VIDC_TRANSFER_LINEAR 8
#define MSM_VIDC_TRANSFER_LOG_100_1 9
#define MSM_VIDC_TRANSFER_LOG_100_SQRT10_1 10
#define MSM_VIDC_TRANSFER_IEC_61966 11
#define MSM_VIDC_TRANSFER_BT_1361 12
#define MSM_VIDC_TRANSFER_SRGB 13
#define MSM_VIDC_TRANSFER_BT_2020_10 14
#define MSM_VIDC_TRANSFER_BT_2020_12 15
#define MSM_VIDC_TRANSFER_SMPTE_ST2084 16
#define MSM_VIDC_TRANSFER_SMPTE_ST428_1 17
#define MSM_VIDC_TRANSFER_HLG 18

#define MSM_VIDC_EXTRADATA_VUI_DISPLAY_INFO 0x7F100006
struct msm_vidc_vui_display_info_payload {
	__u32 video_signal_present_flag;
	__u32 video_format;
	__u32 bit_depth_y;
	__u32 bit_depth_c;
	__u32 video_full_range_flag;
	__u32 color_description_present_flag;
	__u32 color_primaries;
	__u32 transfer_char;
	__u32 matrix_coeffs;
	__u32 chroma_location_info_present_flag;
	__u32 chroma_format_idc;
	__u32 separate_color_plane_flag;
	__u32 chroma_sample_loc_type_top_field;
	__u32 chroma_sample_loc_type_bottom_field;
};

#define  MSM_VIDC_EXTRADATA_HDR_HIST 0x7F100008
struct msm_vidc_extradata_hdr_hist_payload {
	__u32 value_count[1024];
};

#define MSM_VIDC_EXTRADATA_MPEG2_SEQDISP 0x0000000D
struct msm_vidc_mpeg2_seqdisp_payload {
	__u32 video_format;
	__u32 color_descp;
	__u32 color_primaries;
	__u32 transfer_char;
	__u32 matrix_coeffs;
	__u32 disp_width;
	__u32 disp_height;
};

/* VPx color_space values */
#define MSM_VIDC_CS_UNKNOWN 0
#define MSM_VIDC_CS_BT_601 1
#define MSM_VIDC_CS_BT_709 2
#define MSM_VIDC_CS_SMPTE_170 3
#define MSM_VIDC_CS_SMPTE_240 4
#define MSM_VIDC_CS_BT_2020 5
#define MSM_VIDC_CS_RESERVED 6
#define MSM_VIDC_CS_RGB 7
#define MSM_VIDC_EXTRADATA_VPX_COLORSPACE_INFO 0x00000014
struct msm_vidc_vpx_colorspace_payload {
	__u32 color_space;
	__u32 yuv_range_flag;
	__u32 sumsampling_x;
	__u32 sumsampling_y;
};

#define MSM_VIDC_EXTRADATA_METADATA_LTRINFO 0x7F100004
/* Don't use the #define below. It is to bypass checkpatch */
#define LTRINFO MSM_VIDC_EXTRADATA_METADATA_LTRINFO
struct msm_vidc_metadata_ltr_payload {
	__u32 ltr_use_mark;
};

/* ptr[2]: event_notify: pixel_depth */
#define MSM_VIDC_BIT_DEPTH_8 0
#define MSM_VIDC_BIT_DEPTH_10 1
#define MSM_VIDC_BIT_DEPTH_UNSUPPORTED 0XFFFFFFFF

/*  ptr[3]: event_notify: pic_struct */
#define MSM_VIDC_PIC_STRUCT_MAYBE_INTERLACED 0x0
#define MSM_VIDC_PIC_STRUCT_PROGRESSIVE 0x1

/*default when layer ID isn't specified*/
#define MSM_VIDC_ALL_LAYER_ID 0xFF

static inline unsigned int VENUS_EXTRADATA_SIZE(int width, int height)
{
	(void)height;
	(void)width;

	/*
	 * In the future, calculate the size based on the w/h but just
	 * hardcode it for now since 16K satisfies all current usecases.
	 */
	return 16 * 1024;
}

/* V4L2_CID_MPEG_VIDC_VENC_HDR_INFO payload index */
enum msm_vidc_hdr_info_types {
	MSM_VIDC_RGB_PRIMARY_00,
	MSM_VIDC_RGB_PRIMARY_01,
	MSM_VIDC_RGB_PRIMARY_10,
	MSM_VIDC_RGB_PRIMARY_11,
	MSM_VIDC_RGB_PRIMARY_20,
	MSM_VIDC_RGB_PRIMARY_21,
	MSM_VIDC_WHITEPOINT_X,
	MSM_VIDC_WHITEPOINT_Y,
	MSM_VIDC_MAX_DISP_LUM,
	MSM_VIDC_MIN_DISP_LUM,
	MSM_VIDC_RGB_MAX_CLL,
	MSM_VIDC_RGB_MAX_FLL,
};

enum msm_vidc_plane_reserved_field_types {
	MSM_VIDC_BUFFER_FD,
	MSM_VIDC_DATA_OFFSET,
	MSM_VIDC_COMP_RATIO,
	MSM_VIDC_INPUT_TAG_1,
	MSM_VIDC_INPUT_TAG_2,
	MSM_VIDC_FRAMERATE,
};

enum msm_vidc_cb_event_types {
	MSM_VIDC_HEIGHT,
	MSM_VIDC_WIDTH,
	MSM_VIDC_BIT_DEPTH,
	MSM_VIDC_PIC_STRUCT,
	MSM_VIDC_COLOR_SPACE,
	MSM_VIDC_FW_MIN_COUNT,
};
#endif
