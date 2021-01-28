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

#include "hi556mipiraw_Sensor.h"

#define PFX "hi556_camera_sensor"
#define LOG_INF(format, args...)    \
	pr_debug(PFX "[%s] " format, __func__, ##args)

#define MULTI_WRITE 1
static DEFINE_SPINLOCK(imgsensor_drv_lock);

#define per_frame 1

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = HI556_SENSOR_ID,
	.checksum_value = 0x55e2a82f,
	.pre = {
		.pclk = 176000000,
		.linelength = 2816,
		.framelength = 2049,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1296,
		.grabwindow_height = 972,

		.mipi_data_lp2hs_settle_dc = 14,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
	},
	.cap = {
		.pclk = 176000000,
		.linelength = 2816,
		.framelength = 2049,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1944,
		.mipi_data_lp2hs_settle_dc = 14,
		.max_framerate = 300,
	},
	.cap1 = {
		.pclk = 176000000,
		.linelength = 2816,
		.framelength = 4166,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1944,
		.mipi_data_lp2hs_settle_dc = 14,
		.max_framerate = 150,
	},
	.normal_video = {
		.pclk = 176000000,
		.linelength = 2816,
		.framelength = 2049,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1944,
/* MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario */
		.mipi_data_lp2hs_settle_dc = 14,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
	},
	.hs_video = {
	.pclk = 176000000,
	.linelength = 2816,
		.framelength = 520,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 640,
		.grabwindow_height = 480,
		.mipi_data_lp2hs_settle_dc = 14,//unit , ns
		.max_framerate = 1200,
	},
	.slim_video = {
		.pclk = 176000000,
		.linelength = 2816,
		.framelength = 2083,
		.startx = 0,
		.starty = 0,
	.grabwindow_width = 1280,
	.grabwindow_height = 720,
	.mipi_data_lp2hs_settle_dc = 14,//unit , ns
	.max_framerate = 300,
	},

	.margin = 6,
	.min_shutter = 6,
	.max_frame_length = 0x7FFF,
#if per_frame
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,
#else
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 1,
	.ae_ispGain_delay_frame = 2,
#endif

	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,      //1, support; 0,not support
	.ihdr_le_firstline = 0,  //1,le first ; 0, se first
	.sensor_mode_num = 5,	  //support sensor mode num

	.cap_delay_frame = 3,
	.pre_delay_frame = 3,
	.video_delay_frame = 3,
	.hs_video_delay_frame = 3,
	.slim_video_delay_frame = 3,

	//.cap_delay_frame = 2,
	//.pre_delay_frame = 2,
	//.video_delay_frame = 2,
	//enter high speed video  delay frame num
	//.hs_video_delay_frame = 2,
	//enter slim video delay frame num
	//.slim_video_delay_frame = 2,

	.isp_driving_current = ISP_DRIVING_6MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = 1,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gb,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_2_LANE,
	.i2c_addr_table = {0x51, 0x50, 0x40, 0xff},
	.i2c_speed = 400,
};

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x0100,
	.gain = 0xe0,
	.dummy_pixel = 0,
	.dummy_line = 0,
//full size current fps : 24fps for PIP, 30fps for Normal or ZSD
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_en = 0,
	.i2c_write_id = 0x40,
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
	// preview
{ 2592, 1944,    0,    0, 2592, 1944, 1296,  972,
	 0,    0, 1296,  972,    0,    0, 1296,  972},
	// capture
{ 2592, 1944,    0,    0, 2592, 1944, 2592, 1944,
	 0,    0, 2592, 1944,    0,    0, 2592, 1944},
	// video
{ 2592, 1944,    0,    0, 2592, 1944, 2592, 1944,
	 0,    0, 2592, 1944,    0,    0, 2592, 1944},
	// high speed video
{ 2592, 1944,   16,   12, 2560, 1920, 640, 480,
	 0,   0,  640,  480,    0,    0,  640,  480},
	// slim video
{ 2592, 1944,   16,  252, 2560, 1440, 1280, 720,
	 0,    0, 1280,  720,    0,    0, 1280,  720},
};

#if MULTI_WRITE
#define I2C_BUFFER_LEN 765

static kal_uint16 hi556_table_write_cmos_sensor(
					kal_uint16 *para, kal_uint32 len)
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
			puSendCmd[tosend++] = (char)(data >> 8);
			puSendCmd[tosend++] = (char)(data & 0xFF);
			IDX += 2;
			addr_last = addr;
		}

		if ((I2C_BUFFER_LEN - tosend) < 4 ||
			len == IDX ||
			addr != addr_last) {
			iBurstWriteReg_multi(puSendCmd, tosend,
				imgsensor.i2c_write_id,
				4, imgsensor_info.i2c_speed);

			tosend = 0;
		}
	}
	return 0;
}
#endif

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);

	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[4] = {(char)(addr >> 8),
		(char)(addr & 0xFF), (char)(para >> 8), (char)(para & 0xFF)};

	iWriteRegI2C(pu_send_cmd, 4, imgsensor.i2c_write_id);
}

static void write_cmos_sensor_8(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[4] = {(char)(addr >> 8),
		(char)(addr & 0xFF), (char)(para & 0xFF)};

	iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
	LOG_INF("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);
	write_cmos_sensor(0x0006, imgsensor.frame_length & 0xFFFF);
	write_cmos_sensor(0x0008, imgsensor.line_length & 0xFFFF);

}	/*	set_dummy  */

static kal_uint32 return_sensor_id(void)
{
	return ((read_cmos_sensor(0x0F16) << 8) | read_cmos_sensor(0x0F17));
}


static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length) ?
			frame_length : imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length -
		imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length -
			imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;

	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}	/*	set_max_framerate  */

static void write_shutter(kal_uint32 shutter)
{
	kal_uint16 realtime_fps = 0;

	spin_lock(&imgsensor_drv_lock);

	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);

	shutter = (shutter < imgsensor_info.min_shutter) ?
		imgsensor_info.min_shutter : shutter;
	shutter = (shutter >
		(imgsensor_info.max_frame_length - imgsensor_info.margin)) ?
		(imgsensor_info.max_frame_length - imgsensor_info.margin) :
		shutter;
	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk * 10 /
			(imgsensor.line_length * imgsensor.frame_length);
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else
			write_cmos_sensor(0x0006, imgsensor.frame_length);

	} else{
		// Extend frame length

		// ADD ODIN
		realtime_fps = imgsensor.pclk * 10 /
			(imgsensor.line_length * imgsensor.frame_length);
		if (realtime_fps > 300 && realtime_fps < 320)
			set_max_framerate(300, 0);
		// ADD END
			write_cmos_sensor(0x0006, imgsensor.frame_length);
	}

	// Update Shutter
	write_cmos_sensor_8(0x0073, ((shutter & 0xFF0000) >> 16));
	write_cmos_sensor(0x0074, shutter & 0x00FFFF);
	LOG_INF("shutter =%d, framelength =%d",
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

	LOG_INF("set_shutter");
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	write_shutter(shutter);
}	/*	set_shutter */

static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 reg_gain = 0x0000;

	reg_gain = gain / 4 - 16;

	return (kal_uint16)reg_gain;
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
	kal_uint16 reg_gain;

	/* 0x350A[0:1], 0x350B[0:7] AGC real gain */
	/* [0:3] = N meams N /16 X    */
	/* [4:9] = M meams M X         */
	/* Total gain = M + N /16 X   */

	if (gain < BASEGAIN)
		gain = BASEGAIN;
	else if (gain > 16 * BASEGAIN)
		gain = 16 * BASEGAIN;

	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

	write_cmos_sensor_8(0x0077, reg_gain);

	return gain;

}	/*	set_gain  */

#if 0
static void ihdr_write_shutter_gain(kal_uint16 le,
				kal_uint16 se, kal_uint16 gain)
{
	LOG_INF("le:0x%x, se:0x%x, gain:0x%x\n", le, se, gain);
	if (imgsensor.ihdr_en) {
		spin_lock(&imgsensor_drv_lock);
		if (le > imgsensor.min_frame_length - imgsensor_info.margin)
			imgsensor.frame_length = le + imgsensor_info.margin;
		else
			imgsensor.frame_length = imgsensor.min_frame_length;
		if (imgsensor.frame_length > imgsensor_info.max_frame_length)
			imgsensor.frame_length =
				imgsensor_info.max_frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (le < imgsensor_info.min_shutter)
			le = imgsensor_info.min_shutter;
		if (se < imgsensor_info.min_shutter)
			se = imgsensor_info.min_shutter;
		// Extend frame length first
		write_cmos_sensor(0x0006, imgsensor.frame_length);
		write_cmos_sensor(0x3502, (le << 4) & 0xFF);
		write_cmos_sensor(0x3501, (le >> 4) & 0xFF);
		write_cmos_sensor(0x3500, (le >> 12) & 0x0F);
		write_cmos_sensor(0x3508, (se << 4) & 0xFF);
		write_cmos_sensor(0x3507, (se >> 4) & 0xFF);
		write_cmos_sensor(0x3506, (se >> 12) & 0x0F);
		set_gain(gain);
	}
}
#endif


#if 0
static void set_mirror_flip(kal_uint8 image_mirror)
{
	LOG_INF("image_mirror = %d", image_mirror);

	switch (image_mirror) {
	case IMAGE_NORMAL:
		write_cmos_sensor(0x0000, 0x0000);
		break;
	case IMAGE_H_MIRROR:
		write_cmos_sensor(0x0000, 0x0100);

		break;
	case IMAGE_V_MIRROR:
		write_cmos_sensor(0x0000, 0x0200);

		break;
	case IMAGE_HV_MIRROR:
		write_cmos_sensor(0x0000, 0x0300);

		break;
	default:
		LOG_INF("Error image_mirror setting");
		break;
	}

}
#endif
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
static void night_mode(kal_bool enable)
{
/*No Need to implement this function*/
}	/*	night_mode	*/

#if MULTI_WRITE
kal_uint16 addr_data_pair_init_hi556[] = {
	0x0a00, 0x0000,
	0x0e00, 0x0102,
	0x0e02, 0x0102,
	0x0e0c, 0x0100,
	0x2000, 0x7400,
	0x2002, 0x001c,
	0x2004, 0x0242,
	0x2006, 0x0942,
	0x2008, 0x7007,
	0x200a, 0x0fd9,
	0x200c, 0x0259,
	0x200e, 0x7008,
	0x2010, 0x160e,
	0x2012, 0x0047,
	0x2014, 0x2118,
	0x2016, 0x0041,
	0x2018, 0x00d8,
	0x201a, 0x0145,
	0x201c, 0x0006,
	0x201e, 0x0181,
	0x2020, 0x13cc,
	0x2022, 0x2057,
	0x2024, 0x7001,
	0x2026, 0x0fca,
	0x2028, 0x00cb,
	0x202a, 0x009f,
	0x202c, 0x7002,
	0x202e, 0x13cc,
	0x2030, 0x019b,
	0x2032, 0x014d,
	0x2034, 0x2987,
	0x2036, 0x2766,
	0x2038, 0x0020,
	0x203a, 0x2060,
	0x203c, 0x0e5d,
	0x203e, 0x181d,
	0x2040, 0x2066,
	0x2042, 0x20c4,
	0x2044, 0x5000,
	0x2046, 0x0005,
	0x2048, 0x0000,
	0x204a, 0x01db,
	0x204c, 0x025a,
	0x204e, 0x00c0,
	0x2050, 0x0005,
	0x2052, 0x0006,
	0x2054, 0x0ad9,
	0x2056, 0x0259,
	0x2058, 0x0618,
	0x205a, 0x0258,
	0x205c, 0x2266,
	0x205e, 0x20c8,
	0x2060, 0x2060,
	0x2062, 0x707b,
	0x2064, 0x0fdd,
	0x2066, 0x81b8,
	0x2068, 0x5040,
	0x206a, 0x0020,
	0x206c, 0x5060,
	0x206e, 0x3143,
	0x2070, 0x5081,
	0x2072, 0x025c,
	0x2074, 0x7800,
	0x2076, 0x7400,
	0x2078, 0x001c,
	0x207a, 0x0242,
	0x207c, 0x0942,
	0x207e, 0x0bd9,
	0x2080, 0x0259,
	0x2082, 0x7008,
	0x2084, 0x160e,
	0x2086, 0x0047,
	0x2088, 0x2118,
	0x208a, 0x0041,
	0x208c, 0x00d8,
	0x208e, 0x0145,
	0x2090, 0x0006,
	0x2092, 0x0181,
	0x2094, 0x13cc,
	0x2096, 0x2057,
	0x2098, 0x7001,
	0x209a, 0x0fca,
	0x209c, 0x00cb,
	0x209e, 0x009f,
	0x20a0, 0x7002,
	0x20a2, 0x13cc,
	0x20a4, 0x019b,
	0x20a6, 0x014d,
	0x20a8, 0x2987,
	0x20aa, 0x2766,
	0x20ac, 0x0020,
	0x20ae, 0x2060,
	0x20b0, 0x0e5d,
	0x20b2, 0x181d,
	0x20b4, 0x2066,
	0x20b6, 0x20c4,
	0x20b8, 0x50a0,
	0x20ba, 0x0005,
	0x20bc, 0x0000,
	0x20be, 0x01db,
	0x20c0, 0x025a,
	0x20c2, 0x00c0,
	0x20c4, 0x0005,
	0x20c6, 0x0006,
	0x20c8, 0x0ad9,
	0x20ca, 0x0259,
	0x20cc, 0x0618,
	0x20ce, 0x0258,
	0x20d0, 0x2266,
	0x20d2, 0x20c8,
	0x20d4, 0x2060,
	0x20d6, 0x707b,
	0x20d8, 0x0fdd,
	0x20da, 0x86b8,
	0x20dc, 0x50e0,
	0x20de, 0x0020,
	0x20e0, 0x5100,
	0x20e2, 0x3143,
	0x20e4, 0x5121,
	0x20e6, 0x7800,
	0x20e8, 0x3140,
	0x20ea, 0x01c4,
	0x20ec, 0x01c1,
	0x20ee, 0x01c0,
	0x20f0, 0x01c4,
	0x20f2, 0x2700,
	0x20f4, 0x3d40,
	0x20f6, 0x7800,
	0x20f8, 0xffff,
	0x27fe, 0xe000,
	0x3000, 0x60f8,
	0x3002, 0x187f,
	0x3004, 0x7060,
	0x3006, 0x0114,
	0x3008, 0x60b0,
	0x300a, 0x1473,
	0x300c, 0x0013,
	0x300e, 0x140f,
	0x3010, 0x0040,
	0x3012, 0x100f,
	0x3014, 0x60f8,
	0x3016, 0x187f,
	0x3018, 0x7060,
	0x301a, 0x0114,
	0x301c, 0x60b0,
	0x301e, 0x1473,
	0x3020, 0x0013,
	0x3022, 0x140f,
	0x3024, 0x0040,
	0x3026, 0x000f,
	0x0b00, 0x0000,
	0x0b02, 0x0045,
	0x0b04, 0xb405,
	0x0b06, 0xc403,
	0x0b08, 0x0081,
	0x0b0a, 0x8252,
	0x0b0c, 0xf814,
	0x0b0e, 0xc618,
	0x0b10, 0xa828,
	0x0b12, 0x004c,
	0x0b14, 0x4068,
	0x0b16, 0x0000,
	0x0f30, 0x6e25,
	0x0f32, 0x7067,
	0x0954, 0x0009,
	0x0956, 0x1100,
	0x0958, 0xcc80,
	0x095a, 0x0000,
	0x0c00, 0x1110,
	0x0c02, 0x0011,
	0x0c04, 0x0000,
	0x0c06, 0x0200,
	0x0c10, 0x0040,
	0x0c12, 0x0040,
	0x0c14, 0x0040,
	0x0c16, 0x0040,
	0x0a10, 0x4000,
	0x3068, 0xf800,
	0x306a, 0xf876,
	0x006c, 0x0000,
	0x005e, 0x0200,
	0x000e, 0x0100,
	0x0e0a, 0x0001,
	0x004a, 0x0100,
	0x004c, 0x0000,
	0x000c, 0x0022,
	0x0008, 0x0b00,
	0x005a, 0x0202,
	0x0012, 0x000e,
	0x0018, 0x0a31,
	0x0022, 0x0008,
	0x0028, 0x0017,
	0x0024, 0x0028,
	0x002a, 0x002d,
	0x0026, 0x0030,
	0x002c, 0x07c7,
	0x002e, 0x1111,
	0x0030, 0x1111,
	0x0032, 0x1111,
	0x0006, 0x07bc,
	0x0a22, 0x0000,
	0x0a12, 0x0a20,
	0x0a14, 0x0798,
	0x003e, 0x0000,
	0x0074, 0x080e,
	0x0070, 0x0407,
	0x0002, 0x0000,
	0x0a02, 0x0100,
	0x0a24, 0x0100,
	0x0046, 0x0000,
	0x0076, 0x0000,
	0x0060, 0x0000,
	0x0062, 0x0530,
	0x0064, 0x0500,
	0x0066, 0x0530,
	0x0068, 0x0500,
	0x0122, 0x0300,
	0x015a, 0xff08,
	0x0804, 0x0200,
	0x005c, 0x0102,
	0x0a1a, 0x0800,
	0x004e, 0x0100
};
#endif

static void sensor_init(void)
{
#if MULTI_WRITE
	hi556_table_write_cmos_sensor(
		addr_data_pair_init_hi556,
		sizeof(addr_data_pair_init_hi556) /
		sizeof(kal_uint16));
#else
	write_cmos_sensor(0x0a00, 0x0000); // stream off
	write_cmos_sensor(0x0e00, 0x0102);
	write_cmos_sensor(0x0e02, 0x0102);
	write_cmos_sensor(0x0e0c, 0x0100);
	write_cmos_sensor(0x2000, 0x7400);
	write_cmos_sensor(0x2002, 0x001c);
	write_cmos_sensor(0x2004, 0x0242);
	write_cmos_sensor(0x2006, 0x0942);
	write_cmos_sensor(0x2008, 0x7007);
	write_cmos_sensor(0x200a, 0x0fd9);
	write_cmos_sensor(0x200c, 0x0259);
	write_cmos_sensor(0x200e, 0x7008);
	write_cmos_sensor(0x2010, 0x160e);
	write_cmos_sensor(0x2012, 0x0047);
	write_cmos_sensor(0x2014, 0x2118);
	write_cmos_sensor(0x2016, 0x0041);
	write_cmos_sensor(0x2018, 0x00d8);
	write_cmos_sensor(0x201a, 0x0145);
	write_cmos_sensor(0x201c, 0x0006);
	write_cmos_sensor(0x201e, 0x0181);
	write_cmos_sensor(0x2020, 0x13cc);
	write_cmos_sensor(0x2022, 0x2057);
	write_cmos_sensor(0x2024, 0x7001);
	write_cmos_sensor(0x2026, 0x0fca);
	write_cmos_sensor(0x2028, 0x00cb);
	write_cmos_sensor(0x202a, 0x009f);
	write_cmos_sensor(0x202c, 0x7002);
	write_cmos_sensor(0x202e, 0x13cc);
	write_cmos_sensor(0x2030, 0x019b);
	write_cmos_sensor(0x2032, 0x014d);
	write_cmos_sensor(0x2034, 0x2987);
	write_cmos_sensor(0x2036, 0x2766);
	write_cmos_sensor(0x2038, 0x0020);
	write_cmos_sensor(0x203a, 0x2060);
	write_cmos_sensor(0x203c, 0x0e5d);
	write_cmos_sensor(0x203e, 0x181d);
	write_cmos_sensor(0x2040, 0x2066);
	write_cmos_sensor(0x2042, 0x20c4);
	write_cmos_sensor(0x2044, 0x5000);
	write_cmos_sensor(0x2046, 0x0005);
	write_cmos_sensor(0x2048, 0x0000);
	write_cmos_sensor(0x204a, 0x01db);
	write_cmos_sensor(0x204c, 0x025a);
	write_cmos_sensor(0x204e, 0x00c0);
	write_cmos_sensor(0x2050, 0x0005);
	write_cmos_sensor(0x2052, 0x0006);
	write_cmos_sensor(0x2054, 0x0ad9);
	write_cmos_sensor(0x2056, 0x0259);
	write_cmos_sensor(0x2058, 0x0618);
	write_cmos_sensor(0x205a, 0x0258);
	write_cmos_sensor(0x205c, 0x2266);
	write_cmos_sensor(0x205e, 0x20c8);
	write_cmos_sensor(0x2060, 0x2060);
	write_cmos_sensor(0x2062, 0x707b);
	write_cmos_sensor(0x2064, 0x0fdd);
	write_cmos_sensor(0x2066, 0x81b8);
	write_cmos_sensor(0x2068, 0x5040);
	write_cmos_sensor(0x206a, 0x0020);
	write_cmos_sensor(0x206c, 0x5060);
	write_cmos_sensor(0x206e, 0x3143);
	write_cmos_sensor(0x2070, 0x5081);
	write_cmos_sensor(0x2072, 0x025c);
	write_cmos_sensor(0x2074, 0x7800);
	write_cmos_sensor(0x2076, 0x7400);
	write_cmos_sensor(0x2078, 0x001c);
	write_cmos_sensor(0x207a, 0x0242);
	write_cmos_sensor(0x207c, 0x0942);
	write_cmos_sensor(0x207e, 0x0bd9);
	write_cmos_sensor(0x2080, 0x0259);
	write_cmos_sensor(0x2082, 0x7008);
	write_cmos_sensor(0x2084, 0x160e);
	write_cmos_sensor(0x2086, 0x0047);
	write_cmos_sensor(0x2088, 0x2118);
	write_cmos_sensor(0x208a, 0x0041);
	write_cmos_sensor(0x208c, 0x00d8);
	write_cmos_sensor(0x208e, 0x0145);
	write_cmos_sensor(0x2090, 0x0006);
	write_cmos_sensor(0x2092, 0x0181);
	write_cmos_sensor(0x2094, 0x13cc);
	write_cmos_sensor(0x2096, 0x2057);
	write_cmos_sensor(0x2098, 0x7001);
	write_cmos_sensor(0x209a, 0x0fca);
	write_cmos_sensor(0x209c, 0x00cb);
	write_cmos_sensor(0x209e, 0x009f);
	write_cmos_sensor(0x20a0, 0x7002);
	write_cmos_sensor(0x20a2, 0x13cc);
	write_cmos_sensor(0x20a4, 0x019b);
	write_cmos_sensor(0x20a6, 0x014d);
	write_cmos_sensor(0x20a8, 0x2987);
	write_cmos_sensor(0x20aa, 0x2766);
	write_cmos_sensor(0x20ac, 0x0020);
	write_cmos_sensor(0x20ae, 0x2060);
	write_cmos_sensor(0x20b0, 0x0e5d);
	write_cmos_sensor(0x20b2, 0x181d);
	write_cmos_sensor(0x20b4, 0x2066);
	write_cmos_sensor(0x20b6, 0x20c4);
	write_cmos_sensor(0x20b8, 0x50a0);
	write_cmos_sensor(0x20ba, 0x0005);
	write_cmos_sensor(0x20bc, 0x0000);
	write_cmos_sensor(0x20be, 0x01db);
	write_cmos_sensor(0x20c0, 0x025a);
	write_cmos_sensor(0x20c2, 0x00c0);
	write_cmos_sensor(0x20c4, 0x0005);
	write_cmos_sensor(0x20c6, 0x0006);
	write_cmos_sensor(0x20c8, 0x0ad9);
	write_cmos_sensor(0x20ca, 0x0259);
	write_cmos_sensor(0x20cc, 0x0618);
	write_cmos_sensor(0x20ce, 0x0258);
	write_cmos_sensor(0x20d0, 0x2266);
	write_cmos_sensor(0x20d2, 0x20c8);
	write_cmos_sensor(0x20d4, 0x2060);
	write_cmos_sensor(0x20d6, 0x707b);
	write_cmos_sensor(0x20d8, 0x0fdd);
	write_cmos_sensor(0x20da, 0x86b8);
	write_cmos_sensor(0x20dc, 0x50e0);
	write_cmos_sensor(0x20de, 0x0020);
	write_cmos_sensor(0x20e0, 0x5100);
	write_cmos_sensor(0x20e2, 0x3143);
	write_cmos_sensor(0x20e4, 0x5121);
	write_cmos_sensor(0x20e6, 0x7800);
	write_cmos_sensor(0x20e8, 0x3140);
	write_cmos_sensor(0x20ea, 0x01c4);
	write_cmos_sensor(0x20ec, 0x01c1);
	write_cmos_sensor(0x20ee, 0x01c0);
	write_cmos_sensor(0x20f0, 0x01c4);
	write_cmos_sensor(0x20f2, 0x2700);
	write_cmos_sensor(0x20f4, 0x3d40);
	write_cmos_sensor(0x20f6, 0x7800);
	write_cmos_sensor(0x20f8, 0xffff);
	write_cmos_sensor(0x27fe, 0xe000);
	write_cmos_sensor(0x3000, 0x60f8);
	write_cmos_sensor(0x3002, 0x187f);
	write_cmos_sensor(0x3004, 0x7060);
	write_cmos_sensor(0x3006, 0x0114);
	write_cmos_sensor(0x3008, 0x60b0);
	write_cmos_sensor(0x300a, 0x1473);
	write_cmos_sensor(0x300c, 0x0013);
	write_cmos_sensor(0x300e, 0x140f);
	write_cmos_sensor(0x3010, 0x0040);
	write_cmos_sensor(0x3012, 0x100f);
	write_cmos_sensor(0x3014, 0x60f8);
	write_cmos_sensor(0x3016, 0x187f);
	write_cmos_sensor(0x3018, 0x7060);
	write_cmos_sensor(0x301a, 0x0114);
	write_cmos_sensor(0x301c, 0x60b0);
	write_cmos_sensor(0x301e, 0x1473);
	write_cmos_sensor(0x3020, 0x0013);
	write_cmos_sensor(0x3022, 0x140f);
	write_cmos_sensor(0x3024, 0x0040);
	write_cmos_sensor(0x3026, 0x000f);
	write_cmos_sensor(0x0b00, 0x0000);
	write_cmos_sensor(0x0b02, 0x0045);
	write_cmos_sensor(0x0b04, 0xb405);
	write_cmos_sensor(0x0b06, 0xc403);
	write_cmos_sensor(0x0b08, 0x0081);
	write_cmos_sensor(0x0b0a, 0x8252);
	write_cmos_sensor(0x0b0c, 0xf814);
	write_cmos_sensor(0x0b0e, 0xc618);
	write_cmos_sensor(0x0b10, 0xa828);
	write_cmos_sensor(0x0b12, 0x004c);
	write_cmos_sensor(0x0b14, 0x4068);
	write_cmos_sensor(0x0b16, 0x0000);
	write_cmos_sensor(0x0f30, 0x6e25);
	write_cmos_sensor(0x0f32, 0x7067);
	write_cmos_sensor(0x0954, 0x0009);
	write_cmos_sensor(0x0956, 0x1100);
	write_cmos_sensor(0x0958, 0xcc80);
	write_cmos_sensor(0x095a, 0x0000);
	write_cmos_sensor(0x0c00, 0x1110);
	write_cmos_sensor(0x0c02, 0x0011);
	write_cmos_sensor(0x0c04, 0x0000);
	write_cmos_sensor(0x0c06, 0x0200);
	write_cmos_sensor(0x0c10, 0x0040);
	write_cmos_sensor(0x0c12, 0x0040);
	write_cmos_sensor(0x0c14, 0x0040);
	write_cmos_sensor(0x0c16, 0x0040);
	write_cmos_sensor(0x0a10, 0x4000);
	write_cmos_sensor(0x3068, 0xf800);
	write_cmos_sensor(0x306a, 0xf876);
	write_cmos_sensor(0x006c, 0x0000);
	write_cmos_sensor(0x005e, 0x0200);
	write_cmos_sensor(0x000e, 0x0100); //org
	//write_cmos_sensor(0x000e, 0x0000); //odin
	write_cmos_sensor(0x0e0a, 0x0001);
	write_cmos_sensor(0x004a, 0x0100);
	write_cmos_sensor(0x004c, 0x0000);
	write_cmos_sensor(0x000c, 0x0022);
	write_cmos_sensor(0x0008, 0x0b00);
	write_cmos_sensor(0x005a, 0x0202);
	write_cmos_sensor(0x0012, 0x000e);
	write_cmos_sensor(0x0018, 0x0a31);
	write_cmos_sensor(0x0022, 0x0008);
	write_cmos_sensor(0x0028, 0x0017);
	write_cmos_sensor(0x0024, 0x0028);
	write_cmos_sensor(0x002a, 0x002d);
	write_cmos_sensor(0x0026, 0x0030);
	write_cmos_sensor(0x002c, 0x07c7);
	write_cmos_sensor(0x002e, 0x1111);
	write_cmos_sensor(0x0030, 0x1111);
	write_cmos_sensor(0x0032, 0x1111);
	write_cmos_sensor(0x0006, 0x07bc);
	write_cmos_sensor(0x0a22, 0x0000);
	write_cmos_sensor(0x0a12, 0x0a20);
	write_cmos_sensor(0x0a14, 0x0798);
	write_cmos_sensor(0x003e, 0x0000);
	write_cmos_sensor(0x0074, 0x080e);
	write_cmos_sensor(0x0070, 0x0407);
	write_cmos_sensor(0x0002, 0x0000);
	write_cmos_sensor(0x0a02, 0x0100);
	write_cmos_sensor(0x0a24, 0x0100);
	write_cmos_sensor(0x0046, 0x0000);
	write_cmos_sensor(0x0076, 0x0000);
	write_cmos_sensor(0x0060, 0x0000);
	write_cmos_sensor(0x0062, 0x0530);
	write_cmos_sensor(0x0064, 0x0500);
	write_cmos_sensor(0x0066, 0x0530);
	write_cmos_sensor(0x0068, 0x0500);
	write_cmos_sensor(0x0122, 0x0300);
	write_cmos_sensor(0x015a, 0xff08);
	write_cmos_sensor(0x0804, 0x0200);
	write_cmos_sensor(0x005c, 0x0102);
	write_cmos_sensor(0x0a1a, 0x0800);
	write_cmos_sensor(0x004e, 0x0100);
#endif
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_preview_hi556[] = {
	0x0b0a, 0x8259,
	0x0f30, 0x6e25,
	0x0f32, 0x7167,
	0x004a, 0x0100,
	0x004c, 0x0000,
	0x004e, 0x0100,
	0x000c, 0x0122,
	0x0008, 0x0b00,
	0x005a, 0x0404,
	0x0012, 0x000c,
	0x0018, 0x0a33,
	0x0022, 0x0008,
	0x0028, 0x0017,
	0x0024, 0x0022,
	0x002a, 0x002b,
	0x0026, 0x0030,
	0x002c, 0x07c7,
	0x002e, 0x3311,
	0x0030, 0x3311,
	0x0032, 0x3311,
	0x0006, 0x0801,
	0x0a22, 0x0000,
	0x0a12, 0x0510,
	0x0a14, 0x03cc,
	0x003e, 0x0000,
	0x0074, 0x07ff,
	0x0070, 0x0411,
	0x0804, 0x0200,
	0x0a04, 0x016a,
	0x090e, 0x0010,
	0x090c, 0x09c0,
	0x0902, 0x4319,
	0x0914, 0xc106,
	0x0916, 0x040e,
	0x0918, 0x0304,
	0x091a, 0x0709,
	0x091c, 0x0e06,
	0x091e, 0x0300
};
#endif

static void preview_setting(void)
{
#if MULTI_WRITE
	hi556_table_write_cmos_sensor(
		addr_data_pair_preview_hi556,
		sizeof(addr_data_pair_preview_hi556) /
		sizeof(kal_uint16));
#else
	write_cmos_sensor(0x0b0a, 0x8259);
	write_cmos_sensor(0x0f30, 0x6e25);
	write_cmos_sensor(0x0f32, 0x7167);
	write_cmos_sensor(0x004a, 0x0100);
	write_cmos_sensor(0x004c, 0x0000);
	write_cmos_sensor(0x004e, 0x0100); //perframe enable
	write_cmos_sensor(0x000c, 0x0122);
	write_cmos_sensor(0x0008, 0x0b00);
	write_cmos_sensor(0x005a, 0x0404);
	write_cmos_sensor(0x0012, 0x000c);
	write_cmos_sensor(0x0018, 0x0a33);
	write_cmos_sensor(0x0022, 0x0008);
	write_cmos_sensor(0x0028, 0x0017);
	write_cmos_sensor(0x0024, 0x0022);
	write_cmos_sensor(0x002a, 0x002b);
	write_cmos_sensor(0x0026, 0x0030);
	write_cmos_sensor(0x002c, 0x07c7);
	write_cmos_sensor(0x002e, 0x3311);
	write_cmos_sensor(0x0030, 0x3311);
	write_cmos_sensor(0x0032, 0x3311);
	write_cmos_sensor(0x0006, 0x0801);
	write_cmos_sensor(0x0a22, 0x0000);
	write_cmos_sensor(0x0a12, 0x0510);
	write_cmos_sensor(0x0a14, 0x03cc);
	write_cmos_sensor(0x003e, 0x0000);
	write_cmos_sensor(0x0074, 0x07ff);
	write_cmos_sensor(0x0070, 0x0411);
	write_cmos_sensor(0x0804, 0x0200);
	write_cmos_sensor(0x0a04, 0x016a);
	write_cmos_sensor(0x090e, 0x0010);
	write_cmos_sensor(0x090c, 0x09c0);
//===============================================
//             mipi 2 lane 440Mbps
//===============================================
	write_cmos_sensor(0x0902, 0x4319);
	write_cmos_sensor(0x0914, 0xc106);
	write_cmos_sensor(0x0916, 0x040e);
	write_cmos_sensor(0x0918, 0x0304);
	write_cmos_sensor(0x091a, 0x0709);
	write_cmos_sensor(0x091c, 0x0e06);
	write_cmos_sensor(0x091e, 0x0300);
#endif
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_capture_fps_hi556[] = {
	0x0b0a, 0x8252,
	0x0f30, 0x6e25,
	0x0f32, 0x7067,
	0x004a, 0x0100,
	0x004c, 0x0000,
	0x004e, 0x0100,
	0x000c, 0x0022,
	0x0008, 0x0b00,
	0x005a, 0x0202,
	0x0012, 0x000e,
	0x0018, 0x0a31,
	0x0022, 0x0008,
	0x0028, 0x0017,
	0x0024, 0x0028,
	0x002a, 0x002d,
	0x0026, 0x0030,
	0x002c, 0x07c7,
	0x002e, 0x1111,
	0x0030, 0x1111,
	0x0032, 0x1111,
	0x0006, 0x1046,
	0x0a22, 0x0000,
	0x0a12, 0x0a20,
	0x0a14, 0x0798,
	0x003e, 0x0000,
	0x0074, 0x1044,
	0x0070, 0x0411,
	0x0804, 0x0200,
	0x0a04, 0x014a,
	0x090c, 0x0fdc,
	0x090e, 0x002d,
	0x0902, 0x4319,
	0x0914, 0xc10a,
	0x0916, 0x071f,
	0x0918, 0x0408,
	0x091a, 0x0c0d,
	0x091c, 0x0f09,
	0x091e, 0x0a00
};

kal_uint16 addr_data_pair_capture_30fps_hi556[] = {
	0x0b0a, 0x8252,
	0x0f30, 0x6e25,
	0x0f32, 0x7067,
	0x004a, 0x0100,
	0x004c, 0x0000,
	0x004e, 0x0100,
	0x000c, 0x0022,
	0x0008, 0x0b00,
	0x005a, 0x0202,
	0x0012, 0x000e,
	0x0018, 0x0a31,
	0x0022, 0x0008,
	0x0028, 0x0017,
	0x0024, 0x0028,
	0x002a, 0x002d,
	0x0026, 0x0030,
	0x002c, 0x07c7,
	0x002e, 0x1111,
	0x0030, 0x1111,
	0x0032, 0x1111,
	0x0006, 0x0801,
	0x0a22, 0x0000,
	0x0a12, 0x0a20,
	0x0a14, 0x0798,
	0x003e, 0x0000,
	0x0074, 0x07ff,
	0x0070, 0x0411,
	0x0804, 0x0200,
	0x0a04, 0x014a,
	0x090c, 0x0fdc,
	0x090e, 0x002d,
	0x0902, 0x4319,
	0x0914, 0xc10a,
	0x0916, 0x071f,
	0x0918, 0x0408,
	0x091a, 0x0c0d,
	0x091c, 0x0f09,
	0x091e, 0x0a00
};
#endif


static void capture_setting(kal_uint16 currefps)
{
#if MULTI_WRITE
	if (currefps == 300) {
		hi556_table_write_cmos_sensor(
			addr_data_pair_capture_30fps_hi556,
			sizeof(addr_data_pair_capture_30fps_hi556) /
			sizeof(kal_uint16));
	} else {
		hi556_table_write_cmos_sensor(
			addr_data_pair_capture_fps_hi556,
			sizeof(addr_data_pair_capture_fps_hi556) /
			sizeof(kal_uint16));
	}
#else
	if (currefps == 300) {
		LOG_INF("%s fps = 300\n", __func__);
		write_cmos_sensor(0x0b0a, 0x8252);
		write_cmos_sensor(0x0f30, 0x6e25);
		write_cmos_sensor(0x0f32, 0x7067);
		write_cmos_sensor(0x004a, 0x0100);
		write_cmos_sensor(0x004c, 0x0000);
		write_cmos_sensor(0x004e, 0x0100); //perframe enable
		write_cmos_sensor(0x000c, 0x0022);
		write_cmos_sensor(0x0008, 0x0b00);
		write_cmos_sensor(0x005a, 0x0202);
		write_cmos_sensor(0x0012, 0x000e);
		write_cmos_sensor(0x0018, 0x0a31);
		write_cmos_sensor(0x0022, 0x0008);
		write_cmos_sensor(0x0028, 0x0017);
		write_cmos_sensor(0x0024, 0x0028);
		write_cmos_sensor(0x002a, 0x002d);
		write_cmos_sensor(0x0026, 0x0030);
		write_cmos_sensor(0x002c, 0x07c7);
		write_cmos_sensor(0x002e, 0x1111);
		write_cmos_sensor(0x0030, 0x1111);
		write_cmos_sensor(0x0032, 0x1111);
		write_cmos_sensor(0x0006, 0x0801);
		write_cmos_sensor(0x0a22, 0x0000);
		write_cmos_sensor(0x0a12, 0x0a20);
		write_cmos_sensor(0x0a14, 0x0798);
		write_cmos_sensor(0x003e, 0x0000);
		write_cmos_sensor(0x0074, 0x07ff);
		write_cmos_sensor(0x0070, 0x0411);
		write_cmos_sensor(0x0804, 0x0200);
		write_cmos_sensor(0x0a04, 0x014a);
		write_cmos_sensor(0x090c, 0x0fdc);
		write_cmos_sensor(0x090e, 0x002d);
//===============================================
//             mipi 2 lane 880Mbps
//===============================================
		write_cmos_sensor(0x0902, 0x4319);
		write_cmos_sensor(0x0914, 0xc10a);
		write_cmos_sensor(0x0916, 0x071f);
		write_cmos_sensor(0x0918, 0x0408);
		write_cmos_sensor(0x091a, 0x0c0d);
		write_cmos_sensor(0x091c, 0x0f09);
		write_cmos_sensor(0x091e, 0x0a00);
	} else	{
		LOG_INF("%s fps not 300\n", __func__);
		//Sensor Information////////////////////////////
		//Sensor	  : Hi-556
		//Date		  : 2016-10-19
		//Customer        : MTK_validation
		//Image size	  : 2592x1944
		//MCLK		  : 24MHz
		//MIPI speed(Mbps): 880Mbps x 2Lane
		//Frame Length	  : 4166
		//Line Length	  : 2816
		//Max Fps	  : 15.0fps
		//Pixel order	  : Green 1st (=GB)
		//X/Y-flip	  : X-flip
		//BLC offset	  : 64code
		////////////////////////////////////////////////
		write_cmos_sensor(0x0b0a, 0x8252);
		write_cmos_sensor(0x0f30, 0x6e25);
		write_cmos_sensor(0x0f32, 0x7067);
		write_cmos_sensor(0x004a, 0x0100);
		write_cmos_sensor(0x004c, 0x0000);
		write_cmos_sensor(0x004e, 0x0100); //perframe enable
		write_cmos_sensor(0x000c, 0x0022);
		write_cmos_sensor(0x0008, 0x0b00);
		write_cmos_sensor(0x005a, 0x0202);
		write_cmos_sensor(0x0012, 0x000e);
		write_cmos_sensor(0x0018, 0x0a31);
		write_cmos_sensor(0x0022, 0x0008);
		write_cmos_sensor(0x0028, 0x0017);
		write_cmos_sensor(0x0024, 0x0028);
		write_cmos_sensor(0x002a, 0x002d);
		write_cmos_sensor(0x0026, 0x0030);
		write_cmos_sensor(0x002c, 0x07c7);
		write_cmos_sensor(0x002e, 0x1111);
		write_cmos_sensor(0x0030, 0x1111);
		write_cmos_sensor(0x0032, 0x1111);
		write_cmos_sensor(0x0006, 0x1046);
		write_cmos_sensor(0x0a22, 0x0000);
		write_cmos_sensor(0x0a12, 0x0a20);
		write_cmos_sensor(0x0a14, 0x0798);
		write_cmos_sensor(0x003e, 0x0000);
		write_cmos_sensor(0x0074, 0x1044);
		write_cmos_sensor(0x0070, 0x0411);
		write_cmos_sensor(0x0804, 0x0200);
		write_cmos_sensor(0x0a04, 0x014a);
		write_cmos_sensor(0x090c, 0x0fdc);
		write_cmos_sensor(0x090e, 0x002d);
		//===============================================
		//             mipi 2 lane 880Mbps
		//===============================================
		write_cmos_sensor(0x0902, 0x4319);
		write_cmos_sensor(0x0914, 0xc10a);
		write_cmos_sensor(0x0916, 0x071f);
		write_cmos_sensor(0x0918, 0x0408);
		write_cmos_sensor(0x091a, 0x0c0d);
		write_cmos_sensor(0x091c, 0x0f09);
		write_cmos_sensor(0x091e, 0x0a00);
	}
#endif
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_hs_video_hi556[] = {
	0x0b0a, 0x8252,
	0x0f30, 0x6e25,
	0x0f32, 0x7267,
	0x004a, 0x0100,
	0x004c, 0x0000,
	0x004e, 0x0100,
	0x000c, 0x0022,
	0x0008, 0x0b00,
	0x005a, 0x0208,
	0x0012, 0x0018,
	0x0018, 0x0a27,
	0x0022, 0x0008,
	0x0028, 0x0017,
	0x0024, 0x002e,
	0x002a, 0x0033,
	0x0026, 0x003c,
	0x002c, 0x07bb,
	0x002e, 0x1111,
	0x0030, 0x1111,
	0x0032, 0x7711,
	0x0006, 0x0208,
	0x0a22, 0x0100,
	0x0a12, 0x0280,
	0x0a14, 0x01e0,
	0x003e, 0x0000,
	0x0074, 0x0206,
	0x0070, 0x0411,
	0x0804, 0x0200,
	0x0a04, 0x016a,
	0x090c, 0x0270,
	0x090e, 0x000c,
	0x0902, 0x4319,
	0x0914, 0xc103,
	0x0916, 0x0207,
	0x0918, 0x0302,
	0x091a, 0x0406,
	0x091c, 0x0903,
	0x091e, 0x0300
};
#endif

static void hs_video_setting(void)
{
	//Sensor Information////////////////////////////
	//Sensor	  : hi-556
	//Date		  : 2016-10-19
	//Customer        : MTK_validation
	//Image size	  : 640x480
	//MCLK		  : 24MHz
	//MIPI speed(Mbps): 220Mbps x 2Lane
	//Frame Length	  : 520
	//Line Length	  : 2816
	//Max Fps	  : 120.19fps
	//Pixel order	  : Green 1st (=GB)
	//X/Y-flip	  : X-flip
	//BLC offset	  : 64code
	////////////////////////////////////////////////
#if MULTI_WRITE
	hi556_table_write_cmos_sensor(
		addr_data_pair_hs_video_hi556,
		sizeof(addr_data_pair_hs_video_hi556) /
		sizeof(kal_uint16));
#else
	write_cmos_sensor(0x0b0a, 0x8252);
	write_cmos_sensor(0x0f30, 0x6e25);
	write_cmos_sensor(0x0f32, 0x7267);
	write_cmos_sensor(0x004a, 0x0100);
	write_cmos_sensor(0x004c, 0x0000);
	write_cmos_sensor(0x004e, 0x0100); //perframe enable
	write_cmos_sensor(0x000c, 0x0022);
	write_cmos_sensor(0x0008, 0x0b00);
	write_cmos_sensor(0x005a, 0x0208);
	write_cmos_sensor(0x0012, 0x0018);
	write_cmos_sensor(0x0018, 0x0a27);
	write_cmos_sensor(0x0022, 0x0008);
	write_cmos_sensor(0x0028, 0x0017);
	write_cmos_sensor(0x0024, 0x002e);
	write_cmos_sensor(0x002a, 0x0033);
	write_cmos_sensor(0x0026, 0x003c);
	write_cmos_sensor(0x002c, 0x07bb);
	write_cmos_sensor(0x002e, 0x1111);
	write_cmos_sensor(0x0030, 0x1111);
	write_cmos_sensor(0x0032, 0x7711);
	write_cmos_sensor(0x0006, 0x0208);
	write_cmos_sensor(0x0a22, 0x0100);
	write_cmos_sensor(0x0a12, 0x0280);
	write_cmos_sensor(0x0a14, 0x01e0);
	write_cmos_sensor(0x003e, 0x0000);
	write_cmos_sensor(0x0074, 0x0206);
	write_cmos_sensor(0x0070, 0x0411);
	write_cmos_sensor(0x0804, 0x0200);
	write_cmos_sensor(0x0a04, 0x016a);
	write_cmos_sensor(0x090c, 0x0270);
	write_cmos_sensor(0x090e, 0x000c);
	//===============================================
	//             mipi 2 lane 220Mbps
	//===============================================
	write_cmos_sensor(0x0902, 0x4319);
	write_cmos_sensor(0x0914, 0xc103);
	write_cmos_sensor(0x0916, 0x0207);
	write_cmos_sensor(0x0918, 0x0302);
	write_cmos_sensor(0x091a, 0x0406);
	write_cmos_sensor(0x091c, 0x0903);
	write_cmos_sensor(0x091e, 0x0300);
#endif
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_slim_video_hi556[] = {
	0x0b0a, 0x8252,
	0x0f30, 0x6e25,
	0x0f32, 0x7167,
	0x004a, 0x0100,
	0x004c, 0x0000,
	0x004e, 0x0100,
	0x000c, 0x0022,
	0x0008, 0x0b00,
	0x005a, 0x0204,
	0x0012, 0x001c,
	0x0018, 0x0a23,
	0x0022, 0x0008,
	0x0028, 0x0017,
	0x0024, 0x0122,
	0x002a, 0x0127,
	0x0026, 0x012c,
	0x002c, 0x06cb,
	0x002e, 0x1111,
	0x0030, 0x1111,
	0x0032, 0x3311,
	0x0006, 0x0823,
	0x0a22, 0x0000,
	0x0a12, 0x0500,
	0x0a14, 0x02d0,
	0x003e, 0x0000,
	0x0074, 0x0821,
	0x0070, 0x0411,
	0x0804, 0x0200,
	0x0a04, 0x016a,
	0x090e, 0x0010,
	0x090c, 0x09c0,
	0x0902, 0x4319,
	0x0914, 0xc106,
	0x0916, 0x040e,
	0x0918, 0x0304,
	0x091c, 0x0e06,
	0x091a, 0x0709,
	0x091e, 0x0300
};
#endif


static void slim_video_setting(void)
{
	//Sensor Information////////////////////////////
	//Sensor	  : hi-556
	//Date		  : 2016-10-19
	//Customer        : MTK_validation
	//Image size	  : 1280x720
	//MCLK		  : 24MHz
	//MIPI speed(Mbps): 440Mbps x 2Lane
	//Frame Length	  : 2083
	//Line Length	  : 2816
	//Max Fps	  : 30.0fps
	//Pixel order	  : Green 1st (=GB)
	//X/Y-flip	  : X-flip
	//BLC offset	  : 64code
	////////////////////////////////////////////////
#if MULTI_WRITE
	hi556_table_write_cmos_sensor(
		addr_data_pair_slim_video_hi556,
		sizeof(addr_data_pair_slim_video_hi556) /
		sizeof(kal_uint16));
#else
	write_cmos_sensor(0x0b0a, 0x8252);
	write_cmos_sensor(0x0f30, 0x6e25);
	write_cmos_sensor(0x0f32, 0x7167);
	write_cmos_sensor(0x004a, 0x0100);
	write_cmos_sensor(0x004c, 0x0000);
	write_cmos_sensor(0x004e, 0x0100); //perframe enable
	write_cmos_sensor(0x000c, 0x0022);
	write_cmos_sensor(0x0008, 0x0b00);
	write_cmos_sensor(0x005a, 0x0204);
	write_cmos_sensor(0x0012, 0x001c);
	write_cmos_sensor(0x0018, 0x0a23);
	write_cmos_sensor(0x0022, 0x0008);
	write_cmos_sensor(0x0028, 0x0017);
	write_cmos_sensor(0x0024, 0x0122);
	write_cmos_sensor(0x002a, 0x0127);
	write_cmos_sensor(0x0026, 0x012c);
	write_cmos_sensor(0x002c, 0x06cb);
	write_cmos_sensor(0x002e, 0x1111);
	write_cmos_sensor(0x0030, 0x1111);
	write_cmos_sensor(0x0032, 0x3311);
	write_cmos_sensor(0x0006, 0x0823);
	write_cmos_sensor(0x0a22, 0x0000);
	write_cmos_sensor(0x0a12, 0x0500);
	write_cmos_sensor(0x0a14, 0x02d0);
	write_cmos_sensor(0x003e, 0x0000);
	write_cmos_sensor(0x0074, 0x0821);
	write_cmos_sensor(0x0070, 0x0411);
	write_cmos_sensor(0x0804, 0x0200);
	write_cmos_sensor(0x0a04, 0x016a);
	write_cmos_sensor(0x090e, 0x0010);
	write_cmos_sensor(0x090c, 0x09c0);
	//===============================================
	//             mipi 2 lane 440Mbps
	//===============================================
	write_cmos_sensor(0x0902, 0x4319);
	write_cmos_sensor(0x0914, 0xc106);
	write_cmos_sensor(0x0916, 0x040e);
	write_cmos_sensor(0x0918, 0x0304);
	write_cmos_sensor(0x091c, 0x0e06);
	write_cmos_sensor(0x091a, 0x0709);
	write_cmos_sensor(0x091e, 0x0300);
#endif
}

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
					"i2c write id : 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}

			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		LOG_INF("Read id fail,sensor id: 0x%x\n", *sensor_id);
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

	LOG_INF("[%s]: PLATFORM:MT6737,MIPI 24LANE\n", __func__);
	LOG_INF("preview 1296*972@30fps,360Mbps/lane;"
		"capture 2592*1944@30fps,880Mbps/lane\n");
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id();
			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, sensor_id);
				break;
			}

			retry--;
		} while (retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 2;
	}
	if (imgsensor_info.sensor_id != sensor_id) {
		LOG_INF("%s sensor id fail: 0x%x\n", __func__, sensor_id);
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	/* initail sequence write in  */
	sensor_init();

	spin_lock(&imgsensor_drv_lock);
	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_en = 0;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}	/*	open  */
static kal_uint32 close(void)
{
	return ERROR_NONE;
}	/*	close  */


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
	LOG_INF("E");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	return ERROR_NONE;
}	/*	preview   */

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
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;

	if (imgsensor.current_fps == imgsensor_info.cap.max_framerate)	{
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {
	 //PIP capture: 24fps for less than 13M, 20fps for 16M,15fps for 20M
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}

	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("Caputre fps:%d\n", imgsensor.current_fps);
	capture_setting(imgsensor.current_fps);

	return ERROR_NONE;

}	/* capture() */
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.current_fps = 300;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	capture_setting(imgsensor.current_fps);
	return ERROR_NONE;
}	/*	normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	//imgsensor.video_mode = KAL_TRUE;
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();

	return ERROR_NONE;
}    /*    hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();

	return ERROR_NONE;
}    /*    slim_video     */

static kal_uint32 get_resolution(
		MSDK_SENSOR_RESOLUTION_INFO_STRUCT * sensor_resolution)
{
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
	return ERROR_NONE;
}    /*    get_resolution    */


static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			MSDK_SENSOR_INFO_STRUCT *sensor_info,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 5; /* not use */

	sensor_info->SensroInterfaceType =
	imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
	sensor_info->SensorOutputDataFormat =
		imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame =
		imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame =
		imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame =
		imgsensor_info.slim_video_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent =
		imgsensor_info.isp_driving_current;
/* The frame of setting shutter default 0 for TG int */
	sensor_info->AEShutDelayFrame =
		imgsensor_info.ae_shut_delay_frame;
/* The frame of setting sensor gain */
	sensor_info->AESensorGainDelayFrame =
		imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame =
		imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine =
		imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum =
		imgsensor_info.sensor_mode_num;

	sensor_info->SensorMIPILaneNumber =
		imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;  // 0 is default 1x
	sensor_info->SensorHightSampling = 0;    // 0 is default 1x
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
	    sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx;
	    sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;

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
	    sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx;
	    sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;
	    sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;
	break;
	default:
	    sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
	    sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

	    sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
	break;
	}

	return ERROR_NONE;
}    /*    get_info  */


static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		LOG_INF("preview\n");
		preview(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	case MSDK_SCENARIO_ID_CAMERA_ZSD:
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
	default:
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}	/* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{
	LOG_INF("framerate = %d ", framerate);
	// SetVideoMode Function should fix framerate
	if (framerate == 0)
		// Dynamic frame rate
		return ERROR_NONE;
	spin_lock(&imgsensor_drv_lock);

	if ((framerate == 30) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 296;
	else if ((framerate == 15) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 146;
	else
		imgsensor.current_fps = 10 * framerate;
	spin_unlock(&imgsensor_drv_lock);
	set_max_framerate(imgsensor.current_fps, 1);
	set_dummy();
	return ERROR_NONE;
}


static kal_uint32 set_auto_flicker_mode(kal_bool enable,
			UINT16 framerate)
{
	LOG_INF("enable = %d, framerate = %d ", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable)
		imgsensor.autoflicker_en = KAL_TRUE;
	else //Cancel Auto flick
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(
			enum MSDK_SCENARIO_ID_ENUM scenario_id,
			MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("scenario_id = %d, framerate = %d\n",
				scenario_id, framerate);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	    frame_length = imgsensor_info.pre.pclk / framerate * 10 /
			imgsensor_info.pre.linelength;
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.dummy_line = (frame_length >
			imgsensor_info.pre.framelength) ?
			(frame_length - imgsensor_info.pre.framelength) : 0;
	    imgsensor.frame_length = imgsensor_info.pre.framelength +
			imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
	    frame_length = imgsensor_info.normal_video.pclk /
			framerate * 10 / imgsensor_info.normal_video.linelength;
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.dummy_line = (frame_length >
			imgsensor_info.normal_video.framelength) ?
		(frame_length - imgsensor_info.normal_video.framelength) : 0;
	    imgsensor.frame_length = imgsensor_info.normal_video.framelength +
			imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (imgsensor.current_fps ==
		    imgsensor_info.cap1.max_framerate) {
			frame_length = imgsensor_info.cap1.pclk /
				framerate * 10 /
				imgsensor_info.cap1.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length >
				imgsensor_info.cap1.framelength) ?
				(frame_length - imgsensor_info.cap1.framelength)
				: 0;
			imgsensor.frame_length = imgsensor_info.cap1.framelength
				+ imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		} else {
			if (imgsensor.current_fps !=
			    imgsensor_info.cap.max_framerate)
				LOG_INF(
					"fps %d fps not support,use cap: %d fps!\n",
					framerate,
					imgsensor_info.cap.max_framerate / 10);
			frame_length = imgsensor_info.cap.pclk /
				framerate * 10 / imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length >
				imgsensor_info.cap.framelength) ?
			(frame_length - imgsensor_info.cap.framelength) : 0;
			imgsensor.frame_length =
				imgsensor_info.cap.framelength +
				imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		}
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
	    frame_length = imgsensor_info.hs_video.pclk /
			framerate * 10 / imgsensor_info.hs_video.linelength;
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.dummy_line = (frame_length >
			imgsensor_info.hs_video.framelength) ? (frame_length -
			imgsensor_info.hs_video.framelength) : 0;
	    imgsensor.frame_length = imgsensor_info.hs_video.framelength +
			imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
	    frame_length = imgsensor_info.slim_video.pclk /
			framerate * 10 / imgsensor_info.slim_video.linelength;
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.dummy_line = (frame_length >
			imgsensor_info.slim_video.framelength) ? (frame_length -
			imgsensor_info.slim_video.framelength) : 0;
	    imgsensor.frame_length =
			imgsensor_info.slim_video.framelength +
			imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	default:  //coding with  preview scenario by default
	    frame_length = imgsensor_info.pre.pclk / framerate * 10 /
						imgsensor_info.pre.linelength;
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.dummy_line = (frame_length >
			imgsensor_info.pre.framelength) ?
			(frame_length - imgsensor_info.pre.framelength) : 0;
	    imgsensor.frame_length = imgsensor_info.pre.framelength +
				imgsensor.dummy_line;
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
				enum MSDK_SCENARIO_ID_ENUM scenario_id,
				MUINT32 *framerate)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

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
	default:
	break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("%s enable: %d", __func__, enable);

	if (enable) {
// 0x5E00[8]: 1 enable,  0 disable
// 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK
		write_cmos_sensor(0x0a04, 0x0143);
		write_cmos_sensor(0x0200, 0x0002);
	} else {
// 0x5E00[8]: 1 enable,  0 disable
// 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK
		write_cmos_sensor(0x0a04, 0x0142);
		write_cmos_sensor(0x0200, 0x0000);
	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 streaming_control(kal_bool enable)
{
	pr_debug("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);

	if (enable)
		write_cmos_sensor(0x0a00, 0x0100); // stream on
	else
		write_cmos_sensor(0x0a00, 0x0000); // stream off

	mdelay(10);
	return ERROR_NONE;
}

static kal_uint32 feature_control(
			MSDK_SENSOR_FEATURE_ENUM feature_id,
			UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	INT32 *feature_return_para_i32 = (INT32 *) feature_para;
	unsigned long long *feature_data =
		(unsigned long long *) feature_para;

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	LOG_INF("feature_id = %d\n", feature_id);
	switch (feature_id) {
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
	    night_mode((BOOL) * feature_data);
	break;
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
	case SENSOR_FEATURE_SET_TEST_PATTERN:
	    set_test_pattern_mode((BOOL)*feature_data);
	break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
	    *feature_return_para_32 = imgsensor_info.checksum_value;
	    *feature_para_len = 4;
	break;
	case SENSOR_FEATURE_SET_FRAMERATE:
	    LOG_INF("current fps :%d\n", (UINT32)*feature_data);
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.current_fps = *feature_data;
	    spin_unlock(&imgsensor_drv_lock);
	break;

	case SENSOR_FEATURE_SET_HDR:
	    LOG_INF("ihdr enable :%d\n", (BOOL)*feature_data);
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.ihdr_en = (BOOL)*feature_data;
	    spin_unlock(&imgsensor_drv_lock);
	break;
	case SENSOR_FEATURE_GET_CROP_INFO:
	    LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
				(UINT32)*feature_data);

	    wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)
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
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[0],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
		break;
		}
	break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
	    LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16)*feature_data, (UINT16)*(feature_data+1),
			(UINT16)*(feature_data+2));
	#if 0
	    ihdr_write_shutter_gain((UINT16)*feature_data,
			(UINT16)*(feature_data+1), (UINT16)*(feature_data+2));
	#endif
	break;
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		*feature_return_para_i32 = 0;
		*feature_para_len = 4;
	break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		streaming_control(KAL_FALSE);
	break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(KAL_TRUE);
	break;
	default:
	break;
	}

	return ERROR_NONE;
}    /*    feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 HI556_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc =  &sensor_func;
	return ERROR_NONE;
}	/*	HI556_MIPI_RAW_SensorInit	*/
