/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Copyright (C) 2022 XiaoMi, Inc.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *     XAGAOV02B10mipi_Sensor.h
 *
 * Project:
 * --------
 *     ALPS
 *
 * Description:
 * ------------
 *     CMOS sensor header file
 *
 ****************************************************************************/
#ifndef _XAGAOV02B10MIPI_SENSOR_H
#define _XAGAOV02B10MIPI_SENSOR_H

static kal_uint16 xagaov02b10_init_setting[] = {
// base on A01d
//	0xfc, 0x01,
	0xfd, 0x00,
	0xfd, 0x00,
	0x24, 0x02,
	0x25, 0x06,
	0x29, 0x01,
	0x2a, 0xb4,
	0x2b, 0x00,
	0x1e, 0x17,
	0x33, 0x07,
	0x35, 0x07,
	0x4a, 0x0c,
	0x3a, 0x05,
	0x3b, 0x02,
	0x3e, 0x00,
	0x46, 0x01,
	0x6d, 0x03,
	0xfd, 0x01,
	0x0e, 0x02,
	0x0f, 0x1a,
	0x18, 0x00,
	0x22, 0xff,
	0x23, 0x02,
	0x17, 0x2c,
	0x19, 0x20,
	0x1b, 0x06,
	0x1c, 0x04,
	0x20, 0x03,
	0x30, 0x01,
	0x33, 0x01,
	0x31, 0x0a,
	0x32, 0x09,
	0x38, 0x01,
	0x39, 0x01,
	0x3a, 0x01,
	0x3b, 0x01,
	0x4f, 0x04,
	0x4e, 0x05,
	0x50, 0x01,
	0x35, 0x0c,
	0x45, 0x2a,
	0x46, 0x2a,
	0x47, 0x2a,
	0x48, 0x2a,
	0x4a, 0x2c,
	0x4b, 0x2c,
	0x4c, 0x2c,
	0x4d, 0x2c,
	0x56, 0x3a,
	0x57, 0x0a,
	0x58, 0x24,
	0x59, 0x20,
	0x5a, 0x0a,
	0x5b, 0xff,
	0x37, 0x0a,
	0x42, 0x0e,
	0x68, 0x90,
	0x69, 0xcd,
	0x6a, 0x8f,
	0x7c, 0x0a,
	0x7d, 0x09,
	0x7e, 0x09,
	0x7f, 0x08,
	0x83, 0x14,
	0x84, 0x14,
	0x86, 0x14,
	0x87, 0x07,
	0x88, 0x0f,
	0x94, 0x02,
	0x98, 0xd1,
	0xfe, 0x02,
	0xfd, 0x03,
	0x97, 0x78,
	0x98, 0x78,
	0x99, 0x78,
	0x9a, 0x78,
	0xa1, 0x40,
	0xb1, 0x30,
	0xae, 0x0d,
	0x88, 0x5b,
	0x89, 0x7c,
	0xb4, 0x05,
	0x8c, 0x40,
	0x8e, 0x40,
	0x90, 0x40,
	0x92, 0x40,
	0x9b, 0x46,
	0xac, 0x40,
	0xfd, 0x00,
	0x5a, 0x15,
	0x74, 0x01,
	0xfd, 0x00,
	0x50, 0x40,
	0x52, 0xb0,
	0xfd, 0x01,
	0x03, 0x70,
	0x05, 0x10,
	0x07, 0x20,
	0x09, 0xb0,
	0xfd, 0x03,
	0xfb, 0x01,
	0xfd, 0x01,
};


static kal_uint16 xagaov02b10_preview_setting[] = {
	0xfd, 0x01,
	0x14, 0x00,
	0x15, 0x01,
	0xfe, 0x02,
};

//same preview
static kal_uint16 xagaov02b10_capture_setting[] = {};

//4000x2252@30fps
static kal_uint16 xagaov02b10_normal_video_setting[] = {};

//1080P@240fps
static kal_uint16 xagaov02b10_hs_video_setting[] = {};

//same preview
static kal_uint16 xagaov02b10_slim_video_setting[] = {};

//same preview
static kal_uint16 xagaov02b10_custom1_setting[] = {};

//1080P@120fps
static kal_uint16 xagaov02b10_custom2_setting[] = {};

//4000x2252@60fps
static kal_uint16 xagaov02b10_custom3_setting[] = {};

// 12000x9000@10fps
static kal_uint16 xagaov02b10_custom4_setting[] = {};

// Full size crop 12M
static kal_uint16 xagaov02b10_custom5_setting[] = {};

// 4000 x 3000@24fps
static kal_uint16 xagaov02b10_custom6_setting[] = {};


enum IMGSENSOR_MODE {
	IMGSENSOR_MODE_PREVIEW,
	IMGSENSOR_MODE_CAPTURE,
	IMGSENSOR_MODE_VIDEO,
	IMGSENSOR_MODE_HIGH_SPEED_VIDEO,
	IMGSENSOR_MODE_SLIM_VIDEO,
	IMGSENSOR_MODE_CUSTOM1,
	IMGSENSOR_MODE_CUSTOM2,
	IMGSENSOR_MODE_CUSTOM3,
	IMGSENSOR_MODE_CUSTOM4,
	IMGSENSOR_MODE_CUSTOM5,
	IMGSENSOR_MODE_CUSTOM6,
	IMGSENSOR_MODE_CUSTOM7,
	IMGSENSOR_MODE_CUSTOM8,
	IMGSENSOR_MODE_CUSTOM9,
	IMGSENSOR_MODE_CUSTOM10,
	IMGSENSOR_MODE_CUSTOM11,
	IMGSENSOR_MODE_CUSTOM12,
	IMGSENSOR_MODE_CUSTOM13,
	IMGSENSOR_MODE_CUSTOM14,
	IMGSENSOR_MODE_CUSTOM15,
	IMGSENSOR_MODE_INIT = 0xff,
};

enum {
	OTP_QSC_NONE = 0x0,
	OTP_QSC_INTERNAL,
	OTP_QSC_CUSTOM,
};

struct imgsensor_mode_struct {
	kal_uint32 pclk; /* record different mode's pclk */
	kal_uint32 linelength; /* record different mode's linelength */
	kal_uint32 framelength; /* record different mode's framelength */

	kal_uint8 startx; /* record different mode's startx of grabwindow */
	kal_uint8 starty; /* record different mode's startx of grabwindow */

	kal_uint16 grabwindow_width;
	kal_uint16 grabwindow_height;

/* for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario*/
	kal_uint8 mipi_data_lp2hs_settle_dc;

 /*     following for GetDefaultFramerateByScenario()    */
	kal_uint16 max_framerate;
	kal_uint32 mipi_pixel_rate;
};

/* SENSOR PRIVATE STRUCT FOR VARIABLES*/
struct imgsensor_struct {
	kal_uint8 mirror; /* mirrorflip information */

	kal_uint8 sensor_mode; /* record IMGSENSOR_MODE enum value */

	kal_uint32 shutter; /* current shutter */
	kal_uint16 gain; /* current gain */

	kal_uint32 pclk; /* current pclk */

	kal_uint32 frame_length; /* current framelength */
	kal_uint32 line_length; /* current linelength */

	kal_uint32 min_frame_length;
	kal_uint16 dummy_pixel; /* current dummypixel */
	kal_uint16 dummy_line; /* current dummline */

	kal_uint16 current_fps; /* current max fps */
	kal_bool autoflicker_en; /* record autoflicker enable or disable */
	kal_bool test_pattern; /* record test pattern mode or not */
	enum MSDK_SCENARIO_ID_ENUM current_scenario_id;
	kal_bool ihdr_en; /* ihdr enable or disable */
	kal_uint8 ihdr_mode; /* ihdr enable or disable */
	kal_uint8 pdaf_mode; /* ihdr enable or disable */
	kal_uint8 i2c_write_id; /* record current sensor's i2c write id */
};

/* SENSOR PRIVATE STRUCT FOR CONSTANT*/
struct imgsensor_info_struct {
	kal_uint32 sensor_id; /* record sensor id defined in Kd_imgsensor.h */
	kal_uint32 checksum_value; /* checksum value for Camera Auto Test */
	struct imgsensor_mode_struct pre;
	struct imgsensor_mode_struct cap;
	struct imgsensor_mode_struct normal_video;
	struct imgsensor_mode_struct hs_video;
	struct imgsensor_mode_struct slim_video;
	struct imgsensor_mode_struct custom1;
	struct imgsensor_mode_struct custom2;
	struct imgsensor_mode_struct custom3;
	struct imgsensor_mode_struct custom4;
	struct imgsensor_mode_struct custom5;
	struct imgsensor_mode_struct custom6;
	struct imgsensor_mode_struct custom7;
	struct imgsensor_mode_struct custom8;
	struct imgsensor_mode_struct custom9;
	struct imgsensor_mode_struct custom10;
	struct imgsensor_mode_struct custom11;
	struct imgsensor_mode_struct custom12;
	struct imgsensor_mode_struct custom13;
	struct imgsensor_mode_struct custom14;
	struct imgsensor_mode_struct custom15;

	kal_uint8 ae_shut_delay_frame; /* shutter delay frame for AE cycle */
	kal_uint8 ae_sensor_gain_delay_frame;
	kal_uint8 ae_ispGain_delay_frame;
	kal_uint8 ihdr_support; /* 1, support; 0,not support */
	kal_uint8 ihdr_le_firstline; /* 1,le first ; 0, se first */
	kal_uint8 temperature_support;	/* 1, support; 0,not support */
	kal_uint8 sensor_mode_num; /* support sensor mode num */

	kal_uint8 cap_delay_frame; /* enter capture delay frame num */
	kal_uint8 pre_delay_frame; /* enter preview delay frame num */
	kal_uint8 video_delay_frame; /* enter video delay frame num */
	kal_uint8 hs_video_delay_frame;
	kal_uint8 slim_video_delay_frame; /* enter slim video delay frame num */
	kal_uint8 custom1_delay_frame; /* enter custom1 delay frame num */
	kal_uint8 custom2_delay_frame; /* enter custom2 delay frame num */
	kal_uint8 custom3_delay_frame; /* enter custom3 delay frame num */
	kal_uint8 custom4_delay_frame; /* enter custom4 delay frame num */
	kal_uint8 custom5_delay_frame; /* enter custom5 delay frame num */
	kal_uint8 custom6_delay_frame; /* enter custom6 delay frame num */
	kal_uint8 custom7_delay_frame; /* enter custom7 delay frame num */
	kal_uint8 custom8_delay_frame; /* enter custom8 delay frame num */
	kal_uint8 custom9_delay_frame; /* enter custom9 delay frame num */
	kal_uint8 custom10_delay_frame; /* enter custom10 delay frame num */
	kal_uint8 custom11_delay_frame; /* enter custom11 delay frame num */
	kal_uint8 custom12_delay_frame; /* enter custom12 delay frame num */
	kal_uint8 custom13_delay_frame; /* enter custom13 delay frame num */
	kal_uint8 custom14_delay_frame; /* enter custom14 delay frame num */
	kal_uint8 custom15_delay_frame; /* enter custom15 delay frame num */

	kal_uint8  frame_time_delay_frame;
	kal_uint8 margin; /* sensor framelength & shutter margin */
	kal_uint32 min_shutter; /* min shutter */
	kal_uint32 max_frame_length;
	kal_uint32 min_gain;
	kal_uint32 max_gain;
	kal_uint32 min_gain_iso;
	kal_uint32 gain_step;
	kal_uint32 exp_step;
	kal_uint32 gain_type;
	kal_uint8 isp_driving_current; /* mclk driving current */
	kal_uint8 sensor_interface_type; /* sensor_interface_type */
	kal_uint8 mipi_sensor_type;
	/* 0,MIPI_OPHY_NCSI2; 1,MIPI_OPHY_CSI2, default is NCSI2,
	 * don't modify this para
	 */
	kal_uint8 mipi_settle_delay_mode;
	/* 0, high speed signal auto detect;
	 * 1, use settle delay,unit is ns,
	 * default is auto detect, don't modify this para
	 */
	kal_uint8 sensor_output_dataformat;
	kal_uint8 mclk;	 /* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
	kal_uint32 i2c_speed; /* i2c speed */
	kal_uint8 mipi_lane_num; /* mipi lane num */
	kal_uint8 i2c_addr_table[5];
};

/* SENSOR READ/WRITE ID */
/* #define IMGSENSOR_WRITE_ID_1 (0x6c) */
/* #define IMGSENSOR_READ_ID_1  (0x6d) */
/* #define IMGSENSOR_WRITE_ID_2 (0x20) */
/* #define IMGSENSOR_READ_ID_2  (0x21) */

extern int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData,
	u8 *a_pRecvData, u16 a_sizeRecvData,
		       u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId);

extern void read_imx230_eeprom(void);
int iBurstWriteReg_multi(u8 *pData, u32 bytes, u16 i2cId,
	u16 transfer_length, u16 timing);

extern int iReadReg(u16 a_u2Addr, u8 *a_puBuff, u16 i2cId);
extern int iWriteReg(u16 a_u2Addr, u32 a_u4Data, u32 a_u4Bytes, u16 i2cId);
extern void kdSetI2CSpeed(u16 i2cSpeed);
extern int iBurstWriteReg_multi(u8 *pData, u32 bytes, u16 i2cId,
				u16 transfer_length, u16 timing);
#endif
