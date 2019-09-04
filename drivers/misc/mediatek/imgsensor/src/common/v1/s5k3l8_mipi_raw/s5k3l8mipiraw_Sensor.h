/*
 * Copyright (C) 2018 MediaTek Inc.
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

#ifndef _S5K3L8MIPIRAW_SENSOR_H
#define _S5K3L8MIPIRAW_SENSOR_H
#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

enum IMGSENSOR_MODE {
	IMGSENSOR_MODE_INIT,
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
};

struct imgsensor_mode_struct {
	kal_uint32 pclk; //record different mode's pclk
	kal_uint32 linelength; //record different mode's linelength
	kal_uint32 framelength; //record different mode's framelength

	kal_uint8 startx;
	kal_uint8 starty;

	kal_uint16 grabwindow_width;
	kal_uint16 grabwindow_height;

	kal_uint8 mipi_data_lp2hs_settle_dc;

	kal_uint32 mipi_pixel_rate;

	/*	 following for GetDefaultFramerateByScenario()	*/
	kal_uint16 max_framerate;

};

/* SENSOR PRIVATE STRUCT FOR VARIABLES*/
struct imgsensor_struct {
	kal_uint8 mirror;				//mirrorflip information

	kal_uint8 sensor_mode;

	kal_uint32 shutter;				//current shutter
	kal_uint16 gain;				//current gain

	kal_uint32 pclk;				//current pclk

	kal_uint32 frame_length;		//current framelength
	kal_uint32 line_length;			//current linelength

	kal_uint32 min_frame_length;
	kal_uint16 dummy_pixel;			//current dummypixel
	kal_uint16 dummy_line;			//current dummline

	kal_uint16 current_fps;			//current max fps
	kal_bool   autoflicker_en;
	kal_bool test_pattern;
	enum MSDK_SCENARIO_ID_ENUM current_scenario_id;
	kal_bool  ihdr_en;

	kal_uint8 i2c_write_id;
};

/* SENSOR PRIVATE STRUCT FOR CONSTANT*/
struct imgsensor_info_struct {
	kal_uint16 sensor_id;
	kal_uint32 checksum_value;
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

	kal_uint8  ae_shut_delay_frame; //shutter delay frame for AE cycle
	//sensor gain delay frame for AE cycle
	kal_uint8  ae_sensor_gain_delay_frame;
	//isp gain delay frame for AE cycle
	kal_uint8  ae_ispGain_delay_frame;
	kal_uint8  ihdr_support;		//1, support; 0,not support
	kal_uint8  ihdr_le_firstline;	//1,le first ; 0, se first
	kal_uint8  sensor_mode_num;		//support sensor mode num

	kal_uint8  cap_delay_frame; //enter capture delay frame num
	kal_uint8  pre_delay_frame; //enter preview delay frame num
	kal_uint8  video_delay_frame;	//enter video delay frame num
	//enter high speed video  delay frame num
	kal_uint8  hs_video_delay_frame;
	//enter slim video delay frame num
	kal_uint8  slim_video_delay_frame;
	kal_uint8  custom1_delay_frame; //enter custom1 delay frame num
	kal_uint8  custom2_delay_frame; //enter custom1 delay frame num
	kal_uint8  custom3_delay_frame; //enter custom1 delay frame num
	kal_uint8  custom4_delay_frame; //enter custom1 delay frame num
	kal_uint8  custom5_delay_frame; //enter custom1 delay frame num

	kal_uint8  margin; //sensor framelength & shutter margin
	kal_uint32 min_shutter;			//min shutter
//max framelength by sensor register's limitation
	kal_uint32 max_frame_length;

	kal_uint8  isp_driving_current;	//mclk driving current
	kal_uint8  sensor_interface_type;//sensor_interface_type
	kal_uint8  mipi_sensor_type;
	kal_uint8  mipi_settle_delay_mode;
	kal_uint8  sensor_output_dataformat;//sensor output first pixel color
	kal_uint8  mclk; //mclk value, suggest 24 or 26 for 24Mhz or 26Mhz

	kal_uint8  mipi_lane_num;		//mipi lane num
//record sensor support all write id addr, only supprt 4must end with 0xff
	kal_uint8  i2c_addr_table[5];
	kal_uint32  i2c_speed;     //i2c speed
};

extern int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData,
	u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId);
extern void kdSetI2CSpeed(u16 i2cSpeed);
extern bool S5K3L8_read_eeprom(kal_uint16 addr, BYTE *data, kal_uint32 size);

extern int iBurstWriteReg_multi(u8 *pData, u32 bytes, u16 i2cId,
	u16 transfer_length,	u16 timing);
#endif

