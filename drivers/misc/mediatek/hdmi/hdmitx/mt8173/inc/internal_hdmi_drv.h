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

#ifndef _INTERNAL_HDMI_DRV_H__
#define _INTERNAL_HDMI_DRV_H__

#ifdef CONFIG_MTK_INTERNAL_HDMI_SUPPORT
#include "hdmitx.h"

#include "hdmictrl.h"
#include "hdmiedid.h"
#include "hdmicec.h"
#include <linux/types.h>
#include "hdmi_debug.h"
#define AVD_TMR_ISR_TICKS   10
#define MDI_BOUCING_TIMING  50	/* 20 //20ms */
#define RTPM_PRIO_CAMERA_PREVIEW 91
#define RTPM_PRIO_SCRN_UPDATE 94


#ifdef CONFIG_SLIMPORT_COLORADO3
#include "slimport_tx_drv.h"
#endif

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

enum HDMI_OUTPUT_MODE {
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
	enum HDMI_OUTPUT_MODE output_mode;

	int is_force_awake;
	int is_force_landscape;

	unsigned int scaling_factor;	/* determine the scaling of output screen size, valid value 0~10 */
	/* 0 means no scaling, 5 means scaling to 95%, 10 means 90% */
	bool NeedSwHDCP;
	enum HDMI_CABLE_TYPE cabletype;
};



enum HDMI_STATE {
	HDMI_STATE_NO_DEVICE,
	HDMI_STATE_ACTIVE,
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

/* --------------------------------------------------------------------------- */

struct HDMI_UTIL_FUNCS {
	void (*set_reset_pin)(unsigned int value);
	int (*set_gpio_out)(unsigned int gpio, unsigned int value);
	void (*udelay)(unsigned int us);
	void (*mdelay)(unsigned int ms);
	void (*wait_transfer_done)(void);
	void (*state_callback)(enum HDMI_STATE state);
	void (*cec_state_callback)(enum HDMI_CEC_STATE state);
};


struct HDMI_DRIVER {
	void (*set_util_funcs)(const struct HDMI_UTIL_FUNCS *util);
	void (*get_params)(struct HDMI_PARAMS *params);
	int (*hdmidrv_probe)(struct platform_device *pdev, unsigned long u8Res);
	int (*init)(void);
	int (*enter)(void);
	int (*exit)(void);
	void (*suspend)(void);
	void (*resume)(void);
	int (*audio_config)(enum HDMI_AUDIO_FORMAT aformat);
	int (*video_config)(HDMI_VIDEO_RESOLUTION vformat, enum HDMI_VIDEO_INPUT_FORMAT vin,
			     enum HDMI_VIDEO_OUTPUT_FORMAT vou);
	int (*video_enable)(unsigned char enable);
	int (*audio_enable)(unsigned char enable);
	int (*irq_enable)(unsigned char enable);
	int (*power_on)(void);
	void (*power_off)(void);
	enum HDMI_STATE (*get_state)(void);
	void (*set_mode)(unsigned char ucMode);
	void (*dump)(void);
	void (*read)(unsigned int u2Reg, unsigned int *p4Data);
	void (*write)(unsigned int u2Reg, unsigned int u4Data);
	void (*log_enable)(unsigned short enable);
	void (*InfoframeSetting)(unsigned char i1typemode, unsigned char i1typeselect);
	void (*checkedid)(unsigned char i1noedid);
	void (*colordeep)(unsigned char u1colorspace, unsigned char u1deepcolor);
	void (*enablehdcp)(unsigned char u1hdcponoff);
	void (*setcecrxmode)(unsigned char u1cecrxmode);
	void (*hdmistatus)(void);
	void (*hdcpkey)(unsigned char *pbhdcpkey);
	void (*getedid)(HDMI_EDID_T *pv_get_info);
	void (*setcecla)(CEC_DRV_ADDR_CFG *prAddr);
	void (*sendsltdata)(unsigned char *pu1Data);
	void (*getceccmd)(CEC_FRAME_DESCRIPTION_IO *frame);
	void (*getsltdata)(CEC_SLT_DATA *rCecSltData);
	void (*setceccmd)(CEC_SEND_MSG *msg);
	void (*getcectxstatus)(APK_CEC_ACK_INFO *pt);
	void (*cecenable)(unsigned char u1EnCec);
	void (*getcecaddr)(CEC_ADDRESS_IO *cecaddr);
	int (*audiosetting)(HDMITX_AUDIO_PARA *audio_para);
	int (*tmdsonoff)(unsigned char u1ionoff);
	void (*mutehdmi)(unsigned char u1flagvideomute, unsigned char u1flagaudiomute);
	void (*svpmutehdmi)(unsigned char u1svpvideomute, unsigned char u1svpaudiomute);
	void (*cecusrcmd)(unsigned int cmd, unsigned int *result);
	unsigned char (*checkedidheader)(void);
	/* void (*set3dstruct)(unsigned char b3DStruct); */
	unsigned int (*gethdmistatus)(void);
};


/* --------------------------------------------------------------------------- */
/* HDMI Driver Functions */
/* --------------------------------------------------------------------------- */
extern struct semaphore hdmi_update_mutex;
const struct HDMI_DRIVER *HDMI_GetDriver(void);
extern void hdmi_read(unsigned int u2Reg, unsigned int *p4Data);
extern void hdmi_write(unsigned int u2Reg, unsigned int u4Data);
void hdmi_clock_probe(struct platform_device *pdev);
void hdmi_clock_enable(bool bEnable);
void hdmi_poll_isr(unsigned long n);
void cec_poll_isr(unsigned long n);
extern unsigned int hdmi_irq;
extern u32 get_devinfo_with_index(u32 index);
#endif
