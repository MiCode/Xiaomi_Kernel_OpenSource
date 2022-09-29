// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
/*****************************************************************************
 *
 * Filename:
 * ---------
 *     IMX709mipi_Sensor.c
 *
 * Project:
 * --------
 *     ALPS
 *
 * Description:
 * ------------
 *     Source code of Sensor driver
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/
#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/types.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define_v4l2.h"
#include "kd_imgsensor_errcode.h"

#include "imx709mipiraw_Sensor.h"
#include "imx709_eeprom.h"
#include "imx709_ana_gain_table.h"
#include "imx709_Sensor_setting.h"

#include "adaptor-subdrv.h"
#include "adaptor-i2c.h"
#include "adaptor.h"

#define DEBUG_LOG_EN 0

#define PFX "IMX709_camera_sensor"
#define LOG_INF(format, args...) pr_info(PFX "[%s] " format, __func__, ##args)
#define LOG_DEBUG(...) do { if ((DEBUG_LOG_EN)) LOG_INF(__VA_ARGS__); } while (0)

#ifdef AOV_MODE_SENSING
static const char * const clk_names[] = {
	ADAPTOR_CLK_NAMES
};

static const char * const reg_names[] = {
	ADAPTOR_REGULATOR_NAMES
};

static const char * const state_names[] = {
	ADAPTOR_STATE_NAMES
};
#endif

#define read_cmos_sensor_8(...) subdrv_i2c_rd_u8(__VA_ARGS__)
#define read_cmos_sensor_16(...) subdrv_i2c_rd_u16(__VA_ARGS__)
#define write_cmos_sensor_8(...) subdrv_i2c_wr_u8(__VA_ARGS__)
#define imx709_table_write_cmos_sensor(...) subdrv_i2c_wr_regs_u8(__VA_ARGS__)
#define imx709_seq_write_cmos_sensor(...) subdrv_i2c_wr_seq_p8(__VA_ARGS__)

#define _I2C_BUF_SIZE 256
static kal_uint16 _i2c_data[_I2C_BUF_SIZE];
static unsigned int _size_to_write;

static void commit_write_sensor(struct subdrv_ctx *ctx)
{
	if (_size_to_write) {
		imx709_table_write_cmos_sensor(ctx, _i2c_data, _size_to_write);
		memset(_i2c_data, 0x0, sizeof(_i2c_data));
		_size_to_write = 0;
	}
}

static void set_cmos_sensor_8(struct subdrv_ctx *ctx,
			kal_uint16 reg, kal_uint16 val)
{
	if (_size_to_write > _I2C_BUF_SIZE - 2)
		commit_write_sensor(ctx);
	else {
		_i2c_data[_size_to_write++] = reg;
		_i2c_data[_size_to_write++] = val;
	}
}

#if EEPROM_READY
#define PD_PIX_2_EN 0
#define SEQUENTIAL_WRITE_EN 1
#define IMX709_EEPROM_READ_ID  0xA8
#define IMX709_EEPROM_WRITE_ID 0xA9
static kal_uint8 otp_flag;
#endif

static kal_uint32 previous_exp[3];
static kal_uint16 previous_exp_cnt;

#ifdef AOV_MODE_SENSING
static int stream_refcnt_for_aov;
#endif

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = IMX709_SENSOR_ID,
	.checksum_value = 0xAF3E324F,

	.pre = {	/* reg_B3_1 3264x2448@30fps */
		.pclk = 863200000,
		.linelength = 7400,
		.framelength = 3888,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 420400000,
		.max_framerate = 300,	/* 30fps */
	},

	.cap = {	/* reg_B3_1 3264x2448@30fps */
		.pclk = 863200000,
		.linelength = 7400,
		.framelength = 3888,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 420400000,
		.max_framerate = 300,	/* 30fps */
	},

	.normal_video = {	/* reg_C5 3264x1856@30fps */
		.pclk = 863200000,
		.linelength = 7400,
		.framelength = 3888,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 1856,
		.mipi_data_lp2hs_settle_dc = 85,
		/* following for GetDefaultFramerateByScenario() */
		.mipi_pixel_rate = 440800000,
		.max_framerate = 300,	/* 30fps */
	},

	.hs_video = {	/* reg_C1-2 3264x1856@60ps */
		.pclk = 849600000,
		.linelength = 7400,
		.framelength = 1920,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 1856,
		.mipi_data_lp2hs_settle_dc = 85,
		/* following for GetDefaultFramerateByScenario() */
		.mipi_pixel_rate = 415200000,
		.max_framerate = 600, /* 60fps */
	},

	.slim_video = {	/* reg_C1-2 3264x1856@60ps */
		.pclk = 849600000,
		.linelength = 7400,
		.framelength = 1920,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 1856,
		.mipi_data_lp2hs_settle_dc = 85,
		/* following for GetDefaultFramerateByScenario() */
		.mipi_pixel_rate = 415200000,
		.max_framerate = 600, /* 60fps */
	},
#ifdef AOV_MODE_SENSING
	.custom1 = {	/* VGA+ 640x480@10FPS */
		.pclk = 167700000,
		.linelength = 936,
		.framelength = 17912,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 640,
		.grabwindow_height = 480,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 205500000,
		.max_framerate = 100,	/* 10fps */
	},
	.custom2 = {	/* VGA+ 480x320@10FPS */
		.pclk = 215800000,
		.linelength = 936,
		.framelength = 23048,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 480,
		.grabwindow_height = 320,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 224000000,
		.max_framerate = 100,	/* 10fps */
	},
	.custom3 = {	/* QVGA+ 320x240@10FPS */
		.pclk = 431600000,
		.linelength = 1674,
		.framelength = 25776,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 320,
		.grabwindow_height = 240,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 213000000,
		.max_framerate = 100,	/* 10fps */
	},
	.custom4 = {	/* VGA+ 640x480@5FPS */
		.pclk = 105300000,
		.linelength = 936,
		.framelength = 22496,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 640,
		.grabwindow_height = 480,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 150000000,
		.max_framerate = 50,	/* 5fps */
	},
	.custom5 = {	/* VGA+ 640x480@2FPS */
		.pclk = 105300000,
		.linelength = 936,
		.framelength = 56248,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 640,
		.grabwindow_height = 480,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 150000000,
		.max_framerate = 20,	/* 2fps */
	},
	.custom6 = {	/* VGA+ 480x320@5FPS */
		.pclk = 105300000,
		.linelength = 936,
		.framelength = 22496,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 480,
		.grabwindow_height = 320,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 150000000,
		.max_framerate = 50,	/* 5fps */
	},
	.custom7 = {	/* VGA+ 480x320@2FPS */
		.pclk = 105300000,
		.linelength = 936,
		.framelength = 56248,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 480,
		.grabwindow_height = 320,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 150000000,
		.max_framerate = 20,	/* 5fps */
	},
	.custom8 = {	/* QVGA+ 320x240@5FPS */
		.pclk = 210600000,
		.linelength = 1674,
		.framelength = 25160,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 320,
		.grabwindow_height = 240,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 150000000,
		.max_framerate = 50,	/* 5fps */
	},
	.custom9 = {	/* QVGA+ 320x240@2FPS */
		.pclk = 210600000,
		.linelength = 1674,
		.framelength = 62896,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 320,
		.grabwindow_height = 240,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 150000000,
		.max_framerate = 20,	/* 2fps */
	},
#endif
	.custom10 = {    /* 640x480@10FPS */
		.pclk = 465600000,
		.linelength = 3700,
		.framelength = 12576,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 640,
		.grabwindow_height = 480,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 120000000,
		.max_framerate = 100,   /* 10fps */
	},

	.min_gain = BASEGAIN * 1,	/* 1x gain */
	.max_gain = BASEGAIN * 32,	/* 32x gain */
	.min_gain_iso = 100,
	.margin = 24,	/* sensor framelength & shutter margin */
	.min_shutter = 8,	/* min shutter */
	.gain_step = 1,
	.gain_type = 0,
	.max_frame_length = 0xFFFF,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,	/* isp gain delay frame for AE cycle */
	.ihdr_support = 0,	/* 1: support; 0: not support */
	.ihdr_le_firstline = 0,	/* 1:le first; 0: se first */
	.temperature_support = 1,	/* 1, support; 0,not support */
	.sensor_mode_num = 15,	/* support sensor mode num */
	.frame_time_delay_frame = 3,

	.pre_delay_frame = 3,	/* 3 guanjd modify for c-t-s */
	.cap_delay_frame = 3,	/* 3 guanjd modify for c-t-s */
	.video_delay_frame = 3,
	.hs_video_delay_frame = 3,
	.slim_video_delay_frame = 3,
#ifdef AOV_MODE_SENSING
	.custom1_delay_frame = 3,
	.custom2_delay_frame = 3,
	.custom3_delay_frame = 3,
	.custom4_delay_frame = 3,
	.custom5_delay_frame = 3,
	.custom6_delay_frame = 3,
	.custom7_delay_frame = 3,
	.custom8_delay_frame = 3,
	.custom9_delay_frame = 3,
	.custom1_ae_ctrl_support = 0,	/* 1:support; 0:not support */
	.custom2_ae_ctrl_support = 0,	/* 1:support; 0:not support */
	.custom3_ae_ctrl_support = 0,	/* 1:support; 0:not support */
	.custom4_ae_ctrl_support = 0,	/* 1:support; 0:not support */
	.custom5_ae_ctrl_support = 0,	/* 1:support; 0:not support */
	.custom6_ae_ctrl_support = 0,	/* 1:support; 0:not support */
	.custom7_ae_ctrl_support = 0,	/* 1:support; 0:not support */
	.custom8_ae_ctrl_support = 0,	/* 1:support; 0:not support */
	.custom9_ae_ctrl_support = 0,	/* 1:support; 0:not support */
#endif
	.custom10_delay_frame = 3,
	.custom10_ae_ctrl_support = 1,	/* 1:support; 0:not support */
	.isp_driving_current = ISP_DRIVING_6MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,	/* only concern if it's cphy */
	.mipi_settle_delay_mode = 0,
	// .sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_Gb,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_B,
	.mclk = 26,	/* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_addr_table = {0x20, 0xff},
	/* record sensor support all write id addr,
	 * only supprt 4 must end with 0xff
	 */
	.i2c_speed = 1000,	/* i2c read/write speed */
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[15] = {
	{6560, 4928, 0, 16, 6560, 4896, 6560, 2448,
	8, 0, 6544, 2448, 1640, 0, 3264, 2448},	/* Preview reg_B3_1 */
	{6560, 4928, 0, 16, 6560, 4896, 6560, 2448,
	8, 0, 6544, 2448, 1640, 0, 3264, 2448},	/* capture reg_B3_1 */
	{6560, 4928, 0, 608, 6560, 3712, 6560, 1856,
	8, 0, 6544, 1856, 1640, 0, 3264, 1856},	/* normal video reg_C5 */
	{6560, 4928, 0, 608, 6560, 3712, 6560, 1856,
	8, 0, 6544, 1856, 1640, 0, 3264, 1856},	/* hs_video reg_G-2 */
	{6560, 4928, 0, 608, 6560, 3712, 6560, 1856,
	8, 0, 6544, 1856, 1640, 0, 3264, 1856},	/* slim video reg_G-2 */
#ifdef AOV_MODE_SENSING
	{6560, 4928, 0, 544, 6560, 3840, 820, 480,
	90, 0, 640, 480, 0, 0, 640, 480},	/* custom1  VGA+ */
	{6560, 4928, 0, 1184, 6560, 2560, 820, 320,
	170, 0, 480, 320, 0, 0, 480, 320},	/* custom2  VGA+ */
	{6560, 4928, 0, 544, 6560, 3840, 410, 240,
	45, 0, 320, 240, 0, 0, 320, 240},	/* custom3  QVGA+ */
	{6560, 4928, 0, 544, 6560, 3840, 820, 480,
	90, 0, 640, 480, 0, 0, 640, 480},	/* custom4  VGA+ */
	{6560, 4928, 0, 544, 6560, 3840, 820, 480,
	90, 0, 640, 480, 0, 0, 640, 480},	/* custom5  VGA+ */
	{6560, 4928, 0, 1184, 6560, 2560, 820, 320,
	170, 0, 480, 320, 0, 0, 480, 320},	/* custom6  VGA+ */
	{6560, 4928, 0, 1184, 6560, 2560, 820, 320,
	170, 0, 480, 320, 0, 0, 480, 320},	/* custom7  VGA+ */
	{6560, 4928, 0, 544, 6560, 3840, 410, 240,
	45, 0, 320, 240, 0, 0, 320, 240},	/* custom8  QVGA+ */
	{6560, 4928, 0, 544, 6560, 3840, 410, 240,
	45, 0, 320, 240, 0, 0, 320, 240},	/* custom9  QVGA+ */
#endif
	{6560, 4928, 0, 1504, 6560, 1920, 3280, 480,
	1320, 0, 640, 480, 0, 0, 640, 480},     /* custom10 */
};

//the index order of VC_STAGGER_NE/ME/SE in array identify the order of readout in MIPI transfer
static struct SENSOR_VC_INFO2_STRUCT SENSOR_VC_INFO2[15] = {
	{
		0x01, 0x0a, 0x00, 0x08, 0x40, 0x00,	// preivew
		{
			{VC_STAGGER_NE, 0x00, 0x2b, 3264, 2448},
		},
		1
	},
	{
		0x01, 0x0a, 0x00, 0x08, 0x40, 0x00,	// capture
		{
			{VC_STAGGER_NE, 0x00, 0x2b, 3264, 2448},
		},
		1
	},
	{
		0x01, 0x0a, 0x00, 0x08, 0x40, 0x00,	// normal video
		{
			{VC_STAGGER_NE, 0x00, 0x2b, 3264, 1856},
		},
		1
	},
	{
		0x01, 0x0a, 0x00, 0x08, 0x40, 0x00,	// hs_video
		{
			{VC_STAGGER_NE, 0x00, 0x2b, 3264, 1856},
		},
		1
	},
	{
		0x01, 0x0a, 0x00, 0x08, 0x40, 0x00,	// slim video
		{
			{VC_STAGGER_NE, 0x00, 0x2b, 3264, 1856},
		},
		1
	},
#ifdef AOV_MODE_SENSING
	{
		0x01, 0x0a, 0x00, 0x08, 0x40, 0x00,	// custom1
		{
			{VC_STAGGER_NE, 0x00, 0x2a, 640, 480},
		},
		1
	},
	{
		0x01, 0x0a, 0x00, 0x08, 0x40, 0x00,	// custom2
		{
			{VC_STAGGER_NE, 0x00, 0x2a, 480, 320},
		},
		1
	},
	{
		0x01, 0x0a, 0x00, 0x08, 0x40, 0x00,	// custom3
		{
			{VC_STAGGER_NE, 0x00, 0x2a, 320, 240},
		},
		1
	},
	{
		0x01, 0x0a, 0x00, 0x08, 0x40, 0x00,	// custom4
		{
			{VC_STAGGER_NE, 0x00, 0x2a, 640, 480},
		},
		1
	},
	{
		0x01, 0x0a, 0x00, 0x08, 0x40, 0x00,	// custom5
		{
			{VC_STAGGER_NE, 0x00, 0x2a, 640, 480},
		},
		1
	},
	{
		0x01, 0x0a, 0x00, 0x08, 0x40, 0x00,	// custom6
		{
			{VC_STAGGER_NE, 0x00, 0x2a, 480, 320},
		},
		1
	},
	{
		0x01, 0x0a, 0x00, 0x08, 0x40, 0x00,	// custom7
		{
			{VC_STAGGER_NE, 0x00, 0x2a, 480, 320},
		},
		1
	},
	{
		0x01, 0x0a, 0x00, 0x08, 0x40, 0x00,	// custom8
		{
			{VC_STAGGER_NE, 0x00, 0x2a, 320, 240},
		},
		1
	},
	{
		0x01, 0x0a, 0x00, 0x08, 0x40, 0x00,	// custom9
		{
			{VC_STAGGER_NE, 0x00, 0x2a, 320, 240},
		},
		1
	},
#endif
	{
		0x01, 0x0a, 0x00, 0x08, 0x40, 0x00,     // custom10
		{
			{VC_STAGGER_NE, 0x00, 0x2b, 640, 480},
		},
		1
	},
};

//mode 0,  1,  2,   3,  4,  5,  6,  7,   8,   9,  10, 11,  12,  13,  14, 15, 16, 17, 18, 19, 20
static MUINT32 fine_integ_line_table[SENSOR_SCENARIO_ID_MAX] = {
	826,	//mode 0
	826,	//mode 1
	826,	//mode 2
	826,	//mode 3
	826,	//mode 4
	0,	//mode 5
	0,	//mode 6
	0,	//mode 7
	0,	//mode 8
	0,	//mode 9
	0,	//mode 10
	0,	//mode 11
	0,	//mode 12
	0,	//mode 13
	826,	//mode 14
	0,	//mode 15
	0,	//mode 16
	0,	//mode 17
	0,	//mode 18
	0,	//mode 19
	0,	//mode 20
};

static MUINT32 exposure_step_table[SENSOR_SCENARIO_ID_MAX] = {
	4,	//mode 0
	4,	//mode 1
	4,	//mode 2
	4,	//mode 3
	4,	//mode 4
	4,	//mode 5
	4,	//mode 6
	4,	//mode 7
	4,	//mode 8
	4,	//mode 9
	4,	//mode 10
	4,	//mode 11
	4,	//mode 12
	4,	//mode 13
	4,	//mode 14
	4,	//mode 15
	4,	//mode 16
	4,	//mode 17
	4,	//mode 18
	4,	//mode 19
	4,	//mode 20
};

static void get_vc_info_2(struct SENSOR_VC_INFO2_STRUCT *pvcinfo2, kal_uint32 scenario)
{
	switch (scenario) {
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[1],
			sizeof(struct SENSOR_VC_INFO2_STRUCT));
		break;
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[2],
			sizeof(struct SENSOR_VC_INFO2_STRUCT));
		break;
	case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[3],
			sizeof(struct SENSOR_VC_INFO2_STRUCT));
		break;
	case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[4],
			sizeof(struct SENSOR_VC_INFO2_STRUCT));
		break;
#ifdef AOV_MODE_SENSING
	case SENSOR_SCENARIO_ID_CUSTOM1:
		memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[5],
			sizeof(struct SENSOR_VC_INFO2_STRUCT));
		break;
	case SENSOR_SCENARIO_ID_CUSTOM2:
		memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[6],
			sizeof(struct SENSOR_VC_INFO2_STRUCT));
		break;
	case SENSOR_SCENARIO_ID_CUSTOM3:
		memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[7],
			sizeof(struct SENSOR_VC_INFO2_STRUCT));
		break;
	case SENSOR_SCENARIO_ID_CUSTOM4:
		memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[8],
			sizeof(struct SENSOR_VC_INFO2_STRUCT));
		break;
	case SENSOR_SCENARIO_ID_CUSTOM5:
		memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[9],
			sizeof(struct SENSOR_VC_INFO2_STRUCT));
		break;
	case SENSOR_SCENARIO_ID_CUSTOM6:
		memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[10],
			sizeof(struct SENSOR_VC_INFO2_STRUCT));
		break;
	case SENSOR_SCENARIO_ID_CUSTOM7:
		memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[11],
			sizeof(struct SENSOR_VC_INFO2_STRUCT));
		break;
	case SENSOR_SCENARIO_ID_CUSTOM8:
		memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[12],
			sizeof(struct SENSOR_VC_INFO2_STRUCT));
		break;
	case SENSOR_SCENARIO_ID_CUSTOM9:
		memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[13],
			sizeof(struct SENSOR_VC_INFO2_STRUCT));
		break;
#endif
	case SENSOR_SCENARIO_ID_CUSTOM10:
		memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[14],
			sizeof(struct SENSOR_VC_INFO2_STRUCT));
		break;
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
	default:
		memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[0],
			sizeof(struct SENSOR_VC_INFO2_STRUCT));
		break;
	}
}

static int get_frame_desc(struct subdrv_ctx *ctx,
	int scenario_id, struct mtk_mbus_frame_desc *fd);

static kal_uint32 get_exp_cnt_by_scenario(struct subdrv_ctx *ctx, kal_uint32 scenario)
{
	kal_uint32 exp_cnt = 0, i = 0;
	struct mtk_mbus_frame_desc frame_desc;

	DEBUG_LOG(ctx, "E!\n");

	memset(&frame_desc, 0, sizeof(frame_desc));

	get_frame_desc(ctx, scenario, &frame_desc);

	for (i = 0; i < frame_desc.num_entries; ++i) {
		if (frame_desc.entry[i].bus.csi2.user_data_desc > VC_STAGGER_MIN_NUM &&
			frame_desc.entry[i].bus.csi2.user_data_desc < VC_STAGGER_MAX_NUM) {
			exp_cnt++;
		}
	}

	DEBUG_LOG(ctx, "exp_cnt(%d)\n", exp_cnt);
	return max(exp_cnt, (kal_uint32)1);
}

static kal_uint32 get_cur_exp_cnt(struct subdrv_ctx *ctx)
{
	kal_uint32 exp_cnt = 1;

	DEBUG_LOG(ctx, "E!\n");

	if (0x1 == (read_cmos_sensor_8(ctx, 0x3170) & 0x1)) { // DOL_EN
		exp_cnt = 2;
	}

	return exp_cnt;
}

static void imx709_get_pdaf_reg_setting(struct subdrv_ctx *ctx,
		MUINT32 regNum, kal_uint16 *regDa)
{
	int i, idx;

	DEBUG_LOG(ctx, "E!\n");

	for (i = 0; i < regNum; i++) {
		idx = 2 * i;
		regDa[idx + 1] = read_cmos_sensor_8(ctx, regDa[idx]);
	}
}

static void imx709_set_pdaf_reg_setting(struct subdrv_ctx *ctx,
		MUINT32 regNum, kal_uint16 *regDa)
{
	DEBUG_LOG(ctx, "E!\n");

	imx709_table_write_cmos_sensor(ctx, regDa, regNum*2);
}

#if EEPROM_READY
#define QSC_SIZE 3072
#define QSC_TABLE_SIZE (QSC_SIZE*2)
#define QSC_EEPROM_ADDR 0x1E30
#define QSC_OTP_ADDR 0xC800
#define SENSOR_ID_L 0x05
#define SENSOR_ID_H 0x00
#define LENS_ID_L 0x48
#define LENS_ID_H 0x01
#define SENSOR_ID_L_V2 0x0E
#if SEQUENTIAL_WRITE_EN
static kal_uint8 imx709_QSC_setting[QSC_SIZE];
#else
static kal_uint16 imx709_QSC_setting[QSC_TABLE_SIZE];
#endif

static kal_uint16 read_cmos_eeprom_8(struct subdrv_ctx *ctx, kal_uint16 addr)
{
	kal_uint16 get_byte = 0;

	DEBUG_LOG(ctx, "E!\n");

	adaptor_i2c_rd_u8(ctx->i2c_client, IMX709_EEPROM_READ_ID >> 1, addr, (u8 *)&get_byte);
	return get_byte;
}

static void read_sensor_Cali(struct subdrv_ctx *ctx)
{
	kal_uint16 idx = 0;
	kal_uint16 addr_qsc = QSC_EEPROM_ADDR;
#if SEQUENTIAL_WRITE_EN == 0
	kal_uint16 sensor_qsc = QSC_OTP_ADDR;
#endif
	kal_uint8 otp_data[9] = {0};
	int i = 0;

	DEBUG_LOG(ctx, "E!\n");

	/*read otp data to distinguish module*/
	otp_flag = OTP_QSC_NONE;

	for (i = 0; i < 7; i++)
		otp_data[i] = read_cmos_eeprom_8(ctx, 0x0006 + i);

	if ((otp_data[0] == SENSOR_ID_L || otp_data[0] == SENSOR_ID_L_V2) &&
		(otp_data[1] == SENSOR_ID_H) &&
		(otp_data[2] == LENS_ID_L) &&
		(otp_data[3] == LENS_ID_H)) {
		DEBUG_LOG(ctx, "OTP type: Custom Only");
		otp_flag = OTP_QSC_CUSTOM;
		for (idx = 0; idx < QSC_SIZE; idx++) {
			addr_qsc = QSC_EEPROM_ADDR + idx;
#if SEQUENTIAL_WRITE_EN
			imx709_QSC_setting[idx] = read_cmos_eeprom_8(ctx, addr_qsc);
#else
			sensor_qsc = QSC_OTP_ADDR + idx;
			imx709_QSC_setting[2 * idx] = sensor_qsc;
			imx709_QSC_setting[2 * idx + 1] =
				read_cmos_eeprom_8(ctx, addr_qsc);
#endif
		}
	} else {
		LOG_INF(
			"OTP type: No Data, 0x0008 = %d, 0x0009 = %d",
			read_cmos_eeprom_8(ctx, 0x0008),
			read_cmos_eeprom_8(ctx, 0x0009));
	}
	ctx->is_read_preload_eeprom = 1;
}

void write_sensor_QSC(struct subdrv_ctx *ctx)
{
	DEBUG_LOG(ctx, "E!\n");

	/* calibration tool version 3.0 -> 0x4E */
	write_cmos_sensor_8(ctx, 0x86A9, 0x4E);
	/* set QSC from EEPROM to sensor */
	if ((otp_flag == OTP_QSC_CUSTOM) || (otp_flag == OTP_QSC_INTERNAL)) {
#if SEQUENTIAL_WRITE_EN
		imx709_seq_write_cmos_sensor(ctx, QSC_OTP_ADDR,
			imx709_QSC_setting, sizeof(imx709_QSC_setting));
#else
		imx709_table_write_cmos_sensor(ctx, imx709_QSC_setting,
			sizeof(imx709_QSC_setting) / sizeof(kal_uint16));
#endif
	}
	write_cmos_sensor_8(ctx, 0x32D2, 0x01);
}
#endif

static void write_frame_len(struct subdrv_ctx *ctx, kal_uint32 fll)
{
	// write_frame_len should be called inside GRP_PARAM_HOLD (0x0104)
	// FRM_LENGTH_LINES must be multiple of 4
	kal_uint32 exp_cnt = get_cur_exp_cnt(ctx);

	DEBUG_LOG(ctx, "E!\n");

	ctx->frame_length = round_up(fll / exp_cnt, 4) * exp_cnt;

	if (ctx->extend_frame_length_en == KAL_FALSE) {
		DEBUG_LOG(ctx, "fll(%d), exp_cnt(%d)\n", ctx->frame_length, exp_cnt);
		set_cmos_sensor_8(ctx, 0x0340, ctx->frame_length / exp_cnt >> 8);
		set_cmos_sensor_8(ctx, 0x0341, ctx->frame_length / exp_cnt & 0xFF);
	}
}

static void set_dummy(struct subdrv_ctx *ctx)
{
	DEBUG_LOG(ctx,
		"dummyline(%d), dummypixels(%d)\n",
		ctx->dummy_line, ctx->dummy_pixel);
	// return;	/* for test */
	set_cmos_sensor_8(ctx, 0x0104, 0x01);
	write_frame_len(ctx, ctx->frame_length);
	set_cmos_sensor_8(ctx, 0x0104, 0x00);

	commit_write_sensor(ctx);
}	/* set_dummy */

static void set_mirror_flip(struct subdrv_ctx *ctx, kal_uint8 image_mirror)
{
	kal_uint8 itemp;

	DEBUG_LOG(ctx, "image_mirror(%d)\n", image_mirror);
	if (ctx->sensor_mode >= IMGSENSOR_MODE_CUSTOM1 &&
		ctx->sensor_mode < IMGSENSOR_MODE_CUSTOM10)
		itemp = read_cmos_sensor_8(ctx, 0x3874);
	else
		itemp = read_cmos_sensor_8(ctx, 0x0101);

	itemp &= ~0x03;

	switch (image_mirror) {
	case IMAGE_NORMAL:
	if (ctx->sensor_mode >= IMGSENSOR_MODE_CUSTOM1 &&
		ctx->sensor_mode < IMGSENSOR_MODE_CUSTOM10)
		write_cmos_sensor_8(ctx, 0x3874, itemp);
	else
		write_cmos_sensor_8(ctx, 0x0101, itemp);
		break;
	case IMAGE_V_MIRROR:
	if (ctx->sensor_mode >= IMGSENSOR_MODE_CUSTOM1 &&
		ctx->sensor_mode < IMGSENSOR_MODE_CUSTOM10)
		write_cmos_sensor_8(ctx, 0x3874, itemp | 0x02);
	else
		write_cmos_sensor_8(ctx, 0x0101, itemp | 0x02);
		break;
	case IMAGE_H_MIRROR:
	if (ctx->sensor_mode >= IMGSENSOR_MODE_CUSTOM1 &&
		ctx->sensor_mode < IMGSENSOR_MODE_CUSTOM10)
		write_cmos_sensor_8(ctx, 0x3874, itemp | 0x01);
	else
		write_cmos_sensor_8(ctx, 0x0101, itemp | 0x01);
		break;
	case IMAGE_HV_MIRROR:
	if (ctx->sensor_mode >= IMGSENSOR_MODE_CUSTOM1 &&
		ctx->sensor_mode < IMGSENSOR_MODE_CUSTOM10)
		write_cmos_sensor_8(ctx, 0x3874, itemp | 0x03);
	else
		write_cmos_sensor_8(ctx, 0x0101, itemp | 0x03);
		break;
	}
}

static void set_max_framerate(struct subdrv_ctx *ctx, UINT16 framerate, kal_bool min_framelength_en)
{
	/*kal_int16 dummy_line;*/
	kal_uint32 frame_length = ctx->frame_length;

	DEBUG_LOG(ctx,
		"framerate(%d), min framelength should enable(%d)\n",
		framerate, min_framelength_en);

	frame_length = ctx->pclk / framerate * 10 / ctx->line_length;
	if (frame_length >= ctx->min_frame_length)
		ctx->frame_length = frame_length;
	else
		ctx->frame_length = ctx->min_frame_length;

	ctx->dummy_line =
			ctx->frame_length - ctx->min_frame_length;

	if (ctx->frame_length > imgsensor_info.max_frame_length) {
		ctx->frame_length = imgsensor_info.max_frame_length;
		ctx->dummy_line =
			ctx->frame_length - ctx->min_frame_length;
	}
	if (min_framelength_en)
		ctx->min_frame_length = ctx->frame_length;
}	/* set_max_framerate */

static kal_bool set_auto_flicker(struct subdrv_ctx *ctx)
{
	kal_uint16 realtime_fps = 0;

	DEBUG_LOG(ctx, "E!\n");

	if (ctx->autoflicker_en) {
		realtime_fps = ctx->pclk / ctx->line_length * 10
				/ ctx->frame_length;
		DEBUG_LOG(ctx,
			"autoflicker enable, realtime_fps(%d)\n",
			realtime_fps);
		if (realtime_fps >= 593 && realtime_fps <= 615) {
			set_max_framerate(ctx, 592, 0);
			return KAL_TRUE;
		}
		if (realtime_fps >= 297 && realtime_fps <= 305) {
			set_max_framerate(ctx, 296, 0);
			return KAL_TRUE;
		}
		if (realtime_fps >= 147 && realtime_fps <= 150) {
			set_max_framerate(ctx, 146, 0);
			return KAL_TRUE;
		}
	} else
		LOG_INF("(else)ctx->frame_length(%d)\n", ctx->frame_length);

	return KAL_FALSE;
}

#define MAX_CIT_LSHIFT 7
static void write_shutter(struct subdrv_ctx *ctx, kal_uint32 shutter, kal_bool gph)
{
	kal_uint16 l_shift = 1;
	kal_uint32 fineIntegTime = fine_integ_line_table[ctx->current_scenario_id];
	int i;

	DEBUG_LOG(ctx, "E!\n");

	shutter = FINE_INTEG_CONVERT(shutter, fineIntegTime);
	shutter = round_up(shutter, 4);

	// if (shutter > ctx->min_frame_length - imgsensor_info.margin)
		// ctx->frame_length = shutter + imgsensor_info.margin;
	// else
	ctx->frame_length = ctx->min_frame_length;
	if (ctx->frame_length > imgsensor_info.max_frame_length)
		ctx->frame_length = imgsensor_info.max_frame_length;
	if (shutter < imgsensor_info.min_shutter)
		shutter = imgsensor_info.min_shutter;

	/* restore current shutter value */
	for (i = 0; i < previous_exp_cnt; i++)
		previous_exp[i] = 0;
	previous_exp[0] = shutter;
	previous_exp_cnt = 1;

	if (gph)
		set_cmos_sensor_8(ctx, 0x0104, 0x01);

	set_auto_flicker(ctx);

	ctx->shutter = shutter;

	/* long expsoure */
	if (shutter >
		(imgsensor_info.max_frame_length - imgsensor_info.margin)) {

		for (l_shift = 1; l_shift < MAX_CIT_LSHIFT; l_shift++) {
			if ((shutter >> l_shift)
				< (imgsensor_info.max_frame_length - imgsensor_info.margin))
				break;
		}
		if (l_shift > MAX_CIT_LSHIFT) {
			LOG_INF(
				"Unable to set such a long exposure(%d), set to max\n",
				shutter);

			l_shift = MAX_CIT_LSHIFT;
		}
		shutter = shutter >> l_shift;
		// ctx->frame_length = shutter + imgsensor_info.margin;
		LOG_INF("enter long exposure mode, time is(%d)", l_shift);
		set_cmos_sensor_8(ctx, 0x3150, l_shift);
		/* Frame exposure mode customization for LE */
		ctx->ae_frm_mode.frame_mode_1 = IMGSENSOR_AE_MODE_SE;
		ctx->ae_frm_mode.frame_mode_2 = IMGSENSOR_AE_MODE_SE;
		ctx->current_ae_effective_frame = 2;
	} else {
		set_cmos_sensor_8(ctx, 0x3150, 0x00);
		// write_frame_len(ctx, ctx->frame_length);
		ctx->current_ae_effective_frame = 2;
	}

	/* Update Shutter */
	set_cmos_sensor_8(ctx, 0x0350, 0x01); /* Enable auto extend */
	set_cmos_sensor_8(ctx, 0x0202, (shutter >> 8) & 0xFF);
	set_cmos_sensor_8(ctx, 0x0203, shutter  & 0xFF);
	if (!ctx->ae_ctrl_gph_en) {
		if (gph)
			set_cmos_sensor_8(ctx, 0x0104, 0x00);

		commit_write_sensor(ctx);
	}

	DEBUG_LOG(ctx,
		"shutter(%d), framelength(%d)\n",
		shutter, ctx->frame_length);
}	/* write_shutter */

/*************************************************************************
 * FUNCTION
 *	set_shutter
 *
 * DESCRIPTION
 *	This function set e-shutter of sensor to change exposure time.
 *
 * PARAMETERS
 *	iShutter : exposured lines
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static void set_shutter_w_gph(struct subdrv_ctx *ctx, kal_uint32 shutter, kal_bool gph)
{
	DEBUG_LOG(ctx, "E!\n");
	write_shutter(ctx, shutter, gph);
}
static void set_shutter(struct subdrv_ctx *ctx, kal_uint32 shutter)
{
	DEBUG_LOG(ctx, "E!\n");
	set_shutter_w_gph(ctx, shutter, KAL_TRUE);
}	/* set_shutter */

static void set_frame_length(struct subdrv_ctx *ctx, kal_uint16 frame_length)
{
	DEBUG_LOG(ctx, "E!\n");
	if (frame_length > 1)
		ctx->frame_length = frame_length;
	if (ctx->frame_length > imgsensor_info.max_frame_length)
		ctx->frame_length = imgsensor_info.max_frame_length;
	if (ctx->min_frame_length > ctx->frame_length)
		ctx->frame_length = ctx->min_frame_length;
	/* Extend frame length */
	set_cmos_sensor_8(ctx, 0x0104, 0x01);
	write_frame_len(ctx, ctx->frame_length);
	set_cmos_sensor_8(ctx, 0x0104, 0x00);
	commit_write_sensor(ctx);

	DEBUG_LOG(ctx,
		"Framelength: set(%d)/input(%d)/min(%d)\n",
		ctx->frame_length, frame_length, ctx->min_frame_length);
}

static void set_multi_shutter_frame_length(struct subdrv_ctx *ctx,
				kal_uint32 *shutters, kal_uint16 shutter_cnt,
				kal_uint16 frame_length)
{
	int i;
	int readoutDiff = 0;
	kal_uint32 calc_fl = 0;
	kal_uint32 calc_fl2 = 0;
	kal_uint32 calc_fl3 = 0;
	kal_uint16 le = 0;
	kal_uint16 me = 0;
	kal_uint16 se = 0;
	kal_uint32 fineIntegTime = 0;
	kal_uint32 readoutLength = 0;
	kal_uint32 readMargin = 0;

	DEBUG_LOG(ctx, "E!\n");

	fineIntegTime = fine_integ_line_table[ctx->current_scenario_id];
	readoutLength = ctx->readout_length;
	readMargin = ctx->read_margin;

	/* convert & set current available shutter value */
	for (i = 0; i < shutter_cnt; i++) {
		shutters[i] = FINE_INTEG_CONVERT(shutters[i], fineIntegTime);
		shutters[i] = (kal_uint16)max(imgsensor_info.min_shutter,
					(kal_uint32)shutters[i]);
		shutters[i] = round_up((shutters[i]) / shutter_cnt, 4) * shutter_cnt;
	}

	/* fl constraint 1: previous se + previous me + current le */
	calc_fl = shutters[0];
	for (i = 1; i < previous_exp_cnt; i++)
		calc_fl += previous_exp[i];
	calc_fl += imgsensor_info.margin*shutter_cnt*shutter_cnt;

	/* fl constraint 2: current se + current me + current le */
	calc_fl2 = shutters[0];
	for (i = 1; i < shutter_cnt; i++)
		calc_fl2 += shutters[i];
	calc_fl2 += imgsensor_info.margin*shutter_cnt*shutter_cnt;

	/* fl constraint 3: readout time cannot be overlapped */
	calc_fl3 = (readoutLength + readMargin);
	if (previous_exp_cnt == shutter_cnt) {
		for (i = 1; i < shutter_cnt; i++) {
			readoutDiff = previous_exp[i] - shutters[i];
			calc_fl3 += readoutDiff > 0 ? readoutDiff : 0;
		}
	}

	/* using max fl of above value */
	calc_fl = max(calc_fl, calc_fl2);
	calc_fl = max(calc_fl, calc_fl3);

	/* set fl range */
	ctx->frame_length = max((kal_uint32)calc_fl, ctx->min_frame_length);
	ctx->frame_length = max(ctx->frame_length, (kal_uint32)frame_length);
	ctx->frame_length = min(ctx->frame_length, imgsensor_info.max_frame_length);

	/* restore current shutter value */
	for (i = 0; i < previous_exp_cnt; i++)
		previous_exp[i] = 0;
	for (i = 0; i < shutter_cnt; i++)
		previous_exp[i] = shutters[i];
	previous_exp_cnt = shutter_cnt;

	/* register value conversion */
	switch (shutter_cnt) {
	case 3:
		le = shutters[0]/3;
		me = shutters[1]/3;
		se = shutters[2]/3;
		break;
	case 2:
		le = shutters[0]/2;
		me = 0;
		se = shutters[1]/2;
		break;
	case 1:
		le = shutters[0];
		me = 0;
		se = 0;
		break;
	}

	set_cmos_sensor_8(ctx, 0x0104, 0x01);
	if (!set_auto_flicker(ctx))
		write_frame_len(ctx, ctx->frame_length);
	/* Long exposure */
	set_cmos_sensor_8(ctx, 0x0202, (le >> 8) & 0xFF);
	set_cmos_sensor_8(ctx, 0x0203, le & 0xFF);
	/* Middle exposure */
	if (me) {
		/* MID_COARSE_INTEG_TIME[15:8] */
		// set_cmos_sensor_8(ctx, 0x313A, (me >> 8) & 0xFF);
		/* MID_COARSE_INTEG_TIME[7:0] */
		// set_cmos_sensor_8(ctx, 0x313B, me & 0xFF);
	}
	/* Short exposure */
	if (se) {
		set_cmos_sensor_8(ctx, 0x0224, (se >> 8) & 0xFF);
		set_cmos_sensor_8(ctx, 0x0225, se & 0xFF);
	}
	if (!ctx->ae_ctrl_gph_en) {
		set_cmos_sensor_8(ctx, 0x0104, 0x00);

		commit_write_sensor(ctx);
	}

	DEBUG_LOG(ctx,
		"L! le:(0x%x), me:(0x%x), se:(0x%x), fl:(0x%x)\n",
		le, me, se, ctx->frame_length);
}

static void set_shutter_frame_length(struct subdrv_ctx *ctx,
				kal_uint16 shutter, kal_uint16 frame_length,
				kal_bool auto_extend_en)
{
	kal_uint16 realtime_fps = 0;
	kal_int32 dummy_line = 0;

	DEBUG_LOG(ctx, "E!\n");

	ctx->shutter = shutter;

	/* 0x3500, 0x3501, 0x3502 will increase VBLANK to get exposure larger
	 * than frame exposure
	 */
	/* AE doesn't update sensor gain at capture mode, thus extra exposure
	 * lines must be updated here.
	 */

	/* OV Recommend Solution */
	/* if shutter bigger than frame_length,
	 * should extend frame length first
	 */
	/*Change frame time */
	if (frame_length > 1)
		dummy_line = frame_length - ctx->frame_length;
	ctx->frame_length = ctx->frame_length + dummy_line;

	/*  */
	// if (shutter > ctx->frame_length - imgsensor_info.margin)
		// ctx->frame_length = shutter + imgsensor_info.margin;

	if (ctx->frame_length > imgsensor_info.max_frame_length)
		ctx->frame_length = imgsensor_info.max_frame_length;

	shutter = (shutter < imgsensor_info.min_shutter) ?
		imgsensor_info.min_shutter : shutter;

	shutter =
	(shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
	? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

	set_cmos_sensor_8(ctx, 0x0104, 0x01);
	if (ctx->autoflicker_en) {
		realtime_fps = ctx->pclk
			/ ctx->line_length * 10 / ctx->frame_length;

		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(ctx, 296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(ctx, 146, 0);
	}
	write_frame_len(ctx, ctx->frame_length);

	/* Update Shutter */
	if (auto_extend_en)
		set_cmos_sensor_8(ctx, 0x0350, 0x01); /* Enable auto extend */
	else
		set_cmos_sensor_8(ctx, 0x0350, 0x00); /* Disable auto extend */
	set_cmos_sensor_8(ctx, 0x0202, (shutter >> 8) & 0xFF);
	set_cmos_sensor_8(ctx, 0x0203, shutter & 0xFF);
	set_cmos_sensor_8(ctx, 0x0104, 0x00);

	commit_write_sensor(ctx);

	DEBUG_LOG(ctx,
		"shutter(%d), framelength(%d)/(%d), dummy_line(%d)\n",
		shutter, ctx->frame_length, frame_length, dummy_line);
}	/* set_shutter_frame_length */

static kal_uint16 gain2reg(struct subdrv_ctx *ctx, const kal_uint32 gain)
{
	kal_uint16 reg_gain = 0x0;

	DEBUG_LOG(ctx, "E!\n");

	reg_gain = 16384 - (16384 * BASEGAIN) / gain;

	return (kal_uint16) reg_gain;
}

/*************************************************************************
 * FUNCTION
 *	set_gain
 *
 * DESCRIPTION
 *	This function is to set global gain to sensor.
 *
 * PARAMETERS
 *	iGain : sensor global gain(base: 0x400)
 *
 * RETURNS
 *	the actually gain set to sensor.
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 set_gain_w_gph(struct subdrv_ctx *ctx, kal_uint32 gain, kal_bool gph)
{
	kal_uint16 reg_gain, max_gain = imgsensor_info.max_gain;

	DEBUG_LOG(ctx, "E!\n");

	if (gain < imgsensor_info.min_gain || gain > max_gain) {
		LOG_INF("Error gain setting");

	if (gain < imgsensor_info.min_gain)
		gain = imgsensor_info.min_gain;
	else
		gain = max_gain;
	}

	reg_gain = gain2reg(ctx, gain);
	ctx->gain = reg_gain;
	DEBUG_LOG(ctx, "gain(%d), reg_gain(0x%x)\n", gain, reg_gain);

	if (gph && !ctx->ae_ctrl_gph_en)
		set_cmos_sensor_8(ctx, 0x0104, 0x01);
	set_cmos_sensor_8(ctx, 0x0204, (reg_gain>>8) & 0xFF);
	set_cmos_sensor_8(ctx, 0x0205, reg_gain & 0xFF);
	if (gph)
		set_cmos_sensor_8(ctx, 0x0104, 0x00);

	commit_write_sensor(ctx);

	return gain;
}

static kal_uint32 set_gain(struct subdrv_ctx *ctx, kal_uint32 gain)
{
	DEBUG_LOG(ctx, "E!\n");
	return set_gain_w_gph(ctx, gain, KAL_TRUE);
}	/* set_gain */

#ifdef PWR_SEQ_ALL_USE_FOR_AOV_MODE_TRANSITION
static int pwr_seq_common_disable_for_mode_transition(struct adaptor_ctx *ctx)
{
	int ret = 0;

	DEBUG_LOG(ctx, "E!\n");

	// 1. set gpio
	// mclk_driving_current_off
	ret = pinctrl_select_state(ctx->pinctrl,
		ctx->state[STATE_MCLK1_OFF]);
	if (ret < 0) {
		LOG_INF(
			"select(%s)(fail),ret(%d)\n",
			state_names[STATE_MCLK1_OFF], ret);
		return ret;
	}
	LOG_INF("select(%s)(correct)\n", state_names[STATE_MCLK1_OFF]);
	mdelay(6);
	// 2. set reg
	// disable DOVDD
	ret = regulator_disable(ctx->regulator[REGULATOR_DOVDD]);
	if (ret) {
		LOG_INF(
			"disable(%s)(fail),ret(%d)\n",
			reg_names[REGULATOR_DOVDD], ret);
		return ret;
	}
	LOG_INF("select(%s)(correct)\n", reg_names[REGULATOR_DOVDD]);
	mdelay(1);
	// disable DVDD1
	ret = regulator_disable(ctx->regulator[REGULATOR_DVDD1]);
	if (ret) {
		LOG_INF("disable(%s)(fail),ret(%d)\n",
		reg_names[REGULATOR_DVDD1], ret);
		return ret;
	}
	LOG_INF("select(%s)(correct)\n", reg_names[REGULATOR_DVDD1]);
	mdelay(4);
	// disable AVDD2
	ret = regulator_disable(ctx->regulator[REGULATOR_AVDD2]);
	if (ret) {
		LOG_INF(
			"disable(%s)(fail),ret(%d)\n",
			reg_names[REGULATOR_AVDD2], ret);
		return ret;
	}
	LOG_INF("select(%s)(correct)\n", reg_names[REGULATOR_AVDD2]);
	mdelay(3);
	// 3. set mclk
	// disable mclk
	clk_disable_unprepare(ctx->clk[CLK1_MCLK1]);

	return ret;
}

static int pwr_seq_common_enable_for_mode_transition(struct adaptor_ctx *ctx)
{
	int ret = 0;

	DEBUG_LOG(ctx, "E!\n");

	// 1. set mclk
	// 24MHz
	ret = clk_prepare_enable(ctx->clk[CLK1_MCLK1]);
	if (ret) {
		LOG_INF("enable mclk(fail),ret(%d)\n", ret);
		return ret;
	}
	LOG_INF("enable mclk(correct)\n");
	ret = clk_set_parent(ctx->clk[CLK1_MCLK1], ctx->clk[CLK1_26M]);
	if (ret) {
		LOG_INF("enable mclk's parent(fail),ret(%d)\n", ret);
		return ret;
	}
	LOG_INF("enable mclk's parent(correct)\n");
	// 2. set reg
	// enable AVDD2
	ret = regulator_set_voltage(ctx->regulator[REGULATOR_AVDD2], 1800000, 1800000);
	if (ret) {
		LOG_INF(
			"set voltage(%s)(%d)(fail),ret(%d)\n",
			reg_names[REGULATOR_AVDD2], 1800000, ret);
		return ret;
	}
	LOG_INF("set voltage(%s)(%d)(correct)\n",
		reg_names[REGULATOR_AVDD2], 1800000);
	ret = regulator_enable(ctx->regulator[REGULATOR_AVDD2]);
	if (ret) {
		LOG_INF(
			"enable(%s)(fail),ret(%d)\n",
			reg_names[REGULATOR_AVDD2], ret);
		return ret;
	}
	LOG_INF("enable(%s)(correct)\n", reg_names[REGULATOR_AVDD2]);
	mdelay(3);
	// enable DVDD1
	ret = regulator_set_voltage(ctx->regulator[REGULATOR_DVDD1], 855000, 855000);
	if (ret) {
		LOG_INF(
			"set voltage(%s)(%d)(fail),ret(%d)\n",
			reg_names[REGULATOR_DVDD1], 855000, ret);
		return ret;
	}
	LOG_INF("set voltage(%s)(%d)(correct)\n",
		reg_names[REGULATOR_DVDD1], 855000);
	ret = regulator_enable(ctx->regulator[REGULATOR_DVDD1]);
	if (ret) {
		LOG_INF(
			"enable(%s)(fail),ret(%d)\n",
			reg_names[REGULATOR_DVDD1], ret);
		return ret;
	}
	LOG_INF("enable(%s)(correct)\n", reg_names[REGULATOR_DVDD1]);
	mdelay(4);
	// enable DOVDD
	ret = regulator_set_voltage(ctx->regulator[REGULATOR_DOVDD], 1800000, 1800000);
	if (ret) {
		LOG_INF(
			"set voltage(%s)(%d)(fail),ret(%d)\n",
			reg_names[REGULATOR_DOVDD], 1800000, ret);
		return ret;
	}
	LOG_INF("set voltage(%s)(%d)(correct)\n",
		reg_names[REGULATOR_DOVDD], 1800000);
	ret = regulator_enable(ctx->regulator[REGULATOR_DOVDD]);
	if (ret) {
		LOG_INF(
			"enable(%s)(fail),ret(%d)\n",
			reg_names[REGULATOR_DOVDD], ret);
		return ret;
	}
	LOG_INF("enable(%s)(correct)\n", reg_names[REGULATOR_DOVDD]);
	mdelay(1);
	// 3. set gpio
	// mclk_driving_current_on 6MA
	ret = pinctrl_select_state(ctx->pinctrl, ctx->state[STATE_MCLK1_6MA]);
	if (ret < 0) {
		LOG_INF(
			"select(%s)(fail),ret(%d)\n",
			state_names[STATE_MCLK1_6MA], ret);
		return ret;
	}
	LOG_INF("select(%s)(correct)\n", state_names[STATE_MCLK1_6MA]);
	mdelay(6);

	return ret;
}
#endif

static int pwr_seq_reset_view_to_sensing(struct subdrv_ctx *ctx)
{
	int ret = 0;
	struct adaptor_ctx *_adaptor_ctx = NULL;

	if (ctx->i2c_client)
		imgsensor_info.sd = i2c_get_clientdata(ctx->i2c_client);
	if (imgsensor_info.sd)
		_adaptor_ctx = to_ctx(imgsensor_info.sd);
	if (!_adaptor_ctx)
		return -ENODEV;

	/* switch viewing mode sw stand-by to hw stand-by */
	// 1. set gpio
	// xclr(reset) = 0
	ret = pinctrl_select_state(
		_adaptor_ctx->pinctrl,
		_adaptor_ctx->state[STATE_RST1_LOW]);
	if (ret < 0) {
		LOG_INF(
			"select(%s)(fail),ret(%d)\n",
			state_names[STATE_RST1_LOW], ret);
		return ret;
	}
	DEBUG_LOG(ctx, "select(%s)(correct)\n", state_names[STATE_RST1_LOW]);
	mdelay(1);	// response time T4-T6 in datasheet
	// ponv = 0
	ret = pinctrl_select_state(
		_adaptor_ctx->pinctrl,
		_adaptor_ctx->state[STATE_PONV_LOW]);
	if (ret < 0) {
		LOG_INF(
			"select(%s)(fail),ret(%d)\n",
			state_names[STATE_PONV_LOW], ret);
		return ret;
	}
	DEBUG_LOG(ctx, "select(%s)(correct)\n", state_names[STATE_PONV_LOW]);
	mdelay(1);	// response time T4-T6 in datasheet
#ifdef PWR_SEQ_ALL_USE_FOR_AOV_MODE_TRANSITION
	ret = pwr_seq_common_disable_for_mode_transition(_adaptor_ctx);
	if (ret < 0) {
		LOG_INF(
			"pwr_seq_common_disable_for_mode_transition(fail),ret(%d)\n",
			ret);
		return ret;
	}
	DEBUG_LOG(ctx, "pwr_seq_common_disable_for_mode_transition(correct)\n");
	// switch hw stand-by to sensing mode sw stand-by
	ret = pwr_seq_common_enable_for_mode_transition(_adaptor_ctx);
	if (ret < 0) {
		LOG_INF(
			"pwr_seq_common_enable_for_mode_transition(fail),ret(%d)\n",
			ret);
		return ret;
	}
	DEBUG_LOG(ctx, "pwr_seq_common_enable_for_mode_transition)(correct)\n");
#endif
	// xclr(reset) = 1
	ret = pinctrl_select_state(
		_adaptor_ctx->pinctrl,
		_adaptor_ctx->state[STATE_RST1_HIGH]);
	if (ret < 0) {
		LOG_INF(
			"select(%s)(fail),ret(%d)\n",
			state_names[STATE_RST1_HIGH], ret);
		return ret;
	}
	DEBUG_LOG(ctx, "select(%s)(correct)\n", state_names[STATE_RST1_HIGH]);
	mdelay(4);	// response time T7 in datasheet

	return ret;
}

static int pwr_seq_reset_sens_to_viewing(struct subdrv_ctx *ctx)
{
	int ret = 0;
	struct adaptor_ctx *_adaptor_ctx = NULL;

	if (ctx->i2c_client)
		imgsensor_info.sd = i2c_get_clientdata(ctx->i2c_client);
	if (imgsensor_info.sd)
		_adaptor_ctx = to_ctx(imgsensor_info.sd);
	if (!_adaptor_ctx)
		return -ENODEV;

	// i2c bus scl4 on apmcu side
	ret = pinctrl_select_state(
		_adaptor_ctx->pinctrl,
		_adaptor_ctx->state[STATE_SCL_AP]);
	if (ret < 0) {
		LOG_INF(
			"select(%s)(fail),ret(%d)\n",
			state_names[STATE_SCL_AP], ret);
		return ret;
	}
	DEBUG_LOG(ctx, "select(%s)(correct)\n", state_names[STATE_SCL_AP]);

	// i2c bus sda4 on apmcu side
	ret = pinctrl_select_state(
		_adaptor_ctx->pinctrl,
		_adaptor_ctx->state[STATE_SDA_AP]);
	if (ret < 0) {
		LOG_INF(
			"select(%s)(fail),ret(%d)\n",
			state_names[STATE_SDA_AP], ret);
		return ret;
	}
	DEBUG_LOG(ctx, "select(%s)(correct)\n", state_names[STATE_SDA_AP]);
	mdelay(1);

	write_cmos_sensor_8(ctx, 0x0100, 0x00);
	DEBUG_LOG(ctx, "MODE_SEL(%08x)\n", read_cmos_sensor_8(ctx, 0x0100));
	/* switch sensing mode sw stand-by to hw stand-by */
	// 1. set gpio
	// xclr(reset) = 0
	ret = pinctrl_select_state(
		_adaptor_ctx->pinctrl,
		_adaptor_ctx->state[STATE_RST1_LOW]);
	if (ret < 0) {
		LOG_INF(
			"select(%s)(fail),ret(%d)\n",
			state_names[STATE_RST1_LOW], ret);
		return ret;
	}
	DEBUG_LOG(ctx, "select(%s)(correct)\n", state_names[STATE_RST1_LOW]);
	mdelay(1);	// response time T2 in datasheet
#ifdef PWR_SEQ_ALL_USE_FOR_AOV_MODE_TRANSITION
	ret = pwr_seq_common_disable_for_mode_transition(_adaptor_ctx);
	if (ret < 0) {
		LOG_INF(
			"pwr_seq_common_disable_for_mode_transition(fail),ret(%d)\n",
			ret);
		return ret;
	}
	DEBUG_LOG(ctx, "pwr_seq_common_disable_for_mode_transition(correct)\n");

	// switch hw stand-by to viewing mode sw stand-by
	ret = pwr_seq_common_enable_for_mode_transition(_adaptor_ctx);
	if (ret < 0) {
		LOG_INF(
			"pwr_seq_common_enable_for_mode_transition(fail),ret(%d)\n",
			ret);
		return ret;
	}
	DEBUG_LOG(ctx, "pwr_seq_common_enable_for_mode_transition(correct)\n");
#endif
	/* switch hw stand-by to viewing mode sw stand-by */
	// ponv = 1
	ret = pinctrl_select_state(_adaptor_ctx->pinctrl, _adaptor_ctx->state[STATE_PONV_HIGH]);
	if (ret < 0) {
		LOG_INF(
			"select(%s)(fail),ret(%d)\n",
			state_names[STATE_PONV_HIGH], ret);
		return ret;
	}
	DEBUG_LOG(ctx, "select(%s)(correct)\n", state_names[STATE_PONV_HIGH]);
	mdelay(1);
	// xclr(reset) = 1
	ret = pinctrl_select_state(_adaptor_ctx->pinctrl, _adaptor_ctx->state[STATE_RST1_HIGH]);
	if (ret < 0) {
		LOG_INF(
			"select(%s)(fail),ret(%d)\n",
			state_names[STATE_RST1_HIGH], ret);
		return ret;
	}
	DEBUG_LOG(ctx, "select(%s)(correct)\n", state_names[STATE_RST1_HIGH]);
	mdelay(4);	// response time T7 in datasheet

	return ret;
}

static void sensor_init(struct subdrv_ctx *ctx)
{
	imx709_table_write_cmos_sensor(ctx, imx709_init_setting,
		sizeof(imx709_init_setting)/sizeof(kal_uint16));

	LOG_INF("X! init setting!\n");
}	/* sensor_init	*/

static kal_uint32 streaming_control(struct subdrv_ctx *ctx, kal_bool enable)
{
	int ret = 0;

	DEBUG_LOG(ctx, "E!\n");

	LOG_INF("streaming_enable(0=Sw Standby,1=streaming):(%d)\n", enable);
#ifdef AOV_MODE_SENSING
	if (ctx->sensor_mode >= IMGSENSOR_MODE_CUSTOM1 &&
		ctx->sensor_mode < IMGSENSOR_MODE_CUSTOM10) {
		if (ctx->sensor_debug_sensing_ut_on_scp) {
			if (enable)
				stream_refcnt_for_aov = 1;
			else {
				if (stream_refcnt_for_aov) {
					ret = pwr_seq_reset_sens_to_viewing(ctx);
					if (ret)
						LOG_INF(
							"pwr_seq_reset_sens_to_viewing(fail),ret(%d)\n",
							ret);
					else
						LOG_INF(
							"pwr_seq_reset_sens_to_viewing(correct),ret(%d)\n",
							ret);
				}
				stream_refcnt_for_aov = 0;
				LOG_INF(
					"off[correct],stream_refcnt_for_aov(%d)\n",
					stream_refcnt_for_aov);
			}
			LOG_INF(
				"AOV mode(%d) streaming control on scp side\n",
				ctx->sensor_mode);
			return ERROR_NONE;
		}
		LOG_INF(
			"AOV mode(%d) streaming control on apmcu side\n",
			ctx->sensor_mode);
	}
#endif
	if (enable) {
		if (read_cmos_sensor_8(ctx, 0x0350) != 0x01) {
			LOG_INF("single cam scenario enable auto-extend\n");
			write_cmos_sensor_8(ctx, 0x0350, 0x01);
		}

		/* enable temperature sensor, TEMP_SEN_CTL: */
		write_cmos_sensor_8(ctx, 0x0138, 0x01);

		if (!ctx->sensor_debug_sensing_ut_on_scp) {
			stream_refcnt_for_aov = 1;
			LOG_INF(
				"on(correct),stream_refcnt_for_aov(%d)\n",
				stream_refcnt_for_aov);
			if (ctx->sensor_mode >= IMGSENSOR_MODE_CUSTOM1 &&
				ctx->sensor_mode < IMGSENSOR_MODE_CUSTOM10) {
				// write_cmos_sensor_8(ctx, 0x32A0, 0x01);
				write_cmos_sensor_8(ctx, 0x42B0, 0x00);
			}
		}
		write_cmos_sensor_8(ctx, 0x0100, 0X01);
		LOG_INF("MODE_SEL(%08x)\n", read_cmos_sensor_8(ctx, 0x0100));
		ctx->test_pattern = 0;
	} else {
		write_cmos_sensor_8(ctx, 0x0100, 0x00);
		if (!ctx->sensor_debug_sensing_ut_on_scp) {
			if (stream_refcnt_for_aov) {
				if (ctx->sensor_mode >= IMGSENSOR_MODE_CUSTOM1 &&
					ctx->sensor_mode < IMGSENSOR_MODE_CUSTOM10) {
					ret = pwr_seq_reset_sens_to_viewing(ctx);
					if (ret)
						LOG_INF(
							"pwr_seq_reset_sens_to_viewing(fail),ret(%d)\n",
							ret);
					else
						LOG_INF(
							"pwr_seq_reset_sens_to_viewing(correct),ret(%d)\n",
							ret);
				}
			}
			stream_refcnt_for_aov = 0;
			LOG_INF(
				"off(correct),stream_refcnt_for_aov(%d)\n",
				stream_refcnt_for_aov);
		}
	}

	return ERROR_NONE;
}

static void extend_frame_length(struct subdrv_ctx *ctx, kal_uint32 ns)
{
	int i;
	kal_uint32 old_fl = 0;
	kal_uint32 calc_fl = 0;
	kal_uint32 readoutLength = 0;
	kal_uint32 readMargin = 0;
	kal_uint32 per_frame_ns = 0;

	DEBUG_LOG(ctx, "E!\n");

	old_fl = ctx->frame_length;
	readoutLength = ctx->readout_length;
	readMargin = ctx->read_margin;
	per_frame_ns = (kal_uint32)(((unsigned long long)ctx->frame_length *
		(unsigned long long)ctx->line_length * 1000000000) / (unsigned long long)ctx->pclk);

	/* NEED TO FIX start: support 1exp-2exp only; 3exp-?exp instead */
	if (previous_exp_cnt == 1)
		ns = 10000000;

	if (ns)
		ctx->frame_length = (kal_uint32)(((unsigned long long)(per_frame_ns + ns)) *
			ctx->frame_length / per_frame_ns);

	/* fl constraint: normal DOL behavior while stagger seamless switch */
	if (previous_exp_cnt > 1) {
		calc_fl = (readoutLength + readMargin);
		for (i = 1; i < previous_exp_cnt; i++)
			calc_fl += (previous_exp[i] + imgsensor_info.margin * previous_exp_cnt);

		ctx->frame_length = max(calc_fl, ctx->frame_length);
	}
	/* NEED TO FIX end */

	set_cmos_sensor_8(ctx, 0x0104, 0x01);
	write_frame_len(ctx, ctx->frame_length);
	set_cmos_sensor_8(ctx, 0x0104, 0x00);

	commit_write_sensor(ctx);

	ctx->extend_frame_length_en = KAL_TRUE;

	ns = (kal_uint32)(((unsigned long long)(ctx->frame_length - old_fl) *
		(unsigned long long)ctx->line_length * 1000000000) / (unsigned long long)ctx->pclk);
	LOG_INF("new frame len(%d), old frame len(%d), per_frame_ns(%d), add more(%d)ns",
		ctx->frame_length, old_fl, per_frame_ns, ns);
}

static void data_rate_global_timing_phy_ctrl(struct subdrv_ctx *ctx)
{
	DEBUG_LOG(ctx, "E! scenario(%d)\n", ctx->sensor_mode);

	// global timing phy ctrl
	switch (ctx->sensor_mode) {
	case IMGSENSOR_MODE_PREVIEW:
	case IMGSENSOR_MODE_CAPTURE:
		write_cmos_sensor_8(ctx, 0x0808, 0x02);	// PHY_CTRL
		if (!ctx->sensor_debug_dphy_global_timing_continuous_clk) {
			write_cmos_sensor_8(ctx, 0x080A, 0x00);
			write_cmos_sensor_8(ctx, 0x080B, 0x9C);
		} else {
			write_cmos_sensor_8(ctx, 0x080A, 0x00);	// TCLK_POST_EX[9:8]
			write_cmos_sensor_8(ctx, 0x080B, 0x8F);	// TCLK_POST_EX[7:0]
		}
		write_cmos_sensor_8(ctx, 0x080C, 0x00);	// THS_PREPARE_EX[9:8]
		write_cmos_sensor_8(ctx, 0x080D, 0x4F);	// THS_PREPARE_EX[7:0]
		write_cmos_sensor_8(ctx, 0x080E, 0x00);	// THS_ZERO_MIN_EX[9:8]
		write_cmos_sensor_8(ctx, 0x080F, 0x7F);	// THS_ZERO_MIN_EX[7:0]
		write_cmos_sensor_8(ctx, 0x0810, 0x00);	// THS_TRAIL_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0811, 0x47);	// THS_TRAIL_EX[7:0]
		write_cmos_sensor_8(ctx, 0x0812, 0x00);	// TCLK_TRAIL_MIN_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0813, 0x47);	// TCLK_TRAIL_MIN_EX[7:0]
		write_cmos_sensor_8(ctx, 0x0814, 0x00);	// TCLK_PREPARE_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0815, 0x4F);	// TCLK_PREPARE_EX[7:0]
		write_cmos_sensor_8(ctx, 0x0816, 0x01);	// TCLK_ZERO_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0817, 0x1F);	// TCLK_ZERO_EX[7:0]
		write_cmos_sensor_8(ctx, 0x0818, 0x00);	// TLPX_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0819, 0x3F);	// TLPX_EX[7:0]
		if (!ctx->sensor_debug_dphy_global_timing_continuous_clk) {
			write_cmos_sensor_8(ctx, 0x0824, 0x00);	// THS_EXIT_EX[9:8]
			write_cmos_sensor_8(ctx, 0x0825, 0x7F);	// THS_EXIT_EX[7:0]
		} else {
			write_cmos_sensor_8(ctx, 0x0824, 0x00);
			write_cmos_sensor_8(ctx, 0x0825, 0xE6);
		}
		write_cmos_sensor_8(ctx, 0x0826, 0x00);	// TCLK_PRE_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0827, 0x0F);	// TCLK_PRE_EX[7:0]
		break;
	case IMGSENSOR_MODE_VIDEO:
		write_cmos_sensor_8(ctx, 0x0808, 0x02);	// PHY_CTRL
		if (!ctx->sensor_debug_dphy_global_timing_continuous_clk) {
			write_cmos_sensor_8(ctx, 0x080A, 0x00);
			write_cmos_sensor_8(ctx, 0x080B, 0x99);
		} else {
			write_cmos_sensor_8(ctx, 0x080A, 0x00);	// TCLK_POST_EX[9:8]
			write_cmos_sensor_8(ctx, 0x080B, 0x8F);	// TCLK_POST_EX[7:0]
		}
		write_cmos_sensor_8(ctx, 0x080C, 0x00);	// THS_PREPARE_EX[9:8]
		write_cmos_sensor_8(ctx, 0x080D, 0x4F);	// THS_PREPARE_EX[7:0]
		write_cmos_sensor_8(ctx, 0x080E, 0x00);	// THS_ZERO_MIN_EX[9:8]
		write_cmos_sensor_8(ctx, 0x080F, 0x8F);	// THS_ZERO_MIN_EX[7:0]
		write_cmos_sensor_8(ctx, 0x0810, 0x00);	// THS_TRAIL_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0811, 0x4F);	// THS_TRAIL_EX[7:0]
		write_cmos_sensor_8(ctx, 0x0812, 0x00);	// TCLK_TRAIL_MIN_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0813, 0x47);	// TCLK_TRAIL_MIN_EX[7:0]
		write_cmos_sensor_8(ctx, 0x0814, 0x00);	// TCLK_PREPARE_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0815, 0x4F);	// TCLK_PREPARE_EX[7:0]
		write_cmos_sensor_8(ctx, 0x0816, 0x01);	// TCLK_ZERO_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0817, 0x2F);	// TCLK_ZERO_EX[7:0]
		write_cmos_sensor_8(ctx, 0x0818, 0x00);	// TLPX_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0819, 0x3F);	// TLPX_EX[7:0]
		if (!ctx->sensor_debug_dphy_global_timing_continuous_clk) {
			write_cmos_sensor_8(ctx, 0x0824, 0x00);	// THS_EXIT_EX[9:8]
			write_cmos_sensor_8(ctx, 0x0825, 0x7F);	// THS_EXIT_EX[7:0]
		} else {
			write_cmos_sensor_8(ctx, 0x0824, 0x00);
			write_cmos_sensor_8(ctx, 0x0825, 0xE7);
		}
		write_cmos_sensor_8(ctx, 0x0826, 0x00);	// TCLK_PRE_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0827, 0x0F);	// TCLK_PRE_EX[7:0]
		break;
	case IMGSENSOR_MODE_HIGH_SPEED_VIDEO:
	case IMGSENSOR_MODE_SLIM_VIDEO:
		write_cmos_sensor_8(ctx, 0x0808, 0x02);	// PHY_CTRL
		if (!ctx->sensor_debug_dphy_global_timing_continuous_clk) {
			write_cmos_sensor_8(ctx, 0x080A, 0x00);
			write_cmos_sensor_8(ctx, 0x080B, 0x9D);
		} else {
			write_cmos_sensor_8(ctx, 0x080A, 0x00);	// TCLK_POST_EX[9:8]
			write_cmos_sensor_8(ctx, 0x080B, 0x8F);	// TCLK_POST_EX[7:0]
		}
		write_cmos_sensor_8(ctx, 0x080C, 0x00);	// THS_PREPARE_EX[9:8]
		write_cmos_sensor_8(ctx, 0x080D, 0x4F);	// THS_PREPARE_EX[7:0]
		write_cmos_sensor_8(ctx, 0x080E, 0x00);	// THS_ZERO_MIN_EX[9:8]
		write_cmos_sensor_8(ctx, 0x080F, 0x7F);	// THS_ZERO_MIN_EX[7:0]
		write_cmos_sensor_8(ctx, 0x0810, 0x00);	// THS_TRAIL_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0811, 0x47);	// THS_TRAIL_EX[7:0]
		write_cmos_sensor_8(ctx, 0x0812, 0x00);	// TCLK_TRAIL_MIN_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0813, 0x47);	// TCLK_TRAIL_MIN_EX[7:0]
		write_cmos_sensor_8(ctx, 0x0814, 0x00);	// TCLK_PREPARE_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0815, 0x4F);	// TCLK_PREPARE_EX[7:0]
		write_cmos_sensor_8(ctx, 0x0816, 0x01);	// TCLK_ZERO_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0817, 0x1F);	// TCLK_ZERO_EX[7:0]
		write_cmos_sensor_8(ctx, 0x0818, 0x00);	// TLPX_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0819, 0x3F);	// TLPX_EX[7:0]
		if (!ctx->sensor_debug_dphy_global_timing_continuous_clk) {
			write_cmos_sensor_8(ctx, 0x0824, 0x00);	// THS_EXIT_EX[9:8]
			write_cmos_sensor_8(ctx, 0x0825, 0x7F);	// THS_EXIT_EX[7:0]
		} else {
			write_cmos_sensor_8(ctx, 0x0824, 0x00);
			write_cmos_sensor_8(ctx, 0x0825, 0xE6);
		}
		write_cmos_sensor_8(ctx, 0x0826, 0x00);	// TCLK_PRE_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0827, 0x0F);	// TCLK_PRE_EX[7:0]
		break;
	case IMGSENSOR_MODE_CUSTOM1:
		write_cmos_sensor_8(ctx, 0x0808, 0x02);	// PHY_CTRL
		if (!ctx->sensor_debug_dphy_global_timing_continuous_clk) {
			write_cmos_sensor_8(ctx, 0x080A, 0x00);
			write_cmos_sensor_8(ctx, 0x080B, 0xC3);
		} else {
			write_cmos_sensor_8(ctx, 0x080A, 0x00);	// TCLK_POST_EX[9:8]
			write_cmos_sensor_8(ctx, 0x080B, 0x5F);	// TCLK_POST_EX[7:0]
		}
		write_cmos_sensor_8(ctx, 0x080C, 0x00);	// THS_PREPARE_EX[9:8]
		write_cmos_sensor_8(ctx, 0x080D, 0x1F);	// THS_PREPARE_EX[7:0]
		write_cmos_sensor_8(ctx, 0x080E, 0x00);	// THS_ZERO_MIN_EX[9:8]
		write_cmos_sensor_8(ctx, 0x080F, 0x3F);	// THS_ZERO_MIN_EX[7:0]
		write_cmos_sensor_8(ctx, 0x0810, 0x00);	// THS_TRAIL_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0811, 0x1F);	// THS_TRAIL_EX[7:0]
		write_cmos_sensor_8(ctx, 0x0812, 0x00);	// TCLK_TRAIL_MIN_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0813, 0x1F);	// TCLK_TRAIL_MIN_EX[7:0]
		write_cmos_sensor_8(ctx, 0x0814, 0x00);	// TCLK_PREPARE_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0815, 0x1F);	// TCLK_PREPARE_EX[7:0]
		write_cmos_sensor_8(ctx, 0x0816, 0x00);	// TCLK_ZERO_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0817, 0x7F);	// TCLK_ZERO_EX[7:0]
		write_cmos_sensor_8(ctx, 0x0818, 0x00);	// TLPX_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0819, 0x1F);	// TLPX_EX[7:0]
		if (!ctx->sensor_debug_dphy_global_timing_continuous_clk) {
			write_cmos_sensor_8(ctx, 0x0824, 0x00);	// THS_EXIT_EX[9:8]
			write_cmos_sensor_8(ctx, 0x0825, 0x2F);	// THS_EXIT_EX[7:0]
		} else {
			write_cmos_sensor_8(ctx, 0x0824, 0x00);
			write_cmos_sensor_8(ctx, 0x0825, 0xE0);
		}
		write_cmos_sensor_8(ctx, 0x0826, 0x00);	// TCLK_PRE_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0827, 0x0F);	// TCLK_PRE_EX[7:0]
		break;
	case IMGSENSOR_MODE_CUSTOM2:
		write_cmos_sensor_8(ctx, 0x0808, 0x02);	// PHY_CTRL
		if (!ctx->sensor_debug_dphy_global_timing_continuous_clk) {
			write_cmos_sensor_8(ctx, 0x080A, 0x00);
			write_cmos_sensor_8(ctx, 0x080B, 0xC1);
		} else {
			write_cmos_sensor_8(ctx, 0x080A, 0x00);	// TCLK_POST_EX[9:8]
			write_cmos_sensor_8(ctx, 0x080B, 0x5F);	// TCLK_POST_EX[7:0]
		}
		write_cmos_sensor_8(ctx, 0x080C, 0x00);	// THS_PREPARE_EX[9:8]
		write_cmos_sensor_8(ctx, 0x080D, 0x27);	// THS_PREPARE_EX[7:0]
		write_cmos_sensor_8(ctx, 0x080E, 0x00);	// THS_ZERO_MIN_EX[9:8]
		write_cmos_sensor_8(ctx, 0x080F, 0x3F);	// THS_ZERO_MIN_EX[7:0]
		write_cmos_sensor_8(ctx, 0x0810, 0x00);	// THS_TRAIL_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0811, 0x27);	// THS_TRAIL_EX[7:0]
		write_cmos_sensor_8(ctx, 0x0812, 0x00);	// TCLK_TRAIL_MIN_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0813, 0x1F);	// TCLK_TRAIL_MIN_EX[7:0]
		write_cmos_sensor_8(ctx, 0x0814, 0x00);	// TCLK_PREPARE_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0815, 0x1F);	// TCLK_PREPARE_EX[7:0]
		write_cmos_sensor_8(ctx, 0x0816, 0x00);	// TCLK_ZERO_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0817, 0x8F);	// TCLK_ZERO_EX[7:0]
		write_cmos_sensor_8(ctx, 0x0818, 0x00);	// TLPX_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0819, 0x1F);	// TLPX_EX[7:0]
		if (!ctx->sensor_debug_dphy_global_timing_continuous_clk) {
			write_cmos_sensor_8(ctx, 0x0824, 0x00);	// THS_EXIT_EX[9:8]
			write_cmos_sensor_8(ctx, 0x0825, 0x3F);	// THS_EXIT_EX[7:0]
		} else {
			write_cmos_sensor_8(ctx, 0x0824, 0x00);
			write_cmos_sensor_8(ctx, 0x0825, 0xE0);
		}
		write_cmos_sensor_8(ctx, 0x0826, 0x00);	// TCLK_PRE_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0827, 0x0F);	// TCLK_PRE_EX[7:0]
		break;
	case IMGSENSOR_MODE_CUSTOM3:
		write_cmos_sensor_8(ctx, 0x0808, 0x02);	// PHY_CTRL
		if (!ctx->sensor_debug_dphy_global_timing_continuous_clk) {
			write_cmos_sensor_8(ctx, 0x080A, 0x00);
			write_cmos_sensor_8(ctx, 0x080B, 0xC2);
		} else {
			write_cmos_sensor_8(ctx, 0x080A, 0x00);	// TCLK_POST_EX[9:8]
			write_cmos_sensor_8(ctx, 0x080B, 0x5F);	// TCLK_POST_EX[7:0]
		}
		write_cmos_sensor_8(ctx, 0x080C, 0x00);	// THS_PREPARE_EX[9:8]
		write_cmos_sensor_8(ctx, 0x080D, 0x1F);	// THS_PREPARE_EX[7:0]
		write_cmos_sensor_8(ctx, 0x080E, 0x00);	// THS_ZERO_MIN_EX[9:8]
		write_cmos_sensor_8(ctx, 0x080F, 0x3F);	// THS_ZERO_MIN_EX[7:0]
		write_cmos_sensor_8(ctx, 0x0810, 0x00);	// THS_TRAIL_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0811, 0x1F);	// THS_TRAIL_EX[7:0]
		write_cmos_sensor_8(ctx, 0x0812, 0x00);	// TCLK_TRAIL_MIN_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0813, 0x1F);	// TCLK_TRAIL_MIN_EX[7:0]
		write_cmos_sensor_8(ctx, 0x0814, 0x00);	// TCLK_PREPARE_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0815, 0x1F);	// TCLK_PREPARE_EX[7:0]
		write_cmos_sensor_8(ctx, 0x0816, 0x00);	// TCLK_ZERO_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0817, 0x7F);	// TCLK_ZERO_EX[7:0]
		write_cmos_sensor_8(ctx, 0x0818, 0x00);	// TLPX_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0819, 0x1F);	// TLPX_EX[7:0]
		if (!ctx->sensor_debug_dphy_global_timing_continuous_clk) {
			write_cmos_sensor_8(ctx, 0x0824, 0x00);	// THS_EXIT_EX[9:8]
			write_cmos_sensor_8(ctx, 0x0825, 0x2F);	// THS_EXIT_EX[7:0]
		} else {
			write_cmos_sensor_8(ctx, 0x0824, 0x00);
			write_cmos_sensor_8(ctx, 0x0825, 0xE0);
		}
		write_cmos_sensor_8(ctx, 0x0826, 0x00);	// TCLK_PRE_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0827, 0x0F);	// TCLK_PRE_EX[7:0]
		break;
	case IMGSENSOR_MODE_CUSTOM4:
	case IMGSENSOR_MODE_CUSTOM5:
	case IMGSENSOR_MODE_CUSTOM6:
	case IMGSENSOR_MODE_CUSTOM7:
	case IMGSENSOR_MODE_CUSTOM8:
	case IMGSENSOR_MODE_CUSTOM9:
	case IMGSENSOR_MODE_CUSTOM10:
		write_cmos_sensor_8(ctx, 0x0808, 0x02);	// PHY_CTRL
		if (!ctx->sensor_debug_dphy_global_timing_continuous_clk) {
			write_cmos_sensor_8(ctx, 0x080A, 0x00);
			write_cmos_sensor_8(ctx, 0x080B, 0xC9);
		} else {
			write_cmos_sensor_8(ctx, 0x080A, 0x00);	// TCLK_POST_EX[9:8]
			write_cmos_sensor_8(ctx, 0x080B, 0x4F);	// TCLK_POST_EX[7:0]
		}
		write_cmos_sensor_8(ctx, 0x080C, 0x00);	// THS_PREPARE_EX[9:8]
		write_cmos_sensor_8(ctx, 0x080D, 0x17);	// THS_PREPARE_EX[7:0]
		write_cmos_sensor_8(ctx, 0x080E, 0x00);	// THS_ZERO_MIN_EX[9:8]
		write_cmos_sensor_8(ctx, 0x080F, 0x2F);	// THS_ZERO_MIN_EX[7:0]
		write_cmos_sensor_8(ctx, 0x0810, 0x00);	// THS_TRAIL_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0811, 0x17);	// THS_TRAIL_EX[7:0]
		write_cmos_sensor_8(ctx, 0x0812, 0x00);	// TCLK_TRAIL_MIN_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0813, 0x17);	// TCLK_TRAIL_MIN_EX[7:0]
		write_cmos_sensor_8(ctx, 0x0814, 0x00);	// TCLK_PREPARE_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0815, 0x17);	// TCLK_PREPARE_EX[7:0]
		write_cmos_sensor_8(ctx, 0x0816, 0x00);	// TCLK_ZERO_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0817, 0x5F);	// TCLK_ZERO_EX[7:0]
		write_cmos_sensor_8(ctx, 0x0818, 0x00);	// TLPX_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0819, 0x1F);	// TLPX_EX[7:0]
		if (!ctx->sensor_debug_dphy_global_timing_continuous_clk) {
			write_cmos_sensor_8(ctx, 0x0824, 0x00);	// THS_EXIT_EX[9:8]
			write_cmos_sensor_8(ctx, 0x0825, 0x2F);	// THS_EXIT_EX[7:0]
		} else {
			write_cmos_sensor_8(ctx, 0x0824, 0x00);
			write_cmos_sensor_8(ctx, 0x0825, 0xDE);
		}
		write_cmos_sensor_8(ctx, 0x0826, 0x00);	// TCLK_PRE_EX[9:8]
		write_cmos_sensor_8(ctx, 0x0827, 0x0F);	// TCLK_PRE_EX[7:0]
		break;
	default:
		LOG_INF(
			"Wrong scenario(%d) write global timing (fail)!\n",
			ctx->sensor_mode);
		break;
	}
}

static void preview_setting(struct subdrv_ctx *ctx)
{
	imx709_table_write_cmos_sensor(ctx, imx709_preview_setting,
		sizeof(imx709_preview_setting)/sizeof(kal_uint16));

	data_rate_global_timing_phy_ctrl(ctx);

	set_mirror_flip(ctx, ctx->mirror);
	LOG_INF("X! pre setting!\n");
}	/* preview_setting */

static void capture_setting(struct subdrv_ctx *ctx)
{
	imx709_table_write_cmos_sensor(ctx, imx709_capture_setting,
		sizeof(imx709_capture_setting)/sizeof(kal_uint16));

	data_rate_global_timing_phy_ctrl(ctx);

	set_mirror_flip(ctx, ctx->mirror);
	LOG_INF("X! cap setting!\n");
}	/* capture_setting */

static void normal_video_setting(struct subdrv_ctx *ctx)
{
	imx709_table_write_cmos_sensor(ctx, imx709_normal_video_setting,
		sizeof(imx709_normal_video_setting)/sizeof(kal_uint16));

	data_rate_global_timing_phy_ctrl(ctx);

	set_mirror_flip(ctx, ctx->mirror);
	LOG_INF("X! normal video setting!\n");
}	/* normal_video_setting */

static void hs_video_setting(struct subdrv_ctx *ctx)
{
	imx709_table_write_cmos_sensor(ctx, imx709_hs_video_setting,
		sizeof(imx709_hs_video_setting)/sizeof(kal_uint16));

	data_rate_global_timing_phy_ctrl(ctx);

	set_mirror_flip(ctx, ctx->mirror);
	LOG_INF("X! hs video setting!\n");
}	/* hs_video_setting */

static void slim_video_setting(struct subdrv_ctx *ctx)
{
	imx709_table_write_cmos_sensor(ctx, imx709_slim_video_setting,
		sizeof(imx709_slim_video_setting)/sizeof(kal_uint16));

	data_rate_global_timing_phy_ctrl(ctx);

	set_mirror_flip(ctx, ctx->mirror);
	LOG_INF("X! slim video setting!\n");
}	/* slim_video_setting */

static void custom1_setting(struct subdrv_ctx *ctx)
{
	imx709_table_write_cmos_sensor(ctx, imx709_custom1_setting,
		sizeof(imx709_custom1_setting)/sizeof(kal_uint16));

	data_rate_global_timing_phy_ctrl(ctx);

	set_mirror_flip(ctx, ctx->mirror);
	LOG_INF("X! custom1 setting!\n");
}	/* custom1_setting */

static void custom2_setting(struct subdrv_ctx *ctx)
{
	imx709_table_write_cmos_sensor(ctx, imx709_custom2_setting,
		sizeof(imx709_custom2_setting)/sizeof(kal_uint16));

	data_rate_global_timing_phy_ctrl(ctx);

	set_mirror_flip(ctx, ctx->mirror);
	LOG_INF("X! custom2 setting!\n");
}	/* custom2_setting */

static void custom3_setting(struct subdrv_ctx *ctx)
{
	imx709_table_write_cmos_sensor(ctx, imx709_custom3_setting,
		sizeof(imx709_custom3_setting)/sizeof(kal_uint16));

	data_rate_global_timing_phy_ctrl(ctx);

	set_mirror_flip(ctx, ctx->mirror);
	LOG_INF("X! custom3 setting!\n");
}	/* custom3_setting */

static void custom4_setting(struct subdrv_ctx *ctx)
{
	imx709_table_write_cmos_sensor(ctx, imx709_custom4_setting,
		sizeof(imx709_custom4_setting)/sizeof(kal_uint16));

	data_rate_global_timing_phy_ctrl(ctx);

	set_mirror_flip(ctx, ctx->mirror);
	LOG_INF("X! custom4 setting!\n");
}	/* custom4_setting */

static void custom5_setting(struct subdrv_ctx *ctx)
{
	imx709_table_write_cmos_sensor(ctx, imx709_custom5_setting,
		sizeof(imx709_custom5_setting)/sizeof(kal_uint16));

	data_rate_global_timing_phy_ctrl(ctx);

	set_mirror_flip(ctx, ctx->mirror);
	LOG_INF("X! custom5 setting!\n");
}	/* custom5_setting */

static void custom6_setting(struct subdrv_ctx *ctx)
{
	imx709_table_write_cmos_sensor(ctx, imx709_custom6_setting,
		sizeof(imx709_custom6_setting)/sizeof(kal_uint16));

	data_rate_global_timing_phy_ctrl(ctx);

	set_mirror_flip(ctx, ctx->mirror);
	LOG_INF("X! custom6 setting!\n");
}	/* custom6_setting */

static void custom7_setting(struct subdrv_ctx *ctx)
{
	imx709_table_write_cmos_sensor(ctx, imx709_custom7_setting,
		sizeof(imx709_custom7_setting)/sizeof(kal_uint16));

	data_rate_global_timing_phy_ctrl(ctx);

	set_mirror_flip(ctx, ctx->mirror);
	LOG_INF("X! custom7 setting!\n");
}	/* custom7_setting */

static void custom8_setting(struct subdrv_ctx *ctx)
{
	imx709_table_write_cmos_sensor(ctx, imx709_custom8_setting,
		sizeof(imx709_custom8_setting)/sizeof(kal_uint16));

	data_rate_global_timing_phy_ctrl(ctx);

	set_mirror_flip(ctx, ctx->mirror);
	LOG_INF("X! custom8 setting!\n");
}	/* custom8_setting */

static void custom9_setting(struct subdrv_ctx *ctx)
{
	imx709_table_write_cmos_sensor(ctx, imx709_custom9_setting,
		sizeof(imx709_custom9_setting)/sizeof(kal_uint16));

	data_rate_global_timing_phy_ctrl(ctx);

	set_mirror_flip(ctx, ctx->mirror);
	LOG_INF("X! custom9 setting!\n");
}	/* custom9_setting */

static void custom10_setting(struct subdrv_ctx *ctx)
{
	imx709_table_write_cmos_sensor(ctx, imx709_custom10_setting,
		sizeof(imx709_custom10_setting)/sizeof(kal_uint16));

	data_rate_global_timing_phy_ctrl(ctx);

	set_mirror_flip(ctx, ctx->mirror);
	LOG_INF("X! custom10 setting!\n");
}	/* custom10_setting */

static void motion_detection1_setting(struct subdrv_ctx *ctx)
{
	imx709_table_write_cmos_sensor(ctx, imx709_md1_setting,
		sizeof(imx709_md1_setting)/sizeof(kal_uint16));

	LOG_INF("X! motion detection1 setting!\n");
}	/* motion_detection1_setting */

static void motion_detection2_setting(struct subdrv_ctx *ctx)
{
	imx709_table_write_cmos_sensor(ctx, imx709_md2_setting,
		sizeof(imx709_md2_setting)/sizeof(kal_uint16));

	LOG_INF("X! motion detection2 setting!\n");
}	/* motion_detection2_setting */

static void motion_detection3_setting(struct subdrv_ctx *ctx)
{
	imx709_table_write_cmos_sensor(ctx, imx709_md3_setting,
		sizeof(imx709_md3_setting)/sizeof(kal_uint16));

	LOG_INF("X! motion detection3 setting!\n");
}	/* motion_detection3_setting */

static void hdr_write_tri_shutter_w_gph(struct subdrv_ctx *ctx,
		kal_uint32 le, kal_uint32 me, kal_uint32 se, kal_bool gph)
{
	kal_uint16 exposure_cnt = 0;
	kal_uint32 fineIntegTime = 0;
	int i;

	DEBUG_LOG(ctx, "E!\n");

	fineIntegTime = fine_integ_line_table[ctx->current_scenario_id];

	le = FINE_INTEG_CONVERT(le, fineIntegTime);
	me = 0;	// imx709 only support 2EXP
	se = FINE_INTEG_CONVERT(se, fineIntegTime);

	if (le) {
		exposure_cnt++;
		le = (kal_uint16)max(imgsensor_info.min_shutter, (kal_uint32)le);
		le = round_up((le) / exposure_cnt, 4) * exposure_cnt;
	}

	/* if (me) {
	 *	exposure_cnt++;
	 *	me = (kal_uint16)max(imgsensor_info.min_shutter, (kal_uint32)me);
	 *	me = round_up((me) / exposure_cnt, 4) * exposure_cnt;
	 * }
	 */

	if (se) {
		exposure_cnt++;
		se = (kal_uint16)max(imgsensor_info.min_shutter, (kal_uint32)se);
		se = round_up((se) / exposure_cnt, 4) * exposure_cnt;
	}

	ctx->frame_length =
		max((kal_uint32)(le + me + se + imgsensor_info.margin*exposure_cnt*exposure_cnt),
		ctx->min_frame_length);
	ctx->frame_length = min(ctx->frame_length, imgsensor_info.max_frame_length);

	for (i = 0; i < previous_exp_cnt; i++)
		previous_exp[i] = 0;
	previous_exp[0] = le;
	switch (exposure_cnt) {
	/* case 3:
	 *	previous_exp[1] = me;
	 *	previous_exp[2] = se;
	 *	break;
	 */
	case 2:
		previous_exp[1] = se;
		previous_exp[2] = 0;
		break;
	case 1:
	default:
		previous_exp[1] = 0;
		previous_exp[2] = 0;
		break;
	}
	previous_exp_cnt = exposure_cnt;

	if (le)
		le = le / exposure_cnt;
	/* if (me)
	 *	me = me / exposure_cnt;
	 */
	if (se)
		se = se / exposure_cnt;

	if (gph)
		set_cmos_sensor_8(ctx, 0x0104, 0x01);

	set_auto_flicker(ctx);
	// write_frame_len(ctx, ctx->frame_length);

	/* Long exposure */
	set_cmos_sensor_8(ctx, 0x0202, (le >> 8) & 0xFF);
	set_cmos_sensor_8(ctx, 0x0203, le & 0xFF);
	/* Middle exposure */
	/*if (me) {
	 *	// MID_COARSE_INTEG_TIME[15:8]
	 *	set_cmos_sensor_8(ctx, 0x313A, (me >> 8) & 0xFF);
	 *	// MID_COARSE_INTEG_TIME[7:0]
	 *	set_cmos_sensor_8(ctx, 0x313B, me & 0xFF);
	 * }
	 */
	/* Short exposure */
	set_cmos_sensor_8(ctx, 0x0224, (se >> 8) & 0xFF);
	set_cmos_sensor_8(ctx, 0x0225, se & 0xFF);
	if (!ctx->ae_ctrl_gph_en) {
		if (gph)
			set_cmos_sensor_8(ctx, 0x0104, 0x00);

		commit_write_sensor(ctx);
	}
	DEBUG_LOG(ctx,
		"X! le:(0x%x), me:(0x%x), se:(0x%x) autoflicker_en(%d) frame_length(%d)\n",
		le, me, se, ctx->autoflicker_en, ctx->frame_length);
}

static void hdr_write_tri_shutter(struct subdrv_ctx *ctx,
		kal_uint32 le, kal_uint32 me, kal_uint32 se)
{
	hdr_write_tri_shutter_w_gph(ctx, le, me, se, KAL_TRUE);
}

static void hdr_write_tri_gain_w_gph(struct subdrv_ctx *ctx,
		kal_uint32 lg, kal_uint32 mg, kal_uint32 sg, kal_bool gph)
{
	kal_uint16 reg_lg = 0;
	kal_uint16 reg_mg = 0;
	kal_uint16 reg_sg = 0;

	DEBUG_LOG(ctx, "E!\n");

	reg_lg = gain2reg(ctx, lg);
	reg_mg = 0;	// imx709 only support 2EXP
	reg_sg = gain2reg(ctx, sg);

	ctx->gain = reg_lg;
	if (gph && !ctx->ae_ctrl_gph_en)
		set_cmos_sensor_8(ctx, 0x0104, 0x01);
	/* Long Exp Gain */
	set_cmos_sensor_8(ctx, 0x0204, (reg_lg>>8) & 0xFF);
	set_cmos_sensor_8(ctx, 0x0205, reg_lg & 0xFF);
	/* Middle Exp Gain */
	/* if (mg != 0) {
	 *	set_cmos_sensor_8(ctx, 0x313C, (reg_mg>>8) & 0xFF);
	 *	set_cmos_sensor_8(ctx, 0x313D, reg_mg & 0xFF);
	 * }
	 */
	/* Short Exp Gain */
	set_cmos_sensor_8(ctx, 0x0216, (reg_sg>>8) & 0xFF);
	set_cmos_sensor_8(ctx, 0x0217, reg_sg & 0xFF);
	if (gph)
		set_cmos_sensor_8(ctx, 0x0104, 0x00);

	commit_write_sensor(ctx);

	DEBUG_LOG(ctx,
		"lg:(0x%x), reg_lg:(0x%x), mg:(0x%x), reg_mg:(0x%x), sg:(0x%x), reg_sg:(0x%x)\n",
		lg, reg_lg, mg, reg_mg, sg, reg_sg);
}

static void hdr_write_tri_gain(struct subdrv_ctx *ctx,
		kal_uint32 lg, kal_uint32 mg, kal_uint32 sg)
{
	hdr_write_tri_gain_w_gph(ctx, lg, mg, sg, KAL_TRUE);
}

#define FMC_GPH_START do { \
	write_cmos_sensor_8(ctx, 0x0104, 0x01); \
	write_cmos_sensor_8(ctx, 0x3010, 0x02); \
} while (0)

#define FMC_GPH_END do { \
	write_cmos_sensor_8(ctx, 0x0104, 0x00); \
} while (0)

static kal_uint32 seamless_switch(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, uint32_t *ae_ctrl)
{
	ctx->extend_frame_length_en = KAL_FALSE;

	switch (scenario_id) {
	default:
	{
		LOG_INF(
			"error! wrong setting in set_seamless_switch(%u)",
			scenario_id);
		return 0xff;
	}
		break;
	}

	// set_cmos_sensor_8(ctx, 0x3010, 0x00);
	DEBUG_LOG(ctx,
		"success, scenario is switched to (%u)",
		scenario_id);
	return 0;
}

/*************************************************************************
 * FUNCTION
 *	get_imgsensor_id
 *
 * DESCRIPTION
 *	This function get the sensor ID
 *
 * PARAMETERS
 *	*sensorID : return the sensor ID
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static int get_imgsensor_id(struct subdrv_ctx *ctx, UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	/* sensor may have two i2c address 0x34 or 0x20,
	 * we should detect the module used i2c address
	 */
	DEBUG_LOG(ctx, "for imx709 id\n");
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		ctx->i2c_write_id = imgsensor_info.i2c_addr_table[i];
		do {
			*sensor_id = ((read_cmos_sensor_8(ctx, 0x0016) << 8) |
							read_cmos_sensor_8(ctx, 0x0017));
			LOG_INF(
				"read_0x0000=(0x%x), 0x0001=(0x%x), 0x0000_0001=(0x%x)\n",
				read_cmos_sensor_8(ctx, 0x0016),
				read_cmos_sensor_8(ctx, 0x0017),
				read_cmos_sensor_16(ctx, 0x0000));
			if (*sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id(0x%x), sensor id(0x%x)\n",
						ctx->i2c_write_id, *sensor_id);

				return ERROR_NONE;
			}
			LOG_INF("Read sensor id fail, id(0x%x)\n", ctx->i2c_write_id);
			LOG_INF(
				"sensor_id(0x%x), imgsensor_info.sensor_id(0x%x)\n",
				*sensor_id, imgsensor_info.sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		/* if Sensor ID is not correct,
		 * Must set *sensor_id to 0xFFFFFFFF
		 */
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}

/*************************************************************************
 * FUNCTION
 *	open
 *
 * DESCRIPTION
 *	This function initialize the registers of CMOS sensor
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static int open(struct subdrv_ctx *ctx)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint16 sensor_id = 0;

	DEBUG_LOG(ctx, "for imx709 start\n");
	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 * we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		ctx->i2c_write_id = imgsensor_info.i2c_addr_table[i];
		do {
			sensor_id = ((read_cmos_sensor_8(ctx, 0x0016) << 8) |
							read_cmos_sensor_8(ctx, 0x0017));
			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id(0x%x), sensor id(0x%x)\n",
					ctx->i2c_write_id, sensor_id);
				break;
			}
			LOG_INF("Read sensor id fail, id(0x%x)\n", ctx->i2c_write_id);
			retry--;
		} while (retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 2;
	}
	if (imgsensor_info.sensor_id != sensor_id)
		return ERROR_SENSOR_CONNECT_FAIL;

#if EEPROM_READY
	LOG_INF("write_sensor_QSC Start\n");
	write_sensor_QSC(ctx);
	LOG_INF("write_sensor_QSC End\n");
#endif

	ctx->autoflicker_en = KAL_FALSE;
	ctx->sensor_mode = IMGSENSOR_MODE_INIT;
	ctx->shutter = 0x3D0;
	ctx->gain = BASEGAIN * 4;
	ctx->pclk = imgsensor_info.pre.pclk;
	ctx->frame_length = imgsensor_info.pre.framelength;
	ctx->line_length = imgsensor_info.pre.linelength;
	ctx->min_frame_length = imgsensor_info.pre.framelength;
	ctx->dummy_pixel = 0;
	ctx->dummy_line = 0;
	ctx->ihdr_mode = 0;
	ctx->test_pattern = KAL_FALSE;
	ctx->current_fps = imgsensor_info.pre.max_framerate;

	return ERROR_NONE;
}	/* open */

/*************************************************************************
 * FUNCTION
 *	close
 *
 * DESCRIPTION
 *
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static int close(struct subdrv_ctx *ctx)
{
	LOG_INF("E\n");

	streaming_control(ctx, KAL_FALSE);

	return ERROR_NONE;
}	/* close */

/*************************************************************************
 * FUNCTION
 * preview
 *
 * DESCRIPTION
 *	This function start the sensor preview.
 *
 * PARAMETERS
 *	*image_window : address pointer of pixel numbers in one period of HSYNC
 *  *sensor_config_data : address pointer of line numbers in one period of VSYNC
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 preview(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	DEBUG_LOG(ctx, "E!\n");
	ctx->sensor_mode = IMGSENSOR_MODE_PREVIEW;
	ctx->pclk = imgsensor_info.pre.pclk;
	ctx->line_length = imgsensor_info.pre.linelength;
	ctx->frame_length = imgsensor_info.pre.framelength;
	ctx->min_frame_length = imgsensor_info.pre.framelength;
	ctx->readout_length = imgsensor_info.pre.readout_length;
	ctx->read_margin = imgsensor_info.pre.read_margin;
	ctx->autoflicker_en = KAL_FALSE;
	preview_setting(ctx);
	DEBUG_LOG(ctx,
		"X! w*h:(%d) x (%d)\n",
		ctx->line_length, ctx->frame_length);
	mdelay(8);

	return ERROR_NONE;
}	/* preview */

/*************************************************************************
 * FUNCTION
 *	capture
 *
 * DESCRIPTION
 *	This function setup the CMOS sensor in capture MY_OUTPUT mode
 *
 * PARAMETERS
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 capture(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	DEBUG_LOG(ctx, "E!\n");
	ctx->sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (ctx->current_fps != imgsensor_info.cap.max_framerate)
		LOG_INF(
			"Warning: current_fps(%d) fps is not support, so use cap's setting:(%d) fps!\n",
			ctx->current_fps, imgsensor_info.cap.max_framerate / 10);
	ctx->pclk = imgsensor_info.cap.pclk;
	ctx->line_length = imgsensor_info.cap.linelength;
	ctx->frame_length = imgsensor_info.cap.framelength;
	ctx->min_frame_length = imgsensor_info.cap.framelength;
	ctx->readout_length = imgsensor_info.cap.readout_length;
	ctx->read_margin = imgsensor_info.cap.read_margin;
	ctx->autoflicker_en = KAL_FALSE;
	capture_setting(ctx);

	return ERROR_NONE;
}	/* capture */

static kal_uint32 normal_video(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	DEBUG_LOG(ctx, "E!\n");
	ctx->sensor_mode = IMGSENSOR_MODE_VIDEO;
	ctx->pclk = imgsensor_info.normal_video.pclk;
	ctx->line_length = imgsensor_info.normal_video.linelength;
	ctx->frame_length = imgsensor_info.normal_video.framelength;
	ctx->min_frame_length = imgsensor_info.normal_video.framelength;
	ctx->readout_length = imgsensor_info.normal_video.readout_length;
	ctx->read_margin = imgsensor_info.normal_video.read_margin;
	ctx->autoflicker_en = KAL_FALSE;
	normal_video_setting(ctx);

	return ERROR_NONE;
}	/* normal_video */

static kal_uint32 hs_video(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	DEBUG_LOG(ctx, "E!\n");
	ctx->sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	ctx->pclk = imgsensor_info.hs_video.pclk;
	ctx->line_length = imgsensor_info.hs_video.linelength;
	ctx->frame_length = imgsensor_info.hs_video.framelength;
	ctx->min_frame_length = imgsensor_info.hs_video.framelength;
	ctx->readout_length = imgsensor_info.hs_video.readout_length;
	ctx->read_margin = imgsensor_info.hs_video.read_margin;
	ctx->dummy_line = 0;
	ctx->dummy_pixel = 0;
	ctx->autoflicker_en = KAL_FALSE;
	hs_video_setting(ctx);

	return ERROR_NONE;
}	/* hs_video */

static kal_uint32 slim_video(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	DEBUG_LOG(ctx, "E!\n");
	ctx->sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	ctx->pclk = imgsensor_info.slim_video.pclk;
	ctx->line_length = imgsensor_info.slim_video.linelength;
	ctx->frame_length = imgsensor_info.slim_video.framelength;
	ctx->min_frame_length = imgsensor_info.slim_video.framelength;
	ctx->readout_length = imgsensor_info.slim_video.readout_length;
	ctx->read_margin = imgsensor_info.slim_video.read_margin;
	ctx->dummy_line = 0;
	ctx->dummy_pixel = 0;
	ctx->autoflicker_en = KAL_FALSE;
	slim_video_setting(ctx);

	return ERROR_NONE;
}	/* slim_video */

static kal_uint32 custom1(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	struct adaptor_ctx *_adaptor_ctx = NULL;
	int ret = 0;

	if (ctx->i2c_client)
		imgsensor_info.sd = i2c_get_clientdata(ctx->i2c_client);
	if (imgsensor_info.sd)
		_adaptor_ctx = to_ctx(imgsensor_info.sd);
	if (!_adaptor_ctx)
		return -ENODEV;

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	ctx->pclk = imgsensor_info.custom1.pclk;
	ctx->line_length = imgsensor_info.custom1.linelength;
	ctx->frame_length = imgsensor_info.custom1.framelength;
	ctx->min_frame_length = imgsensor_info.custom1.framelength;
	ctx->readout_length = imgsensor_info.custom1.readout_length;
	ctx->read_margin = imgsensor_info.custom1.read_margin;
	ctx->autoflicker_en = KAL_FALSE;
	switch (ctx->sensor_mode_ops) {
	case AOV_MODE_CTRL_OPS_SENSING_CTRL:
	default:
		custom1_setting(ctx);	/* custom1 */
		break;
	case AOV_MODE_CTRL_OPS_MONTION_DETECTION_CTRL:
		motion_detection1_setting(ctx);	/* md1 */
		/* set eint gpio */
		ret = pinctrl_select_state(
			_adaptor_ctx->pinctrl,
			_adaptor_ctx->state[STATE_EINT]);
		if (ret < 0)
			LOG_INF(
				"select(%s)(fail),ret(%d)\n",
				state_names[STATE_EINT], ret);
		else
			DEBUG_LOG(ctx, "select(%s)(correct)\n", state_names[STATE_EINT]);
		break;
	}

	return ERROR_NONE;
}	/* custom1 */

static kal_uint32 custom2(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	struct adaptor_ctx *_adaptor_ctx = NULL;
	int ret = 0;

	if (ctx->i2c_client)
		imgsensor_info.sd = i2c_get_clientdata(ctx->i2c_client);
	if (imgsensor_info.sd)
		_adaptor_ctx = to_ctx(imgsensor_info.sd);
	if (!_adaptor_ctx)
		return -ENODEV;

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM2;
	ctx->pclk = imgsensor_info.custom2.pclk;
	ctx->line_length = imgsensor_info.custom2.linelength;
	ctx->frame_length = imgsensor_info.custom2.framelength;
	ctx->min_frame_length = imgsensor_info.custom2.framelength;
	ctx->readout_length = imgsensor_info.custom2.readout_length;
	ctx->read_margin = imgsensor_info.custom2.read_margin;
	ctx->autoflicker_en = KAL_FALSE;

	switch (ctx->sensor_mode_ops) {
	case AOV_MODE_CTRL_OPS_SENSING_CTRL:
	default:
		custom2_setting(ctx);	/* custom2 */
		break;
	case AOV_MODE_CTRL_OPS_MONTION_DETECTION_CTRL:
		motion_detection2_setting(ctx);	/* md2 */
		/* set eint gpio */
		ret = pinctrl_select_state(
			_adaptor_ctx->pinctrl,
			_adaptor_ctx->state[STATE_EINT]);
		if (ret < 0)
			LOG_INF(
				"select(%s)(fail),ret(%d)\n",
				state_names[STATE_EINT], ret);
		else
			DEBUG_LOG(ctx, "select(%s)(correct)\n", state_names[STATE_EINT]);
		break;
	}

	return ERROR_NONE;
}	/* custom2 */

static kal_uint32 custom3(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	struct adaptor_ctx *_adaptor_ctx = NULL;
	int ret = 0;

	if (ctx->i2c_client)
		imgsensor_info.sd = i2c_get_clientdata(ctx->i2c_client);
	if (imgsensor_info.sd)
		_adaptor_ctx = to_ctx(imgsensor_info.sd);
	if (!_adaptor_ctx)
		return -ENODEV;

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM3;
	ctx->pclk = imgsensor_info.custom3.pclk;
	ctx->line_length = imgsensor_info.custom3.linelength;
	ctx->frame_length = imgsensor_info.custom3.framelength;
	ctx->min_frame_length = imgsensor_info.custom3.framelength;
	ctx->readout_length = imgsensor_info.custom3.readout_length;
	ctx->read_margin = imgsensor_info.custom3.read_margin;
	ctx->autoflicker_en = KAL_FALSE;

	switch (ctx->sensor_mode_ops) {
	case AOV_MODE_CTRL_OPS_SENSING_CTRL:
	default:
		custom3_setting(ctx);	/* custom3 */
		break;
	case AOV_MODE_CTRL_OPS_MONTION_DETECTION_CTRL:
		motion_detection3_setting(ctx);	/* md3 */
		/* set eint gpio */
		ret = pinctrl_select_state(
			_adaptor_ctx->pinctrl,
			_adaptor_ctx->state[STATE_EINT]);
		if (ret < 0)
			LOG_INF(
				"select(%s)(fail),ret(%d)\n",
				state_names[STATE_EINT], ret);
		else
			DEBUG_LOG(ctx, "select(%s)(correct)\n", state_names[STATE_EINT]);
		break;
	}

	return ERROR_NONE;
}	/* custom3 */

static kal_uint32 custom4(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	DEBUG_LOG(ctx, "E!\n");
	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM4;
	ctx->pclk = imgsensor_info.custom4.pclk;
	ctx->line_length = imgsensor_info.custom4.linelength;
	ctx->frame_length = imgsensor_info.custom4.framelength;
	ctx->min_frame_length = imgsensor_info.custom4.framelength;
	ctx->readout_length = imgsensor_info.custom4.readout_length;
	ctx->read_margin = imgsensor_info.custom4.read_margin;
	ctx->autoflicker_en = KAL_FALSE;
	custom4_setting(ctx);	/* custom4 */

	return ERROR_NONE;
}	/* custom4 */

static kal_uint32 custom5(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	DEBUG_LOG(ctx, "E!\n");
	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM5;
	ctx->pclk = imgsensor_info.custom5.pclk;
	ctx->line_length = imgsensor_info.custom5.linelength;
	ctx->frame_length = imgsensor_info.custom5.framelength;
	ctx->min_frame_length = imgsensor_info.custom5.framelength;
	ctx->readout_length = imgsensor_info.custom5.readout_length;
	ctx->read_margin = imgsensor_info.custom5.read_margin;
	ctx->autoflicker_en = KAL_FALSE;
	custom5_setting(ctx);	/* custom5 */

	return ERROR_NONE;
}	/* custom5 */

static kal_uint32 custom6(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	DEBUG_LOG(ctx, "E!\n");
	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM6;
	ctx->pclk = imgsensor_info.custom6.pclk;
	ctx->line_length = imgsensor_info.custom6.linelength;
	ctx->frame_length = imgsensor_info.custom6.framelength;
	ctx->min_frame_length = imgsensor_info.custom6.framelength;
	ctx->readout_length = imgsensor_info.custom6.readout_length;
	ctx->read_margin = imgsensor_info.custom6.read_margin;
	ctx->autoflicker_en = KAL_FALSE;
	custom6_setting(ctx);	/* custom6 */

	return ERROR_NONE;
}	/* custom6 */

static kal_uint32 custom7(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	DEBUG_LOG(ctx, "E!\n");
	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM7;
	ctx->pclk = imgsensor_info.custom7.pclk;
	ctx->line_length = imgsensor_info.custom7.linelength;
	ctx->frame_length = imgsensor_info.custom7.framelength;
	ctx->min_frame_length = imgsensor_info.custom7.framelength;
	ctx->readout_length = imgsensor_info.custom7.readout_length;
	ctx->read_margin = imgsensor_info.custom7.read_margin;
	ctx->autoflicker_en = KAL_FALSE;
	custom7_setting(ctx);	/* custom7 */

	return ERROR_NONE;
}	/* custom7 */

static kal_uint32 custom8(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	DEBUG_LOG(ctx, "E!\n");
	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM8;
	ctx->pclk = imgsensor_info.custom8.pclk;
	ctx->line_length = imgsensor_info.custom8.linelength;
	ctx->frame_length = imgsensor_info.custom8.framelength;
	ctx->min_frame_length = imgsensor_info.custom8.framelength;
	ctx->readout_length = imgsensor_info.custom8.readout_length;
	ctx->read_margin = imgsensor_info.custom8.read_margin;
	ctx->autoflicker_en = KAL_FALSE;
	custom8_setting(ctx);	/* custom8 */

	return ERROR_NONE;
}	/* custom8 */

static kal_uint32 custom9(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	DEBUG_LOG(ctx, "E!\n");
	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM9;
	ctx->pclk = imgsensor_info.custom9.pclk;
	ctx->line_length = imgsensor_info.custom9.linelength;
	ctx->frame_length = imgsensor_info.custom9.framelength;
	ctx->min_frame_length = imgsensor_info.custom9.framelength;
	ctx->readout_length = imgsensor_info.custom9.readout_length;
	ctx->read_margin = imgsensor_info.custom9.read_margin;
	ctx->autoflicker_en = KAL_FALSE;
	custom9_setting(ctx);	/* custom9 */

	return ERROR_NONE;
}	/* custom9 */

static kal_uint32 custom10(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	DEBUG_LOG(ctx, "E!\n");
	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM10;
	ctx->pclk = imgsensor_info.custom10.pclk;
	ctx->line_length = imgsensor_info.custom10.linelength;
	ctx->frame_length = imgsensor_info.custom10.framelength;
	ctx->min_frame_length = imgsensor_info.custom10.framelength;
	ctx->readout_length = imgsensor_info.custom10.readout_length;
	ctx->read_margin = imgsensor_info.custom10.read_margin;
	ctx->autoflicker_en = KAL_FALSE;
	custom10_setting(ctx);
	DEBUG_LOG(ctx,
		"X! w*h:(%d) x (%d)\n",
		ctx->line_length, ctx->frame_length);
	mdelay(8);

	return ERROR_NONE;
}	/* custom10 */

static int get_resolution(struct subdrv_ctx *ctx,
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	int i = 0;

	DEBUG_LOG(ctx, "E!\n");

	for (i = SENSOR_SCENARIO_ID_MIN; i < SENSOR_SCENARIO_ID_MAX; i++) {
		if (i < imgsensor_info.sensor_mode_num &&
			i < ARRAY_SIZE(imgsensor_winsize_info)) {
			sensor_resolution->SensorWidth[i] =
				imgsensor_winsize_info[i].w2_tg_size;
			sensor_resolution->SensorHeight[i] =
				imgsensor_winsize_info[i].h2_tg_size;
		} else {
			sensor_resolution->SensorWidth[i] = 0;
			sensor_resolution->SensorHeight[i] = 0;
		}
	}

	return ERROR_NONE;
}	/* get_resolution */

static int get_info(struct subdrv_ctx *ctx,
		enum MSDK_SCENARIO_ID_ENUM scenario_id,
		MSDK_SENSOR_INFO_STRUCT *sensor_info,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	DEBUG_LOG(ctx, "scenario_id(%d)\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 5; /* not use */

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SensorOutputDataFormat =
		imgsensor_info.sensor_output_dataformat;

	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_NORMAL_PREVIEW] =
		imgsensor_info.pre_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_NORMAL_CAPTURE] =
		imgsensor_info.cap_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_NORMAL_VIDEO] =
		imgsensor_info.video_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO] =
		imgsensor_info.hs_video_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_SLIM_VIDEO] =
		imgsensor_info.slim_video_delay_frame;
#ifdef AOV_MODE_SENSING
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_CUSTOM1] =
		imgsensor_info.custom1_delay_frame;
	sensor_info->Mode_AE_Ctrl_Support[SENSOR_SCENARIO_ID_CUSTOM1] =
		imgsensor_info.custom1_ae_ctrl_support;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_CUSTOM2] =
		imgsensor_info.custom2_delay_frame;
	sensor_info->Mode_AE_Ctrl_Support[SENSOR_SCENARIO_ID_CUSTOM2] =
		imgsensor_info.custom2_ae_ctrl_support;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_CUSTOM3] =
		imgsensor_info.custom3_delay_frame;
	sensor_info->Mode_AE_Ctrl_Support[SENSOR_SCENARIO_ID_CUSTOM3] =
		imgsensor_info.custom3_ae_ctrl_support;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_CUSTOM4] =
		imgsensor_info.custom4_delay_frame;
	sensor_info->Mode_AE_Ctrl_Support[SENSOR_SCENARIO_ID_CUSTOM4] =
		imgsensor_info.custom4_ae_ctrl_support;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_CUSTOM5] =
		imgsensor_info.custom5_delay_frame;
	sensor_info->Mode_AE_Ctrl_Support[SENSOR_SCENARIO_ID_CUSTOM5] =
		imgsensor_info.custom5_ae_ctrl_support;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_CUSTOM6] =
		imgsensor_info.custom6_delay_frame;
	sensor_info->Mode_AE_Ctrl_Support[SENSOR_SCENARIO_ID_CUSTOM6] =
		imgsensor_info.custom6_ae_ctrl_support;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_CUSTOM7] =
		imgsensor_info.custom7_delay_frame;
	sensor_info->Mode_AE_Ctrl_Support[SENSOR_SCENARIO_ID_CUSTOM7] =
		imgsensor_info.custom7_ae_ctrl_support;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_CUSTOM8] =
		imgsensor_info.custom8_delay_frame;
	sensor_info->Mode_AE_Ctrl_Support[SENSOR_SCENARIO_ID_CUSTOM8] =
		imgsensor_info.custom8_ae_ctrl_support;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_CUSTOM9] =
		imgsensor_info.custom9_delay_frame;
	sensor_info->Mode_AE_Ctrl_Support[SENSOR_SCENARIO_ID_CUSTOM9] =
		imgsensor_info.custom9_ae_ctrl_support;
#endif
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_CUSTOM10] =
		imgsensor_info.custom10_delay_frame;
	sensor_info->Mode_AE_Ctrl_Support[SENSOR_SCENARIO_ID_CUSTOM10] =
		imgsensor_info.custom10_ae_ctrl_support;

	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	sensor_info->AESensorGainDelayFrame =
		imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame =
		imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->TEMPERATURE_SUPPORT = imgsensor_info.temperature_support;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
	sensor_info->PDAF_Support = 0;

	sensor_info->HDR_Support = HDR_SUPPORT_STAGGER_FDOL;

	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3;	/* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2;	/* not use */
	sensor_info->SensorPixelClockCount = 3;	/* not use */
	sensor_info->SensorDataLatchCount = 2;	/* not use */

	sensor_info->SensorWidthSampling = 0;	/* 0 is default 1x */
	sensor_info->SensorHightSampling = 0;	/* 0 is default 1x */
	sensor_info->SensorPacketECCOrder = 1;
	sensor_info->FrameTimeDelayFrame = imgsensor_info.frame_time_delay_frame;

	return ERROR_NONE;
}	/* get_info */

static int control(struct subdrv_ctx *ctx,
			enum SENSOR_SCENARIO_ID_ENUM scenario_id,
			MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	int ret = 0;

	DEBUG_LOG(ctx, "scenario_id(%d)\n", scenario_id);
	ctx->current_scenario_id = scenario_id;

	previous_exp_cnt = 0;

	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		/* initail sequence write in */
		sensor_init(ctx);
		preview(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		/* initail sequence write in */
		sensor_init(ctx);
		capture(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		/* initail sequence write in */
		sensor_init(ctx);
		normal_video(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		/* initail sequence write in */
		sensor_init(ctx);
		hs_video(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		/* initail sequence write in */
		sensor_init(ctx);
		slim_video(ctx, image_window, sensor_config_data);
		break;
#ifdef AOV_MODE_SENSING
	case SENSOR_SCENARIO_ID_CUSTOM1:
		ret = pwr_seq_reset_view_to_sensing(ctx);
		if (ret)
			LOG_INF("pwr_seq_reset_view_to_sensing (fail),ret(%d)\n", ret);
		else
			DEBUG_LOG(ctx, "pwr_seq_reset_view_to_sensing (correct)\n");

		/* initail sequence write in */
		sensor_init(ctx);
		custom1(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM2:
		ret = pwr_seq_reset_view_to_sensing(ctx);
		if (ret)
			LOG_INF("pwr_seq_reset_view_to_sensing (fail),ret(%d)\n", ret);
		else
			DEBUG_LOG(ctx, "pwr_seq_reset_view_to_sensing (correct)\n");

		/* initail sequence write in */
		sensor_init(ctx);
		custom2(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM3:
		ret = pwr_seq_reset_view_to_sensing(ctx);
		if (ret)
			LOG_INF("pwr_seq_reset_view_to_sensing (fail),ret(%d)\n", ret);
		else
			DEBUG_LOG(ctx, "pwr_seq_reset_view_to_sensing (correct)\n");

		/* initail sequence write in */
		sensor_init(ctx);
		custom3(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM4:
		ret = pwr_seq_reset_view_to_sensing(ctx);
		if (ret)
			LOG_INF("pwr_seq_reset_view_to_sensing (fail),ret(%d)\n", ret);
		else
			DEBUG_LOG(ctx, "pwr_seq_reset_view_to_sensing (correct)\n");

		/* initail sequence write in */
		sensor_init(ctx);
		custom4(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM5:
		ret = pwr_seq_reset_view_to_sensing(ctx);
		if (ret)
			LOG_INF("pwr_seq_reset_view_to_sensing (fail),ret(%d)\n", ret);
		else
			DEBUG_LOG(ctx, "pwr_seq_reset_view_to_sensing (correct)\n");

		/* initail sequence write in */
		sensor_init(ctx);
		custom5(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM6:
		ret = pwr_seq_reset_view_to_sensing(ctx);
		if (ret)
			LOG_INF("pwr_seq_reset_view_to_sensing (fail),ret(%d)\n", ret);
		else
			DEBUG_LOG(ctx, "pwr_seq_reset_view_to_sensing (correct)\n");

		/* initail sequence write in */
		sensor_init(ctx);
		custom6(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM7:
		ret = pwr_seq_reset_view_to_sensing(ctx);
		if (ret)
			LOG_INF("pwr_seq_reset_view_to_sensing (fail),ret(%d)\n", ret);
		else
			DEBUG_LOG(ctx, "pwr_seq_reset_view_to_sensing (correct)\n");

		/* initail sequence write in */
		sensor_init(ctx);
		custom7(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM8:
		ret = pwr_seq_reset_view_to_sensing(ctx);
		if (ret)
			LOG_INF("pwr_seq_reset_view_to_sensing (fail),ret(%d)\n", ret);
		else
			DEBUG_LOG(ctx, "pwr_seq_reset_view_to_sensing (correct)\n");

		/* initail sequence write in */
		sensor_init(ctx);
		custom8(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM9:
		ret = pwr_seq_reset_view_to_sensing(ctx);
		if (ret)
			LOG_INF("pwr_seq_reset_view_to_sensing (fail),ret(%d)\n", ret);
		else
			DEBUG_LOG(ctx, "pwr_seq_reset_view_to_sensing (correct)\n");

		/* initail sequence write in */
		sensor_init(ctx);
		custom9(ctx, image_window, sensor_config_data);
		break;
#endif
	case SENSOR_SCENARIO_ID_CUSTOM10:
		/* initail sequence write in */
		sensor_init(ctx);
		custom10(ctx, image_window, sensor_config_data);
		break;
	default:
		LOG_INF("Error ScenarioId setting\n");
		preview(ctx, image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}	/* control */

static kal_uint32 set_video_mode(struct subdrv_ctx *ctx, UINT16 framerate)
{
	DEBUG_LOG(ctx, "framerate(%d)\n ", framerate);
	/* SetVideoMode Function should fix framerate */
	if (framerate == 0)
		/* Dynamic frame rate */
		return ERROR_NONE;
	if ((framerate == 300) && (ctx->autoflicker_en == KAL_TRUE))
		ctx->current_fps = 296;
	else if ((framerate == 150) && (ctx->autoflicker_en == KAL_TRUE))
		ctx->current_fps = 146;
	else
		ctx->current_fps = framerate;
	set_max_framerate(ctx, ctx->current_fps, 1);
	set_dummy(ctx);

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(struct subdrv_ctx *ctx,
	kal_bool enable, UINT16 framerate)
{
	DEBUG_LOG(ctx, "enable(%d), framerate(%d)\n", enable, framerate);
	if (enable) /*enable auto flicker*/
		ctx->autoflicker_en = KAL_TRUE;
	else /*Cancel Auto flick*/
		ctx->autoflicker_en = KAL_FALSE;
	return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	DEBUG_LOG(ctx, "scenario_id(%d), framerate(%d)\n", scenario_id, framerate);

	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		frame_length = imgsensor_info.pre.pclk / framerate * 10
				/ imgsensor_info.pre.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.pre.framelength)
		? (frame_length - imgsensor_info.pre.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.pre.framelength + ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		if (ctx->current_fps != imgsensor_info.cap.max_framerate)
			DEBUG_LOG(ctx,
				"Warning: current_fps(%d) is not support, so use cap's setting:(%d)fps!\n",
				framerate, imgsensor_info.cap.max_framerate/10);
		frame_length = imgsensor_info.cap.pclk / framerate * 10
				/ imgsensor_info.cap.linelength;

		if (frame_length > imgsensor_info.max_frame_length) {
			DEBUG_LOG(ctx,
				"Warning: frame_length(%d) > max_frame_length(%d)!\n",
				frame_length, imgsensor_info.max_frame_length);
			break;
		}

		ctx->dummy_line =
			(frame_length > imgsensor_info.cap.framelength)
			? (frame_length - imgsensor_info.cap.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.cap.framelength + ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.normal_video.pclk /
				framerate * 10 /
				imgsensor_info.normal_video.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.normal_video.framelength)
		? (frame_length - imgsensor_info.normal_video.framelength)
		: 0;
		ctx->frame_length =
			imgsensor_info.normal_video.framelength + ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk / framerate * 10
				/ imgsensor_info.hs_video.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.hs_video.framelength)
			? (frame_length - imgsensor_info.hs_video.framelength)
			: 0;
		ctx->frame_length =
			imgsensor_info.hs_video.framelength + ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk / framerate * 10
			/ imgsensor_info.slim_video.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.slim_video.framelength)
			? (frame_length - imgsensor_info.slim_video.framelength)
			: 0;
		ctx->frame_length =
			imgsensor_info.slim_video.framelength + ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		set_dummy(ctx);
		break;
#ifdef AOV_MODE_SENSING
	case SENSOR_SCENARIO_ID_CUSTOM1:
		frame_length = imgsensor_info.custom1.pclk / framerate * 10
				/ imgsensor_info.custom1.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.custom1.framelength)
			? (frame_length - imgsensor_info.custom1.framelength)
			: 0;
		ctx->frame_length =
			imgsensor_info.custom1.framelength + ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM2:
		frame_length = imgsensor_info.custom2.pclk / framerate * 10
				/ imgsensor_info.custom2.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.custom2.framelength)
			? (frame_length - imgsensor_info.custom2.framelength)
			: 0;
		ctx->frame_length =
			imgsensor_info.custom2.framelength + ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM3:
		frame_length = imgsensor_info.custom3.pclk / framerate * 10
				/ imgsensor_info.custom3.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.custom3.framelength)
			? (frame_length - imgsensor_info.custom3.framelength)
			: 0;
		ctx->frame_length =
			imgsensor_info.custom3.framelength + ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM4:
		frame_length = imgsensor_info.custom4.pclk / framerate * 10
				/ imgsensor_info.custom4.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.custom4.framelength)
			? (frame_length - imgsensor_info.custom4.framelength)
			: 0;
		ctx->frame_length =
			imgsensor_info.custom4.framelength + ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		// set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM5:
		frame_length = imgsensor_info.custom5.pclk / framerate * 10
				/ imgsensor_info.custom5.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.custom5.framelength)
			? (frame_length - imgsensor_info.custom5.framelength)
			: 0;
		ctx->frame_length =
			imgsensor_info.custom5.framelength + ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		// set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM6:
		frame_length = imgsensor_info.custom6.pclk / framerate * 10
				/ imgsensor_info.custom6.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.custom6.framelength)
			? (frame_length - imgsensor_info.custom6.framelength)
			: 0;
		ctx->frame_length =
			imgsensor_info.custom6.framelength + ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		// set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM7:
		frame_length = imgsensor_info.custom7.pclk / framerate * 10
				/ imgsensor_info.custom7.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.custom7.framelength)
			? (frame_length - imgsensor_info.custom7.framelength)
			: 0;
		ctx->frame_length =
			imgsensor_info.custom7.framelength + ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		// set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM8:
		frame_length = imgsensor_info.custom8.pclk / framerate * 10
				/ imgsensor_info.custom8.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.custom8.framelength)
			? (frame_length - imgsensor_info.custom8.framelength)
			: 0;
		ctx->frame_length =
			imgsensor_info.custom8.framelength + ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		// set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM9:
		frame_length = imgsensor_info.custom9.pclk / framerate * 10
				/ imgsensor_info.custom9.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.custom9.framelength)
			? (frame_length - imgsensor_info.custom9.framelength)
			: 0;
		ctx->frame_length =
			imgsensor_info.custom9.framelength + ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		// set_dummy(ctx);
		break;
#endif
	case SENSOR_SCENARIO_ID_CUSTOM10:
		frame_length = imgsensor_info.custom10.pclk / framerate * 10
				/ imgsensor_info.custom10.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.custom10.framelength)
		? (frame_length - imgsensor_info.custom10.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.custom10.framelength + ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		set_dummy(ctx);
		break;
	default:  /*coding with  preview scenario by default*/
		frame_length = imgsensor_info.pre.pclk / framerate * 10
			/ imgsensor_info.pre.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.pre.framelength)
			? (frame_length - imgsensor_info.pre.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.pre.framelength + ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		set_dummy(ctx);
		LOG_INF("error scenario_id(%d), we use preview scenario\n",
			scenario_id);
		break;
	}
	return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	DEBUG_LOG(ctx, "scenario_id(%d)\n", scenario_id);

	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		*framerate = imgsensor_info.pre.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		*framerate = imgsensor_info.normal_video.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		*framerate = imgsensor_info.cap.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		*framerate = imgsensor_info.hs_video.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		*framerate = imgsensor_info.slim_video.max_framerate;
		break;
#ifdef AOV_MODE_SENSING
	case SENSOR_SCENARIO_ID_CUSTOM1:
		*framerate = imgsensor_info.custom1.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM2:
		*framerate = imgsensor_info.custom2.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM3:
		*framerate = imgsensor_info.custom3.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM4:
		*framerate = imgsensor_info.custom4.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM5:
		*framerate = imgsensor_info.custom5.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM6:
		*framerate = imgsensor_info.custom6.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM7:
		*framerate = imgsensor_info.custom7.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM8:
		*framerate = imgsensor_info.custom8.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM9:
		*framerate = imgsensor_info.custom9.max_framerate;
		break;
#endif
	case SENSOR_SCENARIO_ID_CUSTOM10:
		*framerate = imgsensor_info.custom10.max_framerate;
		break;
	default:
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 get_fine_integ_line_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, MUINT32 *fine_integ_line)
{
	DEBUG_LOG(ctx, "E!\n");

	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
	case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
	case SENSOR_SCENARIO_ID_SLIM_VIDEO:
	case SENSOR_SCENARIO_ID_CUSTOM1:
	case SENSOR_SCENARIO_ID_CUSTOM2:
	case SENSOR_SCENARIO_ID_CUSTOM3:
	case SENSOR_SCENARIO_ID_CUSTOM4:
	case SENSOR_SCENARIO_ID_CUSTOM5:
	case SENSOR_SCENARIO_ID_CUSTOM6:
	case SENSOR_SCENARIO_ID_CUSTOM7:
	case SENSOR_SCENARIO_ID_CUSTOM8:
	case SENSOR_SCENARIO_ID_CUSTOM9:
	case SENSOR_SCENARIO_ID_CUSTOM10:
	case SENSOR_SCENARIO_ID_CUSTOM11:
	case SENSOR_SCENARIO_ID_CUSTOM12:
	case SENSOR_SCENARIO_ID_CUSTOM13:
	case SENSOR_SCENARIO_ID_CUSTOM14:
	case SENSOR_SCENARIO_ID_CUSTOM15:
	case SENSOR_SCENARIO_ID_CUSTOM16:
		*fine_integ_line = fine_integ_line_table[scenario_id];
		break;
	default:
		break;
	}
	DEBUG_LOG(ctx,
		"scenario_id(%d), fine_integ_line(%u)\n",
		scenario_id, *fine_integ_line);
	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(struct subdrv_ctx *ctx, kal_uint32 mode)
{
	DEBUG_LOG(ctx, "E!\n");

	if (mode != ctx->test_pattern)
		DEBUG_LOG(ctx, "mode %d -> %d\n", ctx->test_pattern, mode);
	//1:Solid Color 2:Color bar 5:Black
	if (mode == 5)
		write_cmos_sensor_8(ctx, 0x020E, 0x00);//Dgain = 0
	else if (mode)
		write_cmos_sensor_8(ctx, 0x0601, mode);

	if ((ctx->test_pattern) && (mode != ctx->test_pattern)) {
		if (ctx->test_pattern == 5)
			write_cmos_sensor_8(ctx, 0x020E, 0x01);
		else if (mode == 0)
			write_cmos_sensor_8(ctx, 0x0601, 0x00); /*No pattern*/
	}
	ctx->test_pattern = mode;
	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_data(struct subdrv_ctx *ctx,
	struct mtk_test_pattern_data *data)
{
	DEBUG_LOG(ctx, "IMX709 Only could support black\n");
	/*
	 * set_cmos_sensor_8(ctx, 0x0602, (data->Channel_R >> 30) & 0x3);
	 * set_cmos_sensor_8(ctx, 0x0603, (data->Channel_R >> 22) & 0xff);
	 * set_cmos_sensor_8(ctx, 0x0604, (data->Channel_Gr >> 30) & 0x3);
	 * set_cmos_sensor_8(ctx, 0x0605, (data->Channel_Gr >> 22) & 0xff);
	 * set_cmos_sensor_8(ctx, 0x0606, (data->Channel_B >> 30) & 0x3);
	 * set_cmos_sensor_8(ctx, 0x0607, (data->Channel_B >> 22) & 0xff);
	 * set_cmos_sensor_8(ctx, 0x0608, (data->Channel_Gb >> 30) & 0x3);
	 * set_cmos_sensor_8(ctx, 0x0609, (data->Channel_Gb >> 22) & 0xff);
	 * commit_write_sensor(ctx);
	 */
	return ERROR_NONE;
}

static kal_int32 get_sensor_temperature(struct subdrv_ctx *ctx)
{
	UINT8 temperature = 0;
	INT32 temperature_convert = 0;

	DEBUG_LOG(ctx, "E!\n");

	temperature = read_cmos_sensor_8(ctx, 0x013a);

	if (temperature <= 0x60)
		temperature_convert = temperature;
	else if (temperature >= 0x61 && temperature <= 0x7F)
		temperature_convert = 97;
	else if (temperature >= 0x80 && temperature <= 0xE2)
		temperature_convert = -30;
	else
		temperature_convert = (INT8)temperature | 0xFFFFFF0;

	return temperature_convert;
}

static int feature_control(struct subdrv_ctx *ctx,
	MSDK_SENSOR_FEATURE_ENUM feature_id,
	UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *) feature_para;
	uint32_t *pAeCtrls;
	uint32_t *pScenarios;
	uint32_t ratio = 1;

	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;

	struct SENSOR_VC_INFO2_STRUCT *pvcinfo2;

	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data
		= (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	DEBUG_LOG(ctx, "feature_id(%u)\n", feature_id);

	switch (feature_id) {
	case SENSOR_FEATURE_GET_OUTPUT_FORMAT_BY_SCENARIO:
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_CUSTOM1:
		case SENSOR_SCENARIO_ID_CUSTOM2:
		case SENSOR_SCENARIO_ID_CUSTOM3:
		case SENSOR_SCENARIO_ID_CUSTOM4:
		case SENSOR_SCENARIO_ID_CUSTOM5:
		case SENSOR_SCENARIO_ID_CUSTOM6:
		case SENSOR_SCENARIO_ID_CUSTOM7:
		case SENSOR_SCENARIO_ID_CUSTOM8:
		case SENSOR_SCENARIO_ID_CUSTOM9:
			*(feature_data + 1)
			= (enum ACDK_SENSOR_OUTPUT_DATA_FORMAT_ENUM)
				SENSOR_OUTPUT_FORMAT_SENSING_MODE_RAW_MONO;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_CUSTOM10:
		case SENSOR_SCENARIO_ID_CUSTOM11:
		case SENSOR_SCENARIO_ID_CUSTOM12:
		case SENSOR_SCENARIO_ID_CUSTOM13:
		case SENSOR_SCENARIO_ID_CUSTOM14:
		case SENSOR_SCENARIO_ID_CUSTOM15:
		case SENSOR_SCENARIO_ID_CUSTOM16:
			*(feature_data + 1)
			= (enum ACDK_SENSOR_OUTPUT_DATA_FORMAT_ENUM)
				imgsensor_info.sensor_output_dataformat;
			break;
		}
		DEBUG_LOG(ctx,
			"GET_OUTPUT_FORMAT_BY_SCENARIO(%u), output_format(%u)\n",
			(*feature_data), (*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_GET_ANA_GAIN_TABLE:
		if ((void *)(uintptr_t) (*(feature_data + 1)) == NULL) {
			*(feature_data + 0) =
				sizeof(imx709_ana_gain_table);
		} else {
			memcpy((void *)(uintptr_t) (*(feature_data + 1)),
			(void *)imx709_ana_gain_table,
			sizeof(imx709_ana_gain_table));
		}
		break;
	case SENSOR_FEATURE_GET_GAIN_RANGE_BY_SCENARIO:
		*(feature_data + 1) = imgsensor_info.min_gain;
		*(feature_data + 2) = imgsensor_info.max_gain;
		DEBUG_LOG(ctx,
			"GET_GAIN_RANGE_BY_SCENARIO(%u), min_gain(%llu), max_gain(%llu)\n",
			(*feature_data), (*(feature_data + 1)), (*(feature_data + 2)));
		break;
	case SENSOR_FEATURE_GET_BASE_GAIN_ISO_AND_STEP:
		*(feature_data + 0) = imgsensor_info.min_gain_iso;
		*(feature_data + 1) = imgsensor_info.gain_step;
		*(feature_data + 2) = imgsensor_info.gain_type;
		DEBUG_LOG(ctx,
			"min_gain_iso(%llu), gain_step(%u), gain_type(%u)\n",
			(*feature_data + 0), (*(feature_data + 1)),
			(*(feature_data + 2)));
		break;
	case SENSOR_FEATURE_GET_MIN_SHUTTER_BY_SCENARIO:
		*(feature_data + 1) = imgsensor_info.min_shutter;
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_CUSTOM1:
		case SENSOR_SCENARIO_ID_CUSTOM2:
		case SENSOR_SCENARIO_ID_CUSTOM3:
		case SENSOR_SCENARIO_ID_CUSTOM4:
		case SENSOR_SCENARIO_ID_CUSTOM5:
		case SENSOR_SCENARIO_ID_CUSTOM6:
		case SENSOR_SCENARIO_ID_CUSTOM7:
		case SENSOR_SCENARIO_ID_CUSTOM8:
		case SENSOR_SCENARIO_ID_CUSTOM9:
		case SENSOR_SCENARIO_ID_CUSTOM10:
		case SENSOR_SCENARIO_ID_CUSTOM11:
		case SENSOR_SCENARIO_ID_CUSTOM12:
		case SENSOR_SCENARIO_ID_CUSTOM13:
		case SENSOR_SCENARIO_ID_CUSTOM14:
		case SENSOR_SCENARIO_ID_CUSTOM15:
		case SENSOR_SCENARIO_ID_CUSTOM16:
			*(feature_data + 2) = exposure_step_table[*feature_data];
			break;
		default:
			*(feature_data + 2) = 4;
			break;
		}
		DEBUG_LOG(ctx,
			"GET_GAIN_RANGE_BY_SCENARIO(%u), min_shutter(%llu), exposure_step_table(%llu)\n",
			(*feature_data), (*(feature_data + 1)), (*(feature_data + 2)));
		break;
	case SENSOR_FEATURE_GET_OFFSET_TO_START_OF_EXPOSURE:
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = -27000000;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.cap.pclk;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.normal_video.pclk;
			break;
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.hs_video.pclk;
			break;
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.slim_video.pclk;
			break;
#ifdef AOV_MODE_SENSING
		case SENSOR_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom1.pclk;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom2.pclk;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom3.pclk;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom4.pclk;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM5:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom5.pclk;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM6:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom6.pclk;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM7:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom7.pclk;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM8:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom8.pclk;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM9:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom9.pclk;
			break;
#endif
		case SENSOR_SCENARIO_ID_CUSTOM10:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom10.pclk;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.pre.pclk;
			break;
		}
		DEBUG_LOG(ctx,
			"GET_PIXEL_CLOCK_FREQ_BY_SCENARIO(%u), pclk(%llu)\n",
			(*feature_data), (*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
		if (*(feature_data + 2) & SENSOR_GET_LINELENGTH_FOR_READOUT)
			ratio = get_exp_cnt_by_scenario(ctx, (*feature_data));
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.cap.framelength << 16)
				+ (ratio * imgsensor_info.cap.linelength);
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.normal_video.framelength << 16)
				+ (ratio * imgsensor_info.normal_video.linelength);
			break;
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.hs_video.framelength << 16)
				+ (ratio * imgsensor_info.hs_video.linelength);
			break;
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.slim_video.framelength << 16)
				+ (ratio * imgsensor_info.slim_video.linelength);
			break;
#ifdef AOV_MODE_SENSING
		case SENSOR_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom1.framelength << 16)
				+ (ratio * imgsensor_info.custom1.linelength);
			break;
		case SENSOR_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom2.framelength << 16)
				+ (ratio * imgsensor_info.custom2.linelength);
			break;
		case SENSOR_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom3.framelength << 16)
				+ (ratio * imgsensor_info.custom3.linelength);
			break;
		case SENSOR_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom4.framelength << 16)
				+ (ratio * imgsensor_info.custom4.linelength);
			break;
		case SENSOR_SCENARIO_ID_CUSTOM5:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom5.framelength << 16)
				+ (ratio * imgsensor_info.custom5.linelength);
			break;
		case SENSOR_SCENARIO_ID_CUSTOM6:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom6.framelength << 16)
				+ (ratio * imgsensor_info.custom6.linelength);
			break;
		case SENSOR_SCENARIO_ID_CUSTOM7:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom7.framelength << 16)
				+ (ratio * imgsensor_info.custom7.linelength);
			break;
		case SENSOR_SCENARIO_ID_CUSTOM8:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom8.framelength << 16)
				+ (ratio * imgsensor_info.custom8.linelength);
			break;
		case SENSOR_SCENARIO_ID_CUSTOM9:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom9.framelength << 16)
				+ (ratio * imgsensor_info.custom9.linelength);
			break;
#endif
		case SENSOR_SCENARIO_ID_CUSTOM10:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom10.framelength << 16)
				+ (ratio * imgsensor_info.custom10.linelength);
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.pre.framelength << 16)
				+ imgsensor_info.pre.linelength;
			break;
		}
		DEBUG_LOG(ctx,
			"GET_PERIOD_BY_SCENARIO(%u), framelength(%llu), linelength(%llu)\n",
			(*feature_data), (*(feature_data + 1) >> 16),
			(*(feature_data + 1) & 0xffff));
		break;
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = ctx->line_length;
		*feature_return_para_16 = ctx->frame_length;
		*feature_para_len = 4;
		DEBUG_LOG(ctx,
			"GET_PERIOD,frame_length(%d), line_length(%d)\n",
			(*feature_return_para_16), (*(feature_return_para_16 + 1)));
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		*feature_return_para_32 = ctx->pclk;
		*feature_para_len = 4;
		DEBUG_LOG(ctx,
			"GET_PIXEL_CLOCK_FREQ, pclk(%u)\n",
			(*feature_return_para_32));
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
#ifdef AOV_MODE_SENSING
		if (ctx->sensor_mode >= IMGSENSOR_MODE_CUSTOM1 &&
			ctx->sensor_mode < IMGSENSOR_MODE_CUSTOM10)
			LOG_INF("AOV mode not support ae shutter control!\n");
		else
#endif
			set_shutter(ctx, *feature_data);
		DEBUG_LOG(ctx, "SET_ESHUTTER, shutter(%llu)\n", (*feature_data));
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain(ctx, (UINT32) *(feature_data));
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		write_cmos_sensor_8(ctx, sensor_reg_data->RegAddr,
			sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData =
			read_cmos_sensor_8(ctx, sensor_reg_data->RegAddr);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		/* get the lens driver ID from EEPROM
		 * or just return LENS_DRIVER_ID_DO_NOT_CARE
		 * if EEPROM does not exist in camera module.
		 */
		*feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		set_video_mode(ctx, *feature_data);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		get_imgsensor_id(ctx, feature_return_para_32);
		break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		set_auto_flicker_mode(ctx, (BOOL)*feature_data_16,
			*(feature_data_16+1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario(ctx,
			(enum SENSOR_SCENARIO_ID_ENUM)*feature_data, *(feature_data+1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario(ctx,
			(enum SENSOR_SCENARIO_ID_ENUM)*(feature_data),
			(MUINT32 *)(uintptr_t)(*(feature_data+1)));
		break;
	case SENSOR_FEATURE_GET_FINE_INTEG_LINE_BY_SCENARIO:
		get_fine_integ_line_by_scenario(ctx,
			(enum SENSOR_SCENARIO_ID_ENUM)*(feature_data),
			(MUINT32 *)(uintptr_t)(*(feature_data+1)));
		break;
	case SENSOR_FEATURE_GET_PDAF_DATA:
		DEBUG_LOG(ctx, "SENSOR_FEATURE_GET_PDAF_DATA\n");
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode(ctx, (UINT32)*feature_data);
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN_DATA:
		set_test_pattern_data(ctx,
			(struct mtk_test_pattern_data *)feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		/* for factory mode auto testing */
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		DEBUG_LOG(ctx,
			"SET_FRAMERATE, current fps(%u)\n",
			(UINT32) (*feature_data_32));
		ctx->current_fps = *feature_data_32;
		break;
	case SENSOR_FEATURE_SET_HDR:
		DEBUG_LOG(ctx, "ihdr enable(%d)\n", (BOOL)*feature_data_32);
		ctx->ihdr_mode = *feature_data_32;
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		DEBUG_LOG(ctx,
			"SENSOR_FEATURE_GET_CROP_INFO scenarioId(%d)\n",
			(UINT32) (*feature_data));
		wininfo =
			(struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));

		switch (*feature_data_32) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[1],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[2],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[3],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[4],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
#ifdef AOV_MODE_SENSING
		case SENSOR_SCENARIO_ID_CUSTOM1:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[5],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM2:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[6],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM3:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[7],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM4:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[8],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM5:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[9],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM6:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[10],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM7:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[11],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM8:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[12],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM9:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[13],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
#endif
		case SENSOR_SCENARIO_ID_CUSTOM10:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[14],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		default:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[0],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PDAF_INFO:
		DEBUG_LOG(ctx,
			"SENSOR_FEATURE_GET_PDAF_INFO scenarioId(%d)\n",
			(UINT16) *feature_data);
		PDAFinfo = (struct SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data+1));
		break;
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		DEBUG_LOG(ctx,
			"SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId(%d)\n",
			(UINT16) *feature_data);
		/* PDAF capacity enable or not, 2p8 only full size support PDAF */
		switch (*feature_data) {
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		}
		break;
	case SENSOR_FEATURE_SET_SEAMLESS_EXTEND_FRAME_LENGTH:
		DEBUG_LOG(ctx, "extend_frame_len (%llu)\n", *feature_data);
		extend_frame_length(ctx, (MUINT32) *feature_data);
		DEBUG_LOG(ctx, "extend_frame_len done (%llu)\n", *feature_data);
		break;
	case SENSOR_FEATURE_SEAMLESS_SWITCH:
	{
		DEBUG_LOG(ctx, "SENSOR_FEATURE_SEAMLESS_SWITCH\n");
		if ((feature_data + 1) != NULL)
			pAeCtrls = (MUINT32 *)((uintptr_t)(*(feature_data + 1)));
		else
			LOG_INF("warning! no ae_ctrl input\n");

		if (feature_data == NULL) {
			LOG_INF("error! input scenario is null!\n");
			return ERROR_INVALID_SCENARIO_ID;
		}
		DEBUG_LOG(ctx, "call seamless_switch\n");
		seamless_switch(ctx, (*feature_data), pAeCtrls);
	}
		break;
	case SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS:
		if ((feature_data + 1) != NULL)
			pScenarios = (MUINT32 *)((uintptr_t)(*(feature_data + 1)));
		else {
			LOG_INF("input pScenarios vector is NULL!\n");
			return ERROR_INVALID_SCENARIO_ID;
		}
		switch (*feature_data) {
		default:
			*pScenarios = 0xff;
			break;
		}
		DEBUG_LOG(ctx,
			"SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS (%llu) (%u)\n",
			*feature_data, *pScenarios);
		break;
	case SENSOR_FEATURE_GET_SENSOR_HDR_CAPACITY:
		/*
		 * HDR_NONE = 0,
		 * HDR_RAW = 1,
		 * HDR_CAMSV = 2,
		 * HDR_RAW_ZHDR = 9,
		 * HDR_MultiCAMSV = 10,
		 * HDR_RAW_STAGGER_2EXP = 0xB,
		 * HDR_RAW_STAGGER_MIN = HDR_RAW_STAGGER_2EXP,
		 * HDR_RAW_STAGGER_3EXP = 0xC,
		 * HDR_RAW_STAGGER_MAX = HDR_RAW_STAGGER_3EXP,
		 */
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		case SENSOR_SCENARIO_ID_CUSTOM10:
		default:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0x0;
			break;
		}
		DEBUG_LOG(ctx,
			"SENSOR_FEATURE_GET_SENSOR_HDR_CAPACITY, scenarioId(%llu), HDR(%llu)\n",
			*feature_data, *(MUINT32 *) (uintptr_t) (*(feature_data + 1)));
		break;
		/* END OF HDR CMD */
	case SENSOR_FEATURE_GET_VC_INFO2:
		DEBUG_LOG(ctx,
			"SENSOR_FEATURE_GET_VC_INFO2 (%u)\n",
			(UINT16) (*feature_data));
		pvcinfo2 =
			(struct SENSOR_VC_INFO2_STRUCT *) (uintptr_t) (*(feature_data + 1));
		get_vc_info_2(pvcinfo2, *feature_data_32);
		break;
	case SENSOR_FEATURE_GET_STAGGER_TARGET_SCENARIO:
		*(feature_data + 2) = 0xff;
		DEBUG_LOG(ctx,
			"SENSOR_FEATURE_GET_STAGGER_TARGET_SCENARIO (%u) (%u) (%u)\n",
			(UINT16) (*feature_data),
			(UINT16) (*(feature_data + 1)),
			(UINT16) (*(feature_data + 2)));
		break;
	case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
		*(feature_data + 1) = 1;	// always 1
		/* margin info by scenario */
		switch (*feature_data) {
		default:
			*(feature_data + 2) = imgsensor_info.margin;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_MAX_EXP_LINE:
	case SENSOR_FEATURE_GET_STAGGER_MAX_EXP_TIME:
		*(feature_data + 2) = 65533 - imgsensor_info.margin;
		break;
	case SENSOR_FEATURE_SET_HDR_SHUTTER:	// for 2EXP
		DEBUG_LOG(ctx,
			"SENSOR_FEATURE_SET_HDR_SHUTTER, LE(%u), SE(%u)\n",
			(UINT32) (*feature_data),
			(UINT32) (*(feature_data + 1)));
#ifdef AOV_MODE_SENSING
		if (ctx->sensor_mode >= IMGSENSOR_MODE_CUSTOM1 &&
			ctx->sensor_mode < IMGSENSOR_MODE_CUSTOM10)
			LOG_INF("AOV mode not support ae shutter control!\n");
		else
			// implement write shutter for NE/SE
#endif
			hdr_write_tri_shutter(ctx, (UINT32)*feature_data,
				0, (UINT32) (*(feature_data+1)));
		break;
	case SENSOR_FEATURE_SET_DUAL_GAIN:	// for 2EXP
		DEBUG_LOG(ctx,
			"SENSOR_FEATURE_SET_DUAL_GAIN, LE(%u), SE(%u)\n",
			(UINT32) (*feature_data),
			(UINT32) (*(feature_data + 1)));
#ifdef AOV_MODE_SENSING
		if (ctx->sensor_mode >= IMGSENSOR_MODE_CUSTOM1 &&
			ctx->sensor_mode < IMGSENSOR_MODE_CUSTOM10)
			LOG_INF("AOV mode not support ae shutter control!\n");
		else
			// implement write gain for NE/SE
#endif
			hdr_write_tri_gain(ctx, (UINT32)*feature_data,
				0, (UINT32) (*(feature_data+1)));
		break;
	case SENSOR_FEATURE_SET_HDR_TRI_SHUTTER:	// for 3EXP
		DEBUG_LOG(ctx,
			"SENSOR_FEATURE_SET_HDR_TRI_SHUTTER, LE(%u), ME(%u), SE(%u)\n",
			(UINT32) (*feature_data),
			(UINT32) (*(feature_data + 1)),
			(UINT32) (*(feature_data + 2)));
#ifdef AOV_MODE_SENSING
		if (ctx->sensor_mode >= IMGSENSOR_MODE_CUSTOM1 &&
			ctx->sensor_mode < IMGSENSOR_MODE_CUSTOM10)
			LOG_INF("AOV mode not support ae shutter control!\n");
		else
#endif
			hdr_write_tri_shutter(ctx,
					(UINT32) (*feature_data),
					(UINT32) (*(feature_data + 1)),
					(UINT32) (*(feature_data + 2)));
		break;
	case SENSOR_FEATURE_SET_HDR_TRI_GAIN:	// for 3EXP
		DEBUG_LOG(ctx,
			"SENSOR_FEATURE_SET_HDR_TRI_GAIN, LG(%u), SG(%u), MG(%u)\n",
			(UINT32) (*feature_data),
			(UINT32) (*(feature_data + 1)),
			(UINT32) (*(feature_data + 2)));
#ifdef AOV_MODE_SENSING
		if (ctx->sensor_mode >= IMGSENSOR_MODE_CUSTOM1 &&
			ctx->sensor_mode < IMGSENSOR_MODE_CUSTOM10)
			LOG_INF("AOV mode not support ae gain control!\n");
		else
#endif
			hdr_write_tri_gain(ctx,
					(UINT32) (*feature_data),
					(UINT32) (*(feature_data + 1)),
					(UINT32) (*(feature_data + 2)));
		break;
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		*feature_return_para_32 = get_sensor_temperature(ctx);
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PDAF_REG_SETTING:
		DEBUG_LOG(ctx,
			"SENSOR_FEATURE_GET_PDAF_REG_SETTING (%u)",
			(*feature_para_len));
		imx709_get_pdaf_reg_setting(ctx,
				(*feature_para_len) / sizeof(UINT32),
				feature_data_16);
		break;
	case SENSOR_FEATURE_SET_PDAF_REG_SETTING:
		imx709_set_pdaf_reg_setting(ctx,
				(*feature_para_len) / sizeof(UINT32),
				feature_data_16);
		break;
	case SENSOR_FEATURE_SET_PDAF:
		DEBUG_LOG(ctx, "PDAF mode(%u)\n", *feature_data_16);
		ctx->pdaf_mode = *feature_data_16;
		break;
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length(ctx,
			(UINT16) (*feature_data),
			(UINT16) (*(feature_data + 1)),
			(BOOL) (*(feature_data + 2)));
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		break;
	case SENSOR_FEATURE_SET_AWB_GAIN:
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		LOG_INF("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(ctx, KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		LOG_INF("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter(%llu)\n",
			*feature_data);
		if (*feature_data != 0) {
#ifdef AOV_MODE_SENSING
			if (ctx->sensor_mode >= IMGSENSOR_MODE_CUSTOM1 &&
				ctx->sensor_mode < IMGSENSOR_MODE_CUSTOM10)
				LOG_INF("AOV mode not support ae shutter control!\n");
			else
#endif
				set_shutter(ctx, *feature_data);
		}
		streaming_control(ctx, KAL_TRUE);
		break;
	case SENSOR_FEATURE_GET_BINNING_TYPE:
		switch (*(feature_data + 1)) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_CUSTOM10:
		default:
			*feature_return_para_32 = 1;	/* BINNING_AVERAGED */
			break;
		}
		DEBUG_LOG(ctx,
			"SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type(%u)\n",
			*feature_return_para_32);
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_AE_FRAME_MODE_FOR_LE:
		memcpy(feature_return_para_32,
		&ctx->ae_frm_mode, sizeof(struct IMGSENSOR_AE_FRM_MODE));
		break;
	case SENSOR_FEATURE_GET_AE_EFFECTIVE_FRAME_FOR_LE:
		*feature_return_para_32 = ctx->current_ae_effective_frame;
		break;
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
	{
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.cap.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.normal_video.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.hs_video.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.slim_video.mipi_pixel_rate;
			break;
#ifdef AOV_MODE_SENSING
		case SENSOR_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom1.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom2.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom3.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom4.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM5:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom5.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM6:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom6.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM7:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom7.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM8:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom8.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM9:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom9.mipi_pixel_rate;
			break;
#endif
		case SENSOR_SCENARIO_ID_CUSTOM10:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom10.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
		DEBUG_LOG(ctx,
			"X! GET_MIPI_PIXEL_RATE mode(%u),mipi_pixel_rate(%u)\n",
			(*feature_data), (*(feature_data + 1)));
	}
		break;
	case SENSOR_FEATURE_PRELOAD_EEPROM_DATA:
		/* get eeprom preloader data */
		*feature_return_para_32 = ctx->is_read_preload_eeprom;
		*feature_para_len = 4;
#if EEPROM_READY
		if (ctx->is_read_preload_eeprom != 1)
			read_sensor_Cali(ctx);
#endif
		break;
	case SENSOR_FEATURE_SET_FRAMELENGTH:
		set_frame_length(ctx, (UINT16) (*feature_data));
		break;
	case SENSOR_FEATURE_SET_MULTI_SHUTTER_FRAME_TIME:
		set_multi_shutter_frame_length(ctx, (UINT32 *)(*feature_data),
					(UINT16) (*(feature_data + 1)),
					(UINT16) (*(feature_data + 2)));
		break;
	case SENSOR_FEATURE_SET_AOV_CSI_CLK:
		switch (*feature_return_para_32) {
		case 130:
			ctx->aov_csi_clk = 130;
			break;
		case 242:
		default:
			ctx->aov_csi_clk = 242;
			break;
		}
		DEBUG_LOG(ctx,
			"[%s] SENSOR_FEATURE_SET_AOV_CSI_CLK(%u)\n",
			__func__, *feature_return_para_32);
		break;
	default:
		break;
	}
	return ERROR_NONE;
}	/* feature_control(ctx) */

#ifdef IMGSENSOR_VC_ROUTING
static struct mtk_mbus_frame_desc_entry frame_desc_prev[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 3264,
			.vsize = 2448,
			.user_data_desc = VC_STAGGER_NE,
		},
	}
};
static struct mtk_mbus_frame_desc_entry frame_desc_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 3264,
			.vsize = 1856,
			.user_data_desc = VC_STAGGER_NE,
		},
	}
};
#ifdef AOV_MODE_SENSING
static struct mtk_mbus_frame_desc_entry frame_desc_cus1[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2a,
			.hsize = 640,
			.vsize = 480,
			.user_data_desc = VC_STAGGER_NE,
		},
	}
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus2[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2a,
			.hsize = 480,
			.vsize = 320,
			.user_data_desc = VC_STAGGER_NE,
		},
	}
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus3[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2a,
			.hsize = 320,
			.vsize = 240,
			.user_data_desc = VC_STAGGER_NE,
		},
	}
};
#endif
static struct mtk_mbus_frame_desc_entry frame_desc_cus10[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 640,
			.vsize = 480,
			.user_data_desc = VC_STAGGER_NE,
		},
	}
};
static int get_frame_desc(struct subdrv_ctx *ctx,
		int scenario_id, struct mtk_mbus_frame_desc *fd)
{
	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_prev);
		memcpy(fd->entry, frame_desc_prev, sizeof(frame_desc_prev));
		break;
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
	case SENSOR_SCENARIO_ID_SLIM_VIDEO:
	case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_vid);
		memcpy(fd->entry, frame_desc_vid, sizeof(frame_desc_vid));
		break;
#ifdef AOV_MODE_SENSING
	case SENSOR_SCENARIO_ID_CUSTOM1:
	case SENSOR_SCENARIO_ID_CUSTOM4:
	case SENSOR_SCENARIO_ID_CUSTOM5:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_cus1);
		memcpy(fd->entry, frame_desc_cus1, sizeof(frame_desc_cus1));
		break;
	case SENSOR_SCENARIO_ID_CUSTOM2:
	case SENSOR_SCENARIO_ID_CUSTOM6:
	case SENSOR_SCENARIO_ID_CUSTOM7:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_cus2);
		memcpy(fd->entry, frame_desc_cus2, sizeof(frame_desc_cus2));
		break;
	case SENSOR_SCENARIO_ID_CUSTOM3:
	case SENSOR_SCENARIO_ID_CUSTOM8:
	case SENSOR_SCENARIO_ID_CUSTOM9:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_cus3);
		memcpy(fd->entry, frame_desc_cus3, sizeof(frame_desc_cus3));
		break;
#endif
	case SENSOR_SCENARIO_ID_CUSTOM10:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_cus10);
		memcpy(fd->entry, frame_desc_cus10, sizeof(frame_desc_cus10));
		break;
	default:
		return -1;
	}

	return 0;
}
#endif

static const struct subdrv_ctx defctx = {
	.ana_gain_def = BASEGAIN * 4,
	.ana_gain_max = BASEGAIN * 32,
	.ana_gain_min = BASEGAIN * 1,
	.ana_gain_step = 1,
	.exposure_def = 0x3D0,
	/* exposure reg is limited to 4x. max = max - margin */
	.exposure_max = 0xffff - 24,
	.exposure_min = 8,
	.exposure_step = 1,
	//.frame_time_delay_frame = None,
	.is_hflip = 1,
	.is_vflip = 1,
	.margin = 24,	/* exp margin */
	.max_frame_length = 0xFFFF,

	.mirror = IMAGE_HV_MIRROR,	/* mirror flip information */
	.sensor_mode = IMGSENSOR_MODE_INIT,
	/* IMGSENSOR_MODE enum value,record current sensor mode,such as:
	 * INIT, Preview, Capture, Video,High Speed Video, Slim Video
	 */
	.shutter = 0x3D0,	/* current shutter */
	.gain = 0x100,	/* current gain */
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */
	.current_fps = 450,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW,
	.ihdr_mode = 0,	/* sensor need support LE, SE with HDR feature */
	.i2c_write_id = 0x20,	/* record current sensor's i2c write id */
	.readout_length = 0,
	.read_margin = 10,
	.current_ae_effective_frame = 2,
	.extend_frame_length_en = KAL_FALSE,
	.ae_ctrl_gph_en = KAL_FALSE,
	.aov_csi_clk = 242,
	.sensor_mode_ops = 0,
	.sensor_debug_sensing_ut_on_scp = true,
	.sensor_debug_dphy_global_timing_continuous_clk = false,
};

static int init_ctx(struct subdrv_ctx *ctx,
		struct i2c_client *i2c_client, u8 i2c_write_id)
{
	memcpy(ctx, &defctx, sizeof(*ctx));
	ctx->i2c_client = i2c_client;
	ctx->i2c_write_id = i2c_write_id;

	return 0;
}

static int get_temp(struct subdrv_ctx *ctx, int *temp)
{
	*temp = get_sensor_temperature(ctx) * 1000;
	return 0;
}

static int get_csi_param(struct subdrv_ctx *ctx,
	enum SENSOR_SCENARIO_ID_ENUM scenario_id,
	struct mtk_csi_param *csi_param)
{
	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		csi_param->legacy_phy = 0;
		csi_param->not_fixed_trail_settle = 1;
		csi_param->not_fixed_dphy_settle = 1;
		csi_param->dphy_data_settle = 0x13;
		csi_param->dphy_clk_settle = 0x13;
		csi_param->dphy_trail = 0x34;
		csi_param->dphy_csi2_resync_dmy_cycle = 0xF;
		break;
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		csi_param->legacy_phy = 0;
		csi_param->not_fixed_trail_settle = 1;
		csi_param->not_fixed_dphy_settle = 1;
		csi_param->dphy_data_settle = 0x12;
		csi_param->dphy_clk_settle = 0x12;
		csi_param->dphy_trail = 0x31;
		csi_param->dphy_csi2_resync_dmy_cycle = 0xF;
		break;
	case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
	case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		csi_param->legacy_phy = 0;
		csi_param->not_fixed_trail_settle = 1;
		csi_param->not_fixed_dphy_settle = 1;
		csi_param->dphy_data_settle = 0x13;
		csi_param->dphy_clk_settle = 0x13;
		csi_param->dphy_trail = 0x35;
		csi_param->dphy_csi2_resync_dmy_cycle = 0xF;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM1:
		csi_param->legacy_phy = 0;
		csi_param->not_fixed_trail_settle = 1;
		csi_param->not_fixed_dphy_settle = 1;
		switch (ctx->aov_csi_clk) {
		case 242:
			csi_param->dphy_data_settle = 0x13;
			csi_param->dphy_clk_settle = 0x13;
			csi_param->dphy_trail = 0x82;
			csi_param->dphy_csi2_resync_dmy_cycle = 0x26;
			break;
		case 130:
			csi_param->dphy_data_settle = 0xB;
			csi_param->dphy_clk_settle = 0xB;
			csi_param->dphy_trail = 0x46;
			csi_param->dphy_csi2_resync_dmy_cycle = 0x15;
			break;
		}
		break;
	case SENSOR_SCENARIO_ID_CUSTOM2:
		csi_param->legacy_phy = 0;
		csi_param->not_fixed_trail_settle = 1;
		csi_param->not_fixed_dphy_settle = 1;
		switch (ctx->aov_csi_clk) {
		case 242:
			csi_param->dphy_data_settle = 0x12;
			csi_param->dphy_clk_settle = 0x12;
			csi_param->dphy_trail = 0x75;
			csi_param->dphy_csi2_resync_dmy_cycle = 0x23;
			break;
		case 130:
			csi_param->dphy_data_settle = 0xA;
			csi_param->dphy_clk_settle = 0xA;
			csi_param->dphy_trail = 0x3F;
			csi_param->dphy_csi2_resync_dmy_cycle = 0x13;
			break;
		}
		break;
	case SENSOR_SCENARIO_ID_CUSTOM3:
		csi_param->legacy_phy = 0;
		csi_param->not_fixed_trail_settle = 1;
		csi_param->not_fixed_dphy_settle = 1;
		switch (ctx->aov_csi_clk) {
		case 242:
			csi_param->dphy_data_settle = 0x13;
			csi_param->dphy_clk_settle = 0x13;
			csi_param->dphy_trail = 0x7F;
			csi_param->dphy_csi2_resync_dmy_cycle = 0x25;
			break;
		case 130:
			csi_param->dphy_data_settle = 0xA;
			csi_param->dphy_clk_settle = 0xA;
			csi_param->dphy_trail = 0x44;
			csi_param->dphy_csi2_resync_dmy_cycle = 0x14;
			break;
		}
		break;
	case SENSOR_SCENARIO_ID_CUSTOM4:
	case SENSOR_SCENARIO_ID_CUSTOM5:
	case SENSOR_SCENARIO_ID_CUSTOM6:
	case SENSOR_SCENARIO_ID_CUSTOM7:
	case SENSOR_SCENARIO_ID_CUSTOM8:
	case SENSOR_SCENARIO_ID_CUSTOM9:
	case SENSOR_SCENARIO_ID_CUSTOM10:
		csi_param->legacy_phy = 0;
		csi_param->not_fixed_trail_settle = 1;
		csi_param->not_fixed_dphy_settle = 1;
		switch (ctx->aov_csi_clk) {
		case 242:
			csi_param->dphy_data_settle = 0x14;
			csi_param->dphy_clk_settle = 0x14;
			csi_param->dphy_trail = 0xB3;
			csi_param->dphy_csi2_resync_dmy_cycle = 0x34;
			break;
		case 130:
			csi_param->dphy_data_settle = 0xB;
			csi_param->dphy_clk_settle = 0xB;
			csi_param->dphy_trail = 0x60;
			csi_param->dphy_csi2_resync_dmy_cycle = 0x1C;
			break;
		}
		break;
	default:
		csi_param->legacy_phy = 0;
		csi_param->not_fixed_trail_settle = 0;
		csi_param->not_fixed_dphy_settle = 0;
		break;
	}
	DEBUG_LOG(ctx,
		"[%s] aov_csi_clk[%u] %d|%d|%d|%d|%d|%d|%d|%d\n",
		__func__,
		ctx->aov_csi_clk,
		csi_param->cphy_settle,
		csi_param->dphy_clk_settle,
		csi_param->dphy_data_settle,
		csi_param->dphy_trail,
		csi_param->not_fixed_trail_settle,
		csi_param->legacy_phy,
		csi_param->dphy_csi2_resync_dmy_cycle,
		csi_param->not_fixed_dphy_settle);
	return 0;
}

static struct subdrv_ops ops = {
	.get_id = get_imgsensor_id,
	.init_ctx = init_ctx,
	.open = open,
	.get_info = get_info,
	.get_resolution = get_resolution,
	.control = control,
	.feature_control = feature_control,
	.close = close,
#ifdef IMGSENSOR_VC_ROUTING
	.get_frame_desc = get_frame_desc,
#endif
	.get_temp = get_temp,
	.get_csi_param = get_csi_param,
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_SCL, 0, 0},	/* default i2c bus scl 4 on apmcu side */
	{HW_ID_SDA, 0, 0},	/* default i2c bus sda 4 on apmcu side */
	{HW_ID_MCLK1, 26, 0},
	// {HW_ID_PDN, 0, 0},
	{HW_ID_PONV, 0, 1},
	{HW_ID_RST1, 0, 1},
	{HW_ID_AVDD2, 1800000, 3},
	{HW_ID_DVDD1, 855000, 4},
	{HW_ID_DOVDD, 1800000, 1},
	{HW_ID_MCLK1_DRIVING_CURRENT, 6, 6},
	// {HW_ID_PDN, 1, 1},
	{HW_ID_PONV, 1, 1},
	{HW_ID_RST1, 1, 4}
};

const struct subdrv_entry imx709_mipi_raw_entry = {
	.name = "imx709_mipi_raw",
	.id = IMX709_SENSOR_ID,
	.pw_seq = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.ops = &ops,
};
