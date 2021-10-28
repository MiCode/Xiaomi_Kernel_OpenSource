/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
/*****************************************************************************
 *
 * Filename:
 * ---------
 *     OV48B2Qmipiraw_Sensor.h
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
#ifndef _OV48B2QMIPIRAW_SENSOR_H
#define _OV48B2QMIPIRAW_SENSOR_H

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define_v4l2.h"
#include "kd_imgsensor_errcode.h"

#include "adaptor-subdrv.h"
#include "adaptor-i2c.h"
#include "adaptor.h"

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
	IMGSENSOR_MODE_CUSTOM6,
	IMGSENSOR_MODE_CUSTOM7,
	IMGSENSOR_MODE_CUSTOM8,
	IMGSENSOR_MODE_CUSTOM9,
	IMGSENSOR_MODE_CUSTOM10,
	IMGSENSOR_MODE_CUSTOM11,
	IMGSENSOR_MODE_CUSTOM12,
};

struct imgsensor_mode_struct {
	kal_uint32 pclk;
	kal_uint32 linelength;
	kal_uint32 framelength;

	kal_uint8 startx;
	kal_uint8 starty;

	kal_uint16 grabwindow_width;
	kal_uint16 grabwindow_height;

	/* MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario */
	kal_uint8 mipi_data_lp2hs_settle_dc;

	/*	 following for GetDefaultFramerateByScenario()	*/
	kal_uint16 max_framerate;
	kal_uint32 mipi_pixel_rate;
};

/* SENSOR PRIVATE STRUCT FOR VARIABLES*/
struct imgsensor_struct {
	kal_uint8 mirror;				//mirrorflip information

	kal_uint8 sensor_mode; //record IMGSENSOR_MODE enum value

	kal_uint32 shutter;				//current shutter
	kal_uint16 gain;				//current gain

	kal_uint32 pclk;				//current pclk

	kal_uint32 frame_length;		//current framelength
	kal_uint32 line_length;			//current linelength

	kal_uint32 min_frame_length; //current min  framelength to max
	kal_int32 dummy_pixel;			//current dummypixel
	kal_int32 dummy_line;			//current dummline
	kal_uint16 current_fps;			//current max fps
	kal_bool   autoflicker_en; //record autoflicker enable or disable
	kal_bool test_pattern; //record test pattern mode or not
	enum MSDK_SCENARIO_ID_ENUM current_scenario_id;//current scenario id
	kal_uint8  ihdr_en;				//ihdr enable or disable
	kal_uint8  pdaf_mode;				//ihdr enable or disable
	kal_uint8 i2c_write_id; //record current sensor's i2c write id
};

/* SENSOR PRIVATE STRUCT FOR CONSTANT*/
struct imgsensor_info_struct {
	kal_uint32 sensor_id; //record sensor id defined in Kd_imgsensor.h
	kal_uint32 checksum_value; //checksum value for Camera Auto Test
	struct imgsensor_mode_struct pre; //preview scenario information
	struct imgsensor_mode_struct cap; //capture scenario  information
	struct imgsensor_mode_struct cap1; //capture for PIP 24fps info
	//normal video  scenario  information
	struct imgsensor_mode_struct normal_video;
	//high speed video scenario  information
	struct imgsensor_mode_struct hs_video;
	//slim video for VT scenario  information
	struct imgsensor_mode_struct slim_video;
	struct imgsensor_mode_struct custom1; //custom1 scenario information
	struct imgsensor_mode_struct custom2; //custom2 scenario information
	struct imgsensor_mode_struct custom3; //custom3 scenario information
	struct imgsensor_mode_struct custom4; //custom4 scenario information
	struct imgsensor_mode_struct custom5; //custom5 scenario information
	struct imgsensor_mode_struct custom6; //custom6 scenario information
	struct imgsensor_mode_struct custom7; //custom7 scenario information
	struct imgsensor_mode_struct custom8; //custom8 scenario information
	struct imgsensor_mode_struct custom9; //custom9 scenario information
	struct imgsensor_mode_struct custom10; //custom10 scenario information
	struct imgsensor_mode_struct custom11; //custom11 scenario information
	struct imgsensor_mode_struct custom12; //custom12 scenario information

	kal_uint8  ae_shut_delay_frame;	//shutter delay frame for AE cycle
	//sensor gain delay frame for AE cycle
	kal_uint8  ae_sensor_gain_delay_frame;
	kal_uint8  ae_ispGain_delay_frame; //ispgaindelayframe for AEcycle
	kal_uint8  ihdr_support;		//1, support; 0,not support
	kal_uint8  ihdr_le_firstline;	//1,le first ; 0, se first
	kal_uint8  sensor_mode_num;		//support sensor mode num

	kal_uint8  cap_delay_frame; //enter capture delay frame num
	kal_uint8  pre_delay_frame; //enter preview delay frame num
	kal_uint8  video_delay_frame; //enter video delay frame num
	kal_uint8  hs_video_delay_frame; //enter hspeedvideo delayframe num
	kal_uint8  slim_video_delay_frame; //enter svideo delay frame num
	kal_uint8  custom1_delay_frame;     //enter custom1 delay frame num
	kal_uint8  custom2_delay_frame;     //enter custom2 delay frame num
	kal_uint8  custom3_delay_frame;     //enter custom3 delay frame num
	kal_uint8  custom4_delay_frame;     //enter custom4 delay frame num
	kal_uint8  custom5_delay_frame;     //enter custom5 delay frame num
	kal_uint8  custom6_delay_frame;     //enter custom6 delay frame num
	kal_uint8  custom7_delay_frame;     //enter custom7 delay frame num
	kal_uint8  custom8_delay_frame;     //enter custom8 delay frame num
	kal_uint8  custom9_delay_frame;     //enter custom9 delay frame num
	kal_uint8  custom10_delay_frame;     //enter custom10 delay frame num
	kal_uint8  custom11_delay_frame;     //enter custom11 delay frame num
	kal_uint8  custom12_delay_frame;     //enter custom12 delay frame num
	kal_uint8  frame_time_delay_frame;

	kal_uint8  margin; //sensor framelength & shutter margin
	kal_uint32 min_shutter; //min shutter
	kal_uint32 max_frame_length; //max framelength by sensor limitation

	kal_uint8  isp_driving_current;	//mclk driving current
	kal_uint8  sensor_interface_type;//sensor_interface_type

	kal_uint8  mipi_sensor_type;
	kal_uint8  mipi_settle_delay_mode;
	kal_uint8  sensor_output_dataformat;
	kal_uint8  mclk; //mclk value, suggest 24Mhz or 26Mhz

	kal_uint8  mipi_lane_num;		//mipi lane num
	kal_uint8  xtalk_flag;
	kal_uint8  i2c_addr_table[5];
	kal_uint32  i2c_speed; //khz
	kal_uint32 min_gain;
	kal_uint32 max_gain;
	kal_uint32 min_gain_iso;
	kal_uint32 gain_step;
	kal_uint32 gain_type;
	kal_uint32 exp_step;

	struct v4l2_subdev *sd;
	struct adaptor_ctx *adaptor_ctx_;
};

extern int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData,
		u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iReadRegI2CTiming(u8 *a_pSendData, u16 a_sizeSendData,
		u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId, u16 timing);
extern int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId);
extern int iWriteRegI2CTiming(u8 *a_pSendData, u16 a_sizeSendData,
		u16 i2cId, u16 timing);
extern int iBurstWriteReg_multi(u8 *pData, u32 bytes, u16 i2cId,
		u16 transfer_length, u16 timing);

#endif
