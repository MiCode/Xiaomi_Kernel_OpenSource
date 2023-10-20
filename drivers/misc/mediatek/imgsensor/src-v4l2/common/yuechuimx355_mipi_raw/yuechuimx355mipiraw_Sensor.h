/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *     YUECHUIMX355mipi_Sensor.h
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
#ifndef _YUECHUIMX355MIPI_SENSOR_H
#define _YUECHUIMX355MIPI_SENSOR_H

static kal_uint16 yuechuimx355_init_setting[] = {
	0x0136, 0x18,
	0x0137, 0x00,
	0x4348, 0x16,
	0x4350, 0x19,
	0x4408, 0x0A,
	0x440C, 0x0B,
	0x4411, 0x5F,
	0x4412, 0x2C,
	0x4623, 0x00,
	0x462C, 0x0F,
	0x462D, 0x00,
	0x462E, 0x00,
	0x4684, 0x54,
	0x480A, 0x07,
	0x4908, 0x07,
	0x4909, 0x07,
	0x490D, 0x0A,
	0x491E, 0x0F,
	0x4921, 0x06,
	0x4923, 0x28,
	0x4924, 0x28,
	0x4925, 0x29,
	0x4926, 0x29,
	0x4927, 0x1F,
	0x4928, 0x20,
	0x4929, 0x20,
	0x492A, 0x20,
	0x492C, 0x05,
	0x492D, 0x06,
	0x492E, 0x06,
	0x492F, 0x06,
	0x4930, 0x03,
	0x4931, 0x04,
	0x4932, 0x04,
	0x4933, 0x05,
	0x595E, 0x01,
	0x5963, 0x01,
	0x0808, 0x02,
	0x080A, 0x00,
	0x080B, 0xAF,
	0x080C, 0x00,
	0x080D, 0x2F,
	0x080E, 0x00,
	0x080F, 0x57,
	0x0810, 0x00,
	0x0811, 0x2F,
	0x0812, 0x00,
	0x0813, 0x2F,
	0x0814, 0x00,
	0x0815, 0x2F,
	0x0816, 0x00,
	0x0817, 0xBF,
	0x0818, 0x00,
	0x0819, 0x27,
	0x30A2, 0x00,
	0x30A3, 0xE3,
	0x30A0, 0x00,
	0x30A1, 0x0F,
};

static kal_uint16 yuechuimx355_preview_setting[] = {
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x03,
	0x0342, 0x0E,
	0x0343, 0x58,
	0x0340, 0x0A,
	0x0341, 0x36,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x0C,
	0x0349, 0xCF,
	0x034A, 0x09,
	0x034B, 0x9F,
	0x0220, 0x00,
	0x0222, 0x01,
	0x0900, 0x00,
	0x0901, 0x11,
	0x0902, 0x00,
	0x034C, 0x0C,
	0x034D, 0xD0,
	0x034E, 0x09,
	0x034F, 0xA0,
	0x0301, 0x05,
	0x0303, 0x01,
	0x0305, 0x02,
	0x0306, 0x00,
	0x0307, 0x78,
	0x030B, 0x01,
	0x030D, 0x03,
	0x030E, 0x00,
	0x030F, 0x5A,
	0x0310, 0x00,
	0x0700, 0x00,
	0x0701, 0x10,
	0x0820, 0x0B,
	0x0821, 0x40,
	0x3088, 0x04,
	0x6813, 0x02,
	0x6835, 0x07,
	0x6836, 0x00,
	0x6837, 0x04,
	0x684D, 0x07,
	0x684E, 0x00,
	0x684F, 0x04,
	0x0202, 0x0A,
	0x0203, 0x2C,
	0x0204, 0x00,
	0x0205, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
};

//same as preview
static kal_uint16 yuechuimx355_capture_setting[] = {};

static kal_uint16 yuechuimx355_normal_video_setting[] = {
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x03,
	0x0342, 0x0E,
	0x0343, 0x58,
	0x0340, 0x0A,
	0x0341, 0x36,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x01,
	0x0347, 0x38,
	0x0348, 0x0C,
	0x0349, 0xCF,
	0x034A, 0x08,
	0x034B, 0x67,
	0x0220, 0x00,
	0x0222, 0x01,
	0x0900, 0x00,
	0x0901, 0x11,
	0x0902, 0x00,
	0x034C, 0x0C,
	0x034D, 0xD0,
	0x034E, 0x07,
	0x034F, 0x30,
	0x0301, 0x05,
	0x0303, 0x01,
	0x0305, 0x02,
	0x0306, 0x00,
	0x0307, 0x78,
	0x030B, 0x01,
	0x030D, 0x03,
	0x030E, 0x00,
	0x030F, 0x5A,
	0x0310, 0x00,
	0x0700, 0x00,
	0x0701, 0x10,
	0x0820, 0x0B,
	0x0821, 0x40,
	0x3088, 0x04,
	0x6813, 0x02,
	0x6835, 0x07,
	0x6836, 0x00,
	0x6837, 0x04,
	0x684D, 0x07,
	0x684E, 0x00,
	0x684F, 0x04,
	0x0202, 0x0A,
	0x0203, 0x2C,
	0x0204, 0x00,
	0x0205, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
};

static kal_uint16 yuechuimx355_hs_video_setting[] = {};

//same as preview
static kal_uint16 yuechuimx355_slim_video_setting[] = {};

static kal_uint16 yuechuimx355_custom1_setting[] = {};

static kal_uint16 yuechuimx355_custom2_setting[] = {
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x03,
	0x0342, 0x0E,
	0x0343, 0x58,
	0x0340, 0x0A,
	0x0341, 0x36,
	0x0344, 0x01,
	0x0345, 0x18,
	0x0346, 0x00,
	0x0347, 0xD4,
	0x0348, 0x0B,
	0x0349, 0xB7,
	0x034A, 0x08,
	0x034B, 0xCB,
	0x0220, 0x00,
	0x0222, 0x01,
	0x0900, 0x00,
	0x0901, 0x11,
	0x0902, 0x00,
	0x034C, 0x0A,
	0x034D, 0xA0,
	0x034E, 0x07,
	0x034F, 0xF8,
	0x0301, 0x05,
	0x0303, 0x01,
	0x0305, 0x02,
	0x0306, 0x00,
	0x0307, 0x78,
	0x030B, 0x01,
	0x030D, 0x03,
	0x030E, 0x00,
	0x030F, 0x5A,
	0x0310, 0x00,
	0x0700, 0x00,
	0x0701, 0x10,
	0x0820, 0x0B,
	0x0821, 0x40,
	0x3088, 0x04,
	0x6813, 0x02,
	0x6835, 0x07,
	0x6836, 0x00,
	0x6837, 0x04,
	0x684D, 0x07,
	0x684E, 0x00,
	0x684F, 0x04,
	0x0202, 0x0A,
	0x0203, 0x2C,
	0x0204, 0x00,
	0x0205, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
};

static kal_uint16 yuechuimx355_custom3_setting[] = {
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x03,
	0x0342, 0x0E,
	0x0343, 0x58,
	0x0340, 0x0A,
	0x0341, 0x36,
	0x0344, 0x03,
	0x0345, 0xE8,
	0x0346, 0x02,
	0x0347, 0xF0,
	0x0348, 0x08,
	0x0349, 0xE7,
	0x034A, 0x06,
	0x034B, 0xAF,
	0x0220, 0x00,
	0x0222, 0x01,
	0x0900, 0x00,
	0x0901, 0x11,
	0x0902, 0x00,
	0x034C, 0x05,
	0x034D, 0x00,
	0x034E, 0x03,
	0x034F, 0xC0,
	0x0301, 0x05,
	0x0303, 0x01,
	0x0305, 0x02,
	0x0306, 0x00,
	0x0307, 0x78,
	0x030B, 0x01,
	0x030D, 0x03,
	0x030E, 0x00,
	0x030F, 0x5A,
	0x0310, 0x00,
	0x0700, 0x00,
	0x0701, 0x10,
	0x0820, 0x0B,
	0x0821, 0x40,
	0x3088, 0x04,
	0x6813, 0x02,
	0x6835, 0x07,
	0x6836, 0x00,
	0x6837, 0x04,
	0x684D, 0x07,
	0x684E, 0x00,
	0x684F, 0x04,
	0x0202, 0x0A,
	0x0203, 0x2C,
	0x0204, 0x00,
	0x0205, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
};

static kal_uint16 yuechuimx355_custom4_setting[] = {
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x03,
	0x0342, 0x07,
	0x0343, 0x2C,
	0x0340, 0x05,
	0x0341, 0x1A,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x01,
	0x0347, 0x30,
	0x0348, 0x0C,
	0x0349, 0xCF,
	0x034A, 0x08,
	0x034B, 0x6F,
	0x0220, 0x00,
	0x0222, 0x01,
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x00,
	0x034C, 0x06,
	0x034D, 0x68,
	0x034E, 0x03,
	0x034F, 0xA0,
	0x0301, 0x05,
	0x0303, 0x02,
	0x0305, 0x02,
	0x0306, 0x00,
	0x0307, 0x78,
	0x030B, 0x01,
	0x030D, 0x03,
	0x030E, 0x00,
	0x030F, 0x5A,
	0x0310, 0x00,
	0x0700, 0x03,
	0x0701, 0x48,
	0x0820, 0x0B,
	0x0821, 0x40,
	0x3088, 0x02,
	0x6813, 0x01,
	0x6835, 0x07,
	0x6836, 0x00,
	0x6837, 0x04,
	0x684D, 0x07,
	0x684E, 0x00,
	0x684F, 0x04,
	0x0202, 0x05,
	0x0203, 0x10,
	0x0204, 0x00,
	0x0205, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
};

static kal_uint16 yuechuimx355_custom5_setting[] = {};

static kal_uint16 yuechuimx355_custom6_setting[] = {};

static kal_uint16 yuechuimx355_custom7_setting[] = {};

static kal_uint16 yuechuimx355_custom8_setting[] = {};

static kal_uint16 yuechuimx355_custom9_setting[] = {};

static kal_uint16 yuechuimx355_custom10_setting[] = {};

static kal_uint16 yuechuimx355_Image_quality_setting[] = {};

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

struct SEAMLESS_SYS_DELAY {
	uint32_t source_scenario;
	uint32_t target_scenario;
	uint32_t sys_delay;
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
