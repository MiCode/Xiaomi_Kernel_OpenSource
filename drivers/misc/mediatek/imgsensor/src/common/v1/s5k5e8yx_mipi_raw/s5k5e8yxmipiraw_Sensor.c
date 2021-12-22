// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define PFX "s5k5e8yx_camera_sensor"
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

#include "s5k5e8yxmipiraw_Sensor.h"


static DEFINE_SPINLOCK(imgsensor_drv_lock);

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = S5K5E8YX_SENSOR_ID,

	.checksum_value = 0x2ae69154,

	.pre = {
		.pclk = 168000000,
		.linelength = 5200,
		.framelength = 1062,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1296,
		.grabwindow_height = 972,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 167200000,
		.max_framerate = 300,
	},
	.cap = {
		.pclk = 169000000,
		.linelength = 2856,
		.framelength = 1985,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1944,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 168000000,
		.max_framerate = 300,
	},
	.cap1 = {
		.pclk = 141000000,
		.linelength = 2856,
		.framelength = 3290,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1944,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 167200000,
		.max_framerate = 150,
	},
	.cap2 = {
		.pclk = 141000000,
		.linelength = 2856,
		.framelength = 2056,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1944,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 167200000,
		.max_framerate = 240,
	},
	.normal_video = {
		.pclk = 169000000,
		.linelength = 2856,
		.framelength = 2008,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1944,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 168000000,
		.max_framerate = 300,
	},
	.hs_video = {
		.pclk = 168000000,
		.linelength = 2816,
		.framelength = 546,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 640,
		.grabwindow_height = 480,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 168000000,
		.max_framerate = 1092,
	},
	.slim_video = {
		.pclk = 168000000,
		.linelength = 2896,
		.framelength = 1954,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1280,
		.grabwindow_height = 720,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 167200000,
		.max_framerate = 300,
	},
	.margin = 6,
	.min_shutter = 2,
	.max_frame_length = 0xffff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 1,
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,
	.ihdr_le_firstline = 0,
	.sensor_mode_num = 5,

	.cap_delay_frame = 3,
	.pre_delay_frame = 3,
	.video_delay_frame = 3,
	.hs_video_delay_frame = 3,
	.slim_video_delay_frame = 3,

	.isp_driving_current = ISP_DRIVING_6MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_2_LANE,
	.i2c_addr_table = {0x20, 0x5a, 0xff},
	.i2c_speed = 400,
};

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x3D0,
	.gain = 0x100,
	.dummy_pixel = 0,
	.dummy_line = 0,
	.current_fps = 0,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = 0,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_en = 0,
	.i2c_write_id = 0x20,
};


/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
{ 2592, 1944,	  0,	0, 2592, 1944, 1296,  972,
	0000, 0000, 1296,  972,	  0,	0, 1296,  972},
{ 2592, 1944,	  0,	0, 2592, 1944, 2592, 1944,
	0000, 0000, 2592, 1944,	  0,	0, 2592, 1944},
{ 2592, 1944,	  0,    0, 2592, 1944, 2592, 1944,
	0000, 0000, 2592, 1944,	  0,	0, 2592, 1944},
{ 2592, 1944,	  24, 260, 2560, 1440,  640,  480,
	0000, 0000,  640,  480,	  0,	0,  640,  480},
{ 2592, 1944,	  24,  20, 2560, 1920, 1280,  720,
	0000, 0000, 1280,  720,	  0,	0, 1280,  720} };


static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;

	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte,
		1, imgsensor.i2c_write_id);

	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[3] = {(char)(addr >> 8),
		(char)(addr & 0xFF), (char)(para & 0xFF)};

	iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}

static void write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
	char pusendcmd[4] = {(char)(addr >> 8),
		(char)(addr & 0xFF), (char)(para & 0xFF)};

	iWriteRegI2C(pusendcmd, 3, imgsensor.i2c_write_id);
}


static void set_dummy(void)
{
	/*pr_info("dummyline = %d, dummypixels = %d\n",
	 *	imgsensor.dummy_line, imgsensor.dummy_pixel);
	 */
	write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
	write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
	write_cmos_sensor(0x0342, imgsensor.line_length >> 8);
	write_cmos_sensor(0x0343, imgsensor.line_length & 0xFF);

}	/*	set_dummy  */

static kal_uint32 return_sensor_id(void)
{
	return ((read_cmos_sensor(0x0000) << 8) |
		read_cmos_sensor(0x0001));

}


static void set_max_framerate(UINT16 framerate,
			kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	pr_info("framerate = %d, min framelength should enable? %d\n",
		framerate, min_framelength_en);

	frame_length = imgsensor.pclk /
		framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length >
		imgsensor.min_frame_length) ?
		frame_length : imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length
		- imgsensor.min_frame_length;
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


static void write_shutter(kal_uint16 shutter)
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
	shutter = (shutter > (imgsensor_info.max_frame_length -
		imgsensor_info.margin)) ? (imgsensor_info.max_frame_length -
		imgsensor_info.margin) : shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk /
			imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			write_cmos_sensor(0x0340,
				imgsensor.frame_length >> 8);
			write_cmos_sensor(0x0341,
				imgsensor.frame_length & 0xFF);
		}
	} else {
		write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
	}


	/*write_cmos_sensor(0x0104, 0x01);*/
	write_cmos_sensor(0x0202, shutter >> 8);
	write_cmos_sensor(0x0203, shutter & 0xFF);
	/*write_cmos_sensor(0x0104, 0x00);*/

	pr_info("shutter =%d, framelength =%d\n",
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
static void set_shutter(kal_uint16 shutter)
{
	unsigned long flags;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	write_shutter(shutter);
}	/*	set_shutter */

static void set_shutter_frame_length(kal_uint16 shutter,
			kal_uint16 frame_length)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	kal_int32 dummy_line = 0;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	spin_lock(&imgsensor_drv_lock);
	/*Change frame time*/
	dummy_line = frame_length - imgsensor.frame_length;
	imgsensor.frame_length = imgsensor.frame_length + dummy_line;
	imgsensor.min_frame_length = imgsensor.frame_length;

	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;

	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter) ?
		imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length -
		imgsensor_info.margin)) ? (imgsensor_info.max_frame_length -
		imgsensor_info.margin) : shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk /
			imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			/*write_cmos_sensor(0x0104, 0x01);*/
			write_cmos_sensor(0x0340,
			imgsensor.frame_length >> 8);
			write_cmos_sensor(0x0341,
			imgsensor.frame_length & 0xFF);
			/*write_cmos_sensor(0x0104, 0x00);*/
		}
	} else {

		/*write_cmos_sensor(0x0104, 0x01);*/
		write_cmos_sensor(0x0340,
			imgsensor.frame_length >> 8);
		write_cmos_sensor(0x0341,
			imgsensor.frame_length & 0xFF);
		/*write_cmos_sensor(0x0104, 0x00);*/
	}

	/*write_cmos_sensor(0x0104, 0x01);*/
	write_cmos_sensor(0x0202, shutter >> 8);
	write_cmos_sensor(0x0203, shutter  & 0xFF);
	/*write_cmos_sensor(0x0104, 0x00);*/
	pr_info("Add for N3D! shutter =%d, framelength =%d\n",
		shutter, imgsensor.frame_length);

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
	/* [0:3] = N meams N /16 X	*/
	/* [4:9] = M meams M X		 */
	/* Total gain = M + N /16 X   */

	reg_gain = gain>>1;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	pr_info("gain = %d , reg_gain = 0x%x\n ",
		gain, reg_gain);

	/*write_cmos_sensor(0x0104, 0x01);*/
	write_cmos_sensor(0x0204, reg_gain >> 8);
	write_cmos_sensor(0x0205, reg_gain & 0xFF);
	/*write_cmos_sensor(0x0104, 0x00);*/

	return gain;
}	/*	set_gain  */

static void ihdr_write_shutter_gain(kal_uint16 le,
			kal_uint16 se, kal_uint16 gain)
{
	pr_info("le:0x%x, se:0x%x, gain:0x%x\n", le, se, gain);
	if (imgsensor.ihdr_en) {

		spin_lock(&imgsensor_drv_lock);
		if (le > imgsensor.min_frame_length - imgsensor_info.margin)
			imgsensor.frame_length = le + imgsensor_info.margin;
		else
			imgsensor.frame_length =
				imgsensor.min_frame_length;
		if (imgsensor.frame_length > imgsensor_info.max_frame_length)
			imgsensor.frame_length =
				imgsensor_info.max_frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (le < imgsensor_info.min_shutter)
			le = imgsensor_info.min_shutter;
		if (se < imgsensor_info.min_shutter)
			se = imgsensor_info.min_shutter;

		write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
		write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);

		write_cmos_sensor(0x3502, (le << 4) & 0xFF);
		write_cmos_sensor(0x3501, (le >> 4) & 0xFF);
		write_cmos_sensor(0x3500, (le >> 12) & 0x0F);

		write_cmos_sensor(0x3508, (se << 4) & 0xFF);
		write_cmos_sensor(0x3507, (se >> 4) & 0xFF);
		write_cmos_sensor(0x3506, (se >> 12) & 0x0F);

		set_gain(gain);
	}

}

static void set_mirror_flip(kal_uint8 image_mirror)
{
	pr_info("image_mirror = %d\n", image_mirror);

	switch (image_mirror) {
	case IMAGE_NORMAL:
		write_cmos_sensor(0x0101, 0x00);
		break;
	case IMAGE_H_MIRROR:
		write_cmos_sensor(0x0101, 0x01);
		break;
	case IMAGE_V_MIRROR:
		write_cmos_sensor(0x0101, 0x02);
		break;
	case IMAGE_HV_MIRROR:
		write_cmos_sensor(0x0101, 0x03);
		break;
	default:
		pr_info("Error image_mirror setting\n");
	}

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
#define MULTI_WRITE 1

#if MULTI_WRITE
#define I2C_BUFFER_LEN 765	/* trans# max is 255, each 3 bytes */
#else
#define I2C_BUFFER_LEN 3

#endif
static kal_uint16 s5k5e8_table_write_cmos_sensor(
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
			puSendCmd[tosend++] = (char)(data & 0xFF);
			IDX += 2;
			addr_last = addr;
		}
#if MULTI_WRITE
		if ((I2C_BUFFER_LEN - tosend) < 3 ||
			len == IDX	||
			addr != addr_last) {

			iBurstWriteReg_multi(puSendCmd,
				tosend,
				imgsensor.i2c_write_id,
				3,
				imgsensor_info.i2c_speed);
				tosend = 0;
			}
#else
		iWriteRegI2C(puSendCmd, 3, imgsensor.i2c_write_id);
		tosend = 0;
#endif
	}
	return 0;
}


kal_uint16 addr_data_pair_init_s5k5e8[] = {
	0x0100, 0x00,
	0x3906, 0x7E,
	0x3C01, 0x0F,
	0x3C14, 0x00,
	0x3235, 0x08,
	0x3063, 0x2E,
	0x307A, 0x10,
	0x307B, 0x0E,
	0x3079, 0x20,
	0x3070, 0x05,
	0x3067, 0x06,
	0x3071, 0x62,
	0x3203, 0x43,
	0x3205, 0x43,
	0x320b, 0x42,
	0x3007, 0x00,
	0x3008, 0x14,
	0x3020, 0x58,
	0x300D, 0x34,
	0x300E, 0x17,
	0x3021, 0x02,
	0x3010, 0x59,
	0x3002, 0x01,
	0x3005, 0x01,
	0x3008, 0x04,
	0x300F, 0x70,
	0x3010, 0x69,
	0x3017, 0x10,
	0x3019, 0x19,
	0x300C, 0x62,
	0x3064, 0x10,
	0x3C08, 0x0E,
	0x3C09, 0x10,
	0x3C31, 0x0D,
	0x3C32, 0xAC,
	0x3929, 0x07,
	0x3C02, 0x0F,
};



static void sensor_init(void)
{
	pr_info("E\n");

	s5k5e8_table_write_cmos_sensor(addr_data_pair_init_s5k5e8,
	   sizeof(addr_data_pair_init_s5k5e8)/sizeof(kal_uint16));
}	/*	sensor_init  */


kal_uint16 addr_data_pair_preview_s5k5e8[] = {
	0x0136, 0x18,
	0x0137, 0x00,
	0x0305, 0x06,
	0x0306, 0x18,
	0x0307, 0xA8,
	0x0308, 0x34,
	0x0309, 0x42,
	0x3C1F, 0x00,
	0x3C17, 0x00,
	0x3C0B, 0x04,
	0x3C1C, 0x47,
	0x3C1D, 0x15,
	0x3C14, 0x04,
	0x3C16, 0x00,
	0x0820, 0x03,
	0x0821, 0x44,
	0x0114, 0x01,
	0x0344, 0x00,
	0x0345, 0x08,
	0x0346, 0x00,
	0x0347, 0x08,
	0x0348, 0x0A,
	0x0349, 0x27,
	0x034A, 0x07,
	0x034B, 0x9F,
	0x034C, 0x05,
	0x034D, 0x10,
	0x034E, 0x03,
	0x034F, 0xCC,
	0x0900, 0x01,
	0x0901, 0x22,
	0x0381, 0x01,
	0x0383, 0x01,
	0x0385, 0x01,
	0x0387, 0x03,
	0x0340, 0x04,
	0x0341, 0x26,
	0x0342, 0x14,
	0x0343, 0x50,
	0x0200, 0x00,
	0x0201, 0x00,
	0x0202, 0x03,
	0x0203, 0xDE,
	0x3303, 0x02,
	0x3400, 0x01,
	0x323b, 0x02,
	0x3301, 0x00,
	0x3321, 0x04,
	0x3306, 0x00,
	0x3307, 0x08,
	0x3308, 0x0A,
	0x3309, 0x27,
	0x330A, 0x01,
	0x330B, 0x01,
	0x330E, 0x00,
	0x330F, 0x08,
	0x3310, 0x07,
	0x3311, 0x9F,
	0x3312, 0x03,
	0x3313, 0x01,
};

static void preview_setting(void)
{
	/*Delay 1 frame*/
	mDELAY(33);
	/*Extclk_frequency_mhz*/
	s5k5e8_table_write_cmos_sensor(addr_data_pair_preview_s5k5e8,
	   sizeof(addr_data_pair_preview_s5k5e8)/sizeof(kal_uint16));
}	/*	preview_setting  */


kal_uint16 addr_data_pair_capture_s5k5e8_15fps[] = {
	0x0100, 0x00,
	0x0136, 0x18,
	0x0137, 0x00,
	0x0305, 0x06,
	0x0306, 0x18,
	0x0307, 0x8D,
	0x0308, 0x34,
	0x0309, 0x42,
	0x3C1F, 0x00,
	0x3C17, 0x00,
	0x3C0B, 0x04,
	0x3C1C, 0x47,
	0x3C1D, 0x15,
	0x3C14, 0x04,
	0x3C16, 0x00,
	0x0820, 0x03,
	0x0821, 0x44,
	0x0114, 0x01,
	0x0344, 0x00,
	0x0345, 0x08,
	0x0346, 0x00,
	0x0347, 0x08,
	0x0348, 0x0A,
	0x0349, 0x27,
	0x034A, 0x07,
	0x034B, 0x9F,
	0x034C, 0x0A,
	0x034D, 0x20,
	0x034E, 0x07,
	0x034F, 0x98,
	0x0900, 0x00,
	0x0901, 0x00,
	0x0381, 0x01,
	0x0383, 0x01,
	0x0385, 0x01,
	0x0387, 0x01,
	0x0340, 0x0C,
	0x0341, 0xDA,
	0x0342, 0x0B,
	0x0343, 0x28,
	0x0200, 0x00,
	0x0201, 0x00,
	0x0202, 0x03,
	0x0203, 0xDE,
	0x3303, 0x02,
	0x3400, 0x01,
	0x323b, 0x02,
	0x3301, 0x00,
	0x3321, 0x04,
	0x3306, 0x00,
	0x3307, 0x08,
	0x3308, 0x0A,
	0x3309, 0x27,
	0x330A, 0x01,
	0x330B, 0x01,
	0x330E, 0x00,
	0x330F, 0x08,
	0x3310, 0x07,
	0x3311, 0x9F,
	0x3312, 0x01,
	0x3313, 0x01,

};

kal_uint16 addr_data_pair_capture_s5k5e8_24fps[] = {
	0x0100, 0x00,
	0x0136, 0x18,
	0x0137, 0x00,
	0x0305, 0x06,
	0x0306, 0x18,
	0x0307, 0x8D,
	0x0308, 0x34,
	0x0309, 0x42,
	0x3C1F, 0x00,
	0x3C17, 0x00,
	0x3C0B, 0x04,
	0x3C1C, 0x47,
	0x3C1D, 0x15,
	0x3C14, 0x04,
	0x3C16, 0x00,
	0x0820, 0x03,
	0x0821, 0x44,
	0x0114, 0x01,
	0x0344, 0x00,
	0x0345, 0x08,
	0x0346, 0x00,
	0x0347, 0x08,
	0x0348, 0x0A,
	0x0349, 0x27,
	0x034A, 0x07,
	0x034B, 0x9F,
	0x034C, 0x0A,
	0x034D, 0x20,
	0x034E, 0x07,
	0x034F, 0x98,
	0x0900, 0x00,
	0x0901, 0x00,
	0x0381, 0x01,
	0x0383, 0x01,
	0x0385, 0x01,
	0x0387, 0x01,
	0x0340, 0x08,
	0x0341, 0x08,
	0x0342, 0x0B,
	0x0343, 0x28,
	0x0200, 0x00,
	0x0201, 0x00,
	0x0202, 0x03,
	0x0203, 0xDE,
	0x3303, 0x02,
	0x3400, 0x01,
	0x323b, 0x02,
	0x3301, 0x00,
	0x3321, 0x04,
	0x3306, 0x00,
	0x3307, 0x08,
	0x3308, 0x0A,
	0x3309, 0x27,
	0x330A, 0x01,
	0x330B, 0x01,
	0x330E, 0x00,
	0x330F, 0x08,
	0x3310, 0x07,
	0x3311, 0x9F,
	0x3312, 0x01,
	0x3313, 0x01,
};

kal_uint16 addr_data_pair_capture_s5k5e8[] = {
	0x0100, 0x00,
	0x0136, 0x18,
	0x0137, 0x00,
	0x0305, 0x06,
	0x0306, 0x18,
	0x0307, 0xA9,
	0x0308, 0x34,
	0x0309, 0x42,
	0x3C1F, 0x00,
	0x3C17, 0x00,
	0x3C0B, 0x04,
	0x3C1C, 0x47,
	0x3C1D, 0x15,
	0x3C14, 0x04,
	0x3C16, 0x00,
	0x0820, 0x03,
	0x0821, 0x44,
	0x0114, 0x01,
	0x0344, 0x00,
	0x0345, 0x08,
	0x0346, 0x00,
	0x0347, 0x08,
	0x0348, 0x0A,
	0x0349, 0x27,
	0x034A, 0x07,
	0x034B, 0x9F,
	0x034C, 0x0A,
	0x034D, 0x20,
	0x034E, 0x07,
	0x034F, 0x98,
	0x0900, 0x00,
	0x0901, 0x00,
	0x0381, 0x01,
	0x0383, 0x01,
	0x0385, 0x01,
	0x0387, 0x01,
	0x0340, 0x07,
	0x0341, 0xC1,
	0x0342, 0x0B,
	0x0343, 0x28,
	0x0200, 0x00,
	0x0201, 0x00,
	0x0202, 0x03,
	0x0203, 0xDE,
	0x3303, 0x02,
	0x3400, 0x01,
	0x323b, 0x02,
	0x3301, 0x00,
	0x3321, 0x04,
	0x3306, 0x00,
	0x3307, 0x08,
	0x3308, 0x0A,
	0x3309, 0x27,
	0x330A, 0x01,
	0x330B, 0x01,
	0x330E, 0x00,
	0x330F, 0x08,
	0x3310, 0x07,
	0x3311, 0x9F,
	0x3312, 0x01,
	0x3313, 0x01,
};

static void capture_setting(kal_uint16 currefps)
{
	pr_info("E! currefps:%d\n", currefps);
	if (currefps == 150) {
		/*write_cmos_sensor(0x0100, 0X00);*/
		/*Delay 1 frame*/
		mDELAY(67);
		s5k5e8_table_write_cmos_sensor(
			addr_data_pair_capture_s5k5e8_15fps,
	sizeof(addr_data_pair_capture_s5k5e8_15fps)/sizeof(kal_uint16));
	} else if (currefps == 240) {
		/*Delay 1 frame*/
		mDELAY(42);
		s5k5e8_table_write_cmos_sensor(
			addr_data_pair_capture_s5k5e8_24fps,
	sizeof(addr_data_pair_capture_s5k5e8_24fps)/sizeof(kal_uint16));
	} else {
		/*Delay 1 frame*/
		mDELAY(33);
		s5k5e8_table_write_cmos_sensor(
			addr_data_pair_capture_s5k5e8,
	sizeof(addr_data_pair_capture_s5k5e8)/sizeof(kal_uint16));
	}
}

kal_uint16 addr_data_pair_video_s5k5e8[] = {
	0x0100, 0x00,
	0x0136, 0x18,
	0x0137, 0x00,
	0x0305, 0x06,
	0x0306, 0x18,
	0x0307, 0xA9,
	0x0308, 0x34,
	0x0309, 0x82,
	0x3C1F, 0x00,
	0x3C17, 0x00,
	0x3C0B, 0x04,
	0x3C1C, 0x47,
	0x3C1D, 0x15,
	0x3C14, 0x04,
	0x3C16, 0x00,
	0x0820, 0x03,
	0x0821, 0x44,
	0x0114, 0x01,
	0x0344, 0x00,
	0x0345, 0x08,
	0x0346, 0x00,
	0x0347, 0x08,
	0x0348, 0x0A,
	0x0349, 0x27,
	0x034A, 0x07,
	0x034B, 0x9F,
	0x034C, 0x0A,
	0x034D, 0x20,
	0x034E, 0x07,
	0x034F, 0x98,
	0x0900, 0x00,
	0x0901, 0x00,
	0x0381, 0x01,
	0x0383, 0x01,
	0x0385, 0x01,
	0x0387, 0x01,
	0x0340, 0x07,
	0x0341, 0xD8,
	0x0342, 0x0B,
	0x0343, 0x28,
	0x0200, 0x00,
	0x0201, 0x00,
	0x0202, 0x03,
	0x0203, 0xDE,
	0x3303, 0x02,
	0x3400, 0x01,
	0x323b, 0x02,
	0x3301, 0x00,
	0x3321, 0x04,
	0x3306, 0x00,
	0x3307, 0x08,
	0x3308, 0x0A,
	0x3309, 0x27,
	0x330A, 0x01,
	0x330B, 0x01,
	0x330E, 0x00,
	0x330F, 0x08,
	0x3310, 0x07,
	0x3311, 0x9F,
	0x3312, 0x01,
	0x3313, 0x01,
};

static void normal_video_setting(kal_uint16 currefps)
{
	pr_info("E! currefps:%d\n", currefps);
	/*Delay 1 frame*/
	mDELAY(33);
	s5k5e8_table_write_cmos_sensor(addr_data_pair_video_s5k5e8,
		sizeof(addr_data_pair_video_s5k5e8)/sizeof(kal_uint16));
}

kal_uint16 addr_data_pair_hs_video_s5k5e8[] = {
	0x0100, 0x00,
	0x0136, 0x14,
	0x0137, 0x00,
	0x0305, 0x05,
	0x0306, 0x14,
	0x0307, 0xA8,
	0x0308, 0x34,
	0x0309, 0x42,
	0x3C1F, 0x00,
	0x3C17, 0x00,
	0x3C0B, 0x04,
	0x3C1C, 0x47,
	0x3C1D, 0x15,
	0x3C14, 0x04,
	0x3C16, 0x00,
	0x0820, 0x03,
	0x0821, 0x44,
	0x0114, 0x01,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x09,
	0x0349, 0xFF,
	0x034A, 0x07,
	0x034B, 0x7F,
	0x034C, 0x02,
	0x034D, 0x80,
	0x034E, 0x01,
	0x034F, 0xE0,
	0x0900, 0x01,
	0x0901, 0x42,
	0x0381, 0x01,
	0x0383, 0x01,
	0x0385, 0x01,
	0x0387, 0x07,
	0x0340, 0x02,
	0x0341, 0x22,
	0x0342, 0x0B,
	0x0343, 0x00,
	0x0200, 0x00,
	0x0201, 0x00,
	0x0202, 0x01,
	0x0203, 0xDE,
	0x3303, 0x02,
	0x3400, 0x01,
	0x323b, 0x02,
	0x3301, 0x00,
	0x3321, 0x04,
	0x3306, 0x00,
	0x3307, 0x00,
	0x3308, 0x09,
	0x3309, 0xFF,
	0x330A, 0x01,
	0x330B, 0x01,
	0x330E, 0x00,
	0x330F, 0x00,
	0x3310, 0x07,
	0x3311, 0x7F,
	0x3312, 0x07,
	0x3313, 0x01,
};

static void hs_video_setting(void)
{
	pr_info("E! VGA 120fps\n");
	/*Delay 1 frame*/
	mDELAY(83);
	s5k5e8_table_write_cmos_sensor(addr_data_pair_hs_video_s5k5e8,
		sizeof(addr_data_pair_hs_video_s5k5e8)/sizeof(kal_uint16));
}

kal_uint16 addr_data_pair_slim_video_s5k5e8[] = {
	0x0100, 0x00,
	0x0136, 0x18,
	0x0137, 0x00,
	0x0305, 0x06,
	0x0306, 0x18,
	0x0307, 0xA8,
	0x0308, 0x34,
	0x0309, 0x42,
	0x3C1F, 0x00,
	0x3C17, 0x00,
	0x3C0B, 0x04,
	0x3C1C, 0x47,
	0x3C1D, 0x15,
	0x3C14, 0x04,
	0x3C16, 0x00,
	0x0820, 0x03,
	0x0821, 0x44,
	0x0114, 0x01,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x0A,
	0x0349, 0x17,
	0x034A, 0x06,
	0x034B, 0xA3,
	0x034C, 0x05,
	0x034D, 0x00,
	0x034E, 0x02,
	0x034F, 0xD0,
	0x0900, 0x01,
	0x0901, 0x22,
	0x0381, 0x01,
	0x0383, 0x01,
	0x0385, 0x01,
	0x0387, 0x03,
	0x0340, 0x07,
	0x0341, 0xA2,
	0x0342, 0x0B,
	0x0343, 0x50,
	0x0200, 0x00,
	0x0201, 0x00,
	0x0202, 0x03,
	0x0203, 0xDE,
	0x3303, 0x02,
	0x3400, 0x01,
	0x323b, 0x02,
	0x3301, 0x00,
	0x3321, 0x04,
	0x3306, 0x00,
	0x3307, 0x00,
	0x3308, 0x0A,
	0x3309, 0x17,
	0x330A, 0x01,
	0x330B, 0x01,
	0x330E, 0x00,
	0x330F, 0x00,
	0x3310, 0x06,
	0x3311, 0xA3,
	0x3312, 0x03,
	0x3313, 0x01,
};
static void slim_video_setting(void)
{
	pr_info("E! HD 30fps\n");
	/*Delay 1 frame*/
	mDELAY(33);
	s5k5e8_table_write_cmos_sensor(addr_data_pair_slim_video_s5k5e8,
		sizeof(addr_data_pair_slim_video_s5k5e8)/sizeof(kal_uint16));
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
#define S5K5E8YXFRONT_I2CBUS    (4)

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
				pr_info("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}
			pr_info("Read id fail,i2c_write_id 0x%x id: 0x%x\n",
				imgsensor.i2c_write_id, *sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
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


	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id();
			if (sensor_id == imgsensor_info.sensor_id) {
				pr_info("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, sensor_id);
				break;
			}
			pr_info("R sensorid fail,write_id 0x%x id: 0x%x\n",
				imgsensor.i2c_write_id, sensor_id);
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
	imgsensor.ihdr_en = 0;
	imgsensor.test_pattern = 0;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}	/*	open  */



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
	pr_info("E\n");

	/*No Need to implement this function*/

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
	pr_info("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	set_mirror_flip(imgsensor.mirror);
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
	pr_info("E\n");
	spin_lock(&imgsensor_drv_lock);

	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (imgsensor.current_fps == imgsensor_info.cap.max_framerate) {
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
		}
	else if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
		pr_info("current_fps %d not support, so cap1's: %d fps!\n",
			imgsensor.current_fps,
			imgsensor_info.cap1.max_framerate/10);
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {
		pr_info("current_fps %d not support, so use cap2's: %d fps!\n",
			imgsensor.current_fps,
			imgsensor_info.cap2.max_framerate/10);
		imgsensor.pclk = imgsensor_info.cap2.pclk;
		imgsensor.line_length = imgsensor_info.cap2.linelength;
		imgsensor.frame_length = imgsensor_info.cap2.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap2.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);

	capture_setting(imgsensor.current_fps);
	set_mirror_flip(imgsensor.mirror);


	return ERROR_NONE;
}	/* capture() */
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_info("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length =
		imgsensor_info.normal_video.linelength;
	imgsensor.frame_length =
		imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length =
		imgsensor_info.normal_video.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting(imgsensor.current_fps);
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/*	normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_info("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	imgsensor.line_length =
		imgsensor_info.hs_video.linelength;
	imgsensor.frame_length =
		imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length =
		imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/*	hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_info("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	imgsensor.line_length =
		imgsensor_info.slim_video.linelength;
	imgsensor.frame_length =
		imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length =
		imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/*	slim_video	 */



static kal_uint32 get_resolution(
		MSDK_SENSOR_RESOLUTION_INFO_STRUCT * sensor_resolution)
{
	pr_info("E\n");
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
}	/*	get_resolution	*/

static kal_uint32 get_info(
			enum MSDK_SCENARIO_ID_ENUM scenario_id,
			MSDK_SENSOR_INFO_STRUCT *sensor_info,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_info("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity =
		SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity =
		SENSOR_CLOCK_POLARITY_LOW; /* not use */
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 5; /* not use */

	sensor_info->SensroInterfaceType =
		imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType =
		imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode =
		imgsensor_info.mipi_settle_delay_mode;
	sensor_info->SensorOutputDataFormat =
		imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
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
	sensor_info->SensorWidthSampling = 0;
	sensor_info->SensorHightSampling = 0;
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
		sensor_info->SensorGrabStartX =
			imgsensor_info.hs_video.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.hs_video.starty;

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
	pr_info("scenario_id = %d\n", scenario_id);
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
	default:
		pr_info("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}	/* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{
	pr_info("framerate = %d\n ", framerate);

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
	pr_info("enable = %d, framerate = %d\n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable)
		imgsensor.autoflicker_en = KAL_TRUE;
	else
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(
			enum MSDK_SCENARIO_ID_ENUM scenario_id,
			MUINT32 framerate)
{
	kal_uint32 frame_length;

	pr_info("scenario_id = %d, framerate = %d\n",
		scenario_id, framerate);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frame_length = imgsensor_info.pre.pclk /
			framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.pre.framelength) ?
			(frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		else {
			/*No need to set*/
			pr_debug("frame_length %d < shutter %d",
				imgsensor.frame_length, imgsensor.shutter);
		}
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.normal_video.pclk /
			framerate * 10 / imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length >
			imgsensor_info.normal_video.framelength) ?
			(frame_length -
			imgsensor_info.normal_video.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.normal_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (imgsensor.current_fps ==
			imgsensor_info.cap1.max_framerate) {
			frame_length = imgsensor_info.cap1.pclk /
				framerate * 10 / imgsensor_info.cap1.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length >
				imgsensor_info.cap1.framelength) ?
				(frame_length -
				imgsensor_info.cap1.framelength) : 0;
			imgsensor.frame_length =
				imgsensor_info.cap1.framelength +
				imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		} else if (imgsensor.current_fps ==
			imgsensor_info.cap2.max_framerate) {
			frame_length = imgsensor_info.cap2.pclk /
				framerate * 10 / imgsensor_info.cap2.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length >
				imgsensor_info.cap2.framelength) ?
				(frame_length -
				imgsensor_info.cap2.framelength) : 0;
			imgsensor.frame_length =
				imgsensor_info.cap2.framelength +
				imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		} else {
			if (imgsensor.current_fps !=
				imgsensor_info.cap.max_framerate) {
				pr_info("%d fps not support,cap's: %d fps\n",
					framerate,
					imgsensor_info.cap.max_framerate/10);
			}
			frame_length = imgsensor_info.cap.pclk /
				framerate * 10 / imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length >
				imgsensor_info.cap.framelength) ?
				(frame_length -
				imgsensor_info.cap.framelength) : 0;
			imgsensor.frame_length =
				imgsensor_info.cap.framelength +
				imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		}
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		else {
			/*No need to set*/
			pr_debug("frame_length %d < shutter %d",
				imgsensor.frame_length, imgsensor.shutter);
		}
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
		else {
			/*No need to set*/
			pr_debug("frame_length %d < shutter %d",
				imgsensor.frame_length, imgsensor.shutter);
		}
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk /
			framerate * 10 / imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length >
			imgsensor_info.slim_video.framelength) ? (frame_length -
			imgsensor_info.slim_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.slim_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		else {
			/*No need to set*/
			pr_debug("frame_length %d < shutter %d",
				imgsensor.frame_length, imgsensor.shutter);
		}
		break;
	default:
		frame_length = imgsensor_info.pre.pclk /
			framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length >
			imgsensor_info.pre.framelength) ? (frame_length -
			imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		else {
			/*No need to set*/
			pr_debug("frame_length %d < shutter %d",
				imgsensor.frame_length, imgsensor.shutter);
		}
		pr_info("error scenario_id = %d, preview scenario\n",
			scenario_id);
		break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(
			enum MSDK_SCENARIO_ID_ENUM scenario_id,
			MUINT32 *framerate)
{
	pr_info("scenario_id = %d\n", scenario_id);

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

static kal_uint32 set_test_pattern_mode(kal_uint32 modes,
	struct SET_SENSOR_PATTERN_SOLID_COLOR *pdata)
{
	kal_uint16 Color_R, Color_Gr, Color_Gb, Color_B;

	pr_info("set_test_pattern enum: %d\n", modes);
	if (modes) {
		write_cmos_sensor(0x0600, modes);
		if (modes == 1 && (pdata != NULL)) { //Solid Color
			pr_info("R=0x%x,Gr=0x%x,B=0x%x,Gb=0x%x",
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

	write_cmos_sensor(0x3200, 0x00);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = modes;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}
static kal_uint32 streaming_control(kal_bool enable)
{
	pr_debug("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
	if (enable) {
		write_cmos_sensor(0x3C16, 0x00);
		write_cmos_sensor(0x3C0D, 0x04);
		write_cmos_sensor(0x0100, 0x01);
		mDELAY(1);
		write_cmos_sensor(0x3C0D, 0x00);
	} else {
		/*Streaming off*/
		write_cmos_sensor(0x0100, 0x00);
	}
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
	unsigned long long *feature_data = (unsigned long long *) feature_para;

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	/*pr_info("feature_id = %d", feature_id);*/
	switch (feature_id) {
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16 = imgsensor.frame_length;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		pr_info("imgsensor.pclk = %d,imgsensor.current_fps = %d\n",
				imgsensor.pclk, imgsensor.current_fps);
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16) *feature_data);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		if ((sensor_reg_data->RegData>>8) > 0)
			write_cmos_sensor(sensor_reg_data->RegAddr,
				sensor_reg_data->RegData);
		else
			write_cmos_sensor_8(sensor_reg_data->RegAddr,
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
		set_test_pattern_mode((UINT32)*feature_data,
		(struct SET_SENSOR_PATTERN_SOLID_COLOR *)(feature_data+1));
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		pr_debug("current fps :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = (UINT16)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		pr_info("Warning! Not Support IHDR Feature");
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_en = KAL_FALSE;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		pr_info("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%llu\n",
			*feature_data);
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
		pr_info("SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16)*feature_data, (UINT16)*(feature_data+1),
			(UINT16)*(feature_data+2));
		ihdr_write_shutter_gain((UINT16)*feature_data,
			(UINT16)*(feature_data+1), (UINT16)*(feature_data+2));
		break;
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length((UINT16)*feature_data,
			(UINT16)*(feature_data+1));
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		pr_debug("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		pr_debug(
		    "SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
		    *feature_data);

		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(KAL_TRUE);
		break;
	case SENSOR_FEATURE_GET_PIXEL_RATE:

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.cap.pclk /
			(imgsensor_info.cap.linelength - 80))*
			imgsensor_info.cap.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.normal_video.pclk /
			(imgsensor_info.normal_video.linelength - 80))*
			imgsensor_info.normal_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.hs_video.pclk /
			(imgsensor_info.hs_video.linelength - 80))*
			imgsensor_info.hs_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.slim_video.pclk /
			(imgsensor_info.slim_video.linelength - 80))*
			imgsensor_info.slim_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.pre.pclk /
			(imgsensor_info.pre.linelength - 80))*
			imgsensor_info.pre.grabwindow_width;
			break;
		}
		break;

	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.cap.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.normal_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.hs_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.slim_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
		break;

	default:
		break;
	}

	return ERROR_NONE;
}	/*	feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 S5K5E8YX_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc =  &sensor_func;
	return ERROR_NONE;
}	/*	S5K5E8YX_MIPI_RAW_SensorInit	*/
