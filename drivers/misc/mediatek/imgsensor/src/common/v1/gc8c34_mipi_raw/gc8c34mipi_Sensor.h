/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef _GC8C34MIPI_SENSOR_H
#define _GC8C34MIPI_SENSOR_H

#undef GC8C34_MIRROR_NORMAL
#undef GC8C34_MIRROR_H
#undef GC8C34_MIRROR_V
#undef GC8C34_MIRROR_HV
#define GC8C34_MIRROR_NORMAL

#if defined(GC8C34_MIRROR_NORMAL)
#define MIRROR			0xe8
#define BinStartY		0x06
#define BinStartX		0x05
#elif defined(GC8C34_MIRROR_H)
#define MIRROR			0xe9
#define BinStartY		0x06
#define BinStartX		0x05
#elif defined(GC8C34_MIRROR_V)
#define MIRROR			0xea
#define BinStartY		0x06
#define BinStartX		0x05
#elif defined(GC8C34_MIRROR_HV)
#define MIRROR			0xeb
#define BinStartY		0x06
#define BinStartX		0x05
#else
#define MIRROR			0xe8
#define BinStartY		0x06
#define BinStartX		0x05
#endif

struct gc_register {
	kal_uint8 addr;
	kal_uint8 value;
};

#define MAX_GAIN_MAP 10
#define AGC_REG_NUM 15

#define LSC_X_GRID 7
#define LSC_Y_GRID 11

struct agc_params_struct {
	kal_uint16	gain_level;
	struct gc_register	agc_register[AGC_REG_NUM];
};

#define SENSOR_BASE_GAIN	0x40
#define SENSOR_MAX_GAIN		(32*0x40)

#define GC8C34OTP_FOR_CUSTOMER
/* Please do not enable the following micro definition, if you are not debuging otp function */
#undef GC8C34OTP_DEBUG

#define OB_ROM_START		0x69
#define OB_WIDTH			0x06

#define DD_WIDTH            1642
#define DD_HEIGHT           1236

#if defined(GC8C34OTP_FOR_CUSTOMER)
#define RG_TYPICAL			0x045E
#define BG_TYPICAL			0x056D
#define AF_ROM_START		0x6b
#define AF_WIDTH			0x04
#define INFO_ROM_START		0x70
#define INFO_WIDTH			0x08
#define WB_ROM_START		0x5f
#define WB_WIDTH			0x04
#define GOLDEN_ROM_START	0x67
#define GOLDEN_WIDTH		0x04
#define LSC_NUM				77		/* 7x11 */
#endif

struct gc8c34_dd_t {
	kal_uint16 x;
	kal_uint16 y;
	kal_uint16 t;
};

struct gc8c34_otp {
	kal_uint8  dd_cnt;
	kal_uint8  dd_flag;
	kal_uint8  ob_flag;
	kal_int32  g_active;
	kal_int32  g_dark;

	struct gc8c34_dd_t dd_param[160];
	kal_uint8  reg_flag;
	kal_uint8  reg_num;
	kal_uint8  reg_page[10];
	kal_uint8  reg_addr[10];
	kal_uint8  reg_value[10];
#if defined(GC8C34OTP_FOR_CUSTOMER)
	kal_uint8  module_id;
	kal_uint8  lens_id;
	kal_uint8  vcm_id;
	kal_uint8  vcm_driver_id;
	kal_uint8  year;
	kal_uint8  month;
	kal_uint8  day;
	kal_uint8  af_flag;
	kal_uint16 af_infinity;
	kal_uint16 af_macro;
	kal_uint8  wb_flag;
	kal_uint16 rg_gain;
	kal_uint16 bg_gain;
	kal_uint8  golden_flag;
	kal_uint16 golden_rg;
	kal_uint16 golden_bg;
	kal_uint8  lsc_flag;		/* 0:Empty 1:Success 2:Invalid */
	kal_uint8  lsc_param[308];
#endif
};

enum {
	otp_close = 0,
	otp_open,
};

enum {
	IMGSENSOR_MODE_INIT,
	IMGSENSOR_MODE_PREVIEW,
	IMGSENSOR_MODE_CAPTURE,
	IMGSENSOR_MODE_VIDEO,
	IMGSENSOR_MODE_HIGH_SPEED_VIDEO,
	IMGSENSOR_MODE_SLIM_VIDEO,
};

struct imgsensor_mode_struct {
	kal_uint32 pclk;				/* record different mode's pclk */
	kal_uint32 linelength;			/* record different mode's linelength */
	kal_uint32
	framelength;			/* record different mode's framelength */

	kal_uint8 startx;				/* record different mode's startx of grabwindow */
	kal_uint8 starty;				/* record different mode's startx of grabwindow */

	kal_uint16
	grabwindow_width;	/* record different mode's width of grabwindow */
	kal_uint16
	grabwindow_height;	/* record different mode's height of grabwindow */

	/* following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario */
	kal_uint8 mipi_data_lp2hs_settle_dc;

	/* following for GetDefaultFramerateByScenario() */
	kal_uint32 mipi_pixel_rate;
	kal_uint16 max_framerate;
};

/* SENSOR PRIVATE STRUCT FOR VARIABLES*/
struct imgsensor_struct {
	kal_uint8 mirror;				/*mirrorflip information*/

	kal_uint8 sensor_mode;			/*record IMGSENSOR_MODE enum value*/

	kal_uint32 shutter;				/*current shutter*/
	kal_uint16 gain;                /*current gain*/

	kal_uint32 pclk;                /*current pclk*/

	kal_uint32 frame_length;		/*current framelength*/
	kal_uint32 line_length;			/*current linelength*/

	kal_uint32
	min_frame_length;	/*current min  framelength to max framerate*/
	kal_uint16 dummy_pixel;			/*current dummypixel*/
	kal_uint16 dummy_line;			/*current dummline*/

	kal_uint16 current_fps;			/*current max fps*/
	kal_bool autoflicker_en;		/*record autoflicker enable or disable*/
	kal_bool test_pattern;			/*record test pattern mode or not*/
	enum MSDK_SCENARIO_ID_ENUM
	current_scenario_id;/*current scenario id*/
	kal_bool  ihdr_en;				/*ihdr enable or disable*/

	kal_uint8 i2c_write_id;			/*record current sensor's i2c write id*/
};

/* SENSOR PRIVATE STRUCT FOR CONSTANT */
struct imgsensor_info_struct {
	kal_uint32
	sensor_id;				/* record sensor id defined in Kd_imgsensor.h */
	kal_uint32
	checksum_value;			/* checksum value for Camera Auto Test */
	struct imgsensor_mode_struct
		pre;	/* preview scenario relative information */
	struct imgsensor_mode_struct
		cap;	/* capture scenario relative information */
	struct imgsensor_mode_struct
		cap1;	/* capture for PIP 24fps relative information, */
	/* capture1 mode must use same framelength, */
	/* linelength with Capture mode for shutter calculate */
	struct imgsensor_mode_struct cap2;
	struct imgsensor_mode_struct
		normal_video;	/* normal video  scenario relative information */
	struct imgsensor_mode_struct
		hs_video;		/* high speed video scenario relative information */
	struct imgsensor_mode_struct
		slim_video;	/* slim video for VT scenario relative information */
	kal_uint8
	ae_shut_delay_frame;				/* shutter delay frame for AE cycle */
	kal_uint8
	ae_sensor_gain_delay_frame;		/* sensor gain delay frame for AE cycle */
	kal_uint8
	ae_ispGain_delay_frame;			/* isp gain delay frame for AE cycle */
	kal_uint8  ihdr_support;					/* 1, support; 0,not support */
	kal_uint8  ihdr_le_firstline;				/* 1,le first ; 0, se first */
	kal_uint8  sensor_mode_num;					/* support sensor mode num */
	kal_uint8
	cap_delay_frame;					/* enter capture delay frame num */
	kal_uint8
	pre_delay_frame;					/* enter preview delay frame num */
	kal_uint8
	video_delay_frame;				/* enter video delay frame num */
	kal_uint8
	hs_video_delay_frame;			/* enter high speed video  delay frame num */
	kal_uint8
	slim_video_delay_frame;			/* enter slim video delay frame num */
	kal_uint8
	margin;							/* sensor framelength & shutter margin */
	kal_uint32 min_shutter;						/* min shutter */
	kal_uint32
	max_frame_length;				/* max framelength by sensor register's limitation */
	kal_uint8  isp_driving_current;				/* mclk driving current */
	kal_uint8  sensor_interface_type;			/* sensor_interface_type */
	kal_uint8
	mipi_sensor_type;				/* 0,MIPI_OPHY_NCSI2; 1,MIPI_OPHY_CSI2 */
	/* default is NCSI2, don't modify this para */
	kal_uint8
	mipi_settle_delay_mode;			/* 0, high speed signal auto detect; 1, use settle delay,unit is ns */
	/* default is auto detect, don't modify this para */
	kal_uint8
	sensor_output_dataformat;		/* sensor output first pixel color */
	kal_uint8
	mclk;							/* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
	kal_uint8  mipi_lane_num;					/* mipi lane num */
	kal_uint8
	i2c_addr_table[5];				/* record sensor support all write id addr, only supprt 4must end with 0xff */
};

extern int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData,
		       u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData,
			u16 i2cId);
extern int iWriteReg(u16 a_u2Addr, u32 a_u4Data, u32 a_u4Bytes,
		     u16 i2cId);
#ifdef CONFIG_IMPROVE_CAMERA_PERFORMANCE
extern void kdSetI2CSpeed(u32 i2cSpeed);
#endif
#endif
