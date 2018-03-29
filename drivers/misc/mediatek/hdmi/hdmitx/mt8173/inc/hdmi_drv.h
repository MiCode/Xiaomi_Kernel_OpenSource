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

#ifndef __HDMI_DRV_H__
#define __HDMI_DRV_H__
#include "hdmitx.h"

#ifdef CONFIG_MTK_MT8193_HDMI_SUPPORT

#include "mt8193hdmictrl.h"
#include "mt8193edid.h"
#include "mt8193cec.h"
#include "hdmitable.h"

#define AVD_TMR_ISR_TICKS   10
#define MDI_BOUCING_TIMING  50	/* 20 //20ms */

enum HDMI_TASK_COMMAND_TYPE_T {
	HDMI_CEC_CMD = 0,
	HDMI_PLUG_DETECT_CMD,
	HDMI_HDCP_PROTOCAL_CMD,
	HDMI_DISABLE_HDMI_TASK_CMD,
	MAX_HDMI_TMR_NUMBER
};

#endif

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

/* #if !defined(CONFIG_MTK_MT8193_HDMI_SUPPORT) */
#if 0
enum HDMI_VIDEO_RESOLUTION {
	HDMI_VIDEO_720x480p_60Hz = 0,
	HDMI_VIDEO_1280x720p_60Hz = 2,
	HDMI_VIDEO_1920x1080p_30Hz = 6,
	HDMI_VIDEO_1920x1080p_60Hz = 0x0b,
	HDMI_VIDEO_RESOLUTION_NUM
};
#endif

enum HDMI_VIDEO_INPUT_FORMAT {
	HDMI_VIN_FORMAT_RGB565,
	HDMI_VIN_FORMAT_RGB666,
	HDMI_VIN_FORMAT_RGB888,
};

enum HDMI_VIDEO_OUTPUT_FORMAT {
	HDMI_VOUT_FORMAT_RGB888,
	HDMI_VOUT_FORMAT_YUV422,
	HDMI_VOUT_FORMAT_YUV444,
};

enum HDMI_AUDIO_FORMAT {
	HDMI_AUDIO_PCM_16bit_48000,
	HDMI_AUDIO_PCM_16bit_44100,
	HDMI_AUDIO_PCM_16bit_32000,
	HDMI_AUDIO_SOURCE_STREAM,
};

struct HDMI_CONFIG {
	HDMI_VIDEO_RESOLUTION vformat;
	enum HDMI_VIDEO_INPUT_FORMAT vin;
	enum HDMI_VIDEO_OUTPUT_FORMAT vout;
	enum HDMI_AUDIO_FORMAT aformat;
};

enum  HDMI_OUTPUT_MODE {
	HDMI_OUTPUT_MODE_LCD_MIRROR,
	HDMI_OUTPUT_MODE_VIDEO_MODE,
	HDMI_OUTPUT_MODE_DPI_BYPASS
};

enum HDMI_CABLE_TYPE {
	HDMI_CABLE,
	MHL_CABLE,
	MHL_SMB_CABLE,
	MHL_2_CABLE		/* /MHL 2.0 */
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
	struct HDMI_OUTPUT_MODE output_mode;

	int is_force_awake;
	int is_force_landscape;

	unsigned int scaling_factor;	/* determine the scaling of output screen size, valid value 0~10 */
	/* 0 means no scaling, 5 means scaling to 95%, 10 means 90% */
	enum HDMI_CABLE_TYPE cabletype;
	bool NeedSwHDCP;
	bool HDCPSupported;
};

enum HDMI_STATE {
	HDMI_STATE_NO_DEVICE,
	HDMI_STATE_ACTIVE,
	HDMI_STATE_CONNECTING,
	HDMI_STATE_PLUGIN_ONLY,
	HDMI_STATE_EDID_UPDATE,
	HDMI_STATE_CEC_UPDATE
};

/* --------------------------------------------------------------------------- */

struct HDMI_UTIL_FUNCS {
	void (*set_reset_pin)(unsigned int value);
	int (*set_gpio_out)(unsigned int gpio, unsigned int value);
	void (*udelay)(unsigned int us);
	void (*mdelay)(unsigned int ms);
	void (*wait_transfer_done)(void);
	void (*state_callback)(enum HDMI_STATE state);
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
#define SINK_1080P23976   (1 << 21)
#define SINK_1080P2997   (1 << 22)


struct HDMI_DRIVER {
	void (*set_util_funcs)(const struct HDMI_UTIL_FUNCS *util);
	void (*get_params)(struct HDMI_PARAMS *params);

	int (*init)(void);
	int (*enter)(void);
	int (*exit)(void);
	void (*suspend)(void);
	void (*resume)(void);
	int (*audio_config)(enum HDMI_AUDIO_FORMAT aformat);
	int (*video_config)(HDMI_VIDEO_RESOLUTION vformat, enum HDMI_VIDEO_INPUT_FORMAT vin,
			     enum HDMI_VIDEO_OUTPUT_FORMAT vou);
	int (*video_enable)(bool enable);
	int (*audio_enable)(bool enable);
	int (*irq_enable)(bool enable);
	int (*power_on)(void);
	void (*power_off)(void);
	enum HDMI_STATE (*get_state)(void);
	void (*set_mode)(unsigned char ucMode);
	void (*dump)(void);
#if !defined(CONFIG_MTK_MT8193_HDMI_SUPPORT)
	void (*read)(unsigned char u8Reg);
	void (*write)(unsigned char u8Reg, unsigned char u8Data);
	void (*log_enable)(bool enable);
	void (*getedid)(void *pv_get_info);
#else
	void (*read)(u16 u2Reg, u32 *p4Data);
	void (*write)(u16 u2Reg, u32 u4Data);
	void (*log_enable)(u16 enable);
	void (*InfoframeSetting)(u8 i1typemode, u8 i1typeselect);
	void (*checkedid)(u8 i1noedid);
	void (*colordeep)(u8 u1colorspace, u8 u1deepcolor);
	void (*enablehdcp)(u8 u1hdcponoff);
	void (*setcecrxmode)(u8 u1cecrxmode);
	void (*hdmistatus)(void);
	void (*hdcpkey)(u8 *pbhdcpkey);
	void (*getedid)(void *pv_get_info);
	void (*setcecla)(CEC_DRV_ADDR_CFG *prAddr);
	void (*sendsltdata)(u8 *pu1Data);
	void (*getceccmd)(CEC_FRAME_DESCRIPTION_IO *frame);
	void (*getsltdata)(CEC_SLT_DATA *rCecSltData);
	void (*setceccmd)(CEC_SEND_MSG *msg);
	void (*cecenable)(u8 u1EnCec);
	void (*getcecaddr)(CEC_ADDRESS_IO *cecaddr);
	void (*mutehdmi)(u8 u1flagvideomute, u8 u1flagaudiomute);
	 u8 (*checkedidheader)(void);
#endif
};


/* --------------------------------------------------------------------------- */
/* HDMI Driver Functions */
/* --------------------------------------------------------------------------- */

const struct HDMI_DRIVER *HDMI_GetDriver(void);

#endif				/* __HDMI_DRV_H__ */
