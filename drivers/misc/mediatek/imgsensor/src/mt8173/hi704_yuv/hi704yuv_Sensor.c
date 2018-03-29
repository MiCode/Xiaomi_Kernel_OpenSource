/*
 * Copyright (C) 2015 MediaTek Inc.
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

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <linux/types.h>
#include <linux/types.h>

/* #include <mach/mt6516_pll.h> */
#include "kd_camera_typedef.h"
/* #include "kd_camera_hw.h" */
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "kd_camera_feature.h"

#include "hi704yuv_Sensor.h"
#include "hi704yuv_Camera_Sensor_para.h"
#include "hi704yuv_CameraCustomized.h"

#define HI704YUV_DEBUG
#ifdef HI704YUV_DEBUG
#define SENSORDB pr_debug
#else
#define SENSORDB pr_err
#endif


static DEFINE_SPINLOCK(hi704_yuv_drv_lock);

kal_uint16 HI704_write_cmos_sensor(kal_uint8 addr, kal_uint8 para)
{
	char puSendCmd[2] = { (char)(addr & 0xFF), (char)(para & 0xFF) };

	iWriteRegI2C(puSendCmd, 2, HI704_WRITE_ID);
	return 0;
}

kal_uint16 HI704_read_cmos_sensor(kal_uint8 addr)
{
	kal_uint16 get_byte = 0;
	char puSendCmd = { (char)(addr & 0xFF) };

	iReadRegI2C(&puSendCmd, 1, (u8 *) &get_byte, 1, HI704_WRITE_ID);
	return get_byte;
}

static void HI704_Set_VGA_mode(void);


/*******************************************************************************
* // Adapter for Winmo typedef
********************************************************************************/
#define WINMO_USE 0

#define Sleep(ms) mdelay(ms)
#define RETAILMSG(x, ...)
#define TEXT


/*******************************************************************************
* follow is define by jun
// color saturation c0->9a
// AWB Rgain Max 52->55
// positive edge 1320 ->03_1321 03->01;1390 ->02_1391 04->01
// Y taarget 4C->30
// change gamma
// GMax 60->70
********************************************************************************/
MSDK_SENSOR_CONFIG_STRUCT HI704SensorConfigData;

static struct HI704_sensor_STRUCT HI704_sensor = {
	.output_format = SENSOR_OUTPUT_FORMAT_YUYV
};

static kal_uint32 HI704_zoom_factor;
static int sensor_id_fail;
const HI704_SENSOR_INIT_INFO HI704_Initial_Setting_Info[] = {
	/* PAGE 0 */
	/* Image Size/Windowing/HSYNC/VSYNC[Type1] */
	{0x03, 0x00},		/* PAGEMODE(0x03) */
	{0x01, 0xf1},
	{0x01, 0xf3},		/* PWRCTL(0x01[P0])Bit[1]:Software Reset. */
	{0x01, 0xf1},

	{0x11, 0x90},		/* For No Fixed Framerate Bit[2] */
	{0x12, 0x04},		/* PCLK INV */

	{0x20, 0x00},
	{0x21, 0x04},		/* Window start X */
	{0x22, 0x00},
	{0x23, 0x04},		/* Window start Y */

	{0x24, 0x01},
	{0x25, 0xe8},		/* Window height : 0x1e8 = 488 */
	{0x26, 0x02},
	{0x27, 0x84},		/* Window width  : 0x284 = 644 */


	{0x40, 0x00},		/* HBLANK: 0x70 = 112 */
	{0x41, 0x70},
	{0x42, 0x00},		/* VBLANK: 0x40 = 64 */
	{0x43, 0x15},		/* 0x04 -> 0x40: For Max Framerate = 30fps */

	/* BLC */
	{0x80, 0x2e},
	{0x81, 0x7e},
	{0x82, 0x90},
	{0x83, 0x30},
	{0x84, 0x2c},
	{0x85, 0x4b},
	{0x88, 0x87},
	{0x89, 0x48},

	{0x90, 0x10},
	{0x91, 0x10},
	{0x92, 0x48},
	{0x93, 0x40},
	{0x98, 0x38},
	{0x99, 0x40},
	{0xa0, 0x00},
	{0xa8, 0x40},

	/* PAGE 2 */
	/* Analog Circuit */
	{0x03, 0x02},
	{0x13, 0x40},
	{0x14, 0x04},
	{0x18, 0x1c},
	{0x19, 0x00},
	{0x1a, 0x00},
	{0x1b, 0x08},
	{0x20, 0x33},
	{0x21, 0xaa},
	{0x22, 0xa7},
	{0x23, 0x30},
	{0x24, 0x4a},
	{0x28, 0x0c},
	{0x29, 0x80},
	{0x31, 0x99},
	{0x32, 0x00},
	{0x33, 0x00},
	{0x34, 0x3c},
	{0x35, 0x01},
	{0x3b, 0x48},
	{0x50, 0x21},
	{0x52, 0xa2},
	{0x53, 0x0a},
	{0x54, 0x30},
	{0x55, 0x10},
	{0x56, 0x08},		/* make for sync Margin */
	{0x59, 0x0F},
	{0x60, 0x63},		/* 101120 */
	{0x61, 0x70},
	{0x62, 0x64},
	{0x63, 0x6e},
	{0x64, 0x64},
	{0x65, 0x6e},
	{0x72, 0x65},
	{0x73, 0x6d},
	{0x74, 0x65},
	{0x75, 0x6d},
	{0x80, 0x02},
	{0x81, 0x57},
	{0x82, 0x07},
	{0x83, 0x14},
	{0x84, 0x07},
	{0x85, 0x14},
	{0x92, 0x2c},
	{0x93, 0x3c},
	{0x94, 0x2c},
	{0x95, 0x3c},
	{0xa0, 0x03},
	{0xa1, 0x55},
	{0xa4, 0x55},
	{0xa5, 0x03},
	{0xa8, 0x18},
	{0xa9, 0x28},
	{0xaa, 0x40},
	{0xab, 0x50},
	{0xac, 0x10},
	{0xad, 0x0e},
	{0xb8, 0x65},
	{0xb9, 0x69},
	{0xbc, 0x05},
	{0xbd, 0x09},
	{0xc0, 0x6f},
	{0xc1, 0x77},
	{0xc2, 0x6f},
	{0xc3, 0x77},
	{0xc4, 0x70},
	{0xc5, 0x76},
	{0xc6, 0x70},
	{0xc7, 0x76},
	{0xc8, 0x71},
	{0xc9, 0x75},
	{0xcc, 0x72},
	{0xcd, 0x74},
	{0xca, 0x71},
	{0xcb, 0x75},
	{0xce, 0x72},
	{0xcf, 0x74},
	{0xd0, 0x63},
	{0xd1, 0x70},

	/* PAGE 10 */
	/* Image Format, Image Effect */
	{0x03, 0x10},
	{0x10, 0x03},
	{0x11, 0x43},
	{0x12, 0x30},

	{0x40, 0x00},
	{0x41, 0xd0},
	{0x48, 0x84},

	{0x50, 0x60},

	{0x60, 0x7f},
	{0x61, 0x00},
	{0x62, 0x9a},
	{0x63, 0x9a},
	{0x64, 0x48},
	{0x66, 0x90},
	{0x67, 0xf6},

	/* PAGE 11 */
	/* Z-LPF */
	{0x03, 0x11},
	{0x10, 0x21},
	{0x11, 0x1f},

	{0x20, 0x00},
	{0x21, 0x50},
	{0x23, 0x0a},

	{0x60, 0x10},
	{0x61, 0x82},
	{0x62, 0x00},
	{0x63, 0x80},
	{0x64, 0x83},
	{0x67, 0xF0},
	{0x68, 0x80},
	{0x69, 0x10},

	/* PAGE 12 */
	/* 2D */
	{0x03, 0x12},

	{0x40, 0xeB},
	{0x41, 0x19},

	{0x50, 0x18},
	{0x51, 0x24},

	{0x70, 0x3f},
	{0x71, 0x00},
	{0x72, 0x00},
	{0x73, 0x00},
	{0x74, 0x10},
	{0x75, 0x10},
	{0x76, 0x20},
	{0x77, 0x80},
	{0x78, 0x88},
	{0x79, 0x18},
	{0x90, 0x3d},
	{0x91, 0x34},
	{0x99, 0x28},
	{0x9c, 0x14},
	{0x9d, 0x15},
	{0x9e, 0x28},
	{0x9f, 0x28},
	{0xb0, 0x7d},
	{0xb5, 0x44},
	{0xb6, 0x82},
	{0xb7, 0x52},
	{0xb8, 0x44},
	{0xb9, 0x15},
	{0xb0, 0x7d},
	{0xb5, 0x44},
	{0xb6, 0x82},
	{0xb7, 0x52},
	{0xb8, 0x44},
	{0xb9, 0x15},

	/* PAGE 13 */
	/* Edge Enhancement */
	{0x03, 0x13},
	{0x10, 0x01},
	{0x11, 0x81},
	{0x12, 0x14},
	{0x13, 0x19},
	{0x14, 0x08},

	{0x20, 0x03},
	{0x21, 0x01},
	{0x23, 0x30},
	{0x24, 0x33},
	{0x25, 0x08},
	{0x26, 0x18},
	{0x27, 0x00},
	{0x28, 0x08},
	{0x29, 0x50},
	{0x2a, 0xe0},
	{0x2b, 0x10},
	{0x2c, 0x28},
	{0x2d, 0x40},
	{0x2e, 0x00},
	{0x2f, 0x00},

	/* PAGE 11 */
	{0x30, 0x11},

	{0x80, 0x03},
	{0x81, 0x07},

	{0x90, 0x02},
	{0x91, 0x01},
	{0x92, 0x00},
	{0x93, 0x20},
	{0x94, 0x41},
	{0x95, 0x60},

	/* PAGE 14 */
	/* Lens Shading Correction */
	{0x03, 0x14},
	{0x10, 0x01},

	{0x20, 0x80},		/* For Y decay */
	{0x21, 0x80},		/* For Y decay */
	{0x22, 0xa0},
	{0x23, 0x5d},
	{0x24, 0x4e},

	/* PAGE 15 */
	/* Color Correction */
	{0x03, 0x15},
	{0x10, 0x0f},
	{0x14, 0x36},
	{0x16, 0x28},
	{0x17, 0x2f},

	{0x30, 0x79},
	{0x31, 0x39},
	{0x32, 0x00},
	{0x33, 0x11},
	{0x34, 0x65},
	{0x35, 0x14},
	{0x36, 0x01},
	{0x37, 0x33},
	{0x38, 0x74},

	{0x40, 0x00},
	{0x41, 0x00},
	{0x42, 0x00},
	{0x43, 0x8b},
	{0x44, 0x07},
	{0x45, 0x04},
	{0x46, 0x84},
	{0x47, 0xa1},
	{0x48, 0x25},

	/* PAGE 16 */
	/* Gamma Correction */
	{0x03, 0x16},

	{0x30, 0x00},
	{0x31, 0x1c},
	{0x32, 0x2c},
	{0x33, 0x3f},
	{0x34, 0x56},
	{0x35, 0x69},
	{0x36, 0x7e},
	{0x37, 0x8c},
	{0x38, 0x99},
	{0x39, 0xa4},
	{0x3a, 0xae},
	{0x3b, 0xbf},
	{0x3c, 0xd0},
	{0x3d, 0xe6},
	{0x3e, 0xff},


	/* PAGE 17 */
	/* Auto Flicker Cancellation */
	{0x03, 0x17},

	{0xc0, 0x01},
	{0xc4, 0x4E},
	{0xc5, 0x41},
	/* PAGE 20 */
	/* AE */
	{0x03, 0x20},

	{0x10, 0x0c},
	{0x11, 0x04},

	{0x20, 0x01},
	{0x28, 0x27},
	{0x29, 0xa1},
	{0x2a, 0xf0},
	{0x2b, 0x34},
	{0x2c, 0x2b},

	{0x30, 0x78},
	{0x39, 0x22},
	{0x3a, 0xde},
	{0x3b, 0x22},
	{0x3c, 0xde},

	{0x60, 0x95},
	{0x68, 0x20},
	{0x69, 0x80},
	{0x6A, 0x20},
	{0x6B, 0xc8},

	{0x70, 0x30},		/* For Y decay */
	{0x76, 0x22},
	{0x77, 0x02},
	{0x78, 0x12},
	{0x79, 0x25},
	{0x7a, 0x23},
	{0x7c, 0x1d},
	{0x7d, 0x22},

	{0x83, 0x00},		/* expTime:0x83,0x84,0x85 */
	{0x84, 0xc3},
	{0x85, 0x00},

	{0x86, 0x00},		/* expMin is minimum time of expTime, */
	{0x87, 0xc0},

	{0x88, 0x03},
	{0x89, 0x0c},
	{0x8a, 0x00},

	{0x8b, 0x3a},
	{0x8c, 0x80},

	{0x8d, 0x30},
	{0x8e, 0xc0},

	{0x91, 0x02},
	{0x92, 0xdc},
	{0x93, 0x6c},
	{0x94, 0x01},
	{0x95, 0xb7},
	{0x96, 0x74},
	{0x98, 0x8C},
	{0x99, 0x23},
	{0x9c, 0x09},		/* EXP Limit 976.56 fps */
	{0x9d, 0xc0},
	{0x9e, 0x00},		/* EXP Unit */
	{0x9f, 0xc0},
	{0xb0, 0x16},		/* 20131212 (b0~bd) */
	{0xb1, 0x16},		/* Analog gain min:1x */
	{0xb2, 0x70},		/* Analog gain max:3.5x */
	{0xb3, 0x16},
	{0xb4, 0x16},
	{0xb5, 0x3C},
	{0xb6, 0x29},
	{0xb7, 0x23},
	{0xb8, 0x1f},
	{0xb9, 0x1d},
	{0xba, 0x1c},
	{0xbb, 0x1b},
	{0xbc, 0x1a},
	{0xbd, 0x1a},
	{0xc0, 0x1a},
	{0xc3, 0x58},		/* 48->58 20131212 */
	{0xc4, 0x58},		/* 48->58 20131212 */



	/* PAGE 22 */
	/* AWB */
	{0x03, 0x22},
	{0x10, 0xfb},		/* fb}, */
	{0x11, 0x26},
	{0x30, 0x80},
	{0x31, 0x80},
	{0x38, 0x12},
	{0x39, 0x33},
	{0x3a, 0x88},
	{0x3b, 0xc4},
	{0x40, 0xf0},
	{0x41, 0x33},
	{0x42, 0x33},
	{0x43, 0xf3},
	{0x44, 0x55},
	{0x45, 0x44},
	{0x46, 0x00},
	{0x60, 0x00},
	{0x61, 0x00},
	{0x80, 0x1b},
	{0x81, 0x20},
	{0x82, 0x50},
	{0x83, 0x55},
	{0x84, 0x1b},
	{0x85, 0x52},
	{0x86, 0x20},
	{0x87, 0x4d},
	{0x88, 0x38},
	{0x89, 0x30},
	{0x8a, 0x20},
	{0x8b, 0x00},
	{0x8d, 0x14},
	{0x8e, 0x41},
	{0x8f, 0x63},
	{0x90, 0x60},
	{0x91, 0x5a},
	{0x92, 0x52},
	{0x93, 0x48},
	{0x94, 0x3d},
	{0x95, 0x35},
	{0x96, 0x31},
	{0x97, 0x2e},
	{0x98, 0x2a},
	{0x99, 0x29},
	{0x9a, 0x26},
	{0x9b, 0x09},
	{0xb0, 0x30},
	{0xb1, 0x48},



	/* PAGE 20 */
	{0x03, 0x20},
	{0x10, 0x9c},

	{0x01, 0xf0},

	/* PAGE 0 */
	{0x03, 0x00},
	{0x01, 0x90},		/* 0xf1 ->0x41 : For Preview Green/Red Line. */
	{0xff, 0xff}		/* End of Initial Setting */

#if 0				/* Old Settings */
	/* PAGE 0 */
	/* Image Size/Windowing/HSYNC/VSYNC[Type1] */
	{0x03, 0x00},		/* PAGEMODE(0x03) */
	{0x01, 0xf1},
	{0x01, 0xf3},		/* PWRCTL(0x01[P0])Bit[1]:Software Reset. */
	{0x01, 0xf1},

	{0x11, 0x90},		/* For No Fixed Framerate Bit[2] */
	{0x12, 0x04},		/* PCLK INV */

	{0x20, 0x00},
	{0x21, 0x04},		/* Window start X */
	{0x22, 0x00},
	{0x23, 0x04},		/* Window start Y */

	{0x24, 0x01},
	{0x25, 0xe8},		/* Window height : 0x1e8 = 488 */
	{0x26, 0x02},
	{0x27, 0x84},		/* Window width  : 0x284 = 644 */


	{0x40, 0x01},		/* HBLANK: 0x70 = 112 */
	{0x41, 0x58},
	{0x42, 0x00},		/* VBLANK: 0x40 = 64 */
	{0x43, 0x13},		/* 0x04 -> 0x40: For Max Framerate = 30fps */

	/* BLC */
	{0x80, 0x2e},
	{0x81, 0x7e},
	{0x82, 0x90},
	{0x83, 0x30},
	{0x84, 0x2c},
	{0x85, 0x4b},
	{0x89, 0x48},

	{0x90, 0x0a},
	{0x91, 0x0a},
	{0x92, 0x48},
	{0x93, 0x40},
	{0x98, 0x38},
	{0x99, 0x40},
	{0xa0, 0x00},
	{0xa8, 0x40},

	/* PAGE 2 */
	/* Analog Circuit */
	{0x03, 0x02},
	{0x13, 0x40},
	{0x14, 0x04},
	{0x1a, 0x00},
	{0x1b, 0x08},

	{0x20, 0x33},
	{0x21, 0xaa},
	{0x22, 0xa7},
	{0x23, 0xb1},		/* For Sun Pot */

	{0x3b, 0x48},

	{0x50, 0x21},
	{0x52, 0xa2},
	{0x53, 0x0a},
	{0x54, 0x30},
	{0x55, 0x10},
	{0x56, 0x0c},
	{0x59, 0x0F},

	{0x60, 0x54},
	{0x61, 0x5d},
	{0x62, 0x56},
	{0x63, 0x5c},
	{0x64, 0x56},
	{0x65, 0x5c},
	{0x72, 0x57},
	{0x73, 0x5b},
	{0x74, 0x57},
	{0x75, 0x5b},
	{0x80, 0x02},
	{0x81, 0x46},
	{0x82, 0x07},
	{0x83, 0x10},
	{0x84, 0x07},
	{0x85, 0x10},
	{0x92, 0x24},
	{0x93, 0x30},
	{0x94, 0x24},
	{0x95, 0x30},
	{0xa0, 0x03},
	{0xa1, 0x45},
	{0xa4, 0x45},
	{0xa5, 0x03},
	{0xa8, 0x12},
	{0xa9, 0x20},
	{0xaa, 0x34},
	{0xab, 0x40},
	{0xb8, 0x55},
	{0xb9, 0x59},
	{0xbc, 0x05},
	{0xbd, 0x09},
	{0xc0, 0x5f},
	{0xc1, 0x67},
	{0xc2, 0x5f},
	{0xc3, 0x67},
	{0xc4, 0x60},
	{0xc5, 0x66},
	{0xc6, 0x60},
	{0xc7, 0x66},
	{0xc8, 0x61},
	{0xc9, 0x65},
	{0xca, 0x61},
	{0xcb, 0x65},
	{0xcc, 0x62},
	{0xcd, 0x64},
	{0xce, 0x62},
	{0xcf, 0x64},
	{0xd0, 0x53},
	{0xd1, 0x68},

	/* PAGE 10 */
	/* Image Format, Image Effect */
	{0x03, 0x10},
	{0x10, 0x03},
	{0x11, 0x43},
	{0x12, 0x30},

	{0x40, 0x80},
	{0x41, 0x02},
	{0x48, 0x98},

	{0x50, 0x48},

	{0x60, 0x7f},
	{0x61, 0x00},
	{0x62, 0xb0},
	{0x63, 0xa8},
	{0x64, 0x48},
	{0x66, 0x90},
	{0x67, 0x42},

	/* PAGE 11 */
	/* Z-LPF */
	{0x03, 0x11},
	{0x10, 0x25},
	{0x11, 0x1f},

	{0x20, 0x00},
	{0x21, 0x38},
	{0x23, 0x0a},

	{0x60, 0x10},
	{0x61, 0x82},
	{0x62, 0x00},
	{0x63, 0x83},
	{0x64, 0x83},
	{0x67, 0xF0},
	{0x68, 0x30},
	{0x69, 0x10},

	/* PAGE 12 */
	/* 2D */
	{0x03, 0x12},

	{0x40, 0xe9},
	{0x41, 0x09},

	{0x50, 0x18},
	{0x51, 0x24},

	{0x70, 0x1f},
	{0x71, 0x00},
	{0x72, 0x00},
	{0x73, 0x00},
	{0x74, 0x10},
	{0x75, 0x10},
	{0x76, 0x20},
	{0x77, 0x80},
	{0x78, 0x88},
	{0x79, 0x18},

	{0xb0, 0x7d},

	/* PAGE 13 */
	/* Edge Enhancement */
	{0x03, 0x13},
	{0x10, 0x01},
	{0x11, 0x89},
	{0x12, 0x14},
	{0x13, 0x19},
	{0x14, 0x08},

	{0x20, 0x06},
	{0x21, 0x03},
	{0x23, 0x30},
	{0x24, 0x33},
	{0x25, 0x08},
	{0x26, 0x18},
	{0x27, 0x00},
	{0x28, 0x08},
	{0x29, 0x50},
	{0x2a, 0xe0},
	{0x2b, 0x10},
	{0x2c, 0x28},
	{0x2d, 0x40},
	{0x2e, 0x00},
	{0x2f, 0x00},

	/* PAGE 11 */
	{0x30, 0x11},

	{0x80, 0x03},
	{0x81, 0x07},

	{0x90, 0x04},
	{0x91, 0x02},
	{0x92, 0x00},
	{0x93, 0x20},
	{0x94, 0x42},
	{0x95, 0x60},

	/* PAGE 14 */
	/* Lens Shading Correction */
	{0x03, 0x14},
	{0x10, 0x01},

	{0x20, 0x80},		/* For Y decay */
	{0x21, 0x80},		/* For Y decay */
	{0x22, 0x78},
	{0x23, 0x4d},
	{0x24, 0x46},

	/* PAGE 15 */
	/* Color Correction */
	{0x03, 0x15},
	{0x10, 0x03},
	{0x14, 0x3c},
	{0x16, 0x2c},
	{0x17, 0x2f},

	{0x30, 0xc4},
	{0x31, 0x5b},
	{0x32, 0x1f},
	{0x33, 0x2a},
	{0x34, 0xce},
	{0x35, 0x24},
	{0x36, 0x0b},
	{0x37, 0x3f},
	{0x38, 0x8a},

	{0x40, 0x87},
	{0x41, 0x18},
	{0x42, 0x91},
	{0x43, 0x94},
	{0x44, 0x9f},
	{0x45, 0x33},
	{0x46, 0x00},
	{0x47, 0x94},
	{0x48, 0x14},

	/* PAGE 16 */
	/* Gamma Correction */
	{0x03, 0x16},

	{0x30, 0x00},
	{0x31, 0x1c},
	{0x32, 0x2d},
	{0x33, 0x4e},
	{0x34, 0x6d},
	{0x35, 0x8b},
	{0x36, 0xa2},
	{0x37, 0xb5},
	{0x38, 0xc4},
	{0x39, 0xd0},
	{0x3a, 0xda},
	{0x3b, 0xea},
	{0x3c, 0xf4},
	{0x3d, 0xfb},
	{0x3e, 0xff},

	/* PAGE 17 */
	/* Auto Flicker Cancellation */
	{0x03, 0x17},

	{0xc4, 0x3c},
	{0xc5, 0x32},

	/* PAGE 20 */
	/* AE */
	{0x03, 0x20},

	{0x10, 0x0c},
	{0x11, 0x04},

	{0x20, 0x01},
	{0x28, 0x27},
	{0x29, 0xa1},
	{0x2a, 0xf0},
	{0x2b, 0x34},
	{0x2c, 0x2b},

	{0x30, 0xf8},
	{0x39, 0x22},
	{0x3a, 0xde},
	{0x3b, 0x22},
	{0x3c, 0xde},

	{0x60, 0x95},
	{0x68, 0x3c},
	{0x69, 0x64},
	{0x6A, 0x28},
	{0x6B, 0xc8},

	{0x70, 0x42},		/* For Y decay */
	{0x76, 0x22},
	{0x77, 0x02},
	{0x78, 0x12},
	{0x79, 0x27},
	{0x7a, 0x23},
	{0x7c, 0x1d},
	{0x7d, 0x22},

	{0x83, 0x00},		/* expTime:0x83,0x84,0x85 */
	{0x84, 0xbe},
	{0x85, 0x6e},

	{0x86, 0x00},		/* expMin is minimum time of expTime, */
	{0x87, 0xfa},

	{0x88, 0x02},
	{0x89, 0x7a},
	{0x8a, 0xc4},

	{0x8b, 0x3f},
	{0x8c, 0x7a},

	{0x8d, 0x34},
	{0x8e, 0xbc},

	{0x91, 0x02},
	{0x92, 0xdc},
	{0x93, 0x6c},
	{0x94, 0x01},
	{0x95, 0xb7},
	{0x96, 0x74},
	{0x98, 0x8C},
	{0x99, 0x23},

	{0x9c, 0x0b},		/* For Y decay: Exposure Time */
	{0x9d, 0xb8},		/* For Y decay: Exposure Time */
	{0x9e, 0x00},
	{0x9f, 0xfa},

	{0xb1, 0x14},
	{0xb2, 0x50},
	{0xb4, 0x14},
	{0xb5, 0x38},
	{0xb6, 0x26},
	{0xb7, 0x20},
	{0xb8, 0x1d},
	{0xb9, 0x1b},
	{0xba, 0x1a},
	{0xbb, 0x19},
	{0xbc, 0x19},
	{0xbd, 0x18},

	{0xc0, 0x16},		/* 0x1a->0x16 */
	{0xc3, 0x48},
	{0xc4, 0x48},

	/* PAGE 22 */
	/* AWB */
	{0x03, 0x22},
	{0x10, 0xe2},
	{0x11, 0x26},

	{0x21, 0x40},

	{0x30, 0x80},
	{0x31, 0x80},
	{0x38, 0x12},
	{0x39, 0x33},

	{0x40, 0xf0},
	{0x41, 0x33},
	{0x42, 0x33},
	{0x43, 0xf3},
	{0x44, 0x55},
	{0x45, 0x44},
	{0x46, 0x02},

	{0x80, 0x45},
	{0x81, 0x20},
	{0x82, 0x48},
	{0x83, 0x55},
	{0x84, 0x1b},
	{0x85, 0x50},
	{0x86, 0x25},
	{0x87, 0x4d},
	{0x88, 0x38},
	{0x89, 0x3e},
	{0x8a, 0x29},
	{0x8b, 0x02},
	{0x8d, 0x22},
	{0x8e, 0x71},
	{0x8f, 0x63},

	{0x90, 0x60},
	{0x91, 0x5c},
	{0x92, 0x56},
	{0x93, 0x52},
	{0x94, 0x4c},
	{0x95, 0x36},
	{0x96, 0x31},
	{0x97, 0x2e},
	{0x98, 0x2a},
	{0x99, 0x29},
	{0x9a, 0x26},
	{0x9b, 0x09},

	/* PAGE 22 */
	{0x03, 0x22},
	{0x10, 0xfb},

	/* PAGE 20 */
	{0x03, 0x20},
	{0x10, 0x9c},

	{0x01, 0xf0},

	/* PAGE 0 */
	{0x03, 0x00},
	{0x01, 0x90},		/* 0xf1 ->0x41 : For Preview Green/Red Line. */

	{0xff, 0xff}		/* End of Initial Setting */
#endif

};

/* FATP mode 1 : Light Field , 30fps , AG 1x */
const HI704_SENSOR_INIT_INFO HI704_Initial_Setting_Info_1[] = {
	/* PAGE 0 */
	/* Image Size/Windowing/HSYNC/VSYNC[Type1] */
	{0x03, 0x00},		/* PAGEMODE(0x03) */
	{0x01, 0xf1},
	{0x01, 0xf3},		/* PWRCTL(0x01[P0])Bit[1]:Software Reset. */
	{0x01, 0xf1},

	{0x11, 0x90},		/* For No Fixed Framerate Bit[2] */
	{0x12, 0x04},		/* PCLK INV */

	{0x20, 0x00},
	{0x21, 0x04},		/* Window start X */
	{0x22, 0x00},
	{0x23, 0x04},		/* Window start Y */

	{0x24, 0x01},
	{0x25, 0xe8},		/* Window height : 0x1e8 = 488 */
	{0x26, 0x02},
	{0x27, 0x84},		/* Window width  : 0x284 = 644 */


	{0x40, 0x01},		/* HBLANK: 0x70 = 112 */
	{0x41, 0x58},
	{0x42, 0x00},		/* VBLANK: 0x40 = 64 */
	{0x43, 0x13},		/* 0x04 -> 0x40: For Max Framerate = 30fps */

	/* BLC */
	{0x80, 0x2e},
	{0x81, 0x7e},
	{0x82, 0x90},
	{0x83, 0x30},
	{0x84, 0x2c},
	{0x85, 0x4b},
	{0x89, 0x48},

	{0x90, 0x0a},
	{0x91, 0x0a},
	{0x92, 0x48},
	{0x93, 0x40},
	{0x98, 0x38},
	{0x99, 0x40},
	{0xa0, 0x00},
	{0xa8, 0x40},

	/* PAGE 2 */
	/* Analog Circuit */
	{0x03, 0x02},
	{0x13, 0x40},
	{0x14, 0x04},
	{0x1a, 0x00},
	{0x1b, 0x08},

	{0x20, 0x33},
	{0x21, 0xaa},
	{0x22, 0xa7},
	{0x23, 0xb1},		/* For Sun Pot */

	{0x3b, 0x48},

	{0x50, 0x21},
	{0x52, 0xa2},
	{0x53, 0x0a},
	{0x54, 0x30},
	{0x55, 0x10},
	{0x56, 0x0c},
	{0x59, 0x0F},

	{0x60, 0x54},
	{0x61, 0x5d},
	{0x62, 0x56},
	{0x63, 0x5c},
	{0x64, 0x56},
	{0x65, 0x5c},
	{0x72, 0x57},
	{0x73, 0x5b},
	{0x74, 0x57},
	{0x75, 0x5b},
	{0x80, 0x02},
	{0x81, 0x46},
	{0x82, 0x07},
	{0x83, 0x10},
	{0x84, 0x07},
	{0x85, 0x10},
	{0x92, 0x24},
	{0x93, 0x30},
	{0x94, 0x24},
	{0x95, 0x30},
	{0xa0, 0x03},
	{0xa1, 0x45},
	{0xa4, 0x45},
	{0xa5, 0x03},
	{0xa8, 0x12},
	{0xa9, 0x20},
	{0xaa, 0x34},
	{0xab, 0x40},
	{0xb8, 0x55},
	{0xb9, 0x59},
	{0xbc, 0x05},
	{0xbd, 0x09},
	{0xc0, 0x5f},
	{0xc1, 0x67},
	{0xc2, 0x5f},
	{0xc3, 0x67},
	{0xc4, 0x60},
	{0xc5, 0x66},
	{0xc6, 0x60},
	{0xc7, 0x66},
	{0xc8, 0x61},
	{0xc9, 0x65},
	{0xca, 0x61},
	{0xcb, 0x65},
	{0xcc, 0x62},
	{0xcd, 0x64},
	{0xce, 0x62},
	{0xcf, 0x64},
	{0xd0, 0x53},
	{0xd1, 0x68},

	/* PAGE 10 */
	/* Image Format, Image Effect */
	{0x03, 0x10},
	{0x10, 0x03},
	{0x11, 0x43},
	{0x12, 0x30},

	{0x40, 0x80},
	{0x41, 0x02},
	{0x48, 0x98},

	{0x50, 0x48},

	{0x60, 0x7f},
	{0x61, 0x00},
	{0x62, 0xb0},
	{0x63, 0xa8},
	{0x64, 0x48},
	{0x66, 0x90},
	{0x67, 0x42},

	/* PAGE 11 */
	/* Z-LPF */
	{0x03, 0x11},
	{0x10, 0x25},
	{0x11, 0x1f},

	{0x20, 0x00},
	{0x21, 0x38},
	{0x23, 0x0a},

	{0x60, 0x10},
	{0x61, 0x82},
	{0x62, 0x00},
	{0x63, 0x83},
	{0x64, 0x83},
	{0x67, 0xF0},
	{0x68, 0x30},
	{0x69, 0x10},

	/* PAGE 12 */
	/* 2D */
	{0x03, 0x12},

	{0x40, 0xe9},
	{0x41, 0x09},

	{0x50, 0x18},
	{0x51, 0x24},

	{0x70, 0x1f},
	{0x71, 0x00},
	{0x72, 0x00},
	{0x73, 0x00},
	{0x74, 0x10},
	{0x75, 0x10},
	{0x76, 0x20},
	{0x77, 0x80},
	{0x78, 0x88},
	{0x79, 0x18},

	{0xb0, 0x7d},

	/* PAGE 13 */
	/* Edge Enhancement */
	{0x03, 0x13},
	{0x10, 0x00},		/* Sharpness Off by Sam */
	{0x11, 0x89},
	{0x12, 0x14},
	{0x13, 0x19},
	{0x14, 0x08},

	{0x20, 0x06},
	{0x21, 0x03},
	{0x23, 0x30},
	{0x24, 0x33},
	{0x25, 0x08},
	{0x26, 0x18},
	{0x27, 0x00},
	{0x28, 0x08},
	{0x29, 0x50},
	{0x2a, 0xe0},
	{0x2b, 0x10},
	{0x2c, 0x28},
	{0x2d, 0x40},
	{0x2e, 0x00},
	{0x2f, 0x00},

	/* PAGE 11 */
	{0x30, 0x11},

	{0x80, 0x03},
	{0x81, 0x07},

	{0x90, 0x04},
	{0x91, 0x02},
	{0x92, 0x00},
	{0x93, 0x20},
	{0x94, 0x42},
	{0x95, 0x60},

	/* PAGE 14 */
	/* Lens Shading Correction */
	{0x03, 0x14},
	{0x10, 0x00},		/* LSC Off by sam */

	{0x20, 0x80},		/* For Y decay */
	{0x21, 0x80},		/* For Y decay */
	{0x22, 0x78},
	{0x23, 0x4d},
	{0x24, 0x46},

	/* PAGE 15 */
	/* Color Correction */
	{0x03, 0x15},
	{0x10, 0x03},
	{0x14, 0x3c},
	{0x16, 0x2c},
	{0x17, 0x2f},

	{0x30, 0xc4},
	{0x31, 0x5b},
	{0x32, 0x1f},
	{0x33, 0x2a},
	{0x34, 0xce},
	{0x35, 0x24},
	{0x36, 0x0b},
	{0x37, 0x3f},
	{0x38, 0x8a},

	{0x40, 0x87},
	{0x41, 0x18},
	{0x42, 0x91},
	{0x43, 0x94},
	{0x44, 0x9f},
	{0x45, 0x33},
	{0x46, 0x00},
	{0x47, 0x94},
	{0x48, 0x14},

	/* PAGE 16 */
	/* Gamma Correction */
	{0x03, 0x16},

	{0x30, 0x00},
	{0x31, 0x1c},
	{0x32, 0x2d},
	{0x33, 0x4e},
	{0x34, 0x6d},
	{0x35, 0x8b},
	{0x36, 0xa2},
	{0x37, 0xb5},
	{0x38, 0xc4},
	{0x39, 0xd0},
	{0x3a, 0xda},
	{0x3b, 0xea},
	{0x3c, 0xf4},
	{0x3d, 0xfb},
	{0x3e, 0xff},

	/* PAGE 17 */
	/* Auto Flicker Cancellation */
	{0x03, 0x17},

	{0xc4, 0x3c},
	{0xc5, 0x32},

	/* PAGE 20 */
	/* AE */
	{0x03, 0x20},

	{0x10, 0x08},		/* Disable AE */
	{0x11, 0x04},

	{0x20, 0x01},
	{0x28, 0x27},
	{0x29, 0xa1},
	{0x2a, 0xf0},
	{0x2b, 0x34},
	{0x2c, 0x2b},

	{0x30, 0xf8},
	{0x39, 0x22},
	{0x3a, 0xde},
	{0x3b, 0x22},
	{0x3c, 0xde},

	{0x60, 0x95},
	{0x68, 0x3c},
	{0x69, 0x64},
	{0x6A, 0x28},
	{0x6B, 0xc8},

	{0x70, 0x42},		/* For Y decay */
	{0x76, 0x22},
	{0x77, 0x02},
	{0x78, 0x12},
	{0x79, 0x27},
	{0x7a, 0x23},
	{0x7c, 0x1d},
	{0x7d, 0x22},

	{0x83, 0x00},		/* expTime:0x83,0x84,0x85 */
	{0x84, 0xc3},
	{0x85, 0x00},

	{0x86, 0x00},		/* expMin is minimum time of expTime, */
	{0x87, 0xfa},

	{0x88, 0x02},
	{0x89, 0x7a},
	{0x8a, 0xc4},

	{0x8b, 0x3f},
	{0x8c, 0x7a},

	{0x8d, 0x34},
	{0x8e, 0xbc},

	{0x91, 0x02},
	{0x92, 0xdc},
	{0x93, 0x6c},
	{0x94, 0x01},
	{0x95, 0xb7},
	{0x96, 0x74},
	{0x98, 0x8C},
	{0x99, 0x23},

	{0x9c, 0x0b},		/* For Y decay: Exposure Time */
	{0x9d, 0xb8},		/* For Y decay: Exposure Time */
	{0x9e, 0x00},
	{0x9f, 0xfa},

	{0xb0, 0x16},		/* Analog gain min:1x */
	{0xb1, 0x14},
	{0xb2, 0x50},
	{0xb4, 0x14},
	{0xb5, 0x38},
	{0xb6, 0x26},
	{0xb7, 0x20},
	{0xb8, 0x1d},
	{0xb9, 0x1b},
	{0xba, 0x1a},
	{0xbb, 0x19},
	{0xbc, 0x19},
	{0xbd, 0x18},

	{0xc0, 0x16},		/* 0x1a->0x16 */
	{0xc3, 0x48},
	{0xc4, 0x48},

	/* PAGE 22 */
	/* AWB */
	{0x03, 0x22},
	{0x10, 0x7b},
	{0x11, 0x26},

	{0x21, 0x40},

	{0x30, 0x80},
	{0x31, 0x80},
	{0x38, 0x12},
	{0x39, 0x33},

	{0x40, 0xf0},
	{0x41, 0x33},
	{0x42, 0x33},
	{0x43, 0xf3},
	{0x44, 0x55},
	{0x45, 0x44},
	{0x46, 0x02},

	{0x80, 0x45},
	{0x81, 0x20},
	{0x82, 0x48},
	{0x83, 0x55},
	{0x84, 0x1b},
	{0x85, 0x50},
	{0x86, 0x25},
	{0x87, 0x4d},
	{0x88, 0x38},
	{0x89, 0x3e},
	{0x8a, 0x29},
	{0x8b, 0x02},
	{0x8d, 0x22},
	{0x8e, 0x71},
	{0x8f, 0x63},

	{0x90, 0x60},
	{0x91, 0x5c},
	{0x92, 0x56},
	{0x93, 0x52},
	{0x94, 0x4c},
	{0x95, 0x36},
	{0x96, 0x31},
	{0x97, 0x2e},
	{0x98, 0x2a},
	{0x99, 0x29},
	{0x9a, 0x26},
	{0x9b, 0x09},

	/* PAGE 22 */
	{0x03, 0x22},
	{0x10, 0xFb},		/* AWB off by Sam fb-->7b */

	/* PAGE 20 */
	{0x03, 0x20},
	{0x10, 0x08},		/* AE off by Sam fc-->1c */

	{0x01, 0xf0},

	/* PAGE 0 */
	{0x03, 0x00},
	{0x01, 0x90},		/* 0xf1 ->0x41 : For Preview Green/Red Line. */

	{0xff, 0xff}		/* End of Initial Setting */

};

/* FATP mode 2 : Dark Field , 7.5fps , AG 3.5x */
const HI704_SENSOR_INIT_INFO HI704_Initial_Setting_Info_2[] = {
	/* PAGE 0 */
	/* Image Size/Windowing/HSYNC/VSYNC[Type1] */
	{0x03, 0x00},		/* PAGEMODE(0x03) */
	{0x01, 0xf1},
	{0x01, 0xf3},		/* PWRCTL(0x01[P0])Bit[1]:Software Reset. */
	{0x01, 0xf1},

	{0x11, 0x90},		/* For No Fixed Framerate Bit[2] */
	{0x12, 0x04},		/* PCLK INV */

	{0x20, 0x00},
	{0x21, 0x04},		/* Window start X */
	{0x22, 0x00},
	{0x23, 0x04},		/* Window start Y */

	{0x24, 0x01},
	{0x25, 0xe8},		/* Window height : 0x1e8 = 488 */
	{0x26, 0x02},
	{0x27, 0x84},		/* Window width  : 0x284 = 644 */


	{0x40, 0x01},		/* HBLANK: 0x70 = 112 */
	{0x41, 0x58},
	{0x42, 0x00},		/* VBLANK: 0x40 = 64 */
	{0x43, 0x13},		/* 0x04 -> 0x40: For Max Framerate = 30fps */

	/* BLC */
	{0x80, 0x2e},
	{0x81, 0x7e},
	{0x82, 0x90},
	{0x83, 0x30},
	{0x84, 0x2c},
	{0x85, 0x4b},
	{0x89, 0x48},

	{0x90, 0x0a},
	{0x91, 0x0a},
	{0x92, 0x48},
	{0x93, 0x40},
	{0x98, 0x38},
	{0x99, 0x40},
	{0xa0, 0x00},
	{0xa8, 0x40},

	/* PAGE 2 */
	/* Analog Circuit */
	{0x03, 0x02},
	{0x13, 0x40},
	{0x14, 0x04},
	{0x1a, 0x00},
	{0x1b, 0x08},

	{0x20, 0x33},
	{0x21, 0xaa},
	{0x22, 0xa7},
	{0x23, 0xb1},		/* For Sun Pot */

	{0x3b, 0x48},

	{0x50, 0x21},
	{0x52, 0xa2},
	{0x53, 0x0a},
	{0x54, 0x30},
	{0x55, 0x10},
	{0x56, 0x0c},
	{0x59, 0x0F},

	{0x60, 0x54},
	{0x61, 0x5d},
	{0x62, 0x56},
	{0x63, 0x5c},
	{0x64, 0x56},
	{0x65, 0x5c},
	{0x72, 0x57},
	{0x73, 0x5b},
	{0x74, 0x57},
	{0x75, 0x5b},
	{0x80, 0x02},
	{0x81, 0x46},
	{0x82, 0x07},
	{0x83, 0x10},
	{0x84, 0x07},
	{0x85, 0x10},
	{0x92, 0x24},
	{0x93, 0x30},
	{0x94, 0x24},
	{0x95, 0x30},
	{0xa0, 0x03},
	{0xa1, 0x45},
	{0xa4, 0x45},
	{0xa5, 0x03},
	{0xa8, 0x12},
	{0xa9, 0x20},
	{0xaa, 0x34},
	{0xab, 0x40},
	{0xb8, 0x55},
	{0xb9, 0x59},
	{0xbc, 0x05},
	{0xbd, 0x09},
	{0xc0, 0x5f},
	{0xc1, 0x67},
	{0xc2, 0x5f},
	{0xc3, 0x67},
	{0xc4, 0x60},
	{0xc5, 0x66},
	{0xc6, 0x60},
	{0xc7, 0x66},
	{0xc8, 0x61},
	{0xc9, 0x65},
	{0xca, 0x61},
	{0xcb, 0x65},
	{0xcc, 0x62},
	{0xcd, 0x64},
	{0xce, 0x62},
	{0xcf, 0x64},
	{0xd0, 0x53},
	{0xd1, 0x68},

	/* PAGE 10 */
	/* Image Format, Image Effect */
	{0x03, 0x10},
	{0x10, 0x03},
	{0x11, 0x43},
	{0x12, 0x30},

	{0x40, 0x80},
	{0x41, 0x02},
	{0x48, 0x98},

	{0x50, 0x48},

	{0x60, 0x7f},
	{0x61, 0x00},
	{0x62, 0xb0},
	{0x63, 0xa8},
	{0x64, 0x48},
	{0x66, 0x90},
	{0x67, 0x42},

	/* PAGE 11 */
	/* Z-LPF */
	{0x03, 0x11},
	{0x10, 0x25},
	{0x11, 0x1f},

	{0x20, 0x00},
	{0x21, 0x38},
	{0x23, 0x0a},

	{0x60, 0x10},
	{0x61, 0x82},
	{0x62, 0x00},
	{0x63, 0x83},
	{0x64, 0x83},
	{0x67, 0xF0},
	{0x68, 0x30},
	{0x69, 0x10},

	/* PAGE 12 */
	/* 2D */
	{0x03, 0x12},

	{0x40, 0xe9},
	{0x41, 0x09},

	{0x50, 0x18},
	{0x51, 0x24},

	{0x70, 0x1f},
	{0x71, 0x00},
	{0x72, 0x00},
	{0x73, 0x00},
	{0x74, 0x10},
	{0x75, 0x10},
	{0x76, 0x20},
	{0x77, 0x80},
	{0x78, 0x88},
	{0x79, 0x18},

	{0xb0, 0x7d},

	/* PAGE 13 */
	/* Edge Enhancement */
	{0x03, 0x13},
	{0x10, 0x00},		/* Sharpness Off by Sam */
	{0x11, 0x89},
	{0x12, 0x14},
	{0x13, 0x19},
	{0x14, 0x08},

	{0x20, 0x06},
	{0x21, 0x03},
	{0x23, 0x30},
	{0x24, 0x33},
	{0x25, 0x08},
	{0x26, 0x18},
	{0x27, 0x00},
	{0x28, 0x08},
	{0x29, 0x50},
	{0x2a, 0xe0},
	{0x2b, 0x10},
	{0x2c, 0x28},
	{0x2d, 0x40},
	{0x2e, 0x00},
	{0x2f, 0x00},

	/* PAGE 11 */
	{0x30, 0x11},

	{0x80, 0x03},
	{0x81, 0x07},

	{0x90, 0x04},
	{0x91, 0x02},
	{0x92, 0x00},
	{0x93, 0x20},
	{0x94, 0x42},
	{0x95, 0x60},

	/* PAGE 14 */
	/* Lens Shading Correction */
	{0x03, 0x14},
	{0x10, 0x00},		/* LSC Off by sam */

	{0x20, 0x80},		/* For Y decay */
	{0x21, 0x80},		/* For Y decay */
	{0x22, 0x78},
	{0x23, 0x4d},
	{0x24, 0x46},

	/* PAGE 15 */
	/* Color Correction */
	{0x03, 0x15},
	{0x10, 0x03},
	{0x14, 0x3c},
	{0x16, 0x2c},
	{0x17, 0x2f},

	{0x30, 0xc4},
	{0x31, 0x5b},
	{0x32, 0x1f},
	{0x33, 0x2a},
	{0x34, 0xce},
	{0x35, 0x24},
	{0x36, 0x0b},
	{0x37, 0x3f},
	{0x38, 0x8a},

	{0x40, 0x87},
	{0x41, 0x18},
	{0x42, 0x91},
	{0x43, 0x94},
	{0x44, 0x9f},
	{0x45, 0x33},
	{0x46, 0x00},
	{0x47, 0x94},
	{0x48, 0x14},

	/* PAGE 16 */
	/* Gamma Correction */
	{0x03, 0x16},

	{0x30, 0x00},
	{0x31, 0x1c},
	{0x32, 0x2d},
	{0x33, 0x4e},
	{0x34, 0x6d},
	{0x35, 0x8b},
	{0x36, 0xa2},
	{0x37, 0xb5},
	{0x38, 0xc4},
	{0x39, 0xd0},
	{0x3a, 0xda},
	{0x3b, 0xea},
	{0x3c, 0xf4},
	{0x3d, 0xfb},
	{0x3e, 0xff},

	/* PAGE 17 */
	/* Auto Flicker Cancellation */
	{0x03, 0x17},

	{0xc4, 0x3c},
	{0xc5, 0x32},

	/* PAGE 20 */
	/* AE */
	{0x03, 0x20},

	{0x10, 0x08},		/* Disable AE */
	{0x11, 0x04},

	{0x20, 0x01},
	{0x28, 0x27},
	{0x29, 0xa1},
	{0x2a, 0xf0},
	{0x2b, 0x34},
	{0x2c, 0x2b},

	{0x30, 0xf8},
	{0x39, 0x22},
	{0x3a, 0xde},
	{0x3b, 0x22},
	{0x3c, 0xde},

	{0x60, 0x95},
	{0x68, 0x3c},
	{0x69, 0x64},
	{0x6A, 0x28},
	{0x6B, 0xc8},

	{0x70, 0x42},		/* For Y decay */
	{0x76, 0x22},
	{0x77, 0x02},
	{0x78, 0x12},
	{0x79, 0x27},
	{0x7a, 0x23},
	{0x7c, 0x1d},
	{0x7d, 0x22},

	{0x83, 0x03},		/* expTime:0x83,0x84,0x85 */
	{0x84, 0x0C},
	{0x85, 0x00},

	{0x86, 0x00},		/* expMin is minimum time of expTime, */
	{0x87, 0xfa},

	{0x88, 0x02},
	{0x89, 0x7a},
	{0x8a, 0xc4},

	{0x8b, 0x3f},
	{0x8c, 0x7a},

	{0x8d, 0x34},
	{0x8e, 0xbc},

	{0x91, 0x02},
	{0x92, 0xdc},
	{0x93, 0x6c},
	{0x94, 0x01},
	{0x95, 0xb7},
	{0x96, 0x74},
	{0x98, 0x8C},
	{0x99, 0x23},

	{0x9c, 0x0b},		/* For Y decay: Exposure Time */
	{0x9d, 0xb8},		/* For Y decay: Exposure Time */
	{0x9e, 0x00},
	{0x9f, 0xfa},

	{0xb0, 0x60},		/* Analog gain min:3.5x */
	{0xb1, 0x14},
	{0xb2, 0x50},
	{0xb4, 0x14},
	{0xb5, 0x38},
	{0xb6, 0x26},
	{0xb7, 0x20},
	{0xb8, 0x1d},
	{0xb9, 0x1b},
	{0xba, 0x1a},
	{0xbb, 0x19},
	{0xbc, 0x19},
	{0xbd, 0x18},

	{0xc0, 0x16},		/* 0x1a->0x16 */
	{0xc3, 0x48},
	{0xc4, 0x48},

	/* PAGE 22 */
	/* AWB */
	{0x03, 0x22},
	{0x10, 0x7b},
	{0x11, 0x26},

	{0x21, 0x40},

	{0x30, 0x80},
	{0x31, 0x80},
	{0x38, 0x12},
	{0x39, 0x33},

	{0x40, 0xf0},
	{0x41, 0x33},
	{0x42, 0x33},
	{0x43, 0xf3},
	{0x44, 0x55},
	{0x45, 0x44},
	{0x46, 0x02},

	{0x80, 0x20},
	{0x81, 0x20},
	{0x82, 0x20},
	{0x83, 0x55},
	{0x84, 0x1b},
	{0x85, 0x50},
	{0x86, 0x25},
	{0x87, 0x4d},
	{0x88, 0x38},
	{0x89, 0x3e},
	{0x8a, 0x29},
	{0x8b, 0x02},
	{0x8d, 0x22},
	{0x8e, 0x71},
	{0x8f, 0x63},

	{0x90, 0x60},
	{0x91, 0x5c},
	{0x92, 0x56},
	{0x93, 0x52},
	{0x94, 0x4c},
	{0x95, 0x36},
	{0x96, 0x31},
	{0x97, 0x2e},
	{0x98, 0x2a},
	{0x99, 0x29},
	{0x9a, 0x26},
	{0x9b, 0x09},

	/* PAGE 22 */
	{0x03, 0x22},
	{0x10, 0x7b},		/* AWB off by Sam fb-->7b */

	/* PAGE 20 */
	{0x03, 0x20},
	{0x10, 0x08},		/* AE off by Sam fc-->1c */

	{0x01, 0xf0},

	/* PAGE 0 */
	{0x03, 0x00},
	{0x01, 0x90},		/* 0xf1 ->0x41 : For Preview Green/Red Line. */

	{0xff, 0xff}		/* End of Initial Setting */

};

/* FATP mode 3 : SFR , 30fps , AG 1x */
const HI704_SENSOR_INIT_INFO HI704_Initial_Setting_Info_3[] = {
	/* PAGE 0 */
	/* Image Size/Windowing/HSYNC/VSYNC[Type1] */
	{0x03, 0x00},		/* PAGEMODE(0x03) */
	{0x01, 0xf1},
	{0x01, 0xf3},		/* PWRCTL(0x01[P0])Bit[1]:Software Reset. */
	{0x01, 0xf1},

	{0x11, 0x90},		/* For No Fixed Framerate Bit[2] */
	{0x12, 0x04},		/* PCLK INV */

	{0x20, 0x00},
	{0x21, 0x04},		/* Window start X */
	{0x22, 0x00},
	{0x23, 0x04},		/* Window start Y */

	{0x24, 0x01},
	{0x25, 0xe8},		/* Window height : 0x1e8 = 488 */
	{0x26, 0x02},
	{0x27, 0x84},		/* Window width  : 0x284 = 644 */


	{0x40, 0x01},		/* HBLANK: 0x70 = 112 */
	{0x41, 0x58},
	{0x42, 0x00},		/* VBLANK: 0x40 = 64 */
	{0x43, 0x13},		/* 0x04 -> 0x40: For Max Framerate = 30fps */

	/* BLC */
	{0x80, 0x2e},
	{0x81, 0x7e},
	{0x82, 0x90},
	{0x83, 0x30},
	{0x84, 0x2c},
	{0x85, 0x4b},
	{0x89, 0x48},

	{0x90, 0x0a},
	{0x91, 0x0a},
	{0x92, 0x48},
	{0x93, 0x40},
	{0x98, 0x38},
	{0x99, 0x40},
	{0xa0, 0x00},
	{0xa8, 0x40},

	/* PAGE 2 */
	/* Analog Circuit */
	{0x03, 0x02},
	{0x13, 0x40},
	{0x14, 0x04},
	{0x1a, 0x00},
	{0x1b, 0x08},

	{0x20, 0x33},
	{0x21, 0xaa},
	{0x22, 0xa7},
	{0x23, 0xb1},		/* For Sun Pot */

	{0x3b, 0x48},

	{0x50, 0x21},
	{0x52, 0xa2},
	{0x53, 0x0a},
	{0x54, 0x30},
	{0x55, 0x10},
	{0x56, 0x0c},
	{0x59, 0x0F},

	{0x60, 0x54},
	{0x61, 0x5d},
	{0x62, 0x56},
	{0x63, 0x5c},
	{0x64, 0x56},
	{0x65, 0x5c},
	{0x72, 0x57},
	{0x73, 0x5b},
	{0x74, 0x57},
	{0x75, 0x5b},
	{0x80, 0x02},
	{0x81, 0x46},
	{0x82, 0x07},
	{0x83, 0x10},
	{0x84, 0x07},
	{0x85, 0x10},
	{0x92, 0x24},
	{0x93, 0x30},
	{0x94, 0x24},
	{0x95, 0x30},
	{0xa0, 0x03},
	{0xa1, 0x45},
	{0xa4, 0x45},
	{0xa5, 0x03},
	{0xa8, 0x12},
	{0xa9, 0x20},
	{0xaa, 0x34},
	{0xab, 0x40},
	{0xb8, 0x55},
	{0xb9, 0x59},
	{0xbc, 0x05},
	{0xbd, 0x09},
	{0xc0, 0x5f},
	{0xc1, 0x67},
	{0xc2, 0x5f},
	{0xc3, 0x67},
	{0xc4, 0x60},
	{0xc5, 0x66},
	{0xc6, 0x60},
	{0xc7, 0x66},
	{0xc8, 0x61},
	{0xc9, 0x65},
	{0xca, 0x61},
	{0xcb, 0x65},
	{0xcc, 0x62},
	{0xcd, 0x64},
	{0xce, 0x62},
	{0xcf, 0x64},
	{0xd0, 0x53},
	{0xd1, 0x68},

	/* PAGE 10 */
	/* Image Format, Image Effect */
	{0x03, 0x10},
	{0x10, 0x03},
	{0x11, 0x43},
	{0x12, 0x30},

	{0x40, 0x80},
	{0x41, 0x02},
	{0x48, 0x98},

	{0x50, 0x48},

	{0x60, 0x7f},
	{0x61, 0x00},
	{0x62, 0xb0},
	{0x63, 0xa8},
	{0x64, 0x48},
	{0x66, 0x90},
	{0x67, 0x42},

	/* PAGE 11 */
	/* Z-LPF */
	{0x03, 0x11},
	{0x10, 0x25},
	{0x11, 0x1f},

	{0x20, 0x00},
	{0x21, 0x38},
	{0x23, 0x0a},

	{0x60, 0x10},
	{0x61, 0x82},
	{0x62, 0x00},
	{0x63, 0x83},
	{0x64, 0x83},
	{0x67, 0xF0},
	{0x68, 0x30},
	{0x69, 0x10},

	/* PAGE 12 */
	/* 2D */
	{0x03, 0x12},

	{0x40, 0xe9},
	{0x41, 0x09},

	{0x50, 0x18},
	{0x51, 0x24},

	{0x70, 0x1f},
	{0x71, 0x00},
	{0x72, 0x00},
	{0x73, 0x00},
	{0x74, 0x10},
	{0x75, 0x10},
	{0x76, 0x20},
	{0x77, 0x80},
	{0x78, 0x88},
	{0x79, 0x18},

	{0xb0, 0x7d},

	/* PAGE 13 */
	/* Edge Enhancement */
	{0x03, 0x13},
	{0x10, 0x00},		/* Sharpness Off by Sam */
	{0x11, 0x89},
	{0x12, 0x14},
	{0x13, 0x19},
	{0x14, 0x08},

	{0x20, 0x06},
	{0x21, 0x03},
	{0x23, 0x30},
	{0x24, 0x33},
	{0x25, 0x08},
	{0x26, 0x18},
	{0x27, 0x00},
	{0x28, 0x08},
	{0x29, 0x50},
	{0x2a, 0xe0},
	{0x2b, 0x10},
	{0x2c, 0x28},
	{0x2d, 0x40},
	{0x2e, 0x00},
	{0x2f, 0x00},

	/* PAGE 11 */
	{0x30, 0x11},

	{0x80, 0x03},
	{0x81, 0x07},

	{0x90, 0x04},
	{0x91, 0x02},
	{0x92, 0x00},
	{0x93, 0x20},
	{0x94, 0x42},
	{0x95, 0x60},

	/* PAGE 14 */
	/* Lens Shading Correction */
	{0x03, 0x14},
	{0x10, 0x00},		/* LSC Off by sam */

	{0x20, 0x80},		/* For Y decay */
	{0x21, 0x80},		/* For Y decay */
	{0x22, 0x78},
	{0x23, 0x4d},
	{0x24, 0x46},

	/* PAGE 15 */
	/* Color Correction */
	{0x03, 0x15},
	{0x10, 0x03},
	{0x14, 0x3c},
	{0x16, 0x2c},
	{0x17, 0x2f},

	{0x30, 0xc4},
	{0x31, 0x5b},
	{0x32, 0x1f},
	{0x33, 0x2a},
	{0x34, 0xce},
	{0x35, 0x24},
	{0x36, 0x0b},
	{0x37, 0x3f},
	{0x38, 0x8a},

	{0x40, 0x87},
	{0x41, 0x18},
	{0x42, 0x91},
	{0x43, 0x94},
	{0x44, 0x9f},
	{0x45, 0x33},
	{0x46, 0x00},
	{0x47, 0x94},
	{0x48, 0x14},

	/* PAGE 16 */
	/* Gamma Correction */
	{0x03, 0x16},

	{0x30, 0x00},
	{0x31, 0x1c},
	{0x32, 0x2d},
	{0x33, 0x4e},
	{0x34, 0x6d},
	{0x35, 0x8b},
	{0x36, 0xa2},
	{0x37, 0xb5},
	{0x38, 0xc4},
	{0x39, 0xd0},
	{0x3a, 0xda},
	{0x3b, 0xea},
	{0x3c, 0xf4},
	{0x3d, 0xfb},
	{0x3e, 0xff},

	/* PAGE 17 */
	/* Auto Flicker Cancellation */
	{0x03, 0x17},

	{0xc4, 0x3c},
	{0xc5, 0x32},

	/* PAGE 20 */
	/* AE */
	{0x03, 0x20},

	{0x10, 0x08},		/* Disable AE */
	{0x11, 0x04},

	{0x20, 0x01},
	{0x28, 0x27},
	{0x29, 0xa1},
	{0x2a, 0xf0},
	{0x2b, 0x34},
	{0x2c, 0x2b},

	{0x30, 0xf8},
	{0x39, 0x22},
	{0x3a, 0xde},
	{0x3b, 0x22},
	{0x3c, 0xde},

	{0x60, 0x95},
	{0x68, 0x3c},
	{0x69, 0x64},
	{0x6A, 0x28},
	{0x6B, 0xc8},

	{0x70, 0x42},		/* For Y decay */
	{0x76, 0x22},
	{0x77, 0x02},
	{0x78, 0x12},
	{0x79, 0x27},
	{0x7a, 0x23},
	{0x7c, 0x1d},
	{0x7d, 0x22},

	{0x83, 0x00},		/* expTime:0x83,0x84,0x85 */
	{0x84, 0xc3},
	{0x85, 0x00},

	{0x86, 0x00},		/* expMin is minimum time of expTime, */
	{0x87, 0xfa},

	{0x88, 0x02},
	{0x89, 0x7a},
	{0x8a, 0xc4},

	{0x8b, 0x3f},
	{0x8c, 0x7a},

	{0x8d, 0x34},
	{0x8e, 0xbc},

	{0x91, 0x02},
	{0x92, 0xdc},
	{0x93, 0x6c},
	{0x94, 0x01},
	{0x95, 0xb7},
	{0x96, 0x74},
	{0x98, 0x8C},
	{0x99, 0x23},

	{0x9c, 0x0b},		/* For Y decay: Exposure Time */
	{0x9d, 0xb8},		/* For Y decay: Exposure Time */
	{0x9e, 0x00},
	{0x9f, 0xfa},

	{0xb0, 0x16},		/* Analog gain min:1x */
	{0xb1, 0x14},
	{0xb2, 0x50},
	{0xb4, 0x14},
	{0xb5, 0x38},
	{0xb6, 0x26},
	{0xb7, 0x20},
	{0xb8, 0x1d},
	{0xb9, 0x1b},
	{0xba, 0x1a},
	{0xbb, 0x19},
	{0xbc, 0x19},
	{0xbd, 0x18},

	{0xc0, 0x16},		/* 0x1a->0x16 */
	{0xc3, 0x48},
	{0xc4, 0x48},

	/* PAGE 22 */
	/* AWB */
	{0x03, 0x22},
	{0x10, 0x7b},
	{0x11, 0x26},

	{0x21, 0x40},

	{0x30, 0x80},
	{0x31, 0x80},
	{0x38, 0x12},
	{0x39, 0x33},

	{0x40, 0xf0},
	{0x41, 0x33},
	{0x42, 0x33},
	{0x43, 0xf3},
	{0x44, 0x55},
	{0x45, 0x44},
	{0x46, 0x02},

	{0x80, 0x20},
	{0x81, 0x20},
	{0x82, 0x20},
	{0x83, 0x55},
	{0x84, 0x1b},
	{0x85, 0x50},
	{0x86, 0x25},
	{0x87, 0x4d},
	{0x88, 0x38},
	{0x89, 0x3e},
	{0x8a, 0x29},
	{0x8b, 0x02},
	{0x8d, 0x22},
	{0x8e, 0x71},
	{0x8f, 0x63},

	{0x90, 0x60},
	{0x91, 0x5c},
	{0x92, 0x56},
	{0x93, 0x52},
	{0x94, 0x4c},
	{0x95, 0x36},
	{0x96, 0x31},
	{0x97, 0x2e},
	{0x98, 0x2a},
	{0x99, 0x29},
	{0x9a, 0x26},
	{0x9b, 0x09},

	/* PAGE 22 */
	{0x03, 0x22},
	{0x10, 0x7b},		/* AWB off by Sam fb-->7b */

	/* PAGE 20 */
	{0x03, 0x20},
	{0x10, 0x08},		/* AE off by Sam fc-->1c */

	{0x01, 0xf0},

	/* PAGE 0 */
	{0x03, 0x00},
	{0x01, 0x90},		/* 0xf1 ->0x41 : For Preview Green/Red Line. */

	{0xff, 0xff}		/* End of Initial Setting */

};



#define HI704_TEST_PATTERN_CHECKSUM 0x5b8de30
UINT32 HI704SetTestPatternMode(kal_bool bEnable)
{
	/* SENSORDB("[HI704SetTestPatternMode] Fail: Not Support. Test pattern enable:%d\n", bEnable); */
	if (bEnable) {
		/* output color bar */
		HI704_write_cmos_sensor(0x03, 0x00);	/* Page 0 */
		HI704_write_cmos_sensor(0x50, 0x04);	/* Sleep ON */
	} else {
		HI704_write_cmos_sensor(0x03, 0x00);	/* Page 0 */
		HI704_write_cmos_sensor(0x50, 0x00);	/* Sleep ON */
	}
	return ERROR_NONE;
}



static void HI704_Initial_Setting(void)
{
	kal_uint32 iEcount;

	SENSORDB("[HI704_Initial_Setting] init config node : %d\n", sub_sensor_init_setting_switch);
	if (sub_sensor_init_setting_switch == 0) {	/* Normal mode 0 */
		for (iEcount = 0; (!((0xff == (HI704_Initial_Setting_Info[iEcount].address))
				     && (0xff == (HI704_Initial_Setting_Info[iEcount].data))));
		     iEcount++) {
			HI704_write_cmos_sensor(HI704_Initial_Setting_Info[iEcount].address,
						HI704_Initial_Setting_Info[iEcount].data);
		}
		HI704_Set_VGA_mode();
	} else if (sub_sensor_init_setting_switch == 1) {	/* FATP mode 1: Light Field */
		for (iEcount = 0; (!((0xff == (HI704_Initial_Setting_Info_1[iEcount].address))
				     && (0xff == (HI704_Initial_Setting_Info_1[iEcount].data))));
		     iEcount++) {
			HI704_write_cmos_sensor(HI704_Initial_Setting_Info_1[iEcount].address,
						HI704_Initial_Setting_Info_1[iEcount].data);
		}
	} else if (sub_sensor_init_setting_switch == 2) {	/* FATP mode 2:Dark Field */
		for (iEcount = 0; (!((0xff == (HI704_Initial_Setting_Info_2[iEcount].address))
				     && (0xff == (HI704_Initial_Setting_Info_2[iEcount].data))));
		     iEcount++) {
			HI704_write_cmos_sensor(HI704_Initial_Setting_Info_2[iEcount].address,
						HI704_Initial_Setting_Info_2[iEcount].data);
		}
	} else if (sub_sensor_init_setting_switch == 3) {	/* FATP mode 3:SFR */
		for (iEcount = 0; (!((0xff == (HI704_Initial_Setting_Info_3[iEcount].address))
				     && (0xff == (HI704_Initial_Setting_Info_3[iEcount].data))));
		     iEcount++) {
			HI704_write_cmos_sensor(HI704_Initial_Setting_Info_3[iEcount].address,
						HI704_Initial_Setting_Info_3[iEcount].data);
		}
	}
/* HI704_Set_VGA_mode(); */
}

static void HI704_Init_Parameter(void)
{
	spin_lock(&hi704_yuv_drv_lock);
	HI704_sensor.first_init = KAL_TRUE;
	HI704_sensor.pv_mode = KAL_TRUE;
	HI704_sensor.night_mode = KAL_FALSE;
	HI704_sensor.MPEG4_Video_mode = KAL_FALSE;

	HI704_sensor.cp_pclk = HI704_sensor.pv_pclk;

	HI704_sensor.pv_dummy_pixels = 0;
	HI704_sensor.pv_dummy_lines = 0;
	HI704_sensor.cp_dummy_pixels = 0;
	HI704_sensor.cp_dummy_lines = 0;

	HI704_sensor.wb = 0;
	HI704_sensor.exposure = 0;
	HI704_sensor.effect = 0;
	HI704_sensor.banding = AE_FLICKER_MODE_50HZ;

	HI704_sensor.pv_line_length = 640;
	HI704_sensor.pv_frame_height = 480;
	HI704_sensor.cp_line_length = 640;
	HI704_sensor.cp_frame_height = 480;

	HI704_sensor.output_format = SENSOR_OUTPUT_FORMAT_YUYV;
	spin_unlock(&hi704_yuv_drv_lock);
}

static kal_uint8 HI704_power_on(void)
{
	kal_uint8 HI704_sensor_id = 0;

	spin_lock(&hi704_yuv_drv_lock);
	HI704_sensor.pv_pclk = 13000000;
	spin_unlock(&hi704_yuv_drv_lock);
	/* Software Reset */
	HI704_write_cmos_sensor(0x01, 0xf1);
	HI704_write_cmos_sensor(0x01, 0xf3);
	HI704_write_cmos_sensor(0x01, 0xf1);

	/* Read Sensor ID  */
	HI704_sensor_id = HI704_read_cmos_sensor(0x04);
	pr_debug("[HI704YUV]:read Sensor ID:%x\n", HI704_sensor_id);
	return HI704_sensor_id;
}


/*************************************************************************
* FUNCTION
*   HI704Open
*
* DESCRIPTION
*   This function initialize the registers of CMOS sensor
*
* PARAMETERS
*   None
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 HI704Open(void)
{
	spin_lock(&hi704_yuv_drv_lock);
	sensor_id_fail = 0;
	spin_unlock(&hi704_yuv_drv_lock);
	pr_debug("[Enter]:HI704 Open func:");

	if (HI704_power_on() != HI704_SENSOR_ID) {
		SENSORDB("[HI704]Error:read sensor ID fail\n");
		spin_lock(&hi704_yuv_drv_lock);
		sensor_id_fail = 1;
		spin_unlock(&hi704_yuv_drv_lock);
		return ERROR_SENSOR_CONNECT_FAIL;
	}

	/* Apply sensor initail setting */
	HI704_Initial_Setting();
	HI704_Init_Parameter();

	SENSORDB("[Exit]:HI704 Open func\n");
	return ERROR_NONE;
}				/* HI704Open() */

/*************************************************************************
* FUNCTION
*   HI704_GetSensorID
*
* DESCRIPTION
*   This function get the sensor ID
*
* PARAMETERS
*   None
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 HI704_GetSensorID(kal_uint32 *sensorID)
{
	pr_debug("[Enter]:HI704 GetSensorID func ");
	*sensorID = HI704_power_on();

	if (*sensorID != HI704_SENSOR_ID) {
		SENSORDB("[HI704]Error:read sensor ID fail\n");
		spin_lock(&hi704_yuv_drv_lock);
		sensor_id_fail = 1;
		spin_unlock(&hi704_yuv_drv_lock);
		*sensorID = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}

	return ERROR_NONE;
}				/* HI704Open  */


/*************************************************************************
* FUNCTION
*   HI704Close
*
* DESCRIPTION
*   This function is to turn off sensor module power.
*
* PARAMETERS
*   None
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 HI704Close(void)
{

	return ERROR_NONE;
}				/* HI704Close() */

static void HI704_Set_Mirror_Flip(kal_uint8 image_mirror)
{
    /********************************************************
    * Page Mode 0: Reg 0x0011 bit[1:0] = [Y Flip : X Flip]
    * 0: Off; 1: On.
    *********************************************************/
	kal_uint8 temp_data;

	SENSORDB("[Enter]:HI704 set Mirror_flip func:image_mirror=%d\n", image_mirror);

	HI704_write_cmos_sensor(0x03, 0x00);	/* Page 0 */
	temp_data = (HI704_read_cmos_sensor(0x11) & 0xfc);
	spin_lock(&hi704_yuv_drv_lock);
	/* HI704_sensor.mirror = (HI704_read_cmos_sensor(0x11) & 0xfc); */
	switch (image_mirror) {
	case IMAGE_NORMAL:
		/* HI704_sensor.mirror |= 0x00; */
		temp_data |= 0x00;
		break;
	case IMAGE_H_MIRROR:
		/* HI704_sensor.mirror |= 0x01; */
		temp_data |= 0x01;
		break;
	case IMAGE_V_MIRROR:
		/* HI704_sensor.mirror |= 0x02; */
		temp_data |= 0x02;
		break;
	case IMAGE_HV_MIRROR:
		/* HI704_sensor.mirror |= 0x03; */
		temp_data |= 0x03;
		break;
	default:
		/* HI704_sensor.mirror |= 0x00; */
		temp_data |= 0x00;
	}
	HI704_sensor.mirror = temp_data;
	spin_unlock(&hi704_yuv_drv_lock);
	HI704_write_cmos_sensor(0x11, HI704_sensor.mirror);
	SENSORDB("[Exit]:HI704 set Mirror_flip func\n");
}

#if 0
static void HI704_set_dummy(kal_uint16 dummy_pixels, kal_uint16 dummy_lines)
{
	HI704_write_cmos_sensor(0x03, 0x00);	/* Page 0 */
	HI704_write_cmos_sensor(0x40, ((dummy_pixels & 0x0F00)) >> 8);	/* HBLANK */
	HI704_write_cmos_sensor(0x41, (dummy_pixels & 0xFF));
	HI704_write_cmos_sensor(0x42, ((dummy_lines & 0xFF00) >> 8));	/* VBLANK ( Vsync Type 1) */
	HI704_write_cmos_sensor(0x43, (dummy_lines & 0xFF));
}
#endif

/* 640 * 480 */
static void HI704_Set_VGA_mode(void)
{
	HI704_write_cmos_sensor(0x01, HI704_read_cmos_sensor(0x01) | 0x01);	/* Sleep: For Write Reg */

	HI704_write_cmos_sensor(0x03, 0x00);
	HI704_write_cmos_sensor(0x10, 0x00);	/* VGA Size */

	HI704_write_cmos_sensor(0x20, 0x00);
	HI704_write_cmos_sensor(0x21, 0x04);	/* Window start X */


	HI704_write_cmos_sensor(0x40, 0x00);	/* HBLANK: 0x70 = 112 */
	HI704_write_cmos_sensor(0x41, 0x70);
	HI704_write_cmos_sensor(0x42, 0x00);	/* VBLANK: 0x04 = 4 */
	HI704_write_cmos_sensor(0x43, 0x15);

	HI704_write_cmos_sensor(0x03, 0x11);
	HI704_write_cmos_sensor(0x10, 0x25);

	HI704_write_cmos_sensor(0x03, 0x20);
	HI704_write_cmos_sensor(0x10, HI704_read_cmos_sensor(0x10) & 0x7f);	/* Close AE */
	HI704_write_cmos_sensor(0x18, HI704_read_cmos_sensor(0x18) | 0x08);	/* Reset AE */

	HI704_write_cmos_sensor(0x83, 0x00);
	HI704_write_cmos_sensor(0x84, 0xbe);
	HI704_write_cmos_sensor(0x85, 0x6e);
	HI704_write_cmos_sensor(0x86, 0x00);
	HI704_write_cmos_sensor(0x87, 0xfa);

	HI704_write_cmos_sensor(0x8b, 0x3f);
	HI704_write_cmos_sensor(0x8c, 0x7a);
	HI704_write_cmos_sensor(0x8d, 0x34);
	HI704_write_cmos_sensor(0x8e, 0xbc);

	HI704_write_cmos_sensor(0x9c, 0x0b);
	HI704_write_cmos_sensor(0x9d, 0xb8);
	HI704_write_cmos_sensor(0x9e, 0x00);
	HI704_write_cmos_sensor(0x9f, 0xfa);

	HI704_write_cmos_sensor(0x01, HI704_read_cmos_sensor(0x01) & 0xfe);	/* Exit Sleep: For Write Reg */

	HI704_write_cmos_sensor(0x03, 0x20);
	HI704_write_cmos_sensor(0x10, HI704_read_cmos_sensor(0x10) | 0x80);	/* Open AE */
	HI704_write_cmos_sensor(0x18, HI704_read_cmos_sensor(0x18) & 0xf7);	/* Reset AE */

}

static void HI704_Cal_Min_Frame_Rate(kal_uint16 min_framerate)
{
	kal_uint32 HI704_expmax = 0;
	kal_uint32 HI704_expbanding = 0;
	kal_uint32 temp_data;

	SENSORDB("[HI704] HI704_Cal_Min_Frame_Rate:min_fps=%d\n", min_framerate);

	if (sub_sensor_init_setting_switch == 0) {
		/* No Fixed Framerate */
		HI704_write_cmos_sensor(0x01, HI704_read_cmos_sensor(0x01) | 0x01);	/* Sleep: For Write Reg */
		HI704_write_cmos_sensor(0x03, 0x00);
		HI704_write_cmos_sensor(0x11, HI704_read_cmos_sensor(0x11) & 0xfb);

		HI704_write_cmos_sensor(0x03, 0x20);
		HI704_write_cmos_sensor(0x10, HI704_read_cmos_sensor(0x10) & 0x7f);	/* Close AE */

		HI704_write_cmos_sensor(0x11, 0x04);
		HI704_write_cmos_sensor(0x18, HI704_read_cmos_sensor(0x18) | 0x08);	/* Reset AE */
		HI704_write_cmos_sensor(0x2a, 0xf0);
		HI704_write_cmos_sensor(0x2b, 0x34);

		HI704_write_cmos_sensor(0x03, 0x00);
		temp_data = ((HI704_read_cmos_sensor(0x40) << 8) | HI704_read_cmos_sensor(0x41));
		spin_lock(&hi704_yuv_drv_lock);
		HI704_sensor.pv_dummy_pixels = temp_data;
		HI704_sensor.pv_line_length =
		    HI704_VGA_DEFAULT_PIXEL_NUMS + HI704_sensor.pv_dummy_pixels;
		spin_unlock(&hi704_yuv_drv_lock);

		if (HI704_sensor.banding == AE_FLICKER_MODE_50HZ) {
			HI704_expbanding =
			    (HI704_sensor.pv_pclk / HI704_sensor.pv_line_length / 100) *
			    HI704_sensor.pv_line_length / 8;
			HI704_expmax = HI704_expbanding * 100 * 10 / min_framerate;
		} else if (HI704_sensor.banding == AE_FLICKER_MODE_60HZ) {
			HI704_expbanding =
			    (HI704_sensor.pv_pclk / HI704_sensor.pv_line_length / 120) *
			    HI704_sensor.pv_line_length / 8;
			HI704_expmax = HI704_expbanding * 120 * 10 / min_framerate;
		} else {
			HI704_expbanding =
			    (HI704_sensor.pv_pclk / HI704_sensor.pv_line_length / 100) *
			    HI704_sensor.pv_line_length / 8;
			HI704_expmax = HI704_expbanding * 100 * 10 / min_framerate;
		}

		HI704_write_cmos_sensor(0x03, 0x20);
		HI704_write_cmos_sensor(0x88, (HI704_expmax >> 16) & 0xff);
		HI704_write_cmos_sensor(0x89, (HI704_expmax >> 8) & 0xff);
		HI704_write_cmos_sensor(0x8a, (HI704_expmax >> 0) & 0xff);

		HI704_write_cmos_sensor(0x01, HI704_read_cmos_sensor(0x01) & 0xfe);	/* Exit Sleep: For Write Reg */

		HI704_write_cmos_sensor(0x03, 0x20);
		HI704_write_cmos_sensor(0x10, HI704_read_cmos_sensor(0x10) | 0x80);	/* Open AE */
		HI704_write_cmos_sensor(0x18, HI704_read_cmos_sensor(0x18) & 0xf7);	/* Reset AE */
	}
}


static void HI704_Fix_Video_Frame_Rate(kal_uint16 fix_framerate)
{
	kal_uint32 HI704_expfix;
	kal_uint32 HI704_expfix_temp;
	kal_uint32 HI704_expmax = 0;
	kal_uint32 HI704_expbanding = 0;
	kal_uint32 temp_data1, temp_data2;

	SENSORDB("[Enter]HI704 Fix_video_frame_rate func: fix_fps=%d\n", fix_framerate);

	spin_lock(&hi704_yuv_drv_lock);
	HI704_sensor.video_current_frame_rate = fix_framerate;
	spin_unlock(&hi704_yuv_drv_lock);
	/* Fixed Framerate */
	if (sub_sensor_init_setting_switch == 0) {
		HI704_write_cmos_sensor(0x01, HI704_read_cmos_sensor(0x01) | 0x01);	/* Sleep: For Write Reg */

		HI704_write_cmos_sensor(0x03, 0x00);
		HI704_write_cmos_sensor(0x11, HI704_read_cmos_sensor(0x11) | 0x04);

		HI704_write_cmos_sensor(0x03, 0x20);
		HI704_write_cmos_sensor(0x10, HI704_read_cmos_sensor(0x10) & 0x7f);	/* Close AE */

		HI704_write_cmos_sensor(0x11, 0x00);
		HI704_write_cmos_sensor(0x18, HI704_read_cmos_sensor(0x18) | 0x08);	/* Reset AE */
		HI704_write_cmos_sensor(0x2a, 0x00);
		HI704_write_cmos_sensor(0x2b, 0x35);

		HI704_write_cmos_sensor(0x03, 0x00);
		temp_data1 = ((HI704_read_cmos_sensor(0x40) << 8) | HI704_read_cmos_sensor(0x41));
		temp_data2 = ((HI704_read_cmos_sensor(0x42) << 8) | HI704_read_cmos_sensor(0x43));
		spin_lock(&hi704_yuv_drv_lock);
		HI704_sensor.pv_dummy_pixels = temp_data1;
		HI704_sensor.pv_line_length =
		    HI704_VGA_DEFAULT_PIXEL_NUMS + HI704_sensor.pv_dummy_pixels;
		HI704_sensor.pv_dummy_lines = temp_data2;
		spin_unlock(&hi704_yuv_drv_lock);

		HI704_expfix_temp =
		    ((HI704_sensor.pv_pclk * 10 / fix_framerate) -
		     (HI704_sensor.pv_line_length * HI704_sensor.pv_dummy_lines)) / 8;
		HI704_expfix =
		    ((HI704_expfix_temp * 8 / HI704_sensor.pv_line_length) *
		     HI704_sensor.pv_line_length) / 8;

		HI704_write_cmos_sensor(0x03, 0x20);
		/* HI704_write_cmos_sensor(0x83, (HI704_expfix>>16)&0xff); */
		/* HI704_write_cmos_sensor(0x84, (HI704_expfix>>8)&0xff); */
		/* HI704_write_cmos_sensor(0x85, (HI704_expfix>>0)&0xff); */
		HI704_write_cmos_sensor(0x91, (HI704_expfix >> 16) & 0xff);
		HI704_write_cmos_sensor(0x92, (HI704_expfix >> 8) & 0xff);
		HI704_write_cmos_sensor(0x93, (HI704_expfix >> 0) & 0xff);

		if (HI704_sensor.banding == AE_FLICKER_MODE_50HZ) {
			HI704_expbanding =
			    ((HI704_read_cmos_sensor(0x8b) << 8) | HI704_read_cmos_sensor(0x8c));
		} else if (HI704_sensor.banding == AE_FLICKER_MODE_60HZ) {
			HI704_expbanding =
			    ((HI704_read_cmos_sensor(0x8d) << 8) | HI704_read_cmos_sensor(0x8e));
		} else {
			HI704_expbanding =
			    ((HI704_read_cmos_sensor(0x8b) << 8) | HI704_read_cmos_sensor(0x8c));

			/* SENSORDB("[HI704]Wrong Banding Setting!!!..."); */
		}
		HI704_expmax =
		    ((HI704_expfix_temp - HI704_expbanding) / HI704_expbanding) * HI704_expbanding;

		HI704_write_cmos_sensor(0x03, 0x20);
		HI704_write_cmos_sensor(0x88, (HI704_expmax >> 16) & 0xff);
		HI704_write_cmos_sensor(0x89, (HI704_expmax >> 8) & 0xff);
		HI704_write_cmos_sensor(0x8a, (HI704_expmax >> 0) & 0xff);

		HI704_write_cmos_sensor(0x01, HI704_read_cmos_sensor(0x01) & 0xfe);	/* Exit Sleep: For Write Reg */

		HI704_write_cmos_sensor(0x03, 0x20);
		HI704_write_cmos_sensor(0x10, HI704_read_cmos_sensor(0x10) | 0x80);	/* Open AE */
		HI704_write_cmos_sensor(0x18, HI704_read_cmos_sensor(0x18) & 0xf7);	/* Reset AE */
	}
}

#if 0
/* 320 * 240 */
static void HI704_Set_QVGA_mode(void)
{
	HI704_write_cmos_sensor(0x01, HI704_read_cmos_sensor(0x01) | 0x01);	/* Sleep: For Write Reg */

	HI704_write_cmos_sensor(0x03, 0x00);
	HI704_write_cmos_sensor(0x10, 0x01);	/* QVGA Size: 0x10 -> 0x01 */

	HI704_write_cmos_sensor(0x20, 0x00);
	HI704_write_cmos_sensor(0x21, 0x02);

	HI704_write_cmos_sensor(0x40, 0x01);	/* HBLANK:  0x0158 = 344 */
	HI704_write_cmos_sensor(0x41, 0x58);
	HI704_write_cmos_sensor(0x42, 0x00);	/* VBLANK:  0x14 = 20 */
	HI704_write_cmos_sensor(0x43, 0x14);

	HI704_write_cmos_sensor(0x03, 0x11);	/* QVGA Fixframerate */
	HI704_write_cmos_sensor(0x10, 0x21);

	HI704_write_cmos_sensor(0x03, 0x20);
	HI704_write_cmos_sensor(0x10, HI704_read_cmos_sensor(0x10) & 0x7f);	/* Close AE */
	HI704_write_cmos_sensor(0x18, HI704_read_cmos_sensor(0x18) | 0x08);	/* Reset AE */

	HI704_write_cmos_sensor(0x83, 0x00);
	HI704_write_cmos_sensor(0x84, 0xaf);
	HI704_write_cmos_sensor(0x85, 0xc8);
	HI704_write_cmos_sensor(0x86, 0x00);
	HI704_write_cmos_sensor(0x87, 0xfa);

	HI704_write_cmos_sensor(0x8b, 0x3a);
	HI704_write_cmos_sensor(0x8c, 0x98);
	HI704_write_cmos_sensor(0x8d, 0x30);
	HI704_write_cmos_sensor(0x8e, 0xd4);

	HI704_write_cmos_sensor(0x9c, 0x0b);
	HI704_write_cmos_sensor(0x9d, 0x3b);
	HI704_write_cmos_sensor(0x9e, 0x00);
	HI704_write_cmos_sensor(0x9f, 0xfa);

	HI704_write_cmos_sensor(0x01, HI704_read_cmos_sensor(0x01) & 0xfe);	/* Exit Sleep: For Write Reg */

	HI704_write_cmos_sensor(0x03, 0x20);
	HI704_write_cmos_sensor(0x10, HI704_read_cmos_sensor(0x10) | 0x80);	/* Open AE */
	HI704_write_cmos_sensor(0x18, HI704_read_cmos_sensor(0x18) & 0xf7);	/* Reset AE */

}
#endif
void HI704_night_mode(kal_bool enable)
{
	SENSORDB("[Enter]HI704 night mode func:enable = %d\n", enable);
	SENSORDB("HI704_sensor.video_mode = %d\n", HI704_sensor.MPEG4_Video_mode);
	SENSORDB("HI704_sensor.night_mode = %d\n", HI704_sensor.night_mode);
	spin_lock(&hi704_yuv_drv_lock);
	HI704_sensor.night_mode = enable;
	spin_unlock(&hi704_yuv_drv_lock);

	if (HI704_sensor.MPEG4_Video_mode == KAL_TRUE)
		return;

	if (enable)
		HI704_Cal_Min_Frame_Rate(HI704_MIN_FRAMERATE_5);
	else
		HI704_Cal_Min_Frame_Rate(HI704_MIN_FRAMERATE_10);
}

/*************************************************************************
* FUNCTION
*   HI704Preview
*
* DESCRIPTION
*   This function start the sensor preview.
*
* PARAMETERS
*   *image_window : address pointer of pixel numbers in one period of HSYNC
*  *sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static UINT32 HI704Preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&hi704_yuv_drv_lock);
	sensor_config_data->SensorImageMirror = IMAGE_HV_MIRROR;
	if (HI704_sensor.first_init == KAL_TRUE)
		HI704_sensor.MPEG4_Video_mode = HI704_sensor.MPEG4_Video_mode;
	else
		HI704_sensor.MPEG4_Video_mode = !HI704_sensor.MPEG4_Video_mode;

	spin_unlock(&hi704_yuv_drv_lock);

	SENSORDB("[Enter]:HI704 preview func:");
	SENSORDB("HI704_sensor.video_mode = %d\n", HI704_sensor.MPEG4_Video_mode);

	spin_lock(&hi704_yuv_drv_lock);
	HI704_sensor.first_init = KAL_FALSE;
	HI704_sensor.pv_mode = KAL_TRUE;
	spin_unlock(&hi704_yuv_drv_lock);

	{
		SENSORDB("[HI704]preview set_VGA_mode\n");
		/* HI704_Set_VGA_mode(); */
	}

	HI704_Set_Mirror_Flip(sensor_config_data->SensorImageMirror);

	SENSORDB("[Exit]:HI704 preview func\n");
	return TRUE;
}				/* HI704_Preview */


UINT32 HI704Capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		    MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	SENSORDB("[HI704][Enter]HI704_capture_func\n");
	spin_lock(&hi704_yuv_drv_lock);
	HI704_sensor.pv_mode = KAL_FALSE;
	spin_unlock(&hi704_yuv_drv_lock);
	sensor_config_data->SensorImageMirror = IMAGE_HV_MIRROR;
	/* HI704_Set_VGA_mode(); */
	HI704_Set_Mirror_Flip(sensor_config_data->SensorImageMirror);

	return ERROR_NONE;
}				/* HM3451Capture() */


UINT32 HI704GetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution)
{
	pr_debug("[Enter]:HI704 get Resolution func\n");

	pSensorResolution->SensorFullWidth = HI704_IMAGE_SENSOR_FULL_WIDTH;
	pSensorResolution->SensorFullHeight = HI704_IMAGE_SENSOR_FULL_HEIGHT;
	pSensorResolution->SensorPreviewWidth = HI704_IMAGE_SENSOR_PV_WIDTH;
	pSensorResolution->SensorPreviewHeight = HI704_IMAGE_SENSOR_PV_HEIGHT;
	pSensorResolution->SensorVideoWidth = HI704_IMAGE_SENSOR_PV_WIDTH;
	pSensorResolution->SensorVideoHeight = HI704_IMAGE_SENSOR_PV_HEIGHT;
	pSensorResolution->Sensor3DFullWidth = HI704_IMAGE_SENSOR_FULL_WIDTH - 10;
	pSensorResolution->Sensor3DFullHeight = HI704_IMAGE_SENSOR_FULL_HEIGHT - 10 - 10;
	pSensorResolution->Sensor3DPreviewWidth = HI704_IMAGE_SENSOR_PV_WIDTH - 16;
	pSensorResolution->Sensor3DPreviewHeight = HI704_IMAGE_SENSOR_PV_HEIGHT - 12 - 10;
	pSensorResolution->Sensor3DVideoWidth = HI704_IMAGE_SENSOR_PV_WIDTH - 16;
	pSensorResolution->Sensor3DVideoHeight = HI704_IMAGE_SENSOR_PV_HEIGHT - 12 - 10;

	pr_debug("[Exit]:HI704 get Resolution func\n");
	return ERROR_NONE;
}				/* HI704GetResolution() */

UINT32 HI704GetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId,
		    MSDK_SENSOR_INFO_STRUCT *pSensorInfo,
		    MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
	pr_err("[Enter]:HI704 getInfo func:ScenarioId = %d\n", ScenarioId);

	pSensorInfo->SensorPreviewResolutionX = HI704_IMAGE_SENSOR_PV_WIDTH;
	pSensorInfo->SensorPreviewResolutionY = HI704_IMAGE_SENSOR_PV_HEIGHT;
	pSensorInfo->SensorFullResolutionX = HI704_IMAGE_SENSOR_FULL_WIDTH;
	pSensorInfo->SensorFullResolutionY = HI704_IMAGE_SENSOR_FULL_HEIGHT;

	/* pSensorInfo->SensorDriver3D=0; */
	/* pSensorInfo->SensorMIPILaneNumber=SENSOR_MIPI_4LANE; */
	pSensorInfo->SensorCameraPreviewFrameRate = 30;
	pSensorInfo->SensorVideoFrameRate = 30;
	pSensorInfo->SensorStillCaptureFrameRate = 30;
	pSensorInfo->SensorWebCamCaptureFrameRate = 15;
	pSensorInfo->SensorResetActiveHigh = FALSE;	/* low is to reset */
	pSensorInfo->SensorResetDelayCount = 4;	/* 4ms */
	pSensorInfo->SensorOutputDataFormat = HI704_sensor.output_format;
	pr_err("pSensorInfo->SensorOutputDataFormat = 0x%x\n", pSensorInfo->SensorOutputDataFormat);
	pSensorInfo->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	pSensorInfo->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
	pSensorInfo->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	pSensorInfo->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	pSensorInfo->SensorInterruptDelayLines = 1;
	pSensorInfo->SensroInterfaceType = SENSOR_INTERFACE_TYPE_PARALLEL;


	pSensorInfo->CaptureDelayFrame = 1;
	pSensorInfo->PreviewDelayFrame = 1;	/* 10; */
	pSensorInfo->VideoDelayFrame = 0;
	pSensorInfo->SensorMasterClockSwitch = 0;
	pSensorInfo->SensorDrivingCurrent = ISP_DRIVING_8MA;

	pSensorInfo->YUVAwbDelayFrame = 2;
	pSensorInfo->YUVEffectDelayFrame = 2;

	switch (ScenarioId) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
	case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW:
	case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO:
		pSensorInfo->SensorClockFreq = 24;	/* 26; */
		pSensorInfo->SensorClockDividCount = 3;
		pSensorInfo->SensorClockRisingCount = 0;
		pSensorInfo->SensorClockFallingCount = 2;
		pSensorInfo->SensorPixelClockCount = 3;
		pSensorInfo->SensorDataLatchCount = 2;
		pSensorInfo->SensorGrabStartX = 0;
		pSensorInfo->SensorGrabStartY = 1;
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	case MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE:
		pSensorInfo->SensorClockFreq = 24;	/* 26; */
		pSensorInfo->SensorClockDividCount = 3;
		pSensorInfo->SensorClockRisingCount = 0;
		pSensorInfo->SensorClockFallingCount = 2;
		pSensorInfo->SensorPixelClockCount = 3;
		pSensorInfo->SensorDataLatchCount = 2;
		pSensorInfo->SensorGrabStartX = 0;
		pSensorInfo->SensorGrabStartY = 1;
		break;
	default:
		pSensorInfo->SensorClockFreq = 24;	/* 26; */
		pSensorInfo->SensorClockDividCount = 3;
		pSensorInfo->SensorClockRisingCount = 0;
		pSensorInfo->SensorClockFallingCount = 2;
		pSensorInfo->SensorPixelClockCount = 3;
		pSensorInfo->SensorDataLatchCount = 2;
		pSensorInfo->SensorGrabStartX = 0;
		pSensorInfo->SensorGrabStartY = 1;
		break;
	}
	/* HI704_PixelClockDivider=pSensorInfo->SensorPixelClockCount; */
	memcpy(pSensorConfigData, &HI704SensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));

	pr_debug("[Exit]:HI704 getInfo func\n");
	return ERROR_NONE;
}				/* HI704GetInfo() */


UINT32 HI704Control(MSDK_SCENARIO_ID_ENUM ScenarioId,
		    MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow,
		    MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
	SENSORDB("[Enter]:HI704 Control func:ScenarioId = %d\n", ScenarioId);

	switch (ScenarioId) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		/* case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW: */
		/* case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO: */
		HI704Preview(pImageWindow, pSensorConfigData);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	case MSDK_SCENARIO_ID_CAMERA_ZSD:
		HI704Capture(pImageWindow, pSensorConfigData);
		break;
	default:
		break;
	}

	SENSORDB("[Exit]:HI704 Control func\n");
	return ERROR_NONE;
}				/* HI704Control() */


/*************************************************************************
* FUNCTION
*   HI704_set_param_wb
*
* DESCRIPTION
*   wb setting.
*
* PARAMETERS
*   none
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
BOOL HI704_set_param_wb(UINT16 para)
{
	/* This sensor need more time to balance AWB, */
	/* we suggest higher fps or drop some frame to avoid garbage color when preview initial */
	SENSORDB("[Enter]HI704 set_param_wb func:para = %d\n", para);

	if (HI704_sensor.wb == para)
		return KAL_TRUE;

	spin_lock(&hi704_yuv_drv_lock);
	HI704_sensor.wb = para;
	spin_unlock(&hi704_yuv_drv_lock);

	switch (para) {
	case AWB_MODE_AUTO:
		{
			HI704_write_cmos_sensor(0x03, 0x22);
			HI704_write_cmos_sensor(0x11, 0x26);
			HI704_write_cmos_sensor(0x80, 0x1b);
			HI704_write_cmos_sensor(0x82, 0x50);
			HI704_write_cmos_sensor(0x83, 0x55);
			HI704_write_cmos_sensor(0x84, 0x1b);
			HI704_write_cmos_sensor(0x85, 0x52);
			HI704_write_cmos_sensor(0x86, 0x20);
			HI704_write_cmos_sensor(0x10, 0xfb);
		}
		break;
	case AWB_MODE_CLOUDY_DAYLIGHT:
		{
			HI704_write_cmos_sensor(0x03, 0x22);
			HI704_write_cmos_sensor(0x11, 0x28);
			HI704_write_cmos_sensor(0x80, 0x71);
			HI704_write_cmos_sensor(0x82, 0x2b);
			HI704_write_cmos_sensor(0x83, 0x72);
			HI704_write_cmos_sensor(0x84, 0x70);
			HI704_write_cmos_sensor(0x85, 0x2b);
			HI704_write_cmos_sensor(0x86, 0x28);
			HI704_write_cmos_sensor(0x10, 0xfb);
		}
		break;
	case AWB_MODE_DAYLIGHT:
		{
			HI704_write_cmos_sensor(0x03, 0x22);
			HI704_write_cmos_sensor(0x11, 0x28);
			HI704_write_cmos_sensor(0x80, 0x59);
			HI704_write_cmos_sensor(0x82, 0x29);
			HI704_write_cmos_sensor(0x83, 0x60);
			HI704_write_cmos_sensor(0x84, 0x50);
			HI704_write_cmos_sensor(0x85, 0x2f);
			HI704_write_cmos_sensor(0x86, 0x23);
			HI704_write_cmos_sensor(0x10, 0xfb);
		}
		break;
	case AWB_MODE_INCANDESCENT:
		{
			HI704_write_cmos_sensor(0x03, 0x22);
			HI704_write_cmos_sensor(0x11, 0x28);
			HI704_write_cmos_sensor(0x80, 0x29);
			HI704_write_cmos_sensor(0x82, 0x54);
			HI704_write_cmos_sensor(0x83, 0x2e);
			HI704_write_cmos_sensor(0x84, 0x23);
			HI704_write_cmos_sensor(0x85, 0x58);
			HI704_write_cmos_sensor(0x86, 0x4f);
			HI704_write_cmos_sensor(0x10, 0xfb);
		}
		break;
	case AWB_MODE_FLUORESCENT:
		{
			HI704_write_cmos_sensor(0x03, 0x22);
			HI704_write_cmos_sensor(0x11, 0x28);
			HI704_write_cmos_sensor(0x80, 0x41);
			HI704_write_cmos_sensor(0x82, 0x42);
			HI704_write_cmos_sensor(0x83, 0x44);
			HI704_write_cmos_sensor(0x84, 0x34);
			HI704_write_cmos_sensor(0x85, 0x46);
			HI704_write_cmos_sensor(0x86, 0x3a);
			HI704_write_cmos_sensor(0x10, 0xfb);
		}
		break;
	case AWB_MODE_TUNGSTEN:
		{
			HI704_write_cmos_sensor(0x03, 0x22);
			HI704_write_cmos_sensor(0x80, 0x24);
			HI704_write_cmos_sensor(0x81, 0x20);
			HI704_write_cmos_sensor(0x82, 0x58);
			HI704_write_cmos_sensor(0x83, 0x27);
			HI704_write_cmos_sensor(0x84, 0x22);
			HI704_write_cmos_sensor(0x85, 0x58);
			HI704_write_cmos_sensor(0x86, 0x52);
			HI704_write_cmos_sensor(0x10, 0xfb);
		}
		break;
	case AWB_MODE_OFF:
		{
			SENSORDB("HI704 AWB OFF");
			HI704_write_cmos_sensor(0x03, 0x22);
			HI704_write_cmos_sensor(0x10, 0xe2);
		}
		break;
	default:
		return FALSE;
	}

	return TRUE;
}				/* HI704_set_param_wb */

/*************************************************************************
* FUNCTION
*   HI704_set_param_effect
*
* DESCRIPTION
*   effect setting.
*
* PARAMETERS
*   none
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
BOOL HI704_set_param_effect(UINT16 para)
{
	SENSORDB("[Enter]HI704 set_param_effect func:para = %d\n", para);

	if (HI704_sensor.effect == para)
		return KAL_TRUE;

	spin_lock(&hi704_yuv_drv_lock);
	HI704_sensor.effect = para;
	spin_unlock(&hi704_yuv_drv_lock);

	switch (para) {
	case MEFFECT_OFF:
		{
			HI704_write_cmos_sensor(0x03, 0x10);
			HI704_write_cmos_sensor(0x11, 0x03);
			HI704_write_cmos_sensor(0x12, 0x30);
			HI704_write_cmos_sensor(0x13, 0x00);
			HI704_write_cmos_sensor(0x44, 0x80);
			HI704_write_cmos_sensor(0x45, 0x80);

			HI704_write_cmos_sensor(0x47, 0x7f);
			HI704_write_cmos_sensor(0x03, 0x13);
			HI704_write_cmos_sensor(0x20, 0x07);
			HI704_write_cmos_sensor(0x21, 0x07);
		}
		break;
	case MEFFECT_SEPIA:
		{
			HI704_write_cmos_sensor(0x03, 0x10);
			HI704_write_cmos_sensor(0x11, 0x03);
			HI704_write_cmos_sensor(0x12, 0x23);
			HI704_write_cmos_sensor(0x13, 0x00);
			HI704_write_cmos_sensor(0x44, 0x70);
			HI704_write_cmos_sensor(0x45, 0x98);

			HI704_write_cmos_sensor(0x47, 0x7f);
			HI704_write_cmos_sensor(0x03, 0x13);
			HI704_write_cmos_sensor(0x20, 0x07);
			HI704_write_cmos_sensor(0x21, 0x07);
		}
		break;
	case MEFFECT_NEGATIVE:
		{
			HI704_write_cmos_sensor(0x03, 0x10);
			HI704_write_cmos_sensor(0x11, 0x03);
			HI704_write_cmos_sensor(0x12, 0x08);
			HI704_write_cmos_sensor(0x13, 0x00);
			HI704_write_cmos_sensor(0x14, 0x00);
		}
		break;
	case MEFFECT_SEPIAGREEN:
		{
			HI704_write_cmos_sensor(0x03, 0x10);
			HI704_write_cmos_sensor(0x11, 0x03);
			HI704_write_cmos_sensor(0x12, 0x03);
			/* HI704_write_cmos_sensor(0x40, 0x00); */
			HI704_write_cmos_sensor(0x13, 0x00);
			HI704_write_cmos_sensor(0x44, 0x30);
			HI704_write_cmos_sensor(0x45, 0x50);
		}
		break;
	case MEFFECT_SEPIABLUE:
		{
			HI704_write_cmos_sensor(0x03, 0x10);
			HI704_write_cmos_sensor(0x11, 0x03);
			HI704_write_cmos_sensor(0x12, 0x03);
			/* HI704_write_cmos_sensor(0x40, 0x00); */
			HI704_write_cmos_sensor(0x13, 0x00);
			HI704_write_cmos_sensor(0x44, 0xb0);
			HI704_write_cmos_sensor(0x45, 0x40);
		}
		break;
	case MEFFECT_MONO:
		{
			HI704_write_cmos_sensor(0x03, 0x10);
			HI704_write_cmos_sensor(0x11, 0x03);
			HI704_write_cmos_sensor(0x12, 0x03);
			/* HI704_write_cmos_sensor(0x40, 0x00); */
			HI704_write_cmos_sensor(0x44, 0x80);
			HI704_write_cmos_sensor(0x45, 0x80);
		}
		break;
	default:
		return KAL_FALSE;
	}

	return KAL_TRUE;
}				/* HI704_set_param_effect */

/*************************************************************************
* FUNCTION
*   HI704_set_param_banding
*
* DESCRIPTION
*   banding setting.
*
* PARAMETERS
*   none
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
BOOL HI704_set_param_banding(UINT16 para)
{
	SENSORDB("[Enter]HI704 set_param_banding func:para = %d\n", para);

	if (HI704_sensor.banding == para)
		return KAL_TRUE;

	spin_lock(&hi704_yuv_drv_lock);
	HI704_sensor.banding = para;
	spin_unlock(&hi704_yuv_drv_lock);

	switch (para) {
/*	case AE_FLICKER_MODE_50HZ:
		{
			HI704_write_cmos_sensor(0x03, 0x20);
			HI704_write_cmos_sensor(0x10, 0x9c);
		}
		break;
	case AE_FLICKER_MODE_60HZ:
		{
			HI704_write_cmos_sensor(0x03, 0x20);
			HI704_write_cmos_sensor(0x10, 0x8c);
		}
		break; */


	case AE_FLICKER_MODE_50HZ:
		{
			HI704_write_cmos_sensor(0x03, 0x20);
			HI704_write_cmos_sensor(0x18, 0x38);
			HI704_write_cmos_sensor(0x83, 0x00);
			HI704_write_cmos_sensor(0x84, 0xaf);
			HI704_write_cmos_sensor(0x85, 0xc8);
			HI704_write_cmos_sensor(0x88, 0x02);
			HI704_write_cmos_sensor(0x89, 0xbf);
			HI704_write_cmos_sensor(0x8a, 0x20);
			HI704_write_cmos_sensor(0x18, 0x30);
			HI704_write_cmos_sensor(0x10, 0x9c);
		}
		break;
	case AE_FLICKER_MODE_60HZ:
		{
			HI704_write_cmos_sensor(0x03, 0x20);
			HI704_write_cmos_sensor(0x18, 0x38);
			HI704_write_cmos_sensor(0x83, 0x00);
			HI704_write_cmos_sensor(0x84, 0xc3);
			HI704_write_cmos_sensor(0x85, 0x50);
			HI704_write_cmos_sensor(0x88, 0x02);
			HI704_write_cmos_sensor(0x89, 0xdc);
			HI704_write_cmos_sensor(0x8a, 0x6c);
			HI704_write_cmos_sensor(0x18, 0x30);
			HI704_write_cmos_sensor(0x10, 0x8c);
		}
		break;			
	case AE_FLICKER_MODE_AUTO:
		{
			HI704_write_cmos_sensor(0x03, 0x20);
			HI704_write_cmos_sensor(0x18, 0x38);
			HI704_write_cmos_sensor(0x83, 0x00);
			HI704_write_cmos_sensor(0x84, 0xaf);
			HI704_write_cmos_sensor(0x85, 0xc8);
			HI704_write_cmos_sensor(0x88, 0x02);
			HI704_write_cmos_sensor(0x89, 0xbf);
			HI704_write_cmos_sensor(0x8a, 0x20);
			HI704_write_cmos_sensor(0x18, 0x30);
			HI704_write_cmos_sensor(0x10, 0x9c);
		}
		break;
	default:
		return KAL_FALSE;
	}

	return KAL_TRUE;
}				/* HI704_set_param_banding */




/*************************************************************************
* FUNCTION
*   HI704_set_param_exposure
*
* DESCRIPTION
*   exposure setting.
*
* PARAMETERS
*   none
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
BOOL HI704_set_param_exposure(UINT16 para)
{
	SENSORDB("[Enter]HI704 set_param_exposure func:para = %d\n", para);

	if (HI704_sensor.exposure == para)
		return KAL_TRUE;

	spin_lock(&hi704_yuv_drv_lock);
	HI704_sensor.exposure = para;
	spin_unlock(&hi704_yuv_drv_lock);

	HI704_write_cmos_sensor(0x03, 0x10);
	HI704_write_cmos_sensor(0x12, HI704_read_cmos_sensor(0x12) | 0x10);
	switch (para) {
	case AE_EV_COMP_13:	/* +4 EV */
		HI704_write_cmos_sensor(0x40, 0x60);
		break;
	case AE_EV_COMP_30:	/* +3 EV */
		HI704_write_cmos_sensor(0x40, 0x48);
		break;
	case AE_EV_COMP_20:	/* +2 EV */
		HI704_write_cmos_sensor(0x40, 0x30);
		break;
	case AE_EV_COMP_10:	/* +1 EV */
		HI704_write_cmos_sensor(0x40, 0x18);
		break;
	case AE_EV_COMP_00:	/* +0 EV */
		HI704_write_cmos_sensor(0x40, 0x0);
		break;
	case AE_EV_COMP_n10:	/* -1 EV */
		HI704_write_cmos_sensor(0x40, 0x98);
		break;
	case AE_EV_COMP_n20:	/* -2 EV */
		HI704_write_cmos_sensor(0x40, 0xb0);
		break;
	case AE_EV_COMP_n30:	/* -3 EV */
		HI704_write_cmos_sensor(0x40, 0xc8);
		break;
	case AE_EV_COMP_n13:	/* -4 EV */
		HI704_write_cmos_sensor(0x40, 0xe0);
		break;
	default:
		return FALSE;
	}

	return TRUE;
}				/* HI704_set_param_exposure */

void HI704_set_AE_mode(UINT32 iPara)
{
	UINT8 temp_AE_reg = 0;

	SENSORDB("HI704_set_AE_mode = %d E\n", iPara);
	HI704_write_cmos_sensor(0x03, 0x20);
	temp_AE_reg = HI704_read_cmos_sensor(0x10);

	if (AE_MODE_OFF == iPara) {
		/* turn off AEC/AGC */
		HI704_write_cmos_sensor(0x10, temp_AE_reg & ~0x10);
	} else {
		HI704_write_cmos_sensor(0x10, temp_AE_reg | 0x10);
	}
}

void HI704_AE_Set_Window(uintptr_t zone_addr, UINT32 prevW, UINT32 prevH)
{
	UINT32 x0, y0, x1, y1, width, height;
	UINT32 *ptr = (UINT32 *) zone_addr;
	UINT32 srcW_maxW;	/* touch coordinate max width */
	UINT32 srcW_maxH;	/* touch coordinate max Height */

	x0 = *ptr;
	y0 = *(ptr + 1);
	x1 = *(ptr + 2);
	y1 = *(ptr + 3);
	width = *(ptr + 4);
	height = *(ptr + 5);
	srcW_maxW = width;
	srcW_maxH = height;

	SENSORDB("[%s]: (%d,%d)~(%d,%d)\n", __func__, x0, y0, x1, y1);

	/* No Touch AE, return to default setting */
	if ((x0 == x1) && (y0 == y1)) {
		HI704_write_cmos_sensor(0x03, 0x20);
		HI704_write_cmos_sensor(0x60, 0x95);	/* AEWGT */
		HI704_write_cmos_sensor(0x68, 0x20);
		HI704_write_cmos_sensor(0x69, 0x80);
		HI704_write_cmos_sensor(0x6A, 0x20);
		HI704_write_cmos_sensor(0x6B, 0xC8);
		mdelay(1);
		SENSORDB("[%s] Default: (%d,%d)~(%d,%d)\n", __func__, x0, y0, x1, y1);
		return;
	}

	/* boundary check */
	if (0 == width)
		width = 320;
	if (0 == height)
		height = 240;
	if (width > prevW)
		width = prevW;
	if (height > prevH)
		height = prevH;
	if (x0 >= srcW_maxW)
		x0 = srcW_maxW - 1;
	if (x1 >= srcW_maxW)
		x1 = srcW_maxW - 1;
	if (y0 >= srcW_maxH)
		y0 = srcW_maxH - 1;
	if (y1 >= srcW_maxH)
		y1 = srcW_maxH - 1;

	srcW_maxW = width;
	srcW_maxH = height;

	/* Rescale */
	x0 = x0 * (prevW / srcW_maxW);
	y0 = y0 * (prevH / srcW_maxH);
	x1 = x1 * (prevW / srcW_maxW);
	y1 = y1 * (prevH / srcW_maxH);

	/* Set AE window */
	HI704_write_cmos_sensor(0x03, 0x20);
	HI704_write_cmos_sensor(0x60, 0xC0);	/* AEWGT */

	/*
	   HI704_write_cmos_sensor(0x68, x0/4);
	   HI704_write_cmos_sensor(0x69, x1/4);
	   HI704_write_cmos_sensor(0x6A, y0/2);
	   HI704_write_cmos_sensor(0x6B, y1/2);
	 */

	/* HV mirror the coordinates */
	SENSORDB("[%s] Remapping : (%d,%d)~(%d,%d)\n", __func__, (prevW - x1), (prevH - y1),
		 (prevW - x0), (prevH - y0));
	HI704_write_cmos_sensor(0x68, (prevW - x1) / 4);
	HI704_write_cmos_sensor(0x69, (prevW - x0) / 4);
	HI704_write_cmos_sensor(0x6A, (prevH - y1) / 2);
	HI704_write_cmos_sensor(0x6B, (prevH - y0) / 2);

	mdelay(1);
}

UINT32 HI704YUVSensorSetting(FEATURE_ID iCmd, UINT32 iPara)
{
	SENSORDB("[Enter]HI704YUVSensorSetting func:cmd = %d , iPara = 0x%x\n", iCmd, iPara);

	switch (iCmd) {
	case FID_SCENE_MODE:	/* auto mode or night mode */
		if (iPara == SCENE_MODE_OFF) {	/* auto mode */
			HI704_night_mode(FALSE);
		} else if (iPara == SCENE_MODE_NIGHTSCENE) {	/* night mode */
			HI704_night_mode(TRUE);
		}
		break;
	case FID_AWB_MODE:
		if (sub_sensor_init_setting_switch == 0)
			HI704_set_param_wb(iPara);
		break;
	case FID_COLOR_EFFECT:
		HI704_set_param_effect(iPara);
		break;
	case FID_AE_EV:
		HI704_set_param_exposure(iPara);
		break;
	case FID_AE_FLICKER:
		if (sub_sensor_init_setting_switch == 0)
			HI704_set_param_banding(iPara);
		break;
	case FID_ZOOM_FACTOR:
		spin_lock(&hi704_yuv_drv_lock);
		HI704_zoom_factor = iPara;
		spin_unlock(&hi704_yuv_drv_lock);
		break;
	case FID_AE_SCENE_MODE:
		HI704_set_AE_mode(iPara);
		break;
	default:
		break;
	}
	return TRUE;
}				/* HI704YUVSensorSetting */

UINT32 HI704YUVSetVideoMode(UINT16 u2FrameRate)
{
	spin_lock(&hi704_yuv_drv_lock);
	HI704_sensor.MPEG4_Video_mode = KAL_TRUE;
	spin_unlock(&hi704_yuv_drv_lock);
	SENSORDB("[Enter]HI704 Set Video Mode:FrameRate= %d\n", u2FrameRate);
	SENSORDB("HI704_sensor.video_mode = %d\n", HI704_sensor.MPEG4_Video_mode);

	if (u2FrameRate == 30)
		u2FrameRate = 20;

	spin_lock(&hi704_yuv_drv_lock);
	HI704_sensor.fix_framerate = u2FrameRate * 10;
	spin_unlock(&hi704_yuv_drv_lock);

	if (HI704_sensor.fix_framerate <= 300)
		HI704_Fix_Video_Frame_Rate(HI704_sensor.fix_framerate);
	else
		SENSORDB("Wrong Frame Rate");

	return TRUE;
}

void HI704GetAFMaxNumFocusAreas(UINT32 *pFeatureReturnPara32)
{
	*pFeatureReturnPara32 = 0;
	SENSORDB("HI704GetAFMaxNumFocusAreas *pFeatureReturnPara32 = %d\n", *pFeatureReturnPara32);
}

void HI704GetAEMaxNumMeteringAreas(UINT32 *pFeatureReturnPara32)
{
	*pFeatureReturnPara32 = 1;
	SENSORDB("HI704GetAEMaxNumMeteringAreas *pFeatureReturnPara32 = %d\n",
		 *pFeatureReturnPara32);
}

UINT32 HI704GetExposureTime(void)
{
	UINT32 ExposureTimems = 0, freq = 0;
	UINT32 exp_time = 0, internal_exp_time = 0;

	HI704_write_cmos_sensor(0x03, 0x20);
	if (HI704_sensor.banding == AE_FLICKER_MODE_50HZ) {
		exp_time = ((HI704_read_cmos_sensor(0x8b) << 8) | HI704_read_cmos_sensor(0x8c));
		freq = 100;
	} else {
		exp_time = ((HI704_read_cmos_sensor(0x8d) << 8) | HI704_read_cmos_sensor(0x8e));
		freq = 120;
	}
	internal_exp_time = ((HI704_read_cmos_sensor(0x80) << 16) |
			     HI704_read_cmos_sensor(0x81) << 8 | HI704_read_cmos_sensor(0x82));
	ExposureTimems = (internal_exp_time / exp_time * 1000 / freq) * 1000;	/* Unit : us */
	SENSORDB("[HI704GetExposureTime] ExposureTimems : %d ms\n", ExposureTimems);

	return ExposureTimems;
}

/*
    get ISO value and speed
    you can just use the return value, means ISO value
*/
UINT32 HI704GetISOValueAndSpeed(UINT32 *iso_speed, UINT32 *iso_value)
{
	UINT32 temp_iso_val = 0;
	UINT32 map_table[4] = { 0x28, 0x58, 0x88, 0xB8 };

	*iso_speed = AE_ISO_100;

	/* get iso value */
	HI704_write_cmos_sensor(0x03, 0x20);
	temp_iso_val = HI704_read_cmos_sensor(0xB0);	/* read gain value */

	/* map gain to AE ISO value  */
	if (temp_iso_val <= 0x28)
		*iso_value = 100;
	else if (temp_iso_val > 0x28 && temp_iso_val <= 0x38)
		*iso_value = 125;
	else if (temp_iso_val > 0x38 && temp_iso_val <= 0x48)
		*iso_value = 160;
	else if (temp_iso_val > 0x48 && temp_iso_val <= 0x58)
		*iso_value = 200;
	else if (temp_iso_val > 0x58 && temp_iso_val <= 0x68)
		*iso_value = 250;
	else if (temp_iso_val > 0x68 && temp_iso_val <= 0x78)
		*iso_value = 320;
	else if (temp_iso_val > 0x78 && temp_iso_val <= 0x88)
		*iso_value = 400;
	else if (temp_iso_val > 0x88 && temp_iso_val <= 0x98)
		*iso_value = 500;
	else if (temp_iso_val > 0x98 && temp_iso_val <= 0xA8)
		*iso_value = 640;
	else if (temp_iso_val > 0xA8 && temp_iso_val <= 0xB8)
		*iso_value = 800;
	else if (temp_iso_val > 0xB8 && temp_iso_val <= 0xD0)
		*iso_value = 1000;
	else if (temp_iso_val > 0xD8 && temp_iso_val <= 0xF0)
		*iso_value = 1250;
	else
		*iso_value = 1600;

	/*
	   Mapp gain to AE ISO speed, which is provided by Hynix
	   actually, you will find that iso_value and iso_speed are not so fitable, just approximate
	 */
	if (temp_iso_val <= map_table[0])
		*iso_speed = AE_ISO_100;
	else if ((temp_iso_val > map_table[0]) && (temp_iso_val <= map_table[1]))
		*iso_speed = AE_ISO_200;
	else if ((temp_iso_val > map_table[1]) && (temp_iso_val <= map_table[2]))
		*iso_speed = AE_ISO_400;
	else if ((temp_iso_val > map_table[2]) && (temp_iso_val <= map_table[3]))
		*iso_speed = AE_ISO_800;
	else if (temp_iso_val > map_table[3])
		*iso_speed = AE_ISO_1600;
	else
		*iso_speed = AE_ISO_AUTO;


	SENSORDB
	    ("[HI704GetISOValueAndSpeed] Read Gain: 0x%x, Map AE_ISO_Speed : %d, temp_iso_val:%u\n",
	     *iso_value, *iso_speed, temp_iso_val);

	return *iso_value;
}



void HI704GetExifInfo(uintptr_t exifAddr)
{
	SENSOR_EXIF_INFO_STRUCT *pExifInfo = (SENSOR_EXIF_INFO_STRUCT *) exifAddr;
	UINT32 iso_speed, iso_value;

	HI704GetISOValueAndSpeed(&iso_speed, &iso_value);
	pExifInfo->FNumber = 24;
	pExifInfo->AEISOSpeed = iso_speed;
	pExifInfo->AWBMode = HI704_sensor.wb;
	pExifInfo->CapExposureTime = HI704GetExposureTime();
	pExifInfo->FlashLightTimeus = 0;
	pExifInfo->RealISOValue = iso_value;
}

UINT32 HI704SetMaxFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 frameRate)
{
	kal_uint32 pclk;
	kal_int16 dummyLine;
	kal_uint16 lineLength, frameHeight;

	SENSORDB("HI704SetMaxFramerateByScenario: scenarioId = %d, frame rate = %d\n", scenarioId,
		 frameRate);

	switch (scenarioId) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		pclk = 480 / 10;
		lineLength = HI704_VGA_DEFAULT_PIXEL_NUMS;
		frameHeight = (10 * pclk) / frameRate / lineLength;
		dummyLine = frameHeight - HI704_VGA_DEFAULT_LINE_NUMS;
		SENSORDB
		    ("HI704SetMaxFramerateByScenario MSDK_SCENARIO_ID_CAMERA_PREVIEW: lineLength = %d, dummy=%d\n",
		     lineLength, dummyLine);
		/* HI704SetDummy(0, dummyLine); */
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		pclk = 480 / 10;
		lineLength = HI704_VGA_DEFAULT_PIXEL_NUMS;
		frameHeight = (10 * pclk) / frameRate / lineLength;
		dummyLine = frameHeight - HI704_VGA_DEFAULT_LINE_NUMS;
		SENSORDB
		    ("HI704SetMaxFramerateByScenario MSDK_SCENARIO_ID_VIDEO_PREVIEW: lineLength = %d, dummy=%d\n",
		     lineLength, dummyLine);
		/* HI704SetDummy(0, dummyLine); */
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	case MSDK_SCENARIO_ID_CAMERA_ZSD:
		pclk = 480 / 10;
		lineLength = HI704_VGA_DEFAULT_PIXEL_NUMS;
		frameHeight = (10 * pclk) / frameRate / lineLength;
		dummyLine = frameHeight - HI704_VGA_DEFAULT_LINE_NUMS;
		SENSORDB
		    ("HI704SetMaxFramerateByScenario MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG: lineLength = %d, dummy=%d\n",
		     lineLength, dummyLine);
		/* HI704SetDummy(0, dummyLine); */
		break;
	case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW:	/* added */
		break;
	case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO:
		break;
	case MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE:	/* added */
		break;
	default:
		break;
	}
	return ERROR_NONE;
}

UINT32 HI704GetDefaultFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 *pframeRate)
{

	switch (scenarioId) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		*pframeRate = 300;
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	case MSDK_SCENARIO_ID_CAMERA_ZSD:
		*pframeRate = 300;
		break;
	case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW:	/* added */
	case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO:
	case MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE:	/* added */
		*pframeRate = 300;
		break;
	default:
		break;
	}

	return ERROR_NONE;
}

#if 0
void HI704_get_AEAWB_lock(UINT32 *pAElockRet, UINT32 *pAWBlockRet)
{
	*pAElockRet = 1;
	*pAWBlockRet = 1;
	SENSORDB("[HI704]GetAEAWBLock,AE=%d ,AWB=%d\n,", *pAElockRet, *pAWBlockRet);
}
#endif

static int strcmp1(MUINT8 *s1, MUINT8 *s2)
{
	while (*s1 && *s2) {
		if (*s1 > *s2)
			return 1;
		else if (*s1 < *s2)
			return -1;
		else
		{
			++s1;
			++s2;
			continue;
		}
	}
	if (*s1)
		return 1;
	if (*s2)
		return -1;
	return 0;
}

static void debug_imgsensor(MSDK_SENSOR_FEATURE_ENUM feature_id,
			    UINT8 *feature_para, UINT32 *feature_para_len)
{
	MSDK_SENSOR_DBG_IMGSENSOR_INFO_STRUCT *debug_info =
	    (MSDK_SENSOR_DBG_IMGSENSOR_INFO_STRUCT *) feature_para;
	if (strcmp1(debug_info->debugStruct, (MUINT8 *) "sensor_output_dataformat") == 0) {
		SENSORDB("enter sensor_output_dataformat\n");
		if (debug_info->isGet == 1)
			debug_info->value = HI704_sensor.output_format;
		else
			HI704_sensor.output_format = debug_info->value;
	} else {
		SENSORDB("unknown debug\n");
	}
}


UINT32 HI704FeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId,
			   UINT8 *pFeaturePara, UINT32 *pFeatureParaLen)
{
	/* UINT16 u2Temp = 0; */
	UINT16 *pFeatureReturnPara16 = (UINT16 *) pFeaturePara;
	UINT16 *pFeatureData16 = (UINT16 *) pFeaturePara;
	UINT32 *pFeatureReturnPara32 = (UINT32 *) pFeaturePara;
	UINT32 *pFeatureData32 = (UINT32 *) pFeaturePara;
	unsigned long long *pFeatureData = (unsigned long long *)pFeaturePara;
	/*unsigned long long *pFeatureReturnPara = (unsigned long long *) pFeaturePara; */
	MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData = (MSDK_SENSOR_CONFIG_STRUCT *) pFeaturePara;
	MSDK_SENSOR_REG_INFO_STRUCT *pSensorRegData = (MSDK_SENSOR_REG_INFO_STRUCT *) pFeaturePara;

	switch (FeatureId) {
	case SENSOR_FEATURE_DEBUG_IMGSENSOR:
		debug_imgsensor(FeatureId, pFeaturePara, pFeatureParaLen);
		break;

	case SENSOR_FEATURE_GET_RESOLUTION:
		*pFeatureReturnPara16++ = HI704_IMAGE_SENSOR_FULL_WIDTH;
		*pFeatureReturnPara16 = HI704_IMAGE_SENSOR_FULL_HEIGHT;
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_GET_PERIOD:
		*pFeatureReturnPara16++ = HI704_IMAGE_SENSOR_PV_WIDTH;	/* +HI704_sensor.pv_dummy_pixels; */
		*pFeatureReturnPara16 = HI704_IMAGE_SENSOR_PV_HEIGHT;	/* +HI704_sensor.pv_dummy_lines; */
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		/* *pFeatureReturnPara32 = HI704_sensor_pclk/10; */
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:

		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		HI704_night_mode((BOOL) *pFeatureData);
		break;
	case SENSOR_FEATURE_SET_GAIN:
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		HI704_write_cmos_sensor(pSensorRegData->RegAddr, pSensorRegData->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		pSensorRegData->RegData = HI704_read_cmos_sensor(pSensorRegData->RegAddr);
		break;
	case SENSOR_FEATURE_GET_CONFIG_PARA:
		memcpy(pSensorConfigData, &HI704SensorConfigData,
		       sizeof(MSDK_SENSOR_CONFIG_STRUCT));
		*pFeatureParaLen = sizeof(MSDK_SENSOR_CONFIG_STRUCT);
		break;
	case SENSOR_FEATURE_SET_CCT_REGISTER:
	case SENSOR_FEATURE_GET_CCT_REGISTER:
	case SENSOR_FEATURE_SET_ENG_REGISTER:
	case SENSOR_FEATURE_GET_ENG_REGISTER:
	case SENSOR_FEATURE_GET_REGISTER_DEFAULT:
	case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:
	case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA:
	case SENSOR_FEATURE_GET_GROUP_INFO:
	case SENSOR_FEATURE_GET_ITEM_INFO:
	case SENSOR_FEATURE_SET_ITEM_INFO:
	case SENSOR_FEATURE_GET_ENG_INFO:
		break;
	case SENSOR_FEATURE_GET_GROUP_COUNT:
		/* *pFeatureReturnPara32++=0; */
		/* *pFeatureParaLen=4; */
		break;

	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		/* get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE */
		/* if EEPROM does not exist in camera module. */
		*pFeatureReturnPara32 = LENS_DRIVER_ID_DO_NOT_CARE;
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_SET_YUV_CMD:
		HI704YUVSensorSetting((FEATURE_ID) *pFeatureData, *(pFeatureData + 1));
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		HI704YUVSetVideoMode(*pFeatureData);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		HI704_GetSensorID(pFeatureData32);
		break;
	case SENSOR_FEATURE_GET_AF_MAX_NUM_FOCUS_AREAS:
		HI704GetAFMaxNumFocusAreas(pFeatureReturnPara32);
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_GET_AE_MAX_NUM_METERING_AREAS:
		HI704GetAEMaxNumMeteringAreas(pFeatureReturnPara32);
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_GET_EXIF_INFO:
		SENSORDB("SENSOR_FEATURE_GET_EXIF_INFO\n");
		/*SENSORDB("EXIF addr = 0x%x\n", *pFeatureData); */
		HI704GetExifInfo(*pFeatureData);
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		HI704SetMaxFramerateByScenario((MSDK_SCENARIO_ID_ENUM) *pFeatureData,
					       *(pFeatureData + 1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		HI704GetDefaultFramerateByScenario((MSDK_SCENARIO_ID_ENUM) *pFeatureData,
						   (MUINT32 *) (*(pFeatureData + 1)));
		break;


	case SENSOR_FEATURE_SET_TEST_PATTERN:
		/* SENSORDB("[HI704] F_SET_TEST_PATTERN: FAIL: NOT Support\n"); */
		HI704SetTestPatternMode((BOOL) *pFeatureData16);
		break;

	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:	/* for factory mode auto testing */
		*pFeatureReturnPara32 = HI704_TEST_PATTERN_CHECKSUM;
		*pFeatureParaLen = 4;
		break;

	case SENSOR_FEATURE_SET_AE_WINDOW:
		SENSORDB("[%s] SENSOR_FEATURE_SET_AE_WINDOW\n", __func__);
		HI704_AE_Set_Window((uintptr_t) *pFeatureData,
				    HI704_IMAGE_SENSOR_PV_WIDTH, HI704_IMAGE_SENSOR_PV_HEIGHT);
		break;

	case SENSOR_FEATURE_GET_AE_AWB_LOCK_INFO:
		/* SENSORDB(" F_GET_AE_AWB_LOCK_INFO\n"); */
		/* HI704_get_AEAWB_lock((UINT32 *)(uintptr_t)(*pFeatureData),
		(UINT32 *)(uintptr_t)*(pFeatureData+1)); */
		break;

	default:
		SENSORDB("[%s] Not support feature cmd id: %d\n", __func__, FeatureId);
		break;
	}
	return ERROR_NONE;
}				/* HI704FeatureControl() */


SENSOR_FUNCTION_STRUCT SensorFuncHI704 = {
	HI704Open,
	HI704GetInfo,
	HI704GetResolution,
	HI704FeatureControl,
	HI704Control,
	HI704Close
};

UINT32 HI704_YUV_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &SensorFuncHI704;

	return ERROR_NONE;
}				/* SensorInit() */
