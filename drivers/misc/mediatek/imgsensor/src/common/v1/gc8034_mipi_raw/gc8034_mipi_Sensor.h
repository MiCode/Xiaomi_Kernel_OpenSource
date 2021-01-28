/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __GC8034MIPI_SENSOR_H__
#define __GC8034MIPI_SENSOR_H__

#undef  GC8034_MIRROR_NORMAL
#undef  GC8034_MIRROR_H
#undef  GC8034_MIRROR_V
#undef  GC8034_MIRROR_HV

#define GC8034_MIRROR_NORMAL

#if defined(GC8034_MIRROR_NORMAL)
#define GC8034_MIRROR         0xc0
#define GC8034_BinStartY      0x04
#define GC8034_BinStartX      0x05
#define GC8034_FullStartY     0x08
#define GC8034_FullStartX     0x09
#elif defined(GC8034_MIRROR_H)
#define GC8034_MIRROR         0xc1
#define GC8034_BinStartY      0x04
#define GC8034_BinStartX      0x05
#define GC8034_FullStartY     0x08
#define GC8034_FullStartX     0x0b
#elif defined(GC8034_MIRROR_V)
#define GC8034_MIRROR         0xc2
#define GC8034_BinStartY      0x04
#define GC8034_BinStartX      0x05
#define GC8034_FullStartY     0x08
#define GC8034_FullStartX     0x09
#elif defined(GC8034_MIRROR_HV)
#define GC8034_MIRROR         0xc3
#define GC8034_BinStartY      0x04
#define GC8034_BinStartX      0x05
#define GC8034_FullStartY     0x08
#define GC8034_FullStartX     0x0b
#else
#define GC8034_MIRROR         0xc0
#define GC8034_BinStartY      0x04
#define GC8034_BinStartX      0x05
#define GC8034_FullStartY     0x08
#define GC8034_FullStartX     0x09
#endif

enum {
	IMGSENSOR_MODE_INIT,
	IMGSENSOR_MODE_PREVIEW,
	IMGSENSOR_MODE_CAPTURE,
	IMGSENSOR_MODE_VIDEO,
	IMGSENSOR_MODE_HIGH_SPEED_VIDEO,
	IMGSENSOR_MODE_SLIM_VIDEO,
};

struct imgsensor_mode_struct {
	kal_uint32
	pclk;                    /*record different mode's pclk*/
	kal_uint32
	linelength;              /*record different mode's linelength*/
	kal_uint32
	framelength;             /*record different mode's framelength*/
	kal_uint8
	startx;                  /*record different mode's startx of grabwindow*/
	kal_uint8
	starty;                  /*record different mode's startx of grabwindow*/
	kal_uint16
	grabwindow_width;        /*record different mode's width of grabwindow*/
	kal_uint16
	grabwindow_height;       /*record different mode's height of grabwindow*/
	/* following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario */
	kal_uint8  mipi_data_lp2hs_settle_dc;
	kal_uint32  mipi_pixel_rate;
	/* following for GetDefaultFramerateByScenario() */
	kal_uint16 max_framerate;
};

/* SENSOR PRIVATE STRUCT FOR VARIABLES*/
struct imgsensor_struct {
	kal_uint8
	mirror;                        /*mirrorflip information*/
	kal_uint8
	sensor_mode;                   /*record IMGSENSOR_MODE enum value*/
	kal_uint32 shutter;                       /*current shutter*/
	kal_uint16 gain;                          /*current gain*/
	kal_uint32 pclk;                          /*current pclk*/
	kal_uint32 frame_length;                  /*current framelength*/
	kal_uint32 line_length;                   /*current linelength*/
	kal_uint32
	min_frame_length;              /*current min  framelength to max framerate*/
	kal_uint16 dummy_pixel;                   /*current dummypixel*/
	kal_uint16 dummy_line;                    /*current dummline*/
	kal_uint16 current_fps;                   /*current max fps*/
	kal_bool
	autoflicker_en;                /*record autoflicker enable or disable*/
	kal_bool
	test_pattern;                  /*record test pattern mode or not*/
	enum MSDK_SCENARIO_ID_ENUM
	current_scenario_id;/*current scenario id*/
	kal_bool
	ihdr_en;                       /*ihdr enable or disable*/
	kal_uint8
	i2c_write_id;                  /*record current sensor's i2c write id*/
};

/* SENSOR PRIVATE STRUCT FOR CONSTANT*/
struct imgsensor_info_struct {
	kal_uint32
	sensor_id;                      /*record sensor id defined in Kd_imgsensor.h*/
	kal_uint32
	checksum_value;                 /*checksum value for Camera Auto Test*/
	struct imgsensor_mode_struct
		pre;          /*preview scenario relative information*/
	struct imgsensor_mode_struct
		cap;          /*capture scenario relative information*/
	struct imgsensor_mode_struct cap1;
	/*capture for PIP 24fps relative information, */
	/*capture1 mode must use same framelength, */
	/*linelength with Capture mode for shutter calculate*/
	struct imgsensor_mode_struct cap2;
	struct imgsensor_mode_struct
		normal_video; /*normal video  scenario relative information*/
	struct imgsensor_mode_struct
		hs_video;     /*high speed video scenario relative information*/
	struct imgsensor_mode_struct
		slim_video;   /*slim video for VT scenario relative information*/
	kal_uint8
	ae_shut_delay_frame;            /*shutter delay frame for AE cycle*/
	kal_uint8
	ae_sensor_gain_delay_frame;     /*sensor gain delay frame for AE cycle*/
	kal_uint8
	ae_ispGain_delay_frame;         /*isp gain delay frame for AE cycle*/
	kal_uint8
	ihdr_support;                   /*1, support; 0,not support*/
	kal_uint8
	ihdr_le_firstline;              /*1,le first ; 0, se first*/
	kal_uint8
	sensor_mode_num;                /*support sensor mode num*/
	kal_uint8
	cap_delay_frame;                /*enter capture delay frame num*/
	kal_uint8
	pre_delay_frame;                /*enter preview delay frame num*/
	kal_uint8
	video_delay_frame;              /*enter video delay frame num*/
	kal_uint8
	hs_video_delay_frame;           /*enter high speed video  delay frame num*/
	kal_uint8
	slim_video_delay_frame;         /*enter slim video delay frame num*/
	kal_uint8
	margin;                         /*sensor framelength & shutter margin*/
	kal_uint32 min_shutter;                    /*min shutter*/
	kal_uint32
	max_frame_length;               /*max framelength by sensor register's limitation*/
	kal_uint8
	isp_driving_current;            /*mclk driving current*/
	kal_uint8
	sensor_interface_type;          /*sensor_interface_type*/
	kal_uint8  mipi_sensor_type;
	/*0,MIPI_OPHY_NCSI2; 1,MIPI_OPHY_CSI2, default is NCSI2, don't modify this para*/
	/*don't modify this para*/
	kal_uint8  mipi_settle_delay_mode;
	/*0, high speed signal auto detect; 1, use settle delay,unit is ns, */
	kal_uint8
	sensor_output_dataformat;       /*sensor output first pixel color*/
	kal_uint8
	mclk;                           /*mclk value, suggest 24 or 26 for 24Mhz or 26Mhz*/
	kal_uint8  mipi_lane_num;                  /*mipi lane num*/
	kal_uint8  i2c_addr_table[5];
	/*record sensor support all write id addr, only supprt 4must end with 0xff*/
};


#define SENSOR_BASE_GAIN       0x40
#define SENSOR_MAX_GAIN        (16 * SENSOR_BASE_GAIN)
#define MAX_AG_INDEX           9
#define MEAG_INDEX             7
#define AGC_REG_NUM            14

#define GC8034OTP_FOR_CUSTOMER
/* Please do not enable the following micro definition, if you are not debuging otp function*/
#undef GC8034OTP_DEBUG

#define DD_WIDTH            3284
#define DD_HEIGHT           2464
#if defined(GC8034OTP_FOR_CUSTOMER)
#define RG_TYPICAL          0x0400
#define BG_TYPICAL          0x0400
#define AF_ROM_START        0x3b
#define AF_WIDTH            0x04
#define INFO_ROM_START      0x70
#define INFO_WIDTH          0x08
#define WB_ROM_START        0x5f
#define WB_WIDTH            0x04
#define GOLDEN_ROM_START    0x67
#define GOLDEN_WIDTH        0x04
#define LSC_NUM             99		/* (7+2)*(9+2) */
#endif

struct gc8034_dd_t {
	kal_uint16 x;
	kal_uint16 y;
	kal_uint16 t;
};

struct gc8034_otp_t {
	kal_uint8  dd_cnt;
	kal_uint8  dd_flag;
	struct gc8034_dd_t dd_param[160];
	kal_uint8  reg_flag;
	kal_uint8  reg_num;
	kal_uint8  reg_page[10];
	kal_uint8  reg_addr[10];
	kal_uint8  reg_value[10];
#if defined(GC8034OTP_FOR_CUSTOMER)
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
	kal_uint8  lsc_param[396];
#endif
};

enum {
	otp_close = 0,
	otp_open,
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

