#ifndef __HDMI_DRV_H__
#define __HDMI_DRV_H__

#ifdef CONFIG_MTK_MT8193_HDMI_SUPPORT

#include "mt8193hdmictrl.h"
#include "mt8193edid.h"
#include "mt8193cec.h"

#define AVD_TMR_ISR_TICKS   10
#define MDI_BOUCING_TIMING  50	/* 20 //20ms */

typedef enum {
	HDMI_CEC_CMD = 0,
	HDMI_PLUG_DETECT_CMD,
	HDMI_HDCP_PROTOCAL_CMD,
	HDMI_DISABLE_HDMI_TASK_CMD,
	MAX_HDMI_TMR_NUMBER
} HDMI_TASK_COMMAND_TYPE_T;

#endif

#ifndef ARY_SIZE
#define ARY_SIZE(x) (sizeof((x)) / sizeof((x[0])))
#endif

typedef enum {
	HDMI_POLARITY_RISING = 0,
	HDMI_POLARITY_FALLING = 1
} HDMI_POLARITY;

typedef enum {
	HDMI_CLOCK_PHASE_0 = 0,
	HDMI_CLOCK_PHASE_90 = 1
} HDMI_CLOCK_PHASE;

typedef enum {
	HDMI_COLOR_ORDER_RGB = 0,
	HDMI_COLOR_ORDER_BGR = 1
} HDMI_COLOR_ORDER;

typedef enum {
	IO_DRIVING_CURRENT_8MA = (1 << 0),
	IO_DRIVING_CURRENT_4MA = (1 << 1),
	IO_DRIVING_CURRENT_2MA = (1 << 2),
	IO_DRIVING_CURRENT_SLEW_CNTL = (1 << 3),
} IO_DRIVING_CURRENT;

#if !defined(CONFIG_MTK_MT8193_HDMI_SUPPORT)
typedef enum {
	HDMI_VIDEO_720x480p_60Hz = 0,
	HDMI_VIDEO_1280x720p_60Hz = 2,
	HDMI_VIDEO_1920x1080p_30Hz = 6,
	HDMI_VIDEO_1920x1080p_60Hz = 0x0b,
	HDMI_VIDEO_RESOLUTION_NUM
} HDMI_VIDEO_RESOLUTION;
#endif

typedef enum {
	HDMI_VIN_FORMAT_RGB565,
	HDMI_VIN_FORMAT_RGB666,
	HDMI_VIN_FORMAT_RGB888,
} HDMI_VIDEO_INPUT_FORMAT;

typedef enum {
	HDMI_VOUT_FORMAT_RGB888,
	HDMI_VOUT_FORMAT_YUV422,
	HDMI_VOUT_FORMAT_YUV444,
} HDMI_VIDEO_OUTPUT_FORMAT;

//Must align to MHL Tx chip driver define
typedef enum
{
	HDMI_AUDIO_32K_2CH		= 0x01,
	HDMI_AUDIO_44K_2CH		= 0x02,
	HDMI_AUDIO_48K_2CH		= 0x03,
	HDMI_AUDIO_96K_2CH		= 0x05,
	HDMI_AUDIO_192K_2CH	    = 0x07,
	HDMI_AUDIO_32K_8CH		= 0x81,
	HDMI_AUDIO_44K_8CH		= 0x82,
	HDMI_AUDIO_48K_8CH		= 0x83,
	HDMI_AUDIO_96K_8CH		= 0x85,
	HDMI_AUDIO_192K_8CH	    = 0x87,
	HDMI_AUDIO_INITIAL		= 0xFF
}HDMI_AUDIO_FORMAT;

typedef struct {
	HDMI_VIDEO_RESOLUTION vformat;
	HDMI_VIDEO_INPUT_FORMAT vin;
	HDMI_VIDEO_OUTPUT_FORMAT vout;
	HDMI_AUDIO_FORMAT aformat;
} HDMI_CONFIG;

typedef enum {
	HDMI_OUTPUT_MODE_LCD_MIRROR,
	HDMI_OUTPUT_MODE_VIDEO_MODE,
	HDMI_OUTPUT_MODE_DPI_BYPASS
} HDMI_OUTPUT_MODE;

typedef enum {
	HDMI_CABLE,
	MHL_CABLE,
	MHL_SMB_CABLE,
	MHL_2_CABLE		/* /MHL 2.0 */
} HDMI_CABLE_TYPE;

typedef struct {
	unsigned int width;
	unsigned int height;

	HDMI_CONFIG init_config;

	/* polarity parameters */
	HDMI_POLARITY clk_pol;
	HDMI_POLARITY de_pol;
	HDMI_POLARITY vsync_pol;
	HDMI_POLARITY hsync_pol;

	/* timing parameters */
	unsigned int hsync_pulse_width;
	unsigned int hsync_back_porch;
	unsigned int hsync_front_porch;
	unsigned int vsync_pulse_width;
	unsigned int vsync_back_porch;
	unsigned int vsync_front_porch;

	/* output format parameters */
	HDMI_COLOR_ORDER rgb_order;

	/* intermediate buffers parameters */
	unsigned int intermediat_buffer_num;	/* 2..3 */

	/* iopad parameters */
	IO_DRIVING_CURRENT io_driving_current;
	HDMI_OUTPUT_MODE output_mode;

	int is_force_awake;
	int is_force_landscape;

	unsigned int scaling_factor;	/* determine the scaling of output screen size, valid value 0~10 */
	/* 0 means no scaling, 5 means scaling to 95%, 10 means 90% */
	HDMI_CABLE_TYPE cabletype;
	bool HDCPSupported;
} HDMI_PARAMS;

typedef enum {
	HDMI_STATE_NO_DEVICE,
	HDMI_STATE_ACTIVE,
	HDMI_STATE_CONNECTING,
	HDMI_STATE_PLUGIN_ONLY,
	HDMI_STATE_EDID_UPDATE,
	HDMI_STATE_CEC_UPDATE
} HDMI_STATE;

/* --------------------------------------------------------------------------- */

typedef struct {
	void (*set_reset_pin) (unsigned int value);
	int (*set_gpio_out) (unsigned int gpio, unsigned int value);
	void (*udelay) (unsigned int us);
	void (*mdelay) (unsigned int ms);
	void (*wait_transfer_done) (void);
	void (*state_callback) (HDMI_STATE state);
} HDMI_UTIL_FUNCS;

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


typedef struct _HDMI_EDID_INFO_T {
	unsigned int ui4_ntsc_resolution;	/* use EDID_VIDEO_RES_T, there are many resolution */
	unsigned int ui4_pal_resolution;	/* use EDID_VIDEO_RES_T */
	unsigned int ui4_sink_native_ntsc_resolution;	/* use EDID_VIDEO_RES_T, only one NTSC resolution, Zero means none native NTSC resolution is avaiable */
	unsigned int ui4_sink_native_pal_resolution;	/* use EDID_VIDEO_RES_T, only one resolution, Zero means none native PAL resolution is avaiable */
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
} HDMI_EDID_INFO_T;


typedef struct {
	void (*set_util_funcs) (const HDMI_UTIL_FUNCS *util);
	void (*get_params) (HDMI_PARAMS *params);

	int (*init) (void);
	int (*enter) (void);
	int (*exit) (void);
	void (*suspend) (void);
	void (*resume) (void);
	int  (*audio_config)(HDMI_AUDIO_FORMAT aformat, int bitWidth);
	int  (*video_config)(HDMI_VIDEO_RESOLUTION vformat, HDMI_VIDEO_INPUT_FORMAT vin, HDMI_VIDEO_OUTPUT_FORMAT vou);
	int (*video_enable) (bool enable);
	int (*audio_enable) (bool enable);
	int (*irq_enable) (bool enable);
	int (*power_on) (void);
	void (*power_off) (void);
	 HDMI_STATE(*get_state) (void);
	void (*set_mode) (unsigned char ucMode);
	void (*dump) (void);
	int (*get_external_device_capablity)(void);
    void (*force_on)(int from_uart_drv);
#if !defined(CONFIG_MTK_MT8193_HDMI_SUPPORT)
	void (*read) (unsigned char u8Reg);
	void (*write) (unsigned char u8Reg, unsigned char u8Data);
	void (*log_enable) (bool enable);
	void (*getedid) (void *pv_get_info);
#else
	void (*read) (u16 u2Reg, u32 *p4Data);
	void (*write) (u16 u2Reg, u32 u4Data);
	void (*log_enable) (u16 enable);
	void (*InfoframeSetting) (u8 i1typemode, u8 i1typeselect);
	void (*checkedid) (u8 i1noedid);
	void (*colordeep) (u8 u1colorspace, u8 u1deepcolor);
	void (*enablehdcp) (u8 u1hdcponoff);
	void (*setcecrxmode) (u8 u1cecrxmode);
	void (*hdmistatus) (void);
	void (*hdcpkey) (u8 *pbhdcpkey);
	void (*getedid) (void *pv_get_info);
	void (*setcecla) (CEC_DRV_ADDR_CFG_T *prAddr);
	void (*sendsltdata) (u8 *pu1Data);
	void (*getceccmd) (CEC_FRAME_DESCRIPTION *frame);
	void (*getsltdata) (CEC_SLT_DATA *rCecSltData);
	void (*setceccmd) (CEC_SEND_MSG_T *msg);
	void (*cecenable) (u8 u1EnCec);
	void (*getcecaddr) (CEC_ADDRESS *cecaddr);
	void (*mutehdmi) (u8 u1flagvideomute, u8 u1flagaudiomute);
	 u8(*checkedidheader) (void);
#endif

} HDMI_DRIVER;


/* --------------------------------------------------------------------------- */
/* HDMI Driver Functions */
/* --------------------------------------------------------------------------- */

const HDMI_DRIVER *HDMI_GetDriver(void);

#endif				/* __HDMI_DRV_H__ */
