/*
 * Copyright (C) 2018 MediaTek Inc.
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

/*****************************************************************************
 *
 * Filename:
 * ---------
 *     gc5035mipi_Sensor.h
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
#ifndef __GC5035MIPI_SENSOR_H__
#define __GC5035MIPI_SENSOR_H__

/* SENSOR MIRROR FLIP INFO */
#define GC5035_MIRROR_FLIP_ENABLE         0
#if GC5035_MIRROR_FLIP_ENABLE
#define GC5035_MIRROR                     0x83
#define GC5035_RSTDUMMY1                  0x03
#define GC5035_RSTDUMMY2                  0xfc
#else
#define GC5035_MIRROR                     0x80
#define GC5035_RSTDUMMY1                  0x02
#define GC5035_RSTDUMMY2                  0x7c
#endif

/* SENSOR PRIVATE INFO FOR GAIN SETTING */
#define GC5035_SENSOR_GAIN_BASE             256
#define GC5035_SENSOR_GAIN_MAX              (16 * GC5035_SENSOR_GAIN_BASE)
#define GC5035_SENSOR_GAIN_MAX_VALID_INDEX  17
#define GC5035_SENSOR_GAIN_MAP_SIZE         17
#define GC5035_SENSOR_DGAIN_BASE            0x100

/* SENSOR PRIVATE INFO FOR OTP SETTINGS */
#define GC5035_OTP_FOR_CUSTOMER            0
#define GC5035_OTP_DEBUG                   0

/* DEBUG */
#if GC5035_OTP_DEBUG
#define GC5035_OTP_START_ADDR              0x0000
#endif

#define GC5035_OTP_DATA_LENGTH             1024

/* OTP FLAG TYPE */
#define GC5035_OTP_FLAG_EMPTY              0x00
#define GC5035_OTP_FLAG_VALID              0x01
#define GC5035_OTP_FLAG_INVALID            0x02
#define GC5035_OTP_FLAG_INVALID2           0x03
#define GC5035_OTP_GET_OFFSET(size)           (size << 3)
#define GC5035_OTP_GET_2BIT_FLAG(flag, bit)   ((flag >> bit) & 0x03)
#define GC5035_OTP_CHECK_1BIT_FLAG(flag, bit) \
	(((flag >> bit) & 0x01) == GC5035_OTP_FLAG_VALID)

#define GC5035_OTP_ID_SIZE                 9
#define GC5035_OTP_ID_DATA_OFFSET          0x0020

/* OTP DPC PARAMETERS */
#define GC5035_OTP_DPC_FLAG_OFFSET         0x0068
#define GC5035_OTP_DPC_TOTAL_NUMBER_OFFSET 0x0070
#define GC5035_OTP_DPC_ERROR_NUMBER_OFFSET 0x0078

/* OTP REGISTER UPDATE PARAMETERS */
#define GC5035_OTP_REG_FLAG_OFFSET         0x0880
#define GC5035_OTP_REG_DATA_OFFSET         0x0888
#define GC5035_OTP_REG_MAX_GROUP           5
#define GC5035_OTP_REG_BYTE_PER_GROUP      5
#define GC5035_OTP_REG_REG_PER_GROUP       2
#define GC5035_OTP_REG_BYTE_PER_REG        2
#define GC5035_OTP_REG_DATA_SIZE \
	(GC5035_OTP_REG_MAX_GROUP * GC5035_OTP_REG_BYTE_PER_GROUP)
#define GC5035_OTP_REG_REG_SIZE \
	(GC5035_OTP_REG_MAX_GROUP * GC5035_OTP_REG_REG_PER_GROUP)

#if GC5035_OTP_FOR_CUSTOMER
#define GC5035_OTP_CHECK_SUM_BYTE          1
#define GC5035_OTP_GROUP_CNT               2

/* OTP MODULE INFO PARAMETERS */
#define GC5035_OTP_MODULE_FLAG_OFFSET      0x1f10
#define GC5035_OTP_MODULE_DATA_OFFSET      0x1f18
#define GC5035_OTP_MODULE_DATA_SIZE        6

/* OTP WB PARAMETERS */
#define GC5035_OTP_WB_FLAG_OFFSET          0x1f78
#define GC5035_OTP_WB_DATA_OFFSET          0x1f80
#define GC5035_OTP_WB_DATA_SIZE            4
#define GC5035_OTP_GOLDEN_DATA_OFFSET      0x1fc0
#define GC5035_OTP_GOLDEN_DATA_SIZE        4
#define GC5035_OTP_WB_CAL_BASE             0x0800
#define GC5035_OTP_WB_GAIN_BASE            0x0400

/* WB TYPICAL VALUE 0.5 */
#define GC5035_OTP_WB_RG_TYPICAL           0x0400
#define GC5035_OTP_WB_BG_TYPICAL           0x0400
#endif

enum {
	otp_close = 0,
	otp_open,
};

/* DPC STRUCTURE */
struct gc5035_dpc_t {
	kal_uint8 flag;
	kal_uint16 total_num;
};

struct gc5035_reg_t {
	kal_uint8 page;
	kal_uint8 addr;
	kal_uint8 value;
};

/* REGISTER UPDATE STRUCTURE */
struct gc5035_reg_update_t {
	kal_uint8 flag;
	kal_uint8 cnt;
	struct gc5035_reg_t reg[GC5035_OTP_REG_REG_SIZE];
};

#if GC5035_OTP_FOR_CUSTOMER
/* MODULE INFO STRUCTURE */
struct gc5035_module_info_t {
	kal_uint8 module_id;
	kal_uint8 lens_id;
	kal_uint8 year;
	kal_uint8 month;
	kal_uint8 day;
};

/* WB STRUCTURE */
struct gc5035_wb_t {
	kal_uint8  flag;
	kal_uint16 rg;
	kal_uint16 bg;
};
#endif

/* OTP STRUCTURE */
struct gc5035_otp_t {
	kal_uint8 otp_id[GC5035_OTP_ID_SIZE];
	struct gc5035_dpc_t dpc;
	struct gc5035_reg_update_t regs;
#if GC5035_OTP_FOR_CUSTOMER
	struct gc5035_wb_t wb;
	struct gc5035_wb_t golden;
#endif
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
	kal_uint32 pclk;                /* record different mode's pclk */
	kal_uint32 linelength;          /* record different mode's linelength */
	kal_uint32 framelength;
	kal_uint8  startx;
	kal_uint8  starty;
	kal_uint16 grabwindow_width;
	kal_uint16 grabwindow_height;
	kal_uint32 mipi_pixel_rate;
	kal_uint8  mipi_data_lp2hs_settle_dc;
	/* following for GetDefaultFramerateByScenario() */
	kal_uint16 max_framerate;
};

/* SENSOR PRIVATE STRUCT FOR VARIABLES */
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
	kal_bool   test_pattern;
	enum MSDK_SCENARIO_ID_ENUM current_scenario_id;
	kal_uint8  ihdr_en;
	kal_uint8 i2c_write_id;
};

/* SENSOR PRIVATE STRUCT FOR CONSTANT */
struct imgsensor_info_struct {
	kal_uint32 sensor_id;
	kal_uint32 checksum_value;
	struct imgsensor_mode_struct pre;
	struct imgsensor_mode_struct cap;
	struct imgsensor_mode_struct cap1;
	struct imgsensor_mode_struct cap2;
	struct imgsensor_mode_struct normal_video;
	struct imgsensor_mode_struct hs_video;
	struct imgsensor_mode_struct slim_video;
	kal_uint8  ae_shut_delay_frame;
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
	kal_uint8  isp_driving_current;
	kal_uint8  sensor_interface_type;
	kal_uint8  mipi_sensor_type;
	kal_uint8  mipi_settle_delay_mode;
	kal_uint8  sensor_output_dataformat;
	kal_uint8  mclk;
	kal_uint8  mipi_lane_num;
	kal_uint8  i2c_addr_table[5];
};

extern int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData,
	u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId);
extern int iWriteReg(u16 a_u2Addr, u32 a_u4Data, u32 a_u4Bytes, u16 i2cId);

#endif

