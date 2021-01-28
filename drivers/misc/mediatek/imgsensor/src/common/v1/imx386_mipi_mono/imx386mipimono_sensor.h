/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _IMX386MIPI_SENSOR_H
#define _IMX386MIPI_SENSOR_H

/* preview capture video hvideo svideo */
enum {
	IMGSENSOR_MODE_INIT,
	IMGSENSOR_MODE_PREVIEW,
	IMGSENSOR_MODE_CAPTURE,
	IMGSENSOR_MODE_VIDEO,
	IMGSENSOR_MODE_HIGH_SPEED_VIDEO,
	IMGSENSOR_MODE_SLIM_VIDEO,
	IMGSENSOR_MODE_CUSTOM1,
	IMGSENSOR_MODE_CUSTOM2,
	IMGSENSOR_MODE_CUSTOM3,
};

struct imgsensor_mode_struct {
	kal_uint32 pclk;
	kal_uint32 linelength;
	kal_uint32 framelength;

	kal_uint8 startx;
	kal_uint8 starty;

	kal_uint16 grabwindow_width;
	kal_uint16 grabwindow_height;

	kal_uint8 mipi_data_lp2hs_settle_dc;

	kal_uint16 max_framerate;

};

/* SENSOR PRIVATE STRUCT FOR VARIABLES*/
struct imgsensor_struct {
	kal_uint8 mirror;
	kal_uint8 sensor_mode;
	kal_uint32 shutter;
	kal_uint16 gain;
	kal_uint32 pclk;
	kal_uint32 frame_length;
	kal_uint32 line_length;
	kal_uint32 min_frame_length;
	kal_uint16 dummy_pixel;
	kal_uint16 dummy_line;

	kal_uint16 current_fps;
	kal_bool   autoflicker_en;
	kal_bool test_pattern;
	enum MSDK_SCENARIO_ID_ENUM current_scenario_id;
	kal_bool  ihdr_en;

	kal_uint8 i2c_write_id;
};

/* SENSOR PRIVATE STRUCT FOR CONSTANT*/
struct imgsensor_info_struct {
	kal_uint32 sensor_id;
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

	kal_uint8  ae_shut_delay_frame;	/* shutter delay frame for AE cycle */
	kal_uint8  ae_sensor_gain_delay_frame;
	kal_uint8  ae_ispGain_delay_frame;
	kal_uint8  frame_time_delay_frame;
	kal_uint8  ihdr_support;		/* 1, support; 0,not support */
	kal_uint8  ihdr_le_firstline;	/* 1,le first ; 0, se first */
	kal_uint8  temperature_support;		/* 1, support; 0,not support */
	kal_uint8  sensor_mode_num;		/* support sensor mode num */

	kal_uint8  cap_delay_frame;
	kal_uint8  pre_delay_frame;
	kal_uint8  video_delay_frame;	/* enter video delay frame num */
	kal_uint8  hs_video_delay_frame;
	kal_uint8  slim_video_delay_frame;
	kal_uint8  custom1_delay_frame;
	kal_uint8  custom2_delay_frame;	 /* enter custom1 delay frame num */
	kal_uint8  custom3_delay_frame;

	kal_uint8  margin;
	kal_uint32 min_shutter;
	kal_uint32 max_frame_length;

	kal_uint8  isp_driving_current;	/* mclk driving current */
	kal_uint8  sensor_interface_type;/* sensor_interface_type */
	kal_uint8  mipi_sensor_type;

	kal_uint8  mipi_settle_delay_mode;
	kal_uint8  sensor_output_dataformat;
	kal_uint8  mclk;

	kal_uint8  mipi_lane_num;		/* mipi lane num */
	kal_uint8  i2c_addr_table[5];
	kal_uint32  i2c_speed;	 /* i2c speed */
};

extern int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData,
	u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId);
extern int iBurstWriteReg(u8 *pData, u32 bytes, u16 i2cId);

#endif
