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

#include "ov13855mipiraw_Sensor.h"

/***********************Modify Following Strings for Debug********************/
#define PFX "OV13855_camera_sensor"
#define LOG_1 cam_pr_debug("OV13855,MIPI 4LANE\n")
#define LOG_2 cam_pr_debug(\
	"preview 2096*1552@30fps;video 4192*3104@30fps; capture 13M@30fps\n")
/***************************   Modify end    *********************************/

#define cam_pr_debug(fmt, args...) \
	pr_debug(PFX "[%s] " fmt, __func__, ##args)

static DEFINE_SPINLOCK(imgsensor_drv_lock);

int m_bpc_select;
/* sensor otp
 * attention
 * Here comment, just wait for otp implementation
 * extern void otp_cali(unsigned char writeid);
 */

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = OV13855_SENSOR_ID,

	.checksum_value = 0x78779e1c,
	.pre = {
		.pclk = 108000000,
		.linelength = 1122,
		.framelength = 3214,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2112,
		.grabwindow_height = 1568,	/*1568 */
		.mipi_data_lp2hs_settle_dc = 40,	/*unit , ns */
		.mipi_pixel_rate = 216000000,
		.max_framerate = 300,
		},
	.cap = {
		.pclk = 108000000,
		.linelength = 1122,
		.framelength = 3214,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4224,	/* 4192, */
		.grabwindow_height = 3136,	/* 3104, */
		.mipi_data_lp2hs_settle_dc = 40,	/* unit , ns */
		.mipi_pixel_rate = 432000000,
		.max_framerate = 300,
		},
	.cap1 = {
		.pclk = 108000000,
		.linelength = 2244,
		.framelength = 3214,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4224,
		.grabwindow_height = 3136,
		.mipi_data_lp2hs_settle_dc = 40,	/* unit , ns */
		.mipi_pixel_rate = 432000000,
		.max_framerate = 150,
		},
	.normal_video = {
		.pclk = 108000000,
		.linelength = 1122,
		.framelength = 3214,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4224,
		.grabwindow_height = 3136,
		.mipi_data_lp2hs_settle_dc = 40,	/* unit , ns */
		.mipi_pixel_rate = 432000000,
		.max_framerate = 300,
		},
	.hs_video = {
		.pclk = 108000000,
		.linelength = 1122,	/* 2968, */
		.framelength = 804,	/* 674, */
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1024,	/* 640 */
		.grabwindow_height = 768,	/* 480 */
		.mipi_data_lp2hs_settle_dc = 40,	/* unit , ns */
		.mipi_pixel_rate = 432000000,
		.max_framerate = 1200,
		},
	.slim_video = {
		.pclk = 108000000,
		.linelength = 1122,	/* 9600,//2400, */
		.framelength = 3214,	/* 834,//3328, */
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2112,
		.grabwindow_height = 1568,
		.mipi_data_lp2hs_settle_dc = 40,	/* unit , ns */
		.mipi_pixel_rate = 432000000,
		.max_framerate = 300,
		},

	.margin = 16,		/* sensor framelength & shutter margin */
	.min_shutter = 0x4,	/* min shutter */
	.max_frame_length = 0x7ff0,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,
	.frame_time_delay_frame = 1,
	.ihdr_support = 0,	/* 1, support; 0,not support */
	.ihdr_le_firstline = 0,	/* 1,le first ; 0, se first */
	.sensor_mode_num = 5,	/* support sensor mode num */

	.cap_delay_frame = 3,	/* enter capture delay frame num */
	.pre_delay_frame = 2,	/* enter preview delay frame num */
	.video_delay_frame = 2,	/* enter video delay frame num */
	.hs_video_delay_frame = 2,
	.slim_video_delay_frame = 2,	/* enter slim video delay frame num */

	.isp_driving_current = ISP_DRIVING_4MA,	/* mclk driving current */
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = 1,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,	/* mipi lane num */
	.i2c_addr_table = {0x20, 0xff}, /* 0x6c no need for 8168 evb */
};

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,
	.sensor_mode = IMGSENSOR_MODE_INIT,
	/* IMGSENSOR_MODE enum value,record current sensor mode,
	 * such as: INIT, Preview, Capture, Video,High Speed Video, Slim Video
	 */
	.shutter = 0x3D0,	/* current shutter */
	.gain = 0x100,		/* current gain */
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_en = 0,
	.i2c_write_id = 0x20,
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
{4256, 3152, 0, 8, 4256, 3144, 2128, 1572, 8, 2, 2112, 1568, 0, 0, 2112, 1568},
{4256, 3152, 0, 8, 4232, 3144, 4232, 3144, 16, 8, 4224, 3136, 0, 0, 4224, 3136},
{4256, 3152, 0, 8, 4232, 3144, 4232, 3144, 16, 8, 4224, 3136, 0, 0, 4224, 3136},
{4256, 3152, 64, 64, 4192, 3088, 1048, 772, 4, 4, 1024, 768, 0, 0, 1024, 768},
{4256, 3152, 0, 8, 4256, 3144, 2128, 1572, 8, 2, 2112, 1568, 0, 0, 2112, 1568}
};

/* add from OV13855_v1_mirror_on_flip_off_mtk2.0.1.ini */
static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
	.i4OffsetX = 72,
	.i4OffsetY = 64,
	.i4PitchX = 32,
	.i4PitchY = 32,
	.i4PairNum = 8,
	.i4SubBlkW = 16,
	.i4SubBlkH = 8,		/* need check xb.pang */
	.i4PosL = {
	    {86, 70}, {102, 70}, {78, 74}, {94, 74},
	    {86, 86}, {102, 86}, {78, 90}, {94, 90} },
	.i4PosR = {
	    {86, 66}, {102, 66}, {78, 78}, {94, 78},
	    {86, 82}, {102, 82}, {78, 94}, {94, 94} },
};

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
	char pu_send_cmd[3] = { (char)(addr >> 8),
		(char)(addr & 0xFF), (char)(para & 0xFF) };

	iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
	/* check */
	cam_pr_debug("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);

	write_cmos_sensor(0x380c, imgsensor.line_length >> 8);
	write_cmos_sensor(0x380d, imgsensor.line_length & 0xFF);
	write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
	write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
}

static kal_uint32 return_sensor_id(void)
{
	return ((read_cmos_sensor(0x300B) << 8) | read_cmos_sensor(0x300C));
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	cam_pr_debug("framerate = %d, min_framelength_en=%d\n",
		framerate, min_framelength_en);
	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	/* cam_pr_debug("frame_length =%d\n", frame_length); */
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
	cam_pr_debug("framerate = %d, min_framelength_en=%d\n",
		framerate, min_framelength_en);
	set_dummy();
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
	kal_uint16 realtime_fps = 0;
	/* kal_uint32 frame_length = 0; */
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
	cam_pr_debug("Enter %s! shutter =%d\n", __func__, shutter);

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

	/* frame_length and shutter should be an even number. */
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
			/* Extend frame length */
			write_cmos_sensor(0x380e,
				(imgsensor.frame_length >> 8) & 0x7f);
			write_cmos_sensor(0x380f,
				imgsensor.frame_length & 0xFF);
		}
	} else {
		/* Extend frame length */
		/* write_cmos_sensor(0x380e, imgsensor.frame_length >> 8); */
		/* write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF); */
		write_cmos_sensor(0x380e, (imgsensor.frame_length >> 8) & 0x7f);
		write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
	}

	/* Update Shutter */
	/* write_cmos_sensor(0x3502, (shutter << 4) & 0xFF); */
	/* write_cmos_sensor(0x3501, (shutter >> 4) & 0xFF); */
	/* write_cmos_sensor(0x3500, (shutter >> 12) & 0x0F); */

	write_cmos_sensor(0x3500, (shutter >> 12) & 0x0F);
	write_cmos_sensor(0x3501, (shutter >> 4) & 0xFF);
	write_cmos_sensor(0x3502, (shutter << 4) & 0xF0);
	cam_pr_debug("Exit! shutter =%d, framelength =%d\n",
		shutter, imgsensor.frame_length);

}				/*    set_shutter */


static void set_shutter_frame_length(kal_uint16 shutter,
	kal_uint16 frame_length)
{

	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	kal_int32 dummy_line = 0;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
	cam_pr_debug("Enter %s! shutter =%d\n", __func__, shutter);

	spin_lock(&imgsensor_drv_lock);
	/* Change frame time */
	if (frame_length > 1)
		dummy_line = frame_length - imgsensor.frame_length;
	imgsensor.frame_length = imgsensor.frame_length + dummy_line;

	/*  */
	if (shutter > imgsensor.frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;

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
		imgsensor_info.margin) : shutter;

	/* frame_length and shutter should be an even number. */
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
			/* Extend frame length */
			write_cmos_sensor(0x380e,
				(imgsensor.frame_length >> 8) & 0x7f);
			write_cmos_sensor(0x380f,
				imgsensor.frame_length & 0xFF);
		}
	} else {
		/* Extend frame length */
		/* write_cmos_sensor(0x380e, imgsensor.frame_length >> 8); */
		/* write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF); */
		write_cmos_sensor(0x380e, (imgsensor.frame_length >> 8) & 0x7f);
		write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
	}

	/* Update Shutter */
	/* write_cmos_sensor(0x3502, (shutter << 4) & 0xFF); */
	/* write_cmos_sensor(0x3501, (shutter >> 4) & 0xFF); */
	/* write_cmos_sensor(0x3500, (shutter >> 12) & 0x0F); */

	write_cmos_sensor(0x3500, (shutter >> 12) & 0x0F);
	write_cmos_sensor(0x3501, (shutter >> 4) & 0xFF);
	write_cmos_sensor(0x3502, (shutter << 4) & 0xF0);
	cam_pr_debug("Exit! shutter =%d, framelength =%d\n",
		shutter, imgsensor.frame_length);

}

static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 iReg = 0x0000;

	iReg = gain * 2;
	if (iReg > 0x7c0)
		iReg = 0x7c0;	/* max sensor gain = 15.5x */

	return iReg;		/* ov13855. sensorGlobalGain */
}

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
	kal_uint16 reg_gain;

	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	cam_pr_debug("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

	write_cmos_sensor(0x3508, reg_gain >> 8);
	write_cmos_sensor(0x3509, reg_gain & 0xFF);

	return gain;
}				/*    set_gain  */

static void ihdr_write_shutter_gain(
	kal_uint16 le,
	kal_uint16 se,
	kal_uint16 gain)
{
	cam_pr_debug("le:0x%x, se:0x%x, gain:0x%x\n", le, se, gain);
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


		/* Extend frame length first */
		write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
		write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);

		write_cmos_sensor(0x3502, (le << 4) & 0xFF);
		write_cmos_sensor(0x3501, (le >> 4) & 0xFF);
		write_cmos_sensor(0x3500, (le >> 12) & 0x0F);

		set_gain(gain);
	}
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
/*No Need to implement this function*/
}				/*    night_mode    */

static void sensor_init(void)
{
	cam_pr_debug("E\n");
	write_cmos_sensor(0x0100, 0x00);
	write_cmos_sensor(0x0103, 0x01);
	mdelay(10);
	write_cmos_sensor(0x0300, 0x02);
	write_cmos_sensor(0x0301, 0x00);
	write_cmos_sensor(0x0302, 0x5a);
	write_cmos_sensor(0x0303, 0x00);
	write_cmos_sensor(0x0304, 0x00);
	write_cmos_sensor(0x0305, 0x01);
	write_cmos_sensor(0x3022, 0x01);
	/* Update MIPI output timing R3012 and R3013 */
	/* when stream off and then on within one frame */
	write_cmos_sensor(0x3012, 0x40);
	write_cmos_sensor(0x3013, 0x72);
	write_cmos_sensor(0x3016, 0x72);
	write_cmos_sensor(0x301b, 0xF0);
	write_cmos_sensor(0x301f, 0xd0);
	write_cmos_sensor(0x3106, 0x15);
	write_cmos_sensor(0x3107, 0x23);
	write_cmos_sensor(0x3500, 0x00);
	write_cmos_sensor(0x3501, 0x80);
	write_cmos_sensor(0x3502, 0x00);
	write_cmos_sensor(0x3508, 0x02);
	write_cmos_sensor(0x3509, 0x00);
	write_cmos_sensor(0x350a, 0x00);
	write_cmos_sensor(0x350e, 0x00);
	write_cmos_sensor(0x3510, 0x00);
	write_cmos_sensor(0x3511, 0x02);
	write_cmos_sensor(0x3512, 0x00);
	write_cmos_sensor(0x3600, 0x2b);
	write_cmos_sensor(0x3601, 0x52);
	write_cmos_sensor(0x3602, 0x60);
	write_cmos_sensor(0x3612, 0x05);
	write_cmos_sensor(0x3613, 0xa4);
	write_cmos_sensor(0x3620, 0x80);
	write_cmos_sensor(0x3621, 0x08);
	write_cmos_sensor(0x3622, 0x30);
	write_cmos_sensor(0x3624, 0x1c);
	write_cmos_sensor(0x3661, 0x80);
	write_cmos_sensor(0x3662, 0x12);
	write_cmos_sensor(0x3664, 0x73);
	write_cmos_sensor(0x3665, 0xa7);
	write_cmos_sensor(0x366e, 0xff);
	write_cmos_sensor(0x366f, 0xf4);
	write_cmos_sensor(0x3674, 0x00);
	write_cmos_sensor(0x3679, 0x0c);
	write_cmos_sensor(0x367f, 0x01);
	write_cmos_sensor(0x3680, 0x0c);
	write_cmos_sensor(0x3681, 0x60);
	write_cmos_sensor(0x3682, 0x17);
	write_cmos_sensor(0x3683, 0xa9);
	write_cmos_sensor(0x3684, 0x9a);
	write_cmos_sensor(0x3709, 0x68);
	write_cmos_sensor(0x3714, 0x24);
	write_cmos_sensor(0x371a, 0x3e);
	write_cmos_sensor(0x3737, 0x04);
	write_cmos_sensor(0x3738, 0xcc);
	write_cmos_sensor(0x3739, 0x12);
	write_cmos_sensor(0x373d, 0x26);
	write_cmos_sensor(0x3764, 0x20);
	write_cmos_sensor(0x3765, 0x20);
	write_cmos_sensor(0x37a1, 0x36);
	write_cmos_sensor(0x37a8, 0x3b);
	write_cmos_sensor(0x37ab, 0x31);
	write_cmos_sensor(0x37c2, 0x04);
	write_cmos_sensor(0x37c3, 0xf1);
	write_cmos_sensor(0x37c5, 0x00);
	write_cmos_sensor(0x37d8, 0x03);
	write_cmos_sensor(0x37d9, 0x0c);
	write_cmos_sensor(0x37da, 0xc2);
	write_cmos_sensor(0x37dc, 0x02);
	write_cmos_sensor(0x37e0, 0x00);
	write_cmos_sensor(0x37e1, 0x0a);
	write_cmos_sensor(0x37e2, 0x14);
	write_cmos_sensor(0x37e3, 0x04);
	write_cmos_sensor(0x37e4, 0x26);
	write_cmos_sensor(0x37e5, 0x03);
	write_cmos_sensor(0x37e6, 0x04);
	write_cmos_sensor(0x3800, 0x00);
	write_cmos_sensor(0x3801, 0x00);
	write_cmos_sensor(0x3802, 0x00);
	write_cmos_sensor(0x3803, 0x08);
	write_cmos_sensor(0x3804, 0x10);
	write_cmos_sensor(0x3805, 0x9f);
	write_cmos_sensor(0x3806, 0x0c);
	write_cmos_sensor(0x3807, 0x57);
	write_cmos_sensor(0x3808, 0x10);
	write_cmos_sensor(0x3809, 0x80);
	write_cmos_sensor(0x380a, 0x0c);
	write_cmos_sensor(0x380b, 0x40);
	write_cmos_sensor(0x380c, 0x04);
	write_cmos_sensor(0x380d, 0x62);
	write_cmos_sensor(0x380e, 0x0c);
	write_cmos_sensor(0x380f, 0x8e);
	write_cmos_sensor(0x3811, 0x10);
	write_cmos_sensor(0x3813, 0x08);
	write_cmos_sensor(0x3814, 0x01);
	write_cmos_sensor(0x3815, 0x01);
	write_cmos_sensor(0x3816, 0x01);
	write_cmos_sensor(0x3817, 0x01);
	write_cmos_sensor(0x3820, 0xa8);
	write_cmos_sensor(0x3821, 0x00);
	write_cmos_sensor(0x3822, 0xc2);
	write_cmos_sensor(0x3823, 0x18);
	write_cmos_sensor(0x3826, 0x11);
	write_cmos_sensor(0x3827, 0x1c);
	write_cmos_sensor(0x3829, 0x03);
	write_cmos_sensor(0x3832, 0x00);
	write_cmos_sensor(0x3c80, 0x00);
	write_cmos_sensor(0x3c87, 0x01);
	write_cmos_sensor(0x3c8c, 0x19);
	write_cmos_sensor(0x3c8d, 0x1c);
	write_cmos_sensor(0x3c90, 0x00);
	write_cmos_sensor(0x3c91, 0x00);
	write_cmos_sensor(0x3c92, 0x00);
	write_cmos_sensor(0x3c93, 0x00);
	write_cmos_sensor(0x3c94, 0x41);
	write_cmos_sensor(0x3c95, 0x54);
	write_cmos_sensor(0x3c96, 0x34);
	write_cmos_sensor(0x3c97, 0x04);
	write_cmos_sensor(0x3c98, 0x00);
	write_cmos_sensor(0x3d8c, 0x73);
	write_cmos_sensor(0x3d8d, 0xc0);
	write_cmos_sensor(0x3f00, 0x0b);
	write_cmos_sensor(0x3f03, 0x00);
	write_cmos_sensor(0x4001, 0xe0);
	write_cmos_sensor(0x4008, 0x00);
	write_cmos_sensor(0x4009, 0x0f);
	write_cmos_sensor(0x4011, 0xf0);
	write_cmos_sensor(0x4050, 0x04);
	write_cmos_sensor(0x4051, 0x0b);
	write_cmos_sensor(0x4052, 0x00);
	write_cmos_sensor(0x4053, 0x80);
	write_cmos_sensor(0x4054, 0x00);
	write_cmos_sensor(0x4055, 0x80);
	write_cmos_sensor(0x4056, 0x00);
	write_cmos_sensor(0x4057, 0x80);
	write_cmos_sensor(0x4058, 0x00);
	write_cmos_sensor(0x4059, 0x80);
	write_cmos_sensor(0x405e, 0x20);
	write_cmos_sensor(0x4500, 0x07);
	write_cmos_sensor(0x4503, 0x00);
	write_cmos_sensor(0x450a, 0x04);
	write_cmos_sensor(0x4809, 0x04);
	write_cmos_sensor(0x480c, 0x12);
	write_cmos_sensor(0x4833, 0x10);
	write_cmos_sensor(0x4837, 0x0e);
	write_cmos_sensor(0x4902, 0x01);
	write_cmos_sensor(0x4d00, 0x03);
	write_cmos_sensor(0x4d01, 0xc9);
	write_cmos_sensor(0x4d02, 0xbc);
	write_cmos_sensor(0x4d03, 0xd7);
	write_cmos_sensor(0x4d04, 0xf0);
	write_cmos_sensor(0x4d05, 0xa2);
	write_cmos_sensor(0x5000, 0xff);
	write_cmos_sensor(0x5001, 0x07);
	write_cmos_sensor(0x5040, 0x39);
	write_cmos_sensor(0x5041, 0x10);
	write_cmos_sensor(0x5042, 0x10);
	write_cmos_sensor(0x5043, 0x84);
	write_cmos_sensor(0x5044, 0x62);
	write_cmos_sensor(0x5180, 0x00);
	write_cmos_sensor(0x5181, 0x10);
	write_cmos_sensor(0x5182, 0x02);
	write_cmos_sensor(0x5183, 0x0f);
	write_cmos_sensor(0x5200, 0x1b);
	write_cmos_sensor(0x520b, 0x07);
	write_cmos_sensor(0x520c, 0x0f);
	write_cmos_sensor(0x5300, 0x04);
	write_cmos_sensor(0x5301, 0x0C);
	write_cmos_sensor(0x5302, 0x0C);
	write_cmos_sensor(0x5303, 0x0f);
	write_cmos_sensor(0x5304, 0x00);
	write_cmos_sensor(0x5305, 0x70);
	write_cmos_sensor(0x5306, 0x00);
	write_cmos_sensor(0x5307, 0x80);
	write_cmos_sensor(0x5308, 0x00);
	write_cmos_sensor(0x5309, 0xa5);
	write_cmos_sensor(0x530a, 0x00);
	write_cmos_sensor(0x530b, 0xd3);
	write_cmos_sensor(0x530c, 0x00);
	write_cmos_sensor(0x530d, 0xf0);
	write_cmos_sensor(0x530e, 0x01);
	write_cmos_sensor(0x530f, 0x10);
	write_cmos_sensor(0x5310, 0x01);
	write_cmos_sensor(0x5311, 0x20);
	write_cmos_sensor(0x5312, 0x01);
	write_cmos_sensor(0x5313, 0x20);
	write_cmos_sensor(0x5314, 0x01);
	write_cmos_sensor(0x5315, 0x20);
	write_cmos_sensor(0x5316, 0x08);
	write_cmos_sensor(0x5317, 0x08);
	write_cmos_sensor(0x5318, 0x10);
	write_cmos_sensor(0x5319, 0x88);
	write_cmos_sensor(0x531a, 0x88);
	write_cmos_sensor(0x531b, 0xa9);
	write_cmos_sensor(0x531c, 0xaa);
	write_cmos_sensor(0x531d, 0x0a);
	write_cmos_sensor(0x5405, 0x02);
	write_cmos_sensor(0x5406, 0x67);
	write_cmos_sensor(0x5407, 0x01);
	write_cmos_sensor(0x5408, 0x4a);
}				/*    sensor_init  */

static void preview_setting(void)
{
	cam_pr_debug("E\n");
	write_cmos_sensor(0x0100, 0x00);
	write_cmos_sensor(0x0303, 0x01);
	write_cmos_sensor(0x3501, 0x40);
	write_cmos_sensor(0x3662, 0x10);
	write_cmos_sensor(0x3714, 0x28);
	write_cmos_sensor(0x3737, 0x08);
	write_cmos_sensor(0x3739, 0x20);
	write_cmos_sensor(0x37c2, 0x14);
	write_cmos_sensor(0x37d9, 0x0c);
	write_cmos_sensor(0x37e3, 0x08);
	write_cmos_sensor(0x37e4, 0x34);
	write_cmos_sensor(0x37e6, 0x08);
	write_cmos_sensor(0x3801, 0x00);
	write_cmos_sensor(0x3802, 0x00);
	write_cmos_sensor(0x3803, 0x08);
	write_cmos_sensor(0x3805, 0x9f);
	write_cmos_sensor(0x3806, 0x0c);
	write_cmos_sensor(0x3807, 0x4f);
	write_cmos_sensor(0x3808, 0x08);
	write_cmos_sensor(0x3809, 0x40);
	write_cmos_sensor(0x380a, 0x06);
	write_cmos_sensor(0x380b, 0x20);
	write_cmos_sensor(0x380c, 0x04);
	write_cmos_sensor(0x380d, 0x62);
	write_cmos_sensor(0x380e, 0x0c);
	write_cmos_sensor(0x380f, 0x8e);
	write_cmos_sensor(0x3811, 0x08);
	write_cmos_sensor(0x3813, 0x02);
	write_cmos_sensor(0x3814, 0x03);
	write_cmos_sensor(0x3816, 0x03);
	write_cmos_sensor(0x3820, 0xab);
	write_cmos_sensor(0x3826, 0x04);
	write_cmos_sensor(0x3827, 0x90);
	write_cmos_sensor(0x3829, 0x07);
	write_cmos_sensor(0x4009, 0x0d);
	write_cmos_sensor(0x4050, 0x04);
	write_cmos_sensor(0x4051, 0x0b);
	write_cmos_sensor(0x4837, 0x1c);
	write_cmos_sensor(0x4902, 0x01);

	/*update sensor PLL1 clock*/
	/*MIP_PCLK:70.5M MIPI_PHY_CLK:564Mbps SCLK:56.4M*/
	write_cmos_sensor(0x0302, 0x5e);
	write_cmos_sensor(0x4837, 0x1c);
	/* write_cmos_sensor(0x0100, 0x01); */
}				/*    preview_setting  */

int capture_first_flag;
int pre_currefps;
static void capture_setting(kal_uint16 currefps)
{
	cam_pr_debug("E! currefps:%d\n", currefps);

	if (pre_currefps != currefps) {
		capture_first_flag = 0;
		pre_currefps = currefps;
	} else
		capture_first_flag = 1;
	if (capture_first_flag == 0) {
		write_cmos_sensor(0x0100, 0x00);

		if (currefps == 300) {	/* 30fps for Normal capture & ZSD */
			/* full size @ 30fps */
			write_cmos_sensor(0x0303, 0x00);
			write_cmos_sensor(0x3501, 0x80);
			write_cmos_sensor(0x3662, 0x12);
			write_cmos_sensor(0x3714, 0x24);
			write_cmos_sensor(0x3737, 0x04);
			write_cmos_sensor(0x3739, 0x12);
			write_cmos_sensor(0x37c2, 0x04);
			write_cmos_sensor(0x37d9, 0x0c);
			write_cmos_sensor(0x37e3, 0x04);
			write_cmos_sensor(0x37e4, 0x26);
			write_cmos_sensor(0x37e6, 0x04);
			write_cmos_sensor(0x3801, 0x00);
			write_cmos_sensor(0x3802, 0x00);
			write_cmos_sensor(0x3803, 0x08);
			write_cmos_sensor(0x3805, 0x9f);
			write_cmos_sensor(0x3806, 0x0c);
			write_cmos_sensor(0x3807, 0x57);
			write_cmos_sensor(0x3808, 0x10);
			write_cmos_sensor(0x3809, 0x80);
			write_cmos_sensor(0x380a, 0x0c);
			write_cmos_sensor(0x380b, 0x40);
			write_cmos_sensor(0x380c, 0x04);
			write_cmos_sensor(0x380d, 0x62);
			write_cmos_sensor(0x380e, 0x0c);
			write_cmos_sensor(0x380f, 0x8e);
			write_cmos_sensor(0x3811, 0x10);
			write_cmos_sensor(0x3813, 0x08);
			write_cmos_sensor(0x3814, 0x01);
			write_cmos_sensor(0x3816, 0x01);
			write_cmos_sensor(0x3820, 0xa8);
			write_cmos_sensor(0x3826, 0x11);
			write_cmos_sensor(0x3827, 0x1c);
			write_cmos_sensor(0x3829, 0x03);
			write_cmos_sensor(0x4009, 0x0f);
			write_cmos_sensor(0x4050, 0x04);
			write_cmos_sensor(0x4051, 0x0b);
			write_cmos_sensor(0x4837, 0x0e);
			write_cmos_sensor(0x4902, 0x01);

			/*update sensor PLL1 clock*/
			/*MIP_PCLK:141M MIPI_PHY_CLK:1128Mbps SCLK:112.8M*/
			write_cmos_sensor(0x0302, 0x5e);
			write_cmos_sensor(0x4837, 0x0e);

		} else {	/* full size 15fps for PIP */


			write_cmos_sensor(0x0303, 0x00);
			write_cmos_sensor(0x3501, 0x80);
			write_cmos_sensor(0x3662, 0x12);
			write_cmos_sensor(0x3714, 0x24);
			write_cmos_sensor(0x3737, 0x04);
			write_cmos_sensor(0x3739, 0x12);
			write_cmos_sensor(0x37c2, 0x04);
			write_cmos_sensor(0x37d9, 0x0c);
			write_cmos_sensor(0x37e3, 0x04);
			write_cmos_sensor(0x37e4, 0x26);
			write_cmos_sensor(0x37e6, 0x04);
			write_cmos_sensor(0x3801, 0x00);
			write_cmos_sensor(0x3802, 0x00);
			write_cmos_sensor(0x3803, 0x08);
			write_cmos_sensor(0x3805, 0x9f);
			write_cmos_sensor(0x3806, 0x0c);
			write_cmos_sensor(0x3807, 0x57);
			write_cmos_sensor(0x3808, 0x10);
			write_cmos_sensor(0x3809, 0x80);
			write_cmos_sensor(0x380a, 0x0c);
			write_cmos_sensor(0x380b, 0x40);
			write_cmos_sensor(0x380c, 0x08);
			write_cmos_sensor(0x380d, 0xc4);
			write_cmos_sensor(0x380e, 0x0c);
			write_cmos_sensor(0x380f, 0x8e);
			write_cmos_sensor(0x3811, 0x10);
			write_cmos_sensor(0x3813, 0x08);
			write_cmos_sensor(0x3814, 0x01);
			write_cmos_sensor(0x3816, 0x01);
			write_cmos_sensor(0x3820, 0xa8);
			write_cmos_sensor(0x3826, 0x11);
			write_cmos_sensor(0x3827, 0x1c);
			write_cmos_sensor(0x3829, 0x03);
			write_cmos_sensor(0x4009, 0x0f);
			write_cmos_sensor(0x4050, 0x04);
			write_cmos_sensor(0x4051, 0x0b);
			write_cmos_sensor(0x4837, 0x1c);
			write_cmos_sensor(0x4902, 0x01);

			/*update sensor PLL1 clock*/
			/*MIP_PCLK:141M MIPI_PHY_CLK:1128Mbps SCLK:112.8M*/
			write_cmos_sensor(0x0302, 0x5e);
			write_cmos_sensor(0x4837, 0x0e);

			if (imgsensor.ihdr_en)
				cam_pr_debug("imgsensor ihdr enable\n");
				/* no implementation */
			else
				cam_pr_debug("imgsensor ihdr disable\n");
				/* no implementation */
		}

		mdelay(20);
		capture_first_flag = 1;
	}

	mdelay(20);

}    /*    capture_setting  */

static void normal_video_setting(kal_uint16 currefps)
{
	cam_pr_debug("E! currefps:%d\n", currefps);

	write_cmos_sensor(0x0100, 0x00);
	mdelay(1);
	write_cmos_sensor(0x0303, 0x00);
	write_cmos_sensor(0x3501, 0x80);
	write_cmos_sensor(0x3662, 0x12);
	write_cmos_sensor(0x3714, 0x24);
	write_cmos_sensor(0x3737, 0x04);
	write_cmos_sensor(0x3739, 0x12);
	write_cmos_sensor(0x37c2, 0x04);
	write_cmos_sensor(0x37d9, 0x0c);
	write_cmos_sensor(0x37e3, 0x04);
	write_cmos_sensor(0x37e4, 0x26);
	write_cmos_sensor(0x37e6, 0x04);
	write_cmos_sensor(0x3801, 0x00);
	write_cmos_sensor(0x3802, 0x00);
	write_cmos_sensor(0x3803, 0x08);
	write_cmos_sensor(0x3805, 0x9f);
	write_cmos_sensor(0x3806, 0x0c);
	write_cmos_sensor(0x3807, 0x57);
	write_cmos_sensor(0x3808, 0x10);
	write_cmos_sensor(0x3809, 0x80);
	write_cmos_sensor(0x380a, 0x0c);
	write_cmos_sensor(0x380b, 0x40);
	write_cmos_sensor(0x380c, 0x04);
	write_cmos_sensor(0x380d, 0x62);
	write_cmos_sensor(0x380e, 0x0c);
	write_cmos_sensor(0x380f, 0x8e);
	write_cmos_sensor(0x3811, 0x10);
	write_cmos_sensor(0x3813, 0x08);
	write_cmos_sensor(0x3814, 0x01);
	write_cmos_sensor(0x3816, 0x01);
	write_cmos_sensor(0x3820, 0xa8);
	write_cmos_sensor(0x3826, 0x11);
	write_cmos_sensor(0x3827, 0x1c);
	write_cmos_sensor(0x3829, 0x03);
	write_cmos_sensor(0x4009, 0x0f);
	write_cmos_sensor(0x4050, 0x04);
	write_cmos_sensor(0x4051, 0x0b);
	write_cmos_sensor(0x4837, 0x0e);
	write_cmos_sensor(0x4902, 0x01);
	/* write_cmos_sensor(0x0100, 0x01); */

	/*update sensor PLL1 clock*/
	/*MIP_PCLK:141M MIPI_PHY_CLK:1128Mbps SCLK:112.8M*/
	write_cmos_sensor(0x0302, 0x5e);
	write_cmos_sensor(0x4837, 0x0e);

	if (imgsensor.ihdr_en)
		cam_pr_debug("imgsensor ihdr enable\n");
		/* no implementation */
	else
		cam_pr_debug("imgsensor ihdr disable\n");
		/* no implementation */

	mdelay(20);
}    /*    normal_video_setting  */

static void hs_video_setting(void)
{
	cam_pr_debug("E\n");
	write_cmos_sensor(0x0100, 0x00);
	write_cmos_sensor(0x0303, 0x03);
	write_cmos_sensor(0x3501, 0x20);
	write_cmos_sensor(0x3662, 0x08);
	write_cmos_sensor(0x3714, 0x30);
	write_cmos_sensor(0x3737, 0x08);
	write_cmos_sensor(0x3739, 0x20);
	write_cmos_sensor(0x37c2, 0x2c);
	write_cmos_sensor(0x37d9, 0x06);
	write_cmos_sensor(0x37e3, 0x08);
	write_cmos_sensor(0x37e4, 0x34);
	write_cmos_sensor(0x37e6, 0x08);
	write_cmos_sensor(0x3801, 0x40);
	write_cmos_sensor(0x3802, 0x00);
	write_cmos_sensor(0x3803, 0x40);
	write_cmos_sensor(0x3805, 0x5f);
	write_cmos_sensor(0x3806, 0x0c);
	write_cmos_sensor(0x3807, 0x5f);
	write_cmos_sensor(0x3808, 0x04);
	write_cmos_sensor(0x3809, 0x00);
	write_cmos_sensor(0x380a, 0x03);
	write_cmos_sensor(0x380b, 0x00);
	write_cmos_sensor(0x380c, 0x04);
	write_cmos_sensor(0x380d, 0x62);
	write_cmos_sensor(0x380e, 0x03);
	write_cmos_sensor(0x380f, 0x24);
	write_cmos_sensor(0x3811, 0x04);
	write_cmos_sensor(0x3813, 0x04);
	write_cmos_sensor(0x3814, 0x07);
	write_cmos_sensor(0x3816, 0x07);
	write_cmos_sensor(0x3820, 0xac);
	write_cmos_sensor(0x3826, 0x04);
	write_cmos_sensor(0x3827, 0x48);
	write_cmos_sensor(0x3829, 0x03);
	write_cmos_sensor(0x4009, 0x05);
	write_cmos_sensor(0x4050, 0x02);
	write_cmos_sensor(0x4051, 0x05);
	write_cmos_sensor(0x4837, 0x38);
	write_cmos_sensor(0x4902, 0x02);

	/*update sensor PLL1 clock*/
	/*MIP_PCLK:35.25M MIPI_PHY_CLK:282Mbps SCLK:28.2M*/
	write_cmos_sensor(0x0302, 0x5e);
	write_cmos_sensor(0x4837, 0x39);

	if (imgsensor.ihdr_en)
		cam_pr_debug("imgsensor ihdr enable\n");
		/* no implementation */
	else
		cam_pr_debug("imgsensor ihdr disable\n");
		/* no implementation */

	mdelay(10);
}

static void slim_video_setting(void)
{
	cam_pr_debug("E");
	write_cmos_sensor(0x0100, 0x00);
	mdelay(1);
	write_cmos_sensor(0x0303, 0x01);
	write_cmos_sensor(0x3501, 0x40);
	write_cmos_sensor(0x3662, 0x10);
	write_cmos_sensor(0x3714, 0x28);
	write_cmos_sensor(0x3737, 0x08);
	write_cmos_sensor(0x3739, 0x20);
	write_cmos_sensor(0x37c2, 0x14);
	write_cmos_sensor(0x37d9, 0x0c);
	write_cmos_sensor(0x37e3, 0x08);
	write_cmos_sensor(0x37e4, 0x34);
	write_cmos_sensor(0x37e6, 0x08);

	/* crop */
	write_cmos_sensor(0x3801, 0x00);
	write_cmos_sensor(0x3803, 0x08);
	write_cmos_sensor(0x3805, 0x9f);
	write_cmos_sensor(0x3807, 0x4f);
	write_cmos_sensor(0x3808, 0x08);
	write_cmos_sensor(0x3809, 0x40);
	write_cmos_sensor(0x380a, 0x06);
	write_cmos_sensor(0x380b, 0x20);
	write_cmos_sensor(0x380c, 0x04);
	write_cmos_sensor(0x380d, 0x62);
	write_cmos_sensor(0x380e, 0x0c);
	write_cmos_sensor(0x380f, 0x8e);
	write_cmos_sensor(0x3811, 0x08);
	write_cmos_sensor(0x3813, 0x02);
	write_cmos_sensor(0x3814, 0x03);
	write_cmos_sensor(0x3816, 0x03);
	write_cmos_sensor(0x3820, 0xab);
	write_cmos_sensor(0x3826, 0x04);
	write_cmos_sensor(0x3827, 0x90);
	write_cmos_sensor(0x3829, 0x07);
	write_cmos_sensor(0x4009, 0x0d);
	write_cmos_sensor(0x4050, 0x04);
	write_cmos_sensor(0x4051, 0x0b);
	write_cmos_sensor(0x4837, 0x1c);
	write_cmos_sensor(0x4902, 0x01);

	/*update sensor PLL1 clock*/
	/*MIP_PCLK:70.5M MIPI_PHY_CLK:564Mbps SCLK:56.4M*/
	write_cmos_sensor(0x0302, 0x5e);
	write_cmos_sensor(0x4837, 0x1c);

	if (imgsensor.ihdr_en)
		cam_pr_debug("imgsensor ihdr enable\n");
		/* no implementation */
	else
		cam_pr_debug("imgsensor ihdr disable\n");
		/* no implementation */

	mdelay(10);

}

#if 0
static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	cam_pr_debug("enable: %d\n", enable);

	if (enable) {
		/* 0x5E00[8]: 1 enable,  0 disable */
		/* 0x5E00[1:0];00 Color bar,01 Random Data,10 Square,11 BLACK */
		/* disable lenc and otp_dpc */
		write_cmos_sensor(0x5000, 0xdb);
		write_cmos_sensor(0x4503, 0x80);
	} else {
		/* 0x5E00[8]: 1 enable,  0 disable */
		/* 0x5E00[1:0];00 Color bar,01 Random Data,10 Square,11 BLACK */
		write_cmos_sensor(0x5000, 0xff);	/* enable otp_dpc */
		write_cmos_sensor(0x4503, 0x00);
	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}
#endif

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
			cam_pr_debug("Read sensor id fail:0x%x, id: 0x%x\n",
				imgsensor.i2c_write_id,
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
			cam_pr_debug("Read sensor id fail: 0x%x, id: 0x%x\n",
				imgsensor.i2c_write_id,
				sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 2;
	}
	if (imgsensor_info.sensor_id != sensor_id)
		return ERROR_SENSOR_CONNECT_FAIL;

	if ((read_cmos_sensor(0x36f0)) == 0x06)
		cam_pr_debug("----EC0----\n");
	else
		cam_pr_debug("----PRE_ECO----\n");

	/* initail sequence write in  */
	sensor_init();
	/* for OTP */
	/* otp_cali(imgsensor.i2c_write_id); */
	/* write_cmos_sensor(0x0100, 0x00); */

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
	/* for zsd multi capture setting */
	capture_first_flag = 0;
	pre_currefps = 0;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}				/*    open  */



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
}				/*    close  */


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
	/* set_mirror_flip(imgsensor.mirror); */
	/* set_mirror_flip(sensor_config_data->SensorImageMirror); */
	return ERROR_NONE;
}				/*    preview   */

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
	/* imgsensor.current_fps = 300; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting(imgsensor.current_fps);
	/* set_mirror_flip(sensor_config_data->SensorImageMirror); */
	/* set_mirror_flip(imgsensor.mirror); */
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
	/* set_mirror_flip(imgsensor.mirror); */
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
	slim_video_setting();
	/* set_mirror_flip(sensor_config_data->SensorImageMirror); */
	/* set_mirror_flip(imgsensor.mirror); */
	return ERROR_NONE;
}				/*    slim_video     */



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

	sensor_info->SensorMasterClockSwitch = 0;	/* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	/* The frame of setting shutter default 0 for TG int */
	sensor_info->AESensorGainDelayFrame =
		imgsensor_info.ae_sensor_gain_delay_frame;
	/* The frame of setting sensor gain */
	sensor_info->AEISPGainDelayFrame =
		imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->FrameTimeDelayFrame =
		imgsensor_info.frame_time_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
	sensor_info->PDAF_Support = 1;
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
		capture_first_flag = 0;
		pre_currefps = 0;
		preview(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		capture(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		capture_first_flag = 0;
		pre_currefps = 0;
		normal_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		capture_first_flag = 0;
		pre_currefps = 0;
		hs_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		capture_first_flag = 0;
		pre_currefps = 0;
		slim_video(image_window, sensor_config_data);
		break;
	default:
		cam_pr_debug("Error ScenarioId setting");
		capture_first_flag = 0;
		pre_currefps = 0;
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}				/* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{				/* This Function not used after ROME */
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
	/* kal_int16 dummyLine; */
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
		imgsensor.frame_length = imgsensor_info.pre.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/* set_dummy(); */
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
		/* set_dummy(); */
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
				imgsensor_info.cap.framelength) : 0;
			imgsensor.frame_length =
				imgsensor_info.cap.framelength +
				imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		}
		/* set_dummy(); */
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length =
			imgsensor_info.hs_video.pclk / framerate * 10 /
			imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.hs_video.framelength)
			? (frame_length -
			imgsensor_info.hs_video.framelength)
			: 0;
		imgsensor.frame_length =
			imgsensor_info.hs_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/* set_dummy(); */
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
		/* set_dummy(); */
		break;
	default:		/* coding with  preview scenario by default */
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
		imgsensor.frame_length = imgsensor_info.pre.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/* set_dummy(); */
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
	default:
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	cam_pr_debug("enable: %d\n", enable);

	if (enable) {
		write_cmos_sensor(0x5000, 0x81);
		write_cmos_sensor(0x5080, 0x80);
	} else {
		write_cmos_sensor(0x5000, 0xff);
		write_cmos_sensor(0x5080, 0x00);
	}

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
	unsigned long long *feature_data = (unsigned long long *)feature_para;
	/* unsigned long long *feature_return_para = */
	/*	(unsigned long long *) feature_para; */

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	/* add for ov13855 pdaf */
	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	cam_pr_debug("feature_id = %d\n", feature_id);
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
	#if 0
	case SENSOR_FEATURE_GET_PDAF_DATA:
		cam_pr_debug("SENSOR_FEATURE_GET_PDAF_DATA\n");
		read_ov13855_eeprom((kal_uint16)(*feature_data),
		(char *)(uintptr_t)(*(feature_data+1)),
		(kal_uint32)(*(feature_data+2)));
		break;
	#endif
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL) * feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
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
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length((UINT16) (*feature_data),
			(UINT16) (*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_GET_PDAF_INFO:
		cam_pr_debug("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%lld\n",
			*feature_data);
		PDAFinfo = (struct SET_PD_BLOCK_INFO_T *) (uintptr_t)
			(*(feature_data + 1));

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info,
			       sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			break;
		}
		break;
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		cam_pr_debug("SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%lld\n",
			*feature_data);
		/* PDAF capacity enable or not, */
		/* ov13855 only full size support PDAF */
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 1;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0;
			break;
		default:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0;
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

	default:
		break;
	}

	return ERROR_NONE;
}				/*    feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 OV13855_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}				/*    OV13855_MIPI_RAW_SensorInit    */
