// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

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
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "s5k3m5sxmipiraw_Sensor.h"


#undef VENDOR_EDIT

/***************Modify Following Strings for Debug**********************/
#define PFX "S5K3M5SX_camera_sensor"
/****************************   Modify end	**************************/
#define LOG_INF(format, args...) pr_debug(PFX "[%s] " format, __func__, ##args)

#define MULTI_WRITE_REGISTER_VALUE  (16)

#if MULTI_WRITE_REGISTER_VALUE == 8
	#define I2C_BUFFER_LEN 765 /* trans# max is 255, each 3 bytes */
#elif MULTI_WRITE_REGISTER_VALUE == 16
	#define I2C_BUFFER_LEN 1020 /* trans# max is 255, each 4 bytes */
#endif


#ifdef VENDOR_EDIT
#define MODULE_ID_OFFSET 0x0000
#endif


static DEFINE_SPINLOCK(imgsensor_drv_lock);


static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = S5K3M5SX_SENSOR_ID,
	.checksum_value = 0x30a07776,
	.pre = {
		.pclk = 482000000,
		.linelength = 8816,
		.framelength = 1816,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2104,
		.grabwindow_height = 1560,
		.mipi_pixel_rate = 316800000,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
	},
	.cap = {
		.pclk = 482000000,
		.linelength = 4848,
		.framelength = 3314,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4208,
		.grabwindow_height = 3120,
		.mipi_pixel_rate = 576000000,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
	},
	.normal_video = {
		.pclk = 482000000,
		.linelength = 8816,
		.framelength = 1816,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2104,
		.grabwindow_height = 1560,
		.mipi_pixel_rate = 316800000,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
	},
	.hs_video = {
		.pclk = 482000000,
		.linelength = 8816,
		.framelength = 1816,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2104,
		.grabwindow_height = 1560,
		.mipi_pixel_rate = 316800000,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
	},
	.slim_video = {
		.pclk = 482000000,
		.linelength = 8816,
		.framelength = 1816,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2104,
		.grabwindow_height = 1560,
		.mipi_pixel_rate = 316800000,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
	},
	.custom1 = {
		.pclk = 482000000,
		.linelength = 5904,
		.framelength = 3400,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4208,
		.grabwindow_height = 3120,
		.mipi_pixel_rate = 473600000,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 240,
	},
	.custom2 = {
		.pclk = 482000000,
		.linelength = 4848,
		.framelength = 1656,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2104,
		.grabwindow_height = 1560,
		.mipi_pixel_rate = 576000000,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 600,
	},
	.custom3 = {
		.pclk = 482000000,
		.linelength = 8816,
		.framelength = 1816,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2104,
		.grabwindow_height = 1560,
		.mipi_pixel_rate = 316800000,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
	},
	.custom4 = {
		.pclk = 482000000,
		.linelength = 8816,
		.framelength = 1816,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2104,
		.grabwindow_height = 1560,
		.mipi_pixel_rate = 316800000,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
	},
	.custom5 = {
		.pclk = 482000000,
		.linelength = 8816,
		.framelength = 1816,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2104,
		.grabwindow_height = 1560,
		.mipi_pixel_rate = 316800000,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
	},
	.margin = 4,
	.min_shutter = 4,
	.min_gain = 64,
	.max_gain = 1024,
	.min_gain_iso = 100,
	.gain_step = 1,
	.gain_type = 2,
	.max_frame_length = 0xffff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,
	.ihdr_le_firstline = 0,
	.sensor_mode_num = 5,
	.cap_delay_frame = 2,
	.pre_delay_frame = 2,
	.video_delay_frame = 2,
	.hs_video_delay_frame = 2,
	.slim_video_delay_frame = 2,
	.custom1_delay_frame = 2,
	.custom2_delay_frame = 2,
	.custom3_delay_frame = 2,
	.custom4_delay_frame = 2,
	.custom5_delay_frame = 2,
	.isp_driving_current = ISP_DRIVING_4MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = 0,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_addr_table = {0x20, 0x5a, 0xff},
	.i2c_speed = 1000,
};

static kal_uint16 sensor_init_setting_array[] = {
	0x6028, 0x2000,
	0x602A, 0x3EAC,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0549,
	0x6F12, 0x0448,
	0x6F12, 0x054A,
	0x6F12, 0xC1F8,
	0x6F12, 0xC804,
	0x6F12, 0x101A,
	0x6F12, 0xA1F8,
	0x6F12, 0xCC04,
	0x6F12, 0x00F0,
	0x6F12, 0x70BA,
	0x6F12, 0x2000,
	0x6F12, 0x4594,
	0x6F12, 0x2000,
	0x6F12, 0x2E50,
	0x6F12, 0x2000,
	0x6F12, 0x7000,
	0x6F12, 0x10B5,
	0x6F12, 0x00F0,
	0x6F12, 0xB7FA,
	0x6F12, 0xFF49,
	0x6F12, 0x0120,
	0x6F12, 0x0880,
	0x6F12, 0x10BD,
	0x6F12, 0x2DE9,
	0x6F12, 0xF041,
	0x6F12, 0xFD4C,
	0x6F12, 0xFB4F,
	0x6F12, 0x0026,
	0x6F12, 0xB4F8,
	0x6F12, 0x6A52,
	0x6F12, 0x3888,
	0x6F12, 0x08B1,
	0x6F12, 0xA4F8,
	0x6F12, 0x6A62,
	0x6F12, 0x00F0,
	0x6F12, 0xABFA,
	0x6F12, 0x3E80,
	0x6F12, 0xA4F8,
	0x6F12, 0x6A52,
	0x6F12, 0xBDE8,
	0x6F12, 0xF081,
	0x6F12, 0x2DE9,
	0x6F12, 0xF041,
	0x6F12, 0x0746,
	0x6F12, 0xF248,
	0x6F12, 0x0E46,
	0x6F12, 0x0022,
	0x6F12, 0x4068,
	0x6F12, 0x84B2,
	0x6F12, 0x050C,
	0x6F12, 0x2146,
	0x6F12, 0x2846,
	0x6F12, 0x00F0,
	0x6F12, 0x9EFA,
	0x6F12, 0x3146,
	0x6F12, 0x3846,
	0x6F12, 0x00F0,
	0x6F12, 0x9FFA,
	0x6F12, 0xED4F,
	0x6F12, 0x4DF2,
	0x6F12, 0x0C26,
	0x6F12, 0x4FF4,
	0x6F12, 0x8061,
	0x6F12, 0x3A78,
	0x6F12, 0x3046,
	0x6F12, 0x00F0,
	0x6F12, 0x91FA,
	0x6F12, 0x7878,
	0x6F12, 0xB8B3,
	0x6F12, 0x0022,
	0x6F12, 0x8021,
	0x6F12, 0x3046,
	0x6F12, 0x00F0,
	0x6F12, 0x8AFA,
	0x6F12, 0xE648,
	0x6F12, 0x0088,
	0x6F12, 0xE64B,
	0x6F12, 0xA3F8,
	0x6F12, 0x5C02,
	0x6F12, 0xE448,
	0x6F12, 0x001D,
	0x6F12, 0x0088,
	0x6F12, 0xA3F8,
	0x6F12, 0x5E02,
	0x6F12, 0xB3F8,
	0x6F12, 0x5C02,
	0x6F12, 0xB3F8,
	0x6F12, 0x5E12,
	0x6F12, 0x4218,
	0x6F12, 0x02D0,
	0x6F12, 0x8002,
	0x6F12, 0xB0FB,
	0x6F12, 0xF2F2,
	0x6F12, 0x91B2,
	0x6F12, 0xDE4A,
	0x6F12, 0xA3F8,
	0x6F12, 0x6012,
	0x6F12, 0xB2F8,
	0x6F12, 0x1602,
	0x6F12, 0xB2F8,
	0x6F12, 0x1422,
	0x6F12, 0xA3F8,
	0x6F12, 0x9805,
	0x6F12, 0xA3F8,
	0x6F12, 0x9A25,
	0x6F12, 0x8018,
	0x6F12, 0x04D0,
	0x6F12, 0x9202,
	0x6F12, 0xB2FB,
	0x6F12, 0xF0F0,
	0x6F12, 0xA3F8,
	0x6F12, 0x9C05,
	0x6F12, 0xB3F8,
	0x6F12, 0x9C05,
	0x6F12, 0x0A18,
	0x6F12, 0x01FB,
	0x6F12, 0x1020,
	0x6F12, 0x40F3,
	0x6F12, 0x9510,
	0x6F12, 0x1028,
	0x6F12, 0x06DC,
	0x6F12, 0x0028,
	0x6F12, 0x05DA,
	0x6F12, 0x0020,
	0x6F12, 0x03E0,
	0x6F12, 0xFFE7,
	0x6F12, 0x0122,
	0x6F12, 0xC5E7,
	0x6F12, 0x1020,
	0x6F12, 0xCE49,
	0x6F12, 0x0880,
	0x6F12, 0x2146,
	0x6F12, 0x2846,
	0x6F12, 0xBDE8,
	0x6F12, 0xF041,
	0x6F12, 0x0122,
	0x6F12, 0x00F0,
	0x6F12, 0x4ABA,
	0x6F12, 0xF0B5,
	0x6F12, 0xCA4C,
	0x6F12, 0xDDE9,
	0x6F12, 0x0565,
	0x6F12, 0x08B1,
	0x6F12, 0x2788,
	0x6F12, 0x0760,
	0x6F12, 0x09B1,
	0x6F12, 0x6088,
	0x6F12, 0x0860,
	0x6F12, 0x12B1,
	0x6F12, 0xA088,
	0x6F12, 0x401C,
	0x6F12, 0x1060,
	0x6F12, 0x0BB1,
	0x6F12, 0xE088,
	0x6F12, 0x1860,
	0x6F12, 0x0EB1,
	0x6F12, 0xA07B,
	0x6F12, 0x3060,
	0x6F12, 0x002D,
	0x6F12, 0x01D0,
	0x6F12, 0xE07B,
	0x6F12, 0x2860,
	0x6F12, 0xF0BD,
	0x6F12, 0x70B5,
	0x6F12, 0x0646,
	0x6F12, 0xB648,
	0x6F12, 0x0022,
	0x6F12, 0x8068,
	0x6F12, 0x84B2,
	0x6F12, 0x050C,
	0x6F12, 0x2146,
	0x6F12, 0x2846,
	0x6F12, 0x00F0,
	0x6F12, 0x26FA,
	0x6F12, 0x3046,
	0x6F12, 0x00F0,
	0x6F12, 0x2DFA,
	0x6F12, 0xB848,
	0x6F12, 0x0368,
	0x6F12, 0xB3F8,
	0x6F12, 0x7401,
	0x6F12, 0x010A,
	0x6F12, 0xB648,
	0x6F12, 0x4268,
	0x6F12, 0x82F8,
	0x6F12, 0x5010,
	0x6F12, 0x93F8,
	0x6F12, 0x7511,
	0x6F12, 0x82F8,
	0x6F12, 0x5210,
	0x6F12, 0xB3F8,
	0x6F12, 0x7811,
	0x6F12, 0x090A,
	0x6F12, 0x82F8,
	0x6F12, 0x5810,
	0x6F12, 0x93F8,
	0x6F12, 0x7911,
	0x6F12, 0x82F8,
	0x6F12, 0x5A10,
	0x6F12, 0x33F8,
	0x6F12, 0xF01F,
	0x6F12, 0x0068,
	0x6F12, 0x090A,
	0x6F12, 0x00F8,
	0x6F12, 0xCE1F,
	0x6F12, 0x5978,
	0x6F12, 0x8170,
	0x6F12, 0x5988,
	0x6F12, 0x090A,
	0x6F12, 0x0171,
	0x6F12, 0xD978,
	0x6F12, 0x8171,
	0x6F12, 0x988C,
	0x6F12, 0x000A,
	0x6F12, 0x9074,
	0x6F12, 0x93F8,
	0x6F12, 0x2500,
	0x6F12, 0x1075,
	0x6F12, 0xD88C,
	0x6F12, 0x000A,
	0x6F12, 0x9075,
	0x6F12, 0x93F8,
	0x6F12, 0x2700,
	0x6F12, 0x1076,
	0x6F12, 0xB3F8,
	0x6F12, 0xB000,
	0x6F12, 0x000A,
	0x6F12, 0x82F8,
	0x6F12, 0x7E00,
	0x6F12, 0x93F8,
	0x6F12, 0xB100,
	0x6F12, 0x82F8,
	0x6F12, 0x8000,
	0x6F12, 0x9548,
	0x6F12, 0x90F8,
	0x6F12, 0xB313,
	0x6F12, 0x82F8,
	0x6F12, 0x8210,
	0x6F12, 0x90F8,
	0x6F12, 0xB103,
	0x6F12, 0x82F8,
	0x6F12, 0x8400,
	0x6F12, 0x93F8,
	0x6F12, 0xB400,
	0x6F12, 0x82F8,
	0x6F12, 0x8600,
	0x6F12, 0x0020,
	0x6F12, 0x82F8,
	0x6F12, 0x8800,
	0x6F12, 0x93F8,
	0x6F12, 0x6211,
	0x6F12, 0x82F8,
	0x6F12, 0x9610,
	0x6F12, 0x93F8,
	0x6F12, 0x0112,
	0x6F12, 0x82F8,
	0x6F12, 0x9E10,
	0x6F12, 0x93F8,
	0x6F12, 0x0212,
	0x6F12, 0x82F8,
	0x6F12, 0xA010,
	0x6F12, 0x82F8,
	0x6F12, 0xA200,
	0x6F12, 0x82F8,
	0x6F12, 0xA400,
	0x6F12, 0x93F8,
	0x6F12, 0x0512,
	0x6F12, 0x82F8,
	0x6F12, 0xA610,
	0x6F12, 0x93F8,
	0x6F12, 0x0612,
	0x6F12, 0x82F8,
	0x6F12, 0xA810,
	0x6F12, 0x93F8,
	0x6F12, 0x0712,
	0x6F12, 0x82F8,
	0x6F12, 0xAA10,
	0x6F12, 0x82F8,
	0x6F12, 0xAC00,
	0x6F12, 0x5A20,
	0x6F12, 0x82F8,
	0x6F12, 0xAD00,
	0x6F12, 0x93F8,
	0x6F12, 0x0902,
	0x6F12, 0x82F8,
	0x6F12, 0xAE00,
	0x6F12, 0x2146,
	0x6F12, 0x2846,
	0x6F12, 0xBDE8,
	0x6F12, 0x7040,
	0x6F12, 0x0122,
	0x6F12, 0x00F0,
	0x6F12, 0xAFB9,
	0x6F12, 0x70B5,
	0x6F12, 0x7548,
	0x6F12, 0x0022,
	0x6F12, 0x0169,
	0x6F12, 0x0C0C,
	0x6F12, 0x8DB2,
	0x6F12, 0x2946,
	0x6F12, 0x2046,
	0x6F12, 0x00F0,
	0x6F12, 0xA5F9,
	0x6F12, 0x00F0,
	0x6F12, 0xB2F9,
	0x6F12, 0x7248,
	0x6F12, 0x8078,
	0x6F12, 0x08B1,
	0x6F12, 0x4F22,
	0x6F12, 0x00E0,
	0x6F12, 0x2522,
	0x6F12, 0x7748,
	0x6F12, 0x90F8,
	0x6F12, 0xE400,
	0x6F12, 0x0328,
	0x6F12, 0x07D1,
	0x6F12, 0x42F0,
	0x6F12, 0x8002,
	0x6F12, 0x4FF4,
	0x6F12, 0x8361,
	0x6F12, 0x48F6,
	0x6F12, 0x7A20,
	0x6F12, 0x00F0,
	0x6F12, 0xA4F9,
	0x6F12, 0x2946,
	0x6F12, 0x2046,
	0x6F12, 0xBDE8,
	0x6F12, 0x7040,
	0x6F12, 0x0122,
	0x6F12, 0x00F0,
	0x6F12, 0x89B9,
	0x6F12, 0x10B5,
	0x6F12, 0x0221,
	0x6F12, 0x7620,
	0x6F12, 0x00F0,
	0x6F12, 0x9DF9,
	0x6F12, 0x0221,
	0x6F12, 0x4420,
	0x6F12, 0x00F0,
	0x6F12, 0x99F9,
	0x6F12, 0x4021,
	0x6F12, 0x4520,
	0x6F12, 0x00F0,
	0x6F12, 0x95F9,
	0x6F12, 0x5D49,
	0x6F12, 0x0420,
	0x6F12, 0xA1F8,
	0x6F12, 0x3A06,
	0x6F12, 0x10BD,
	0x6F12, 0x7047,
	0x6F12, 0x7047,
	0x6F12, 0x08B5,
	0x6F12, 0x5949,
	0x6F12, 0x3120,
	0x6F12, 0x6A46,
	0x6F12, 0x81F8,
	0x6F12, 0x4306,
	0x6F12, 0x5A20,
	0x6F12, 0x8DF8,
	0x6F12, 0x0000,
	0x6F12, 0x0121,
	0x6F12, 0x7520,
	0x6F12, 0x00F0,
	0x6F12, 0x86F9,
	0x6F12, 0x9DF8,
	0x6F12, 0x0000,
	0x6F12, 0x08BD,
	0x6F12, 0x7047,
	0x6F12, 0x5248,
	0x6F12, 0x10B5,
	0x6F12, 0x8078,
	0x6F12, 0x18B1,
	0x6F12, 0x0021,
	0x6F12, 0x4420,
	0x6F12, 0x00F0,
	0x6F12, 0x75F9,
	0x6F12, 0x4D49,
	0x6F12, 0x0220,
	0x6F12, 0xA1F8,
	0x6F12, 0x3A06,
	0x6F12, 0x10BD,
	0x6F12, 0x5448,
	0x6F12, 0x90F8,
	0x6F12, 0xE400,
	0x6F12, 0x0328,
	0x6F12, 0x01D0,
	0x6F12, 0x00F0,
	0x6F12, 0x73B9,
	0x6F12, 0xAFF2,
	0x6F12, 0x2B01,
	0x6F12, 0x4648,
	0x6F12, 0xC0F8,
	0x6F12, 0x4C16,
	0x6F12, 0x4649,
	0x6F12, 0x8978,
	0x6F12, 0x11B1,
	0x6F12, 0xAFF2,
	0x6F12, 0x8501,
	0x6F12, 0x01E0,
	0x6F12, 0xAFF2,
	0x6F12, 0x6501,
	0x6F12, 0xC0F8,
	0x6F12, 0x4816,
	0x6F12, 0xAFF2,
	0x6F12, 0x6B01,
	0x6F12, 0xC0F8,
	0x6F12, 0x4416,
	0x6F12, 0xAFF2,
	0x6F12, 0x7101,
	0x6F12, 0xC0F8,
	0x6F12, 0x5016,
	0x6F12, 0xAFF2,
	0x6F12, 0x5901,
	0x6F12, 0xC0F8,
	0x6F12, 0x5416,
	0x6F12, 0x7047,
	0x6F12, 0x2DE9,
	0x6F12, 0xF041,
	0x6F12, 0x434C,
	0x6F12, 0x4249,
	0x6F12, 0x0646,
	0x6F12, 0xB4F8,
	0x6F12, 0x6670,
	0x6F12, 0xC989,
	0x6F12, 0xB4F8,
	0x6F12, 0x7E20,
	0x6F12, 0x0020,
	0x6F12, 0xC1B1,
	0x6F12, 0x2146,
	0x6F12, 0xD1F8,
	0x6F12, 0x9010,
	0x6F12, 0x72B1,
	0x6F12, 0x8FB1,
	0x6F12, 0x0846,
	0x6F12, 0x00F0,
	0x6F12, 0x48F9,
	0x6F12, 0x0546,
	0x6F12, 0xA06F,
	0x6F12, 0x00F0,
	0x6F12, 0x44F9,
	0x6F12, 0x8542,
	0x6F12, 0x02D2,
	0x6F12, 0xD4F8,
	0x6F12, 0x9000,
	0x6F12, 0x26E0,
	0x6F12, 0xA06F,
	0x6F12, 0x24E0,
	0x6F12, 0x002F,
	0x6F12, 0xFBD1,
	0x6F12, 0x002A,
	0x6F12, 0x24D0,
	0x6F12, 0x0846,
	0x6F12, 0x1EE0,
	0x6F12, 0x2849,
	0x6F12, 0x8D88,
	0x6F12, 0x8968,
	0x6F12, 0x4B42,
	0x6F12, 0x77B1,
	0x6F12, 0x2F48,
	0x6F12, 0x406F,
	0x6F12, 0x10E0,
	0x6F12, 0x4242,
	0x6F12, 0x00E0,
	0x6F12, 0x0246,
	0x6F12, 0x0029,
	0x6F12, 0x0FDB,
	0x6F12, 0x8A42,
	0x6F12, 0x0FDD,
	0x6F12, 0x3046,
	0x6F12, 0xBDE8,
	0x6F12, 0xF041,
	0x6F12, 0x00F0,
	0x6F12, 0x28B9,
	0x6F12, 0x002A,
	0x6F12, 0x0CD0,
	0x6F12, 0x2748,
	0x6F12, 0xD0F8,
	0x6F12, 0x8800,
	0x6F12, 0x25B1,
	0x6F12, 0x0028,
	0x6F12, 0xEDDA,
	0x6F12, 0xEAE7,
	0x6F12, 0x1946,
	0x6F12, 0xEDE7,
	0x6F12, 0x00F0,
	0x6F12, 0x20F9,
	0x6F12, 0xE060,
	0x6F12, 0x0120,
	0x6F12, 0x3DE6,
	0x6F12, 0x2DE9,
	0x6F12, 0xF047,
	0x6F12, 0x8146,
	0x6F12, 0x0F46,
	0x6F12, 0x0846,
	0x6F12, 0x00F0,
	0x6F12, 0x1BF9,
	0x6F12, 0x1B4C,
	0x6F12, 0x0026,
	0x6F12, 0x608A,
	0x6F12, 0x10B1,
	0x6F12, 0x00F0,
	0x6F12, 0x1AF9,
	0x6F12, 0x6682,
	0x6F12, 0x194D,
	0x6F12, 0x2888,
	0x6F12, 0x0128,
	0x6F12, 0x60D1,
	0x6F12, 0xA08B,
	0x6F12, 0x0028,
	0x6F12, 0x5DD1,
	0x6F12, 0x002F,
	0x6F12, 0x5BD1,
	0x6F12, 0x104F,
	0x6F12, 0x3868,
	0x6F12, 0xB0F8,
	0x6F12, 0x1403,
	0x6F12, 0x38B1,
	0x6F12, 0x2889,
	0x6F12, 0x401C,
	0x6F12, 0x80B2,
	0x6F12, 0x2881,
	0x6F12, 0xFF28,
	0x6F12, 0x01D9,
	0x6F12, 0xA08C,
	0x6F12, 0x2881,
	0x6F12, 0x0F48,
	0x6F12, 0xEE60,
	0x6F12, 0xB0F8,
	0x6F12, 0x5E80,
	0x6F12, 0x1BE0,
	0x6F12, 0x2000,
	0x6F12, 0x4580,
	0x6F12, 0x2000,
	0x6F12, 0x2E50,
	0x6F12, 0x2000,
	0x6F12, 0x6200,
	0x6F12, 0x4000,
	0x6F12, 0x9404,
	0x6F12, 0x2000,
	0x6F12, 0x38E0,
	0x6F12, 0x4000,
	0x6F12, 0xD000,
	0x6F12, 0x4000,
	0x6F12, 0xA410,
	0x6F12, 0x2000,
	0x6F12, 0x2C66,
	0x6F12, 0x2000,
	0x6F12, 0x0890,
	0x6F12, 0x2000,
	0x6F12, 0x3620,
	0x6F12, 0x2000,
	0x6F12, 0x0DE0,
	0x6F12, 0x2000,
	0x6F12, 0x2BC0,
	0x6F12, 0x2000,
	0x6F12, 0x3580,
	0x6F12, 0x4000,
	0x6F12, 0x7000,
	0x6F12, 0x40F2,
	0x6F12, 0xFF31,
	0x6F12, 0x0B20,
	0x6F12, 0x00F0,
	0x6F12, 0xE2F8,
	0x6F12, 0x3868,
	0x6F12, 0xB0F8,
	0x6F12, 0x1213,
	0x6F12, 0x19B1,
	0x6F12, 0x4846,
	0x6F12, 0x00F0,
	0x6F12, 0xC7F8,
	0x6F12, 0x0AE0,
	0x6F12, 0xB0F8,
	0x6F12, 0x1403,
	0x6F12, 0xC0B1,
	0x6F12, 0x2889,
	0x6F12, 0xB4F9,
	0x6F12, 0x2410,
	0x6F12, 0x8842,
	0x6F12, 0x13DB,
	0x6F12, 0x4846,
	0x6F12, 0xFFF7,
	0x6F12, 0x5AFF,
	0x6F12, 0x78B1,
	0x6F12, 0x2E81,
	0x6F12, 0x00F0,
	0x6F12, 0xD0F8,
	0x6F12, 0xE868,
	0x6F12, 0x2861,
	0x6F12, 0x208E,
	0x6F12, 0x18B1,
	0x6F12, 0x608E,
	0x6F12, 0x18B9,
	0x6F12, 0x00F0,
	0x6F12, 0xCDF8,
	0x6F12, 0x608E,
	0x6F12, 0x10B1,
	0x6F12, 0xE889,
	0x6F12, 0x2887,
	0x6F12, 0x6686,
	0x6F12, 0x4046,
	0x6F12, 0xBDE8,
	0x6F12, 0xF047,
	0x6F12, 0x00F0,
	0x6F12, 0xC8B8,
	0x6F12, 0xBDE8,
	0x6F12, 0xF087,
	0x6F12, 0x10B5,
	0x6F12, 0x6021,
	0x6F12, 0x0B20,
	0x6F12, 0x00F0,
	0x6F12, 0xC6F8,
	0x6F12, 0x8106,
	0x6F12, 0x4FEA,
	0x6F12, 0x4061,
	0x6F12, 0x05D5,
	0x6F12, 0x0029,
	0x6F12, 0x0BDA,
	0x6F12, 0xBDE8,
	0x6F12, 0x1040,
	0x6F12, 0x00F0,
	0x6F12, 0xC1B8,
	0x6F12, 0x0029,
	0x6F12, 0x03DA,
	0x6F12, 0xBDE8,
	0x6F12, 0x1040,
	0x6F12, 0x00F0,
	0x6F12, 0xC0B8,
	0x6F12, 0x8006,
	0x6F12, 0x03D5,
	0x6F12, 0xBDE8,
	0x6F12, 0x1040,
	0x6F12, 0x00F0,
	0x6F12, 0xBFB8,
	0x6F12, 0x10BD,
	0x6F12, 0x70B5,
	0x6F12, 0x1E4C,
	0x6F12, 0x0020,
	0x6F12, 0x2080,
	0x6F12, 0xAFF2,
	0x6F12, 0xDF40,
	0x6F12, 0x1C4D,
	0x6F12, 0x2861,
	0x6F12, 0xAFF2,
	0x6F12, 0xD940,
	0x6F12, 0x0022,
	0x6F12, 0xAFF2,
	0x6F12, 0xB941,
	0x6F12, 0xA861,
	0x6F12, 0x1948,
	0x6F12, 0x00F0,
	0x6F12, 0xB2F8,
	0x6F12, 0x0022,
	0x6F12, 0xAFF2,
	0x6F12, 0xD531,
	0x6F12, 0x6060,
	0x6F12, 0x1748,
	0x6F12, 0x00F0,
	0x6F12, 0xABF8,
	0x6F12, 0x0022,
	0x6F12, 0xAFF2,
	0x6F12, 0x1341,
	0x6F12, 0xA060,
	0x6F12, 0x1448,
	0x6F12, 0x00F0,
	0x6F12, 0xA4F8,
	0x6F12, 0x0022,
	0x6F12, 0xAFF2,
	0x6F12, 0xED21,
	0x6F12, 0xE060,
	0x6F12, 0x1248,
	0x6F12, 0x00F0,
	0x6F12, 0x9DF8,
	0x6F12, 0x2061,
	0x6F12, 0xAFF2,
	0x6F12, 0x4920,
	0x6F12, 0x0022,
	0x6F12, 0xAFF2,
	0x6F12, 0x0B21,
	0x6F12, 0xE863,
	0x6F12, 0x0E48,
	0x6F12, 0x00F0,
	0x6F12, 0x93F8,
	0x6F12, 0x0022,
	0x6F12, 0xAFF2,
	0x6F12, 0x8511,
	0x6F12, 0x0C48,
	0x6F12, 0x00F0,
	0x6F12, 0x8DF8,
	0x6F12, 0x0022,
	0x6F12, 0xAFF2,
	0x6F12, 0xA701,
	0x6F12, 0xBDE8,
	0x6F12, 0x7040,
	0x6F12, 0x0948,
	0x6F12, 0x00F0,
	0x6F12, 0x85B8,
	0x6F12, 0x2000,
	0x6F12, 0x4580,
	0x6F12, 0x2000,
	0x6F12, 0x0840,
	0x6F12, 0x0001,
	0x6F12, 0x020D,
	0x6F12, 0x0000,
	0x6F12, 0x67CD,
	0x6F12, 0x0000,
	0x6F12, 0x3AE1,
	0x6F12, 0x0000,
	0x6F12, 0x72B1,
	0x6F12, 0x0000,
	0x6F12, 0x56D7,
	0x6F12, 0x0000,
	0x6F12, 0x5735,
	0x6F12, 0x0000,
	0x6F12, 0x0631,
	0x6F12, 0x45F6,
	0x6F12, 0x250C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x45F6,
	0x6F12, 0xF31C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x4AF2,
	0x6F12, 0xD74C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x40F2,
	0x6F12, 0x0D2C,
	0x6F12, 0xC0F2,
	0x6F12, 0x010C,
	0x6F12, 0x6047,
	0x6F12, 0x46F2,
	0x6F12, 0xCD7C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x47F2,
	0x6F12, 0xB12C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x47F2,
	0x6F12, 0x4F2C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x47F6,
	0x6F12, 0x017C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x47F6,
	0x6F12, 0x636C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x47F2,
	0x6F12, 0x0D0C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x4AF2,
	0x6F12, 0x5F4C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x45F2,
	0x6F12, 0xA56C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x45F2,
	0x6F12, 0x1F5C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x45F2,
	0x6F12, 0x7F5C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x45F2,
	0x6F12, 0x312C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x40F2,
	0x6F12, 0xAB2C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x45F2,
	0x6F12, 0xF34C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x45F2,
	0x6F12, 0x395C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x40F2,
	0x6F12, 0x117C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x40F2,
	0x6F12, 0xD92C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x45F2,
	0x6F12, 0x054C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x45F2,
	0x6F12, 0xAF3C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x45F2,
	0x6F12, 0x4B2C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x4AF6,
	0x6F12, 0xE75C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x30D5,
	0x6F12, 0x0103,
	0x6F12, 0x0000,
	0x6F12, 0x005E,
	0x602A, 0x1662,
	0x6F12, 0x1E00,
	0x602A, 0x1C9A,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x602A, 0x0FF2,
	0x6F12, 0x0020,
	0x602A, 0x0EF6,
	0x6F12, 0x0100,
	0x602A, 0x23B2,
	0x6F12, 0x0001,
	0x602A, 0x0FE4,
	0x6F12, 0x0107,
	0x6F12, 0x07D0,
	0x602A, 0x12F8,
	0x6F12, 0x3D09,
	0x602A, 0x0E18,
	0x6F12, 0x0040,
	0x602A, 0x1066,
	0x6F12, 0x000C,
	0x602A, 0x13DE,
	0x6F12, 0x0000,
	0x602A, 0x12F2,
	0x6F12, 0x0F0F,
	0x602A, 0x13DC,
	0x6F12, 0x806F,
	0xF46E, 0x00C3,
	0xF46C, 0xBFA0,
	0xF44A, 0x0007,
	0xF456, 0x000A,
	0x6028, 0x2000,
	0x602A, 0x12F6,
	0x6F12, 0x7008,
	0x0BC6, 0x0000,
	0x0B36, 0x0001,
	0x6028, 0x2000,
	0x602A, 0x2BC2,
	0x6F12, 0x0020,
	0x602A, 0x2BC4,
	0x6F12, 0x0020,
	0x602A, 0x6204,
	0x6F12, 0x0001,
	0x602A, 0x6208,
	0x6F12, 0x0000,
	0x6F12, 0x0030,
	0x6028, 0x2000,
	0x602A, 0x17C0,
	0x6F12, 0x143C,
};

static kal_uint16 preview_setting_array[] = {
	0x6214, 0x7971,
	0x6218, 0x7150,
	0x0344, 0x0000,
	0x0346, 0x0008,
	0x0348, 0x1077,
	0x034A, 0x0C37,
	0x034C, 0x0838,
	0x034E, 0x0618,
	0x0340, 0x0718,
	0x0342, 0x2270,
	0x0900, 0x0112,
	0x0380, 0x0001,
	0x0382, 0x0001,
	0x0384, 0x0001,
	0x0386, 0x0003,
	0x0402, 0x1010,
	0x0404, 0x2000,
	0x0350, 0x0004,
	0x0352, 0x0000,
	0x0136, 0x1800,
	0x013E, 0x0000,
	0x0300, 0x0008,
	0x0302, 0x0001,
	0x0304, 0x0006,
	0x0306, 0x00F1,
	0x0308, 0x0008,
	0x030A, 0x0001,
	0x030C, 0x0000,
	0x030E, 0x0003,
	0x0310, 0x0063,
	0x0312, 0x0001,
	0x0B06, 0x0101,
	0x6028, 0x2000,
	0x602A, 0x1FF6,
	0x6F12, 0x0000,
	0x021E, 0x0000,
	0x0202, 0x0100,
	0x0204, 0x0020,
	0x0D00, 0x0101,
	0x0D02, 0x0101,
	0x0114, 0x0301,
	0x0D06, 0x0208,
	0x0D08, 0x0300,
	0x6028, 0x2000,
	0x602A, 0x0F10,
	0x6F12, 0x0003,
	0x602A, 0x0F12,
	0x6F12, 0x0200,
	0x602A, 0x2BC0,
	0x6F12, 0x0001,
	0x0B30, 0x0000,
	0x0B32, 0x0000,
	0x0B34, 0x0001,
};

static kal_uint16 capture_setting_array[] = {
	0x6214, 0x7971,
	0x6218, 0x7150,
	0x0344, 0x0000,
	0x0346, 0x0008,
	0x0348, 0x1077,
	0x034A, 0x0C37,
	0x034C, 0x1070,
	0x034E, 0x0C30,
	0x0340, 0x0CF2,
	0x0342, 0x12F0,
	0x0900, 0x0011,
	0x0380, 0x0001,
	0x0382, 0x0001,
	0x0384, 0x0001,
	0x0386, 0x0001,
	0x0402, 0x1010,
	0x0404, 0x1000,
	0x0350, 0x0008,
	0x0352, 0x0000,
	0x0136, 0x1800,
	0x013E, 0x0000,
	0x0300, 0x0008,
	0x0302, 0x0001,
	0x0304, 0x0006,
	0x0306, 0x00F1,
	0x0308, 0x0008,
	0x030A, 0x0001,
	0x030C, 0x0000,
	0x030E, 0x0003,
	0x0310, 0x005A,
	0x0312, 0x0000,
	0x0B06, 0x0101,
	0x6028, 0x2000,
	0x602A, 0x1FF6,
	0x6F12, 0x0000,
	0x021E, 0x0000,
	0x0202, 0x0100,
	0x0204, 0x0020,
	0x0D00, 0x0101,
	0x0D02, 0x0101,
	0x0114, 0x0301,
	0x0D06, 0x0208,
	0x0D08, 0x0300,
	0x6028, 0x2000,
	0x602A, 0x0F10,
	0x6F12, 0x0003,
	0x602A, 0x0F12,
	0x6F12, 0x0200,
	0x602A, 0x2BC0,
	0x6F12, 0x0001,
	0x0B30, 0x0000,
	0x0B32, 0x0000,
	0x0B34, 0x0001,
};

static kal_uint16 normal_video_setting_array[] = {
	0x6214, 0x7971,
	0x6218, 0x7150,
	0x0344, 0x0000,
	0x0346, 0x0008,
	0x0348, 0x1077,
	0x034A, 0x0C37,
	0x034C, 0x0838,
	0x034E, 0x0618,
	0x0340, 0x0718,
	0x0342, 0x2270,
	0x0900, 0x0112,
	0x0380, 0x0001,
	0x0382, 0x0001,
	0x0384, 0x0001,
	0x0386, 0x0003,
	0x0402, 0x1010,
	0x0404, 0x2000,
	0x0350, 0x0004,
	0x0352, 0x0000,
	0x0136, 0x1800,
	0x013E, 0x0000,
	0x0300, 0x0008,
	0x0302, 0x0001,
	0x0304, 0x0006,
	0x0306, 0x00F1,
	0x0308, 0x0008,
	0x030A, 0x0001,
	0x030C, 0x0000,
	0x030E, 0x0003,
	0x0310, 0x0063,
	0x0312, 0x0001,
	0x0B06, 0x0101,
	0x6028, 0x2000,
	0x602A, 0x1FF6,
	0x6F12, 0x0000,
	0x021E, 0x0000,
	0x0202, 0x0100,
	0x0204, 0x0020,
	0x0D00, 0x0101,
	0x0D02, 0x0101,
	0x0114, 0x0301,
	0x0D06, 0x0208,
	0x0D08, 0x0300,
	0x6028, 0x2000,
	0x602A, 0x0F10,
	0x6F12, 0x0003,
	0x602A, 0x0F12,
	0x6F12, 0x0200,
	0x602A, 0x2BC0,
	0x6F12, 0x0001,
	0x0B30, 0x0000,
	0x0B32, 0x0000,
	0x0B34, 0x0001,
};

static kal_uint16 hs_video_setting_array[] = {
	0x6214, 0x7971,
	0x6218, 0x7150,
	0x0344, 0x0000,
	0x0346, 0x0008,
	0x0348, 0x1077,
	0x034A, 0x0C37,
	0x034C, 0x0838,
	0x034E, 0x0618,
	0x0340, 0x0718,
	0x0342, 0x2270,
	0x0900, 0x0112,
	0x0380, 0x0001,
	0x0382, 0x0001,
	0x0384, 0x0001,
	0x0386, 0x0003,
	0x0402, 0x1010,
	0x0404, 0x2000,
	0x0350, 0x0004,
	0x0352, 0x0000,
	0x0136, 0x1800,
	0x013E, 0x0000,
	0x0300, 0x0008,
	0x0302, 0x0001,
	0x0304, 0x0006,
	0x0306, 0x00F1,
	0x0308, 0x0008,
	0x030A, 0x0001,
	0x030C, 0x0000,
	0x030E, 0x0003,
	0x0310, 0x0063,
	0x0312, 0x0001,
	0x0B06, 0x0101,
	0x6028, 0x2000,
	0x602A, 0x1FF6,
	0x6F12, 0x0000,
	0x021E, 0x0000,
	0x0202, 0x0100,
	0x0204, 0x0020,
	0x0D00, 0x0100,
	0x0D02, 0x0001,
	0x0114, 0x0301,
	0x0D06, 0x0208,
	0x0D08, 0x0300,
	0x6028, 0x2000,
	0x602A, 0x0F10,
	0x6F12, 0x0003,
	0x602A, 0x0F12,
	0x6F12, 0x0200,
	0x602A, 0x2BC0,
	0x6F12, 0x0001,
	0x0B30, 0x0000,
	0x0B32, 0x0000,
	0x0B34, 0x0001,
};

static kal_uint16 slim_video_setting_array[] = {
	0x6214, 0x7971,
	0x6218, 0x7150,
	0x0344, 0x0000,
	0x0346, 0x0008,
	0x0348, 0x1077,
	0x034A, 0x0C37,
	0x034C, 0x0838,
	0x034E, 0x0618,
	0x0340, 0x0718,
	0x0342, 0x2270,
	0x0900, 0x0112,
	0x0380, 0x0001,
	0x0382, 0x0001,
	0x0384, 0x0001,
	0x0386, 0x0003,
	0x0402, 0x1010,
	0x0404, 0x2000,
	0x0350, 0x0004,
	0x0352, 0x0000,
	0x0136, 0x1800,
	0x013E, 0x0000,
	0x0300, 0x0008,
	0x0302, 0x0001,
	0x0304, 0x0006,
	0x0306, 0x00F1,
	0x0308, 0x0008,
	0x030A, 0x0001,
	0x030C, 0x0000,
	0x030E, 0x0003,
	0x0310, 0x0063,
	0x0312, 0x0001,
	0x0B06, 0x0101,
	0x6028, 0x2000,
	0x602A, 0x1FF6,
	0x6F12, 0x0000,
	0x021E, 0x0000,
	0x0202, 0x0100,
	0x0204, 0x0020,
	0x0D00, 0x0100,
	0x0D02, 0x0001,
	0x0114, 0x0301,
	0x0D06, 0x0208,
	0x0D08, 0x0300,
	0x6028, 0x2000,
	0x602A, 0x0F10,
	0x6F12, 0x0003,
	0x602A, 0x0F12,
	0x6F12, 0x0200,
	0x602A, 0x2BC0,
	0x6F12, 0x0001,
	0x0B30, 0x0000,
	0x0B32, 0x0000,
	0x0B34, 0x0001,
};

static kal_uint16 custom1_setting_array[] = {
	0x6214, 0x7971,
	0x6218, 0x7150,
	0x0344, 0x0000,
	0x0346, 0x0008,
	0x0348, 0x1077,
	0x034A, 0x0C37,
	0x034C, 0x1070,
	0x034E, 0x0C30,
	0x0340, 0x0D48,
	0x0342, 0x1710,
	0x0900, 0x0011,
	0x0380, 0x0001,
	0x0382, 0x0001,
	0x0384, 0x0001,
	0x0386, 0x0001,
	0x0402, 0x1010,
	0x0404, 0x1000,
	0x0350, 0x0008,
	0x0352, 0x0000,
	0x0136, 0x1800,
	0x013E, 0x0000,
	0x0300, 0x0008,
	0x0302, 0x0001,
	0x0304, 0x0006,
	0x0306, 0x00F1,
	0x0308, 0x0008,
	0x030A, 0x0001,
	0x030C, 0x0000,
	0x030E, 0x0003,
	0x0310, 0x004A,
	0x0312, 0x0000,
	0x0B06, 0x0101,
	0x6028, 0x2000,
	0x602A, 0x602A,
	0x6F12, 0x0000,
	0x021E, 0x0000,
	0x0202, 0x0100,
	0x0204, 0x0020,
	0x0D00, 0x0100,
	0x0D02, 0x0001,
	0x0114, 0x0301,
	0x0D06, 0x0208,
	0x0D08, 0x0300,
	0x6028, 0x2000,
	0x602A, 0x602A,
	0x6F12, 0x0003,
	0x602A, 0x602A,
	0x6F12, 0x0200,
	0x602A, 0x602A,
	0x6F12, 0x0001,
	0x0B30, 0x0000,
	0x0B32, 0x0000,
	0x0B34, 0x0001,
};

static kal_uint16 custom2_setting_array[] = {
	0x6214, 0x7971,
	0x6218, 0x7150,
	0x0344, 0x0000,
	0x0346, 0x0008,
	0x0348, 0x1077,
	0x034A, 0x0C37,
	0x034C, 0x0838,
	0x034E, 0x0618,
	0x0340, 0x0678,
	0x0342, 0x12F0,
	0x0900, 0x0112,
	0x0380, 0x0001,
	0x0382, 0x0001,
	0x0384, 0x0001,
	0x0386, 0x0003,
	0x0402, 0x1010,
	0x0404, 0x2000,
	0x0350, 0x0004,
	0x0352, 0x0000,
	0x0136, 0x1800,
	0x013E, 0x0000,
	0x0300, 0x0008,
	0x0302, 0x0001,
	0x0304, 0x0006,
	0x0306, 0x00F1,
	0x0308, 0x0008,
	0x030A, 0x0001,
	0x030C, 0x0000,
	0x030E, 0x0003,
	0x0310, 0x005A,
	0x0312, 0x0000,
	0x0B06, 0x0101,
	0x6028, 0x2000,
	0x602A, 0x1FF6,
	0x6F12, 0x0000,
	0x021E, 0x0000,
	0x0202, 0x0100,
	0x0204, 0x0020,
	0x0D00, 0x0100,
	0x0D02, 0x0001,
	0x0114, 0x0301,
	0x0D06, 0x0208,
	0x0D08, 0x0300,
	0x6028, 0x2000,
	0x602A, 0x0F10,
	0x6F12, 0x0003,
	0x602A, 0x0F12,
	0x6F12, 0x0200,
	0x602A, 0x2BC0,
	0x6F12, 0x0001,
	0x0B30, 0x0000,
	0x0B32, 0x0000,
	0x0B34, 0x0001,
};

static kal_uint16 custom3_setting_array[] = {
	0x6214, 0x7971,
	0x6218, 0x7150,
	0x0344, 0x0000,
	0x0346, 0x0008,
	0x0348, 0x1077,
	0x034A, 0x0C37,
	0x034C, 0x0838,
	0x034E, 0x0618,
	0x0340, 0x0718,
	0x0342, 0x2270,
	0x0900, 0x0112,
	0x0380, 0x0001,
	0x0382, 0x0001,
	0x0384, 0x0001,
	0x0386, 0x0003,
	0x0402, 0x1010,
	0x0404, 0x2000,
	0x0350, 0x0004,
	0x0352, 0x0000,
	0x0136, 0x1800,
	0x013E, 0x0000,
	0x0300, 0x0008,
	0x0302, 0x0001,
	0x0304, 0x0006,
	0x0306, 0x00F1,
	0x0308, 0x0008,
	0x030A, 0x0001,
	0x030C, 0x0000,
	0x030E, 0x0003,
	0x0310, 0x0063,
	0x0312, 0x0001,
	0x0B06, 0x0101,
	0x6028, 0x2000,
	0x602A, 0x1FF6,
	0x6F12, 0x0000,
	0x021E, 0x0000,
	0x0202, 0x0100,
	0x0204, 0x0020,
	0x0D00, 0x0100,
	0x0D02, 0x0001,
	0x0114, 0x0301,
	0x0D06, 0x0208,
	0x0D08, 0x0300,
	0x6028, 0x2000,
	0x602A, 0x0F10,
	0x6F12, 0x0003,
	0x602A, 0x0F12,
	0x6F12, 0x0200,
	0x602A, 0x2BC0,
	0x6F12, 0x0001,
	0x0B30, 0x0000,
	0x0B32, 0x0000,
	0x0B34, 0x0001,
};

static kal_uint16 custom4_setting_array[] = {
	0x6214, 0x7971,
	0x6218, 0x7150,
	0x0344, 0x0000,
	0x0346, 0x0008,
	0x0348, 0x1077,
	0x034A, 0x0C37,
	0x034C, 0x0838,
	0x034E, 0x0618,
	0x0340, 0x0718,
	0x0342, 0x2270,
	0x0900, 0x0112,
	0x0380, 0x0001,
	0x0382, 0x0001,
	0x0384, 0x0001,
	0x0386, 0x0003,
	0x0402, 0x1010,
	0x0404, 0x2000,
	0x0350, 0x0004,
	0x0352, 0x0000,
	0x0136, 0x1800,
	0x013E, 0x0000,
	0x0300, 0x0008,
	0x0302, 0x0001,
	0x0304, 0x0006,
	0x0306, 0x00F1,
	0x0308, 0x0008,
	0x030A, 0x0001,
	0x030C, 0x0000,
	0x030E, 0x0003,
	0x0310, 0x0063,
	0x0312, 0x0001,
	0x0B06, 0x0101,
	0x6028, 0x2000,
	0x602A, 0x1FF6,
	0x6F12, 0x0000,
	0x021E, 0x0000,
	0x0202, 0x0100,
	0x0204, 0x0020,
	0x0D00, 0x0100,
	0x0D02, 0x0001,
	0x0114, 0x0301,
	0x0D06, 0x0208,
	0x0D08, 0x0300,
	0x6028, 0x2000,
	0x602A, 0x0F10,
	0x6F12, 0x0003,
	0x602A, 0x0F12,
	0x6F12, 0x0200,
	0x602A, 0x2BC0,
	0x6F12, 0x0001,
	0x0B30, 0x0000,
	0x0B32, 0x0000,
	0x0B34, 0x0001,
};

static kal_uint16 custom5_setting_array[] = {
	0x6214, 0x7971,
	0x6218, 0x7150,
	0x0344, 0x0000,
	0x0346, 0x0008,
	0x0348, 0x1077,
	0x034A, 0x0C37,
	0x034C, 0x0838,
	0x034E, 0x0618,
	0x0340, 0x0718,
	0x0342, 0x2270,
	0x0900, 0x0112,
	0x0380, 0x0001,
	0x0382, 0x0001,
	0x0384, 0x0001,
	0x0386, 0x0003,
	0x0402, 0x1010,
	0x0404, 0x2000,
	0x0350, 0x0004,
	0x0352, 0x0000,
	0x0136, 0x1800,
	0x013E, 0x0000,
	0x0300, 0x0008,
	0x0302, 0x0001,
	0x0304, 0x0006,
	0x0306, 0x00F1,
	0x0308, 0x0008,
	0x030A, 0x0001,
	0x030C, 0x0000,
	0x030E, 0x0003,
	0x0310, 0x0063,
	0x0312, 0x0001,
	0x0B06, 0x0101,
	0x6028, 0x2000,
	0x602A, 0x1FF6,
	0x6F12, 0x0000,
	0x021E, 0x0000,
	0x0202, 0x0100,
	0x0204, 0x0020,
	0x0D00, 0x0100,
	0x0D02, 0x0001,
	0x0114, 0x0301,
	0x0D06, 0x0208,
	0x0D08, 0x0300,
	0x6028, 0x2000,
	0x602A, 0x0F10,
	0x6F12, 0x0003,
	0x602A, 0x0F12,
	0x6F12, 0x0200,
	0x602A, 0x2BC0,
	0x6F12, 0x0001,
	0x0B30, 0x0000,
	0x0B32, 0x0000,
	0x0B34, 0x0001,
};

/* VC2 for PDAF */
static struct SENSOR_VC_INFO_STRUCT vc_info_preview = {
	0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
	0x00, 0x2b, 0x0838, 0x0618, /* VC0 */
	0x00, 0x00, 0x0000, 0x0000, /* VC1 */
	0x01, 0x30, 0x0208, 0x0300, /* VC2 */
	0x00, 0x00, 0x0000, 0x0000, /* VC3 */
};

static struct SENSOR_VC_INFO_STRUCT vc_info_capture = {
	0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
	0x00, 0x2b, 0x1070, 0x0c30, /* VC0 */
	0x00, 0x00, 0x0000, 0x0000, /* VC1 */
	0x01, 0x30, 0x0208, 0x0300, /* VC2 */
	0x00, 0x00, 0x0000, 0x0000, /* VC3 */
};

static struct SENSOR_VC_INFO_STRUCT vc_info_video = {
	0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
	0x00, 0x2b, 0x0838, 0x0618, /* VC0 */
	0x00, 0x00, 0x0000, 0x0000, /* VC1 */
	0x01, 0x30, 0x0208, 0x0300, /* VC2 */
	0x00, 0x00, 0x0000, 0x0000, /* VC3 */
};

static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[] = {
{4208, 3120, 0, 0, 4208, 3120, 2104, 1560, 0, 0, 2104, 1560, 0, 0, 2104, 1560},
{4208, 3120, 0, 0, 4208, 3120, 4208, 3120, 0, 0, 4208, 3120, 0, 0, 4208, 3120},
{4208, 3120, 0, 0, 4208, 3120, 2104, 1560, 0, 0, 2104, 1560, 0, 0, 2104, 1560},
{4208, 3120, 0, 0, 4208, 3120, 2104, 1560, 0, 0, 2104, 1560, 0, 0, 2104, 1560},
{4208, 3120, 0, 0, 4208, 3120, 2104, 1560, 0, 0, 2104, 1560, 0, 0, 2104, 1560},
{4208, 3120, 0, 0, 4208, 3120, 4208, 3120, 0, 0, 4208, 3120, 0, 0, 4208, 3120},
{4208, 3120, 0, 0, 4208, 3120, 2104, 1560, 0, 0, 2104, 1560, 0, 0, 2104, 1560},
{4208, 3120, 0, 0, 4208, 3120, 2104, 1560, 0, 0, 2104, 1560, 0, 0, 2104, 1560},
{4208, 3120, 0, 0, 4208, 3120, 2104, 1560, 0, 0, 2104, 1560, 0, 0, 2104, 1560},
{4208, 3120, 0, 0, 4208, 3120, 2104, 1560, 0, 0, 2104, 1560, 0, 0, 2104, 1560},
};

//customerconfig
static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,	/* mirrorflip information */
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x3D0,	/* current shutter */
	.gain = 0x100,		/* current gain */
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = 0,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_mode = 0, /* sensor need support LE, SE with HDR feature */
	.i2c_write_id = 0x34, /* record current sensor's i2c write id */
	.current_ae_effective_frame = 2,
};

static struct IMGSENSOR_I2C_CFG *get_i2c_cfg(void)
{
	return &(((struct IMGSENSOR_SENSOR_INST *)
		  (imgsensor.psensor_func->psensor_inst))->i2c_cfg);
}

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF)};

	imgsensor_i2c_read(
		get_i2c_cfg(),
		pusendcmd,
		2,
		(u8 *)&get_byte,
		2,
		imgsensor.i2c_write_id,
		IMGSENSOR_I2C_SPEED);

	return ((get_byte<<8)&0xff00) | ((get_byte>>8)&0x00ff);
}

static kal_uint16 read_cmos_sensor_8(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	imgsensor_i2c_read(
		get_i2c_cfg(),
		pusendcmd,
		2,
		(u8 *)&get_byte,
		1,
		imgsensor.i2c_write_id,
		IMGSENSOR_I2C_SPEED);

	return get_byte;
}

static void write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
	char pusendcmd[3] = {(char)(addr >> 8), (char)(addr & 0xFF),
		(char)(para & 0xFF)};

	imgsensor_i2c_write(
		get_i2c_cfg(),
		pusendcmd,
		3,
		3,
		imgsensor.i2c_write_id,
		IMGSENSOR_I2C_SPEED);
}

static void write_cmos_sensor(kal_uint16 addr, kal_uint16 para)
{
	char pusendcmd[4] = {
		(char)(addr >> 8),
		(char)(addr & 0xFF),
		(char)(para >> 8),
		(char)(para & 0xFF) };

	imgsensor_i2c_write(
		get_i2c_cfg(),
		pusendcmd,
		4,
		4,
		imgsensor.i2c_write_id,
		IMGSENSOR_I2C_SPEED);
}

static kal_uint16 table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len)
{
	char puSendCmd[I2C_BUFFER_LEN];
	kal_uint32 tosend, IDX;
	kal_uint16 addr = 0, addr_last = 0, data;

	tosend = 0;
	IDX = 0;

	while (len > IDX) {
		addr = para[IDX];
		{
			puSendCmd[tosend++] = (char)(addr >> 8);
			puSendCmd[tosend++] = (char)(addr & 0xFF);
			data = para[IDX + 1];
#if MULTI_WRITE_REGISTER_VALUE == 16
			puSendCmd[tosend++] = (char)(data >> 8);
#endif
			puSendCmd[tosend++] = (char)(data & 0xFF);
			IDX += 2;
			addr_last = addr;
		}

#if MULTI_WRITE_REGISTER_VALUE == 8
		/* Write when remain buffer size is less than 3 bytes
		 * or reach end of data
		 */
		if ((I2C_BUFFER_LEN - tosend) < 3
		    || IDX == len
		    || addr != addr_last) {

			imgsensor_i2c_write(
							get_i2c_cfg(),
							pusendcmd,
							tosend,
							3,
							imgsensor.i2c_write_id,
							imgsensor_info.i2c_speed);

			tosend = 0;
		}

#elif MULTI_WRITE_REGISTER_VALUE == 16
		if ((I2C_BUFFER_LEN - tosend) < 4
		    || len == IDX
		    || addr != addr_last) {

			imgsensor_i2c_write(
						get_i2c_cfg(),
						puSendCmd,
						tosend,
						4,
						imgsensor.i2c_write_id,
						imgsensor_info.i2c_speed);

			tosend = 0;
		}

#else
		/*just for debug*/
		iWriteRegI2C(puSendCmd, 3, imgsensor.i2c_write_id);
		tosend = 0;
#endif
	}

#if 0 /*for debug*/
	for (int i = 0; i < len/2; i++)
		LOG_INF("readback addr(0x%x)=0x%x\n",
			para[2*i], read_cmos_sensor(para[2*i]));
#endif
	return 0;
}

static void set_mirror_flip(kal_uint8 image_mirror)
{
	kal_uint8 itemp;

	LOG_INF("image_mirror = %d\n", image_mirror);
	itemp = read_cmos_sensor_8(0x0101);
	itemp &= ~0x03;

	switch (image_mirror) {

	case IMAGE_NORMAL:
		write_cmos_sensor_8(0x0101, itemp);
		break;

	case IMAGE_V_MIRROR:
		write_cmos_sensor_8(0x0101, itemp | 0x02);
		break;

	case IMAGE_H_MIRROR:
		write_cmos_sensor_8(0x0101, itemp | 0x01);
		break;

	case IMAGE_HV_MIRROR:
		write_cmos_sensor_8(0x0101, itemp | 0x03);
		break;
	}
}

static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 reg_gain = 0x0;

	reg_gain = gain / 2;
	return (kal_uint16) reg_gain;
}


static kal_uint32 set_test_pattern_mode(kal_uint32 modes,
	struct SET_SENSOR_PATTERN_SOLID_COLOR *pdata)
{
	kal_uint16 Color_R, Color_Gr, Color_Gb, Color_B;

	LOG_INF("set_test_pattern enum: %d\n", modes);
	if (modes) {
		write_cmos_sensor(0x0600, modes);
		if (modes == 1 && (pdata != NULL)) { //Solid Color
			pr_debug("R=0x%x,Gr=0x%x,B=0x%x,Gb=0x%x",
				pdata->COLOR_R, pdata->COLOR_Gr, pdata->COLOR_B, pdata->COLOR_Gb);
			Color_R = (pdata->COLOR_R >> 22) & 0x3FF; //10bits depth color
			Color_Gr = (pdata->COLOR_Gr >> 22) & 0x3FF;
			Color_B = (pdata->COLOR_B >> 22) & 0x3FF;
			Color_Gb = (pdata->COLOR_Gb >> 22) & 0x3FF;
			//write_cmos_sensor(0x0603, (Color_R >> 8) & 0x3);
			write_cmos_sensor(0x0602, Color_R & 0x3FF);
			//write_cmos_sensor(0x0605, (Color_Gr >> 8) & 0x3);
			write_cmos_sensor(0x0604, Color_Gr & 0x3FF);
			//write_cmos_sensor(0x0607, (Color_B >> 8) & 0x3);
			write_cmos_sensor(0x0606, Color_B & 0x3FF);
			//write_cmos_sensor(0x0609, (Color_Gb >> 8) & 0x3);
			write_cmos_sensor(0x0608, Color_Gb & 0x3FF);
		}
	}
	else
		write_cmos_sensor(0x0600, 0x0000); /*No pattern*/
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = modes;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_int32 get_sensor_temperature(void)
{
	UINT8 temperature = 0;
	INT32 temperature_convert = 0;

	temperature = read_cmos_sensor_8(0x013a);

	if (temperature >= 0x0 && temperature <= 0x60)
		temperature_convert = temperature;
	else if (temperature >= 0x61 && temperature <= 0x7F)
		temperature_convert = 97;
	else if (temperature >= 0x80 && temperature <= 0xE2)
		temperature_convert = -30;
	else
		temperature_convert = (INT8)temperature | 0xFFFFFF0;

	/* LOG_INF("temp_c(%d), read_reg(%d)\n",
	 * temperature_convert, temperature);
	 */

	return temperature_convert;
}

static void set_dummy(void)
{
	LOG_INF("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);

	write_cmos_sensor(0x0340, imgsensor.frame_length & 0xFFFF);
	write_cmos_sensor(0x0342, imgsensor.line_length & 0xFFFF);
}	/*  set_dummy  */

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	/*  kal_int16 dummy_line;  */
	kal_uint32 frame_length = imgsensor.frame_length;

	LOG_INF("framerate = %d, min framelength should enable %d\n",
		framerate, min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	if (frame_length >= imgsensor.min_frame_length)
		imgsensor.frame_length = frame_length;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;

	imgsensor.dummy_line
		= imgsensor.frame_length - imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line
			= imgsensor.frame_length - imgsensor.min_frame_length;
	}

	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;

	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}	/*  set_max_framerate  */

#define MAX_CIT_LSHIFT 7
static void write_shutter(kal_uint32 shutter)
{
	kal_uint16 realtime_fps = 0;
	kal_uint16 l_shift = 1;

	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;

	spin_unlock(&imgsensor_drv_lock);
	if (shutter < imgsensor_info.min_shutter)
		shutter = imgsensor_info.min_shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps
			= imgsensor.pclk
			/ imgsensor.line_length * 10
			/ imgsensor.frame_length;

		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			/* Extend frame length*/
			write_cmos_sensor(0x0340,
					  imgsensor.frame_length & 0xFFFF);
		}
	} else {
		/* Extend frame length*/
		write_cmos_sensor(0x0340, imgsensor.frame_length & 0xFFFF);

		LOG_INF("(else)imgsensor.frame_length = %d\n",
			imgsensor.frame_length);
	}

	/* long expsoure */
	if (shutter
	    > (imgsensor_info.max_frame_length - imgsensor_info.margin)) {

		for (l_shift = 1; l_shift < MAX_CIT_LSHIFT; l_shift++) {
			if ((shutter >> l_shift)
			    < (imgsensor_info.max_frame_length
			       - imgsensor_info.margin))

				break;
		}
		if (l_shift > MAX_CIT_LSHIFT) {
			LOG_INF(
				"Unable to set such a long exposure %d, set to max\n",
				shutter);

			l_shift = MAX_CIT_LSHIFT;
		}
		shutter = shutter >> l_shift;
		imgsensor.frame_length = shutter + imgsensor_info.margin;

		LOG_INF("enter long exposure mode, time is %d", l_shift);

		write_cmos_sensor(0x0340, imgsensor.frame_length & 0xFFFF);

		/* Frame exposure mode customization for LE*/
		imgsensor.ae_frm_mode.frame_mode_1 = IMGSENSOR_AE_MODE_SE;
		imgsensor.ae_frm_mode.frame_mode_2 = IMGSENSOR_AE_MODE_SE;
		imgsensor.current_ae_effective_frame = 2;
	} else {
		write_cmos_sensor(0x0340, imgsensor.frame_length & 0xFFFF);
		imgsensor.current_ae_effective_frame = 2;

		LOG_INF("exit long exposure mode");
	}

	/* Update Shutter */
	write_cmos_sensor(0X0202, shutter & 0xFFFF);
	LOG_INF("shutter =%d, framelength =%d\n",
		shutter, imgsensor.frame_length);

}	/*  write_shutter  */

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
static void set_shutter(kal_uint32 shutter)
{
	unsigned long flags;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	write_shutter(shutter);
} /* set_shutter */

/*************************************************************************
 * FUNCTION
 *	set_shutter_frame_length
 *
 * DESCRIPTION
 *	for frame & 3A sync
 *
 *************************************************************************/
static void set_shutter_frame_length(kal_uint32 shutter,
				     kal_uint32 frame_length,
				     kal_bool auto_extend_en)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	spin_lock(&imgsensor_drv_lock);

	/* Change frame time */
	/* dummy_line = frame_length - imgsensor.frame_length;
	 * imgsensor.frame_length = imgsensor.frame_length + dummy_line;
	 * imgsensor.min_frame_length = imgsensor.frame_length;

	 * if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
	 *	imgsensor.frame_length = shutter + imgsensor_info.margin;
	 */

	if (frame_length > 1)
		imgsensor.frame_length = frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;

	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter)
		? imgsensor_info.min_shutter
		: shutter;

	shutter = (shutter > (imgsensor_info.max_frame_length
			      - imgsensor_info.margin))
		? (imgsensor_info.max_frame_length - imgsensor_info.margin)
		: shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps
			= imgsensor.pclk
			/ imgsensor.line_length * 10
			/ imgsensor.frame_length;

		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			/* Extend frame length */
			write_cmos_sensor(0x0340,
					  imgsensor.frame_length & 0xFFFF);
		}
	} else {
		/* Extend frame length */
		write_cmos_sensor(0x0340, imgsensor.frame_length & 0xFFFF);
	}

	/* Update Shutter */
	write_cmos_sensor(0X0202, shutter & 0xFFFF);

	LOG_INF("Exit! shutter =%d, framelength =%d\n",
		shutter, imgsensor.frame_length);

}	/* set_shutter_frame_length */

/*************************************************************************
 * FUNCTION
 *	set_gain
 *
 * DESCRIPTION
 *	This function is to set global gain to sensor.
 *
 * PARAMETERS
 *	iGain : sensor global gain(base: 0x40)
 *
 * RETURNS
 *	the actually gain set to sensor.
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain;

	if (gain < imgsensor_info.min_gain || gain > imgsensor_info.max_gain) {
		LOG_INF("Error gain setting");

		if (gain < imgsensor_info.min_gain)
			gain = imgsensor_info.min_gain;
		else
			gain = imgsensor_info.max_gain;
	}

	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("gain = %d, reg_gain = 0x%x\n ", gain, reg_gain);

	write_cmos_sensor(0x0204, (reg_gain & 0xFFFF));
	return gain;
} /* set_gain */

/*************************************************************************
 * FUNCTION
 *	night_mode
 *
 * DESCRIPTION
 *	This function night mode of sensor.
 *
 * PARAMETERS
 *	bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
#define CHECK_STREAM 1
#if CHECK_STREAM
static void check_streamon(void)
{
	unsigned int i = 0, framecnt = 0;

	for (i = 0; i < 30; i++) {
		framecnt = read_cmos_sensor_8(0x0005);
		LOG_INF("Stream on framecnt = %d\n", framecnt);
		if (framecnt != 0xFF)
			return;
		mdelay(1);
	}
}

static void check_streamoff(void)
{
	unsigned int i = 0, framecnt = 0;
	int timeout = (10000/imgsensor.current_fps)+1;

	for (i = 0; i < timeout; i++) {
		framecnt = read_cmos_sensor_8(0x0005);
		LOG_INF("Stream off framecnt = %d\n", framecnt);
		if (framecnt == 0xFF)
			return;
		mdelay(1);
	}
	LOG_INF(" Stream Off Fail1!\n");
}
#endif

static kal_uint32 streaming_control(kal_bool enable)
{
	unsigned int tmp;

	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
	if (enable) {
		write_cmos_sensor_8(0x0100, 0X01);
		/* Make sure streaming is ongoing */
		check_streamon();
	} else {
		tmp = read_cmos_sensor_8(0x0100);
		if (tmp)
			write_cmos_sensor_8(0x0100, 0x00);
		//check_streamoff();
	}
	return ERROR_NONE;
}

static void sensor_init(void)
{
	LOG_INF("E\n");
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x0000, 0x0006);
	write_cmos_sensor(0x0000, 0x30D5);
	write_cmos_sensor(0x6214, 0x7971);
	write_cmos_sensor(0x6218, 0x7150);
	mdelay(3);
	write_cmos_sensor(0x0A02, 0x7800);
	table_write_cmos_sensor(
		sensor_init_setting_array,
		sizeof(sensor_init_setting_array)/sizeof(kal_uint16));
}

static void preview_setting(void)
{
	LOG_INF("E\n");
	table_write_cmos_sensor(
		preview_setting_array,
		sizeof(preview_setting_array)/sizeof(kal_uint16));
	LOG_INF("X");
} /* preview_setting */

static void capture_setting(kal_uint16 currefps)
{
	LOG_INF(" E! currefps:%d\n",  currefps);
	table_write_cmos_sensor(
		capture_setting_array,
		sizeof(capture_setting_array)/sizeof(kal_uint16));
}

static void normal_video_setting(kal_uint16 currefps)
{
	LOG_INF(" E! currefps:%d\n",  currefps);
	table_write_cmos_sensor(
		normal_video_setting_array,
		sizeof(normal_video_setting_array)/sizeof(kal_uint16));
}

static void hs_video_setting(void)
{
	LOG_INF("E\n");
	table_write_cmos_sensor(
		hs_video_setting_array,
		sizeof(hs_video_setting_array)/sizeof(kal_uint16));
}

static void slim_video_setting(void)
{
	LOG_INF("E\n");
	table_write_cmos_sensor(
		slim_video_setting_array,
		sizeof(slim_video_setting_array)/sizeof(kal_uint16));
}

static void custom1_setting(void)
{
	LOG_INF("E\n");
	table_write_cmos_sensor(
		custom1_setting_array,
		sizeof(custom1_setting_array)/sizeof(kal_uint16));
}

static void custom2_setting(void)
{
	LOG_INF("E\n");
	table_write_cmos_sensor(
		custom2_setting_array,
		sizeof(custom2_setting_array)/sizeof(kal_uint16));
}

static void custom3_setting(void)
{
	LOG_INF("E\n");
	table_write_cmos_sensor(
		custom3_setting_array,
		sizeof(custom3_setting_array)/sizeof(kal_uint16));
}

static void custom4_setting(void)
{
	LOG_INF("E\n");
	table_write_cmos_sensor(
		custom4_setting_array,
		sizeof(custom4_setting_array)/sizeof(kal_uint16));
}

static void custom5_setting(void)
{
	LOG_INF("E\n");
	table_write_cmos_sensor(
		custom5_setting_array,
		sizeof(custom5_setting_array)/sizeof(kal_uint16));
}

static kal_uint32 return_sensor_id(void)
{
	return ((read_cmos_sensor_8(0x0000) << 8) | read_cmos_sensor_8(0x0001));
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
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = return_sensor_id();
			if (*sensor_id == imgsensor_info.sensor_id) {
				LOG_INF(
					"i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);

				return ERROR_NONE;
			}
			LOG_INF("Read sensor id fail, id: 0x%x\n",
				imgsensor.i2c_write_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		/*if Sensor ID is not correct,
		 *Must set *sensor_id to 0xFFFFFFFF
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
static kal_uint32 open(void)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint32 sensor_id = 0;

	/*sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 *we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			get_imgsensor_id(&sensor_id);
			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_INF(
					"i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, sensor_id);
				break;
			}
			LOG_INF("Read sensor id fail, id: 0x%x\n",
				imgsensor.i2c_write_id);
			retry--;
		} while (retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 2;
	}
	if (imgsensor_info.sensor_id != sensor_id)
		return ERROR_SENSOR_CONNECT_FAIL;

	/* initail sequence write in  */
	sensor_init();

	spin_lock(&imgsensor_drv_lock);

	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.shutter = 0x3D0;
	imgsensor.gain = 0x100;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_mode = 0;
	imgsensor.test_pattern = 0;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
} /* open */

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
static kal_uint32 close(void)
{
	LOG_INF("E\n");

	/*No Need to implement this function*/
	streaming_control(0);
	return ERROR_NONE;
} /* close */

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
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("%s E\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	set_mirror_flip(imgsensor.mirror);

	preview_setting();

	return ERROR_NONE;
} /* preview */

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
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;


	if (imgsensor.current_fps != imgsensor_info.cap.max_framerate) {
		LOG_INF(
			"Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
			imgsensor.current_fps,
			imgsensor_info.cap.max_framerate / 10);

		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;

	} else {
		/* PIP capture:
		 *	24fps for less than 13M,
		 *	20fps for 16M,
		 *	15fps for 20M
		 */
		LOG_INF(
			"Warning:=== current_fps %d fps is not support, so use cap1's setting\n",
			imgsensor.current_fps / 10);

		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);

	capture_setting(imgsensor.current_fps);

	/* set_mirror_flip(imgsensor.mirror); */

	return ERROR_NONE;
}	/* capture() */

static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			       MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting(imgsensor.current_fps);
	/*set_mirror_flip(imgsensor.mirror);*/

	return ERROR_NONE;
}	/*	normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	/*imgsensor.video_mode = KAL_TRUE;*/
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/*imgsensor.current_fps = 300;*/
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	/*set_mirror_flip(imgsensor.mirror);*/

	return ERROR_NONE;
}	/*	hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	/*imgsensor.video_mode = KAL_TRUE;*/
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/*imgsensor.current_fps = 300;*/
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	/*set_mirror_flip(imgsensor.mirror);*/

	return ERROR_NONE;
}	/* slim_video */

static kal_uint32 custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("%s.\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk = imgsensor_info.custom1.pclk;
	imgsensor.line_length = imgsensor_info.custom1.linelength;
	imgsensor.frame_length = imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom1_setting();

	return ERROR_NONE;
}	/* custom1 */

static kal_uint32 custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("%s.\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM2;
	imgsensor.pclk = imgsensor_info.custom2.pclk;
	imgsensor.line_length = imgsensor_info.custom2.linelength;
	imgsensor.frame_length = imgsensor_info.custom2.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom2_setting();

	return ERROR_NONE;
}	/* custom2 */

static kal_uint32 custom3(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("%s.\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM3;
	imgsensor.pclk = imgsensor_info.custom3.pclk;
	imgsensor.line_length = imgsensor_info.custom3.linelength;
	imgsensor.frame_length = imgsensor_info.custom3.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom3.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom3_setting();

	return ERROR_NONE;
}	/* custom1 */

static kal_uint32 custom4(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("%s.\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM4;
	imgsensor.pclk = imgsensor_info.custom4.pclk;
	imgsensor.line_length = imgsensor_info.custom4.linelength;
	imgsensor.frame_length = imgsensor_info.custom4.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom4.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom4_setting();

	return ERROR_NONE;
}

static kal_uint32 custom5(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("%s.\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM5;
	imgsensor.pclk = imgsensor_info.custom5.pclk;
	imgsensor.line_length = imgsensor_info.custom5.linelength;
	imgsensor.frame_length = imgsensor_info.custom5.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom5.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom5_setting();

	return ERROR_NONE;
}

static kal_uint32 get_resolution(
			MSDK_SENSOR_RESOLUTION_INFO_STRUCT * sensor_resolution)
{
	LOG_INF("E\n");
	sensor_resolution->SensorFullWidth =
		imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight =
		imgsensor_info.cap.grabwindow_height;

	sensor_resolution->SensorPreviewWidth =
		imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight =
		imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorVideoWidth =
		imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight =
		imgsensor_info.normal_video.grabwindow_height;

	sensor_resolution->SensorHighSpeedVideoWidth =
		imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight =
		imgsensor_info.hs_video.grabwindow_height;

	sensor_resolution->SensorSlimVideoWidth =
		imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight =
		imgsensor_info.slim_video.grabwindow_height;

	sensor_resolution->SensorCustom1Width =
		imgsensor_info.custom1.grabwindow_width;
	sensor_resolution->SensorCustom1Height =
		imgsensor_info.custom1.grabwindow_height;

	sensor_resolution->SensorCustom2Width =
		imgsensor_info.custom2.grabwindow_width;
	sensor_resolution->SensorCustom2Height =
		imgsensor_info.custom2.grabwindow_height;

	sensor_resolution->SensorCustom3Width =
		imgsensor_info.custom3.grabwindow_width;
	sensor_resolution->SensorCustom3Height =
		imgsensor_info.custom3.grabwindow_height;

	sensor_resolution->SensorCustom4Width =
		imgsensor_info.custom4.grabwindow_width;
	sensor_resolution->SensorCustom4Height =
		imgsensor_info.custom4.grabwindow_height;

	sensor_resolution->SensorCustom5Width =
		imgsensor_info.custom5.grabwindow_width;
	sensor_resolution->SensorCustom5Height =
		imgsensor_info.custom5.grabwindow_height;

	return ERROR_NONE;
} /* get_resolution */

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			   MSDK_SENSOR_INFO_STRUCT *sensor_info,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 5; /* not use */

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
	sensor_info->SensorOutputDataFormat =
		imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame =
		imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame =
		imgsensor_info.slim_video_delay_frame;
	sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;
	sensor_info->Custom2DelayFrame = imgsensor_info.custom2_delay_frame;
	sensor_info->Custom3DelayFrame = imgsensor_info.custom3_delay_frame;
	sensor_info->Custom4DelayFrame = imgsensor_info.custom4_delay_frame;
	sensor_info->Custom5DelayFrame = imgsensor_info.custom5_delay_frame;


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
	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0; /* 0 is default 1x */
	sensor_info->SensorHightSampling = 0; /* 0 is default 1x */
	sensor_info->SensorPacketECCOrder = 1;

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.cap.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:

		sensor_info->SensorGrabStartX =
			imgsensor_info.normal_video.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.normal_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		sensor_info->SensorGrabStartX =
			imgsensor_info.slim_video.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.slim_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;

		break;

	case MSDK_SCENARIO_ID_CUSTOM1:
		sensor_info->SensorGrabStartX = imgsensor_info.custom1.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom1.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;
		break;

	case MSDK_SCENARIO_ID_CUSTOM2:
		sensor_info->SensorGrabStartX = imgsensor_info.custom2.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom2.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom2.mipi_data_lp2hs_settle_dc;
		break;

	case MSDK_SCENARIO_ID_CUSTOM3:
		sensor_info->SensorGrabStartX = imgsensor_info.custom3.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom3.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom3.mipi_data_lp2hs_settle_dc;
		break;

	case MSDK_SCENARIO_ID_CUSTOM4:
		sensor_info->SensorGrabStartX = imgsensor_info.custom4.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom4.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom4.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		sensor_info->SensorGrabStartX = imgsensor_info.custom5.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom5.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom5.mipi_data_lp2hs_settle_dc;
		break;

	default:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	}

	return ERROR_NONE;
}	/*	get_info  */

static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			  MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);

	/* streamoff should be finished before mode changes */
	check_streamoff();

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		preview(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		capture(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		normal_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		hs_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		slim_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		custom1(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		custom2(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		custom3(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		custom4(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		custom5(image_window, sensor_config_data);
		break;
	default:
		LOG_INF("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}	/* control() */

static kal_uint32 set_video_mode(UINT16 framerate)
{
	LOG_INF("framerate = %d\n ", framerate);
	/* SetVideoMode Function should fix framerate */
	if (framerate == 0)
		return ERROR_NONE;
	spin_lock(&imgsensor_drv_lock);
	if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 296;
	else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 146;
	else
		imgsensor.current_fps = framerate;
	spin_unlock(&imgsensor_drv_lock);
	set_max_framerate(imgsensor.current_fps, 1);

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	LOG_INF("enable = %d, framerate = %d\n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable) /*enable auto flicker*/
		imgsensor.autoflicker_en = KAL_TRUE;
	else /*Cancel Auto flick*/
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(
		enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frame_length = imgsensor_info.pre.pclk / framerate * 10
			/ imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);

		imgsensor.dummy_line =
			(frame_length > imgsensor_info.pre.framelength)
			? (frame_length - imgsensor_info.pre.framelength)
			: 0;

		imgsensor.frame_length =
			imgsensor_info.pre.framelength
			+ imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();

		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;

		frame_length
			= imgsensor_info.normal_video.pclk
			/ framerate * 10
			/ imgsensor_info.normal_video.linelength;

		spin_lock(&imgsensor_drv_lock);

		imgsensor.dummy_line
			= (frame_length
			   > imgsensor_info.normal_video.framelength)
			? (frame_length
			   - imgsensor_info.normal_video.framelength)
			: 0;

		imgsensor.frame_length =
			imgsensor_info.normal_video.framelength
			+ imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();

		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate) {
			LOG_INF(
				"Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
				framerate,
				imgsensor_info.cap.max_framerate/10);

			frame_length
				= imgsensor_info.cap.pclk
				/ framerate * 10
				/ imgsensor_info.cap.linelength;

			spin_lock(&imgsensor_drv_lock);

			imgsensor.dummy_line
				= (frame_length
				   > imgsensor_info.cap.framelength)
				? (frame_length
				   - imgsensor_info.cap.framelength)
				: 0;

			imgsensor.frame_length
				= imgsensor_info.cap.framelength
				+ imgsensor.dummy_line;

			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		}

		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();

		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length
			= imgsensor_info.hs_video.pclk
			/ framerate * 10
			/ imgsensor_info.hs_video.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line
			= (frame_length > imgsensor_info.hs_video.framelength)
			? (frame_length - imgsensor_info.hs_video.framelength)
			: 0;

		imgsensor.frame_length
			= imgsensor_info.hs_video.framelength
			+ imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();

		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length
			= imgsensor_info.slim_video.pclk
			/ framerate * 10
			/ imgsensor_info.slim_video.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line
			= (frame_length > imgsensor_info.slim_video.framelength)
			? (frame_length - imgsensor_info.slim_video.framelength)
			: 0;

		imgsensor.frame_length
			= imgsensor_info.slim_video.framelength
			+ imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();

		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		frame_length
			= imgsensor_info.custom1.pclk
			/ framerate * 10
			/ imgsensor_info.custom1.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line
			= (frame_length > imgsensor_info.custom1.framelength)
			? (frame_length - imgsensor_info.custom1.framelength)
			: 0;

		imgsensor.frame_length
			= imgsensor_info.custom1.framelength
			+ imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();

		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		frame_length
			= imgsensor_info.custom2.pclk
			/ framerate * 10
			/ imgsensor_info.custom2.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line
			= (frame_length > imgsensor_info.custom2.framelength)
			? (frame_length - imgsensor_info.custom2.framelength)
			: 0;

		imgsensor.frame_length
			= imgsensor_info.custom2.framelength
			+ imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();

		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		frame_length
			= imgsensor_info.custom3.pclk
			/ framerate * 10
			/ imgsensor_info.custom3.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line
			= (frame_length > imgsensor_info.custom3.framelength)
			? (frame_length - imgsensor_info.custom3.framelength)
			: 0;

		imgsensor.frame_length =
			imgsensor_info.custom3.framelength
			+ imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();

		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		frame_length
			= imgsensor_info.custom4.pclk
			/ framerate * 10
			/ imgsensor_info.custom4.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line
			= (frame_length > imgsensor_info.custom4.framelength)
			? (frame_length - imgsensor_info.custom4.framelength)
			: 0;

		imgsensor.frame_length
			= imgsensor_info.custom4.framelength
			+ imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();

		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		frame_length
			= imgsensor_info.custom5.pclk
			/ framerate * 10
			/ imgsensor_info.custom5.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line
			= (frame_length > imgsensor_info.custom5.framelength)
			? (frame_length - imgsensor_info.custom5.framelength)
			: 0;

		imgsensor.frame_length =
			imgsensor_info.custom5.framelength
			+ imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();

		break;
	default:  /*coding with  preview scenario by default*/
		frame_length
			= imgsensor_info.pre.pclk
			/ framerate * 10
			/ imgsensor_info.pre.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line
			= (frame_length > imgsensor_info.pre.framelength)
			? (frame_length - imgsensor_info.pre.framelength)
			: 0;

		imgsensor.frame_length
			= imgsensor_info.pre.framelength
			+ imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();

		LOG_INF("error scenario_id = %d, we use preview scenario\n",
			scenario_id);

		break;
	}
	return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(
		    enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		*framerate = imgsensor_info.pre.max_framerate;
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		*framerate = imgsensor_info.normal_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		*framerate = imgsensor_info.cap.max_framerate;
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		*framerate = imgsensor_info.hs_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		*framerate = imgsensor_info.slim_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		*framerate = imgsensor_info.custom1.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		*framerate = imgsensor_info.custom2.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		*framerate = imgsensor_info.custom3.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		*framerate = imgsensor_info.custom4.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		*framerate = imgsensor_info.custom5.max_framerate;
		break;
	default:
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
				  UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *) feature_para;
	/* unsigned long long *feature_return_para
	 *  = (unsigned long long *) feature_para;
	 */
	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	struct SENSOR_VC_INFO_STRUCT *pvcinfo;
	/* SET_SENSOR_AWB_GAIN *pSetSensorAWB
	 *  = (SET_SENSOR_AWB_GAIN *)feature_para;
	 */
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data
		= (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	/*LOG_INF("feature_id = %d\n", feature_id);*/
	switch (feature_id) {
	case SENSOR_FEATURE_GET_GAIN_RANGE_BY_SCENARIO:
		*(feature_data + 1) = imgsensor_info.min_gain;
		*(feature_data + 2) = imgsensor_info.max_gain;
		break;
	case SENSOR_FEATURE_GET_BASE_GAIN_ISO_AND_STEP:
		*(feature_data + 0) = imgsensor_info.min_gain_iso;
		*(feature_data + 1) = imgsensor_info.gain_step;
		*(feature_data + 2) = imgsensor_info.gain_type;
		break;
	case SENSOR_FEATURE_GET_MIN_SHUTTER_BY_SCENARIO:
		*(feature_data + 1) = imgsensor_info.min_shutter;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.cap.pclk;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.normal_video.pclk;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.hs_video.pclk;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.slim_video.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom1.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom2.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom3.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom4.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom5.pclk;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.pclk;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= (imgsensor_info.cap.framelength << 16)
				+ imgsensor_info.cap.linelength;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= (imgsensor_info.normal_video.framelength
				   << 16)
				+ imgsensor_info.normal_video.linelength;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= (imgsensor_info.hs_video.framelength << 16)
				+ imgsensor_info.hs_video.linelength;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= (imgsensor_info.slim_video.framelength << 16)
				+ imgsensor_info.slim_video.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= (imgsensor_info.custom1.framelength << 16)
				+ imgsensor_info.custom1.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= (imgsensor_info.custom2.framelength << 16)
				+ imgsensor_info.custom2.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= (imgsensor_info.custom3.framelength << 16)
				+ imgsensor_info.custom3.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= (imgsensor_info.custom4.framelength << 16)
				+ imgsensor_info.custom4.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= (imgsensor_info.custom5.framelength << 16)
				+ imgsensor_info.custom5.linelength;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= (imgsensor_info.pre.framelength << 16)
				+ imgsensor_info.pre.linelength;
			break;
		}
		break;
#if defined(IMGSENSOR_MT6885)
	case SENSOR_FEATURE_GET_OFFSET_TO_START_OF_EXPOSURE:
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1000000;
		break;
#elif defined(IMGSENSOR_MT6877)
	case SENSOR_FEATURE_GET_OFFSET_TO_START_OF_EXPOSURE:
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1500000;
		break;
#endif
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16 = imgsensor.frame_length;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		/* night_mode((BOOL) *feature_data); */
		break;
#ifdef VENDOR_EDIT
	case SENSOR_FEATURE_CHECK_MODULE_ID:
		*feature_return_para_32 = imgsensor_info.module_id;
		break;
#endif
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16) *feature_data);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		write_cmos_sensor(sensor_reg_data->RegAddr,
				  sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData =
			read_cmos_sensor(sensor_reg_data->RegAddr);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		/*get the lens driver ID from EEPROM
		 * or just return LENS_DRIVER_ID_DO_NOT_CARE
		 * if EEPROM does not exist in camera module.
		 */
		*feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		set_video_mode(*feature_data);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		get_imgsensor_id(feature_return_para_32);
		break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		set_auto_flicker_mode((BOOL)*feature_data_16,
				      *(feature_data_16+1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario(
				(enum MSDK_SCENARIO_ID_ENUM)*feature_data,
				*(feature_data+1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario(
				(enum MSDK_SCENARIO_ID_ENUM)*(feature_data),
				(MUINT32 *)(uintptr_t)(*(feature_data+1)));
		break;
	case SENSOR_FEATURE_GET_PDAF_DATA:
		LOG_INF("SENSOR_FEATURE_GET_PDAF_DATA\n");
		#if 0
		read_3P8_eeprom((kal_uint16)(*feature_data),
				(char *)(uintptr_t)(*(feature_data+1)),
				(kal_uint32)(*(feature_data+2)));
		#endif
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((UINT32)*feature_data,
		(struct SET_SENSOR_PATTERN_SOLID_COLOR *)(uintptr_t)(*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		/* for factory mode auto testing */
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		LOG_INF("current fps :%d\n", (UINT32)*feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = *feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		LOG_INF("ihdr enable :%d\n", (BOOL)*feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_mode = *feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
	#if 0
		LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
			(UINT32)*feature_data);
	#endif
		wininfo =
			(struct SENSOR_WINSIZE_INFO_STRUCT *)
				(uintptr_t)(*(feature_data+1));

		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)wininfo,
			       (void *)&imgsensor_winsize_info[1],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)wininfo,
			       (void *)&imgsensor_winsize_info[2],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy((void *)wininfo,
			       (void *)&imgsensor_winsize_info[3],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy((void *)wininfo,
			       (void *)&imgsensor_winsize_info[4],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			memcpy((void *)wininfo,
			       (void *)&imgsensor_winsize_info[5],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			memcpy((void *)wininfo,
			       (void *)&imgsensor_winsize_info[6],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			memcpy((void *)wininfo,
			       (void *)&imgsensor_winsize_info[7],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			memcpy((void *)wininfo,
			       (void *)&imgsensor_winsize_info[8],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			memcpy((void *)wininfo,
			       (void *)&imgsensor_winsize_info[9],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo,
			       (void *)&imgsensor_winsize_info[0],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PDAF_INFO:
		LOG_INF("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%d\n",
			(UINT16) *feature_data);

		PDAFinfo =
			(struct SET_PD_BLOCK_INFO_T *)
				(uintptr_t)(*(feature_data+1));

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			#if 0
			memcpy((void *)PDAFinfo,
				(void *)&imgsensor_pd_info,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			#endif
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			#if 0
			memcpy((void *)PDAFinfo,
				(void *)&imgsensor_pd_info_16_9,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			#endif
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		default:
			break;
		}
		break;
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		LOG_INF(
			"SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%d\n",
			(UINT16) *feature_data);

		/*PDAF capacity enable or not, 2p8 only full size support PDAF*/
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			/* video & capture use same setting */
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		*feature_return_para_32 = get_sensor_temperature();
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PDAF_REG_SETTING:
		LOG_INF("SENSOR_FEATURE_GET_PDAF_REG_SETTING %d",
			(*feature_para_len));

		break;
	case SENSOR_FEATURE_SET_PDAF_REG_SETTING:
		/*LOG_INF("SENSOR_FEATURE_SET_PDAF_REG_SETTING %d",
		 *	(*feature_para_len));
		 */

		break;
	case SENSOR_FEATURE_SET_PDAF:
		LOG_INF("PDAF mode :%d\n", *feature_data_16);
		imgsensor.pdaf_mode = *feature_data_16;
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16)*feature_data,
			(UINT16)*(feature_data+1),
			(UINT16)*(feature_data+2));
		break;
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length((UINT16) (*feature_data),
					 (UINT16) (*(feature_data + 1)),
					 (BOOL) (*(feature_data + 2)));
		break;
	case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
		/*
		 * 1, if driver support new sw frame sync
		 * set_shutter_frame_length() support third para auto_extend_en
		 */
		*(feature_data + 1) = 1;
		/* margin info by scenario */
		*(feature_data + 2) = imgsensor_info.margin;
		break;
	case SENSOR_FEATURE_SET_AWB_GAIN:
		break;
	case SENSOR_FEATURE_SET_HDR_SHUTTER:
		LOG_INF("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
			(UINT16)*feature_data, (UINT16)*(feature_data+1));
		#if 0
		ihdr_write_shutter((UINT16)*feature_data,
				   (UINT16)*(feature_data+1));
		#endif
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		LOG_INF("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		LOG_INF("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
			*feature_data);
		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(KAL_TRUE);
		break;
	case SENSOR_FEATURE_GET_BINNING_TYPE:
		switch (*(feature_data + 1)) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CUSTOM1:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_CUSTOM2:
		case MSDK_SCENARIO_ID_CUSTOM3:
		case MSDK_SCENARIO_ID_CUSTOM4:
		case MSDK_SCENARIO_ID_CUSTOM5:
		default:
			*feature_return_para_32 = 1; /*BINNING_AVERAGED*/
			break;
		}
		pr_debug("SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",
			 *feature_return_para_32);
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_AE_EFFECTIVE_FRAME_FOR_LE:
		*feature_return_para_32 = imgsensor.current_ae_effective_frame;
		break;
	case SENSOR_FEATURE_GET_AE_FRAME_MODE_FOR_LE:
		memcpy(feature_return_para_32,
		       &imgsensor.ae_frm_mode,
		       sizeof(struct IMGSENSOR_AE_FRM_MODE));
		break;
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.cap.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.normal_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.hs_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom1.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom2.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom3.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom4.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom5.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_VC_INFO:
	#if 0
		LOG_INF("SENSOR_FEATURE_GET_VC_INFO %d\n",
			(UINT16)*feature_data);
	#endif
		pvcinfo =
			(struct SENSOR_VC_INFO_STRUCT *)
				(uintptr_t)(*(feature_data+1));

		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			memcpy((void *)pvcinfo, (void *)&vc_info_preview,
			       sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)pvcinfo, (void *)&vc_info_capture,
			       sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)pvcinfo, (void *)&vc_info_video,
			       sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		default:
			#if 0
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[0],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			#endif
			break;
		}
	default:
		break;
	}

	return ERROR_NONE;
} /* feature_control() */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 S5K3M5SX_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	sensor_func.arch = IMGSENSOR_ARCH_V2;
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	if (imgsensor.psensor_func == NULL)
		imgsensor.psensor_func = &sensor_func;
	return ERROR_NONE;
}
