/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 imx214mipi_Sensor.h
 *
 * Project:
 * --------
 *	 ALPS
 *
 * Description:
 * ------------
 *	 CMOS sensor header file
 *
 ****************************************************************************/
#ifndef _IMX376MIPI_SENSOR_H
#define _IMX376MIPI_SENSOR_H


enum IMGSENSOR_MODE {
	IMGSENSOR_MODE_INIT,
	IMGSENSOR_MODE_PREVIEW,
	IMGSENSOR_MODE_CAPTURE,
	IMGSENSOR_MODE_VIDEO,
	IMGSENSOR_MODE_HIGH_SPEED_VIDEO,
	IMGSENSOR_MODE_SLIM_VIDEO,
};

struct imgsensor_mode_struct {
	kal_uint32 pclk;		/* record different mode's pclk */
	kal_uint32 linelength;	/* record different mode's linelength */
	kal_uint32 framelength;	/* record different mode's framelength */

	kal_uint8 startx; /* record different mode's startx of grabwindow */
	kal_uint8 starty; /* record different mode's startx of grabwindow */

	kal_uint16 grabwindow_width;
	kal_uint16 grabwindow_height;

	/* following for MIPIDataLowPwr2HighSpeedSettleDelayCount
	 *by different scenario
	 */
	kal_uint8 mipi_data_lp2hs_settle_dc;
	kal_uint32 mipi_pixel_rate; /* record the mipi pixel rate */

	/* following for GetDefaultFramerateByScenario() */
	kal_uint16 max_framerate;

};

/* SENSOR PRIVATE STRUCT FOR VARIABLES*/
struct imgsensor_struct {
	kal_uint8 mirror;	/* mirrorflip information */

	kal_uint8 sensor_mode;	/* record IMGSENSOR_MODE enum value */

	kal_uint32 shutter;	/* current shutter */
	kal_uint16 gain;	/* current gain */

	kal_uint32 pclk;	/* current pclk */

	kal_uint32 frame_length;	/* current framelength */
	kal_uint32 line_length;	/* current linelength */

	kal_uint32 min_frame_length;
	kal_uint16 dummy_pixel;	/* current dummypixel */
	kal_uint16 dummy_line;	/* current dummline */

	kal_uint16 current_fps;	/* current max fps */
	kal_bool   autoflicker_en; /* record autoflicker enable or disable */
	kal_bool test_pattern;	/* record test pattern mode or not */
	enum MSDK_SCENARIO_ID_ENUM current_scenario_id;/* current scenario id */
	kal_uint8  ihdr_mode;

	kal_uint8 i2c_write_id; /* record current sensor's i2c write id */
};

/* SENSOR PRIVATE STRUCT FOR CONSTANT*/
struct imgsensor_info_struct {
	kal_uint16 sensor_id;	/* record sensor id defined in Kd_imgsensor.h */

	/*zhaozhengtao 2016/02/19,modify for different module*/
	kal_uint16 module_id;

	kal_uint32 checksum_value; /* checksum value for Camera Auto Test */
	struct imgsensor_mode_struct pre;
	struct imgsensor_mode_struct cap;
	struct imgsensor_mode_struct cap1;
	struct imgsensor_mode_struct cap2;
	struct imgsensor_mode_struct normal_video;
	struct imgsensor_mode_struct hs_video;
	struct imgsensor_mode_struct slim_video;

	kal_uint8  ae_shut_delay_frame;	/* shutter delay frame for AE cycle */
	kal_uint8  ae_sensor_gain_delay_frame;
	kal_uint8  ae_ispGain_delay_frame;
	kal_uint8  ihdr_support;
	kal_uint8  ihdr_le_firstline;
	kal_uint8  sensor_mode_num;

	kal_uint8  cap_delay_frame;
	kal_uint8  pre_delay_frame;
	kal_uint8  video_delay_frame;
	kal_uint8  hs_video_delay_frame;
	kal_uint8  slim_video_delay_frame;

	kal_uint8  margin;
	kal_uint32 min_shutter;
	kal_uint32 max_frame_length;

	kal_uint8  isp_driving_current;	/* mclk driving current */
	kal_uint8  sensor_interface_type;/* sensor_interface_type */
	kal_uint8  mipi_sensor_type;
	kal_uint8  mipi_settle_delay_mode;
	kal_uint8  sensor_output_dataformat;
	kal_uint8  mclk;
	kal_uint32  i2c_speed;     /* i2c speed */
	kal_uint8  mipi_lane_num;		/* mipi lane num */
	kal_uint8  i2c_addr_table[5];
};

/* SENSOR READ/WRITE ID */
/* #define IMGSENSOR_WRITE_ID_1 (0x6c) */
/* #define IMGSENSOR_READ_ID_1  (0x6d) */
/* #define IMGSENSOR_WRITE_ID_2 (0x20) */
/* #define IMGSENSOR_READ_ID_2  (0x21) */

extern void kdSetI2CSpeed(u16 i2cSpeed);
extern int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData,
			u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId);
extern int iReadReg(u16 a_u2Addr, u8 *a_puBuff, u16 i2cId);
extern int iWriteReg(u16 a_u2Addr, u32 a_u4Data, u32 a_u4Bytes, u16 i2cId);
extern void kdSetI2CSpeed(u16 i2cSpeed);
extern int iBurstWriteReg_multi(u8 *pData, u32 bytes,
				u16 i2cId, u16 transfer_length, u16 timing);


extern struct mutex sensor_eeprom_lock;

/*extern bool read_3P3_eeprom( kal_uint16 addr, BYTE* data, kal_uint32 size);*/


#endif
