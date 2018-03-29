/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __hdmiedid_h__
#define __hdmiedid_h__

/* (5) Define the EDID relative information */
/* (5.1) Define one EDID block length */
#define EDID_BLOCK_LEN      128
/* (5.2) Define EDID header length */
#define EDID_HEADER_LEN     8
/* (5.3) Define the address for EDID info. (ref. EDID Recommended Practive for EIA/CEA-861) */
/* Base Block 0 */
#define EDID_ADDR_HEADER                      0x00
#define EDID_ADDR_VERSION                     0x12
#define EDID_ADDR_REVISION                    0x13
#define EDID_IMAGE_HORIZONTAL_SIZE            0x15
#define EDID_IMAGE_VERTICAL_SIZE              0x16
#define EDID_ADDR_FEATURE_SUPPORT             0x18
#define EDID_ADDR_TIMING_DSPR_1               0x36
#define EDID_ADDR_TIMING_DSPR_2               0x48
#define EDID_ADDR_MONITOR_DSPR_1              0x5A
#define EDID_ADDR_MONITOR_DSPR_2              0x6C
#define EDID_ADDR_EXT_BLOCK_FLAG              0x7E
#define EDID_ADDR_EXTEND_BYTE3                0x03	/* EDID address: 0x83 */
						   /* for ID receiver if RGB, YCbCr 4:2:2 or 4:4:4 */
/* Extension Block 1: */
#define EXTEDID_ADDR_TAG                      0x00
#define EXTEDID_ADDR_REVISION                 0x01
#define EXTEDID_ADDR_OFST_TIME_DSPR           0x02

/* (5.4) Define the ID for descriptor block type */
/* Notice: reference Table 11 ~ 14 of "EDID Recommended Practive for EIA/CEA-861" */
#define DETAIL_TIMING_DESCRIPTOR              -1
#define UNKNOWN_DESCRIPTOR                    -255
#define MONITOR_NAME_DESCRIPTOR               0xFC
#define MONITOR_RANGE_LIMITS_DESCRIPTOR       0xFD


/* (5.5) Define the offset address of info. within detail timing descriptor block */
#define OFST_PXL_CLK_LO       0
#define OFST_PXL_CLK_HI       1
#define OFST_H_ACTIVE_LO      2
#define OFST_H_BLANKING_LO    3
#define OFST_H_ACT_BLA_HI     4
#define OFST_V_ACTIVE_LO      5
#define OFST_V_BLANKING_LO    6
#define OFST_V_ACTIVE_HI      7
#define OFST_FLAGS            17

/* (5.6) Define the ID for EDID extension type */
#define LCD_TIMING                  0x1
#define CEA_TIMING_EXTENSION        0x01
#define EDID_20_EXTENSION           0x20
#define COLOR_INFO_TYPE0            0x30
#define DVI_FEATURE_DATA            0x40
#define TOUCH_SCREEN_MAP            0x50
#define BLOCK_MAP                   0xF0
#define EXTENSION_DEFINITION        0xFF

/* (5.7) Define EDID VSDB header length */
#define EDID_VSDB_LEN               0x03
enum HDMI_SINK_DEEP_COLOR_T {
	HDMI_SINK_NO_DEEP_COLOR = 0,
	HDMI_SINK_DEEP_COLOR_10_BIT = (1 << 0),
	HDMI_SINK_DEEP_COLOR_12_BIT = (1 << 1),
	HDMI_SINK_DEEP_COLOR_16_BIT = (1 << 2)
};

struct HDMI_SINK_AV_CAP_T {
	unsigned int ui4_sink_cea_ntsc_resolution;	/* use HDMI_SINK_VIDEO_RES_T */
	unsigned int ui4_sink_cea_pal_resolution;	/* use HDMI_SINK_VIDEO_RES_T */
	unsigned int ui4_sink_dtd_ntsc_resolution;	/* use HDMI_SINK_VIDEO_RES_T */
	unsigned int ui4_sink_dtd_pal_resolution;	/* use HDMI_SINK_VIDEO_RES_T */
	unsigned int ui4_sink_1st_dtd_ntsc_resolution;	/* use HDMI_SINK_VIDEO_RES_T */
	unsigned int ui4_sink_1st_dtd_pal_resolution;	/* use HDMI_SINK_VIDEO_RES_T */
	unsigned int ui4_sink_native_ntsc_resolution;	/* use HDMI_SINK_VIDEO_RES_T */
	unsigned int ui4_sink_native_pal_resolution;	/* use HDMI_SINK_VIDEO_RES_T */
	unsigned short ui2_sink_colorimetry;	/* use HDMI_SINK_VIDEO_COLORIMETRY_T */
	unsigned short ui2_sink_vcdb_data;	/* use HDMI_SINK_VCDB_T */
	unsigned short ui2_sink_aud_dec;	/* HDMI_SINK_AUDIO_DECODER_T */
	unsigned char ui1_sink_dsd_ch_num;
	/* n: channel number index, value: each bit means sampling rate for this channel number (SINK_AUDIO_32k..) */
	unsigned char ui1_sink_pcm_ch_sampling[7];
	/* //n: channel number index, value: each bit means bit size for this channel number */
	unsigned char ui1_sink_pcm_bit_size[7];
	/* n: channel number index, value: each bit means sampling rate for this channel number (SINK_AUDIO_32k..) */
	unsigned char ui1_sink_dst_ch_sampling[7];
	/* n: channel number index, value: each bit means sampling rate for this channel number (SINK_AUDIO_32k..) */
	unsigned char ui1_sink_dsd_ch_sampling[7];
	unsigned short ui1_sink_max_tmds_clock;
	unsigned char ui1_sink_spk_allocation;
	unsigned char ui1_sink_content_cnc;
	unsigned char ui1_sink_p_latency_present;
	unsigned char ui1_sink_i_latency_present;
	unsigned char ui1_sink_p_audio_latency;
	unsigned char ui1_sink_p_video_latency;
	unsigned char ui1_sink_i_audio_latency;
	unsigned char ui1_sink_i_video_latency;
	unsigned char e_sink_rgb_color_bit;
	unsigned char e_sink_ycbcr_color_bit;
	unsigned char u1_sink_support_ai;	/* kenny add 2010/4/25 */
	unsigned char u1_sink_max_tmds;	/* kenny add 2010/4/25 */
	unsigned short ui2_edid_chksum_and_audio_sup;	/* HDMI_EDID_CHKSUM_AND_AUDIO_SUP_T */
	unsigned short ui2_sink_cec_address;
	unsigned char b_sink_edid_ready;
	unsigned char b_sink_support_hdmi_mode;
	unsigned char ui1_ExtEdid_Revision;
	unsigned char ui1_Edid_Version;
	unsigned char ui1_Edid_Revision;
	unsigned char ui1_sink_support_ai;
	unsigned char ui1_Display_Horizontal_Size;
	unsigned char ui1_Display_Vertical_Size;
	unsigned char b_sink_hdmi_video_present;
	unsigned char ui1_CNC;
	unsigned char b_sink_3D_present;
	unsigned int ui4_sink_cea_3D_resolution;
	unsigned char b_sink_SCDC_present;
	unsigned char b_sink_LTE_340M_sramble;
	unsigned int ui4_sink_hdmi_4k2kvic;
	unsigned int bandwidth;
};

extern struct semaphore hdmi_edid_mutex;
extern void hdmi_checkedid(unsigned char i1noedid);
extern unsigned char hdmi_fgreadedid(unsigned char i1noedid);
extern void vShowEdidInformation(void);
extern void vShowEdidRawData(void);
extern void vClearEdidInfo(void);
extern void hdmi_AppGetEdidInfo(HDMI_EDID_T *pv_get_info);
extern unsigned char vCheckPcmBitSize(unsigned char ui1ChNumInx);

extern unsigned char hdmi_check_edid_header(void);
#endif
