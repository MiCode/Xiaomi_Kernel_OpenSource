#ifndef __mt8193edid_h__
#define __mt8193edid_h__
#ifdef CONFIG_MTK_MT8193_HDMI_SUPPORT



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
typedef enum {
	HDMI_SINK_NO_DEEP_COLOR = 0,
	HDMI_SINK_DEEP_COLOR_10_BIT = (1 << 0),
	HDMI_SINK_DEEP_COLOR_12_BIT = (1 << 1),
	HDMI_SINK_DEEP_COLOR_16_BIT = (1 << 2)
} HDMI_SINK_DEEP_COLOR_T;

typedef struct _HDMI_SINK_AV_CAP_T {
	u32 ui4_sink_cea_ntsc_resolution;	/* use HDMI_SINK_VIDEO_RES_T */
	u32 ui4_sink_cea_pal_resolution;	/* use HDMI_SINK_VIDEO_RES_T */
	u32 ui4_sink_dtd_ntsc_resolution;	/* use HDMI_SINK_VIDEO_RES_T */
	u32 ui4_sink_dtd_pal_resolution;	/* use HDMI_SINK_VIDEO_RES_T */
	u32 ui4_sink_1st_dtd_ntsc_resolution;	/* use HDMI_SINK_VIDEO_RES_T */
	u32 ui4_sink_1st_dtd_pal_resolution;	/* use HDMI_SINK_VIDEO_RES_T */
	u32 ui4_sink_native_ntsc_resolution;	/* use HDMI_SINK_VIDEO_RES_T */
	u32 ui4_sink_native_pal_resolution;	/* use HDMI_SINK_VIDEO_RES_T */
	u16 ui2_sink_colorimetry;	/* use HDMI_SINK_VIDEO_COLORIMETRY_T */
	u16 ui2_sink_vcdb_data;	/* use HDMI_SINK_VCDB_T */
	u16 ui2_sink_aud_dec;	/* HDMI_SINK_AUDIO_DECODER_T */
	u8 ui1_sink_dsd_ch_num;
	u8 ui1_sink_pcm_ch_sampling[7];	/* n: channel number index, value: each bit means sampling rate for this channel number (SINK_AUDIO_32k..) */
	u8 ui1_sink_pcm_bit_size[7];	/* //n: channel number index, value: each bit means bit size for this channel number */
	u8 ui1_sink_dst_ch_sampling[7];	/* n: channel number index, value: each bit means sampling rate for this channel number (SINK_AUDIO_32k..) */
	u8 ui1_sink_dsd_ch_sampling[7];	/* n: channel number index, value: each bit means sampling rate for this channel number (SINK_AUDIO_32k..) */
	u16 ui1_sink_max_tmds_clock;
	u8 ui1_sink_spk_allocation;
	u8 ui1_sink_content_cnc;
	u8 ui1_sink_p_latency_present;
	u8 ui1_sink_i_latency_present;
	u8 ui1_sink_p_audio_latency;
	u8 ui1_sink_p_video_latency;
	u8 ui1_sink_i_audio_latency;
	u8 ui1_sink_i_video_latency;
	u8 e_sink_rgb_color_bit;
	u8 e_sink_ycbcr_color_bit;
	u8 u1_sink_support_ai;	/* kenny add 2010/4/25 */
	u8 u1_sink_max_tmds;	/* kenny add 2010/4/25 */
	u16 ui2_edid_chksum_and_audio_sup;	/* HDMI_EDID_CHKSUM_AND_AUDIO_SUP_T */
	u16 ui2_sink_cec_address;
	u8 b_sink_edid_ready;
	u8 b_sink_support_hdmi_mode;
	u8 ui1_ExtEdid_Revision;
	u8 ui1_Edid_Version;
	u8 ui1_Edid_Revision;
	u8 ui1_sink_support_ai;
	u8 ui1_Display_Horizontal_Size;
	u8 ui1_Display_Vertical_Size;
	u8 b_sink_hdmi_video_present;
	u8 ui1_CNC;
	u8 b_sink_3D_present;
	u32 ui4_sink_cea_3D_resolution;
} HDMI_SINK_AV_CAP_T;

typedef struct _HDMI_EDID_INFO_T {
	u32 ui4_ntsc_resolution;	/* use EDID_VIDEO_RES_T, there are many resolution */
	u32 ui4_pal_resolution;	/* use EDID_VIDEO_RES_T */
	u32 ui4_sink_native_ntsc_resolution;	/* use EDID_VIDEO_RES_T, only one NTSC resolution, Zero means none native NTSC resolution is avaiable */
	u32 ui4_sink_native_pal_resolution;	/* use EDID_VIDEO_RES_T, only one resolution, Zero means none native PAL resolution is avaiable */
	u32 ui4_sink_cea_ntsc_resolution;	/* use EDID_VIDEO_RES_T */
	u32 ui4_sink_cea_pal_resolution;	/* use EDID_VIDEO_RES_T */
	u32 ui4_sink_dtd_ntsc_resolution;	/* use EDID_VIDEO_RES_T */
	u32 ui4_sink_dtd_pal_resolution;	/* use EDID_VIDEO_RES_T */
	u32 ui4_sink_1st_dtd_ntsc_resolution;	/* use EDID_VIDEO_RES_T */
	u32 ui4_sink_1st_dtd_pal_resolution;	/* use EDID_VIDEO_RES_T */
	u16 ui2_sink_colorimetry;	/* use EDID_VIDEO_COLORIMETRY_T */
	u8 ui1_sink_rgb_color_bit;	/* color bit for RGB */
	u8 ui1_sink_ycbcr_color_bit;	/* color bit for YCbCr */
	u16 ui2_sink_aud_dec;	/* use EDID_AUDIO_DECODER_T */
	u8 ui1_sink_is_plug_in;	/* 1: Plug in 0:Plug Out */
	u32 ui4_hdmi_pcm_ch_type;	/* use EDID_A_FMT_CH_TYPE */
	u32 ui4_hdmi_pcm_ch3ch4ch5ch7_type;	/* use EDID_A_FMT_CH_TYPE1 */
	u32 ui4_dac_pcm_ch_type;	/* use EDID_A_FMT_CH_TYPE */
	u8 ui1_sink_i_latency_present;
	u8 ui1_sink_p_audio_latency;
	u8 ui1_sink_p_video_latency;
	u8 ui1_sink_i_audio_latency;
	u8 ui1_sink_i_video_latency;
	u8 ui1ExtEdid_Revision;
	u8 ui1Edid_Version;
	u8 ui1Edid_Revision;
	u8 ui1_Display_Horizontal_Size;
	u8 ui1_Display_Vertical_Size;
	u32 ui4_ID_Serial_Number;
	u32 ui4_sink_cea_3D_resolution;
	u8 ui1_sink_support_ai;	/* 0: not support AI, 1:support AI */
	u16 ui2_sink_cec_address;
	u16 ui1_sink_max_tmds_clock;
	u16 ui2_sink_3D_structure;
	u32 ui4_sink_cea_FP_SUP_3D_resolution;
	u32 ui4_sink_cea_TOB_SUP_3D_resolution;
	u32 ui4_sink_cea_SBS_SUP_3D_resolution;
	u16 ui2_sink_ID_manufacturer_name;	/* (08H~09H) */
	u16 ui2_sink_ID_product_code;	/* (0aH~0bH) */
	u32 ui4_sink_ID_serial_number;	/* (0cH~0fH) */
	u8 ui1_sink_week_of_manufacture;	/* (10H) */
	u8 ui1_sink_year_of_manufacture;	/* (11H)  base on year 1990 */
} HDMI_EDID_INFO_T;


extern void mt8193_checkedid(u8 i1noedid);
extern u8 mt8193_fgreadedid(u8 i1noedid);
extern void vShowEdidInformation(void);
extern void vShowEdidRawData(void);
extern void vClearEdidInfo(void);
extern void mt8193_AppGetEdidInfo(HDMI_EDID_INFO_T *pv_get_info);
extern u8 vCheckPcmBitSize(u8 ui1ChNumInx);
extern u8 mt8193_Check_EdidHeader(void);
#endif
#endif
