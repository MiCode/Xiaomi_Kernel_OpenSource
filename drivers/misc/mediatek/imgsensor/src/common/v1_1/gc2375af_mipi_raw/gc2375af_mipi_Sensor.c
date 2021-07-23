/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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
#include <linux/atomic.h>
#include <linux/types.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "gc2375af_mipi_Sensor.h"

/*********************Modify Following Strings for Debug*********************/
#define PFX "GC2375AF_camera_sensor"
#define LOG_1 cam_pr_debug("GC2375AF_,MIPI 1LANE\n")
/***************************   Modify end    ********************************/

#define cam_pr_debug(format, args...) \
	pr_debug("[%s] " format, __func__, ##args)

static DEFINE_SPINLOCK(imgsensor_drv_lock);
#define SETTLE_DELAY 85

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = GC2375AF_SENSOR_ID,

	.checksum_value = 0x8726c936,

	.pre = {
		.pclk = 39000000,
		.linelength = 1040,
		.framelength = 1240,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = SETTLE_DELAY,
		.mipi_pixel_rate = 62400000,
		.max_framerate = 300,
		},
	.cap = {
		.pclk = 39000000,
		.linelength = 1040,
		.framelength = 1240,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = SETTLE_DELAY,
		.max_framerate = 300,
		.mipi_pixel_rate = 62400000,
		},
	.cap1 = {
		.pclk = 39000000,
		.linelength = 1040,
		.framelength = 1240,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = SETTLE_DELAY,
		.mipi_pixel_rate = 62400000,
		.max_framerate = 300,
		},
	.normal_video = {
		.pclk = 39000000,
		.linelength = 1040,
		.framelength = 1240,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = SETTLE_DELAY,
		.mipi_pixel_rate = 62400000,
		.max_framerate = 300,
		},
	.hs_video = {
		.pclk = 39000000,
		.linelength = 1040,
		.framelength = 1240,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = SETTLE_DELAY,
		.mipi_pixel_rate = 62400000,
		.max_framerate = 300,
		},
	.slim_video = {
		.pclk = 39000000,
		.linelength = 1040,
		.framelength = 1240,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = SETTLE_DELAY,
		.mipi_pixel_rate = 62400000,
		.max_framerate = 300,
		},
	.margin = 16,		/* sensor framelength & shutter margin */
	.min_shutter = 1,	/* min shutter */
	.max_frame_length = 0x24c7,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,	/* isp gain delay frame for AE cycle */
	.ihdr_support = 0,	/* 1, support; 0,not support */
	.ihdr_le_firstline = 0,	/* 1,le first ; 0, se first */
	.sensor_mode_num = 3,	/* support sensor mode num */

	.cap_delay_frame = 2,
	.pre_delay_frame = 2,
	.video_delay_frame = 2,
	.hs_video_delay_frame = 2,
	.slim_video_delay_frame = 2,

	.isp_driving_current = ISP_DRIVING_6MA,	/* mclk driving current */
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	/* 0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL */
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_MANUAL,
#if GC2375H_MIRROR_FLIP_ENABLE
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
#else
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R,
#endif
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_1_LANE,	/* mipi lane num */
	.i2c_addr_table = {0x2e, 0xff},
};

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,	/* mirrorflip information */
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x3ED,	/* current shutter */
	.gain = 0x40,		/* current gain */
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */
	/* full size current fps : 24fps for PIP, 30fps for Normal or ZSD */
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_en = 0,
	.i2c_write_id = 0x2e,	/* record current sensor's i2c write id */
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
	{1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0000, 0000, 1600, 1200,
		0, 0, 1600, 1200},	/* Preview */
	{1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0000, 0000, 1600, 1200,
		0, 0, 1600, 1200},	/* capture */
	{1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0000, 0000, 1600, 1200,
		0, 0, 1600, 1200},	/* video */
	{1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0000, 0000, 1600, 1200,
		0, 0, 1600, 1200},	/* high speed video */
	{1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0000, 0000, 1600, 1200,
		0, 0, 1600, 1200}	/* slim video */
};

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;

	char pu_send_cmd[1] = { (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 1, (u8 *) &get_byte, 1,
		imgsensor.i2c_write_id);

	return get_byte;

}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[2] = { (char)(addr & 0xFF), (char)(para & 0xFF) };

	iWriteRegI2C(pu_send_cmd, 2, imgsensor.i2c_write_id);

}

static kal_uint16 read_eeprom_module_id(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;

	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *) &get_byte, 1, 0xA2);//Sunny

	return get_byte;
}

static void set_dummy(void)
{
	kal_uint32 vb = 0;
	kal_uint32 basic_line = 1224;

	cam_pr_debug("dummyline = %d\n", imgsensor.dummy_line);

	vb = imgsensor.frame_length - basic_line;
	vb = (vb < 16) ? 16 : vb;
	vb = (vb > 8191) ? 8191 : vb;
    cam_pr_debug("vb = %d.\n", vb);

	write_cmos_sensor(0x07, (vb >> 8) & 0x1F);
	write_cmos_sensor(0x08, vb & 0xFF);

}				/*    set_dummy  */

static kal_uint32 return_sensor_id(void)
{
	kal_uint32 sensor_id = 0;
	kal_uint16 vcm_id = 0;

	sensor_id = ((read_cmos_sensor(0xf0) << 8) | read_cmos_sensor(0xf1));

	vcm_id = read_eeprom_module_id(0x000A);

	if (0x0A == vcm_id)
		sensor_id += 1;

	printk("[%s][gc2375af] sensor_id: 0x%x vcm_id: 0x%x", __func__, sensor_id, vcm_id);

	return sensor_id;
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	cam_pr_debug("framerate = %d, min framelength should enable = %d\n",
		framerate,
		min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length =
		(frame_length > imgsensor.min_frame_length)
		? frame_length
		: imgsensor.min_frame_length;
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
}				/*    set_max_framerate  */

/*************************************************************************
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
 *************************************************************************/
static void set_shutter(kal_uint16 shutter)
{
	unsigned long flags;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

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
	shutter =
		(shutter >
		(imgsensor_info.max_frame_length -
		imgsensor_info.margin))
		? (imgsensor_info.max_frame_length -
		imgsensor_info.margin)
		: shutter;

    cam_pr_debug("current_f1 = %d. ", (imgsensor.dummy_line+imgsensor_info.pre.framelength));

    shutter = (shutter == imgsensor.dummy_line+imgsensor_info.pre.framelength-1) ? shutter + 1 : shutter;

	/* Update Shutter */
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x03, (shutter >> 8) & 0x3F);
	write_cmos_sensor(0x04, shutter & 0xFF);

	cam_pr_debug("Exit! shutter =%d, framelength =%d\n",
		shutter, imgsensor.frame_length);

}				/*    set_shutter */

static void set_shutter_frame_length(kal_uint16 shutter,
			kal_uint16 frame_length)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	kal_int32 dummy_line = 0;
	kal_uint32 basic_line = 1224;
	kal_uint32 vb = 0;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	spin_lock(&imgsensor_drv_lock);
	/*Change frame time*/
	dummy_line = frame_length - imgsensor.frame_length;
	imgsensor.frame_length = imgsensor.frame_length + dummy_line;
	imgsensor.min_frame_length = imgsensor.frame_length;
	//
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
			// Extend frame length

			//write_cmos_sensor(0x0340, imgsensor.frame_length);
      vb = imgsensor.frame_length - basic_line;
	    write_cmos_sensor(0x07, (vb >> 8) & 0x1F);
	    write_cmos_sensor(0x08, vb & 0xFF);

		}
	} else {
		// Extend frame length
      vb = imgsensor.frame_length - basic_line;
	    write_cmos_sensor(0x07, (vb >> 8) & 0x1F);
	    write_cmos_sensor(0x08, vb & 0xFF);
	}

	// Update Shutter
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x03, (shutter >> 8) & 0x3F);
	write_cmos_sensor(0x04, shutter & 0xFF);
	printk("Add for N3D! shutter =%d, framelength =%d\n",
		shutter, imgsensor.frame_length);

}



#if 0				/* not used function */
static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 reg_gain = 0x0000;

	reg_gain = ((gain / BASEGAIN) << 4) +
		((gain % BASEGAIN) * 16 / BASEGAIN);
	reg_gain = reg_gain & 0xFFFF;
	return (kal_uint16) reg_gain;
}
#endif

/*************************************************************************
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
 *************************************************************************/
static kal_uint16 set_gain(kal_uint16 gain)
{
#define ANALOG_GAIN_1 64	/* 1.00x */
#define ANALOG_GAIN_2 92	/* 1.43x */
#define ANALOG_GAIN_3 128	/* 2.00x */
#define ANALOG_GAIN_4 182	/* 2.84x */
#define ANALOG_GAIN_5 254	/* 3.97x */
#define ANALOG_GAIN_6 363	/* 5.68x */
#define ANALOG_GAIN_7 521	/* 8.14x */
#define ANALOG_GAIN_8 725	/* 11.34x */
#define ANALOG_GAIN_9 1038	/* 16.23x */

	kal_uint16 iReg, temp;

	iReg = gain;

	if (iReg < 0x40)
		iReg = 0x40;

	if ((iReg >= ANALOG_GAIN_1) && (iReg < ANALOG_GAIN_2)) {
		write_cmos_sensor(0x20, 0x0b);
		write_cmos_sensor(0x22, 0x0c);
		write_cmos_sensor(0x26, 0x0e);
		/* analog gain */
		write_cmos_sensor(0xb6, 0x00);
		temp = iReg;
		write_cmos_sensor(0xb1, temp >> 6);
		write_cmos_sensor(0xb2, (temp << 2) & 0xfc);
		cam_pr_debug("GC2375AF_MIPI analogic gain 1x, GC2375AF_MIPI add pregain = %d\n",
			temp);
	} else if ((iReg >= ANALOG_GAIN_2) && (iReg < ANALOG_GAIN_3)) {

		write_cmos_sensor(0x20, 0x0c);
		write_cmos_sensor(0x22, 0x0e);
		write_cmos_sensor(0x26, 0x0e);
		/* analog gain */
		write_cmos_sensor(0xb6, 0x01);
		temp = 64 * iReg / ANALOG_GAIN_2;
		write_cmos_sensor(0xb1, temp >> 6);
		write_cmos_sensor(0xb2, (temp << 2) & 0xfc);
		cam_pr_debug("GC2375AF_MIPI analogic gain 1.43x , GC2375AF_MIPI add pregain = %d\n",
			temp);
	} else if ((iReg >= ANALOG_GAIN_3) && (iReg < ANALOG_GAIN_4)) {
		write_cmos_sensor(0x20, 0x0c);
		write_cmos_sensor(0x22, 0x0e);
		write_cmos_sensor(0x26, 0x0e);
		/* analog gain */
		write_cmos_sensor(0xb6, 0x02);
		temp = 64 * iReg / ANALOG_GAIN_3;
		write_cmos_sensor(0xb1, temp >> 6);
		write_cmos_sensor(0xb2, (temp << 2) & 0xfc);
		cam_pr_debug("GC2375AF_MIPI analogic gain 2x , GC2375AF_MIPI add pregain = %d\n",
			temp);
	} else if ((iReg >= ANALOG_GAIN_4) && (iReg < ANALOG_GAIN_5)) {
		write_cmos_sensor(0x20, 0x0c);
		write_cmos_sensor(0x22, 0x0e);
		write_cmos_sensor(0x26, 0x0e);
		/* analog gain */
		write_cmos_sensor(0xb6, 0x03);
		temp = 64 * iReg / ANALOG_GAIN_4;
		write_cmos_sensor(0xb1, temp >> 6);
		write_cmos_sensor(0xb2, (temp << 2) & 0xfc);
		cam_pr_debug("GC2375AF_MIPI analogic gain 2.84x , GC2375AF_MIPI add pregain = %d\n",
			temp);
	} else if ((iReg >= ANALOG_GAIN_5) && (iReg < ANALOG_GAIN_6)) {
		write_cmos_sensor(0x20, 0x0c);
		write_cmos_sensor(0x22, 0x0e);
		write_cmos_sensor(0x26, 0x0e);
		/* analog gain */
		write_cmos_sensor(0xb6, 0x04);
		temp = 64 * iReg / ANALOG_GAIN_5;
		write_cmos_sensor(0xb1, temp >> 6);
		write_cmos_sensor(0xb2, (temp << 2) & 0xfc);
		cam_pr_debug("GC2375AF_MIPI analogic gain 3.97x , GC2375AF_MIPI add pregain = %d\n",
			temp);
	} else if ((iReg >= ANALOG_GAIN_6) && (iReg < ANALOG_GAIN_7)) {
		write_cmos_sensor(0x20, 0x0e);
		write_cmos_sensor(0x22, 0x0e);
		write_cmos_sensor(0x26, 0x0e);
		/* analog gain */
		write_cmos_sensor(0xb6, 0x05);
		temp = 64 * iReg / ANALOG_GAIN_6;
		write_cmos_sensor(0xb1, temp >> 6);
		write_cmos_sensor(0xb2, (temp << 2) & 0xfc);
		cam_pr_debug("GC2375AF_MIPI analogic gain 5.68x , GC2375AF_MIPI add pregain = %d\n",
			temp);
	} else if ((iReg >= ANALOG_GAIN_7) && (iReg < ANALOG_GAIN_8)) {
		write_cmos_sensor(0x20, 0x0c);
		write_cmos_sensor(0x22, 0x0c);
		write_cmos_sensor(0x26, 0x0e);
		/* analog gain */
		write_cmos_sensor(0xb6, 0x06);
		temp = 64 * iReg / ANALOG_GAIN_7;
		write_cmos_sensor(0xb1, temp >> 6);
		write_cmos_sensor(0xb2, (temp << 2) & 0xfc);
		cam_pr_debug("GC2375AF_MIPI analogic gain 8.14x , GC2375AF_MIPI add pregain = %d\n",
			temp);
	} else if ((iReg >= ANALOG_GAIN_8) && (iReg < ANALOG_GAIN_9)) {
		write_cmos_sensor(0x20, 0x0e);
		write_cmos_sensor(0x22, 0x0e);
		write_cmos_sensor(0x26, 0x0e);
		/* analog gain */
		write_cmos_sensor(0xb6, 0x07);
		temp = 64 * iReg / ANALOG_GAIN_8;
		write_cmos_sensor(0xb1, temp >> 6);
		write_cmos_sensor(0xb2, (temp << 2) & 0xfc);
		cam_pr_debug("GC2375AF_MIPI analogic gain 11.34x , GC2375AF_MIPI add pregain = %d\n",
			temp);
	} else {
		write_cmos_sensor(0x20, 0x0c);
		write_cmos_sensor(0x22, 0x0e);
		write_cmos_sensor(0x26, 0x0e);
		/* analog gain */
		write_cmos_sensor(0xb6, 0x08);
		temp = 64 * iReg / ANALOG_GAIN_9;
		write_cmos_sensor(0xb1, temp >> 6);
		write_cmos_sensor(0xb2, (temp << 2) & 0xfc);
		cam_pr_debug("GC2375AF_MIPI analogic gain 16.23x , GC2375AF_MIPI add pregain = %d\n",
			temp);
	}
	return gain;

}				/*    set_gain  */

static void ihdr_write_shutter_gain(
	kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{
	cam_pr_debug("le:0x%x, se:0x%x, gain:0x%x\n", le, se, gain);
}


#if 0				/* not used function */
static void set_mirror_flip(kal_uint8 image_mirror)
{
	cam_pr_debug("image_mirror = %d\n", image_mirror);

	switch (image_mirror) {
	case IMAGE_NORMAL:	/* IMAGE_NORMAL: */
		write_cmos_sensor(0x17, 0x14);	/* bit[1][0] */
		/* write_cmos_sensor(0x92, 0x03); */
		/* write_cmos_sensor(0x94, 0x0b); */
		break;
	case IMAGE_H_MIRROR:	/* IMAGE_H_MIRROR: */
		write_cmos_sensor(0x17, 0x15);
		/* write_cmos_sensor(0x92, 0x03); */
		/* write_cmos_sensor(0x94, 0x0b); */
		break;
	case IMAGE_V_MIRROR:	/* IMAGE_V_MIRROR: */
		write_cmos_sensor(0x17, 0x16);
		/* write_cmos_sensor(0x92, 0x02); */
		/* write_cmos_sensor(0x94, 0x0b); */
		break;
	case IMAGE_HV_MIRROR:	/* IMAGE_HV_MIRROR: */
		write_cmos_sensor(0x17, 0x17);
		/* write_cmos_sensor(0x92, 0x02); */
		/* write_cmos_sensor(0x94, 0x0b); */
		break;
	}
#endif

/*************************************************************************
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
 *************************************************************************/
static void night_mode(kal_bool enable)
{
	/*No Need to implement this function */
}			/*    night_mode    */

static void sensor_init(void)
{
	cam_pr_debug("E");

	/* System */
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xf7, 0x01);
	write_cmos_sensor(0xf8, 0x0c);
	write_cmos_sensor(0xf9, 0x42);
	write_cmos_sensor(0xfa, 0x88);
	write_cmos_sensor(0xfc, 0x8e);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x88, 0x03);
	write_cmos_sensor(0xef, 0x00); /* stream off */

	/* Analog */
	write_cmos_sensor(0x03, 0x04);
	write_cmos_sensor(0x04, 0x65);
	write_cmos_sensor(0x05, 0x02);
	write_cmos_sensor(0x06, 0x5a);
	write_cmos_sensor(0x07, 0x00);
	write_cmos_sensor(0x08, 0x10);
	write_cmos_sensor(0x09, 0x00);
	write_cmos_sensor(0x0a, 0x04);
	write_cmos_sensor(0x0b, 0x00);
	write_cmos_sensor(0x0c, 0x14);
	write_cmos_sensor(0x0d, 0x04);
	write_cmos_sensor(0x0e, 0xb8);
	write_cmos_sensor(0x0f, 0x06);
	write_cmos_sensor(0x10, 0x48);
	write_cmos_sensor(0x17, GC2375H_MIRROR);
	write_cmos_sensor(0x1c, 0x10);
	write_cmos_sensor(0x1d, 0x13);
	write_cmos_sensor(0x20, 0x0b);
	write_cmos_sensor(0x21, 0x6d);
	write_cmos_sensor(0x22, 0x0c);
	write_cmos_sensor(0x25, 0xc1);
	write_cmos_sensor(0x26, 0x0e);
	write_cmos_sensor(0x27, 0x22);
	write_cmos_sensor(0x29, 0x5f);
	write_cmos_sensor(0x2b, 0x88);
	write_cmos_sensor(0x2f, 0x12);
	write_cmos_sensor(0x38, 0x86);
	write_cmos_sensor(0x3d, 0x00);
	write_cmos_sensor(0xcd, 0xa3);
	write_cmos_sensor(0xce, 0x57);
	write_cmos_sensor(0xd0, 0x09);
	write_cmos_sensor(0xd1, 0xca);
	write_cmos_sensor(0xd2, 0x34);
	write_cmos_sensor(0xd3, 0xbb);
	write_cmos_sensor(0xd8, 0x60);
	write_cmos_sensor(0xe0, 0x08);
	write_cmos_sensor(0xe1, 0x1f);
	write_cmos_sensor(0xe4, 0xf8);
	write_cmos_sensor(0xe5, 0x0c);
	write_cmos_sensor(0xe6, 0x10);
	write_cmos_sensor(0xe7, 0xcc);
	write_cmos_sensor(0xe8, 0x02);
	write_cmos_sensor(0xe9, 0x01);
	write_cmos_sensor(0xea, 0x02);
	write_cmos_sensor(0xeb, 0x01);

	/* out crop */
	//write_cmos_sensor(0x86, 0x54);
	write_cmos_sensor(0x90, 0x01);
	write_cmos_sensor(0x92, STARTY);
	write_cmos_sensor(0x94, STARTX);
	write_cmos_sensor(0x95, 0x04);
	write_cmos_sensor(0x96, 0xb0);
	write_cmos_sensor(0x97, 0x06);
	write_cmos_sensor(0x98, 0x40);

	/* BLK */
	write_cmos_sensor(0x18, 0x02);
	write_cmos_sensor(0x1a, 0x18);
	write_cmos_sensor(0x28, 0x00);
	write_cmos_sensor(0x3f, 0x40);
	write_cmos_sensor(0x40, 0x26);
	write_cmos_sensor(0x41, 0x00);
	write_cmos_sensor(0x43, 0x03);
	write_cmos_sensor(0x4a, 0x00);
	write_cmos_sensor(0x4e, GC2375H_BLK_Select1_H);
	write_cmos_sensor(0x4f, GC2375H_BLK_Select1_L);
	write_cmos_sensor(0x66, GC2375H_BLK_Select2_H);
	write_cmos_sensor(0x67, GC2375H_BLK_Select2_L);

	/* Dark sun */
	write_cmos_sensor(0x68, 0x00);

	/* Gain */
	write_cmos_sensor(0xb0, 0x58);
	write_cmos_sensor(0xb1, 0x01);
	write_cmos_sensor(0xb2, 0x00);
	write_cmos_sensor(0xb6, 0x00);

	/* MIPI */
	//write_cmos_sensor(0xef, 0x90);
	write_cmos_sensor(0xfe, 0x03);
	write_cmos_sensor(0x01, 0x03);
	write_cmos_sensor(0x02, 0x33);
	write_cmos_sensor(0x03, 0x90);
	write_cmos_sensor(0x04, 0x04);
	write_cmos_sensor(0x05, 0x00);
	write_cmos_sensor(0x06, 0x80);
	write_cmos_sensor(0x11, 0x2b);
	write_cmos_sensor(0x12, 0xd0);
	write_cmos_sensor(0x13, 0x07);
	write_cmos_sensor(0x15, 0x00);
	write_cmos_sensor(0x21, 0x08);
	write_cmos_sensor(0x22, 0x05);
	write_cmos_sensor(0x23, 0x13);
	write_cmos_sensor(0x24, 0x02);
	write_cmos_sensor(0x25, 0x13);
	write_cmos_sensor(0x26, 0x08);
	write_cmos_sensor(0x29, 0x06);
	write_cmos_sensor(0x2a, 0x08);
	write_cmos_sensor(0x2b, 0x08);
	write_cmos_sensor(0xfe, 0x00);
}			/*    sensor_init  */

static void preview_setting(void)
{
	cam_pr_debug("E!\n");
}			/*    preview_setting  */

static void capture_setting(kal_uint16 currefps)
{
	cam_pr_debug("E! currefps:%d\n", currefps);
}

static void normal_video_setting(kal_uint16 currefps)
{
	cam_pr_debug("E! currefps:%d\n", currefps);
}

static void hs_video_setting(void)
{
	cam_pr_debug("E\n");
}

static void slim_video_setting(void)
{
	cam_pr_debug("E\n");
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	cam_pr_debug("enable: %d\n", enable);

	if (enable) {
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0x8c, 0x11);
	} else {
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0x8c, 0x10);
	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

/*************************************************************************
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
				cam_pr_debug("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}
			cam_pr_debug("Read sensor id fail, write id: 0x%x, id: 0x%x\n",
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
 *************************************************************************/
static kal_uint32 open(void)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint32 sensor_id = 0;

	LOG_1;

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id();
			if (sensor_id == imgsensor_info.sensor_id) {
				cam_pr_debug("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, sensor_id);
				break;
			}
			cam_pr_debug("Read sensor id fail, write id: 0x%x, id: 0x%x\n",
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
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_en = imgsensor_info.ihdr_support;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}			/*    open  */

/*************************************************************************
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
 *************************************************************************/
static kal_uint32 close(void)
{
	cam_pr_debug("E\n");
	/*No Need to implement this function */
	return ERROR_NONE;
}			/*    close  */

/*************************************************************************
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
 *************************************************************************/
static kal_uint32 preview(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT * image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	cam_pr_debug("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	/* imgsensor.video_mode = KAL_FALSE; */
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	/* set_mirror_flip(imgsensor.mirror); */
	return ERROR_NONE;
}			/*    preview   */

/*************************************************************************
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
 *************************************************************************/
static kal_uint32 capture(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT * image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	cam_pr_debug("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			cam_pr_debug("Warning: current_fps %d fps is not support, so use cap1's setting: %d fps!\n",
				imgsensor.current_fps,
				imgsensor_info.cap1.max_framerate / 10);
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
}			/* capture() */

static kal_uint32 normal_video(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT * image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	cam_pr_debug("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting(imgsensor.current_fps);
	/* set_mirror_flip(imgsensor.mirror); */
	return ERROR_NONE;
}			/*    normal_video   */

static kal_uint32 hs_video(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT * image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	cam_pr_debug("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	/* set_mirror_flip(imgsensor.mirror); */
	return ERROR_NONE;
}			/*    hs_video   */

static kal_uint32 slim_video(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT * image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	cam_pr_debug("E\n");

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
	/* set_mirror_flip(imgsensor.mirror); */
	return ERROR_NONE;
}			/*    slim_video     */

static kal_uint32 get_resolution(
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT * sensor_resolution)
{
	cam_pr_debug("E\n");

	spin_lock(&imgsensor_drv_lock);
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
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}			/*    get_resolution    */

static kal_uint32 get_info(
	enum MSDK_SCENARIO_ID_ENUM scenario_id,
	MSDK_SENSOR_INFO_STRUCT *sensor_info,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	cam_pr_debug("scenario_id = %d\n", scenario_id);

	spin_lock(&imgsensor_drv_lock);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
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

	/* The frame of setting sensor gain */
	sensor_info->AESensorGainDelayFrame =
		imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame =
		imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;

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
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}			/*    get_info  */

static kal_uint32 control(
	enum MSDK_SCENARIO_ID_ENUM scenario_id,
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	cam_pr_debug("scenario_id = %d\n", scenario_id);
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
		cam_pr_debug("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}			/* control() */

static kal_uint32 set_video_mode(UINT16 framerate)
{
	cam_pr_debug("framerate = %d\n ", framerate);
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
	cam_pr_debug("enable = %d, framerate = %d\n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable)	/* enable auto flicker */
		imgsensor.autoflicker_en = KAL_TRUE;
	else		/* Cancel Auto flick */
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id,
	MUINT32 framerate)
{
	kal_uint32 frame_length;

	cam_pr_debug("scenario_id = %d, framerate = %d\n",
		scenario_id,
		framerate);

	if (framerate == 0)
		return ERROR_NONE;
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frame_length =
			imgsensor_info.pre.pclk / framerate * 10 /
			imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length >
			imgsensor_info.pre.framelength)
			? (frame_length -
			imgsensor_info.pre.framelength)
			: 0;
		imgsensor.frame_length =
			imgsensor_info.pre.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		frame_length =
			imgsensor_info.normal_video.pclk / framerate * 10 /
			imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length >
			imgsensor_info.normal_video.framelength)
			? (frame_length -
			imgsensor_info.normal_video.framelength)
			: 0;
		imgsensor.frame_length =
			imgsensor_info.normal_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (imgsensor.current_fps ==
			imgsensor_info.cap1.max_framerate) {
			frame_length =
				imgsensor_info.cap1.pclk / framerate * 10 /
				imgsensor_info.cap1.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
				(frame_length >
				imgsensor_info.cap1.framelength)
				? (frame_length -
				imgsensor_info.cap1.framelength)
				: 0;
			imgsensor.frame_length =
				imgsensor_info.cap1.framelength +
				imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		} else {
			if (imgsensor.current_fps !=
				imgsensor_info.cap.max_framerate)
				cam_pr_debug("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
					framerate,
					imgsensor_info.cap.max_framerate / 10);
			frame_length =
				imgsensor_info.cap.pclk / framerate * 10 /
				imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
				(frame_length >
				imgsensor_info.cap.framelength)
				? (frame_length -
				imgsensor_info.cap.framelength)
				: 0;
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
		frame_length =
			imgsensor_info.hs_video.pclk / framerate * 10 /
			imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length >
			imgsensor_info.hs_video.framelength)
			? (frame_length -
			imgsensor_info.hs_video.framelength)
			: 0;
		imgsensor.frame_length =
			imgsensor_info.hs_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length =
			imgsensor_info.slim_video.pclk / framerate * 10 /
			imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length >
			imgsensor_info.slim_video.framelength)
			? (frame_length -
			imgsensor_info.slim_video.framelength)
			: 0;
		imgsensor.frame_length =
			imgsensor_info.slim_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	default:	/* coding with  preview scenario by default */
		frame_length =
			imgsensor_info.pre.pclk / framerate * 10 /
			imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length >
			imgsensor_info.pre.framelength)
			? (frame_length -
			imgsensor_info.pre.framelength)
			: 0;
		imgsensor.frame_length =
			imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		cam_pr_debug("error scenario_id = %d, we use preview scenario\n",
			scenario_id);
		break;
	}
	return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id,
	MUINT32 *framerate)
{
	cam_pr_debug("scenario_id = %d\n", scenario_id);
	if (framerate == NULL) {
		cam_pr_debug("---- framerate is NULL ---- check here\n");
		return ERROR_NONE;
	}
	spin_lock(&imgsensor_drv_lock);
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
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}


static kal_uint32 streaming_control(kal_bool enable)
{
	cam_pr_debug("streaming_enable(0=Sw Standby,1=streaming): %d\n",
		enable);
	if (enable) {
		/* MIPI// */
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0xef, 0x90);	/*Stream on */
	} else {
		/* MIPI// */
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0xef, 0x00);	/*Stream off */
	}

	mdelay(25);
	return ERROR_NONE;
}

static kal_uint32 feature_control(
	MSDK_SENSOR_FEATURE_ENUM feature_id,
	UINT8 *feature_para,
	UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *)feature_para;

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
	    (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	if (feature_para == NULL) {
		cam_pr_debug(" ---- %s ---- error params1\n", __func__);
		return ERROR_NONE;
	}

	cam_pr_debug("feature_id = %d\n", feature_id);
	switch (feature_id) {
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16 = imgsensor.frame_length;
		*feature_para_len = 4;
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
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.pre.framelength << 16)
				+ imgsensor_info.pre.linelength;
			break;
		}
		break;

	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		cam_pr_debug("feature_Control imgsensor.pclk = %d,imgsensor.current_fps = %d\n",
			imgsensor.pclk, imgsensor.current_fps);
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
		set_auto_flicker_mode((BOOL) * feature_data_16,
			*(feature_data_16 + 1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM) *feature_data,
			*(feature_data + 1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM) *(feature_data),
			(MUINT32 *) (uintptr_t) (*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL) * feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
/*	case SENSOR_FEATURE_GET_SENSOR_ID:
		*feature_return_para_32 = imgsensor_info.sensor_id;
		*feature_para_len = 4;
		break;
*/
	case SENSOR_FEATURE_SET_FRAMERATE:
		cam_pr_debug("current fps :%d\n", (UINT32) *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = *feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		cam_pr_debug("ihdr enable :%d\n", (BOOL) * feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_en = (BOOL) * feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		cam_pr_debug("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
			(UINT32) *feature_data);

		wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)
			(uintptr_t) (*(feature_data + 1));

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
		cam_pr_debug("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16) *feature_data, (UINT16) *(feature_data + 1),
			(UINT16) *(feature_data + 2));
		ihdr_write_shutter_gain((UINT16) *feature_data,
			(UINT16) *(feature_data + 1),
			(UINT16) *(feature_data + 2));
		break;
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
	{
		kal_uint32 rate;

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			rate = imgsensor_info.cap.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			rate = imgsensor_info.normal_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			rate = imgsensor_info.hs_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			rate = imgsensor_info.slim_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			rate = imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
		*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = rate;
	}
		break;
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length((UINT16)(*feature_data),
						(UINT16)(*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		cam_pr_debug("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		cam_pr_debug("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
			*feature_data);
		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(KAL_TRUE);
		break;
	default:
		break;
	}

	return ERROR_NONE;
}			/*    feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 GC2375AF_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}			/*    GC2375AF_MIPI_RAW_SensorInit    */
