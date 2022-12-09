// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 IMX686mipi_Sensor.c
 *
 * Project:
 * --------
 *	 ALPS
 *
 * Description:
 * ------------
 *	 Source code of Sensor driver
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/
#define PFX "IMX686_camera_sensor"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__


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

#include "imx686mipiraw_Sensor.h"
#include "imx686_eeprom.h"

#undef VENDOR_EDIT

#define USE_BURST_MODE 1

#define I2C_BUFFER_LEN 255 /* trans# max is 255, each 3 bytes */

static kal_uint8 qsc_flag;
// static kal_uint8 otp_flag;

#if USE_BURST_MODE
static kal_uint16 imx686_table_write_cmos_sensor(
		kal_uint16 *para, kal_uint32 len);
#endif
static DEFINE_SPINLOCK(imgsensor_drv_lock);

#define imx686_flag 0

#define use_capture_setting 1

#define new_init_setting 1
#define imx686_seamless_test 1

#ifdef imx686_seamless_test
#define _I2C_BUF_SIZE 4096
kal_uint16 imx686_i2c_data[_I2C_BUF_SIZE];
unsigned int imx686_size_to_write;
bool imx686_is_seamless;
#endif

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = IMX686_SENSOR_ID,

		.checksum_value = 0xc1242d24,
#ifdef use_capture_setting
	.pre = { /*reg_C-1 16M@30fps*/
		.pclk = 1096000000,
		.linelength = 10208,
		.framelength = 3578,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4608,
		.grabwindow_height = 3456,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 666510000,
		.max_framerate = 300,
	},
#else
	.pre = { /*reg_M 16M@60fps*/
		.pclk = 2196000000,
		.linelength = 10208,
		.framelength = 7168,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4608,
		.grabwindow_height = 3456,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1388570000,
		.max_framerate = 300,
	},
#endif
	.cap = { /*reg_C-1 16M@30fps*/
		.pclk = 1096000000,
		.linelength = 10208,
		.framelength = 3578,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4608,
		.grabwindow_height = 3456,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 666510000,
		.max_framerate = 300,
	},

	.normal_video = { /* reg_D 4608x2592 @30fps */
		.pclk = 824000000,
		.linelength = 10208,
		.framelength = 2690,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4608,
		.grabwindow_height = 2592,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 499200000,
		.max_framerate = 300,
	},


	.hs_video = { /* reg_I 2304x1296 @120fps (binning)*/
		.pclk = 988000000,
		.linelength = 5488,
		.framelength = 1500,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2304,
		.grabwindow_height = 1296,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 538970000,
		.max_framerate = 1200,
	},

	.slim_video = { /* reg_D 4608x2592 @30fps */
		.pclk = 824000000,
		.linelength = 10208,
		.framelength = 2690,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4608,
		.grabwindow_height = 2592,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 499200000,
		.max_framerate = 300,
	},

	.custom1 = { /*reg_D-1 4608x3456 @24fps (binning)*/
		.pclk = 864000000,
		.linelength = 10208,
		.framelength = 3526,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4608,
		.grabwindow_height = 3456,
		.mipi_data_lp2hs_settle_dc = 85,
		/* following for GetDefaultFramerateByScenario() */
		.mipi_pixel_rate = 525260000,
		.max_framerate = 240, /* 24fps */
	},

	.custom2 = { /* reg_E 16:9 @ 60fps */
		.pclk = 1720000000,
		.linelength = 10208,
		.framelength = 2808,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4608,
		.grabwindow_height = 2592,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1053260000,
		.max_framerate = 600,
	},
#ifdef new_init_setting
	.custom3 = { //Reg_C fullsize(4:3) remosaic @10FPS(Cphy1.0)
		.pclk = 2192000000,
		.linelength = 5488,
		.framelength = 2550,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1280,
		.grabwindow_height = 720,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1138290000,
		.max_framerate = 1200,
	},
#else
	.custom3 = { //Reg_C fullsize(4:3) remosaic @10FPS(Cphy1.0)
		.pclk = 2196000000,
		.linelength = 15168,
		.framelength = 6999,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 9216,
		.grabwindow_height = 6912,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1618970000,
		.max_framerate = 210,
	},
#endif
	.custom4 = { /* reg_O 2306x1296 @240fps */
		.pclk = 2016000000,
		.linelength = 5488,
		.framelength = 1530,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2304,
		.grabwindow_height = 1296,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1124570000,
		.max_framerate = 2400,
	},

#ifdef new_init_setting
	.custom5 = { /* reg_L 4608x3456 @60fps */
		.pclk = 2192000000,
		.linelength = 10208,
		.framelength = 7156,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4608,
		.grabwindow_height = 3456,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 2391770000,
		.max_framerate = 300,
	},
#else
	.custom5 = { /* reg_L 4608x2992 @60fps */
		.pclk = 1968000000,
		.linelength = 10208,
		.framelength = 3212,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4608,
		.grabwindow_height = 2992,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1230170000,
		.max_framerate = 600,
	},
#endif

#ifdef new_init_setting
	.custom6 = { /* reg_L 4608x2992 @60fps */
		.pclk = 2192000000,
		.linelength = 10336,
		.framelength = 7069,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 9216,
		.grabwindow_height = 6912,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 2391770000,
		.max_framerate = 300,
	},
#else
	.custom6 = { /* reg_L 4608x2992 @60fps */
		.pclk = 1968000000,
		.linelength = 10208,
		.framelength = 3212,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4608,
		.grabwindow_height = 2992,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1230170000,
		.max_framerate = 600,
	},
#endif
	.margin = 64,		/* sensor framelength & shutter margin */
	.min_shutter = 4,	/* min shutter */
	.min_gain = 64, /*1x gain*/
	.max_gain = 4096, /*64x gain*/
	.min_gain_iso = 100,
	.gain_step = 1,
	.gain_type = 0,/*to be modify,no gain table for sony*/
	.max_frame_length = 0xffff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,	/* isp gain delay frame for AE cycle */
	.ihdr_support = 0,	/* 1, support; 0,not support */
	.ihdr_le_firstline = 0,	/* 1,le first ; 0, se first */
	.temperature_support = 1,/* 1, support; 0,not support */
	.sensor_mode_num = 11,	/* support sensor mode num */

	.cap_delay_frame = 2,	/* enter capture delay frame num */
	.pre_delay_frame = 2,	/* enter preview delay frame num */
	.video_delay_frame = 2,	/* enter video delay frame num */
	.hs_video_delay_frame = 2,
	.slim_video_delay_frame = 2,	/* enter slim video delay frame num */
	.custom1_delay_frame = 2,	/* enter custom1 delay frame num */
	.custom2_delay_frame = 2,	/* enter custom2 delay frame num */
	.custom3_delay_frame = 2,	/* enter custom2 delay frame num */
	.custom4_delay_frame = 2,	/* enter custom2 delay frame num */
	.custom5_delay_frame = 2,	/* enter custom2 delay frame num */
	.custom6_delay_frame = 2,	/* enter custom2 delay frame num */
	.frame_time_delay_frame = 3,

	.isp_driving_current = ISP_DRIVING_4MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	/* .mipi_sensor_type = MIPI_OPHY_NCSI2, */
	/* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
	.mipi_sensor_type = MIPI_CPHY, /* 0,MIPI_OPHY_NCSI2; 1,MIPI_OPHY_CSI2 */
	.mipi_settle_delay_mode = 0,
#ifdef new_init_setting
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_R,
#else
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_R,
#endif
	.mclk = 24, /* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
	/*.mipi_lane_num = SENSOR_MIPI_4_LANE,*/
	.mipi_lane_num = SENSOR_MIPI_3_LANE,
	.i2c_addr_table = {0x34, 0xff},
	/* record sensor support all write id addr,
	 * only supprt 4 must end with 0xff
	 */
	.i2c_speed = 1000, /* i2c read/write speed */
};

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,	/* mirrorflip information */
	.sensor_mode = IMGSENSOR_MODE_INIT,
	/* IMGSENSOR_MODE enum value,record current sensor mode,such as:
	 * INIT, Preview, Capture, Video,High Speed Video, Slim Video
	 */
	.shutter = 0x3D0,	/* current shutter */
	.gain = 0x100,		/* current gain */
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_mode = 0, /* sensor need support LE, SE with HDR feature */
	.i2c_write_id = 0x34, /* record current sensor's i2c write id */
};


/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[11] = {
	{9248, 6944, 0,   16, 9248, 6912, 4624, 3456,
	8,  0, 4608, 3456,  0,  0, 4608, 3456}, /* Preview */
	{9248, 6944, 0,   16, 9248, 6912, 4624, 3456,
	8,  0, 4608, 3456,  0,  0, 4608, 3456}, /* capture */
	{9248, 6944, 0,  880, 9248, 5184, 4624, 2592,
	8,  0, 4608, 2592,  0,  0, 4608, 2592}, /* normal video */
	{9248, 6944, 0,  880, 9248, 5184, 2312, 1296,
	4,  0, 2304, 1296,  0,  0, 2304, 1296}, /* hs_video */
	{9248, 6944, 0,  880, 9248, 5184, 4624, 2592,
	8,  0, 4608, 2592,  0,  0, 4608, 2592}, /* slim video */
	{9248, 6944, 0,   16, 9248, 6912, 4624, 3456,
	8,  0, 4608, 3456,  0,  0, 4608, 3456}, /* custom1 */
	{9248, 6944, 0,  880, 9248, 5184, 4624, 2592,
	8,  0, 4608, 2592,  0,  0, 4608, 2592}, /* custom2 */
#ifdef new_init_setting
	{9248, 6944, 0, 2032, 9248, 2880, 2312,  720,
	516, 0, 1280,  720,  0,  0, 1280,  720}, /* custom3 */
#else
	{9248, 6944, 0,   16, 9248, 6912, 9248, 6912,
	16,   0, 9216, 6912,  0,  0, 9216, 6912}, /* custom3 */
#endif
	{9248, 6944, 0,  880, 9248, 5184, 2312, 1296,
	4,  0, 2304, 1296,  0,  0, 2304, 1296}, /* custom4 */
#ifdef new_init_setting
	{9248, 6944, 0,  16, 9248, 6912, 4624, 3456,
	8, 0, 4608, 3456,  0,  0, 4608, 3456}, /* custom5 */
#else
	{9248, 6944, 0,  480, 9248, 5984, 4624, 2992,
	8, 0, 4608, 2992,  0,  0, 4608, 2992}, /* custom5 */
#endif

#ifdef new_init_setting
	{9248, 6944, 0,  16, 9248, 6912, 9248, 6912,
	16, 0, 9216, 6912,  0,  0, 9216, 6912}, /* custom6 */
#else
	{9248, 6944, 0,  480, 9248, 5984, 4624, 2992,
	8, 0, 4608, 2992,  0,  0, 4608, 2992}, /* custom6 */
#endif
};
 /*VC1 for HDR(DT=0X35), VC2 for PDAF(DT=0X36), unit : 10bit */
static struct SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[7] = {
#ifdef new_init_setting
	/* Preview mode setting */ /*index:0*/
	{0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x1200, 0x0D80, 0x00, 0x00, 0x00, 0x00,
	 0x01, 0x2b, 0x047C, 0x06B8, 0x00, 0x00, 0x0000, 0x0000},
#else
	/* Preview mode setting */ /*index:0*/
	{0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x1200, 0x0D80, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x30, 0x05A0, 0x06B8, 0x00, 0x00, 0x0000, 0x0000},
#endif

	/* Slim_Video mode setting */ /*index:1*/
	{0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x1200, 0x0BB0, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x30, 0x05A0, 0x05D8, 0x00, 0x00, 0x0000, 0x0000},
	/* 4K_Video mode setting */ /*index:2*/
	{0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x1200, 0x0A20, 0x00, 0x00, 0x0000, 0x0000,
	 0x00, 0x30, 0x05A0, 0x0508, 0x00, 0x00, 0x0000, 0x0000},

#ifdef new_init_setting
	/* Normal_Video mode setting */ /*index:3*/
	{0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x1200, 0x0A20, 0x00, 0x00, 0x0000, 0x0000,
	 0x01, 0x2b, 0x047C, 0x0508, 0x00, 0x00, 0x0000, 0x0000},
#else
	/* Normal_Video mode setting */ /*index:3*/
	{0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x1200, 0x0A20, 0x00, 0x00, 0x0000, 0x0000,
	 0x00, 0x30, 0x05A0, 0x0508, 0x00, 0x00, 0x0000, 0x0000},
#endif

#ifdef new_init_setting
	/*custom3*/ /*index:4*/
	{0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x0500, 0x02D0, 0x00, 0x00, 0x0000, 0x0000,
	 0x00, 0x00, 0x0000, 0x0000, 0x00, 0x00, 0x0000, 0x0000},
#else
	/*custom3*/ /*index:4*/
	{0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x0500, 0x02D0, 0x00, 0x00, 0x0000, 0x0000,
	 0x00, 0x30, 0x0000, 0x0000, 0x00, 0x00, 0x0000, 0x0000},
#endif
	/*custom5*/
	{0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x1200, 0x0D80, 0x00, 0x00, 0x0000, 0x0000,
	 0x01, 0x2b, 0x047C, 0x06B8, 0x00, 0x00, 0x0000, 0x0000},//pd size
	/*custom6*/
	{0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x2400, 0x1B00, 0x00, 0x00, 0x0000, 0x0000,
	 0x01, 0x2b, 0x047C, 0x06B8, 0x00, 0x00, 0x0000, 0x0000} //pd size
};


/* If mirror flip */
static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_binning = {
	.i4OffsetX = 17,
	.i4OffsetY = 16,
	.i4PitchX  =  8,
	.i4PitchY  = 16,
	.i4PairNum  = 8,
	.i4SubBlkW  = 8,
	.i4SubBlkH  = 2,
	.i4PosL = { {20, 17}, {18, 19}, {22, 21}, {24, 23},
		   {20, 25}, {18, 27}, {22, 29}, {24, 31} },
	.i4PosR = { {19, 17}, {17, 19}, {21, 21}, {23, 23},
		   {19, 25}, {17, 27}, {21, 29}, {23, 31} },
	.i4BlockNumX = 574,
	.i4BlockNumY = 215,
	.iMirrorFlip = 0,
	.i4Crop = { {8, 8}, {8, 8}, {8, 440}, {0, 0}, {8, 440},
		    {8, 8}, {8, 440}, {0, 0}, {0, 0}, {8, 240} },
};

#ifdef imx686_seamless_test
/* TODO: measure the delay */
static struct SEAMLESS_SYS_DELAY seamless_sys_delays[] = {
	{ MSDK_SCENARIO_ID_CUSTOM5, MSDK_SCENARIO_ID_CUSTOM6, 1 },
	{ MSDK_SCENARIO_ID_CUSTOM6, MSDK_SCENARIO_ID_CUSTOM5, 1 },
};
#endif

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

static void write_cmos_sensor(kal_uint16 addr, kal_uint16 para)
{
	char pusendcmd[4] = {(char)(addr >> 8), (char)(addr & 0xFF),
			     (char)(para >> 8), (char)(para & 0xFF)};

	imgsensor_i2c_write(
		get_i2c_cfg(),
		pusendcmd,
		4,
		4,
		imgsensor.i2c_write_id,
		IMGSENSOR_I2C_SPEED);
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

static void imx686_get_pdaf_reg_setting(MUINT32 regNum, kal_uint16 *regDa)
{
	int i, idx;

	for (i = 0; i < regNum; i++) {
		idx = 2 * i;
		regDa[idx + 1] = read_cmos_sensor_8(regDa[idx]);
		pr_debug("%x %x", regDa[idx], regDa[idx+1]);
	}
}
static void imx686_set_pdaf_reg_setting(MUINT32 regNum, kal_uint16 *regDa)
{
	int i, idx;

	for (i = 0; i < regNum; i++) {
		idx = 2 * i;
		write_cmos_sensor_8(regDa[idx], regDa[idx + 1]);
		pr_debug("%x %x", regDa[idx], regDa[idx+1]);
	}
}

static kal_uint16 read_cmos_eeprom_8(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 1, 0xA0);
	return get_byte;
}

static kal_uint16 imx686_QSC_setting[3024 * 2];
static kal_uint16 imx686_LRC_setting[504 * 2];
static void read_sensor_Cali(void)
{
	kal_uint16 idx = 0, addr_qsc = 0x1880, sensor_addr = 0xCA00;
	kal_uint16 addr_lrc = 0x2460;

	for (idx = 0; idx < 3024; idx++) {
		addr_qsc = 0x1880 + idx;
		sensor_addr = 0xCA00 + idx;
		imx686_QSC_setting[2*idx] = sensor_addr;
		imx686_QSC_setting[2*idx + 1] = read_cmos_eeprom_8(addr_qsc);
	}

	for (idx = 0; idx < 252; idx++) {
		addr_lrc = 0x2460 + idx;
		sensor_addr = 0x7B00 + idx;
		imx686_LRC_setting[2*idx] = sensor_addr;
		imx686_LRC_setting[2*idx + 1] = read_cmos_eeprom_8(addr_lrc);
	}
	for (idx = 252; idx < 504; idx++) {
		addr_lrc = 0x255c + idx;
		sensor_addr = 0x7C00 + idx - 252;
		imx686_LRC_setting[2*idx] = sensor_addr;
		imx686_LRC_setting[2*idx + 1] = read_cmos_eeprom_8(addr_lrc);
	}
}

static void write_sensor_QSC(void)
{
	#if USE_BURST_MODE
	imx686_table_write_cmos_sensor(imx686_QSC_setting,
		sizeof(imx686_QSC_setting)/sizeof(kal_uint16));
	#else
	kal_uint16 idx = 0, addr_qsc = 0xCA00;

	for (idx = 0; idx < 3024; idx++) {
		addr_qsc = 0xCA00 + idx;
		write_cmos_sensor_8(addr_qsc, imx686_QSC_setting[2 * idx + 1]);
	}
	#endif
}

static void write_sensor_LRC(void)
{
	#if USE_BURST_MODE
	imx686_table_write_cmos_sensor(imx686_LRC_setting,
		sizeof(imx686_LRC_setting)/sizeof(kal_uint16));
	#else
	kal_uint16 idx = 0, addr_lrc = 0x7510;

	for (idx = 0; idx < 252; idx++) {
		addr_lrc = 0x7B00 + idx;
		write_cmos_sensor_8(addr_lrc, imx686_LRC_setting[2*idx + 1]);
	}
	for (idx = 252; idx < 504; idx++) {
		addr_lrc = 0x7C00 + idx - 252;
		write_cmos_sensor_8(addr_lrc, imx686_LRC_setting[2*idx + 1]);
	}
	#endif
}

static void set_dummy(void)
{
	pr_debug("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);
	/* return;*/ /* for test */

#ifdef imx686_seamless_test
	if (!imx686_is_seamless) {
		write_cmos_sensor_8(0x0104, 0x01);

		write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
		write_cmos_sensor_8(0x0342, imgsensor.line_length >> 8);
		write_cmos_sensor_8(0x0343, imgsensor.line_length & 0xFF);

		write_cmos_sensor_8(0x0104, 0x00);
	} else {
		imx686_i2c_data[imx686_size_to_write++] = 0x0340;
		imx686_i2c_data[imx686_size_to_write++] = imgsensor.frame_length >> 8;
		imx686_i2c_data[imx686_size_to_write++] = 0x0341;
		imx686_i2c_data[imx686_size_to_write++] = imgsensor.frame_length & 0xFF;
		imx686_i2c_data[imx686_size_to_write++] = 0x0342;
		imx686_i2c_data[imx686_size_to_write++] = imgsensor.frame_length >> 8;
		imx686_i2c_data[imx686_size_to_write++] = 0x0343;
		imx686_i2c_data[imx686_size_to_write++] = imgsensor.frame_length & 0xFF;
	}
#else
	write_cmos_sensor_8(0x0104, 0x01);

	write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
	write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
	write_cmos_sensor_8(0x0342, imgsensor.line_length >> 8);
	write_cmos_sensor_8(0x0343, imgsensor.line_length & 0xFF);

	write_cmos_sensor_8(0x0104, 0x00);
#endif
}	/*	set_dummy  */

static void set_mirror_flip(kal_uint8 image_mirror)
{
	kal_uint8 itemp;

	pr_debug("image_mirror = %d\n", image_mirror);
	itemp = read_cmos_sensor_8(0x0101);
	itemp &= ~0x03;
	pr_debug("itemp = %d\n", itemp);
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

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	/*kal_int16 dummy_line;*/
	kal_uint32 frame_length = imgsensor.frame_length;

	pr_debug(
		"framerate = %d, min framelength should enable %d\n", framerate,
		min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	if (frame_length >= imgsensor.min_frame_length)
		imgsensor.frame_length = frame_length;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;

	imgsensor.dummy_line =
			imgsensor.frame_length - imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line =
			imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}	/*	set_max_framerate  */

static void write_shutter(kal_uint32 shutter)
{
	kal_uint16 realtime_fps = 0;
	int longexposure_times = 0;
	static int long_exposure_status;

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
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10
				/ imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
	}

#ifdef imx686_seamless_test
	if (!imx686_is_seamless)
		write_cmos_sensor_8(0x0104, 0x01);
#endif

	while (shutter >= 65535) {
		shutter = shutter / 2;
		longexposure_times += 1;
	}

#ifdef imx686_seamless_test
	if (!imx686_is_seamless) //need check
		if (read_cmos_sensor_8(0x0350) != 0x01) {
			pr_debug("single cam scenario enable auto-extend");
			write_cmos_sensor_8(0x0350, 0x01);
		}
#endif

	if (longexposure_times > 0) {
		pr_debug("Enter Long Exposure Mode, Time Is %d",
			longexposure_times);
		long_exposure_status = 1;
		imgsensor.frame_length = shutter + 32;

#ifdef imx686_seamless_test
		if (!imx686_is_seamless)
			write_cmos_sensor_8(0x3100, longexposure_times & 0x07);
		else {
			imx686_i2c_data[imx686_size_to_write++] = 0x3100;
			imx686_i2c_data[imx686_size_to_write++] = longexposure_times & 0x07;
		}
#else
		write_cmos_sensor_8(0x3100, longexposure_times & 0x07);
#endif

	} else if (long_exposure_status == 1) {
		long_exposure_status = 0;
#ifdef imx686_seamless_test
		if (!imx686_is_seamless) {
			write_cmos_sensor_8(0x0104, 0x01);
			write_cmos_sensor_8(0x3100, 0x00);
			write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
			write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
			write_cmos_sensor_8(0x0104, 0x00);
		} else {
			imx686_i2c_data[imx686_size_to_write++] = 0x0104;
			imx686_i2c_data[imx686_size_to_write++] = 1;
			imx686_i2c_data[imx686_size_to_write++] = 0x3100;
			imx686_i2c_data[imx686_size_to_write++] = 0;
			imx686_i2c_data[imx686_size_to_write++] = 0x0340;
			imx686_i2c_data[imx686_size_to_write++] = imgsensor.frame_length >> 8;
			imx686_i2c_data[imx686_size_to_write++] = 0x0341;
			imx686_i2c_data[imx686_size_to_write++] = imgsensor.frame_length & 0xFF;
			imx686_i2c_data[imx686_size_to_write++] = 0x0104;
			imx686_i2c_data[imx686_size_to_write++] = 0;
		}
#else
		write_cmos_sensor_8(0x0104, 0x01);
		write_cmos_sensor_8(0x3100, 0x00);
		write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
		write_cmos_sensor_8(0x0104, 0x00);
#endif
		pr_debug("Exit Long Exposure Mode");
	}
	/* Update Shutter */

#ifdef imx686_seamless_test
	if (!imx686_is_seamless) {
		write_cmos_sensor_8(0x0104, 0x01);
		write_cmos_sensor_8(0x0350, 0x01);
		write_cmos_sensor_8(0x0202, (shutter >> 8) & 0xFF);
		write_cmos_sensor_8(0x0203, shutter  & 0xFF);
		write_cmos_sensor_8(0x0104, 0x00);
	} else {
		imx686_i2c_data[imx686_size_to_write++] = 0x0104;
		imx686_i2c_data[imx686_size_to_write++] = 1;
		imx686_i2c_data[imx686_size_to_write++] = 0x0350;
		imx686_i2c_data[imx686_size_to_write++] = 1;
		imx686_i2c_data[imx686_size_to_write++] = 0x0202;
		imx686_i2c_data[imx686_size_to_write++] = (shutter >> 8) & 0xFF;
		imx686_i2c_data[imx686_size_to_write++] = 0x0203;
		imx686_i2c_data[imx686_size_to_write++] = shutter  & 0xFF;
		imx686_i2c_data[imx686_size_to_write++] = 0x0104;
		imx686_i2c_data[imx686_size_to_write++] = 0;
	}
#else
	write_cmos_sensor_8(0x0104, 0x01);
	write_cmos_sensor_8(0x0350, 0x01);
	write_cmos_sensor_8(0x0202, (shutter >> 8) & 0xFF);
	write_cmos_sensor_8(0x0203, shutter  & 0xFF);
	write_cmos_sensor_8(0x0104, 0x00);
#endif

	pr_debug("shutter =%d, framelength =%d\n",
		shutter, imgsensor.frame_length);

}	/*	write_shutter  */

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
static void set_shutter_frame_length(kal_uint16 shutter,
				     kal_uint16 frame_length,
				     kal_bool auto_extend_en)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	kal_int32 dummy_line = 0;

	kal_uint32 record_framelength;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	/*0x3500, 0x3501, 0x3502 will increase VBLANK to
	 *get exposure larger than frame exposure
	 *AE doesn't update sensor gain at capture mode,
	 *thus extra exposure lines must be updated here.
	 */

	/* OV Recommend Solution */
	/*if shutter bigger than frame_length,
	 *should extend frame length first
	 */
	record_framelength = ((read_cmos_sensor_8(0x0341) & 0xFF)
					| (read_cmos_sensor_8(0x0340) << 8));
	spin_lock(&imgsensor_drv_lock);
	/* Change frame time */
	if (frame_length > 1)
		dummy_line = frame_length - imgsensor.frame_length;

	imgsensor.frame_length = imgsensor.frame_length + dummy_line;

//	if (shutter > imgsensor.frame_length - imgsensor_info.margin)
//		imgsensor.frame_length = shutter + imgsensor_info.margin;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter)
			? imgsensor_info.min_shutter : shutter;
	shutter =
	(shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
		? (imgsensor_info.max_frame_length - imgsensor_info.margin)
		: shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 /
				imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			/* Extend frame length */
			write_cmos_sensor_8(0x0104, 0x01);
			write_cmos_sensor_8(0x0340,
					imgsensor.frame_length >> 8);
			write_cmos_sensor_8(0x0341,
					imgsensor.frame_length & 0xFF);
			write_cmos_sensor_8(0x0104, 0x00);
		}
	} else {
		/* Extend frame length */
		write_cmos_sensor_8(0x0104, 0x01);
		write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
		write_cmos_sensor_8(0x0104, 0x00);
	}

	/* Update Shutter */
	write_cmos_sensor_8(0x0104, 0x01);
	if (auto_extend_en)
		write_cmos_sensor_8(0x0350, 0x01); /* Enable auto extend */
	else
		write_cmos_sensor_8(0x0350, 0x00); /* Disable auto extend */
	write_cmos_sensor_8(0x0202, (shutter >> 8) & 0xFF);
	write_cmos_sensor_8(0x0203, shutter  & 0xFF);
	write_cmos_sensor_8(0x0104, 0x00);
	pr_debug(
		"Exit! shutter =%d, framelength =%d/%d, record_framelength = %d, dummy_line=%d, auto_extend=%d\n",
		shutter, imgsensor.frame_length, frame_length,
		record_framelength, dummy_line, read_cmos_sensor_8(0x0350));

}	/* set_shutter_frame_length */

static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 reg_gain = 0x0;

	reg_gain = 1024 - (1024*64)/gain;
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
	kal_uint16 reg_gain, max_gain = imgsensor_info.max_gain;

	if (imgsensor.sensor_mode == IMGSENSOR_MODE_CUSTOM3) {
		/* 48M@30FPS */
		max_gain = 16 * BASEGAIN;
	}

	if (gain < imgsensor_info.min_gain || gain > max_gain) {
		pr_debug("Error gain setting");

		if (gain < imgsensor_info.min_gain)
			gain = imgsensor_info.min_gain;
		else if (gain > max_gain)
			gain = max_gain;
	}

	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	pr_debug("gain = %d, reg_gain = 0x%x, max_gain:0x%x\n ",
		gain, reg_gain, max_gain);

	if (!imx686_is_seamless) {
		write_cmos_sensor_8(0x0104, 0x01);
		write_cmos_sensor_8(0x0204, (reg_gain>>8) & 0xFF);
		write_cmos_sensor_8(0x0205, reg_gain & 0xFF);
		write_cmos_sensor_8(0x0104, 0x00);
	} else {
		imx686_i2c_data[imx686_size_to_write++] = 0x0204;
		imx686_i2c_data[imx686_size_to_write++] =  (reg_gain>>8) & 0xFF;
		imx686_i2c_data[imx686_size_to_write++] = 0x0205;
		imx686_i2c_data[imx686_size_to_write++] = reg_gain & 0xFF;
	}


	return gain;
} /* set_gain */

static kal_uint32 imx686_awb_gain(struct SET_SENSOR_AWB_GAIN *pSetSensorAWB)
{
#if imx686_flag
	UINT32 rgain_32, grgain_32, gbgain_32, bgain_32;

	grgain_32 = (pSetSensorAWB->ABS_GAIN_GR + 1) >> 1;
	rgain_32 = (pSetSensorAWB->ABS_GAIN_R + 1) >> 1;
	bgain_32 = (pSetSensorAWB->ABS_GAIN_B + 1) >> 1;
	gbgain_32 = (pSetSensorAWB->ABS_GAIN_GB + 1) >> 1;
	pr_debug("[%s] ABS_GAIN_GR:%d, grgain_32:%d\n",
		__func__,
		pSetSensorAWB->ABS_GAIN_GR, grgain_32);
	pr_debug("[%s] ABS_GAIN_R:%d, rgain_32:%d\n",
		__func__,
		pSetSensorAWB->ABS_GAIN_R, rgain_32);
	pr_debug("[%s] ABS_GAIN_B:%d, bgain_32:%d\n",
		__func__,
		pSetSensorAWB->ABS_GAIN_B, bgain_32);
	pr_debug("[%s] ABS_GAIN_GB:%d, gbgain_32:%d\n",
		__func__,
		pSetSensorAWB->ABS_GAIN_GB, gbgain_32);

	write_cmos_sensor_8(0x0b8e, (grgain_32 >> 8) & 0xFF);
	write_cmos_sensor_8(0x0b8f, grgain_32 & 0xFF);
	write_cmos_sensor_8(0x0b90, (rgain_32 >> 8) & 0xFF);
	write_cmos_sensor_8(0x0b91, rgain_32 & 0xFF);
	write_cmos_sensor_8(0x0b92, (bgain_32 >> 8) & 0xFF);
	write_cmos_sensor_8(0x0b93, bgain_32 & 0xFF);
	write_cmos_sensor_8(0x0b94, (gbgain_32 >> 8) & 0xFF);
	write_cmos_sensor_8(0x0b95, gbgain_32 & 0xFF);

	imx686_awb_gain_table[1]  = (grgain_32 >> 8) & 0xFF;
	imx686_awb_gain_table[3]  = grgain_32 & 0xFF;
	imx686_awb_gain_table[5]  = (rgain_32 >> 8) & 0xFF;
	imx686_awb_gain_table[7]  = rgain_32 & 0xFF;
	imx686_awb_gain_table[9]  = (bgain_32 >> 8) & 0xFF;
	imx686_awb_gain_table[11] = bgain_32 & 0xFF;
	imx686_awb_gain_table[13] = (gbgain_32 >> 8) & 0xFF;
	imx686_awb_gain_table[15] = gbgain_32 & 0xFF;
	imx686_table_write_cmos_sensor(imx686_awb_gain_table,
		sizeof(imx686_awb_gain_table)/sizeof(kal_uint16));
#endif

	return ERROR_NONE;
}

static kal_uint16 imx686_feedback_awbgain[] = {
	0x0b90, 0x00,
	0x0b91, 0x01,
	0x0b92, 0x00,
	0x0b93, 0x01,
};
/*write AWB gain to sensor*/
static void feedback_awbgain(kal_uint32 r_gain, kal_uint32 b_gain)
{
	UINT32 r_gain_int = 0;
	UINT32 b_gain_int = 0;

	r_gain_int = r_gain / 512;
	b_gain_int = b_gain / 512;
#if imx686_flag
	/*write r_gain*/
	write_cmos_sensor_8(0x0B90, r_gain_int);
	write_cmos_sensor_8(0x0B91,
		(((r_gain*100) / 512) - (r_gain_int * 100)) * 2);

	/*write _gain*/
	write_cmos_sensor_8(0x0B92, b_gain_int);
	write_cmos_sensor_8(0x0B93,
		(((b_gain * 100) / 512) - (b_gain_int * 100)) * 2);
#else
	imx686_feedback_awbgain[1] = r_gain_int;
	imx686_feedback_awbgain[3] = (
		((r_gain*100) / 512) - (r_gain_int * 100)) * 2;
	imx686_feedback_awbgain[5] = b_gain_int;
	imx686_feedback_awbgain[7] = (
		((b_gain * 100) / 512) - (b_gain_int * 100)) * 2;
	imx686_table_write_cmos_sensor(imx686_feedback_awbgain,
		sizeof(imx686_feedback_awbgain)/sizeof(kal_uint16));
#endif
}

static void imx686_set_lsc_reg_setting(
		kal_uint8 index, kal_uint16 *regDa, MUINT32 regNum)
{
	int i;
	int startAddr[4] = {0x9D88, 0x9CB0, 0x9BD8, 0x9B00};
	/*0:B,1:Gb,2:Gr,3:R*/

	pr_debug("E! index:%d, regNum:%d\n", index, regNum);

	write_cmos_sensor_8(0x0B00, 0x01); /*lsc enable*/
	write_cmos_sensor_8(0x9014, 0x01);
	write_cmos_sensor_8(0x4439, 0x01);
	mdelay(1);
	pr_debug("Addr 0xB870, 0x380D Value:0x%x %x\n",
		read_cmos_sensor_8(0xB870), read_cmos_sensor_8(0x380D));
	/*define Knot point, 2'b01:u3.7*/
	write_cmos_sensor_8(0x9750, 0x01);
	write_cmos_sensor_8(0x9751, 0x01);
	write_cmos_sensor_8(0x9752, 0x01);
	write_cmos_sensor_8(0x9753, 0x01);

	for (i = 0; i < regNum; i++)
		write_cmos_sensor(startAddr[index] + 2*i, regDa[i]);

	write_cmos_sensor_8(0x0B00, 0x00); /*lsc disable*/
}
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
static kal_uint32 streaming_control(kal_bool enable)
{
	pr_debug("streaming_enable(0=Sw Standby,1=streaming): %d\n",
		enable);
	if (enable)
		write_cmos_sensor_8(0x0100, 0X01);
	else
		write_cmos_sensor_8(0x0100, 0x00);
	return ERROR_NONE;
}

#if USE_BURST_MODE
#define I2C_BUFFER_LEN 255 /* trans# max is 255, each 3 bytes */
static kal_uint16 imx686_table_write_cmos_sensor(kal_uint16 *para,
						 kal_uint32 len)
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
			puSendCmd[tosend++] = (char)(data & 0xFF);
			IDX += 2;
			addr_last = addr;

		}
		/* Write when remain buffer size is less than 3 bytes
		 * or reach end of data
		 */
		if ((I2C_BUFFER_LEN - tosend) < 3
			|| IDX == len || addr != addr_last) {
			imgsensor_i2c_write(
				get_i2c_cfg(),
				puSendCmd,
				tosend,
				3,
				imgsensor.i2c_write_id,
				imgsensor_info.i2c_speed);
			tosend = 0;
		}
	}
	return 0;
}


static kal_uint16 imx686_init_setting[] = {
#ifdef new_init_setting
	/*External Clock Setting*/
	0x0136, 0x18,
	0x0137, 0x00,
	/*Register version*/
	0x33F0, 0x02,
	0x33F1, 0x08,
	/*Signaling mode setting*/
	0x0111, 0x03,
	/*PDAF TYPE1 data type Setting*/
	0x3062, 0x00,
	0x3063, 0x30,
	/*PDAF TYPE2 data type Setting*/
	0x3076, 0x01,
	0x3077, 0x2b,
	/*Global Setting*/
	0x4008, 0x10,
	0x4009, 0x10,
	0x400A, 0x10,
	0x400B, 0x10,
	0x400C, 0x10,
	0x400F, 0x01,
	0x4011, 0x01,
	0x4013, 0x01,
	0x4015, 0x01,
	0x4017, 0x40,
	0x4FA3, 0x1F,
	0x4FA5, 0xA6,
	0x4FA7, 0x63,
	0x4FF7, 0x1F,
	0x4FF9, 0xEA,
	0x4FFB, 0x63,
	0x59DD, 0x02,
	0x5B72, 0x05,
	0x5B74, 0x06,
	0x5B86, 0x05,
	0x5BCC, 0x0B,
	0x5C59, 0x52,
	0x5C5C, 0x48,
	0x5C5E, 0x48,
	0x5EDA, 0x02,
	0x5EDB, 0x02,
	0x5EDF, 0x02,
	0x5EE7, 0x02,
	0x5EE8, 0x02,
	0x5EEC, 0x02,
	0x5F3E, 0x19,
	0x5F40, 0x0F,
	0x5F42, 0x05,
	0x5F50, 0x32,
	0x5F52, 0x32,
	0x5F54, 0x2D,
	0x6069, 0x15,
	0x606B, 0x15,
	0x606F, 0x14,
	0x607D, 0x14,
	0x607F, 0x14,
	0x6083, 0x14,
	0x60E9, 0x1B,
	0x6104, 0x00,
	0x6105, 0x1C,
	0x6108, 0x00,
	0x6109, 0x1C,
	0x6110, 0x00,
	0x6111, 0x1C,
	0x61C9, 0x5F,
	0x61E4, 0x00,
	0x61E5, 0x60,
	0x61E8, 0x00,
	0x61E9, 0x60,
	0x61F0, 0x00,
	0x61F1, 0x60,
	0x9003, 0x08,
	0x9004, 0x18,
	0x9200, 0x54,
	0x9201, 0x4A,
	0x9202, 0x54,
	0x9203, 0x4D,
	0x9204, 0x58,
	0x9205, 0x59,
	0x9206, 0x57,
	0x9207, 0x1A,
	0x9208, 0x50,
	0x9209, 0xF8,
	0x920A, 0x50,
	0x920B, 0xF9,
	0x9210, 0xEA,
	0x9211, 0x7A,
	0x9212, 0xEA,
	0x9213, 0x7D,
	0x9214, 0xEA,
	0x9215, 0x80,
	0x9216, 0xEA,
	0x9217, 0x83,
	0x9218, 0xEA,
	0x9219, 0x86,
	0x921A, 0xEA,
	0x921B, 0xB8,
	0x921C, 0xEA,
	0x921D, 0xB9,
	0x921E, 0xEA,
	0x921F, 0xBE,
	0x9220, 0xEA,
	0x9221, 0xBF,
	0x9222, 0xEA,
	0x9223, 0xC4,
	0x9224, 0xEA,
	0x9225, 0xC5,
	0x9226, 0xEA,
	0x9227, 0xCA,
	0x9228, 0xEA,
	0x9229, 0xCB,
	0x922A, 0xEA,
	0x922B, 0xD0,
	0x922C, 0xEA,
	0x922D, 0xD1,
	0x922E, 0x91,
	0x922F, 0x2A,
	0x9230, 0xE2,
	0x9231, 0xC0,
	0x9232, 0xE2,
	0x9233, 0xC1,
	0x9234, 0xE2,
	0x9235, 0xC2,
	0x9236, 0xE2,
	0x9237, 0xC3,
	0x9238, 0xE2,
	0x9239, 0xD4,
	0x923A, 0xE2,
	0x923B, 0xD5,
	0x923C, 0x43,
	0x923D, 0x28,
	0x923E, 0x43,
	0x923F, 0x29,
	0xBC76, 0x10,
	0xBC7A, 0x34,
	0xBC7B, 0xA0,
	0xBC7C, 0x12,
	0xBC7D, 0xB0,
	0xBC7E, 0x1B,
	0xBC80, 0x1B,
	0xBC81, 0x90,
	0xBC82, 0x16,
	0xBC83, 0x60,
	0xBC84, 0x3F,
	0xBC85, 0xF0,
	0xBC86, 0x15,
	0xBC87, 0xE0,
	0xBC88, 0x19,
	0xBC89, 0xD0,
	/*Image Quality*/
	0x3547, 0x00,
	0x3549, 0x00,
	0x354B, 0x00,
	0x354D, 0x00,
	0x85B1, 0x01,
	0x9865, 0xA0,
	0x9866, 0x14,
	0x9867, 0x0A,
	0x98D7, 0xB4,
	0x98D8, 0x8C,
	0x98DA, 0xA0,
	0x98DB, 0x78,
	0x98DC, 0x50,
	0x99B8, 0x17,
	0x99BA, 0x17,
	0x99C4, 0x16,
	0x9A12, 0x15,
	0x9A13, 0x15,
	0x9A14, 0x15,
	0x9A15, 0x0B,
	0x9A16, 0x0B,
	0x9A49, 0x0B,
	0x9A4A, 0x0B,
	0xA503, 0x04,
	0xA539, 0x03,
	0xA53A, 0x03,
	0xA53B, 0x03,
	0xA575, 0x03,
	0xA576, 0x03,
	0xA577, 0x03,
	0xA57A, 0x80,
	0xA660, 0x01,
	0xA661, 0x69,
	0xA66C, 0x01,
	0xA66D, 0x27,
	0xA673, 0x40,
	0xA675, 0x40,
	0xA677, 0x43,
	0xA67D, 0x06,
	0xA6DE, 0x01,
	0xA6DF, 0x69,
	0xA6EA, 0x01,
	0xA6EB, 0x27,
	0xA6F1, 0x40,
	0xA6F3, 0x40,
	0xA6F5, 0x43,
	0xA6FB, 0x06,
	0xA76D, 0x40,
	0xA76F, 0x40,
	0xA771, 0x43,
	0xAA37, 0x76,
	0xAA39, 0xAC,
	0xAA3B, 0xC8,
	0xAA3D, 0x76,
	0xAA3F, 0xAC,
	0xAA41, 0xC8,
	0xAA43, 0x76,
	0xAA45, 0xAC,
	0xAA47, 0xC8,
	0xAD1C, 0x01,
	0xAD1D, 0x3D,
	0xAD23, 0x4F,
	0xAD4C, 0x01,
	0xAD4D, 0x3D,
	0xAD53, 0x4F,
	0xAD7C, 0x01,
	0xAD7D, 0x3D,
	0xAD83, 0x4F,
	0xADAC, 0x01,
	0xADAD, 0x3D,
	0xADB3, 0x4F,
	0xAE00, 0x01,
	0xAE01, 0xA9,
	0xAE02, 0x01,
	0xAE03, 0xA9,
	0xAE05, 0x86,
	0xAE0D, 0x10,
	0xAE0F, 0x10,
	0xAE11, 0x10,
	0xAE24, 0x03,
	0xAE25, 0x03,
	0xAE26, 0x02,
	0xAE27, 0x49,
	0xAE28, 0x01,
	0xAE29, 0x3B,
	0xAE31, 0x10,
	0xAE33, 0x10,
	0xAE35, 0x10,
	0xAE48, 0x02,
	0xAE4A, 0x01,
	0xAE4B, 0x80,
	0xAE4D, 0x80,
	0xAE55, 0x10,
	0xAE57, 0x10,
	0xAE59, 0x10,
	0xAE6C, 0x01,
	0xAE6D, 0xC1,
	0xAE6F, 0xA5,
	0xAE79, 0x10,
	0xAE7B, 0x10,
	0xAE7D, 0x13,
	0xAE90, 0x04,
	0xAE91, 0xB0,
	0xAE92, 0x01,
	0xAE93, 0x70,
	0xAE94, 0x01,
	0xAE95, 0x3B,
	0xAE9D, 0x10,
	0xAE9F, 0x10,
	0xAEA1, 0x10,
	0xAEB4, 0x02,
	0xAEB5, 0xCB,
	0xAEB6, 0x01,
	0xAEB7, 0x58,
	0xAEB9, 0xB4,
	0xAEC1, 0x10,
	0xAEC3, 0x10,
	0xAEC5, 0x10,
	0xAF01, 0x13,
	0xAF02, 0x00,
	0xAF08, 0x78,
	0xAF09, 0x6E,
	0xAF0A, 0x64,
	0xAF0B, 0x5A,
	0xAF0C, 0x50,
	0xAF0D, 0x46,
	0xAF0E, 0x3C,
	0xAF0F, 0x32,
	0xAF10, 0x28,
	0xAF11, 0x00,
	0xAF17, 0x50,
	0xAF18, 0x3C,
	0xAF19, 0x28,
	0xAF1A, 0x14,
	0xAF1B, 0x00,
	0xAF26, 0xA0,
	0xAF27, 0x96,
	0xAF28, 0x8C,
	0xAF29, 0x82,
	0xAF2A, 0x78,
	0xAF2B, 0x6E,
	0xAF2C, 0x64,
	0xAF2D, 0x5A,
	0xAF2E, 0x50,
	0xAF2F, 0x00,
	0xAF31, 0x96,
	0xAF32, 0x8C,
	0xAF33, 0x82,
	0xAF34, 0x78,
	0xAF35, 0x6E,
	0xAF36, 0x64,
	0xAF38, 0x3C,
	0xAF39, 0x00,
	0xAF3A, 0xA0,
	0xAF3B, 0x96,
	0xAF3C, 0x8C,
	0xAF3D, 0x82,
	0xAF3E, 0x78,
	0xAF3F, 0x6E,
	0xAF40, 0x64,
	0xAF41, 0x50,
	0xAF94, 0x03,
	0xAF95, 0x02,
	0xAF96, 0x02,
	0xAF99, 0x01,
	0xAF9B, 0x02,
	0xAFA5, 0x01,
	0xAFA7, 0x03,
	0xAFB4, 0x02,
	0xAFB5, 0x02,
	0xAFB6, 0x03,
	0xAFB7, 0x03,
	0xAFB8, 0x03,
	0xAFB9, 0x04,
	0xAFBA, 0x04,
	0xAFBC, 0x03,
	0xAFBD, 0x03,
	0xAFBE, 0x02,
	0xAFBF, 0x02,
	0xAFC0, 0x02,
	0xAFC3, 0x01,
	0xAFC5, 0x03,
	0xAFC6, 0x04,
	0xAFC7, 0x04,
	0xAFC8, 0x03,
	0xAFC9, 0x03,
	0xAFCA, 0x02,
	0xAFCC, 0x01,
	0xAFCE, 0x02,
	0xB02A, 0x00,
	0xB02E, 0x02,
	0xB030, 0x02,
	0xB501, 0x02,
	0xB503, 0x02,
	0xB505, 0x02,
	0xB507, 0x02,
	0xB515, 0x00,
	0xB517, 0x00,
	0xB519, 0x02,
	0xB51F, 0x00,
	0xB521, 0x01,
	0xB527, 0x02,
	0xB53D, 0x01,
	0xB53F, 0x02,
	0xB541, 0x02,
	0xB543, 0x02,
	0xB545, 0x02,
	0xB547, 0x02,
	0xB54B, 0x03,
	0xB54D, 0x03,
	0xB551, 0x02,
	0xB553, 0x02,
	0xB555, 0x02,
	0xB557, 0x02,
	0xB559, 0x02,
	0xB55B, 0x02,
	0xB55D, 0x01,
	0xB563, 0x02,
	0xB565, 0x03,
	0xB567, 0x03,
	0xB569, 0x02,
	0xB56B, 0x02,
	0xB58D, 0xE7,
	0xB58F, 0xCC,
	0xB591, 0xAD,
	0xB593, 0x88,
	0xB595, 0x66,
	0xB597, 0x88,
	0xB599, 0xAD,
	0xB59B, 0xCC,
	0xB59D, 0xE7,
	0xB5A1, 0x2A,
	0xB5A3, 0x1A,
	0xB5A5, 0x27,
	0xB5A7, 0x1A,
	0xB5A9, 0x2A,
	0xB5AB, 0x3C,
	0xB5AD, 0x59,
	0xB5AF, 0x77,
	0xB5B1, 0x9A,
	0xB5B3, 0xE9,
	0xB5C9, 0x5B,
	0xB5CB, 0x73,
	0xB5CD, 0x9D,
	0xB5CF, 0xBA,
	0xB5D1, 0xD9,
	0xB5D3, 0xED,
	0xB5D5, 0xF9,
	0xB5D7, 0xFE,
	0xB5D8, 0x01,
	0xB5D9, 0x00,
	0xB5DA, 0x01,
	0xB5DB, 0x00,
	0xB5DD, 0xF6,
	0xB5DF, 0xE9,
	0xB5E1, 0xD1,
	0xB5E3, 0xBB,
	0xB5E5, 0x9A,
	0xB5E7, 0x77,
	0xB5E9, 0x59,
	0xB5EB, 0x77,
	0xB5ED, 0x9A,
	0xB5EF, 0xE9,
	0xB600, 0x01,
	0xB601, 0x00,
	0xB603, 0xFE,
	0xB605, 0xF8,
	0xB607, 0xED,
	0xB609, 0xD4,
	0xB60B, 0xB7,
	0xB60D, 0x93,
	0xB60F, 0xB7,
	0xB611, 0xD4,
	0xB612, 0x00,
	0xB613, 0xFE,
	0xB628, 0x00,
	0xB629, 0xAA,
	0xB62A, 0x00,
	0xB62B, 0x78,
	0xB62D, 0x55,
	0xB62F, 0x3E,
	0xB631, 0x2B,
	0xB633, 0x20,
	0xB635, 0x18,
	0xB637, 0x12,
	0xB639, 0x0E,
	0xB63B, 0x06,
	0xB63C, 0x02,
	0xB63D, 0xAA,
	0xB63E, 0x02,
	0xB63F, 0x00,
	0xB640, 0x01,
	0xB641, 0x99,
	0xB642, 0x01,
	0xB643, 0x24,
	0xB645, 0xCC,
	0xB647, 0x66,
	0xB649, 0x38,
	0xB64B, 0x21,
	0xB64D, 0x14,
	0xB64F, 0x0E,
	0xB664, 0x00,
	0xB665, 0xCC,
	0xB666, 0x00,
	0xB667, 0x92,
	0xB669, 0x66,
	0xB66B, 0x4B,
	0xB66D, 0x34,
	0xB66F, 0x28,
	0xB671, 0x1E,
	0xB673, 0x18,
	0xB675, 0x11,
	0xB677, 0x08,
	0xB678, 0x04,
	0xB679, 0x00,
	0xB67A, 0x04,
	0xB67B, 0x00,
	0xB67C, 0x02,
	0xB67D, 0xAA,
	0xB67E, 0x02,
	0xB67F, 0x00,
	0xB680, 0x01,
	0xB681, 0x99,
	0xB682, 0x01,
	0xB683, 0x24,
	0xB685, 0xCC,
	0xB687, 0x66,
	0xB689, 0x38,
	0xB68B, 0x0E,
	0xB68C, 0x02,
	0xB68D, 0xAA,
	0xB68E, 0x02,
	0xB68F, 0x00,
	0xB690, 0x01,
	0xB691, 0x99,
	0xB692, 0x01,
	0xB693, 0x24,
	0xB695, 0xE3,
	0xB697, 0x9D,
	0xB699, 0x71,
	0xB69B, 0x37,
	0xB69D, 0x1F,
	0xE869, 0x00,
	0xE877, 0x00,
	0xEE01, 0x30,
	0xEE03, 0x30,
	0xEE07, 0x08,
	0xEE09, 0x08,
	0xEE0B, 0x08,
	0xEE0D, 0x30,
	0xEE0F, 0x30,
	0xEE12, 0x00,
	0xEE13, 0x10,
	0xEE14, 0x00,
	0xEE15, 0x10,
	0xEE16, 0x00,
	0xEE17, 0x10,
	0xEE31, 0x30,
	0xEE33, 0x30,
	0xEE3D, 0x30,
	0xEE3F, 0x30,
	0xF645, 0x40,
	0xF646, 0x01,
	0xF647, 0x00,
#else
	/*External Clock Setting*/
	0x0136, 0x18,
	0x0137, 0x00,
	/*Register version*/
	0x33F0, 0x03,
	0x33F1, 0x05,
	/*Signaling mode setting*/
	0x0111, 0x03,
	/*PDAF TYPE2 data type Setting*/
	0x3076, 0x00,
	0x3077, 0x30,
	/*Global Setting*/
	0x31C0, 0x01,
	0x33BC, 0x00,
	0x4000, 0x10,
	0x4001, 0x10,
	0x4002, 0x10,
	0x4003, 0x10,
	0x4004, 0x10,
	0x4007, 0x01,
	0x4009, 0x01,
	0x400B, 0x01,
	0x400D, 0x01,
	0x4328, 0x00,
	0x4329, 0xB4,
	0x4E08, 0x4B,
	0x4E21, 0x35,
	0x4E25, 0x10,
	0x4E2F, 0x25,
	0x4E3B, 0xB5,
	0x4E49, 0x21,
	0x4E57, 0x3F,
	0x4E63, 0xAB,
	0x4E6B, 0x44,
	0x4E6F, 0x19,
	0x4E71, 0x62,
	0x4E73, 0x5D,
	0x4E75, 0xAB,
	0x4E87, 0x2B,
	0x4E8B, 0x10,
	0x4E91, 0xAF,
	0x4E95, 0x4E,
	0x4EA1, 0xF1,
	0x4EAB, 0x4C,
	0x4EBF, 0x4E,
	0x4EC3, 0x19,
	0x4EC5, 0x71,
	0x4EC7, 0x5D,
	0x4EC9, 0xF1,
	0x4ECB, 0x6F,
	0x4ECD, 0x5D,
	0x4EDF, 0x2B,
	0x4EE3, 0x0E,
	0x4EED, 0x27,
	0x4EF9, 0xAB,
	0x4F01, 0x4E,
	0x4F05, 0x19,
	0x4F07, 0x4A,
	0x4F09, 0x5D,
	0x4F0B, 0xAB,
	0x4F19, 0x83,
	0x4F1D, 0x3C,
	0x4F26, 0x01,
	0x4F27, 0x07,
	0x4F32, 0x04,
	0x4F33, 0x11,
	0x4F3C, 0x4B,
	0x4F59, 0x2D,
	0x4F5D, 0x5A,
	0x4F63, 0x46,
	0x4F69, 0x9E,
	0x4F6E, 0x03,
	0x4F6F, 0x23,
	0x4F81, 0x27,
	0x4F85, 0x5A,
	0x4F8B, 0x62,
	0x4F91, 0x9E,
	0x4F96, 0x03,
	0x4F97, 0x39,
	0x4F9F, 0x41,
	0x4FA3, 0x19,
	0x4FA5, 0xA3,
	0x4FA7, 0x5D,
	0x4FA8, 0x03,
	0x4FA9, 0x39,
	0x4FBF, 0x4A,
	0x4FC3, 0x5A,
	0x4FC5, 0xE5,
	0x4FC9, 0x83,
	0x4FCF, 0x9E,
	0x4FD5, 0xD0,
	0x4FE5, 0x9E,
	0x4FE9, 0xE5,
	0x4FF3, 0x41,
	0x4FF7, 0x19,
	0x4FF9, 0x98,
	0x4FFB, 0x5D,
	0x4FFD, 0xD0,
	0x4FFF, 0xA5,
	0x5001, 0x5D,
	0x5003, 0xE5,
	0x5017, 0x07,
	0x5021, 0x36,
	0x5035, 0x2D,
	0x5039, 0x19,
	0x503B, 0x63,
	0x503D, 0x5D,
	0x5051, 0x42,
	0x5055, 0x5A,
	0x505B, 0xC7,
	0x5061, 0x9E,
	0x5067, 0xD5,
	0x5079, 0x15,
	0x5083, 0x20,
	0x509D, 0x5A,
	0x509F, 0x5A,
	0x50A1, 0x5A,
	0x50A5, 0x5A,
	0x50B5, 0x9E,
	0x50B7, 0x9E,
	0x50B9, 0x9E,
	0x50BD, 0x9E,
	0x50C7, 0x9E,
	0x544A, 0xE0,
	0x544D, 0xE2,
	0x551C, 0x03,
	0x551F, 0x64,
	0x5521, 0xD2,
	0x5523, 0x64,
	0x5549, 0x5A,
	0x554B, 0x9E,
	0x554D, 0x5A,
	0x554F, 0x9E,
	0x5551, 0x5A,
	0x5553, 0x9E,
	0x5559, 0x5A,
	0x555B, 0x9E,
	0x5561, 0x9E,
	0x55CD, 0x5A,
	0x55CF, 0x9E,
	0x55D1, 0x5A,
	0x55D3, 0x9E,
	0x55D5, 0x5A,
	0x55D7, 0x9E,
	0x55DD, 0x5A,
	0x55DF, 0x9E,
	0x55E7, 0x9E,
	0x571A, 0x00,
	0x581B, 0x46,
	0x5839, 0x8A,
	0x5852, 0x00,
	0x59C7, 0x10,
	0x59CB, 0x40,
	0x59D1, 0x01,
	0x59EB, 0x00,
	0x5A27, 0x01,
	0x5A46, 0x09,
	0x5A47, 0x09,
	0x5A48, 0x09,
	0x5A49, 0x13,
	0x5A50, 0x0D,
	0x5A51, 0x0D,
	0x5A52, 0x0D,
	0x5A53, 0x0D,
	0x5A54, 0x03,
	0x5B0A, 0x04,
	0x5B0B, 0x04,
	0x5B0C, 0x04,
	0x5B0D, 0x04,
	0x5B0E, 0x04,
	0x5B0F, 0x04,
	0x5B10, 0x04,
	0x5B11, 0x04,
	0x5B12, 0x04,
	0x5B13, 0x04,
	0x5B1A, 0x08,
	0x5B1E, 0x04,
	0x5B1F, 0x04,
	0x5B20, 0x04,
	0x5B21, 0x04,
	0x5B22, 0x08,
	0x5B23, 0x08,
	0x5B24, 0x04,
	0x5B25, 0x08,
	0x5B26, 0x04,
	0x5B27, 0x08,
	0x5B32, 0x04,
	0x5B33, 0x04,
	0x5B34, 0x04,
	0x5B35, 0x04,
	0x5B38, 0x04,
	0x5B3A, 0x04,
	0x5B3E, 0x10,
	0x5B40, 0x10,
	0x5B46, 0x08,
	0x5B47, 0x04,
	0x5B48, 0x04,
	0x5B49, 0x08,
	0x5B4C, 0x08,
	0x5B4E, 0x08,
	0x5B52, 0x1F,
	0x5B53, 0x1F,
	0x5B57, 0x04,
	0x5B58, 0x04,
	0x5B5E, 0x1F,
	0x5B5F, 0x1F,
	0x5B63, 0x08,
	0x5B64, 0x08,
	0x5B68, 0x1F,
	0x5B69, 0x1F,
	0x5B6C, 0x1F,
	0x5B6D, 0x1F,
	0x5B72, 0x06,
	0x5B76, 0x07,
	0x5B7E, 0x10,
	0x5B7F, 0x10,
	0x5B81, 0x10,
	0x5B83, 0x10,
	0x5B86, 0x07,
	0x5B88, 0x07,
	0x5B8A, 0x07,
	0x5B98, 0x08,
	0x5B99, 0x08,
	0x5B9A, 0x09,
	0x5B9B, 0x08,
	0x5B9C, 0x07,
	0x5B9D, 0x08,
	0x5B9F, 0x10,
	0x5BA2, 0x10,
	0x5BA5, 0x10,
	0x5BA8, 0x10,
	0x5BAA, 0x10,
	0x5BAC, 0x0C,
	0x5BAD, 0x0C,
	0x5BAE, 0x0A,
	0x5BAF, 0x0C,
	0x5BB0, 0x07,
	0x5BB1, 0x0C,
	0x5BC0, 0x11,
	0x5BC1, 0x10,
	0x5BC4, 0x10,
	0x5BC5, 0x10,
	0x5BC7, 0x10,
	0x5BC8, 0x10,
	0x5BCC, 0x0B,
	0x5BCD, 0x0C,
	0x5BE5, 0x03,
	0x5BE6, 0x03,
	0x5BE7, 0x03,
	0x5BE8, 0x03,
	0x5BE9, 0x03,
	0x5BEA, 0x03,
	0x5BEB, 0x03,
	0x5BEC, 0x03,
	0x5BED, 0x03,
	0x5BF3, 0x03,
	0x5BF4, 0x03,
	0x5BF5, 0x03,
	0x5BF6, 0x03,
	0x5BF7, 0x03,
	0x5BF8, 0x03,
	0x5BF9, 0x03,
	0x5BFA, 0x03,
	0x5BFB, 0x03,
	0x5C01, 0x03,
	0x5C02, 0x03,
	0x5C03, 0x03,
	0x5C04, 0x03,
	0x5C05, 0x03,
	0x5C06, 0x03,
	0x5C07, 0x03,
	0x5C08, 0x03,
	0x5C09, 0x03,
	0x5C0F, 0x03,
	0x5C10, 0x03,
	0x5C11, 0x03,
	0x5C12, 0x03,
	0x5C13, 0x03,
	0x5C14, 0x03,
	0x5C15, 0x03,
	0x5C16, 0x03,
	0x5C17, 0x03,
	0x5C1A, 0x03,
	0x5C1B, 0x03,
	0x5C1C, 0x03,
	0x5C1D, 0x03,
	0x5C1E, 0x03,
	0x5C1F, 0x03,
	0x5C20, 0x03,
	0x5C21, 0x03,
	0x5C22, 0x03,
	0x5C25, 0x03,
	0x5C26, 0x03,
	0x5C27, 0x03,
	0x5C28, 0x03,
	0x5C29, 0x03,
	0x5C2A, 0x03,
	0x5C2B, 0x03,
	0x5C2C, 0x03,
	0x5C2D, 0x03,
	0x5C2E, 0x03,
	0x5C2F, 0x03,
	0x5C30, 0x03,
	0x5C31, 0x03,
	0x5C32, 0x03,
	0x5C33, 0x03,
	0x5C34, 0x03,
	0x5C35, 0x03,
	0x5C46, 0x62,
	0x5C4D, 0x6C,
	0x5C53, 0x62,
	0x5C58, 0x62,
	0x5EDD, 0x05,
	0x5EDE, 0x05,
	0x5EDF, 0x05,
	0x5EE3, 0x05,
	0x5EEA, 0x05,
	0x5EEB, 0x05,
	0x5EEC, 0x05,
	0x5EF0, 0x05,
	0x5EF7, 0x05,
	0x5EF8, 0x05,
	0x5EF9, 0x05,
	0x5EFD, 0x05,
	0x5F04, 0x05,
	0x5F05, 0x05,
	0x5F06, 0x05,
	0x5F0A, 0x05,
	0x5F0E, 0x05,
	0x5F0F, 0x05,
	0x5F10, 0x05,
	0x5F14, 0x05,
	0x5F18, 0x05,
	0x5F19, 0x05,
	0x5F1A, 0x05,
	0x5F1E, 0x05,
	0x5F20, 0x05,
	0x5F24, 0x05,
	0x5F36, 0x1E,
	0x5F38, 0x1E,
	0x5F3A, 0x1E,
	0x6081, 0x10,
	0x6082, 0x10,
	0x6085, 0x10,
	0x6088, 0x10,
	0x608B, 0x10,
	0x608D, 0x10,
	0x6095, 0x0C,
	0x6096, 0x0C,
	0x6099, 0x0C,
	0x609C, 0x0C,
	0x609D, 0x04,
	0x609E, 0x04,
	0x609F, 0x0C,
	0x60A1, 0x0C,
	0x60A2, 0x04,
	0x60A9, 0x0C,
	0x60AA, 0x0C,
	0x60AB, 0x10,
	0x60AC, 0x10,
	0x60AD, 0x0C,
	0x60AE, 0x10,
	0x60AF, 0x10,
	0x60B0, 0x0C,
	0x60B1, 0x04,
	0x60B2, 0x04,
	0x60B3, 0x0C,
	0x60B5, 0x0C,
	0x60B6, 0x04,
	0x60B9, 0x04,
	0x60BA, 0x04,
	0x60BB, 0x0C,
	0x60BC, 0x0C,
	0x60BE, 0x0C,
	0x60BF, 0x0C,
	0x60C0, 0x04,
	0x60C1, 0x04,
	0x60C5, 0x04,
	0x60C6, 0x04,
	0x60C7, 0x0C,
	0x60C8, 0x0C,
	0x60CA, 0x0C,
	0x60CB, 0x0C,
	0x60CC, 0x04,
	0x60CD, 0x04,
	0x60CF, 0x04,
	0x60D0, 0x04,
	0x60D3, 0x04,
	0x60D4, 0x04,
	0x60DD, 0x19,
	0x60E1, 0x19,
	0x60E9, 0x19,
	0x60EB, 0x19,
	0x60EF, 0x19,
	0x60F1, 0x19,
	0x60F9, 0x19,
	0x60FD, 0x19,
	0x610D, 0x2D,
	0x610F, 0x2D,
	0x6115, 0x2D,
	0x611B, 0x2D,
	0x6121, 0x2D,
	0x6125, 0x2D,
	0x6135, 0x3C,
	0x6137, 0x3C,
	0x613D, 0x3C,
	0x6143, 0x3C,
	0x6145, 0x5A,
	0x6147, 0x5A,
	0x6149, 0x3C,
	0x614D, 0x3C,
	0x614F, 0x5A,
	0x615D, 0x3C,
	0x615F, 0x3C,
	0x6161, 0x2D,
	0x6163, 0x2D,
	0x6165, 0x3C,
	0x6167, 0x2D,
	0x6169, 0x2D,
	0x616B, 0x3C,
	0x616D, 0x5A,
	0x616F, 0x5A,
	0x6171, 0x3C,
	0x6175, 0x3C,
	0x6177, 0x5A,
	0x617D, 0x5A,
	0x617F, 0x5A,
	0x6181, 0x3C,
	0x6183, 0x3C,
	0x6187, 0x3C,
	0x6189, 0x3C,
	0x618B, 0x5A,
	0x618D, 0x5A,
	0x6195, 0x5A,
	0x6197, 0x5A,
	0x6199, 0x3C,
	0x619B, 0x3C,
	0x619F, 0x3C,
	0x61A1, 0x3C,
	0x61A3, 0x5A,
	0x61A5, 0x5A,
	0x61A9, 0x5A,
	0x61AB, 0x5A,
	0x61B1, 0x5A,
	0x61B3, 0x5A,
	0x61BD, 0x5D,
	0x61C1, 0x5D,
	0x61C9, 0x5D,
	0x61CB, 0x5D,
	0x61CF, 0x5D,
	0x61D1, 0x5D,
	0x61D9, 0x5D,
	0x61DD, 0x5D,
	0x61ED, 0x71,
	0x61EF, 0x71,
	0x61F5, 0x71,
	0x61FB, 0x71,
	0x6201, 0x71,
	0x6205, 0x71,
	0x6215, 0x80,
	0x6217, 0x80,
	0x621D, 0x80,
	0x6223, 0x80,
	0x6225, 0x9E,
	0x6227, 0x9E,
	0x6229, 0x80,
	0x622D, 0x80,
	0x622F, 0x9E,
	0x623D, 0x80,
	0x623F, 0x80,
	0x6241, 0x71,
	0x6243, 0x71,
	0x6245, 0x80,
	0x6247, 0x71,
	0x6249, 0x71,
	0x624B, 0x80,
	0x624D, 0x9E,
	0x624F, 0x9E,
	0x6251, 0x80,
	0x6255, 0x80,
	0x6257, 0x9E,
	0x625D, 0x9E,
	0x625F, 0x9E,
	0x6261, 0x80,
	0x6263, 0x80,
	0x6267, 0x80,
	0x6269, 0x80,
	0x626B, 0x9E,
	0x626D, 0x9E,
	0x6275, 0x9E,
	0x6277, 0x9E,
	0x6279, 0x80,
	0x627B, 0x80,
	0x627F, 0x80,
	0x6281, 0x80,
	0x6283, 0x9E,
	0x6285, 0x9E,
	0x6289, 0x9E,
	0x628B, 0x9E,
	0x6291, 0x9E,
	0x6293, 0x9E,
	0x629B, 0x5D,
	0x629F, 0x5D,
	0x62A1, 0x5D,
	0x62A5, 0x5D,
	0x62BF, 0x9E,
	0x62CD, 0x9E,
	0x62D3, 0x9E,
	0x62D9, 0x9E,
	0x62DD, 0x9E,
	0x62E3, 0x9E,
	0x62E5, 0x9E,
	0x62E7, 0x9E,
	0x62E9, 0x9E,
	0x62EB, 0x9E,
	0x62F3, 0x28,
	0x630E, 0x28,
	0x6481, 0x0D,
	0x648A, 0x0D,
	0x648B, 0x0D,
	0x64A3, 0x0B,
	0x64A4, 0x0B,
	0x64A5, 0x0B,
	0x64A9, 0x0B,
	0x64AF, 0x0B,
	0x64B0, 0x0B,
	0x64B1, 0x0B,
	0x64B5, 0x0B,
	0x64BB, 0x0B,
	0x64BC, 0x0B,
	0x64BD, 0x0B,
	0x64C1, 0x0B,
	0x64C7, 0x0B,
	0x64C8, 0x0B,
	0x64C9, 0x0B,
	0x64CD, 0x0B,
	0x64D0, 0x0B,
	0x64D1, 0x0B,
	0x64D2, 0x0B,
	0x64D6, 0x0B,
	0x64D9, 0x0B,
	0x64DA, 0x0B,
	0x64DB, 0x0B,
	0x64DF, 0x0B,
	0x64E0, 0x0B,
	0x64E4, 0x0B,
	0x64ED, 0x05,
	0x64EE, 0x05,
	0x64EF, 0x05,
	0x64F3, 0x05,
	0x64F9, 0x05,
	0x64FA, 0x05,
	0x64FB, 0x05,
	0x64FF, 0x05,
	0x6505, 0x05,
	0x6506, 0x05,
	0x6507, 0x05,
	0x650B, 0x05,
	0x6511, 0x05,
	0x6512, 0x05,
	0x6513, 0x05,
	0x6517, 0x05,
	0x651A, 0x05,
	0x651B, 0x05,
	0x651C, 0x05,
	0x6520, 0x05,
	0x6523, 0x05,
	0x6524, 0x05,
	0x6525, 0x05,
	0x6529, 0x05,
	0x652A, 0x05,
	0x652E, 0x05,
	0x7314, 0x02,
	0x7315, 0x40,
	0x7600, 0x03,
	0x7630, 0x04,
	0x8744, 0x00,
	0x9004, 0x0F,
	0x9200, 0xEA,
	0x9201, 0x7A,
	0x9202, 0xEA,
	0x9203, 0x7D,
	0x9204, 0xEA,
	0x9205, 0x80,
	0x9206, 0xEA,
	0x9207, 0x83,
	0x9208, 0xEA,
	0x9209, 0x86,
	0x920A, 0xEA,
	0x920B, 0xB8,
	0x920C, 0xEA,
	0x920D, 0xB9,
	0x920E, 0xEA,
	0x920F, 0xBE,
	0x9210, 0xEA,
	0x9211, 0xBF,
	0x9212, 0xEA,
	0x9213, 0xC4,
	0x9214, 0xEA,
	0x9215, 0xC5,
	0x9216, 0xEA,
	0x9217, 0xCA,
	0x9218, 0xEA,
	0x9219, 0xCB,
	0x921A, 0xEA,
	0x921B, 0xD0,
	0x921C, 0xEA,
	0x921D, 0xD1,
	0xB0BE, 0x04,
	0xC5C6, 0x01,
	0xC5D8, 0x3F,
	0xC5DA, 0x35,
	0xE70E, 0x06,
	0xE70F, 0x0C,
	0xE710, 0x00,
	0xE711, 0x00,
	0xE712, 0x00,
	0xE713, 0x00,
#endif
};



static kal_uint16 imx686_preview_setting[] = {
#ifdef use_capture_setting
	/*MIPI output setting*/
	0x0112,  0x0A,
	0x0113,  0x0A,
	0x0114,  0x02,
	/*Line Length PCK Setting*/
	0x0342,  0x27,
	0x0343,  0xE0,
	/*Frame Length Lines Setting*/
	0x0340,  0x0D,
	0x0341,  0xFA,
	/*ROI Setting*/
	0x0344,  0x00,
	0x0345,  0x00,
	0x0346,  0x00,
	0x0347,  0x10,
	0x0348,  0x24,
	0x0349,  0x1F,
	0x034A,  0x1B,
	0x034B,  0x0F,
	/*Mode Setting*/
	0x0220,  0x62,
	0x0221,  0x11,
	0x0222,  0x01,
	0x0900,  0x01,
	0x0901,  0x22,
	0x0902,  0x08,
	0x30D8,  0x04,
	0x3200,  0x41,
	0x3201,  0x41,
	0x350C,  0x00,
	0x350D,  0x00,
	/*Digital Crop & Scaling*/
	0x0408,  0x00,
	0x0409,  0x08,
	0x040A,  0x00,
	0x040B,  0x00,
	0x040C,  0x12,
	0x040D,  0x00,
	0x040E,  0x0D,
	0x040F,  0x80,
	/*Output Size Setting*/
	0x034C,  0x12,
	0x034D,  0x00,
	0x034E,  0x0D,
	0x034F,  0x80,
	/*Clock Setting*/
	0x0301,  0x08,
	0x0303,  0x04,
	0x0305,  0x03,
	0x0306,  0x01,
	0x0307,  0x12,
	0x030B,  0x04,
	0x030D,  0x03,
	0x030E,  0x01,
	0x030F,  0xE6,
	0x0310,  0x01,
	/*Other Setting*/
	0x30D9,  0x00,
	0x32D5,  0x00,
	0x32D6,  0x00,
	0x403D,  0x05,
	0x403E,  0x00,
	0x403F,  0x78,
	0x40BC,  0x01,
	0x40BD,  0x6D,
	0x40BE,  0x00,
	0x40BF,  0x00,
	0x40B8,  0x01,
	0x40B9,  0x54,
	0x40A4,  0x00,
	0x40A5,  0x00,
	0x40A0,  0x00,
	0x40A1,  0x00,
	0x40A6,  0x00,
	0x40A7,  0x00,
	0x40AA,  0x00,
	0x40AB,  0x00,
	0x4000,  0xE0,
	0x4001,  0xE2,
	0x4002,  0x01,
	0x4003,  0x00,
	0x4004,  0xFF,
	0x4005,  0x00,
	0x4006,  0x00,
	0x4007,  0x00,
	0x401E,  0x00,
	0x401F,  0xCC,
	0x59EE,  0x00,
	0x59D1,  0x01,
	0xAF06,  0x07,
	0xAF07,  0xF1,
	/*Integration Setting*/
	0x0202,  0x0D,
	0x0203,  0xBA,
	0x0224,  0x01,
	0x0225,  0xF4,
	0x3116,  0x01,
	0x3117,  0xF4,
	/*Gain Setting*/
	0x0204,  0x00,
	0x0205,  0x00,
	0x0216,  0x00,
	0x0217,  0x00,
	0x0218,  0x01,
	0x0219,  0x00,
	0x020E,  0x01,
	0x020F,  0x00,
	0x3118,  0x00,
	0x3119,  0x00,
	0x311A,  0x01,
	0x311B,  0x00,
	/*PDAF Setting*/
	0x4018,  0x04,
	0x4019,  0x7C,
	0x401A,  0x00,
	0x401B,  0x01,
	/*HG MODE Setting*/
	0x30D0,  0x00,
	0x30D1,  0x00,
	0x30D2,  0x00,
	/*PDAF TYPE Setting*/
	0x3400,  0x02,
	/*PDAF TYPE1 Setting*/
	0x3091,  0x00,
	/*PDAF TYPE2 Setting*/
	0x3092,  0x01,
#else
	/*MIPI output setting*/
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x02,
	/*Line Length PCK Setting*/
	0x0342, 0x27,
	0x0343, 0xE0,
	/*Frame Length Lines Setting*/
	0x0340, 0x1C,
	0x0341, 0x00,
	/*ROI Setting*/
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x10,
	0x0348, 0x24,
	0x0349, 0x1F,
	0x034A, 0x1B,
	0x034B, 0x17,
	/*Mode Setting*/
	0x0220, 0x62,
	0x0221, 0x11,
	0x0222, 0x01,
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x08,
	0x30D8, 0x04,
	0x3200, 0x41,
	0x3201, 0x41,
	0x350C, 0x00,
	/*Digital Crop & Scaling*/
	0x0408, 0x00,
	0x0409, 0x08,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x12,
	0x040D, 0x00,
	0x040E, 0x0D,
	0x040F, 0x80,
	/*Output Size Setting*/
	0x034C, 0x12,
	0x034D, 0x00,
	0x034E, 0x0D,
	0x034F, 0x80,
	/*Clock Setting*/
	0x0301, 0x08,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x6E,
	0x030B, 0x02,
	0x030D, 0x04,
	0x030E, 0x02,
	0x030F, 0xA3,
	0x0310, 0x01,
	/*Other Setting*/
	0x30D9, 0x00,
	0x32D5, 0x00,
	0x32D6, 0x00,
	0x403D, 0x05,
	0x403E, 0x00,
	0x403F, 0x78,
	0x40A0, 0x00,
	0x40A1, 0x00,
	0x40A4, 0x00,
	0x40A5, 0x00,
	0x40A6, 0x00,
	0x40A7, 0x00,
	0x40B8, 0x01,
	0x40B9, 0x54,
	0x40BC, 0x01,
	0x40BD, 0x6D,
	0x40BE, 0x00,
	0x40BF, 0x32,
	0xAF06, 0x07,
	0xAF07, 0xF1,
	/*Integration Setting*/
	0x0202, 0x0D,
	0x0203, 0xB0,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x3116, 0x01,
	0x3117, 0xF4,
	/*Gain Setting*/
	0x0204, 0x00,
	0x0205, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x0218, 0x01,
	0x0219, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x3118, 0x00,
	0x3119, 0x00,
	0x311A, 0x01,
	0x311B, 0x00,
	/*PDAF TYPE2 Setting*/
	0x3400, 0x02,
	0x3092, 0x01,
	0x4324, 0x02,
	0x4325, 0x40,
#endif
};


static kal_uint16 imx686_capture_setting[] = {
#ifdef new_init_setting
	/*MIPI output setting*/
	0x0112,  0x0A,
	0x0113,  0x0A,
	0x0114,  0x02,
	/*Line Length PCK Setting*/
	0x0342,  0x27,
	0x0343,  0xE0,
	/*Frame Length Lines Setting*/
	0x0340,  0x0D,
	0x0341,  0xFA,
	/*ROI Setting*/
	0x0344,  0x00,
	0x0345,  0x00,
	0x0346,  0x00,
	0x0347,  0x10,
	0x0348,  0x24,
	0x0349,  0x1F,
	0x034A,  0x1B,
	0x034B,  0x0F,
	/*Mode Setting*/
	0x0220,  0x62,
	0x0221,  0x11,
	0x0222,  0x01,
	0x0900,  0x01,
	0x0901,  0x22,
	0x0902,  0x08,
	0x30D8,  0x04,
	0x3200,  0x41,
	0x3201,  0x41,
	0x350C,  0x00,
	0x350D,  0x00,
	/*Digital Crop & Scaling*/
	0x0408,  0x00,
	0x0409,  0x08,
	0x040A,  0x00,
	0x040B,  0x00,
	0x040C,  0x12,
	0x040D,  0x00,
	0x040E,  0x0D,
	0x040F,  0x80,
	/*Output Size Setting*/
	0x034C,  0x12,
	0x034D,  0x00,
	0x034E,  0x0D,
	0x034F,  0x80,
	/*Clock Setting*/
	0x0301,  0x08,
	0x0303,  0x04,
	0x0305,  0x03,
	0x0306,  0x01,
	0x0307,  0x12,
	0x030B,  0x04,
	0x030D,  0x03,
	0x030E,  0x01,
	0x030F,  0xE6,
	0x0310,  0x01,
	/*Other Setting*/
	0x30D9,  0x00,
	0x32D5,  0x00,
	0x32D6,  0x00,
	0x403D,  0x05,
	0x403E,  0x00,
	0x403F,  0x78,
	0x40BC,  0x01,
	0x40BD,  0x6D,
	0x40BE,  0x00,
	0x40BF,  0x00,
	0x40B8,  0x01,
	0x40B9,  0x54,
	0x40A4,  0x00,
	0x40A5,  0x00,
	0x40A0,  0x00,
	0x40A1,  0x00,
	0x40A6,  0x00,
	0x40A7,  0x00,
	0x40AA,  0x00,
	0x40AB,  0x00,
	0x4000,  0xE0,
	0x4001,  0xE2,
	0x4002,  0x01,
	0x4003,  0x00,
	0x4004,  0xFF,
	0x4005,  0x00,
	0x4006,  0x00,
	0x4007,  0x00,
	0x401E,  0x00,
	0x401F,  0xCC,
	0x59EE,  0x00,
	0x59D1,  0x01,
	0xAF06,  0x07,
	0xAF07,  0xF1,
	/*Integration Setting*/
	0x0202,  0x0D,
	0x0203,  0xBA,
	0x0224,  0x01,
	0x0225,  0xF4,
	0x3116,  0x01,
	0x3117,  0xF4,
	/*Gain Setting*/
	0x0204,  0x00,
	0x0205,  0x00,
	0x0216,  0x00,
	0x0217,  0x00,
	0x0218,  0x01,
	0x0219,  0x00,
	0x020E,  0x01,
	0x020F,  0x00,
	0x3118,  0x00,
	0x3119,  0x00,
	0x311A,  0x01,
	0x311B,  0x00,
	/*PDAF Setting*/
	0x4018,  0x04,
	0x4019,  0x7C,
	0x401A,  0x00,
	0x401B,  0x01,
	/*HG MODE Setting*/
	0x30D0,  0x00,
	0x30D1,  0x00,
	0x30D2,  0x00,
	/*PDAF TYPE Setting*/
	0x3400,  0x02,
	/*PDAF TYPE1 Setting*/
	0x3091,  0x00,
	/*PDAF TYPE2 Setting*/
	0x3092,  0x01,
#else
	/*MIPI output setting*/
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x02,
	/*Line Length PCK Setting*/
	0x0342, 0x37,
	0x0343, 0x40,
	/*Frame Length Lines Setting*/
	0x0340, 0x0E,
	0x0341, 0x14,
	/*ROI Setting*/
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x10,
	0x0348, 0x24,
	0x0349, 0x1F,
	0x034A, 0x1B,
	0x034B, 0x17,
	/*Mode Setting*/
	0x0220, 0x62,
	0x0221, 0x11,
	0x0222, 0x01,
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x08,
	0x30D8, 0x04,
	0x3200, 0x41,
	0x3201, 0x41,
	0x350C, 0x00,
	/*Digital Crop & Scaling*/
	0x0408, 0x00,
	0x0409, 0x08,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x12,
	0x040D, 0x00,
	0x040E, 0x0D,
	0x040F, 0x80,
	/*Output Size Setting*/
	0x034C, 0x12,
	0x034D, 0x00,
	0x034E, 0x0D,
	0x034F, 0x80,
	/*Clock Setting*/
	0x0301, 0x08,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x00,
	0x0307, 0xFF,
	0x030B, 0x04,
	0x030D, 0x04,
	0x030E, 0x02,
	0x030F, 0x88,
	0x0310, 0x01,
	/*Other Setting*/
	0x30D9, 0x00,
	0x32D5, 0x00,
	0x32D6, 0x00,
	0x403D, 0x05,
	0x403E, 0x00,
	0x403F, 0x78,
	0x40A0, 0x00,
	0x40A1, 0x00,
	0x40A4, 0x00,
	0x40A5, 0x00,
	0x40A6, 0x00,
	0x40A7, 0x00,
	0x40B8, 0x01,
	0x40B9, 0x54,
	0x40BC, 0x01,
	0x40BD, 0x6D,
	0x40BE, 0x00,
	0x40BF, 0x32,
	0xAF06, 0x07,
	0xAF07, 0xF1,
	/*Integration Setting*/
	0x0202, 0x0D,
	0x0203, 0xC4,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x3116, 0x01,
	0x3117, 0xF4,
	/*Gain Setting*/
	0x0204, 0x00,
	0x0205, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x0218, 0x01,
	0x0219, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x3118, 0x00,
	0x3119, 0x00,
	0x311A, 0x01,
	0x311B, 0x00,
	/*PDAF TYPE2 Setting*/
	0x3400, 0x02,
	0x3092, 0x01,
	0x4324, 0x02,
	0x4325, 0x40,
#endif
};

static kal_uint16 imx686_normal_video_setting_60FPS[] = {
#ifdef new_init_setting
	/*MIPI output setting*/
	0x0112,  0x0A,
	0x0113,  0x0A,
	0x0114,  0x02,
	/*Line Length PCK Setting*/
	0x0342,  0x27,
	0x0343,  0xE0,
	/*Frame Length Lines Setting*/
	0x0340,  0x0A,
	0x0341,  0xF8,
	/*ROI Setting*/
	0x0344,  0x00,
	0x0345,  0x00,
	0x0346,  0x03,
	0x0347,  0x70,
	0x0348,  0x24,
	0x0349,  0x1F,
	0x034A,  0x17,
	0x034B,  0xAF,
	/*Mode Setting*/
	0x0220,  0x62,
	0x0221,  0x11,
	0x0222,  0x01,
	0x0900,  0x01,
	0x0901,  0x22,
	0x0902,  0x08,
	0x30D8,  0x04,
	0x3200,  0x41,
	0x3201,  0x41,
	0x350C,  0x00,
	0x350D,  0x00,
	/*Digital Crop & Scaling*/
	0x0408,  0x00,
	0x0409,  0x08,
	0x040A,  0x00,
	0x040B,  0x00,
	0x040C,  0x12,
	0x040D,  0x00,
	0x040E,  0x0A,
	0x040F,  0x20,
	/*Output Size Setting*/
	0x034C,  0x12,
	0x034D,  0x00,
	0x034E,  0x0A,
	0x034F,  0x20,
	/*Clock Setting*/
	0x0301,  0x08,
	0x0303,  0x02,
	0x0305,  0x03,
	0x0306,  0x00,
	0x0307,  0xD7,
	0x030B,  0x02,
	0x030D,  0x03,
	0x030E,  0x01,
	0x030F,  0x80,
	0x0310,  0x01,
	/*Other Setting*/
	0x30D9,  0x00,
	0x32D5,  0x00,
	0x32D6,  0x00,
	0x403D,  0x05,
	0x403E,  0x00,
	0x403F,  0x78,
	0x40BC,  0x01,
	0x40BD,  0x6D,
	0x40BE,  0x00,
	0x40BF,  0x00,
	0x40B8,  0x01,
	0x40B9,  0x54,
	0x40A4,  0x00,
	0x40A5,  0x00,
	0x40A0,  0x00,
	0x40A1,  0x00,
	0x40A6,  0x00,
	0x40A7,  0x00,
	0x40AA,  0x00,
	0x40AB,  0x00,
	0x4000,  0xE0,
	0x4001,  0xE2,
	0x4002,  0x01,
	0x4003,  0x00,
	0x4004,  0xFF,
	0x4005,  0x00,
	0x4006,  0x00,
	0x4007,  0x00,
	0x401E,  0x00,
	0x401F,  0xCC,
	0x59EE,  0x00,
	0x59D1,  0x01,
	0xAF06,  0x07,
	0xAF07,  0xF1,
	/*Integration Setting*/
	0x0202,  0x0A,
	0x0203,  0xB8,
	0x0224,  0x01,
	0x0225,  0xF4,
	0x3116,  0x01,
	0x3117,  0xF4,
	/*Gain Setting*/
	0x0204,  0x00,
	0x0205,  0x00,
	0x0216,  0x00,
	0x0217,  0x00,
	0x0218,  0x01,
	0x0219,  0x00,
	0x020E,  0x01,
	0x020F,  0x00,
	0x3118,  0x00,
	0x3119,  0x00,
	0x311A,  0x01,
	0x311B,  0x00,
	/*PDAF Setting*/
	0x4018,  0x04,
	0x4019,  0x7C,
	0x401A,  0x00,
	0x401B,  0x01,
	/*HG MODE Setting*/
	0x30D0,  0x00,
	0x30D1,  0x00,
	0x30D2,  0x00,
	/*PDAF TYPE Setting*/
	0x3400,  0x02,
	/*PDAF TYPE1 Setting*/
	0x3091,  0x00,
	/*PDAF TYPE2 Setting*/
	0x3092,  0x01,
#else
	/*MIPI output setting*/
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x02,
	/*Line Length PCK Setting*/
	0x0342, 0x27,
	0x0343, 0xE0,
	/*Frame Length Lines Setting*/
	0x0340, 0x0A,
	0x0341, 0xE6,
	/*ROI Setting*/
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x03,
	0x0347, 0x70,
	0x0348, 0x24,
	0x0349, 0x1F,
	0x034A, 0x17,
	0x034B, 0xB7,
	/*Mode Setting*/
	0x0220, 0x62,
	0x0221, 0x11,
	0x0222, 0x01,
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x08,
	0x30D8, 0x04,
	0x3200, 0x41,
	0x3201, 0x41,
	0x350C, 0x00,
	/*Digital Crop & Scaling*/
	0x0408, 0x00,
	0x0409, 0x08,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x12,
	0x040D, 0x00,
	0x040E, 0x0A,
	0x040F, 0x20,
	/*Output Size Setting*/
	0x034C, 0x12,
	0x034D, 0x00,
	0x034E, 0x0A,
	0x034F, 0x20,
	/*Clock Setting*/
	0x0301, 0x08,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x1D,
	0x030B, 0x02,
	0x030D, 0x04,
	0x030E, 0x02,
	0x030F, 0x00,
	0x0310, 0x01,
	/*Other Setting*/
	0x30D9, 0x00,
	0x32D5, 0x00,
	0x32D6, 0x00,
	0x403D, 0x05,
	0x403E, 0x00,
	0x403F, 0x78,
	0x40A0, 0x00,
	0x40A1, 0x00,
	0x40A4, 0x00,
	0x40A5, 0x00,
	0x40A6, 0x00,
	0x40A7, 0x00,
	0x40B8, 0x01,
	0x40B9, 0x54,
	0x40BC, 0x01,
	0x40BD, 0x6D,
	0x40BE, 0x00,
	0x40BF, 0x32,
	0xAF06, 0x07,
	0xAF07, 0xF1,
	/*Integration Setting*/
	0x0202, 0x0A,
	0x0203, 0x96,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x3116, 0x01,
	0x3117, 0xF4,
	/*Gain Setting*/
	0x0204, 0x00,
	0x0205, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x0218, 0x01,
	0x0219, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x3118, 0x00,
	0x3119, 0x00,
	0x311A, 0x01,
	0x311B, 0x00,
	/*PDAF TYPE2 Setting*/
	0x3400, 0x02,
	0x3092, 0x01,
	0x4324, 0x02,
	0x4325, 0x40,
#endif
};

#if imx686_flag
static kal_uint16 imx686_slim_video_setting[] = {
#ifdef new_init_setting
	/*MIPI output setting*/
	0x0112,  0x0A,
	0x0113,  0x0A,
	0x0114,  0x02,
	/*Line Length PCK Setting*/
	0x0342,  0x27,
	0x0343,  0xE0,
	/*Frame Length Lines Setting*/
	0x0340,  0x0A,
	0x0341,  0x82,
	/*ROI Setting*/
	0x0344,  0x00,
	0x0345,  0x00,
	0x0346,  0x03,
	0x0347,  0x70,
	0x0348,  0x24,
	0x0349,  0x1F,
	0x034A,  0x17,
	0x034B,  0xAF,
	/*Mode Setting*/
	0x0220,  0x62,
	0x0221,  0x11,
	0x0222,  0x01,
	0x0900,  0x01,
	0x0901,  0x22,
	0x0902,  0x08,
	0x30D8,  0x04,
	0x3200,  0x41,
	0x3201,  0x41,
	0x350C,  0x00,
	0x350D,  0x00,
	/*Digital Crop & Scaling*/
	0x0408,  0x00,
	0x0409,  0x08,
	0x040A,  0x00,
	0x040B,  0x00,
	0x040C,  0x12,
	0x040D,  0x00,
	0x040E,  0x0A,
	0x040F,  0x20,
	/*Output Size Setting*/
	0x034C,  0x12,
	0x034D,  0x00,
	0x034E,  0x0A,
	0x034F,  0x20,
	/*Clock Setting*/
	0x0301,  0x08,
	0x0303,  0x04,
	0x0305,  0x03,
	0x0306,  0x00,
	0x0307,  0xCE,
	0x030B,  0x04,
	0x030D,  0x03,
	0x030E,  0x01,
	0x030F,  0x6C,
	0x0310,  0x01,
	/*Other Setting*/
	0x30D9,  0x00,
	0x32D5,  0x00,
	0x32D6,  0x00,
	0x403D,  0x05,
	0x403E,  0x00,
	0x403F,  0x78,
	0x40BC,  0x01,
	0x40BD,  0x6D,
	0x40BE,  0x00,
	0x40BF,  0x00,
	0x40B8,  0x01,
	0x40B9,  0x54,
	0x40A4,  0x00,
	0x40A5,  0x00,
	0x40A0,  0x00,
	0x40A1,  0x00,
	0x40A6,  0x00,
	0x40A7,  0x00,
	0x40AA,  0x00,
	0x40AB,  0x00,
	0x4000,  0xE0,
	0x4001,  0xE2,
	0x4002,  0x01,
	0x4003,  0x00,
	0x4004,  0xFF,
	0x4005,  0x00,
	0x4006,  0x00,
	0x4007,  0x00,
	0x401E,  0x00,
	0x401F,  0xCC,
	0x59EE,  0x00,
	0x59D1,  0x01,
	0xAF06,  0x07,
	0xAF07,  0xF1,
	/*Integration Setting*/
	0x0202,  0x0A,
	0x0203,  0x42,
	0x0224,  0x01,
	0x0225,  0xF4,
	0x3116,  0x01,
	0x3117,  0xF4,
	/*Gain Setting*/
	0x0204,  0x00,
	0x0205,  0x00,
	0x0216,  0x00,
	0x0217,  0x00,
	0x0218,  0x01,
	0x0219,  0x00,
	0x020E,  0x01,
	0x020F,  0x00,
	0x3118,  0x00,
	0x3119,  0x00,
	0x311A,  0x01,
	0x311B,  0x00,
	/*PDAF Setting*/
	0x4018,  0x04,
	0x4019,  0x7C,
	0x401A,  0x00,
	0x401B,  0x01,
	/*HG MODE Setting*/
	0x30D0,  0x00,
	0x30D1,  0x00,
	0x30D2,  0x00,
	/*PDAF TYPE Setting*/
	0x3400,  0x02,
	/*PDAF TYPE1 Setting*/
	0x3091,  0x00,
	/*PDAF TYPE2 Setting*/
	0x3092,  0x01,
#else
	/*MIPI output setting*/
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x02,
	/*Line Length PCK Setting*/
	0x0342, 0x37,
	0x0343, 0x40,
	/*Frame Length Lines Setting*/
	0x0340, 0x0C,
	0x0341, 0x34,
	/*ROI Setting*/
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x01,
	0x0347, 0xE0,
	0x0348, 0x24,
	0x0349, 0x1F,
	0x034A, 0x19,
	0x034B, 0x47,
	/*Mode Setting*/
	0x0220, 0x62,
	0x0221, 0x11,
	0x0222, 0x01,
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x08,
	0x30D8, 0x04,
	0x3200, 0x41,
	0x3201, 0x41,
	0x350C, 0x00,
	/*Digital Crop & Scaling*/
	0x0408, 0x00,
	0x0409, 0x08,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x12,
	0x040D, 0x00,
	0x040E, 0x0B,
	0x040F, 0xB0,
	/*Output Size Setting*/
	0x034C, 0x12,
	0x034D, 0x00,
	0x034E, 0x0B,
	0x034F, 0xB0,
	/*Clock Setting*/
	0x0301, 0x08,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x00,
	0x0307, 0xDD,
	0x030B, 0x04,
	0x030D, 0x04,
	0x030E, 0x02,
	0x030F, 0x2F,
	0x0310, 0x01,
	/*Other Setting*/
	0x30D9, 0x00,
	0x32D5, 0x00,
	0x32D6, 0x00,
	0x403D, 0x05,
	0x403E, 0x00,
	0x403F, 0x78,
	0x40A0, 0x00,
	0x40A1, 0x00,
	0x40A4, 0x00,
	0x40A5, 0x00,
	0x40A6, 0x00,
	0x40A7, 0x00,
	0x40B8, 0x01,
	0x40B9, 0x54,
	0x40BC, 0x01,
	0x40BD, 0x6D,
	0x40BE, 0x00,
	0x40BF, 0x32,
	0xAF06, 0x07,
	0xAF07, 0xF1,
	/*Integration Setting*/
	0x0202, 0x0B,
	0x0203, 0xE4,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x3116, 0x01,
	0x3117, 0xF4,
	/*Gain Setting*/
	0x0204, 0x00,
	0x0205, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x0218, 0x01,
	0x0219, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x3118, 0x00,
	0x3119, 0x00,
	0x311A, 0x01,
	0x311B, 0x00,
	/*PDAF TYPE2 Setting*/
	0x3400, 0x02,
	0x3092, 0x01,
	0x4324, 0x02,
	0x4325, 0x40,
#endif
};
#endif

static kal_uint16 imx686_hs_video_setting[] = {
#ifdef new_init_setting
	/*MIPI output setting*/
	0x0112,  0x0A,
	0x0113,  0x0A,
	0x0114,  0x02,
	/*Line Length PCK Setting*/
	0x0342,  0x15,
	0x0343,  0x70,
	/*Frame Length Lines Setting*/
	0x0340,  0x05,
	0x0341,  0xDC,
	/*ROI Setting*/
	0x0344,  0x00,
	0x0345,  0x00,
	0x0346,  0x03,
	0x0347,  0x70,
	0x0348,  0x24,
	0x0349,  0x1F,
	0x034A,  0x17,
	0x034B,  0xAF,
	/*Mode Setting*/
	0x0220,  0x62,
	0x0221,  0x11,
	0x0222,  0x01,
	0x0900,  0x01,
	0x0901,  0x44,
	0x0902,  0x08,
	0x30D8,  0x00,
	0x3200,  0x43,
	0x3201,  0x43,
	0x350C,  0x00,
	0x350D,  0x00,
	/*Digital Crop & Scaling*/
	0x0408,  0x00,
	0x0409,  0x04,
	0x040A,  0x00,
	0x040B,  0x00,
	0x040C,  0x09,
	0x040D,  0x00,
	0x040E,  0x05,
	0x040F,  0x10,
	/*Output Size Setting*/
	0x034C,  0x09,
	0x034D,  0x00,
	0x034E,  0x05,
	0x034F,  0x10,
	/*Clock Setting*/
	0x0301,  0x08,
	0x0303,  0x04,
	0x0305,  0x03,
	0x0306,  0x00,
	0x0307,  0xF7,
	0x030B,  0x04,
	0x030D,  0x03,
	0x030E,  0x01,
	0x030F,  0x89,
	0x0310,  0x01,
	/*Other Setting*/
	0x30D9,  0x01,
	0x32D5,  0x00,
	0x32D6,  0x00,
	0x403D,  0x06,
	0x403E,  0x00,
	0x403F,  0x5A,
	0x40BC,  0x01,
	0x40BD,  0x0E,
	0x40BE,  0x01,
	0x40BF,  0x0E,
	0x40B8,  0x01,
	0x40B9,  0x54,
	0x40A4,  0x01,
	0x40A5,  0x2C,
	0x40A0,  0x00,
	0x40A1,  0xC8,
	0x40A6,  0x00,
	0x40A7,  0x00,
	0x40AA,  0x00,
	0x40AB,  0x00,
	0x4000,  0xE0,
	0x4001,  0xE2,
	0x4002,  0x01,
	0x4003,  0x00,
	0x4004,  0xFF,
	0x4005,  0x00,
	0x4006,  0x00,
	0x4007,  0x00,
	0x401E,  0x00,
	0x401F,  0xCC,
	0x59EE,  0x00,
	0x59D1,  0x01,
	0xAF06,  0xFF,
	0xAF07,  0xFF,
	/*Integration Setting*/
	0x0202,  0x05,
	0x0203,  0x9C,
	0x0224,  0x01,
	0x0225,  0xF4,
	0x3116,  0x01,
	0x3117,  0xF4,
	/*Gain Setting*/
	0x0204,  0x00,
	0x0205,  0x00,
	0x0216,  0x00,
	0x0217,  0x00,
	0x0218,  0x01,
	0x0219,  0x00,
	0x020E,  0x01,
	0x020F,  0x00,
	0x3118,  0x00,
	0x3119,  0x00,
	0x311A,  0x01,
	0x311B,  0x00,
	/*PDAF Setting*/
	0x4018,  0x00,
	0x4019,  0x00,
	0x401A,  0x00,
	0x401B,  0x00,
	/*HG MODE Setting*/
	0x30D0,  0x00,
	0x30D1,  0x00,
	0x30D2,  0x00,
	/*PDAF TYPE Setting*/
	0x3400,  0x01,
	/*PDAF TYPE1 Setting*/
	0x3091,  0x00,
	/*PDAF TYPE2 Setting*/
	0x3092,  0x00,
#else
	/*MIPI output setting*/
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x02,
	/*Line Length PCK Setting*/
	0x0342, 0x15,
	0x0343, 0x70,
	/*Frame Length Lines Setting*/
	0x0340, 0x05,
	0x0341, 0xF6,
	/*ROI Setting*/
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x03,
	0x0347, 0x70,
	0x0348, 0x24,
	0x0349, 0x1F,
	0x034A, 0x17,
	0x034B, 0xAF,
	/*Mode Setting*/
	0x0220, 0x62,
	0x0221, 0x11,
	0x0222, 0x01,
	0x0900, 0x01,
	0x0901, 0x44,
	0x0902, 0x08,
	0x30D8, 0x00,
	0x3200, 0x43,
	0x3201, 0x43,
	0x350C, 0x00,
	/*Digital Crop & Scaling*/
	0x0408, 0x00,
	0x0409, 0x04,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x09,
	0x040D, 0x00,
	0x040E, 0x05,
	0x040F, 0x10,
	/*Output Size Setting*/
	0x034C, 0x09,
	0x034D, 0x00,
	0x034E, 0x05,
	0x034F, 0x10,
	/*Clock Setting*/
	0x0301, 0x08,
	0x0303, 0x04,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x4F,
	0x030B, 0x04,
	0x030D, 0x04,
	0x030E, 0x02,
	0x030F, 0x0D,
	0x0310, 0x01,
	/*Other Setting*/
	0x30D9, 0x01,
	0x32D5, 0x00,
	0x32D6, 0x00,
	0x403D, 0x06,
	0x403E, 0x00,
	0x403F, 0x5A,
	0x40A0, 0x00,
	0x40A1, 0xC8,
	0x40A4, 0x01,
	0x40A5, 0x2C,
	0x40A6, 0x00,
	0x40A7, 0x00,
	0x40B8, 0x01,
	0x40B9, 0x54,
	0x40BC, 0x01,
	0x40BD, 0x0E,
	0x40BE, 0x01,
	0x40BF, 0x0E,
	0xAF06, 0xFF,
	0xAF07, 0xFF,
	/*Integration Setting*/
	0x0202, 0x05,
	0x0203, 0xA6,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x3116, 0x01,
	0x3117, 0xF4,
	/*Gain Setting*/
	0x0204, 0x00,
	0x0205, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x0218, 0x01,
	0x0219, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x3118, 0x00,
	0x3119, 0x00,
	0x311A, 0x01,
	0x311B, 0x00,
	/*PDAF TYPE2 Setting*/
	0x3400, 0x01,
	0x3092, 0x00,
	0x4324, 0x00,
	0x4325, 0x00,
#endif
};

static kal_uint16 imx686_normal_video_setting[] = {
#ifdef new_init_setting
	/*MIPI output setting*/
	0x0112,  0x0A,
	0x0113,  0x0A,
	0x0114,  0x02,
	/*Line Length PCK Setting*/
	0x0342,  0x27,
	0x0343,  0xE0,
	/*Frame Length Lines Setting*/
	0x0340,  0x0A,
	0x0341,  0x82,
	/*ROI Setting*/
	0x0344,  0x00,
	0x0345,  0x00,
	0x0346,  0x03,
	0x0347,  0x70,
	0x0348,  0x24,
	0x0349,  0x1F,
	0x034A,  0x17,
	0x034B,  0xAF,
	/*Mode Setting*/
	0x0220,  0x62,
	0x0221,  0x11,
	0x0222,  0x01,
	0x0900,  0x01,
	0x0901,  0x22,
	0x0902,  0x08,
	0x30D8,  0x04,
	0x3200,  0x41,
	0x3201,  0x41,
	0x350C,  0x00,
	0x350D,  0x00,
	/*Digital Crop & Scaling*/
	0x0408,  0x00,
	0x0409,  0x08,
	0x040A,  0x00,
	0x040B,  0x00,
	0x040C,  0x12,
	0x040D,  0x00,
	0x040E,  0x0A,
	0x040F,  0x20,
	/*Output Size Setting*/
	0x034C,  0x12,
	0x034D,  0x00,
	0x034E,  0x0A,
	0x034F,  0x20,
	/*Clock Setting*/
	0x0301,  0x08,
	0x0303,  0x04,
	0x0305,  0x03,
	0x0306,  0x00,
	0x0307,  0xCE,
	0x030B,  0x04,
	0x030D,  0x03,
	0x030E,  0x01,
	0x030F,  0x6C,
	0x0310,  0x01,
	/*Other Setting*/
	0x30D9,  0x00,
	0x32D5,  0x00,
	0x32D6,  0x00,
	0x403D,  0x05,
	0x403E,  0x00,
	0x403F,  0x78,
	0x40BC,  0x01,
	0x40BD,  0x6D,
	0x40BE,  0x00,
	0x40BF,  0x00,
	0x40B8,  0x01,
	0x40B9,  0x54,
	0x40A4,  0x00,
	0x40A5,  0x00,
	0x40A0,  0x00,
	0x40A1,  0x00,
	0x40A6,  0x00,
	0x40A7,  0x00,
	0x40AA,  0x00,
	0x40AB,  0x00,
	0x4000,  0xE0,
	0x4001,  0xE2,
	0x4002,  0x01,
	0x4003,  0x00,
	0x4004,  0xFF,
	0x4005,  0x00,
	0x4006,  0x00,
	0x4007,  0x00,
	0x401E,  0x00,
	0x401F,  0xCC,
	0x59EE,  0x00,
	0x59D1,  0x01,
	0xAF06,  0x07,
	0xAF07,  0xF1,
	/*Integration Setting*/
	0x0202,  0x0A,
	0x0203,  0x42,
	0x0224,  0x01,
	0x0225,  0xF4,
	0x3116,  0x01,
	0x3117,  0xF4,
	/*Gain Setting*/
	0x0204,  0x00,
	0x0205,  0x00,
	0x0216,  0x00,
	0x0217,  0x00,
	0x0218,  0x01,
	0x0219,  0x00,
	0x020E,  0x01,
	0x020F,  0x00,
	0x3118,  0x00,
	0x3119,  0x00,
	0x311A,  0x01,
	0x311B,  0x00,
	/*PDAF Setting*/
	0x4018,  0x04,
	0x4019,  0x7C,
	0x401A,  0x00,
	0x401B,  0x01,
	/*HG MODE Setting*/
	0x30D0,  0x00,
	0x30D1,  0x00,
	0x30D2,  0x00,
	/*PDAF TYPE Setting*/
	0x3400,  0x02,
	/*PDAF TYPE1 Setting*/
	0x3091,  0x00,
	/*PDAF TYPE2 Setting*/
	0x3092,  0x01,
#else
	/*MIPI output setting*/
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x02,
	/*Line Length PCK Setting*/
	0x0342, 0x37,
	0x0343, 0x40,
	/*Frame Length Lines Setting*/
	0x0340, 0x0A,
	0x0341, 0x9A,
	/*ROI Setting*/
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x03,
	0x0347, 0x70,
	0x0348, 0x24,
	0x0349, 0x1F,
	0x034A, 0x17,
	0x034B, 0xB7,
	/*Mode Setting*/
	0x0220, 0x62,
	0x0221, 0x11,
	0x0222, 0x01,
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x08,
	0x30D8, 0x04,
	0x3200, 0x41,
	0x3201, 0x41,
	0x350C, 0x00,
	/*Digital Crop & Scaling*/
	0x0408, 0x00,
	0x0409, 0x08,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x12,
	0x040D, 0x00,
	0x040E, 0x0A,
	0x040F, 0x20,
	/*Output Size Setting*/
	0x034C, 0x12,
	0x034D, 0x00,
	0x034E, 0x0A,
	0x034F, 0x20,
	/*Clock Setting*/
	0x0301, 0x08,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x00,
	0x0307, 0xC0,
	0x030B, 0x04,
	0x030D, 0x04,
	0x030E, 0x01,
	0x030F, 0xE6,
	0x0310, 0x01,
	/*Other Setting*/
	0x30D9, 0x00,
	0x32D5, 0x00,
	0x32D6, 0x00,
	0x403D, 0x05,
	0x403E, 0x00,
	0x403F, 0x78,
	0x40A0, 0x00,
	0x40A1, 0x00,
	0x40A4, 0x00,
	0x40A5, 0x00,
	0x40A6, 0x00,
	0x40A7, 0x00,
	0x40B8, 0x01,
	0x40B9, 0x54,
	0x40BC, 0x01,
	0x40BD, 0x6D,
	0x40BE, 0x00,
	0x40BF, 0x32,
	0xAF06, 0x07,
	0xAF07, 0xF1,
	/*Integration Setting*/
	0x0202, 0x0A,
	0x0203, 0x4A,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x3116, 0x01,
	0x3117, 0xF4,
	/*Gain Setting*/
	0x0204, 0x00,
	0x0205, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x0218, 0x01,
	0x0219, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x3118, 0x00,
	0x3119, 0x00,
	0x311A, 0x01,
	0x311B, 0x00,
	/*PDAF TYPE2 Setting*/
	0x3400, 0x02,
	0x3092, 0x01,
	0x4324, 0x02,
	0x4325, 0x40,
#endif
};

static kal_uint16 imx686_custom3_setting[] = {
#ifdef new_init_setting
	/*MIPI output setting*/
	0x0112,  0x0A,
	0x0113,  0x0A,
	0x0114,  0x02,
	/*Line Length PCK Setting*/
	0x0342,  0x15,
	0x0343,  0x70,
	/*Frame Length Lines Setting*/
	0x0340,  0x09,
	0x0341,  0xF6,
	/*ROI Setting*/
	0x0344,  0x00,
	0x0345,  0x00,
	0x0346,  0x07,
	0x0347,  0xF0,
	0x0348,  0x24,
	0x0349,  0x1F,
	0x034A,  0x13,
	0x034B,  0x2F,
	/*Mode Setting*/
	0x0220,  0x62,
	0x0221,  0x11,
	0x0222,  0x01,
	0x0900,  0x01,
	0x0901,  0x44,
	0x0902,  0x08,
	0x30D8,  0x00,
	0x3200,  0x43,
	0x3201,  0x43,
	0x350C,  0x00,
	0x350D,  0x00,
	/*Digital Crop & Scaling*/
	0x0408,  0x02,
	0x0409,  0x04,
	0x040A,  0x00,
	0x040B,  0x00,
	0x040C,  0x05,
	0x040D,  0x00,
	0x040E,  0x02,
	0x040F,  0xD0,
	/*Output Size Setting*/
	0x034C,  0x05,
	0x034D,  0x00,
	0x034E,  0x02,
	0x034F,  0xD0,
	/*Clock Setting*/
	0x0301,  0x08,
	0x0303,  0x02,
	0x0305,  0x03,
	0x0306,  0x00,
	0x0307,  0xD2,
	0x030B,  0x02,
	0x030D,  0x03,
	0x030E,  0x01,
	0x030F,  0x9F,
	0x0310,  0x01,
	/*Other Setting*/
	0x30D9,  0x01,
	0x32D5,  0x00,
	0x32D6,  0x00,
	0x403D,  0x06,
	0x403E,  0x00,
	0x403F,  0x5A,
	0x40BC,  0x01,
	0x40BD,  0x0E,
	0x40BE,  0x01,
	0x40BF,  0x0E,
	0x40B8,  0x01,
	0x40B9,  0x54,
	0x40A4,  0x01,
	0x40A5,  0x2C,
	0x40A0,  0x00,
	0x40A1,  0xC8,
	0x40A6,  0x00,
	0x40A7,  0x00,
	0x40AA,  0x00,
	0x40AB,  0x00,
	0x4000,  0xE0,
	0x4001,  0xE2,
	0x4002,  0x01,
	0x4003,  0x00,
	0x4004,  0xFF,
	0x4005,  0x00,
	0x4006,  0x00,
	0x4007,  0x00,
	0x401E,  0x00,
	0x401F,  0xCC,
	0x59EE,  0x00,
	0x59D1,  0x01,
	0xAF06,  0xFF,
	0xAF07,  0xFF,
	/*Integration Setting*/
	0x0202,  0x09,
	0x0203,  0xB6,
	0x0224,  0x01,
	0x0225,  0xF4,
	0x3116,  0x01,
	0x3117,  0xF4,
	/*Gain Setting*/
	0x0204,  0x00,
	0x0205,  0x00,
	0x0216,  0x00,
	0x0217,  0x00,
	0x0218,  0x01,
	0x0219,  0x00,
	0x020E,  0x01,
	0x020F,  0x00,
	0x3118,  0x00,
	0x3119,  0x00,
	0x311A,  0x01,
	0x311B,  0x00,
	/*PDAF Setting*/
	0x4018,  0x00,
	0x4019,  0x00,
	0x401A,  0x00,
	0x401B,  0x00,
	/*HG MODE Setting*/
	0x30D0,  0x00,
	0x30D1,  0x00,
	0x30D2,  0x00,
	/*PDAF TYPE Setting*/
	0x3400,  0x01,
	/*PDAF TYPE1 Setting*/
	0x3091,  0x00,
	/*PDAF TYPE2 Setting*/
	0x3092,  0x00,
#else
	/*MIPI output setting*/
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x02,
	/*Line Length PCK Setting*/
	0x0342, 0x3B,
	0x0343, 0x40,
	/*Frame Length Lines Setting*/
	0x0340, 0x1B,
	0x0341, 0x57,
	/*ROI Setting*/
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x10,
	0x0348, 0x24,
	0x0349, 0x1F,
	0x034A, 0x1B,
	0x034B, 0x0F,
	/*Mode Setting*/
	0x0220, 0x62,
	0x0221, 0x11,
	0x0222, 0x01,
	0x0900, 0x00,
	0x0901, 0x11,
	0x0902, 0x0A,
	0x30D8, 0x00,
	0x3200, 0x01,
	0x3201, 0x01,
	0x350C, 0x00,
	/*Digital Crop & Scaling*/
	0x0408, 0x00,
	0x0409, 0x10,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x24,
	0x040D, 0x00,
	0x040E, 0x1B,
	0x040F, 0x00,
	/*Output Size Setting*/
	0x034C, 0x24,
	0x034D, 0x00,
	0x034E, 0x1B,
	0x034F, 0x00,
	/*Clock Setting*/
	0x0301, 0x08,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x6E,
	0x030B, 0x02,
	0x030D, 0x04,
	0x030E, 0x03,
	0x030F, 0x13,
	0x0310, 0x01,
	/*Other Setting*/
	0x30D9, 0x01,
	0x32D5, 0x01,
	0x32D6, 0x01,
	0x403D, 0x10,
	0x403E, 0x00,
	0x403F, 0x78,
	0x40A0, 0x03,
	0x40A1, 0xD4,
	0x40A4, 0x00,
	0x40A5, 0x00,
	0x40A6, 0x00,
	0x40A7, 0x00,
	0x40B8, 0x01,
	0x40B9, 0xC2,
	0x40BC, 0x01,
	0x40BD, 0x40,
	0x40BE, 0x01,
	0x40BF, 0x40,
	0xAF06, 0x03,
	0xAF07, 0xFB,
	/*Integration Setting*/
	0x0202, 0x1B,
	0x0203, 0x07,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x3116, 0x01,
	0x3117, 0xF4,
	/*Gain Setting*/
	0x0204, 0x00,
	0x0205, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x0218, 0x01,
	0x0219, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x3118, 0x00,
	0x3119, 0x00,
	0x311A, 0x01,
	0x311B, 0x00,
	/*PDAF TYPE2 Setting*/
	0x3400, 0x04,
	0x3092, 0x7C,
	0x4324, 0x00,
	0x4325, 0x01,
#endif
};

static kal_uint16 imx686_custom4_setting[] = {
#ifdef new_init_setting
	/*MIPI output setting*/
	0x0112,  0x0A,
	0x0113,  0x0A,
	0x0114,  0x02,
	/*Line Length PCK Setting*/
	0x0342,  0x15,
	0x0343,  0x70,
	/*Frame Length Lines Setting*/
	0x0340,  0x05,
	0x0341,  0xFA,
	/*ROI Setting*/
	0x0344,  0x00,
	0x0345,  0x00,
	0x0346,  0x03,
	0x0347,  0x70,
	0x0348,  0x24,
	0x0349,  0x1F,
	0x034A,  0x17,
	0x034B,  0xAF,
	/*Mode Setting*/
	0x0220,  0x62,
	0x0221,  0x11,
	0x0222,  0x01,
	0x0900,  0x01,
	0x0901,  0x44,
	0x0902,  0x08,
	0x30D8,  0x00,
	0x3200,  0x43,
	0x3201,  0x43,
	0x350C,  0x00,
	0x350D,  0x00,
	/*Digital Crop & Scaling*/
	0x0408,  0x00,
	0x0409,  0x04,
	0x040A,  0x00,
	0x040B,  0x00,
	0x040C,  0x09,
	0x040D,  0x00,
	0x040E,  0x05,
	0x040F,  0x10,
	/*Output Size Setting*/
	0x034C,  0x09,
	0x034D,  0x00,
	0x034E,  0x05,
	0x034F,  0x10,
	/*Clock Setting*/
	0x0301,  0x08,
	0x0303,  0x02,
	0x0305,  0x03,
	0x0306,  0x00,
	0x0307,  0xFC,
	0x030B,  0x02,
	0x030D,  0x03,
	0x030E,  0x01,
	0x030F,  0x9A,
	0x0310,  0x01,
	/*Other Setting*/
	0x30D9,  0x01,
	0x32D5,  0x00,
	0x32D6,  0x00,
	0x403D,  0x06,
	0x403E,  0x00,
	0x403F,  0x5A,
	0x40BC,  0x01,
	0x40BD,  0x0E,
	0x40BE,  0x01,
	0x40BF,  0x0E,
	0x40B8,  0x01,
	0x40B9,  0x54,
	0x40A4,  0x01,
	0x40A5,  0x2C,
	0x40A0,  0x00,
	0x40A1,  0xC8,
	0x40A6,  0x00,
	0x40A7,  0x00,
	0x40AA,  0x00,
	0x40AB,  0x00,
	0x4000,  0xE0,
	0x4001,  0xE2,
	0x4002,  0x01,
	0x4003,  0x00,
	0x4004,  0xFF,
	0x4005,  0x00,
	0x4006,  0x00,
	0x4007,  0x00,
	0x401E,  0x00,
	0x401F,  0xCC,
	0x59EE,  0x00,
	0x59D1,  0x01,
	0xAF06,  0xFF,
	0xAF07,  0xFF,
	/*Integration Setting*/
	0x0202,  0x05,
	0x0203,  0xBA,
	0x0224,  0x01,
	0x0225,  0xF4,
	0x3116,  0x01,
	0x3117,  0xF4,
	/*Gain Setting*/
	0x0204,  0x00,
	0x0205,  0x00,
	0x0216,  0x00,
	0x0217,  0x00,
	0x0218,  0x01,
	0x0219,  0x00,
	0x020E,  0x01,
	0x020F,  0x00,
	0x3118,  0x00,
	0x3119,  0x00,
	0x311A,  0x01,
	0x311B,  0x00,
	/*PDAF Setting*/
	0x4018,  0x00,
	0x4019,  0x00,
	0x401A,  0x00,
	0x401B,  0x00,
	/*HG MODE Setting*/
	0x30D0,  0x00,
	0x30D1,  0x00,
	0x30D2,  0x00,
	/*PDAF TYPE Setting*/
	0x3400,  0x01,
	/*PDAF TYPE1 Setting*/
	0x3091,  0x00,
	/*PDAF TYPE2 Setting*/
	0x3092,  0x00,
#else
	/*MIPI output setting*/
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x02,
	/*Line Length PCK Setting*/
	0x0342, 0x15,
	0x0343, 0x70,
	/*Frame Length Lines Setting*/
	0x0340, 0x05,
	0x0341, 0xF6,
	/*ROI Setting*/
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x03,
	0x0347, 0x70,
	0x0348, 0x24,
	0x0349, 0x1F,
	0x034A, 0x17,
	0x034B, 0xAF,
	/*Mode Setting*/
	0x0220, 0x62,
	0x0221, 0x11,
	0x0222, 0x01,
	0x0900, 0x01,
	0x0901, 0x44,
	0x0902, 0x08,
	0x30D8, 0x00,
	0x3200, 0x43,
	0x3201, 0x43,
	0x350C, 0x00,
	/*Digital Crop & Scaling*/
	0x0408, 0x00,
	0x0409, 0x04,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x09,
	0x040D, 0x00,
	0x040E, 0x05,
	0x040F, 0x10,
	/*Output Size Setting*/
	0x034C, 0x09,
	0x034D, 0x00,
	0x034E, 0x05,
	0x034F, 0x10,
	/*Clock Setting*/
	0x0301, 0x08,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x4F,
	0x030B, 0x02,
	0x030D, 0x04,
	0x030E, 0x02,
	0x030F, 0x19,
	0x0310, 0x01,
	/*Other Setting*/
	0x30D9, 0x01,
	0x32D5, 0x00,
	0x32D6, 0x00,
	0x403D, 0x06,
	0x403E, 0x00,
	0x403F, 0x5A,
	0x40A0, 0x00,
	0x40A1, 0xC8,
	0x40A4, 0x01,
	0x40A5, 0x2C,
	0x40A6, 0x00,
	0x40A7, 0x00,
	0x40B8, 0x01,
	0x40B9, 0x54,
	0x40BC, 0x01,
	0x40BD, 0x0E,
	0x40BE, 0x01,
	0x40BF, 0x0E,
	0xAF06, 0xFF,
	0xAF07, 0xFF,
	/*Integration Setting*/
	0x0202, 0x05,
	0x0203, 0xA6,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x3116, 0x01,
	0x3117, 0xF4,
	/*Gain Setting*/
	0x0204, 0x00,
	0x0205, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x0218, 0x01,
	0x0219, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x3118, 0x00,
	0x3119, 0x00,
	0x311A, 0x01,
	0x311B, 0x00,
	/*PDAF TYPE2 Setting*/
	0x3400, 0x01,
	0x3092, 0x00,
	0x4324, 0x00,
	0x4325, 0x00,
#endif
};

static kal_uint16 imx686_custom1_setting[] = {
#ifdef new_init_setting
	/*MIPI output setting*/
	0x0112,  0x0A,
	0x0113,  0x0A,
	0x0114,  0x02,
	/*Line Length PCK Setting*/
	0x0342,  0x27,
	0x0343,  0xE0,
	/*Frame Length Lines Setting*/
	0x0340,  0x0D,
	0x0341,  0xC6,
	/*ROI Setting*/
	0x0344,  0x00,
	0x0345,  0x00,
	0x0346,  0x00,
	0x0347,  0x10,
	0x0348,  0x24,
	0x0349,  0x1F,
	0x034A,  0x1B,
	0x034B,  0x0F,
	/*Mode Setting*/
	0x0220,  0x62,
	0x0221,  0x11,
	0x0222,  0x01,
	0x0900,  0x01,
	0x0901,  0x22,
	0x0902,  0x08,
	0x30D8,  0x04,
	0x3200,  0x41,
	0x3201,  0x41,
	0x350C,  0x00,
	0x350D,  0x00,
	/*Digital Crop & Scaling*/
	0x0408,  0x00,
	0x0409,  0x08,
	0x040A,  0x00,
	0x040B,  0x00,
	0x040C,  0x12,
	0x040D,  0x00,
	0x040E,  0x0D,
	0x040F,  0x80,
	/*Output Size Setting*/
	0x034C,  0x12,
	0x034D,  0x00,
	0x034E,  0x0D,
	0x034F,  0x80,
	/*Clock Setting*/
	0x0301,  0x08,
	0x0303,  0x04,
	0x0305,  0x03,
	0x0306,  0x00,
	0x0307,  0xD8,
	0x030B,  0x04,
	0x030D,  0x03,
	0x030E,  0x01,
	0x030F,  0x7F,
	0x0310,  0x01,
	/*Other Setting*/
	0x30D9,  0x00,
	0x32D5,  0x00,
	0x32D6,  0x00,
	0x403D,  0x05,
	0x403E,  0x00,
	0x403F,  0x78,
	0x40BC,  0x01,
	0x40BD,  0x6D,
	0x40BE,  0x00,
	0x40BF,  0x00,
	0x40B8,  0x01,
	0x40B9,  0x54,
	0x40A4,  0x00,
	0x40A5,  0x00,
	0x40A0,  0x00,
	0x40A1,  0x00,
	0x40A6,  0x00,
	0x40A7,  0x00,
	0x40AA,  0x00,
	0x40AB,  0x00,
	0x4000,  0xE0,
	0x4001,  0xE2,
	0x4002,  0x01,
	0x4003,  0x00,
	0x4004,  0xFF,
	0x4005,  0x00,
	0x4006,  0x00,
	0x4007,  0x00,
	0x401E,  0x00,
	0x401F,  0xCC,
	0x59EE,  0x00,
	0x59D1,  0x01,
	0xAF06,  0x07,
	0xAF07,  0xF1,
	/*Integration Setting*/
	0x0202,  0x0D,
	0x0203,  0x86,
	0x0224,  0x01,
	0x0225,  0xF4,
	0x3116,  0x01,
	0x3117,  0xF4,
	/*Gain Setting*/
	0x0204,  0x00,
	0x0205,  0x00,
	0x0216,  0x00,
	0x0217,  0x00,
	0x0218,  0x01,
	0x0219,  0x00,
	0x020E,  0x01,
	0x020F,  0x00,
	0x3118,  0x00,
	0x3119,  0x00,
	0x311A,  0x01,
	0x311B,  0x00,
	/*PDAF Setting*/
	0x4018,  0x04,
	0x4019,  0x7C,
	0x401A,  0x00,
	0x401B,  0x01,
	/*HG MODE Setting*/
	0x30D0,  0x00,
	0x30D1,  0x00,
	0x30D2,  0x00,
	/*PDAF TYPE Setting*/
	0x3400,  0x02,
	/*PDAF TYPE1 Setting*/
	0x3091,  0x00,
	/*PDAF TYPE2 Setting*/
	0x3092,  0x01,
#else
	/*MIPI output setting*/
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x02,
	/*Line Length PCK Setting*/
	0x0342, 0x37,
	0x0343, 0x40,
	/*Frame Length Lines Setting*/
	0x0340, 0x0D,
	0x0341, 0xF2,
	/*ROI Setting*/
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x10,
	0x0348, 0x24,
	0x0349, 0x1F,
	0x034A, 0x1B,
	0x034B, 0x17,
	/*Mode Setting*/
	0x0220, 0x62,
	0x0221, 0x11,
	0x0222, 0x01,
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x08,
	0x30D8, 0x04,
	0x3200, 0x41,
	0x3201, 0x41,
	0x350C, 0x00,
	/*Digital Crop & Scaling*/
	0x0408, 0x00,
	0x0409, 0x08,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x12,
	0x040D, 0x00,
	0x040E, 0x0D,
	0x040F, 0x80,
	/*Output Size Setting*/
	0x034C, 0x12,
	0x034D, 0x00,
	0x034E, 0x0D,
	0x034F, 0x80,
	/*Clock Setting*/
	0x0301, 0x08,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x00,
	0x0307, 0xCA,
	0x030B, 0x04,
	0x030D, 0x04,
	0x030E, 0x01,
	0x030F, 0xFF,
	0x0310, 0x01,
	/*Other Setting*/
	0x30D9, 0x00,
	0x32D5, 0x00,
	0x32D6, 0x00,
	0x403D, 0x05,
	0x403E, 0x00,
	0x403F, 0x78,
	0x40A0, 0x00,
	0x40A1, 0x00,
	0x40A4, 0x00,
	0x40A5, 0x00,
	0x40A6, 0x00,
	0x40A7, 0x00,
	0x40B8, 0x01,
	0x40B9, 0x54,
	0x40BC, 0x01,
	0x40BD, 0x6D,
	0x40BE, 0x00,
	0x40BF, 0x32,
	0xAF06, 0x07,
	0xAF07, 0xF1,
	/*Integration Setting*/
	0x0202, 0x0D,
	0x0203, 0xA2,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x3116, 0x01,
	0x3117, 0xF4,
	/*Gain Setting*/
	0x0204, 0x00,
	0x0205, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x0218, 0x01,
	0x0219, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x3118, 0x00,
	0x3119, 0x00,
	0x311A, 0x01,
	0x311B, 0x00,
	/*PDAF TYPE2 Setting*/
	0x3400, 0x02,
	0x3092, 0x01,
	0x4324, 0x02,
	0x4325, 0x40,
#endif
};

static kal_uint16 imx686_custom5_setting[] = {
#ifdef new_init_setting
	/*MIPI output setting*/
	0x0112,  0x0A,
	0x0113,  0x0A,
	0x0114,  0x02,
	/*Line Length PCK Setting*/
	0x0342,  0x27,
	0x0343,  0xE0,
	/*Frame Length Lines Setting*/
	0x0340,  0x1B,
	0x0341,  0xF4,
	/*ROI Setting*/
	0x0344,  0x00,
	0x0345,  0x00,
	0x0346,  0x00,
	0x0347,  0x10,
	0x0348,  0x24,
	0x0349,  0x1F,
	0x034A,  0x1B,
	0x034B,  0x0F,
	/*Mode Setting*/
	0x0220,  0x62,
	0x0221,  0x11,
	0x0222,  0x01,
	0x0900,  0x01,
	0x0901,  0x22,
	0x0902,  0x08,
	0x30D8,  0x04,
	0x3200,  0x41,
	0x3201,  0x41,
	0x350C,  0x00,
	0x350D,  0x00,
	/*Digital Crop & Scaling*/
	0x0408,  0x00,
	0x0409,  0x08,
	0x040A,  0x00,
	0x040B,  0x00,
	0x040C,  0x12,
	0x040D,  0x00,
	0x040E,  0x0D,
	0x040F,  0x80,
	/*Output Size Setting*/
	0x034C,  0x12,
	0x034D,  0x00,
	0x034E,  0x0D,
	0x034F,  0x80,
	/*Clock Setting*/
	0x0301,  0x08,
	0x0303,  0x02,
	0x0305,  0x03,
	0x0306,  0x01,
	0x0307,  0x12,
	0x030B,  0x01,
	0x030D,  0x03,
	0x030E,  0x01,
	0x030F,  0xB4,
	0x0310,  0x01,
	/*Other Setting*/
	0x30D9,  0x00,
	0x32D5,  0x00,
	0x32D6,  0x00,
	0x403D,  0x05,
	0x403E,  0x00,
	0x403F,  0x78,
	0x40BC,  0x01,
	0x40BD,  0x6D,
	0x40BE,  0x00,
	0x40BF,  0x00,
	0x40B8,  0x01,
	0x40B9,  0x54,
	0x40A4,  0x00,
	0x40A5,  0x00,
	0x40A0,  0x00,
	0x40A1,  0x00,
	0x40A6,  0x00,
	0x40A7,  0x00,
	0x40AA,  0x00,
	0x40AB,  0x00,
	0x4000,  0xE0,
	0x4001,  0xE2,
	0x4002,  0x01,
	0x4003,  0x00,
	0x4004,  0xFF,
	0x4005,  0x00,
	0x4006,  0x00,
	0x4007,  0x00,
	0x401E,  0x00,
	0x401F,  0xCC,
	0x59EE,  0x00,
	0x59D1,  0x01,
	0xAF06,  0x07,
	0xAF07,  0xF1,
	/*Integration Setting*/
	0x0202,  0x1B,
	0x0203,  0xB4,
	0x0224,  0x01,
	0x0225,  0xF4,
	0x3116,  0x01,
	0x3117,  0xF4,
	/*Gain Setting*/
	0x0204,  0x00,
	0x0205,  0x00,
	0x0216,  0x00,
	0x0217,  0x00,
	0x0218,  0x01,
	0x0219,  0x00,
	0x020E,  0x01,
	0x020F,  0x00,
	0x3118,  0x00,
	0x3119,  0x00,
	0x311A,  0x01,
	0x311B,  0x00,
	/*PDAF Setting*/
	0x4018,  0x04,
	0x4019,  0x7C,
	0x401A,  0x00,
	0x401B,  0x01,
	/*HG MODE Setting*/
	0x30D0,  0x00,
	0x30D1,  0x00,
	0x30D2,  0x00,
	/*PDAF TYPE Setting*/
	0x3400,  0x02,
	/*PDAF TYPE1 Setting*/
	0x3091,  0x00,
	/*PDAF TYPE2 Setting*/
	0x3092,  0x01,
#else
	/*MIPI output setting*/
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x02,
	/*Line Length PCK Setting*/
	0x0342, 0x27,
	0x0343, 0xE0,
	/*Frame Length Lines Setting*/
	0x0340, 0x0C,
	0x0341, 0x8C,
	/*ROI Setting*/
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x01,
	0x0347, 0xE0,
	0x0348, 0x24,
	0x0349, 0x1F,
	0x034A, 0x19,
	0x034B, 0x47,
	/*Mode Setting*/
	0x0220, 0x62,
	0x0221, 0x11,
	0x0222, 0x01,
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x08,
	0x30D8, 0x04,
	0x3200, 0x41,
	0x3201, 0x41,
	0x350C, 0x00,
	/*Digital Crop & Scaling*/
	0x0408, 0x00,
	0x0409, 0x08,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x12,
	0x040D, 0x00,
	0x040E, 0x0B,
	0x040F, 0xB0,
	/*Output Size Setting*/
	0x034C, 0x12,
	0x034D, 0x00,
	0x034E, 0x0B,
	0x034F, 0xB0,
	/*Clock Setting*/
	0x0301, 0x08,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x48,
	0x030B, 0x02,
	0x030D, 0x04,
	0x030E, 0x02,
	0x030F, 0x56,
	0x0310, 0x01,
	/*Other Setting*/
	0x30D9, 0x00,
	0x32D5, 0x00,
	0x32D6, 0x00,
	0x403D, 0x05,
	0x403E, 0x00,
	0x403F, 0x78,
	0x40A0, 0x00,
	0x40A1, 0x00,
	0x40A4, 0x00,
	0x40A5, 0x00,
	0x40A6, 0x00,
	0x40A7, 0x00,
	0x40B8, 0x01,
	0x40B9, 0x54,
	0x40BC, 0x01,
	0x40BD, 0x6D,
	0x40BE, 0x00,
	0x40BF, 0x32,
	0xAF06, 0x07,
	0xAF07, 0xF1,
	/*Integration Setting*/
	0x0202, 0x0C,
	0x0203, 0x3C,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x3116, 0x01,
	0x3117, 0xF4,
	/*Gain Setting*/
	0x0204, 0x00,
	0x0205, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x0218, 0x01,
	0x0219, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x3118, 0x00,
	0x3119, 0x00,
	0x311A, 0x01,
	0x311B, 0x00,
	/*PDAF TYPE2 Setting*/
	0x3400, 0x02,
	0x3092, 0x01,
	0x4324, 0x02,
	0x4325, 0x40,
#endif
};

static kal_uint16 imx686_custom6_setting[] = {
#ifdef new_init_setting
	/*MIPI output setting*/
	0x0112,  0x0A,
	0x0113,  0x0A,
	0x0114,  0x02,
	/*Line Length PCK Setting*/
	0x0342,  0x28,
	0x0343,  0x60,
	/*Frame Length Lines Setting*/
	0x0340,  0x1B,
	0x0341,  0x9D,
	/*ROI Setting*/
	0x0344,  0x00,
	0x0345,  0x00,
	0x0346,  0x00,
	0x0347,  0x10,
	0x0348,  0x24,
	0x0349,  0x1F,
	0x034A,  0x1B,
	0x034B,  0x0F,
	/*Mode Setting*/
	0x0220,  0x62,
	0x0221,  0x11,
	0x0222,  0x01,
	0x0900,  0x00,
	0x0901,  0x11,
	0x0902,  0x0A,
	0x30D8,  0x00,
	0x3200,  0x01,
	0x3201,  0x01,
	0x350C,  0x00,
	0x350D,  0x00,
	/*Digital Crop & Scaling*/
	0x0408,  0x00,
	0x0409,  0x10,
	0x040A,  0x00,
	0x040B,  0x00,
	0x040C,  0x24,
	0x040D,  0x00,
	0x040E,  0x1B,
	0x040F,  0x00,
	/*Output Size Setting*/
	0x034C,  0x24,
	0x034D,  0x00,
	0x034E,  0x1B,
	0x034F,  0x00,
	/*Clock Setting*/
	0x0301,  0x08,
	0x0303,  0x02,
	0x0305,  0x03,
	0x0306,  0x01,
	0x0307,  0x12,
	0x030B,  0x01,
	0x030D,  0x03,
	0x030E,  0x01,
	0x030F,  0xB4,
	0x0310,  0x01,
	/*Other Setting*/
	0x30D9,  0x01,
	0x32D5,  0x01,//enable remosaic
	0x32D6,  0x00,//disable QSC
	0x403D,  0x10,
	0x403E,  0x00,
	0x403F,  0x78,
	0x40BC,  0x00,
	0x40BD,  0xA0,
	0x40BE,  0x00,
	0x40BF,  0xA0,
	0x40B8,  0x01,
	0x40B9,  0xF4,
	0x40A4,  0x02,
	0x40A5,  0xA8,
	0x40A0,  0x02,
	0x40A1,  0xA8,
	0x40A6,  0x00,
	0x40A7,  0x00,
	0x40AA,  0x00,
	0x40AB,  0x00,
	0x4000,  0xE0,
	0x4001,  0xE2,
	0x4002,  0x01,
	0x4003,  0x00,
	0x4004,  0xFF,
	0x4005,  0x00,
	0x4006,  0x00,
	0x4007,  0x00,
	0x401E,  0x00,
	0x401F,  0xCC,
	0x59EE,  0x00,
	0x59D1,  0x01,
	0xAF06,  0x03,
	0xAF07,  0xFB,
	/*Integration Setting*/
	0x0202,  0x1B,
	0x0203,  0x5D,
	0x0224,  0x01,
	0x0225,  0xF4,
	0x3116,  0x01,
	0x3117,  0xF4,
	/*Gain Setting*/
	0x0204,  0x00,
	0x0205,  0x00,
	0x0216,  0x00,
	0x0217,  0x00,
	0x0218,  0x01,
	0x0219,  0x00,
	0x020E,  0x01,
	0x020F,  0x00,
	0x3118,  0x00,
	0x3119,  0x00,
	0x311A,  0x01,
	0x311B,  0x00,
	/*PDAF Setting*/
	0x4018,  0x04,
	0x4019,  0x7C,
	0x401A,  0x00,
	0x401B,  0x01,
	/*HG MODE Setting*/
	0x30D0,  0x00,
	0x30D1,  0x00,
	0x30D2,  0x00,
	/*PDAF TYPE Setting*/
	0x3400,  0x02,
	/*PDAF TYPE1 Setting*/
	0x3091,  0x00,
	/*PDAF TYPE2 Setting*/
	0x3092,  0x01,
#else
	/*MIPI output setting*/
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x02,
	/*Line Length PCK Setting*/
	0x0342, 0x27,
	0x0343, 0xE0,
	/*Frame Length Lines Setting*/
	0x0340, 0x0C,
	0x0341, 0x8C,
	/*ROI Setting*/
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x01,
	0x0347, 0xE0,
	0x0348, 0x24,
	0x0349, 0x1F,
	0x034A, 0x19,
	0x034B, 0x47,
	/*Mode Setting*/
	0x0220, 0x62,
	0x0221, 0x11,
	0x0222, 0x01,
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x08,
	0x30D8, 0x04,
	0x3200, 0x41,
	0x3201, 0x41,
	0x350C, 0x00,
	/*Digital Crop & Scaling*/
	0x0408, 0x00,
	0x0409, 0x08,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x12,
	0x040D, 0x00,
	0x040E, 0x0B,
	0x040F, 0xB0,
	/*Output Size Setting*/
	0x034C, 0x12,
	0x034D, 0x00,
	0x034E, 0x0B,
	0x034F, 0xB0,
	/*Clock Setting*/
	0x0301, 0x08,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x48,
	0x030B, 0x02,
	0x030D, 0x04,
	0x030E, 0x02,
	0x030F, 0x56,
	0x0310, 0x01,
	/*Other Setting*/
	0x30D9, 0x00,
	0x32D5, 0x00,
	0x32D6, 0x00,
	0x403D, 0x05,
	0x403E, 0x00,
	0x403F, 0x78,
	0x40A0, 0x00,
	0x40A1, 0x00,
	0x40A4, 0x00,
	0x40A5, 0x00,
	0x40A6, 0x00,
	0x40A7, 0x00,
	0x40B8, 0x01,
	0x40B9, 0x54,
	0x40BC, 0x01,
	0x40BD, 0x6D,
	0x40BE, 0x00,
	0x40BF, 0x32,
	0xAF06, 0x07,
	0xAF07, 0xF1,
	/*Integration Setting*/
	0x0202, 0x0C,
	0x0203, 0x3C,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x3116, 0x01,
	0x3117, 0xF4,
	/*Gain Setting*/
	0x0204, 0x00,
	0x0205, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x0218, 0x01,
	0x0219, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x3118, 0x00,
	0x3119, 0x00,
	0x311A, 0x01,
	0x311B, 0x00,
	/*PDAF TYPE2 Setting*/
	0x3400, 0x02,
	0x3092, 0x01,
	0x4324, 0x02,
	0x4325, 0x40,
#endif
};
#endif

static void sensor_init(void)
{
	pr_debug("[%s] imx686_sensor_init_start\n", __func__);
	#if USE_BURST_MODE
	imx686_table_write_cmos_sensor(imx686_init_setting,
		sizeof(imx686_init_setting)/sizeof(kal_uint16));
	#else
	/*External Clock Setting*/
	write_cmos_sensor_8(0x0136, 0x18);
	write_cmos_sensor_8(0x0137, 0x00);
	/*Register version*/
	write_cmos_sensor_8(0x3C7E, 0x04);
	write_cmos_sensor_8(0x3C7F, 0x08);
	/*Signaling mode setting*/
	write_cmos_sensor_8(0x0111, 0x03);
	/*Global Setting*/
	write_cmos_sensor_8(0x380C, 0x00);
	write_cmos_sensor_8(0x3C00, 0x10);
	write_cmos_sensor_8(0x3C01, 0x10);
	write_cmos_sensor_8(0x3C02, 0x10);
	write_cmos_sensor_8(0x3C03, 0x10);
	write_cmos_sensor_8(0x3C04, 0x10);
	write_cmos_sensor_8(0x3C05, 0x01);
	write_cmos_sensor_8(0x3C06, 0x00);
	write_cmos_sensor_8(0x3C07, 0x00);
	write_cmos_sensor_8(0x3C08, 0x03);
	write_cmos_sensor_8(0x3C09, 0xFF);
	write_cmos_sensor_8(0x3C0A, 0x01);
	write_cmos_sensor_8(0x3C0B, 0x00);
	write_cmos_sensor_8(0x3C0C, 0x00);
	write_cmos_sensor_8(0x3C0D, 0x03);
	write_cmos_sensor_8(0x3C0E, 0xFF);
	write_cmos_sensor_8(0x3C0F, 0x20);
	write_cmos_sensor_8(0x3F88, 0x00);
	write_cmos_sensor_8(0x3F8E, 0x00);
	write_cmos_sensor_8(0x5282, 0x01);
	write_cmos_sensor_8(0x9004, 0x14);
	write_cmos_sensor_8(0x9200, 0xF4);
	write_cmos_sensor_8(0x9201, 0xA7);
	write_cmos_sensor_8(0x9202, 0xF4);
	write_cmos_sensor_8(0x9203, 0xAA);
	write_cmos_sensor_8(0x9204, 0xF4);
	write_cmos_sensor_8(0x9205, 0xAD);
	write_cmos_sensor_8(0x9206, 0xF4);
	write_cmos_sensor_8(0x9207, 0xB0);
	write_cmos_sensor_8(0x9208, 0xF4);
	write_cmos_sensor_8(0x9209, 0xB3);
	write_cmos_sensor_8(0x920A, 0xB7);
	write_cmos_sensor_8(0x920B, 0x34);
	write_cmos_sensor_8(0x920C, 0xB7);
	write_cmos_sensor_8(0x920D, 0x36);
	write_cmos_sensor_8(0x920E, 0xB7);
	write_cmos_sensor_8(0x920F, 0x37);
	write_cmos_sensor_8(0x9210, 0xB7);
	write_cmos_sensor_8(0x9211, 0x38);
	write_cmos_sensor_8(0x9212, 0xB7);
	write_cmos_sensor_8(0x9213, 0x39);
	write_cmos_sensor_8(0x9214, 0xB7);
	write_cmos_sensor_8(0x9215, 0x3A);
	write_cmos_sensor_8(0x9216, 0xB7);
	write_cmos_sensor_8(0x9217, 0x3C);
	write_cmos_sensor_8(0x9218, 0xB7);
	write_cmos_sensor_8(0x9219, 0x3D);
	write_cmos_sensor_8(0x921A, 0xB7);
	write_cmos_sensor_8(0x921B, 0x3E);
	write_cmos_sensor_8(0x921C, 0xB7);
	write_cmos_sensor_8(0x921D, 0x3F);
	write_cmos_sensor_8(0x921E, 0x77);
	write_cmos_sensor_8(0x921F, 0x77);
	write_cmos_sensor_8(0x9222, 0xC4);
	write_cmos_sensor_8(0x9223, 0x4B);
	write_cmos_sensor_8(0x9224, 0xC4);
	write_cmos_sensor_8(0x9225, 0x4C);
	write_cmos_sensor_8(0x9226, 0xC4);
	write_cmos_sensor_8(0x9227, 0x4D);
	write_cmos_sensor_8(0x9810, 0x14);
	write_cmos_sensor_8(0x9814, 0x14);
	write_cmos_sensor_8(0x99B2, 0x20);
	write_cmos_sensor_8(0x99B3, 0x0F);
	write_cmos_sensor_8(0x99B4, 0x0F);
	write_cmos_sensor_8(0x99B5, 0x0F);
	write_cmos_sensor_8(0x99B6, 0x0F);
	write_cmos_sensor_8(0x99E4, 0x0F);
	write_cmos_sensor_8(0x99E5, 0x0F);
	write_cmos_sensor_8(0x99E6, 0x0F);
	write_cmos_sensor_8(0x99E7, 0x0F);
	write_cmos_sensor_8(0x99E8, 0x0F);
	write_cmos_sensor_8(0x99E9, 0x0F);
	write_cmos_sensor_8(0x99EA, 0x0F);
	write_cmos_sensor_8(0x99EB, 0x0F);
	write_cmos_sensor_8(0x99EC, 0x0F);
	write_cmos_sensor_8(0x99ED, 0x0F);
	write_cmos_sensor_8(0xA569, 0x06);
	write_cmos_sensor_8(0xA679, 0x20);
	write_cmos_sensor_8(0xC020, 0x01);
	write_cmos_sensor_8(0xC61D, 0x00);
	write_cmos_sensor_8(0xC625, 0x00);
	write_cmos_sensor_8(0xC638, 0x03);
	write_cmos_sensor_8(0xC63B, 0x01);
	write_cmos_sensor_8(0xE286, 0x31);
	write_cmos_sensor_8(0xE2A6, 0x32);
	write_cmos_sensor_8(0xE2C6, 0x33);
	#endif
	/*enable temperature sensor, TEMP_SEN_CTL:*/
	write_cmos_sensor_8(0x0138, 0x01);

	set_mirror_flip(imgsensor.mirror);
	pr_debug("[%s] imx686_sensor_init_End\n", __func__);
}	/*	  sensor_init  */


static void preview_setting(void)
{
	pr_debug("%s +\n", __func__);

	#if USE_BURST_MODE
	//Feiping.Li, 60fps
	imx686_table_write_cmos_sensor(imx686_preview_setting,
		sizeof(imx686_preview_setting)/sizeof(kal_uint16));
	#else
	/*MIPI output setting*/
	write_cmos_sensor_8(0x0112, 0x0A);
	write_cmos_sensor_8(0x0113, 0x0A);
	write_cmos_sensor_8(0x0114, 0x02);
	/*Line Length PCK Setting*/
	write_cmos_sensor_8(0x0342, 0x1E);
	write_cmos_sensor_8(0x0343, 0xC0);
	/*Frame Length Lines Setting*/
	write_cmos_sensor_8(0x0340, 0x0B);
	write_cmos_sensor_8(0x0341, 0xFC);
	/*ROI Setting*/
	write_cmos_sensor_8(0x0344, 0x00);
	write_cmos_sensor_8(0x0345, 0x00);
	write_cmos_sensor_8(0x0346, 0x00);
	write_cmos_sensor_8(0x0347, 0x00);
	write_cmos_sensor_8(0x0348, 0x1F);
	write_cmos_sensor_8(0x0349, 0x3F);
	write_cmos_sensor_8(0x034A, 0x17);
	write_cmos_sensor_8(0x034B, 0x6F);
	/*Mode Setting*/
	write_cmos_sensor_8(0x0220, 0x62);
	write_cmos_sensor_8(0x0222, 0x01);
	write_cmos_sensor_8(0x0900, 0x01);
	write_cmos_sensor_8(0x0901, 0x22);
	write_cmos_sensor_8(0x0902, 0x08);
	write_cmos_sensor_8(0x3140, 0x00);
	write_cmos_sensor_8(0x3246, 0x81);
	write_cmos_sensor_8(0x3247, 0x81);
	write_cmos_sensor_8(0x3F15, 0x00);
	/*Digital Crop & Scaling*/
	write_cmos_sensor_8(0x0401, 0x00);
	write_cmos_sensor_8(0x0404, 0x00);
	write_cmos_sensor_8(0x0405, 0x10);
	write_cmos_sensor_8(0x0408, 0x00);
	write_cmos_sensor_8(0x0409, 0x00);
	write_cmos_sensor_8(0x040A, 0x00);
	write_cmos_sensor_8(0x040B, 0x00);
	write_cmos_sensor_8(0x040C, 0x0F);
	write_cmos_sensor_8(0x040D, 0xA0);
	write_cmos_sensor_8(0x040E, 0x0B);
	write_cmos_sensor_8(0x040F, 0xB8);
	/*Output Size Setting*/
	write_cmos_sensor_8(0x034C, 0x0F);
	write_cmos_sensor_8(0x034D, 0xA0);
	write_cmos_sensor_8(0x034E, 0x0B);
	write_cmos_sensor_8(0x034F, 0xB8);
	/*Clock Setting*/
	write_cmos_sensor_8(0x0301, 0x05);
	write_cmos_sensor_8(0x0303, 0x02);
	write_cmos_sensor_8(0x0305, 0x04);
	write_cmos_sensor_8(0x0306, 0x01);
	write_cmos_sensor_8(0x0307, 0x2E);
	write_cmos_sensor_8(0x030B, 0x01);
	write_cmos_sensor_8(0x030D, 0x0C);
	write_cmos_sensor_8(0x030E, 0x02);
	write_cmos_sensor_8(0x030F, 0xC6);
	write_cmos_sensor_8(0x0310, 0x01);
	/*Other Setting*/
	write_cmos_sensor_8(0x3620, 0x00);
	write_cmos_sensor_8(0x3621, 0x00);
	write_cmos_sensor_8(0x3C11, 0x04);
	write_cmos_sensor_8(0x3C12, 0x03);
	write_cmos_sensor_8(0x3C13, 0x2D);
	write_cmos_sensor_8(0x3F0C, 0x01);
	write_cmos_sensor_8(0x3F14, 0x00);
	write_cmos_sensor_8(0x3F80, 0x01);
	write_cmos_sensor_8(0x3F81, 0x90);
	write_cmos_sensor_8(0x3F8C, 0x00);
	write_cmos_sensor_8(0x3F8D, 0x14);
	write_cmos_sensor_8(0x3FF8, 0x01);
	write_cmos_sensor_8(0x3FF9, 0x2A);
	write_cmos_sensor_8(0x3FFE, 0x00);
	write_cmos_sensor_8(0x3FFF, 0x6C);
	/*Integration Setting*/
	write_cmos_sensor_8(0x0202, 0x0B);
	write_cmos_sensor_8(0x0203, 0xCC);
	write_cmos_sensor_8(0x0224, 0x01);
	write_cmos_sensor_8(0x0225, 0xF4);
	write_cmos_sensor_8(0x3116, 0x01);
	write_cmos_sensor_8(0x3117, 0xF4);
	/*Gain Setting*/
	write_cmos_sensor_8(0x0204, 0x00);
	write_cmos_sensor_8(0x0205, 0x70);
	write_cmos_sensor_8(0x0216, 0x00);
	write_cmos_sensor_8(0x0217, 0x70);
	write_cmos_sensor_8(0x0218, 0x01);
	write_cmos_sensor_8(0x0219, 0x00);
	write_cmos_sensor_8(0x020E, 0x01);
	write_cmos_sensor_8(0x020F, 0x00);
	write_cmos_sensor_8(0x0210, 0x01);
	write_cmos_sensor_8(0x0211, 0x00);
	write_cmos_sensor_8(0x0212, 0x01);
	write_cmos_sensor_8(0x0213, 0x00);
	write_cmos_sensor_8(0x0214, 0x01);
	write_cmos_sensor_8(0x0215, 0x00);
	write_cmos_sensor_8(0x3FE2, 0x00);
	write_cmos_sensor_8(0x3FE3, 0x70);
	write_cmos_sensor_8(0x3FE4, 0x01);
	write_cmos_sensor_8(0x3FE5, 0x00);
	/*PDAF TYPE1 Setting*/
	write_cmos_sensor_8(0x3E20, 0x02);
	write_cmos_sensor_8(0x3E37, 0x00);
	/*PDAF TYPE2 Setting*/
	write_cmos_sensor_8(0x3E20, 0x02);
	write_cmos_sensor_8(0x3E3B, 0x01);
	write_cmos_sensor_8(0x4434, 0x01);
	write_cmos_sensor_8(0x4435, 0xF0);
	#endif
	pr_debug("%s -\n", __func__);
} /* preview_setting */

/*full size 30fps*/
static void capture_setting(kal_uint16 currefps)
{
	pr_debug("%s 30 fps E! currefps:%d\n", __func__, currefps);
	/*************MIPI output setting************/
	#if USE_BURST_MODE
	imx686_table_write_cmos_sensor(imx686_capture_setting,
		sizeof(imx686_capture_setting)/sizeof(kal_uint16));
	#else
	/*MIPI output setting*/
	write_cmos_sensor_8(0x0112, 0x0A);
	write_cmos_sensor_8(0x0113, 0x0A);
	write_cmos_sensor_8(0x0114, 0x02);
	/*Line Length PCK Setting*/
	write_cmos_sensor_8(0x0342, 0x1E);
	write_cmos_sensor_8(0x0343, 0xC0);
	/*Frame Length Lines Setting*/
	write_cmos_sensor_8(0x0340, 0x0B);
	write_cmos_sensor_8(0x0341, 0xFC);
	/*ROI Setting*/
	write_cmos_sensor_8(0x0344, 0x00);
	write_cmos_sensor_8(0x0345, 0x00);
	write_cmos_sensor_8(0x0346, 0x00);
	write_cmos_sensor_8(0x0347, 0x00);
	write_cmos_sensor_8(0x0348, 0x1F);
	write_cmos_sensor_8(0x0349, 0x3F);
	write_cmos_sensor_8(0x034A, 0x17);
	write_cmos_sensor_8(0x034B, 0x6F);
	/*Mode Setting*/
	write_cmos_sensor_8(0x0220, 0x62);
	write_cmos_sensor_8(0x0222, 0x01);
	write_cmos_sensor_8(0x0900, 0x01);
	write_cmos_sensor_8(0x0901, 0x22);
	write_cmos_sensor_8(0x0902, 0x08);
	write_cmos_sensor_8(0x3140, 0x00);
	write_cmos_sensor_8(0x3246, 0x81);
	write_cmos_sensor_8(0x3247, 0x81);
	write_cmos_sensor_8(0x3F15, 0x00);
	/*Digital Crop & Scaling*/
	write_cmos_sensor_8(0x0401, 0x00);
	write_cmos_sensor_8(0x0404, 0x00);
	write_cmos_sensor_8(0x0405, 0x10);
	write_cmos_sensor_8(0x0408, 0x00);
	write_cmos_sensor_8(0x0409, 0x00);
	write_cmos_sensor_8(0x040A, 0x00);
	write_cmos_sensor_8(0x040B, 0x00);
	write_cmos_sensor_8(0x040C, 0x0F);
	write_cmos_sensor_8(0x040D, 0xA0);
	write_cmos_sensor_8(0x040E, 0x0B);
	write_cmos_sensor_8(0x040F, 0xB8);
	/*Output Size Setting*/
	write_cmos_sensor_8(0x034C, 0x0F);
	write_cmos_sensor_8(0x034D, 0xA0);
	write_cmos_sensor_8(0x034E, 0x0B);
	write_cmos_sensor_8(0x034F, 0xB8);
	/*Clock Setting*/
	write_cmos_sensor_8(0x0301, 0x05);
	write_cmos_sensor_8(0x0303, 0x04);
	write_cmos_sensor_8(0x0305, 0x04);
	write_cmos_sensor_8(0x0306, 0x01);
	write_cmos_sensor_8(0x0307, 0x2E);
	write_cmos_sensor_8(0x030B, 0x02);
	write_cmos_sensor_8(0x030D, 0x0C);
	write_cmos_sensor_8(0x030E, 0x02);
	write_cmos_sensor_8(0x030F, 0xBA);
	write_cmos_sensor_8(0x0310, 0x01);
	/*Other Setting*/
	write_cmos_sensor_8(0x3620, 0x00);
	write_cmos_sensor_8(0x3621, 0x00);
	write_cmos_sensor_8(0x3C11, 0x04);
	write_cmos_sensor_8(0x3C12, 0x03);
	write_cmos_sensor_8(0x3C13, 0x2D);
	write_cmos_sensor_8(0x3F0C, 0x01);
	write_cmos_sensor_8(0x3F14, 0x00);
	write_cmos_sensor_8(0x3F80, 0x01);
	write_cmos_sensor_8(0x3F81, 0x90);
	write_cmos_sensor_8(0x3F8C, 0x00);
	write_cmos_sensor_8(0x3F8D, 0x14);
	write_cmos_sensor_8(0x3FF8, 0x01);
	write_cmos_sensor_8(0x3FF9, 0x2A);
	write_cmos_sensor_8(0x3FFE, 0x00);
	write_cmos_sensor_8(0x3FFF, 0x6C);
	/*Integration Setting*/
	write_cmos_sensor_8(0x0202, 0x0B);
	write_cmos_sensor_8(0x0203, 0xCC);
	write_cmos_sensor_8(0x0224, 0x01);
	write_cmos_sensor_8(0x0225, 0xF4);
	write_cmos_sensor_8(0x3116, 0x01);
	write_cmos_sensor_8(0x3117, 0xF4);
	/*Gain Setting*/
	write_cmos_sensor_8(0x0204, 0x00);
	write_cmos_sensor_8(0x0205, 0x70);
	write_cmos_sensor_8(0x0216, 0x00);
	write_cmos_sensor_8(0x0217, 0x70);
	write_cmos_sensor_8(0x0218, 0x01);
	write_cmos_sensor_8(0x0219, 0x00);
	write_cmos_sensor_8(0x020E, 0x01);
	write_cmos_sensor_8(0x020F, 0x00);
	write_cmos_sensor_8(0x0210, 0x01);
	write_cmos_sensor_8(0x0211, 0x00);
	write_cmos_sensor_8(0x0212, 0x01);
	write_cmos_sensor_8(0x0213, 0x00);
	write_cmos_sensor_8(0x0214, 0x01);
	write_cmos_sensor_8(0x0215, 0x00);
	write_cmos_sensor_8(0x3FE2, 0x00);
	write_cmos_sensor_8(0x3FE3, 0x70);
	write_cmos_sensor_8(0x3FE4, 0x01);
	write_cmos_sensor_8(0x3FE5, 0x00);
	/*PDAF TYPE2 Setting*/
	write_cmos_sensor_8(0x3E20, 0x02);
	write_cmos_sensor_8(0x3E3B, 0x01);
	write_cmos_sensor_8(0x4434, 0x01);
	write_cmos_sensor_8(0x4435, 0xF0);
	#endif
	pr_debug("%s 30 fpsX\n", __func__);
}

static void normal_video_setting(kal_uint16 currefps)
{
	pr_debug("%s E! currefps:%d\n", __func__, currefps);

	#if USE_BURST_MODE
	imx686_table_write_cmos_sensor(imx686_normal_video_setting,
		sizeof(imx686_normal_video_setting)/sizeof(kal_uint16));
	#else
	/*MIPI output setting*/
	write_cmos_sensor_8(0x0112, 0x0A);
	write_cmos_sensor_8(0x0113, 0x0A);
	write_cmos_sensor_8(0x0114, 0x02);
	/*Line Length PCK Setting*/
	write_cmos_sensor_8(0x0342, 0x1E);
	write_cmos_sensor_8(0x0343, 0xC0);
	/*Frame Length Lines Setting*/
	write_cmos_sensor_8(0x0340, 0x0E);
	write_cmos_sensor_8(0x0341, 0x9A);
	/*ROI Setting*/
	write_cmos_sensor_8(0x0344, 0x00);
	write_cmos_sensor_8(0x0345, 0x00);
	write_cmos_sensor_8(0x0346, 0x01);
	write_cmos_sensor_8(0x0347, 0x90);
	write_cmos_sensor_8(0x0348, 0x1F);
	write_cmos_sensor_8(0x0349, 0x3F);
	write_cmos_sensor_8(0x034A, 0x15);
	write_cmos_sensor_8(0x034B, 0xDF);
	/*Mode Setting*/
	write_cmos_sensor_8(0x0220, 0x62);
	write_cmos_sensor_8(0x0222, 0x01);
	write_cmos_sensor_8(0x0900, 0x01);
	write_cmos_sensor_8(0x0901, 0x22);
	write_cmos_sensor_8(0x0902, 0x08);
	write_cmos_sensor_8(0x3140, 0x00);
	write_cmos_sensor_8(0x3246, 0x81);
	write_cmos_sensor_8(0x3247, 0x81);
	write_cmos_sensor_8(0x3F15, 0x00);
	/*Digital Crop & Scaling*/
	write_cmos_sensor_8(0x0401, 0x00);
	write_cmos_sensor_8(0x0404, 0x00);
	write_cmos_sensor_8(0x0405, 0x10);
	write_cmos_sensor_8(0x0408, 0x00);
	write_cmos_sensor_8(0x0409, 0x00);
	write_cmos_sensor_8(0x040A, 0x00);
	write_cmos_sensor_8(0x040B, 0x00);
	write_cmos_sensor_8(0x040C, 0x0F);
	write_cmos_sensor_8(0x040D, 0xA0);
	write_cmos_sensor_8(0x040E, 0x0A);
	write_cmos_sensor_8(0x040F, 0x28);
	/*Output Size Setting*/
	write_cmos_sensor_8(0x034C, 0x0F);
	write_cmos_sensor_8(0x034D, 0xA0);
	write_cmos_sensor_8(0x034E, 0x0A);
	write_cmos_sensor_8(0x034F, 0x28);
	/*Clock Setting*/
	write_cmos_sensor_8(0x0301, 0x05);
	write_cmos_sensor_8(0x0303, 0x02);
	write_cmos_sensor_8(0x0305, 0x04);
	write_cmos_sensor_8(0x0306, 0x00);
	write_cmos_sensor_8(0x0307, 0xB8);
	write_cmos_sensor_8(0x030B, 0x02);
	write_cmos_sensor_8(0x030D, 0x04);
	write_cmos_sensor_8(0x030E, 0x01);
	write_cmos_sensor_8(0x030F, 0x1C);
	write_cmos_sensor_8(0x0310, 0x01);
	/*Other Setting*/
	write_cmos_sensor_8(0x3620, 0x00);
	write_cmos_sensor_8(0x3621, 0x00);
	write_cmos_sensor_8(0x3C11, 0x04);
	write_cmos_sensor_8(0x3C12, 0x03);
	write_cmos_sensor_8(0x3C13, 0x2D);
	write_cmos_sensor_8(0x3F0C, 0x01);
	write_cmos_sensor_8(0x3F14, 0x00);
	write_cmos_sensor_8(0x3F80, 0x01);
	write_cmos_sensor_8(0x3F81, 0x90);
	write_cmos_sensor_8(0x3F8C, 0x00);
	write_cmos_sensor_8(0x3F8D, 0x14);
	write_cmos_sensor_8(0x3FF8, 0x01);
	write_cmos_sensor_8(0x3FF9, 0x2A);
	write_cmos_sensor_8(0x3FFE, 0x00);
	write_cmos_sensor_8(0x3FFF, 0x6C);
	/*Integration Setting*/
	write_cmos_sensor_8(0x0202, 0x0E);
	write_cmos_sensor_8(0x0203, 0x6A);
	write_cmos_sensor_8(0x0224, 0x01);
	write_cmos_sensor_8(0x0225, 0xF4);
	write_cmos_sensor_8(0x3116, 0x01);
	write_cmos_sensor_8(0x3117, 0xF4);
	/*Gain Setting*/
	write_cmos_sensor_8(0x0204, 0x00);
	write_cmos_sensor_8(0x0205, 0x70);
	write_cmos_sensor_8(0x0216, 0x00);
	write_cmos_sensor_8(0x0217, 0x70);
	write_cmos_sensor_8(0x0218, 0x01);
	write_cmos_sensor_8(0x0219, 0x00);
	write_cmos_sensor_8(0x020E, 0x01);
	write_cmos_sensor_8(0x020F, 0x00);
	write_cmos_sensor_8(0x0210, 0x01);
	write_cmos_sensor_8(0x0211, 0x00);
	write_cmos_sensor_8(0x0212, 0x01);
	write_cmos_sensor_8(0x0213, 0x00);
	write_cmos_sensor_8(0x0214, 0x01);
	write_cmos_sensor_8(0x0215, 0x00);
	write_cmos_sensor_8(0x3FE2, 0x00);
	write_cmos_sensor_8(0x3FE3, 0x70);
	write_cmos_sensor_8(0x3FE4, 0x01);
	write_cmos_sensor_8(0x3FE5, 0x00);
	/*PDAF TYPE2 Setting*/
	write_cmos_sensor_8(0x3E20, 0x02);
	write_cmos_sensor_8(0x3E3B, 0x01);
	write_cmos_sensor_8(0x4434, 0x01);
	write_cmos_sensor_8(0x4435, 0xF0);
	#endif
	pr_debug("X\n");
}

static void hs_video_setting(void)
{
	pr_debug("%s E! currefps 120\n", __func__);

	#if USE_BURST_MODE
	imx686_table_write_cmos_sensor(imx686_hs_video_setting,
		sizeof(imx686_hs_video_setting)/sizeof(kal_uint16));
	#else
	/*MIPI output setting*/
	write_cmos_sensor_8(0x0112, 0x0A);
	write_cmos_sensor_8(0x0113, 0x0A);
	write_cmos_sensor_8(0x0114, 0x02);
	/*Line Length PCK Setting*/
	write_cmos_sensor_8(0x0342, 0x15);
	write_cmos_sensor_8(0x0343, 0x00);
	/*Frame Length Lines Setting*/
	write_cmos_sensor_8(0x0340, 0x04);
	write_cmos_sensor_8(0x0341, 0xA6);
	/*ROI Setting*/
	write_cmos_sensor_8(0x0344, 0x00);
	write_cmos_sensor_8(0x0345, 0x00);
	write_cmos_sensor_8(0x0346, 0x02);
	write_cmos_sensor_8(0x0347, 0xE8);
	write_cmos_sensor_8(0x0348, 0x1F);
	write_cmos_sensor_8(0x0349, 0x3F);
	write_cmos_sensor_8(0x034A, 0x14);
	write_cmos_sensor_8(0x034B, 0x87);
	/*Mode Setting*/
	write_cmos_sensor_8(0x0220, 0x62);
	write_cmos_sensor_8(0x0222, 0x01);
	write_cmos_sensor_8(0x0900, 0x01);
	write_cmos_sensor_8(0x0901, 0x44);
	write_cmos_sensor_8(0x0902, 0x08);
	write_cmos_sensor_8(0x3140, 0x00);
	write_cmos_sensor_8(0x3246, 0x89);
	write_cmos_sensor_8(0x3247, 0x89);
	write_cmos_sensor_8(0x3F15, 0x00);
	/*Digital Crop & Scaling*/
	write_cmos_sensor_8(0x0401, 0x00);
	write_cmos_sensor_8(0x0404, 0x00);
	write_cmos_sensor_8(0x0405, 0x10);
	write_cmos_sensor_8(0x0408, 0x00);
	write_cmos_sensor_8(0x0409, 0x00);
	write_cmos_sensor_8(0x040A, 0x00);
	write_cmos_sensor_8(0x040B, 0x00);
	write_cmos_sensor_8(0x040C, 0x07);
	write_cmos_sensor_8(0x040D, 0xD0);
	write_cmos_sensor_8(0x040E, 0x04);
	write_cmos_sensor_8(0x040F, 0x68);
	/*Output Size Setting*/
	write_cmos_sensor_8(0x034C, 0x07);
	write_cmos_sensor_8(0x034D, 0xD0);
	write_cmos_sensor_8(0x034E, 0x04);
	write_cmos_sensor_8(0x034F, 0x68);
	/*Clock Setting*/
	write_cmos_sensor_8(0x0301, 0x05);
	write_cmos_sensor_8(0x0303, 0x04);
	write_cmos_sensor_8(0x0305, 0x04);
	write_cmos_sensor_8(0x0306, 0x01);
	write_cmos_sensor_8(0x0307, 0x40);
	write_cmos_sensor_8(0x030B, 0x04);
	write_cmos_sensor_8(0x030D, 0x0C);
	write_cmos_sensor_8(0x030E, 0x03);
	write_cmos_sensor_8(0x030F, 0xF0);
	write_cmos_sensor_8(0x0310, 0x01);
	/*Other Setting*/
	write_cmos_sensor_8(0x3620, 0x00);
	write_cmos_sensor_8(0x3621, 0x00);
	write_cmos_sensor_8(0x3C11, 0x0C);
	write_cmos_sensor_8(0x3C12, 0x05);
	write_cmos_sensor_8(0x3C13, 0x2C);
	write_cmos_sensor_8(0x3F0C, 0x00);
	write_cmos_sensor_8(0x3F14, 0x00);
	write_cmos_sensor_8(0x3F80, 0x02);
	write_cmos_sensor_8(0x3F81, 0x67);
	write_cmos_sensor_8(0x3F8C, 0x02);
	write_cmos_sensor_8(0x3F8D, 0x44);
	write_cmos_sensor_8(0x3FF8, 0x00);
	write_cmos_sensor_8(0x3FF9, 0x00);
	write_cmos_sensor_8(0x3FFE, 0x01);
	write_cmos_sensor_8(0x3FFF, 0x90);
	/*Integration Setting*/
	write_cmos_sensor_8(0x0202, 0x04);
	write_cmos_sensor_8(0x0203, 0x76);
	write_cmos_sensor_8(0x0224, 0x01);
	write_cmos_sensor_8(0x0225, 0xF4);
	write_cmos_sensor_8(0x3116, 0x01);
	write_cmos_sensor_8(0x3117, 0xF4);
	/*Gain Setting*/
	write_cmos_sensor_8(0x0204, 0x00);
	write_cmos_sensor_8(0x0205, 0x70);
	write_cmos_sensor_8(0x0216, 0x00);
	write_cmos_sensor_8(0x0217, 0x70);
	write_cmos_sensor_8(0x0218, 0x01);
	write_cmos_sensor_8(0x0219, 0x00);
	write_cmos_sensor_8(0x020E, 0x01);
	write_cmos_sensor_8(0x020F, 0x00);
	write_cmos_sensor_8(0x0210, 0x01);
	write_cmos_sensor_8(0x0211, 0x00);
	write_cmos_sensor_8(0x0212, 0x01);
	write_cmos_sensor_8(0x0213, 0x00);
	write_cmos_sensor_8(0x0214, 0x01);
	write_cmos_sensor_8(0x0215, 0x00);
	write_cmos_sensor_8(0x3FE2, 0x00);
	write_cmos_sensor_8(0x3FE3, 0x70);
	write_cmos_sensor_8(0x3FE4, 0x01);
	write_cmos_sensor_8(0x3FE5, 0x00);
	/*PDAF TYPE1 Setting*/
	write_cmos_sensor_8(0x3E20, 0x01);
	write_cmos_sensor_8(0x3E37, 0x00);
	/*PDAF TYPE2 Setting*/
	write_cmos_sensor_8(0x3E20, 0x01);
	write_cmos_sensor_8(0x3E3B, 0x00);
	write_cmos_sensor_8(0x4434, 0x00);
	write_cmos_sensor_8(0x4435, 0xF8);
	#endif
	pr_debug("X\n");
}

static void slim_video_setting(void)
{
	pr_debug("%s E! 4000*2256@30fps\n", __func__);

	#if USE_BURST_MODE
	imx686_table_write_cmos_sensor(imx686_normal_video_setting,
		sizeof(imx686_normal_video_setting)/sizeof(kal_uint16));
	#else
	/*MIPI output setting*/
	write_cmos_sensor_8(0x0112, 0x0A);
	write_cmos_sensor_8(0x0113, 0x0A);
	write_cmos_sensor_8(0x0114, 0x02);
	/*Line Length PCK Setting*/
	write_cmos_sensor_8(0x0342, 0x1E);
	write_cmos_sensor_8(0x0343, 0xC0);
	/*Frame Length Lines Setting*/
	write_cmos_sensor_8(0x0340, 0x0E);
	write_cmos_sensor_8(0x0341, 0x9A);
	/*ROI Setting*/
	write_cmos_sensor_8(0x0344, 0x00);
	write_cmos_sensor_8(0x0345, 0x00);
	write_cmos_sensor_8(0x0346, 0x02);
	write_cmos_sensor_8(0x0347, 0xE8);
	write_cmos_sensor_8(0x0348, 0x1F);
	write_cmos_sensor_8(0x0349, 0x3F);
	write_cmos_sensor_8(0x034A, 0x14);
	write_cmos_sensor_8(0x034B, 0x87);
	/*Mode Setting*/
	write_cmos_sensor_8(0x0220, 0x62);
	write_cmos_sensor_8(0x0222, 0x01);
	write_cmos_sensor_8(0x0900, 0x01);
	write_cmos_sensor_8(0x0901, 0x22);
	write_cmos_sensor_8(0x0902, 0x08);
	write_cmos_sensor_8(0x3140, 0x00);
	write_cmos_sensor_8(0x3246, 0x81);
	write_cmos_sensor_8(0x3247, 0x81);
	write_cmos_sensor_8(0x3F15, 0x00);
	/*Digital Crop & Scaling*/
	write_cmos_sensor_8(0x0401, 0x00);
	write_cmos_sensor_8(0x0404, 0x00);
	write_cmos_sensor_8(0x0405, 0x10);
	write_cmos_sensor_8(0x0408, 0x00);
	write_cmos_sensor_8(0x0409, 0x00);
	write_cmos_sensor_8(0x040A, 0x00);
	write_cmos_sensor_8(0x040B, 0x00);
	write_cmos_sensor_8(0x040C, 0x0F);
	write_cmos_sensor_8(0x040D, 0xA0);
	write_cmos_sensor_8(0x040E, 0x08);
	write_cmos_sensor_8(0x040F, 0xD0);
	/*Output Size Setting*/
	write_cmos_sensor_8(0x034C, 0x0F);
	write_cmos_sensor_8(0x034D, 0xA0);
	write_cmos_sensor_8(0x034E, 0x08);
	write_cmos_sensor_8(0x034F, 0xD0);
	/*Clock Setting*/
	write_cmos_sensor_8(0x0301, 0x05);
	write_cmos_sensor_8(0x0303, 0x02);
	write_cmos_sensor_8(0x0305, 0x04);
	write_cmos_sensor_8(0x0306, 0x00);
	write_cmos_sensor_8(0x0307, 0xB8);
	write_cmos_sensor_8(0x030B, 0x02);
	write_cmos_sensor_8(0x030D, 0x04);
	write_cmos_sensor_8(0x030E, 0x01);
	write_cmos_sensor_8(0x030F, 0x1C);
	write_cmos_sensor_8(0x0310, 0x01);
	/*Other Setting*/
	write_cmos_sensor_8(0x3620, 0x00);
	write_cmos_sensor_8(0x3621, 0x00);
	write_cmos_sensor_8(0x3C11, 0x04);
	write_cmos_sensor_8(0x3C12, 0x03);
	write_cmos_sensor_8(0x3C13, 0x2D);
	write_cmos_sensor_8(0x3F0C, 0x01);
	write_cmos_sensor_8(0x3F14, 0x00);
	write_cmos_sensor_8(0x3F80, 0x01);
	write_cmos_sensor_8(0x3F81, 0x90);
	write_cmos_sensor_8(0x3F8C, 0x00);
	write_cmos_sensor_8(0x3F8D, 0x14);
	write_cmos_sensor_8(0x3FF8, 0x01);
	write_cmos_sensor_8(0x3FF9, 0x2A);
	write_cmos_sensor_8(0x3FFE, 0x00);
	write_cmos_sensor_8(0x3FFF, 0x6C);
	/*Integration Setting*/
	write_cmos_sensor_8(0x0202, 0x0E);
	write_cmos_sensor_8(0x0203, 0x6A);
	write_cmos_sensor_8(0x0224, 0x01);
	write_cmos_sensor_8(0x0225, 0xF4);
	write_cmos_sensor_8(0x3116, 0x01);
	write_cmos_sensor_8(0x3117, 0xF4);
	/*Gain Setting*/
	write_cmos_sensor_8(0x0204, 0x00);
	write_cmos_sensor_8(0x0205, 0x70);
	write_cmos_sensor_8(0x0216, 0x00);
	write_cmos_sensor_8(0x0217, 0x70);
	write_cmos_sensor_8(0x0218, 0x01);
	write_cmos_sensor_8(0x0219, 0x00);
	write_cmos_sensor_8(0x020E, 0x01);
	write_cmos_sensor_8(0x020F, 0x00);
	write_cmos_sensor_8(0x0210, 0x01);
	write_cmos_sensor_8(0x0211, 0x00);
	write_cmos_sensor_8(0x0212, 0x01);
	write_cmos_sensor_8(0x0213, 0x00);
	write_cmos_sensor_8(0x0214, 0x01);
	write_cmos_sensor_8(0x0215, 0x00);
	write_cmos_sensor_8(0x3FE2, 0x00);
	write_cmos_sensor_8(0x3FE3, 0x70);
	write_cmos_sensor_8(0x3FE4, 0x01);
	write_cmos_sensor_8(0x3FE5, 0x00);
	/*PDAF TYPE2 Setting*/
	write_cmos_sensor_8(0x3E20, 0x02);
	write_cmos_sensor_8(0x3E3B, 0x01);
	write_cmos_sensor_8(0x4434, 0x01);
	write_cmos_sensor_8(0x4435, 0xF0);
	#endif
	pr_debug("X\n");
}


/*full size 30fps*/
static void custom3_setting(void)
{
	pr_debug("%s 30 fps E!\n", __func__);
	/*************MIPI output setting************/
	#if USE_BURST_MODE
	imx686_table_write_cmos_sensor(imx686_custom3_setting,
		sizeof(imx686_custom3_setting)/sizeof(kal_uint16));
	#else
	/*MIPI output setting*/
	write_cmos_sensor_8(0x0112, 0x0A);
	write_cmos_sensor_8(0x0113, 0x0A);
	write_cmos_sensor_8(0x0114, 0x02);
	/*Line Length PCK Setting*/
	write_cmos_sensor_8(0x0342, 0x24);
	write_cmos_sensor_8(0x0343, 0xE0);
	/*Frame Length Lines Setting*/
	write_cmos_sensor_8(0x0340, 0x17);
	write_cmos_sensor_8(0x0341, 0xB3);
	/*ROI Setting*/
	write_cmos_sensor_8(0x0344, 0x00);
	write_cmos_sensor_8(0x0345, 0x00);
	write_cmos_sensor_8(0x0346, 0x00);
	write_cmos_sensor_8(0x0347, 0x00);
	write_cmos_sensor_8(0x0348, 0x1F);
	write_cmos_sensor_8(0x0349, 0x3F);
	write_cmos_sensor_8(0x034A, 0x17);
	write_cmos_sensor_8(0x034B, 0x6F);
	/*Mode Setting*/
	write_cmos_sensor_8(0x0220, 0x62);
	write_cmos_sensor_8(0x0222, 0x01);
	write_cmos_sensor_8(0x0900, 0x00);
	write_cmos_sensor_8(0x0901, 0x11);
	write_cmos_sensor_8(0x0902, 0x08);
	write_cmos_sensor_8(0x3140, 0x00);
	write_cmos_sensor_8(0x3246, 0x01);
	write_cmos_sensor_8(0x3247, 0x01);
	write_cmos_sensor_8(0x3F15, 0x00);
	/*Digital Crop & Scaling*/
	write_cmos_sensor_8(0x0401, 0x00);
	write_cmos_sensor_8(0x0404, 0x00);
	write_cmos_sensor_8(0x0405, 0x10);
	write_cmos_sensor_8(0x0408, 0x00);
	write_cmos_sensor_8(0x0409, 0x00);
	write_cmos_sensor_8(0x040A, 0x00);
	write_cmos_sensor_8(0x040B, 0x00);
	write_cmos_sensor_8(0x040C, 0x1F);
	write_cmos_sensor_8(0x040D, 0x40);
	write_cmos_sensor_8(0x040E, 0x17);
	write_cmos_sensor_8(0x040F, 0x70);
	/*Output Size Setting*/
	write_cmos_sensor_8(0x034C, 0x1F);
	write_cmos_sensor_8(0x034D, 0x40);
	write_cmos_sensor_8(0x034E, 0x17);
	write_cmos_sensor_8(0x034F, 0x70);
	/*Clock Setting*/
	write_cmos_sensor_8(0x0301, 0x05);
	write_cmos_sensor_8(0x0303, 0x02);
	write_cmos_sensor_8(0x0305, 0x04);
	write_cmos_sensor_8(0x0306, 0x01);
	write_cmos_sensor_8(0x0307, 0x66);
	write_cmos_sensor_8(0x030B, 0x01);
	write_cmos_sensor_8(0x030D, 0x0C);
	write_cmos_sensor_8(0x030E, 0x04);
	write_cmos_sensor_8(0x030F, 0xD2);
	write_cmos_sensor_8(0x0310, 0x01);
	/*Other Setting*/
	write_cmos_sensor_8(0x3620, 0x01);
	write_cmos_sensor_8(0x3621, 0x01);
	write_cmos_sensor_8(0x3C11, 0x08);
	write_cmos_sensor_8(0x3C12, 0x08);
	write_cmos_sensor_8(0x3C13, 0x2A);
	write_cmos_sensor_8(0x3F0C, 0x00);
	write_cmos_sensor_8(0x3F14, 0x01);
	write_cmos_sensor_8(0x3F80, 0x02);
	write_cmos_sensor_8(0x3F81, 0x20);
	write_cmos_sensor_8(0x3F8C, 0x01);
	write_cmos_sensor_8(0x3F8D, 0x9A);
	write_cmos_sensor_8(0x3FF8, 0x00);
	write_cmos_sensor_8(0x3FF9, 0x00);
	write_cmos_sensor_8(0x3FFE, 0x02);
	write_cmos_sensor_8(0x3FFF, 0x0E);
	/*Integration Setting*/
	write_cmos_sensor_8(0x0202, 0x17);
	write_cmos_sensor_8(0x0203, 0x83);
	write_cmos_sensor_8(0x0224, 0x01);
	write_cmos_sensor_8(0x0225, 0xF4);
	write_cmos_sensor_8(0x3116, 0x01);
	write_cmos_sensor_8(0x3117, 0xF4);
	/*Gain Setting*/
	write_cmos_sensor_8(0x0204, 0x00);
	write_cmos_sensor_8(0x0205, 0x70);
	write_cmos_sensor_8(0x0216, 0x00);
	write_cmos_sensor_8(0x0217, 0x70);
	write_cmos_sensor_8(0x0218, 0x01);
	write_cmos_sensor_8(0x0219, 0x00);
	write_cmos_sensor_8(0x020E, 0x01);
	write_cmos_sensor_8(0x020F, 0x00);
	write_cmos_sensor_8(0x0210, 0x01);
	write_cmos_sensor_8(0x0211, 0x00);
	write_cmos_sensor_8(0x0212, 0x01);
	write_cmos_sensor_8(0x0213, 0x00);
	write_cmos_sensor_8(0x0214, 0x01);
	write_cmos_sensor_8(0x0215, 0x00);
	write_cmos_sensor_8(0x3FE2, 0x00);
	write_cmos_sensor_8(0x3FE3, 0x70);
	write_cmos_sensor_8(0x3FE4, 0x01);
	write_cmos_sensor_8(0x3FE5, 0x00);
	/*PDAF TYPE2 Setting*/
	write_cmos_sensor_8(0x3E20, 0x02);
	write_cmos_sensor_8(0x3E3B, 0x00);
	write_cmos_sensor_8(0x4434, 0x00);
	write_cmos_sensor_8(0x4435, 0xF8);
	#endif
	pr_debug("%s 30 fpsX\n", __func__);
}

static void custom4_setting(void)
{
	pr_debug("%s 720p@240 fps E! currefps\n", __func__);

	#if USE_BURST_MODE
	imx686_table_write_cmos_sensor(imx686_custom4_setting,
		sizeof(imx686_custom4_setting)/sizeof(kal_uint16));
	#else
	/*MIPI output setting*/
	write_cmos_sensor_8(0x0112, 0x0A);
	write_cmos_sensor_8(0x0113, 0x0A);
	write_cmos_sensor_8(0x0114, 0x02);
	/*Line Length PCK Setting*/
	write_cmos_sensor_8(0x0342, 0x15);
	write_cmos_sensor_8(0x0343, 0x00);
	/*Frame Length Lines Setting*/
	write_cmos_sensor_8(0x0340, 0x05);
	write_cmos_sensor_8(0x0341, 0x2C);
	/*ROI Setting*/
	write_cmos_sensor_8(0x0344, 0x00);
	write_cmos_sensor_8(0x0345, 0x00);
	write_cmos_sensor_8(0x0346, 0x02);
	write_cmos_sensor_8(0x0347, 0xE8);
	write_cmos_sensor_8(0x0348, 0x1F);
	write_cmos_sensor_8(0x0349, 0x3F);
	write_cmos_sensor_8(0x034A, 0x14);
	write_cmos_sensor_8(0x034B, 0x87);
	/*Mode Setting*/
	write_cmos_sensor_8(0x0220, 0x62);
	write_cmos_sensor_8(0x0222, 0x01);
	write_cmos_sensor_8(0x0900, 0x01);
	write_cmos_sensor_8(0x0901, 0x44);
	write_cmos_sensor_8(0x0902, 0x08);
	write_cmos_sensor_8(0x3140, 0x00);
	write_cmos_sensor_8(0x3246, 0x89);
	write_cmos_sensor_8(0x3247, 0x89);
	write_cmos_sensor_8(0x3F15, 0x00);
	/*Digital Crop & Scaling*/
	write_cmos_sensor_8(0x0401, 0x00);
	write_cmos_sensor_8(0x0404, 0x00);
	write_cmos_sensor_8(0x0405, 0x10);
	write_cmos_sensor_8(0x0408, 0x00);
	write_cmos_sensor_8(0x0409, 0x00);
	write_cmos_sensor_8(0x040A, 0x00);
	write_cmos_sensor_8(0x040B, 0x00);
	write_cmos_sensor_8(0x040C, 0x07);
	write_cmos_sensor_8(0x040D, 0xD0);
	write_cmos_sensor_8(0x040E, 0x04);
	write_cmos_sensor_8(0x040F, 0x68);
	/*Output Size Setting*/
	write_cmos_sensor_8(0x034C, 0x07);
	write_cmos_sensor_8(0x034D, 0xD0);
	write_cmos_sensor_8(0x034E, 0x04);
	write_cmos_sensor_8(0x034F, 0x68);
	/*Clock Setting*/
	write_cmos_sensor_8(0x0301, 0x05);
	write_cmos_sensor_8(0x0303, 0x02);
	write_cmos_sensor_8(0x0305, 0x04);
	write_cmos_sensor_8(0x0306, 0x01);
	write_cmos_sensor_8(0x0307, 0x64);
	write_cmos_sensor_8(0x030B, 0x01);
	write_cmos_sensor_8(0x030D, 0x0C);
	write_cmos_sensor_8(0x030E, 0x02);
	write_cmos_sensor_8(0x030F, 0x71);
	write_cmos_sensor_8(0x0310, 0x01);
	/*Other Setting*/
	write_cmos_sensor_8(0x3620, 0x00);
	write_cmos_sensor_8(0x3621, 0x00);
	write_cmos_sensor_8(0x3C11, 0x0C);
	write_cmos_sensor_8(0x3C12, 0x05);
	write_cmos_sensor_8(0x3C13, 0x2C);
	write_cmos_sensor_8(0x3F0C, 0x00);
	write_cmos_sensor_8(0x3F14, 0x00);
	write_cmos_sensor_8(0x3F80, 0x02);
	write_cmos_sensor_8(0x3F81, 0x67);
	write_cmos_sensor_8(0x3F8C, 0x02);
	write_cmos_sensor_8(0x3F8D, 0x44);
	write_cmos_sensor_8(0x3FF8, 0x00);
	write_cmos_sensor_8(0x3FF9, 0x00);
	write_cmos_sensor_8(0x3FFE, 0x01);
	write_cmos_sensor_8(0x3FFF, 0x90);
	/*Integration Setting*/
	write_cmos_sensor_8(0x0202, 0x04);
	write_cmos_sensor_8(0x0203, 0xFC);
	write_cmos_sensor_8(0x0224, 0x01);
	write_cmos_sensor_8(0x0225, 0xF4);
	write_cmos_sensor_8(0x3FE0, 0x01);
	write_cmos_sensor_8(0x3FE1, 0xF4);
	/*Gain Setting*/
	write_cmos_sensor_8(0x0204, 0x00);
	write_cmos_sensor_8(0x0205, 0x70);
	write_cmos_sensor_8(0x0216, 0x00);
	write_cmos_sensor_8(0x0217, 0x70);
	write_cmos_sensor_8(0x0218, 0x01);
	write_cmos_sensor_8(0x0219, 0x00);
	write_cmos_sensor_8(0x020E, 0x01);
	write_cmos_sensor_8(0x020F, 0x00);
	write_cmos_sensor_8(0x0210, 0x01);
	write_cmos_sensor_8(0x0211, 0x00);
	write_cmos_sensor_8(0x0212, 0x01);
	write_cmos_sensor_8(0x0213, 0x00);
	write_cmos_sensor_8(0x0214, 0x01);
	write_cmos_sensor_8(0x0215, 0x00);
	write_cmos_sensor_8(0x3FE2, 0x00);
	write_cmos_sensor_8(0x3FE3, 0x70);
	write_cmos_sensor_8(0x3FE4, 0x01);
	write_cmos_sensor_8(0x3FE5, 0x00);
	/*PDAF TYPE2 Setting*/
	write_cmos_sensor_8(0x3E20, 0x01);
	write_cmos_sensor_8(0x3E3B, 0x00);
	write_cmos_sensor_8(0x4434, 0x00);
	write_cmos_sensor_8(0x4435, 0xF8);
	#endif
	pr_debug("X\n");
}

static void custom5_setting(void)
{
	int _length = 0;

	_length = sizeof(imx686_custom5_setting)/sizeof(kal_uint16);

	pr_debug("%s 720p@240 fps E! currefps\n", __func__);

#ifdef imx686_seamless_test
	if (!imx686_is_seamless)
		imx686_table_write_cmos_sensor(imx686_custom5_setting, _length);
	else {
		pr_debug("%s imx686_is_seamless %d, imx686_size_to_write %d\n",
			__func__, imx686_is_seamless, imx686_size_to_write);
		if (imx686_size_to_write + _length > _I2C_BUF_SIZE) {
			pr_debug("_too much i2c data for fast siwtch %d\n",
				imx686_size_to_write + _length);
			return;
		}
		memcpy((void *) (imx686_i2c_data + imx686_size_to_write),
			imx686_custom5_setting,
			sizeof(imx686_custom5_setting));
		imx686_size_to_write += _length;
	}

	//if (otp_flag == OTP_QSC_NONE) {
	//	pr_info("OTP no QSC Data, close qsc register");
	//	if (!imx686_is_seamless)
	//		write_cmos_sensor_8(0x3621, 0x00);//need check
	//	else {
	//		imx686_i2c_data[imx686_size_to_write++] = 0x3621;
	//		imx686_i2c_data[imx686_size_to_write++] = 0x0;
	//	}
	//}
#else
	#if USE_BURST_MODE
	imx686_table_write_cmos_sensor(imx686_custom5_setting,
		sizeof(imx686_custom5_setting)/sizeof(kal_uint16));
	#else
	/*MIPI output setting*/
	write_cmos_sensor_8(0x0112, 0x0A);
	write_cmos_sensor_8(0x0113, 0x0A);
	write_cmos_sensor_8(0x0114, 0x02);
	/*Line Length PCK Setting*/
	write_cmos_sensor_8(0x0342, 0x15);
	write_cmos_sensor_8(0x0343, 0x00);
	/*Frame Length Lines Setting*/
	write_cmos_sensor_8(0x0340, 0x05);
	write_cmos_sensor_8(0x0341, 0x2C);
	/*ROI Setting*/
	write_cmos_sensor_8(0x0344, 0x00);
	write_cmos_sensor_8(0x0345, 0x00);
	write_cmos_sensor_8(0x0346, 0x02);
	write_cmos_sensor_8(0x0347, 0xE8);
	write_cmos_sensor_8(0x0348, 0x1F);
	write_cmos_sensor_8(0x0349, 0x3F);
	write_cmos_sensor_8(0x034A, 0x14);
	write_cmos_sensor_8(0x034B, 0x87);
	/*Mode Setting*/
	write_cmos_sensor_8(0x0220, 0x62);
	write_cmos_sensor_8(0x0222, 0x01);
	write_cmos_sensor_8(0x0900, 0x01);
	write_cmos_sensor_8(0x0901, 0x44);
	write_cmos_sensor_8(0x0902, 0x08);
	write_cmos_sensor_8(0x3140, 0x00);
	write_cmos_sensor_8(0x3246, 0x89);
	write_cmos_sensor_8(0x3247, 0x89);
	write_cmos_sensor_8(0x3F15, 0x00);
	/*Digital Crop & Scaling*/
	write_cmos_sensor_8(0x0401, 0x00);
	write_cmos_sensor_8(0x0404, 0x00);
	write_cmos_sensor_8(0x0405, 0x10);
	write_cmos_sensor_8(0x0408, 0x00);
	write_cmos_sensor_8(0x0409, 0x00);
	write_cmos_sensor_8(0x040A, 0x00);
	write_cmos_sensor_8(0x040B, 0x00);
	write_cmos_sensor_8(0x040C, 0x07);
	write_cmos_sensor_8(0x040D, 0xD0);
	write_cmos_sensor_8(0x040E, 0x04);
	write_cmos_sensor_8(0x040F, 0x68);
	/*Output Size Setting*/
	write_cmos_sensor_8(0x034C, 0x07);
	write_cmos_sensor_8(0x034D, 0xD0);
	write_cmos_sensor_8(0x034E, 0x04);
	write_cmos_sensor_8(0x034F, 0x68);
	/*Clock Setting*/
	write_cmos_sensor_8(0x0301, 0x05);
	write_cmos_sensor_8(0x0303, 0x02);
	write_cmos_sensor_8(0x0305, 0x04);
	write_cmos_sensor_8(0x0306, 0x01);
	write_cmos_sensor_8(0x0307, 0x64);
	write_cmos_sensor_8(0x030B, 0x01);
	write_cmos_sensor_8(0x030D, 0x0C);
	write_cmos_sensor_8(0x030E, 0x02);
	write_cmos_sensor_8(0x030F, 0x71);
	write_cmos_sensor_8(0x0310, 0x01);
	/*Other Setting*/
	write_cmos_sensor_8(0x3620, 0x00);
	write_cmos_sensor_8(0x3621, 0x00);
	write_cmos_sensor_8(0x3C11, 0x0C);
	write_cmos_sensor_8(0x3C12, 0x05);
	write_cmos_sensor_8(0x3C13, 0x2C);
	write_cmos_sensor_8(0x3F0C, 0x00);
	write_cmos_sensor_8(0x3F14, 0x00);
	write_cmos_sensor_8(0x3F80, 0x02);
	write_cmos_sensor_8(0x3F81, 0x67);
	write_cmos_sensor_8(0x3F8C, 0x02);
	write_cmos_sensor_8(0x3F8D, 0x44);
	write_cmos_sensor_8(0x3FF8, 0x00);
	write_cmos_sensor_8(0x3FF9, 0x00);
	write_cmos_sensor_8(0x3FFE, 0x01);
	write_cmos_sensor_8(0x3FFF, 0x90);
	/*Integration Setting*/
	write_cmos_sensor_8(0x0202, 0x04);
	write_cmos_sensor_8(0x0203, 0xFC);
	write_cmos_sensor_8(0x0224, 0x01);
	write_cmos_sensor_8(0x0225, 0xF4);
	write_cmos_sensor_8(0x3FE0, 0x01);
	write_cmos_sensor_8(0x3FE1, 0xF4);
	/*Gain Setting*/
	write_cmos_sensor_8(0x0204, 0x00);
	write_cmos_sensor_8(0x0205, 0x70);
	write_cmos_sensor_8(0x0216, 0x00);
	write_cmos_sensor_8(0x0217, 0x70);
	write_cmos_sensor_8(0x0218, 0x01);
	write_cmos_sensor_8(0x0219, 0x00);
	write_cmos_sensor_8(0x020E, 0x01);
	write_cmos_sensor_8(0x020F, 0x00);
	write_cmos_sensor_8(0x0210, 0x01);
	write_cmos_sensor_8(0x0211, 0x00);
	write_cmos_sensor_8(0x0212, 0x01);
	write_cmos_sensor_8(0x0213, 0x00);
	write_cmos_sensor_8(0x0214, 0x01);
	write_cmos_sensor_8(0x0215, 0x00);
	write_cmos_sensor_8(0x3FE2, 0x00);
	write_cmos_sensor_8(0x3FE3, 0x70);
	write_cmos_sensor_8(0x3FE4, 0x01);
	write_cmos_sensor_8(0x3FE5, 0x00);
	/*PDAF TYPE2 Setting*/
	write_cmos_sensor_8(0x3E20, 0x01);
	write_cmos_sensor_8(0x3E3B, 0x00);
	write_cmos_sensor_8(0x4434, 0x00);
	write_cmos_sensor_8(0x4435, 0xF8);
	#endif
#endif
	pr_debug("X\n");
}

static void custom6_setting(void)
{
	int _length = 0;

	_length = sizeof(imx686_custom6_setting)/sizeof(kal_uint16);

	pr_debug("%s 720p@240 fps E! currefps\n", __func__);

#ifdef imx686_seamless_test //need check
	if (!imx686_is_seamless)
		imx686_table_write_cmos_sensor(imx686_custom6_setting, _length);
	else {
		pr_debug("%s imx686_is_seamless %d, imx686_size_to_write %d\n",
			__func__, imx686_is_seamless, imx686_size_to_write);
		if (imx686_size_to_write + _length > _I2C_BUF_SIZE) {
			pr_debug("_too much i2c data for fast siwtch %d\n",
				imx686_size_to_write + _length);
			return;
		}
		memcpy((void *) (imx686_i2c_data + imx686_size_to_write),
			imx686_custom6_setting,
			sizeof(imx686_custom6_setting));
		imx686_size_to_write += _length;
	}

	//if (otp_flag == OTP_QSC_NONE) {
	//	pr_info("OTP no QSC Data, close qsc register");
	//	if (!imx686_is_seamless)
	//		write_cmos_sensor_8(0x3621, 0x00);//need check
	//	else {
	//		imx686_i2c_data[imx686_size_to_write++] = 0x3621;
	//		imx686_i2c_data[imx686_size_to_write++] = 0x0;
	//	}
	//}
#else
	#if USE_BURST_MODE
	imx686_table_write_cmos_sensor(imx686_custom6_setting,
		sizeof(imx686_custom6_setting)/sizeof(kal_uint16));
	#else
	/*MIPI output setting*/
	write_cmos_sensor_8(0x0112, 0x0A);
	write_cmos_sensor_8(0x0113, 0x0A);
	write_cmos_sensor_8(0x0114, 0x02);
	/*Line Length PCK Setting*/
	write_cmos_sensor_8(0x0342, 0x15);
	write_cmos_sensor_8(0x0343, 0x00);
	/*Frame Length Lines Setting*/
	write_cmos_sensor_8(0x0340, 0x05);
	write_cmos_sensor_8(0x0341, 0x2C);
	/*ROI Setting*/
	write_cmos_sensor_8(0x0344, 0x00);
	write_cmos_sensor_8(0x0345, 0x00);
	write_cmos_sensor_8(0x0346, 0x02);
	write_cmos_sensor_8(0x0347, 0xE8);
	write_cmos_sensor_8(0x0348, 0x1F);
	write_cmos_sensor_8(0x0349, 0x3F);
	write_cmos_sensor_8(0x034A, 0x14);
	write_cmos_sensor_8(0x034B, 0x87);
	/*Mode Setting*/
	write_cmos_sensor_8(0x0220, 0x62);
	write_cmos_sensor_8(0x0222, 0x01);
	write_cmos_sensor_8(0x0900, 0x01);
	write_cmos_sensor_8(0x0901, 0x44);
	write_cmos_sensor_8(0x0902, 0x08);
	write_cmos_sensor_8(0x3140, 0x00);
	write_cmos_sensor_8(0x3246, 0x89);
	write_cmos_sensor_8(0x3247, 0x89);
	write_cmos_sensor_8(0x3F15, 0x00);
	/*Digital Crop & Scaling*/
	write_cmos_sensor_8(0x0401, 0x00);
	write_cmos_sensor_8(0x0404, 0x00);
	write_cmos_sensor_8(0x0405, 0x10);
	write_cmos_sensor_8(0x0408, 0x00);
	write_cmos_sensor_8(0x0409, 0x00);
	write_cmos_sensor_8(0x040A, 0x00);
	write_cmos_sensor_8(0x040B, 0x00);
	write_cmos_sensor_8(0x040C, 0x07);
	write_cmos_sensor_8(0x040D, 0xD0);
	write_cmos_sensor_8(0x040E, 0x04);
	write_cmos_sensor_8(0x040F, 0x68);
	/*Output Size Setting*/
	write_cmos_sensor_8(0x034C, 0x07);
	write_cmos_sensor_8(0x034D, 0xD0);
	write_cmos_sensor_8(0x034E, 0x04);
	write_cmos_sensor_8(0x034F, 0x68);
	/*Clock Setting*/
	write_cmos_sensor_8(0x0301, 0x05);
	write_cmos_sensor_8(0x0303, 0x02);
	write_cmos_sensor_8(0x0305, 0x04);
	write_cmos_sensor_8(0x0306, 0x01);
	write_cmos_sensor_8(0x0307, 0x64);
	write_cmos_sensor_8(0x030B, 0x01);
	write_cmos_sensor_8(0x030D, 0x0C);
	write_cmos_sensor_8(0x030E, 0x02);
	write_cmos_sensor_8(0x030F, 0x71);
	write_cmos_sensor_8(0x0310, 0x01);
	/*Other Setting*/
	write_cmos_sensor_8(0x3620, 0x00);
	write_cmos_sensor_8(0x3621, 0x00);
	write_cmos_sensor_8(0x3C11, 0x0C);
	write_cmos_sensor_8(0x3C12, 0x05);
	write_cmos_sensor_8(0x3C13, 0x2C);
	write_cmos_sensor_8(0x3F0C, 0x00);
	write_cmos_sensor_8(0x3F14, 0x00);
	write_cmos_sensor_8(0x3F80, 0x02);
	write_cmos_sensor_8(0x3F81, 0x67);
	write_cmos_sensor_8(0x3F8C, 0x02);
	write_cmos_sensor_8(0x3F8D, 0x44);
	write_cmos_sensor_8(0x3FF8, 0x00);
	write_cmos_sensor_8(0x3FF9, 0x00);
	write_cmos_sensor_8(0x3FFE, 0x01);
	write_cmos_sensor_8(0x3FFF, 0x90);
	/*Integration Setting*/
	write_cmos_sensor_8(0x0202, 0x04);
	write_cmos_sensor_8(0x0203, 0xFC);
	write_cmos_sensor_8(0x0224, 0x01);
	write_cmos_sensor_8(0x0225, 0xF4);
	write_cmos_sensor_8(0x3FE0, 0x01);
	write_cmos_sensor_8(0x3FE1, 0xF4);
	/*Gain Setting*/
	write_cmos_sensor_8(0x0204, 0x00);
	write_cmos_sensor_8(0x0205, 0x70);
	write_cmos_sensor_8(0x0216, 0x00);
	write_cmos_sensor_8(0x0217, 0x70);
	write_cmos_sensor_8(0x0218, 0x01);
	write_cmos_sensor_8(0x0219, 0x00);
	write_cmos_sensor_8(0x020E, 0x01);
	write_cmos_sensor_8(0x020F, 0x00);
	write_cmos_sensor_8(0x0210, 0x01);
	write_cmos_sensor_8(0x0211, 0x00);
	write_cmos_sensor_8(0x0212, 0x01);
	write_cmos_sensor_8(0x0213, 0x00);
	write_cmos_sensor_8(0x0214, 0x01);
	write_cmos_sensor_8(0x0215, 0x00);
	write_cmos_sensor_8(0x3FE2, 0x00);
	write_cmos_sensor_8(0x3FE3, 0x70);
	write_cmos_sensor_8(0x3FE4, 0x01);
	write_cmos_sensor_8(0x3FE5, 0x00);
	/*PDAF TYPE2 Setting*/
	write_cmos_sensor_8(0x3E20, 0x01);
	write_cmos_sensor_8(0x3E3B, 0x00);
	write_cmos_sensor_8(0x4434, 0x00);
	write_cmos_sensor_8(0x4435, 0xF8);
	#endif
#endif
	pr_debug("X\n");
}

static void custom1_setting(void)
{
	pr_debug("%s CUS1_12M_60_FPS E! currefps\n", __func__);
	/*************MIPI output setting************/
	#if USE_BURST_MODE
	imx686_table_write_cmos_sensor(imx686_custom1_setting,
		sizeof(imx686_custom1_setting)/sizeof(kal_uint16));
	#else
	/*MIPI output setting*/
	write_cmos_sensor_8(0x0112, 0x0A);
	write_cmos_sensor_8(0x0113, 0x0A);
	write_cmos_sensor_8(0x0114, 0x02);
	/*Line Length PCK Setting*/
	write_cmos_sensor_8(0x0342, 0x1E);
	write_cmos_sensor_8(0x0343, 0xC0);
	/*Frame Length Lines Setting*/
	write_cmos_sensor_8(0x0340, 0x0C);
	write_cmos_sensor_8(0x0341, 0x0E);
	/*ROI Setting*/
	write_cmos_sensor_8(0x0344, 0x00);
	write_cmos_sensor_8(0x0345, 0x00);
	write_cmos_sensor_8(0x0346, 0x00);
	write_cmos_sensor_8(0x0347, 0x00);
	write_cmos_sensor_8(0x0348, 0x1F);
	write_cmos_sensor_8(0x0349, 0x3F);
	write_cmos_sensor_8(0x034A, 0x17);
	write_cmos_sensor_8(0x034B, 0x6F);
	/*Mode Setting*/
	write_cmos_sensor_8(0x0220, 0x62);
	write_cmos_sensor_8(0x0222, 0x01);
	write_cmos_sensor_8(0x0900, 0x01);
	write_cmos_sensor_8(0x0901, 0x22);
	write_cmos_sensor_8(0x0902, 0x08);
	write_cmos_sensor_8(0x3140, 0x00);
	write_cmos_sensor_8(0x3246, 0x81);
	write_cmos_sensor_8(0x3247, 0x81);
	write_cmos_sensor_8(0x3F15, 0x00);
	/*Digital Crop & Scaling*/
	write_cmos_sensor_8(0x0401, 0x00);
	write_cmos_sensor_8(0x0404, 0x00);
	write_cmos_sensor_8(0x0405, 0x10);
	write_cmos_sensor_8(0x0408, 0x00);
	write_cmos_sensor_8(0x0409, 0x00);
	write_cmos_sensor_8(0x040A, 0x00);
	write_cmos_sensor_8(0x040B, 0x00);
	write_cmos_sensor_8(0x040C, 0x0F);
	write_cmos_sensor_8(0x040D, 0xA0);
	write_cmos_sensor_8(0x040E, 0x0B);
	write_cmos_sensor_8(0x040F, 0xB8);
	/*Output Size Setting*/
	write_cmos_sensor_8(0x034C, 0x0F);
	write_cmos_sensor_8(0x034D, 0xA0);
	write_cmos_sensor_8(0x034E, 0x0B);
	write_cmos_sensor_8(0x034F, 0xB8);
	/*Clock Setting*/
	write_cmos_sensor_8(0x0301, 0x05);
	write_cmos_sensor_8(0x0303, 0x04);
	write_cmos_sensor_8(0x0305, 0x04);
	write_cmos_sensor_8(0x0306, 0x00);
	write_cmos_sensor_8(0x0307, 0xF3);
	write_cmos_sensor_8(0x030B, 0x02);
	write_cmos_sensor_8(0x030D, 0x0C);
	write_cmos_sensor_8(0x030E, 0x02);
	write_cmos_sensor_8(0x030F, 0x49);
	write_cmos_sensor_8(0x0310, 0x01);
	/*Other Setting*/
	write_cmos_sensor_8(0x3620, 0x00);
	write_cmos_sensor_8(0x3621, 0x00);
	write_cmos_sensor_8(0x3C11, 0x04);
	write_cmos_sensor_8(0x3C12, 0x03);
	write_cmos_sensor_8(0x3C13, 0x2D);
	write_cmos_sensor_8(0x3F0C, 0x01);
	write_cmos_sensor_8(0x3F14, 0x00);
	write_cmos_sensor_8(0x3F80, 0x01);
	write_cmos_sensor_8(0x3F81, 0x90);
	write_cmos_sensor_8(0x3F8C, 0x00);
	write_cmos_sensor_8(0x3F8D, 0x14);
	write_cmos_sensor_8(0x3FF8, 0x01);
	write_cmos_sensor_8(0x3FF9, 0x2A);
	write_cmos_sensor_8(0x3FFE, 0x00);
	write_cmos_sensor_8(0x3FFF, 0x6C);
	/*Integration Setting*/
	write_cmos_sensor_8(0x0202, 0x0B);
	write_cmos_sensor_8(0x0203, 0xDE);
	write_cmos_sensor_8(0x0224, 0x01);
	write_cmos_sensor_8(0x0225, 0xF4);
	write_cmos_sensor_8(0x3116, 0x01);
	write_cmos_sensor_8(0x3117, 0xF4);
	/*Gain Setting*/
	write_cmos_sensor_8(0x0204, 0x00);
	write_cmos_sensor_8(0x0205, 0x70);
	write_cmos_sensor_8(0x0216, 0x00);
	write_cmos_sensor_8(0x0217, 0x70);
	write_cmos_sensor_8(0x0218, 0x01);
	write_cmos_sensor_8(0x0219, 0x00);
	write_cmos_sensor_8(0x020E, 0x01);
	write_cmos_sensor_8(0x020F, 0x00);
	write_cmos_sensor_8(0x0210, 0x01);
	write_cmos_sensor_8(0x0211, 0x00);
	write_cmos_sensor_8(0x0212, 0x01);
	write_cmos_sensor_8(0x0213, 0x00);
	write_cmos_sensor_8(0x0214, 0x01);
	write_cmos_sensor_8(0x0215, 0x00);
	write_cmos_sensor_8(0x3FE2, 0x00);
	write_cmos_sensor_8(0x3FE3, 0x70);
	write_cmos_sensor_8(0x3FE4, 0x01);
	write_cmos_sensor_8(0x3FE5, 0x00);
	/*PDAF TYPE2 Setting*/
	write_cmos_sensor_8(0x3E20, 0x02);
	write_cmos_sensor_8(0x3E3B, 0x01);
	write_cmos_sensor_8(0x4434, 0x01);
	write_cmos_sensor_8(0x4435, 0xF0);
	#endif
	pr_debug("X");
}

static void custom2_setting(void)
{
	pr_debug("%s 3840*2160@60fps E! currefps\n", __func__);
	/*************MIPI output setting************/
	#if USE_BURST_MODE
	imx686_table_write_cmos_sensor(imx686_normal_video_setting_60FPS,
		sizeof(imx686_normal_video_setting_60FPS)/sizeof(kal_uint16));
	#else
	/*MIPI output setting*/
	write_cmos_sensor_8(0x0112, 0x0A);
	write_cmos_sensor_8(0x0113, 0x0A);
	write_cmos_sensor_8(0x0114, 0x02);
	/*Line Length PCK Setting*/
	write_cmos_sensor_8(0x0342, 0x1E);
	write_cmos_sensor_8(0x0343, 0xC0);
	/*Frame Length Lines Setting*/
	write_cmos_sensor_8(0x0340, 0x08);
	write_cmos_sensor_8(0x0341, 0xB0);
	/*ROI Setting*/
	write_cmos_sensor_8(0x0344, 0x00);
	write_cmos_sensor_8(0x0345, 0x00);
	write_cmos_sensor_8(0x0346, 0x03);
	write_cmos_sensor_8(0x0347, 0x48);
	write_cmos_sensor_8(0x0348, 0x1F);
	write_cmos_sensor_8(0x0349, 0x3F);
	write_cmos_sensor_8(0x034A, 0x14);
	write_cmos_sensor_8(0x034B, 0x27);
	/*Mode Setting*/
	write_cmos_sensor_8(0x0220, 0x62);
	write_cmos_sensor_8(0x0222, 0x01);
	write_cmos_sensor_8(0x0900, 0x01);
	write_cmos_sensor_8(0x0901, 0x22);
	write_cmos_sensor_8(0x0902, 0x08);
	write_cmos_sensor_8(0x3140, 0x00);
	write_cmos_sensor_8(0x3246, 0x81);
	write_cmos_sensor_8(0x3247, 0x81);
	write_cmos_sensor_8(0x3F15, 0x00);
	/*Digital Crop & Scaling*/
	write_cmos_sensor_8(0x0401, 0x00);
	write_cmos_sensor_8(0x0404, 0x00);
	write_cmos_sensor_8(0x0405, 0x10);
	write_cmos_sensor_8(0x0408, 0x00);
	write_cmos_sensor_8(0x0409, 0x50);
	write_cmos_sensor_8(0x040A, 0x00);
	write_cmos_sensor_8(0x040B, 0x00);
	write_cmos_sensor_8(0x040C, 0x0F);
	write_cmos_sensor_8(0x040D, 0x00);
	write_cmos_sensor_8(0x040E, 0x08);
	write_cmos_sensor_8(0x040F, 0x70);
	/*Output Size Setting*/
	write_cmos_sensor_8(0x034C, 0x0F);
	write_cmos_sensor_8(0x034D, 0x00);
	write_cmos_sensor_8(0x034E, 0x08);
	write_cmos_sensor_8(0x034F, 0x70);
	/*Clock Setting*/
	write_cmos_sensor_8(0x0301, 0x05);
	write_cmos_sensor_8(0x0303, 0x02);
	write_cmos_sensor_8(0x0305, 0x04);
	write_cmos_sensor_8(0x0306, 0x00);
	write_cmos_sensor_8(0x0307, 0xDB);
	write_cmos_sensor_8(0x030B, 0x02);
	write_cmos_sensor_8(0x030D, 0x0C);
	write_cmos_sensor_8(0x030E, 0x03);
	write_cmos_sensor_8(0x030F, 0xD2);
	write_cmos_sensor_8(0x0310, 0x01);
	/*Other Setting*/
	write_cmos_sensor_8(0x3620, 0x00);
	write_cmos_sensor_8(0x3621, 0x00);
	write_cmos_sensor_8(0x3C11, 0x04);
	write_cmos_sensor_8(0x3C12, 0x03);
	write_cmos_sensor_8(0x3C13, 0x2D);
	write_cmos_sensor_8(0x3F0C, 0x01);
	write_cmos_sensor_8(0x3F14, 0x00);
	write_cmos_sensor_8(0x3F80, 0x01);
	write_cmos_sensor_8(0x3F81, 0x90);
	write_cmos_sensor_8(0x3F8C, 0x00);
	write_cmos_sensor_8(0x3F8D, 0x14);
	write_cmos_sensor_8(0x3FF8, 0x01);
	write_cmos_sensor_8(0x3FF9, 0x2A);
	write_cmos_sensor_8(0x3FFE, 0x00);
	write_cmos_sensor_8(0x3FFF, 0x6C);
	/*Integration Setting*/
	write_cmos_sensor_8(0x0202, 0x08);
	write_cmos_sensor_8(0x0203, 0x80);
	write_cmos_sensor_8(0x0224, 0x01);
	write_cmos_sensor_8(0x0225, 0xF4);
	write_cmos_sensor_8(0x3116, 0x01);
	write_cmos_sensor_8(0x3117, 0xF4);
	/*Gain Setting*/
	write_cmos_sensor_8(0x0204, 0x00);
	write_cmos_sensor_8(0x0205, 0x70);
	write_cmos_sensor_8(0x0216, 0x00);
	write_cmos_sensor_8(0x0217, 0x70);
	write_cmos_sensor_8(0x0218, 0x01);
	write_cmos_sensor_8(0x0219, 0x00);
	write_cmos_sensor_8(0x020E, 0x01);
	write_cmos_sensor_8(0x020F, 0x00);
	write_cmos_sensor_8(0x0210, 0x01);
	write_cmos_sensor_8(0x0211, 0x00);
	write_cmos_sensor_8(0x0212, 0x01);
	write_cmos_sensor_8(0x0213, 0x00);
	write_cmos_sensor_8(0x0214, 0x01);
	write_cmos_sensor_8(0x0215, 0x00);
	write_cmos_sensor_8(0x3FE2, 0x00);
	write_cmos_sensor_8(0x3FE3, 0x70);
	write_cmos_sensor_8(0x3FE4, 0x01);
	write_cmos_sensor_8(0x3FE5, 0x00);
	/*PDAF TYPE2 Setting*/
	write_cmos_sensor_8(0x3E20, 0x02);
	write_cmos_sensor_8(0x3E3B, 0x01);
	write_cmos_sensor_8(0x4434, 0x01);
	write_cmos_sensor_8(0x4435, 0xE0);
	#endif
	pr_debug("X");
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
	/*sensor have two i2c address 0x34 & 0x20,
	 *we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = ((read_cmos_sensor_8(0x0016) << 8)
					| read_cmos_sensor_8(0x0017));
			pr_debug(
				"read_0x0000=0x%x, 0x0001=0x%x,0x0000_0001=0x%x\n",
				read_cmos_sensor_8(0x0016),
				read_cmos_sensor_8(0x0017),
				read_cmos_sensor(0x0000));
			if (*sensor_id == imgsensor_info.sensor_id) {
				pr_debug("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);
				read_sensor_Cali();
				return ERROR_NONE;
			}

			pr_debug("Read sensor id fail, id: 0x%x\n",
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
	kal_uint16 sensor_id = 0;

	pr_debug("%s +\n", __func__);
	/*sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 *we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = ((read_cmos_sensor_8(0x0016) << 8)
					| read_cmos_sensor_8(0x0017));
			if (sensor_id == imgsensor_info.sensor_id) {
				pr_debug("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, sensor_id);
				break;
			}
			pr_debug("Read sensor id fail, id: 0x%x\n",
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
	if (0)
		write_sensor_LRC();
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
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);
	pr_debug("%s -\n", __func__);

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
	pr_debug("E\n");
	/* No Need to implement this function */
	streaming_control(KAL_FALSE);
	qsc_flag = 0;
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
	pr_debug("%s. 4608*3456@30FPS\n", __func__);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	//set_mirror_flip(imgsensor.mirror);
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
	pr_debug("%s. 4608*3456@30FPS\n", __func__);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;

	if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
		pr_debug(
			"Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
			imgsensor.current_fps,
			imgsensor_info.cap.max_framerate / 10);
	imgsensor.pclk = imgsensor_info.cap.pclk;
	imgsensor.line_length = imgsensor_info.cap.linelength;
	imgsensor.frame_length = imgsensor_info.cap.framelength;
	imgsensor.min_frame_length = imgsensor_info.cap.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;

	spin_unlock(&imgsensor_drv_lock);
	capture_setting(imgsensor.current_fps);
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/* capture() */
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("%s. 4608*2592@30FPS\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting(imgsensor.current_fps);
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/*	normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("%s. 2304*1296@120FPS\n", __func__);

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
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/*	hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("%s. 4608*2592@30FPS\n", __func__);

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
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/* slim_video */


static kal_uint32 custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("%s. 4608*3456@24FPS\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk = imgsensor_info.custom1.pclk;
	imgsensor.line_length = imgsensor_info.custom1.linelength;
	imgsensor.frame_length = imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom1_setting();

	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/* custom1 */

static kal_uint32 custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("%s. 4608*2592@60FPS\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM2;
	imgsensor.pclk = imgsensor_info.custom2.pclk;
	imgsensor.line_length = imgsensor_info.custom2.linelength;
	imgsensor.frame_length = imgsensor_info.custom2.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom2_setting();
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/* custom2 */

static kal_uint32 custom3(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("%s. 1280*720@120FPS\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM3;
	imgsensor.pclk = imgsensor_info.custom3.pclk;
	imgsensor.line_length = imgsensor_info.custom3.linelength;
	imgsensor.frame_length = imgsensor_info.custom3.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom3.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	if (!qsc_flag) {
		pr_debug("write_sensor_QSC Start\n");
		mdelay(67);
		write_sensor_QSC();
		pr_debug("write_sensor_QSC End\n");
		qsc_flag = 1;
	}
	custom3_setting();
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/* custom3 */

static kal_uint32 custom4(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("%s. 2304*1296@240FPS\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM4;
	imgsensor.pclk = imgsensor_info.custom4.pclk;
	imgsensor.line_length = imgsensor_info.custom4.linelength;
	imgsensor.frame_length = imgsensor_info.custom4.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom4.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom4_setting();
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/* custom4 */

static kal_uint32 custom5(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("%s. 4608*3456@30FPS\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM5;
	imgsensor.pclk = imgsensor_info.custom5.pclk;
	imgsensor.line_length = imgsensor_info.custom5.linelength;
	imgsensor.frame_length = imgsensor_info.custom5.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom5.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom5_setting();
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/* custom5 */

static kal_uint32 custom6(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("%s. 9216*6912@30FPS\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM6;
	imgsensor.pclk = imgsensor_info.custom6.pclk;
	imgsensor.line_length = imgsensor_info.custom6.linelength;
	imgsensor.frame_length = imgsensor_info.custom6.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom6.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	//if (!qsc_flag) {
	//	pr_debug("write_sensor_QSC Start\n");
	//	mdelay(1);
	//	write_sensor_QSC();
	//	pr_debug("write_sensor_QSC End\n");
	//	qsc_flag = 1;
	//}
	custom6_setting();
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/* custom6 */

static kal_uint32
get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	pr_debug("E\n");
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

	sensor_resolution->SensorCustom6Width =
		imgsensor_info.custom6.grabwindow_width;
	sensor_resolution->SensorCustom6Height =
		imgsensor_info.custom6.grabwindow_height;

	return ERROR_NONE;
} /* get_resolution */

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			   MSDK_SENSOR_INFO_STRUCT *sensor_info,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("scenario_id = %d\n", scenario_id);

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
	sensor_info->Custom6DelayFrame = imgsensor_info.custom6_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	sensor_info->AESensorGainDelayFrame =
		imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame =
		imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
	sensor_info->PDAF_Support = 2;
	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->TEMPERATURE_SUPPORT = imgsensor_info.temperature_support;
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

	sensor_info->FrameTimeDelayFrame =
		imgsensor_info.frame_time_delay_frame;

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

	case MSDK_SCENARIO_ID_CUSTOM6:
		sensor_info->SensorGrabStartX = imgsensor_info.custom6.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom6.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom6.mipi_data_lp2hs_settle_dc;
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
	pr_debug("scenario_id = %d\n", scenario_id);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);
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
	case MSDK_SCENARIO_ID_CUSTOM6:
		custom6(image_window, sensor_config_data);
		break;
	default:
		pr_debug("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}

	return ERROR_NONE;
}	/* control() */

#ifdef imx686_seamless_test
static kal_uint32 seamless_switch(enum MSDK_SCENARIO_ID_ENUM scenario_id,
	kal_uint32 shutter, kal_uint32 gain,
	kal_uint32 shutter_2ndframe, kal_uint32 gain_2ndframe)
{
	//int k = 0;
	imx686_is_seamless = true;
	memset(imx686_i2c_data, 0x0, sizeof(imx686_i2c_data));
	imx686_size_to_write = 0;

	pr_debug("seamless switch %d, %d, %d, %d, %d sizeof(imx686_i2c_data) %d\n",
		scenario_id, shutter, gain,
		shutter_2ndframe, gain_2ndframe,
		sizeof(imx686_i2c_data));

	if (scenario_id != MSDK_SCENARIO_ID_CUSTOM5 &&
		scenario_id != MSDK_SCENARIO_ID_CUSTOM6)
		return ERROR_INVALID_SCENARIO_ID;


	imx686_i2c_data[imx686_size_to_write++] = 0x0104;
	imx686_i2c_data[imx686_size_to_write++] = 0x01;

	control(scenario_id, NULL, NULL);
	if (shutter != 0)
		set_shutter(shutter);
	if (gain != 0)
		set_gain(gain);

	imx686_i2c_data[imx686_size_to_write++] = 0x0104;
	imx686_i2c_data[imx686_size_to_write++] = 0;

	pr_debug("%s imx686_is_seamless %d, imx686_size_to_write %d\n",
				__func__, imx686_is_seamless, imx686_size_to_write);

	imx686_table_write_cmos_sensor(
		imx686_i2c_data,
		imx686_size_to_write);

	imx686_is_seamless = false;
	pr_debug("exit\n");
	return ERROR_NONE;
}
#endif

static kal_uint32 set_video_mode(UINT16 framerate)
{
	pr_debug("framerate = %d\n ", framerate);
	/* SetVideoMode Function should fix framerate */
	if (framerate == 0)
		/* Dynamic frame rate */
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
	pr_debug("enable = %d, framerate = %d\n", enable, framerate);
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

	pr_debug("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frame_length = imgsensor_info.pre.pclk / framerate * 10
				/ imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.pre.framelength)
		? (frame_length - imgsensor_info.pre.framelength) : 0;
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
		frame_length = imgsensor_info.normal_video.pclk /
				framerate * 10 /
				imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.normal_video.framelength)
		? (frame_length - imgsensor_info.normal_video.framelength)
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
	if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
		pr_debug(
			"Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n"
			, framerate, imgsensor_info.cap.max_framerate/10);
		frame_length = imgsensor_info.cap.pclk / framerate * 10
				/ imgsensor_info.cap.linelength;
		spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
			(frame_length > imgsensor_info.cap.framelength)
			  ? (frame_length - imgsensor_info.cap.framelength) : 0;
			imgsensor.frame_length =
				imgsensor_info.cap.framelength
				+ imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);

		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk / framerate * 10
				/ imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.hs_video.framelength)
			  ? (frame_length - imgsensor_info.hs_video.framelength)
			  : 0;
		imgsensor.frame_length =
			imgsensor_info.hs_video.framelength
				+ imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk / framerate * 10
			/ imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.slim_video.framelength)
			? (frame_length - imgsensor_info.slim_video.framelength)
			: 0;
		imgsensor.frame_length =
			imgsensor_info.slim_video.framelength
			+ imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		frame_length = imgsensor_info.custom1.pclk / framerate * 10
				/ imgsensor_info.custom1.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.custom1.framelength)
			? (frame_length - imgsensor_info.custom1.framelength)
			: 0;
		imgsensor.frame_length =
			imgsensor_info.custom1.framelength
			+ imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		frame_length = imgsensor_info.custom2.pclk / framerate * 10
				/ imgsensor_info.custom2.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.custom2.framelength)
			? (frame_length - imgsensor_info.custom2.framelength)
			: 0;
		imgsensor.frame_length =
			imgsensor_info.custom2.framelength
			+ imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		frame_length = imgsensor_info.custom3.pclk / framerate * 10
				/ imgsensor_info.custom3.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.custom3.framelength)
		? (frame_length - imgsensor_info.custom3.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.custom3.framelength
			+ imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		frame_length = imgsensor_info.custom4.pclk / framerate * 10
				/ imgsensor_info.custom4.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.custom4.framelength)
		? (frame_length - imgsensor_info.custom4.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.custom4.framelength
			+ imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		frame_length = imgsensor_info.custom5.pclk / framerate * 10
				/ imgsensor_info.custom5.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.custom5.framelength)
		? (frame_length - imgsensor_info.custom5.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.custom5.framelength
			+ imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM6:
		frame_length = imgsensor_info.custom6.pclk / framerate * 10
				/ imgsensor_info.custom6.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.custom6.framelength)
		? (frame_length - imgsensor_info.custom6.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.custom6.framelength
			+ imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	default:  /*coding with  preview scenario by default*/
		frame_length = imgsensor_info.pre.pclk / framerate * 10
			/ imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.pre.framelength)
			? (frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		pr_debug("error scenario_id = %d, we use preview scenario\n",
			scenario_id);
		break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(
		enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	pr_debug("scenario_id = %d\n", scenario_id);

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
	case MSDK_SCENARIO_ID_CUSTOM6:
		*framerate = imgsensor_info.custom6.max_framerate;
		break;
	default:
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	pr_debug("enable: %d\n", enable);

	if (enable)
		write_cmos_sensor_8(0x0601, 0x0002); /*100% Color bar*/
	else
		write_cmos_sensor_8(0x0601, 0x0000); /*No pattern*/

	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 get_sensor_temperature(void)
{
	UINT8 temperature;
	INT32 temperature_convert;

	temperature = read_cmos_sensor_8(0x013a);

	if (temperature <= 0x4F)
		temperature_convert = temperature;
	else if (temperature >= 0x50 && temperature <= 0x7F)
		temperature_convert = 80;
	else if (temperature >= 0x80 && temperature <= 0xEC)
		temperature_convert = -20;
	else
		temperature_convert = (INT8) temperature;

	/* LOG_INF("temp_c(%d), read_reg(%d)\n", */
	/* temperature_convert, temperature); */

	return temperature_convert;
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
	uint32_t *pAeCtrls;
	uint32_t *pScenarios;
	/* SET_SENSOR_AWB_GAIN *pSetSensorAWB
	 *  = (SET_SENSOR_AWB_GAIN *)feature_para;
	 */
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data
		= (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	/*pr_debug("feature_id = %d\n", feature_id);*/
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
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_CUSTOM1:
		case MSDK_SCENARIO_ID_CUSTOM2:
		case MSDK_SCENARIO_ID_CUSTOM4:
		case MSDK_SCENARIO_ID_CUSTOM5:
			*(feature_data + 2) = 2;
			break;
		case MSDK_SCENARIO_ID_CUSTOM6:
			*(feature_data + 2) = 2;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
		default:
			*(feature_data + 2) = 1;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_AWB_REQ_BY_SCENARIO:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1;
			break;
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
		}
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
		case MSDK_SCENARIO_ID_CUSTOM6:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom6.pclk;
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
			= (imgsensor_info.normal_video.framelength << 16)
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
		case MSDK_SCENARIO_ID_CUSTOM6:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom6.framelength << 16)
				+ imgsensor_info.custom6.linelength;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.pre.framelength << 16)
				+ imgsensor_info.pre.linelength;
			break;
		}
		break;

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
#ifdef imx686_seamless_test
		if (sensor_reg_data->RegAddr == 0xff) {
			seamless_switch(sensor_reg_data->RegData, 0, 0, 0, 0);
		} else {
			write_cmos_sensor_8(sensor_reg_data->RegAddr,
			sensor_reg_data->RegData);
		}
		break;
#else
		write_cmos_sensor_8(sensor_reg_data->RegAddr,
				    sensor_reg_data->RegData);
#endif
		break;

	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData =
			read_cmos_sensor_8(sensor_reg_data->RegAddr);
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
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		*feature_return_para_32 = get_sensor_temperature();
		*feature_para_len = 4;
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
		pr_debug("SENSOR_FEATURE_GET_PDAF_DATA\n");
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL)*feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		/* for factory mode auto testing */
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		pr_debug("current fps :%d\n", (UINT32)*feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = *feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		pr_debug("ihdr enable :%d\n", (BOOL)*feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_mode = *feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		pr_debug("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
			(UINT32)*feature_data);
		wininfo =
	(struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));

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
		case MSDK_SCENARIO_ID_CUSTOM6:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[10],
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
		pr_debug("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%d\n",
			(UINT16) *feature_data);
		PDAFinfo =
		  (struct SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data+1));
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CUSTOM1:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG: //4608*3456
			imgsensor_pd_info_binning.i4BlockNumX = 574;
			imgsensor_pd_info_binning.i4BlockNumY = 215;
			memcpy((void *)PDAFinfo,
				(void *)&imgsensor_pd_info_binning,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			imgsensor_pd_info_binning.i4BlockNumX = 574;
			imgsensor_pd_info_binning.i4BlockNumY = 187;
			memcpy((void *)PDAFinfo,
				(void *)&imgsensor_pd_info_binning,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case MSDK_SCENARIO_ID_CUSTOM6:
			imgsensor_pd_info_binning.i4BlockNumX = 574;
			imgsensor_pd_info_binning.i4BlockNumY = 187;
			memcpy((void *)PDAFinfo,
				(void *)&imgsensor_pd_info_binning,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_CUSTOM2:
		case MSDK_SCENARIO_ID_SLIM_VIDEO: // 4608*2592
			imgsensor_pd_info_binning.i4BlockNumX = 574;
			imgsensor_pd_info_binning.i4BlockNumY = 161;
			memcpy((void *)PDAFinfo,
				(void *)&imgsensor_pd_info_binning,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		default:
			break;
		}
		break;
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		pr_debug(
		"SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%d\n",
			(UINT16) *feature_data);
		/*PDAF capacity enable or not, 2p8 only full size support PDAF*/
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_CUSTOM1:
		case MSDK_SCENARIO_ID_CUSTOM2:
		case MSDK_SCENARIO_ID_CUSTOM5:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_CUSTOM6:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PDAF_REG_SETTING:
		pr_debug("SENSOR_FEATURE_GET_PDAF_REG_SETTING %d",
			(*feature_para_len));
		imx686_get_pdaf_reg_setting((*feature_para_len) / sizeof(UINT32)
					   , feature_data_16);
		break;
	case SENSOR_FEATURE_SET_PDAF_REG_SETTING:
		pr_debug("SENSOR_FEATURE_SET_PDAF_REG_SETTING %d",
			(*feature_para_len));
		imx686_set_pdaf_reg_setting((*feature_para_len) / sizeof(UINT32)
					   , feature_data_16);
		break;
	case SENSOR_FEATURE_SET_PDAF:
		pr_debug("PDAF mode :%d\n", *feature_data_16);
		imgsensor.pdaf_mode = *feature_data_16;
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		pr_debug("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
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
	case SENSOR_FEATURE_SET_HDR_SHUTTER:
		pr_debug("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
			(UINT16)*feature_data, (UINT16)*(feature_data+1));
#if imx686_flag
		ihdr_write_shutter((UINT16)*feature_data,
				   (UINT16)*(feature_data+1));
#endif
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		pr_debug("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		pr_debug("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
			*feature_data);
		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(KAL_TRUE);
		break;
	case SENSOR_FEATURE_GET_BINNING_TYPE:
		switch (*(feature_data + 1)) {
		case MSDK_SCENARIO_ID_CUSTOM3:
#ifdef new_init_setting
			*feature_return_para_32 = 1; /*BINNING_NONE*/
#else
			*feature_return_para_32 = 1; /*BINNING_NONE*/
#endif
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
		case MSDK_SCENARIO_ID_CUSTOM6:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CUSTOM4:
		default:
			*feature_return_para_32 = 1; /*BINNING_AVERAGED*/
			break;
		}
		pr_debug("SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",
			*feature_return_para_32);
		*feature_para_len = 4;

		break;
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
		{
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
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.slim_video.mipi_pixel_rate;
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
		case MSDK_SCENARIO_ID_CUSTOM6:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom6.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
		}
		break;

	case SENSOR_FEATURE_GET_VC_INFO:
		pr_debug("SENSOR_FEATURE_GET_VC_INFO %d\n",
			(UINT16)*feature_data);
		pvcinfo =
		 (struct SENSOR_VC_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));
		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CUSTOM1:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[0],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[4],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[5],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM6:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[6],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_CUSTOM2:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[3],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		default:
#if imx686_flag
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[0],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
#endif
			break;
		}
		break;
	case SENSOR_FEATURE_SET_AWB_GAIN:
		/* modify to separate 3hdr and remosaic */
		if (imgsensor.sensor_mode == IMGSENSOR_MODE_CUSTOM3) {
			/*write AWB gain to sensor*/
			feedback_awbgain((UINT32)*(feature_data_32 + 1),
					(UINT32)*(feature_data_32 + 2));
		} else {
			imx686_awb_gain(
				(struct SET_SENSOR_AWB_GAIN *) feature_para);
		}
		break;
	case SENSOR_FEATURE_SET_LSC_TBL:
		{
		kal_uint8 index =
			*(((kal_uint8 *)feature_para) + (*feature_para_len));

		imx686_set_lsc_reg_setting(index, feature_data_16,
					  (*feature_para_len)/sizeof(UINT16));
		}
		break;
#ifdef imx686_seamless_test
	case SENSOR_FEATURE_SEAMLESS_SWITCH:
		{
		if ((feature_data + 1) != NULL) {
			pAeCtrls =
			(MUINT32 *)((uintptr_t)(*(feature_data + 1)));
		} else {
			pr_debug(
			"warning! no ae_ctrl input");
		}
		if (feature_data == NULL) {
			pr_info("error! input scenario is null!");
			return ERROR_INVALID_SCENARIO_ID;
		}

		if (pAeCtrls != NULL) {
			seamless_switch((*feature_data),
					*pAeCtrls, *(pAeCtrls + 1),
					*(pAeCtrls + 4), *(pAeCtrls + 5));
		} else {
			seamless_switch((*feature_data),
					0, 0, 0, 0);
		}
		}
		break;
	case SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS:
		if ((feature_data + 1) != NULL) {
			pScenarios =
			(MUINT32 *)((uintptr_t)(*(feature_data + 1)));
		} else {
			pr_info("input pScenarios vector is NULL!\n");
			return ERROR_INVALID_SCENARIO_ID;
		}
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CUSTOM5:
			*pScenarios = MSDK_SCENARIO_ID_CUSTOM6;
			break;
		case MSDK_SCENARIO_ID_CUSTOM6:
			*pScenarios = MSDK_SCENARIO_ID_CUSTOM5;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_CUSTOM1:
		case MSDK_SCENARIO_ID_CUSTOM2:
		case MSDK_SCENARIO_ID_CUSTOM7:
		case MSDK_SCENARIO_ID_CUSTOM8:
		case MSDK_SCENARIO_ID_CUSTOM9:
		case MSDK_SCENARIO_ID_CUSTOM10:
		case MSDK_SCENARIO_ID_CUSTOM11:
		case MSDK_SCENARIO_ID_CUSTOM12:
		case MSDK_SCENARIO_ID_CUSTOM13:
		case MSDK_SCENARIO_ID_CUSTOM14:
		case MSDK_SCENARIO_ID_CUSTOM15:
		default:
			*pScenarios = 0xff;
			break;
		}
		pr_debug("SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS %d %d\n",
		*feature_data, *pScenarios);
		break;
	case SENSOR_FEATURE_GET_SEAMLESS_SYSTEM_DELAY:
		{
		int i;
		int seamless_length = sizeof(seamless_sys_delays)/sizeof(struct SEAMLESS_SYS_DELAY);
		*(feature_data + 2) = 0;
		for (i = 0; i < seamless_length; i++) {
			if (*feature_data == seamless_sys_delays[i].source_scenario &&
				*(feature_data + 1) == seamless_sys_delays[i].target_scenario) {
				*(feature_data + 2) = seamless_sys_delays[i].sys_delay;
				break;
			}
		}
		}
		break;
#endif
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

UINT32 IMX686_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	sensor_func.arch = IMGSENSOR_ARCH_V2;
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	if (imgsensor.psensor_func == NULL)
		imgsensor.psensor_func = &sensor_func;
	return ERROR_NONE;
} /* IMX686_MIPI_RAW_SensorInit */
