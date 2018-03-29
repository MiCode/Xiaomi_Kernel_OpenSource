/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef HDMITX_H
#define     HDMITX_H

/* #define HDMI_DRV "/dev/hdmitx" */
#include "mtkfb.h"
#include "disp_session.h"


typedef enum {
	HDMI_VIDEO_720x480p_60Hz = 0,	/* 0 */
	HDMI_VIDEO_720x576p_50Hz,	/* 1 */
	HDMI_VIDEO_1280x720p_60Hz,	/* 2 */
	HDMI_VIDEO_1280x720p_50Hz,	/* 3 */
	HDMI_VIDEO_1920x1080i_60Hz,	/* 4 */
	HDMI_VIDEO_1920x1080i_50Hz,	/* 5 */
	HDMI_VIDEO_1920x1080p_30Hz,	/* 6 */
	HDMI_VIDEO_1920x1080p_25Hz,	/* 7 */
	HDMI_VIDEO_1920x1080p_24Hz,	/* 8 */
	HDMI_VIDEO_1920x1080p_23Hz,	/* 9 */
	HDMI_VIDEO_1920x1080p_29Hz,	/* a */
	HDMI_VIDEO_1920x1080p_60Hz,	/* b */
	HDMI_VIDEO_1920x1080p_50Hz,	/* c */

	HDMI_VIDEO_1280x720p3d_60Hz,	/* d */
	HDMI_VIDEO_1280x720p3d_50Hz,	/* e */
	HDMI_VIDEO_1920x1080i3d_60Hz,	/* f */
	HDMI_VIDEO_1920x1080i3d_50Hz,	/* 10 */
	HDMI_VIDEO_1920x1080p3d_24Hz,	/* 11 */
	HDMI_VIDEO_1920x1080p3d_23Hz,	/* 12 */

	/*the 2160 mean 3840x2160 */
	HDMI_VIDEO_2160P_23_976HZ,	/* 13 */
	HDMI_VIDEO_2160P_24HZ,	/* 14 */
	HDMI_VIDEO_2160P_25HZ,	/* 15 */
	HDMI_VIDEO_2160P_29_97HZ,	/* 16 */
	HDMI_VIDEO_2160P_30HZ,	/* 17 */
	/*the 2161 mean 4096x2160 */
	HDMI_VIDEO_2161P_24HZ,	/* 18 */


	HDMI_VIDEO_RESOLUTION_NUM
} HDMI_VIDEO_RESOLUTION;

typedef enum {
	HDMI_DEEP_COLOR_AUTO = 0,
	HDMI_NO_DEEP_COLOR,
	HDMI_DEEP_COLOR_10_BIT,
	HDMI_DEEP_COLOR_12_BIT,
	HDMI_DEEP_COLOR_16_BIT
} HDMI_DEEP_COLOR_T;

typedef enum {
	SV_I2S = 0,
	SV_SPDIF
} HDMI_AUDIO_INPUT_TYPE_T;

typedef enum			/* new add 2007/9/12 */
{
	FS_16K = 0x00,
	FS_22K,
	FS_24K,
	FS_32K,
	FS_44K,
	FS_48K,
	FS_64K,
	FS_88K,
	FS_96K,
	FS_176K,
	FS_192K,
	FS512_44K,		/* for DSD */
	FS_768K,
	FS128_44k,
	FS_128K,
	FS_UNKNOWN,
	FS_48K_MAX_CH
} AUDIO_SAMPLING_T;

typedef enum {
	IEC_48K = 0,
	IEC_96K,
	IEC_192K,
	IEC_768K,
	IEC_44K,
	IEC_88K,
	IEC_176K,
	IEC_705K,
	IEC_16K,
	IEC_22K,
	IEC_24K,
	IEC_32K,


} IEC_FRAME_RATE_T;

typedef enum {
	HDMI_FS_32K = 0,
	HDMI_FS_44K,
	HDMI_FS_48K,
	HDMI_FS_88K,
	HDMI_FS_96K,
	HDMI_FS_176K,
	HDMI_FS_192K
} HDMI_AUDIO_SAMPLING_T;

typedef enum {
	PCM_16BIT = 0,
	PCM_20BIT,
	PCM_24BIT
} PCM_BIT_SIZE_T;

typedef enum {
	HDMI_RGB = 0,
	HDMI_RGB_FULL,
	HDMI_YCBCR_444,
	HDMI_YCBCR_422,
	HDMI_XV_YCC,
	HDMI_YCBCR_444_FULL,
	HDMI_YCBCR_422_FULL
} HDMI_OUT_COLOR_SPACE_T;

typedef enum {
	HDMI_RJT_24BIT = 0,
	HDMI_RJT_16BIT,
	HDMI_LJT_24BIT,
	HDMI_LJT_16BIT,
	HDMI_I2S_24BIT,
	HDMI_I2S_16BIT
} HDMI_AUDIO_I2S_FMT_T;

typedef enum {
	AUD_INPUT_1_0 = 0,
	AUD_INPUT_1_1,
	AUD_INPUT_2_0,
	AUD_INPUT_2_1,
	AUD_INPUT_3_0,		/* C,L,R */
	AUD_INPUT_3_1,		/* C,L,R */
	AUD_INPUT_4_0,		/* L,R,RR,RL */
	AUD_INPUT_4_1,		/* L,R,RR,RL */
	AUD_INPUT_5_0,
	AUD_INPUT_5_1,
	AUD_INPUT_6_0,
	AUD_INPUT_6_1,
	AUD_INPUT_7_0,
	AUD_INPUT_7_1,
	AUD_INPUT_3_0_LRS,	/* LRS */
	AUD_INPUT_3_1_LRS,	/* LRS */
	AUD_INPUT_4_0_CLRS,	/* C,L,R,S */
	AUD_INPUT_4_1_CLRS,	/* C,L,R,S */
	/* new layout added for DTS */
	AUD_INPUT_6_1_Cs,
	AUD_INPUT_6_1_Ch,
	AUD_INPUT_6_1_Oh,
	AUD_INPUT_6_1_Chr,
	AUD_INPUT_7_1_Lh_Rh,
	AUD_INPUT_7_1_Lsr_Rsr,
	AUD_INPUT_7_1_Lc_Rc,
	AUD_INPUT_7_1_Lw_Rw,
	AUD_INPUT_7_1_Lsd_Rsd,
	AUD_INPUT_7_1_Lss_Rss,
	AUD_INPUT_7_1_Lhs_Rhs,
	AUD_INPUT_7_1_Cs_Ch,
	AUD_INPUT_7_1_Cs_Oh,
	AUD_INPUT_7_1_Cs_Chr,
	AUD_INPUT_7_1_Ch_Oh,
	AUD_INPUT_7_1_Ch_Chr,
	AUD_INPUT_7_1_Oh_Chr,
	AUD_INPUT_7_1_Lss_Rss_Lsr_Rsr,
	AUD_INPUT_6_0_Cs,
	AUD_INPUT_6_0_Ch,
	AUD_INPUT_6_0_Oh,
	AUD_INPUT_6_0_Chr,
	AUD_INPUT_7_0_Lh_Rh,
	AUD_INPUT_7_0_Lsr_Rsr,
	AUD_INPUT_7_0_Lc_Rc,
	AUD_INPUT_7_0_Lw_Rw,
	AUD_INPUT_7_0_Lsd_Rsd,
	AUD_INPUT_7_0_Lss_Rss,
	AUD_INPUT_7_0_Lhs_Rhs,
	AUD_INPUT_7_0_Cs_Ch,
	AUD_INPUT_7_0_Cs_Oh,
	AUD_INPUT_7_0_Cs_Chr,
	AUD_INPUT_7_0_Ch_Oh,
	AUD_INPUT_7_0_Ch_Chr,
	AUD_INPUT_7_0_Oh_Chr,
	AUD_INPUT_7_0_Lss_Rss_Lsr_Rsr,
	AUD_INPUT_8_0_Lh_Rh_Cs,
	AUD_INPUT_UNKNOWN = 0xFF
} AUD_CH_NUM_T;
typedef enum {
	MCLK_128FS,
	MCLK_192FS,
	MCLK_256FS,
	MCLK_384FS,
	MCLK_512FS,
	MCLK_768FS,
	MCLK_1152FS,
} SAMPLE_FREQUENCY_T;


/* /////////////////////////////////////////////////////////// */
typedef struct _AUDIO_DEC_OUTPUT_CHANNEL_T {
	unsigned short FL:1;	/* bit0 */
	unsigned short FR:1;	/* bit1 */
	unsigned short LFE:1;	/* bit2 */
	unsigned short FC:1;	/* bit3 */
	unsigned short RL:1;	/* bit4 */
	unsigned short RR:1;	/* bit5 */
	unsigned short RC:1;	/* bit6 */
	unsigned short FLC:1;	/* bit7 */
	unsigned short FRC:1;	/* bit8 */
	unsigned short RRC:1;	/* bit9 */
	unsigned short RLC:1;	/* bit10 */

} HDMI_AUDIO_DEC_OUTPUT_CHANNEL_T;

typedef union _AUDIO_DEC_OUTPUT_CHANNEL_UNION_T {
	HDMI_AUDIO_DEC_OUTPUT_CHANNEL_T bit;	/* HDMI_AUDIO_DEC_OUTPUT_CHANNEL_T */
	unsigned short word;

} AUDIO_DEC_OUTPUT_CHANNEL_UNION_T;

/* //////////////////////////////////////////////////////// */
typedef struct _HDMI_AV_INFO_T {
	HDMI_VIDEO_RESOLUTION e_resolution;
	unsigned char fgHdmiOutEnable;
	unsigned char u2VerFreq;
	unsigned char b_hotplug_state;
	HDMI_OUT_COLOR_SPACE_T e_video_color_space;
	HDMI_DEEP_COLOR_T e_deep_color_bit;
	unsigned char ui1_aud_out_ch_number;
	HDMI_AUDIO_SAMPLING_T e_hdmi_fs;
	unsigned char bhdmiRChstatus[6];
	unsigned char bhdmiLChstatus[6];
	unsigned char bMuteHdmiAudio;
	unsigned char u1HdmiI2sMclk;
	unsigned char u1hdcponoff;
	unsigned char u1audiosoft;
	unsigned char fgHdmiTmdsEnable;
	AUDIO_DEC_OUTPUT_CHANNEL_UNION_T ui2_aud_out_ch;

	unsigned char e_hdmi_aud_in;
	unsigned char e_iec_frame;
	unsigned char e_aud_code;
	unsigned char u1Aud_Input_Chan_Cnt;
	unsigned char e_I2sFmt;
} HDMI_AV_INFO_T;


typedef enum {
	AVD_BITS_NONE = 0,
	AVD_LPCM = 1,
	AVD_AC3,
	AVD_MPEG1_AUD,
	AVD_MP3,
	AVD_MPEG2_AUD,
	AVD_AAC,
	AVD_DTS,
	AVD_ATRAC,
	AVD_DSD,
	AVD_DOLBY_PLUS,
	AVD_DTS_HD,
	AVD_MAT_MLP,
	AVD_DST,
	AVD_WMA,
	AVD_CDDA,
	AVD_SACD_PCM,
	AVD_HDCD = 0xfe,
	AVD_BITS_OTHERS = 0xff
} AUDIO_BITSTREAM_TYPE_T;

typedef enum {
	EXTERNAL_EDID = 0,
	INTERNAL_EDID,
	NO_EDID
} GET_EDID_T;

#define SINK_480P      (1 << 0)
#define SINK_720P60    (1 << 1)
#define SINK_1080I60   (1 << 2)
#define SINK_1080P60   (1 << 3)
#define SINK_480P_1440 (1 << 4)
#define SINK_480P_2880 (1 << 5)
#define SINK_480I      (1 << 6)
#define SINK_480I_1440 (1 << 7)
#define SINK_480I_2880 (1 << 8)
#define SINK_1080P30   (1 << 9)
#define SINK_576P      (1 << 10)
#define SINK_720P50    (1 << 11)
#define SINK_1080I50   (1 << 12)
#define SINK_1080P50   (1 << 13)
#define SINK_576P_1440 (1 << 14)
#define SINK_576P_2880 (1 << 15)
#define SINK_576I      (1 << 16)
#define SINK_576I_1440 (1 << 17)
#define SINK_576I_2880 (1 << 18)
#define SINK_1080P25   (1 << 19)
#define SINK_1080P24   (1 << 20)
#define SINK_1080P23976   (1 << 21)
#define SINK_1080P2997   (1 << 22)

/*the 2160 mean 3840x2160 */
#define SINK_2160P_23_976HZ (1 << 0)
#define SINK_2160P_24HZ (1 << 1)
#define SINK_2160P_25HZ (1 << 2)
#define SINK_2160P_29_97HZ (1 << 3)
#define SINK_2160P_30HZ (1 << 4)
/*the 2161 mean 4096x2160 */
#define SINK_2161P_24HZ (1 << 5)
#define SINK_2161P_25HZ (1 << 6)
#define SINK_2161P_30HZ (1 << 7)
#define SINK_2160P_50HZ (1 << 8)
#define SINK_2160P_60HZ (1 << 9)
#define SINK_2161P_50HZ (1 << 10)
#define SINK_2161P_60HZ (1 << 11)

/* This HDMI_SINK_VIDEO_COLORIMETRY_T will define what kind of YCBCR */
/* can be supported by sink. */
/* And each bit also defines the colorimetry data block of EDID. */
#define SINK_YCBCR_444 (1<<0)
#define SINK_YCBCR_422 (1<<1)
#define SINK_XV_YCC709 (1<<2)
#define SINK_XV_YCC601 (1<<3)
#define SINK_METADATA0 (1<<4)
#define SINK_METADATA1 (1<<5)
#define SINK_METADATA2 (1<<6)
#define SINK_RGB       (1<<7)

/* The bandwitdh definiton For those TVs which support DP */
#define SINK_BW_6P75G 6750
#define SINK_BW_5P4G 5400
#define SINK_BW_2P7G 2700
#define SINK_BW_1P62G 1620


/* HDMI_SINK_VCDB_T Each bit defines the VIDEO Capability Data Block of EDID. */
#define SINK_CE_ALWAYS_OVERSCANNED                  (1<<0)
#define SINK_CE_ALWAYS_UNDERSCANNED                 (1<<1)
#define SINK_CE_BOTH_OVER_AND_UNDER_SCAN            (1<<2)
#define SINK_IT_ALWAYS_OVERSCANNED                  (1<<3)
#define SINK_IT_ALWAYS_UNDERSCANNED                 (1<<4)
#define SINK_IT_BOTH_OVER_AND_UNDER_SCAN            (1<<5)
#define SINK_PT_ALWAYS_OVERSCANNED                  (1<<6)
#define SINK_PT_ALWAYS_UNDERSCANNED                 (1<<7)
#define SINK_PT_BOTH_OVER_AND_UNDER_SCAN            (1<<8)
#define SINK_RGB_SELECTABLE                         (1<<9)


/* HDMI_SINK_AUDIO_DECODER_T define what kind of audio decoder */
/* can be supported by sink. */
#define   HDMI_SINK_AUDIO_DEC_LPCM        (1<<0)
#define   HDMI_SINK_AUDIO_DEC_AC3         (1<<1)
#define   HDMI_SINK_AUDIO_DEC_MPEG1       (1<<2)
#define   HDMI_SINK_AUDIO_DEC_MP3         (1<<3)
#define   HDMI_SINK_AUDIO_DEC_MPEG2       (1<<4)
#define   HDMI_SINK_AUDIO_DEC_AAC         (1<<5)
#define   HDMI_SINK_AUDIO_DEC_DTS         (1<<6)
#define   HDMI_SINK_AUDIO_DEC_ATRAC       (1<<7)
#define   HDMI_SINK_AUDIO_DEC_DSD         (1<<8)
#define   HDMI_SINK_AUDIO_DEC_DOLBY_PLUS   (1<<9)
#define   HDMI_SINK_AUDIO_DEC_DTS_HD      (1<<10)
#define   HDMI_SINK_AUDIO_DEC_MAT_MLP     (1<<11)
#define   HDMI_SINK_AUDIO_DEC_DST         (1<<12)
#define   HDMI_SINK_AUDIO_DEC_WMA         (1<<13)


/* Sink audio channel ability for a fixed Fs */
#define SINK_AUDIO_2CH   (1<<0)
#define SINK_AUDIO_3CH   (1<<1)
#define SINK_AUDIO_4CH   (1<<2)
#define SINK_AUDIO_5CH   (1<<3)
#define SINK_AUDIO_6CH   (1<<4)
#define SINK_AUDIO_7CH   (1<<5)
#define SINK_AUDIO_8CH   (1<<6)

/* Sink supported sampling rate for a fixed channel number */
#define SINK_AUDIO_32k (1<<0)
#define SINK_AUDIO_44k (1<<1)
#define SINK_AUDIO_48k (1<<2)
#define SINK_AUDIO_88k (1<<3)
#define SINK_AUDIO_96k (1<<4)
#define SINK_AUDIO_176k (1<<5)
#define SINK_AUDIO_192k (1<<6)

/* The following definition is for Sink speaker allocation data block . */
#define SINK_AUDIO_FL_FR   (1<<0)
#define SINK_AUDIO_LFE     (1<<1)
#define SINK_AUDIO_FC      (1<<2)
#define SINK_AUDIO_RL_RR   (1<<3)
#define SINK_AUDIO_RC      (1<<4)
#define SINK_AUDIO_FLC_FRC (1<<5)
#define SINK_AUDIO_RLC_RRC (1<<6)

/* The following definition is */
/* For EDID Audio Support, //HDMI_EDID_CHKSUM_AND_AUDIO_SUP_T */
#define SINK_BASIC_AUDIO_NO_SUP    (1<<0)
#define SINK_SAD_NO_EXIST          (1<<1)	/* short audio descriptor */
#define SINK_BASE_BLK_CHKSUM_ERR   (1<<2)
#define SINK_EXT_BLK_CHKSUM_ERR    (1<<3)


/* The following definition is for the output channel of */
/* audio decoder AUDIO_DEC_OUTPUT_CHANNEL_T */
#define AUDIO_DEC_FL   (1<<0)
#define AUDIO_DEC_FR   (1<<1)
#define AUDIO_DEC_LFE  (1<<2)
#define AUDIO_DEC_FC   (1<<3)
#define AUDIO_DEC_RL   (1<<4)
#define AUDIO_DEC_RR   (1<<5)
#define AUDIO_DEC_RC   (1<<6)
#define AUDIO_DEC_FLC  (1<<7)
#define AUDIO_DEC_FRC  (1<<8)

typedef enum {
	HDMI_STATUS_OK = 0,
	HDMI_STATUS_NOT_IMPLEMENTED,
	HDMI_STATUS_ALREADY_SET,
	HDMI_STATUS_ERROR,
} HDMI_STATUS;

typedef enum {
	SMART_BOOK_DISCONNECTED = 0,
	SMART_BOOK_CONNECTED,
} SMART_BOOK_STATE;

typedef enum {
	HDMI_POWER_STATE_OFF = 0,
	HDMI_POWER_STATE_ON,
	HDMI_POWER_STATE_STANDBY,
} HDMI_POWER_STATE;

typedef enum {
	HDMI_MAX_CHANNEL_2 = 0x2,
	HDMI_MAX_CHANNEL_3 = 0x3,
	HDMI_MAX_CHANNEL_4 = 0x4,
	HDMI_MAX_CHANNEL_5 = 0x5,
	HDMI_MAX_CHANNEL_6 = 0x6,
	HDMI_MAX_CHANNEL_7 = 0x7,
	HDMI_MAX_CHANNEL_8 = 0x8,
} AUDIO_MAX_CHANNEL;

typedef enum {
	HDMI_MAX_SAMPLERATE_32  = 0x1,
	HDMI_MAX_SAMPLERATE_44  = 0x2,
	HDMI_MAX_SAMPLERATE_48  = 0x3,
	HDMI_MAX_SAMPLERATE_96  = 0x4,
	HDMI_MAX_SAMPLERATE_192 = 0x5,
} AUDIO_MAX_SAMPLERATE;

typedef enum {
	HDMI_MAX_BITWIDTH_16  = 0x1,
	HDMI_MAX_BITWIDTH_24  = 0x2,
} AUDIO_MAX_BITWIDTH;

typedef enum {
	HDMI_SCALE_ADJUSTMENT_SUPPORT = 0x01,
	HDMI_ONE_RDMA_LIMITATION = 0x02,
	HDMI_PHONE_GPIO_REUSAGE       = 0x04,
	/*bit3-bit6: channal count; bit7-bit9: sample rate; bit10-bit11: bitwidth*/
	HDMI_FACTORY_MODE_NEW	 = 0x1000,
} HDMI_CAPABILITY;


typedef enum {
	HDMI_TO_TV = 0x0,
	HDMI_TO_SMB,
} hdmi_device_type;

typedef enum {
	HDMI_IS_DISCONNECTED = 0,
	HDMI_IS_CONNECTED = 1,
	HDMI_IS_RES_CHG = 0x11,
} hdmi_connect_status;

#define MAKE_MTK_HDMI_FORMAT_ID(id, bpp)  (((id) << 8) | (bpp))
typedef enum {
	MTK_HDMI_FORMAT_UNKNOWN = 0,

	MTK_HDMI_FORMAT_RGB565 = MAKE_MTK_HDMI_FORMAT_ID(1, 2),
	MTK_HDMI_FORMAT_RGB888 = MAKE_MTK_HDMI_FORMAT_ID(2, 3),
	MTK_HDMI_FORMAT_BGR888 = MAKE_MTK_HDMI_FORMAT_ID(3, 3),
	MTK_HDMI_FORMAT_ARGB8888 = MAKE_MTK_HDMI_FORMAT_ID(4, 4),
	MTK_HDMI_FORMAT_ABGR8888 = MAKE_MTK_HDMI_FORMAT_ID(5, 4),
	MTK_HDMI_FORMAT_YUV422 = MAKE_MTK_HDMI_FORMAT_ID(6, 2),
	MTK_HDMI_FORMAT_XRGB8888 = MAKE_MTK_HDMI_FORMAT_ID(7, 4),
	MTK_HDMI_FORMAT_XBGR8888 = MAKE_MTK_HDMI_FORMAT_ID(8, 4),
	MTK_HDMI_FORMAT_BPP_MASK = 0xFF,
} MTK_HDMI_FORMAT;

typedef struct {
	bool is_audio_enabled;
	bool is_video_enabled;
} hdmi_device_status;

typedef struct {
	void *src_base_addr;
	void *src_phy_addr;
	int src_fmt;
	unsigned int src_pitch;
	unsigned int src_offset_x, src_offset_y;
	unsigned int src_width, src_height;

	int next_buff_idx;
	int identity;
	int connected_type;
	unsigned int security;

} hdmi_video_buffer_info;


typedef struct {
	/* Input */
	int ion_fd;
	/* Output */
	unsigned int index;	/* fence count */
	int fence_fd;		/* fence fd */
} hdmi_buffer_info;

#define MTK_HDMI_NO_FENCE_FD        ((int)(-1))	/* ((int)(~0U>>1)) */
#define MTK_HDMI_NO_ION_FD        ((int)(-1))	/* ((int)(~0U>>1)) */

typedef struct {
	unsigned int u4Addr;
	unsigned int u4Data;
} hdmi_device_write;

typedef struct {
	unsigned int u4Data1;
	unsigned int u4Data2;
} hdmi_para_setting;

typedef struct {
	unsigned char u1Hdcpkey[287];
} hdmi_hdcp_key;

typedef struct {
	unsigned char u1Hdcpkey[384];
} hdmi_hdcp_drmkey;

typedef struct {
	unsigned char u1sendsltdata[15];
} send_slt_data;

typedef struct _HDMI_EDID_T {
	unsigned int ui4_ntsc_resolution;	/* use EDID_VIDEO_RES_T, there are many resolution */
	unsigned int ui4_pal_resolution;	/* use EDID_VIDEO_RES_T */
	/* use EDID_VIDEO_RES_T, only one NTSC resolution, Zero means none native NTSC resolution is available */
	unsigned int ui4_sink_native_ntsc_resolution;
	/* use EDID_VIDEO_RES_T, only one resolution, Zero means none native PAL resolution is available */
	unsigned int ui4_sink_native_pal_resolution;
	unsigned int ui4_sink_cea_ntsc_resolution;	/* use EDID_VIDEO_RES_T */
	unsigned int ui4_sink_cea_pal_resolution;	/* use EDID_VIDEO_RES_T */
	unsigned int ui4_sink_dtd_ntsc_resolution;	/* use EDID_VIDEO_RES_T */
	unsigned int ui4_sink_dtd_pal_resolution;	/* use EDID_VIDEO_RES_T */
	unsigned int ui4_sink_1st_dtd_ntsc_resolution;	/* use EDID_VIDEO_RES_T */
	unsigned int ui4_sink_1st_dtd_pal_resolution;	/* use EDID_VIDEO_RES_T */
	unsigned short ui2_sink_colorimetry;	/* use EDID_VIDEO_COLORIMETRY_T */
	unsigned char ui1_sink_rgb_color_bit;	/* color bit for RGB */
	unsigned char ui1_sink_ycbcr_color_bit;	/* color bit for YCbCr */
	unsigned short ui2_sink_aud_dec;	/* use EDID_AUDIO_DECODER_T */
	unsigned char ui1_sink_is_plug_in;	/* 1: Plug in 0:Plug Out */
	unsigned int ui4_hdmi_pcm_ch_type;	/* use EDID_A_FMT_CH_TYPE */
	unsigned int ui4_hdmi_pcm_ch3ch4ch5ch7_type;	/* use EDID_A_FMT_CH_TYPE1 */
	unsigned int ui4_dac_pcm_ch_type;	/* use EDID_A_FMT_CH_TYPE */
	unsigned char ui1_sink_i_latency_present;
	unsigned char ui1_sink_p_audio_latency;
	unsigned char ui1_sink_p_video_latency;
	unsigned char ui1_sink_i_audio_latency;
	unsigned char ui1_sink_i_video_latency;
	unsigned char ui1ExtEdid_Revision;
	unsigned char ui1Edid_Version;
	unsigned char ui1Edid_Revision;
	unsigned char ui1_Display_Horizontal_Size;
	unsigned char ui1_Display_Vertical_Size;
	unsigned int ui4_ID_Serial_Number;
	unsigned int ui4_sink_cea_3D_resolution;
	unsigned char ui1_sink_support_ai;	/* 0: not support AI, 1:support AI */
	unsigned short ui2_sink_cec_address;
	unsigned short ui1_sink_max_tmds_clock;
	unsigned short ui2_sink_3D_structure;
	unsigned int ui4_sink_cea_FP_SUP_3D_resolution;
	unsigned int ui4_sink_cea_TOB_SUP_3D_resolution;
	unsigned int ui4_sink_cea_SBS_SUP_3D_resolution;
	unsigned short ui2_sink_ID_manufacturer_name;	/* (08H~09H) */
	unsigned short ui2_sink_ID_product_code;	/* (0aH~0bH) */
	unsigned int ui4_sink_ID_serial_number;	/* (0cH~0fH) */
	unsigned char ui1_sink_week_of_manufacture;	/* (10H) */
	unsigned char ui1_sink_year_of_manufacture;	/* (11H)  base on year 1990 */
	unsigned char b_sink_SCDC_present;
	unsigned char b_sink_LTE_340M_sramble;
	unsigned int ui4_sink_hdmi_4k2kvic;
	unsigned char ui1rawdata_edid[512];
	unsigned int  bandwidth;
} HDMI_EDID_T;

typedef struct {
	unsigned int ui4_sink_FP_SUP_3D_resolution;
	unsigned int ui4_sink_TOB_SUP_3D_resolution;
	unsigned int ui4_sink_SBS_SUP_3D_resolution;
} MHL_3D_SUPP_T;

#define CEC_MAX_DEV_LA_NUM 3
#define CEC_MAX_OPERAND_SIZE       14
#define LOCAL_CEC_MAX_OPERAND_SIZE 14

typedef struct {
	unsigned char ui1_la_num;
	unsigned char e_la[CEC_MAX_DEV_LA_NUM];
	unsigned short ui2_pa;
	unsigned short h_cecm_svc;
} CEC_DRV_ADDR_CFG;

typedef struct {
	unsigned char destination:4;
	unsigned char initiator:4;
} CEC_HEADER_BLOCK_IO;

typedef struct {
	CEC_HEADER_BLOCK_IO header;
	unsigned char opcode;
	unsigned char operand[15];
} CEC_FRAME_BLOCK_IO;

typedef struct {
	unsigned char size;
	unsigned char sendidx;
	unsigned char reTXcnt;
	void *txtag;
	CEC_FRAME_BLOCK_IO blocks;
} CEC_FRAME_DESCRIPTION_IO;

typedef struct _CEC_FRAME_INFO {
	unsigned char ui1_init_addr;
	unsigned char ui1_dest_addr;
	unsigned short ui2_opcode;
	unsigned char aui1_operand[CEC_MAX_OPERAND_SIZE];
	unsigned int z_operand_size;
} CEC_FRAME_INFO;

typedef struct _CEC_SEND_MSG {
	void *pv_tag;
	CEC_FRAME_INFO t_frame_info;
	unsigned char b_enqueue_ok;
} CEC_SEND_MSG;

/* ACK condition */
typedef enum {
	APK_CEC_ACK_COND_OK = 0,
	APK_CEC_ACK_COND_NO_RESPONSE,
} APK_CEC_ACK_COND;

/* ACK info */
typedef struct _APK_CEC_ACK_INFO {
	void *pv_tag;
	APK_CEC_ACK_COND e_ack_cond;
} APK_CEC_ACK_INFO;

typedef struct {
	unsigned char ui1_la;
	unsigned short ui2_pa;
} CEC_ADDRESS_IO;

typedef struct {
	unsigned char u1Size;
	unsigned char au1Data[14];
} CEC_SLT_DATA;

typedef struct {
	unsigned int cmd;
	unsigned int result;
} CEC_USR_CMD_T;


typedef struct  {
	unsigned int u1address;
	unsigned int pu1Data;
} READ_REG_VALUE;

typedef struct {
	unsigned char e_hdmi_aud_in;
	unsigned char e_iec_frame;
	unsigned char e_hdmi_fs;
	unsigned char e_aud_code;
	unsigned char u1Aud_Input_Chan_Cnt;
	unsigned char e_I2sFmt;
	unsigned char u1HdmiI2sMclk;
	unsigned char bhdmi_LCh_status[5];
	unsigned char bhdmi_RCh_status[5];
} HDMITX_AUDIO_PARA;

struct HDMI_HDCP_BKSV_INFO {
	unsigned char bksv_list[160];
	unsigned int bstatus;
	unsigned char is_plug_in;
	unsigned char is_hdcp_ok;
};

#define HDMI_IOW(num, dtype)     _IOW('H', num, dtype)
#define HDMI_IOR(num, dtype)     _IOR('H', num, dtype)
#define HDMI_IOWR(num, dtype)    _IOWR('H', num, dtype)
#define HDMI_IO(num)             _IO('H', num)

#define MTK_HDMI_AUDIO_VIDEO_ENABLE             HDMI_IO(1)
#define MTK_HDMI_AUDIO_ENABLE                   HDMI_IO(2)
#define MTK_HDMI_VIDEO_ENABLE                   HDMI_IO(3)
#define MTK_HDMI_GET_CAPABILITY                 HDMI_IOWR(4, HDMI_CAPABILITY)
#define MTK_HDMI_GET_DEVICE_STATUS              HDMI_IOWR(5, hdmi_device_status)
#define MTK_HDMI_VIDEO_CONFIG                   HDMI_IOWR(6, int)
#define MTK_HDMI_AUDIO_CONFIG                   HDMI_IOWR(7, int)
#define MTK_HDMI_FORCE_FULLSCREEN_ON            HDMI_IOWR(8, int)
#define MTK_HDMI_FORCE_FULLSCREEN_OFF           HDMI_IOWR(9, int)
#define MTK_HDMI_IPO_POWEROFF                   HDMI_IOWR(10, int)
#define MTK_HDMI_IPO_POWERON                    HDMI_IOWR(11, int)
#define MTK_HDMI_POWER_ENABLE                   HDMI_IOW(12, int)
#define MTK_HDMI_PORTRAIT_ENABLE                HDMI_IOW(13, int)
#define MTK_HDMI_FORCE_OPEN                     HDMI_IOWR(14, int)
#define MTK_HDMI_FORCE_CLOSE                    HDMI_IOWR(15, int)
#define MTK_HDMI_IS_FORCE_AWAKE                 HDMI_IOWR(16, int)

#define MTK_HDMI_POST_VIDEO_BUFFER              HDMI_IOW(20,  struct fb_overlay_layer)
#define MTK_HDMI_AUDIO_SETTING                  HDMI_IOWR(21, HDMITX_AUDIO_PARA)
#define HDMI_SET_MULTIPLE_LAYERS                HDMI_IOW(22,  struct fb_overlay_layer)

#define MTK_HDMI_FACTORY_MODE_ENABLE            HDMI_IOW(30, int)
#define MTK_HDMI_FACTORY_GET_STATUS             HDMI_IOWR(31, int)
#define MTK_HDMI_FACTORY_DPI_TEST               HDMI_IOWR(32, int)

#define MTK_HDMI_USBOTG_STATUS                  HDMI_IOWR(33, int)
#define MTK_HDMI_GET_DRM_ENABLE                 HDMI_IOWR(34, int)

#define MTK_HDMI_GET_DEV_INFO                   HDMI_IOWR(35, mtk_dispif_info_t)
#define MTK_HDMI_PREPARE_BUFFER                 HDMI_IOW(36, struct fb_overlay_buffer)
#define MTK_HDMI_SCREEN_CAPTURE                 HDMI_IOW(37, unsigned long)

#define MTK_HDMI_WRITE_DEV                      HDMI_IOWR(52, hdmi_device_write)
#define MTK_HDMI_READ_DEV                       HDMI_IOWR(53, unsigned int)
#define MTK_HDMI_ENABLE_LOG                     HDMI_IOWR(54, unsigned int)
#define MTK_HDMI_CHECK_EDID                     HDMI_IOWR(55, unsigned int)
#define MTK_HDMI_INFOFRAME_SETTING              HDMI_IOWR(56, hdmi_para_setting)
#define MTK_HDMI_COLOR_DEEP                     HDMI_IOWR(57, hdmi_para_setting)
#define MTK_HDMI_ENABLE_HDCP                    HDMI_IOWR(58, unsigned int)
#define MTK_HDMI_STATUS                         HDMI_IOWR(59, unsigned int)
#define MTK_HDMI_HDCP_KEY                       HDMI_IOWR(60, hdmi_hdcp_key)
#define MTK_HDMI_GET_EDID                       HDMI_IOWR(61, HDMI_EDID_T)
#define MTK_HDMI_SETLA                          HDMI_IOWR(62, CEC_DRV_ADDR_CFG)
#define MTK_HDMI_GET_CECCMD                     HDMI_IOWR(63, CEC_FRAME_DESCRIPTION_IO)
#define MTK_HDMI_SET_CECCMD                     HDMI_IOWR(64, CEC_SEND_MSG)
#define MTK_HDMI_CEC_ENABLE                     HDMI_IOWR(65, unsigned int)
#define MTK_HDMI_GET_CECADDR                    HDMI_IOWR(66, CEC_ADDRESS_IO)
#define MTK_HDMI_CECRX_MODE                     HDMI_IOWR(67, unsigned int)
#define MTK_HDMI_SENDSLTDATA                    HDMI_IOWR(68, send_slt_data)
#define MTK_HDMI_GET_SLTDATA                    HDMI_IOWR(69, CEC_SLT_DATA)
#define MTK_HDMI_VIDEO_MUTE                     HDMI_IOWR(70, int)

#define MTK_HDMI_READ                           HDMI_IOWR(81, unsigned int)
#define MTK_HDMI_WRITE                          HDMI_IOWR(82, unsigned int)
#define MTK_HDMI_CMD                            HDMI_IOWR(83, unsigned int)
#define MTK_HDMI_DUMP                           HDMI_IOWR(84, unsigned int)
#define MTK_HDMI_DUMP6397                       HDMI_IOWR(85, unsigned int)
#define MTK_HDMI_DUMP6397_W                     HDMI_IOWR(86, unsigned int)
#define MTK_HDMI_CBUS_STATUS                    HDMI_IOWR(87, unsigned int)
#define MTK_HDMI_CONNECT_STATUS                 HDMI_IOWR(88, unsigned int)
#define MTK_HDMI_DUMP6397_R                     HDMI_IOWR(89, unsigned int)
#define MTK_MHL_GET_DCAP                        HDMI_IOWR(90, unsigned int)
#define MTK_MHL_GET_3DINFO                      HDMI_IOWR(91, unsigned int)
#define MTK_HDMI_HDCP                           HDMI_IOWR(92, unsigned int)
#define MTK_HDMI_GET_CECSTS                    HDMI_IOWR(93, APK_CEC_ACK_INFO)
#define MTK_HDMI_CEC_USR_CMD                    HDMI_IOWR(94, CEC_USR_CMD_T)
#define MTK_HDMI_SET_3D_STRUCT                    HDMI_IOWR(95, unsigned int)
#define MTK_HDMI_GET_HDMI_STATUS                    HDMI_IOWR(96, unsigned int)
#define MTK_HDMI_USER_MUTE			HDMI_IOWR(97, unsigned int)
#define MTK_HDMI_GET_HDCP_INFO                  HDMI_IOWR(98, unsigned int)

#define MTK_HDMI_FACTORY_CHIP_INIT              HDMI_IOWR(99, int)
#define MTK_HDMI_FACTORY_JUDGE_CALLBACK         HDMI_IOWR(100, int)
#define MTK_HDMI_FACTORY_START_DPI_AND_CONFIG   HDMI_IOWR(101, int)
#define MTK_HDMI_FACTORY_DPI_STOP_AND_POWER_OFF HDMI_IOWR(102, int)

extern void hdmi_force_resolution(int res);	/* coding sytle */
extern void hdmi_switch_resolution(int res);	/* coding sytle */
extern int hdmi_get_tv_fps(void);
extern int hdmi_get_frc_log_level(void);

void hdmi_set_layer_num(int layer_num);

#if CONFIG_COMPAT
#include <linux/compat.h>
#include <linux/uaccess.h>

struct COMPAT_HDMI_EDID_T {
	compat_uint_t ui4_ntsc_resolution;
	compat_uint_t ui4_pal_resolution;
	compat_uint_t ui4_sink_native_ntsc_resolution;
	compat_uint_t ui4_sink_native_pal_resolution;
	compat_uint_t ui4_sink_cea_ntsc_resolution;
	compat_uint_t ui4_sink_cea_pal_resolution;
	compat_uint_t ui4_sink_dtd_ntsc_resolution;
	compat_uint_t ui4_sink_dtd_pal_resolution;
	compat_uint_t ui4_sink_1st_dtd_ntsc_resolution;
	compat_uint_t ui4_sink_1st_dtd_pal_resolution;
	unsigned short ui2_sink_colorimetry;
	unsigned char ui1_sink_rgb_color_bit;
	unsigned char ui1_sink_ycbcr_color_bit;
	unsigned short ui2_sink_aud_dec;
	unsigned char ui1_sink_is_plug_in;
	compat_uint_t ui4_hdmi_pcm_ch_type;
	compat_uint_t ui4_hdmi_pcm_ch3ch4ch5ch7_type;
	compat_uint_t ui4_dac_pcm_ch_type;
	unsigned char ui1_sink_i_latency_present;
	unsigned char ui1_sink_p_audio_latency;
	unsigned char ui1_sink_p_video_latency;
	unsigned char ui1_sink_i_audio_latency;
	unsigned char ui1_sink_i_video_latency;
	unsigned char ui1ExtEdid_Revision;
	unsigned char ui1Edid_Version;
	unsigned char ui1Edid_Revision;
	unsigned char ui1_Display_Horizontal_Size;
	unsigned char ui1_Display_Vertical_Size;
	compat_uint_t ui4_ID_Serial_Number;
	compat_uint_t ui4_sink_cea_3D_resolution;
	unsigned char ui1_sink_support_ai;
	unsigned short ui2_sink_cec_address;
	unsigned short ui1_sink_max_tmds_clock;
	unsigned short ui2_sink_3D_structure;
	compat_uint_t ui4_sink_cea_FP_SUP_3D_resolution;
	compat_uint_t ui4_sink_cea_TOB_SUP_3D_resolution;
	compat_uint_t ui4_sink_cea_SBS_SUP_3D_resolution;
	unsigned short ui2_sink_ID_manufacturer_name;
	unsigned short ui2_sink_ID_product_code;
	compat_uint_t ui4_sink_ID_serial_number;
	unsigned char ui1_sink_week_of_manufacture;
	unsigned char ui1_sink_year_of_manufacture;
	unsigned char b_sink_SCDC_present;
	unsigned char b_sink_LTE_340M_sramble;
	compat_uint_t ui4_sink_hdmi_4k2kvic;
	unsigned char ui1rawdata_edid[512];
	unsigned int  bandwidth;

};

struct COMPAT_HDMITX_AUDIO_PARA {
	unsigned char e_hdmi_aud_in;
	unsigned char e_iec_frame;
	unsigned char e_hdmi_fs;
	unsigned char e_aud_code;
	unsigned char u1Aud_Input_Chan_Cnt;
	unsigned char e_I2sFmt;
	unsigned char u1HdmiI2sMclk;
	unsigned char bhdmi_LCh_status[5];
	unsigned char bhdmi_RCh_status[5];
};


#define	COMPAT_MTK_HDMI_AUDIO_SETTING		HDMI_IOWR(21, struct COMPAT_HDMITX_AUDIO_PARA)
#define	COMPAT_MTK_HDMI_GET_EDID			HDMI_IOWR(61, struct COMPAT_HDMI_EDID_T)

#endif
#endif
