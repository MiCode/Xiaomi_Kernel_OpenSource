/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *     OS05A20mipi_Sensor.c
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

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "ov05a20mipiraw_Sensor.h"

/*********************Modify Following Strings for Debug******************/
#define PFX "OV05A20_camera_sensor"
#define LOG_1 cam_pr_debug("OV05A20,MIPI 4LANE\n")
#define LOG_2 cam_pr_debug("420Mbps/lane, prv 1280*960@30fps; cap 5M@15fps\n")
/**********************   Modify end    **********************************/
#define NO_USE_3HDR 1
#define cam_pr_debug(format, args...) pr_debug(PFX "[%s] " format, __func__, ##args)
static int first_config;

static DEFINE_SPINLOCK(imgsensor_drv_lock);

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = OV05A20_SENSOR_ID,

	.checksum_value = 0xfb225e4d,

	.pre = {
		.pclk = 108000000,
		.linelength = 720,
		.framelength = 5000,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2560,
		.grabwindow_height = 1920,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 601600000,
		.max_framerate = 300,
		},
	.cap = {
		.pclk = 108000000,
		.linelength = 720,
		.framelength = 5000,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2560,
		.grabwindow_height = 1920,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 601600000,
		.max_framerate = 300,
		},
	.cap1 = {
		.pclk = 108000000,
		.linelength = 720,
		.framelength = 50000,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2560,
		.grabwindow_height = 1920,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 601600000,
		.max_framerate = 300,
		},
	.normal_video = {
		.pclk = 108000000,
		.linelength = 720,
		.framelength = 5000,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2560,
		.grabwindow_height = 1920,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 601600000,
		.max_framerate = 300,
		},
	.hs_video = {
		.pclk = 54000000,
		.linelength = 720,
		.framelength = 2496,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2560,
		.grabwindow_height = 1920,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 217600000,
		.max_framerate = 300,
		},
	.slim_video = {
		.pclk = 54000000,
		.linelength = 720,
		.framelength = 2496,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2560,
		.grabwindow_height = 1920,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 217600000,
		.max_framerate = 300,
		},
	.custom1 = {
		.pclk = 108000000,
		.linelength = 1440,
		.framelength = 2500,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2560,
		.grabwindow_height = 1920,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 601600000,
		.max_framerate = 300,
		},
	.margin = 10,		/* sensor framelength & shutter margin */
	.min_shutter = 4,	/* min shutter */
	.min_gain = 64, /*1x gain*/
	.max_gain = 15872, /*16x gain*/
	.min_gain_iso = 100,
	.gain_step = 16,
	.gain_type = 0,
	.max_frame_length = 0x7fff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,	/* isp gain delay frame for AE cycle */
	.ihdr_support = 0,	/* 1, support; 0,not support */
	.ihdr_le_firstline = 0,	/* 1,le first ; 0, se first */
	.sensor_mode_num = 6,	/* support sensor mode num */

	.cap_delay_frame = 2,
	.pre_delay_frame = 2,
	.video_delay_frame = 2,
	.hs_video_delay_frame = 2,
	.slim_video_delay_frame = 2,
	.custom1_delay_frame = 2,
	.isp_driving_current = ISP_DRIVING_6MA,	/* mclk driving current */
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,
	/* 0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL */
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,	/* mipi lane num */
	.i2c_addr_table = {0x6c, 0xff},
};

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,	/* mirrorflip information */
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x3D0,	/* current shutter */
	.gain = 0x100,		/* current gain */
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_en = 0,
	.i2c_write_id = 0x6c,	/* record current sensor's i2c write id */
};

static struct SENSOR_VC_INFO2_STRUCT SENSOR_VC_INFO2[3] = {

	{0x01, 0x0a, 0x00, 0x08, 0x40, 0x00,  //number of stream, legacy field, legacy field, ...
	// feature type, channel id, data type, width in pixel, height in pixel
	 {{VC_STAGGER_NE, 0x00, 0x2b, 0x0A00, 0x780},
	 },
	 1
	},

	{0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,  //	custom1  2exp
	 {{VC_STAGGER_NE, 0x00, 0x2b, 0x0A00, 0x780},
	 {VC_STAGGER_SE, 0x01, 0x2b, 0x0A00, 0x780},
	 },
	 1
	},
	{0x01, 0x0a, 0x00, 0x08, 0x40, 0x00,   // for shortExp test
	 {{VC_STAGGER_NE, 0x01, 0x2b, 0x0A00, 0x780},
	 },
	 1
	},
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[6] = {
	/* Preview */
	{2560, 1920, 0, 0, 2560, 1920, 2560, 1920, 0, 0, 2560, 1920, 0, 0, 2560, 1920},
	/* capture */
	{2560, 1920, 0, 0, 2560, 1920, 2560, 1920, 0, 0, 2560, 1920, 0, 0, 2560, 1920},
	/* video*/
	{2560, 1920, 0, 0, 2560, 1920, 2560, 1920, 0, 0, 2560, 1920, 0, 0, 2560, 1920},
	/* hight speed video */
	{2560, 1920, 0, 0, 2560, 1920, 2560, 1920, 0, 0, 2560, 1920, 0, 0, 2560, 1920},
	/* slim video */
	{2560, 1920, 0, 0, 2560, 1920, 2560, 1920, 0, 0, 2560, 1920, 0, 0, 2560, 1920},
	/* custom1 staggered HDR */
	{2560, 1920, 0, 0, 2560, 1920, 2560, 1920, 0, 0, 2560, 1920, 0, 0, 2560, 1920}
};

static kal_uint32 AGain_table[] = {
	128, 136, 144, 152, 160, 168, 176, 184, 192, 200, 208, 216, 224, 232, 240, 248,
	256, 272, 288, 304, 320, 336, 352, 368, 384, 400, 416, 432, 448, 464, 480, 496,
	512, 544, 576, 608, 640, 672, 704, 736, 768, 800, 832, 864, 896, 928, 960, 992,
	1024, 1088, 1152, 1216, 1280, 1344, 1408, 1472, 1536, 1600, 1664, 1728, 1792, 1856,
	1920, 1984
};

static kal_uint32 binary_find_AGain(kal_uint32 *array,
	kal_uint32 key, kal_uint32 total)
{
	kal_uint32 left = 0;
	kal_uint32 right = total - 1;
	kal_uint32 mid = 0;

	while (left != right) {
		mid = (left + right) / 2;

		if (key <= array[mid])
			right = mid;
		else
			left = mid;
		if (right - left < 2)
			break;
	}

	return (array[right] == key) ? array[right] : array[left];
}

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;

	char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *) &get_byte, 1,
		imgsensor.i2c_write_id);

	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[3] = { (char)(addr >> 8), (char)(addr & 0xFF),
		(char)(para & 0xFF) };
	iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
	cam_pr_debug("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);

	write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
	write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
	//write_cmos_sensor(0x380c, imgsensor.line_length >> 8);
	//write_cmos_sensor(0x380d, imgsensor.line_length & 0xFF);

}				/*    set_dummy  */

static kal_uint32 return_sensor_id(void)
{
	return ((read_cmos_sensor(0x300A) << 8) | read_cmos_sensor(0x300B));
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
		(frame_length > imgsensor.min_frame_length) ?
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
}				/*    set_max_framerate  */

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
	shutter =
	    (shutter >
	     (imgsensor_info.max_frame_length -
	      imgsensor_info.margin)) ? (imgsensor_info.max_frame_length -
					 imgsensor_info.margin) : shutter;

	// Framelength should be an even number
	shutter = (shutter >> 1) << 1;
	imgsensor.frame_length = (imgsensor.frame_length >> 1) << 1;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 /
			imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			// Extend frame length
			write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
			write_cmos_sensor(0x380f,
				imgsensor.frame_length & 0xFF);
		}
	} else {
		// Extend frame length
		write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
		write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
	}

	// Update Shutter
	//write_cmos_sensor(0x3500, (shutter >> 12) & 0x0F);
	write_cmos_sensor(0x3501, (shutter >> 8) & 0xFF);
	write_cmos_sensor(0x3502, shutter & 0xFF);

	cam_pr_debug("Exit shutter =%d, framelength =%d\n",
		shutter,
		imgsensor.frame_length);
}

static void hdr_write_shutter(kal_uint16 LE, kal_uint16 SE)
{
//	kal_uint16 realtime_fps = 0;
	kal_uint16 TE = 0;
	//kal_uint16 reg_gain;
	TE = LE + SE + imgsensor_info.margin;
	cam_pr_debug("write_shutter LE =%d SE =%d\n", LE, SE);
	//cam_pr_debug("LE:0x%x, SE:0x%x\n", LE, SE);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.framelength_tmp = imgsensor.frame_length;
	if ((imgsensor.min_frame_length - imgsensor_info.margin) < TE)
		imgsensor.frame_length = TE + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	if (imgsensor_info.min_shutter > TE)
		TE = imgsensor_info.min_shutter;
	if ((imgsensor.min_frame_length - imgsensor_info.margin) < TE)
		imgsensor.frame_length = TE + imgsensor_info.margin;

	imgsensor.frame_length = (imgsensor.frame_length >> 1) << 1;
#if 0
	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
			write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
		}
	} else {
		write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
		write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
	}
#endif
    //LE = (LE >> 1) << 1;
    //SE = (SE >> 1) << 1;
	imgsensor.hdr_LE = LE;
	imgsensor.hdr_SE = SE;
#if 0
	write_cmos_sensor(0x320d, 0x00);
	write_cmos_sensor(0x3208, 0x00);
	write_cmos_sensor(0x0808, 0x00);
	write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
	write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
	// Long exposure
	write_cmos_sensor(0x3501, (LE >> 8) & 0xFF);
	write_cmos_sensor(0x3502, LE & 0xFF);
	// Short exposure
	write_cmos_sensor(0x3511, (SE >> 8) & 0xFF);
	write_cmos_sensor(0x3512, SE & 0xFF);

	write_cmos_sensor(0x3508, imgsensor.hdr_a_lgain >> 8);
	write_cmos_sensor(0x3509, imgsensor.hdr_a_lgain & 0xFF);
	write_cmos_sensor(0x350c, imgsensor.hdr_a_sgain >> 8);
	write_cmos_sensor(0x350d, imgsensor.hdr_a_sgain & 0xFF);

	write_cmos_sensor(0x350a, imgsensor.hdr_d_lgain >> 8);
	write_cmos_sensor(0x350b, imgsensor.hdr_d_lgain & 0xFF);
	write_cmos_sensor(0x350e, imgsensor.hdr_d_sgain >> 8);
	write_cmos_sensor(0x350f, imgsensor.hdr_d_sgain & 0xFF);


	write_cmos_sensor(0x3208, 0x10);
	write_cmos_sensor(0x320d, 0x00);
	write_cmos_sensor(0x3208, 0xa0);

	cam_pr_debug("Exit shutter LE =%d SE =%d, framelength =%d\n",
		LE, SE,
		imgsensor.frame_length);
	cam_pr_debug("Exit gain A_LG =%d D_LG =%d A_SG =%d  D_SG =%d\n",
		imgsensor.hdr_a_lgain, imgsensor.hdr_d_lgain,
		imgsensor.hdr_a_sgain, imgsensor.hdr_d_sgain);
#endif
}


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

	write_shutter(shutter);

}

static kal_uint32 gain2reg(const kal_uint32 gain)
{
	kal_uint32 iReg = 0x0000;

	//platform 1xgain = 64, sensor driver 1*gain = 0x100
	iReg = gain*128/BASEGAIN;
	/*
	 * if(iReg < 0x80)// sensor 1xGain
	 * {
	 *	iReg = 0X80;
	 * }
	 * if(iReg > 0x7ff)// sensor 15.5xGain
	 * {
	 *	iReg = 0X7ff;
	 * }
	 */
	return iReg;
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

static kal_uint32 set_gain(kal_uint32 gain)
{
	kal_uint32 reg_gain;
	kal_uint32 reg_a_gain;
	kal_uint32 reg_d_gain;

	cam_pr_debug("setting gain = %d\n", gain);

	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	cam_pr_debug("setting reg_gain = %d\n ", reg_gain);

	if (reg_gain > 0x7c0) {
		reg_d_gain = reg_gain*1024/1984;

		if (reg_d_gain > 0x3fff)// sensor 16xGain
			reg_d_gain = 0x3fff;
		/* long exposure */
		write_cmos_sensor(0x0350a, (reg_d_gain >> 8));
		write_cmos_sensor(0x0350b, (reg_d_gain&0xff));
		write_cmos_sensor(0x03508, 0x07);
		write_cmos_sensor(0x03509, 0xc0);
		cam_pr_debug("setting reg_d_gain = %d\n ", reg_d_gain);
	} else {
		if (reg_gain < 0x80)// sensor 1xGain
			reg_gain = 0x80;

		/* binary to find A_Gain */
		reg_a_gain = binary_find_AGain(AGain_table,
			reg_gain, ARRAY_SIZE(AGain_table));

		/* in case of insufficient accurary, use D_Gain supplement A_Gain */
		reg_d_gain = reg_gain*1024/reg_a_gain;
		/* long exposure */
		write_cmos_sensor(0x0350a, (reg_d_gain >> 8));
		write_cmos_sensor(0x0350b, (reg_d_gain&0xff));
		write_cmos_sensor(0x03508, (reg_a_gain >> 8));
		write_cmos_sensor(0x03509, (reg_a_gain&0xff));
		cam_pr_debug("setting reg_a_gain = %d  reg_d_gain = %d\n ",
			reg_a_gain, reg_d_gain);
	}

	cam_pr_debug("Exit setting gain\n");

	return gain;
}				/*    set_gain  */

#if 0
static void hdr_write_gain(kal_uint16 lgain, kal_uint16 sgain)
{
	kal_uint32 reg_lgain;
	kal_uint32 reg_sgain;
	kal_uint32 reg_d_lgain;
	kal_uint32 reg_d_sgain;
	kal_uint32 reg_a_lgain;
	kal_uint32 reg_a_sgain;

	cam_pr_debug("setting lgain=%d sgain=%d\n", lgain, sgain);

	reg_lgain = gain2reg(lgain);
	reg_sgain = gain2reg(sgain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_lgain;
	spin_unlock(&imgsensor_drv_lock);
	cam_pr_debug("setting reg_lgain = %d reg_sgain = %d\n ",
		reg_lgain, reg_sgain);

	if (reg_lgain > 0x7c0) {

		reg_d_lgain = reg_lgain*1024/1984;

		if (reg_d_lgain < 0x400)// sensor 1xGain
			reg_d_lgain = 0x400;
		if (reg_d_lgain > 0x3fff)// sensor 16xGain
			reg_d_lgain = 0x3fff;
		reg_a_lgain = 0x07c0;
	} else {
		if (reg_lgain < 0x80)// sensor 1xGain
			reg_lgain = 0x80;
		if (reg_lgain > 0x7c0)// sensor 15.5xGain
			reg_lgain = 0x7c0;

		/* binary to find A_Gain */
		reg_a_lgain = binary_find_AGain(AGain_table,
			reg_lgain, ARRAY_SIZE(AGain_table));

		/* in case of insufficient accurary, use D_Gain supplement A_Gain */
		reg_d_lgain = reg_lgain*1024/reg_a_lgain;
	}

	if (reg_sgain > 0x7c0) {

		reg_d_sgain = reg_sgain*1024/1984;

		if (reg_d_sgain < 0x400)// sensor 1xGain
			reg_d_sgain = 0x400;
		if (reg_d_sgain > 0x3fff)// sensor 16xGain
			reg_d_sgain = 0x3fff;
		reg_a_sgain = 0x7c0;

	} else {

		if (reg_sgain < 0x80)// sensor 1xGain
			reg_sgain = 0x80;
		if (reg_sgain > 0x7c0)// sensor 15.5xGain
			reg_sgain = 0x7c0;

		/* binary to find A_Gain */
		reg_a_sgain = binary_find_AGain(AGain_table,
			reg_sgain, ARRAY_SIZE(AGain_table));

		/* in case of insufficient accurary, use D_Gain supplement A_Gain */
		reg_d_sgain = reg_sgain*1024/reg_a_sgain;
	}

	write_cmos_sensor(0x320d, 0x00);
	write_cmos_sensor(0x3208, 0x00);
	write_cmos_sensor(0x0808, 0x00);
	/* long gain */
	write_cmos_sensor(0x0350a, (reg_d_lgain >> 8));
	write_cmos_sensor(0x0350b, (reg_d_lgain&0xff));
	write_cmos_sensor(0x03508, (reg_a_lgain >> 8));
	write_cmos_sensor(0x03509, (reg_a_lgain&0xff));

	/* short exposure */
	write_cmos_sensor(0x0350e, (reg_d_sgain >> 8));
	write_cmos_sensor(0x0350f, (reg_d_sgain&0xff));
	write_cmos_sensor(0x0350c, (reg_a_sgain >> 8));
	write_cmos_sensor(0x0350d, (reg_a_sgain&0xff));
	write_cmos_sensor(0x3208, 0x10);
	write_cmos_sensor(0x320d, 0x00);
	write_cmos_sensor(0x3208, 0xa0);

	cam_pr_debug("setting LONG Gain reg_a_lgain =%d  reg_d_lgain =%d",
		reg_a_lgain, reg_d_lgain);
	cam_pr_debug("setting SHORT Gain reg_a_sgain =%d  reg_d_sgain =%d",
		reg_a_sgain, reg_d_sgain);
	cam_pr_debug("Exit setting hdr_dual_gain\n");
}
#endif

static void hdr_write_gain(kal_uint16 lgain, kal_uint16 sgain)
{
	kal_uint32 reg_lgain;
	kal_uint32 reg_sgain;
	kal_uint32 reg_d_lgain;
	kal_uint32 reg_d_sgain;
	kal_uint32 reg_a_gain;   // match to gain_table
	kal_uint32 reg_d_gain;

	cam_pr_debug("setting lgain=%d sgain=%d\n", lgain, sgain);

	reg_lgain = gain2reg(lgain);
	reg_sgain = gain2reg(sgain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_lgain;
	spin_unlock(&imgsensor_drv_lock);
	cam_pr_debug("setting reg_lgain = %d reg_sgain = %d\n ",
		reg_lgain, reg_sgain);

	if (reg_lgain > 0x7c0) {
		reg_d_lgain = reg_lgain*1024/1984;

		if (reg_d_lgain > 0x3fff)// sensor 16xGain
			reg_d_lgain = 0x3fff;
		/* long exposure */
		/*
		 * write_cmos_sensor(0x0350a,(reg_d_lgain >> 8));
		 * write_cmos_sensor(0x0350b,(reg_d_lgain&0xff));
		 * write_cmos_sensor(0x03508, 0x07);
		 * write_cmos_sensor(0x03509, 0xc0);
		 */
		cam_pr_debug("setting reg_d_lgain = %d\n ", reg_d_lgain);
		imgsensor.hdr_d_lgain = reg_d_lgain;
		imgsensor.hdr_a_lgain = 0x07c0;
	} else {
		if (reg_lgain < 0x80)// sensor 1xGain
			reg_lgain = 0x80;

		/* binary to find A_Gain */
		reg_a_gain = binary_find_AGain(AGain_table,
			reg_lgain, ARRAY_SIZE(AGain_table));

		/* in case of insufficient accurary, use D_Gain supplement A_Gain */
		reg_d_gain = reg_lgain*1024/reg_a_gain;

		/* long gain */
		/*
		 * write_cmos_sensor(0x0350a, (reg_d_gain >> 8));
		 * write_cmos_sensor(0x0350b, (reg_d_gain&0xff));
		 * write_cmos_sensor(0x03508, (reg_a_gain >> 8));
		 * write_cmos_sensor(0x03509, (reg_a_gain&0xff));
		 */
		cam_pr_debug("setting LONG Gain reg_a_gain =%d  reg_d_gain =%d\n ",
			reg_a_gain, reg_d_gain);
		imgsensor.hdr_d_lgain = reg_d_gain;
		imgsensor.hdr_a_lgain = reg_a_gain;
	}

	if (reg_sgain > 0x7c0) {
		reg_d_sgain = reg_sgain*1024/1984;

		if (reg_d_sgain > 0x3fff)// sensor 16xGain
			reg_d_sgain = 0x3fff;
		/* short gain */
		/*
		 * write_cmos_sensor(0x0350e,(reg_d_sgain >> 8));
		 * write_cmos_sensor(0x0350f,(reg_d_sgain&0xff));
		 * write_cmos_sensor(0x0350c, 0x07);
		 * write_cmos_sensor(0x0350d, 0xc0);
		 */
		cam_pr_debug("setting reg_d_sgain = %d\n ", reg_d_sgain);
		imgsensor.hdr_d_sgain = reg_d_sgain;
		imgsensor.hdr_a_sgain = 0x07c0;
	} else {
		if (reg_sgain < 0x80)// sensor 1xGain
			reg_sgain = 0x80;

		/* binary to find A_Gain */
		reg_a_gain = binary_find_AGain(AGain_table,
			reg_sgain, ARRAY_SIZE(AGain_table));

		/* in case of insufficient accurary, use D_Gain supplement A_Gain */
		reg_d_gain = reg_sgain*1024/reg_a_gain;

		/* short exposure */
		/*
		 * write_cmos_sensor(0x0350e, (reg_d_gain >> 8));
		 * write_cmos_sensor(0x0350f, (reg_d_gain&0xff));
		 * write_cmos_sensor(0x0350c, (reg_a_gain >> 8));
		 * write_cmos_sensor(0x0350d, (reg_a_gain&0xff));
		 */
		cam_pr_debug("setting SHORT Gain reg_a_gain = %d reg_d_gain = %d\n ",
			reg_a_gain, reg_d_gain);
		imgsensor.hdr_d_sgain = reg_d_gain;
		imgsensor.hdr_a_sgain = reg_a_gain;
	}

	//imgsensor.framelength_tmp = ((read_cmos_sensor(0x380e)<<8) + read_cmos_sensor(0x380f));

	if (!first_config) {
		write_cmos_sensor(0x320d, 0x00);
		write_cmos_sensor(0x3208, 0x00);
		write_cmos_sensor(0x0808, 0x00);
	}
	/*
	 * if (imgsensor.frame_length < imgsensor.framelength_tmp) {
	 *	write_cmos_sensor(0x380e, imgsensor.framelength_tmp >> 8);
	 *	write_cmos_sensor(0x380f, imgsensor.framelength_tmp & 0xFF);
	 *	cam_pr_debug("debug setting imgsensor.framelength_tmp\n");
	 * } else {
	 *	write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
	 *	write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
	 *	cam_pr_debug("debug setting imgsensor.frame_length\n");
	 * }
	 */
	write_cmos_sensor(0x380e, 0x09);
	write_cmos_sensor(0x380f, 0xc4);
	//write_cmos_sensor(0x380e, 0x13);
	//write_cmos_sensor(0x380f, 0x88);

	// Long exposure
	write_cmos_sensor(0x3501, (imgsensor.hdr_LE >> 8) & 0xFF);
	write_cmos_sensor(0x3502, imgsensor.hdr_LE & 0xFF);
	// Short exposure
	write_cmos_sensor(0x3511, (imgsensor.hdr_SE >> 8) & 0xFF);
	write_cmos_sensor(0x3512, imgsensor.hdr_SE & 0xFF);
	write_cmos_sensor(0x3508, imgsensor.hdr_a_lgain >> 8);
	write_cmos_sensor(0x3509, imgsensor.hdr_a_lgain & 0xFF);
	write_cmos_sensor(0x350c, imgsensor.hdr_a_sgain >> 8);
	write_cmos_sensor(0x350d, imgsensor.hdr_a_sgain & 0xFF);
	write_cmos_sensor(0x350a, imgsensor.hdr_d_lgain >> 8);
	write_cmos_sensor(0x350b, imgsensor.hdr_d_lgain & 0xFF);
	write_cmos_sensor(0x350e, imgsensor.hdr_d_sgain >> 8);
	write_cmos_sensor(0x350f, imgsensor.hdr_d_sgain & 0xFF);

	if (!first_config) {
		cam_pr_debug("group config\n");
		write_cmos_sensor(0x3208, 0x10);
		write_cmos_sensor(0x320d, 0x00);
		write_cmos_sensor(0x3208, 0xa0);
	} else {
		cam_pr_debug("init config\n");
		first_config = 0;
	}

	cam_pr_debug("Exit shutter LE =%d SE =%d, framelength =%d\n",
		imgsensor.hdr_LE, imgsensor.hdr_SE,
		imgsensor.frame_length);
	cam_pr_debug("Exit gain L_AG =%d L_SG =%d S_AG =%d  S_DG =%d\n",
		imgsensor.hdr_a_lgain, imgsensor.hdr_d_lgain,
		imgsensor.hdr_a_sgain, imgsensor.hdr_d_sgain);
	cam_pr_debug("Exit setting hdr_dual_gain\n");
}


static void ihdr_write_shutter_gain(kal_uint16 le,
	kal_uint16 se, kal_uint16 gain)
{
	/* not support HDR */
	/* cam_pr_debug("le:0x%x, se:0x%x, gain:0x%x\n",le,se,gain); */
}

#if 0
static void set_mirror_flip(kal_uint8 image_mirror)
{
	cam_pr_debug("image_mirror = %d\n", image_mirror);

	/********************************************************
	 *
	 *   0x3820[2] ISP Vertical flip
	 *   0x3820[1] Sensor Vertical flip
	 *
	 *   0x3821[2] ISP Horizontal mirror
	 *   0x3821[1] Sensor Horizontal mirror
	 *
	 *   ISP and Sensor flip or mirror register bit should be the same!!
	 *
	 ********************************************************/

	switch (image_mirror) {
	case IMAGE_NORMAL:
		write_cmos_sensor(0x3820,
			((read_cmos_sensor(0x3820) & 0xF9) | 0x00));
		write_cmos_sensor(0x3821,
			((read_cmos_sensor(0x3821) & 0xF9) | 0x06));
		break;
	case IMAGE_H_MIRROR:
		write_cmos_sensor(0x3820,
			((read_cmos_sensor(0x3820) & 0xF9) | 0x00));
		write_cmos_sensor(0x3821,
			((read_cmos_sensor(0x3821) & 0xF9) | 0x00));
		break;
	case IMAGE_V_MIRROR:
		write_cmos_sensor(0x3820,
			((read_cmos_sensor(0x3820) & 0xF9) | 0x06));
		write_cmos_sensor(0x3821,
			((read_cmos_sensor(0x3821) & 0xF9) | 0x06));
		break;
	case IMAGE_HV_MIRROR:
		write_cmos_sensor(0x3820,
			((read_cmos_sensor(0x3820) & 0xF9) | 0x06));
		write_cmos_sensor(0x3821,
			((read_cmos_sensor(0x3821) & 0xF9) | 0x00));
		break;
	default:
		cam_pr_debug("Error image_mirror setting\n");
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
}				/*      night_mode      */

static void sensor_init(void)
{
	cam_pr_debug("E\n");

	write_cmos_sensor(0x0103, 0x01);
	mdelay(5);
	write_cmos_sensor(0x0303, 0x01);
	write_cmos_sensor(0x0305, 0x44);
	write_cmos_sensor(0x0306, 0x00);
	write_cmos_sensor(0x0307, 0x00);
	write_cmos_sensor(0x0308, 0x03);
	write_cmos_sensor(0x0309, 0x04);
	write_cmos_sensor(0x030c, 0x01);
	write_cmos_sensor(0x0322, 0x01);
	write_cmos_sensor(0x032a, 0x00);
	write_cmos_sensor(0x031e, 0x09);
	write_cmos_sensor(0x0325, 0x48);
	write_cmos_sensor(0x0328, 0x07);
	write_cmos_sensor(0x300d, 0x11);
	write_cmos_sensor(0x300e, 0x11);
	write_cmos_sensor(0x300f, 0x11);
	write_cmos_sensor(0x3010, 0x01);
	write_cmos_sensor(0x3012, 0x41);
	write_cmos_sensor(0x3016, 0xf0);
	write_cmos_sensor(0x3018, 0xf0);
	write_cmos_sensor(0x3028, 0xf0);
	write_cmos_sensor(0x301e, 0x98);
	write_cmos_sensor(0x3010, 0x04);
	write_cmos_sensor(0x3011, 0x06);
	write_cmos_sensor(0x3031, 0xa9);
	write_cmos_sensor(0x3103, 0x48);
	write_cmos_sensor(0x3104, 0x01);
	write_cmos_sensor(0x3106, 0x10);
	write_cmos_sensor(0x3400, 0x04);
	write_cmos_sensor(0x3025, 0x03);
	write_cmos_sensor(0x3425, 0x01);
	write_cmos_sensor(0x3428, 0x01);
	write_cmos_sensor(0x3406, 0x08);
	write_cmos_sensor(0x3408, 0x03);
	write_cmos_sensor(0x3501, 0x09);
	write_cmos_sensor(0x3502, 0xa0);
	write_cmos_sensor(0x3505, 0x83);
	write_cmos_sensor(0x3508, 0x00);
	write_cmos_sensor(0x3509, 0x80);
	write_cmos_sensor(0x350a, 0x04);
	write_cmos_sensor(0x350b, 0x00);
	write_cmos_sensor(0x350c, 0x00);
	write_cmos_sensor(0x350d, 0x80);
	write_cmos_sensor(0x350e, 0x04);
	write_cmos_sensor(0x350f, 0x00);
	write_cmos_sensor(0x3600, 0x00);
	write_cmos_sensor(0x3626, 0xff);
	write_cmos_sensor(0x3605, 0x50);
	write_cmos_sensor(0x3609, 0xb5);
	write_cmos_sensor(0x3610, 0x69);
	write_cmos_sensor(0x360c, 0x01);
	write_cmos_sensor(0x3628, 0xa4);
	write_cmos_sensor(0x3629, 0x6a);
	write_cmos_sensor(0x362d, 0x10);
	write_cmos_sensor(0x3660, 0x43);
	write_cmos_sensor(0x3661, 0x06);
	write_cmos_sensor(0x3662, 0x00);
	write_cmos_sensor(0x3663, 0x28);
	write_cmos_sensor(0x3664, 0x0d);
	write_cmos_sensor(0x366a, 0x38);
	write_cmos_sensor(0x366b, 0xa0);
	write_cmos_sensor(0x366d, 0x00);
	write_cmos_sensor(0x366e, 0x00);
	write_cmos_sensor(0x3680, 0x00);
	write_cmos_sensor(0x36c0, 0x00);
	write_cmos_sensor(0x3621, 0x81);
	write_cmos_sensor(0x3634, 0x31);
	write_cmos_sensor(0x3620, 0x00);
	write_cmos_sensor(0x3622, 0x00);
	write_cmos_sensor(0x362a, 0xd0);
	write_cmos_sensor(0x362e, 0x8c);
	write_cmos_sensor(0x362f, 0x98);
	write_cmos_sensor(0x3630, 0xb0);
	write_cmos_sensor(0x3631, 0xd7);
	write_cmos_sensor(0x3701, 0x0f);
	write_cmos_sensor(0x3737, 0x02);
	write_cmos_sensor(0x3740, 0x18);
	write_cmos_sensor(0x3741, 0x04);
	write_cmos_sensor(0x373c, 0x0f);
	write_cmos_sensor(0x373b, 0x02);
	write_cmos_sensor(0x3705, 0x00);
	write_cmos_sensor(0x3706, 0x50);
	write_cmos_sensor(0x370a, 0x00);
	write_cmos_sensor(0x370b, 0xe4);
	write_cmos_sensor(0x3709, 0x4a);
	write_cmos_sensor(0x3714, 0x21);
	write_cmos_sensor(0x371c, 0x00);
	write_cmos_sensor(0x371d, 0x08);
	write_cmos_sensor(0x375e, 0x0e);
	write_cmos_sensor(0x3760, 0x13);
	write_cmos_sensor(0x3776, 0x10);
	write_cmos_sensor(0x3781, 0x02);
	write_cmos_sensor(0x3782, 0x04);
	write_cmos_sensor(0x3783, 0x02);
	write_cmos_sensor(0x3784, 0x08);
	write_cmos_sensor(0x3785, 0x08);
	write_cmos_sensor(0x3788, 0x01);
	write_cmos_sensor(0x3789, 0x01);
	write_cmos_sensor(0x3797, 0x84);
	write_cmos_sensor(0x3798, 0x01);
	write_cmos_sensor(0x3799, 0x00);
	write_cmos_sensor(0x3761, 0x02);
	write_cmos_sensor(0x3762, 0x0d);
	write_cmos_sensor(0x3800, 0x00);
	write_cmos_sensor(0x3801, 0x00);
	write_cmos_sensor(0x3802, 0x00);
	write_cmos_sensor(0x3803, 0x0c);
	write_cmos_sensor(0x3804, 0x0e);
	write_cmos_sensor(0x3805, 0xff);
	write_cmos_sensor(0x3806, 0x08);
	write_cmos_sensor(0x3807, 0x6f);
	write_cmos_sensor(0x3808, 0x0a);
	write_cmos_sensor(0x3809, 0x00);   // 0a00 2560
	write_cmos_sensor(0x380a, 0x07);
	write_cmos_sensor(0x380b, 0x80);   // 0780 1920
	write_cmos_sensor(0x380c, 0x02);
	write_cmos_sensor(0x380d, 0xd0);
	write_cmos_sensor(0x380e, 0x09);
	write_cmos_sensor(0x380f, 0xc0);
	write_cmos_sensor(0x3813, 0x04);
	write_cmos_sensor(0x3814, 0x01);
	write_cmos_sensor(0x3815, 0x01);
	write_cmos_sensor(0x3816, 0x01);
	write_cmos_sensor(0x3817, 0x01);
	write_cmos_sensor(0x381c, 0x00);
	write_cmos_sensor(0x3820, 0x00);
	write_cmos_sensor(0x3821, 0x04);
	write_cmos_sensor(0x3823, 0x18);
	write_cmos_sensor(0x3826, 0x00);
	write_cmos_sensor(0x3827, 0x01);
	write_cmos_sensor(0x3833, 0x00);
	write_cmos_sensor(0x3832, 0x02);
	write_cmos_sensor(0x383c, 0x48);
	write_cmos_sensor(0x383d, 0xff);   // full size
	write_cmos_sensor(0x3843, 0x20);
	write_cmos_sensor(0x382d, 0x08);
	write_cmos_sensor(0x3d85, 0x0b);
	write_cmos_sensor(0x3d84, 0x40);
	write_cmos_sensor(0x3d8c, 0x63);
	write_cmos_sensor(0x3d8d, 0x00);
	write_cmos_sensor(0x4000, 0x78);
	write_cmos_sensor(0x4001, 0x2b);
	write_cmos_sensor(0x4004, 0x00);
	write_cmos_sensor(0x4005, 0x40);
	write_cmos_sensor(0x4028, 0x2f);
	write_cmos_sensor(0x400a, 0x01);
	write_cmos_sensor(0x4010, 0x12);
	write_cmos_sensor(0x4008, 0x02);
	write_cmos_sensor(0x4009, 0x0d);
	write_cmos_sensor(0x401a, 0x58);
	write_cmos_sensor(0x4050, 0x00);
	write_cmos_sensor(0x4051, 0x01);
	write_cmos_sensor(0x4052, 0x00);
	write_cmos_sensor(0x4053, 0x80);
	write_cmos_sensor(0x4054, 0x00);
	write_cmos_sensor(0x4055, 0x80);
	write_cmos_sensor(0x4056, 0x00);
	write_cmos_sensor(0x4057, 0x80);
	write_cmos_sensor(0x4058, 0x00);
	write_cmos_sensor(0x4059, 0x80);
	write_cmos_sensor(0x430b, 0xff);
	write_cmos_sensor(0x430c, 0xff);
	write_cmos_sensor(0x430d, 0x00);
	write_cmos_sensor(0x430e, 0x00);
	write_cmos_sensor(0x4501, 0x18);
	write_cmos_sensor(0x4502, 0x00);
	write_cmos_sensor(0x4643, 0x00);
	write_cmos_sensor(0x4640, 0x01);
	write_cmos_sensor(0x4641, 0x04);
	write_cmos_sensor(0x480e, 0x00);
	write_cmos_sensor(0x4813, 0x00);
	write_cmos_sensor(0x4815, 0x2b);
	write_cmos_sensor(0x486e, 0x36);
	write_cmos_sensor(0x486f, 0x84);
	write_cmos_sensor(0x4860, 0x00);
	write_cmos_sensor(0x4861, 0xa0);
	write_cmos_sensor(0x484b, 0x05);
	write_cmos_sensor(0x4850, 0x00);
	write_cmos_sensor(0x4851, 0xaa);
	write_cmos_sensor(0x4852, 0xff);
	write_cmos_sensor(0x4853, 0x8a);
	write_cmos_sensor(0x4854, 0x08);
	write_cmos_sensor(0x4855, 0x30);
	write_cmos_sensor(0x4800, 0x60);
	write_cmos_sensor(0x4837, 0x1d);
	write_cmos_sensor(0x484a, 0x3f);
	write_cmos_sensor(0x5000, 0xc9);
	write_cmos_sensor(0x5001, 0x43);
	write_cmos_sensor(0x5002, 0x00);
	write_cmos_sensor(0x5211, 0x03);
	write_cmos_sensor(0x5291, 0x03);
	write_cmos_sensor(0x520d, 0x0f);
	write_cmos_sensor(0x520e, 0xfd);
	write_cmos_sensor(0x520f, 0xa5);
	write_cmos_sensor(0x5210, 0xa5);
	write_cmos_sensor(0x528d, 0x0f);
	write_cmos_sensor(0x528e, 0xfd);
	write_cmos_sensor(0x528f, 0xa5);
	write_cmos_sensor(0x5290, 0xa5);
	write_cmos_sensor(0x5004, 0x40);
	write_cmos_sensor(0x5005, 0x00);
	write_cmos_sensor(0x5180, 0x00);
	write_cmos_sensor(0x5181, 0x10);
	write_cmos_sensor(0x5182, 0x0f);
	write_cmos_sensor(0x5183, 0xff);
	write_cmos_sensor(0x580b, 0x03);
	write_cmos_sensor(0x4d00, 0x03);
	write_cmos_sensor(0x4d01, 0xe9);
	write_cmos_sensor(0x4d02, 0xba);
	write_cmos_sensor(0x4d03, 0x66);
	write_cmos_sensor(0x4d04, 0x46);
	write_cmos_sensor(0x4d05, 0xa5);
	write_cmos_sensor(0x3603, 0x3c);
	write_cmos_sensor(0x3703, 0x26);
	write_cmos_sensor(0x3709, 0x49);
	write_cmos_sensor(0x3708, 0x2d);
	write_cmos_sensor(0x3719, 0x1c);
	write_cmos_sensor(0x371a, 0x06);
	write_cmos_sensor(0x4000, 0x79);
	write_cmos_sensor(0x3501, 0x09);
	write_cmos_sensor(0x3502, 0xbc);
}				/*    MIPI_sensor_Init  */


static void preview_setting(void)
{
	/********************************************************
	 *
	 *   2592x1944 30fps 4 lane MIPI 544Mbps/lane
	 *
	 ********************************************************/
	cam_pr_debug("E\n");
	write_cmos_sensor(0x0100, 0x00); /* Stream Off */
	write_cmos_sensor(0x0103, 0x01);
	mdelay(5);
	write_cmos_sensor(0x0303, 0x01);
	write_cmos_sensor(0x0305, 0x5e);
	write_cmos_sensor(0x0306, 0x00);
	write_cmos_sensor(0x0307, 0x00);
	write_cmos_sensor(0x0308, 0x03);
	write_cmos_sensor(0x0309, 0x04);
	write_cmos_sensor(0x032a, 0x00);
	write_cmos_sensor(0x031e, 0x09);
	write_cmos_sensor(0x0325, 0x48);
	write_cmos_sensor(0x0328, 0x07);
	write_cmos_sensor(0x300d, 0x11);
	write_cmos_sensor(0x300e, 0x11);
	write_cmos_sensor(0x300f, 0x11);
	write_cmos_sensor(0x3010, 0x01);
	write_cmos_sensor(0x3012, 0x41);
	write_cmos_sensor(0x3016, 0xf0);
	write_cmos_sensor(0x3018, 0xf0);
	write_cmos_sensor(0x3028, 0xf0);
	write_cmos_sensor(0x301e, 0x98);
	write_cmos_sensor(0x3010, 0x04);
	write_cmos_sensor(0x3011, 0x06);
	write_cmos_sensor(0x3031, 0xa9);
	write_cmos_sensor(0x3103, 0x48);
	write_cmos_sensor(0x3104, 0x01);
	write_cmos_sensor(0x3106, 0x10);
	write_cmos_sensor(0x3400, 0x04);
	write_cmos_sensor(0x3025, 0x03);
	write_cmos_sensor(0x3425, 0x01);
	write_cmos_sensor(0x3428, 0x01);
	write_cmos_sensor(0x3406, 0x08);
	write_cmos_sensor(0x3408, 0x03);
	write_cmos_sensor(0x3501, 0x09);
	write_cmos_sensor(0x3502, 0x2c);
	write_cmos_sensor(0x3505, 0x83);
	write_cmos_sensor(0x3508, 0x00);
	write_cmos_sensor(0x3509, 0x80);
	write_cmos_sensor(0x350a, 0x04);
	write_cmos_sensor(0x350b, 0x00);
	write_cmos_sensor(0x350c, 0x00);
	write_cmos_sensor(0x350d, 0x80);
	write_cmos_sensor(0x350e, 0x04);
	write_cmos_sensor(0x350f, 0x00);
	write_cmos_sensor(0x3600, 0x00);
	write_cmos_sensor(0x3626, 0xff);
	write_cmos_sensor(0x3605, 0x50);
	write_cmos_sensor(0x3609, 0xb5);
	write_cmos_sensor(0x3610, 0x69);
	write_cmos_sensor(0x360c, 0x01);
	write_cmos_sensor(0x3628, 0xa4);
	write_cmos_sensor(0x3629, 0x6a);
	write_cmos_sensor(0x362d, 0x10);
	write_cmos_sensor(0x3660, 0x43);
	write_cmos_sensor(0x3661, 0x06);
	write_cmos_sensor(0x3662, 0x00);
	write_cmos_sensor(0x3663, 0x28);
	write_cmos_sensor(0x3664, 0x0d);
	write_cmos_sensor(0x366a, 0x38);
	write_cmos_sensor(0x366b, 0xa0);
	write_cmos_sensor(0x366d, 0x00);
	write_cmos_sensor(0x366e, 0x00);
	write_cmos_sensor(0x3680, 0x00);
	write_cmos_sensor(0x36c0, 0x00);
	write_cmos_sensor(0x3621, 0x81);
	write_cmos_sensor(0x3634, 0x31);
	write_cmos_sensor(0x3620, 0x00);
	write_cmos_sensor(0x3622, 0x00);
	write_cmos_sensor(0x362a, 0xd0);
	write_cmos_sensor(0x362e, 0x8c);
	write_cmos_sensor(0x362f, 0x98);
	write_cmos_sensor(0x3630, 0xb0);
	write_cmos_sensor(0x3631, 0xd7);
	write_cmos_sensor(0x3701, 0x0f);
	write_cmos_sensor(0x3737, 0x02);
	write_cmos_sensor(0x3740, 0x18);
	write_cmos_sensor(0x3741, 0x04);
	write_cmos_sensor(0x373c, 0x0f);
	write_cmos_sensor(0x373b, 0x02);
	write_cmos_sensor(0x3705, 0x00);
	write_cmos_sensor(0x3706, 0x50);
	write_cmos_sensor(0x370a, 0x00);
	write_cmos_sensor(0x370b, 0xe4);
	write_cmos_sensor(0x3709, 0x4a);
	write_cmos_sensor(0x3714, 0x21);
	write_cmos_sensor(0x371c, 0x00);
	write_cmos_sensor(0x371d, 0x08);
	write_cmos_sensor(0x375e, 0x0e);
	write_cmos_sensor(0x3760, 0x13);
	write_cmos_sensor(0x3776, 0x10);
	write_cmos_sensor(0x3781, 0x02);
	write_cmos_sensor(0x3782, 0x04);
	write_cmos_sensor(0x3783, 0x02);
	write_cmos_sensor(0x3784, 0x08);
	write_cmos_sensor(0x3785, 0x08);
	write_cmos_sensor(0x3788, 0x01);
	write_cmos_sensor(0x3789, 0x01);
	write_cmos_sensor(0x3797, 0x84);
	write_cmos_sensor(0x3798, 0x01);
	write_cmos_sensor(0x3799, 0x00);
	write_cmos_sensor(0x3761, 0x02);
	write_cmos_sensor(0x3762, 0x0d);
	write_cmos_sensor(0x3800, 0x00);
	write_cmos_sensor(0x3801, 0x00);
	write_cmos_sensor(0x3802, 0x00);
	write_cmos_sensor(0x3803, 0x0c);
	write_cmos_sensor(0x3804, 0x0e);
	write_cmos_sensor(0x3805, 0xff);
	write_cmos_sensor(0x3806, 0x08);
	write_cmos_sensor(0x3807, 0x6f);
	write_cmos_sensor(0x3808, 0x0a);
	write_cmos_sensor(0x3809, 0x00);
	write_cmos_sensor(0x380a, 0x07);
	write_cmos_sensor(0x380b, 0x80);
	write_cmos_sensor(0x380c, 0x03);
	write_cmos_sensor(0x380d, 0xf0);
	write_cmos_sensor(0x380e, 0x09);
	write_cmos_sensor(0x380f, 0x4c);
	write_cmos_sensor(0x3813, 0x04);
	write_cmos_sensor(0x3814, 0x01);
	write_cmos_sensor(0x3815, 0x01);
	write_cmos_sensor(0x3816, 0x01);
	write_cmos_sensor(0x3817, 0x01);
	write_cmos_sensor(0x381c, 0x00);
	write_cmos_sensor(0x3820, 0x00);
	write_cmos_sensor(0x3821, 0x04);
	write_cmos_sensor(0x3823, 0x18);
	write_cmos_sensor(0x3826, 0x00);
	write_cmos_sensor(0x3827, 0x01);
	write_cmos_sensor(0x3833, 0x00);
	write_cmos_sensor(0x3832, 0x02);
	write_cmos_sensor(0x383c, 0x48);
	write_cmos_sensor(0x383d, 0xff);
	write_cmos_sensor(0x3843, 0x20);
	write_cmos_sensor(0x382d, 0x08);
	write_cmos_sensor(0x3d85, 0x0b);
	write_cmos_sensor(0x3d84, 0x40);
	write_cmos_sensor(0x3d8c, 0x63);
	write_cmos_sensor(0x3d8d, 0x00);
	write_cmos_sensor(0x4000, 0x78);
	write_cmos_sensor(0x4001, 0x2b);
	write_cmos_sensor(0x4004, 0x00);
	write_cmos_sensor(0x4005, 0x40);
	write_cmos_sensor(0x4028, 0x2f);
	write_cmos_sensor(0x400a, 0x01);
	write_cmos_sensor(0x4010, 0x12);
	write_cmos_sensor(0x4008, 0x02);
	write_cmos_sensor(0x4009, 0x0d);
	write_cmos_sensor(0x401a, 0x58);
	write_cmos_sensor(0x4050, 0x00);
	write_cmos_sensor(0x4051, 0x01);
	write_cmos_sensor(0x4052, 0x00);
	write_cmos_sensor(0x4053, 0x80);
	write_cmos_sensor(0x4054, 0x00);
	write_cmos_sensor(0x4055, 0x80);
	write_cmos_sensor(0x4056, 0x00);
	write_cmos_sensor(0x4057, 0x80);
	write_cmos_sensor(0x4058, 0x00);
	write_cmos_sensor(0x4059, 0x80);
	write_cmos_sensor(0x430b, 0xff);
	write_cmos_sensor(0x430c, 0xff);
	write_cmos_sensor(0x430d, 0x00);
	write_cmos_sensor(0x430e, 0x00);
	write_cmos_sensor(0x4501, 0x18);
	write_cmos_sensor(0x4502, 0x00);
	write_cmos_sensor(0x4643, 0x00);
	write_cmos_sensor(0x4640, 0x01);
	write_cmos_sensor(0x4641, 0x04);
	write_cmos_sensor(0x480e, 0x00);
	write_cmos_sensor(0x4813, 0x00);
	write_cmos_sensor(0x4815, 0x2b);
	write_cmos_sensor(0x486e, 0x36);
	write_cmos_sensor(0x486f, 0x84);
	write_cmos_sensor(0x4860, 0x00);
	write_cmos_sensor(0x4861, 0xa0);
	write_cmos_sensor(0x484b, 0x05);
	write_cmos_sensor(0x4850, 0x00);
	write_cmos_sensor(0x4851, 0xaa);
	write_cmos_sensor(0x4852, 0xff);
	write_cmos_sensor(0x4853, 0x8a);
	write_cmos_sensor(0x4854, 0x08);
	write_cmos_sensor(0x4855, 0x30);
	write_cmos_sensor(0x4800, 0x60);
	write_cmos_sensor(0x4837, 0x0a);
	write_cmos_sensor(0x484a, 0x3f);
	write_cmos_sensor(0x5000, 0xc9);
	write_cmos_sensor(0x5001, 0x43);
	write_cmos_sensor(0x5002, 0x00);
	write_cmos_sensor(0x5211, 0x03);
	write_cmos_sensor(0x5291, 0x03);
	write_cmos_sensor(0x520d, 0x0f);
	write_cmos_sensor(0x520e, 0xfd);
	write_cmos_sensor(0x520f, 0xa5);
	write_cmos_sensor(0x5210, 0xa5);
	write_cmos_sensor(0x528d, 0x0f);
	write_cmos_sensor(0x528e, 0xfd);
	write_cmos_sensor(0x528f, 0xa5);
	write_cmos_sensor(0x5290, 0xa5);
	write_cmos_sensor(0x5004, 0x40);
	write_cmos_sensor(0x5005, 0x00);
	write_cmos_sensor(0x5180, 0x00);
	write_cmos_sensor(0x5181, 0x10);
	write_cmos_sensor(0x5182, 0x0f);
	write_cmos_sensor(0x5183, 0xff);
	write_cmos_sensor(0x580b, 0x03);
	write_cmos_sensor(0x4d00, 0x03);
	write_cmos_sensor(0x4d01, 0xe9);
	write_cmos_sensor(0x4d02, 0xba);
	write_cmos_sensor(0x4d03, 0x66);
	write_cmos_sensor(0x4d04, 0x46);
	write_cmos_sensor(0x4d05, 0xa5);
	write_cmos_sensor(0x3603, 0x3c);
	write_cmos_sensor(0x3703, 0x26);
	write_cmos_sensor(0x3709, 0x49);
	write_cmos_sensor(0x3708, 0x2d);
	write_cmos_sensor(0x3719, 0x1c);
	write_cmos_sensor(0x371a, 0x06);
	write_cmos_sensor(0x4000, 0x79);
	write_cmos_sensor(0x380c, 0x02);
	write_cmos_sensor(0x380d, 0xd0);
	write_cmos_sensor(0x380e, 0x13);
	write_cmos_sensor(0x380f, 0x88);
	write_cmos_sensor(0x3501, 0x13);
	write_cmos_sensor(0x3502, 0x80);
	write_cmos_sensor(0x0100, 0x01);
	cam_pr_debug("X\n");
} /* preview_setting */

static void video_1080p_setting(void)
{
	cam_pr_debug("E\n");
	write_cmos_sensor(0x0303, 0x01);
	write_cmos_sensor(0x0305, 0x44);
	write_cmos_sensor(0x0306, 0x00);
	write_cmos_sensor(0x0307, 0x00);
	write_cmos_sensor(0x0308, 0x03);
	write_cmos_sensor(0x0309, 0x04);
	write_cmos_sensor(0x030c, 0x01);
	write_cmos_sensor(0x0322, 0x01);
	write_cmos_sensor(0x032a, 0x00);
	write_cmos_sensor(0x031e, 0x09);
	write_cmos_sensor(0x0325, 0x48);
	write_cmos_sensor(0x0328, 0x07);
	write_cmos_sensor(0x300d, 0x11);
	write_cmos_sensor(0x300e, 0x11);
	write_cmos_sensor(0x300f, 0x11);
	write_cmos_sensor(0x3010, 0x01);
	write_cmos_sensor(0x3012, 0x41);
	write_cmos_sensor(0x3016, 0xf0);
	write_cmos_sensor(0x3018, 0xf0);
	write_cmos_sensor(0x3028, 0xf0);
	write_cmos_sensor(0x301e, 0x98);
	write_cmos_sensor(0x3010, 0x04);
	write_cmos_sensor(0x3011, 0x06);
	write_cmos_sensor(0x3031, 0xa9);
	write_cmos_sensor(0x3103, 0x48);
	write_cmos_sensor(0x3104, 0x01);
	write_cmos_sensor(0x3106, 0x10);
	write_cmos_sensor(0x3400, 0x04);
	write_cmos_sensor(0x3025, 0x03);
	write_cmos_sensor(0x3425, 0x01);
	write_cmos_sensor(0x3428, 0x01);
	write_cmos_sensor(0x3406, 0x08);
	write_cmos_sensor(0x3408, 0x03);
	write_cmos_sensor(0x3501, 0x09);
	write_cmos_sensor(0x3502, 0xa0);
	write_cmos_sensor(0x3505, 0x83);
	write_cmos_sensor(0x3508, 0x00);
	write_cmos_sensor(0x3509, 0x80);
	write_cmos_sensor(0x350a, 0x04);
	write_cmos_sensor(0x350b, 0x00);
	write_cmos_sensor(0x350c, 0x00);
	write_cmos_sensor(0x350d, 0x80);
	write_cmos_sensor(0x350e, 0x04);
	write_cmos_sensor(0x350f, 0x00);
	write_cmos_sensor(0x3600, 0x00);
	write_cmos_sensor(0x3626, 0xff);
	write_cmos_sensor(0x3605, 0x50);
	write_cmos_sensor(0x3609, 0xb5);
	write_cmos_sensor(0x3610, 0x69);
	write_cmos_sensor(0x360c, 0x01);
	write_cmos_sensor(0x3628, 0xa4);
	write_cmos_sensor(0x3629, 0x6a);
	write_cmos_sensor(0x362d, 0x10);
	write_cmos_sensor(0x3660, 0x43);
	write_cmos_sensor(0x3661, 0x06);
	write_cmos_sensor(0x3662, 0x00);
	write_cmos_sensor(0x3663, 0x28);
	write_cmos_sensor(0x3664, 0x0d);
	write_cmos_sensor(0x366a, 0x38);
	write_cmos_sensor(0x366b, 0xa0);
	write_cmos_sensor(0x366d, 0x00);
	write_cmos_sensor(0x366e, 0x00);
	write_cmos_sensor(0x3680, 0x00);
	write_cmos_sensor(0x36c0, 0x00);
	write_cmos_sensor(0x3621, 0x81);
	write_cmos_sensor(0x3634, 0x31);
	write_cmos_sensor(0x3620, 0x00);
	write_cmos_sensor(0x3622, 0x00);
	write_cmos_sensor(0x362a, 0xd0);
	write_cmos_sensor(0x362e, 0x8c);
	write_cmos_sensor(0x362f, 0x98);
	write_cmos_sensor(0x3630, 0xb0);
	write_cmos_sensor(0x3631, 0xd7);
	write_cmos_sensor(0x3701, 0x0f);
	write_cmos_sensor(0x3737, 0x02);
	write_cmos_sensor(0x3740, 0x18);
	write_cmos_sensor(0x3741, 0x04);
	write_cmos_sensor(0x373c, 0x0f);
	write_cmos_sensor(0x373b, 0x02);
	write_cmos_sensor(0x3705, 0x00);
	write_cmos_sensor(0x3706, 0x50);
	write_cmos_sensor(0x370a, 0x00);
	write_cmos_sensor(0x370b, 0xe4);
	write_cmos_sensor(0x3709, 0x4a);
	write_cmos_sensor(0x3714, 0x21);
	write_cmos_sensor(0x371c, 0x00);
	write_cmos_sensor(0x371d, 0x08);
	write_cmos_sensor(0x375e, 0x0e);
	write_cmos_sensor(0x3760, 0x13);
	write_cmos_sensor(0x3776, 0x10);
	write_cmos_sensor(0x3781, 0x02);
	write_cmos_sensor(0x3782, 0x04);
	write_cmos_sensor(0x3783, 0x02);
	write_cmos_sensor(0x3784, 0x08);
	write_cmos_sensor(0x3785, 0x08);
	write_cmos_sensor(0x3788, 0x01);
	write_cmos_sensor(0x3789, 0x01);
	write_cmos_sensor(0x3797, 0x84);
	write_cmos_sensor(0x3798, 0x01);
	write_cmos_sensor(0x3799, 0x00);
	write_cmos_sensor(0x3761, 0x02);
	write_cmos_sensor(0x3762, 0x0d);
	write_cmos_sensor(0x3800, 0x00);
	write_cmos_sensor(0x3801, 0x00);
	write_cmos_sensor(0x3802, 0x00);
	write_cmos_sensor(0x3803, 0x0c);
	write_cmos_sensor(0x3804, 0x0e);
	write_cmos_sensor(0x3805, 0xff);
	write_cmos_sensor(0x3806, 0x08);
	write_cmos_sensor(0x3807, 0x6f);
	write_cmos_sensor(0x3808, 0x0a);
	write_cmos_sensor(0x3809, 0x00);
	write_cmos_sensor(0x380a, 0x07);
	write_cmos_sensor(0x380b, 0x80);
	write_cmos_sensor(0x380c, 0x02);
	write_cmos_sensor(0x380d, 0xd0);
	write_cmos_sensor(0x380e, 0x09);
	write_cmos_sensor(0x380f, 0xc0);
	write_cmos_sensor(0x3813, 0x04);
	write_cmos_sensor(0x3814, 0x01);
	write_cmos_sensor(0x3815, 0x01);
	write_cmos_sensor(0x3816, 0x01);
	write_cmos_sensor(0x3817, 0x01);
	write_cmos_sensor(0x381c, 0x00);
	write_cmos_sensor(0x3820, 0x00);
	write_cmos_sensor(0x3821, 0x04);
	write_cmos_sensor(0x3823, 0x18);
	write_cmos_sensor(0x3826, 0x00);
	write_cmos_sensor(0x3827, 0x01);
	write_cmos_sensor(0x3833, 0x00);
	write_cmos_sensor(0x3832, 0x02);
	write_cmos_sensor(0x383c, 0x48);
	write_cmos_sensor(0x383d, 0xff);
	write_cmos_sensor(0x3843, 0x20);
	write_cmos_sensor(0x382d, 0x08);
	write_cmos_sensor(0x3d85, 0x0b);
	write_cmos_sensor(0x3d84, 0x40);
	write_cmos_sensor(0x3d8c, 0x63);
	write_cmos_sensor(0x3d8d, 0x00);
	write_cmos_sensor(0x4000, 0x78);
	write_cmos_sensor(0x4001, 0x2b);
	write_cmos_sensor(0x4004, 0x00);
	write_cmos_sensor(0x4005, 0x40);
	write_cmos_sensor(0x4028, 0x2f);
	write_cmos_sensor(0x400a, 0x01);
	write_cmos_sensor(0x4010, 0x12);
	write_cmos_sensor(0x4008, 0x02);
	write_cmos_sensor(0x4009, 0x0d);
	write_cmos_sensor(0x401a, 0x58);
	write_cmos_sensor(0x4050, 0x00);
	write_cmos_sensor(0x4051, 0x01);
	write_cmos_sensor(0x4052, 0x00);
	write_cmos_sensor(0x4053, 0x80);
	write_cmos_sensor(0x4054, 0x00);
	write_cmos_sensor(0x4055, 0x80);
	write_cmos_sensor(0x4056, 0x00);
	write_cmos_sensor(0x4057, 0x80);
	write_cmos_sensor(0x4058, 0x00);
	write_cmos_sensor(0x4059, 0x80);
	write_cmos_sensor(0x430b, 0xff);
	write_cmos_sensor(0x430c, 0xff);
	write_cmos_sensor(0x430d, 0x00);
	write_cmos_sensor(0x430e, 0x00);
	write_cmos_sensor(0x4501, 0x18);
	write_cmos_sensor(0x4502, 0x00);
	write_cmos_sensor(0x4643, 0x00);
	write_cmos_sensor(0x4640, 0x01);
	write_cmos_sensor(0x4641, 0x04);
	write_cmos_sensor(0x480e, 0x00);
	write_cmos_sensor(0x4813, 0x00);
	write_cmos_sensor(0x4815, 0x2b);
	write_cmos_sensor(0x486e, 0x36);
	write_cmos_sensor(0x486f, 0x84);
	write_cmos_sensor(0x4860, 0x00);
	write_cmos_sensor(0x4861, 0xa0);
	write_cmos_sensor(0x484b, 0x05);
	write_cmos_sensor(0x4850, 0x00);
	write_cmos_sensor(0x4851, 0xaa);
	write_cmos_sensor(0x4852, 0xff);
	write_cmos_sensor(0x4853, 0x8a);
	write_cmos_sensor(0x4854, 0x08);
	write_cmos_sensor(0x4855, 0x30);
	write_cmos_sensor(0x4800, 0x60);
	write_cmos_sensor(0x4837, 0x1d);
	write_cmos_sensor(0x484a, 0x3f);
	write_cmos_sensor(0x5000, 0xc9);
	write_cmos_sensor(0x5001, 0x43);
	write_cmos_sensor(0x5002, 0x00);
	write_cmos_sensor(0x5211, 0x03);
	write_cmos_sensor(0x5291, 0x03);
	write_cmos_sensor(0x520d, 0x0f);
	write_cmos_sensor(0x520e, 0xfd);
	write_cmos_sensor(0x520f, 0xa5);
	write_cmos_sensor(0x5210, 0xa5);
	write_cmos_sensor(0x528d, 0x0f);
	write_cmos_sensor(0x528e, 0xfd);
	write_cmos_sensor(0x528f, 0xa5);
	write_cmos_sensor(0x5290, 0xa5);
	write_cmos_sensor(0x5004, 0x40);
	write_cmos_sensor(0x5005, 0x00);
	write_cmos_sensor(0x5180, 0x00);
	write_cmos_sensor(0x5181, 0x10);
	write_cmos_sensor(0x5182, 0x0f);
	write_cmos_sensor(0x5183, 0xff);
	write_cmos_sensor(0x580b, 0x03);
	write_cmos_sensor(0x4d00, 0x03);
	write_cmos_sensor(0x4d01, 0xe9);
	write_cmos_sensor(0x4d02, 0xba);
	write_cmos_sensor(0x4d03, 0x66);
	write_cmos_sensor(0x4d04, 0x46);
	write_cmos_sensor(0x4d05, 0xa5);
	write_cmos_sensor(0x3603, 0x3c);
	write_cmos_sensor(0x3703, 0x26);
	write_cmos_sensor(0x3709, 0x49);
	write_cmos_sensor(0x3708, 0x2d);
	write_cmos_sensor(0x3719, 0x1c);
	write_cmos_sensor(0x371a, 0x06);
	write_cmos_sensor(0x4000, 0x79);
	write_cmos_sensor(0x3501, 0x09);
	write_cmos_sensor(0x3502, 0xbc);
} /* preview_setting */

static void video_720p_setting(void)
{
	/********************************************************
	 *
	 *   720p 30fps 2 lane MIPI 420Mbps/lane
	 *    @@720p_30fps
	 *     ;;pclk=84M,HTS=3728,VTS=748
	 ********************************************************/
	cam_pr_debug("E\n");
	write_cmos_sensor(0x0100, 0x00);	/* Stream Off */

	mdelay(5);
	write_cmos_sensor(0x3500, 0x00);	/* exposure [19:16] */
	write_cmos_sensor(0x3501, 0x2d);	/* exposure */
	write_cmos_sensor(0x3502, 0xc0);	/* exposure */

	write_cmos_sensor(0x3708, 0x66);

	write_cmos_sensor(0x3709, 0x52);
	write_cmos_sensor(0x370c, 0xcf);
	write_cmos_sensor(0x3800, 0x00);	/* x start = 16 */
	write_cmos_sensor(0x3801, 0x10);	/* x start */
	write_cmos_sensor(0x3802, 0x00);	/* y start = 254 */
	write_cmos_sensor(0x3803, 0xfe);	/* y start */
	write_cmos_sensor(0x3804, 0x0a);	/* xend = 2607 */
	write_cmos_sensor(0x3805, 0x2f);	/* xend */
	write_cmos_sensor(0x3806, 0x06);	/* yend = 1701 */
	write_cmos_sensor(0x3807, 0xa5);	/* yend */
	write_cmos_sensor(0x3808, 0x05);	/* x output size = 1280 */
	write_cmos_sensor(0x3809, 0x00);	/* x output size */
	write_cmos_sensor(0x380a, 0x02);	/* y output size = 720 */
	write_cmos_sensor(0x380b, 0xd0);	/* y output size */

	write_cmos_sensor(0x380c,
		((imgsensor_info.slim_video.linelength >> 8) & 0xFF));
	write_cmos_sensor(0x380d,
		(imgsensor_info.slim_video.linelength & 0xFF));
	write_cmos_sensor(0x380e,
		((imgsensor_info.slim_video.framelength >> 8) & 0xFF));
	write_cmos_sensor(0x380f,
		(imgsensor_info.slim_video.framelength & 0xFF));

	write_cmos_sensor(0x3810, 0x00);	/* isp x win = 8 */
	write_cmos_sensor(0x3811, 0x08);	/* isp x win */
	write_cmos_sensor(0x3812, 0x00);	/* isp y win = 2 */
	write_cmos_sensor(0x3813, 0x02);	/* isp y win */
	write_cmos_sensor(0x3814, 0x31);	/* x inc */
	write_cmos_sensor(0x3815, 0x31);	/* y inc */
	write_cmos_sensor(0x3817, 0x00);	/* hsync start */


	/********************************************************
	 *
	 *   0x3820[2] ISP Vertical flip
	 *   0x3820[1] Sensor Vertical flip
	 *
	 *   0x3821[2] ISP Horizontal mirror
	 *   0x3821[1] Sensor Horizontal mirror
	 *
	 *   ISP and Sensor flip or mirror register bit should be the same!!
	 *
	 ********************************************************/
	write_cmos_sensor(0x3820, 0x08);	/* flip off, v bin off */
	write_cmos_sensor(0x3821, 0x07);	/* mirror on, h bin on */


	write_cmos_sensor(0x4004, 0x02);	/* black line number */
	write_cmos_sensor(0x4005, 0x18);	/* blc normal freeze */


	/* write_cmos_sensor(0x350b, 0x80); // gain = 8x */
	write_cmos_sensor(0x4837, 0x18);	/* MIPI global timing */

	write_cmos_sensor(0x0100, 0x01);	/* Stream On */

	cam_pr_debug("Exit!");
}				/*    preview_setting  */

static void hs_video_setting(void)
{
	cam_pr_debug("E\n");

	video_1080p_setting();
}

static void slim_video_setting(void)
{
	cam_pr_debug("E\n");

	video_720p_setting();
}
static void custom1_setting(void)
{
	cam_pr_debug("E\n");
	write_cmos_sensor(0x0100, 0x00);
	write_cmos_sensor(0x0103, 0x01);
	write_cmos_sensor(0x0303, 0x01);
	write_cmos_sensor(0x0305, 0x5e);
	write_cmos_sensor(0x0306, 0x00);
	write_cmos_sensor(0x0307, 0x00);
	write_cmos_sensor(0x0308, 0x03);
	write_cmos_sensor(0x0309, 0x04);
	write_cmos_sensor(0x032a, 0x00);
	write_cmos_sensor(0x031e, 0x09);
	write_cmos_sensor(0x0325, 0x48);
	write_cmos_sensor(0x0328, 0x07);
	write_cmos_sensor(0x300d, 0x11);
	write_cmos_sensor(0x300e, 0x11);
	write_cmos_sensor(0x300f, 0x11);
	write_cmos_sensor(0x3026, 0x00);
	write_cmos_sensor(0x3027, 0x00);
	write_cmos_sensor(0x3010, 0x01);
	write_cmos_sensor(0x3012, 0x41);
	write_cmos_sensor(0x3016, 0xf0);
	write_cmos_sensor(0x3018, 0xf0);
	write_cmos_sensor(0x3028, 0xf0);
	write_cmos_sensor(0x301e, 0x98);
	write_cmos_sensor(0x3010, 0x01);
	write_cmos_sensor(0x3011, 0x04);
	write_cmos_sensor(0x3031, 0xa9);
	write_cmos_sensor(0x3103, 0x48);
	write_cmos_sensor(0x3104, 0x01);
	write_cmos_sensor(0x3106, 0x10);
	write_cmos_sensor(0x3501, 0x09);
	write_cmos_sensor(0x3502, 0xa0);
	write_cmos_sensor(0x3505, 0x83);
	write_cmos_sensor(0x3508, 0x00);
	write_cmos_sensor(0x3509, 0x80);
	write_cmos_sensor(0x350a, 0x04);
	write_cmos_sensor(0x350b, 0x00);
	write_cmos_sensor(0x350c, 0x00);
	write_cmos_sensor(0x350d, 0x80);
	write_cmos_sensor(0x350e, 0x04);
	write_cmos_sensor(0x350f, 0x00);
	write_cmos_sensor(0x3600, 0x00);
	write_cmos_sensor(0x3626, 0xff);
	write_cmos_sensor(0x3605, 0x50);
	write_cmos_sensor(0x3609, 0xb5);
	write_cmos_sensor(0x3610, 0x69);
	write_cmos_sensor(0x360c, 0x01);
	write_cmos_sensor(0x3628, 0xa4);
	write_cmos_sensor(0x3629, 0x6a);
	write_cmos_sensor(0x362d, 0x10);
	write_cmos_sensor(0x3660, 0x42);
	write_cmos_sensor(0x3661, 0x07);
	write_cmos_sensor(0x3662, 0x00);
	write_cmos_sensor(0x3663, 0x28);
	write_cmos_sensor(0x3664, 0x0d);
	write_cmos_sensor(0x366a, 0x38);
	write_cmos_sensor(0x366b, 0xa0);
	write_cmos_sensor(0x366d, 0x00);
	write_cmos_sensor(0x366e, 0x00);
	write_cmos_sensor(0x3680, 0x00);
	write_cmos_sensor(0x36c0, 0x00);
	write_cmos_sensor(0x3621, 0x81);
	write_cmos_sensor(0x3634, 0x31);
	write_cmos_sensor(0x3620, 0x00);
	write_cmos_sensor(0x3622, 0x00);
	write_cmos_sensor(0x362a, 0xd0);
	write_cmos_sensor(0x362e, 0x8c);
	write_cmos_sensor(0x362f, 0x98);
	write_cmos_sensor(0x3630, 0xb0);
	write_cmos_sensor(0x3631, 0xd7);
	write_cmos_sensor(0x3701, 0x0f);
	write_cmos_sensor(0x3737, 0x02);
	write_cmos_sensor(0x3740, 0x18);
	write_cmos_sensor(0x3741, 0x04);
	write_cmos_sensor(0x373c, 0x0f);
	write_cmos_sensor(0x373b, 0x02);
	write_cmos_sensor(0x3705, 0x00);
	write_cmos_sensor(0x3706, 0x50);
	write_cmos_sensor(0x370a, 0x00);
	write_cmos_sensor(0x370b, 0xe4);
	write_cmos_sensor(0x3709, 0x4a);
	write_cmos_sensor(0x3714, 0x21);
	write_cmos_sensor(0x371c, 0x00);
	write_cmos_sensor(0x371d, 0x08);
	write_cmos_sensor(0x375e, 0x0e);
	write_cmos_sensor(0x3760, 0x13);
	write_cmos_sensor(0x3776, 0x10);
	write_cmos_sensor(0x3781, 0x02);
	write_cmos_sensor(0x3782, 0x04);
	write_cmos_sensor(0x3783, 0x02);
	write_cmos_sensor(0x3784, 0x08);
	write_cmos_sensor(0x3785, 0x08);
	write_cmos_sensor(0x3788, 0x01);
	write_cmos_sensor(0x3789, 0x01);
	write_cmos_sensor(0x3797, 0x04);
	write_cmos_sensor(0x3798, 0x01);
	write_cmos_sensor(0x3799, 0x00);
	write_cmos_sensor(0x3761, 0x02);
	write_cmos_sensor(0x3762, 0x0d);
	write_cmos_sensor(0x3800, 0x00);
	write_cmos_sensor(0x3801, 0x00);
	write_cmos_sensor(0x3802, 0x00);
	write_cmos_sensor(0x3803, 0x0c);
	write_cmos_sensor(0x3804, 0x0e);
	write_cmos_sensor(0x3805, 0xff);
	write_cmos_sensor(0x3806, 0x08);
	write_cmos_sensor(0x3807, 0x6f);
	write_cmos_sensor(0x3808, 0x0a);
	write_cmos_sensor(0x3809, 0x00);
	write_cmos_sensor(0x380a, 0x07);
	write_cmos_sensor(0x380b, 0x80);
	write_cmos_sensor(0x380c, 0x02);
	write_cmos_sensor(0x380d, 0xd0);
	write_cmos_sensor(0x380e, 0x09);
	write_cmos_sensor(0x380f, 0xc0);
	write_cmos_sensor(0x3811, 0x10);
	write_cmos_sensor(0x3813, 0x04);
	write_cmos_sensor(0x3814, 0x01);
	write_cmos_sensor(0x3815, 0x01);
	write_cmos_sensor(0x3816, 0x01);
	write_cmos_sensor(0x3817, 0x01);
	write_cmos_sensor(0x381c, 0x08);
	write_cmos_sensor(0x3820, 0x00);
	write_cmos_sensor(0x3821, 0x24);
	write_cmos_sensor(0x3822, 0x54);
	write_cmos_sensor(0x3823, 0x08);
	write_cmos_sensor(0x3826, 0x00);
	write_cmos_sensor(0x3827, 0x01);
	write_cmos_sensor(0x3833, 0x01);
	write_cmos_sensor(0x3832, 0x02);
	write_cmos_sensor(0x383c, 0x48);
	write_cmos_sensor(0x383d, 0xff);
	write_cmos_sensor(0x3843, 0x20);
	write_cmos_sensor(0x382d, 0x08);
	write_cmos_sensor(0x3d85, 0x0b);
	write_cmos_sensor(0x3d84, 0x40);
	write_cmos_sensor(0x3d8c, 0x63);
	write_cmos_sensor(0x3d8d, 0x00);
	write_cmos_sensor(0x4000, 0x78);
	write_cmos_sensor(0x4001, 0x2b);
	write_cmos_sensor(0x4004, 0x00);
	write_cmos_sensor(0x4005, 0x40);
	write_cmos_sensor(0x4028, 0x2f);
	write_cmos_sensor(0x400a, 0x01);
	write_cmos_sensor(0x4010, 0x12);
	write_cmos_sensor(0x4008, 0x02);
	write_cmos_sensor(0x4009, 0x0d);
	write_cmos_sensor(0x401a, 0x58);
	write_cmos_sensor(0x4050, 0x00);
	write_cmos_sensor(0x4051, 0x01);
	write_cmos_sensor(0x4052, 0x00);
	write_cmos_sensor(0x4053, 0x80);
	write_cmos_sensor(0x4054, 0x00);
	write_cmos_sensor(0x4055, 0x80);
	write_cmos_sensor(0x4056, 0x00);
	write_cmos_sensor(0x4057, 0x80);
	write_cmos_sensor(0x4058, 0x00);
	write_cmos_sensor(0x4059, 0x80);
	write_cmos_sensor(0x430b, 0xff);
	write_cmos_sensor(0x430c, 0xff);
	write_cmos_sensor(0x430d, 0x00);
	write_cmos_sensor(0x430e, 0x00);
	write_cmos_sensor(0x4501, 0x18);
	write_cmos_sensor(0x4502, 0x00);
	write_cmos_sensor(0x4643, 0x00);
	write_cmos_sensor(0x4640, 0x01);
	write_cmos_sensor(0x4641, 0x04);
	write_cmos_sensor(0x480e, 0x04);
	write_cmos_sensor(0x4813, 0x98);
	write_cmos_sensor(0x4815, 0x2b);
	write_cmos_sensor(0x486e, 0x36);
	write_cmos_sensor(0x486f, 0x84);
	write_cmos_sensor(0x4860, 0x00);
	write_cmos_sensor(0x4861, 0xa0);
	write_cmos_sensor(0x484b, 0x05);
	write_cmos_sensor(0x4850, 0x00);
	write_cmos_sensor(0x4851, 0xaa);
	write_cmos_sensor(0x4852, 0xff);
	write_cmos_sensor(0x4853, 0x8a);
	write_cmos_sensor(0x4854, 0x08);
	write_cmos_sensor(0x4855, 0x30);
	write_cmos_sensor(0x4800, 0x60);
	write_cmos_sensor(0x4837, 0x0a);
	write_cmos_sensor(0x484a, 0x3f);
	write_cmos_sensor(0x5000, 0xc9);
	write_cmos_sensor(0x5001, 0x43);
	write_cmos_sensor(0x5002, 0x00);
	write_cmos_sensor(0x5211, 0x03);
	write_cmos_sensor(0x5291, 0x03);
	write_cmos_sensor(0x520d, 0x0f);
	write_cmos_sensor(0x520e, 0xfd);
	write_cmos_sensor(0x520f, 0xa5);
	write_cmos_sensor(0x5210, 0xa5);
	write_cmos_sensor(0x528d, 0x0f);
	write_cmos_sensor(0x528e, 0xfd);
	write_cmos_sensor(0x528f, 0xa5);
	write_cmos_sensor(0x5290, 0xa5);
	write_cmos_sensor(0x5004, 0x40);
	write_cmos_sensor(0x5005, 0x00);
	write_cmos_sensor(0x5180, 0x00);
	write_cmos_sensor(0x5181, 0x10);
	write_cmos_sensor(0x5182, 0x0f);
	write_cmos_sensor(0x5183, 0xff);
	write_cmos_sensor(0x580b, 0x03);
	write_cmos_sensor(0x4d00, 0x03);
	write_cmos_sensor(0x4d01, 0xe9);
	write_cmos_sensor(0x4d02, 0xba);
	write_cmos_sensor(0x4d03, 0x66);
	write_cmos_sensor(0x4d04, 0x46);
	write_cmos_sensor(0x4d05, 0xa5);
	write_cmos_sensor(0x3603, 0x3c);
	write_cmos_sensor(0x3703, 0x26);
	write_cmos_sensor(0x3709, 0x49);
	write_cmos_sensor(0x3708, 0x2d);
	write_cmos_sensor(0x3719, 0x1c);
	write_cmos_sensor(0x371a, 0x06);
	write_cmos_sensor(0x4000, 0x79);
	write_cmos_sensor(0x380c, 0x02);
	write_cmos_sensor(0x380d, 0xd0);
	write_cmos_sensor(0x380e, 0x09);
	write_cmos_sensor(0x380f, 0xc4);
	write_cmos_sensor(0x3501, 0x08);  // 08
	write_cmos_sensor(0x3502, 0xc4);  // c4
	write_cmos_sensor(0x3511, 0x00);
	write_cmos_sensor(0x3512, 0x20);
	write_cmos_sensor(0x3808, 0x0a);
	write_cmos_sensor(0x3809, 0x00);
	write_cmos_sensor(0x380a, 0x07);
	write_cmos_sensor(0x380b, 0x80);
	write_cmos_sensor(0x0100, 0x01);
	first_config = 1;
	cam_pr_debug("EXIT\n");
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
				cam_pr_debug("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}
			cam_pr_debug("Read sensor id fail, id: 0x%x\n",
				*sensor_id);
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

	LOG_1;
	LOG_2;

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
	imgsensor.ihdr_en = 0;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}				/*    open  */



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
	cam_pr_debug("E\n");
	//streaming_control(KAL_FALSE);
	write_cmos_sensor(0x0100, 0x00);
	/*No Need to implement this function */

	return ERROR_NONE;
}				/*      close  */


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
	//custom1_setting();
	/* set_mirror_flip(sensor_config_data->SensorImageMirror); */
	return ERROR_NONE;
}				/*    preview   */

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
			cam_pr_debug("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
				imgsensor.current_fps,
				imgsensor_info.cap.max_framerate / 10);
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	return ERROR_NONE;
}				/* capture() */

static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
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

	preview_setting();

	return ERROR_NONE;
}				/*    normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	cam_pr_debug("E\n");

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
	hs_video_setting();
	/* set_mirror_flip(sensor_config_data->SensorImageMirror); */
	return ERROR_NONE;
}				/*    hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
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
	//preview_setting();
	slim_video_setting();
	/* set_mirror_flip(sensor_config_data->SensorImageMirror); */

	return ERROR_NONE;
}				/*    slim_video     */

static kal_uint32 Custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	cam_pr_debug("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk = imgsensor_info.custom1.pclk;
	imgsensor.line_length = imgsensor_info.custom1.linelength;
	imgsensor.frame_length = imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom1_setting();
	/* set_mirror_flip(sensor_config_data->SensorImageMirror); */

	return ERROR_NONE;
}				/*    Custome1  staggered HDR    */


static kal_uint32 get_resolution(
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT * sensor_resolution)
{
	cam_pr_debug("E\n");
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

	return ERROR_NONE;
}				/*    get_resolution    */

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
	MSDK_SENSOR_INFO_STRUCT *sensor_info,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	cam_pr_debug("scenario_id = %d\n", scenario_id);

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
	sensor_info->Custom1DelayFrame =
		imgsensor_info.custom1_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0;	/* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
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
	sensor_info->HDR_Support = HDR_SUPPORT_STAGGER;

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
		sensor_info->SensorGrabStartX =
			imgsensor_info.custom1.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.custom1.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;

		break;
	default:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	}

	return ERROR_NONE;
}				/*    get_info  */

static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
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
	case MSDK_SCENARIO_ID_CUSTOM1:
		Custom1(image_window, sensor_config_data);
		break;
	default:
		cam_pr_debug("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}

	return ERROR_NONE;
}				/* control() */

static kal_uint32 set_video_mode(UINT16 framerate)
{
	cam_pr_debug("framerate = %d\n ", framerate);

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
	cam_pr_debug("enable = %d, framerate = %d\n", enable, framerate);

	spin_lock(&imgsensor_drv_lock);
	if (enable)		/* enable auto flicker */
		imgsensor.autoflicker_en = KAL_TRUE;
	else			/* Cancel Auto flick */
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
		scenario_id, framerate);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frame_length =
			imgsensor_info.pre.pclk / framerate * 10 /
			imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length >
			imgsensor_info.pre.framelength) ? (frame_length -
			imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length =
			imgsensor_info.normal_video.pclk / framerate * 10 /
			imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length >
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
		frame_length =
			imgsensor_info.cap.pclk / framerate * 10 /
			imgsensor_info.cap.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length >
			imgsensor_info.cap.framelength) ?
			(frame_length -
			imgsensor_info.cap.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.cap.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length =
		    imgsensor_info.hs_video.pclk / framerate * 10 /
		    imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length >
			imgsensor_info.hs_video.framelength) ?
			(frame_length -
			imgsensor_info.hs_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.hs_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length =
			imgsensor_info.slim_video.pclk / framerate * 10 /
			imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length >
			imgsensor_info.slim_video.framelength) ?
			(frame_length -
			imgsensor_info.slim_video.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.slim_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		frame_length =
			imgsensor_info.custom1.pclk / framerate * 10 /
			imgsensor_info.custom1.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length >
			imgsensor_info.custom1.framelength) ?
			(frame_length -
			imgsensor_info.custom1.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.custom1.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	default:
		frame_length =
			imgsensor_info.pre.pclk / framerate * 10 /
			imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length >
			imgsensor_info.pre.framelength) ?
			(frame_length -
			imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
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
	default:
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	cam_pr_debug("enable: %d\n", enable);

	if (enable)
		write_cmos_sensor(0x5081, 0x80);
	else
		write_cmos_sensor(0x5081, 0x00);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}

static kal_uint32 streaming_control(kal_bool enable)
{
	cam_pr_debug("streaming_enable(0=Sw Standby,1=streaming): %d\n",
		enable);

	if (enable)
		write_cmos_sensor(0x0100, 0x01);
	else
		write_cmos_sensor(0x0100, 0x00);

	mdelay(20);
	return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
				  UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *)feature_para;

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	//struct SENSOR_VC_INFO_STRUCT *pvcinfo;
	struct SENSOR_VC_INFO2_STRUCT *pvcinfo2;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	cam_pr_debug("feature_id = %d\n", feature_id);
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
			*(feature_data + 2) = 1;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
		default:
			*(feature_data + 2) = 1;
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
		night_mode((BOOL) * feature_data);
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT32) *feature_data);
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
			(enum MSDK_SCENARIO_ID_ENUM) *feature_data,
			(MUINT32 *) (uintptr_t) (*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL) * feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		cam_pr_debug("current fps :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = (UINT16) *feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		cam_pr_debug("ihdr enable :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		//imgsensor.ihdr_en = (BOOL) * feature_data_32;
		imgsensor.hdr_mode = (UINT8)*feature_data_32;
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
		case MSDK_SCENARIO_ID_CUSTOM1:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[5],
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
			(UINT16) *feature_data,
			(UINT16) *(feature_data + 1),
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
		case MSDK_SCENARIO_ID_CUSTOM1:
			rate = imgsensor_info.custom1.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			rate = imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
		*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = rate;
	}
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
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
#if NO_USE_3HDR
			if (imgsensor.hdr_mode)
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre_3HDR.pclk;
			else
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.pclk;
#else
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= imgsensor_info.pre.pclk;
#endif
			break;
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
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
#if NO_USE_3HDR
			if (imgsensor.hdr_mode)
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= (imgsensor_info.pre_3HDR.framelength << 16)
				+ imgsensor_info.pre_3HDR.linelength;
			else
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= (imgsensor_info.pre.framelength << 16)
				+ imgsensor_info.pre.linelength;
#else
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= (imgsensor_info.pre.framelength << 16)
					+ imgsensor_info.pre.linelength;
#endif
				break;
			}
		break;
	case SENSOR_FEATURE_GET_SENSOR_HDR_CAPACITY:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t) (*(feature_data + 1)) = HDR_RAW_STAGGER_2EXP;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		default:
			*(MUINT32 *)(uintptr_t) (*(feature_data + 1)) = HDR_NONE;
			// other scenario do not support HDR
			break;
		}
		cam_pr_debug(
			"SENSOR_FEATURE_GET_SENSOR_HDR_CAPACITY scenarioId:%llu, HDR:%llu\n",
			*feature_data, *(feature_data+1));
		break;
	case SENSOR_FEATURE_GET_VC_INFO2:
		cam_pr_debug("SENSOR_FEATURE_GET_VC_INFO2 %d\n", (UINT16)(*feature_data));
		pvcinfo2 =
		(struct SENSOR_VC_INFO2_STRUCT *)(uintptr_t)(*(feature_data + 1));
		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CUSTOM1:
			memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[1],
				sizeof(struct SENSOR_VC_INFO2_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[0],
			    sizeof(struct SENSOR_VC_INFO2_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
		(*(feature_data + 1)) = 1;
		(*(feature_data + 2)) = imgsensor_info.margin;
		break;
	case SENSOR_FEATURE_GET_STAGGER_MAX_EXP_TIME:
		if (*feature_data == MSDK_SCENARIO_ID_CUSTOM1) {
			switch (*(feature_data + 1)) {
			case VC_STAGGER_NE:
				*(feature_data + 2) = 32757;
				break;
			case VC_STAGGER_ME:
				*(feature_data + 2) = 32757;
				break;
			case VC_STAGGER_SE:
				*(feature_data + 2) = 32757;
				break;
			default:
				*(feature_data + 2) = 32757;
				break;
			}
		} else {
			*(feature_data + 2) = 0;
		}
		break;
	case SENSOR_FEATURE_GET_STAGGER_TARGET_SCENARIO:
		if (*feature_data == MSDK_SCENARIO_ID_CAMERA_PREVIEW) {
			switch (*(feature_data + 1)) {
			case HDR_RAW_STAGGER_2EXP:
				*(feature_data + 2) = MSDK_SCENARIO_ID_CUSTOM1;
				break;
			case HDR_RAW_STAGGER_3EXP:
			default:
				break;
			}
		}
		break;
	case SENSOR_FEATURE_SET_HDR_SHUTTER: // for 2EXP
		cam_pr_debug(
			"SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
			(UINT16)*(feature_data), (UINT16) *(feature_data + 1));
		// implement write shutter for NE/ME/SE
		hdr_write_shutter((UINT16)*feature_data, (UINT16)*(feature_data+1));
		break;
	case SENSOR_FEATURE_SET_DUAL_GAIN:
		cam_pr_debug(
			"SENSOR_FEATURE_SET_DUAL_GAIN LG=%d, SG=%d\n",
			(UINT16)*(feature_data), (UINT16) *(feature_data + 1));
		hdr_write_gain((UINT16)*feature_data, (UINT16)*(feature_data+1));
		break;
	case SENSOR_FEATURE_SET_HDR_TRI_SHUTTER: // for 3EXP
		cam_pr_debug(
			"SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d, ME=%d\n",
			(UINT16) *feature_data,
			(UINT16) *(feature_data + 1),
			(UINT16) *(feature_data + 2));
		// implement write shutter for NE/ME/SE
		// function: hdr_write_tri_shutter
		break;
	default:
		break;
	}
	return ERROR_NONE;
}		/*    feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 OV05A20_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}				/*    OV5648MIPISensorInit    */
