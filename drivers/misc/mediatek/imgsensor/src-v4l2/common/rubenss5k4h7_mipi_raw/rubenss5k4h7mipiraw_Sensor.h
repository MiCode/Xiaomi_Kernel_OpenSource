/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Copyright (C) 2022 XiaoMi, Inc.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *     RUBENSS5K4H7mipi_Sensor.h
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
#ifndef _RUBENSS5K4H7MIPI_SENSOR_H
#define _RUBENSS5K4H7MIPI_SENSOR_H


static kal_uint16 rubenss5k4h7_init_setting[] = {
	0x0100, 0x00,
	0x0B05, 0x01,
	0x3074, 0x06,
	0x3075, 0x2F,
	0x308A, 0x20,
	0x308B, 0x08,
	0x308C, 0x0B,
	0x3081, 0x07,
	0x307B, 0x85,
	0x307A, 0x0A,
	0x3079, 0x0A,
	0x306E, 0x71,
	0x306F, 0x28,
	0x301F, 0x20,
	0x306B, 0x9A,
	0x3091, 0x1F,
	0x30C4, 0x06,
	0x3200, 0x09,
	0x306A, 0x79,
	0x30B0, 0xFF,
	0x306D, 0x08,
	0x3080, 0x00,
	0x3929, 0x3F,
	0x3084, 0x16,
	0x3070, 0x0F,
	0x3B45, 0x01,
	0x30C2, 0x05,
	0x3069, 0x87,
	0x3924, 0x7F,
	0x3925, 0xFD,
	0x3C08, 0xFF,
	0x3C09, 0xFF,
	0x3C31, 0xFF,
	0x3C32, 0xFF,
};

static kal_uint16 rubenss5k4h7_preview_setting[] = {
	0x0136, 0x18,
	0x0137, 0x00,
	0x0305, 0x06,
	0x0306, 0x00,
	0x0307, 0x8C,
	0x030D, 0x06,
	0x030E, 0x00,
	0x030F, 0xA5,
	0x3C1F, 0x00,
	0x3C17, 0x00,
	0x3C1C, 0x05,
	0x3C1D, 0x15,
	0x0301, 0x04,
	0x0820, 0x02,
	0x0821, 0x94,
	0x0822, 0x00,
	0x0823, 0x00,
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x03,
	0x3906, 0x04,
	0x0344, 0x00,
	0x0345, 0x08,
	0x0346, 0x00,
	0x0347, 0x08,
	0x0348, 0x0C,
	0x0349, 0xC7,
	0x034A, 0x09,
	0x034B, 0x97,
	0x034C, 0x0C,
	0x034D, 0xC0,
	0x034E, 0x09,
	0x034F, 0x90,
	0x0900, 0x00,
	0x0901, 0x00,
	0x0381, 0x01,
	0x0383, 0x01,
	0x0385, 0x01,
	0x0387, 0x01,
	0x0101, 0x00,
	0x0340, 0x09,
	0x0341, 0xE2,
	0x0342, 0x0E,
	0x0343, 0x68,
	0x0200, 0x0D,
	0x0201, 0xD8,
	0x0202, 0x00,
	0x0203, 0x02,
	0x3931, 0x02,
	0x3400, 0x01,
};

//same preview
static kal_uint16 rubenss5k4h7_capture_setting[] = {

};

static kal_uint16 rubenss5k4h7_normal_video_setting[] = {
	0x0136, 0x18,
	0x0137, 0x00,
	0x0305, 0x06,
	0x0306, 0x00,
	0x0307, 0x8C,
	0x030D, 0x06,
	0x030E, 0x00,
	0x030F, 0xA5,
	0x3C1F, 0x00,
	0x3C17, 0x00,
	0x3C1C, 0x05,
	0x3C1D, 0x15,
	0x0301, 0x04,
	0x0820, 0x02,
	0x0821, 0x94,
	0x0822, 0x00,
	0x0823, 0x00,
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x03,
	0x3906, 0x04,
	0x0344, 0x00,
	0x0345, 0x08,
	0x0346, 0x01,
	0x0347, 0x3A,
	0x0348, 0x0C,
	0x0349, 0xC7,
	0x034A, 0x08,
	0x034B, 0x65,
	0x034C, 0x0C,
	0x034D, 0xC0,
	0x034E, 0x07,
	0x034F, 0x2C,
	0x0900, 0x00,
	0x0901, 0x00,
	0x0381, 0x01,
	0x0383, 0x01,
	0x0385, 0x01,
	0x0387, 0x01,
	0x0101, 0x00,
	0x0340, 0x09,
	0x0341, 0xE2,
	0x0342, 0x0E,
	0x0343, 0x68,
	0x0200, 0x0D,
	0x0201, 0xD8,
	0x0202, 0x00,
	0x0203, 0x02,
	0x3931, 0x02,
	0x3400, 0x01,
};

static kal_uint16 rubenss5k4h7_custom1_setting[] = {
	0x0136, 0x18,
	0x0137, 0x00,
	0x0305, 0x06,
	0x0306, 0x00,
	0x0307, 0x8C,
	0x030D, 0x06,
	0x030E, 0x00,
	0x030F, 0xA5,
	0x3C1F, 0x00,
	0x3C17, 0x00,
	0x3C1C, 0x05,
	0x3C1D, 0x15,
	0x0301, 0x04,
	0x0820, 0x02,
	0x0821, 0x94,
	0x0822, 0x00,
	0x0823, 0x00,
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x03,
	0x3906, 0x04,
	0x0344, 0x01,
	0x0345, 0xB8,
	0x0346, 0x01,
	0x0347, 0x4C,
	0x0348, 0x0B,
	0x0349, 0x17,
	0x034A, 0x08,
	0x034B, 0x53,
	0x034C, 0x09,
	0x034D, 0x60,
	0x034E, 0x07,
	0x034F, 0x08,
	0x0900, 0x00,
	0x0901, 0x00,
	0x0381, 0x01,
	0x0383, 0x01,
	0x0385, 0x01,
	0x0387, 0x01,
	0x0101, 0x00,
	0x0340, 0x09,
	0x0341, 0xE2,
	0x0342, 0x0E,
	0x0343, 0x68,
	0x0200, 0x0D,
	0x0201, 0xD8,
	0x0202, 0x00,
	0x0203, 0x02,
	0x3931, 0x02,
	0x3400, 0x01,
};

static kal_uint16 rubenss5k4h7_custom2_setting[] = {
	0x0136, 0x18,
	0x0137, 0x00,
	0x0305, 0x06,
	0x0306, 0x00,
	0x0307, 0x8C,
	0x030D, 0x06,
	0x030E, 0x00,
	0x030F, 0xA5,
	0x3C1F, 0x00,
	0x3C17, 0x00,
	0x3C1C, 0x05,
	0x3C1D, 0x15,
	0x0301, 0x04,
	0x0820, 0x02,
	0x0821, 0x94,
	0x0822, 0x00,
	0x0823, 0x00,
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x03,
	0x3906, 0x04,
	0x0344, 0x02,
	0x0345, 0x94,
	0x0346, 0x01,
	0x0347, 0xF0,
	0x0348, 0x0A,
	0x0349, 0x3B,
	0x034A, 0x07,
	0x034B, 0xAF,
	0x034C, 0x07,
	0x034D, 0xA8,
	0x034E, 0x05,
	0x034F, 0xC0,
	0x0900, 0x00,
	0x0901, 0x00,
	0x0381, 0x01,
	0x0383, 0x01,
	0x0385, 0x01,
	0x0387, 0x01,
	0x0101, 0x00,
	0x0340, 0x09,
	0x0341, 0xE2,
	0x0342, 0x0E,
	0x0343, 0x68,
	0x0200, 0x0D,
	0x0201, 0xD8,
	0x0202, 0x00,
	0x0203, 0x02,
	0x3931, 0x02,
	0x3400, 0x01,
};


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
	struct imgsensor_mode_struct cap1;
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
