// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/************************************************************************
 *
 * Filename:
 * ---------
 *     IMX481mipi_Sensor.c
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
 *-----------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *=====================================================
 ************************************************************************/
#define PFX "IMX481_camera_sensor"
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

#include "imx481mipiraw_Sensor.h"


/************************************************************************
 * Proifling
 ************************************************************************/
#define PROFILE 0
#if PROFILE
static struct timeval tv1, tv2;
static DEFINE_SPINLOCK(kdsensor_drv_lock);
/************************************************************************
 *
 ************************************************************************/
static void KD_SENSOR_PROFILE_INIT(void)
{
	do_gettimeofday(&tv1);
}

/************************************************************************
 *
 ************************************************************************/
static void KD_SENSOR_PROFILE(char *tag)
{
	unsigned long TimeIntervalUS;

	spin_lock(&kdsensor_drv_lock);

	do_gettimeofday(&tv2);
	TimeIntervalUS =
	    (tv2.tv_sec - tv1.tv_sec) * 1000000 + (tv2.tv_usec - tv1.tv_usec);
	tv1 = tv2;

	spin_unlock(&kdsensor_drv_lock);
	pr_debug("[%s]Profile = %lu us\n", tag, TimeIntervalUS);
}
#else
static void KD_SENSOR_PROFILE_INIT(void)
{
}

static void KD_SENSOR_PROFILE(char *tag)
{
}
#endif

static DEFINE_SPINLOCK(imgsensor_drv_lock);

static struct imgsensor_info_struct imgsensor_info = {
	/* record sensor id defined in kd_imgsensor.h */
	.sensor_id = IMX481_SENSOR_ID,

	/* checksum value for Camera Auto Test */
	.checksum_value = 0x5f9bf7a1,

	.pre = {/*data rate 1836 Mbps/lane */
		.pclk = 580000000,
		.linelength = 5120,
		.framelength = 3776,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2328,
		.grabwindow_height = 1746,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 734400000,
		.max_framerate = 300,
	},
	.cap = {/*data rate 1836 Mbps/lane */
		.pclk = 580000000,
		.linelength = 5120,
		.framelength = 3776,
		.startx = 0,
		.starty = 2,
		.grabwindow_width = 4656,
		.grabwindow_height = 3492,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 734400000,
		.max_framerate = 300,
	},
	.normal_video = {/*data rate 1836 Mbps/lane */
		.pclk = 580000000,
		.linelength = 5120,
		.framelength = 3776,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4656,
		.grabwindow_height = 2608,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 734400000,
		.max_framerate = 300,
	},
	.hs_video = {/*data rate 1840 Mbps/lane */
		.pclk = 580000000,
		.linelength = 2560,
		.framelength = 1888,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1920,
		.grabwindow_height = 1080,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 736000000,
		.max_framerate = 1200,
	},
	.slim_video = {/*data rate 1836 Mbps/lane */
		.pclk = 580000000,
		.linelength = 5120,
		.framelength = 3776,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2328,
		.grabwindow_height = 1746,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 734400000,
		.max_framerate = 300,
	},

	.margin = 18,		/* sensor framelength & shutter margin */
	.min_shutter = 4,	/* min shutter */
	.min_gain = 64,
	.max_gain = 1024,
	.min_gain_iso = 100,
	.gain_step = 1,
	.gain_type = 0,

	/* max framelength by sensor register's limitation */
	.max_frame_length = 0xffff,

	/* shutter delay frame for AE cycle,
	 * 2 frame with ispGain_delay-shut_delay=2-0=2
	 */
	.ae_shut_delay_frame = 0,

	/* sensor gain delay frame for AE cycle,
	 * 2 frame with ispGain_delay-sensor_gain_delay=2-0=2
	 */
	.ae_sensor_gain_delay_frame = 0,

	.ae_ispGain_delay_frame = 2,	/* isp gain delay frame for AE cycle */
	.ihdr_support = 0,	/* 1, support; 0,not support */
	.ihdr_le_firstline = 0,	/* 1,le first ; 0, se first */
	.temperature_support = 1,	/* 1, support; 0,not support */
	.sensor_mode_num = 5,	/* support sensor mode num */
	.frame_time_delay_frame = 3,
	.cap_delay_frame = 2,	/* enter capture delay frame num */
	.pre_delay_frame = 2,	/* enter preview delay frame num */
	.video_delay_frame = 2,	/* enter video delay frame num */
	.hs_video_delay_frame = 3,/* enter high speed video  delay frame num */
	.slim_video_delay_frame = 3,/* enter slim video delay frame num */

	.isp_driving_current = ISP_DRIVING_2MA,	/* mclk driving current */

	/* sensor_interface_type */
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,

	/* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
	.mipi_sensor_type = MIPI_OPHY_NCSI2,

	/* 0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL */
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,

	/* sensor output first pixel color */
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R,
	.mclk = 24,/* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
	.mipi_lane_num = SENSOR_MIPI_4_LANE,	/* mipi lane num */

	/* record sensor support all write id addr,
	 * only supprt 4must end with 0xff
	 */
	.i2c_addr_table = {0x20, 0xff},
	.i2c_speed = 1000,	/* i2c read/write speed */
};

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,	/* mirrorflip information */
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x3D0,	/* current shutter */
	.gain = 0x100,		/* current gain */
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */

	/* full size current fps : 24fps for PIP, 30fps for Normal or ZSD */
	.current_fps = 300,

	/* auto flicker enable: KAL_FALSE for disable auto flicker,
	 * KAL_TRUE for enable auto flicker
	 */
	.autoflicker_en = KAL_FALSE,

	.test_pattern = 0,

	/* current scenario id */
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.hdr_mode = 0,/* sensor need support LE, SE with HDR feature */
	.i2c_write_id = 0x20,/* record current sensor's i2c write id */
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[10] = {
	{4656, 3496, 0, 0, 4656, 3496, 2328, 1748,
	0000, 0000, 2328, 1748, 0, 0, 2328, 1746},	/*Preview*/
	{4656, 3496, 0, 0, 4656, 3496, 4656, 3496,
	0000, 0000, 4656, 3496, 0, 2, 4656, 3492},	/*Capture*/
	{4656, 3496, 0, 444, 4656, 2608, 4656, 2608,
	0000, 0000, 4656, 2608, 0, 0, 4656, 2608},	/*Video*/
	{4656, 3496, 0, 664, 4656, 2160, 2328, 1080,
	204, 0000, 1920, 1080, 0, 0, 1920, 1080},	/*hs-video*/
	{4656, 3496, 0, 0, 4656, 3496, 2328, 1748,
	0000, 0000, 2328, 1748, 0, 0, 2328, 1746},	/*slim video*/
};

static struct SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[4] = {
	/* Preview mode setting */
	{0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x0918, 0x06d2, 0x00, 0x00, 0x0000, 0x0000,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0000, 0x0000},
	/* Capture mode setting */
	{0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x1230, 0x0da4, 0x00, 0x00, 0x0000, 0x0000,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0000, 0x0000},
	/* Video mode setting */
	{0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x1230, 0x0a30, 0x00, 0x00, 0x0000, 0x0000,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0000, 0x0000},
	/* hs & slim mode setting */
	{0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x780, 0x438, 0x00, 0x00, 0x0000, 0x0000,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0000, 0x0000}
};

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;

	char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(
		pu_send_cmd, 2, (u8 *) &get_byte, 1, imgsensor.i2c_write_id);

	return get_byte;
}

static int write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[3] = {
		(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF) };

	return iWriteRegI2CTiming(
	    pu_send_cmd, 3, imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
}

static void set_dummy(void)
{
	pr_debug("frame_length = %d, line_length = %d\n",
	    imgsensor.frame_length,
	    imgsensor.line_length);

	write_cmos_sensor(0x0104, 0x01);

	write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
	write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
	write_cmos_sensor(0x0342, imgsensor.line_length >> 8);
	write_cmos_sensor(0x0343, imgsensor.line_length & 0xFF);

	write_cmos_sensor(0x0104, 0x00);
} /* set_dummy */

static kal_uint32 return_lot_id_from_otp(void)
{
	kal_uint16 val = 0;
	int i = 0;

	if (write_cmos_sensor(0x0a02, 0x1B) < 0) {
		pr_debug("read otp fail Err!\n");
		return 0;
	}
	write_cmos_sensor(0x0a00, 0x01);

	for (i = 0; i < 3; i++) {
		val = read_cmos_sensor(0x0A01);
		if ((val & 0x01) == 0x01)
			break;
		mDELAY(3);
	}
	if (i == 3) {
		pr_debug("read otp fail Err!\n");
		return 0;
	}
	/* Check with Mud'mail */
	return((read_cmos_sensor(0x0A22) << 4) | read_cmos_sensor(0x0A23) >> 4);
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;
	/* unsigned long flags; */

	pr_debug("framerate = %d, min framelength should enable %d\n",
			framerate,
			min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length)
	    ? frame_length : imgsensor.min_frame_length;

	imgsensor.dummy_line =
	    imgsensor.frame_length - imgsensor.min_frame_length;

	/* dummy_line = frame_length - imgsensor.min_frame_length; */
	/* if (dummy_line < 0) */
	/* imgsensor.dummy_line = 0; */
	/* else */
	/* imgsensor.dummy_line = dummy_line; */
	/* imgsensor.frame_length = frame_length + imgsensor.dummy_line; */
	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line
			= imgsensor.frame_length
			- imgsensor.min_frame_length;

	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
} /* set_max_framerate */

/************************************************************************
 * FUNCTION
 *    set_shutter
 *
 * DESCRIPTION
 *    This function set e-shutter of sensor to change exposure time.
 *
 * PARAMETERS
 *    iShutter : exposured lines
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 ************************************************************************/
#define MAX_CIT_LSHIFT 7
static void set_shutter(kal_uint32 shutter)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	kal_uint16 l_shift = 1;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);


	/* if shutter bigger than frame_length, extend frame length first */
	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;

	spin_unlock(&imgsensor_drv_lock);

	shutter = (shutter < imgsensor_info.min_shutter)
		? imgsensor_info.min_shutter
		: shutter;

	/* long expsoure */
	if (shutter > (imgsensor_info.max_frame_length
		       - imgsensor_info.margin)) {

		for (l_shift = 1; l_shift < MAX_CIT_LSHIFT; l_shift++) {
			if ((shutter >> l_shift)
			    < (imgsensor_info.max_frame_length
			       - imgsensor_info.margin))

				break;
		}
		if (l_shift > MAX_CIT_LSHIFT) {
			pr_debug(
			    "Unable to set such a long exposure %d, set to max\n",
			    shutter);

			l_shift = MAX_CIT_LSHIFT;
		}
		shutter = shutter >> l_shift;
		imgsensor.frame_length = shutter + imgsensor_info.margin;
		write_cmos_sensor(0x3100, read_cmos_sensor(0x3100)
						| (l_shift & 0x7));

		/* pr_debug("0x3028 0x%x\n", read_cmos_sensor(0x3028)); */

	} else {
		write_cmos_sensor(0x3100, read_cmos_sensor(0x3100) & 0xf8);
	}

	shutter	= (shutter > (imgsensor_info.max_frame_length
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
		else if (realtime_fps >= 237 && realtime_fps <= 243)
			set_max_framerate(236, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			/* Extend frame length */
			write_cmos_sensor(0x0104, 0x01);
			write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
			write_cmos_sensor(0x0341,
				imgsensor.frame_length & 0xFF);

			write_cmos_sensor(0x0104, 0x00);
		}
	} else {
		/* Extend frame length */
		write_cmos_sensor(0x0104, 0x01);
		write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
		write_cmos_sensor(0x0104, 0x00);
	}

	/* Update Shutter */
	write_cmos_sensor(0x0104, 0x01);
	write_cmos_sensor(0x0202, (shutter >> 8) & 0xFF);
	write_cmos_sensor(0x0203, shutter & 0xFF);
	write_cmos_sensor(0x0104, 0x00);
	pr_debug(
	    "Exit! shutter =%d, framelength =%d\n",
	    shutter,
	    imgsensor.frame_length);

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

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	/* OV Recommend Solution */
	/*if shutter bigger than frame_length,
	 *should extend frame length first
	 */
	spin_lock(&imgsensor_drv_lock);
	/* Change frame time */
	if (frame_length > 1)
		dummy_line = frame_length - imgsensor.frame_length;

	imgsensor.frame_length = imgsensor.frame_length + dummy_line;

	if (shutter > imgsensor.frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;

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
			write_cmos_sensor(0x0104, 0x01);
			write_cmos_sensor(0x0340,
					imgsensor.frame_length >> 8);
			write_cmos_sensor(0x0341,
					imgsensor.frame_length & 0xFF);
			write_cmos_sensor(0x0104, 0x00);
		}
	} else {
		/* Extend frame length */
		write_cmos_sensor(0x0104, 0x01);
		write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
		write_cmos_sensor(0x0104, 0x00);
	}

	/* Update Shutter */
	write_cmos_sensor(0x0104, 0x01);
	if (auto_extend_en)
		write_cmos_sensor(0x0350, 0x01); /* Enable auto extend */
	else
		write_cmos_sensor(0x0350, 0x00); /* Disable auto extend */
	write_cmos_sensor(0x0202, (shutter >> 8) & 0xFF);
	write_cmos_sensor(0x0203, shutter  & 0xFF);
	write_cmos_sensor(0x0104, 0x00);
	pr_debug(
		"Exit! shutter =%d, framelength =%d/%d, dummy_line=%d, auto_extend=%d\n",
		shutter, imgsensor.frame_length, frame_length, dummy_line,
		read_cmos_sensor(0x0350));

}	/* set_shutter_frame_length */

static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 reg_gain = 0x0;

	reg_gain = 1024 - (1024 * 64) / gain;
	return (kal_uint16) reg_gain;
}

/************************************************************************
 * FUNCTION
 *    set_gain
 *
 * DESCRIPTION
 *    This function is to set global gain to sensor.
 *
 * PARAMETERS
 *    iGain : sensor global gain(base: 0x40)
 *
 * RETURNS
 *    the actually gain set to sensor.
 *
 * GLOBALS AFFECTED
 *
 ************************************************************************/
static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain;

	/*  */
	if (gain < imgsensor_info.min_gain || gain > imgsensor_info.max_gain) {
		pr_debug("Error gain setting");

		if (gain < imgsensor_info.min_gain)
			gain = imgsensor_info.min_gain;
		else
			gain = imgsensor_info.max_gain;
	}

	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	pr_debug("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

	write_cmos_sensor(0x0104, 0x01);
	/* Global analog Gain for Long expo */
	write_cmos_sensor(0x0204, (reg_gain >> 8) & 0xFF);
	write_cmos_sensor(0x0205, reg_gain & 0xFF);
	write_cmos_sensor(0x0104, 0x00);


	return gain;
} /* set_gain */

#if 0
static void set_mirror_flip(kal_uint8 image_mirror)
{
	switch (image_mirror) {
	case IMAGE_NORMAL:
		write_cmos_sensor(0x0101, image_mirror);
		break;

	case IMAGE_V_MIRROR:
		write_cmos_sensor(0x0101, image_mirror);
		break;

	case IMAGE_H_MIRROR:
		write_cmos_sensor(0x0101, image_mirror);
		break;

	case IMAGE_HV_MIRROR:
		write_cmos_sensor(0x0101, image_mirror);
		break;
	}
}
#endif

/************************************************************************
 * FUNCTION
 *    night_mode
 *
 * DESCRIPTION
 *    This function night mode of sensor.
 *
 * PARAMETERS
 *    bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 ************************************************************************/
static void night_mode(kal_bool enable)
{
	/*No Need to implement this function*/
} /* night_mode */


#define MULTI_WRITE 1

#if MULTI_WRITE
#define I2C_BUFFER_LEN 765	/* trans# max is 255, each 3 bytes */
#else
#define I2C_BUFFER_LEN 3

#endif
static kal_uint16 imx481_table_write_cmos_sensor(
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

		if ((I2C_BUFFER_LEN - tosend) < 3
		    || len == IDX
		    || addr != addr_last) {

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

kal_uint16 addr_data_pair_init_imx481[] = {
	/* External Clock Setting */
	0x0136, 0x18,
	0x0137, 0x00,
	/* Register version */
	0x3C7E, 0x02,
	0x3C7F, 0x07,
	/* Global Setting */
	0x3F7F, 0x01,
	0x531C, 0x01,
	0x531D, 0x02,
	0x531E, 0x04,
	0x5928, 0x00,
	0x5929, 0x2F,
	0x592A, 0x00,
	0x592B, 0x85,
	0x592C, 0x00,
	0x592D, 0x32,
	0x592E, 0x00,
	0x592F, 0x88,
	0x5930, 0x00,
	0x5931, 0x3D,
	0x5932, 0x00,
	0x5933, 0x93,
	0x5938, 0x00,
	0x5939, 0x24,
	0x593A, 0x00,
	0x593B, 0x7A,
	0x593C, 0x00,
	0x593D, 0x24,
	0x593E, 0x00,
	0x593F, 0x7A,
	0x5940, 0x00,
	0x5941, 0x2F,
	0x5942, 0x00,
	0x5943, 0x85,
	0x5F0E, 0x6E,
	0x5F11, 0xC6,
	0x5F17, 0x5E,
	0x7990, 0x01,
	0x7993, 0x5D,
	0x7994, 0x5D,
	0x7995, 0xA1,
	0x799A, 0x01,
	0x799D, 0x00,
	0x8169, 0x01,
	0x8359, 0x01,
	0x88C7, 0x00,
	0x88D4, 0x03,
	0x9302, 0x1E,
	0x9306, 0x1F,
	0x930A, 0x26,
	0x930E, 0x23,
	0x9312, 0x23,
	0x9316, 0x2C,
	0x9317, 0x19,
	0x9960, 0x00,
	0x9963, 0x64,
	0x9964, 0x50,
	0xA391, 0x04,
	0xB046, 0x01,
	0xB048, 0x01,
	/* Image Quality adjustment setting */
	0x8145, 0x00,
	0x8146, 0x04,
	0x8341, 0x00,
	0x8343, 0x08,
	0xA801, 0x00,
	0xA802, 0x00,
	0xA903, 0x00,
	0xA905, 0x00,
	0xA909, 0x00,
	0xA90B, 0x00,
	0xA925, 0x02,
	0xA927, 0x02,
	0xA929, 0x02,
	0xA92B, 0x00,
	0xA92D, 0x00,
	0xA92F, 0x00,
	0xA933, 0x00,
	0xA935, 0x00,
	0xA939, 0x00,
	0xA93B, 0x00,
	0xA955, 0x02,
	0xA957, 0x02,
	0xA959, 0x02,
	0xA95B, 0x00,
	0xA95D, 0x00,
	0xA95F, 0x00,
	0xA963, 0x00,
	0xA965, 0x00,
	0xA969, 0x00,
	0xA96B, 0x00,
	0xA985, 0x02,
	0xA987, 0x02,
	0xA989, 0x02,
	0xA98B, 0x00,
	0xA98D, 0x00,
	0xA98F, 0x00,
	0xAA06, 0x3F,
	0xAA07, 0x05,
	0xAA08, 0x04,
	0xAA12, 0x3F,
	0xAA13, 0x04,
	0xAA14, 0x03,
	0xAB55, 0x02,
	0xAB57, 0x01,
	0xAB59, 0x01,
	0xABB4, 0x00,
	0xABB5, 0x01,
	0xABB6, 0x00,
	0xABB7, 0x01,
	0xABB8, 0x00,
	0xABB9, 0x01,
	0xAE08, 0x00,
	0xAE0B, 0x00,
	0xAE0E, 0x00,
	0xAE11, 0x00,
	0xAE14, 0x00,
	0xAE1A, 0x00,
	0xAE2E, 0x00,
	0xAE31, 0x00,
	0xAE37, 0x00,
	0xAE40, 0x00,
	0xAE54, 0x00,
	0xAE57, 0x00,
	0xAE5D, 0x00,
	0xAE66, 0x00,
};

static void sensor_init(void)
{
	pr_debug("02E\n");
	imx481_table_write_cmos_sensor(addr_data_pair_init_imx481,
	    sizeof(addr_data_pair_init_imx481)/sizeof(kal_uint16));

	/*enable temperature sensor, TEMP_SEN_CTL:*/
	write_cmos_sensor(0x0138, 0x01);
} /* sensor_init  */

kal_uint16 addr_data_pair_preview_imx481[] = {
	/* MIPI output setting */
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x03,
	/* Line Length PCK Setting */
	0x0342, 0x14,
	0x0343, 0x00,
	/* Frame Length Lines Setting */
	0x0340, 0x0E,
	0x0341, 0xC0,
	/* ROI Setting */
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x12,
	0x0349, 0x2F,
	0x034A, 0x0D,
	0x034B, 0xA7,
	/* Mode Setting */
	0x0381, 0x01,
	0x0383, 0x01,
	0x0385, 0x01,
	0x0387, 0x01,
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x0A,
	0x3F4C, 0x01,
	0x3F4D, 0x03,
	/* Digital Crop & Scaling */
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x09,
	0x040D, 0x18,
	0x040E, 0x06,
	0x040F, 0xD4,
	/* Output Size Setting */
	0x034C, 0x09,
	0x034D, 0x18,
	0x034E, 0x06,
	0x034F, 0xD4,
	/* Clock Setting */
	0x0301, 0x06,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x22,
	0x030B, 0x01,
	0x030D, 0x02,
	0x030E, 0x00,
	0x030F, 0x99,
	0x0310, 0x01,
	/* PDAF Setting */
	0x3E20, 0x01,
	0x3E37, 0x00,
	0x3E3B, 0x00,
	/* Other Setting */
	0x3F78, 0x02,
	0x3F79, 0x0A,
	0x3FFE, 0x00,
	0x3FFF, 0x0F,
	0x5F0A, 0xB2,
	0xA828, 0x02,
	0xA829, 0x02,
	0xA84F, 0x01,
	0xA850, 0x01,
	0xB2DF, 0x12,
	0xB2E5, 0x06,
	/* Integration Setting */
	0x0202, 0x0E,
	0x0203, 0xAE,
	/* Gain Setting */
	0x0204, 0x00,
	0x0205, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
};

static void preview_setting(void)
{
	imx481_table_write_cmos_sensor(
	addr_data_pair_preview_imx481,
	sizeof(addr_data_pair_preview_imx481) / sizeof(kal_uint16));
} /* preview_setting */

kal_uint16 addr_data_pair_capture_imx481[] = {
	/* MIPI output setting */
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x03,
	/* Line Length PCK Setting */
	0x0342, 0x14,
	0x0343, 0x00,
	/* Frame Length Lines Setting */
	0x0340, 0x0E,
	0x0341, 0xC0,
	/* ROI Setting */
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x12,
	0x0349, 0x2F,
	0x034A, 0x0D,
	0x034B, 0xA7,
	/* Mode Setting */
	0x0381, 0x01,
	0x0383, 0x01,
	0x0385, 0x01,
	0x0387, 0x01,
	0x0900, 0x00,
	0x0901, 0x11,
	0x0902, 0x0A,
	0x3F4C, 0x01,
	0x3F4D, 0x01,
	/* Digital Crop & Scaling */
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x12,
	0x040D, 0x30,
	0x040E, 0x0D,
	0x040F, 0xA8,
	/* Output Size Setting */
	0x034C, 0x12,
	0x034D, 0x30,
	0x034E, 0x0D,
	0x034F, 0xA8,
	/* Clock Setting */
	0x0301, 0x06,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x22,
	0x030B, 0x01,
	0x030D, 0x02,
	0x030E, 0x00,
	0x030F, 0x99,
	0x0310, 0x01,
	/* PDAF Setting */
	0x3E20, 0x01,
	0x3E37, 0x00,
	0x3E3B, 0x00,
	/* Other Setting */
	0x3F78, 0x02,
	0x3F79, 0x0A,
	0x3FFE, 0x00,
	0x3FFF, 0x18,
	0x5F0A, 0xB2,
	0xA828, 0x00,
	0xA829, 0x00,
	0xA84F, 0x00,
	0xA850, 0x00,
	0xB2DF, 0x00,
	0xB2E5, 0x00,
	/* Integration Setting */
	0x0202, 0x0E,
	0x0203, 0xAE,
	/* Gain Setting */
	0x0204, 0x00,
	0x0205, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
};

static void capture_setting(kal_uint16 currefps)
{
	imx481_table_write_cmos_sensor(addr_data_pair_capture_imx481,
		sizeof(addr_data_pair_capture_imx481) / sizeof(kal_uint16));
}

kal_uint16 addr_data_pair_video_imx481[] = {
	/* MIPI output setting */
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x03,
	/* Line Length PCK Setting */
	0x0342, 0x14,
	0x0343, 0x00,
	/* Frame Length Lines Setting */
	0x0340, 0x0E,
	0x0341, 0xC0,
	/* ROI Setting */
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x12,
	0x0349, 0x2F,
	0x034A, 0x0D,
	0x034B, 0xA7,
	/* Mode Setting */
	0x0381, 0x01,
	0x0383, 0x01,
	0x0385, 0x01,
	0x0387, 0x01,
	0x0900, 0x00,
	0x0901, 0x11,
	0x0902, 0x0A,
	0x3F4C, 0x01,
	0x3F4D, 0x01,
	/* Digital Crop & Scaling */
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x01,
	0x040B, 0xBC,
	0x040C, 0x12,
	0x040D, 0x30,
	0x040E, 0x0A,
	0x040F, 0x30,
	/* Output Size Setting */
	0x034C, 0x12,
	0x034D, 0x30,
	0x034E, 0x0A,
	0x034F, 0x30,
	/* Clock Setting */
	0x0301, 0x06,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x22,
	0x030B, 0x01,
	0x030D, 0x02,
	0x030E, 0x00,
	0x030F, 0x99,
	0x0310, 0x01,
	/* PDAF Setting */
	0x3E20, 0x01,
	0x3E37, 0x00,
	0x3E3B, 0x00,
	/* Other Setting */
	0x3F78, 0x02,
	0x3F79, 0x0A,
	0x3FFE, 0x00,
	0x3FFF, 0x18,
	0x5F0A, 0xB2,
	0xA828, 0x00,
	0xA829, 0x00,
	0xA84F, 0x00,
	0xA850, 0x00,
	0xB2DF, 0x00,
	0xB2E5, 0x00,
	/* Integration Setting */
	0x0202, 0x0E,
	0x0203, 0xAE,
	/* Gain Setting */
	0x0204, 0x00,
	0x0205, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
};

static void normal_video_setting(kal_uint16 currefps)
{
	pr_debug("E! %s:%d\n", __func__, currefps);
	imx481_table_write_cmos_sensor(addr_data_pair_video_imx481,
		sizeof(addr_data_pair_video_imx481) / sizeof(kal_uint16));
}

kal_uint16 addr_data_pair_hs_video_imx481[] = {	/*720 120fps */
	/* MIPI output setting */
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x03,
	/* Line Length PCK Setting */
	0x0342, 0x0A,
	0x0343, 0x00,
	/* Frame Length Lines Setting */
	0x0340, 0x07,
	0x0341, 0x60,
	/* ROI Setting */
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x02,
	0x0347, 0x98,
	0x0348, 0x12,
	0x0349, 0x2F,
	0x034A, 0x0B,
	0x034B, 0x07,
	/* Mode Setting */
	0x0381, 0x01,
	0x0383, 0x01,
	0x0385, 0x01,
	0x0387, 0x01,
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x0A,
	0x3F4C, 0x01,
	0x3F4D, 0x01,
	/* Digital Crop & Scaling */
	0x0408, 0x00,
	0x0409, 0xCC,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x07,
	0x040D, 0x80,
	0x040E, 0x04,
	0x040F, 0x38,
	/* Output Size Setting */
	0x034C, 0x07,
	0x034D, 0x80,
	0x034E, 0x04,
	0x034F, 0x38,
	/* Clock Setting */
	0x0301, 0x06,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x22,
	0x030B, 0x01,
	0x030D, 0x06,
	0x030E, 0x01,
	0x030F, 0xCC,
	0x0310, 0x01,
	/* PDAF Setting */
	0x3E20, 0x01,
	0x3E37, 0x00,
	0x3E3B, 0x00,
	/* Other Setting */
	0x3F78, 0x02,
	0x3F79, 0xA8,
	0x3FFE, 0x00,
	0x3FFF, 0x88,
	0x5F0A, 0xB2,
	0xA828, 0x02,
	0xA829, 0x02,
	0xA84F, 0x01,
	0xA850, 0x01,
	0xB2DF, 0x12,
	0xB2E5, 0x06,
	/* Integration Setting */
	0x0202, 0x07,
	0x0203, 0x4E,
	/* Gain Setting */
	0x0204, 0x00,
	0x0205, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
};

static void hs_video_setting(void)
{
	imx481_table_write_cmos_sensor(addr_data_pair_hs_video_imx481,
		sizeof(addr_data_pair_hs_video_imx481) / sizeof(kal_uint16));
}

kal_uint16 addr_data_pair_slim_video_imx481[] = {
	/* MIPI output setting */
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x03,
	/* Line Length PCK Setting */
	0x0342, 0x14,
	0x0343, 0x00,
	/* Frame Length Lines Setting */
	0x0340, 0x0E,
	0x0341, 0xC0,
	/* ROI Setting */
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x12,
	0x0349, 0x2F,
	0x034A, 0x0D,
	0x034B, 0xA7,
	/* Mode Setting */
	0x0381, 0x01,
	0x0383, 0x01,
	0x0385, 0x01,
	0x0387, 0x01,
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x0A,
	0x3F4C, 0x01,
	0x3F4D, 0x03,
	/* Digital Crop & Scaling */
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x09,
	0x040D, 0x18,
	0x040E, 0x06,
	0x040F, 0xD4,
	/* Output Size Setting */
	0x034C, 0x09,
	0x034D, 0x18,
	0x034E, 0x06,
	0x034F, 0xD4,
	/* Clock Setting */
	0x0301, 0x06,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x22,
	0x030B, 0x01,
	0x030D, 0x02,
	0x030E, 0x00,
	0x030F, 0x99,
	0x0310, 0x01,
	/* PDAF Setting */
	0x3E20, 0x01,
	0x3E37, 0x00,
	0x3E3B, 0x00,
	/* Other Setting */
	0x3F78, 0x02,
	0x3F79, 0x0A,
	0x3FFE, 0x00,
	0x3FFF, 0x0F,
	0x5F0A, 0xB2,
	0xA828, 0x02,
	0xA829, 0x02,
	0xA84F, 0x01,
	0xA850, 0x01,
	0xB2DF, 0x12,
	0xB2E5, 0x06,
	/* Integration Setting */
	0x0202, 0x0E,
	0x0203, 0xAE,
	/* Gain Setting */
	0x0204, 0x00,
	0x0205, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
};

static void slim_video_setting(void)
{
	imx481_table_write_cmos_sensor(addr_data_pair_slim_video_imx481,
	    sizeof(addr_data_pair_slim_video_imx481) / sizeof(kal_uint16));
}

static kal_uint32 set_test_pattern_mode(kal_uint32 modes,
	struct SET_SENSOR_PATTERN_SOLID_COLOR *pdata)
{
	kal_uint16 Color_R, Color_Gr, Color_Gb, Color_B;

	pr_debug("set_test_pattern enum: %d\n", modes);

	if (modes) {
		write_cmos_sensor(0x0601, modes);
		if (modes == 1 && (pdata != NULL)) { //Solid Color
			pr_debug("R=0x%x,Gr=0x%x,B=0x%x,Gb=0x%x",
				pdata->COLOR_R, pdata->COLOR_Gr, pdata->COLOR_B, pdata->COLOR_Gb);
			Color_R = (pdata->COLOR_R >> 22) & 0x3FF; //10bits depth color
			Color_Gr = (pdata->COLOR_Gr >> 22) & 0x3FF;
			Color_B = (pdata->COLOR_B >> 22) & 0x3FF;
			Color_Gb = (pdata->COLOR_Gb >> 22) & 0x3FF;
			write_cmos_sensor(0x0602, (Color_R >> 8) & 0x3);
			write_cmos_sensor(0x0603, Color_R & 0xFF);
			write_cmos_sensor(0x0604, (Color_Gr >> 8) & 0x3);
			write_cmos_sensor(0x0605, Color_Gr & 0xFF);
			write_cmos_sensor(0x0606, (Color_B >> 8) & 0x3);
			write_cmos_sensor(0x0607, Color_B & 0xFF);
			write_cmos_sensor(0x0608, (Color_Gb >> 8) & 0x3);
			write_cmos_sensor(0x0609, Color_Gb & 0xFF);
		}
	}
	else
		write_cmos_sensor(0x0601, 0x00); /*No pattern*/

	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = modes;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

/************************************************************************
 * FUNCTION
 *    get_imgsensor_id
 *
 * DESCRIPTION
 *    This function get the sensor ID
 *
 * PARAMETERS
 *    *sensorID : return the sensor ID
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 ************************************************************************/
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = return_lot_id_from_otp();
			if (*sensor_id == imgsensor_info.sensor_id) {
				pr_debug("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}
			pr_debug(
			    "Read sensor id fail, write id: 0x%x, id: 0x%x\n",
			    imgsensor.i2c_write_id,
			    *sensor_id);

			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	/* if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF */
	if (*sensor_id != imgsensor_info.sensor_id) {
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}

	return ERROR_NONE;
}


/************************************************************************
 * FUNCTION
 *    open
 *
 * DESCRIPTION
 *    This function initialize the registers of CMOS sensor
 *
 * PARAMETERS
 *    None
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 ************************************************************************/
static kal_uint32 open(void)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint32 sensor_id = 0;

	pr_debug("IMX481, MIPI 4LANE\n");
	pr_debug(
	 "preview 2328*1746@30fps; video 4656*3496@30fps; capture 13M@30fps\n");


	KD_SENSOR_PROFILE_INIT();

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_lot_id_from_otp();
			if (sensor_id == imgsensor_info.sensor_id) {
				pr_debug(
				    "i2c write id: 0x%x, sensor id: 0x%x\n",
				    imgsensor.i2c_write_id, sensor_id);
				break;
			}
			pr_debug(
			    "Read sensor id fail, write id: 0x%x, id: 0x%x\n",
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

	KD_SENSOR_PROFILE("open_1");
	/* initail sequence write in  */
	sensor_init();

	KD_SENSOR_PROFILE("sensor_init");

	spin_lock(&imgsensor_drv_lock);

	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.hdr_mode = 0;
	imgsensor.test_pattern = 0;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);

	KD_SENSOR_PROFILE("open_2");
	return ERROR_NONE;
} /* open */


/************************************************************************
 * FUNCTION
 *    close
 *
 * DESCRIPTION
 *
 *
 * PARAMETERS
 *    None
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 ************************************************************************/
static kal_uint32 close(void)
{
	write_cmos_sensor(0x0100, 0x00);/*stream off */
	return ERROR_NONE;
} /* close */


/************************************************************************
 * FUNCTION
 * preview
 *
 * DESCRIPTION
 *    This function start the sensor preview.
 *
 * PARAMETERS
 *    *image_window : address pointer of pixel numbers in one period of HSYNC
 *  *sensor_config_data : address pointer of line numbers in one period of VSYNC
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 ************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("1E\n");

	KD_SENSOR_PROFILE_INIT();

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	/* imgsensor.video_mode = KAL_FALSE; */
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	KD_SENSOR_PROFILE("pre_lock");
	preview_setting();
	KD_SENSOR_PROFILE("pre_setting");
	return ERROR_NONE;
} /* preview */

/************************************************************************
 * FUNCTION
 *    capture
 *
 * DESCRIPTION
 *    This function setup the CMOS sensor in capture MY_OUTPUT mode
 *
 * PARAMETERS
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 ************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("E\n");

	KD_SENSOR_PROFILE_INIT();


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

	KD_SENSOR_PROFILE("cap_lock");
	capture_setting(imgsensor.current_fps);	/*Full mode */
	KD_SENSOR_PROFILE("cap_setting");

	return ERROR_NONE;
} /* capture */

static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			       MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("E\n");

	KD_SENSOR_PROFILE_INIT();

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	/* imgsensor.current_fps = 300; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	KD_SENSOR_PROFILE("nv_lock");
	normal_video_setting(imgsensor.current_fps);
	KD_SENSOR_PROFILE("nv_setting");

	return ERROR_NONE;
} /* normal_video */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	KD_SENSOR_PROFILE_INIT();

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	/* imgsensor.video_mode = KAL_TRUE; */
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	KD_SENSOR_PROFILE("hv_lock");
	hs_video_setting();
	KD_SENSOR_PROFILE("hv_setting");

	return ERROR_NONE;
} /* hs_video */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	KD_SENSOR_PROFILE_INIT();

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

	KD_SENSOR_PROFILE("sv_lock");
	slim_video_setting();
	KD_SENSOR_PROFILE("sv_setting");

	return ERROR_NONE;
} /* slim_video */

static kal_uint32 get_resolution(
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT(*sensor_resolution))
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
} /* get_resolution */

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			   MSDK_SENSOR_INFO_STRUCT *sensor_info,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	/*pr_debug("scenario_id = %d\n", scenario_id); */

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;

	/* not use */
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;

	/* inverse with datasheet */
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4;	/* not use */
	sensor_info->SensorResetActiveHigh = FALSE;	/* not use */
	sensor_info->SensorResetDelayCount = 5;	/* not use */

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

	sensor_info->SensorMasterClockSwitch = 0;	/* not use */
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

	/*0: NO PDAF, 1: PDAF Raw Data mode, 2:PDAF VC mode */
	sensor_info->PDAF_Support = PDAF_SUPPORT_NA;

	sensor_info->SensorHorFOV = 63;
	sensor_info->SensorVerFOV = 49;

	sensor_info->HDR_Support = 0;/*0: NO HDR, 1: iHDR, 2:mvHDR, 3:zHDR */

	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3;	/* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2;	/* not use */
	sensor_info->SensorPixelClockCount = 3;	/* not use */
	sensor_info->SensorDataLatchCount = 2;	/* not use */

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;	/* 0 is default 1x */
	sensor_info->SensorHightSampling = 0;	/* 0 is default 1x */
	sensor_info->SensorPacketECCOrder = 1;

	sensor_info->SensorMIPIDeskew = 1;

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
	default:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	}

	return ERROR_NONE;
} /* get_info */

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
	default:
		pr_debug("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
} /* control */

/* This Function not used after ROME */
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
	if (enable)		/* enable auto flicker */
		imgsensor.autoflicker_en = KAL_TRUE;
	else			/* Cancel Auto flick */
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
		else {
			/*No need to set*/
			pr_debug("frame_length %d < shutter %d",
				imgsensor.frame_length, imgsensor.shutter);
		}
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

		imgsensor.frame_length
			= imgsensor_info.normal_video.framelength
			+ imgsensor.dummy_line;

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
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (imgsensor.current_fps
			!= imgsensor_info.cap.max_framerate)
			pr_debug(
				"Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
				framerate,
				imgsensor_info.cap.max_framerate / 10);

		frame_length
			= imgsensor_info.cap.pclk
			/ framerate * 10
			/ imgsensor_info.cap.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line
			= (frame_length > imgsensor_info.cap.framelength)
			? (frame_length - imgsensor_info.cap.framelength)
			: 0;

		imgsensor.frame_length
			= imgsensor_info.cap.framelength
			+ imgsensor.dummy_line;

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
		else {
			/*No need to set*/
			pr_debug("frame_length %d < shutter %d",
				imgsensor.frame_length, imgsensor.shutter);
		}
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
		else {
			/*No need to set*/
			pr_debug("frame_length %d < shutter %d",
				imgsensor.frame_length, imgsensor.shutter);
		}
		break;

	/* coding with  preview scenario by default */
	default:
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
			= imgsensor_info.pre.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		else {
			/*No need to set*/
			pr_debug("frame_length %d < shutter %d",
				imgsensor.frame_length, imgsensor.shutter);
		}
		pr_debug("error scenario_id = %d, we use preview scenario\n",
			scenario_id);

		break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	/*pr_debug("scenario_id = %d\n", scenario_id); */

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


static kal_uint32 imx481_awb_gain(struct SET_SENSOR_AWB_GAIN *pSetSensorAWB)
{
	UINT32 rgain_32, grgain_32, gbgain_32, bgain_32;

	pr_debug("%s\n", __func__);

	grgain_32 = (pSetSensorAWB->ABS_GAIN_GR << 8) >> 9;
	rgain_32 = (pSetSensorAWB->ABS_GAIN_R << 8) >> 9;
	bgain_32 = (pSetSensorAWB->ABS_GAIN_B << 8) >> 9;
	gbgain_32 = (pSetSensorAWB->ABS_GAIN_GB << 8) >> 9;

	pr_debug(
		"[%s] ABS_GAIN_GR:%d, grgain_32:%d\n, ABS_GAIN_R:%d, rgain_32:%d\n, ABS_GAIN_B:%d, bgain_32:%d,ABS_GAIN_GB:%d, gbgain_32:%d\n",
		__func__,
		pSetSensorAWB->ABS_GAIN_GR, grgain_32,
		pSetSensorAWB->ABS_GAIN_R, rgain_32,
		pSetSensorAWB->ABS_GAIN_B, bgain_32,
		pSetSensorAWB->ABS_GAIN_GB, gbgain_32);

	write_cmos_sensor(0x0b8e, (grgain_32 >> 8) & 0xFF);
	write_cmos_sensor(0x0b8f, grgain_32 & 0xFF);
	write_cmos_sensor(0x0b90, (rgain_32 >> 8) & 0xFF);
	write_cmos_sensor(0x0b91, rgain_32 & 0xFF);
	write_cmos_sensor(0x0b92, (bgain_32 >> 8) & 0xFF);
	write_cmos_sensor(0x0b93, bgain_32 & 0xFF);
	write_cmos_sensor(0x0b94, (gbgain_32 >> 8) & 0xFF);
	write_cmos_sensor(0x0b95, gbgain_32 & 0xFF);
	return ERROR_NONE;
}

static kal_uint32 get_sensor_temperature(void)
{
	UINT8 temperature;
	INT32 temperature_convert;

	temperature = read_cmos_sensor(0x013a);

	if (temperature >= 0x0 && temperature <= 0x4F)
		temperature_convert = temperature;
	else if (temperature >= 0x50 && temperature <= 0x7F)
		temperature_convert = 80;
	else if (temperature >= 0x80 && temperature <= 0xEC)
		temperature_convert = -20;
	else
		temperature_convert = (INT8) temperature;

	/* pr_debug("temp_c(%d), read_reg(%d)\n",*/
	/*	temperature_convert, temperature); */

	return temperature_convert;
}

static void check_stream_is_on(void)
{
	int i = 0;
	UINT32 framecnt;
	int timeout = (10000/imgsensor.current_fps)+1;

	for (i = 0; i < timeout; i++) {

		framecnt = read_cmos_sensor(0x0005);
		if (framecnt != 0xFF) {
			pr_debug("IMX481 stream is on, %d \\n", framecnt);
			break;
		}
		pr_debug("IMX481 stream is not on %d \\n", framecnt);
		mdelay(1);
	}
}

static kal_uint32 streaming_control(kal_bool enable)
{
	pr_debug("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
	if (enable) {
		write_cmos_sensor(0x0100, 0X01);
		check_stream_is_on();
	} else
		write_cmos_sensor(0x0100, 0x00);
	return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
				  UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	INT32 *feature_return_para_i32 = (INT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *)feature_para;
	struct SENSOR_VC_INFO_STRUCT *pvcinfo;
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	struct SET_SENSOR_AWB_GAIN *pSetSensorAWB =
		(struct SET_SENSOR_AWB_GAIN *) feature_para;

	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

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
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.pclk;
			break;
		}
		break;
#ifdef IMGSENSOR_MT6885
	case SENSOR_FEATURE_GET_OFFSET_TO_START_OF_EXPOSURE:
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1500000;
		break;
#endif
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
			= (imgsensor_info.hs_video.framelength << 16)
				+ imgsensor_info.slim_video.linelength;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.pre.framelength << 16)
				+ imgsensor_info.pre.linelength;
			break;
		}
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		night_mode((BOOL) (*feature_data));
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16) (*feature_data));
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		write_cmos_sensor(
			sensor_reg_data->RegAddr, sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData =
			read_cmos_sensor(sensor_reg_data->RegAddr);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:

		/* get the lens driver ID from EEPROM or
		 *just return LENS_DRIVER_ID_DO_NOT_CARE
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
		set_auto_flicker_mode(
		    (BOOL) (*feature_data_16),
		    *(feature_data_16 + 1));

		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario(
		    (enum MSDK_SCENARIO_ID_ENUM) *feature_data,
		    *(feature_data + 1));

		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario(
		    (enum MSDK_SCENARIO_ID_ENUM) *feature_data,
		    (MUINT32 *) (uintptr_t) (*(feature_data + 1)));

		break;
	case SENSOR_FEATURE_GET_PDAF_DATA:
		pr_debug("Please use EEPROM function\n");
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((UINT32)*feature_data,
		(struct SET_SENSOR_PATTERN_SOLID_COLOR *)(feature_data+1));
		break;

	/* for factory mode auto testing */
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
	case SENSOR_FEATURE_GET_CROP_INFO:
	#if 0
		pr_debug("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
			(UINT32) *feature_data);
	#endif

		wininfo =
	(struct SENSOR_WINSIZE_INFO_STRUCT *) (uintptr_t) (*(feature_data + 1));

		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy(
			    (void *)wininfo,
			    (void *)&imgsensor_winsize_info[1],
			    sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));

			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy(
			    (void *)wininfo,
			    (void *)&imgsensor_winsize_info[2],
			    sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));

			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy(
			    (void *)wininfo,
			    (void *)&imgsensor_winsize_info[3],
			    sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));

			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy(
			    (void *)wininfo,
			    (void *)&imgsensor_winsize_info[4],
			    sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));

			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy(
			    (void *)wininfo,
			    (void *)&imgsensor_winsize_info[0],
			    sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));

			break;
		}
		break;
	case SENSOR_FEATURE_GET_AWB_REQ_BY_SCENARIO:
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1;
		break;
	case SENSOR_FEATURE_SET_AWB_GAIN:
		imx481_awb_gain(pSetSensorAWB);
		break;
	case SENSOR_FEATURE_GET_SENSOR_HDR_CAPACITY:
		pr_debug(
		    "SENSOR_FEATURE_GET_SENSOR_HDR_CAPACITY scenarioId:%llu\n",
		    *feature_data);
		*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0x0;
		break;

		/*END OF HDR CMD */
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		*feature_return_para_i32 = get_sensor_temperature();
		*feature_para_len = 4;
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
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
	}
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
	case SENSOR_FEATURE_GET_VC_INFO:
		pr_info("SENSOR_FEATURE_GET_VC_INFO %d\n",
			(UINT16) (*feature_data));

		pvcinfo =
	    (struct SENSOR_VC_INFO_STRUCT *) (uintptr_t) (*(feature_data + 1));

		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[1],
			       sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[2],
			       sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[3],
			       sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[0],
			       sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		}
		break;
	default:
		break;
	}

	return ERROR_NONE;
} /* feature_control */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};


UINT32 IMX481_MIPI_RAW_SensorInit(
			struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
} /* IMX481_MIPI_RAW_SensorInit */
