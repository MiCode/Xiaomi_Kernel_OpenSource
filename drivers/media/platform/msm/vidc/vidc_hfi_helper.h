/* Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
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

#ifndef __H_VIDC_HFI_HELPER_H__
#define __H_VIDC_HFI_HELPER_H__

#define HFI_COMMON_BASE				(0)
#define HFI_OX_BASE					(0x01000000)

#define HFI_VIDEO_DOMAIN_ENCODER	(HFI_COMMON_BASE + 0x1)
#define HFI_VIDEO_DOMAIN_DECODER	(HFI_COMMON_BASE + 0x2)
#define HFI_VIDEO_DOMAIN_VPE		(HFI_COMMON_BASE + 0x4)
#define HFI_VIDEO_DOMAIN_CVP		(HFI_COMMON_BASE + 0x8)

#define HFI_DOMAIN_BASE_COMMON		(HFI_COMMON_BASE + 0)
#define HFI_DOMAIN_BASE_VDEC		(HFI_COMMON_BASE + 0x01000000)
#define HFI_DOMAIN_BASE_VENC		(HFI_COMMON_BASE + 0x02000000)
#define HFI_DOMAIN_BASE_VPE			(HFI_COMMON_BASE + 0x03000000)
#define HFI_DOMAIN_BASE_CVP			(HFI_COMMON_BASE + 0x04000000)

#define HFI_VIDEO_ARCH_OX			(HFI_COMMON_BASE + 0x1)

#define HFI_ARCH_COMMON_OFFSET		(0)
#define HFI_ARCH_OX_OFFSET			(0x00200000)

#define  HFI_CMD_START_OFFSET		(0x00010000)
#define  HFI_MSG_START_OFFSET		(0x00020000)

#define HFI_ERR_NONE						HFI_COMMON_BASE
#define HFI_ERR_SYS_FATAL				(HFI_COMMON_BASE + 0x1)
#define HFI_ERR_SYS_INVALID_PARAMETER		(HFI_COMMON_BASE + 0x2)
#define HFI_ERR_SYS_VERSION_MISMATCH		(HFI_COMMON_BASE + 0x3)
#define HFI_ERR_SYS_INSUFFICIENT_RESOURCES	(HFI_COMMON_BASE + 0x4)
#define HFI_ERR_SYS_MAX_SESSIONS_REACHED	(HFI_COMMON_BASE + 0x5)
#define HFI_ERR_SYS_UNSUPPORTED_CODEC		(HFI_COMMON_BASE + 0x6)
#define HFI_ERR_SYS_SESSION_IN_USE			(HFI_COMMON_BASE + 0x7)
#define HFI_ERR_SYS_SESSION_ID_OUT_OF_RANGE	(HFI_COMMON_BASE + 0x8)
#define HFI_ERR_SYS_UNSUPPORTED_DOMAIN		(HFI_COMMON_BASE + 0x9)
#define HFI_ERR_SYS_NOC_ERROR			(HFI_COMMON_BASE + 0x11)
#define HFI_ERR_SESSION_FATAL			(HFI_COMMON_BASE + 0x1001)
#define HFI_ERR_SESSION_INVALID_PARAMETER	(HFI_COMMON_BASE + 0x1002)
#define HFI_ERR_SESSION_BAD_POINTER		(HFI_COMMON_BASE + 0x1003)
#define HFI_ERR_SESSION_INVALID_SESSION_ID	(HFI_COMMON_BASE + 0x1004)
#define HFI_ERR_SESSION_INVALID_STREAM_ID	(HFI_COMMON_BASE + 0x1005)
#define HFI_ERR_SESSION_INCORRECT_STATE_OPERATION		\
	(HFI_COMMON_BASE + 0x1006)
#define HFI_ERR_SESSION_UNSUPPORTED_PROPERTY	(HFI_COMMON_BASE + 0x1007)

#define HFI_ERR_SESSION_UNSUPPORTED_SETTING	(HFI_COMMON_BASE + 0x1008)

#define HFI_ERR_SESSION_INSUFFICIENT_RESOURCES	(HFI_COMMON_BASE + 0x1009)

#define HFI_ERR_SESSION_STREAM_CORRUPT_OUTPUT_STALLED	\
	(HFI_COMMON_BASE + 0x100A)

#define HFI_ERR_SESSION_STREAM_CORRUPT		(HFI_COMMON_BASE + 0x100B)
#define HFI_ERR_SESSION_ENC_OVERFLOW		(HFI_COMMON_BASE + 0x100C)
#define HFI_ERR_SESSION_UNSUPPORTED_STREAM	(HFI_COMMON_BASE + 0x100D)
#define HFI_ERR_SESSION_CMDSIZE			(HFI_COMMON_BASE + 0x100E)
#define HFI_ERR_SESSION_UNSUPPORT_CMD		(HFI_COMMON_BASE + 0x100F)
#define HFI_ERR_SESSION_UNSUPPORT_BUFFERTYPE	(HFI_COMMON_BASE + 0x1010)
#define HFI_ERR_SESSION_BUFFERCOUNT_TOOSMALL	(HFI_COMMON_BASE + 0x1011)
#define HFI_ERR_SESSION_INVALID_SCALE_FACTOR	(HFI_COMMON_BASE + 0x1012)
#define HFI_ERR_SESSION_UPSCALE_NOT_SUPPORTED	(HFI_COMMON_BASE + 0x1013)

#define HFI_EVENT_SYS_ERROR				(HFI_COMMON_BASE + 0x1)
#define HFI_EVENT_SESSION_ERROR			(HFI_COMMON_BASE + 0x2)

#define HFI_VIDEO_CODEC_H264				0x00000002
#define HFI_VIDEO_CODEC_MPEG1				0x00000008
#define HFI_VIDEO_CODEC_MPEG2				0x00000010
#define HFI_VIDEO_CODEC_VP8				0x00001000
#define HFI_VIDEO_CODEC_HEVC				0x00002000
#define HFI_VIDEO_CODEC_VP9				0x00004000
#define HFI_VIDEO_CODEC_TME				0x00008000
#define HFI_VIDEO_CODEC_CVP				0x00010000

#define HFI_PROFILE_UNKNOWN				0x00000000
#define HFI_LEVEL_UNKNOWN				0x00000000

#define HFI_H264_PROFILE_BASELINE			0x00000001
#define HFI_H264_PROFILE_MAIN				0x00000002
#define HFI_H264_PROFILE_HIGH				0x00000004
#define HFI_H264_PROFILE_STEREO_HIGH			0x00000008
#define HFI_H264_PROFILE_MULTIVIEW_HIGH		0x00000010
#define HFI_H264_PROFILE_CONSTRAINED_BASE		0x00000020
#define HFI_H264_PROFILE_CONSTRAINED_HIGH		0x00000040

#define HFI_LEVEL_UNKNOWN					0x00000000
#define HFI_H264_LEVEL_1					0x00000001
#define HFI_H264_LEVEL_1b					0x00000002
#define HFI_H264_LEVEL_11					0x00000004
#define HFI_H264_LEVEL_12					0x00000008
#define HFI_H264_LEVEL_13					0x00000010
#define HFI_H264_LEVEL_2					0x00000020
#define HFI_H264_LEVEL_21					0x00000040
#define HFI_H264_LEVEL_22					0x00000080
#define HFI_H264_LEVEL_3					0x00000100
#define HFI_H264_LEVEL_31					0x00000200
#define HFI_H264_LEVEL_32					0x00000400
#define HFI_H264_LEVEL_4					0x00000800
#define HFI_H264_LEVEL_41					0x00001000
#define HFI_H264_LEVEL_42					0x00002000
#define HFI_H264_LEVEL_5					0x00004000
#define HFI_H264_LEVEL_51					0x00008000
#define HFI_H264_LEVEL_52					0x00010000
#define HFI_H264_LEVEL_6					0x00020000
#define HFI_H264_LEVEL_61					0x00040000
#define HFI_H264_LEVEL_62					0x00080000

#define HFI_MPEG2_PROFILE_SIMPLE			0x00000001
#define HFI_MPEG2_PROFILE_MAIN				0x00000002

#define HFI_MPEG2_LEVEL_LL					0x00000001
#define HFI_MPEG2_LEVEL_ML					0x00000002
#define HFI_MPEG2_LEVEL_HL					0x00000004

#define HFI_VP8_PROFILE_MAIN			0x00000001

#define HFI_VP8_LEVEL_VERSION_0			0x00000001
#define HFI_VP8_LEVEL_VERSION_1			0x00000002
#define HFI_VP8_LEVEL_VERSION_2			0x00000004
#define HFI_VP8_LEVEL_VERSION_3			0x00000008

#define  HFI_HEVC_PROFILE_MAIN			0x00000001
#define  HFI_HEVC_PROFILE_MAIN10		0x00000002
#define  HFI_HEVC_PROFILE_MAIN_STILL_PIC	0x00000004

#define  HFI_HEVC_LEVEL_1	0x00000001
#define  HFI_HEVC_LEVEL_2	0x00000002
#define  HFI_HEVC_LEVEL_21	0x00000004
#define  HFI_HEVC_LEVEL_3	0x00000008
#define  HFI_HEVC_LEVEL_31	0x00000010
#define  HFI_HEVC_LEVEL_4	0x00000020
#define  HFI_HEVC_LEVEL_41	0x00000040
#define  HFI_HEVC_LEVEL_5	0x00000080
#define  HFI_HEVC_LEVEL_51	0x00000100
#define  HFI_HEVC_LEVEL_52	0x00000200
#define  HFI_HEVC_LEVEL_6	0x00000400
#define  HFI_HEVC_LEVEL_61	0x00000800
#define  HFI_HEVC_LEVEL_62	0x00001000

#define HFI_HEVC_TIER_MAIN	0x1
#define HFI_HEVC_TIER_HIGH0	0x2

#define  HFI_TME_PROFILE_DEFAULT	0x00000001
#define  HFI_TME_PROFILE_FRC		0x00000002
#define  HFI_TME_PROFILE_ASW		0x00000004
#define  HFI_TME_PROFILE_DFS_BOKEH	0x00000008

#define HFI_TME_LEVEL_INTEGER		0x00000001

#define HFI_BUFFER_INPUT				(HFI_COMMON_BASE + 0x1)
#define HFI_BUFFER_OUTPUT				(HFI_COMMON_BASE + 0x2)
#define HFI_BUFFER_OUTPUT2				(HFI_COMMON_BASE + 0x3)
#define HFI_BUFFER_INTERNAL_PERSIST		(HFI_COMMON_BASE + 0x4)
#define HFI_BUFFER_INTERNAL_PERSIST_1		(HFI_COMMON_BASE + 0x5)
#define HFI_BUFFER_COMMON_INTERNAL_SCRATCH	(HFI_COMMON_BASE + 0x6)
#define HFI_BUFFER_COMMON_INTERNAL_SCRATCH_1	(HFI_COMMON_BASE + 0x7)
#define HFI_BUFFER_COMMON_INTERNAL_SCRATCH_2	(HFI_COMMON_BASE + 0x8)
#define HFI_BUFFER_COMMON_INTERNAL_RECON	(HFI_COMMON_BASE + 0x9)
#define HFI_BUFFER_EXTRADATA_OUTPUT		(HFI_COMMON_BASE + 0xA)
#define HFI_BUFFER_EXTRADATA_OUTPUT2		(HFI_COMMON_BASE + 0xB)
#define HFI_BUFFER_EXTRADATA_INPUT		(HFI_COMMON_BASE + 0xC)

#define  HFI_BITDEPTH_8				(HFI_COMMON_BASE + 0x0)
#define  HFI_BITDEPTH_9				(HFI_COMMON_BASE + 0x1)
#define  HFI_BITDEPTH_10			(HFI_COMMON_BASE + 0x2)

#define HFI_VENC_PERFMODE_MAX_QUALITY	0x1
#define HFI_VENC_PERFMODE_POWER_SAVE	0x2

#define  HFI_WORKMODE_1		(HFI_COMMON_BASE + 0x1)
#define  HFI_WORKMODE_2		(HFI_COMMON_BASE + 0x2)

struct hfi_buffer_info {
	u32 buffer_addr;
	u32 extra_data_addr;
};

#define HFI_PROPERTY_SYS_COMMON_START		\
	(HFI_DOMAIN_BASE_COMMON + HFI_ARCH_COMMON_OFFSET + 0x0000)
#define HFI_PROPERTY_SYS_DEBUG_CONFIG		\
	(HFI_PROPERTY_SYS_COMMON_START + 0x001)
#define HFI_PROPERTY_SYS_RESOURCE_OCMEM_REQUIREMENT_INFO	\
	(HFI_PROPERTY_SYS_COMMON_START + 0x002)
#define HFI_PROPERTY_SYS_CONFIG_VCODEC_CLKFREQ				\
	(HFI_PROPERTY_SYS_COMMON_START + 0x003)
#define HFI_PROPERTY_SYS_IDLE_INDICATOR         \
	(HFI_PROPERTY_SYS_COMMON_START + 0x004)
#define  HFI_PROPERTY_SYS_CODEC_POWER_PLANE_CTRL     \
	(HFI_PROPERTY_SYS_COMMON_START + 0x005)
#define  HFI_PROPERTY_SYS_IMAGE_VERSION    \
	(HFI_PROPERTY_SYS_COMMON_START + 0x006)
#define  HFI_PROPERTY_SYS_CONFIG_COVERAGE    \
	(HFI_PROPERTY_SYS_COMMON_START + 0x007)
#define  HFI_PROPERTY_SYS_UBWC_CONFIG    \
	(HFI_PROPERTY_SYS_COMMON_START + 0x008)

#define HFI_PROPERTY_PARAM_COMMON_START	\
	(HFI_DOMAIN_BASE_COMMON + HFI_ARCH_COMMON_OFFSET + 0x1000)
#define HFI_PROPERTY_PARAM_FRAME_SIZE		\
	(HFI_PROPERTY_PARAM_COMMON_START + 0x001)
#define HFI_PROPERTY_PARAM_UNCOMPRESSED_PLANE_ACTUAL_INFO	\
	(HFI_PROPERTY_PARAM_COMMON_START + 0x002)
#define HFI_PROPERTY_PARAM_UNCOMPRESSED_FORMAT_SELECT		\
	(HFI_PROPERTY_PARAM_COMMON_START + 0x003)
#define HFI_PROPERTY_PARAM_UNCOMPRESSED_FORMAT_SUPPORTED	\
	(HFI_PROPERTY_PARAM_COMMON_START + 0x004)
#define HFI_PROPERTY_PARAM_PROFILE_LEVEL_CURRENT			\
	(HFI_PROPERTY_PARAM_COMMON_START + 0x005)
#define HFI_PROPERTY_PARAM_PROFILE_LEVEL_SUPPORTED			\
	(HFI_PROPERTY_PARAM_COMMON_START + 0x006)
#define HFI_PROPERTY_PARAM_CAPABILITY_SUPPORTED				\
	(HFI_PROPERTY_PARAM_COMMON_START + 0x007)
#define HFI_PROPERTY_PARAM_PROPERTIES_SUPPORTED				\
	(HFI_PROPERTY_PARAM_COMMON_START + 0x008)
#define HFI_PROPERTY_PARAM_CODEC_SUPPORTED			\
	(HFI_PROPERTY_PARAM_COMMON_START + 0x009)
#define HFI_PROPERTY_PARAM_NAL_STREAM_FORMAT_SUPPORTED		\
	(HFI_PROPERTY_PARAM_COMMON_START + 0x00A)
#define HFI_PROPERTY_PARAM_NAL_STREAM_FORMAT_SELECT			\
	(HFI_PROPERTY_PARAM_COMMON_START + 0x00B)
#define HFI_PROPERTY_PARAM_MULTI_VIEW_FORMAT				\
	(HFI_PROPERTY_PARAM_COMMON_START + 0x00C)
#define  HFI_PROPERTY_PARAM_CODEC_MASK_SUPPORTED            \
	(HFI_PROPERTY_PARAM_COMMON_START + 0x00E)
#define  HFI_PROPERTY_PARAM_MAX_SESSIONS_SUPPORTED	    \
	(HFI_PROPERTY_PARAM_COMMON_START + 0x010)
#define  HFI_PROPERTY_PARAM_SECURE_SESSION		\
	(HFI_PROPERTY_PARAM_COMMON_START + 0x011)
#define  HFI_PROPERTY_PARAM_WORK_MODE                       \
	(HFI_PROPERTY_PARAM_COMMON_START + 0x015)
#define  HFI_PROPERTY_TME_VERSION_SUPPORTED                 \
	(HFI_PROPERTY_PARAM_COMMON_START + 0x016)
#define  HFI_PROPERTY_PARAM_WORK_ROUTE                 \
	(HFI_PROPERTY_PARAM_COMMON_START + 0x017)

#define HFI_PROPERTY_CONFIG_COMMON_START				\
	(HFI_DOMAIN_BASE_COMMON + HFI_ARCH_COMMON_OFFSET + 0x2000)
#define HFI_PROPERTY_CONFIG_FRAME_RATE					\
	(HFI_PROPERTY_CONFIG_COMMON_START + 0x001)
#define HFI_PROPERTY_CONFIG_VIDEOCORES_USAGE				\
	(HFI_PROPERTY_CONFIG_COMMON_START + 0x002)
#define HFI_PROPERTY_CONFIG_OPERATING_RATE				\
	(HFI_PROPERTY_CONFIG_COMMON_START + 0x003)

#define HFI_PROPERTY_PARAM_VDEC_COMMON_START				\
	(HFI_DOMAIN_BASE_VDEC + HFI_ARCH_COMMON_OFFSET + 0x3000)
#define HFI_PROPERTY_PARAM_VDEC_MULTI_STREAM				\
	(HFI_PROPERTY_PARAM_VDEC_COMMON_START + 0x001)
#define HFI_PROPERTY_PARAM_VDEC_CONCEAL_COLOR				\
	(HFI_PROPERTY_PARAM_VDEC_COMMON_START + 0x002)
#define  HFI_PROPERTY_PARAM_VDEC_PIXEL_BITDEPTH				\
	(HFI_PROPERTY_PARAM_VDEC_COMMON_START + 0x007)
#define  HFI_PROPERTY_PARAM_VDEC_PIC_STRUCT				\
	(HFI_PROPERTY_PARAM_VDEC_COMMON_START + 0x009)
#define  HFI_PROPERTY_PARAM_VDEC_COLOUR_SPACE				\
	(HFI_PROPERTY_PARAM_VDEC_COMMON_START + 0x00A)
#define  HFI_PROPERTY_PARAM_VDEC_DPB_COUNTS				\
	(HFI_PROPERTY_PARAM_VDEC_COMMON_START + 0x00B)


#define HFI_PROPERTY_CONFIG_VDEC_COMMON_START				\
	(HFI_DOMAIN_BASE_VDEC + HFI_ARCH_COMMON_OFFSET + 0x4000)

#define HFI_PROPERTY_PARAM_VENC_COMMON_START				\
	(HFI_DOMAIN_BASE_VENC + HFI_ARCH_COMMON_OFFSET + 0x5000)
#define HFI_PROPERTY_PARAM_VENC_SLICE_DELIVERY_MODE			\
	(HFI_PROPERTY_PARAM_VENC_COMMON_START + 0x001)
#define HFI_PROPERTY_PARAM_VENC_H264_ENTROPY_CONTROL		\
	(HFI_PROPERTY_PARAM_VENC_COMMON_START + 0x002)
#define HFI_PROPERTY_PARAM_VENC_H264_DEBLOCK_CONTROL		\
	(HFI_PROPERTY_PARAM_VENC_COMMON_START + 0x003)
#define HFI_PROPERTY_PARAM_VENC_RATE_CONTROL				\
	(HFI_PROPERTY_PARAM_VENC_COMMON_START + 0x004)
#define  HFI_PROPERTY_PARAM_VENC_SESSION_QP_RANGE		\
	(HFI_PROPERTY_PARAM_VENC_COMMON_START + 0x009)
#define  HFI_PROPERTY_PARAM_VENC_OPEN_GOP                   \
	(HFI_PROPERTY_PARAM_VENC_COMMON_START + 0x00C)
#define HFI_PROPERTY_PARAM_VENC_INTRA_REFRESH				\
	(HFI_PROPERTY_PARAM_VENC_COMMON_START + 0x00D)
#define HFI_PROPERTY_PARAM_VENC_MULTI_SLICE_CONTROL			\
	(HFI_PROPERTY_PARAM_VENC_COMMON_START + 0x00E)
#define  HFI_PROPERTY_PARAM_VENC_VBV_HRD_BUF_SIZE           \
	(HFI_PROPERTY_PARAM_VENC_COMMON_START + 0x00F)
#define  HFI_PROPERTY_PARAM_VENC_QUALITY_VS_SPEED           \
	(HFI_PROPERTY_PARAM_VENC_COMMON_START + 0x010)
#define  HFI_PROPERTY_PARAM_VENC_H264_SPS_ID                \
	(HFI_PROPERTY_PARAM_VENC_COMMON_START + 0x014)
#define  HFI_PROPERTY_PARAM_VENC_H264_PPS_ID               \
	(HFI_PROPERTY_PARAM_VENC_COMMON_START + 0x015)
#define HFI_PROPERTY_PARAM_VENC_GENERATE_AUDNAL	\
	(HFI_PROPERTY_PARAM_VENC_COMMON_START + 0x016)
#define HFI_PROPERTY_PARAM_VENC_ASPECT_RATIO			\
	(HFI_PROPERTY_PARAM_VENC_COMMON_START + 0x017)
#define HFI_PROPERTY_PARAM_VENC_NUMREF					\
	(HFI_PROPERTY_PARAM_VENC_COMMON_START + 0x018)
#define HFI_PROPERTY_PARAM_VENC_LTRMODE		\
	 (HFI_PROPERTY_PARAM_VENC_COMMON_START + 0x01C)
#define HFI_PROPERTY_PARAM_VENC_VIDEO_SIGNAL_INFO	\
	(HFI_PROPERTY_PARAM_VENC_COMMON_START + 0x01D)
#define HFI_PROPERTY_PARAM_VENC_VUI_TIMING_INFO	\
	(HFI_PROPERTY_PARAM_VENC_COMMON_START + 0x01E)
#define HFI_PROPERTY_PARAM_VENC_LOW_LATENCY_MODE	\
	(HFI_PROPERTY_PARAM_VENC_COMMON_START + 0x022)
#define HFI_PROPERTY_PARAM_VENC_PRESERVE_TEXT_QUALITY \
	(HFI_PROPERTY_PARAM_VENC_COMMON_START + 0x023)
#define HFI_PROPERTY_PARAM_VENC_H264_8X8_TRANSFORM \
	(HFI_PROPERTY_PARAM_VENC_COMMON_START + 0x025)
#define HFI_PROPERTY_PARAM_VENC_HIER_P_MAX_NUM_ENH_LAYER	\
	(HFI_PROPERTY_PARAM_VENC_COMMON_START + 0x026)
#define HFI_PROPERTY_PARAM_VENC_DISABLE_RC_TIMESTAMP \
	(HFI_PROPERTY_PARAM_VENC_COMMON_START + 0x027)
#define HFI_PROPERTY_PARAM_VENC_VPX_ERROR_RESILIENCE_MODE	\
	(HFI_PROPERTY_PARAM_VENC_COMMON_START + 0x029)
#define HFI_PROPERTY_PARAM_VENC_HIER_B_MAX_NUM_ENH_LAYER	\
	(HFI_PROPERTY_PARAM_VENC_COMMON_START + 0x02C)
#define  HFI_PROPERTY_PARAM_VENC_HIER_P_HYBRID_MODE	\
	(HFI_PROPERTY_PARAM_VENC_COMMON_START + 0x02F)
#define  HFI_PROPERTY_PARAM_VENC_BITRATE_TYPE		\
	(HFI_PROPERTY_PARAM_VENC_COMMON_START + 0x031)
#define  HFI_PROPERTY_PARAM_VENC_IFRAMESIZE			\
	(HFI_PROPERTY_PARAM_VENC_COMMON_START + 0x034)
#define  HFI_PROPERTY_PARAM_VENC_SEND_OUTPUT_FOR_SKIPPED_FRAMES	\
	(HFI_PROPERTY_PARAM_VENC_COMMON_START + 0x035)
#define  HFI_PROPERTY_PARAM_VENC_HDR10_PQ_SEI			\
	(HFI_PROPERTY_PARAM_VENC_COMMON_START + 0x036)
#define  HFI_PROPERTY_PARAM_VENC_ADAPTIVE_B \
	(HFI_PROPERTY_PARAM_VENC_COMMON_START + 0x037)
#define  HFI_PROPERTY_PARAM_VENC_BITRATE_SAVINGS \
	(HFI_PROPERTY_PARAM_VENC_COMMON_START + 0x038)

#define HFI_PROPERTY_CONFIG_VENC_COMMON_START				\
	(HFI_DOMAIN_BASE_VENC + HFI_ARCH_COMMON_OFFSET + 0x6000)
#define HFI_PROPERTY_CONFIG_VENC_TARGET_BITRATE				\
	(HFI_PROPERTY_CONFIG_VENC_COMMON_START + 0x001)
#define HFI_PROPERTY_CONFIG_VENC_IDR_PERIOD				\
	(HFI_PROPERTY_CONFIG_VENC_COMMON_START + 0x002)
#define HFI_PROPERTY_CONFIG_VENC_INTRA_PERIOD				\
	(HFI_PROPERTY_CONFIG_VENC_COMMON_START + 0x003)
#define HFI_PROPERTY_CONFIG_VENC_REQUEST_SYNC_FRAME			\
	(HFI_PROPERTY_CONFIG_VENC_COMMON_START + 0x004)
#define  HFI_PROPERTY_CONFIG_VENC_SLICE_SIZE                \
	(HFI_PROPERTY_CONFIG_VENC_COMMON_START + 0x005)
#define  HFI_PROPERTY_CONFIG_VENC_SYNC_FRAME_SEQUENCE_HEADER	\
	(HFI_PROPERTY_CONFIG_VENC_COMMON_START + 0x008)
#define  HFI_PROPERTY_CONFIG_VENC_MARKLTRFRAME			\
	(HFI_PROPERTY_CONFIG_VENC_COMMON_START + 0x009)
#define  HFI_PROPERTY_CONFIG_VENC_USELTRFRAME			\
	(HFI_PROPERTY_CONFIG_VENC_COMMON_START + 0x00A)
#define  HFI_PROPERTY_CONFIG_VENC_HIER_P_ENH_LAYER		\
	(HFI_PROPERTY_CONFIG_VENC_COMMON_START + 0x00B)
#define  HFI_PROPERTY_CONFIG_VENC_VBV_HRD_BUF_SIZE		\
	(HFI_PROPERTY_CONFIG_VENC_COMMON_START + 0x00D)
#define  HFI_PROPERTY_CONFIG_VENC_PERF_MODE			\
	(HFI_PROPERTY_CONFIG_VENC_COMMON_START + 0x00E)
#define HFI_PROPERTY_CONFIG_VENC_BASELAYER_PRIORITYID		\
	(HFI_PROPERTY_CONFIG_VENC_COMMON_START + 0x00F)
#define HFI_PROPERTY_CONFIG_VENC_BLUR_FRAME_SIZE		\
	(HFI_PROPERTY_CONFIG_VENC_COMMON_START + 0x010)
#define HFI_PROPERTY_CONFIG_VENC_FRAME_QP			\
	(HFI_PROPERTY_CONFIG_VENC_COMMON_START + 0x012)
#define HFI_PROPERTY_CONFIG_HEIC_FRAME_CROP_INFO		\
	(HFI_PROPERTY_CONFIG_VENC_COMMON_START + 0x013)
#define HFI_PROPERTY_CONFIG_HEIC_FRAME_QUALITY			\
	(HFI_PROPERTY_CONFIG_VENC_COMMON_START + 0x014)
#define HFI_PROPERTY_CONFIG_HEIC_GRID_ENABLE			\
	(HFI_PROPERTY_CONFIG_VENC_COMMON_START + 0x015)

#define HFI_PROPERTY_PARAM_VPE_COMMON_START				\
	(HFI_DOMAIN_BASE_VPE + HFI_ARCH_COMMON_OFFSET + 0x7000)
#define HFI_PROPERTY_PARAM_VPE_ROTATION				\
	(HFI_PROPERTY_PARAM_VPE_COMMON_START + 0x001)
#define HFI_PROPERTY_PARAM_VPE_COLOR_SPACE_CONVERSION		\
	(HFI_PROPERTY_PARAM_VPE_COMMON_START + 0x002)

#define HFI_PROPERTY_CONFIG_VPE_COMMON_START				\
	(HFI_DOMAIN_BASE_VPE + HFI_ARCH_COMMON_OFFSET + 0x8000)

struct hfi_pic_struct {
	u32 progressive_only;
};

struct hfi_bitrate {
	u32 bit_rate;
	u32 layer_id;
};

struct hfi_colour_space {
	u32 colour_space;
};

#define HFI_CAPABILITY_FRAME_WIDTH			(HFI_COMMON_BASE + 0x1)
#define HFI_CAPABILITY_FRAME_HEIGHT			(HFI_COMMON_BASE + 0x2)
#define HFI_CAPABILITY_MBS_PER_FRAME			(HFI_COMMON_BASE + 0x3)
#define HFI_CAPABILITY_MBS_PER_SECOND			(HFI_COMMON_BASE + 0x4)
#define HFI_CAPABILITY_FRAMERATE			(HFI_COMMON_BASE + 0x5)
#define HFI_CAPABILITY_SCALE_X				(HFI_COMMON_BASE + 0x6)
#define HFI_CAPABILITY_SCALE_Y				(HFI_COMMON_BASE + 0x7)
#define HFI_CAPABILITY_BITRATE				(HFI_COMMON_BASE + 0x8)
#define HFI_CAPABILITY_BFRAME				(HFI_COMMON_BASE + 0x9)
#define HFI_CAPABILITY_PEAKBITRATE			(HFI_COMMON_BASE + 0xa)
#define HFI_CAPABILITY_HIER_P_NUM_ENH_LAYERS		(HFI_COMMON_BASE + 0x10)
#define HFI_CAPABILITY_ENC_LTR_COUNT			(HFI_COMMON_BASE + 0x11)
#define HFI_CAPABILITY_CP_OUTPUT2_THRESH		(HFI_COMMON_BASE + 0x12)
#define HFI_CAPABILITY_HIER_B_NUM_ENH_LAYERS	(HFI_COMMON_BASE + 0x13)
#define HFI_CAPABILITY_LCU_SIZE				(HFI_COMMON_BASE + 0x14)
#define HFI_CAPABILITY_HIER_P_HYBRID_NUM_ENH_LAYERS	(HFI_COMMON_BASE + 0x15)
#define HFI_CAPABILITY_MBS_PER_SECOND_POWERSAVE		(HFI_COMMON_BASE + 0x16)
#define HFI_CAPABILITY_EXTRADATA			(HFI_COMMON_BASE + 0X17)
#define HFI_CAPABILITY_PROFILE				(HFI_COMMON_BASE + 0X18)
#define HFI_CAPABILITY_LEVEL				(HFI_COMMON_BASE + 0X19)
#define HFI_CAPABILITY_I_FRAME_QP			(HFI_COMMON_BASE + 0X20)
#define HFI_CAPABILITY_P_FRAME_QP			(HFI_COMMON_BASE + 0X21)
#define HFI_CAPABILITY_B_FRAME_QP			(HFI_COMMON_BASE + 0X22)
#define HFI_CAPABILITY_RATE_CONTROL_MODES		(HFI_COMMON_BASE + 0X23)
#define HFI_CAPABILITY_BLUR_WIDTH			(HFI_COMMON_BASE + 0X24)
#define HFI_CAPABILITY_BLUR_HEIGHT			(HFI_COMMON_BASE + 0X25)
#define HFI_CAPABILITY_SLICE_DELIVERY_MODES		(HFI_COMMON_BASE + 0X26)
#define HFI_CAPABILITY_SLICE_BYTE			(HFI_COMMON_BASE + 0X27)
#define HFI_CAPABILITY_SLICE_MB				(HFI_COMMON_BASE + 0X28)
#define HFI_CAPABILITY_SECURE				(HFI_COMMON_BASE + 0X29)
#define HFI_CAPABILITY_MAX_NUM_B_FRAMES			(HFI_COMMON_BASE + 0X2A)
#define HFI_CAPABILITY_MAX_VIDEOCORES			(HFI_COMMON_BASE + 0X2B)
#define HFI_CAPABILITY_MAX_WORKMODES			(HFI_COMMON_BASE + 0X2C)
#define HFI_CAPABILITY_UBWC_CR_STATS			(HFI_COMMON_BASE + 0X2D)
#define HFI_CAPABILITY_ROTATION				(HFI_COMMON_BASE + 0X2F)
#define HFI_CAPABILITY_COLOR_SPACE_CONVERSION		(HFI_COMMON_BASE + 0X30)
#define HFI_CAPABILITY_MAX_WORKROUTES			(HFI_COMMON_BASE + 0X31)
#define HFI_CAPABILITY_CQ_QUALITY_LEVEL			(HFI_COMMON_BASE + 0X32)

struct hfi_capability_supported {
	u32 capability_type;
	u32 min;
	u32 max;
	u32 step_size;
};

struct hfi_capability_supported_info {
	u32 num_capabilities;
	struct hfi_capability_supported rg_data[1];
};

#define HFI_DEBUG_MSG_LOW					0x00000001
#define HFI_DEBUG_MSG_MEDIUM					0x00000002
#define HFI_DEBUG_MSG_HIGH					0x00000004
#define HFI_DEBUG_MSG_ERROR					0x00000008
#define HFI_DEBUG_MSG_FATAL					0x00000010
#define HFI_DEBUG_MSG_PERF					0x00000020

#define HFI_DEBUG_MODE_QUEUE					0x00000001
#define HFI_DEBUG_MODE_QDSS					0x00000002

struct hfi_debug_config {
	u32 debug_config;
	u32 debug_mode;
};

struct hfi_enable {
	u32 enable;
};

#define HFI_H264_DB_MODE_DISABLE			(HFI_COMMON_BASE + 0x1)
#define HFI_H264_DB_MODE_SKIP_SLICE_BOUNDARY	\
	(HFI_COMMON_BASE + 0x2)
#define HFI_H264_DB_MODE_ALL_BOUNDARY		(HFI_COMMON_BASE + 0x3)

struct hfi_h264_db_control {
	u32 mode;
	u32 slice_alpha_offset;
	u32 slice_beta_offset;
};

#define HFI_H264_ENTROPY_CAVLC				(HFI_COMMON_BASE + 0x1)
#define HFI_H264_ENTROPY_CABAC				(HFI_COMMON_BASE + 0x2)

#define HFI_H264_CABAC_MODEL_0				(HFI_COMMON_BASE + 0x1)
#define HFI_H264_CABAC_MODEL_1				(HFI_COMMON_BASE + 0x2)
#define HFI_H264_CABAC_MODEL_2				(HFI_COMMON_BASE + 0x3)

struct hfi_h264_entropy_control {
	u32 entropy_mode;
	u32 cabac_model;
};

struct hfi_frame_rate {
	u32 buffer_type;
	u32 frame_rate;
};

struct hfi_heic_frame_quality {
	u32 frame_quality;
	u32 reserved[3];
};

struct hfi_heic_grid_enable {
	u32 grid_enable;
};

struct hfi_operating_rate {
	u32 operating_rate;
};

#define HFI_INTRA_REFRESH_NONE				(HFI_COMMON_BASE + 0x1)
#define HFI_INTRA_REFRESH_CYCLIC			(HFI_COMMON_BASE + 0x2)
#define HFI_INTRA_REFRESH_RANDOM			(HFI_COMMON_BASE + 0x5)

struct hfi_intra_refresh {
	u32 mode;
	u32 mbs;
};

struct hfi_idr_period {
	u32 idr_period;
};

struct hfi_vpe_rotation_type {
	u32 rotation;
	u32 flip;
};

struct hfi_conceal_color {
	u32 conceal_color_8bit;
	u32 conceal_color_10bit;
};

struct hfi_intra_period {
	u32 pframes;
	u32 bframes;
};

struct hfi_multi_stream {
	u32 buffer_type;
	u32 enable;
};

struct hfi_multi_view_format {
	u32 views;
	u32 rg_view_order[1];
};

#define HFI_MULTI_SLICE_OFF				(HFI_COMMON_BASE + 0x1)
#define HFI_MULTI_SLICE_BY_MB_COUNT		(HFI_COMMON_BASE + 0x2)
#define HFI_MULTI_SLICE_BY_BYTE_COUNT	(HFI_COMMON_BASE + 0x3)

struct hfi_multi_slice_control {
	u32 multi_slice;
	u32 slice_size;
};

#define HFI_NAL_FORMAT_STARTCODES			0x00000001
#define HFI_NAL_FORMAT_ONE_NAL_PER_BUFFER	0x00000002
#define HFI_NAL_FORMAT_ONE_BYTE_LENGTH		0x00000004
#define HFI_NAL_FORMAT_TWO_BYTE_LENGTH		0x00000008
#define HFI_NAL_FORMAT_FOUR_BYTE_LENGTH		0x00000010

struct hfi_nal_stream_format_supported {
	u32 nal_stream_format_supported;
};

struct hfi_nal_stream_format_select {
	u32 nal_stream_format_select;
};
#define HFI_PICTURE_TYPE_I					0x01
#define HFI_PICTURE_TYPE_P					0x02
#define HFI_PICTURE_TYPE_B					0x04
#define HFI_PICTURE_TYPE_IDR					0x08
#define HFI_PICTURE_TYPE_CRA					0x10

struct hfi_profile_level {
	u32 profile;
	u32 level;
};

struct hfi_dpb_counts {
	u32 max_dpb_count;
	u32 max_ref_count;
	u32 max_dec_buffering;
};

struct hfi_profile_level_supported {
	u32 profile_count;
	struct hfi_profile_level rg_profile_level[1];
};

struct hfi_quality_vs_speed {
	u32 quality_vs_speed;
};

struct hfi_quantization {
	u32 qp_packed;
	u32 layer_id;
	u32 enable;
	u32 reserved[3];
};

struct hfi_quantization_range {
	struct hfi_quantization min_qp;
	struct hfi_quantization max_qp;
	u32 reserved[4];
};

#define HFI_LTR_MODE_DISABLE	0x0
#define HFI_LTR_MODE_MANUAL		0x1

struct hfi_ltr_mode {
	u32 ltr_mode;
	u32 ltr_count;
	u32 trust_mode;
};

struct hfi_ltr_use {
	u32 ref_ltr;
	u32 use_constrnt;
	u32 frames;
};

struct hfi_ltr_mark {
	u32 mark_frame;
};

struct hfi_frame_size {
	u32 buffer_type;
	u32 width;
	u32 height;
};

struct hfi_videocores_usage_type {
	u32 video_core_enable_mask;
};

struct hfi_video_work_mode {
	u32 video_work_mode;
};

struct hfi_video_work_route {
	u32 video_work_route;
};

struct hfi_video_signal_metadata {
	u32 enable;
	u32 video_format;
	u32 video_full_range;
	u32 color_description;
	u32 color_primaries;
	u32 transfer_characteristics;
	u32 matrix_coeffs;
};

struct hfi_vui_timing_info {
	u32 enable;
	u32 fixed_frame_rate;
	u32 time_scale;
};

struct hfi_bit_depth {
	u32 buffer_type;
	u32 bit_depth;
};

struct hfi_picture_type {
	u32 is_sync_frame;
	u32 picture_type;
};

/* Base Offset for UBWC color formats  */
#define HFI_COLOR_FORMAT_UBWC_BASE        (0x8000)
/* Base Offset for 10-bit color formats */
#define HFI_COLOR_FORMAT_10_BIT_BASE      (0x4000)

#define HFI_COLOR_FORMAT_MONOCHROME			(HFI_COMMON_BASE + 0x1)
#define HFI_COLOR_FORMAT_NV12				(HFI_COMMON_BASE + 0x2)
#define HFI_COLOR_FORMAT_NV21				(HFI_COMMON_BASE + 0x3)
#define HFI_COLOR_FORMAT_NV12_4x4TILE		(HFI_COMMON_BASE + 0x4)
#define HFI_COLOR_FORMAT_NV21_4x4TILE		(HFI_COMMON_BASE + 0x5)
#define HFI_COLOR_FORMAT_YUYV				(HFI_COMMON_BASE + 0x6)
#define HFI_COLOR_FORMAT_YVYU				(HFI_COMMON_BASE + 0x7)
#define HFI_COLOR_FORMAT_UYVY				(HFI_COMMON_BASE + 0x8)
#define HFI_COLOR_FORMAT_VYUY				(HFI_COMMON_BASE + 0x9)
#define HFI_COLOR_FORMAT_RGB565				(HFI_COMMON_BASE + 0xA)
#define HFI_COLOR_FORMAT_BGR565				(HFI_COMMON_BASE + 0xB)
#define HFI_COLOR_FORMAT_RGB888				(HFI_COMMON_BASE + 0xC)
#define HFI_COLOR_FORMAT_BGR888				(HFI_COMMON_BASE + 0xD)
#define HFI_COLOR_FORMAT_YUV444				(HFI_COMMON_BASE + 0xE)
#define HFI_COLOR_FORMAT_RGBA8888			(HFI_COMMON_BASE + 0x10)

#define HFI_COLOR_FORMAT_YUV420_TP10					\
		(HFI_COLOR_FORMAT_10_BIT_BASE + HFI_COLOR_FORMAT_NV12)
#define HFI_COLOR_FORMAT_P010					\
		(HFI_COLOR_FORMAT_10_BIT_BASE + HFI_COLOR_FORMAT_NV12 + 0x1)

#define HFI_COLOR_FORMAT_NV12_UBWC					\
		(HFI_COLOR_FORMAT_UBWC_BASE + HFI_COLOR_FORMAT_NV12)

#define HFI_COLOR_FORMAT_YUV420_TP10_UBWC				\
		(HFI_COLOR_FORMAT_UBWC_BASE + HFI_COLOR_FORMAT_YUV420_TP10)

#define  HFI_COLOR_FORMAT_RGBA8888_UBWC					\
		(HFI_COLOR_FORMAT_UBWC_BASE + HFI_COLOR_FORMAT_RGBA8888)

#define HFI_MAX_MATRIX_COEFFS 9
#define HFI_MAX_BIAS_COEFFS 3
#define HFI_MAX_LIMIT_COEFFS 6

#define HFI_STATISTICS_MODE_DEFAULT 0x10
#define HFI_STATISTICS_MODE_1 0x11
#define HFI_STATISTICS_MODE_2 0x12
#define HFI_STATISTICS_MODE_3 0x13

struct hfi_uncompressed_format_select {
	u32 buffer_type;
	u32 format;
};

struct hfi_uncompressed_format_supported {
	u32 buffer_type;
	u32 format_entries;
	u32 rg_format_info[1];
};

struct hfi_uncompressed_plane_actual {
	u32 actual_stride;
	u32 actual_plane_buffer_height;
};

struct hfi_uncompressed_plane_actual_info {
	u32 buffer_type;
	u32 num_planes;
	struct hfi_uncompressed_plane_actual rg_plane_format[1];
};

struct hfi_uncompressed_plane_constraints {
	u32 stride_multiples;
	u32 max_stride;
	u32 min_plane_buffer_height_multiple;
	u32 buffer_alignment;
};

struct hfi_uncompressed_plane_info {
	u32 format;
	u32 num_planes;
	struct hfi_uncompressed_plane_constraints rg_plane_format[1];
};

struct hfi_codec_supported {
	u32 decoder_codec_supported;
	u32 encoder_codec_supported;
};

struct hfi_properties_supported {
	u32 num_properties;
	u32 rg_properties[1];
};

struct hfi_max_sessions_supported {
	u32 max_sessions;
};

struct hfi_vpe_color_space_conversion {
	u32 input_color_primaries;
	u32 custom_matrix_enabled;
	u32 csc_matrix[HFI_MAX_MATRIX_COEFFS];
	u32 csc_bias[HFI_MAX_BIAS_COEFFS];
	u32 csc_limit[HFI_MAX_LIMIT_COEFFS];
};

#define HFI_ROTATE_NONE					(HFI_COMMON_BASE + 0x1)
#define HFI_ROTATE_90					(HFI_COMMON_BASE + 0x2)
#define HFI_ROTATE_180					(HFI_COMMON_BASE + 0x3)
#define HFI_ROTATE_270					(HFI_COMMON_BASE + 0x4)

#define HFI_FLIP_NONE					(HFI_COMMON_BASE + 0x1)
#define HFI_FLIP_HORIZONTAL				(HFI_COMMON_BASE + 0x2)
#define HFI_FLIP_VERTICAL				(HFI_COMMON_BASE + 0x4)

#define HFI_RESOURCE_SYSCACHE 0x00000002

struct hfi_resource_subcache_type {
	u32 size;
	u32 sc_id;
};

struct hfi_resource_syscache_info_type {
	u32 num_entries;
	struct hfi_resource_subcache_type rg_subcache_entries[1];
};

struct hfi_property_sys_image_version_info_type {
	u32 string_size;
	u8  str_image_version[1];
};

struct hfi_venc_config_advanced {
	u8 pipe2d;
	u8 hw_mode;
	u8 low_delay_enforce;
	u8 worker_vppsg_delay;
	u32 close_gop;
	u32 h264_constrain_intra_pred;
	u32 h264_transform_8x8_flag;
	u32 multi_refp_en;
	u32 qmatrix_en;
	u8 vpp_info_packet_mode;
	u8 ref_tile_mode;
	u8 bitstream_flush_mode;
	u32 vppsg_vspap_fb_sync_delay;
	u32 rc_initial_delay;
	u32 peak_bitrate_constraint;
	u32 ds_display_frame_width;
	u32 ds_display_frame_height;
	u32 perf_tune_param_ptr;
	u32 input_x_offset;
	u32 input_y_offset;
	u32 input_roi_width;
	u32 input_roi_height;
	u32 vsp_fifo_dma_sel;
	u32 h264_num_ref_frames;
};

struct hfi_vbv_hrd_bufsize {
	u32 buffer_size;
};

struct hfi_codec_mask_supported {
	u32 codecs;
	u32 video_domains;
};

struct hfi_aspect_ratio {
	u32 aspect_width;
	u32 aspect_height;
};

#define HFI_IFRAME_SIZE_DEFAULT			(HFI_COMMON_BASE + 0x1)
#define HFI_IFRAME_SIZE_MEDIUM			(HFI_COMMON_BASE + 0x2)
#define HFI_IFRAME_SIZE_HIGH			(HFI_COMMON_BASE + 0x3)
#define HFI_IFRAME_SIZE_UNLIMITED		(HFI_COMMON_BASE + 0x4)
struct hfi_iframe_size {
	u32 type;
};


#define HFI_CMD_SYS_COMMON_START			\
(HFI_DOMAIN_BASE_COMMON + HFI_ARCH_COMMON_OFFSET + HFI_CMD_START_OFFSET \
	+ 0x0000)
#define HFI_CMD_SYS_INIT		(HFI_CMD_SYS_COMMON_START + 0x001)
#define HFI_CMD_SYS_PC_PREP		(HFI_CMD_SYS_COMMON_START + 0x002)
#define HFI_CMD_SYS_SET_RESOURCE	(HFI_CMD_SYS_COMMON_START + 0x003)
#define HFI_CMD_SYS_RELEASE_RESOURCE (HFI_CMD_SYS_COMMON_START + 0x004)
#define HFI_CMD_SYS_SET_PROPERTY	(HFI_CMD_SYS_COMMON_START + 0x005)
#define HFI_CMD_SYS_GET_PROPERTY	(HFI_CMD_SYS_COMMON_START + 0x006)
#define HFI_CMD_SYS_SESSION_INIT	(HFI_CMD_SYS_COMMON_START + 0x007)
#define HFI_CMD_SYS_SESSION_END		(HFI_CMD_SYS_COMMON_START + 0x008)
#define HFI_CMD_SYS_SET_BUFFERS		(HFI_CMD_SYS_COMMON_START + 0x009)
#define HFI_CMD_SYS_TEST_START		(HFI_CMD_SYS_COMMON_START + 0x100)

#define HFI_CMD_SESSION_COMMON_START		\
	(HFI_DOMAIN_BASE_COMMON + HFI_ARCH_COMMON_OFFSET +	\
	HFI_CMD_START_OFFSET + 0x1000)
#define HFI_CMD_SESSION_SET_PROPERTY		\
	(HFI_CMD_SESSION_COMMON_START + 0x001)
#define HFI_CMD_SESSION_SET_BUFFERS			\
	(HFI_CMD_SESSION_COMMON_START + 0x002)
#define HFI_CMD_SESSION_GET_SEQUENCE_HEADER	\
	(HFI_CMD_SESSION_COMMON_START + 0x003)

#define HFI_MSG_SYS_COMMON_START			\
	(HFI_DOMAIN_BASE_COMMON + HFI_ARCH_COMMON_OFFSET +	\
	HFI_MSG_START_OFFSET + 0x0000)
#define HFI_MSG_SYS_INIT_DONE			(HFI_MSG_SYS_COMMON_START + 0x1)
#define HFI_MSG_SYS_PC_PREP_DONE		(HFI_MSG_SYS_COMMON_START + 0x2)
#define HFI_MSG_SYS_RELEASE_RESOURCE	(HFI_MSG_SYS_COMMON_START + 0x3)
#define HFI_MSG_SYS_DEBUG			(HFI_MSG_SYS_COMMON_START + 0x4)
#define HFI_MSG_SYS_SESSION_INIT_DONE	(HFI_MSG_SYS_COMMON_START + 0x6)
#define HFI_MSG_SYS_SESSION_END_DONE	(HFI_MSG_SYS_COMMON_START + 0x7)
#define HFI_MSG_SYS_IDLE		(HFI_MSG_SYS_COMMON_START + 0x8)
#define HFI_MSG_SYS_COV                 (HFI_MSG_SYS_COMMON_START + 0x9)
#define HFI_MSG_SYS_PROPERTY_INFO	(HFI_MSG_SYS_COMMON_START + 0xA)
#define HFI_MSG_SESSION_SYNC_DONE      (HFI_MSG_SESSION_OX_START + 0xD)

#define HFI_MSG_SESSION_COMMON_START		\
	(HFI_DOMAIN_BASE_COMMON + HFI_ARCH_COMMON_OFFSET +	\
	HFI_MSG_START_OFFSET + 0x1000)
#define HFI_MSG_EVENT_NOTIFY	(HFI_MSG_SESSION_COMMON_START + 0x1)
#define HFI_MSG_SESSION_GET_SEQUENCE_HEADER_DONE	\
	(HFI_MSG_SESSION_COMMON_START + 0x2)

#define HFI_CMD_SYS_TEST_SSR	(HFI_CMD_SYS_TEST_START + 0x1)
#define HFI_TEST_SSR_SW_ERR_FATAL	0x1
#define HFI_TEST_SSR_SW_DIV_BY_ZERO	0x2
#define HFI_TEST_SSR_HW_WDOG_IRQ	0x3

struct vidc_hal_cmd_pkt_hdr {
	u32 size;
	u32 packet_type;
};

struct vidc_hal_msg_pkt_hdr {
	u32 size;
	u32 packet;
};

struct vidc_hal_session_cmd_pkt {
	u32 size;
	u32 packet_type;
	u32 session_id;
};

struct hfi_cmd_sys_init_packet {
	u32 size;
	u32 packet_type;
	u32 arch_type;
};

struct hfi_cmd_sys_pc_prep_packet {
	u32 size;
	u32 packet_type;
};

struct hfi_cmd_sys_set_resource_packet {
	u32 size;
	u32 packet_type;
	u32 resource_handle;
	u32 resource_type;
	u32 rg_resource_data[1];
};

struct hfi_cmd_sys_release_resource_packet {
	u32 size;
	u32 packet_type;
	u32 resource_type;
	u32 resource_handle;
};

struct hfi_cmd_sys_set_property_packet {
	u32 size;
	u32 packet_type;
	u32 num_properties;
	u32 rg_property_data[1];
};

struct hfi_cmd_sys_get_property_packet {
	u32 size;
	u32 packet_type;
	u32 num_properties;
	u32 rg_property_data[1];
};

struct hfi_cmd_sys_session_init_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	u32 session_domain;
	u32 session_codec;
};

struct hfi_cmd_sys_session_end_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
};

struct hfi_cmd_sys_set_buffers_packet {
	u32 size;
	u32 packet_type;
	u32 buffer_type;
	u32 buffer_size;
	u32 num_buffers;
	u32 rg_buffer_addr[1];
};

struct hfi_cmd_session_set_property_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	u32 num_properties;
	u32 rg_property_data[1];
};

struct hfi_cmd_session_set_buffers_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	u32 buffer_type;
	u32 buffer_size;
	u32 extra_data_size;
	u32 min_buffer_size;
	u32 num_buffers;
	u32 rg_buffer_info[1];
};

struct hfi_buffer_mapping_type {
	u32 index;
	u32 device_addr;
	u32 size;
};

struct hfi_cmd_session_register_buffers_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	u32 client_data;
	u32 response_req;
	u32 num_buffers;
	struct hfi_buffer_mapping_type buffer[1];
};

struct hfi_cmd_session_unregister_buffers_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	u32 client_data;
	u32 response_req;
	u32 num_buffers;
	struct hfi_buffer_mapping_type buffer[1];
};

struct hfi_cmd_session_sync_process_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	u32 sync_id;
	u32 rg_data[1];
};

struct hfi_msg_event_notify_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	u32 event_id;
	u32 event_data1;
	u32 event_data2;
	u32 rg_ext_event_data[1];
};

struct hfi_msg_release_buffer_ref_event_packet {
	u32 packet_buffer;
	u32 extra_data_buffer;
	u32 output_tag;
};

struct hfi_msg_sys_init_done_packet {
	u32 size;
	u32 packet_type;
	u32 error_type;
	u32 num_properties;
	u32 rg_property_data[1];
};

struct hfi_msg_sys_pc_prep_done_packet {
	u32 size;
	u32 packet_type;
	u32 error_type;
};

struct hfi_msg_sys_release_resource_done_packet {
	u32 size;
	u32 packet_type;
	u32 resource_handle;
	u32 error_type;
};

struct hfi_msg_sys_session_init_done_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	u32 error_type;
	u32 num_properties;
	u32 rg_property_data[1];
};

struct hfi_msg_sys_session_end_done_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	u32 error_type;
};

struct hfi_msg_session_get_sequence_header_done_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	u32 error_type;
	u32 header_len;
	u32 sequence_header;
};

struct hfi_msg_sys_debug_packet {
	u32 size;
	u32 packet_type;
	u32 msg_type;
	u32 msg_size;
	u32 time_stamp_hi;
	u32 time_stamp_lo;
	u8 rg_msg_data[1];
};

struct hfi_msg_sys_coverage_packet {
	u32 size;
	u32 packet_type;
	u32 msg_size;
	u32 time_stamp_hi;
	u32 time_stamp_lo;
	u8 rg_msg_data[1];
};

enum HFI_VENUS_QTBL_STATUS {
	HFI_VENUS_QTBL_DISABLED = 0x00,
	HFI_VENUS_QTBL_ENABLED = 0x01,
	HFI_VENUS_QTBL_INITIALIZING = 0x02,
	HFI_VENUS_QTBL_DEINITIALIZING = 0x03
};

enum HFI_VENUS_CTRL_INIT_STATUS {
	HFI_VENUS_CTRL_NOT_INIT = 0x0,
	HFI_VENUS_CTRL_READY = 0x1,
	HFI_VENUS_CTRL_ERROR_FATAL = 0x2
};

struct hfi_sfr_struct {
	u32 bufSize;
	u8 rg_data[1];
};

struct hfi_cmd_sys_test_ssr_packet {
	u32 size;
	u32 packet_type;
	u32 trigger_type;
};

struct hfi_mastering_display_colour_sei_payload {
	u32 display_primariesX[3];
	u32 display_primariesY[3];
	u32 white_pointX;
	u32 white_pointY;
	u32 max_display_mastering_luminance;
	u32 min_display_mastering_luminance;
};

struct hfi_content_light_level_sei_payload {
	u32 max_content_light;
	u32 max_pic_average_light;
};

struct hfi_hdr10_pq_sei {
	struct hfi_mastering_display_colour_sei_payload mdisp_info;
	struct hfi_content_light_level_sei_payload cll_info;
};

struct hfi_vbv_hdr_buf_size {
	u32 vbv_hdr_buf_size;
};

#endif
