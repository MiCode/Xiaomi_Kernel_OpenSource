/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Joey Pan <joey.pan@mediatek.com>
 */

#ifndef __HDMI_DRV_H__
#define __HDMI_DRV_H__
#ifndef CONFIG_MTK_INTERNAL_HDMI_SUPPORT
#include "lcm_drv.h"
#endif
#ifdef HDMI_MT8193_SUPPORT
#include "mt8193hdmictrl.h"
#include "mt8193edid.h"
#include "mt8193cec.h"
#endif

#define AVD_TMR_ISR_TICKS   10
#define MDI_BOUCING_TIMING  50	/* 20 //20ms */

#ifdef CONFIG_MTK_INTERNAL_HDMI_SUPPORT
#include "hdmicec.h"
#endif

enum HDMI_TASK_COMMAND_TYPE_T {
	HDMI_CEC_CMD = 0,
	HDMI_PLUG_DETECT_CMD,
	HDMI_HDCP_PROTOCAL_CMD,
	HDMI_DISABLE_HDMI_TASK_CMD,
	MAX_HDMI_TMR_NUMBER
};

#ifndef ARY_SIZE
#define ARY_SIZE(x) (sizeof((x)) / sizeof((x[0])))
#endif

enum HDMI_POLARITY {
	HDMI_POLARITY_RISING = 0,
	HDMI_POLARITY_FALLING = 1
};

enum HDMI_CLOCK_PHASE {
	HDMI_CLOCK_PHASE_0 = 0,
	HDMI_CLOCK_PHASE_90 = 1
};

enum HDMI_COLOR_ORDER {
	HDMI_COLOR_ORDER_RGB = 0,
	HDMI_COLOR_ORDER_BGR = 1
};

enum IO_DRIVING_CURRENT {
	IO_DRIVING_CURRENT_8MA = (1 << 0),
	IO_DRIVING_CURRENT_4MA = (1 << 1),
	IO_DRIVING_CURRENT_2MA = (1 << 2),
	IO_DRIVING_CURRENT_SLEW_CNTL = (1 << 3),
};

struct HDMI_EDID_INFO_T {
	/* use EDID_VIDEO_RES_T, there are many resolution */
	unsigned int ui4_ntsc_resolution;
	/* use EDID_VIDEO_RES_T */
	unsigned int ui4_pal_resolution;
	unsigned int ui4_sink_native_ntsc_resolution;
	unsigned int ui4_sink_native_pal_resolution;
	/* use EDID_VIDEO_RES_T */
	unsigned int ui4_sink_cea_ntsc_resolution;
	/* use EDID_VIDEO_RES_T */
	unsigned int ui4_sink_cea_pal_resolution;
	/* use EDID_VIDEO_RES_T */
	unsigned int ui4_sink_dtd_ntsc_resolution;
	/* use EDID_VIDEO_RES_T */
	unsigned int ui4_sink_dtd_pal_resolution;
	/* use EDID_VIDEO_RES_T */
	unsigned int ui4_sink_1st_dtd_ntsc_resolution;
	/* use EDID_VIDEO_RES_T */
	unsigned int ui4_sink_1st_dtd_pal_resolution;
	/* use EDID_VIDEO_COLORIMETRY_T */
	unsigned short ui2_sink_colorimetry;
	/* color bit for RGB */
	unsigned char ui1_sink_rgb_color_bit;
	/* color bit for YCbCr */
	unsigned char ui1_sink_ycbcr_color_bit;
	/* use EDID_AUDIO_DECODER_T */
	unsigned short ui2_sink_aud_dec;
	/* 1: Plug in 0:Plug Out */
	unsigned char ui1_sink_is_plug_in;
	/* use EDID_A_FMT_CH_TYPE */
	unsigned int ui4_hdmi_pcm_ch_type;
	/* use EDID_A_FMT_CH_TYPE1 */
	unsigned int ui4_hdmi_pcm_ch3ch4ch5ch7_type;
	/* use EDID_A_FMT_CH_TYPE */
	unsigned int ui4_dac_pcm_ch_type;
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
	/* 0: not support AI, 1:support AI */
	unsigned char ui1_sink_support_ai;
	unsigned short ui2_sink_cec_address;
	unsigned short ui1_sink_max_tmds_clock;
	unsigned short ui2_sink_3D_structure;
	unsigned int ui4_sink_cea_FP_SUP_3D_resolution;
	unsigned int ui4_sink_cea_TOB_SUP_3D_resolution;
	unsigned int ui4_sink_cea_SBS_SUP_3D_resolution;
	/* (08H~09H) */
	unsigned short ui2_sink_ID_manufacturer_name;
	/* (0aH~0bH) */
	unsigned short ui2_sink_ID_product_code;
	/* (0cH~0fH) */
	unsigned int ui4_sink_ID_serial_number;
	/* (10H) */
	unsigned char ui1_sink_week_of_manufacture;
	/* (11H)  base on year 1990 */
	unsigned char ui1_sink_year_of_manufacture;
};

#ifdef CONFIG_MTK_INTERNAL_HDMI_SUPPORT
enum HDMI_VIDEO_RESOLUTION {
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
	HDMI_VIDEO_2160p_DSC_30Hz,
	HDMI_VIDEO_2160p_DSC_24Hz,

	HDMI_VIDEO_RESOLUTION_NUM
};
#else
#if !defined(HDMI_MT8193_SUPPORT)
enum HDMI_VIDEO_RESOLUTION {
	HDMI_VIDEO_720x480p_60Hz = 0,
	HDMI_VIDEO_1440x480i_60Hz = 1,
	HDMI_VIDEO_1280x720p_60Hz = 2,
	HDMI_VIDEO_1920x1080i_60Hz = 5,
	HDMI_VIDEO_1920x1080p_30Hz = 6,
	HDMI_VIDEO_720x480i_60Hz = 0xD,
	HDMI_VIDEO_1920x1080p_60Hz = 0x0b,
	HDMI_VIDEO_2160p_DSC_30Hz = 0x13,
	HDMI_VIDEO_2160p_DSC_24Hz = 0x14,
	HDMI_VIDEO_RESOLUTION_NUM
};
#endif
#endif
enum TEST_CASE_TYPE {
	Test_RGB888 = 0, Test_YUV422 = 1, Test_YUV444 = 2, Test_Reserved = 3
};

enum HDMI_VIDEO_INPUT_FORMAT {
	HDMI_VIN_FORMAT_RGB565,
	HDMI_VIN_FORMAT_RGB666,
	HDMI_VIN_FORMAT_RGB888,
};

enum HDMI_VIDEO_OUTPUT_FORMAT {
	HDMI_VOUT_FORMAT_RGB888,
	HDMI_VOUT_FORMAT_YUV422,
	HDMI_VOUT_FORMAT_YUV444,

	HDMI_VOUT_FORMAT_2D = 1 << 16,
	HDMI_VOUT_FORMAT_3D_SBS = 1 << 17,
	HDMI_VOUT_FORMAT_3D_TAB = 1 << 18,
};

/* Must align to MHL Tx chip driver define */
enum HDMI_AUDIO_FORMAT {
	HDMI_AUDIO_32K_2CH = 0x01,
	HDMI_AUDIO_44K_2CH = 0x02,
	HDMI_AUDIO_48K_2CH = 0x03,
	HDMI_AUDIO_96K_2CH = 0x05,
	HDMI_AUDIO_192K_2CH = 0x07,
	HDMI_AUDIO_32K_8CH = 0x81,
	HDMI_AUDIO_44K_8CH = 0x82,
	HDMI_AUDIO_48K_8CH = 0x83,
	HDMI_AUDIO_96K_8CH = 0x85,
	HDMI_AUDIO_192K_8CH = 0x87,
	HDMI_AUDIO_INITIAL = 0xFF
};

enum HDMI_AUDIO_PCM_FORMAT {
	HDMI_AUDIO_PCM_16bit_48000,
	HDMI_AUDIO_PCM_16bit_44100,
	HDMI_AUDIO_PCM_16bit_32000,
	HDMI_AUDIO_SOURCE_STREAM,
};

struct HDMI_CONFIG {
	enum HDMI_VIDEO_RESOLUTION vformat;
	enum HDMI_VIDEO_INPUT_FORMAT vin;
	enum HDMI_VIDEO_OUTPUT_FORMAT vout;
	enum HDMI_AUDIO_FORMAT aformat;
};

enum HDMI_OUTPUT_MODE {
	HDMI_OUTPUT_MODE_LCD_MIRROR,
	HDMI_OUTPUT_MODE_VIDEO_MODE,
	HDMI_OUTPUT_MODE_DPI_BYPASS
};

enum HDMI_CABLE_TYPE {
	HDMI_CABLE,
	MHL_CABLE,
	MHL_SMB_CABLE,
	MHL_2_CABLE,		/* /MHL 2.0 */
	MHL_3D_GLASSES,
	SLIMPORT_CABLE
};

enum HDMI_3D_FORMAT_ENUM {
	HDMI_2D,
	HDMI_3D_SBS,
	HDMI_3D_TAB,
	HDMI_3D_FP
};

struct HDMI_PARAMS {
	unsigned int width;
	unsigned int height;

	struct HDMI_CONFIG init_config;

	/* polarity parameters */
	enum HDMI_POLARITY clk_pol;
	enum HDMI_POLARITY de_pol;
	enum HDMI_POLARITY vsync_pol;
	enum HDMI_POLARITY hsync_pol;

	/* timing parameters */
	unsigned int hsync_pulse_width;
	unsigned int hsync_back_porch;
	unsigned int hsync_front_porch;
	unsigned int vsync_pulse_width;
	unsigned int vsync_back_porch;
	unsigned int vsync_front_porch;

	/* output format parameters */
	enum HDMI_COLOR_ORDER rgb_order;

	/* intermediate buffers parameters */
	unsigned int intermediat_buffer_num;	/* 2..3 */

	/* iopad parameters */
	enum IO_DRIVING_CURRENT io_driving_current;
	enum HDMI_OUTPUT_MODE output_mode;

	int is_force_awake;
	int is_force_landscape;

	/* determine the scaling of output screen size, valid value 0~10 */
	unsigned int scaling_factor;
	/* 0 means no scaling, 5 means scaling to 95%, 10 means 90% */

	bool NeedSwHDCP;
	enum HDMI_CABLE_TYPE cabletype;
	unsigned int HDCPSupported;
	int is_3d_support;
	unsigned int input_clock;
#ifndef CONFIG_MTK_INTERNAL_HDMI_SUPPORT
	struct LCM_DSI_PARAMS dsi_params;
#endif
};

enum HDMI_STATE {
	HDMI_STATE_NO_DEVICE,
	HDMI_STATE_ACTIVE,
	HDMI_STATE_CONNECTING,
	HDMI_STATE_PLUGIN_ONLY,
	HDMI_STATE_EDID_UPDATE,
	HDMI_STATE_CEC_UPDATE,
	HDMI_STATE_NO_DEVICE_IN_BOOT,
	HDMI_STATE_ACTIVE_IN_BOOT,

};

enum HDMI_CEC_STATE {
	HDMI_CEC_STATE_PLUG_OUT = 0,
	HDMI_CEC_STATE_GET_PA,
	HDMI_CEC_STATE_TX_STS,
	HDMI_CEC_STATE_GET_CMD
};

enum HDMI_DEEP_COLOR_T {
	HDMI_DEEP_COLOR_AUTO = 0,
	HDMI_NO_DEEP_COLOR,
	HDMI_DEEP_COLOR_10_BIT,
	HDMI_DEEP_COLOR_12_BIT,
	HDMI_DEEP_COLOR_16_BIT
};

enum HDMI_OUT_COLOR_SPACE_T {
	HDMI_RGB = 0,
	HDMI_RGB_FULL,
	HDMI_YCBCR_444,
	HDMI_YCBCR_422,
	HDMI_XV_YCC,
	HDMI_YCBCR_444_FULL,
	HDMI_YCBCR_422_FULL
};

enum HDMI_AUDIO_SAMPLING_T {
	HDMI_FS_32K = 0,
	HDMI_FS_44K,
	HDMI_FS_48K,
	HDMI_FS_88K,
	HDMI_FS_96K,
	HDMI_FS_176K,
	HDMI_FS_192K
};

struct HDMI_AUDIO_DEC_OUTPUT_CHANNEL_T {
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

};

enum HDMI_AUDIO_INPUT_TYPE_T {
	SV_I2S = 0,
	SV_SPDIF
};

enum HDMI_AUDIO_I2S_FMT_T {
	HDMI_RJT_24BIT = 0,
	HDMI_RJT_16BIT,
	HDMI_LJT_24BIT,
	HDMI_LJT_16BIT,
	HDMI_I2S_24BIT,
	HDMI_I2S_16BIT
};

enum SAMPLE_FREQUENCY_T {
	MCLK_128FS,
	MCLK_192FS,
	MCLK_256FS,
	MCLK_384FS,
	MCLK_512FS,
	MCLK_768FS,
	MCLK_1152FS,
};

enum IEC_FRAME_RATE_T {
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
};


union AUDIO_DEC_OUTPUT_CHANNEL_UNION_T {
	/* HDMI_AUDIO_DEC_OUTPUT_CHANNEL_T */
	struct HDMI_AUDIO_DEC_OUTPUT_CHANNEL_T bit;
	unsigned short word;

};

struct HDMI_AV_INFO_T {
	enum HDMI_VIDEO_RESOLUTION e_resolution;
	unsigned char fgHdmiOutEnable;
	unsigned char u2VerFreq;
	unsigned char b_hotplug_state;
	enum HDMI_OUT_COLOR_SPACE_T e_video_color_space;
	enum HDMI_DEEP_COLOR_T e_deep_color_bit;
	unsigned char ui1_aud_out_ch_number;
	enum HDMI_AUDIO_SAMPLING_T e_hdmi_fs;
	unsigned char bhdmiRChstatus[6];
	unsigned char bhdmiLChstatus[6];
	unsigned char bMuteHdmiAudio;
	unsigned char u1HdmiI2sMclk;
	unsigned char u1hdcponoff;
	unsigned char u1audiosoft;
	unsigned char fgHdmiTmdsEnable;
	union AUDIO_DEC_OUTPUT_CHANNEL_UNION_T ui2_aud_out_ch;

	unsigned char e_hdmi_aud_in;
	unsigned char e_iec_frame;
	unsigned char e_aud_code;
	unsigned char u1Aud_Input_Chan_Cnt;
	unsigned char e_I2sFmt;
};


/* ---------------------------------------------------- */

struct HDMI_UTIL_FUNCS {
	void (*set_reset_pin)(unsigned int value);
	int (*set_gpio_out)(unsigned int gpio, unsigned int value);
	void (*udelay)(unsigned int us);
	void (*mdelay)(unsigned int ms);
	void (*wait_transfer_done)(void);
	void (*state_callback)(enum HDMI_STATE state);
	void (*cec_state_callback)(enum HDMI_CEC_STATE state);
};

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
#define SINK_1080P23976  (1 << 21)
#define SINK_1080P2997   (1 << 22)
#define SINK_2160p30   (1 << 23)
#define SINK_2160p24   (1 << 24)

typedef void (*CABLE_INSERT_CALLBACK) (enum HDMI_STATE state);

struct HDMI_DRIVER {
	void (*set_util_funcs)(const struct HDMI_UTIL_FUNCS *util);
	void (*get_params)(struct HDMI_PARAMS *params);
	int (*init)(void);
	int (*enter)(void);
	int (*exit)(void);
	void (*suspend)(void);
	void (*resume)(void);
	int (*video_config)(enum HDMI_VIDEO_RESOLUTION vformat,
			     enum HDMI_VIDEO_INPUT_FORMAT vin, int vou);
	int (*audio_config)(enum HDMI_AUDIO_FORMAT aformat, int bitWidth);
	int (*video_enable)(bool enable);
	int (*audio_enable)(bool enable);
	int (*irq_enable)(bool enable);
	int (*power_on)(void);
	void (*power_off)(void);
	enum HDMI_STATE (*get_state)(void);
	void (*set_mode)(unsigned char ucMode);
	void (*dump)(void);
	int (*get_external_device_capablity)(void);
	void (*force_on)(int from_uart_drv);
	void (*register_callback)(CABLE_INSERT_CALLBACK cb);
	void (*unregister_callback)(CABLE_INSERT_CALLBACK cb);
#if !defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT)
	void (*read)(unsigned char u8Reg);
	void (*write)(unsigned char u8Reg, unsigned char u8Data);
	void (*log_enable)(bool enable);
	void (*getedid)(void *pv_get_info);
#else
	void (*read)(unsigned long u2Reg, unsigned int *p4Data);
	void (*write)(unsigned long u2Reg, unsigned int u4Data);
	void (*log_enable)(u16 enable);
	void (*InfoframeSetting)(u8 i1typemode, u8 i1typeselect);
	void (*checkedid)(u8 i1noedid);
	void (*colordeep)(u8 u1colorspace, u8 u1deepcolor);
	void (*enablehdcp)(u8 u1hdcponoff);
	void (*setcecrxmode)(u8 u1cecrxmode);
	void (*hdmistatus)(void);
	void (*hdcpkey)(u8 *pbhdcpkey);
	void (*getedid)(struct _HDMI_EDID_T *pv_get_info);
	void (*setcecla)(struct CEC_DRV_ADDR_CFG_T *prAddr);
	void (*sendsltdata)(u8 *pu1Data);
	void (*getceccmd)(struct CEC_FRAME_DESCRIPTION_IO *frame);
	void (*getsltdata)(struct CEC_SLT_DATA *rCecSltData);
	void (*setceccmd)(struct CEC_SEND_MSG_T *msg);
	void (*cecenable)(u8 u1EnCec);
	void (*getcecaddr_)(struct CEC_ADDRESS_IO *cecaddr);
	 u8 (*checkedidheader)(void);
	int (*audiosetting)(struct HDMITX_AUDIO_PARA *audio_para);
	int (*tmdsonoff)(unsigned char u1ionoff);
	void (*mutehdmi)(unsigned char u1flagvideomute,
			  unsigned char u1flagaudiomute);
	void (*svpmutehdmi)(unsigned char u1svpvideomute,
			     unsigned char u1svpaudiomute);
	void (*cecusrcmd)(unsigned int cmd, unsigned int *result);
	void (*getcectxstatus)(struct CEC_ACK_INFO_T *pt);
	u32 (*gethdmistatus)(void);
	void (*resolution_setting)(int res);
#endif

};
/* --------------------------------------------- */
/* HDMI Driver Functions */
/* --------------------------------------------- */
extern unsigned int dst_is_dsi;
extern struct semaphore hdmi_update_mutex;
const struct HDMI_DRIVER *HDMI_GetDriver(void);
void Notify_AP_MHL_TX_Event(unsigned int event, unsigned int event_param,
			    void *param);
extern int chip_device_id;
extern bool need_reset_usb_switch;
#endif				/* __HDMI_DRV_H__ */
